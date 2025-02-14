#include "gateway.h"
#include <fstream>
#include <thread>
#include <chrono>
#include <random>
#include <ctime>
#include <iostream>
#include "controller/library.hpp"
#include "controller/pjrtc-adm.hpp"
#include <filesystem>
#include <td/telegram/Log.h>
#include "queue.h"
#include "settings.h"
#include "tg.h"
#include <string>
#include <limits.h>
#include <unistd.h>
#include <iomanip>
#include <libgen.h>
#include <linux/limits.h>
#include <string.h>
#include <regex>
#include <memory>
#include <algorithm>


namespace sml = boost::sml;
namespace td_api = td::td_api;

bool is_digits(const std::string &str) { return std::all_of(str.begin(), str.end(), ::isdigit); };
namespace { std::vector<std::string> voip_version(const Settings& settings) {
    const auto forced = "5.0.0";
    return {forced};
    return voip::Library::instance().versions();}}

namespace state_machine::guards {
    bool IsIncoming::operator()(const td::td_api::object_ptr<td::td_api::updateCall> &event) const {
        return !event->call_->is_outgoing_;}
    bool IsInState::operator()(const td::td_api::object_ptr<td::td_api::updateCall> &event) const {
        return event->call_->state_->get_id() == id_;}

    bool CallbackUriIsSet::operator()(const Settings &settings) const {  

    return !settings.callback_uri().empty();  }

    bool IsMediaReady::operator()(const sip::events::CallMediaStateUpdate &event) const { return event.has_media;}
    bool IsSipInState::operator()(const sip::events::CallStateUpdate &event) const {return event.state == state_;}
    bool IsTextContent::operator()(const td::td_api::object_ptr<td::td_api::updateNewMessage> &event) const {
    return event->message_->content_->get_id() == td::td_api::messageText::ID;}
     bool IsDtmfString::operator()(const td::td_api::object_ptr<td::td_api::updateNewMessage> &event) const {
        auto text = static_cast<const td_api::messageText &>(*event->message_->content_).text_->text_;
        const std::regex dtmf_regex("^[0-9A-D*#]{1,32}$");
        auto result = regex_match(text, dtmf_regex);
        return result;}}


namespace state_machine::actions {
    void StoreSipId::operator()(Context &ctx, const sip::events::IncomingCall &event) const { ctx.sip_call_id = event.id;}
    void StoreTgId::operator()(Context &ctx, const td::td_api::object_ptr<td::td_api::updateCall> &event ) const { ctx.tg_call_id = event->call_->id_;}
    void StoreTgUserId::operator()(Context &ctx, const td::td_api::object_ptr<td::td_api::updateCall> &event) const { ctx.user_id = event->call_->user_id_;}
    void CleanTgId::operator()(Context &ctx) const {ctx.tg_call_id = 0;}
    void CleanSipId::operator()(Context &ctx) const {ctx.sip_call_id = PJSUA_INVALID_ID;}


    void CleanUp::operator()(Context &ctx, sip::Client &sip_client, tg::Client &tg_client
                             ) const {

         flag_play = false;
         flag_rec  = false;

         if (ctx.controller)        {
            ctx.controller->Stop();
            ctx.controller.reset();  }

          if (ctx.tg_call_id != 0) {
            auto result = tg_client.send_query_async(td_api::make_object<td_api::discardCall>(
                ctx.tg_call_id,false,0,false,ctx.tg_call_id)).get();
            
            if (result->get_id() == td_api::error::ID)
             ctx.tg_call_id = 0;   }

        if (ctx.sip_call_id != PJSUA_INVALID_ID) {
            try {
                sip_client.Hangup(ctx.sip_call_id, ctx.hangup_prm);
            } catch (const pj::Error &error) {
               cout <<   error.reason << endl;
            }
            ctx.sip_call_id = PJSUA_INVALID_ID;   }}


    void DialSip::operator()(Context &ctx, tg::Client &tg_client, sip::Client &sip_client,
                             const td_api::object_ptr<td_api::updateCall> &event,
                             OptionalQueue<state_machine::events::Event> &internal_events,
                             const Settings &settings) const {
//////////////////

cout << settings.callback_uri() << endl;
cout << "call in incoming " << endl;
in_flag = false;


          auto tg_user_id = event->call_->user_id_;
        auto response = tg_client.send_query_async(td_api::make_object<td_api::getUser>(tg_user_id)).get();

        if (response->get_id() == td_api::error::ID) {
            cout << " get user info of id  failed " << ctx.id()<< " "<< tg_user_id<<" " <<to_string(response)<< endl;
            auto err = td_api::move_object_as<td_api::error>(response);
            throw std::runtime_error{err->message_};
        }

        auto user = td::move_tl_object_as<td_api::user>(response);
        auto prm = pj::CallOpParam(true);

        auto &headers = prm.txOption.headers;
        auto header = pj::SipHeader();

        {
            // debug purpose header
            header.hName = "X-GW-Context";
            header.hValue = ctx.id();
            headers.push_back(header);
        }

        {
            header.hName = "X-TG-ID";
            header.hValue = std::to_string(tg_user_id);
            headers.push_back(header);
        }

        if (!user->first_name_.empty()) {
            header.hName = "X-TG-FirstName";
            header.hValue = user->first_name_;
            headers.push_back(header);
        }

        if (!user->last_name_.empty()) {
            header.hName = "X-TG-LastName";
            header.hValue = user->last_name_;
            headers.push_back(header);
        }

        if (!user->phone_number_.empty()) {
            header.hName = "X-TG-Phone";
            header.hValue = user->phone_number_;
            headers.push_back(header);
        }

        try {
            ctx.sip_call_id = sip_client.Dial(settings.callback_uri(), prm);
        } catch (const pj::Error &error) {

           cout << "error.........."<< endl;

            pj::CallOpParam hangup_prm;
            hangup_prm.statusCode = PJSIP_SC_INTERNAL_SERVER_ERROR;
            hangup_prm.reason = error.reason;

            internal_events.emplace(state_machine::events::InternalError{ctx.id(), hangup_prm});

            return;
        }

        cout << " associated with SIP " << ctx.id() << " "<< ctx.sip_call_id<<endl;
    }

    void AnswerTg::operator()(Context &ctx, tg::Client &tg_client, const Settings &settings,
                              OptionalQueue<state_machine::events::Event> &internal_events)
                              const {

         auto response = tg_client.send_query_async(td_api::make_object<td_api::acceptCall>(
                ctx.tg_call_id,
                td_api::make_object<td_api::callProtocol>(true,
                                                          true,
                                                          voip::Library::instance().callProtoMinLayer(),
                                                          voip::Library::instance().callProtoMaxLayer(),
                                                          voip_version(settings))
        )).get();
        cout<<to_string(response)<<endl;
        if (response->get_id() == td_api::error::ID) {
            pj::CallOpParam hangup_prm;
            hangup_prm.statusCode = PJSIP_SC_DECLINE;
            hangup_prm.reason = "Decline";
            internal_events.emplace(state_machine::events::InternalError{ctx.id(), hangup_prm});
        }

    }


void AcceptIncomingSip::operator()(Context &ctx, tg::Client &tg_client, sip::Client &sip_client, const Settings &settings,
                                       const sip::events::IncomingCall &event,
                                       OptionalQueue<state_machine::events::Event> &internal_events
                                       ){


        cout << "call in outcoming " << endl;
        auto ext = event.extension;
        in_flag = true;
        bool ext_valid{true};

        if (ext.length() > 3 && ext.substr(0, 3) == "aa_") {
            ctx.ext_username = ext.substr(3, std::string::npos);
        } else if (ext.length() > 1 && ext[0] == '+' && is_digits(ext.substr(1, std::string::npos))) {
            ctx.ext_phone = ext.substr(1, std::string::npos);
        } else if (is_digits(ext)) {
            try {
                ctx.user_id = std::stol(ext);
            } catch (const std::invalid_argument &e) {
                ext_valid = false;
            } catch (const std::out_of_range &e) {
                ext_valid = false;
            }
        } else {
            ext_valid = false;
        }

        if (ext_valid) {
            auto a_prm = pj::CallOpParam(true);
            a_prm.statusCode = PJSIP_SC_RINGING;
            
            try {
                sip_client.Answer(ctx.sip_call_id, a_prm);
            } catch (const pj::Error &error) {
                pj::CallOpParam hangup_prm;
                hangup_prm.statusCode = PJSIP_SC_INTERNAL_SERVER_ERROR;
                hangup_prm.reason = error.reason;
                internal_events.emplace(state_machine::events::InternalError{ctx.id(), hangup_prm});
            } catch (const std::runtime_error &error) {
                pj::CallOpParam hangup_prm;
                hangup_prm.statusCode = PJSIP_SC_INTERNAL_SERVER_ERROR;
                hangup_prm.reason = error.what();
                internal_events.emplace(state_machine::events::InternalError{ctx.id(), hangup_prm});
            }

        } else {
            pj::CallOpParam prm;
            prm.statusCode = PJSIP_SC_BAD_EXTENSION;
            internal_events.emplace(state_machine::events::InternalError{ctx.id(), prm});
        }
    }

    void AnswerSip::operator()(Context &ctx, sip::Client &sip_client, const Settings &settings,
                               OptionalQueue<state_machine::events::Event> &internal_events
                              ) const {

        auto prm = pj::CallOpParam(true);
        prm.statusCode = PJSIP_SC_OK;
       try {
            sip_client.Answer(ctx.sip_call_id, prm);
        } catch (const pj::Error &error) {
            pj::CallOpParam hangup_prm;
            hangup_prm.statusCode = PJSIP_SC_DECLINE;
            hangup_prm.reason = "Decline";
            internal_events.emplace(state_machine::events::InternalError{ctx.id(), hangup_prm});
        } catch (const std::runtime_error &error) {
            pj::CallOpParam hangup_prm;
            hangup_prm.statusCode = PJSIP_SC_DECLINE;
            hangup_prm.reason = "Decline";
            internal_events.emplace(state_machine::events::InternalError{ctx.id(), hangup_prm});
        }

    }

    void state_machine::actions::CreateVoip::operator()(Gateway *gw,
                                                        Context &ctx, const Settings &settings,
                                                        tg::Client &tg_client,
                                                        const td::td_api::object_ptr<td::td_api::updateCall> &event
                                                        ) const {
           
          
        ctx.controller = voip::Library::instance().createController
            (Controller::CreateCtx{gw,
                                   std::cref(event),
                                   std::cref(settings)
                                   });
        ctx.controller->Start();
        ctx.controller->Connect();

    }

    void state_machine::actions::UpdateSignaling::operator()(Context &ctx, const Settings &settings,const td::td_api::object_ptr<td::td_api::updateNewCallSignalingData> &event) const {ctx.controller->UpdateSignaling(event->data_);}



    void state_machine::actions::BridgeAudio::operator()(Context &ctx, sip::Client &sip_client,
                                                         OptionalQueue<state_machine::events::Event> &internal_events
                                                         ) const {
 
        try {
            sip_client.BridgeAudio(ctx.sip_call_id,
                                   ctx.controller->AudioMediaInput(),
                                   ctx.controller->AudioMediaOutput());
        } catch (const pj::Error &error) {
            pj::CallOpParam hangup_prm;
            hangup_prm.statusCode = PJSIP_SC_DECLINE;
            hangup_prm.reason = "Decline";
            internal_events.emplace(state_machine::events::InternalError{ctx.id(), hangup_prm});
        } catch (const std::runtime_error &error) {
            pj::CallOpParam hangup_prm;
            hangup_prm.statusCode = PJSIP_SC_DECLINE;
            hangup_prm.reason = "Decline";
            internal_events.emplace(state_machine::events::InternalError{ctx.id(), hangup_prm});
        }
    }

    void SetHangupPrm::operator()(Context &ctx, const state_machine::events::InternalError &event) const {
        ctx.hangup_prm = event.prm;
    }

    void DialTg::operator()(Context &ctx, tg::Client &tg_client, const Settings &settings,Cache &cache,
                            OptionalQueue<state_machine::events::Event> &internal_events
                           ) { 

    

        ctx_ = &ctx;
        tg_client_ = &tg_client;
        settings_ = &settings;
        cache_ = &cache;
        
        internal_events_ = &internal_events;
    
        if (!ctx.ext_username.empty()) {
            dial_by_username();
        } else if (!ctx.ext_phone.empty()) {
            dial_by_phone();
        } else {
            dial_by_id(ctx.user_id);
        }
    }

    void DialTg::dial_by_id(int64_t id) 
    {
         auto response = tg_client_->send_query_async(td_api::make_object<td_api::createCall>(
                id,
                td_api::make_object<td_api::callProtocol>(true,true,
                                                          voip::Library::instance().callProtoMinLayer(),
                                                          voip::Library::instance().callProtoMaxLayer(),
                                                          voip_version(*settings_)),false)).get();
         cout<<to_string(response)<<endl;

        if (response->get_id() == td_api::error::ID) {
            auto prm = pj::CallOpParam(true);
            prm.statusCode =  PJSIP_SC_DECLINE;
            prm.reason = "Decline";
            internal_events_->emplace(state_machine::events::InternalError{ctx_->id(), prm});
            return;
        }
        auto call_id_ = td::move_tl_object_as<td_api::callId>(response);
        ctx_->tg_call_id = call_id_->id_;
    }

    void DialTg::dial_by_phone() {

        auto it = cache_->phone_cache.find(ctx_->ext_phone);
        if (it != cache_->phone_cache.end()) {
            cout<<" found phone in cache "<<ctx_->ext_phone<<" ID is "<<it->second<<endl;
            dial_by_id(it->second);
            return;
        }

    

        auto contact = td_api::make_object<td_api::contact>();
        contact->phone_number_ = ctx_->ext_phone;

        auto contacts = std::vector<td_api::object_ptr<td_api::contact>>();
        contacts.emplace_back(std::move(contact));

        auto response = tg_client_->send_query_async(
                td_api::make_object<td_api::importContacts>(std::move(contacts))).get();


        if (response->get_id() == td_api::error::ID) {
            cout<<to_string(response)<<endl;   
            auto error = td::move_tl_object_as<td_api::error>(response);

            auto prm = pj::CallOpParam(true);
            prm.statusCode = PJSIP_SC_INTERNAL_SERVER_ERROR;
            prm.reason = std::to_string(error->code_) + "; " + error->message_;
            internal_events_->emplace(state_machine::events::InternalError{ctx_->id(), prm});
            return;
        }

        auto imported_contacts = td::move_tl_object_as<td_api::importedContacts>(response);
        auto user_id_ = imported_contacts->user_ids_[0];

        if (user_id_ == 0) {
           

            auto prm = pj::CallOpParam(true);
            prm.statusCode = PJSIP_SC_NOT_FOUND;
            prm.reason = "not registered in telegram";
            internal_events_->emplace(state_machine::events::InternalError{ctx_->id(), prm});

            return;
        }

       
        cache_->phone_cache.emplace(ctx_->ext_phone, user_id_);
        dial_by_id(user_id_);
    }

    void DialTg::dial_by_username() {
        auto it = cache_->username_cache.find(ctx_->ext_username);
        if (it != cache_->username_cache.end()) {
           cout<<" found username in cache " <<ctx_->ext_username<<" ID is "<<it->second<<endl;
            dial_by_id(it->second);
            return;
        }
      
        auto response = tg_client_->send_query_async(
                td_api::make_object<td_api::searchPublicChat>(ctx_->ext_username)).get();


        if (response->get_id() == td_api::error::ID) {
            cout<<to_string(response)<<endl;
            auto error = td::move_tl_object_as<td_api::error>(response);
            auto prm = pj::CallOpParam(true);
            prm.statusCode = PJSIP_SC_INTERNAL_SERVER_ERROR;
            prm.reason = std::to_string(error->code_) + "; " + error->message_;
            internal_events_->emplace(state_machine::events::InternalError{ctx_->id(), prm});
            return;
        }

        auto chat = td::move_tl_object_as<td_api::chat>(response);

        if (chat->type_->get_id() != td_api::chatTypePrivate::ID) {

            auto prm = pj::CallOpParam(true);
            prm.statusCode = PJSIP_SC_INTERNAL_SERVER_ERROR;
            prm.reason = "not a user";
            internal_events_->emplace(state_machine::events::InternalError{ctx_->id(), prm});

            return;
        }

        auto id = chat->id_;
        cache_->username_cache.emplace(ctx_->ext_username, id);
        dial_by_id(id);
    }



    void DialDtmf::operator()(Context &ctx, sip::Client &sip_client,
                              const td_api::object_ptr<td::td_api::updateNewMessage> &event
                              ) const {

        auto text = static_cast<const td_api::messageText &>(*event->message_->content_).text_->text_;
      
        try {
            sip_client.DialDtmf(ctx.sip_call_id, text);
        } catch (const pj::Error &error) {
            cout<<error.reason<<endl;
        }
    }
}


namespace state_machine {

    Logger::Logger(std::string context_id) : context_id_(std::move(context_id)) {}
    Logger::~Logger() {};

    template<class SM>
    void Logger::log_process_event(const td::td_api::object_ptr<td::td_api::updateCall> &event) {};

    template<class SM>
    void Logger::log_process_event(const sip::events::Event &event) {};

    template<class SM, class TEvent>
    void Logger::log_process_event(const TEvent &) {}

    template<class SM, class TGuard, class TEvent>
    void Logger::log_guard(const TGuard &, const TEvent &event, bool result) {};

    template<class SM, class TAction, class TEvent>
    void Logger::log_action(const TAction &, const TEvent &event) {};

    template<class SmLogger, class TSrcState, class TDstState>
    void Logger::log_state_change(const TSrcState &src, const TDstState &dst) {}

    struct from_tg {
        auto operator()() const {
            using namespace boost::sml;
            using namespace td::td_api;
            using namespace guards;
            using namespace actions;
            return make_transition_table(
                    *"sip_wait_media"_s + event<sip::events::CallMediaStateUpdate>[IsMediaReady{}]
                                          / AnswerTg{} = "wait_tg"_s,
                    "wait_tg"_s + event<object_ptr<updateCall>>[IsInState{callStateReady::ID}]
                                  / (CreateVoip{}, BridgeAudio{}) = "wait_dtmf"_s,
                    "wait_dtmf"_s + event<object_ptr<updateNewMessage>>[IsTextContent{} && IsDtmfString{}]
                                  / DialDtmf{} = "wait_dtmf"_s,

                    "sip_wait_media"_s + event<object_ptr<updateNewCallSignalingData>>
                                         / UpdateSignaling{} = "sip_wait_media"_s,

                    "wait_dtmf"_s + event<object_ptr<updateNewCallSignalingData>>
                                    / UpdateSignaling{} = "wait_dtmf"_s

            );
        }
    };

   struct from_sip {
        auto operator()() const {
            using namespace boost::sml;
            using namespace td::td_api;
            using namespace guards;
            using namespace actions;
            return make_transition_table(
                    *"sip_wait_confirm"_s + event<sip::events::CallStateUpdate>
                                            [IsSipInState{PJSIP_INV_STATE_EARLY}] / DialTg{} = "wait_tg"_s,
                    "wait_tg"_s + event<object_ptr<updateCall>>[IsInState{callStateReady::ID}]
                                  / (StoreTgUserId{}, CreateVoip{}, AnswerSip{}) = "sip_wait_media"_s,
                    "sip_wait_media"_s + event<sip::events::CallMediaStateUpdate>[IsMediaReady{}]
                                         / BridgeAudio{} = "wait_dtmf"_s,
                    "wait_dtmf"_s + event<object_ptr<updateNewMessage>>[IsTextContent{} && IsDtmfString{}]
                                    / DialDtmf{} = "wait_dtmf"_s,

                    "sip_wait_media"_s + event<object_ptr<updateNewCallSignalingData>>
                                         / UpdateSignaling{} = "sip_wait_media"_s,

                    "wait_dtmf"_s + event<object_ptr<updateNewCallSignalingData>>
                                    / UpdateSignaling{} = "wait_dtmf"_s
            );
        }
    };

    struct StateMachine {
        auto operator()() const {
            using namespace boost::sml;
            using namespace td::td_api;
            using namespace guards;
            using namespace actions;
            using namespace events;
            return make_transition_table(
                    *"init"_s + event<object_ptr<updateCall>>
                                [IsIncoming{} && IsInState{callStatePending::ID} && CallbackUriIsSet{}]
                                / (StoreTgId{}, StoreTgUserId{}, DialSip{}) = state<from_tg>,
                    "init"_s + event<object_ptr<updateCall>>
                               [IsIncoming{} && IsInState{callStatePending::ID} && !CallbackUriIsSet{}]
                               / StoreTgId{} = X,
                    "init"_s + event<object_ptr<updateCall>> = X,
                    "init"_s + event<sip::events::IncomingCall> / (StoreSipId{}, AcceptIncomingSip{}) = state<from_sip>,
                    "init"_s + event<sip::events::CallStateUpdate> = X,
                    "init"_s + event<sip::events::CallMediaStateUpdate> = X,

                    state<from_tg> + event<object_ptr<updateCall>>
                                     [IsInState{callStateDiscarded::ID} || IsInState{callStateError::ID}] /
                                     CleanTgId{} = X,
                    state<from_tg> + event<sip::events::CallStateUpdate>
                                     [IsSipInState{PJSIP_INV_STATE_DISCONNECTED}] / CleanSipId{} = X,
                    state<from_tg> + event<InternalError> / SetHangupPrm{} = X,

                    state<from_sip> + event<object_ptr<updateCall>>
                                      [IsInState{callStateDiscarded::ID} || IsInState{callStateError::ID}] /
                                      CleanTgId{} = X,
                    state<from_sip> + event<InternalError> / SetHangupPrm{} = X,
                    state<from_sip> + event<sip::events::CallStateUpdate>
                                      [IsSipInState{PJSIP_INV_STATE_DISCONNECTED}] / CleanSipId{} = X,

                    X + on_entry<_> / CleanUp{}
            );
        }
    };
}

Context::Context() : id_(next_ctx_id()) {}

const std::string Context::id() const { return id_; };

std::string Context::next_ctx_id() {
    static int64_t ctx_counter{0};
    static std::string id_prefix = std::to_string(getpid()) + "-";
    return id_prefix + std::to_string(++ctx_counter);
}

Gateway::Gateway(sip::Client &sip_client_, tg::Client &tg_client_,
                 OptionalQueue<sip::events::Event> &sip_events_,
                 OptionalQueue<tg::Client::Object> &tg_events_,
                 Settings &settings)
        : sip_client_(sip_client_), tg_client_(tg_client_), 
          sip_events_(sip_events_), tg_events_(tg_events_), settings_(settings) {}

void Gateway::start() {

   pthread_setname_np(pthread_self(), "gw");
   std::this_thread::sleep_for(std::chrono::milliseconds(2000));
   load_cache();


     while (true) {

        if (auto event = internal_events_.pop(); event) {
            std::visit([this](auto &&casted_event) {
                process_event(casted_event);
            }, event.value());
        }

        if (auto event = tg_events_.pop(); event) {
            using namespace td::td_api;
            auto &&object = event.value();
            switch (object->get_id())
            {
                 case updateCall::ID:
                    process_event(move_object_as<updateCall>(object));
                    break;
                case updateNewCallSignalingData::ID:
                    process_event(move_object_as<updateNewCallSignalingData>(object));
                    break;
                case updateNewMessage::ID:
                    process_event(move_object_as<updateNewMessage>(object));
                    break;
                default:
                    break;
            }
        }

        if (auto event = sip_events_.pop(); event) {
            std::visit([this](auto &&casted_event) {
                process_event(casted_event);
            }, event.value());}

            std::this_thread::sleep_for(std::chrono::milliseconds(10)); 
         }}

void Gateway::sendCallSignalingData(int call_id, std::string data) {
    internal_events_.emplace(state_machine::events::SendCallSignaling{call_id, data});
}

void Gateway::load_cache() {
    auto search_response = tg_client_.send_query_async(
            td::td_api::make_object<td::td_api::searchContacts>("", 5000)).get();

    if (search_response->get_id() == td::td_api::error::ID) 
        {
        cout<<to_string(search_response)<<endl;
        return;
         }

    auto users = td::td_api::move_object_as<td::td_api::users>(search_response);
    std::vector<std::future<tg::Client::Object>> responses;
    for (auto user_id : users->user_ids_) {
        responses.emplace_back(tg_client_.send_query_async(td::td_api::make_object<td::td_api::getUser>(user_id)));
    }
    for (auto &future : responses) {
        auto response = future.get();

        if (response->get_id() == td::td_api::error::ID) 
            { cout<<to_string(response)<<endl;
            continue;
}
        auto user = td::td_api::move_object_as<td::td_api::user>(response);

        if (!user->have_access_)
            return;
        if (user->usernames_ != nullptr)
            cache_.username_cache.emplace(user->usernames_->active_usernames_[0], user->id_);
        if (!user->phone_number_.empty())
            cache_.phone_cache.emplace(user->phone_number_, user->id_);
    }

    cout<<" Loaded usernames "<< cache_.username_cache.size() <<" and  phones into contacts cache "<<cache_.phone_cache.size()<<endl;
}

vector<Bridge *>::iterator Gateway::search_call(const function<bool(const Bridge *)> &predicate) {
    auto iter = find_if(bridges.begin(), bridges.end(), predicate);

    if (iter == bridges.end()) {
        auto ctx = make_unique<Context>();
        auto sm_logger = std::make_unique<state_machine::Logger>(ctx->id());
        auto sm = make_unique<state_machine::sm_t>(this, *sm_logger, sip_client_, tg_client_, settings_,*ctx,cache_,internal_events_);
        auto bridge = new Bridge;
        bridge->ctx = move(ctx);
        bridge->sm = move(sm);
        bridge->logger = move(sm_logger);
        bridges.emplace_back(bridge);
        iter = bridges.end() - 1;
    }
    return iter;
}

void Gateway::process_event(td::td_api::object_ptr<td::td_api::updateCall> update_call) {

    auto iter = search_call([id = update_call->call_->id_](const Bridge *bridge) {
        return bridge->ctx->tg_call_id == id;
    });

    (*iter)->sm->process_event(update_call);

    if ((*iter)->sm->is(sml::X)) {
        delete *iter;
        bridges.erase(iter);     }
}

void Gateway::process_event(td::td_api::object_ptr<td::td_api::updateNewCallSignalingData> update_signaling_data) {

    auto iter = search_call([id = update_signaling_data->call_id_](const Bridge *bridge) {
        return bridge->ctx->tg_call_id == id;
    });

    (*iter)->sm->process_event(update_signaling_data);
}

void Gateway::process_event(td::td_api::object_ptr<td::td_api::updateNewMessage> update_message) {

    auto &sender = update_message->message_->sender_id_;
    auto user = static_cast<const td_api::messageSenderUser *>(sender.get());

    std::vector<Bridge *> matches;
    for (auto bridge : bridges) 
        if (bridge->ctx->user_id == user->user_id_) 
            matches.emplace_back(bridge);

    if (matches.size() > 1) 
        return;
     else if (matches.size() == 1) 
        matches[0]->sm->process_event(update_message);
}

void Gateway::process_event(state_machine::events::SendCallSignaling &event) {
    auto iter = std::find_if(bridges.begin(),
                             bridges.end(),
                             [id = event.call_id](const Bridge *bridge) {
                                 return bridge->ctx->tg_call_id == id;  });

    if (iter == bridges.end())
    return;

    auto rq = td::td_api::make_object<td::td_api::sendCallSignalingData>(event.call_id, event.data);
    tg_client_.send_query_async(std::move(rq)).get();
}



void Gateway::process_event(state_machine::events::InternalError &event) {
    auto iter = std::find_if(bridges.begin(), bridges.end(), [id = event.ctx_id](const Bridge *bridge) {
        return bridge->ctx->id() == id;
    });

    if (iter == bridges.end()) 
        return;

    (*iter)->sm->process_event(event);

    if ((*iter)->sm->is(sml::X)) {
        delete *iter;
        bridges.erase(iter);}}

template<typename TSipEvent>
void Gateway::process_event(const TSipEvent &event) {

    auto iter = search_call([id = event.id](const Bridge *bridge) {
        return bridge->ctx->sip_call_id == id;
    });
    (*iter)->sm->process_event(event);
    if ((*iter)->sm->is(sml::X)) {
        delete *iter;
        bridges.erase(iter);}}

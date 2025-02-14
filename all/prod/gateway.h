#ifndef TG2SIP_GATEWAY_H
#define TG2SIP_GATEWAY_H
#include "controller/controller.hpp"
#include "controller/tgcalls.hpp"

#include "sml.hpp"
#include "sip.h"
#include "tg.h"
#include "queue.h"

extern bool in_flag;


namespace sml = boost::sml;
using namespace std;
class Context;
class Gateway;
struct Cache;


namespace state_machine::events {

    struct InternalError {
        std::string ctx_id;
        pj::CallOpParam prm;
    };
    struct SendCallSignaling {
        int call_id;
        std::string data;
    };
    typedef std::variant<InternalError, SendCallSignaling> Event;
}

namespace state_machine::guards {
    struct IsIncoming {
        bool operator()(const td::td_api::object_ptr<td::td_api::updateCall> &event) const;
    };

    class IsInState {
    public:
        explicit IsInState(int64_t id) : id_(id) {}

        bool operator()(const td::td_api::object_ptr<td::td_api::updateCall> &event) const;

    private:
        const int64_t id_;
    };

    class IsSipInState {
    public:
        explicit IsSipInState(pjsip_inv_state state) : state_(state) {}

        bool operator()(const sip::events::CallStateUpdate &event) const;

    private:
        const pjsip_inv_state state_;
    };

    struct CallbackUriIsSet {
        bool operator()(const Settings &settings) const;
    };

    struct IsMediaReady {
        bool operator()(const sip::events::CallMediaStateUpdate &event) const;
    };

    struct IsTextContent {
        bool operator()(const td::td_api::object_ptr<td::td_api::updateNewMessage> &event) const;
    };

    struct IsDtmfString {
        bool operator()(const td::td_api::object_ptr<td::td_api::updateNewMessage> &event) const;
    };
}

namespace state_machine::actions {

    struct StoreTgId {
        void operator()(Context &ctx, const td::td_api::object_ptr<td::td_api::updateCall> &event
                        ) const;
    };

    struct StoreTgUserId {
        void operator()(Context &ctx, const td::td_api::object_ptr<td::td_api::updateCall> &event
                        ) const;
    };

    struct CleanTgId {
        void operator()(Context &ctx) const;
    };

    struct CleanSipId {
        void operator()(Context &ctx) const;
    };

    struct StoreSipId {
        void operator()(Context &ctx, const sip::events::IncomingCall &event
                        ) const;
    };

    struct DialSip {
        void operator()(Context &ctx, tg::Client &tg_client, sip::Client &sip_client,
                        const td::td_api::object_ptr<td::td_api::updateCall> &event,
                        OptionalQueue<state_machine::events::Event> &internal_events,
                        const Settings &settings) const;
    };

    struct AnswerTg {
        void operator()(Context &ctx, tg::Client &tg_client, const Settings &settings,
                        OptionalQueue<state_machine::events::Event> &internal_events
                        ) const;
    };

    struct AcceptIncomingSip {

        void operator()(Context &ctx, tg::Client &tg_client, sip::Client &sip_client, const Settings &settings,
                        const sip::events::IncomingCall &event,
                        OptionalQueue<state_machine::events::Event> &internal_events
                        );
  
       
       void parse_error(td::td_api::object_ptr<td::td_api::error> error);

    };

    struct AnswerSip {
        void operator()(Context &ctx, sip::Client &sip_client, const Settings &settings,
                        OptionalQueue<state_machine::events::Event> &internal_events
                        ) const;
    };

    struct BridgeAudio {
        void operator()(Context &ctx, sip::Client &sip_client,
                        OptionalQueue<state_machine::events::Event> &internal_events
                        ) const;
    };

    struct CreateVoip {
        void operator()(Gateway *gw,Context &ctx, const Settings &settings,tg::Client &tg_client,
                        const td::td_api::object_ptr<td::td_api::updateCall> &event
                        ) const;
    };

    struct UpdateSignaling {
        void operator()(Context &ctx, const Settings &settings,
                        const td::td_api::object_ptr<td::td_api::updateNewCallSignalingData> &event
                        ) const;
    };

    struct CleanUp {
        void operator()(Context &ctx, sip::Client &sip_client, tg::Client &tg_client
                        ) const;
    };

    struct SetHangupPrm {
        void operator()(Context &ctx, const state_machine::events::InternalError &event) const;
    };

    class DialTg {
    private:
        Context *ctx_;
        tg::Client *tg_client_;
        Settings const *settings_;
        Cache *cache_;
        
        OptionalQueue<state_machine::events::Event> *internal_events_;

        void parse_error(td::td_api::object_ptr<td::td_api::error> error);

        void dial_by_id(int64_t id);

        void dial_by_phone();

        void dial_by_username();

    public:
        
        void operator()(Context &ctx, tg::Client &tg_client, const Settings &settings,Cache &cache,
                        OptionalQueue<state_machine::events::Event> &internal_events
                        );
    };

    struct DialDtmf {
        void operator()(Context &ctx, sip::Client &sip_client,
                        const td::td_api::object_ptr<td::td_api::updateNewMessage> &event
                        ) const;
    };
}

namespace state_machine {
    class Logger {
    public:
        Logger(std::string context_id);

        virtual ~Logger();

        template<class SM>
        void log_process_event(const td::td_api::object_ptr<td::td_api::updateCall> &event);

        template<class SM>
        void log_process_event(const sip::events::Event &event);

        template<class SM, class TEvent>
        void log_process_event(const TEvent &);

        template<class SM, class TGuard, class TEvent>
        void log_guard(const TGuard &, const TEvent &event, bool result);

        template<class SM, class TAction, class TEvent>
        void log_action(const TAction &, const TEvent &event);

        template<class SmLogger, class TSrcState, class TDstState>
        void log_state_change(const TSrcState &src, const TDstState &dst);

    private:
        const std::string context_id_;
    };

    struct StateMachine;

    typedef sml::sm<StateMachine, sml::logger<Logger>, sml::thread_safe<std::recursive_mutex>> sm_t;
}


struct Cache {
    std::map<std::string, int64_t> username_cache;
    std::map<std::string, int64_t> phone_cache;
};

class Context {
public:
    Context();

    const std::string id() const;

    pjsua_call_id sip_call_id{PJSUA_INVALID_ID};
    int32_t tg_call_id{0};
    std::shared_ptr<Controller> controller{nullptr};

    std::string ext_phone;
    std::string ext_username;
    int64_t user_id{0};

    pj::CallOpParam hangup_prm;

private:
    const std::string id_;

    std::string next_ctx_id();
};

struct Bridge {
    std::unique_ptr<state_machine::sm_t> sm;
    std::unique_ptr<Context> ctx;
    std::unique_ptr<state_machine::Logger> logger;
};

class Gateway {
public:
    Gateway(sip::Client &sip_client_, tg::Client &tg_client_,
            OptionalQueue<sip::events::Event> &sip_events_,
            OptionalQueue<tg::Client::Object> &tg_events_,
            Settings &settings);

    Gateway(const Gateway &) = delete;

    Gateway &operator=(const Gateway &) = delete;

    void start();

    void sendCallSignalingData(int call_id, std::string data);

private:

    sip::Client &sip_client_;
    tg::Client &tg_client_;
    const Settings &settings_;

    OptionalQueue<sip::events::Event> &sip_events_;
    OptionalQueue<tg::Client::Object> &tg_events_;
    OptionalQueue<state_machine::events::Event> internal_events_;
    Cache cache_;
    std::vector<Bridge *> bridges;
    
    std::vector<Bridge *>::iterator search_call(const std::function<bool(const Bridge *)> &predicate);

    void process_event(td::td_api::object_ptr<td::td_api::updateCall> update_call);

    void process_event(td::td_api::object_ptr<td::td_api::updateNewMessage> update_message);

    void process_event(td::td_api::object_ptr<td::td_api::updateNewCallSignalingData> update_signaling_data);

    void process_event(state_machine::events::InternalError &event);
    
    void process_event(state_machine::events::SendCallSignaling &event);


    template<typename TSipEvent>
    void process_event(const TSipEvent &event);
    void load_cache();
};

#endif //TG2SIP_GATEWAY_H

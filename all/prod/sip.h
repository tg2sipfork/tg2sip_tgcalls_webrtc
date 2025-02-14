#ifndef TG2SIP_SIP_H
#define TG2SIP_SIP_H
#include <variant>
#include <pjsua2.hpp>
#include "settings.h"
#include "queue.h"
namespace sip {

    namespace events {
        struct IncomingCall {
            pjsua_call_id id;
            std::string extension;
        };

        struct CallStateUpdate {
            pjsua_call_id id;
            pjsip_inv_state state;
        };

        struct CallMediaStateUpdate {
            pjsua_call_id id;
            bool has_media;
        };

        typedef std::variant<IncomingCall, CallStateUpdate, CallMediaStateUpdate> Event;
    }


class LogWriter : public pj::LogWriter {
    public:
        explicit LogWriter() {};

        ~LogWriter() override = default;

        void write(const pj::LogEntry &entry) override;
    };


  
    struct AccountConfig : public pj::AccountConfig {
        explicit AccountConfig(Settings &settings);
    };

    class Call : public pj::Call {
    public:
        Call(pj::Account &acc,  int call_id);

        ~Call() override;

        void addHandler(std::function<void(pj::OnCallStateParam &)> &&handler);

        void addHandler(std::function<void(pj::OnCallMediaStateParam &)> &&handler);

        const std::string localUser();

        pj::AudioMedia *audio_media();


    private:
        friend class Client;

        void onCallState(pj::OnCallStateParam &prm) override;

        void onCallMediaState(pj::OnCallMediaStateParam &prm) override;

        std::function<void(pj::OnCallStateParam &)> onCallStateHandler;
        std::function<void(pj::OnCallMediaStateParam &)> onCallMediaStateHandler;
        std::string local_user_;
    };

    class Account : public pj::Account {
    public:
        explicit Account() {};

        void addHandler(std::function<void(pj::OnIncomingCallParam &)> &&handler);

    private:
        void onIncomingCall(pj::OnIncomingCallParam &prm) override;

        std::function<void(pj::OnIncomingCallParam &)> onIncomingCallHandler;
    };

    class Client {
    public:
        Client(std::unique_ptr<Account> account_, std::unique_ptr<AccountConfig> account_cfg_,
               OptionalQueue<events::Event> &events, Settings &settings,LogWriter *sip_log_writer);
               

        Client(const Client &) = delete;

        Client &operator=(const Client &) = delete;

        virtual ~Client();

        void start();

        pjsua_call_id Dial(const std::string &uri, const pj::CallOpParam &prm);

        void Answer(pjsua_call_id call_id, const pj::CallOpParam &prm);

        void Hangup(pjsua_call_id call_id, const pj::CallOpParam &prm);

        void DialDtmf(pjsua_call_id call_id, const string &dtmf_digits);

        void BridgeAudio(pjsua_call_id call_id, pj::AudioMedia *input, pj::AudioMedia *output);

    private:

        std::unique_ptr<Account> account;
        std::unique_ptr<AccountConfig> account_cfg;

        OptionalQueue<events::Event> &events;
        std::map<int, std::shared_ptr<Call>> calls;

        static void init_pj_endpoint(Settings &settings,LogWriter *sip_log_writer);

        static std::string user_from_uri(const std::string &uri);

        void set_default_handlers(const std::shared_ptr<Call> &call);
    };
}

#endif //TG2SIP_SIP_H

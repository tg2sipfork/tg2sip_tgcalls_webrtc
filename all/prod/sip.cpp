#include "sip.h"
using namespace sip;
using namespace std;

void LogWriter::write(const pj::LogEntry &entry) {
    auto msg = entry.msg;
}


AccountConfig::AccountConfig(Settings &settings) {

    idUri = settings.id_uri();

    callConfig.timerMinSESec = 120;
    callConfig.timerSessExpiresSec = 1800;
}

void Account::addHandler(std::function<void(pj::OnIncomingCallParam &)> &&handler) {
    onIncomingCallHandler = std::move(handler);
}

void Account::onIncomingCall(pj::OnIncomingCallParam &prm) {
    if (onIncomingCallHandler) {
        onIncomingCallHandler(prm);
    }
}

Call::Call(pj::Account &acc, int call_id = PJSUA_INVALID_ID)
        : pj::Call(acc, call_id) {}


Call::~Call() {}

void Call::addHandler(std::function<void(pj::OnCallStateParam &)> &&handler) {
    onCallStateHandler = std::move(handler);
}

void Call::addHandler(std::function<void(pj::OnCallMediaStateParam &)> &&handler) {
    onCallMediaStateHandler = std::move(handler);
}

pj::AudioMedia *Call::audio_media() {

    pj::AudioMedia *aud_med{nullptr};

    auto ci = getInfo();

    for (unsigned i = 0; i < ci.media.size(); i++) {
        if (ci.media[i].type == PJMEDIA_TYPE_AUDIO && getMedia(i)) {
            aud_med = dynamic_cast<pj::AudioMedia *>(getMedia(i));
            break;
        }
    }

    return aud_med;
}

void Call::onCallState(pj::OnCallStateParam &prm) {
    if (onCallStateHandler) {
        onCallStateHandler(prm);
    }
}

void Call::onCallMediaState(pj::OnCallMediaStateParam &prm) {
    if (onCallMediaStateHandler) {
        onCallMediaStateHandler(prm);
    }
}

const string Call::localUser() {
    return local_user_;
}

Client::Client(std::unique_ptr<Account> account_, std::unique_ptr<AccountConfig> account_cfg_,
                     OptionalQueue<events::Event> &events_, Settings &settings,LogWriter *sip_log_writer)
        : account(std::move(account_)),
          account_cfg(std::move(account_cfg_)),
          events(events_)
          { 

    static auto ep = std::make_unique<pj::Endpoint>();

    if (ep->libGetState() == PJSUA_STATE_NULL) 
        init_pj_endpoint(settings,sip_log_writer);

    account->addHandler([this](pj::OnIncomingCallParam &prm) {
        auto call = std::make_shared<Call>(*account, prm.callId);
        auto ci = call->getInfo();

        call->local_user_ = user_from_uri(ci.localUri);
        calls.emplace(call->getId(), call);

        set_default_handlers(call);
        events.emplace(events::IncomingCall{ci.id, call->localUser()});
    });

}

Client::~Client() {
}

void Client::init_pj_endpoint(Settings &settings,LogWriter *sip_log_writer) {
  
    using namespace pj;

    auto &ep = pj::Endpoint::instance();

    pj_log_set_level(0);
    ep.libCreate();
    EpConfig ep_cfg;

    ep_cfg.logConfig.msgLogging = 0;
    ep_cfg.logConfig.consoleLevel = 6;

    // TODO: partially remove decor. Leave filename. Could require pjsip recompile.
    ep_cfg.logConfig.decor = 0;
    ep_cfg.logConfig.writer = sip_log_writer;

    // https://trac.pjsip.org/repos/wiki/FAQ#high-perf
    // disable echo canceller
    ep_cfg.medConfig.ecTailLen = 0;
    // set SIP threads number
    ep_cfg.medConfig.threadCnt = settings.sip_thread_count();

    // 10ms ptime required to keep L16 RTP packet below MTU
    ep_cfg.medConfig.audioFramePtime = 10;
    ep_cfg.medConfig.ptime = 10;

    // must be the same as used in media ports
    ep_cfg.medConfig.clockRate = 48000;

    // in case of trouble check:
    //      PJSUA_MAX_CALLS
    //      PJMEDIA_MAX_MTU
    //      PJ_IOQUEUE_MAX_HANDLES = calls x2
    //      PJSUA_MAX_PLAYERS = max calls

    ep_cfg.uaConfig.maxCalls = PJSUA_MAX_CALLS;

    ep.libInit(ep_cfg);
    ep.audDevManager().setNullDev();

    // pjSIP with switch board require matching of SIP audio
    // and TG audio port clock rate so we MUST force
    // using 48kHz codecs for all SIP calls
    std::string codecId = false ? "L16/48000/1" : "opus/48000/2";
    CodecInfoVector codecVector = ep.codecEnum();

    for (auto const &value : codecVector) {
        ep.codecSetPriority(value->codecId, (pj_uint8_t) (value->codecId == codecId ? 255 : 0));
    }

    TransportConfig t_cfg;

    // defaults to any open port
    t_cfg.port = settings.sip_port();
    t_cfg.portRange = 0;

 auto public_address = settings.public_address();
    if (!public_address.empty())
       t_cfg.publicAddress = public_address;

    ep.transportCreate(PJSIP_TRANSPORT_UDP, t_cfg);

    ep.libStart();
}

void Client::start() {
    pthread_setname_np(pthread_self(), "sip");
    account->create(*account_cfg);
}

std::string Client::user_from_uri(const std::string &uri) {
    std::string user{};

    pj_pool_t *pj_pool = pjsua_pool_create("temp%p", 2048, 1024);

    if (pj_pool != nullptr) {

        std::vector<char> buffer(uri.begin(), uri.end());
        buffer.push_back('\0');

        pjsip_uri *pj_uri = pjsip_parse_uri(pj_pool, &buffer[0], uri.size(), PJSIP_PARSE_URI_IN_FROM_TO_HDR);

        if (pj_uri != nullptr && PJSIP_URI_SCHEME_IS_SIP(pj_uri)) {
            auto pj_sip_uri = static_cast<pjsip_sip_uri *>(pjsip_uri_get_uri(pj_uri));
            user = std::string(pj_sip_uri->user.ptr,
                               static_cast<unsigned long>(pj_sip_uri->user.slen));
        }

        pj_pool_release(pj_pool);
    }

    return user;
}

pjsua_call_id Client::Dial(const std::string &uri, const pj::CallOpParam &prm) {
    auto call = std::make_shared<Call>(*account);
    set_default_handlers(call);

    // sip ID is known only after making call
    call->makeCall(uri, prm);
    auto sip_id = call->getId();
    calls.emplace(sip_id, call);

    return sip_id;
}

void Client::set_default_handlers(const std::shared_ptr<Call> &call) {
    call->addHandler([this, call_wpt = std::weak_ptr<Call>(call)](pj::OnCallStateParam &prm) {
        if (auto call_spt = call_wpt.lock()) {
            events.emplace(events::CallStateUpdate{call_spt->getId(), call_spt->getInfo().state});
        }
    });

    call->addHandler([this, call_wpt = std::weak_ptr<Call>(call)](pj::OnCallMediaStateParam &prm) {
        if (auto call_spt = call_wpt.lock()) {
            events.emplace(events::CallMediaStateUpdate{call_spt->getId(), call_spt->hasMedia()});
        }
    });
}

void Client::Hangup(const pjsua_call_id call_id, const pj::CallOpParam &prm) {
    auto it = calls.find(call_id);
    if (it != calls.end()) {
        auto call = it->second;
        call->hangup(prm);
        calls.erase(it);
    }

}

void Client::BridgeAudio(const pjsua_call_id call_id, pj::AudioMedia *input, pj::AudioMedia *output) {

    auto it = calls.find(call_id);

    if (it == calls.end()) {
        throw std::runtime_error{"CALL_NOT_FOUND"};
    }

    auto sip_audio = it->second->audio_media();

    if (sip_audio == nullptr) {
        throw std::runtime_error{"SIP_MEDIA_NOT_READY"};
    }

    sip_audio->startTransmit(*input);
    output->startTransmit(*sip_audio);

}

void Client::Answer(pjsua_call_id call_id, const pj::CallOpParam &prm) {
    auto it = calls.find(call_id);

    if (it == calls.end()) {
        throw std::runtime_error{"CALL_NOT_FOUND"};
    }

    auto call = it->second;
    call->answer(prm);
}

void Client::DialDtmf(pjsua_call_id call_id, const string &dtmf_digits) {
    auto it = calls.find(call_id);

    if (it == calls.end()) {
        throw std::runtime_error{"CALL_NOT_FOUND"};
    }

    auto call = it->second;
    call->dialDtmf(dtmf_digits);
}

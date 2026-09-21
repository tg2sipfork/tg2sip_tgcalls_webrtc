#ifndef TG2SIP_CONTROLLER_HPP
#define TG2SIP_CONTROLLER_HPP

#include <td/telegram/Client.h>
#include "prod/settings.h"
#include "logging.h"



namespace pj {
    class AudioMedia;
}
namespace tg {
    class Client;
}

class Gateway;

class Controller {
public:
    virtual void Start() = 0;
    virtual void Stop() = 0;
    virtual void Connect() = 0;

    virtual void UpdateSignaling(const std::string &s) {};

    virtual int32_t GetConnectionMaxLayer() = 0;

    virtual pj::AudioMedia* AudioMediaInput() = 0;
    virtual pj::AudioMedia* AudioMediaOutput() = 0;

    struct CreateCtx {
        Gateway *gw;
        std::reference_wrapper<const td::td_api::object_ptr<td::td_api::updateCall>> event;
        std::reference_wrapper<const Settings> settings;
        std::shared_ptr<spdlog::logger> logger;
    };
};

#endif // TG2SIP_CONTROLLER_HPP














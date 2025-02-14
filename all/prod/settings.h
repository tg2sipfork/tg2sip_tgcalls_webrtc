
#ifndef TG2SIP_SETTINGS_H
#define TG2SIP_SETTINGS_H

#include "INIReader.h"

class Settings {
private:
    bool is_loaded_{false};
    int tdlib_log_level_;    

    unsigned int sip_port_;
    std::string id_uri_;
    std::string callback_uri_;
    std::string public_address_;
    unsigned int sip_thread_count_;

    int api_id_;
    std::string api_hash_;

    std::string system_language_code_;
    std::string device_model_;
    std::string system_version_;
    std::string application_version_;
    
    bool proxy_enabled_;
    std::string proxy_address_;
    std::int32_t proxy_port_;
    std::string proxy_username_;
    std::string proxy_password_;
public:
    explicit Settings(INIReader &reader);

    Settings(const Settings &) = delete;

    Settings &operator=(const Settings &) = delete;

    bool is_loaded() const { return is_loaded_; };
 
    int tdlib_log_level() const { return tdlib_log_level_; };


    int api_id() const { return api_id_; };

    unsigned int sip_port() const { return sip_port_; };
 
    string id_uri() const { return id_uri_; };

    string public_address() const { return public_address_; };

    unsigned int sip_thread_count() const { return sip_thread_count_; };

    std::string api_hash() const { return api_hash_; };

     string callback_uri() const { return callback_uri_; };

    std::string sys_lang_code() const { return system_language_code_; };

    std::string device_model() const { return device_model_; };

    std::string system_version() const { return system_version_; };

    std::string app_version() const { return application_version_; };

    bool proxy_enabled() const { return proxy_enabled_; };

    std::string proxy_address() const { return proxy_address_; };

    int32_t proxy_port() const { return proxy_port_; };

    std::string proxy_username() const { return proxy_username_; };

    std::string proxy_password() const { return proxy_password_; };


};

#endif //TG2SIP_SETTINGS_H

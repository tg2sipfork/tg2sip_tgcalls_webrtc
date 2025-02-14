#include "settings.h"

Settings::Settings(INIReader &reader) {

    tdlib_log_level_ = static_cast<int>(reader.GetInteger("logging", "tdlib", 0));

    sip_port_ = static_cast<unsigned int>(reader.GetInteger("sip", "port", 0));
    id_uri_ = reader.Get("sip", "id_uri", "sip:localhost");
    callback_uri_ = reader.Get("sip", "callback_uri", "");
    public_address_ = reader.Get("sip", "public_address", "");
    sip_thread_count_ = static_cast<unsigned int>(reader.GetInteger("sip", "thread_count", 1));

    api_id_ = static_cast<int>(reader.GetInteger("telegram", "api_id", 0));
    api_hash_ = reader.Get("telegram", "api_hash", "");
    system_language_code_ = reader.Get("telegram", "system_language_code", "");
    device_model_ = reader.Get("telegram", "device_model", "");
    system_version_ = reader.Get("telegram", "system_version", "");
    application_version_ = reader.Get("telegram", "application_version", "");

    proxy_enabled_ = reader.GetBoolean("telegram", "use_proxy", false);
    proxy_address_ = reader.Get("telegram", "proxy_address", "");
    proxy_port_ = static_cast<int32_t>(reader.GetInteger("telegram", "proxy_port", 0));
    proxy_username_ = reader.Get("telegram", "proxy_username", "");
    proxy_password_ = reader.Get("telegram", "proxy_password", "");

    is_loaded_ = true;
}

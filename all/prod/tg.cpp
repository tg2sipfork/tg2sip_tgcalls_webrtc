#include "tg.h"
#include "queue.h"
#include <iostream>
#include <fstream>
#include <td/telegram/Log.h>
#include <ctime>
#include <chrono>
#include <thread>
#include <iomanip>
using namespace tg;
using namespace std;


Client::Client(Settings &settings,
               OptionalQueue<Object> &events_)
        :events(events_) {

    client = std::make_unique<td::Client>();
    init_lib_parameters(settings);
    td::ClientManager::execute(td_api::make_object<td_api::setLogVerbosityLevel>(settings.tdlib_log_level()));
    
   if (settings.tdlib_log_level() > 1 )
         td::Log::set_file_path("tdlib.cpp");


    if (settings.proxy_enabled()) {
        auto socks_proxy_type = td_api::make_object<td_api::proxyTypeSocks5>(
                settings.proxy_username(),
                settings.proxy_password()
        );
        set_proxy = td_api::make_object<td_api::addProxy>(
                settings.proxy_address(),
                settings.proxy_port(),
                true,
                td_api::move_object_as<td_api::ProxyType>(socks_proxy_type));
    } else {
        set_proxy = td_api::make_object<td_api::disableProxy>();
    }

    send_query_async(std::move(set_proxy));


}

Client::~Client() {
    is_closed = true;
    client.release();
}

void Client::init_lib_parameters(Settings &settings) {

    lib_parameters  = td_api::make_object<td_api::setTdlibParameters>();
    lib_parameters->api_id_ = settings.api_id();
    lib_parameters->api_hash_ = settings.api_hash();

    lib_parameters->system_language_code_ = settings.sys_lang_code();
    lib_parameters->device_model_ = settings.device_model();
    lib_parameters->system_version_ = settings.system_version();
    lib_parameters->application_version_ = settings.app_version();
}

void Client::loop() {

    while (!is_closed) {
        auto response = client->receive(WAIT_TIMEOUT);
        process_response(std::move(response));
    }

}

void Client::start() {
    pthread_setname_np(pthread_self(), "tg");
    thread_ = std::thread(&Client::loop, this);
}

void Client::process_response(td::Client::Response response) {

    if (!response.object)
        return;

    if (response.id == 0)
        return process_update(std::move(response.object));

    auto it = handlers.find(response.id);
    if (it != handlers.end()) {
        it->second(std::move(response.object));
        handlers.erase(it->first);
    }
}


void Client::send_query(td_api::object_ptr<td_api::Function> f, std::function<void(Object)> handler) {
    auto query_id = next_query_id();
    if (handler) {
        handlers.emplace(query_id, std::move(handler));
    }
    client->send({query_id, std::move(f)});
}

std::future<Client::Object> Client::send_query_async(td_api::object_ptr<td_api::Function> f) {

    if (std::this_thread::get_id() == thread_.get_id()) 
        throw;

    auto promise = std::make_shared<std::promise<Object>>();
    auto future = promise->get_future();

    send_query(std::move(f), [this, promise](Object object) {
        try {
            promise->set_value(std::move(object));
        } catch (std::future_error &error) {
            cout<<"failed to set send_query_async"<<error.code()<<endl;
        }
    });

    return future;
}

std::uint64_t Client::next_query_id() {
    return ++current_query_id;
}

auto Client::create_authentication_query_handler() {
    return [this](Object object) {
        check_authentication_error(std::move(object));
    };
}


void Client::check_authentication_error(Object object) {
    if (object->get_id() == td_api::error::ID) {
        auto error = td_api::move_object_as<td_api::error>(object);
    }
}


void Client::process_update(Object update) {
    switch (update->get_id()) {
        case td_api::updateAuthorizationState::ID: {
            auto update_authorization_state = td_api::move_object_as<td_api::updateAuthorizationState>(update);
            on_authorization_state_update(std::move(update_authorization_state->authorization_state_));
            break;
        }
        case td_api::updateCall::ID: 
        case td_api::updateNewCallSignalingData::ID:
        case td_api::updateNewMessage::ID: {
            events.emplace(std::move(update));
            break;
        }
        default:
            break;
    }
}



void Client::on_authorization_state_update(td_api::object_ptr<td_api::AuthorizationState> authorization_state) {
    switch (authorization_state->get_id()) {
    case td_api::authorizationStateReady::ID:
            is_ready_.set_value(true);
            break;
    case td_api::authorizationStateWaitTdlibParameters::ID: {
            send_query(std::move(lib_parameters), create_authentication_query_handler());
            break;
        }

        default:
            is_ready_.set_value(false);
            break;
    }
}


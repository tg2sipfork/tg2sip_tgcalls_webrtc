#include <fstream>
#include <iostream>
#include "queue.h"
#include "tg.h"
#include "sip.h"
#include "gateway.h"
#include <td/telegram/Log.h>
#include <ctime>
#include <chrono>
#include <thread>
#include <iomanip>


int main() {
    
    pthread_setname_np(pthread_self(), "main");
    auto reader = INIReader("settings.ini");
    Settings settings(reader);
    OptionalQueue<sip::events::Event> sip_events;
    auto sip_log_writer = new sip::LogWriter();
    auto sip_account = std::make_unique<sip::Account>();
    auto sip_account_cfg = std::make_unique<sip::AccountConfig>(settings);
   
    auto sip_client = std::make_unique<sip::Client>(
            std::move(sip_account),
            std::move(sip_account_cfg),
            sip_events,
            settings,
            sip_log_writer
    );

             try {
                 sip_client->start();
            } catch (int e) {
                std::cout << "sip exit.." << e;
                exit(1);
            }

    OptionalQueue<tg::Client::Object> tg_events;
    auto tg_client = std::make_unique<tg::Client>(settings, tg_events);
    tg_client->start();
    auto tg_is_ready_future = tg_client->is_ready();
    auto gateway = std::make_unique<Gateway>(*sip_client, *tg_client, sip_events, tg_events,settings);
    gateway->start();

    return 0;
}

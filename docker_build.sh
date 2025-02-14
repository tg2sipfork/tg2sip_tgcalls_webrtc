#!/bin/bash

# TG2SIP Webrtc fork

#image 22.04 \ gcc 11 \ c++ 17 \ tdlib latest git \ pjsip 2.9 
#need edit settings file before build to add all settings that need
#nano settings.ini

#build
docker build -t tg:latest .

#run
#docker compose up -d

#in docker
#docker exec -it tg bash

#gw in build dir
#cd /home/prod/build

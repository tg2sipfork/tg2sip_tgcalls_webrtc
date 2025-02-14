cmake_minimum_required(VERSION 3.16)
project(prod)
add_definitions(-w)
set(CMAKE_CXX_STANDARD 17)
find_package(PkgConfig REQUIRED)
find_package(Td 1.8.45 REQUIRED)
pkg_check_modules(PJSIP libpjproject=2.9 REQUIRED)
pkg_check_modules(OPUS opus REQUIRED)
include(${CMAKE_HOME_DIRECTORY}/voip/CMakeLists.txt)
add_executable(prod
        prod/main.cpp
        prod/tg.cpp
        prod/tg.h
        prod/sip.cpp
        prod/sip.h
        prod/queue.h
        prod/gateway.cpp
        prod/settings.cpp
        prod/settings.h
        prod/gateway.h 
        prod/controller/PjStubInterface.cpp
        prod/controller/PjStubInterface
        prod/controller/library.cpp
        prod/controller/library.hpp
        prod/controller/controller.hpp
        prod/controller/tgcalls.cpp
        prod/controller/tgcalls.hpp
        prod/controller/pjrtc-adm.cpp
        prod/controller/pjrtc-adm.hpp
        prod/controller/pjrtc-audio-in.cpp
        prod/controller/pjrtc-audio-in.hpp
        prod/controller/pjrtc-audio-out.cpp
        prod/controller/pjrtc-audio-out.hpp
        )

target_include_directories(prod PRIVATE
        ${PROJECT_SOURCE_DIR}
        ${OPUS_INCLUDE_DIRS}
        prod)

target_link_libraries(prod PRIVATE
        ${PJSIP_LIBRARIES}
        ${OPUS_LIBRARIES}
        Td::TdStatic
        tgcalls
        tgcalls_tp)

add_executable(gen_db
        prod/gen_db.cpp)

target_include_directories(gen_db PRIVATE
        include)

target_link_libraries(gen_db PRIVATE
        Td::TdStatic)

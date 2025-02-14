FROM ubuntu:22.04

MAINTAINER of Product <sip.to.tg@gmail.com>

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get -yqq update \
    && apt-get -yqq install git build-essential pkg-config cmake wget gdb \
       gperf zlib1g-dev libssl-dev nasm ccache build-essential git \
        wget ca-certificates \
        pkg-config libopus-dev libssl-dev \
        zlib1g-dev gperf ccache cmake libavutil-dev libavformat-dev

RUN apt-get update \
    && apt-get install -y \
        build-essential git \
        wget ca-certificates \
        pkg-config libopus-dev libssl-dev \
        zlib1g-dev gperf ccache 

RUN wget https://cmake.org/files/v3.18/cmake-3.18.0-Linux-x86_64.sh \
    && sh cmake-3.18.0-Linux-x86_64.sh --prefix=/usr --exclude-subdir


# Install the app-specific packages
RUN apt-get -yqq install libopus-dev

# Get & build Td lib. Note: MASTER!
WORKDIR /tmp
RUN git clone --depth=1 https://github.com/tdlib/td.git td && mkdir td_BUILD
RUN cd td_BUILD \
    && cmake -DCMAKE_BUILD_TYPE=Release ../td \
    && cmake --build . -- -j10 \
    && make install

# Get & build pjsip. NOTE: It is patched from the tgcalls repo (see COPY)!
WORKDIR /tmp
RUN wget https://github.com/pjsip/pjproject/archive/refs/tags/2.9.tar.gz \
    && tar -xzf 2.9.tar.gz
COPY config_site.h pjproject-2.9/pjlib/include/pj
RUN cd pjproject-2.9 \
    && ./aconfigure --prefix=/usr CFLAGS="-O3 -DNDEBUG" --enable-shared --disable-pjsua2 \ 
    --disable-video --disable-sound --disable-speex-aec --disable-g711-codec \ 
    --disable-l16-codec --disable-gsm-codec --disable-g722-codec --disable-g7221-codec \
    --disable-speex-codec --disable-ilbc-codec --disable-opencore-amrnb --disable-sdl \
    --disable-ffmpeg --disable-v4l2 --disable-openh264 --disable-vpx \
    --disable-android-mediacodec  --disable-ssl  --disable-darwin-ssl  --disable-silk \ 
    --disable-bcg729 --disable-libsrtp --disable-libyuv --disable-libwebrtc \
    && make -j10 \
    && make install

# Get & build libvpx. NOTE: webrtc expects includes under libvpx
# so ln it appropriately
WORKDIR /tmp
RUN git clone -b v1.11.0 --depth=1 https://github.com/webmproject/libvpx.git  libvpx \
    && cd libvpx \
    && ./configure --prefix=/usr\
        --disable-examples \
        --disable-unit-tests \
        --disable-tools \
        --disable-docs \
        --enable-vp8 \
        --enable-vp9 \
        --enable-webm-io \
    && make -j10 \
    && make install \
    && ln -s /usr/include/vpx /usr/include/libvpx


#build tg2dev

RUN cp -r /usr/include/opus/opus* /usr/include/

RUN mkdir -p /home/prod

COPY all /home/prod

WORKDIR /home/prod

RUN mkdir -p build

COPY settings.ini /home/prod/build/

RUN cd /home/prod/build && cmake -DCMAKE_BUILD_TYPE=Release .. && cmake --build . -j 10
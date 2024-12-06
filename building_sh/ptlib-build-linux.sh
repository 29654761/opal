

ABI=linux
CONFIG=release
ROOT_DIR=/cxx-build
INSTALL_DIR=${ROOT_DIR}/opal/out/${ABI}-${CONFIG}/ptlib


# tar -jxvf ptlib-2.18.8.tar.bz2

cd ./ptlib-2.18.8

export PKG_CONFIG_PATH=${ROOT_DIR}/openssl/out/${ABI}-${CONFIG}/lib64/pkgconfig
export LDFLAGS="-ldl"

./configure --prefix=${INSTALL_DIR} --disable-shared --enable-cpp11 \
    --disable-ftp \
    --disable-snmp \
    --disable-telnet \
    --disable-pop3smtp \
    --disable-http \
    --disable-video \
    --disable-audio \
    --disable-plugins \
    --disable-dtmf \
    --disable-nat \
    --disable-stun \
    --disable-stunsrvr \
    --disable-pipechan \
    --disable-soap \
    --disable-serial \
    --disable-sdl \
    --disable-alsa \
    --disable-esd \
    --disable-oss \
    --enable-pulse \
    --disable-sunaudio \
    --disable-shmaudio \
    --disable-portaudio \
    --disable-mediafile \
    --disable-wavfile \
    --disable-vidfile \
    --disable-gstreamer \
    --disable-shmvideo \
    --disable-curses \
    --disable-appshare \
    --disable-libjpe \
    --disable-imagemagick \
    --disable-cli

make
make install

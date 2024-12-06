

#ABI=arm64-v8a,armeabi-v7a
ABI=$1
CONFIG=release
ROOT_DIR=/Users/seastart/cpp/cxx-build
INSTALL_DIR=${ROOT_DIR}/opal/out/${ABI}-${CONFIG}/ptlib



export ANDROID_NDK_ROOT=/Applications/AndroidNDK11394342.app/Contents/NDK
TOOLCHAIN=$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/darwin-x86_64
SYSROOT="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/darwin-x86_64/sysroot"
API=26





export AR=$TOOLCHAIN/bin/llvm-ar
export LD=$TOOLCHAIN/bin/ld
export STRINGS=$TOOLCHAIN/bin/llvm-strings
export STRIP=$TOOLCHAIN/bin/llvm-strip
export RANLIB=$TOOLCHAIN/bin/llvm-ranlib

export CFLAGS="${CFLAGS} -fpic -DP_NO_CANCEL -DP_ANDROID -DP_STD_ATOMIC -DHAVE_IFADDRS_H -DP_GETGRGID_R5 -DP_GETGRNAM_R5"
export CXXFLAGS="${CFLAGS} -Wno-enum-constexpr-conversion"




if [ "${ABI}" == "arm64-v8a" ];then
    HOST=aarch64-linux-android
    TARGET=aarch64-linux-android
    export CROSS_COMPILE=TOOLCHAIN/bin/$TARGET$API-
    export CC=$TOOLCHAIN/bin/$TARGET$API-clang
    export CXX=$TOOLCHAIN/bin/$TARGET$API-clang++
    export CFLAGS="${CFLAGS} -I${SYSROOT}/usr/include/aarch64-linux-android"
elif [ "${ABI}" == "armeabi-v7a" ]; then
    HOST=armv7a-linux-android
    export CROSS_COMPILE=TOOLCHAIN/bin/$TARGET$API-
    TARGET=armv7a-linux-androideabi
    export CC=$TOOLCHAIN/bin/$TARGET$API-clang
    export CXX=$TOOLCHAIN/bin/$TARGET$API-clang++
    export CFLAGS="${CFLAGS} -I${SYSROOT}/usr/include/arm-linux-androideabi"
fi


# tar -jxvf ptlib-2.18.8.tar.bz2

cd ./ptlib-2.18.8

export PKG_CONFIG_PATH=${ROOT_DIR}/openssl/out/${ABI}-${CONFIG}/lib/pkgconfig

./configure --prefix=${INSTALL_DIR} --host=${HOST} --disable-shared --enable-cpp17 \
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

make clean
make
make install

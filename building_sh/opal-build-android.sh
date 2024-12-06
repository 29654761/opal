
#ABI=arm64-v8a,armeabi-v7a
ABI=$1
CONFIG=release
ROOT_DIR=/Users/seastart/cpp/cxx-build
INSTALL_DIR=${ROOT_DIR}/opal/out/${ABI}-${CONFIG}/opal


export ANDROID_NDK_ROOT=/Applications/AndroidNDK11394342.app/Contents/NDK
TOOLCHAIN=$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/darwin-x86_64
SYSROOT="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/darwin-x86_64/sysroot"
API=26


export AR=$TOOLCHAIN/bin/llvm-ar
export LD=$TOOLCHAIN/bin/ld
export STRINGS=$TOOLCHAIN/bin/llvm-strings
export STRIP=$TOOLCHAIN/bin/llvm-strip
export RANLIB=$TOOLCHAIN/bin/llvm-ranlib

export CFLAGS="${CFLAGS} -fpic -DP_NO_CANCEL"
export CXXFLAGS="${CFLAGS} -Wno-enum-constexpr-conversion"
export LDFLAGS="${LDFLAGS} -static-libstdc++"




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



# tar -jxvf opal-3.18.8.tar.bz2


export PKG_CONFIG_PATH=${ROOT_DIR}/openssl/out/${ABI}-${CONFIG}/lib/pkgconfig:${ROOT_DIR}/opal/out/${ABI}-${CONFIG}/ptlib/lib/pkgconfig
export LDFLAGS="-ldl"
echo $PKG_CONFIG_PATH

cd ./opal-3.18.8

./configure --prefix=${INSTALL_DIR} --host=${HOST} --disable-shared --enable-cpp17 \
    --disable-java \
    --disable-csharp \
    --disable-ruby \
    --disable-srtp \
    --disable-sip \
    --disable-sipim \
    --disable-sdp \
    --disable-msrp \
    --disable-rfc4103 \
    --disable-skinny \
    --disable-t38 \
     --disable-mixer \
    --disable-plugins

make clean
make
make install

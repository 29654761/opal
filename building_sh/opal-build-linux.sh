

ABI=linux
CONFIG=release
ROOT_DIR=/cxx-build
INSTALL_DIR=${ROOT_DIR}/opal/out/${ABI}-${CONFIG}/opal

#tar -jxvf opal-3.18.8.tar.bz2


export PKG_CONFIG_PATH=${ROOT_DIR}/openssl/out/${ABI}-${CONFIG}/lib64/pkgconfig:${ROOT_DIR}/opal/out/${ABI}-${CONFIG}/ptlib/lib/pkgconfig
export LDFLAGS="-ldl"
echo $PKG_CONFIG_PATH

cd ./opal-3.18.8

./configure --prefix=${INSTALL_DIR} --disable-shared --enable-cpp11 \
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
make
make install

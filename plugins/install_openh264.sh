#!/bin/sh

set -e

if [ `whoami` != root ]; then
   echo "`basename $0` must be run as root!"
   exit 1
fi

VERSION=2.1.1
LIBCVER=6

LIBDIR=/usr/local/lib

case `uname -sm` in
   Linux*x86_64 )
     PLATFORM=linux64
     LIBEXT=so
     LINKEXT=${LIBEXT}.${LIBCVER}
     LIBDIR=/usr/local/lib64
   ;;
   Linux*x86 )
     PLATFORM=linux32
     LIBEXT=so
     LINKEXT=${LIBEXT}.${LIBCVER}
   ;;
   Darwin*x86_64 )
     PLATFORM=osx64
     LIBEXT=dylib
     LINKEXT=${LIBCVER}.${LIBEXT}
   ;;
   Darwin*x86 )
     PLATFORM=osx32
     LIBEXT=dylib
     LINKEXT=${LIBCVER}.${LIBEXT}
   ;;
   * )
     echo Unknown platform `uname -sm`
     exit 2
   ;;
esac

LIBBASE=libopenh264
LIBFILE=${LIBBASE}-${VERSION}-${PLATFORM}.${LIBCVER}.${LIBEXT}

if [ -n "$1" ]; then
    LIBDIR="$1"
fi

if [ \! -d $LIBDIR ]; then
   mkdir -p $LIBDIR
fi

cd $LIBDIR
if [ -e "$LIBFILE" ]; then
    echo Already installed.
else
    echo Installing $LIBBASE to $LIBDIR.
    rm -f ${LIBBASE}*
    curl --silent http://ciscobinary.openh264.org/$LIBFILE.bz2 | bunzip2 > $LIBFILE
    ln -s $LIBFILE ${LIBBASE}.${LINKEXT}
    ln -s ${LIBBASE}.${LINKEXT} ${LIBBASE}.${LIBEXT}
fi
ls -l $LIBDIR/${LIBBASE}*

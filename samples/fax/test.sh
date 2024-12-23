#!/bin/bash

#set -x

CURDIR=`pwd`

FAXOPAL=`ls $CURDIR/obj_*/faxopal | head -1`
if [ -z "$FAXOPAL" ]; then
  echo "faxopal not built."
  exit 1
fi

RESULT_DIR=$CURDIR/results
if [ -d $RESULT_DIR ]; then
  rm -f $RESULT_DIR/*
else
  mkdir $RESULT_DIR || exit $?
fi

HOST=127.0.0.1

function test_fax()
{
  RESULT_PREFIX="$RESULT_DIR/${1}_${2}_${3}_"
  if [ -n "$4" ]; then
    RESULT_PREFIX+="${4}_"
  fi

  TX_ARG="-o${RESULT_PREFIX}tx.log"
  RX_ARG="-o${RESULT_PREFIX}rx.log"
  XX_ARG="-ttttt --trace-option +file+date-time --no-lid --no-capi --no-sdp --timeout 2:30"

  if [ "$1" = "sip" ]; then
    TX_ARG+=" --sip $HOST:25060"
    RX_ARG+=" --sip $HOST:15060"
    XX_ARG+=" --no-h323"
    DEST_URL="sip:$HOST:15060"
  else
    TX_ARG+=" --h323 $HOST:21720"
    RX_ARG+=" --h323 $HOST:11720"
    XX_ARG+=" --no-sip"
    DEST_URL="h323:$HOST:11720"
  fi

  if [ "$2" = "g711" ]; then
    TX_ARG+=" --audio"
  fi

  if [ "$3" = "g711" ]; then
    RX_ARG+=" --audio"
  fi

  if [ "$4" = "slow" ]; then
    XX_ARG+=" --no-fast"
  fi

  if [ "$2" = "t38" -a "$3" = "g711" ]; then
    TX_ARG+=" --switch-on-ced"
  fi

  if [ "$2" = "t38" -a "$3" = "t38" ]; then
    XX_ARG+=" --no-fallback"
  fi

  TX_ARG+=" $CURDIR/F06_200.tif $DEST_URL"
  RX_ARG+=" ${RESULT_PREFIX}rx.tif"

  TX_OUT=${RESULT_PREFIX}tx.out
  RX_OUT=${RESULT_PREFIX}rx.out

  echo $FAXOPAL $XX_ARG $TX_ARG > $TX_OUT
  echo $FAXOPAL $XX_ARG $RX_ARG > $RX_OUT


  $FAXOPAL $XX_ARG $RX_ARG < /dev/null >> $RX_OUT 2>&1 &
  rx_pid=$!
  sleep 2
  $FAXOPAL $XX_ARG $TX_ARG < /dev/null >> $TX_OUT 2>&1 &
  tx_pid=$!

  echo -n "Performing test:$4 $1, $2 (pid=$tx_pid) -> $3 (pid=$rx_pid) ... "
  wait $rx_pid $tx_pid

  if grep -q Success $TX_OUT && grep -q Success $RX_OUT ; then
    echo "successful."
  else
    echo "failed."
  fi
}

if [ -n "$PTLIBDIR" -a -n "$OPALDIR" ]; then
  # The MacOSX "System Integrity Protection" kills this from the parent shell
  export DYLD_LIBRARY_PATH=$PTLIBDIR/lib_Darwin_x86_64:$OPALDIR/lib_Darwin_x86_64
fi

if [ $# -ge 3 ]; then
  test_fax $*
elif [ $# = 0 ]; then
  test_fax sip t38  t38
  test_fax sip g711 t38
  test_fax sip t38  g711
  test_fax sip g711 g711

  test_fax h323 t38  t38  fast
  test_fax h323 g711 t38  fast
  test_fax h323 t38  g711 fast
  test_fax h323 g711 g711 fast

  test_fax h323 t38  t38  slow
  test_fax h323 g711 t38  slow
  test_fax h323 t38  g711 slow
  test_fax h323 g711 g711 slow
else
  echo "usage: $0 { sip | h323 } { t38 | g711 } { t38 | g711 } [ slow | fast ]"
fi


#
# opal_config.mak
#
# Make symbols include file for Open Phone Abstraction library
#
# Copyright (c) 2001 Equivalence Pty. Ltd.
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
# the License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is Open Phone Abstraction library.
#
# The Initial Developer of the Original Code is Equivalence Pty. Ltd.
#
# Contributor(s): ______________________________________.
#

OPAL_MAJOR :=@OPAL_MAJOR@
OPAL_MINOR :=@OPAL_MINOR@
OPAL_STAGE :=@OPAL_STAGE@
OPAL_PATCH :=@OPAL_PATCH@
OPAL_OEM   :=@OPAL_OEM@

# detected platform
target_cpu       := @target_cpu@
target_os        := @target_os@
target           := @target@

# The install directories
ifndef prefix
  prefix := @prefix@
endif
ifndef exec_prefix
  exec_prefix := @exec_prefix@
endif
ifndef libdir
  libdir := @libdir@
endif
ifndef includedir
  includedir := @includedir@
endif
ifndef datarootdir
  datarootdir := @datarootdir@
endif

OPAL_PLUGIN_DIR  := @OPAL_PLUGIN_DIR@

# Tool names detected by configure
CC               := @CC@
CXX              := @CXX@
LD               := @LD@
AR               := @AR@
RANLIB           := @RANLIB@
LN_S             := @LN_S@
MKDIR_P          := @MKDIR_P@
INSTALL          := @INSTALL@
SVN              := @SVN@
SWIG             := @SWIG@

# Compile/tool flags
CPPFLAGS         := @CPPFLAGS@ $(CPPFLAGS)
CXXFLAGS         := @CXXFLAGS@ $(CXXFLAGS)
CFLAGS           := @CFLAGS@ $(CFLAGS)
LDFLAGS          := @LDFLAGS@ $(LDFLAGS)
LIBS             := @LIBS@ $(LIBS)
SHARED_CPPFLAGS  := @SHARED_CPPFLAGS@
SHARED_LDFLAGS    = @SHARED_LDFLAGS@
DEBUG_CPPFLAGS   := @DEBUG_CPPFLAGS@
DEBUG_CFLAGS     := @DEBUG_CFLAGS@
OPT_CPPFLAGS     := @OPT_CPPFLAGS@
OPT_CFLAGS       := @OPT_CFLAGS@
ARFLAGS          := @ARFLAGS@

SHAREDLIBEXT     := @SHAREDLIBEXT@
STATICLIBEXT     := @STATICLIBEXT@

# Configuration options
OPAL_PLUGINS     := @OPAL_PLUGINS@
OPAL_SAMPLES     := @OPAL_SAMPLES@

OPAL_H323        := @OPAL_H323@
OPAL_SDP         := @OPAL_SDP@
OPAL_SIP         := @OPAL_SIP@
OPAL_IAX2        := @OPAL_IAX2@
OPAL_SKINNY      := @OPAL_SKINNY@
OPAL_VIDEO       := @OPAL_VIDEO@
OPAL_ZRTP        := @OPAL_ZRTP@
OPAL_LID         := @OPAL_LID@
OPAL_CAPI        := @OPAL_CAPI@
OPAL_DAHDI       := @OPAL_DAHDI@
OPAL_IVR         := @OPAL_IVR@
OPAL_HAS_H224    := @OPAL_HAS_H224@
OPAL_HAS_H281    := @OPAL_HAS_H281@
OPAL_H235_6      := @OPAL_H235_6@
OPAL_H235_8      := @OPAL_H235_8@
OPAL_H450        := @OPAL_H450@
OPAL_H460        := @OPAL_H460@
OPAL_H501        := @OPAL_H501@
OPAL_T120DATA    := @OPAL_T120DATA@
OPAL_SRTP        := @OPAL_SRTP@
SRTP_SYSTEM      := @SRTP_SYSTEM@
OPAL_RFC4175     := @OPAL_RFC4175@
OPAL_RFC2435     := @OPAL_RFC2435@
OPAL_AEC         := @OPAL_AEC@
OPAL_G711PLC     := @OPAL_G711PLC@
OPAL_T38_CAP     := @OPAL_T38_CAPABILITY@
OPAL_FAX         := @OPAL_FAX@
OPAL_JAVA        := @OPAL_JAVA@
OPAL_CSHARP      := @OPAL_CSHARP@
SPEEXDSP_SYSTEM  := @SPEEXDSP_SYSTEM@
OPAL_HAS_PRESENCE:= @OPAL_HAS_PRESENCE@
OPAL_HAS_MSRP    := @OPAL_HAS_MSRP@
OPAL_HAS_SIPIM   := @OPAL_HAS_SIPIM@
OPAL_HAS_RFC4103 := @OPAL_HAS_RFC4103@
OPAL_HAS_MIXER   := @OPAL_HAS_MIXER@
OPAL_HAS_PCSS    := @OPAL_HAS_PCSS@

# PTLib interlocks
OPAL_PTLIB_SSL          := @OPAL_PTLIB_SSL@
OPAL_PTLIB_SSL_AES      := @OPAL_PTLIB_SSL_AES@
OPAL_PTLIB_ASN          := @OPAL_PTLIB_ASN@
OPAL_PTLIB_EXPAT        := @OPAL_PTLIB_EXPAT@
OPAL_PTLIB_AUDIO        := @OPAL_PTLIB_AUDIO@
OPAL_PTLIB_VIDEO        := @OPAL_PTLIB_VIDEO@
OPAL_PTLIB_WAVFILE      := @OPAL_PTLIB_WAVFILE@
OPAL_PTLIB_DTMF         := @OPAL_PTLIB_DTMF@
OPAL_PTLIB_IPV6         := @OPAL_PTLIB_IPV6@
OPAL_PTLIB_DNS_RESOLVER := @OPAL_PTLIB_DNS_RESOLVER@
OPAL_PTLIB_LDAP         := @OPAL_PTLIB_LDAP@
OPAL_PTLIB_VXML         := @OPAL_PTLIB_VXML@
OPAL_PTLIB_CONFIG_FILE  := @OPAL_PTLIB_CONFIG_FILE@
OPAL_PTLIB_HTTPSVC      := @OPAL_PTLIB_HTTPSVC@
OPAL_PTLIB_STUN         := @OPAL_PTLIB_STUN@
OPAL_PTLIB_CLI          := @OPAL_PTLIB_CLI@
OPAL_GSTREAMER          := @OPAL_PTLIB_GSTREAMER@

PTLIB_MAKE_DIR   := @PTLIB_MAKE_DIR@
PTLIB_LIB_DIR    := @PTLIB_LIB_DIR@


# Remember where this make file is, it is the platform specific one and there
# is a corresponding platform specific include file that goes with it
OPAL_PLATFORM_INC_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))../include)


# End of file


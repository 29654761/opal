/*
 * opal.h
 *
 * "C" language interface for OPAL
 *
 * Open Phone Abstraction Library (OPAL)
 *
 * Copyright (c) 2008 Vox Lucida
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Open Phone Abstraction Library.
 *
 * The Initial Developer of the Original Code is Vox Lucida (Robert Jongbloed)
 *
 * This code was initially written with the assisance of funding from
 * Stonevoice. http://www.stonevoice.com.
 *
 * Contributor(s): ______________________________________.
 */

#ifndef OPAL_OPAL_H
#define OPAL_OPAL_H

#include <stdlib.h>


#ifdef __cplusplus
extern "C" {
#endif

/**\file opal.h
   \brief This file contains a simplified API to the OPAL system. It provides a
    pure "C" language interface as will as a very simple C++ class and a
    contrained set of functions for "late binding".

    It should be noted that the simplified API, is sill not very simple. There are
    complexities invoved that cannot be avoided. However, this API does remove
    some issues with the full API such as multi-threading and advanced C++ concepts.

    The other major feature of this API is the ability to be  easily "late bound"
    using Windows LoadLibrary() or Unix dlopen() at run time. You may look at the
    sample code in opal/samples/c_api/main.c for an example of how to do late binding.

    Late binding also allows for easier integration of OPAL fucntionality to
    interpreted languages such as Java, Perl etc. Systems like "swig" may be used to
    produce interface files for such languages.

    To make the above easier, there are only four functions: OpalInitialise(),
    OpalShutDown(), OpalGetMessage() and OpalSendMessage(). All commands to OPAL and
    indications back from OPAL are done through the latter two functions.

    This API also provides a basic C++ class OpalContext, which may be used for C++
    programmers that do not wish to learn the large number of classes in the full
    API. At the cost of minimal extensibility and control.
  */

#ifdef _WIN32
  #define OPAL_EXPORT __stdcall
#else
  #define OPAL_EXPORT
#endif

/// Handle to initialised OPAL instance.
typedef struct OpalHandleStruct * OpalHandle;


typedef struct OpalMessage OpalMessage;

/// Current API version
#define OPAL_C_API_VERSION 40


///////////////////////////////////////

/** Initialise the OPAL system, returning a "handle" to the system that must
    be used in other calls to OPAL.

    The version parameter indicates the version of the API being used by the
    caller. It should always be set to the constant OPAL_C_API_VERSION. On
    return the library will indicate the API version it supports, if it is
    lower than that provided by the application.

    The C string options are space separated tokens indicating various options
    to be enabled, for example the protocols to be available. NULL or an empty
    string will load all available protocols. The current protocol tokens are:
        <code>
        sip sips h323 h323s iax2 pc local pots pstn ivr
        </code>
    The above protocols are in priority order, so if a protocol is not
    explicitly in the address, then the first one of the opposite "category"
    s used. There are two categories, network protocols (sip, h323, iax & pstn)
    and non-network protocols (pc, local, pots & ivr).

    Additional options are in similar form to command line arguments:
        <table border=0>
        <tr><td>-t or --trace         <td>Enable trace log. Multiple instances
                                          increase the trace level.
        <tr><td>-l or --trace-level X <td>Enable trace log and set level to X.
        <tr><td>-o or --output "name" <td>Set the filename for trace log output.
        <tr><td>-l or --trace-option X <td>Enable trace log option, use +X or -X
                                           to add/remove option where X is one of:
            <table>
            <tr><td>block    <td>PTrace::Block constructs in output
            <tr><td>time     <td>time since prgram start
            <tr><td>date     <td>date and time
            <tr><td>gmt      <td>Date/time is in UTC
            <tr><td>thread   <td>thread name and identifier
            <tr><td>level    <td>log level
            <tr><td>file     <td>source file name and line number
            <tr><td>object   <td>PObject pointer
            <tr><td>context  <td>context identifier
            <tr><td>daily    <td>rotate output file daily
            <tr><td>hour     <td>rotate output file hourly
            <tr><td>minute   <td>rotate output file every minute
            <tr><td>append   <td>append to output file, otherwise overwrites
                             &lt;perm&gt; <td>file permission similar to unix chmod,
                             but starts with +/- and only has one combination at a
                             time, e.g. +uw is user write, +or is other read, etc
            </table>
        <tr><td>-c or --config "dir"  <td>Configuration file or directory.
        <tr><td>-p or --plugin "dir"  <td>Plugin module directory.
        <tr><td>-m or --manaufacturer "str" <td>Manufacturer name for application.
        <tr><td>-n or --name "str"    <td>Product name for application.
        <tr><td>-M or --major X       <td>Major version number.
        <tr><td>-N or --minor X       <td>Minor version number.
        <tr><td>-R or --status X      <td>Code status ("alpha", "beta" or "release").
        <tr><td>-B or --build X       <td>Build/patch number.
        </table>
    It should also be noted that there must not be spaces around the '=' sign
    in the above options.

    If NULL is returned then an initialisation error occurred. This can only
    really occur if the user specifies prefixes which are not supported by
    the library.

    Example:
      <code>
      OpalHandle hOPAL;
      unsigned   version;

      version = OPAL_C_API_VERSION;
      if ((hOPAL = OpalInitialise(&version,
                                  OPAL_PREFIX_H323  " "
                                  OPAL_PREFIX_SIP   " "
                                  OPAL_PREFIX_IAX2  " "
                                  OPAL_PREFIX_PCSS
                                  " TraceLevel=4")) == NULL) {
        fputs("Could not initialise OPAL\n", stderr);
        return false;
      }
      </code>
  */
extern OpalHandle OPAL_EXPORT OpalInitialise(unsigned * version, const char * options);

/** String representation of the OpalIntialise() which may be used for late
    binding to the library.
 */
#if _WIN32
  #define OPAL_INITIALISE_FUNCTION MAKEINTRESOURCE(1)
#else
  #define OPAL_INITIALISE_FUNCTION "OpalInitialise"
#endif


/** Typedef representation of the pointer to the OpalIntialise() function which
    may be used for late binding to the library.
 */
typedef OpalHandle (OPAL_EXPORT *OpalInitialiseFunction)(unsigned * version, const char * options);


///////////////////////////////////////

/** Shut down and clean up all resource used by the OPAL system. The parameter
    must be the handle returned by OpalInitialise().

    Example:
      <code>
      OpalShutDown(hOPAL);
      </code>
  */
extern void OPAL_EXPORT OpalShutDown(OpalHandle opal);

/** String representation of the OpalShutDown() which may be used for late
    binding to the library.
 */
#if _WIN32
  #define OPAL_SHUTDOWN_FUNCTION MAKEINTRESOURCE(2)
#else
  #define OPAL_SHUTDOWN_FUNCTION "OpalShutDown"
#endif

/** Typedef representation of the pointer to the OpalShutDown() function which
    may be used for late binding to the library.
 */
typedef void (OPAL_EXPORT *OpalShutDownFunction)(OpalHandle opal);


///////////////////////////////////////

/** Get a message from the OPAL system. The first parameter must be the handle
    returned by OpalInitialise(). The second parameter is a timeout in
    milliseconds. NULL is returned if a timeout occurs. A value of UINT_MAX
    will wait forever for a message.

    The returned message must be disposed of by a call to OpalFreeMessage().

    The OPAL system will serialise all messages returned from this function to
    avoid any multi-threading issues. If the application wishes to avoid even
    this small delay, there is a callback function that may be configured that
    is not thread safe but may be used to get the messages as soon as they are
    generated. See OpalCmdSetGeneralParameters.

    Note if OpalShutDown() is called from a different thread then this function
    will break from its block and return NULL.

    Example:
      <code>
      OpalMessage * message;
        
      while ((message = OpalGetMessage(hOPAL, timeout)) != NULL) {
        switch (message->m_type) {
          case OpalIndRegistration :
            HandleRegistration(message);
            break;
          case OpalIndIncomingCall :
            Ring(message);
            break;
          case OpalIndCallCleared :
            HandleHangUp(message);
            break;
        }
        OpalFreeMessage(message);
      }
      </code>
  */
extern OpalMessage * OPAL_EXPORT OpalGetMessage(OpalHandle opal, unsigned timeout);

/** String representation of the OpalGetMessage() which may be used for late
    binding to the library.
 */
#if _WIN32
  #define OPAL_GET_MESSAGE_FUNCTION MAKEINTRESOURCE(3)
#else
  #define OPAL_GET_MESSAGE_FUNCTION "OpalGetMessage"
#endif

/** Typedef representation of the pointer to the OpalGetMessage() function which
    may be used for late binding to the library.
 */
typedef OpalMessage * (OPAL_EXPORT *OpalGetMessageFunction)(OpalHandle opal, unsigned timeout);


///////////////////////////////////////

/** Send a message to the OPAL system. The first parameter must be the handle
    returned by OpalInitialise(). The second parameter is a constructed message
    which is a command to the OPAL system.

    Within the command message, generally a NULL or empty string, or zero value
    for integral types, indicates the particular parameter is to be ignored.
    Documentation on individiual messages will indicate which are mandatory.
    
    The return value is another message which will have a type of
    OpalIndCommandError if an error occurs. The OpalMessage::m_commandError field
    will contain a string indicating the error that occurred.

    If successful, the the type of the message is the same as the command type.
    The message fields in the return will generally be set to the previous value
    for the field, where relevant. For example in the OpalCmdSetGeneralParameters
    command the OpalParamGeneral::m_natServer would contain the STUN server name
    prior to the command.

    A NULL is only returned if the either OpalHandle or OpalMessage parameters is NULL.

    The returned message must be disposed of by a call to OpalFreeMessage().

    Example:
      <code>
      void SendCommand(OpalMessage * command)
      {
        OpalMessage * response;
        if ((response = OpalSendMessage(hOPAL, command)) == NULL) {
          puts("OPAL not initialised.");
        else if (response->m_type != OpalIndCommandError)
          HandleResponse(response);
        else if (response->m_param.m_commandError == NULL || *response->m_param.m_commandError == '\\0')
          puts("OPAL error.");
        else
          printf("OPAL error: %s\n", response->m_param.m_commandError);

        OpalFreeMessage(response);
      }
      </code>
  */
extern OpalMessage * OPAL_EXPORT OpalSendMessage(OpalHandle opal, const OpalMessage * message);

/** String representation of the OpalSendMessage() which may be used for late
    binding to the library.
 */
typedef OpalMessage * (OPAL_EXPORT *OpalSendMessageFunction)(OpalHandle opal, const OpalMessage * message);

/** Typedef representation of the pointer to the OpalSendMessage() function which
    may be used for late binding to the library.
 */
#if _WIN32
  #define OPAL_SEND_MESSAGE_FUNCTION MAKEINTRESOURCE(4)
#else
  #define OPAL_SEND_MESSAGE_FUNCTION "OpalSendMessage"
#endif


///////////////////////////////////////

/** Free memeory in message the OPAL system has sent. The parameter must be
    the message returned by OpalGetMessage() or OpalSendMessage().
  */
extern void OPAL_EXPORT OpalFreeMessage(OpalMessage * message);

/** String representation of the OpalFreeMessage() which may be used for late
    binding to the library.
 */
#if _WIN32
  #define OPAL_FREE_MESSAGE_FUNCTION MAKEINTRESOURCE(5)
#else
  #define OPAL_FREE_MESSAGE_FUNCTION "OpalFreeMessage"
#endif

/** Typedef representation of the pointer to the OpalFreeMessage() function which
    may be used for late binding to the library.
 */
typedef void (OPAL_EXPORT *OpalFreeMessageFunction)(OpalMessage * message);


///////////////////////////////////////

#define OPAL_PREFIX_H323   "h323"   ///< H.323 Protocol supported string for OpalInitialise()
#define OPAL_PREFIX_H323S  "h323s"  ///< Secure H.323 Protocol supported string for OpalInitialise()
#define OPAL_PREFIX_SIP    "sip"    ///< SIP Protocol supported string for OpalInitialise()
#define OPAL_PREFIX_SIPS   "sips"   ///< Secure SIP Protocol supported string for OpalInitialise()
#define OPAL_PREFIX_SDP    "sdp"    ///< SDP over HTTP (e.g. for WebRTC) supported string for OpalInitialise()
#define OPAL_PREFIX_IAX2   "iax2"   ///< IAX2 Protocol supported string for OpalInitialise()
#define OPAL_PREFIX_PCSS   "pc"     ///< PC sound system supported string for OpalInitialise()
#define OPAL_PREFIX_LOCAL  "local"  ///< Local endpoint supported string for OpalInitialise()
#define OPAL_PREFIX_POTS   "pots"   ///< Plain Old Telephone System supported string for OpalInitialise()
#define OPAL_PREFIX_PSTN   "pstn"   ///< Public Switched Network supported string for OpalInitialise()
#define OPAL_PREFIX_CAPI   "isdn"   ///< ISDN (via CAPI) string for OpalInitialise()
#define OPAL_PREFIX_FAX    "fax"    ///< G.711 fax supported string for OpalInitialise()
#define OPAL_PREFIX_T38    "t38"    ///< G.711 fax supported string for OpalInitialise()
#define OPAL_PREFIX_IVR    "ivr"    ///< Interactive Voice Response supported string for OpalInitialise()
#define OPAL_PREFIX_MIXER  "mcu"    ///< Mixer for conferencing
#define OPAL_PREFIX_IM     "im"     ///< Interactive Voice Response supported string for OpalInitialise()
#define OPAL_PREFIX_GST    "gst"    ///< GStreamer supported string for OpalInitialise()
#define OPAL_PREFIX_SKINNY "sccp"   ///< Cisco Skinny Client Control Protocol supported string for OpalInitialise()
#define OPAL_PREFIX_LYNC   "lync"   ///< Microsoft Lync (UCMA) supported string for OpalInitialise()

#define OPAL_PREFIX_ALL OPAL_PREFIX_SIP    " " \
                        OPAL_PREFIX_SIPS   " " \
                        OPAL_PREFIX_H323   " " \
                        OPAL_PREFIX_H323S  " " \
                        OPAL_PREFIX_IAX2   " " \
                        OPAL_PREFIX_SDP    " " \
                        OPAL_PREFIX_SKINNY " " \
                        OPAL_PREFIX_LYNC   " " \
                        OPAL_PREFIX_PCSS   " " \
                        OPAL_PREFIX_LOCAL  " " \
                        OPAL_PREFIX_GST    " " \
                        OPAL_PREFIX_POTS   " " \
                        OPAL_PREFIX_PSTN   " " \
                        OPAL_PREFIX_FAX    " " \
                        OPAL_PREFIX_T38    " " \
                        OPAL_PREFIX_IVR    " " \
                        OPAL_PREFIX_MIXER  " " \
                        OPAL_PREFIX_IM


/**Type code for messages defined by OpalMessage.
  */
typedef enum OpalMessageType {
  OpalIndCommandError,          /**<An error occurred during a command. This is only returned by
                                    OpalSendMessage(). The details of the error are shown in the
                                    OpalMessage::m_commandError field. */
  OpalCmdSetGeneralParameters,  /**<Set general parameters command. This configures global settings in OPAL.
                                    See the OpalParamGeneral structure for more information. */
  OpalCmdSetProtocolParameters, /**<Set protocol parameters command. This configures settings in OPAL that
                                    may be different for each protocol, e.g. SIP & H.323. See the 
                                    OpalParamProtocol structure for more information. */
  OpalCmdRegistration,          /**<Register/Unregister command. This initiates a registration or
                                    unregistration operation with a protocol dependent server. Currently
                                    only for H.323 and SIP. See the OpalParamRegistration structure for more
                                    information. */
  OpalIndRegistration,          /**<Status of registration indication. After the OpalCmdRegistration has
                                    initiated a registration, this indication will be returned by the
                                    OpalGetMessage() function when the status of the registration changes,
                                    e.g. successful registration or communications failure etc. See the
                                    OpalStatusRegistration structure for more information. */
  OpalCmdSetUpCall,             /**<Set up a call command. This starts the outgoing call process. The
                                    OpalIndAlerting, OpalIndEstablished and OpalIndCallCleared messages are
                                    returned by OpalGetMessage() to indicate the call progress. See the 
                                    OpalParamSetUpCall structure for more information. */
  OpalIndIncomingCall,          /**<Incoming call indication. This is returned by the OpalGetMessage() function
                                    at any time after listeners are set up via the OpalCmdSetProtocolParameters
                                    command. See the OpalStatusIncomingCall structure for more information. */
  OpalCmdAnswerCall,            /**<Answer call command. After a OpalIndIncomingCall is returned by the
                                    OpalGetMessage() function, an application maye indicate that the call is
                                    to be answered with this message. The OpalMessage m_callToken field is
                                    set to the token returned in OpalIndIncomingCall. */
  OpalCmdClearCall,             /**<Hang Up call command. After a OpalCmdSetUpCall command is executed or a
                                    OpalIndIncomingCall indication is received then this may be used to
                                    "hang up" the call. The OpalIndCallCleared is subsequently returned in
                                    the OpalGetMessage() when the call has completed its hang up operation.
                                    See OpalParamCallCleared structure for more information.*/
  OpalIndAlerting,              /**<Remote is alerting indication. This message is returned in the
                                    OpalGetMessage() function when the underlying protocol states the remote
                                    telephone is "ringing". See the OpalParamSetUpCall structure for more
                                    information. */
  OpalIndEstablished,           /**<Call is established indication. This message is returned in the
                                    OpalGetMessage() function when the remote or local endpont has "answered"
                                    the call and there is media flowing. See the  OpalParamSetUpCall
                                    structure for more information. */
  OpalIndUserInput,             /**<User input indication. This message is returned in the OpalGetMessage()
                                    function when, during a call, user indications (aka DTMF tones) are
                                    received. See the OpalStatusUserInput structure for more information. */
  OpalIndCallCleared,           /**<Call is cleared indication. This message is returned in the
                                    OpalGetMessage() function when the call has completed. The OpalMessage
                                    m_callToken field indicates which call cleared. */
  OpalCmdHoldCall,              /**<Place call in a hold state. The OpalMessage m_callToken field is set to
                                    the token returned in OpalIndIncomingCall. */
  OpalCmdRetrieveCall,          /**<Retrieve call from hold state. The OpalMessage m_callToken field is set
                                    to the token for the call. */
  OpalCmdTransferCall,          /**<Transfer a call to another party. This starts the outgoing call process
                                    for the other party. See the  OpalParamSetUpCall structure for more
                                    information.*/
  OpalCmdUserInput,             /**<User input command. This sends specified user input to the remote
                                    connection. See the OpalStatusUserInput structure for more information. */
  OpalIndMessageWaiting,        /**<Message Waiting indication. This message is returned in the
                                    OpalGetMessage() function when an MWI is received on any of the supported
                                    protocols. */
  OpalIndMediaStream,           /**<A media stream has started/stopped. This message is returned in the
                                    OpalGetMessage() function when a media stream is started or stopped. See the
                                    OpalStatusMediaStream structure for more information. */
  OpalCmdMediaStream,           /**<Execute control on a media stream. See the OpalStatusMediaStream structure
                                    for more information. */
  OpalCmdSetUserData,           /**<Set the user data field associated with a call */
  OpalIndLineAppearance,        /**<Line Appearance indication. This message is returned in the
                                    OpalGetMessage() function when any of the supported protocols indicate that
                                    the state of a "line" has changed, e.g. free, busy, on hold etc. */
  OpalCmdStartRecording,        /**<Start recording an active call. See the OpalParamRecording structure
                                    for more information. */
  OpalCmdStopRecording,         /**<Stop recording an active call. Only the m_callToken field of the
                                    OpalMessage union is used. */
  OpalIndProceeding,            /**<Call has been accepted by remote. This message is returned in the
                                    OpalGetMessage() function when the underlying protocol states the remote
                                    endpoint acknowledged that it will route the call. This is distinct from
                                    OpalIndAlerting in that it is not known at this time if anything is
                                    ringing. This indication may be used to distinguish between "transport"
                                    level error, in which case another host may be tried, and that the
                                    responsibility for finalising the call has moved "upstream". See the
                                    OpalParamSetUpCall structure for more information. */
  OpalCmdAlerting,              /**<Send an indication to the remote that we are "ringing". The OpalMessage
                                    m_callToken field indicates which call is alerting.  */
  OpalIndOnHold,                /**<Indicate a call has been placed on hold by remote. This message is returned
                                    in the OpalGetMessage() function. */
  OpalIndOffHold,               /**<Indicate a call has been retrieved from hold by remote. This message is
                                    returned in the OpalGetMessage() function. */
  OpalIndTransferCall,          /**<Status of transfer operation that is under way. This message is returned in
                                    the OpalGetMessage() function. See the OpalStatusTransferCall structure for
                                    more information. */
  OpalIndCompletedIVR,          /**<Indicates completion of the IVR (VXML) script. This message is returned in
                                    the OpalGetMessage() function. See the OpalStatusIVR structure for
                                    more information. */
  OpalCmdAuthorisePresence,     /**<Permit or deny authority for the remote presentity to view the presence
                                    state of a local presentity. See the OpalPresenceStatus structure for
                                    more information. */
  OpalCmdSubscribePresence,     /**<Subscribe to the change in presence state for a presentity. See the
                                    OpalPresenceStatus structure for more information. */
  OpalCmdSetLocalPresence,      /**<Set, and publish, the local presence state. See the OpalPresenceStatus
                                    structure for more information. */
  OpalIndPresenceChange,        /**<Indicates a change the the presence state for a given presentity. This
                                    message is returned in the OpalGetMessage() function. See the
                                    OpalPresenceStatus structure for more information. */
  OpalCmdSendIM,                /**<Send an Instant Message. See the OpalInstantMessage structure for more
                                    information. */
  OpalIndReceiveIM,             /**<Indicates receipt of an instant message. This message is returned in the
                                    OpalGetMessage() function. See the OpalInstantMessage structure for
                                    more information. */
  OpalIndSentIM,                /**<Get indication of the disposition of a sent instant message. This message
                                    is returned in the OpalGetMessage() function. See the OpalInstantMessage
                                    structure for more information. */
  OpalIndProtocolMessage,       /**<Get indication of protocol specific messages. See the OpalProtocolMessage
                                    structure for more information. */

// Always add new messages to ethe end to maintain backward compatibility
  OpalMessageTypeCount
} OpalMessageType;


/**Type code the silence detect algorithm modes.
   This is used by the OpalCmdSetGeneralParameters command in the OpalParamGeneral structure.
  */
typedef enum OpalSilenceDetectMode {
  OpalSilenceDetectNoChange,  /**< No change to the silence detect mode. */
  OpalSilenceDetectDisabled,  /**< Indicate silence detect is disabled */
  OpalSilenceDetectFixed,     /**< Indicate silence detect uses a fixed threshold */
  OpalSilenceDetectAdaptive   /**< Indicate silence detect uses an adaptive threashold */
} OpalSilenceDetectMode;


/**Type code the echo cancellation algorithm modes.
   This is used by the OpalCmdSetGeneralParameters command in the OpalParamGeneral structure.
  */
typedef enum OpalEchoCancelMode {
  OpalEchoCancelNoChange,   /**< No change to the echo cancellation mode. */
  OpalEchoCancelDisabled,   /**< Indicate the echo cancellation is disabled */
  OpalEchoCancelEnabled     /**< Indicate the echo cancellation is enabled */
} OpalEchoCancelMode;


/** Function for reading/writing media data.
    The m_mediaReadData and m_mediaWriteData memebers of OpalParamGeneral are
    the mechanism by which an application can be sent the raw media for a
    call.
    
    This requires the inclusion of the OPAL_PREFIX_LOCAL ("local") or
    OPAL_PREFIX_PCSS ("pc") in the OpalInitialise() call. If the latter is
    used the m_pcssMediaOverride in OpalParamGeneral must also be set for the
    specific media you wish th callback to apply to. For the local endpoint
    all media is sent to the callback.

    Note that incoming calls are sent to the local endpoints in order they are
    specified in OpalInitialise, so make sure OPAL_PREFIX_LOCAL is the first,
    or only entry, in the list to OpalInitialise() for it to be selected as
    the default is for OPAL_PREFIX_PCSS to be used.

    The "write" function, which is taking data from a remote and providing it
    to the "C" application for writing, should not be assumed to have a one to
    one correspondence with RTP packets. The OPAL jiter buffer may insert
    "silence" data for missing or too late packets. In this case the function
    is called with the size parameter equal to zero. It is up to the
    application what it does in that circumstance.

    if \p format is "YUV420P" then \p data will point to four 32 bit integers
    being the x, y, width and height of the image, following by the YUV planar
    pixel data.

    Note that this function will be called in the context of different threads
    so the user must take care of any mutex and synchonisation issues.

    Returns size of data actually read or written, or -1 if there is an error
    and the media stream should be shut down.
 */
typedef int (*OpalMediaDataFunction)(
  const char * token,   /**< Call token for media data as returned by OpalIndIncomingCall.
                             This may be used to discriminate between individiual calls. */
  const char * stream,  /**< Stream identifier for media data. This may be used to
                             discriminate between media streams within a call, applicable
                             if there can be more than one stream of a particular format,
                             e.g. two H.263 video channels. */
  const char * format,  /**< Format of media data, e.g. "PCM-16" */
  void * userData,      /**< user data associated with the call */
  void * data,          /**< Data to read/write */
  int size              /**< Maximum size of data to read, or size of actual data to write */
);


/** Function called when a message event becomes available.
    This function is called before the message is queued for the GetMessage()
    function.

    A return value of zero indicates that the message is not to be passed on
    to the GetMessage(). A non-zero value will pass the message on.

    Note that this function will be called in the context of different threads
    so the user must take care of any mutex and synchonisation issues. If the
    user subsequently uses the GetMessage() then the message will have been
    serialised so that there are no multi-threading issues.

    A simple use case would be for this function to send a signal or message
    to the applications main thread and then return a non-zero value. The
    main thread would then wake up and get the message using GetMessage.
 */
typedef int (*OpalMessageAvailableFunction)(
  const OpalMessage * message  /**< Message that has become available. */
);


/**Type code the media data call back functions data type.
   This is used by the OpalCmdSetGeneralParameters command in the
   OpalParamGeneral structure.

   This controls if the whole RTP data frame or just the paylaod part
   is passed to the read/write function.

   Default is OpalMediaDataPayloadOnly.
  */
typedef enum OpalMediaDataType {
  OpalMediaDataNoChange,      /**< No change to the media data type. */
  OpalMediaDataPayloadOnly,   /**< Indicate only the RTP payload is passed to the
                                   read/write function */
  OpalMediaDataWithHeader     /**< Indicate the whole RTP frame including header is
                                   passed to the read/write function */
} OpalMediaDataType;


/**Timing mode for the media data call back functions data type.
   This is used by the OpalCmdSetGeneralParameters command in the
   OpalParamGeneral structure.

   This controls if the read/write function is in control of the real time
   aspects of the media flow. If synchronous then the read/write function
   is expected to handle the real time "pacing" of the read or written data.

   Note this is important both for reads and writes. For example in
   synchronous mode you cannot simply read from a file and send, or you will
   likely overrun the remotes buffers. Similarly for writing to a file, the
   correct operation of the OPAL jitter buffer is dependent on it not being
   drained too fast by the "write" function.

   If marked as asynchroous then the OPAL stack itself will take care of the
   timing and things like read/write to a disk file will work correctly.
  */
typedef enum OpalMediaTiming {
  OpalMediaTimingNoChange,      /**< No change to the media data type. */
  OpalMediaTimingSynchronous,   /**< Indicate the read/write function is going to handle
                                     all real time aspects of the media flow. */
  OpalMediaTimingAsynchronous,  /**< Indicate the read/write function does not require
                                     real time aspects of the media flow. */
  OpalMediaTimingSimulated      /**< Indicate the read/write function does not handle
                                     the real time aspects of the media flow and they
                                     must be simulated by the OPAL library. */
} OpalMediaTiming;


/**General parameters for the OpalCmdSetGeneralParameters command.
   This is only passed to and returned from the OpalSendMessage() function.

   Example:
      <code>
      OpalMessage   command;
      OpalMessage * response;

      memset(&command, 0, sizeof(command));
      command.m_type = OpalCmdSetGeneralParameters;
      command.m_param.m_general.m_natServer = "stun.l.google.com:19302";
      command.m_param.m_general.m_mediaMask = "RFC4175*";
      response = OpalSendMessage(hOPAL, &command);
      </code>

   For m_mediaOrder and m_mediaMask, each '\n' seperated sub-string in the
   array is checked using a simple wildcard matching algorithm.

   The '*' character indicates substrings, for example: "G.711*" would remove
   "G.711-uLaw-64k" and "G.711-ALaw-64k".

   The '@' character indicates a type of media format, so say "\@video" would
   remove all video codecs.

   The '!' character indicates a negative test. That is the entres that do NOT
   match the string are removed. The string after the '!' may contain '*' and
   '@' characters.

   It should be noted that when the ! operator is used, they are combined
   differently to the usual application of each entry in turn. Thus, the string
   "!A\n!B" will result in keeping <i>both</i> A and B formats.
  */
typedef struct OpalParamGeneral {
  const char * m_audioRecordDevice;   /**< Audio recording device name. Note, if OPAL_PREFIX_PCSS is
                                           used, then this is the operating system device name. If
                                           OPAL_PREFIX_GST is used then this is the gstreamer element
                                           for the audio source. */
  const char * m_audioPlayerDevice;   /**< Audio playback device name. Note, if OPAL_PREFIX_PCSS is
                                           used, then this is the operating system device name. If
                                           OPAL_PREFIX_GST is used then this is the gstreamer element
                                           for the audio sink. */
  const char * m_videoInputDevice;    /**< Video input (e.g. camera) device name. Note, if
                                           OPAL_PREFIX_PCSS is used, then this is the operating system
                                           device name for the camera, or other pseudo-device. If
                                           OPAL_PREFIX_GST is used then this is the gstreamer element
                                           for the video source. */
  const char * m_videoOutputDevice;   /**< Video output (e.g. window) device name. Note, if
                                           OPAL_PREFIX_PCSS is used, then this is the operating system
                                           dependent name for a window, or other pseudo-device. If
                                           OPAL_PREFIX_GST is used then this is the gstreamer element
                                           for the video sink. */
  const char * m_videoPreviewDevice;  /**< Video preview (e.g. window) device name. Note, if
                                           OPAL_PREFIX_PCSS is used, then this is the operating system
                                           dependent name for a window. If OPAL_PREFIX_GST is used then
                                           this is ignored. */
  const char * m_mediaOrder;          /**< List of media format names to set the preference order for
                                           media. This list of names (e.g. "G.723.1") is separated by
                                           the '\n' character. */
  const char * m_mediaMask;           /**< List of media format names to set media to be excluded.
                                           This list of names (e.g. "G.723.1") is separated by the '\n'
                                           character. */
  const char * m_autoRxMedia;         /**< List of media types (e.g. audio, video) separated by spaces
                                           which may automatically be received automatically. If NULL
                                           no change is made, but if "" then all media is prevented from
                                           auto-starting. */
  const char * m_autoTxMedia;         /**< List of media types (e.g. audio, video) separated by spaces
                                           which may automatically be transmitted automatically. If NULL
                                           no change is made, but if "" then all media is prevented from
                                           auto-starting.  */
  const char * m_natMethod;           /**< A list of Network Address Translation methods to use, in
                                           priority order, separaeted by '\n'. For backward compatibility,
                                           if this is an empty string and m_natServer is not empty, then
                                           only "STUN" is assumed, and if this is a host name or IP
                                           address, then a "Fixed" NAT router is used. */
  const char * m_natServer;           /**< The host name or IP address of the NAT (e.g. STUN) server which
                                           may be used to determine the NAT router characteristics. The local
                                           interface used may be optionally set after a '\t' characters. If
                                           m_natMethod has multiple entries, then this must have corresponding
                                           '\n' separated entries. */
  unsigned     m_tcpPortBase;         /**< Base of range of ports to use for TCP communications. This may
                                           be required by some firewalls. */
  unsigned     m_tcpPortMax;          /**< Max of range of ports to use for TCP communications. This may
                                           be required by some firewalls. */
  unsigned     m_udpPortBase;         /**< Base of range of ports to use for UDP communications. This may
                                           be required by some firewalls. */
  unsigned     m_udpPortMax;          /**< Max of range of ports to use for UDP communications. This may
                                           be required by some firewalls. */
  unsigned     m_rtpPortBase;         /**< Base of range of ports to use for RTP/UDP communications. This may
                                           be required by some firewalls. */
  unsigned     m_rtpPortMax;          /**< Max of range of ports to use for RTP/UDP communications. This may
                                           be required by some firewalls. */
  unsigned     m_rtpTypeOfService;    /**< Value for the Type Of Service byte with UDP/IP packets which may
                                           be used by some routers for simple Quality of Service control. */
  unsigned     m_rtpMaxPayloadSize;   /**< Maximum payload size for RTP packets. This may sometimes need to
                                           be set according to the MTU or the underlying network. */
  int          m_minAudioJitter;      /**< Minimum jitter time in milliseconds. For audio RTP data being
                                           received this sets the minimum time of the adaptive jitter buffer
                                           which smooths out irregularities in the transmission of audio
                                           data over the Internet. A negative value will disable the JB. */
  unsigned     m_maxAudioJitter;      /**< Maximum jitter time in milliseconds. For audio RTP data being
                                           received this sets the maximum time of the adaptive jitter buffer
                                           which smooths out irregularities in the transmission of audio
                                           data over the Internet. If this is less than m_minAudioJitter
                                           then m_minAudioJitter is used. */
  OpalSilenceDetectMode m_silenceDetectMode; /**< Silence detection mode. This controls the silence
                                           detection algorithm for audio transmission: 0=no change,
                                           1=disabled, 2=fixed, 3=adaptive. */
  unsigned     m_silenceThreshold;    /**< Silence detection threshold value. This applies if
                                           m_silenceDetectMode is fixed (2) and is a PCM-16 value. */
  unsigned     m_signalDeadband;      /**< Time signal is required before audio is transmitted. This is
                                           is RTP timestamp units (8000Hz). */
  unsigned     m_silenceDeadband;     /**< Time silence is required before audio is transmission is stopped.
                                           This is is RTP timestamp units (8000Hz). */
  unsigned     m_silenceAdaptPeriod;  /**< Window for adapting the silence threashold. This applies if
                                           m_silenceDetectMode is adaptive (3). This is is RTP timestamp
                                           units (8000Hz). */
  OpalEchoCancelMode m_echoCancellation; /**< Accoustic Echo Cancellation control. 0=no change, 1=disabled,
                                              2=enabled. */
  unsigned     m_audioBuffers;        /**< Set the number of hardware sound buffers to use.
                                           Note the largest of m_audioBuffers and m_audioBufferTime/frametime
                                           will be used. */
  OpalMediaDataFunction m_mediaReadData;   /**< Callback function for reading raw media data. See
                                                OpalMediaDataFunction for more information. */
  OpalMediaDataFunction m_mediaWriteData;  /**< Callback function for writing raw media data. See
                                                OpalMediaDataFunction for more information.  */
  OpalMediaDataType     m_mediaDataHeader; /**< Indicate that the media read/write callback function
                                           is passed the full RTP header or just the payload.
                                           0=no change, 1=payload only, 2=with RTP header. */
  OpalMessageAvailableFunction m_messageAvailable; /**< If non-null then this function is called before
                                                        the message is queued for return in the
                                                        GetMessage(). See the
                                                        OpalMessageAvailableFunction for more details. */
  const char * m_mediaOptions;        /**< List of media format options to be set. This is a '\n' separated
                                           list of entries of the form "codec:option=value". Codec is either
                                           a media type (e.g. "Audio" or "Video") or a specific media format,
                                           for example:
                                             <code>
                                             "G.723.1:Tx Frames Per Packet=2\nH.263:Annex T=0\n"
                                             "Video:Max Rx Frame Width=176\nVideo:Max Rx Frame Height=144"
                                             </code>
                                           */
  unsigned     m_audioBufferTime;     /**< Set the hardware sound buffers to use in milliseconds.
                                           Note the largest of m_audioBuffers and m_audioBufferTime/frametime
                                           will be used. */
  unsigned m_manualAlerting;          /**< Indicate that an "alerting" message is automatically (value=1)
                                           or manually (value=2) sent to the remote on receipt of an
                                           OpalIndIncomingCall message. If set to manual then it is up
                                           to the application to send a OpalCmdAlerting message to
                                           indicate to the remote system that we are "ringing".
                                           If zero then no change is made. */
  OpalMediaTiming m_mediaTiming;      /**< Indicate how the media read/write callback function
                                           handles the real time aspects of the media flow.
                                           0=no change, 1=synchronous, 2=asynchronous,
                                           3=simulate synchronous. */
  OpalMediaTiming m_videoSourceTiming;/**< Indicate that the video read callback function
                                           handles the real time aspects of the media flow.
                                           This can override the m_mediaTiming. */
  const char * m_pcssMediaOverride;   /**< When the OPAL_PREFIX_PCSS is in use, this provides a mask of
                                           which media streams (e.g audio/video rx/tx) is overridden from
                                           the internal devices. For example, redirecting only received
                                           video to the application, and audio and camera grabbing is
                                           handled as normal. The string is a space separated list of
                                           values being the direction, dash and the media type, e.g.
                                           "rx-video rx-audio tx-audio". When present, the same behaviour
                                           as for OPAL_PREFIX_LOCAL is executed for that media stream and
                                           m_mediaReadData/m_mediaWriteData is called. See
                                           OpalMediaDataFunction for more information. */
  unsigned m_noMediaTimeout;          /**< Time in milliseconds for which, if no media is received, the
                                           call is cleared. */
  const char * m_caFiles;             /**< File or directory containing Certificate Authority root certificates
                                           to validate remotes in TLS connections, e.g. sips or h323s. Note,
                                           an empty string "" is a valid value, and only NULL can be used
                                           for "no change". */
  const char * m_certificate;         /**< Certificate to use to identify this endpoint in TLS connections,
                                           e.g. sips or h323s. This can either be a filename or a PEM format
                                           certificate as a string. Note, an empty string "" is a valid value,
                                           and only NULL can be used for "no change". */
  const char * m_privateKey;          /**< Private key to use with the above certificate file. This can either
                                           be a filename or a PEM format certificate as a string. Note, an empty
                                           string "" is a valid value, and only NULL can be used for "no change". */
  unsigned m_autoCreateCertificate;   /**< Indicate a self signed certificate should be generated automatically
                                           if the certicalte and private key files are not found at the locations
                                           indicated (value=1), or that only the file/value indicated in above
                                           fields is used exclusively (value=2). */
} OpalParamGeneral;


/**Product description variables.
  */
typedef struct OpalProductDescription {
  const char * m_vendor;              /**< Name of the vendor or manufacturer of the application. This is
                                           used to identify the software which can be very useful for
                                           solving interoperability issues. e.g. "Vox Lucida". */
  const char * m_name;                /**< Name of the product within the vendor name space. This is
                                           used to identify the software which can be very useful for
                                           solving interoperability issues. e.g. "OpenPhone". */
  const char * m_version;             /**< Version of the product within the vendor/product name space. This
                                           is used to identify the software which can be very useful for
                                           solving interoperability issues. e.g. "2.1.4". */
  unsigned     m_t35CountryCode;      /**< T.35 country code for the name space in which the vendor or
                                           manufacturer is identified. This is the part of the H.221
                                           equivalent of the m_vendor string above and  used to identify the
                                           software which can be very useful for solving interoperability
                                           issues. e.g. 9 is for Australia. */
  unsigned     m_t35Extension;        /**< T.35 country extension code for the name space in which the vendor or
                                           manufacturer is identified. This is the part of the H.221
                                           equivalent of the m_vendor string above and  used to identify the
                                           software which can be very useful for solving interoperability
                                           issues. Very rarely used. */
  unsigned     m_manufacturerCode;    /**< Manuacturer code for the name space in which the vendor or
                                           manufacturer is identified. This is the part of the H.221
                                           equivalent of the m_vendor string above and  used to identify the
                                           software which can be very useful for solving interoperability
                                           issues. e.g. 61 is for Equivalance and was allocated by the
                                           Australian Communications Authority, Oct 2000. */
} OpalProductDescription;


/**Type code for controlling the mode in which user input (DTMF) is sent.
   This is used by the OpalCmdSetProtocolParameters command in the OpalParamProtocol structure.
  */
typedef enum OpalUserInputModes {
  OpalUserInputDefault,   ///< Default mode for protocol
  OpalUserInputAsQ931,    ///< Use Q.931 Information Elements (H.323 only)
  OpalUserInputAsString,  ///< Use arbitrary strings (H.245 string, or INFO dtmf)
  OpalUserInputAsTone,    ///< Use DTMF specific names (H.245 signal, or INFO dtmf-relay)
  OpalUserInputAsRFC2833, ///< Use RFC 2833 for DTMF only
  OpalUserInputInBand,    ///< Use in-band generated audio tones for DTMF
} OpalUserInputModes;


/**Protocol parameters for the OpalCmdSetProtocolParameters command.
   This is only passed to and returned from the OpalSendMessage() function.

   Example:
      <code>
      OpalMessage   command;
      OpalMessage * response;

      memset(&command, 0, sizeof(command));
      command.m_type = OpalCmdSetProtocolParameters;
      command.m_param.m_protocol.m_userName = "robertj";
      command.m_param.m_protocol.m_displayName = "Robert Jongbloed";
      command.m_param.m_protocol.m_interfaceAddresses = "*";
      response = OpalSendMessage(hOPAL, &command);
      </code>
  */
typedef struct OpalParamProtocol {
  const char * m_prefix;              /**< Protocol prefix for parameters, e.g. "h323" or "sip". If this is
                                           NULL or empty string, then the parameters are set for all protocols
                                           where they maybe set. */
  const char * m_userName;            /**< User name to identify the endpoint. This is usually the protocol
                                           specific name and may interact with the OpalCmdRegistration
                                           command. e.g. "robertj" or 61295552148 */
  const char * m_displayName;         /**< Display name to be used. This is the human readable form of the
                                           users name, e.g. "Robert Jongbloed". */
  OpalProductDescription m_product;   /**< Product description data */
  const char * m_interfaceAddresses;  /**< A list of interfaces to start listening for incoming calls.
                                           This list is separated by the '\n' character. If NULL no
                                           listeners are started or stopped. If and empty string ("")
                                           then all listeners are stopped. If a "*" then listeners
                                           are started for all interfaces in the system.

                                           If the prefix is OPAL_PREFIX_IVR, then this is the default
                                           VXML script or URL to execute on incoming calls.

                                           If the prefix is OPAL_PREFIX_GST, then this is a '\n' separated
                                           list of mappings for media formats to GStreamer elements. Each
                                           mapping consists of five fields separated by the '\t' chacracter.
                                           The fields are media format, encoder, decoder, RTP packetiser
                                           and RTP depacketiser. e.g.
                                              "G.722.2\tamrwbenc\tamrwbdec\trtpamrpay\trtpamrdepay"
                                           The last two may be omitted and a default is used. Note, omission
                                           is not the same as an empty string. In addition, there are two
                                           special lines:
                                              "SourceColourConverter\tautoconvert\n"
                                              "SinkColourConverter\tautoconvert\n"
                                           may also be present.
                                         */
  OpalUserInputModes m_userInputMode; /**< The mode for user input transmission. Note this only applies if an
                                           explicit protocol is indicated in m_prefix. See OpalUserInputModes
                                           for more information. */
  const char * m_defaultOptions;      /**< Default options for new calls using the specified protocol. This
                                           string is of the form key=value\nkey=value */
  const char * m_mediaCryptoSuites;   /**< A list of \n separated strings indicated enabled media
                                           crypto suites for this endpoint. Note, order of entries
                                           indicates priority. The special value of "!Clear" may also
                                           be used indicating all available crypto suites are offered
                                           but there must be encryption. */
  const char * m_allMediaCryptoSuites;/**< This is only provided as a return value, and lists all of the
                                           crypto suites supported by this protocol in the form:
                                              "name1=description1\nname2=description2\n" */
  unsigned m_maxSizeUDP;              /**< Maximum size of signalling UDP packet. */
  const char * m_protocolMessageIdentifiers; /**< List of \n separated regular expressions (extended variant, and
                                                  with ignore case enabled) for protocol message identifers, that
                                                  OPAL will return a OpalIndProtocolMessage for. */
} OpalParamProtocol;


/// Name of SIP event package for Message Waiting events.
#define OPAL_MWI_EVENT_PACKAGE             "message-summary"

/// Name of SIP even package fo rmonitoring call status
#define OPAL_LINE_APPEARANCE_EVENT_PACKAGE "dialog;sla;ma"

/**Registration parameters for the OpalCmdRegistration command.
   This is only passed to and returned from the OpalSendMessage() function.

   Example:
      <code>
      OpalMessage   command;
      OpalMessage * response;

      // H.323 register with gatekeeper
      memset(&command, 0, sizeof(command));
      command.m_type = OpalCmdRegistration;
      command.m_param.m_registrationInfo.m_protocol = "h323";
      command.m_param.m_registrationInfo.m_identifier = "31415";
      command.m_param.m_registrationInfo.m_hostName = gk.voxgratia.org;
      command.m_param.m_registrationInfo.m_password = "secret";
      command.m_param.m_registrationInfo.m_timeToLive = 300;
      response = OpalSendMessage(hOPAL, &command);
      if (response != NULL && response->m_type == OpalCmdRegistration)
        m_AddressOfRecord = response->m_param.m_registrationInfo.m_identifier

      // SIP register with regstrar/proxy
      memset(&command, 0, sizeof(command));
      command.m_type = OpalCmdRegistration;
      command.m_param.m_registrationInfo.m_protocol = "sip";
      command.m_param.m_registrationInfo.m_identifier = "rjongbloed@ekiga.net";
      command.m_param.m_registrationInfo.m_password = "secret";
      command.m_param.m_registrationInfo.m_timeToLive = 300;
      response = OpalSendMessage(hOPAL, &command);
      if (response != NULL && response->m_type == OpalCmdRegistration)
        m_AddressOfRecord = response->m_param.m_registrationInfo.m_identifier

      // Presence registration
      memset(&command, 0, sizeof(command));
      command.m_type = OpalCmdRegistration;
      command.m_param.m_registrationInfo.m_protocol = "pres";
      command.m_param.m_registrationInfo.m_identifier = "sip:rjongbloed@ekiga.net";
      command.m_param.m_registrationInfo.m_password = "secret";
      command.m_param.m_registrationInfo.m_timeToLive = 300;
      response = OpalSendMessage(hOPAL, &command);
      if (response != NULL && response->m_type == OpalCmdRegistration)
        m_AddressOfRecord = response->m_param.m_registrationInfo.m_identifier

      // unREGISTER
      memset(&command, 0, sizeof(command));
      command.m_type = OpalCmdRegistration;
      command.m_param.m_registrationInfo.m_protocol = "sip";
      command.m_param.m_registrationInfo.m_identifier = m_AddressOfRecord;
      command.m_param.m_registrationInfo.m_timeToLive = 0;
      response = OpalSendMessage(hOPAL, &command);

      // Set event package so do SUBSCRIBE
      memset(&command, 0, sizeof(command));
      command.m_type = OpalCmdRegistration;
      command.m_param.m_registrationInfo.m_protocol = "sip";
      command.m_param.m_registrationInfo.m_identifier = "2012@pbx.local";
      command.m_param.m_registrationInfo.m_hostName = "sa@pbx.local";
      command.m_param.m_registrationInfo.m_eventPackage = OPAL_LINE_APPEARANCE_EVENT_PACKAGE;
      command.m_param.m_registrationInfo.m_timeToLive = 300;
      response = OpalSendMessage(hOPAL, &command);
      if (response != NULL && response->m_type == OpalCmdRegistration)
        m_AddressOfRecord = response->m_param.m_registrationInfo.m_identifier

      // unSUBSCRIBE
      memset(&command, 0, sizeof(command));
      command.m_type = OpalCmdRegistration;
      command.m_param.m_registrationInfo.m_protocol = "sip";
      command.m_param.m_registrationInfo.m_identifier = m_AddressOfRecord;
      command.m_param.m_registrationInfo.m_eventPackage = OPAL_LINE_APPEARANCE_EVENT_PACKAGE;
      command.m_param.m_registrationInfo.m_timeToLive = 0;
      response = OpalSendMessage(hOPAL, &command);
      </code>
  */
typedef struct OpalParamRegistration {
  const char * m_protocol;      /**< Protocol prefix for registration. Currently must be "h323" or
                                     "sip", cannot be NULL. */
  const char * m_identifier;    /**< Identifier for name to be registered at server. If NULL
                                     or empty then the value provided in the OpalParamProtocol::m_userName
                                     field of the OpalCmdSetProtocolParameters command is used. Note
                                     that for SIP the default value will have "@" and the
                                     OpalParamRegistration::m_hostName field apepnded to it to create
                                     and Address-Of_Record. */
  const char * m_hostName;      /**< Host or domain name for server. For SIP this cannot be NULL.
                                     For H.323 a NULL value indicates that a broadcast discovery is
                                     be performed. If, for SIP, this contains an "@" and a user part
                                     then a "third party" registration is performed. */
  const char * m_authUserName;  /**< User name for authentication. */
  const char * m_password;      ///< Password for authentication with server.
  const char * m_adminEntity;   /**< Identification of the administrative entity. For H.323 this will
                                     by the gatekeeper identifier. For SIP this is the authentication
                                     realm. */
  unsigned     m_timeToLive;    /**< Time in seconds between registration updates. If this is zero then
                                     the identifier is unregistered from the server. */
  unsigned     m_restoreTime;   /**< Time in seconds between attempts to restore a registration after
                                     registrar/gatekeeper has gone offline. If zero then a default
                                     value is used. */
  const char * m_eventPackage;  /**< If non-NULL then this indicates that a subscription is made
                                     rather than a registration. The string represents the particular
                                     event package being subscribed too.
                                     A value of OPAL_MWI_EVENT_PACKAGE will cause an
                                     OpalIndMessageWaiting to be sent.
                                     A value of OPAL_LINE_APPEARANCE_EVENT_PACKAGE will cause the
                                     OpalIndLineAppearance to be sent.
                                     Other values are currently not supported. */
  const char * m_attributes;    /**< Protocol dependent information in the form:
                                           key=value\n
                                           key=value\n
                                           etc */
} OpalParamRegistration;


/**Type code for media stream status/control.
   This is used by the OpalIndRegistration indication in the OpalStatusRegistration structure.
  */
typedef enum OpalRegistrationStates {
  OpalRegisterSuccessful,   /**< Successfully registered. */
  OpalRegisterRemoved,      /**< Successfully unregistered. Note that the m_error field may be
                                 non-null if an error occurred during unregistration, however
                                 the unregistration will "complete" as far as the local endpoint
                                 is concerned and no more registration retries are made. */
  OpalRegisterFailed,       /**< Registration has failed. The m_error field of the
                                 OpalStatusRegistration structure will contain more details. */
  OpalRegisterRetrying,     /**< Registrar/Gatekeeper has gone offline and a failed retry
                                 has been executed. */
  OpalRegisterRestored,     /**< Registration has been restored after a succesfull retry. */
} OpalRegistrationStates;


/**Registration status for the OpalIndRegistration indication.
   This is only returned from the OpalGetMessage() function.
  */
typedef struct OpalStatusRegistration {
  const char * m_protocol;    /**< Protocol prefix for registration. Currently must be "h323" or
                                   "sip", is never NULL. */
  const char * m_serverName;  /**< Nmae of the registration server. The exact format is protocol
                                   specific but generally contains the host or domain name, e.g.
                                   "GkId@gatekeeper.voxgratia.org" or "sip.voxgratia.org". */
  const char * m_error;       /**< Error message for registration. If any error in the initial
                                   registration or any subsequent registration update occurs, then
                                   this contains a string indicating the type of error. If no
                                   error occured then this will be NULL. */
  OpalRegistrationStates m_status; /**< Status of registration, see enum for details. */
  OpalProductDescription m_product; /**< Product description data */
} OpalStatusRegistration;


/**Arbitrary information identified by MIME type.
   Commonly used for multi-part MIME data.
  */
typedef struct OpalMIME
{
  const char * m_type;      ///< MIME type for data, e.g. "text/html"
  unsigned     m_length;    ///< Length of data, relevant mainly for if data is binary
  const char * m_data;      ///< Pointer to data
} OpalMIME;


/**Set up call parameters for several command and indication messages.

   When establishing a new call via the OpalCmdSetUpCall command, the m_partyA and
   m_partyB fields indicate the parties to connect.

   For OpalCmdTransferCall, m_partyA indicates the connection to be transferred and
   m_partyB is the party to be transferred to. If the call transfer is successful then
   a OpalIndCallCleared message will be received clearing the local call.

   For OpalIndAlerting and OpalIndEstablished indications the three fields are set
   to the data for the call in progress.

   Example:
      <code>
      OpalMessage   command;
      OpalMessage * response;

      memset(&command, 0, sizeof(command));
      command.m_type = OpalCmdSetUpCall;
      command.m_param.m_callSetUp.m_partyB = "h323:10.0.1.11";
      response = OpalSendMessage(hOPAL, &command);

      memset(&command, 0, sizeof(command));
      command.m_type = OpalCmdSetUpCall;
      command.m_param.m_callSetUp.m_partyA = "pots:LINE1";
      command.m_param.m_callSetUp.m_partyB = "sip:10.0.1.11";
      response = OpalSendMessage(hOPAL, &command);
      callToken = strdup(response->m_param.m_callSetUp.m_callToken);

      memset(&command, 0, sizeof(command));
      command.m_type = OpalCmdTransferCall;
      command.m_param.m_callSetUp.m_callToken = callToken;
      command.m_param.m_callSetUp.m_partyB = "sip:10.0.1.12";
      response = OpalSendMessage(hOPAL, &command);
      </code>
  */
typedef struct OpalParamSetUpCall {
  const char * m_partyA;      /**< A-Party for call.

                                   For OpalCmdSetUpCall, this indicates what subsystem will
                                   be starting the call, e.g. "pots:Handset One". If NULL
                                   or empty string then "pc:*" is used indication that the
                                   standard PC sound system ans screen is to be used.

                                   For OpalCmdTransferCall this indicates the party to be
                                   transferred, e.g. "sip:fred@nurk.com". If NULL then
                                   it is assumed that the party to be transferred is of the
                                   same "protocol" as the m_partyB field, e.g. "pc" or
                                   "sip". If "*", then the party to be transferred will be
                                   of the same network attribute, e.g. "pc" would match
                                   "ivr" and "sip would match "h323" but "pc" would not
                                   match "sip".

                                   For OpalIndAlerting and OpalIndEstablished this indicates
                                   the A-party of the call in progress. */
  const char * m_partyB;      /**< B-Party for call. This is typically a remote host URL
                                   address with protocol, e.g. "h323:simple.com" or
                                   "sip:fred@nurk.com".

                                   This must be provided in the OpalCmdSetUpCall and
                                   OpalCmdTransferCall commands, and is set by the system
                                   in the OpalIndAlerting and OpalIndEstablished indications.

                                   If used in the OpalCmdTransferCall command, this may be
                                   a valid call token for another call on hold. The remote
                                   is transferred to the call on hold and both calls are
                                   then cleared. */
  const char * m_callToken;   /**< Value of call token for new call. The user would pass NULL
                                   for this string in OpalCmdSetUpCall, a new value is
                                   returned by the OpalSendMessage() function. The user would
                                   provide the call token for the call being transferred when
                                   OpalCmdTransferCall is being called. */
  const char * m_alertingType;/**< The type of "distinctive ringing" for the call. The string
                                   is protocol dependent, so the caller would need to be aware
                                   of the type of call being made. Some protocols may ignore
                                   the field completely.

                                   For SIP this corresponds to the string contained in the
                                   "Alert-Info" header field of the INVITE. This is typically
                                   a URI for the ring file.

                                   For H.323 this must be a string representation of an
                                   integer from 0 to 7 which will be contained in the
                                   Q.931 SIGNAL (0x34) Information Element.

                                   This is only used in OpalCmdSetUpCall to set the string to
                                   be sent to the remote to change the type of ring the remote
                                   may emit.

                                   For other indications this field is NULL. */

  const char * m_protocolCallId;  /**< ID assigned by the underlying protocol for the call. This
                                       is returned in the OpalIndIncomingCall, OpalIndAlerting
                                       and OpalIndEstablished messages.
                                       Only available in version 18 and above */
  OpalParamProtocol m_overrides;  /**< Overrides for the default parameters for the protocol.
                                       For example, m_userName and m_displayName can be
                                       changed on a call by call basis. */
  unsigned     m_extraCount; /**<Count of extra information items in m_extras. This fields contains any
                                 extra information that is available about the outgoing call. It will
                                 typically be protocol specific. For example, for SIP, this is the
                                 multi-part MIME data that may be in the INVITE. */
  const OpalMIME * m_extras; /**<Data for each extra piece of extra information. */
} OpalParamSetUpCall;


/**Incoming call information for the OpalIndIncomingCall indication.
   This is only returned from the OpalGetMessage() function.
  */
typedef struct OpalStatusIncomingCall {
  const char * m_callToken;         ///< Call token for new call.
  const char * m_localAddress;      ///< URL of local interface. e.g. "sip:me@here.com"
  const char * m_remoteAddress;     /**< URL of calling party. e.g. "sip:them@there.com", this is
                                         the best guess on how to call the remote party back. This
                                         may not be the same as the non server specific "identity"
                                         of the remote user, see m_remoteIdentity. */
  const char * m_remotePartyNumber; ///< This is the E.164 number of the caller, if available.
  const char * m_remoteDisplayName; ///< Display name calling party. e.g. "Fred Nurk"
  const char * m_calledAddress;     ///< URL of called party the remote is trying to contact.
  const char * m_calledPartyNumber; ///< This is the E.164 number of the called party, if available.
  OpalProductDescription m_product; /**< Product description data */
  const char * m_alertingType;/**< The type of "distinctive ringing" for the call. The string
                                   is protocol dependent, so the caller would need to be aware
                                   of the type of call being made. Some protocols may ignore
                                   the field completely.

                                   For SIP this corresponds to the string contained in the
                                   "Alert-Info" header field of the INVITE. This is typically
                                   a URI for the ring file.

                                   For H.323 this must be a string representation of an
                                   integer from 0 to 7 which will be contained in the
                                   Q.931 SIGNAL (0x34) Information Element. */
  const char * m_protocolCallId;  /**< ID assigned by the underlying protocol for the call. 
                                       Only available in version 18 and above */
  const char * m_referredByAddress; ///< This is the full address of the party doing transfer, if available.
  const char * m_redirectingNumber; ///< This is the E.164 number of the party doing transfer, if available.
  unsigned     m_extraCount; /**<Count of extra information items in m_extras. This fields contains any
                                 extra information that is available about the incoming call. It will
                                 typically be protocol specific. For example, for SIP, this is the
                                 multi-part MIME data that may be in the INVITE. */
  const OpalMIME * m_extras; /**<Data for each extra piece of extra information. */
  const char * m_remoteIdentity; /**< This is the identity of the remote user. Usually it is
                                      identical to m_remoteAddress, but depending on the protocol
                                      and system configuration, it may be different. A simple
                                      example is where the identity is "fred@nurk.com" but the
                                      address is "sip:fred@10.11.12.13:1415" */
  const char * m_supportedFeatures; /** A list of supported features by name, separated by '\n'.
                                        This is protocol dependent, for example, it corresponds to
                                        the values of the "Supported" header in an incoming INVITE.
                                        For H.323 it would be things like "H.460.18" etc. Note:
                                        NULL indicagtes not supported by C API version, while empty
                                        string indicates supported but no features indicated. */
} OpalStatusIncomingCall;


/**Incoming call response parameters for OpalCmdAlerting and OpalCmdAnswerCall messages.

   When a new call is detected via the OpalIndIncomingCall indication, the application
   should respond with OpalCmdClearCall, which does not use this structure, or
   OpalCmdAnswerCall, which does. An optional OpalCmdAlerting may also be sent
   which also uses this structure to allow for the override of default call parameters
   such as suer name or display name on a call by call basis.
  */
typedef struct OpalParamAnswerCall {
  const char * m_callToken;       ///< Call token for call to be answered.
  OpalParamProtocol m_overrides;  /**< Overrides for the default parameters for the protocol.
                                       For example, m_userName and m_displayName can be
                                       changed on a call by call basis. */
  unsigned m_withMedia;           /**< When used with OpalCmdAlerting, if non-zero this
                                       indicates that early media is to be started. */
} OpalParamAnswerCall;

/**Type code for media stream status/control.
   This is used by the OpalIndMediaStream indication and OpalCmdMediaStream command
   in the OpalStatusMediaStream structure.
  */
typedef enum OpalMediaStates {
  OpalMediaStateNoChange,   /**< No change to the media stream state. */
  OpalMediaStateOpen,       /**< Media stream has been opened when indication,
                                 or is to be opened when a command. */
  OpalMediaStateClose,      /**< Media stream has been closed when indication,
                                 or is to be closed when a command. */
  OpalMediaStatePause,      /**< Media stream has been paused when indication,
                                 or is to be paused when a command. */
  OpalMediaStateResume      /**< Media stream has been paused when indication,
                                 or is to be paused when a command. */
} OpalMediaStates;


/**Media stream information for the OpalIndMediaStream indication and
   OpalCmdMediaStream command.

   This is may be returned from the OpalGetMessage() function or
   provided to the OpalSendMessage() function.
  */
typedef struct OpalStatusMediaStream {
  const char    * m_callToken;   ///< Call token for the call the media stream is.
  const char    * m_identifier;  /**< Unique identifier for the media stream. For OpalCmdMediaStream
                                      this may be left empty and the first stream of the type
                                      indicated by m_mediaType is used. */
  const char    * m_type;        /**< Media type and direction for the stream. This is a keyword such
                                      as "audio" or "video" indicating the type of the stream, a space,
                                      then either "in" or "out" indicating the direction. For
                                      OpalCmdMediaStream this may be left empty if m_identifier is
                                      used. */
  const char    * m_format;      /**< Media format for the stream. For OpalIndMediaStream this
                                      shows the format being used. For OpalCmdMediaStream this
                                      is the format to use. In the latter case, if empty or
                                      NULL, then a default is used. */
  OpalMediaStates m_state;       /**< For OpalIndMediaStream this indicates the status of the stream.
                                      For OpalCmdMediaStream this indicates the state to move to, see
                                      OpalMediaStates for more information. */
  int             m_volume;      /**< Set the volume for the media stream as a percentage. Note this
                                      is dependent on the stream type and may be ignored. Also, a
                                      percentage of zero does not indicate muting, it indicates no
                                      change in volume. Use -1, to mute. */
  const char    * m_watermark;   /**< For a video transmit meda stream, this indicates a secondary
                                      video source device place on lower right corner. It would
                                      typically be a .BMP or .JPG file, but theoretically could be
                                      any video source device, including another camera. */
} OpalStatusMediaStream;


/** Assign a user data field to a call
*/
typedef struct OpalParamSetUserData {
  const char    * m_callToken;   ///< Call token for the call the media stream is.
  void *        m_userData;      ///< user data value to associate with this call
} OpalParamSetUserData;


/**User input information for the OpalIndUserInput/OpalCmdUserInput indication.

   This is may be returned from the OpalGetMessage() function or
   provided to the OpalSendMessage() function.
  */
typedef struct OpalStatusUserInput {
  const char * m_callToken;   ///< Call token for the call the User Input was received on.
  const char * m_userInput;   ///< User input string, e.g. "#".
  unsigned     m_duration;    /**< Duration in milliseconds for tone. For DTMF style user input
                                   the time the tone was detected may be placed in this field.
                                   Generally zero is passed which means the m_userInput is a
                                   single "string" input. If non-zero then m_userInput must
                                   be a single character. */
} OpalStatusUserInput, OpalParamUserInput;


/**Message Waiting information for the OpalIndMessageWaiting indication.
   This is only returned from the OpalGetMessage() function.
  */
typedef struct OpalStatusMessageWaiting {
  const char * m_party;     ///< Party for which the MWI is directed
  const char * m_type;      ///< Type for MWI, "Voice", "Fax", "Pager", "Multimedia", "Text", "None"
  const char * m_extraInfo; /**< Extra information for the MWI, e.g. "SUBSCRIBED",
                                 "UNSUBSCRIBED", "2/8 (0/2)"
                             */
} OpalStatusMessageWaiting;


/**Type code for media stream status/control.
   This is used by the OpalIndMediaStream indication and OpalCmdMediaStream command
   in the OpalStatusMediaStream structure.
  */
typedef enum OpalLineAppearanceStates {
  OpalLineTerminated,  /**< Line has ended a call. */
  OpalLineTrying,      /**< Line has been siezed. */
  OpalLineProceeding,  /**< Line is trying to make a call. */
  OpalLineRinging,     /**< Line is ringing. */
  OpalLineConnected,   /**< Line is connected. */
  OpalLineSubcribed,   /**< Line appearance subscription successful. */
  OpalLineUnsubcribed, /**< Line appearance unsubscription successful. */

  OpalLineIdle = OpalLineTerminated // Kept for backward compatibility
} OpalLineAppearanceStates;


/**Line Appearance information for the OpalIndLineAppearance indication.
   This is only returned from the OpalGetMessage() function.
  */
typedef struct OpalStatusLineAppearance {
  const char *             m_line;       ///< URI for the line whose state is changing
  OpalLineAppearanceStates m_state;      ///< State the line has just moved to.
  int                      m_appearance; /**< Appearance code, this is an arbitrary integer
                                              and is defined by the remote servers. If negative
                                              then it is undefined. */
  const char *             m_callId;     /**< If line is "in use" then this gives information
                                              that identifies the call. Note that this will
                                              include the from/to "tags" that can identify
                                              the dialog for REFER/Replace. */
  const char *             m_partyA;     /**< A-Party for call. */
  const char *             m_partyB;     /**< B-Party for call. */
} OpalStatusLineAppearance;


/**Type code for presence states.
   This is used by the OpalPresenceStatus structure.
  */
typedef enum OpalPresenceStates {
  OpalPresenceAuthRequest = -100, ///< Authorisation to view a users state is required
  OpalUnknownPresentity = -4,     ///< Presentity does not exist
  OpalPresenceError,              ///< Something bad happened
  OpalPresenceForbidden,          ///< Access to presence information was specifically forbidden
  OpalPresenceNone,               ///< No presence status - not the same as Unavailable or Away
  OpalPresenceUnchanged,          ///< State has not changed from last time
  OpalPresenceAvailable,          ///< User has a presence and is available to be contacted
  OpalPresenceUnavailable         ///< User has a presence, but is cannot be contacted
} OpalPresenceStates;


/**Opal Presence information for the various presence messages.

   For OpalIndPresenceChange, m_entity is the local presentity, m_target is
   the presentity for which the status is changing. This may be a remote
   presentity, or the same as the m_entity field. The latter would occur after
   OpalCmdSetLocalPresence, for example, and can indicate if that operation
   was successful or not.

   For OpalCmdAuthorisePresence, m_entity is the local presentity, m_target is
   the remote presentity asking for permission to view the local presentities
   status. The m_state is used to deny acces (OpalPresenceForbidden), deny
   access politely (OpalPresenceUnavailable) or permit access
   (OpalPresenceAvailable). While this is usually called in response to an
   OpalIndPresenceChange with m_state == OpalPresenceAuthRequest, it can also
   be sent at any time to remove an authorisation. In this case m_state is set
   to OpalPresenceNone.

   For OpalCmdSetLocalPresence, m_target is unused, m_state should be a
   positive value. If m_state is OpalPresenceUnchanged then no change is made
   and the current presence state is returned. The m_note field can be used
   to provide extra information about the state change.

   For OpalCmdSubscribePresence, m_entity is the local presentity, m_target is
   the remote presentity we wish to monitor. The m_state is OpalPresenceNone
   when we wish to stop monitoring, any other value to request. The m_note
   field may be used to give extra information to the remote system if
   authorisation is required.
  */
typedef struct OpalPresenceStatus {
  const char *       m_entity;    /**<For OpalIndPresenceChange, this is the presentity
                                      whose state had changed, usually a remote. For other
                                      messages, this is the local registered presentity. */
  const char *       m_target;    /**<The presentity that is being informed about the state
                                      change. Only used for OpalIndPresenceChange. */
  const char *       m_service;   /**<Device/system for the presentity that is getting a
                                      state change. Ignored for commands. */
  const char *       m_contact;   /**<Contact address, typically a URL, for the service. */
  const char *       m_capabilities;/**<Capabilities for the service. A '\n' separated list
                                      of keywords, such as "audio", "Video", "text" etc. */
  OpalPresenceStates m_state;     /**<The new state of the target entity. */
  const char *       m_activities;/**<The optional activities, if m_state is OpalPresenceAvailable
                                      or OpalPresenceUnavailable. Typically something like
                                      "Busy" or "Away". This can be a '\n' seaparated
                                      list of simultaneous activities. */
  const char *       m_note;      /**<Additional "note" that may be attached to the state
                                      change, e.g. "I want to be frends with you". If
                                      m_state is OpalPresenceError, then this may contain
                                      extra information on the error. */
  const char *       m_infoType;  ///< MIME tyupe for m_infoData, e.g. application/pidf+xml
  const char *       m_infoData;  ///< Raw information as provided by underlying protocol, e.g. XML.
} OpalPresenceStatus;


/**Opal Instant Message information for the various presence messages.
   This can be filled out and used in the OpalCmdSendIM command. The result
   of that transmission is returned by OpalIndSentIM, where m_textBody contains
   a string indicating the disposition of the message.

   The OpalIndReceiveIM message uses this structure for incoming instant
   messages from a remote.
  */
typedef struct OpalInstantMessage {
  const char *  m_from;      /**<Address from whom the message is sent. */
  const char *  m_to;        /**<Address to which the message is sent. */
  const char *  m_host;      /**<Optional host/proxy. If blank then it is
                                derived from the m_to address. */
  const char *  m_conversationId; /**<Conversation identifier. This may be
                                provided by the caller if athe conversation
                                exists. If starting a new conversation, leave
                                empty and OpalCmdSendIM will return it. */
  const char *  m_textBody;  /**<Simple text body, if present. This will always
                                be MIME type "text/plain". It will also be included
                                in the m_bodyCount and m_bodies. */
  unsigned      m_bodyCount; /**<Count of bodies in m_mimeType and m_bodies */
  const char ** m_mimeType;  /**<MIME type for each body, e.g. "text/html" */
  const char ** m_bodies;    /**<Body data for each MIME type. Deprecated in favour
                                of m_bodyData which supports binary data. */
  unsigned      m_messageId; /**<Identifer for this message. This can be used
                                 to match a message sent with OpalCmdSendIM with
                                 the disposition in OpalIndSentIM. It is not set
                                 by the user, and is returned by OpalCmdSendIM. */
  const char *  m_htmlBody;  /**<HTML text body, if present. This will always
                                be MIME type "text/html". It will also be included
                                in the m_bodyCount and m_bodies. */
  const OpalMIME * m_bodyData; /**<Body data. Pointer to m_bodyCount entries. */
} OpalInstantMessage;


/**Type of mixing for video when recording.
   This is used by the OpalCmdStartRecording command in the OpalParamRecording structure.
  */
typedef enum OpalVideoRecordMixMode {
  OpalSideBySideLetterbox, /**< Two images side by side with black bars top and bottom.
                                It is expected that the input frames and output are all
                                the same aspect ratio, e.g. 4:3. Works well if inputs
                                are QCIF and output is CIF for example. */
  OpalSideBySideScaled,    /**< Two images side by side, scaled to fit halves of output
                                frame. It is expected that the output frame be double
                                the width of the input data to maintain aspect ratio.
                                e.g. for CIF inputs, output would be 704x288. */
  OpalStackedPillarbox,    /**< Two images, one on top of the other with black bars down
                                the sides. It is expected that the input frames and output
                                are all the same aspect ratio, e.g. 4:3. Works well if
                                inputs are QCIF and output is CIF for example. */
  OpalStackedScaled,       /**< Two images, one on top of the other, scaled to fit halves
                                of output frame. It is expected that the output frame be
                                double the height of the input data to maintain aspect
                                ratio. e.g. for CIF inputs, output would be 352x576. */
} OpalVideoRecordMixMode;


/**Call recording information for the OpalCmdStartRecording command.
  */
typedef struct OpalParamRecording {
  const char * m_callToken;  ///< Call token for call being recorded.
  const char * m_file;       /**< File to record into. If NULL then a test is done
                                  for if recording is currently active. */
  unsigned     m_channels;   /**< Number of channels in WAV file, 1 for mono (default) or 2 for
                                  stereo where incoming & outgoing audio are in individual
                                  channels. */
  const char * m_audioFormat; /**< Audio recording format. This is generally an OpalMediaFormat
                                   name which will be used in the recording file. The exact
                                   values possible is dependent on many factors including the
                                   specific file type and what codecs are loaded as plug ins. */
  const char * m_videoFormat; /**< Video recording format. This is generally an OpalMediaFormat
                                   name which will be used in the recording file. The exact
                                   values possible is dependent on many factors including the
                                   specific file type and what codecs are loaded as plug ins. */
  unsigned     m_videoWidth;  /**< Width of image for recording video. */
  unsigned     m_videoHeight; /**< Height of image for recording video. */
  unsigned     m_videoRate;   /**< Frame rate for recording video. */
  OpalVideoRecordMixMode m_videoMixing; /**< How the two images are saved in video recording. */
  unsigned     m_audioBufferSize; /**< Size of buffer before writing to output. Note, this
                                       will always be rounded up to whole packet sizes. */
} OpalParamRecording;


/**Call transfer information for the OpalIndTransferCall indication.
   This is only returned from the OpalGetMessage() function.
  */
typedef struct OpalStatusTransferCall {
  const char * m_callToken;       ///< Call token for call being transferred.
  const char * m_protocolCallId;  /**< ID assigned by the underlying protocol for the call. 
                                       Only available in version 18 and above */
  const char * m_result;          /**< Result of transfer operation. This is one of:
                                            "progress"  transfer of this call is still in progress.
                                            "success"   transfer of this call completed, call will
                                                        be cleared.
                                            "failed"    transfer initiated by this call did not
                                                        complete, call remains active.
                                            "started"   remote system has asked local connection
                                                        to transfer to another target.
                                            "completed" local connection has completed the transfer
                                                        to other target.
                                            "forwarded" remote has forwarded call local system has
                                                        initiated to another address.
                                            "incoming"  this call is the target of an incoming
                                                        transfer, e.g. party C in a consultation
                                                        transfer scenario. */
  const char * m_info;    /**< Protocol dependent information in the form:
                                           key=value\n
                                           key=value\n
                                           etc */
} OpalStatusTransferCall;


/**IVR information for the OpalIndCompletedIVR indication.
   This is only returned from the OpalGetMessage() function.
  */
typedef struct OpalStatusIVR {
  const char * m_callToken;   ///< Call token for call being cleared.
  const char * m_variables;   /**< Final values for variables defined by the script.
                                   These will be in the form:
                                           varname=value\n
                                           varname=value\n
                                           etc */
} OpalStatusIVR;


/**Indication of a protocol specific message.
   Sent by OpalIndProtocolMessage message.

   In the case of a SIP INFO message, the protocol message identifier
   (m_identifier) is the string "INFO\t" followed by the "Info-Package"
   header for that message.

   In the case if a SIP re-INVITE, the identifier will be "INVITE" and the
   payload will be the SDP received, if any.

   In all cases, if multi-part mime was received, that information is placed
   into the "extras" fields, similar to OpalParamSetUpCall and OpalStatusIncomingCall.
  */
typedef struct OpalProtocolMessage {
  const char * m_protocol;   ///< Protocol this message is from, e.g. "sip".
  const char * m_callToken;  ///< Call token for context of the message.
  const char * m_identifier; ///< Protocol specific indentifier for what this message is about.
  const void * m_payload;    ///< Extra protocol and identifier specific data.
  unsigned     m_size;       ///< Size of the above data.
  unsigned     m_extraCount; /**<Count of extra information items in m_extras. This fields contains any
                             extra information that is available about the outgoing call. It will
                             typically be protocol specific. For example, for SIP, this is the
                             multi-part MIME data that may be in the re-INVITE. */
  const OpalMIME * m_extras; /**<Data for each extra piece of extra information. */
} OpalProtocolMessage;


/**Call clearance information for the OpalIndCallCleared indication.
   This is only returned from the OpalGetMessage() function.
  */
typedef struct OpalStatusCallCleared {
  const char * m_callToken;   ///< Call token for call being cleared.
  const char * m_reason;      /**< String representing the reason for the call
                                   completing. This string begins with a numeric
                                   code corresponding to values in the
                                   OpalCallEndReason enum, followed by a colon and
                                   an English description. */
} OpalStatusCallCleared;


/**Type code for the reasons a call was ended.
  */
typedef enum OpalCallEndReason {
    OpalCallEndedByLocalUser,              ///< Local endpoint application cleared call
    OpalCallEndedByNoAccept,               ///< Local endpoint did not accept call OnIncomingCall()=false
    OpalCallEndedByAnswerDenied,           ///< Local endpoint declined to answer call
    OpalCallEndedByRemoteUser,             ///< Remote endpoint application cleared call
    OpalCallEndedByRefusal,                ///< Remote endpoint refused call
    OpalCallEndedByNoAnswer,               ///< Remote endpoint did not answer in required time
    OpalCallEndedByCallerAbort,            ///< Remote endpoint stopped calling
    OpalCallEndedByTransportFail,          ///< Transport error cleared call
    OpalCallEndedByConnectFail,            ///< Transport connection failed to establish call
    OpalCallEndedByGatekeeper,             ///< Gatekeeper has cleared call
    OpalCallEndedByNoUser,                 ///< Call failed as could not find user (in GK)
    OpalCallEndedByNoBandwidth,            ///< Call failed as could not get enough bandwidth
    OpalCallEndedByCapabilityExchange,     ///< Could not find common capabilities
    OpalCallEndedByCallForwarded,          ///< Call was forwarded using FACILITY message
    OpalCallEndedBySecurityDenial,         ///< Call failed a security check and was ended
    OpalCallEndedByLocalBusy,              ///< Local endpoint busy
    OpalCallEndedByLocalCongestion,        ///< Local endpoint congested
    OpalCallEndedByRemoteBusy,             ///< Remote endpoint busy
    OpalCallEndedByRemoteCongestion,       ///< Remote endpoint congested
    OpalCallEndedByUnreachable,            ///< Could not reach the remote party
    OpalCallEndedByNoEndPoint,             ///< The remote party is not running an endpoint
    OpalCallEndedByHostOffline,            ///< The remote party host off line
    OpalCallEndedByTemporaryFailure,       ///< The remote failed temporarily app may retry
    OpalCallEndedByQ931Cause,              ///< The remote ended the call with Q.931 cause code in MS byte
    OpalCallEndedByDurationLimit,          ///< Call cleared due to an enforced duration limit
    OpalCallEndedByInvalidConferenceID,    ///< Call cleared due to invalid conference ID
    OpalCallEndedByNoDialTone,             ///< Call cleared due to missing dial tone
    OpalCallEndedByNoRingBackTone,         ///< Call cleared due to missing ringback tone
    OpalCallEndedByOutOfService,           ///< Call cleared because the line is out of service, 
    OpalCallEndedByAcceptingCallWaiting,   ///< Call cleared because another call is answered
    OpalCallEndedByGkAdmissionFailed,      ///< Call cleared because gatekeeper admission request failed.
    OpalCallEndedByMediaFailed,            ///< Call cleared due to loss of media flow.
    OpalCallEndedByCallCompletedElsewhere, ///< Call cleared because it was answered by another extension.
    OpalCallEndedByCertificateAuthority,   ///< When using TLS, the remote certifcate was not authenticated
    OpalCallEndedByIllegalAddress,         ///< Destination Address  format was incorrect format
    OpalCallEndedByCustomCode              ///< End call with custom protocol specific code (e.g. SIP)
} OpalCallEndReason;


/**Call clearance information for the OpalCmdClearCall command.
  */
typedef struct OpalParamCallCleared {
  const char      * m_callToken;  ///< Call token for call being cleared.
  OpalCallEndReason m_reason;     /**< Code for the call termination to be provided to the
                                       remote system. */
  unsigned          m_custom;     /**< Custom code for OpalCallEndedByQ931Cause &
                                       OpalCallEndedByCustomCode reasons */
} OpalParamCallCleared;


union OpalMessageParam {
  const char *             m_commandError;       ///< Used by OpalIndCommandError
  OpalParamGeneral         m_general;            ///< Used by OpalCmdSetGeneralParameters
  OpalParamProtocol        m_protocol;           ///< Used by OpalCmdSetProtocolParameters
  OpalParamRegistration    m_registrationInfo;   ///< Used by OpalCmdRegistration
  OpalStatusRegistration   m_registrationStatus; ///< Used by OpalIndRegistration
  OpalParamSetUpCall       m_callSetUp;          ///< Used by OpalCmdSetUpCall/OpalIndProceeding/OpalIndAlerting/OpalIndEstablished
  const char *             m_callToken;          ///< Used by OpalCmdHoldcall/OpalCmdRetrieveCall/OpalCmdStopRecording
  OpalStatusIncomingCall   m_incomingCall;       ///< Used by OpalIndIncomingCall
  OpalParamAnswerCall      m_answerCall;         ///< Used by OpalCmdAnswerCall/OpalCmdAlerting
  OpalStatusUserInput      m_userInput;          ///< Used by OpalIndUserInput/OpalCmdUserInput
  OpalStatusMessageWaiting m_messageWaiting;     ///< Used by OpalIndMessageWaiting
  OpalStatusLineAppearance m_lineAppearance;     ///< Used by OpalIndLineAppearance
  OpalStatusCallCleared    m_callCleared;        ///< Used by OpalIndCallCleared
  OpalParamCallCleared     m_clearCall;          ///< Used by OpalCmdClearCall
  OpalStatusMediaStream    m_mediaStream;        ///< Used by OpalIndMediaStream/OpalCmdMediaStream
  OpalParamSetUserData     m_setUserData;        ///< Used by OpalCmdSetUserData
  OpalParamRecording       m_recording;          ///< Used by OpalCmdStartRecording
  OpalStatusTransferCall   m_transferStatus;     ///< Used by OpalIndTransferCall
  OpalStatusIVR            m_ivrStatus;          ///< Used by OpalIndCompletedIVR
  OpalPresenceStatus       m_presenceStatus;     ///< used by OpalCmdAuthorisePresence/OpalCmdSubscribePresence/OpalIndPresenceChange/OpalCmdSetLocalPresence
  OpalInstantMessage       m_instantMessage;     ///< Used by OpalCmdSendIM/OpalIndReceiveIM
  OpalProtocolMessage      m_protocolMessage;    ///< Used by OpalIndProtocolMessage
};


/** Message to/from OPAL system.
    This is passed via the OpalGetMessage() or OpalSendMessage() functions.
  */
struct OpalMessage {
  OpalMessageType m_type;   ///< Type of message
  union OpalMessageParam m_param;   ///< Context sensitive parameter based on m_type
};


#define OPALMSG_INIT(msg,type,field) (memset(&(msg), 0, sizeof(msg)),(msg).m_type=type,&(msg).m_param.field)
  
/// Initialise an OpalMessage for OpalCmdSetGeneralParameters, returns OpalParamGeneral*.
#define OPALMSG_GENERAL_PARAM(msg) OPALMSG_INIT(msg,OpalCmdSetGeneralParameters,m_general)
  
/// Initialise an OpalMessage for OpalCmdSetProtocolParameters, returns OpalParamProtocol*.
#define OPALMSG_PROTO_PARAM(msg) OPALMSG_INIT(msg,OpalCmdSetProtocolParameters,m_protocol)
  
/// Initialise an OpalMessage for OpalCmdRegistration, returns OpalParamRegistration*.
#define OPALMSG_REGISTRATION(msg) OPALMSG_INIT(msg,OpalCmdRegistration,m_registrationInfo)
  
/// Initialise an OpalMessage for OpalCmdSetUpCall, returns OpalParamSetUpCall*.
#define OPALMSG_SETUP_CALL(msg) OPALMSG_INIT(msg,OpalCmdSetUpCall,m_callSetUp)
  
/// Initialise an OpalMessage for OpalCmdTransferCall, returns OpalParamSetUpCall*.
#define OPALMSG_TRANSFER(msg) OPALMSG_INIT(msg,OpalCmdTransferCall,m_callSetUp)
  
/// Initialise an OpalMessage for OpalCmdAnswerCall, returns OpalParamAnswerCall*.
#define OPALMSG_ANSWER_CALL(msg) OPALMSG_INIT(msg,OpalCmdAnswerCall,m_answerCall)
  
/// Initialise an OpalMessage for OpalCmdClearCall, returns OpalParamCallCleared*.
#define OPALMSG_CLEAR_CALL(msg) OPALMSG_INIT(msg,OpalCmdClearCall,m_clearCall)

/// Initialise an OpalMessage for OpalCmdSetUserData, returns OpalParamSetUserData*.
#define OPALMSG_SET_USER_DATA(msg) OPALMSG_INIT(msg,OpalCmdSetUserData,m_setUserData)
  
/// Initialise an OpalMessage for OpalCmdStartRecording, returns OpalParamRecording*.
#define OPALMSG_START_RECORDING(msg) OPALMSG_INIT(msg,OpalCmdStartRecording,m_recording)
  
  
#ifdef __cplusplus
};
#endif

#if defined(__cplusplus) || defined(DOC_PLUS_PLUS)

/// Wrapper around the OpalMessage structure
class OpalMessagePtr
{
  public:
    OpalMessagePtr(OpalMessageType type = OpalIndCommandError);
    ~OpalMessagePtr();

    OpalMessageType GetType() const;
    OpalMessagePtr & SetType(OpalMessageType type);

    const char               * GetCallToken() const;          ///< Used by OpalCmdHoldCall/OpalCmdRetrieveCall/OpalCmdStopRecording
    void                       SetCallToken(const char * token);

    const char               * GetCommandError() const;       ///< Used by OpalIndCommandError

    OpalParamGeneral         * GetGeneralParams() const;      ///< Used by OpalCmdSetGeneralParameters
    OpalParamProtocol        * GetProtocolParams() const;     ///< Used by OpalCmdSetProtocolParameters
    OpalParamRegistration    * GetRegistrationParams() const; ///< Used by OpalCmdRegistration
    OpalStatusRegistration   * GetRegistrationStatus() const; ///< Used by OpalIndRegistration
    OpalParamSetUpCall       * GetCallSetUp() const;          ///< Used by OpalCmdSetUpCall/OpalIndProceeding/OpalIndAlerting/OpalIndEstablished
    OpalStatusIncomingCall   * GetIncomingCall() const;       ///< Used by OpalIndIncomingCall
    OpalParamAnswerCall      * GetAnswerCall() const;         ///< Used by OpalCmdAnswerCall/OpalCmdAlerting
    OpalStatusUserInput      * GetUserInput() const;          ///< Used by OpalIndUserInput/OpalCmdUserInput
    OpalStatusMessageWaiting * GetMessageWaiting() const;     ///< Used by OpalIndMessageWaiting
    OpalStatusLineAppearance * GetLineAppearance() const;     ///< Used by OpalIndLineAppearance
    OpalStatusCallCleared    * GetCallCleared() const;        ///< Used by OpalIndCallCleared
    OpalParamCallCleared     * GetClearCall() const;          ///< Used by OpalCmdClearCall
    OpalStatusMediaStream    * GetMediaStream() const;        ///< Used by OpalIndMediaStream/OpalCmdMediaStream
    OpalParamSetUserData     * GetSetUserData() const;        ///< Used by OpalCmdSetUserData
    OpalParamRecording       * GetRecording() const;          ///< Used by OpalCmdStartRecording
    OpalStatusTransferCall   * GetTransferStatus() const;     ///< Used by OpalIndTransferCall
    OpalPresenceStatus       * GetPresenceStatus() const;     ///< Used by OpalCmdAuthorisePresence/OpalCmdSubscribePresence/OpalIndPresenceChange/OpalCmdSetLocalPresence
    OpalInstantMessage       * GetInstantMessage() const;     ///< Used by OpalCmdSendIM/OpalIndReceiveIM

#ifndef SWIG
    operator OpalParamGeneral         *() const { return GetGeneralParams(); }
    operator OpalParamProtocol        *() const { return GetProtocolParams(); }
    operator OpalParamRegistration    *() const { return GetRegistrationParams(); }
    operator OpalStatusRegistration   *() const { return GetRegistrationStatus(); }
    operator OpalParamSetUpCall       *() const { return GetCallSetUp(); }
    operator OpalStatusIncomingCall   *() const { return GetIncomingCall(); }
    operator OpalParamAnswerCall      *() const { return GetAnswerCall(); }
    operator OpalStatusUserInput      *() const { return GetUserInput(); }
    operator OpalStatusMessageWaiting *() const { return GetMessageWaiting(); }
    operator OpalStatusLineAppearance *() const { return GetLineAppearance(); }
    operator OpalStatusCallCleared    *() const { return GetCallCleared(); }
    operator OpalParamCallCleared     *() const { return GetClearCall(); }
    operator OpalStatusMediaStream    *() const { return GetMediaStream(); }
    operator OpalParamSetUserData     *() const { return GetSetUserData(); }
    operator OpalParamRecording       *() const { return GetRecording(); }
    operator OpalStatusTransferCall   *() const { return GetTransferStatus(); }
    operator OpalPresenceStatus       *() const { return GetPresenceStatus(); }
    operator OpalInstantMessage       *() const { return GetInstantMessage(); }
#endif // SWIG

  protected:
    OpalMessage * m_message;

  private:
    OpalMessagePtr(const OpalMessagePtr &) { }
    void operator=(const OpalMessagePtr &) { }

  friend class OpalContext;
};


#ifdef GetMessage
#undef GetMessage
#endif
#ifdef SendMessage
#undef SendMessage
#endif


/** This class is a wrapper around the "C" API.

    It may seem odd to have a C++ wrapper around a "C" API which is itself a
    wrapper around a C++ API, but sometimes a C++ programmer may wish to
    access the OPAL system via this simplified API instead of the quite
    complex one in the base OPAL library.
  */
class OpalContext
{
  public:
    /// Construct an unintialised OPAL context.
    OpalContext();

    /// Destroy the OPAL context, calls ShutDown().
    virtual ~OpalContext();

    /// Calls OpalIntialise() to initialise the OPAL context.
    /// Returns version of API supported by library, zero if error.
    unsigned Initialise(
      const char * options,  ///< List of options to pass to OpalIntialise()
      unsigned version = OPAL_C_API_VERSION ///< Version expected by application
    );

    /// Indicate if the OPAL context has been initialised.
    bool IsInitialised() const { return m_handle != NULL; }

    /// Calls OpalShutDown() to dispose of the OPAL context.
    void ShutDown();

    /// Calls OpalGetMessage() to get next message from the OPAL context.
    bool GetMessage(
      OpalMessagePtr & message,
      unsigned timeout = 0
    );

    /// Calls OpalSendMessage() to send a message to the OPAL context.
    bool SendMessage(
      const OpalMessagePtr & message  ///< Message to send to OPAL.
    );
    bool SendMessage(
      const OpalMessagePtr & message,  ///< Message to send to OPAL.
      OpalMessagePtr & response        ///< Response from OPAL.
    );


    /// Execute OpalSendMessage() using OpalCmdSetUpCall
    bool SetUpCall(
      OpalMessagePtr & response,       ///< Response from OPAL context on initiating call.
      const char * partyB,             ///< Destination address, see OpalCmdSetUpCall.
      const char * partyA = NULL,      ///< Calling sub-system, see OpalCmdSetUpCall.
      const char * alertingType = NULL ///< Alerting type code, see OpalCmdSetUpCall.
    );

    /// Answer a call using OpalCmdAnswerCall via OpalSendMessage()
    bool AnswerCall(
      const char * callToken           ///< Call token for call being answered.
    );

    /// Clear a call using OpalCmdClearCall via OpalSendMessage()
    bool ClearCall(
      const char * callToken,          ///< Call token for call being cleared.
      OpalCallEndReason reason = OpalCallEndedByLocalUser  ///< Code for the call termination, see OpalCmdClearCall.
    );

    /// Send user input using OpalCmdUserInput via OpalSendMessage()
    bool SendUserInput(
      const char * callToken,     ///< Call token for the call, see OpalCmdUserInput.
      const char * userInput,     ///< User input string, e.g. "#", see OpalCmdUserInput.
      unsigned     duration = 0   ///< Duration in milliseconds for tone, see OpalCmdUserInput.
    );

#ifndef SWIG
    // Windows API compatibility
    __inline bool GetMessageA(
      OpalMessagePtr & message,
      unsigned timeout = 0
    ) { return GetMessage(message, timeout); }
    __inline bool GetMessageW(
      OpalMessagePtr & message,
      unsigned timeout = 0
    ) { return GetMessage(message, timeout); }
    __inline bool SendMessageA(
      const OpalMessagePtr & message
    ) { return SendMessage(message); }
    __inline bool SendMessageA(
      const OpalMessagePtr & message,
      OpalMessagePtr & response
    ) { return SendMessage(message, response); }
    __inline bool SendMessageW(
      const OpalMessagePtr & message
    ) { return SendMessage(message); }
    __inline bool SendMessageW(
      const OpalMessagePtr & message,
      OpalMessagePtr & response
    ) { return SendMessage(message, response); }
#endif // SWIG

  protected:
    OpalHandle m_handle;
};

#endif // defined(__cplusplus)

#endif // OPAL_OPAL_H


/////////////////////////////////////////////////////////////////////////////

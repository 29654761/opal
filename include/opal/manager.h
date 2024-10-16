/*
 * manager.h
 *
 * OPAL system manager.
 *
 * Open Phone Abstraction Library (OPAL)
 * Formally known as the Open H323 project.
 *
 * Copyright (c) 2001 Equivalence Pty. Ltd.
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
 * The Initial Developer of the Original Code is Equivalence Pty. Ltd.
 *
 * Contributor(s): ______________________________________.
 */

#ifndef OPAL_OPAL_MANAGER_H
#define OPAL_OPAL_MANAGER_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#include <opal_config.h>

#include <opal/pres_ent.h>
#include <opal/call.h>
#include <opal/connection.h> //OpalConnection::AnswerCallResponse
#include <opal/guid.h>
#include <codec/silencedetect.h>
#include <codec/echocancel.h>
#include <im/im.h>

#include <ptclib/pstun.h>
#include <ptclib/url.h>
#include <ptclib/pxml.h>
#include <ptclib/threadpool.h>

#if OPAL_VIDEO
// Inside #if so does not force loading of factories when statically linked.
#include <ptlib/videoio.h>
#endif


class OpalEndPoint;
class OpalMediaPatch;
class OpalLocalConnection;
class PSSLCertificate;
class PSSLPrivateKey;


#define OPAL_SCRIPT_CALL_TABLE_NAME "OpalCall"


class OpalConferenceState : public PObject
{
  PCLASSINFO(OpalConferenceState, PObject);
public:
  OpalConferenceState()
    : m_active(true)
    , m_locked(false)
    , m_maxUsers(0)
  { }

  enum ChangeType {
    Created,
    Destroyed,
    UserAdded,
    UserRemoved,
    NumChangeTypes
  };
  friend ostream & operator<<(ostream & strm, ChangeType type);

  PString  m_internalURI;         ///< Internal URI, e.g. mcu:5e6f7347-dcd6-e011-9853-0026b9b946a5

  PString  m_displayText;         ///< Human readable text for conference name
  PString  m_subject;             ///< Subject for conference
  PString  m_notes;               ///< Free text about conference
  PString  m_keywords;            ///< Space separated list of keywords for conference
  bool     m_active;              ///< Conference is active and can accept incoming connections
  bool     m_locked;              ///< Conference cannot accept new participants.

  struct URI
  {
    PString m_uri;                ///< URI for access/service in conference
    PString m_displayText;        ///< Human readable form of resource
    PString m_purpose;            /**< Purpose of URI, e.g. "participation" indicates
                                       a URI to join conference, "streaming" indicates
                                       a "listening only" connection */
  };
  typedef std::vector<URI> URIs;

  URIs m_accessURI;               ///< All URIs that can access the conference
  URIs m_serviceURI;              /**< All URIs that describe auxilliary services for
                                       conference, e.g. purpose could be "web-page" or
                                       "recording". */

  unsigned m_maxUsers;            ///< Maximum users that can join the conference

  struct User
  {
    PString    m_uri;             ///< URI that the user used to access this conference
    PString    m_displayText;     ///< Human readable form of users connection
    PStringSet m_roles;           ///< Role for user, e.g. "participant".
  };
  typedef std::vector<User> Users;
  Users m_users;

#if P_EXPAT
  /** Optional XML as per RFC 4575 "application/conference-info+xml".
      If this member is set, then this is conferted to astring and sent in SIP
      NOTIFY commands. If it is not set, then the XML is generated from the
      other information, in this way if extended XML fields are required it
      can be added by an application.
    */
  PXML m_xml;
#endif
};

typedef std::list<OpalConferenceState> OpalConferenceStates;


/**This class is the central manager for OPAL.
   The OpalManager embodies the root of the tree of objects that constitute an
   OPAL system. It contains all of the endpoints that make up the system. Other
   entities such as media streams etc are in turn contained in these objects.
   It is expected that an application would only ever have one instance of
   this class, and also descend from it to override call back functions.

   The manager is the eventual destination for call back indications from
   various other objects. It is possible, for instance, to get an indication
   of a completed call by creating a descendant of the OpalCall and overriding
   the OnClearedCall() virtual. However, this could quite unwieldy for all of
   the possible classes, so the default behaviour is to call the equivalent
   function on the OpalManager. This allows most applications to only have to
   create a descendant of the OpalManager and override virtual functions there
   to get all the indications it needs.
 */
class OpalManager : public PObject
{
    PCLASSINFO(OpalManager, PObject);
  public:
  /**@name Construction */
  //@{
    /**Create a new manager.
     */
    OpalManager();

    /**Destroy the manager.
       This will clear all calls, then delete all endpoints still attached to
       the manager.
     */
    ~OpalManager();
  //@}

  /**@name Endpoint management */
  //@{
    /**Attach a new endpoint to the manager.
       This is an internal function called by the OpalEndPoint constructor.

       Note that usually the endpoint is automatically "owned" by the manager.
       They should not be deleted directly. The DetachEndPoint() command
       should be used to do this.
      */
    void AttachEndPoint(
      OpalEndPoint * endpoint,    ///< EndPoint to add to the manager
      const PString & prefix = PString::Empty()  ///< Prefix to use, if empty uses endpoint->GetPrefixName()
    );

    /**Remove an endpoint from the manager.
       This will delete the endpoint object.
      */
    void DetachEndPoint(
      const PString & prefix
    );
    void DetachEndPoint(
      OpalEndPoint * endpoint
    );

    /**Find an endpoint instance that is using the specified prefix.
      */
    OpalEndPoint * FindEndPoint(
      const PString & prefix  ///< Prefix string for endpoint URL
    ) const;

    /**Find an endpoint instance that is using the specified prefix.
      */
    template <class T> T * FindEndPointAs(
      const PString & prefix  ///< Prefix string for endpoint URL
    ) const { return dynamic_cast<T *>(FindEndPoint(prefix)); }

    /**Get the endpoints attached to this manager.
      */
    PList<OpalEndPoint> GetEndPoints() const;

    /**Get all the prefixes for endpoints attached to this manager.
      */
    PStringList GetPrefixNames(
      const OpalEndPoint * endpoint = NULL  ///< Optional ep to get prefixes for
    ) const;

    /**Build a list of network accessible URIs given a user name.
       This typically gets URI's like sip:user@interface, h323:user@interface
       etc, for each listener of each endpoint.
      */
    virtual PStringList GetNetworkURIs(
      const PString & name
    ) const;

    /**Shut down all of the endpoints, clearing all calls.
       This is synchonous and will wait till everything is shut down.
       This will also assure no new calls come in whilein the process of
       shutting down.
      */
    void ShutDownEndpoints();
  //@}


#if OPAL_HAS_PRESENCE
  /**@name Presence management */
  //@{
    /**Add a presentity.
       If the presentity is already present, a new one is not added, and the
       existing instance is returned.

       Returns a Read/Write locked pointer to presentity.
      */
    virtual PSafePtr<OpalPresentity> AddPresentity(
      const PString & presentity  ///< Presentity URI
    );

    /**Get a presentity.
      */
    virtual PSafePtr<OpalPresentity> GetPresentity(
      const PString & presentity,         ///< Presentity URI
      PSafetyMode mode = PSafeReference   ///< Safety mode for presentity
    );

    /**Get all presentities.
      */
    virtual PStringList GetPresentities() const;

    /**Remove a presentity.
      */
    virtual bool RemovePresentity(
      const PString & presentity  ///< Presentity URI
    );
  //@}
#endif // OPAL_HAS_PRESENCE


  /**@name Call management */
  //@{
    /**Set up a call between two parties.
       This is used to initiate a call. Incoming calls are "answered" using a
       different mechanism.

       The A party and B party strings indicate the protocol and address of
       the party to call in the style of a URL. The A party is the initiator
       of the call and the B party is the remote system being called. See the
       MakeConnection() function for more details on the format of these
       strings.

       The token returned is a unique identifier for the call that allows an
       application to gain access to the call at later time. This is necesary
       as any pointer being returned could become invalid (due to being
       deleted) at any time due to the multithreaded nature of the OPAL
       system. 
     */
    virtual PSafePtr<OpalCall> SetUpCall(
      const PString & partyA,       ///<  The A party of call
      const PString & partyB,       ///<  The B party of call
      void * userData = NULL,       ///<  user data passed to Call and Connection
      unsigned options = 0,         ///<  options passed to connection
      OpalConnection::StringOptions * stringOptions = NULL ///< Options to pass to connection
    );
    virtual PBoolean SetUpCall(
      const PString & partyA,       ///<  The A party of call
      const PString & partyB,       ///<  The B party of call
      PString & token,              ///<  Token for call
      void * userData = NULL,       ///<  user data passed to Call and Connection
      unsigned options = 0,         ///<  options passed to connection
      OpalConnection::StringOptions * stringOptions = NULL ///< Options to pass to connection
    );

#if OPAL_HAS_MIXER
    /**Set up a conference between the parties.
       The \p call is added to a conference specified by \p mixerURI.

       If \p mixerURI is NULL or empty, then a suitable default is created
       based on the OpalMixerEndPoint contained in the manager.

       If the conference node does not exist then it is created.

       If the mixer node (conference) is empty then the \p localParty is also
       added to the conference.

       If \p localURI is NULL then a suitable default (e.g. "pc:*") is used,
       hoeever, if it an empty string, then no local connection is created.
      */
    virtual bool SetUpConference(
      OpalCall & call,
      const char * mixerURI = NULL,
      const char * localURI = NULL
    );
#endif // OPAL_HAS_MIXER

    /**Determine if a call is active.
       Return true if there is an active call with the specified token. Note
       that the call could clear any time (even milliseconds) after this
       function returns true.
      */
    virtual PBoolean HasCall(
      const PString & token  ///<  Token for identifying call
    ) { return m_activeCalls.Find(token, PSafeReference) != NULL; }

    /**Return the number of active calls.
      */
    PINDEX GetCallCount() const { return m_activeCalls.GetSize(); }

    /**Get all tokens for active calls.
      */
     PArray<PString> GetAllCalls() const { return m_activeCalls.GetKeys(); }

    /**Find a call with the specified token.
       This searches the manager database for the call that contains the token
       as provided by functions such as SetUpCall().

       Note the caller of this function MUST call the OpalCall::Unlock()
       function if this function returns a non-NULL pointer. If it does not
       then a deadlock can occur.
      */
    PSafePtr<OpalCall> FindCallWithLock(
      const PString & token,  ///<  Token to identify connection
      PSafetyMode mode = PSafeReadWrite ///< Lock mode
    ) const { return m_activeCalls.Find(token, mode); }

    /**A call back function whenever a call is being terminated locally.
       An application may use this function to auto-answer an incoming call
       from the a network. Or indicate to a user interface that an incoming
       call has to be answered asynchronously.

       This called from the OpalLocalEndPoint::OnIncomingCall() function.

       The default behaviour does nothing and returns true.

        @return false to refuse the call.
      */
    virtual bool OnLocalIncomingCall(
      OpalLocalConnection & connection ///< Connection for incoming call
    );

    /**A call back function whenever a call is being initated locally.
       An application may use this function to indicate that a call is in progress.

       This called from the OpalLocalEndPoint::OnOutgoingCall() function.

       The default behaviour does nothing and returns true.
      */
    virtual bool OnLocalOutgoingCall(
      const OpalLocalConnection & connection ///< Connection for outgoing call
    );

    /**A call back function whenever a call is completed.
       In telephony terminology a completed call is one where there is an
       established link between two parties.

       This called from the OpalCall::OnEstablished() function.

       The default behaviour does nothing.
      */
    virtual void OnEstablishedCall(
      OpalCall & call   ///<  Call that was completed
    );

    /**Determine if a call is established.
       Return true if there is an active call with the specified token and
       that call has at least two parties with media flowing between them.
       Note that the call could clear any time (even milliseconds) after this
       function returns true.
      */
    virtual PBoolean IsCallEstablished(
      const PString & token  ///<  Token for identifying call
    );

    /**Clear a call.
       This finds the call by using the token then calls the OpalCall::Clear()
       function on it. All connections are released, and the connections and
       call are disposed of. Note that this function returns quickly and the
       disposal happens at some later time in a background thread. It is safe
       to call this function from anywhere.

       If \p sync is not NULL then it is signalled when the calls are cleared.
      */
    virtual PBoolean ClearCall(
      const PString & token,    ///<  Token for identifying connection
      OpalConnection::CallEndReason reason = OpalConnection::EndedByLocalUser, ///<  Reason for call clearing
      PSyncPoint * sync = NULL  ///<  Sync point to wait on.
    );

    /**Clear a call.
       This finds the call by using the token then calls the OpalCall::Clear()
       function on it. All connections are released, and the connections and
       caller disposed of. Note that this function waits until the call has
       been cleared and all responses timeouts etc completed. Care must be
       used as to when it is called as deadlocks may result.
      */
    virtual PBoolean ClearCallSynchronous(
      const PString & token,    ///<  Token for identifying connection
      OpalConnection::CallEndReason reason = OpalConnection::EndedByLocalUser ///<  Reason for call clearing
    );

    /**Clear all current calls.
       This effectively executes OpalCall::Clear() on every call that the
       manager has active.
       This function can not be called from several threads at the same time.
      */
    virtual void ClearAllCalls(
      OpalConnection::CallEndReason reason = OpalConnection::EndedByLocalUser, ///<  Reason for call clearing
      PBoolean wait = true   ///<  Flag to wait for calls to e cleared.
    );

    /**A call back function whenever a call is cleared.
       A call is cleared whenever there is no longer any connections attached
       to it. This function is called just before the call is deleted.
       However, it may be used to display information on the call after
       completion, eg the call parties and duration.

       Note that there is not a one to one relationship with the
       OnEstablishedCall() function. This function may be called without that
       function being called. For example if MakeConnection() was used but
       the call never completed.

       The default behaviour removes the call from the activeCalls dictionary.
      */
    virtual void OnClearedCall(
      OpalCall & call   ///<  Connection that was established
    );

    /**Create a call object.
       This function allows an application to have the system create
       desccendants of the OpalCall class instead of instances of that class
       directly. The application can thus override call backs or add extra
       information that it wishes to maintain on a call by call basis.

       The default behavious returns an instance of OpalCall.
      */
    virtual OpalCall * CreateCall(
      void * userData            ///<  user data passed to SetUpCall
    );
    OpalCall * InternalCreateCall(void * userData = NULL);

    /**Destroy a call object.
       This gets called from background thread that garbage collects all calls
       and connections. If an application has object lifetime issues with the
       threading, it can override this function and take responsibility for
       deleting the object at some later time.

       The default behaviour simply calls "delete call".
      */
    virtual void DestroyCall(
      OpalCall * call
    );

    /**Get next unique token ID for calls or connections.
       This is an internal function called by the OpalCall and other
       constructors.
      */
    virtual PString GetNextToken(char prefix);
  //@}


  /**@name Connection internal routing */
  //@{
    /**Entry in the route table.
       See AddRouteEntry() for more details.
      */
    class RouteEntry : public PObject
    {
        PCLASSINFO(RouteEntry, PObject);
      public:
        RouteEntry(const PString & partyA, const PString & partyB, const PString & dest);
        RouteEntry(const PString & spec);

        PObject * Clone() const { return new RouteEntry(*this); }
        void PrintOn(ostream & strm) const;

        bool IsValid() const;
        bool IsMatch(const PString & search) const;

        const PString & GetPartyA() const { return m_partyA; }
        const PString & GetPartyB() const { return m_partyB; }
        const PString & GetDestination() const { return m_destination; }

      protected:
        PString            m_partyA;      ///< URL of caller
        PString            m_partyB;      ///< URL caller want to conect to
        PString            m_destination; ///< URL we map above to, with macro substitutions
        PRegularExpression m_regex;       ///< Compiled Regular expression from pattern

        void CompileRegEx();
    };
    PARRAY(RouteTable, RouteEntry);

    /**Add a route entry to the route table.

       The specification string is of the form:

                 pattern '=' destination 
       where:

            pattern      regular expression used to select route

            destination  destination for the call

       The "pattern" string regex is compared against routing strings that are built 
       as follows:
       
                 a_party '\\t' b_party

       where:

            a_party      name associated with a local connection i.e. "pots:vpb:1/2" or
                         "h323:myname@myhost.com". 

            b_party      destination specified by the call, which may be a full URI
                         or a simple digit string

       Note that all "pattern" strings have an implied '^' at the beginning and
       a '$' at the end. This forces the "pattern" to match the entire source string. 
       For convenience, the sub-expression ".*\\t" is inserted immediately after
       any ':' character if no '\\t' is present.

       Route entries are stored and searched in the route table in the order they are added. 
       
       The "destination" string is determines the endpoint used for the outbound
       leg of the route, when a match to the "pattern" is found. It can be a literal string, 
       or can be constructed using various meta-strings that correspond to parts of the source.
       See below for a list of the available meta-strings

       A "destination" starting with the string 'label:' causes the router to restart 
       searching from the beginning of the route table using the new string as the "a_party". 
       Thus, a route table with the folllwing entries:
       
                  "label:speeddial=h323:10.0.1.1" 
                  "pots:26=label:speeddial" 

       will produce the same result as the single entry "pots:26=h323:10.0.1.1".

       If the "destination" parameter is of the form \@filename, then the file
       is read with each line consisting of a pattern=destination route
       specification. 
       
       "destination" strings without an equal sign or beginning with '#' are ignored.

       Pattern Regex Examples:
       -----------------------

       1) A local H.323 endpoint with with name of "myname@myhost.com" that receives a 
          call with a destination h323Id of "boris" would generate:
          
                          "h323:myname@myhost.com\\tboris"

       2) A local SIP endpoint with with name of "fred@nurk.com" that receives a 
          call with a destination of "sip:fred@nurk.com" would generate:
          
                          "sip:fred@nurk.com\\tsip:fred@nurk.com"

       3) Using line 0 of a PhoneJACK handset with a serial # of 01AB3F4 to dial
          the digits 2, 6 and # would generate:

                          "pots:Quicknet:01AB3F4:0\\t26"


       Destination meta-strings:
       -------------------------

       The available meta-strings are:
       
         <da>    Replaced by the "b_party" string. For example
                 "pc:.*\\t.* = sip:<da>" directs calls to the SIP protocol. In
                 this case there is a special condition where if the original
                 destination had a valid protocol, eg h323:fred.com, then
                 the entire string is replaced not just the <da> part.

         <db>    Same as <da>, but without the special condtion.

         <du>    Copy the "user" part of the "b_party" string. This is
                 essentially the component after the : and before the '@', or
                 the whole "b_party" string if these are not present.

         <!du>   The rest of the "b_party" string after the <du> section. The 
                 protocol is still omitted. This is usually the '@' and onward.
                 Note if there is already an '@' in the destination before the
                 <!du> and what is abour to replace it also has an '@' then
                 everything between the @ and the <!du> (inclusive) is deleted,
                 then the substitution is made so a legal URL can result.

         <dn>    Copy all valid consecutive E.164 digits from the "b_party" so
                 pots:0061298765\@vpb:1/2 becomes sip:0061298765@carrier.com

         <dnX>   As above but skip X digits, eg <dn2> skips 2 digits, so
                 pots:00612198765 becomes sip:61298765@carrier.com

         <!dn>   The rest of the "b_party" after the <dn> or <dnX> sections.

         <dn2ip> Translate digits separated by '*' characters to an IP
                 address. e.g. 10*0*1*1 becomes 10.0.1.1, also
                 1234*10*0*1*1 becomes 1234\@10.0.1.1 and
                 1234*10*0*1*1*1722 becomes 1234\@10.0.1.1:1722.

         <cu>    Copy the "user" part of the "a_party" string. This is
                 essentially the component after the : and before the '@', or
                 the whole "b_party" string if these are not present.


       Returns true if an entry was added.
      */
    virtual PBoolean AddRouteEntry(
      const PString & spec  ///<  Specification string to add
    );

    /**Parse a route table specification list for the manager.
       This removes the current routeTable and calls AddRouteEntry for every
       string in the array.

       Returns true if at least one entry was added.
      */
    PBoolean SetRouteTable(
      const PStringArray & specs  ///<  Array of specification strings.
    );

    /**Set a route table for the manager.
       Note that this will make a copy of the table and not maintain a
       reference.
      */
    void SetRouteTable(
      const RouteTable & table  ///<  New table to set for routing
    );

    /**Get the active route table for the manager.
      */
    const RouteTable & GetRouteTable() const { return m_routeTable; }

    /**Route the source address to a destination using the route table.
       The source parameter may be something like pots:vpb:1/2 or
       sip:fred\@nurk.com.

       The destination parameter is a partial URL, it does not include the
       protocol, but may be of the form user\@host, or simply digits.
      */
    virtual PString ApplyRouteTable(
      const PString & source,      ///< Source address, including endpoint protocol
      const PString & destination, ///< Destination address read from source protocol
      PINDEX & entry               ///< Index into table to start search
    );

    /**Route a connection to another connection from an endpoint.

       The default behaviour gets the destination address from the connection
       and translates it into an address by using the routeTable member
       variable and uses MakeConnection() to start the B-party connection.
      */
    virtual bool OnRouteConnection(
      PStringSet & routesTried,     ///< Set of routes already tried
      const PString & a_party,      ///< Source local address
      const PString & b_party,      ///< Destination indicated by source
      OpalCall & call,              ///< Call for new connection
      unsigned options,             ///< Options for new connection (can't use default as overrides will fail)
      OpalConnection::StringOptions * stringOptions ///< Options to pass to connection
    );
  //@}


  /**@name Connection management */
  //@{
    /**Set up a connection to a remote party.
       An appropriate protocol (endpoint) is determined from the party
       parameter. That endpoint is then called to create a connection and that
       connection is attached to the call provided.
       
       If the endpoint is already occupied in a call then the endpoints list
       is further searched for additional endpoints that support the protocol.
       For example multiple pstn endpoints may be present for multiple LID's.
       
       The general form for this party parameter is:

            [proto:][alias@][transport$]address[:port]

       where the various fields will have meanings specific to the endpoint
       type. For example, with H.323 it could be "h323:Fred@site.com" which
       indicates a user Fred at gatekeeper size.com. Whereas for the PSTN
       endpoint it could be "pstn:5551234" which is to call 5551234 on the
       first available PSTN line.

       The default for the proto is the name of the protocol for the first
       endpoint attached to the manager. Other fields default to values on an
       endpoint basis.

       This function usually returns almost immediately with the connection
       continuing to occur in a new background thread.

       If false is returned then the connection could not be established. For
       example if a PSTN endpoint is used and the associated line is engaged
       then it may return immediately. Returning a non-NULL value does not
       mean that the connection will succeed, only that an attempt is being
       made.

       The default behaviour is pure.
     */
    virtual PSafePtr<OpalConnection> MakeConnection(
      OpalCall & call,                   ///<  Owner of connection
      const PString & party,             ///<  Party to call
      void * userData = NULL,            ///<  user data to pass to connections
      unsigned int options = 0,          ///<  options to pass to conneciton
      OpalConnection::StringOptions * stringOptions = NULL ///< Options to pass to connection
    );

    /**Call back for a new connection has been constructed.
       This is called after CreateConnection has returned a new connection.
       It allows an application to make any custom adjustments to the
       connection before it begins to process the protocol. behind it.
      */
    virtual void OnNewConnection(
      OpalConnection & connection   ///< New connection just created
    );

    /**Call back for answering an incoming call.
       This function is used for an application to control the answering of
       incoming calls.

       If true is returned then the connection continues. If false then the
       connection is aborted.

       Note this function should not block for any length of time. If the
       decision to answer the call may take some time eg waiting for a user to
       pick up the phone, then AnswerCallPending or AnswerCallDeferred should
       be returned.

       If an application overrides this function, it should generally call the
       ancestor version to complete calls. Unless the application completely
       takes over that responsibility. Generally, an application would only
       intercept this function if it wishes to do some form of logging. For
       this you can obtain the name of the caller by using the function
       OpalConnection::GetRemotePartyName().

       The default behaviour is to call OnRouteConnection to determine a
       B party for the connection.

       If the call associated with the incoming call already had two parties
       and this connection is a third party for a conference call then
       AnswerCallNow is returned as a B party is not required.
     */
    virtual PBoolean OnIncomingConnection(
      OpalConnection & connection,   ///<  Connection that is calling
      unsigned options,              ///<  options for new connection (can't use default as overrides will fail)
      OpalConnection::StringOptions * stringOptions ///< Options to pass to connection
    );

    /**Call back to optionally modify string options.
       This called when a conenction is about to apply string options for a new
       connection. The application has an opportunity to "tweak" them before
       they are used.
      */
    virtual void OnApplyStringOptions(
      OpalConnection & connection,                  ///< Connection applying options.
      OpalConnection::StringOptions & stringOptions ///< Options being applied.
    );

    /**Call back for remote party is now responsible for completing the call.
       This function is called when the remote system has been contacted and it
       has accepted responsibility for completing, or failing, the call. This
       is distinct from OnAlerting() in that it is not known at this time if
       anything is ringing. This indication may be used to distinguish between
       "transport" level error, in which case another host may be tried, and
       that finalising the call has moved "upstream" and the local system has
       no more to do but await a result.

       If an application overrides this function, it should generally call the
       ancestor version for correct operation.

       The default behaviour calls the OnProceeding() on the connection's
       associated OpalCall object.
     */
    virtual void OnProceeding(
      OpalConnection & connection   ///<  Connection that is proceeeding
    );

    /**Call back for remote party being alerted on outgoing call.
       This function is called after the connection is informed that the
       remote endpoint is "ringing". This function is generally called
       some time after the MakeConnection() function was called.

       If false is returned the connection is aborted.

       If an application overrides this function, it should generally call the
       ancestor version for correct operation. An application would typically
       only intercept this function if it wishes to do some form of logging.
       For this you can obtain the name of the caller by using the function
       OpalConnection::GetRemotePartyName().

       The default behaviour calls the OnAlerting() on the connection's
       associated OpalCall object.
     */
    virtual void OnAlerting(
      OpalConnection & connection,   ///<  Connection that indicates it is alerting
      bool withMedia                 ///<  Indicated media should be started, if possible
    );
    virtual void OnAlerting(OpalConnection & connection); // For backward compatibility

    /**Call back for answering an incoming call.
       This function is called after the connection has been acknowledged
       but before the connection is established

       This gives the application time to wait for some event before
       signalling to the endpoint that the connection is to proceed. For
       example the user pressing an "Answer call" button.

       If AnswerCallDenied is returned the connection is aborted and the
       connetion specific end call PDU is sent. If AnswerCallNow is returned 
       then the connection proceeding, Finally if AnswerCallPending is returned then the
       protocol negotiations are paused until the AnsweringCall() function is
       called.

       The default behaviour simply returns AnswerNow.
     */
    virtual OpalConnection::AnswerCallResponse OnAnswerCall(
      OpalConnection & connection,    ///<  connection that is being answered
       const PString & caller         ///<  caller
    );

    /**A call back function whenever a connection is "connected".
       This indicates that a connection to an endpoint was connected. That
       is the endpoint received acknowledgement via whatever protocol it uses
       that the connection may now start media streams.

       In the context of H.323 this means that the CONNECT pdu has been
       received.

       The default behaviour calls the OnConnected() on the connections
       associated OpalCall object.
      */
    virtual void OnConnected(
      OpalConnection & connection   ///<  Connection that was established
    );

    /**A call back function whenever a connection is "established".
       This indicates that a connection to an endpoint was established. This
       usually occurs after OnConnected() and indicates that the connection
       is both connected and media can flow.

       In the context of H.323 this means that the CONNECT pdu has been
       received and either fast start was in operation or the subsequent Open
       Logical Channels have occurred. For SIP it indicates the INVITE/OK/ACK
       sequence is complete.

       The default behaviour calls the OnEstablished() on the connection's
       associated OpalCall object.
      */
    virtual void OnEstablished(
      OpalConnection & connection   ///<  Connection that was established
    );

    /**A call back function whenever a connection is released.
       This function can do any internal cleaning up and waiting on background
       threads that may be using the connection object.

       Classes that override this function should make sure they call the
       ancestor version for correct operation.

       An application will not typically call this function as it is used by
       the OpalManager during a release of the connection.

       The default behaviour calls OnReleased() on the connection's
       associated OpalCall object. This indicates to the call that the
       connection has been released so it can release the last remaining
       connection and then returns true.
      */
    virtual void OnReleased(
      OpalConnection & connection   ///<  Connection that was established
    );
    
    /**A call back function whenever a connection is "held" or "retrieved".
       This indicates that a connection to an endpoint was held, or
       retrieved, either locally or by the remote endpoint.

       The default behaviour does nothing.
      */
    virtual void OnHold(
      OpalConnection & connection,   ///<  Connection that was held/retrieved
      bool fromRemote,               ///<  Indicates remote has held local connection
      bool onHold                    ///<  Indicates have just been held/retrieved.
    );
    virtual void OnHold(OpalConnection & connection); // For backward compatibility

    /**A call back function whenever a connection is forwarded.

       The default behaviour does nothing.
      */
    virtual PBoolean OnForwarded(
      OpalConnection & connection,  ///<  Connection that was held
      const PString & remoteParty         ///<  The new remote party
    );

    /**A call back function to monitor the progress of a transfer.
       When a transfer operation is initiated, the Transfer() function will
       generally return immediately and the transfer may take some time. This
       call back can give an indication to the application of the progress of
       the transfer.
       the transfer.

       For example in SIP, the OpalCall::Transfer() function will have sent a
       REFER request to the remote party. The remote party sends us NOTIFY
       requests about the progress of the REFER request.

       An application can now make a decision during the transfer operation
       to short circuit the sequence, or let it continue. It can also
       determine if the transfer did not go through, and it should "take back"
       the call. Note no action is required to "take back" the call other than
       indicate to the user that they are back on.

       A return value of false will immediately disconnect the current call.

       The exact format of the \p info parameter is dependent on the protocol
       being used. As a minimum, it will always have a values info["result"]
       and info["party"].

       The info["party"] indicates the part the \p connection is playing in
       the transfer. This will be:
          "A"   party being transferred
          "B"   party initiating the transfer of "A"
          "C"   party "A" is being transferred to

       The info["result"] will be at least one of the following:
          "success"     Transfer completed successfully (party A or B)
          "incoming"    New call was from a transfer (party C)
          "started"     Transfer operation has started (party A)
          "progress"    Transfer is in progress (party B)
          "blind"       Transfer is blind, no further notification (party B)
          "error"       Transfer could not begin (party B)
          "failed"      Transfer started but did not complete (party A or B)

       For SIP, there may be an additional info["state"] containing the NOTIFY
       subscription state, an info["code"] entry containing the 3 digit
       code returned in the NOTIFY body and info["Referred-By"] indicating the
       URI of party B. Other fields may also be present.

       The default behaviour returns false if info["result"] == "success".
      */
    virtual bool OnTransferNotify(
      OpalConnection & connection,  ///< Connection being transferred.
      const PStringToString & info  ///< Information on the transfer
    );
  //@}


  /**@name Media Streams management */
  //@{
    /**Get common media formats.
       This is called by various places to get common media formats for the
       basic connection classes.

       The default behaviour uses the mediaFormatOrder and mediaFormatMask
       member variables to adjust the mediaFormats list.
      */
    virtual OpalMediaFormatList GetCommonMediaFormats(
      bool transportable,  ///< Include transportable media formats
      bool pcmAudio        ///< Include raw PCM audio media formats
    ) const;

    /**Adjust media formats available on a connection.
       This is called by a connection after it has called
       OpalCall::GetMediaFormats() to get all media formats that it can use so
       that an application may remove or reorder the media formats before they
       are used to open media streams.

       The default behaviour uses the mediaFormatOrder and mediaFormatMask
       member variables to adjust the mediaFormats list.
      */
    virtual void AdjustMediaFormats(
      bool local,                         ///<  Media formats a local ones to be presented to remote
      const OpalConnection & connection,  ///<  Connection that is about to use formats
      OpalMediaFormatList & mediaFormats  ///<  Media formats to use
    ) const;

    /// How to handle media between two "network" connections.
    P_DECLARE_TRACED_ENUM(MediaTransferMode,
      MediaTransferBypass,   /**< Media bypasses this host completely. The RTP
                                  addressess of each side is passed to the
                                  other so media goes directly. */
      MediaTransferForward,  /**< Media passed through this host but is not
                                  changed, RTP packets a simply forwareded
                                  to the other side. */
      MediaTransferTranscode /**< Media is passed through this host and if
                                  necessary transcoded between media formats.
                                  Note this can take a lot of CPU. */
    );

    /**Determine how to handle media between two "network" connections.
       Determine if media is to bypass this host when it is possible to do so.
       For example if the two connections are SIP and H.323, they both use RTP
       and the packets can go directly between the remote endpoints.

       An application may override this function in order to conditionally
       enable this feature, or for example if firewall traversal is in play,
       or Lawful Intercept, or any application defined reason.

       The default behaviour returns MediaTransferForward, disallowing
       transcoding and full media bypass.
     */
    virtual MediaTransferMode GetMediaTransferMode(
      const OpalConnection & provider,    ///< Half of call providing media transport addresses
      const OpalConnection & consumer,    ///< Other half of call needing media transport addresses
      const OpalMediaType & mediaType     ///<  Media type for session
    ) const;

    /**Get transports for the media session on the connection.
       This is primarily used by the media bypass feature controlled by the
       OpalManager::GetMediaTransferMode() function. It allows one side of the
       call to get the transport address of the media on the other side, so it
       can pass it on, bypassing the local host.

       It may also be sued by "external" RTP systems where a non network
       connection can redirect media to seom other transport address.

       Default behaviour checks if both connections a "network" and if so uses
       GetMediaTransferMode() to determine if in bypass mode, otherwise returns
       false. Note this default implementation does not fill in the \p
       transports, as that is usually done by derived OpalConnection class. If
       the \p transports is set, then the derived classes to no override it.

       @return true if a transport address is available and may be used to pass
               on to a remote system for direct access.
     */
    virtual bool GetMediaTransportAddresses(
      const OpalConnection & provider,       ///< Half of call providing media transport addresses
      const OpalConnection & consumer,       ///< Other half of call needing media transport addresses
      unsigned sessionId,                    ///< Session identifier
      const OpalMediaType & mediaType,       ///< Media type for session to return information
      OpalTransportAddressArray & transports ///<  Information on media session
    ) const;

    /**Call back when opening a media stream.
       This function is called when a connection has created a new media
       stream according to the logic of its underlying protocol.

       The usual requirement is that media streams are created on all other
       connections participating in the call and all of the media streams are
       attached to an instance of an OpalMediaPatch object that will read from
       one of the media streams passing data to the other media streams.

       The default behaviour achieves the above using the FindMatchingCodecs()
       to determine what (if any) software codecs are required, the
       OpalConnection::CreateMediaStream() function to open streams and the
       CreateMediaPatch() function to create a patch for all of the streams
       and codecs just produced.
      */
    virtual PBoolean OnOpenMediaStream(
      OpalConnection & connection,  ///<  Connection that owns the media stream
      OpalMediaStream & stream    ///<  New media stream being opened
    );

    /**Indicate is a local RTP connection.
       This is called when a new media stream has been created and it has been
       detected that media will be flowing between two RTP sessions within the
       same process. An application could take advantage of this by optimising
       the transfer in some way, rather than the full media path of codecs and
       sockets which might not be necessary.

       Note this is the complement to SetMediaPassThrough() as this function stops
       RTP data from being sent/received, while SetMediaPassThrough() transfers
       RTP data between the two endpoints.

       The default behaviour returns false.

       @return true if the application is going to execute some form of
               bypass, and the media patch threads should not be started.
      */
    virtual bool OnLocalRTP(
      OpalConnection & connection1, ///< First connection
      OpalConnection & connection2, ///< Second connection
      unsigned sessionID,           ///< Session ID of RTP session
      bool opened                   ///< Media streams are opened/closed
    ) const;

    /**Set pass though mode for media.

       Bypass the internal media handling, passing RTP data directly from
       one call/connection to another.

       This can be useful for back to back calls that happen to be the same
       media format and you wish to avoid double decoding and encoding of
       media. Note this scenario is not the same as two OpalConnections within
       the same OpalCall, but two completely independent OpalCall where one
       connection is to be bypassed. For example, two OpalCall instances might
       have two SIPConnection instances and two OpalMixerConnection instances
       connected via a single OpalMixerNode. Now while there are ONLY two
       calls in the node, it is a waste to decode the audio, add to mixer and
       re-encode it again. In practice this is identical to just bypassing the
       mixer node completely, until a third party is added, then we need to
       switch back to normal (non-pass-through) operation.

       Note this is the complement to OnLocalRTP() as this function transfers
       RTP data directly between the two endpoints, while OnLocalRTP() stops
       the RTP data from being sent/received.

       @return true if pass through is started/stopped, false if there was no
               such call/connection/stream, the streams are incompatible formats
               or a conflicting bypass is already in place.
      */
    bool SetMediaPassThrough(
      const PString & token1, ///< First calls token
      const PString & token2, ///< Second calls token
      bool bypass,            ///< Bypass the media
      unsigned sessionID = 0, ///< Session ID of media stream, 0 indicates all
      bool network  = true    ///< Pass through the network connections of the calls only
    );
    static bool SetMediaPassThrough(
      OpalConnection & connection1, ///< First connection
      OpalConnection & connection2, ///< Second connection
      bool bypass,                  ///< Bypass the media
      unsigned sessionID = 0        ///< Session ID of media stream, 0 indicates all
    );

    /**Call back for closed a media stream.

       The default behaviour does nothing.
      */
    virtual void OnClosedMediaStream(
      const OpalMediaStream & stream     ///<  Stream being closed
    );

    /**Call back for a media stream that failed to open.

       The default behaviour does nothing.
      */
    virtual void OnFailedMediaStream(
      OpalConnection & connection,  ///<  Connection that attempted to open media stream
      bool fromRemote,              ///< Flag indicating the attempt to open was from remote
      const PString & reason        ///< Reason for the open fail
    );

#if OPAL_VIDEO
    /**Create a PVideoInputDevice for a source media stream.
      */
    virtual PBoolean CreateVideoInputDevice(
      const OpalConnection & connection,    ///<  Connection needing created video device
      const OpalMediaFormat & mediaFormat,  ///<  Media format for stream
      PVideoInputDevice * & device,         ///<  Created device
      PBoolean & autoDelete                     ///<  Flag for auto delete device
    );

    /**Create an PVideoOutputDevice for a sink media stream or the preview
       display for a source media stream.
      */
    virtual PBoolean CreateVideoOutputDevice(
      const OpalConnection & connection,    ///<  Connection needing created video device
      const OpalMediaFormat & mediaFormat,  ///<  Media format for stream
      PBoolean preview,                         ///<  Flag indicating is a preview output
      PVideoOutputDevice * & device,        ///<  Created device
      PBoolean & autoDelete                     ///<  Flag for auto delete device
    );

    /**Create a PVideoInputDevice for a source media stream.
      */
    virtual bool CreateVideoInputDevice(
      const OpalConnection & connection,    ///<  Connection needing created video device
      const PVideoDevice::OpenArgs & args,  ///< Device to change to
      PVideoInputDevice * & device,         ///<  Created device
      PBoolean & autoDelete                     ///<  Flag for auto delete device
    );

    /**Create an PVideoOutputDevice for a sink media stream or the preview
       display for a source media stream.
      */
    virtual bool CreateVideoOutputDevice(
      const OpalConnection & connection,    ///<  Connection needing created video device
      const PVideoDevice::OpenArgs & args,  ///< Device to change to
      PVideoOutputDevice * & device,        ///<  Created device
      PBoolean & autoDelete                     ///<  Flag for auto delete device
    );
#endif // OPAL_VIDEO

    /**Create a OpalMediaPatch instance.
       This function allows an application to have the system create descendant
       class versions of the OpalMediPatch class. The application could use
       this to modify the default behaviour of a patch.

       The default behaviour returns an instance of OpalMediaPatch.
      */
    virtual OpalMediaPatch * CreateMediaPatch(
      OpalMediaStream & source,         ///<  Source media stream
      PBoolean requiresPatchThread = true  ///< The patch requires a thread
    );

    /**Call back for a media patch thread starting.
       This function is called within the context of the thread associated
       with the media patch.

       The default behaviour does nothing
      */
    virtual void OnStartMediaPatch(
      OpalConnection & connection,  ///< Connection patch is in
      OpalMediaPatch & patch        ///< Media patch being started
    );

    /**Call back when media stream patch thread stops.
      */
    virtual void OnStopMediaPatch(
      OpalConnection & connection,  ///< Connection patch is in
      OpalMediaPatch & patch        ///< Media Patch being stopped
    );

    /**Call back when media stops unexpectedly.
       This allows the application to take some action when a "no media"
       condition is detected. For example clear the call.

       The \p source indicates if the media is in a source OpalMediaStream of
       the conenction, for example on RTP connections (SIP/H.323) true
       indicates incoming media, fals indicates transmitted media.

       The SetNoMediaTimeout() can be used to set the default time for a
       source stream (e.g. received RTP) to call this function.

       Default behaviour releases the connection.

       @Return true if the specific media session is to be aborted.
      */
    virtual bool OnMediaFailed(
      OpalConnection & connection,  ///< Connection session is in
      unsigned sessionId            ///< Session ID of media that stopped.
    );
  //@}


  /**@name User indications */
  //@{
    /**Call back for remote endpoint has sent user input as a string.

       The default behaviour call OpalConnection::SetUserInput() which
       saves the value so the GetUserInput() function can return it.
      */
    virtual void OnUserInputString(
      OpalConnection & connection,  ///<  Connection input has come from
      const PString & value         ///<  String value of indication
    );

    /**Call back for remote enpoint has sent user input as tones.
       If \p duration is zero then this indicates the beginning of the tone.
       If \p duration is greater than zero then it indicates the end of the
       tone output and how long the tone had run.

       Note, there is no guarantee a zero value (start tone) will occur. There
       is also no guarantee this function is called at all, given how the
       remote may send user indications. For simple, "event" based, user
       indications the OnUserInputString() should be used. THis function is
       only for when a more precise representation of the tone, and it's
       duration, is required.

       The default behaviour calls the OpalCall function of the same name.
      */
    virtual void OnUserInputTone(
      OpalConnection & connection,  ///<  Connection input has come from
      char tone,                    ///<  Tone received
      int duration                  ///<  Duration of tone
    );

    /**Read a sequence of user indications from connection with timeouts.
      */
    virtual PString ReadUserInput(
      OpalConnection & connection,        ///<  Connection to read input from
      const char * terminators = "YX#\r\n", ///<  Characters that can terminate input
      unsigned lastDigitTimeout = 4,      ///<  Timeout on last digit in string
      unsigned firstDigitTimeout = 30     ///<  Timeout on receiving any digits
    );
  //@}


#if OPAL_HAS_MIXER
  /**@name Call recording */
  //@{
    /**Start recording a call.
       Current version saves to a WAV file. It may either mix the receive and
       transmit audio stream to a single mono file, or the streams are placed
       into the left and right channels of a stereo WAV file.

       Returns true if the call exists and there is no recording in progress
               for the call.
      */
    virtual PBoolean StartRecording(
      const PString & callToken,  ///< Call token for call to record
      const PFilePath & filename, ///< File into which to record
      const OpalRecordManager::Options & options = false ///< Record mixing options
    );

    /**Indicate if recording is currently active on call.
      */
    virtual bool IsRecording(
      const PString & callToken   ///< Call token for call to check if recording
    );

    /** Stop a recording.
        Returns true if the call does exists, that recording is active is
                not indicated.
      */
    virtual bool StopRecording(
      const PString & callToken   ///< Call token for call to stop recording
    );

  //@}
#endif


#if OPAL_HAS_IM
  /**@name Instant Messaging management */
  //@{
    /**Call back on a changes Instant Messaging context, aka conversation.
       An application can intercept this and set options on the IM context.
     */
    virtual void OnConversation(
      const OpalIMContext::ConversationInfo & info ///< Info for conversation that changed state
    );

    /**Send an Instant Message to a remote party.
       Details of the message must be filled out in the \p message structure.

       Note that message is non-const as this function can be used to initiate
       a conversation, and the created conversation ID is returned in the
       message.m_conversationId member variable.

       This will fail if an OpalIMEndPoint has not been created.
     */
    virtual PBoolean Message(
      OpalIM & message
    );

    ///< Send an Instant Message to a remote party. Backward compatible to old API.
    virtual PBoolean Message(
      const PString & to, 
      const PString & body
    );

    ///< Send an Instant Message to a remote party. Backward compatible to old API.
    virtual PBoolean Message(
      const PURL & to, 
      const PString & type,
      const PString & body,
      PURL & from, 
      PString & conversationId
    );

    /**Called when Instant Message is received.

       The default action is to pass the message on to a suitable
       OpalPresentity function of the same name.
     */
    virtual void OnMessageReceived(
      const OpalIM & message    ///< Message information
    );

    /**Called when Instant Message event is delivered, or not.

       The default action does nothing.
     */
    virtual void OnMessageDisposition(
      const OpalIMContext::DispositionInfo & info    ///< Message disposition information
    );

    /** Called when the remote composition indication changes state.

       The default action does nothing.
      */
    virtual void OnCompositionIndication(
      const OpalIMContext::CompositionInfo & info     ///< New composition state information
    );
  //@}
#endif


  /**@name Other services */
  //@{
    /// Message waiting sub-types
    enum MessageWaitingType { 
      NoMessageWaiting,
      VoiceMessageWaiting, 
      FaxMessageWaiting,
      PagerMessageWaiting,
      MultimediaMessageWaiting,
      TextMessageWaiting,
      NumMessageWaitingTypes
    };

    /**Callback called when Message Waiting Indication (MWI) is received.
       Multiple callbacks may occur with each MessageWaitingType. A \p type
       of NumMessageWaitingTypes indicates the server is unable to distinguish
       the message type.

       The \p extraInfo parameter is generally of the form "a/b" where a and b
       unsigned integers representing new and old message count. However, it
       may be a simple "yes" or "no" if the remote cannot provide a message
       count.
     */
    virtual void OnMWIReceived(
      const PString & party,    ///< Name of party MWI is for
      MessageWaitingType type,  ///< Type of message that is waiting
      const PString & extraInfo ///< Addition information on the MWI
    );

    /**Get conference state information for all nodes.
       This obtains the state of one or more conferences managed by any
       endpoints. If no endpoints do conferencing, then false is returned.

       The \p name parameter may be one of the aliases for the conference, or
       the internal URI for the conference. An empty string indicates all 
       active conferences are to be returned.

       Note that if the \p name does not match an active conference, true is
       still returned, but the states list will be empty.

       The default behaviour returns false indicating this is not a
       conferencing endpoint.
      */
    virtual bool GetConferenceStates(
      OpalConferenceStates & states,           ///< List of conference states
      const PString & name = PString::Empty() ///< Name for specific node, empty string is all
    ) const;

    /**Call back when conferencing state information changes.
       If a conferencing endpoint type detects a change in a conference nodes
       state, as would be returned by GetConferenceStatus() then this function
       will be called on all endpoints in the OpalManager.

       The \p uri parameter is as is the internal URI for the conference.

       Default behaviour does nothing.
      */
    virtual void OnConferenceStatusChanged(
      OpalEndPoint & endpoint,  /// < Endpoint sending state change
      const PString & uri,      ///< Internal URI of conference node that changed
      OpalConferenceState::ChangeType change ///< Change that occurred
    );

    /**Indicate presentation token change.
       The \p request parameter indicates if this is an "after the fact"
       indication has changed, or if the connection may reject the change and
       retain the token it already has.

       Default behaviour returns true.
      */
    virtual bool OnChangedPresentationRole(
      OpalConnection & connection,   ///< COnnection that has had the change
      const PString & newChairURI,   ///< URI for new confernce chair
      bool request                   ///< Indicates change is requested
    );
  //@}


  /**@name Networking and NAT Management */
  //@{
#if OPAL_PTLIB_SSL
    /** Apply the SSL certificates/key for SSL based calls, e.g. sips or h323s
        This function loads the certificates and keys for use by a OpalListener
        or OpalTransport on the \p endpoint parameter. It allows for embedded
        certificates and keys, while the default behaviour loads the
        certificates and keys from files pointed to by member variables.

        Note that a listener must have a cert/key and may have CA directory/list
        for bi-directional authentication. A transport should have the CA
        directory/list set, and if missing then no server authentication is
        performed. Similarly if a transport may have an optional cert/key for
        bi-directional authentication.
      */
    virtual bool ApplySSLCredentials(
      const OpalEndPoint & ep,  ///< Endpoint transport is based on.
      PSSLContext & context,    ///< Context on which to set certificates
      bool create               ///< Create self signed cert/key if required
    ) const;

    /**Get the default CA filenames (';' separated) or dirctory for CA file.
      */
    const PString & GetSSLCertificateAuthorityFiles() const { return m_caFiles; }

    /**Set the default CA filename
      */
    void SetSSLCertificateAuthorityFiles(const PString & files) { m_caFiles = files; }

    /**Get the default local certificate filename
      */
    const PString & GetSSLCertificateFile() const { return m_certificateFile; }

    /**Set the default local certificate filename
      */
    void SetSSLCertificateFile(const PString & file) { m_certificateFile = file; }

    /**Get the default local private key filename
      */
    const PString & GetSSLPrivateKeyFile() const { return m_privateKeyFile; }

    /**Set the default local private key filename
      */
    void SetSSLPrivateKeyFile(const PString & file) { m_privateKeyFile = file; }

    /**Set flag to auto-create a self signed root certificate and private key.
     */
    void SetSSLAutoCreateCertificate(bool yes) { m_autoCreateCertificate = yes; }

    /**Get flag to auto-create a self signed root certificate and private key.
     */
    bool GetSSLAutoCreateCertificate() const { return m_autoCreateCertificate; }
#endif

    /**Determine if the address is "local", ie does not need any address
       translation (fixed or via STUN) to access.

       The default behaviour checks if remoteAddress is a private, non-routable,
       IP, e.g. 10.x.x.x, 127.x.x.x etc, the "any" or "broadcast" IP, or the IP
       of a local interface.
     */
    virtual PBoolean IsLocalAddress(
      const PIPSocket::Address & remoteAddress
    ) const;

    /**Determine if the RTP session needs to accommodate a NAT router.
       For endpoints that do not use STUN or something similar to set up all the
       correct protocol embeddded addresses correctly when a NAT router is between
       the endpoints, it is possible to still accommodate the call, with some
       restrictions. This function determines if the RTP can proceed with special
       NAT allowances.

       The special allowance is that the RTP code will ignore whatever the remote
       indicates in the protocol for the address to send RTP data and wait for
       the first packet to arrive from the remote and will then proceed to send
       all RTP data back to that address AND port.

       The default behaviour checks the values of the physical link
       (localAddr/peerAddr) against the signaling address the remote indicated in
       the protocol, eg H.323 SETUP sourceCallSignalAddress or SIP "To" or
       "Contact" fields, and makes a guess that the remote is behind a NAT router.
     */
    virtual PBoolean IsRTPNATEnabled(
      OpalConnection & connection,            ///< Connection being checked
      const PIPSocket::Address & localAddr,   ///< Local physical address of connection
      const PIPSocket::Address & peerAddr,    ///< Remote physical address of connection
      const PIPSocket::Address & signalAddr,  ///< Remotes signaling address as indicated by protocol of connection
      PBoolean incoming                       ///< Incoming/outgoing connection
    );

    /**Provide address translation hook.
       This will check to see that remoteAddress is NOT a local address by
       using IsLocalAddress() and if not, set localAddress to the
       translationAddress (if valid) which would normally be the router
       address of a NAT system.
     */
    virtual PBoolean TranslateIPAddress(
      PIPSocket::Address & localAddress,
      const PIPSocket::Address & remoteAddress
    );

#if OPAL_PTLIB_NAT
    /** Get all NAT Methods
      */
    PNatMethods & GetNatMethods() const { return *m_natMethods; }

    /**Set the NAT method to use.
      */
    bool SetNATServer(
      const PString & method,
      const PString & server,
      bool active = true,
      unsigned priority = 0, // Zero is no change
      const PString & iface = PString::Empty() // Any interface
    );

    /**Get the current host name and optional port for the NAT server.
      */
    PString GetNATServer(
      const PString & method = PString::Empty()
    ) const;

    // Backward compatibility
    void SetTranslationAddress(const PString & addr) { SetNATServer(PNatMethod_Fixed::MethodName(), addr); }
#if P_STUN
    PNatMethod::NatTypes SetSTUNServer(const PString & addr)
	{ return SetNATServer(PSTUNClient::MethodName(), addr) ? GetNatMethods().GetMethodByName(PSTUNClient::MethodName())->GetNatType() : PNatMethod::UnknownNat; }
#endif // P_STUN
#endif // OPAL_PTLIB_NAT

    /**Get the TCP port number base.
     */
    WORD GetTCPPortBase() const { return m_tcpPorts.GetBase(); }

    /**Get the TCP port number maximum.
     */
    WORD GetTCPPortMax() const { return m_tcpPorts.GetMax(); }

    /**Set the TCP port number base and max.
     */
    void SetTCPPorts(unsigned tcpBase, unsigned tcpMax);

    /**Get the TCP port range to use.
     */
    PIPSocket::PortRange & GetTCPPortRange() { return m_tcpPorts; }
    const PIPSocket::PortRange & GetTCPPortRange() const { return m_tcpPorts; }

    /**Get the UDP port number base.
     */
    WORD GetUDPPortBase() const { return m_udpPorts.GetBase(); }

    /**Get the UDP port number maximum.
     */
    WORD GetUDPPortMax() const { return m_udpPorts.GetMax(); }


    /**Set the UDP port number base and max for RAS channels.
     */
    void SetUDPPorts(unsigned udpBase, unsigned udpMax);

    /**Get the UDP port range to use.
     */
    PIPSocket::PortRange & GetUDPPortRange() { return m_udpPorts; }
    const PIPSocket::PortRange & GetUDPPortRange() const { return m_udpPorts; }

    /**Get the UDP port number base for RTP channels.
     */
    WORD GetRtpIpPortBase() const { return m_rtpIpPorts.GetBase(); }

    /**Get the max UDP port number for RTP channels.
     */
    WORD GetRtpIpPortMax() const { return m_rtpIpPorts.GetMax(); }

    /**Set the UDP port number base and max for RTP channels.
     */
    void SetRtpIpPorts(unsigned udpBase, unsigned udpMax);

    /**Get the UDP port range for RTP channels.
     */
    PIPSocket::PortRange & GetRtpIpPortRange() { return m_rtpIpPorts; }
    const PIPSocket::PortRange & GetRtpIpPortRange() const { return m_rtpIpPorts; }

    /**Get the IP Type Of Service byte for media (eg RTP) channels.
     */
    BYTE GetMediaTypeOfService() const;

    /**Set the IP Type Of Service byte for media (eg RTP) channels.
     */
    void SetMediaTypeOfService(unsigned tos);

    /**Get the IP Type Of Service byte for media (eg RTP) channels.
     */
    BYTE GetMediaTypeOfService(const OpalMediaType & type) const;

    /**Set the IP Type Of Service byte for media (eg RTP) channels.
     */
    void SetMediaTypeOfService(const OpalMediaType & type, unsigned tos);

    /**Get the IP Quality of Service info for media (eg RTP) channels.
     */
    const PIPSocket::QoS & GetMediaQoS(const OpalMediaType & type) const;

    /**Set the IP Quality of Service info for media (eg RTP) channels.
     */
    void SetMediaQoS(const OpalMediaType & type, const PIPSocket::QoS & qos);

    /**Get the maximum transmitted RTP payload size.
       Defaults to maximum safe MTU size (1400 bytes) minus the
       typical size of the IP, UDP an RTP headers.
      */
    PINDEX GetMaxRtpPayloadSize() const { return m_rtpPayloadSizeMax; }

    /**Get the maximum transmitted RTP payload size.
       Defaults to maximum safe MTU size (576 bytes as per RFC879) minus the
       typical size of the IP, UDP an RTP headers.
      */
    void SetMaxRtpPayloadSize(
      PINDEX size,
      bool mtu = false
    ) { m_rtpPayloadSizeMax = size - (mtu ? (20+16+12) : 0); }

    /**Get the maximum received RTP packet size.
       Defaults to 10k.
      */
    PINDEX GetMaxRtpPacketSize() const { return m_rtpPacketSizeMax; }

    /**Get the maximum received RTP packet size.
       Defaults to 10k.
      */
    void SetMaxRtpPacketSize(
      PINDEX size
    ) { m_rtpPacketSizeMax = size; }
  //@}


  /**@name Member variable access */
  //@{
    /**Get the product info for all endpoints.
      */
    const OpalProductInfo & GetProductInfo() const { return m_productInfo; }

    /**Set the product info for all endpoints.
      */
    void SetProductInfo(
      const OpalProductInfo & info, ///< New information
      bool updateAll = true         ///< Update all registered endpoints
    );

    /**Get the default username for all endpoints.
      */
    const PString & GetDefaultUserName() const { return m_defaultUserName; }

    /**Set the default username for all endpoints.
      */
    void SetDefaultUserName(
      const PString & name,   ///< New name
      bool updateAll = true   ///< Update all registered endpoints
    );

    /**Get the default display name for all endpoints.
      */
    const PString & GetDefaultDisplayName() const { return m_defaultDisplayName; }

    /**Set the default display name for all endpoints.
      */
    void SetDefaultDisplayName(
      const PString & name,   ///< New name
      bool updateAll = true   ///< Update all registered endpoints
    );

    /**Set default connection string options.
       Note that if the individual string option is already present for a
       connection, then it is not overridden by an entry here.
      */
    void SetDefaultConnectionOptions(
      const OpalConnection::StringOptions & stringOptions
    ) { m_defaultConnectionOptions = stringOptions; }

#if OPAL_VIDEO

    //
    // these functions are deprecated and used only for backwards compatibility
    // applications should use OpalConnection::GetAutoStart() to check whether
    // a specific media type can be auto-started
    //

    /**See if should auto-start receive video channels on connection.
     */
    bool CanAutoStartReceiveVideo() const { return (OpalMediaType::Video().GetAutoStart()&OpalMediaType::Receive) != 0; }

    /**Set if should auto-start receive video channels on connection.
     */
    void SetAutoStartReceiveVideo(bool can) { OpalMediaType::Video()->SetAutoStart(OpalMediaType::Receive, can); }

    /**See if should auto-start transmit video channels on connection.
     */
    bool CanAutoStartTransmitVideo() const { return (OpalMediaType::Video().GetAutoStart()&OpalMediaType::Transmit) != 0; }

    /**Set if should auto-start transmit video channels on connection.
     */
    void SetAutoStartTransmitVideo(bool can) { OpalMediaType::Video()->SetAutoStart(OpalMediaType::Transmit, can); }

#endif

    /**Get the default jitter parameters.
     */
    const OpalJitterBuffer::Params & GetJitterParameters() const { return m_jitterParams; }

    /**Set the default jitter parameters.
     */
    void SetJitterParameters(const OpalJitterBuffer::Params & params) { m_jitterParams = params; }

    /**Get the default maximum audio jitter delay parameter.
       Defaults to 50ms
     */
    unsigned GetMinAudioJitterDelay() const { return m_jitterParams.m_minJitterDelay; }

    /**Get the default maximum audio jitter delay parameter.
       Defaults to 250ms.
     */
    unsigned GetMaxAudioJitterDelay() const { return m_jitterParams.m_maxJitterDelay; }

    /**Set the maximum audio jitter delay parameter.

       If minDelay is set to zero then both the minimum and maximum will be
       set to zero which will disable the jitter buffer entirely.

       If maxDelay is zero, or just less that minDelay, then the maximum
       jitter is set to the minimum and this disables the adaptive jitter, a
       fixed value is used.
     */
    void SetAudioJitterDelay(
      unsigned minDelay,   ///<  New minimum jitter buffer delay in milliseconds
      unsigned maxDelay    ///<  New maximum jitter buffer delay in milliseconds
    );

    /**Get the default media format order.
     */
    const PStringArray & GetMediaFormatOrder() const { return m_mediaFormatOrder; }

    /**Set the default media format order.
     */
    void SetMediaFormatOrder(
      const PStringArray & order   ///< New order
    );

    /**Get the default media format mask.
       The is the default list of media format names to be removed from media
       format lists bfeore use by a connection.
       See OpalMediaFormatList::Remove() for more information.
     */
    const PStringArray & GetMediaFormatMask() const { return m_mediaFormatMask; }

    /**Set the default media format mask.
       The is the default list of media format names to be removed from media
       format lists bfeore use by a connection.
       See OpalMediaFormatList::Remove() for more information.
     */
    void SetMediaFormatMask(
      const PStringArray & mask   //< New mask
    );

    /**Set the default parameters for the silence detector.
     */
    virtual void SetSilenceDetectParams(
      const OpalSilenceDetector::Params & params
    ) { m_silenceDetectParams = params; }

    /**Get the default parameters for the silence detector.
     */
    const OpalSilenceDetector::Params & GetSilenceDetectParams() const { return m_silenceDetectParams; }
    
#if OPAL_AEC
    /**Set the default parameters for the echo cancelation.
     */
    virtual void SetEchoCancelParams(
      const OpalEchoCanceler::Params & params
    ) { m_echoCancelParams = params; }

    /**Get the default parameters for the silence detector.
     */
    const OpalEchoCanceler::Params & GetEchoCancelParams() const { return m_echoCancelParams; }
#endif

#if OPAL_VIDEO

    /**Set the parameters for the video device to be used for input.
       If the name is not suitable for use with the PVideoInputDevice class
       then the function will return false and not change the device.
     */
    virtual bool SetVideoInputDevice(
      const PVideoDevice::OpenArgs & deviceArgs, ///<  Full description of device
      OpalVideoFormat::ContentRole role = OpalVideoFormat::eNoRole  ///< Role for video stream to set
    );

    /**Get the parameters for the video device to be used for input.
     */
    const PVideoDevice::OpenArgs & GetVideoInputDevice(
      OpalVideoFormat::ContentRole role = OpalVideoFormat::eNoRole  ///< Role for video stream to get
    ) const { return m_videoInputDevice[role]; }

    /**Set the parameters for the video device to be used to preview input.
       If the name is not suitable for use with the PVideoOutputDevice class
       then the function will return false and not change the device.

       This defaults to the value of the PVideoInputDevice::GetOutputDeviceNames()
       function.
     */
    virtual PBoolean SetVideoPreviewDevice(
      const PVideoDevice::OpenArgs & deviceArgs, ///<  Full description of device
      OpalVideoFormat::ContentRole role = OpalVideoFormat::eNoRole  ///< Role for video stream to set
    );

    /**Get the parameters for the video device to be used for input.
       This defaults to the value of the PSoundChannel::GetInputDeviceNames()[0].
     */
    const PVideoDevice::OpenArgs & GetVideoPreviewDevice(
      OpalVideoFormat::ContentRole role = OpalVideoFormat::eNoRole  ///< Role for video stream to get
    ) const { return m_videoPreviewDevice[role]; }

    /**Set the parameters for the video device to be used for output.
       If the name is not suitable for use with the PVideoOutputDevice class
       then the function will return false and not change the device.

       This defaults to the value of the PVideoInputDevice::GetOutputDeviceNames()
       function.
     */
    virtual PBoolean SetVideoOutputDevice(
      const PVideoDevice::OpenArgs & deviceArgs, ///<  Full description of device
      OpalVideoFormat::ContentRole role = OpalVideoFormat::eNoRole  ///< Role for video stream to set
    );

    /**Get the parameters for the video device to be used for input.
       This defaults to the value of the PSoundChannel::GetOutputDeviceNames()[0].
     */
    const PVideoDevice::OpenArgs & GetVideoOutputDevice(
      OpalVideoFormat::ContentRole role = OpalVideoFormat::eNoRole  ///< Role for video stream to get
    ) const { return m_videoOutputDevice[role]; }

#endif

    PBoolean DetectInBandDTMFDisabled() const
      { return m_disableDetectInBandDTMF; }

    /**Set the default H.245 tunneling mode.
      */
    void DisableDetectInBandDTMF(
      PBoolean mode ///<  New default mode
    ) { m_disableDetectInBandDTMF = mode; } 

    /**Get the amount of time with no media that will cause a call to clear
     */
    const PTimeInterval & GetNoMediaTimeout() const { return m_noMediaTimeout; }

    /**Set the amount of time with no media that will cause a call to clear
     */
    void SetNoMediaTimeout(
      const PTimeInterval & newInterval  ///<  New timeout for media
    ) { m_noMediaTimeout = newInterval; }

    /**Get the amount of time with tx media errors (ICMP) that will cause a call to clear
     */
    const PTimeInterval & GetTxMediaTimeout() const { return m_txMediaTimeout; }

    /**Set the amount of time with tx media errors (ICMP) that will cause a call to clear
     */
    void SetTxMediaTimeout(
      const PTimeInterval & newInterval  ///<  New timeout for media
    ) { m_txMediaTimeout = newInterval; }

    /**Get the amount of time to wait on signaling channel
     */
    const PTimeInterval & GetSignalingTimeout() const { return m_signalingTimeout; }

    /**Set the amount of time to wait on signaling channel
     */
    void SetSignalingTimeout(
      const PTimeInterval & newInterval  ///<  New timeout for signaling
    ) { m_signalingTimeout = newInterval; }

    /**Get the amount of time a transport can be idle before it is closed
     */
    const PTimeInterval & GetTransportIdleTime() const { return m_transportIdleTime; }

    /**Set the amount of time a transport can be idle before it is closed
     */
    void SetTransportIdleTime(
      const PTimeInterval & newInterval  ///<  New timeout
    ) { m_transportIdleTime = newInterval; }

    /**Get the amount of time between "keep-alive" packets to maintain NAT pin-hole.
    */
    const PTimeInterval & GetNatKeepAliveTime() const { return m_natKeepAliveTime; }

    /**Set the amount of time between "keep-alive" packets to maintain NAT pin-hole.
    */
    void SetNatKeepAliveTime(
      const PTimeInterval & newInterval  ///<  New timeout
    ) { m_natKeepAliveTime = newInterval; }

#if OPAL_ICE
    /**Get the amount of time to wait for ICE/STUN packets.
    */
    const PTimeInterval & GetICETimeout() const { return m_iceTimeout; }

    /**Set the amount of time to wait for ICE/STUN packets.
    */
    void SetICETimeout(
      const PTimeInterval & newInterval  ///<  New timeout
    ) { m_iceTimeout = newInterval; }
#endif // OPAL_ICE

   /**Get the amount of time before an RTP receive SSRC is stale and removed.
    */
    const PTimeInterval & GetStaleReceiverTimeout() const { return m_staleReceiverTimeout; }

    /**Set the amount of time before an RTP receive SSRC is stale and removed.
    */
    void SetStaleReceiverTimeout(
      const PTimeInterval & newInterval  ///<  New timeout
    ) { m_staleReceiverTimeout = newInterval; }

#if OPAL_SRTP
    /**Get the amount of time to wait for DTLS handshake.
    */
    const PTimeInterval & GetDTLSTimeout() const { return m_dtlsTimeout; }

    /**Set the amount of time to wait for DTLS handshake.
    */
    void SetDTLSTimeout(
      const PTimeInterval & newInterval  ///<  New timeout
    ) { m_dtlsTimeout = newInterval; }
#endif // OPAL_SRTP

    /**Get the default ILS server to use for user lookup.
      */
    const PString & GetDefaultILSServer() const { return m_ilsServer; }

    /**Set the default ILS server to use for user lookup.
      */
    void SetDefaultILSServer(
      const PString & server
    ) { m_ilsServer = server; }

#if OPAL_SCRIPT
    /**Get the script interpreter interface for application.
       The script can contain functions which OPAL will call, and can
       call some functions within OPAL to get information or execute
       desired behaviour.

       The script can typically also call other sub-system, for example with
       Lua, the "require" keyword can be used to load extra modules such as
       sockets or SQL integration, though explanation of it's use is outside
       of the scope of OPAL documentation.

       The table <i>OpalCall</i> is always available and is an array of the active
       calls indeaxed by the call token. Each call has further tables for each
       connection in the call indexed by connection token.

       The script can contain the following functions, which OPAL will call:
          OnNewCall(token)
          OnDestroyCall(token)
          OnNewConnection(callToken, connectionToken)
          OnDestroyConnection(callToken, connectionToken)
          OnIncoming(callToken, connectionToken, remoteParty, localParty, destination)
            optional return value is an adjusted destination URI.
          OnProceeding(callToken, connectionToken)
          OnAlerting(callToken, connectionToken)
          OnConnected(callToken, connectionToken)
          OnEstablished(callToken)
          OnStartMedia(callToken, mediaId)
          OnStopMedia(callToken, mediaId)
          OnShutdown()

       The script may call the following functions within OPAL:
          PTRACE(level, arg [, arg [, ...]])
          OpalCall[token].Clear([endedByCode [, wait] ])
          OpalCall[callToken][conToken].Release([endedbyCode])
          OpalCall[callToken][conToken].SetOption(key, value [, key, value])
          OpalCall[callToken][conToken].GetLocalPartyURL()
          OpalCall[callToken][conToken].GetRemotePartyURL()
          OpalCall[callToken][conToken].GetCalledPartyURL()
          OpalCall[callToken][conToken].GetRedirectingParty()

        Some additional table fields:
          OpalCall[callToken][conToken].callToken
          OpalCall[callToken][conToken].connectionToken
          OpalCall[callToken][conToken].prefix
          OpalCall[callToken][conToken].originating
      */
    PScriptLanguage * GetScript() const { return m_script; }

    /**Set script for application.
      */
    bool RunScript(
      const PString & script,
      const char * language = "Lua"
    );
#endif // OPAL_SCRIPT
  //@}

    // needs to be public for gcc 3.4
    void GarbageCollection();

    // Decoupled event to avoid deadlocks, especially from patch threads
    void QueueDecoupledEvent(PSafeWork * work, const char * group = NULL) { m_decoupledEventPool.AddWork(work, group); }

    typedef std::map<OpalMediaType, PIPSocket::QoS> MediaQoSMap;

  protected:
    // Configuration variables
    OpalProductInfo m_productInfo;

    PString       m_defaultUserName;
    PString       m_defaultDisplayName;

    mutable MediaQoSMap m_mediaQoS;

    OpalConnection::StringOptions m_defaultConnectionOptions;

    PINDEX        m_rtpPayloadSizeMax;
    PINDEX        m_rtpPacketSizeMax;
    OpalJitterBuffer::Params m_jitterParams;
    PStringArray  m_mediaFormatOrder;
    PStringArray  m_mediaFormatMask;
    bool          m_disableDetectInBandDTMF;
    PTimeInterval m_noMediaTimeout;
    PTimeInterval m_txMediaTimeout;
    PTimeInterval m_signalingTimeout;
    PTimeInterval m_transportIdleTime;
    PTimeInterval m_natKeepAliveTime;
#if OPAL_ICE
    PTimeInterval m_iceTimeout;
#endif
    PTimeInterval m_staleReceiverTimeout;
#if OPAL_SRTP
    PTimeInterval m_dtlsTimeout;
#endif
    PString       m_ilsServer;

    OpalSilenceDetector::Params m_silenceDetectParams;
#if OPAL_AEC
    OpalEchoCanceler::Params m_echoCancelParams;
#endif

#if OPAL_VIDEO
    PVideoDevice::OpenArgs m_videoInputDevice[OpalVideoFormat::NumContentRole];
    PVideoDevice::OpenArgs m_videoPreviewDevice[OpalVideoFormat::NumContentRole];
    PVideoDevice::OpenArgs m_videoOutputDevice[OpalVideoFormat::NumContentRole];
#endif

    PIPSocket::PortRange m_tcpPorts, m_udpPorts, m_rtpIpPorts;
    
#if OPAL_PTLIB_SSL
    PString   m_caFiles;
    PFilePath m_certificateFile;
    PFilePath m_privateKeyFile;
    bool      m_autoCreateCertificate;
#endif

#if OPAL_PTLIB_NAT
    PNatMethods * m_natMethods;
    PDECLARE_InterfaceNotifier(OpalManager, OnInterfaceChange);
    PInterfaceMonitor::Notifier m_onInterfaceChange;
#endif

    RouteTable m_routeTable;
    PDECLARE_MUTEX(m_routeMutex);

    // Dynamic variables
    PDECLARE_READ_WRITE_MUTEX(m_endpointsMutex);
    PList<OpalEndPoint> m_endpointList;
    std::map<PString, OpalEndPoint *> m_endpointMap;

    atomic<unsigned> lastCallTokenID;

    class CallDict : public PSafeDictionary<PString, OpalCall>
    {
      public:
        CallDict(OpalManager & mgr) : manager(mgr) { }
        virtual void DeleteObject(PObject * object) const;
        OpalManager & manager;
    } m_activeCalls;

#if OPAL_HAS_PRESENCE
    PSafeDictionary<PString, OpalPresentity> m_presentities;
#endif // OPAL_HAS_PRESENCE

    atomic<PINDEX> m_clearingAllCallsCount;
    PDECLARE_MUTEX(m_clearingAllCallsMutex);
    PSyncPoint     m_allCallsCleared;
    void InternalClearAllCalls(OpalConnection::CallEndReason reason, bool wait, bool first);

    PThread    * m_garbageCollector;
    PSyncPoint   m_garbageCollectExit;
    PTime        m_garbageCollectChangeTime;
    PDECLARE_NOTIFIER(PThread, OpalManager, GarbageMain);

    friend OpalCall::OpalCall(OpalManager & mgr);
    friend void OpalCall::InternalOnClear();

    PSafeThreadPool m_decoupledEventPool;

#if OPAL_SCRIPT
    PScriptLanguage * m_script;
#endif

  private:
    P_REMOVE_VIRTUAL(OpalCall *,CreateCall(), 0);
    P_REMOVE_VIRTUAL(PBoolean, OnIncomingConnection(OpalConnection &, unsigned), false);
    P_REMOVE_VIRTUAL(PBoolean, OnIncomingConnection(OpalConnection &), false);
    P_REMOVE_VIRTUAL(PBoolean, OnStartMediaPatch(const OpalMediaPatch &), false);
    P_REMOVE_VIRTUAL_VOID(AdjustMediaFormats(const OpalConnection &, OpalMediaFormatList &) const);
    P_REMOVE_VIRTUAL_VOID(OnMessageReceived(const PURL&,const PString&,const PURL&,const PString&,const PString&,const PString&));
    P_REMOVE_VIRTUAL_VOID(OnRTPStatistics(const OpalConnection &, const OpalRTPSession &));
    P_REMOVE_VIRTUAL(PBoolean, IsMediaBypassPossible(const OpalConnection &,const OpalConnection &,unsigned) const, false);
#if OPAL_PTLIB_NAT
    P_REMOVE_VIRTUAL(PNatMethod *, GetNatMethod(const PIPSocket::Address &) const, NULL);
#endif
    P_REMOVE_VIRTUAL(bool,OnLocalIncomingCall(OpalCall &),false);
    P_REMOVE_VIRTUAL(bool,OnLocalOutgoingCall(OpalCall &),false);
    P_REMOVE_VIRTUAL(bool,OnMediaFailed(OpalConnection &,unsigned,bool),false);
};


void OpalGetVersion(PProcess::VersionInfo version);
PString OpalGetVersion();


#endif // OPAL_OPAL_MANAGER_H


// End of File ///////////////////////////////////////////////////////////////

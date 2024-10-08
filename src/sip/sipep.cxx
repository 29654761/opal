/*
 * sipep.cxx
 *
 * Session Initiation Protocol endpoint.
 *
 * Open Phone Abstraction Library (OPAL)
 *
 * Copyright (c) 2000 Equivalence Pty. Ltd.
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
 *
 */

#include <ptlib.h>
#include <opal_config.h>

#if OPAL_SIP

#ifdef __GNUC__
#pragma implementation "sipep.h"
#endif

#include <sip/sipep.h>

#include <ptclib/enum.h>
#include <sdp/sdp.h>
#include <sip/sippres.h>
#include <im/sipim.h>
#include <opal.h>


class SIP_PDU_Work : public SIPWorkItem
{
  public:
    SIP_PDU_Work(SIPEndPoint & ep, const PString & token, SIP_PDU * pdu);
    virtual ~SIP_PDU_Work();

    virtual void Work();

    SIP_PDU * m_pdu;
};


#define PTraceModule() "SIP"
#define new PNEW


static BYTE DefaultKeepAliveData[] = { '\r', '\n', '\r', '\n' };


////////////////////////////////////////////////////////////////////////////

SIPEndPoint::SIPEndPoint(OpalManager & mgr, unsigned maxThreads)
  : OpalSDPEndPoint(mgr, OPAL_PREFIX_SIP, IsNetworkEndPoint | SupportsE164)
  , m_defaultPrackMode(SIPConnection::e_prackSupported)
  , m_maxPacketSizeUDP(1300)         // As per RFC 3261 section 18.1.1
  , m_maxRetries(10)
  , m_retryTimeoutMin(500)             // 0.5 seconds
  , m_retryTimeoutMax(0, 4)            // 4 seconds
  , m_nonInviteTimeout(0, 16)          // 16 seconds
  , m_pduCleanUpTimeout(0, 5)          // 5 seconds
  , m_inviteTimeout(0, 32)             // 32 seconds
  , m_progressTimeout(0, 0, 3)       // 3 minutes
  , m_ackTimeout(0, 32)                // 32 seconds
  , m_registrarTimeToLive(0, 0, 0, 1)  // 1 hour
  , m_notifierTimeToLive(0, 0, 0, 1)   // 1 hour
  , m_keepAliveTimeout(0, 0, 1)      // 1 minute
  , m_keepAliveType(NoKeepAlive)
  , m_registeredUserMode(false)
  , m_shuttingDown(false)
  , m_lastSentCSeq(0)
  , m_defaultAppearanceCode(-1)
  , m_threadPool(maxThreads, "SIP Pool")
  , m_onHighPriorityInterfaceChange(PCREATE_InterfaceNotifier(OnHighPriorityInterfaceChange))
  , m_onLowPriorityInterfaceChange(PCREATE_InterfaceNotifier(OnLowPriorityInterfaceChange))
  , m_disableTrying(true)
{
  m_allowedEvents += SIPEventPackage(SIPSubscribe::Dialog);
  m_allowedEvents += SIPEventPackage(SIPSubscribe::Conference);

  // Make sure these have been contructed now to avoid
  // payload type disambiguation problems.
  GetOpalRFC2833();

#if OPAL_T38_CAPABILITY
  GetOpalCiscoNSE();
#endif

#if OPAL_PTLIB_SSL
  m_manager.AttachEndPoint(this, OPAL_PREFIX_SIPS);
#endif

  PInterfaceMonitor::GetInstance().AddNotifier(m_onHighPriorityInterfaceChange, 80);
  PInterfaceMonitor::GetInstance().AddNotifier(m_onLowPriorityInterfaceChange, 30);

  PTRACE(4, "Created endpoint.");
}


SIPEndPoint::~SIPEndPoint()
{
  PInterfaceMonitor::GetInstance().RemoveNotifier(m_onHighPriorityInterfaceChange);
  PInterfaceMonitor::GetInstance().RemoveNotifier(m_onLowPriorityInterfaceChange);
}


void SIPEndPoint::ShutDown()
{
  PTRACE(4, "Shutting down.");
  m_shuttingDown = true;

  // Clean up the handlers, wait for them to finish before destruction.
  for (;;) {
    bool allShutDown = true;
    for (SIPHandlers::iterator it = m_activeSIPHandlers.begin(); it != m_activeSIPHandlers.end(); ++it) {
      if (!it->second->ShutDown()) {
        allShutDown = false;
        break;
      }
    }
    if (allShutDown)
      break;
    PThread::Sleep(100);
  }
  m_activeSIPHandlers.RemoveAll();

  // Clean up transactions still in progress, waiting for them to terminate.
  for (;;) {
    bool allTerminated = true;
    for (PSafeDictionary<PString, SIPTransactionBase>::iterator it = m_activeTransactions.begin(); it != m_activeTransactions.end(); ++it) {
      if (!it->second->IsTerminated()) {
        allTerminated = false;
        break;
      }
    }
    if (allTerminated)
      break;
    PThread::Sleep(100);
  }
  m_activeTransactions.RemoveAll();

  for (PSafeDictionary<OpalTransportAddress, OpalTransport>::iterator it = m_transportsTable.begin(); it != m_transportsTable.end(); ++it)
    it->second->CloseWait();
  m_transportsTable.RemoveAll(true); // Make sure anything left is really deleted

  // Now shut down listeners and aggregators
  OpalEndPoint::ShutDown();
}


PString SIPEndPoint::GetDefaultTransport() const 
{
  PStringStream strm;
  strm << OpalTransportAddress::UdpPrefix() << ',' << OpalTransportAddress::TcpPrefix()
#if OPAL_PTLIB_SSL
       << ',' + OpalTransportAddress::TlsPrefix() << ':' << SIPURL::DefaultSecurePort
#if OPAL_PTLIB_HTTP
       << ',' << OpalTransportAddress::WsPrefix() << ":10080"
       << ',' << OpalTransportAddress::WssPrefix() << ":10081"
#endif
#endif
       ;
    return strm;
}


WORD SIPEndPoint::GetDefaultSignalPort() const
{
  return SIPURL::DefaultPort;
}


PStringList SIPEndPoint::GetNetworkURIs(const PString & name) const
{
  PStringList list = OpalRTPEndPoint::GetNetworkURIs(name);

  for (SIPHandlers::const_iterator it = m_activeSIPHandlers.begin(); it != m_activeSIPHandlers.end(); ++it) {
    if (it->second->GetMethod() == SIP_PDU::Method_REGISTER && it->second->GetAddressOfRecord().GetUserName() == name)
      list += it->second->GetAddressOfRecord();
  }

  return list;
}


void SIPEndPoint::NewIncomingConnection(OpalListener &, const OpalTransportPtr & transport)
{
  if (transport == NULL || m_shuttingDown)
    return;

  if (!transport->IsReliable()) {
    HandlePDU(transport); // Always just one PDU
    return;
  }

  AddTransport(transport, m_keepAliveType);
  TransportThreadMain(transport);
}


void SIPEndPoint::AddTransport(const OpalTransportPtr & transport, KeepAliveType keepAliveType)
{
  switch (keepAliveType) {
    case KeepAliveByCRLF :
      transport->SetKeepAlive(m_keepAliveTimeout, PBYTEArray(DefaultKeepAliveData, sizeof(DefaultKeepAliveData)));
      break;

    case KeepAliveByOPTION :
    {
      SIPURL addr(transport->GetRemoteAddress());
      SIP_PDU pdu(SIP_PDU::Method_OPTIONS, transport);
      pdu.InitialiseHeaders(addr, addr, addr, SIPTransaction::GenerateCallID(), 1);
      PString str;
      PINDEX len;
      pdu.Build(str, len);
      transport->SetKeepAlive(m_keepAliveTimeout, PBYTEArray((const BYTE *)str.GetPointer(), len));
      break;
    }

    default :
      break;
  }

  m_transportsTable.SetAt(transport->GetRemoteAddress(), transport);
  PTRACE(4, "Remembering transport " << *transport);
}


void SIPEndPoint::TransportThreadMain(OpalTransportPtr transport)
{
  if (transport != NULL) {
    PTRACE(4, "Transport read thread started on " << *transport);
    do {
      HandlePDU(transport);
    } while (transport->IsGood());

    transport->Close();
    PTRACE(4, "Transport read thread finished on " << *transport);
  }
  else
    PTRACE(4, "Transport read thread did not start");
}


OpalTransportPtr SIPEndPoint::GetTransport(const SIPTransactionOwner & transactor,
                                            SIP_PDU::StatusCodes & reason)
{
  OpalTransportAddress remoteAddress = transactor.GetRemoteTransportAddress();
  if (remoteAddress.IsEmpty()) {
    for (SIPHandlers::iterator it = m_activeSIPHandlers.begin(); ; ++it) {
      if (it->second->GetMethod() == SIP_PDU::Method_REGISTER) {
        remoteAddress = it->second->GetRemoteTransportAddress();
        PTRACE(4, "Found registration: aor=" << it->second->GetAddressOfRecord() << ", remote" << remoteAddress);
        break;
      }
    }
  }

  OpalTransportPtr transport;
  {
    P_INSTRUMENTED_WAIT_AND_SIGNAL(m_transportsMutex);

    // See if already have a link to that remote
    transport = m_transportsTable.Find(remoteAddress, PSafeReference);
    if (transport != NULL && transport->IsOpen()) {
      PTRACE(4, "Found existing transport " << *transport);
      return transport;
    }

    if (transport == NULL) {
      // No link, so need to create one
      PTRACE(4, "Creating transport to " << remoteAddress);

      KeepAliveType keepAliveType = m_keepAliveType;

      // See if we already have an interface, or have been told what to use
      PString localInterface = transactor.GetInterface();
      if (localInterface.IsEmpty())
        localInterface = transactor.GetRemoteURI().GetParamVars()(OPAL_INTERFACE_PARAM);
      if (localInterface.IsEmpty()) {
        // Get registration for domain and use interface we are currently using for that
        PString domain = transactor.GetRequestURI().GetHostPort();

        // Unlock to avoid deadlock through the registrar handler list
        m_transportsMutex.InstrumentedSignal(P_DEBUG_LOCATION);

        PSafePtr<SIPRegisterHandler> handler = PSafePtrCast<SIPHandler, SIPRegisterHandler>(m_activeSIPHandlers.FindSIPHandlerByDomain(domain, SIP_PDU::Method_REGISTER, PSafeReadOnly));

        // Lock it again, as the rest of this must be atomic
        m_transportsMutex.InstrumentedWait(PMaxTimeInterval, P_DEBUG_LOCATION);

        if (handler != NULL) {
          switch (handler->GetParams().m_compatibility) {
            case SIPRegister::e_RFC5626:
            case SIPRegister::e_Cisco:
              keepAliveType = KeepAliveByCRLF;
            default :
              break;
          }
        }

        // See if the above unlocked section had us create the same desired transport in a different thread
        transport = m_transportsTable.Find(remoteAddress, PSafeReference);
        if (transport != NULL) {
          if (transport->IsOpen()) {
            PTRACE(4, "Found newly created transport " << *transport);
            return transport;
          }
          PTRACE(4, "Re-opening newly created transport " << *transport);
        }
        else if (handler != NULL) {
          localInterface = handler->GetInterface();
          PTRACE(4, "Found registrar on domain " << domain << ", using interface \"" << localInterface << '"');
        }
        else {
          PTRACE(4, "No registrar on domain " << domain);
          PIPAddress remoteIP;
          if (remoteAddress.GetIpAddress(remoteIP)) {
            PIPAddress localIP = PIPSocket::GetRouteInterfaceAddress(remoteIP);
              for (OpalListenerList::iterator listener = m_listeners.begin(); listener != m_listeners.end(); ++listener) {
                PIPAddress listenIP;
                if (listener->GetProtoPrefix() == remoteAddress.GetProtoPrefix() && listener->GetLocalAddress().GetIpAddress(listenIP) && listenIP == localIP) {
                  localInterface = localIP.AsString();
                  PTRACE(4, "Using interface on listener " << *listener);
                  break;
                }
              }
          }
        }
      }

      if (transport == NULL) {
        OpalTransportAddress localAddress(localInterface, 0, remoteAddress.GetProtoPrefix());
        for (OpalListenerList::iterator listener = m_listeners.begin(); listener != m_listeners.end(); ++listener) {
          if ((transport = listener->CreateTransport(localAddress, remoteAddress)) != NULL)
            break;
        }

        if (transport == NULL) {
          // No compatible listeners, can't create a transport to send if we cannot hear the responses!
          PTRACE(2, "No compatible listener to create transport for " << remoteAddress);
          reason = SIP_PDU::Local_NoCompatibleListener;
          return NULL;
        }

        if (!transport->SetRemoteAddress(remoteAddress)) {
          PTRACE(1, "Could not use address \"" << remoteAddress << '"');
          reason = SIP_PDU::Local_BadTransportAddress;
          return NULL;
        }

        transport->GetChannel()->SetBufferSize(m_maxSizeUDP);

        PTRACE(4, "Created transport " << *transport << ", keepAlive=" << keepAliveType);
        AddTransport(transport, keepAliveType);
      }
    }
    else {
      PTRACE(4, "Re-opening transport " << *transport);
      transport->ResetIdle();
    }
  }

  // Link just created or was closed/lost
  if (!transport->Connect()) {
    PTRACE(1, "Could not connect to " << remoteAddress << " - " << transport->GetErrorText());
    switch (transport->GetErrorCode()) {
      case PChannel::Timeout :
        reason = SIP_PDU::Local_Timeout;
        break;
      case PChannel::AccessDenied :
        reason = SIP_PDU::Local_NotAuthenticated;
        break;
      default :
        reason = SIP_PDU::Local_TransportError;
    }
  }
  else if (!transport->IsAuthenticated((transactor.GetProxy().IsEmpty() ? transactor.GetRequestURI() : transactor.GetProxy()).GetHostName()))
    reason = SIP_PDU::Local_NotAuthenticated;
  else {
    if (transport->IsReliable())
      transport->AttachThread(new PThreadObj1Arg<SIPEndPoint, OpalTransportPtr>
              (*this, transport, &SIPEndPoint::TransportThreadMain, false, "SIP Transport", PThread::HighestPriority));
    else
      transport->SetPromiscuous(OpalTransport::AcceptFromAny);

    return transport;
  }

  // Outside of m_transportsTableMutex to avoid deadlock in CloseWait
  if (transport != NULL)
    transport->CloseWait();

  return NULL;
}


void SIPEndPoint::HandlePDU(const OpalTransportPtr & transport)
{
  // create a SIP_PDU structure, then get it to read and process PDU
  SIP_PDU * pdu = new SIP_PDU(SIP_PDU::NumMethods, transport);

  PTRACE(4, "Waiting for PDU on " << *transport);
  SIP_PDU::StatusCodes status = pdu->Read();
  switch (status) {
    case SIP_PDU::Local_KeepAlive :
      transport->Write("\r\n", 2); // Send PONG
      break;

    case SIP_PDU::Local_TransportLost :
      transport->Close();
      if (transport->IsReliable() && transport->HasKeepAlive()) {
        PTRACE(4, "Trying to reconnect dropped transport " << *transport);
        for (SIPHandlers::iterator it = m_activeSIPHandlers.begin(); it != m_activeSIPHandlers.end(); ++it) {
          SIPRegisterHandler * regHandler = dynamic_cast<SIPRegisterHandler *>(&*it->second);
          if (  regHandler != NULL &&
                regHandler->GetState() == SIPHandler::Subscribed &&
                regHandler->GetParams().m_compatibility == SIPRegister::e_RFC5626 &&
                regHandler->GetRemoteTransportAddress().IsEquivalent(transport->GetRemoteAddress())) {
            SIPHandler::State newState = SIPHandler::Restoring;
            if (!transport->Connect()) {
              // In case remote is bouncing, and is back up quickly, have another go
              PThread::Sleep(1000);
              if (!transport->Connect()) {
                // Remote has not come back quickly, possibly never, set register into Unavailable
                // mode where it periodically retries reconnect.
                newState = SIPHandler::Unavailable;
              }
            }
            regHandler->ActivateState(newState);
            break;
          }
        }
      }
      break;

    case SIP_PDU::Successful_OK :
      if (OnReceivedPDU(pdu)) 
        return;
      break;

    default :
      const SIPMIMEInfo & mime = pdu->GetMIME();
      if (status >= 300 && pdu->GetMethod() != SIP_PDU::NumMethods &&
          !mime.GetCSeq().IsEmpty() &&
          !mime.GetVia().IsEmpty() &&
          !mime.GetCallID().IsEmpty() &&
          !mime.GetFrom().IsEmpty() &&
          !mime.GetTo().IsEmpty())
        pdu->SendResponse(status);
  }

  delete pdu;
}


static PString TranslateENUM(const PString & remoteParty)
{
#if OPAL_PTLIB_DNS_RESOLVER
  // if there is no '@', and then attempt to use ENUM
  if (remoteParty.Find('@') == P_MAX_INDEX) {

    // make sure the number has only digits
    PINDEX pos = remoteParty.Find(':');
    PString e164 = pos != P_MAX_INDEX ? remoteParty.Mid(pos+1) : remoteParty;
    if (OpalIsE164(e164)) {
      PString str;
      if (PDNS::ENUMLookup(e164, "E2U+SIP", str)) {
        PTRACE(4, "ENUM converted remote party " << remoteParty << " to " << str);
        return str;
      }
    }
  }
#endif // OPAL_PTLIB_DNS_RESOLVER

  return remoteParty;
}


PSafePtr<OpalConnection> SIPEndPoint::MakeConnection(OpalCall & call,
                                                const PString & remoteParty,
                                                         void * userData,
                                                   unsigned int options,
                                OpalConnection::StringOptions * stringOptions)
{
  if (m_listeners.IsEmpty())
    return NULL;

  SIPConnection::Init init(call, *this);
  init.m_token = SIPURL::GenerateTag();
  init.m_userData = userData;
  init.m_address = TranslateENUM(remoteParty);
  init.m_options = options;
  init.m_stringOptions = stringOptions;
  return AddConnection(CreateConnection(init));
}


void SIPEndPoint::OnReleased(OpalConnection & connection)
{
  m_receivedConnectionMutex.Wait();
  m_receivedConnectionTokens.RemoveAt(connection.GetIdentifier());
  m_receivedConnectionMutex.Signal();
  OpalEndPoint::OnReleased(connection);
}


void SIPEndPoint::OnConferenceStatusChanged(OpalEndPoint & endpoint, const PString & uri, OpalConferenceState::ChangeType change)
{
  OpalConferenceStates states;
  if (!endpoint.GetConferenceStates(states, uri) || states.empty()) {
    PTRACE(2, "Unexpectedly unable to get conference state for " << uri);
    return;
  }

  const OpalConferenceState & state = states.front();
  PTRACE(4, "Conference state for " << state.m_internalURI << " has " << change);

  ConferenceMap::iterator confAOR = m_conferenceAOR.find(uri);
  if (confAOR != m_conferenceAOR.end())
    Notify(confAOR->second, SIPEventPackage(SIPSubscribe::Conference), state);

  for (OpalConferenceState::URIs::const_iterator it = state.m_accessURI.begin(); it != state.m_accessURI.begin(); ++it) {
    PTRACE(4, "Conference access URI: \"" << it->m_uri << '"');

    PURL aor = it->m_uri;
    if (aor.GetScheme().NumCompare("sip") != EqualTo)
      continue;

    switch (change) {
      case OpalConferenceState::Destroyed :
        Unregister(it->m_uri);
        break;

      case OpalConferenceState::Created :
        if (m_activeSIPHandlers.FindSIPHandlerByDomain(aor.GetHostName(), SIP_PDU::Method_REGISTER, PSafeReference) == NULL) {
          PTRACE(4, "Conference domain " << aor.GetHostName() << " unregistered, not registering name " << aor.GetUserName());
        }
        else {
          SIPRegister::Params params;
          params.m_addressOfRecord = it->m_uri;
          PString dummy;
          Register(params, dummy);
        }
        break;

      default :
        break;
    }
  }
}


PBoolean SIPEndPoint::GarbageCollection()
{
  PTRACE(6, "Garbage collection: transactions=" << m_activeTransactions.GetSize() << ", connections=" << m_connectionsActive.GetSize());

  for (PSafeDictionary<PString, SIPTransactionBase>::iterator it = m_activeTransactions.begin(); it != m_activeTransactions.end(); ++it) {
    if (it->second->IsTerminated())
      m_activeTransactions.RemoveAt(it->first); // Unlike a PDictionary() or std::map<>, this is safe to do
  }
  bool transactionsDone = m_activeTransactions.DeleteObjectsToBeRemoved();

  for (SIPHandlers::iterator it = m_activeSIPHandlers.begin(); it != m_activeSIPHandlers.end(); ++it) {
    // If unsubscribed then we do the shut down to clean up the handler
    if (it->second->GetState() == SIPHandler::Unsubscribed && it->second->ShutDown())
      m_activeSIPHandlers.RemoveAt(it->first); // Unlike a PDictionary() or std::map<>, this is safe to do
  }
  bool handlersDone = m_activeSIPHandlers.DeleteObjectsToBeRemoved();


  {
    std::list<OpalTransportPtr> transportsToClose;

    // Do not do the CloseWait() inside this mutex, can cause phantom (and, possibly, actual) deadlocks
    {
      P_INSTRUMENTED_WAIT_AND_SIGNAL(m_transportsMutex);
      for (PSafeDictionary<OpalTransportAddress, OpalTransport>::iterator it = m_transportsTable.begin(); it != m_transportsTable.end(); ++it) {
        if (it->second->IsIdle()) {
          PTRACE(3, "Removing transport to " << it->first);
          transportsToClose.push_back(it->second);
          m_transportsTable.RemoveAt(it->first);
        }
      }
    }

    for (std::list<OpalTransportPtr>::iterator it = transportsToClose.begin(); it != transportsToClose.end(); ++it)
      (*it)->CloseWait();

    /* Let transportsToClose go out of scope before m_transportsTable.DeleteObjectsToBeRemoved()
        so refernces removed, and transports can be actually be deleted. */
  }
  bool transportsDone = m_transportsTable.DeleteObjectsToBeRemoved();


  for (RegistrarDict::iterator it = m_registeredUAs.begin(); it != m_registeredUAs.end(); ++it) {
    if (it->second->ExpireBindings())
      OnChangedRegistrarAoR(*it->second);
    if (!it->second->HasBindings())
      m_registeredUAs.RemoveAt(it->first);
  }
  bool registrarDone = m_registeredUAs.DeleteObjectsToBeRemoved();

  if (!OpalSDPEndPoint::GarbageCollection())
    return false;

  if (m_shuttingDown)
    return transactionsDone && handlersDone && transportsDone && registrarDone;

  return true;
}


PStringList SIPEndPoint::GetAvailableStringOptions() const
{
  static char const * const StringOpts[] = {
    OPAL_OPT_FORWARD_REFER,
    OPAL_OPT_REFER_SUB,
    OPAL_OPT_NO_REFER_SUB,
    OPAL_OPT_PRACK_MODE,
    OPAL_OPT_INITIAL_OFFER,
    OPAL_OPT_ALLOW_EARLY_REPLACE,
    OPAL_OPT_EXTERNAL_SDP,
    OPAL_OPT_SDP_SSRC_INFO,
    OPAL_OPT_ENABLE_DTLS,
    OPAL_OPT_UNSECURE_SRTP
  };

  PStringList list = OpalSDPEndPoint::GetAvailableStringOptions();
  list += PStringList(PARRAYSIZE(StringOpts), StringOpts, true);
  return list;
}


PBoolean SIPEndPoint::IsAcceptedAddress(const SIPURL & /*toAddr*/)
{
  return true;
}



SIPConnection * SIPEndPoint::CreateConnection(const SIPConnection::Init & init)
{
  return new SIPConnection(init);
}


PBoolean SIPEndPoint::SetupTransfer(SIPConnection & transferredConnection,
                                    const PString & remoteParty,
                                    const PString & replaces)
{
  OpalConnection::StringOptions options;

  if (replaces.IsEmpty()) {
    PSafePtr<OpalConnection> transferredOtherConnection = transferredConnection.GetOtherPartyConnection();
    if (transferredOtherConnection != NULL &&
        m_manager.FindEndPoint(transferredOtherConnection->GetPrefixName()) != this &&
        remoteParty.NumCompare(transferredOtherConnection->GetPrefixName()+':') == EqualTo)
    {
      if (!transferredOtherConnection->TransferConnection(remoteParty))
        return false;
      PTRACE(3, "Bypassed transfer of " << *transferredOtherConnection << " to \"" << remoteParty << '"');
      return true;
    }
  }
  else {
    options.SetAt(SIP_HEADER_REPLACES, replaces);
    PSafePtr<SIPConnection> replacedConnection = GetSIPConnectionWithLock(replaces, PSafeReference);
    if (replacedConnection != NULL) {
      // We are transferring to another part of our system, see if it can be short circuited.
      PSafePtr<OpalConnection> transferredOtherConnection = transferredConnection.GetOtherPartyConnection();
      PSafePtr<OpalConnection> replacedOtherConnection = replacedConnection->GetOtherPartyConnection();
      if (transferredOtherConnection != NULL && replacedOtherConnection != NULL &&
          transferredOtherConnection->GetPrefixName() == replacedOtherConnection->GetPrefixName() &&
          m_manager.FindEndPoint(transferredOtherConnection->GetPrefixName()) != this)
      {
        if (!transferredOtherConnection->TransferConnection(replacedOtherConnection->GetToken()))
          return false;
        PTRACE(3, "Bypassed transfer of " << *transferredOtherConnection << " to " << *replacedOtherConnection);
        return true;
      }
    }
  }

  PTRACE(3, "Transferring " << transferredConnection << " to " << remoteParty << (replaces.IsEmpty() ? "" : " replacing ") << replaces);
  options.SetAt(SIP_HEADER_REFERRED_BY, transferredConnection.GetRedirectingParty());
  options.SetAt(OPAL_OPT_CALLING_PARTY_URL, transferredConnection.GetLocalPartyURL());

  SIPConnection::Init init(transferredConnection.GetCall(), *this);
  init.m_token = SIPURL::GenerateTag();
  init.m_address = TranslateENUM(remoteParty);
  init.m_stringOptions = &options;
  SIPConnection * newConnection = CreateConnection(init);
  if (!AddConnection(newConnection))
    return false;

  if (remoteParty.Find(OPAL_MAKE_URL_PARAM(OPAL_SIP_REFERRED_CONNECTION)) == P_MAX_INDEX)
    transferredConnection.Release(OpalConnection::EndedByCallForwarded);
  else
    transferredConnection.SetPhase(OpalConnection::ForwardingPhase);
  transferredConnection.CloseMediaStreams();

  return newConnection->SetUpConnection();
}


PBoolean SIPEndPoint::ForwardConnection(SIPConnection & connection, const PString & forwardParty)
{
  PSafePtr<OpalConnection> otherConnection = connection.GetOtherPartyConnection();
  if (otherConnection != NULL &&
      forwardParty.NumCompare(otherConnection->GetPrefixName()+':') == EqualTo &&
      otherConnection->ForwardCall(forwardParty))
  {
    PTRACE(3, "Bypassed forward of " << *otherConnection << " to \"" << forwardParty << '"');
    return true;
  }

  OpalCall & call = connection.GetCall();
  
  SIPConnection::Init init(call, *this);
  init.m_token = SIPURL::GenerateTag();
  init.m_address = forwardParty;
  SIPConnection * conn = CreateConnection(init);
  if (!AddConnection(conn))
    return false;

  connection.SetPhase(OpalConnection::ForwardingPhase);
  conn->SetUpConnection();
  connection.Release(OpalConnection::EndedByCallForwarded);

  return true;
}


bool SIPEndPoint::ClearDialogContext(const PString & descriptor)
{
  SIPDialogContext context;
  return context.FromString(descriptor) && ClearDialogContext(context);
}


bool SIPEndPoint::ClearDialogContext(SIPDialogContext & context)
{
  if (!context.IsEstablished())
    return true; // Was not actually fully formed dialog, assume cleared

  /* This is an extra increment of the sequence number to allow for
     any PDU's in the dialog being sent between the last saved
     context. Highly unlikely this will ever be by a million ... */
  context.IncrementCSeq(1000000);

  PSafePtr<SIPTransaction> byeTransaction = new SIPBye(*this, context);
  byeTransaction->WaitForCompletion();
  return !byeTransaction->IsFailed();
}


bool SIPEndPoint::OnReceivedPDU(SIP_PDU * pdu)
{
  if (PAssertNULL(pdu) == NULL)
    return false;

  PTRACE(4, "OnReceivedPDU: method=" << pdu->GetMethod() << ", id=" << pdu->GetTransactionID());

  // Prevent any new INVITE/SUBSCRIBE etc etc while we are on the way out.
  if (m_shuttingDown && pdu->GetMethod() != SIP_PDU::NumMethods) {
    pdu->SendResponse(SIP_PDU::Failure_ServiceUnavailable);
    return false;
  }

  // Check if we have already received this request (have a transaction in play)
  // But not ACK as that is really part of the INVITE transaction
  if (pdu->GetMethod() != SIP_PDU::Method_ACK) {
    PSafePtr<SIPTransaction> transaction = GetTransaction(pdu->GetTransactionID(), PSafeReadOnly);
    if (transaction != NULL && transaction->ReSend(*pdu))
      return false;
  }

  const SIPMIMEInfo & mime = pdu->GetMIME();

  /* Get tokens to determine the connection to operate on, not as easy as it
     sounds due to allowing for talking to ones self, always thought madness
     generally lies that way ... */

  PString fromToken = mime.GetFromTag();
  PString toToken = mime.GetToTag();
  bool hasFromConnection = HasConnection(fromToken);
  bool hasToConnection = HasConnection(toToken);

  switch (pdu->GetMethod()) {
    case SIP_PDU::Method_CANCEL :
      {
        m_receivedConnectionMutex.Wait();
        PString token = m_receivedConnectionTokens(mime.GetCallID());
        m_receivedConnectionMutex.Signal();
        if (!token.IsEmpty()) {
          new SIP_PDU_Work(*this, token, pdu);
          return true;
        }
      }
      // Do next case

    case SIP_PDU::NumMethods :  // Response
      {
        PString id = pdu->GetTransactionID();
        PSafePtr<SIPTransaction> transaction = GetTransaction(id, PSafeReference); // GetConnection() immutable so don't need read only
        if (transaction != NULL) {
          SIPConnection * connection = transaction->GetConnection();
          new SIP_PDU_Work(*this, connection != NULL ? connection->GetToken() : id, pdu);
          return true;
        }

        PTRACE(2, "Received response for unmatched transaction, id=" << id);
        if (pdu->GetMethod() == SIP_PDU::Method_CANCEL)
          pdu->SendResponse(SIP_PDU::Failure_TransactionDoesNotExist);
        return false;
      }

    case SIP_PDU::Method_INVITE :
      // Do we already know about this dialog?
      if (hasToConnection || hasFromConnection)
        break;

      if (toToken.IsEmpty()) {
        PWaitAndSignal mutex(m_receivedConnectionMutex);

        PString token = m_receivedConnectionTokens(mime.GetCallID());
        if (!token.IsEmpty()) {
          PSafePtr<SIPConnection> connection = GetSIPConnectionWithLock(token, PSafeReadOnly);
          if (connection != NULL) {
            PTRACE_CONTEXT_ID_PUSH_THREAD(*connection);
            switch (connection->CheckINVITE(*pdu)) {
              case SIPConnection::IsNewINVITE: // Process new INVITE
                break;

              case SIPConnection::IsDuplicateINVITE: // Completely ignore duplicate INVITE
                return false;

              case SIPConnection::IsReINVITE:
                if (connection->IsReleased()) {
                  pdu->SendResponse(SIP_PDU::Failure_RequestPending);  /// Pending request will be the BYE
                  return false;
                }
                new SIP_PDU_Work(*this, token, pdu); // Pass on to worker thread if re-INVITE
                return true;

              case SIPConnection::IsLoopedINVITE: // Send back error if looped INVITE
                SIP_PDU response(*pdu, SIP_PDU::Failure_LoopDetected);
                response.GetMIME().SetProductInfo(GetUserAgent(), connection->GetProductInfo());
                response.Send();
                return false;
            }
          }
        }

        PTRACE(4, "Received a new INVITE, sending 100 Trying");
        pdu->SendResponse(SIP_PDU::Information_Trying);
        return OnReceivedINVITE(pdu);
      }

      // Has to tag but doesn't correspond to a known connection, wrong.
      pdu->SendResponse(SIP_PDU::Failure_TransactionDoesNotExist);
      return false;

    case SIP_PDU::Method_BYE :
    case SIP_PDU::Method_ACK :
      if (!hasToConnection && !hasFromConnection) {
        PTRACE(4, "Does not have connection for "
               << (hasToConnection ? "" : "To tag")
               << (hasToConnection || hasFromConnection ? " " : " or ")
               << (hasFromConnection ? "" : "From tag"));
        pdu->SendResponse(SIP_PDU::Failure_TransactionDoesNotExist);
        return false;
      }
      break;

    default :   // any known method other than INVITE, CANCEL and ACK
      if (!m_disableTrying)
        pdu->SendResponse(SIP_PDU::Information_Trying);
      break;
  }

  if (hasToConnection || hasFromConnection) {
    new SIP_PDU_Work(*this, hasToConnection ? toToken : fromToken, pdu);
    return true;
  }

  PSafePtr<SIPHandler> handler = FindHandlerByPDU(*pdu, PSafeReference);
  new SIP_PDU_Work(*this, handler != NULL ? handler->GetCallID() : pdu->GetTransactionID(), pdu);
  return true;
}


bool SIPEndPoint::OnReceivedREGISTER(SIP_PDU & request)
{
  if (m_registrarDomains.IsEmpty())
    return false;

  SIPMIMEInfo & mime = request.GetMIME();
  mime.SetRecordRoute(PString::Empty()); // RFC3261/10.3

  if (!m_registrarDomains.Contains(request.GetURI().GetHostPort()) &&
      !m_registrarDomains.Contains(mime.GetTo().GetHostPort())) {
    request.SendResponse(SIP_PDU::Failure_NotFound);
    return true;
  }

  if (!mime.GetRequire().IsEmpty()) {
    PTRACE(3, "SIP-Reg", "REGISTER required unsupported feature: " << setfill(',') << mime.GetRequire());
    request.SendResponse(SIP_PDU::Failure_BadExtension);
    return true;
  }

  PTRACE(3, "SIP-Reg", "Handling REGISTER: " << mime.GetTo());

  SIP_PDU response(request, SIP_PDU::Successful_OK);
  response.SetStatusCode(InternalHandleREGISTER(request, &response));
  if (response.GetStatusCode() == SIP_PDU::Successful_OK) {
    // Private extension for mass registration.
    static const PConstCaselessString AoRListKey("X-OPAL-AoR-List");
    SIPURLList aorList;
    if (aorList.FromString(mime.Get(AoRListKey), SIPURL::ExternalURI)) {
      mime.Remove(AoRListKey);

      SIPURLList successList;
      for (SIPURLList::iterator aor = aorList.begin(); aor != aorList.end(); ++aor) {
        mime.SetTo(*aor);
        if (InternalHandleREGISTER(request, NULL))
          successList.push_back(*aor);
      }

      if (!successList.empty())
        response.GetMIME().Set(AoRListKey, successList.ToString());
    }
  }

  response.Send();
  return true;
}


SIP_PDU::StatusCodes SIPEndPoint::InternalHandleREGISTER(SIP_PDU & request, SIP_PDU * response)
{
  PSafePtr<RegistrarAoR> ua = m_registeredUAs.Find(request.GetMIME().GetTo());
  if (ua != NULL) {
    SIP_PDU::StatusCodes status = ua->OnReceivedREGISTER(*this, request);
    if (status != SIP_PDU::Successful_OK)
      return status;
  }
  else {
    if (request.GetMIME().GetExpires() == 0)
      return SIP_PDU::Failure_NotFound;

    ua = CreateRegistrarAoR(request);
    if (ua == NULL)
      return SIP_PDU::Failure_Forbidden;

    SIP_PDU::StatusCodes status = ua->OnReceivedREGISTER(*this, request);
    if (status != SIP_PDU::Successful_OK)
      return status;

    if (!ua->HasBindings())
      return SIP_PDU::Failure_NotFound;

    PTRACE(3, "SIP-Reg", "Created new Registered UA: " << *ua);
    m_registeredUAs.SetAt(ua->GetAoR(), ua);
  }

  OnChangedRegistrarAoR(*ua);
  if (response != NULL && ua->HasBindings())
    response->GetMIME().SetContact(ua->GetContacts().ToString());
  return SIP_PDU::Successful_OK;
}


SIPEndPoint::RegistrarAoR * SIPEndPoint::CreateRegistrarAoR(const SIP_PDU & request)
{
  return new RegistrarAoR(request.GetMIME().GetTo());
}


SIPURLList SIPEndPoint::GetRegistrarAoRs() const
{
  SIPURLList list;
  for (PSafePtr<RegistrarAoR> ua(m_registeredUAs); ua != NULL; ++ua)
    list.push_back(ua->GetAoR());
  return list;
}


void SIPEndPoint::OnChangedRegistrarAoR(RegistrarAoR & PTRACE_PARAM(ua))
{
  PTRACE(3, "SIP-Reg", "Registered UA status: " << ua);
}


SIPEndPoint::RegistrarAoR::RegistrarAoR(const PURL & aor)
  : m_aor(aor)
{
}


PObject::Comparison SIPEndPoint::RegistrarAoR::Compare(const PObject & obj) const
{
  return m_aor.Compare(dynamic_cast<const RegistrarAoR &>(obj).m_aor);
}


void SIPEndPoint::RegistrarAoR::PrintOn(ostream & strm) const
{
  strm << m_aor;
  if (m_bindings.empty())
    strm << "<unbound>";
  else {
    strm << " => ";
    for (BindingMap::const_iterator it = m_bindings.begin(); it != m_bindings.end(); ++it) {
      if (it != m_bindings.begin())
        strm << ',';
      strm << it->first;
    }
  }
}


PBoolean SIPEndPoint::RegistrarAoR::ExpireBindings()
{
  PTime now;
  bool expiredOne = false;

  for (BindingMap::iterator it = m_bindings.begin(); it != m_bindings.end(); ) {
    int expires = it->first.GetFieldParameters().GetInteger("expires") + 5; // A few seconds grace
    if ((now - it->second.m_lastUpdate).GetSeconds() < expires)
      ++it;
    else {
      PTRACE(4, "SIP-Reg", "Expired Contact " << it->first << " for AoR=" << m_aor);
      m_bindings.erase(it++);
      expiredOne = true;
    }
  }

  return expiredOne;
}


SIPURLList SIPEndPoint::RegistrarAoR::GetContacts() const
{
  SIPURLList list;
  for (BindingMap::const_iterator it = m_bindings.begin(); it != m_bindings.end(); ++it)
    list.push_back(it->first);
  return list;
}


SIP_PDU::StatusCodes SIPEndPoint::RegistrarAoR::OnReceivedREGISTER(SIPEndPoint & endpoint, const SIP_PDU & request)
{
  const SIPMIMEInfo & mime = request.GetMIME();

  SIPURLList newContacts;
  if (!mime.GetContacts(newContacts, endpoint.GetRegistrarTimeToLive().GetSeconds())) {
    PTRACE(4, "SIP-Reg", "Empty Contacts header");
    return SIP_PDU::Successful_OK;
  }

  PString id = mime.GetCallID();
  {
    unsigned cseq = mime.GetCSeqIndex();
    std::map<PString, unsigned>::iterator it = m_cseq.find(id);
    if (it == m_cseq.end())
      m_cseq[id] = cseq;
    else if (cseq > it->second)
      it->second = cseq;
    else {
      PTRACE(4, "SIP-Reg", "Old/duplicate REGISTER, not updating anything");
      return SIP_PDU::Successful_OK;
    }
  }

  // Remove all with this ID, if in REGISTER again will be added back
  for (BindingMap::iterator it = m_bindings.begin(); it != m_bindings.end();) {
    if (it->second.m_id != id)
      ++it;
    else
      m_bindings.erase(it++);
  }

  unsigned expires = mime.GetExpires(0);

  // Special case of '*', everything says removed
  if (newContacts.size() == 1 && newContacts.front().GetHostName() == "*") {
    if (expires != 0) {
      PTRACE(2, "SIP-Reg", "Non zero Expires with '*' Contacts");
      return SIP_PDU::Failure_BadRequest;
    }

    return SIP_PDU::Successful_OK;
  }

  // Put bindings we have been given back again, effectively updating them
  for (SIPURLList::const_iterator contact = newContacts.begin(); contact != newContacts.end(); ++contact) {
    if (contact->GetFieldParameters().GetInteger("expires", expires) > 0)
      m_bindings[*contact].m_id = id;
  }

  request.GetMIME().GetProductInfo(m_productInfo);

  return SIP_PDU::Successful_OK;
}


bool SIPEndPoint::OnReceivedSUBSCRIBE(SIP_PDU & request, SIPDialogContext * dialog)
{
  SIPMIMEInfo & mime = request.GetMIME();

  SIPSubscribe::EventPackage eventPackage(mime.GetEvent());

  CanNotifyResult canNotify = CanNotifyImmediate;

  // See if already subscribed. Now this is not perfect as we only check the call-id and strictly
  // speaking we should check the from-tag and to-tags as well due to it being a dialog.
  PSafePtr<SIPHandler> handler = FindHandlerByPDU(request, PSafeReadWrite);
  if (handler == NULL) {
    SIPDialogContext newDialog(mime);
    if (dialog == NULL)
      dialog = &newDialog;

    if ((canNotify = CanNotify(eventPackage, dialog->GetLocalURI())) == CannotNotify) {
      SIPResponse * response = new SIPResponse(*this, request, SIP_PDU::Failure_BadEvent);
      response->GetMIME().SetAllowEvents(m_allowedEvents); // Required by spec
      response->Send();
      return true;
    }

    handler = new SIPNotifyHandler(*this, eventPackage, *dialog);
    handler.SetSafetyMode(PSafeReadWrite);
    m_activeSIPHandlers.Append(handler);

    mime.SetTo(dialog->GetLocalURI());
  }

  // Update expiry time
  unsigned expires = mime.GetExpires();

  SIPResponse * response = new SIPResponse(*this, request, SIP_PDU::Successful_Accepted);
  response->GetMIME().SetEvent(eventPackage); // Required by spec
  response->GetMIME().SetExpires(expires);    // Required by spec
  response->Send();

  if (handler->IsDuplicateCSeq(mime.GetCSeqIndex()))
    return true;

  if (expires == 0) {
    handler->ActivateState(SIPHandler::Unsubscribing);
    return true;
  }

  handler->SetExpire(expires);

  if (canNotify == CanNotifyImmediate)
    handler->SendNotify(NULL); // Send initial NOTIFY as per spec 3.1.6.2/RFC3265

  return true;
}


void SIPEndPoint::OnReceivedResponse(SIPTransaction &, SIP_PDU &)
{
}


PSafePtr<SIPConnection> SIPEndPoint::GetSIPConnectionWithLock(const PString & token,
                                                              PSafetyMode mode,
                                                              SIP_PDU::StatusCodes * errorCode)
{
  PSafePtr<SIPConnection> connection = PSafePtrCast<OpalConnection, SIPConnection>(GetConnectionWithLock(token, mode));
  if (connection != NULL)
    return connection;

  PString to;
  static const char toTag[] = ";to-tag=";
  PINDEX pos = token.Find(toTag);
  if (pos != P_MAX_INDEX) {
    pos += sizeof(toTag)-1;
    to = token(pos, token.Find(';', pos)-1).Trim();
  }

  PString from;
  static const char fromTag[] = ";from-tag=";
  pos = token.Find(fromTag);
  if (pos != P_MAX_INDEX) {
    pos += sizeof(fromTag)-1;
    from = token(pos, token.Find(';', pos)-1).Trim();
  }

  PString callid = token.Left(token.Find(';')).Trim();
  if (callid.IsEmpty() || to.IsEmpty() || from.IsEmpty()) {
    if (errorCode != NULL)
      *errorCode = SIP_PDU::Failure_BadRequest;
    return NULL;
  }

  connection = PSafePtrCast<OpalConnection, SIPConnection>(m_connectionsActive.GetAt(0, PSafeReference));
  while (connection != NULL) {
    const SIPDialogContext & context = connection->GetDialog();
    if (context.GetCallID() == callid) {
      if (context.GetLocalTag() == to && context.GetRemoteTag() == from) {
        if (connection.SetSafetyMode(mode))
          return connection;
        break;
      }

      PTRACE(4, "Replaces header matches callid, but not to/from tags: "
                "to=" << context.GetLocalTag() << ", from=" << context.GetRemoteTag());
    }

    ++connection;
  }

  if (errorCode != NULL)
    *errorCode = SIP_PDU::Failure_TransactionDoesNotExist;
  return NULL;
}


bool SIPEndPoint::OnReceivedINVITE(SIP_PDU * request)
{
  SIPMIMEInfo & mime = request->GetMIME();

  // parse the incoming To field, and check if we accept incoming calls for this address
  SIPURL toAddr(mime.GetTo());
  if (!IsAcceptedAddress(toAddr)) {
    PTRACE(2, "Incoming INVITE for " << request->GetURI() << " for unacceptable address " << toAddr);
    request->SendResponse(SIP_PDU::Failure_NotFound);
    return false;
  }

  if (!request->IsContentSDP(true)) {
    // Do not currently support anything other than SDP, in particular multipart stuff.
    PTRACE(2, "Incoming INVITE for " << request->GetURI() << " does not contain SDP");
    SIP_PDU response(*request, SIP_PDU::Failure_UnsupportedMediaType);
    response.GetMIME().SetAccept(OpalSDPEndPoint::ContentType());
    response.GetMIME().SetAcceptEncoding("identity");
    response.SetAllow(GetAllowedMethods());
    response.Send();
    return false;
  }

  // See if we are replacing an existing call.
  OpalCall * call = NULL;
  if (mime.Contains("Replaces")) {
    SIP_PDU::StatusCodes errorCode;
    PSafePtr<SIPConnection> replacedConnection = GetSIPConnectionWithLock(mime("Replaces"), PSafeReference, &errorCode);
    if (replacedConnection == NULL) {
      PTRACE_IF(2, errorCode==SIP_PDU::Failure_BadRequest,
                "Bad Replaces header in INVITE for " << request->GetURI());
      PTRACE_IF(2, errorCode==SIP_PDU::Failure_TransactionDoesNotExist,
                "No connection matching dialog info in Replaces header of INVITE from " << request->GetURI());
      request->SendResponse(errorCode);
      return false;
    }

    // Use the existing call instance when replacing the SIP side of it.
    call = &replacedConnection->GetCall();
    PTRACE(3, "Incoming INVITE replaces connection " << *replacedConnection);
  }

  if (call == NULL) {
    // Get new instance of a call, abort if none created
    call = m_manager.InternalCreateCall();
    if (call == NULL) {
      request->SendResponse(SIP_PDU::Failure_TemporarilyUnavailable);
      return false;
    }
  }

  PTRACE_CONTEXT_ID_PUSH_THREAD(call);

  // ask the endpoint for a connection
  SIPConnection::Init init(*call, *this);
  init.m_token = SIPURL::GenerateTag();
  init.m_invite = request;
  SIPConnection *connection = CreateConnection(init);
  if (!AddConnection(connection)) {
    PTRACE(1, "Failed to create SIPConnection for INVITE for " << request->GetURI() << " to " << toAddr);
    request->SendResponse(SIP_PDU::Failure_NotFound);
    return false;
  }

  // m_receivedConnectionMutex already set
  PString token = connection->GetToken();
  m_receivedConnectionTokens.SetAt(mime.GetCallID(), token);

  // Get the connection to handle the rest of the INVITE in the thread pool
  new SIP_PDU_Work(*this, token, request);
  return true;
}


void SIPEndPoint::OnTransactionFailed(SIPTransaction &)
{
}


bool SIPEndPoint::OnReceivedREFER(SIP_PDU & request)
{
  // REFER outside of a connect dialog is bizarre, but that's Cisco for you

  SIPURL url = request.GetMIME().GetTo();
  PSafePtr<SIPHandler> handler = FindSIPHandlerByUrl(url, SIP_PDU::Method_REGISTER, PSafeReference);

  if (handler == NULL) {
    url = request.GetMIME().GetFrom();
    handler = FindSIPHandlerByUrl(url, SIP_PDU::Method_REGISTER, PSafeReference);
  }

  if (handler == NULL || dynamic_cast<SIPRegisterHandler *>(&*handler)->GetParams().m_compatibility != SIPRegister::e_Cisco) {
    PTRACE(3, "Could not find a Cisco REGISTER corresponding to the REFER " << url);
    return false; // Returns method not allowed
  }

  SIPResponse * response = new SIPResponse(*this, request, SIP_PDU::Successful_OK);
  response->Send();
  return true;
}


bool SIPEndPoint::OnReceivedNOTIFY(SIP_PDU & request)
{
  const SIPMIMEInfo & mime = request.GetMIME();
  SIPEventPackage eventPackage(mime.GetEvent());

  PTRACE(3, "Received NOTIFY " << eventPackage);
  
  // A NOTIFY will have the same CallID than the SUBSCRIBE request it corresponds to
  // Technically should check for whole dialog, but call-id will do.
  PSafePtr<SIPHandler> handler = FindHandlerByPDU(request, PSafeReadWrite);

  if (handler == NULL && eventPackage == SIPSubscribe::MessageSummary) {
    PTRACE(4, "Work around Asterisk bug in message-summary event package.");
    SIPURL to(mime.GetFrom().GetHostName());
    to.SetUserName(mime.GetTo().GetUserName());
    handler = m_activeSIPHandlers.FindSIPHandlerByUrl(to, SIP_PDU::Method_SUBSCRIBE, eventPackage, PSafeReadWrite);
  }

  if (handler == NULL) {
    PTRACE(3, "Could not find a SUBSCRIBE corresponding to the NOTIFY " << eventPackage);
    SIPResponse * response = new SIPResponse(*this, request, SIP_PDU::Failure_TransactionDoesNotExist);
    response->Send();
    return true;
  }

  PTRACE_CONTEXT_ID_PUSH_THREAD(handler);
  PTRACE(3, "Found a SUBSCRIBE corresponding to the NOTIFY " << eventPackage);
  return handler->OnReceivedNOTIFY(request);
}


bool SIPEndPoint::OnReceivedMESSAGE(SIP_PDU & request)
{
  // handle a MESSAGE received outside the context of a call
  PTRACE(4, "Received MESSAGE outside the context of a call");

  // if there is a callback, assume that the application knows what it is doing
  if (!m_onConnectionlessMessage.IsNULL()) {
    ConnectionlessMessageInfo info(request);
    m_onConnectionlessMessage(*this, info);
    switch (info.m_status) {
      case ConnectionlessMessageInfo::MethodNotAllowed :
        return false;

      case ConnectionlessMessageInfo::SendOK :
        {
          SIPResponse * response = new SIPResponse(*this, request, SIP_PDU::Successful_OK);
          response->Send();
        }
        // Do next case

      case ConnectionlessMessageInfo::ResponseSent :
        return true;

      default :
        break;
    }
  }

#if OPAL_HAS_SIPIM
  OpalSIPIMContext::OnReceivedMESSAGE(*this, NULL, request);
#else
  request.SendResponse(SIP_PDU::Failure_BadRequest);
#endif
  return true;
}


bool SIPEndPoint::OnReceivedOPTIONS(SIP_PDU & request)
{
  SIPResponse * response = new SIPResponse(*this, request, SIP_PDU::Successful_OK);
  response->Send();
  return true;
}


bool SIPEndPoint::Register(const PString & host,
                           const PString & user,
                           const PString & authName,
                           const PString & password,
                           const PString & realm,
                           unsigned expire,
                           const PTimeInterval & minRetryTime,
                           const PTimeInterval & maxRetryTime)
{
  SIPRegister::Params params;
  params.m_addressOfRecord = user;
  params.m_registrarAddress = host;
  params.m_authID = authName;
  params.m_password = password;
  params.m_realm = realm;
  params.m_expire = expire;
  params.m_minRetryTime = minRetryTime;
  params.m_maxRetryTime = maxRetryTime;

  PString dummy;
  return Register(params, dummy);
}


bool SIPEndPoint::Register(const SIPRegister::Params & newParams, PString & aor, bool asynchronous)
{
  SIP_PDU::StatusCodes reason;
  return Register(newParams, aor, asynchronous ? NULL : &reason);
}


bool SIPEndPoint::Register(const SIPRegister::Params & newParams, PString & aor, SIP_PDU::StatusCodes * reason)
{
  SIPRegister::Params params(newParams);
  if (!params.Normalise(GetDefaultLocalPartyName(), GetRegistrarTimeToLive()))
    return false;

  PTRACE(4, "Start REGISTER\n" << params);
  PSafePtr<SIPHandler> handler = m_activeSIPHandlers.FindSIPHandlerByUrl(params.m_addressOfRecord, SIP_PDU::Method_REGISTER, PSafeReadWrite);

  // If there is already a request with this URL and method, 
  // then update it with the new information
  if (handler != NULL) {
    PSafePtrCast<SIPHandler, SIPRegisterHandler>(handler)->UpdateParameters(params);
  }
  else {
    // Otherwise create a new request with this method type
    handler = CreateRegisterHandler(params);
    m_activeSIPHandlers.Append(handler);
  }

  aor = handler->GetAddressOfRecord().AsString();

  if (!handler->ActivateState(SIPHandler::Subscribing))
    return false;

  if (reason == NULL)
    return true;

  m_registrationComplete[aor].m_sync.Wait();
  *reason = m_registrationComplete[aor].m_reason;
  m_registrationComplete.erase(aor);
  return handler->GetState() == SIPHandler::Subscribed;
}


SIPRegisterHandler * SIPEndPoint::CreateRegisterHandler(const SIPRegister::Params & params)
{
  return new SIPRegisterHandler(*this, params);
}


PBoolean SIPEndPoint::IsRegistered(const PString & token, bool includeOffline) 
{
  PSafePtr<SIPHandler> handler = m_activeSIPHandlers.FindSIPHandlerByCallID(token, PSafeReference);
  if (handler == NULL)
    handler = m_activeSIPHandlers.FindSIPHandlerByUrl(token, SIP_PDU::Method_REGISTER, PSafeReference);

  if (handler != NULL)
    return includeOffline ? (handler->GetState() != SIPHandler::Unsubscribed)
                          : (handler->GetState() == SIPHandler::Subscribed);

  PTRACE(1, "Could not find active REGISTER for " << token);
  return false;
}


PBoolean SIPEndPoint::Unregister(const PString & token)
{
  PSafePtr<SIPHandler> handler = m_activeSIPHandlers.FindSIPHandlerByCallID(token, PSafeReference);
  if (handler == NULL)
    handler = m_activeSIPHandlers.FindSIPHandlerByUrl(token, SIP_PDU::Method_REGISTER, PSafeReference);

  if (handler != NULL)
    return handler->ActivateState(SIPHandler::Unsubscribing);

  PTRACE(1, "Could not find active REGISTER for \"" << token << '"');
  return false;
}


bool SIPEndPoint::UnregisterAll()
{
  bool atLeastOne = false;

  for (SIPHandlers::iterator it = m_activeSIPHandlers.begin(); it != m_activeSIPHandlers.end(); ++it) {
    if (it->second->GetMethod() == SIP_PDU::Method_REGISTER &&
        it->second->ActivateState(SIPHandler::Unsubscribing))
      atLeastOne = true;
  }

  return atLeastOne;
}


static void OutputStatus1(ostream & strm, const SIPURL & aor, bool was, const char * op)
{
  SIPURL sanitisedAOR = aor;
  sanitisedAOR.Sanitise(SIPURL::ExternalURI);

  strm << "SIP ";
  if (!was)
    strm << "un";
  strm << op << " of " << sanitisedAOR;
}


static void OutputStatus2(ostream & strm, SIP_PDU::StatusCodes reason)
{
  switch (reason) {
    case SIP_PDU::Successful_OK :
      strm << " successful";
      break;

    case SIP_PDU::Failure_RequestTimeout :
      strm << " proxy";
    case SIP_PDU::Local_Timeout :
      strm << " time out";
      break;

    case SIP_PDU::Failure_UnAuthorised :
      strm << " has invalid credentials";
      break;

    case SIP_PDU::Local_NotAuthenticated :
      strm << " has invalid certificates";
      break;

    case SIP_PDU::Local_NoCompatibleListener :
      strm << " has no compatible listener";
      break;

    default :
      strm << " failed (" << reason << ')';
  }
  strm << '.';
}


ostream & operator<<(ostream & strm, const SIPEndPoint::RegistrationStatus & status)
{
  OutputStatus1(strm, status.m_addressofRecord, status.m_wasRegistering, "registration");
  OutputStatus2(strm, status.m_reason);
  return strm;
}


ostream & operator<<(ostream & strm, const SIPSubscribe::SubscriptionStatus & status)
{
  OutputStatus1(strm, status.m_addressofRecord, status.m_wasSubscribing, "subscription");
  strm << " to " << status.m_handler->GetEventPackage() << " events";
  OutputStatus2(strm, status.m_reason);
  return strm;
}


bool SIPEndPoint::GetRegistrationStatus(const PString & token, RegistrationStatus & status)
{
  PSafePtr<SIPHandler> handler = m_activeSIPHandlers.FindSIPHandlerByCallID(token, PSafeReference);
  if (handler == NULL)
    handler = m_activeSIPHandlers.FindSIPHandlerByUrl(token, SIP_PDU::Method_REGISTER, PSafeReference);

  if (handler == NULL) {
    PTRACE(1, "Could not find active REGISTER for " << token);
    return false;
  }

  status.m_handler = dynamic_cast<SIPRegisterHandler *>(&*handler);
  status.m_addressofRecord = handler->GetAddressOfRecord();
  status.m_wasRegistering = handler->GetState() != SIPHandler::Unsubscribing;
  status.m_reRegistering = handler->GetState() == SIPHandler::Subscribed;
  status.m_reason = handler->GetLastResponseStatus();
  status.m_productInfo = handler->GetProductInfo();
  status.m_userData = NULL;
  return true;
}


void SIPEndPoint::OnRegistrationStatus(const RegistrationStatus & status)
{
  OnRegistrationStatus(status.m_addressofRecord, status.m_wasRegistering, status.m_reRegistering, status.m_reason);

  if (!status.m_wasRegistering ||
       status.m_reRegistering ||
       status.m_reason == SIP_PDU::Information_Trying)
    return;

  std::map<PString, RegistrationCompletion>::iterator it = m_registrationComplete.find(status.m_addressofRecord);
  if (it != m_registrationComplete.end()) {
    it->second.m_reason = status.m_reason;
    it->second.m_sync.Signal();
  }
}


void SIPEndPoint::OnRegistrationStatus(const PString & aor,
                                       PBoolean wasRegistering,
                                       PBoolean /*reRegistering*/,
                                       SIP_PDU::StatusCodes reason)
{
  if (reason == SIP_PDU::Information_Trying)
    return;

  if (reason == SIP_PDU::Successful_OK)
    OnRegistered(aor, wasRegistering);
  else
    OnRegistrationFailed(aor, reason, wasRegistering);
}


void SIPEndPoint::OnRegistrationFailed(const PString & /*aor*/, 
               SIP_PDU::StatusCodes /*reason*/, 
               PBoolean /*wasRegistering*/)
{
}
    

void SIPEndPoint::OnRegistered(const PString & /*aor*/, 
             PBoolean /*wasRegistering*/)
{
}


bool SIPEndPoint::Subscribe(SIPSubscribe::PredefinedPackages eventPackage, unsigned expire, const PString & to)
{
  SIPSubscribe::Params params(eventPackage);
  params.m_addressOfRecord = to;
  params.m_expire = expire;

  PString dummy;
  return Subscribe(params, dummy);
}


bool SIPEndPoint::Subscribe(const SIPSubscribe::Params & newParams, PString & token, bool tokenIsAOR)
{
  SIPSubscribe::Params params(newParams);
  if (!params.Normalise(PString::Empty(), GetNotifierTimeToLive()))
    return false;

  PTRACE(4, "Start SUBSCRIBE\n" << params);
  PSafePtr<SIPHandler> handler = m_activeSIPHandlers.FindSIPHandlerByUrl(params.m_addressOfRecord, SIP_PDU::Method_SUBSCRIBE, params.m_eventPackage, PSafeReadWrite);

  // If there is already a request with this URL and method, 
  // then update it with the new information
  if (handler != NULL)
    PSafePtrCast<SIPHandler, SIPSubscribeHandler>(handler)->UpdateParameters(params);
  else {
    // Otherwise create a new request with this method type
    handler = new SIPSubscribeHandler(*this, params);
    m_activeSIPHandlers.Append(handler);
  }

  token = tokenIsAOR ? handler->GetAddressOfRecord().AsString() : handler->GetCallID();

  return handler->ActivateState(SIPHandler::Subscribing);
}


bool SIPEndPoint::IsSubscribed(const PString & token, bool includeOffline) 
{
  PSafePtr<SIPHandler> handler = m_activeSIPHandlers.FindSIPHandlerByCallID(token, PSafeReadOnly);
  if (handler == NULL)
    return false;

  return includeOffline ? (handler->GetState() != SIPHandler::Unsubscribed)
                        : (handler->GetState() == SIPHandler::Subscribed);
}


bool SIPEndPoint::IsSubscribed(const PString & eventPackage, const PString & token, bool includeOffline) 
{
  PSafePtr<SIPHandler> handler = m_activeSIPHandlers.FindSIPHandlerByCallID(token, PSafeReference);
  if (handler == NULL) {
    handler = m_activeSIPHandlers.FindSIPHandlerByUrl(token, SIP_PDU::Method_SUBSCRIBE, eventPackage, PSafeReference);
    if (handler == NULL) {
      PTRACE(4, "Could not find subscription: token=\"" << token << "\", event=" << eventPackage);
      return false;
    }
  }
  else {
    if (handler->GetEventPackage() != eventPackage) {
      PTRACE(3, "Subscription mismatch: token=\"" << token << "\", event=" << eventPackage);
      return false;
    }
  }

  PTRACE(4, "Checking subscription: token=\"" << token << "\", event=" << eventPackage << ", state=" << handler->GetState());
  return includeOffline ? (handler->GetState() != SIPHandler::Unsubscribed)
                        : (handler->GetState() == SIPHandler::Subscribed);
}


bool SIPEndPoint::Unsubscribe(const PString & token, bool invalidateNotifiers)
{
  return Unsubscribe(SIPEventPackage(), token, invalidateNotifiers);
}


bool SIPEndPoint::Unsubscribe(SIPSubscribe::PredefinedPackages eventPackage,
                              const PString & token,
                              bool invalidateNotifiers)
{
  return Unsubscribe(SIPEventPackage(eventPackage), token, invalidateNotifiers);
}


bool SIPEndPoint::Unsubscribe(const PString & eventPackage, const PString & token, bool invalidateNotifiers)
{
  PSafePtr<SIPSubscribeHandler> handler = PSafePtrCast<SIPHandler, SIPSubscribeHandler>(
                                                m_activeSIPHandlers.FindSIPHandlerByCallID(token, PSafeReference));
  if (handler == NULL)
    handler = PSafePtrCast<SIPHandler, SIPSubscribeHandler>(
          m_activeSIPHandlers.FindSIPHandlerByUrl(token, SIP_PDU::Method_SUBSCRIBE, eventPackage, PSafeReference));
  else {
    if (!eventPackage.IsEmpty() && handler->GetEventPackage() != eventPackage)
      handler.SetNULL();
  }

  if (handler == NULL) {
    PTRACE(1, "Could not find active SUBSCRIBE of " << eventPackage << " package to " << token);
    return false;
  }

  if (SIPEventPackage(SIPSubscribe::Conference) == eventPackage) {
    ConferenceMap::iterator it = m_conferenceAOR.begin();
    while (it != m_conferenceAOR.end()) {
      if (it->second != handler->GetAddressOfRecord())
        ++it;
      else
        m_conferenceAOR.erase(it++);
    }
  }

  if (invalidateNotifiers) {
    SIPSubscribe::Params params(handler->GetParams());
    params.m_onNotify = NULL;
    params.m_onSubcribeStatus = NULL;
    handler->UpdateParameters(params);
  }

  return handler->ActivateState(SIPHandler::Unsubscribing);
}


bool SIPEndPoint::UnsubcribeAll(SIPSubscribe::PredefinedPackages eventPackage)
{
  return UnsubcribeAll(SIPEventPackage(eventPackage));
}


bool SIPEndPoint::UnsubcribeAll(const PString & eventPackage)
{
  bool atLeastOne = false;

  for (SIPHandlers::iterator it = m_activeSIPHandlers.begin(); it != m_activeSIPHandlers.end(); ++it) {
    if (it->second->GetMethod() == SIP_PDU::Method_SUBSCRIBE &&
        it->second->GetEventPackage() == eventPackage &&
        it->second->ActivateState(SIPHandler::Unsubscribing))
      atLeastOne = true;
  }

  return atLeastOne;
}


bool SIPEndPoint::GetSubscriptionStatus(const PString & token, const PString & eventPackage, SubscriptionStatus & status)
{
  PSafePtr<SIPSubscribeHandler> handler = PSafePtrCast<SIPHandler, SIPSubscribeHandler>(
                                                m_activeSIPHandlers.FindSIPHandlerByCallID(token, PSafeReference));
  if (handler == NULL)
    handler = PSafePtrCast<SIPHandler, SIPSubscribeHandler>(
          m_activeSIPHandlers.FindSIPHandlerByUrl(token, SIP_PDU::Method_SUBSCRIBE, eventPackage, PSafeReference));
  else {
    if (!eventPackage.IsEmpty() && handler->GetEventPackage() != eventPackage)
      handler.SetNULL();
  }

  if (handler == NULL) {
    PTRACE(1, "Could not find active SUBSCRIBE of " << eventPackage << " package to " << token);
    return false;
  }

  status.m_handler = handler;
  status.m_addressofRecord = handler->GetAddressOfRecord();
  status.m_wasSubscribing = handler->GetState() != SIPHandler::Unsubscribing;
  status.m_reSubscribing = handler->GetState() == SIPHandler::Subscribed;
  status.m_reason = handler->GetLastResponseStatus();
  status.m_productInfo = handler->GetProductInfo();
  status.m_userData = NULL;
  return true;
}


void SIPEndPoint::OnSubscriptionStatus(const SubscriptionStatus & status)
{
  // backwards compatiblity
  OnSubscriptionStatus(*status.m_handler,
                        status.m_addressofRecord,
                        status.m_wasSubscribing,
                        status.m_reSubscribing,
                        status.m_reason);
}


void SIPEndPoint::OnSubscriptionStatus(const PString & /*eventPackage*/,
                                       const SIPURL & /*aor*/,
                                       bool /*wasSubscribing*/,
                                       bool /*reSubscribing*/,
                                       SIP_PDU::StatusCodes /*reason*/)
{
}


void SIPEndPoint::OnSubscriptionStatus(SIPSubscribeHandler & handler,
                                       const SIPURL & aor,
                                       bool wasSubscribing,
                                       bool reSubscribing,
                                       SIP_PDU::StatusCodes reason)
{
  // backwards compatiblity
  OnSubscriptionStatus(handler.GetParams().m_eventPackage, 
                       aor, 
                       wasSubscribing, 
                       reSubscribing, 
                       reason);
}


bool SIPEndPoint::CanNotify(const PString & eventPackage)
{
  if (m_allowedEvents.Contains(eventPackage))
    return true;

  PTRACE(3, "Cannot notify event \"" << eventPackage << "\" not one of ["
         << setfill(',') << m_allowedEvents << setfill(' ') << ']');
  return false;
}


SIPEndPoint::CanNotifyResult SIPEndPoint::CanNotify(const PString & eventPackage, const SIPURL & aor)
{
  if (SIPEventPackage(SIPSubscribe::Conference) == eventPackage) {
    OpalConferenceStates states;
    if (m_manager.GetConferenceStates(states, aor.GetUserName()) || states.empty()) {
      PString uri = states.front().m_internalURI;
      ConferenceMap::iterator it = m_conferenceAOR.find(uri);
      while (it != m_conferenceAOR.end() && it->first == uri) {
        if (it->second == aor)
          return CanNotifyImmediate;
      }

      m_conferenceAOR.insert(ConferenceMap::value_type(uri, aor));
      return CanNotifyImmediate;
    }

    PTRACE(3, "Cannot notify \"" << eventPackage << "\" event, no conferences for " << aor);
    return CannotNotify;
  }

#if OPAL_SIP_PRESENCE
  if (SIPEventPackage(SIPSubscribe::Presence) == eventPackage) {
    PSafePtr<OpalPresentity> presentity = m_manager.GetPresentity(aor);
    if (presentity != NULL && presentity->GetAttributes().GetEnum(
                  SIP_Presentity::SubProtocolKey, SIP_Presentity::e_WithAgent) == SIP_Presentity::e_PeerToPeer)
      return CanNotifyImmediate;

    PTRACE(3, "Cannot notify \"" << eventPackage << "\" event, no presentity " << aor);
    return CannotNotify;
  }
#endif // OPAL_SIP_PRESENCE

  return CanNotify(eventPackage) ? CanNotifyImmediate : CannotNotify;
}


bool SIPEndPoint::Notify(const SIPURL & aor, const PString & eventPackage, const PObject & body)
{
  bool atLeastOne = false;

  for (SIPHandlers::iterator it = m_activeSIPHandlers.begin(); it != m_activeSIPHandlers.end(); ++it) {
    if (it->second->GetMethod() == SIP_PDU::Method_NOTIFY &&
        it->second->GetAddressOfRecord() == aor &&
        it->second->GetEventPackage() == eventPackage &&
        it->second->SendNotify(&body))
      atLeastOne = true;
  }

  return atLeastOne;
}


bool SIPEndPoint::SendMESSAGE(SIPMessage::Params & params)
{
  if (!params.Normalise(PString::Empty(), GetRegistrarTimeToLive()))
    return false;

  PTRACE(4, "Start MESSAGE\n" << params);

  // don't send empty MESSAGE because some clients barf (cough...X-Lite...cough)
  if (params.m_body.IsEmpty()) {
    PTRACE(2, "Cannot send empty MESSAGE.");
    return false;
  }

  /* if conversation ID has been set, assume the handler with the matching
     call ID is what was used last time. If no conversation ID has been set,
     see if the destination AOR exists and use that handler (and it's
     call ID). Else create a new conversation. */
  PSafePtr<SIPHandler> handler;
  if (params.m_id.IsEmpty())
    handler = m_activeSIPHandlers.FindSIPHandlerByUrl(params.m_remoteAddress, SIP_PDU::Method_MESSAGE, PSafeReference);
  else
    handler = m_activeSIPHandlers.FindSIPHandlerByCallID(params.m_id, PSafeReference);

  // create or update the handler if required
  if (handler == NULL) {
    handler = new SIPMessageHandler(*this, params);
    m_activeSIPHandlers.Append(handler);
  }
  else
    PSafePtrCast<SIPHandler, SIPMessageHandler>(handler)->UpdateParameters(params);

  params.m_id = handler->GetCallID();

  return handler->ActivateState(SIPHandler::Subscribing);
}


#if OPAL_HAS_SIPIM
void SIPEndPoint::OnMESSAGECompleted(const SIPMessage::Params & params, SIP_PDU::StatusCodes reason)
{
    OpalSIPIMContext::OnMESSAGECompleted(*this, params, reason);
}
#else
void SIPEndPoint::OnMESSAGECompleted(const SIPMessage::Params &, SIP_PDU::StatusCodes)
{
}
#endif


bool SIPEndPoint::SendOPTIONS(const SIPOptions::Params & newParams)
{
  SIPOptions::Params params(newParams);
  if (!params.Normalise(GetDefaultLocalPartyName(), GetNotifierTimeToLive()))
    return false;

  PTRACE(4, "Start OPTIONS\n" << params);
  new SIPOptions(*this, params);
  return true;
}


void SIPEndPoint::OnOptionsCompleted(const SIPOptions::Params & PTRACE_PARAM(params),
                                                const SIP_PDU & PTRACE_PARAM(response))
{
  PTRACE(3, "Completed OPTIONS command to " << params.m_remoteAddress << ", status=" << response.GetStatusCode());
}


PBoolean SIPEndPoint::Ping(const PURL & to)
{
  PSafePtr<SIPHandler> handler = m_activeSIPHandlers.FindSIPHandlerByUrl(to, SIP_PDU::Method_PING, PSafeReference);
  if (handler == NULL) {
    handler = new SIPPingHandler(*this, to);
    m_activeSIPHandlers.Append(handler);
  }

  return handler->ActivateState(SIPHandler::Subscribing);
}


bool SIPEndPoint::Publish(const SIPSubscribe::Params & newParams, const PString & body, PString & aor)
{
  SIPSubscribe::Params params(newParams);
  if (!params.Normalise(GetDefaultLocalPartyName(), newParams.m_expire))
    return false;

  PTRACE(4, "Start PUBLISH\n" << params);
  PSafePtr<SIPHandler> handler = m_activeSIPHandlers.FindSIPHandlerByUrl(params.m_addressOfRecord, SIP_PDU::Method_PUBLISH, params.m_eventPackage, PSafeReadWrite);
  if (handler != NULL)
    handler->SetBody(params.m_expire != 0 ? body : PString::Empty());
  else {
    handler = new SIPPublishHandler(*this, params, body);
    m_activeSIPHandlers.Append(handler);
  }

  aor = handler->GetAddressOfRecord().AsString();

  return handler->ActivateState(params.m_expire != 0 ? SIPHandler::Subscribing : SIPHandler::Unsubscribing);
}


bool SIPEndPoint::Publish(const PString & to, const PString & body, unsigned expire)
{
  SIPSubscribe::Params params(SIPSubscribe::Presence);
  params.m_addressOfRecord = to;
  params.m_expire = expire;

  PString aor;
  return Publish(params, body, aor);
}


#if OPAL_SIP_PRESENCE
bool SIPEndPoint::PublishPresence(const SIPPresenceInfo & info, unsigned expire)
{
  SIPSubscribe::Params params(SIPSubscribe::Presence);
  params.m_addressOfRecord = info.m_contact.IsEmpty() ? info.m_entity.AsString() : info.m_contact;
  params.m_expire          = expire;
  params.m_agentAddress    = info.m_presenceAgent;
  params.m_contentType     = "application/pidf+xml";

  PString aor;
  return Publish(params, expire == 0 ? PString::Empty() : info.AsXML(), aor);
}


void SIPEndPoint::OnPresenceInfoReceived(const SIPPresenceInfo & info)
{
  PTRACE(4, "Received presence for entity '" << info.m_entity << "' using old API");

  // For backward compatibility
  switch (info.m_state) {
    case OpalPresenceInfo::Available :
      OnPresenceInfoReceived(info.m_entity, "open", info.m_note);
      break;
    case OpalPresenceInfo::NoPresence :
      OnPresenceInfoReceived(info.m_entity, "closed", info.m_note);
      break;
    default :
      OnPresenceInfoReceived(info.m_entity, PString::Empty(), info.m_note);
  }
}


void SIPEndPoint::OnPresenceInfoReceived(const PString & /*entity*/,
                                         const PString & /*basic*/,
                                         const PString & /*note*/)
{
}
#endif // OPAL_SIP_PRESENCE


bool SIPEndPoint::OnReINVITE(SIPConnection &, bool, const PString &)
{
  return true;
}


void SIPEndPoint::OnDialogInfoReceived(const SIPDialogNotification & PTRACE_PARAM(info))
{
  PTRACE(3, "Received dialog info for \"" << info.m_entity << "\" id=\"" << info.m_callId << '"');
}


void SIPEndPoint::SendNotifyDialogInfo(const SIPDialogNotification & info)
{
  Notify(info.m_entity, SIPEventPackage(SIPSubscribe::Dialog), info);
}


void SIPEndPoint::OnRegInfoReceived(const SIPRegNotification & PTRACE_PARAM(info))
{
  PTRACE(3, "Received registration info for \"" << info.m_aor << "\" state=" << info.GetStateName());
}


bool SIPEndPoint::OnReceivedInfoPackage(SIPConnection & /*connection*/,
                                        const PString & /*package*/,
                                        const PMultiPartList & /*content*/)
{
  return false;
}


void SIPEndPoint::SetProxy(const PString & hostname,
                           const PString & username,
                           const PString & password)
{
  PStringStream str;
  if (!hostname.IsEmpty()) {
    str << "sip:";
    if (!username.IsEmpty()) {
      str << username;
      if (!password.IsEmpty())
        str << ':' << password;
      str << '@';
    }
    str << hostname;
  }
  m_proxy = str;
}


void SIPEndPoint::SetProxy(const SIPURL & url) 
{ 
  m_proxy = url; 
  PTRACE_IF(3, !url.IsEmpty(), "Outbound proxy for endpoint set to " << url);
}


PString SIPEndPoint::GetUserAgent() const 
{
  return m_userAgentString;
}

void SIPEndPoint::OnStartTransaction(SIPConnection & /*conn*/, SIPTransaction & /*transaction*/)
{
}

unsigned SIPEndPoint::GetAllowedMethods() const
{
  return (1<<SIP_PDU::Method_INVITE   )|
         (1<<SIP_PDU::Method_ACK      )|
         (1<<SIP_PDU::Method_CANCEL   )|
         (1<<SIP_PDU::Method_BYE      )|
         (1<<SIP_PDU::Method_OPTIONS  )|
         (1<<SIP_PDU::Method_NOTIFY   )|
         (1<<SIP_PDU::Method_REFER    )|
         (1<<SIP_PDU::Method_MESSAGE  )|
         (1<<SIP_PDU::Method_INFO     )|
         (1<<SIP_PDU::Method_PING     )|
         (1<<SIP_PDU::Method_PRACK    )|
         (1<<SIP_PDU::Method_SUBSCRIBE);
}


bool SIPEndPoint::GetAuthentication(const PString & realm, PString & authId, PString & password)
{
  // Try to find authentication parameters for the given realm
  PSafePtr<SIPHandler> handler = m_activeSIPHandlers.FindSIPHandlerByAuthRealm(realm, authId, PSafeReadOnly);
  if (handler == NULL) {
    if (m_registeredUserMode)
      return false;

    if ((handler = m_activeSIPHandlers.FindSIPHandlerByAuthRealm(realm, PSafeReadOnly)) == NULL) {
      for (SIPHandlers::iterator it = m_activeSIPHandlers.begin(); it != m_activeSIPHandlers.end(); ++it) {
        if (it->second->GetMethod() == SIP_PDU::Method_REGISTER) {
          handler = it->second;
          break;
        }
      }
      if (handler == NULL)
        return false;
      PTRACE(4, "Using first registrar " << handler->GetAddressOfRecord() << " for authentication");
    }
  }

  // really just after password, but username MAY change too.
  authId = handler->GetAuthID();
  password = handler->GetPassword();
  return true;
}


SIPURL SIPEndPoint::GetDefaultLocalURL(const OpalTransport & transport, const SIPURL & remoteAddress)
{
  OpalTransportAddress localAddress;

  OpalTransportAddressArray interfaces = GetInterfaceAddresses(&transport);
  if (!interfaces.IsEmpty())
    localAddress = interfaces[0];
  else {
    PIPSocket::Address myAddress = PIPSocket::GetInvalidAddress();
    PIPSocket::GetHostAddress(myAddress);
    PIPSocket::Address transportAddress;
    if (transport.GetRemoteAddress().GetIpAddress(transportAddress))
      GetManager().TranslateIPAddress(myAddress, transportAddress);
    localAddress = OpalTransportAddress(myAddress, GetDefaultSignalPort(), transport.GetProtoPrefix());
  }

  PString scheme = remoteAddress.GetScheme();
  if (scheme == "tel")
    scheme.MakeEmpty();

  SIPURL localURL;

  PString defPartyName(GetDefaultLocalPartyName());
  PString user, host;
  if (!defPartyName.Split('@', user, host)) 
    localURL = SIPURL(defPartyName, localAddress, 0, scheme);
  else {
    localURL = SIPURL(user, localAddress, 0, scheme);   // set transport from address
    localURL.SetHostName(host);
  }

  localURL.SetDisplayName(GetDefaultDisplayName());
  PTRACE(4, "Generated default local URI: " << localURL);
  return localURL;
}


void SIPEndPoint::AdjustToRegistration(SIP_PDU & pdu, SIPConnection * connection, const OpalTransport * transport)
{
  bool isMethod;
  switch (pdu.GetMethod()) {
    case SIP_PDU::Method_REGISTER :
      return;

    case SIP_PDU::NumMethods :
      isMethod = false;
      break;

    default :
      isMethod = true;
  }

  SIPMIMEInfo & mime = pdu.GetMIME();

  SIPURL from(mime.GetFrom());
  SIPURL to(mime.GetTo());

  PString user, domain, scheme;
  if (isMethod) {
    user   = from.GetUserName();
    domain = to.GetHostName();
    scheme = to.GetScheme();
  }
  else {
    user   = to.GetUserName();
    domain = from.GetHostName();
    scheme = from.GetScheme();
    if (connection != NULL && to.GetDisplayName() != connection->GetDisplayName()) {
      to.SetDisplayName(connection->GetDisplayName());
      mime.SetTo(to);
    }
  }

  const SIPRegisterHandler * registrar = NULL;
  PSafePtr<SIPHandler> handler;

  if (scheme != "tel") {
    SIPURL url(domain);
    url.SetUserName(user);
    handler = m_activeSIPHandlers.FindSIPHandlerByUrl(url, SIP_PDU::Method_REGISTER, PSafeReadOnly);
    PTRACE_IF(4, handler != NULL, "Found registrar on aor sip:" << user << '@' << domain);
  }
  else {
    if (domain.IsEmpty() || OpalIsE164(domain)) {
      // No context, just get first registration
      for (SIPHandlers::iterator it = m_activeSIPHandlers.begin(); it != m_activeSIPHandlers.end(); ++it) {
        if (it->second->GetMethod() == SIP_PDU::Method_REGISTER) {
          handler = it->second;
          break;
        }
      }
      if (handler != NULL) {
        PTRACE(4, "Using first registrar " << handler->GetAddressOfRecord() << " for tel URI");
        if (connection != NULL)
          connection->GetDialog().SetProxy(handler->GetAddressOfRecord(), false);
      }
      else {
        PTRACE(2, "No registrars available for tel URI");
        if (connection != NULL) {
          connection->Release(OpalConnection::EndedByIllegalAddress);
          return;
        }
      }
    }

    // A "tel" scheme just uses default for transport type
    scheme.MakeEmpty();
  }

  // If precise AOR not found, locate the name used for the domain.
  if (handler == NULL && !m_registeredUserMode) {
    handler = m_activeSIPHandlers.FindSIPHandlerByDomain(domain, SIP_PDU::Method_REGISTER, PSafeReadOnly);
    PTRACE_IF(4, handler != NULL, "Found registrar on domain " << domain);
  }
  if (handler != NULL) {
    registrar = dynamic_cast<const SIPRegisterHandler *>(&*handler);
    PAssertNULL(registrar);
  }
  else {
    PTRACE(4, "No registrar for aor sip:" << user << '@' << domain);
  }

  if (isMethod && registrar != NULL) {
    if (!mime.Has("Route")) {
      if (!pdu.SetRoute(registrar->GetServiceRoute()))
        pdu.SetRoute(registrar->GetProxy());
    }

    // For many servers the From address must be address-of-record, but don't touch if dialog already done
    if (connection == NULL || !connection->GetDialog().IsEstablished()) {
      PStringToString fieldParams = from.GetFieldParameters();
      from = registrar->GetAddressOfRecord();
      from.GetFieldParameters() = fieldParams;
      if (connection != NULL)
        from.SetDisplayName(connection->GetDisplayName());
      from.Sanitise(SIPURL::FromURI);
      mime.SetFrom(from);
      PTRACE(4, "Adjusted 'From' to " << from << " from registered user.");
      user = from.GetUserName();
    }
  }

  if (!mime.Has("Contact") && pdu.GetStatusCode() != SIP_PDU::Information_Trying) {
    OpalTransportAddress remoteAddress = pdu.GetURI().GetTransportAddress();
    SIPURL contact;
    if (transport == NULL)
      transport = pdu.GetTransport();
    if (transport != NULL) {
      OpalTransportAddress localAddress = transport->GetLocalAddress();
      remoteAddress = transport->GetRemoteAddress();

      if (registrar != NULL) {
        contact = registrar->GetContacts().FindCompatible(localAddress PTRACE_PARAM(, "registered"));
        PTRACE_IF(4, !contact.IsEmpty(), "Adjusted Contact to "
                  << contact << " from registration " << registrar->GetAddressOfRecord());
      }

      if (contact.IsEmpty()) {
        SIPURLList listenerAddresses;
        OpalTransportAddressArray interfaces = GetInterfaceAddresses(transport);
        for (PINDEX i = 0; i < interfaces.GetSize(); ++i)
          listenerAddresses.push_back(SIPURL(user, interfaces[i], 0, scheme));
        contact = listenerAddresses.FindCompatible(localAddress PTRACE_PARAM(, "listening"));
        PTRACE_IF(4, !contact.IsEmpty(), "Adjusted Contact to " << contact << " from listeners and local address " << localAddress);
      }
    }

    if (contact.IsEmpty()) {
      contact = SIPURL(user, m_listeners[0].GetLocalAddress(remoteAddress), 0, scheme);
      PTRACE(4, "Adjusted Contact to " << contact << " from first listener.");
    }

    if (connection != NULL) {
      PSafePtr<OpalConnection> other = connection->GetOtherPartyConnection();
      if (other != NULL && other->GetConferenceState(NULL))
        contact.GetFieldParameters().Set("isfocus", "");

      contact.SetDisplayName(connection->GetDisplayName());
    }

    contact.Sanitise(SIPURL::ContactURI);
    mime.SetContact(contact.AsQuotedString());
  }
}


PSafePtr<SIPHandler> SIPEndPoint::FindHandlerByPDU(const SIP_PDU & pdu, PSafetyMode mode)
{
  const SIPMIMEInfo & mime = pdu.GetMIME();

  PSafePtr<SIPHandler> handler;

  PString id = mime.GetCallID();
  if ((handler = m_activeSIPHandlers.FindSIPHandlerByCallID(id, mode)) != NULL)
    return handler;

  PString tag = mime.GetTo().GetTag();
  if ((handler = m_activeSIPHandlers.FindSIPHandlerByCallID(tag, mode)) != NULL)
    return handler;

  return m_activeSIPHandlers.FindSIPHandlerByCallID(id+';'+tag, mode);
}


///////////////////////////////////////////////////////////////////////////////////////////////////

SIP_PDU_Work::SIP_PDU_Work(SIPEndPoint & ep, const PString & token, SIP_PDU * pdu)
  : SIPWorkItem(ep, token)
  , m_pdu(pdu)
{
  PTRACE(4, "Queueing PDU \"" << *m_pdu << "\", transaction="
         << m_pdu->GetTransactionID() << ", token=" << m_token);
  ep.GetThreadPool().AddWork(this, token);
}


SIP_PDU_Work::~SIP_PDU_Work()
{
  delete m_pdu;
}


void SIP_PDU_Work::Work()
{
  if (PAssertNULL(m_pdu) == NULL)
    return;

  // Check if we have already have a transaction in play
  // But not ACK as that is really part of the INVITE transaction
  if (m_pdu->GetMethod() != SIP_PDU::Method_ACK) {
    PString transactionID = m_pdu->GetTransactionID();
    PSafePtr<SIPTransaction> transaction = m_endpoint.GetTransaction(transactionID, PSafeReference);
    if (transaction != NULL) {
      PTRACE_CONTEXT_ID_PUSH_THREAD(*transaction);

      if (m_pdu->GetMethod() == SIP_PDU::NumMethods) {
        PTRACE(3, "Handling PDU \"" << *m_pdu << "\" for transaction=" << transactionID);
        transaction->OnReceivedResponse(*m_pdu);
        PTRACE(4, "Handled PDU \"" << *m_pdu << '"');
      }
      else if (transaction.SetSafetyMode(PSafeReadWrite)) {
        PTRACE(4, "Retransmitting previous response for transaction id=" << transactionID);
        transaction->InitialiseHeaders(*m_pdu);
        transaction->Send();
      }
      return;
    }

    if (m_pdu->GetMethod() == SIP_PDU::NumMethods) {
      PTRACE(2, "Cannot find transaction " << transactionID << " for response PDU \"" << *m_pdu << '"');
      return;
    }
  }

  PSafePtr<SIPConnection> connection = m_endpoint.GetSIPConnectionWithLock(m_token, PSafeReadWrite);
  if (connection != NULL) {
    PTRACE_CONTEXT_ID_PUSH_THREAD(*connection);
    PTRACE(3, "Handling connection PDU \"" << *m_pdu << "\" for token=" << m_token);
    connection->OnReceivedPDU(*m_pdu);
    PTRACE(4, "Handled connection PDU \"" << *m_pdu << '"');
    return;
  }

  PTRACE(3, "Handling non-connection PDU \"" << *m_pdu << "\" for token=" << m_token);

  bool sendResponse = true;
  switch (m_pdu->GetMethod()) {
    case SIP_PDU::Method_REGISTER :
      if (m_endpoint.OnReceivedREGISTER(*m_pdu))
        sendResponse = false;
      break;

    case SIP_PDU::Method_SUBSCRIBE :
      if (m_endpoint.OnReceivedSUBSCRIBE(*m_pdu, NULL))
        sendResponse = false;
      break;

    case SIP_PDU::Method_REFER :
       if (m_endpoint.OnReceivedREFER(*m_pdu))
        sendResponse = false;
       break;

    case SIP_PDU::Method_NOTIFY :
       if (m_endpoint.OnReceivedNOTIFY(*m_pdu))
        sendResponse = false;
       break;

    case SIP_PDU::Method_MESSAGE :
      if (m_endpoint.OnReceivedMESSAGE(*m_pdu))
        sendResponse = false;
      break;
   
    case SIP_PDU::Method_OPTIONS :
      if (m_endpoint.OnReceivedOPTIONS(*m_pdu))
        sendResponse = false;
      break;

    default :
      break;
  }

  if (sendResponse) {
    SIP_PDU response(*m_pdu, SIP_PDU::Failure_MethodNotAllowed);
    response.SetAllow(m_endpoint.GetAllowedMethods()); // Required by spec
    response.Send();
  }

  PTRACE(3, "Handled non-connection PDU \"" << *m_pdu << "\" for token=" << m_token);
}


OpalTransportAddress SIPEndPoint::NextSRVAddress(const SIPURL & url)
{
  PWaitAndSignal lock(m_SRVIndexMutex);
  if (GetSRVIndex(url) == P_MAX_INDEX)
    return OpalTransportAddress();

  // After GetSRVIndex() we know it exists in map
  SRVIndexMap::iterator it = m_SRVIndex.find(url.GetHostName());
  OpalTransportAddress addr = url.GetTransportAddress(++it->second);
  if (addr.IsEmpty()) {
    PTRACE(4, "Reached last SRV record, trying again from beginning");
    it->second = 0;
    addr = url.GetTransportAddress(0);
  }

  return addr;
}


PINDEX SIPEndPoint::GetSRVIndex(const SIPURL & url)
{
  PWaitAndSignal lock(m_SRVIndexMutex);
  SRVIndexMap::iterator it = m_SRVIndex.find(url.GetHostName());
  if (it == m_SRVIndex.end())
    it = m_SRVIndex.insert(std::make_pair(url.GetHostName(), url.CanLookupSRV() ? 0 : P_MAX_INDEX)).first;
  return it->second;
}


void SIPEndPoint::ResetSRVIndex(const SIPURL & url)
{
  PWaitAndSignal lock(m_SRVIndexMutex);
  SRVIndexMap::iterator it = m_SRVIndex.find(url.GetHostName());
  if (it != m_SRVIndex.end() && it->second != P_MAX_INDEX)
    it->second = 0;
}


///////////////////////////////////////////////////////////////////////////////////////////////

void SIPEndPoint::OnHighPriorityInterfaceChange(PInterfaceMonitor &, PInterfaceMonitor::InterfaceChange entry)
{
  if (entry.m_added) {
    // special case if interface filtering is used: A new interface may 'hide' the old interface.
    // If this is the case, remove the transport interface. 
    //
    // There is a race condition: If the transport interface binding is cleared AFTER
    // PMonitoredSockets::ReadFromSocket() is interrupted and starts listening again,
    // the transport will still listen on the old interface only. Therefore, clear the
    // socket binding BEFORE the monitored sockets update their interfaces.
    if (PInterfaceMonitor::GetInstance().HasInterfaceFilter()) {
      for (SIPHandlers::iterator it = m_activeSIPHandlers.begin(); it != m_activeSIPHandlers.end(); ++it) {
        if (it->second->GetInterface() == entry.GetName())
          it->second->ActivateState(SIPHandler::Unavailable, true);
      }
    }
  }
}


void SIPEndPoint::OnLowPriorityInterfaceChange(PInterfaceMonitor &, PInterfaceMonitor::InterfaceChange entry)
{
  for (SIPHandlers::iterator it = m_activeSIPHandlers.begin(); it != m_activeSIPHandlers.end(); ++it) {
    if (entry.m_added) {
      if (it->second->GetState() == SIPHandler::Unavailable)
        it->second->ActivateState(SIPHandler::Restoring);
    }
    else {
      if (it->second->GetInterface() == entry.GetName())
        it->second->ActivateState(it->second->GetState() == SIPHandler::Subscribed
                                  ? SIPHandler::Refreshing : SIPHandler::Restoring, true);
    }
  }
}


#endif // OPAL_SIP

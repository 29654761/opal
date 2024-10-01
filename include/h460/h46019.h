//
// h46019.h
//
// Code automatically generated by asnparse.
//

#include <opal_config.h>

#if ! H323_DISABLE_H46019

#ifndef __H46019_H
#define __H46019_H

#ifdef P_USE_PRAGMA
#pragma interface
#endif

#include <ptclib/asner.h>

#include <asn/h225.h>
#include <asn/h245.h>


//
// TraversalParameters
//

class H46019_TraversalParameters : public PASN_Sequence
{
#ifndef PASN_LEANANDMEAN
    PCLASSINFO(H46019_TraversalParameters, PASN_Sequence);
#endif
  public:
    H46019_TraversalParameters(unsigned tag = UniversalSequence, TagClass tagClass = UniversalTagClass);

    enum OptionalFields {
      e_multiplexedMediaChannel,
      e_multiplexedMediaControlChannel,
      e_multiplexID,
      e_keepAliveChannel,
      e_keepAlivePayloadType,
      e_keepAliveInterval
    };

    H245_TransportAddress m_multiplexedMediaChannel;
    H245_TransportAddress m_multiplexedMediaControlChannel;
    PASN_Integer m_multiplexID;
    H245_TransportAddress m_keepAliveChannel;
    PASN_Integer m_keepAlivePayloadType;
    H225_TimeToLive m_keepAliveInterval;

    PINDEX GetDataLength() const;
    PBoolean Decode(PASN_Stream & strm);
    void Encode(PASN_Stream & strm) const;
#ifndef PASN_NOPRINTON
    void PrintOn(ostream & strm) const;
#endif
    Comparison Compare(const PObject & obj) const;
    PObject * Clone() const;
};


#endif // __H46019_H

#endif // if ! H323_DISABLE_H46019


// End of h46019.h

/* ----------------------------------------------------------------------------
 * This file was automatically generated by SWIG (http://www.swig.org).
 * Version 4.0.1
 *
 * Do not make changes to this file unless you know what you are doing--modify
 * the SWIG interface file instead.
 * ----------------------------------------------------------------------------- */

package org.opalvoip.opal;

public class OpalContext {
  private transient long swigCPtr;
  protected transient boolean swigCMemOwn;

  protected OpalContext(long cPtr, boolean cMemoryOwn) {
    swigCMemOwn = cMemoryOwn;
    swigCPtr = cPtr;
  }

  protected static long getCPtr(OpalContext obj) {
    return (obj == null) ? 0 : obj.swigCPtr;
  }

  @SuppressWarnings("deprecation")
  protected void finalize() {
    delete();
  }

  public synchronized void delete() {
    if (swigCPtr != 0) {
      if (swigCMemOwn) {
        swigCMemOwn = false;
        OPALJNI.delete_OpalContext(swigCPtr);
      }
      swigCPtr = 0;
    }
  }

  public OpalContext() {
    this(OPALJNI.new_OpalContext(), true);
  }

  public long Initialise(String options, long version) {
    return OPALJNI.OpalContext_Initialise__SWIG_0(swigCPtr, this, options, version);
  }

  public long Initialise(String options) {
    return OPALJNI.OpalContext_Initialise__SWIG_1(swigCPtr, this, options);
  }

  public boolean IsInitialised() {
    return OPALJNI.OpalContext_IsInitialised(swigCPtr, this);
  }

  public void ShutDown() {
    OPALJNI.OpalContext_ShutDown(swigCPtr, this);
  }

  public boolean GetMessage(OpalMessagePtr message, long timeout) {
    return OPALJNI.OpalContext_GetMessage__SWIG_0(swigCPtr, this, OpalMessagePtr.getCPtr(message), message, timeout);
  }

  public boolean GetMessage(OpalMessagePtr message) {
    return OPALJNI.OpalContext_GetMessage__SWIG_1(swigCPtr, this, OpalMessagePtr.getCPtr(message), message);
  }

  public boolean SendMessage(OpalMessagePtr message) {
    return OPALJNI.OpalContext_SendMessage__SWIG_0(swigCPtr, this, OpalMessagePtr.getCPtr(message), message);
  }

  public boolean SendMessage(OpalMessagePtr message, OpalMessagePtr response) {
    return OPALJNI.OpalContext_SendMessage__SWIG_1(swigCPtr, this, OpalMessagePtr.getCPtr(message), message, OpalMessagePtr.getCPtr(response), response);
  }

  public boolean SetUpCall(OpalMessagePtr response, String partyB, String partyA, String alertingType) {
    return OPALJNI.OpalContext_SetUpCall__SWIG_0(swigCPtr, this, OpalMessagePtr.getCPtr(response), response, partyB, partyA, alertingType);
  }

  public boolean SetUpCall(OpalMessagePtr response, String partyB, String partyA) {
    return OPALJNI.OpalContext_SetUpCall__SWIG_1(swigCPtr, this, OpalMessagePtr.getCPtr(response), response, partyB, partyA);
  }

  public boolean SetUpCall(OpalMessagePtr response, String partyB) {
    return OPALJNI.OpalContext_SetUpCall__SWIG_2(swigCPtr, this, OpalMessagePtr.getCPtr(response), response, partyB);
  }

  public boolean AnswerCall(String callToken) {
    return OPALJNI.OpalContext_AnswerCall(swigCPtr, this, callToken);
  }

  public boolean ClearCall(String callToken, OpalCallEndReason reason) {
    return OPALJNI.OpalContext_ClearCall__SWIG_0(swigCPtr, this, callToken, reason.swigValue());
  }

  public boolean ClearCall(String callToken) {
    return OPALJNI.OpalContext_ClearCall__SWIG_1(swigCPtr, this, callToken);
  }

  public boolean SendUserInput(String callToken, String userInput, long duration) {
    return OPALJNI.OpalContext_SendUserInput__SWIG_0(swigCPtr, this, callToken, userInput, duration);
  }

  public boolean SendUserInput(String callToken, String userInput) {
    return OPALJNI.OpalContext_SendUserInput__SWIG_1(swigCPtr, this, callToken, userInput);
  }

}

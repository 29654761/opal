//------------------------------------------------------------------------------
// <auto-generated />
//
// This file was automatically generated by SWIG (http://www.swig.org).
// Version 4.0.1
//
// Do not make changes to this file unless you know what you are doing--modify
// the SWIG interface file instead.
//------------------------------------------------------------------------------


public class OpalStatusIVR : global::System.IDisposable {
  private global::System.Runtime.InteropServices.HandleRef swigCPtr;
  protected bool swigCMemOwn;

  internal OpalStatusIVR(global::System.IntPtr cPtr, bool cMemoryOwn) {
    swigCMemOwn = cMemoryOwn;
    swigCPtr = new global::System.Runtime.InteropServices.HandleRef(this, cPtr);
  }

  internal static global::System.Runtime.InteropServices.HandleRef getCPtr(OpalStatusIVR obj) {
    return (obj == null) ? new global::System.Runtime.InteropServices.HandleRef(null, global::System.IntPtr.Zero) : obj.swigCPtr;
  }

  ~OpalStatusIVR() {
    Dispose(false);
  }

  public void Dispose() {
    Dispose(true);
    global::System.GC.SuppressFinalize(this);
  }

  protected virtual void Dispose(bool disposing) {
    lock(this) {
      if (swigCPtr.Handle != global::System.IntPtr.Zero) {
        if (swigCMemOwn) {
          swigCMemOwn = false;
          OPALPINVOKE.delete_OpalStatusIVR(swigCPtr);
        }
        swigCPtr = new global::System.Runtime.InteropServices.HandleRef(null, global::System.IntPtr.Zero);
      }
    }
  }

  public string callToken {
    set {
      OPALPINVOKE.OpalStatusIVR_callToken_set(swigCPtr, value);
    } 
    get {
      string ret = OPALPINVOKE.OpalStatusIVR_callToken_get(swigCPtr);
      return ret;
    } 
  }

  public string variables {
    set {
      OPALPINVOKE.OpalStatusIVR_variables_set(swigCPtr, value);
    } 
    get {
      string ret = OPALPINVOKE.OpalStatusIVR_variables_get(swigCPtr);
      return ret;
    } 
  }

  public OpalStatusIVR() : this(OPALPINVOKE.new_OpalStatusIVR(), true) {
  }

}
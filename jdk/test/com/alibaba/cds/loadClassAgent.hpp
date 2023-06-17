#include "jvmti.h"

class AgentException {
public:
  AgentException(jvmtiError err) {
    m_error = err;
  }

  char* what() const throw() {
    return "AgentException";
  }

  jvmtiError ErrCode() const throw() {
    return m_error;
  }

private:
  jvmtiError m_error;
};


class LoadClassAgent {
public:
  LoadClassAgent() throw(AgentException){}

  ~LoadClassAgent() throw(AgentException);

  void Init(JavaVM *vm) const throw(AgentException);

  void RegisterEvent() const throw(AgentException);

  static void JNICALL HandleLoadClass(jvmtiEnv* jvmti, JNIEnv* jni, jthread thread, jobject cld);

private:
  static void CheckException(jvmtiError error) throw(AgentException) {
    if (error != JVMTI_ERROR_NONE) {
      throw AgentException(error);
    }
  }

  static jvmtiEnv * m_jvmti;
};

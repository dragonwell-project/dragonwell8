#include <iostream>

#include "loadClassAgent.hpp"
#include "jvmti.h"

using namespace std;

JNIEXPORT jint JNICALL Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {
  cout << "Agent_OnLoad(" << vm << ")" << endl;
  try {
    LoadClassAgent* agent = new LoadClassAgent();
    agent->Init(vm);
    agent->RegisterEvent();

  } catch (AgentException& e) {
    cout << "Error when enter HandleMethodEntry: " << e.what() << " [" << e.ErrCode() << "]";
    return JNI_ERR;
  }

  return JNI_OK;
}

JNIEXPORT void JNICALL Agent_OnUnload(JavaVM *vm) {
  cout << "Agent_OnUnload(" << vm << ")" << endl;
}

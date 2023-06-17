#include "jni.h"
#include "jni_util.h"
#include "jvm.h"
#include "jmm.h"
#include "com_alibaba_util_QuickStart.h"
#define THREAD "Ljava/lang/Thread;"
#define ARRAY_LENGTH(a) (sizeof(a)/sizeof(a[0]))

static JNINativeMethod methods[] = {
    {"notifyDump0",       "()V",              (void *)&JVM_NotifyDump },
};

JNIEXPORT void JNICALL
Java_com_alibaba_util_QuickStart_registerNatives(JNIEnv *env, jclass cls)
{
    (*env)->RegisterNatives(env, cls, methods, ARRAY_LENGTH(methods));
}
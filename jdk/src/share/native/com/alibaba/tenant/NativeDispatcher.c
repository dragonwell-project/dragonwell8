
#include "jni.h"
#include "jni_util.h"
#include "jvm.h"
#include "jmm.h"
#include "com_alibaba_tenant_NativeDispatcher.h"

#define TENANT "Lcom/alibaba/tenant/TenantContainer;"
#define THREAD "Ljava/lang/Thread;"
#define ARRAY_LENGTH(a) (sizeof(a)/sizeof(a[0]))

static const JmmInterface* jmm_interface = NULL;

static JNINativeMethod methods[] = {
    {"attach",                              "(" TENANT ")V",                      (void *)&JVM_AttachToTenant},
    {"createTenantAllocationContext",       "(" TENANT "J)V",                     (void *)&JVM_CreateTenantAllocationContext},
    {"destroyTenantAllocationContext",      "(J)V",                               (void *)&JVM_DestroyTenantAllocationContext},
    {"getTenantOccupiedMemory",             "(J)J",                               (void *)&JVM_GetTenantOccupiedMemory},
    {"containerOf",                         "(Ljava/lang/Object;)"TENANT,         (void *)&JVM_TenantContainerOf },
};

JNIEXPORT void JNICALL
Java_com_alibaba_tenant_NativeDispatcher_registerNatives0(JNIEnv *env, jclass cls)
{
    (*env)->RegisterNatives(env, cls, methods, ARRAY_LENGTH(methods));
}

JNIEXPORT void JNICALL
Java_com_alibaba_tenant_NativeDispatcher_getThreadsAllocatedMemory(JNIEnv *env,
                                                                jobject obj,
                                                                jlongArray ids,
                                                                jlongArray sizeArray)
{
    if (NULL == jmm_interface) {
        jmm_interface = (JmmInterface*) JVM_GetManagement(JMM_VERSION_1_0);
    }

    jmm_interface->GetThreadAllocatedMemory(env, ids, sizeArray);
}

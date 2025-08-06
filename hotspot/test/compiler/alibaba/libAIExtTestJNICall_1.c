/*
 * Copyright (c) 2025, Alibaba Group Holding Limited. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "aiext.h"

JNIEXPORT aiext_result_t JNICALL aiext_init(const aiext_env_t* env,
                                            aiext_handle_t handle) {
  return AIEXT_OK;
}

JNIEXPORT aiext_result_t JNICALL aiext_post_init(const aiext_env_t* env,
                                                 aiext_handle_t handle) {
  aiext_result_t result = AIEXT_ERROR;
  JNIEnv* jni = env->get_jni_env();
  if (jni == NULL) {
    return AIEXT_ERROR;
  }

  // Test JNI invocation.
  const char* test = "test_string";
  jstring str = (*jni)->NewStringUTF(jni, test);
  jsize len = (*jni)->GetStringUTFLength(jni, str);
  if (len != (jsize)strlen(test)) {
    return AIEXT_ERROR;
  } else {
    fprintf(stdout, "JNI call is success\n");
  }

  // Test Java method invocation.
  jclass testclass = (*jni)->FindClass(jni, "TestAIExtension$JNITestClass");
  if (testclass == NULL) {
    fprintf(stderr, "Can not find test class\n");
    return AIEXT_ERROR;
  }

  jmethodID m = (*jni)->GetStaticMethodID(jni, testclass, "testMethod", "()V");
  if (m == NULL) {
    fprintf(stderr, "Can not find method\n");
    return AIEXT_ERROR;
  }

  (*jni)->CallStaticVoidMethod(jni, testclass, m);
  return AIEXT_OK;
}

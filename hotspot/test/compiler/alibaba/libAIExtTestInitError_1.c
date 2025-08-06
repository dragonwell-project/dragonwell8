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

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include "aiext.h"

#define INIT_ERROR "init_error="
#define INIT_ERROR_LEN (sizeof(INIT_ERROR) - 1)
#define POST_INIT_ERROR "post_init_error="
#define POST_INIT_ERROR_LEN (sizeof(POST_INIT_ERROR) - 1)

#define MIN(x, y) ((x) < (y) ? (x) : (y))

static bool init_error = false, post_init_error = false;

// Parses command line arguments.
static void parse_param(const char* param, size_t len) {
  if (strncmp(param, INIT_ERROR, MIN(INIT_ERROR_LEN, len)) == 0) {
    // Parse `init_error`.
    param += INIT_ERROR_LEN;
    init_error = strncmp(param, "0", MIN(1, len)) != 0;
  } else if (strncmp(param, POST_INIT_ERROR, MIN(POST_INIT_ERROR_LEN, len)) ==
             0) {
    // Parse `post_init_error`.
    param += POST_INIT_ERROR_LEN;
    post_init_error = strncmp(param, "0", MIN(1, len)) != 0;
  }
}

// Parses parameter list.
static void parse_params(const char* params) {
  size_t len = strlen(params);
  const char* param = params;
  for (;;) {
    char* colon = strchr(param, ':');
    if (colon == NULL) {
      parse_param(param, len - (param - params));
      break;
    } else {
      parse_param(param, colon - param);
      param = colon + 1;
    }
  }
}

JNIEXPORT aiext_result_t JNICALL aiext_init(const aiext_env_t* env,
                                            aiext_handle_t handle) {
  // Parse parameter list.
  char params[128];
  aiext_result_t result =
      env->get_unit_info(handle, NULL, 0, NULL, 0, params, sizeof(params));
  if (result != AIEXT_OK) {
    printf("Failed to get unit info\n");
    return AIEXT_ERROR;
  }
  parse_params(params);

  // Check if we should return an error.
  if (init_error) {
    printf("Returning error in `aiext_init`...\n");
    return AIEXT_ERROR;
  }
  return AIEXT_OK;
}

JNIEXPORT aiext_result_t JNICALL aiext_post_init(const aiext_env_t* env,
                                                 aiext_handle_t handle) {
  if (post_init_error) {
    printf("Returning error in `aiext_post_init`...\n");
    return AIEXT_ERROR;
  }
  return AIEXT_OK;
}

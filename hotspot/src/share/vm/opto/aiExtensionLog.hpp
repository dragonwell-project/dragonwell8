/*
 * Copyright (c) 2026 Alibaba Group Holding Limited. All Rights Reserved.
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

#ifndef SHARE_OPTO_AIEXTENSIONLOG_HPP
#define SHARE_OPTO_AIEXTENSIONLOG_HPP

#include "runtime/globals.hpp"
#include "utilities/globalDefinitions.hpp"
#include "utilities/ostream.hpp"

inline void aiext_log(const char* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  tty->vprint_cr(fmt, args);
  va_end(args);
}

inline void aiext_dummy_log(const char*, ...) {}

#define log_info(x) (PrintAIExtensionDetails ? aiext_log : aiext_dummy_log)
#define log_error(x) (PrintAIExtensionDetails ? aiext_log : aiext_dummy_log)
#define log_is_enabled(...) PrintAIExtensionDetails

#endif  // SHARE_OPTO_AIEXTENSIONLOG_HPP

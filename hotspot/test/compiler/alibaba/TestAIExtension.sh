#!/bin/sh

#
#  Copyright (c) 2025, Alibaba Group Holding Limited. All Rights Reserved.
#  DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
#  This code is free software; you can redistribute it and/or modify it
#  under the terms of the GNU General Public License version 2 only, as
#  published by the Free Software Foundation.
#
#  This code is distributed in the hope that it will be useful, but WITHOUT
#  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
#  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
#  version 2 for more details (a copy is included in the LICENSE file that
#  accompanied this code).
#
#  You should have received a copy of the GNU General Public License version
#  2 along with this work; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
#
#  Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
#  or visit www.oracle.com if you need additional information or have any
#  questions.
#

if [ "${TESTSRC}" = "" ]
then
  echo "TESTSRC not set.  Test cannot execute.  Failed."
  exit 1
fi
echo "TESTSRC=${TESTSRC}"

. ${TESTSRC}/../../test_env.sh

if [ $VM_OS != "linux" ]; then
  echo "Requires linux."
  exit 1
fi

gcc_cmd=`which gcc`
if [ "x$gcc_cmd" = "x" ]; then
  echo "gcc not found. Cannot execute test."
  exit 1
fi

compile() {
  $gcc_cmd -O1 -DLINUX -fPIC -shared \
    -o $2 \
    -I${TESTJAVA}${FS}include \
    -I${TESTJAVA}${FS}include${FS}linux \
    $1
}

THIS_DIR=.

for src in ${TESTSRC}${FS}*.c; do
  src_file=`basename $src`
  lib=${THIS_DIR}${FS}${src_file%.c}.so
  compile $src $lib
done

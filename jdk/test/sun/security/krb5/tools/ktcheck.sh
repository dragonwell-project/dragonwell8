#
# Copyright (c) 2010, 2021, Oracle and/or its affiliates. All rights reserved.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# This code is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation.
#
# This code is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
# version 2 for more details (a copy is included in the LICENSE file that
# accompanied this code).
#
# You should have received a copy of the GNU General Public License version
# 2 along with this work; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
#
# Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
# or visit www.oracle.com if you need additional information or have any
# questions.
#

# @test
# @bug 6950546 8139348
# @summary "ktab -d name etype" to "ktab -d name [-e etype] [kvno | all | old]"
# @run shell ktcheck.sh
#

if [ "${TESTJAVA}" = "" ] ; then
  JAVAC_CMD=`which javac`
  TESTJAVA=`dirname $JAVAC_CMD`/..
fi

if [ "${TESTSRC}" = "" ] ; then
  TESTSRC="."
fi

OS=`uname -s`
case "$OS" in
  CYGWIN* )
    FS="/"
    ;;
  Windows_* )
    FS="\\"
    ;;
  * )
    FS="/"
    echo "Unsupported system!"
    exit 0;
    ;;
esac

KEYTAB=ktab.tmp

rm $KEYTAB
${TESTJAVA}${FS}bin${FS}javac -d . ${TESTSRC}${FS}KtabCheck.java

EXTRA_OPTIONS="-Djava.security.krb5.conf=${TESTSRC}${FS}onlythree.conf"
KTAB="${TESTJAVA}${FS}bin${FS}ktab -J${EXTRA_OPTIONS} -k $KEYTAB -f"
CHECK="${TESTJAVA}${FS}bin${FS}java ${TESTVMOPTS} ${EXTRA_OPTIONS} KtabCheck $KEYTAB"

echo ${EXTRA_OPTIONS}

# This test uses a krb5.conf file (onlythree.conf) in which
# only 2 etypes in the default_tkt_enctypes setting are enabled
# by default: aes128-cts(17), aes256-cts(18).

$KTAB -a me mine
$CHECK 1 17 1 18 || exit 1
$KTAB -a me mine -n 0
$CHECK 0 17 0 18 || exit 1
$KTAB -a me mine -n 1 -append
$CHECK 0 17 0 18 1 17 1 18 || exit 1
$KTAB -a me mine -append
$CHECK 0 17 0 18 1 17 1 18 2 17 2 18 || exit 1
$KTAB -a me mine
$CHECK 3 17 3 18 || exit 1
$KTAB -a me mine -n 4 -append
$CHECK 3 17 3 18 4 17 4 18 || exit 1
$KTAB -a me mine -n 5 -append
$CHECK 3 17 3 18 4 17 4 18 5 17 5 18 || exit 1
$KTAB -a me mine -n 6 -append
$CHECK 3 17 3 18 4 17 4 18 5 17 5 18 6 17 6 18 || exit 1
$KTAB -d me 3
$CHECK 4 17 4 18 5 17 5 18 6 17 6 18 || exit 1
$KTAB -d me -e 17 6
$CHECK 4 17 4 18 5 17 5 18 6 18 || exit 1
$KTAB -d me -e 17 5
$CHECK 4 17 4 18 5 18 6 18 || exit 1
$KTAB -d me old
$CHECK 4 17 6 18 || exit 1
$KTAB -d me old
$CHECK 4 17 6 18 || exit 1
$KTAB -d me
$CHECK || exit 1

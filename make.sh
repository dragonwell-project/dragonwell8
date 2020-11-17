#!/bin/bash
# build properties
#
# Copyright (c) 2019 Alibaba Group Holding Limited. All Rights Reserved.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# This code is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation. Alibaba designates this
# particular file as subject to the "Classpath" exception as provided
# by Oracle in the LICENSE file that accompanied this code.
#
# This code is distributed in the hope that it will be useful, but WITHOUT
# ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
# FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
# version 2 for more details (a copy is included in the LICENSE file that
# accompanied this code).
#
# You should have received a copy of the GNU General Public License version
# 2 along with this work; if not, write to the Free Software Foundation,
# Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
#

if [ ! -f "dragonwell_version" ]; then
	echo "File 'dragonwell_version' doesn't exist!"
	exit 1;
fi
source ./dragonwell_version


if [ $# -lt 1 ]; then
  echo "USAGE: $0 release/debug/fastdebug"
fi

ARCH=$(uname -m)
if [ "x"$ARCH = 'xx86_64' ]; then
  ARCH_DIR="amd64"
elif [ "x"$ARCH = 'xaarch64' ]; then
  ARCH_DIR="aarch64"
else
  echo "Unrecognized architecture: ${ARCH}"
  exit 1
fi


LC_ALL=C
BUILD_MODE=$1

case "$BUILD_MODE" in
    release)
        DEBUG_LEVEL="release"
        JDK_IMAGES_DIR=$(pwd)/build/linux-${ARCH}-normal-server-release/images
    ;;
    debug)
        DEBUG_LEVEL="slowdebug"
        JDK_IMAGES_DIR=$(pwd)/build/linux-${ARCH}-normal-server-slowdebug/images
    ;;
    fastdebug)
	DEBUG_LEVEL="fastdebug"
	JDK_IMAGES_DIR=$(pwd)/build/linux-${ARCH}-normal-server-fastdebug/images
    ;;
    *)
        echo "Argument must be release or debug or fastdebug!"
        exit 1
    ;;
esac

NEW_JAVA_HOME=$JDK_IMAGES_DIR/j2sdk-image
NEW_JRE_HOME=$JDK_IMAGES_DIR/j2re-image

\rm -rf build

if [ -z "$BUILD_NUMBER" ]; then
  BUILD_INDEX=b00
else
  BUILD_INDEX=b$BUILD_NUMBER
fi

DISTRO_VERSION=${DRAGONWELL_VERSION}

shift

bash ./configure --with-milestone=fcs \
                 --with-build-number=${BUILD_INDEX} \
                 --with-user-release-suffix="" \
                 --enable-unlimited-crypto \
                 --with-cacerts-file=$(pwd)/common/security/cacerts \
                 --with-jvm-variants=server \
                 --with-debug-level=$DEBUG_LEVEL \
                 --with-zlib=system $*
make clean

# The default DISTRO_VERSION in spec.gmk.in does not contain the customer defined build number.
# Hack DISTRO_VERSION to introduce the number.
make LOG=debug DISTRO_VERSION=${DISTRO_VERSION} images

# Sanity tests
JAVA_EXES=("$NEW_JAVA_HOME/bin/java" "$NEW_JAVA_HOME/jre/bin/java" "$NEW_JRE_HOME/bin/java")
VERSION_OPTS=("-version" "-Xinternalversion" "-fullversion")
for exe in "${JAVA_EXES[@]}"; do
  for opt in "${VERSION_OPTS[@]}"; do
    $exe $opt > /dev/null 2>&1
    if [ 0 -ne $? ]; then
      echo "Failed: $exe $opt"
      exit 128
    fi
  done
done

# Keep old output
$NEW_JAVA_HOME/bin/java -version

cat > /tmp/systemProperty.java << EOF
public class systemProperty {
    public static void main(String[] args) {
        System.getProperties().list(System.out);
    }
}
EOF

$NEW_JAVA_HOME/bin/javac /tmp/systemProperty.java
$NEW_JAVA_HOME/bin/java -cp /tmp/ systemProperty > /tmp/systemProperty.out

EXPECTED_PATTERN=('^java\.vm\.vendor\=.*Alibaba.*$'
                '^java\.vendor\.url\=http\:\/\/www\.alibabagroup\.com$'
                '^java\.vendor\=Alibaba$'
                '^java\.vendor\.url\.bug\=mailto\:dragonwell_use@googlegroups\.com$')
RET=0
for p in ${EXPECTED_PATTERN[*]}
do
    cat /tmp/systemProperty.out | grep "$p"
    if [ 0 != $? ]; then RET=1; fi
done

\rm -f /tmp/systemProperty*

# check version string
$NEW_JAVA_HOME/bin/java -version > /tmp/version.out 2>&1
grep "^OpenJDK Runtime" /tmp/version.out | grep "(Alibaba Dragonwell $DISTRO_VERSION)"
if [ 0 != $? ]; then RET=1; fi
grep "^OpenJDK .*VM" /tmp/version.out | grep "(Alibaba Dragonwell $DISTRO_VERSION)"
if [ 0 != $? ]; then RET=1; fi
\rm -f /tmp/version.out

ldd $NEW_JAVA_HOME/jre/lib/${ARCH_DIR}/libzip.so|grep libz
if [ 0 != $? ]; then RET=1; fi

exit $RET

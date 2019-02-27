#!/bin/bash
# build properties
JDK_UPDATE_VERSION=202
DISTRO_NAME=Dragonwell
DISTRO_VERSION=8.0

if [ $# != 1 ]; then 
  echo "USAGE: $0 release/debug"
fi

ps -e | grep docker
if [ $? -eq 0 ]; then
    echo "We will build AJDK in Docker!"
    sudo docker pull reg.docker.alibaba-inc.com/ajdk/8-dev.alios5
    docker run -u admin -i --rm -e BUILD_NUMBER=$BUILD_NUMBER -v `pwd`:`pwd` -w `pwd` \
               --entrypoint=bash reg.docker.alibaba-inc.com/ajdk/8-dev.alios5 `pwd`/make.sh $1
    exit $?
fi


source /vmfarm/tools/env.sh
LC_ALL=C

BUILD_MODE=$1

case "$BUILD_MODE" in
    release)
        DEBUG_LEVEL="release"
        JDK_IMAGES_DIR=`pwd`/build/linux-x86_64-normal-server-release/images
    ;;
    debug)
        DEBUG_LEVEL="slowdebug"
        JDK_IMAGES_DIR=`pwd`/build/linux-x86_64-normal-server-slowdebug/images
    ;;
    *)
        echo "Argument must be release or debug!"
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

export LDFLAGS_JDK="-L/vmfarm/tools/jemalloc/lib -ljemalloc"
bash ./configure --with-milestone=fcs \
                 --with-freetype=/vmfarm/tools/freetype/ \
                 --with-update-version=$JDK_UPDATE_VERSION \
                 --with-build-number=$BUILD_INDEX \
                 --with-user-release-suffix="" \
                 --enable-unlimited-crypto \
                 --with-cacerts-file=/vmfarm/security/cacerts \
                 --with-jtreg=/vmfarm/tools/jtreg4.1 \
                 --with-tools-dir=/vmfarm/tools/install/gcc-4.4.7/bin \
                 --with-jvm-variants=server \
                 --with-debug-level=$DEBUG_LEVEL \
                 --with-extra-cflags="-DVENDOR='\"Alibaba\"'                                         \
                                      -DVENDOR_URL='\"http://www.alibabagroup.com\"'                 \
                                      -DVENDOR_URL_BUG='\"mailto:jvm@list.alibaba-inc.com\"'"        \
                 --with-zlib=system \
                 --with-extra-ldflags="-Wl,--build-id=sha1"
make clean
make LOG=debug DISTRO_NAME=$DISTRO_NAME DISTRO_VERSION=$DISTRO_VERSION COMPANY_NAME=Alibaba images

\cp -f /vmfarm/tools/hsdis/8/amd64/hsdis-amd64.so  $NEW_JAVA_HOME/jre/lib/amd64/
\cp -f /vmfarm/tools/jemalloc/lib/libjemalloc.so.2 $NEW_JAVA_HOME/jre/lib/amd64/
\cp -f /vmfarm/tools/jemalloc/lib/libjemalloc.so.2 $NEW_JAVA_HOME/lib/amd64/
\cp -f /vmfarm/tools/jemalloc/lib/libjemalloc.so.2 $NEW_JRE_HOME/lib/amd64/

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
                '^java\.vendor\.url\.bug\=mailto\:jvm@list\.alibaba-inc\.com$')
RET=0
for p in ${EXPECTED_PATTERN[*]}
do
    cat /tmp/systemProperty.out | grep "$p"
    if [ 0 != $? ]; then RET=1; fi
done

\rm -f /tmp/systemProperty*

# check version string
$NEW_JAVA_HOME/bin/java -version > /tmp/version.out 2>&1
grep "^OpenJDK Runtime" /tmp/version.out | grep "($DISTRO_NAME $DISTRO_VERSION)"
if [ 0 != $? ]; then RET=1; fi
grep "^OpenJDK .*VM" /tmp/version.out | grep "($DISTRO_NAME $DISTRO_VERSION)"
if [ 0 != $? ]; then RET=1; fi
\rm -f /tmp/version.out

ldd $NEW_JAVA_HOME/jre/lib/amd64/libzip.so|grep libz
if [ 0 != $? ]; then RET=1; fi

exit $RET


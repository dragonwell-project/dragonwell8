#
# Copyright (c) 2011, 2018, Oracle and/or its affiliates. All rights reserved.
# DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
#
# This code is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License version 2 only, as
# published by the Free Software Foundation.  Oracle designates this
# particular file as subject to the "Classpath" exception as provided
# by Oracle in the LICENSE file that accompanied this code.
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

########################################################################
# This file is responsible for detecting, verifying and setting up the 
# toolchain, i.e. the compiler, linker and related utilities. It will setup 
# proper paths to the binaries, but it will not setup any flags.
#
# The binaries used is determined by the toolchain type, which is the family of 
# compilers and related tools that are used.
########################################################################


# All valid toolchains, regardless of platform (used by help.m4)
VALID_TOOLCHAINS_all="gcc clang solstudio xlc microsoft"

# These toolchains are valid on different platforms
VALID_TOOLCHAINS_linux="gcc clang"
VALID_TOOLCHAINS_solaris="solstudio"
VALID_TOOLCHAINS_macosx="gcc clang"
VALID_TOOLCHAINS_aix="xlc"
VALID_TOOLCHAINS_windows="microsoft"

# Toolchain descriptions
TOOLCHAIN_DESCRIPTION_clang="clang/LLVM"
TOOLCHAIN_DESCRIPTION_gcc="GNU Compiler Collection"
TOOLCHAIN_DESCRIPTION_microsoft="Microsoft Visual Studio"
TOOLCHAIN_DESCRIPTION_solstudio="Oracle Solaris Studio"
TOOLCHAIN_DESCRIPTION_xlc="IBM XL C/C++"

# Prepare the system so that TOOLCHAIN_CHECK_COMPILER_VERSION can be called.
# Must have CC_VERSION_NUMBER and CXX_VERSION_NUMBER.
# $1 - optional variable prefix for compiler and version variables (BUILD_)
# $2 - optional variable prefix for comparable variable (OPENJDK_BUILD_)
AC_DEFUN([TOOLCHAIN_PREPARE_FOR_VERSION_COMPARISONS],
[
  if test "x$CC_VERSION_NUMBER" != "x$CXX_VERSION_NUMBER"; then
    AC_MSG_WARN([C and C++ compiler has different version numbers, $CC_VERSION_NUMBER vs $CXX_VERSION_NUMBER.])
    AC_MSG_WARN([This typically indicates a broken setup, and is not supported])
  fi

  # We only check CC_VERSION_NUMBER since we assume CXX_VERSION_NUMBER is equal.
  if [ [[ "$CC_VERSION_NUMBER" =~ (.*\.){3} ]] ]; then
    AC_MSG_WARN([C compiler version number has more than three parts (X.Y.Z): $CC_VERSION. Comparisons might be wrong.])
  fi

  if [ [[  "$CC_VERSION_NUMBER" =~ [0-9]{6} ]] ]; then
    AC_MSG_WARN([C compiler version number has a part larger than 99999: $CC_VERSION. Comparisons might be wrong.])
  fi

  $2COMPARABLE_ACTUAL_VERSION=`$AWK -F. '{ printf("%05d%05d%05d\n", [$]1, [$]2, [$]3) }' <<< "$CC_VERSION_NUMBER"`
])

# Check if the configured compiler (C and C++) is of a specific version or
# newer. TOOLCHAIN_PREPARE_FOR_VERSION_COMPARISONS must have been called before.
#
# Arguments:
#   $1:   The version string to check against the found version
#   $2:   block to run if the compiler is at least this version (>=)
#   $3:   block to run if the compiler is older than this version (<)
AC_DEFUN([TOOLCHAIN_CHECK_COMPILER_VERSION],
[
  REFERENCE_VERSION=$1

  if [ [[ "$REFERENCE_VERSION" =~ (.*\.){3} ]] ]; then
    AC_MSG_ERROR([Internal error: Cannot compare to $REFERENCE_VERSION, only three parts (X.Y.Z) is supported])
  fi

  if [ [[ "$REFERENCE_VERSION" =~ [0-9]{6} ]] ]; then
    AC_MSG_ERROR([Internal error: Cannot compare to $REFERENCE_VERSION, only parts < 99999 is supported])
  fi

  # Version comparison method inspired by http://stackoverflow.com/a/24067243
  COMPARABLE_REFERENCE_VERSION=`$AWK -F. '{ printf("%05d%05d%05d\n", [$]1, [$]2, [$]3) }' <<< "$REFERENCE_VERSION"`

  if test $COMPARABLE_ACTUAL_VERSION -ge $COMPARABLE_REFERENCE_VERSION ; then
    m4_ifval([$2], [$2], [:])
  else
    m4_ifval([$3], [$3], [:])
  fi
])


# Setup a number of variables describing how native output files are
# named on this platform/toolchain.
AC_DEFUN([TOOLCHAIN_SETUP_FILENAME_PATTERNS],
[
  # Define filename patterns
  if test "x$OPENJDK_TARGET_OS" = xwindows; then
    LIBRARY_PREFIX=
    SHARED_LIBRARY_SUFFIX='.dll'
    STATIC_LIBRARY_SUFFIX='.lib'
    SHARED_LIBRARY='[$]1.dll'
    STATIC_LIBRARY='[$]1.lib'
    OBJ_SUFFIX='.obj'
    EXE_SUFFIX='.exe'
  else
    LIBRARY_PREFIX=lib
    SHARED_LIBRARY_SUFFIX='.so'
    STATIC_LIBRARY_SUFFIX='.a'
    SHARED_LIBRARY='lib[$]1.so'
    STATIC_LIBRARY='lib[$]1.a'
    OBJ_SUFFIX='.o'
    EXE_SUFFIX=''
    if test "x$OPENJDK_TARGET_OS" = xmacosx; then
      SHARED_LIBRARY='lib[$]1.dylib'
      SHARED_LIBRARY_SUFFIX='.dylib'
    fi
  fi

  AC_SUBST(LIBRARY_PREFIX)
  AC_SUBST(SHARED_LIBRARY_SUFFIX)
  AC_SUBST(STATIC_LIBRARY_SUFFIX)
  AC_SUBST(SHARED_LIBRARY)
  AC_SUBST(STATIC_LIBRARY)
  AC_SUBST(OBJ_SUFFIX)
  AC_SUBST(EXE_SUFFIX)  
])

# Determine which toolchain type to use, and make sure it is valid for this
# platform. Setup various information about the selected toolchain.
AC_DEFUN_ONCE([TOOLCHAIN_DETERMINE_TOOLCHAIN_TYPE],
[
  AC_ARG_WITH(toolchain-type, [AS_HELP_STRING([--with-toolchain-type],
      [the toolchain type (or family) to use, use '--help' to show possible values @<:@platform dependent@:>@])])

  # Use indirect variable referencing
  toolchain_var_name=VALID_TOOLCHAINS_$OPENJDK_BUILD_OS
  VALID_TOOLCHAINS=${!toolchain_var_name}
 
  if test "x$OPENJDK_TARGET_OS" = xmacosx; then
    # On Mac OS X, default toolchain to clang after Xcode 5
    XCODE_VERSION_OUTPUT=`xcodebuild -version 2>&1 | $HEAD -n 1`
    $ECHO "$XCODE_VERSION_OUTPUT" | $GREP "Xcode " > /dev/null
    if test $? -ne 0; then
      AC_MSG_ERROR([Failed to determine Xcode version.])
    fi
    XCODE_MAJOR_VERSION=`$ECHO $XCODE_VERSION_OUTPUT | \
        $SED -e 's/^Xcode \(@<:@1-9@:>@@<:@0-9.@:>@*\)/\1/' | \
        $CUT -f 1 -d .`
    AC_MSG_NOTICE([Xcode major version: $XCODE_MAJOR_VERSION])
    if test $XCODE_MAJOR_VERSION -ge 5; then
        DEFAULT_TOOLCHAIN="clang"
    else
        DEFAULT_TOOLCHAIN="gcc"
    fi
  else
    # First toolchain type in the list is the default
    DEFAULT_TOOLCHAIN=${VALID_TOOLCHAINS%% *}
  fi

  if test "x$with_toolchain_type" = xlist; then
    # List all toolchains
    AC_MSG_NOTICE([The following toolchains are valid on this platform:])
    for toolchain in $VALID_TOOLCHAINS; do
      toolchain_var_name=TOOLCHAIN_DESCRIPTION_$toolchain
      TOOLCHAIN_DESCRIPTION=${!toolchain_var_name}
      $PRINTF "  %-10s  %s\n" $toolchain "$TOOLCHAIN_DESCRIPTION"
    done
    
    exit 0
  elif test "x$with_toolchain_type" != x; then
    # User override; check that it is valid
    if test "x${VALID_TOOLCHAINS/$with_toolchain_type/}" = "x${VALID_TOOLCHAINS}"; then
      AC_MSG_NOTICE([Toolchain type $with_toolchain_type is not valid on this platform.])
      AC_MSG_NOTICE([Valid toolchains: $VALID_TOOLCHAINS.])
      AC_MSG_ERROR([Cannot continue.])
    fi
    TOOLCHAIN_TYPE=$with_toolchain_type
  else
    # No flag given, use default
    TOOLCHAIN_TYPE=$DEFAULT_TOOLCHAIN
  fi
  AC_SUBST(TOOLCHAIN_TYPE)

  TOOLCHAIN_CC_BINARY_clang="clang"
  TOOLCHAIN_CC_BINARY_gcc="gcc"
  TOOLCHAIN_CC_BINARY_microsoft="cl"
  TOOLCHAIN_CC_BINARY_solstudio="cc"
  TOOLCHAIN_CC_BINARY_xlc="xlc_r"

  TOOLCHAIN_CXX_BINARY_clang="clang++"
  TOOLCHAIN_CXX_BINARY_gcc="g++"
  TOOLCHAIN_CXX_BINARY_microsoft="cl"
  TOOLCHAIN_CXX_BINARY_solstudio="CC"
  TOOLCHAIN_CXX_BINARY_xlc="xlC_r"

  # Use indirect variable referencing
  toolchain_var_name=TOOLCHAIN_DESCRIPTION_$TOOLCHAIN_TYPE
  TOOLCHAIN_DESCRIPTION=${!toolchain_var_name}
  toolchain_var_name=TOOLCHAIN_CC_BINARY_$TOOLCHAIN_TYPE
  TOOLCHAIN_CC_BINARY=${!toolchain_var_name}
  toolchain_var_name=TOOLCHAIN_CXX_BINARY_$TOOLCHAIN_TYPE
  TOOLCHAIN_CXX_BINARY=${!toolchain_var_name}

  TOOLCHAIN_SETUP_FILENAME_PATTERNS

  if test "x$TOOLCHAIN_TYPE" = "x$DEFAULT_TOOLCHAIN"; then
    AC_MSG_NOTICE([Using default toolchain $TOOLCHAIN_TYPE ($TOOLCHAIN_DESCRIPTION)])
  else
    AC_MSG_NOTICE([Using user selected toolchain $TOOLCHAIN_TYPE ($TOOLCHAIN_DESCRIPTION). Default toolchain is $DEFAULT_TOOLCHAIN.])
  fi 
])

# Before we start detecting the toolchain executables, we might need some 
# special setup, e.g. additional paths etc.
AC_DEFUN_ONCE([TOOLCHAIN_PRE_DETECTION],
[
  # FIXME: Is this needed?
  AC_LANG(C++)

  # Store the CFLAGS etal passed to the configure script.
  ORG_CFLAGS="$CFLAGS"
  ORG_CXXFLAGS="$CXXFLAGS"
  ORG_OBJCFLAGS="$OBJCFLAGS"

  # autoconf magic only relies on PATH, so update it if tools dir is specified
  OLD_PATH="$PATH"

  # On Windows, we need to detect the visual studio installation first.
  # This will change the PATH, but we need to keep that new PATH even 
  # after toolchain detection is done, since the compiler (on x86) uses
  # it for DLL resolution in runtime.
  if test "x$OPENJDK_BUILD_OS" = "xwindows" \
      && test "x$TOOLCHAIN_TYPE" = "xmicrosoft"; then
    TOOLCHAIN_SETUP_VISUAL_STUDIO_ENV
    # Reset path to VS_PATH. It will include everything that was on PATH at the time we
    # ran TOOLCHAIN_SETUP_VISUAL_STUDIO_ENV.
    PATH="$VS_PATH"
    # The microsoft toolchain also requires INCLUDE and LIB to be set.
    export INCLUDE="$VS_INCLUDE"
    export LIB="$VS_LIB"
  fi

  # Before we locate the compilers, we need to sanitize the Xcode build environment
  if test "x$OPENJDK_TARGET_OS" = "xmacosx"; then
    # determine path to Xcode developer directory
    # can be empty in which case all the tools will rely on a sane Xcode installation
    SET_DEVELOPER_DIR=

    if test -n "$XCODE_PATH"; then
      DEVELOPER_DIR="$XCODE_PATH"/Contents/Developer
    fi

    # DEVELOPER_DIR could also be provided directly
    AC_MSG_CHECKING([Determining if we need to set DEVELOPER_DIR])
    if test -n "$DEVELOPER_DIR"; then
      if test ! -d "$DEVELOPER_DIR"; then
        AC_MSG_ERROR([Xcode Developer path does not exist: $DEVELOPER_DIR, please provide a path to the Xcode application bundle using --with-xcode-path])
      fi
      if test ! -f "$DEVELOPER_DIR"/usr/bin/xcodebuild; then
        AC_MSG_ERROR([Xcode Developer path is not valid: $DEVELOPER_DIR, it must point to Contents/Developer inside an Xcode application bundle])
      fi
      # make it visible to all the tools immediately
      export DEVELOPER_DIR
      SET_DEVELOPER_DIR="export DEVELOPER_DIR := $DEVELOPER_DIR"
      AC_MSG_RESULT([yes ($DEVELOPER_DIR)])
    else
      AC_MSG_RESULT([no])
    fi
    AC_SUBST(SET_DEVELOPER_DIR)

    AC_PATH_PROG(XCODEBUILD, xcodebuild)
    if test -z "$XCODEBUILD"; then
      AC_MSG_ERROR([The xcodebuild tool was not found, the Xcode command line tools are required to build on Mac OS X])
    fi

    # Some versions of Xcode command line tools install gcc and g++ as symlinks to
    # clang and clang++, which will break the build. So handle that here if we need to.
    if test -L "/usr/bin/gcc" -o -L "/usr/bin/g++"; then
      # use xcrun to find the real gcc and add it's directory to PATH
      # then autoconf magic will find it
      AC_MSG_NOTICE([Found gcc symlinks to clang in /usr/bin, adding path to real gcc to PATH])
      XCODE_BIN_PATH=$(dirname `xcrun -find gcc`)
      PATH="$XCODE_BIN_PATH":$PATH
    fi

    # Determine appropriate SDKPATH, don't use SDKROOT as it interferes with the stub tools
    AC_MSG_CHECKING([Determining Xcode SDK path])
    # allow SDKNAME to be set to override the default SDK selection
    SDKPATH=`"$XCODEBUILD" -sdk ${SDKNAME:-macosx} -version | grep '^Path: ' | sed 's/Path: //'`
    if test -n "$SDKPATH"; then
      AC_MSG_RESULT([$SDKPATH])
    else
      AC_MSG_RESULT([(none, will use system headers and frameworks)])
    fi
    AC_SUBST(SDKPATH)

    # Perform a basic sanity test
    if test ! -f "$SDKPATH/System/Library/Frameworks/Foundation.framework/Headers/Foundation.h"; then
      AC_MSG_ERROR([Unable to find required framework headers, provide a valid path to Xcode using --with-xcode-path])
    fi

    # if SDKPATH is non-empty then we need to add -isysroot and -iframework for gcc and g++
    if test -n "$SDKPATH"; then
      # We need -isysroot <path> and -iframework<path>/System/Library/Frameworks
      CFLAGS_JDK="${CFLAGS_JDK} -isysroot \"$SDKPATH\" -iframework\"$SDKPATH/System/Library/Frameworks\""
      CXXFLAGS_JDK="${CXXFLAGS_JDK} -isysroot \"$SDKPATH\" -iframework\"$SDKPATH/System/Library/Frameworks\""
      LDFLAGS_JDK="${LDFLAGS_JDK} -isysroot \"$SDKPATH\" -iframework\"$SDKPATH/System/Library/Frameworks\""
    fi

    if test -d "$SDKPATH/System/Library/Frameworks/JavaVM.framework/Frameworks" ; then
      # These always need to be set on macOS 10.X, or we can't find the frameworks embedded in JavaVM.framework
      # set this here so it doesn't have to be peppered throughout the forest
      CFLAGS_JDK="$CFLAGS_JDK -F\"$SDKPATH/System/Library/Frameworks/JavaVM.framework/Frameworks\""
      CXXFLAGS_JDK="$CXXFLAGS_JDK -F\"$SDKPATH/System/Library/Frameworks/JavaVM.framework/Frameworks\""
      LDFLAGS_JDK="$LDFLAGS_JDK -F\"$SDKPATH/System/Library/Frameworks/JavaVM.framework/Frameworks\""
    fi
  fi

  # For solaris we really need solaris tools, and not the GNU equivalent.
  # The build tools on Solaris reside in /usr/ccs (C Compilation System),
  # so add that to path before starting to probe.
  # FIXME: This was originally only done for AS,NM,GNM,STRIP,MCS,OBJCOPY,OBJDUMP.
  if test "x$OPENJDK_BUILD_OS" = xsolaris; then
    PATH="/usr/ccs/bin:$PATH"
  fi

  # Finally add TOOLCHAIN_PATH at the beginning, to allow --with-tools-dir to 
  # override all other locations.
  if test "x$TOOLCHAIN_PATH" != x; then
    PATH=$TOOLCHAIN_PATH:$PATH
  fi
])

# Restore path, etc
AC_DEFUN_ONCE([TOOLCHAIN_POST_DETECTION],
[
  # Restore old path, except for the microsoft toolchain, which requires VS_PATH
  # to remain in place. Otherwise the compiler will not work in some situations
  # in later configure checks.
  if test "x$TOOLCHAIN_TYPE" != "xmicrosoft"; then
    PATH="$OLD_PATH"
  fi

  # Restore the flags to the user specified values.
  # This is necessary since AC_PROG_CC defaults CFLAGS to "-g -O2"
  CFLAGS="$ORG_CFLAGS"
  CXXFLAGS="$ORG_CXXFLAGS"
  OBJCFLAGS="$ORG_OBJCFLAGS"
])

# Check if a compiler is of the toolchain type we expect, and save the version
# information from it. If the compiler does not match the expected type,
# this function will abort using AC_MSG_ERROR. If it matches, the version will
# be stored in CC_VERSION_NUMBER/CXX_VERSION_NUMBER (as a dotted number), and
# the full version string in CC_VERSION_STRING/CXX_VERSION_STRING.
#
# $1 = compiler to test (CC or CXX)
# $2 = human readable name of compiler (C or C++)
AC_DEFUN([TOOLCHAIN_EXTRACT_COMPILER_VERSION],
[
  COMPILER=[$]$1
  COMPILER_NAME=$2

  if test "x$TOOLCHAIN_TYPE" = xsolstudio; then
    # cc -V output typically looks like
    #     cc: Sun C 5.12 Linux_i386 2011/11/16
    COMPILER_VERSION_OUTPUT=`$COMPILER -V 2>&1`
    # Check that this is likely to be the Solaris Studio cc.
    $ECHO "$COMPILER_VERSION_OUTPUT" | $GREP "^.*: Sun $COMPILER_NAME" > /dev/null
    if test $? -ne 0; then
      ALT_VERSION_OUTPUT=`$COMPILER --version 2>&1`
      AC_MSG_NOTICE([The $COMPILER_NAME compiler (located as $COMPILER) does not seem to be the required $TOOLCHAIN_TYPE compiler.])
      AC_MSG_NOTICE([The result from running with -V was: "$COMPILER_VERSION_OUTPUT"])
      AC_MSG_NOTICE([The result from running with --version was: "$ALT_VERSION_OUTPUT"])
      AC_MSG_ERROR([A $TOOLCHAIN_TYPE compiler is required. Try setting --with-tools-dir.])
    fi
    # Remove usage instructions (if present), and
    # collapse compiler output into a single line
    COMPILER_VERSION_STRING=`$ECHO $COMPILER_VERSION_OUTPUT | \
        $SED -e 's/ *@<:@Uu@:>@sage:.*//'`
    COMPILER_VERSION_NUMBER=`$ECHO $COMPILER_VERSION_OUTPUT | \
        $SED -e "s/^.*@<:@ ,\t@:>@$COMPILER_NAME@<:@ ,\t@:>@\(@<:@1-9@:>@\.@<:@0-9@:>@@<:@0-9@:>@*\).*/\1/"`
  elif test  "x$TOOLCHAIN_TYPE" = xxlc; then
    # xlc -qversion output typically looks like
    #     IBM XL C/C++ for AIX, V11.1 (5724-X13)
    #     Version: 11.01.0000.0015
    COMPILER_VERSION_OUTPUT=`$COMPILER -qversion 2>&1`
    # Check that this is likely to be the IBM XL C compiler.
    $ECHO "$COMPILER_VERSION_OUTPUT" | $GREP "IBM XL C" > /dev/null
    if test $? -ne 0; then
      ALT_VERSION_OUTPUT=`$COMPILER --version 2>&1`
      AC_MSG_NOTICE([The $COMPILER_NAME compiler (located as $COMPILER) does not seem to be the required $TOOLCHAIN_TYPE compiler.])
      AC_MSG_NOTICE([The result from running with -qversion was: "$COMPILER_VERSION_OUTPUT"])
      AC_MSG_NOTICE([The result from running with --version was: "$ALT_VERSION_OUTPUT"])
      AC_MSG_ERROR([A $TOOLCHAIN_TYPE compiler is required. Try setting --with-tools-dir.])
    fi
    # Collapse compiler output into a single line
    COMPILER_VERSION_STRING=`$ECHO $COMPILER_VERSION_OUTPUT`
    COMPILER_VERSION_NUMBER=`$ECHO $COMPILER_VERSION_OUTPUT | \
        $SED -e 's/^.*, V\(@<:@1-9@:>@@<:@0-9.@:>@*\).*$/\1/'`
  elif test  "x$TOOLCHAIN_TYPE" = xmicrosoft; then
    # There is no specific version flag, but all output starts with a version string.
    # First line typically looks something like:
    # Microsoft (R) 32-bit C/C++ Optimizing Compiler Version 16.00.40219.01 for 80x86
    COMPILER_VERSION_OUTPUT=`$COMPILER 2>&1 | $HEAD -n 1 | $TR -d '\r'`
    # Check that this is likely to be Microsoft CL.EXE.
    $ECHO "$COMPILER_VERSION_OUTPUT" | $GREP "Microsoft.*Compiler" > /dev/null
    if test $? -ne 0; then
      AC_MSG_NOTICE([The $COMPILER_NAME compiler (located as $COMPILER) does not seem to be the required $TOOLCHAIN_TYPE compiler.])
      AC_MSG_NOTICE([The result from running it was: "$COMPILER_VERSION_OUTPUT"])
      AC_MSG_ERROR([A $TOOLCHAIN_TYPE compiler is required. Try setting --with-tools-dir.])
    fi
    # Collapse compiler output into a single line
    COMPILER_VERSION_STRING=`$ECHO $COMPILER_VERSION_OUTPUT`
    COMPILER_VERSION_NUMBER=`$ECHO $COMPILER_VERSION_OUTPUT | \
        $SED -e 's/^.*ersion.\(@<:@1-9@:>@@<:@0-9.@:>@*\) .*$/\1/'`
  elif test  "x$TOOLCHAIN_TYPE" = xgcc; then
    # gcc --version output typically looks like
    #     gcc (Ubuntu/Linaro 4.8.1-10ubuntu9) 4.8.1
    #     Copyright (C) 2013 Free Software Foundation, Inc.
    #     This is free software; see the source for copying conditions.  There is NO
    #     warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
    COMPILER_VERSION_OUTPUT=`$COMPILER --version 2>&1`
    # Check that this is likely to be GCC.
    $ECHO "$COMPILER_VERSION_OUTPUT" | $GREP "Free Software Foundation" > /dev/null
    if test $? -ne 0; then
      AC_MSG_NOTICE([The $COMPILER_NAME compiler (located as $COMPILER) does not seem to be the required $TOOLCHAIN_TYPE compiler.])
      AC_MSG_NOTICE([The result from running with --version was: "$COMPILER_VERSION"])
      AC_MSG_ERROR([A $TOOLCHAIN_TYPE compiler is required. Try setting --with-tools-dir.])
    fi
    # Remove Copyright and legalese from version string, and
    # collapse into a single line
    COMPILER_VERSION_STRING=`$ECHO $COMPILER_VERSION_OUTPUT | \
        $SED -e 's/ *Copyright .*//'`
    COMPILER_VERSION_NUMBER=`$ECHO $COMPILER_VERSION_OUTPUT | \
        $SED -e 's/^.* \(@<:@1-9@:>@@<:@0-9@:>@*\.@<:@0-9.@:>@*\) .*$/\1/'`
  elif test  "x$TOOLCHAIN_TYPE" = xclang; then
    # clang --version output typically looks like
    #    Apple LLVM version 5.0 (clang-500.2.79) (based on LLVM 3.3svn)
    #    clang version 3.3 (tags/RELEASE_33/final)
    # or
    #    Debian clang version 3.2-7ubuntu1 (tags/RELEASE_32/final) (based on LLVM 3.2)
    #    Target: x86_64-pc-linux-gnu
    #    Thread model: posix
    COMPILER_VERSION_OUTPUT=`$COMPILER --version 2>&1`
    # Check that this is likely to be clang
    $ECHO "$COMPILER_VERSION_OUTPUT" | $GREP "clang" > /dev/null
    if test $? -ne 0; then
      AC_MSG_NOTICE([The $COMPILER_NAME compiler (located as $COMPILER) does not seem to be the required $TOOLCHAIN_TYPE compiler.])
      AC_MSG_NOTICE([The result from running with --version was: "$COMPILER_VERSION_OUTPUT"])
      AC_MSG_ERROR([A $TOOLCHAIN_TYPE compiler is required. Try setting --with-tools-dir.])
    fi
    # Collapse compiler output into a single line
    COMPILER_VERSION_STRING=`$ECHO $COMPILER_VERSION_OUTPUT`
    COMPILER_VERSION_NUMBER=`$ECHO $COMPILER_VERSION_OUTPUT | \
        $SED -e 's/^.*clang version \(@<:@1-9@:>@@<:@0-9.@:>@*\).*$/\1/'`

  else
      AC_MSG_ERROR([Unknown toolchain type $TOOLCHAIN_TYPE.])
  fi
  # This sets CC_VERSION_NUMBER or CXX_VERSION_NUMBER. (This comment is a grep marker)
  $1_VERSION_NUMBER="$COMPILER_VERSION_NUMBER"
  # This sets CC_VERSION_STRING or CXX_VERSION_STRING. (This comment is a grep marker)
  $1_VERSION_STRING="$COMPILER_VERSION_STRING"

  AC_MSG_NOTICE([Using $TOOLCHAIN_TYPE $COMPILER_NAME compiler version $COMPILER_VERSION_NUMBER @<:@$COMPILER_VERSION_STRING@:>@])
])


# Try to locate the given C or C++ compiler in the path, or otherwise.
#
# $1 = compiler to test (CC or CXX)
# $2 = human readable name of compiler (C or C++)
# $3 = list of compiler names to search for
AC_DEFUN([TOOLCHAIN_FIND_COMPILER],
[
  COMPILER_NAME=$2
  SEARCH_LIST="$3"

  if test "x[$]$1" != x; then
    # User has supplied compiler name already, always let that override.
    AC_MSG_NOTICE([Will use user supplied compiler $1=[$]$1])
    if test "x`basename [$]$1`" = "x[$]$1"; then
      # A command without a complete path is provided, search $PATH.
      
      AC_PATH_PROGS(POTENTIAL_$1, [$]$1)
      if test "x$POTENTIAL_$1" != x; then
        $1=$POTENTIAL_$1
      else
        AC_MSG_ERROR([User supplied compiler $1=[$]$1 could not be found])
      fi
    else
      # Otherwise it might already be a complete path
      if test ! -x "[$]$1"; then
        AC_MSG_ERROR([User supplied compiler $1=[$]$1 does not exist])
      fi
    fi
  else
    # No user supplied value. Locate compiler ourselves.

    # If we are cross compiling, assume cross compilation tools follows the
    # cross compilation standard where they are prefixed with the autoconf
    # standard name for the target. For example the binary
    # i686-sun-solaris2.10-gcc will cross compile for i686-sun-solaris2.10.
    # If we are not cross compiling, then the default compiler name will be
    # used.

    $1=
    # If TOOLCHAIN_PATH is set, check for all compiler names in there first
    # before checking the rest of the PATH.
    # FIXME: Now that we prefix the TOOLS_DIR to the PATH in the PRE_DETECTION
    # step, this should not be necessary.
    if test -n "$TOOLCHAIN_PATH"; then
      PATH_save="$PATH"
      PATH="$TOOLCHAIN_PATH"
      AC_PATH_PROGS(TOOLCHAIN_PATH_$1, $SEARCH_LIST)
      $1=$TOOLCHAIN_PATH_$1
      PATH="$PATH_save"
    fi

    # AC_PATH_PROGS can't be run multiple times with the same variable,
    # so create a new name for this run.
    if test "x[$]$1" = x; then
      AC_PATH_PROGS(POTENTIAL_$1, $3)
      $1=$POTENTIAL_$1
    fi

    if test "x[$]$1" = x; then
      HELP_MSG_MISSING_DEPENDENCY([devkit])
      AC_MSG_ERROR([Could not find a $COMPILER_NAME compiler. $HELP_MSG])
    fi
  fi

  # Now we have a compiler binary in $1. Make sure it's okay.
  BASIC_FIXUP_EXECUTABLE($1)
  TEST_COMPILER="[$]$1"

  AC_MSG_CHECKING([resolved symbolic links for $1])
  SYMLINK_ORIGINAL="$TEST_COMPILER"
  BASIC_REMOVE_SYMBOLIC_LINKS(SYMLINK_ORIGINAL)
  if test "x$TEST_COMPILER" = "x$SYMLINK_ORIGINAL"; then
    AC_MSG_RESULT([no symlink])
  else
    AC_MSG_RESULT([$SYMLINK_ORIGINAL])
    AC_MSG_CHECKING([if $1 is disguised ccache])
    COMPILER_BASENAME=`$BASENAME "$SYMLINK_ORIGINAL"`
    if test "x$COMPILER_BASENAME" = "xccache"; then
      AC_MSG_RESULT([yes, trying to find proper $COMPILER_NAME compiler])
      # We /usr/lib/ccache in the path, so cc is a symlink to /usr/bin/ccache.
      # We want to control ccache invocation ourselves, so ignore this cc and try
      # searching again.

      # Remove the path to the fake ccache cc from the PATH
      RETRY_COMPILER_SAVED_PATH="$PATH"
      COMPILER_DIRNAME=`$DIRNAME [$]$1`
      PATH="`$ECHO $PATH | $SED -e "s,$COMPILER_DIRNAME,,g" -e "s,::,:,g" -e "s,^:,,g"`"
      # Try again looking for our compiler
      AC_CHECK_TOOLS(PROPER_COMPILER_$1, $3)
      BASIC_FIXUP_EXECUTABLE(PROPER_COMPILER_$1)
      PATH="$RETRY_COMPILER_SAVED_PATH"

      AC_MSG_CHECKING([for resolved symbolic links for $1])
      BASIC_REMOVE_SYMBOLIC_LINKS(PROPER_COMPILER_$1)
      AC_MSG_RESULT([$PROPER_COMPILER_$1])
      $1="$PROPER_COMPILER_$1"
    else
      AC_MSG_RESULT([no, keeping $1])
    fi
  fi

  TOOLCHAIN_EXTRACT_COMPILER_VERSION([$1], [$COMPILER_NAME])
])

# Detect the core components of the toolchain, i.e. the compilers (CC and CXX),
# preprocessor (CPP and CXXCPP), the linker (LD), the assembler (AS) and the
# archiver (AR). Verify that the compilers are correct according to the
# toolchain type.
AC_DEFUN_ONCE([TOOLCHAIN_DETECT_TOOLCHAIN_CORE],
[
  #
  # Setup the compilers (CC and CXX)
  #
  TOOLCHAIN_FIND_COMPILER([CC], [C], $TOOLCHAIN_CC_BINARY)
  # Now that we have resolved CC ourself, let autoconf have its go at it
  AC_PROG_CC([$CC])

  TOOLCHAIN_FIND_COMPILER([CXX], [C++], $TOOLCHAIN_CXX_BINARY)
  # Now that we have resolved CXX ourself, let autoconf have its go at it
  AC_PROG_CXX([$CXX])

  # This is the compiler version number on the form X.Y[.Z]
  AC_SUBST(CC_VERSION_NUMBER)
  AC_SUBST(CXX_VERSION_NUMBER)

  TOOLCHAIN_PREPARE_FOR_VERSION_COMPARISONS

  #
  # Setup the preprocessor (CPP and CXXCPP)
  #
  AC_PROG_CPP
  BASIC_FIXUP_EXECUTABLE(CPP)
  AC_PROG_CXXCPP
  BASIC_FIXUP_EXECUTABLE(CXXCPP)

  #
  # Setup the linker (LD)
  #
  if test "x$TOOLCHAIN_TYPE" = xmicrosoft; then
    # In the Microsoft toolchain we have a separate LD command "link".
    # Make sure we reject /usr/bin/link (as determined in CYGWIN_LINK), which is
    # a cygwin program for something completely different.
    AC_CHECK_PROG([LD], [link],[link],,, [$CYGWIN_LINK])
    BASIC_FIXUP_EXECUTABLE(LD)
    # Verify that we indeed succeeded with this trick.
    AC_MSG_CHECKING([if the found link.exe is actually the Visual Studio linker])
    "$LD" --version > /dev/null
    if test $? -eq 0 ; then
      AC_MSG_RESULT([no])
      AC_MSG_ERROR([This is the Cygwin link tool. Please check your PATH and rerun configure.])
    else
      AC_MSG_RESULT([yes])
    fi
    LDCXX="$LD"
  else
    # All other toolchains use the compiler to link.
    LD="$CC"
    LDCXX="$CXX"
  fi
  AC_SUBST(LD)
  # FIXME: it should be CXXLD, according to standard (cf CXXCPP)
  AC_SUBST(LDCXX)

  #
  # Setup the assembler (AS)
  #
  if test "x$OPENJDK_TARGET_OS" = xsolaris; then
    # FIXME: should this really be solaris, or solstudio?
    BASIC_PATH_PROGS(AS, as)
    BASIC_FIXUP_EXECUTABLE(AS)
  else
    # FIXME: is this correct for microsoft?
    AS="$CC -c"
  fi
  AC_SUBST(AS)

  #
  # Setup the archiver (AR)
  #
  if test "x$TOOLCHAIN_TYPE" = xmicrosoft; then
    # The corresponding ar tool is lib.exe (used to create static libraries)
    AC_CHECK_PROG([AR], [lib],[lib],,,)
  else
    BASIC_CHECK_TOOLS(AR, ar)
  fi
  BASIC_FIXUP_EXECUTABLE(AR)
])

# Setup additional tools that is considered a part of the toolchain, but not the
# core part. Many of these are highly platform-specific and do not exist,
# and/or are not needed on all platforms.
AC_DEFUN_ONCE([TOOLCHAIN_DETECT_TOOLCHAIN_EXTRA],
[
  if test "x$OPENJDK_TARGET_OS" = "xmacosx"; then
    AC_PROG_OBJC
    BASIC_FIXUP_EXECUTABLE(OBJC)
    BASIC_PATH_PROGS(LIPO, lipo)
    BASIC_FIXUP_EXECUTABLE(LIPO)
  else
    OBJC=
  fi

  if test "x$TOOLCHAIN_TYPE" = xmicrosoft; then
    AC_CHECK_PROG([MT], [mt], [mt],,, [/usr/bin/mt])
    BASIC_FIXUP_EXECUTABLE(MT)
    # Setup the resource compiler (RC)
    AC_CHECK_PROG([RC], [rc], [rc],,, [/usr/bin/rc])
    BASIC_FIXUP_EXECUTABLE(RC)
    AC_CHECK_PROG([DUMPBIN], [dumpbin], [dumpbin],,,)
    BASIC_FIXUP_EXECUTABLE(DUMPBIN)
    # We need to check for 'msbuild.exe' because at the place where we expect to
    # find 'msbuild.exe' there's also a directory called 'msbuild' and configure
    # won't find the 'msbuild.exe' executable in that case (and the
    # 'ac_executable_extensions' is unusable due to performance reasons).
    # Notice that we intentionally don't fix up the path to MSBUILD because we
    # will call it in a DOS shell during freetype detection on Windows (see
    # 'LIB_SETUP_FREETYPE' in "libraries.m4"
    AC_CHECK_PROG([MSBUILD], [msbuild.exe], [msbuild.exe],,,)
  fi

  if test "x$OPENJDK_TARGET_OS" = xsolaris; then
    BASIC_PATH_PROGS(STRIP, strip)
    BASIC_FIXUP_EXECUTABLE(STRIP)
    BASIC_PATH_PROGS(NM, nm)
    BASIC_FIXUP_EXECUTABLE(NM)
    BASIC_PATH_PROGS(GNM, gnm)
    BASIC_FIXUP_EXECUTABLE(GNM)

    BASIC_PATH_PROGS(MCS, mcs)
    BASIC_FIXUP_EXECUTABLE(MCS)
  elif test "x$OPENJDK_TARGET_OS" != xwindows; then
    # FIXME: we should unify this with the solaris case above.
    BASIC_CHECK_TOOLS(STRIP, strip)
    BASIC_FIXUP_EXECUTABLE(STRIP)
    AC_PATH_PROG(OTOOL, otool)
    if test "x$OTOOL" = "x"; then
      OTOOL="true"
    fi
    BASIC_CHECK_TOOLS(NM, nm)
    BASIC_FIXUP_EXECUTABLE(NM)
    GNM="$NM"
    AC_SUBST(GNM)
  fi

  # objcopy is used for moving debug symbols to separate files when
  # full debug symbols are enabled.
  if test "x$OPENJDK_TARGET_OS" = xsolaris || test "x$OPENJDK_TARGET_OS" = xlinux; then
    BASIC_CHECK_TOOLS(OBJCOPY, [gobjcopy objcopy])
    # Only call fixup if objcopy was found.
    if test -n "$OBJCOPY"; then
      BASIC_FIXUP_EXECUTABLE(OBJCOPY)
    fi
  fi

  BASIC_CHECK_TOOLS(OBJDUMP, [gobjdump objdump])
  if test "x$OBJDUMP" != x; then
    # Only used for compare.sh; we can live without it. BASIC_FIXUP_EXECUTABLE
    # bails if argument is missing.
    BASIC_FIXUP_EXECUTABLE(OBJDUMP)
  fi
])

# Setup the build tools (i.e, the compiler and linker used to build programs
# that should be run on the build platform, not the target platform, as a build
# helper). Since the non-cross-compile case uses the normal, target compilers
# for this, we can only do this after these have been setup.
AC_DEFUN_ONCE([TOOLCHAIN_SETUP_BUILD_COMPILERS],
[
  if test "x$COMPILE_TYPE" = "xcross"; then
    # Now we need to find a C/C++ compiler that can build executables for the
    # build platform. We can't use the AC_PROG_CC macro, since it can only be
    # used once. Also, we need to do this without adding a tools dir to the
    # path, otherwise we might pick up cross-compilers which don't use standard
    # naming.

    # FIXME: we should list the discovered compilers as an exclude pattern!
    # If we do that, we can do this detection before POST_DETECTION, and still
    # find the build compilers in the tools dir, if needed.
    if test "x$OPENJDK_BUILD_OS" = xmacosx; then
      BASIC_PATH_PROGS(BUILD_CC, [clang cl cc gcc])
      BASIC_PATH_PROGS(BUILD_CXX, [clang++ cl CC g++])
    else
      BASIC_REQUIRE_PROGS(BUILD_CC, [cl cc gcc])
      BASIC_REQUIRE_PROGS(BUILD_CXX, [cl CC g++])
    fi
    BASIC_FIXUP_EXECUTABLE(BUILD_CC)
    BASIC_FIXUP_EXECUTABLE(BUILD_CXX)
    BASIC_PATH_PROGS(BUILD_LD, ld)
    BASIC_FIXUP_EXECUTABLE(BUILD_LD)
  else
    # If we are not cross compiling, use the normal target compilers for
    # building the build platform executables.
    BUILD_CC="$CC"
    BUILD_CXX="$CXX"
    BUILD_LD="$LD"
  fi
])

# Setup legacy variables that are still needed as alternative ways to refer to
# parts of the toolchain.
AC_DEFUN_ONCE([TOOLCHAIN_SETUP_LEGACY],
[
  if test "x$TOOLCHAIN_TYPE" = xmicrosoft; then
    # For hotspot, we need these in Windows mixed path,
    # so rewrite them all. Need added .exe suffix.
    HOTSPOT_CXX="$CXX.exe"
    HOTSPOT_LD="$LD.exe"
    HOTSPOT_MT="$MT.exe"
    HOTSPOT_RC="$RC.exe"
    BASIC_WINDOWS_REWRITE_AS_WINDOWS_MIXED_PATH(HOTSPOT_CXX)
    BASIC_WINDOWS_REWRITE_AS_WINDOWS_MIXED_PATH(HOTSPOT_LD)
    BASIC_WINDOWS_REWRITE_AS_WINDOWS_MIXED_PATH(HOTSPOT_MT)
    BASIC_WINDOWS_REWRITE_AS_WINDOWS_MIXED_PATH(HOTSPOT_RC)
    AC_SUBST(HOTSPOT_MT)
    AC_SUBST(HOTSPOT_RC)
  else
    HOTSPOT_CXX="$CXX"
    HOTSPOT_LD="$LD"
  fi
  AC_SUBST(HOTSPOT_CXX)
  AC_SUBST(HOTSPOT_LD)

  if test  "x$TOOLCHAIN_TYPE" = xclang; then
    USE_CLANG=true
  fi
  AC_SUBST(USE_CLANG)

  # LDEXE is the linker to use, when creating executables. Not really used.
  # FIXME: These should just be removed!
  LDEXE="$LD"
  LDEXECXX="$LDCXX"
  AC_SUBST(LDEXE)
  AC_SUBST(LDEXECXX)
])

# Do some additional checks on the detected tools.
AC_DEFUN_ONCE([TOOLCHAIN_MISC_CHECKS],
 [
  # The package path is used only on macosx?
  # FIXME: clean this up, and/or move it elsewhere.
  PACKAGE_PATH=/opt/local
  AC_SUBST(PACKAGE_PATH)

  # Check for extra potential brokenness.
  if test  "x$TOOLCHAIN_TYPE" = xmicrosoft; then
    # On Windows, double-check that we got the right compiler.
    CC_VERSION_OUTPUT=`$CC 2>&1 | $HEAD -n 1 | $TR -d '\r'`
    COMPILER_CPU_TEST=`$ECHO $CC_VERSION_OUTPUT | $SED -n "s/^.* \(.*\)$/\1/p"`
    if test "x$OPENJDK_TARGET_CPU" = "xx86"; then
      if test "x$COMPILER_CPU_TEST" != "x80x86" -a "x$COMPILER_CPU_TEST" != "xx86"; then
        AC_MSG_ERROR([Target CPU mismatch. We are building for $OPENJDK_TARGET_CPU but CL is for "$COMPILER_CPU_TEST"; expected "80x86" or "x86".])
      fi
    elif test "x$OPENJDK_TARGET_CPU" = "xx86_64"; then
      if test "x$COMPILER_CPU_TEST" != "xx64"; then
        AC_MSG_ERROR([Target CPU mismatch. We are building for $OPENJDK_TARGET_CPU but CL is for "$COMPILER_CPU_TEST"; expected "x64".])
      fi
    fi
  fi

  if test "x$TOOLCHAIN_TYPE" = xgcc; then
    # If this is a --hash-style=gnu system, use --hash-style=both, why?
    HAS_GNU_HASH=`$CC -dumpspecs 2>/dev/null | $GREP 'hash-style=gnu'`
    # This is later checked when setting flags.
  fi

  # Check for broken SuSE 'ld' for which 'Only anonymous version tag is allowed
  # in executable.'
  USING_BROKEN_SUSE_LD=no
  if test "x$OPENJDK_TARGET_OS" = xlinux && test "x$TOOLCHAIN_TYPE" = xgcc; then
    AC_MSG_CHECKING([for broken SuSE 'ld' which only understands anonymous version tags in executables])
    echo "SUNWprivate_1.1 { local: *; };" > version-script.map
    echo "int main() { }" > main.c
    if $CXX -Xlinker -version-script=version-script.map main.c 2>&AS_MESSAGE_LOG_FD >&AS_MESSAGE_LOG_FD; then
      AC_MSG_RESULT(no)
      USING_BROKEN_SUSE_LD=no
    else
      AC_MSG_RESULT(yes)
      USING_BROKEN_SUSE_LD=yes
    fi
    rm -rf version-script.map main.c
  fi
  AC_SUBST(USING_BROKEN_SUSE_LD)
])

# Setup the JTReg Regression Test Harness.
AC_DEFUN_ONCE([TOOLCHAIN_SETUP_JTREG],
[
  AC_ARG_WITH(jtreg, [AS_HELP_STRING([--with-jtreg],
      [Regression Test Harness @<:@probed@:>@])],
      [],
      [with_jtreg=no])

  if test "x$with_jtreg" = xno; then
    # jtreg disabled
    AC_MSG_CHECKING([for jtreg])
    AC_MSG_RESULT(no)
  else
    if test "x$with_jtreg" != xyes; then
      # with path specified.
      JT_HOME="$with_jtreg"
    fi

    if test "x$JT_HOME" != x; then
      AC_MSG_CHECKING([for jtreg])

      # use JT_HOME enviroment var.
      BASIC_FIXUP_PATH([JT_HOME])

      # jtreg win32 script works for everybody
      JTREGEXE="$JT_HOME/bin/jtreg"

      if test ! -f "$JTREGEXE"; then
        AC_MSG_ERROR([JTReg executable does not exist: $JTREGEXE])
      fi

      AC_MSG_RESULT($JTREGEXE)
    else
      # try to find jtreg on path
      BASIC_REQUIRE_PROGS(JTREGEXE, jtreg)
      JT_HOME="`$DIRNAME $JTREGEXE`"
    fi
  fi

  AC_SUBST(JT_HOME)
  AC_SUBST(JTREGEXE)
])


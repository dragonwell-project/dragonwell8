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

AC_DEFUN_ONCE([LIB_SETUP_INIT],
[

  ###############################################################################
  #
  # OS specific settings that we never will need to probe.
  #
  if test "x$OPENJDK_TARGET_OS" = xlinux; then
    AC_MSG_CHECKING([what is not needed on Linux?])
    PULSE_NOT_NEEDED=yes
    AC_MSG_RESULT([pulse])
  fi

  if test "x$OPENJDK_TARGET_OS" = xsolaris; then
    AC_MSG_CHECKING([what is not needed on Solaris?])
    ALSA_NOT_NEEDED=yes
    PULSE_NOT_NEEDED=yes
    AC_MSG_RESULT([alsa pulse])
  fi

  if test "x$OPENJDK_TARGET_OS" = xaix; then
    AC_MSG_CHECKING([what is not needed on AIX?])
    ALSA_NOT_NEEDED=yes
    PULSE_NOT_NEEDED=yes
    AC_MSG_RESULT([alsa pulse])
  fi


  if test "x$OPENJDK_TARGET_OS" = xwindows; then
    AC_MSG_CHECKING([what is not needed on Windows?])
    CUPS_NOT_NEEDED=yes
    ALSA_NOT_NEEDED=yes
    PULSE_NOT_NEEDED=yes
    X11_NOT_NEEDED=yes
    FONTCONFIG_NOT_NEEDED=yes
    AC_MSG_RESULT([alsa cups pulse x11])
  fi

  if test "x$OPENJDK_TARGET_OS" = xmacosx; then
    AC_MSG_CHECKING([what is not needed on MacOSX?])
    ALSA_NOT_NEEDED=yes
    PULSE_NOT_NEEDED=yes
    X11_NOT_NEEDED=yes
    FONTCONFIG_NOT_NEEDED=yes
    AC_MSG_RESULT([alsa pulse x11])
  fi

  if test "x$OPENJDK_TARGET_OS" = xbsd; then
    AC_MSG_CHECKING([what is not needed on bsd?])
    ALSA_NOT_NEEDED=yes
    AC_MSG_RESULT([alsa])
  fi

  if test "x$OPENJDK" = "xfalse"; then
    FREETYPE_NOT_NEEDED=yes
  fi

  if test "x$SUPPORT_HEADFUL" = xno; then
    X11_NOT_NEEDED=yes
  fi

  # Deprecated and now ignored
  BASIC_DEPRECATED_ARG_ENABLE(macosx-runtime-support, macosx_runtime_support)
])

AC_DEFUN_ONCE([LIB_SETUP_X11],
[

  ###############################################################################
  #
  # Check for X Windows
  #

  # Check if the user has specified sysroot, but not --x-includes or --x-libraries.
  # Make a simple check for the libraries at the sysroot, and setup --x-includes and
  # --x-libraries for the sysroot, if that seems to be correct.
  if test "x$OPENJDK_TARGET_OS" = "xlinux"; then
    if test "x$SYSROOT" != "x"; then
      if test "x$x_includes" = xNONE; then
        if test -f "$SYSROOT/usr/X11R6/include/X11/Xlib.h"; then
          x_includes="$SYSROOT/usr/X11R6/include"
        elif test -f "$SYSROOT/usr/include/X11/Xlib.h"; then
          x_includes="$SYSROOT/usr/include"
        fi
      fi
      if test "x$x_libraries" = xNONE; then
        if test -f "$SYSROOT/usr/X11R6/lib/libX11.so"; then
          x_libraries="$SYSROOT/usr/X11R6/lib"
        elif test "$SYSROOT/usr/lib64/libX11.so" && test "x$OPENJDK_TARGET_CPU_BITS" = x64; then
          x_libraries="$SYSROOT/usr/lib64"
        elif test -f "$SYSROOT/usr/lib/libX11.so"; then
          x_libraries="$SYSROOT/usr/lib"
        fi
      fi
    fi
  fi

  # Now let autoconf do it's magic
  AC_PATH_X
  AC_PATH_XTRA

  # AC_PATH_XTRA creates X_LIBS and sometimes adds -R flags. When cross compiling
  # this doesn't make sense so we remove it.
  if test "x$COMPILE_TYPE" = xcross; then
    X_LIBS=`$ECHO $X_LIBS | $SED 's/-R \{0,1\}[[^ ]]*//g'`
  fi

  if test "x$no_x" = xyes && test "x$X11_NOT_NEEDED" != xyes; then
    HELP_MSG_MISSING_DEPENDENCY([x11])
    AC_MSG_ERROR([Could not find X11 libraries. $HELP_MSG])
  fi


  if test "x$OPENJDK_TARGET_OS" = xsolaris; then
    OPENWIN_HOME="/usr/openwin"
    X_CFLAGS="-I$SYSROOT$OPENWIN_HOME/include -I$SYSROOT$OPENWIN_HOME/include/X11/extensions"
    X_LIBS="-L$SYSROOT$OPENWIN_HOME/sfw/lib$OPENJDK_TARGET_CPU_ISADIR \
        -L$SYSROOT$OPENWIN_HOME/lib$OPENJDK_TARGET_CPU_ISADIR \
        -R$OPENWIN_HOME/sfw/lib$OPENJDK_TARGET_CPU_ISADIR \
        -R$OPENWIN_HOME/lib$OPENJDK_TARGET_CPU_ISADIR"
  fi

  #
  # Weird Sol10 something check...TODO change to try compile
  #
  if test "x${OPENJDK_TARGET_OS}" = xsolaris; then
    if test "`uname -r`" = "5.10"; then
      if test "`${EGREP} -c XLinearGradient ${OPENWIN_HOME}/share/include/X11/extensions/Xrender.h`" = "0"; then
        X_CFLAGS="${X_CFLAGS} -DSOLARIS10_NO_XRENDER_STRUCTS"
      fi
    fi
  fi

  AC_LANG_PUSH(C)
  OLD_CFLAGS="$CFLAGS"
  CFLAGS="$CFLAGS $X_CFLAGS"

  # Need to include Xlib.h and Xutil.h to avoid "present but cannot be compiled" warnings on Solaris 10
  AC_CHECK_HEADERS([X11/extensions/shape.h X11/extensions/Xrender.h X11/extensions/XTest.h X11/Intrinsic.h],
      [X11_A_OK=yes],
      [X11_A_OK=no; break],
      [
        # include <X11/Xlib.h>
        # include <X11/Xutil.h>
      ]
  )

  CFLAGS="$OLD_CFLAGS"
  AC_LANG_POP(C)

  if test "x$X11_A_OK" = xno && test "x$X11_NOT_NEEDED" != xyes; then
    HELP_MSG_MISSING_DEPENDENCY([x11])
    AC_MSG_ERROR([Could not find all X11 headers (shape.h Xrender.h XTest.h Intrinsic.h). $HELP_MSG])
  fi

  AC_SUBST(X_CFLAGS)
  AC_SUBST(X_LIBS)
])

AC_DEFUN_ONCE([LIB_SETUP_CUPS],
[

  ###############################################################################
  #
  # The common unix printing system cups is used to print from java.
  #
  AC_ARG_WITH(cups, [AS_HELP_STRING([--with-cups],
      [specify prefix directory for the cups package
      (expecting the headers under PATH/include)])])
  AC_ARG_WITH(cups-include, [AS_HELP_STRING([--with-cups-include],
      [specify directory for the cups include files])])

  if test "x$CUPS_NOT_NEEDED" = xyes; then
    if test "x${with_cups}" != x || test "x${with_cups_include}" != x; then
      AC_MSG_WARN([cups not used, so --with-cups is ignored])
    fi
    CUPS_CFLAGS=
  else
    CUPS_FOUND=no

    if test "x${with_cups}" = xno || test "x${with_cups_include}" = xno; then
      AC_MSG_ERROR([It is not possible to disable the use of cups. Remove the --without-cups option.])
    fi

    if test "x${with_cups}" != x; then
      CUPS_CFLAGS="-I${with_cups}/include"
      CUPS_FOUND=yes
    fi
    if test "x${with_cups_include}" != x; then
      CUPS_CFLAGS="-I${with_cups_include}"
      CUPS_FOUND=yes
    fi
    if test "x$CUPS_FOUND" = xno; then
      BDEPS_CHECK_MODULE(CUPS, cups, xxx, [CUPS_FOUND=yes])
    fi
    if test "x$CUPS_FOUND" = xno; then
      # Are the cups headers installed in the default /usr/include location?
      AC_CHECK_HEADERS([cups/cups.h cups/ppd.h],
          [
            CUPS_FOUND=yes
            CUPS_CFLAGS=
            DEFAULT_CUPS=yes
          ]
      )
    fi
    if test "x$CUPS_FOUND" = xno; then
      # Getting nervous now? Lets poke around for standard Solaris third-party
      # package installation locations.
      AC_MSG_CHECKING([for cups headers])
      if test -s $SYSROOT/opt/sfw/cups/include/cups/cups.h; then
        # An SFW package seems to be installed!
        CUPS_FOUND=yes
        CUPS_CFLAGS="-I$SYSROOT/opt/sfw/cups/include"
      elif test -s $SYSROOT/opt/csw/include/cups/cups.h; then
        # A CSW package seems to be installed!
        CUPS_FOUND=yes
        CUPS_CFLAGS="-I$SYSROOT/opt/csw/include"
      fi
      AC_MSG_RESULT([$CUPS_FOUND])
    fi
    if test "x$CUPS_FOUND" = xno; then
      HELP_MSG_MISSING_DEPENDENCY([cups])
      AC_MSG_ERROR([Could not find cups! $HELP_MSG ])
    fi
  fi

  AC_SUBST(CUPS_CFLAGS)

])

AC_DEFUN([LIB_CHECK_POTENTIAL_FREETYPE],
[
  POTENTIAL_FREETYPE_INCLUDE_PATH="$1"
  POTENTIAL_FREETYPE_LIB_PATH="$2"
  METHOD="$3"

  # Let's start with an optimistic view of the world :-)
  FOUND_FREETYPE=yes

  # First look for the canonical freetype main include file ft2build.h.
  if ! test -s "$POTENTIAL_FREETYPE_INCLUDE_PATH/ft2build.h"; then
    # Oh no! Let's try in the freetype2 directory.
    POTENTIAL_FREETYPE_INCLUDE_PATH="$POTENTIAL_FREETYPE_INCLUDE_PATH/freetype2"
    if ! test -s "$POTENTIAL_FREETYPE_INCLUDE_PATH/ft2build.h"; then
      # Fail.
      FOUND_FREETYPE=no
    fi
  fi

  if test "x$FOUND_FREETYPE" = "xyes"; then
    # Include file found, let's continue the sanity check.
    AC_MSG_NOTICE([Found freetype include files at $POTENTIAL_FREETYPE_INCLUDE_PATH using $METHOD])
    FOUND_FREETYPE=yes

    FREETYPE_LIB_NAME="${LIBRARY_PREFIX}${FREETYPE_BASE_NAME}${SHARED_LIBRARY_SUFFIX}"
    if ! test -s "$POTENTIAL_FREETYPE_LIB_PATH/$FREETYPE_LIB_NAME"; then
      AC_MSG_NOTICE([Could not find $POTENTIAL_FREETYPE_LIB_PATH/$FREETYPE_LIB_NAME. Ignoring location.])
      FOUND_FREETYPE=no
    fi
  fi

  if test "x$FOUND_FREETYPE" = "xyes"; then
    FREETYPE_INCLUDE_PATH="$POTENTIAL_FREETYPE_INCLUDE_PATH"
    AC_MSG_CHECKING([for freetype includes])
    AC_MSG_RESULT([$FREETYPE_INCLUDE_PATH])
    FREETYPE_LIB_PATH="$POTENTIAL_FREETYPE_LIB_PATH"
    AC_MSG_CHECKING([for freetype libraries])
    AC_MSG_RESULT([$FREETYPE_LIB_PATH])
  fi
])

AC_DEFUN_ONCE([LIB_SETUP_FREETYPE],
[
  AC_ARG_WITH(freetype, [AS_HELP_STRING([--with-freetype],
      [specify whether to use 'system' or 'bundled' freetype.
       The selected option applies to both build time and run time.
       The default behaviour can be platform dependent.
       If using 'system' and either the include files or libraries cannot be
       located automatically, then additionally specify both using
       --with-freetype-include and --with-freetype-lib.])])
  AC_ARG_WITH(freetype-include, [AS_HELP_STRING([--with-freetype-include],
      [specify directory for the freetype include files])])
  AC_ARG_WITH(freetype-lib, [AS_HELP_STRING([--with-freetype-lib],
      [specify directory for the freetype library])])

  # This setup is to verify access to system installed freetype header and
  # libraries. On Windows and MacOS this does not apply and using these options
  # will report an error. On other platforms they will default to using the
  # system libraries. If they are found automatically, nothing need be done.
  # If they are not found, the configure "--with-freetype-*" options may be
  # used to fix that. If the preference is to bundle on these platforms then
  # use --with-freetype=bundled.
  FREETYPE_BASE_NAME=freetype
  FREETYPE_CFLAGS=
  FREETYPE_LIBS=
  if (test "x$with_freetype_include" = "x" && test "x$with_freetype_lib" != "x") || \
     (test "x$with_freetype_include" != "x" && test "x$with_freetype_lib" = "x"); then
    AC_MSG_ERROR([Must specify both or neither of --with-freetype-include and --with-freetype-lib])
  fi
  FREETYPE_TO_USE=bundled
  if test "x$OPENJDK_TARGET_OS" != "xwindows" && \
      test "x$OPENJDK_TARGET_OS" != "xmacosx" && \
      test "x$OPENJDK_TARGET_OS" != "xaix"; then
    FREETYPE_TO_USE=system
  fi
  if test "x$with_freetype" != "x" ; then
    if test "x$with_freetype" = "xsystem" ; then
      FREETYPE_TO_USE=system
    elif test "x$with_freetype" = "xbundled" ; then
      FREETYPE_TO_USE=bundled
      if test "x$with_freetype_include" != "x" || \
          test "x$with_freetype_lib" != "x" ; then
        AC_MSG_ERROR(['bundled' cannot be specified with --with-freetype-include and --with-freetype-lib])
      fi
    else
      AC_MSG_ERROR([Valid values for --with-freetype are 'system' and 'bundled'])
    fi
  fi

  if test "x$with_freetype_include" != "x" && \
      test "x$with_freetype_lib" != "x" ; then
    FREETYPE_TO_USE=system
  fi

  if test "x$FREETYPE_TO_USE" = "xsystem" && \
     (test "x$OPENJDK_TARGET_OS" = "xwindows" || \
     test "x$OPENJDK_TARGET_OS" = "xmacosx"); then
    AC_MSG_ERROR([Only bundled freetype can be specified on Mac and Windows])
  fi

  if test "x$with_freetype_include" != "x" ; then
    POTENTIAL_FREETYPE_INCLUDE_PATH="$with_freetype_include"
  fi
  if test "x$with_freetype_lib" != "x" ; then
    POTENTIAL_FREETYPE_LIB_PATH="$with_freetype_lib"
  fi

  if test "x$FREETYPE_TO_USE" = "xsystem" ; then
    if test "x$POTENTIAL_FREETYPE_INCLUDE_PATH" != "x" && \
        test "x$POTENTIAL_FREETYPE_LIB_PATH" != "x" ; then
      # Okay, we got it. Check that it works.
      LIB_CHECK_POTENTIAL_FREETYPE($POTENTIAL_FREETYPE_INCLUDE_PATH,
          $POTENTIAL_FREETYPE_LIB_PATH, [--with-freetype])
      if test "x$FOUND_FREETYPE" != "xyes" ; then
        AC_MSG_ERROR([Can not find or use freetype at location given by --with-freetype-lib|include])
      fi
    else
      # User did not specify a location, but asked for system freetype.
      # Try to locate it.
      # If we have a sysroot, assume that's where we are supposed to look and
      # skip pkg-config.
      if test "x$SYSROOT" = "x" ; then
        if test "x$FOUND_FREETYPE" != "xyes" ; then
          # Check modules using pkg-config, but only if we have it (ugly output
          # results otherwise)
          if test "x$PKG_CONFIG" != "x" ; then
            PKG_CHECK_MODULES(FREETYPE, freetype2, [FOUND_FREETYPE=yes], [FOUND_FREETYPE=no])
            if test "x$FOUND_FREETYPE" = "xyes" ; then
              AC_MSG_CHECKING([for freetype])
              AC_MSG_RESULT([yes (using pkg-config)])
            fi
          fi
        fi
      fi

      if test "x$FOUND_FREETYPE" != "xyes" ; then
        # Check in well-known locations
        FREETYPE_BASE_DIR="$SYSROOT/usr"
        if test "x$OPENJDK_TARGET_CPU_BITS" = "x64" ; then
          LIB_CHECK_POTENTIAL_FREETYPE([$FREETYPE_BASE_DIR/include],
              [$FREETYPE_BASE_DIR/lib/$OPENJDK_TARGET_CPU-linux-gnu], [well-known location])
          if test "x$FOUND_FREETYPE" != "xyes" ; then
            LIB_CHECK_POTENTIAL_FREETYPE([$FREETYPE_BASE_DIR/include],
                [$FREETYPE_BASE_DIR/lib64], [well-known location])
          fi
        else
          LIB_CHECK_POTENTIAL_FREETYPE([$FREETYPE_BASE_DIR/include],
              [$FREETYPE_BASE_DIR/lib/i386-linux-gnu], [well-known location])
          if test "x$FOUND_FREETYPE" != "xyes" ; then
            LIB_CHECK_POTENTIAL_FREETYPE([$FREETYPE_BASE_DIR/include],
                [$FREETYPE_BASE_DIR/lib32], [well-known location])
          fi
        fi

        if test "x$FOUND_FREETYPE" != "xyes" ; then
          LIB_CHECK_POTENTIAL_FREETYPE([$FREETYPE_BASE_DIR/include],
              [$FREETYPE_BASE_DIR/lib], [well-known location])
        fi

        if test "x$FOUND_FREETYPE" != "xyes" ; then
          LIB_CHECK_POTENTIAL_FREETYPE([$FREETYPE_BASE_DIR/include],
              [$FREETYPE_BASE_DIR/lib/$OPENJDK_TARGET_CPU-$OPENJDK_TARGET_OS-$OPENJDK_TARGET_ABI], [well-known location])
        fi

        if test "x$FOUND_FREETYPE" != "xyes" ; then
          LIB_CHECK_POTENTIAL_FREETYPE([$FREETYPE_BASE_DIR/include],
              [$FREETYPE_BASE_DIR/lib/$OPENJDK_TARGET_CPU_AUTOCONF-$OPENJDK_TARGET_OS-$OPENJDK_TARGET_ABI], [well-known location])
        fi

        if test "x$FOUND_FREETYPE" != "xyes" ; then
          FREETYPE_BASE_DIR="$SYSROOT/usr/X11"
          LIB_CHECK_POTENTIAL_FREETYPE([$FREETYPE_BASE_DIR/include],
              [$FREETYPE_BASE_DIR/lib], [well-known location])
        fi

        if test "x$FOUND_FREETYPE" != "xyes" ; then
          FREETYPE_BASE_DIR="$SYSROOT/usr/local"
          LIB_CHECK_POTENTIAL_FREETYPE([$FREETYPE_BASE_DIR/include],
              [$FREETYPE_BASE_DIR/lib], [well-known location])
        fi
      fi # end check in well-known locations

      if test "x$FOUND_FREETYPE" != "xyes" ; then
        HELP_MSG_MISSING_DEPENDENCY([freetype])
        AC_MSG_ERROR([Could not find freetype! $HELP_MSG ])
      fi
    fi # end user specified settings

    # Set FREETYPE_CFLAGS, _LIBS and _LIB_PATH from include and lib dir.
    if test "x$FREETYPE_CFLAGS" = "x" ; then
      if test -d $FREETYPE_INCLUDE_PATH/freetype2/freetype ; then
        FREETYPE_CFLAGS="-I$FREETYPE_INCLUDE_PATH/freetype2 -I$FREETYPE_INCLUDE_PATH"
      else
        FREETYPE_CFLAGS="-I$FREETYPE_INCLUDE_PATH"
      fi
    fi

    if test "x$FREETYPE_LIBS" = "x" ; then
      FREETYPE_LIBS="-L$FREETYPE_LIB_PATH -l$FREETYPE_BASE_NAME"
    fi
  fi

  AC_MSG_RESULT([Using freetype: $FREETYPE_TO_USE])

  AC_SUBST(FREETYPE_TO_USE)
  AC_SUBST(FREETYPE_CFLAGS)
  AC_SUBST(FREETYPE_LIBS)
])

AC_DEFUN_ONCE([LIB_SETUP_ALSA],
[

  ###############################################################################
  #
  # Check for alsa headers and libraries. Used on Linux/GNU systems.
  #
  AC_ARG_WITH(alsa, [AS_HELP_STRING([--with-alsa],
      [specify prefix directory for the alsa package
      (expecting the libraries under PATH/lib and the headers under PATH/include)])])
  AC_ARG_WITH(alsa-include, [AS_HELP_STRING([--with-alsa-include],
      [specify directory for the alsa include files])])
  AC_ARG_WITH(alsa-lib, [AS_HELP_STRING([--with-alsa-lib],
      [specify directory for the alsa library])])

  if test "x$ALSA_NOT_NEEDED" = xyes; then
    if test "x${with_alsa}" != x || test "x${with_alsa_include}" != x || test "x${with_alsa_lib}" != x; then
      AC_MSG_WARN([alsa not used, so --with-alsa is ignored])
    fi
    ALSA_CFLAGS=
    ALSA_LIBS=
  else
    ALSA_FOUND=no

    if test "x${with_alsa}" = xno || test "x${with_alsa_include}" = xno || test "x${with_alsa_lib}" = xno; then
      AC_MSG_ERROR([It is not possible to disable the use of alsa. Remove the --without-alsa option.])
    fi

    if test "x${with_alsa}" != x; then
      ALSA_LIBS="-L${with_alsa}/lib -lasound"
      ALSA_CFLAGS="-I${with_alsa}/include"
      ALSA_FOUND=yes
    fi
    if test "x${with_alsa_include}" != x; then
      ALSA_CFLAGS="-I${with_alsa_include}"
      ALSA_FOUND=yes
    fi
    if test "x${with_alsa_lib}" != x; then
      ALSA_LIBS="-L${with_alsa_lib} -lasound"
      ALSA_FOUND=yes
    fi
    if test "x$ALSA_FOUND" = xno; then
      BDEPS_CHECK_MODULE(ALSA, alsa, xxx, [ALSA_FOUND=yes], [ALSA_FOUND=no])
    fi
    # Do not try pkg-config if we have a sysroot set.
    if test "x$SYSROOT" = x; then
      if test "x$ALSA_FOUND" = xno; then
        PKG_CHECK_MODULES(ALSA, alsa, [ALSA_FOUND=yes], [ALSA_FOUND=no])
      fi
    fi
    if test "x$ALSA_FOUND" = xno; then
      AC_CHECK_HEADERS([alsa/asoundlib.h],
          [
            ALSA_FOUND=yes
            ALSA_CFLAGS=-Iignoreme
            ALSA_LIBS=-lasound
            DEFAULT_ALSA=yes
          ],
          [ALSA_FOUND=no])
    fi
    if test "x$ALSA_FOUND" = xno; then
      HELP_MSG_MISSING_DEPENDENCY([alsa])
      AC_MSG_ERROR([Could not find alsa! $HELP_MSG ])
    fi
  fi

  AC_SUBST(ALSA_CFLAGS)
  AC_SUBST(ALSA_LIBS)
])

################################################################################
# Setup fontconfig
################################################################################
AC_DEFUN_ONCE([LIB_SETUP_FONTCONFIG],
[
  AC_ARG_WITH(fontconfig, [AS_HELP_STRING([--with-fontconfig],
      [specify prefix directory for the fontconfig package
      (expecting the headers under PATH/include)])])
  AC_ARG_WITH(fontconfig-include, [AS_HELP_STRING([--with-fontconfig-include],
      [specify directory for the fontconfig include files])])

  if test "x$FONTCONFIG_NOT_NEEDED" = xyes; then
    if (test "x${with_fontconfig}" != x && test "x${with_fontconfig}" != xno) || \
        (test "x${with_fontconfig_include}" != x && test "x${with_fontconfig_include}" != xno); then
      AC_MSG_WARN([[fontconfig not used, so --with-fontconfig[-*] is ignored]])
    fi
    FONTCONFIG_CFLAGS=
  else
    FONTCONFIG_FOUND=no

    if test "x${with_fontconfig}" = xno || test "x${with_fontconfig_include}" = xno; then
      AC_MSG_ERROR([It is not possible to disable the use of fontconfig. Remove the --without-fontconfig option.])
    fi

    if test "x${with_fontconfig}" != x; then
      AC_MSG_CHECKING([for fontconfig headers])
      if test -s "${with_fontconfig}/include/fontconfig/fontconfig.h"; then
        FONTCONFIG_CFLAGS="-I${with_fontconfig}/include"
        FONTCONFIG_FOUND=yes
        AC_MSG_RESULT([$FONTCONFIG_FOUND])
      else
        AC_MSG_ERROR([Can't find 'include/fontconfig/fontconfig.h' under ${with_fontconfig} given with the --with-fontconfig option.])
      fi
    fi
    if test "x${with_fontconfig_include}" != x; then
      AC_MSG_CHECKING([for fontconfig headers])
      if test -s "${with_fontconfig_include}/fontconfig/fontconfig.h"; then
        FONTCONFIG_CFLAGS="-I${with_fontconfig_include}"
        FONTCONFIG_FOUND=yes
        AC_MSG_RESULT([$FONTCONFIG_FOUND])
      else
        AC_MSG_ERROR([Can't find 'fontconfig/fontconfig.h' under ${with_fontconfig_include} given with the --with-fontconfig-include option.])
      fi
    fi
    if test "x$FONTCONFIG_FOUND" = xno; then
      # Are the fontconfig headers installed in the default /usr/include location?
      AC_CHECK_HEADERS([fontconfig/fontconfig.h], [
          FONTCONFIG_FOUND=yes
          FONTCONFIG_CFLAGS=
          DEFAULT_FONTCONFIG=yes
      ])
    fi
    if test "x$FONTCONFIG_FOUND" = xno; then
      HELP_MSG_MISSING_DEPENDENCY([fontconfig])
      AC_MSG_ERROR([Could not find fontconfig! $HELP_MSG ])
    fi
  fi

  AC_SUBST(FONTCONFIG_CFLAGS)
])

AC_DEFUN_ONCE([LIB_SETUP_MISC_LIBS],
[

  ###############################################################################
  #
  # Check for the jpeg library
  #

  USE_EXTERNAL_LIBJPEG=true
  AC_CHECK_LIB(jpeg, main, [],
      [ USE_EXTERNAL_LIBJPEG=false
      AC_MSG_NOTICE([Will use jpeg decoder bundled with the OpenJDK source])
  ])
  AC_SUBST(USE_EXTERNAL_LIBJPEG)

  ###############################################################################
  #
  # Check for the gif library
  #

  AC_ARG_WITH(giflib, [AS_HELP_STRING([--with-giflib],
      [use giflib from build system or OpenJDK source (system, bundled) @<:@bundled@:>@])])


  AC_MSG_CHECKING([for which giflib to use])

  # default is bundled
  DEFAULT_GIFLIB=bundled

  #
  # if user didn't specify, use DEFAULT_GIFLIB
  #
  if test "x${with_giflib}" = "x"; then
    with_giflib=${DEFAULT_GIFLIB}
  fi

  AC_MSG_RESULT(${with_giflib})

  if test "x${with_giflib}" = "xbundled"; then
    USE_EXTERNAL_LIBGIF=false
  elif test "x${with_giflib}" = "xsystem"; then
    AC_CHECK_HEADER(gif_lib.h, [],
        [ AC_MSG_ERROR([--with-giflib=system specified, but gif_lib.h not found!])])
    AC_CHECK_LIB(gif, DGifGetCode, [],
        [ AC_MSG_ERROR([--with-giflib=system specified, but no giflib found!])])

    USE_EXTERNAL_LIBGIF=true
  else
    AC_MSG_ERROR([Invalid value of --with-giflib: ${with_giflib}, use 'system' or 'bundled'])
  fi
  AC_SUBST(USE_EXTERNAL_LIBGIF)

  ###############################################################################
  #
  # Check for the zlib library
  #

  AC_ARG_WITH(zlib, [AS_HELP_STRING([--with-zlib],
      [use zlib from build system or OpenJDK source (system, bundled) @<:@bundled@:>@])])

  AC_CHECK_LIB(z, compress,
      [ ZLIB_FOUND=yes ],
      [ ZLIB_FOUND=no ])

  AC_MSG_CHECKING([for which zlib to use])

  DEFAULT_ZLIB=bundled
  if test "x$OPENJDK_TARGET_OS" = xmacosx; then
    #
    # On macosx default is system...on others default is
    #
    DEFAULT_ZLIB=system
  fi

  if test "x${ZLIB_FOUND}" != "xyes"; then
    #
    # If we don't find any system...set default to bundled
    #
    DEFAULT_ZLIB=bundled
  fi

  #
  # If user didn't specify, use DEFAULT_ZLIB
  #
  if test "x${with_zlib}" = "x"; then
    with_zlib=${DEFAULT_ZLIB}
  fi

  if test "x${with_zlib}" = "xbundled"; then
    USE_EXTERNAL_LIBZ=false
    AC_MSG_RESULT([bundled])
  elif test "x${with_zlib}" = "xsystem"; then
    if test "x${ZLIB_FOUND}" = "xyes"; then
      USE_EXTERNAL_LIBZ=true
      AC_MSG_RESULT([system])
    else
      AC_MSG_RESULT([system not found])
      AC_MSG_ERROR([--with-zlib=system specified, but no zlib found!])
    fi
  else
    AC_MSG_ERROR([Invalid value for --with-zlib: ${with_zlib}, use 'system' or 'bundled'])
  fi

  AC_SUBST(USE_EXTERNAL_LIBZ)

  ###############################################################################
  LIBZIP_CAN_USE_MMAP=true

  AC_SUBST(LIBZIP_CAN_USE_MMAP)

  ###############################################################################
  #
  # Check if altzone exists in time.h
  #

  AC_LINK_IFELSE([AC_LANG_PROGRAM([#include <time.h>], [return (int)altzone;])],
      [has_altzone=yes],
      [has_altzone=no])
  if test "x$has_altzone" = xyes; then
    AC_DEFINE([HAVE_ALTZONE], 1, [Define if you have the external 'altzone' variable in time.h])
  fi

  ###############################################################################
  #
  # Check the maths library
  #

  AC_CHECK_LIB(m, cos, [],
      [
        AC_MSG_NOTICE([Maths library was not found])
      ]
  )
  AC_SUBST(LIBM)

  ###############################################################################
  #
  # Check for libdl.so

  save_LIBS="$LIBS"
  LIBS=""
  AC_CHECK_LIB(dl,dlopen)
  LIBDL="$LIBS"
  AC_SUBST(LIBDL)
  LIBS="$save_LIBS"
])

AC_DEFUN_ONCE([LIB_SETUP_STATIC_LINK_LIBSTDCPP],
[
  ###############################################################################
  #
  # statically link libstdc++ before C++ ABI is stablized on Linux unless
  # dynamic build is configured on command line.
  #
  AC_ARG_WITH([stdc++lib], [AS_HELP_STRING([--with-stdc++lib=<static>,<dynamic>,<default>],
      [force linking of the C++ runtime on Linux to either static or dynamic, default is static with dynamic as fallback])],
      [
        if test "x$with_stdc__lib" != xdynamic && test "x$with_stdc__lib" != xstatic \
                && test "x$with_stdc__lib" != xdefault; then
          AC_MSG_ERROR([Bad parameter value --with-stdc++lib=$with_stdc__lib!])
        fi
      ],
      [with_stdc__lib=default]
  )

  if test "x$OPENJDK_TARGET_OS" = xlinux; then
    # Test if -lstdc++ works.
    AC_MSG_CHECKING([if dynamic link of stdc++ is possible])
    AC_LANG_PUSH(C++)
    OLD_CXXFLAGS="$CXXFLAGS"
    CXXFLAGS="$CXXFLAGS -lstdc++"
    AC_LINK_IFELSE([AC_LANG_PROGRAM([], [return 0;])],
        [has_dynamic_libstdcxx=yes],
        [has_dynamic_libstdcxx=no])
    CXXFLAGS="$OLD_CXXFLAGS"
    AC_LANG_POP(C++)
    AC_MSG_RESULT([$has_dynamic_libstdcxx])

    # Test if stdc++ can be linked statically.
    AC_MSG_CHECKING([if static link of stdc++ is possible])
    STATIC_STDCXX_FLAGS="-Wl,-Bstatic -lstdc++ -lgcc -Wl,-Bdynamic"
    AC_LANG_PUSH(C++)
    OLD_LIBS="$LIBS"
    OLD_CXX="$CXX"
    LIBS="$STATIC_STDCXX_FLAGS"
    CXX="$CC"
    AC_LINK_IFELSE([AC_LANG_PROGRAM([], [return 0;])],
        [has_static_libstdcxx=yes],
        [has_static_libstdcxx=no])
    LIBS="$OLD_LIBS"
    CXX="$OLD_CXX"
    AC_LANG_POP(C++)
    AC_MSG_RESULT([$has_static_libstdcxx])

    if test "x$has_static_libstdcxx" = xno && test "x$has_dynamic_libstdcxx" = xno; then
      AC_MSG_ERROR([Cannot link to stdc++, neither dynamically nor statically!])
    fi

    if test "x$with_stdc__lib" = xstatic && test "x$has_static_libstdcxx" = xno; then
      AC_MSG_ERROR([Static linking of libstdc++ was not possible!])
    fi

    if test "x$with_stdc__lib" = xdynamic && test "x$has_dynamic_libstdcxx" = xno; then
      AC_MSG_ERROR([Dynamic linking of libstdc++ was not possible!])
    fi

    AC_MSG_CHECKING([how to link with libstdc++])
    # If dynamic was requested, it's available since it would fail above otherwise.
    # If dynamic wasn't requested, go with static unless it isn't available.
    if test "x$with_stdc__lib" = xdynamic || test "x$has_static_libstdcxx" = xno || test "x$JVM_VARIANT_ZEROSHARK" = xtrue; then
      LIBCXX="$LIBCXX -lstdc++"
      LDCXX="$CXX"
      STATIC_CXX_SETTING="STATIC_CXX=false"
      AC_MSG_RESULT([dynamic])
    else
      LIBCXX="$LIBCXX $STATIC_STDCXX_FLAGS"
      LDCXX="$CC"
      STATIC_CXX_SETTING="STATIC_CXX=true"
      AC_MSG_RESULT([static])
    fi
  fi
  AC_SUBST(STATIC_CXX_SETTING)

  if test "x$JVM_VARIANT_ZERO" = xtrue || test "x$JVM_VARIANT_ZEROSHARK" = xtrue; then
    # Figure out LIBFFI_CFLAGS and LIBFFI_LIBS
    PKG_CHECK_MODULES([LIBFFI], [libffi])

  fi

  if test "x$JVM_VARIANT_ZEROSHARK" = xtrue; then
    AC_CHECK_PROG([LLVM_CONFIG], [llvm-config], [llvm-config])

    if test "x$LLVM_CONFIG" != xllvm-config; then
      AC_MSG_ERROR([llvm-config not found in $PATH.])
    fi

    llvm_components="jit mcjit engine nativecodegen native"
    unset LLVM_CFLAGS
    for flag in $("$LLVM_CONFIG" --cxxflags); do
      if echo "${flag}" | grep -q '^-@<:@ID@:>@'; then
        if test "${flag}" != "-D_DEBUG" ; then
          if test "${LLVM_CFLAGS}" != "" ; then
            LLVM_CFLAGS="${LLVM_CFLAGS} "
          fi
          LLVM_CFLAGS="${LLVM_CFLAGS}${flag}"
        fi
      fi
    done
    llvm_version=$("${LLVM_CONFIG}" --version | sed 's/\.//; s/svn.*//')
    LLVM_CFLAGS="${LLVM_CFLAGS} -DSHARK_LLVM_VERSION=${llvm_version}"

    unset LLVM_LDFLAGS
    for flag in $("${LLVM_CONFIG}" --ldflags); do
      if echo "${flag}" | grep -q '^-L'; then
        if test "${LLVM_LDFLAGS}" != ""; then
          LLVM_LDFLAGS="${LLVM_LDFLAGS} "
        fi
        LLVM_LDFLAGS="${LLVM_LDFLAGS}${flag}"
      fi
    done

    unset LLVM_LIBS
    for flag in $("${LLVM_CONFIG}" --libs ${llvm_components}); do
      if echo "${flag}" | grep -q '^-l'; then
        if test "${LLVM_LIBS}" != ""; then
          LLVM_LIBS="${LLVM_LIBS} "
        fi
        LLVM_LIBS="${LLVM_LIBS}${flag}"
      fi
    done

    AC_SUBST(LLVM_CFLAGS)
    AC_SUBST(LLVM_LDFLAGS)
    AC_SUBST(LLVM_LIBS)
  fi

  # libCrun is the c++ runtime-library with SunStudio (roughly the equivalent of gcc's libstdc++.so)
  if test "x$TOOLCHAIN_TYPE" = xsolstudio && test "x$LIBCXX" = x; then
    LIBCXX="${SYSROOT}/usr/lib${OPENJDK_TARGET_CPU_ISADIR}/libCrun.so.1"
  fi

  # TODO better (platform agnostic) test
  if test "x$OPENJDK_TARGET_OS" = xmacosx && test "x$LIBCXX" = x ; then 
    if test "x$TOOLCHAIN_TYPE" = xgcc; then
      LIBCXX="-lstdc++"
    elif test "x$TOOLCHAIN_TYPE" = xclang; then
      LIBCXX="-stdlib=libc++"
    fi
  fi

  AC_SUBST(LIBCXX)
])

AC_DEFUN_ONCE([LIB_SETUP_ON_WINDOWS],
[
  if test "x$OPENJDK_TARGET_OS" = "xwindows"; then
    TOOLCHAIN_SETUP_VS_RUNTIME_DLLS
    BASIC_DEPRECATED_ARG_WITH([dxsdk])
    BASIC_DEPRECATED_ARG_WITH([dxsdk-lib])
    BASIC_DEPRECATED_ARG_WITH([dxsdk-include])
  fi
])

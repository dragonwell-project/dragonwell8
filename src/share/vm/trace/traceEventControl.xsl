<?xml version="1.0" encoding="utf-8"?>
<!--
 Copyright (c) 2019, Oracle and/or its affiliates. All rights reserved.
 DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.

 This code is free software; you can redistribute it and/or modify it
 under the terms of the GNU General Public License version 2 only, as
 published by the Free Software Foundation.

 This code is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 version 2 for more details (a copy is included in the LICENSE file that
 accompanied this code).

 You should have received a copy of the GNU General Public License version
 2 along with this work; if not, write to the Free Software Foundation,
 Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.

 Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 or visit www.oracle.com if you need additional information or have any
 questions.

-->

<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform" version="1.0">
<xsl:import href="xsl_util.xsl"/>
<xsl:output method="text" indent="no" omit-xml-declaration="yes"/>

<xsl:template match="/">
  <xsl:call-template name="file-header"/>

#ifndef TRACEFILES_JFR_NATIVE_EVENTSETTING_HPP
#define TRACEFILES_JFR_NATIVE_EVENTSETTING_HPP

#include "utilities/macros.hpp"
#if INCLUDE_TRACE
#include "tracefiles/traceEventIds.hpp"

/**
 * Event setting. We add some padding so we can use our
 * event IDs as indexes into this.
 */

struct jfrNativeEventSetting {
  jlong  threshold_ticks;
  jlong  cutoff_ticks;
  u1     stacktrace;
  u1     enabled;
  u1     pad[6]; // Because GCC on linux ia32 at least tries to pack this.
};

union JfrNativeSettings {
  // Array version.
  jfrNativeEventSetting bits[MaxTraceEventId];
  // Then, to make it easy to debug,
  // add named struct members also.
  struct {
    jfrNativeEventSetting pad[NUM_RESERVED_EVENTS];
<xsl:for-each select="trace/events/*">
  <xsl:text>    </xsl:text>
  <xsl:value-of select="concat('jfrNativeEventSetting ', @id, ';')"/>
  <xsl:if test="position() != last()">
    <xsl:text>
</xsl:text>
  </xsl:if>
</xsl:for-each>
  } ev;
};

#endif // INCLUDE_TRACE
#endif // TRACEFILES_JFR_NATIVE_EVENTSETTING_HPP
</xsl:template>

</xsl:stylesheet>

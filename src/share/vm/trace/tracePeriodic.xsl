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

#ifndef JFRFILES_JFRPERIODICEVENTSET_HPP
#define JFRFILES_JFRPERIODICEVENTSET_HPP

#include "utilities/macros.hpp"
#if INCLUDE_TRACE
#include "memory/allocation.hpp"
#include "tracefiles/traceEventIds.hpp"

class JfrPeriodicEventSet : public AllStatic {
 public:
  static void requestEvent(TraceEventId id) {
    switch(id) {
  <xsl:for-each select="trace/events/event[@is_requestable='true']">
      case Trace<xsl:value-of select="@id"/>Event:
        request<xsl:value-of select="@id"/>();
        break;
  </xsl:for-each>
      default:
        break;
      }
    }

 private:
<xsl:for-each select="trace/events/event[@is_requestable='true']">
  static void request<xsl:value-of select="@id"/>(void);
</xsl:for-each>
};

#endif // INCLUDE_TRACE
#endif // JFRFILES_JFRPERIODICEVENTSET_HPP
</xsl:template>
</xsl:stylesheet>

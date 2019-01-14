<?xml version="1.0" encoding="utf-8"?>
<!--
 Copyright (c) 2012, 2019, Oracle and/or its affiliates. All rights reserved.
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

#ifndef TRACEFILES_TRACEEVENTCLASSES_HPP
#define TRACEFILES_TRACEEVENTCLASSES_HPP

// On purpose outside the INCLUDE_TRACE
// Some parts of traceEvent.hpp are used outside of
// INCLUDE_TRACE

#include "trace/traceEvent.hpp"
#include "tracefiles/traceTypes.hpp"
#include "utilities/macros.hpp"
#if INCLUDE_TRACE
#include "jfr/recorder/service/jfrEvent.hpp"
/*
 * Each event class has an assert member function verify() which is invoked
 * just before the engine writes the event and its fields to the data stream.
 * The purpose of verify() is to ensure that all fields in the event are initialized
 * and set before attempting to commit.
 *
 * We enforce this requirement because events are generally stack allocated and therefore
 * *not* initialized to default values. This prevents us from inadvertently committing
 * uninitialized values to the data stream.
 *
 * The assert message contains both the index (zero based) as well as the name of the field.
 */

<xsl:apply-templates select="trace/events/struct" mode="jfr"/>
<xsl:apply-templates select="trace/events/event" mode="jfr"/>

#else // !INCLUDE_TRACE

class TraceEvent {
 public:
  TraceEvent() {}
  void set_starttime(const JfrTraceTime&amp; ignore) const {}
  void set_endtime(const JfrTraceTime&amp; ignore) const {}
  void set_starttime(const Ticks&amp; ignore) const {}
  void set_endtime(const Ticks&amp; ignore) const {}
  bool should_commit() const { return false; }
  static bool is_enabled() { return false; }
  void commit() {}
};

  <xsl:apply-templates select="trace/events/struct" mode="empty"/>
  <xsl:apply-templates select="trace/events/event" mode="empty"/>

#endif // INCLUDE_TRACE
#endif // JFRFILES_JFREVENTCLASSES_HPP
</xsl:template>

<xsl:template match="struct" mode="jfr">
struct TraceStruct<xsl:value-of select="@id"/>
{
 private:
<xsl:apply-templates select="value" mode="write-fields"/>

 public:
<xsl:apply-templates select="value" mode="write-setters"/>

  template &lt;typename Writer&gt;
  void writeData(Writer&amp; w) {
<xsl:apply-templates select="value" mode="write-data"/>
  }
};

</xsl:template>

<xsl:template match="struct" mode="empty">
struct TraceStruct<xsl:value-of select="@id"/>
{
 public:
<xsl:apply-templates select="value" mode="write-empty-setters"/>
};

</xsl:template>

<xsl:template match="event" mode="empty">
  <xsl:value-of select="concat('class Event', @id, ' : public TraceEvent')"/>
{
 public:
<xsl:value-of select="concat('  Event', @id, '(EventStartTime ignore=TIMED) {}')"/>
  <xsl:text>
</xsl:text>

<xsl:apply-templates select="value|structvalue|transition_value|relation" mode="write-empty-setters">
  <xsl:with-param name="cls" select="concat('Event',@id)"/>
</xsl:apply-templates>
};

</xsl:template>

<xsl:template match="event" mode="jfr">
  <xsl:value-of select="concat('class Event', @id, ' : public JfrTraceEvent&lt;Event', @id, '&gt;')"/>
{
 private:
<xsl:apply-templates select="value|structvalue|transition_value|relation" mode="write-fields"/>

 public:
  static const bool hasThread = <xsl:value-of select="@has_thread"/>;
  static const bool hasStackTrace = <xsl:value-of select="@has_stacktrace"/>;
  static const bool isInstant = <xsl:value-of select="@is_instant"/>;
  static const bool hasCutoff = <xsl:value-of select="@cutoff"/>;
  static const bool isRequestable = <xsl:value-of select="@is_requestable"/>;
  static const TraceEventId eventId = <xsl:value-of select="concat('Trace', @id, 'Event')"/>;

<xsl:value-of select="concat('  Event', @id, '(EventStartTime timing=TIMED) : JfrTraceEvent&lt;Event', @id, '&gt;(timing) {}&#10;&#10;')"/>

<xsl:apply-templates select="value|structvalue|transition_value|relation" mode="write-setters-with-verification"/>

  template &lt;typename Writer&gt;
  void writeData(Writer&amp; w) {
<xsl:apply-templates select="value|structvalue|transition_value|relation" mode="write-data"/>
  }

<xsl:value-of select="concat('  using TraceEvent&lt;Event', @id, '&gt;::commit;')"/>

<xsl:variable name="instant" select="@is_instant"/>

<!-- non static method (only for non instant events)-->
<xsl:if test="$instant='false'">
  <xsl:value-of select="concat('  Event', @id)"/>(
    <xsl:for-each select="value|structvalue|transition_value|relation">
    <xsl:apply-templates select="." mode="cpp-type"/><xsl:value-of select="concat(' ', @field)"/>
    <xsl:if test="position() != last()">,
    </xsl:if></xsl:for-each>) : JfrTraceEvent&lt;<xsl:value-of select="concat('Event', @id)"/>&gt;(TIMED) {
    if (should_commit()) {<xsl:for-each select="value|structvalue|transition_value|relation">
      set_<xsl:value-of select="@field"/>(<xsl:value-of select="@field"/>);</xsl:for-each>
    }
  }

  void commit(<xsl:for-each select="value|structvalue|transition_value|relation">
    <xsl:apply-templates select="." mode="cpp-type"/><xsl:value-of select="concat(' ', @field)"/>
    <xsl:if test="position() != last()">,
              </xsl:if></xsl:for-each>) {
    if (should_commit()) {
      <xsl:for-each select="value|structvalue|transition_value|relation">set_<xsl:value-of select="@field"/>(<xsl:value-of select="@field"/>);
      </xsl:for-each>commit();
    }
  }</xsl:if>
<!-- static method (for all events) -->
  static void commit(<xsl:if test="$instant='false'">const Ticks&amp; startTicks,
                     const Ticks&amp; endTicks<xsl:choose><xsl:when test="value|structvalue|transition_value|relation">,
                     </xsl:when></xsl:choose></xsl:if>
                     <xsl:for-each select="value|structvalue|transition_value|relation">
    <xsl:apply-templates select="." mode="cpp-type"/><xsl:value-of select="concat(' ', @field)"/>
    <xsl:if test="position() != last()">,
                     </xsl:if></xsl:for-each>) {
    <xsl:value-of select="concat('Event', @id)"/> me(UNTIMED);

    if (me.should_commit()) {
      <xsl:if test="$instant='false'">me.set_starttime(startTicks);
      me.set_endtime(endTicks);
      </xsl:if>
      <xsl:for-each select="value|structvalue|transition_value|relation">me.set_<xsl:value-of select="@field"/>(<xsl:value-of select="@field"/>);
      </xsl:for-each>me.commit();
    }
  }

#ifdef ASSERT
  void verify() const {
<xsl:apply-templates select="value" mode="write-verify"/>
  }
#endif
};
<xsl:text>
</xsl:text>
</xsl:template>

<xsl:template match="value|transition_value|relation" mode="write-empty-setters">
  <xsl:param name="cls"/>
  <xsl:variable name="type" select="@type"/>
  <xsl:variable name="wt" select="//primary_type[@symbol=$type]/@type"/>
  <xsl:value-of select="concat('  void set_', @field, '(', $wt, ' ignore) { }')"/>
  <xsl:if test="position() != last()">
    <xsl:text>
</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="structvalue" mode="write-empty-setters">
  <xsl:param name="cls"/>
  <xsl:value-of select="concat('  void set_', @field, '(const TraceStruct', @type, '&amp; ignore) { }')"/>
  <xsl:if test="position() != last()">
    <xsl:text>
</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="value[@type='TICKS']" mode="write-setters">
  <xsl:value-of select="concat('  void set_', @field, '(const Ticks&amp; time) {&#10;    this->_', @field, ' = time; }')"/>
  <xsl:text>&#10;</xsl:text>
</xsl:template>

<xsl:template match="value[@type='TICKS']" mode="write-setters-with-verification">
  <xsl:value-of select="concat('  void set_', @field, '(const Ticks&amp; time) {&#10;    this->_', @field, ' = time;&#10;')"/>
  <xsl:value-of select="concat('    DEBUG_ONLY(set_field_bit(', position() - 1, '));&#10;  }')"/>
  <xsl:text>&#10;</xsl:text>
</xsl:template>

<xsl:template match="value[@type='TICKSPAN']" mode="write-setters">
  <xsl:value-of select="concat('  void set_', @field, '(const Tickspan&amp; time) {&#10;    this->_', @field, ' = time; }')"/>
  <xsl:text>&#10;</xsl:text>
</xsl:template>

<xsl:template match="value[@type='TICKSPAN']" mode="write-setters-with-verification">
  <xsl:value-of select="concat('  void set_', @field, '(const Tickspan&amp; time) {&#10;    this->_', @field, ' = time;&#10;')"/>
  <xsl:value-of select="concat('    DEBUG_ONLY(set_field_bit(', position() - 1, '));&#10;  }')"/>
  <xsl:text>&#10;</xsl:text>
</xsl:template>

<xsl:template match="value|transition_value|relation" mode="write-setters">
  <xsl:param name="cls"/>
  <xsl:variable name="type" select="@type"/>
  <xsl:variable name="wt" select="//primary_type[@symbol=$type]/@type"/>
  <xsl:value-of select="concat('  void set_', @field, '(', $wt, ' new_value) { this->_', @field, ' = new_value; }')"/>
  <xsl:if test="position() != last()">
    <xsl:text>
</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="value|transition|relation" mode="write-setters-with-verification">
  <xsl:param name="cls"/>
  <xsl:variable name="type" select="@type"/>
  <xsl:variable name="wt" select="//primary_type[@symbol=$type]/@type"/>
  <xsl:value-of select="concat('  void set_', @field, '(', $wt, ' new_value) {&#10;    this->_', @field, ' = new_value;&#10;')"/>
  <xsl:value-of select="concat('    DEBUG_ONLY(set_field_bit(', position() - 1, '));&#10;  }')"/>
  <xsl:if test="position() != last()">
    <xsl:text>
</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="structvalue" mode="write-setters">
  <xsl:param name="cls"/>
  <xsl:value-of select="concat('  void set_', @field, '(const TraceStruct', @type, '&amp; value) { this->_', @field, ' = value; }')"/>
  <xsl:if test="position() != last()">
    <xsl:text>
</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="structvalue" mode="write-setters-with-verification">
  <xsl:param name="cls"/>
  <xsl:value-of select="concat('  void set_', @field, '(const TraceStruct', @type, '&amp; value) {&#10;    this->_', @field, ' = value;&#10;')"/>
  <xsl:value-of select="concat('    DEBUG_ONLY(set_field_bit(', position() - 1, '));&#10;')"/>
  <xsl:text>  }</xsl:text>
  <xsl:if test="position() != last()">
    <xsl:text>
</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="value|transition_value|relation" mode="write-fields">
  <xsl:variable name="type" select="@type"/>
  <xsl:variable name="wt" select="//primary_type[@symbol=$type]/@type"/>
  <xsl:value-of select="concat('  ', $wt, ' _', @field, ';')"/>
  <xsl:if test="position() != last()">
    <xsl:text>
</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="structvalue" mode="write-fields">
  <xsl:value-of select="concat('  TraceStruct', @type, ' _', @field, ';')"/>
  <xsl:if test="position() != last()">
    <xsl:text>
</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="value" mode="write-verify">
  <xsl:param name="cls"/>
  <xsl:text>    </xsl:text>
  <xsl:value-of select="concat('assert(verify_field_bit(', position() - 1, '), ' , $quote, 'Attempting to write an uninitialized event field: ', '_', @field, $quote, ');')"/>
  <xsl:if test="position() != last()">
    <xsl:text>
</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="value|transition_value|relation" mode="write-data">
  <xsl:variable name="type" select="@type"/>
  <xsl:value-of select="concat('    w.write(_', @field, ');')"/>
  <xsl:if test="position() != last()">
    <xsl:text>
</xsl:text>
  </xsl:if>
</xsl:template>

<xsl:template match="structvalue" mode="write-data">
  <xsl:variable name="structtype" select="@type"/>
  <xsl:variable name="structname" select="@field"/>
  <xsl:value-of select="concat('    _', @field, '.writeData(w);')"/>
    <xsl:if test="position() != last()">
      <xsl:text>
</xsl:text>
    </xsl:if>
</xsl:template>


<xsl:template match="value|transition_value|relation" mode="cpp-type">
  <xsl:variable name="type" select="@type"/>
  <xsl:value-of select="//primary_type[@symbol=$type]/@type"/>
</xsl:template>
<xsl:template match="structvalue" mode="cpp-type">
  <xsl:value-of select="concat('const TraceStruct', @type, '&amp;')"/>
</xsl:template>

</xsl:stylesheet>

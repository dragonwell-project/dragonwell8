#
# Copyright (c) 2003, 2010, Oracle and/or its affiliates. All rights reserved.
# Copyright (c) 2018-2019, Azul Systems, Inc. All rights reserved.
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
#

# This makefile (jfr.make) is included from the jfr.make in the
# build directories.
#
# It knows how to build and run the tools to generate jfr.

!include $(WorkSpace)/make/windows/makefiles/rules.make

# #########################################################################
# Build tools needed for the Jfr source code generation

GENERATED   = ../generated

JFR_TOOLS_SRCDIR = $(WorkSpace)/src/share/vm/jfr
JFR_TOOLS_OUTPUTDIR = $(GENERATED)/tools/jfr

JFR_OUTPUTDIR = $(GENERATED)/jfrfiles
JFR_SRCDIR = $(WorkSpace)/src/share/vm/jfr/metadata

METADATA_XML = $(JFR_SRCDIR)/metadata.xml
METADATA_XSD = $(JFR_SRCDIR)/metadata.xsd

# Changing these will trigger a rebuild of generated jfr files.
JFR_DEPS = $(METADATA_XML) \
	$(METADATA_XSD)

JfrGeneratedFiles = \
	$(JFR_OUTPUTDIR)/jfrEventControl.hpp \
	$(JFR_OUTPUTDIR)/jfrEventIds.hpp \
	$(JFR_OUTPUTDIR)/jfrPeriodic.hpp \
	$(JFR_OUTPUTDIR)/jfrTypes.hpp

JfrGenSource = $(JFR_TOOLS_SRCDIR)/GenerateJfrFiles.java
JfrGenClass = $(JFR_TOOLS_OUTPUTDIR)/build/tools/jfr/GenerateJfrFiles.class

.PHONY: all cleanall

# #########################################################################

all: $(JfrGeneratedFiles)

$(JfrGenClass): $(JfrGenSource)
	mkdir -p $(@D)
	$(COMPILE_JAVAC) -d $(JFR_TOOLS_OUTPUTDIR) $(JfrGenSource)

$(JFR_OUTPUTDIR)/jfrEventClasses.hpp: $(METADATA_XML) $(METADATA_XSD) $(JfrGenClass)
	echo Generating $(@F)
	mkdir -p $(@D)
	$(RUN_JAVA) -cp $(JFR_TOOLS_OUTPUTDIR) build.tools.jfr.GenerateJfrFiles $(METADATA_XML) $(METADATA_XSD) $(JFR_OUTPUTDIR)
	test -f $@

$(JfrGeneratedFiles): $(JFR_OUTPUTDIR)/jfrEventClasses.hpp

# #########################################################################

cleanall :
	rm $(JfrGenClass) $(JfrGeneratedFiles) $(JFR_OUTPUTDIR)/jfrEventClasses.hpp

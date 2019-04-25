#
# Copyright (c) 2010, 2011, Oracle and/or its affiliates. All rights reserved.
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

#
# Makefile to run tests from multiple sibling directories
#

# Root of the forest that was built
TOPDIR=..

# This makefile depends on the availability of sibling directories.
LANGTOOLS_DIR=$(TOPDIR)/langtools
JDK_DIR=$(TOPDIR)/jdk
HOTSPOT_DIR=$(TOPDIR)/hotspot

# Macro to run a test target in a subdir
define SUBDIR_TEST # subdirectory target
if [ -d $1 ] ; then \
  if [ -r $1/test/Makefile ] ; then \
    echo "$(MAKE) -k -C $1/test $2" ; \
    $(MAKE) -k -C $1/test $2 ; \
  else \
    echo "ERROR: File does not exist: $1/test/Makefile"; \
    exit 1; \
  fi; \
else \
  echo "WARNING: No testing done, directory does not exist: $1"; \
fi
endef

# Macro to print a summary for a given test subdirectory
define SUBDIR_SUMMARY # subdirectory to print summary
if [ -d $1 ] ; then \
  if [ -r $1/Stats.txt ] ; then \
    cat $1/Stats.txt; \
    echo ""; \
  else \
    echo "ERROR: File does not exist: $1/Stats.txt"; \
    exit 1; \
  fi; \
else \
  echo "WARNING: Expected directory does not exist: $1"; \
  echo "         Test summary might be incorrect."; \
fi
endef

# Default test target (core)
default: jdk_core langtools_jtreg

# All testing
all: jdk_all langtools_all

# Test targets
langtools_% :
	@$(NO_STOPPING)$(call SUBDIR_TEST, $(LANGTOOLS_DIR), JT_JAVA=$(PRODUCT_HOME) JTREG_HOME=$(JT_HOME) UNIQUE_DIR="$@" TEST="$(subst langtools_,,$@)" $(subst langtools_,,$@))

jdk_% core_%s svc_%:
	@$(NO_STOPPING)$(call SUBDIR_TEST, $(JDK_DIR), TEST="$@" $@)

hotspot_%:
	@$(NO_STOPPING)$(call SUBDIR_TEST, $(HOTSPOT_DIR), TEST="$@" $@)

# Variables for tier1 testing
TIER1_TESTOUTPUT="$(ALT_OUTPUTDIR)/testoutput"
TIER1_STATUS_FILE="$(TIER1_TESTOUTPUT)/tier1_exitcode.txt"

# Note: Test failures are handled via summary_tier1 as the
#       tier1 targets are never aborted even if tests fail.
tier1: prep_tier1 jdk_tier1 langtools_tier1 hotspot_tier1 summary_tier1

prep_tier1:
	@rm -rf $(TIER1_STATUS_FILE)

# This relies on jdk_tier1, langtools_tier1, hotspot_tier1 producing
# Stats.txt (summary) and exitcode.txt files.
summary_tier1:
	@(EXIT_VAL=0; \
	  echo ""; \
	  echo "-------------- Test Summary ------------"; \
	  echo ""; \
	  for test_dir in $$(find "$(ALT_OUTPUTDIR)" -type d -name \*_tier1); do \
	    $(call SUBDIR_SUMMARY, $${test_dir}); \
	    EXIT_VAL=$$(expr $${EXIT_VAL} + $$(cat $${test_dir}/exitcode.txt)); \
	  done; \
	  echo $${EXIT_VAL} > $(TIER1_STATUS_FILE); \
	  echo "For details see:"; \
	  echo $(TIER1_TESTOUTPUT); \
	  echo ""; \
	  echo "-------------- Test Summary ------------"; \
	  echo "")

#
# jtreg_tests
#
# Invocation:
#
# make jtreg_tests TESTDIRS=<test-dirs>
#
# where <test-dirs> is something like '../<component>/test/runtime',
# <component> in turn being one of the top level directories (for
# example 'hotspot').
#
# The below will strip the path prefix and delegate to the
# corresponding ../<component>/test/Makefile.

ifneq ($(TESTDIRS),)
# Extract the component from ../<component>/...
COMPONENT=$(word 2,$(subst /, ,$(TESTDIRS)))

# Strip off the ../<component>/test prefix and pass the rest as TESTDIRS
# to the delegate Makefile
TESTDIRS_TESTS=$(patsubst ../$(COMPONENT)/test/%,%,$(TESTDIRS))
endif

jtreg_tests:
	@$(NO_STOPPING)$(call SUBDIR_TEST, $(TOPDIR)/$(COMPONENT), TESTDIRS=$(TESTDIRS_TESTS) $@)

################################################################

# Phony targets (e.g. these are not filenames)
.PHONY: all clean summary_tier1 prep_tier1

################################################################

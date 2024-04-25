#
# Copyright (c) 2006, 2013, Oracle and/or its affiliates. All rights reserved.
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

!include local.make

all: checkCL checkLink

checkCL:
	@ if "$(MSC_VER)" NEQ "1310" if "$(MSC_VER)" NEQ "1399" if "$(MSC_VER)" NEQ "1400" if "$(MSC_VER)" NEQ "1500" if "$(MSC_VER)" NEQ "1600" if "$(MSC_VER)" NEQ "1700" \
	if "$(MSC_VER)" NEQ "1800" \
	if "$(MSC_VER)" NEQ "1900" \
	if "$(MSC_VER)" NEQ "1912" \
	if "$(MSC_VER)" NEQ "1920" if "$(MSC_VER)" NEQ "1921" if "$(MSC_VER)" NEQ "1922" if "$(MSC_VER)" NEQ "1923" if "$(MSC_VER)" NEQ "1924" \
	if "$(MSC_VER)" NEQ "1925" if "$(MSC_VER)" NEQ "1926" if "$(MSC_VER)" NEQ "1927" if "$(MSC_VER)" NEQ "1928" if "$(MSC_VER)" NEQ "1929" \
	if "$(MSC_VER)" NEQ "1930" if "$(MSC_VER)" NEQ "1931" if "$(MSC_VER)" NEQ "1932" if "$(MSC_VER)" NEQ "1933" if "$(MSC_VER)" NEQ "1934" \
	if "$(MSC_VER)" NEQ "1935" if "$(MSC_VER)" NEQ "1936" if "$(MSC_VER)" NEQ "1937" if "$(MSC_VER)" NEQ "1938" \
	echo *** WARNING *** unrecognized cl.exe version $(MSC_VER) ($(RAW_MSC_VER)).  Use FORCE_MSC_VER to override automatic detection.

checkLink:
	@ if "$(LD_VER)" NEQ "710" if "$(LD_VER)" NEQ "800" if "$(LD_VER)" NEQ "900" if "$(LD_VER)" NEQ "1000" if "$(LD_VER)" NEQ "1100" \
	if "$(LD_VER)" NEQ "1200" \
	if "$(LD_VER)" NEQ "1300" \
	if "$(LD_VER)" NEQ "1400" \
	if "$(LD_VER)" NEQ "1412" \
	if "$(LD_VER)" NEQ "1420" if "$(LD_VER)" NEQ "1421" if "$(LD_VER)" NEQ "1422" if "$(LD_VER)" NEQ "1423" if "$(LD_VER)" NEQ "1424" \
	if "$(LD_VER)" NEQ "1425" if "$(LD_VER)" NEQ "1426" if "$(LD_VER)" NEQ "1427" if "$(LD_VER)" NEQ "1428" if "$(LD_VER)" NEQ "1429" \
	if "$(LD_VER)" NEQ "1430" if "$(LD_VER)" NEQ "1431" if "$(LD_VER)" NEQ "1432" if "$(LD_VER)" NEQ "1433" if "$(LD_VER)" NEQ "1434" \
	if "$(LD_VER)" NEQ "1435" if "$(LD_VER)" NEQ "1436" if "$(LD_VER)" NEQ "1437" if "$(LD_VER)" NEQ "1438" \
	echo *** WARNING *** unrecognized link.exe version $(LD_VER) ($(RAW_LD_VER)).  Use FORCE_LD_VER to override automatic detection.

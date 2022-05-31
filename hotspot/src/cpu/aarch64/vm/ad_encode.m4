dnl Copyright (c) 2014, 2022 Red Hat Inc. All rights reserved.
dnl DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
dnl
dnl This code is free software; you can redistribute it and/or modify it
dnl under the terms of the GNU General Public License version 2 only, as
dnl published by the Free Software Foundation.
dnl
dnl This code is distributed in the hope that it will be useful, but WITHOUT
dnl ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
dnl FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
dnl version 2 for more details (a copy is included in the LICENSE file that
dnl accompanied this code).
dnl
dnl You should have received a copy of the GNU General Public License version
dnl 2 along with this work; if not, write to the Free Software Foundation,
dnl Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
dnl
dnl Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
dnl or visit www.oracle.com if you need additional information or have any
dnl questions.
dnl
dnl 
dnl Process this file with m4 ad_encode.m4 to generate the load/store
dnl patterns used in aarch64.ad.
dnl
dnl
dnl
define(LOAD,`
  // This encoding class is generated automatically from ad_encode.m4.
  // DO NOT EDIT ANYTHING IN THIS SECTION OF THE FILE
  enc_class aarch64_enc_$2($1 dst, memory mem) %{
    $3Register dst_reg = as_$3Register($dst$$reg);
    loadStore(MacroAssembler(&cbuf), &MacroAssembler::$2, dst_reg, $mem->opcode(),
               as_Register($mem$$base), $mem$$index, $mem$$scale, $mem$$disp);
  %}')dnl
dnl
dnl
dnl
define(LOADV,`
  // This encoding class is generated automatically from ad_encode.m4.
  // DO NOT EDIT ANYTHING IN THIS SECTION OF THE FILE
  enc_class aarch64_enc_$2($1 dst, memory mem) %{
    FloatRegister dst_reg = as_FloatRegister($dst$$reg);
    loadStore(MacroAssembler(&cbuf), &MacroAssembler::ldr, dst_reg, MacroAssembler::$3,
       $mem->opcode(), as_Register($mem$$base), $mem$$index, $mem$$scale, $mem$$disp);
  %}')dnl
dnl
dnl
dnl
define(STORE,`
  // This encoding class is generated automatically from ad_encode.m4.
  // DO NOT EDIT ANYTHING IN THIS SECTION OF THE FILE
  enc_class aarch64_enc_$2($1 src, memory mem) %{
    $3Register src_reg = as_$3Register($src$$reg);
    $4loadStore(MacroAssembler(&cbuf), &MacroAssembler::$2, src_reg, $mem->opcode(),
               as_Register($mem$$base), $mem$$index, $mem$$scale, $mem$$disp);
  %}')dnl
dnl
dnl
dnl
define(STORE0,`
  // This encoding class is generated automatically from ad_encode.m4.
  // DO NOT EDIT ANYTHING IN THIS SECTION OF THE FILE
  enc_class aarch64_enc_$2`'0(memory mem) %{
    MacroAssembler _masm(&cbuf);
    $4loadStore(_masm, &MacroAssembler::$2, zr, $mem->opcode(),
               as_Register($mem$$base), $mem$$index, $mem$$scale, $mem$$disp);
  %}')dnl
dnl
dnl
dnl
define(STOREL,`
  // This encoding class is generated automatically from ad_encode.m4.
  // DO NOT EDIT ANYTHING IN THIS SECTION OF THE FILE
  enc_class aarch64_enc_$2($1 src, memory mem) %{
    $3Register src_reg = as_$3Register($src$$reg);
    $4loadStore(MacroAssembler(&cbuf), &MacroAssembler::$2, src_reg, $mem->opcode(),
               as_Register($mem$$base), $mem$$index, $mem$$scale, $mem$$disp, $5);
  %}')dnl
dnl
dnl
dnl
define(STOREL0,`
  // This encoding class is generated automatically from ad_encode.m4.
  // DO NOT EDIT ANYTHING IN THIS SECTION OF THE FILE
  enc_class aarch64_enc_$2`'0(memory mem) %{
    MacroAssembler _masm(&cbuf);
    $4loadStore(_masm, &MacroAssembler::$2, zr, $mem->opcode(),
               as_Register($mem$$base), $mem$$index, $mem$$scale, $mem$$disp, $5);
  %}')dnl
dnl
dnl
dnl
define(STOREV,`
  // This encoding class is generated automatically from ad_encode.m4.
  // DO NOT EDIT ANYTHING IN THIS SECTION OF THE FILE
  enc_class aarch64_enc_$2($1 src, memory mem) %{
    FloatRegister src_reg = as_FloatRegister($src$$reg);
    loadStore(MacroAssembler(&cbuf), &MacroAssembler::str, src_reg, MacroAssembler::$3,
       $mem->opcode(), as_Register($mem$$base), $mem$$index, $mem$$scale, $mem$$disp);
  %}')dnl
dnl
dnl
dnl
LOAD(iRegI,ldrsbw)
LOAD(iRegI,ldrsb)
LOAD(iRegI,ldrb)
LOAD(iRegL,ldrb)
LOAD(iRegI,ldrshw)
LOAD(iRegI,ldrsh)
LOAD(iRegI,ldrh)
LOAD(iRegL,ldrh)
LOAD(iRegI,ldrw)
LOAD(iRegL,ldrw)
LOAD(iRegL,ldrsw)
LOAD(iRegL,ldr)
LOAD(vRegF,ldrs,Float)
LOAD(vRegD,ldrd,Float)
LOADV(vecD,ldrvS,S)
LOADV(vecD,ldrvD,D)
LOADV(vecX,ldrvQ,Q)
STORE(iRegI,strb)
STORE0(iRegI,strb)

  // This encoding class is generated automatically from ad_encode.m4.
  // DO NOT EDIT ANYTHING IN THIS SECTION OF THE FILE
  enc_class aarch64_enc_strb0_ordered(memory mem) %{
    MacroAssembler _masm(&cbuf);
    __ membar(Assembler::StoreStore);
    loadStore(_masm, &MacroAssembler::strb, zr, $mem->opcode(),
               as_Register($mem$$base), $mem$$index, $mem$$scale, $mem$$disp);
  %}dnl

STOREL(iRegI,strh,,,2)
STOREL0(iRegI,strh,,,2)
STOREL(iRegI,strw,,,4)
STOREL0(iRegI,strw,,,4)
STOREL(iRegL,str,,
`// we sometimes get asked to store the stack pointer into the
    // current thread -- we cannot do that directly on AArch64
    if (src_reg == r31_sp) {
      MacroAssembler _masm(&cbuf);
      assert(as_Register($mem$$base) == rthread, "unexpected store for sp");
      __ mov(rscratch2, sp);
      src_reg = rscratch2;
    }
    ',8)
STOREL0(iRegL,str,,,8)
STORE(vRegF,strs,Float)
STORE(vRegD,strd,Float)
STOREV(vecD,strvS,S)
STOREV(vecD,strvD,D)
STOREV(vecX,strvQ,Q)

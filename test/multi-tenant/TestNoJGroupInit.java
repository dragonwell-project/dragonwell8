/*
 * Copyright (c) 2020 Alibaba Group Holding Limited. All Rights Reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation. Alibaba designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 */
import java.lang.reflect.Field;

/* @test
 * @requires os.family == "Linux"
 * @requires os.arch == "amd64"
 * @summary test scenario where JGroup will not be initialized
 * @compile TestNoJGroupInit.java
 * @run main/othervm/timeout=300 -XX:+MultiTenant TestNoJGroupInit
 */

 public class TestNoJGroupInit {
     public static void main(String[] args) throws Exception {
         Class jgroupClazz = Class.forName("com.alibaba.tenant.NativeDispatcher");
         Field f = jgroupClazz.getDeclaredField("CGROUP_INIT_SUCCESS");
         f.setAccessible(true);
         Boolean b = (Boolean)f.get(null);
         if (b) {
             throw new RuntimeException("Should not be initialized!");
         }
     }
 }
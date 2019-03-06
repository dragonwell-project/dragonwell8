/*
 * Copyright (c) 2019 Alibaba Group Holding Limited. All Rights Reserved.
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
 *
 */

import java.util.ArrayList;

public class TestPrintYoungGenHistoAfterParNewGC {
    public static void main(String []args) {
        int count = 0;
        ArrayList list = new ArrayList();
        for (int i = 0; ; i++) {
            byte[] data = new byte[1024];
            list.add(data);
            InnerA aa = new InnerA();
            InnerA bb = new InnerA();
            if (i % 100000 == 0) {
                count++;
                list.clear();
                if (count > 1000000) {
                    break;
                }
            }
        }
    }

    public static class InnerA {
        String name;
        int id;
        String number;
        public InnerA() {
            name = "hello";
            id = 23333;
            number = "010110";
        }
    }
}

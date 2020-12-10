package sun.nio.ch.rdma;/*
 * Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.
 *
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 *
 */

import java.net.ProtocolFamily;
import java.net.SocketOption;
import java.net.StandardSocketOptions;
import java.util.HashMap;
import java.util.Map;

class RdmaSocketOptionRegistry {

    private RdmaSocketOptionRegistry() { }

    private static class RegistryKey {
        private final SocketOption<?> name;
        private final ProtocolFamily family;
        RegistryKey(SocketOption<?> name, ProtocolFamily family) {
            this.name = name;
            this.family = family;
        }
        public int hashCode() {
            return name.hashCode() + family.hashCode();
        }
        public boolean equals(Object ob) {
            if (ob == null) return false;
            if (!(ob instanceof RegistryKey)) return false;
            RegistryKey other = (RegistryKey)ob;
            if (this.name != other.name) return false;
            if (this.family != other.family) return false;
            return true;
        }
    }

    private static class LazyInitialization {

        static final Map<RegistryKey,RdmaOptionKey> options = options();

        private static Map<RegistryKey,RdmaOptionKey> options() {
            Map<RegistryKey,RdmaOptionKey> map =
                    new HashMap<RegistryKey,RdmaOptionKey>();
            map.put(new RegistryKey(StandardSocketOptions.SO_SNDBUF,
                    RdmaNet.UNSPEC), new RdmaOptionKey(1, 7));
            map.put(new RegistryKey(StandardSocketOptions.SO_RCVBUF,
                    RdmaNet.UNSPEC), new RdmaOptionKey(1, 8));
            map.put(new RegistryKey(StandardSocketOptions.SO_REUSEADDR,
                    RdmaNet.UNSPEC), new RdmaOptionKey(1, 2));

            map.put(new RegistryKey(StandardSocketOptions.TCP_NODELAY,
                    RdmaNet.UNSPEC), new RdmaOptionKey(6, 1));
            return map;
        }
    }


    public static RdmaOptionKey findOption(SocketOption<?> name,
                                           ProtocolFamily family) {
        RegistryKey key = new RegistryKey(name, family);
        return LazyInitialization.options.get(key);
    }
}

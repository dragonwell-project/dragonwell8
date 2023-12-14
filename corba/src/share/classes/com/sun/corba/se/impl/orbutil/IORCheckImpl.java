/*
 * Copyright (c) 2023, Azul Systems, Inc. All rights reserved.
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
 */
package com.sun.corba.se.impl.orbutil;

import java.io.InvalidObjectException;
import java.security.AccessController;
import java.util.*;

import sun.security.action.GetPropertyAction;

public final class IORCheckImpl {

    private static final Set<String> stubsToCheck;

    static {
        boolean checkLocalStubs =
            !getBooleanProperty(ORBConstants.DISABLE_IOR_CHECK_FOR_LOCAL_STUBS,
                getBooleanProperty(ORBConstants.ALLOW_DESERIALIZE_OBJECT, false));

        boolean checkRemoteStubs =
            getBooleanProperty(ORBConstants.ENABLE_IOR_CHECK_FOR_REMOTE_STUBS, false);

        stubsToCheck = getStubsToCheck(checkLocalStubs, checkRemoteStubs);
    }

    private static Set<String> getStubsToCheck(boolean checkLocalStubs, boolean checkRemoteStubs) {
        if (!checkLocalStubs && !checkRemoteStubs) {
            return Collections.emptySet();
        }
        List<String> stubs = new ArrayList<>();
        if (checkLocalStubs) {
            stubs.addAll(getLocalStubs());
        }
        if (checkRemoteStubs) {
            stubs.addAll(getRemoteStubs());
        }
        return Collections.unmodifiableSet(new HashSet<>(stubs));
    }

    private static List<String> getLocalStubs() {
        String[] localStubs = {
            "org.omg.DynamicAny._DynAnyFactoryStub",
            "org.omg.DynamicAny._DynAnyStub",
            "org.omg.DynamicAny._DynArrayStub",
            "org.omg.DynamicAny._DynEnumStub",
            "org.omg.DynamicAny._DynFixedStub",
            "org.omg.DynamicAny._DynSequenceStub",
            "org.omg.DynamicAny._DynStructStub",
            "org.omg.DynamicAny._DynUnionStub",
            "org.omg.DynamicAny._DynValueStub"
        };
        return Arrays.asList(localStubs);
    }

    private static List<String> getRemoteStubs() {
        String[] remoteStubs = {
            "com.sun.corba.se.spi.activation._ActivatorStub",
            "com.sun.corba.se.spi.activation._InitialNameServiceStub",
            "com.sun.corba.se.spi.activation._LocatorStub",
            "com.sun.corba.se.spi.activation._RepositoryStub",
            "com.sun.corba.se.spi.activation._ServerManagerStub",
            "com.sun.corba.se.spi.activation._ServerStub",
            "org.omg.CosNaming._BindingIteratorStub",
            "org.omg.CosNaming._NamingContextExtStub",
            "org.omg.CosNaming._NamingContextStub",
            "org.omg.PortableServer._ServantActivatorStub",
            "org.omg.PortableServer._ServantLocatorStub"
        };
        return Arrays.asList(remoteStubs);
    }

    /*
     * The str parameter is expected to start with "IOR:".
     * Otherwise, the method throws the InvalidObjectException exception.
     */
    public static void check(String str, String stubClassName) throws InvalidObjectException {
        if (stubsToCheck.contains(stubClassName) && !str.startsWith(ORBConstants.STRINGIFY_PREFIX)) {
            throw new InvalidObjectException("IOR: expected");
        }
    }

    private static boolean getBooleanProperty(String property, boolean defaultValue) {
        String value = AccessController.doPrivileged(
            new GetPropertyAction(property, String.valueOf(defaultValue)));
        return "true".equalsIgnoreCase(value);
    }
}

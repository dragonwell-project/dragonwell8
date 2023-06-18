/*
 * Copyright (c) 2002, 2020, Oracle and/or its affiliates. All rights reserved.
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

package sun.misc;

import javax.crypto.SealedObject;

import sun.nio.ch.IOEventAccess;
import sun.nio.ch.IOUtil;
import sun.nio.ch.IOUtilAccess;
import sun.nio.ch.NetAccess;
import sun.nio.ch.Net;

import java.util.jar.JarFile;
import java.io.Console;
import java.io.FileDescriptor;
import java.io.ObjectInputStream;
import java.security.ProtectionDomain;
import java.security.Signature;

import java.security.AccessController;

/** A repository of "shared secrets", which are a mechanism for
    calling implementation-private methods in another package without
    using reflection. A package-private class implements a public
    interface and provides the ability to call package-private methods
    within that package; the object implementing that interface is
    provided through a third package to which access is restricted.
    This framework avoids the primary disadvantage of using reflection
    for this purpose, namely the loss of compile-time checking. */

public class SharedSecrets {
    private static final Unsafe unsafe = Unsafe.getUnsafe();
    private static JavaUtilJarAccess javaUtilJarAccess;
    private static JavaLangAccess javaLangAccess;
    private static JavaLangClassLoaderAccess javaLangClassLoaderAccess;
    private static JavaLangRefAccess javaLangRefAccess;
    private static JavaIOAccess javaIOAccess;
    private static JavaNetAccess javaNetAccess;
    private static JavaNetSocketAccess javaNetSocketAccess;
    private static JavaNetHttpCookieAccess javaNetHttpCookieAccess;
    private static JavaNioAccess javaNioAccess;
    private static JavaIOFileDescriptorAccess javaIOFileDescriptorAccess;
    private static JavaSecurityProtectionDomainAccess javaSecurityProtectionDomainAccess;
    private static JavaSecurityAccess javaSecurityAccess;
    private static JavaUtilZipFileAccess javaUtilZipFileAccess;
    private static JavaAWTAccess javaAWTAccess;
    private static JavaOISAccess javaOISAccess;
    private static JavaxCryptoSealedObjectAccess javaxCryptoSealedObjectAccess;
    private static JavaObjectInputStreamReadString javaObjectInputStreamReadString;
    private static JavaObjectInputStreamAccess javaObjectInputStreamAccess;
    private static TenantAccess tenantAccess;
    private static JavaSecuritySignatureAccess javaSecuritySignatureAccess;
    private static WispEngineAccess wispEngineAccess;
    private static IOEventAccess ioEventAccess;
    private static IOUtilAccess ioUtilAccess;
    private static NetAccess netAccess;

    public static JavaUtilJarAccess javaUtilJarAccess() {
        if (javaUtilJarAccess == null) {
            // Ensure JarFile is initialized; we know that that class
            // provides the shared secret
            unsafe.ensureClassInitialized(JarFile.class);
        }
        return javaUtilJarAccess;
    }

    public static void setJavaUtilJarAccess(JavaUtilJarAccess access) {
        javaUtilJarAccess = access;
    }

    public static void setJavaLangAccess(JavaLangAccess jla) {
        javaLangAccess = jla;
    }

    public static JavaLangAccess getJavaLangAccess() {
        return javaLangAccess;
    }

    public static void setJavaLangClassLoaderAccess(JavaLangClassLoaderAccess jlcla) {
        javaLangClassLoaderAccess = jlcla;
    }

    public static JavaLangClassLoaderAccess getJavaLangClassLoaderAccess() {
        return javaLangClassLoaderAccess;
    }

    public static void setJavaLangRefAccess(JavaLangRefAccess jlra) {
        javaLangRefAccess = jlra;
    }

    public static JavaLangRefAccess getJavaLangRefAccess() {
        return javaLangRefAccess;
    }

    public static void setJavaNetAccess(JavaNetAccess jna) {
        javaNetAccess = jna;
    }

    public static JavaNetAccess getJavaNetAccess() {
        return javaNetAccess;
    }

    public static void setJavaNetSocketAccess(JavaNetSocketAccess javaNetSocketAccess) {
        SharedSecrets.javaNetSocketAccess = javaNetSocketAccess;
    }

    public static JavaNetSocketAccess getJavaNetSocketAccess() {
        return javaNetSocketAccess;
    }

    public static void setJavaNetHttpCookieAccess(JavaNetHttpCookieAccess a) {
        javaNetHttpCookieAccess = a;
    }

    public static JavaNetHttpCookieAccess getJavaNetHttpCookieAccess() {
        if (javaNetHttpCookieAccess == null)
            unsafe.ensureClassInitialized(java.net.HttpCookie.class);
        return javaNetHttpCookieAccess;
    }

    public static void setJavaNioAccess(JavaNioAccess jna) {
        javaNioAccess = jna;
    }

    public static JavaNioAccess getJavaNioAccess() {
        if (javaNioAccess == null) {
            // Ensure java.nio.ByteOrder is initialized; we know that
            // this class initializes java.nio.Bits that provides the
            // shared secret.
            unsafe.ensureClassInitialized(java.nio.ByteOrder.class);
        }
        return javaNioAccess;
    }

    public static void setJavaIOAccess(JavaIOAccess jia) {
        javaIOAccess = jia;
    }

    public static JavaIOAccess getJavaIOAccess() {
        if (javaIOAccess == null) {
            unsafe.ensureClassInitialized(Console.class);
        }
        return javaIOAccess;
    }

    public static void setJavaIOFileDescriptorAccess(JavaIOFileDescriptorAccess jiofda) {
        javaIOFileDescriptorAccess = jiofda;
    }

    public static JavaIOFileDescriptorAccess getJavaIOFileDescriptorAccess() {
        if (javaIOFileDescriptorAccess == null)
            unsafe.ensureClassInitialized(FileDescriptor.class);

        return javaIOFileDescriptorAccess;
    }

    public static void setJavaOISAccess(JavaOISAccess access) {
        javaOISAccess = access;
    }

    public static JavaOISAccess getJavaOISAccess() {
        if (javaOISAccess == null)
            unsafe.ensureClassInitialized(ObjectInputStream.class);

        return javaOISAccess;
    }


    public static void setJavaSecurityProtectionDomainAccess
        (JavaSecurityProtectionDomainAccess jspda) {
            javaSecurityProtectionDomainAccess = jspda;
    }

    public static JavaSecurityProtectionDomainAccess
        getJavaSecurityProtectionDomainAccess() {
            if (javaSecurityProtectionDomainAccess == null)
                unsafe.ensureClassInitialized(ProtectionDomain.class);
            return javaSecurityProtectionDomainAccess;
    }

    public static void setJavaSecurityAccess(JavaSecurityAccess jsa) {
        javaSecurityAccess = jsa;
    }

    public static JavaSecurityAccess getJavaSecurityAccess() {
        if (javaSecurityAccess == null) {
            unsafe.ensureClassInitialized(AccessController.class);
        }
        return javaSecurityAccess;
    }

    public static JavaUtilZipFileAccess getJavaUtilZipFileAccess() {
        if (javaUtilZipFileAccess == null)
            unsafe.ensureClassInitialized(java.util.zip.ZipFile.class);
        return javaUtilZipFileAccess;
    }

    public static void setJavaUtilZipFileAccess(JavaUtilZipFileAccess access) {
        javaUtilZipFileAccess = access;
    }

    public static void setJavaAWTAccess(JavaAWTAccess jaa) {
        javaAWTAccess = jaa;
    }

    public static JavaAWTAccess getJavaAWTAccess() {
        // this may return null in which case calling code needs to
        // provision for.
        if (javaAWTAccess == null) {
            return null;
        }
        return javaAWTAccess;
    }

    public static JavaObjectInputStreamReadString getJavaObjectInputStreamReadString() {
        if (javaObjectInputStreamReadString == null) {
            unsafe.ensureClassInitialized(ObjectInputStream.class);
        }
        return javaObjectInputStreamReadString;
    }

    public static void setJavaObjectInputStreamReadString(JavaObjectInputStreamReadString access) {
        javaObjectInputStreamReadString = access;
    }

    public static JavaObjectInputStreamAccess getJavaObjectInputStreamAccess() {
        if (javaObjectInputStreamAccess == null) {
            unsafe.ensureClassInitialized(ObjectInputStream.class);
        }
        return javaObjectInputStreamAccess;
    }

    public static void setJavaObjectInputStreamAccess(JavaObjectInputStreamAccess access) {
        javaObjectInputStreamAccess = access;
    }

    public static void setJavaSecuritySignatureAccess(JavaSecuritySignatureAccess jssa) {
        javaSecuritySignatureAccess = jssa;
    }

    public static JavaSecuritySignatureAccess getJavaSecuritySignatureAccess() {
        if (javaSecuritySignatureAccess == null) {
            unsafe.ensureClassInitialized(Signature.class);
        }
        return javaSecuritySignatureAccess;
    }

    public static void setJavaxCryptoSealedObjectAccess(JavaxCryptoSealedObjectAccess jcsoa) {
        javaxCryptoSealedObjectAccess = jcsoa;
    }

    public static JavaxCryptoSealedObjectAccess getJavaxCryptoSealedObjectAccess() {
        if (javaxCryptoSealedObjectAccess == null) {
            unsafe.ensureClassInitialized(SealedObject.class);
        }
        return javaxCryptoSealedObjectAccess;
    }

    public static void setTenantAccess(TenantAccess access) {
        tenantAccess = access;
    }

    public static TenantAccess getTenantAccess() {
        return tenantAccess;
    }

    public static WispEngineAccess getWispEngineAccess() {
        return wispEngineAccess;
    }

    public static void setWispEngineAccess(WispEngineAccess wispEngineAccess) {
        SharedSecrets.wispEngineAccess = wispEngineAccess;
    }

    public static UnsafeAccess getUnsafeAccess() {
        return Unsafe.access;
    }

    public static IOEventAccess getIOEventAccess() {
        if (ioEventAccess == null) {
            IOEventAccess.initializeEvent();
        }
        return ioEventAccess;
    }

    public static void setIOEventAccess(IOEventAccess ioEventAccess) {
        SharedSecrets.ioEventAccess = ioEventAccess;
    }

    public static IOUtilAccess getIoUtilAccess() {
        if (ioUtilAccess == null) {
            unsafe.ensureClassInitialized(IOUtil.class);
        }
        return ioUtilAccess;
    }

    public static void setIoUtilAccess(IOUtilAccess ioUtilAccess) {
        SharedSecrets.ioUtilAccess = ioUtilAccess;
    }

    public static NetAccess getNetAccess() {
        if (netAccess == null) {
            unsafe.ensureClassInitialized(Net.class);
        }
        return netAccess;
    }

    public static void setNetAccess(NetAccess netAccess) {
        SharedSecrets.netAccess = netAccess;
    }
}

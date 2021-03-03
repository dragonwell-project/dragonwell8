/*
 * Copyright (c) 2010, 2013, Oracle and/or its affiliates. All rights reserved.
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

package jdk.nashorn.internal.runtime;

import java.security.CodeSource;
import java.util.Objects;

/**
 * Responsible for loading script generated classes.
 *
 */
final class ScriptLoader extends NashornLoader {
    private static final String NASHORN_PKG_PREFIX = "jdk.nashorn.internal.";

    private final Context context;

    /*package-private*/ Context getContext() {
        return context;
    }

    /**
     * Constructor.
     */
    ScriptLoader(final Context context) {
        super(context.getStructLoader());
        this.context = context;
    }

    @Override
    protected Class<?> loadClass(final String name, final boolean resolve) throws ClassNotFoundException {
        checkPackageAccess(name);
        return super.loadClass(name, resolve);
    }


     @Override
     protected Class<?> findClass(String name) throws ClassNotFoundException {
         final ClassLoader appLoader = context.getAppLoader();

         /*
          * If the appLoader is null, don't bother side-delegating to it!
          * Bootloader has been already attempted via parent loader
          * delegation from the "loadClass" method.
          *
          * Also, make sure that we don't delegate to the app loader
          * for nashorn's own classes or nashorn generated classes!
          */
         if (appLoader == null || name.startsWith(NASHORN_PKG_PREFIX)) {
             throw new ClassNotFoundException(name);
         }

         /*
          * This split-delegation is used so that caller loader
          * based resolutions of classes would work. For example,
          * java.sql.DriverManager uses caller's class loader to
          * get Driver instances. Without this split-delegation
          * a script class evaluating DriverManager.getDrivers()
          * will not get back any JDBC driver!
          */
         return appLoader.loadClass(name);
     }

    // package-private and private stuff below this point

    /**
     * Install a class for use by the Nashorn runtime
     *
     * @param name Binary name of class.
     * @param data Class data bytes.
     * @param cs CodeSource code source of the class bytes.
     *
     * @return Installed class.
     */
    synchronized Class<?> installClass(final String name, final byte[] data, final CodeSource cs) {
        return defineClass(name, data, 0, data.length, Objects.requireNonNull(cs));
    }
}

/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

package jdk.xml.internal;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.InputStream;
import java.security.AccessController;
import java.security.PrivilegedAction;
import java.security.PrivilegedActionException;
import java.security.PrivilegedExceptionAction;
import java.util.Properties;

/**
 * This class contains utility methods for reading resources in the JAXP packages
 */
class SecuritySupport {
    /**
     * Cache for properties in java.home/lib/jaxp.properties
     */
    static final Properties cacheProps = new Properties();

    /**
     * Flag indicating whether java.home/lib/jaxp.properties has been read
     */
    static volatile boolean firstTime = true;

    private SecuritySupport() {}

    /**
     * Reads JAXP system property with privilege
     *
     * @param propName the name of the property
     * @return the value of the property
     */
    public static String getSystemProperty(final String propName) {
        return AccessController.doPrivileged(new PrivilegedAction<String>() {
            @Override
            public String run() {
                return System.getProperty(propName);
            }
        });
    }

    /**
     * Reads a system property.
     *
     * @param <T> the type of the property value
     * @param type the type of the property value
     * @param propName the name of the property
     * @param defValue the default value
     * @return the value of the property, or the default value of no system
     * property is found
     */
    public static <T> T getJAXPSystemProperty(Class<T> type, String propName, String defValue) {
        String value = getJAXPSystemProperty(propName);
        if (value == null) {
            value = defValue;
        }
        if (Integer.class.isAssignableFrom(type)) {
            return type.cast(Integer.parseInt(value));
        } else if (Boolean.class.isAssignableFrom(type)) {
            return type.cast(Boolean.parseBoolean(value));
        }
        return type.cast(value);
    }

    /**
     * Reads JAXP system property in this order: system property,
     * $java.home/lib/jaxp.properties if the system property is not specified
     *
     * @param propName the name of the property
     * @return the value of the property
     */
    public static String getJAXPSystemProperty(String propName) {
        String value = getSystemProperty(propName);
        if (value == null) {
            value = readJAXPProperty(propName);
        }
        return value;
    }

    /**
     * Reads the specified property from $java.home/lib/jaxp.properties
     *
     * @param propName the name of the property
     * @return the value of the property
     */
    public static String readJAXPProperty(String propName) {
        String value = null;
        InputStream is = null;
        try {
            if (firstTime) {
                synchronized (cacheProps) {
                    if (firstTime) {
                        String configFile = getSystemProperty("java.home") + File.separator
                                + "lib" + File.separator + "jaxp.properties";
                        File f = new File(configFile);
                        if (getFileExists(f)) {
                            is = getFileInputStream(f);
                            cacheProps.load(is);
                        }
                        firstTime = false;
                    }
                }
            }
            value = cacheProps.getProperty(propName);

        } catch (IOException ex) {
        } finally {
            if (is != null) {
                try {
                    is.close();
                } catch (IOException ex) {}
            }
        }

        return value;
    }

//------------------- private methods ---------------------------
    static boolean getFileExists(final File f) {
        return AccessController.doPrivileged(new PrivilegedAction<Boolean>() {
            @Override
            public Boolean run() {
                return f.exists() ? Boolean.TRUE : Boolean.FALSE;
            }
        });
    }

    static FileInputStream getFileInputStream(final File file) throws FileNotFoundException {
        try {
            return AccessController.doPrivileged(new PrivilegedExceptionAction<FileInputStream>() {
                @Override
                public FileInputStream run() throws Exception {
                    return new FileInputStream(file);
                }
            });
        } catch (PrivilegedActionException e) {
            throw (FileNotFoundException) e.getException();
        }
    }
}

/*
 * Copyright (c) 2016, 2022, Oracle and/or its affiliates. All rights reserved.
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

import com.sun.org.apache.xalan.internal.xsltc.trax.TransformerFactoryImpl;
import com.sun.org.apache.xerces.internal.impl.Constants;
import com.sun.org.apache.xerces.internal.jaxp.DocumentBuilderFactoryImpl;
import com.sun.org.apache.xerces.internal.jaxp.SAXParserFactoryImpl;
import javax.xml.XMLConstants;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.parsers.ParserConfigurationException;
import javax.xml.parsers.SAXParserFactory;
import javax.xml.transform.TransformerConfigurationException;
import javax.xml.transform.sax.SAXTransformerFactory;
import org.w3c.dom.Document;
import org.xml.sax.SAXException;
import org.xml.sax.SAXNotRecognizedException;
import org.xml.sax.SAXNotSupportedException;
import org.xml.sax.XMLReader;

/**
 * Constants for use across JAXP processors.
 */
public class JdkXmlUtils {
    private static final String DOM_FACTORY_ID = "javax.xml.parsers.DocumentBuilderFactory";
    private static final String SAX_FACTORY_ID = "javax.xml.parsers.SAXParserFactory";
    private static final String SAX_DRIVER = "org.xml.sax.driver";

    /**
     * Xerces features
     */
    public static final String NAMESPACES_FEATURE =
        Constants.SAX_FEATURE_PREFIX + Constants.NAMESPACES_FEATURE;
    public static final String NAMESPACE_PREFIXES_FEATURE =
        Constants.SAX_FEATURE_PREFIX + Constants.NAMESPACE_PREFIXES_FEATURE;

    /**
     * jdk.xml.overrideDefaultParser: enables the use of a 3rd party's parser
     * implementation to override the system-default parser.
     */
    public static final String OVERRIDE_PARSER = "jdk.xml.overrideDefaultParser";
    public static final boolean OVERRIDE_PARSER_DEFAULT = SecuritySupport.getJAXPSystemProperty(
                    Boolean.class, OVERRIDE_PARSER, "false");

    /**
     * Values for a feature
     */
    public static final String FEATURE_TRUE = "true";
    public static final String FEATURE_FALSE = "false";

    /**
     * The system-default factory
     */
    private static final SAXParserFactory defaultSAXFactory = getSAXFactory(false);

    /**
     * Returns the value.
     *
     * @param value the specified value
     * @param defValue the default value
     * @return the value, or the default value if the value is null
     */
    public static int getValue(Object value, int defValue) {
        if (value == null) {
            return defValue;
        }

        if (value instanceof Number) {
            return ((Number) value).intValue();
        } else if (value instanceof String) {
            return Integer.parseInt(String.valueOf(value));
        } else {
            throw new IllegalArgumentException("Unexpected class: "
                    + value.getClass());
        }
    }

    /**
     * Sets the XMLReader instance with the specified property if the the
     * property is supported, ignores error if not, issues a warning if so
     * requested.
     *
     * @param reader an XMLReader instance
     * @param property the name of the property
     * @param value the value of the property
     * @param warn a flag indicating whether a warning should be issued
     */
    public static void setXMLReaderPropertyIfSupport(XMLReader reader, String property,
            Object value, boolean warn) {
        try {
            reader.setProperty(property, value);
        } catch (SAXNotRecognizedException | SAXNotSupportedException e) {
            if (warn) {
                XMLSecurityManager.printWarning(reader.getClass().getName(),
                        property, e);
            }
        }
    }

    /**
     * Returns an XMLReader instance. If overrideDefaultParser is requested, use
     * SAXParserFactory or XMLReaderFactory, otherwise use the system-default
     * SAXParserFactory to locate an XMLReader.
     *
     * @param overrideDefaultParser a flag indicating whether a 3rd party's
     * parser implementation may be used to override the system-default one
     * @param secureProcessing a flag indicating whether secure processing is
     * requested
     * @return an XMLReader instance
     */
    public static XMLReader getXMLReader(boolean overrideDefaultParser,
            boolean secureProcessing) {
        SAXParserFactory saxFactory;
        XMLReader reader = null;
        String spSAXDriver = SecuritySupport.getSystemProperty(SAX_DRIVER);
        if (spSAXDriver != null) {
            reader = getXMLReaderWXMLReaderFactory();
        } else if (overrideDefaultParser) {
            reader = getXMLReaderWSAXFactory(overrideDefaultParser);
        }

        if (reader != null) {
            if (secureProcessing) {
                try {
                    reader.setFeature(XMLConstants.FEATURE_SECURE_PROCESSING, secureProcessing);
                } catch (SAXException e) {
                    XMLSecurityManager.printWarning(reader.getClass().getName(),
                            XMLConstants.FEATURE_SECURE_PROCESSING, e);
                }
            }
            try {
                reader.setFeature(NAMESPACES_FEATURE, true);
                reader.setFeature(NAMESPACE_PREFIXES_FEATURE, false);
            } catch (SAXException se) {
                // older version of a parser
            }
            return reader;
        }

        // use the system-default
        saxFactory = defaultSAXFactory;

        try {
            reader = saxFactory.newSAXParser().getXMLReader();
        } catch (ParserConfigurationException | SAXException ex) {
            // shall not happen with the system-default reader
        }
        return reader;
    }

    /**
     * Creates a system-default DOM Document.
     *
     * @return a DOM Document instance
     */
    public static Document getDOMDocument() {
        try {
            DocumentBuilderFactory dbf = JdkXmlUtils.getDOMFactory(false);
            return dbf.newDocumentBuilder().newDocument();
        } catch (ParserConfigurationException pce) {
            // can never happen with the system-default configuration
        }
        return null;
    }

    /**
     * Returns a DocumentBuilderFactory instance.
     *
     * @param overrideDefaultParser a flag indicating whether the system-default
     * implementation may be overridden. If the system property of the
     * DOM factory ID is set, override is always allowed.
     *
     * @return a DocumentBuilderFactory instance.
     */
    public static DocumentBuilderFactory getDOMFactory(boolean overrideDefaultParser) {
        boolean override = overrideDefaultParser;
        String spDOMFactory = SecuritySupport.getJAXPSystemProperty(DOM_FACTORY_ID);

        if (spDOMFactory != null && System.getSecurityManager() == null) {
            override = true;
        }
        DocumentBuilderFactory dbf
                = !override
                        ? new DocumentBuilderFactoryImpl()
                        : DocumentBuilderFactory.newInstance();
        dbf.setNamespaceAware(true);
        // false is the default setting. This step here is for compatibility
        dbf.setValidating(false);
        return dbf;
    }

    /**
     * Returns a SAXParserFactory instance.
     *
     * @param overrideDefaultParser a flag indicating whether the system-default
     * implementation may be overridden. If the system property of the
     * DOM factory ID is set, override is always allowed.
     *
     * @return a SAXParserFactory instance.
     */
    public static SAXParserFactory getSAXFactory(boolean overrideDefaultParser) {
        boolean override = overrideDefaultParser;
        String spSAXFactory = SecuritySupport.getJAXPSystemProperty(SAX_FACTORY_ID);
        if (spSAXFactory != null && System.getSecurityManager() == null) {
            override = true;
        }

        SAXParserFactory factory
                = !override
                        ? new SAXParserFactoryImpl()
                        : SAXParserFactory.newInstance();
        factory.setNamespaceAware(true);
        return factory;
    }

    public static SAXTransformerFactory getSAXTransformFactory(boolean overrideDefaultParser) {
        SAXTransformerFactory tf = overrideDefaultParser
                ? (SAXTransformerFactory) SAXTransformerFactory.newInstance()
                : (SAXTransformerFactory) new TransformerFactoryImpl();
        try {
            tf.setFeature(OVERRIDE_PARSER, overrideDefaultParser);
        } catch (TransformerConfigurationException ex) {
            // ignore since it'd never happen with the JDK impl.
        }
        return tf;
    }

    /**
     * Returns the external declaration for a DTD construct.
     *
     * @param publicId the public identifier
     * @param systemId the system identifier
     * @return a DTD external declaration
     */
    public static String getDTDExternalDecl(String publicId, String systemId) {
        StringBuilder sb = new StringBuilder();
        if (null != publicId) {
            sb.append(" PUBLIC ");
            sb.append(quoteString(publicId));
        }

        if (null != systemId) {
            if (null == publicId) {
                sb.append(" SYSTEM ");
            } else {
                sb.append(" ");
            }

            sb.append(quoteString(systemId));
        }
        return sb.toString();
    }

    /**
     * Returns the input string quoted with double quotes or single ones if
     * there is a double quote in the string.
     * @param s the input string, can not be null
     * @return the quoted string
     */
    private static String quoteString(String s) {
        char c = (s.indexOf('"') > -1) ? '\'' : '"';
        return c + s + c;
    }

    private static XMLReader getXMLReaderWSAXFactory(boolean overrideDefaultParser) {
        SAXParserFactory saxFactory = getSAXFactory(overrideDefaultParser);
        try {
            return saxFactory.newSAXParser().getXMLReader();
        } catch (ParserConfigurationException | SAXException ex) {
            return getXMLReaderWXMLReaderFactory();
        }
    }

    @SuppressWarnings("deprecation")
    private static XMLReader getXMLReaderWXMLReaderFactory() {
        try {
            return org.xml.sax.helpers.XMLReaderFactory.createXMLReader();
        } catch (SAXException ex1) {
        }
        return null;
    }
}

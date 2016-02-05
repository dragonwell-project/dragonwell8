/*
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
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

import java.io.ByteArrayInputStream;
import javax.xml.parsers.SAXParser;
import javax.xml.parsers.SAXParserFactory;

import org.testng.annotations.DataProvider;
import org.testng.annotations.Test;
import org.xml.sax.SAXParseException;
import org.xml.sax.helpers.DefaultHandler;

/**
 * @test
 * @bug 8072081
 * @summary verifies that supplementary characters are supported as character
 * data in xml 1.0, and also names in xml 1.1.
 * @run testng/othervm SupplementaryChars
 */
/*
 * Joe Wang (huizhe.wang@oracle.com)
 */

public class SupplementaryChars {

    @Test(dataProvider = "supported")
    public void test(String xml) throws Exception {
        ByteArrayInputStream stream = new ByteArrayInputStream(xml.getBytes("UTF-8"));
        getParser().parse(stream, new DefaultHandler());
        stream.close();
    }

    @Test(dataProvider = "unsupported", expectedExceptions = SAXParseException.class)
    public void testInvalid(String xml) throws Exception {
        ByteArrayInputStream stream = new ByteArrayInputStream(xml.getBytes("UTF-8"));
        getParser().parse(stream, new DefaultHandler());
        stream.close();
    }

    @DataProvider(name = "supported")
    private Object[][] supported() {

        return new Object[][] {
            {"<?xml version=\"1.0\"?><tag>\uD840\uDC0B</tag>"},
            {"<?xml version=\"1.0\"?><!-- \uD840\uDC0B --><tag/>"},
            {"<?xml version=\"1.1\"?><tag\uD840\uDC0B>in tag name</tag\uD840\uDC0B>"},
            {"<?xml version=\"1.1\"?><tag attr\uD840\uDC0B=\"in attribute\">in attribute name</tag>"},
            {"<?xml version=\"1.1\"?><tag>\uD840\uDC0B</tag>"},
            {"<?xml version=\"1.1\"?><!-- \uD840\uDC0B --><dontCare/>"}
        };
    }

    @DataProvider(name = "unsupported")
    private Object[][] unsupported() {
        return new Object[][] {
            {"<?xml version=\"1.0\"?><tag\uD840\uDC0B>in tag name</tag\uD840\uDC0B>"},
            {"<?xml version=\"1.0\"?><tag attr\uD840\uDC0B=\"in attribute\">in attribute name</tag>"}
        };
    }

    private SAXParser getParser() {
        SAXParser parser = null;
        try {
            SAXParserFactory factory = SAXParserFactory.newInstance();
            parser = factory.newSAXParser();
        } catch (Exception e) {
            throw new RuntimeException(e.getMessage());
        }
        return parser;
    }
}

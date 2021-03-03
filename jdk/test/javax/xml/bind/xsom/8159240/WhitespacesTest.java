/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
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

/*
 * @test
 * @bug 8159240
 * @summary Check if types and names with whitespaces are collapsed by
 *          XSOM schema parser
 * @compile -XDignore.symbol.file WhitespacesTest.java
 * @run testng WhitespacesTest
 *
 */

import com.sun.xml.internal.xsom.parser.XSOMParser;
import java.io.File;
import java.nio.file.Paths;
import javax.xml.parsers.SAXParserFactory;
import org.testng.annotations.DataProvider;
import org.testng.annotations.Test;
import org.xml.sax.ErrorHandler;
import org.xml.sax.SAXException;
import org.xml.sax.SAXParseException;

public class WhitespacesTest {

    @Test(dataProvider = "TestSchemaFiles")
    public void testWhitespacesCollapse(String schemaFile) throws Exception {
        XSOMParser parser = new XSOMParser(SAXParserFactory.newInstance());
        XsomErrorHandler eh = new XsomErrorHandler();

        parser.setErrorHandler(eh);
        parser.parse(getSchemaFile(schemaFile));

        if (eh.gotError) {
            throw new RuntimeException("XSOM parser encountered error", eh.e);
        }
    }

    // Get location of schema file located in test source directory
    private static File getSchemaFile(String filename) {
        String testSrc = System.getProperty("test.src", ".");
        return Paths.get(testSrc).resolve(filename).toFile();
    }

    @DataProvider(name = "TestSchemaFiles")
    private Object[][] dataTestSchemaFiles() {
        return new Object[][] {
            {"complexType.xsd"},
            {"complexTypeExtension.xsd"},
            {"complexTypeRestriction.xsd"},
            {"identityConstraint.xsd"},
            {"particlesAndAttributes.xsd"},
            {"simpleType.xsd"}
        };

    }

    // Test XSOM error handler
    private static class XsomErrorHandler implements ErrorHandler {

        public boolean gotError = false;
        public Exception e = null;

        @Override
        public void warning(SAXParseException ex) throws SAXException {
            System.err.println("XSOM warning:" + ex);
        }

        @Override
        public void error(SAXParseException ex) throws SAXException {
            System.err.println("XSOM error:" + ex);
            e = ex;
            gotError = true;
        }

        @Override
        public void fatalError(SAXParseException ex) throws SAXException {
            System.err.println("XSOM fatal error:" + ex);
            e = ex;
            gotError = true;
        }
    }
}

/*
 * Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
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

/* @test
 * @bug 8162598
 * @summary Test XSLTC handling of namespaces, especially empty namespace definitions to reset the
 *          default namespace.
 * @run testng/othervm TransformerTest
 */

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;

import javax.xml.transform.Transformer;
import javax.xml.transform.TransformerException;
import javax.xml.transform.TransformerFactory;
import javax.xml.transform.dom.DOMResult;
import javax.xml.transform.stream.StreamSource;

import org.testng.Assert;
import org.testng.annotations.Test;
import org.w3c.dom.Document;
import org.w3c.dom.Node;

import com.sun.org.apache.xml.internal.serialize.OutputFormat;
import com.sun.org.apache.xml.internal.serialize.XMLSerializer;

public class TransformerTest {

    private Transformer createTransformerFromInputstream(InputStream xslStream) throws TransformerException {
        return TransformerFactory.newInstance().newTransformer(new StreamSource(xslStream));
    }

    private Document transformInputStreamToDocument(Transformer transformer, InputStream sourceStream) throws TransformerException {
        DOMResult response = new DOMResult();
        transformer.transform(new StreamSource(sourceStream), response);
        return (Document)response.getNode();
    }

    /**
     * Utility method for testBug8162598().
     * Provides a convenient way to check/assert the expected namespaces
     * of a Node and its siblings.
     *
     * @param test
     * The node to check
     * @param nstest
     * Expected namespace of the node
     * @param nsb
     * Expected namespace of the first sibling
     * @param nsc
     * Expected namespace of the first sibling of the first sibling
     */
    private void checkNodeNS8162598(Node test, String nstest, String nsb, String nsc) {
        String testNodeName = test.getNodeName();
        if (nstest == null) {
            Assert.assertNull(test.getNamespaceURI(), "unexpected namespace for " + testNodeName);
        } else {
            Assert.assertEquals(test.getNamespaceURI(), nstest, "unexpected namespace for " + testNodeName);
        }
        Node b = test.getChildNodes().item(0);
        if (nsb == null) {
            Assert.assertNull(b.getNamespaceURI(), "unexpected namespace for " + testNodeName + "->b");
        } else {
            Assert.assertEquals(b.getNamespaceURI(), nsb, "unexpected namespace for " + testNodeName + "->b");
        }
        Node c = b.getChildNodes().item(0);
        if (nsc == null) {
            Assert.assertNull(c.getNamespaceURI(), "unexpected namespace for " + testNodeName + "->b->c");
        } else {
            Assert.assertEquals(c.getNamespaceURI(), nsc, "unexpected namespace for " + testNodeName + "->b->c");
        }
    }

    /*
     * @bug 8162598
     * @summary Test XSLTC handling of namespaces, especially empty namespace definitions to reset the
     *          default namespace
     */
    @Test
    public final void testBug8162598() throws IOException, TransformerException {
        final String LINE_SEPARATOR = System.getProperty("line.separator");

        final String xsl =
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" + LINE_SEPARATOR +
            "<xsl:stylesheet xmlns:xsl=\"http://www.w3.org/1999/XSL/Transform\" version=\"1.0\">" + LINE_SEPARATOR +
            "    <xsl:template match=\"/\">" + LINE_SEPARATOR +
            "        <root xmlns=\"ns1\">" + LINE_SEPARATOR +
            "            <xsl:call-template name=\"transform\"/>" + LINE_SEPARATOR +
            "        </root>" + LINE_SEPARATOR +
            "    </xsl:template>" + LINE_SEPARATOR +
            "    <xsl:template name=\"transform\">" + LINE_SEPARATOR +
            "        <test1 xmlns=\"ns2\"><b xmlns=\"ns2\"><c xmlns=\"\"></c></b></test1>" + LINE_SEPARATOR +
            "        <test2 xmlns=\"ns1\"><b xmlns=\"ns2\"><c xmlns=\"\"></c></b></test2>" + LINE_SEPARATOR +
            "        <test3><b><c xmlns=\"\"></c></b></test3>" + LINE_SEPARATOR +
            "        <test4 xmlns=\"\"><b><c xmlns=\"\"></c></b></test4>" + LINE_SEPARATOR +
            "        <test5 xmlns=\"ns1\"><b><c xmlns=\"\"></c></b></test5>" + LINE_SEPARATOR +
            "        <test6 xmlns=\"\"/>" + LINE_SEPARATOR +
            "    </xsl:template>" + LINE_SEPARATOR +
            "</xsl:stylesheet>";


        final String sourceXml =
                "<?xml version=\"1.0\" encoding=\"UTF-8\"?><aaa></aaa>" + LINE_SEPARATOR;

        System.out.println("Stylesheet:");
        System.out.println("=============================");
        System.out.println(xsl);
        System.out.println();

        System.out.println("Source before transformation:");
        System.out.println("=============================");
        System.out.println(sourceXml);
        System.out.println();

        System.out.println("Result after transformation:");
        System.out.println("============================");
        Document document = transformInputStreamToDocument(
            createTransformerFromInputstream(new ByteArrayInputStream(xsl.getBytes())),
                                             new ByteArrayInputStream(sourceXml.getBytes()));
        OutputFormat format = new OutputFormat();
        format.setIndenting(true);
        new XMLSerializer(System.out, format).serialize(document);
        System.out.println();
        checkNodeNS8162598(document.getElementsByTagName("test1").item(0), "ns2", "ns2", null);
        checkNodeNS8162598(document.getElementsByTagName("test2").item(0), "ns1", "ns2", null);
        checkNodeNS8162598(document.getElementsByTagName("test3").item(0), null, null, null);
        checkNodeNS8162598(document.getElementsByTagName("test4").item(0), null, null, null);
        checkNodeNS8162598(document.getElementsByTagName("test5").item(0), "ns1", "ns1", null);
        Assert.assertNull(document.getElementsByTagName("test6").item(0).getNamespaceURI(), "unexpected namespace for test6");
    }
}

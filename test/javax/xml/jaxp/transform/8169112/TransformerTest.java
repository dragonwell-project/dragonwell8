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
 * @bug 8169112
 * @summary Test compilation of large xsl file with outlining.
 * @run testng/othervm TransformerTest
 */

import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.FileNotFoundException;

import javax.xml.transform.Transformer;
import javax.xml.transform.TransformerException;
import javax.xml.transform.TransformerFactory;
import javax.xml.transform.stream.StreamResult;
import javax.xml.transform.stream.StreamSource;

import org.testng.annotations.Test;

public class TransformerTest {

     /**
     * @bug 8169112
     * @summary Test compilation of large xsl file with outlining.
     *
     * This test merely compiles a large xsl file and tests if its bytecode
     * passes verification by invoking the transform() method for
     * dummy content. The test succeeds if no Exception is thrown
     */
    @Test
    public final void testBug8169112() throws FileNotFoundException,
        TransformerException
    {
        TransformerFactory tf = TransformerFactory.newInstance();
        String xslFile = getClass().getResource("Bug8169112.xsl").toString();
        Transformer t = tf.newTransformer(new StreamSource(xslFile));
        String xmlIn = "<?xml version=\"1.0\"?><DOCROOT/>";
        ByteArrayInputStream bis = new ByteArrayInputStream(xmlIn.getBytes());
        ByteArrayOutputStream bos = new ByteArrayOutputStream();
        t.transform(new StreamSource(bis), new StreamResult(bos));
    }
}

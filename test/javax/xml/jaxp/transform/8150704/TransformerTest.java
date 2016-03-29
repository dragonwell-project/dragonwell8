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
 * @bug 8150704
 * @summary Test that XSL transformation with lots of temporary result trees will not run out of DTM IDs.
 * @run testng/othervm TransformerTest
 */

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.io.IOException;
import java.io.StringWriter;
import javax.xml.transform.Transformer;
import javax.xml.transform.TransformerException;
import javax.xml.transform.TransformerFactory;
import javax.xml.transform.stream.StreamResult;
import javax.xml.transform.stream.StreamSource;
import org.testng.Assert;
import org.testng.annotations.DataProvider;
import org.testng.annotations.Test;


public class TransformerTest {

    @Test(dataProvider = "bug8150704")
    public final void testBug8150704(String xsl, String xml, String ref) throws Exception {
        System.out.println("Testing transformation of "+xml);
        Transformer transformer = createTransformerFromResource(xsl);
        StringWriter result = transformResourceToStringWriter(transformer, xml);
        String resultstring = result.toString().replaceAll("\\r\\n", "\n").replaceAll("\\r", "\n");
        String reference = getFileContentAsString(new File(getClass().getResource(ref).getPath()));
        Assert.assertEquals(resultstring, reference, "Output of transformation of "+xml+" does not match reference");
    }

    @DataProvider(name = "bug8150704")
    private Object[][] dataBug8150704() {
        return new Object[][] {
            //xsl file, xml file, reference file
            {"Bug8150704-1.xsl", "Bug8150704-1.xml", "Bug8150704-1.ref"},
            {"Bug8150704-2.xsl", "Bug8150704-2.xml", "Bug8150704-2.ref"},
        };
    }

    private Transformer createTransformerFromResource(String xslResource) throws TransformerException {
        return TransformerFactory.newInstance().newTransformer(new StreamSource(getClass().getResource(xslResource).toString()));
    }

    private StringWriter transformResourceToStringWriter(Transformer transformer, String xmlResource) throws TransformerException {
        StringWriter sw = new StringWriter();
        transformer.transform(new StreamSource(getClass().getResource(xmlResource).toString()), new StreamResult(sw));
        return sw;
    }

    /**
     * Reads the contents of the given file into a string.
     * WARNING: this method adds a final line feed even if the last line of the file doesn't contain one.
     *
     * @param f
     * The file to read
     * @return The content of the file as a string, with line terminators as \"n"
     * for all platforms
     * @throws IOException
     * If there was an error reading
     */
    private String getFileContentAsString(File f) throws IOException {
        try (BufferedReader reader = new BufferedReader(new FileReader(f))) {
            String line;
            StringBuilder sb = new StringBuilder();
            while ((line = reader.readLine()) != null) {
                sb.append(line).append("\n");
            }
            return sb.toString();
        }
    }
}

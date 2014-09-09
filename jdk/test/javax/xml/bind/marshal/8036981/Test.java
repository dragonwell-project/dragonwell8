/*
 * Copyright (c) 2014, Oracle and/or its affiliates. All rights reserved.
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
 * @test Test.java
 * @bug 8036981
 * @summary JAXB not preserving formatting during unmarshalling/marshalling
 * @compile Good.java Main.java ObjectFactory.java Root.java
 * @run main/othervm Test
 */

import javax.xml.bind.JAXBContext;
import javax.xml.bind.Marshaller;
import javax.xml.bind.Unmarshaller;
import java.io.File;
import java.io.StringWriter;

/**
 * Test for marshalling and unmarshalling mixed and unmixed content
 */
public class Test {

    private static final String EXPECTED = "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?><main><Root>\n" +
            "        <SomeTag>\n" +
            "            <AChildTag>\n" +
            "                <AnotherChildTag/>\n" +
            "                <AnotherChildTag/>\n" +
            "            </AChildTag>\n" +
            "        </SomeTag>\n" +
            "    </Root><Good><VeryGood><TheBest><MegaSuper/><MegaSuper/>\n" +
            "            </TheBest>\n" +
            "        </VeryGood></Good></main>";

    public static void main(String[] args) throws Exception {
        JAXBContext jc = JAXBContext.newInstance("testjaxbcontext");
        Unmarshaller u = jc.createUnmarshaller();
        Object result = u.unmarshal(new File(System.getProperty("test.src", ".") + "/test.xml"));
        StringWriter sw = new StringWriter();
        Marshaller m = jc.createMarshaller();
        m.marshal(result, sw);
        System.out.println("Expected:" + EXPECTED);
        System.out.println("Observed:" + sw.toString());
        if (!EXPECTED.equals(sw.toString())) {
            throw new Exception("Unmarshal/Marshal generates different content");
        }
    }
}

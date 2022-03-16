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

/*
 * @test
 * @bug 8028363
 * @summary XmlGregorianCalendarImpl.getTimezone() returns wrong timezone if
 * offset's minutes value is less then 10 (e.g. GMT+10:05)
 * @run main TestXMLGregorianCalendarTimeZone
*/

import javax.xml.datatype.DatatypeConfigurationException;
import javax.xml.datatype.DatatypeConstants;
import javax.xml.datatype.DatatypeFactory;
import javax.xml.datatype.XMLGregorianCalendar;
import java.util.TimeZone;

public class TestXMLGregorianCalendarTimeZone {

    public static void main(String[] args) throws DatatypeConfigurationException {
        for (int i = 0; i < 60; i++) {
            test(i);
        }
    }

    private static void test(int offsetMinutes)
            throws DatatypeConfigurationException {
        XMLGregorianCalendar calendar = DatatypeFactory.newInstance().
                newXMLGregorianCalendar();
        calendar.setTimezone(60 + offsetMinutes);
        TimeZone timeZone = calendar.getTimeZone(DatatypeConstants.FIELD_UNDEFINED);
        String expected = (offsetMinutes < 10 ? "GMT+01:0" : "GMT+01:")
                + offsetMinutes;
        if (!timeZone.getID().equals(expected)) {
            throw new RuntimeException("Test failed: expected timezone: " +
                    expected + " Actual: " + timeZone.getID());
        }
    }
}

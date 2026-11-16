/*
 * Copyright (c) 2012, 2026, Oracle and/or its affiliates. All rights reserved.
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

package build.tools.cldrconverter;

import java.io.File;
import java.io.IOException;
import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Date;
import org.xml.sax.Attributes;
import org.xml.sax.InputSource;
import org.xml.sax.SAXException;

class MetaZonesParseHandler extends AbstractLDMLHandler<String> {
    // "from"/"to" attribute values of <usesMetazone> in metaZones.xml
    private static final SimpleDateFormat MZ_TIME = new SimpleDateFormat("yyyy-MM-dd HH:mm");
    private static final Date DATE_MIN = new Date(Long.MIN_VALUE);
    private static final Date DATE_MAX = new Date(Long.MAX_VALUE);
    private static final Date NOW = new Date();

    private String tzid, metazone;

    MetaZonesParseHandler() {
    }

    @Override
    public InputSource resolveEntity(String publicID, String systemID) throws IOException, SAXException {
        // avoid HTTP traffic to unicode.org
        if (systemID.startsWith(CLDRConverter.SPPL_LDML_DTD_SYSTEM_ID)) {
            return new InputSource((new File(CLDRConverter.LOCAL_SPPL_LDML_DTD)).toURI().toString());
        }
        return null;
    }

    // metaZone: ID -> metazone
    // per locale: ID -> names, metazone -> names
    @Override
    public void startElement(String uri, String localName, String qName, Attributes attributes) throws SAXException {
        switch (qName) {
        case "timezone":
            tzid = attributes.getValue("type");
            pushContainer(qName, attributes);
            break;

        case "usesMetazone":
            // uses the time of the JDK build to determine metazones.
            Date from = parseMzTime(attributes.getValue("from"), DATE_MIN);
            Date to = parseMzTime(attributes.getValue("to"), DATE_MAX);

            if (from.before(NOW) && to.after(NOW)) {
                metazone = attributes.getValue("mzone");

                // Explicit metazone DST offsets. Only the "dst" offset is needed,
                // as "std" is used by default when it doesn't match.
                String dstOffset = attributes.getValue("dstOffset");
                if (dstOffset != null) {
                    CLDRConverter.explicitDstOffsets.put(tzid, dstOffset);
                }
            }

            pushIgnoredContainer(qName);
            break;

        case "version":
        case "generation":
            pushIgnoredContainer(qName);
            break;

        default:
            // treat anything else as a container
            pushContainer(qName, attributes);
            break;
        }
    }

    @Override
    public void endElement(String uri, String localName, String qName) throws SAXException {
        assert qName.equals(currentContainer.getqName()) : "current=" + currentContainer.getqName() + ", param=" + qName;
        switch (qName) {
        case "timezone":
            if (tzid == null) {
                throw new InternalError();
            } else if (metazone == null) {
                CLDRConverter.info("No metazone defined for %s%n", tzid);
            } else {
                put(tzid, metazone);
            }
            tzid = null;
            metazone = null;
            break;
        }
        currentContainer = currentContainer.getParent();
    }

    private static Date parseMzTime(String value, Date defaultValue) throws SAXException {
        if (value == null) {
            return defaultValue;
        }
        try {
            return MZ_TIME.parse(value);
        } catch (ParseException e) {
            throw new SAXException("invalid metazone time: " + value, e);
        }
    }
}

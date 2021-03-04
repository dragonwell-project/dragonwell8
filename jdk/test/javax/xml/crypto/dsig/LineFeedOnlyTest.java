/*
 * Copyright (c) 2005, 2020, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2020 Red Hat, Inc.
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

import java.io.File;
import java.io.FileInputStream;
import java.io.StringWriter;
import java.security.Key;
import java.security.KeyStore;
import java.security.cert.Certificate;
import java.util.Base64;
import java.util.Collections;

import javax.xml.crypto.Data;
import javax.xml.crypto.OctetStreamData;
import javax.xml.crypto.URIDereferencer;
import javax.xml.crypto.URIReference;
import javax.xml.crypto.URIReferenceException;
import javax.xml.crypto.XMLCryptoContext;
import javax.xml.crypto.dsig.CanonicalizationMethod;
import javax.xml.crypto.dsig.DigestMethod;
import javax.xml.crypto.dsig.Reference;
import javax.xml.crypto.dsig.SignatureMethod;
import javax.xml.crypto.dsig.SignedInfo;
import javax.xml.crypto.dsig.XMLSignature;
import javax.xml.crypto.dsig.XMLSignatureFactory;
import javax.xml.crypto.dsig.dom.DOMSignContext;
import javax.xml.crypto.dsig.keyinfo.KeyInfo;
import javax.xml.crypto.dsig.keyinfo.KeyInfoFactory;
import javax.xml.crypto.dsig.spec.C14NMethodParameterSpec;
import javax.xml.parsers.DocumentBuilder;
import javax.xml.parsers.DocumentBuilderFactory;
import javax.xml.transform.OutputKeys;
import javax.xml.transform.Transformer;
import javax.xml.transform.TransformerFactory;
import javax.xml.transform.dom.DOMSource;
import javax.xml.transform.stream.StreamResult;

import org.w3c.dom.Document;
import org.w3c.dom.Node;
import org.w3c.dom.NodeList;

/* @test
 * @bug 8236645
 * @summary Test "lineFeedOnly" property prevents CR in Base64 encoded output
 * @run main/othervm/timeout=300 -Dcom.sun.org.apache.xml.internal.security.lineFeedOnly=false LineFeedOnlyTest
 * @run main/othervm/timeout=300 -Dcom.sun.org.apache.xml.internal.security.lineFeedOnly=true LineFeedOnlyTest
 * @run main/othervm/timeout=300 -Dcom.sun.org.apache.xml.internal.security.lineFeedOnly=true
 *     -Dcom.sun.org.apache.xml.internal.security.ignoreLineBreaks=true LineFeedOnlyTest
 * @run main/othervm/timeout=300 LineFeedOnlyTest
 */
public class LineFeedOnlyTest {

    private final static String DIR = System.getProperty("test.src", ".");
    private final static String DATA_DIR =
            DIR + System.getProperty("file.separator") + "data";
    private final static String KEYSTORE =
            DATA_DIR + System.getProperty("file.separator") + "certs" +
            System.getProperty("file.separator") + "test.jks";
    private final static String STYLESHEET =
            "http://www.w3.org/TR/xml-stylesheet";

    private static XMLSignatureFactory fac;
    private static KeyInfoFactory kifac;
    private static DocumentBuilder db;
    private static CanonicalizationMethod withoutComments;
    private static SignatureMethod dsaSha1;
    private static DigestMethod sha1;
    private static Key signingKey;
    private static Certificate signingCert;
    private static KeyStore ks;
    private static URIDereferencer httpUd;

    // Much of this test is derived from GenerationTests. We use a separate file in order to test
    // when the system property "com.sun.org.apache.xml.internal.security.lineFeedOnly" is enabled/disabled.
    public static void main(String[] args) throws Exception {
        boolean lineFeedOnly = Boolean.getBoolean("com.sun.org.apache.xml.internal.security.lineFeedOnly");
        boolean ignoreLineBreaks = Boolean.getBoolean("com.sun.org.apache.xml.internal.security.ignoreLineBreaks");

        setup();
        test_create_signature_line_endings(lineFeedOnly, ignoreLineBreaks);
    }

    private static void setup() throws Exception {
        fac = XMLSignatureFactory.getInstance();
        kifac = fac.getKeyInfoFactory();
        DocumentBuilderFactory dbf = DocumentBuilderFactory.newInstance();
        dbf.setNamespaceAware(true);
        db = dbf.newDocumentBuilder();

        // get key & self-signed certificate from keystore
        FileInputStream fis = new FileInputStream(KEYSTORE);
        ks = KeyStore.getInstance("JKS");
        ks.load(fis, "changeit".toCharArray());
        signingKey = ks.getKey("user", "changeit".toCharArray());
        signingCert = ks.getCertificate("user");

        // create common objects
        withoutComments = fac.newCanonicalizationMethod
            (CanonicalizationMethod.INCLUSIVE, (C14NMethodParameterSpec)null);
        dsaSha1 = fac.newSignatureMethod(SignatureMethod.DSA_SHA1, null);
        sha1 = fac.newDigestMethod(DigestMethod.SHA1, null);

        httpUd = new HttpURIDereferencer();
    }

    private static void test_create_signature_line_endings(boolean lineFeedOnly,
        boolean ignoreLineBreaks) throws Exception {
        System.out.println("* Generating signature-line-endings.xml");
        System.out.println("* com.sun.org.apache.xml.internal.security.lineFeedOnly is "
                + String.valueOf(lineFeedOnly));
        System.out.println("* com.sun.org.apache.xml.internal.security.ignoreLineBreaks is "
                + String.valueOf(ignoreLineBreaks));

        // create reference
        Reference ref = fac.newReference(STYLESHEET, sha1);

        // create SignedInfo
        SignedInfo si = fac.newSignedInfo(withoutComments, dsaSha1,
                Collections.singletonList(ref));

        Document doc = db.newDocument();

        // create XMLSignature
        KeyInfo crt = kifac.newKeyInfo(Collections.singletonList
                (kifac.newX509Data(Collections.singletonList(signingCert))));
        XMLSignature sig = fac.newXMLSignature(si, crt);

        DOMSignContext dsc = new DOMSignContext(signingKey, doc);
        dsc.setURIDereferencer(httpUd);

        sig.sign(dsc);

        NodeList list = doc.getElementsByTagName("X509Certificate");
        if (list.getLength() != 1) {
            throw new Exception("Expected exactly one X509Certificate tag");
        }
        Node item = list.item(0);
        StringWriter writer = new StringWriter();
        Transformer tr = TransformerFactory.newInstance().newTransformer();
        tr.setOutputProperty(OutputKeys.METHOD, "xml");
        tr.setOutputProperty(OutputKeys.OMIT_XML_DECLARATION, "yes");
        tr.transform(new DOMSource(item.getFirstChild()), new StreamResult(writer));
        String actual = writer.toString();

        String expected;
        if (ignoreLineBreaks) {
            expected = Base64.getEncoder().encodeToString(signingCert.getEncoded());
        } else if (lineFeedOnly) {
            expected = Base64.getMimeEncoder(76, new byte[] {'\n'}).encodeToString(signingCert.getEncoded());
        } else {
            expected = Base64.getMimeEncoder().encodeToString(signingCert.getEncoded());
        }

        if (!expected.equals(actual)) {
            if (ignoreLineBreaks && actual.contains("\n")) {
                throw new Exception("ignoreLineBreaks did not take precedence over lineFeedOnly");
            } else if (lineFeedOnly && actual.contains("\r\n")) {
                throw new Exception("Expected LF only, but found CRLF");
            } else if (!lineFeedOnly && !actual.contains("\r\n")) {
                throw new Exception("Expected CRLF, but found LF only");
            }
            throw new Exception("Unexpected output in encoded certificate");
        }
    }

    /**
     * This URIDereferencer returns locally cached copies of http content to
     * avoid test failures due to network glitches, etc.
     */
    private static class HttpURIDereferencer implements URIDereferencer {
        private URIDereferencer defaultUd;

        HttpURIDereferencer() {
            defaultUd = XMLSignatureFactory.getInstance().getURIDereferencer();
        }

        public Data dereference(final URIReference ref, XMLCryptoContext ctx)
        throws URIReferenceException {
            String uri = ref.getURI();
            if (uri.equals(STYLESHEET)) {
                try {
                    FileInputStream fis = new FileInputStream(new File
                        (DATA_DIR, uri.substring(uri.lastIndexOf('/'))));
                    return new OctetStreamData(fis,ref.getURI(),ref.getType());
                } catch (Exception e) { throw new URIReferenceException(e); }
            }

            // fallback on builtin deref
            return defaultUd.dereference(ref, ctx);
        }
    }

}

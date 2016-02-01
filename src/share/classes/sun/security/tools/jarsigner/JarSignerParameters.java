/*
 * Copyright (c) 2016, Oracle and/or its affiliates. All rights reserved.
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

package sun.security.tools.jarsigner;

import java.security.cert.Certificate;
import java.security.cert.X509Certificate;
import java.net.URI;
import java.util.zip.*;

import com.sun.jarsigner.ContentSignerParameters;

class JarSignerParameters implements ContentSignerParameters {

    private String[] args;
    private URI tsa;
    private X509Certificate tsaCertificate;
    private byte[] signature;
    private String signatureAlgorithm;
    private X509Certificate[] signerCertificateChain;
    private byte[] content;
    private ZipFile source;
    private String tSAPolicyID;
    private String tSADigestAlg;

    /**
     * Create a new object.
     */
    JarSignerParameters(String[] args, URI tsa, X509Certificate tsaCertificate,
        String tSAPolicyID, String tSADigestAlg,
        byte[] signature, String signatureAlgorithm,
        X509Certificate[] signerCertificateChain, byte[] content,
        ZipFile source) {

        if (signature == null || signatureAlgorithm == null ||
            signerCertificateChain == null || tSADigestAlg == null) {
            throw new NullPointerException();
        }
        this.args = args;
        this.tsa = tsa;
        this.tsaCertificate = tsaCertificate;
        this.tSAPolicyID = tSAPolicyID;
        this.tSADigestAlg = tSADigestAlg;
        this.signature = signature;
        this.signatureAlgorithm = signatureAlgorithm;
        this.signerCertificateChain = signerCertificateChain;
        this.content = content;
        this.source = source;
    }

    /**
     * Retrieves the command-line arguments.
     *
     * @return The command-line arguments. May be null.
     */
    public String[] getCommandLine() {
        return args;
    }

    /**
     * Retrieves the identifier for a Timestamping Authority (TSA).
     *
     * @return The TSA identifier. May be null.
     */
    public URI getTimestampingAuthority() {
        return tsa;
    }

    /**
     * Retrieves the certificate for a Timestamping Authority (TSA).
     *
     * @return The TSA certificate. May be null.
     */
    public X509Certificate getTimestampingAuthorityCertificate() {
        return tsaCertificate;
    }

    public String getTSAPolicyID() {
        return tSAPolicyID;
    }

    public String getTSADigestAlg() {
        return tSADigestAlg;
    }

    /**
     * Retrieves the signature.
     *
     * @return The non-null signature bytes.
     */
    public byte[] getSignature() {
        return signature;
    }

    /**
     * Retrieves the name of the signature algorithm.
     *
     * @return The non-null string name of the signature algorithm.
     */
    public String getSignatureAlgorithm() {
        return signatureAlgorithm;
    }

    /**
     * Retrieves the signer's X.509 certificate chain.
     *
     * @return The non-null array of X.509 public-key certificates.
     */
    public X509Certificate[] getSignerCertificateChain() {
        return signerCertificateChain;
    }

    /**
     * Retrieves the content that was signed.
     *
     * @return The content bytes. May be null.
     */
    public byte[] getContent() {
        return content;
    }

    /**
     * Retrieves the original source ZIP file before it was signed.
     *
     * @return The original ZIP file. May be null.
     */
    public ZipFile getSource() {
        return source;
    }
}

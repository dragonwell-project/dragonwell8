/*
 * Copyright (c) 2020, Azul Systems, Inc. All rights reserved.
 * Copyright (c) 2020, Red Hat, Inc.
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

package sun.security.ssl;

import javax.crypto.SecretKey;
import javax.crypto.spec.SecretKeySpec;
import javax.net.ssl.SSLHandshakeException;

import java.io.IOException;
import java.security.AccessControlContext;
import java.security.AccessController;
import java.security.PrivilegedActionException;
import java.security.PrivilegedExceptionAction;
import java.security.SecureRandom;
import java.security.spec.AlgorithmParameterSpec;

final class KrbKeyExchange {
    static final SSLPossessionGenerator poGenerator =
            new KrbPossessionGenerator();
    static final SSLKeyAgreementGenerator kaGenerator =
            new KrbKAGenerator();

    static final
            class KrbPossessionGenerator implements SSLPossessionGenerator {
        /**
         * Kerberos service credentials. Having this possession in
         * the server side will enable the use of a KRB cipher suite in
         * T12ServerHelloProducer::chooseCipherSuite.
         */
        @Override
        public SSLPossession createPossession(HandshakeContext handshakeContext) {
            Object serviceCreds = null;
            try {
                final AccessControlContext acc = handshakeContext.conContext.acc;
                serviceCreds = AccessController.doPrivileged(
                        // Eliminate dependency on KerberosKey
                        new PrivilegedExceptionAction<Object>() {
                            @Override
                            public Object run() throws Exception {
                                // get kerberos key for the default principal
                                return Krb5Helper.getServiceCreds(acc);
                            }});

                // check permission to access and use the secret key of the
                // Kerberized "host" service
                if (serviceCreds != null) {
                    if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                        SSLLogger.fine("Using Kerberos creds");
                    }
                    String serverPrincipal =
                            Krb5Helper.getServerPrincipalName(serviceCreds);
                    if (serverPrincipal != null) {
                        // When service is bound, we check ASAP. Otherwise,
                        // will check after client request is received
                        // in Kerberos ClientKeyExchange
                        SecurityManager sm = System.getSecurityManager();
                        try {
                            if (sm != null) {
                                // Eliminate dependency on ServicePermission
                                sm.checkPermission(Krb5Helper.getServicePermission(
                                        serverPrincipal, "accept"), acc);
                            }
                        } catch (SecurityException se) {
                            // Do not destroy keys. Will affect Subject
                            if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                                SSLLogger.fine("Permission to access Kerberos"
                                        + " secret key denied");
                            }
                            return null;
                        }
                    }
                    return new KrbServiceCreds(serviceCreds);
                }
            } catch (PrivilegedActionException e) {
                // Likely exception here is LoginException
                if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                    SSLLogger.fine("Attempt to obtain Kerberos key failed: "
                            + e.toString());
                }
            }
            return null;
        }
    }

    static final
        class KrbServiceCreds implements SSLPossession {
        final Object serviceCreds;

        KrbServiceCreds(Object serviceCreds) {
            this.serviceCreds = serviceCreds;
        }
    }

    static final
        class KrbPremasterSecret implements SSLPossession, SSLCredentials {
        final byte[] preMaster; // 48 bytes

        KrbPremasterSecret(byte[] premasterSecret) {
            preMaster = premasterSecret;
        }

        static KrbPremasterSecret createPremasterSecret(
                ProtocolVersion protocolVersion, SecureRandom rand) {
            byte[] pm = new byte[48];
            rand.nextBytes(pm);
            pm[0] = protocolVersion.major;
            pm[1] = protocolVersion.minor;
            return new KrbPremasterSecret(pm);
        }

        static KrbPremasterSecret decode(ProtocolVersion protocolVersion,
                ProtocolVersion clientVersion, byte[] preMaster,
                SecureRandom rand) {
            KrbPremasterSecret preMasterSecret = null;
            boolean versionMismatch = true;
            ProtocolVersion preMasterProtocolVersion = null;
            if (preMaster != null && preMaster.length == 48) {
                preMasterProtocolVersion =
                        ProtocolVersion.valueOf(preMaster[0], preMaster[1]);
                if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                     SSLLogger.fine("Kerberos pre-master secret protocol" +
                             " version: " + preMasterProtocolVersion);
                }

                // Check if the pre-master secret protocol version is valid.
                // The specification states that it must be the maximum version
                // supported by the client from its ClientHello message. However,
                // many old implementations send the negotiated version, so accept
                // both for SSL v3.0 and TLS v1.0.
                versionMismatch =
                        (preMasterProtocolVersion.compare(clientVersion) != 0);
                if (versionMismatch &&
                        (clientVersion.compare(ProtocolVersion.TLS10) <= 0)) {
                    versionMismatch =
                            (preMasterProtocolVersion.compare(protocolVersion) != 0);
                }
            }

            if (versionMismatch) {
                /*
                 * Bogus decrypted ClientKeyExchange? If so, conjure a
                 * a random pre-master secret that will fail later during
                 * Finished message processing. This is a countermeasure against
                 * the "interactive RSA PKCS#1 encryption envelop attack" reported
                 * in June 1998. Preserving the execution path will
                 * mitigate timing attacks and force consistent error handling.
                 */
                preMasterSecret = createPremasterSecret(clientVersion, rand);
                if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                    SSLLogger.fine("Kerberos pre-master secret error," +
                            " generating random secret for safe failure.");
                }
            } else {
                preMasterSecret = new KrbPremasterSecret(preMaster);
            }
            return preMasterSecret;
        }
    }

    private static final class KrbKAGenerator implements SSLKeyAgreementGenerator {
        // Prevent instantiation of this class.
        private KrbKAGenerator() {
            // blank
        }

        @Override
        public SSLKeyDerivation createKeyDerivation(
                HandshakeContext context) throws IOException {

            KrbPremasterSecret premaster = null;
            if (context instanceof ClientHandshakeContext) {
                for (SSLPossession possession : context.handshakePossessions) {
                    if (possession instanceof KrbPremasterSecret) {
                        premaster = (KrbPremasterSecret)possession;
                        break;
                    }
                }
            } else {
                for (SSLCredentials credential : context.handshakeCredentials) {
                    if (credential instanceof KrbPremasterSecret) {
                        premaster = (KrbPremasterSecret)credential;
                        break;
                    }
                }
            }

            if (premaster == null) {
                throw context.conContext.fatal(Alert.HANDSHAKE_FAILURE,
                        "No sufficient KRB key agreement parameters negotiated");
            }

            return new KRBKAKeyDerivation(context, premaster.preMaster);

        }

        private static final
            class KRBKAKeyDerivation implements SSLKeyDerivation {
            private final HandshakeContext context;
            private final byte[] secretBytes;

            KRBKAKeyDerivation(HandshakeContext context,
                               byte[] secret) {
                this.context = context;
                this.secretBytes = secret;
            }

            @Override
            public SecretKey deriveKey(String algorithm,
                                       AlgorithmParameterSpec params) throws IOException {
                try {
                    SecretKey preMasterSecret = new SecretKeySpec(secretBytes,
                            "TlsPremasterSecret");

                    SSLMasterKeyDerivation mskd =
                            SSLMasterKeyDerivation.valueOf(
                                    context.negotiatedProtocol);
                    if (mskd == null) {
                        // unlikely
                        throw new SSLHandshakeException(
                                "No expected master key derivation for protocol: " +
                                        context.negotiatedProtocol.name);
                    }
                    SSLKeyDerivation kd = mskd.createKeyDerivation(
                            context, preMasterSecret);
                    return kd.deriveKey("MasterSecret", params);
                } catch (Exception gse) {
                    throw (SSLHandshakeException) new SSLHandshakeException(
                            "Could not generate secret").initCause(gse);
                }
            }
        }
    }
}

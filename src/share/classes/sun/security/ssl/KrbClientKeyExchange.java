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

import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.nio.ByteBuffer;
import java.security.AccessControlContext;
import java.security.AccessController;
import java.security.Principal;
import java.security.PrivilegedAction;
import java.text.MessageFormat;
import java.util.Locale;
import javax.crypto.SecretKey;
import javax.net.ssl.SNIHostName;
import javax.net.ssl.StandardConstants;

import sun.misc.HexDumpEncoder;
import sun.security.ssl.KrbKeyExchange.KrbPremasterSecret;
import sun.security.ssl.SSLHandshake.HandshakeMessage;

/**
 * Pack of the "ClientKeyExchange" handshake message.
 */
final class KrbClientKeyExchange {
    static final SSLConsumer krbHandshakeConsumer =
            new KrbClientKeyExchangeConsumer();
    static final HandshakeProducer krbHandshakeProducer =
            new KrbClientKeyExchangeProducer();

    /**
     * The KRB5 ClientKeyExchange handshake message (CLIENT -> SERVER).
     * It holds the Kerberos ticket and the encrypted pre-master secret
     * encrypted with the session key sealed in the ticket.
     *
     * From RFC 2712:
     *
     *  struct
     *  {
     *    opaque Ticket;
     *    opaque authenticator;            // optional, ignored
     *    opaque EncryptedPreMasterSecret; // encrypted with the session key
     *                                     // sealed in the ticket
     *  } KerberosWrapper;
     *
     */
    private static final
            class KrbClientKeyExchangeMessage extends HandshakeMessage {

        private static final String KRB5_CLASS_NAME =
                "sun.security.ssl.krb5.KrbClientKeyExchangeHelperImpl";

        private static final Class<?> krb5Class = AccessController.doPrivileged(
                new PrivilegedAction<Class<?>>() {
                    @Override
                    public Class<?> run() {
                        try {
                            return Class.forName(KRB5_CLASS_NAME, true, null);
                        } catch (ClassNotFoundException cnf) {
                            return null;
                        }
                    }
                });

        private static KrbClientKeyExchangeHelper newKrb5Instance() {
            if (krb5Class != null) {
                try {
                    return (KrbClientKeyExchangeHelper)krb5Class.
                            getDeclaredConstructor().newInstance();
                } catch (InstantiationException | IllegalAccessException |
                        NoSuchMethodException | InvocationTargetException e) {
                    throw new AssertionError(e);
                }
            }
            return null;
        }

        private final KrbClientKeyExchangeHelper krb5Helper;

        private KrbClientKeyExchangeMessage(HandshakeContext context) {
            super(context);
            if ((krb5Helper = newKrb5Instance()) == null)
                throw new IllegalStateException("Kerberos is unavailable");
        }

        KrbClientKeyExchangeMessage(HandshakeContext context,
                byte[] preMaster, String serverName,
                AccessControlContext acc) throws IOException {
            this(context);
            krb5Helper.init(preMaster, serverName, acc);
        }

        KrbClientKeyExchangeMessage(HandshakeContext context,
                ByteBuffer message, Object serverKeys,
                AccessControlContext acc) throws IOException {
            this(context);
            byte[] encodedTicket = Record.getBytes16(message);
            if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                SSLLogger.fine("encoded Kerberos service ticket",
                        encodedTicket);
            }
            // Read and ignore the authenticator
            Record.getBytes16(message);
            byte[] encryptedPreMasterSecret = Record.getBytes16(message);
            if (encryptedPreMasterSecret != null && SSLLogger.isOn &&
                    SSLLogger.isOn("ssl,handshake")) {
                SSLLogger.fine("encrypted Kerberos pre-master secret",
                        encryptedPreMasterSecret);
            }
            krb5Helper.init(encodedTicket, encryptedPreMasterSecret,
                    serverKeys, acc);
        }

        @Override
        SSLHandshake handshakeType() {
            return SSLHandshake.CLIENT_KEY_EXCHANGE;
        }

        @Override
        int messageLength() {
            return (6 + krb5Helper.getEncodedTicket().length +
                    krb5Helper.getEncryptedPreMasterSecret().length);
        }

        @Override
        void send(HandshakeOutStream hos) throws IOException {
            hos.putBytes16(krb5Helper.getEncodedTicket());
            hos.putBytes16(null); // XXX no authenticator
            hos.putBytes16(krb5Helper.getEncryptedPreMasterSecret());
        }

        byte[] getPlainPreMasterSecret() {
            return krb5Helper.getPlainPreMasterSecret();
        }

        Principal getPeerPrincipal() {
            return krb5Helper.getPeerPrincipal();
        }

        Principal getLocalPrincipal() {
            return krb5Helper.getLocalPrincipal();
        }

        @Override
        public String toString() {
            MessageFormat messageFormat = new MessageFormat(
                    "\"KRB5 ClientKeyExchange\": '{'\n" +
                    "  \"ticket\": '{'\n" +
                    "{0}\n" +
                    "  '}'\n" +
                    "  \"pre-master\": '{'\n" +
                    "    \"plain\": '{'\n" +
                    "{1}\n" +
                    "    '}'\n" +
                    "    \"encrypted\": '{'\n" +
                    "{2}\n" +
                    "    '}'\n" +
                    "  '}'\n" +
                    "'}'",
                    Locale.ENGLISH);

            HexDumpEncoder hexEncoder = new HexDumpEncoder();
            Object[] messageFields = {
                Utilities.indent(
                        hexEncoder.encodeBuffer(
                                krb5Helper.getEncodedTicket()), "  "),
                Utilities.indent(
                        hexEncoder.encodeBuffer(
                                krb5Helper.getPlainPreMasterSecret()), "      "),
                Utilities.indent(
                        hexEncoder.encodeBuffer(
                                krb5Helper.getEncryptedPreMasterSecret()), "      "),
            };
            return messageFormat.format(messageFields);
        }
    }

    /**
     * The KRB5 "ClientKeyExchange" handshake message producer.
     */
    private static final
            class KrbClientKeyExchangeProducer implements HandshakeProducer {
        // Prevent instantiation of this class.
        private KrbClientKeyExchangeProducer() {
            // blank
        }

        @Override
        public byte[] produce(ConnectionContext context,
                HandshakeMessage message) throws IOException {
            // This happens in client side only.
            ClientHandshakeContext chc = (ClientHandshakeContext)context;

            KrbClientKeyExchangeMessage kerberosMsg = null;
            String hostName = null;
            if (chc.negotiatedServerName != null) {
                if (chc.negotiatedServerName.getType() ==
                        StandardConstants.SNI_HOST_NAME) {
                    SNIHostName sniHostName = null;
                    if (chc.negotiatedServerName instanceof SNIHostName) {
                        sniHostName = (SNIHostName) chc.negotiatedServerName;
                    } else {
                        try {
                            sniHostName = new SNIHostName(
                                    chc.negotiatedServerName.getEncoded());
                        } catch (IllegalArgumentException iae) {
                            // unlikely to happen, just in case ...
                        }
                    }
                    if (sniHostName != null)
                        hostName = sniHostName.getAsciiName();
                }
            } else {
                hostName = chc.handshakeSession.getPeerHost();
            }
            try {
                KrbPremasterSecret premasterSecret =
                        KrbPremasterSecret.createPremasterSecret(
                                chc.negotiatedProtocol,
                                chc.sslContext.getSecureRandom());
                kerberosMsg = new KrbClientKeyExchangeMessage(chc,
                        premasterSecret.preMaster, hostName,
                        chc.conContext.acc);
                chc.handshakePossessions.add(premasterSecret);
            } catch (IOException e) {
                if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                    SSLLogger.fine(
                            "Error generating KRB premaster secret." +
                            " Hostname: " + hostName + " - Negotiated" +
                            " server name: " + chc.negotiatedServerName);
                }
                throw chc.conContext.fatal(Alert.ILLEGAL_PARAMETER,
                        "Cannot generate KRB premaster secret", e);
            }
            if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                SSLLogger.fine(
                    "Produced KRB5 ClientKeyExchange handshake message",
                    kerberosMsg);
            }

            // Record the principals involved in the exchange
            chc.handshakeSession.setPeerPrincipal(kerberosMsg.getPeerPrincipal());
            chc.handshakeSession.setLocalPrincipal(kerberosMsg.getLocalPrincipal());

            // Output the handshake message.
            kerberosMsg.write(chc.handshakeOutput);
            chc.handshakeOutput.flush();

            // update the states
            SSLKeyExchange ke = SSLKeyExchange.valueOf(
                    chc.negotiatedCipherSuite.keyExchange,
                    chc.negotiatedProtocol);
            if (ke == null) {
                // unlikely
                throw chc.conContext.fatal(Alert.INTERNAL_ERROR,
                        "Not supported key exchange type");
            } else {
                SSLKeyDerivation masterKD = ke.createKeyDerivation(chc);
                SecretKey masterSecret =
                        masterKD.deriveKey("MasterSecret", null);

                chc.handshakeSession.setMasterSecret(masterSecret);

                SSLTrafficKeyDerivation kd =
                        SSLTrafficKeyDerivation.valueOf(chc.negotiatedProtocol);
                if (kd == null) {
                    // unlikely
                    throw chc.conContext.fatal(Alert.INTERNAL_ERROR,
                            "Not supported key derivation: " +
                                    chc.negotiatedProtocol);
                } else {
                    chc.handshakeKeyDerivation =
                            kd.createKeyDerivation(chc, masterSecret);
                }
            }

            // The handshake message has been delivered.
            return null;
        }
    }

    /**
     * The KRB5 "ClientKeyExchange" handshake message consumer.
     */
    private static final
            class KrbClientKeyExchangeConsumer implements SSLConsumer {
        // Prevent instantiation of this class.
        private KrbClientKeyExchangeConsumer() {
            // blank
        }

        @Override
        public void consume(ConnectionContext context,
                ByteBuffer message) throws IOException {
            // The consuming happens in server side only.
            ServerHandshakeContext shc = (ServerHandshakeContext)context;

            Object serviceCreds = null;
            for (SSLPossession possession : shc.handshakePossessions) {
                if (possession instanceof KrbKeyExchange.KrbServiceCreds) {
                    serviceCreds = ((KrbKeyExchange.KrbServiceCreds) possession)
                            .serviceCreds;
                    break;
                }
            }
            if (serviceCreds == null) {  // unlikely
                throw shc.conContext.fatal(Alert.ILLEGAL_PARAMETER,
                    "No Kerberos service credentials for KRB Client Key Exchange");
            }

            KrbClientKeyExchangeMessage kerberosMsg =
                    new KrbClientKeyExchangeMessage(shc,
                            message, serviceCreds, shc.conContext.acc);
            KrbPremasterSecret premasterSecret = KrbPremasterSecret.decode(
                    shc.negotiatedProtocol,
                    ProtocolVersion.valueOf(shc.clientHelloVersion),
                    kerberosMsg.getPlainPreMasterSecret(),
                    shc.sslContext.getSecureRandom());
            shc.handshakeSession.setPeerPrincipal(kerberosMsg.getPeerPrincipal());
            shc.handshakeSession.setLocalPrincipal(kerberosMsg.getLocalPrincipal());
            shc.handshakeCredentials.add(premasterSecret);
            if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                SSLLogger.fine(
                    "Consuming KRB5 ClientKeyExchange handshake message",
                    kerberosMsg);
            }

            // update the states
            SSLKeyExchange ke = SSLKeyExchange.valueOf(
                    shc.negotiatedCipherSuite.keyExchange,
                    shc.negotiatedProtocol);
            if (ke == null) {   // unlikely
                throw shc.conContext.fatal(Alert.INTERNAL_ERROR,
                        "Not supported key exchange type");
            } else {
                SSLKeyDerivation masterKD = ke.createKeyDerivation(shc);
                SecretKey masterSecret =
                        masterKD.deriveKey("MasterSecret", null);

                // update the states
                shc.handshakeSession.setMasterSecret(masterSecret);
                SSLTrafficKeyDerivation kd =
                        SSLTrafficKeyDerivation.valueOf(shc.negotiatedProtocol);
                if (kd == null) {       // unlikely
                    throw shc.conContext.fatal(Alert.INTERNAL_ERROR,
                            "Not supported key derivation: " +
                                    shc.negotiatedProtocol);
                } else {
                    shc.handshakeKeyDerivation =
                            kd.createKeyDerivation(shc, masterSecret);
                }
            }
        }
    }
}

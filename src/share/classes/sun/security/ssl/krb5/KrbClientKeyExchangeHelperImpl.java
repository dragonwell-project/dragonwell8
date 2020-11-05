/*
 * Copyright (c) 2003, 2020, Oracle and/or its affiliates. All rights reserved.
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

package sun.security.ssl.krb5;

import java.io.IOException;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.security.AccessController;
import java.security.AccessControlContext;
import java.security.PrivilegedExceptionAction;
import java.security.PrivilegedActionException;
import java.util.Arrays;
import java.security.PrivilegedAction;

import javax.security.auth.kerberos.KerberosTicket;
import javax.net.ssl.SSLKeyException;
import javax.security.auth.kerberos.KerberosKey;
import javax.security.auth.kerberos.KerberosPrincipal;
import javax.security.auth.kerberos.ServicePermission;
import sun.security.jgss.GSSCaller;

import sun.security.krb5.EncryptionKey;
import sun.security.krb5.EncryptedData;
import sun.security.krb5.PrincipalName;
import sun.security.krb5.internal.Ticket;
import sun.security.krb5.internal.EncTicketPart;
import sun.security.krb5.internal.crypto.KeyUsage;

import sun.security.jgss.krb5.Krb5Util;
import sun.security.jgss.krb5.ServiceCreds;
import sun.security.krb5.KrbException;

import sun.security.ssl.Krb5Helper;
import sun.security.ssl.SSLLogger;

public final class KrbClientKeyExchangeHelperImpl
        implements sun.security.ssl.KrbClientKeyExchangeHelper {

    private byte[] preMaster;
    private byte[] preMasterEnc;
    private byte[] encodedTicket;
    private KerberosPrincipal peerPrincipal;
    private KerberosPrincipal localPrincipal;

    /**
     * Initialises an instance of KrbClientKeyExchangeHelperImpl.
     * Used by the TLS client to create a Kerberos Client Key Exchange
     * message.
     *
     * @param preMaster plain pre-master secret
     * @param serverName name of the TLS server to perform the handshake;
     *                   used by the TLS client to get a Kerberos service
     *                   ticket which contains the session key to encrypt
     *                   the pre-master secret
     * @param acc the TLS client security context for the handshake
     */
    @Override
    public void init(byte[] preMaster, String serverName,
            AccessControlContext acc) throws IOException {
        this.preMaster = preMaster;

        // Get service ticket
        KerberosTicket ticket = getServiceTicket(serverName, acc);
        encodedTicket = ticket.getEncoded();

        // Record the Kerberos principals
        peerPrincipal = ticket.getServer();
        localPrincipal = ticket.getClient();

        // Encrypt the pre-master secret with the Kerberos session key
        EncryptionKey sessionKey = new EncryptionKey(
                ticket.getSessionKeyType(),
                ticket.getSessionKey().getEncoded());
        encryptPremasterSecret(sessionKey);
    }

    /**
     * Initialises an instance of KrbClientKeyExchangeHelperImpl.
     * Used by the TLS server to process the content of a Kerberos Client Key
     * Exchange message (received from a TLS client).
     *
     * @param encodedTicket the encoded Kerberos ticket (TGS) received from the
     *                      TLS client. This ticket seals the Kerberos session
     *                      key.
     * @param preMasterEnc the pre-master secret encrypted with the Kerberos
     *                     session key
     * @param serviceCreds the TLS server Kerberos credentials used to process
     *                     the received Kerberos ticket.
     * @param acc the TLS server security context for the handshake
     */
    @Override
    public void init(byte[] encodedTicket, byte[] preMasterEnc,
            Object serviceCreds, AccessControlContext acc) throws IOException {
        this.encodedTicket = encodedTicket;
        this.preMasterEnc = preMasterEnc;
        EncryptionKey sessionKey = null;

        try {
            Ticket t = new Ticket(encodedTicket);

            EncryptedData encPart = t.encPart;
            PrincipalName ticketSname = t.sname;

            final ServiceCreds creds = (ServiceCreds) serviceCreds;
            final KerberosPrincipal princ =
                    new KerberosPrincipal(ticketSname.toString());

            // For bound service, permission already checked at setup
            if (creds.getName() == null) {
                SecurityManager sm = System.getSecurityManager();
                try {
                    if (sm != null) {
                        // Eliminate dependency on ServicePermission
                        sm.checkPermission(Krb5Helper.getServicePermission(
                                ticketSname.toString(), "accept"), acc);
                    }
                } catch (SecurityException se) {
                    // Do not destroy keys. Will affect Subject
                    if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                        SSLLogger.fine("Permission to access Kerberos"
                                + " secret key denied");
                    }
                    throw new IOException("Kerberos service not allowed");
                }
            }
            KerberosKey[] serverKeys = AccessController.doPrivileged(
                    new PrivilegedAction<KerberosKey[]>() {
                        @Override
                        public KerberosKey[] run() {
                            return creds.getKKeys(princ);
                        }
                    });
            if (serverKeys.length == 0) {
                throw new IOException("Found no key for " + princ +
                        (creds.getName() == null ? "" :
                        (", this keytab is for " + creds.getName() + " only")));
            }

            // See if we have the right key to decrypt the ticket to get
            // the session key.
            int encPartKeyType = encPart.getEType();
            Integer encPartKeyVersion = encPart.getKeyVersionNumber();
            KerberosKey dkey = null;
            try {
                dkey = findKey(encPartKeyType, encPartKeyVersion, serverKeys);
            } catch (KrbException ke) { // a kvno mismatch
                throw new IOException(
                        "Cannot find key matching version number", ke);
            }
            if (dkey == null) {
                // %%% Should print string repr of etype
                throw new IOException("Cannot find key of appropriate type" +
                        " to decrypt ticket - need etype " + encPartKeyType);
            }

            EncryptionKey secretKey = new EncryptionKey(
                encPartKeyType,
                dkey.getEncoded());

            // Decrypt encPart using server's secret key
            byte[] bytes = encPart.decrypt(secretKey, KeyUsage.KU_TICKET);

            // Reset data stream after decryption, remove redundant bytes
            byte[] temp = encPart.reset(bytes);
            EncTicketPart encTicketPart = new EncTicketPart(temp);

            // Record the Kerberos Principals
            peerPrincipal =
                    new KerberosPrincipal(encTicketPart.cname.getName());
            localPrincipal = new KerberosPrincipal(ticketSname.getName());

            sessionKey = encTicketPart.key;

            if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                SSLLogger.fine("server principal: " + ticketSname);
                SSLLogger.fine("cname: " + encTicketPart.cname.toString());
            }
        } catch (Exception e) {
            sessionKey = null;
            if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                SSLLogger.fine("Error getting the Kerberos session key" +
                        " to decrypt the pre-master secret");
            }
        }
        if (sessionKey != null)
            decryptPremasterSecret(sessionKey);
    }

    @Override
    public byte[] getEncodedTicket() {
        return encodedTicket;
    }

    @Override
    public byte[] getEncryptedPreMasterSecret() {
        return preMasterEnc;
    }

    @Override
    public byte[] getPlainPreMasterSecret() {
        return preMaster;
    }

    @Override
    public KerberosPrincipal getPeerPrincipal() {
        return peerPrincipal;
    }

    @Override
    public KerberosPrincipal getLocalPrincipal() {
        return localPrincipal;
    }

    private void encryptPremasterSecret(EncryptionKey sessionKey)
            throws IOException {
        if (sessionKey.getEType() ==
                EncryptedData.ETYPE_DES3_CBC_HMAC_SHA1_KD) {
            throw new IOException(
                    "session keys with des3-cbc-hmac-sha1-kd encryption type " +
                    "are not supported for TLS Kerberos cipher suites");
        }
        try {
            EncryptedData eData = new EncryptedData(sessionKey, preMaster,
                    KeyUsage.KU_UNKNOWN);
            preMasterEnc = eData.getBytes(); // not ASN.1 encoded.
        } catch (KrbException e) {
            throw (IOException) new SSLKeyException("Kerberos pre-master" +
                    " secret error").initCause(e);
        }
    }

    private void decryptPremasterSecret(EncryptionKey sessionKey)
            throws IOException {
        if (sessionKey.getEType() ==
                EncryptedData.ETYPE_DES3_CBC_HMAC_SHA1_KD) {
            throw new IOException(
                    "session keys with des3-cbc-hmac-sha1-kd encryption type " +
                    "are not supported for TLS Kerberos cipher suites");
        }
        try {
            EncryptedData data = new EncryptedData(sessionKey.getEType(),
                        null /* optional kvno */, preMasterEnc);
            byte[] temp = data.decrypt(sessionKey, KeyUsage.KU_UNKNOWN);
            if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                 if (preMasterEnc != null) {
                     SSLLogger.fine("decrypted premaster secret", temp);
                 }
            }

            // Remove padding bytes after decryption. Only DES and DES3 have
            // paddings and we don't support DES3 in TLS (see above)
            if (temp.length == 52 &&
                    data.getEType() == EncryptedData.ETYPE_DES_CBC_CRC) {
                // For des-cbc-crc, 4 paddings. Value can be 0x04 or 0x00.
                if (paddingByteIs(temp, 52, (byte)4) ||
                        paddingByteIs(temp, 52, (byte)0)) {
                    temp = Arrays.copyOf(temp, 48);
                }
            } else if (temp.length == 56 &&
                    data.getEType() == EncryptedData.ETYPE_DES_CBC_MD5) {
                // For des-cbc-md5, 8 paddings with 0x08, or no padding
                if (paddingByteIs(temp, 56, (byte)8)) {
                    temp = Arrays.copyOf(temp, 48);
                }
            }

            preMaster = temp;
        } catch (Exception e) {
            // Decrypting the pre-master secret was not possible.
            if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                SSLLogger.fine("Error decrypting the pre-master secret");
            }
        }
    }

    /**
     * Checks if all paddings of data are b
     * @param data the block with padding
     * @param len length of data, >= 48
     * @param b expected padding byte
     */
    private static boolean paddingByteIs(byte[] data, int len, byte b) {
        for (int i=48; i<len; i++) {
            if (data[i] != b) return false;
        }
        return true;
    }

    // Similar to sun.security.jgss.krb5.Krb5InitCredenetial/Krb5Context
    private static KerberosTicket getServiceTicket(String serverName,
        final AccessControlContext acc) throws IOException {

        if ("localhost".equals(serverName) ||
                "localhost.localdomain".equals(serverName)) {

            if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                SSLLogger.fine("Get the local hostname");
            }
            String localHost = java.security.AccessController.doPrivileged(
                new java.security.PrivilegedAction<String>() {
                public String run() {
                    try {
                        return InetAddress.getLocalHost().getHostName();
                    } catch (UnknownHostException e) {
                        if (SSLLogger.isOn && SSLLogger.isOn("ssl,handshake")) {
                            SSLLogger.fine("Warning,"
                                + " cannot get the local hostname: "
                                + e.getMessage());
                        }
                        return null;
                    }
                }
            });
            if (localHost != null) {
                serverName = localHost;
            }
        }

        // Resolve serverName (possibly in IP addr form) to Kerberos principal
        // name for service with hostname
        String serviceName = "host/" + serverName;
        PrincipalName principal;
        try {
            principal = new PrincipalName(serviceName,
                                PrincipalName.KRB_NT_SRV_HST);
        } catch (SecurityException se) {
            throw se;
        } catch (Exception e) {
            IOException ioe = new IOException("Invalid service principal" +
                                " name: " + serviceName);
            ioe.initCause(e);
            throw ioe;
        }
        String realm = principal.getRealmAsString();

        final String serverPrincipal = principal.toString();
        final String tgsPrincipal = "krbtgt/" + realm + "@" + realm;
        final String clientPrincipal = null;  // use default


        // check permission to obtain a service ticket to initiate a
        // context with the "host" service
        SecurityManager sm = System.getSecurityManager();
        if (sm != null) {
           sm.checkPermission(new ServicePermission(serverPrincipal,
                                "initiate"), acc);
        }

        try {
            KerberosTicket ticket = AccessController.doPrivileged(
                new PrivilegedExceptionAction<KerberosTicket>() {
                public KerberosTicket run() throws Exception {
                    return Krb5Util.getTicketFromSubjectAndTgs(
                        GSSCaller.CALLER_SSL_CLIENT,
                        clientPrincipal, serverPrincipal,
                        tgsPrincipal, acc);
                        }});

            if (ticket == null) {
                throw new IOException("Failed to find any kerberos service" +
                        " ticket for " + serverPrincipal);
            }
            return ticket;
        } catch (PrivilegedActionException e) {
            IOException ioe = new IOException(
                "Attempt to obtain kerberos service ticket for " +
                        serverPrincipal + " failed!");
            ioe.initCause(e);
            throw ioe;
        }
    }

    /**
     * Determines if a kvno matches another kvno. Used in the method
     * findKey(etype, version, keys). Always returns true if either input
     * is null or zero, in case any side does not have kvno info available.
     *
     * Note: zero is included because N/A is not a legal value for kvno
     * in javax.security.auth.kerberos.KerberosKey. Therefore, the info
     * that the kvno is N/A might be lost when converting between
     * EncryptionKey and KerberosKey.
     */
    private static boolean versionMatches(Integer v1, int v2) {
        if (v1 == null || v1 == 0 || v2 == 0) {
            return true;
        }
        return v1.equals(v2);
    }

    private static KerberosKey findKey(int etype, Integer version,
            KerberosKey[] keys) throws KrbException {
        int ktype;
        boolean etypeFound = false;

        // When no matched kvno is found, returns tke key of the same
        // etype with the highest kvno
        int kvno_found = 0;
        KerberosKey key_found = null;

        for (int i = 0; i < keys.length; i++) {
            ktype = keys[i].getKeyType();
            if (etype == ktype) {
                int kv = keys[i].getVersionNumber();
                etypeFound = true;
                if (versionMatches(version, kv)) {
                    return keys[i];
                } else if (kv > kvno_found) {
                    key_found = keys[i];
                    kvno_found = kv;
                }
            }
        }
        // Key not found.
        // %%% kludge to allow DES keys to be used for diff etypes
        if ((etype == EncryptedData.ETYPE_DES_CBC_CRC ||
            etype == EncryptedData.ETYPE_DES_CBC_MD5)) {
            for (int i = 0; i < keys.length; i++) {
                ktype = keys[i].getKeyType();
                if (ktype == EncryptedData.ETYPE_DES_CBC_CRC ||
                        ktype == EncryptedData.ETYPE_DES_CBC_MD5) {
                    int kv = keys[i].getVersionNumber();
                    etypeFound = true;
                    if (versionMatches(version, kv)) {
                        return new KerberosKey(keys[i].getPrincipal(),
                            keys[i].getEncoded(),
                            etype,
                            kv);
                    } else if (kv > kvno_found) {
                        key_found = new KerberosKey(keys[i].getPrincipal(),
                                keys[i].getEncoded(),
                                etype,
                                kv);
                        kvno_found = kv;
                    }
                }
            }
        }
        if (etypeFound) {
            return key_found;
        }
        return null;
    }
}

/*
 * Copyright (c) 2005, 2015, Oracle and/or its affiliates. All rights reserved.
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

package com.sun.crypto.provider;

import java.io.*;
import java.nio.ByteBuffer;
import java.nio.CharBuffer;
import java.nio.charset.Charset;
import java.util.Arrays;
import java.security.MessageDigest;
import java.util.Locale;
import java.security.KeyRep;
import java.security.GeneralSecurityException;
import java.security.NoSuchAlgorithmException;
import java.security.spec.InvalidKeySpecException;
import javax.crypto.Mac;
import javax.crypto.SecretKey;
import javax.crypto.spec.PBEKeySpec;

/**
 * This class represents a PBE key derived using PBKDF2 defined
 * in PKCS#5 v2.0. meaning that
 * 1) the password must consist of characters which will be converted
 *    to bytes using UTF-8 character encoding.
 * 2) salt, iteration count, and to be derived key length are supplied
 *
 * @author Valerie Peng
 *
 */
final class PBKDF2KeyImpl implements javax.crypto.interfaces.PBEKey {

    private static final long serialVersionUID = -2234868909660948157L;

    private char[] passwd;
    private final byte[] salt;
    private final int iterCount;
    private byte[] key;

    private final Mac prf;

    private static byte[] getPasswordBytes(char[] passwd) {
        Charset utf8 = Charset.forName("UTF-8");
        CharBuffer cb = CharBuffer.wrap(passwd);
        ByteBuffer bb = utf8.encode(cb);

        int len = bb.limit();
        byte[] passwdBytes = new byte[len];
        bb.get(passwdBytes, 0, len);

        return passwdBytes;
    }

    /**
     * Creates a PBE key from a given PBE key specification.
     *
     * @param key the given PBE key specification
     */
    PBKDF2KeyImpl(PBEKeySpec keySpec, String prfAlgo)
        throws InvalidKeySpecException {
        char[] passwd = keySpec.getPassword();
        if (passwd == null) {
            // Should allow an empty password.
            this.passwd = new char[0];
        } else {
            this.passwd = passwd.clone();
        }
        // Convert the password from char[] to byte[]
        byte[] passwdBytes = getPasswordBytes(this.passwd);
        // remove local copy
        if (passwd != null) Arrays.fill(passwd, '\0');

        this.salt = keySpec.getSalt();
        if (salt == null) {
            throw new InvalidKeySpecException("Salt not found");
        }
        this.iterCount = keySpec.getIterationCount();
        if (iterCount == 0) {
            throw new InvalidKeySpecException("Iteration count not found");
        } else if (iterCount < 0) {
            throw new InvalidKeySpecException("Iteration count is negative");
        }
        int keyLength = keySpec.getKeyLength();
        if (keyLength == 0) {
            throw new InvalidKeySpecException("Key length not found");
        } else if (keyLength < 0) {
            throw new InvalidKeySpecException("Key length is negative");
        }
        try {
            this.prf = Mac.getInstance(prfAlgo, SunJCE.getInstance());
            this.key = deriveKey(prf, passwdBytes, salt, iterCount, keyLength);
        } catch (NoSuchAlgorithmException nsae) {
            // not gonna happen; re-throw just in case
            InvalidKeySpecException ike = new InvalidKeySpecException();
            ike.initCause(nsae);
            throw ike;
        } finally {
            Arrays.fill(passwdBytes, (byte)0x00);
        }
    }

    private static byte[] deriveKey(final Mac prf, final byte[] password,
            byte[] salt, int iterCount, int keyLengthInBit) {
        int keyLength = keyLengthInBit/8;
        byte[] key = new byte[keyLength];
        try {
            int hlen = prf.getMacLength();
            int intL = (keyLength + hlen - 1)/hlen; // ceiling
            int intR = keyLength - (intL - 1)*hlen; // residue
            byte[] ui = new byte[hlen];
            byte[] ti = new byte[hlen];
            String algName = prf.getAlgorithm();
            // SecretKeySpec cannot be used, since password can be empty here.
            SecretKey macKey = new SecretKey() {
                private static final long serialVersionUID = 7874493593505141603L;
                @Override
                public String getAlgorithm() {
                    return algName;
                }
                @Override
                public String getFormat() {
                    return "RAW";
                }
                @Override
                public byte[] getEncoded() {
                    return password;
                }
                @Override
                public int hashCode() {
                    return Arrays.hashCode(password) * 41 +
                      algName.toLowerCase(Locale.ENGLISH).hashCode();
                }
                @Override
                public boolean equals(Object obj) {
                    if (this == obj) return true;
                    if (this.getClass() != obj.getClass()) return false;
                    SecretKey sk = (SecretKey)obj;
                    return algName.equalsIgnoreCase(
                        sk.getAlgorithm()) &&
                        MessageDigest.isEqual(password, sk.getEncoded());
                }
                // This derived key can't be deserialized.
                private void readObject(ObjectInputStream stream)
                        throws IOException, ClassNotFoundException {
                    throw new InvalidObjectException(
                            "PBKDF2KeyImpl SecretKeys are not " +
                            "directly deserializable");
                }
            };

            prf.init(macKey);

            byte[] ibytes = new byte[4];
            for (int i = 1; i <= intL; i++) {
                prf.update(salt);
                ibytes[3] = (byte) i;
                ibytes[2] = (byte) ((i >> 8) & 0xff);
                ibytes[1] = (byte) ((i >> 16) & 0xff);
                ibytes[0] = (byte) ((i >> 24) & 0xff);
                prf.update(ibytes);
                prf.doFinal(ui, 0);
                System.arraycopy(ui, 0, ti, 0, ui.length);

                for (int j = 2; j <= iterCount; j++) {
                    prf.update(ui);
                    prf.doFinal(ui, 0);
                    // XOR the intermediate Ui's together.
                    for (int k = 0; k < ui.length; k++) {
                        ti[k] ^= ui[k];
                    }
                }
                if (i == intL) {
                    System.arraycopy(ti, 0, key, (i-1)*hlen, intR);
                } else {
                    System.arraycopy(ti, 0, key, (i-1)*hlen, hlen);
                }
            }
        } catch (GeneralSecurityException gse) {
            throw new RuntimeException("Error deriving PBKDF2 keys");
        }
        return key;
    }

    public synchronized byte[] getEncoded() {
        return key.clone();
    }

    public String getAlgorithm() {
        return "PBKDF2With" + prf.getAlgorithm();
    }

    public int getIterationCount() {
        return iterCount;
    }

    public synchronized char[] getPassword() {
        return passwd.clone();
    }

    public byte[] getSalt() {
        return salt.clone();
    }

    public String getFormat() {
        return "RAW";
    }

    /**
     * Calculates a hash code value for the object.
     * Objects that are equal will also have the same hashcode.
     */
    public int hashCode() {
        int retval = 0;
        for (int i = 1; i < this.key.length; i++) {
            retval += this.key[i] * i;
        }
        return(retval ^= getAlgorithm().toLowerCase(Locale.ENGLISH).hashCode());
    }

    public boolean equals(Object obj) {
        if (obj == this)
            return true;

        if (!(obj instanceof SecretKey))
            return false;

        SecretKey that = (SecretKey) obj;

        if (!(that.getAlgorithm().equalsIgnoreCase(getAlgorithm())))
            return false;
        if (!(that.getFormat().equalsIgnoreCase("RAW")))
            return false;
        byte[] thatEncoded = that.getEncoded();
        boolean ret = MessageDigest.isEqual(key, thatEncoded);
        Arrays.fill(thatEncoded, (byte)0x00);
        return ret;
    }

    /**
     * Replace the PBE key to be serialized.
     *
     * @return the standard KeyRep object to be serialized
     *
     * @throws ObjectStreamException if a new object representing
     * this PBE key could not be created
     */
    private Object writeReplace() throws ObjectStreamException {
            return new KeyRep(KeyRep.Type.SECRET, getAlgorithm(),
                              getFormat(), getEncoded());
    }

    /**
     * Ensures that the password bytes of this key are
     * erased when there are no more references to it.
     */
    protected void finalize() throws Throwable {
        try {
            synchronized (this) {
                if (this.passwd != null) {
                    java.util.Arrays.fill(this.passwd, '\0');
                    this.passwd = null;
                }
                if (this.key != null) {
                    java.util.Arrays.fill(this.key, (byte)0x00);
                    this.key = null;
                }
            }
        } finally {
            super.finalize();
        }
    }

    /**
     * Restores the state of this object from the stream.
     * <p>
     * Deserialization of this class is not supported.
     *
     * @param  stream the {@code ObjectInputStream} from which data is read
     * @throws IOException if an I/O error occurs
     * @throws ClassNotFoundException if a serialized class cannot be loaded
     */
    private void readObject(ObjectInputStream stream)
            throws IOException, ClassNotFoundException {
        throw new InvalidObjectException(
                "PBKDF2KeyImpl keys are not directly deserializable");
    }
}

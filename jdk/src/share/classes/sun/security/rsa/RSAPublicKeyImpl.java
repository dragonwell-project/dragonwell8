/*
 * Copyright (c) 2003, 2023, Oracle and/or its affiliates. All rights reserved.
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

package sun.security.rsa;

import java.io.IOException;
import java.io.InvalidObjectException;
import java.io.ObjectInputStream;
import java.math.BigInteger;

import java.security.*;
import java.security.spec.*;
import java.security.interfaces.*;

import sun.security.util.*;
import sun.security.x509.X509Key;
import sun.security.x509.AlgorithmId;

import static sun.security.rsa.RSAUtil.KeyType;

/**
 * RSA public key implementation for "RSA", "RSASSA-PSS" algorithms.
 * <p>
 * Note: RSA keys must be at least 512 bits long
 *
 * @see RSAPrivateCrtKeyImpl
 * @see RSAPrivateKeyImpl
 * @see RSAKeyFactory
 *
 * @since   1.5
 * @author  Andreas Sterbenz
 */
public final class RSAPublicKeyImpl extends X509Key implements RSAPublicKey {

    private static final long serialVersionUID = 2644735423591199609L;
    private static final BigInteger THREE = BigInteger.valueOf(3);

    private BigInteger n;       // modulus
    private BigInteger e;       // public exponent

    // optional parameters associated with this RSA key
    // specified in the encoding of its AlgorithmId
    // must be null for "RSA" keys.
    private AlgorithmParameterSpec keyParams;

    /**
     * Generate a new RSAPublicKey from the specified encoding.
     * Used by SunPKCS11 provider.
     */
    public static RSAPublicKey newKey(byte[] encoded)
            throws InvalidKeyException {
        return new RSAPublicKeyImpl(encoded);
    }

    /**
     * Generate a new RSAPublicKey from the specified type and components.
     * Used by SunPKCS11 provider.
     */
    public static RSAPublicKey newKey(KeyType type,
            AlgorithmParameterSpec params, BigInteger n, BigInteger e)
            throws InvalidKeyException {
        AlgorithmId rsaId = RSAUtil.createAlgorithmId(type, params);
        return new RSAPublicKeyImpl(rsaId, n, e);
    }

    /**
     * Construct a RSA key from AlgorithmId and its components. Used by
     * RSAKeyFactory and RSAKeyPairGenerator.
     */
    RSAPublicKeyImpl(AlgorithmId rsaId, BigInteger n, BigInteger e)
            throws InvalidKeyException {
        RSAKeyFactory.checkRSAProviderKeyLengths(n.bitLength(), e);
        checkExponentRange(n, e);

        this.n = n;
        this.e = e;
        this.keyParams = RSAUtil.getParamSpec(rsaId);

        // generate the encoding
        algid = rsaId;
        try {
            DerOutputStream out = new DerOutputStream();
            out.putInteger(n);
            out.putInteger(e);
            byte[] keyArray =
                new DerValue(DerValue.tag_Sequence,
                             out.toByteArray()).toByteArray();
            setKey(new BitArray(keyArray.length*8, keyArray));
        } catch (IOException exc) {
            // should never occur
            throw new InvalidKeyException(exc);
        }
    }

    /**
     * Construct a key from its encoding. Used by RSAKeyFactory.
     */
    RSAPublicKeyImpl(byte[] encoded) throws InvalidKeyException {
        if (encoded == null || encoded.length == 0) {
            throw new InvalidKeyException("Missing key encoding");
        }
        decode(encoded); // this sets n and e value
        RSAKeyFactory.checkRSAProviderKeyLengths(n.bitLength(), e);
        checkExponentRange(n, e);

        try {
            // this will check the validity of params
            this.keyParams = RSAUtil.getParamSpec(algid);
        } catch (ProviderException e) {
            throw new InvalidKeyException(e);
        }
    }

    // pkg private utility method for checking RSA modulus and public exponent
    static void checkExponentRange(BigInteger mod, BigInteger exp)
            throws InvalidKeyException {
        // the exponent should be smaller than the modulus
        if (exp.compareTo(mod) >= 0) {
            throw new InvalidKeyException("exponent is larger than modulus");
        }

        // the exponent should be at least 3
        if (exp.compareTo(THREE) < 0) {
            throw new InvalidKeyException("exponent is smaller than 3");
        }
    }

    // see JCA doc
    @Override
    public String getAlgorithm() {
        return algid.getName();
    }

    // see JCA doc
    @Override
    public BigInteger getModulus() {
        return n;
    }

    // see JCA doc
    @Override
    public BigInteger getPublicExponent() {
        return e;
    }

    // see JCA doc
    @Override
    public AlgorithmParameterSpec getParams() {
        return keyParams;
    }

    /**
     * Parse the key. Called by X509Key.
     */
    protected void parseKeyBits() throws InvalidKeyException {
        try {
            DerInputStream in = new DerInputStream(getKey().toByteArray());
            DerValue derValue = in.getDerValue();
            if (derValue.tag != DerValue.tag_Sequence) {
                throw new IOException("Not a SEQUENCE");
            }
            DerInputStream data = derValue.data;
            n = data.getPositiveBigInteger();
            e = data.getPositiveBigInteger();
            if (derValue.data.available() != 0) {
                throw new IOException("Extra data available");
            }
        } catch (IOException e) {
            throw new InvalidKeyException("Invalid RSA public key", e);
        }
    }

    // return a string representation of this key for debugging
    @Override
    public String toString() {
        return "Sun " + getAlgorithm() + " public key, " + n.bitLength()
               + " bits" + "\n  params: " + keyParams + "\n  modulus: " + n
               + "\n  public exponent: " + e;
    }

    private Object writeReplace() throws java.io.ObjectStreamException {
        return new KeyRep(KeyRep.Type.PUBLIC,
                        getAlgorithm(),
                        getFormat(),
                        getEncoded());
    }

    /**
     * Restores the state of this object from the stream.
     * <p>
     * Deserialization of this object is not supported.
     *
     * @param  stream the {@code ObjectInputStream} from which data is read
     * @throws IOException if an I/O error occurs
     * @throws ClassNotFoundException if a serialized class cannot be loaded
     */
    private void readObject(ObjectInputStream stream)
            throws IOException, ClassNotFoundException {
        throw new InvalidObjectException(
                "RSAPublicKeyImpl keys are not directly deserializable");
    }
}

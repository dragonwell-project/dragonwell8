/*
 * Copyright (c) 2015, Oracle and/or its affiliates. All rights reserved.
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

package sun.security.pkcs11;

import java.io.IOException;
import java.math.BigInteger;
import java.security.*;
import java.security.interfaces.*;
import java.security.spec.*;

import sun.security.ec.ECPublicKeyImpl;
import sun.security.ec.ECPrivateKeyImpl;
import sun.security.x509.X509Key;

final class P11ECUtil {

    static ECPublicKey decodeX509ECPublicKey(byte[] encoded)
            throws InvalidKeySpecException {
        X509EncodedKeySpec keySpec = new X509EncodedKeySpec(encoded);

        return (ECPublicKey)ECGeneratePublic(keySpec);
    }

    static byte[] x509EncodeECPublicKey(ECPoint w,
            ECParameterSpec params) throws InvalidKeySpecException {
        ECPublicKeySpec keySpec = new ECPublicKeySpec(w, params);
        X509Key key = (X509Key)ECGeneratePublic(keySpec);

        return key.getEncoded();
    }

    static ECPrivateKey decodePKCS8ECPrivateKey(byte[] encoded)
            throws InvalidKeySpecException {
        PKCS8EncodedKeySpec keySpec = new PKCS8EncodedKeySpec(encoded);

        return (ECPrivateKey)ECGeneratePrivate(keySpec);
    }

    static ECPrivateKey generateECPrivateKey(BigInteger s,
            ECParameterSpec params) throws InvalidKeySpecException {
        ECPrivateKeySpec keySpec = new ECPrivateKeySpec(s, params);

        return (ECPrivateKey)ECGeneratePrivate(keySpec);
    }

    private static PublicKey ECGeneratePublic(KeySpec keySpec)
            throws InvalidKeySpecException {
        try {
            if (keySpec instanceof X509EncodedKeySpec) {
               X509EncodedKeySpec x509Spec = (X509EncodedKeySpec)keySpec;
                return new ECPublicKeyImpl(x509Spec.getEncoded());
            } else if (keySpec instanceof ECPublicKeySpec) {
                ECPublicKeySpec ecSpec = (ECPublicKeySpec)keySpec;
                return new ECPublicKeyImpl(
                    ecSpec.getW(),
                    ecSpec.getParams()
                );
            } else {
                throw new InvalidKeySpecException("Only ECPublicKeySpec "
                    + "and X509EncodedKeySpec supported for EC public keys");
            }
        } catch (InvalidKeySpecException e) {
            throw e;
        } catch (GeneralSecurityException e) {
            throw new InvalidKeySpecException(e);
        }
    }

    private static PrivateKey ECGeneratePrivate(KeySpec keySpec)
            throws InvalidKeySpecException {
        try {
            if (keySpec instanceof PKCS8EncodedKeySpec) {
                PKCS8EncodedKeySpec pkcsSpec = (PKCS8EncodedKeySpec)keySpec;
                return new ECPrivateKeyImpl(pkcsSpec.getEncoded());
            } else if (keySpec instanceof ECPrivateKeySpec) {
                ECPrivateKeySpec ecSpec = (ECPrivateKeySpec)keySpec;
                return new ECPrivateKeyImpl(ecSpec.getS(), ecSpec.getParams());
            } else {
                throw new InvalidKeySpecException("Only ECPrivateKeySpec "
                    + "and PKCS8EncodedKeySpec supported for EC private keys");
            }
        } catch (InvalidKeySpecException e) {
            throw e;
        } catch (GeneralSecurityException e) {
            throw new InvalidKeySpecException(e);
        }
    }

    private P11ECUtil() {}

}

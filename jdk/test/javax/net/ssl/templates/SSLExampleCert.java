/*
 * Copyright (C) 2022 THL A29 Limited, a Tencent company. All rights reserved.
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

import javax.net.ssl.KeyManagerFactory;
import javax.net.ssl.SSLContext;
import javax.net.ssl.TrustManagerFactory;
import java.io.*;
import java.net.InetAddress;
import java.security.KeyFactory;
import java.security.KeyStore;
import java.security.PrivateKey;
import java.security.cert.Certificate;
import java.security.cert.CertificateFactory;
import java.security.spec.PKCS8EncodedKeySpec;
import java.util.Base64;

// A template to use "www.example.com" as the server name.  The caller should
// set a virtual hosts file with System Property, "jdk.net.hosts.file". This
// class will map the loopback address to "www.example.com", and write to
// the specified hosts file.
public enum SSLExampleCert {
    // Version: 3 (0x2)
    // Serial Number: 15223159159760931473 (0xd34386999cc8ca91)
    // Signature Algorithm: sha256WithRSAEncryption
    // Issuer: C=US, ST=California, O=Example, OU=Test
    // Validity
    //     Not Before: Jan 26 04:50:29 2022 GMT
    //     Not After : Feb 25 04:50:29 2022 GMT
    // Subject: C=US, ST=California, O=Example, OU=Test
    // Public Key Algorithm: rsaEncryption
    CA_RSA("RSA",

            "-----BEGIN CERTIFICATE-----\n" +
            "MIIDXDCCAkSgAwIBAgIJANNDhpmcyMqRMA0GCSqGSIb3DQEBCwUAMEMxCzAJBgNV\n" +
            "BAYTAlVTMRMwEQYDVQQIDApDYWxpZm9ybmlhMRAwDgYDVQQKDAdFeGFtcGxlMQ0w\n" +
            "CwYDVQQLDARUZXN0MB4XDTIyMDEyNjA0NTAyOVoXDTIyMDIyNTA0NTAyOVowQzEL\n" +
            "MAkGA1UEBhMCVVMxEzARBgNVBAgMCkNhbGlmb3JuaWExEDAOBgNVBAoMB0V4YW1w\n" +
            "bGUxDTALBgNVBAsMBFRlc3QwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIB\n" +
            "AQDnOL2hB/GYyYziXo/ppxi7V1LfMMLeHt0lZbYlrmNxUlln4naI4B4Lkg75eb1Y\n" +
            "DgC7MZQd5nKijK9Dkq52Z2zLxaqBYnLxKJ36qKPqbtTL3I8mfUvVEeNIDN/8YTAt\n" +
            "suIEQi54dNtQVrB4YReMdnUq+xCKLAfEio4QLEQr7KtyCBXHZpM7RYRT0giQFvDU\n" +
            "2kls9lFLeqKXgocnA7VpoL0V12hpxDeJoRm1szf0M5YXGJumQLaE5qM/+P2OOhw/\n" +
            "T+xkupy2GF02s6FBXkH7NrFIjtuBSaVhSvCG/N7njWSn339thr3kiPEaCS4KSH5E\n" +
            "E6FEazxZQrTCbkQQ+v3y1pS1AgMBAAGjUzBRMA8GA1UdEwEB/wQFMAMBAf8wHQYD\n" +
            "VR0OBBYEFMFw2FWUvwZx3FJjm1G9TujCjAJSMB8GA1UdIwQYMBaAFMFw2FWUvwZx\n" +
            "3FJjm1G9TujCjAJSMA0GCSqGSIb3DQEBCwUAA4IBAQCJsJjeYcT/GtKp64C+9KCi\n" +
            "Vgw/WnBZwbosSFZmqyID8aAnAxaGMkZ2B2pUZHTtCkBf6d9c0tuWb5yF8npV77sE\n" +
            "bZqeNg2GU7EvH3WPgPbQVT7+Qb+WbY3EEPgJHLytch61Rm/TRQ3OqD0B+Gs7YjAU\n" +
            "fEspmk1JJ6DWuXX13SHoGWgVnO7rXBnCJaGnGpONtggG4oO5hrwnMzQZKh5eZDhC\n" +
            "7tkNPqVDoLv+QqnFk8q6k8hhxnVf+aw56IdsebN+9Bi+Lv6OQ+stKUo/u/RTW2z1\n" +
            "odCOwc8DPF3jbacJsOmLhl3ciuWGOckx9KCvaBeTTgkPdLQH1gpNha2tnqgxUXmC\n" +
            "-----END CERTIFICATE-----",

            "MIIEvwIBADANBgkqhkiG9w0BAQEFAASCBKkwggSlAgEAAoIBAQDnOL2hB/GYyYzi\n" +
            "Xo/ppxi7V1LfMMLeHt0lZbYlrmNxUlln4naI4B4Lkg75eb1YDgC7MZQd5nKijK9D\n" +
            "kq52Z2zLxaqBYnLxKJ36qKPqbtTL3I8mfUvVEeNIDN/8YTAtsuIEQi54dNtQVrB4\n" +
            "YReMdnUq+xCKLAfEio4QLEQr7KtyCBXHZpM7RYRT0giQFvDU2kls9lFLeqKXgocn\n" +
            "A7VpoL0V12hpxDeJoRm1szf0M5YXGJumQLaE5qM/+P2OOhw/T+xkupy2GF02s6FB\n" +
            "XkH7NrFIjtuBSaVhSvCG/N7njWSn339thr3kiPEaCS4KSH5EE6FEazxZQrTCbkQQ\n" +
            "+v3y1pS1AgMBAAECggEBAJQAKLkTWZx/njMjbiCT+Wuo6H2+O21r+ge/BAk4h6R4\n" +
            "nou1VEQmmHS1h+o992mOhP9NK867vDK5tFGfaRaW+vevzYTF3GbqpbxVB56+VG0s\n" +
            "/2AWoVx/96gdvZ1RJEKMFsm9BvvJaLwS0SAsnaMmC7d4Ps0Cg/JU8bv+aaBn/BGf\n" +
            "TWiofYWeUk6llco4kO9H2APxUVzlaUUU/cPAJqX7XktnhDCI9/esuVg7nVR0XxOF\n" +
            "GDrV/jdqSYmSbp4aTRXgI9nwxOmlKiGgevTrCUXl3/KaJxZekllVjushY1VVzgbY\n" +
            "K5R4bcN5MXMmFdgF9DTEW72RqEfg9iXqyhYbZp3Q/UECgYEA/yiaJd0w2HS22HSg\n" +
            "o4dJ072WbyR3qUqQmPbSUn9hBQTJAz1eX3cci8u4oawo/S+jN38b/DfpSg3eIMLB\n" +
            "vnXW3wZtodpJnFaweKd3yUaSF2r9vZRHJgfPfe67VbruEOF6BsCjTq/deGeNnGeH\n" +
            "/IDVn9WtSeRX/bv/s5YHvGaHGGUCgYEA5/vugmilOBq979EqksCG/7EQHSOoEukO\n" +
            "J/aunDyEwz+BMEHOEW7tDMUefdiSfnGXSW+ZTmpmvc+aLk37Xuo34jpugK5ayUFY\n" +
            "iYVgiqdnygGiBevBM2o0O/parQkAGEB8GPButrYItUzubUgXnuw+EdMiXGnpjJaK\n" +
            "S3dPYJEHvhECgYEAjqIIwV/LPUTJLWjMn30yBN43KLvu9ECNYiSfX6R6/I43O8tj\n" +
            "ZOQ1nePsutt9MkMd7xjr8OrkSxRDdnbITQqcaaGzSUW33mALV/btnCMJ6XNSklZA\n" +
            "C39UOuZn7D2JdQBF8V5gK81ddUAVxjeNqdXvFOEidGrj0R/1iVM10dhSbo0CgYBk\n" +
            "P8GtR02Gtj+4P/qW2m48VqbxALSkH2SHrpl8WMbCnVHVqcpETFxSNWjc11dPHwVS\n" +
            "rdBhS6fEhM9LDVYAiVTHBZs1LqN67ys0mpfCs18ts5Dx4BRohI+4D5NZzVbmJA+8\n" +
            "s0IU4QtYVbt/LDVQ7yRPjZ7+sqJDp9ZxkEiUIXhoEQKBgQCPG2f8BhsCYNAOV60F\n" +
            "hJVjhDdf59Mia3B2J9SSnrg6Tl2rWB7GPP19TiSPFS6Sn95mWrMjZ2ZuOXtCYV4H\n" +
            "+hmu87AV2CXFhW5c588cajXMT92GFVMSXdE1OHRzDjhpH+/ll8SnmQa7sXmEV36+\n" +
            "sawd7mwcB9IEi562wglADxBUCA=="
            ),

    // Version: 3 (0x2)
    // Serial Number: 14845986384898254166 (0xce0789f5ac256556)
    // Signature Algorithm: sha1WithRSAEncryption
    // Issuer: C=US, ST=California, O=Example, OU=Test
    // Validity
    //     Not Before: Jan 26 04:50:29 2022 GMT
    //     Not After : Feb 25 04:50:29 2022 GMT
    // Subject: C=US, ST=California, O=Example, OU=Test, CN=www.example.com
    // Public Key Algorithm: rsaEncryption
    SERVER_EXAMPLE_RSA("RSA",

            "-----BEGIN CERTIFICATE-----\n" +
            "MIIDjDCCAnSgAwIBAgIJAM4HifWsJWVWMA0GCSqGSIb3DQEBBQUAMEMxCzAJBgNV\n" +
            "BAYTAlVTMRMwEQYDVQQIDApDYWxpZm9ybmlhMRAwDgYDVQQKDAdFeGFtcGxlMQ0w\n" +
            "CwYDVQQLDARUZXN0MB4XDTIyMDEyNjA0NTAyOVoXDTIyMDIyNTA0NTAyOVowXTEL\n" +
            "MAkGA1UEBhMCVVMxEzARBgNVBAgMCkNhbGlmb3JuaWExEDAOBgNVBAoMB0V4YW1w\n" +
            "bGUxDTALBgNVBAsMBFRlc3QxGDAWBgNVBAMMD3d3dy5leGFtcGxlLmNvbTCCASIw\n" +
            "DQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAK/i4bVSAF6/aFHyzrFAON8Uwn2a\n" +
            "Q9jFoAMowUkH6+PCexlt4wCOEMVH9IQPa1yzeVlkYRqHggz3i6aVYvd27Q+c6vOF\n" +
            "FuRegWdJusyEuoXd5nwxSGiZZMLmFQswODSPmucroCG+Tq9RKY5oEKiRP8tUDqyn\n" +
            "eE52PaeSR2Q6mng7BM5Llj7amVgimO3jlH2AWLLpHNTkc3j/M1QrLFV2PqN4dpxT\n" +
            "OeFuVWzpfTWJSH+9Kq4u/zBMbYl8ON7DSgJAzc2BOw8VPIYIK6JIw6IDbLn4dIQf\n" +
            "GikdgHKB2K6EDOc9LuX6UqQQODDFU88muAyPgpGfUQjxKZeUoTqoAFyisI0CAwEA\n" +
            "AaNpMGcwCQYDVR0TBAIwADAdBgNVHQ4EFgQUQqf/4nqluVMZ8huD3I5FNqLXTqAw\n" +
            "HwYDVR0jBBgwFoAUwXDYVZS/BnHcUmObUb1O6MKMAlIwGgYDVR0RBBMwEYIPd3d3\n" +
            "LmV4YW1wbGUuY29tMA0GCSqGSIb3DQEBBQUAA4IBAQBl8FJD98fJh/0KY+3LtZDW\n" +
            "CQZDeBSfnpq4milrvHH+gcOCaKYlB9702tAQ6rL1c1dLz/Lpw1x7EYvO8XIwXMRc\n" +
            "DZghf8EJ6wMpZbLVLAQLCTggiB65XPwmhUoMvgVRVLYoXT3ozmNPt7P+ZURisqem\n" +
            "0/xVVfxqmHw9Hb4DNlc7oZDgOK7IrqriaBK6Amsu3m2eThsvkwTdJfeFG3ZdV6x5\n" +
            "mbaGZDMt/wgWIqyq5CZNpXPaFRH7KM4zhcoqXvFAoq9jxWOuBkJUUx5EHaxGhhPw\n" +
            "SMgE1yl4O+N7GJmF/WMR5zp2LGKMqJ3vwLPv6QNkUmLwB5ZLYJ8E2dpj6DjQWa7X\n" +
            "-----END CERTIFICATE-----",

            "MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQCv4uG1UgBev2hR\n" +
            "8s6xQDjfFMJ9mkPYxaADKMFJB+vjwnsZbeMAjhDFR/SED2tcs3lZZGEah4IM94um\n" +
            "lWL3du0PnOrzhRbkXoFnSbrMhLqF3eZ8MUhomWTC5hULMDg0j5rnK6Ahvk6vUSmO\n" +
            "aBCokT/LVA6sp3hOdj2nkkdkOpp4OwTOS5Y+2plYIpjt45R9gFiy6RzU5HN4/zNU\n" +
            "KyxVdj6jeHacUznhblVs6X01iUh/vSquLv8wTG2JfDjew0oCQM3NgTsPFTyGCCui\n" +
            "SMOiA2y5+HSEHxopHYBygdiuhAznPS7l+lKkEDgwxVPPJrgMj4KRn1EI8SmXlKE6\n" +
            "qABcorCNAgMBAAECggEBAJb2SvfP7BVmf+lmV9V249lE/jHECFu0M8TCZDOEowiX\n" +
            "0gRfdqjxRp+tRMdcXK/yM0Nwjo+wowTyK2DNc2YnIw11h4uAPcfA/ZxjgfssKNPh\n" +
            "Q4Rw4E826W8HACTcPEGQyEmF/ik4KFz9coeR9kpYcMLZ4MZ77xyZDA4Z1UDHs/Fg\n" +
            "egrd4k35fxIFOavsjNuekMIjZlyQ2xU1a11QDBrZAu/pjITXXulg4jCxLbeNOOPs\n" +
            "hH+Sx+jnsVV7qIBNwulzxEdpb3+NkWIMmOQK4HOSeHgjRVvSXXBPgYadMaP/Bzvx\n" +
            "AwJ9WeLZg7KWHE03aOc7CSMoyHfmhXx3gD8icGpSq8ECgYEA3TWGBSgny/wket+V\n" +
            "65ldWjp4NOKHbLPtBdL9ygIO+1bGfLl5srCxUWHBYdcWCXKuB5ALCBu0hMY+TlwF\n" +
            "s/BzZmvVW7RLAEZZ3Q5HpawDlr9j8Kenm4X2Mqh0MSkmwIDRDHF8jRXAvWIzcBiS\n" +
            "+rPZm2CMZpznUSE4X+GrvTSCBbkCgYEAy4yJ58wNUavj/tR8KySn5ygPABFZ1Uoy\n" +
            "pIyabm18Oe3gCl5UulBskoN0WreTKnA4r9jGlnrgeuu5WfMK53AZKbC+YduZixW4\n" +
            "QFuJBSMbFCBDt4nkhkMMR7VcV4jIqaOK7qNrs7ubGTZhsG8wj6/WWdCp3Avnx1rS\n" +
            "WCCoYNhAK3UCgYADaLnCBpZmbGJbimqTEPABXflQR1Vy9WrnthK3NETq1rGEZo9b\n" +
            "k6GH8Yu7aEcsqhnIgA3LeDHWAgAf0Qc9eK0unObS3Ppy7KKh54BvKzF690QhB1Rr\n" +
            "7yqWKUZxI4M3YETYfj8/JWCtCoBkb9yEBJWL8Xb4dd6Sv4JQ5/dvmQmP8QKBgBX+\n" +
            "5+Aeksnik060U36uBV7bW1OcjGKaFALoFsAcILJ53B4Ct5Eyo6jpf6dV8xdA7T9D\n" +
            "Y6JbQOrHkk4AD4uW94Ej0k7s1hjLjg+WVKYzdvejzO2GfyVrFWaiWIo1A8ohHCBR\n" +
            "lI/llAsTb1cLjOnaDIXEILbgqnlGfTh8vvVIKRcJAoGALF6Q1Ca2GIx4kJZ2/Az5\n" +
            "qx89q95wQoWVHcJCA91g/GKg/bD0ZJMWEhhVH3QcnDOU7afRGKOPGdSSBdPQnspQ\n" +
            "1OPpPjA3U5Mffy5PJ2bHTpPBeeHOzDO4Jt073zK3N81QovJzS8COoOwuGdcocURH\n" +
            "mFMnEtK7d+EK1C1NTWDyNzU="
            ),


    // Version: 3 (0x2)
    // Serial Number: 14845986384898254167 (0xce0789f5ac256557)
    // Signature Algorithm: sha1WithRSAEncryption
    // Issuer: C=US, ST=California, O=Example, OU=Test
    // Validity
    //     Not Before: Jan 26 04:50:29 2022 GMT
    //     Not After : Feb 25 04:50:29 2022 GMT
    // Subject: C=US, ST=California, O=Example, OU=Test, CN=Do-Not-Reply
    // Public Key Algorithm: rsaEncryption
    CLIENT_EXAMPLE_RSA("RSA",

            "-----BEGIN CERTIFICATE-----\n" +
            "MIIDkjCCAnqgAwIBAgIJAM4HifWsJWVXMA0GCSqGSIb3DQEBBQUAMEMxCzAJBgNV\n" +
            "BAYTAlVTMRMwEQYDVQQIDApDYWxpZm9ybmlhMRAwDgYDVQQKDAdFeGFtcGxlMQ0w\n" +
            "CwYDVQQLDARUZXN0MB4XDTIyMDEyNjA0NTAyOVoXDTIyMDIyNTA0NTAyOVowWjEL\n" +
            "MAkGA1UEBhMCVVMxEzARBgNVBAgMCkNhbGlmb3JuaWExEDAOBgNVBAoMB0V4YW1w\n" +
            "bGUxDTALBgNVBAsMBFRlc3QxFTATBgNVBAMMDERvLU5vdC1SZXBseTCCASIwDQYJ\n" +
            "KoZIhvcNAQEBBQADggEPADCCAQoCggEBAODbsCteHcAg3dktUO5fiPTytIahKZMg\n" +
            "U2h6h0+BT803tYcdN6WDHnLNlU/3iFnBMpyTwzWhYIftC9c/TIkXBAGfWl4HHQgc\n" +
            "x08NHPms0E+GF6CDthvHERSvRCBrIyVna0KIZPemBzUfeBeNdiqwsLvg+C5dqWu5\n" +
            "YcILvL6GzTMvdMwJeEX+c2ZYHibqd8aZydWsT+IPVZnueDX6KTOnYvexLFIoid2a\n" +
            "62FavnMWiPKnICertDDg+gHlN2XceW3tlYQwO+HMig4DH3u2x0SApOoM3y8k29Ew\n" +
            "Fn6wquSRomcbDiI8SEOeaBFenu6W0g24iaxIZfqEM52kPbQFdzqg+O0CAwEAAaNy\n" +
            "MHAwCQYDVR0TBAIwADAdBgNVHQ4EFgQU0t+I5iAfPq6C/I2CSvTKHGK6+2kwHwYD\n" +
            "VR0jBBgwFoAUwXDYVZS/BnHcUmObUb1O6MKMAlIwIwYDVR0RBBwwGoEYZG8tbm90\n" +
            "LXJlcGx5QGV4YW1wbGUuY29tMA0GCSqGSIb3DQEBBQUAA4IBAQBqWk35XXpb3L+N\n" +
            "7kPlNmSlhfamenVOVYxPBq/tSFpl728CV0OrGAy79awEFDvzhpbBg9Mz7HS/a7ax\n" +
            "f+lBbsAt1maWlsVSsaaJrmy3znl9CZiIg+ykZ5ZzLS5FkIbQ6LkzvxYZJ1JYCzXm\n" +
            "P/5+rbQyIvQaDGL23PmE7AB2Q0q+imt4b9HKs+SnI4XERyj8OF/kseRtILtP2ltV\n" +
            "r+3XgcBxwyU17CLwsHrjnQ8/1eGovBKzGAfUXeHBdzOuD3ZemjnqwlzQTf2TAkBP\n" +
            "OuMc8gr2Umc5tMtdiRUFPODO7DG7vB7LKEsJGKWLUtGbR+3S55lIcCF5NFZ//TNZ\n" +
            "4x2GPOJ+\n" +
            "-----END CERTIFICATE-----",

            "MIIEvQIBADANBgkqhkiG9w0BAQEFAASCBKcwggSjAgEAAoIBAQDg27ArXh3AIN3Z\n" +
            "LVDuX4j08rSGoSmTIFNoeodPgU/NN7WHHTelgx5yzZVP94hZwTKck8M1oWCH7QvX\n" +
            "P0yJFwQBn1peBx0IHMdPDRz5rNBPhhegg7YbxxEUr0QgayMlZ2tCiGT3pgc1H3gX\n" +
            "jXYqsLC74PguXalruWHCC7y+hs0zL3TMCXhF/nNmWB4m6nfGmcnVrE/iD1WZ7ng1\n" +
            "+ikzp2L3sSxSKIndmuthWr5zFojypyAnq7Qw4PoB5Tdl3Hlt7ZWEMDvhzIoOAx97\n" +
            "tsdEgKTqDN8vJNvRMBZ+sKrkkaJnGw4iPEhDnmgRXp7ultINuImsSGX6hDOdpD20\n" +
            "BXc6oPjtAgMBAAECggEAKjqX900RoUeK4oKUNHBUtEvwg2g4+pyTjYeVaeULK6tO\n" +
            "uDVQghEB4uWhKQd/3/tcmfNWMfhAvMZT9vS4Vvavle5rdkU3upJNDBeWXX2LEaRJ\n" +
            "Q6f4x3a3Sn8v+DamvxuRFUmwTKItsFhcoW+7xYCxcFdrxKlqbATAy0SRCecfGoFw\n" +
            "9pAsFIgIqLGQtV9V9fZWKdXIfLkH3venNImHt0n5pYadl4kZu0gNCmOGRO1MqB51\n" +
            "bdAck3dBg22TE5+sZBIZmCjBAEbrc2RUQu5mAoDs8eeenxBlFYszWZxqM8BkJL5e\n" +
            "SqQkbc18E8Gzdx52xEx6BCLTq0MITKliKX4B2tsQsQKBgQD1DIvt13yg9LlyoiAb\n" +
            "Fc1zzKZQBPRgDUJCGzCPeC6AqbPUjoxvFAHxGNgmJXqAXwsqcs88qlSGAjk6ANAx\n" +
            "fHWUQ3UmkZjGvV0ru3rIPcXvS7isjzm/cbq1YZua6+ohFl4+hojc/iyUrOaCd9uY\n" +
            "V2zwrr+A0JJrlev72niEuAtEmwKBgQDq6CaLP79dHqAOIV43+SyX7KdwNkMMWIR7\n" +
            "El6E/74V0IWOYWFXLmV4sX6BKi0ZBTFZ3wKwMTDqQBYD2/a7gq43HjzuWu+2dFhA\n" +
            "pCQumMOKNqcNS9NldqoxAiGLxC+JMhGGyf3RO0Ey9gdPnQuje3133Wce8WWaHHv2\n" +
            "lD5BcmzdFwKBgQCCeca7wiPy07s2ZUqxAT/eq5XWP30a85RW/IEzsusXyMQepjPy\n" +
            "JPYPuInGbeg3F+QrGuxrQco1fFOaJbq0zq8QXYawHY/6KfPFCFMM8Y9FpczT3IME\n" +
            "A3tFfo5Kw9hq+6z8n8eZ26BDHXiy+Tysdchksrb20JdVv4LiG+ZVzGT7hwKBgG+b\n" +
            "Bp0IF4JFh6PPBLWxRBeWT2MH1Mkr0R2r945W91fj72BbMeU63Oj/42u4vx5xEiZx\n" +
            "xxQw+t2AvzTsMAicqOr1CdvxBoz4L+neUnZ1DApBtxKhIPnG7EtGiOuftToIuL0C\n" +
            "gP4EmhB9RbH0mk/83vqxDUptRGl4+QiJHB76H3DXAoGASGT6tfs1/92rGqe0COm3\n" +
            "aHpelvW7Wwr9AuBumE7/ur/JWAAiM4pm6w7m5H5duYZtG3eWz1s+jvjy8jIPBIkN\n" +
            "RA2DoneC2J/tsRRNFBqZrOWon5Jv4dc9R79qR13B/Oqu8H6FYSB2oDyh67Csud3q\n" +
            "PRrz4o7UKM+HQ/obr1rUYqM="
            );

    final String keyAlgo;
    final String certStr;
    final String privateKeyStr;

    // Trusted certificate
    private final static SSLExampleCert[] TRUSTED_CERTS = {
            SSLExampleCert.CA_RSA
    };

    // Server certificate.
    private final static SSLExampleCert[] SERVER_CERTS = {
            SSLExampleCert.SERVER_EXAMPLE_RSA
    };

    // Client certificate.
    private final static SSLExampleCert[] CLIENT_CERTS = {
            SSLExampleCert.CLIENT_EXAMPLE_RSA
    };

    // Set "www.example.com" to loopback address.
    static {
        String hostsFileName = System.getProperty("jdk.net.hosts.file");
        String loopbackHostname =
                InetAddress.getLoopbackAddress().getHostAddress() +
                " " + "www.example.com    www.example.com.\n";
        try (FileWriter writer= new FileWriter(hostsFileName, false)) {
             writer.write(loopbackHostname);
        } catch (IOException ioe) {
             // ignore
        }
    }

    SSLExampleCert(String keyAlgo, String certStr, String privateKeyStr) {
        this.keyAlgo = keyAlgo;
        this.certStr = certStr;
        this.privateKeyStr = privateKeyStr;
    }

    public static SSLContext createClientSSLContext() throws Exception {
        return createSSLContext(TRUSTED_CERTS, CLIENT_CERTS);
    }

    public static SSLContext createServerSSLContext() throws Exception {
        return createSSLContext(TRUSTED_CERTS, SERVER_CERTS);
    }

    private static SSLContext createSSLContext(
            SSLExampleCert[] trustedCerts,
            SSLExampleCert[] endEntityCerts) throws Exception {
        char[] passphrase = "passphrase".toCharArray();

        // Generate certificate from cert string.
        CertificateFactory cf = CertificateFactory.getInstance("X.509");

        // Import the trusted certs.
        KeyStore ts = null;     // trust store
        if (trustedCerts != null && trustedCerts.length != 0) {
            ts = KeyStore.getInstance("PKCS12");
            ts.load(null, null);

            Certificate[] trustedCert = new Certificate[trustedCerts.length];
            for (int i = 0; i < trustedCerts.length; i++) {
                try (ByteArrayInputStream is = new ByteArrayInputStream(
                        trustedCerts[i].certStr.getBytes())) {
                    trustedCert[i] = cf.generateCertificate(is);
                }

                ts.setCertificateEntry("trusted-cert-" +
                        trustedCerts[i].name(), trustedCert[i]);
            }
        }

        // Import the key materials.
        KeyStore ks = null;     // trust store
        if (endEntityCerts != null && endEntityCerts.length != 0) {
            ks = KeyStore.getInstance("PKCS12");
            ks.load(null, null);

            for (SSLExampleCert endEntityCert : endEntityCerts) {
                // generate the private key.
                PKCS8EncodedKeySpec priKeySpec = new PKCS8EncodedKeySpec(
                        Base64.getMimeDecoder()
                                .decode(endEntityCert.privateKeyStr));
                KeyFactory kf =
                        KeyFactory.getInstance(
                                endEntityCert.keyAlgo);
                PrivateKey priKey = kf.generatePrivate(priKeySpec);

                // generate certificate chain
                Certificate keyCert;
                try (ByteArrayInputStream is = new ByteArrayInputStream(
                        endEntityCert.certStr.getBytes())) {
                    keyCert = cf.generateCertificate(is);
                }

                Certificate[] chain = new Certificate[]{keyCert};

                // import the key entry.
                ks.setKeyEntry("end-entity-cert-" +
                                endEntityCert.name(),
                        priKey, passphrase, chain);
            }
        }

        // Create an SSLContext object.
        TrustManagerFactory tmf =
                TrustManagerFactory.getInstance("PKIX");
        tmf.init(ts);

        SSLContext context = SSLContext.getInstance("TLS");
        if (endEntityCerts != null && endEntityCerts.length != 0) {
            KeyManagerFactory kmf =
                    KeyManagerFactory.getInstance("NewSunX509");
            kmf.init(ks, passphrase);

            context.init(kmf.getKeyManagers(), tmf.getTrustManagers(), null);
        } else {
            context.init(null, tmf.getTrustManagers(), null);
        }

        return context;
    }
}

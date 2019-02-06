/*
 * Copyright (c) 1997, 2017, Oracle and/or its affiliates. All rights reserved.
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

import java.io.*;
import java.security.cert.CertPathValidatorException;
import java.security.cert.PKIXBuilderParameters;
import java.util.*;
import java.util.stream.Collectors;
import java.util.zip.*;
import java.util.jar.*;
import java.math.BigInteger;
import java.net.URI;
import java.net.URISyntaxException;
import java.text.Collator;
import java.text.MessageFormat;
import java.security.cert.Certificate;
import java.security.cert.X509Certificate;
import java.security.cert.CertificateException;
import java.security.*;
import java.lang.reflect.Constructor;

import com.sun.jarsigner.ContentSigner;
import com.sun.jarsigner.ContentSignerParameters;
import java.net.SocketTimeoutException;
import java.net.URL;
import java.net.URLClassLoader;
import java.security.cert.CertPath;
import java.security.cert.CertificateExpiredException;
import java.security.cert.CertificateFactory;
import java.security.cert.CertificateNotYetValidException;
import java.security.cert.TrustAnchor;
import java.util.Map.Entry;
import sun.security.pkcs.PKCS7;
import sun.security.pkcs.SignerInfo;
import sun.security.timestamp.TimestampToken;
import sun.security.tools.KeyStoreUtil;
import sun.security.tools.PathList;
import sun.security.validator.Validator;
import sun.security.validator.ValidatorException;
import sun.security.x509.*;
import sun.security.util.*;
import java.util.Base64;


/**
 * <p>The jarsigner utility.
 *
 * The exit codes for the main method are:
 *
 * 0: success
 * 1: any error that the jar cannot be signed or verified, including:
 *      keystore loading error
 *      TSP communication error
 *      jarsigner command line error...
 * otherwise: error codes from -strict
 *
 * @author Roland Schemers
 * @author Jan Luehe
 */

public class Main {

    // for i18n
    private static final java.util.ResourceBundle rb =
        java.util.ResourceBundle.getBundle
        ("sun.security.tools.jarsigner.Resources");
    private static final Collator collator = Collator.getInstance();
    static {
        // this is for case insensitive string comparisions
        collator.setStrength(Collator.PRIMARY);
    }

    private static final String META_INF = "META-INF/";

    private static final Class<?>[] PARAM_STRING = { String.class };

    private static final String NONE = "NONE";
    private static final String P11KEYSTORE = "PKCS11";

    private static final long SIX_MONTHS = 180*24*60*60*1000L; //milliseconds
    private static final long ONE_YEAR = 366*24*60*60*1000L;

    private static final DisabledAlgorithmConstraints DISABLED_CHECK =
            new DisabledAlgorithmConstraints(
                    DisabledAlgorithmConstraints.PROPERTY_JAR_DISABLED_ALGS);

    private static final Set<CryptoPrimitive> DIGEST_PRIMITIVE_SET = Collections
            .unmodifiableSet(EnumSet.of(CryptoPrimitive.MESSAGE_DIGEST));
    private static final Set<CryptoPrimitive> SIG_PRIMITIVE_SET = Collections
            .unmodifiableSet(EnumSet.of(CryptoPrimitive.SIGNATURE));

    static final String VERSION = "1.0";

    static final int IN_KEYSTORE = 0x01;        // signer is in keystore
    static final int IN_SCOPE = 0x02;
    static final int NOT_ALIAS = 0x04;          // alias list is NOT empty and
    // signer is not in alias list
    static final int SIGNED_BY_ALIAS = 0x08;    // signer is in alias list

    // Attention:
    // This is the entry that get launched by the security tool jarsigner.
    public static void main(String args[]) throws Exception {
        Main js = new Main();
        js.run(args);
    }

    X509Certificate[] certChain;    // signer's cert chain (when composing)
    PrivateKey privateKey;          // private key
    KeyStore store;                 // the keystore specified by -keystore
                                    // or the default keystore, never null

    String keystore; // key store file
    boolean nullStream = false; // null keystore input stream (NONE)
    boolean token = false; // token-based keystore
    String jarfile;  // jar files to sign or verify
    String alias;    // alias to sign jar with
    List<String> ckaliases = new ArrayList<>(); // aliases in -verify
    char[] storepass; // keystore password
    boolean protectedPath; // protected authentication path
    String storetype; // keystore type
    String providerName; // provider name
    Vector<String> providers = null; // list of providers
    // arguments for provider constructors
    HashMap<String,String> providerArgs = new HashMap<>();
    char[] keypass; // private key password
    String sigfile; // name of .SF file
    String sigalg; // name of signature algorithm
    String digestalg = "SHA-256"; // name of digest algorithm
    String signedjar; // output filename
    String tsaUrl; // location of the Timestamping Authority
    String tsaAlias; // alias for the Timestamping Authority's certificate
    String altCertChain; // file to read alternative cert chain from
    String tSAPolicyID;
    String tSADigestAlg = "SHA-256";
    boolean verify = false; // verify the jar
    String verbose = null; // verbose output when signing/verifying
    boolean showcerts = false; // show certs when verifying
    boolean debug = false; // debug
    boolean signManifest = true; // "sign" the whole manifest
    boolean externalSF = true; // leave the .SF out of the PKCS7 block
    boolean strict = false;  // treat warnings as error

    // read zip entry raw bytes
    private ByteArrayOutputStream baos = new ByteArrayOutputStream(2048);
    private byte[] buffer = new byte[8192];
    private ContentSigner signingMechanism = null;
    private String altSignerClass = null;
    private String altSignerClasspath = null;
    private ZipFile zipFile = null;

    // Informational warnings
    private boolean hasExpiringCert = false;
    private boolean hasExpiringTsaCert = false;
    private boolean noTimestamp = true;

    // Expiration date. The value could be null if signed by a trusted cert.
    private Date expireDate = null;
    private Date tsaExpireDate = null;

    // If there is a time stamp block inside the PKCS7 block file
    boolean hasTimestampBlock = false;


    // Severe warnings.

    // jarsigner used to check signer cert chain validity and key usages
    // itself and set various warnings. Later CertPath validation is
    // added but chainNotValidated is only flagged when no other existing
    // warnings are set. TSA cert chain check is added separately and
    // only tsaChainNotValidated is set, i.e. has no affect on hasExpiredCert,
    // notYetValidCert, or any badXyzUsage.

    private int weakAlg = 0; // 1. digestalg, 2. sigalg, 4. tsadigestalg
    private boolean hasExpiredCert = false;
    private boolean hasExpiredTsaCert = false;
    private boolean notYetValidCert = false;
    private boolean chainNotValidated = false;
    private boolean tsaChainNotValidated = false;
    private boolean notSignedByAlias = false;
    private boolean aliasNotInStore = false;
    private boolean hasUnsignedEntry = false;
    private boolean badKeyUsage = false;
    private boolean badExtendedKeyUsage = false;
    private boolean badNetscapeCertType = false;
    private boolean signerSelfSigned = false;

    private Throwable chainNotValidatedReason = null;
    private Throwable tsaChainNotValidatedReason = null;

    private boolean seeWeak = false;

    PKIXBuilderParameters pkixParameters;
    Set<X509Certificate> trustedCerts = new HashSet<>();

    public void run(String args[]) {
        try {
            parseArgs(args);

            // Try to load and install the specified providers
            if (providers != null) {
                ClassLoader cl = ClassLoader.getSystemClassLoader();
                Enumeration<String> e = providers.elements();
                while (e.hasMoreElements()) {
                    String provName = e.nextElement();
                    Class<?> provClass;
                    if (cl != null) {
                        provClass = cl.loadClass(provName);
                    } else {
                        provClass = Class.forName(provName);
                    }

                    String provArg = providerArgs.get(provName);
                    Object obj;
                    if (provArg == null) {
                        obj = provClass.newInstance();
                    } else {
                        Constructor<?> c =
                                provClass.getConstructor(PARAM_STRING);
                        obj = c.newInstance(provArg);
                    }

                    if (!(obj instanceof Provider)) {
                        MessageFormat form = new MessageFormat(rb.getString
                            ("provName.not.a.provider"));
                        Object[] source = {provName};
                        throw new Exception(form.format(source));
                    }
                    Security.addProvider((Provider)obj);
                }
            }

            if (verify) {
                try {
                    loadKeyStore(keystore, false);
                } catch (Exception e) {
                    if ((keystore != null) || (storepass != null)) {
                        System.out.println(rb.getString("jarsigner.error.") +
                                        e.getMessage());
                        System.exit(1);
                    }
                }
                /*              if (debug) {
                    SignatureFileVerifier.setDebug(true);
                    ManifestEntryVerifier.setDebug(true);
                }
                */
                verifyJar(jarfile);
            } else {
                loadKeyStore(keystore, true);
                getAliasInfo(alias);

                // load the alternative signing mechanism
                if (altSignerClass != null) {
                    signingMechanism = loadSigningMechanism(altSignerClass,
                        altSignerClasspath);
                }
                signJar(jarfile, alias, args);
            }
        } catch (Exception e) {
            System.out.println(rb.getString("jarsigner.error.") + e);
            if (debug) {
                e.printStackTrace();
            }
            System.exit(1);
        } finally {
            // zero-out private key password
            if (keypass != null) {
                Arrays.fill(keypass, ' ');
                keypass = null;
            }
            // zero-out keystore password
            if (storepass != null) {
                Arrays.fill(storepass, ' ');
                storepass = null;
            }
        }

        if (strict) {
            int exitCode = 0;
            if (weakAlg != 0 || chainNotValidated || hasExpiredCert
                    || hasExpiredTsaCert || notYetValidCert || signerSelfSigned) {
                exitCode |= 4;
            }
            if (badKeyUsage || badExtendedKeyUsage || badNetscapeCertType) {
                exitCode |= 8;
            }
            if (hasUnsignedEntry) {
                exitCode |= 16;
            }
            if (notSignedByAlias || aliasNotInStore) {
                exitCode |= 32;
            }
            if (tsaChainNotValidated) {
                exitCode |= 64;
            }
            if (exitCode != 0) {
                System.exit(exitCode);
            }
        }
    }

    /*
     * Parse command line arguments.
     */
    void parseArgs(String args[]) {
        /* parse flags */
        int n = 0;

        if (args.length == 0) fullusage();
        for (n=0; n < args.length; n++) {

            String flags = args[n];
            String modifier = null;

            if (flags.startsWith("-")) {
                int pos = flags.indexOf(':');
                if (pos > 0) {
                    modifier = flags.substring(pos+1);
                    flags = flags.substring(0, pos);
                }
            }

            if (!flags.startsWith("-")) {
                if (jarfile == null) {
                    jarfile = flags;
                } else {
                    alias = flags;
                    ckaliases.add(alias);
                }
            } else if (collator.compare(flags, "-keystore") == 0) {
                if (++n == args.length) usageNoArg();
                keystore = args[n];
            } else if (collator.compare(flags, "-storepass") ==0) {
                if (++n == args.length) usageNoArg();
                storepass = getPass(modifier, args[n]);
            } else if (collator.compare(flags, "-storetype") ==0) {
                if (++n == args.length) usageNoArg();
                storetype = args[n];
            } else if (collator.compare(flags, "-providerName") ==0) {
                if (++n == args.length) usageNoArg();
                providerName = args[n];
            } else if ((collator.compare(flags, "-provider") == 0) ||
                        (collator.compare(flags, "-providerClass") == 0)) {
                if (++n == args.length) usageNoArg();
                if (providers == null) {
                    providers = new Vector<String>(3);
                }
                providers.add(args[n]);

                if (args.length > (n+1)) {
                    flags = args[n+1];
                    if (collator.compare(flags, "-providerArg") == 0) {
                        if (args.length == (n+2)) usageNoArg();
                        providerArgs.put(args[n], args[n+2]);
                        n += 2;
                    }
                }
            } else if (collator.compare(flags, "-protected") ==0) {
                protectedPath = true;
            } else if (collator.compare(flags, "-certchain") ==0) {
                if (++n == args.length) usageNoArg();
                altCertChain = args[n];
            } else if (collator.compare(flags, "-tsapolicyid") ==0) {
                if (++n == args.length) usageNoArg();
                tSAPolicyID = args[n];
            } else if (collator.compare(flags, "-tsadigestalg") ==0) {
                if (++n == args.length) usageNoArg();
                tSADigestAlg = args[n];
            } else if (collator.compare(flags, "-debug") ==0) {
                debug = true;
            } else if (collator.compare(flags, "-keypass") ==0) {
                if (++n == args.length) usageNoArg();
                keypass = getPass(modifier, args[n]);
            } else if (collator.compare(flags, "-sigfile") ==0) {
                if (++n == args.length) usageNoArg();
                sigfile = args[n];
            } else if (collator.compare(flags, "-signedjar") ==0) {
                if (++n == args.length) usageNoArg();
                signedjar = args[n];
            } else if (collator.compare(flags, "-tsa") ==0) {
                if (++n == args.length) usageNoArg();
                tsaUrl = args[n];
            } else if (collator.compare(flags, "-tsacert") ==0) {
                if (++n == args.length) usageNoArg();
                tsaAlias = args[n];
            } else if (collator.compare(flags, "-altsigner") ==0) {
                if (++n == args.length) usageNoArg();
                altSignerClass = args[n];
            } else if (collator.compare(flags, "-altsignerpath") ==0) {
                if (++n == args.length) usageNoArg();
                altSignerClasspath = args[n];
            } else if (collator.compare(flags, "-sectionsonly") ==0) {
                signManifest = false;
            } else if (collator.compare(flags, "-internalsf") ==0) {
                externalSF = false;
            } else if (collator.compare(flags, "-verify") ==0) {
                verify = true;
            } else if (collator.compare(flags, "-verbose") ==0) {
                verbose = (modifier != null) ? modifier : "all";
            } else if (collator.compare(flags, "-sigalg") ==0) {
                if (++n == args.length) usageNoArg();
                sigalg = args[n];
            } else if (collator.compare(flags, "-digestalg") ==0) {
                if (++n == args.length) usageNoArg();
                digestalg = args[n];
            } else if (collator.compare(flags, "-certs") ==0) {
                showcerts = true;
            } else if (collator.compare(flags, "-strict") ==0) {
                strict = true;
            } else if (collator.compare(flags, "-h") == 0 ||
                        collator.compare(flags, "-help") == 0) {
                fullusage();
            } else {
                System.err.println(
                        rb.getString("Illegal.option.") + flags);
                usage();
            }
        }

        // -certs must always be specified with -verbose
        if (verbose == null) showcerts = false;

        if (jarfile == null) {
            System.err.println(rb.getString("Please.specify.jarfile.name"));
            usage();
        }
        if (!verify && alias == null) {
            System.err.println(rb.getString("Please.specify.alias.name"));
            usage();
        }
        if (!verify && ckaliases.size() > 1) {
            System.err.println(rb.getString("Only.one.alias.can.be.specified"));
            usage();
        }

        if (storetype == null) {
            storetype = KeyStore.getDefaultType();
        }
        storetype = KeyStoreUtil.niceStoreTypeName(storetype);

        try {
            if (signedjar != null && new File(signedjar).getCanonicalPath().equals(
                    new File(jarfile).getCanonicalPath())) {
                signedjar = null;
            }
        } catch (IOException ioe) {
            // File system error?
            // Just ignore it.
        }

        if (P11KEYSTORE.equalsIgnoreCase(storetype) ||
                KeyStoreUtil.isWindowsKeyStore(storetype)) {
            token = true;
            if (keystore == null) {
                keystore = NONE;
            }
        }

        if (NONE.equals(keystore)) {
            nullStream = true;
        }

        if (token && !nullStream) {
            System.err.println(MessageFormat.format(rb.getString
                (".keystore.must.be.NONE.if.storetype.is.{0}"), storetype));
            usage();
        }

        if (token && keypass != null) {
            System.err.println(MessageFormat.format(rb.getString
                (".keypass.can.not.be.specified.if.storetype.is.{0}"), storetype));
            usage();
        }

        if (protectedPath) {
            if (storepass != null || keypass != null) {
                System.err.println(rb.getString
                        ("If.protected.is.specified.then.storepass.and.keypass.must.not.be.specified"));
                usage();
            }
        }
        if (KeyStoreUtil.isWindowsKeyStore(storetype)) {
            if (storepass != null || keypass != null) {
                System.err.println(rb.getString
                        ("If.keystore.is.not.password.protected.then.storepass.and.keypass.must.not.be.specified"));
                usage();
            }
        }
    }

    static char[] getPass(String modifier, String arg) {
        char[] output = KeyStoreUtil.getPassWithModifier(modifier, arg, rb);
        if (output != null) return output;
        usage();
        return null;    // Useless, usage() already exit
    }

    static void usageNoArg() {
        System.out.println(rb.getString("Option.lacks.argument"));
        usage();
    }

    static void usage() {
        System.out.println();
        System.out.println(rb.getString("Please.type.jarsigner.help.for.usage"));
        System.exit(1);
    }

    static void fullusage() {
        System.out.println(rb.getString
                ("Usage.jarsigner.options.jar.file.alias"));
        System.out.println(rb.getString
                (".jarsigner.verify.options.jar.file.alias."));
        System.out.println();
        System.out.println(rb.getString
                (".keystore.url.keystore.location"));
        System.out.println();
        System.out.println(rb.getString
                (".storepass.password.password.for.keystore.integrity"));
        System.out.println();
        System.out.println(rb.getString
                (".storetype.type.keystore.type"));
        System.out.println();
        System.out.println(rb.getString
                (".keypass.password.password.for.private.key.if.different."));
        System.out.println();
        System.out.println(rb.getString
                (".certchain.file.name.of.alternative.certchain.file"));
        System.out.println();
        System.out.println(rb.getString
                (".sigfile.file.name.of.SF.DSA.file"));
        System.out.println();
        System.out.println(rb.getString
                (".signedjar.file.name.of.signed.JAR.file"));
        System.out.println();
        System.out.println(rb.getString
                (".digestalg.algorithm.name.of.digest.algorithm"));
        System.out.println();
        System.out.println(rb.getString
                (".sigalg.algorithm.name.of.signature.algorithm"));
        System.out.println();
        System.out.println(rb.getString
                (".verify.verify.a.signed.JAR.file"));
        System.out.println();
        System.out.println(rb.getString
                (".verbose.suboptions.verbose.output.when.signing.verifying."));
        System.out.println(rb.getString
                (".suboptions.can.be.all.grouped.or.summary"));
        System.out.println();
        System.out.println(rb.getString
                (".certs.display.certificates.when.verbose.and.verifying"));
        System.out.println();
        System.out.println(rb.getString
                (".tsa.url.location.of.the.Timestamping.Authority"));
        System.out.println();
        System.out.println(rb.getString
                (".tsacert.alias.public.key.certificate.for.Timestamping.Authority"));
        System.out.println();
        System.out.println(rb.getString
                (".tsapolicyid.tsapolicyid.for.Timestamping.Authority"));
        System.out.println();
        System.out.println(rb.getString
                (".tsadigestalg.algorithm.of.digest.data.in.timestamping.request"));
        System.out.println();
        System.out.println(rb.getString
                (".altsigner.class.class.name.of.an.alternative.signing.mechanism"));
        System.out.println();
        System.out.println(rb.getString
                (".altsignerpath.pathlist.location.of.an.alternative.signing.mechanism"));
        System.out.println();
        System.out.println(rb.getString
                (".internalsf.include.the.SF.file.inside.the.signature.block"));
        System.out.println();
        System.out.println(rb.getString
                (".sectionsonly.don.t.compute.hash.of.entire.manifest"));
        System.out.println();
        System.out.println(rb.getString
                (".protected.keystore.has.protected.authentication.path"));
        System.out.println();
        System.out.println(rb.getString
                (".providerName.name.provider.name"));
        System.out.println();
        System.out.println(rb.getString
                (".providerClass.class.name.of.cryptographic.service.provider.s"));
        System.out.println(rb.getString
                (".providerArg.arg.master.class.file.and.constructor.argument"));
        System.out.println();
        System.out.println(rb.getString
                (".strict.treat.warnings.as.errors"));
        System.out.println();

        System.exit(0);
    }

    void verifyJar(String jarName)
        throws Exception
    {
        boolean anySigned = false;  // if there exists entry inside jar signed
        JarFile jf = null;
        Map<String,String> digestMap = new HashMap<>();
        Map<String,PKCS7> sigMap = new HashMap<>();
        Map<String,String> sigNameMap = new HashMap<>();
        Map<String,String> unparsableSignatures = new HashMap<>();

        try {
            jf = new JarFile(jarName, true);
            Vector<JarEntry> entriesVec = new Vector<>();
            byte[] buffer = new byte[8192];

            Enumeration<JarEntry> entries = jf.entries();
            while (entries.hasMoreElements()) {
                JarEntry je = entries.nextElement();
                entriesVec.addElement(je);
                try (InputStream is = jf.getInputStream(je)) {
                    String name = je.getName();
                    if (signatureRelated(name)
                            && SignatureFileVerifier.isBlockOrSF(name)) {
                        String alias = name.substring(name.lastIndexOf('/') + 1,
                                name.lastIndexOf('.'));
                try {
                            if (name.endsWith(".SF")) {
                                Manifest sf = new Manifest(is);
                                boolean found = false;
                                for (Object obj : sf.getMainAttributes().keySet()) {
                                    String key = obj.toString();
                                    if (key.endsWith("-Digest-Manifest")) {
                                        digestMap.put(alias,
                                                key.substring(0, key.length() - 16));
                                        found = true;
                                        break;
                                    }
                                }
                                if (!found) {
                                    unparsableSignatures.putIfAbsent(alias,
                                        String.format(
                                            rb.getString("history.unparsable"),
                                            name));
                                }
                            } else {
                                sigNameMap.put(alias, name);
                                sigMap.put(alias, new PKCS7(is));
                            }
                        } catch (IOException ioe) {
                            unparsableSignatures.putIfAbsent(alias, String.format(
                                    rb.getString("history.unparsable"), name));
                        }
                    } else {
                        while (is.read(buffer, 0, buffer.length) != -1) {
                        // we just read. this will throw a SecurityException
                        // if  a signature/digest check fails.
                    }
                    }
                }
            }

            Manifest man = jf.getManifest();
            boolean hasSignature = false;

            // The map to record display info, only used when -verbose provided
            //      key: signer info string
            //      value: the list of files with common key
            Map<String,List<String>> output = new LinkedHashMap<>();

            if (man != null) {
                if (verbose != null) System.out.println();
                Enumeration<JarEntry> e = entriesVec.elements();

                String tab = rb.getString("6SPACE");

                while (e.hasMoreElements()) {
                    JarEntry je = e.nextElement();
                    String name = je.getName();

                    hasSignature = hasSignature
                            || SignatureFileVerifier.isBlockOrSF(name);

                    CodeSigner[] signers = je.getCodeSigners();
                    boolean isSigned = (signers != null);
                    anySigned |= isSigned;
                    hasUnsignedEntry |= !je.isDirectory() && !isSigned
                                        && !signatureRelated(name);

                    int inStoreOrScope = inKeyStore(signers);

                    boolean inStore = (inStoreOrScope & IN_KEYSTORE) != 0;
                    boolean inScope = (inStoreOrScope & IN_SCOPE) != 0;

                    notSignedByAlias |= (inStoreOrScope & NOT_ALIAS) != 0;
                    if (keystore != null) {
                        aliasNotInStore |= isSigned && (!inStore && !inScope);
                    }

                    // Only used when -verbose provided
                    StringBuffer sb = null;
                    if (verbose != null) {
                        sb = new StringBuffer();
                        boolean inManifest =
                            ((man.getAttributes(name) != null) ||
                             (man.getAttributes("./"+name) != null) ||
                             (man.getAttributes("/"+name) != null));
                        sb.append(
                          (isSigned ? rb.getString("s") : rb.getString("SPACE")) +
                          (inManifest ? rb.getString("m") : rb.getString("SPACE")) +
                          (inStore ? rb.getString("k") : rb.getString("SPACE")) +
                          (inScope ? rb.getString("i") : rb.getString("SPACE")) +
                          ((inStoreOrScope & NOT_ALIAS) != 0 ?"X":" ") +
                          rb.getString("SPACE"));
                        sb.append("|");
                    }

                    // When -certs provided, display info has extra empty
                    // lines at the beginning and end.
                    if (isSigned) {
                        if (showcerts) sb.append('\n');
                        for (CodeSigner signer: signers) {
                            // signerInfo() must be called even if -verbose
                            // not provided. The method updates various
                            // warning flags.
                            String si = signerInfo(signer, tab);
                            if (showcerts) {
                                sb.append(si);
                                sb.append('\n');
                            }
                        }
                    } else if (showcerts && !verbose.equals("all")) {
                        // Print no info for unsigned entries when -verbose:all,
                        // to be consistent with old behavior.
                        if (signatureRelated(name)) {
                            sb.append("\n" + tab + rb.getString(
                                    ".Signature.related.entries.") + "\n\n");
                        } else {
                            sb.append("\n" + tab + rb.getString(
                                    ".Unsigned.entries.") + "\n\n");
                        }
                    }

                    if (verbose != null) {
                        String label = sb.toString();
                        if (signatureRelated(name)) {
                            // Entries inside META-INF and other unsigned
                            // entries are grouped separately.
                            label = "-" + label;
                        }

                        // The label finally contains 2 parts separated by '|':
                        // The legend displayed before the entry names, and
                        // the cert info (if -certs specified).

                        if (!output.containsKey(label)) {
                            output.put(label, new ArrayList<String>());
                        }

                        StringBuffer fb = new StringBuffer();
                        String s = Long.toString(je.getSize());
                        for (int i = 6 - s.length(); i > 0; --i) {
                            fb.append(' ');
                        }
                        fb.append(s).append(' ').
                                append(new Date(je.getTime()).toString());
                        fb.append(' ').append(name);

                        output.get(label).add(fb.toString());
                    }
                }
            }
            if (verbose != null) {
                for (Entry<String,List<String>> s: output.entrySet()) {
                    List<String> files = s.getValue();
                    String key = s.getKey();
                    if (key.charAt(0) == '-') { // the signature-related group
                        key = key.substring(1);
                    }
                    int pipe = key.indexOf('|');
                    if (verbose.equals("all")) {
                        for (String f: files) {
                            System.out.println(key.substring(0, pipe) + f);
                            System.out.printf(key.substring(pipe+1));
                        }
                    } else {
                        if (verbose.equals("grouped")) {
                            for (String f: files) {
                                System.out.println(key.substring(0, pipe) + f);
                            }
                        } else if (verbose.equals("summary")) {
                            System.out.print(key.substring(0, pipe));
                            if (files.size() > 1) {
                                System.out.println(files.get(0) + " " +
                                        String.format(rb.getString(
                                        ".and.d.more."), files.size()-1));
                            } else {
                                System.out.println(files.get(0));
                            }
                        }
                        System.out.printf(key.substring(pipe+1));
                    }
                }
                System.out.println();
                System.out.println(rb.getString(
                    ".s.signature.was.verified."));
                System.out.println(rb.getString(
                    ".m.entry.is.listed.in.manifest"));
                System.out.println(rb.getString(
                    ".k.at.least.one.certificate.was.found.in.keystore"));
                System.out.println(rb.getString(
                    ".i.at.least.one.certificate.was.found.in.identity.scope"));
                if (ckaliases.size() > 0) {
                    System.out.println(rb.getString(
                        ".X.not.signed.by.specified.alias.es."));
                }
            }
            if (man == null) {
                System.out.println();
                System.out.println(rb.getString("no.manifest."));
            }

            // Even if the verbose option is not specified, all out strings
            // must be generated so seeWeak can be updated.
            if (!digestMap.isEmpty()
                    || !sigMap.isEmpty()
                    || !unparsableSignatures.isEmpty()) {
                if (verbose != null) {
                System.out.println();
            }
                for (String s : sigMap.keySet()) {
                    if (!digestMap.containsKey(s)) {
                        unparsableSignatures.putIfAbsent(s, String.format(
                                rb.getString("history.nosf"), s));
                    }
                }
                for (String s : digestMap.keySet()) {
                    PKCS7 p7 = sigMap.get(s);
                    if (p7 != null) {
                        String history;
                        try {
                            SignerInfo si = p7.getSignerInfos()[0];
                            X509Certificate signer = si.getCertificate(p7);
                            String digestAlg = digestMap.get(s);
                            String sigAlg = AlgorithmId.makeSigAlg(
                                    si.getDigestAlgorithmId().getName(),
                                    si.getDigestEncryptionAlgorithmId().getName());
                            PublicKey key = signer.getPublicKey();
                            PKCS7 tsToken = si.getTsToken();
                            if (tsToken != null) {
                                hasTimestampBlock = true;
                                SignerInfo tsSi = tsToken.getSignerInfos()[0];
                                X509Certificate tsSigner = tsSi.getCertificate(tsToken);
                                byte[] encTsTokenInfo = tsToken.getContentInfo().getData();
                                TimestampToken tsTokenInfo = new TimestampToken(encTsTokenInfo);
                                PublicKey tsKey = tsSigner.getPublicKey();
                                String tsDigestAlg = tsTokenInfo.getHashAlgorithm().getName();
                                String tsSigAlg = AlgorithmId.makeSigAlg(
                                        tsSi.getDigestAlgorithmId().getName(),
                                        tsSi.getDigestEncryptionAlgorithmId().getName());
                                Calendar c = Calendar.getInstance(
                                        TimeZone.getTimeZone("UTC"),
                                        Locale.getDefault(Locale.Category.FORMAT));
                                c.setTime(tsTokenInfo.getDate());
                                history = String.format(
                                        rb.getString("history.with.ts"),
                                        signer.getSubjectX500Principal(),
                                        withWeak(digestAlg, DIGEST_PRIMITIVE_SET),
                                        withWeak(sigAlg, SIG_PRIMITIVE_SET),
                                        withWeak(key),
                                        c,
                                        tsSigner.getSubjectX500Principal(),
                                        withWeak(tsDigestAlg, DIGEST_PRIMITIVE_SET),
                                        withWeak(tsSigAlg, SIG_PRIMITIVE_SET),
                                        withWeak(tsKey));
                            } else {
                                history = String.format(
                                        rb.getString("history.without.ts"),
                                        signer.getSubjectX500Principal(),
                                        withWeak(digestAlg, DIGEST_PRIMITIVE_SET),
                                        withWeak(sigAlg, SIG_PRIMITIVE_SET),
                                        withWeak(key));
                            }
                        } catch (Exception e) {
                            // The only usage of sigNameMap, remember the name
                            // of the block file if it's invalid.
                            history = String.format(
                                    rb.getString("history.unparsable"),
                                    sigNameMap.get(s));
                        }
                        if (verbose != null) {
                            System.out.println(history);
                        }
                    } else {
                        unparsableSignatures.putIfAbsent(s, String.format(
                                rb.getString("history.nobk"), s));
                    }
                }
                if (verbose != null) {
                    for (String s : unparsableSignatures.keySet()) {
                        System.out.println(unparsableSignatures.get(s));
                    }
                }
            }
            System.out.println();

            // If signer is a trusted cert or private entry in user's own
            // keystore, it can be self-signed. Please note aliasNotInStore
            // is always false when ~/.keystore is used.
            if (!aliasNotInStore && keystore != null) {
                signerSelfSigned = false;
            }

            if (!anySigned) {
                if (seeWeak) {
                    if (verbose != null) {
                        System.out.println(rb.getString("jar.treated.unsigned.see.weak.verbose"));
                        System.out.println("\n  " +
                                DisabledAlgorithmConstraints.PROPERTY_JAR_DISABLED_ALGS +
                                "=" + Security.getProperty(DisabledAlgorithmConstraints.PROPERTY_JAR_DISABLED_ALGS));
                    } else {
                        System.out.println(rb.getString("jar.treated.unsigned.see.weak"));
                    }
                } else if (hasSignature) {
                    System.out.println(rb.getString("jar.treated.unsigned"));
                } else {
                    System.out.println(rb.getString("jar.is.unsigned"));
                }
            } else {
                displayMessagesAndResult(false);
            }
            return;
        } catch (Exception e) {
            System.out.println(rb.getString("jarsigner.") + e);
            if (debug) {
                e.printStackTrace();
            }
        } finally { // close the resource
            if (jf != null) {
                jf.close();
            }
        }

        System.exit(1);
    }

    private void displayMessagesAndResult(boolean isSigning) {
        String result;
        List<String> errors = new ArrayList<>();
        List<String> warnings = new ArrayList<>();
        List<String> info = new ArrayList<>();

        boolean signerNotExpired = expireDate == null
                || expireDate.after(new Date());

        if (badKeyUsage || badExtendedKeyUsage || badNetscapeCertType ||
                notYetValidCert || chainNotValidated || hasExpiredCert ||
                hasUnsignedEntry || signerSelfSigned || (weakAlg != 0) ||
                aliasNotInStore || notSignedByAlias ||
                tsaChainNotValidated ||
                (hasExpiredTsaCert && !signerNotExpired)) {

            if (strict) {
                result = rb.getString(isSigning
                        ? "jar.signed.with.signer.errors."
                        : "jar.verified.with.signer.errors.");
            } else {
                result = rb.getString(isSigning
                        ? "jar.signed."
                        : "jar.verified.");
            }

            if (badKeyUsage) {
                errors.add(rb.getString(isSigning
                        ? "The.signer.certificate.s.KeyUsage.extension.doesn.t.allow.code.signing."
                        : "This.jar.contains.entries.whose.signer.certificate.s.KeyUsage.extension.doesn.t.allow.code.signing."));
            }

            if (badExtendedKeyUsage) {
                errors.add(rb.getString(isSigning
                        ? "The.signer.certificate.s.ExtendedKeyUsage.extension.doesn.t.allow.code.signing."
                        : "This.jar.contains.entries.whose.signer.certificate.s.ExtendedKeyUsage.extension.doesn.t.allow.code.signing."));
            }

            if (badNetscapeCertType) {
                errors.add(rb.getString(isSigning
                        ? "The.signer.certificate.s.NetscapeCertType.extension.doesn.t.allow.code.signing."
                        : "This.jar.contains.entries.whose.signer.certificate.s.NetscapeCertType.extension.doesn.t.allow.code.signing."));
            }

            // only in verifying
            if (hasUnsignedEntry) {
                errors.add(rb.getString(
                        "This.jar.contains.unsigned.entries.which.have.not.been.integrity.checked."));
            }
            if (hasExpiredCert) {
                errors.add(rb.getString(isSigning
                        ? "The.signer.certificate.has.expired."
                        : "This.jar.contains.entries.whose.signer.certificate.has.expired."));
            }
            if (notYetValidCert) {
                errors.add(rb.getString(isSigning
                        ? "The.signer.certificate.is.not.yet.valid."
                        : "This.jar.contains.entries.whose.signer.certificate.is.not.yet.valid."));
            }

            if (chainNotValidated) {
                errors.add(String.format(rb.getString(isSigning
                                ? "The.signer.s.certificate.chain.is.invalid.reason.1"
                                : "This.jar.contains.entries.whose.certificate.chain.is.invalid.reason.1"),
                        chainNotValidatedReason.getLocalizedMessage()));
            }

            if (hasExpiredTsaCert) {
                errors.add(rb.getString("The.timestamp.has.expired."));
            }
            if (tsaChainNotValidated) {
                errors.add(String.format(rb.getString(isSigning
                                ? "The.tsa.certificate.chain.is.invalid.reason.1"
                                : "This.jar.contains.entries.whose.tsa.certificate.chain.is.invalid.reason.1"),
                        tsaChainNotValidatedReason.getLocalizedMessage()));
            }

            // only in verifying
            if (notSignedByAlias) {
                errors.add(
                        rb.getString("This.jar.contains.signed.entries.which.is.not.signed.by.the.specified.alias.es."));
            }

            // only in verifying
            if (aliasNotInStore) {
                errors.add(rb.getString("This.jar.contains.signed.entries.that.s.not.signed.by.alias.in.this.keystore."));
            }

            if (signerSelfSigned) {
                errors.add(rb.getString(isSigning
                        ? "The.signer.s.certificate.is.self.signed."
                        : "This.jar.contains.entries.whose.signer.certificate.is.self.signed."));
            }

            // weakAlg only detected in signing. The jar file is
            // now simply treated unsigned in verifying.
            if ((weakAlg & 1) == 1) {
                errors.add(String.format(
                        rb.getString("The.1.algorithm.specified.for.the.2.option.is.considered.a.security.risk."),
                        digestalg, "-digestalg"));
            }

            if ((weakAlg & 2) == 2) {
                errors.add(String.format(
                        rb.getString("The.1.algorithm.specified.for.the.2.option.is.considered.a.security.risk."),
                        sigalg, "-sigalg"));
            }
            if ((weakAlg & 4) == 4) {
                errors.add(String.format(
                        rb.getString("The.1.algorithm.specified.for.the.2.option.is.considered.a.security.risk."),
                        tSADigestAlg, "-tsadigestalg"));
            }
            if ((weakAlg & 8) == 8) {
                errors.add(String.format(
                        rb.getString("The.1.signing.key.has.a.keysize.of.2.which.is.considered.a.security.risk."),
                        privateKey.getAlgorithm(), KeyUtil.getKeySize(privateKey)));
            }
        } else {
            result = rb.getString(isSigning ? "jar.signed." : "jar.verified.");
        }

        if (hasExpiredTsaCert) {
            // No need to warn about expiring if already expired
            hasExpiringTsaCert = false;
        }

        if (hasExpiringCert ||
                (hasExpiringTsaCert  && expireDate != null) ||
                (noTimestamp && expireDate != null) ||
                (hasExpiredTsaCert && signerNotExpired)) {

            if (hasExpiredTsaCert && signerNotExpired) {
                if (expireDate != null) {
                    warnings.add(String.format(
                            rb.getString("The.timestamp.expired.1.but.usable.2"),
                            tsaExpireDate,
                            expireDate));
                }
                // Reset the flag so exit code is 0
                hasExpiredTsaCert = false;
            }
            if (hasExpiringCert) {
                warnings.add(rb.getString(isSigning
                        ? "The.signer.certificate.will.expire.within.six.months."
                        : "This.jar.contains.entries.whose.signer.certificate.will.expire.within.six.months."));
            }
            if (hasExpiringTsaCert && expireDate != null) {
                if (expireDate.after(tsaExpireDate)) {
                    warnings.add(String.format(rb.getString(
                            "The.timestamp.will.expire.within.one.year.on.1.but.2"), tsaExpireDate, expireDate));
                } else {
                    warnings.add(String.format(rb.getString(
                            "The.timestamp.will.expire.within.one.year.on.1"), tsaExpireDate));
                }
            }
            if (noTimestamp && expireDate != null) {
                if (hasTimestampBlock) {
                    warnings.add(String.format(rb.getString(isSigning
                            ? "invalid.timestamp.signing"
                            : "bad.timestamp.verifying"), expireDate));
                } else {
                    warnings.add(String.format(rb.getString(isSigning
                            ? "no.timestamp.signing"
                            : "no.timestamp.verifying"), expireDate));
                }
            }
        }

        System.out.println(result);
        if (strict) {
            if (!errors.isEmpty()) {
                System.out.println();
                System.out.println(rb.getString("Error."));
                errors.forEach(System.out::println);
            }
            if (!warnings.isEmpty()) {
                System.out.println();
                System.out.println(rb.getString("Warning."));
                warnings.forEach(System.out::println);
            }
        } else {
            if (!errors.isEmpty() || !warnings.isEmpty()) {
                System.out.println();
                System.out.println(rb.getString("Warning."));
                errors.forEach(System.out::println);
                warnings.forEach(System.out::println);
            }
        }
        if (!isSigning && (!errors.isEmpty() || !warnings.isEmpty())) {
            if (! (verbose != null && showcerts)) {
                System.out.println();
                System.out.println(rb.getString(
                        "Re.run.with.the.verbose.and.certs.options.for.more.details."));
            }
        }

        if (isSigning || verbose != null) {
            // Always print out expireDate, unless expired or expiring.
            if (!hasExpiringCert && !hasExpiredCert
                    && expireDate != null && signerNotExpired) {
                info.add(String.format(rb.getString(
                        "The.signer.certificate.will.expire.on.1."), expireDate));
            }
            if (!noTimestamp) {
                if (!hasExpiringTsaCert && !hasExpiredTsaCert && tsaExpireDate != null) {
                    if (signerNotExpired) {
                        info.add(String.format(rb.getString(
                                "The.timestamp.will.expire.on.1."), tsaExpireDate));
                    } else {
                        info.add(String.format(rb.getString(
                                "signer.cert.expired.1.but.timestamp.good.2."),
                                expireDate,
                                tsaExpireDate));
                    }
                }
            }
        }

        if (!info.isEmpty()) {
            System.out.println();
            info.forEach(System.out::println);
        }
    }

    private String withWeak(String alg, Set<CryptoPrimitive> primitiveSet) {
        if (DISABLED_CHECK.permits(primitiveSet, alg, null)) {
            return alg;
        } else {
            seeWeak = true;
            return String.format(rb.getString("with.weak"), alg);
        }
    }

    private String withWeak(PublicKey key) {
        if (DISABLED_CHECK.permits(SIG_PRIMITIVE_SET, key)) {
            return String.format(
                    rb.getString("key.bit"), KeyUtil.getKeySize(key));
        } else {
            seeWeak = true;
            return String.format(
                    rb.getString("key.bit.weak"), KeyUtil.getKeySize(key));
        }
    }

    private static MessageFormat validityTimeForm = null;
    private static MessageFormat notYetTimeForm = null;
    private static MessageFormat expiredTimeForm = null;
    private static MessageFormat expiringTimeForm = null;

    /**
     * Returns a string about a certificate:
     *
     * [<tab>] <cert-type> [", " <subject-DN>] [" (" <keystore-entry-alias> ")"]
     * [<validity-period> | <expiry-warning>]
     * [<key-usage-warning>]
     *
     * Note: no newline character at the end.
     *
     * This method sets global flags like hasExpiringCert, hasExpiredCert,
     * notYetValidCert, badKeyUsage, badExtendedKeyUsage, badNetscapeCertType,
     * hasExpiringTsaCert, hasExpiredTsaCert.
     *
     * @param isTsCert true if c is in the TSA cert chain, false otherwise.
     * @param checkUsage true to check code signer keyUsage
     */
    String printCert(boolean isTsCert, String tab, Certificate c,
        Date timestamp, boolean checkUsage) throws Exception {

        StringBuilder certStr = new StringBuilder();
        String space = rb.getString("SPACE");
        X509Certificate x509Cert = null;

        if (c instanceof X509Certificate) {
            x509Cert = (X509Certificate) c;
            certStr.append(tab).append(x509Cert.getType())
                .append(rb.getString("COMMA"))
                .append(x509Cert.getSubjectDN().getName());
        } else {
            certStr.append(tab).append(c.getType());
        }

        String alias = storeHash.get(c);
        if (alias != null) {
            certStr.append(space).append(alias);
        }

        if (x509Cert != null) {

            certStr.append("\n").append(tab).append("[");

            if (trustedCerts.contains(x509Cert)) {
                certStr.append(rb.getString("trusted.certificate"));
            } else {
                Date notAfter = x509Cert.getNotAfter();
                try {
                    boolean printValidity = true;
                    if (isTsCert) {
                        if (tsaExpireDate == null || tsaExpireDate.after(notAfter)) {
                            tsaExpireDate = notAfter;
                        }
                    } else {
                        if (expireDate == null || expireDate.after(notAfter)) {
                            expireDate = notAfter;
                        }
                    }
                    if (timestamp == null) {
                        x509Cert.checkValidity();
                        // test if cert will expire within six months (or one year for tsa)
                        long age = isTsCert ? ONE_YEAR : SIX_MONTHS;
                        if (notAfter.getTime() < System.currentTimeMillis() + age) {
                            if (isTsCert) {
                                hasExpiringTsaCert = true;
                            } else {
                                hasExpiringCert = true;
                            }
                            if (expiringTimeForm == null) {
                                expiringTimeForm = new MessageFormat(
                                        rb.getString("certificate.will.expire.on"));
                            }
                            Object[] source = {notAfter};
                            certStr.append(expiringTimeForm.format(source));
                            printValidity = false;
                        }
                    } else {
                        x509Cert.checkValidity(timestamp);
                    }
                    if (printValidity) {
                        if (validityTimeForm == null) {
                            validityTimeForm = new MessageFormat(
                                    rb.getString("certificate.is.valid.from"));
                        }
                        Object[] source = {x509Cert.getNotBefore(), notAfter};
                        certStr.append(validityTimeForm.format(source));
                    }
                } catch (CertificateExpiredException cee) {
                    if (isTsCert) {
                        hasExpiredTsaCert = true;
                    } else {
                        hasExpiredCert = true;
                    }

                    if (expiredTimeForm == null) {
                        expiredTimeForm = new MessageFormat(
                                rb.getString("certificate.expired.on"));
                    }
                    Object[] source = {notAfter};
                    certStr.append(expiredTimeForm.format(source));

                } catch (CertificateNotYetValidException cnyve) {
                    if (!isTsCert) notYetValidCert = true;

                    if (notYetTimeForm == null) {
                        notYetTimeForm = new MessageFormat(
                                rb.getString("certificate.is.not.valid.until"));
                    }
                    Object[] source = {x509Cert.getNotBefore()};
                    certStr.append(notYetTimeForm.format(source));
                }
            }
            certStr.append("]");

            if (checkUsage) {
                boolean[] bad = new boolean[3];
                checkCertUsage(x509Cert, bad);
                if (bad[0] || bad[1] || bad[2]) {
                    String x = "";
                    if (bad[0]) {
                        x ="KeyUsage";
                    }
                    if (bad[1]) {
                        if (x.length() > 0) x = x + ", ";
                        x = x + "ExtendedKeyUsage";
                    }
                    if (bad[2]) {
                        if (x.length() > 0) x = x + ", ";
                        x = x + "NetscapeCertType";
                    }
                    certStr.append("\n").append(tab)
                        .append(MessageFormat.format(rb.getString(
                        ".{0}.extension.does.not.support.code.signing."), x));
                }
            }
        }
        return certStr.toString();
    }

    private static MessageFormat signTimeForm = null;

    private String printTimestamp(String tab, Timestamp timestamp) {

        if (signTimeForm == null) {
            signTimeForm =
                new MessageFormat(rb.getString("entry.was.signed.on"));
        }
        Object[] source = { timestamp.getTimestamp() };

        return new StringBuilder().append(tab).append("[")
            .append(signTimeForm.format(source)).append("]").toString();
    }

    private Map<CodeSigner,Integer> cacheForInKS = new IdentityHashMap<>();

    private int inKeyStoreForOneSigner(CodeSigner signer) {
        if (cacheForInKS.containsKey(signer)) {
            return cacheForInKS.get(signer);
        }

        boolean found = false;
        int result = 0;
        List<? extends Certificate> certs = signer.getSignerCertPath().getCertificates();
        for (Certificate c : certs) {
            String alias = storeHash.get(c);
            if (alias != null) {
                if (alias.startsWith("(")) {
                    result |= IN_KEYSTORE;
                } else if (alias.startsWith("[")) {
                    result |= IN_SCOPE;
                }
                if (ckaliases.contains(alias.substring(1, alias.length() - 1))) {
                    result |= SIGNED_BY_ALIAS;
                }
            } else {
                if (store != null) {
                    try {
                        alias = store.getCertificateAlias(c);
                    } catch (KeyStoreException kse) {
                        // never happens, because keystore has been loaded
                    }
                    if (alias != null) {
                        storeHash.put(c, "(" + alias + ")");
                        found = true;
                        result |= IN_KEYSTORE;
                    }
                }
                if (ckaliases.contains(alias)) {
                    result |= SIGNED_BY_ALIAS;
                }
            }
        }
        cacheForInKS.put(signer, result);
        return result;
    }

    Hashtable<Certificate, String> storeHash = new Hashtable<>();

    int inKeyStore(CodeSigner[] signers) {

        if (signers == null)
            return 0;

        int output = 0;

        for (CodeSigner signer: signers) {
            int result = inKeyStoreForOneSigner(signer);
            output |= result;
        }
        if (ckaliases.size() > 0 && (output & SIGNED_BY_ALIAS) == 0) {
            output |= NOT_ALIAS;
        }
        return output;
    }

    void signJar(String jarName, String alias, String[] args)
            throws Exception {

        DisabledAlgorithmConstraints dac =
                new DisabledAlgorithmConstraints(
                        DisabledAlgorithmConstraints.PROPERTY_CERTPATH_DISABLED_ALGS);

        if (digestalg != null && !dac.permits(
                Collections.singleton(CryptoPrimitive.MESSAGE_DIGEST), digestalg, null)) {
            weakAlg |= 1;
        }
        if (tSADigestAlg != null && !dac.permits(
                Collections.singleton(CryptoPrimitive.MESSAGE_DIGEST), tSADigestAlg, null)) {
            weakAlg |= 4;
        }
        if (sigalg != null && !dac.permits(
                Collections.singleton(CryptoPrimitive.SIGNATURE), sigalg, null)) {
            weakAlg |= 2;
        }

        boolean aliasUsed = false;
        X509Certificate tsaCert = null;

        if (sigfile == null) {
            sigfile = alias;
            aliasUsed = true;
        }

        if (sigfile.length() > 8) {
            sigfile = sigfile.substring(0, 8).toUpperCase(Locale.ENGLISH);
        } else {
            sigfile = sigfile.toUpperCase(Locale.ENGLISH);
        }

        StringBuilder tmpSigFile = new StringBuilder(sigfile.length());
        for (int j = 0; j < sigfile.length(); j++) {
            char c = sigfile.charAt(j);
            if (!
                ((c>= 'A' && c<= 'Z') ||
                (c>= '0' && c<= '9') ||
                (c == '-') ||
                (c == '_'))) {
                if (aliasUsed) {
                    // convert illegal characters from the alias to be _'s
                    c = '_';
                } else {
                 throw new
                   RuntimeException(rb.getString
                        ("signature.filename.must.consist.of.the.following.characters.A.Z.0.9.or."));
                }
            }
            tmpSigFile.append(c);
        }

        sigfile = tmpSigFile.toString();

        String tmpJarName;
        if (signedjar == null) tmpJarName = jarName+".sig";
        else tmpJarName = signedjar;

        File jarFile = new File(jarName);
        File signedJarFile = new File(tmpJarName);

        // Open the jar (zip) file
        try {
            zipFile = new ZipFile(jarName);
        } catch (IOException ioe) {
            error(rb.getString("unable.to.open.jar.file.")+jarName, ioe);
        }

        FileOutputStream fos = null;
        try {
            fos = new FileOutputStream(signedJarFile);
        } catch (IOException ioe) {
            error(rb.getString("unable.to.create.")+tmpJarName, ioe);
        }

        PrintStream ps = new PrintStream(fos);
        ZipOutputStream zos = new ZipOutputStream(ps);

        /* First guess at what they might be - we don't xclude RSA ones. */
        String sfFilename = (META_INF + sigfile + ".SF").toUpperCase(Locale.ENGLISH);
        String bkFilename = (META_INF + sigfile + ".DSA").toUpperCase(Locale.ENGLISH);

        Manifest manifest = new Manifest();
        Map<String,Attributes> mfEntries = manifest.getEntries();

        // The Attributes of manifest before updating
        Attributes oldAttr = null;

        boolean mfModified = false;
        boolean mfCreated = false;
        byte[] mfRawBytes = null;

        try {
            MessageDigest digests[] = { MessageDigest.getInstance(digestalg) };

            // Check if manifest exists
            ZipEntry mfFile;
            if ((mfFile = getManifestFile(zipFile)) != null) {
                // Manifest exists. Read its raw bytes.
                mfRawBytes = getBytes(zipFile, mfFile);
                manifest.read(new ByteArrayInputStream(mfRawBytes));
                oldAttr = (Attributes)(manifest.getMainAttributes().clone());
            } else {
                // Create new manifest
                Attributes mattr = manifest.getMainAttributes();
                mattr.putValue(Attributes.Name.MANIFEST_VERSION.toString(),
                               "1.0");
                String javaVendor = System.getProperty("java.vendor");
                String jdkVersion = System.getProperty("java.version");
                mattr.putValue("Created-By", jdkVersion + " (" +javaVendor
                               + ")");
                mfFile = new ZipEntry(JarFile.MANIFEST_NAME);
                mfCreated = true;
            }

            /*
             * For each entry in jar
             * (except for signature-related META-INF entries),
             * do the following:
             *
             * - if entry is not contained in manifest, add it to manifest;
             * - if entry is contained in manifest, calculate its hash and
             *   compare it with the one in the manifest; if they are
             *   different, replace the hash in the manifest with the newly
             *   generated one. (This may invalidate existing signatures!)
             */
            Vector<ZipEntry> mfFiles = new Vector<>();

            boolean wasSigned = false;

            for (Enumeration<? extends ZipEntry> enum_=zipFile.entries();
                        enum_.hasMoreElements();) {
                ZipEntry ze = enum_.nextElement();

                if (ze.getName().startsWith(META_INF)) {
                    // Store META-INF files in vector, so they can be written
                    // out first
                    mfFiles.addElement(ze);

                    if (SignatureFileVerifier.isBlockOrSF(
                            ze.getName().toUpperCase(Locale.ENGLISH))) {
                        wasSigned = true;
                    }

                    if (signatureRelated(ze.getName())) {
                        // ignore signature-related and manifest files
                        continue;
                    }
                }

                if (manifest.getAttributes(ze.getName()) != null) {
                    // jar entry is contained in manifest, check and
                    // possibly update its digest attributes
                    if (updateDigests(ze, zipFile, digests,
                                      manifest) == true) {
                        mfModified = true;
                    }
                } else if (!ze.isDirectory()) {
                    // Add entry to manifest
                    Attributes attrs = getDigestAttributes(ze, zipFile,
                                                           digests);
                    mfEntries.put(ze.getName(), attrs);
                    mfModified = true;
                }
            }

            // Recalculate the manifest raw bytes if necessary
            if (mfModified) {
                ByteArrayOutputStream baos = new ByteArrayOutputStream();
                manifest.write(baos);
                if (wasSigned) {
                    byte[] newBytes = baos.toByteArray();
                    if (mfRawBytes != null
                            && oldAttr.equals(manifest.getMainAttributes())) {

                        /*
                         * Note:
                         *
                         * The Attributes object is based on HashMap and can handle
                         * continuation columns. Therefore, even if the contents are
                         * not changed (in a Map view), the bytes that it write()
                         * may be different from the original bytes that it read()
                         * from. Since the signature on the main attributes is based
                         * on raw bytes, we must retain the exact bytes.
                         */

                        int newPos = findHeaderEnd(newBytes);
                        int oldPos = findHeaderEnd(mfRawBytes);

                        if (newPos == oldPos) {
                            System.arraycopy(mfRawBytes, 0, newBytes, 0, oldPos);
                        } else {
                            // cat oldHead newTail > newBytes
                            byte[] lastBytes = new byte[oldPos +
                                    newBytes.length - newPos];
                            System.arraycopy(mfRawBytes, 0, lastBytes, 0, oldPos);
                            System.arraycopy(newBytes, newPos, lastBytes, oldPos,
                                    newBytes.length - newPos);
                            newBytes = lastBytes;
                        }
                    }
                    mfRawBytes = newBytes;
                } else {
                    mfRawBytes = baos.toByteArray();
                }
            }

            // Write out the manifest
            if (mfModified) {
                // manifest file has new length
                mfFile = new ZipEntry(JarFile.MANIFEST_NAME);
            }
            if (verbose != null) {
                if (mfCreated) {
                    System.out.println(rb.getString(".adding.") +
                                        mfFile.getName());
                } else if (mfModified) {
                    System.out.println(rb.getString(".updating.") +
                                        mfFile.getName());
                }
            }
            zos.putNextEntry(mfFile);
            zos.write(mfRawBytes);

            // Calculate SignatureFile (".SF") and SignatureBlockFile
            ManifestDigester manDig = new ManifestDigester(mfRawBytes);
            SignatureFile sf = new SignatureFile(digests, manifest, manDig,
                                                 sigfile, signManifest);

            if (tsaAlias != null) {
                tsaCert = getTsaCert(tsaAlias);
            }

            if (tsaUrl == null && tsaCert == null) {
                noTimestamp = true;
            }

            SignatureFile.Block block = null;

            try {
                block =
                    sf.generateBlock(privateKey, sigalg, certChain,
                        externalSF, tsaUrl, tsaCert, tSAPolicyID, tSADigestAlg,
                        signingMechanism, args, zipFile);
            } catch (SocketTimeoutException e) {
                // Provide a helpful message when TSA is beyond a firewall
                error(rb.getString("unable.to.sign.jar.") +
                rb.getString("no.response.from.the.Timestamping.Authority.") +
                "\n  -J-Dhttp.proxyHost=<hostname>" +
                "\n  -J-Dhttp.proxyPort=<portnumber>\n" +
                rb.getString("or") +
                "\n  -J-Dhttps.proxyHost=<hostname> " +
                "\n  -J-Dhttps.proxyPort=<portnumber> ", e);
            }

            sfFilename = sf.getMetaName();
            bkFilename = block.getMetaName();

            ZipEntry sfFile = new ZipEntry(sfFilename);
            ZipEntry bkFile = new ZipEntry(bkFilename);

            long time = System.currentTimeMillis();
            sfFile.setTime(time);
            bkFile.setTime(time);

            // signature file
            zos.putNextEntry(sfFile);
            sf.write(zos);
            if (verbose != null) {
                if (zipFile.getEntry(sfFilename) != null) {
                    System.out.println(rb.getString(".updating.") +
                                sfFilename);
                } else {
                    System.out.println(rb.getString(".adding.") +
                                sfFilename);
                }
            }

            if (verbose != null) {
                if (tsaUrl != null || tsaCert != null) {
                    System.out.println(
                        rb.getString("requesting.a.signature.timestamp"));
                }
                if (tsaUrl != null) {
                    System.out.println(rb.getString("TSA.location.") + tsaUrl);
                }
                if (tsaCert != null) {
                    URI tsaURI = TimestampedSigner.getTimestampingURI(tsaCert);
                    if (tsaURI != null) {
                        System.out.println(rb.getString("TSA.location.") +
                            tsaURI);
                    }
                    System.out.println(rb.getString("TSA.certificate.") +
                            printCert(true, "", tsaCert, null, false));
                }
                if (signingMechanism != null) {
                    System.out.println(
                        rb.getString("using.an.alternative.signing.mechanism"));
                }
            }

            // signature block file
            zos.putNextEntry(bkFile);
            block.write(zos);
            if (verbose != null) {
                if (zipFile.getEntry(bkFilename) != null) {
                    System.out.println(rb.getString(".updating.") +
                        bkFilename);
                } else {
                    System.out.println(rb.getString(".adding.") +
                        bkFilename);
                }
            }

            // Write out all other META-INF files that we stored in the
            // vector
            for (int i=0; i<mfFiles.size(); i++) {
                ZipEntry ze = mfFiles.elementAt(i);
                if (!ze.getName().equalsIgnoreCase(JarFile.MANIFEST_NAME)
                    && !ze.getName().equalsIgnoreCase(sfFilename)
                    && !ze.getName().equalsIgnoreCase(bkFilename)) {
                    writeEntry(zipFile, zos, ze);
                }
            }

            // Write out all other files
            for (Enumeration<? extends ZipEntry> enum_=zipFile.entries();
                        enum_.hasMoreElements();) {
                ZipEntry ze = enum_.nextElement();

                if (!ze.getName().startsWith(META_INF)) {
                    if (verbose != null) {
                        if (manifest.getAttributes(ze.getName()) != null)
                          System.out.println(rb.getString(".signing.") +
                                ze.getName());
                        else
                          System.out.println(rb.getString(".adding.") +
                                ze.getName());
                    }
                    writeEntry(zipFile, zos, ze);
                }
            }
        } catch(IOException ioe) {
            error(rb.getString("unable.to.sign.jar.")+ioe, ioe);
        } finally {
            // close the resouces
            if (zipFile != null) {
                zipFile.close();
                zipFile = null;
            }

            if (zos != null) {
                zos.close();
            }
        }

        // The JarSigner API always accepts the timestamp received.
        // We need to extract the certs from the signed jar to
        // validate it.
        try (JarFile check = new JarFile(signedJarFile)) {
            PKCS7 p7 = new PKCS7(check.getInputStream(check.getEntry(
                    "META-INF/" + sigfile + "." + privateKey.getAlgorithm())));
            Timestamp ts = null;
            try {
                SignerInfo si = p7.getSignerInfos()[0];
                if (si.getTsToken() != null) {
                    hasTimestampBlock = true;
                }
                ts = si.getTimestamp();
            } catch (Exception e) {
                tsaChainNotValidated = true;
                tsaChainNotValidatedReason = e;
            }
            // Spaces before the ">>> Signer" and other lines are different
            String result = certsAndTSInfo("", "    ", Arrays.asList(certChain), ts);
            if (verbose != null) {
                System.out.println(result);
            }
        } catch (Exception e) {
            if (debug) {
                e.printStackTrace();
            }
        }

        if (signedjar == null) {
            // attempt an atomic rename. If that fails,
            // rename the original jar file, then the signed
            // one, then delete the original.
            if (!signedJarFile.renameTo(jarFile)) {
                File origJar = new File(jarName+".orig");

                if (jarFile.renameTo(origJar)) {
                    if (signedJarFile.renameTo(jarFile)) {
                        origJar.delete();
                    } else {
                        MessageFormat form = new MessageFormat(rb.getString
                    ("attempt.to.rename.signedJarFile.to.jarFile.failed"));
                        Object[] source = {signedJarFile, jarFile};
                        error(form.format(source));
                    }
                } else {
                    MessageFormat form = new MessageFormat(rb.getString
                        ("attempt.to.rename.jarFile.to.origJar.failed"));
                    Object[] source = {jarFile, origJar};
                    error(form.format(source));
                }
            }
        }

        displayMessagesAndResult(true);
    }

    /**
     * Find the length of header inside bs. The header is a multiple (>=0)
     * lines of attributes plus an empty line. The empty line is included
     * in the header.
     */
    @SuppressWarnings("fallthrough")
    private int findHeaderEnd(byte[] bs) {
        // Initial state true to deal with empty header
        boolean newline = true;     // just met a newline
        int len = bs.length;
        for (int i=0; i<len; i++) {
            switch (bs[i]) {
                case '\r':
                    if (i < len - 1 && bs[i+1] == '\n') i++;
                    // fallthrough
                case '\n':
                    if (newline) return i+1;    //+1 to get length
                    newline = true;
                    break;
                default:
                    newline = false;
            }
        }
        // If header end is not found, it means the MANIFEST.MF has only
        // the main attributes section and it does not end with 2 newlines.
        // Returns the whole length so that it can be completely replaced.
        return len;
    }

    /**
     * signature-related files include:
     * . META-INF/MANIFEST.MF
     * . META-INF/SIG-*
     * . META-INF/*.SF
     * . META-INF/*.DSA
     * . META-INF/*.RSA
     * . META-INF/*.EC
     */
    private boolean signatureRelated(String name) {
        return SignatureFileVerifier.isSigningRelated(name);
    }

    Map<CodeSigner,String> cacheForSignerInfo = new IdentityHashMap<>();

    /**
     * Returns a string of signer info, with a newline at the end.
     * Called by verifyJar().
     */
    private String signerInfo(CodeSigner signer, String tab) throws Exception {
        if (cacheForSignerInfo.containsKey(signer)) {
            return cacheForSignerInfo.get(signer);
        }
        List<? extends Certificate> certs = signer.getSignerCertPath().getCertificates();
        // signing time is only displayed on verification
        Timestamp ts = signer.getTimestamp();
        String tsLine = "";
        if (ts != null) {
            tsLine = printTimestamp(tab, ts) + "\n";
        }
        // Spaces before the ">>> Signer" and other lines are the same.

        String result = certsAndTSInfo(tab, tab, certs, ts);
        cacheForSignerInfo.put(signer, tsLine + result);
        return result;
    }

    /**
     * Fills info on certs and timestamp into a StringBuilder, sets
     * warning flags (through printCert) and validates cert chains.
     *
     * @param tab1 spaces before the ">>> Signer" line
     * @param tab2 spaces before the other lines
     * @param certs the signer cert
     * @param ts the timestamp, can be null
     * @return the info as a string
     */
    private String certsAndTSInfo(
            String tab1,
            String tab2,
            List<? extends Certificate> certs, Timestamp ts)
            throws Exception {

        Date timestamp;
        if (ts != null) {
            timestamp = ts.getTimestamp();
            noTimestamp = false;
        } else {
            timestamp = null;
        }
        // display the certificate(s). The first one is end-entity cert and
        // its KeyUsage should be checked.
        boolean first = true;
        StringBuilder sb = new StringBuilder();
        sb.append(tab1).append(rb.getString("...Signer")).append('\n');
        for (Certificate c : certs) {
            sb.append(printCert(false, tab2, c, timestamp, first));
            sb.append('\n');
            first = false;
        }
        try {
            validateCertChain(Validator.VAR_CODE_SIGNING, certs, ts);
        } catch (Exception e) {
            chainNotValidated = true;
            chainNotValidatedReason = e;
            sb.append(tab2).append(rb.getString(".Invalid.certificate.chain."))
                    .append(e.getLocalizedMessage()).append("]\n");
        }
        if (ts != null) {
            sb.append(tab1).append(rb.getString("...TSA")).append('\n');
            for (Certificate c : ts.getSignerCertPath().getCertificates()) {
                sb.append(printCert(true, tab2, c, null, false));
                sb.append('\n');
            }
            try {
                validateCertChain(Validator.VAR_TSA_SERVER,
                        ts.getSignerCertPath().getCertificates(), null);
            } catch (Exception e) {
                tsaChainNotValidated = true;
                tsaChainNotValidatedReason = e;
                sb.append(tab2).append(rb.getString(".Invalid.TSA.certificate.chain."))
                        .append(e.getLocalizedMessage()).append("]\n");
            }
        }
        if (certs.size() == 1
                && KeyStoreUtil.isSelfSigned((X509Certificate)certs.get(0))) {
            signerSelfSigned = true;
        }

        return sb.toString();
    }

    private void writeEntry(ZipFile zf, ZipOutputStream os, ZipEntry ze)
    throws IOException
    {
        ZipEntry ze2 = new ZipEntry(ze.getName());
        ze2.setMethod(ze.getMethod());
        ze2.setTime(ze.getTime());
        ze2.setComment(ze.getComment());
        ze2.setExtra(ze.getExtra());
        if (ze.getMethod() == ZipEntry.STORED) {
            ze2.setSize(ze.getSize());
            ze2.setCrc(ze.getCrc());
        }
        os.putNextEntry(ze2);
        writeBytes(zf, ze, os);
    }

    /**
     * Writes all the bytes for a given entry to the specified output stream.
     */
    private synchronized void writeBytes
        (ZipFile zf, ZipEntry ze, ZipOutputStream os) throws IOException {
        int n;

        InputStream is = null;
        try {
            is = zf.getInputStream(ze);
            long left = ze.getSize();

            while((left > 0) && (n = is.read(buffer, 0, buffer.length)) != -1) {
                os.write(buffer, 0, n);
                left -= n;
            }
        } finally {
            if (is != null) {
                is.close();
            }
        }
    }

    void loadKeyStore(String keyStoreName, boolean prompt) {

        if (!nullStream && keyStoreName == null) {
            keyStoreName = System.getProperty("user.home") + File.separator
                + ".keystore";
        }

        try {
            try {
                KeyStore caks = KeyStoreUtil.getCacertsKeyStore();
                if (caks != null) {
                    Enumeration<String> aliases = caks.aliases();
                    while (aliases.hasMoreElements()) {
                        String a = aliases.nextElement();
                        try {
                            trustedCerts.add((X509Certificate)caks.getCertificate(a));
                        } catch (Exception e2) {
                            // ignore, when a SecretkeyEntry does not include a cert
                        }
                    }
                }
            } catch (Exception e) {
                // Ignore, if cacerts cannot be loaded
            }

            if (providerName == null) {
                store = KeyStore.getInstance(storetype);
            } else {
                store = KeyStore.getInstance(storetype, providerName);
            }

            // Get pass phrase
            // XXX need to disable echo; on UNIX, call getpass(char *prompt)Z
            // and on NT call ??
            if (token && storepass == null && !protectedPath
                    && !KeyStoreUtil.isWindowsKeyStore(storetype)) {
                storepass = getPass
                        (rb.getString("Enter.Passphrase.for.keystore."));
            } else if (!token && storepass == null && prompt) {
                storepass = getPass
                        (rb.getString("Enter.Passphrase.for.keystore."));
            }

            try {
                if (nullStream) {
                    store.load(null, storepass);
                } else {
                    keyStoreName = keyStoreName.replace(File.separatorChar, '/');
                    URL url = null;
                    try {
                        url = new URL(keyStoreName);
                    } catch (java.net.MalformedURLException e) {
                        // try as file
                        url = new File(keyStoreName).toURI().toURL();
                    }
                    InputStream is = null;
                    try {
                        is = url.openStream();
                        store.load(is, storepass);
                    } finally {
                        if (is != null) {
                            is.close();
                        }
                    }
                }
                Enumeration<String> aliases = store.aliases();
                while (aliases.hasMoreElements()) {
                    String a = aliases.nextElement();
                    try {
                        X509Certificate c = (X509Certificate)store.getCertificate(a);
                        // Only add TrustedCertificateEntry and self-signed
                        // PrivateKeyEntry
                        if (store.isCertificateEntry(a) ||
                                c.getSubjectDN().equals(c.getIssuerDN())) {
                            trustedCerts.add(c);
                        }
                    } catch (Exception e2) {
                        // ignore, when a SecretkeyEntry does not include a cert
                    }
                }
            } finally {
                try {
                    pkixParameters = new PKIXBuilderParameters(
                            trustedCerts.stream()
                                    .map(c -> new TrustAnchor(c, null))
                                    .collect(Collectors.toSet()),
                            null);
                    pkixParameters.setRevocationEnabled(false);
                } catch (InvalidAlgorithmParameterException ex) {
                    // Only if tas is empty
                }
            }
        } catch (IOException ioe) {
            throw new RuntimeException(rb.getString("keystore.load.") +
                                        ioe.getMessage());
        } catch (java.security.cert.CertificateException ce) {
            throw new RuntimeException(rb.getString("certificate.exception.") +
                                        ce.getMessage());
        } catch (NoSuchProviderException pe) {
            throw new RuntimeException(rb.getString("keystore.load.") +
                                        pe.getMessage());
        } catch (NoSuchAlgorithmException nsae) {
            throw new RuntimeException(rb.getString("keystore.load.") +
                                        nsae.getMessage());
        } catch (KeyStoreException kse) {
            throw new RuntimeException
                (rb.getString("unable.to.instantiate.keystore.class.") +
                kse.getMessage());
        }
    }

    X509Certificate getTsaCert(String alias) {

        java.security.cert.Certificate cs = null;

        try {
            cs = store.getCertificate(alias);
        } catch (KeyStoreException kse) {
            // this never happens, because keystore has been loaded
        }
        if (cs == null || (!(cs instanceof X509Certificate))) {
            MessageFormat form = new MessageFormat(rb.getString
                ("Certificate.not.found.for.alias.alias.must.reference.a.valid.KeyStore.entry.containing.an.X.509.public.key.certificate.for.the"));
            Object[] source = {alias, alias};
            error(form.format(source));
        }
        return (X509Certificate) cs;
    }

    /**
     * Check if userCert is designed to be a code signer
     * @param userCert the certificate to be examined
     * @param bad 3 booleans to show if the KeyUsage, ExtendedKeyUsage,
     *            NetscapeCertType has codeSigning flag turned on.
     *            If null, the class field badKeyUsage, badExtendedKeyUsage,
     *            badNetscapeCertType will be set.
     */
    void checkCertUsage(X509Certificate userCert, boolean[] bad) {

        // Can act as a signer?
        // 1. if KeyUsage, then [0:digitalSignature] or
        //    [1:nonRepudiation] should be true
        // 2. if ExtendedKeyUsage, then should contains ANY or CODE_SIGNING
        // 3. if NetscapeCertType, then should contains OBJECT_SIGNING
        // 1,2,3 must be true

        if (bad != null) {
            bad[0] = bad[1] = bad[2] = false;
        }

        boolean[] keyUsage = userCert.getKeyUsage();
        if (keyUsage != null) {
            keyUsage = Arrays.copyOf(keyUsage, 9);
            if (!keyUsage[0] && !keyUsage[1]) {
                if (bad != null) {
                    bad[0] = true;
                    badKeyUsage = true;
                }
            }
        }

        try {
            List<String> xKeyUsage = userCert.getExtendedKeyUsage();
            if (xKeyUsage != null) {
                if (!xKeyUsage.contains("2.5.29.37.0") // anyExtendedKeyUsage
                        && !xKeyUsage.contains("1.3.6.1.5.5.7.3.3")) {  // codeSigning
                    if (bad != null) {
                        bad[1] = true;
                        badExtendedKeyUsage = true;
                    }
                }
            }
        } catch (java.security.cert.CertificateParsingException e) {
            // shouldn't happen
        }

        try {
            // OID_NETSCAPE_CERT_TYPE
            byte[] netscapeEx = userCert.getExtensionValue
                    ("2.16.840.1.113730.1.1");
            if (netscapeEx != null) {
                DerInputStream in = new DerInputStream(netscapeEx);
                byte[] encoded = in.getOctetString();
                encoded = new DerValue(encoded).getUnalignedBitString()
                        .toByteArray();

                NetscapeCertTypeExtension extn =
                        new NetscapeCertTypeExtension(encoded);

                Boolean val = extn.get(NetscapeCertTypeExtension.OBJECT_SIGNING);
                if (!val) {
                    if (bad != null) {
                        bad[2] = true;
                        badNetscapeCertType = true;
                    }
                }
            }
        } catch (IOException e) {
            //
        }
    }

    // Called by signJar().
    void getAliasInfo(String alias) throws Exception {

        Key key = null;

        try {
            java.security.cert.Certificate[] cs = null;
            if (altCertChain != null) {
                try (FileInputStream fis = new FileInputStream(altCertChain)) {
                    cs = CertificateFactory.getInstance("X.509").
                            generateCertificates(fis).
                            toArray(new Certificate[0]);
                } catch (FileNotFoundException ex) {
                    error(rb.getString("File.specified.by.certchain.does.not.exist"));
                } catch (CertificateException | IOException ex) {
                    error(rb.getString("Cannot.restore.certchain.from.file.specified"));
                }
            } else {
                try {
                    cs = store.getCertificateChain(alias);
                } catch (KeyStoreException kse) {
                    // this never happens, because keystore has been loaded
                }
            }
            if (cs == null || cs.length == 0) {
                if (altCertChain != null) {
                    error(rb.getString
                            ("Certificate.chain.not.found.in.the.file.specified."));
                } else {
                    MessageFormat form = new MessageFormat(rb.getString
                        ("Certificate.chain.not.found.for.alias.alias.must.reference.a.valid.KeyStore.key.entry.containing.a.private.key.and"));
                    Object[] source = {alias, alias};
                    error(form.format(source));
                }
            }

            certChain = new X509Certificate[cs.length];
            for (int i=0; i<cs.length; i++) {
                if (!(cs[i] instanceof X509Certificate)) {
                    error(rb.getString
                        ("found.non.X.509.certificate.in.signer.s.chain"));
                }
                certChain[i] = (X509Certificate)cs[i];
            }

            try {
                if (!token && keypass == null)
                    key = store.getKey(alias, storepass);
                else
                    key = store.getKey(alias, keypass);
            } catch (UnrecoverableKeyException e) {
                if (token) {
                    throw e;
                } else if (keypass == null) {
                    // Did not work out, so prompt user for key password
                    MessageFormat form = new MessageFormat(rb.getString
                        ("Enter.key.password.for.alias."));
                    Object[] source = {alias};
                    keypass = getPass(form.format(source));
                    key = store.getKey(alias, keypass);
                }
            }
        } catch (NoSuchAlgorithmException e) {
            error(e.getMessage());
        } catch (UnrecoverableKeyException e) {
            error(rb.getString("unable.to.recover.key.from.keystore"));
        } catch (KeyStoreException kse) {
            // this never happens, because keystore has been loaded
        }

        if (!(key instanceof PrivateKey)) {
            MessageFormat form = new MessageFormat(rb.getString
                ("key.associated.with.alias.not.a.private.key"));
            Object[] source = {alias};
            error(form.format(source));
        } else {
            privateKey = (PrivateKey)key;
        }
    }

    void error(String message)
    {
        System.out.println(rb.getString("jarsigner.")+message);
        System.exit(1);
    }


    void error(String message, Exception e)
    {
        System.out.println(rb.getString("jarsigner.")+message);
        if (debug) {
            e.printStackTrace();
        }
        System.exit(1);
    }

    /**
     * Validates a cert chain.
     *
     * @param parameter this might be a timestamp
     */
    void validateCertChain(String variant, List<? extends Certificate> certs,
                           Timestamp parameter)
            throws Exception {
        try {
            Validator.getInstance(Validator.TYPE_PKIX,
                    variant,
                    pkixParameters)
                    .validate(certs.toArray(new X509Certificate[certs.size()]),
                            null, parameter);
        } catch (Exception e) {
            if (debug) {
                e.printStackTrace();
            }

            // Exception might be dismissed if another warning flag
            // is already set by printCert.

            if (variant.equals(Validator.VAR_TSA_SERVER) &&
                    e instanceof ValidatorException) {
                // Throw cause if it's CertPathValidatorException,
                if (e.getCause() != null &&
                        e.getCause() instanceof CertPathValidatorException) {
                    e = (Exception) e.getCause();
                    Throwable t = e.getCause();
                    if ((t instanceof CertificateExpiredException &&
                            hasExpiredTsaCert)) {
                        // we already have hasExpiredTsaCert
                        return;
                    }
                }
            }

            if (variant.equals(Validator.VAR_CODE_SIGNING) &&
                    e instanceof ValidatorException) {
                // Throw cause if it's CertPathValidatorException,
                if (e.getCause() != null &&
                        e.getCause() instanceof CertPathValidatorException) {
                    e = (Exception) e.getCause();
                    Throwable t = e.getCause();
                    if ((t instanceof CertificateExpiredException &&
                                hasExpiredCert) ||
                            (t instanceof CertificateNotYetValidException &&
                                    notYetValidCert)) {
                        // we already have hasExpiredCert and notYetValidCert
                        return;
                    }
                }
                if (e instanceof ValidatorException) {
                    ValidatorException ve = (ValidatorException)e;
                    if (ve.getErrorType() == ValidatorException.T_EE_EXTENSIONS &&
                            (badKeyUsage || badExtendedKeyUsage || badNetscapeCertType)) {
                        // We already have badKeyUsage, badExtendedKeyUsage
                        // and badNetscapeCertType
                        return;
                    }
                }
            }
            throw e;
        }
    }

    char[] getPass(String prompt)
    {
        System.err.print(prompt);
        System.err.flush();
        try {
            char[] pass = Password.readPassword(System.in);

            if (pass == null) {
                error(rb.getString("you.must.enter.key.password"));
            } else {
                return pass;
            }
        } catch (IOException ioe) {
            error(rb.getString("unable.to.read.password.")+ioe.getMessage());
        }
        // this shouldn't happen
        return null;
    }

    /*
     * Reads all the bytes for a given zip entry.
     */
    private synchronized byte[] getBytes(ZipFile zf,
                                         ZipEntry ze) throws IOException {
        int n;

        InputStream is = null;
        try {
            is = zf.getInputStream(ze);
            baos.reset();
            long left = ze.getSize();

            while((left > 0) && (n = is.read(buffer, 0, buffer.length)) != -1) {
                baos.write(buffer, 0, n);
                left -= n;
            }
        } finally {
            if (is != null) {
                is.close();
            }
        }

        return baos.toByteArray();
    }

    /*
     * Returns manifest entry from given jar file, or null if given jar file
     * does not have a manifest entry.
     */
    private ZipEntry getManifestFile(ZipFile zf) {
        ZipEntry ze = zf.getEntry(JarFile.MANIFEST_NAME);
        if (ze == null) {
            // Check all entries for matching name
            Enumeration<? extends ZipEntry> enum_ = zf.entries();
            while (enum_.hasMoreElements() && ze == null) {
                ze = enum_.nextElement();
                if (!JarFile.MANIFEST_NAME.equalsIgnoreCase
                    (ze.getName())) {
                    ze = null;
                }
            }
        }
        return ze;
    }

    /*
     * Computes the digests of a zip entry, and returns them as an array
     * of base64-encoded strings.
     */
    private synchronized String[] getDigests(ZipEntry ze, ZipFile zf,
                                             MessageDigest[] digests)
        throws IOException {

        int n, i;
        InputStream is = null;
        try {
            is = zf.getInputStream(ze);
            long left = ze.getSize();
            while((left > 0)
                && (n = is.read(buffer, 0, buffer.length)) != -1) {
                for (i=0; i<digests.length; i++) {
                    digests[i].update(buffer, 0, n);
                }
                left -= n;
            }
        } finally {
            if (is != null) {
                is.close();
            }
        }

        // complete the digests
        String[] base64Digests = new String[digests.length];
        for (i=0; i<digests.length; i++) {
            base64Digests[i] = Base64.getEncoder().encodeToString(digests[i].digest());
        }
        return base64Digests;
    }

    /*
     * Computes the digests of a zip entry, and returns them as a list of
     * attributes
     */
    private Attributes getDigestAttributes(ZipEntry ze, ZipFile zf,
                                           MessageDigest[] digests)
        throws IOException {

        String[] base64Digests = getDigests(ze, zf, digests);
        Attributes attrs = new Attributes();

        for (int i=0; i<digests.length; i++) {
            attrs.putValue(digests[i].getAlgorithm()+"-Digest",
                           base64Digests[i]);
        }
        return attrs;
    }

    /*
     * Updates the digest attributes of a manifest entry, by adding or
     * replacing digest values.
     * A digest value is added if the manifest entry does not contain a digest
     * for that particular algorithm.
     * A digest value is replaced if it is obsolete.
     *
     * Returns true if the manifest entry has been changed, and false
     * otherwise.
     */
    private boolean updateDigests(ZipEntry ze, ZipFile zf,
                                  MessageDigest[] digests,
                                  Manifest mf) throws IOException {
        boolean update = false;

        Attributes attrs = mf.getAttributes(ze.getName());
        String[] base64Digests = getDigests(ze, zf, digests);

        for (int i=0; i<digests.length; i++) {
            // The entry name to be written into attrs
            String name = null;
            try {
                // Find if the digest already exists
                AlgorithmId aid = AlgorithmId.get(digests[i].getAlgorithm());
                for (Object key: attrs.keySet()) {
                    if (key instanceof Attributes.Name) {
                        String n = ((Attributes.Name)key).toString();
                        if (n.toUpperCase(Locale.ENGLISH).endsWith("-DIGEST")) {
                            String tmp = n.substring(0, n.length() - 7);
                            if (AlgorithmId.get(tmp).equals(aid)) {
                                name = n;
                                break;
                            }
                        }
                    }
                }
            } catch (NoSuchAlgorithmException nsae) {
                // Ignored. Writing new digest entry.
            }

            if (name == null) {
                name = digests[i].getAlgorithm()+"-Digest";
                attrs.putValue(name, base64Digests[i]);
                update=true;
            } else {
                // compare digests, and replace the one in the manifest
                // if they are different
                String mfDigest = attrs.getValue(name);
                if (!mfDigest.equalsIgnoreCase(base64Digests[i])) {
                    attrs.putValue(name, base64Digests[i]);
                    update=true;
                }
            }
        }
        return update;
    }

    /*
     * Try to load the specified signing mechanism.
     * The URL class loader is used.
     */
    private ContentSigner loadSigningMechanism(String signerClassName,
        String signerClassPath) throws Exception {

        // construct class loader
        String cpString = null;   // make sure env.class.path defaults to dot

        // do prepends to get correct ordering
        cpString = PathList.appendPath(System.getProperty("env.class.path"), cpString);
        cpString = PathList.appendPath(System.getProperty("java.class.path"), cpString);
        cpString = PathList.appendPath(signerClassPath, cpString);
        URL[] urls = PathList.pathToURLs(cpString);
        ClassLoader appClassLoader = new URLClassLoader(urls);

        // attempt to find signer
        Class<?> signerClass = appClassLoader.loadClass(signerClassName);

        // Check that it implements ContentSigner
        Object signer = signerClass.newInstance();
        if (!(signer instanceof ContentSigner)) {
            MessageFormat form = new MessageFormat(
                rb.getString("signerClass.is.not.a.signing.mechanism"));
            Object[] source = {signerClass.getName()};
            throw new IllegalArgumentException(form.format(source));
        }
        return (ContentSigner)signer;
    }
}

class SignatureFile {

    /** SignatureFile */
    Manifest sf;

    /** .SF base name */
    String baseName;

    public SignatureFile(MessageDigest digests[],
                         Manifest mf,
                         ManifestDigester md,
                         String baseName,
                         boolean signManifest)

    {
        this.baseName = baseName;

        String version = System.getProperty("java.version");
        String javaVendor = System.getProperty("java.vendor");

        sf = new Manifest();
        Attributes mattr = sf.getMainAttributes();

        mattr.putValue(Attributes.Name.SIGNATURE_VERSION.toString(), "1.0");
        mattr.putValue("Created-By", version + " (" + javaVendor + ")");

        if (signManifest) {
            // sign the whole manifest
            for (int i=0; i < digests.length; i++) {
                mattr.putValue(digests[i].getAlgorithm()+"-Digest-Manifest",
                               Base64.getEncoder().encodeToString(md.manifestDigest(digests[i])));
            }
        }

        // create digest of the manifest main attributes
        ManifestDigester.Entry mde =
                md.get(ManifestDigester.MF_MAIN_ATTRS, false);
        if (mde != null) {
            for (int i=0; i < digests.length; i++) {
                mattr.putValue(digests[i].getAlgorithm() +
                        "-Digest-" + ManifestDigester.MF_MAIN_ATTRS,
                        Base64.getEncoder().encodeToString(mde.digest(digests[i])));
            }
        } else {
            throw new IllegalStateException
                ("ManifestDigester failed to create " +
                "Manifest-Main-Attribute entry");
        }

        /* go through the manifest entries and create the digests */

        Map<String,Attributes> entries = sf.getEntries();
        Iterator<Map.Entry<String,Attributes>> mit =
                                mf.getEntries().entrySet().iterator();
        while(mit.hasNext()) {
            Map.Entry<String,Attributes> e = mit.next();
            String name = e.getKey();
            mde = md.get(name, false);
            if (mde != null) {
                Attributes attr = new Attributes();
                for (int i=0; i < digests.length; i++) {
                    attr.putValue(digests[i].getAlgorithm()+"-Digest",
                                  Base64.getEncoder().encodeToString(mde.digest(digests[i])));
                }
                entries.put(name, attr);
            }
        }
    }

    /**
     * Writes the SignatureFile to the specified OutputStream.
     *
     * @param out the output stream
     * @exception IOException if an I/O error has occurred
     */

    public void write(OutputStream out) throws IOException
    {
        sf.write(out);
    }

    /**
     * get .SF file name
     */
    public String getMetaName()
    {
        return "META-INF/"+ baseName + ".SF";
    }

    /**
     * get base file name
     */
    public String getBaseName()
    {
        return baseName;
    }

    /*
     * Generate a signed data block.
     * If a URL or a certificate (containing a URL) for a Timestamping
     * Authority is supplied then a signature timestamp is generated and
     * inserted into the signed data block.
     *
     * @param sigalg signature algorithm to use, or null to use default
     * @param tsaUrl The location of the Timestamping Authority. If null
     *               then no timestamp is requested.
     * @param tsaCert The certificate for the Timestamping Authority. If null
     *               then no timestamp is requested.
     * @param signingMechanism The signing mechanism to use.
     * @param args The command-line arguments to jarsigner.
     * @param zipFile The original source Zip file.
     */
    public Block generateBlock(PrivateKey privateKey,
                               String sigalg,
                               X509Certificate[] certChain,
                               boolean externalSF, String tsaUrl,
                               X509Certificate tsaCert,
                               String tSAPolicyID,
                               String tSADigestAlg,
                               ContentSigner signingMechanism,
                               String[] args, ZipFile zipFile)
        throws NoSuchAlgorithmException, InvalidKeyException, IOException,
            SignatureException, CertificateException
    {
        return new Block(this, privateKey, sigalg, certChain, externalSF,
                tsaUrl, tsaCert, tSAPolicyID, tSADigestAlg, signingMechanism, args, zipFile);
    }


    public static class Block {

        private byte[] block;
        private String blockFileName;

        /*
         * Construct a new signature block.
         */
        Block(SignatureFile sfg, PrivateKey privateKey, String sigalg,
            X509Certificate[] certChain, boolean externalSF, String tsaUrl,
            X509Certificate tsaCert, String tSAPolicyID, String tSADigestAlg,
            ContentSigner signingMechanism, String[] args, ZipFile zipFile)
            throws NoSuchAlgorithmException, InvalidKeyException, IOException,
            SignatureException, CertificateException {

            Principal issuerName = certChain[0].getIssuerDN();
            if (!(issuerName instanceof X500Name)) {
                // must extract the original encoded form of DN for subsequent
                // name comparison checks (converting to a String and back to
                // an encoded DN could cause the types of String attribute
                // values to be changed)
                X509CertInfo tbsCert = new
                    X509CertInfo(certChain[0].getTBSCertificate());
                issuerName = (Principal)
                    tbsCert.get(X509CertInfo.ISSUER + "." +
                                X509CertInfo.DN_NAME);
                }
            BigInteger serial = certChain[0].getSerialNumber();

            String signatureAlgorithm;
            String keyAlgorithm = privateKey.getAlgorithm();
            /*
             * If no signature algorithm was specified, we choose a
             * default that is compatible with the private key algorithm.
             */
            if (sigalg == null) {

                if (keyAlgorithm.equalsIgnoreCase("DSA"))
                    signatureAlgorithm = "SHA256withDSA";
                else if (keyAlgorithm.equalsIgnoreCase("RSA"))
                    signatureAlgorithm = "SHA256withRSA";
                else if (keyAlgorithm.equalsIgnoreCase("EC"))
                    signatureAlgorithm = "SHA256withECDSA";
                else
                    throw new RuntimeException("private key is not a DSA or "
                                               + "RSA key");
            } else {
                signatureAlgorithm = sigalg;
            }

            // check common invalid key/signature algorithm combinations
            String sigAlgUpperCase = signatureAlgorithm.toUpperCase(Locale.ENGLISH);
            if ((sigAlgUpperCase.endsWith("WITHRSA") &&
                !keyAlgorithm.equalsIgnoreCase("RSA")) ||
                (sigAlgUpperCase.endsWith("WITHECDSA") &&
                !keyAlgorithm.equalsIgnoreCase("EC")) ||
                (sigAlgUpperCase.endsWith("WITHDSA") &&
                !keyAlgorithm.equalsIgnoreCase("DSA"))) {
                throw new SignatureException
                    ("private key algorithm is not compatible with signature algorithm");
            }

            blockFileName = "META-INF/"+sfg.getBaseName()+"."+keyAlgorithm;

            AlgorithmId sigAlg = AlgorithmId.get(signatureAlgorithm);
            AlgorithmId digEncrAlg = AlgorithmId.get(keyAlgorithm);

            Signature sig = Signature.getInstance(signatureAlgorithm);
            sig.initSign(privateKey);

            ByteArrayOutputStream baos = new ByteArrayOutputStream();
            sfg.write(baos);

            byte[] content = baos.toByteArray();

            sig.update(content);
            byte[] signature = sig.sign();

            // Timestamp the signature and generate the signature block file
            if (signingMechanism == null) {
                signingMechanism = new TimestampedSigner();
            }
            URI tsaUri = null;
            try {
                if (tsaUrl != null) {
                    tsaUri = new URI(tsaUrl);
                }
            } catch (URISyntaxException e) {
                throw new IOException(e);
            }

            // Assemble parameters for the signing mechanism
            ContentSignerParameters params =
                new JarSignerParameters(args, tsaUri, tsaCert, tSAPolicyID,
                        tSADigestAlg, signature,
                    signatureAlgorithm, certChain, content, zipFile);

            // Generate the signature block
            block = signingMechanism.generateSignedData(
                    params, externalSF, (tsaUrl != null || tsaCert != null));
        }

        /*
         * get block file name.
         */
        public String getMetaName()
        {
            return blockFileName;
        }

        /**
         * Writes the block file to the specified OutputStream.
         *
         * @param out the output stream
         * @exception IOException if an I/O error has occurred
         */

        public void write(OutputStream out) throws IOException
        {
            out.write(block);
        }
    }
}

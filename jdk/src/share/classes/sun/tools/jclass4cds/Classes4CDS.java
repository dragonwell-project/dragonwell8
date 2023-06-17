package sun.tools.jclass4cds;

import java.io.*;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.jar.JarFile;
import java.util.zip.ZipEntry;

public class Classes4CDS {
    BufferedReader in;
    PrintStream out;


    final String tmpDir = "./tmp";
    static Classes4CDS instance = null;
    public static Classes4CDS getInstance() {
        if (instance == null) {
            instance = new Classes4CDS();
        }
        return instance;
    }

    private Classes4CDS() {}   // prevent created from outside.
    static HashMap<String, String> fatJarCache = new HashMap<>();

    // recursive call
    private void mkdir(File dir) {
        if (!dir.exists()) {
            File parentDir = dir.getParentFile();
            if (!parentDir.exists()) {
                mkdir(parentDir);
            }
            dir.mkdir();
        }
    }

    // extract fat jar from main jar, put it to local tmp dir
    private boolean extractFatJar(CDSData data, String mainJarName, String fatJarName) {
        String source = fatJarCache.get(fatJarName);
        if (source != null) {
            data.source =  source;
            return true;
        }
        try {
            JarFile jar = new JarFile(mainJarName);
            ZipEntry ze = jar.getEntry(fatJarName);
            String tmpFile = tmpDir + "/" + fatJarName;    // this will be ./tmp/BOOT/.../fatJar.jar
            if (ze != null) {
                InputStream sin = jar.getInputStream(ze);
                File parent = new File(tmpFile).getParentFile();
                mkdir(parent);   // if not exists the dir, create it.
                tmpFile = tmpDir + "/" + fatJarName;
                FileOutputStream output = new FileOutputStream(tmpFile);
                byte buff[] = new byte[4096];
                int n;
                while ((n = sin.read(buff)) > 0) {
                    output.write(buff, 0, n);
                }
            } else {
                System.out.println("ZipEntry does not exist for " + fatJarName);
                return false;
            }
            data.source = tmpFile;
            fatJarCache.put(fatJarName, tmpFile);
            return true;
        } catch (IOException e) {
            System.out.println("Exception happened: " + e);
            return false;
        }
    }

    private boolean extractFileDir(CDSData data, String mainJarName, String path) throws Exception {
        String dir = fatJarCache.get(path);
        if (dir != null) {
            return true;
        }
        String newName = data.className;
        String filePrePath = null;
        if (newName.contains("/")) {      // what does these code work for??
            int index = newName.length() - 1;
            while(newName.charAt(index) != '/') index--;
            if (!path.endsWith("/")) path += "/";
            filePrePath = newName.substring(0, index+1);
            newName = newName.substring(index+1);
        } else if (newName.contains(".")) {
            int index = newName.length() - 1;
            while(newName.charAt(index) != '.') index--;
            if (!path.endsWith("/")) path += "/";
            filePrePath = newName.substring(0, index+1);
            filePrePath = filePrePath.replace('.', '/');
            newName = newName.substring(index+1);
        }

        try {
            JarFile jar = new JarFile(mainJarName);
            if (filePrePath != null) {
                path += filePrePath;
            }
            String total = path + newName + ".class";
            ZipEntry ze = jar.getEntry(total);
            String tmpFile;
            if (ze != null) {
                InputStream input = jar.getInputStream(ze);
                File fdir = new File(tmpDir + "/" + path);
                mkdir(fdir);
                tmpFile = tmpDir + "/" + path + "/" + newName + ".class";
                FileOutputStream output = new FileOutputStream(tmpFile);
                byte buff[] = new byte[4096];
                int n;
                while ((n = input.read(buff)) > 0) {
                    output.write(buff, 0, n);
                }
            } else {
                throw new Exception("ZipEntry does not exist for " + path + " ?");
            }
            fatJarCache.put(path, tmpFile);
            data.source = tmpDir + "/" + path;
            return true;
        } catch (IOException e) {
            System.out.println("Exception happened: " + e);
            e.printStackTrace();
            throw e;
        }
    }

    public void setInputStream(BufferedReader input) {
        in = input;
    }

    public void setOutputStream(PrintStream prt) {
        out = prt;
    }

    boolean status = true; // succeeded
    public boolean succeeded() { return status; }

    /*
       exclude: classname contains *$$Lambda$
                source contains "__JVM_DefineClass__"
       for classes loaded by bootloader, source is jrt:/java.*
       struct CDSKlass:  className(String)
                id(String)
                superId(String)
                interFaceIds(String[])
                source(String)

       ArrayList<CDSKlass) all: contains all the parsed CDS classes
       HashSet<id, intId>:
                id is string and unique, intId will be increased by 1;
                before output, replace id by intId;
       cond:    if source is jrt:/...  source set to null
                HashSet<id, className>
       convert id based on 1 (java/lang/Object)

     */

    int p1 = 0, p2 = 0;  // index for line1 and line2 position.

    class CDSData {
        public String className;
        public String id;
        public String superId;
        public List<String> interfaceIds;
        public String source;
        public String definingHash;
        public String initiatingHash;
        public String fingerprint;
        public CDSData(String name, String id, String superId, List<String> iids, String sourcePath, String definingHash, String fingerprint) {
            className = name;
            this.id = id;
            this.superId = superId;
            interfaceIds = iids;
            source = sourcePath;
            this.definingHash = definingHash;
            this.initiatingHash = null;
            this.fingerprint = fingerprint;
        }

        public CDSData(String name, String sourcePath, String initiatingHash) {
            className = name;
            this.id = null;
            this.superId = null;
            interfaceIds = null;
            source = sourcePath;
            this.definingHash = null;
            this.initiatingHash = initiatingHash;
            this.fingerprint = null;
        }

    }

    private String getKlassName(String line) {
        int index = 0;
        while (line.charAt(index) != ' ') index++;
        String name = line.substring(0, index);
        return name;
    }

    private String getKlassPath(String line) {
        int index = line.indexOf("source:");
        if (index == -1) {
            return null;
        }
        index += "source:".length();
        while(line.charAt(index) == ' ') index++;
        int start = index;
        while(line.charAt(index) != ' ') index++;
        String path = line.substring(start, index);
        return path;
    }

    private String getId(String line) throws Exception {
        int index = line.indexOf("klass: ");
        if (index == -1) {
            throw new Exception("no Id!");
        }
        index += "klass: ".length();
        while(line.charAt(index) == ' ') index++;
        int start = index;
        while(index < line.length() && line.charAt(index) != ' ') index++;
        String id = line.substring(start, index);
        return id.trim();
    }

    private String getSuperId(String line) {
        int index = line.indexOf("super: ");
	if (index == -1) {
	    return null;
	}
        index += "super: ".length();
        while(line.charAt(index) == ' ') index++;
        int start = index;
        while(index < line.length() && line.charAt(index) != ' ') index++;
        String superId = line.substring(start, index);
        return superId;
    }

    private List<String> getInterfaces(String line) {
        List<String> itfs = new ArrayList<String>();
        int index = line.indexOf("interfaces: ");
        if (index == -1) {
            return itfs;
        }
        index += "interfaces: ".length();
        while(line.charAt(index) == ' ') index++;

        String intfs;
        // find first alphabet
        int i;
        for (i = index; i < line.length(); ) {
            i += 18;
            if (i >= line.length()) {
                break;
            } else {
                while(line.charAt(i) == ' ') i++;
                if ((i + 1) >= line.length()) {
                    break;
                } else {
                    if (line.charAt(i) == '0' && line.charAt(i + 1) == 'x') {
                        continue;
                    } else {
                        break;
                    }
                }
            }
        }
        if (i == line.length()) {   // no alphabet anymore
            intfs = line.substring(index).trim();
        } else {
            intfs = line.substring(index, i).trim();
        }

        String[] iids = intfs.split(" ");
        for(String s: iids) {
            itfs.add(s);
        }
        return itfs;
    }

    private String getDefiningLoaderHash(String line) {
        int index = line.indexOf("defining_loader_hash: ");
        if (index == -1)  return null;
        index += "defining_loader_hash: ".length();
        while(line.charAt(index) == ' ') index++;
        int start = index;
        while(index < line.length() && line.charAt(index) != ' ') index++;
        String hash = line.substring(start, index);
        return hash;
    }

    private String getInitiatingLoaderHash(String line) {
        int index = line.indexOf("initiating_loader_hash: ");
        if (index == -1)  return null;
        index += "initiating_loader_hash: ".length();
        while(line.charAt(index) == ' ') index++;
        int start = index;
        while(index < line.length() && line.charAt(index) != ' ') index++;
        String hash = line.substring(start, index);
        return hash;
    }

    private String getFingerprint(String line) {
        int index = line.indexOf("fingerprint: ");
        if (index == -1)  return null;
        index += "fingerprint: ".length();
        while(line.charAt(index) == ' ') index++;
        int start = index;
        while(index < line.length() && line.charAt(index) != ' ') index++;
        String name = line.substring(start, index);
        return name;
    }

    void printCDSData(CDSData data) {
        out.print(data.className); out.print(" ");
        out.print("id: " + data.id); out.print(" ");

        if (data.source != null && !data.source.contains("jrt:")) {
            if (data.superId != null) {
                out.print("super: " + data.superId);
                out.print(" ");
            }
            if (data.interfaceIds != null && data.interfaceIds.size() != 0) {
                out.print("interfaces: ");
                for (String s : data.interfaceIds) {
                    out.print(s);
                    out.print(" ");
                }
            }
            out.print("source: " + data.source); out.print(" ");
            //System.out.println(data.source);
        }
        if (data.initiatingHash != null) {
            out.print("initiating_loader_hash: " + data.initiatingHash);
            out.print(" ");
        } else if (data.definingHash != null) {
            out.print("initiating_loader_hash: " + data.definingHash);
            out.print(" ");
        }

        if (data.definingHash != null) {
            out.print("defining_loader_hash: " + data.definingHash);
            out.print(" ");
        }
        if (data.fingerprint != null) {
            out.print("fingerprint: " + data.fingerprint);
            out.print(" ");
        }
        out.println();
    }

    private void decodeSource(CDSData data) throws Exception {
        if (data.source == null) {
            return;
        }
        String source = data.source;  // convenience
        int end = -1;
        int start = -1;
        boolean isFatJar = false;
        boolean isJarFile = false;
        boolean isFileDir = false;
        String mainJarName;
        String fatJarName;
        // assert source.contains(".jar");
        if (source.contains("jar:file:")) {

            String[] jjs = source.split("!");
            if (jjs.length >= 2) {
                if (jjs[0].contains(".jar")) {
                    if (jjs[1].contains(".jar")) {
                        isFatJar = true;
                    } else {
                        if (jjs[1].length() == 1 && jjs[1].endsWith("/")) {
                            // this is a regular jar (note it is processed by custom loader)
                            isJarFile    = true;
                        } else {
                            // assume that it is a file dir in jar. It may be wrong here!
                            isFileDir = true;
                        }
                    }
                } else {
                    throw new Exception("Cannot parse " + source);
                }
            }

            if (isJarFile) {
                start = source.indexOf("jar:file:");
                end   = jjs[0].length() - 1;
                while (jjs[0].charAt(end) == '/') end--;
                mainJarName = jjs[0].substring(start, end + 1);
                source = mainJarName;
                source = source.substring("jar:file:".length());
                data.source = source;
            }

            // this is a fat jar case
            // jar:file:<main jar>!<fat jar>!/
            if (isFatJar) {
                mainJarName = jjs[0].substring("jar:file:".length());
                start = 0;
                end = jjs[1].length() - 1;
                // skip '/' at first
                while (jjs[1].charAt(start) == '/') {
                    start++;
                }
                // skip "!/" at end
                while (jjs[1].charAt(end) == '/' || jjs[1].charAt(end) == '!') {
                    end --;
                }
                fatJarName = jjs[1].substring(start, end + 1);
                extractFatJar(data, mainJarName, fatJarName);
            }
            // file dir in jar
            // Test id:626736 super:4080 source:jar:file:/home/myid/tests/appcds/test-1.0-SNAPSHOT.jar!/BOOT-INF/classes!/
            if (isFileDir) {
                mainJarName = jjs[0].substring("jar:file:".length());
                start = 0;
                end   = jjs[1].length() - 1;;
                while (jjs[1].charAt(end) == '/' || jjs[1].charAt(end) == '!') end--;
                while (jjs[1].charAt(start) == '/') start++;
                String fileDir = jjs[1].substring(start, end + 1);
                // fatJarCache.put(mainJarName, fileDir);

                extractFileDir(data, mainJarName, fileDir);
            }
        } else if (source.contains("file:")) {
            // regular jar case
            // file:<dir/<main jar>
            int index = source.indexOf("file:");
            index += "file:".length();
            while(source.charAt(index) == ' ') {
                index++;
            }
            data.source = source.substring(index);
        }
    }

    private boolean invalidCheck(CDSData data) {
        if (data.source == null) {
            return false;
        }
        // appCDS
        if (data.definingHash == null) {
            if (eagerCDSSet.contains(data.className) || appCDSSet.contains(data.className)) {
                return true;
            } else {
                appCDSSet.add(data.className);
            }
        } else {
            //eagerAppCDS
            if (appCDSSet.contains(data.className)) {
                return true;
            } else {
                eagerCDSSet.add(data.className);
            }
        }
        return false;
    }

    HashMap<String, String> idIds = new HashMap<String, String>();
    HashMap<String, CDSData> nameidCDSData = new HashMap<String, CDSData>();
    HashMap<String, CDSData> notFoundCDSData = new HashMap<String, CDSData>();
    Set<String> eagerCDSSet = new HashSet<>();
    Set<String> appCDSSet = new HashSet<>();
    Set<String> notfoundSet = new HashSet<>();

    List<CDSData> all = new ArrayList<CDSData>();
    List<CDSData> allnotfound = new ArrayList<CDSData>();

    void run() {
        try {
            String line = in.readLine();
            while (!line.isEmpty()) {
                if (line.contains("defining_loader_hash:") && line.contains("initiating_loader_hash:")) {
                    String name = getKlassName(line);
                    String id = getId(line);
                    String definingHash = getDefiningLoaderHash(line);
                    String initiatingHash = getInitiatingLoaderHash(line);
                    CDSData oldData = nameidCDSData.get(name + id + definingHash);
                    if (oldData.initiatingHash == null) {
                        oldData.initiatingHash = initiatingHash;
                    }
                } else if (line.contains("source: not.found.class")) {
                    String name = getKlassName(line);
                    String source = getKlassPath(line);
                    String initiatingHash = getInitiatingLoaderHash(line);
                    if (notfoundSet.contains(name + initiatingHash) == false) {
                        CDSData newData = new CDSData(name, source, initiatingHash);
                        allnotfound.add(newData);
                        notfoundSet.add(name + initiatingHash);
                    }
                } else if (line.contains("source: __JVM_DefineClass__")) {
                    // nothing to do
                } else {
                    String name = getKlassName(line);
                    String id = getId(line);
                    String source = getKlassPath(line);
                    String superId = getSuperId(line);
                    List<String> iids = getInterfaces(line);
                    String definingHash = getDefiningLoaderHash(line);
                    String fingerprint = getFingerprint(line);
                    CDSData newData = new CDSData(name, id, superId, iids, source, definingHash, fingerprint);
                    all.add(newData);
                    nameidCDSData.put(name + id + definingHash, newData);
                }
                line = in.readLine();
                if (line == null) {
                    break;
                }
            }
        } catch (Exception e) {
            e.printStackTrace();
            status = false;
            return;
        }

        // Now replace Id with incremental numbers
        // First should be java/lang/Object
        System.out.println("Total class load: " + all.size());
        int klassID = 1;
        CDSData data = all.get(0);
        if (!data.className.equals("java/lang/Object")) {
            System.out.println("First should be java/lang/Object!");
            status = false;
            return;
        }
        data.superId = null;
        idIds.put(data.id, "1");
        data.id = String.valueOf(klassID);
        try {
            decodeSource(data);
        } catch (Exception e) {
            System.out.println("Error happened, Exception is " + e);
            e.printStackTrace();
            status = false;
            return;
        }
        printCDSData(data);
        for (int i = 1; i < all.size(); i++) {
            boolean isvalid = true;
            data = all.get(i);
            if (invalidCheck(data)) {
                continue;
            }
            String newId = String.valueOf(++klassID);
            idIds.put(data.id, newId);
            data.id = newId;
	    if (data.superId != null) {
            	String sp = idIds.get(data.superId);
            	if (sp == null) {
                    isvalid = false;
            	}
            	data.superId = sp;
	    }
            if (data.interfaceIds.size() != 0) {
                for (int j = 0; j < data.interfaceIds.size(); j++) {
                    String intf = data.interfaceIds.get(j);
                    String iid = idIds.get(intf);
                    if (iid == null) {
                        isvalid = false;
                    }
                    data.interfaceIds.remove(j);
                    data.interfaceIds.add(j, iid);
                }
            }
            if (isvalid) {
                try {
                    decodeSource(data);
                } catch (Exception e) {
                    System.out.println("Error happened, Exception is " + e);
                    e.printStackTrace();
                    status = false;
                    return;
                }
                printCDSData(data);
            }
        }
        for (int i = 0; i < allnotfound.size(); i++) {
            data = allnotfound.get(i);
            String name = data.className;
            if (appCDSSet.contains(name)) {
                continue;
            }
            String newId = String.valueOf(++klassID);
            data.id = newId;
            printCDSData(data);
        }
    }

    public static void main(String... args) throws Exception {
        if (args.length != 2) {
            printHelp();
        }

        File f  = new File(args[0]);
        if (!f.exists()) {
            System.out.println("Non exists input file: " + args[0]);
            return;
        }
        BufferedReader in = new BufferedReader(new FileReader(f));
        // clean file content
        PrintWriter pw = new PrintWriter(args[1]);
        pw.close();
        PrintStream out   = new PrintStream(args[1]);
        Classes4CDS cds = Classes4CDS.getInstance();
        cds.setInputStream(in);
        cds.setOutputStream(out);
        cds.run();
        if (cds.status) {
            System.out.println("Succeeded!");
        } else {
            System.out.println("Failed!");
        }
    }
    public static void printHelp() {
        System.out.println("Usage: ");
        System.out.println(" <input file> <output file>");
        System.exit(0);
    }
}

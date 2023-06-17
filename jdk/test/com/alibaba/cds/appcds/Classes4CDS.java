import java.io.BufferedReader;
import java.io.DataInputStream;
import java.io.File;
import java.io.FileReader;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.PrintStream;
import java.io.PrintWriter;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.ArrayList;
import java.util.Map;
import java.util.Scanner;
import java.util.Set;
import java.util.jar.JarFile;
import java.util.regex.MatchResult;
import java.util.regex.Matcher;
import java.util.regex.Pattern;
import java.util.zip.ZipEntry;

/* for jdk11
 * the output file line is: <class name> id: <int> super: <int> interfaces: <int>...<int> source: <string>
 * for bootloader,  ext-loader and app-loader only <class name> and id are necessary.
 * 1) source: jar:file:/home/myid/tests/appcds/test-1.0-SNAPSHOT.jar!
 * 2) source: jar:file:/home/myid/tests/appcds/test-1.0-SNAPSHOT.jar!/BOOT-INF/lib/spring-core-5.0.5.RELEASE.jar!/
 * 3) source: jar:file:/home/myid/tests/appcds/test-1.0-SNAPSHOT.jar!/BOOT-INF/classes
 */

class Classes4CDS {
    boolean DBG = true;
    BufferedReader in;
    PrintStream out;

    void debugPrint(String... messages) {
        if (DBG) {
            for (String msg: messages) {
                System.out.print(msg + " ");
            }
            System.out.println();
        }
    }
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
        debugPrint("Create Dir: " + dir);
        if (dir.isFile()) {
            debugPrint("Error: " + dir + " is not a directory!");
        }
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
        try {
            JarFile jar = new JarFile(mainJarName);
            ZipEntry ze = jar.getEntry(fatJarName);
            String tmpFile = tmpDir + "/" + fatJarName;    // this will be ./tmp/BOOT/.../fatJar.jar
            if (ze != null) {
                InputStream sin = jar.getInputStream(ze);
                File parent = new File(tmpFile).getParentFile();
                mkdir(parent);   // if not exists the dir, create it.
                tmpFile = tmpDir + "/" + fatJarName;
                if (DBG) {
                    System.out.println("Extracted file: " + tmpFile);
                }
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

    private boolean extractFileDir(CDSData data, String mainJarName, String path) {
        String dir = fatJarCache.get(path);
        if (dir != null) {
            debugPrint("File dir already extracted: " + path);
            return true;
        }
        String newName = data.className;
        if (newName.contains("/")) {
            int index = newName.length() - 1;
            while(newName.charAt(index) != '/') index--;
            newName = newName.substring(index+1);
        } else if (newName.contains(".")) {
            int index = newName.length() - 1;
            while(newName.charAt(index) != '.') index--;
            newName = newName.substring(index+1);
        }

        try {
            JarFile jar = new JarFile(mainJarName);
            ZipEntry ze = jar.getEntry(path + "/" + newName + ".class");
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
                System.out.println("ZipEntry does not exist for " + path);
                return false;
            }
            fatJarCache.put(path, tmpFile);
            data.source = tmpDir + "/" + path;
            return true;
        } catch (IOException e) {
            System.out.println("Exception happened: " + e);
            return false;
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
        public boolean isValid;
        public CDSData(String name, String id, String superId, List<String> iids, String sourcePath) {
            className = name;
            this.id = id;
            this.superId = superId;
            interfaceIds = iids;
            source = sourcePath;
            isValid = true;
        }
    }

    private String getKlassName(String line) {
        int index = line.indexOf("[info][class,load]");
        if (index == -1) {
            index = line.indexOf("[info ][class,load]");
            if (index != -1) {
                index += "[info ][class,load]".length();
            }
        } else {
            index += "[info][class,load]".length();
        }
        if (index == -1) {
            return null;
        }
        if (line.charAt(index) == ' ') index++;
        int start = index;
        while (line.charAt(index) != ' ') index++;
        String name = line.substring(start, index);
        // debugPrint("ClassName: " + name);
        return name;
    }

    private String getKlassPath(String line) {
        int index = line.indexOf("source:");
        if (index == -1) {
            return null;
        }
        index += "source:".length();
        while(line.charAt(index) == ' ') index++;
        String path = line.substring(index);
        // debugPrint("Source: " + path);
        /*if (path.contains("jrt:")) {
            return null;
        }*/
        return path;
    }

    private String getId(String line) {
        int index = line.indexOf("klass: ");
        if (index == -1) {
            return null;
        }
        index += "klass: ".length();
        while(line.charAt(index) == ' ') index++;
        int start = index;
        while(line.charAt(index) != ' ') index++;
        String id = line.substring(start, index);
        return id.trim();
    }

    private String getSuperId(String line) {
        int index = line.indexOf("super: ");
        index += "super: ".length();
        while(line.charAt(index) == ' ') index++;
        int start = index;
        while(line.charAt(index) != ' ') index++;
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
        int end = line.indexOf("loader:");
        while(line.charAt(index) == ' ') index++;
        String intfs = line.substring(index, end-1).trim();
        String[] iids = intfs.split(" ");
        for(String s: iids) {
            itfs.add(s);
        }
        return itfs;
    }

    void printCDSData(CDSData data) {
        if (!data.isValid) {
            return;
        }

        out.print(data.className); out.print(" ");
        out.print("id: " + data.id); out.print(" ");

        if (data.source != null && !data.source.contains("jrt:")) {
            if (data.superId != null) {
                out.print("super: " + data.superId);
                out.print(" ");
            }
            if (data.interfaceIds.size() != 0) {
                out.print("interfaces: ");
                for (String s : data.interfaceIds) {
                    out.print(s);
                    out.print(" ");
                }
            }
            out.print("source: " + data.source);
        }
        out.println();
    }

    private static boolean isValidSource(String src) {
        if (src.startsWith("jrt:") || src.startsWith("jar:file") || src.startsWith("file:")) {
            return true;
        }
        // class file
        if (src.contains(".class")) return true;
        return false;
    }

    // HashMap<String, String> fatJars = new HashMap<String, String>();
    private void decodeSource(CDSData data) throws Exception {
        if (data.source.startsWith("jrt:")) {
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
                data.source = mainJarName;
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
                // debugPrint("mainJarName: " + mainJarName + "  fatJarName: " + fatJarName);
                // fatJarCache.put(mainJarName, fatJarName);
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
                // debugPrint("isFileDir mainJarName: " + mainJarName + " fileDir: " + fileDir);
                // fatJarCache.put(mainJarName, fileDir);
                extractFileDir(data, mainJarName, fileDir);
            }
        } else if (source.contains("file:")) {
            // regular jar case
            // file:<dir/<main jar>
            int index = source.indexOf("file:");
            index += "file:".length();
            while(source.charAt(index) == ' ') index++;
            data.source = source.substring(index);
        } else {
            // regular file path, should not be in the file.
            throw new Exception("The path should not be in the file: " + source);
        }
    }

    // convert a.b.c.X to a/b/c/X
    void decodeClassName(CDSData data) {
	String className = data.className;
        if (className.contains("/")) {
            // no need
            return;
        }
        if (className.contains(".")) {
            String newName = className.replace(".", "/");
            data.className = newName;
        }
    }

    HashMap<String, String> idIds = new HashMap<String, String>();
    List<CDSData> all = new ArrayList<CDSData>();

    void run() {
        try {
            String line1 = null;
            String line2 = null;
            line1 = in.readLine();
            while (!line1.isEmpty()) {
                if (!line1.contains("[info ][class,load]") && !line1.contains("source:")) {
                    line1 = in.readLine();
                    continue;
                }
                line2 = in.readLine();
                if (!line2.contains("[debug][class,load]") && !line2.contains("klass:")) {
                    line2 = in.readLine();
                    continue;
                }
                // out.println(line1);
                // out.println(line2);
                String name = getKlassName(line1);
                String source = getKlassPath(line1);
                String id = getId(line2);
                String superId = getSuperId(line2);
                List<String> iids = getInterfaces(line2);
                all.add(new CDSData(name, id, superId, iids, source));
                line1 = in.readLine();
            }
        } catch (Exception e) {
            System.out.println("Error happened: " + e);
            status = false;
            return;
        }

        // Now replace Id with incremental numbers
        // First should be java/lang/Object
        System.out.println("Total: " + all.size());
        CDSData data = (CDSData) all.get(0);
        if (!data.className.equals("java.lang.Object")) {
                System.out.println("First should be java/lang/Object!");
                status = false;
                return;
        }
        data.superId = null;
        idIds.put(data.id, "1");
        data.id = "1";
        try {
            decodeClassName(data);
            decodeSource(data);
        } catch (Exception e) {
            System.out.println("Error happened, Exception is " + e);
            e.printStackTrace();
            status = false;
            return;
        }
        data.isValid = true;
        printCDSData(data);
        for (int i = 1; i < all.size(); i++) {
            data = all.get(i);
            String newId = String.valueOf(i + 1);
            idIds.put(data.id, newId);
            data.id = newId;
            String sp = idIds.get(data.superId);
            data.superId = sp;
            if (data.interfaceIds.size() != 0) {
                for (int j = 0; j < data.interfaceIds.size(); j++) {
                    String intf = data.interfaceIds.get(j);
                    String iid = idIds.get(intf);
                    data.interfaceIds.remove(j);
                    data.interfaceIds.add(j, iid);
                }
            }
            data.isValid = isValidSource(data.source);
            if (data.isValid) {
                try {
                    decodeSource(data);
                    decodeClassName(data);
                } catch (Exception e) {
                    System.out.println("Error happened, Exception is " + e);
                    e.printStackTrace();
                    status = false;
                    return;
                }
            }
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

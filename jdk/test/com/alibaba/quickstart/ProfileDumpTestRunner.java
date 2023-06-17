import java.io.File;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.stream.Collectors;

public class ProfileDumpTestRunner extends QuickStartTestRunner {
    ProjectWorkDir projectWorkDir;
    private QuickStartFeature feature;

    public ProfileDumpTestRunner(QuickStartFeature feature) {
        this.feature = feature;
    }

    @Override
    protected void run(SingleProjectProvider projectProvider) throws Exception {
        String workDir = System.getProperty("user.dir");
        projectWorkDir = new ProjectWorkDir(workDir + File.separator + "a");
        Project project = projectProvider.getProject();
        project.build(projectWorkDir);
        runAsProfile(project, projectWorkDir);
        runAsDump(project, projectWorkDir);
        runAsReplayer(project, projectWorkDir, false);
    }

    @Override
    protected void run(PairProjectProvider projectProvider) throws Exception {
        throw new RuntimeException("Not support!");
    }

    @Override
    public String[] getQuickStartOptions(File cacheDir, boolean doClassDiff) {
        return merge(new String[][]{feature.getAppendixOption(),
                new String[]{"-Xquickstart:verbose,path=" + cacheDir.getAbsolutePath() + append(feature.getQuickstartSubOption())}});
    }

    private String append(String subOption) {
        return null == subOption || "".equals(subOption) ? "" : "," + subOption;
    }

    private String[] getProfileOptions(File cacheDir) {
        return merge(new String[][]{feature.getAppendixOption(),
                new String[]{"-Xquickstart:profile,verbose,path=" + cacheDir.getAbsolutePath() + append(feature.getQuickstartSubOption())}});
    }

    private String[] getDumpOptions(File cacheDir) {
        return new String[]{"-Xquickstart:dump,verbose,path=" + cacheDir.getAbsolutePath()};
    }

    @Override
    void postCheck() throws Exception {
        //check if all classes and jars has been removed.
        List<Path> files = Files.walk(projectWorkDir.getCacheDir().toPath())
                .filter((f) -> (f.endsWith(".jar") || f.endsWith(".class")))
                .collect(Collectors.toList());
        if (!files.isEmpty()) {
            files.forEach((f) -> System.out.println("Found file :" + f.toFile().getAbsolutePath() + " in cache directory!"));
            throw new RuntimeException("Cache directory " + projectWorkDir.getCacheDir().getAbsolutePath() + " should not contain jars and class file!");
        }
    }

    protected void runAsProfile(Project p, ProjectWorkDir workDir) throws Exception {
        String[] commands = p.getRunConf().buildJavaRunCommands(workDir.getBuild(), p.getArtifacts());
        List<String> cp = p.getRunConf().classpath(workDir.getBuild(), p.getArtifacts());
        ProcessBuilder pb = createJavaProcessBuilder(cp, merge(new String[][]{
                getProfileOptions(workDir.getCacheDir()), commands}));
        jdk.test.lib.process.OutputAnalyzer output = new jdk.test.lib.process.OutputAnalyzer(pb.start());
        output.shouldContain("Running as profiler");
        output.shouldHaveExitValue(0);
    }

    protected void runAsDump(Project p, ProjectWorkDir workDir) throws Exception {
        ProcessBuilder pb = createJavaProcessBuilderNoCP(getDumpOptions(workDir.getCacheDir()));
        jdk.test.lib.process.OutputAnalyzer output = new jdk.test.lib.process.OutputAnalyzer(pb.start());
        output.shouldContain("Running as dumper");
        output.shouldHaveExitValue(0);
    }
}
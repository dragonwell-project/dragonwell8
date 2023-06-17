package trivial;

public class MainEntry {
    public static void main(String[] args) {
        // resolve constant pool but not loading
        Class c = InitializeWithVMDefinedClass.class;
    }
}

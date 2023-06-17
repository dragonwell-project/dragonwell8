package sun.misc;

public interface JavaLangClassLoaderAccess {
    int getSignature(ClassLoader cl);

    void setSignature(ClassLoader cl, int value);
}

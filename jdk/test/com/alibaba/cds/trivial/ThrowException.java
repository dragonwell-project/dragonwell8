package trivial;

public class ThrowException {
    public static void throwError() {
        System.out.println("throwError");
        throw new Error("testing");
    }
}

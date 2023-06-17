package trivial;

public class MyFoo {
    static {
        Runnable r = () -> {
            System.out.println(MyFoo.class.getName() + "AB");
        };
        r.run();
    }
}

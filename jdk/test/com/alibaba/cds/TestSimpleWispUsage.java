public class TestSimpleWispUsage {
    public static void main(String[] args) throws Exception {
        Thread io = new Thread(() -> {
            try {
                int i = 0;
                while (i < 3) {
                    try {
                        Thread.sleep(1000);
                    } catch (Exception e) {
                    }
                    i++;
                }

            } catch (Throwable t) {
            }
        }, "IO thread");

        io.start();

        long start = System.currentTimeMillis();
        Thread.sleep(1000);
    }
}
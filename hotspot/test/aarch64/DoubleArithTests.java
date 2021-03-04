public class DoubleArithTests {

    private static double test_neg(double a) {
        return -a;
    }

    private static double test_add(double a, double b) {
        return a + b;
    }

    private static double test_sub(double a, double b) {
        return a - b;
    }

    private static double test_mul(double a, double b) {
        return a * b;
    }

    private static double test_div(double a, double b) {
        return a / b;
    }

    private static double test_rem(double a, double b) {
        return a % b;
    }

    private static void assertThat(boolean assertion) {
        if (! assertion) {
            throw new AssertionError();
        }
    }

    public static void main(String[] args) {
        assertThat(test_neg(10.0) == -10.0);
        assertThat(test_add(3.0, 2.0) == 5.0);

        assertThat(test_sub(40.0, 13.0) == 27.0);

        assertThat(test_mul(5.0, 200.0) == 1000.0);

        assertThat(test_div(30.0, 3.0) == 10.0);
        assertThat(test_div(30.0, 0.0) == Double.POSITIVE_INFINITY);

        assertThat(test_rem(30.0, 3.0) == 0.0);
        assertThat(test_rem(29.0, 3.0) == 2.0);
        assertThat(Double.isNaN(test_rem(30.0, 0.0)));

    }
}

public class FloatArithTests {

    private static float test_neg(float a) {
        return -a;
    }

    private static float test_add(float a, float b) {
        return a + b;
    }

    private static float test_sub(float a, float b) {
        return a - b;
    }

    private static float test_mul(float a, float b) {
        return a * b;
    }

    private static float test_div(float a, float b) {
        return a / b;
    }

    private static float test_rem(float a, float b) {
        return a % b;
    }

    private static void assertThat(boolean assertion) {
        if (! assertion) {
            throw new AssertionError();
        }
    }

    public static void main(String[] args) {
        assertThat(test_neg(10F) == -10F);
        assertThat(test_add(3F, 2F) == 5F);

        assertThat(test_sub(40F, 13F) == 27F);

        assertThat(test_mul(5F, 200F) == 1000F);

        assertThat(test_div(30F, 3F) == 10F);
        assertThat(test_div(30, 0) == Float.POSITIVE_INFINITY);

        assertThat(test_rem(30F, 3F) == 0);
        assertThat(test_rem(29F, 3F) == 2F);
        assertThat(Float.isNaN(test_rem(30F, 0F)));

    }
}

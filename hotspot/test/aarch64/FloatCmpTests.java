
public class FloatCmpTests {

    private static boolean test_isEq(float a, float b) {
        return a == b;
    }

    private static boolean test_isNe(float a, float b) {
        return a != b;
    }

    private static boolean test_isLt(float a, float b) {
        return a < b;
    }

    private static boolean test_isLe(float a, float b) {
        return a <= b;
    }

    private static boolean test_isGe(float a, float b) {
        return a >= b;
    }

    private static boolean test_isGt(float a, float b) {
        return a > b;
    }

    private static boolean test_isEqC(float a) {
        return a == 7F;
    }

    private static boolean test_isNeC(float a) {
        return a != 7F;
    }

    private static boolean test_isLtC(float a) {
        return a < 7F;
    }

    private static boolean test_isLeC(float a) {
        return a <= 7F;
    }

    private static boolean test_isGeC(float a) {
        return a >= 7F;
    }

    private static boolean test_isGtC(float a) {
        return a > 7F;
    }

    private static void assertThat(boolean assertion) {
        if (! assertion) {
            throw new AssertionError();
        }
    }

    public static void main(String[] args) {
        assertThat(test_isEq(7F, 7F));
        assertThat(! test_isEq(70F, 7F));
        assertThat(! test_isNe(7F, 7F));
        assertThat(test_isNe(70F, 7F));

        assertThat(test_isLt(7F, 70F));
        assertThat(! test_isLt(70F, 7F));
        assertThat(! test_isLt(7F, 7F));

        assertThat(test_isLe(7F, 70F));
        assertThat(! test_isLe(70F, 7F));
        assertThat(test_isLe(7F, 7F));

        assertThat(!test_isGe(7F, 70F));
        assertThat(test_isGe(70F, 7F));
        assertThat(test_isGe(7F, 7F));

        assertThat(!test_isGt(7F, 70F));
        assertThat(test_isGt(70F, 7F));
        assertThat(! test_isGt(7F, 7F));


        assertThat(test_isEqC(7F));
        assertThat(! test_isEqC(70F));
        assertThat(! test_isNeC(7F));
        assertThat(test_isNeC(70F));

        assertThat(test_isLtC(6F));
        assertThat(! test_isLtC(70F));
        assertThat(! test_isLtC(7F));

        assertThat(test_isLeC(6F));
        assertThat(! test_isLeC(70F));
        assertThat(test_isLeC(7F));

        assertThat(!test_isGeC(6F));
        assertThat(test_isGeC(70F));
        assertThat(test_isGeC(7F));

        assertThat(!test_isGtC(6F));
        assertThat(test_isGtC(70F));
        assertThat(! test_isGtC(7F));
    }
}

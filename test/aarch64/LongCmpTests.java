
public class LongCmpTests {

    private static boolean test_isEq(long a, long b) {
        return a == b;
    }

    private static boolean test_isNe(long a, long b) {
        return a != b;
    }

    private static boolean test_isLt(long a, long b) {
        return a < b;
    }

    private static boolean test_isLe(long a, long b) {
        return a <= b;
    }

    private static boolean test_isGe(long a, long b) {
        return a >= b;
    }

    private static boolean test_isGt(long a, long b) {
        return a > b;
    }

    private static boolean test_isEqC(long a) {
        return a == 7L;
    }

    private static boolean test_isNeC(long a) {
        return a != 7L;
    }

    private static boolean test_isLtC(long a) {
        return a < 7L;
    }

    private static boolean test_isLeC(long a) {
        return a <= 7L;
    }

    private static boolean test_isGeC(long a) {
        return a >= 7L;
    }

    private static boolean test_isGtC(long a) {
        return a > 7L;
    }

    private static void assertThat(boolean assertion) {
        if (! assertion) {
            throw new AssertionError();
        }
    }

    public static void main(String[] args) {
        assertThat(test_isEq(7L, 7L));
        assertThat(! test_isEq(70L, 7L));
        assertThat(! test_isNe(7L, 7L));
        assertThat(test_isNe(70L, 7L));

        assertThat(test_isLt(7L, 70L));
        assertThat(! test_isLt(70L, 7L));
        assertThat(! test_isLt(7L, 7L));

        assertThat(test_isLe(7L, 70L));
        assertThat(! test_isLe(70L, 7L));
        assertThat(test_isLe(7L, 7L));

        assertThat(!test_isGe(7L, 70L));
        assertThat(test_isGe(70L, 7L));
        assertThat(test_isGe(7L, 7L));

        assertThat(!test_isGt(7L, 70L));
        assertThat(test_isGt(70L, 7L));
        assertThat(! test_isGt(7L, 7L));

        assertThat(test_isEqC(7L));
        assertThat(! test_isEqC(70L));
        assertThat(! test_isNeC(7L));
        assertThat(test_isNeC(70L));

        assertThat(test_isLtC(6L));
        assertThat(! test_isLtC(70L));
        assertThat(! test_isLtC(7L));

        assertThat(test_isLeC(6L));
        assertThat(! test_isLeC(70L));
        assertThat(test_isLeC(7L));

        assertThat(!test_isGeC(6L));
        assertThat(test_isGeC(70L));
        assertThat(test_isGeC(7L));

        assertThat(!test_isGtC(6L));
        assertThat(test_isGtC(70L));
        assertThat(! test_isGtC(7L));

    }
}

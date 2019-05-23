/*
 * Copyright (c) 2019, Red Hat Inc. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */
import java.lang.reflect.Constructor;
import java.security.SecureRandom;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;


/**
 * @test
 * @bug 8203190
 * @summary Manual test to verify number of collisions in
 *          sun.security.ssl.SessionId.hashCode()
 * @run main/manual SessionIdCollisionTest 100 20480 10000000
 */

/**
 *
 * Notes:
 * - This is a manual test, not run automatically.
 * - New default value of javax.net.ssl.sessionCacheSize in JDK 12+ is 20480
 * - According to JDK-8210985 24 hours expired cache may exceed several million
 *   entries = > 10_000_000
 *
 * Example usage: java SessionIdCollissionTest 100 20480 10000000
 *
 * Expected outcome of running the test is to see fewer collisions and, more
 * importantly fewer elements in buckets when there are collisions. See:
 * http://mail.openjdk.java.net/pipermail/jdk8u-dev/2019-May/009345.html
 *
 */
public class SessionIdCollisionTest {

    private List<Integer> prepareHashCodes(int total) throws Exception {
        Class<?> sessionIdClass = Class.forName("sun.security.ssl.SessionId");
        Constructor<?> c = sessionIdClass.getDeclaredConstructor(byte[].class);
        c.setAccessible(true);
        // case of rejoinable session ids generates 32 random bytes
        byte[] sessionIdBytes = new byte[32];
        List<Integer> hashCodes = new ArrayList<>();
        for (int i = 0; i < total; i++) {
            SecureRandom random = new SecureRandom();
            random.nextBytes(sessionIdBytes);
            Object sessionId = c.newInstance(sessionIdBytes);
            int hashCode = sessionId.hashCode();
            hashCodes.add(hashCode);
        }
        return hashCodes;
    }

    private void printSummary(boolean withDistribution,
                              List<Integer> hashCodes) throws Exception {
        final int bound = hashCodes.size();
        Collections.sort(hashCodes);
        int collisions = 0;
        Map<Integer, List<Integer>> collCountsReverse = new HashMap<>();
        for (int i = 0; i < bound - 1; i++) {
            int oldval = hashCodes.get(i);
            int nextval = hashCodes.get(i+1);
            if (oldval == nextval) {
                collisions++;
                if (i == bound - 2) { // last elements
                    updateCollCountsReverse(collisions, collCountsReverse, oldval);
                }
            } else {
                updateCollCountsReverse(collisions, collCountsReverse, oldval);
                collisions = 0;
                if (i == bound - 2) { // last elements
                    updateCollCountsReverse(collisions, collCountsReverse, nextval);
                }
            }
        }
        if (withDistribution) {
            System.out.println("---- distribution ----");
        }
        int collCount = 0;
        int maxLength = 0;
        List<Integer> sorted = new ArrayList<>(collCountsReverse.size());
        sorted.addAll(collCountsReverse.keySet());
        Collections.sort(sorted);
        for (Integer coll: sorted) {
            List<Integer> hc = collCountsReverse.get(coll);
            if (withDistribution) {
                System.out.printf("Hashcodes with %02d collisions | " +
                                  "hashCodes: %s\n", coll, hc.toString());
            }
            collCount += coll * hc.size();
            if (coll > maxLength) {
                maxLength = coll;
            }
        }
        if (withDistribution) {
            System.out.println("---- distribution ----");
        }
        System.out.println("Total number of collisions: " + collCount);
        if (collCount > 0) {
            System.out.println("Max length of collision list over all buckets: " +
                                maxLength);
        }
    }

    private void updateCollCountsReverse(int collisions,
            Map<Integer, List<Integer>> reverse, int val) {
        List<Integer> hc = reverse.get(collisions);
        if (hc == null) {
            hc = new ArrayList<>();
            hc.add(val);
            reverse.put(collisions, hc);
        } else {
            hc.add(val);
        }
    }

    public void doCollissionTest(int total) throws Exception {
        System.out.println("Collision test for " + total + " sessions:");
        System.out.println("------------------------------------------------");
        List<Integer> hashcodes = prepareHashCodes(total);
        printSummary(false, hashcodes);
        System.out.println();
    }

    public static void main(String[] args) throws Exception {
        if (args.length < 1) {
            System.err.println("java " + SessionIdCollisionTest.class.getSimpleName() +
                               "<num-sessions> [<num-sessions> ...]");
            System.exit(1);
        }
        SessionIdCollisionTest collTest = new SessionIdCollisionTest();
        for (int i = 0; i < args.length; i++) {
            int total = Integer.parseInt(args[i]);
            collTest.doCollissionTest(total);
        }
    }

}

/*
 * Copyright (c) 2023, 2026, Oracle and/or its affiliates. All rights reserved.
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

/* @test
 * @summary Verify that ZipFileSystem rejects files with CEN sizes exceeding the implementation limit
 * @library /lib/testlibrary
 * @build jdk.testlibrary.Utils
 * @build jdk.testlibrary.ZipUtils
 * @run testng/othervm EndOfCenValidation
 */

import org.testng.annotations.AfterTest;
import org.testng.annotations.BeforeTest;
import org.testng.annotations.Test;
import org.testng.annotations.DataProvider;

import jdk.testlibrary.Utils;
import jdk.testlibrary.ZipUtils;

import java.io.IOException;
import java.nio.file.FileSystems;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.Paths;
import java.util.zip.ZipError;

import static org.testng.Assert.*;
import static jdk.testlibrary.ZipUtils.*;

/**
 * This test augments {@link TestTooManyEntries}. It creates sparse ZIPs where
 * the CEN size is inflated to the desired value. This helps this test run
 * fast with much less resources.
 *
 * While the CEN in these files are zero-filled and the produced ZIPs are technically
 * invalid, the CEN is never actually read by ZipFileSystem since it does
 * 'End of central directory record' (END header) validation before reading the CEN.
 */
public class EndOfCenValidation {

    // Zip files produced by this test
    static final Path CEN_TOO_LARGE_ZIP = Paths.get("cen-size-too-large.zip");
    static final Path INVALID_CEN_SIZE = Paths.get("invalid-zen-size.zip");
    static final Path BAD_CEN_OFFSET_ZIP = Paths.get("bad-cen-offset.zip");
    static final Path BAD_ENTRY_COUNT_ZIP = Paths.get("bad-entry-count.zip");

    // Maximum allowed CEN size allowed by ZipFileSystem
    static final int MAX_CEN_SIZE = Integer.MAX_VALUE - 8;

    /**
     * Delete big files after test, in case the file system did not support sparse files.
     * @throws IOException if an error occurs
     */
    @AfterTest
    public void cleanup() throws IOException {
        Files.deleteIfExists(CEN_TOO_LARGE_ZIP);
        Files.deleteIfExists(INVALID_CEN_SIZE);
        Files.deleteIfExists(BAD_CEN_OFFSET_ZIP);
        Files.deleteIfExists(BAD_ENTRY_COUNT_ZIP);
    }

    @DataProvider
    Object[][] totalEntries() {
        return new Object[][] {
            new Object[] { Long.valueOf(-1) },                   // Negative
            new Object[] { Long.MIN_VALUE },                     // Very negative
            new Object[] { Long.valueOf(0x3B / 3L - 1) },        // Cannot fit in test ZIP's CEN
            new Object[] { Long.valueOf(MAX_CEN_SIZE / 3 + 1) }, // Too large to allocate int[] entries array
            new Object[] { Long.MAX_VALUE }                      // Unreasonably large
        };
    }

    /**
     * Validates that an 'End of central directory record' (END header) with a CEN
     * length exceeding {@link #MAX_CEN_SIZE} limit is rejected
     * @throws IOException if an error occurs
     */
    @Test
    public void shouldRejectTooLargeCenSize() throws IOException {
        int size = MAX_CEN_SIZE + 1;
        Path zip = zipWithModifiedEndRecord(size, true, 0, CEN_TOO_LARGE_ZIP);
        verifyRejection(zip, INVALID_CEN_SIZE_TOO_LARGE);
    }

    /**
     * Validate that an 'End of central directory record' (END header)
     * where the value of the CEN size field exceeds the position of
     * the END header is rejected.
     * @throws IOException if an error occurs
     */
    @Test
    public void shouldRejectInvalidCenSize() throws IOException {
        int size = MAX_CEN_SIZE;
        Path zip = zipWithModifiedEndRecord(size, false, 0, INVALID_CEN_SIZE);
        verifyRejection(zip, INVALID_CEN_BAD_SIZE);
    }

    /**
     * Validate that an 'End of central directory record' (the END header)
     * where the value of the CEN offset field is larger than the position
     * of the END header minus the CEN size is rejected
     * @throws IOException if an error occurs
     */
    @Test
    public void shouldRejectInvalidCenOffset() throws IOException {
        int size = MAX_CEN_SIZE;
        Path zip = zipWithModifiedEndRecord(size, true, 100, BAD_CEN_OFFSET_ZIP);
        verifyRejection(zip, INVALID_CEN_BAD_OFFSET);
    }

    /**
     * Validate that a 'Zip64 End of Central Directory' record (the END header)
     * where the value of the 'total entries' field is larger than what fits
     * in the CEN size is rejected.
     *
     * @throws IOException if an error occurs
     */
    @Test(dataProvider = "totalEntries")
    public void shouldRejectBadTotalEntries(long totalEntries) throws IOException {
        Path zip = zip64WithModifiedTotalEntries(BAD_ENTRY_COUNT_ZIP, totalEntries);
        verifyRejection(zip, INVALID_BAD_ENTRY_COUNT);
    }

    /**
     * Verify that ZipFileSystem.newFileSystem rejects the ZIP file with a ZipError
     * with the given message
     * @param zip ZIP file to open
     * @param msg exception message to expect
     */
    private static void verifyRejection(Path zip, String msg) {
        ZipError ex = expectThrows(ZipError.class, () -> {
            FileSystems.newFileSystem(zip, null);
        });
        assertEquals(ex.getMessage(), msg);
    }
}

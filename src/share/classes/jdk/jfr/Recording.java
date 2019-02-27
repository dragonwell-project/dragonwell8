/*
 * Copyright (c) 2016, 2019, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
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

package jdk.jfr;

import java.io.Closeable;
import java.io.IOException;
import java.io.InputStream;
import java.nio.file.Path;
import java.time.Duration;
import java.time.Instant;
import java.util.Collections;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;

import jdk.jfr.internal.PlatformRecording;
import jdk.jfr.internal.Type;
import jdk.jfr.internal.Utils;
import jdk.jfr.internal.WriteableUserPath;

/**
 * Provides means to configure, start, stop and dump recording data to disk.
 * <p>
 * Example,
 *
 * <pre>
 * <code>
 *   Configuration c = Configuration.getConfiguration("default");
 *   Recording r = new Recording(c);
 *   r.start();
 *   System.gc();
 *   Thread.sleep(5000);
 *   r.stop();
 *   r.copyTo(Files.createTempFile("my-recording", ".jfr"));
 * </code>
 * </pre>
 *
 * @since 9
 */
public final class Recording implements Closeable {

    private static class RecordingSettings extends EventSettings {

        private final Recording recording;
        private final String identifier;

        RecordingSettings(Recording r, String identifier) {
            this.recording = r;
            this.identifier = identifier;
        }

        RecordingSettings(Recording r, Class<? extends Event> eventClass) {
            Utils.ensureValidEventSubclass(eventClass);
            this.recording = r;
            this.identifier = String.valueOf(Type.getTypeId(eventClass));
        }

        @Override
        public EventSettings with(String name, String value) {
            Objects.requireNonNull(value);
            recording.setSetting(identifier + "#" + name, value);
            return this;
        }

        @Override
        public Map<String, String> toMap() {
            return recording.getSettings();
        }
    }

    private final PlatformRecording internal;

    private Recording(PlatformRecording internal) {
        this.internal = internal;
        this.internal.setRecording(this);
        if (internal.getRecording() != this) {
            throw new InternalError("Internal recording not properly setup");
        }
    }

    /**
     * Creates a recording without any settings.
     * <p>
     * A newly created recording will be in {@link RecordingState#NEW}. To start
     * the recording invoke {@link Recording#start()}.
     *
     * @throws IllegalStateException if the platform Flight Recorder couldn't be
     *         created, for instance if commercial features isn't unlocked, or
     *         if the file repository can't be created or accessed.
     *
     * @throws SecurityException If a security manager is used and
     *         FlightRecorderPermission "accessFlightRecorder" is not set.
     */
    public Recording() {
        this(FlightRecorder.getFlightRecorder().newInternalRecording(new HashMap<String, String>()));
    }

    /**
     * Creates a recording with settings from a configuration.
     * <p>
     * Example,
     *
     * <pre>
     * <code>
     * Recording r = new Recording(Configuration.getConfiguration("default"));
     * </code>
     * </pre>
     *
     * The newly created recording will be in {@link RecordingState#NEW}. To
     * start the recording invoke {@link Recording#start()}.
     *
     * @param configuration configuration containing settings to be used, not
     *        {@code null}
     *
     * @throws IllegalStateException if the platform Flight Recorder couldn't be
     *         created, for instance if commercial features isn't unlocked, or
     *         if the file repository can't be created or accessed.
     *
     * @throws SecurityException If a security manager is used and
     *         FlightRecorderPermission "accessFlightRecorder" is not set.
     *
     * @see Configuration
     */
    public Recording(Configuration configuration) {
        this(FlightRecorder.getFlightRecorder().newInternalRecording(configuration.getSettings()));
    }

    /**
     * Starts this recording.
     * <p>
     * It's recommended that recording options and event settings are configured
     * before calling this method, since it guarantees a more consistent state
     * when analyzing the recorded data. It may also have performance benefits,
     * since the configuration can be applied atomically.
     * <p>
     * After a successful invocation of this method, the state of this recording will be
     * in {@code RUNNING} state.
     *
     * @throws IllegalStateException if recording has already been started or is
     *         in {@code CLOSED} state
     */
    public void start() {
        internal.start();
    }

    /**
     * Starts this recording after a delay.
     * <p>
     * After a successful invocation of this method, the state of this recording will be
     * in the {@code DELAYED} state.
     *
     * @param delay the time to wait before starting this recording, not
     *        {@code null}
     * @throws IllegalStateException if already started or is in {@code CLOSED}
     *         state
     */
    public void scheduleStart(Duration delay) {
        Objects.requireNonNull(delay);
        internal.scheduleStart(delay);
    }

    /**
     * Stops this recording.
     * <p>
     * Once a recording has been stopped it can't be restarted. If this
     * recording has a destination, data will be written to that destination and
     * the recording closed. Once a recording is closed, the data is no longer
     * available.
     * <p>
     * After a successful invocation of this method, the state of this recording will be
     * in {@code STOPPED} state.
     *
     * @return {@code true} if recording was stopped, {@code false} otherwise
     *
     * @throws IllegalStateException if not started or already been stopped
     *
     * @throws SecurityException if a security manager exists and the caller
     *         doesn't have {@code FilePermission} to write at the destination
     *         path
     *
     * @see #setDestination(Path)
     *
     */
    public boolean stop() {
        return internal.stop("Stopped by user");
    }

    /**
     * Returns settings used by this recording.
     * <p>
     * Modifying the returned map will not change settings for this recording.
     * <p>
     * If no settings have been set for this recording, an empty map is
     * returned.
     *
     * @return recording settings, not {@code null}
     */
    public Map<String, String> getSettings() {
        return new HashMap<>(internal.getSettings());
    }

    /**
     * Returns the current size of this recording in the disk repository,
     * measured in bytes.
     *
     * Size is updated when recording buffers are flushed. If the recording is
     * not to disk the returned is always {@code 0}.
     *
     * @return amount of recorded data, measured in bytes, or {@code 0} if the
     *         recording is not to disk
     */
    public long getSize() {
        return internal.getSize();
    }

    /**
     * Returns the time when this recording was stopped.
     *
     * @return the the time, or {@code null} if this recording hasn't been
     *         stopped
     */
    public Instant getStopTime() {
        return internal.getStopTime();
    }

    /**
     * Returns the time when this recording was started.
     *
     * @return the the time, or {@code null} if this recording hasn't been
     *         started
     */
    public Instant getStartTime() {
        return internal.getStartTime();
    }

    /**
     * Returns the max size, measured in bytes, at which data will no longer be
     * kept in the disk repository.
     *
     * @return max size in bytes, or {@code 0} if no max size has been set
     */
    public long getMaxSize() {
        return internal.getMaxSize();
    }

    /**
     * Returns how long data is to be kept in the disk repository before it is
     * thrown away.
     *
     * @return max age, or {@code null} if no maximum age has been set
     */
    public Duration getMaxAge() {
        return internal.getMaxAge();
    }

    /**
     * Returns the name of this recording.
     * <p>
     * By default the name is the same as the recording id.
     *
     * @return recording name, not {@code null}
     */
    public String getName() {
        return internal.getName();
    }

    /**
     * Replaces all settings for this recording,
     * <p>
     * Example,
     *
     * <pre>
     * <code>
     *     Map{@literal <}String, String{@literal >} settings = new HashMap{@literal <}{@literal >}();
     *     settings.putAll(EventSettings.enabled("com.oracle.jdk.CPUSample").withPeriod(Duration.ofSeconds(2)).toMap());
     *     settings.putAll(EventSettings.enabled(MyEvent.class).withThreshold(Duration.ofSeconds(2)).withoutStackTrace().toMap());
     *     settings.put("com.oracle.jdk.ExecutionSample#period", "10 ms");
     *     recording.setSettings(settings);
     * </code>
     * </pre>
     *
     * To merge with existing settings do.
     *
     * <pre>
     * {@code
     * Map<String, String> settings = recording.getSettings();
     * settings.putAll(additionalSettings));
     * recording.setSettings(settings);
     * }
     * </pre>
     *
     * @param settings the settings to set, not {@code null}
     */
    public void setSettings(Map<String, String> settings) {
        Objects.requireNonNull(settings);
        Map<String, String> sanitized = Utils.sanitizeNullFreeStringMap(settings);
        internal.setSettings(sanitized);
    }

    /**
     * Returns the <code>RecordingState</code> this recording is currently in.
     *
     * @return the recording state, not {@code null}
     *
     * @see RecordingState
     */
    public RecordingState getState() {
        return internal.getState();
    }

    /**
     * Releases all data associated with this recording.
     * <p>
     * After a successful invocation of this method, the state of this recording will be
     * in the {@code CLOSED} state.
     */
    @Override
    public void close() {
        internal.close();
    }

    /**
     * Returns a clone of this recording, but with a new recording id and name.
     *
     * Clones are useful for dumping data without stopping the recording. After
     * a clone is created the amount of data to be copied out can be constrained
     * with the {@link #setMaxAge(Duration)} and {@link #setMaxSize(long)}.
     *
     * @param stop {@code true} if the newly created copy should be stopped
     *        immediately, {@code false} otherwise
     * @return the recording copy, not {@code null}
     */
    public Recording copy(boolean stop) {
        return internal.newCopy(stop);
    }

    /**
     * Writes recording data to a file.
     * <p>
     * Recording must be started, but not necessarily stopped.
     *
     * @param destination where recording data should be written, not
     *        {@code null}
     *
     * @throws IOException if recording can't be copied to {@code path}
     *
     * @throws SecurityException if a security manager exists and the caller
     *         doesn't have {@code FilePermission} to write at the destination
     *         path
     */
    public void dump(Path destination) throws IOException {
        Objects.requireNonNull(destination);
        internal.copyTo(new WriteableUserPath(destination), "Dumped by user", Collections.emptyMap());
    }

    /**
     * Returns if this recording is to disk.
     * <p>
     * If no value has been set, {@code true} is returned.
     *
     * @return {@code true} if recording is to disk, {@code false} otherwise
     */
    public boolean isToDisk() {
        return internal.isToDisk();
    }

    /**
     * Determines how much data should be kept in disk repository.
     * <p>
     * In order to control the amount of recording data stored on disk, and not
     * having it grow indefinitely, Max size can be used to limit the amount of
     * data retained. When the {@code maxSize} limit has been exceeded, the JVM
     * will remove the oldest chunk to make room for a more recent chunk.
     *
     * @param maxSize or {@code 0} if infinite
     *
     * @throws IllegalArgumentException if <code>maxSize</code> is negative
     *
     * @throws IllegalStateException if the recording is in {@code CLOSED} state
     */
    public void setMaxSize(long maxSize) {
        if (maxSize < 0) {
            throw new IllegalArgumentException("Max size of recording can't be negative");
        }
        internal.setMaxSize(maxSize);
    }

    /**
     * Determines how far back data should be kept in disk repository.
     * <p>
     * In order to control the amount of recording data stored on disk, and not
     * having it grow indefinitely, {@code maxAge} can be used to limit the
     * amount of data retained. Data stored on disk which is older than
     * {@code maxAge} will be removed by Flight Recorder.
     *
     * @param maxAge how long data can be kept, or {@code null} if infinite
     *
     * @throws IllegalArgumentException if <code>maxAge</code> is negative
     *
     * @throws IllegalStateException if the recording is in closed state
     */
    public void setMaxAge(Duration maxAge) {
        if (maxAge != null && maxAge.isNegative()) {
            throw new IllegalArgumentException("Max age of recording can't be negative");
        }
        internal.setMaxAge(maxAge);
    }

    /**
     * Sets a location where data will be written on recording stop, or
     * {@code null} if data should not be dumped automatically.
     * <p>
     * If a destination is set, this recording will be closed automatically
     * after data has been copied successfully to the destination path.
     * <p>
     * If a destination is <em>not</em> set, Flight Recorder will hold on to
     * recording data until this recording is closed. Use {@link #dump(Path)} to
     * write data to a file manually.
     *
     * @param destination destination path, or {@code null} if recording should
     *        not be dumped at stop
     *
     * @throws IllegalStateException if recording is in {@code ETOPPED} or
     *         {@code CLOSED} state.
     *
     * @throws SecurityException if a security manager exists and the caller
     *         doesn't have {@code FilePermission} to read, write and delete the
     *         {@code destination} file
     *
     * @throws IOException if path is not writable
     */
    public void setDestination(Path destination) throws IOException {
        internal.setDestination(destination != null ? new WriteableUserPath(destination) : null);
    }

    /**
     * Returns destination file, where recording data will be written when the
     * recording stops, or {@code null} if no destination has been set.
     *
     * @return the destination file, or {@code null} if not set.
     */
    public Path getDestination() {
        WriteableUserPath usp = internal.getDestination();
        if (usp == null) {
            return null;
        } else {
            return usp.getPotentiallyMaliciousOriginal();
        }
    }

    /**
     * Returns a unique identifier for this recording.
     *
     * @return the recording identifier
     */
    public long getId() {
        return internal.getId();
    }

    /**
     * Sets a human-readable name, such as {@code "My Recording"}.
     *
     * @param name the recording name, not {@code null}
     *
     * @throws IllegalStateException if the recording is in closed state
     */
    public void setName(String name) {
        Objects.requireNonNull(name);
        internal.setName(name);
    }

    /**
     * Sets if this recording should be dumped to disk when the JVM exits.
     *
     * @param dumpOnExit if recording should be dumped on JVM exit
     */
    public void setDumpOnExit(boolean dumpOnExit) {
        internal.setDumpOnExit(dumpOnExit);
    }

    /**
     * Returns if this recording should be dumped to disk when the JVM exits.
     * <p>
     * If dump on exit has not been set, {@code false} is returned.
     *
     * @return {@code true} if recording should be dumped on exit, {@code false}
     *         otherwise.
     */
    public boolean getDumpOnExit() {
        return internal.getDumpOnExit();
    }

    /**
     * Determines if this recording should be flushed to disk continuously or if
     * data should be constrained to what is available in memory buffers.
     *
     * @param disk {@code true} if recording should be written to disk,
     *        {@code false} if in-memory
     *
     */
    public void setToDisk(boolean disk) {
        internal.setToDisk(disk);
    }

    /**
     * Creates a data stream for a specified interval.
     *
     * The stream may contain some data outside the specified range.
     *
     * @param start start time for the stream, or {@code null} to get data from
     *        start time of the recording
     *
     * @param end end time for the stream, or {@code null} to get data until the
     *        present time.
     *
     * @return an input stream, or {@code null} if no data is available in the
     *         interval.
     *
     * @throws IllegalArgumentException if {@code end} happens before
     *         {@code start}
     *
     * @throws IOException if a stream can't be opened
     */
    public InputStream getStream(Instant start, Instant end) throws IOException {
        if (start != null && end != null && end.isBefore(start)) {
            throw new IllegalArgumentException("End time of requested stream must not be before start time");
        }
        return internal.open(start, end);
    }

    /**
     * Returns the desired duration for this recording, or {@code null} if no
     * duration has been set.
     * <p>
     * The duration can only be set when the recording state is
     * {@link RecordingState#NEW}.
     *
     * @return the desired duration of the recording, or {@code null} if no
     *         duration has been set.
     */
    public Duration getDuration() {
        return internal.getDuration();
    }

    /**
     * Sets a duration for how long a recording should run before it's stopped.
     * <p>
     * By default a recording has no duration (<code>null</code>).
     *
     * @param duration the duration or {@code null} if no duration should be
     *        used
     *
     * @throws IllegalStateException if recording is in stopped or closed state
     */
    public void setDuration(Duration duration) {
        internal.setDuration(duration);
    }

    /**
     * Enables the event with the specified name.
     * <p>
     * If there are multiple events with same name, which can be the case if the
     * same class is loaded in different class loaders, all events matching the
     * name will be enabled. To enable a specific class, use the
     * {@link #enable(Class)} method or a String representation of the event
     * type id.
     *
     * @param name the settings for the event, not {@code null}
     *
     * @return an event setting for further configuration, not {@code null}
     *
     * @see EventType
     */
    public EventSettings enable(String name) {
        Objects.requireNonNull(name);
        RecordingSettings rs = new RecordingSettings(this, name);
        rs.with("enabled", "true");
        return rs;
    }

    /**
     * Disables event with the specified name.
     * <p>
     * If there are multiple events with same name, which can be the case if the
     * same class is loaded in different class loaders, all events matching the
     * name will be disabled. To disable a specific class, use the
     * {@link #disable(Class)} method or a String representation of the event
     * type id.
     *
     * @param name the settings for the event, not {@code null}
     *
     * @return an event setting for further configuration, not {@code null}
     *
     */
    public EventSettings disable(String name) {
        Objects.requireNonNull(name);
        RecordingSettings rs = new RecordingSettings(this, name);
        rs.with("enabled", "false");
        return rs;
    }

    /**
     * Enables event.
     *
     * @param eventClass the event to enable, not {@code null}
     *
     * @throws IllegalArgumentException if {@code eventClass} is an abstract
     *         class or not a subclass of {@link Event}
     *
     * @return an event setting for further configuration, not {@code null}
     */
    public EventSettings enable(Class<? extends Event> eventClass) {
        Objects.requireNonNull(eventClass);
        RecordingSettings rs = new RecordingSettings(this, eventClass);
        rs.with("enabled", "true");
        return rs;
    }

    /**
     * Disables event.
     *
     * @param eventClass the event to enable, not {@code null}
     *
     * @throws IllegalArgumentException if {@code eventClass} is an abstract
     *         class or not a subclass of {@link Event}
     *
     * @return an event setting for further configuration, not {@code null}
     *
     */
    public EventSettings disable(Class<? extends Event> eventClass) {
        Objects.requireNonNull(eventClass);
        RecordingSettings rs = new RecordingSettings(this, eventClass);
        rs.with("enabled", "false");
        return rs;
    }

    // package private
    PlatformRecording getInternal() {
        return internal;
    }

    private void setSetting(String id, String value) {
        Objects.requireNonNull(id);
        Objects.requireNonNull(value);
        internal.setSetting(id, value);
    }

}

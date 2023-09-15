/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package me.zhanghai.android.libarchive;

import android.system.ErrnoException;
import android.system.Os;

import java.nio.ByteBuffer;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

public class Archive {

    public static final int ERRNO_EOF = 1;
    public static final int ERRNO_OK = 0;
    public static final int ERRNO_RETRY = -10;
    public static final int ERRNO_WARN =-20;
    public static final int ERRNO_FAILED = -25;
    public static final int ERRNO_FATAL = -30;

    public static final int FILTER_NONE = 0;
    public static final int FILTER_GZIP = 1;
    public static final int FILTER_BZIP2 = 2;
    public static final int FILTER_COMPRESS = 3;
    public static final int FILTER_PROGRAM = 4;
    public static final int FILTER_LZMA = 5;
    public static final int FILTER_XZ = 6;
    public static final int FILTER_UU = 7;
    public static final int FILTER_RPM = 8;
    public static final int FILTER_LZIP = 9;
    public static final int FILTER_LRZIP = 10;
    public static final int FILTER_LZOP = 11;
    public static final int FILTER_GRZIP = 12;
    public static final int FILTER_LZ4 = 13;
    public static final int FILTER_ZSTD = 14;

    public static final int FORMAT_BASE_MASK = 0xff0000;
    public static final int FORMAT_CPIO = 0x10000;
    public static final int FORMAT_CPIO_POSIX = FORMAT_CPIO | 1;
    public static final int FORMAT_CPIO_BIN_LE = FORMAT_CPIO | 2;
    public static final int FORMAT_CPIO_BIN_BE = FORMAT_CPIO | 3;
    public static final int FORMAT_CPIO_SVR4_NOCRC = FORMAT_CPIO | 4;
    public static final int FORMAT_CPIO_SVR4_CRC = FORMAT_CPIO | 5;
    public static final int FORMAT_CPIO_AFIO_LARGE = FORMAT_CPIO | 6;
    public static final int FORMAT_CPIO_PWB = FORMAT_CPIO | 7;
    public static final int FORMAT_SHAR = 0x20000;
    public static final int FORMAT_SHAR_BASE = FORMAT_SHAR | 1;
    public static final int FORMAT_SHAR_DUMP = FORMAT_SHAR | 2;
    public static final int FORMAT_TAR = 0x30000;
    public static final int FORMAT_TAR_USTAR = FORMAT_TAR | 1;
    public static final int FORMAT_TAR_PAX_INTERCHANGE = FORMAT_TAR | 2;
    public static final int FORMAT_TAR_PAX_RESTRICTED = FORMAT_TAR | 3;
    public static final int FORMAT_TAR_GNUTAR = FORMAT_TAR | 4;
    public static final int FORMAT_ISO9660 = 0x40000;
    public static final int FORMAT_ISO9660_ROCKRIDGE = FORMAT_ISO9660 | 1;
    public static final int FORMAT_ZIP = 0x50000;
    public static final int FORMAT_EMPTY = 0x60000;
    public static final int FORMAT_AR = 0x70000;
    public static final int FORMAT_AR_GNU = FORMAT_AR | 1;
    public static final int FORMAT_AR_BSD = FORMAT_AR | 2;
    public static final int FORMAT_MTREE = 0x80000;
    public static final int FORMAT_RAW = 0x90000;
    public static final int FORMAT_XAR = 0xA0000;
    public static final int FORMAT_LHA = 0xB0000;
    public static final int FORMAT_CAB = 0xC0000;
    public static final int FORMAT_RAR = 0xD0000;
    public static final int FORMAT_7ZIP = 0xE0000;
    public static final int FORMAT_WARC = 0xF0000;
    public static final int FORMAT_RAR_V5 = 0x100000;

    public static final int READ_FORMAT_CAPS_NONE = 0;
    /** @noinspection PointlessBitwiseExpression*/
    public static final int READ_FORMAT_CAPS_ENCRYPT_DATA = 1 << 0;
    public static final int READ_FORMAT_CAPS_ENCRYPT_METADATA = 1 << 1;

    public static final int READ_FORMAT_ENCRYPTION_UNSUPPORTED = -2;
    public static final int READ_FORMAT_ENCRYPTION_DONT_KNOW = -1;

    private static final String ENV_TMPDIR = "TMPDIR";
    private static final String PROPERTY_TMPDIR = "java.io.tmpdir";

    static {
        ensureTmpdirEnv();
        System.loadLibrary("archive-jni");
    }

    private static void ensureTmpdirEnv() {
        // The TMPDIR environment variable is required for writing formats like 7z which calls
        // mkstemp().
        // /tmp isn't available on Android, and /data/local/tmp is only accessible to Shell, so we
        // need to set it to the app data cache directory, which we have to do manually on older
        // platforms.
        // See also
        // https://android.googlesource.com/platform/frameworks/base/+/d5ccb038f69193fb63b5169d7adc5da19859c9d8
        if (Os.getenv(ENV_TMPDIR) == null) {
            String tmpdir = System.getProperty(PROPERTY_TMPDIR);
            if (tmpdir != null) {
                try {
                    Os.setenv(ENV_TMPDIR, tmpdir, true);
                } catch (ErrnoException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    static void staticInit() {}

    private Archive() {}

    public static native int versionNumber();
    @NonNull
    public static native byte[] versionString();
    @NonNull
    public static native byte[] versionDetails();

    @NonNull
    public static native byte[] zlibVersion();
    @NonNull
    public static native byte[] liblzmaVersion();
    @NonNull
    public static native byte[] bzlibVersion();
    @NonNull
    public static native byte[] liblz4Version();
    @NonNull
    public static native byte[] libzstdVersion();

    public static native long readNew() throws ArchiveException;

    public static native void readSupportFilterAll(long archive) throws ArchiveException;
    public static native void readSupportFilterByCode(long archive, int code)
            throws ArchiveException;
    public static void readSupportFilterBzip2(long archive) throws ArchiveException {
        readSupportFilterByCode(archive, FILTER_BZIP2);
    }
    public static void readSupportFilterCompress(long archive) throws ArchiveException {
        readSupportFilterByCode(archive, FILTER_COMPRESS);
    }
    public static void readSupportFilterGzip(long archive) throws ArchiveException {
        readSupportFilterByCode(archive, FILTER_GZIP);
    }
    public static void readSupportFilterGrzip(long archive) throws ArchiveException {
        readSupportFilterByCode(archive, FILTER_GRZIP);
    }
    public static void readSupportFilterLrzip(long archive) throws ArchiveException {
        readSupportFilterByCode(archive, FILTER_LRZIP);
    }
    public static void readSupportFilterLz4(long archive) throws ArchiveException {
        readSupportFilterByCode(archive, FILTER_LZ4);
    }
    public static void readSupportFilterLzip(long archive) throws ArchiveException {
        readSupportFilterByCode(archive, FILTER_LZIP);
    }
    public static void readSupportFilterLzma(long archive) throws ArchiveException {
        readSupportFilterByCode(archive, FILTER_LZMA);
    }
    public static void readSupportFilterLzop(long archive) throws ArchiveException {
        readSupportFilterByCode(archive, FILTER_LZOP);
    }
    public static void readSupportFilterNone(long archive) throws ArchiveException {
        readSupportFilterByCode(archive, FILTER_NONE);
    }
    public static void readSupportFilterProgram(long archive, @NonNull byte[] command)
            throws ArchiveException {
        readSupportFilterProgramSignature(archive, command, null);
    }
    public static native void readSupportFilterProgramSignature(long archive,
            @NonNull byte[] command, @Nullable byte[] signature) throws ArchiveException;
    public static void readSupportFilterRpm(long archive) throws ArchiveException {
        readSupportFilterByCode(archive, FILTER_RPM);
    }
    public static void readSupportFilterUu(long archive) throws ArchiveException {
        readSupportFilterByCode(archive, FILTER_UU);
    }
    public static void readSupportFilterXz(long archive) throws ArchiveException {
        readSupportFilterByCode(archive, FILTER_XZ);
    }
    public static void readSupportFilterZstd(long archive) throws ArchiveException {
        readSupportFilterByCode(archive, FILTER_ZSTD);
    }

    public static void readSupportFormat7zip(long archive) throws ArchiveException {
        readSupportFormatByCode(archive, FORMAT_7ZIP);
    }
    public static native void readSupportFormatAll(long archive) throws ArchiveException;
    public static void readSupportFormatAr(long archive) throws ArchiveException {
        readSupportFormatByCode(archive, FORMAT_AR);
    }
    public static native void readSupportFormatByCode(long archive, int code)
            throws ArchiveException;
    public static void readSupportFormatCab(long archive) throws ArchiveException {
        readSupportFormatByCode(archive, FORMAT_CAB);
    }
    public static void readSupportFormatCpio(long archive) throws ArchiveException {
        readSupportFormatByCode(archive, FORMAT_CPIO);
    }
    public static void readSupportFormatEmpty(long archive) throws ArchiveException {
        readSupportFormatByCode(archive, FORMAT_EMPTY);
    }
    public static void readSupportFormatGnutar(long archive) throws ArchiveException {
        readSupportFormatTar(archive);
    }
    public static void readSupportFormatIso9660(long archive) throws ArchiveException {
        readSupportFormatByCode(archive, FORMAT_ISO9660);
    }
    public static void readSupportFormatLha(long archive) throws ArchiveException {
        readSupportFormatByCode(archive, FORMAT_LHA);
    }
    public static void readSupportFormatMtree(long archive) throws ArchiveException {
        readSupportFormatByCode(archive, FORMAT_MTREE);
    }
    public static void readSupportFormatRar(long archive) throws ArchiveException {
        readSupportFormatByCode(archive, FORMAT_RAR);
    }
    public static void readSupportFormatRar5(long archive) throws ArchiveException {
        readSupportFormatByCode(archive, FORMAT_RAR_V5);
    }
    public static void readSupportFormatRaw(long archive) throws ArchiveException {
        readSupportFormatByCode(archive, FORMAT_RAW);
    }
    public static void readSupportFormatTar(long archive) throws ArchiveException {
        readSupportFormatByCode(archive, FORMAT_TAR);
    }
    public static void readSupportFormatWarc(long archive) throws ArchiveException {
        readSupportFormatByCode(archive, FORMAT_WARC);
    }
    public static void readSupportFormatXar(long archive) throws ArchiveException {
        readSupportFormatByCode(archive, FORMAT_XAR);
    }
    public static void readSupportFormatZip(long archive) throws ArchiveException {
        readSupportFormatByCode(archive, FORMAT_ZIP);
    }
    public static native void readSupportFormatZipStreamable(long archive) throws ArchiveException;
    public static native void readSupportFormatZipSeekable(long archive) throws ArchiveException;

    public static native void readSetFormat(long archive, int code) throws ArchiveException;
    public static native void readAppendFilter(long archive, int code)
            throws ArchiveException;
    public static void readAppendFilterProgram(long archive, @NonNull byte[] command)
            throws ArchiveException {
        readAppendFilterProgramSignature(archive, command, null);
    }
    public static native void readAppendFilterProgramSignature(long archive,
            @NonNull byte[] command, @Nullable byte[] signature) throws ArchiveException;

    public static native <T> void readSetOpenCallback(long archive,
            @Nullable OpenCallback<T> callback) throws ArchiveException;
    public static native <T> void readSetReadCallback(long archive,
            @Nullable ReadCallback<T> callback) throws ArchiveException;
    public static native <T> void readSetSeekCallback(long archive,
            @Nullable SeekCallback<?> callback) throws ArchiveException;
    public static native <T> void readSetSkipCallback(long archive,
            @Nullable SkipCallback<T> callback) throws ArchiveException;
    public static native <T> void readSetCloseCallback(long archive,
            @Nullable CloseCallback<T> callback) throws ArchiveException;
    public static native <T> void readSetSwitchCallback(long archive,
            @Nullable SwitchCallback<T> callback) throws ArchiveException;

    public static <T> void readSetCallbackData(long archive, T clientData) throws ArchiveException {
        readSetCallbackData2(archive, clientData, 0);
    }
    public static native <T> void readSetCallbackData2(long archive, T clientData, int index)
            throws ArchiveException;
    public static native <T> void readAddCallbackData(long archive, T clientData, int index)
            throws ArchiveException;
    public static native <T> void readAppendCallbackData(long archive, T clientData)
            throws ArchiveException;
    public static <T> void readPrependCallbackData(long archive, T clientData)
            throws ArchiveException {
        readAddCallbackData(archive, clientData, 0);
    }

    public static native void readOpen1(long archive) throws ArchiveException;
    public static <T> void readOpen(long archive, T clientData,
            @Nullable OpenCallback<T> openCallback, @NonNull ReadCallback<T> readCallback,
            @Nullable CloseCallback<T> closeCallback) throws ArchiveException {
        readSetCallbackData(archive, clientData);
        readSetOpenCallback(archive, openCallback);
        readSetReadCallback(archive, readCallback);
        readSetCloseCallback(archive, closeCallback);
        readOpen1(archive);
    }
    public static <T> void readOpen2(long archive, T clientData,
            @Nullable OpenCallback<T> openCallback, @NonNull ReadCallback<T> readCallback,
            @Nullable SkipCallback<T> skipCallback, @Nullable CloseCallback<T> closeCallback)
            throws ArchiveException {
        readSetCallbackData(archive, clientData);
        readSetOpenCallback(archive, openCallback);
        readSetReadCallback(archive, readCallback);
        readSetSkipCallback(archive, skipCallback);
        readSetCloseCallback(archive, closeCallback);
        readOpen1(archive);
    }
    public static native void readOpenFileName(long archive, @Nullable byte[] fileName,
            long blockSize) throws ArchiveException;
    public static native void readOpenFileNames(long archive, @NonNull byte[][] fileNames,
            long blockSize) throws ArchiveException;
    public static native void readOpenMemory(long archive, @NonNull ByteBuffer buffer)
            throws ArchiveException;
    public static native void readOpenFd(long archive, int fd, long blockSize)
            throws ArchiveException;

    public static native long readNextHeader(long archive) throws ArchiveException;
    public static native long readNextHeader2(long archive, long entry) throws ArchiveException;
    public static native long readHeaderPosition(long archive) throws ArchiveException;

    public static native int readHasEncryptedEntries(long archive);
    public static native int readFormatCapabilities(long archive);

    public static native void readData(long archive, @NonNull ByteBuffer buffer)
            throws ArchiveException;
    public static native long seekData(long archive, long offset, int whence)
            throws ArchiveException;
    public static native void readDataSkip(long archive) throws ArchiveException;
    public static native void readDataIntoFd(long archive, int fd) throws ArchiveException;

    public static native void readSetFormatOption(long archive, @Nullable byte[] module,
            @NonNull byte[] option, @Nullable byte[] value) throws ArchiveException;
    public static native void readSetFilterOption(long archive, @Nullable byte[] module,
            @NonNull byte[] option, @Nullable byte[] value) throws ArchiveException;
    public static native void readSetOption(long archive, @Nullable byte[] module,
            @NonNull byte[] option, @Nullable byte[] value) throws ArchiveException;
    public static native void readSetOptions(long archive, @NonNull byte[] options)
            throws ArchiveException;

    public static native void readAddPassphrase(long archive, @NonNull byte[] passphrase)
            throws ArchiveException;
    public static native <T> void readSetPassphraseCallback(long archive, T clientData,
            @Nullable PassphraseCallback<T> callback) throws ArchiveException;

    public static native void readClose(long archive) throws ArchiveException;
    public static void readFree(long archive) throws ArchiveException {
        free(archive);
    }

    public static native long writeNew() throws ArchiveException;
    public static native void writeSetBytesPerBlock(long archive, int bytesPerBlock)
            throws ArchiveException;
    public static native int writeGetBytesPerBlock(long archive) throws ArchiveException;
    public static native void writeSetBytesInLastBlock(long archive, int bytesInLastBlock)
            throws ArchiveException;
    public static native int writeGetBytesInLastBlock(long archive) throws ArchiveException;

    public static native void writeAddFilter(long archive, int code) throws ArchiveException;
    public static native void writeAddFilterByName(long archive, @NonNull byte[] name)
            throws ArchiveException;
    public static native void writeAddFilterB64encode(long archive) throws ArchiveException;
    public static native void writeAddFilterBzip2(long archive) throws ArchiveException;
    public static native void writeAddFilterCompress(long archive) throws ArchiveException;
    public static native void writeAddFilterGrzip(long archive) throws ArchiveException;
    public static native void writeAddFilterGzip(long archive) throws ArchiveException;
    public static native void writeAddFilterLrzip(long archive) throws ArchiveException;
    public static native void writeAddFilterLz4(long archive) throws ArchiveException;
    public static native void writeAddFilterLzip(long archive) throws ArchiveException;
    public static native void writeAddFilterLzma(long archive) throws ArchiveException;
    public static native void writeAddFilterLzop(long archive) throws ArchiveException;
    public static native void writeAddFilterNone(long archive) throws ArchiveException;
    public static native void writeAddFilterProgram(long archive, @NonNull byte[] command)
            throws ArchiveException;
    public static native void writeAddFilterUuencode(long archive) throws ArchiveException;
    public static native void writeAddFilterXz(long archive) throws ArchiveException;
    public static native void writeAddFilterZstd(long archive) throws ArchiveException;

    public static native void writeSetFormat(long archive, int format_code) throws ArchiveException;
    public static native void writeSetFormatByName(long archive, @NonNull byte[] name)
            throws ArchiveException;
    public static native void writeSetFormat7zip(long archive) throws ArchiveException;
    public static native void writeSetFormatArBsd(long archive) throws ArchiveException;
    public static native void writeSetFormatArSvr4(long archive) throws ArchiveException;
    public static native void writeSetFormatCpio(long archive) throws ArchiveException;
    public static native void writeSetFormatCpioBin(long archive) throws ArchiveException;
    public static native void writeSetFormatCpioNewc(long archive) throws ArchiveException;
    public static native void writeSetFormatCpioOdc(long archive) throws ArchiveException;
    public static native void writeSetFormatCpioPwb(long archive) throws ArchiveException;
    public static native void writeSetFormatGnutar(long archive) throws ArchiveException;
    public static native void writeSetFormatIso9660(long archive) throws ArchiveException;
    public static native void writeSetFormatMtree(long archive) throws ArchiveException;
    public static native void writeSetFormatMtreeClassic(long archive) throws ArchiveException;
    public static native void writeSetFormatPax(long archive) throws ArchiveException;
    public static native void writeSetFormatPaxRestricted(long archive) throws ArchiveException;
    public static native void writeSetFormatRaw(long archive) throws ArchiveException;
    public static native void writeSetFormatShar(long archive) throws ArchiveException;
    public static native void writeSetFormatSharDump(long archive) throws ArchiveException;
    public static native void writeSetFormatUstar(long archive) throws ArchiveException;
    public static native void writeSetFormatV7tar(long archive) throws ArchiveException;
    public static native void writeSetFormatWarc(long archive) throws ArchiveException;
    public static native void writeSetFormatXar(long archive) throws ArchiveException;
    public static native void writeSetFormatZip(long archive) throws ArchiveException;

    public static native void writeSetFormatFilterByExt(long archive, @NonNull byte[] fileName)
            throws ArchiveException;
    public static native void writeSetFormatFilterByExtDef(long archive, @NonNull byte[] fileName,
            @NonNull byte[] defaultExtension) throws ArchiveException;

    public static native void writeZipSetCompressionDeflate(long archive) throws ArchiveException;
    public static native void writeZipSetCompressionStore(long archive) throws ArchiveException;

    public static <T> void writeOpen(long archive, T clientData,
            @Nullable OpenCallback<T> openCallback, @NonNull WriteCallback<T> writeCallback,
            @Nullable CloseCallback<T> closeCallback) throws ArchiveException {
        writeOpen2(archive, clientData, openCallback, writeCallback, closeCallback, null);
    }
    public static native <T> void writeOpen2(long archive, T clientData,
            @Nullable OpenCallback<T> openCallback, @NonNull WriteCallback<T> writeCallback,
            @Nullable CloseCallback<T> closeCallback, @Nullable FreeCallback<T> freeCallback)
            throws ArchiveException;
    public static native void writeOpenFd(long archive, int fd) throws ArchiveException;
    public static native void writeOpenFileName(long archive, @NonNull byte[] fileName)
            throws ArchiveException;
    public static native void writeOpenMemory(long archive, @NonNull ByteBuffer buffer)
            throws ArchiveException;

    public static native void writeHeader(long archive, long entry) throws ArchiveException;
    public static native void writeData(long archive, @NonNull ByteBuffer buffer)
            throws ArchiveException;

    public static native void writeFinishEntry(long archive) throws ArchiveException;
    public static native void writeClose(long archive) throws ArchiveException;
    public static native void writeFail(long archive) throws ArchiveException;
    public static void writeFree(long archive) throws ArchiveException {
        free(archive);
    }

    public static native void writeSetFormatOption(long archive, @Nullable byte[] module,
            @NonNull byte[] option, @Nullable byte[] value) throws ArchiveException;
    public static native void writeSetFilterOption(long archive, @Nullable byte[] module,
            @NonNull byte[] option, @Nullable byte[] value) throws ArchiveException;
    public static native void writeSetOption(long archive, @Nullable byte[] module,
            @NonNull byte[] option, @Nullable byte[] value) throws ArchiveException;
    public static native void writeSetOptions(long archive, @NonNull byte[] options)
            throws ArchiveException;

    public static native void writeSetPassphrase(long archive, @NonNull byte[] passphrase)
            throws ArchiveException;
    public static native <T> void writeSetPassphraseCallback(long archive, T clientData,
            @Nullable PassphraseCallback<T> callback) throws ArchiveException;

    public static native void free(long archive) throws ArchiveException;

    public static native int filterCount(long archive);
    public static native long filterBytes(long archive, int index);
    public static native int filterCode(long archive, int index);
    @Nullable
    public static native byte[] filterName(long archive, int index);

    public static native int errno(long archive);
    @Nullable
    public static native byte[] errorString(long archive);
    @Nullable
    public static native byte[] formatName(long archive);
    public static native int format(long archive);
    public static native void clearError(long archive);
    public static native void setError(long archive, int number, @Nullable byte[] string);
    public static native void copyError(long destination, long source);
    public static native int fileCount(long archive);
    @Nullable
    public static native byte[] charset(long archive);
    public static native void setCharset(long archive, @Nullable byte[] charset)
            throws ArchiveException;

    public interface ReadCallback<T> {
        @Nullable
        ByteBuffer onRead(long archive, T clientData) throws ArchiveException;
    }

    public interface SkipCallback<T> {
        long onSkip(long archive, T clientData, long request) throws ArchiveException;
    }

    public interface SeekCallback<T> {
        long onSeek(long archive, T clientData, long offset, int whence) throws ArchiveException;
    }

    public interface WriteCallback<T> {
        void onWrite(long archive, T clientData, @NonNull ByteBuffer buffer)
                throws ArchiveException;
    }

    public interface OpenCallback<T> {
        void onOpen(long archive, T clientData) throws ArchiveException;
    }

    public interface CloseCallback<T> {
        void onClose(long archive, T clientData) throws ArchiveException;
    }

    public interface FreeCallback<T> {
        void onFree(long archive, T clientData) throws ArchiveException;
    }

    public interface SwitchCallback<T> {
        void onSwitch(long archive, T clientData1, T clientData2) throws ArchiveException;
    }

    public interface PassphraseCallback<T> {
        @Nullable
        byte[] onPassphrase(long archive, T clientData) throws ArchiveException;
    }
}

# libarchive-android

[![Android CI status](https://github.com/zhanghai/libarchive-android/workflows/Android%20CI/badge.svg)](https://github.com/zhanghai/libarchive-android/actions)

[libarchive](https://github.com/libarchive/libarchive) built with Android NDK, packaged as an
Android library with some Java binding.

The bundled [libarchive](https://github.com/libarchive/libarchive) is built with
[libz](https://developer.android.com/ndk/guides/stable_apis#zlib_compression),
[libbz2](https://gitlab.com/bzip2/bzip2), [liblzma](https://github.com/tukaani-project/xz),
[liblz4](https://github.com/lz4/lz4), [libzstd](https://github.com/facebook/zstd) and
[libmbedcrypto](https://github.com/Mbed-TLS/mbedtls).

This is not an officially supported Google product.

## Integration

Gradle:

```gradle
implementation 'me.zhanghai.android.libarchive:library:1.1.6'
```

## Usage

See [`Archive.java`](library/src/main/java/me/zhanghai/android/libarchive/Archive.java) and
[`ArchiveEntry.java`](library/src/main/java/me/zhanghai/android/libarchive/ArchiveEntry.java), which
contain the Java bindings for
[`archive.h`](https://github.com/libarchive/libarchive/blob/master/libarchive/archive.h) and
[`archive_entry.h`](https://github.com/libarchive/libarchive/blob/master/libarchive/archive_entry.h)
.

See also
[`MainActivity.java`](sample/src/main/java/me/zhanghai/android/libarchive/sample/MainActivity.java)
for an example on how to use this library to read archive files.

# libarchive-android

[![Android CI status](https://github.com/zhanghai/libarchive-android/workflows/Android%20CI/badge.svg)](https://github.com/zhanghai/libarchive-android/actions)

[libarchive](https://github.com/libarchive/libarchive) built with Android NDK, packaged as an Android library with some Java binding.

This is not an officially supported Google product.

## Integration

Gradle:

```gradle
implementation 'me.zhanghai.android.libarchive:library:1.0.0'
```

## Usage

See [`Archive.java`](library/src/main/java/me/zhanghai/android/libarchive/Archive.java) and [`ArchiveEntry.java`](library/src/main/java/me/zhanghai/android/libarchive/ArchiveEntry.java).

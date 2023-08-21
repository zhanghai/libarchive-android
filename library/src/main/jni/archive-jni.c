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

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include <jni.h>

#include <android/log.h>

#include <archive.h>
#include <archive_entry.h>

#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, LOG_TAG, __VA_ARGS__)
#define ALOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#define LOG_TAG "archive-jni"

struct ArchiveJniData {
    jbyteArray openMemoryJavaArray;
    jbyte *openMemoryArray;
    jint openMemoryArrayReleaseMode;
    jobject writeOpenMemoryJavaBuffer;
    jint writeOpenMemoryPosition;
    size_t writeOpenMemoryUsed;
    bool hasReadClientData;
    jobject writeClientData;
    jobject readCallback;
    jbyteArray readJavaArray;
    jbyte *readArray;
    jobject skipCallback;
    jobject seekCallback;
    jobject writeCallback;
    jobject openCallback;
    jobject closeCallback;
    jobject freeCallback;
    jobject switchCallback;
    jobject passphraseClientData;
    jobject passphraseCallback;
    char *passphrase;
};

static char *mallocStringFromBytes(JNIEnv *env, jbyteArray javaBytes) {
    if (!javaBytes) {
        return NULL;
    }
    void *bytes = (*env)->GetByteArrayElements(env, javaBytes, NULL);
    size_t length = (*env)->GetArrayLength(env, javaBytes);
    char *string = malloc(length + 1);
    if (!string) {
        return NULL;
    }
    memcpy(string, bytes, length);
    (*env)->ReleaseByteArrayElements(env, javaBytes, bytes, JNI_ABORT);
    string[length] = '\0';
    return string;
}

static jbyteArray newBytesFromString(JNIEnv *env, const char *string) {
    if (!string) {
        return NULL;
    }
    jsize length = (jsize) strlen(string);
    jbyteArray bytes = (*env)->NewByteArray(env, length);
    if (!bytes) {
        return NULL;
    }
    const void *stringBytes = string;
    (*env)->SetByteArrayRegion(env, bytes, 0, length, stringBytes);
    return bytes;
}

static char **mallocStringArrayFromBytesArray(JNIEnv *env, jobjectArray bytesArray) {
    jsize length = (*env)->GetArrayLength(env, bytesArray);
    char **stringArray = malloc((length + 1) * sizeof(*stringArray));
    for (jsize i = 0; i < length; ++i) {
        jbyteArray bytes = (*env)->GetObjectArrayElement(env, bytesArray, i);
        char *string = mallocStringFromBytes(env, bytes);
        if (bytes && !string) {
            free(stringArray);
            return NULL;
        }
        stringArray[i] = string;
    }
    stringArray[length] = NULL;
    return stringArray;
}

static jclass findClass(JNIEnv *env, const char *name) {
    jclass localClass = (*env)->FindClass(env, name);
    if (!localClass) {
        ALOGE("Failed to find class '%s'", name);
        abort();
    }
    jclass globalClass = (*env)->NewGlobalRef(env, localClass);
    (*env)->DeleteLocalRef(env, localClass);
    if (!globalClass) {
        ALOGE("Failed to create a global reference for '%s'", name);
        abort();
    }
    return globalClass;
}

static jfieldID findField(JNIEnv *env, jclass clazz, const char *name, const char *signature) {
    jfieldID field = (*env)->GetFieldID(env, clazz, name, signature);
    if (!field) {
        ALOGE("Failed to find field '%s' '%s'", name, signature);
        abort();
    }
    return field;
}

static jmethodID findMethod(JNIEnv *env, jclass clazz, const char *name, const char *signature) {
    jmethodID method = (*env)->GetMethodID(env, clazz, name, signature);
    if (!method) {
        ALOGE("Failed to find method '%s' '%s'", name, signature);
        abort();
    }
    return method;
}

static JavaVM *gVm;

JNIEXPORT jint JNI_OnLoad(JavaVM *vm, void *reserved) {
    gVm = vm;
    return JNI_VERSION_1_6;
}

static JNIEnv *getEnv() {
    JNIEnv *env = NULL;
    (*gVm)->GetEnv(gVm, (void **) &env, JNI_VERSION_1_6);
    if (!env) {
        ALOGE("Failed to get JNIEnv");
    }
    return env;
}

static jclass getArchiveExceptionClass(JNIEnv *env) {
    static jclass clazz = NULL;
    if (!clazz) {
        clazz = findClass(env, "me/zhanghai/android/libarchive/ArchiveException");
    }
    return clazz;
}

static bool isArchiveException(JNIEnv *env, jthrowable throwable) {
    return (*env)->IsInstanceOf(env, throwable, getArchiveExceptionClass(env));
}

static jint getArchiveExceptionCode(JNIEnv *env, jthrowable exception) {
    static jmethodID method = NULL;
    if (!method) {
        method = findMethod(env, getArchiveExceptionClass(env), "getCode", "()I");
    }
    return (*env)->CallIntMethod(env, exception, method);
}

static jstring getThrowableMessage(JNIEnv *env, jthrowable exception) {
    static jclass clazz = NULL;
    if (!clazz) {
        clazz = findClass(env, "java/lang/Throwable");
    }
    static jmethodID method = NULL;
    if (!method) {
        method = findMethod(env, clazz, "getMessage", "()Ljava/lang/String;");
    }
    return (*env)->CallObjectMethod(env, exception, method);
}

static bool setArchiveErrorFromException(JNIEnv *env, struct archive* archive) {
    jthrowable throwable = (*env)->ExceptionOccurred(env);
    if (!throwable) {
        return false;
    }
    (*env)->ExceptionClear(env);
    int errorCode = ARCHIVE_FATAL;
    if (isArchiveException(env, throwable)) {
        errorCode = getArchiveExceptionCode(env, throwable);
    }
    jstring javaErrorMessage = getThrowableMessage(env, throwable);
    (*env)->DeleteLocalRef(env, throwable);
    const char *errorMessage = NULL;
    if (!javaErrorMessage) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
    } else {
        errorMessage = (*env)->GetStringUTFChars(env, javaErrorMessage, NULL);
        if (!errorMessage) {
            (*env)->ExceptionDescribe(env);
            (*env)->ExceptionClear(env);
        }
    }
    if (errorMessage) {
        archive_set_error(archive, errorCode, "%s", errorMessage);
        (*env)->ReleaseStringUTFChars(env, javaErrorMessage, errorMessage);
    } else {
        archive_set_error(archive, errorCode, NULL);
    }
    if (javaErrorMessage) {
        (*env)->DeleteLocalRef(env, javaErrorMessage);
    }
    return true;
}

static void throwArchiveException(JNIEnv* env, int code, const char *message) {
    jclass clazz = getArchiveExceptionClass(env);
    static jmethodID constructor3 = NULL;
    if (!constructor3) {
        constructor3 = findMethod(env, clazz, "<init>",
                "(ILjava/lang/String;Ljava/lang/Throwable;)V");
    }
    static jmethodID constructor2 = NULL;
    if (!constructor2) {
        constructor2 = findMethod(env, clazz, "<init>", "(ILjava/lang/String;)V");
    }
    jthrowable cause = (*env)->ExceptionOccurred(env);
    if (cause) {
        (*env)->ExceptionClear(env);
    }
    jstring javaMessage = NULL;
    if (message) {
        javaMessage = (*env)->NewStringUTF(env, message);
        if (!javaMessage) {
            (*env)->ExceptionDescribe(env);
            (*env)->ExceptionClear(env);
        }
    }
    jobject exception;
    if (cause) {
        exception = (*env)->NewObject(env, clazz, constructor3, code, javaMessage, cause);
        (*env)->DeleteLocalRef(env, cause);
    } else {
        exception = (*env)->NewObject(env, clazz, constructor2, code, javaMessage);
    }
    (*env)->DeleteLocalRef(env, javaMessage);
    if (!exception) {
        (*env)->ExceptionDescribe(env);
        return;
    }
    (*env)->Throw(env, exception);
    (*env)->DeleteLocalRef(env, exception);
}

static void throwArchiveExceptionFromError(JNIEnv* env, struct archive *archive) {
    int code = archive_errno(archive);
    const char *message = archive_error_string(archive);
    throwArchiveException(env, code, message);
}

static jobject callArchiveReadCallbackOnRead(JNIEnv *env, jobject callback, jlong archive,
        jobject clientData) {
    static jclass clazz = NULL;
    if (!clazz) {
        clazz = findClass(env, "me/zhanghai/android/libarchive/Archive$ReadCallback");
    }
    static jmethodID method = NULL;
    if (!method) {
        method = findMethod(env, clazz, "onRead", "(JLjava/lang/Object;)Ljava/nio/ByteBuffer;");
    }
    return (*env)->CallObjectMethod(env, callback, method, archive, clientData);
}

static jlong callArchiveSkipCallbackOnSkip(JNIEnv *env, jobject callback, jlong archive,
        jobject clientData, jlong request) {
    static jclass clazz = NULL;
    if (!clazz) {
        clazz = findClass(env, "me/zhanghai/android/libarchive/Archive$SkipCallback");
    }
    static jmethodID method = NULL;
    if (!method) {
        method = findMethod(env, clazz, "onSkip", "(JLjava/lang/Object;J)J");
    }
    return (*env)->CallLongMethod(env, callback, method, archive, clientData, request);
}

static jlong callArchiveSeekCallbackOnSeek(JNIEnv *env, jobject callback, jlong archive,
        jobject clientData, jlong offset, jint whence) {
    static jclass clazz = NULL;
    if (!clazz) {
        clazz = findClass(env, "me/zhanghai/android/libarchive/Archive$SeekCallback");
    }
    static jmethodID method = NULL;
    if (!method) {
        method = findMethod(env, clazz, "onSeek", "(JLjava/lang/Object;JI)J");
    }
    return (*env)->CallLongMethod(env, callback, method, archive, clientData, offset, whence);
}

static void callArchiveWriteCallbackOnWrite(JNIEnv *env, jobject callback, jlong archive,
        jobject clientData, jobject buffer) {
    static jclass clazz = NULL;
    if (!clazz) {
        clazz = findClass(env, "me/zhanghai/android/libarchive/Archive$WriteCallback");
    }
    static jmethodID method = NULL;
    if (!method) {
        method = findMethod(env, clazz, "onWrite", "(JLjava/lang/Object;Ljava/nio/ByteBuffer;)V");
    }
    (*env)->CallVoidMethod(env, callback, method, archive, clientData, buffer);
}

static void callArchiveOpenCallbackOnOpen(JNIEnv *env, jobject callback, jlong archive,
        jobject clientData) {
    static jclass clazz = NULL;
    if (!clazz) {
        clazz = findClass(env, "me/zhanghai/android/libarchive/Archive$OpenCallback");
    }
    static jmethodID method = NULL;
    if (!method) {
        method = findMethod(env, clazz, "onOpen", "(JLjava/lang/Object;)V");
    }
    (*env)->CallVoidMethod(env, callback, method, archive, clientData);
}

static void callArchiveCloseCallbackOnClose(JNIEnv *env, jobject callback, jlong archive,
        jobject clientData) {
    static jclass clazz = NULL;
    if (!clazz) {
        clazz = findClass(env, "me/zhanghai/android/libarchive/Archive$CloseCallback");
    }
    static jmethodID method = NULL;
    if (!method) {
        method = findMethod(env, clazz, "onClose", "(JLjava/lang/Object;)V");
    }
    (*env)->CallVoidMethod(env, callback, method, archive, clientData);
}

static void callArchiveFreeCallbackOnFree(JNIEnv *env, jobject callback, jlong archive,
        jobject clientData) {
    static jclass clazz = NULL;
    if (!clazz) {
        clazz = findClass(env, "me/zhanghai/android/libarchive/Archive$FreeCallback");
    }
    static jmethodID method = NULL;
    if (!method) {
        method = findMethod(env, clazz, "onFree", "(JLjava/lang/Object;)V");
    }
    (*env)->CallVoidMethod(env, callback, method, archive, clientData);
}

static void callArchiveSwitchCallbackOnSwitch(JNIEnv *env, jobject callback, jlong archive,
        jobject clientData1, jobject clientData2) {
    static jclass clazz = NULL;
    if (!clazz) {
        clazz = findClass(env, "me/zhanghai/android/libarchive/Archive$SwitchCallback");
    }
    static jmethodID method = NULL;
    if (!method) {
        method = findMethod(env, clazz, "onSwitch", "(JLjava/lang/Object;Ljava/lang/Object;)V");
    }
    (*env)->CallVoidMethod(env, callback, method, archive, clientData1, clientData2);
}

static jbyteArray callArchivePassphraseCallbackOnPassphrase(JNIEnv *env, jobject callback,
        jlong archive, jobject clientData) {
    static jclass clazz = NULL;
    if (!clazz) {
        clazz = findClass(env, "me/zhanghai/android/libarchive/Archive$PassphraseCallback");
    }
    static jmethodID method = NULL;
    if (!method) {
        method = findMethod(env, clazz, "onPassphrase", "(JLjava/lang/Object;)[B");
    }
    return (*env)->CallObjectMethod(env, callback, method, archive, clientData);
}

static jclass getByteBufferClass(JNIEnv *env) {
    static jclass clazz = NULL;
    if (!clazz) {
        clazz = findClass(env, "java/nio/ByteBuffer");
    }
    return clazz;
}

static jboolean getByteBufferHasArray(JNIEnv *env, jobject byteBuffer) {
    jclass clazz = getByteBufferClass(env);
    static jmethodID method = NULL;
    if (!method) {
        method = findMethod(env, clazz, "hasArray", "()Z");
    }
    return (*env)->CallBooleanMethod(env, byteBuffer, method);
}

static jbyteArray getByteBufferArray(JNIEnv *env, jobject byteBuffer) {
    jclass clazz = getByteBufferClass(env);
    static jmethodID method = NULL;
    if (!method) {
        method = findMethod(env, clazz, "array", "()[B");
    }
    return (*env)->CallObjectMethod(env, byteBuffer, method);
}

static jint getByteBufferArrayOffset(JNIEnv *env, jobject byteBuffer) {
    jclass clazz = getByteBufferClass(env);
    static jmethodID method = NULL;
    if (!method) {
        method = findMethod(env, clazz, "arrayOffset", "()I");
    }
    return (*env)->CallIntMethod(env, byteBuffer, method);
}

static jint getByteBufferLimit(JNIEnv *env, jobject byteBuffer) {
    jclass clazz = getByteBufferClass(env);
    static jmethodID method = NULL;
    if (!method) {
        method = findMethod(env, clazz, "limit", "()I");
    }
    return (*env)->CallIntMethod(env, byteBuffer, method);
}

static jint getByteBufferPosition(JNIEnv *env, jobject byteBuffer) {
    jclass clazz = getByteBufferClass(env);
    static jmethodID method = NULL;
    if (!method) {
        method = findMethod(env, clazz, "position", "()I");
    }
    return (*env)->CallIntMethod(env, byteBuffer, method);
}

static const char *getByteBufferBuffer(
        JNIEnv *env, jobject javaBuffer, bool newGlobalRef, jint *outPosition,
        jbyteArray *outJavaArray, jbyte** outArray, void **outBuffer, int32_t *outBufferSize) {
    int32_t position = getByteBufferPosition(env, javaBuffer);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        return "ByteBuffer.position";
    }
    int32_t limit = getByteBufferLimit(env, javaBuffer);
    if ((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
        return "ByteBuffer.limit";
    }
    uint8_t *address = (*env)->GetDirectBufferAddress(env, javaBuffer);
    if (address) {
        *outJavaArray = NULL;
        *outArray = NULL;
        *outBuffer = address + position;
    } else {
        bool hasArray = getByteBufferHasArray(env, javaBuffer);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionDescribe(env);
            (*env)->ExceptionClear(env);
            return "ByteBuffer.hasArray";
        }
        if (!hasArray) {
            return "!(GetDirectBufferAddress() || ByteBuffer.hasArray())";
        }
        ptrdiff_t arrayOffset = getByteBufferArrayOffset(env, javaBuffer);
        if ((*env)->ExceptionCheck(env)) {
            (*env)->ExceptionDescribe(env);
            (*env)->ExceptionClear(env);
            return "ByteBuffer.arrayOffset";
        }
        jbyteArray javaArray = getByteBufferArray(env, javaBuffer);
        if (!javaArray) {
            (*env)->ExceptionDescribe(env);
            (*env)->ExceptionClear(env);
            return "ByteBuffer.array";
        }
        if (newGlobalRef) {
            jbyteArray javaArrayRef = (*env)->NewGlobalRef(env, javaArray);
            (*env)->DeleteLocalRef(env, javaArray);
            if (!javaArrayRef) {
                return "NewGlobalRef";
            }
            javaArray = javaArrayRef;
        }
        jbyte *array = (*env)->GetByteArrayElements(env, javaArray, NULL);
        if (!array) {
            if (newGlobalRef) {
                (*env)->DeleteGlobalRef(env, javaArray);
            } else {
                (*env)->DeleteLocalRef(env, javaArray);
            }
            return "GetByteArrayElements";
        }
        *outJavaArray = javaArray;
        *outArray = array;
        *outBuffer = array + arrayOffset + position;
    }
    if (outPosition) {
        *outPosition = position;
    }
    *outBufferSize = limit - position;
    return NULL;
}

static jobject newHeapByteBufferFromBuffer(JNIEnv *env, const void *buffer, size_t bufferSize,
        bool clearException) {
    jclass clazz = getByteBufferClass(env);
    static jmethodID method = NULL;
    if (!method) {
        method = findMethod(env, clazz, "allocate", "(I)Ljava/nio/ByteBuffer;");
    }
    jint javaBufferSize = (jint) bufferSize;
    jobject javaBuffer = (*env)->CallStaticObjectMethod(env, clazz, method, javaBufferSize);
    if (!javaBuffer) {
        if (clearException) {
            (*env)->ExceptionDescribe(env);
            (*env)->ExceptionClear(env);
        }
        return NULL;
    }
    jbyteArray javaArray = getByteBufferArray(env, javaBuffer);
    if (!javaArray) {
        if (clearException) {
            (*env)->ExceptionDescribe(env);
            (*env)->ExceptionClear(env);
        }
        (*env)->DeleteLocalRef(env, javaBuffer);
        return NULL;
    }
    jbyte *javaArrayBytes = (*env)->GetByteArrayElements(env, javaArray, NULL);
    if (!javaArrayBytes) {
        (*env)->DeleteLocalRef(env, javaArray);
        (*env)->DeleteLocalRef(env, javaBuffer);
        return NULL;
    }
    memcpy(javaArrayBytes, buffer, bufferSize);
    (*env)->ReleaseByteArrayElements(env, javaArray, javaArrayBytes, 0);
    (*env)->DeleteLocalRef(env, javaArray);
    return javaBuffer;
}

static void setByteBufferPosition(JNIEnv *env, jobject byteBuffer, jint position) {
    jclass clazz = getByteBufferClass(env);
    static jmethodID method = NULL;
    if (!method) {
        method = findMethod(env, clazz, "position", "(I)Ljava/nio/Buffer;");
    }
    jobject buffer = (*env)->CallObjectMethod(env, byteBuffer, method, position);
    (*env)->DeleteLocalRef(env, buffer);
}

static jclass getStructTimespecClass(JNIEnv *env) {
    static jclass clazz = NULL;
    if (!clazz) {
        clazz = findClass(env, "me/zhanghai/android/libarchive/ArchiveEntry$StructTimespec");
    }
    return clazz;
}

static jfieldID getStructTimespecTvSecField(JNIEnv *env) {
    static jfieldID field = NULL;
    if (!field) {
        field = findField(env, getStructTimespecClass(env), "tvSec", "J");
    }
    return field;
}

static jfieldID getStructTimespecTvNsecField(JNIEnv *env) {
    static jfieldID field = NULL;
    if (!field) {
        field = findField(env, getStructTimespecClass(env), "tvNsec", "J");
    }
    return field;
}

static jobject newStructTimespec(JNIEnv *env, const struct timespec *timespec) {
    jclass clazz = getStructTimespecClass(env);
    static jmethodID constructor = NULL;
    if (!constructor) {
        constructor = findMethod(env, clazz, "<init>", "()V");
    }
    jobject javaTimespec = (*env)->NewObject(env, clazz, constructor);
    if (!javaTimespec) {
        return NULL;
    }
    (*env)->SetLongField(env, javaTimespec, getStructTimespecTvSecField(env), timespec->tv_sec);
    (*env)->SetLongField(env, javaTimespec, getStructTimespecTvNsecField(env), timespec->tv_nsec);
    return javaTimespec;
}

static void readStructTimespec(JNIEnv *env, jobject javaTimespec, struct timespec *timespec) {
    if (!javaTimespec) {
        return;
    }
    timespec->tv_sec = (time_t) (*env)->GetLongField(env, javaTimespec, getStructTimespecTvSecField(
            env));
    timespec->tv_nsec = (long) (*env)->GetLongField(env, javaTimespec, getStructTimespecTvNsecField(
            env));
}

static jclass getStructStatClass(JNIEnv *env) {
    static jclass clazz = NULL;
    if (!clazz) {
        clazz = findClass(env, "me/zhanghai/android/libarchive/ArchiveEntry$StructStat");
    }
    return clazz;
}

static jfieldID getStructStatStDevField(JNIEnv *env) {
    static jfieldID field = NULL;
    if (!field) {
        field = findField(env, getStructStatClass(env), "stDev", "J");
    }
    return field;
}

static jfieldID getStructStatStModeField(JNIEnv *env) {
    static jfieldID field = NULL;
    if (!field) {
        field = findField(env, getStructStatClass(env), "stMode", "I");
    }
    return field;
}

static jfieldID getStructStatStNlinkField(JNIEnv *env) {
    static jfieldID field = NULL;
    if (!field) {
        field = findField(env, getStructStatClass(env), "stNlink", "I");
    }
    return field;
}

static jfieldID getStructStatStUidField(JNIEnv *env) {
    static jfieldID field = NULL;
    if (!field) {
        field = findField(env, getStructStatClass(env), "stUid", "I");
    }
    return field;
}

static jfieldID getStructStatStGidField(JNIEnv *env) {
    static jfieldID field = NULL;
    if (!field) {
        field = findField(env, getStructStatClass(env), "stGid", "I");
    }
    return field;
}

static jfieldID getStructStatStRdevField(JNIEnv *env) {
    static jfieldID field = NULL;
    if (!field) {
        field = findField(env, getStructStatClass(env), "stRdev", "J");
    }
    return field;
}

static jfieldID getStructStatStSizeField(JNIEnv *env) {
    static jfieldID field = NULL;
    if (!field) {
        field = findField(env, getStructStatClass(env), "stSize", "J");
    }
    return field;
}

static jfieldID getStructStatStBlksizeField(JNIEnv *env) {
    static jfieldID field = NULL;
    if (!field) {
        field = findField(env, getStructStatClass(env), "stBlksize", "J");
    }
    return field;
}

static jfieldID getStructStatStBlocksField(JNIEnv *env) {
    static jfieldID field = NULL;
    if (!field) {
        field = findField(env, getStructStatClass(env), "stBlocks", "J");
    }
    return field;
}

static jfieldID getStructStatStAtimField(JNIEnv *env) {
    static jfieldID field = NULL;
    if (!field) {
        field = findField(env, getStructStatClass(env), "stAtim",
                "Lme/zhanghai/android/libarchive/ArchiveEntry$StructTimespec;");
    }
    return field;
}

static jfieldID getStructStatStMtimField(JNIEnv *env) {
    static jfieldID field = NULL;
    if (!field) {
        field = findField(env, getStructStatClass(env), "stMtim",
                "Lme/zhanghai/android/libarchive/ArchiveEntry$StructTimespec;");
    }
    return field;
}

static jfieldID getStructStatStCtimField(JNIEnv *env) {
    static jfieldID field = NULL;
    if (!field) {
        field = findField(env, getStructStatClass(env), "stCtim",
                "Lme/zhanghai/android/libarchive/ArchiveEntry$StructTimespec;");
    }
    return field;
}

static jfieldID getStructStatStInoField(JNIEnv *env) {
    static jfieldID field = NULL;
    if (!field) {
        field = findField(env, getStructStatClass(env), "stIno", "J");
    }
    return field;
}

static jobject newStructStat(JNIEnv *env, const struct stat *stat) {
    jclass clazz = getStructStatClass(env);
    static jmethodID constructor = NULL;
    if (!constructor) {
        constructor = findMethod(env, clazz, "<init>", "()V");
    }
    jobject javaStat = (*env)->NewObject(env, clazz, constructor);
    if (!javaStat) {
        return NULL;
    }
    (*env)->SetLongField(env, javaStat, getStructStatStDevField(env), (jlong) stat->st_dev);
    (*env)->SetIntField(env, javaStat, getStructStatStModeField(env), (jint) stat->st_mode);
    (*env)->SetIntField(env, javaStat, getStructStatStNlinkField(env), (jint) stat->st_nlink);
    (*env)->SetIntField(env, javaStat, getStructStatStUidField(env), (jint) stat->st_uid);
    (*env)->SetIntField(env, javaStat, getStructStatStGidField(env), (jint) stat->st_gid);
    (*env)->SetLongField(env, javaStat, getStructStatStRdevField(env), (jlong) stat->st_rdev);
    (*env)->SetLongField(env, javaStat, getStructStatStSizeField(env), stat->st_size);
    (*env)->SetLongField(env, javaStat, getStructStatStBlksizeField(env), stat->st_blksize);
    (*env)->SetLongField(env, javaStat, getStructStatStBlocksField(env), (jlong) stat->st_blocks);
    jobject stAtim = newStructTimespec(env, &stat->st_atim);
    if (!stAtim) {
        (*env)->DeleteLocalRef(env, javaStat);
        return NULL;
    }
    jobject stMtim = newStructTimespec(env, &stat->st_mtim);
    if (!stMtim) {
        (*env)->DeleteLocalRef(env, stAtim);
        (*env)->DeleteLocalRef(env, javaStat);
        return NULL;
    }
    jobject stCtim = newStructTimespec(env, &stat->st_ctim);
    if (!stCtim) {
        (*env)->DeleteLocalRef(env, stMtim);
        (*env)->DeleteLocalRef(env, stAtim);
        (*env)->DeleteLocalRef(env, javaStat);
        return NULL;
    }
    (*env)->SetObjectField(env, javaStat, getStructStatStAtimField(env), stAtim);
    (*env)->SetObjectField(env, javaStat, getStructStatStMtimField(env), stMtim);
    (*env)->SetObjectField(env, javaStat, getStructStatStCtimField(env), stCtim);
    (*env)->SetLongField(env, javaStat, getStructStatStInoField(env), (jlong) stat->st_ino);
    return javaStat;
}

static void readStructStat(JNIEnv *env, jobject javaStat, struct stat *stat) {
    if (!javaStat) {
        return;
    }
    stat->st_dev = (*env)->GetLongField(env, javaStat, getStructStatStDevField(env));
    stat->st_mode = (*env)->GetIntField(env, javaStat, getStructStatStModeField(env));
    stat->st_nlink = (*env)->GetIntField(env, javaStat, getStructStatStNlinkField(env));
    stat->st_uid = (*env)->GetIntField(env, javaStat, getStructStatStUidField(env));
    stat->st_gid = (*env)->GetIntField(env, javaStat, getStructStatStGidField(env));
    stat->st_rdev = (*env)->GetLongField(env, javaStat, getStructStatStRdevField(env));
    stat->st_size = (*env)->GetLongField(env, javaStat, getStructStatStSizeField(env));
    stat->st_blksize = (*env)->GetLongField(env, javaStat, getStructStatStBlksizeField(env));
    stat->st_blocks = (*env)->GetLongField(env, javaStat, getStructStatStBlocksField(env));
    jobject stAtim = (*env)->GetObjectField(env, javaStat, getStructStatStAtimField(env));
    readStructTimespec(env, stAtim, &stat->st_atim);
    jobject stMtim = (*env)->GetObjectField(env, javaStat, getStructStatStMtimField(env));
    readStructTimespec(env, stMtim, &stat->st_mtim);
    jobject stCtim = (*env)->GetObjectField(env, javaStat, getStructStatStCtimField(env));
    readStructTimespec(env, stCtim, &stat->st_ctim);
    stat->st_ino = (*env)->GetLongField(env, javaStat, getStructStatStInoField(env));
}

JNIEXPORT jint JNICALL
Java_me_zhanghai_android_libarchive_Archive_versionNumber(
        JNIEnv *env, jclass clazz) {
    return archive_version_number();
}

JNIEXPORT jbyteArray JNICALL
Java_me_zhanghai_android_libarchive_Archive_versionString(
        JNIEnv *env, jclass clazz) {
    const char *versionString = archive_version_string();
    return newBytesFromString(env, versionString);
}

JNIEXPORT jbyteArray JNICALL
Java_me_zhanghai_android_libarchive_Archive_versionDetails(
        JNIEnv *env, jclass clazz) {
    const char *versionDetails = archive_version_details();
    return newBytesFromString(env, versionDetails);
}

JNIEXPORT jbyteArray JNICALL
Java_me_zhanghai_android_libarchive_Archive_zlibVersion(
        JNIEnv *env, jclass clazz) {
    const char *zlibVersion = archive_zlib_version();
    return newBytesFromString(env, zlibVersion);
}

JNIEXPORT jbyteArray JNICALL
Java_me_zhanghai_android_libarchive_Archive_liblzmaVersion(
        JNIEnv *env, jclass clazz) {
    const char *liblzmaVersion = archive_liblzma_version();
    return newBytesFromString(env, liblzmaVersion);
}

JNIEXPORT jbyteArray JNICALL
Java_me_zhanghai_android_libarchive_Archive_bzlibVersion(
        JNIEnv *env, jclass clazz) {
    const char *bzlibVersion = archive_bzlib_version();
    return newBytesFromString(env, bzlibVersion);
}

JNIEXPORT jbyteArray JNICALL
Java_me_zhanghai_android_libarchive_Archive_liblz4Version(
        JNIEnv *env, jclass clazz) {
    const char *liblz4Version = archive_liblz4_version();
    return newBytesFromString(env, liblz4Version);
}

JNIEXPORT jbyteArray JNICALL
Java_me_zhanghai_android_libarchive_Archive_libzstdVersion(
        JNIEnv *env, jclass clazz) {
    const char *libzstdVersion = archive_libzstd_version();
    return newBytesFromString(env, libzstdVersion);
}

static bool mallocArchiveJniData(JNIEnv* env, struct archive *archive) {
    struct ArchiveJniData *jniData = calloc(1, sizeof(*jniData));
    if (!jniData) {
        return false;
    }
    archive_set_user_data(archive, jniData);
    return true;
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_Archive_readNew(
        JNIEnv* env, jclass clazz) {
    struct archive *archive = archive_read_new();
    if (!archive) {
        throwArchiveException(env, ARCHIVE_FATAL, "archive_read_new");
        return (jlong) NULL;
    }
    if (!mallocArchiveJniData(env, archive)) {
        archive_read_free(archive);
        throwArchiveException(env, ARCHIVE_FATAL, "mallocArchiveJniData");
        return (jlong) NULL;
    }
    return (jlong) archive;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readSupportFilterAll(
        JNIEnv* env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_read_support_filter_all(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readSupportFilterByCode(
        JNIEnv* env, jclass clazz, jlong javaArchive, jint code) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_read_support_filter_by_code(archive, code);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readSupportFilterProgramSignature(
        JNIEnv *env, jclass clazz, jlong javaArchive, jbyteArray javaCommand,
        jbyteArray javaSignature) {
    struct archive *archive = (struct archive *) javaArchive;
    char *command = mallocStringFromBytes(env, javaCommand);
    if (!command) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    void *signature = (*env)->GetByteArrayElements(env, javaSignature, NULL);
    size_t signatureLength = (*env)->GetArrayLength(env, javaSignature);
    int errorCode = archive_read_support_filter_program_signature(archive, command, signature,
            signatureLength);
    (*env)->ReleaseByteArrayElements(env, javaSignature, signature, JNI_ABORT);
    free(command);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readSupportFormatAll(
        JNIEnv* env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_read_support_format_all(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readSupportFormatByCode(
        JNIEnv* env, jclass clazz, jlong javaArchive, jint code) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_read_support_format_by_code(archive, code);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readSupportFormatZipStreamable(
        JNIEnv* env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_read_support_format_zip_streamable(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readSupportFormatZipSeekable(
        JNIEnv* env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_read_support_format_zip_seekable(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readSetFormat(
        JNIEnv *env, jclass clazz, jlong javaArchive, jint code) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_read_set_format(archive, code);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readAppendFilter(
        JNIEnv *env, jclass clazz, jlong javaArchive, jint code) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_read_append_filter(archive, code);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readAppendFilterProgramSignature(
        JNIEnv *env, jclass clazz, jlong javaArchive, jbyteArray javaCommand,
        jbyteArray javaSignature) {
    struct archive *archive = (struct archive *) javaArchive;
    char *command = mallocStringFromBytes(env, javaCommand);
    if (!command) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    void *signature = (*env)->GetByteArrayElements(env, javaSignature, NULL);
    size_t signatureLength = (*env)->GetArrayLength(env, javaSignature);
    int errorCode = archive_read_append_filter_program_signature(archive, command, signature,
            signatureLength);
    (*env)->ReleaseByteArrayElements(env, javaSignature, signature, JNI_ABORT);
    free(command);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

static int archiveOpenCallback(struct archive *archive, void *client_data) {
    JNIEnv *env = getEnv();
    if ((*env)->PushLocalFrame(env, 0)) {
        archive_set_error(archive, ARCHIVE_FATAL, "PushLocalFrame");
        return ARCHIVE_FATAL;
    }
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    jobject callback = jniData->openCallback;
    jlong javaArchive = (jlong) archive;
    callArchiveOpenCallbackOnOpen(env, callback, javaArchive, client_data);
    if (setArchiveErrorFromException(env, archive)) {
        (*env)->PopLocalFrame(env, NULL);
        return ARCHIVE_FATAL;
    }
    (*env)->PopLocalFrame(env, NULL);
    return ARCHIVE_OK;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readSetOpenCallback(
        JNIEnv *env, jclass clazz, jlong javaArchive, jobject javaCallback) {
    struct archive *archive = (struct archive *) javaArchive;
    jobject javaCallbackRef = (*env)->NewGlobalRef(env, javaCallback);
    if (javaCallback && !javaCallbackRef) {
        throwArchiveException(env, ARCHIVE_FATAL, "NewGlobalRef");
        return;
    }
    archive_open_callback *callback = javaCallbackRef ? archiveOpenCallback : NULL;
    int errorCode = archive_read_set_open_callback(archive, callback);
    if (errorCode) {
        (*env)->DeleteGlobalRef(env, javaCallbackRef);
        throwArchiveExceptionFromError(env, archive);
        return;
    }
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    (*env)->DeleteGlobalRef(env, jniData->openCallback);
    jniData->openCallback = javaCallbackRef;
}

static la_ssize_t archiveReadCallback(struct archive *archive, void *client_data,
        const void **outBuffer) {
    *outBuffer = NULL;
    JNIEnv *env = getEnv();
    if ((*env)->PushLocalFrame(env, 0)) {
        archive_set_error(archive, ARCHIVE_FATAL, "PushLocalFrame");
        return -1;
    }
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    if (jniData->readArray) {
        (*env)->ReleaseByteArrayElements(env, jniData->readJavaArray, jniData->readArray,
                JNI_ABORT);
        jniData->readArray = NULL;
    }
    (*env)->DeleteGlobalRef(env, jniData->readJavaArray);
    jniData->readJavaArray = NULL;
    jobject callback = jniData->readCallback;
    jlong javaArchive = (jlong) archive;
    jobject javaBuffer = callArchiveReadCallbackOnRead(env, callback, javaArchive, client_data);
    if (setArchiveErrorFromException(env, archive)) {
        (*env)->PopLocalFrame(env, NULL);
        return -1;
    }
    if (!javaBuffer) {
        (*env)->PopLocalFrame(env, NULL);
        return 0;
    }
    void *buffer = NULL;
    int32_t bufferSize = 0;
    const char *errorMessage = getByteBufferBuffer(env, javaBuffer, true, NULL,
            &jniData->readJavaArray, &jniData->readArray, &buffer, &bufferSize);
    if (errorMessage) {
        archive_set_error(archive, ARCHIVE_FATAL, "%s", errorMessage);
        (*env)->PopLocalFrame(env, NULL);
        return -1;
    }
    *outBuffer = buffer;
    (*env)->PopLocalFrame(env, NULL);
    return bufferSize;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readSetReadCallback(
        JNIEnv *env, jclass clazz, jlong javaArchive, jobject javaCallback) {
    struct archive *archive = (struct archive *) javaArchive;
    jobject javaCallbackRef = (*env)->NewGlobalRef(env, javaCallback);
    if (javaCallback && !javaCallbackRef) {
        throwArchiveException(env, ARCHIVE_FATAL, "NewGlobalRef");
        return;
    }
    archive_read_callback *callback = javaCallbackRef ? archiveReadCallback : NULL;
    int errorCode = archive_read_set_read_callback(archive, callback);
    if (errorCode) {
        (*env)->DeleteGlobalRef(env, javaCallbackRef);
        throwArchiveExceptionFromError(env, archive);
        return;
    }
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    (*env)->DeleteGlobalRef(env, jniData->readCallback);
    jniData->readCallback = javaCallbackRef;
}

static la_int64_t archiveSeekCallback(struct archive *archive, void *client_data, la_int64_t offset,
        int whence) {
    JNIEnv *env = getEnv();
    if ((*env)->PushLocalFrame(env, 0)) {
        archive_set_error(archive, ARCHIVE_FATAL, "PushLocalFrame");
        return ARCHIVE_FATAL;
    }
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    jobject callback = jniData->seekCallback;
    jlong javaArchive = (jlong) archive;
    la_int64_t position = callArchiveSeekCallbackOnSeek(env, callback, javaArchive, client_data,
            offset, whence);
    if (setArchiveErrorFromException(env, archive)) {
        (*env)->PopLocalFrame(env, NULL);
        return ARCHIVE_FATAL;
    }
    (*env)->PopLocalFrame(env, NULL);
    return position;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readSetSeekCallback(
        JNIEnv *env, jclass clazz, jlong javaArchive, jobject javaCallback) {
    struct archive *archive = (struct archive *) javaArchive;
    jobject javaCallbackRef = (*env)->NewGlobalRef(env, javaCallback);
    if (javaCallback && !javaCallbackRef) {
        throwArchiveException(env, ARCHIVE_FATAL, "NewGlobalRef");
        return;
    }
    archive_seek_callback *callback = javaCallbackRef ? archiveSeekCallback : NULL;
    int errorCode = archive_read_set_seek_callback(archive, callback);
    if (errorCode) {
        (*env)->DeleteGlobalRef(env, javaCallbackRef);
        throwArchiveExceptionFromError(env, archive);
        return;
    }
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    (*env)->DeleteGlobalRef(env, jniData->seekCallback);
    jniData->seekCallback = javaCallbackRef;
}


static la_int64_t archiveSkipCallback(struct archive *archive, void *client_data,
        la_int64_t request) {
    JNIEnv *env = getEnv();
    if ((*env)->PushLocalFrame(env, 0)) {
        archive_set_error(archive, ARCHIVE_FATAL, "PushLocalFrame");
        return ARCHIVE_FATAL;
    }
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    jobject callback = jniData->skipCallback;
    jlong javaArchive = (jlong) archive;
    la_int64_t skipped = callArchiveSkipCallbackOnSkip(env, callback, javaArchive, client_data,
            request);
    if (setArchiveErrorFromException(env, archive)) {
        (*env)->PopLocalFrame(env, NULL);
        return ARCHIVE_FATAL;
    }
    (*env)->PopLocalFrame(env, NULL);
    return skipped;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readSetSkipCallback(
        JNIEnv *env, jclass clazz, jlong javaArchive, jobject javaCallback) {
    struct archive *archive = (struct archive *) javaArchive;
    jobject javaCallbackRef = (*env)->NewGlobalRef(env, javaCallback);
    if (javaCallback && !javaCallbackRef) {
        throwArchiveException(env, ARCHIVE_FATAL, "NewGlobalRef");
        return;
    }
    archive_skip_callback *callback = javaCallbackRef ? archiveSkipCallback : NULL;
    int errorCode = archive_read_set_skip_callback(archive, callback);
    if (errorCode) {
        (*env)->DeleteGlobalRef(env, javaCallbackRef);
        throwArchiveExceptionFromError(env, archive);
        return;
    }
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    (*env)->DeleteGlobalRef(env, jniData->skipCallback);
    jniData->skipCallback = javaCallbackRef;
}

static int archiveCloseCallback(struct archive *archive, void *client_data) {
    JNIEnv *env = getEnv();
    if ((*env)->PushLocalFrame(env, 0)) {
        archive_set_error(archive, ARCHIVE_FATAL, "PushLocalFrame");
        return ARCHIVE_FATAL;
    }
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    jobject callback = jniData->closeCallback;
    jlong javaArchive = (jlong) archive;
    callArchiveCloseCallbackOnClose(env, callback, javaArchive, client_data);
    if (setArchiveErrorFromException(env, archive)) {
        (*env)->PopLocalFrame(env, NULL);
        return ARCHIVE_FATAL;
    }
    (*env)->PopLocalFrame(env, NULL);
    return ARCHIVE_OK;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readSetCloseCallback(
        JNIEnv *env, jclass clazz, jlong javaArchive, jobject javaCallback) {
    struct archive *archive = (struct archive *) javaArchive;
    jobject javaCallbackRef = (*env)->NewGlobalRef(env, javaCallback);
    if (javaCallback && !javaCallbackRef) {
        throwArchiveException(env, ARCHIVE_FATAL, "NewGlobalRef");
        return;
    }
    archive_close_callback *callback = javaCallbackRef ? archiveCloseCallback : NULL;
    int errorCode = archive_read_set_close_callback(archive, callback);
    if (errorCode) {
        (*env)->DeleteGlobalRef(env, javaCallbackRef);
        throwArchiveExceptionFromError(env, archive);
        return;
    }
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    (*env)->DeleteGlobalRef(env, jniData->closeCallback);
    jniData->closeCallback = javaCallbackRef;
}

static int archiveSwitchCallback(struct archive *archive, void *client_data1, void *client_data2) {
    JNIEnv *env = getEnv();
    if ((*env)->PushLocalFrame(env, 0)) {
        archive_set_error(archive, ARCHIVE_FATAL, "PushLocalFrame");
        return ARCHIVE_FATAL;
    }
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    jobject callback = jniData->switchCallback;
    jlong javaArchive = (jlong) archive;
    callArchiveSwitchCallbackOnSwitch(env, callback, javaArchive, client_data1, client_data2);
    if (setArchiveErrorFromException(env, archive)) {
        (*env)->PopLocalFrame(env, NULL);
        return ARCHIVE_FATAL;
    }
    (*env)->PopLocalFrame(env, NULL);
    return ARCHIVE_OK;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readSetSwitchCallback(
        JNIEnv *env, jclass clazz, jlong javaArchive, jobject javaCallback) {
    struct archive *archive = (struct archive *) javaArchive;
    jobject javaCallbackRef = (*env)->NewGlobalRef(env, javaCallback);
    if (javaCallback && !javaCallbackRef) {
        throwArchiveException(env, ARCHIVE_FATAL, "NewGlobalRef");
        return;
    }
    archive_switch_callback *callback = javaCallbackRef ? archiveSwitchCallback : NULL;
    int errorCode = archive_read_set_switch_callback(archive, callback);
    if (errorCode) {
        (*env)->DeleteGlobalRef(env, javaCallbackRef);
        throwArchiveExceptionFromError(env, archive);
        return;
    }
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    (*env)->DeleteGlobalRef(env, jniData->switchCallback);
    jniData->switchCallback = javaCallbackRef;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readSetCallbackData2(
        JNIEnv *env, jclass clazz, jlong javaArchive, jobject clientData, jint index) {
    struct archive *archive = (struct archive *) javaArchive;
    jobject clientDataRef = (*env)->NewGlobalRef(env, clientData);
    if (clientData && !clientDataRef) {
        throwArchiveException(env, ARCHIVE_FATAL, "NewGlobalRef");
        return;
    }
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    if (jniData->hasReadClientData) {
        unsigned int clientDataSize = archive_read_get_callback_data_size(archive);
        if (index < clientDataSize) {
            jobject oldClientData = archive_read_get_callback_data(archive, index);
            (*env)->DeleteGlobalRef(env, oldClientData);
        }
    }
    int errorCode = archive_read_set_callback_data2(archive, clientDataRef, index);
    if (errorCode) {
        (*env)->DeleteGlobalRef(env, clientDataRef);
        throwArchiveExceptionFromError(env, archive);
        return;
    }
    jniData->hasReadClientData = true;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readAddCallbackData(
        JNIEnv *env, jclass clazz, jlong javaArchive, jobject clientData, jint index) {
    struct archive *archive = (struct archive *) javaArchive;
    jobject clientDataRef = (*env)->NewGlobalRef(env, clientData);
    if (clientData && !clientDataRef) {
        throwArchiveException(env, ARCHIVE_FATAL, "NewGlobalRef");
        return;
    }
    int errorCode = archive_read_add_callback_data(archive, clientDataRef, index);
    if (errorCode) {
        (*env)->DeleteGlobalRef(env, clientDataRef);
        throwArchiveExceptionFromError(env, archive);
        return;
    }
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    jniData->hasReadClientData = true;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readAppendCallbackData(
        JNIEnv *env, jclass clazz, jlong javaArchive, jobject clientData) {
    struct archive *archive = (struct archive *) javaArchive;
    jobject clientDataRef = (*env)->NewGlobalRef(env, clientData);
    if (clientData && !clientDataRef) {
        throwArchiveException(env, ARCHIVE_FATAL, "NewGlobalRef");
        return;
    }
    int errorCode = archive_read_append_callback_data(archive, clientDataRef);
    if (errorCode) {
        (*env)->DeleteGlobalRef(env, clientDataRef);
        throwArchiveExceptionFromError(env, archive);
        return;
    }
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    jniData->hasReadClientData = true;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readOpen1(
        JNIEnv* env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_read_open1(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readOpenFileName(
        JNIEnv *env, jclass clazz, jlong javaArchive, jbyteArray javaFileName, jlong blockSize) {
    struct archive *archive = (struct archive *) javaArchive;
    char *fileName = mallocStringFromBytes(env, javaFileName);
    if (javaFileName && !fileName) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    int errorCode = archive_read_open_filename(archive, fileName, blockSize);
    free(fileName);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readOpenFileNames(
        JNIEnv *env, jclass clazz, jlong javaArchive, jobjectArray javaFileNames,
        jlong blockSize) {
    struct archive *archive = (struct archive *) javaArchive;
    const char **fileNames = (const char **) mallocStringArrayFromBytesArray(env, javaFileNames);
    if (!fileNames) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringArrayFromBytesArray");
        return;
    }
    int errorCode = archive_read_open_filenames(archive, fileNames, blockSize);
    free(fileNames);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readOpenMemory(
        JNIEnv *env, jclass clazz, jlong javaArchive, jobject javaBuffer) {
    struct archive *archive = (struct archive *) javaArchive;
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    if (jniData->openMemoryArray) {
        (*env)->ReleaseByteArrayElements(env, jniData->openMemoryJavaArray,
                jniData->openMemoryArray, jniData->openMemoryArrayReleaseMode);
        jniData->openMemoryArray = NULL;
        jniData->openMemoryArrayReleaseMode = 0;
    }
    (*env)->DeleteGlobalRef(env, jniData->openMemoryJavaArray);
    jniData->openMemoryJavaArray = NULL;
    jniData->openMemoryArrayReleaseMode = JNI_ABORT;
    void *buffer = NULL;
    int32_t bufferSize = 0;
    const char *errorMessage = getByteBufferBuffer(env, javaBuffer, false, NULL,
            &jniData->openMemoryJavaArray, &jniData->openMemoryArray, &buffer, &bufferSize);
    if (errorMessage) {
        throwArchiveException(env, ARCHIVE_FATAL, errorMessage);
        return;
    }
    int errorCode = archive_read_open_memory(archive, buffer, bufferSize);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readOpenFd(
        JNIEnv *env, jclass clazz, jlong javaArchive, jint fd, jlong blockSize) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_read_open_fd(archive, fd, blockSize);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_Archive_readNextHeader(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    struct archive_entry *entry = NULL;
    int errorCode = archive_read_next_header(archive, &entry);
    if (errorCode) {
        if (errorCode != ARCHIVE_EOF) {
            throwArchiveExceptionFromError(env, archive);
        }
        return (jlong) NULL;
    }
    return (jlong) entry;
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_Archive_readNextHeader2(
        JNIEnv *env, jclass clazz, jlong javaArchive, jlong javaEntry) {
    struct archive *archive = (struct archive *) javaArchive;
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    int errorCode = archive_read_next_header2(archive, entry);
    if (errorCode) {
        if (errorCode != ARCHIVE_EOF) {
            throwArchiveExceptionFromError(env, archive);
        }
        return (jlong) NULL;
    }
    return (jlong) entry;
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_Archive_readHeaderPosition(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    la_int64_t position = archive_read_header_position(archive);
    if (position < 0) {
        throwArchiveExceptionFromError(env, archive);
        return position;
    }
    return position;
}

JNIEXPORT jint JNICALL
Java_me_zhanghai_android_libarchive_Archive_readHasEncryptedEntries(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    return archive_read_has_encrypted_entries(archive);
}

JNIEXPORT jint JNICALL
Java_me_zhanghai_android_libarchive_Archive_readFormatCapabilities(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    return archive_read_format_capabilities(archive);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readData(
        JNIEnv *env, jclass clazz, jlong javaArchive, jobject javaBuffer) {
    struct archive *archive = (struct archive *) javaArchive;
    jint position = 0;
    jbyteArray javaArray = NULL;
    jbyte *array = NULL;
    void *buffer = NULL;
    int32_t bufferSize = 0;
    const char *errorMessage = getByteBufferBuffer(env, javaBuffer, false, &position, &javaArray,
            &array, &buffer, &bufferSize);
    if (errorMessage) {
        throwArchiveException(env, ARCHIVE_FATAL, errorMessage);
        return;
    }
    int bytesRead = archive_read_data(archive, buffer, bufferSize);
    if (array) {
        (*env)->ReleaseByteArrayElements(env, javaArray, array, 0);
    }
    if (bytesRead < 0) {
        throwArchiveExceptionFromError(env, archive);
        return;
    }
    jint newPosition = position + bytesRead;
    setByteBufferPosition(env, javaBuffer, newPosition);
    if ((*env)->ExceptionCheck(env)) {
        throwArchiveException(env, ARCHIVE_FATAL, "ByteBuffer.position()");
    }
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_Archive_seekData(
        JNIEnv *env, jclass clazz, jlong javaArchive, jlong offset, jint whence) {
    struct archive *archive = (struct archive *) javaArchive;
    la_int64_t position = archive_seek_data(archive, offset, whence);
    if (position < 0) {
        throwArchiveExceptionFromError(env, archive);
        return position;
    }
    return position;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readDataSkip(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_read_data_skip(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readDataIntoFd(
        JNIEnv *env, jclass clazz, jlong javaArchive, jint fd) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_read_data_into_fd(archive, fd);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readSetFormatOption(
        JNIEnv *env, jclass clazz, jlong javaArchive, jbyteArray javaModule, jbyteArray javaOption,
        jbyteArray javaValue) {
    struct archive *archive = (struct archive *) javaArchive;
    char *module = mallocStringFromBytes(env, javaModule);
    if (javaModule && !module) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    char *option = mallocStringFromBytes(env, javaOption);
    if (javaOption && !option) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    char *value = mallocStringFromBytes(env, javaValue);
    if (javaValue && !value) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    int errorCode = archive_read_set_format_option(archive, module, option, value);
    free(value);
    free(option);
    free(module);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readSetFilterOption(
        JNIEnv *env, jclass clazz, jlong javaArchive, jbyteArray javaModule, jbyteArray javaOption,
        jbyteArray javaValue) {
    struct archive *archive = (struct archive *) javaArchive;
    char *module = mallocStringFromBytes(env, javaModule);
    if (javaModule && !module) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    char *option = mallocStringFromBytes(env, javaOption);
    if (javaOption && !option) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    char *value = mallocStringFromBytes(env, javaValue);
    if (javaValue && !value) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    int errorCode = archive_read_set_filter_option(archive, module, option, value);
    free(value);
    free(option);
    free(module);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readSetOption(
        JNIEnv *env, jclass clazz, jlong javaArchive, jbyteArray javaModule, jbyteArray javaOption,
        jbyteArray javaValue) {
    struct archive *archive = (struct archive *) javaArchive;
    char *module = mallocStringFromBytes(env, javaModule);
    if (javaModule && !module) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    char *option = mallocStringFromBytes(env, javaOption);
    if (javaOption && !option) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    char *value = mallocStringFromBytes(env, javaValue);
    if (javaValue && !value) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    int errorCode = archive_read_set_option(archive, module, option, value);
    free(value);
    free(option);
    free(module);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readSetOptions(
        JNIEnv *env, jclass clazz, jlong javaArchive, jbyteArray javaOptions) {
    struct archive *archive = (struct archive *) javaArchive;
    char *options = mallocStringFromBytes(env, javaOptions);
    if (javaOptions && !options) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    int errorCode = archive_read_set_options(archive, options);
    free(options);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readAddPassphrase(
        JNIEnv *env, jclass clazz, jlong javaArchive, jbyteArray javaPassphrase) {
    struct archive *archive = (struct archive *) javaArchive;
    char *passphrase = mallocStringFromBytes(env, javaPassphrase);
    if (javaPassphrase && !passphrase) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    int errorCode = archive_read_add_passphrase(archive, passphrase);
    free(passphrase);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

static const char *archivePassphraseCallback(struct archive *archive, void *client_data) {
    JNIEnv *env = getEnv();
    if ((*env)->PushLocalFrame(env, 0)) {
        archive_set_error(archive, ARCHIVE_FATAL, "PushLocalFrame");
        return NULL;
    }
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    free(jniData->passphrase);
    jniData->passphrase = NULL;
    jobject callback = jniData->passphraseCallback;
    jlong javaArchive = (jlong) archive;
    jbyteArray javaPassphrase = callArchivePassphraseCallbackOnPassphrase(env, callback,
            javaArchive, client_data);
    if (setArchiveErrorFromException(env, archive)) {
        (*env)->PopLocalFrame(env, NULL);
        return NULL;
    }
    char *passphrase = mallocStringFromBytes(env, javaPassphrase);
    if (javaPassphrase && !passphrase) {
        archive_set_error(archive, ARCHIVE_FATAL, "mallocStringFromBytes");
        (*env)->PopLocalFrame(env, NULL);
        return NULL;
    }
    jniData->passphrase = passphrase;
    (*env)->PopLocalFrame(env, NULL);
    return passphrase;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readSetPassphraseCallback(
        JNIEnv *env, jclass clazz, jlong javaArchive, jobject clientData, jobject javaCallback) {
    struct archive *archive = (struct archive *) javaArchive;
    jobject clientDataRef = (*env)->NewGlobalRef(env, clientData);
    if (clientData && !clientDataRef) {
        throwArchiveException(env, ARCHIVE_FATAL, "NewGlobalRef");
        return;
    }
    jobject javaCallbackRef = (*env)->NewGlobalRef(env, javaCallback);
    if (javaCallback && !javaCallbackRef) {
        (*env)->DeleteGlobalRef(env, clientDataRef);
        throwArchiveException(env, ARCHIVE_FATAL, "NewGlobalRef");
        return;
    }
    archive_passphrase_callback *callback = javaCallbackRef ? archivePassphraseCallback : NULL;
    int errorCode = archive_read_set_passphrase_callback(archive, clientDataRef, callback);
    if (errorCode) {
        (*env)->DeleteGlobalRef(env, javaCallbackRef);
        (*env)->DeleteGlobalRef(env, clientDataRef);
        throwArchiveExceptionFromError(env, archive);
        return;
    }
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    (*env)->DeleteGlobalRef(env, jniData->passphraseClientData);
    (*env)->DeleteGlobalRef(env, jniData->passphraseCallback);
    jniData->passphraseClientData = clientDataRef;
    jniData->passphraseCallback = javaCallbackRef;
}

static void closeArchiveJniData(JNIEnv* env, struct archive *archive) {
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    if (jniData->openMemoryArray) {
        (*env)->ReleaseByteArrayElements(env, jniData->openMemoryJavaArray,
                jniData->openMemoryArray, jniData->openMemoryArrayReleaseMode);
        jniData->openMemoryArray = NULL;
        jniData->openMemoryArrayReleaseMode = 0;
    }
    (*env)->DeleteGlobalRef(env, jniData->openMemoryJavaArray);
    jniData->openMemoryJavaArray = NULL;
    if (jniData->writeOpenMemoryJavaBuffer) {
        setByteBufferPosition(env, jniData->writeOpenMemoryJavaBuffer,
                jniData->writeOpenMemoryPosition + (jint) jniData->writeOpenMemoryUsed);
        if ((*env)->ExceptionCheck(env)) {
            throwArchiveException(env, ARCHIVE_FATAL, "ByteBuffer.position()");
        }
        jniData->writeOpenMemoryPosition = 0;
        jniData->writeOpenMemoryUsed = 0;
        (*env)->DeleteGlobalRef(env, jniData->writeOpenMemoryJavaBuffer);
        jniData->writeOpenMemoryJavaBuffer = NULL;
    }
    if (jniData->readArray) {
        (*env)->ReleaseByteArrayElements(env, jniData->readJavaArray, jniData->readArray,
                JNI_ABORT);
        jniData->readArray = NULL;
    }
    (*env)->DeleteGlobalRef(env, jniData->readJavaArray);
    jniData->readJavaArray = NULL;
    free(jniData->passphrase);
    jniData->passphrase = NULL;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_readClose(
        JNIEnv* env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    closeArchiveJniData(env, archive);
    int errorCode = archive_read_close(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeNew(
        JNIEnv* env, jclass clazz) {
    struct archive *archive = archive_write_new();
    if (!archive) {
        throwArchiveException(env, ARCHIVE_FATAL, "archive_write_new");
        return (jlong) NULL;
    }
    if (!mallocArchiveJniData(env, archive)) {
        archive_read_free(archive);
        throwArchiveException(env, ARCHIVE_FATAL, "mallocArchiveJniData");
        return (jlong) NULL;
    }
    return (jlong) archive;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetBytesPerBlock(
        JNIEnv *env, jclass clazz, jlong javaArchive, jint bytesPerBlock) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_bytes_per_block(archive, bytesPerBlock);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT jint JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeGetBytesPerBlock(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int bytesPerBlock = archive_write_get_bytes_per_block(archive);
    if (bytesPerBlock < -1) {
        throwArchiveExceptionFromError(env, archive);
        return bytesPerBlock;
    }
    return bytesPerBlock;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetBytesInLastBlock(
        JNIEnv *env, jclass clazz, jlong javaArchive, jint bytesInLastBlock) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_bytes_in_last_block(archive, bytesInLastBlock);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT jint JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeGetBytesInLastBlock(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int bytesInLastBlock = archive_write_get_bytes_in_last_block(archive);
    if (bytesInLastBlock < -1) {
        throwArchiveExceptionFromError(env, archive);
        return bytesInLastBlock;
    }
    return bytesInLastBlock;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeAddFilter(
        JNIEnv *env, jclass clazz, jlong javaArchive, jint code) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_add_filter(archive, code);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeAddFilterByName(
        JNIEnv *env, jclass clazz, jlong javaArchive, jbyteArray javaName) {
    struct archive *archive = (struct archive *) javaArchive;
    char *name = mallocStringFromBytes(env, javaName);
    if (javaName && !name) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    int errorCode = archive_write_add_filter_by_name(archive, name);
    free(name);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeAddFilterB64encode(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_add_filter_b64encode(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeAddFilterBzip2(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_add_filter_bzip2(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeAddFilterCompress(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_add_filter_compress(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeAddFilterGrzip(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_add_filter_grzip(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeAddFilterGzip(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_add_filter_gzip(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeAddFilterLrzip(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_add_filter_lrzip(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeAddFilterLz4(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_add_filter_lz4(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeAddFilterLzip(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_add_filter_lzip(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeAddFilterLzma(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_add_filter_lzma(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeAddFilterLzop(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_add_filter_lzop(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeAddFilterNone(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_add_filter_none(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeAddFilterProgram(
        JNIEnv *env, jclass clazz, jlong javaArchive, jbyteArray javaCommand) {
    struct archive *archive = (struct archive *) javaArchive;
    char *command = mallocStringFromBytes(env, javaCommand);
    if (javaCommand && !command) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    int errorCode = archive_write_add_filter_program(archive, command);
    free(command);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeAddFilterUuencode(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_add_filter_uuencode(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeAddFilterXz(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_add_filter_xz(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeAddFilterZstd(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_add_filter_zstd(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormat(
        JNIEnv *env, jclass clazz, jlong javaArchive, jint code) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_format(archive, code);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatByName(
        JNIEnv *env, jclass clazz, jlong javaArchive, jbyteArray javaName) {
    struct archive *archive = (struct archive *) javaArchive;
    char *name = mallocStringFromBytes(env, javaName);
    if (javaName && !name) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    int errorCode = archive_write_set_format_by_name(archive, name);
    free(name);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormat7zip(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_format_7zip(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatArBsd(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_format_ar_bsd(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatArSvr4(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_format_ar_svr4(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatCpio(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_format_cpio(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatCpioBin(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_format_cpio_bin(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatCpioNewc(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_format_cpio_newc(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatCpioOdc(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_format_cpio_odc(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatCpioPwb(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_format_cpio_pwb(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatGnutar(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_format_gnutar(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatIso9660(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_format_iso9660(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatMtree(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_format_mtree(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatMtreeClassic(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_format_mtree_classic(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatPax(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_format_pax(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatPaxRestricted(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_format_pax_restricted(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatRaw(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_format_raw(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatShar(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_format_shar(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatSharDump(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_format_shar_dump(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatUstar(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_format_ustar(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatV7tar(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_format_v7tar(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatWarc(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_format_warc(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatXar(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_format_xar(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatZip(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_set_format_zip(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatFilterByExt(
        JNIEnv *env, jclass clazz, jlong javaArchive, jbyteArray javaFileName) {
    struct archive *archive = (struct archive *) javaArchive;
    char *fileName = mallocStringFromBytes(env, javaFileName);
    if (javaFileName && !fileName) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    int errorCode = archive_write_set_format_filter_by_ext(archive, fileName);
    free(fileName);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatFilterByExtDef(
        JNIEnv *env, jclass clazz, jlong javaArchive, jbyteArray javaFileName,
        jbyteArray javaDefaultExtension) {
    struct archive *archive = (struct archive *) javaArchive;
    char *fileName = mallocStringFromBytes(env, javaFileName);
    if (javaFileName && !fileName) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    char *defaultExtension = mallocStringFromBytes(env, javaDefaultExtension);
    if (javaDefaultExtension && !defaultExtension) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    int errorCode = archive_write_set_format_filter_by_ext_def(archive, fileName, defaultExtension);
    free(defaultExtension);
    free(fileName);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeZipSetCompressionDeflate(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_zip_set_compression_deflate(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeZipSetCompressionStore(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_zip_set_compression_store(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

static la_ssize_t archiveWriteCallback(struct archive *archive, void *client_data,
        const void *buffer, size_t length) {
    JNIEnv *env = getEnv();
    if ((*env)->PushLocalFrame(env, 0)) {
        archive_set_error(archive, ARCHIVE_FATAL, "PushLocalFrame");
        return -1;
    }
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    jobject callback = jniData->writeCallback;
    jlong javaArchive = (jlong) archive;
    jobject javaBuffer = (*env)->NewDirectByteBuffer(env, (void *) buffer, length);
    if (!javaBuffer) {
        javaBuffer = newHeapByteBufferFromBuffer(env, buffer, length, true);
        if (!javaBuffer) {
            archive_set_error(archive, ARCHIVE_FATAL,
                    "!(NewDirectByteBuffer || newHeapByteBufferFromBuffer)");
            (*env)->PopLocalFrame(env, NULL);
            return -1;
        }
    }
    callArchiveWriteCallbackOnWrite(env, callback, javaArchive, client_data, javaBuffer);
    if (setArchiveErrorFromException(env, archive)) {
        (*env)->PopLocalFrame(env, NULL);
        return -1;
    }
    jint position = getByteBufferPosition(env, javaBuffer);
    if ((*env)->ExceptionCheck(env)) {
        archive_set_error(archive, ARCHIVE_FATAL, "ByteBuffer.position()");
        (*env)->PopLocalFrame(env, NULL);
        return -1;
    }
    (*env)->PopLocalFrame(env, NULL);
    return position;
}

static int archiveFreeCallback(struct archive *archive, void *client_data) {
    JNIEnv *env = getEnv();
    if ((*env)->PushLocalFrame(env, 0)) {
        archive_set_error(archive, ARCHIVE_FATAL, "PushLocalFrame");
        return ARCHIVE_FATAL;
    }
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    jobject callback = jniData->freeCallback;
    jlong javaArchive = (jlong) archive;
    callArchiveFreeCallbackOnFree(env, callback, javaArchive, client_data);
    if (setArchiveErrorFromException(env, archive)) {
        (*env)->PopLocalFrame(env, NULL);
        return ARCHIVE_FATAL;
    }
    (*env)->PopLocalFrame(env, NULL);
    return ARCHIVE_OK;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeOpen2(
        JNIEnv *env, jclass clazz, jlong javaArchive, jobject clientData, jobject javaOpenCallback,
        jobject javaWriteCallback, jobject javaCloseCallback, jobject javaFreeCallback) {
    struct archive *archive = (struct archive *) javaArchive;
    jobject clientDataRef = (*env)->NewGlobalRef(env, clientData);
    if (clientData && !clientDataRef) {
        throwArchiveException(env, ARCHIVE_FATAL, "NewGlobalRef");
        return;
    }
    jobject javaOpenCallbackRef = (*env)->NewGlobalRef(env, javaOpenCallback);
    if (javaOpenCallback && !javaOpenCallbackRef) {
        (*env)->DeleteGlobalRef(env, clientDataRef);
        throwArchiveException(env, ARCHIVE_FATAL, "NewGlobalRef");
        return;
    }
    archive_open_callback *openCallback = javaOpenCallbackRef ? archiveOpenCallback : NULL;
    jobject javaWriteCallbackRef = (*env)->NewGlobalRef(env, javaWriteCallback);
    if (javaWriteCallback && !javaWriteCallbackRef) {
        (*env)->DeleteGlobalRef(env, javaOpenCallbackRef);
        (*env)->DeleteGlobalRef(env, clientDataRef);
        throwArchiveException(env, ARCHIVE_FATAL, "NewGlobalRef");
        return;
    }
    archive_write_callback *writeCallback = javaWriteCallbackRef ? archiveWriteCallback : NULL;
    jobject javaCloseCallbackRef = (*env)->NewGlobalRef(env, javaCloseCallback);
    if (javaCloseCallback && !javaCloseCallbackRef) {
        (*env)->DeleteGlobalRef(env, javaWriteCallbackRef);
        (*env)->DeleteGlobalRef(env, javaOpenCallbackRef);
        (*env)->DeleteGlobalRef(env, clientDataRef);
        throwArchiveException(env, ARCHIVE_FATAL, "NewGlobalRef");
        return;
    }
    archive_close_callback *closeCallback = javaCloseCallbackRef ? archiveCloseCallback : NULL;
    jobject javaFreeCallbackRef = (*env)->NewGlobalRef(env, javaFreeCallback);
    if (javaFreeCallback && !javaFreeCallbackRef) {
        (*env)->DeleteGlobalRef(env, javaCloseCallbackRef);
        (*env)->DeleteGlobalRef(env, javaWriteCallbackRef);
        (*env)->DeleteGlobalRef(env, javaOpenCallbackRef);
        (*env)->DeleteGlobalRef(env, clientDataRef);
        throwArchiveException(env, ARCHIVE_FATAL, "NewGlobalRef");
        return;
    }
    archive_free_callback *freeCallback = javaFreeCallbackRef ? archiveFreeCallback : NULL;
    int errorCode = archive_write_open2(archive, clientDataRef, openCallback, writeCallback,
            closeCallback, freeCallback);
    if (errorCode) {
        (*env)->DeleteGlobalRef(env, javaFreeCallbackRef);
        (*env)->DeleteGlobalRef(env, javaCloseCallbackRef);
        (*env)->DeleteGlobalRef(env, javaWriteCallbackRef);
        (*env)->DeleteGlobalRef(env, javaOpenCallbackRef);
        (*env)->DeleteGlobalRef(env, clientDataRef);
        throwArchiveExceptionFromError(env, archive);
        return;
    }
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    (*env)->DeleteGlobalRef(env, jniData->writeClientData);
    (*env)->DeleteGlobalRef(env, jniData->openCallback);
    (*env)->DeleteGlobalRef(env, jniData->writeCallback);
    (*env)->DeleteGlobalRef(env, jniData->closeCallback);
    (*env)->DeleteGlobalRef(env, jniData->freeCallback);
    jniData->writeClientData = clientDataRef;
    jniData->openCallback = javaOpenCallbackRef;
    jniData->writeCallback = javaWriteCallbackRef;
    jniData->closeCallback = javaCloseCallbackRef;
    jniData->freeCallback = javaFreeCallbackRef;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeOpenFd(
        JNIEnv *env, jclass clazz, jlong javaArchive, jint fd) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_open_fd(archive, fd);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeOpenFileName(
        JNIEnv *env, jclass clazz, jlong javaArchive, jbyteArray javaFileName) {
    struct archive *archive = (struct archive *) javaArchive;
    char *fileName = mallocStringFromBytes(env, javaFileName);
    if (javaFileName && !fileName) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    int errorCode = archive_write_open_filename(archive, fileName);
    free(fileName);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeOpenMemory(
        JNIEnv *env, jclass clazz, jlong javaArchive, jobject javaBuffer) {
    struct archive *archive = (struct archive *) javaArchive;
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    if (jniData->openMemoryArray) {
        (*env)->ReleaseByteArrayElements(env, jniData->openMemoryJavaArray,
                jniData->openMemoryArray, JNI_ABORT);
        jniData->openMemoryArray = NULL;
        jniData->openMemoryArrayReleaseMode = 0;
    }
    (*env)->DeleteGlobalRef(env, jniData->openMemoryJavaArray);
    jniData->openMemoryJavaArray = NULL;
    if (jniData->writeOpenMemoryJavaBuffer) {
        (*env)->DeleteGlobalRef(env, jniData->writeOpenMemoryJavaBuffer);
        jniData->writeOpenMemoryJavaBuffer = NULL;
    }
    jniData->writeOpenMemoryPosition = 0;
    jniData->writeOpenMemoryUsed = 0;
    jniData->openMemoryArrayReleaseMode = 0;
    void *buffer = NULL;
    int32_t bufferSize = 0;
    const char *errorMessage = getByteBufferBuffer(env, javaBuffer, false,
            &jniData->writeOpenMemoryPosition, &jniData->openMemoryJavaArray,
            &jniData->openMemoryArray, &buffer, &bufferSize);
    if (errorMessage) {
        throwArchiveException(env, ARCHIVE_FATAL, errorMessage);
        return;
    }
    jniData->writeOpenMemoryJavaBuffer = (*env)->NewGlobalRef(env, javaBuffer);
    if (!jniData->writeOpenMemoryJavaBuffer) {
        throwArchiveException(env, ARCHIVE_FATAL, "NewGlobalRef");
        return;
    }
    int errorCode = archive_write_open_memory(archive, buffer, bufferSize,
            &jniData->writeOpenMemoryUsed);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeHeader(
        JNIEnv *env, jclass clazz, jlong javaArchive, jlong javaEntry) {
    struct archive *archive = (struct archive *) javaArchive;
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    int errorCode = archive_write_header(archive, entry);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeData(
        JNIEnv *env, jclass clazz, jlong javaArchive, jobject javaBuffer) {
    struct archive *archive = (struct archive *) javaArchive;
    jint position = 0;
    jbyteArray javaArray = NULL;
    jbyte *array = NULL;
    void *buffer = NULL;
    int32_t bufferSize = 0;
    const char *errorMessage = getByteBufferBuffer(env, javaBuffer, false, &position, &javaArray,
            &array, &buffer, &bufferSize);
    if (errorMessage) {
        throwArchiveException(env, ARCHIVE_FATAL, errorMessage);
        return;
    }
    int bytesWritten = archive_write_data(archive, buffer, bufferSize);
    if (array) {
        (*env)->ReleaseByteArrayElements(env, javaArray, array, JNI_ABORT);
    }
    if (bytesWritten < 0) {
        throwArchiveExceptionFromError(env, archive);
        return;
    }
    jint newPosition = position + bytesWritten;
    setByteBufferPosition(env, javaBuffer, newPosition);
    if ((*env)->ExceptionCheck(env)) {
        throwArchiveException(env, ARCHIVE_FATAL, "ByteBuffer.position()");
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeFinishEntry(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_finish_entry(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeClose(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    closeArchiveJniData(env, archive);
    int errorCode = archive_write_close(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeFail(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    int errorCode = archive_write_fail(archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFormatOption(
        JNIEnv *env, jclass clazz, jlong javaArchive, jbyteArray javaModule, jbyteArray javaOption,
        jbyteArray javaValue) {
    struct archive *archive = (struct archive *) javaArchive;
    char *module = mallocStringFromBytes(env, javaModule);
    if (javaModule && !module) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    char *option = mallocStringFromBytes(env, javaOption);
    if (javaOption && !option) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    char *value = mallocStringFromBytes(env, javaValue);
    if (javaValue && !value) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    int errorCode = archive_write_set_format_option(archive, module, option, value);
    free(value);
    free(option);
    free(module);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetFilterOption(
        JNIEnv *env, jclass clazz, jlong javaArchive, jbyteArray javaModule, jbyteArray javaOption,
        jbyteArray javaValue) {
    struct archive *archive = (struct archive *) javaArchive;
    char *module = mallocStringFromBytes(env, javaModule);
    if (javaModule && !module) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    char *option = mallocStringFromBytes(env, javaOption);
    if (javaOption && !option) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    char *value = mallocStringFromBytes(env, javaValue);
    if (javaValue && !value) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    int errorCode = archive_write_set_filter_option(archive, module, option, value);
    free(value);
    free(option);
    free(module);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetOption(
        JNIEnv *env, jclass clazz, jlong javaArchive, jbyteArray javaModule, jbyteArray javaOption,
        jbyteArray javaValue) {
    struct archive *archive = (struct archive *) javaArchive;
    char *module = mallocStringFromBytes(env, javaModule);
    if (javaModule && !module) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    char *option = mallocStringFromBytes(env, javaOption);
    if (javaOption && !option) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    char *value = mallocStringFromBytes(env, javaValue);
    if (javaValue && !value) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    int errorCode = archive_write_set_option(archive, module, option, value);
    free(value);
    free(option);
    free(module);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetOptions(
        JNIEnv *env, jclass clazz, jlong javaArchive, jbyteArray javaOptions) {
    struct archive *archive = (struct archive *) javaArchive;
    char *options = mallocStringFromBytes(env, javaOptions);
    if (javaOptions && !options) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    int errorCode = archive_write_set_options(archive, options);
    free(options);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetPassphrase(
        JNIEnv *env, jclass clazz, jlong javaArchive, jbyteArray javaPassphrase) {
    struct archive *archive = (struct archive *) javaArchive;
    char *passphrase = mallocStringFromBytes(env, javaPassphrase);
    if (javaPassphrase && !passphrase) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    int errorCode = archive_write_set_passphrase(archive, passphrase);
    free(passphrase);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_writeSetPassphraseCallback(
        JNIEnv *env, jclass clazz, jlong javaArchive, jobject clientData, jobject javaCallback) {
    struct archive *archive = (struct archive *) javaArchive;
    jobject clientDataRef = (*env)->NewGlobalRef(env, clientData);
    if (clientData && !clientDataRef) {
        throwArchiveException(env, ARCHIVE_FATAL, "NewGlobalRef");
        return;
    }
    jobject javaCallbackRef = (*env)->NewGlobalRef(env, javaCallback);
    if (javaCallback && !javaCallbackRef) {
        (*env)->DeleteGlobalRef(env, clientDataRef);
        throwArchiveException(env, ARCHIVE_FATAL, "NewGlobalRef");
        return;
    }
    archive_passphrase_callback *callback = javaCallbackRef ? archivePassphraseCallback : NULL;
    int errorCode = archive_write_set_passphrase_callback(archive, clientDataRef, callback);
    if (errorCode) {
        (*env)->DeleteGlobalRef(env, javaCallbackRef);
        (*env)->DeleteGlobalRef(env, clientDataRef);
        throwArchiveExceptionFromError(env, archive);
        return;
    }
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    (*env)->DeleteGlobalRef(env, jniData->passphraseClientData);
    (*env)->DeleteGlobalRef(env, jniData->passphraseCallback);
    jniData->passphraseClientData = clientDataRef;
    jniData->passphraseCallback = javaCallbackRef;
}

static void freeArchiveJniData(JNIEnv* env, struct archive *archive) {
    struct ArchiveJniData *jniData = archive_get_user_data(archive);
    if (jniData->openMemoryArray) {
        (*env)->ReleaseByteArrayElements(env, jniData->openMemoryJavaArray,
                jniData->openMemoryArray, jniData->openMemoryArrayReleaseMode);
    }
    (*env)->DeleteGlobalRef(env, jniData->openMemoryJavaArray);
    if (jniData->writeOpenMemoryJavaBuffer) {
        setByteBufferPosition(env, jniData->writeOpenMemoryJavaBuffer,
                jniData->writeOpenMemoryPosition + (jint) jniData->writeOpenMemoryUsed);
        if ((*env)->ExceptionCheck(env)) {
            throwArchiveException(env, ARCHIVE_FATAL, "ByteBuffer.position()");
        }
        (*env)->DeleteGlobalRef(env, jniData->writeOpenMemoryJavaBuffer);
    }
    if (jniData->hasReadClientData) {
        unsigned int readClientDataSize = archive_read_get_callback_data_size(archive);
        for (unsigned int i = 0; i < readClientDataSize; ++i) {
            jobject readClientData = archive_read_get_callback_data(archive, i);
            (*env)->DeleteGlobalRef(env, readClientData);
        }
    }
    (*env)->DeleteGlobalRef(env, jniData->writeClientData);
    (*env)->DeleteGlobalRef(env, jniData->readCallback);
    if (jniData->readArray) {
        (*env)->ReleaseByteArrayElements(env, jniData->readJavaArray, jniData->readArray,
                JNI_ABORT);
    }
    (*env)->DeleteGlobalRef(env, jniData->readJavaArray);
    (*env)->DeleteGlobalRef(env, jniData->skipCallback);
    (*env)->DeleteGlobalRef(env, jniData->seekCallback);
    (*env)->DeleteGlobalRef(env, jniData->writeCallback);
    (*env)->DeleteGlobalRef(env, jniData->openCallback);
    (*env)->DeleteGlobalRef(env, jniData->closeCallback);
    (*env)->DeleteGlobalRef(env, jniData->freeCallback);
    (*env)->DeleteGlobalRef(env, jniData->switchCallback);
    (*env)->DeleteGlobalRef(env, jniData->passphraseClientData);
    (*env)->DeleteGlobalRef(env, jniData->passphraseCallback);
    free(jniData->passphrase);
    free(jniData);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_free(
        JNIEnv* env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    // Must call archive_free() before freeArchiveJniData() because it may need to finish writing
    // data.
    int errorCode = archive_free(archive);
    freeArchiveJniData(env, archive);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT jint JNICALL
Java_me_zhanghai_android_libarchive_Archive_filterCount(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    return archive_filter_count(archive);
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_Archive_filterBytes(
        JNIEnv *env, jclass clazz, jlong javaArchive, jint index) {
    struct archive *archive = (struct archive *) javaArchive;
    return archive_filter_bytes(archive, index);
}

JNIEXPORT jint JNICALL
Java_me_zhanghai_android_libarchive_Archive_filterCode(
        JNIEnv *env, jclass clazz, jlong javaArchive, jint index) {
    struct archive *archive = (struct archive *) javaArchive;
    return archive_filter_code(archive, index);
}

JNIEXPORT jbyteArray JNICALL
Java_me_zhanghai_android_libarchive_Archive_filterName(
        JNIEnv *env, jclass clazz, jlong javaArchive, jint index) {
    struct archive *archive = (struct archive *) javaArchive;
    const char *filterName = archive_filter_name(archive, index);
    return newBytesFromString(env, filterName);
}

JNIEXPORT jint JNICALL
Java_me_zhanghai_android_libarchive_Archive_errno(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    return archive_errno(archive);
}

JNIEXPORT jbyteArray JNICALL
Java_me_zhanghai_android_libarchive_Archive_errorString(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    const char *string = archive_error_string(archive);
    return newBytesFromString(env, string);
}

JNIEXPORT jbyteArray JNICALL
Java_me_zhanghai_android_libarchive_Archive_formatName(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    const char *formatName = archive_format_name(archive);
    return newBytesFromString(env, formatName);
}

JNIEXPORT jint JNICALL
Java_me_zhanghai_android_libarchive_Archive_format(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    return archive_format(archive);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_clearError(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    archive_clear_error(archive);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_setError(
        JNIEnv *env, jclass clazz, jlong javaArchive, jint number, jbyteArray javaString) {
    struct archive *archive = (struct archive *) javaArchive;
    if (javaString) {
        char *string = mallocStringFromBytes(env, javaString);
        archive_set_error(archive, number, "%s", string);
        free(string);
    } else {
        archive_set_error(archive, number, NULL);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_copyError(
        JNIEnv *env, jclass clazz, jlong javaDestination, jlong javaSource) {
    struct archive *destination = (struct archive *) javaDestination;
    struct archive *source = (struct archive *) javaSource;
    archive_copy_error(destination, source);
}

JNIEXPORT jint JNICALL
Java_me_zhanghai_android_libarchive_Archive_fileCount(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    return archive_file_count(archive);
}

JNIEXPORT jbyteArray JNICALL
Java_me_zhanghai_android_libarchive_Archive_charset(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    const char *charset = archive_charset(archive);
    return newBytesFromString(env, charset);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_Archive_setCharset(
        JNIEnv *env, jclass clazz, jlong javaArchive, jbyteArray javaCharset) {
    struct archive *archive = (struct archive *) javaArchive;
    char *charset = mallocStringFromBytes(env, javaCharset);
    if (javaCharset && !charset) {
        throwArchiveException(env, ARCHIVE_FATAL, "mallocStringFromBytes");
        return;
    }
    int errorCode = archive_set_charset(archive, charset);
    free(charset);
    if (errorCode) {
        throwArchiveExceptionFromError(env, archive);
    }
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_clear(
        JNIEnv* env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    archive_entry_clear(entry);
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_clone(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return (jlong) archive_entry_clone(entry);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_free(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    archive_entry_free(entry);
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_new1(
        JNIEnv *env, jclass clazz) {
    return (jlong) archive_entry_new();
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_new2(
        JNIEnv *env, jclass clazz, jlong javaArchive) {
    struct archive *archive = (struct archive *) javaArchive;
    return (jlong) archive_entry_new2(archive);
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_atime(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_atime(entry);
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_atimeNsec(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_atime_nsec(entry);
}

JNIEXPORT jboolean JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_atimeIsSet(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_atime_is_set(entry) != 0;
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_birthtime(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_birthtime(entry);
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_birthtimeNsec(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_birthtime_nsec(entry);
}

JNIEXPORT jboolean JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_birthtimeIsSet(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_birthtime_is_set(entry) != 0;
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_ctime(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_ctime(entry);
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_ctimeNsec(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_ctime_nsec(entry);
}

JNIEXPORT jboolean JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_ctimeIsSet(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_ctime_is_set(entry) != 0;
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_dev(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_dev(entry);
}

JNIEXPORT jboolean JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_devIsSet(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_dev_is_set(entry) != 0;
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_devmajor(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_devmajor(entry);
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_devminor(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_devminor(entry);
}

JNIEXPORT jint JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_filetype(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_filetype(entry);
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_fflagsSet(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    unsigned long fflagsSet;
    unsigned long fflagsClear;
    archive_entry_fflags(entry, &fflagsSet, &fflagsClear);
    return fflagsSet;
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_fflagsClear(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    unsigned long fflagsSet;
    unsigned long fflagsClear;
    archive_entry_fflags(entry, &fflagsSet, &fflagsClear);
    return fflagsClear;
}

JNIEXPORT jbyteArray JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_fflagsText(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *fflagsText = archive_entry_fflags_text(entry);
    return newBytesFromString(env, fflagsText);
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_gid(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_gid(entry);
}

JNIEXPORT jbyteArray JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_gname(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *gname = archive_entry_gname(entry);
    return newBytesFromString(env, gname);
}

JNIEXPORT jstring JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_gnameUtf8(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *gnameUtf8 = archive_entry_gname_utf8(entry);
    return (*env)->NewStringUTF(env, gnameUtf8);
}

JNIEXPORT jbyteArray JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_hardlink(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *hardlink = archive_entry_hardlink(entry);
    return newBytesFromString(env, hardlink);
}

JNIEXPORT jstring JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_hardlinkUtf8(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *hardlinkUtf8 = archive_entry_hardlink_utf8(entry);
    return (*env)->NewStringUTF(env, hardlinkUtf8);
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_ino(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_ino64(entry);
}

JNIEXPORT jboolean JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_inoIsSet(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_ino_is_set(entry) != 0;
}

JNIEXPORT jint JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_mode(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_mode(entry);
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_mtime(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_mtime(entry);
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_mtimeNsec(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_mtime_nsec(entry);
}

JNIEXPORT jboolean JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_mtimeIsSet(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_mtime_is_set(entry) != 0;
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_nlink(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_nlink(entry);
}

JNIEXPORT jbyteArray JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_pathname(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *pathname = archive_entry_pathname(entry);
    return newBytesFromString(env, pathname);
}

JNIEXPORT jstring JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_pathnameUtf8(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *pathnameUtf8 = archive_entry_pathname_utf8(entry);
    return (*env)->NewStringUTF(env, pathnameUtf8);
}

JNIEXPORT jint JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_perm(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_perm(entry);
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_rdev(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_rdev(entry);
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_rdevmajor(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_rdevmajor(entry);
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_rdevminor(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_rdevminor(entry);
}

JNIEXPORT jbyteArray JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_sourcepath(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *sourcepath = archive_entry_sourcepath(entry);
    return newBytesFromString(env, sourcepath);
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_size(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_size(entry);
}

JNIEXPORT jboolean JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_sizeIsSet(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_size_is_set(entry) != 0;
}

JNIEXPORT jbyteArray JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_strmode(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *strmode = archive_entry_strmode(entry);
    return newBytesFromString(env, strmode);
}

JNIEXPORT jbyteArray JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_symlink(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *symlink = archive_entry_symlink(entry);
    return newBytesFromString(env, symlink);
}

JNIEXPORT jstring JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_symlinkUtf8(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *symlinkUtf8 = archive_entry_symlink_utf8(entry);
    return (*env)->NewStringUTF(env, symlinkUtf8);
}

JNIEXPORT jint JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_symlinkType(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_symlink_type(entry);
}

JNIEXPORT jlong JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_uid(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_uid(entry);
}

JNIEXPORT jbyteArray JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_uname(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *uname = archive_entry_uname(entry);
    return newBytesFromString(env, uname);
}

JNIEXPORT jstring JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_unameUtf8(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *unameUtf8 = archive_entry_uname_utf8(entry);
    return (*env)->NewStringUTF(env, unameUtf8);
}

JNIEXPORT jboolean JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_isDataEncrypted(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_is_data_encrypted(entry) != 0;
}

JNIEXPORT jboolean JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_isMetadataEncrypted(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_is_metadata_encrypted(entry) != 0;
}

JNIEXPORT jboolean JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_isEncrypted(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    return archive_entry_is_encrypted(entry) != 0;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setAtime(
        JNIEnv *env, jclass clazz, jlong javaEntry, jlong javaAtime, jlong javaAtimeNsec) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    time_t atime = (time_t) javaAtime;
    long atimeNsec = (long) javaAtimeNsec;
    archive_entry_set_atime(entry, atime, atimeNsec);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_unsetAtime(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    archive_entry_unset_atime(entry);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setBirthtime(
        JNIEnv *env, jclass clazz, jlong javaEntry, jlong javaBirthtime, jlong javaBirthtimeNsec) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    time_t birthtime = (time_t) javaBirthtime;
    long birthtimeNsec = (long) javaBirthtimeNsec;
    archive_entry_set_birthtime(entry, birthtime, birthtimeNsec);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_unsetBirthtime(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    archive_entry_unset_birthtime(entry);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setCtime(
        JNIEnv *env, jclass clazz, jlong javaEntry, jlong javaCtime, jlong javaCtimeNsec) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    time_t ctime = (time_t) javaCtime;
    long ctimeNsec = (long) javaCtimeNsec;
    archive_entry_set_ctime(entry, ctime, ctimeNsec);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_unsetCtime(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    archive_entry_unset_ctime(entry);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setDev(
        JNIEnv *env, jclass clazz, jlong javaEntry, jlong dev) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    archive_entry_set_dev(entry, dev);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setDevmajor(
        JNIEnv *env, jclass clazz, jlong javaEntry, jlong devmajor) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    archive_entry_set_devmajor(entry, devmajor);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setDevminor(
        JNIEnv *env, jclass clazz, jlong javaEntry, jlong devminor) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    archive_entry_set_devminor(entry, devminor);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setFiletype(
        JNIEnv *env, jclass clazz, jlong javaEntry, jint filetype) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    archive_entry_set_filetype(entry, filetype);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setFflags(
        JNIEnv *env, jclass clazz, jlong javaEntry, jlong set, jlong clear) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    archive_entry_set_fflags(entry, set, clear);
}

JNIEXPORT jint JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setFflagsText(
        JNIEnv *env, jclass clazz, jlong javaEntry, jbyteArray javaFflags) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    char *fflags = mallocStringFromBytes(env, javaFflags);
    const char *fflags2 = archive_entry_copy_fflags_text(entry, fflags);
    jint index = fflags2 - fflags;
    free(fflags);
    return index;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setGid(
        JNIEnv *env, jclass clazz, jlong javaEntry, jlong gid) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    archive_entry_set_gid(entry, gid);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setGname(
        JNIEnv *env, jclass clazz, jlong javaEntry, jbyteArray javaGname) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    char *gname = mallocStringFromBytes(env, javaGname);
    archive_entry_set_gname(entry, gname);
    free(gname);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setGnameUtf8(
        JNIEnv *env, jclass clazz, jlong javaEntry, jstring javaGname) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *gname = (*env)->GetStringUTFChars(env, javaGname, NULL);
    archive_entry_set_gname_utf8(entry, gname);
    (*env)->ReleaseStringUTFChars(env, javaGname, gname);
}

JNIEXPORT jboolean JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_updateGnameUtf8(
        JNIEnv *env, jclass clazz, jlong javaEntry, jstring javaGname) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *gname = (*env)->GetStringUTFChars(env, javaGname, NULL);
    int updated = archive_entry_update_gname_utf8(entry, gname);
    (*env)->ReleaseStringUTFChars(env, javaGname, gname);
    return updated != 0;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setHardlink(
        JNIEnv *env, jclass clazz, jlong javaEntry, jbyteArray javaHardlink) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    char *hardlink = mallocStringFromBytes(env, javaHardlink);
    archive_entry_set_hardlink(entry, hardlink);
    free(hardlink);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setHardlinkUtf8(
        JNIEnv *env, jclass clazz, jlong javaEntry, jstring javaHardlink) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *hardlink = (*env)->GetStringUTFChars(env, javaHardlink, NULL);
    archive_entry_set_hardlink_utf8(entry, hardlink);
    (*env)->ReleaseStringUTFChars(env, javaHardlink, hardlink);
}

JNIEXPORT jboolean JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_updateHardlinkUtf8(
        JNIEnv *env, jclass clazz, jlong javaEntry, jstring javaHardlink) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *hardlink = (*env)->GetStringUTFChars(env, javaHardlink, NULL);
    int updated = archive_entry_update_hardlink_utf8(entry, hardlink);
    (*env)->ReleaseStringUTFChars(env, javaHardlink, hardlink);
    return updated != 0;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setIno(
        JNIEnv *env, jclass clazz, jlong javaEntry, jlong ino) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    archive_entry_set_ino(entry, ino);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setLink(
        JNIEnv *env, jclass clazz, jlong javaEntry, jbyteArray javaLink) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    char *link = mallocStringFromBytes(env, javaLink);
    archive_entry_set_link(entry, link);
    free(link);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setLinkUtf8(
        JNIEnv *env, jclass clazz, jlong javaEntry, jstring javaLink) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *link = (*env)->GetStringUTFChars(env, javaLink, NULL);
    archive_entry_set_link_utf8(entry, link);
    (*env)->ReleaseStringUTFChars(env, javaLink, link);
}

JNIEXPORT jboolean JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_updateLinkUtf8(
        JNIEnv *env, jclass clazz, jlong javaEntry, jstring javaLink) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *link = (*env)->GetStringUTFChars(env, javaLink, NULL);
    int updated = archive_entry_update_link_utf8(entry, link);
    (*env)->ReleaseStringUTFChars(env, javaLink, link);
    return updated != 0;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setMode(
        JNIEnv *env, jclass clazz, jlong javaEntry, jint mode) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    archive_entry_set_mode(entry, mode);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setMtime(
        JNIEnv *env, jclass clazz, jlong javaEntry, jlong javaMtime, jlong javaMtimeNsec) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    time_t mtime = (time_t) javaMtime;
    long mtimeNsec = (long) javaMtimeNsec;
    archive_entry_set_mtime(entry, mtime, mtimeNsec);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_unsetMtime(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    archive_entry_unset_mtime(entry);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setNlink(
        JNIEnv *env, jclass clazz, jlong javaEntry, jint nlink) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    archive_entry_set_nlink(entry, nlink);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setPathname(
        JNIEnv *env, jclass clazz, jlong javaEntry, jbyteArray javaPathname) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    char *pathname = mallocStringFromBytes(env, javaPathname);
    archive_entry_set_pathname(entry, pathname);
    free(pathname);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setPathnameUtf8(
        JNIEnv *env, jclass clazz, jlong javaEntry, jstring javaPathname) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *pathname = (*env)->GetStringUTFChars(env, javaPathname, NULL);
    archive_entry_set_pathname_utf8(entry, pathname);
    (*env)->ReleaseStringUTFChars(env, javaPathname, pathname);
}

JNIEXPORT jboolean JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_updatePathnameUtf8(
        JNIEnv *env, jclass clazz, jlong javaEntry, jstring javaPathname) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *pathname = (*env)->GetStringUTFChars(env, javaPathname, NULL);
    int updated = archive_entry_update_pathname_utf8(entry, pathname);
    (*env)->ReleaseStringUTFChars(env, javaPathname, pathname);
    return updated != 0;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setPerm(
        JNIEnv *env, jclass clazz, jlong javaEntry, jint perm) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    archive_entry_set_perm(entry, perm);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setRdev(
        JNIEnv *env, jclass clazz, jlong javaEntry, jlong rdev) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    archive_entry_set_rdev(entry, rdev);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setRdevmajor(
        JNIEnv *env, jclass clazz, jlong javaEntry, jlong rdevmajor) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    archive_entry_set_rdevmajor(entry, rdevmajor);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setRdevminor(
        JNIEnv *env, jclass clazz, jlong javaEntry, jlong rdevminor) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    archive_entry_set_rdevminor(entry, rdevminor);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setSize(
        JNIEnv *env, jclass clazz, jlong javaEntry, jlong size) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    archive_entry_set_size(entry, size);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_unsetSize(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    archive_entry_unset_size(entry);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setSourcepath(
        JNIEnv *env, jclass clazz, jlong javaEntry, jbyteArray javaSourcepath) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    char *sourcepath = mallocStringFromBytes(env, javaSourcepath);
    archive_entry_copy_sourcepath(entry, sourcepath);
    free(sourcepath);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setSymlink(
        JNIEnv *env, jclass clazz, jlong javaEntry, jbyteArray javaSymlink) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    char *symlink = mallocStringFromBytes(env, javaSymlink);
    archive_entry_set_symlink(entry, symlink);
    free(symlink);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setSymlinkType(
        JNIEnv *env, jclass clazz, jlong javaEntry, jint type) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    archive_entry_set_symlink_type(entry, type);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setSymlinkUtf8(
        JNIEnv *env, jclass clazz, jlong javaEntry, jstring javaSymlink) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *symlink = (*env)->GetStringUTFChars(env, javaSymlink, NULL);
    archive_entry_set_symlink_utf8(entry, symlink);
    (*env)->ReleaseStringUTFChars(env, javaSymlink, symlink);
}

JNIEXPORT jboolean JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_updateSymlinkUtf8(
        JNIEnv *env, jclass clazz, jlong javaEntry, jstring javaSymlink) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *symlink = (*env)->GetStringUTFChars(env, javaSymlink, NULL);
    int updated = archive_entry_update_symlink_utf8(entry, symlink);
    (*env)->ReleaseStringUTFChars(env, javaSymlink, symlink);
    return updated != 0;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setUid(
        JNIEnv *env, jclass clazz, jlong javaEntry, jlong uid) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    archive_entry_set_uid(entry, uid);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setUname(
        JNIEnv *env, jclass clazz, jlong javaEntry, jbyteArray javaUname) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    char *uname = mallocStringFromBytes(env, javaUname);
    archive_entry_set_uname(entry, uname);
    free(uname);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setUnameUtf8(
        JNIEnv *env, jclass clazz, jlong javaEntry, jstring javaUname) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *uname = (*env)->GetStringUTFChars(env, javaUname, NULL);
    archive_entry_set_uname_utf8(entry, uname);
    (*env)->ReleaseStringUTFChars(env, javaUname, uname);
}

JNIEXPORT jboolean JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_updateUnameUtf8(
        JNIEnv *env, jclass clazz, jlong javaEntry, jstring javaUname) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const char *uname = (*env)->GetStringUTFChars(env, javaUname, NULL);
    int updated = archive_entry_update_uname_utf8(entry, uname);
    (*env)->ReleaseStringUTFChars(env, javaUname, uname);
    return updated != 0;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setDataEncrypted(
        JNIEnv *env, jclass clazz, jlong javaEntry, jboolean javaEncrypted) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    char encrypted = (char) javaEncrypted;
    archive_entry_set_is_data_encrypted(entry, encrypted);
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setMetadataEncrypted(
        JNIEnv *env, jclass clazz, jlong javaEntry, jboolean javaEncrypted) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    char encrypted = (char) javaEncrypted;
    archive_entry_set_is_metadata_encrypted(entry, encrypted);
}

JNIEXPORT jobject JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_stat(
        JNIEnv *env, jclass clazz, jlong javaEntry) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const struct stat *stat = archive_entry_stat(entry);
    jobject javaStat = newStructStat(env, stat);
    return javaStat;
}

JNIEXPORT void JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_setStat(
        JNIEnv *env, jclass clazz, jlong javaEntry, jobject javaStat) {
    if (!javaStat) {
        return;
    }
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    struct stat stat = {};
    readStructStat(env, javaStat, &stat);
    archive_entry_copy_stat(entry, &stat);
}

JNIEXPORT jobject JNICALL
Java_me_zhanghai_android_libarchive_ArchiveEntry_digest(
        JNIEnv *env, jclass clazz, jlong javaEntry, jint type) {
    struct archive_entry *entry = (struct archive_entry *) javaEntry;
    const unsigned char *digest = archive_entry_digest(entry, type);
    if (!digest) {
        return NULL;
    }
    jlong digestSize = archive_entry_digest_size(type);
    jobject javaDigest = (*env)->NewDirectByteBuffer(env, (void *) digest, digestSize);
    if (!javaDigest) {
        javaDigest = newHeapByteBufferFromBuffer(env, digest, digestSize, false);
    }
    return javaDigest;
}

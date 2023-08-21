# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

cmake_minimum_required(VERSION 3.22.1)

project(libarchive-android
        LANGUAGES C ASM)
# CMAKE_INTERPROCEDURAL_OPTIMIZATION sets -fuse-ld=gold and -flto=thin.
#set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
add_compile_options(
        -fdata-sections
        -ffunction-sections
        -flto)
add_link_options(
        LINKER:--gc-sections)

# https://developer.android.com/ndk/guides/stable_apis#zlib_compression
# Platform libz has custom SIMD optimizations (*_simd.c) so we should use that instead of building
# from official zlib repository.
find_library(Z_LIBRARY libz.a)

# https://android.googlesource.com/platform/external/bzip2/+/main/Android.bp
add_library(bz STATIC
        src/main/jni/external/bzip2/blocksort.c
        src/main/jni/external/bzip2/bzlib.c
        src/main/jni/external/bzip2/compress.c
        src/main/jni/external/bzip2/crctable.c
        src/main/jni/external/bzip2/decompress.c
        src/main/jni/external/bzip2/huffman.c
        src/main/jni/external/bzip2/randtable.c)
target_compile_definitions(bz
        PRIVATE
        USE_MMAP)
target_compile_options(bz
        PRIVATE
        -Werror
        -Wno-unused-parameter)
if(CMAKE_ANDROID_ARCH_ABI STREQUAL "armeabi-v7a")
    target_compile_options(bz
            PRIVATE
            -marm)
endif()
target_include_directories(bz
        PUBLIC
        src/main/jni/external/bzip2)

# https://github.com/tukaani-project/xz/blob/master/configure.ac
function(configure_liblzma)
    set(HAVE_BSWAP_16 1)
    set(HAVE_BSWAP_32 1)
    set(HAVE_BSWAP_64 1)
    set(HAVE_BYTESWAP_H 1)
    set(HAVE_CHECK_CRC32 1)
    set(HAVE_CHECK_CRC64 1)
    set(HAVE_CHECK_SHA256 1)
    set(HAVE_CLOCK_GETTIME 1)
    set(HAVE_CLOCK_MONOTONIC 1)
    if(CMAKE_ANDROID_ARCH_ABI MATCHES "x86|x86_64")
        set(HAVE_CPUID_H 1)
    endif()
    set(HAVE_DECODERS 1)
    set(HAVE_DECODER_ARM 1)
    set(HAVE_DECODER_ARM64 1)
    set(HAVE_DECODER_ARMTHUMB 1)
    set(HAVE_DECODER_DELTA 1)
    set(HAVE_DECODER_IA64 1)
    set(HAVE_DECODER_LZMA1 1)
    set(HAVE_DECODER_LZMA2 1)
    set(HAVE_DECODER_POWERPC 1)
    set(HAVE_DECODER_SPARC 1)
    set(HAVE_DECODER_X86 1)
    set(HAVE_ENCODERS 1)
    set(HAVE_ENCODER_ARM 1)
    set(HAVE_ENCODER_ARM64 1)
    set(HAVE_ENCODER_ARMTHUMB 1)
    set(HAVE_ENCODER_DELTA 1)
    set(HAVE_ENCODER_IA64 1)
    set(HAVE_ENCODER_LZMA1 1)
    set(HAVE_ENCODER_LZMA2 1)
    set(HAVE_ENCODER_POWERPC 1)
    set(HAVE_ENCODER_SPARC 1)
    set(HAVE_ENCODER_X86 1)
    set(HAVE_FUNC_ATTRIBUTE_CONSTRUCTOR 1)
    if(CMAKE_ANDROID_ARCH_ABI MATCHES "x86|x86_64")
        set(HAVE_IMMINTRIN_H 1)
    endif()
    set(HAVE_INTTYPES_H 1)
    set(HAVE_LZIP_DECODER 1)
    set(HAVE_MBRTOWC 1)
    set(HAVE_MF_BT2 1)
    set(HAVE_MF_BT3 1)
    set(HAVE_MF_BT4 1)
    set(HAVE_MF_HC3 1)
    set(HAVE_MF_HC4 1)
    set(HAVE_PTHREAD_CONDATTR_SETCLOCK 1)
    set(HAVE_STDBOOL_H 1)
    set(HAVE_STDINT_H 1)
    set(HAVE_SYS_BYTEORDER_H 1)
    set(HAVE_SYS_ENDIAN_H 1)
    set(HAVE_SYS_PARAM_H 1)
    if(CMAKE_ANDROID_ARCH_ABI MATCHES "x86|x86_64")
        set(HAVE_USABLE_CLMUL 1)
    endif()
    set(HAVE_VISIBILITY 1)
    set(HAVE_WCWIDTH 1)
    set(HAVE__BOOL 1)
    set(HAVE__MM_MOVEMASK_EPI8 1)
    set(HAVE___BUILTIN_ASSUME_ALIGNED 1)
    set(HAVE___BUILTIN_BSWAPXX 1)
    set(MYTHREAD_POSIX 1)
    set(NDEBUG 1)
    # https://developer.android.com/ndk/guides/cpu-features#cpu_core_counts_using_libcs_sysconf3
    set(TUKLIB_CPUCORES_SYSCONF 1)
    # https://github.com/tukaani-project/xz/blob/master/m4/tuklib_integer.m4
    set(TUKLIB_FAST_UNALIGNED_ACCESS 1)
    # https://developer.android.com/reference/android/system/OsConstants#_SC_PAGESIZE
    # https://developer.android.com/reference/android/system/OsConstants#_SC_PHYS_PAGES
    set(TUKLIB_PHYSMEM_SYSCONF 1)
    set(_FILE_OFFSET_BITS 64)
    configure_file(src/main/jni/liblzma-config.h.in liblzma/config.h @ONLY)
endfunction()
configure_liblzma()

# https://github.com/tukaani-project/xz/blob/master/src/liblzma/Makefile.am
add_library(lzma STATIC
        # Makefile.am
        src/main/jni/external/xz/src/common/tuklib_physmem.c
        src/main/jni/external/xz/src/common/tuklib_cpucores.c
        # common/Makefile.inc
        src/main/jni/external/xz/src/liblzma/common/common.c
        src/main/jni/external/xz/src/liblzma/common/block_util.c
        src/main/jni/external/xz/src/liblzma/common/easy_preset.c
        src/main/jni/external/xz/src/liblzma/common/filter_common.c
        src/main/jni/external/xz/src/liblzma/common/hardware_physmem.c
        src/main/jni/external/xz/src/liblzma/common/index.c
        src/main/jni/external/xz/src/liblzma/common/stream_flags_common.c
        src/main/jni/external/xz/src/liblzma/common/string_conversion.c
        src/main/jni/external/xz/src/liblzma/common/vli_size.c
        src/main/jni/external/xz/src/liblzma/common/hardware_cputhreads.c
        src/main/jni/external/xz/src/liblzma/common/outqueue.c
        src/main/jni/external/xz/src/liblzma/common/alone_encoder.c
        src/main/jni/external/xz/src/liblzma/common/block_buffer_encoder.c
        src/main/jni/external/xz/src/liblzma/common/block_encoder.c
        src/main/jni/external/xz/src/liblzma/common/block_header_encoder.c
        src/main/jni/external/xz/src/liblzma/common/easy_buffer_encoder.c
        src/main/jni/external/xz/src/liblzma/common/easy_encoder.c
        src/main/jni/external/xz/src/liblzma/common/easy_encoder_memusage.c
        src/main/jni/external/xz/src/liblzma/common/filter_buffer_encoder.c
        src/main/jni/external/xz/src/liblzma/common/filter_encoder.c
        src/main/jni/external/xz/src/liblzma/common/filter_flags_encoder.c
        src/main/jni/external/xz/src/liblzma/common/index_encoder.c
        src/main/jni/external/xz/src/liblzma/common/stream_buffer_encoder.c
        src/main/jni/external/xz/src/liblzma/common/stream_encoder.c
        src/main/jni/external/xz/src/liblzma/common/stream_flags_encoder.c
        src/main/jni/external/xz/src/liblzma/common/vli_encoder.c
        src/main/jni/external/xz/src/liblzma/common/stream_encoder_mt.c
        src/main/jni/external/xz/src/liblzma/common/alone_decoder.c
        src/main/jni/external/xz/src/liblzma/common/auto_decoder.c
        src/main/jni/external/xz/src/liblzma/common/block_buffer_decoder.c
        src/main/jni/external/xz/src/liblzma/common/block_decoder.c
        src/main/jni/external/xz/src/liblzma/common/block_header_decoder.c
        src/main/jni/external/xz/src/liblzma/common/easy_decoder_memusage.c
        src/main/jni/external/xz/src/liblzma/common/file_info.c
        src/main/jni/external/xz/src/liblzma/common/filter_buffer_decoder.c
        src/main/jni/external/xz/src/liblzma/common/filter_decoder.c
        src/main/jni/external/xz/src/liblzma/common/filter_flags_decoder.c
        src/main/jni/external/xz/src/liblzma/common/index_decoder.c
        src/main/jni/external/xz/src/liblzma/common/index_hash.c
        src/main/jni/external/xz/src/liblzma/common/stream_buffer_decoder.c
        src/main/jni/external/xz/src/liblzma/common/stream_decoder.c
        src/main/jni/external/xz/src/liblzma/common/stream_flags_decoder.c
        src/main/jni/external/xz/src/liblzma/common/vli_decoder.c
        src/main/jni/external/xz/src/liblzma/common/stream_decoder_mt.c
        src/main/jni/external/xz/src/liblzma/common/lzip_decoder.c
        # check/Makefile.inc
        src/main/jni/external/xz/src/liblzma/check/check.c
        src/main/jni/external/xz/src/liblzma/check/crc32_table.c
        src/main/jni/external/xz/src/liblzma/check/crc32_fast.c
        src/main/jni/external/xz/src/liblzma/check/crc64_table.c
        src/main/jni/external/xz/src/liblzma/check/crc64_fast.c
        src/main/jni/external/xz/src/liblzma/check/sha256.c
        # lz/Makefile.inc
        src/main/jni/external/xz/src/liblzma/lz/lz_encoder.c
        src/main/jni/external/xz/src/liblzma/lz/lz_encoder_mf.c
        src/main/jni/external/xz/src/liblzma/lz/lz_decoder.c
        # lzma/Makefile.inc
        src/main/jni/external/xz/src/liblzma/lzma/lzma_encoder_presets.c
        src/main/jni/external/xz/src/liblzma/lzma/lzma_encoder.c
        src/main/jni/external/xz/src/liblzma/lzma/lzma_encoder_optimum_fast.c
        src/main/jni/external/xz/src/liblzma/lzma/lzma_encoder_optimum_normal.c
        src/main/jni/external/xz/src/liblzma/lzma/fastpos_table.c
        src/main/jni/external/xz/src/liblzma/lzma/lzma_decoder.c
        src/main/jni/external/xz/src/liblzma/lzma/lzma2_encoder.c
        src/main/jni/external/xz/src/liblzma/lzma/lzma2_decoder.c
        # rangecoder/Makefile.inc
        src/main/jni/external/xz/src/liblzma/rangecoder/price_table.c
        # delta/Makefile.inc
        src/main/jni/external/xz/src/liblzma/delta/delta_common.c
        src/main/jni/external/xz/src/liblzma/delta/delta_encoder.c
        src/main/jni/external/xz/src/liblzma/delta/delta_decoder.c
        # simple/Makefile.inc
        src/main/jni/external/xz/src/liblzma/simple/simple_coder.c
        src/main/jni/external/xz/src/liblzma/simple/simple_encoder.c
        src/main/jni/external/xz/src/liblzma/simple/simple_decoder.c
        src/main/jni/external/xz/src/liblzma/simple/x86.c
        src/main/jni/external/xz/src/liblzma/simple/powerpc.c
        src/main/jni/external/xz/src/liblzma/simple/ia64.c
        src/main/jni/external/xz/src/liblzma/simple/arm.c
        src/main/jni/external/xz/src/liblzma/simple/armthumb.c
        src/main/jni/external/xz/src/liblzma/simple/arm64.c
        src/main/jni/external/xz/src/liblzma/simple/sparc.c)
target_compile_definitions(lzma
        PRIVATE
        HAVE_CONFIG_H
        TUKLIB_SYMBOL_PREFIX=lzma_)
target_compile_options(lzma
        PRIVATE
        -Wall
        -Wextra
        -Wvla
        -Wformat=2
        -Winit-self
        -Wmissing-include-dirs
        -Wstrict-overflow=3
        -Wfloat-equal
        -Wundef
        -Wshadow
        -Wpointer-arith
        -Wbad-function-cast
        -Wwrite-strings
        -Wdate-time
        -Wsign-conversion
        -Wfloat-conversion
        -Waggregate-return
        -Wstrict-prototypes
        -Wold-style-definition
        -Wmissing-prototypes
        -Wmissing-declarations
        -Wredundant-decls
        -Wc99-compat
        -Wc11-extensions
        -Wc2x-extensions
        -Wpre-c2x-compat
        -Warray-bounds-pointer-arithmetic
        -Wassign-enum
        -Wconditional-uninitialized
        -Wdocumentation
        -Wduplicate-enum
        -Wempty-translation-unit
        -Wflexible-array-extensions
        -Wmissing-variable-declarations
        -Wnewline-eof
        -Wshift-sign-overflow
        -Wstring-conversion
        -Werror)
target_include_directories(lzma
        PUBLIC
        src/main/jni/external/xz/src/liblzma/api
        PRIVATE
        src/main/jni/external/xz/src/liblzma/common
        src/main/jni/external/xz/src/liblzma/check
        src/main/jni/external/xz/src/liblzma/lz
        src/main/jni/external/xz/src/liblzma/rangecoder
        src/main/jni/external/xz/src/liblzma/lzma
        src/main/jni/external/xz/src/liblzma/delta
        src/main/jni/external/xz/src/liblzma/simple
        src/main/jni/external/xz/src/common
        "${CMAKE_CURRENT_BINARY_DIR}/liblzma")
target_link_options(lzma
        PRIVATE
        -no-undefined
        -version-info 10:99:5)

# https://android.googlesource.com/platform/external/lz4/+/main/lib/Android.bp
add_library(lz4 STATIC
        src/main/jni/external/lz4/lib/lz4.c
        src/main/jni/external/lz4/lib/lz4hc.c
        src/main/jni/external/lz4/lib/lz4frame.c
        src/main/jni/external/lz4/lib/xxhash.c)
target_compile_options(lz4
        PRIVATE
        -Wall
        -Werror)
target_include_directories(lz4
        PUBLIC
        src/main/jni/external/lz4/lib)

# https://android.googlesource.com/platform/external/zstd/+/main/Android.bp
file(GLOB_RECURSE ZSTD_C_SOURCES
        # Don't include unnecessary deprecated/, dictBuilder/ and legacy/.
        #src/main/jni/external/zstd/lib/*.c
        src/main/jni/external/zstd/lib/common/*.c
        src/main/jni/external/zstd/lib/compress/*.c
        src/main/jni/external/zstd/lib/decompress/*.c)
add_library(zstd STATIC
        ${ZSTD_C_SOURCES})
if(CMAKE_ANDROID_ARCH_ABI STREQUAL "x86_64")
    target_sources(zstd
            PRIVATE
            src/main/jni/external/zstd/lib/decompress/huf_decompress_amd64.S)
endif()
target_compile_options(zstd
        PRIVATE
        -Wall
        -Werror)
target_include_directories(zstd
        PUBLIC
        src/main/jni/external/zstd/lib
        PRIVATE
        src/main/jni/external/zstd/lib/common)

# https://github.com/Mbed-TLS/mbedtls/blob/v2.28.4/CMakeLists.txt
# https://github.com/Mbed-TLS/mbedtls/blob/v2.28.4/library/CMakeLists.txt
add_library(mbedcrypto STATIC
        # src_crypto
        src/main/jni/external/mbedtls/library/aes.c
        src/main/jni/external/mbedtls/library/aesni.c
        src/main/jni/external/mbedtls/library/arc4.c
        src/main/jni/external/mbedtls/library/aria.c
        src/main/jni/external/mbedtls/library/asn1parse.c
        src/main/jni/external/mbedtls/library/asn1write.c
        src/main/jni/external/mbedtls/library/base64.c
        src/main/jni/external/mbedtls/library/bignum.c
        src/main/jni/external/mbedtls/library/blowfish.c
        src/main/jni/external/mbedtls/library/camellia.c
        src/main/jni/external/mbedtls/library/ccm.c
        src/main/jni/external/mbedtls/library/chacha20.c
        src/main/jni/external/mbedtls/library/chachapoly.c
        src/main/jni/external/mbedtls/library/cipher.c
        src/main/jni/external/mbedtls/library/cipher_wrap.c
        src/main/jni/external/mbedtls/library/constant_time.c
        src/main/jni/external/mbedtls/library/cmac.c
        src/main/jni/external/mbedtls/library/ctr_drbg.c
        src/main/jni/external/mbedtls/library/des.c
        src/main/jni/external/mbedtls/library/dhm.c
        src/main/jni/external/mbedtls/library/ecdh.c
        src/main/jni/external/mbedtls/library/ecdsa.c
        src/main/jni/external/mbedtls/library/ecjpake.c
        src/main/jni/external/mbedtls/library/ecp.c
        src/main/jni/external/mbedtls/library/ecp_curves.c
        src/main/jni/external/mbedtls/library/entropy.c
        src/main/jni/external/mbedtls/library/entropy_poll.c
        src/main/jni/external/mbedtls/library/error.c
        src/main/jni/external/mbedtls/library/gcm.c
        src/main/jni/external/mbedtls/library/havege.c
        src/main/jni/external/mbedtls/library/hkdf.c
        src/main/jni/external/mbedtls/library/hmac_drbg.c
        src/main/jni/external/mbedtls/library/md.c
        src/main/jni/external/mbedtls/library/md2.c
        src/main/jni/external/mbedtls/library/md4.c
        src/main/jni/external/mbedtls/library/md5.c
        src/main/jni/external/mbedtls/library/memory_buffer_alloc.c
        src/main/jni/external/mbedtls/library/mps_reader.c
        src/main/jni/external/mbedtls/library/mps_trace.c
        src/main/jni/external/mbedtls/library/nist_kw.c
        src/main/jni/external/mbedtls/library/oid.c
        src/main/jni/external/mbedtls/library/padlock.c
        src/main/jni/external/mbedtls/library/pem.c
        src/main/jni/external/mbedtls/library/pk.c
        src/main/jni/external/mbedtls/library/pk_wrap.c
        src/main/jni/external/mbedtls/library/pkcs12.c
        src/main/jni/external/mbedtls/library/pkcs5.c
        src/main/jni/external/mbedtls/library/pkparse.c
        src/main/jni/external/mbedtls/library/pkwrite.c
        src/main/jni/external/mbedtls/library/platform.c
        src/main/jni/external/mbedtls/library/platform_util.c
        src/main/jni/external/mbedtls/library/poly1305.c
        src/main/jni/external/mbedtls/library/psa_crypto.c
        src/main/jni/external/mbedtls/library/psa_crypto_aead.c
        src/main/jni/external/mbedtls/library/psa_crypto_cipher.c
        src/main/jni/external/mbedtls/library/psa_crypto_client.c
        src/main/jni/external/mbedtls/library/psa_crypto_driver_wrappers.c
        src/main/jni/external/mbedtls/library/psa_crypto_ecp.c
        src/main/jni/external/mbedtls/library/psa_crypto_hash.c
        src/main/jni/external/mbedtls/library/psa_crypto_mac.c
        src/main/jni/external/mbedtls/library/psa_crypto_rsa.c
        src/main/jni/external/mbedtls/library/psa_crypto_se.c
        src/main/jni/external/mbedtls/library/psa_crypto_slot_management.c
        src/main/jni/external/mbedtls/library/psa_crypto_storage.c
        src/main/jni/external/mbedtls/library/psa_its_file.c
        src/main/jni/external/mbedtls/library/ripemd160.c
        src/main/jni/external/mbedtls/library/rsa.c
        src/main/jni/external/mbedtls/library/rsa_internal.c
        src/main/jni/external/mbedtls/library/sha1.c
        src/main/jni/external/mbedtls/library/sha256.c
        src/main/jni/external/mbedtls/library/sha512.c
        src/main/jni/external/mbedtls/library/threading.c
        src/main/jni/external/mbedtls/library/timing.c
        src/main/jni/external/mbedtls/library/version.c
        src/main/jni/external/mbedtls/library/version_features.c
        src/main/jni/external/mbedtls/library/xtea.c)
target_compile_options(mbedcrypto
        PRIVATE
        -Wall
        -Wextra
        -Wwrite-strings
        -Wpointer-arith
        -Wimplicit-fallthrough
        -Wshadow
        -Wvla
        -Wformat=2
        -Wno-format-nonliteral
        -Werror
        -Wmissing-declarations
        -Wmissing-prototypes
        -Wdocumentation
        -Wno-documentation-deprecated-sync
        -Wunreachable-code)
target_include_directories(mbedcrypto
        PUBLIC
        src/main/jni/external/mbedtls/include
        PRIVATE
        src/main/jni/external/mbedtls/library)

# https://github.com/libarchive/libarchive/blob/master/CMakeLists.txt
set(LIBARCHIVE_CONFIG_DIR "${CMAKE_CURRENT_BINARY_DIR}/libarchive-config")
function(configure_libarchive)
    set(HAVE_LIBZ 1)
    set(HAVE_ZLIB_H 1)
    set(HAVE_LIBBZ2 1)
    set(HAVE_BZLIB_H 1)
    set(HAVE_LIBLZMA 1)
    set(HAVE_LZMA_H 1)
    set(HAVE_LZMA_STREAM_ENCODER_MT 1)
    set(HAVE_LIBLZ4 1)
    set(HAVE_LZ4_H 1)
    set(HAVE_LZ4HC_H 1)
    set(HAVE_ZSTD_H 1)
    set(HAVE_LIBZSTD 1)
    set(HAVE_LIBZSTD_COMPRESSOR 1)
    set(HAVE_LIBMBEDCRYPTO 1)
    set(HAVE_MBEDTLS_AES_H 1)
    set(HAVE_MBEDTLS_MD_H 1)
    set(HAVE_MBEDTLS_PKCS5_H 1)
    set(ARCHIVE_CRYPTO_MD5_MBEDTLS 1)
    set(ARCHIVE_CRYPTO_RMD160_MBEDTLS 1)
    set(ARCHIVE_CRYPTO_SHA1_MBEDTLS 1)
    set(ARCHIVE_CRYPTO_SHA256_MBEDTLS 1)
    set(ARCHIVE_CRYPTO_SHA384_MBEDTLS 1)
    set(ARCHIVE_CRYPTO_SHA512_MBEDTLS 1)
    include(src/main/jni/LibarchiveChecks.cmake)
    configure_file(src/main/jni/external/libarchive/build/cmake/config.h.in
            "${LIBARCHIVE_CONFIG_DIR}/config.h" @ONLY)
endfunction()
configure_libarchive()

# https://github.com/libarchive/libarchive/blob/master/libarchive/CMakeLists.txt
find_package(Patch REQUIRED)
set(LIBARCHIVE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/main/jni/external/libarchive/libarchive")
set(LIBARCHIVE_PATCH_DIR "${CMAKE_CURRENT_BINARY_DIR}/libarchive-patch")
set(LIBARCHIVE_SOURCES
        # libarchive_SOURCES
        "${LIBARCHIVE_SOURCE_DIR}/archive_acl.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_acl_private.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_check_magic.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_cmdline.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_cmdline_private.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_crc32.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_cryptor.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_cryptor_private.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_digest.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_digest_private.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_endian.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_entry.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_entry.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_entry_copy_stat.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_entry_link_resolver.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_entry_locale.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_entry_private.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_entry_sparse.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_entry_stat.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_entry_strmode.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_entry_xattr.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_getdate.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_getdate.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_hmac.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_hmac_private.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_match.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_openssl_evp_private.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_openssl_hmac_private.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_options.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_options_private.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_pack_dev.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_pack_dev.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_pathmatch.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_pathmatch.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_platform.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_platform_acl.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_platform_xattr.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_ppmd_private.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_ppmd8.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_ppmd8_private.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_ppmd7.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_ppmd7_private.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_private.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_random.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_random_private.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_rb.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_rb.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_add_passphrase.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_append_filter.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_data_into_fd.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_disk_entry_from_file.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_disk_posix.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_disk_private.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_disk_set_standard_lookup.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_extract.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_extract2.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_open_fd.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_open_file.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_open_filename.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_open_memory.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_private.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_set_format.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_set_options.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_filter_all.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_filter_by_code.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_filter_bzip2.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_filter_compress.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_filter_gzip.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_filter_grzip.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_filter_lrzip.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_filter_lz4.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_filter_lzop.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_filter_none.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_filter_program.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_filter_rpm.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_filter_uu.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_filter_xz.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_filter_zstd.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_format_7zip.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_format_all.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_format_ar.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_format_by_code.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_format_cab.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_format_cpio.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_format_empty.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_format_iso9660.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_format_lha.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_format_mtree.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_format_rar.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_format_rar5.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_format_raw.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_format_tar.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_format_warc.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_format_xar.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_read_support_format_zip.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_string.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_string.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_string_composition.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_string_sprintf.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_util.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_version_details.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_virtual.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_disk_posix.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_disk_private.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_disk_set_standard_lookup.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_private.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_open_fd.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_open_file.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_open_filename.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_open_memory.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_add_filter.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_add_filter_b64encode.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_add_filter_by_name.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_add_filter_bzip2.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_add_filter_compress.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_add_filter_grzip.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_add_filter_gzip.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_add_filter_lrzip.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_add_filter_lz4.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_add_filter_lzop.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_add_filter_none.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_add_filter_program.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_add_filter_uuencode.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_add_filter_xz.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_add_filter_zstd.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_set_format.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_set_format_7zip.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_set_format_ar.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_set_format_by_name.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_set_format_cpio.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_set_format_cpio_binary.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_set_format_cpio_newc.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_set_format_cpio_odc.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_set_format_filter_by_ext.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_set_format_gnutar.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_set_format_iso9660.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_set_format_mtree.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_set_format_pax.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_set_format_private.h"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_set_format_raw.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_set_format_shar.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_set_format_ustar.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_set_format_v7tar.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_set_format_warc.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_set_format_xar.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_set_format_zip.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_set_options.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_write_set_passphrase.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_xxhash.h"
        "${LIBARCHIVE_SOURCE_DIR}/filter_fork_posix.c"
        "${LIBARCHIVE_SOURCE_DIR}/filter_fork.h"
        "${LIBARCHIVE_SOURCE_DIR}/xxhash.c"
        # ARCHIVE_BLAKE2
        "${LIBARCHIVE_SOURCE_DIR}/archive_blake2sp_ref.c"
        "${LIBARCHIVE_SOURCE_DIR}/archive_blake2s_ref.c")
list(TRANSFORM LIBARCHIVE_SOURCES REPLACE  "${LIBARCHIVE_SOURCE_DIR}" "${LIBARCHIVE_PATCH_DIR}"
        OUTPUT_VARIABLE LIBARCHIVE_PATCH_SOURCES)
add_custom_command(OUTPUT
        ${LIBARCHIVE_PATCH_SOURCES}
        COMMAND "${CMAKE_COMMAND}" -E copy_directory "${LIBARCHIVE_SOURCE_DIR}"
        "${LIBARCHIVE_PATCH_DIR}"
        COMMAND "${Patch_EXECUTABLE}" -i "${CMAKE_CURRENT_SOURCE_DIR}/src/main/jni/libarchive.patch"
        DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/src/main/jni/libarchive.patch"
        ${LIBARCHIVE_SOURCES}
        WORKING_DIRECTORY "${LIBARCHIVE_PATCH_DIR}"
        COMMENT "Patching libarchive"
        VERBATIM)
add_library(archive STATIC
        ${LIBARCHIVE_PATCH_SOURCES})
target_compile_definitions(archive
        PRIVATE
        HAVE_CONFIG_H
        LIBARCHIVE_STATIC)
target_compile_options(archive
        PRIVATE
        -Wall
        -Wformat
        -Wformat-security
        -Werror
        -ffunction-sections
        -fdata-sections)
target_include_directories(archive
        PUBLIC
        "${LIBARCHIVE_PATCH_DIR}"
        PRIVATE
        src/main/jni/external/libarchive/contrib/android/include
        "${LIBARCHIVE_CONFIG_DIR}")
target_link_libraries(archive
        PRIVATE
        "${Z_LIBRARY}"
        bz
        lzma
        lz4
        zstd
        mbedcrypto)
target_link_options(archive
        PRIVATE
        LINKER:--gc-sections)

find_library(LOG_LIBRARY log)
add_library(archive-jni SHARED src/main/jni/archive-jni.c)
target_compile_options(archive-jni
        PRIVATE
        -Wall
        -Werror)
target_link_libraries(archive-jni archive "${LOG_LIBRARY}")

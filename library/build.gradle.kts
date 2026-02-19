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

plugins {
    id("com.android.library")
    id("com.vanniktech.maven.publish")
}

android {
    namespace = "me.zhanghai.android.libarchive"
    buildToolsVersion = "36.1.0"
    compileSdk = 36
    ndkVersion = "29.0.14206865"
    enableKotlin = false

    defaultConfig {
        minSdk = 21
        consumerProguardFiles("proguard-rules.pro")
        externalNativeBuild { cmake { arguments += "-DANDROID_STL=none" } }
    }

    buildTypes { release { isMinifyEnabled = false } }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }
    externalNativeBuild { cmake { path = file("CMakeLists.txt") } }
}

dependencies { implementation("androidx.annotation:annotation:1.9.1") }

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

plugins { id("com.android.application") }

android {
    namespace = "me.zhanghai.android.libarchive.sample"
    buildToolsVersion = "36.1.0"
    compileSdk = 36
    enableKotlin = false

    defaultConfig {
        applicationId = "me.zhanghai.android.libarchive.sample"
        minSdk = 21
        targetSdk = 36
        versionCode = providers.gradleProperty("VERSION_CODE").get().toInt()
        versionName = providers.gradleProperty("VERSION_NAME").get()
    }

    buildTypes {
        release {
            isMinifyEnabled = true
            isShrinkResources = true
            proguardFiles(
                getDefaultProguardFile("proguard-android-optimize.txt"),
                "proguard-rules.pro",
            )
        }
    }
    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_1_8
        targetCompatibility = JavaVersion.VERSION_1_8
    }
}

dependencies {
    implementation(project(":library"))
    implementation("androidx.annotation:annotation:1.9.1")
}

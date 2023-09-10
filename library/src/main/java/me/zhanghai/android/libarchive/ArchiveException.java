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

import java.io.IOException;

import androidx.annotation.Nullable;

public class ArchiveException extends IOException {
    private final int mCode;

    public ArchiveException(int code, @Nullable String message) {
        this(code, message, null);
    }

    public ArchiveException(int code, @Nullable String message,
            @Nullable Throwable cause) {
        super(message, cause);

        mCode = code;
    }

    public int getCode() {
        return mCode;
    }
}

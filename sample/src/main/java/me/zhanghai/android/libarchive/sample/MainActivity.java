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

package me.zhanghai.android.libarchive.sample;

import android.app.Activity;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;
import android.system.ErrnoException;
import android.system.Os;
import android.system.OsConstants;
import android.text.format.DateUtils;
import android.text.format.Formatter;
import android.view.Menu;
import android.view.MenuItem;
import android.view.ViewGroup;
import android.widget.HorizontalScrollView;
import android.widget.ScrollView;
import android.widget.TextView;

import java.io.FileDescriptor;
import java.io.IOException;
import java.io.InterruptedIOException;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.util.concurrent.TimeUnit;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import me.zhanghai.android.libarchive.Archive;
import me.zhanghai.android.libarchive.ArchiveEntry;
import me.zhanghai.android.libarchive.ArchiveException;

public class MainActivity extends Activity {

    private static final int MENU_ID_OPEN = 1;

    private static final int REQUEST_CODE_OPEN_DOCUMENT = 1;

    private static final boolean READ_WITH_CALLBACKS = true;

    private ScrollView mScrollView;
    private HorizontalScrollView mHorizontalScrollView;
    private TextView mTextView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        String versionDetails = newStringFromBytes(Archive.versionDetails());

        mTextView = new TextView(this);
        mTextView.setText(versionDetails);
        mHorizontalScrollView = new HorizontalScrollView(this);
        mHorizontalScrollView.setFillViewport(true);
        mHorizontalScrollView.setHorizontalScrollBarEnabled(true);
        mHorizontalScrollView.addView(mTextView, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        mScrollView = new ScrollView(this);
        mScrollView.setFillViewport(true);
        mHorizontalScrollView.setVerticalScrollBarEnabled(true);
        mScrollView.addView(mHorizontalScrollView, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));
        setContentView(mScrollView, new ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));
    }

    @Override
    public boolean onCreateOptionsMenu(@NonNull Menu menu) {
        super.onCreateOptionsMenu(menu);

        menu.add(Menu.NONE, MENU_ID_OPEN, 0, "Open")
                .setShowAsAction(MenuItem.SHOW_AS_ACTION_ALWAYS);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(@NonNull MenuItem item) {
        //noinspection SwitchStatementWithTooFewBranches
        switch (item.getItemId()) {
            case MENU_ID_OPEN:
                onOpenDocument();
                return true;
            default:
                return super.onOptionsItemSelected(item);
        }
    }

    private void onOpenDocument() {
        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT)
                .addCategory(Intent.CATEGORY_OPENABLE)
                .setType("*/*");
        startActivityForResult(intent, REQUEST_CODE_OPEN_DOCUMENT);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, @Nullable Intent data) {
        //noinspection SwitchStatementWithTooFewBranches
        switch (requestCode) {
            case REQUEST_CODE_OPEN_DOCUMENT:
                onOpenDocumentResult(resultCode, data);
                break;
            default:
                super.onActivityResult(requestCode, resultCode, data);
        }
    }

    private void onOpenDocumentResult(int resultCode, @Nullable Intent data) {
        if (resultCode != RESULT_OK || data == null) {
            return;
        }
        Uri uri = data.getData();
        if (uri == null) {
            return;
        }
        // HACK: I/O on main thread.
        String output = readArchive(uri);
        mScrollView.scrollTo(0, 0);
        mHorizontalScrollView.scrollTo(0, 0);
        mTextView.setText(output);
    }

    @NonNull
    private String readArchive(@NonNull Uri uri) {
        try {
            StringBuilder builder = new StringBuilder();
            String displayName = getUriDisplayName(uri);
            builder.append(displayName).append('\n');
            try (ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(uri, "r")) {
                if (pfd == null) {
                    throw new IOException("ContentResolver.openFileDescriptor() returned null");
                }
                long archive = Archive.readNew();
                try {
                    Archive.readSupportFilterAll(archive);
                    Archive.readSupportFormatAll(archive);
                    if (READ_WITH_CALLBACKS) {
                        Archive.readSetCallbackData(archive, pfd.getFileDescriptor());
                        ByteBuffer buffer = ByteBuffer.allocateDirect(8192);
                        Archive.readSetReadCallback(archive, (_1, fd) -> {
                            buffer.clear();
                            try {
                                Os.read((FileDescriptor) fd, buffer);
                            } catch (ErrnoException | InterruptedIOException e) {
                                throw new ArchiveException(Archive.ERRNO_FATAL, "Os.read", e);
                            }
                            buffer.flip();
                            return buffer;
                        });
                        Archive.readSetSkipCallback(archive, (_1, fd, request) -> {
                            try {
                                Os.lseek((FileDescriptor) fd, request, OsConstants.SEEK_CUR);
                            } catch (ErrnoException e) {
                                throw new ArchiveException(Archive.ERRNO_FATAL, "Os.lseek", e);
                            }
                            return request;
                        });
                        Archive.readSetSeekCallback(archive, (_1, fd, offset, whence) -> {
                            try {
                                return Os.lseek((FileDescriptor) fd, offset, whence);
                            } catch (ErrnoException e) {
                                throw new ArchiveException(Archive.ERRNO_FATAL, "Os.lseek", e);
                            }
                        });
                        Archive.readOpen1(archive);
                    } else {
                        Archive.readOpenFd(archive, pfd.getFd(), 8192);
                    }
                    long entry = Archive.readNextHeader(archive);
                    String formatName = newStringFromBytes(Archive.formatName(archive));
                    int filterCount = Archive.filterCount(archive);
                    String filterName = filterCount > 1 ? newStringFromBytes(Archive.filterName(
                            archive, 0)) : null;
                    builder.append(formatName).append('\t').append(filterName).append('\n');
                    while (entry != 0) {
                        String entryName = newStringFromBytes(ArchiveEntry.pathname(entry));
                        ArchiveEntry.StructStat stat = ArchiveEntry.stat(entry);
                        String entrySizeString = Formatter.formatShortFileSize(this, stat.stSize);
                        long entryModifiedTime = getMillisFromTimes(stat.stMtim.tvSec,
                                stat.stMtim.tvNsec);
                        String entryModifiedTimeString = formatDateTime(entryModifiedTime);
                        builder.append(entrySizeString).append('\t').append(entryModifiedTimeString)
                                .append('\t').append(entryName).append('\n');
                        entry = Archive.readNextHeader(archive);
                    }
                } finally {
                    Archive.free(archive);
                }
            }
            return builder.toString();
        } catch (IOException e) {
            e.printStackTrace();
            return e.toString();
        }
    }

    @NonNull
    private String getUriDisplayName(@NonNull Uri uri) {
        try (Cursor cursor = getContentResolver().query(uri,
                new String[] { OpenableColumns.DISPLAY_NAME }, null, null, null)) {
            if (cursor != null && cursor.moveToFirst()) {
                int displayNameIndex = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                if (displayNameIndex != -1) {
                    String displayName = cursor.getString(displayNameIndex);
                    if (displayName != null && !displayName.isEmpty()) {
                        return displayName;
                    }
                }
            }
        }
        String lastPathSegment = uri.getLastPathSegment();
        if (lastPathSegment != null) {
            return lastPathSegment;
        }
        return uri.toString();
    }

    @Nullable
    private String newStringFromBytes(@Nullable byte[] bytes) {
        return bytes != null ? new String(bytes, StandardCharsets.UTF_8) : null;
    }

    private long getMillisFromTimes(long seconds, long nanoseconds) {
        return TimeUnit.SECONDS.toMillis(seconds) + TimeUnit.NANOSECONDS.toMillis(nanoseconds);
    }

    @NonNull
    private String formatDateTime(long millis) {
        return DateUtils.formatDateTime(this, millis, DateUtils.FORMAT_SHOW_TIME
                | DateUtils.FORMAT_SHOW_DATE | DateUtils.FORMAT_NO_NOON
                | DateUtils.FORMAT_NO_MIDNIGHT | DateUtils.FORMAT_ABBREV_ALL);
    }
}

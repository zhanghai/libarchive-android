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

import java.nio.ByteBuffer;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/** @noinspection OctalInteger*/
public class ArchiveEntry {

    static {
        Archive.staticInit();
    }

    private ArchiveEntry() {}

    public static final int AE_IFMT = 0170000;
    public static final int AE_IFREG = 0100000;
    public static final int AE_IFLNK = 0120000;
    public static final int AE_IFSOCK = 0140000;
    public static final int AE_IFCHR = 0020000;
    public static final int AE_IFBLK = 0060000;
    public static final int AE_IFDIR = 0040000;
    public static final int AE_IFIFO = 0010000;

    public static final int AE_SYMLINK_TYPE_UNDEFINED = 0;
    public static final int AE_SYMLINK_TYPE_FILE = 1;
    public static final int AE_SYMLINK_TYPE_DIRECTORY = 2;

    public static final int DIGEST_MD5 = 0x00000001;
    public static final int DIGEST_RMD160 = 0x00000002;
    public static final int DIGEST_SHA1 = 0x00000003;
    public static final int DIGEST_SHA256 = 0x00000004;
    public static final int DIGEST_SHA384 = 0x00000005;
    public static final int DIGEST_SHA512 = 0x00000006;

    public static native void clear(long entry);
    public static native long clone(long entry);
    public static native void free(long entry);
    public static native long new1();
    public static native long new2(long archive);

    public static native long atime(long entry);
    public static native long atimeNsec(long entry);
    public static native boolean atimeIsSet(long entry);
    public static native long birthtime(long entry);
    public static native long birthtimeNsec(long entry);
    public static native boolean birthtimeIsSet(long entry);
    public static native long ctime(long entry);
    public static native long ctimeNsec(long entry);
    public static native boolean ctimeIsSet(long entry);
    public static native long dev(long entry);
    public static native boolean devIsSet(long entry);
    public static native long devmajor(long entry);
    public static native long devminor(long entry);
    public static native int filetype(long entry);
    public static native boolean filetypeIsSet(long entry);
    public static native long fflagsSet(long entry);
    public static native long fflagsClear(long entry);
    @Nullable
    public static native byte[] fflagsText(long entry);
    public static native long gid(long entry);
    public static native boolean gidIsSet(long entry);
    @Nullable
    public static native byte[] gname(long entry);
    @Nullable
    public static native String gnameUtf8(long entry);
    @Nullable
    public static native byte[] hardlink(long entry);
    @Nullable
    public static native String hardlinkUtf8(long entry);
    public static native boolean hardlinkIsSet(long entry);
    public static native long ino(long entry);
    public static native boolean inoIsSet(long entry);
    public static native int mode(long entry);
    public static native long mtime(long entry);
    public static native long mtimeNsec(long entry);
    public static native boolean mtimeIsSet(long entry);
    public static native long nlink(long entry);
    @Nullable
    public static native byte[] pathname(long entry);
    @Nullable
    public static native String pathnameUtf8(long entry);
    public static native int perm(long entry);
    public static native boolean permIsSet(long entry);
    public static native boolean rdevIsSet(long entry);
    public static native long rdev(long entry);
    public static native long rdevmajor(long entry);
    public static native long rdevminor(long entry);
    @Nullable
    public static native byte[] sourcepath(long entry);
    public static native long size(long entry);
    public static native boolean sizeIsSet(long entry);
    @Nullable
    public static native byte[] strmode(long entry);
    @Nullable
    public static native byte[] symlink(long entry);
    @Nullable
    public static native String symlinkUtf8(long entry);
    public static native int symlinkType(long entry);
    public static native long uid(long entry);
    public static native boolean uidIsSet(long entry);
    @Nullable
    public static native byte[] uname(long entry);
    @Nullable
    public static native String unameUtf8(long entry);
    public static native boolean isDataEncrypted(long entry);
    public static native boolean isMetadataEncrypted(long entry);
    public static native boolean isEncrypted(long entry);

    public static native void setAtime(long entry, long atime, long atimeNsec);
    public static native void unsetAtime(long entry);
    public static native void setBirthtime(long entry, long birthtime, long birthtimeNsec);
    public static native void unsetBirthtime(long entry);
    public static native void setCtime(long entry, long ctime, long ctimeNsec);
    public static native void unsetCtime(long entry);
    public static native void setDev(long entry, long dev);
    public static native void setDevmajor(long entry, long devmajor);
    public static native void setDevminor(long entry, long devminor);
    public static native void setFiletype(long entry, int filetype);
    public static native void setFflags(long entry, long set, long clear);
    public static native int setFflagsText(long entry, @Nullable byte[] fflags);
    public static native void setGid(long entry, long gid);
    public static native void setGname(long entry, @Nullable byte[] gname);
    public static native void setGnameUtf8(long entry, @Nullable String gname);
    public static native boolean updateGnameUtf8(long entry, @Nullable String gname);
    public static native void setHardlink(long entry, @Nullable byte[] hardlink);
    public static native void setHardlinkUtf8(long entry, @Nullable String hardlink);
    public static native boolean updateHardlinkUtf8(long entry, @Nullable String hardlink);
    public static native void setIno(long entry, long ino);
    public static native void setLink(long entry, @Nullable byte[] link);
    public static native void setLinkUtf8(long entry, @Nullable String link);
    public static native boolean updateLinkUtf8(long entry, @Nullable String link);
    public static native void setMode(long entry, int mode);
    public static native void setMtime(long entry, long mtime, long mtimeNsec);
    public static native void unsetMtime(long entry);
    public static native void setNlink(long entry, int nlink);
    public static native void setPathname(long entry, @Nullable byte[] pathname);
    public static native void setPathnameUtf8(long entry, @Nullable String pathname);
    public static native boolean updatePathnameUtf8(long entry, @Nullable String pathname);
    public static native void setPerm(long entry, int perm);
    public static native void setRdev(long entry, long rdev);
    public static native void setRdevmajor(long entry, long rdevmajor);
    public static native void setRdevminor(long entry, long rdevminor);
    public static native void setSize(long entry, long size);
    public static native void unsetSize(long entry);
    public static native void setSourcepath(long entry, @Nullable byte[] sourcepath);
    public static native void setSymlink(long entry, @Nullable byte[] symlink);
    public static native void setSymlinkType(long entry, int type);
    public static native void setSymlinkUtf8(long entry, @Nullable String symlink);
    public static native boolean updateSymlinkUtf8(long entry, @Nullable String symlink);
    public static native void setUid(long entry, long uid);
    public static native void setUname(long entry, @Nullable byte[] uname);
    public static native void setUnameUtf8(long entry, @Nullable String uname);
    public static native boolean updateUnameUtf8(long entry, @Nullable String uname);
    public static native void setDataEncrypted(long entry, boolean encrypted);
    public static native void setMetadataEncrypted(long entry, boolean encrypted);

    @NonNull
    public static native StructStat stat(long entry);
    public static native void setStat(long entry, @NonNull StructStat stat);

    @Nullable
    public static native ByteBuffer digest(long entry, int type);

    public static class StructTimespec {
        public long tvSec;
        public long tvNsec;
    }

    public static class StructStat {
        public long stDev;
        public int stMode;
        public int stNlink;
        public int stUid;
        public int stGid;
        public long stRdev;
        public long stSize;
        public long stBlksize;
        public long stBlocks;
        public StructTimespec stAtim;
        public StructTimespec stMtim;
        public StructTimespec stCtim;
        public long stIno;
    }
}

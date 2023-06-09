/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include <stdlib.h> /* malloc */
#include <string.h>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>

#include <zlib.h>
#include <zstd.h>

#ifdef WIN32
#  include "BLI_fileops_types.h"
#  include "BLI_winstuff.h"
#  include "utf_winfunc.h"
#  include "utfconv.h"
#  include <io.h>
#  include <shellapi.h>
#  include <shobjidl.h>
#  include <windows.h>
#else
#  if defined(__APPLE__)
#    include <CoreFoundation/CoreFoundation.h>
#    include <objc/message.h>
#    include <objc/runtime.h>
#  endif
#  include <dirent.h>
#  include <sys/param.h>
#  include <sys/wait.h>
#  include <unistd.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_utils.h"
#include "BLI_sys_types.h" /* for intptr_t support */
#include "BLI_utildefines.h"

/** Sizes above this must be allocated. */
#define FILE_MAX_STATIC_BUF 256

#ifdef WIN32
/* Text string used as the "verb" for Windows shell operations. */
static char *windows_operation_string(FileExternalOperation operation)
{
  switch (operation) {
    case FILE_EXTERNAL_OPERATION_OPEN:
      return "open";
    case FILE_EXTERNAL_OPERATION_FOLDER_OPEN:
      return "open";
    case FILE_EXTERNAL_OPERATION_EDIT:
      return "edit";
    case FILE_EXTERNAL_OPERATION_NEW:
      return "new";
    case FILE_EXTERNAL_OPERATION_FIND:
      return "find";
    case FILE_EXTERNAL_OPERATION_SHOW:
      return "show";
    case FILE_EXTERNAL_OPERATION_PLAY:
      return "play";
    case FILE_EXTERNAL_OPERATION_BROWSE:
      return "browse";
    case FILE_EXTERNAL_OPERATION_PREVIEW:
      return "preview";
    case FILE_EXTERNAL_OPERATION_PRINT:
      return "print";
    case FILE_EXTERNAL_OPERATION_INSTALL:
      return "install";
    case FILE_EXTERNAL_OPERATION_RUNAS:
      return "runas";
    case FILE_EXTERNAL_OPERATION_PROPERTIES:
      return "properties";
    case FILE_EXTERNAL_OPERATION_FOLDER_FIND:
      return "find";
    case FILE_EXTERNAL_OPERATION_FOLDER_CMD:
      return "cmd";
  }
  BLI_assert_unreachable();
  return "";
}
#endif

bool BLI_file_external_operation_supported(const char *filepath, FileExternalOperation operation)
{
#ifdef WIN32
  char *opstring = windows_operation_string(operation);
  return BLI_windows_external_operation_supported(filepath, opstring);
#else
  UNUSED_VARS(filepath, operation);
  return false;
#endif
}

bool BLI_file_external_operation_execute(const char *filepath, FileExternalOperation operation)
{
#ifdef WIN32
  char *opstring = windows_operation_string(operation);
  if (BLI_windows_external_operation_supported(filepath, opstring) &&
      BLI_windows_external_operation_execute(filepath, opstring))
  {
    return true;
  }
  return false;
#else
  UNUSED_VARS(filepath, operation);
  return false;
#endif
}

size_t BLI_file_zstd_from_mem_at_pos(
    void *buf, size_t len, FILE *file, size_t file_offset, int compression_level)
{
  fseek(file, file_offset, SEEK_SET);

  ZSTD_CCtx *ctx = ZSTD_createCCtx();
  ZSTD_CCtx_setParameter(ctx, ZSTD_c_compressionLevel, compression_level);

  ZSTD_inBuffer input = {buf, len, 0};

  size_t out_len = ZSTD_CStreamOutSize();
  void *out_buf = MEM_mallocN(out_len, __func__);
  size_t total_written = 0;

  /* Compress block and write it out until the input has been consumed. */
  while (input.pos < input.size) {
    ZSTD_outBuffer output = {out_buf, out_len, 0};
    size_t ret = ZSTD_compressStream2(ctx, &output, &input, ZSTD_e_continue);
    if (ZSTD_isError(ret)) {
      break;
    }
    if (fwrite(out_buf, 1, output.pos, file) != output.pos) {
      break;
    }
    total_written += output.pos;
  }

  /* Finalize the `Zstd` frame. */
  size_t ret = 1;
  while (ret != 0) {
    ZSTD_outBuffer output = {out_buf, out_len, 0};
    ret = ZSTD_compressStream2(ctx, &output, &input, ZSTD_e_end);
    if (ZSTD_isError(ret)) {
      break;
    }
    if (fwrite(out_buf, 1, output.pos, file) != output.pos) {
      break;
    }
    total_written += output.pos;
  }

  MEM_freeN(out_buf);
  ZSTD_freeCCtx(ctx);

  return ZSTD_isError(ret) ? 0 : total_written;
}

size_t BLI_file_unzstd_to_mem_at_pos(void *buf, size_t len, FILE *file, size_t file_offset)
{
  fseek(file, file_offset, SEEK_SET);

  ZSTD_DCtx *ctx = ZSTD_createDCtx();

  size_t in_len = ZSTD_DStreamInSize();
  void *in_buf = MEM_mallocN(in_len, __func__);
  ZSTD_inBuffer input = {in_buf, in_len, 0};

  ZSTD_outBuffer output = {buf, len, 0};

  size_t ret = 0;
  /* Read and decompress chunks of input data until we have enough output. */
  while (output.pos < output.size && !ZSTD_isError(ret)) {
    input.size = fread(in_buf, 1, in_len, file);
    if (input.size == 0) {
      break;
    }

    /* Consume input data until we run out or have enough output. */
    input.pos = 0;
    while (input.pos < input.size && output.pos < output.size) {
      ret = ZSTD_decompressStream(ctx, &output, &input);

      if (ZSTD_isError(ret)) {
        break;
      }
    }
  }

  MEM_freeN(in_buf);
  ZSTD_freeDCtx(ctx);

  return ZSTD_isError(ret) ? 0 : output.pos;
}

bool BLI_file_magic_is_gzip(const char header[4])
{
  /* GZIP itself starts with the magic bytes 0x1f 0x8b.
   * The third byte indicates the compression method, which is 0x08 for DEFLATE. */
  return header[0] == 0x1f && header[1] == 0x8b && header[2] == 0x08;
}

bool BLI_file_magic_is_zstd(const char header[4])
{
  /* ZSTD files consist of concatenated frames, each either a Zstd frame or a skippable frame.
   * Both types of frames start with a magic number: 0xFD2FB528 for Zstd frames and 0x184D2A5*
   * for skippable frames, with the * being anything from 0 to F.
   *
   * To check whether a file is Zstd-compressed, we just check whether the first frame matches
   * either. Seeking through the file until a Zstd frame is found would make things more
   * complicated and the probability of a false positive is rather low anyways.
   *
   * Note that LZ4 uses a compatible format, so even though its compressed frames have a
   * different magic number, a valid LZ4 file might also start with a skippable frame matching
   * the second check here.
   *
   * For more details, see https://github.com/facebook/zstd/blob/dev/doc/zstd_compression_format.md
   */

  uint32_t magic = *((uint32_t *)header);
  if (magic == 0xFD2FB528) {
    return true;
  }
  if ((magic >> 4) == 0x184D2A5) {
    return true;
  }
  return false;
}

bool BLI_file_is_writable(const char *filepath)
{
  bool writable;
  if (BLI_access(filepath, W_OK) == 0) {
    /* file exists and I can write to it */
    writable = true;
  }
  else if (errno != ENOENT) {
    /* most likely file or containing directory cannot be accessed */
    writable = false;
  }
  else {
    /* file doesn't exist -- check I can create it in parent directory */
    char parent[FILE_MAX];
    BLI_path_split_dir_part(filepath, parent, sizeof(parent));
#ifdef WIN32
    /* windows does not have X_OK */
    writable = BLI_access(parent, W_OK) == 0;
#else
    writable = BLI_access(parent, X_OK | W_OK) == 0;
#endif
  }
  return writable;
}

bool BLI_file_touch(const char *filepath)
{
  FILE *f = BLI_fopen(filepath, "r+b");

  if (f != NULL) {
    int c = getc(f);

    if (c == EOF) {
      /* Empty file, reopen in truncate write mode... */
      fclose(f);
      f = BLI_fopen(filepath, "w+b");
    }
    else {
      /* Otherwise, rewrite first byte. */
      rewind(f);
      putc(c, f);
    }
  }
  else {
    f = BLI_fopen(filepath, "wb");
  }
  if (f) {
    fclose(f);
    return true;
  }
  return false;
}

static bool dir_create_recursive(char *dirname, int len)
{
  BLI_assert(strlen(dirname) == len);
  BLI_assert(BLI_exists(dirname) == 0);
  /* Caller must ensure the path doesn't have trailing slashes. */
  BLI_assert_msg(len && !BLI_path_slash_is_native_compat(dirname[len - 1]),
                 "Paths must not end with a slash!");
  BLI_assert_msg(!((len >= 3) && BLI_path_slash_is_native_compat(dirname[len - 3]) &&
                   STREQ(dirname + (len - 2), "..")),
                 "Paths containing \"..\" components must be normalized first!");

  bool ret = true;
  char *dirname_parent_end = (char *)BLI_path_parent_dir_end(dirname, len);
  if (dirname_parent_end) {
    const char dirname_parent_end_value = *dirname_parent_end;
    *dirname_parent_end = '\0';
#ifdef WIN32
    /* Check special case `c:\foo`, don't try create `c:`, harmless but unnecessary. */
    if (dirname[0] && !BLI_path_is_win32_drive_only(dirname))
#endif
    {
      const int mode = BLI_exists(dirname);
      if (mode != 0) {
        if (!S_ISDIR(mode)) {
          ret = false;
        }
      }
      else if (!dir_create_recursive(dirname, dirname_parent_end - dirname)) {
        ret = false;
      }
    }
    *dirname_parent_end = dirname_parent_end_value;
  }
  if (ret) {
#ifdef WIN32
    if (umkdir(dirname) == -1) {
      ret = false;
    }
#else
    if (mkdir(dirname, 0777) != 0) {
      ret = false;
    }
#endif
  }
  return ret;
}

bool BLI_dir_create_recursive(const char *dirname)
{
  const int mode = BLI_exists(dirname);
  if (mode != 0) {
    /* The file exists, either it's a directory (ok), or not,
     * in which case this function can't do anything useful
     * (the caller could remove it and re-run this function). */
    return S_ISDIR(mode) ? true : false;
  }

  char dirname_static_buf[FILE_MAX];
  char *dirname_mut = dirname_static_buf;

  size_t len = strlen(dirname);
  if (len >= sizeof(dirname_static_buf)) {
    dirname_mut = MEM_mallocN(len + 1, __func__);
  }
  memcpy(dirname_mut, dirname, len + 1);

  /* Strip trailing chars, important for first entering #dir_create_recursive
   * when then ensures this is the case for recursive calls. */
  while ((len > 0) && BLI_path_slash_is_native_compat(dirname_mut[len - 1])) {
    len--;
  }
  dirname_mut[len] = '\0';

  const bool ret = (len > 0) && dir_create_recursive(dirname_mut, len);

  /* Ensure the string was properly restored. */
  BLI_assert(memcmp(dirname, dirname_mut, len) == 0);

  if (dirname_mut != dirname_static_buf) {
    MEM_freeN(dirname_mut);
  }

  return ret;
}

bool BLI_file_ensure_parent_dir_exists(const char *filepath)
{
  char di[FILE_MAX];
  BLI_path_split_dir_part(filepath, di, sizeof(di));

  /* Make if the dir doesn't exist. */
  return BLI_dir_create_recursive(di);
}

int BLI_rename(const char *from, const char *to)
{
#ifdef WIN32
  return urename(from, to);
#else
  return rename(from, to);
#endif
}

int BLI_rename_overwrite(const char *from, const char *to)
{
  if (!BLI_exists(from)) {
    return 1;
  }

  /* NOTE(@ideasman42): there are no checks that `from` & `to` *aren't* the same file.
   * It's up to the caller to ensure this. In practice these paths are often generated
   * and known to be different rather than arbitrary user input.
   * In the case of arbitrary paths (renaming a file in the file-selector for example),
   * the caller must ensure file renaming doesn't cause user data loss.
   *
   * Support for checking the files aren't the same could be added, however path comparison
   * alone is *not* a guarantee the files are different (given the possibility of accessing
   * the same file through different paths via symbolic-links), we could instead support a
   * version of Python's `os.path.samefile(..)` which compares the I-node & device.
   * In this particular case we would not want to follow symbolic-links as well.
   * Since this functionality isn't required at the moment, leave this as-is.
   * Noting it as a potential improvement. */
  if (BLI_exists(to)) {
    if (BLI_delete(to, false, false)) {
      return 1;
    }
  }

  return BLI_rename(from, to);
}

#ifdef WIN32

static void callLocalErrorCallBack(const char *err)
{
  printf("%s\n", err);
}

FILE *BLI_fopen(const char *filepath, const char *mode)
{
  BLI_assert(!BLI_path_is_rel(filepath));

  return ufopen(filepath, mode);
}

void BLI_get_short_name(char short_name[256], const char *filepath)
{
  wchar_t short_name_16[256];
  int i = 0;

  UTF16_ENCODE(filepath);

  GetShortPathNameW(filepath_16, short_name_16, 256);

  for (i = 0; i < 256; i++) {
    short_name[i] = (char)short_name_16[i];
  }

  UTF16_UN_ENCODE(filepath);
}

void *BLI_gzopen(const char *filepath, const char *mode)
{
  gzFile gzfile;

  BLI_assert(!BLI_path_is_rel(filepath));

  /* XXX: Creates file before transcribing the path. */
  if (mode[0] == 'w') {
    FILE *file = ufopen(filepath, "a");
    if (file == NULL) {
      /* File couldn't be opened, e.g. due to permission error. */
      return NULL;
    }
    fclose(file);
  }

  /* temporary #if until we update all libraries to 1.2.7
   * for correct wide char path handling */
#  if ZLIB_VERNUM >= 0x1270
  UTF16_ENCODE(filepath);

  gzfile = gzopen_w(filepath_16, mode);

  UTF16_UN_ENCODE(filepath);
#  else
  {
    char short_name[256];
    BLI_get_short_name(short_name, filepath);
    gzfile = gzopen(short_name, mode);
  }
#  endif

  return gzfile;
}

int BLI_open(const char *filepath, int oflag, int pmode)
{
  BLI_assert(!BLI_path_is_rel(filepath));

  return uopen(filepath, oflag, pmode);
}

int BLI_access(const char *filepath, int mode)
{
  BLI_assert(!BLI_path_is_rel(filepath));

  return uaccess(filepath, mode);
}

static bool delete_soft(const wchar_t *path_16, const char **error_message)
{
  /* Deletes file or directory to recycling bin. The latter moves all contained files and
   * directories recursively to the recycling bin as well. */
  IFileOperation *pfo;
  IShellItem *psi;

  HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

  if (SUCCEEDED(hr)) {
    /* This is also the case when COM was previously initialized and CoInitializeEx returns
     * S_FALSE, which is not an error. Both HRESULT values S_OK and S_FALSE indicate success. */

    hr = CoCreateInstance(
        &CLSID_FileOperation, NULL, CLSCTX_ALL, &IID_IFileOperation, (void **)&pfo);

    if (SUCCEEDED(hr)) {
      /* Flags for deletion:
       * FOF_ALLOWUNDO: Enables moving file to recycling bin.
       * FOF_SILENT: Don't show progress dialog box.
       * FOF_WANTNUKEWARNING: Show dialog box if file can't be moved to recycling bin. */
      hr = pfo->lpVtbl->SetOperationFlags(pfo, FOF_ALLOWUNDO | FOF_SILENT | FOF_WANTNUKEWARNING);

      if (SUCCEEDED(hr)) {
        hr = SHCreateItemFromParsingName(path_16, NULL, &IID_IShellItem, (void **)&psi);

        if (SUCCEEDED(hr)) {
          hr = pfo->lpVtbl->DeleteItem(pfo, psi, NULL);

          if (SUCCEEDED(hr)) {
            hr = pfo->lpVtbl->PerformOperations(pfo);

            if (FAILED(hr)) {
              *error_message = "Failed to prepare delete operation";
            }
          }
          else {
            *error_message = "Failed to prepare delete operation";
          }
          psi->lpVtbl->Release(psi);
        }
        else {
          *error_message = "Failed to parse path";
        }
      }
      else {
        *error_message = "Failed to set operation flags";
      }
      pfo->lpVtbl->Release(pfo);
    }
    else {
      *error_message = "Failed to create FileOperation instance";
    }
    CoUninitialize();
  }
  else {
    *error_message = "Failed to initialize COM";
  }

  return FAILED(hr);
}

static bool delete_unique(const char *path, const bool dir)
{
  bool err;

  UTF16_ENCODE(path);

  if (dir) {
    err = !RemoveDirectoryW(path_16);
    if (err) {
      printf("Unable to remove directory\n");
    }
  }
  else {
    err = !DeleteFileW(path_16);
    if (err) {
      callLocalErrorCallBack("Unable to delete file");
    }
  }

  UTF16_UN_ENCODE(path);

  return err;
}

static bool delete_recursive(const char *dir)
{
  struct direntry *filelist, *fl;
  bool err = false;
  uint filelist_num, i;

  i = filelist_num = BLI_filelist_dir_contents(dir, &filelist);
  fl = filelist;
  while (i--) {
    if (FILENAME_IS_CURRPAR(fl->relname)) {
      /* Skip! */
    }
    else if (S_ISDIR(fl->type)) {
      char path[FILE_MAXDIR];

      /* dir listing produces dir path without trailing slash... */
      STRNCPY(path, fl->path);
      BLI_path_slash_ensure(path, sizeof(path));

      if (delete_recursive(path)) {
        err = true;
      }
    }
    else {
      if (delete_unique(fl->path, false)) {
        err = true;
      }
    }
    fl++;
  }

  if (!err && delete_unique(dir, true)) {
    err = true;
  }

  BLI_filelist_free(filelist, filelist_num);

  return err;
}

int BLI_delete(const char *path, bool dir, bool recursive)
{
  int err;

  BLI_assert(!BLI_path_is_rel(path));

  if (recursive) {
    err = delete_recursive(path);
  }
  else {
    err = delete_unique(path, dir);
  }

  return err;
}

/**
 * Moves the files or directories to the recycling bin.
 */
int BLI_delete_soft(const char *file, const char **error_message)
{
  int err;

  BLI_assert(!BLI_path_is_rel(file));

  UTF16_ENCODE(file);

  err = delete_soft(file_16, error_message);

  UTF16_UN_ENCODE(file);

  return err;
}

/**
 * MS-Windows doesn't support moving to a directory, it has to be
 * `mv filepath filepath` and not `mv filepath destination_directory` (same for copying).
 *
 * So when `path_dst` ends with as slash:
 * ensure the filename component of `path_src` is added to a copy of `path_dst`.
 */
static const char *path_destination_ensure_filename(const char *path_src,
                                                    const char *path_dst,
                                                    char *buf,
                                                    size_t buf_size)
{
  const char *filename_src = BLI_path_basename(path_src);
  /* Unlikely but possible this has no slashes. */
  if (filename_src != path_src) {
    const size_t path_dst_len = strlen(path_dst);
    /* Check if `path_dst` points to a directory. */
    if (path_dst_len && BLI_path_slash_is_native_compat(path_dst[path_dst_len - 1])) {
      size_t buf_size_needed = path_dst_len + strlen(filename_src) + 1;
      char *path_dst_with_filename = (buf_size_needed <= buf_size) ?
                                         buf :
                                         MEM_mallocN(buf_size_needed, __func__);
      BLI_string_join(path_dst_with_filename, buf_size_needed, path_dst, filename_src);
      return path_dst_with_filename;
    }
  }
  return path_dst;
}

int BLI_path_move(const char *path_src, const char *path_dst)
{
  char path_dst_buf[FILE_MAX_STATIC_BUF];
  const char *path_dst_with_filename = path_destination_ensure_filename(
      path_src, path_dst, path_dst_buf, sizeof(path_dst_buf));

  int err;

  UTF16_ENCODE(path_src);
  UTF16_ENCODE(path_dst_with_filename);
  err = !MoveFileW(path_src_16, path_dst_with_filename_16);
  UTF16_UN_ENCODE(path_dst_with_filename);
  UTF16_UN_ENCODE(path_src);

  if (err) {
    callLocalErrorCallBack("Unable to move file");
    printf(" Move from '%s' to '%s' failed\n", path_src, path_dst_with_filename);
  }

  if (!ELEM(path_dst_with_filename, path_dst_buf, path_dst)) {
    MEM_freeN((void *)path_dst_with_filename);
  }

  return err;
}

int BLI_copy(const char *path_src, const char *path_dst)
{
  char path_dst_buf[FILE_MAX_STATIC_BUF];
  const char *path_dst_with_filename = path_destination_ensure_filename(
      path_src, path_dst, path_dst_buf, sizeof(path_dst_buf));
  int err;

  UTF16_ENCODE(path_src);
  UTF16_ENCODE(path_dst_with_filename);
  err = !CopyFileW(path_src_16, path_dst_with_filename_16, false);
  UTF16_UN_ENCODE(path_dst_with_filename);
  UTF16_UN_ENCODE(path_src);

  if (err) {
    callLocalErrorCallBack("Unable to copy file!");
    printf(" Copy from '%s' to '%s' failed\n", path_src, path_dst_with_filename);
  }

  if (!ELEM(path_dst_with_filename, path_dst_buf, path_dst)) {
    MEM_freeN((void *)path_dst_with_filename);
  }

  return err;
}

#  if 0
int BLI_create_symlink(const char *path_src, const char *path_dst)
{
  /* See patch from #30870, should this ever become needed. */
  callLocalErrorCallBack("Linking files is unsupported on Windows");
  (void)path_src;
  (void)path_dst;
  return 1;
}
#  endif

#else /* The UNIX world */

/* results from recursive_operation and its callbacks */
enum {
  /* operation succeeded */
  RecursiveOp_Callback_OK = 0,

  /* operation requested not to perform recursive digging for current path */
  RecursiveOp_Callback_StopRecurs = 1,

  /* error occurred in callback and recursive walking should stop immediately */
  RecursiveOp_Callback_Error = 2,
};

typedef int (*RecursiveOp_Callback)(const char *from, const char *to);

/* appending of filename to dir (ensures for buffer size before appending) */
static void join_dirfile_alloc(char **dst, size_t *alloc_len, const char *dir, const char *file)
{
  size_t len = strlen(dir) + strlen(file) + 1;

  if (*dst == NULL) {
    *dst = MEM_mallocN(len + 1, "join_dirfile_alloc path");
  }
  else if (*alloc_len < len) {
    *dst = MEM_reallocN(*dst, len + 1);
  }

  *alloc_len = len;

  BLI_path_join(*dst, len + 1, dir, file);
}

static char *strip_last_slash(const char *dirpath)
{
  char *result = BLI_strdup(dirpath);
  BLI_path_slash_rstrip(result);

  return result;
}

/**
 * Scans \a startfrom, generating a corresponding destination name for each item found by
 * prefixing it with startto, recursively scanning subdirectories, and invoking the specified
 * callbacks for files and subdirectories found as appropriate.
 *
 * \param startfrom: Top-level source path.
 * \param startto: Top-level destination path.
 * \param callback_dir_pre: Optional, to be invoked before entering a subdirectory, can return
 *                          RecursiveOp_Callback_StopRecurs to skip the subdirectory.
 * \param callback_file: Optional, to be invoked on each file found.
 * \param callback_dir_post: optional, to be invoked after leaving a subdirectory.
 * \return
 */
static int recursive_operation(const char *startfrom,
                               const char *startto,
                               RecursiveOp_Callback callback_dir_pre,
                               RecursiveOp_Callback callback_file,
                               RecursiveOp_Callback callback_dir_post)
{
  struct stat st;
  char *from = NULL, *to = NULL;
  char *from_path = NULL, *to_path = NULL;
  struct dirent **dirlist = NULL;
  size_t from_alloc_len = -1, to_alloc_len = -1;
  int i, n = 0, ret = 0;

  do { /* once */
    /* ensure there's no trailing slash in file path */
    from = strip_last_slash(startfrom);
    if (startto) {
      to = strip_last_slash(startto);
    }

    ret = lstat(from, &st);
    if (ret < 0) {
      /* source wasn't found, nothing to operate with */
      break;
    }

    if (!S_ISDIR(st.st_mode)) {
      /* source isn't a directory, can't do recursive walking for it,
       * so just call file callback and leave */
      if (callback_file != NULL) {
        ret = callback_file(from, to);
        if (ret != RecursiveOp_Callback_OK) {
          ret = -1;
        }
      }
      break;
    }

    n = scandir(startfrom, &dirlist, NULL, alphasort);
    if (n < 0) {
      /* error opening directory for listing */
      perror("scandir");
      ret = -1;
      break;
    }

    if (callback_dir_pre != NULL) {
      ret = callback_dir_pre(from, to);
      if (ret != RecursiveOp_Callback_OK) {
        if (ret == RecursiveOp_Callback_StopRecurs) {
          /* callback requested not to perform recursive walking, not an error */
          ret = 0;
        }
        else {
          ret = -1;
        }
        break;
      }
    }

    for (i = 0; i < n; i++) {
      const struct dirent *const dirent = dirlist[i];

      if (FILENAME_IS_CURRPAR(dirent->d_name)) {
        continue;
      }

      join_dirfile_alloc(&from_path, &from_alloc_len, from, dirent->d_name);
      if (to) {
        join_dirfile_alloc(&to_path, &to_alloc_len, to, dirent->d_name);
      }

      bool is_dir;

#  ifdef __HAIKU__
      {
        struct stat st_dir;
        char filepath[FILE_MAX];
        BLI_path_join(filepath, sizeof(filepath), startfrom, dirent->d_name, NULL);
        lstat(filepath, &st_dir);
        is_dir = S_ISDIR(st_dir.st_mode);
      }
#  else
      is_dir = (dirent->d_type == DT_DIR);
#  endif

      if (is_dir) {
        /* Recurse into sub-directories. */
        ret = recursive_operation(
            from_path, to_path, callback_dir_pre, callback_file, callback_dir_post);
      }
      else if (callback_file != NULL) {
        ret = callback_file(from_path, to_path);
        if (ret != RecursiveOp_Callback_OK) {
          ret = -1;
        }
      }

      if (ret != 0) {
        break;
      }
    }
    if (ret != 0) {
      break;
    }

    if (callback_dir_post != NULL) {
      ret = callback_dir_post(from, to);
      if (ret != RecursiveOp_Callback_OK) {
        ret = -1;
      }
    }
  } while (false);

  if (dirlist != NULL) {
    for (i = 0; i < n; i++) {
      free(dirlist[i]);
    }
    free(dirlist);
  }
  if (from_path != NULL) {
    MEM_freeN(from_path);
  }
  if (to_path != NULL) {
    MEM_freeN(to_path);
  }
  if (from != NULL) {
    MEM_freeN(from);
  }
  if (to != NULL) {
    MEM_freeN(to);
  }

  return ret;
}

static int delete_callback_post(const char *from, const char *UNUSED(to))
{
  if (rmdir(from)) {
    perror("rmdir");

    return RecursiveOp_Callback_Error;
  }

  return RecursiveOp_Callback_OK;
}

static int delete_single_file(const char *from, const char *UNUSED(to))
{
  if (unlink(from)) {
    perror("unlink");

    return RecursiveOp_Callback_Error;
  }

  return RecursiveOp_Callback_OK;
}

#  ifdef __APPLE__
static int delete_soft(const char *file, const char **error_message)
{
  int ret = -1;

  Class NSAutoreleasePoolClass = objc_getClass("NSAutoreleasePool");
  SEL allocSel = sel_registerName("alloc");
  SEL initSel = sel_registerName("init");
  id poolAlloc = ((id(*)(Class, SEL))objc_msgSend)(NSAutoreleasePoolClass, allocSel);
  id pool = ((id(*)(id, SEL))objc_msgSend)(poolAlloc, initSel);

  Class NSStringClass = objc_getClass("NSString");
  SEL stringWithUTF8StringSel = sel_registerName("stringWithUTF8String:");
  id pathString = ((
      id(*)(Class, SEL, const char *))objc_msgSend)(NSStringClass, stringWithUTF8StringSel, file);

  Class NSFileManagerClass = objc_getClass("NSFileManager");
  SEL defaultManagerSel = sel_registerName("defaultManager");
  id fileManager = ((id(*)(Class, SEL))objc_msgSend)(NSFileManagerClass, defaultManagerSel);

  Class NSURLClass = objc_getClass("NSURL");
  SEL fileURLWithPathSel = sel_registerName("fileURLWithPath:");
  id nsurl = ((id(*)(Class, SEL, id))objc_msgSend)(NSURLClass, fileURLWithPathSel, pathString);

  SEL trashItemAtURLSel = sel_registerName("trashItemAtURL:resultingItemURL:error:");
  BOOL deleteSuccessful = ((
      BOOL(*)(id, SEL, id, id, id))objc_msgSend)(fileManager, trashItemAtURLSel, nsurl, nil, nil);

  if (deleteSuccessful) {
    ret = 0;
  }
  else {
    *error_message = "The Cocoa API call to delete file or directory failed";
  }

  SEL drainSel = sel_registerName("drain");
  ((void (*)(id, SEL))objc_msgSend)(pool, drainSel);

  return ret;
}
#  else
static int delete_soft(const char *file, const char **error_message)
{
  const char *args[5];
  const char *process_failed;

  char *xdg_current_desktop = getenv("XDG_CURRENT_DESKTOP");
  char *xdg_session_desktop = getenv("XDG_SESSION_DESKTOP");

  if ((xdg_current_desktop != NULL && STREQ(xdg_current_desktop, "KDE")) ||
      (xdg_session_desktop != NULL && STREQ(xdg_session_desktop, "KDE")))
  {
    args[0] = "kioclient5";
    args[1] = "move";
    args[2] = file;
    args[3] = "trash:/";
    args[4] = NULL;
    process_failed = "kioclient5 reported failure";
  }
  else {
    args[0] = "gio";
    args[1] = "trash";
    args[2] = file;
    args[3] = NULL;
    process_failed = "gio reported failure";
  }

  int pid = fork();

  if (pid != 0) {
    /* Parent process */
    int wstatus = 0;

    waitpid(pid, &wstatus, 0);

    if (!WIFEXITED(wstatus)) {
      *error_message =
          "Blender may not support moving files or directories to trash on your system.";
      return -1;
    }
    if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus)) {
      *error_message = process_failed;
      return -1;
    }

    return 0;
  }

  execvp(args[0], (char **)args);

  *error_message = "Forking process failed.";
  return -1; /* This should only be reached if execvp fails and stack isn't replaced. */
}
#  endif

FILE *BLI_fopen(const char *filepath, const char *mode)
{
  BLI_assert(!BLI_path_is_rel(filepath));

  return fopen(filepath, mode);
}

void *BLI_gzopen(const char *filepath, const char *mode)
{
  BLI_assert(!BLI_path_is_rel(filepath));

  return gzopen(filepath, mode);
}

int BLI_open(const char *filepath, int oflag, int pmode)
{
  BLI_assert(!BLI_path_is_rel(filepath));

  return open(filepath, oflag, pmode);
}

int BLI_access(const char *filepath, int mode)
{
  BLI_assert(!BLI_path_is_rel(filepath));

  return access(filepath, mode);
}

int BLI_delete(const char *path, bool dir, bool recursive)
{
  BLI_assert(!BLI_path_is_rel(path));

  if (recursive) {
    return recursive_operation(path, NULL, NULL, delete_single_file, delete_callback_post);
  }
  if (dir) {
    return rmdir(path);
  }
  return remove(path);
}

int BLI_delete_soft(const char *file, const char **error_message)
{
  BLI_assert(!BLI_path_is_rel(file));

  return delete_soft(file, error_message);
}

/**
 * Do the two paths denote the same file-system object?
 */
static bool check_the_same(const char *path_a, const char *path_b)
{
  struct stat st_a, st_b;

  if (lstat(path_a, &st_a)) {
    return false;
  }

  if (lstat(path_b, &st_b)) {
    return false;
  }

  return st_a.st_dev == st_b.st_dev && st_a.st_ino == st_b.st_ino;
}

/**
 * Sets the mode and ownership of file to the values from st.
 */
static int set_permissions(const char *file, const struct stat *st)
{
  if (chown(file, st->st_uid, st->st_gid)) {
    perror("chown");
    return -1;
  }

  if (chmod(file, st->st_mode)) {
    perror("chmod");
    return -1;
  }

  return 0;
}

/* pre-recursive callback for copying operation
 * creates a destination directory where all source content fill be copied to */
static int copy_callback_pre(const char *from, const char *to)
{
  struct stat st;

  if (check_the_same(from, to)) {
    fprintf(stderr, "%s: '%s' is the same as '%s'\n", __func__, from, to);
    return RecursiveOp_Callback_Error;
  }

  if (lstat(from, &st)) {
    perror("stat");
    return RecursiveOp_Callback_Error;
  }

  /* create a directory */
  if (mkdir(to, st.st_mode)) {
    perror("mkdir");
    return RecursiveOp_Callback_Error;
  }

  /* set proper owner and group on new directory */
  if (chown(to, st.st_uid, st.st_gid)) {
    perror("chown");
    return RecursiveOp_Callback_Error;
  }

  return RecursiveOp_Callback_OK;
}

static int copy_single_file(const char *from, const char *to)
{
  FILE *from_stream, *to_stream;
  struct stat st;
  char buf[4096];
  size_t len;

  if (check_the_same(from, to)) {
    fprintf(stderr, "%s: '%s' is the same as '%s'\n", __func__, from, to);
    return RecursiveOp_Callback_Error;
  }

  if (lstat(from, &st)) {
    perror("lstat");
    return RecursiveOp_Callback_Error;
  }

  if (S_ISLNK(st.st_mode)) {
    /* symbolic links should be copied in special way */
    char *link_buffer;
    int need_free;
    ssize_t link_len;

    /* get large enough buffer to read link content */
    if ((st.st_size + 1) < sizeof(buf)) {
      link_buffer = buf;
      need_free = 0;
    }
    else {
      link_buffer = MEM_callocN(st.st_size + 2, "copy_single_file link_buffer");
      need_free = 1;
    }

    link_len = readlink(from, link_buffer, st.st_size + 1);
    if (link_len < 0) {
      perror("readlink");

      if (need_free) {
        MEM_freeN(link_buffer);
      }

      return RecursiveOp_Callback_Error;
    }

    link_buffer[link_len] = '\0';

    if (symlink(link_buffer, to)) {
      perror("symlink");
      if (need_free) {
        MEM_freeN(link_buffer);
      }
      return RecursiveOp_Callback_Error;
    }

    if (need_free) {
      MEM_freeN(link_buffer);
    }

    return RecursiveOp_Callback_OK;
  }
  if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode) || S_ISFIFO(st.st_mode) || S_ISSOCK(st.st_mode)) {
    /* copy special type of file */
    if (mknod(to, st.st_mode, st.st_rdev)) {
      perror("mknod");
      return RecursiveOp_Callback_Error;
    }

    if (set_permissions(to, &st)) {
      return RecursiveOp_Callback_Error;
    }

    return RecursiveOp_Callback_OK;
  }
  if (!S_ISREG(st.st_mode)) {
    fprintf(stderr, "Copying of this kind of files isn't supported yet\n");
    return RecursiveOp_Callback_Error;
  }

  from_stream = fopen(from, "rb");
  if (!from_stream) {
    perror("fopen");
    return RecursiveOp_Callback_Error;
  }

  to_stream = fopen(to, "wb");
  if (!to_stream) {
    perror("fopen");
    fclose(from_stream);
    return RecursiveOp_Callback_Error;
  }

  while ((len = fread(buf, 1, sizeof(buf), from_stream)) > 0) {
    fwrite(buf, 1, len, to_stream);
  }

  fclose(to_stream);
  fclose(from_stream);

  if (set_permissions(to, &st)) {
    return RecursiveOp_Callback_Error;
  }

  return RecursiveOp_Callback_OK;
}

static int move_callback_pre(const char *from, const char *to)
{
  int ret = rename(from, to);

  if (ret) {
    return copy_callback_pre(from, to);
  }

  return RecursiveOp_Callback_StopRecurs;
}

static int move_single_file(const char *from, const char *to)
{
  int ret = rename(from, to);

  if (ret) {
    return copy_single_file(from, to);
  }

  return RecursiveOp_Callback_OK;
}

int BLI_path_move(const char *path_src, const char *path_dst)
{
  int ret = recursive_operation(path_src, path_dst, move_callback_pre, move_single_file, NULL);

  if (ret && ret != -1) {
    return recursive_operation(path_src, NULL, NULL, delete_single_file, delete_callback_post);
  }

  return ret;
}

static const char *path_destination_ensure_filename(const char *path_src,
                                                    const char *path_dst,
                                                    char *buf,
                                                    size_t buf_size)
{
  if (BLI_is_dir(path_dst)) {
    char *path_src_no_slash = strip_last_slash(path_src);
    const char *filename_src = BLI_path_basename(path_src_no_slash);
    if (filename_src != path_src_no_slash) {
      const size_t buf_size_needed = strlen(path_dst) + 1 + strlen(filename_src) + 1;
      char *path_dst_with_filename = (buf_size_needed <= buf_size) ?
                                         buf :
                                         MEM_mallocN(buf_size_needed, __func__);
      BLI_path_join(path_dst_with_filename, buf_size_needed, path_dst, filename_src);
      path_dst = path_dst_with_filename;
    }
    MEM_freeN(path_src_no_slash);
  }
  return path_dst;
}

int BLI_copy(const char *path_src, const char *path_dst)
{
  char path_dst_buf[FILE_MAX_STATIC_BUF];
  const char *path_dst_with_filename = path_destination_ensure_filename(
      path_src, path_dst, path_dst_buf, sizeof(path_dst_buf));
  int ret;

  ret = recursive_operation(
      path_src, path_dst_with_filename, copy_callback_pre, copy_single_file, NULL);

  if (!ELEM(path_dst_with_filename, path_dst_buf, path_dst)) {
    MEM_freeN((void *)path_dst_with_filename);
  }

  return ret;
}

#  if 0
int BLI_create_symlink(const char *path_src, const char *path_dst)
{
  return symlink(path_dst, path_src);
}
#  endif

#endif

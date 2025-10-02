/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup imbuf
 */

#include <algorithm>
#include <cstdlib>

#include "MEM_guardedalloc.h"

#include "BKE_blendfile.hh"

#include "BLI_fileops.h"
#include "BLI_ghash.h"
#include "BLI_hash_md5.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_system.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"
#include BLI_SYSTEM_PID_H

#include "DNA_space_types.h" /* For FILE_MAX_LIBEXTRA */

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "IMB_metadata.hh"
#include "IMB_thumbs.hh"

#include "MOV_read.hh"

#include <cctype>
#include <cstring>
#include <ctime>
#include <sys/stat.h>
#include <sys/types.h>

#ifdef WIN32
/* Need to include windows.h so _WIN32_IE is defined. */
#  include <windows.h>
#  ifndef _WIN32_IE
/* Minimal requirements for SHGetSpecialFolderPath on MINGW MSVC has this defined already. */
#    define _WIN32_IE 0x0400
#  endif
/* For SHGetSpecialFolderPath, has to be done before BLI_winstuff
 * because 'near' is disabled through BLI_windstuff */
#  include "BLI_winstuff.h"
#  include "utfconv.hh"
#  include <direct.h> /* #chdir */
#  include <shlobj.h>
#endif

#if defined(WIN32) || defined(__APPLE__)
/* pass */
#else
#  define USE_FREEDESKTOP
#endif

/* '$HOME/.cache/thumbnails' or '$HOME/.thumbnails' */
#ifdef USE_FREEDESKTOP
#  define THUMBNAILS "thumbnails"
#else
#  define THUMBNAILS ".thumbnails"
#endif

#define URI_MAX (FILE_MAX * 3 + 8)

static bool get_thumb_dir(char *dir, ThumbSize size)
{
  char *s = dir;
  const char *subdir;
#ifdef WIN32
  wchar_t dir_16[MAX_PATH];
  /* Yes, applications shouldn't store data there, but so does GIMP :). */
  SHGetSpecialFolderPathW(0, dir_16, CSIDL_PROFILE, 0);
  conv_utf_16_to_8(dir_16, dir, FILE_MAX);
  s += strlen(dir);
#else
#  if defined(USE_FREEDESKTOP)
  const char *home_cache = BLI_getenv("XDG_CACHE_HOME");
  const char *home = home_cache ? home_cache : BLI_dir_home();
#  else
  const char *home = BLI_dir_home();
#  endif
  if (!home) {
    return false;
  }
  s += BLI_strncpy_rlen(s, home, FILE_MAX);

#  ifdef USE_FREEDESKTOP
  if (!home_cache) {
    s += BLI_strncpy_rlen(s, "/.cache", FILE_MAX - (s - dir));
  }
#  endif
#endif
  switch (size) {
    case THB_NORMAL:
      subdir = SEP_STR THUMBNAILS SEP_STR "normal" SEP_STR;
      break;
    case THB_LARGE:
      subdir = SEP_STR THUMBNAILS SEP_STR "large" SEP_STR;
      break;
    case THB_FAIL:
      subdir = SEP_STR THUMBNAILS SEP_STR "fail" SEP_STR "blender" SEP_STR;
      break;
    default:
      return false; /* unknown size */
  }

  s += BLI_strncpy_rlen(s, subdir, FILE_MAX - (s - dir));
  (void)s;

  return true;
}

#undef THUMBNAILS

/* --- Begin of adapted code from glib. --- */

/* -------------------------------------------------------------------- */
/** \name Escape URI String
 *
 * The following code is adapted from function g_escape_uri_string from the gnome glib
 * Source: http://svn.gnome.org/viewcvs/glib/trunk/glib/gconvert.c?view=markup
 * released under the Gnu General Public License.
 *
 * \{ */

enum eUnsafeCharacterSet {
  UNSAFE_ALL = 0x1,        /* Escape all unsafe characters. */
  UNSAFE_ALLOW_PLUS = 0x2, /* Allows '+' */
  UNSAFE_PATH = 0x8,       /* Allows '/', '&', '=', ':', '@', '+', '$' and ',' */
  UNSAFE_HOST = 0x10,      /* Allows '/' and ':' and '@' */
  UNSAFE_SLASHES = 0x20,   /* Allows all characters except for '/' and '%' */
};

/* Don't lose comment alignment. */
/* clang-format off */
static const uchar acceptable[96] = {
    /* A table of the ASCII chars from space (32) to DEL (127) */
    /*      !    "    #    $    %    &    '    (    )    *    +    ,    -    .    / */
    0x00,0x3F,0x20,0x20,0x28,0x00,0x2C,0x3F,0x3F,0x3F,0x3F,0x2A,0x28,0x3F,0x3F,0x1C,
    /* 0    1    2    3    4    5    6    7    8    9    :    ;    <    =    >    ? */
    0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x38,0x20,0x20,0x2C,0x20,0x20,
    /* @    A    B    C    D    E    F    G    H    I    J    K    L    M    N    O */
    0x38,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,
    /* P    Q    R    S    T    U    V    W    X    Y    Z    [    \    ]    ^    _ */
    0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x20,0x20,0x20,0x20,0x3F,
    /* `    a    b    c    d    e    f    g    h    i    j    k    l    m    n    o */
    0x20,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,
    /* p    q    r    s    t    u    v    w    x    y    z    {    |    }    ~  DEL */
    0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x20,0x20,0x20,0x3F,0x20,
};
/* clang-format on */

static const char hex[17] = "0123456789abcdef";

/* NOTE: This escape function works on file: URIs, but if you want to
 * escape something else, please read RFC-2396 */
static void escape_uri_string(const char *string,
                              char *escaped_string,
                              const int escaped_string_size,
                              const eUnsafeCharacterSet mask)
{
#define ACCEPTABLE(a) ((a) >= 32 && (a) < 128 && (acceptable[(a) - 32] & mask))

  BLI_assert(escaped_string_size > 0);
  /* Remove space for \0. */
  int escaped_string_len = escaped_string_size - 1;

  const char *p;
  char *q;
  int c;

  for (q = escaped_string, p = string; (*p != '\0') && escaped_string_len; p++) {
    c = uchar(*p);

    if (!ACCEPTABLE(c)) {
      if (escaped_string_len < 3) {
        break;
      }

      *q++ = '%'; /* means hex coming */
      *q++ = hex[c >> 4];
      *q++ = hex[c & 15];
      escaped_string_len -= 3;
    }
    else {
      *q++ = *p;
      escaped_string_len -= 1;
    }
  }

  *q = '\0';
}

/** \} */

/* --- End of adapted code from glib. --- */

static bool thumbhash_from_path(const char * /*path*/, ThumbSource source, char *r_hash)
{
  switch (source) {
    case THB_SOURCE_FONT:
      return IMB_thumb_load_font_get_hash(r_hash);
    default:
      r_hash[0] = '\0';
      return false;
  }
}

static bool uri_from_filepath(const char *path, char *uri)
{
  char orig_uri[URI_MAX];

#ifdef WIN32
  bool path_is_unc = BLI_path_is_unc(path);
  char path_unc_normalized[FILE_MAX];
  if (path_is_unc) {
    STRNCPY(path_unc_normalized, path);
    BLI_path_normalize_unc(path_unc_normalized, sizeof(path_unc_normalized));
    path = path_unc_normalized;
    /* Assign again because a normalized UNC path may resolve to a drive letter. */
    path_is_unc = BLI_path_is_unc(path);
  }

  if (path_is_unc) {
    /* Skip over the `\\` prefix, it's not needed for a URI. */
    SNPRINTF(orig_uri, "file://%s", BLI_path_slash_skip(path));
  }
  else if (BLI_path_is_win32_drive(path)) {
    SNPRINTF(orig_uri, "file:///%s", path);
    /* Always use an uppercase drive/volume letter in the URI. */
    orig_uri[8] = char(toupper(orig_uri[8]));
  }
  else {
    /* Not a correct absolute path with a drive letter or UNC prefix. */
    return false;
  }
  BLI_string_replace_char(orig_uri, '\\', '/');
#else
  SNPRINTF(orig_uri, "file://%s", path);
#endif

  escape_uri_string(orig_uri, uri, URI_MAX, UNSAFE_PATH);

  return true;
}

static bool thumbpathname_from_uri(const char *uri,
                                   char *r_path,
                                   const int path_maxncpy,
                                   char *r_name,
                                   int name_maxncpy,
                                   ThumbSize size)
{
  char name_buff[40];

  if (r_path && !r_name) {
    r_name = name_buff;
    name_maxncpy = sizeof(name_buff);
  }

  if (r_name) {
    char hexdigest[33];
    uchar digest[16];
    BLI_hash_md5_buffer(uri, strlen(uri), digest);
    hexdigest[0] = '\0';
    BLI_snprintf(r_name, name_maxncpy, "%s.png", BLI_hash_md5_to_hexdigest(digest, hexdigest));
    //      printf("%s: '%s' --> '%s'\n", __func__, uri, r_name);
  }

  if (r_path) {
    char tmppath[FILE_MAX];

    if (get_thumb_dir(tmppath, size)) {
      BLI_snprintf(r_path, path_maxncpy, "%s%s", tmppath, r_name);
      //          printf("%s: '%s' --> '%s'\n", __func__, uri, r_path);
      return true;
    }
  }
  return false;
}

static void thumbname_from_uri(const char *uri, char *thumb, const int thumb_maxncpy)
{
  thumbpathname_from_uri(uri, nullptr, 0, thumb, thumb_maxncpy, THB_FAIL);
}

static bool thumbpath_from_uri(const char *uri, char *path, const int path_maxncpy, ThumbSize size)
{
  return thumbpathname_from_uri(uri, path, path_maxncpy, nullptr, 0, size);
}

void IMB_thumb_makedirs()
{
  char tpath[FILE_MAX];
#if 0 /* UNUSED */
  if (get_thumb_dir(tpath, THB_NORMAL)) {
    BLI_dir_create_recursive(tpath);
  }
#endif
  if (get_thumb_dir(tpath, THB_LARGE)) {
    BLI_dir_create_recursive(tpath);
  }
  if (get_thumb_dir(tpath, THB_FAIL)) {
    BLI_dir_create_recursive(tpath);
  }
}

/* create thumbnail for file and returns new imbuf for thumbnail */
static ImBuf *thumb_create_ex(const char *file_path,
                              const char *uri,
                              const char *thumb,
                              const bool use_hash,
                              const char *hash,
                              const char *blen_group,
                              const char *blen_id,
                              ThumbSize size,
                              ThumbSource source,
                              ImBuf *img)
{
  char desc[URI_MAX + 22];
  char tpath[FILE_MAX];
  char tdir[FILE_MAX];
  char temp[FILE_MAX];
  char mtime[40] = "0"; /* in case we can't stat the file */
  short tsize = 128;
  BLI_stat_t info;

  switch (size) {
    case THB_NORMAL:
      tsize = PREVIEW_RENDER_DEFAULT_HEIGHT;
      break;
    case THB_LARGE:
      tsize = PREVIEW_RENDER_LARGE_HEIGHT;
      break;
    case THB_FAIL:
      tsize = 1;
      break;
    default:
      return nullptr; /* unknown size */
  }

  if (get_thumb_dir(tdir, size)) {
    SNPRINTF(tpath, "%s%s", tdir, thumb);
    // thumb[8] = '\0'; /* shorten for `temp` name, not needed anymore */
    SNPRINTF(temp, "%sblender_%d_%s.png", tdir, abs(getpid()), thumb);
    if (BLI_path_ncmp(file_path, tdir, sizeof(tdir)) == 0) {
      return nullptr;
    }
    if (size == THB_FAIL) {
      img = IMB_allocImBuf(1, 1, 32, IB_byte_data | IB_metadata);
      if (!img) {
        return nullptr;
      }
    }
    else {
      if (ELEM(source, THB_SOURCE_IMAGE, THB_SOURCE_BLEND, THB_SOURCE_FONT, THB_SOURCE_OBJECT_IO))
      {
        /* only load if we didn't give an image */
        if (img == nullptr) {
          switch (source) {
            case THB_SOURCE_IMAGE:
              img = IMB_thumb_load_image(file_path, tsize, nullptr);
              break;
            case THB_SOURCE_BLEND:
              img = IMB_thumb_load_blend(file_path, blen_group, blen_id);
              break;
            case THB_SOURCE_FONT:
              img = IMB_thumb_load_font(file_path, tsize, tsize);
              break;
            case THB_SOURCE_OBJECT_IO: {
              if (BLI_path_extension_check(file_path, ".svg")) {
                img = IMB_thumb_load_image(file_path, tsize, nullptr);
              }
              break;
            }
            default:
              BLI_assert_unreachable(); /* This should never happen */
          }
        }

        if (img != nullptr) {
          if (BLI_stat(file_path, &info) != -1) {
            SNPRINTF_UTF8(mtime, "%ld", (long int)info.st_mtime);
          }
        }
      }
      else if (THB_SOURCE_MOVIE == source) {
        MovieReader *anim = nullptr;
        /* Image buffer is converted from float to byte and only the latter one is used, and the
         * conversion process is aware of the float colorspace. So it is possible to save some
         * compute time by keeping the original colorspace for movies. */
        anim = MOV_open_file(file_path, IB_byte_data | IB_metadata, 0, true, nullptr);
        if (anim != nullptr) {
          img = MOV_decode_frame(anim, 0, IMB_TC_NONE, IMB_PROXY_NONE);
          if (img == nullptr) {
            // printf("not an anim; %s\n", file_path);
          }
          else {
            IMB_freeImBuf(img);
            img = MOV_decode_preview_frame(anim);
          }
          MOV_close(anim);
        }
        if (BLI_stat(file_path, &info) != -1) {
          SNPRINTF_UTF8(mtime, "%ld", (long int)info.st_mtime);
        }
      }
      if (!img) {
        return nullptr;
      }

      if (img->x > tsize || img->y > tsize) {
        float scale = std::min(float(tsize) / float(img->x), float(tsize) / float(img->y));
        /* Scaling down must never assign zero width/height, see: #89868. */
        short ex = std::max(short(1), short(img->x * scale));
        short ey = std::max(short(1), short(img->y * scale));
        /* Save some time by only scaling byte buffer. */
        if (img->float_buffer.data) {
          if (img->byte_buffer.data == nullptr) {
            IMB_byte_from_float(img);
          }
          IMB_free_float_pixels(img);
        }
        IMB_scale(img, ex, ey, IMBScaleFilter::Box, false);
      }
    }
    SNPRINTF_UTF8(desc, "Thumbnail for %s", uri);
    IMB_metadata_ensure(&img->metadata);
    IMB_metadata_set_field(img->metadata, "Software", "Blender");
    IMB_metadata_set_field(img->metadata, "Thumb::URI", uri);
    IMB_metadata_set_field(img->metadata, "Description", desc);
    IMB_metadata_set_field(img->metadata, "Thumb::MTime", mtime);
    if (use_hash) {
      IMB_metadata_set_field(img->metadata, "X-Blender::Hash", hash);
    }
    img->ftype = IMB_FTYPE_PNG;
    img->planes = 32;

    /* If we generated from a 16bit PNG e.g., we have a float rect, not a byte one - fix this. */
    IMB_byte_from_float(img);
    IMB_free_float_pixels(img);

    if (IMB_save_image(img, temp, IB_byte_data | IB_metadata)) {
#ifndef WIN32
      chmod(temp, S_IRUSR | S_IWUSR);
#endif
      // printf("%s saving thumb: '%s'\n", __func__, tpath);

      BLI_rename_overwrite(temp, tpath);
    }
  }
  return img;
}

static ImBuf *thumb_create_or_fail(const char *file_path,
                                   const char *uri,
                                   const char *thumb,
                                   const bool use_hash,
                                   const char *hash,
                                   const char *blen_group,
                                   const char *blen_id,
                                   ThumbSize size,
                                   ThumbSource source)
{
  ImBuf *img = thumb_create_ex(
      file_path, uri, thumb, use_hash, hash, blen_group, blen_id, size, source, nullptr);

  if (!img) {
    /* thumb creation failed, write fail thumb */
    img = thumb_create_ex(
        file_path, uri, thumb, use_hash, hash, blen_group, blen_id, THB_FAIL, source, nullptr);
    if (img) {
      /* we don't need failed thumb anymore */
      IMB_freeImBuf(img);
      img = nullptr;
    }
  }

  return img;
}

ImBuf *IMB_thumb_create(const char *filepath, ThumbSize size, ThumbSource source, ImBuf *img)
{
  char uri[URI_MAX] = "";
  char thumb_name[40];

  if (!uri_from_filepath(filepath, uri)) {
    return nullptr;
  }
  thumbname_from_uri(uri, thumb_name, sizeof(thumb_name));

  return thumb_create_ex(
      filepath, uri, thumb_name, false, THUMB_DEFAULT_HASH, nullptr, nullptr, size, source, img);
}

ImBuf *IMB_thumb_read(const char *file_or_lib_path, ThumbSize size)
{
  char thumb[FILE_MAX];
  char uri[URI_MAX];
  ImBuf *img = nullptr;

  if (!uri_from_filepath(file_or_lib_path, uri)) {
    return nullptr;
  }
  if (thumbpath_from_uri(uri, thumb, sizeof(thumb), size)) {
    img = IMB_load_image_from_filepath(thumb, IB_byte_data | IB_metadata);
  }

  return img;
}

void IMB_thumb_delete(const char *file_or_lib_path, ThumbSize size)
{
  char thumb[FILE_MAX];
  char uri[URI_MAX];

  if (!uri_from_filepath(file_or_lib_path, uri)) {
    return;
  }
  if (thumbpath_from_uri(uri, thumb, sizeof(thumb), size)) {
    if (BLI_path_ncmp(file_or_lib_path, thumb, sizeof(thumb)) == 0) {
      return;
    }
    if (BLI_exists(thumb)) {
      BLI_delete(thumb, false, false);
    }
  }
}

ImBuf *IMB_thumb_manage(const char *file_or_lib_path, ThumbSize size, ThumbSource source)
{
  char path_buff[FILE_MAX_LIBEXTRA];
  char *blen_group = nullptr, *blen_id = nullptr;

  /* Will be the actual path to the file, i.e. the same as #file_or_lib_path, or if that points
   * into a .blend, the path of the .blend. */
  const char *file_path = file_or_lib_path;
  if (source == THB_SOURCE_BLEND) {
    if (BKE_blendfile_library_path_explode(file_or_lib_path, path_buff, &blen_group, &blen_id)) {
      if (blen_group) {
        if (!blen_id) {
          /* No preview for blen groups */
          return nullptr;
        }
        file_path = path_buff; /* path needs to be a valid file! */
      }
    }
  }

  BLI_stat_t st;
  if (BLI_stat(file_path, &st) == -1) {
    return nullptr;
  }
  char uri[URI_MAX];
  if (!uri_from_filepath(file_or_lib_path, uri)) {
    return nullptr;
  }

  /* Don't access offline files, only use already existing thumbnails (don't recreate). */
  const eFileAttributes file_attributes = BLI_file_attributes(file_path);
  if (file_attributes & FILE_ATTR_OFFLINE) {
    char thumb_path[FILE_MAX];
    if (thumbpath_from_uri(uri, thumb_path, sizeof(thumb_path), size)) {
      return IMB_load_image_from_filepath(thumb_path, IB_byte_data | IB_metadata);
    }
    return nullptr;
  }

  char thumb_path[FILE_MAX];
  if (thumbpath_from_uri(uri, thumb_path, sizeof(thumb_path), THB_FAIL)) {
    /* failure thumb exists, don't try recreating */
    if (BLI_exists(thumb_path)) {
      /* clear out of date fail case (note for blen IDs we use blender file itself here) */
      if (BLI_file_older(thumb_path, file_path)) {
        BLI_delete(thumb_path, false, false);
      }
      else {
        return nullptr;
      }
    }
  }

  ImBuf *img = nullptr;
  char thumb_name[40];
  if (thumbpathname_from_uri(
          uri, thumb_path, sizeof(thumb_path), thumb_name, sizeof(thumb_name), size))
  {
    /* The requested path points to a generated thumbnail already (path into the thumbnail cache
     * directory). Attempt to load that, there's nothing we can recreate. */
    if (BLI_path_ncmp(file_or_lib_path, thumb_path, sizeof(thumb_path)) == 0) {
      img = IMB_load_image_from_filepath(file_or_lib_path, IB_byte_data);
    }
    else {
      img = IMB_load_image_from_filepath(thumb_path, IB_byte_data | IB_metadata);
      if (img) {
        bool regenerate = false;

        char mtime[40];
        char thumb_hash[33];
        char thumb_hash_curr[33];

        const bool use_hash = thumbhash_from_path(file_path, source, thumb_hash);

        if (IMB_metadata_get_field(img->metadata, "Thumb::MTime", mtime, sizeof(mtime))) {
          regenerate = (st.st_mtime != atol(mtime));
        }
        else {
          /* illegal thumb, regenerate it! */
          regenerate = true;
        }

        if (use_hash && !regenerate) {
          if (IMB_metadata_get_field(
                  img->metadata, "X-Blender::Hash", thumb_hash_curr, sizeof(thumb_hash_curr)))
          {
            regenerate = !STREQ(thumb_hash, thumb_hash_curr);
          }
          else {
            regenerate = true;
          }
        }

        if (regenerate) {
          /* recreate all thumbs */
          IMB_freeImBuf(img);
          img = nullptr;
          IMB_thumb_delete(file_or_lib_path, THB_NORMAL);
          IMB_thumb_delete(file_or_lib_path, THB_LARGE);
          IMB_thumb_delete(file_or_lib_path, THB_FAIL);
          img = thumb_create_or_fail(
              file_path, uri, thumb_name, use_hash, thumb_hash, blen_group, blen_id, size, source);
        }
      }
      else {
        char thumb_hash[33];
        const bool use_hash = thumbhash_from_path(file_path, source, thumb_hash);

        img = thumb_create_or_fail(
            file_path, uri, thumb_name, use_hash, thumb_hash, blen_group, blen_id, size, source);
      }
    }
  }

  /* Our imbuf **must** have a valid rect (i.e. 8-bits/channels)
   * data, we rely on this in draw code.
   * However, in some cases we may end loading 16bits PNGs, which generated float buffers.
   * This should be taken care of in generation step, but add also a safeguard here! */
  if (img) {
    IMB_byte_from_float(img);
    IMB_free_float_pixels(img);
  }

  return img;
}

/* ***** Threading ***** */
/* Thumbnail handling is not really threadsafe in itself.
 * However, as long as we do not operate on the same file, we shall have no collision.
 * So idea is to 'lock' a given source file path.
 */

static struct IMBThumbLocks {
  GSet *locked_paths;
  int lock_counter;
  ThreadCondition cond;
} thumb_locks = {nullptr};

void IMB_thumb_locks_acquire()
{
  BLI_thread_lock(LOCK_IMAGE);

  if (thumb_locks.lock_counter == 0) {
    BLI_assert(thumb_locks.locked_paths == nullptr);
    thumb_locks.locked_paths = BLI_gset_str_new(__func__);
    BLI_condition_init(&thumb_locks.cond);
  }
  thumb_locks.lock_counter++;

  BLI_assert(thumb_locks.locked_paths != nullptr);
  BLI_assert(thumb_locks.lock_counter > 0);
  BLI_thread_unlock(LOCK_IMAGE);
}

void IMB_thumb_locks_release()
{
  BLI_thread_lock(LOCK_IMAGE);
  BLI_assert((thumb_locks.locked_paths != nullptr) && (thumb_locks.lock_counter > 0));

  thumb_locks.lock_counter--;
  if (thumb_locks.lock_counter == 0) {
    BLI_gset_free(thumb_locks.locked_paths, MEM_freeN);
    thumb_locks.locked_paths = nullptr;
    BLI_condition_end(&thumb_locks.cond);
  }

  BLI_thread_unlock(LOCK_IMAGE);
}

void IMB_thumb_path_lock(const char *path)
{
  void *key = BLI_strdup(path);

  BLI_thread_lock(LOCK_IMAGE);
  BLI_assert((thumb_locks.locked_paths != nullptr) && (thumb_locks.lock_counter > 0));

  if (thumb_locks.locked_paths) {
    while (!BLI_gset_add(thumb_locks.locked_paths, key)) {
      BLI_condition_wait_global_mutex(&thumb_locks.cond, LOCK_IMAGE);
    }
  }

  BLI_thread_unlock(LOCK_IMAGE);
}

void IMB_thumb_path_unlock(const char *path)
{
  const void *key = path;

  BLI_thread_lock(LOCK_IMAGE);
  BLI_assert((thumb_locks.locked_paths != nullptr) && (thumb_locks.lock_counter > 0));

  if (thumb_locks.locked_paths) {
    if (!BLI_gset_remove(thumb_locks.locked_paths, key, MEM_freeN)) {
      BLI_assert_unreachable();
    }
    BLI_condition_notify_all(&thumb_locks.cond);
  }

  BLI_thread_unlock(LOCK_IMAGE);
}

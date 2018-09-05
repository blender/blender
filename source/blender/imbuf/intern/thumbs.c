/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2007 Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Andrea Weikert.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/imbuf/intern/thumbs.c
 *  \ingroup imbuf
 */

#include <stdio.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_string.h"
#include "BLI_path_util.h"
#include "BLI_fileops.h"
#include "BLI_ghash.h"
#include "BLI_hash_md5.h"
#include "BLI_system.h"
#include "BLI_threads.h"
#include BLI_SYSTEM_PID_H

#include "BLO_readfile.h"

#include "DNA_space_types.h"  /* For FILE_MAX_LIBEXTRA */

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_thumbs.h"
#include "IMB_metadata.h"

#include <ctype.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>

#ifdef WIN32
#  include <windows.h> /* need to include windows.h so _WIN32_IE is defined  */
#  ifndef _WIN32_IE
#    define _WIN32_IE 0x0400 /* minimal requirements for SHGetSpecialFolderPath on MINGW MSVC has this defined already */
#  endif
#  include <shlobj.h>  /* for SHGetSpecialFolderPath, has to be done before BLI_winstuff
                        * because 'near' is disabled through BLI_windstuff */
#  include <direct.h> /* chdir */
#  include "BLI_winstuff.h"
#  include "utfconv.h"
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
	/* yes, applications shouldn't store data there, but so does GIMP :)*/
	SHGetSpecialFolderPathW(0, dir_16, CSIDL_PROFILE, 0);
	conv_utf_16_to_8(dir_16, dir, FILE_MAX);
	s += strlen(dir);
#else
#if defined(USE_FREEDESKTOP)
	const char *home_cache = BLI_getenv("XDG_CACHE_HOME");
	const char *home = home_cache ? home_cache : BLI_getenv("HOME");
#else
	const char *home = BLI_getenv("HOME");
#endif
	if (!home) return 0;
	s += BLI_strncpy_rlen(s, home, FILE_MAX);

#ifdef USE_FREEDESKTOP
	if (!home_cache) {
		s += BLI_strncpy_rlen(s, "/.cache", FILE_MAX - (s - dir));
	}
#endif
#endif
	switch (size) {
		case THB_NORMAL:
			subdir = "/" THUMBNAILS "/normal/";
			break;
		case THB_LARGE:
			subdir = "/" THUMBNAILS "/large/";
			break;
		case THB_FAIL:
			subdir = "/" THUMBNAILS "/fail/blender/";
			break;
		default:
			return 0; /* unknown size */
	}

	s += BLI_strncpy_rlen(s, subdir, FILE_MAX - (s - dir));
	(void)s;

	return 1;
}

#undef THUMBNAILS


/** ----- begin of adapted code from glib ---
 * The following code is adapted from function g_escape_uri_string from the gnome glib
 * Source: http://svn.gnome.org/viewcvs/glib/trunk/glib/gconvert.c?view=markup
 * released under the Gnu General Public License.
 */
typedef enum {
	UNSAFE_ALL        = 0x1, /* Escape all unsafe characters   */
	UNSAFE_ALLOW_PLUS = 0x2, /* Allows '+'  */
	UNSAFE_PATH       = 0x8, /* Allows '/', '&', '=', ':', '@', '+', '$' and ',' */
	UNSAFE_HOST       = 0x10, /* Allows '/' and ':' and '@' */
	UNSAFE_SLASHES    = 0x20 /* Allows all characters except for '/' and '%' */
} UnsafeCharacterSet;

static const unsigned char acceptable[96] = {
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
	0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x3F,0x20,0x20,0x20,0x3F,0x20
};

static const char hex[17] = "0123456789abcdef";

/* Note: This escape function works on file: URIs, but if you want to
 * escape something else, please read RFC-2396 */
static void escape_uri_string(const char *string, char *escaped_string, int escaped_string_size, UnsafeCharacterSet mask)
{
#define ACCEPTABLE(a) ((a) >= 32 && (a) < 128 && (acceptable[(a) - 32] & use_mask))

	const char *p;
	char *q;
	int c;
	UnsafeCharacterSet use_mask;
	use_mask = mask;

	BLI_assert(escaped_string_size > 0);

	/* space for \0 */
	escaped_string_size -= 1;

	for (q = escaped_string, p = string; (*p != '\0') && escaped_string_size; p++) {
		c = (unsigned char) *p;

		if (!ACCEPTABLE(c)) {
			if (escaped_string_size < 3) {
				break;
			}

			*q++ = '%'; /* means hex coming */
			*q++ = hex[c >> 4];
			*q++ = hex[c & 15];
			escaped_string_size -= 3;
		}
		else {
			*q++ = *p;
			escaped_string_size -= 1;
		}
	}

	*q = '\0';
}

/** ----- end of adapted code from glib --- */

static bool thumbhash_from_path(const char *UNUSED(path), ThumbSource source, char *r_hash)
{
	switch (source) {
		case THB_SOURCE_FONT:
			return IMB_thumb_load_font_get_hash(r_hash);
		default:
			r_hash[0] = '\0';
			return false;
	}
}

static bool uri_from_filename(const char *path, char *uri)
{
	char orig_uri[URI_MAX];
	const char *dirstart = path;

#ifdef WIN32
	{
		char vol[3];

		BLI_strncpy(orig_uri, "file:///", FILE_MAX);
		if (strlen(path) < 2 && path[1] != ':') {
			/* not a correct absolute path */
			return 0;
		}
		/* on windows, using always uppercase drive/volume letter in uri */
		vol[0] = (unsigned char)toupper(path[0]);
		vol[1] = ':';
		vol[2] = '\0';
		strcat(orig_uri, vol);
		dirstart += 2;
	}
	strcat(orig_uri, dirstart);
	BLI_str_replace_char(orig_uri, '\\', '/');
#else
	BLI_snprintf(orig_uri, URI_MAX, "file://%s", dirstart);
#endif

	escape_uri_string(orig_uri, uri, URI_MAX, UNSAFE_PATH);

	return 1;
}

static bool thumbpathname_from_uri(
        const char *uri, char *r_path, const int path_len, char *r_name, int name_len, ThumbSize size)
{
	char name_buff[40];

	if (r_path && !r_name) {
		r_name = name_buff;
		name_len = sizeof(name_buff);
	}

	if (r_name) {
		char hexdigest[33];
		unsigned char digest[16];
		BLI_hash_md5_buffer(uri, strlen(uri), digest);
		hexdigest[0] = '\0';
		BLI_snprintf(r_name, name_len, "%s.png", BLI_hash_md5_to_hexdigest(digest, hexdigest));
//		printf("%s: '%s' --> '%s'\n", __func__, uri, r_name);
	}

	if (r_path) {
		char tmppath[FILE_MAX];

		if (get_thumb_dir(tmppath, size)) {
			BLI_snprintf(r_path, path_len, "%s%s", tmppath, r_name);
//			printf("%s: '%s' --> '%s'\n", __func__, uri, r_path);
			return true;
		}
	}
	return false;
}

static void thumbname_from_uri(const char *uri, char *thumb, const int thumb_len)
{
	thumbpathname_from_uri(uri, NULL, 0, thumb, thumb_len, THB_FAIL);
}

static bool thumbpath_from_uri(const char *uri, char *path, const int path_len, ThumbSize size)
{
	return thumbpathname_from_uri(uri, path, path_len, NULL, 0, size);
}

void IMB_thumb_makedirs(void)
{
	char tpath[FILE_MAX];
#if 0  /* UNUSED */
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
static ImBuf *thumb_create_ex(
        const char *file_path, const char *uri, const char *thumb, const bool use_hash, const char *hash,
        const char *blen_group, const char *blen_id,
        ThumbSize size, ThumbSource source, ImBuf *img)
{
	char desc[URI_MAX + 22];
	char tpath[FILE_MAX];
	char tdir[FILE_MAX];
	char temp[FILE_MAX];
	char mtime[40] = "0"; /* in case we can't stat the file */
	char cwidth[40] = "0"; /* in case images have no data */
	char cheight[40] = "0";
	short tsize = 128;
	short ex, ey;
	float scaledx, scaledy;
	BLI_stat_t info;

	switch (size) {
		case THB_NORMAL:
			tsize = PREVIEW_RENDER_DEFAULT_HEIGHT;
			break;
		case THB_LARGE:
			tsize = PREVIEW_RENDER_DEFAULT_HEIGHT * 2;
			break;
		case THB_FAIL:
			tsize = 1;
			break;
		default:
			return NULL; /* unknown size */
	}

	/* exception, skip images over 100mb */
	if (source == THB_SOURCE_IMAGE) {
		const size_t file_size = BLI_file_size(file_path);
		if (file_size != -1 && file_size > THUMB_SIZE_MAX) {
			// printf("file too big: %d, skipping %s\n", (int)size, file_path);
			return NULL;
		}
	}

	if (get_thumb_dir(tdir, size)) {
		BLI_snprintf(tpath, FILE_MAX, "%s%s", tdir, thumb);
//		thumb[8] = '\0'; /* shorten for tempname, not needed anymore */
		BLI_snprintf(temp, FILE_MAX, "%sblender_%d_%s.png", tdir, abs(getpid()), thumb);
		if (BLI_path_ncmp(file_path, tdir, sizeof(tdir)) == 0) {
			return NULL;
		}
		if (size == THB_FAIL) {
			img = IMB_allocImBuf(1, 1, 32, IB_rect | IB_metadata);
			if (!img) return NULL;
		}
		else {
			if (ELEM(source, THB_SOURCE_IMAGE, THB_SOURCE_BLEND, THB_SOURCE_FONT)) {
				/* only load if we didn't give an image */
				if (img == NULL) {
					switch (source) {
						case THB_SOURCE_IMAGE:
							img = IMB_loadiffname(file_path, IB_rect | IB_metadata, NULL);
							break;
						case THB_SOURCE_BLEND:
							img = IMB_thumb_load_blend(file_path, blen_group, blen_id);
							break;
						case THB_SOURCE_FONT:
							img = IMB_thumb_load_font(file_path, tsize, tsize);
							break;
						default:
							BLI_assert(0); /* This should never happen */
					}
				}

				if (img != NULL) {
					if (BLI_stat(file_path, &info) != -1) {
						BLI_snprintf(mtime, sizeof(mtime), "%ld", (long int)info.st_mtime);
					}
					BLI_snprintf(cwidth, sizeof(cwidth), "%d", img->x);
					BLI_snprintf(cheight, sizeof(cheight), "%d", img->y);
				}
			}
			else if (THB_SOURCE_MOVIE == source) {
				struct anim *anim = NULL;
				anim = IMB_open_anim(file_path, IB_rect | IB_metadata, 0, NULL);
				if (anim != NULL) {
					img = IMB_anim_absolute(anim, 0, IMB_TC_NONE, IMB_PROXY_NONE);
					if (img == NULL) {
						printf("not an anim; %s\n", file_path);
					}
					else {
						IMB_freeImBuf(img);
						img = IMB_anim_previewframe(anim);
					}
					IMB_free_anim(anim);
				}
				if (BLI_stat(file_path, &info) != -1) {
					BLI_snprintf(mtime, sizeof(mtime), "%ld", (long int)info.st_mtime);
				}
			}
			if (!img) return NULL;

			if (img->x > img->y) {
				scaledx = (float)tsize;
				scaledy =  ( (float)img->y / (float)img->x) * tsize;
			}
			else {
				scaledy = (float)tsize;
				scaledx =  ( (float)img->x / (float)img->y) * tsize;
			}
			ex = (short)scaledx;
			ey = (short)scaledy;

			/* save some time by only scaling byte buf */
			if (img->rect_float) {
				if (img->rect == NULL) {
					IMB_rect_from_float(img);
				}

				imb_freerectfloatImBuf(img);
			}

			IMB_scaleImBuf(img, ex, ey);
		}
		BLI_snprintf(desc, sizeof(desc), "Thumbnail for %s", uri);
		IMB_metadata_ensure(&img->metadata);
		IMB_metadata_set_field(img->metadata, "Software", "Blender");
		IMB_metadata_set_field(img->metadata, "Thumb::URI", uri);
		IMB_metadata_set_field(img->metadata, "Description", desc);
		IMB_metadata_set_field(img->metadata, "Thumb::MTime", mtime);
		if (use_hash) {
			IMB_metadata_set_field(img->metadata, "X-Blender::Hash", hash);
		}
		if (ELEM(source, THB_SOURCE_IMAGE, THB_SOURCE_BLEND, THB_SOURCE_FONT)) {
			IMB_metadata_set_field(img->metadata, "Thumb::Image::Width", cwidth);
			IMB_metadata_set_field(img->metadata, "Thumb::Image::Height", cheight);
		}
		img->ftype = IMB_FTYPE_PNG;
		img->planes = 32;

		/* If we generated from a 16bit PNG e.g., we have a float rect, not a byte one - fix this. */
		IMB_rect_from_float(img);
		imb_freerectfloatImBuf(img);

		if (IMB_saveiff(img, temp, IB_rect | IB_metadata)) {
#ifndef WIN32
			chmod(temp, S_IRUSR | S_IWUSR);
#endif
			// printf("%s saving thumb: '%s'\n", __func__, tpath);

			BLI_rename(temp, tpath);
		}
	}
	return img;
}

static ImBuf *thumb_create_or_fail(
        const char *file_path, const char *uri, const char *thumb, const bool use_hash, const char *hash,
        const char *blen_group, const char *blen_id, ThumbSize size, ThumbSource source)
{
	ImBuf *img = thumb_create_ex(file_path, uri, thumb, use_hash, hash, blen_group, blen_id, size, source, NULL);

	if (!img) {
		/* thumb creation failed, write fail thumb */
		img = thumb_create_ex(file_path, uri, thumb, use_hash, hash, blen_group, blen_id, THB_FAIL, source, NULL);
		if (img) {
			/* we don't need failed thumb anymore */
			IMB_freeImBuf(img);
			img = NULL;
		}
	}

	return img;
}

ImBuf *IMB_thumb_create(const char *path, ThumbSize size, ThumbSource source, ImBuf *img)
{
	char uri[URI_MAX] = "";
	char thumb_name[40];

	if (!uri_from_filename(path, uri)) {
		return NULL;
	}
	thumbname_from_uri(uri, thumb_name, sizeof(thumb_name));

	return thumb_create_ex(path, uri, thumb_name, false, THUMB_DEFAULT_HASH, NULL, NULL, size, source, img);
}

/* read thumbnail for file and returns new imbuf for thumbnail */
ImBuf *IMB_thumb_read(const char *path, ThumbSize size)
{
	char thumb[FILE_MAX];
	char uri[URI_MAX];
	ImBuf *img = NULL;

	if (!uri_from_filename(path, uri)) {
		return NULL;
	}
	if (thumbpath_from_uri(uri, thumb, sizeof(thumb), size)) {
		img = IMB_loadiffname(thumb, IB_rect | IB_metadata, NULL);
	}

	return img;
}

/* delete all thumbs for the file */
void IMB_thumb_delete(const char *path, ThumbSize size)
{
	char thumb[FILE_MAX];
	char uri[URI_MAX];

	if (!uri_from_filename(path, uri)) {
		return;
	}
	if (thumbpath_from_uri(uri, thumb, sizeof(thumb), size)) {
		if (BLI_path_ncmp(path, thumb, sizeof(thumb)) == 0) {
			return;
		}
		if (BLI_exists(thumb)) {
			BLI_delete(thumb, false, false);
		}
	}
}


/* create the thumb if necessary and manage failed and old thumbs */
ImBuf *IMB_thumb_manage(const char *org_path, ThumbSize size, ThumbSource source)
{
	char thumb_path[FILE_MAX];
	char thumb_name[40];
	char uri[URI_MAX];
	char path_buff[FILE_MAX_LIBEXTRA];
	const char *file_path;
	const char *path;
	BLI_stat_t st;
	ImBuf *img = NULL;
	char *blen_group = NULL, *blen_id = NULL;

	path = file_path = org_path;
	if (source == THB_SOURCE_BLEND) {
		if (BLO_library_path_explode(path, path_buff, &blen_group, &blen_id)) {
			if (blen_group) {
				if (!blen_id) {
					/* No preview for blen groups */
					return NULL;
				}
				file_path = path_buff;  /* path needs to be a valid file! */
			}
		}
	}

	if (BLI_stat(file_path, &st) == -1) {
		return NULL;
	}
	if (!uri_from_filename(path, uri)) {
		return NULL;
	}
	if (thumbpath_from_uri(uri, thumb_path, sizeof(thumb_path), THB_FAIL)) {
		/* failure thumb exists, don't try recreating */
		if (BLI_exists(thumb_path)) {
			/* clear out of date fail case (note for blen IDs we use blender file itself here) */
			if (BLI_file_older(thumb_path, file_path)) {
				BLI_delete(thumb_path, false, false);
			}
			else {
				return NULL;
			}
		}
	}

	if (thumbpathname_from_uri(uri, thumb_path, sizeof(thumb_path), thumb_name, sizeof(thumb_name), size)) {
		if (BLI_path_ncmp(path, thumb_path, sizeof(thumb_path)) == 0) {
			img = IMB_loadiffname(path, IB_rect, NULL);
		}
		else {
			img = IMB_loadiffname(thumb_path, IB_rect | IB_metadata, NULL);
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
					if (IMB_metadata_get_field(img->metadata, "X-Blender::Hash", thumb_hash_curr, sizeof(thumb_hash_curr))) {
						regenerate = !STREQ(thumb_hash, thumb_hash_curr);
					}
					else {
						regenerate = true;
					}
				}

				if (regenerate) {
					/* recreate all thumbs */
					IMB_freeImBuf(img);
					img = NULL;
					IMB_thumb_delete(path, THB_NORMAL);
					IMB_thumb_delete(path, THB_LARGE);
					IMB_thumb_delete(path, THB_FAIL);
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

	/* Our imbuf **must** have a valid rect (i.e. 8-bits/channels) data, we rely on this in draw code.
	 * However, in some cases we may end loading 16bits PNGs, which generated float buffers.
	 * This should be taken care of in generation step, but add also a safeguard here! */
	if (img) {
		IMB_rect_from_float(img);
		imb_freerectfloatImBuf(img);
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
} thumb_locks = {0};

void IMB_thumb_locks_acquire(void)
{
	BLI_thread_lock(LOCK_IMAGE);

	if (thumb_locks.lock_counter == 0) {
		BLI_assert(thumb_locks.locked_paths == NULL);
		thumb_locks.locked_paths = BLI_gset_str_new(__func__);
		BLI_condition_init(&thumb_locks.cond);
	}
	thumb_locks.lock_counter++;

	BLI_assert(thumb_locks.locked_paths != NULL);
	BLI_assert(thumb_locks.lock_counter > 0);
	BLI_thread_unlock(LOCK_IMAGE);
}

void IMB_thumb_locks_release(void)
{
	BLI_thread_lock(LOCK_IMAGE);
	BLI_assert((thumb_locks.locked_paths != NULL) && (thumb_locks.lock_counter > 0));

	thumb_locks.lock_counter--;
	if (thumb_locks.lock_counter == 0) {
		BLI_gset_free(thumb_locks.locked_paths, MEM_freeN);
		thumb_locks.locked_paths = NULL;
		BLI_condition_end(&thumb_locks.cond);
	}

	BLI_thread_unlock(LOCK_IMAGE);
}

void IMB_thumb_path_lock(const char *path)
{
	void *key = BLI_strdup(path);

	BLI_thread_lock(LOCK_IMAGE);
	BLI_assert((thumb_locks.locked_paths != NULL) && (thumb_locks.lock_counter > 0));

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
	BLI_assert((thumb_locks.locked_paths != NULL) && (thumb_locks.lock_counter > 0));

	if (thumb_locks.locked_paths) {
		if (!BLI_gset_remove(thumb_locks.locked_paths, key, MEM_freeN)) {
			BLI_assert(0);
		}
		BLI_condition_notify_all(&thumb_locks.cond);
	}

	BLI_thread_unlock(LOCK_IMAGE);
}

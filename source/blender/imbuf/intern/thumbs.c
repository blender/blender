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

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_md5.h"

#include "BKE_utildefines.h"

#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"
#include "IMB_thumbs.h"
#include "IMB_metadata.h"

#include <ctype.h>
#include <stdlib.h>
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
#  include <process.h> /* getpid */
#  include <direct.h> /* chdir */
#  include "BLI_winstuff.h"
#  include "utfconv.h"
#else
#  include <unistd.h>
#endif

#define URI_MAX FILE_MAX*3 + 8

static int get_thumb_dir( char* dir , ThumbSize size)
{
#ifdef WIN32
	wchar_t dir_16 [MAX_PATH];
	/* yes, applications shouldn't store data there, but so does GIMP :)*/
	SHGetSpecialFolderPathW(0, dir_16, CSIDL_PROFILE, 0);
	conv_utf_16_to_8(dir_16,dir,FILE_MAX);


#else
	const char* home = getenv("HOME");
	if (!home) return 0;
	BLI_strncpy(dir, home, FILE_MAX);
#endif
	switch(size) {
		case THB_NORMAL:
			strcat(dir, "/.thumbnails/normal/");
			break;
		case THB_LARGE:
			strcat(dir, "/.thumbnails/large/");
			break;
		case THB_FAIL:
			strcat(dir, "/.thumbnails/fail/blender/");
			break;
		default:
			return 0; /* unknown size */
	}
	return 1;
}

/** ----- begin of adapted code from glib ---
 * The following code is adapted from function g_escape_uri_string from the gnome glib
 * Source: http://svn.gnome.org/viewcvs/glib/trunk/glib/gconvert.c?view=markup
 * released under the Gnu General Public License.
 */
typedef enum {
  UNSAFE_ALL        = 0x1,  /* Escape all unsafe characters   */
  UNSAFE_ALLOW_PLUS = 0x2,  /* Allows '+'  */
  UNSAFE_PATH       = 0x8,  /* Allows '/', '&', '=', ':', '@', '+', '$' and ',' */
  UNSAFE_HOST       = 0x10, /* Allows '/' and ':' and '@' */
  UNSAFE_SLASHES    = 0x20  /* Allows all characters except for '/' and '%' */
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
static void escape_uri_string (const char *string, char* escaped_string, int len,UnsafeCharacterSet mask)
{
#define ACCEPTABLE(a) ((a)>=32 && (a)<128 && (acceptable[(a)-32] & use_mask))

	const char *p;
	char *q;
	int c;
	UnsafeCharacterSet use_mask;
	use_mask = mask;

	for (q = escaped_string, p = string; (*p != '\0') && len; p++) {
		c = (unsigned char) *p;
		len--;

		if (!ACCEPTABLE (c)) {
			*q++ = '%'; /* means hex coming */
			*q++ = hex[c >> 4];
			*q++ = hex[c & 15];
		}
		else {
			*q++ = *p;
		}
	}
  
	*q = '\0';
}

static void to_hex_char(char* hexbytes, const unsigned char* bytes, int len)
{
	const unsigned char *p;
	char *q;

	for (q = hexbytes, p = bytes; len; p++) {
		const unsigned char c = (unsigned char) *p;
		len--;
		*q++ = hex[c >> 4];
		*q++ = hex[c & 15];
	}
}

/** ----- end of adapted code from glib --- */

static int uri_from_filename(const char *path, char *uri)
{
	char orig_uri[URI_MAX];	
	const char* dirstart = path;
	
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
#else
	BLI_strncpy(orig_uri, "file://", FILE_MAX);
#endif
	strcat(orig_uri, dirstart);
	BLI_char_switch(orig_uri, '\\', '/');
	
#ifdef WITH_ICONV
	{
		char uri_utf8[FILE_MAX*3+8];
		escape_uri_string(orig_uri, uri_utf8, FILE_MAX*3+8, UNSAFE_PATH);
		BLI_string_to_utf8(uri_utf8, uri, NULL);
	}
#else 
	escape_uri_string(orig_uri, uri, FILE_MAX*3+8, UNSAFE_PATH);
#endif
	return 1;
}

static void thumbname_from_uri(const char* uri, char* thumb, const int thumb_len)
{
	char hexdigest[33];
	unsigned char digest[16];

	md5_buffer( uri, strlen(uri), digest);
	hexdigest[0] = '\0';
	to_hex_char(hexdigest, digest, 16);
	hexdigest[32] = '\0';
	BLI_snprintf(thumb, thumb_len, "%s.png", hexdigest);
}

static int thumbpath_from_uri(const char* uri, char* path, const int path_len, ThumbSize size)
{
	char tmppath[FILE_MAX];
	int rv = 0;

	if (get_thumb_dir(tmppath, size)) {
		char thumb[40];
		thumbname_from_uri(uri, thumb, sizeof(thumb));
		BLI_snprintf(path, path_len, "%s%s", tmppath, thumb);
		rv = 1;
	}
	return rv;
}

void IMB_thumb_makedirs(void)
{
	char tpath[FILE_MAX];
	if (get_thumb_dir(tpath, THB_NORMAL)) {
		BLI_dir_create_recursive(tpath);
	}
	if (get_thumb_dir(tpath, THB_FAIL)) {
		BLI_dir_create_recursive(tpath);
	}
}

/* create thumbnail for file and returns new imbuf for thumbnail */
ImBuf* IMB_thumb_create(const char* path, ThumbSize size, ThumbSource source, ImBuf *img)
{
	char uri[URI_MAX]= "";
	char desc[URI_MAX+22];
	char tpath[FILE_MAX];
	char tdir[FILE_MAX];
	char temp[FILE_MAX];
	char mtime[40]= "0"; /* in case we can't stat the file */
	char cwidth[40]= "0"; /* in case images have no data */
	char cheight[40]= "0";
	char thumb[40];
	short tsize = 128;
	short ex, ey;
	float scaledx, scaledy;	
	struct stat info;

	switch(size) {
		case THB_NORMAL:
			tsize = 128;
			break;
		case THB_LARGE:
			tsize = 256;
			break;
		case THB_FAIL:
			tsize = 1;
			break;
		default:
			return NULL; /* unknown size */
	}

	/* exception, skip images over 100mb */
	if (source == THB_SOURCE_IMAGE) {
		const size_t size= BLI_file_size(path);
		if (size != -1 && size > THUMB_SIZE_MAX) {
			// printf("file too big: %d, skipping %s\n", (int)size, path);
			return NULL;
		}
	}

	uri_from_filename(path, uri);
	thumbname_from_uri(uri, thumb, sizeof(thumb));
	if (get_thumb_dir(tdir, size)) {
		BLI_snprintf(tpath, FILE_MAX, "%s%s", tdir, thumb);
		thumb[8] = '\0'; /* shorten for tempname, not needed anymore */
		BLI_snprintf(temp, FILE_MAX, "%sblender_%d_%s.png", tdir, abs(getpid()), thumb);
		if (BLI_path_ncmp(path, tdir, sizeof(tdir)) == 0) {
			return NULL;
		}
		if (size == THB_FAIL) {
			img = IMB_allocImBuf(1,1,32, IB_rect | IB_metadata);
			if (!img) return NULL;
		}
		else {
			if (THB_SOURCE_IMAGE == source || THB_SOURCE_BLEND == source) {
				
				/* only load if we didnt give an image */
				if (img==NULL) {
					if (THB_SOURCE_BLEND == source) {
						img = IMB_loadblend_thumb(path);
					}
					else {
						img = IMB_loadiffname(path, IB_rect | IB_metadata);
					}
				}

				if (img != NULL) {
					stat(path, &info);
					BLI_snprintf(mtime, sizeof(mtime), "%ld", (long int)info.st_mtime);
					BLI_snprintf(cwidth, sizeof(cwidth), "%d", img->x);
					BLI_snprintf(cheight, sizeof(cheight), "%d", img->y);
				}
			}
			else if (THB_SOURCE_MOVIE == source) {
				struct anim * anim = NULL;
				anim = IMB_open_anim(path, IB_rect | IB_metadata, 0);
				if (anim != NULL) {
					img = IMB_anim_absolute(anim, 0, IMB_TC_NONE, IMB_PROXY_NONE);
					if (img == NULL) {
						printf("not an anim; %s\n", path);
					}
					else {
						IMB_freeImBuf(img);
						img = IMB_anim_previewframe(anim);						
					}
					IMB_free_anim(anim);
				}
				stat(path, &info);
				BLI_snprintf(mtime, sizeof(mtime), "%ld", (long int)info.st_mtime);
			}
			if (!img) return NULL;

			if (img->x > img->y) {
				scaledx = (float)tsize;
				scaledy =  ( (float)img->y/(float)img->x )*tsize;
			}
			else {
				scaledy = (float)tsize;
				scaledx =  ( (float)img->x/(float)img->y )*tsize;
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
		IMB_metadata_change_field(img, "Description", desc);
		IMB_metadata_change_field(img, "Software", "Blender");
		IMB_metadata_change_field(img, "Thumb::URI", uri);
		IMB_metadata_change_field(img, "Thumb::MTime", mtime);
		if (THB_SOURCE_IMAGE == source) {
			IMB_metadata_change_field(img, "Thumb::Image::Width", cwidth);
			IMB_metadata_change_field(img, "Thumb::Image::Height", cheight);
		}
		img->ftype = PNG;
		img->planes = 32;
		if (IMB_saveiff(img, temp, IB_rect | IB_metadata)) {
#ifndef WIN32
			chmod(temp, S_IRUSR | S_IWUSR);
#endif	
			BLI_rename(temp, tpath);
		}

		return img;
	}
	return img;
}

/* read thumbnail for file and returns new imbuf for thumbnail */
ImBuf* IMB_thumb_read(const char* path, ThumbSize size)
{
	char thumb[FILE_MAX];
	char uri[FILE_MAX*3+8];
	ImBuf *img = NULL;

	if (!uri_from_filename(path,uri)) {
		return NULL;
	}
	if (thumbpath_from_uri(uri, thumb, sizeof(thumb), size)) {		
		img = IMB_loadiffname(thumb, IB_rect | IB_metadata);
	}

	return img;
}

/* delete all thumbs for the file */
void IMB_thumb_delete(const char* path, ThumbSize size)
{
	char thumb[FILE_MAX];
	char uri[FILE_MAX*3+8];

	if (!uri_from_filename(path ,uri)) {
		return;
	}
	if (thumbpath_from_uri(uri, thumb, sizeof(thumb), size)) {
		if (BLI_path_ncmp(path, thumb, sizeof(thumb)) == 0) {
			return;
		}
		if (BLI_exists(thumb)) {
			BLI_delete(thumb, 0, 0);
		}
	}
}


/* create the thumb if necessary and manage failed and old thumbs */
ImBuf* IMB_thumb_manage(const char* path, ThumbSize size, ThumbSource source)
{
	char thumb[FILE_MAX];
	char uri[FILE_MAX*3+8];
	struct stat st;
	ImBuf* img = NULL;
	
	if (stat(path, &st)) {
		return NULL;
	}	
	if (!uri_from_filename(path,uri)) {
		return NULL;
	}
	if (thumbpath_from_uri(uri, thumb, sizeof(thumb), THB_FAIL)) {
		/* failure thumb exists, don't try recreating */
		if (BLI_exists(thumb)) {
			return NULL;
		}
	}

	if (thumbpath_from_uri(uri, thumb, sizeof(thumb), size)) {
		if (BLI_path_ncmp(path, thumb, sizeof(thumb)) == 0) {
			img = IMB_loadiffname(path, IB_rect);
		}
		else {
			img = IMB_loadiffname(thumb, IB_rect | IB_metadata);
			if (img) {
				char mtime[40];
				if (!IMB_metadata_get_field(img, "Thumb::MTime", mtime, 40)) {
					/* illegal thumb, forget it! */
					IMB_freeImBuf(img);
					img = NULL;
				}
				else {
					time_t t = atol(mtime);
					if (st.st_mtime != t) {
						/* recreate all thumbs */
						IMB_freeImBuf(img);
						img = NULL;
						IMB_thumb_delete(path, THB_NORMAL);
						IMB_thumb_delete(path, THB_LARGE);
						IMB_thumb_delete(path, THB_FAIL);
						img = IMB_thumb_create(path, size, source, NULL);
						if (!img) {
							/* thumb creation failed, write fail thumb */
							img = IMB_thumb_create(path, THB_FAIL, source, NULL);
							if (img) {
								/* we don't need failed thumb anymore */
								IMB_freeImBuf(img);
								img = NULL;
							}
						}
					}
				}
			}
			else {
				img = IMB_thumb_create(path, size, source, NULL);
				if (!img) {
					/* thumb creation failed, write fail thumb */
					img = IMB_thumb_create(path, THB_FAIL, source, NULL);
					if (img) {
						/* we don't need failed thumb anymore */
						IMB_freeImBuf(img);
						img = NULL;
					}
				}
			}
		}
	}

	return img;
}



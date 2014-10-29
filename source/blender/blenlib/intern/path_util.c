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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * various string, file, list operations.
 */

/** \file blender/blenlib/intern/path_util.c
 *  \ingroup bli
 */

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#include "DNA_listBase.h"

#include "BLI_utildefines.h"
#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_fnmatch.h"

#include "../blenkernel/BKE_blender.h"  /* BLENDER_VERSION, bad level include (no function call) */

#include "GHOST_Path-api.h"

#include "MEM_guardedalloc.h"

#ifdef WIN32
#  include "utf_winfunc.h"
#  include "utfconv.h"
#  include <io.h>
#  ifdef _WIN32_IE
#    undef _WIN32_IE
#  endif
#  define _WIN32_IE 0x0501
#  include <windows.h>
#  include <shlobj.h>
#  include "BLI_winstuff.h"
#else /* non windows */
#  ifdef WITH_BINRELOC
#    include "binreloc.h"
#  endif
#  include <unistd.h>  /* mkdtemp on OSX (and probably all *BSD?), not worth making specific check for this OS. */
#endif /* WIN32 */

/* local */
#define UNIQUE_NAME_MAX 128

static char bprogname[FILE_MAX];    /* full path to program executable */
static char bprogdir[FILE_MAX];     /* full path to directory in which executable is located */
static char btempdir_base[FILE_MAX];          /* persistent temporary directory */
static char btempdir_session[FILE_MAX] = "";  /* volatile temporary directory */

/* implementation */

/**
 * Looks for a sequence of decimal digits in string, preceding any filename extension,
 * returning the integer value if found, or 0 if not.
 *
 * \param string  String to scan.
 * \param head  Optional area to return copy of part of string prior to digits, or before dot if no digits.
 * \param tail  Optional area to return copy of part of string following digits, or from dot if no digits.
 * \param numlen  Optional to return number of digits found.
 */
int BLI_stringdec(const char *string, char *head, char *tail, unsigned short *numlen)
{
	unsigned int nums = 0, nume = 0;
	int i;
	bool found_digit = false;
	const char * const lslash = BLI_last_slash(string);
	const unsigned int string_len = strlen(string);
	const unsigned int lslash_len = lslash != NULL ? (int)(lslash - string) : 0;
	unsigned int name_end = string_len;

	while (name_end > lslash_len && string[--name_end] != '.') {} /* name ends at dot if present */
	if (name_end == lslash_len && string[name_end] != '.') name_end = string_len;

	for (i = name_end - 1; i >= (int)lslash_len; i--) {
		if (isdigit(string[i])) {
			if (found_digit) {
				nums = i;
			}
			else {
				nume = i;
				nums = i;
				found_digit = true;
			}
		}
		else {
			if (found_digit) break;
		}
	}

	if (found_digit) {
		if (tail) strcpy(tail, &string[nume + 1]);
		if (head) {
			strcpy(head, string);
			head[nums] = 0;
		}
		if (numlen) *numlen = nume - nums + 1;
		return ((int)atoi(&(string[nums])));
	}
	else {
		if (tail) strcpy(tail, string + name_end);
		if (head) {
			/* name_end points to last character of head,
			 * make it +1 so null-terminator is nicely placed
			 */
			BLI_strncpy(head, string, name_end + 1);
		}
		if (numlen) *numlen = 0;
		return 0;
	}
}


/**
 * Returns in area pointed to by string a string of the form "<head><pic><tail>", where pic
 * is formatted as numlen digits with leading zeroes.
 */
void BLI_stringenc(char *string, const char *head, const char *tail, unsigned short numlen, int pic)
{
	sprintf(string, "%s%.*d%s", head, numlen, MAX2(0, pic), tail);
}

/**
 * Looks for a numeric suffix preceded by delim character on the end of
 * name, puts preceding part into *left and value of suffix into *nr.
 * Returns the length of *left.
 *
 * Foo.001 -> "Foo", 1
 * Returning the length of "Foo"
 *
 * \param left  Where to return copy of part preceding delim
 * \param nr  Where to return value of numeric suffix
 * \param name  String to split
 * \param delim  Delimiter character
 * \return  Length of \a left
 */
int BLI_split_name_num(char *left, int *nr, const char *name, const char delim)
{
	const int name_len = strlen(name);

	*nr = 0;
	memcpy(left, name, (name_len + 1) * sizeof(char));

	/* name doesn't end with a delimiter "foo." */
	if ((name_len > 1 && name[name_len - 1] == delim) == 0) {
		int a = name_len;
		while (a--) {
			if (name[a] == delim) {
				left[a] = '\0';  /* truncate left part here */
				*nr = atol(name + a + 1);
				/* casting down to an int, can overflow for large numbers */
				if (*nr < 0)
					*nr = 0;
				return a;
			}
			else if (isdigit(name[a]) == 0) {
				/* non-numeric suffix - give up */
				break;
			}
		}
	}

	return name_len;
}

/**
 * Looks for a string of digits within name (using BLI_stringdec) and adjusts it by add.
 */
void BLI_newname(char *name, int add)
{
	char head[UNIQUE_NAME_MAX], tail[UNIQUE_NAME_MAX];
	int pic;
	unsigned short digits;
	
	pic = BLI_stringdec(name, head, tail, &digits);
	
	/* are we going from 100 -> 99 or from 10 -> 9 */
	if (add < 0 && digits < 4 && digits > 0) {
		int i, exp;
		exp = 1;
		for (i = digits; i > 1; i--) exp *= 10;
		if (pic >= exp && (pic + add) < exp) digits--;
	}
	
	pic += add;
	
	if (digits == 4 && pic < 0) pic = 0;
	BLI_stringenc(name, head, tail, digits, pic);
}

/**
 * Ensures name is unique (according to criteria specified by caller in unique_check callback),
 * incrementing its numeric suffix as necessary. Returns true if name had to be adjusted.
 *
 * \param unique_check  Return true if name is not unique
 * \param arg  Additional arg to unique_check--meaning is up to caller
 * \param defname  To initialize name if latter is empty
 * \param delim  Delimits numeric suffix in name
 * \param name  Name to be ensured unique
 * \param name_len  Maximum length of name area
 * \return true if there if the name was changed
 */
bool BLI_uniquename_cb(bool (*unique_check)(void *arg, const char *name),
                       void *arg, const char *defname, char delim, char *name, int name_len)
{
	if (name[0] == '\0') {
		BLI_strncpy(name, defname, name_len);
	}

	if (unique_check(arg, name)) {
		char numstr[16];
		char tempname[UNIQUE_NAME_MAX];
		char left[UNIQUE_NAME_MAX];
		int number;
		int len = BLI_split_name_num(left, &number, name, delim);
		do {
			const int numlen = BLI_snprintf(numstr, sizeof(numstr), "%c%03d", delim, ++number);

			/* highly unlikely the string only has enough room for the number
			 * but support anyway */
			if ((len == 0) || (numlen >= name_len)) {
				/* number is know not to be utf-8 */
				BLI_strncpy(tempname, numstr, name_len);
			}
			else {
				char *tempname_buf;
				tempname[0] = '\0';
				tempname_buf = BLI_strncat_utf8(tempname, left, name_len - numlen);
				memcpy(tempname_buf, numstr, numlen + 1);
			}
		} while (unique_check(arg, tempname));

		BLI_strncpy(name, tempname, name_len);
		
		return true;
	}
	
	return false;
}

/* little helper macro for BLI_uniquename */
#ifndef GIVE_STRADDR
#  define GIVE_STRADDR(data, offset) ( ((char *)data) + offset)
#endif

/* Generic function to set a unique name. It is only designed to be used in situations
 * where the name is part of the struct, and also that the name is at most UNIQUE_NAME_MAX chars long.
 * 
 * For places where this is used, see constraint.c for example...
 *
 *  name_offs: should be calculated using offsetof(structname, membername) macro from stddef.h
 *  len: maximum length of string (to prevent overflows, etc.)
 *  defname: the name that should be used by default if none is specified already
 *  delim: the character which acts as a delimiter between parts of the name
 */
static bool uniquename_find_dupe(ListBase *list, void *vlink, const char *name, int name_offs)
{
	Link *link;

	for (link = list->first; link; link = link->next) {
		if (link != vlink) {
			if (STREQ(GIVE_STRADDR(link, name_offs), name)) {
				return true;
			}
		}
	}

	return false;
}

static bool uniquename_unique_check(void *arg, const char *name)
{
	struct {ListBase *lb; void *vlink; int name_offs; } *data = arg;
	return uniquename_find_dupe(data->lb, data->vlink, name, data->name_offs);
}

/**
 * Ensures that the specified block has a unique name within the containing list,
 * incrementing its numeric suffix as necessary.
 *
 * \param list  List containing the block
 * \param vlink  The block to check the name for
 * \param defname  To initialize block name if latter is empty
 * \param delim  Delimits numeric suffix in name
 * \param name_offs  Offset of name within block structure
 * \param name_len  Maximum length of name area
 */
void BLI_uniquename(ListBase *list, void *vlink, const char *defname, char delim, int name_offs, int name_len)
{
	struct {ListBase *lb; void *vlink; int name_offs; } data;
	data.lb = list;
	data.vlink = vlink;
	data.name_offs = name_offs;

	assert((name_len > 1) && (name_len <= UNIQUE_NAME_MAX));

	/* See if we are given an empty string */
	if (ELEM(NULL, vlink, defname))
		return;

	BLI_uniquename_cb(uniquename_unique_check, &data, defname, delim, GIVE_STRADDR(vlink, name_offs), name_len);
}

static int BLI_path_unc_prefix_len(const char *path); /* defined below in same file */

/* ******************** string encoding ***************** */

/* This is quite an ugly function... its purpose is to
 * take the dir name, make it absolute, and clean it up, replacing
 * excess file entry stuff (like /tmp/../tmp/../)
 * note that dir isn't protected for max string names... 
 * 
 * If relbase is NULL then its ignored
 */

void BLI_cleanup_path(const char *relabase, char *path)
{
	ptrdiff_t a;
	char *start, *eind;
	if (relabase) {
		BLI_path_abs(path, relabase);
	}
	else {
		if (path[0] == '/' && path[1] == '/') {
			if (path[2] == '\0') {
				return; /* path is "//" - cant clean it */
			}
			path = path + 2;  /* leave the initial "//" untouched */
		}
	}
	
	/* Note
	 *   memmove(start, eind, strlen(eind) + 1);
	 * is the same as
	 *   strcpy( start, eind ); 
	 * except strcpy should not be used because there is overlap,
	 * so use memmove's slightly more obscure syntax - Campbell
	 */
	
#ifdef WIN32
	while ( (start = strstr(path, "\\..\\")) ) {
		eind = start + strlen("\\..\\") - 1;
		a = start - path - 1;
		while (a > 0) {
			if (path[a] == '\\') break;
			a--;
		}
		if (a < 0) {
			break;
		}
		else {
			memmove(path + a, eind, strlen(eind) + 1);
		}
	}

	while ( (start = strstr(path, "\\.\\")) ) {
		eind = start + strlen("\\.\\") - 1;
		memmove(start, eind, strlen(eind) + 1);
	}

	/* remove two consecutive backslashes, but skip the UNC prefix,
	 * which needs to be preserved */
	while ( (start = strstr(path + BLI_path_unc_prefix_len(path), "\\\\")) ) {
		eind = start + strlen("\\\\") - 1;
		memmove(start, eind, strlen(eind) + 1);
	}
#else
	while ( (start = strstr(path, "/../")) ) {
		a = start - path - 1;
		if (a > 0) {
			/* <prefix>/<parent>/../<postfix> => <prefix>/<postfix> */
			eind = start + (4 - 1) /* strlen("/../") - 1 */; /* strip "/.." and keep last "/" */
			while (a > 0 && path[a] != '/') { /* find start of <parent> */
				a--;
			}
			memmove(path + a, eind, strlen(eind) + 1);
		}
		else {
			/* support for odd paths: eg /../home/me --> /home/me
			 * this is a valid path in blender but we cant handle this the usual way below
			 * simply strip this prefix then evaluate the path as usual.
			 * pythons os.path.normpath() does this */

			/* Note: previous version of following call used an offset of 3 instead of 4,
			 * which meant that the "/../home/me" example actually became "home/me".
			 * Using offset of 3 gives behaviour consistent with the abovementioned
			 * Python routine. */
			memmove(path, path + 3, strlen(path + 3) + 1);
		}
	}

	while ( (start = strstr(path, "/./")) ) {
		eind = start + (3 - 1) /* strlen("/./") - 1 */;
		memmove(start, eind, strlen(eind) + 1);
	}

	while ( (start = strstr(path, "//")) ) {
		eind = start + (2 - 1) /* strlen("//") - 1 */;
		memmove(start, eind, strlen(eind) + 1);
	}
#endif
}

void BLI_cleanup_dir(const char *relabase, char *dir)
{
	BLI_cleanup_path(relabase, dir);
	BLI_add_slash(dir);

}

void BLI_cleanup_file(const char *relabase, char *path)
{
	BLI_cleanup_path(relabase, path);
	BLI_del_slash(path);
}

/**
 * Does path begin with the special "//" prefix that Blender uses to indicate
 * a path relative to the .blend file.
 */
bool BLI_path_is_rel(const char *path)
{
	return path[0] == '/' && path[1] == '/';
}

/* return true if the path is a UNC share */
bool BLI_path_is_unc(const char *name)
{
	return name[0] == '\\' && name[1] == '\\';
}

/**
 * Returns the length of the identifying prefix
 * of a UNC path which can start with '\\' (short version)
 * or '\\?\' (long version)
 * If the path is not a UNC path, return 0
 */
static int BLI_path_unc_prefix_len(const char *path)
{
	if (BLI_path_is_unc(path)) {
		if ((path[2] == '?') && (path[3] == '\\') ) {
			/* we assume long UNC path like \\?\server\share\folder etc... */
			return 4;
		}
		else {
			return 2;
		}
	}

	return 0;
}

#if defined(WIN32)

/* return true if the path is absolute ie starts with a drive specifier (eg A:\) or is a UNC path */
static bool BLI_path_is_abs(const char *name)
{
	return (name[1] == ':' && (name[2] == '\\' || name[2] == '/') ) || BLI_path_is_unc(name);
}

static wchar_t *next_slash(wchar_t *path)
{
	wchar_t *slash = path;
	while (*slash && *slash != L'\\') slash++;
	return slash;
}

/* adds a slash if the unc path points sto a share */
static void BLI_path_add_slash_to_share(wchar_t *uncpath)
{
	wchar_t *slash_after_server = next_slash(uncpath + 2);
	if (*slash_after_server) {
		wchar_t *slash_after_share = next_slash(slash_after_server + 1);
		if (!(*slash_after_share)) {
			slash_after_share[0] = L'\\';
			slash_after_share[1] = L'\0';
		}
	}
}

static void BLI_path_unc_to_short(wchar_t *unc)
{
	wchar_t tmp[PATH_MAX];

	int len = wcslen(unc);
	int copy_start = 0;
	/* convert:
	 *    \\?\UNC\server\share\folder\... to \\server\share\folder\...
	 *    \\?\C:\ to C:\ and \\?\C:\folder\... to C:\folder\...
	 */
	if ((len > 3) &&
	    (unc[0] ==  L'\\') &&
	    (unc[1] ==  L'\\') &&
	    (unc[2] ==  L'?') &&
	    ((unc[3] ==  L'\\') || (unc[3] ==  L'/')))
	{
		if ((len > 5) && (unc[5] ==  L':')) {
			wcsncpy(tmp, unc + 4, len - 4);
			tmp[len - 4] = L'\0';
			wcscpy(unc, tmp);
		}
		else if ((len > 7) && (wcsncmp(&unc[4], L"UNC", 3) == 0) &&
		         ((unc[7] ==  L'\\') || (unc[7] ==  L'/')))
		{
			tmp[0] = L'\\';
			tmp[1] = L'\\';
			wcsncpy(tmp + 2, unc + 8, len - 8);
			tmp[len - 6] = L'\0';
			wcscpy(unc, tmp);
		}
	}
}

void BLI_cleanup_unc(char *path, int maxlen)
{
	wchar_t *tmp_16 = alloc_utf16_from_8(path, 1);
	BLI_cleanup_unc_16(tmp_16);
	conv_utf_16_to_8(tmp_16, path, maxlen);
}

void BLI_cleanup_unc_16(wchar_t *path_16)
{
	BLI_path_unc_to_short(path_16);
	BLI_path_add_slash_to_share(path_16);
}
#endif

/**
 * Replaces *file with a relative version (prefixed by "//") such that BLI_path_abs, given
 * the same *relfile, will convert it back to its original value.
 */
void BLI_path_rel(char *file, const char *relfile)
{
	const char *lslash;
	char temp[FILE_MAX];
	char res[FILE_MAX];
	
	/* if file is already relative, bail out */
	if (BLI_path_is_rel(file)) {
		return;
	}
	
	/* also bail out if relative path is not set */
	if (relfile[0] == '\0') {
		return;
	}

#ifdef WIN32
	if (BLI_strnlen(relfile, 3) > 2 && !BLI_path_is_abs(relfile)) {
		char *ptemp;
		/* fix missing volume name in relative base,
		 * can happen with old recent-files.txt files */
		get_default_root(temp);
		ptemp = &temp[2];
		if (relfile[0] != '\\' && relfile[0] != '/') {
			ptemp++;
		}
		BLI_strncpy(ptemp, relfile, FILE_MAX - 3);
	}
	else {
		BLI_strncpy(temp, relfile, FILE_MAX);
	}

	if (BLI_strnlen(file, 3) > 2) {
		bool is_unc = BLI_path_is_unc(file);

		/* Ensure paths are both UNC paths or are both drives */
		if (BLI_path_is_unc(temp) != is_unc) {
			return;
		}

		/* Ensure both UNC paths are on the same share */
		if (is_unc) {
			int off;
			int slash = 0;
			for (off = 0; temp[off] && slash < 4; off++) {
				if (temp[off] != file[off])
					return;

				if (temp[off] == '\\')
					slash++;
			}
		}
		else if (temp[1] == ':' && file[1] == ':' && temp[0] != file[0]) {
			return;
		}
	}
#else
	BLI_strncpy(temp, relfile, FILE_MAX);
#endif

	BLI_char_switch(temp + BLI_path_unc_prefix_len(temp), '\\', '/');
	BLI_char_switch(file + BLI_path_unc_prefix_len(file), '\\', '/');
	
	/* remove /./ which confuse the following slash counting... */
	BLI_cleanup_path(NULL, file);
	BLI_cleanup_path(NULL, temp);
	
	/* the last slash in the file indicates where the path part ends */
	lslash = BLI_last_slash(temp);

	if (lslash) {
		/* find the prefix of the filename that is equal for both filenames.
		 * This is replaced by the two slashes at the beginning */
		const char *p = temp;
		const char *q = file;
		char *r = res;

#ifdef WIN32
		while (tolower(*p) == tolower(*q))
#else
		while (*p == *q)
#endif
		{
			p++;
			q++;

			/* don't search beyond the end of the string
			 * in the rare case they match */
			if ((*p == '\0') || (*q == '\0')) {
				break;
			}
		}

		/* we might have passed the slash when the beginning of a dir matches 
		 * so we rewind. Only check on the actual filename
		 */
		if (*q != '/') {
			while ( (q >= file) && (*q != '/') ) { --q; --p; }
		}
		else if (*p != '/') {
			while ( (p >= temp) && (*p != '/') ) { --p; --q; }
		}
		
		r += BLI_strcpy_rlen(r, "//");

		/* p now points to the slash that is at the beginning of the part
		 * where the path is different from the relative path. 
		 * We count the number of directories we need to go up in the
		 * hierarchy to arrive at the common 'prefix' of the path
		 */
		if (p < temp) p = temp;
		while (p && p < lslash) {
			if (*p == '/') {
				r += BLI_strcpy_rlen(r, "../");
			}
			p++;
		}

		/* don't copy the slash at the beginning */
		r += BLI_strcpy_rlen(r, q + 1);
		
#ifdef  WIN32
		BLI_char_switch(res + 2, '/', '\\');
#endif
		strcpy(file, res);
	}
}

/**
 * Appends a suffix to the string, fitting it before the extension
 *
 * string = Foo.png, suffix = 123, separator = _
 * Foo.png -> Foo_123.png
 *
 * \param string  original (and final) string
 * \param maxlen  Maximum length of string
 * \param suffix  String to append to the original string
 * \param sep Optional separator character
 * \return  true if succeeded
 */
bool BLI_path_suffix(char *string, size_t maxlen, const char *suffix, const char *sep)
{
	const size_t string_len = strlen(string);
	const size_t suffix_len = strlen(suffix);
	const size_t sep_len = strlen(sep);
	ssize_t a;
	char extension[FILE_MAX];
	bool has_extension = false;

	if (string_len + sep_len + suffix_len >= maxlen)
		return false;

	for (a = string_len - 1; a >= 0; a--) {
		if (string[a] == '.') {
			has_extension = true;
			break;
		}
		else if (ELEM(string[a], '/', '\\')) {
			break;
		}
	}

	if (!has_extension)
		a = string_len;

	BLI_strncpy(extension, string + a, sizeof(extension));
	sprintf(string + a, "%s%s%s", sep, suffix, extension);
	return true;
}

/**
 * Replaces path with the path of its parent directory, returning true if
 * it was able to find a parent directory within the pathname.
 */
bool BLI_parent_dir(char *path)
{
	const char parent_dir[] = {'.', '.', SEP, '\0'}; /* "../" or "..\\" */
	char tmp[FILE_MAX + 4];

	BLI_join_dirfile(tmp, sizeof(tmp), path, parent_dir);
	BLI_cleanup_dir(NULL, tmp); /* does all the work of normalizing the path for us */

	if (!BLI_testextensie(tmp, parent_dir)) {
		BLI_strncpy(path, tmp, sizeof(tmp));
		return true;
	}
	else {
		return false;
	}
}

/**
 * Looks for a sequence of "#" characters in the last slash-separated component of *path,
 * returning the indexes of the first and one past the last character in the sequence in
 * *char_start and *char_end respectively. Returns true if such a sequence was found.
 */
static bool stringframe_chars(const char *path, int *char_start, int *char_end)
{
	unsigned int ch_sta, ch_end, i;
	/* Insert current frame: file### -> file001 */
	ch_sta = ch_end = 0;
	for (i = 0; path[i] != '\0'; i++) {
		if (path[i] == '\\' || path[i] == '/') {
			ch_end = 0; /* this is a directory name, don't use any hashes we found */
		}
		else if (path[i] == '#') {
			ch_sta = i;
			ch_end = ch_sta + 1;
			while (path[ch_end] == '#') {
				ch_end++;
			}
			i = ch_end - 1; /* keep searching */
			
			/* don't break, there may be a slash after this that invalidates the previous #'s */
		}
	}

	if (ch_end) {
		*char_start = ch_sta;
		*char_end = ch_end;
		return true;
	}
	else {
		*char_start = -1;
		*char_end = -1;
		return false;
	}
}

/**
 * Ensure *path contains at least one "#" character in its last slash-separated
 * component, appending one digits long if not.
 */
static void ensure_digits(char *path, int digits)
{
	char *file = (char *)BLI_last_slash(path);

	if (file == NULL)
		file = path;

	if (strrchr(file, '#') == NULL) {
		int len = strlen(file);

		while (digits--) {
			file[len++] = '#';
		}
		file[len] = '\0';
	}
}

/**
 * Replaces "#" character sequence in last slash-separated component of *path
 * with frame as decimal integer, with leading zeroes as necessary, to make digits digits.
 */
bool BLI_path_frame(char *path, int frame, int digits)
{
	int ch_sta, ch_end;

	if (digits)
		ensure_digits(path, digits);

	if (stringframe_chars(path, &ch_sta, &ch_end)) { /* warning, ch_end is the last # +1 */
		char tmp[FILE_MAX];
		BLI_snprintf(tmp, sizeof(tmp),
		             "%.*s%.*d%s",
		             ch_sta, path, ch_end - ch_sta, frame, path + ch_end);
		BLI_strncpy(path, tmp, FILE_MAX);
		return true;
	}
	return false;
}

/**
 * Replaces "#" character sequence in last slash-separated component of *path
 * with sta and end as decimal integers, with leading zeroes as necessary, to make digits
 * digits each, with a hyphen in-between.
 */
bool BLI_path_frame_range(char *path, int sta, int end, int digits)
{
	int ch_sta, ch_end;

	if (digits)
		ensure_digits(path, digits);

	if (stringframe_chars(path, &ch_sta, &ch_end)) { /* warning, ch_end is the last # +1 */
		char tmp[FILE_MAX];
		BLI_snprintf(tmp, sizeof(tmp),
		             "%.*s%.*d-%.*d%s",
		             ch_sta, path, ch_end - ch_sta, sta, ch_end - ch_sta, end, path + ch_end);
		BLI_strncpy(path, tmp, FILE_MAX);
		return true;
	}
	return false;
}

/**
 * Check if we have '#' chars, usable for #BLI_path_frame, #BLI_path_frame_range
 */
bool BLI_path_frame_check_chars(const char *path)
{
	int ch_sta, ch_end;  /* dummy args */
	return stringframe_chars(path, &ch_sta, &ch_end);
}

/**
 * If path begins with "//", strips that and replaces it with basepath directory. Also converts
 * a drive-letter prefix to something more sensible if this is a non-drive-letter-based system.
 * Returns true if "//" prefix expansion was done.
 */
bool BLI_path_abs(char *path, const char *basepath)
{
	const bool wasrelative = BLI_path_is_rel(path);
	char tmp[FILE_MAX];
	char base[FILE_MAX];
#ifdef WIN32

	/* without this: "" --> "C:\" */
	if (*path == '\0') {
		return wasrelative;
	}

	/* we are checking here if we have an absolute path that is not in the current
	 * blend file as a lib main - we are basically checking for the case that a 
	 * UNIX root '/' is passed.
	 */
	if (!wasrelative && !BLI_path_is_abs(path)) {
		char *p = path;
		get_default_root(tmp);
		// get rid of the slashes at the beginning of the path
		while (*p == '\\' || *p == '/') {
			p++;
		}
		strcat(tmp, p);
	}
	else {
		BLI_strncpy(tmp, path, FILE_MAX);
	}
#else
	BLI_strncpy(tmp, path, sizeof(tmp));
	
	/* Check for loading a windows path on a posix system
	 * in this case, there is no use in trying C:/ since it 
	 * will never exist on a unix os.
	 * 
	 * Add a / prefix and lowercase the driveletter, remove the :
	 * C:\foo.JPG -> /c/foo.JPG */
	
	if (isalpha(tmp[0]) && tmp[1] == ':' && (tmp[2] == '\\' || tmp[2] == '/') ) {
		tmp[1] = tolower(tmp[0]); /* replace ':' with driveletter */
		tmp[0] = '/'; 
		/* '\' the slash will be converted later */
	}
	
#endif

	/* push slashes into unix mode - strings entering this part are
	 * potentially messed up: having both back- and forward slashes.
	 * Here we push into one conform direction, and at the end we
	 * push them into the system specific dir. This ensures uniformity
	 * of paths and solving some problems (and prevent potential future
	 * ones) -jesterKing.
	 * For UNC paths the first characters containing the UNC prefix
	 * shouldn't be switched as we need to distinguish them from
	 * paths relative to the .blend file -elubie */
	BLI_char_switch(tmp + BLI_path_unc_prefix_len(tmp), '\\', '/');

	/* Paths starting with // will get the blend file as their base,
	 * this isn't standard in any os but is used in blender all over the place */
	if (wasrelative) {
		const char *lslash;
		BLI_strncpy(base, basepath, sizeof(base));

		/* file component is ignored, so don't bother with the trailing slash */
		BLI_cleanup_path(NULL, base);
		lslash = BLI_last_slash(base);
		BLI_char_switch(base + BLI_path_unc_prefix_len(base), '\\', '/');

		if (lslash) {
			const int baselen = (int) (lslash - base) + 1;  /* length up to and including last "/" */
			/* use path for temp storage here, we copy back over it right away */
			BLI_strncpy(path, tmp + 2, FILE_MAX);  /* strip "//" */
			
			memcpy(tmp, base, baselen);  /* prefix with base up to last "/" */
			BLI_strncpy(tmp + baselen, path, sizeof(tmp) - baselen);  /* append path after "//" */
			BLI_strncpy(path, tmp, FILE_MAX);  /* return as result */
		}
		else {
			/* base doesn't seem to be a directory--ignore it and just strip "//" prefix on path */
			BLI_strncpy(path, tmp + 2, FILE_MAX);
		}
	}
	else {
		/* base ignored */
		BLI_strncpy(path, tmp, FILE_MAX);
	}

	BLI_cleanup_path(NULL, path);

#ifdef WIN32
	/* skip first two chars, which in case of
	 * absolute path will be drive:/blabla and
	 * in case of relpath //blabla/. So relpath
	 * // will be retained, rest will be nice and
	 * shiny win32 backward slashes :) -jesterKing
	 */
	BLI_char_switch(path + 2, '/', '\\');
#endif
	
	return wasrelative;
}


/**
 * Expands path relative to the current working directory, if it was relative.
 * Returns true if such expansion was done.
 *
 * \note Should only be done with command line paths.
 * this is _not_ something blenders internal paths support like the "//" prefix
 */
bool BLI_path_cwd(char *path)
{
	bool wasrelative = true;
	const int filelen = strlen(path);
	
#ifdef WIN32
	if ((filelen >= 3 && BLI_path_is_abs(path)) || BLI_path_is_unc(path))
		wasrelative = false;
#else
	if (filelen >= 2 && path[0] == '/')
		wasrelative = false;
#endif
	
	if (wasrelative) {
		char cwd[FILE_MAX] = "";
		BLI_current_working_dir(cwd, sizeof(cwd)); /* in case the full path to the blend isn't used */
		
		if (cwd[0] == '\0') {
			printf("Could not get the current working directory - $PWD for an unknown reason.\n");
		}
		else {
			/* uses the blend path relative to cwd important for loading relative linked files.
			 *
			 * cwd should contain c:\ etc on win32 so the relbase can be NULL
			 * relbase being NULL also prevents // being misunderstood as relative to the current
			 * blend file which isn't a feature we want to use in this case since were dealing
			 * with a path from the command line, rather than from inside Blender */

			char origpath[FILE_MAX];
			BLI_strncpy(origpath, path, FILE_MAX);
			
			BLI_make_file_string(NULL, path, cwd, origpath); 
		}
	}
	
	return wasrelative;
}

/**
 * Copies into *last the part of *dir following the second-last slash.
 */
void BLI_getlastdir(const char *dir, char *last, const size_t maxlen)
{
	const char *s = dir;
	const char *lslash = NULL;
	const char *prevslash = NULL;
	while (*s) {
		if ((*s == '\\') || (*s == '/')) {
			prevslash = lslash;
			lslash = s;
		}
		s++;
	}
	if (prevslash) {
		BLI_strncpy(last, prevslash + 1, maxlen);
	}
	else {
		BLI_strncpy(last, dir, maxlen);
	}
}

/* This is now only used to really get the user's default document folder */
/* On Windows I chose the 'Users/<MyUserName>/Documents' since it's used
 * as default location to save documents */
const char *BLI_getDefaultDocumentFolder(void)
{
#ifndef WIN32
	const char * const xdg_documents_dir = getenv("XDG_DOCUMENTS_DIR");

	if (xdg_documents_dir)
		return xdg_documents_dir;

	return getenv("HOME");
#else /* Windows */
	static char documentfolder[MAXPATHLEN];
	HRESULT hResult;

	/* Check for %HOME% env var */
	if (uput_getenv("HOME", documentfolder, MAXPATHLEN)) {
		if (BLI_is_dir(documentfolder)) return documentfolder;
	}
				
	/* add user profile support for WIN 2K / NT.
	 * This is %APPDATA%, which translates to either
	 * %USERPROFILE%\Application Data or since Vista
	 * to %USERPROFILE%\AppData\Roaming
	 */
	hResult = SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, documentfolder);
		
	if (hResult == S_OK) {
		if (BLI_is_dir(documentfolder)) return documentfolder;
	}
		
	return NULL;
#endif /* WIN32 */
}

/* NEW stuff, to be cleaned up when fully migrated */
/* ************************************************************* */
/* ************************************************************* */

// #define PATH_DEBUG

/* returns a formatted representation of the specified version number. Non-reentrant! */
static char *blender_version_decimal(const int ver)
{
	static char version_str[5];
	sprintf(version_str, "%d.%02d", ver / 100, ver % 100);
	return version_str;
}

/**
 * Concatenates path_base, (optional) path_sep and (optional) folder_name into targetpath,
 * returning true if result points to a directory.
 */
static bool test_path(char *targetpath, const char *path_base, const char *path_sep, const char *folder_name)
{
	char tmppath[FILE_MAX];
	
	if (path_sep) BLI_join_dirfile(tmppath, sizeof(tmppath), path_base, path_sep);
	else BLI_strncpy(tmppath, path_base, sizeof(tmppath));

	/* rare cases folder_name is omitted (when looking for ~/.blender/2.xx dir only) */
	if (folder_name)
		BLI_make_file_string("/", targetpath, tmppath, folder_name);
	else
		BLI_strncpy(targetpath, tmppath, sizeof(tmppath));
	/* FIXME: why is "//" on front of tmppath expanded to "/" (by BLI_join_dirfile)
	 * if folder_name is specified but not otherwise? */

	if (BLI_is_dir(targetpath)) {
#ifdef PATH_DEBUG
		printf("\t%s found: %s\n", __func__, targetpath);
#endif
		return true;
	}
	else {
#ifdef PATH_DEBUG
		printf("\t%s missing: %s\n", __func__, targetpath);
#endif
		//targetpath[0] = '\0';
		return false;
	}
}

/**
 * Puts the value of the specified environment variable into *path if it exists
 * and points at a directory. Returns true if this was done.
 */
static bool test_env_path(char *path, const char *envvar)
{
	const char *env = envvar ? getenv(envvar) : NULL;
	if (!env) return false;
	
	if (BLI_is_dir(env)) {
		BLI_strncpy(path, env, FILE_MAX);
#ifdef PATH_DEBUG
		printf("\t%s env %s found: %s\n", __func__, envvar, env);
#endif
		return true;
	}
	else {
		path[0] = '\0';
#ifdef PATH_DEBUG
		printf("\t%s env %s missing: %s\n", __func__, envvar, env);
#endif
		return false;
	}
}

/**
 * Constructs in \a targetpath the name of a directory relative to a version-specific
 * subdirectory in the parent directory of the Blender executable.
 *
 * \param targetpath  String to return path
 * \param folder_name  Optional folder name within version-specific directory
 * \param subfolder_name  Optional subfolder name within folder_name
 * \param ver  To construct name of version-specific directory within bprogdir
 * \return true if such a directory exists.
 */
static bool get_path_local(char *targetpath, const char *folder_name, const char *subfolder_name, const int ver)
{
	char relfolder[FILE_MAX];
	
#ifdef PATH_DEBUG
	printf("%s...\n", __func__);
#endif

	if (folder_name) {
		if (subfolder_name) {
			BLI_join_dirfile(relfolder, sizeof(relfolder), folder_name, subfolder_name);
		}
		else {
			BLI_strncpy(relfolder, folder_name, sizeof(relfolder));
		}
	}
	else {
		relfolder[0] = '\0';
	}

	/* try EXECUTABLE_DIR/2.5x/folder_name - new default directory for local blender installed files */
#ifdef __APPLE__
	static char osx_resourses[FILE_MAX]; /* due new codesign situation in OSX > 10.9.5 we must move the blender_version dir with contents to Resources */
	sprintf(osx_resourses, "%s../Resources", bprogdir);
	return test_path(targetpath, osx_resourses, blender_version_decimal(ver), relfolder);
#else
	return test_path(targetpath, bprogdir, blender_version_decimal(ver), relfolder);
#endif
}

/**
 * Is this an install with user files kept together with the Blender executable and its
 * installation files.
 */
static bool is_portable_install(void)
{
	/* detect portable install by the existence of config folder */
	const int ver = BLENDER_VERSION;
	char path[FILE_MAX];

	return get_path_local(path, "config", NULL, ver);
}

/**
 * Returns the path of a folder within the user-files area.
 *
 *
 * \param targetpath  String to return path
 * \param folder_name  default name of folder within user area
 * \param subfolder_name  optional name of subfolder within folder
 * \param envvar  name of environment variable which, if defined, overrides folder_name
 * \param ver  Blender version, used to construct a subdirectory name
 * \return true if it was able to construct such a path.
 */
static bool get_path_user(char *targetpath, const char *folder_name, const char *subfolder_name, const char *envvar, const int ver)
{
	char user_path[FILE_MAX];
	const char *user_base_path;

	/* for portable install, user path is always local */
	if (is_portable_install())
		return get_path_local(targetpath, folder_name, subfolder_name, ver);
	
	user_path[0] = '\0';

	if (test_env_path(user_path, envvar)) {
		if (subfolder_name) {
			return test_path(targetpath, user_path, NULL, subfolder_name);
		}
		else {
			BLI_strncpy(targetpath, user_path, FILE_MAX);
			return true;
		}
	}

	user_base_path = (const char *)GHOST_getUserDir(ver, blender_version_decimal(ver));
	if (user_base_path)
		BLI_strncpy(user_path, user_base_path, FILE_MAX);

	if (!user_path[0])
		return false;
	
#ifdef PATH_DEBUG
	printf("%s: %s\n", __func__, user_path);
#endif
	
	if (subfolder_name) {
		return test_path(targetpath, user_path, folder_name, subfolder_name);
	}
	else {
		return test_path(targetpath, user_path, NULL, folder_name);
	}
}

/**
 * Returns the path of a folder within the Blender installation directory.
 *
 * \param targetpath  String to return path
 * \param folder_name  default name of folder within installation area
 * \param subfolder_name  optional name of subfolder within folder
 * \param envvar  name of environment variable which, if defined, overrides folder_name
 * \param ver  Blender version, used to construct a subdirectory name
 * \return  true if it was able to construct such a path.
 */
static bool get_path_system(char *targetpath, const char *folder_name, const char *subfolder_name, const char *envvar, const int ver)
{
	char system_path[FILE_MAX];
	const char *system_base_path;
	char cwd[FILE_MAX];
	char relfolder[FILE_MAX];

	if (folder_name) {
		if (subfolder_name) {
			BLI_join_dirfile(relfolder, sizeof(relfolder), folder_name, subfolder_name);
		}
		else {
			BLI_strncpy(relfolder, folder_name, sizeof(relfolder));
		}
	}
	else {
		relfolder[0] = '\0';
	}

	/* first allow developer only overrides to the system path
	 * these are only used when running blender from source */

	/* try CWD/release/folder_name */
	if (BLI_current_working_dir(cwd, sizeof(cwd))) {
		if (test_path(targetpath, cwd, "release", relfolder)) {
			return true;
		}
	}

	/* try EXECUTABLE_DIR/release/folder_name */
	if (test_path(targetpath, bprogdir, "release", relfolder))
		return true;

	/* end developer overrides */



	system_path[0] = '\0';

	if (test_env_path(system_path, envvar)) {
		if (subfolder_name) {
			return test_path(targetpath, system_path, NULL, subfolder_name);
		}
		else {
			BLI_strncpy(targetpath, system_path, FILE_MAX);
			return true;
		}
	}

	system_base_path = (const char *)GHOST_getSystemDir(ver, blender_version_decimal(ver));
	if (system_base_path)
		BLI_strncpy(system_path, system_base_path, FILE_MAX);
	
	if (!system_path[0])
		return false;
	
#ifdef PATH_DEBUG
	printf("%s: %s\n", __func__, system_path);
#endif
	
	if (subfolder_name) {
		/* try $BLENDERPATH/folder_name/subfolder_name */
		return test_path(targetpath, system_path, folder_name, subfolder_name);
	}
	else {
		/* try $BLENDERPATH/folder_name */
		return test_path(targetpath, system_path, NULL, folder_name);
	}
}

/* get a folder out of the 'folder_id' presets for paths */
/* returns the path if found, NULL string if not */
const char *BLI_get_folder(int folder_id, const char *subfolder)
{
	const int ver = BLENDER_VERSION;
	static char path[FILE_MAX] = "";
	
	switch (folder_id) {
		case BLENDER_DATAFILES:     /* general case */
			if (get_path_user(path, "datafiles", subfolder, "BLENDER_USER_DATAFILES", ver)) break;
			if (get_path_local(path, "datafiles", subfolder, ver)) break;
			if (get_path_system(path, "datafiles", subfolder, "BLENDER_SYSTEM_DATAFILES", ver)) break;
			return NULL;
			
		case BLENDER_USER_DATAFILES:
			if (get_path_user(path, "datafiles", subfolder, "BLENDER_USER_DATAFILES", ver)) break;
			return NULL;
			
		case BLENDER_SYSTEM_DATAFILES:
			if (get_path_local(path, "datafiles", subfolder, ver)) break;
			if (get_path_system(path, "datafiles", subfolder, "BLENDER_SYSTEM_DATAFILES", ver)) break;
			return NULL;
			
		case BLENDER_USER_AUTOSAVE:
			if (get_path_user(path, "autosave", subfolder, "BLENDER_USER_DATAFILES", ver)) break;
			return NULL;

		case BLENDER_USER_CONFIG:
			if (get_path_user(path, "config", subfolder, "BLENDER_USER_CONFIG", ver)) break;
			return NULL;
			
		case BLENDER_USER_SCRIPTS:
			if (get_path_user(path, "scripts", subfolder, "BLENDER_USER_SCRIPTS", ver)) break;
			return NULL;
			
		case BLENDER_SYSTEM_SCRIPTS:
			if (get_path_local(path, "scripts", subfolder, ver)) break;
			if (get_path_system(path, "scripts", subfolder, "BLENDER_SYSTEM_SCRIPTS", ver)) break;
			return NULL;
			
		case BLENDER_SYSTEM_PYTHON:
			if (get_path_local(path, "python", subfolder, ver)) break;
			if (get_path_system(path, "python", subfolder, "BLENDER_SYSTEM_PYTHON", ver)) break;
			return NULL;

		default:
			BLI_assert(0);
			break;
	}
	
	return path;
}

/**
 * Returns the path to a folder in the user area without checking that it actually exists first.
 */
const char *BLI_get_user_folder_notest(int folder_id, const char *subfolder)
{
	const int ver = BLENDER_VERSION;
	static char path[FILE_MAX] = "";

	switch (folder_id) {
		case BLENDER_USER_DATAFILES:
			get_path_user(path, "datafiles", subfolder, "BLENDER_USER_DATAFILES", ver);
			break;
		case BLENDER_USER_CONFIG:
			get_path_user(path, "config", subfolder, "BLENDER_USER_CONFIG", ver);
			break;
		case BLENDER_USER_AUTOSAVE:
			get_path_user(path, "autosave", subfolder, "BLENDER_USER_AUTOSAVE", ver);
			break;
		case BLENDER_USER_SCRIPTS:
			get_path_user(path, "scripts", subfolder, "BLENDER_USER_SCRIPTS", ver);
			break;
		default:
			BLI_assert(0);
			break;
	}

	if ('\0' == path[0]) {
		return NULL;
	}
	return path;
}

/**
 * Returns the path to a folder in the user area, creating it if it doesn't exist.
 */
const char *BLI_get_folder_create(int folder_id, const char *subfolder)
{
	const char *path;

	/* only for user folders */
	if (!ELEM(folder_id, BLENDER_USER_DATAFILES, BLENDER_USER_CONFIG, BLENDER_USER_SCRIPTS, BLENDER_USER_AUTOSAVE))
		return NULL;
	
	path = BLI_get_folder(folder_id, subfolder);
	
	if (!path) {
		path = BLI_get_user_folder_notest(folder_id, subfolder);
		if (path) BLI_dir_create_recursive(path);
	}
	
	return path;
}

/**
 * Returns the path of the top-level version-specific local, user or system directory.
 * If do_check, then the result will be NULL if the directory doesn't exist.
 */
const char *BLI_get_folder_version(const int id, const int ver, const bool do_check)
{
	static char path[FILE_MAX] = "";
	bool ok;
	switch (id) {
		case BLENDER_RESOURCE_PATH_USER:
			ok = get_path_user(path, NULL, NULL, NULL, ver);
			break;
		case BLENDER_RESOURCE_PATH_LOCAL:
			ok = get_path_local(path, NULL, NULL, ver);
			break;
		case BLENDER_RESOURCE_PATH_SYSTEM:
			ok = get_path_system(path, NULL, NULL, NULL, ver);
			break;
		default:
			path[0] = '\0'; /* in case do_check is false */
			ok = false;
			BLI_assert(!"incorrect ID");
			break;
	}

	if (!ok && do_check) {
		return NULL;
	}

	return path;
}

/* End new stuff */
/* ************************************************************* */
/* ************************************************************* */



#ifdef PATH_DEBUG
#  undef PATH_DEBUG
#endif

/**
 * Sets the specified environment variable to the specified value,
 * and clears it if val == NULL.
 */
void BLI_setenv(const char *env, const char *val)
{
	/* free windows */
#if (defined(WIN32) || defined(WIN64)) && defined(FREE_WINDOWS)
	char *envstr;

	if (val)
		envstr = BLI_sprintfN("%s=%s", env, val);
	else
		envstr = BLI_sprintfN("%s=", env);

	putenv(envstr);
	MEM_freeN(envstr);

	/* non-free windows */
#elif (defined(WIN32) || defined(WIN64)) /* not free windows */
	uputenv(env, val);


#else
	/* linux/osx/bsd */
	if (val)
		setenv(env, val, 1);
	else
		unsetenv(env);
#endif
}


/**
 * Only set an env var if already not there.
 * Like Unix setenv(env, val, 0);
 *
 * (not used anywhere).
 */
void BLI_setenv_if_new(const char *env, const char *val)
{
	if (getenv(env) == NULL)
		BLI_setenv(env, val);
}

/**
 * Change every \a from in \a string into \a to. The
 * result will be in \a string
 *
 * \param string The string to work on
 * \param from The character to replace
 * \param to The character to replace with
 */
void BLI_char_switch(char *string, char from, char to) 
{
	while (*string != 0) {
		if (*string == from) *string = to;
		string++;
	}
}

/**
 * Strips off nonexistent subdirectories from the end of *dir, leaving the path of
 * the lowest-level directory that does exist.
 */
void BLI_make_exist(char *dir)
{
	int a;

	BLI_char_switch(dir, ALTSEP, SEP);

	a = strlen(dir);

	while (!BLI_is_dir(dir)) {
		a--;
		while (dir[a] != SEP) {
			a--;
			if (a <= 0) break;
		}
		if (a >= 0) {
			dir[a + 1] = '\0';
		}
		else {
#ifdef WIN32
			get_default_root(dir);
#else
			strcpy(dir, "/");
#endif
			break;
		}
	}
}

/**
 * Ensures that the parent directory of *name exists.
 */
void BLI_make_existing_file(const char *name)
{
	char di[FILE_MAX];
	BLI_split_dir_part(name, di, sizeof(di));

	/* make if the dir doesn't exist */
	BLI_dir_create_recursive(di);
}

/**
 * Returns in *string the concatenation of *dir and *file (also with *relabase on the
 * front if specified and *dir begins with "//"). Normalizes all occurrences of path
 * separators, including ensuring there is exactly one between the copies of *dir and *file,
 * and between the copies of *relabase and *dir.
 *
 * \param relabase  Optional prefix to substitute for "//" on front of *dir
 * \param string  Area to return result
 */
void BLI_make_file_string(const char *relabase, char *string, const char *dir, const char *file)
{
	int sl;

	if (string) {
		/* ensure this is always set even if dir/file are NULL */
		string[0] = '\0';

		if (ELEM(NULL, dir, file)) {
			return; /* We don't want any NULLs */
		}
	}
	else {
		return; /* string is NULL, probably shouldnt happen but return anyway */
	}


	/* we first push all slashes into unix mode, just to make sure we don't get
	 * any mess with slashes later on. -jesterKing */
	/* constant strings can be passed for those parameters - don't change them - elubie */
#if 0
	BLI_char_switch(relabase, '\\', '/');
	BLI_char_switch(dir, '\\', '/');
	BLI_char_switch(file, '\\', '/');
#endif

	/* Resolve relative references */
	if (relabase && dir[0] == '/' && dir[1] == '/') {
		char *lslash;
		
		/* Get the file name, chop everything past the last slash (ie. the filename) */
		strcpy(string, relabase);
		
		lslash = (char *)BLI_last_slash(string);
		if (lslash) *(lslash + 1) = 0;

		dir += 2; /* Skip over the relative reference */
	}
#ifdef WIN32
	else {
		if (BLI_strnlen(dir, 3) >= 2 && dir[1] == ':') {
			BLI_strncpy(string, dir, 3);
			dir += 2;
		}
		else if (BLI_strnlen(dir, 3) >= 2 && BLI_path_is_unc(dir)) {
			string[0] = 0;
		}
		else { /* no drive specified */
			   /* first option: get the drive from the relabase if it has one */
			if (relabase && BLI_strnlen(relabase, 3) >= 2 && relabase[1] == ':') {
				BLI_strncpy(string, relabase, 3);
				string[2] = '\\';
				string[3] = '\0';
			}
			else { /* we're out of luck here, guessing the first valid drive, usually c:\ */
				get_default_root(string);
			}
			
			/* ignore leading slashes */
			while (*dir == '/' || *dir == '\\') dir++;
		}
	}
#endif

	strcat(string, dir);

	/* Make sure string ends in one (and only one) slash */
	/* first trim all slashes from the end of the string */
	sl = strlen(string);
	while (sl > 0 && (string[sl - 1] == '/' || string[sl - 1] == '\\') ) {
		string[sl - 1] = '\0';
		sl--;
	}
	/* since we've now removed all slashes, put back one slash at the end. */
	strcat(string, "/");
	
	while (*file && (*file == '/' || *file == '\\')) /* Trim slashes from the front of file */
		file++;
		
	strcat(string, file);
	
	/* Push all slashes to the system preferred direction */
	BLI_path_native_slash(string);
}

static bool testextensie_ex(const char *str, const size_t str_len,
                            const char *ext, const size_t ext_len)
{
	BLI_assert(strlen(str) == str_len);
	BLI_assert(strlen(ext) == ext_len);

	return  (((str_len == 0 || ext_len == 0 || ext_len >= str_len) == 0) &&
	         (BLI_strcasecmp(ext, str + str_len - ext_len) == 0));
}

/* does str end with ext. */
bool BLI_testextensie(const char *str, const char *ext)
{
	return testextensie_ex(str, strlen(str), ext, strlen(ext));
}

bool BLI_testextensie_n(const char *str, ...)
{
	const size_t str_len = strlen(str);

	va_list args;
	const char *ext;
	bool ret = false;

	va_start(args, str);

	while ((ext = (const char *) va_arg(args, void *))) {
		if (testextensie_ex(str, str_len, ext, strlen(ext))) {
			ret = true;
			goto finally;
		}
	}

finally:
	va_end(args);

	return ret;
}

/* does str end with any of the suffixes in *ext_array. */
bool BLI_testextensie_array(const char *str, const char **ext_array)
{
	const size_t str_len = strlen(str);
	int i = 0;

	while (ext_array[i]) {
		if (testextensie_ex(str, str_len, ext_array[i], strlen(ext_array[i]))) {
			return true;
		}

		i++;
	}
	return false;
}

/**
 * Semicolon separated wildcards, eg:
 *  '*.zip;*.py;*.exe'
 * does str match any of the semicolon-separated glob patterns in fnmatch.
 */
bool BLI_testextensie_glob(const char *str, const char *ext_fnmatch)
{
	const char *ext_step = ext_fnmatch;
	char pattern[16];

	while (ext_step[0]) {
		const char *ext_next;
		int len_ext;

		if ((ext_next = strchr(ext_step, ';'))) {
			len_ext = (int)(ext_next - ext_step) + 1;
		}
		else {
			len_ext = sizeof(pattern);
		}

		BLI_strncpy(pattern, ext_step, len_ext);

		if (fnmatch(pattern, str, FNM_CASEFOLD) == 0) {
			return true;
		}
		ext_step += len_ext;
	}

	return false;
}


/**
 * Removes any existing extension on the end of \a path and appends \a ext.
 * \return false if there was no room.
 */
bool BLI_replace_extension(char *path, size_t maxlen, const char *ext)
{
	const size_t path_len = strlen(path);
	const size_t ext_len = strlen(ext);
	ssize_t a;

	for (a = path_len - 1; a >= 0; a--) {
		if (ELEM(path[a], '.', '/', '\\')) {
			break;
		}
	}

	if ((a < 0) || (path[a] != '.')) {
		a = path_len;
	}

	if (a + ext_len >= maxlen)
		return false;

	memcpy(path + a, ext, ext_len + 1);
	return true;
}

/**
 * Strip's trailing '.'s and adds the extension only when needed
 */
bool BLI_ensure_extension(char *path, size_t maxlen, const char *ext)
{
	const size_t path_len = strlen(path);
	const size_t ext_len = strlen(ext);
	ssize_t a;

	/* first check the extension is already there */
	if (    (ext_len <= path_len) &&
	        (strcmp(path + (path_len - ext_len), ext) == 0))
	{
		return true;
	}

	for (a = path_len - 1; a >= 0; a--) {
		if (path[a] == '.') {
			path[a] = '\0';
		}
		else {
			break;
		}
	}
	a++;

	if (a + ext_len >= maxlen)
		return false;

	memcpy(path + a, ext, ext_len + 1);
	return true;
}

bool BLI_ensure_filename(char *filepath, size_t maxlen, const char *filename)
{
	char *c = (char *)BLI_last_slash(filepath);
	if (!c || ((c - filepath) < maxlen - (strlen(filename) + 1))) {
		strcpy(c ? &c[1] : filepath, filename);
		return true;
	}
	return false;
}

/* Converts "/foo/bar.txt" to "/foo/" and "bar.txt"
 * - wont change 'string'
 * - wont create any directories
 * - dosnt use CWD, or deal with relative paths.
 * - Only fill's in *dir and *file when they are non NULL
 * */
void BLI_split_dirfile(const char *string, char *dir, char *file, const size_t dirlen, const size_t filelen)
{
	const char *lslash_str = BLI_last_slash(string);
	const size_t lslash = lslash_str ? (size_t)(lslash_str - string) + 1 : 0;

	if (dir) {
		if (lslash) {
			BLI_strncpy(dir, string, MIN2(dirlen, lslash + 1)); /* +1 to include the slash and the last char */
		}
		else {
			dir[0] = '\0';
		}
	}
	
	if (file) {
		BLI_strncpy(file, string + lslash, filelen);
	}
}

/**
 * Copies the parent directory part of string into *dir, max length dirlen.
 */
void BLI_split_dir_part(const char *string, char *dir, const size_t dirlen)
{
	BLI_split_dirfile(string, dir, NULL, dirlen, 0);
}

/**
 * Copies the leaf filename part of string into *file, max length filelen.
 */
void BLI_split_file_part(const char *string, char *file, const size_t filelen)
{
	BLI_split_dirfile(string, NULL, file, 0, filelen);
}

/**
 * Append a filename to a dir, ensuring slash separates.
 */
void BLI_path_append(char *__restrict dst, const size_t maxlen, const char *__restrict file)
{
	size_t dirlen = BLI_strnlen(dst, maxlen);

	/* inline BLI_add_slash */
	if ((dirlen > 0) && (dst[dirlen - 1] != SEP)) {
		dst[dirlen++] = SEP;
		dst[dirlen] = '\0';
	}

	if (dirlen >= maxlen) {
		return; /* fills the path */
	}

	BLI_strncpy(dst + dirlen, file, maxlen - dirlen);
}

/**
 * Simple appending of filename to dir, does not check for valid path!
 * Puts result into *dst, which may be same area as *dir.
 */
void BLI_join_dirfile(char *__restrict dst, const size_t maxlen, const char *__restrict dir, const char *__restrict file)
{
	size_t dirlen = BLI_strnlen(dir, maxlen);

	/* args can't match */
	BLI_assert(!ELEM(dst, dir, file));

	if (dirlen == maxlen) {
		memcpy(dst, dir, dirlen);
		dst[dirlen - 1] = '\0';
		return; /* dir fills the path */
	}
	else {
		memcpy(dst, dir, dirlen + 1);
	}

	if (dirlen + 1 >= maxlen) {
		return; /* fills the path */
	}

	/* inline BLI_add_slash */
	if ((dirlen > 0) && (dst[dirlen - 1] != SEP)) {
		dst[dirlen++] = SEP;
		dst[dirlen] = '\0';
	}

	if (dirlen >= maxlen) {
		return; /* fills the path */
	}

	BLI_strncpy(dst + dirlen, file, maxlen - dirlen);
}

/**
 * like pythons os.path.basename()
 *
 * \return The pointer into \a path string immediately after last slash,
 * or start of \a path if none found.
 */
const char *BLI_path_basename(const char *path)
{
	const char * const filename = BLI_last_slash(path);
	return filename ? filename + 1 : path;
}

/* UNUSED */
#if 0
/**
 * Produce image export path.
 * 
 * Returns:
 * 0        if image filename is empty or if destination path
 *          matches image path (i.e. both are the same file).
 * 2        if source is identical to destination.
 * 1        if rebase was successful
 * -------------------------------------------------------------
 * Hint: Trailing slash in dest_dir is optional.
 *
 * Logic:
 *
 * - if an image is "below" current .blend file directory:
 *   rebuild the same dir structure in dest_dir
 *
 *   Example: 
 *   src : //textures/foo/bar.png
 *   dest: [dest_dir]/textures/foo/bar.png.
 *
 * - if an image is not "below" current .blend file directory,
 *   disregard it's path and copy it into the destination  
 *   directory.
 *
 *   Example:
 *   src : //../foo/bar.png becomes
 *   dest: [dest_dir]/bar.png.
 *
 * This logic ensures that all image paths are relative and
 * that a user gets his images in one place. It'll also provide
 * consistent behavior across exporters.
 * IMPORTANT NOTE: If base_dir contains an empty string, then
 * this function returns wrong results!
 * XXX: test on empty base_dir and return an error ?
 */

/**
 *
 * \param abs  Optional string to return new full path
 * \param abs_len  Size of *abs string
 * \param rel  Optional area to return new path relative to parent directory of .blend file
 *             (only meaningful if item is in a subdirectory thereof)
 * \param rel_len  Size of *rel area
 * \param base_dir  Path of .blend file
 * \param src_dir  Original path of item (any initial "//" will be expanded to
 *                 parent directory of .blend file)
 * \param dest_dir  New directory into which item will be moved
 * \return bli_rebase_state
 *
 * \note Not actually used anywhere!
 */
int BLI_rebase_path(char *abs, size_t abs_len,
                    char *rel, size_t rel_len,
                    const char *base_dir, const char *src_dir, const char *dest_dir)
{
	char path[FILE_MAX];  /* original full path of item */
	char dir[FILE_MAX];   /* directory part of src_dir */
	char base[FILE_MAX];  /* basename part of src_dir */
	char blend_dir[FILE_MAX];   /* directory, where current .blend file resides */
	char dest_path[FILE_MAX];
	char rel_dir[FILE_MAX];
	int len;

	if (abs)
		abs[0] = 0;

	if (rel)
		rel[0] = 0;

	BLI_split_dir_part(base_dir, blend_dir, sizeof(blend_dir));

	if (src_dir[0] == '\0')
		return BLI_REBASE_NO_SRCDIR;

	BLI_strncpy(path, src_dir, sizeof(path));

	/* expand "//" in filename and get absolute path */
	BLI_path_abs(path, base_dir);

	/* get the directory part */
	BLI_split_dirfile(path, dir, base, sizeof(dir), sizeof(base));

	len = strlen(blend_dir);

	rel_dir[0] = 0;

	/* if image is "below" current .blend file directory */
	if (!BLI_path_ncmp(path, blend_dir, len)) {

		if (BLI_path_cmp(dir, blend_dir) == 0) {
			/* image is directly in .blend file parent directory => put directly in dest_dir */
			BLI_join_dirfile(dest_path, sizeof(dest_path), dest_dir, base);
		}
		else {
			/* "below" (in subdirectory of .blend file parent directory) => put in same relative directory structure in dest_dir */
			/* rel = image_path_dir - blend_dir */
			BLI_strncpy(rel_dir, dir + len, sizeof(rel_dir));
			/* subdirectories relative to blend_dir */
			BLI_join_dirfile(dest_path, sizeof(dest_path), dest_dir, rel_dir);
			/* same subdirectories relative to dest_dir */
			BLI_path_append(dest_path, sizeof(dest_path), base);
			/* keeping original item basename */
		}

	}
	/* image is out of current directory -- just put straight in dest_dir */
	else {
		BLI_join_dirfile(dest_path, sizeof(dest_path), dest_dir, base);
	}

	if (abs)
		BLI_strncpy(abs, dest_path, abs_len);

	if (rel) {
		strncat(rel, rel_dir, rel_len);
		strncat(rel, base, rel_len); /* FIXME: could overflow rel area! */
	}

	/* return 2 if (src == dest) */
	if (BLI_path_cmp(path, dest_path) == 0) {
		// if (G.debug & G_DEBUG) printf("%s and %s are the same file\n", path, dest_path);
		return BLI_REBASE_IDENTITY;
	}

	return BLI_REBASE_OK;
}
#endif


/**
 * Returns pointer to the leftmost path separator in string. Not actually used anywhere.
 */
const char *BLI_first_slash(const char *string)
{
	const char * const ffslash = strchr(string, '/');
	const char * const fbslash = strchr(string, '\\');
	
	if (!ffslash) return fbslash;
	else if (!fbslash) return ffslash;
	
	if ((intptr_t)ffslash < (intptr_t)fbslash) return ffslash;
	else return fbslash;
}

/**
 * Returns pointer to the rightmost path separator in string.
 */
const char *BLI_last_slash(const char *string)
{
	const char * const lfslash = strrchr(string, '/');
	const char * const lbslash = strrchr(string, '\\');

	if (!lfslash) return lbslash; 
	else if (!lbslash) return lfslash;
	
	if ((intptr_t)lfslash < (intptr_t)lbslash) return lbslash;
	else return lfslash;
}

/**
 * Appends a slash to string if there isn't one there already.
 * Returns the new length of the string.
 */
int BLI_add_slash(char *string)
{
	int len = strlen(string);
	if (len == 0 || string[len - 1] != SEP) {
		string[len] = SEP;
		string[len + 1] = '\0';
		return len + 1;
	}
	return len;
}

/**
 * Removes the last slash and everything after it to the end of string, if there is one.
 */
void BLI_del_slash(char *string)
{
	int len = strlen(string);
	while (len) {
		if (string[len - 1] == SEP) {
			string[len - 1] = '\0';
			len--;
		}
		else {
			break;
		}
	}
}

/**
 * Changes to the path separators to the native ones for this OS.
 */
void BLI_path_native_slash(char *path)
{
#ifdef WIN32
	if (path && BLI_strnlen(path, 3) > 2) {
		BLI_char_switch(path + 2, '/', '\\');
	}
#else
	BLI_char_switch(path + BLI_path_unc_prefix_len(path), '\\', '/');
#endif
}

/**
 * Tries appending each of the semicolon-separated extensions in the PATHEXT
 * environment variable (Windows-only) onto *name in turn until such a file is found.
 * Returns success/failure.
 */
static int add_win32_extension(char *name)
{
	int retval = 0;
	int type;

	type = BLI_exists(name);
	if ((type == 0) || S_ISDIR(type)) {
#ifdef _WIN32
		char filename[FILE_MAX];
		char ext[FILE_MAX];
		const char *extensions = getenv("PATHEXT");
		if (extensions) {
			char *temp;
			do {
				strcpy(filename, name);
				temp = strstr(extensions, ";");
				if (temp) {
					strncpy(ext, extensions, temp - extensions);
					ext[temp - extensions] = 0;
					extensions = temp + 1;
					strcat(filename, ext);
				}
				else {
					strcat(filename, extensions);
				}

				type = BLI_exists(filename);
				if (type && (!S_ISDIR(type))) {
					retval = 1;
					strcpy(name, filename);
					break;
				}
			} while (temp);
		}
#endif
	}
	else {
		retval = 1;
	}

	return (retval);
}

/**
 * Checks if name is a fully qualified filename to an executable.
 * If not it searches $PATH for the file. On Windows it also
 * adds the correct extension (.com .exe etc) from
 * $PATHEXT if necessary. Also on Windows it translates
 * the name to its 8.3 version to prevent problems with
 * spaces and stuff. Final result is returned in fullname.
 *
 * \param fullname The full path and full name of the executable
 * (must be FILE_MAX minimum)
 * \param name The name of the executable (usually argv[0]) to be checked
 */
static void bli_where_am_i(char *fullname, const size_t maxlen, const char *name)
{
	char filename[FILE_MAX];
	const char *path = NULL, *temp;

#ifdef _WIN32
	const char *separator = ";";
#else
	const char *separator = ":";
#endif

	
#ifdef WITH_BINRELOC
	/* linux uses binreloc since argv[0] is not reliable, call br_init( NULL ) first */
	path = br_find_exe(NULL);
	if (path) {
		BLI_strncpy(fullname, path, maxlen);
		free((void *)path);
		return;
	}
#endif

#ifdef _WIN32
	wchar_t *fullname_16 = MEM_mallocN(maxlen * sizeof(wchar_t), "ProgramPath");
	if (GetModuleFileNameW(0, fullname_16, maxlen)) {
		conv_utf_16_to_8(fullname_16, fullname, maxlen);
		if (!BLI_exists(fullname)) {
			printf("path can't be found: \"%.*s\"\n", (int)maxlen, fullname);
			MessageBox(NULL, "path contains invalid characters or is too long (see console)", "Error", MB_OK);
		}
		MEM_freeN(fullname_16);
		return;
	}

	MEM_freeN(fullname_16);
#endif

	/* unix and non linux */
	if (name && name[0]) {

		BLI_strncpy(fullname, name, maxlen);
		if (name[0] == '.') {
			char wdir[FILE_MAX] = "";
			BLI_current_working_dir(wdir, sizeof(wdir));     /* backup cwd to restore after */

			// not needed but avoids annoying /./ in name
			if (name[1] == SEP)
				BLI_join_dirfile(fullname, maxlen, wdir, name + 2);
			else
				BLI_join_dirfile(fullname, maxlen, wdir, name);

			add_win32_extension(fullname); /* XXX, doesnt respect length */
		}
		else if (BLI_last_slash(name)) {
			// full path
			BLI_strncpy(fullname, name, maxlen);
			add_win32_extension(fullname);
		}
		else {
			// search for binary in $PATH
			path = getenv("PATH");
			if (path) {
				do {
					temp = strstr(path, separator);
					if (temp) {
						strncpy(filename, path, temp - path);
						filename[temp - path] = 0;
						path = temp + 1;
					}
					else {
						strncpy(filename, path, sizeof(filename));
					}
					BLI_path_append(fullname, maxlen, name);
					if (add_win32_extension(filename)) {
						BLI_strncpy(fullname, filename, maxlen);
						break;
					}
				} while (temp);
			}
		}
#if defined(DEBUG)
		if (strcmp(name, fullname)) {
			printf("guessing '%s' == '%s'\n", name, fullname);
		}
#endif
	}
}

void BLI_init_program_path(const char *argv0)
{
	bli_where_am_i(bprogname, sizeof(bprogname), argv0);
	BLI_split_dir_part(bprogname, bprogdir, sizeof(bprogdir));
}

/**
 * Path to executable
 */
const char *BLI_program_path(void)
{
	return bprogname;
}

/**
 * Path to directory of executable
 */
const char *BLI_program_dir(void)
{
	return bprogdir;
}

/**
 * Gets the temp directory when blender first runs.
 * If the default path is not found, use try $TEMP
 * 
 * Also make sure the temp dir has a trailing slash
 *
 * \param fullname The full path to the temporary temp directory
 * \param basename The full path to the persistent temp directory (may be NULL)
 * \param maxlen The size of the fullname buffer
 * \param userdir Directory specified in user preferences 
 */
static void BLI_where_is_temp(char *fullname, char *basename, const size_t maxlen, char *userdir)
{
	/* Clear existing temp dir, if needed. */
	BLI_temp_dir_session_purge();

	fullname[0] = '\0';
	if (basename) {
		basename[0] = '\0';
	}

	if (userdir && BLI_is_dir(userdir)) {
		BLI_strncpy(fullname, userdir, maxlen);
	}
	
	
#ifdef WIN32
	if (fullname[0] == '\0') {
		const char *tmp = getenv("TEMP"); /* Windows */
		if (tmp && BLI_is_dir(tmp)) {
			BLI_strncpy(fullname, tmp, maxlen);
		}
	}
#else
	/* Other OS's - Try TMP and TMPDIR */
	if (fullname[0] == '\0') {
		const char *tmp = getenv("TMP");
		if (tmp && BLI_is_dir(tmp)) {
			BLI_strncpy(fullname, tmp, maxlen);
		}
	}
	
	if (fullname[0] == '\0') {
		const char *tmp = getenv("TMPDIR");
		if (tmp && BLI_is_dir(tmp)) {
			BLI_strncpy(fullname, tmp, maxlen);
		}
	}
#endif
	
	if (fullname[0] == '\0') {
		BLI_strncpy(fullname, "/tmp/", maxlen);
	}
	else {
		/* add a trailing slash if needed */
		BLI_add_slash(fullname);
#ifdef WIN32
		if (userdir && userdir != fullname) {
			BLI_strncpy(userdir, fullname, maxlen); /* also set user pref to show %TEMP%. /tmp/ is just plain confusing for Windows users. */
		}
#endif
	}

	/* Now that we have a valid temp dir, add system-generated unique sub-dir. */
	if (basename) {
		/* 'XXXXXX' is kind of tag to be replaced by mktemp-familly by an uuid. */
		char *tmp_name = BLI_strdupcat(fullname, "blender_XXXXXX");
		const size_t ln = strlen(tmp_name) + 1;
		if (ln <= maxlen) {
#ifdef WIN32
			if (_mktemp_s(tmp_name, ln) == 0) {
				BLI_dir_create_recursive(tmp_name);
			}
#else
			mkdtemp(tmp_name);
#endif
		}
		if (BLI_is_dir(tmp_name)) {
			BLI_strncpy(basename, fullname, maxlen);
			BLI_strncpy(fullname, tmp_name, maxlen);
			BLI_add_slash(fullname);
		}
		else {
			printf("Warning! Could not generate a temp file name for '%s', falling back to '%s'\n", tmp_name, fullname);
		}

		MEM_freeN(tmp_name);
	}
}

/**
 * Sets btempdir_base to userdir if specified and is a valid directory, otherwise
 * chooses a suitable OS-specific temporary directory.
 * Sets btempdir_session to a mkdtemp-generated sub-dir of btempdir_base.
 */
void BLI_temp_dir_init(char *userdir)
{
	BLI_where_is_temp(btempdir_session, btempdir_base, FILE_MAX, userdir);
;
}

/**
 * Path to temporary directory (with trailing slash)
 */
const char *BLI_temp_dir_session(void)
{
	return btempdir_session[0] ? btempdir_session : BLI_temp_dir_base();
}

/**
 * Path to persistent temporary directory (with trailing slash)
 */
const char *BLI_temp_dir_base(void)
{
	return btempdir_base;
}

/**
 * Path to the system temporary directory (with trailing slash)
 */
void BLI_system_temporary_dir(char *dir)
{
	BLI_where_is_temp(dir, NULL, FILE_MAX, NULL);
}

/**
 * Delete content of this instance's temp dir.
 */
void BLI_temp_dir_session_purge(void)
{
	if (btempdir_session[0] && BLI_is_dir(btempdir_session)) {
		BLI_delete(btempdir_session, true, true);
	}
}

#ifdef WITH_ICONV

/**
 * Converts a string encoded in the charset named by *code to UTF-8.
 * Opens a new iconv context each time it is run, which is probably not the
 * most efficient. */
void BLI_string_to_utf8(char *original, char *utf_8, const char *code)
{
	size_t inbytesleft = strlen(original);
	size_t outbytesleft = 512;
	size_t rv = 0;
	iconv_t cd;
	
	if (NULL == code) {
		code = locale_charset();
	}
	cd = iconv_open("UTF-8", code);

	if (cd == (iconv_t)(-1)) {
		printf("iconv_open Error");
		*utf_8 = '\0';
		return;
	}
	rv = iconv(cd, &original, &inbytesleft, &utf_8, &outbytesleft);
	if (rv == (size_t) -1) {
		printf("iconv Error\n");
		iconv_close(cd);
		return;
	}
	*utf_8 = '\0';
	iconv_close(cd);
}
#endif // WITH_ICONV

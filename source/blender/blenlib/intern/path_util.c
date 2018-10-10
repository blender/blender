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
#  include "BLI_alloca.h"
#else
#  include "unistd.h"
#endif /* WIN32 */

#include "MEM_guardedalloc.h"

/* Declarations */

#ifdef WIN32

/* return true if the path is absolute ie starts with a drive specifier (eg A:\) or is a UNC path */
static bool BLI_path_is_abs(const char *name);

#endif  /* WIN32 */

// #define DEBUG_STRSIZE

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
int BLI_stringdec(const char *string, char *head, char *tail, ushort *r_num_len)
{
	uint nums = 0, nume = 0;
	int i;
	bool found_digit = false;
	const char * const lslash = BLI_last_slash(string);
	const uint string_len = strlen(string);
	const uint lslash_len = lslash != NULL ? (int)(lslash - string) : 0;
	uint name_end = string_len;

	while (name_end > lslash_len && string[--name_end] != '.') {
		/* name ends at dot if present */
	}
	if (name_end == lslash_len && string[name_end] != '.') {
		name_end = string_len;
	}

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
		const long long int ret = strtoll(&(string[nums]), NULL, 10);
		if (ret >= INT_MIN && ret <= INT_MAX) {
			if (tail) {
				strcpy(tail, &string[nume + 1]);
			}
			if (head) {
				strcpy(head, string);
				head[nums] = 0;
			}
			if (r_num_len) {
				*r_num_len = nume - nums + 1;
			}
			return (int)ret;
		}
	}

	if (tail) {
		strcpy(tail, string + name_end);
	}
	if (head) {
		/* name_end points to last character of head,
		 * make it +1 so null-terminator is nicely placed
		 */
		BLI_strncpy(head, string, name_end + 1);
	}
	if (r_num_len) {
		*r_num_len = 0;
	}
	return 0;
}


/**
 * Returns in area pointed to by string a string of the form "<head><pic><tail>", where pic
 * is formatted as numlen digits with leading zeroes.
 */
void BLI_stringenc(char *string, const char *head, const char *tail, unsigned short numlen, int pic)
{
	sprintf(string, "%s%.*d%s", head, numlen, MAX2(0, pic), tail);
}

static int BLI_path_unc_prefix_len(const char *path); /* defined below in same file */

/* ******************** string encoding ***************** */

/**
 * Remove redundant characters from \a path and optionally make absolute.
 *
 * \param relabase: The path this is relative to, or ignored when NULL.
 * \param path: Can be any input, and this function converts it to a regular full path.
 * Also removes garbage from directory paths, like `/../` or double slashes etc.
 *
 * \note \a path isn't protected for max string names...
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
	 *   strcpy(start, eind);
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

/**
 * Cleanup filepath ensuring a trailing slash.
 */
void BLI_cleanup_dir(const char *relabase, char *dir)
{
	BLI_cleanup_path(relabase, dir);
	BLI_add_slash(dir);

}

/**
 * Cleanup filepath ensuring no trailing slash.
 */
void BLI_cleanup_file(const char *relabase, char *path)
{
	BLI_cleanup_path(relabase, path);
	BLI_del_slash(path);
}


/**
 * Make given name safe to be used in paths.
 *
 * \return true if \a fname was changed, false otherwise.
 *
 * For now, simply replaces reserved chars (as listed in
 * https://en.wikipedia.org/wiki/Filename#Reserved_characters_and_words )
 * by underscores ('_').
 *
 * \note Space case ' ' is a bit of an edge case here - in theory it is allowed, but again can be an issue
 *       in some cases, so we simply replace it by an underscore too (good practice anyway).
 *       REMOVED based on popular demand (see T45900).
 *       Percent '%' char is a bit same case - not recommended to use it, but supported by all decent FS/OS around...
 *
 * \note On Windows, it also ensures there is no '.' (dot char) at the end of the file, this can lead to issues...
 *
 * \note On Windows, it also checks for forbidden names
 *       (see https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247%28v=vs.85%29.aspx ).
 */
bool BLI_filename_make_safe(char *fname)
{
	const char *invalid = "\x01\x02\x03\x04\x05\x06\x07\x08\x09\x0a\x0b\x0c\x0d\x0e\x0f"
	                      "\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f"
	                      "/\\?*:|\"<>";
	char *fn;
	bool changed = false;

	if (*fname == '\0') {
		return changed;
	}

	for (fn = fname; *fn && (fn = strpbrk(fn, invalid)); fn++) {
		*fn = '_';
		changed = true;
	}

	/* Forbid only dots. */
	for (fn = fname; *fn == '.'; fn++);
	if (*fn == '\0') {
		*fname = '_';
		changed = true;
	}

#ifdef WIN32
	{
		const size_t len = strlen(fname);
		const char *invalid_names[] = {
		    "con", "prn", "aux", "null",
		    "com1", "com2", "com3", "com4", "com5", "com6", "com7", "com8", "com9",
		    "lpt1", "lpt2", "lpt3", "lpt4", "lpt5", "lpt6", "lpt7", "lpt8", "lpt9",
		    NULL
		};
		char *lower_fname = BLI_strdup(fname);
		const char **iname;

		/* Forbid trailing dot (trailing space has already been replaced above). */
		if (fname[len - 1] == '.') {
			fname[len - 1] = '_';
			changed = true;
		}

		/* Check for forbidden names - not we have to check all combination of upper and lower cases, hence the usage
		 * of lower_fname (more efficient than using BLI_strcasestr repeatedly). */
		BLI_str_tolower_ascii(lower_fname, len);
		for (iname = invalid_names; *iname; iname++) {
			if (strstr(lower_fname, *iname) == lower_fname) {
				const size_t iname_len = strlen(*iname);
				/* Only invalid if the whole name is made of the invalid chunk, or it has an (assumed extension) dot
				 * just after. This means it will also catch 'valid' names like 'aux.foo.bar', but should be
				 * good enough for us! */
				if ((iname_len == len) || (lower_fname[iname_len] == '.')) {
					*fname = '_';
					changed = true;
					break;
				}
			}
		}

		MEM_freeN(lower_fname);
	}
#endif

	return changed;
}

/**
 * Make given path OS-safe.
 *
 * \return true if \a path was changed, false otherwise.
 */
bool BLI_path_make_safe(char *path)
{
	/* Simply apply BLI_filename_make_safe() over each component of the path.
	 * Luckily enough, same 'safe' rules applies to filenames and dirnames. */
	char *curr_slash, *curr_path = path;
	bool changed = false;
	bool skip_first = false;

#ifdef WIN32
	if (BLI_path_is_abs(path)) {
		/* Do not make safe 'C:' in 'C:\foo\bar'... */
		skip_first = true;
	}
#endif

	for (curr_slash = (char *)BLI_first_slash(curr_path); curr_slash; curr_slash = (char *)BLI_first_slash(curr_path)) {
		const char backup = *curr_slash;
		*curr_slash = '\0';
		if (!skip_first && (*curr_path != '\0') && BLI_filename_make_safe(curr_path)) {
			changed = true;
		}
		skip_first = false;
		curr_path = curr_slash + 1;
		*curr_slash = backup;
	}
	if (BLI_filename_make_safe(curr_path)) {
		changed = true;
	}

	return changed;
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

	BLI_str_replace_char(temp + BLI_path_unc_prefix_len(temp), '\\', '/');
	BLI_str_replace_char(file + BLI_path_unc_prefix_len(file), '\\', '/');

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
		BLI_str_replace_char(res + 2, '/', '\\');
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
#ifdef DEBUG_STRSIZE
	memset(string, 0xff, sizeof(*string) * maxlen);
#endif
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

	if (!BLI_path_extension_check(tmp, parent_dir)) {
		strcpy(path, tmp);  /* We assume pardir is always shorter... */
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
	uint ch_sta, ch_end, i;
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
 * Get the frame from a filename formatted by blender's frame scheme
 */
bool BLI_path_frame_get(char *path, int *r_frame, int *r_numdigits)
{
	if (*path) {
		char *file = (char *)BLI_last_slash(path);
		char *c;
		int len, numdigits;

		numdigits = *r_numdigits = 0;

		if (file == NULL)
			file = path;

		/* first get the extension part */
		len = strlen(file);

		c = file + len;

		/* isolate extension */
		while (--c != file) {
			if (*c == '.') {
				c--;
				break;
			}
		}

		/* find start of number */
		while (c != (file - 1) && isdigit(*c)) {
			c--;
			numdigits++;
		}

		if (numdigits) {
			char prevchar;

			c++;
			prevchar = c[numdigits];
			c[numdigits] = 0;

			/* was the number really an extension? */
			*r_frame = atoi(c);
			c[numdigits] = prevchar;

			*r_numdigits = numdigits;

			return true;
		}
	}

	return false;
}

void BLI_path_frame_strip(char *path, bool set_frame_char, char *ext)
{
	if (*path) {
		char *file = (char *)BLI_last_slash(path);
		char *c, *suffix;
		int len;
		int numdigits = 0;

		if (file == NULL)
			file = path;

		/* first get the extension part */
		len = strlen(file);

		c = file + len;

		/* isolate extension */
		while (--c != file) {
			if (*c == '.') {
				c--;
				break;
			}
		}

		suffix = c + 1;

		/* find start of number */
		while (c != (file - 1) && isdigit(*c)) {
			c--;
			numdigits++;
		}

		c++;

		if (numdigits) {
			/* replace the number with the suffix and terminate the string */
			while (numdigits--) {
				*ext++ = *suffix;
				*c++ = set_frame_char ? '#' : *suffix;
				suffix++;
			}
			*c = '\0';
			*ext = '\0';
		}
	}
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
 * If path begins with "//", strips that and replaces it with basepath directory.
 *
 * \note Also converts drive-letter prefix to something more sensible
 * if this is a non-drive-letter-based system.
 *
 * \param path: The path to convert.
 * \param basepath: The directory to base relative paths with.
 * \return true if the path was relative (started with "//").
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
	BLI_str_replace_char(tmp + BLI_path_unc_prefix_len(tmp), '\\', '/');

	/* Paths starting with // will get the blend file as their base,
	 * this isn't standard in any os but is used in blender all over the place */
	if (wasrelative) {
		const char *lslash;
		BLI_strncpy(base, basepath, sizeof(base));

		/* file component is ignored, so don't bother with the trailing slash */
		BLI_cleanup_path(NULL, base);
		lslash = BLI_last_slash(base);
		BLI_str_replace_char(base + BLI_path_unc_prefix_len(base), '\\', '/');

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

#ifdef WIN32
	/* skip first two chars, which in case of
	 * absolute path will be drive:/blabla and
	 * in case of relpath //blabla/. So relpath
	 * // will be retained, rest will be nice and
	 * shiny win32 backward slashes :) -jesterKing
	 */
	BLI_str_replace_char(path + 2, '/', '\\');
#endif

	/* ensure this is after correcting for path switch */
	BLI_cleanup_path(NULL, path);

	return wasrelative;
}


/**
 * Expands path relative to the current working directory, if it was relative.
 * Returns true if such expansion was done.
 *
 * \note Should only be done with command line paths.
 * this is _not_ something blenders internal paths support like the "//" prefix
 */
bool BLI_path_cwd(char *path, const size_t maxlen)
{
#ifdef DEBUG_STRSIZE
	memset(path, 0xff, sizeof(*path) * maxlen);
#endif
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
		char cwd[FILE_MAX];
		/* in case the full path to the blend isn't used */
		if (BLI_current_working_dir(cwd, sizeof(cwd))) {
			char origpath[FILE_MAX];
			BLI_strncpy(origpath, path, FILE_MAX);
			BLI_join_dirfile(path, maxlen, cwd, origpath);
		}
		else {
			printf("Could not get the current working directory - $PWD for an unknown reason.\n");
		}
	}

	return wasrelative;
}

#ifdef _WIN32
/**
 * Tries appending each of the semicolon-separated extensions in the PATHEXT
 * environment variable (Windows-only) onto *name in turn until such a file is found.
 * Returns success/failure.
 */
bool BLI_path_program_extensions_add_win32(char *name, const size_t maxlen)
{
	bool retval = false;
	int type;

	type = BLI_exists(name);
	if ((type == 0) || S_ISDIR(type)) {
		/* typically 3-5, ".EXE", ".BAT"... etc */
		const int ext_max = 12;
		const char *ext = BLI_getenv("PATHEXT");
		if (ext) {
			const int name_len = strlen(name);
			char *filename = alloca(name_len + ext_max);
			char *filename_ext;
			const char *ext_next;

			/* null terminated in the loop */
			memcpy(filename, name, name_len);
			filename_ext = filename + name_len;

			do {
				int ext_len;
				ext_next = strchr(ext, ';');
				ext_len = ext_next ? ((ext_next++) - ext) : strlen(ext);

				if (LIKELY(ext_len < ext_max)) {
					memcpy(filename_ext, ext, ext_len);
					filename_ext[ext_len] = '\0';

					type = BLI_exists(filename);
					if (type && (!S_ISDIR(type))) {
						retval = true;
						BLI_strncpy(name, filename, maxlen);
						break;
					}
				}
			} while ((ext = ext_next));
		}
	}
	else {
		retval = true;
	}

	return retval;
}
#endif  /* WIN32 */

/**
 * Search for a binary (executable)
 */
bool BLI_path_program_search(
        char *fullname, const size_t maxlen,
        const char *name)
{
#ifdef DEBUG_STRSIZE
	memset(fullname, 0xff, sizeof(*fullname) * maxlen);
#endif
	const char *path;
	bool retval = false;

#ifdef _WIN32
	const char separator = ';';
#else
	const char separator = ':';
#endif

	path = BLI_getenv("PATH");
	if (path) {
		char filename[FILE_MAX];
		const char *temp;

		do {
			temp = strchr(path, separator);
			if (temp) {
				memcpy(filename, path, temp - path);
				filename[temp - path] = 0;
				path = temp + 1;
			}
			else {
				BLI_strncpy(filename, path, sizeof(filename));
			}

			BLI_path_append(filename, maxlen, name);
			if (
#ifdef _WIN32
			    BLI_path_program_extensions_add_win32(filename, maxlen)
#else
			    BLI_exists(filename)
#endif
			    )
			{
				BLI_strncpy(fullname, filename, maxlen);
				retval = true;
				break;
			}
		} while (temp);
	}

	if (retval == false) {
		*fullname = '\0';
	}

	return retval;
}

/**
 * Sets the specified environment variable to the specified value,
 * and clears it if val == NULL.
 */
void BLI_setenv(const char *env, const char *val)
{
	/* free windows */

#if (defined(WIN32) || defined(WIN64))
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
	if (BLI_getenv(env) == NULL)
		BLI_setenv(env, val);
}

/**
* get an env var, result has to be used immediately
*/
const char *BLI_getenv(const char *env)
{
#ifdef _MSC_VER
	static char buffer[32767]; /* 32767 is the total size of the environment block on windows*/
	if (GetEnvironmentVariableA(env, buffer, sizeof(buffer)))
		return buffer;
	else
		return NULL;
#else
	return getenv(env);
#endif
}


/**
 * Strips off nonexistent (or non-accessible) subdirectories from the end of *dir, leaving the path of
 * the lowest-level directory that does exist and we can read.
 */
void BLI_make_exist(char *dir)
{
	bool valid_path = true;

	/* Loop as long as cur path is not a dir, and we can get a parent path. */
	while ((BLI_access(dir, R_OK) != 0) && (valid_path = BLI_parent_dir(dir)));

	/* If we could not find an existing dir, use default root... */
	if (!valid_path || !dir[0]) {
#ifdef WIN32
		get_default_root(dir);
#else
		strcpy(dir, "/");
#endif
	}
}

/**
 * Ensures that the parent directory of *name exists.
 *
 * \return true on success (i.e. given path now exists on FS), false otherwise.
 */
bool BLI_make_existing_file(const char *name)
{
	char di[FILE_MAX];
	BLI_split_dir_part(name, di, sizeof(di));

	/* make if the dir doesn't exist */
	return BLI_dir_create_recursive(di);
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
	BLI_str_replace_char(relabase, '\\', '/');
	BLI_str_replace_char(dir, '\\', '/');
	BLI_str_replace_char(file, '\\', '/');
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

static bool path_extension_check_ex(const char *str, const size_t str_len,
                            const char *ext, const size_t ext_len)
{
	BLI_assert(strlen(str) == str_len);
	BLI_assert(strlen(ext) == ext_len);

	return  (((str_len == 0 || ext_len == 0 || ext_len >= str_len) == 0) &&
	         (BLI_strcasecmp(ext, str + str_len - ext_len) == 0));
}

/* does str end with ext. */
bool BLI_path_extension_check(const char *str, const char *ext)
{
	return path_extension_check_ex(str, strlen(str), ext, strlen(ext));
}

bool BLI_path_extension_check_n(const char *str, ...)
{
	const size_t str_len = strlen(str);

	va_list args;
	const char *ext;
	bool ret = false;

	va_start(args, str);

	while ((ext = (const char *) va_arg(args, void *))) {
		if (path_extension_check_ex(str, str_len, ext, strlen(ext))) {
			ret = true;
			break;
		}
	}

	va_end(args);

	return ret;
}

/* does str end with any of the suffixes in *ext_array. */
bool BLI_path_extension_check_array(const char *str, const char **ext_array)
{
	const size_t str_len = strlen(str);
	int i = 0;

	while (ext_array[i]) {
		if (path_extension_check_ex(str, str_len, ext_array[i], strlen(ext_array[i]))) {
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
bool BLI_path_extension_check_glob(const char *str, const char *ext_fnmatch)
{
	const char *ext_step = ext_fnmatch;
	char pattern[16];

	while (ext_step[0]) {
		const char *ext_next;
		size_t len_ext;

		if ((ext_next = strchr(ext_step, ';'))) {
			len_ext = ext_next - ext_step + 1;
			BLI_strncpy(pattern, ext_step, (len_ext > sizeof(pattern)) ? sizeof(pattern) : len_ext);
		}
		else {
			len_ext = BLI_strncpy_rlen(pattern, ext_step, sizeof(pattern));
		}

		if (fnmatch(pattern, str, FNM_CASEFOLD) == 0) {
			return true;
		}
		ext_step += len_ext;
	}

	return false;
}

/**
 * Does basic validation of the given glob string, to prevent common issues from string truncation.
 *
 * For now, only forbids last group to be a wildcard-only one, if there are more than one group
 * (i.e. things like "*.txt;*.cpp;*" are changed to "*.txt;*.cpp;")
 *
 * \returns true if it had to modify given \a ext_fnmatch pattern.
 */
bool BLI_path_extension_glob_validate(char *ext_fnmatch)
{
	bool only_wildcards = false;

	for (size_t i = strlen(ext_fnmatch); i-- > 0; ) {
		if (ext_fnmatch[i] == ';') {
			/* Group separator, we truncate here if we only had wildcards so far. Otherwise, all is sound and fine. */
			if (only_wildcards) {
				ext_fnmatch[i] = '\0';
				return true;
			}
			return false;
		}
		if (!ELEM(ext_fnmatch[i], '?', '*')) {
			/* Non-wildcard char, we can break here and consider the pattern valid. */
			return false;
		}
		/* So far, only wildcards in last group of the pattern... */
		only_wildcards = true;
	}
	/* Only one group in the pattern, so even if its only made of wildcard(s), it is assumed vaid. */
	return false;
}

/**
 * Removes any existing extension on the end of \a path and appends \a ext.
 * \return false if there was no room.
 */
bool BLI_path_extension_replace(char *path, size_t maxlen, const char *ext)
{
#ifdef DEBUG_STRSIZE
	memset(path, 0xff, sizeof(*path) * maxlen);
#endif
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
bool BLI_path_extension_ensure(char *path, size_t maxlen, const char *ext)
{
#ifdef DEBUG_STRSIZE
	memset(path, 0xff, sizeof(*path) * maxlen);
#endif
	const size_t path_len = strlen(path);
	const size_t ext_len = strlen(ext);
	ssize_t a;

	/* first check the extension is already there */
	if ((ext_len <= path_len) && (STREQ(path + (path_len - ext_len), ext))) {
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
#ifdef DEBUG_STRSIZE
	memset(filepath, 0xff, sizeof(*filepath) * maxlen);
#endif
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
#ifdef DEBUG_STRSIZE
	memset(dir, 0xff, sizeof(*dir) * dirlen);
	memset(file, 0xff, sizeof(*file) * filelen);
#endif
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
#ifdef DEBUG_STRSIZE
	memset(dst, 0xff, sizeof(*dst) * maxlen);
#endif
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
	if ((dirlen > 0) && !ELEM(dst[dirlen - 1], SEP, ALTSEP)) {
		dst[dirlen++] = SEP;
		dst[dirlen] = '\0';
	}

	if (dirlen >= maxlen) {
		return; /* fills the path */
	}

	BLI_strncpy(dst + dirlen, file, maxlen - dirlen);
}

/**
 * Join multiple strings into a path, ensuring only a single path separator between each,
 * and trailing slash is kept.
 *
 * \note If you want a trailing slash, add ``SEP_STR`` as the last path argument,
 * duplicate slashes will be cleaned up.
 */
size_t BLI_path_join(char *__restrict dst, const size_t dst_len, const char *path, ...)
{
#ifdef DEBUG_STRSIZE
	memset(dst, 0xff, sizeof(*dst) * dst_len);
#endif
	if (UNLIKELY(dst_len == 0)) {
		return 0;
	}
	const size_t dst_last = dst_len - 1;
	size_t ofs = BLI_strncpy_rlen(dst, path, dst_len);

	if (ofs == dst_last) {
		return ofs;
	}

	/* remove trailing slashes, unless there are _only_ trailing slashes
	 * (allow "//" as the first argument). */
	bool has_trailing_slash = false;
	if (ofs != 0) {
		size_t len = ofs;
		while ((len != 0) && ELEM(path[len - 1], SEP, ALTSEP)) {
			len -= 1;
		}
		if (len != 0) {
			ofs = len;
		}
		has_trailing_slash = (path[len] != '\0');
	}

	va_list args;
	va_start(args, path);
	while ((path = (const char *) va_arg(args, const char *))) {
		has_trailing_slash = false;
		const char *path_init = path;
		while (ELEM(path[0], SEP, ALTSEP)) {
			path++;
		}
		size_t len = strlen(path);
		if (len != 0) {
			while ((len != 0) && ELEM(path[len - 1], SEP, ALTSEP)) {
				len -= 1;
			}

			if (len != 0) {
				/* the very first path may have a slash at the end */
				if (ofs && !ELEM(dst[ofs - 1], SEP, ALTSEP)) {
					dst[ofs++] = SEP;
					if (ofs == dst_last) {
						break;
					}
				}
				has_trailing_slash = (path[len] != '\0');
				if (ofs + len >= dst_last) {
					len = dst_last - ofs;
				}
				memcpy(&dst[ofs], path, len);
				ofs += len;
				if (ofs == dst_last) {
					break;
				}
			}
		}
		else {
			has_trailing_slash = (path_init != path);
		}
	}
	va_end(args);

	if (has_trailing_slash) {
		if ((ofs != dst_last) && (ofs != 0) && (ELEM(dst[ofs - 1], SEP, ALTSEP) == 0)) {
			dst[ofs++] = SEP;
		}
	}

	BLI_assert(ofs <= dst_last);
	dst[ofs] = '\0';

	return ofs;
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

/**
 * Get an element of the path at an index, eg:
 * "/some/path/file.txt" where an index of...
 * - 0 or -3: "some"
 * - 1 or -2: "path"
 * - 2 or -1: "file.txt"
 *
 * Ignores multiple slashes at any point in the path (including start/end).
 */
bool BLI_path_name_at_index(const char *path, const int index, int *r_offset, int *r_len)
{
	if (index >= 0) {
		int index_step = 0;
		int prev = -1;
		int i = 0;
		while (true) {
			const char c = path[i];
			if (ELEM(c, SEP, ALTSEP, '\0')) {
				if (prev + 1 != i) {
					prev += 1;
					if (index_step == index) {
						*r_offset = prev;
						*r_len = i - prev;
						/* printf("!!! %d %d\n", start, end); */
						return true;
					}
					index_step += 1;
				}
				if (c == '\0') {
					break;
				}
				prev = i;
			}
			i += 1;
		}
		return false;
	}
	else {
		/* negative number, reverse where -1 is the last element */
		int index_step = -1;
		int prev = strlen(path);
		int i = prev - 1;
		while (true) {
			const char c = i >= 0 ? path[i] : '\0';
			if (ELEM(c, SEP, ALTSEP, '\0')) {
				if (prev - 1 != i) {
					i += 1;
					if (index_step == index) {
						*r_offset = i;
						*r_len = prev - i;
						return true;
					}
					index_step -= 1;
				}
				if (c == '\0') {
					break;
				}
				prev = i;
			}
			i -= 1;
		}
		return false;
	}
}

/**
 * Returns pointer to the leftmost path separator in string. Not actually used anywhere.
 */
const char *BLI_first_slash(const char *string)
{
	const char * const ffslash = strchr(string, '/');
	const char * const fbslash = strchr(string, '\\');

	if (!ffslash) return fbslash;
	else if (!fbslash) return ffslash;

	return (ffslash < fbslash) ? ffslash : fbslash;
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

	return (lfslash > lbslash) ? lfslash : lbslash;
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
		BLI_str_replace_char(path + 2, '/', '\\');
	}
#else
	BLI_str_replace_char(path + BLI_path_unc_prefix_len(path), '\\', '/');
#endif
}

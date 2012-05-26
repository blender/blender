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

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"

#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_utildefines.h"
#include "BKE_blender.h"    // BLENDER_VERSION

#include "GHOST_Path-api.h"

#if defined WIN32 && !defined _LIBC  || defined __sun
#  include "BLI_fnmatch.h" /* use fnmatch included in blenlib */
#else
#  ifndef _GNU_SOURCE
#    define _GNU_SOURCE
#  endif
#  include <fnmatch.h>
#endif

#ifdef WIN32
#include "utf_winfunc.h"
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
#endif /* WIN32 */

/* local */
#define UNIQUE_NAME_MAX 128

static char bprogname[FILE_MAX];    /* path to program executable */
static char bprogdir[FILE_MAX];     /* path in which executable is located */
static char btempdir[FILE_MAX];     /* temporary directory */

static int add_win32_extension(char *name);
static char *blender_version_decimal(const int ver);

/* implementation */

int BLI_stringdec(const char *string, char *head, char *tail, unsigned short *numlen)
{
	unsigned short len, len2, lenlslash = 0, nums = 0, nume = 0;
	short i, found = 0;
	char *lslash = BLI_last_slash(string);
	len2 = len = strlen(string);
	if (lslash)
		lenlslash = (int)(lslash - string);

	while (len > lenlslash && string[--len] != '.') {};
	if (len == lenlslash && string[len] != '.') len = len2;

	for (i = len - 1; i >= lenlslash; i--) {
		if (isdigit(string[i])) {
			if (found) {
				nums = i;
			}
			else {
				nume = i;
				nums = i;
				found = 1;
			}
		}
		else {
			if (found) break;
		}
	}
	if (found) {
		if (tail) strcpy(tail, &string[nume + 1]);
		if (head) {
			strcpy(head, string);
			head[nums] = 0;
		}
		if (numlen) *numlen = nume - nums + 1;
		return ((int)atoi(&(string[nums])));
	}
	if (tail) strcpy(tail, string + len);
	if (head) {
		strncpy(head, string, len);
		head[len] = '\0';
	}
	if (numlen) *numlen = 0;
	return 0;
}


void BLI_stringenc(char *string, const char *head, const char *tail, unsigned short numlen, int pic)
{
	char fmtstr[16] = "";
	if (pic < 0) pic = 0;
	sprintf(fmtstr, "%%s%%.%dd%%s", numlen);
	sprintf(string, fmtstr, head, pic, tail);
}

/* Foo.001 -> "Foo", 1
 * Returns the length of "Foo" */
int BLI_split_name_num(char *left, int *nr, const char *name, const char delim)
{
	int a;

	*nr = 0;
	a = strlen(name);
	memcpy(left, name, (a + 1) * sizeof(char));

	if (a > 1 && name[a - 1] == delim) return a;
	
	while (a--) {
		if (name[a] == delim) {
			left[a] = 0;
			*nr = atol(name + a + 1);
			/* casting down to an int, can overflow for large numbers */
			if (*nr < 0)
				*nr = 0;
			return a;
		}
		if (isdigit(name[a]) == 0) break;
		
		left[a] = 0;
	}

	for (a = 0; name[a]; a++)
		left[a] = name[a];

	return a;
}

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



int BLI_uniquename_cb(int (*unique_check)(void *, const char *), void *arg, const char defname[], char delim, char *name, short name_len)
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
			int numlen = BLI_snprintf(numstr, sizeof(numstr), "%c%03d", delim, ++number);

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
		
		return 1;
	}
	
	return 0;
}

/* little helper macro for BLI_uniquename */
#ifndef GIVE_STRADDR
	#define GIVE_STRADDR(data, offset) ( ((char *)data) + offset)
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
static int uniquename_find_dupe(ListBase *list, void *vlink, const char *name, short name_offs)
{
	Link *link;

	for (link = list->first; link; link = link->next) {
		if (link != vlink) {
			if (!strcmp(GIVE_STRADDR(link, name_offs), name)) {
				return 1;
			}
		}
	}

	return 0;
}

static int uniquename_unique_check(void *arg, const char *name)
{
	struct {ListBase *lb; void *vlink; short name_offs; } *data = arg;
	return uniquename_find_dupe(data->lb, data->vlink, name, data->name_offs);
}

void BLI_uniquename(ListBase *list, void *vlink, const char defname[], char delim, short name_offs, short name_len)
{
	struct {ListBase *lb; void *vlink; short name_offs; } data;
	data.lb = list;
	data.vlink = vlink;
	data.name_offs = name_offs;

	assert((name_len > 1) && (name_len <= UNIQUE_NAME_MAX));

	/* See if we are given an empty string */
	if (ELEM(NULL, vlink, defname))
		return;

	BLI_uniquename_cb(uniquename_unique_check, &data, defname, delim, GIVE_STRADDR(vlink, name_offs), name_len);
}



/* ******************** string encoding ***************** */

/* This is quite an ugly function... its purpose is to
 * take the dir name, make it absolute, and clean it up, replacing
 * excess file entry stuff (like /tmp/../tmp/../)
 * note that dir isn't protected for max string names... 
 * 
 * If relbase is NULL then its ignored
 */

void BLI_cleanup_path(const char *relabase, char *dir)
{
	ptrdiff_t a;
	char *start, *eind;
	if (relabase) {
		BLI_path_abs(dir, relabase);
	}
	else {
		if (dir[0] == '/' && dir[1] == '/') {
			if (dir[2] == '\0') {
				return; /* path is "//" - cant clean it */
			}
			dir = dir + 2; /* skip the first // */
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
	
	/* Note, this should really be moved to the file selector,
	 * since this function is used in many areas */
	if (strcmp(dir, ".") == 0) {  /* happens for example in FILE_MAIN */
		get_default_root(dir);
		return;
	}	

	while ( (start = strstr(dir, "\\..\\")) ) {
		eind = start + strlen("\\..\\") - 1;
		a = start - dir - 1;
		while (a > 0) {
			if (dir[a] == '\\') break;
			a--;
		}
		if (a < 0) {
			break;
		}
		else {
			memmove(dir + a, eind, strlen(eind) + 1);
		}
	}

	while ( (start = strstr(dir, "\\.\\")) ) {
		eind = start + strlen("\\.\\") - 1;
		memmove(start, eind, strlen(eind) + 1);
	}

	while ( (start = strstr(dir, "\\\\")) ) {
		eind = start + strlen("\\\\") - 1;
		memmove(start, eind, strlen(eind) + 1);
	}
#else
	if (dir[0] == '.') {  /* happens, for example in FILE_MAIN */
		dir[0] = '/';
		dir[1] = 0;
		return;
	}

	/* support for odd paths: eg /../home/me --> /home/me
	 * this is a valid path in blender but we cant handle this the usual way below
	 * simply strip this prefix then evaluate the path as usual. pythons os.path.normpath() does this */
	while ((strncmp(dir, "/../", 4) == 0)) {
		memmove(dir, dir + 4, strlen(dir + 4) + 1);
	}

	while ( (start = strstr(dir, "/../")) ) {
		eind = start + (4 - 1) /* strlen("/../") - 1 */;
		a = start - dir - 1;
		while (a > 0) {
			if (dir[a] == '/') break;
			a--;
		}
		if (a < 0) {
			break;
		}
		else {
			memmove(dir + a, eind, strlen(eind) + 1);
		}
	}

	while ( (start = strstr(dir, "/./")) ) {
		eind = start + (3 - 1) /* strlen("/./") - 1 */;
		memmove(start, eind, strlen(eind) + 1);
	}

	while ( (start = strstr(dir, "//")) ) {
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

void BLI_cleanup_file(const char *relabase, char *dir)
{
	BLI_cleanup_path(relabase, dir);
	BLI_del_slash(dir);
}

void BLI_path_rel(char *file, const char *relfile)
{
	char *lslash;
	char temp[FILE_MAX];
	char res[FILE_MAX];
	
	/* if file is already relative, bail out */
	if (file[0] == '/' && file[1] == '/') return;
	
	/* also bail out if relative path is not set */
	if (relfile[0] == 0) return;

#ifdef WIN32
	if (BLI_strnlen(relfile, 3) > 2 && relfile[1] != ':') {
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
		if (temp[1] == ':' && file[1] == ':' && temp[0] != file[0])
			return;
	}
#else
	BLI_strncpy(temp, relfile, FILE_MAX);
#endif

	BLI_char_switch(temp, '\\', '/');
	BLI_char_switch(file, '\\', '/');
	
	/* remove /./ which confuse the following slash counting... */
	BLI_cleanup_path(NULL, file);
	BLI_cleanup_path(NULL, temp);
	
	/* the last slash in the file indicates where the path part ends */
	lslash = BLI_last_slash(temp);

	if (lslash) {
		/* find the prefix of the filename that is equal for both filenames.
		 * This is replaced by the two slashes at the beginning */
		char *p = temp;
		char *q = file;

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
		
		strcpy(res, "//");

		/* p now points to the slash that is at the beginning of the part
		 * where the path is different from the relative path. 
		 * We count the number of directories we need to go up in the
		 * hierarchy to arrive at the common 'prefix' of the path
		 */
		while (p && p < lslash) {
			if (*p == '/') 
				strcat(res, "../");
			p++;
		}

		strcat(res, q + 1); /* don't copy the slash at the beginning */
		
#ifdef  WIN32
		BLI_char_switch(res + 2, '/', '\\');
#endif
		strcpy(file, res);
	}
}

int BLI_has_parent(char *path)
{
	int len;
	int slashes = 0;
	BLI_clean(path);
	len = BLI_add_slash(path) - 1;

	while (len >= 0) {
		if ((path[len] == '\\') || (path[len] == '/'))
			slashes++;
		len--;
	}
	return slashes > 1;
}

int BLI_parent_dir(char *path)
{
	static char parent_dir[] = {'.', '.', SEP, '\0'}; /* "../" or "..\\" */
	char tmp[FILE_MAX + 4];
	BLI_strncpy(tmp, path, sizeof(tmp) - 4);
	BLI_add_slash(tmp);
	strcat(tmp, parent_dir);
	BLI_cleanup_dir(NULL, tmp);

	if (!BLI_testextensie(tmp, parent_dir)) {
		BLI_strncpy(path, tmp, sizeof(tmp));	
		return 1;
	}
	else {
		return 0;
	}
}

static int stringframe_chars(char *path, int *char_start, int *char_end)
{
	int ch_sta, ch_end, i;
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
		return 1;
	}
	else {
		*char_start = -1;
		*char_end = -1;
		return 0;
	}
}

static void ensure_digits(char *path, int digits)
{
	char *file = BLI_last_slash(path);

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

int BLI_path_frame(char *path, int frame, int digits)
{
	int ch_sta, ch_end;

	if (digits)
		ensure_digits(path, digits);

	if (stringframe_chars(path, &ch_sta, &ch_end)) { /* warning, ch_end is the last # +1 */
		char tmp[FILE_MAX];
		sprintf(tmp, "%.*s%.*d%s", ch_sta, path, ch_end - ch_sta, frame, path + ch_end);
		strcpy(path, tmp);
		return 1;
	}
	return 0;
}

int BLI_path_frame_range(char *path, int sta, int end, int digits)
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
		return 1;
	}
	return 0;
}

int BLI_path_abs(char *path, const char *basepath)
{
	int wasrelative = (strncmp(path, "//", 2) == 0);
	char tmp[FILE_MAX];
	char base[FILE_MAX];
#ifdef WIN32
	char vol[3] = {'\0', '\0', '\0'};

	BLI_strncpy(vol, path, 3);
	/* we are checking here if we have an absolute path that is not in the current
	 * blend file as a lib main - we are basically checking for the case that a 
	 * UNIX root '/' is passed.
	 */
	if (!wasrelative && (vol[1] != ':' && (vol[0] == '\0' || vol[0] == '/' || vol[0] == '\\'))) {
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

	BLI_strncpy(base, basepath, sizeof(base));

	/* file component is ignored, so don't bother with the trailing slash */
	BLI_cleanup_path(NULL, base);
	
	/* push slashes into unix mode - strings entering this part are
	 * potentially messed up: having both back- and forward slashes.
	 * Here we push into one conform direction, and at the end we
	 * push them into the system specific dir. This ensures uniformity
	 * of paths and solving some problems (and prevent potential future
	 * ones) -jesterKing. */
	BLI_char_switch(tmp, '\\', '/');
	BLI_char_switch(base, '\\', '/');	

	/* Paths starting with // will get the blend file as their base,
	 * this isn't standard in any os but is used in blender all over the place */
	if (wasrelative) {
		char *lslash = BLI_last_slash(base);
		if (lslash) {
			int baselen = (int) (lslash - base) + 1;
			/* use path for temp storage here, we copy back over it right away */
			BLI_strncpy(path, tmp + 2, FILE_MAX);
			
			memcpy(tmp, base, baselen);
			BLI_strncpy(tmp + baselen, path, sizeof(tmp) - baselen);
			BLI_strncpy(path, tmp, FILE_MAX);
		}
		else {
			BLI_strncpy(path, tmp + 2, FILE_MAX);
		}
	}
	else {
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


/*
 * Should only be done with command line paths.
 * this is NOT something blenders internal paths support like the // prefix
 */
int BLI_path_cwd(char *path)
{
	int wasrelative = 1;
	int filelen = strlen(path);
	
#ifdef WIN32
	if (filelen >= 3 && path[1] == ':' && (path[2] == '\\' || path[2] == '/'))
		wasrelative = 0;
#else
	if (filelen >= 2 && path[0] == '/')
		wasrelative = 0;
#endif
	
	if (wasrelative == 1) {
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


/* 'di's filename component is moved into 'fi', di is made a dir path */
void BLI_splitdirstring(char *di, char *fi)
{
	char *lslash = BLI_last_slash(di);

	if (lslash) {
		BLI_strncpy(fi, lslash + 1, FILE_MAXFILE);
		*(lslash + 1) = 0;
	}
	else {
		BLI_strncpy(fi, di, FILE_MAXFILE);
		di[0] = 0;
	}
}

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
	const char *xdg_documents_dir= getenv("XDG_DOCUMENTS_DIR");

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

// #define PATH_DEBUG2

static char *blender_version_decimal(const int ver)
{
	static char version_str[5];
	sprintf(version_str, "%d.%02d", ver / 100, ver % 100);
	return version_str;
}

static int test_path(char *targetpath, const char *path_base, const char *path_sep, const char *folder_name)
{
	char tmppath[FILE_MAX];
	
	if (path_sep) BLI_join_dirfile(tmppath, sizeof(tmppath), path_base, path_sep);
	else BLI_strncpy(tmppath, path_base, sizeof(tmppath));

	/* rare cases folder_name is omitted (when looking for ~/.blender/2.xx dir only) */
	if (folder_name)
		BLI_make_file_string("/", targetpath, tmppath, folder_name);
	else
		BLI_strncpy(targetpath, tmppath, sizeof(tmppath));

	if (BLI_is_dir(targetpath)) {
#ifdef PATH_DEBUG2
		printf("\tpath found: %s\n", targetpath);
#endif
		return 1;
	}
	else {
#ifdef PATH_DEBUG2
		printf("\tpath missing: %s\n", targetpath);
#endif
		//targetpath[0] = '\0';
		return 0;
	}
}

static int test_env_path(char *path, const char *envvar)
{
	const char *env = envvar ? getenv(envvar) : NULL;
	if (!env) return 0;
	
	if (BLI_is_dir(env)) {
		BLI_strncpy(path, env, FILE_MAX);
		return 1;
	}
	else {
		path[0] = '\0';
		return 0;
	}
}

static int get_path_local(char *targetpath, const char *folder_name, const char *subfolder_name, const int ver)
{
	char relfolder[FILE_MAX];
	
#ifdef PATH_DEBUG2
	printf("get_path_local...\n");
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
	if (test_path(targetpath, bprogdir, blender_version_decimal(ver), relfolder))
		return 1;

	return 0;
}

static int is_portable_install(void)
{
	/* detect portable install by the existance of config folder */
	const int ver = BLENDER_VERSION;
	char path[FILE_MAX];

	return get_path_local(path, "config", NULL, ver);
}

static int get_path_user(char *targetpath, const char *folder_name, const char *subfolder_name, const char *envvar, const int ver)
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
			return 1;
		}
	}

	user_base_path = (const char *)GHOST_getUserDir(ver, blender_version_decimal(ver));
	if (user_base_path)
		BLI_strncpy(user_path, user_base_path, FILE_MAX);

	if (!user_path[0])
		return 0;
	
#ifdef PATH_DEBUG2
	printf("get_path_user: %s\n", user_path);
#endif
	
	if (subfolder_name) {
		/* try $HOME/folder_name/subfolder_name */
		return test_path(targetpath, user_path, folder_name, subfolder_name);
	}
	else {
		/* try $HOME/folder_name */
		return test_path(targetpath, user_path, NULL, folder_name);
	}
}

static int get_path_system(char *targetpath, const char *folder_name, const char *subfolder_name, const char *envvar, const int ver)
{
	char system_path[FILE_MAX];
	const char *system_base_path;


	/* first allow developer only overrides to the system path
	 * these are only used when running blender from source */
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

	/* try CWD/release/folder_name */
	if (BLI_current_working_dir(cwd, sizeof(cwd))) {
		if (test_path(targetpath, cwd, "release", relfolder)) {
			return 1;
		}
	}

	/* try EXECUTABLE_DIR/release/folder_name */
	if (test_path(targetpath, bprogdir, "release", relfolder))
		return 1;
	/* end developer overrides */



	system_path[0] = '\0';

	if (test_env_path(system_path, envvar)) {
		if (subfolder_name) {
			return test_path(targetpath, system_path, NULL, subfolder_name);
		}
		else {
			BLI_strncpy(targetpath, system_path, FILE_MAX);
			return 1;
		}
	}

	system_base_path = (const char *)GHOST_getSystemDir(ver, blender_version_decimal(ver));
	if (system_base_path)
		BLI_strncpy(system_path, system_base_path, FILE_MAX);
	
	if (!system_path[0])
		return 0;
	
#ifdef PATH_DEBUG2
	printf("get_path_system: %s\n", system_path);
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
char *BLI_get_folder(int folder_id, const char *subfolder)
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
	}
	
	return path;
}

char *BLI_get_user_folder_notest(int folder_id, const char *subfolder)
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
	}
	if ('\0' == path[0]) {
		return NULL;
	}
	return path;
}

char *BLI_get_folder_create(int folder_id, const char *subfolder)
{
	char *path;

	/* only for user folders */
	if (!ELEM4(folder_id, BLENDER_USER_DATAFILES, BLENDER_USER_CONFIG, BLENDER_USER_SCRIPTS, BLENDER_USER_AUTOSAVE))
		return NULL;
	
	path = BLI_get_folder(folder_id, subfolder);
	
	if (!path) {
		path = BLI_get_user_folder_notest(folder_id, subfolder);
		if (path) BLI_dir_create_recursive(path);
	}
	
	return path;
}

char *BLI_get_folder_version(const int id, const int ver, const int do_check)
{
	static char path[FILE_MAX] = "";
	int ok;
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
			ok = FALSE;
			BLI_assert(!"incorrect ID");
	}

	if ((ok == FALSE) && do_check) {
		return NULL;
	}

	return path;
}

/* End new stuff */
/* ************************************************************* */
/* ************************************************************* */



#ifdef PATH_DEBUG
#undef PATH_DEBUG
#endif

void BLI_setenv(const char *env, const char *val)
{
	/* free windows */
#if (defined(WIN32) || defined(WIN64)) && defined(FREE_WINDOWS)
	char *envstr = MEM_mallocN(sizeof(char) * (strlen(env) + strlen(val) + 2), "envstr"); /* one for = another for \0 */

	sprintf(envstr, "%s=%s", env, val);
	putenv(envstr);
	MEM_freeN(envstr);

	/* non-free windows */
#elif (defined(WIN32) || defined(WIN64)) /* not free windows */
	uputenv(env, val);


#else
	/* linux/osx/bsd */
	setenv(env, val, 1);
#endif
}


/**
 * Only set an env var if already not there.
 * Like Unix setenv(env, val, 0);
 */
void BLI_setenv_if_new(const char *env, const char *val)
{
	if (getenv(env) == NULL)
		BLI_setenv(env, val);
}


void BLI_clean(char *path)
{
	if (path == NULL) return;

#ifdef WIN32
	if (path && BLI_strnlen(path, 3) > 2) {
		BLI_char_switch(path + 2, '/', '\\');
	}
#else
	BLI_char_switch(path, '\\', '/');
#endif
}

void BLI_char_switch(char *string, char from, char to) 
{
	if (string == NULL) return;
	while (*string != 0) {
		if (*string == from) *string = to;
		string++;
	}
}

void BLI_make_exist(char *dir)
{
	int a;

	BLI_char_switch(dir, ALTSEP, SEP);

	a = strlen(dir);

	while (BLI_is_dir(dir) == 0) {
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

void BLI_make_existing_file(const char *name)
{
	char di[FILE_MAX], fi[FILE_MAXFILE];

	BLI_strncpy(di, name, sizeof(di));
	BLI_splitdirstring(di, fi);
	
	/* test exist */
	if (BLI_exists(di) == 0) {
		BLI_dir_create_recursive(di);
	}
}


void BLI_make_file_string(const char *relabase, char *string,  const char *dir, const char *file)
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
		
		lslash = BLI_last_slash(string);
		if (lslash) *(lslash + 1) = 0;

		dir += 2; /* Skip over the relative reference */
	}
#ifdef WIN32
	else {
		if (BLI_strnlen(dir, 3) >= 2 && dir[1] == ':') {
			BLI_strncpy(string, dir, 3);
			dir += 2;
		}
		else { /* no drive specified */
			   /* first option: get the drive from the relabase if it has one */
			if (relabase && strlen(relabase) >= 2 && relabase[1] == ':') {
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
	BLI_clean(string);
}

int BLI_testextensie(const char *str, const char *ext)
{
	short a, b;
	int retval;
	
	a = strlen(str);
	b = strlen(ext);
	
	if (a == 0 || b == 0 || b >= a) {
		retval = 0;
	}
	else if (BLI_strcasecmp(ext, str + a - b)) {
		retval = 0;	
	}
	else {
		retval = 1;
	}
	
	return (retval);
}

int BLI_testextensie_array(const char *str, const char **ext_array)
{
	int i = 0;
	while (ext_array[i]) {
		if (BLI_testextensie(str, ext_array[i])) {
			return 1;
		}

		i++;
	}
	return 0;
}

/* semicolon separated wildcards, eg:
 *  '*.zip;*.py;*.exe' */
int BLI_testextensie_glob(const char *str, const char *ext_fnmatch)
{
	const char *ext_step = ext_fnmatch;
	char pattern[16];

	while (ext_step[0]) {
		char *ext_next;
		int len_ext;

		if ((ext_next = strchr(ext_step, ';'))) {
			len_ext = (int)(ext_next - ext_step) + 1;
		}
		else {
			len_ext = sizeof(pattern);
		}

		BLI_strncpy(pattern, ext_step, len_ext);

		if (fnmatch(pattern, str, FNM_CASEFOLD) == 0) {
			return 1;
		}
		ext_step += len_ext;
	}

	return 0;
}


int BLI_replace_extension(char *path, size_t maxlen, const char *ext)
{
	size_t path_len = strlen(path);
	size_t ext_len = strlen(ext);
	ssize_t a;

	for (a = path_len - 1; a >= 0; a--) {
		if (ELEM3(path[a], '.', '/', '\\')) {
			break;
		}
	}

	if ((a < 0) || (path[a] != '.')) {
		a = path_len;
	}

	if (a + ext_len >= maxlen)
		return 0;

	memcpy(path + a, ext, ext_len + 1);
	return 1;
}

/* strip's trailing '.'s and adds the extension only when needed */
int BLI_ensure_extension(char *path, size_t maxlen, const char *ext)
{
	size_t path_len = strlen(path);
	size_t ext_len = strlen(ext);
	ssize_t a;

	/* first check the extension is alread there */
	if (    (ext_len <= path_len) &&
	        (strcmp(path + (path_len - ext_len), ext) == 0))
	{
		return 1;
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
		return 0;

	memcpy(path + a, ext, ext_len + 1);
	return 1;
}

/* Converts "/foo/bar.txt" to "/foo/" and "bar.txt"
 * - wont change 'string'
 * - wont create any directories
 * - dosnt use CWD, or deal with relative paths.
 * - Only fill's in *dir and *file when they are non NULL
 * */
void BLI_split_dirfile(const char *string, char *dir, char *file, const size_t dirlen, const size_t filelen)
{
	char *lslash_str = BLI_last_slash(string);
	size_t lslash = lslash_str ? (size_t)(lslash_str - string) + 1 : 0;

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

void BLI_split_dir_part(const char *string, char *dir, const size_t dirlen)
{
	BLI_split_dirfile(string, dir, NULL, dirlen, 0);
}

void BLI_split_file_part(const char *string, char *file, const size_t filelen)
{
	BLI_split_dirfile(string, NULL, file, 0, filelen);
}

/* simple appending of filename to dir, does not check for valid path! */
void BLI_join_dirfile(char *dst, const size_t maxlen, const char *dir, const char *file)
{
	size_t dirlen = BLI_strnlen(dir, maxlen);

	if (dst != dir) {
		if (dirlen == maxlen) {
			memcpy(dst, dir, dirlen);
			dst[dirlen - 1] = '\0';
			return; /* dir fills the path */
		}
		else {
			memcpy(dst, dir, dirlen + 1);
		}
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

	if (file == NULL) {
		return;
	}

	BLI_strncpy(dst + dirlen, file, maxlen - dirlen);
}

/* like pythons os.path.basename( ) */
char *BLI_path_basename(char *path)
{
	char *filename = BLI_last_slash(path);
	return filename ? filename + 1 : path;
}

/**
 * Produce image export path.
 *
 * Fails returning 0 if image filename is empty or if destination path
 * matches image path (i.e. both are the same file).
 *
 * Trailing slash in dest_dir is optional.
 *
 * Logic:
 *
 * - if an image is "below" current .blend file directory, rebuild the
 * same dir structure in dest_dir
 *
 * For example //textures/foo/bar.png becomes
 * [dest_dir]/textures/foo/bar.png.
 *
 * - if an image is not "below" current .blend file directory,
 * disregard it's path and copy it in the same directory where 3D file
 * goes.
 *
 * For example //../foo/bar.png becomes [dest_dir]/bar.png.
 *
 * This logic will help ensure that all image paths are relative and
 * that a user gets his images in one place. It'll also provide
 * consistent behavior across exporters.
 */
int BKE_rebase_path(char *abs, size_t abs_len, char *rel, size_t rel_len, const char *base_dir, const char *src_dir, const char *dest_dir)
{
	char path[FILE_MAX];
	char dir[FILE_MAX];
	char base[FILE_MAX];
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
		return 0;

	BLI_strncpy(path, src_dir, sizeof(path));

	/* expand "//" in filename and get absolute path */
	BLI_path_abs(path, base_dir);

	/* get the directory part */
	BLI_split_dirfile(path, dir, base, sizeof(dir), sizeof(base));

	len = strlen(blend_dir);

	rel_dir[0] = 0;

	/* if image is "below" current .blend file directory */
	if (!strncmp(path, blend_dir, len)) {

		/* if image is _in_ current .blend file directory */
		if (BLI_path_cmp(dir, blend_dir) == 0) {
			BLI_join_dirfile(dest_path, sizeof(dest_path), dest_dir, base);
		}
		/* "below" */
		else {
			/* rel = image_path_dir - blend_dir */
			BLI_strncpy(rel_dir, dir + len, sizeof(rel_dir));

			BLI_join_dirfile(dest_path, sizeof(dest_path), dest_dir, rel_dir);
			BLI_join_dirfile(dest_path, sizeof(dest_path), dest_path, base);
		}

	}
	/* image is out of current directory */
	else {
		BLI_join_dirfile(dest_path, sizeof(dest_path), dest_dir, base);
	}

	if (abs)
		BLI_strncpy(abs, dest_path, abs_len);

	if (rel) {
		strncat(rel, rel_dir, rel_len);
		strncat(rel, base, rel_len);
	}

	/* return 2 if src=dest */
	if (BLI_path_cmp(path, dest_path) == 0) {
		// if (G.debug & G_DEBUG) printf("%s and %s are the same file\n", path, dest_path);
		return 2;
	}

	return 1;
}

char *BLI_first_slash(char *string)
{
	char *ffslash, *fbslash;
	
	ffslash = strchr(string, '/');
	fbslash = strchr(string, '\\');
	
	if (!ffslash) return fbslash;
	else if (!fbslash) return ffslash;
	
	if ((intptr_t)ffslash < (intptr_t)fbslash) return ffslash;
	else return fbslash;
}

char *BLI_last_slash(const char *string)
{
	char *lfslash, *lbslash;
	
	lfslash = strrchr(string, '/');
	lbslash = strrchr(string, '\\');

	if (!lfslash) return lbslash; 
	else if (!lbslash) return lfslash;
	
	if ((intptr_t)lfslash < (intptr_t)lbslash) return lbslash;
	else return lfslash;
}

/* adds a slash if there isn't one there already */
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

/* removes a slash if there is one */
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
			printf("path can't be found: \"%.*s\"\n", maxlen, fullname);
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
					BLI_join_dirfile(fullname, maxlen, fullname, name);
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

const char *BLI_program_path(void)
{
	return bprogname;
}

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
 * \param fullname The full path to the temp directory
 * \param maxlen The size of the fullname buffer
 * \param userdir Directory specified in user preferences 
 */
static void BLI_where_is_temp(char *fullname, const size_t maxlen, char *userdir)
{
	fullname[0] = '\0';
	
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
}

void BLI_init_temporary_dir(char *userdir)
{
	BLI_where_is_temp(btempdir, FILE_MAX, userdir);
}

const char *BLI_temporary_dir(void)
{
	return btempdir;
}

void BLI_system_temporary_dir(char *dir)
{
	BLI_where_is_temp(dir, FILE_MAX, NULL);
}

#ifdef WITH_ICONV

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
		return;
	}
	*utf_8 = '\0';
	iconv_close(cd);
}
#endif // WITH_ICONV

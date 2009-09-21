/* util.c
 *
 * various string, file, list operations.
 *
 *
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
 */

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h> /* for log10 */

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_userdef_types.h"

#include "BLI_blenlib.h"
#include "BLI_storage.h"
#include "BLI_storage_types.h"
#include "BLI_dynamiclist.h"

#include "BLI_util.h"
#include "BKE_utildefines.h"




#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef WIN32 
#include <unistd.h>
#else
#include <io.h>
#endif

#ifdef WIN32

#ifdef _WIN32_IE
#undef _WIN32_IE
#endif
#define _WIN32_IE 0x0501
#include <windows.h>
#include <shlobj.h>

#include "BLI_winstuff.h"

#endif


#ifndef WIN32
#include <sys/time.h>
#endif

#ifdef __APPLE__
#include <sys/param.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

#ifdef __linux__
#include "binreloc.h"
#endif

/* local */

static int add_win32_extension(char *name);

/* implementation */

int BLI_stringdec(char *string, char *kop, char *start, unsigned short *numlen)
{
	unsigned short len, len2, nums = 0, nume = 0;
	short i, found = 0;

	len2 = len = strlen(string);
	
	if (len > 6) {
		if (BLI_strncasecmp(string + len - 6, ".blend", 6) == 0) len -= 6;
		else if (BLI_strncasecmp(string + len - 6, ".trace", 6) == 0) len -= 6;
	}
	
	if (len > 9) {
		if (BLI_strncasecmp(string + len - 9, ".blend.gz", 9) == 0) len -= 9;
	}
		
	if (len == len2) {
		if (len > 4) {
			/* handle .jf0 en .jf1 for jstreams */
			if (BLI_strncasecmp(string + len - 4, ".jf", 3) == 0) len -= 4;
			else if (BLI_strncasecmp(string + len - 4, ".tga", 4) == 0) len -= 4;
			else if (BLI_strncasecmp(string + len - 4, ".jpg", 4) == 0) len -= 4;
			else if (BLI_strncasecmp(string + len - 4, ".png", 4) == 0) len -= 4;
			else if (BLI_strncasecmp(string + len - 4, ".txt", 4) == 0) len -= 4;
			else if (BLI_strncasecmp(string + len - 4, ".cyc", 4) == 0) len -= 4;
			else if (BLI_strncasecmp(string + len - 4, ".enh", 4) == 0) len -= 4;
			else if (BLI_strncasecmp(string + len - 4, ".rgb", 4) == 0) len -= 4;
			else if (BLI_strncasecmp(string + len - 4, ".psx", 4) == 0) len -= 4;
			else if (BLI_strncasecmp(string + len - 4, ".ble", 4) == 0) len -= 4;
			else if (BLI_strncasecmp(string + len - 4, ".exr", 4) == 0) len -= 4;
		}
	}
	
	for (i = len - 1; i >= 0; i--) {
		if (string[i] == '/') break;
		if (isdigit(string[i])) {
			if (found){
				nums = i;
			}
			else{
				nume = i;
				nums = i;
				found = 1;
			}
		}
		else {
			if (found) break;
		}
	}
	if (found){
		if (start) strcpy(start,&string[nume+1]);
		if (kop) {
			strcpy(kop,string);
			kop[nums]=0;
		}
		if (numlen) *numlen = nume-nums+1;
		return ((int)atoi(&(string[nums])));
	}
	if (start) strcpy(start, string + len);
	if (kop) {
		strncpy(kop, string, len);
		kop[len] = 0;
	}
	if (numlen) *numlen=0;
	return 0;
}


void BLI_stringenc(char *string, char *kop, char *start, unsigned short numlen, int pic)
{
	char numstr[10]="";
	unsigned short len,i;

	strcpy(string,kop);
	
	if (pic>0 || numlen==4) {
		len= sprintf(numstr,"%d",pic);
		
		for(i=len;i<numlen;i++){
			strcat(string,"0");
		}
		strcat(string,numstr);
	}
	strcat(string, start);
}


void BLI_newname(char *name, int add)
{
	char head[128], tail[128];
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
	
	if (digits==4 && pic<0) pic= 0;
	BLI_stringenc(name, head, tail, digits, pic);
}

/* little helper macro for BLI_uniquename */
#ifndef GIVE_STRADDR
	#define GIVE_STRADDR(data, offset) ( ((char *)data) + offset )
#endif

/* Generic function to set a unique name. It is only designed to be used in situations
 * where the name is part of the struct, and also that the name is at most 128 chars long.
 * 
 * For places where this is used, see constraint.c for example...
 *
 * 	name_offs: should be calculated using offsetof(structname, membername) macro from stddef.h
 *	len: maximum length of string (to prevent overflows, etc.)
 *	defname: the name that should be used by default if none is specified already
 *	delim: the character which acts as a delimeter between parts of the name
 */
void BLI_uniquename(ListBase *list, void *vlink, char defname[], char delim, short name_offs, short len)
{
	Link *link;
	char tempname[128];
	int	number = 1, exists = 0;
	char *dot;
	
	/* Make sure length can be handled */
	if ((len < 0) || (len > 128))
		return;
	
	/* See if we are given an empty string */
	if (ELEM(NULL, vlink, defname))
		return;
	
	if (GIVE_STRADDR(vlink, name_offs) == '\0') {
		/* give it default name first */
		BLI_strncpy(GIVE_STRADDR(vlink, name_offs), defname, len);
	}
	
	/* See if we even need to do this */
	if (list == NULL)
		return;
	
	for (link = list->first; link; link= link->next) {
		if (link != vlink) {
			if (!strcmp(GIVE_STRADDR(link, name_offs), GIVE_STRADDR(vlink, name_offs))) {
				exists = 1;
				break;
			}
		}
	}
	if (exists == 0)
		return;

	/* Strip off the suffix */
	dot = strchr(GIVE_STRADDR(vlink, name_offs), delim);
	if (dot)
		*dot=0;
	
	for (number = 1; number <= 999; number++) {
		BLI_snprintf(tempname, 128, "%s%c%03d", GIVE_STRADDR(vlink, name_offs), delim, number);
		
		exists = 0;
		for (link= list->first; link; link= link->next) {
			if (vlink != link) {
				if (!strcmp(GIVE_STRADDR(link, name_offs), tempname)) {
					exists = 1;
					break;
				}
			}
		}
		if (exists == 0) {
			BLI_strncpy(GIVE_STRADDR(vlink, name_offs), tempname, len);
			return;
		}
	}
}

/* ******************** string encoding ***************** */

/* This is quite an ugly function... its purpose is to
 * take the dir name, make it absolute, and clean it up, replacing
 * excess file entry stuff (like /tmp/../tmp/../)
 * note that dir isn't protected for max string names... 
 * 
 * If relbase is NULL then its ignored
 */

void BLI_cleanup_dir(const char *relabase, char *dir)
{
	BLI_cleanup_file(relabase, dir);
	BLI_add_slash(dir);

}

void BLI_cleanup_file(const char *relabase, char *dir)
{
	short a;
	char *start, *eind;
	if (relabase) {
		BLI_convertstringcode(dir, relabase);
	} else {
		if (dir[0]=='/' && dir[1]=='/') {
			if (dir[2]== '\0') {
				return; /* path is "//" - cant clean it */
			}
			dir = dir+2; /* skip the first // */
		}
	}
	
	/* Note
	 *   memmove( start, eind, strlen(eind)+1 );
	 * is the same as
	 *   strcpy( start, eind ); 
	 * except strcpy should not be used because there is overlap,
	  * so use memmove's slightly more obscure syntax - Campbell
	 */
	
#ifdef WIN32
	
	/* Note, this should really be moved to the file selector,
	 * since this function is used in many areas */
	if(strcmp(dir, ".")==0) {	/* happens for example in FILE_MAIN */
	   get_default_root(dir);
	   return;
	}	

	while ( (start = strstr(dir, "\\..\\")) ) {
		eind = start + strlen("\\..\\") - 1;
		a = start-dir-1;
		while (a>0) {
			if (dir[a] == '\\') break;
			a--;
		}
		if (a<0) {
			break;
		} else {
			memmove( dir+a, eind, strlen(eind)+1 );
		}
	}

	while ( (start = strstr(dir,"\\.\\")) ){
		eind = start + strlen("\\.\\") - 1;
		memmove( start, eind, strlen(eind)+1 );
	}

	while ( (start = strstr(dir,"\\\\" )) ){
		eind = start + strlen("\\\\") - 1;
		memmove( start, eind, strlen(eind)+1 );
	}

	if((a = strlen(dir))){				/* remove the '\\' at the end */
		while(a>0 && dir[a-1] == '\\'){
			a--;
			dir[a] = 0;
		}
	}
#else
	if(dir[0]=='.') {	/* happens, for example in FILE_MAIN */
	   dir[0]= '/';
	   dir[1]= 0;
	   return;
	}	

	while ( (start = strstr(dir, "/../")) ) {
		eind = start + strlen("/../") - 1;
		a = start-dir-1;
		while (a>0) {
			if (dir[a] == '/') break;
			a--;
		}
		if (a<0) {
			break;
		} else {
			memmove( dir+a, eind, strlen(eind)+1 );
		}
	}

	while ( (start = strstr(dir,"/./")) ){
		eind = start + strlen("/./") - 1;
		memmove( start, eind, strlen(eind)+1 );
	}

	while ( (start = strstr(dir,"//" )) ){
		eind = start + strlen("//") - 1;
		memmove( start, eind, strlen(eind)+1 );
	}

	if( (a = strlen(dir)) ){				/* remove all '/' at the end */
		while(dir[a-1] == '/'){
			a--;
			dir[a] = 0;
			if (a<=0) break;
		}
	}
#endif
}


void BLI_makestringcode(const char *relfile, char *file)
{
	char * p;
	char * q;
	char * lslash;
	char temp[FILE_MAXDIR+FILE_MAXFILE];
	char res[FILE_MAXDIR+FILE_MAXFILE];
	
	/* if file is already relative, bail out */
	if(file[0]=='/' && file[1]=='/') return;
	
	/* also bail out if relative path is not set */
	if (relfile[0] == 0) return;

#ifdef WIN32 
	if (strlen(relfile) > 2 && relfile[1] != ':') {
		char* ptemp;
		/* fix missing volume name in relative base,
		   can happen with old .Blog files */
		get_default_root(temp);
		ptemp = &temp[2];
		if (relfile[0] != '\\' && relfile[0] != '/') {
			ptemp++;
		}
		BLI_strncpy(ptemp, relfile, FILE_MAXDIR + FILE_MAXFILE-3);
	} else {
		BLI_strncpy(temp, relfile, FILE_MAXDIR + FILE_MAXFILE);
	}

	if (strlen(file) > 2) {
		if ( temp[1] == ':' && file[1] == ':' && temp[0] != file[0] )
			return;
	}
#else
	BLI_strncpy(temp, relfile, FILE_MAX);
#endif

	BLI_char_switch(temp, '\\', '/');
	BLI_char_switch(file, '\\', '/');
	
	/* remove /./ which confuse the following slash counting... */
	BLI_cleanup_file(NULL, file);
	BLI_cleanup_file(NULL, temp);
	
	/* the last slash in the file indicates where the path part ends */
	lslash = BLI_last_slash(temp);

	if (lslash) 
	{	
		/* find the prefix of the filename that is equal for both filenames.
		   This is replaced by the two slashes at the beginning */
		p = temp;
		q = file;
		while (*p == *q) {
			++p; ++q;
		}
		/* we might have passed the slash when the beginning of a dir matches 
		   so we rewind. Only check on the actual filename
		*/
		if (*q != '/') {
			while ( (q >= file) && (*q != '/') ) { --q; --p; }
		} 
		else if (*p != '/') {
			while ( (p >= temp) && (*p != '/') ) { --p; --q; }
		}
		
		strcpy(res,	"//");

		/* p now points to the slash that is at the beginning of the part
		   where the path is different from the relative path. 
		   We count the number of directories we need to go up in the
		   hierarchy to arrive at the common 'prefix' of the path
		*/			
		while (p && p < lslash)	{
			if (*p == '/') 
				strcat(res,	"../");
			++p;
		}

		strcat(res, q+1); /* don't copy the slash at the beginning */
		
#ifdef	WIN32
		BLI_char_switch(res+2, '/', '\\');
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

	while (len>=0) {
		if ((path[len] == '\\') || (path[len] == '/'))
			slashes++;
		len--;
	}
	return slashes > 1;
}

int BLI_parent_dir(char *path)
{
#ifdef WIN32
	static char *parent_dir="..\\";
#else
	static char *parent_dir="../";
#endif
	char tmp[FILE_MAXDIR+FILE_MAXFILE+4];
	BLI_strncpy(tmp, path, sizeof(tmp));
	BLI_add_slash(tmp);
	strcat(tmp, parent_dir);
	BLI_cleanup_dir(NULL, tmp);
 	
	if (!BLI_testextensie(tmp, parent_dir)) {
		BLI_strncpy(path, tmp, sizeof(tmp));	
		return 1;
	} else {
		return 0;
	}
}

int BLI_convertstringframe(char *path, int frame)
{
	int ch_sta, ch_end, i;
	/* Insert current frame: file### -> file001 */
	ch_sta = ch_end = 0;
	for (i = 0; path[i] != '\0'; i++) {
		if (path[i] == '\\' || path[i] == '/') {
			ch_end = 0; /* this is a directory name, dont use any hashes we found */
		} else if (path[i] == '#') {
			ch_sta = i;
			ch_end = ch_sta+1;
			while (path[ch_end] == '#') {
				ch_end++;
			}
			i = ch_end-1; /* keep searching */
			
			/* dont break, there may be a slash after this that invalidates the previous #'s */
		}
	}
	if (ch_end) { /* warning, ch_end is the last # +1 */
		/* Add the frame number? */
		short numlen, hashlen;
		char tmp[FILE_MAX];
		
		char format[16]; /* 6 is realistically the maxframe (300000), so 8 should be enough, but 16 to be safe. */
		if (((ch_end-1)-ch_sta) >= 16) {
			ch_end = ch_sta+15; /* disallow values longer then 'format' can hold */
		}
		
		strcpy(tmp, path);
		
		numlen = 1 + (int)log10((double)frame); /* this is the number of chars in the number */
		hashlen = ch_end - ch_sta;
		
		sprintf(format, "%d", frame);
		
		if (numlen==hashlen) { /* simple case */
			memcpy(tmp+ch_sta, format, numlen);
		} else if (numlen < hashlen) {
			memcpy(tmp+ch_sta + (hashlen-numlen), format, numlen); /*dont copy the string terminator */
			memset(tmp+ch_sta, '0', hashlen-numlen);
		} else {
			/* number is longer then number of #'s */
			if (tmp[ch_end] == '\0') { /* hashes are last, no need to move any string*/
				/* bad juju - not testing string length here :/ */
				memcpy(tmp+ch_sta, format, numlen+1); /* add 1 to get the string terminator \0 */
			} else {
				/* we need to move the end characters, reuse i */
				int j;
				
				i = strlen(tmp); /* +1 to copy the string terminator */
				j = i + (numlen-hashlen); /* from/to */
				
				while (i >= ch_end) {
					tmp[j] = tmp[i]; 
					i--;
					j--;
				}
				memcpy(tmp + ch_sta, format, numlen);
			}
		}	
		strcpy(path, tmp);
		return 1;
	}
	return 0;
}


int BLI_convertstringcode(char *path, const char *basepath)
{
	int wasrelative = (strncmp(path, "//", 2)==0);
	char tmp[FILE_MAX];
	char base[FILE_MAX];
#ifdef WIN32
	char vol[3] = {'\0', '\0', '\0'};

	BLI_strncpy(vol, path, 3);
	/* we are checking here if we have an absolute path that is not in the current
	   blend file as a lib main - we are basically checking for the case that a 
	   UNIX root '/' is passed.
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
	BLI_strncpy(tmp, path, FILE_MAX);
	
	/* Check for loading a windows path on a posix system
	 * in this case, there is no use in trying C:/ since it 
	 * will never exist on a unix os.
	 * 
	 * Add a / prefix and lowercase the driveletter, remove the :
	 * C:\foo.JPG -> /c/foo.JPG */
	
	if (isalpha(tmp[0]) && tmp[1] == ':' && (tmp[2]=='\\' || tmp[2]=='/') ) {
		tmp[1] = tolower(tmp[0]); /* replace ':' with driveletter */
		tmp[0] = '/'; 
		/* '\' the slash will be converted later */
	}
	
#endif

	BLI_strncpy(base, basepath, FILE_MAX);
	
	BLI_cleanup_file(NULL, base);
	
	/* push slashes into unix mode - strings entering this part are
	   potentially messed up: having both back- and forward slashes.
	   Here we push into one conform direction, and at the end we
	   push them into the system specific dir. This ensures uniformity
	   of paths and solving some problems (and prevent potential future
	   ones) -jesterKing. */
	BLI_char_switch(tmp, '\\', '/');
	BLI_char_switch(base, '\\', '/');	

	/* Paths starting with // will get the blend file as their base,
	 * this isnt standard in any os but is uesed in blender all over the place */
	if (wasrelative) {
		char *lslash= BLI_last_slash(base);
		if (lslash) {
			int baselen= (int) (lslash-base) + 1;
			/* use path for for temp storage here, we copy back over it right away */
			BLI_strncpy(path, tmp+2, FILE_MAX);
			
			memcpy(tmp, base, baselen);
			strcpy(tmp+baselen, path);
			strcpy(path, tmp);
		} else {
			strcpy(path, tmp+2);
		}
	} else {
		strcpy(path, tmp);
	}
	
	if (path[0]!='\0') {
		if ( path[strlen(path)-1]=='/') {
			BLI_cleanup_dir(NULL, path);
		} else {
			BLI_cleanup_file(NULL, path);
		}
	}
	
#ifdef WIN32
	/* skip first two chars, which in case of
	   absolute path will be drive:/blabla and
	   in case of relpath //blabla/. So relpath
	   // will be retained, rest will be nice and
	   shiny win32 backward slashes :) -jesterKing
	*/
	BLI_char_switch(path+2, '/', '\\');
#endif
	
	return wasrelative;
}


/*
 * Should only be done with command line paths.
 * this is NOT somthing blenders internal paths support like the // prefix
 */
int BLI_convertstringcwd(char *path)
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
	
	if (wasrelative==1) {
		char cwd[FILE_MAXDIR + FILE_MAXFILE];
		BLI_getwdN(cwd); /* incase the full path to the blend isnt used */
		
		if (cwd[0] == '\0') {
			printf( "Could not get the current working directory - $PWD for an unknown reason.");
		} else {
			/* uses the blend path relative to cwd important for loading relative linked files.
			*
			* cwd should contain c:\ etc on win32 so the relbase can be NULL
			* relbase being NULL also prevents // being misunderstood as relative to the current
			* blend file which isnt a feature we want to use in this case since were dealing
			* with a path from the command line, rather then from inside Blender */
			
			char origpath[FILE_MAXDIR + FILE_MAXFILE];
			BLI_strncpy(origpath, path, FILE_MAXDIR + FILE_MAXFILE);
			
			BLI_make_file_string(NULL, path, cwd, origpath); 
		}
	}
	
	return wasrelative;
}


/* copy di to fi, filename only */
void BLI_splitdirstring(char *di, char *fi)
{
	char *lslash= BLI_last_slash(di);

	if (lslash) {
		BLI_strncpy(fi, lslash+1, FILE_MAXFILE);
		*(lslash+1)=0;
	} else {
		BLI_strncpy(fi, di, FILE_MAXFILE);
		di[0]= 0;
	}
}

void BLI_getlastdir(const char* dir, char *last, int maxlen)
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
		BLI_strncpy(last, prevslash+1, maxlen);
	} else {
		BLI_strncpy(last, dir, maxlen);
	}
}

char *BLI_gethome(void) {
	#if !defined(WIN32)
		return getenv("HOME");

	#else /* Windows */
		char * ret;
		static char dir[512];
		static char appdatapath[MAXPATHLEN];
		HRESULT hResult;

		/* Check for %HOME% env var */

		ret = getenv("HOME");
		if(ret) {
			sprintf(dir, "%s\\.blender", ret);
			if (BLI_exists(dir)) return dir;
		}

		/* else, check install dir (path containing blender.exe) */

		BLI_getInstallationDir(dir);

		if (BLI_exists(dir))
		{
			strcat(dir,"\\.blender");
			if (BLI_exists(dir)) return(dir);
		}

				
		/* add user profile support for WIN 2K / NT */
		hResult = SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, appdatapath);
		
		if (hResult == S_OK)
		{
			if (BLI_exists(appdatapath)) { /* from fop, also below... */
				sprintf(dir, "%s\\Blender Foundation\\Blender", appdatapath);
				BLI_recurdir_fileops(dir);
				if (BLI_exists(dir)) {
					strcat(dir,"\\.blender");
					if(BLI_exists(dir)) return(dir);
				}
			}
			hResult = SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, SHGFP_TYPE_CURRENT, appdatapath);
			if (hResult == S_OK)
			{
				if (BLI_exists(appdatapath)) 
				{ /* from fop, also below... */
					sprintf(dir, "%s\\Blender Foundation\\Blender", appdatapath);
					BLI_recurdir_fileops(dir);
					if (BLI_exists(dir)) {
						strcat(dir,"\\.blender");
						if(BLI_exists(dir)) return(dir);
					}
				}
			}
		}
#if 0
		ret = getenv("USERPROFILE");
		if (ret) {
			if (BLI_exists(ret)) { /* from fop, also below...  */
				sprintf(dir, "%s\\Application Data\\Blender Foundation\\Blender", ret);
				BLI_recurdir_fileops(dir);
				if (BLI_exists(dir)) {
					strcat(dir,"\\.blender");
					if(BLI_exists(dir)) return(dir);
				}
			}
		}
#endif

		/* 
		   Saving in the Windows dir is less than desirable. 
		   Use as a last resort ONLY! (aphex)
		*/
		
		ret = getenv("WINDOWS");		
		if (ret) {
			if(BLI_exists(ret)) return ret;
		}

		ret = getenv("WINDIR");	
		if (ret) {
			if(BLI_exists(ret)) return ret;
		}
		
		return "C:\\Temp";	/* sheesh! bad, bad, bad! (aphex) */
	#endif
}

/* this function returns the path to a blender folder, if it exists
 * utility functions for BLI_gethome_folder */

/* #define PATH_DEBUG */ /* for testing paths that are checked */

static int test_data_path(char *targetpath, char *path_base, char *path_sep, char *folder_name)
{
	char tmppath[FILE_MAXDIR];
	
	if(path_sep)	BLI_join_dirfile(tmppath, path_base, path_sep);
	else			BLI_strncpy(tmppath, path_base, sizeof(tmppath));
	
	BLI_make_file_string("/", targetpath, tmppath, folder_name);
	
	if (BLI_exists(targetpath)) {
#ifdef PATH_DEBUG
		printf("\tpath found: %s\n", targetpath);
#endif
		return 1;
	}
	else {
#ifdef PATH_DEBUG
		printf("\tpath missing: %s\n", targetpath);
#endif
		targetpath[0] = '\0';
		return 0;
	}
}

static int gethome_path_local(char *targetpath, char *folder_name)
{
	extern char bprogname[]; /* argv[0] from creator.c */
	char bprogdir[FILE_MAXDIR];
	char cwd[FILE_MAXDIR];
	char *s;
	int i;
	
#ifdef PATH_DEBUG
	printf("gethome_path_local...\n");
#endif
	
	/* try release/folder_name (binary relative) */
	/* use argv[0] (bprogname) to get the path to the executable */
	s = BLI_last_slash(bprogname);
	i = s - bprogname + 1;
	BLI_strncpy(bprogdir, bprogname, i);
	
	/* try ./.blender/folder_name */
	if(test_data_path(targetpath, bprogdir, ".blender", folder_name))
		return 1;
	
	if(test_data_path(targetpath, bprogdir, "release", folder_name))
		return 1;
	
	/* try release/folder_name (CWD relative) */
	if(test_data_path(targetpath, BLI_getwdN(cwd), "release", folder_name))
		return 1;
	
	return 0;
}

static int gethome_path_user(char *targetpath, char *folder_name)
{
	char *home_path= BLI_gethome();

#ifdef PATH_DEBUG
	printf("gethome_path_user...\n");
#endif
	
	/* try $HOME/folder_name */
	return test_data_path(targetpath, home_path, ".blender", folder_name);
}

static int gethome_path_system(char *targetpath, char *folder_name)
{
	extern char blender_path[]; /* unix prefix eg. /usr/share/blender/2.5 creator.c */
	
	if(!blender_path[0])
		return 0;
	
#ifdef PATH_DEBUG
	printf("gethome_path_system...\n");
#endif
	
	/* try $BLENDERPATH/folder_name */
	return test_data_path(targetpath, blender_path, NULL, folder_name);
}

char *BLI_gethome_folder(char *folder_name, int flag)
{
	static char fulldir[FILE_MAXDIR] = "";
	
	/* first check if this is a redistributable bundle */
	if(flag & BLI_GETHOME_LOCAL) {
		if (gethome_path_local(fulldir, folder_name))
			return fulldir;
	}

	/* then check if the OS has blender data files installed in a global location */
	if(flag & BLI_GETHOME_SYSTEM) {
		if (gethome_path_system(fulldir, folder_name))
			return fulldir;
	}
	
	/* now check the users home dir for data files */
	if(flag & BLI_GETHOME_USER) {
		if (gethome_path_user(fulldir, folder_name))
			return fulldir;
	}
	
	return NULL;
}

#ifdef PATH_DEBUG
#undef PATH_DEBUG
#endif

void BLI_setenv(const char *env, const char*val)
{
	/* SGI or free windows */
#if (defined(__sgi) || ((defined(WIN32) || defined(WIN64)) && defined(FREE_WINDOWS)))
	char *envstr= malloc(sizeof(char) * (strlen(env) + strlen(val) + 2)); /* one for = another for \0 */

	sprintf(envstr, "%s=%s", env, val);
	putenv(envstr);
	free(envstr);

	/* non-free windows */
#elif (defined(WIN32) || defined(WIN64)) /* not free windows */
	_putenv_s(env, val);
#else
	/* linux/osx/bsd */
	setenv(env, val, 1);
#endif
}

void BLI_clean(char *path)
{
	if(path==0) return;
#ifdef WIN32
	if(path && strlen(path)>2) {
		BLI_char_switch(path+2, '/', '\\');
	}
#else
	BLI_char_switch(path, '\\', '/');
#endif
}

void BLI_char_switch(char *string, char from, char to) 
{
	if(string==0) return;
	while (*string != 0) {
		if (*string == from) *string = to;
		string++;
	}
}

void BLI_make_exist(char *dir) {
	int a;

	#ifdef WIN32
		BLI_char_switch(dir, '/', '\\');
	#else
		BLI_char_switch(dir, '\\', '/');
	#endif	
	
	a = strlen(dir);
	
#ifdef WIN32	
	while(BLI_exists(dir) == 0){
		a --;
		while(dir[a] != '\\'){
			a--;
			if (a <= 0) break;
		}
		if (a >= 0) dir[a+1] = 0;
		else {
			/* defaulting to drive (usually 'C:') of Windows installation */
			get_default_root(dir);
			break;
		}
	}
#else
	while(BLI_exist(dir) == 0){
		a --;
		while(dir[a] != '/'){
			a--;
			if (a <= 0) break;
		}
		if (a >= 0) dir[a+1] = 0;
		else {
			strcpy(dir,"/");
			break;
		}
	}
#endif
}

void BLI_make_existing_file(char *name)
{
	char di[FILE_MAXDIR], fi[FILE_MAXFILE];
	
	strcpy(di, name);
	BLI_splitdirstring(di, fi);
	
	/* test exist */
	if (BLI_exists(di) == 0) {
		BLI_recurdir_fileops(di);
	}
}


void BLI_make_file_string(const char *relabase, char *string,  const char *dir, const char *file)
{
	int sl;

	if (!string || !dir || !file) return; /* We don't want any NULLs */
	
	string[0]= 0; /* ton */

	/* we first push all slashes into unix mode, just to make sure we don't get
	   any mess with slashes later on. -jesterKing */
	/* constant strings can be passed for those parameters - don't change them - elubie */
	/*
	BLI_char_switch(relabase, '\\', '/');
	BLI_char_switch(dir, '\\', '/');
	BLI_char_switch(file, '\\', '/');
	*/

	/* Resolve relative references */	
	if (relabase && dir[0] == '/' && dir[1] == '/') {
		char *lslash;
		
		/* Get the file name, chop everything past the last slash (ie. the filename) */
		strcpy(string, relabase);
		
		lslash= (strrchr(string, '/')>strrchr(string, '\\'))?strrchr(string, '/'):strrchr(string, '\\');
		
		if(lslash) *(lslash+1)= 0;

		dir+=2; /* Skip over the relative reference */
	}
#ifdef WIN32
	else {
		if (strlen(dir) >= 2 && dir[1] == ':' ) {
			BLI_strncpy(string, dir, 3);
			dir += 2;
		}
		else { /* no drive specified */
			/* first option: get the drive from the relabase if it has one */
			if (relabase && strlen(relabase) >= 2 && relabase[1] == ':' ) {
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
	while (sl>0 && ( string[sl-1] == '/' || string[sl-1] == '\\') ) {
		string[sl-1] = '\0';
		sl--;
	}
	/* since we've now removed all slashes, put back one slash at the end. */
	strcat(string, "/");
	
	while (*file && (*file == '/' || *file == '\\')) /* Trim slashes from the front of file */
		file++;
		
	strcat (string, file);
	
	/* Push all slashes to the system preferred direction */
	BLI_clean(string);
}

int BLI_testextensie(const char *str, const char *ext)
{
	short a, b;
	int retval;

	a= strlen(str);
	b= strlen(ext);

	if(a==0 || b==0 || b>=a) {
		retval = 0;
	} else if (BLI_strcasecmp(ext, str + a - b)) {
		retval = 0;	
	} else {
		retval = 1;
	}

	return (retval);
}

/*
 * This is a simple version of BLI_split_dirfile that has the following advantages...
 * 
 * Converts "/foo/bar.txt" to "/foo/" and "bar.txt"
 * - wont change 'string'
 * - wont create any directories
 * - dosnt use CWD, or deal with relative paths.
 * - Only fill's in *dir and *file when they are non NULL
 * */
void BLI_split_dirfile_basic(const char *string, char *dir, char *file)
{
	int lslash=0, i = 0;
	for (i=0; string[i]!='\0'; i++) {
		if (string[i]=='\\' || string[i]=='/')
			lslash = i+1;
	}
	if (dir) {
		if (lslash) {
			BLI_strncpy( dir, string, lslash+1); /* +1 to include the slash and the last char */
		} else {
			dir[0] = '\0';
		}
	}
	
	if (file) {
		strcpy( file, string+lslash);
	}
}


/* Warning,
 * - May modify 'string' variable
 * - May create the directory if it dosnt exist
 * if this is not needed use BLI_split_dirfile_basic(...)
 */
void BLI_split_dirfile(char *string, char *dir, char *file)
{
	int a;
#ifdef WIN32
	int sl;
	short is_relative = 0;
	char path[FILE_MAX];
#endif

	dir[0]= 0;
	file[0]= 0;

#ifdef WIN32
	BLI_strncpy(path, string, FILE_MAX);
	BLI_char_switch(path, '/', '\\'); /* make sure we have a valid path format */
	sl = strlen(path);
	if (sl) {
		int len;
		if (path[0] == '/' || path[0] == '\\') { 
			BLI_strncpy(dir, path, FILE_MAXDIR);
			if (sl > 1 && path[0] == '\\' && path[1] == '\\') is_relative = 1;
		} else if (sl > 2 && path[1] == ':' && path[2] == '\\') {
			BLI_strncpy(dir, path, FILE_MAXDIR);
		} else {
			BLI_getwdN(dir);
			strcat(dir,"\\");
			strcat(dir,path);
			BLI_strncpy(path,dir,FILE_MAXDIR+FILE_MAXFILE);
		}
		
		// BLI_exist doesn't recognize a slashed dirname as a dir
		//  check if a trailing slash exists, and remove it. Do not do this
		//  when we are already at root. -jesterKing
		a = strlen(dir);
		if(a>=4 && dir[a-1]=='\\') dir[a-1] = 0;

		if (is_relative) {
			printf("WARNING: BLI_split_dirfile needs absolute dir\n");
		}
		else {
			BLI_make_exist(dir);
		}

		if (S_ISDIR(BLI_exist(dir))) {

			/* copy from end of string into file, to ensure filename itself isn't truncated 
			if string is too long. (aphex) */

			len = FILE_MAXFILE - strlen(path);

			if (len < 0)
				BLI_strncpy(file,path + abs(len),FILE_MAXFILE);
			else
				BLI_strncpy(file,path,FILE_MAXFILE);
		    
			if (strrchr(path,'\\')) {
				BLI_strncpy(file,strrchr(path,'\\')+1,FILE_MAXFILE);
			}
			
			if ( (a = strlen(dir)) ) {
				if (dir[a-1] != '\\') strcat(dir,"\\");
			}
		}
		else {
			a = strlen(dir) - 1;
			while(a>0 && dir[a] != '\\') a--;
			dir[a + 1] = 0;
			BLI_strncpy(file, path + strlen(dir),FILE_MAXFILE);
		}

	}
	else {
		/* defaulting to first valid drive hoping it's not empty CD and DVD drives */
		get_default_root(dir);
		file[0]=0;
	}
#else
	if (strlen(string)) {
		if (string[0] == '/') { 
			strcpy(dir, string);
		} else if (string[1] == ':' && string[2] == '\\') {
			string+=2;
			strcpy(dir, string);
		} else {
			BLI_getwdN(dir);
			strcat(dir,"/");
			strcat(dir,string);
			strcpy((char *)string,dir);
		}

		BLI_make_exist(dir);
			
		if (S_ISDIR(BLI_exist(dir))) {
			strcpy(file,string + strlen(dir));

			if (strrchr(file,'/')) strcpy(file,strrchr(file,'/')+1);
		
			if ( (a = strlen(dir)) ) {
				if (dir[a-1] != '/') strcat(dir,"/");
			}
		}
		else {
			a = strlen(dir) - 1;
			while(dir[a] != '/') a--;
			dir[a + 1] = 0;
			strcpy(file, string + strlen(dir));
		}
	}
	else {
		BLI_getwdN(dir);
		strcat(dir, "/");
		file[0] = 0;
	}
#endif
}

/* simple appending of filename to dir, does not check for valid path! */
void BLI_join_dirfile(char *string, const char *dir, const char *file)
{
	int sl_dir;
	
	if(string != dir) /* compare pointers */
		BLI_strncpy(string, dir, FILE_MAX);
	
	sl_dir= BLI_add_slash(string);
	
	if (sl_dir <FILE_MAX) {
		BLI_strncpy(string + sl_dir, file, FILE_MAX-sl_dir);
	}
}

static int add_win32_extension(char *name)
{
	int retval = 0;
	int type;

	type = BLI_exist(name);
	if ((type == 0) || S_ISDIR(type)) {
#ifdef _WIN32
		char filename[FILE_MAXDIR+FILE_MAXFILE];
		char ext[FILE_MAXDIR+FILE_MAXFILE];
		char *extensions = getenv("PATHEXT");
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
				} else {
					strcat(filename, extensions);
				}

				type = BLI_exist(filename);
				if (type && (! S_ISDIR(type))) {
					retval = 1;
					strcpy(name, filename);
					break;
				}
			} while (temp);
		}
#endif
	} else {
		retval = 1;
	}

	return (retval);
}

void BLI_where_am_i(char *fullname, const char *name)
{
	char filename[FILE_MAXDIR+FILE_MAXFILE];
	char *path = NULL, *temp;
	
#ifdef _WIN32
	char *seperator = ";";
	char slash = '\\';
#else
	char *seperator = ":";
	char slash = '/';
#endif

	
#ifdef __linux__
	/* linux uses binreloc since argv[0] is not relyable, call br_init( NULL ) first */
	path = br_find_exe( NULL );
	if (path) {
		strcpy(fullname, path);
		free(path);
		return;
	}
#endif
	
	/* unix and non linux */
	if (name && fullname && strlen(name)) {
		strcpy(fullname, name);
		if (name[0] == '.') {
			// relative path, prepend cwd
			BLI_getwdN(fullname);
			
			// not needed but avoids annoying /./ in name
			if(name && name[0]=='.' && name[1]==slash)
				BLI_join_dirfile(fullname, fullname, name+2);
			else
				BLI_join_dirfile(fullname, fullname, name);
			
			add_win32_extension(fullname);
		} else if (BLI_last_slash(name)) {
			// full path
			strcpy(fullname, name);
			add_win32_extension(fullname);
		} else {
			// search for binary in $PATH
			path = getenv("PATH");
			if (path) {
				do {
					temp = strstr(path, seperator);
					if (temp) {
						strncpy(filename, path, temp - path);
						filename[temp - path] = 0;
						path = temp + 1;
					} else {
						strncpy(filename, path, sizeof(filename));
					}
					BLI_join_dirfile(fullname, fullname, name);
					if (add_win32_extension(filename)) {
						strcpy(fullname, filename);
						break;
					}
				} while (temp);
			}
		}
#ifndef NDEBUG
		if (strcmp(name, fullname)) {
			printf("guessing '%s' == '%s'\n", name, fullname);
		}
#endif

#ifdef _WIN32
		// in windows change long filename to short filename because
		// win2k doesn't know how to parse a commandline with lots of
		// spaces and double-quotes. There's another solution to this
		// with spawnv(P_WAIT, bprogname, argv) instead of system() but
		// that's even uglier
		GetShortPathName(fullname, fullname, FILE_MAXDIR+FILE_MAXFILE);
#ifndef NDEBUG
		printf("Shortname = '%s'\n", fullname);
#endif
#endif
	}
}

void BLI_where_is_temp(char *fullname, int usertemp)
{
	fullname[0] = '\0';
	
	if (usertemp && BLI_exists(U.tempdir)) {
		strcpy(fullname, U.tempdir);
	}
	
	
#ifdef WIN32
	if (fullname[0] == '\0') {
		char *tmp = getenv("TEMP"); /* Windows */
		if (tmp && BLI_exists(tmp)) {
			strcpy(fullname, tmp);
		}
	}
#else
	/* Other OS's - Try TMP and TMPDIR */
	if (fullname[0] == '\0') {
		char *tmp = getenv("TMP");
		if (tmp && BLI_exists(tmp)) {
			strcpy(fullname, tmp);
		}
	}
	
	if (fullname[0] == '\0') {
		char *tmp = getenv("TMPDIR");
		if (tmp && BLI_exists(tmp)) {
			strcpy(fullname, tmp);
		}
	}
#endif	
	
	if (fullname[0] == '\0') {
		strcpy(fullname, "/tmp/");
	} else {
		/* add a trailing slash if needed */
		BLI_add_slash(fullname);
	}
}

char *get_install_dir(void) {
	extern char bprogname[];
	char *tmpname = BLI_strdup(bprogname);
	char *cut;

#ifdef __APPLE__
	cut = strstr(tmpname, ".app");
	if (cut) cut[0] = 0;
#endif

	cut = BLI_last_slash(tmpname);

	if (cut) {
		cut[0] = 0;
		return tmpname;
	} else {
		MEM_freeN(tmpname);
		return NULL;
	}
}

/* 
 * returns absolute path to the app bundle
 * only useful on OS X 
 */
#ifdef __APPLE__
char* BLI_getbundle(void) {
	CFURLRef bundleURL;
	CFStringRef pathStr;
	static char path[MAXPATHLEN];
	CFBundleRef mainBundle = CFBundleGetMainBundle();

	bundleURL = CFBundleCopyBundleURL(mainBundle);
	pathStr = CFURLCopyFileSystemPath(bundleURL, kCFURLPOSIXPathStyle);
	CFStringGetCString(pathStr, path, MAXPATHLEN, kCFStringEncodingASCII);
	return path;
}
#endif

#ifdef WITH_ICONV
#include "iconv.h"
#include "localcharset.h"

void BLI_string_to_utf8(char *original, char *utf_8, const char *code)
{
	size_t inbytesleft=strlen(original);
	size_t outbytesleft=512;
	size_t rv=0;
	iconv_t cd;
	
	if (NULL == code) {
		code = locale_charset();
	}
	cd=iconv_open("UTF-8", code);

	if (cd == (iconv_t)(-1)) {
		printf("iconv_open Error");
		*utf_8='\0';
		return ;
	}
	rv=iconv(cd, &original, &inbytesleft, &utf_8, &outbytesleft);
	if (rv == (size_t) -1) {
		printf("iconv Error\n");
		return ;
	}
	*utf_8 = '\0';
	iconv_close(cd);
}
#endif // WITH_ICONV



/**
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
 * Windows-posix compatibility layer, windows-specific functions.
 */

#ifdef WIN32

#include <stdlib.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_path_util.h"
#include "BLI_string.h"
#define WIN32_SKIP_HKEY_PROTECTION		// need to use HKEY
#include "BLI_winstuff.h"

#include "BKE_utildefines.h" /* FILE_MAXDIR + FILE_MAXFILE */

int BLI_getInstallationDir( char * str ) {
	char dir[FILE_MAXDIR];
	char file[FILE_MAXFILE];
	int a;
	
	GetModuleFileName(NULL,str,FILE_MAXDIR+FILE_MAXFILE);
	BLI_split_dirfile(str,dir,file); /* shouldn't be relative */
	a = strlen(dir);
	if(dir[a-1] == '\\') dir[a-1]=0;
	
	strcpy(str,dir);
	
	return 1;
}


void RegisterBlendExtension(char * str) {
	LONG lresult;
	HKEY hkey = 0;
	DWORD dwd = 0;
	char buffer[128];
	
	lresult = RegCreateKeyEx(HKEY_CLASSES_ROOT, "blendfile\\shell\\open\\command", 0,
		"", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, &dwd);

	if (lresult == ERROR_SUCCESS) {
		sprintf(buffer, "\"%s\" \"%%1\"", str);
		lresult = RegSetValueEx(hkey, NULL, 0, REG_SZ, buffer, strlen(buffer) + 1);
		RegCloseKey(hkey);
	}

	lresult = RegCreateKeyEx(HKEY_CLASSES_ROOT, "blendfile\\DefaultIcon", 0,
		"", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, &dwd);

	if (lresult == ERROR_SUCCESS) {
		sprintf(buffer, "\"%s\",1", str);
		lresult = RegSetValueEx(hkey, NULL, 0, REG_SZ, buffer, strlen(buffer) + 1);
		RegCloseKey(hkey);
	}

	lresult = RegCreateKeyEx(HKEY_CLASSES_ROOT, ".blend", 0,
		"", REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hkey, &dwd);

	if (lresult == ERROR_SUCCESS) {
		sprintf(buffer, "%s", "blendfile");
		lresult = RegSetValueEx(hkey, NULL, 0, REG_SZ, buffer, strlen(buffer) + 1);
		RegCloseKey(hkey);
	}
}

DIR *opendir (const char *path) {
	if (GetFileAttributes(path) & FILE_ATTRIBUTE_DIRECTORY) {
		DIR *newd= MEM_mallocN(sizeof(DIR), "opendir");

		newd->handle = INVALID_HANDLE_VALUE;
		sprintf(newd->path, "%s\\*",path);
		
		newd->direntry.d_ino= 0;
		newd->direntry.d_off= 0;
		newd->direntry.d_reclen= 0;
		newd->direntry.d_name= NULL;
		
		return newd;
	} else {
		return NULL;
	}
}

struct dirent *readdir(DIR *dp) {
	if (dp->direntry.d_name) {
		MEM_freeN(dp->direntry.d_name);
		dp->direntry.d_name= NULL;
	}
		
	if (dp->handle==INVALID_HANDLE_VALUE) {
		dp->handle= FindFirstFile(dp->path, &(dp->data));
		if (dp->handle==INVALID_HANDLE_VALUE)
			return NULL;
			
		dp->direntry.d_name= BLI_strdup(dp->data.cFileName);

		return &dp->direntry;
	} else if (FindNextFile (dp->handle, &(dp->data))) {
		dp->direntry.d_name= BLI_strdup(dp->data.cFileName);

		return &dp->direntry;
	} else {
		return NULL;
	}
}

int closedir (DIR *dp) {
	if (dp->direntry.d_name) MEM_freeN(dp->direntry.d_name);
	if (dp->handle!=INVALID_HANDLE_VALUE) FindClose(dp->handle);

	MEM_freeN(dp);
	
	return 0;
}

void get_default_root(char* root) {
	char str[MAX_PATH+1];
	
	/* the default drive to resolve a directory without a specified drive 
	   should be the Windows installation drive, since this was what the OS
	   assumes. */
	if (GetWindowsDirectory(str,MAX_PATH+1)) {
		root[0] = str[0];
		root[1] = ':';
		root[2] = '\\';
		root[3] = '\0';
	} else {		
		/* if GetWindowsDirectory fails, something has probably gone wrong, 
		   we are trying the blender install dir though */
		if (GetModuleFileName(NULL,str,MAX_PATH+1)) {
			printf("Error! Could not get the Windows Directory - Defaulting to Blender installation Dir!");
			root[0] = str[0];
			root[1] = ':';
			root[2] = '\\';
			root[3] = '\0';
		} else {
			DWORD tmp;
			int i;
			int rc = 0;
			/* now something has gone really wrong - still trying our best guess */
			printf("Error! Could not get the Windows Directory - Defaulting to first valid drive! Path might be invalid!");
			tmp= GetLogicalDrives();
			for (i=2; i < 26; i++) {
				if ((tmp>>i) & 1) {
					root[0] = 'a'+i;
					root[1] = ':';
					root[2] = '\\';
					root[3] = '\0';
					if (GetFileAttributes(root) != 0xFFFFFFFF) {
						rc = i;
						break;			
					}
				}
			}
			if (0 == rc) {
				printf("ERROR in 'get_default_root': can't find a valid drive!");
				root[0] = 'C';
				root[1] = ':';
				root[2] = '\\';
				root[3] = '\0';
			}
		}		
	}
}

int check_file_chars(char *filename)
{
	char *p = filename;
	while (*p) {
		switch (*p) {
			case ':':
			case '?':
			case '*':
			case '|':
			case '\\':
			case '/':
			case '\"':
				return 0;
				break;
		}

		p++;
	}
	return 1;
}

/* Copied from http://sourceware.org/ml/newlib/2005/msg00248.html */
/* Copyright 2005 Shaun Jackman
 * Permission to use, copy, modify, and distribute this software
 * is freely granted, provided that this notice is preserved.
 */
#include <string.h>
char* dirname(char *path)
{
	   char *p;
	   if( path == NULL || *path == '\0' )
			   return ".";
	   p = path + strlen(path) - 1;
	   while( *p == '/' ) {
			   if( p == path )
					   return path;
			   *p-- = '\0';
	   }
	   while( p >= path && *p != '/' )
			   p--;
	   return
			   p < path ? "." :
			   p == path ? "/" :
			   (*p = '\0', path);
}
/* End of copied part */

#else

/* intentionally empty for UNIX */

#endif

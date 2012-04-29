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
 * Contributor(s): Andrea Weikert (c) 2008 Blender Foundation.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_file/fsmenu.c
 *  \ingroup spfile
 */


#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "DNA_space_types.h" /* FILE_MAX */

#include "BLI_blenlib.h"
#include "BLI_linklist.h"
#include "BLI_dynstr.h"

#ifdef WIN32
#  include <windows.h> /* need to include windows.h so _WIN32_IE is defined  */
#  ifndef _WIN32_IE
#    define _WIN32_IE 0x0400 /* minimal requirements for SHGetSpecialFolderPath on MINGW MSVC has this defined already */
#  endif
#  include <shlobj.h>  /* for SHGetSpecialFolderPath, has to be done before BLI_winstuff
                        * because 'near' is disabled through BLI_windstuff */
#  include "BLI_winstuff.h"
#endif

#ifdef __APPLE__
   /* XXX BIG WARNING: carbon.h can not be included in blender code, it conflicts with struct ID */
#  define ID ID_
#  include <CoreServices/CoreServices.h>
#endif /* __APPLE__ */

#ifdef __linux__
#include <mntent.h>
#endif

#include "fsmenu.h"  /* include ourselves */


/* FSMENU HANDLING */

	/* FSMenuEntry's without paths indicate seperators */
typedef struct _FSMenuEntry FSMenuEntry;
struct _FSMenuEntry {
	FSMenuEntry *next;

	char *path;
	short save;
};

typedef struct FSMenu
{
	FSMenuEntry *fsmenu_system;
	FSMenuEntry *fsmenu_bookmarks;
	FSMenuEntry *fsmenu_recent;

} FSMenu;

static FSMenu *g_fsmenu = NULL;

struct FSMenu* fsmenu_get(void)
{
	if (!g_fsmenu) {
		g_fsmenu=MEM_callocN(sizeof(struct FSMenu), "fsmenu");
	}
	return g_fsmenu;
}

static FSMenuEntry *fsmenu_get_category(struct FSMenu* fsmenu, FSMenuCategory category)
{
	FSMenuEntry *fsms = NULL;

	switch (category) {
		case FS_CATEGORY_SYSTEM:
			fsms = fsmenu->fsmenu_system;
			break;
		case FS_CATEGORY_BOOKMARKS:
			fsms = fsmenu->fsmenu_bookmarks;
			break;
		case FS_CATEGORY_RECENT:
			fsms = fsmenu->fsmenu_recent;
			break;
	}
	return fsms;
}

static void fsmenu_set_category(struct FSMenu* fsmenu, FSMenuCategory category, FSMenuEntry *fsms)
{
	switch (category) {
		case FS_CATEGORY_SYSTEM:
			fsmenu->fsmenu_system = fsms;
			break;
		case FS_CATEGORY_BOOKMARKS:
			fsmenu->fsmenu_bookmarks = fsms;
			break;
		case FS_CATEGORY_RECENT:
			fsmenu->fsmenu_recent = fsms;
			break;
	}
}

int fsmenu_get_nentries(struct FSMenu* fsmenu, FSMenuCategory category)
{
	FSMenuEntry *fsme;
	int count= 0;

	for (fsme= fsmenu_get_category(fsmenu, category); fsme; fsme= fsme->next) 
		count++;

	return count;
}

char *fsmenu_get_entry(struct FSMenu* fsmenu, FSMenuCategory category, int idx)
{
	FSMenuEntry *fsme;

	for (fsme= fsmenu_get_category(fsmenu, category); fsme && idx; fsme= fsme->next)
		idx--;

	return fsme?fsme->path:NULL;
}

short fsmenu_can_save (struct FSMenu* fsmenu, FSMenuCategory category, int idx)
{
	FSMenuEntry *fsme;

	for (fsme= fsmenu_get_category(fsmenu, category); fsme && idx; fsme= fsme->next)
		idx--;

	return fsme?fsme->save:0;
}

void fsmenu_insert_entry(struct FSMenu* fsmenu, FSMenuCategory category, const char *path, int sorted, short save)
{
	FSMenuEntry *prev;
	FSMenuEntry *fsme;
	FSMenuEntry *fsms;

	fsms = fsmenu_get_category(fsmenu, category);
	prev= fsme= fsms;

	for (; fsme; prev= fsme, fsme= fsme->next) {
		if (fsme->path) {
			const int cmp_ret= BLI_path_cmp(path, fsme->path);
			if (cmp_ret == 0) {
				return;
			}
			else if (sorted && cmp_ret < 0) {
				break;
			}
		}
		else {
			// if we're bookmarking this, file should come 
			// before the last separator, only automatically added
			// current dir go after the last sep.
			if (save) {
				break;
			}
		}
	}
	
	fsme= MEM_mallocN(sizeof(*fsme), "fsme");
	fsme->path= BLI_strdup(path);
	fsme->save = save;

	if (prev) {
		fsme->next= prev->next;
		prev->next= fsme;
	}
	else {
		fsme->next= fsms;
		fsmenu_set_category(fsmenu, category, fsme);
	}
}

void fsmenu_remove_entry(struct FSMenu* fsmenu, FSMenuCategory category, int idx)
{
	FSMenuEntry *prev= NULL, *fsme= NULL;
	FSMenuEntry *fsms = fsmenu_get_category(fsmenu, category);

	for (fsme= fsms; fsme && idx; prev= fsme, fsme= fsme->next)		
		idx--;

	if (fsme) {
		/* you should only be able to remove entries that were 
		 * not added by default, like windows drives.
		 * also separators (where path == NULL) shouldn't be removed */
		if (fsme->save && fsme->path) {

			/* remove fsme from list */
			if (prev) {
				prev->next= fsme->next;
			}
			else {
				fsms= fsme->next;
				fsmenu_set_category(fsmenu, category, fsms);
			}
			/* free entry */
			MEM_freeN(fsme->path);
			MEM_freeN(fsme);
		}
	}
}

void fsmenu_write_file(struct FSMenu* fsmenu, const char *filename)
{
	FSMenuEntry *fsme= NULL;
	int nskip= 0;

	FILE *fp = BLI_fopen(filename, "w");
	if (!fp) return;
	
	fprintf(fp, "[Bookmarks]\n");
	for (fsme= fsmenu_get_category(fsmenu, FS_CATEGORY_BOOKMARKS); fsme; fsme= fsme->next) {
		if (fsme->path && fsme->save) {
			fprintf(fp, "%s\n", fsme->path);
		}
	}
	fprintf(fp, "[Recent]\n");
	nskip = fsmenu_get_nentries(fsmenu, FS_CATEGORY_RECENT) - FSMENU_RECENT_MAX;
	// skip first entries if list too long
	for (fsme= fsmenu_get_category(fsmenu, FS_CATEGORY_RECENT); fsme && (nskip>0); fsme= fsme->next, --nskip) {
		/* pass */
	}
	for (; fsme; fsme= fsme->next) {
		if (fsme->path && fsme->save) {
			fprintf(fp, "%s\n", fsme->path);
		}
	}
	fclose(fp);
}

void fsmenu_read_bookmarks(struct FSMenu* fsmenu, const char *filename)
{
	char line[256];
	FSMenuCategory category = FS_CATEGORY_BOOKMARKS;
	FILE *fp;

	fp = BLI_fopen(filename, "r");
	if (!fp) return;

	while ( fgets ( line, 256, fp ) != NULL ) {  /* read a line */
		if (strncmp(line, "[Bookmarks]", 11)==0) {
			category = FS_CATEGORY_BOOKMARKS;
		}
		else if (strncmp(line, "[Recent]", 8)==0) {
			category = FS_CATEGORY_RECENT;
		}
		else {
			int len = strlen(line);
			if (len>0) {
				if (line[len-1] == '\n') {
					line[len-1] = '\0';
				}
				if (BLI_exists(line)) {
					fsmenu_insert_entry(fsmenu, category, line, 0, 1);
				}
			}
		}
	}
	fclose(fp);
}

void fsmenu_read_system(struct FSMenu* fsmenu)
{
	char line[256];
#ifdef WIN32
	/* Add the drive names to the listing */
	{
		__int64 tmp;
		char tmps[4];
		int i;
			
		tmp= GetLogicalDrives();
		
		for (i=0; i < 26; i++) {
			if ((tmp>>i) & 1) {
				tmps[0]='A'+i;
				tmps[1]=':';
				tmps[2]='\\';
				tmps[3]=0;
				
				fsmenu_insert_entry(fsmenu, FS_CATEGORY_SYSTEM, tmps, 1, 0);
			}
		}

		/* Adding Desktop and My Documents */
		SHGetSpecialFolderPath(0, line, CSIDL_PERSONAL, 0);
		fsmenu_insert_entry(fsmenu, FS_CATEGORY_BOOKMARKS, line, 1, 0);
		SHGetSpecialFolderPath(0, line, CSIDL_DESKTOPDIRECTORY, 0);
		fsmenu_insert_entry(fsmenu, FS_CATEGORY_BOOKMARKS, line, 1, 0);
	}
#else
#ifdef __APPLE__
	{
#if (MAC_OS_X_VERSION_MIN_REQUIRED <= MAC_OS_X_VERSION_10_4)
		OSErr err=noErr;
		int i;
		const char *home;
		
		/* loop through all the OS X Volumes, and add them to the SYSTEM section */
		for (i = 1; err != nsvErr; i++) {
			FSRef dir;
			unsigned char path[FILE_MAX];
			
			err = FSGetVolumeInfo(kFSInvalidVolumeRefNum, i, NULL, kFSVolInfoNone, NULL, NULL, &dir);
			if (err != noErr)
				continue;
			
			FSRefMakePath(&dir, path, FILE_MAX);
			if (strcmp((char*)path, "/home") && strcmp((char*)path, "/net")) {
				/* /net and /home are meaningless on OSX, home folders are stored in /Users */
				fsmenu_insert_entry(fsmenu, FS_CATEGORY_SYSTEM, (char *)path, 1, 0);
			}
		}

		/* As 10.4 doesn't provide proper API to retrieve the favorite places,
		 * assume they are the standard ones 
		 * TODO : replace hardcoded paths with proper BLI_get_folder calls */
		home = getenv("HOME");
		if (home) {
			BLI_snprintf(line, 256, "%s/", home);
			fsmenu_insert_entry(fsmenu, FS_CATEGORY_BOOKMARKS, line, 1, 0);
			BLI_snprintf(line, 256, "%s/Desktop/", home);
			if (BLI_exists(line)) {
				fsmenu_insert_entry(fsmenu, FS_CATEGORY_BOOKMARKS, line, 1, 0);
			}
			BLI_snprintf(line, 256, "%s/Documents/", home);
			if (BLI_exists(line)) {
				fsmenu_insert_entry(fsmenu, FS_CATEGORY_BOOKMARKS, line, 1, 0);
			}
			BLI_snprintf(line, 256, "%s/Pictures/", home);
			if (BLI_exists(line)) {
				fsmenu_insert_entry(fsmenu, FS_CATEGORY_BOOKMARKS, line, 1, 0);
			}
			BLI_snprintf(line, 256, "%s/Music/", home);
			if (BLI_exists(line)) {
				fsmenu_insert_entry(fsmenu, FS_CATEGORY_BOOKMARKS, line, 1, 0);
			}
			BLI_snprintf(line, 256, "%s/Movies/", home);
			if (BLI_exists(line)) {
				fsmenu_insert_entry(fsmenu, FS_CATEGORY_BOOKMARKS, line, 1, 0);
			}
		}
#else
		/* 10.5 provides ability to retrieve Finder favorite places */
		UInt32 seed;
		OSErr err = noErr;
		CFArrayRef pathesArray;
		LSSharedFileListRef list;
		LSSharedFileListItemRef itemRef;
		CFIndex i, pathesCount;
		CFURLRef cfURL = NULL;
		CFStringRef pathString = NULL;
		
		/* First get local mounted volumes */
		list = LSSharedFileListCreate(NULL, kLSSharedFileListFavoriteVolumes, NULL);
		pathesArray = LSSharedFileListCopySnapshot(list, &seed);
		pathesCount = CFArrayGetCount(pathesArray);
		
		for (i=0; i<pathesCount; i++) {
			itemRef = (LSSharedFileListItemRef)CFArrayGetValueAtIndex(pathesArray, i);
			
			err = LSSharedFileListItemResolve(itemRef, 
											  kLSSharedFileListNoUserInteraction
											  | kLSSharedFileListDoNotMountVolumes, 
											  &cfURL, NULL);
			if (err != noErr)
				continue;
			
			pathString = CFURLCopyFileSystemPath(cfURL, kCFURLPOSIXPathStyle);
			
			if (!CFStringGetCString(pathString, line, 256, kCFStringEncodingASCII))
				continue;
			fsmenu_insert_entry(fsmenu, FS_CATEGORY_SYSTEM, line, 1, 0);
			
			CFRelease(pathString);
			CFRelease(cfURL);
		}
		
		CFRelease(pathesArray);
		CFRelease(list);
		
		/* Then get network volumes */
		err = noErr;
		for (i=1; err!=nsvErr; i++) {
			FSRef dir;
			FSVolumeRefNum volRefNum;
			struct GetVolParmsInfoBuffer volParmsBuffer;
			unsigned char path[FILE_MAX];
			
			err = FSGetVolumeInfo(kFSInvalidVolumeRefNum, i, &volRefNum, kFSVolInfoNone, NULL, NULL, &dir);
			if (err != noErr)
				continue;
			
			err = FSGetVolumeParms(volRefNum, &volParmsBuffer, sizeof(volParmsBuffer));
			if ((err != noErr) || (volParmsBuffer.vMServerAdr == 0)) /* Exclude local devices */
				continue;
			
			
			FSRefMakePath(&dir, path, FILE_MAX);
			fsmenu_insert_entry(fsmenu, FS_CATEGORY_SYSTEM, (char *)path, 1, 0);
		}
		
		/* Finally get user favorite places */
		list = LSSharedFileListCreate(NULL, kLSSharedFileListFavoriteItems, NULL);
		pathesArray = LSSharedFileListCopySnapshot(list, &seed);
		pathesCount = CFArrayGetCount(pathesArray);
		
		for (i=0; i<pathesCount; i++) {
			itemRef = (LSSharedFileListItemRef)CFArrayGetValueAtIndex(pathesArray, i);
			
			err = LSSharedFileListItemResolve(itemRef, 
											  kLSSharedFileListNoUserInteraction
											  | kLSSharedFileListDoNotMountVolumes, 
											  &cfURL, NULL);
			if (err != noErr)
				continue;
			
			pathString = CFURLCopyFileSystemPath(cfURL, kCFURLPOSIXPathStyle);
			
			if (!CFStringGetCString(pathString, line, 256, kCFStringEncodingASCII))
				continue;
			fsmenu_insert_entry(fsmenu, FS_CATEGORY_BOOKMARKS, line, 1, 0);
			
			CFRelease(pathString);
			CFRelease(cfURL);
		}
		
		CFRelease(pathesArray);
		CFRelease(list);
#endif /* OSX 10.5+ */
	}
#else
	/* unix */
	{
		const char *home= getenv("HOME");

		if (home) {
			BLI_snprintf(line, FILE_MAXDIR, "%s/", home);
			fsmenu_insert_entry(fsmenu, FS_CATEGORY_BOOKMARKS, line, 1, 0);
			BLI_snprintf(line, FILE_MAXDIR, "%s/Desktop/", home);
			if (BLI_exists(line)) {
				fsmenu_insert_entry(fsmenu, FS_CATEGORY_BOOKMARKS, line, 1, 0);
			}
		}

		{
			int found= 0;
#ifdef __linux__
			/* loop over mount points */
			struct mntent *mnt;
			int len;
			FILE *fp;

			fp = setmntent (MOUNTED, "r");
			if (fp == NULL) {
				fprintf(stderr, "could not get a list of mounted filesystemts\n");
			}
			else {
				while ((mnt = getmntent (fp))) {
					/* not sure if this is right, but seems to give the relevant mnts */
					if (strncmp(mnt->mnt_fsname, "/dev", 4))
						continue;

					len= strlen(mnt->mnt_dir);
					if (len && mnt->mnt_dir[len-1] != '/') {
						BLI_snprintf(line, FILE_MAXDIR, "%s/", mnt->mnt_dir);
						fsmenu_insert_entry(fsmenu, FS_CATEGORY_SYSTEM, line, 1, 0);
					}
					else
						fsmenu_insert_entry(fsmenu, FS_CATEGORY_SYSTEM, mnt->mnt_dir, 1, 0);

					found= 1;
				}
				if (endmntent (fp) == 0) {
					fprintf(stderr, "could not close the list of mounted filesystemts\n");
				}
			}
#endif

			/* fallback */
			if (!found)
				fsmenu_insert_entry(fsmenu, FS_CATEGORY_SYSTEM, "/", 1, 0);
		}
	}
#endif
#endif
}


static void fsmenu_free_category(struct FSMenu* fsmenu, FSMenuCategory category)
{
	FSMenuEntry *fsme= fsmenu_get_category(fsmenu, category);

	while (fsme) {
		FSMenuEntry *n= fsme->next;

		if (fsme->path) MEM_freeN(fsme->path);
		MEM_freeN(fsme);

		fsme= n;
	}
}

void fsmenu_free(struct FSMenu* fsmenu)
{
	fsmenu_free_category(fsmenu, FS_CATEGORY_SYSTEM);
	fsmenu_free_category(fsmenu, FS_CATEGORY_BOOKMARKS);
	fsmenu_free_category(fsmenu, FS_CATEGORY_RECENT);
	MEM_freeN(fsmenu);
}


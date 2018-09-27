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

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"

#include "BKE_appdir.h"

#include "DNA_space_types.h"

#include "ED_fileselect.h"

#ifdef WIN32
#  include <windows.h> /* need to include windows.h so _WIN32_IE is defined  */
#  include <shlobj.h>  /* for SHGetSpecialFolderPath, has to be done before BLI_winstuff
                        * because 'near' is disabled through BLI_windstuff */
#  include "BLI_winstuff.h"
#endif

#ifdef __APPLE__
#include <Carbon/Carbon.h>
#endif /* __APPLE__ */

#ifdef __linux__
#include <mntent.h>
#endif

#include "fsmenu.h"  /* include ourselves */


/* FSMENU HANDLING */

typedef struct FSMenu {
	FSMenuEntry *fsmenu_system;
	FSMenuEntry *fsmenu_system_bookmarks;
	FSMenuEntry *fsmenu_bookmarks;
	FSMenuEntry *fsmenu_recent;
} FSMenu;

static FSMenu *g_fsmenu = NULL;

FSMenu *ED_fsmenu_get(void)
{
	if (!g_fsmenu) {
		g_fsmenu = MEM_callocN(sizeof(struct FSMenu), "fsmenu");
	}
	return g_fsmenu;
}

struct FSMenuEntry *ED_fsmenu_get_category(struct FSMenu *fsmenu, FSMenuCategory category)
{
	FSMenuEntry *fsm_head = NULL;

	switch (category) {
		case FS_CATEGORY_SYSTEM:
			fsm_head = fsmenu->fsmenu_system;
			break;
		case FS_CATEGORY_SYSTEM_BOOKMARKS:
			fsm_head = fsmenu->fsmenu_system_bookmarks;
			break;
		case FS_CATEGORY_BOOKMARKS:
			fsm_head = fsmenu->fsmenu_bookmarks;
			break;
		case FS_CATEGORY_RECENT:
			fsm_head = fsmenu->fsmenu_recent;
			break;
	}
	return fsm_head;
}

void ED_fsmenu_set_category(struct FSMenu *fsmenu, FSMenuCategory category, FSMenuEntry *fsm_head)
{
	switch (category) {
		case FS_CATEGORY_SYSTEM:
			fsmenu->fsmenu_system = fsm_head;
			break;
		case FS_CATEGORY_SYSTEM_BOOKMARKS:
			fsmenu->fsmenu_system_bookmarks = fsm_head;
			break;
		case FS_CATEGORY_BOOKMARKS:
			fsmenu->fsmenu_bookmarks = fsm_head;
			break;
		case FS_CATEGORY_RECENT:
			fsmenu->fsmenu_recent = fsm_head;
			break;
	}
}

int ED_fsmenu_get_nentries(struct FSMenu *fsmenu, FSMenuCategory category)
{
	FSMenuEntry *fsm_iter;
	int count = 0;

	for (fsm_iter = ED_fsmenu_get_category(fsmenu, category); fsm_iter; fsm_iter = fsm_iter->next) {
		count++;
	}

	return count;
}

FSMenuEntry *ED_fsmenu_get_entry(struct FSMenu *fsmenu, FSMenuCategory category, int index)
{
	FSMenuEntry *fsm_iter;

	for (fsm_iter = ED_fsmenu_get_category(fsmenu, category); fsm_iter && index; fsm_iter = fsm_iter->next) {
		index--;
	}

	return fsm_iter;
}

char *ED_fsmenu_entry_get_path(struct FSMenuEntry *fsentry)
{
	return fsentry->path;
}

void ED_fsmenu_entry_set_path(struct FSMenuEntry *fsentry, const char *path)
{
	if ((!fsentry->path || !path || !STREQ(path, fsentry->path)) && (fsentry->path != path)) {
		char tmp_name[FILE_MAXFILE];

		MEM_SAFE_FREE(fsentry->path);

		fsentry->path = (path && path[0]) ? BLI_strdup(path) : NULL;

		BLI_make_file_string("/", tmp_name, BKE_appdir_folder_id_create(BLENDER_USER_CONFIG, NULL), BLENDER_BOOKMARK_FILE);
		fsmenu_write_file(ED_fsmenu_get(), tmp_name);
	}
}

static void fsmenu_entry_generate_name(struct FSMenuEntry *fsentry, char *name, size_t name_size)
{
	int offset = 0;
	int len = name_size;

	if (BLI_path_name_at_index(fsentry->path, -1, &offset, &len)) {
		/* use as size */
		len += 1;
	}

	BLI_strncpy(name, &fsentry->path[offset], MIN2(len, name_size));
	if (!name[0]) {
		name[0] = '/';
		name[1] = '\0';
	}
}

char *ED_fsmenu_entry_get_name(struct FSMenuEntry *fsentry)
{
	if (fsentry->name[0]) {
		return fsentry->name;
	}
	else {
		/* Here we abuse fsm_iter->name, keeping first char NULL. */
		char *name = fsentry->name + 1;
		size_t name_size = sizeof(fsentry->name) - 1;

		fsmenu_entry_generate_name(fsentry, name, name_size);
		return name;
	}
}

void ED_fsmenu_entry_set_name(struct FSMenuEntry *fsentry, const char *name)
{
	if (!STREQ(name, fsentry->name)) {
		char tmp_name[FILE_MAXFILE];
		size_t tmp_name_size = sizeof(tmp_name);

		fsmenu_entry_generate_name(fsentry, tmp_name, tmp_name_size);
		if (!name[0] || STREQ(tmp_name, name)) {
			/* reset name to default behavior. */
			fsentry->name[0] = '\0';
		}
		else {
			BLI_strncpy(fsentry->name, name, sizeof(fsentry->name));
		}

		BLI_make_file_string("/", tmp_name, BKE_appdir_folder_id_create(BLENDER_USER_CONFIG, NULL), BLENDER_BOOKMARK_FILE);
		fsmenu_write_file(ED_fsmenu_get(), tmp_name);
	}
}

void fsmenu_entry_refresh_valid(struct FSMenuEntry *fsentry)
{
	if (fsentry->path && fsentry->path[0]) {
#ifdef WIN32
		/* XXX Special case, always consider those as valid.
		 *     Thanks to Windows, which can spend five seconds to perform a mere stat() call on those paths...
		 *     See T43684.
		 */
		const char *exceptions[] = {"A:\\", "B:\\", NULL};
		const size_t exceptions_len[] = {strlen(exceptions[0]), strlen(exceptions[1]), 0};
		int i;

		for (i = 0; exceptions[i]; i++) {
			if (STRCASEEQLEN(fsentry->path, exceptions[i], exceptions_len[i])) {
				fsentry->valid = true;
				return;
			}
		}
#endif
		fsentry->valid = BLI_is_dir(fsentry->path);
	}
	else {
		fsentry->valid = false;
	}
}

short fsmenu_can_save(struct FSMenu *fsmenu, FSMenuCategory category, int idx)
{
	FSMenuEntry *fsm_iter;

	for (fsm_iter = ED_fsmenu_get_category(fsmenu, category); fsm_iter && idx; fsm_iter = fsm_iter->next) {
		idx--;
	}

	return fsm_iter ? fsm_iter->save : 0;
}

void fsmenu_insert_entry(struct FSMenu *fsmenu, FSMenuCategory category, const char *path, const char *name, FSMenuInsert flag)
{
	FSMenuEntry *fsm_prev;
	FSMenuEntry *fsm_iter;
	FSMenuEntry *fsm_head;

	fsm_head = ED_fsmenu_get_category(fsmenu, category);
	fsm_prev = fsm_head;  /* this is odd and not really correct? */

	for (fsm_iter = fsm_head; fsm_iter; fsm_prev = fsm_iter, fsm_iter = fsm_iter->next) {
		if (fsm_iter->path) {
			const int cmp_ret = BLI_path_cmp(path, fsm_iter->path);
			if (cmp_ret == 0) {
				if (flag & FS_INSERT_FIRST) {
					if (fsm_iter != fsm_head) {
						fsm_prev->next = fsm_iter->next;
						fsm_iter->next = fsm_head;
						ED_fsmenu_set_category(fsmenu, category, fsm_iter);
					}
				}
				return;
			}
			else if ((flag & FS_INSERT_SORTED) && cmp_ret < 0) {
				break;
			}
		}
		else {
			/* if we're bookmarking this, file should come
			 * before the last separator, only automatically added
			 * current dir go after the last sep. */
			if (flag & FS_INSERT_SAVE) {
				break;
			}
		}
	}

	fsm_iter = MEM_mallocN(sizeof(*fsm_iter), "fsme");
	fsm_iter->path = BLI_strdup(path);
	fsm_iter->save = (flag & FS_INSERT_SAVE) != 0;

	if ((category == FS_CATEGORY_RECENT) && (!name || !name[0])) {
		/* Special handling when adding new recent entry - check if dir exists in some other categories,
		 * and try to use name from there if so. */
		FSMenuCategory cats[] = {FS_CATEGORY_SYSTEM, FS_CATEGORY_SYSTEM_BOOKMARKS, FS_CATEGORY_BOOKMARKS};
		int i = ARRAY_SIZE(cats);

		while (i--) {
			FSMenuEntry *tfsm = ED_fsmenu_get_category(fsmenu, cats[i]);

			for (; tfsm; tfsm = tfsm->next) {
				if (STREQ(tfsm->path, fsm_iter->path)) {
					if (tfsm->name[0]) {
						name = tfsm->name;
					}
					break;
				}
			}
			if (tfsm) {
				break;
			}
		}
	}

	if (name && name[0]) {
		BLI_strncpy(fsm_iter->name, name, sizeof(fsm_iter->name));
	}
	else {
		fsm_iter->name[0] = '\0';
	}
	fsmenu_entry_refresh_valid(fsm_iter);

	if (fsm_prev) {
		if (flag & FS_INSERT_FIRST) {
			fsm_iter->next = fsm_head;
			ED_fsmenu_set_category(fsmenu, category, fsm_iter);
		}
		else {
			fsm_iter->next = fsm_prev->next;
			fsm_prev->next = fsm_iter;
		}
	}
	else {
		fsm_iter->next = fsm_head;
		ED_fsmenu_set_category(fsmenu, category, fsm_iter);
	}
}

void fsmenu_remove_entry(struct FSMenu *fsmenu, FSMenuCategory category, int idx)
{
	FSMenuEntry *fsm_prev = NULL;
	FSMenuEntry *fsm_iter;
	FSMenuEntry *fsm_head;

	fsm_head = ED_fsmenu_get_category(fsmenu, category);

	for (fsm_iter = fsm_head; fsm_iter && idx; fsm_prev = fsm_iter, fsm_iter = fsm_iter->next)
		idx--;

	if (fsm_iter) {
		/* you should only be able to remove entries that were
		 * not added by default, like windows drives.
		 * also separators (where path == NULL) shouldn't be removed */
		if (fsm_iter->save && fsm_iter->path) {

			/* remove fsme from list */
			if (fsm_prev) {
				fsm_prev->next = fsm_iter->next;
			}
			else {
				fsm_head = fsm_iter->next;
				ED_fsmenu_set_category(fsmenu, category, fsm_head);
			}
			/* free entry */
			MEM_freeN(fsm_iter->path);
			MEM_freeN(fsm_iter);
		}
	}
}

void fsmenu_write_file(struct FSMenu *fsmenu, const char *filename)
{
	FSMenuEntry *fsm_iter = NULL;
	char fsm_name[FILE_MAX];
	int nwritten = 0;

	FILE *fp = BLI_fopen(filename, "w");
	if (!fp) return;

	fprintf(fp, "[Bookmarks]\n");
	for (fsm_iter = ED_fsmenu_get_category(fsmenu, FS_CATEGORY_BOOKMARKS); fsm_iter; fsm_iter = fsm_iter->next) {
		if (fsm_iter->path && fsm_iter->save) {
			fsmenu_entry_generate_name(fsm_iter, fsm_name, sizeof(fsm_name));
			if (fsm_iter->name[0] && !STREQ(fsm_iter->name, fsm_name)) {
				fprintf(fp, "!%s\n", fsm_iter->name);
			}
			fprintf(fp, "%s\n", fsm_iter->path);
		}
	}
	fprintf(fp, "[Recent]\n");
	for (fsm_iter = ED_fsmenu_get_category(fsmenu, FS_CATEGORY_RECENT); fsm_iter && (nwritten < FSMENU_RECENT_MAX); fsm_iter = fsm_iter->next, ++nwritten) {
		if (fsm_iter->path && fsm_iter->save) {
			fsmenu_entry_generate_name(fsm_iter, fsm_name, sizeof(fsm_name));
			if (fsm_iter->name[0] && !STREQ(fsm_iter->name, fsm_name)) {
				fprintf(fp, "!%s\n", fsm_iter->name);
			}
			fprintf(fp, "%s\n", fsm_iter->path);
		}
	}
	fclose(fp);
}

void fsmenu_read_bookmarks(struct FSMenu *fsmenu, const char *filename)
{
	char line[FILE_MAXDIR];
	char name[FILE_MAXFILE];
	FSMenuCategory category = FS_CATEGORY_BOOKMARKS;
	FILE *fp;

	fp = BLI_fopen(filename, "r");
	if (!fp) return;

	name[0] = '\0';

	while (fgets(line, sizeof(line), fp) != NULL) {       /* read a line */
		if (STREQLEN(line, "[Bookmarks]", 11)) {
			category = FS_CATEGORY_BOOKMARKS;
		}
		else if (STREQLEN(line, "[Recent]", 8)) {
			category = FS_CATEGORY_RECENT;
		}
		else if (line[0] == '!') {
			int len = strlen(line);
			if (len > 0) {
				if (line[len - 1] == '\n') {
					line[len - 1] = '\0';
				}
				BLI_strncpy(name, line + 1, sizeof(name));
			}
		}
		else {
			int len = strlen(line);
			if (len > 0) {
				if (line[len - 1] == '\n') {
					line[len - 1] = '\0';
				}
				/* don't do this because it can be slow on network drives,
				 * having a bookmark from a drive that's ejected or so isn't
				 * all _that_ bad */
#if 0
				if (BLI_exists(line))
#endif
				{
					fsmenu_insert_entry(fsmenu, category, line, name, FS_INSERT_SAVE);
				}
			}
			/* always reset name. */
			name[0] = '\0';
		}
	}
	fclose(fp);
}

void fsmenu_read_system(struct FSMenu *fsmenu, int read_bookmarks)
{
	char line[FILE_MAXDIR];
#ifdef WIN32
	/* Add the drive names to the listing */
	{
		wchar_t wline[FILE_MAXDIR];
		__int64 tmp;
		char tmps[4], *name;
		int i;

		tmp = GetLogicalDrives();

		for (i = 0; i < 26; i++) {
			if ((tmp >> i) & 1) {
				tmps[0] = 'A' + i;
				tmps[1] = ':';
				tmps[2] = '\\';
				tmps[3] = '\0';
				name = NULL;

				/* Flee from horrible win querying hover floppy drives! */
				if (i > 1) {
					/* Try to get volume label as well... */
					BLI_strncpy_wchar_from_utf8(wline, tmps, 4);
					if (GetVolumeInformationW(wline, wline + 4, FILE_MAXDIR - 4, NULL, NULL, NULL, NULL, 0)) {
						size_t label_len;

						BLI_strncpy_wchar_as_utf8(line, wline + 4, FILE_MAXDIR - 4);

						label_len = MIN2(strlen(line), FILE_MAXDIR - 6);
						BLI_snprintf(line + label_len, 6, " (%.2s)", tmps);

						name = line;
					}
				}

				fsmenu_insert_entry(fsmenu, FS_CATEGORY_SYSTEM, tmps, name, FS_INSERT_SORTED);
			}
		}

		/* Adding Desktop and My Documents */
		if (read_bookmarks) {
			SHGetSpecialFolderPathW(0, wline, CSIDL_PERSONAL, 0);
			BLI_strncpy_wchar_as_utf8(line, wline, FILE_MAXDIR);
			fsmenu_insert_entry(fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS, line, NULL, FS_INSERT_SORTED);
			SHGetSpecialFolderPathW(0, wline, CSIDL_DESKTOPDIRECTORY, 0);
			BLI_strncpy_wchar_as_utf8(line, wline, FILE_MAXDIR);
			fsmenu_insert_entry(fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS, line, NULL, FS_INSERT_SORTED);
		}
	}
#else
#ifdef __APPLE__
	{
		/* Get mounted volumes better method OSX 10.6 and higher, see: */
		/*https://developer.apple.com/library/mac/#documentation/CoreFOundation/Reference/CFURLRef/Reference/reference.html*/
		/* we get all volumes sorted including network and do not relay on user-defined finder visibility, less confusing */

		CFURLRef cfURL = NULL;
		CFURLEnumeratorResult result = kCFURLEnumeratorSuccess;
		CFURLEnumeratorRef volEnum = CFURLEnumeratorCreateForMountedVolumes(NULL, kCFURLEnumeratorSkipInvisibles, NULL);

		while (result != kCFURLEnumeratorEnd) {
			char defPath[FILE_MAX];

			result = CFURLEnumeratorGetNextURL(volEnum, &cfURL, NULL);
			if (result != kCFURLEnumeratorSuccess)
				continue;

			CFURLGetFileSystemRepresentation(cfURL, false, (UInt8 *)defPath, FILE_MAX);

			/* Add end slash for consistency with other platforms */
			BLI_add_slash(defPath);

			fsmenu_insert_entry(fsmenu, FS_CATEGORY_SYSTEM, defPath, NULL, FS_INSERT_SORTED);
		}

		CFRelease(volEnum);

		/* Finally get user favorite places */
		if (read_bookmarks) {
			UInt32 seed;
			LSSharedFileListRef list = LSSharedFileListCreate(NULL, kLSSharedFileListFavoriteItems, NULL);
			CFArrayRef pathesArray = LSSharedFileListCopySnapshot(list, &seed);
			CFIndex pathesCount = CFArrayGetCount(pathesArray);

			for (CFIndex i = 0; i < pathesCount; i++) {
				LSSharedFileListItemRef itemRef = (LSSharedFileListItemRef)CFArrayGetValueAtIndex(pathesArray, i);

				CFURLRef cfURL = NULL;
				OSErr err = LSSharedFileListItemResolve(itemRef,
				                                        kLSSharedFileListNoUserInteraction |
				                                        kLSSharedFileListDoNotMountVolumes,
				                                        &cfURL, NULL);
				if (err != noErr || !cfURL)
					continue;

				CFStringRef pathString = CFURLCopyFileSystemPath(cfURL, kCFURLPOSIXPathStyle);

				if (pathString == NULL || !CFStringGetCString(pathString, line, sizeof(line), kCFStringEncodingUTF8))
					continue;

				/* Add end slash for consistency with other platforms */
				BLI_add_slash(line);

				/* Exclude "all my files" as it makes no sense in blender fileselector */
				/* Exclude "airdrop" if wlan not active as it would show "" ) */
				if (!strstr(line, "myDocuments.cannedSearch") && (*line != '\0')) {
					fsmenu_insert_entry(fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS, line, NULL, FS_INSERT_LAST);
				}

				CFRelease(pathString);
				CFRelease(cfURL);
			}

			CFRelease(pathesArray);
			CFRelease(list);
		}
	}
#else
	/* unix */
	{
		const char *home = BLI_getenv("HOME");

		if (read_bookmarks && home) {
			BLI_snprintf(line, sizeof(line), "%s/", home);
			fsmenu_insert_entry(fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS, line, NULL, FS_INSERT_SORTED);
			BLI_snprintf(line, sizeof(line), "%s/Desktop/", home);
			if (BLI_exists(line)) {
				fsmenu_insert_entry(fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS, line, NULL, FS_INSERT_SORTED);
			}
		}

		{
			int found = 0;
#ifdef __linux__
			/* loop over mount points */
			struct mntent *mnt;
			int len;
			FILE *fp;

			fp = setmntent(MOUNTED, "r");
			if (fp == NULL) {
				fprintf(stderr, "could not get a list of mounted filesystemts\n");
			}
			else {
				while ((mnt = getmntent(fp))) {
					/* not sure if this is right, but seems to give the relevant mnts */
					if (!STREQLEN(mnt->mnt_fsname, "/dev", 4))
						continue;

					len = strlen(mnt->mnt_dir);
					if (len && mnt->mnt_dir[len - 1] != '/') {
						BLI_snprintf(line, sizeof(line), "%s/", mnt->mnt_dir);
						fsmenu_insert_entry(fsmenu, FS_CATEGORY_SYSTEM, line, NULL, FS_INSERT_SORTED);
					}
					else {
						fsmenu_insert_entry(fsmenu, FS_CATEGORY_SYSTEM, mnt->mnt_dir, NULL, FS_INSERT_SORTED);
					}

					found = 1;
				}
				if (endmntent(fp) == 0) {
					fprintf(stderr, "could not close the list of mounted filesystemts\n");
				}
			}
#endif

			/* fallback */
			if (!found)
				fsmenu_insert_entry(fsmenu, FS_CATEGORY_SYSTEM, "/", NULL, FS_INSERT_SORTED);
		}
	}
#endif
#endif
}


static void fsmenu_free_category(struct FSMenu *fsmenu, FSMenuCategory category)
{
	FSMenuEntry *fsm_iter = ED_fsmenu_get_category(fsmenu, category);

	while (fsm_iter) {
		FSMenuEntry *fsm_next = fsm_iter->next;

		if (fsm_iter->path) {
			MEM_freeN(fsm_iter->path);
		}
		MEM_freeN(fsm_iter);

		fsm_iter = fsm_next;
	}
}

void fsmenu_refresh_system_category(struct FSMenu *fsmenu)
{
	fsmenu_free_category(fsmenu, FS_CATEGORY_SYSTEM);
	ED_fsmenu_set_category(fsmenu, FS_CATEGORY_SYSTEM, NULL);

	fsmenu_free_category(fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS);
	ED_fsmenu_set_category(fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS, NULL);

	/* Add all entries to system category */
	fsmenu_read_system(fsmenu, true);
}

void fsmenu_refresh_bookmarks_status(struct FSMenu *fsmenu)
{
	int categories[] = {FS_CATEGORY_SYSTEM, FS_CATEGORY_SYSTEM_BOOKMARKS, FS_CATEGORY_BOOKMARKS, FS_CATEGORY_RECENT};
	int i;

	for (i = sizeof(categories) / sizeof(*categories); i--; ) {
		FSMenuEntry *fsm_iter = ED_fsmenu_get_category(fsmenu, categories[i]);
		for ( ; fsm_iter; fsm_iter = fsm_iter->next) {
			fsmenu_entry_refresh_valid(fsm_iter);
		}
	}
}

void fsmenu_free(void)
{
	if (g_fsmenu) {
		fsmenu_free_category(g_fsmenu, FS_CATEGORY_SYSTEM);
		fsmenu_free_category(g_fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS);
		fsmenu_free_category(g_fsmenu, FS_CATEGORY_BOOKMARKS);
		fsmenu_free_category(g_fsmenu, FS_CATEGORY_RECENT);
		MEM_freeN(g_fsmenu);
	}

	g_fsmenu = NULL;
}

int fsmenu_get_active_indices(struct FSMenu *fsmenu, enum FSMenuCategory category, const char *dir)
{
	FSMenuEntry *fsm_iter = ED_fsmenu_get_category(fsmenu, category);
	int i;

	for (i = 0; fsm_iter; fsm_iter = fsm_iter->next, i++) {
		if (BLI_path_cmp(dir, fsm_iter->path) == 0) {
			return i;
		}
	}

	return -1;
}

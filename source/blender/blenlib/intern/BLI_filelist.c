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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/BLI_filelist.c
 *  \ingroup bli
 */

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef WIN32
#  include <dirent.h>
#endif

#include <time.h>
#include <sys/stat.h>
#include <string.h>  /* strcpy etc.. */

#ifdef WIN32
#  include <io.h>
#  include <direct.h>
#  include "BLI_winstuff.h"
#  include "utfconv.h"
#else
#  include <sys/ioctl.h>
#  include <unistd.h>
#  include <pwd.h>
#endif

/* lib includes */
#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_fileops.h"
#include "BLI_fileops_types.h"
#include "BLI_path_util.h"

#include "../imbuf/IMB_imbuf.h"


/*
 * Ordering function for sorting lists of files/directories. Returns -1 if
 * entry1 belongs before entry2, 0 if they are equal, 1 if they should be swapped.
 */
static int bli_compare(struct direntry *entry1, struct direntry *entry2)
{
	/* type is equal to stat.st_mode */

	/* directories come before non-directories */
	if (S_ISDIR(entry1->type)) {
		if (S_ISDIR(entry2->type) == 0) return (-1);
	}
	else {
		if (S_ISDIR(entry2->type)) return (1);
	}
	/* non-regular files come after regular files */
	if (S_ISREG(entry1->type)) {
		if (S_ISREG(entry2->type) == 0) return (-1);
	}
	else {
		if (S_ISREG(entry2->type)) return (1);
	}
	/* arbitrary, but consistent, ordering of different types of non-regular files */
	if ((entry1->type & S_IFMT) < (entry2->type & S_IFMT)) return (-1);
	if ((entry1->type & S_IFMT) > (entry2->type & S_IFMT)) return (1);

	/* OK, now we know their S_IFMT fields are the same, go on to a name comparison */
	/* make sure "." and ".." are always first */
	if (FILENAME_IS_CURRENT(entry1->relname)) return (-1);
	if (FILENAME_IS_CURRENT(entry2->relname)) return (1);
	if (FILENAME_IS_PARENT(entry1->relname)) return (-1);
	if (FILENAME_IS_PARENT(entry2->relname)) return (1);

	return (BLI_natstrcmp(entry1->relname, entry2->relname));
}


struct BuildDirCtx {
	struct direntry *files; /* array[nrfiles] */
	int nrfiles;
};

/**
 * Scans the directory named *dirname and appends entries for its contents to files.
 */
static void bli_builddir(struct BuildDirCtx *dir_ctx, const char *dirname)
{
	struct ListBase dirbase = {NULL, NULL};
	int newnum = 0;
	DIR *dir;

	if ((dir = opendir(dirname)) != NULL) {
		const struct dirent *fname;
		bool has_current = false, has_parent = false;

		while ((fname = readdir(dir)) != NULL) {
			struct dirlink * const dlink = (struct dirlink *)malloc(sizeof(struct dirlink));
			if (dlink != NULL) {
				dlink->name = BLI_strdup(fname->d_name);
				if (FILENAME_IS_PARENT(dlink->name)) {
					has_parent = true;
				}
				else if (FILENAME_IS_CURRENT(dlink->name)) {
					has_current = true;
				}
				BLI_addhead(&dirbase, dlink);
				newnum++;
			}
		}

		if (!has_parent) {
			char pardir[FILE_MAXDIR];

			BLI_strncpy(pardir, dirname, sizeof(pardir));
			if (BLI_parent_dir(pardir) && (BLI_access(pardir, R_OK) == 0)) {
				struct dirlink * const dlink = (struct dirlink *)malloc(sizeof(struct dirlink));
				if (dlink != NULL) {
					dlink->name = BLI_strdup(FILENAME_PARENT);
					BLI_addhead(&dirbase, dlink);
					newnum++;
				}
			}
		}
		if (!has_current) {
			struct dirlink * const dlink = (struct dirlink *)malloc(sizeof(struct dirlink));
			if (dlink != NULL) {
				dlink->name = BLI_strdup(FILENAME_CURRENT);
				BLI_addhead(&dirbase, dlink);
				newnum++;
			}
		}

		if (newnum) {
			if (dir_ctx->files) {
				void * const tmp = MEM_reallocN(dir_ctx->files, (dir_ctx->nrfiles + newnum) * sizeof(struct direntry));
				if (tmp) {
					dir_ctx->files = (struct direntry *)tmp;
				}
				else { /* realloc fail */
					MEM_freeN(dir_ctx->files);
					dir_ctx->files = NULL;
				}
			}

			if (dir_ctx->files == NULL)
				dir_ctx->files = (struct direntry *)MEM_mallocN(newnum * sizeof(struct direntry), __func__);

			if (dir_ctx->files) {
				struct dirlink * dlink = (struct dirlink *) dirbase.first;
				struct direntry *file = &dir_ctx->files[dir_ctx->nrfiles];
				while (dlink) {
					char fullname[PATH_MAX];
					memset(file, 0, sizeof(struct direntry));
					file->relname = dlink->name;
					file->path = BLI_strdupcat(dirname, dlink->name);
					BLI_join_dirfile(fullname, sizeof(fullname), dirname, dlink->name);
					if (BLI_stat(fullname, &file->s) != -1) {
						file->type = file->s.st_mode;
					}
					else if (FILENAME_IS_CURRPAR(file->relname)) {
						/* Hack around for UNC paths on windows - does not support stat on '\\SERVER\foo\..', sigh... */
						file->type |= S_IFDIR;
					}
					file->flags = 0;
					dir_ctx->nrfiles++;
					file++;
					dlink = dlink->next;
				}
			}
			else {
				printf("Couldn't get memory for dir\n");
				exit(1);
			}

			BLI_freelist(&dirbase);
			if (dir_ctx->files) {
				qsort(dir_ctx->files, dir_ctx->nrfiles, sizeof(struct direntry), (int (*)(const void *, const void *))bli_compare);
			}
		}
		else {
			printf("%s empty directory\n", dirname);
		}

		closedir(dir);
	}
	else {
		printf("%s non-existent directory\n", dirname);
	}
}

/**
 * Fills in the "mode[123]", "size" and "string" fields in the elements of the files
 * array with descriptive details about each item. "string" will have a format similar to "ls -l".
 */
static void bli_adddirstrings(struct BuildDirCtx *dir_ctx)
{
	const char *types[8] = {"---", "--x", "-w-", "-wx", "r--", "r-x", "rw-", "rwx"};
	/* symbolic display, indexed by mode field value */
	int num;
#ifdef WIN32
	__int64 st_size;
#else
	off_t st_size;
	int mode;
#endif

	struct direntry *file;
	struct tm *tm;
	time_t zero = 0;

	for (num = 0, file = dir_ctx->files; num < dir_ctx->nrfiles; num++, file++) {


		/* Mode */
#ifdef WIN32
		BLI_strncpy(file->mode1, types[0], sizeof(file->mode1));
		BLI_strncpy(file->mode2, types[0], sizeof(file->mode2));
		BLI_strncpy(file->mode3, types[0], sizeof(file->mode3));
#else
		mode = file->s.st_mode;

		BLI_strncpy(file->mode1, types[(mode & 0700) >> 6], sizeof(file->mode1));
		BLI_strncpy(file->mode2, types[(mode & 0070) >> 3], sizeof(file->mode2));
		BLI_strncpy(file->mode3, types[(mode & 0007)],      sizeof(file->mode3));

		if (((mode & S_ISGID) == S_ISGID) && (file->mode2[2] == '-')) file->mode2[2] = 'l';

		if (mode & (S_ISUID | S_ISGID)) {
			if (file->mode1[2] == 'x') file->mode1[2] = 's';
			else file->mode1[2] = 'S';

			if (file->mode2[2] == 'x') file->mode2[2] = 's';
		}

		if (mode & S_ISVTX) {
			if (file->mode3[2] == 'x') file->mode3[2] = 't';
			else file->mode3[2] = 'T';
		}
#endif


		/* User */
#ifdef WIN32
		strcpy(file->owner, "user");
#else
		{
			struct passwd *pwuser;
			pwuser = getpwuid(file->s.st_uid);
			if (pwuser) {
				BLI_strncpy(file->owner, pwuser->pw_name, sizeof(file->owner));
			}
			else {
				BLI_snprintf(file->owner, sizeof(file->owner), "%u", file->s.st_uid);
			}
		}
#endif


		/* Time */
		tm = localtime(&file->s.st_mtime);
		// prevent impossible dates in windows
		if (tm == NULL) tm = localtime(&zero);
		strftime(file->time, sizeof(file->time), "%H:%M", tm);
		strftime(file->date, sizeof(file->date), "%d-%b-%y", tm);


		/* Size */
		/*
		 * Seems st_size is signed 32-bit value in *nix and Windows.  This
		 * will buy us some time until files get bigger than 4GB or until
		 * everyone starts using __USE_FILE_OFFSET64 or equivalent.
		 */
		st_size = file->s.st_size;

		if (st_size > 1024 * 1024 * 1024) {
			BLI_snprintf(file->size, sizeof(file->size), "%.2f GiB", ((double)st_size) / (1024 * 1024 * 1024));
		}
		else if (st_size > 1024 * 1024) {
			BLI_snprintf(file->size, sizeof(file->size), "%.1f MiB", ((double)st_size) / (1024 * 1024));
		}
		else if (st_size > 1024) {
			BLI_snprintf(file->size, sizeof(file->size), "%d KiB", (int)(st_size / 1024));
		}
		else {
			BLI_snprintf(file->size, sizeof(file->size), "%d B", (int)st_size);
		}
	}
}

/**
 * Scans the contents of the directory named *dirname, and allocates and fills in an
 * array of entries describing them in *filelist.
 *
 * \return The length of filelist array.
 */
unsigned int BLI_filelist_dir_contents(const char *dirname,  struct direntry **filelist)
{
	struct BuildDirCtx dir_ctx;

	dir_ctx.nrfiles = 0;
	dir_ctx.files = NULL;

	bli_builddir(&dir_ctx, dirname);
	bli_adddirstrings(&dir_ctx);

	if (dir_ctx.files) {
		*filelist = dir_ctx.files;
	}
	else {
		// keep blender happy. Blender stores this in a variable
		// where 0 has special meaning.....
		*filelist = MEM_mallocN(sizeof(**filelist), __func__);
	}

	return dir_ctx.nrfiles;
}

/**
 * Deep-duplicate of an array of direntries, including the array itself.
 *
 * \param dup_poin If given, called for each non-NULL direntry->poin. Otherwise, pointer is always simply copied over.
 */
void BLI_filelist_duplicate(
        struct direntry **dest_filelist, struct direntry *src_filelist, unsigned int nrentries,
        void *(*dup_poin)(void *))
{
	unsigned int i;

	*dest_filelist = MEM_mallocN(sizeof(**dest_filelist) * (size_t)(nrentries), __func__);
	for (i = 0; i < nrentries; ++i) {
		struct direntry * const src = &src_filelist[i];
		struct direntry *dest = &(*dest_filelist)[i];
		*dest = *src;
		if (dest->image) {
			dest->image = IMB_dupImBuf(src->image);
		}
		if (dest->relname) {
			dest->relname = MEM_dupallocN(src->relname);
		}
		if (dest->path) {
			dest->path = MEM_dupallocN(src->path);
		}
		if (dest->poin && dup_poin) {
			dest->poin = dup_poin(src->poin);
		}
	}
}

/**
 * frees storage for an array of direntries, including the array itself.
 */
void BLI_filelist_free(struct direntry *filelist, unsigned int nrentries, void (*free_poin)(void *))
{
	unsigned int i;
	for (i = 0; i < nrentries; ++i) {
		struct direntry *entry = filelist + i;
		if (entry->image) {
			IMB_freeImBuf(entry->image);
		}
		if (entry->relname)
			MEM_freeN(entry->relname);
		if (entry->path)
			MEM_freeN(entry->path);
		if (entry->poin && free_poin)
			free_poin(entry->poin);
	}

	if (filelist != NULL) {
		MEM_freeN(filelist);
	}
}

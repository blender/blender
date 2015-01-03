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
 * Reorganised mar-01 nzc
 * Some really low-level file thingies.
 */

/** \file blender/blenlib/intern/storage.c
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

#if defined(__NetBSD__) || defined(__DragonFly__) || defined(__sun__) || defined(__sun)
   /* Other modern unix os's should probably use this also */
#  include <sys/statvfs.h>
#  define USE_STATFS_STATVFS
#elif (defined(__sparc) || defined(__sparc__)) && !defined(__FreeBSD__) && !defined(__linux__)
#  include <sys/statfs.h>
   /* 4 argument version (not common) */
#  define USE_STATFS_4ARGS
#endif

#if defined(__APPLE__) || defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
   /* For statfs */
#  include <sys/param.h>
#  include <sys/mount.h>
#endif

#if defined(__linux__) || defined(__hpux) || defined(__GNU__) || defined(__GLIBC__)
#  include <sys/vfs.h>
#endif

#include <fcntl.h>
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
#include "BLI_linklist.h"
#include "BLI_string.h"
#include "BLI_fileops.h"
#include "BLI_fileops_types.h"
#include "BLI_path_util.h"

#include "../imbuf/IMB_imbuf.h"

/**
 * Copies the current working directory into *dir (max size maxncpy), and
 * returns a pointer to same.
 *
 * \note can return NULL when the size is not big enough
 */
char *BLI_current_working_dir(char *dir, const size_t maxncpy)
{
	const char *pwd = getenv("PWD");
	if (pwd) {
		BLI_strncpy(dir, pwd, maxncpy);
		return dir;
	}

	return getcwd(dir, maxncpy);
}

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
	if (strcmp(entry1->relname, ".") == 0) return (-1);
	if (strcmp(entry2->relname, ".") == 0) return (1);
	if (strcmp(entry1->relname, "..") == 0) return (-1);
	if (strcmp(entry2->relname, "..") == 0) return (1);

	return (BLI_natstrcmp(entry1->relname, entry2->relname));
}

/**
 * Returns the number of free bytes on the volume containing the specified pathname. */
/* Not actually used anywhere.
 */
double BLI_dir_free_space(const char *dir)
{
#ifdef WIN32
	DWORD sectorspc, bytesps, freec, clusters;
	char tmp[4];
	
	tmp[0] = '\\'; tmp[1] = 0; /* Just a failsafe */
	if (dir[0] == '/' || dir[0] == '\\') {
		tmp[0] = '\\';
		tmp[1] = 0;
	}
	else if (dir[1] == ':') {
		tmp[0] = dir[0];
		tmp[1] = ':';
		tmp[2] = '\\';
		tmp[3] = 0;
	}

	GetDiskFreeSpace(tmp, &sectorspc, &bytesps, &freec, &clusters);

	return (double) (freec * bytesps * sectorspc);
#else

#ifdef USE_STATFS_STATVFS
	struct statvfs disk;
#else
	struct statfs disk;
#endif

	char name[FILE_MAXDIR], *slash;
	int len = strlen(dir);
	
	if (len >= FILE_MAXDIR) /* path too long */
		return -1;
	
	strcpy(name, dir);

	if (len) {
		slash = strrchr(name, '/');
		if (slash) slash[1] = 0;
	}
	else {
		strcpy(name, "/");
	}

#if  defined(USE_STATFS_STATVFS)
	if (statvfs(name, &disk)) return -1;
#elif defined(USE_STATFS_4ARGS)
	if (statfs(name, &disk, sizeof(struct statfs), 0)) return -1;
#else
	if (statfs(name, &disk)) return -1;
#endif

	return ( ((double) disk.f_bsize) * ((double) disk.f_bfree));
#endif
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
		while ((fname = readdir(dir)) != NULL) {
			struct dirlink * const dlink = (struct dirlink *)malloc(sizeof(struct dirlink));
			if (dlink != NULL) {
				dlink->name = BLI_strdup(fname->d_name);
				BLI_addhead(&dirbase, dlink);
				newnum++;
			}
		}

		if (newnum) {

			if (dir_ctx->files) {
				void * const tmp = realloc(dir_ctx->files, (dir_ctx->nrfiles + newnum) * sizeof(struct direntry));
				if (tmp) {
					dir_ctx->files = (struct direntry *)tmp;
				}
				else { /* realloc fail */
					free(dir_ctx->files);
					dir_ctx->files = NULL;
				}
			}
			
			if (dir_ctx->files == NULL)
				dir_ctx->files = (struct direntry *)malloc(newnum * sizeof(struct direntry));

			if (dir_ctx->files) {
				struct dirlink * dlink = (struct dirlink *) dirbase.first;
				struct direntry *file = &dir_ctx->files[dir_ctx->nrfiles];
				while (dlink) {
					char fullname[PATH_MAX];
					memset(file, 0, sizeof(struct direntry));
					file->relname = dlink->name;
					file->path = BLI_strdupcat(dirname, dlink->name);
					BLI_join_dirfile(fullname, sizeof(fullname), dirname, dlink->name);
					BLI_stat(fullname, &file->s);
					file->type = file->s.st_mode;
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
		*filelist = malloc(sizeof(struct direntry));
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

	*dest_filelist = malloc(sizeof(**dest_filelist) * (size_t)(nrentries));
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
		struct direntry * const entry = filelist + i;
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

	free(filelist);
}


/**
 * Returns the file size of an opened file descriptor.
 */
size_t BLI_file_descriptor_size(int file)
{
	struct stat st;
	if ((file < 0) || (fstat(file, &st) == -1))
		return -1;
	return st.st_size;
}

/**
 * Returns the size of a file.
 */
size_t BLI_file_size(const char *path)
{
	BLI_stat_t stats;
	if (BLI_stat(path, &stats) == -1)
		return -1;
	return stats.st_size;
}

/**
 * Returns the st_mode from statting the specified path name, or 0 if it couldn't be statted
 * (most likely doesn't exist or no access).
 */
int BLI_exists(const char *name)
{
#if defined(WIN32) 
	BLI_stat_t st;
	wchar_t *tmp_16 = alloc_utf16_from_8(name, 1);
	int len, res;
	unsigned int old_error_mode;

	len = wcslen(tmp_16);
	/* in Windows #stat doesn't recognize dir ending on a slash
	 * so we remove it here */
	if (len > 3 && (tmp_16[len - 1] == L'\\' || tmp_16[len - 1] == L'/')) {
		tmp_16[len - 1] = '\0';
	}
	/* two special cases where the trailing slash is needed:
	 * 1. after the share part of a UNC path
	 * 2. after the C:\ when the path is the volume only
	 */
	if ((len >= 3) && (tmp_16[0] ==  L'\\') && (tmp_16[1] ==  L'\\')) {
		BLI_cleanup_unc_16(tmp_16);
	}

	if ((tmp_16[1] ==  L':') && (tmp_16[2] ==  L'\0')) {
		tmp_16[2] = L'\\';
		tmp_16[3] = L'\0';
	}


	/* change error mode so user does not get a "no disk in drive" popup
	 * when looking for a file on an empty CD/DVD drive */
	old_error_mode = SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOOPENFILEERRORBOX);

	res = BLI_wstat(tmp_16, &st);

	SetErrorMode(old_error_mode);

	free(tmp_16);
	if (res == -1) return(0);
#else
	struct stat st;
	if (stat(name, &st)) return(0);
#endif
	return(st.st_mode);
}


#ifdef WIN32
int BLI_stat(const char *path, BLI_stat_t *buffer)
{
	int r;
	UTF16_ENCODE(path);

	r = BLI_wstat(path_16, buffer);

	UTF16_UN_ENCODE(path);
	return r;
}

int BLI_wstat(const wchar_t *path, BLI_stat_t *buffer)
{
#if defined(_MSC_VER) || defined(__MINGW64__)
	return _wstat64(path, buffer);
#elif defined(__MINGW32__)
	return _wstati64(path, buffer);
#else
	return _wstat(path, buffer);
#endif
}
#else
int BLI_stat(const char *path, struct stat *buffer)
{
	return stat(path, buffer);
}
#endif

/**
 * Does the specified path point to a directory?
 * \note Would be better in fileops.c except that it needs stat.h so add here
 */
bool BLI_is_dir(const char *file)
{
	return S_ISDIR(BLI_exists(file));
}

/**
 * Does the specified path point to a non-directory?
 */
bool BLI_is_file(const char *path)
{
	const int mode = BLI_exists(path);
	return (mode && !S_ISDIR(mode));
}

/**
 * Reads the contents of a text file and returns the lines in a linked list.
 */
LinkNode *BLI_file_read_as_lines(const char *name)
{
	FILE *fp = BLI_fopen(name, "r");
	LinkNode *lines = NULL;
	char *buf;
	size_t size;

	if (!fp) return NULL;
		
	fseek(fp, 0, SEEK_END);
	size = (size_t)ftell(fp);
	fseek(fp, 0, SEEK_SET);

	buf = MEM_mallocN(size, "file_as_lines");
	if (buf) {
		size_t i, last = 0;
		
		/*
		 * size = because on win32 reading
		 * all the bytes in the file will return
		 * less bytes because of crnl changes.
		 */
		size = fread(buf, 1, size, fp);
		for (i = 0; i <= size; i++) {
			if (i == size || buf[i] == '\n') {
				char *line = BLI_strdupn(&buf[last], i - last);

				BLI_linklist_prepend(&lines, line);
				/* faster to build singly-linked list in reverse order */
				/* alternatively, could process buffer in reverse order so
				 * list ends up right way round to start with */
				last = i + 1;
			}
		}
		
		MEM_freeN(buf);
	}
	
	fclose(fp);

	/* get them the right way round */
	BLI_linklist_reverse(&lines);
	return lines;
}

/*
 * Frees memory from a previous call to BLI_file_read_as_lines.
 */
void BLI_file_free_lines(LinkNode *lines)
{
	BLI_linklist_freeN(lines);
}

/** is file1 older then file2 */
bool BLI_file_older(const char *file1, const char *file2)
{
#ifdef WIN32
#ifndef __MINGW32__
	struct _stat st1, st2;
#else
	struct _stati64 st1, st2;
#endif

	UTF16_ENCODE(file1);
	UTF16_ENCODE(file2);
	
#ifndef __MINGW32__
	if (_wstat(file1_16, &st1)) return false;
	if (_wstat(file2_16, &st2)) return false;
#else
	if (_wstati64(file1_16, &st1)) return false;
	if (_wstati64(file2_16, &st2)) return false;
#endif


	UTF16_UN_ENCODE(file2);
	UTF16_UN_ENCODE(file1);
#else
	struct stat st1, st2;

	if (stat(file1, &st1)) return false;
	if (stat(file2, &st2)) return false;
#endif
	return (st1.st_mtime < st2.st_mtime);
}


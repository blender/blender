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
 */

/** \file blender/blenlib/intern/fileops.c
 *  \ingroup bli
 */


#include <stdlib.h>  /* malloc */
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>

#include "zlib.h"

#ifdef WIN32
#  ifdef __MINGW32__
#    include <ctype.h>
#  endif
#  include <io.h>
#  include "BLI_winstuff.h"
#  include "BLI_callbacks.h"
#  include "BLI_fileops_types.h"
#  include "utf_winfunc.h"
#  include "utfconv.h"
#else
#  include <sys/param.h>
#  include <dirent.h>
#  include <unistd.h>
#  include <sys/stat.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_string.h"
#include "BLI_path_util.h"
#include "BLI_fileops.h"
#include "BLI_sys_types.h" // for intptr_t support

#if 0  /* UNUSED */
/* gzip the file in from and write it to "to". 
 * return -1 if zlib fails, -2 if the originating file does not exist
 * note: will remove the "from" file
 */
int BLI_file_gzip(const char *from, const char *to)
{
	char buffer[10240];
	int file;
	int readsize = 0;
	int rval = 0, err;
	gzFile gzfile;

	/* level 1 is very close to 3 (the default) in terms of file size,
	 * but about twice as fast, best use for speedy saving - campbell */
	gzfile = BLI_gzopen(to, "wb1");
	if (gzfile == NULL)
		return -1;
	file = BLI_open(from, O_BINARY | O_RDONLY, 0);
	if (file == -1)
		return -2;

	while (1) {
		readsize = read(file, buffer, sizeof(buffer));

		if (readsize < 0) {
			rval = -2; /* error happened in reading */
			fprintf(stderr, "Error reading file %s: %s.\n", from, strerror(errno));
			break;
		}
		else if (readsize == 0)
			break;  /* done reading */
		
		if (gzwrite(gzfile, buffer, readsize) <= 0) {
			rval = -1; /* error happened in writing */
			fprintf(stderr, "Error writing gz file %s: %s.\n", to, gzerror(gzfile, &err));
			break;
		}
	}
	
	gzclose(gzfile);
	close(file);

	return rval;
}
#endif

/* gzip the file in from_file and write it to memory to_mem, at most size bytes.
 * return the unziped size
 */
char *BLI_file_ungzip_to_mem(const char *from_file, int *r_size)
{
	gzFile gzfile;
	int readsize, size, alloc_size = 0;
	char *mem = NULL;
	const int chunk_size = 512 * 1024;

	size = 0;

	gzfile = BLI_gzopen(from_file, "rb");
	for (;; ) {
		if (mem == NULL) {
			mem = MEM_callocN(chunk_size, "BLI_ungzip_to_mem");
			alloc_size = chunk_size;
		}
		else {
			mem = MEM_reallocN(mem, size + chunk_size);
			alloc_size += chunk_size;
		}

		readsize = gzread(gzfile, mem + size, chunk_size);
		if (readsize > 0) {
			size += readsize;
		}
		else {
			break;
		}
	}
	
	gzclose(gzfile);

	if (size == 0) {
		MEM_freeN(mem);
		mem = NULL;
	}
	else if (alloc_size != size)
		mem = MEM_reallocN(mem, size);

	*r_size = size;

	return mem;
}

/**
 * Returns true if the file with the specified name can be written.
 * This implementation uses access(2), which makes the check according
 * to the real UID and GID of the process, not its effective UID and GID.
 * This shouldn't matter for Blender, which is not going to run privileged
 * anyway.
 */
bool BLI_file_is_writable(const char *filename)
{
	bool writable;
	if (BLI_access(filename, W_OK) == 0) {
		/* file exists and I can write to it */
		writable = true;
	}
	else if (errno != ENOENT) {
		/* most likely file or containing directory cannot be accessed */
		writable = false;
	}
	else {
		/* file doesn't exist -- check I can create it in parent directory */
		char parent[FILE_MAX];
		BLI_split_dirfile(filename, parent, NULL, sizeof(parent), 0);
#ifdef WIN32
		/* windows does not have X_OK */
		writable = BLI_access(parent, W_OK) == 0;
#else
		writable = BLI_access(parent, X_OK | W_OK) == 0;
#endif
	}
	return writable;
}

/**
 * Creates the file with nothing in it, or updates its last-modified date if it already exists.
 * Returns true if successful. (like the unix touch command)
 */
bool BLI_file_touch(const char *file)
{
	FILE *f = BLI_fopen(file, "r+b");
	if (f != NULL) {
		int c = getc(f);
		rewind(f);
		putc(c, f);
	}
	else {
		f = BLI_fopen(file, "wb");
	}
	if (f) {
		fclose(f);
		return true;
	}
	return false;
}

#ifdef WIN32

static void callLocalErrorCallBack(const char *err)
{
	printf("%s\n", err);
}

static char str[MAXPATHLEN + 12];

FILE *BLI_fopen(const char *filename, const char *mode)
{
	return ufopen(filename, mode);
}

void BLI_get_short_name(char short_name[256], const char *filename)
{
	wchar_t short_name_16[256];
	int i = 0;

	UTF16_ENCODE(filename);

	GetShortPathNameW(filename_16, short_name_16, 256);

	for (i = 0; i < 256; i++) {
		short_name[i] = (char)short_name_16[i];
	}

	UTF16_UN_ENCODE(filename);
}

void *BLI_gzopen(const char *filename, const char *mode)
{
	gzFile gzfile;

	if (!filename || !mode) {
		return 0;
	}
	else {
		/* xxx Creates file before transcribing the path */
		if (mode[0] == 'w')
			fclose(ufopen(filename, "a"));

		/* temporary #if until we update all libraries to 1.2.7
		 * for correct wide char path handling */
#if ZLIB_VERNUM >= 0x1270 && !defined(FREE_WINDOWS)
		UTF16_ENCODE(filename);

		gzfile = gzopen_w(filename_16, mode);

		UTF16_UN_ENCODE(filename);
#else
		{
			char short_name[256];
			BLI_get_short_name(short_name, filename);
			gzfile = gzopen(short_name, mode);
		}
#endif
	}

	return gzfile;
}

int   BLI_open(const char *filename, int oflag, int pmode)
{
	return uopen(filename, oflag, pmode);
}

int   BLI_access(const char *filename, int mode)
{
	return uaccess(filename, mode);
}

static bool delete_unique(const char *path, const bool dir)
{
	bool err;

	UTF16_ENCODE(path);

	if (dir) {
		err = !RemoveDirectoryW(path_16);
		if (err) printf("Unable to remove directory");
	}
	else {
		err = !DeleteFileW(path_16);
		if (err) callLocalErrorCallBack("Unable to delete file");
	}

	UTF16_UN_ENCODE(path);

	return err;
}

static bool delete_recursive(const char *dir)
{
	struct direntry *filelist, *fl;
	bool err = false;
	unsigned int nbr, i;

	i = nbr = BLI_filelist_dir_contents(dir, &filelist);
	fl = filelist;
	while (i--) {
		char file[8];
		BLI_split_file_part(fl->path, file, sizeof(file));
		if (STREQ(file, ".") || STREQ(file, "..")) {
			/* Skip! */
		}
		else if (S_ISDIR(fl->type)) {
			if (delete_recursive(fl->path)) {
				err = true;
			}
		}
		else {
			if (delete_unique(fl->path, false)) {
				err = true;
			}
		}
		++fl;
	}

	if (!err && delete_unique(dir, true)) {
		err = true;
	}

	BLI_filelist_free(filelist, nbr, NULL);

	return err;
}

int BLI_delete(const char *file, bool dir, bool recursive)
{
	int err;

	if (recursive) {
		err = delete_recursive(file);
	}
	else {
		err = delete_unique(file, dir);
	}

	return err;
}

/* Not used anywhere! */
#if 0
int BLI_move(const char *file, const char *to)
{
	int err;

	/* windows doesn't support moving to a directory
	 * it has to be 'mv filename filename' and not
	 * 'mv filename destdir' */

	BLI_strncpy(str, to, sizeof(str));
	/* points 'to' to a directory ? */
	if (BLI_last_slash(str) == (str + strlen(str) - 1)) {
		if (BLI_last_slash(file) != NULL) {
			strcat(str, BLI_last_slash(file) + 1);
		}
	}
	
	UTF16_ENCODE(file);
	UTF16_ENCODE(str);
	err = !MoveFileW(file_16, str_16);
	UTF16_UN_ENCODE(str);
	UTF16_UN_ENCODE(file);

	if (err) {
		callLocalErrorCallBack("Unable to move file");
		printf(" Move from '%s' to '%s' failed\n", file, str);
	}

	return err;
}
#endif

int BLI_copy(const char *file, const char *to)
{
	int err;

	/* windows doesn't support copying to a directory
	 * it has to be 'cp filename filename' and not
	 * 'cp filename destdir' */

	BLI_strncpy(str, to, sizeof(str));
	/* points 'to' to a directory ? */
	if (BLI_last_slash(str) == (str + strlen(str) - 1)) {
		if (BLI_last_slash(file) != NULL) {
			strcat(str, BLI_last_slash(file) + 1);
		}
	}

	UTF16_ENCODE(file);
	UTF16_ENCODE(str);
	err = !CopyFileW(file_16, str_16, false);
	UTF16_UN_ENCODE(str);
	UTF16_UN_ENCODE(file);

	if (err) {
		callLocalErrorCallBack("Unable to copy file!");
		printf(" Copy from '%s' to '%s' failed\n", file, str);
	}

	return err;
}

int BLI_create_symlink(const char *file, const char *to)
{
	callLocalErrorCallBack("Linking files is unsupported on Windows");
	(void)file;
	(void)to;
	return 1;
}

void BLI_dir_create_recursive(const char *dirname)
{
	char *lslash;
	char tmp[MAXPATHLEN];

	/* First remove possible slash at the end of the dirname.
	 * This routine otherwise tries to create
	 * blah1/blah2/ (with slash) after creating
	 * blah1/blah2 (without slash) */

	BLI_strncpy(tmp, dirname, sizeof(tmp));
	lslash = (char *)BLI_last_slash(tmp);

	if (lslash && (*(lslash + 1) == '\0')) {
		*lslash = '\0';
	}

	/* check special case "c:\foo", don't try create "c:", harmless but prints an error below */
	if (isalpha(tmp[0]) && (tmp[1] == ':') && tmp[2] == '\0') return;

	if (BLI_exists(tmp)) return;

	lslash = (char *)BLI_last_slash(tmp);

	if (lslash) {
		/* Split about the last slash and recurse */
		*lslash = 0;
		BLI_dir_create_recursive(tmp);
	}

	if (dirname[0]) {  /* patch, this recursive loop tries to create a nameless directory */
		if (umkdir(dirname) == -1) {
			printf("Unable to create directory %s\n", dirname);
		}
	}
}

int BLI_rename(const char *from, const char *to)
{
	if (!BLI_exists(from)) return 0;

	/* make sure the filenames are different (case insensitive) before removing */
	if (BLI_exists(to) && BLI_strcasecmp(from, to))
		if (BLI_delete(to, false, false)) return 1;
	
	return urename(from, to);
}

#else /* The UNIX world */

/* results from recursive_operation and its callbacks */
enum {
	/* operation succeeded */
	RecursiveOp_Callback_OK = 0,

	/* operation requested not to perform recursive digging for current path */
	RecursiveOp_Callback_StopRecurs = 1,

	/* error occured in callback and recursive walking should stop immediately */
	RecursiveOp_Callback_Error = 2
};

typedef int (*RecursiveOp_Callback)(const char *from, const char *to);

/* appending of filename to dir (ensures for buffer size before appending) */
static void join_dirfile_alloc(char **dst, size_t *alloc_len, const char *dir, const char *file)
{
	size_t len = strlen(dir) + strlen(file) + 1;

	if (*dst == NULL)
		*dst = MEM_mallocN(len + 1, "join_dirfile_alloc path");
	else if (*alloc_len < len)
		*dst = MEM_reallocN(*dst, len + 1);

	*alloc_len = len;

	BLI_join_dirfile(*dst, len + 1, dir, file);
}

static char *strip_last_slash(const char *dir)
{
	char *result = BLI_strdup(dir);
	BLI_del_slash(result);

	return result;
}



/**
 * Scans \a startfrom, generating a corresponding destination name for each item found by
 * prefixing it with startto, recursively scanning subdirectories, and invoking the specified
 * callbacks for files and subdirectories found as appropriate.
 *
 * \param startfrom  Top-level source path.
 * \param startto  Top-level destination path.
 * \param callback_dir_pre  Optional, to be invoked before entering a subdirectory, can return
 *                          RecursiveOp_Callback_StopRecurs to skip the subdirectory.
 * \param callback_file  Optional, to be invoked on each file found.
 * \param callback_dir_post  optional, to be invoked after leaving a subdirectory.
 * \return
 */
static int recursive_operation(const char *startfrom, const char *startto,
                               RecursiveOp_Callback callback_dir_pre,
                               RecursiveOp_Callback callback_file, RecursiveOp_Callback callback_dir_post)
{
	struct stat st;
	char *from = NULL, *to = NULL;
	char *from_path = NULL, *to_path = NULL;
	struct dirent **dirlist = NULL;
	size_t from_alloc_len = -1, to_alloc_len = -1;
	int i, n, ret = 0;

	do {  /* once */
		/* ensure there's no trailing slash in file path */
		from = strip_last_slash(startfrom);
		if (startto)
			to = strip_last_slash(startto);

		ret = lstat(from, &st);
		if (ret < 0)
			/* source wasn't found, nothing to operate with */
			break;

		if (!S_ISDIR(st.st_mode)) {
			/* source isn't a directory, can't do recursive walking for it,
			 * so just call file callback and leave */
			if (callback_file != NULL) {
				ret = callback_file(from, to);
				if (ret != RecursiveOp_Callback_OK)
					ret = -1;
			}
			break;
		}

		n = scandir(startfrom, &dirlist, NULL, alphasort);
		if (n < 0) {
			/* error opening directory for listing */
			perror("scandir");
			ret = -1;
			break;
		}

		if (callback_dir_pre != NULL) {
			ret = callback_dir_pre(from, to);
			if (ret != RecursiveOp_Callback_OK) {
				if (ret == RecursiveOp_Callback_StopRecurs)
					/* callback requested not to perform recursive walking, not an error */
					ret = 0;
				else
					ret = -1;
				break;
			}
		}

		for (i = 0; i < n; i++) {
			const struct dirent * const dirent = dirlist[i];

			if (!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, ".."))
				continue;

			join_dirfile_alloc(&from_path, &from_alloc_len, from, dirent->d_name);
			if (to)
				join_dirfile_alloc(&to_path, &to_alloc_len, to, dirent->d_name);

			if (dirent->d_type == DT_DIR) {
				/* recursively dig into a subfolder */
				ret = recursive_operation(from_path, to_path, callback_dir_pre, callback_file, callback_dir_post);
			}
			else if (callback_file != NULL) {
				ret = callback_file(from_path, to_path);
				if (ret != RecursiveOp_Callback_OK)
					ret = -1;
			}

			if (ret != 0)
				break;
		}
		if (ret != 0)
			break;

		if (callback_dir_post != NULL) {
			ret = callback_dir_post(from, to);
			if (ret != RecursiveOp_Callback_OK)
				ret = -1;
		}
	}
	while (false);

	if (dirlist != NULL) {
		for (i = 0; i < n; i++) {
			free(dirlist[i]);
		}
		free(dirlist);
	}
	if (from_path != NULL)
		MEM_freeN(from_path);
	if (to_path != NULL)
		MEM_freeN(to_path);
	if (from != NULL)
		MEM_freeN(from);
	if (to != NULL)
		MEM_freeN(to);

	return ret;
}

static int delete_callback_post(const char *from, const char *UNUSED(to))
{
	if (rmdir(from)) {
		perror("rmdir");

		return RecursiveOp_Callback_Error;
	}

	return RecursiveOp_Callback_OK;
}

static int delete_single_file(const char *from, const char *UNUSED(to))
{
	if (unlink(from)) {
		perror("unlink");

		return RecursiveOp_Callback_Error;
	}

	return RecursiveOp_Callback_OK;
}

FILE *BLI_fopen(const char *filename, const char *mode)
{
	return fopen(filename, mode);
}

void *BLI_gzopen(const char *filename, const char *mode)
{
	return gzopen(filename, mode);
}

int BLI_open(const char *filename, int oflag, int pmode)
{
	return open(filename, oflag, pmode);
}

int   BLI_access(const char *filename, int mode)
{
	return access(filename, mode);
}


/**
 * Deletes the specified file or directory (depending on dir), optionally
 * doing recursive delete of directory contents.
 */
int BLI_delete(const char *file, bool dir, bool recursive)
{
	if (recursive) {
		return recursive_operation(file, NULL, NULL, delete_single_file, delete_callback_post);
	}
	else if (dir) {
		return rmdir(file);
	}
	else {
		return remove(file);
	}
}

/**
 * Do the two paths denote the same filesystem object?
 */
static bool check_the_same(const char *path_a, const char *path_b)
{
	struct stat st_a, st_b;

	if (lstat(path_a, &st_a))
		return false;

	if (lstat(path_b, &st_b))
		return false;

	return st_a.st_dev == st_b.st_dev && st_a.st_ino == st_b.st_ino;
}

/**
 * Sets the mode and ownership of file to the values from st.
 */
static int set_permissions(const char *file, const struct stat *st)
{
	if (chown(file, st->st_uid, st->st_gid)) {
		perror("chown");
		return -1;
	}

	if (chmod(file, st->st_mode)) {
		perror("chmod");
		return -1;
	}

	return 0;
}

/* pre-recursive callback for copying operation
 * creates a destination directory where all source content fill be copied to */
static int copy_callback_pre(const char *from, const char *to)
{
	struct stat st;

	if (check_the_same(from, to)) {
		fprintf(stderr, "%s: '%s' is the same as '%s'\n", __func__, from, to);
		return RecursiveOp_Callback_Error;
	}

	if (lstat(from, &st)) {
		perror("stat");
		return RecursiveOp_Callback_Error;
	}

	/* create a directory */
	if (mkdir(to, st.st_mode)) {
		perror("mkdir");
		return RecursiveOp_Callback_Error;
	}

	/* set proper owner and group on new directory */
	if (chown(to, st.st_uid, st.st_gid)) {
		perror("chown");
		return RecursiveOp_Callback_Error;
	}

	return RecursiveOp_Callback_OK;
}

static int copy_single_file(const char *from, const char *to)
{
	FILE *from_stream, *to_stream;
	struct stat st;
	char buf[4096];
	size_t len;

	if (check_the_same(from, to)) {
		fprintf(stderr, "%s: '%s' is the same as '%s'\n", __func__, from, to);
		return RecursiveOp_Callback_Error;
	}

	if (lstat(from, &st)) {
		perror("lstat");
		return RecursiveOp_Callback_Error;
	}

	if (S_ISLNK(st.st_mode)) {
		/* symbolic links should be copied in special way */
		char *link_buffer;
		int need_free;
		ssize_t link_len;

		/* get large enough buffer to read link content */
		if (st.st_size < sizeof(buf)) {
			link_buffer = buf;
			need_free = 0;
		}
		else {
			link_buffer = MEM_callocN(st.st_size + 2, "copy_single_file link_buffer");
			need_free = 1;
		}

		link_len = readlink(from, link_buffer, st.st_size + 1);
		if (link_len < 0) {
			perror("readlink");

			if (need_free) MEM_freeN(link_buffer);

			return RecursiveOp_Callback_Error;
		}

		link_buffer[link_len] = 0;

		if (symlink(link_buffer, to)) {
			perror("symlink");
			if (need_free) MEM_freeN(link_buffer);
			return RecursiveOp_Callback_Error;
		}

		if (need_free)
			MEM_freeN(link_buffer);

		return RecursiveOp_Callback_OK;
	}
	else if (S_ISCHR(st.st_mode) ||
	         S_ISBLK(st.st_mode) ||
	         S_ISFIFO(st.st_mode) ||
	         S_ISSOCK(st.st_mode))
	{
		/* copy special type of file */
		if (mknod(to, st.st_mode, st.st_rdev)) {
			perror("mknod");
			return RecursiveOp_Callback_Error;
		}

		if (set_permissions(to, &st))
			return RecursiveOp_Callback_Error;

		return RecursiveOp_Callback_OK;
	}
	else if (!S_ISREG(st.st_mode)) {
		fprintf(stderr, "Copying of this kind of files isn't supported yet\n");
		return RecursiveOp_Callback_Error;
	}

	from_stream = fopen(from, "rb");
	if (!from_stream) {
		perror("fopen");
		return RecursiveOp_Callback_Error;
	}

	to_stream = fopen(to, "wb");
	if (!to_stream) {
		perror("fopen");
		fclose(from_stream);
		return RecursiveOp_Callback_Error;
	}

	while ((len = fread(buf, 1, sizeof(buf), from_stream)) > 0) {
		fwrite(buf, 1, len, to_stream);
	}

	fclose(to_stream);
	fclose(from_stream);

	if (set_permissions(to, &st))
		return RecursiveOp_Callback_Error;

	return RecursiveOp_Callback_OK;
}

/* Not used anywhere! */
#if 0
static int move_callback_pre(const char *from, const char *to)
{
	int ret = rename(from, to);

	if (ret)
		return copy_callback_pre(from, to);

	return RecursiveOp_Callback_StopRecurs;
}

static int move_single_file(const char *from, const char *to)
{
	int ret = rename(from, to);

	if (ret)
		return copy_single_file(from, to);

	return RecursiveOp_Callback_OK;
}

/* if *file represents a directory, moves all its contents into *to, else renames
 * file itself to *to. */
int BLI_move(const char *file, const char *to)
{
	int ret = recursive_operation(file, to, move_callback_pre, move_single_file, NULL);

	if (ret && ret != -1) {
		return recursive_operation(file, NULL, NULL, delete_single_file, delete_callback_post);
	}

	return ret;
}
#endif

static const char *check_destination(const char *file, const char *to)
{
	struct stat st;

	if (!stat(to, &st)) {
		if (S_ISDIR(st.st_mode)) {
			char *str, *path;
			const char *filename;
			size_t len = 0;

			str = strip_last_slash(file);
			filename = BLI_last_slash(str);

			if (!filename) {
				MEM_freeN(str);
				return (char *)to;
			}

			/* skip slash */
			filename += 1;

			len = strlen(to) + strlen(filename) + 1;
			path = MEM_callocN(len + 1, "check_destination path");
			BLI_join_dirfile(path, len + 1, to, filename);

			MEM_freeN(str);

			return path;
		}
	}

	return to;
}

int BLI_copy(const char *file, const char *to)
{
	const char *actual_to = check_destination(file, to);
	int ret;

	ret = recursive_operation(file, actual_to, copy_callback_pre, copy_single_file, NULL);

	if (actual_to != to)
		MEM_freeN((void *)actual_to);

	return ret;
}

int BLI_create_symlink(const char *file, const char *to)
{
	return symlink(to, file);
}

void BLI_dir_create_recursive(const char *dirname)
{
	char *lslash;
	size_t size;
#ifdef MAXPATHLEN
	char static_buf[MAXPATHLEN];
#endif
	char *tmp;

	if (BLI_exists(dirname)) return;

#ifdef MAXPATHLEN
	size = MAXPATHLEN;
	tmp = static_buf;
#else
	size = strlen(dirname) + 1;
	tmp = MEM_callocN(size, __func__);
#endif

	BLI_strncpy(tmp, dirname, size);
		
	lslash = (char *)BLI_last_slash(tmp);
	if (lslash) {
		/* Split about the last slash and recurse */
		*lslash = 0;
		BLI_dir_create_recursive(tmp);
	}

#ifndef MAXPATHLEN
	MEM_freeN(tmp);
#endif

	mkdir(dirname, 0777);
}

int BLI_rename(const char *from, const char *to)
{
	if (!BLI_exists(from)) return 0;
	
	if (BLI_exists(to))
		if (BLI_delete(to, false, false)) return 1;

	return rename(from, to);
}

#endif

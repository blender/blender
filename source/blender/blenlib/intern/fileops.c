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


#include <string.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>

#include "zlib.h"

#ifdef WIN32
#include <io.h>
#  include "BLI_winstuff.h"
#  include "BLI_callbacks.h"
#  include "utf_winfunc.h"
#  include "utfconv.h"
#else
#  include <unistd.h> // for read close
#  include <sys/param.h>
#  include <dirent.h>
#  include <unistd.h>
#  include <sys/stat.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_utildefines.h"

#include "BLO_sys_types.h" // for intptr_t support


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
	if (file < 0)
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

/* gzip the file in from_file and write it to memery to_mem, at most size bytes.
 * return the unziped size
 */
char *BLI_file_ungzip_to_mem(const char *from_file, int *size_r)
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
		else break;
	}

	if (size == 0) {
		MEM_freeN(mem);
		mem = NULL;
	}
	else if (alloc_size != size)
		mem = MEM_reallocN(mem, size);

	*size_r = size;

	return mem;
}


/* return 1 when file can be written */
int BLI_file_is_writable(const char *filename)
{
	int file;
	
	/* first try to open without creating */
	file = BLI_open(filename, O_BINARY | O_RDWR, 0666);
	
	if (file < 0) {
		/* now try to open and create. a test without actually
		 * creating a file would be nice, but how? */
		file = BLI_open(filename, O_BINARY | O_RDWR | O_CREAT, 0666);
		
		if (file < 0) {
			return 0;
		}
		else {
			/* success, delete the file we create */
			close(file);
			BLI_delete(filename, 0, 0);
			return 1;
		}
	}
	else {
		close(file);
		return 1;
	}
}

int BLI_file_touch(const char *file)
{
	FILE *f = BLI_fopen(file, "r+b");
	if (f != NULL) {
		char c = getc(f);
		rewind(f);
		putc(c, f);
	}
	else {
		f = BLI_fopen(file, "wb");
	}
	if (f) {
		fclose(f);
		return 1;
	}
	return 0;
}

#ifdef WIN32

static char str[MAXPATHLEN + 12];

FILE *BLI_fopen(const char *filename, const char *mode)
{
	return ufopen(filename, mode);
}

void *BLI_gzopen(const char *filename, const char *mode)
{
	FILE *file;
	gzFile gzfile = NULL;
	wchar_t short_name_16[256];

	if (!filename || !mode) 
		return 0;

	/* xxx Creates file before transcribing the path */
	if (mode[0] == 'w')
		fclose(ufopen(filename, "a"));

	UTF16_ENCODE(filename);
	UTF16_ENCODE(mode);

	GetShortPathNameW(filename_16, short_name_16, 256);

	if ((file = _wfopen(short_name_16, mode_16))) {
		if (!(gzfile = gzdopen(fileno(file), mode))) {
			fclose(file);
		}
	}

	UTF16_UN_ENCODE(mode);
	UTF16_UN_ENCODE(filename);

	return gzfile;
}

int   BLI_open(const char *filename, int oflag, int pmode)
{
	return uopen(filename, oflag, pmode);
}

int BLI_delete(const char *file, int dir, int recursive)
{
	int err;
	
	UTF16_ENCODE(file);

	if (recursive) {
		callLocalErrorCallBack("Recursive delete is unsupported on Windows");
		err = 1;
	}
	else if (dir) {
		err = !RemoveDirectoryW(file_16);
		if (err) printf("Unable to remove directory");
	}
	else {
		err = !DeleteFileW(file_16);
		if (err) callLocalErrorCallBack("Unable to delete file");
	}

	UTF16_UN_ENCODE(file);

	return err;
}

int BLI_move(const char *file, const char *to)
{
	int err;

	// windows doesn't support moveing to a directory
	// it has to be 'mv filename filename' and not
	// 'mv filename destdir'

	BLI_strncpy(str, to, sizeof(str));
	// points 'to' to a directory ?
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


int BLI_copy(const char *file, const char *to)
{
	int err;

	// windows doesn't support copying to a directory
	// it has to be 'cp filename filename' and not
	// 'cp filename destdir'

	BLI_strncpy(str, to, sizeof(str));
	// points 'to' to a directory ?
	if (BLI_last_slash(str) == (str + strlen(str) - 1)) {
		if (BLI_last_slash(file) != NULL) {
			strcat(str, BLI_last_slash(file) + 1);
		}
	}

	UTF16_ENCODE(file);
	UTF16_ENCODE(str);
	err = !CopyFileW(file_16, str_16, FALSE);
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
	lslash = BLI_last_slash(tmp);
	
	if (lslash == tmp + strlen(tmp) - 1) {
		*lslash = 0;
	}
	
	if (BLI_exists(tmp)) return;

	lslash = BLI_last_slash(tmp);
	if (lslash) {
		/* Split about the last slash and recurse */
		*lslash = 0;
		BLI_dir_create_recursive(tmp);
	}
	
	if (dirname[0]) /* patch, this recursive loop tries to create a nameless directory */
		if (umkdir(dirname) == -1)
			printf("Unable to create directory %s\n", dirname);
}

int BLI_rename(const char *from, const char *to)
{
	if (!BLI_exists(from)) return 0;

	/* make sure the filenames are different (case insensitive) before removing */
	if (BLI_exists(to) && BLI_strcasecmp(from, to))
		if (BLI_delete(to, 0, 0)) return 1;
	
	return urename(from, to);
}

#else /* The UNIX world */

enum {
	/* operation succeeded succeeded */
	recursiveOp_Callback_OK = 0,

	/* operation requested not to perform recursive digging for current path */
	recursiveOp_Callback_StopRecurs = 1,

	/* error occured in callback and recursive walking should stop immediately */
	recursiveOp_Callback_Error = 2
} recuresiveOp_Callback_Result;

typedef int (*recursiveOp_Callback)(const char *from, const char *to);

/* appending of filename to dir (ensures for buffer size before appending) */
static void join_dirfile_alloc(char **dst, size_t *alloc_len, const char *dir, const char *file)
{
	size_t len = strlen(dir) + strlen(file) + 1;

	if (!*dst)
		*dst = MEM_callocN(len + 1, "join_dirfile_alloc path");
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

static int recursive_operation(const char *startfrom, const char *startto, recursiveOp_Callback callback_dir_pre,
                               recursiveOp_Callback callback_file, recursiveOp_Callback callback_dir_post)
{
	struct dirent **dirlist;
	struct stat st;
	char *from = NULL, *to = NULL;
	char *from_path = NULL, *to_path = NULL;
	size_t from_alloc_len = -1, to_alloc_len = -1;
	int i, n, ret = 0;

	/* ensure there's no trailing slash in file path */
	from = strip_last_slash(startfrom);
	if (startto)
		to = strip_last_slash(startto);

	ret = lstat(from, &st);
	if (ret < 0) {
		/* source wasn't found, nothing to operate with */
		return ret;
	}

	if (!S_ISDIR(st.st_mode)) {
		/* source isn't a directory, can't do recursive walking for it,
		 * so just call file callback and leave */
		if (callback_file) {
			ret = callback_file(from, to);

			if (ret != recursiveOp_Callback_OK)
				ret = -1;
		}

		MEM_freeN(from);
		if (to) MEM_freeN(to);

		return ret;
	}


	n = scandir(startfrom, &dirlist, 0, alphasort);
	if (n < 0) {
		/* error opening directory for listing */
		perror("scandir");

		MEM_freeN(from);
		if (to) MEM_freeN(to);

		return -1;
	}

	if (callback_dir_pre) {
		/* call pre-recursive walking directory callback */
		ret = callback_dir_pre(from, to);

		if (ret != recursiveOp_Callback_OK) {
			MEM_freeN(from);
			if (to) free(to);

			if (ret == recursiveOp_Callback_StopRecurs) {
				/* callback requested not to perform recursive walking, not an error */
				return 0;
			}

			return -1;
		}
	}

	for (i = 0; i < n; i++) {
		struct dirent *dirent = dirlist[i];

		if (!strcmp(dirent->d_name, ".") || !strcmp(dirent->d_name, "..")) {
			free(dirent);
			continue;
		}

		join_dirfile_alloc(&from_path, &from_alloc_len, from, dirent->d_name);

		if (to)
			join_dirfile_alloc(&to_path, &to_alloc_len, to, dirent->d_name);

		if (dirent->d_type == DT_DIR) {
			/* recursively dig into a folder */
			ret = recursive_operation(from_path, to_path, callback_dir_pre, callback_file, callback_dir_post);
		}
		else if (callback_file) {
			/* call file callback for current path */
			ret = callback_file(from_path, to_path);
			if (ret != recursiveOp_Callback_OK)
				ret = -1;
		}

		if (ret != 0) {
			while (i < n)
				free(dirlist[i]);
			break;
		}
	}

	free(dirlist);

	if (ret == 0) {
		if (callback_dir_post) {
			/* call post-recursive directory callback */
			ret = callback_dir_post(from, to);
			if (ret != recursiveOp_Callback_OK)
				ret = -1;
		}
	}

	if (from_path) MEM_freeN(from_path);
	if (to_path) MEM_freeN(to_path);

	MEM_freeN(from);
	if (to) MEM_freeN(to);

	return ret;
}

static int delete_callback_post(const char *from, const char *UNUSED(to))
{
	if (rmdir(from)) {
		perror("rmdir");

		return recursiveOp_Callback_Error;
	}

	return recursiveOp_Callback_OK;
}

static int delete_single_file(const char *from, const char *UNUSED(to))
{
	if (unlink(from)) {
		perror("unlink");

		return recursiveOp_Callback_Error;
	}

	return recursiveOp_Callback_OK;
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

int BLI_delete(const char *file, int dir, int recursive) 
{
	if (strchr(file, '"')) {
		printf("Error: not deleted file %s because of quote!\n", file);
	}
	else {
		if (recursive) {
			return recursive_operation(file, NULL, NULL, delete_single_file, delete_callback_post);
		}
		else if (dir) {
			return rmdir(file);
		}
		else {
			return remove(file); //BLI_snprintf(str, sizeof(str), "/bin/rm -f \"%s\"", file);
		}
	}
	return -1;
}

static int check_the_same(const char *path_a, const char *path_b)
{
	struct stat st_a, st_b;

	if (lstat(path_a, &st_a))
		return 0;

	if (lstat(path_b, &st_b))
		return 0;

	return st_a.st_dev == st_b.st_dev && st_a.st_ino == st_b.st_ino;
}

static int set_permissions(const char *file, struct stat *st)
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
		return recursiveOp_Callback_Error;
	}

	if (lstat(from, &st)) {
		perror("stat");
		return recursiveOp_Callback_Error;
	}

	/* create a directory */
	if (mkdir(to, st.st_mode)) {
		perror("mkdir");
		return recursiveOp_Callback_Error;
	}

	/* set proper owner and group on new directory */
	if (chown(to, st.st_uid, st.st_gid)) {
		perror("chown");
		return recursiveOp_Callback_Error;
	}

	return recursiveOp_Callback_OK;
}

static int copy_single_file(const char *from, const char *to)
{
	FILE *from_stream, *to_stream;
	struct stat st;
	char buf[4096];
	size_t len;

	if (check_the_same(from, to)) {
		fprintf(stderr, "%s: '%s' is the same as '%s'\n", __func__, from, to);
		return recursiveOp_Callback_Error;
	}

	if (lstat(from, &st)) {
		perror("lstat");
		return recursiveOp_Callback_Error;
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

			return recursiveOp_Callback_Error;
		}

		link_buffer[link_len] = 0;

		if (symlink(link_buffer, to)) {
			perror("symlink");
			if (need_free) MEM_freeN(link_buffer);
			return recursiveOp_Callback_Error;
		}

		if (need_free)
			MEM_freeN(link_buffer);

		return recursiveOp_Callback_OK;
	}
	else if (S_ISCHR(st.st_mode) ||
	         S_ISBLK(st.st_mode) ||
	         S_ISFIFO(st.st_mode) ||
	         S_ISSOCK(st.st_mode))
	{
		/* copy special type of file */
		if (mknod(to, st.st_mode, st.st_rdev)) {
			perror("mknod");
			return recursiveOp_Callback_Error;
		}

		if (set_permissions(to, &st))
			return recursiveOp_Callback_Error;

		return recursiveOp_Callback_OK;
	}
	else if (!S_ISREG(st.st_mode)) {
		fprintf(stderr, "Copying of this kind of files isn't supported yet\n");
		return recursiveOp_Callback_Error;
	}

	from_stream = fopen(from, "rb");
	if (!from_stream) {
		perror("fopen");
		return recursiveOp_Callback_Error;
	}

	to_stream = fopen(to, "wb");
	if (!to_stream) {
		perror("fopen");
		fclose(from_stream);
		return recursiveOp_Callback_Error;
	}

	while ((len = fread(buf, 1, sizeof(buf), from_stream)) > 0) {
		fwrite(buf, 1, len, to_stream);
	}

	fclose(to_stream);
	fclose(from_stream);

	if (set_permissions(to, &st))
		return recursiveOp_Callback_Error;

	return recursiveOp_Callback_OK;
}

static int move_callback_pre(const char *from, const char *to)
{
	int ret = rename(from, to);

	if (ret)
		return copy_callback_pre(from, to);

	return recursiveOp_Callback_StopRecurs;
}

static int move_single_file(const char *from, const char *to)
{
	int ret = rename(from, to);

	if (ret)
		return copy_single_file(from, to);

	return recursiveOp_Callback_OK;
}

int BLI_move(const char *file, const char *to)
{
	int ret = recursive_operation(file, to, move_callback_pre, move_single_file, NULL);

	if (ret) {
		return recursive_operation(file, NULL, NULL, delete_single_file, delete_callback_post);
	}

	return ret;
}

static char *check_destination(const char *file, const char *to)
{
	struct stat st;

	if (!stat(to, &st)) {
		if (S_ISDIR(st.st_mode)) {
			char *str, *filename, *path;
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

	return (char *)to;
}

int BLI_copy(const char *file, const char *to)
{
	char *actual_to = check_destination(file, to);
	int ret;

	ret = recursive_operation(file, actual_to, copy_callback_pre, copy_single_file, NULL);

	if (actual_to != to)
		MEM_freeN(actual_to);

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
	int needs_free;

	if (BLI_exists(dirname)) return;

#ifdef MAXPATHLEN
	size = MAXPATHLEN;
	tmp = static_buf;
	needs_free = 0;
#else
	size = strlen(dirname) + 1;
	tmp = MEM_callocN(size, "BLI_dir_create_recursive tmp");
	needs_free = 1;
#endif

	BLI_strncpy(tmp, dirname, size);
		
	lslash = BLI_last_slash(tmp);
	if (lslash) {
		/* Split about the last slash and recurse */
		*lslash = 0;
		BLI_dir_create_recursive(tmp);
	}

	if (needs_free)
		MEM_freeN(tmp);

	mkdir(dirname, 0777);
}

int BLI_rename(const char *from, const char *to)
{
	if (!BLI_exists(from)) return 0;
	
	if (BLI_exists(to))
		if (BLI_delete(to, 0, 0)) return 1;

	return rename(from, to);
}

#endif


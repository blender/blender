/*
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
 * The Original Code is Copyright (C) 2009 by Stichting Blender Foundation.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 * BFILE* based abstraction for file access.
 */

#include <string.h>
#include <stdlib.h>
#ifndef WIN32
 #include <libgen.h>
 #include <unistd.h>
 #include <sys/param.h>
#else
 #include <io.h>
 #include "BLI_winstuff.h"
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "MEM_guardedalloc.h"
#include "BKE_utildefines.h"
#include "BKE_blender.h"
#include "BLI_path_util.h"
#include "BLI_fileops.h"
#include "BLI_storage.h"
#include "BLI_bfile.h"

/* Internal bfile classification flags */
#define BCF_OPEN     (0)
#define BCF_FOPEN    (1<<0)
#define BCF_READ     (1<<1)
#define BCF_WRITE    (1<<2)
#define BCF_AT_END   (1<<3)
#define BCF_DISCARD  (1<<4)

/* Declaration of internal functions */
void chomp(char* line);
void expand_envvars(char* src, char* dst);
void fill_paths(BFILE *bfile, const char *path);
char* find_in_pathlist(char* filename, char* pathlist);
void init_vars_from_file(const char* path);
void setup_temp();

/*** Exported functions ***/

BFILE *BLI_bfile_fopen(const char *path, const char *mode, int bflags,
                       BEnvVarFam envvars)
{
	BFILE *bfile;

	bfile = MEM_mallocN(sizeof(BFILE), "bfile-fopen");
	bfile->classf = BCF_FOPEN;
	bfile->uflags = bflags;

	/* From fopen() doc, we can guess some logic:
	r  BCF_READ
	r+ BCF_READ | BCF_WRITE
	w  BCF_DISCARD | BCF_WRITE
	w+ BCF_DISCARD | BCF_WRITE | BCF_READ
	a  BCF_AT_END | BCF_WRITE
	a+ BCF_AT_END | BCF_WRITE | BCF_READ
	*/
	if(strchr(mode, 'r'))
		bfile->classf |= BCF_READ;
	if(strchr(mode, 'w'))
		bfile->classf |= (BCF_DISCARD | BCF_WRITE);
	if(strchr(mode, 'a'))
		bfile->classf |= (BCF_AT_END | BCF_WRITE);
	if(strchr(mode, '+'))
		bfile->classf |= (BCF_READ | BCF_WRITE);

	fill_paths(bfile, path);

	bfile->stream = fopen(bfile->tpath, mode);
	// detect failed fopen
	bfile->fd = fileno(bfile->stream);
	return bfile;
}


BFILE *BLI_bfile_open(const char *pathname, int flags, int bflags,
                      BEnvVarFam envvars)
{
	BFILE *bfile;

	bfile = MEM_mallocN(sizeof(BFILE), "bfile-open");
	bfile->classf = BCF_OPEN;
	bfile->uflags = bflags;

	/* Easy mapping for open() */
	if(flags & O_RDONLY)
		bfile->classf |= BCF_READ;
	if(flags & O_WRONLY)
		bfile->classf |= BCF_WRITE;
	if(flags & O_RDWR)
		bfile->classf |= (BCF_READ | BCF_WRITE);
	if(flags & O_APPEND)
		bfile->classf |= BCF_AT_END;
	if(flags & O_TRUNC)
		bfile->classf |= BCF_DISCARD;

	fill_paths(bfile, pathname);

	bfile->fd = open(bfile->tpath, flags);
	// detect failed open
//	bfile->stream = fdopen(bfile->fd, XXX); /* MSWindows _fdopen? */
	return bfile;
}


FILE *BLI_bfile_file_from_bfile(BFILE *bfile) {
	return bfile->stream;
}


int BLI_bfile_fd_from_bfile(BFILE *bfile) {
	return bfile->fd;
}


ssize_t BLI_bfile_write(BFILE *f, const void *buf, size_t count) {
	ssize_t ret;

	ret = write((f->fd), buf, count);
	if (ret == -1) {
		f->error = 1;
	}

	return ret;
}


ssize_t BLI_bfile_read(BFILE *f, void *buf, size_t count) {
	ssize_t ret;

	ret = read((f->fd), buf, count);
	if (ret == -1) {
		f->error = 1;
	}

	return ret;
}


size_t BLI_bfile_fwrite(const void *ptr, size_t size, size_t nmemb,
                        BFILE *f)
{
	size_t ret;

	ret = fwrite(ptr, size, nmemb, f->stream);
	if (ret < 0) {
		f->error = 1;
	}

	return ret;
}


size_t BLI_bfile_fread(void *ptr, size_t size, size_t nmemb, BFILE *f) {
	size_t ret;

	ret = fread(ptr, size, nmemb, f->stream);
	if ((ret < 0) && ferror(f->stream)) {
		f->error = 1;
	}

	return ret;
}


void BLI_bfile_close(BFILE *bfile) {
	if((bfile->classf | BCF_WRITE) &&
	   !(bfile->uflags | BFILE_RAW)) {
		/* Make sure data is on disk */
		/* Move to final name if no errors */
	}

	/* Normal close */

	/* Cleanup */
	if(bfile->fpath) {
		MEM_freeN(bfile->fpath);
	}
	if(bfile->tpath) {
		MEM_freeN(bfile->tpath);
	}
}


void BLI_bfile_clear_error(BFILE *bfile) {
	bfile->error = 0;
}


void BLI_bfile_set_error(BFILE *bfile, int error) {
	/* No cheating, use clear_error() for 0 */
	if (error) {
		bfile->error = error;
	}
}


#if defined(WIN32)
 #define LAST_SESSION_FILE "%HOME%\\Blender\\last-session FIXME FIXME FIXME"
 #define ENVIRONMENT_FILE "FIXME"
 #define SHARED_DIRECTORY "FIXME TOO"
#elif defined(OSX)
 #define LAST_SESSION_FILE "${HOME}/Library/Application Support/Blender/last-session"
 #define ENVIRONMENT_FILE "${HOME}/Library/Application Support/Blender/${BLENDER_VERSION}/environment"
 #define SHARED_DIRECTORY "/Library/Application Support/Blender"
#else
 #define LAST_SESSION_FILE "${HOME}/.blender/last-session"
 #define ENVIRONMENT_FILE "${HOME}/.blender/${BLENDER_VERSION}/environment"
 #define SHARED_DIRECTORY "/usr/share/blender"
#endif
void BLI_bfile_init_vars() {
	char file[MAXPATHLEN];
	char temp[MAXPATHLEN];
	extern char bprogname[];
	FILE* fp;

	/* This one is unconditional */
	sprintf(temp, "%d", BLENDER_VERSION);
	BLI_setenv("BLENDER_VERSION", temp);

	/* Is this unpack&run? */
	sprintf(temp, "%s/%d/environment", dirname(bprogname), BLENDER_VERSION);
	if(BLI_exist(temp)) {
		BLI_setenv_if_new("BLENDER_SHARE", dirname(bprogname));
	} else {
		BLI_setenv_if_new("BLENDER_SHARE", SHARED_DIRECTORY);
	}

	expand_envvars(LAST_SESSION_FILE, file);
	fp = fopen(file, "r");
	/* 1st line, read previous version */
	if (fp && (fscanf(fp, "%3c\n", temp) == 1)) {
		temp[3] = '\0';
		BLI_setenv("BLENDER_VERSION_PREV", temp);
		/* 2nd line, read previous session path if needed */
		if(!getenv("BLENDER_TEMP")) {
			if ((fgets(temp, MAXPATHLEN, fp) != NULL)) {
				/* Clean any \n */
				chomp(temp);
				/* Check the dir is still there or generate new one */
				if(!BLI_exist(temp)) {
					setup_temp();
				}
			} else {
				/* We have to generate it for sure */
				setup_temp();
			}
		}
	} else {
		/* Probably new user, or only <=249 before */
		BLI_setenv("BLENDER_VERSION_PREV", "0");
		setup_temp();
	}

	if(fp) {
		fclose(fp);
	}

	/* Load vars from user and system files */
	expand_envvars(ENVIRONMENT_FILE, file);
	init_vars_from_file(file);
	sprintf(temp, "/%d/environment", BLENDER_VERSION);
	BLI_make_file_string("/", file, getenv("BLENDER_SHARE"), temp);
	init_vars_from_file(file);
}


/*** Internal functions ***/

/**
 Eliminate trailing EOL by writing a \0 over it.
 Name taken from Perl.
 */
void chomp(char* line) {
	int len = strlen(line);
#ifndef WIN32
	if (line[len - 1] == '\n') {
		line[len - 1] = '\0';
	}
#else
	if ((line[len - 2] == '\r' ) && ((line[len - 1] == '\n'))) {
		line[len - 2] = '\0';
	}
#endif /* WIN32 */
}


/**
 Parse a file with lines like FOO=bar (comment lines have # as first
 character) assigning to envvar FOO the value bar if FOO does not
 exist yet.
 Any white space before FOO, around the = or trailing will be used,
 so beware.
 */
#define MAX_LINE 4096
#define ENV_VAR 256
#define VAR_LEN 8192
void init_vars_from_file(const char* path) {
	char line[MAX_LINE];
	char name[ENV_VAR];
	FILE *fp;
	char* separator;
	char expanded[VAR_LEN];

	fp = fopen(path, "r");
	if (!fp) return;

	while (fgets(line, MAX_LINE, fp) != NULL) {
		/* Ignore comment lines */
		if (line[0] == '#')
			continue;

		/* Split into envvar name and contents */
		separator = strchr(line, '=');
		if(separator && ((separator - line) < ENV_VAR)) {
			/* First remove EOL */
			chomp(line);
			strncpy(name, line, separator - line);
			name[separator - line] = '\0';
			expand_envvars(separator + 1, expanded);
			BLI_setenv_if_new(name, expanded);
		}
	}
	fclose(fp);
}


/**
 Look for ${} (or %%) env vars in src and expand if the var
 exists (even if empty value). If not exist, the name is left as is.
 The process is done all over src, and nested ${${}} is not supported.
 src must be \0 terminated, and dst must be big enough.
*/
#ifndef WIN32
 #define ENVVAR_PREFFIX "${"
 #define ENVVAR_P_SIZE 2
 #define ENVVAR_SUFFIX "}"
 #define ENVVAR_S_SIZE 1
#else
 #define ENVVAR_PREFFIX "%"
 #define ENVVAR_P_SIZE 1
 #define ENVVAR_SUFFIX "%"
 #define ENVVAR_S_SIZE 1
#endif /* WIN32 */
void expand_envvars(char* src, char* dst) {
	char* hit1;
	char* hit2;
	char name[ENV_VAR];
	char* value;
	int prevlen;
	int done = 0;
	char* source = src;

	dst[0] = '\0';
	while (!done) {
		hit1 = strstr(source, ENVVAR_PREFFIX);
		if (hit1) {
			hit2 = strstr(hit1 + ENVVAR_P_SIZE, ENVVAR_SUFFIX);
			if (hit2) {
				/* "Copy" the leading part, if any */
				if (hit1 != source) {
					prevlen = strlen(dst);
					strncat(dst, source, hit1 - source);
					dst[prevlen + (hit1 - source)] = '\0';
				}
				/* Figure the name of the env var we just found  */
				strncpy(name, hit1 + ENVVAR_P_SIZE,
				        hit2 - (hit1 + ENVVAR_P_SIZE));
				name[hit2 - (hit1 + ENVVAR_P_SIZE)] = '\0';
				/* See if we can get something with that name */
				value = getenv(name);
				if (value) {
					/* Push the var value */
					strcat(dst, value);
				} else {
					/* Leave the var name, so it is clear that it failed */
					strcat(dst, ENVVAR_PREFFIX);
					strcat(dst, name);
					strcat(dst, ENVVAR_SUFFIX);
				}
				/* Continue after closing mark, like a new string */
				source = hit2 + ENVVAR_S_SIZE;
			} else {
				/* Non terminated var so "copy as is" and finish */
				strcat(dst, source);
				done = 1;
			}
		} else {
			/* "Copy" whatever is left */
			strcat(dst, source);
			done = 1;
		}
	}
}


/**
 Return a full path if the filename exists when combined
 with any item from pathlist. Or NULL otherwise.
 */
#ifdef WIN32
 #define SEPARATOR ';'
#else
 #define SEPARATOR ':'
#endif
char* find_in_pathlist(char* filename, char* pathlist) {
	char first[FILE_MAX + 10];
	char* rest = NULL;

	/* Separate first path from rest, use typical separator for current OS */
	rest = strchr(pathlist, SEPARATOR);
	if (rest) {
		strncpy(first, pathlist, rest - pathlist);
		first[rest - pathlist] = '\0';
		/* Skip the separator so it becomes a valid new pathlist */
		rest++;
	} else {
		strcpy(first, pathlist);
	}

	/* Check if combination exists */
	BLI_add_slash(first);
	strcat(first, filename);
	if(BLI_exist(first)) {
		return strdup(first);
	}

	/* First path failed, try with rest of paths if possible */
	if(rest) {
		return find_in_pathlist(filename, rest);
	} else {
		return NULL;
	}
}


/**
 Setup fpath and tpath based in the needs of the bfile.
 */
void fill_paths(BFILE *bfile, const char *path) {
	char* source_path = NULL;
	char* temp_path = NULL;
	int bflags = bfile->uflags;

	if(bflags & BFILE_NORMAL || bflags & BFILE_RAW) {
//		bfile->fpath is path with // replaced
	}
	if(bflags & BFILE_TEMP) {
		temp_path = MEM_mallocN(MAXPATHLEN, "bfile-fpath-1");
		snprintf(temp_path, MAXPATHLEN, "%s/%s", getenv("BLENDER_TEMP"), path);
		bfile->fpath = temp_path;
	}
	if(bflags & BFILE_CONFIG) {
//		bfile->fpath is userdir+version+path
//		source_path is first hit in (if using fallback to older versions)
//		    userdir+curversion+path (... userdir+limitversion+path) sysdir+path
//		(limitversion is based in path, using some kind of regex or "tables")
	}

	if(bfile->classf & BCF_WRITE && !(bflags & BFILE_RAW)) {
		/* Generate temp path */
		temp_path = MEM_mallocN(MAXPATHLEN, "bfile-fpath-2");
		snprintf(temp_path, MAXPATHLEN, "%s.XXXXXX", path);
		bfile->tpath = mkdtemp(temp_path);
		if(!(bfile->classf & BCF_DISCARD)) {
			/* Copy data to tpath */
			if(source_path) {
				// copy it from older version or sys version
			}
		}
	} else {
		bfile->tpath = bfile->fpath;
	}
}


/**
 Create a temp directory in safe and multiuser way.
 */
void setup_temp() {
	char template[MAXPATHLEN];
	char* tempdir;

	if(getenv("TMPDIR")) {
		sprintf(template, "%s/blender-XXXXXX", getenv("TMPDIR"));
	} else {
		sprintf(template, "/tmp/blender-XXXXXX");
// MacOSX NSTemporaryDirectory and WIN32 ???
	}
	tempdir = mkdtemp(template);
	BLI_setenv("BLENDER_TEMP", tempdir);
}


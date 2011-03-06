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
#include "BLI_winstuff.h"
#include "BLI_callbacks.h"
#else
#include <unistd.h> // for read close
#include <sys/param.h>
#endif

#include "BLI_blenlib.h"

#include "BKE_utildefines.h"

#include "BLO_sys_types.h" // for intptr_t support


/* gzip the file in from and write it to "to". 
 return -1 if zlib fails, -2 if the originating file does not exist
 note: will remove the "from" file
  */
int BLI_gzip(const char *from, const char *to) {
	char buffer[10240];
	int file;
	int readsize = 0;
	int rval= 0, err;
	gzFile gzfile;
	
	gzfile = gzopen(to, "wb"); 
	if(gzfile == NULL)
		return -1;
	
	file = open(from, O_BINARY|O_RDONLY);
	if(file < 0)
		return -2;

	while(1) {
		readsize = read(file, buffer, 10240);

		if(readsize < 0) {
			rval= -2; /* error happened in reading */
			fprintf(stderr, "Error reading file %s: %s.\n", from, strerror(errno));
			break;
		}
		else if(readsize == 0)
			break; /* done reading */
		
		if(gzwrite(gzfile, buffer, readsize) <= 0) {
			rval= -1; /* error happened in writing */
			fprintf(stderr, "Error writing gz file %s: %s.\n", to, gzerror(gzfile, &err));
			break;
		}
	}
	
	gzclose(gzfile);
	close(file);

	return rval;
}

/* return 1 when file can be written */
int BLI_is_writable(const char *filename)
{
	int file;
	
	/* first try to open without creating */
	file = open(filename, O_BINARY | O_RDWR, 0666);
	
	if (file < 0) {
		/* now try to open and create. a test without actually
		 * creating a file would be nice, but how? */
		file = open(filename, O_BINARY | O_RDWR | O_CREAT, 0666);
		
		if(file < 0) {
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

int BLI_touch(const char *file)
{
   FILE *f = fopen(file,"r+b");
   if (f != NULL) {
		char c = getc(f);
		rewind(f);
		putc(c,f);
	} else {
	   f = fopen(file,"wb");
	}
	if (f) {
		fclose(f);
		return 1;
	}
	return 0;
}

int BLI_exists(const char *file) {
	return BLI_exist(file);
}

#ifdef WIN32

static char str[MAXPATHLEN+12];

int BLI_delete(const char *file, int dir, int recursive) {
	int err;

	if (recursive) {
		callLocalErrorCallBack("Recursive delete is unsupported on Windows");
		err= 1;
	} else if (dir) {
		err= !RemoveDirectory(file);
		if (err) printf ("Unable to remove directory");
	} else {
		err= !DeleteFile(file);
		if (err) callLocalErrorCallBack("Unable to delete file");
	}

	return err;
}

int BLI_move(const char *file, const char *to) {
	int err;

	// windows doesn't support moveing to a directory
	// it has to be 'mv filename filename' and not
	// 'mv filename destdir'

	strcpy(str, to);
	// points 'to' to a directory ?
	if (BLI_last_slash(str) == (str + strlen(str) - 1)) {
		if (BLI_last_slash(file) != NULL) {
			strcat(str, BLI_last_slash(file) + 1);
		}
	}

	err= !MoveFile(file, str);
	if (err) {
		callLocalErrorCallBack("Unable to move file");
		printf(" Move from '%s' to '%s' failed\n", file, str);
	}

	return err;
}


int BLI_copy_fileops(const char *file, const char *to) {
	int err;

	// windows doesn't support copying to a directory
	// it has to be 'cp filename filename' and not
	// 'cp filename destdir'

	strcpy(str, to);
	// points 'to' to a directory ?
	if (BLI_last_slash(str) == (str + strlen(str) - 1)) {
		if (BLI_last_slash(file) != NULL) {
			strcat(str, BLI_last_slash(file) + 1);
		}
	}

	err= !CopyFile(file,str,FALSE);
	
	if (err) {
		callLocalErrorCallBack("Unable to copy file!");
		printf(" Copy from '%s' to '%s' failed\n", file, str);
	}

	return err;
}

int BLI_link(const char *file, const char *to) {
	callLocalErrorCallBack("Linking files is unsupported on Windows");
	
	return 1;
}

void BLI_recurdir_fileops(const char *dirname) {
	char *lslash;
	char tmp[MAXPATHLEN];
	
	// First remove possible slash at the end of the dirname.
	// This routine otherwise tries to create
	// blah1/blah2/ (with slash) after creating
	// blah1/blah2 (without slash)

	strcpy(tmp, dirname);
	lslash= BLI_last_slash(tmp);

	if (lslash == tmp + strlen(tmp) - 1) {
		*lslash = 0;
	}
	
	if (BLI_exists(tmp)) return;
		
	lslash= BLI_last_slash(tmp);
	if (lslash) {
			/* Split about the last slash and recurse */	
		*lslash = 0;
		BLI_recurdir_fileops(tmp);
	}
	
	if(dirname[0]) /* patch, this recursive loop tries to create a nameless directory */
		if (!CreateDirectory(dirname, NULL))
			callLocalErrorCallBack("Unable to create directory\n");
}

int BLI_rename(const char *from, const char *to) {
	if (!BLI_exists(from)) return 0;

	/* make sure the filenames are different (case insensitive) before removing */
	if (BLI_exists(to) && BLI_strcasecmp(from, to))
		if(BLI_delete(to, 0, 0)) return 1;

	return rename(from, to);
}

#else /* The UNIX world */

/*
 * but the UNIX world is tied to the interface, and the system
 * timer, and... We implement a callback mechanism. The system will
 * have to initialise the callback before the functions will work!
 * */
static char str[12 + (MAXPATHLEN * 2)];

int BLI_delete(const char *file, int dir, int recursive) 
{
	if(strchr(file, '"')) {
		printf("Error: not deleted file %s because of quote!\n", file);
	}
	else {
		if (recursive) {
			BLI_snprintf(str, sizeof(str), "/bin/rm -rf \"%s\"", file);
			return system(str);
		}
		else if (dir) {
			BLI_snprintf(str, sizeof(str), "/bin/rmdir \"%s\"", file);
			return system(str);
		}
		else {
			return remove(file); //BLI_snprintf(str, sizeof(str), "/bin/rm -f \"%s\"", file);
		}
	}
	return -1;
}

int BLI_move(const char *file, const char *to) {
	BLI_snprintf(str, sizeof(str), "/bin/mv -f \"%s\" \"%s\"", file, to);

	return system(str);
}

int BLI_copy_fileops(const char *file, const char *to) {
	BLI_snprintf(str, sizeof(str), "/bin/cp -rf \"%s\" \"%s\"", file, to);

	return system(str);
}

int BLI_link(const char *file, const char *to) {
	BLI_snprintf(str, sizeof(str), "/bin/ln -f \"%s\" \"%s\"", file, to);
	
	return system(str);
}

void BLI_recurdir_fileops(const char *dirname) {
	char *lslash;
	char tmp[MAXPATHLEN];
		
	if (BLI_exists(dirname)) return;

	strcpy(tmp, dirname);
		
	lslash= BLI_last_slash(tmp);
	if (lslash) {
			/* Split about the last slash and recurse */	
		*lslash = 0;
		BLI_recurdir_fileops(tmp);
	}

	mkdir(dirname, 0777);
}

int BLI_rename(const char *from, const char *to) {
	if (!BLI_exists(from)) return 0;
	
	if (BLI_exists(to))	if(BLI_delete(to, 0, 0)) return 1;

	return rename(from, to);
}

#endif

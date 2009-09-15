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
 * blenloader readfile private function prototypes
 */
#ifndef READFILE_H
#define READFILE_H

#include "zlib.h"

struct OldNewMap;
struct MemFile;
struct bheadsort;
struct ReportList;

typedef struct FileData {
	// linked list of BHeadN's
	ListBase listbase;
	int flags;
	int eof;
	int buffersize;
	int seek;
	int (*read)(struct FileData *filedata, void *buffer, unsigned int size);

	// variables needed for reading from memory / stream
	char *buffer;
	// variables needed for reading from memfile (undo)
	struct MemFile *memfile;

	// variables needed for reading from file
	int filedes;
	gzFile gzfiledes;

	// now only in use for library appending
	char filename[FILE_MAXDIR+FILE_MAXFILE];
	
	// variables needed for reading from stream
	char headerdone;
	int inbuffer;
	
	// general reading variables
	struct SDNA *filesdna;
	struct SDNA *memsdna;
	char *compflags;
	
	int fileversion;
	int id_name_offs;		/* used to retrieve ID names from (bhead+1) */
	int globalf, fileflags;	/* for do_versions patching */
	
	struct OldNewMap *datamap;
	struct OldNewMap *globmap;
	struct OldNewMap *libmap;
	struct OldNewMap *imamap;
	
	struct bheadsort *bheadmap;
	int tot_bheadmap;
	
	ListBase mainlist;
	
		/* ick ick, used to return
		 * data through streamglue.
		 */
	BlendFileData **bfd_r;
	struct ReportList *reports;
} FileData;

typedef struct BHeadN {
	struct BHeadN *next, *prev;
	struct BHead bhead;
} BHeadN;


#define FD_FLAGS_SWITCH_ENDIAN             (1<<0)
#define FD_FLAGS_FILE_POINTSIZE_IS_4       (1<<1)
#define FD_FLAGS_POINTSIZE_DIFFERS         (1<<2)
#define FD_FLAGS_FILE_OK                   (1<<3)
#define FD_FLAGS_NOT_MY_BUFFER			   (1<<4)
#define FD_FLAGS_NOT_MY_LIBMAP			   (1<<5)

#define SIZEOFBLENDERHEADER 12

	/***/
struct Main;
void blo_join_main(ListBase *mainlist);
void blo_split_main(ListBase *mainlist, struct Main *main);

BlendFileData *blo_read_file_internal(FileData *fd, char *file);

FileData *blo_openblenderfile(char *name, struct ReportList *reports);
FileData *blo_openblendermemory(void *buffer, int buffersize, struct ReportList *reports);
FileData *blo_openblendermemfile(struct MemFile *memfile, struct ReportList *reports);

void blo_clear_proxy_pointers_from_lib(FileData *fd, Main *oldmain);
void blo_make_image_pointer_map(FileData *fd, Main *oldmain);
void blo_end_image_pointer_map(FileData *fd, Main *oldmain);
void blo_add_library_pointer_map(ListBase *mainlist, FileData *fd);

void blo_freefiledata( FileData *fd);

BHead *blo_firstbhead(FileData *fd);
BHead *blo_nextbhead(FileData *fd, BHead *thisblock);
BHead *blo_prevbhead(FileData *fd, BHead *thisblock);

char *bhead_id_name(FileData *fd, BHead *bhead);

#endif


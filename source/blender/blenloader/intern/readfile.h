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
 * blenloader readfile private function prototypes
 */

/** \file blender/blenloader/intern/readfile.h
 *  \ingroup blenloader
 */

#ifndef __READFILE_H__
#define __READFILE_H__

#include "zlib.h"
#include "DNA_sdna_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"  /* for ReportType */

struct OldNewMap;
struct MemFile;
struct ReportList;
struct Object;
struct PartEff;
struct View3D;
struct Key;

typedef struct FileData {
	// linked list of BHeadN's
	ListBase listbase;
	int flags;
	int eof;
	int buffersize;
	int seek;
	int (*read)(struct FileData *filedata, void *buffer, unsigned int size);

	// variables needed for reading from memory / stream
	const char *buffer;
	// variables needed for reading from memfile (undo)
	struct MemFile *memfile;

	// variables needed for reading from file
	int filedes;
	gzFile gzfiledes;

	// now only in use for library appending
	char relabase[FILE_MAX];

	// variables needed for reading from stream
	char headerdone;
	int inbuffer;

	// gzip stream for memory decompression
	z_stream strm;

	// general reading variables
	struct SDNA *filesdna;
	const struct SDNA *memsdna;
	const char *compflags;  /* array of eSDNA_StructCompare */

	int fileversion;
	int id_name_offs;       /* used to retrieve ID names from (bhead+1) */
	int globalf, fileflags; /* for do_versions patching */

	eBLOReadSkip skip_flags;  /* skip some data-blocks */

	struct OldNewMap *datamap;
	struct OldNewMap *globmap;
	struct OldNewMap *libmap;
	struct OldNewMap *imamap;
	struct OldNewMap *movieclipmap;
	struct OldNewMap *scenemap;
	struct OldNewMap *soundmap;
	struct OldNewMap *packedmap;

	struct BHeadSort *bheadmap;
	int tot_bheadmap;

	/* see: USE_GHASH_BHEAD */
	struct GHash *bhead_idname_hash;

	ListBase *mainlist;
	ListBase *old_mainlist;  /* Used for undo. */

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

/* FileData->flags */
enum {
	FD_FLAGS_SWITCH_ENDIAN         = 1 << 0,
	FD_FLAGS_FILE_POINTSIZE_IS_4   = 1 << 1,
	FD_FLAGS_POINTSIZE_DIFFERS     = 1 << 2,
	FD_FLAGS_FILE_OK               = 1 << 3,
	FD_FLAGS_NOT_MY_BUFFER         = 1 << 4,
	FD_FLAGS_NOT_MY_LIBMAP         = 1 << 5,  /* XXX Unused in practice (checked once but never set). */
};

#define SIZEOFBLENDERHEADER 12

/***/
struct Main;
void blo_join_main(ListBase *mainlist);
void blo_split_main(ListBase *mainlist, struct Main *main);

BlendFileData *blo_read_file_internal(FileData *fd, const char *filepath);

FileData *blo_openblenderfile(const char *filepath, struct ReportList *reports);
FileData *blo_openblendermemory(const void *buffer, int buffersize, struct ReportList *reports);
FileData *blo_openblendermemfile(struct MemFile *memfile, struct ReportList *reports);

void blo_clear_proxy_pointers_from_lib(Main *oldmain);
void blo_make_image_pointer_map(FileData *fd, Main *oldmain);
void blo_end_image_pointer_map(FileData *fd, Main *oldmain);
void blo_make_scene_pointer_map(FileData *fd, Main *oldmain);
void blo_end_scene_pointer_map(FileData *fd, Main *oldmain);
void blo_make_movieclip_pointer_map(FileData *fd, Main *oldmain);
void blo_end_movieclip_pointer_map(FileData *fd, Main *oldmain);
void blo_make_sound_pointer_map(FileData *fd, Main *oldmain);
void blo_end_sound_pointer_map(FileData *fd, Main *oldmain);
void blo_make_packed_pointer_map(FileData *fd, Main *oldmain);
void blo_end_packed_pointer_map(FileData *fd, Main *oldmain);
void blo_add_library_pointer_map(ListBase *old_mainlist, FileData *fd);

void blo_freefiledata(FileData *fd);

BHead *blo_firstbhead(FileData *fd);
BHead *blo_nextbhead(FileData *fd, BHead *thisblock);
BHead *blo_prevbhead(FileData *fd, BHead *thisblock);

const char *bhead_id_name(const FileData *fd, const BHead *bhead);

/* do versions stuff */

void blo_reportf_wrap(struct ReportList *reports, ReportType type, const char *format, ...) ATTR_PRINTF_FORMAT(3, 4);

void blo_do_versions_oldnewmap_insert(struct OldNewMap *onm, const void *oldaddr, void *newaddr, int nr);
void *blo_do_versions_newlibadr(struct FileData *fd, const void *lib, const void *adr);
void *blo_do_versions_newlibadr_us(struct FileData *fd, const void *lib, const void *adr);

struct PartEff *blo_do_version_give_parteff_245(struct Object *ob);
void blo_do_version_old_trackto_to_constraints(struct Object *ob);
void blo_do_versions_view3d_split_250(struct View3D *v3d, struct ListBase *regions);
void blo_do_versions_key_uidgen(struct Key *key);

void blo_do_versions_pre250(struct FileData *fd, struct Library *lib, struct Main *bmain);
void blo_do_versions_250(struct FileData *fd, struct Library *lib, struct Main *bmain);
void blo_do_versions_260(struct FileData *fd, struct Library *lib, struct Main *bmain);
void blo_do_versions_270(struct FileData *fd, struct Library *lib, struct Main *bmain);
void blo_do_versions_280(struct FileData *fd, struct Library *lib, struct Main *bmain);

void do_versions_after_linking_270(struct Main *bmain);
void do_versions_after_linking_280(struct Main *bmain);

#endif

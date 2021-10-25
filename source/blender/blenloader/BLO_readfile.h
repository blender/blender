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
#ifndef __BLO_READFILE_H__
#define __BLO_READFILE_H__

/** \file BLO_readfile.h
 *  \ingroup blenloader
 *  \brief external readfile function prototypes.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct BlendThumbnail;
struct bScreen;
struct LinkNode;
struct Main;
struct MemFile;
struct ReportList;
struct Scene;
struct UserDef;
struct View3D;
struct bContext;
struct BHead;
struct FileData;

typedef struct BlendHandle BlendHandle;

typedef enum BlenFileType {
	BLENFILETYPE_BLEND = 1,
	BLENFILETYPE_PUB = 2,
	BLENFILETYPE_RUNTIME = 3
} BlenFileType;

typedef struct BlendFileData {
	struct Main *main;
	struct UserDef *user;

	int fileflags;
	int globalf;
	char filename[1024];    /* 1024 = FILE_MAX */
	
	struct bScreen *curscreen;
	struct Scene *curscene;
	
	BlenFileType type;
} BlendFileData;


/* skip reading some data-block types (may want to skip screen data too). */
typedef enum eBLOReadSkip {
	BLO_READ_SKIP_NONE          = 0,
	BLO_READ_SKIP_USERDEF       = (1 << 0),
	BLO_READ_SKIP_DATA          = (1 << 1),
} eBLOReadSkip;
#define BLO_READ_SKIP_ALL \
	(BLO_READ_SKIP_USERDEF | BLO_READ_SKIP_DATA)

BlendFileData *BLO_read_from_file(
        const char *filepath,
        struct ReportList *reports, eBLOReadSkip skip_flag);
BlendFileData *BLO_read_from_memory(
        const void *mem, int memsize,
        struct ReportList *reports, eBLOReadSkip skip_flag);
BlendFileData *BLO_read_from_memfile(
        struct Main *oldmain, const char *filename, struct MemFile *memfile,
        struct ReportList *reports, eBLOReadSkip skip_flag);

void BLO_blendfiledata_free(BlendFileData *bfd);

BlendHandle *BLO_blendhandle_from_file(const char *filepath, struct ReportList *reports);
BlendHandle *BLO_blendhandle_from_memory(const void *mem, int memsize);

struct LinkNode *BLO_blendhandle_get_datablock_names(BlendHandle *bh, int ofblocktype, int *tot_names);
struct LinkNode *BLO_blendhandle_get_previews(BlendHandle *bh, int ofblocktype, int *tot_prev);
struct LinkNode *BLO_blendhandle_get_linkable_groups(BlendHandle *bh);

void BLO_blendhandle_close(BlendHandle *bh);

/***/

#define BLO_GROUP_MAX 32

bool BLO_has_bfile_extension(const char *str);
bool BLO_library_path_explode(const char *path, char *r_dir, char **r_group, char **r_name);

struct Main *BLO_library_link_begin(struct Main *mainvar, BlendHandle **bh, const char *filepath);
struct ID *BLO_library_link_named_part(struct Main *mainl, BlendHandle **bh, const short idcode, const char *name);
struct ID *BLO_library_link_named_part_ex(
        struct Main *mainl, BlendHandle **bh,
        const short idcode, const char *name, const short flag,
        struct Scene *scene, struct View3D *v3d,
        const bool use_placeholders, const bool force_indirect);
void BLO_library_link_end(struct Main *mainl, BlendHandle **bh, short flag, struct Scene *scene, struct View3D *v3d);

void BLO_library_link_copypaste(struct Main *mainl, BlendHandle *bh);

void *BLO_library_read_struct(struct FileData *fd, struct BHead *bh, const char *blockname);

BlendFileData *blo_read_blendafterruntime(int file, const char *name, int actualsize, struct ReportList *reports);

/* internal function but we need to expose it */
void blo_lib_link_screen_restore(struct Main *newmain, struct bScreen *curscreen, struct Scene *curscene);

typedef void (*BLOExpandDoitCallback) (void *fdhandle, struct Main *mainvar, void *idv);

void BLO_main_expander(BLOExpandDoitCallback expand_doit_func);
void BLO_expand_main(void *fdhandle, struct Main *mainvar);

/* Update defaults in startup.blend & userprefs.blend, without having to save and embed it */
void BLO_update_defaults_userpref_blend(void);
void BLO_update_defaults_startup_blend(struct Main *mainvar);

struct BlendThumbnail *BLO_thumbnail_from_file(const char *filepath);

#ifdef __cplusplus
} 
#endif

#endif  /* __BLO_READFILE_H__ */

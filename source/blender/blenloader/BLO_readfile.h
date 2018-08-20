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
struct ListBase;
struct Main;
struct MemFile;
struct ReportList;
struct Scene;
struct ViewLayer;
struct UserDef;
struct View3D;
struct bContext;
struct BHead;
struct FileData;
struct wmWindowManager;

typedef struct BlendHandle BlendHandle;

typedef enum eBlenFileType {
	BLENFILETYPE_BLEND = 1,
	BLENFILETYPE_PUB = 2,
	BLENFILETYPE_RUNTIME = 3
} eBlenFileType;

typedef struct BlendFileData {
	struct Main *main;
	struct UserDef *user;

	int fileflags;
	int globalf;
	char filename[1024];    /* 1024 = FILE_MAX */

	struct bScreen *curscreen; /* TODO think this isn't needed anymore? */
	struct Scene *curscene;
	struct ViewLayer *cur_view_layer; /* layer to activate in workspaces when reading without UI */

	eBlenFileType type;
} BlendFileData;

typedef struct WorkspaceConfigFileData {
	struct Main *main; /* has to be freed when done reading file data */

	struct ListBase workspaces;
} WorkspaceConfigFileData;


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
#define BLO_EMBEDDED_STARTUP_BLEND "<startup.blend>"

bool BLO_has_bfile_extension(const char *str);
bool BLO_library_path_explode(const char *path, char *r_dir, char **r_group, char **r_name);

/* Options controlling behavior of append/link code.
 * Note: merged with 'user-level' options from operators etc. in 16 lower bits
 *       (see eFileSel_Params_Flag in DNA_space_types.h). */
typedef enum BLO_LibLinkFlags {
	/* Generate a placeholder (empty ID) if not found in current lib file. */
	BLO_LIBLINK_USE_PLACEHOLDERS = 1 << 16,
	/* Force loaded ID to be tagged as LIB_TAG_INDIRECT (used in reload context only). */
	BLO_LIBLINK_FORCE_INDIRECT   = 1 << 17,
} BLO_LinkFlags;

struct Main *BLO_library_link_begin(struct Main *mainvar, BlendHandle **bh, const char *filepath);
struct ID *BLO_library_link_named_part(struct Main *mainl, BlendHandle **bh, const short idcode, const char *name);
struct ID *BLO_library_link_named_part_ex(
        struct Main *mainl, BlendHandle **bh,
        const short idcode, const char *name, const int flag,
        struct Main *bmain, struct Scene *scene, struct ViewLayer *view_layer);
void BLO_library_link_end(
        struct Main *mainl, BlendHandle **bh, int flag,
        struct Main *bmain, struct Scene *scene, struct ViewLayer *view_layer);

void BLO_library_link_copypaste(struct Main *mainl, BlendHandle *bh);

void *BLO_library_read_struct(struct FileData *fd, struct BHead *bh, const char *blockname);

/* internal function but we need to expose it */
void blo_lib_link_restore(
        struct Main *newmain, struct wmWindowManager *curwm,
        struct Scene *curscene, struct ViewLayer *cur_render_layer);

typedef void (*BLOExpandDoitCallback) (void *fdhandle, struct Main *mainvar, void *idv);

void BLO_main_expander(BLOExpandDoitCallback expand_doit_func);
void BLO_expand_main(void *fdhandle, struct Main *mainvar);

/* Update defaults in startup.blend & userprefs.blend, without having to save and embed it */
void BLO_update_defaults_userpref_blend(void);
void BLO_update_defaults_startup_blend(struct Main *mainvar);

struct BlendThumbnail *BLO_thumbnail_from_file(const char *filepath);

struct Main *BLO_main_from_memfile(struct MemFile *memfile, struct Main *bmain, struct Scene **r_scene);

/* datafiles (generated theme) */
extern const struct bTheme U_theme_default;

#ifdef __cplusplus
}
#endif

#endif  /* __BLO_READFILE_H__ */

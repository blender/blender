/*
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
 */
#pragma once

#include "BLI_listbase.h"
#include "BLI_sys_types.h"

/** \file
 * \ingroup blenloader
 * \brief external readfile function prototypes.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct BHead;
struct BlendThumbnail;
struct FileData;
struct LinkNode;
struct ListBase;
struct Main;
struct MemFile;
struct ReportList;
struct Scene;
struct UserDef;
struct View3D;
struct ViewLayer;
struct WorkSpace;
struct bScreen;
struct wmWindowManager;

typedef struct BlendHandle BlendHandle;

typedef struct WorkspaceConfigFileData {
  struct Main *main; /* has to be freed when done reading file data */

  struct ListBase workspaces;
} WorkspaceConfigFileData;

/* -------------------------------------------------------------------- */
/** \name BLO Read File API
 *
 * \see #BLO_write_file for file writing.
 * \{ */

typedef enum eBlenFileType {
  BLENFILETYPE_BLEND = 1,
  /* BLENFILETYPE_PUB = 2, */     /* UNUSED */
  /* BLENFILETYPE_RUNTIME = 3, */ /* UNUSED */
} eBlenFileType;

typedef struct BlendFileData {
  struct Main *main;
  struct UserDef *user;

  int fileflags;
  int globalf;
  char filename[1024]; /* 1024 = FILE_MAX */

  struct bScreen *curscreen; /* TODO: think this isn't needed anymore? */
  struct Scene *curscene;
  struct ViewLayer *cur_view_layer; /* layer to activate in workspaces when reading without UI */

  eBlenFileType type;
} BlendFileData;

struct BlendFileReadParams {
  uint skip_flags : 3; /* #eBLOReadSkip */
  uint is_startup : 1;

  /** Whether we are reading the memfile for an undo or a redo. */
  int undo_direction; /* #eUndoStepDir */
};

typedef struct BlendFileReadReport {
  /* General reports handling. */
  struct ReportList *reports;

  /* Timing information. */
  struct {
    double whole;
    double libraries;
    double lib_overrides;
    double lib_overrides_resync;
    double lib_overrides_recursive_resync;
  } duration;

  /* Count information. */
  struct {
    /* Some numbers of IDs that ended up in a specific state, or required some specific process
     * during this file read. */
    int missing_libraries;
    int missing_linked_id;
    /* Some sub-categories of the above `missing_linked_id` counter. */
    int missing_obdata;
    int missing_obproxies;

    /* Number of root override IDs that were resynced. */
    int resynced_lib_overrides;

    /* Number of (non-converted) linked proxies. */
    int linked_proxies;
    /* Number of proxies converted to library overrides. */
    int proxies_to_lib_overrides_success;
    /* Number of proxies that failed to convert to library overrides. */
    int proxies_to_lib_overrides_failures;
    /* Number of sequencer strips that were not read because were in non-supported channels. */
    int vse_strips_skipped;
  } count;

  /* Number of libraries which had overrides that needed to be resynced, and a single linked list
   * of those. */
  int resynced_lib_overrides_libraries_count;
  bool do_resynced_lib_overrides_libraries_list;
  struct LinkNode *resynced_lib_overrides_libraries;
} BlendFileReadReport;

/* skip reading some data-block types (may want to skip screen data too). */
typedef enum eBLOReadSkip {
  BLO_READ_SKIP_NONE = 0,
  BLO_READ_SKIP_USERDEF = (1 << 0),
  BLO_READ_SKIP_DATA = (1 << 1),
  /** Do not attempt to re-use IDs from old bmain for unchanged ones in case of undo. */
  BLO_READ_SKIP_UNDO_OLD_MAIN = (1 << 2),
} eBLOReadSkip;
#define BLO_READ_SKIP_ALL (BLO_READ_SKIP_USERDEF | BLO_READ_SKIP_DATA)

BlendFileData *BLO_read_from_file(const char *filepath,
                                  eBLOReadSkip skip_flags,
                                  struct BlendFileReadReport *reports);
BlendFileData *BLO_read_from_memory(const void *mem,
                                    int memsize,
                                    eBLOReadSkip skip_flags,
                                    struct ReportList *reports);
BlendFileData *BLO_read_from_memfile(struct Main *oldmain,
                                     const char *filename,
                                     struct MemFile *memfile,
                                     const struct BlendFileReadParams *params,
                                     struct ReportList *reports);

void BLO_blendfiledata_free(BlendFileData *bfd);

/** \} */

/* -------------------------------------------------------------------- */
/** \name BLO Blend File Handle API
 * \{ */

typedef struct BLODataBlockInfo {
  char name[64]; /* MAX_NAME */
  struct AssetMetaData *asset_data;
} BLODataBlockInfo;

BlendHandle *BLO_blendhandle_from_file(const char *filepath, struct BlendFileReadReport *reports);
BlendHandle *BLO_blendhandle_from_memory(const void *mem,
                                         int memsize,
                                         struct BlendFileReadReport *reports);

struct LinkNode *BLO_blendhandle_get_datablock_names(BlendHandle *bh,
                                                     int ofblocktype,

                                                     const bool use_assets_only,
                                                     int *r_tot_names);
struct LinkNode * /*BLODataBlockInfo */ BLO_blendhandle_get_datablock_info(
    BlendHandle *bh, int ofblocktype, const bool use_assets_only, int *r_tot_info_items);
struct LinkNode *BLO_blendhandle_get_previews(BlendHandle *bh, int ofblocktype, int *r_tot_prev);
struct PreviewImage *BLO_blendhandle_get_preview_for_id(BlendHandle *bh,
                                                        int ofblocktype,
                                                        const char *name);
struct LinkNode *BLO_blendhandle_get_linkable_groups(BlendHandle *bh);

void BLO_blendhandle_close(BlendHandle *bh);

/** \} */

#define BLO_GROUP_MAX 32
#define BLO_EMBEDDED_STARTUP_BLEND "<startup.blend>"

bool BLO_has_bfile_extension(const char *str);
bool BLO_library_path_explode(const char *path, char *r_dir, char **r_group, char **r_name);

/* -------------------------------------------------------------------- */
/** \name BLO Blend File Linking API
 * \{ */

/**
 * Options controlling behavior of append/link code.
 * \note merged with 'user-level' options from operators etc. in 16 lower bits
 * (see #eFileSel_Params_Flag in DNA_space_types.h).
 */
typedef enum eBLOLibLinkFlags {
  /** Generate a placeholder (empty ID) if not found in current lib file. */
  BLO_LIBLINK_USE_PLACEHOLDERS = 1 << 16,
  /** Force loaded ID to be tagged as #LIB_TAG_INDIRECT (used in reload context only). */
  BLO_LIBLINK_FORCE_INDIRECT = 1 << 17,
  /**
   * When set, tag ID types that pass the internal check #library_link_idcode_needs_tag_check
   *
   * Currently this is only used to instantiate objects in the scene.
   * Set this from #BLO_library_link_params_init_with_context so callers
   * don't need to remember to set this flag.
   */
  BLO_LIBLINK_NEEDS_ID_TAG_DOIT = 1 << 18,
  /** Set fake user on appended IDs. */
  BLO_LIBLINK_APPEND_SET_FAKEUSER = 1 << 19,
  /** Append (make local) also indirect dependencies of appended IDs coming from other libraries.
   * NOTE: All IDs (including indirectly linked ones) coming from the same initial library are
   * always made local. */
  BLO_LIBLINK_APPEND_RECURSIVE = 1 << 20,
  /** Try to re-use previously appended matching ID on new append. */
  BLO_LIBLINK_APPEND_LOCAL_ID_REUSE = 1 << 21,
  /** Instantiate object data IDs (i.e. create objects for them if needed). */
  BLO_LIBLINK_OBDATA_INSTANCE = 1 << 24,
  /** Instantiate collections as empties, instead of linking them into current view layer. */
  BLO_LIBLINK_COLLECTION_INSTANCE = 1 << 25,
} eBLOLibLinkFlags;

/**
 * Struct for passing arguments to
 * #BLO_library_link_begin, #BLO_library_link_named_part & #BLO_library_link_end.
 * Wrap these in parameters since it's important both functions receive matching values.
 */
struct LibraryLink_Params {
  /** The current main database, e.g. #G_MAIN or `CTX_data_main(C)`. */
  struct Main *bmain;
  /** Options for linking, used for instantiating. */
  int flag;
  /** Additional tag for #ID.tag. */
  int id_tag_extra;
  /** Context for instancing objects (optional, no instantiation will be performed when NULL). */
  struct {
    /** The scene in which to instantiate objects/collections. */
    struct Scene *scene;
    /** The scene layer in which to instantiate objects/collections. */
    struct ViewLayer *view_layer;
    /** The active 3D viewport (only used to define local-view). */
    const struct View3D *v3d;
  } context;
};

void BLO_library_link_params_init(struct LibraryLink_Params *params,
                                  struct Main *bmain,
                                  const int flag,
                                  const int id_tag_extra);
void BLO_library_link_params_init_with_context(struct LibraryLink_Params *params,
                                               struct Main *bmain,
                                               const int flag,
                                               const int id_tag_extra,
                                               struct Scene *scene,
                                               struct ViewLayer *view_layer,
                                               const struct View3D *v3d);

struct Main *BLO_library_link_begin(BlendHandle **bh,
                                    const char *filepath,
                                    const struct LibraryLink_Params *params);
struct ID *BLO_library_link_named_part(struct Main *mainl,
                                       BlendHandle **bh,
                                       const short idcode,
                                       const char *name,
                                       const struct LibraryLink_Params *params);
void BLO_library_link_end(struct Main *mainl,
                          BlendHandle **bh,
                          const struct LibraryLink_Params *params);

int BLO_library_link_copypaste(struct Main *mainl, BlendHandle *bh, const uint64_t id_types_mask);

/**
 * Struct for temporarily loading datablocks from a blend file.
 */
typedef struct TempLibraryContext {
  /** Temporary main used for library data. */
  struct Main *bmain_lib;
  /** Temporary main used to load data into (currently initialized from `real_main`). */
  struct Main *bmain_base;
  struct BlendHandle *blendhandle;
  struct BlendFileReadReport bf_reports;
  struct LibraryLink_Params liblink_params;
  struct Library *lib;

  /* The ID datablock that was loaded. Is NULL if loading failed. */
  struct ID *temp_id;
} TempLibraryContext;

TempLibraryContext *BLO_library_temp_load_id(struct Main *real_main,
                                             const char *blend_file_path,
                                             const short idcode,
                                             const char *idname,
                                             struct ReportList *reports);
void BLO_library_temp_free(TempLibraryContext *temp_lib_ctx);

/** \} */

void *BLO_library_read_struct(struct FileData *fd, struct BHead *bh, const char *blockname);

/* internal function but we need to expose it */
void blo_lib_link_restore(struct Main *oldmain,
                          struct Main *newmain,
                          struct wmWindowManager *curwm,
                          struct Scene *curscene,
                          struct ViewLayer *cur_view_layer);

typedef void (*BLOExpandDoitCallback)(void *fdhandle, struct Main *mainvar, void *idv);

void BLO_main_expander(BLOExpandDoitCallback expand_doit_func);
void BLO_expand_main(void *fdhandle, struct Main *mainvar);

/**
 * Update defaults in startup.blend, without having to save and embed it.
 * \note defaults for preferences are stored in `userdef_default.c` and can be updated there.
 */
void BLO_update_defaults_startup_blend(struct Main *bmain, const char *app_template);
void BLO_update_defaults_workspace(struct WorkSpace *workspace, const char *app_template);

/* Disable unwanted experimental feature settings on startup. */
void BLO_sanitize_experimental_features_userpref_blend(struct UserDef *userdef);

struct BlendThumbnail *BLO_thumbnail_from_file(const char *filepath);

/* datafiles (generated theme) */
extern const struct bTheme U_theme_default;
extern const struct UserDef U_default;

#ifdef __cplusplus
}
#endif

/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */
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
  char filepath[1024]; /* 1024 = FILE_MAX */

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

    /* Number of proxies converted to library overrides. */
    int proxies_to_lib_overrides_success;
    /* Number of proxies that failed to convert to library overrides. */
    int proxies_to_lib_overrides_failures;
    /* Number of sequencer strips that were not read because were in non-supported channels. */
    int sequence_strips_skipped;
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
ENUM_OPERATORS(eBLOReadSkip, BLO_READ_SKIP_UNDO_OLD_MAIN)
#define BLO_READ_SKIP_ALL (BLO_READ_SKIP_USERDEF | BLO_READ_SKIP_DATA)

/**
 * Open a blender file from a pathname. The function returns NULL
 * and sets a report in the list if it cannot open the file.
 *
 * \param filepath: The path of the file to open.
 * \param reports: If the return value is NULL, errors indicating the cause of the failure.
 * \return The data of the file.
 */
BlendFileData *BLO_read_from_file(const char *filepath,
                                  eBLOReadSkip skip_flags,
                                  struct BlendFileReadReport *reports);
/**
 * Open a blender file from memory. The function returns NULL
 * and sets a report in the list if it cannot open the file.
 *
 * \param mem: The file data.
 * \param memsize: The length of \a mem.
 * \param reports: If the return value is NULL, errors indicating the cause of the failure.
 * \return The data of the file.
 */
BlendFileData *BLO_read_from_memory(const void *mem,
                                    int memsize,
                                    eBLOReadSkip skip_flags,
                                    struct ReportList *reports);
/**
 * Used for undo/redo, skips part of libraries reading
 * (assuming their data are already loaded & valid).
 *
 * \param oldmain: old main,
 * from which we will keep libraries and other data-blocks that should not have changed.
 * \param filepath: current file, only for retrieving library data.
 * Typically `BKE_main_blendfile_path(oldmain)`.
 */
BlendFileData *BLO_read_from_memfile(struct Main *oldmain,
                                     const char *filepath,
                                     struct MemFile *memfile,
                                     const struct BlendFileReadParams *params,
                                     struct ReportList *reports);

/**
 * Frees a BlendFileData structure and *all* the data associated with it
 * (the userdef data, and the main libblock data).
 *
 * \param bfd: The structure to free.
 */
void BLO_blendfiledata_free(BlendFileData *bfd);

/** \} */

/* -------------------------------------------------------------------- */
/** \name BLO Blend File Handle API
 * \{ */

typedef struct BLODataBlockInfo {
  char name[64]; /* MAX_NAME */
  struct AssetMetaData *asset_data;
  bool free_asset_data;
  /* Optimization: Tag data-blocks for which we know there is no preview.
   * Knowing this can be used to skip the (potentially expensive) preview loading process. If this
   * is set to true it means we looked for a preview and couldn't find one. False may mean that
   * either no preview was found, or that it wasn't looked for in the first place. */
  bool no_preview_found;
} BLODataBlockInfo;

/**
 * Frees contained data, not \a datablock_info itself.
 */
void BLO_datablock_info_free(BLODataBlockInfo *datablock_info);
/**
 * Can be used to free the list returned by #BLO_blendhandle_get_datablock_info().
 */
void BLO_datablock_info_linklist_free(struct LinkNode * /*BLODataBlockInfo*/ datablock_infos);

/**
 * Open a blendhandle from a file path.
 *
 * \param filepath: The file path to open.
 * \param reports: Report errors in opening the file (can be NULL).
 * \return A handle on success, or NULL on failure.
 */
BlendHandle *BLO_blendhandle_from_file(const char *filepath, struct BlendFileReadReport *reports);
/**
 * Open a blendhandle from memory.
 *
 * \param mem: The data to load from.
 * \param memsize: The size of the data.
 * \return A handle on success, or NULL on failure.
 */
BlendHandle *BLO_blendhandle_from_memory(const void *mem,
                                         int memsize,
                                         struct BlendFileReadReport *reports);

/**
 * Gets the names of all the data-blocks in a file of a certain type
 * (e.g. all the scene names in a file).
 *
 * \param bh: The blendhandle to access.
 * \param ofblocktype: The type of names to get.
 * \param use_assets_only: Only list IDs marked as assets.
 * \param r_tot_names: The length of the returned list.
 * \return A BLI_linklist of strings. The string links should be freed with #MEM_freeN().
 */
struct LinkNode *BLO_blendhandle_get_datablock_names(BlendHandle *bh,
                                                     int ofblocktype,

                                                     bool use_assets_only,
                                                     int *r_tot_names);
/**
 * Gets the names and asset-data (if ID is an asset) of data-blocks in a file of a certain type.
 * The data-blocks can be limited to assets.
 *
 * \param bh: The blendhandle to access.
 * \param ofblocktype: The type of names to get.
 * \param use_assets_only: Limit the result to assets only.
 * \param r_tot_info_items: The length of the returned list.
 *
 * \return A BLI_linklist of `BLODataBlockInfo *`.
 *
 * \note The links should be freed using #BLO_datablock_info_free() or the entire list using
 *       #BLO_datablock_info_linklist_free().
 */
struct LinkNode * /*BLODataBlockInfo*/ BLO_blendhandle_get_datablock_info(BlendHandle *bh,
                                                                          int ofblocktype,
                                                                          bool use_assets_only,
                                                                          int *r_tot_info_items);
/**
 * Gets the previews of all the data-blocks in a file of a certain type
 * (e.g. all the scene previews in a file).
 *
 * \param bh: The blendhandle to access.
 * \param ofblocktype: The type of names to get.
 * \param r_tot_prev: The length of the returned list.
 * \return A BLI_linklist of #PreviewImage. The #PreviewImage links should be freed with malloc.
 */
struct LinkNode *BLO_blendhandle_get_previews(BlendHandle *bh, int ofblocktype, int *r_tot_prev);
/**
 * Get the PreviewImage of a single data block in a file.
 * (e.g. all the scene previews in a file).
 *
 * \param bh: The blendhandle to access.
 * \param ofblocktype: The type of names to get.
 * \param name: Name of the block without the ID_ prefix, to read the preview image from.
 * \return PreviewImage or NULL when no preview Images have been found. Caller owns the returned
 */
struct PreviewImage *BLO_blendhandle_get_preview_for_id(BlendHandle *bh,
                                                        int ofblocktype,
                                                        const char *name);
/**
 * Gets the names of all the linkable data-block types available in a file.
 * (e.g. "Scene", "Mesh", "Light", etc.).
 *
 * \param bh: The blendhandle to access.
 * \return A BLI_linklist of strings. The string links should be freed with #MEM_freeN().
 */
struct LinkNode *BLO_blendhandle_get_linkable_groups(BlendHandle *bh);

/**
 * Close and free a blendhandle. The handle becomes invalid after this call.
 *
 * \param bh: The handle to close.
 */
void BLO_blendhandle_close(BlendHandle *bh);

/** Mark the given Main (and the 'root' local one in case of lib-split Mains) as invalid, and
 * generate an error report containing given `message`. */
void BLO_read_invalidate_message(BlendHandle *bh, struct Main *bmain, const char *message);

/**
 * BLI_assert-like macro to check a condition, and if `false`, fail the whole .blend reading
 * process by marking the Main data-base as invalid, and returning provided `_ret_value`.
 *
 * NOTE: About usages:
 *   - #BLI_assert should be used when the error is considered as a bug, but there is some code to
 *     recover from it and produce a valid Main data-base.
 *   - #BLO_read_assert_message should be used when the error is not considered as recoverable.
 */
#define BLO_read_assert_message(_check_expr, _ret_value, _bh, _bmain, _message) \
  if (_check_expr) { \
    BLO_read_invalidate_message((_bh), (_bmain), (_message)); \
    return _ret_value; \
  } \
  (void)0

/** \} */

#define BLO_GROUP_MAX 32
#define BLO_EMBEDDED_STARTUP_BLEND "<startup.blend>"

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
  /** Set fake user on appended IDs. */
  BLO_LIBLINK_APPEND_SET_FAKEUSER = 1 << 19,
  /** Append (make local) also indirect dependencies of appended IDs coming from other libraries.
   * NOTE: All IDs (including indirectly linked ones) coming from the same initial library are
   * always made local. */
  BLO_LIBLINK_APPEND_RECURSIVE = 1 << 20,
  /** Try to re-use previously appended matching ID on new append. */
  BLO_LIBLINK_APPEND_LOCAL_ID_REUSE = 1 << 21,
  /** Clear the asset data. */
  BLO_LIBLINK_APPEND_ASSET_DATA_CLEAR = 1 << 22,
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
typedef struct LibraryLink_Params {
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
} LibraryLink_Params;

void BLO_library_link_params_init(struct LibraryLink_Params *params,
                                  struct Main *bmain,
                                  int flag,
                                  int id_tag_extra);
void BLO_library_link_params_init_with_context(struct LibraryLink_Params *params,
                                               struct Main *bmain,
                                               int flag,
                                               int id_tag_extra,
                                               struct Scene *scene,
                                               struct ViewLayer *view_layer,
                                               const struct View3D *v3d);

/**
 * Initialize the #BlendHandle for linking library data.
 *
 * \param bh: A blender file handle as returned by
 * #BLO_blendhandle_from_file or #BLO_blendhandle_from_memory.
 * \param filepath: Used for relative linking, copied to the `lib->filepath`.
 * \param params: Settings for linking that don't change from beginning to end of linking.
 * \return the library #Main, to be passed to #BLO_library_link_named_part as \a mainl.
 */
struct Main *BLO_library_link_begin(BlendHandle **bh,
                                    const char *filepath,
                                    const struct LibraryLink_Params *params);
/**
 * Link a named data-block from an external blend file.
 *
 * \param mainl: The main database to link from (not the active one).
 * \param bh: The blender file handle.
 * \param idcode: The kind of data-block to link.
 * \param name: The name of the data-block (without the 2 char ID prefix).
 * \return the linked ID when found.
 */
struct ID *BLO_library_link_named_part(struct Main *mainl,
                                       BlendHandle **bh,
                                       short idcode,
                                       const char *name,
                                       const struct LibraryLink_Params *params);
/**
 * Finalize linking from a given .blend file (library).
 * Optionally instance the indirect object/collection in the scene when the flags are set.
 * \note Do not use \a bh after calling this function, it may frees it.
 *
 * \param mainl: The main database to link from (not the active one).
 * \param bh: The blender file handle (WARNING! may be freed by this function!).
 * \param params: Settings for linking that don't change from beginning to end of linking.
 */
void BLO_library_link_end(struct Main *mainl,
                          BlendHandle **bh,
                          const struct LibraryLink_Params *params);

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
                                             short idcode,
                                             const char *idname,
                                             struct ReportList *reports);
void BLO_library_temp_free(TempLibraryContext *temp_lib_ctx);

/** \} */

void *BLO_library_read_struct(struct FileData *fd, struct BHead *bh, const char *blockname);

/* internal function but we need to expose it */
/**
 * Used to link a file (without UI) to the current UI.
 * Note that it assumes the old pointers in UI are still valid, so old Main is not freed.
 */
void blo_lib_link_restore(struct Main *oldmain,
                          struct Main *newmain,
                          struct wmWindowManager *curwm,
                          struct Scene *curscene,
                          struct ViewLayer *cur_view_layer);

typedef void (*BLOExpandDoitCallback)(void *fdhandle, struct Main *mainvar, void *idv);

/**
 * Set the callback func used over all ID data found by \a BLO_expand_main func.
 *
 * \param expand_doit_func: Called for each ID block it finds.
 */
void BLO_main_expander(BLOExpandDoitCallback expand_doit_func);
/**
 * Loop over all ID data in Main to mark relations.
 * Set (id->tag & LIB_TAG_NEED_EXPAND) to mark expanding. Flags get cleared after expanding.
 *
 * \param fdhandle: usually filedata, or own handle.
 * \param mainvar: the Main database to expand.
 */
void BLO_expand_main(void *fdhandle, struct Main *mainvar);

/**
 * Update defaults in startup.blend, without having to save and embed it.
 * \note defaults for preferences are stored in `userdef_default.c` and can be updated there.
 */
/**
 * Update defaults in startup.blend, without having to save and embed the file.
 * This function can be emptied each time the startup.blend is updated.
 *
 * \note Screen data may be cleared at this point, this will happen in the case
 * an app-template's data needs to be versioned when read-file is called with "Load UI" disabled.
 * Versioning the screen data can be safely skipped without "Load UI" since the screen data
 * will have been versioned when it was first loaded.
 */
void BLO_update_defaults_startup_blend(struct Main *bmain, const char *app_template);
void BLO_update_defaults_workspace(struct WorkSpace *workspace, const char *app_template);

/* Disable unwanted experimental feature settings on startup. */
void BLO_sanitize_experimental_features_userpref_blend(struct UserDef *userdef);

/**
 * Does a very light reading of given .blend file to extract its stored thumbnail.
 *
 * \param filepath: The path of the file to extract thumbnail from.
 * \return The raw thumbnail
 * (MEM-allocated, as stored in file, use #BKE_main_thumbnail_to_imbuf()
 * to convert it to ImBuf image).
 */
struct BlendThumbnail *BLO_thumbnail_from_file(const char *filepath);

/* datafiles (generated theme) */
extern const struct bTheme U_theme_default;
extern const struct UserDef U_default;

#ifdef __cplusplus
}
#endif

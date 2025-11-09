/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

#include "DNA_listBase.h"

#include "BLI_compiler_attrs.h"
#include "BLI_enum_flags.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_sys_types.h"
#include "BLI_utility_mixins.hh"

/** \file
 * \ingroup blenloader
 * \brief external readfile function prototypes.
 */

struct AssetMetaData;
struct BHead;
struct BlendfileLinkAppendContext;
struct BlendHandle;
struct BlendThumbnail;
struct FileData;
struct FileReader;
struct ID;
struct Library;
struct LinkNode;
struct ListBase;
struct Main;
struct MemFile;
struct PreviewImage;
struct ReportList;
struct Scene;
struct UserDef;
struct View3D;
struct ViewLayer;
struct WorkSpace;
struct bScreen;
struct wmWindowManager;

struct WorkspaceConfigFileData {
  Main *main; /* has to be freed when done reading file data */

  ListBase workspaces;
};

/* -------------------------------------------------------------------- */
/** \name BLO Read File API
 *
 * \see #BLO_write_file for file writing.
 * \{ */

enum eBlenFileType {
  BLENFILETYPE_BLEND = 1,
  // BLENFILETYPE_PUB = 2,     /* UNUSED */
  // BLENFILETYPE_RUNTIME = 3, /* UNUSED */
};

struct BlendFileData : blender::NonCopyable, blender::NonMovable {
  Main *main = nullptr;
  UserDef *user = nullptr;

  int fileflags = 0;
  int globalf = 0;
  /** Typically the actual filepath of the read blend-file, except when recovering
   * save-on-exit/autosave files. In the latter case, it will be the path of the file that
   * generated the auto-saved one being recovered.
   *
   * NOTE: Currently expected to be the same path as #BlendFileData.filepath. */
  char filepath[/*FILE_MAX*/ 1024] = {};

  /** TODO: think this isn't needed anymore? */
  bScreen *curscreen = nullptr;
  Scene *curscene = nullptr;
  /** Layer to activate in workspaces when reading without UI. */
  ViewLayer *cur_view_layer = nullptr;

  eBlenFileType type = eBlenFileType(0);
};

/**
 * Data used by WM readfile code and BKE's setup_app_data to handle the complex preservation logic
 * of WindowManager and other UI data-blocks across blend-file reading process.
 */
struct BlendFileReadWMSetupData {
  /** The existing WM when file-reading process is started. */
  wmWindowManager *old_wm;

  /** The startup file is being read. */
  bool is_read_homefile;
  /** The factory startup file is being read. */
  bool is_factory_startup;
};

struct BlendFileReadParams {
  uint skip_flags : 3; /* #eBLOReadSkip */
  uint is_startup : 1;
  uint is_factory_settings : 1;

  /** Whether we are reading the memfile for an undo or a redo. */
  int undo_direction; /* #eUndoStepDir */
};

struct BlendFileReadReport {
  /** General reports handling. */
  ReportList *reports;

  /** Timing information. */
  struct {
    double whole;
    double libraries;
    double lib_overrides;
    double lib_overrides_resync;
    double lib_overrides_recursive_resync;
  } duration;

  /** Count information. */
  struct {
    /**
     * Some numbers of IDs that ended up in a specific state, or required some specific process
     * during this file read.
     */
    int missing_libraries;
    int missing_linked_id;
    /** Some sub-categories of the above `missing_linked_id` counter. */
    int missing_obdata;
    int missing_obproxies;

    /** Number of root override IDs that were resynced. */
    int resynced_lib_overrides;

    /** Number of proxies converted to library overrides. */
    int proxies_to_lib_overrides_success;
    /** Number of proxies that failed to convert to library overrides. */
    int proxies_to_lib_overrides_failures;
    /** Number of sequencer strips that were not read because were in non-supported channels. */
    int sequence_strips_skipped;
  } count;

  /**
   * Number of libraries which had overrides that needed to be resynced,
   * and a single linked list of those.
   */
  int resynced_lib_overrides_libraries_count;
  bool do_resynced_lib_overrides_libraries_list;
  LinkNode *resynced_lib_overrides_libraries;

  /** Whether a pre-2.50 blend file was loaded, in which case any animation is lost. */
  bool pre_animato_file_loaded;
};

/** Skip reading some data-block types (may want to skip screen data too). */
enum eBLOReadSkip {
  BLO_READ_SKIP_NONE = 0,
  /** Skip #BLO_CODE_USER blocks. */
  BLO_READ_SKIP_USERDEF = (1 << 0),
  /** Only read #BLO_CODE_USER (and associated data). */
  BLO_READ_SKIP_DATA = (1 << 1),
  /** Do not attempt to re-use IDs from old bmain for unchanged ones in case of undo. */
  BLO_READ_SKIP_UNDO_OLD_MAIN = (1 << 2),
};
ENUM_OPERATORS(eBLOReadSkip)
#define BLO_READ_SKIP_ALL (BLO_READ_SKIP_USERDEF | BLO_READ_SKIP_DATA)

/**
 * Open a blender file from a `filepath`. The function returns NULL
 * and sets a report in the list if it cannot open the file.
 *
 * \param filepath: The path of the file to open.
 * \param reports: If the return value is NULL, errors indicating the cause of the failure.
 * \return The data of the file.
 */
BlendFileData *BLO_read_from_file(const char *filepath,
                                  eBLOReadSkip skip_flags,
                                  BlendFileReadReport *reports);
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
                                    ReportList *reports);
/**
 * Used for undo/redo, skips part of libraries reading
 * (assuming their data are already loaded & valid).
 *
 * \param oldmain: old main,
 * from which we will keep libraries and other data-blocks that should not have changed.
 * \param filepath: current file, only for retrieving library data.
 * Typically `BKE_main_blendfile_path(oldmain)`.
 */
BlendFileData *BLO_read_from_memfile(Main *oldmain,
                                     const char *filepath,
                                     MemFile *memfile,
                                     const BlendFileReadParams *params,
                                     ReportList *reports);

/**
 * Frees a BlendFileData structure and *all* the data associated with it
 * (the userdef data, and the main libblock data).
 *
 * \param bfd: The structure to free.
 */
void BLO_blendfiledata_free(BlendFileData *bfd);

/**
 * Does versioning code that requires the Main data-base to be fully loaded and valid.
 *
 * readfile's `do_versions` does not allow to create (or delete) IDs, and only operates on a single
 * library at a time.
 *
 * Called at the end of #setup_add_data from BKE's `blendfile.cc`.
 *
 * \param new_bmain: the newly read Main data-base.
 */
void BLO_read_do_version_after_setup(Main *new_bmain,
                                     BlendfileLinkAppendContext *lapp_context,
                                     BlendFileReadReport *reports);

/** \} */

/* -------------------------------------------------------------------- */
/** \name BLO Blend File Handle API
 * \{ */

struct BLODataBlockInfo {
  char name[/*MAX_ID_NAME-2*/ 256];
  AssetMetaData *asset_data;
  /** Ownership over #asset_data above can be "stolen out" of this struct, for more permanent
   * storage. In that case, set this to false to avoid double freeing of the stolen data. */
  bool free_asset_data;
  /**
   * Optimization: Tag data-blocks for which we know there is no preview.
   * Knowing this can be used to skip the (potentially expensive) preview loading process. If this
   * is set to true it means we looked for a preview and couldn't find one. False may mean that
   * either no preview was found, or that it wasn't looked for in the first place.
   */
  bool no_preview_found;
};

/**
 * Frees contained data, not \a datablock_info itself.
 */
void BLO_datablock_info_free(BLODataBlockInfo *datablock_info);
/**
 * Can be used to free the list returned by #BLO_blendhandle_get_datablock_info().
 */
void BLO_datablock_info_linklist_free(LinkNode * /*BLODataBlockInfo*/ datablock_infos);

/**
 * Open a blendhandle from a file path.
 *
 * \param filepath: The file path to open.
 * \param reports: Report errors in opening the file (can be NULL).
 * \return A handle on success, or NULL on failure.
 */
BlendHandle *BLO_blendhandle_from_file(const char *filepath, BlendFileReadReport *reports);
/**
 * Open a blendhandle from memory.
 *
 * \param mem: The data to load from.
 * \param memsize: The size of the data.
 * \return A handle on success, or NULL on failure.
 */
BlendHandle *BLO_blendhandle_from_memory(const void *mem,
                                         int memsize,
                                         BlendFileReadReport *reports);

/** Returns the major and minor version number of Blender used to create the file. */
blender::int3 BLO_blendhandle_get_version(const BlendHandle *bh);

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
LinkNode *BLO_blendhandle_get_datablock_names(BlendHandle *bh,
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
LinkNode * /*BLODataBlockInfo*/ BLO_blendhandle_get_datablock_info(BlendHandle *bh,
                                                                   int ofblocktype,
                                                                   bool use_assets_only,
                                                                   int *r_tot_info_items);
/**
 * Get the PreviewImage of a single data block in a file.
 * (e.g. all the scene previews in a file).
 *
 * \param bh: The blendhandle to access.
 * \param ofblocktype: The type of names to get.
 * \param name: Name of the block without the ID_ prefix, to read the preview image from.
 * \return PreviewImage or NULL when no preview Images have been found. Caller owns the returned
 */
PreviewImage *BLO_blendhandle_get_preview_for_id(BlendHandle *bh,
                                                 int ofblocktype,
                                                 const char *name);
/**
 * Gets the names of all the linkable data-block types available in a file.
 * (e.g. "Scene", "Mesh", "Light", etc.).
 *
 * \param bh: The blendhandle to access.
 * \return A BLI_linklist of strings. The string links should be freed with #MEM_freeN().
 */
LinkNode *BLO_blendhandle_get_linkable_groups(BlendHandle *bh);

/**
 * Close and free a blendhandle. The handle becomes invalid after this call.
 *
 * \param bh: The handle to close.
 */
void BLO_blendhandle_close(BlendHandle *bh) ATTR_NONNULL(1);

/**
 * Mark the given Main (and the 'root' local one in case of lib-split Mains) as invalid, and
 * generate an error report containing given `message`.
 */
void BLO_read_invalidate_message(BlendHandle *bh, Main *bmain, const char *message);

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
enum eBLOLibLinkFlags {
  /** Generate a placeholder (empty ID) if not found in current lib file. */
  BLO_LIBLINK_USE_PLACEHOLDERS = 1 << 16,
  /** Force loaded ID to be tagged as #ID_TAG_INDIRECT (used in reload context only). */
  BLO_LIBLINK_FORCE_INDIRECT = 1 << 17,
  /**
   * Set the object active when #OB_FLAG_ACTIVE_CLIPBOARD is set.
   * Used for copy & paste so the active object is preserved.
   */
  BLO_LIBLINK_APPEND_SET_OB_ACTIVE_CLIPBOARD = 1 << 18,
  /** Set fake user on appended IDs. */
  BLO_LIBLINK_APPEND_SET_FAKEUSER = 1 << 19,
  /**
   * Append (make local) also indirect dependencies of appended IDs coming from other libraries.
   * NOTE: All IDs (including indirectly linked ones) coming from the same initial library are
   * always made local.
   */
  BLO_LIBLINK_APPEND_RECURSIVE = 1 << 20,
  /** Try to re-use previously appended matching ID on new append. */
  BLO_LIBLINK_APPEND_LOCAL_ID_REUSE = 1 << 21,
  /** Clear the asset data. */
  BLO_LIBLINK_APPEND_ASSET_DATA_CLEAR = 1 << 22,
  /** Instantiate object data IDs (i.e. create objects for them if needed). */
  BLO_LIBLINK_OBDATA_INSTANCE = 1 << 24,
  /** Instantiate collections as empties, instead of linking them into current view layer. */
  BLO_LIBLINK_COLLECTION_INSTANCE = 1 << 25,
  /**
   * Do not rebuild collections hierarchy runtime data (mainly the parents info)
   * as part of #BLO_library_link_end.
   * Needed when some IDs have been temporarily removed from Main,
   * see e.g. #BKE_blendfile_library_relocate.
   */
  BLO_LIBLINK_COLLECTION_NO_HIERARCHY_REBUILD = 1 << 26,
  /**
   * Pack the linked data-blocks to keep them working even if the source file is not available.
   */
  BLO_LIBLINK_PACK = 1 << 27,
};

/**
 * Struct for passing arguments to
 * #BLO_library_link_begin, #BLO_library_link_named_part & #BLO_library_link_end.
 * Wrap these in parameters since it's important both functions receive matching values.
 */
struct LibraryLink_Params {
  /** The current main database, e.g. #G_MAIN or `CTX_data_main(C)`. */
  Main *bmain;
  /** Options for linking, used for instantiating. */
  int flag;
  /** Additional tag for #ID.tag. */
  int id_tag_extra;
  /** Context for instancing objects (optional, no instantiation will be performed when NULL). */
  struct {
    /** The scene in which to instantiate objects/collections. */
    Scene *scene;
    /** The scene layer in which to instantiate objects/collections. */
    ViewLayer *view_layer;
    /** The active 3D viewport (only used to define local-view). */
    const View3D *v3d;
  } context;
};

void BLO_library_link_params_init(LibraryLink_Params *params,
                                  Main *bmain,
                                  int flag,
                                  int id_tag_extra);
void BLO_library_link_params_init_with_context(LibraryLink_Params *params,
                                               Main *bmain,
                                               int flag,
                                               int id_tag_extra,
                                               Scene *scene,
                                               ViewLayer *view_layer,
                                               const View3D *v3d);

/**
 * Initialize the #BlendHandle for linking library data.
 *
 * \param bh: A blender file handle as returned by
 * #BLO_blendhandle_from_file or #BLO_blendhandle_from_memory.
 * \param filepath: Used for relative linking, copied to the `lib->filepath`.
 * \param params: Settings for linking that don't change from beginning to end of linking.
 * \return the library #Main, to be passed to #BLO_library_link_named_part as \a mainl.
 */
Main *BLO_library_link_begin(BlendHandle **bh,
                             const char *filepath,
                             const LibraryLink_Params *params);
/**
 * Link a named data-block from an external blend file.
 *
 * \param mainl: The main database to link from (not the active one).
 * \param bh: The blender file handle.
 * \param idcode: The kind of data-block to link.
 * \param name: The name of the data-block (without the 2 char ID prefix).
 * \return the linked ID when found.
 */
ID *BLO_library_link_named_part(Main *mainl,
                                BlendHandle **bh,
                                short idcode,
                                const char *name,
                                const LibraryLink_Params *params);
/**
 * Finalize linking from a given .blend file (library).
 * Optionally instance the indirect object/collection in the scene when the flags are set.
 * \note Do not use \a bh after calling this function, it may frees it.
 *
 * \param mainl: The main database to link from (not the active one).
 * \param bh: The blender file handle (WARNING! may be freed by this function!).
 * \param params: Settings for linking that don't change from beginning to end of linking.
 */
void BLO_library_link_end(Main *mainl,
                          BlendHandle **bh,
                          const LibraryLink_Params *params,
                          ReportList *reports);

/**
 * Struct for temporarily loading datablocks from a blend file.
 */
struct TempLibraryContext {
  /** Temporary main used to load data into (currently initialized from `real_main`). */
  Main *bmain_base;
  BlendFileReadReport bf_reports;

  /** The ID datablock that was loaded. Is NULL if loading failed. */
  ID *temp_id;
};

TempLibraryContext *BLO_library_temp_load_id(Main *real_main,
                                             const char *blend_file_path,
                                             short idcode,
                                             const char *idname,
                                             ReportList *reports);
void BLO_library_temp_free(TempLibraryContext *temp_lib_ctx);

/** \} */

void *BLO_library_read_struct(FileData *fd, BHead *bh, const char *blockname);

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
void BLO_update_defaults_startup_blend(Main *bmain, const char *app_template);
void BLO_update_defaults_workspace(WorkSpace *workspace, const char *app_template);

/** Disable unwanted experimental feature settings on startup. */
void BLO_sanitize_experimental_features_userpref_blend(UserDef *userdef);

/**
 * Does a very light reading of given .blend file to extract its stored thumbnail.
 *
 * \param filepath: The path of the file to extract thumbnail from.
 * \return The raw thumbnail
 * (MEM-allocated, as stored in file, use #BKE_main_thumbnail_to_imbuf()
 * to convert it to ImBuf image).
 */
BlendThumbnail *BLO_thumbnail_from_file(const char *filepath);

/**
 * Does a very light reading of given .blend file to extract its version.
 *
 * \param filepath: The path of the blend file to extract version from.
 * \return The file version
 */
short BLO_version_from_file(const char *filepath);

/**
 * Runtime structure on `ID.runtime.readfile_data` that is available during the readfile process.
 *
 * This is intended for short-lived data, for example for things that are detected in an early
 * phase of versioning that should be used in a later stage of versioning.
 *
 * \note This is NOT allocated when 'reading' an undo step, as that doesn't have to deal with
 * versioning, linking, and the other stuff that this struct was meant for.
 */
struct ID_Readfile_Data {
  struct Tags {
    /* General ID reading related tags. */

    /**
     * Mark ID placeholders for linked data-blocks needing to be read from their library
     * blend-files.
     */
    bool is_link_placeholder : 1;
    /**
     * Mark IDs needing to be expanded (only done once). See #expand_main.
     */
    bool needs_expanding : 1;
    /**
     * Mark IDs needing to be 'lib-linked', i.e. to get their pointers to other data-blocks
     * updated from the 'UID' values stored in `.blend` files to the new, actual pointers.
     */
    bool needs_linking : 1;

    /* Specific ID-type reading/versioning related tags. */

    /**
     * Set when this ID used a legacy Action, in which case it also should pick
     * an appropriate slot.
     *
     * \see ANIM_versioning.hh
     */
    bool action_assignment_needs_slot : 1;
  } tags;
};

/**
 * Return `id->runtime->readfile_data->tags` if the `readfile_data` is allocated,
 * otherwise return an all-zero set of tags.
 */
ID_Readfile_Data::Tags BLO_readfile_id_runtime_tags(ID &id);

/**
 * Create the `readfile_data` if needed, and return `id->runtime->readfile_data->tags`.
 *
 * Use it instead of #BLO_readfile_id_runtime_tags when tags need to be set.
 */
ID_Readfile_Data::Tags &BLO_readfile_id_runtime_tags_for_write(ID &id);

/**
 * Free the #ID_Readfile_Data of all IDs in this bmain and all their embedded IDs.
 *
 * This is typically called at the end of the versioning process, as after that
 * `ID.runtime.readfile_data` should no longer be needed.
 */
void BLO_readfile_id_runtime_data_free_all(Main &bmain);

/**
 *  Free the #ID_Readfile_Data of this ID. Does _not_ deal with embedded IDs.
 */
void BLO_readfile_id_runtime_data_free(ID &id);

#define BLEN_THUMB_MEMSIZE_FILE(_x, _y) (sizeof(int) * (2 + (size_t)(_x) * (size_t)(_y)))

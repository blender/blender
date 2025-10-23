/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 * \section aboutmain Main struct
 * Main is the root of the 'data-base' of a Blender context. All data is put into lists, and all
 * these lists are stored here.
 *
 * \note A Blender file is not much more than a binary dump of these lists. This list of lists is
 * not serialized itself.
 *
 * \note `BKE_main` files are for operations over the Main database itself, or generating extra
 * temp data to help working with it. Those should typically not affect the data-blocks themselves.
 *
 * \section Function Names
 *
 * - `BKE_main_` should be used for functions in that file.
 */

#include <array>

#include "DNA_listBase.h"

#include "BLI_compiler_attrs.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_sys_types.h"
#include "BLI_utility_mixins.hh"
#include "BLI_vector_set.hh"

#include "BKE_lib_query.hh" /* For LibraryForeachIDCallbackFlag. */

struct BLI_mempool;
struct BlendThumbnail;
struct GHash;
struct GSet;
struct ID;
struct IDNameLib_Map;
struct ImBuf;
struct Library;
struct MainLock;
struct ReportList;
struct UniqueName_Map;

/**
 * Blender thumbnail, as written to the `.blend` file (width, height, and data as char RGBA).
 */
struct BlendThumbnail {
  int width, height;
  /** Pixel data, RGBA (repeated): `sizeof(char[4]) * width * height`. */
  char rect[0];
};

/** Structs caching relations between data-blocks in a given Main. */
struct MainIDRelationsEntryItem {
  MainIDRelationsEntryItem *next;

  union {
    /** For `from_ids` list, a user of the hashed ID. */
    ID *from;
    /** For `to_ids` list, an ID used by the hashed ID. */
    ID **to;
  } id_pointer;
  /** Session uid of the `id_pointer`. */
  uint session_uid;

  /** Using IDWALK_ enums, defined in BKE_lib_query.hh */
  LibraryForeachIDCallbackFlag usage_flag;
};

struct MainIDRelationsEntry {
  /** Linked list of IDs using that ID. */
  MainIDRelationsEntryItem *from_ids;
  /** Linked list of IDs used by that ID. */
  MainIDRelationsEntryItem *to_ids;

  /** Session UID of the ID matching that entry. */
  uint session_uid;

  /** Runtime tags, users should ensure those are reset after usage. */
  uint tags;
};

/** #MainIDRelationsEntry.tags */
enum eMainIDRelationsEntryTags {
  /** Generic tag marking the entry as to be processed. */
  MAINIDRELATIONS_ENTRY_TAGS_DOIT = 1 << 0,

  /**
   * Generic tag marking the entry as processed in the `to` direction
   * (i.e. the IDs used by this item have been processed).
   */
  MAINIDRELATIONS_ENTRY_TAGS_PROCESSED_TO = 1 << 4,
  /**
   * Generic tag marking the entry as processed in the `from` direction
   * (i.e. the IDs using this item have been processed).
   */
  MAINIDRELATIONS_ENTRY_TAGS_PROCESSED_FROM = 1 << 5,
  /** Generic tag marking the entry as processed. */
  MAINIDRELATIONS_ENTRY_TAGS_PROCESSED = MAINIDRELATIONS_ENTRY_TAGS_PROCESSED_TO |
                                         MAINIDRELATIONS_ENTRY_TAGS_PROCESSED_FROM,

  /**
   * Generic tag marking the entry as being processed in the `to` direction
   * (i.e. the IDs used by this item are being processed).
   * Useful for dependency loops detection and handling.
   */
  MAINIDRELATIONS_ENTRY_TAGS_INPROGRESS_TO = 1 << 8,
  /**
   * Generic tag marking the entry as being processed in the `from` direction
   * (i.e. the IDs using this item are being processed).
   * Useful for dependency loops detection and handling.
   */
  MAINIDRELATIONS_ENTRY_TAGS_INPROGRESS_FROM = 1 << 9,
  /**
   * Generic tag marking the entry as being processed.
   * Useful for dependency loops detection and handling.
   */
  MAINIDRELATIONS_ENTRY_TAGS_INPROGRESS = MAINIDRELATIONS_ENTRY_TAGS_INPROGRESS_TO |
                                          MAINIDRELATIONS_ENTRY_TAGS_INPROGRESS_FROM,
};

struct MainIDRelations {
  /**
   * Mapping from an ID pointer to all of its parents (IDs using it) and children (IDs it uses).
   * Values are `MainIDRelationsEntry` pointers.
   */
  GHash *relations_from_pointers;
  /* NOTE: we could add more mappings when needed (e.g. from session uid?). */

  short flag;

  /* Private... */
  BLI_mempool *entry_items_pool;
};

enum {
  /** Those bmain relations include pointers/usages from editors. */
  MAINIDRELATIONS_INCLUDE_UI = 1 << 0,
};

struct MainColorspace {
  /*
   * File working colorspace for all scene linear colors.
   * The name is only for the user interface and is not a unique identifier, the matrix is
   * the XYZ colorspace is the source of truth.
   * */
  char scene_linear_name[64 /*MAX_COLORSPACE_NAME*/] = "";
  blender::float3x3 scene_linear_to_xyz = blender::float3x3::zero();

  /*
   * A colorspace, view or display was not found, which likely means the OpenColorIO config
   * used to create this blend file is missing.
   */
  bool is_missing_opencolorio_config = false;
};

struct Main : blender::NonCopyable, blender::NonMovable {
  /**
   * Runtime vector storing all split Mains (one Main for each library data), during readfile or
   * linking process.
   * Shared across all of the split mains when defined.
   */
  std::shared_ptr<blender::VectorSet<Main *>> split_mains = {};
  /**
   * The file-path of this blend file, an empty string indicates an unsaved file.
   *
   * \note For the current loaded blend file this path must be absolute & normalized.
   * This prevents redundant leading slashes or current-working-directory relative paths
   * from causing problems with absolute/relative conversion which relies on this `filepath`
   * being absolute. See #BLI_path_canonicalize_native.
   *
   * This rule is not strictly enforced as in some cases loading a #Main is performed
   * to read data temporarily (preferences & startup) for example
   * where the `filepath` is not persistent or used as a basis for other paths.
   */
  char filepath[/*FILE_MAX*/ 1024] = "";
  /* See BLENDER_FILE_VERSION, BLENDER_FILE_SUBVERSION. */
  short versionfile = 0;
  short subversionfile = 0;
  /* See BLENDER_FILE_MIN_VERSION, BLENDER_FILE_MIN_SUBVERSION. */
  short minversionfile = 0;
  short minsubversionfile = 0;
  /**
   * The currently opened .blend file was written from a newer version of Blender, and has forward
   * compatibility issues (data loss).
   *
   * \note In practice currently this is only based on the version numbers, in the future it
   * could try to use more refined detection on load. */
  bool has_forward_compatibility_issues = false;

  /**
   * This file was written by the asset system with the #G_FILE_ASSET_EDIT_FILE flag (now cleared).
   * It must not be overwritten, except by the asset system itself. Otherwise the file could end up
   * with user created data that would be lost when the asset system regenerates the file.
   */
  bool is_asset_edit_file = false;

  /** Commit timestamp from `buildinfo`. */
  uint64_t build_commit_timestamp = 0;
  /** Commit Hash from `buildinfo`. */
  char build_hash[16] = {};
  /** Indicate the #Main.filepath (file) is the recovered one. */
  bool recovered = false;
  /** All current ID's exist in the last memfile undo step. */
  bool is_memfile_undo_written = false;
  /**
   * An ID needs its data to be flushed back.
   * use "needs_flush_to_id" in edit data to flag data which needs updating.
   */
  bool is_memfile_undo_flush_needed = false;
  /**
   * Indicates that next memfile undo step should not allow reusing old bmain when re-read, but
   * instead do a complete full re-read/update from stored memfile.
   */
  bool use_memfile_full_barrier = false;

  /**
   * When linking, disallow creation of new data-blocks.
   * Make sure we don't do this by accident, see #76738.
   */
  bool is_locked_for_linking = false;

  /**
   * When set, indicates that an unrecoverable error/data corruption was detected.
   * Should only be set by readfile code, and used by upper-level code (typically #setup_app_data)
   * to cancel a file reading operation.
   */
  bool is_read_invalid = false;

  /**
   * True if this main is the 'GMAIN' of current Blender.
   *
   * \note There should always be only one global main, all others generated temporarily for
   * various data management process must have this property set to false..
   */
  bool is_global_main = false;

  /**
   * True if the Action Slot-to-ID mapping is dirty.
   *
   * If this flag is set, the next call to `animrig::Slot::users(bmain)` and related functions
   * will trigger a rebuild of the Slot-to-ID mapping. Since constructing this mapping requires
   * a full scan of the animatable IDs in this `Main` anyway, it is kept as a flag here.
   *
   * \note This flag should not be set directly. Use #animrig::Slot::users_invalidate() instead.
   * That way the handling of this flag is limited to the code in #animrig::Slot.
   *
   * \see `blender::animrig::Slot::users_invalidate(Main &bmain)`
   */
  bool is_action_slot_to_id_map_dirty = false;

  /**
   * The blend-file thumbnail. If set, it will show as image preview of the blend-file in the
   * system's file-browser.
   */
  BlendThumbnail *blen_thumb = nullptr;

  /**
   * The library matching the current Main.
   *
   * Typically `nullptr` (for the `G_MAIN` representing the currently opened blend-file).
   *
   * Mainly set and used during the blend-file read/write process when 'split' Mains are used to
   * isolate and process all linked IDs from a single library.
   */
  Library *curlib = nullptr;

  /*
   * Colorspace information for this file.
   */
  MainColorspace colorspace;

  /* List bases for all ID types, containing all IDs for the current #Main. */

  ListBase scenes = {};
  ListBase libraries = {};
  ListBase objects = {};
  ListBase meshes = {};
  ListBase curves = {};
  ListBase metaballs = {};
  ListBase materials = {};
  ListBase textures = {};
  ListBase images = {};
  ListBase lattices = {};
  ListBase lights = {};
  ListBase cameras = {};
  ListBase shapekeys = {};
  ListBase worlds = {};
  ListBase screens = {};
  ListBase fonts = {};
  ListBase texts = {};
  ListBase speakers = {};
  ListBase lightprobes = {};
  ListBase sounds = {};
  ListBase collections = {};
  ListBase armatures = {};
  ListBase actions = {};
  ListBase nodetrees = {};
  ListBase brushes = {};
  ListBase particles = {};
  ListBase palettes = {};
  ListBase paintcurves = {};
  /** Singleton (exception). */
  ListBase wm = {};
  /** Legacy Grease Pencil. */
  ListBase gpencils = {};
  ListBase grease_pencils = {};
  ListBase movieclips = {};
  ListBase masks = {};
  ListBase linestyles = {};
  ListBase cachefiles = {};
  ListBase workspaces = {};
  /**
   * \note The name `hair_curves` is chosen to be different than `curves`,
   * but they are generic curve data-blocks, not just for hair.
   */
  ListBase hair_curves = {};
  ListBase pointclouds = {};
  ListBase volumes = {};

  /**
   * Must be generated, used and freed by same code - never assume this is valid data unless you
   * know when, who and how it was created.
   * Used by code doing a lot of remapping etc. at once to speed things up.
   */
  MainIDRelations *relations = nullptr;

  /** IDMap of IDs. Currently used when reading (expanding) libraries. */
  IDNameLib_Map *id_map = nullptr;

  /** Used for efficient calculations of unique names. */
  UniqueName_Map *name_map = nullptr;

  /**
   * Used for efficient calculations of unique names. Covers all names in current Main, including
   * linked data ones.
   */
  UniqueName_Map *name_map_global = nullptr;

  MainLock *lock = nullptr;

  /* Constructors and destructors. */
  Main();
  ~Main();
};

/**
 * Create a new Main data-base.
 *
 * \note Always generate a non-global Main, use #BKE_blender_globals_main_replace to put a newly
 * created one in `G_MAIN`.
 */
Main *BKE_main_new();
/**
 * Make given \a bmain empty again, and free all runtime mappings.
 *
 * This is similar to deleting and re-creating the Main, however the internal #Main::lock is kept
 * unchanged, and the #Main::is_global_main flag is not reset to `true` either.
 *
 * \note Unlike #BKE_main_free, only process the given \a bmain, without handling any potential
 * other linked Main.
 */
void BKE_main_clear(Main &bmain);
/**
 * Completely destroy the given \a bmain, and all its linked 'libraries' ones if any (all other
 * bmains, following the #Main.next chained list).
 */
void BKE_main_free(Main *bmain);

/** Struct packaging log/report info about a Main merge result. */
struct MainMergeReport {
  ReportList *reports = nullptr;

  /** Number of IDs from source Main that have been moved into destination Main. */
  int num_merged_ids = 0;
  /**
   * Number of (non-library) IDs from source Main that were expected
   * to have a matching ID in destination Main, but did not.
   * These have not been moved, and their usages have been remapped to null.
   */
  int num_unknown_ids = 0;
  /**
   * Number of (non-library) IDs from source Main that already had a matching ID
   * in destination Main.
   */
  int num_remapped_ids = 0;
  /**
   * Number of Library IDs from source Main that already had a matching Library ID
   * in destination Main.
   */
  int num_remapped_libraries = 0;
};

/**
 * Merge the content of `bmain_src` into `bmain_dst`.
 *
 * In case of collision (ID from same library with same name), the existing ID in `bmain_dst` is
 * kept, the one from `bmain_src` is left in its original Main, and its usages in `bmain_dst` (from
 * newly moved-in IDs) are remapped to its matching counterpart already in `bmain_dst`.
 *
 * Libraries are also de-duplicated, based on their absolute filepath, and remapped accordingly.
 * Note that local IDs in source Main always remain local IDs in destination Main.
 *
 * In case some source IDs are linked data from the blendfile of `bmain_dst`, they are never moved.
 * If a matching destination local ID is found, their usage get remapped as expected, otherwise
 * they are dropped, their usages are remapped to null, and a warning is printed.
 *
 * Since `bmain_src` is either empty or contains left-over IDs with (likely) invalid ID
 * relationships and other potential issues after the merge, it is always freed.
 */
void BKE_main_merge(Main *bmain_dst, Main **r_bmain_src, MainMergeReport &reports);

/**
 * Check whether given `bmain` is empty or contains some IDs.
 */
bool BKE_main_is_empty(Main *bmain);

/**
 * Check whether the bmain has issues, e.g. for reporting in the status bar.
 */
bool BKE_main_has_issues(const Main *bmain);

/**
 * Check whether user confirmation should be required when overwriting this `bmain` into its source
 * blendfile.
 */
bool BKE_main_needs_overwrite_confirm(const Main *bmain);

void BKE_main_lock(Main *bmain);
void BKE_main_unlock(Main *bmain);

/** Generate the mappings between used IDs and their users, and vice-versa. */
void BKE_main_relations_create(Main *bmain, short flag);
void BKE_main_relations_free(Main *bmain);
/** Set or clear given `tag` in all relation entries of given `bmain`. */
void BKE_main_relations_tag_set(Main *bmain, eMainIDRelationsEntryTags tag, bool value);

/**
 * Create a #GSet storing all IDs present in given \a bmain, by their pointers.
 *
 * \param gset: If not NULL, given GSet will be extended with IDs from given \a bmain,
 * instead of creating a new one.
 */
GSet *BKE_main_gset_create(Main *bmain, GSet *gset);

/* Temporary runtime API to allow re-using local (already appended)
 * IDs instead of appending a new copy again. */

struct MainLibraryWeakReferenceMap;

/**
 * Generate a mapping between 'library path' of an ID
 * (as a pair (relative blend file path, id name)), and a current local ID, if any.
 *
 * This uses the information stored in `ID.library_weak_reference`.
 */
MainLibraryWeakReferenceMap *BKE_main_library_weak_reference_create(Main *bmain) ATTR_NONNULL();
/**
 * Destroy the data generated by #BKE_main_library_weak_reference_create.
 */
void BKE_main_library_weak_reference_destroy(
    MainLibraryWeakReferenceMap *library_weak_reference_mapping) ATTR_NONNULL();
/**
 * Search for a local ID matching the given linked ID reference.
 *
 * \param library_weak_reference_mapping: the mapping data generated by
 * #BKE_main_library_weak_reference_create.
 * \param library_filepath: the path of a blend file library (relative to current working one).
 * \param library_id_name: the full ID name, including the leading two chars encoding the ID
 * type.
 */
ID *BKE_main_library_weak_reference_search_item(
    MainLibraryWeakReferenceMap *library_weak_reference_mapping,
    const char *library_filepath,
    const char *library_id_name) ATTR_NONNULL();
/**
 * Add the given ID weak library reference to given local ID and the runtime mapping.
 *
 * \param library_weak_reference_mapping: the mapping data generated by
 * #BKE_main_library_weak_reference_create.
 * \param library_filepath: the path of a blend file library (relative to current working one).
 * \param library_id_name: the full ID name, including the leading two chars encoding the ID type.
 * \param new_id: New local ID matching given weak reference.
 */
void BKE_main_library_weak_reference_add_item(
    MainLibraryWeakReferenceMap *library_weak_reference_mapping,
    const char *library_filepath,
    const char *library_id_name,
    ID *new_id) ATTR_NONNULL();
/**
 * Update the status of the given ID weak library reference in current local IDs and the runtime
 * mapping.
 *
 * This effectively transfers the 'ownership' of the given weak reference from `old_id` to
 * `new_id`.
 *
 * \param library_weak_reference_mapping: the mapping data generated by
 * #BKE_main_library_weak_reference_create.
 * \param library_filepath: the path of a blend file library (relative to current working one).
 * \param library_id_name: the full ID name, including the leading two chars encoding the ID type.
 * \param old_id: Existing local ID matching given weak reference.
 * \param new_id: New local ID matching given weak reference.
 */
void BKE_main_library_weak_reference_update_item(
    MainLibraryWeakReferenceMap *library_weak_reference_mapping,
    const char *library_filepath,
    const char *library_id_name,
    ID *old_id,
    ID *new_id) ATTR_NONNULL();
/**
 * Remove the given ID weak library reference from the given local ID and the runtime mapping.
 *
 * \param library_weak_reference_mapping: the mapping data generated by
 * #BKE_main_library_weak_reference_create.
 * \param library_filepath: the path of a blend file library (relative to current working one).
 * \param library_id_name: the full ID name, including the leading two chars encoding the ID type.
 * \param old_id: Existing local ID matching given weak reference.
 */
void BKE_main_library_weak_reference_remove_item(
    MainLibraryWeakReferenceMap *library_weak_reference_mapping,
    const char *library_filepath,
    const char *library_id_name,
    ID *old_id) ATTR_NONNULL();

/**
 * Find local ID with weak library reference matching library and ID name.
 * For cases where creating a full MainLibraryWeakReferenceMap is unnecessary.
 */
ID *BKE_main_library_weak_reference_find(Main *bmain,
                                         const char *library_filepath,
                                         const char *library_id_name);

/**
 * Add library weak reference to ID, referencing the specified library and ID name.
 * For cases where creating a full MainLibraryWeakReferenceMap is unnecessary.
 */
void BKE_main_library_weak_reference_add(ID *local_id,
                                         const char *library_filepath,
                                         const char *library_id_name);

/* *** Generic utils to loop over whole Main database. *** */

#define FOREACH_MAIN_LISTBASE_ID_BEGIN(_lb, _id) \
  { \
    ID *_id_next = static_cast<ID *>((_lb)->first); \
    for ((_id) = _id_next; (_id) != nullptr; (_id) = _id_next) { \
      _id_next = static_cast<ID *>((_id)->next);

#define FOREACH_MAIN_LISTBASE_ID_END \
  } \
  } \
  ((void)0)

#define FOREACH_MAIN_LISTBASE_BEGIN(_bmain, _lb) \
  { \
    MainListsArray _lbarray = BKE_main_lists_get(*(_bmain)); \
    size_t _i = _lbarray.size(); \
    while (_i--) { \
      (_lb) = _lbarray[_i];

#define FOREACH_MAIN_LISTBASE_END \
  } \
  } \
  ((void)0)

/**
 * Top level `foreach`-like macro allowing to loop over all IDs in a given #Main data-base.
 *
 * NOTE: Order tries to go from 'user IDs' to 'used IDs' (e.g. collections will be processed
 * before objects, which will be processed before obdata types, etc.).
 *
 * WARNING: DO NOT use break statement with that macro, use #FOREACH_MAIN_LISTBASE and
 * #FOREACH_MAIN_LISTBASE_ID instead if you need that kind of control flow. */
#define FOREACH_MAIN_ID_BEGIN(_bmain, _id) \
  { \
    ListBase *_lb; \
    FOREACH_MAIN_LISTBASE_BEGIN ((_bmain), _lb) { \
      FOREACH_MAIN_LISTBASE_ID_BEGIN (_lb, (_id))

#define FOREACH_MAIN_ID_END \
  FOREACH_MAIN_LISTBASE_ID_END; \
  } \
  FOREACH_MAIN_LISTBASE_END; \
  } \
  ((void)0)

/**
 * Generates a raw .blend file thumbnail data from a raw image buffer.
 *
 * \param bmain: If not NULL, also store generated data in this Main.
 * \param rect: RGBA image buffer.
 * \param size: The size of `rect`.
 * \return The generated .blend file raw thumbnail data.
 */
BlendThumbnail *BKE_main_thumbnail_from_buffer(Main *bmain,
                                               const uint8_t *rect,
                                               const int size[2]);

/**
 * Generates a raw .blend file thumbnail data from given image.
 *
 * \param bmain: If not NULL, also store generated data in this Main.
 * \param img: ImBuf image to generate thumbnail data from.
 * \return The generated .blend file raw thumbnail data.
 */
BlendThumbnail *BKE_main_thumbnail_from_imbuf(Main *bmain, ImBuf *img);
/**
 * Generates an image from raw .blend file thumbnail \a data.
 *
 * \param bmain: Use this bmain->blen_thumb data if given \a data is NULL.
 * \param data: Raw .blend file thumbnail data.
 * \return An ImBuf from given data, or NULL if invalid.
 */
ImBuf *BKE_main_thumbnail_to_imbuf(Main *bmain, BlendThumbnail *data);
/**
 * Generates an empty (black) thumbnail for given Main.
 */
void BKE_main_thumbnail_create(Main *bmain);

/**
 * Return file-path of given \a main.
 */
const char *BKE_main_blendfile_path(const Main *bmain) ATTR_NONNULL();
/**
 * Return file-path of global main #G_MAIN.
 *
 * \warning Usage is not recommended,
 * you should always try to get a valid Main pointer from context.
 */
const char *BKE_main_blendfile_path_from_global();
/**
 * Return the absolute file-path of a library.
 */
const char *BKE_main_blendfile_path_from_library(const Library &library);

/**
 * \return A pointer to the \a ListBase of given \a bmain for requested \a type ID type.
 */
ListBase *which_libbase(Main *bmain, short type);

/** Subtracting 1, because #INDEX_ID_NULL is ignored here. */
using MainListsArray = std::array<ListBase *, INDEX_ID_MAX - 1>;

/**
 * Returns the pointers to all the #ListBase structs in given `bmain`.
 *
 * This is useful for generic traversal of all the blocks in a #Main (by traversing all the lists
 * in turn), without worrying about block types.
 *
 * \note The order of each ID type #ListBase in the array is determined by the `INDEX_ID_<IDTYPE>`
 * enum definitions in `DNA_ID.h`. See also the #FOREACH_MAIN_ID_BEGIN macro in `BKE_main.hh`
 */
MainListsArray BKE_main_lists_get(Main &bmain);

#define MAIN_VERSION_FILE_ATLEAST(main, ver, subver) \
  ((main)->versionfile > (ver) || \
   ((main)->versionfile == (ver) && (main)->subversionfile >= (subver)))

#define MAIN_VERSION_FILE_OLDER(main, ver, subver) \
  ((main)->versionfile < (ver) || \
   ((main)->versionfile == (ver) && (main)->subversionfile < (subver)))

#define MAIN_VERSION_FILE_OLDER_OR_EQUAL(main, ver, subver) \
  ((main)->versionfile < (ver) || \
   ((main)->versionfile == (ver) && (main)->subversionfile <= (subver)))

/* NOTE: in case versionfile is 0, this check is invalid, always return false then. This happens
 * typically when a library is missing, by definition its data (placeholder IDs) does not need
 * versionning anyway then. */
#define LIBRARY_VERSION_FILE_ATLEAST(lib, ver, subver) \
  ((lib)->runtime->versionfile == 0 || (lib)->runtime->versionfile > (ver) || \
   ((lib)->runtime->versionfile == (ver) && (lib)->runtime->subversionfile >= (subver)))

/**
 * The size of thumbnails (optionally) stored in the `.blend` files header.
 *
 * NOTE(@ideasman42): This is kept small as it's stored uncompressed in the `.blend` file,
 * where a larger size would increase the size of every `.blend` file unreasonably.
 * If we wanted to increase the size, we'd want to use compression (JPEG or similar).
 */
#define BLEN_THUMB_SIZE 128

#define BLEN_THUMB_MEMSIZE(_x, _y) \
  (sizeof(BlendThumbnail) + ((size_t)(_x) * (size_t)(_y)) * sizeof(int))
/** Protect against buffer overflow vulnerability & negative sizes. */
#define BLEN_THUMB_MEMSIZE_IS_VALID(_x, _y) \
  (((_x) > 0 && (_y) > 0) && ((uint64_t)(_x) * (uint64_t)(_y) < (SIZE_MAX / (sizeof(int) * 4))))

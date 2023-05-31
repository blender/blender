/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */
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

#include "DNA_listBase.h"

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BLI_mempool;
struct BlendThumbnail;
struct GHash;
struct GSet;
struct IDNameLib_Map;
struct ImBuf;
struct Library;
struct MainLock;
struct UniqueName_Map;

/**
 * Blender thumbnail, as written to the `.blend` file (width, height, and data as char RGBA).
 */
typedef struct BlendThumbnail {
  int width, height;
  /** Pixel data, RGBA (repeated): `sizeof(char[4]) * width * height`. */
  char rect[0];
} BlendThumbnail;

/** Structs caching relations between data-blocks in a given Main. */
typedef struct MainIDRelationsEntryItem {
  struct MainIDRelationsEntryItem *next;

  union {
    /* For `from_ids` list, a user of the hashed ID. */
    struct ID *from;
    /* For `to_ids` list, an ID used by the hashed ID. */
    struct ID **to;
  } id_pointer;
  /* Session uuid of the `id_pointer`. */
  uint session_uuid;

  int usage_flag; /* Using IDWALK_ enums, defined in BKE_lib_query.h */
} MainIDRelationsEntryItem;

typedef struct MainIDRelationsEntry {
  /* Linked list of IDs using that ID. */
  struct MainIDRelationsEntryItem *from_ids;
  /* Linked list of IDs used by that ID. */
  struct MainIDRelationsEntryItem *to_ids;

  /* Session uuid of the ID matching that entry. */
  uint session_uuid;

  /* Runtime tags, users should ensure those are reset after usage. */
  uint tags;
} MainIDRelationsEntry;

/** #MainIDRelationsEntry.tags */
typedef enum eMainIDRelationsEntryTags {
  /* Generic tag marking the entry as to be processed. */
  MAINIDRELATIONS_ENTRY_TAGS_DOIT = 1 << 0,

  /* Generic tag marking the entry as processed in the `to` direction (i.e. the IDs used by this
   * item have been processed). */
  MAINIDRELATIONS_ENTRY_TAGS_PROCESSED_TO = 1 << 4,
  /* Generic tag marking the entry as processed in the `from` direction (i.e. the IDs using this
   * item have been processed). */
  MAINIDRELATIONS_ENTRY_TAGS_PROCESSED_FROM = 1 << 5,
  /* Generic tag marking the entry as processed. */
  MAINIDRELATIONS_ENTRY_TAGS_PROCESSED = MAINIDRELATIONS_ENTRY_TAGS_PROCESSED_TO |
                                         MAINIDRELATIONS_ENTRY_TAGS_PROCESSED_FROM,

  /* Generic tag marking the entry as being processed in the `to` direction (i.e. the IDs used by
   * this item are being processed). Useful for dependency loops detection and handling. */
  MAINIDRELATIONS_ENTRY_TAGS_INPROGRESS_TO = 1 << 8,
  /* Generic tag marking the entry as being processed in the `from` direction (i.e. the IDs using
   * this item are being processed). Useful for dependency loops detection and handling. */
  MAINIDRELATIONS_ENTRY_TAGS_INPROGRESS_FROM = 1 << 9,
  /* Generic tag marking the entry as being processed. Useful for dependency loops detection and
   * handling. */
  MAINIDRELATIONS_ENTRY_TAGS_INPROGRESS = MAINIDRELATIONS_ENTRY_TAGS_INPROGRESS_TO |
                                          MAINIDRELATIONS_ENTRY_TAGS_INPROGRESS_FROM,
} eMainIDRelationsEntryTags;

typedef struct MainIDRelations {
  /* Mapping from an ID pointer to all of its parents (IDs using it) and children (IDs it uses).
   * Values are `MainIDRelationsEntry` pointers. */
  struct GHash *relations_from_pointers;
  /* NOTE: we could add more mappings when needed (e.g. from session uuid?). */

  short flag;

  /* Private... */
  struct BLI_mempool *entry_items_pool;
} MainIDRelations;

enum {
  /* Those bmain relations include pointers/usages from editors. */
  MAINIDRELATIONS_INCLUDE_UI = 1 << 0,
};

typedef struct Main {
  struct Main *next, *prev;
  /**
   * The file-path of this blend file, an empty string indicates an unsaved file.
   *
   * \note For the current loaded blend file this path must be absolute & normalized.
   * This prevents redundant leading slashes or current-working-directory relative paths
   * from causing problems with absolute/relative conversion which relies on this `filepath`
   * being absolute. See #BLI_path_canonicalize_native.
   *
   * This rule is not strictly enforced as in some cases loading a #Main is performed
   * to read data temporarily (preferences & startup) for e.g.
   * where the `filepath` is not persistent or used as a basis for other paths.
   */
  char filepath[1024];               /* 1024 = FILE_MAX */
  short versionfile, subversionfile; /* see BLENDER_FILE_VERSION, BLENDER_FILE_SUBVERSION */
  short minversionfile, minsubversionfile;
  /** Commit timestamp from `buildinfo`. */
  uint64_t build_commit_timestamp;
  /** Commit Hash from `buildinfo`. */
  char build_hash[16];
  /** Indicate the #Main.filepath (file) is the recovered one. */
  bool recovered;
  /** All current ID's exist in the last memfile undo step. */
  bool is_memfile_undo_written;
  /**
   * An ID needs its data to be flushed back.
   * use "needs_flush_to_id" in edit data to flag data which needs updating.
   */
  bool is_memfile_undo_flush_needed;
  /**
   * Indicates that next memfile undo step should not allow reusing old bmain when re-read, but
   * instead do a complete full re-read/update from stored memfile.
   */
  bool use_memfile_full_barrier;

  /**
   * When linking, disallow creation of new data-blocks.
   * Make sure we don't do this by accident, see #76738.
   */
  bool is_locked_for_linking;

  /**
   * When set, indicates that an unrecoverable error/data corruption was detected.
   * Should only be set by readfile code, and used by upper-level code (typically #setup_app_data)
   * to cancel a file reading operation.
   */
  bool is_read_invalid;

  /**
   * True if this main is the 'GMAIN' of current Blender.
   *
   * \note There should always be only one global main, all others generated temporarily for
   * various data management process must have this property set to false..
   */
  bool is_global_main;

  BlendThumbnail *blen_thumb;

  struct Library *curlib;
  ListBase scenes;
  ListBase libraries;
  ListBase objects;
  ListBase meshes;
  ListBase curves;
  ListBase metaballs;
  ListBase materials;
  ListBase textures;
  ListBase images;
  ListBase lattices;
  ListBase lights;
  ListBase cameras;
  ListBase ipo; /* Deprecated (only for versioning). */
  ListBase shapekeys;
  ListBase worlds;
  ListBase screens;
  ListBase fonts;
  ListBase texts;
  ListBase speakers;
  ListBase lightprobes;
  ListBase sounds;
  ListBase collections;
  ListBase armatures;
  ListBase actions;
  ListBase nodetrees;
  ListBase brushes;
  ListBase particles;
  ListBase palettes;
  ListBase paintcurves;
  ListBase wm;       /* Singleton (exception). */
  ListBase gpencils; /* Legacy Grease Pencil. */
  ListBase grease_pencils;
  ListBase movieclips;
  ListBase masks;
  ListBase linestyles;
  ListBase cachefiles;
  ListBase workspaces;
  /**
   * \note The name `hair_curves` is chosen to be different than `curves`,
   * but they are generic curve data-blocks, not just for hair.
   */
  ListBase hair_curves;
  ListBase pointclouds;
  ListBase volumes;
  ListBase simulations;

  /**
   * Must be generated, used and freed by same code - never assume this is valid data unless you
   * know when, who and how it was created.
   * Used by code doing a lot of remapping etc. at once to speed things up.
   */
  struct MainIDRelations *relations;

  /** IDMap of IDs. Currently used when reading (expanding) libraries. */
  struct IDNameLib_Map *id_map;

  /** Used for efficient calculations of unique names. */
  struct UniqueName_Map *name_map;

  struct MainLock *lock;
} Main;

/**
 * Create a new Main data-base.
 *
 * \note Always generate a non-global Main, use #BKE_blender_globals_main_replace to put a newly
 * created one in `G_MAIN`.
 */
struct Main *BKE_main_new(void);
void BKE_main_free(struct Main *mainvar);

/**
 * Check whether given `bmain` is empty or contains some IDs.
 */
bool BKE_main_is_empty(struct Main *bmain);

void BKE_main_lock(struct Main *bmain);
void BKE_main_unlock(struct Main *bmain);

/** Generate the mappings between used IDs and their users, and vice-versa. */
void BKE_main_relations_create(struct Main *bmain, short flag);
void BKE_main_relations_free(struct Main *bmain);
/** Set or clear given `tag` in all relation entries of given `bmain`. */
void BKE_main_relations_tag_set(struct Main *bmain, eMainIDRelationsEntryTags tag, bool value);

/**
 * Create a #GSet storing all IDs present in given \a bmain, by their pointers.
 *
 * \param gset: If not NULL, given GSet will be extended with IDs from given \a bmain,
 * instead of creating a new one.
 */
struct GSet *BKE_main_gset_create(struct Main *bmain, struct GSet *gset);

/* Temporary runtime API to allow re-using local (already appended)
 * IDs instead of appending a new copy again. */

/**
 * Generate a mapping between 'library path' of an ID
 * (as a pair (relative blend file path, id name)), and a current local ID, if any.
 *
 * This uses the information stored in `ID.library_weak_reference`.
 */
struct GHash *BKE_main_library_weak_reference_create(struct Main *bmain) ATTR_NONNULL();
/**
 * Destroy the data generated by #BKE_main_library_weak_reference_create.
 */
void BKE_main_library_weak_reference_destroy(struct GHash *library_weak_reference_mapping)
    ATTR_NONNULL();
/**
 * Search for a local ID matching the given linked ID reference.
 *
 * \param library_weak_reference_mapping: the mapping data generated by
 * #BKE_main_library_weak_reference_create.
 * \param library_filepath: the path of a blend file library (relative to current working one).
 * \param library_id_name: the full ID name, including the leading two chars encoding the ID
 * type.
 */
struct ID *BKE_main_library_weak_reference_search_item(
    struct GHash *library_weak_reference_mapping,
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
void BKE_main_library_weak_reference_add_item(struct GHash *library_weak_reference_mapping,
                                              const char *library_filepath,
                                              const char *library_id_name,
                                              struct ID *new_id) ATTR_NONNULL();
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
void BKE_main_library_weak_reference_update_item(struct GHash *library_weak_reference_mapping,
                                                 const char *library_filepath,
                                                 const char *library_id_name,
                                                 struct ID *old_id,
                                                 struct ID *new_id) ATTR_NONNULL();
/**
 * Remove the given ID weak library reference from the given local ID and the runtime mapping.
 *
 * \param library_weak_reference_mapping: the mapping data generated by
 * #BKE_main_library_weak_reference_create.
 * \param library_filepath: the path of a blend file library (relative to current working one).
 * \param library_id_name: the full ID name, including the leading two chars encoding the ID type.
 * \param old_id: Existing local ID matching given weak reference.
 */
void BKE_main_library_weak_reference_remove_item(struct GHash *library_weak_reference_mapping,
                                                 const char *library_filepath,
                                                 const char *library_id_name,
                                                 struct ID *old_id) ATTR_NONNULL();

/* *** Generic utils to loop over whole Main database. *** */

#define FOREACH_MAIN_LISTBASE_ID_BEGIN(_lb, _id) \
  { \
    ID *_id_next = (ID *)(_lb)->first; \
    for ((_id) = _id_next; (_id) != NULL; (_id) = _id_next) { \
      _id_next = (ID *)(_id)->next;

#define FOREACH_MAIN_LISTBASE_ID_END \
  } \
  } \
  ((void)0)

#define FOREACH_MAIN_LISTBASE_BEGIN(_bmain, _lb) \
  { \
    ListBase *_lbarray[INDEX_ID_MAX]; \
    int _i = set_listbasepointers((_bmain), _lbarray); \
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
 * Generates a raw .blend file thumbnail data from given image.
 *
 * \param bmain: If not NULL, also store generated data in this Main.
 * \param img: ImBuf image to generate thumbnail data from.
 * \return The generated .blend file raw thumbnail data.
 */
struct BlendThumbnail *BKE_main_thumbnail_from_imbuf(struct Main *bmain, struct ImBuf *img);
/**
 * Generates an image from raw .blend file thumbnail \a data.
 *
 * \param bmain: Use this bmain->blen_thumb data if given \a data is NULL.
 * \param data: Raw .blend file thumbnail data.
 * \return An ImBuf from given data, or NULL if invalid.
 */
struct ImBuf *BKE_main_thumbnail_to_imbuf(struct Main *bmain, struct BlendThumbnail *data);
/**
 * Generates an empty (black) thumbnail for given Main.
 */
void BKE_main_thumbnail_create(struct Main *bmain);

/**
 * Return file-path of given \a main.
 */
const char *BKE_main_blendfile_path(const struct Main *bmain) ATTR_NONNULL();
/**
 * Return file-path of global main #G_MAIN.
 *
 * \warning Usage is not recommended,
 * you should always try to get a valid Main pointer from context.
 */
const char *BKE_main_blendfile_path_from_global(void);

/**
 * \return A pointer to the \a ListBase of given \a bmain for requested \a type ID type.
 */
struct ListBase *which_libbase(struct Main *bmain, short type);

//#define INDEX_ID_MAX 41
/**
 * Put the pointers to all the #ListBase structs in given `bmain` into the `*lb[INDEX_ID_MAX]`
 * array, and return the number of those for convenience.
 *
 * This is useful for generic traversal of all the blocks in a #Main (by traversing all the lists
 * in turn), without worrying about block types.
 *
 * \param lb: Array of lists #INDEX_ID_MAX in length.
 *
 * \note The order of each ID type #ListBase in the array is determined by the `INDEX_ID_<IDTYPE>`
 * enum definitions in `DNA_ID.h`. See also the #FOREACH_MAIN_ID_BEGIN macro in `BKE_main.h`
 */
int set_listbasepointers(struct Main *main, struct ListBase *lb[]);

#define MAIN_VERSION_ATLEAST(main, ver, subver) \
  ((main)->versionfile > (ver) || \
   ((main)->versionfile == (ver) && (main)->subversionfile >= (subver)))

#define MAIN_VERSION_OLDER(main, ver, subver) \
  ((main)->versionfile < (ver) || \
   ((main)->versionfile == (ver) && (main)->subversionfile < (subver)))

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

#ifdef __cplusplus
}
#endif

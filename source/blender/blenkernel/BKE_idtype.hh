/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 *
 * ID type structure, helping to factorize common operations and data for all data-block types.
 */

#include <optional>

#include "BLI_color_types.hh"
#include "BLI_function_ref.hh"
#include "BLI_implicit_sharing_ptr.hh"
#include "BLI_sys_types.h"

struct AssetTypeInfo;
struct BPathForeachPathData;
struct BlendDataReader;
struct BlendLibReader;
struct BlendWriter;
struct ID;
struct Library;
struct LibraryForeachIDData;
struct Main;

/** IDTypeInfo.flags. */
enum {
  /** Indicates that the given IDType does not support copying. */
  IDTYPE_FLAGS_NO_COPY = 1 << 0,
  /** Indicates that the given IDType does not support linking/appending from a library file. */
  IDTYPE_FLAGS_NO_LIBLINKING = 1 << 1,
  /**
   * Indicates that the given IDType should not be directly linked from a library file,
   * but may be appended.
   * NOTE: Mutually exclusive with `IDTYPE_FLAGS_NO_LIBLINKING`.
   */
  IDTYPE_FLAGS_ONLY_APPEND = 1 << 2,
  /**
   * Allow to re-use an existing local ID with matching weak library reference
   * instead of creating a new copy of it, when appending.
   * See also #LibraryWeakReference in `DNA_ID.h`.
   */
  IDTYPE_FLAGS_APPEND_IS_REUSABLE = 1 << 3,
  /** Indicates that the given IDType does not have animation data. */
  IDTYPE_FLAGS_NO_ANIMDATA = 1 << 4,
  /**
   * Indicates that the given IDType is not handled through memfile (aka global) undo.
   *
   * \note This currently only affect local data-blocks.
   *
   * \note Current readfile undo code expects these data-blocks to not be used by any 'regular'
   * data-blocks.
   */
  IDTYPE_FLAGS_NO_MEMFILE_UNDO = 1 << 5,
  /**
   * Indicates that the given IDType is considered as unused.
   *
   * This is used for some 'root' ID types which typically do not have any actual user (WM.
   * Scene...). It prevents e.g. their deletion through the 'Purge' operation.
   *
   * \note This applies to local IDs. Linked data should essentially ignore this flag. In practice,
   * currently, only the Scene ID can be linked among the `never unused` types.
   *
   * \note The implementation of the expected behaviors related to this characteristic is somewhat
   * fragile and inconsistent currently. In most case though, code is expected to ensure that such
   * IDs have at least an 'extra user' (#ID_TAG_EXTRAUSER).
   */
  IDTYPE_FLAGS_NEVER_UNUSED = 1 << 6,
};

struct IDCacheKey {
  /** The session UID of the ID owning the cached data. */
  unsigned int id_session_uid;
  /**
   * Value uniquely identifying the cache within its ID.
   * Typically the offset of its member in the data-block struct, but can be anything.
   */
  size_t identifier;
};

uint BKE_idtype_cache_key_hash(const void *key_v);
bool BKE_idtype_cache_key_cmp(const void *key_a_v, const void *key_b_v);

/* ********** Prototypes for #IDTypeInfo callbacks. ********** */

using IDTypeInitDataFunction = void (*)(ID *id);

/** \param flag: Copying options (see BKE_lib_id.hh's LIB_ID_COPY_... flags for more). */
using IDTypeCopyDataFunction = void (*)(
    Main *bmain, std::optional<Library *> owner_library, ID *id_dst, const ID *id_src, int flag);

using IDTypeFreeDataFunction = void (*)(ID *id);

/** \param flags: See BKE_lib_id.hh's LIB_ID_MAKELOCAL_... flags. */
using IDTypeMakeLocalFunction = void (*)(Main *bmain, ID *id, int flags);

using IDTypeForeachIDFunction = void (*)(ID *id, LibraryForeachIDData *data);

enum eIDTypeInfoCacheCallbackFlags {
  /**
   * Indicates to the callback that cache may be stored in the .blend file,
   * so its pointer should not be cleared at read-time.
   */
  IDTYPE_CACHE_CB_FLAGS_PERSISTENT = 1 << 0,
};
using IDTypeForeachCacheFunctionCallback =
    void (*)(ID *id, const IDCacheKey *cache_key, void **cache_p, uint flags, void *user_data);
using IDTypeForeachCacheFunction = void (*)(ID *id,
                                            IDTypeForeachCacheFunctionCallback function_callback,
                                            void *user_data);

using IDTypeForeachPathFunction = void (*)(ID *id, BPathForeachPathData *bpath_data);

/* Foreach scene linear color can do either a single color, or an implicitly shared array
 * for geometry attributes. */
struct IDTypeForeachColorFunctionCallback {
  const blender::FunctionRef<void(float rgb[3])> single;
  const blender::FunctionRef<void(
      blender::ImplicitSharingPtr<> &sharing_info, blender::ColorGeometry4f *&data, size_t size)>
      implicit_sharing_array;
};
using IDTypeForeachColorFunction = void (*)(ID *id, const IDTypeForeachColorFunctionCallback &cb);

/**
 * Callback returning the address of the pointer to the owner ID,
 * for embedded (and Shape-key) ones.
 *
 * \param debug_relationship_assert: usually the owner <-> embedded relation pointers should be
 * fully valid, and can be asserted on. But in some cases, they are not (fully) valid, e.g when
 * copying an ID and all of its embedded data.
 */
using IDTypeEmbeddedOwnerPointerGetFunction = ID **(*)(ID * id, bool debug_relationship_assert);

using IDTypeBlendWriteFunction = void (*)(BlendWriter *writer, ID *id, const void *id_address);
using IDTypeBlendReadDataFunction = void (*)(BlendDataReader *reader, ID *id);
using IDTypeBlendReadAfterLiblinkFunction = void (*)(BlendLibReader *reader, ID *id);

using IDTypeBlendReadUndoPreserve = void (*)(BlendLibReader *reader, ID *id_new, ID *id_old);

using IDTypeLibOverrideApplyPost = void (*)(ID *id_dst, ID *id_src);

struct IDTypeInfo {
  /* ********** General IDType data. ********** */

  /**
   * Unique identifier of this type, either as a short or an array of two chars, see
   * DNA_ID_enums.h's ID_XX enums.
   */
  short id_code;
  /**
   * Bit-flag matching id_code, used for filtering (e.g. in file browser), see DNA_ID.h's
   * FILTER_ID_XX enums.
   */
  uint64_t id_filter;

  /**
   * Known types of ID dependencies.
   *
   * Used by #BKE_library_id_can_use_filter_id, together with additional runtime heuristics, to
   * generate a filter value containing only ID types that given ID could be using.
   */
  uint64_t dependencies_id_types;

  /**
   * Define the position of this data-block type in the virtual list of all data in a Main that is
   * returned by `BKE_main_lists_get()`.
   * Very important, this has to be unique and below INDEX_ID_MAX, see DNA_ID.h.
   */
  int main_listbase_index;

  /** Memory size of a data-block of that type. */
  size_t struct_size;

  /**
   * The user visible name for this data-block, also used as default name for a new data-block.
   *
   * \note: Also used for the 'filepath' ID type part when listing IDs in library blend-files
   * (`my_blendfile.blend/<IDType.name>/my_id_name`, e.g. `boat-v001.blend/Collection/PR-boat` for
   * the `GRPR-boat` Collection ID in `boat-v001.blend`).
   */
  const char *name;
  /** Plural version of the user-visible name. */
  const char *name_plural;
  /** Translation context to use for UI messages related to that type of data-block. */
  const char *translation_context;

  /** Generic info flags about that data-block type. */
  uint32_t flags;

  /**
   * Information and callbacks for assets, based on the type of asset.
   */
  AssetTypeInfo *asset_type_info;

  /* ********** ID management callbacks ********** */

  /**
   * Initialize a new, empty calloc'ed data-block. May be NULL if there is nothing to do.
   */
  IDTypeInitDataFunction init_data;

  /**
   * Copy the given data-block's data from source to destination.
   * May be NULL if mere memory-copy of the ID struct itself is enough.
   */
  IDTypeCopyDataFunction copy_data;

  /**
   * Free the data of the data-block (NOT the ID itself). May be NULL if there is nothing to do.
   */
  IDTypeFreeDataFunction free_data;

  /**
   * Make a linked data-block local. May be NULL if default behavior from
   * `BKE_lib_id_make_local_generic()` is enough.
   */
  IDTypeMakeLocalFunction make_local;

  /**
   * Called by `BKE_library_foreach_ID_link()` to apply a callback over all other ID usages (ID
   * pointers) of given data-block.
   */
  IDTypeForeachIDFunction foreach_id;

  /**
   * Iterator over all cache pointers of given ID.
   */
  IDTypeForeachCacheFunction foreach_cache;

  /**
   * Iterator over all file paths of given ID.
   */
  IDTypeForeachPathFunction foreach_path;

  /**
   * Iterator to edit all scene linear RGB colors of given ID.
   * Alpha should not be premultiplied in the RGB values.
   */
  IDTypeForeachColorFunction foreach_working_space_color;

  /**
   * For embedded IDs, return the address of the pointer to their owner ID.
   */
  IDTypeEmbeddedOwnerPointerGetFunction owner_pointer_get;

  /* ********** Callbacks for reading and writing .blend files. ********** */

  /**
   * Write all structs that should be saved in a .blend file.
   */
  IDTypeBlendWriteFunction blend_write;

  /**
   * Update pointers for all structs directly owned by this data block.
   */
  IDTypeBlendReadDataFunction blend_read_data;

  /**
   * Used to do some validation and/or complex processing on the ID after it has been fully read
   * and its ID pointers have been updated to valid values (lib linking process).
   *
   * Note that this is still called _before_ the `do_versions_after_linking` versioning code.
   */
  IDTypeBlendReadAfterLiblinkFunction blend_read_after_liblink;

  /**
   * Allow an ID type to preserve some of its data across (memfile) undo steps.
   *
   * \note Called from #setup_app_data when undoing or redoing a memfile step.
   *
   * \note In case the whole ID should be fully preserved across undo steps, it is better to flag
   * its type with `IDTYPE_FLAGS_NO_MEMFILE_UNDO`, since that flag allows more aggressive
   * optimizations in readfile code for memfile undo.
   */
  IDTypeBlendReadUndoPreserve blend_read_undo_preserve;

  /**
   * Called after library override operations have been applied.
   *
   * \note Currently needed for some update operation on point caches.
   */
  IDTypeLibOverrideApplyPost lib_override_apply_post;
};

/* ********** Declaration of each IDTypeInfo. ********** */

/* Those are defined in the respective BKE files. */
extern IDTypeInfo IDType_ID_SCE;
extern IDTypeInfo IDType_ID_LI;
extern IDTypeInfo IDType_ID_OB;
extern IDTypeInfo IDType_ID_ME;
extern IDTypeInfo IDType_ID_CU_LEGACY;
extern IDTypeInfo IDType_ID_MB;
extern IDTypeInfo IDType_ID_MA;
extern IDTypeInfo IDType_ID_TE;
extern IDTypeInfo IDType_ID_IM;
extern IDTypeInfo IDType_ID_LT;
extern IDTypeInfo IDType_ID_LA;
extern IDTypeInfo IDType_ID_CA;
extern IDTypeInfo IDType_ID_KE;
extern IDTypeInfo IDType_ID_WO;
extern IDTypeInfo IDType_ID_SCR;
extern IDTypeInfo IDType_ID_VF;
extern IDTypeInfo IDType_ID_TXT;
extern IDTypeInfo IDType_ID_SPK;
extern IDTypeInfo IDType_ID_SO;
extern IDTypeInfo IDType_ID_GR;
extern IDTypeInfo IDType_ID_AR;
extern IDTypeInfo IDType_ID_AC;
extern IDTypeInfo IDType_ID_NT;
extern IDTypeInfo IDType_ID_BR;
extern IDTypeInfo IDType_ID_PA;
extern IDTypeInfo IDType_ID_GD_LEGACY;
extern IDTypeInfo IDType_ID_WM;
extern IDTypeInfo IDType_ID_MC;
extern IDTypeInfo IDType_ID_MSK;
extern IDTypeInfo IDType_ID_LS;
extern IDTypeInfo IDType_ID_PAL;
extern IDTypeInfo IDType_ID_PC;
extern IDTypeInfo IDType_ID_CF;
extern IDTypeInfo IDType_ID_WS;
extern IDTypeInfo IDType_ID_LP;
extern IDTypeInfo IDType_ID_CV;
extern IDTypeInfo IDType_ID_PT;
extern IDTypeInfo IDType_ID_VO;
extern IDTypeInfo IDType_ID_GP;

/** Empty shell mostly, but needed for read code. */
extern IDTypeInfo IDType_ID_LINK_PLACEHOLDER;

/* ********** Helpers/Utils API. ********** */

/* Module initialization. */
void BKE_idtype_init();

/* General helpers. */
const IDTypeInfo *BKE_idtype_get_info_from_idtype_index(const int idtype_index);
const IDTypeInfo *BKE_idtype_get_info_from_idcode(short id_code);
const IDTypeInfo *BKE_idtype_get_info_from_id(const ID *id);

/**
 * Convert an \a idcode into a name.
 *
 * \param idcode: The code to convert.
 * \return A static string representing the name of the code.
 */
const char *BKE_idtype_idcode_to_name(short idcode);
/**
 * Convert an \a idcode into a name (plural).
 *
 * \param idcode: The code to convert.
 * \return A static string representing the name of the code.
 */
const char *BKE_idtype_idcode_to_name_plural(short idcode);
/**
 * Convert an \a idcode into its translations' context.
 *
 * \param idcode: The code to convert.
 * \return A static string representing the i18n context of the code.
 */
const char *BKE_idtype_idcode_to_translation_context(short idcode);

/**
 * Return if the ID code is a valid ID code.
 *
 * \param idcode: The code to check.
 * \return Boolean, 0 when invalid.
 */
bool BKE_idtype_idcode_is_valid(short idcode);

/**
 * Check if an ID type is linkable.
 *
 * \param idcode: The IDType code to check.
 * \return Boolean, false when non linkable, true otherwise.
 */
bool BKE_idtype_idcode_is_linkable(short idcode);
/**
 * Check if an ID type is only appendable.
 *
 * \param idcode: The IDType code to check.
 * \return Boolean, false when also linkable, true when only appendable.
 */
bool BKE_idtype_idcode_is_only_appendable(short idcode);
/**
 * Check if an ID type can try to reuse and existing matching local one when being appended again.
 *
 * \param idcode: The IDType code to check.
 * \return Boolean, false when it cannot be re-used, true otherwise.
 */
bool BKE_idtype_idcode_append_is_reusable(short idcode);
/* Macro currently, since any linkable IDtype should be localizable. */
#define BKE_idtype_idcode_is_localizable BKE_idtype_idcode_is_linkable

/**
 * Convert an ID-type name into an \a idcode (ie. #ID_SCE)
 *
 * \param idtype_name: The ID-type's "user visible name" to convert.
 * \return The \a idcode for the name, or 0 if invalid.
 */
short BKE_idtype_idcode_from_name(const char *idtype_name);

/**
 * Convert an \a idcode into an \a idtype_index (e.g. #ID_OB -> #INDEX_ID_OB).
 */
int BKE_idtype_idcode_to_index(short idcode);
/**
 * Convert an \a id_filter into an \a idtype_index (e.g. #FILTER_ID_OB -> #INDEX_ID_OB).
 */
int BKE_idtype_idfilter_to_index(uint64_t id_filter);

/**
 * Convert an \a idtype_index into an \a idcode (e.g. #INDEX_ID_OB -> #ID_OB).
 */
short BKE_idtype_index_to_idcode(int idtype_index);
/**
 * Convert an \a idtype_index into an \a idfilter (e.g. #INDEX_ID_OB -> #FILTER_ID_OB).
 */
uint64_t BKE_idtype_index_to_idfilter(int idtype_index);

/**
 * Convert an \a idcode into an \a idfilter (e.g. #ID_OB -> #FILTER_ID_OB).
 */
uint64_t BKE_idtype_idcode_to_idfilter(short idcode);
/**
 * Convert an \a idfilter into an \a idcode (e.g. #FILTER_ID_OB -> #ID_OB).
 */
short BKE_idtype_idfilter_to_idcode(uint64_t idfilter);

/**
 * Return an ID code and steps the index forward 1.
 *
 * \param index: start as 0.
 * \return the code, 0 when all codes have been returned.
 */
short BKE_idtype_idcode_iter_step(int *idtype_index);

/* Some helpers/wrappers around callbacks defined in #IDTypeInfo, dealing e.g. with embedded IDs.
 * XXX Ideally those would rather belong to #BKE_lib_id, but using callback function pointers makes
 * this hard to do properly if we want to avoid headers includes in headers. */

/**
 * Wrapper around #IDTypeInfo foreach_cache that also handles embedded IDs.
 */
void BKE_idtype_id_foreach_cache(ID *id,
                                 IDTypeForeachCacheFunctionCallback function_callback,
                                 void *user_data);

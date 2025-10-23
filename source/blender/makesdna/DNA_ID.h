/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 * \brief ID and Library types, which are fundamental for SDNA.
 */

#pragma once

#include "DNA_ID_enums.h"
#include "DNA_defs.h"
#include "DNA_listBase.h"

#include "BLI_assert.h"
#include "BLI_compiler_typecheck.h"

/** Workaround to forward-declare C++ type in C header. */
#ifdef __cplusplus
#  include <cstring>
#  include <type_traits>

namespace blender::bke::id {
struct ID_Runtime;
}
namespace blender::bke {
struct PreviewImageRuntime;
}
namespace blender::bke::idprop {
struct IDPropertyGroupChildrenSet;
}
namespace blender::bke::library {
struct LibraryRuntime;
}
using ID_RuntimeHandle = blender::bke::id::ID_Runtime;
using PreviewImageRuntimeHandle = blender::bke::PreviewImageRuntime;
using LibraryRuntimeHandle = blender::bke::library::LibraryRuntime;
using IDPropertyGroupChildrenSet = blender::bke::idprop::IDPropertyGroupChildrenSet;
#else
typedef struct PreviewImageRuntimeHandle PreviewImageRuntimeHandle;
typedef struct LibraryRuntimeHandle LibraryRuntimeHandle;
typedef struct IDPropertyGroupChildrenSet IDPropertyGroupChildrenSet;
typedef struct ID_RuntimeHandle ID_RuntimeHandle;
#endif

struct FileData;
struct GHash;
struct ID;
struct Library;
struct PackedFile;
struct UniqueName_Map;

typedef struct IDPropertyUIData {
  /** Tool-tip / property description pointer. Owned by the #IDProperty. */
  char *description;
  /** RNA `subtype`, used for every type except string properties (#PropertySubType). */
  int rna_subtype;

  char _pad[4];
} IDPropertyUIData;

/* DNA version of #EnumPropertyItem. */
typedef struct IDPropertyUIDataEnumItem {
  /* Unique identifier, used for string lookup. */
  char *identifier;
  /* UI name of the item. */
  char *name;
  /* Optional description. */
  char *description;
  /* Unique integer value, should never change. */
  int value;
  /* Optional icon. */
  int icon;
} IDPropertyUIDataEnumItem;

/* IDP_UI_DATA_TYPE_INT */
typedef struct IDPropertyUIDataInt {
  IDPropertyUIData base;
  int *default_array; /* Only for array properties. */
  int default_array_len;

  int min;
  int max;
  int soft_min;
  int soft_max;
  int step;
  int default_value;

  int enum_items_num;
  IDPropertyUIDataEnumItem *enum_items;
} IDPropertyUIDataInt;

/** For #IDP_UI_DATA_TYPE_BOOLEAN Use `int8_t` because DNA does not support `bool`. */
typedef struct IDPropertyUIDataBool {
  IDPropertyUIData base;
  int8_t *default_array; /* Only for array properties. */
  int default_array_len;
  char _pad[3];

  int8_t default_value;
} IDPropertyUIDataBool;

/** For #IDP_UI_DATA_TYPE_FLOAT */
typedef struct IDPropertyUIDataFloat {
  IDPropertyUIData base;
  double *default_array; /* Only for array properties. */
  int default_array_len;
  char _pad[4];

  float step;
  int precision;

  double min;
  double max;
  double soft_min;
  double soft_max;
  double default_value;
} IDPropertyUIDataFloat;

/** For #IDP_UI_DATA_TYPE_STRING */
typedef struct IDPropertyUIDataString {
  IDPropertyUIData base;
  char *default_value;
} IDPropertyUIDataString;

/** For #IDP_UI_DATA_TYPE_ID. */
typedef struct IDPropertyUIDataID {
  IDPropertyUIData base;
  /**
   * #ID_Type. With python-defined properties, this type is not enforced. A value of `0` means any
   * ID type.
   *
   * However, when defined/edited from the UI (Custom Properties panel), it must/will be defined,
   * as generic 'Any ID type' selection is a TODO UI-wise.
   */
  short id_type;
  char _pad[6];
} IDPropertyUIDataID;

typedef struct IDPropertyData {
  void *pointer;
  ListBase group;
  /**
   * Allows constant time lookup by name of the children in this group. This may be null if the
   * group is empty. The order may not be exactly the same as in #group.
   */
  IDPropertyGroupChildrenSet *children_map;
  /** NOTE: a `double` is written into two 32bit integers. */
  int val, val2;
} IDPropertyData;

typedef struct IDProperty {
  struct IDProperty *next, *prev;
  /** #eIDPropertyType */
  char type;
  /**
   * #eIDPropertySubType when `type` is #IDP_STRING.
   * #eIDPropertyType for all other types.
   */
  char subtype;
  /** #IDP_FLAG_GHOST and others. */
  short flag;
  char name[/*MAX_IDPROP_NAME*/ 64];

  char _pad0[4];

  /** NOTE: alignment for 64 bits. */
  IDPropertyData data;

  /**
   * Array length, and importantly string length + 1.
   * the idea is to be able to reuse array reallocation functions on strings.
   */
  int len;
  /**
   * Strings and arrays are both buffered, though the buffer isn't saved.
   * `totallen` is total length of allocated array/string, including a buffer.
   * \note the buffering is mild; see #IDP_ResizeIDPArray for details.
   */
  int totallen;

  IDPropertyUIData *ui_data;
} IDProperty;

#define MAX_IDPROP_NAME 64
#define DEFAULT_ALLOC_FOR_NULL_STRINGS 64

/* add any future new id property types here. */

/* Static ID override structs. */

typedef struct IDOverrideLibraryPropertyOperation {
  struct IDOverrideLibraryPropertyOperation *next, *prev;

  /* Type of override. */
  short operation;
  short flag;

  /** Runtime, tags are common to both #IDOverrideProperty and #IDOverridePropertyOperation. */
  short tag;
  char _pad0[2];

  /* Sub-item references, if needed (for arrays or collections only).
   * We need both reference and local values to allow e.g. insertion into RNA collections
   * (constraints, modifiers...).
   * In RNA collection case, if names are defined, they are used in priority.
   * Names are pointers (instead of char[64]) to save some space, NULL or empty string when unset.
   * Indices are -1 when unset.
   *
   * NOTE: For insertion operations in RNA collections, reference may not actually exist in the
   * linked reference data. It is used to identify the anchor of the insertion operation (i.e. the
   * item after or before which the new local item should be inserted), in the local override. */
  char *subitem_reference_name;
  char *subitem_local_name;
  int subitem_reference_index;
  int subitem_local_index;
  /** Additional pointer to an ID. Only used and relevant when the related RNA collection stores ID
   * pointers, to help disambiguate cases where several IDs from different libraries have the exact
   * same name. */
  struct ID *subitem_reference_id;
  struct ID *subitem_local_id;
} IDOverrideLibraryPropertyOperation;

/* IDOverrideLibraryPropertyOperation->operation. */
enum {
  /* Basic operations. */
  LIBOVERRIDE_OP_NOOP = 0, /* Special value, forbids any overriding. */

  LIBOVERRIDE_OP_REPLACE = 1, /* Fully replace local value by reference one. */

  /* Numeric-only operations. */
  LIBOVERRIDE_OP_ADD = 101, /* Add local value to reference one. */
  /* Subtract local value from reference one (needed due to unsigned values etc.). */
  LIBOVERRIDE_OP_SUBTRACT = 102,
  /* Multiply reference value by local one (more useful than diff for scales and the like). */
  LIBOVERRIDE_OP_MULTIPLY = 103,

  /* Collection-only operations. */
  LIBOVERRIDE_OP_INSERT_AFTER = 201,  /* Insert after given reference's subitem. */
  LIBOVERRIDE_OP_INSERT_BEFORE = 202, /* Insert before given reference's subitem. */
  /* We can add more if needed (move, delete, ...). */
};

/* IDOverrideLibraryPropertyOperation->flag. */
enum {
  /** User cannot remove that override operation. */
  LIBOVERRIDE_OP_FLAG_MANDATORY = 1 << 0,
  /** User cannot change that override operation. */
  LIBOVERRIDE_OP_FLAG_LOCKED = 1 << 1,

  /**
   * For overrides of ID pointers: this override still matches (follows) the hierarchy of the
   * reference linked data.
   */
  LIBOVERRIDE_OP_FLAG_IDPOINTER_MATCH_REFERENCE = 1 << 8,
  /**
   * For overrides of ID pointers within RNA collections: this override is using the ID
   * pointer in addition to the item name (to fully disambiguate the reference, since IDs from
   * different libraries can have a same name).
   */
  LIBOVERRIDE_OP_FLAG_IDPOINTER_ITEM_USE_ID = 1 << 9,
};

/** A single overridden property, contain all operations on this one. */
typedef struct IDOverrideLibraryProperty {
  struct IDOverrideLibraryProperty *next, *prev;

  /**
   * Path from ID to overridden property.
   * *Does not* include indices/names for final arrays/collections items.
   */
  char *rna_path;

  /**
   * List of overriding operations (IDOverrideLibraryPropertyOperation) applied to this property.
   * Recreated as part of the diffing, so do not store any of these elsewhere.
   */
  ListBase operations;

  /**
   * Runtime, tags are common to both IDOverrideLibraryProperty and
   * IDOverrideLibraryPropertyOperation. */
  short tag;
  char _pad[2];

  /** The property type matching the rna_path. */
  unsigned int rna_prop_type;
} IDOverrideLibraryProperty;

/* IDOverrideLibraryProperty->tag and IDOverrideLibraryPropertyOperation->tag. */
enum {
  /** This override property (operation) is unused and should be removed by cleanup process. */
  LIBOVERRIDE_PROP_OP_TAG_UNUSED = 1 << 0,

  /** This override property is forbidden and should be restored to its linked reference value. */
  LIBOVERRIDE_PROP_TAG_NEEDS_RETORE = 1 << 1,
};

#
#
typedef struct IDOverrideLibraryRuntime {
  struct GHash *rna_path_to_override_properties;
  uint tag;
} IDOverrideLibraryRuntime;

/* IDOverrideLibraryRuntime->tag. */
enum {
  /** This override needs to be reloaded. */
  LIBOVERRIDE_TAG_NEEDS_RELOAD = 1 << 0,

  /**
   * This override contains properties with forbidden changes, which should be restored to their
   * linked reference value.
   */
  LIBOVERRIDE_TAG_NEEDS_RESTORE = 1 << 1,

  /**
   * This override is detected as being cut from its hierarchy root. Temporarily used during
   * resync process.
   */
  LIBOVERRIDE_TAG_RESYNC_ISOLATED_FROM_ROOT = 1 << 2,
  /**
   * This override was detected as needing resync outside of the resync process (it is a 'really
   * need resync' case, not a 'need resync for hierarchy reasons' one). Temporarily used during
   * resync process.
   */
  LIBOVERRIDE_TAG_NEED_RESYNC_ORIGINAL = 1 << 3,
};

/* Main container for all overriding data info of a data-block. */
typedef struct IDOverrideLibrary {
  /** Reference linked ID which this one overrides. */
  struct ID *reference;
  /** List of IDOverrideLibraryProperty structs. */
  ListBase properties;

  /**
   * Override hierarchy root ID. Usually the actual root of the hierarchy, but not always
   * in degenerated cases.
   *
   * All liboverrides of a same hierarchy (e.g. a character collection) share the same root.
   */
  struct ID *hierarchy_root;

  IDOverrideLibraryRuntime *runtime;

  unsigned int flag;
  char _pad_1[4];
} IDOverrideLibrary;

/* IDOverrideLibrary->flag */
enum {
  /**
   * The override data-block should not be considered as part of an override hierarchy (generally
   * because it was created as an single override, outside of any hierarchy consideration).
   */
  LIBOVERRIDE_FLAG_NO_HIERARCHY = 1 << 0,
  /**
   * The override ID is required for the system to work (because of ID dependencies), but is not
   * seen as editable by the user.
   */
  LIBOVERRIDE_FLAG_SYSTEM_DEFINED = 1 << 1,
};

/* watch it: Strip has identical beginning. */
/**
 * ID is the first thing included in all serializable types. It
 * provides a common handle to place all data in double-linked lists.
 */

/* 2 characters for ID code and 256 for actual name */
#define MAX_ID_NAME 258

/** #ID_Runtime_Remap.status */
enum {
  /** new_id is directly linked in current .blend. */
  ID_REMAP_IS_LINKED_DIRECT = 1 << 0,
  /** There was some skipped 'user_one' usages of old_id. */
  ID_REMAP_IS_USER_ONE_SKIPPED = 1 << 1,
};

typedef struct IDHash {
  char data[16];

#ifdef __cplusplus
  uint64_t hash() const
  {
    return *reinterpret_cast<const uint64_t *>(this->data);
  }

  static constexpr IDHash get_null()
  {
    return {};
  }
  bool is_null() const
  {
    return *this == IDHash::get_null();
  }

  friend bool operator==(const IDHash &a, const IDHash &b)
  {
    return memcmp(a.data, b.data, sizeof(a.data)) == 0;
  }

  friend bool operator!=(const IDHash &a, const IDHash &b)
  {
    return !(a == b);
  }

#endif
} IDHash;

typedef struct ID {
  /* There's a nasty circular dependency here.... 'void *' to the rescue! I
   * really wonder why this is needed. */
  void *next, *prev;
  struct ID *newid;

  struct Library *lib;

  /** If the ID is an asset, this pointer is set. Owning pointer. */
  struct AssetMetaData *asset_data;

  /**
   * Main identifier for this data-block. Must be unique within the ID name-space (defined by its
   * type, and owning #Library).
   *
   * The first two bytes are always the #ID_Type code of the data-block's type.
   *
   * One critical usage is to reference external linked data. */
  char name[/*MAX_ID_NAME*/ 258];

  /**
   * ID_FLAG_... flags report on status of the data-block this ID belongs to
   * (persistent, saved to and read from .blend).
   */
  short flag;
  /**
   * ID_TAG_... tags (runtime only, cleared at read time).
   */
  int tag;
  int us;
  int icon_id;
  unsigned int recalc;
  /**
   * Used by undo code. recalc_after_undo_push contains the changes between the
   * last undo push and the current state. This is accumulated as IDs are tagged
   * for update in the depsgraph, and only cleared on undo push.
   *
   * recalc_up_to_undo_push is saved to undo memory, and is the value of
   * recalc_after_undo_push at the time of the undo push. This means it can be
   * used to find the changes between undo states.
   */
  unsigned int recalc_up_to_undo_push;
  unsigned int recalc_after_undo_push;

  /**
   * A session-wide unique identifier for a given ID, that remain the same across potential
   * re-allocations (e.g. due to undo/redo steps).
   */
  unsigned int session_uid;

  /**
   * This is only available on packed linked data-blocks. It is a hash of the contents the
   * data-block including all its dependencies. It is computed when first packing the data-block
   * and is not changed afterwards. It can be used to detect that packed data-blocks in two
   * separate .blend files are the same.
   *
   * Two data-blocks with the same deep hash are assumed to be interchangeable, but not necessarily
   * exactly the same. For example, it's possible to change node positions on packed data-blocks
   * without changing the deep hash.
   */
  IDHash deep_hash;

  /**
   * User-defined custom properties storage. Typically Accessed through the 'dict' syntax from
   * Python.
   */
  IDProperty *properties;

  /**
   * System-defined custom properties storage. Used to store data dynamically defined either by
   * Blender itself (e.g. the GeoNode modifier), or some python script, extension etc.
   *
   * Typically accessed through RNA paths (`C.object.my_dynamic_float_property = 33.3`), when
   * wrapped/defined by RNA.
   */
  IDProperty *system_properties;

  void *_pad1;

  /** Reference linked ID which this one overrides. */
  IDOverrideLibrary *override_library;

  /**
   * Only set for data-blocks which are coming from copy-on-evaluation, points to
   * the original version of it.
   * Also used temporarily during memfile undo to keep a reference to old ID when found.
   */
  struct ID *orig_id;

  /**
   * Holds the #PyObject reference to the ID (initialized on demand).
   *
   * This isn't essential, it could be removed however it gives some advantages:
   *
   * - Every time the #ID is accessed a #BPy_StructRNA doesn't have to be created & destroyed
   *   (consider all the polling and drawing functions that access ID's).
   *
   * - When this #ID is deleted, the #BPy_StructRNA can be invalidated
   *   so accessing it from Python raises an exception instead of crashing.
   *
   *   This is of limited benefit though, as it doesn't apply to non #ID data
   *   that references this ID (the bones of an armature or the modifiers of an object for
   *   example).
   */
  void *py_instance;

  /**
   * Weak reference to an ID in a given library file, used to allow re-using already appended data
   * in some cases, instead of appending it again.
   *
   * May be NULL.
   */
  struct LibraryWeakReference *library_weak_reference;

  /**
   * Allocated runtime data, never written on disk or in undo steps.
   *
   * _Always_ valid for code handling IDs managed by the `BKE_lib_id` API.
   *
   * Internal low-level implementation of ID creation/copying/deletion, and code handling IDs
   * themselves in non-standard ways (mainly the CoW IDs in depsgraph, and some temporary IDs in
   * readfile) may have to manage this pointer themselves (see also #BKE_libblock_runtime_ensure
   * and #BKE_libblock_free_runtime_data).
   */
  ID_RuntimeHandle *runtime;
} ID;

/**
 * For each library file used, a Library struct is added to Main.
 */
typedef struct Library {
#ifdef __cplusplus
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_LI;
#endif

  ID id;
  /** Path name used for reading, can be relative and edited in the outliner. */
  char filepath[/*FILE_MAX*/ 1024];

  /** Flags defining specific characteristics of a library. See #LibraryFlag. */
  uint16_t flag;
  char _pad[6];

  /**
   * For archive library only (#LIBRARY_FLAG_IS_ARCHIVE): The main library owning it.
   *
   * `archive_parent_library` and `packedfile` should never be both non-null in a same Library ID.
   */
  struct Library *archive_parent_library;

  /**
   * Packed blendfile of the library, nullptr if not packed.
   *
   * \note Individual IDs may be packed even if the entire library is not packed.
   *
   * `archive_parent_library` and `packedfile` should never be both non-null in a same Library ID.
   */
  struct PackedFile *packedfile;

  /**
   * Runtime only data, never written in blendfile.
   *
   * Typically allocated when creating a new Library or reading it from a blendfile.
   */
  LibraryRuntimeHandle *runtime;

  void *_pad2;
} Library;

/**
 * #Library.flag
 *
 * Some of these flags define a 'virtual' library, which may not be an actual blendfile, store
 * 'archived' embedded data, etc. IDs contained in these virtual libraries are _not_ managed by
 * regular linking code.
 */
enum LibraryFlag {
  /** The library is an 'archive' that only contains embedded linked data. */
  LIBRARY_FLAG_IS_ARCHIVE = 1 << 0,
};

/**
 * A weak library/ID reference for local data that has been appended, to allow re-using that local
 * data instead of creating a new copy of it in future appends.
 *
 * NOTE: This is by design a week reference, in other words code should be totally fine and perform
 * a regular append if it cannot find a valid matching local ID.
 *
 * NOTE: There should always be only one single ID in current Main matching a given linked
 * reference.
 */
typedef struct LibraryWeakReference {
  /**  Expected to match a `Library.filepath`. */
  char library_filepath[/*FILE_MAX*/ 1024];

  /** May be different from the current local ID name. */
  char library_id_name[/*MAX_ID_NAME*/ 258];

  char _pad[2];
} LibraryWeakReference;

/* PreviewImage.flag */
enum ePreviewImage_Flag {
  PRV_CHANGED = (1 << 0),
  /** If user-edited, do not auto-update this anymore! */
  PRV_USER_EDITED = (1 << 1),
  /* Rendering was invoked. Cleared on file read. */
  PRV_RENDERING = (1 << 2),
};

/* PreviewImage.tag */
enum {
  /** Deferred preview is being loaded. */
  PRV_TAG_DEFFERED_RENDERING = (1 << 1),
  /** Deferred preview should be deleted asap. */
  PRV_TAG_DEFFERED_DELETE = (1 << 2),
  /** This deferred preview could not be loaded (e.g. not found on disk). */
  PRV_TAG_DEFFERED_INVALID = (1 << 3),
};

/**
 * This type allows shallow copies. Use #BKE_previewimg_free() to release contained resources.
 * Don't call this for shallow copies (or the original instance will have dangling pointers).
 */
typedef struct PreviewImage {
  DNA_DEFINE_CXX_METHODS(PreviewImage)
  /* All values of 2 are really NUM_ICON_SIZES */
  unsigned int w[2];
  unsigned int h[2];
  short flag[2];
  short changed_timestamp[2];
  unsigned int *rect[2];

  PreviewImageRuntimeHandle *runtime;
} PreviewImage;

/**
 * Amount of 'fake user' usages of this ID.
 * Always 0 or 1.
 */
#define ID_FAKE_USERS(id) ((((const ID *)id)->flag & ID_FLAG_FAKEUSER) ? 1 : 0)
/**
 * Amount of defined 'extra' shallow, runtime-only usages of this ID (typically from UI).
 * Always 0 or 1.
 *
 * \warning May not actually be part of the total #ID.us count, see #ID_EXTRA_REAL_USERS.
 */
#define ID_EXTRA_USERS(id) (((const ID *)id)->tag & ID_TAG_EXTRAUSER ? 1 : 0)
/**
 * Amount of real 'extra' shallow, runtime-only usages of this ID (typically from UI).
 * Always 0 or 1.
 *
 * \note Actual number of usages added to #ID.us by these extra usages.
 * May be 0 even if there are some 'extra' usages of this ID,
 * when there are also other 'normal' reference-counting usages of it.
 */
#define ID_EXTRA_REAL_USERS(id) (((const ID *)id)->tag & ID_TAG_EXTRAUSER_SET ? 1 : 0)
/**
 * Amount of real usages of this ID (i.e. excluding the 'fake user' one, but including a potential
 * 'extra' shallow/runtime usage).
 */
#define ID_REAL_USERS(id) (((const ID *)id)->us - ID_FAKE_USERS(id))
/**
 * Amount of 'normal' reference-counting usages of this ID
 * (i.e. excluding the 'fake user' one, and a potential 'extra' shallow/runtime usage).
 */
#define ID_REFCOUNTING_USERS(id) (ID_REAL_USERS(id) - ID_EXTRA_REAL_USERS(id))

#define ID_CHECK_UNDO(id) (!ELEM(GS((id)->name), ID_SCR, ID_WM, ID_WS, ID_BR))

#define ID_BLEND_PATH(_bmain, _id) \
  ((_id)->lib ? BKE_main_blendfile_path_from_library(*(_id)->lib) : \
                BKE_main_blendfile_path((_bmain)))
#define ID_BLEND_PATH_FROM_GLOBAL(_id) \
  ((_id)->lib ? BKE_main_blendfile_path_from_library(*(_id)->lib) : \
                BKE_main_blendfile_path_from_global())

#define ID_MISSING(_id) ((((const ID *)(_id))->tag & ID_TAG_MISSING) != 0)

#define ID_IS_LINKED(_id) (((const ID *)(_id))->lib != NULL)
/**
 * Indicates that this ID is linked but also packed into the current .blend file. Note that this
 * just means that this specific ID and its dependencies are packed, not the entire library. So
 * this is separate from #Library::packedfile.
 */
#define ID_IS_PACKED(_id) (ID_IS_LINKED(_id) && ((_id)->flag & ID_FLAG_LINKED_AND_PACKED))

#define ID_TYPE_SUPPORTS_ASSET_EDITABLE(id_type) \
  ELEM(id_type, ID_BR, ID_TE, ID_NT, ID_IM, ID_PC, ID_MA)

#define ID_IS_EDITABLE(_id) \
  ((((const ID *)(_id))->lib == NULL) || \
   ((((const ID *)(_id))->lib->runtime->tag & LIBRARY_ASSET_EDITABLE) && \
    ID_TYPE_SUPPORTS_ASSET_EDITABLE(GS((((const ID *)(_id))->name)))))

/* Note that these are fairly high-level checks, should be used at user interaction level, not in
 * BKE_library_override typically (especially due to the check on ID_TAG_EXTERN). */
#define ID_IS_OVERRIDABLE_LIBRARY_HIERARCHY(_id) \
  (ID_IS_LINKED(_id) && !ID_MISSING(_id) && \
   (BKE_idtype_get_info_from_id((const ID *)(_id))->flags & IDTYPE_FLAGS_NO_LIBLINKING) == 0 && \
   !ELEM(GS(((ID *)(_id))->name), ID_SCE))
#define ID_IS_OVERRIDABLE_LIBRARY(_id) \
  (ID_IS_OVERRIDABLE_LIBRARY_HIERARCHY((_id)) && (((const ID *)(_id))->tag & ID_TAG_EXTERN) != 0)

/* NOTE: The three checks below do not take into account whether given ID is linked or not (when
 * chaining overrides over several libraries). User must ensure the ID is not linked itself
 * currently. */
/* TODO: add `_EDITABLE` versions of those macros (that would check if ID is linked or not)? */
#define ID_IS_OVERRIDE_LIBRARY_REAL(_id) \
  (((const ID *)(_id))->override_library != NULL && \
   ((const ID *)(_id))->override_library->reference != NULL)

#define ID_IS_OVERRIDE_LIBRARY_VIRTUAL(_id) \
  ((((const ID *)(_id))->flag & ID_FLAG_EMBEDDED_DATA_LIB_OVERRIDE) != 0)

#define ID_IS_OVERRIDE_LIBRARY(_id) \
  (ID_IS_OVERRIDE_LIBRARY_REAL(_id) || ID_IS_OVERRIDE_LIBRARY_VIRTUAL(_id))

#define ID_IS_OVERRIDE_LIBRARY_HIERARCHY_ROOT(_id) \
  (!ID_IS_OVERRIDE_LIBRARY_REAL(_id) || \
   ((ID *)(_id))->override_library->hierarchy_root == ((ID *)(_id)))

#define ID_IS_ASSET(_id) (((const ID *)(_id))->asset_data != NULL)

/* Check whether datablock type is covered by copy-on-evaluation. */
#define ID_TYPE_USE_COPY_ON_EVAL(_id_type) \
  (!ELEM(_id_type, ID_LI, ID_SCR, ID_VF, ID_BR, ID_WM, ID_PAL, ID_PC, ID_WS, ID_IM))

/* Check whether data-block type requires copy-on-evaluation from #ID_RECALC_PARAMETERS.
 * Keep in sync with #BKE_id_eval_properties_copy. */
#define ID_TYPE_SUPPORTS_PARAMS_WITHOUT_COW(id_type) ELEM(id_type, ID_ME)

/* This used to be ELEM(id_type, ID_IP), currently there is no deprecated ID
 * type. ID_IP was removed in Blender 5.0. */
#define ID_TYPE_IS_DEPRECATED(id_type) false

#ifdef GS
#  undef GS
#endif
#define GS(a) (CHECK_TYPE_ANY(a, char *, const char *), (ID_Type)(*((const short *)(a))))

#define ID_NEW_SET(_id, _idn) \
  (((ID *)(_id))->newid = (ID *)(_idn), \
   ((ID *)(_id))->newid->tag |= ID_TAG_NEW, \
   (void *)((ID *)(_id))->newid)
#define ID_NEW_REMAP(a) \
  if ((a) && (a)->id.newid) { \
    *(void **)&(a) = (a)->id.newid; \
  } \
  ((void)0)

/** id->flag (persistent). */
enum {
  /** Don't delete the data-block even if unused. */
  ID_FLAG_FAKEUSER = 1 << 9,
  /**
   * The data-block is a sub-data of another one.
   * Direct persistent references are not allowed.
   */
  ID_FLAG_EMBEDDED_DATA = 1 << 10,
  /**
   * Data-block is from a library and linked indirectly, with ID_TAG_INDIRECT
   * tag set. But the current .blend file also has a weak pointer to it that
   * we want to restore if possible, and silently drop if it's missing.
   */
  ID_FLAG_INDIRECT_WEAK_LINK = 1 << 11,
  /**
   * The data-block is a sub-data of another one, which is an override.
   * Note that this also applies to shape-keys, even though they are not 100% embedded data.
   */
  ID_FLAG_EMBEDDED_DATA_LIB_OVERRIDE = 1 << 12,
  /**
   * The override data-block appears to not be needed anymore after resync with linked data, but it
   * was kept around (because e.g. detected as user-edited).
   */
  ID_FLAG_LIB_OVERRIDE_RESYNC_LEFTOVER = 1 << 13,
  /**
   * This `id` was explicitly copied as part of a clipboard copy operation.
   * When reading the clipboard back, this can be used to check which ID's are
   * intended to be part of the clipboard, compared with ID's that were indirectly referenced.
   *
   * While the flag is typically cleared, a saved file may have this set for some data-blocks,
   * so it must be treated as dirty.
   */
  ID_FLAG_CLIPBOARD_MARK = 1 << 14,
  /**
   * Indicates that this linked ID is packed into the current .blend file. This should never be set
   * on local ID (without)one with a null `ID::lib` pointer).
   */
  ID_FLAG_LINKED_AND_PACKED = 1 << 15,
};

/**
 * id->tag (runtime-only).
 *
 * Those tags belong to three different categories, which have different expected handling in
 * code:
 *
 * - RESET_BEFORE_USE: piece of code that wants to use such flag has to ensure they are properly
 *   "reset" first.
 * - RESET_AFTER_USE: piece of code that wants to use such flag has to ensure they are properly
 *   "reset" after usage (though "lifetime" of those flags is a bit fuzzy, e.g. _RECALC ones are
 *   reset on depsgraph evaluation...).
 * - RESET_NEVER: these flags are 'status' ones, and never actually need any reset (except on
 *   initialization during `.blend` file reading).
 *
 * \note These tags are purely runtime, so changing there value is not an issue. When adding new
 * tags, please put them in the relevant category and always keep their values strictly increasing.
 */
enum {
  /**
   * Long-life tags giving important info about general ID management.
   *
   * These tags are typically not changed often, if ever, during an ID's life.
   */

  /**
   * ID is from current .blend file.
   *
   * RESET_NEVER
   */
  ID_TAG_LOCAL = 0,
  /**
   * ID is from a library, but is used (linked) directly by current .blend file.
   *
   * RESET_NEVER
   */
  ID_TAG_EXTERN = 1 << 0,
  /**
   * ID is from a library, and is only used (linked) indirectly through other libraries.
   *
   * RESET_NEVER
   */
  ID_TAG_INDIRECT = 1 << 1,

  /**
   * ID is considered as runtime, and should not be saved when writing .blend file, nor influence
   * (in)direct status of linked data.
   *
   * Only meaningful for IDs belonging to regular Main database, all other cases are implicitly
   * considered runtime-only.
   *
   * RESET_NEVER
   */
  ID_TAG_RUNTIME = 1 << 2,

  /**
   * ID is a place-holder, an 'empty shell' (because the real one could not be linked from its
   * library e.g.).
   *
   * RESET_NEVER
   */
  ID_TAG_MISSING = 1 << 3,

  /**
   * ID has an extra virtual user (aka 'ensured real', as set by e.g. some editors, not to be
   * confused with the `ID_FLAG_FAKEUSER` flag).
   *
   * RESET_NEVER
   *
   * \note This tag does not necessarily mean the actual user count of the ID is increased, this is
   * defined by #ID_TAG_EXTRAUSER_SET.
   */
  ID_TAG_EXTRAUSER = 1 << 4,
  /**
   * ID actually has increased user-count for the extra virtual user.
   *
   * RESET_NEVER
   */
  ID_TAG_EXTRAUSER_SET = 1 << 5,

  /**
   * ID is up-to-date regarding its reference (only for library overrides).
   *
   * RESET_NEVER
   */
  ID_TAG_LIBOVERRIDE_REFOK = 1 << 6,
  /**
   * ID needs an auto-diffing execution, if enabled (only for library overrides).
   *
   * RESET_NEVER
   */
  ID_TAG_LIBOVERRIDE_AUTOREFRESH = 1 << 7,
  /**
   * ID is a library override that needs re-sync to its linked reference.
   *
   * \note Also used by readfile code when creating a missing ID placeholder if it is detected as
   * being a linked liboverride ID.
   *
   * RESET_NEVER
   */
  ID_TAG_LIBOVERRIDE_NEED_RESYNC = 1 << 8,

  /**
   * Short-life tags used during specific processes, like blend-file reading.
   */

  /**
   * ID is newly duplicated/copied (see #ID_NEW_SET macro above).
   *
   * RESET_AFTER_USE
   *
   * \note Also used internally in `readfile.cc` to mark data-blocks needing do_versions.
   */
  ID_TAG_NEW = 1 << 12,
  /**
   * ID is already existing. Set before linking, to distinguish between existing data-blocks and
   * newly linked ones.
   *
   * RESET_AFTER_USE
   */
  ID_TAG_PRE_EXISTING = 1 << 13,

  /**
   * ID is being re-used from the old Main (instead of read from memfile), during memfile undo
   * processing, because it was detected as unchanged.
   *
   * \note Also means that such ID does not need to be lib-linked during undo readfile process.
   *
   * RESET_AFTER_USE
   */
  ID_TAG_UNDO_OLD_ID_REUSED_UNCHANGED = 1 << 17,
  /**
   * ID is being re-used from the old Main (instead of read from memfile), during memfile undo
   * processing, because it is a 'NO_UNDO' type of ID.
   *
   * \note Also means that such ID does not need to be lib-linked during undo readfile process. It
   * does need to be relinked in a different way however, doing a `session_uid`-based lookup into
   * the newly read main database.
   *
   * RESET_AFTER_USE
   */
  ID_TAG_UNDO_OLD_ID_REUSED_NOUNDO = 1 << 18,
  /**
   * ID has be re-read in-place, the ID address is the same as in the old main, but the content is
   * different.
   *
   * RESET_AFTER_USE
   */
  ID_TAG_UNDO_OLD_ID_REREAD_IN_PLACE = 1 << 19,

  /* ------------------------------------------------------------------------------------------- */
  /**
   * Tags for special kind of IDs, not living in regular BMain data-base.
   *
   * IDs with such tags have typically a short life.
   */

  /**
   * ID is part of a temporary #Main which is expected to be freed in a short time-frame.
   *
   * RESET_NEVER
   *
   * Don't allow assigning this to non-temporary members (since it's likely to cause errors).
   * When set #ID.session_uid isn't initialized, since the data isn't part of the session.
   */
  ID_TAG_TEMP_MAIN = 1 << 20,
  /** General ID management info, for freeing or copying behavior e.g. */
  /**
   * ID is not listed/stored in any #Main database.
   *
   * RESET_NEVER
   */
  ID_TAG_NO_MAIN = 1 << 21,
  /**
   * ID is fully outside of any ID management area, and should be considered as a purely
   * independent data.
   *
   * RESET_NEVER
   *
   * \note Only used by node-trees currently.
   */
  ID_TAG_LOCALIZED = 1 << 22,
  /**
   * ID is an evaluated/localized version.
   *
   * RESET_NEVER
   *
   * \warning This should not be cleared on existing data.
   * If support for this is needed, see #88026 as this flag controls memory ownership
   * of physics *shared* pointers.
   */
  ID_TAG_COPIED_ON_EVAL = 1 << 23,
  /**
   * ID is not the original evaluated ID created by the depsgraph, but has been re-allocated during
   * the evaluation process of another ID.
   *
   * RESET_NEVER
   *
   * Typical example is object data, when evaluating the object's modifier stack the final obdata
   * can be different than the evaluated initial obdata ID.
   */
  ID_TAG_COPIED_ON_EVAL_FINAL_RESULT = 1 << 24,

  /**
   * ID management status tags related to non-standard BMain IDs.
   */

  /**
   * Datablock does not refcount usages of other IDs.
   *
   * RESET_NEVER
   */
  ID_TAG_NO_USER_REFCOUNT = 1 << 25,
  /**
   * ID was not allocated by standard system (BKE_libblock_alloc), do not free its memory
   * (usual type-specific freeing is called though).
   *
   * RESET_NEVER
   */
  ID_TAG_NOT_ALLOCATED = 1 << 26,

  /* ------------------------------------------------------------------------------------------- */

  /**
   * Free to use tag, often used in BKE code to mark IDs to be processed.
   *
   * RESET_BEFORE_USE
   *
   * \todo Make it a RESET_AFTER_USE too.
   */
  ID_TAG_DOIT = 1u << 31,
};

/**
 * Most of ID tags are cleared on file write (i.e. also when storing undo steps), since they
 * either have of very short lifetime (not expected to exist across undo steps), or are info that
 * will be re-generated when reading undo steps.
 *
 * However a few of these need to be explicitly preserved across undo steps.
 */
#define ID_TAG_KEEP_ON_UNDO (ID_TAG_EXTRAUSER | ID_TAG_MISSING | ID_TAG_RUNTIME)

/* Tag given ID for an update in all the dependency graphs. */
typedef enum IDRecalcFlag {
  /***************************************************************************
   * Individual update tags, this is what ID gets tagged for update with. */

  /* ** Object transformation changed. ** */
  ID_RECALC_TRANSFORM = (1 << 0),

  /* ** Geometry changed. **
   *
   * When object of armature type gets tagged with this flag, its pose is
   * re-evaluated.
   *
   * When object of other type is tagged with this flag it makes the modifier
   * stack to be re-evaluated.
   *
   * When object data type (mesh, curve, ...) gets tagged with this flag it
   * makes all objects which shares this data-block to be updated.
   *
   * Note that the evaluation depends on the object-mode.
   * So edit-mesh data for example only reevaluate with the updated edit-mesh.
   * When geometry in the original ID has been modified #ID_RECALC_GEOMETRY_ALL_MODES
   * must be used instead.
   *
   * When a collection gets tagged with this flag, all objects depending on the geometry and
   * transforms on any of the objects in the collection are updated. */
  ID_RECALC_GEOMETRY = (1 << 1),

  /* ** Animation or time changed and animation is to be re-evaluated. ** */
  ID_RECALC_ANIMATION = (1 << 2),

  /* ** Particle system changed. ** */
  /* Only do path-cache etc. */
  ID_RECALC_PSYS_REDO = (1 << 3),
  /* Reset everything including point-cache. */
  ID_RECALC_PSYS_RESET = (1 << 4),
  /* Only child settings changed. */
  ID_RECALC_PSYS_CHILD = (1 << 5),
  /* Physics type changed. */
  ID_RECALC_PSYS_PHYS = (1 << 6),

  /* ** Material and shading ** */

  /* For materials and node trees this means that topology of the shader tree
   * changed, and the shader is to be recompiled.
   * For objects it means that the draw batch cache is to be redone. */
  ID_RECALC_SHADING = (1 << 7),
  /* TODO(sergey): Consider adding an explicit ID_RECALC_SHADING_PARAMETERS
   * which can be used for cases when only socket value changed, to speed up
   * redraw update in that case. */

  /* Selection of the ID itself or its components (for example, vertices) did
   * change, and all the drawing data is to be updated. */
  ID_RECALC_SELECT = (1 << 9),
  /* Flags on the base did change, and is to be copied onto all the copies of
   * corresponding objects. */
  ID_RECALC_BASE_FLAGS = (1 << 10),
  ID_RECALC_POINT_CACHE = (1 << 11),
  /* Only inform editors about the change. Is used to force update of editors
   * when data-block which is not a part of dependency graph did change.
   *
   * For example, brush texture did change and the preview is to be
   * re-rendered. */
  ID_RECALC_EDITORS = (1 << 12),

  /* ** Update evaluated copy component. **
   *
   * This is most generic tag which should only be used when nothing else matches.
   * It is not to explicitly mixed in with other recalculation flags.
   */
  ID_RECALC_SYNC_TO_EVAL = (1 << 13),

  /* Sequences in the sequencer did change.
   * Use this tag with a scene ID which owns the sequences. */
  ID_RECALC_SEQUENCER_STRIPS = (1 << 14),

  /* Runs on frame-change (used for seeking audio too). */
  ID_RECALC_FRAME_CHANGE = (1 << 15),

  ID_RECALC_AUDIO_FPS = (1 << 16),
  ID_RECALC_AUDIO_VOLUME = (1 << 17),
  ID_RECALC_AUDIO_MUTE = (1 << 18),
  ID_RECALC_AUDIO_LISTENER = (1 << 19),

  ID_RECALC_AUDIO = (1 << 20),

  /* NOTE: This triggers copy-on-eval for types that require it.
   * Exceptions to this can be added using #ID_TYPE_SUPPORTS_PARAMS_WITHOUT_COW,
   * this has the advantage that large arrays stored in the idea data don't
   * have to be copied on every update. */
  ID_RECALC_PARAMETERS = (1 << 21),

  /* Input has changed and data-block is to be reload from disk.
   * Applies to movie clips to inform that copy-on-written version is to be refreshed for the new
   * input file or for color space changes. */
  ID_RECALC_SOURCE = (1 << 23),

  /* Virtual recalc tag/marker required for undo in some cases, where actual data does not change
   * and hence do not require an update, but conceptually we are dealing with something new.
   *
   * Current known case: linked IDs made local without requiring any copy. While their users do not
   * require any update, they have actually been 'virtually' remapped from the linked ID to the
   * local one.
   */
  ID_RECALC_TAG_FOR_UNDO = (1 << 24),

  /* The node tree has changed in a way that affects its output nodes. */
  ID_RECALC_NTREE_OUTPUT = (1 << 25),

  /* Hierarchy of collection and object within collection changed. */
  ID_RECALC_HIERARCHY = (1 << 26),

  /* Provisioned flags.
   *
   * Not for actual use. The idea of them is to have all bits of the `IDRecalcFlag` defined to a
   * known value, silencing sanitizer warnings when checking bits of the ID_RECALC_ALL. */
  ID_RECALC_PROVISION_27 = (1 << 27),
  ID_RECALC_PROVISION_28 = (1 << 28),
  ID_RECALC_PROVISION_29 = (1 << 29),
  ID_RECALC_PROVISION_30 = (1 << 30),
  ID_RECALC_PROVISION_31 = (1u << 31),

  /***************************************************************************
   * Pseudonyms, to have more semantic meaning in the actual code without
   * using too much low-level and implementation specific tags. */

  /* Update animation data-block itself, without doing full re-evaluation of
   * all dependent objects. */
  ID_RECALC_ANIMATION_NO_FLUSH = ID_RECALC_SYNC_TO_EVAL,

  /* Ensure geometry of object and edit modes are both up-to-date in the evaluated data-block.
   * Example usage is when mesh validation modifies the non-edit-mode data,
   * which we want to be copied over to the evaluated data-block. */
  ID_RECALC_GEOMETRY_ALL_MODES = ID_RECALC_GEOMETRY | ID_RECALC_SYNC_TO_EVAL,

  /***************************************************************************
   * Aggregate flags, use only for checks on runtime.
   * Do NOT use those for tagging. */

  /* Identifies that SOMETHING has been changed in this ID. */
  ID_RECALC_ALL = (0xffffffff),

  /* Identifies that something in particle system did change. */
  ID_RECALC_PSYS_ALL = (ID_RECALC_PSYS_REDO | ID_RECALC_PSYS_RESET | ID_RECALC_PSYS_CHILD |
                        ID_RECALC_PSYS_PHYS),

} IDRecalcFlag;

/* To filter ID types (filter_id). 64 bit to fit all types. */
#define FILTER_ID_AC (1ULL << 0)
#define FILTER_ID_AR (1ULL << 1)
#define FILTER_ID_BR (1ULL << 2)
#define FILTER_ID_CA (1ULL << 3)
#define FILTER_ID_CU_LEGACY (1ULL << 4)
#define FILTER_ID_GD_LEGACY (1ULL << 5)
#define FILTER_ID_GR (1ULL << 6)
#define FILTER_ID_IM (1ULL << 7)
#define FILTER_ID_LA (1ULL << 8)
#define FILTER_ID_LS (1ULL << 9)
#define FILTER_ID_LT (1ULL << 10)
#define FILTER_ID_MA (1ULL << 11)
#define FILTER_ID_MB (1ULL << 12)
#define FILTER_ID_MC (1ULL << 13)
#define FILTER_ID_ME (1ULL << 14)
#define FILTER_ID_MSK (1ULL << 15)
#define FILTER_ID_NT (1ULL << 16)
#define FILTER_ID_OB (1ULL << 17)
#define FILTER_ID_PAL (1ULL << 18)
#define FILTER_ID_PC (1ULL << 19)
#define FILTER_ID_SCE (1ULL << 20)
#define FILTER_ID_SPK (1ULL << 21)
#define FILTER_ID_SO (1ULL << 22)
#define FILTER_ID_TE (1ULL << 23)
#define FILTER_ID_TXT (1ULL << 24)
#define FILTER_ID_VF (1ULL << 25)
#define FILTER_ID_WO (1ULL << 26)
#define FILTER_ID_PA (1ULL << 27)
#define FILTER_ID_CF (1ULL << 28)
#define FILTER_ID_WS (1ULL << 29)
#define FILTER_ID_LP (1ULL << 31)
#define FILTER_ID_CV (1ULL << 32)
#define FILTER_ID_PT (1ULL << 33)
#define FILTER_ID_VO (1ULL << 34)
#define FILTER_ID_SIM (1ULL << 35)
#define FILTER_ID_KE (1ULL << 36)
#define FILTER_ID_SCR (1ULL << 37)
#define FILTER_ID_WM (1ULL << 38)
#define FILTER_ID_LI (1ULL << 39)
#define FILTER_ID_GP (1ULL << 40)
#define FILTER_ID_IP (1ULL << 41)

#define FILTER_ID_ALL \
  (FILTER_ID_AC | FILTER_ID_AR | FILTER_ID_BR | FILTER_ID_CA | FILTER_ID_CU_LEGACY | \
   FILTER_ID_GD_LEGACY | FILTER_ID_GR | FILTER_ID_IM | FILTER_ID_LA | FILTER_ID_LS | \
   FILTER_ID_LT | FILTER_ID_MA | FILTER_ID_MB | FILTER_ID_MC | FILTER_ID_ME | FILTER_ID_MSK | \
   FILTER_ID_NT | FILTER_ID_OB | FILTER_ID_PA | FILTER_ID_PAL | FILTER_ID_PC | FILTER_ID_SCE | \
   FILTER_ID_SPK | FILTER_ID_SO | FILTER_ID_TE | FILTER_ID_TXT | FILTER_ID_VF | FILTER_ID_WO | \
   FILTER_ID_CF | FILTER_ID_WS | FILTER_ID_LP | FILTER_ID_CV | FILTER_ID_PT | FILTER_ID_VO | \
   FILTER_ID_SIM | FILTER_ID_KE | FILTER_ID_SCR | FILTER_ID_WM | FILTER_ID_LI | FILTER_ID_GP | \
   FILTER_ID_IP)

/**
 * This enum defines the index assigned to each type of IDs in the array returned by
 * #BKE_main_lists_get, and by extension, controls the default order in which each ID type is
 * processed during standard 'foreach' looping over all IDs of a #Main data-base.
 *
 * About Order:
 * ------------
 *
 * This is (loosely) defined with a relationship order in mind, from lowest level (ID types using,
 * referencing almost no other ID types) to highest level (ID types potentially using many other ID
 * types).
 *
 * So e.g. it ensures that this dependency chain is respected:
 *   #Material <- #Mesh <- #Object <- #Collection <- #Scene
 *
 * Default order of processing of IDs in 'foreach' macros (#FOREACH_MAIN_ID_BEGIN and the like),
 * built on top of #BKE_main_lists_get, is actually reversed compared to the order defined here,
 * since processing usually needs to happen on users before it happens on used IDs (when freeing
 * e.g.).
 *
 * DO NOT rely on this order as being full-proofed dependency order, there are many cases were it
 * can be violated (most obvious cases being custom properties and drivers, which can reference any
 * other ID types).
 *
 * However, this order can be considered as an optimization heuristic, especially when processing
 * relationships in a non-recursive pattern: in typical cases, a vast majority of those
 * relationships can be processed fine in the first pass, and only few additional passes are
 * required to address all remaining relationship cases.
 * See e.g. how #BKE_library_unused_linked_data_set_tag is doing this.
 */
typedef enum eID_Index {
  /* Special case: Library, should never ever depend on any other type. */
  INDEX_ID_LI = 0,

  /* Animation types, might be used by almost all other types. */
  INDEX_ID_AC,

  /* Grease Pencil, special case, should be with the other obdata, but it can also be used by many
   * other ID types, including node trees e.g.
   * So there is no proper place for those, for now keep close to the lower end of the processing
   * hierarchy, but we may want to re-evaluate that at some point. */
  INDEX_ID_GD_LEGACY,

  /* Node trees, abstraction for procedural data, potentially used by many other ID types.
   *
   * NOTE: While node trees can also use many other ID types, they should not /own/ any of those,
   * while they are being owned by many other ID types. This is why they are placed here. */
  INDEX_ID_NT,

  /* File-wrapper types, those usually 'embed' external files in Blender, with no dependencies to
   * other ID types. */
  INDEX_ID_VF,
  INDEX_ID_TXT,
  INDEX_ID_SO,

  /* Image/movie types, can be used by shading ID types, but also directly by Objects, Scenes, etc.
   */
  INDEX_ID_MSK,
  INDEX_ID_IM,
  INDEX_ID_MC,

  /* Shading types. */
  INDEX_ID_TE,
  INDEX_ID_MA,
  INDEX_ID_LS,
  INDEX_ID_WO,

  /* Simulation-related types. */
  INDEX_ID_CF,
  INDEX_ID_PA,

  /* Shape Keys snow-flake, can be used by several obdata types. */
  INDEX_ID_KE,

  /* Object data types. */
  INDEX_ID_AR,
  INDEX_ID_ME,
  INDEX_ID_CU_LEGACY,
  INDEX_ID_MB,
  INDEX_ID_CV,
  INDEX_ID_PT,
  INDEX_ID_VO,
  INDEX_ID_LT,
  INDEX_ID_LA,
  INDEX_ID_CA,
  INDEX_ID_SPK,
  INDEX_ID_LP,
  INDEX_ID_GP,

  /* Collection and object types. */
  INDEX_ID_OB,
  INDEX_ID_GR,

  /* Preset-like, not-really-data types, can use many other ID types but should never be used by
   * any actual data type (besides Scene, due to tool settings). */
  INDEX_ID_PAL,
  INDEX_ID_PC,
  INDEX_ID_BR,

  /* Scene, after preset-like ID types because of tool settings. */
  INDEX_ID_SCE,

  /* UI-related types, should never be used by any other data type. */
  INDEX_ID_SCR,
  INDEX_ID_WS,
  INDEX_ID_WM,

  /* Special values, keep last. */
  INDEX_ID_NULL,
} eID_Index;

#define INDEX_ID_MAX (INDEX_ID_NULL + 1)

#ifdef __cplusplus
namespace blender::dna {
namespace detail {
template<typename, typename = void> struct has_ID_member : std::false_type {};
template<typename T> struct has_ID_member<T, std::void_t<decltype(&T::id)>> : std::true_type {};
template<typename T> constexpr bool has_ID_as_first_member()
{
  if constexpr (std::is_standard_layout_v<T> && has_ID_member<T>::value) {
    return offsetof(T, id) == 0 && std::is_same_v<decltype(T::id), ID>;
  }
  else {
    return false;
  }
}
}  // namespace detail

/**
 * Type trait to check if a type is a ID data-block. It just actually checks whether the type has
 * #ID is first data member, which should be good enough in practice.
 */
template<typename T>
constexpr bool is_ID_v = detail::has_ID_as_first_member<T>() || std::is_same_v<T, ID>;

}  // namespace blender::dna

namespace blender {

namespace dna::detail {
template<typename Dst, typename Src, typename SrcRuntime>
constexpr void id_cast_assert([[maybe_unused]] SrcRuntime *src)
{
  static_assert(blender::dna::is_ID_v<Src>);
  static_assert(blender::dna::is_ID_v<Dst>);
  if constexpr (std::is_same_v<Src, ID> && !std::is_same_v<Dst, ID>) {
    /* Runtime check for when converting from #ID to subtype like #Object. */
    BLI_assert(src == nullptr || GS(src->name) == Dst::id_type);
  }
  else if constexpr (!std::is_same_v<Src, ID> && std::is_same_v<Dst, ID>) {
    /* Converting from subtype like #Object to #ID is always allowed. */
  }
  else {
    /* Converting between the same types is always allowed. */
    static_assert(std::is_same_v<Src, Dst>);
  }
}
}  // namespace dna::detail

/**
 * A drop-in replacement for `reinterpret_cast` that does additional checks:
 * - Static check that the source and destination types are data-block types.
 * - Run-time assert when down-casting from #ID to e.g. #Object.
 *
 * \note This can't be used with forward-declared types as the type information is necessary for
 * the additional checks. For the same reason, it also can't be used to convert from void pointers.
 */
template<typename Dst, typename Src> inline Dst id_cast(Src &&id)
{
  using DstDecay = std::decay_t<Dst>;
  using SrcDecay = std::decay_t<Src>;
  static_assert(std::is_pointer_v<SrcDecay> == std::is_pointer_v<DstDecay>);
  if constexpr (std::is_pointer_v<SrcDecay>) {
    dna::detail::id_cast_assert<std::decay_t<std::remove_pointer_t<DstDecay>>,
                                std::decay_t<std::remove_pointer_t<SrcDecay>>>(id);
  }
  else {
    static_assert(std::is_lvalue_reference_v<Src> && std::is_lvalue_reference_v<Dst>);
    dna::detail::id_cast_assert<DstDecay, SrcDecay>(&id);
  }
  /* This also makes sure that we don't cast away constness. */
  return reinterpret_cast<Dst>(id);
}

}  // namespace blender
#endif

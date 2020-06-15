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

/** \file
 * \ingroup DNA
 * \brief ID and Library types, which are fundamental for sdna.
 */

#ifndef __DNA_ID_H__
#define __DNA_ID_H__

#include "DNA_defs.h"
#include "DNA_listBase.h"

#ifdef __cplusplus
extern "C" {
#endif

struct FileData;
struct GHash;
struct GPUTexture;
struct ID;
struct Library;
struct PackedFile;

/* Runtime display data */
struct DrawData;
typedef void (*DrawDataInitCb)(struct DrawData *engine_data);
typedef void (*DrawDataFreeCb)(struct DrawData *engine_data);

#
#
typedef struct DrawData {
  struct DrawData *next, *prev;
  struct DrawEngineType *engine_type;
  /* Only nested data, NOT the engine data itself. */
  DrawDataFreeCb free;
  /* Accumulated recalc flags, which corresponds to ID->recalc flags. */
  int recalc;
} DrawData;

typedef struct DrawDataList {
  struct DrawData *first, *last;
} DrawDataList;

typedef struct IDPropertyData {
  void *pointer;
  ListBase group;
  /** Note, we actually fit a double into these two ints. */
  int val, val2;
} IDPropertyData;

typedef struct IDProperty {
  struct IDProperty *next, *prev;
  char type, subtype;
  short flag;
  /** MAX_IDPROP_NAME. */
  char name[64];

  /* saved is used to indicate if this struct has been saved yet.
   * seemed like a good idea as a '_pad' var was needed anyway :) */
  int saved;
  /** Note, alignment for 64 bits. */
  IDPropertyData data;

  /* array length, also (this is important!) string length + 1.
   * the idea is to be able to reuse array realloc functions on strings.*/
  int len;

  /* Strings and arrays are both buffered, though the buffer isn't saved. */
  /* totallen is total length of allocated array/string, including a buffer.
   * Note that the buffering is mild; the code comes from python's list implementation. */
  int totallen;
} IDProperty;

#define MAX_IDPROP_NAME 64
#define DEFAULT_ALLOC_FOR_NULL_STRINGS 64

/*->type*/
enum {
  IDP_STRING = 0,
  IDP_INT = 1,
  IDP_FLOAT = 2,
  IDP_ARRAY = 5,
  IDP_GROUP = 6,
  IDP_ID = 7,
  IDP_DOUBLE = 8,
  IDP_IDPARRAY = 9,
  IDP_NUMTYPES = 10,
};

/** Used by some IDP utils, keep values in sync with type enum above. */
enum {
  IDP_TYPE_FILTER_STRING = 1 << 0,
  IDP_TYPE_FILTER_INT = 1 << 1,
  IDP_TYPE_FILTER_FLOAT = 1 << 2,
  IDP_TYPE_FILTER_ARRAY = 1 << 5,
  IDP_TYPE_FILTER_GROUP = 1 << 6,
  IDP_TYPE_FILTER_ID = 1 << 7,
  IDP_TYPE_FILTER_DOUBLE = 1 << 8,
  IDP_TYPE_FILTER_IDPARRAY = 1 << 9,
};

/*->subtype */

/* IDP_STRING */
enum {
  IDP_STRING_SUB_UTF8 = 0, /* default */
  IDP_STRING_SUB_BYTE = 1, /* arbitrary byte array, _not_ null terminated */
};

/*->flag*/
enum {
  /** This IDProp may be statically overridden.
   * Should only be used/be relevant for custom properties. */
  IDP_FLAG_OVERRIDABLE_LIBRARY = 1 << 0,

  /** This means the property is set but RNA will return false when checking
   * 'RNA_property_is_set', currently this is a runtime flag */
  IDP_FLAG_GHOST = 1 << 7,
};

/* add any future new id property types here.*/

/* Static ID override structs. */

typedef struct IDOverrideLibraryPropertyOperation {
  struct IDOverrideLibraryPropertyOperation *next, *prev;

  /* Type of override. */
  short operation;
  short flag;

  /** Runtime, tags are common to both IDOverrideProperty and IDOverridePropertyOperation. */
  short tag;
  char _pad0[2];

  /* Sub-item references, if needed (for arrays or collections only).
   * We need both reference and local values to allow e.g. insertion into collections
   * (constraints, modifiers...).
   * In collection case, if names are defined, they are used in priority.
   * Names are pointers (instead of char[64]) to save some space, NULL when unset.
   * Indices are -1 when unset. */
  char *subitem_reference_name;
  char *subitem_local_name;
  int subitem_reference_index;
  int subitem_local_index;
} IDOverrideLibraryPropertyOperation;

/* IDOverridePropertyOperation->operation. */
enum {
  /* Basic operations. */
  IDOVERRIDE_LIBRARY_OP_NOOP = 0, /* Special value, forbids any overriding. */

  IDOVERRIDE_LIBRARY_OP_REPLACE = 1, /* Fully replace local value by reference one. */

  /* Numeric-only operations. */
  IDOVERRIDE_LIBRARY_OP_ADD = 101, /* Add local value to reference one. */
  /* Subtract local value from reference one (needed due to unsigned values etc.). */
  IDOVERRIDE_LIBRARY_OP_SUBTRACT = 102,
  /* Multiply reference value by local one (more useful than diff for scales and the like). */
  IDOVERRIDE_LIBRARY_OP_MULTIPLY = 103,

  /* Collection-only operations. */
  IDOVERRIDE_LIBRARY_OP_INSERT_AFTER = 201,  /* Insert after given reference's subitem. */
  IDOVERRIDE_LIBRARY_OP_INSERT_BEFORE = 202, /* Insert before given reference's subitem. */
  /* We can add more if needed (move, delete, ...). */
};

/* IDOverridePropertyOperation->flag. */
enum {
  /** User cannot remove that override operation. */
  IDOVERRIDE_LIBRARY_FLAG_MANDATORY = 1 << 0,
  /** User cannot change that override operation. */
  IDOVERRIDE_LIBRARY_FLAG_LOCKED = 1 << 1,
};

/** A single overridden property, contain all operations on this one. */
typedef struct IDOverrideLibraryProperty {
  struct IDOverrideLibraryProperty *next, *prev;

  /**
   * Path from ID to overridden property.
   * *Does not* include indices/names for final arrays/collections items.
   */
  char *rna_path;

  /** List of overriding operations (IDOverridePropertyOperation) applied to this property. */
  ListBase operations;

  /** Runtime, tags are common to both IDOverrideProperty and IDOverridePropertyOperation. */
  short tag;
  char _pad0[6];
} IDOverrideLibraryProperty;

/* IDOverrideProperty->tag and IDOverridePropertyOperation->tag. */
enum {
  /** This override property (operation) is unused and should be removed by cleanup process. */
  IDOVERRIDE_LIBRARY_TAG_UNUSED = 1 << 0,
};

/* We do not need a full struct for that currently, just a GHash. */
typedef struct GHash IDOverrideLibraryRuntime;

/* Main container for all overriding data info of a data-block. */
typedef struct IDOverrideLibrary {
  /** Reference linked ID which this one overrides. */
  struct ID *reference;
  /** List of IDOverrideProperty structs. */
  ListBase properties;

  /* Read/write data. */
  /* Temp ID storing extra override data (used for differential operations only currently).
   * Always NULL outside of read/write context. */
  struct ID *storage;

  IDOverrideLibraryRuntime *runtime;
} IDOverrideLibrary;

/* watch it: Sequence has identical beginning. */
/**
 * ID is the first thing included in all serializable types. It
 * provides a common handle to place all data in double-linked lists.
 * */

/* 2 characters for ID code and 64 for actual name */
#define MAX_ID_NAME 66

/* There's a nasty circular dependency here.... 'void *' to the rescue! I
 * really wonder why this is needed. */
typedef struct ID {
  void *next, *prev;
  struct ID *newid;
  struct Library *lib;
  /** MAX_ID_NAME. */
  char name[66];
  /**
   * LIB_... flags report on status of the data-block this ID belongs to
   * (persistent, saved to and read from .blend).
   */
  short flag;
  /**
   * LIB_TAG_... tags (runtime only, cleared at read time).
   */
  int tag;
  int us;
  int icon_id;
  int recalc;
  /**
   * Used by undo code. recalc_after_undo_push contains the changes between the
   * last undo push and the current state. This is accumulated as IDs are tagged
   * for update in the depsgraph, and only cleared on undo push.
   *
   * recalc_up_to_undo_push is saved to undo memory, and is the value of
   * recalc_after_undo_push at the time of the undo push. This means it can be
   * used to find the changes between undo states.
   */
  int recalc_up_to_undo_push;
  int recalc_after_undo_push;

  /**
   * A session-wide unique identifier for a given ID, that remain the same across potential
   * re-allocations (e.g. due to undo/redo steps).
   */
  unsigned int session_uuid;

  IDProperty *properties;

  /** Reference linked ID which this one overrides. */
  IDOverrideLibrary *override_library;

  /**
   * Only set for data-blocks which are coming from copy-on-write, points to
   * the original version of it.
   */
  struct ID *orig_id;

  void *py_instance;
} ID;

/**
 * For each library file used, a Library struct is added to Main
 * WARNING: readfile.c, expand_doit() reads this struct without DNA check!
 */
typedef struct Library {
  ID id;
  struct FileData *filedata;
  /** Path name used for reading, can be relative and edited in the outliner. */
  char name[1024];

  /**
   * Absolute filepath, this is only for convenience,
   * 'name' is the real path used on file read but in
   * some cases its useful to access the absolute one.
   * This is set on file read.
   * Use BKE_library_filepath_set() rather than setting 'name'
   * directly and it will be kept in sync - campbell */
  char filepath[1024];

  /** Set for indirectly linked libs, used in the outliner and while reading. */
  struct Library *parent;

  struct PackedFile *packedfile;

  /* Temp data needed by read/write code. */
  int temp_index;
  /** See BLENDER_FILE_VERSION, BLENDER_FILE_SUBVERSION, needed for do_versions. */
  short versionfile, subversionfile;
} Library;

enum eIconSizes {
  ICON_SIZE_ICON = 0,
  ICON_SIZE_PREVIEW = 1,

  NUM_ICON_SIZES,
};

/* for PreviewImage->flag */
enum ePreviewImage_Flag {
  PRV_CHANGED = (1 << 0),
  PRV_USER_EDITED = (1 << 1), /* if user-edited, do not auto-update this anymore! */
};

/* for PreviewImage->tag */
enum {
  PRV_TAG_DEFFERED = (1 << 0),           /* Actual loading of preview is deferred. */
  PRV_TAG_DEFFERED_RENDERING = (1 << 1), /* Deferred preview is being loaded. */
  PRV_TAG_DEFFERED_DELETE = (1 << 2),    /* Deferred preview should be deleted asap. */
};

typedef struct PreviewImage {
  /* All values of 2 are really NUM_ICON_SIZES */
  unsigned int w[2];
  unsigned int h[2];
  short flag[2];
  short changed_timestamp[2];
  unsigned int *rect[2];

  /* Runtime-only data. */
  struct GPUTexture *gputexture[2];
  /** Used by previews outside of ID context. */
  int icon_id;

  /** Runtime data. */
  short tag;
  char _pad[2];
} PreviewImage;

#define PRV_DEFERRED_DATA(prv) \
  (CHECK_TYPE_INLINE(prv, PreviewImage *), \
   BLI_assert((prv)->tag & PRV_TAG_DEFFERED), \
   (void *)((prv) + 1))

/**
 * Defines for working with IDs.
 *
 * The tags represent types! This is a dirty way of enabling RTTI. The
 * sig_byte end endian defines aren't really used much.
 */

#ifdef __BIG_ENDIAN__
/* big endian */
#  define MAKE_ID2(c, d) ((c) << 8 | (d))
#else
/* little endian  */
#  define MAKE_ID2(c, d) ((d) << 8 | (c))
#endif

/**
 * ID from database.
 *
 * Written to #BHead.code (for file IO)
 * and the first 2 bytes of #ID.name (for runtime checks, see #GS macro).
 */
typedef enum ID_Type {
  ID_SCE = MAKE_ID2('S', 'C'), /* Scene */
  ID_LI = MAKE_ID2('L', 'I'),  /* Library */
  ID_OB = MAKE_ID2('O', 'B'),  /* Object */
  ID_ME = MAKE_ID2('M', 'E'),  /* Mesh */
  ID_CU = MAKE_ID2('C', 'U'),  /* Curve */
  ID_MB = MAKE_ID2('M', 'B'),  /* MetaBall */
  ID_MA = MAKE_ID2('M', 'A'),  /* Material */
  ID_TE = MAKE_ID2('T', 'E'),  /* Tex (Texture) */
  ID_IM = MAKE_ID2('I', 'M'),  /* Image */
  ID_LT = MAKE_ID2('L', 'T'),  /* Lattice */
  ID_LA = MAKE_ID2('L', 'A'),  /* Light */
  ID_CA = MAKE_ID2('C', 'A'),  /* Camera */
  ID_IP = MAKE_ID2('I', 'P'),  /* Ipo (depreciated, replaced by FCurves) */
  ID_KE = MAKE_ID2('K', 'E'),  /* Key (shape key) */
  ID_WO = MAKE_ID2('W', 'O'),  /* World */
  ID_SCR = MAKE_ID2('S', 'R'), /* Screen */
  ID_VF = MAKE_ID2('V', 'F'),  /* VFont (Vector Font) */
  ID_TXT = MAKE_ID2('T', 'X'), /* Text */
  ID_SPK = MAKE_ID2('S', 'K'), /* Speaker */
  ID_SO = MAKE_ID2('S', 'O'),  /* Sound */
  ID_GR = MAKE_ID2('G', 'R'),  /* Group */
  ID_AR = MAKE_ID2('A', 'R'),  /* bArmature */
  ID_AC = MAKE_ID2('A', 'C'),  /* bAction */
  ID_NT = MAKE_ID2('N', 'T'),  /* bNodeTree */
  ID_BR = MAKE_ID2('B', 'R'),  /* Brush */
  ID_PA = MAKE_ID2('P', 'A'),  /* ParticleSettings */
  ID_GD = MAKE_ID2('G', 'D'),  /* bGPdata, (Grease Pencil) */
  ID_WM = MAKE_ID2('W', 'M'),  /* WindowManager */
  ID_MC = MAKE_ID2('M', 'C'),  /* MovieClip */
  ID_MSK = MAKE_ID2('M', 'S'), /* Mask */
  ID_LS = MAKE_ID2('L', 'S'),  /* FreestyleLineStyle */
  ID_PAL = MAKE_ID2('P', 'L'), /* Palette */
  ID_PC = MAKE_ID2('P', 'C'),  /* PaintCurve  */
  ID_CF = MAKE_ID2('C', 'F'),  /* CacheFile */
  ID_WS = MAKE_ID2('W', 'S'),  /* WorkSpace */
  ID_LP = MAKE_ID2('L', 'P'),  /* LightProbe */
  ID_HA = MAKE_ID2('H', 'A'),  /* Hair */
  ID_PT = MAKE_ID2('P', 'T'),  /* PointCloud */
  ID_VO = MAKE_ID2('V', 'O'),  /* Volume */
  ID_SIM = MAKE_ID2('S', 'I'), /* Simulation */
} ID_Type;

/* Only used as 'placeholder' in .blend files for directly linked data-blocks. */
#define ID_LINK_PLACEHOLDER MAKE_ID2('I', 'D') /* (internal use only) */

/* Deprecated. */
#define ID_SCRN MAKE_ID2('S', 'N')

/* NOTE! Fake IDs, needed for g.sipo->blocktype or outliner */
#define ID_SEQ MAKE_ID2('S', 'Q')
/* constraint */
#define ID_CO MAKE_ID2('C', 'O')
/* pose (action channel, used to be ID_AC in code, so we keep code for backwards compat) */
#define ID_PO MAKE_ID2('A', 'C')
/* used in outliner... */
#define ID_NLA MAKE_ID2('N', 'L')
/* fluidsim Ipo */
#define ID_FLUIDSIM MAKE_ID2('F', 'S')

#define ID_FAKE_USERS(id) ((((ID *)id)->flag & LIB_FAKEUSER) ? 1 : 0)
#define ID_REAL_USERS(id) (((ID *)id)->us - ID_FAKE_USERS(id))
#define ID_EXTRA_USERS(id) (((ID *)id)->tag & LIB_TAG_EXTRAUSER ? 1 : 0)

#define ID_CHECK_UNDO(id) \
  ((GS((id)->name) != ID_SCR) && (GS((id)->name) != ID_WM) && (GS((id)->name) != ID_WS))

#define ID_BLEND_PATH(_bmain, _id) \
  ((_id)->lib ? (_id)->lib->filepath : BKE_main_blendfile_path((_bmain)))
#define ID_BLEND_PATH_FROM_GLOBAL(_id) \
  ((_id)->lib ? (_id)->lib->filepath : BKE_main_blendfile_path_from_global())

#define ID_MISSING(_id) ((((ID *)(_id))->tag & LIB_TAG_MISSING) != 0)

#define ID_IS_LINKED(_id) (((ID *)(_id))->lib != NULL)

/* Note that this is a fairly high-level check, should be used at user interaction level, not in
 * BKE_library_override typically (especially due to the check on LIB_TAG_EXTERN). */
#define ID_IS_OVERRIDABLE_LIBRARY(_id) \
  (ID_IS_LINKED(_id) && !ID_MISSING(_id) && (((ID *)(_id))->tag & LIB_TAG_EXTERN) != 0)

#define ID_IS_OVERRIDE_LIBRARY(_id) \
  (((ID *)(_id))->override_library != NULL && ((ID *)(_id))->override_library->reference != NULL)

#define ID_IS_OVERRIDE_LIBRARY_TEMPLATE(_id) \
  (((ID *)(_id))->override_library != NULL && ((ID *)(_id))->override_library->reference == NULL)

/* Check whether datablock type is covered by copy-on-write. */
#define ID_TYPE_IS_COW(_id_type) (!ELEM(_id_type, ID_BR, ID_PAL, ID_IM))

#ifdef GS
#  undef GS
#endif
#define GS(a) \
  (CHECK_TYPE_ANY(a, char *, const char *, char[66], const char[66]), \
   (ID_Type)(*((const short *)(a))))

#define ID_NEW_SET(_id, _idn) \
  (((ID *)(_id))->newid = (ID *)(_idn), \
   ((ID *)(_id))->newid->tag |= LIB_TAG_NEW, \
   (void *)((ID *)(_id))->newid)
#define ID_NEW_REMAP(a) \
  if ((a) && (a)->id.newid) \
  (a) = (void *)(a)->id.newid

/** id->flag (persitent). */
enum {
  /** Don't delete the datablock even if unused. */
  LIB_FAKEUSER = 1 << 9,
  /**
   * The data-block is a sub-data of another one.
   * Direct persistent references are not allowed.
   */
  LIB_EMBEDDED_DATA = 1 << 10,
  /**
   * Datablock is from a library and linked indirectly, with LIB_TAG_INDIRECT
   * tag set. But the current .blend file also has a weak pointer to it that
   * we want to restore if possible, and silently drop if it's missing.
   */
  LIB_INDIRECT_WEAK_LINK = 1 << 11,
};

/**
 * id->tag (runtime-only).
 *
 * Those flags belong to three different categories,
 * which have different expected handling in code:
 *
 * - RESET_BEFORE_USE: piece of code that wants to use such flag
 *   has to ensure they are properly 'reset' first.
 * - RESET_AFTER_USE: piece of code that wants to use such flag has to ensure they are properly
 *   'reset' after usage
 *   (though 'lifetime' of those flags is a bit fuzzy, e.g. _RECALC ones are reset on depsgraph
 *   evaluation...).
 * - RESET_NEVER: those flags are 'status' one, and never actually need any reset
 *   (except on initialization during .blend file reading).
 */
enum {
  /* RESET_NEVER Datablock is from current .blend file. */
  LIB_TAG_LOCAL = 0,
  /* RESET_NEVER Datablock is from a library,
   * but is used (linked) directly by current .blend file. */
  LIB_TAG_EXTERN = 1 << 0,
  /* RESET_NEVER Datablock is from a library,
   * and is only used (linked) indirectly through other libraries. */
  LIB_TAG_INDIRECT = 1 << 1,

  /* RESET_AFTER_USE Flag used internally in readfile.c,
   * to mark IDs needing to be expanded (only done once). */
  LIB_TAG_NEED_EXPAND = 1 << 3,
  /* RESET_AFTER_USE Flag used internally in readfile.c to mark ID
   * placeholders for linked data-blocks needing to be read. */
  LIB_TAG_ID_LINK_PLACEHOLDER = 1 << 4,
  /* RESET_AFTER_USE */
  LIB_TAG_NEED_LINK = 1 << 5,

  /* RESET_NEVER tag data-block as a place-holder
   * (because the real one could not be linked from its library e.g.). */
  LIB_TAG_MISSING = 1 << 6,

  /* RESET_NEVER tag data-block as being up-to-date regarding its reference. */
  LIB_TAG_OVERRIDE_LIBRARY_REFOK = 1 << 9,
  /* RESET_NEVER tag data-block as needing an auto-override execution, if enabled. */
  LIB_TAG_OVERRIDE_LIBRARY_AUTOREFRESH = 1 << 17,

  /* tag data-block as having an extra user. */
  LIB_TAG_EXTRAUSER = 1 << 2,
  /* tag data-block as having actually increased usercount for the extra virtual user. */
  LIB_TAG_EXTRAUSER_SET = 1 << 7,

  /* RESET_AFTER_USE tag newly duplicated/copied IDs.
   * Also used internally in readfile.c to mark data-blocks needing do_versions. */
  LIB_TAG_NEW = 1 << 8,
  /* RESET_BEFORE_USE free test flag.
   * TODO make it a RESET_AFTER_USE too. */
  LIB_TAG_DOIT = 1 << 10,
  /* RESET_AFTER_USE tag existing data before linking so we know what is new. */
  LIB_TAG_PRE_EXISTING = 1 << 11,

  /* The data-block is a copy-on-write/localized version. */
  LIB_TAG_COPIED_ON_WRITE = 1 << 12,
  LIB_TAG_COPIED_ON_WRITE_EVAL_RESULT = 1 << 13,
  LIB_TAG_LOCALIZED = 1 << 14,

  /* RESET_NEVER tag data-block for freeing etc. behavior
   * (usually set when copying real one into temp/runtime one). */
  LIB_TAG_NO_MAIN = 1 << 15,          /* Datablock is not listed in Main database. */
  LIB_TAG_NO_USER_REFCOUNT = 1 << 16, /* Datablock does not refcount usages of other IDs. */
  /* Datablock was not allocated by standard system (BKE_libblock_alloc), do not free its memory
   * (usual type-specific freeing is called though). */
  LIB_TAG_NOT_ALLOCATED = 1 << 18,

  /* RESET_AFTER_USE Used by undo system to tag unchanged IDs re-used from old Main (instead of
   * read from memfile). */
  LIB_TAG_UNDO_OLD_ID_REUSED = 1 << 19,
};

/* Tag given ID for an update in all the dependency graphs. */
typedef enum IDRecalcFlag {
  /***************************************************************************
   * Individual update tags, this is what ID gets tagged for update with. */

  /* ** Object transformation changed. ** */
  ID_RECALC_TRANSFORM = (1 << 0),

  /* ** Object geometry changed. **
   *
   * When object of armature type gets tagged with this flag, it's pose is
   * re-evaluated.
   * When object of other type is tagged with this flag it makes the modifier
   * stack to be re-evaluated.
   * When object data type (mesh, curve, ...) gets tagged with this flag it
   * makes all objects which shares this data-block to be updated. */
  ID_RECALC_GEOMETRY = (1 << 1),

  /* ** Animation or time changed and animation is to be re-evaluated. ** */
  ID_RECALC_ANIMATION = (1 << 2),

  /* ** Particle system changed. ** */
  /* Only do pathcache etc. */
  ID_RECALC_PSYS_REDO = (1 << 3),
  /* Reset everything including pointcache. */
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
  /* TODO(sergey): Consider adding an explicit ID_RECALC_SHADING_PARAMATERS
   * which can be used for cases when only socket value changed, to speed up
   * redraw update in that case. */

  /* Selection of the ID itself or its components (for example, vertices) did
   * change, and all the drawing data is to eb updated. */
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

  /* ** Update copy on write component. **
   * This is most generic tag which should only be used when nothing else
   * matches.
   */
  ID_RECALC_COPY_ON_WRITE = (1 << 13),

  /* Sequences in the sequencer did change.
   * Use this tag with a scene ID which owns the sequences. */
  ID_RECALC_SEQUENCER_STRIPS = (1 << 14),

  ID_RECALC_AUDIO_SEEK = (1 << 15),
  ID_RECALC_AUDIO_FPS = (1 << 16),
  ID_RECALC_AUDIO_VOLUME = (1 << 17),
  ID_RECALC_AUDIO_MUTE = (1 << 18),
  ID_RECALC_AUDIO_LISTENER = (1 << 19),

  ID_RECALC_AUDIO = (1 << 20),

  ID_RECALC_PARAMETERS = (1 << 21),

  /* Makes it so everything what depends on time.
   * Basically, the same what changing frame in a timeline will do. */
  ID_RECALC_TIME = (1 << 22),

  /* Input has changed and datablock is to be reload from disk.
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

  /***************************************************************************
   * Pseudonyms, to have more semantic meaning in the actual code without
   * using too much low-level and implementation specific tags. */

  /* Update animation data-block itself, without doing full re-evaluation of
   * all dependent objects. */
  ID_RECALC_ANIMATION_NO_FLUSH = ID_RECALC_COPY_ON_WRITE,

  /***************************************************************************
   * Aggregate flags, use only for checks on runtime.
   * Do NOT use those for tagging. */

  /* Identifies that SOMETHING has been changed in this ID. */
  ID_RECALC_ALL = ~(0),
  /* Identifies that something in particle system did change. */
  ID_RECALC_PSYS_ALL = (ID_RECALC_PSYS_REDO | ID_RECALC_PSYS_RESET | ID_RECALC_PSYS_CHILD |
                        ID_RECALC_PSYS_PHYS),

} IDRecalcFlag;

/* To filter ID types (filter_id). 64 bit to fit all types. */
#define FILTER_ID_AC (1ULL << 0)
#define FILTER_ID_AR (1ULL << 1)
#define FILTER_ID_BR (1ULL << 2)
#define FILTER_ID_CA (1ULL << 3)
#define FILTER_ID_CU (1ULL << 4)
#define FILTER_ID_GD (1ULL << 5)
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
#define FILTER_ID_HA (1ULL << 32)
#define FILTER_ID_PT (1ULL << 33)
#define FILTER_ID_VO (1ULL << 34)
#define FILTER_ID_SIM (1ULL << 35)

#define FILTER_ID_ALL \
  (FILTER_ID_AC | FILTER_ID_AR | FILTER_ID_BR | FILTER_ID_CA | FILTER_ID_CU | FILTER_ID_GD | \
   FILTER_ID_GR | FILTER_ID_IM | FILTER_ID_LA | FILTER_ID_LS | FILTER_ID_LT | FILTER_ID_MA | \
   FILTER_ID_MB | FILTER_ID_MC | FILTER_ID_ME | FILTER_ID_MSK | FILTER_ID_NT | FILTER_ID_OB | \
   FILTER_ID_PA | FILTER_ID_PAL | FILTER_ID_PC | FILTER_ID_SCE | FILTER_ID_SPK | FILTER_ID_SO | \
   FILTER_ID_TE | FILTER_ID_TXT | FILTER_ID_VF | FILTER_ID_WO | FILTER_ID_CF | FILTER_ID_WS | \
   FILTER_ID_LP | FILTER_ID_HA | FILTER_ID_PT | FILTER_ID_VO | FILTER_ID_SIM)

/* IMPORTANT: this enum matches the order currently use in set_listbasepointers,
 * keep them in sync! */
enum {
  INDEX_ID_LI = 0,
  INDEX_ID_IP,
  INDEX_ID_AC,
  INDEX_ID_KE,
  INDEX_ID_PAL,
  INDEX_ID_GD,
  INDEX_ID_NT,
  INDEX_ID_IM,
  INDEX_ID_TE,
  INDEX_ID_MA,
  INDEX_ID_VF,
  INDEX_ID_AR,
  INDEX_ID_CF,
  INDEX_ID_ME,
  INDEX_ID_CU,
  INDEX_ID_MB,
  INDEX_ID_HA,
  INDEX_ID_PT,
  INDEX_ID_VO,
  INDEX_ID_LT,
  INDEX_ID_LA,
  INDEX_ID_CA,
  INDEX_ID_TXT,
  INDEX_ID_SO,
  INDEX_ID_GR,
  INDEX_ID_PC,
  INDEX_ID_BR,
  INDEX_ID_PA,
  INDEX_ID_SPK,
  INDEX_ID_LP,
  INDEX_ID_WO,
  INDEX_ID_MC,
  INDEX_ID_SCR,
  INDEX_ID_OB,
  INDEX_ID_LS,
  INDEX_ID_SCE,
  INDEX_ID_WS,
  INDEX_ID_WM,
  INDEX_ID_MSK,
  INDEX_ID_SIM,
  INDEX_ID_NULL,
  INDEX_ID_MAX,
};

#ifdef __cplusplus
}
#endif

#endif

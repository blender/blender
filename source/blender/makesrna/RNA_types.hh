/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

/* Use a define instead of `#pragma once` because of `BKE_addon.h`, `ED_object.hh` & others. */
#ifndef __RNA_TYPES_H__
#define __RNA_TYPES_H__

#include <optional>
#include <string>

#include "../blenlib/BLI_enum_flags.hh"
#include "../blenlib/BLI_function_ref.hh"
#include "../blenlib/BLI_sys_types.h"
#include "../blenlib/BLI_vector.hh"

struct BlenderRNA;
struct FunctionRNA;
struct ID;
struct Main;
struct ParameterList;
struct PropertyRNA;
struct ReportList;
struct StructRNA;
struct bContext;

/**
 * An ancestor of a given PointerRNA. The owner ID is not needed here, it is assumed to always be
 * the same as the owner ID of the PropertyRNA itself.
 */
struct AncestorPointerRNA {
  StructRNA *type;
  void *data;
};
/** Allows to benefit from the `max_full_copy_size` optimization on copy of #blender::Vector. */
constexpr int64_t ANCESTOR_POINTERRNA_DEFAULT_SIZE = 2;

/**
 * Pointer
 *
 * RNA pointers are not a single C pointer but include the type,
 * and a pointer to the ID struct that owns the struct, since
 * in some cases this information is needed to correctly get/set
 * the properties and validate them. */

struct PointerRNA {
  ID *owner_id = nullptr;
  StructRNA *type = nullptr;
  void *data = nullptr;

  /**
   * A chain of ancestors of this PointerRNA, if known. The last item is the closest ancestor.
   *
   * E.g. Parsing `vgroup = C.object.data.vertices[0].groups[0]` would result in the PointerRNA of
   * `vgroup` having two ancestors: `vertices[0]` and `data` (aka the Mesh ID).
   *
   * By definition, PointerRNA of IDs are currently always 'discrete', i.e. do not have ancestors
   * information, since an ID PointerRNA should always be its own root.
   *
   * \note: Currently, it is assumed that embedded or evaluated IDs can also be discrete
   * PointerRNA. This should be fine, since they should all have their 'owner ID' or 'orig ID'
   * pointer info. This may become a problem e.g. if in the future we allow embedded IDs into
   * sub-structs of IDs.
   *
   * There is no guarantee that this chain is always (fully) valid and will lead to the root owner
   * of the wrapped data (an ID). Depending on how the PointerRNA was created, and the available
   * information at that time, it could be empty or only feature a partial ancestors chain. This
   * can happen if the initial pointer is created as discrete (e.g. from an operator that does not
   * have access to/knowledge of the whole ancestor chain), and a sub-struct is accessed through
   * regular RNA property access (like a call to RNA_property_pointer_get etc.).
   */
  blender::Vector<AncestorPointerRNA, ANCESTOR_POINTERRNA_DEFAULT_SIZE> ancestors = {};

  PointerRNA() = default;
  PointerRNA(const PointerRNA &) = default;
  PointerRNA(PointerRNA &&) = default;
  PointerRNA &operator=(const PointerRNA &other) = default;
  PointerRNA &operator=(PointerRNA &&other) = default;

  PointerRNA(ID *owner_id, StructRNA *type, void *data)
      : owner_id(owner_id), type(type), data(data), ancestors{}
  {
  }
  PointerRNA(ID *owner_id, StructRNA *type, void *data, const PointerRNA &parent)
      : owner_id(owner_id), type(type), data(data), ancestors(parent.ancestors)
  {
    this->ancestors.append({parent.type, parent.data});
  }
  PointerRNA(ID *owner_id, StructRNA *type, void *data, blender::Span<AncestorPointerRNA> parents)
      : owner_id(owner_id), type(type), data(data), ancestors(parents)
  {
  }

  /** Reset the pointer to its initial empty state, such that it equals to PointerRNA_NULL. */
  void reset()
  {
    *this = {};
  }

  /**
   * Make the pointer invalid.
   *
   * This is especially important for the Python API, as any access to an invalid PointerRNA should
   * raise an exception in `bpy` code.
   */
  void invalidate()
  {
    this->reset();
  }

  /**
   * Get the data as a specific type. This expects that the caller knows what the type is and has
   * undefined behavior otherwise. Using this method is less verbose than casting the type at the
   * call-site and allows us to potentially add run-time type checks in the future.
   *
   * This method is intentionally const while still returning a non-const pointer. This is because
   * the constness of the `PointerRNA` is not propagated to the data it references. One can always
   * just copy the `PointerRNA` to get a non-const version of it.
   */
  template<typename T> T *data_as() const
  {
    return static_cast<T *>(this->data);
  }

  /**
   * Get the immediate parent pointer, if any.
   */
  PointerRNA parent() const
  {
    if (ancestors.is_empty()) {
      return PointerRNA();
    }

    return PointerRNA(owner_id, ancestors.last().type, ancestors.last().data);
  }
};

extern const PointerRNA PointerRNA_NULL;

struct PropertyPointerRNA {
  PointerRNA ptr = {};
  PropertyRNA *prop = nullptr;
};

/**
 * Stored result of a RNA path lookup (as used by anim-system)
 */
struct PathResolvedRNA {
  PointerRNA ptr = {};
  PropertyRNA *prop = nullptr;
  /** -1 for non-array access. */
  int prop_index = -1;
};

/* Property */

enum PropertyType {
  PROP_BOOLEAN = 0,
  PROP_INT = 1,
  PROP_FLOAT = 2,
  PROP_STRING = 3,
  PROP_ENUM = 4,
  PROP_POINTER = 5,
  PROP_COLLECTION = 6,
};

/* also update rna_property_subtype_unit when you change this */
enum PropertyUnit {
  PROP_UNIT_NONE = (0 << 16),
  PROP_UNIT_LENGTH = (1 << 16),             /* m */
  PROP_UNIT_AREA = (2 << 16),               /* m^2 */
  PROP_UNIT_VOLUME = (3 << 16),             /* m^3 */
  PROP_UNIT_MASS = (4 << 16),               /* kg */
  PROP_UNIT_ROTATION = (5 << 16),           /* radians */
  PROP_UNIT_TIME = (6 << 16),               /* frame */
  PROP_UNIT_TIME_ABSOLUTE = (7 << 16),      /* time in seconds (independent of scene) */
  PROP_UNIT_VELOCITY = (8 << 16),           /* m/s */
  PROP_UNIT_ACCELERATION = (9 << 16),       /* m/(s^2) */
  PROP_UNIT_CAMERA = (10 << 16),            /* mm */
  PROP_UNIT_POWER = (11 << 16),             /* W */
  PROP_UNIT_TEMPERATURE = (12 << 16),       /* C */
  PROP_UNIT_WAVELENGTH = (13 << 16),        /* `nm` (independent of scene). */
  PROP_UNIT_COLOR_TEMPERATURE = (14 << 16), /* K */
  PROP_UNIT_FREQUENCY = (15 << 16),         /* Hz */
};
ENUM_OPERATORS(PropertyUnit)

/**
 * Use values besides #PROP_SCALE_LINEAR
 * so the movement of the mouse doesn't map linearly to the value of the slider.
 *
 * For some settings it's useful to space motion in a non-linear way, see #77868.
 *
 * NOTE: The scale types are available for all float sliders.
 * For integer sliders they are only available if they use the visible value bar.
 * Sliders with logarithmic scale and value bar must have a range > 0
 * while logarithmic sliders without the value bar can have a range of >= 0.
 */
enum PropertyScaleType {
  /** Linear scale (default). */
  PROP_SCALE_LINEAR = 0,
  /**
   * Logarithmic scale
   * - Maximum range: `0 <= x < inf`
   */
  PROP_SCALE_LOG = 1,
  /**
   * Cubic scale.
   * - Maximum range: `-inf < x < inf`
   */
  PROP_SCALE_CUBIC = 2,
};

#define RNA_SUBTYPE_UNIT(subtype) ((subtype) & 0x00FF0000)
#define RNA_SUBTYPE_VALUE(subtype) ((subtype) & ~0x00FF0000)
#define RNA_SUBTYPE_UNIT_VALUE(subtype) ((subtype) >> 16)

#define RNA_ENUM_BITFLAG_SIZE 32

#define RNA_TRANSLATION_PREC_DEFAULT 5

#define RNA_STACK_ARRAY 32

/**
 * \note Also update enums in `rna_rna.cc` when adding items here.
 * Watch it: these values are written to files as part of node socket button sub-types!
 */
enum PropertySubType {
  PROP_NONE = 0,

  /* strings */
  PROP_FILEPATH = 1,
  PROP_DIRPATH = 2,
  PROP_FILENAME = 3,
  /** A string which should be represented as bytes in python, NULL terminated though. */
  PROP_BYTESTRING = 4,
  /* 5 was used by "PROP_TRANSLATE" sub-type, which is now a flag. */
  /** A string which should not be displayed in UI. */
  PROP_PASSWORD = 6,

  /* numbers */
  /** A dimension in pixel units, possibly before DPI scaling (so value may not be the final pixel
   * value but the one to apply DPI scale to). */
  PROP_PIXEL = 12,
  PROP_UNSIGNED = 13,
  PROP_PERCENTAGE = 14,
  PROP_FACTOR = 15,
  PROP_ANGLE = 16 | PROP_UNIT_ROTATION,
  PROP_TIME = 17 | PROP_UNIT_TIME,
  PROP_TIME_ABSOLUTE = 17 | PROP_UNIT_TIME_ABSOLUTE,
  /** Distance in 3d space, don't use for pixel distance for eg. */
  PROP_DISTANCE = 18 | PROP_UNIT_LENGTH,
  PROP_DISTANCE_CAMERA = 19 | PROP_UNIT_CAMERA,

  /* number arrays */
  PROP_COLOR = 20,
  PROP_TRANSLATION = 21 | PROP_UNIT_LENGTH,
  PROP_DIRECTION = 22,
  PROP_VELOCITY = 23 | PROP_UNIT_VELOCITY,
  PROP_ACCELERATION = 24 | PROP_UNIT_ACCELERATION,
  PROP_MATRIX = 25,
  PROP_EULER = 26 | PROP_UNIT_ROTATION,
  PROP_QUATERNION = 27,
  PROP_AXISANGLE = 28,
  PROP_XYZ = 29,
  PROP_XYZ_LENGTH = 29 | PROP_UNIT_LENGTH,
  /** Used for colors which would be color managed before display. */
  PROP_COLOR_GAMMA = 30,
  /** Generic array, no units applied, only that x/y/z/w are used (Python vector). */
  PROP_COORDS = 31,

  /* booleans */
  PROP_LAYER = 40,
  PROP_LAYER_MEMBER = 41,

  /** Light */
  PROP_POWER = 42 | PROP_UNIT_POWER,

  /* temperature */
  PROP_TEMPERATURE = 43 | PROP_UNIT_TEMPERATURE,

  /* wavelength */
  PROP_WAVELENGTH = 44 | PROP_UNIT_WAVELENGTH,

  /* wavelength */
  PROP_COLOR_TEMPERATURE = 45 | PROP_UNIT_COLOR_TEMPERATURE,

  PROP_FREQUENCY = 46 | PROP_UNIT_FREQUENCY,
  PROP_PIXEL_DIAMETER = 47,
  PROP_DISTANCE_DIAMETER = 48 | PROP_UNIT_LENGTH,
};

/* Make sure enums are updated with these */
/* HIGHEST FLAG IN USE: 1u << 31
 * FREE FLAGS: 13. */
enum PropertyFlag {
  /**
   * Editable means the property is editable in the user
   * interface, properties are editable by default except
   * for pointers and collections.
   */
  PROP_EDITABLE = (1 << 0),
  /**
   * This property is editable even if it is lib linked,
   * meaning it will get lost on reload, but it's useful
   * for editing.
   */
  PROP_LIB_EXCEPTION = (1 << 16),
  /**
   * Animatable means the property can be driven by some
   * other input, be it animation curves, expressions, ..
   * properties are animatable by default except for pointers
   * and collections.
   */
  PROP_ANIMATABLE = (1 << 1),
  /**
   * This flag means when the property's widget is in 'text-edit' mode, it will be updated
   * after every typed char, instead of waiting final validation. Used e.g. for text search-box.
   * It will also cause UI_BUT_VALUE_CLEAR to be set for text buttons. We could add a separate flag
   * for search/filter properties, but this works just fine for now.
   */
  PROP_TEXTEDIT_UPDATE = (1u << 31),

  /* icon */
  PROP_ICONS_CONSECUTIVE = (1 << 12),
  PROP_ICONS_REVERSE = (1 << 8),

  /**
   * Hide in the user interface. That is, from auto-generated operator property UIs (like the
   * redo panel) and the outliner "Data API" display mode. Does not hide it in the keymap UI.
   *
   * Also don't save in presets, as if #PROP_SKIP_PRESET was set.
   */
  PROP_HIDDEN = (1 << 19),
  /**
   * Doesn't preserve the last value for repeated operator calls.
   *
   * Also don't save in presets, as if #PROP_SKIP_PRESET was set.
   */
  PROP_SKIP_SAVE = (1 << 28),

  /* numbers */

  /** Each value is related proportionally (object scale, image size). */
  PROP_PROPORTIONAL = (1 << 26),

  /* pointers */

  /**
   * Mark this property as handling ID user count.
   *
   * This is done automatically by the auto-generated setter function. If an RNA property has a
   * custom setter, it's the setter's responsibility to correctly update the user count.
   *
   * \note In most basic cases, makesrna will automatically set this flag, based on the
   * `STRUCT_ID_REFCOUNT` flag of the defined pointer type. This only works if makesrna can find a
   * matching DNA property though, 'virtual' RNA properties (using both a getter and setter) will
   * never get this flag defined automatically.
   */
  PROP_ID_REFCOUNT = (1 << 6),

  /**
   * Disallow assigning a variable to itself, eg an object tracking itself
   * only apply this to types that are derived from an ID ().
   */
  PROP_ID_SELF_CHECK = (1 << 20),
  /**
   * Use for...
   * - pointers: in the UI and python so unsetting or setting to None won't work.
   * - strings: so our internal generated get/length/set
   *   functions know to do NULL checks before access #30865.
   */
  PROP_NEVER_NULL = (1 << 18),
  /**
   * Currently only used for UI, this is similar to PROP_NEVER_NULL
   * except that the value may be NULL at times, used for ObData, where an Empty's will be NULL
   * but setting NULL on a mesh object is not possible.
   * So if it's not NULL, setting NULL can't be done!
   */
  PROP_NEVER_UNLINK = (1 << 25),

  /**
   * Pointers to data that is not owned by the struct.
   * Typical example: Bone.parent, Bone.child, etc., and nearly all ID pointers.
   * This is crucial information for processes that walk the whole data of an ID e.g.
   * (like library override).
   * Note that all ID pointers are enforced to this by default,
   * this probably will need to be rechecked
   * (see ugly infamous node-trees of material/texture/scene/etc.).
   */
  PROP_PTR_NO_OWNERSHIP = (1 << 7),

  /**
   * flag contains multiple enums.
   * NOTE: not to be confused with `prop->enumbitflags`
   * this exposes the flag as multiple options in python and the UI.
   *
   * \note These can't be animated so use with care.
   */
  PROP_ENUM_FLAG = (1 << 21),

  /* need context for update function */
  PROP_CONTEXT_UPDATE = (1 << 22),
  PROP_CONTEXT_PROPERTY_UPDATE = PROP_CONTEXT_UPDATE | (1 << 27),

  /* registering */
  PROP_REGISTER = (1 << 4),
  PROP_REGISTER_OPTIONAL = PROP_REGISTER | (1 << 5),

  /**
   * Use for allocated function return values of arrays or strings
   * for any data that should not have a reference kept.
   *
   * It can be used for properties which are dynamically allocated too.
   *
   * \note Currently dynamic sized thick wrapped data isn't supported.
   * This would be a useful addition and avoid a fixed maximum sized as in done at the moment.
   */
  PROP_THICK_WRAP = (1 << 23),

  /** This is an IDProperty, not a DNA one. */
  PROP_IDPROPERTY = (1 << 10),
  /** For dynamic arrays & return values of type string. */
  PROP_DYNAMIC = (1 << 17),
  /** For enum that shouldn't be contextual */
  PROP_ENUM_NO_CONTEXT = (1 << 24),
  /** For enums not to be translated (e.g. view-layers' names in nodes). */
  PROP_ENUM_NO_TRANSLATE = (1 << 29),

  /**
   * Don't do dependency graph tag from a property update callback.
   * Use this for properties which defines interface state, for example,
   * properties which denotes whether modifier panel is collapsed or not.
   */
  PROP_NO_DEG_UPDATE = (1 << 30),

  /**
   * Property needs to ensure evaluated data-blocks are in sync with their original counter-part
   * but the property does not affect evaluation itself.
   */
  PROP_DEG_SYNC_ONLY = (1 << 9),

  /**
   * File-paths that refer to output get a special treatment such
   * as having the +/- operators available in the file browser.
   */
  PROP_PATH_OUTPUT = (1 << 2),
  /**
   * Path supports relative prefix: `//`,
   * paths which don't support the relative suffix show a warning if the suffix is used.
   */
  PROP_PATH_SUPPORTS_BLEND_RELATIVE = (1 << 15),

  /**
   * Paths that are evaluated with templating.
   *
   * Note that this doesn't cause the property to support templating, but rather
   * *indicates* to other parts of Blender whether it supports templating.
   * Support for templating needs to be manually implemented.
   *
   * When this is set, the property's `path_template_type` field should also be
   * set.
   *
   * \see The top-level documentation of BKE_path_templates.hh.
   */
  PROP_PATH_SUPPORTS_TEMPLATES = (1 << 14),

  /** Do not write in presets (#PROP_HIDDEN and #PROP_SKIP_SAVE won't either). */
  PROP_SKIP_PRESET = (1 << 11),
};
ENUM_OPERATORS(PropertyFlag)

/**
 * For properties that support path templates, this indicates which
 * purpose-specific variables (if any) should be available to them and how those
 * variables should be built.
 *
 * \see The top-level documentation of BKE_path_templates.hh.
 */
enum PropertyPathTemplateType {
  /* Only supports general and type-specific variables, no purpose-specific
   * variables. */
  PROP_VARIABLES_NONE = 0,

  /* Supports render output variables.
   *
   * \see BKE_add_template_variables_for_render_path() */
  PROP_VARIABLES_RENDER_OUTPUT,
};

/**
 * Flags related to comparing and overriding RNA properties.
 * Make sure enums are updated with these.
 *
 * FREE FLAGS: 2, 3, 4, 5, 6, 7, 8, 9, 12 and above.
 */
enum PropertyOverrideFlag {
  /** Means that the property can be overridden by a local override of some linked datablock. */
  PROPOVERRIDE_OVERRIDABLE_LIBRARY = (1 << 0),

  /**
   * Forbid usage of this property in comparison (& hence override) code.
   * Useful e.g. for collections of data like mesh's geometry, particles, etc.
   * Also for runtime data that should never be considered as part of actual Blend data (e.g.
   * depsgraph from ViewLayers...).
   */
  PROPOVERRIDE_NO_COMPARISON = (1 << 1),

  /**
   * Means the property can be fully ignored by override process.
   * Unlike NO_COMPARISON, it can still be used by diffing code, but no override operation will be
   * created for it, and no attempt to restore the data from linked reference either.
   *
   * WARNING: This flag should be used with a lot of caution, as it completely bypasses override
   * system. It is currently only used for ID's names, since we cannot prevent local override to
   * get a different name from the linked reference, and ID names are 'rna name property' (i.e. are
   * used in overrides of collections of IDs). See also `BKE_lib_override_library_update()` where
   * we deal manually with the value of that property at DNA level. */
  PROPOVERRIDE_IGNORE = (1 << 2),

  /*** Collections-related ***/

  /** The property supports insertion (collections only). */
  PROPOVERRIDE_LIBRARY_INSERTION = (1 << 10),

  /**
   * Only use indices to compare items in the property, never names (collections only).
   *
   * Useful when #StructRNA::nameproperty of the items is generated from other data
   * (e.g. name of material slots is actually name of assigned material).
   */
  PROPOVERRIDE_NO_PROP_NAME = (1 << 11),
};
ENUM_OPERATORS(PropertyOverrideFlag);

/**
 * Function parameters flags.
 * \warning 16bits only.
 */
enum ParameterFlag {
  PARM_REQUIRED = (1 << 0),
  PARM_OUTPUT = (1 << 1),
  PARM_RNAPTR = (1 << 2),
  /**
   * This allows for non-breaking API updates when adding non-critical new parameters
   * to functions which Python classes register.
   * This way, old Python code defining functions without that parameter would still work.
   *
   * WARNING: any parameter after the first #PARM_PYFUNC_REGISTER_OPTIONAL
   * one will be considered as optional!
   * \note only for input parameters!
   */
  PARM_PYFUNC_REGISTER_OPTIONAL = (1 << 3),
};
ENUM_OPERATORS(ParameterFlag)

struct CollectionPropertyIterator;
struct Link;
using IteratorSkipFunc = bool (*)(CollectionPropertyIterator *iter, void *data);

struct ListBaseIterator {
  Link *link;
  int flag;
  IteratorSkipFunc skip;
};

struct ArrayIterator {
  char *ptr;
  /** Past the last valid pointer, only for comparisons, ignores skipped values. */
  char *endptr;
  /** Will be freed if set. */
  void *free_ptr;
  int itemsize;

  /**
   * Array length with no skip functions applied,
   * take care not to compare against index from animsys or Python indices.
   */
  int length;

  /**
   * Optional skip function,
   * when set the array as viewed by rna can contain only a subset of the members.
   * this changes indices so quick array index lookups are not possible when skip function is used.
   */
  IteratorSkipFunc skip;
};

struct CountIterator {
  void *ptr;
  int item;
};

struct CollectionPropertyIterator {
  /* internal */
  PointerRNA parent;
  PointerRNA builtin_parent;
  PropertyRNA *prop;
  union {
    /* Keep biggest object first in the union, for zero-initialization to work properly. */
    ArrayIterator array;
    ListBaseIterator listbase;
    CountIterator count;
    void *custom;
  } internal;
  int idprop;
  int level;

  /* external */
  PointerRNA ptr;
  bool valid;
};

struct CollectionVector {
  blender::Vector<PointerRNA> items;
};

enum RawPropertyType {
  PROP_RAW_UNSET = -1,
  PROP_RAW_INT, /* XXX: abused for types that are not set, eg. MFace.verts, needs fixing. */
  PROP_RAW_SHORT,
  PROP_RAW_CHAR,
  PROP_RAW_BOOLEAN,
  PROP_RAW_DOUBLE,
  PROP_RAW_FLOAT,
  PROP_RAW_UINT8,
  PROP_RAW_UINT16,
  PROP_RAW_INT64,
  PROP_RAW_UINT64,
  PROP_RAW_INT8,
};

struct RawArray {
  void *array;
  RawPropertyType type;
  int len;
  int stride;
};

/**
 * This struct is typically defined in arrays which define an *enum* for RNA,
 * which is used by the RNA API both for user-interface and the Python API.
 */
struct EnumPropertyItem {
  /** The internal value of the enum, not exposed to users. */
  int value;
  /**
   * Note that identifiers must be unique within the array,
   * by convention they're upper case with underscores for separators.
   * - An empty string is used to define menu separators.
   * - NULL denotes the end of the array of items.
   */
  const char *identifier;
  /** Optional icon, typically 'ICON_NONE' */
  int icon;
  /** Name displayed in the interface. */
  const char *name;
  /** Longer description used in the interface. */
  const char *description;
};

/**
 * Heading for RNA enum items (shown in the UI).
 *
 * The description is currently only shown in the Python documentation.
 * By convention the value should be a non-empty string or NULL when there is no description
 * (never an empty string).
 */
#define RNA_ENUM_ITEM_HEADING(name, description) {0, "", 0, name, description}

/** Separator for RNA enum items (shown in the UI). */
#define RNA_ENUM_ITEM_SEPR {0, "", 0, NULL, NULL}

/** Separator for RNA enum that begins a new column in menus (shown in the UI). */
#define RNA_ENUM_ITEM_SEPR_COLUMN RNA_ENUM_ITEM_HEADING("", NULL)

/* Extended versions with PropertyRNA argument. Used in particular by the bpy code to wrap all the
 * py-defined callbacks when defining a property using `bpy.props` module.
 *
 * The 'Transform' ones allow to add a transform step (applied after getting, or before setting the
 * value), which only modifies the value, but does not handle actual storage. Currently only used
 * by `bpy`, more details in the documentation of #BPyPropStore.
 */
using BooleanPropertyGetFunc = bool (*)(PointerRNA *ptr, PropertyRNA *prop);
using BooleanPropertySetFunc = void (*)(PointerRNA *ptr, PropertyRNA *prop, bool value);
using BooleanPropertyGetTransformFunc = bool (*)(PointerRNA *ptr,
                                                 PropertyRNA *prop,
                                                 bool value,
                                                 bool is_set);
using BooleanPropertySetTransformFunc =
    bool (*)(PointerRNA *ptr, PropertyRNA *prop, bool new_value, bool curr_value, bool is_set);

using BooleanArrayPropertyGetFunc = void (*)(PointerRNA *ptr, PropertyRNA *prop, bool *r_values);
using BooleanArrayPropertySetFunc = void (*)(PointerRNA *ptr,
                                             PropertyRNA *prop,
                                             const bool *r_values);
using BooleanArrayPropertyGetTransformFunc = void (*)(
    PointerRNA *ptr, PropertyRNA *prop, const bool *curr_values, bool is_set, bool *r_values);
using BooleanArrayPropertySetTransformFunc = void (*)(PointerRNA *ptr,
                                                      PropertyRNA *prop,
                                                      const bool *new_values,
                                                      const bool *curr_values,
                                                      bool is_set,
                                                      bool *r_values);

using IntPropertyGetFunc = int (*)(PointerRNA *ptr, PropertyRNA *prop);
using IntPropertySetFunc = void (*)(PointerRNA *ptr, PropertyRNA *prop, int value);
using IntPropertyGetTransformFunc = int (*)(PointerRNA *ptr,
                                            PropertyRNA *prop,
                                            int value,
                                            bool is_set);
using IntPropertySetTransformFunc =
    int (*)(PointerRNA *ptr, PropertyRNA *prop, int new_value, int curr_value, bool is_set);
using IntArrayPropertyGetFunc = void (*)(PointerRNA *ptr, PropertyRNA *prop, int *values);
using IntArrayPropertySetFunc = void (*)(PointerRNA *ptr, PropertyRNA *prop, const int *values);
using IntArrayPropertyGetTransformFunc = void (*)(
    PointerRNA *ptr, PropertyRNA *prop, const int *curr_values, bool is_set, int *r_values);
using IntArrayPropertySetTransformFunc = void (*)(PointerRNA *ptr,
                                                  PropertyRNA *prop,
                                                  const int *new_values,
                                                  const int *curr_values,
                                                  bool is_set,
                                                  int *r_values);
using IntPropertyRangeFunc =
    void (*)(PointerRNA *ptr, PropertyRNA *prop, int *min, int *max, int *softmin, int *softmax);

using FloatPropertyGetFunc = float (*)(PointerRNA *ptr, PropertyRNA *prop);
using FloatPropertySetFunc = void (*)(PointerRNA *ptr, PropertyRNA *prop, float value);
using FloatPropertyGetTransformFunc = float (*)(PointerRNA *ptr,
                                                PropertyRNA *prop,
                                                float value,
                                                bool is_set);
using FloatPropertySetTransformFunc =
    float (*)(PointerRNA *ptr, PropertyRNA *prop, float new_value, float curr_value, bool is_set);
using FloatArrayPropertyGetFunc = void (*)(PointerRNA *ptr, PropertyRNA *prop, float *values);
using FloatArrayPropertySetFunc = void (*)(PointerRNA *ptr,
                                           PropertyRNA *prop,
                                           const float *values);
using FloatArrayPropertyGetTransformFunc = void (*)(
    PointerRNA *ptr, PropertyRNA *prop, const float *curr_values, bool is_set, float *r_values);
using FloatArrayPropertySetTransformFunc = void (*)(PointerRNA *ptr,
                                                    PropertyRNA *prop,
                                                    const float *new_values,
                                                    const float *curr_values,
                                                    bool is_set,
                                                    float *r_values);
using FloatPropertyRangeFunc = void (*)(
    PointerRNA *ptr, PropertyRNA *prop, float *min, float *max, float *softmin, float *softmax);

using StringPropertyGetFunc = std::string (*)(PointerRNA *ptr, PropertyRNA *prop);
using StringPropertyLengthFunc = int (*)(PointerRNA *ptr, PropertyRNA *prop);
using StringPropertySetFunc = void (*)(PointerRNA *ptr,
                                       PropertyRNA *prop,
                                       const std::string &value);
using StringPropertyGetTransformFunc = std::string (*)(PointerRNA *ptr,
                                                       PropertyRNA *prop,
                                                       const std::string &value,
                                                       bool is_set);
using StringPropertySetTransformFunc = std::string (*)(PointerRNA *ptr,
                                                       PropertyRNA *prop,
                                                       const std::string &new_value,
                                                       const std::string &curr_value,
                                                       bool is_set);

struct StringPropertySearchVisitParams {
  /** Text being searched for. */
  std::string text;
  /** Additional information to display. */
  std::optional<std::string> info;
  /* Optional icon instead of #ICON_NONE. */
  std::optional<int> icon_id;
};

enum eStringPropertySearchFlag {
  /**
   * Used so the result of #RNA_property_string_search_flag can be used to check
   * if search is supported.
   */
  PROP_STRING_SEARCH_SUPPORTED = (1 << 0),
  /** Items resulting from the search must be sorted. */
  PROP_STRING_SEARCH_SORT = (1 << 1),
  /**
   * Allow members besides the ones listed to be entered.
   *
   * \warning disabling this options causes the search callback to run on redraw and should
   * only be enabled this doesn't cause performance issues.
   */
  PROP_STRING_SEARCH_SUGGESTION = (1 << 2),
};
ENUM_OPERATORS(eStringPropertySearchFlag)

/**
 * \param C: context, may be NULL (in this case all available items should be shown).
 * \param ptr: RNA pointer.
 * \param prop: RNA property. This must have its #StringPropertyRNA.search callback set,
 * to check this use `RNA_property_string_search_flag(prop) & PROP_STRING_SEARCH_SUPPORTED`.
 * \param edit_text: Optionally use the string being edited by the user as a basis
 * for the search results (auto-complete Python attributes for example).
 * \param visit_fn: This function is called with every search candidate and is typically
 * responsible for storing the search results.
 */
using StringPropertySearchFunc =
    void (*)(const bContext *C,
             PointerRNA *ptr,
             PropertyRNA *prop,
             const char *edit_text,
             blender::FunctionRef<void(StringPropertySearchVisitParams)> visit_fn);

/**
 * Returns an optional glob pattern (e.g. `*.png`) that can be passed to the file browser to filter
 * valid files for this property.
 */
using StringPropertyPathFilterFunc = std::optional<std::string> (*)(const bContext *C,
                                                                    PointerRNA *ptr,
                                                                    PropertyRNA *prop);

using EnumPropertyGetFunc = int (*)(PointerRNA *ptr, PropertyRNA *prop);
using EnumPropertySetFunc = void (*)(PointerRNA *ptr, PropertyRNA *prop, int value);
using EnumPropertyGetTransformFunc = int (*)(PointerRNA *ptr,
                                             PropertyRNA *prop,
                                             int value,
                                             bool is_set);
using EnumPropertySetTransformFunc =
    int (*)(PointerRNA *ptr, PropertyRNA *prop, int new_value, int curr_value, bool is_set);
/* same as PropEnumItemFunc */
using EnumPropertyItemFunc = const EnumPropertyItem *(*)(bContext * C,
                                                         PointerRNA *ptr,
                                                         PropertyRNA *prop,
                                                         bool *r_free);

struct PropertyRNA;

/* Parameter List */

struct ParameterList {
  /** Storage for parameters*. */
  void *data;

  /** Function passed at creation time. */
  FunctionRNA *func;

  /** Store the parameter size. */
  int alloc_size;

  int arg_count, ret_count;
};

struct ParameterIterator {
  ParameterList *parms;
  // PointerRNA funcptr; /* UNUSED */
  void *data;
  int size, offset;

  PropertyRNA *parm;
  bool valid;
};

/** Mainly to avoid confusing casts. */
struct ParameterDynAlloc {
  /** Important, this breaks when set to an int. */
  intptr_t array_tot;
  void *array;
};

/* Function */

/**
 * Options affecting callback signature.
 *
 * Those add additional parameters at the beginning of the C callback, like that:
 * <pre>
 * rna_my_func([ID *_selfid],
 *             [<DNA_STRUCT> *self|StructRNA *type],
 *             [Main *bmain],
 *             [bContext *C],
 *             [ReportList *reports],
 *             <other RNA-defined parameters>);
 * </pre>
 */
enum FunctionFlag {
  /**
   * Pass ID owning 'self' data
   * (i.e. ptr->owner_id, might be same as self in case data is an ID...).
   */
  FUNC_USE_SELF_ID = (1 << 11),

  /**
   * Pass 'self' data as a PointerRNA (by value), rather than as a pointer of the relevant DNA
   * type.
   *
   * Mutually exclusive with #FUNC_NO_SELF and #FUNC_USE_SELF_TYPE.
   *
   * Useful for functions that need to access `self` as RNA data, not as DNA data (e.g. when doing
   * 'generic', type-agnostic processing).
   */
  FUNC_SELF_AS_RNA = (1 << 13),
  /**
   * Do not pass the object (DNA struct pointer) from which it is called,
   * used to define static or class functions.
   *
   * Mutually exclusive with #FUNC_SELF_AS_RNA.
   */
  FUNC_NO_SELF = (1 << 0),
  /** Pass RNA type, used to define class functions, only valid when #FUNC_NO_SELF is set. */
  FUNC_USE_SELF_TYPE = (1 << 1),

  /* Pass Main, bContext and/or ReportList. */
  FUNC_USE_MAIN = (1 << 2),
  FUNC_USE_CONTEXT = (1 << 3),
  FUNC_USE_REPORTS = (1 << 4),

  /***** Registering of Python subclasses. *****/
  /**
   * This function is part of the registerable class' interface,
   * and can be implemented/redefined in Python.
   */
  FUNC_REGISTER = (1 << 5),
  /** Subclasses can choose not to implement this function. */
  FUNC_REGISTER_OPTIONAL = FUNC_REGISTER | (1 << 6),
  /**
   * If not set, the Python function implementing this call
   * is not allowed to write into data-blocks.
   * Except for WindowManager and Screen currently, see rna_id_write_error() in `bpy_rna.cc`.
   */
  FUNC_ALLOW_WRITE = (1 << 12),

  /***** Internal flags. *****/
  /** UNUSED CURRENTLY? ??? */
  FUNC_BUILTIN = (1 << 7),
  /** UNUSED CURRENTLY. ??? */
  FUNC_EXPORT = (1 << 8),
  /** Function has been defined at runtime, not statically in RNA source code. */
  FUNC_RUNTIME = (1 << 9),
  /**
   * UNUSED CURRENTLY? Function owns its identifier and description strings,
   * and has to free them when deleted.
   */
  FUNC_FREE_POINTERS = (1 << 10),
};

using CallFunc = void (*)(bContext *C, ReportList *reports, PointerRNA *ptr, ParameterList *parms);

struct FunctionRNA;

/* Struct */

enum StructFlag {
  /** Indicates that this struct is an ID struct. */
  STRUCT_ID = (1 << 0),
  /**
   * Indicates that this ID type's usages should typically be refcounted (i.e. makesrna will
   * automatically set `PROP_ID_REFCOUNT` to PointerRNA properties that have this RNA type
   * assigned).
   */
  STRUCT_ID_REFCOUNT = (1 << 1),
  /** defaults on, indicates when changes in members of a StructRNA should trigger undo steps. */
  STRUCT_UNDO = (1 << 2),

  /* internal flags */
  STRUCT_RUNTIME = (1 << 3),
  /* STRUCT_GENERATED = (1 << 4), */ /* UNUSED */
  STRUCT_FREE_POINTERS = (1 << 5),
  /** Menus and Panels don't need properties */
  STRUCT_NO_IDPROPERTIES = (1 << 6),
  /** e.g. for Operator */
  STRUCT_NO_DATABLOCK_IDPROPERTIES = (1 << 7),
  /** for PropertyGroup which contains pointers to datablocks */
  STRUCT_CONTAINS_DATABLOCK_IDPROPERTIES = (1 << 8),
  /** Added to type-map #BlenderRNA.structs_map */
  STRUCT_PUBLIC_NAMESPACE = (1 << 9),
  /** All sub-types are added too. */
  STRUCT_PUBLIC_NAMESPACE_INHERIT = (1 << 10),
  /**
   * When the #PointerRNA.owner_id is NULL, this signifies the property should be accessed
   * without any context (the key-map UI and import/export for example).
   * So accessing the property should not read from the current context to derive values/limits.
   */
  STRUCT_NO_CONTEXT_WITHOUT_OWNER_ID = (1 << 11),
};

using StructValidateFunc = int (*)(PointerRNA *ptr, void *data, bool *have_function);
using StructCallbackFunc = int (*)(bContext *C,
                                   PointerRNA *ptr,
                                   FunctionRNA *func,
                                   ParameterList *list);
using StructFreeFunc = void (*)(void *data);
using StructRegisterFunc = StructRNA *(*)(Main * bmain,
                                          ReportList *reports,
                                          void *data,
                                          const char *identifier,
                                          StructValidateFunc validate,
                                          StructCallbackFunc call,
                                          StructFreeFunc free);
/** Return true when `type` was successfully unregistered & freed. */
using StructUnregisterFunc = bool (*)(Main *bmain, StructRNA *type);
using StructInstanceFunc = void **(*)(PointerRNA * ptr);

struct StructRNA;

/**
 * Blender RNA
 *
 * Root RNA data structure that lists all struct types.
 */
struct BlenderRNA;

/**
 * Extending
 *
 * This struct must be embedded in *Type structs in
 * order to make them definable through RNA.
 */
struct ExtensionRNA {
  /**
   * \note For Python types this holds the Python class but does *not* own a reference.
   * The same value is typically stored in `srna->py_type` which does own a reference.
   */
  void *data;
  StructRNA *srna;
  StructCallbackFunc call;
  StructFreeFunc free;
};

/**
 * Information about deprecated properties.
 *
 * Used by the API documentation and Python API to print warnings
 * when accessing a deprecated property.
 */
struct DeprecatedRNA {
  /** Single line deprecation message, suggest alternatives where possible. */
  const char *note;
  /** The released version this was deprecated. */
  short version;
  /**
   * The version this will be removed.
   * The value represents major, minor versions (sub-version isn't supported).
   * Compatible with #Main::versionfile (e.g. `502` for `v5.2`).
   */
  short removal_version;
};

/* Primitive types. */

struct PrimitiveStringRNA {
  const char *value;
};

struct PrimitiveIntRNA {
  int value;
};

struct PrimitiveFloatRNA {
  float value;
};

struct PrimitiveBooleanRNA {
  bool value;
};

#endif /* __RNA_TYPES_H__ */

/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>

#include <CLG_log.h>

#include "BLT_translation.hh"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

/* -------------------------------------------------------------------- */
/** \name Generic Enum's
 * \{ */

/* Reuse for dynamic types. */

const EnumPropertyItem rna_enum_dummy_NULL_items[] = {
    {0, nullptr, 0, nullptr, nullptr},
};

/* Reuse for dynamic types with default value. */

const EnumPropertyItem rna_enum_dummy_DEFAULT_items[] = {
    {0, "DEFAULT", 0, "Default", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name RNA Enum's
 * \{ */

const EnumPropertyItem rna_enum_property_type_items[] = {
    {PROP_BOOLEAN, "BOOLEAN", 0, "Boolean", ""},
    {PROP_INT, "INT", 0, "Integer", ""},
    {PROP_FLOAT, "FLOAT", 0, "Float", ""},
    {PROP_STRING, "STRING", 0, "String", ""},
    {PROP_ENUM, "ENUM", 0, "Enumeration", ""},
    {PROP_POINTER, "POINTER", 0, "Pointer", ""},
    {PROP_COLLECTION, "COLLECTION", 0, "Collection", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

/* Wraps multiple enums onto a single line in a way that is difficult to read.
 * NOTE: these enums are split up based on their use in `bpy.props` Python module. */

/* clang-format off */
#define RNA_ENUM_PROPERTY_SUBTYPE_STRING_ITEMS \
  {PROP_FILEPATH, "FILE_PATH", 0, "File Path", ""}, \
  {PROP_DIRPATH, "DIR_PATH", 0, "Directory Path", ""}, \
  {PROP_FILENAME, "FILE_NAME", 0, "File Name", ""}, \
  {PROP_BYTESTRING, "BYTE_STRING", 0, "Byte String", ""}, \
  {PROP_PASSWORD, "PASSWORD", 0, "Password", "A string that is displayed hidden ('********')"}

#define RNA_ENUM_PROPERTY_SUBTYPE_NUMBER_ITEMS \
  {PROP_PIXEL, "PIXEL", 0, "Pixel", "A distance on screen"}, \
  {PROP_PIXEL_DIAMETER, "PIXEL_DIAMETER", 0, "Pixel", "A distance on screen, specifically representing a diameter value"}, \
  {PROP_UNSIGNED, "UNSIGNED", 0, "Unsigned", ""}, \
  {PROP_PERCENTAGE, "PERCENTAGE", 0, "Percentage", "A percentage between 0 and 100"}, \
  {PROP_FACTOR, "FACTOR", 0, "Factor", "A factor between 0.0 and 1.0"}, \
  {PROP_ANGLE, "ANGLE", 0, "Angle", "A rotational value specified in radians"}, \
  {PROP_TIME, "TIME", 0, "Time (Scene Relative)", \
   "Time specified in frames, converted to seconds based on scene frame rate"}, \
  {PROP_TIME_ABSOLUTE, "TIME_ABSOLUTE", 0, "Time (Absolute)", \
   "Time specified in seconds, independent of the scene"}, \
  {PROP_DISTANCE, "DISTANCE", 0, "Distance", "A distance between two points"}, \
  {PROP_DISTANCE_DIAMETER, "DISTANCE_DIAMETER", 0, "Distance", "A distance between two points, specifically representing a diameter value"}, \
  {PROP_DISTANCE_CAMERA, "DISTANCE_CAMERA", 0, "Camera Distance", ""}, \
  {PROP_POWER, "POWER", 0, "Power", ""}, \
  {PROP_TEMPERATURE, "TEMPERATURE", 0, "Temperature", ""}, \
  {PROP_WAVELENGTH, "WAVELENGTH", 0, "Wavelength", ""}, \
  {PROP_COLOR_TEMPERATURE, "COLOR_TEMPERATURE", 0, "Color Temperature", ""}, \
  {PROP_FREQUENCY, "FREQUENCY", 0, "Frequency", ""}

#define RNA_ENUM_PROPERTY_SUBTYPE_NUMBER_ARRAY_ITEMS \
  {PROP_COLOR, "COLOR", 0, "Linear Color", "Color in the scene linear working color space"}, \
  {PROP_TRANSLATION, "TRANSLATION", 0, "Translation", ""}, \
  {PROP_DIRECTION, "DIRECTION", 0, "Direction", ""}, \
  {PROP_VELOCITY, "VELOCITY", 0, "Velocity", ""}, \
  {PROP_ACCELERATION, "ACCELERATION", 0, "Acceleration", ""}, \
  {PROP_MATRIX, "MATRIX", 0, "Matrix", ""}, \
  {PROP_EULER, "EULER", 0, "Euler Angles", "Euler rotation angles in radians"}, \
  {PROP_QUATERNION, "QUATERNION", 0, "Quaternion", "Quaternion rotation (affects NLA blending)"}, \
  {PROP_AXISANGLE, "AXISANGLE", 0, "Axis-Angle", "Angle and axis to rotate around"}, \
  {PROP_XYZ, "XYZ", 0, "XYZ", ""}, \
  {PROP_XYZ_LENGTH, "XYZ_LENGTH", 0, "XYZ Length", ""}, \
  {PROP_COLOR_GAMMA, "COLOR_GAMMA", 0, "sRGB Color", "Color in sRGB color space (mainly for user interface colors)"}, \
  {PROP_COORDS, "COORDINATES", 0, "Coordinates", ""}, \
  /* Boolean. */ \
  {PROP_LAYER, "LAYER", 0, "Layer", ""}, \
  {PROP_LAYER_MEMBER, "LAYER_MEMBER", 0, "Layer Member", ""}

/* clang-format on */

const EnumPropertyItem rna_enum_property_subtype_string_items[] = {
    RNA_ENUM_PROPERTY_SUBTYPE_STRING_ITEMS,

    {PROP_NONE, "NONE", 0, "None", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_property_subtype_number_items[] = {
    RNA_ENUM_PROPERTY_SUBTYPE_NUMBER_ITEMS,

    {PROP_NONE, "NONE", 0, "None", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_property_subtype_number_array_items[] = {
    RNA_ENUM_PROPERTY_SUBTYPE_NUMBER_ARRAY_ITEMS,

    {PROP_NONE, "NONE", 0, "None", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_property_subtype_items[] = {
    {PROP_NONE, "NONE", 0, "None", ""},

    /* String. */
    RNA_ENUM_PROPERTY_SUBTYPE_STRING_ITEMS,

    /* Number. */
    RNA_ENUM_PROPERTY_SUBTYPE_NUMBER_ITEMS,

    /* Number array. */
    RNA_ENUM_PROPERTY_SUBTYPE_NUMBER_ARRAY_ITEMS,

    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_property_unit_items[] = {
    {PROP_UNIT_NONE, "NONE", 0, "None", ""},
    {PROP_UNIT_LENGTH, "LENGTH", 0, "Length", ""},
    {PROP_UNIT_AREA, "AREA", 0, "Area", ""},
    {PROP_UNIT_VOLUME, "VOLUME", 0, "Volume", ""},
    {PROP_UNIT_ROTATION, "ROTATION", 0, "Rotation", ""},
    {PROP_UNIT_TIME, "TIME", 0, "Time (Scene Relative)", ""},
    {PROP_UNIT_TIME_ABSOLUTE, "TIME_ABSOLUTE", 0, "Time (Absolute)", ""},
    {PROP_UNIT_VELOCITY, "VELOCITY", 0, "Velocity", ""},
    {PROP_UNIT_ACCELERATION, "ACCELERATION", 0, "Acceleration", ""},
    {PROP_UNIT_MASS, "MASS", 0, "Mass", ""},
    {PROP_UNIT_CAMERA, "CAMERA", 0, "Camera", ""},
    {PROP_UNIT_POWER, "POWER", 0, "Power", ""},
    {PROP_UNIT_TEMPERATURE, "TEMPERATURE", 0, "Temperature", ""},
    {PROP_UNIT_WAVELENGTH, "WAVELENGTH", 0, "Wavelength", ""},
    {PROP_UNIT_COLOR_TEMPERATURE, "COLOR_TEMPERATURE", 0, "Color Temperature", ""},
    {PROP_UNIT_FREQUENCY, "FREQUENCY", 0, "Frequency", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

/* Descriptions for rna_enum_property_flag_items and rna_enum_property_flag_enum_items. */
static constexpr auto PROP_READ_ONLY_DESCR = "When set, the property cannot be edited";
static constexpr auto PROP_HIDDEN_DESCR =
    "For operators: hide from places in the user interface where Blender would add the property "
    "automatically, like Adjust Last Operation. Also this property is not written to presets.";
static constexpr auto PROP_SKIP_SAVE_DESCR =
    "For operators: the value of this property will not be remembered between invocations of the "
    "operator; instead, each invocation will start by using the default value. Also this "
    "property is not written to presets.";
static constexpr auto PROP_SKIP_PRESET_DESCR = "Do not write in presets";
static constexpr auto PROP_ANIMATABLE_DESCR = "";
static constexpr auto PROP_LIB_EXCEPTION_DESCR =
    "This property can be edited, even when it is used on linked data (which normally is "
    "read-only). Note that edits to the property will not be saved to the blend file.";
static constexpr auto PROP_PROPORTIONAL_DESCR = "";
static constexpr auto PROP_TEXTEDIT_UPDATE_DESCR = "";
static constexpr auto PROP_PATH_OUTPUT_DESCR = "";
static constexpr auto PROP_PATH_RELATIVE_DESCR =
    "This path supports relative prefix \"//\" which is expanded the directory "
    "where the current \".blend\" file is located.";
static constexpr auto PROP_PATH_SUPPORTS_TEMPLATES_DESCR =
    "This path supports the \"{variable_name}\" template syntax, which substitutes the "
    "value of the referenced variable in place of the template expression";
static constexpr auto PROP_ENUM_FLAG_DESCR = "";

const EnumPropertyItem rna_enum_property_flag_items[] = {
    /* NOTE: This is used only in the `bpy.props` module to define runtime RNA properties.
     * The value of this 'READ_ONLY' enum item is logically inverted compared to the
     * `PROP_EDITABLE` used everywhere else in RNA-related code.
     *
     * This inversion logic is handled by the bpy property definition code (see
     * #bpy_prop_assign_flag), and does not affect any other code area.
     *
     * This special handling is done to allow python property definitions to create by default
     * editable properties, without having to specify the `options={'EDITABLE', ...}` parameter all
     * the time.
     */
    {PROP_EDITABLE, "READ_ONLY", 0, "Read Only", PROP_READ_ONLY_DESCR},

    {PROP_HIDDEN, "HIDDEN", 0, "Hidden", PROP_HIDDEN_DESCR},
    {PROP_SKIP_SAVE, "SKIP_SAVE", 0, "Skip Save", PROP_SKIP_SAVE_DESCR},
    {PROP_SKIP_PRESET, "SKIP_PRESET", 0, "Skip Preset", PROP_SKIP_PRESET_DESCR},
    {PROP_ANIMATABLE, "ANIMATABLE", 0, "Animatable", PROP_ANIMATABLE_DESCR},
    {PROP_LIB_EXCEPTION, "LIBRARY_EDITABLE", 0, "Library Editable", PROP_LIB_EXCEPTION_DESCR},
    {PROP_PROPORTIONAL,
     "PROPORTIONAL",
     0,
     "Adjust values proportionally to each other",
     PROP_PROPORTIONAL_DESCR},
    {int(PROP_TEXTEDIT_UPDATE),
     "TEXTEDIT_UPDATE",
     0,
     "Update on every keystroke in textedit 'mode'",
     PROP_TEXTEDIT_UPDATE_DESCR},
    {PROP_PATH_OUTPUT, "OUTPUT_PATH", 0, "Output Path", PROP_PATH_OUTPUT_DESCR},
    {PROP_PATH_SUPPORTS_BLEND_RELATIVE,
     "PATH_SUPPORTS_BLEND_RELATIVE",
     0,
     "Relative Path Support",
     PROP_PATH_RELATIVE_DESCR},
    {PROP_PATH_SUPPORTS_TEMPLATES,
     "SUPPORTS_TEMPLATES",
     0,
     "Variable expression support",
     PROP_PATH_SUPPORTS_TEMPLATES_DESCR},
    {0, nullptr, 0, nullptr, nullptr},
};

/** Only for enum type properties. */
const EnumPropertyItem rna_enum_property_flag_enum_items[] = {
    /* NOTE: This is used only in the `bpy.props` module to define runtime RNA properties.
     * The value of this 'READ_ONLY' enum item is logically inverted compared to the
     * `PROP_EDITABLE` used everywhere else in RNA-related code.
     *
     * This inversion logic is handled by the bpy property definition code (see
     * #bpy_prop_assign_flag), and does not affect any other code area.
     *
     * This special handling is done to allow python property definitions to create by default
     * editable properties, without having to specify the `options={'EDITABLE', ...}` parameter all
     * the time.
     */
    {PROP_EDITABLE, "READ_ONLY", 0, "Read Only", PROP_READ_ONLY_DESCR},

    {PROP_HIDDEN, "HIDDEN", 0, "Hidden", PROP_HIDDEN_DESCR},
    {PROP_SKIP_SAVE, "SKIP_SAVE", 0, "Skip Save", PROP_SKIP_SAVE_DESCR},
    {PROP_ANIMATABLE, "ANIMATABLE", 0, "Animatable", PROP_ANIMATABLE_DESCR},
    {PROP_LIB_EXCEPTION, "LIBRARY_EDITABLE", 0, "Library Editable", PROP_LIB_EXCEPTION_DESCR},
    {PROP_ENUM_FLAG, "ENUM_FLAG", 0, "Enum Flag", PROP_ENUM_FLAG_DESCR},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_property_item_library_overridable{
    PROPOVERRIDE_OVERRIDABLE_LIBRARY,
    "LIBRARY_OVERRIDABLE",
    0,
    "Library Overridable",
    "Make that property editable in library overrides of linked data-blocks.\n"
    "NOTE: For a property to be overridable, its whole chain of parent properties must also be "
    "defined as overridable"};

const EnumPropertyItem rna_enum_property_override_flag_items[] = {
    rna_enum_property_item_library_overridable,
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_property_override_flag_collection_items[] = {
    rna_enum_property_item_library_overridable,
    {PROPOVERRIDE_NO_PROP_NAME,
     "NO_PROPERTY_NAME",
     0,
     "No Name",
     "Do not use the names of the items, only their indices in the collection"},
    {PROPOVERRIDE_LIBRARY_INSERTION,
     "USE_INSERTION",
     0,
     "Use Insertion",
     "Allow users to add new items in that collection in library overrides"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_property_string_search_flag_items[] = {
    {PROP_STRING_SEARCH_SORT, "SORT", 0, "Sort Search Results", ""},
    {PROP_STRING_SEARCH_SUGGESTION,
     "SUGGESTION",
     0,
     "Suggestion",
     "Search results are suggestions (other values may be entered)"},

    {0, nullptr, 0, nullptr, nullptr},
};

/** \} */

#ifdef RNA_RUNTIME
#  include "BLI_ghash.h"
#  include "BLI_string.h"

#  include "MEM_guardedalloc.h"

#  include "BKE_idprop.hh"
#  include "BKE_lib_override.hh"

#  include "RNA_path.hh"

#  include <optional>
#  include <string>

#  include <fmt/format.h>

static CLG_LogRef LOG_COMPARE_OVERRIDE = {"rna.rna_compare_override"};

/* Struct */

static void rna_Struct_identifier_get(PointerRNA *ptr, char *value)
{
  strcpy(value, ((StructRNA *)ptr->data)->identifier);
}

static int rna_Struct_identifier_length(PointerRNA *ptr)
{
  return strlen(((StructRNA *)ptr->data)->identifier);
}

static void rna_Struct_description_get(PointerRNA *ptr, char *value)
{
  strcpy(value, ((StructRNA *)ptr->data)->description);
}

static int rna_Struct_description_length(PointerRNA *ptr)
{
  return strlen(((StructRNA *)ptr->data)->description);
}

static void rna_Struct_name_get(PointerRNA *ptr, char *value)
{
  strcpy(value, ((StructRNA *)ptr->data)->name);
}

static int rna_Struct_name_length(PointerRNA *ptr)
{
  return strlen(((StructRNA *)ptr->data)->name);
}

static void rna_Struct_translation_context_get(PointerRNA *ptr, char *value)
{
  strcpy(value, ((StructRNA *)ptr->data)->translation_context);
}

static int rna_Struct_translation_context_length(PointerRNA *ptr)
{
  return strlen(((StructRNA *)ptr->data)->translation_context);
}

static PointerRNA rna_Struct_base_get(PointerRNA *ptr)
{
  return RNA_pointer_create_discrete(nullptr, &RNA_Struct, ((StructRNA *)ptr->data)->base);
}

static PointerRNA rna_Struct_nested_get(PointerRNA *ptr)
{
  return RNA_pointer_create_discrete(nullptr, &RNA_Struct, ((StructRNA *)ptr->data)->nested);
}

static PointerRNA rna_Struct_name_property_get(PointerRNA *ptr)
{
  return RNA_pointer_create_discrete(
      nullptr, &RNA_Property, ((StructRNA *)ptr->data)->nameproperty);
}

/* Struct property iteration. This is quite complicated, the purpose is to
 * iterate over properties of all inheritance levels, and for each struct to
 * also iterator over id properties not known by RNA. */

static bool rna_idproperty_known(CollectionPropertyIterator *iter, void *data)
{
  IDProperty *idprop = (IDProperty *)data;
  PropertyRNA *prop;
  StructRNA *ptype = iter->builtin_parent.type;

  /* Function to skip any id properties that are already known by RNA,
   * for the second loop where we go over unknown id properties.
   *
   * Note that only dynamically-defined RNA properties (the ones actually using IDProperties as
   * storage back-end) should be checked here. If a custom property is named the same as a 'normal'
   * RNA property, they are different data. */
  do {
    for (prop = static_cast<PropertyRNA *>(ptype->cont.properties.first); prop; prop = prop->next)
    {
      if ((prop->flag_internal & PROP_INTERN_BUILTIN) == 0 &&
          (prop->flag & PROP_IDPROPERTY) != 0 && STREQ(prop->identifier, idprop->name))
      {
        return true;
      }
    }
  } while ((ptype = ptype->base));

  return false;
}

static bool rna_property_builtin(CollectionPropertyIterator * /*iter*/, void *data)
{
  PropertyRNA *prop = (PropertyRNA *)data;

  /* function to skip builtin rna properties */

  return (prop->flag_internal & PROP_INTERN_BUILTIN) != 0;
}

static bool rna_function_builtin(CollectionPropertyIterator * /*iter*/, void *data)
{
  FunctionRNA *func = (FunctionRNA *)data;

  /* function to skip builtin rna functions */

  return (func->flag & FUNC_BUILTIN) != 0;
}

static void rna_inheritance_next_level_restart(CollectionPropertyIterator *iter,
                                               PointerRNA *ptr,
                                               IteratorSkipFunc skip,
                                               int funcs)
{
  /* RNA struct inheritance */
  while (!iter->valid && iter->level > 0) {
    StructRNA *srna;
    int i;

    srna = (StructRNA *)iter->parent.data;
    iter->level--;
    for (i = iter->level; i > 0; i--) {
      srna = srna->base;
    }

    rna_iterator_listbase_end(iter);

    if (funcs) {
      rna_iterator_listbase_begin(iter, ptr, &srna->functions, skip);
    }
    else {
      rna_iterator_listbase_begin(iter, ptr, &srna->cont.properties, skip);
    }
  }
}

static void rna_inheritance_properties_listbase_begin(CollectionPropertyIterator *iter,
                                                      PointerRNA *ptr,
                                                      ListBase *lb,
                                                      IteratorSkipFunc skip)
{
  rna_iterator_listbase_begin(iter, ptr, lb, skip);
  rna_inheritance_next_level_restart(iter, ptr, skip, 0);
}

static void rna_inheritance_properties_listbase_next(CollectionPropertyIterator *iter,
                                                     IteratorSkipFunc skip)
{
  rna_iterator_listbase_next(iter);
  rna_inheritance_next_level_restart(iter, &iter->parent, skip, 0);
}

static void rna_inheritance_functions_listbase_begin(CollectionPropertyIterator *iter,
                                                     PointerRNA *ptr,
                                                     ListBase *lb,
                                                     IteratorSkipFunc skip)
{
  rna_iterator_listbase_begin(iter, ptr, lb, skip);
  rna_inheritance_next_level_restart(iter, ptr, skip, 1);
}

static void rna_inheritance_functions_listbase_next(CollectionPropertyIterator *iter,
                                                    IteratorSkipFunc skip)
{
  rna_iterator_listbase_next(iter);
  rna_inheritance_next_level_restart(iter, &iter->parent, skip, 1);
}

static void rna_Struct_properties_next(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;
  IDProperty *group;

  if (internal->flag) {
    /* id properties */
    rna_iterator_listbase_next(iter);
  }
  else {
    /* regular properties */
    rna_inheritance_properties_listbase_next(iter, rna_property_builtin);

    /* Try IDProperties (i.e. custom data).
     *
     * NOTE: System IDProperties should not need to be handled here, as they are expected to have a
     * valid (runtime-defined) RNA property to wrap them, which will have been processed above as
     * part of `rna_inheritance_properties_listbase_next`. */
    if (!iter->valid) {
      group = RNA_struct_idprops(&iter->builtin_parent, 0);

      if (group) {
        rna_iterator_listbase_end(iter);
        rna_iterator_listbase_begin(iter, &iter->parent, &group->data.group, rna_idproperty_known);
        internal = &iter->internal.listbase;
        internal->flag = 1;
      }
    }
  }
}

static void rna_Struct_properties_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  StructRNA *srna;

  /* here ptr->data should always be the same as iter->parent.type */
  srna = (StructRNA *)ptr->data;

  while (srna->base) {
    iter->level++;
    srna = srna->base;
  }

  rna_inheritance_properties_listbase_begin(
      iter, ptr, &srna->cont.properties, rna_property_builtin);
}

static PointerRNA rna_Struct_properties_get(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;

  /* we return either PropertyRNA* or IDProperty*, the rna_access.cc
   * functions can handle both as PropertyRNA* with some tricks */
  return RNA_pointer_create_discrete(nullptr, &RNA_Property, internal->link);
}

static void rna_Struct_functions_next(CollectionPropertyIterator *iter)
{
  rna_inheritance_functions_listbase_next(iter, rna_function_builtin);
}

static void rna_Struct_functions_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  StructRNA *srna;

  /* here ptr->data should always be the same as iter->parent.type */
  srna = (StructRNA *)ptr->data;

  while (srna->base) {
    iter->level++;
    srna = srna->base;
  }

  rna_inheritance_functions_listbase_begin(iter, ptr, &srna->functions, rna_function_builtin);
}

static PointerRNA rna_Struct_functions_get(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;

  /* we return either PropertyRNA* or IDProperty*, the rna_access.cc
   * functions can handle both as PropertyRNA* with some tricks */
  return RNA_pointer_create_discrete(nullptr, &RNA_Function, internal->link);
}

static void rna_Struct_property_tags_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  /* here ptr->data should always be the same as iter->parent.type */
  StructRNA *srna = (StructRNA *)ptr->data;
  const EnumPropertyItem *tag_defines = RNA_struct_property_tag_defines(srna);
  uint tag_count = tag_defines ? RNA_enum_items_count(tag_defines) : 0;

  rna_iterator_array_begin(
      iter, ptr, (void *)tag_defines, sizeof(EnumPropertyItem), tag_count, 0, nullptr);
}

/* Builtin properties iterator re-uses the Struct properties iterator, only
 * difference is that we need to set the ptr data to the type of the struct
 * whose properties we want to iterate over. */

void rna_builtin_properties_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  PointerRNA newptr;

  /* we create a new pointer with the type as the data */
  newptr.type = &RNA_Struct;
  newptr.data = ptr->type;

  if (ptr->type->flag & STRUCT_ID) {
    newptr.owner_id = static_cast<ID *>(ptr->data);
  }
  else {
    newptr.owner_id = nullptr;
  }

  iter->parent = newptr;
  iter->builtin_parent = *ptr;

  rna_Struct_properties_begin(iter, &newptr);
}

void rna_builtin_properties_next(CollectionPropertyIterator *iter)
{
  rna_Struct_properties_next(iter);
}

PointerRNA rna_builtin_properties_get(CollectionPropertyIterator *iter)
{
  return rna_Struct_properties_get(iter);
}

bool rna_builtin_properties_lookup_string(PointerRNA *ptr, const char *key, PointerRNA *r_ptr)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = ptr->type;

  do {
    if (srna->cont.prop_lookup_set) {
      PropertyRNA *const *lookup_prop = srna->cont.prop_lookup_set->lookup_key_ptr_as(key);
      prop = lookup_prop ? *lookup_prop : nullptr;
      if (prop) {
        *r_ptr = {nullptr, &RNA_Property, prop};
        return true;
      }
    }
    else {
      for (prop = static_cast<PropertyRNA *>(srna->cont.properties.first); prop; prop = prop->next)
      {
        if (!(prop->flag_internal & PROP_INTERN_BUILTIN) && STREQ(prop->identifier, key)) {
          *r_ptr = {nullptr, &RNA_Property, prop};
          return true;
        }
      }
    }
  } while ((srna = srna->base));

  *r_ptr = {};
  return false;
}

PointerRNA rna_builtin_type_get(PointerRNA *ptr)
{
  return RNA_pointer_create_discrete(nullptr, &RNA_Struct, ptr->type);
}

/* Property */

static StructRNA *rna_Property_refine(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;

  switch (RNA_property_type(prop)) {
    case PROP_BOOLEAN:
      return &RNA_BoolProperty;
    case PROP_INT:
      return &RNA_IntProperty;
    case PROP_FLOAT:
      return &RNA_FloatProperty;
    case PROP_STRING:
      return &RNA_StringProperty;
    case PROP_ENUM:
      return &RNA_EnumProperty;
    case PROP_POINTER:
      return &RNA_PointerProperty;
    case PROP_COLLECTION:
      return &RNA_CollectionProperty;
    default:
      return &RNA_Property;
  }
}

static void rna_Property_identifier_get(PointerRNA *ptr, char *value)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  strcpy(value, RNA_property_identifier(prop));
}

static int rna_Property_identifier_length(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  return strlen(RNA_property_identifier(prop));
}

static void rna_Property_name_get(PointerRNA *ptr, char *value)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  const char *name = RNA_property_ui_name_raw(prop);
  strcpy(value, name ? name : "");
}

static int rna_Property_name_length(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  const char *name = RNA_property_ui_name_raw(prop);
  return name ? strlen(name) : 0;
}

static void rna_Property_description_get(PointerRNA *ptr, char *value)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  const char *description = RNA_property_ui_description_raw(prop);
  strcpy(value, description ? description : "");
}
static int rna_Property_description_length(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  const char *description = RNA_property_ui_description_raw(prop);
  return description ? strlen(description) : 0;
}

static void rna_Property_translation_context_get(PointerRNA *ptr, char *value)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  strcpy(value, RNA_property_translation_context(prop));
}

static int rna_Property_translation_context_length(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  return strlen(RNA_property_translation_context(prop));
}

static int rna_Property_type_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  return RNA_property_type(prop);
}

static int rna_Property_subtype_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  return RNA_property_subtype(prop);
}

static PointerRNA rna_Property_srna_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  return RNA_pointer_create_discrete(nullptr, &RNA_Struct, prop->srna);
}

static int rna_Property_unit_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  return RNA_property_unit(prop);
}

static int rna_Property_icon_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  return RNA_property_ui_icon(prop);
}

static bool rna_Property_readonly_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;

  /* don't use this because it will call functions that check the internal
   * data for introspection we only need to know if it can be edited so the
   * flag is better for this */
  // return RNA_property_editable(ptr, prop);
  return (prop->flag & PROP_EDITABLE) == 0;
}

static bool rna_Property_animatable_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;

  return (prop->flag & PROP_ANIMATABLE) != 0;
}

static bool rna_Property_overridable_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;

  IDProperty *idprop = rna_idproperty_check(&prop, ptr);

  return idprop != nullptr ? (idprop->flag & IDP_FLAG_OVERRIDABLE_LIBRARY) != 0 :
                             (prop->flag_override & PROPOVERRIDE_OVERRIDABLE_LIBRARY) != 0;
}

static bool rna_Property_use_output_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  return (prop->flag_parameter & PARM_OUTPUT) != 0;
}

static bool rna_Property_is_required_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  return (prop->flag_parameter & PARM_REQUIRED) != 0;
}

static bool rna_Property_is_argument_optional_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  return (prop->flag_parameter & PARM_PYFUNC_REGISTER_OPTIONAL) != 0;
}

static bool rna_Property_is_never_none_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  return (prop->flag & PROP_NEVER_NULL) != 0;
}

static bool rna_Property_is_hidden_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  return (prop->flag & PROP_HIDDEN) != 0;
}

static bool rna_Property_is_skip_save_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  return (prop->flag & PROP_SKIP_SAVE) != 0;
}

static bool rna_Property_is_skip_preset_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  return (prop->flag & (PROP_SKIP_SAVE | PROP_HIDDEN | PROP_SKIP_PRESET)) != 0;
}

static bool rna_Property_is_enum_flag_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  return (prop->flag & PROP_ENUM_FLAG) != 0;
}

static bool rna_Property_is_library_editable_flag_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  return (prop->flag & PROP_LIB_EXCEPTION) != 0;
}

static bool rna_Property_is_path_output_flag_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  return (prop->flag & PROP_PATH_OUTPUT) != 0;
}

static bool rna_Property_is_path_supports_blend_relative_flag_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  return (prop->flag & PROP_PATH_SUPPORTS_BLEND_RELATIVE) != 0;
}

static bool rna_Property_is_path_supports_templates_flag_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  return (prop->flag & PROP_PATH_SUPPORTS_TEMPLATES) != 0;
}

static int rna_Property_tags_get(PointerRNA *ptr)
{
  return RNA_property_tags(static_cast<PropertyRNA *>(ptr->data));
}

static const EnumPropertyItem *rna_Property_tags_itemf(bContext * /*C*/,
                                                       PointerRNA *ptr,
                                                       PropertyRNA * /*prop*/,
                                                       bool *r_free)
{
  PropertyRNA *this_prop = (PropertyRNA *)ptr->data;
  const StructRNA *srna = RNA_property_pointer_type(ptr, this_prop);
  EnumPropertyItem *prop_tags;
  EnumPropertyItem tmp = {0, "", 0, "", ""};
  int totitem = 0;

  for (const EnumPropertyItem *struct_tags = RNA_struct_property_tag_defines(srna);
       struct_tags != nullptr && struct_tags->identifier != nullptr;
       struct_tags++)
  {
    memcpy(&tmp, struct_tags, sizeof(tmp));
    RNA_enum_item_add(&prop_tags, &totitem, &tmp);
  }
  RNA_enum_item_end(&prop_tags, &totitem);
  *r_free = true;

  return prop_tags;
}

static int rna_Property_array_length_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  return prop->totarraylength;
}

static void rna_Property_array_dimensions_get(PointerRNA *ptr,
                                              int dimensions[RNA_MAX_ARRAY_DIMENSION])
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);

  if (prop->arraydimension > 1) {
    for (int i = RNA_MAX_ARRAY_DIMENSION; i--;) {
      dimensions[i] = (i >= prop->arraydimension) ? 0 : prop->arraylength[i];
    }
  }
  else {
    memset(dimensions, 0, sizeof(*dimensions) * RNA_MAX_ARRAY_DIMENSION);
    dimensions[0] = prop->totarraylength;
  }
}

static bool rna_Property_is_registered_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  return (prop->flag & PROP_REGISTER) != 0;
}

static bool rna_Property_is_registered_optional_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  return (prop->flag & PROP_REGISTER_OPTIONAL) != 0;
}

static bool rna_Property_is_runtime_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  return RNA_property_is_runtime(prop);
}

static bool rna_Property_is_deprecated_get(PointerRNA *ptr)
{
  const PropertyRNA *prop = (const PropertyRNA *)ptr->data;
  return RNA_property_deprecated(prop) != nullptr;
}

static int rna_Property_deprecated_note_length(PointerRNA *ptr)
{
  const PropertyRNA *prop = (const PropertyRNA *)ptr->data;
  if (const DeprecatedRNA *deprecated = RNA_property_deprecated(prop)) {
    return strlen(deprecated->note);
  }
  return 0;
}

static void rna_Property_deprecated_note_get(PointerRNA *ptr, char *value)
{
  const PropertyRNA *prop = (const PropertyRNA *)ptr->data;
  if (const DeprecatedRNA *deprecated = RNA_property_deprecated(prop)) {
    strcpy(value, deprecated->note);
  }
  else {
    value[0] = '\0';
  }
}

static void rna_Property_deprecated_version_get(PointerRNA *ptr, int *value)
{
  const PropertyRNA *prop = (const PropertyRNA *)ptr->data;
  short version = 0;
  if (const DeprecatedRNA *deprecated = RNA_property_deprecated(prop)) {
    version = deprecated->version;
  }
  ARRAY_SET_ITEMS(value, version / 100, version % 100, 0);
}

static void rna_Property_deprecated_removal_version_get(PointerRNA *ptr, int *value)
{
  const PropertyRNA *prop = (const PropertyRNA *)ptr->data;
  short version = 0;
  if (const DeprecatedRNA *deprecated = RNA_property_deprecated(prop)) {
    version = deprecated->removal_version;
  }

  ARRAY_SET_ITEMS(value, version / 100, version % 100, 0);
}

static bool rna_BoolProperty_default_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  return ((BoolPropertyRNA *)prop)->defaultvalue;
}

static int rna_IntProperty_default_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  return ((IntPropertyRNA *)prop)->defaultvalue;
}
/* int/float/bool */
static int rna_NumberProperty_default_array_get_length(const PointerRNA *ptr,
                                                       int length[RNA_MAX_ARRAY_DIMENSION])
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);

  length[0] = prop->totarraylength;

  return length[0];
}
static bool rna_NumberProperty_is_array_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;

  return RNA_property_array_check(prop);
}

static void rna_IntProperty_default_array_get(PointerRNA *ptr, int *values)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  if (prop->totarraylength > 0) {
    PointerRNA null_ptr = PointerRNA_NULL;
    RNA_property_int_get_default_array(&null_ptr, prop, values);
  }
}

static void rna_BoolProperty_default_array_get(PointerRNA *ptr, bool *values)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  if (prop->totarraylength > 0) {
    PointerRNA null_ptr = PointerRNA_NULL;
    RNA_property_boolean_get_default_array(&null_ptr, prop, values);
  }
}

static void rna_FloatProperty_default_array_get(PointerRNA *ptr, float *values)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  if (prop->totarraylength > 0) {
    PointerRNA null_ptr = PointerRNA_NULL;
    RNA_property_float_get_default_array(&null_ptr, prop, values);
  }
}

static int rna_IntProperty_hard_min_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  return ((IntPropertyRNA *)prop)->hardmin;
}

static int rna_IntProperty_hard_max_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  return ((IntPropertyRNA *)prop)->hardmax;
}

static int rna_IntProperty_soft_min_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  return ((IntPropertyRNA *)prop)->softmin;
}

static int rna_IntProperty_soft_max_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  return ((IntPropertyRNA *)prop)->softmax;
}

static int rna_IntProperty_step_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  return ((IntPropertyRNA *)prop)->step;
}

static float rna_FloatProperty_default_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  return ((FloatPropertyRNA *)prop)->defaultvalue;
}
static float rna_FloatProperty_hard_min_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  return ((FloatPropertyRNA *)prop)->hardmin;
}

static float rna_FloatProperty_hard_max_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  return ((FloatPropertyRNA *)prop)->hardmax;
}

static float rna_FloatProperty_soft_min_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  return ((FloatPropertyRNA *)prop)->softmin;
}

static float rna_FloatProperty_soft_max_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  return ((FloatPropertyRNA *)prop)->softmax;
}

static float rna_FloatProperty_step_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  return ((FloatPropertyRNA *)prop)->step;
}

static int rna_FloatProperty_precision_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  return ((FloatPropertyRNA *)prop)->precision;
}

static void rna_StringProperty_default_get(PointerRNA *ptr, char *value)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  strcpy(value, ((StringPropertyRNA *)prop)->defaultvalue);
}
static int rna_StringProperty_default_length(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  return strlen(((StringPropertyRNA *)prop)->defaultvalue);
}

static int rna_StringProperty_max_length_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  return ((StringPropertyRNA *)prop)->maxlength;
}

static const EnumPropertyItem *rna_EnumProperty_default_itemf(bContext *C,
                                                              PointerRNA *ptr,
                                                              PropertyRNA *prop_parent,
                                                              bool *r_free)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  EnumPropertyRNA *eprop;

  prop = rna_ensure_property(prop);
  eprop = (EnumPropertyRNA *)prop;

  /* incompatible default attributes */
  if ((prop_parent->flag & PROP_ENUM_FLAG) != (prop->flag & PROP_ENUM_FLAG)) {
    return rna_enum_dummy_NULL_items;
  }

  if ((eprop->item_fn == nullptr) || (eprop->item_fn == rna_EnumProperty_default_itemf) ||
      (ptr->type == &RNA_EnumProperty) || (C == nullptr))
  {
    if (eprop->item) {
      return eprop->item;
    }
  }

  return eprop->item_fn(C, ptr, prop, r_free);
}

/* XXX: not sure this is needed? */
static int rna_EnumProperty_default_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  return ((EnumPropertyRNA *)prop)->defaultvalue;
}

static bool rna_enum_check_separator(CollectionPropertyIterator * /*iter*/, void *data)
{
  EnumPropertyItem *item = (EnumPropertyItem *)data;

  return (item->identifier[0] == 0);
}

static void rna_EnumProperty_items_begin_impl(CollectionPropertyIterator *iter,
                                              PointerRNA *ptr,
                                              IteratorSkipFunc skip_fn)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  // EnumPropertyRNA *eprop; /* UNUSED */
  const EnumPropertyItem *item = nullptr;
  int totitem;
  bool free;

  prop = rna_ensure_property(prop);
  // eprop = (EnumPropertyRNA *)prop;

  RNA_property_enum_items_ex(nullptr,
                             ptr,
                             prop,
                             STREQ(iter->prop->identifier, "enum_items_static"),
                             &item,
                             &totitem,
                             &free);
  rna_iterator_array_begin(
      iter, ptr, (void *)item, sizeof(EnumPropertyItem), totitem, free, skip_fn);
}

static void rna_EnumProperty_items_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  rna_EnumProperty_items_begin_impl(iter, ptr, rna_enum_check_separator);
}

static void rna_EnumProperty_items_ui_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  /* No skip-function, include all "UI" items. */
  rna_EnumProperty_items_begin_impl(iter, ptr, nullptr);
}

static void rna_EnumPropertyItem_identifier_get(PointerRNA *ptr, char *value)
{
  strcpy(value, ((EnumPropertyItem *)ptr->data)->identifier);
}

static int rna_EnumPropertyItem_identifier_length(PointerRNA *ptr)
{
  return strlen(((EnumPropertyItem *)ptr->data)->identifier);
}

static void rna_EnumPropertyItem_name_get(PointerRNA *ptr, char *value)
{
  const EnumPropertyItem *eprop = static_cast<const EnumPropertyItem *>(ptr->data);
  /* Name can be nullptr in the case of separators
   * which are exposed via `_bpy.rna_enum_items_static`. */
  if (eprop->name) {
    strcpy(value, eprop->name);
  }
  else {
    value[0] = '\0';
  }
}

static int rna_EnumPropertyItem_name_length(PointerRNA *ptr)
{
  const EnumPropertyItem *eprop = static_cast<const EnumPropertyItem *>(ptr->data);
  if (eprop->name) {
    return strlen(eprop->name);
  }
  return 0;
}

static void rna_EnumPropertyItem_description_get(PointerRNA *ptr, char *value)
{
  const EnumPropertyItem *eprop = static_cast<const EnumPropertyItem *>(ptr->data);

  if (eprop->description) {
    strcpy(value, eprop->description);
  }
  else {
    value[0] = '\0';
  }
}

static int rna_EnumPropertyItem_description_length(PointerRNA *ptr)
{
  EnumPropertyItem *eprop = (EnumPropertyItem *)ptr->data;

  if (eprop->description) {
    return strlen(eprop->description);
  }
  return 0;
}

static int rna_EnumPropertyItem_value_get(PointerRNA *ptr)
{
  return ((EnumPropertyItem *)ptr->data)->value;
}

static int rna_EnumPropertyItem_icon_get(PointerRNA *ptr)
{
  return ((EnumPropertyItem *)ptr->data)->icon;
}

static PointerRNA rna_PointerProperty_fixed_type_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  return RNA_pointer_create_discrete(nullptr, &RNA_Struct, ((PointerPropertyRNA *)prop)->type);
}

static PointerRNA rna_CollectionProperty_fixed_type_get(PointerRNA *ptr)
{
  PropertyRNA *prop = (PropertyRNA *)ptr->data;
  prop = rna_ensure_property(prop);
  return RNA_pointer_create_discrete(
      nullptr, &RNA_Struct, ((CollectionPropertyRNA *)prop)->item_type);
}

/* Function */

static void rna_Function_identifier_get(PointerRNA *ptr, char *value)
{
  strcpy(value, ((FunctionRNA *)ptr->data)->identifier);
}

static int rna_Function_identifier_length(PointerRNA *ptr)
{
  return strlen(((FunctionRNA *)ptr->data)->identifier);
}

static void rna_Function_description_get(PointerRNA *ptr, char *value)
{
  strcpy(value, ((FunctionRNA *)ptr->data)->description);
}

static int rna_Function_description_length(PointerRNA *ptr)
{
  return strlen(((FunctionRNA *)ptr->data)->description);
}

static void rna_Function_parameters_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  rna_iterator_listbase_begin(
      iter, ptr, &((FunctionRNA *)ptr->data)->cont.properties, rna_property_builtin);
}

static bool rna_Function_registered_get(PointerRNA *ptr)
{
  FunctionRNA *func = (FunctionRNA *)ptr->data;
  return 0 != (func->flag & FUNC_REGISTER);
}

static bool rna_Function_registered_optional_get(PointerRNA *ptr)
{
  FunctionRNA *func = (FunctionRNA *)ptr->data;
  return 0 != (func->flag & (FUNC_REGISTER_OPTIONAL & ~FUNC_REGISTER));
}

static bool rna_Function_no_self_get(PointerRNA *ptr)
{
  FunctionRNA *func = (FunctionRNA *)ptr->data;
  return !(func->flag & FUNC_NO_SELF);
}

static bool rna_Function_use_self_type_get(PointerRNA *ptr)
{
  FunctionRNA *func = (FunctionRNA *)ptr->data;
  return 0 != (func->flag & FUNC_USE_SELF_TYPE);
}

/* Blender RNA */

static bool rna_struct_is_publc(CollectionPropertyIterator * /*iter*/, void *data)
{
  StructRNA *srna = static_cast<StructRNA *>(data);

  return !(srna->flag & STRUCT_PUBLIC_NAMESPACE);
}

static void rna_BlenderRNA_structs_begin(CollectionPropertyIterator *iter, PointerRNA *ptr)
{
  BlenderRNA *brna = static_cast<BlenderRNA *>(ptr->data);
  rna_iterator_listbase_begin(iter, ptr, &brna->structs, rna_struct_is_publc);
}

/* optional, for faster lookups */
static int rna_BlenderRNA_structs_length(PointerRNA *ptr)
{
  BlenderRNA *brna = static_cast<BlenderRNA *>(ptr->data);
  BLI_assert(brna->structs_len == BLI_listbase_count(&brna->structs));
  return brna->structs_len;
}
static bool rna_BlenderRNA_structs_lookup_int(PointerRNA *ptr, int index, PointerRNA *r_ptr)
{
  BlenderRNA *brna = static_cast<BlenderRNA *>(ptr->data);
  StructRNA *srna = static_cast<StructRNA *>(
      index < brna->structs_len ? BLI_findlink(&brna->structs, index) : nullptr);
  if (srna != nullptr) {
    *r_ptr = RNA_pointer_create_discrete(nullptr, &RNA_Struct, srna);
    return true;
  }
  else {
    return false;
  }
}
static bool rna_BlenderRNA_structs_lookup_string(PointerRNA *ptr,
                                                 const char *key,
                                                 PointerRNA *r_ptr)
{
  BlenderRNA *brna = static_cast<BlenderRNA *>(ptr->data);
  StructRNA *srna = static_cast<StructRNA *>(BLI_ghash_lookup(brna->structs_map, (void *)key));
  if (srna != nullptr) {
    *r_ptr = RNA_pointer_create_discrete(nullptr, &RNA_Struct, srna);
    return true;
  }

  return false;
}

/* Default override (and compare) callbacks. */

/**
 * Ensures it makes sense to go inside the pointers to compare their content
 * (if they are IDs, or have different names or RNA type, then this would be meaningless).
 */
struct RNACompareOverrideDiffPropPtrContext {
  /** General RNA diffing context. */
  RNAPropertyOverrideDiffContext &rnadiff_ctx;

  /** RNA pointer specific diffing parameters. */
  ID *owner_id_a = nullptr;
  ID *owner_id_b = nullptr;
  PointerRNA propptr_a = {};
  PointerRNA propptr_b = {};
  PropertyType property_type = PROP_BOOLEAN;

  /**
   * Indicates the RNA structure containing that RNA pointer does not own the data it points to.
   * See also #PROP_PTR_NO_OWNERSHIP documentation.
   */
  bool no_ownership = false;
  /**
   * RNA collection items: do not attempt to get the item property name
   * (i.e. only uses its index).
   */
  bool no_prop_name = false;
  /** RNA collection items: forcefully get an item property name, even if one of the items is
   *  null/doesn't have one. Mutually exclusive with `no_prop_name`. */
  bool do_force_name = false;
  /** RNA collection ID items: also check and store item's ID pointers. */
  bool use_id_pointer = false;

  /**
   * Whether the processed property definition has liboverride apply callback.
   *
   * Such a callback is mandatory for REPLACE operations unless the pointer is an ID one, and
   * INSERT operations in all cases (for RNA collections).
   *
   * \note IDproperty-based "RNA property" are handled by default apply call, so they are
   *       considered as always having an apply callback.
   */
  bool has_liboverride_apply_cb = false;

  /** Information specific to RNA collections. */
  /* NOTE: names (and ID pointers, in case items are ID pointers) are typically set by a call
   * to #rna_property_override_diff_propptr_validate_diffing. Indices are typically set directly
   * from the loop over all RNA collections items in #rna_property_override_diff_default. */
  std::optional<std::string> rna_itemname_a;
  std::optional<std::string> rna_itemname_b;
  std::optional<ID *> rna_itemid_a;
  std::optional<ID *> rna_itemid_b;
  int rna_itemindex_a = -1;
  int rna_itemindex_b = -1;

  /**
   * Status info, usually set by a call to #rna_property_override_diff_propptr_validate_diffing.
   */
  /** Indicate whether the two given RNA pointers can be considered 'matching data', i.e. the
   * pointers themselves should not be compared, but rather the content of the RNA structs they
   * point to. Note that this is never the case for ID RNA pointers (i.e. when `is_id` below is
   * `true`). */
  bool is_valid_for_diffing = true;

  bool is_id = false;
  bool is_null = false;
  bool is_type_diff = false;

  RNACompareOverrideDiffPropPtrContext(RNAPropertyOverrideDiffContext &rnadiff_ctx_)
      : rnadiff_ctx(rnadiff_ctx_)
  {
  }
};
static void rna_property_override_diff_propptr_validate_diffing(
    RNACompareOverrideDiffPropPtrContext &ptrdiff_ctx)
{
  PointerRNA *propptr_a = &ptrdiff_ctx.propptr_a;
  PointerRNA *propptr_b = &ptrdiff_ctx.propptr_b;
  const bool no_ownership = ptrdiff_ctx.no_ownership;
  const bool no_prop_name = ptrdiff_ctx.no_prop_name;
  const bool do_force_name = ptrdiff_ctx.do_force_name;

  ptrdiff_ctx.is_valid_for_diffing = true;

  BLI_assert(propptr_a != nullptr);

  if (do_force_name || ptrdiff_ctx.use_id_pointer) {
    BLI_assert(!no_prop_name);
  }

  /* Beware, PointerRNA_NULL has no type and is considered a 'blank page'! */
  if (ELEM(nullptr, propptr_a->type, propptr_a->data)) {
    if (ELEM(nullptr, propptr_b, propptr_b->type, propptr_b->data)) {
      ptrdiff_ctx.is_null = true;
    }
    else {
      ptrdiff_ctx.is_id = RNA_struct_is_ID(propptr_b->type);
      ptrdiff_ctx.is_null = true;
      ptrdiff_ctx.is_type_diff = propptr_a->type != propptr_b->type;
    }
    ptrdiff_ctx.is_valid_for_diffing = false;
  }
  else {
    ptrdiff_ctx.is_id = RNA_struct_is_ID(propptr_a->type);
    ptrdiff_ctx.is_null = (ELEM(nullptr, propptr_b, propptr_b->type, propptr_b->data));
    ptrdiff_ctx.is_type_diff = (propptr_b == nullptr || propptr_b->type != propptr_a->type);
    ptrdiff_ctx.is_valid_for_diffing = !((ptrdiff_ctx.is_id && no_ownership) ||
                                         ptrdiff_ctx.is_null || ptrdiff_ctx.is_type_diff);
  }

  /* We do a generic quick first comparison checking for "name" and/or "type" properties.
   * We assume that is any of those are false, then we are not handling the same data.
   * This helps a lot in library override case, especially to detect inserted items in collections.
   */
  if (!no_prop_name && (ptrdiff_ctx.is_valid_for_diffing || do_force_name)) {
    PropertyRNA *nameprop_a = (propptr_a->type != nullptr) ?
                                  RNA_struct_name_property(propptr_a->type) :
                                  nullptr;
    PropertyRNA *nameprop_b = (propptr_b != nullptr && propptr_b->type != nullptr) ?
                                  RNA_struct_name_property(propptr_b->type) :
                                  nullptr;
    const bool do_id_pointer = ptrdiff_ctx.use_id_pointer && ptrdiff_ctx.is_id;

    /* NOTE: Until we have a `std::string` version of `RNA_property_string_get`, these C string
     * pointers and buffers are needed here. Otherwise, in case of C string allocation, if the
     * result of `RNA_property_string_get` is directly assigned to a `std::string`, there is no way
     * to free this memory. */
    int rna_itemname_a_len = 0, rna_itemname_b_len = 0;
    char *rna_itemname_a = nullptr;
    char *rna_itemname_b = nullptr;
    char buff_a[4096];
    char buff_b[4096];
    if (nameprop_a != nullptr) {
      rna_itemname_a = RNA_property_string_get_alloc(
          propptr_a, nameprop_a, buff_a, sizeof(buff_a), &rna_itemname_a_len);
      //          printf("rna_itemname_a = %s\n", rna_itemname_a ? rna_itemname_a : "<NONE>");
      ptrdiff_ctx.rna_itemname_a = rna_itemname_a;
    }
    //      else printf("item of type %s a has no name property!\n", propptr_a->type->name);
    if (nameprop_b != nullptr) {
      rna_itemname_b = RNA_property_string_get_alloc(
          propptr_b, nameprop_b, buff_b, sizeof(buff_b), &rna_itemname_b_len);
      ptrdiff_ctx.rna_itemname_b = rna_itemname_b;
    }

    /* NOTE: This will always assign nullptr to these lib-pointers in case `do_id_lib` is false,
     * which ensures that they will not affect the result of `ptrdiff_ctx.is_valid_for_diffing` in
     * the last check below. */
    ID *rna_itemid_a = (do_id_pointer && propptr_a->data) ? static_cast<ID *>(propptr_a->data) :
                                                            nullptr;
    ID *rna_itemid_b = (do_id_pointer && propptr_b->data) ? static_cast<ID *>(propptr_b->data) :
                                                            nullptr;
    if (do_id_pointer) {
      ptrdiff_ctx.rna_itemid_a = rna_itemid_a;
      ptrdiff_ctx.rna_itemid_b = rna_itemid_b;
    }

    if (rna_itemname_a != nullptr && rna_itemname_b != nullptr) {
      if (rna_itemid_a != rna_itemid_b || rna_itemname_a_len != rna_itemname_b_len ||
          rna_itemname_a[0] != rna_itemname_b[0] || !STREQ(rna_itemname_a, rna_itemname_b))
      {
        ptrdiff_ctx.is_valid_for_diffing = false;
        //              printf("%s: different names\n", rna_path ? rna_path : "<UNKNOWN>");
      }
    }
    if (UNLIKELY(rna_itemname_a && rna_itemname_a != buff_a)) {
      MEM_freeN(rna_itemname_a);
    }
    if (UNLIKELY(rna_itemname_b && rna_itemname_b != buff_b)) {
      MEM_freeN(rna_itemname_b);
    }
  }

  if (ptrdiff_ctx.is_id) {
    BLI_assert(propptr_a->data == propptr_a->owner_id && propptr_b->data == propptr_b->owner_id);
  }
}

/* Used for both Pointer and Collection properties. */
static void rna_property_override_diff_propptr(Main *bmain,
                                               RNACompareOverrideDiffPropPtrContext &ptrdiff_ctx)
{
  ID *owner_id_a = ptrdiff_ctx.owner_id_a;
  ID *owner_id_b = ptrdiff_ctx.owner_id_b;
  PointerRNA *propptr_a = &ptrdiff_ctx.propptr_a;
  PointerRNA *propptr_b = &ptrdiff_ctx.propptr_b;

  eRNACompareMode mode = ptrdiff_ctx.rnadiff_ctx.mode;
  const bool no_ownership = ptrdiff_ctx.no_ownership;

  IDOverrideLibrary *liboverride = ptrdiff_ctx.rnadiff_ctx.liboverride;
  const eRNAOverrideMatch liboverride_flags = ptrdiff_ctx.rnadiff_ctx.liboverride_flags;
  const char *rna_path = ptrdiff_ctx.rnadiff_ctx.rna_path;
  size_t rna_path_len = ptrdiff_ctx.rnadiff_ctx.rna_path_len;
  const PropertyType property_type = ptrdiff_ctx.property_type;

  std::optional<std::string> &rna_itemname_a = ptrdiff_ctx.rna_itemname_a;
  std::optional<std::string> &rna_itemname_b = ptrdiff_ctx.rna_itemname_b;
  const int rna_itemindex_a = ptrdiff_ctx.rna_itemindex_a;
  const int rna_itemindex_b = ptrdiff_ctx.rna_itemindex_b;

  BLI_assert(ELEM(property_type, PROP_POINTER, PROP_COLLECTION));

  rna_property_override_diff_propptr_validate_diffing(ptrdiff_ctx);

  /* If false, it means that the whole data itself is different,
   * so no point in going inside of it at all! */
  const bool is_valid_for_diffing = ptrdiff_ctx.is_valid_for_diffing;
  const bool is_id = ptrdiff_ctx.is_id;
  const bool is_null = ptrdiff_ctx.is_null;
  const bool is_type_diff = ptrdiff_ctx.is_type_diff;

  const bool has_liboverride_apply_cb = ptrdiff_ctx.has_liboverride_apply_cb;
  const bool do_create = (liboverride != nullptr &&
                          /* Replacing a Pointer can only happen if it's an ID one, or there is a
                           * dedicated apply callback defined for the property. */
                          (is_id || has_liboverride_apply_cb) &&
                          (liboverride_flags & RNA_OVERRIDE_COMPARE_CREATE) != 0 &&
                          rna_path != nullptr);

  if (is_id) {
    /* Owned IDs (the ones we want to actually compare in depth, instead of just comparing pointer
     * values) should be always properly tagged as 'virtual' overrides. */
    ID *id = propptr_a->owner_id;
    if (id != nullptr && !ID_IS_OVERRIDE_LIBRARY(id)) {
      id = propptr_b->owner_id;
      if (id != nullptr && !ID_IS_OVERRIDE_LIBRARY(id)) {
        id = nullptr;
      }
    }

    BLI_assert(no_ownership || id == nullptr || ID_IS_OVERRIDE_LIBRARY_VIRTUAL(id));
    UNUSED_VARS_NDEBUG(id);
  }

  if (liboverride) {
    if (no_ownership || is_null || is_type_diff || !is_valid_for_diffing) {
      /* In case this pointer prop does not own its data (or one is nullptr), do not compare
       * structs! This is a quite safe path to infinite loop, among other nasty issues. Instead,
       * just compare pointers themselves. */
      ptrdiff_ctx.rnadiff_ctx.comparison = (propptr_a->data != propptr_b->data);

      if (do_create && ptrdiff_ctx.rnadiff_ctx.comparison != 0) {
        bool created = false;
        IDOverrideLibraryProperty *op = BKE_lib_override_library_property_get(
            liboverride, rna_path, &created);

        /* If not yet overridden, or if we are handling sub-items (inside a collection)... */
        if (op != nullptr) {
          if (created || op->rna_prop_type == 0) {
            op->rna_prop_type = property_type;
          }
          else {
            BLI_assert(op->rna_prop_type == property_type);
          }

          IDOverrideLibraryPropertyOperation *opop = nullptr;
          if (created || (rna_itemname_a && !rna_itemname_a->empty()) ||
              (rna_itemname_b && !rna_itemname_b->empty()) || rna_itemindex_a != -1 ||
              rna_itemindex_b != -1)
          {
            const char *subitem_refname = rna_itemname_b ? rna_itemname_b->c_str() : nullptr;
            const char *subitem_locname = rna_itemname_a ? rna_itemname_a->c_str() : nullptr;
            opop = BKE_lib_override_library_property_operation_get(op,
                                                                   LIBOVERRIDE_OP_REPLACE,
                                                                   subitem_refname,
                                                                   subitem_locname,
                                                                   ptrdiff_ctx.rna_itemid_b,
                                                                   ptrdiff_ctx.rna_itemid_a,
                                                                   rna_itemindex_b,
                                                                   rna_itemindex_a,
                                                                   true,
                                                                   nullptr,
                                                                   &created);
            /* Do not use BKE_lib_override_library_operations_tag here, we do not want to validate
             * as used all of its operations. */
            op->tag &= ~LIBOVERRIDE_PROP_OP_TAG_UNUSED;
            opop->tag &= ~LIBOVERRIDE_PROP_OP_TAG_UNUSED;
            if (created) {
              ptrdiff_ctx.rnadiff_ctx.report_flag |= RNA_OVERRIDE_MATCH_RESULT_CREATED;
            }
          }
          else {
            BKE_lib_override_library_operations_tag(op, LIBOVERRIDE_PROP_OP_TAG_UNUSED, false);
          }

          if (is_id && no_ownership) {
            if (opop == nullptr) {
              const char *subitem_refname = rna_itemname_b ? rna_itemname_b->c_str() : nullptr;
              const char *subitem_locname = rna_itemname_a ? rna_itemname_a->c_str() : nullptr;
              opop = BKE_lib_override_library_property_operation_get(op,
                                                                     LIBOVERRIDE_OP_REPLACE,
                                                                     subitem_refname,
                                                                     subitem_locname,
                                                                     ptrdiff_ctx.rna_itemid_b,
                                                                     ptrdiff_ctx.rna_itemid_a,
                                                                     rna_itemindex_b,
                                                                     rna_itemindex_a,
                                                                     true,
                                                                     nullptr,
                                                                     &created);
              /* Do not use BKE_lib_override_library_operations_tag here, we do not want to
               * validate as used all of its operations. */
              op->tag &= ~LIBOVERRIDE_PROP_OP_TAG_UNUSED;
              opop->tag &= ~LIBOVERRIDE_PROP_OP_TAG_UNUSED;
              if (created) {
                ptrdiff_ctx.rnadiff_ctx.report_flag |= RNA_OVERRIDE_MATCH_RESULT_CREATED;
              }
            }

            BLI_assert(propptr_a->data == propptr_a->owner_id);
            BLI_assert(propptr_b->data == propptr_b->owner_id);
            ID *id_a = static_cast<ID *>(propptr_a->data);
            ID *id_b = static_cast<ID *>(propptr_b->data);
            if (ELEM(nullptr, id_a, id_b)) {
              /* In case one of the pointer is nullptr and not the other, we consider that the
               * liboverride is not matching its reference anymore. */
              opop->flag &= ~LIBOVERRIDE_OP_FLAG_IDPOINTER_MATCH_REFERENCE;
            }
            else if ((owner_id_a->tag & ID_TAG_LIBOVERRIDE_NEED_RESYNC) != 0 ||
                     (owner_id_b->tag & ID_TAG_LIBOVERRIDE_NEED_RESYNC) != 0)
            {
              /* In case one of the owner of the checked property is tagged as needing resync, do
               * not change the 'match reference' status of its ID pointer properties overrides,
               * since many non-matching ones are likely due to missing resync. */
              CLOG_DEBUG(
                  &LOG_COMPARE_OVERRIDE,
                  "Not checking matching ID pointer properties, since owner %s is tagged as "
                  "needing resync.\n",
                  id_a->name);
            }
            else if (id_a->override_library != nullptr &&
                     id_a->override_library->reference == id_b)
            {
              opop->flag |= LIBOVERRIDE_OP_FLAG_IDPOINTER_MATCH_REFERENCE;
            }
            else if (id_b->override_library != nullptr &&
                     id_b->override_library->reference == id_a)
            {
              opop->flag |= LIBOVERRIDE_OP_FLAG_IDPOINTER_MATCH_REFERENCE;
            }
            else {
              opop->flag &= ~LIBOVERRIDE_OP_FLAG_IDPOINTER_MATCH_REFERENCE;
            }
          }
        }
      }
    }
    else {
/* In case we got some array/collection like items identifiers, now is the time to generate a
 * proper rna path from those. */
#  define RNA_PATH_BUFFSIZE 8192

      char extended_rna_path_buffer[RNA_PATH_BUFFSIZE];
      char *extended_rna_path = extended_rna_path_buffer;
      size_t extended_rna_path_len = 0;

      /* There may be a propname defined in some cases, while no actual name set
       * (e.g. happens with point cache), in that case too we want to fall back to index.
       * Note that we do not need the RNA path for insertion operations. */
      if (rna_path) {
        if ((rna_itemname_a && !rna_itemname_a->empty()) &&
            (rna_itemname_b && !rna_itemname_b->empty()))
        {
          BLI_assert(*rna_itemname_a == *rna_itemname_b);

          char esc_item_name[RNA_PATH_BUFFSIZE];
          const size_t esc_item_name_len = BLI_str_escape(
              esc_item_name, rna_itemname_a->c_str(), sizeof(esc_item_name));
          extended_rna_path_len = rna_path_len + 2 + esc_item_name_len + 2;
          if (extended_rna_path_len >= RNA_PATH_BUFFSIZE) {
            extended_rna_path = MEM_malloc_arrayN<char>(extended_rna_path_len + 1, __func__);
          }

          memcpy(extended_rna_path, rna_path, rna_path_len);
          extended_rna_path[rna_path_len] = '[';
          extended_rna_path[rna_path_len + 1] = '"';
          memcpy(extended_rna_path + rna_path_len + 2, esc_item_name, esc_item_name_len);
          extended_rna_path[rna_path_len + 2 + esc_item_name_len] = '"';
          extended_rna_path[rna_path_len + 2 + esc_item_name_len + 1] = ']';
          extended_rna_path[extended_rna_path_len] = '\0';
        }
        else if (rna_itemindex_a != -1) { /* Based on index... */
          BLI_assert(rna_itemindex_a == rna_itemindex_b);

          /* low-level specific highly-efficient conversion of positive integer to string. */
          char item_index_buff[32];
          size_t item_index_buff_len = 0;
          if (rna_itemindex_a == 0) {
            item_index_buff[0] = '0';
            item_index_buff_len = 1;
          }
          else {
            uint index;
            for (index = rna_itemindex_a;
                 index > 0 && item_index_buff_len < sizeof(item_index_buff);
                 index /= 10)
            {
              item_index_buff[item_index_buff_len++] = '0' + char(index % 10);
            }
            BLI_assert(index == 0);
          }

          extended_rna_path_len = rna_path_len + item_index_buff_len + 2;
          if (extended_rna_path_len >= RNA_PATH_BUFFSIZE) {
            extended_rna_path = MEM_malloc_arrayN<char>(extended_rna_path_len + 1, __func__);
          }

          memcpy(extended_rna_path, rna_path, rna_path_len);
          extended_rna_path[rna_path_len] = '[';
          for (size_t i = 1; i <= item_index_buff_len; i++) {
            /* The first loop above generated inverted string representation of our index number.
             */
            extended_rna_path[rna_path_len + i] = item_index_buff[item_index_buff_len - i];
          }
          extended_rna_path[rna_path_len + 1 + item_index_buff_len] = ']';
          extended_rna_path[extended_rna_path_len] = '\0';
        }
        else {
          extended_rna_path = (char *)rna_path;
          extended_rna_path_len = rna_path_len;
        }
      }

      const bool match = RNA_struct_override_matches(bmain,
                                                     propptr_a,
                                                     propptr_b,
                                                     extended_rna_path,
                                                     extended_rna_path_len,
                                                     liboverride,
                                                     liboverride_flags,
                                                     &ptrdiff_ctx.rnadiff_ctx.report_flag);
      ptrdiff_ctx.rnadiff_ctx.comparison = !match;

      /* Regardless of whether the data from both pointers matches or not, a potentially existing
       * operation on current extended rna path should be removed. This is done by tagging said
       * operation as unused, while clearing the property tag (see also
       * #RNA_struct_override_matches handling of #LIBOVERRIDE_PROP_OP_TAG_UNUSED). Note that a
       * property with no operations will also be cleared by
       * #BKE_lib_override_library_id_unused_cleanup. */
      IDOverrideLibraryProperty *op = BKE_lib_override_library_property_find(liboverride,
                                                                             extended_rna_path);
      if (op != nullptr) {
        op->tag &= ~LIBOVERRIDE_PROP_OP_TAG_UNUSED;
      }

      if (!ELEM(extended_rna_path, extended_rna_path_buffer, rna_path)) {
        MEM_freeN(extended_rna_path);
      }

#  undef RNA_PATH_BUFFSIZE
    }
  }
  else {
    if (no_ownership || is_null || is_type_diff || !is_valid_for_diffing) {
      /* In case this pointer prop does not own its data (or one is nullptr), do not compare
       * structs! This is a quite safe path to infinite loop, among other nasty issues. Instead,
       * just compare pointers themselves. */
      ptrdiff_ctx.rnadiff_ctx.comparison = (propptr_a->data != propptr_b->data);
    }
    else {
      /* We could also use is_diff_pointer, but then we potentially lose the greater-than/less-than
       * info - and don't think performances are critical here for now anyway. */
      ptrdiff_ctx.rnadiff_ctx.comparison = !RNA_struct_equals(bmain, propptr_a, propptr_b, mode);
    }
  }
}

#  define RNA_PROPERTY_GET_SINGLE(_typename, _ptr, _prop, _index) \
    (is_array ? RNA_property_##_typename##_get_index((_ptr), (_prop), (_index)) : \
                RNA_property_##_typename##_get((_ptr), (_prop)))
#  define RNA_PROPERTY_SET_SINGLE(_typename, _ptr, _prop, _index, _value) \
    (is_array ? RNA_property_##_typename##_set_index((_ptr), (_prop), (_index), (_value)) : \
                RNA_property_##_typename##_set((_ptr), (_prop), (_value)))

/**
 * \return `0` is matching, `-1` if `prop_a < prop_b`, `1` if `prop_a > prop_b`. Note that for
 * unquantifiable properties (e.g. pointers or collections), return value should be interpreted as
 * a boolean (false == matching, true == not matching).
 */
void rna_property_override_diff_default(Main *bmain, RNAPropertyOverrideDiffContext &rnadiff_ctx)
{
  PropertyRNAOrID *prop_a = rnadiff_ctx.prop_a;
  PropertyRNAOrID *prop_b = rnadiff_ctx.prop_b;
  PointerRNA *ptr_a = prop_a->ptr;
  PointerRNA *ptr_b = prop_b->ptr;
  PropertyRNA *rawprop_a = prop_a->rawprop;
  PropertyRNA *rawprop_b = prop_b->rawprop;
  const uint len_a = prop_a->array_len;
  const uint len_b = prop_b->array_len;

  IDOverrideLibrary *liboverride = rnadiff_ctx.liboverride;
  const char *rna_path = rnadiff_ctx.rna_path;
  const int liboverride_flags = rnadiff_ctx.liboverride_flags;

  BLI_assert(len_a == len_b);

  /* NOTE: at this point, we are sure that when len_a is zero,
   * we are not handling an (empty) array. */

  const bool do_create = liboverride != nullptr &&
                         (liboverride_flags & RNA_OVERRIDE_COMPARE_CREATE) != 0 &&
                         rna_path != nullptr;

  const bool no_ownership = (prop_a->rnaprop->flag & PROP_PTR_NO_OWNERSHIP) != 0;

  const PropertyType rna_prop_type = RNA_property_type(prop_a->rnaprop);
  bool created = false;
  IDOverrideLibraryProperty *op = nullptr;

  switch (rna_prop_type) {
    case PROP_BOOLEAN: {
      if (len_a) {
        bool array_stack_a[RNA_STACK_ARRAY], array_stack_b[RNA_STACK_ARRAY];
        bool *array_a, *array_b;

        array_a = (len_a > RNA_STACK_ARRAY) ?
                      MEM_malloc_arrayN<bool>(size_t(len_a), "RNA equals") :
                      array_stack_a;
        array_b = (len_b > RNA_STACK_ARRAY) ?
                      MEM_malloc_arrayN<bool>(size_t(len_b), "RNA equals") :
                      array_stack_b;

        RNA_property_boolean_get_array(ptr_a, rawprop_a, array_a);
        RNA_property_boolean_get_array(ptr_b, rawprop_b, array_b);

        rnadiff_ctx.comparison = memcmp(array_a, array_b, sizeof(bool) * len_a);

        if (do_create && rnadiff_ctx.comparison != 0) {
          /* XXX TODO: this will have to be refined to handle array items. */
          op = BKE_lib_override_library_property_get(liboverride, rna_path, &created);

          if (op != nullptr && created) {
            BKE_lib_override_library_property_operation_get(op,
                                                            LIBOVERRIDE_OP_REPLACE,
                                                            nullptr,
                                                            nullptr,
                                                            {},
                                                            {},
                                                            -1,
                                                            -1,
                                                            true,
                                                            nullptr,
                                                            nullptr);
            rnadiff_ctx.report_flag |= RNA_OVERRIDE_MATCH_RESULT_CREATED;
          }
          else {
            /* Already overridden prop, we'll have to check arrays items etc. */
          }
        }

        if (array_a != array_stack_a) {
          MEM_freeN(array_a);
        }
        if (array_b != array_stack_b) {
          MEM_freeN(array_b);
        }
      }
      else {
        const bool value_a = RNA_property_boolean_get(ptr_a, rawprop_a);
        const bool value_b = RNA_property_boolean_get(ptr_b, rawprop_b);
        rnadiff_ctx.comparison = (value_a < value_b) ? -1 : (value_a > value_b) ? 1 : 0;

        if (do_create && rnadiff_ctx.comparison != 0) {
          op = BKE_lib_override_library_property_get(liboverride, rna_path, &created);

          if (op != nullptr && created) { /* If not yet overridden... */
            BKE_lib_override_library_property_operation_get(op,
                                                            LIBOVERRIDE_OP_REPLACE,
                                                            nullptr,
                                                            nullptr,
                                                            {},
                                                            {},
                                                            -1,
                                                            -1,
                                                            true,
                                                            nullptr,
                                                            nullptr);
            rnadiff_ctx.report_flag |= RNA_OVERRIDE_MATCH_RESULT_CREATED;
          }
        }
      }
      break;
    }

    case PROP_INT: {
      if (len_a) {
        int array_stack_a[RNA_STACK_ARRAY], array_stack_b[RNA_STACK_ARRAY];
        int *array_a, *array_b;

        array_a = (len_a > RNA_STACK_ARRAY) ? MEM_malloc_arrayN<int>(size_t(len_a), "RNA equals") :
                                              array_stack_a;
        array_b = (len_b > RNA_STACK_ARRAY) ? MEM_malloc_arrayN<int>(size_t(len_b), "RNA equals") :
                                              array_stack_b;

        RNA_property_int_get_array(ptr_a, rawprop_a, array_a);
        RNA_property_int_get_array(ptr_b, rawprop_b, array_b);

        rnadiff_ctx.comparison = memcmp(array_a, array_b, sizeof(int) * len_a);

        if (do_create && rnadiff_ctx.comparison != 0) {
          /* XXX TODO: this will have to be refined to handle array items. */
          op = BKE_lib_override_library_property_get(liboverride, rna_path, &created);

          if (op != nullptr && created) {
            BKE_lib_override_library_property_operation_get(op,
                                                            LIBOVERRIDE_OP_REPLACE,
                                                            nullptr,
                                                            nullptr,
                                                            {},
                                                            {},
                                                            -1,
                                                            -1,
                                                            true,
                                                            nullptr,
                                                            nullptr);
            if (created) {
              rnadiff_ctx.report_flag |= RNA_OVERRIDE_MATCH_RESULT_CREATED;
            }
          }
          else {
            /* Already overridden prop, we'll have to check arrays items etc. */
          }
        }

        if (array_a != array_stack_a) {
          MEM_freeN(array_a);
        }
        if (array_b != array_stack_b) {
          MEM_freeN(array_b);
        }
      }
      else {
        const int value_a = RNA_property_int_get(ptr_a, rawprop_a);
        const int value_b = RNA_property_int_get(ptr_b, rawprop_b);
        rnadiff_ctx.comparison = (value_a < value_b) ? -1 : (value_a > value_b) ? 1 : 0;

        if (do_create && rnadiff_ctx.comparison != 0) {
          op = BKE_lib_override_library_property_get(liboverride, rna_path, &created);

          if (op != nullptr && created) { /* If not yet overridden... */
            BKE_lib_override_library_property_operation_get(op,
                                                            LIBOVERRIDE_OP_REPLACE,
                                                            nullptr,
                                                            nullptr,
                                                            {},
                                                            {},
                                                            -1,
                                                            -1,
                                                            true,
                                                            nullptr,
                                                            nullptr);
            rnadiff_ctx.report_flag |= RNA_OVERRIDE_MATCH_RESULT_CREATED;
          }
        }
      }
      break;
    }

    case PROP_FLOAT: {
      if (len_a) {
        float array_stack_a[RNA_STACK_ARRAY], array_stack_b[RNA_STACK_ARRAY];
        float *array_a, *array_b;

        array_a = (len_a > RNA_STACK_ARRAY) ?
                      MEM_malloc_arrayN<float>(size_t(len_a), "RNA equals") :
                      array_stack_a;
        array_b = (len_b > RNA_STACK_ARRAY) ?
                      MEM_malloc_arrayN<float>(size_t(len_b), "RNA equals") :
                      array_stack_b;

        RNA_property_float_get_array(ptr_a, rawprop_a, array_a);
        RNA_property_float_get_array(ptr_b, rawprop_b, array_b);

        rnadiff_ctx.comparison = memcmp(array_a, array_b, sizeof(float) * len_a);

        if (do_create && rnadiff_ctx.comparison != 0) {
          /* XXX TODO: this will have to be refined to handle array items. */
          op = BKE_lib_override_library_property_get(liboverride, rna_path, &created);

          if (op != nullptr && created) {
            BKE_lib_override_library_property_operation_get(op,
                                                            LIBOVERRIDE_OP_REPLACE,
                                                            nullptr,
                                                            nullptr,
                                                            {},
                                                            {},
                                                            -1,
                                                            -1,
                                                            true,
                                                            nullptr,
                                                            nullptr);
            rnadiff_ctx.report_flag |= RNA_OVERRIDE_MATCH_RESULT_CREATED;
          }
          else {
            /* Already overridden prop, we'll have to check arrays items etc. */
          }
        }

        if (array_a != array_stack_a) {
          MEM_freeN(array_a);
        }
        if (array_b != array_stack_b) {
          MEM_freeN(array_b);
        }
      }
      else {
        const float value_a = RNA_property_float_get(ptr_a, rawprop_a);
        const float value_b = RNA_property_float_get(ptr_b, rawprop_b);
        rnadiff_ctx.comparison = (value_a < value_b) ? -1 : (value_a > value_b) ? 1 : 0;

        if (do_create && rnadiff_ctx.comparison != 0) {
          op = BKE_lib_override_library_property_get(liboverride, rna_path, &created);

          if (op != nullptr && created) { /* If not yet overridden... */
            BKE_lib_override_library_property_operation_get(op,
                                                            LIBOVERRIDE_OP_REPLACE,
                                                            nullptr,
                                                            nullptr,
                                                            {},
                                                            {},
                                                            -1,
                                                            -1,
                                                            true,
                                                            nullptr,
                                                            nullptr);
            rnadiff_ctx.report_flag |= RNA_OVERRIDE_MATCH_RESULT_CREATED;
          }
        }
      }
      break;
    }

    case PROP_ENUM: {
      const int value_a = RNA_property_enum_get(ptr_a, rawprop_a);
      const int value_b = RNA_property_enum_get(ptr_b, rawprop_b);
      rnadiff_ctx.comparison = value_a != value_b;

      if (do_create && rnadiff_ctx.comparison != 0) {
        op = BKE_lib_override_library_property_get(liboverride, rna_path, &created);

        if (op != nullptr && created) { /* If not yet overridden... */
          BKE_lib_override_library_property_operation_get(op,
                                                          LIBOVERRIDE_OP_REPLACE,
                                                          nullptr,
                                                          nullptr,
                                                          {},
                                                          {},
                                                          -1,
                                                          -1,
                                                          true,
                                                          nullptr,
                                                          nullptr);
          rnadiff_ctx.report_flag |= RNA_OVERRIDE_MATCH_RESULT_CREATED;
        }
      }
      break;
    }

    case PROP_STRING: {
      char fixed_a[4096], fixed_b[4096];
      int len_str_a, len_str_b;
      char *value_a = RNA_property_string_get_alloc(
          ptr_a, rawprop_a, fixed_a, sizeof(fixed_a), &len_str_a);
      char *value_b = RNA_property_string_get_alloc(
          ptr_b, rawprop_b, fixed_b, sizeof(fixed_b), &len_str_b);
/* TODO: we could do a check on length too,
 * but then we would not have a 'real' string comparison...
 * Maybe behind a eRNAOverrideMatch flag? */
#  if 0
      rnadiff_ctx.comparison = len_str_a < len_str_b ? -1 :
                               len_str_a > len_str_b ? 1 :
                                                       strcmp(value_a, value_b);
#  endif
      rnadiff_ctx.comparison = strcmp(value_a, value_b);

      if (do_create && rnadiff_ctx.comparison != 0) {
        op = BKE_lib_override_library_property_get(liboverride, rna_path, &created);

        if (op != nullptr && created) { /* If not yet overridden... */
          BKE_lib_override_library_property_operation_get(op,
                                                          LIBOVERRIDE_OP_REPLACE,
                                                          nullptr,
                                                          nullptr,
                                                          {},
                                                          {},
                                                          -1,
                                                          -1,
                                                          true,
                                                          nullptr,
                                                          nullptr);
          rnadiff_ctx.report_flag |= RNA_OVERRIDE_MATCH_RESULT_CREATED;
        }
      }

      if (value_a != fixed_a) {
        MEM_freeN(value_a);
      }
      if (value_b != fixed_b) {
        MEM_freeN(value_b);
      }
      break;
    }

    case PROP_POINTER: {
      /* Using property name check only makes sense for items of a collection, not for a single
       * pointer.
       * Doing this here avoids having to manually specify `PROPOVERRIDE_NO_PROP_NAME` to things
       * like ShapeKey pointers. */
      if (STREQ(prop_a->identifier, "rna_type")) {
        /* Dummy 'pass' answer, this is a meta-data and must be ignored... */
        return;
      }
      else {
        RNACompareOverrideDiffPropPtrContext ptrdiff_ctx(rnadiff_ctx);
        ptrdiff_ctx.owner_id_a = ptr_a->owner_id;
        ptrdiff_ctx.owner_id_b = ptr_b->owner_id;
        /* Do not create empty IDPGroup properties here. This is not only a totally useless
         * overhead, but it also remains unsafe in parallelized context, despite the usage of a
         * mutex in #property_pointer_get(). See also #146221. */
        ptrdiff_ctx.propptr_a = RNA_property_pointer_get_never_create(ptr_a, rawprop_a);
        ptrdiff_ctx.propptr_b = RNA_property_pointer_get_never_create(ptr_b, rawprop_b);
        ptrdiff_ctx.no_prop_name = true;
        ptrdiff_ctx.no_ownership = no_ownership;
        ptrdiff_ctx.property_type = PROP_POINTER;
        /* IDProperties (real or backend storage for dynamic RNA) have a default handling for
         * 'pointer' data creation. */
        if (prop_a->is_idprop || prop_a->is_rna_storage_idprop || prop_a->rnaprop->override_apply)
        {
          BLI_assert(prop_a->is_idprop == prop_b->is_idprop);
          BLI_assert(prop_a->is_rna_storage_idprop == prop_b->is_rna_storage_idprop);
          BLI_assert(prop_a->rnaprop->override_apply == prop_b->rnaprop->override_apply);
          ptrdiff_ctx.has_liboverride_apply_cb = true;
        }
        rna_property_override_diff_propptr(bmain, ptrdiff_ctx);
      }
      break;
    }

    case PROP_COLLECTION: {
      const bool no_prop_name = (prop_a->rnaprop->flag_override & PROPOVERRIDE_NO_PROP_NAME) != 0;

      /* IDProperties (real or backend storage for dynamic RNA) have a default handling for
       * 'pointer' data creation and 'collection' items insertion. */
      BLI_assert(prop_a->is_idprop == prop_b->is_idprop);
      BLI_assert(prop_a->is_rna_storage_idprop == prop_b->is_rna_storage_idprop);
      BLI_assert(prop_a->rnaprop->override_apply == prop_b->rnaprop->override_apply);
      const bool has_liboverride_apply_cb = (prop_a->is_idprop || prop_a->is_rna_storage_idprop ||
                                             prop_a->rnaprop->override_apply);
      if (!has_liboverride_apply_cb &&
          (prop_a->rnaprop->flag_override & PROPOVERRIDE_LIBRARY_INSERTION) != 0)
      {
        CLOG_ERROR(&LOG_COMPARE_OVERRIDE,
                   "RNA collection '%s' defined as supporting liboverride insertion of items, but "
                   "no liboverride apply callback defined for it. No insertion will happen.",
                   rna_path);
      }

      /* NOTE: we assume we only insert in ptr_a (i.e. we can only get new items in ptr_a),
       * and that we never remove anything. */
      const bool use_collection_insertion = has_liboverride_apply_cb && do_create &&
                                            (prop_a->rnaprop->flag_override &
                                             PROPOVERRIDE_LIBRARY_INSERTION) != 0;

      bool equals = true;
      bool abort = false;
      int idx_a = 0;
      int idx_b = 0;
      std::optional<std::string> prev_rna_itemname_a;
      std::optional<ID *> prev_rna_itemid_a;

      CollectionPropertyIterator iter_a, iter_b;
      RNA_property_collection_begin(ptr_a, rawprop_a, &iter_a);
      RNA_property_collection_begin(ptr_b, rawprop_b, &iter_b);

      if (use_collection_insertion) {
        /* We need to clean up all possible existing insertion operations, and then re-generate
         * them, otherwise we'd end up with a mess of opop's every time something changes. */
        op = BKE_lib_override_library_property_find(liboverride, rna_path);
        if (op != nullptr) {
          LISTBASE_FOREACH_MUTABLE (IDOverrideLibraryPropertyOperation *, opop, &op->operations) {
            if (ELEM(opop->operation, LIBOVERRIDE_OP_INSERT_AFTER, LIBOVERRIDE_OP_INSERT_BEFORE)) {
              BKE_lib_override_library_property_operation_delete(op, opop);
            }
          }
          op = nullptr;
        }
      }

      for (; iter_a.valid && !abort;) {
        do {
          bool is_valid_for_insertion = use_collection_insertion;

          RNACompareOverrideDiffPropPtrContext ptrdiff_ctx(rnadiff_ctx);
          ptrdiff_ctx.owner_id_a = ptr_a->owner_id;
          ptrdiff_ctx.owner_id_b = ptr_b->owner_id;
          ptrdiff_ctx.propptr_a = iter_a.ptr;
          if (iter_b.valid) {
            ptrdiff_ctx.propptr_b = iter_b.ptr;
          }
          ptrdiff_ctx.no_prop_name = no_prop_name;
          ptrdiff_ctx.do_force_name = !no_prop_name;
          ptrdiff_ctx.use_id_pointer = !no_prop_name;
          ptrdiff_ctx.no_ownership = no_ownership;
          ptrdiff_ctx.property_type = PROP_COLLECTION;
          ptrdiff_ctx.has_liboverride_apply_cb = has_liboverride_apply_cb;

          if (iter_b.valid || use_collection_insertion) {
            rna_property_override_diff_propptr_validate_diffing(ptrdiff_ctx);
            /* Getting props names has already been done in call above, no need to redo it later
             * when calling #rna_property_override_diff_propptr. */
            ptrdiff_ctx.no_prop_name = true;
            ptrdiff_ctx.do_force_name = false;
            ptrdiff_ctx.use_id_pointer = false;
          }

          const bool is_valid_for_diffing = ptrdiff_ctx.is_valid_for_diffing;
          const bool is_id = ptrdiff_ctx.is_id;

          /* We do not support insertion of IDs for now, neither handle nullptr pointers. */
          if (is_id || is_valid_for_diffing) {
            is_valid_for_insertion = false;
          }

#  if 0
          if (rna_path) {
            printf(
                "Checking %s, %s [%d] vs %s [%d]; is_id: %d, diffing: %d; "
                "insert: %d (could be used: %d, do_create: %d)\n",
                rna_path,
                ptrdiff_ctx.rna_itemname_a ? ptrdiff_ctx.rna_itemname_a->c_str() : "",
                idx_a,
                ptrdiff_ctx.rna_itemname_b ? ptrdiff_ctx.rna_itemname_b->c_str() : "",
                idx_b,
                is_id,
                is_valid_for_diffing,
                is_valid_for_insertion,
                use_collection_insertion,
                do_create);
          }
#  endif

          if (!(is_id || is_valid_for_diffing || is_valid_for_insertion)) {
            /* Differences we cannot handle, we can break here. */
            equals = false;
            abort = true;
            break;
          }

          /* Collections do not support replacement of their data (except for collections of ID
           * pointers), since they do not support removing, only in *some* cases, insertion. We
           * also assume then that _a data is the one where things are inserted.
           *
           * NOTE: In insertion case, both 'local' and 'reference' (aka anchor) sub-item
           * identifiers refer to collection items in the local override. The 'reference' may match
           * an item in the linked reference data, but it can also be another local-only item added
           * by a previous INSERT operation. */
          if (is_valid_for_insertion && use_collection_insertion) {
            op = BKE_lib_override_library_property_get(liboverride, rna_path, &created);

            BKE_lib_override_library_property_operation_get(
                op,
                LIBOVERRIDE_OP_INSERT_AFTER,
                (no_prop_name || !prev_rna_itemname_a) ? nullptr : prev_rna_itemname_a->c_str(),
                (no_prop_name || !ptrdiff_ctx.rna_itemname_a) ?
                    nullptr :
                    ptrdiff_ctx.rna_itemname_a->c_str(),
                prev_rna_itemid_a,
                ptrdiff_ctx.rna_itemid_a,
                idx_a - 1,
                idx_a,
                true,
                nullptr,
                nullptr);
#  if 0
            printf("%s: Adding insertion op override after '%s'/%d\n",
                   rna_path,
                   prev_rna_itemname_a ? prev_rna_itemname_a->c_str() : "",
                   idx_a - 1);
#  endif
            op = nullptr;

            equals = false;
          }
          else if (is_id || is_valid_for_diffing) {
            if (equals || do_create) {
              ptrdiff_ctx.rna_itemindex_a = idx_a;
              ptrdiff_ctx.rna_itemindex_b = idx_b;
              rna_property_override_diff_propptr(bmain, ptrdiff_ctx);
              equals = equals && (rnadiff_ctx.comparison == 0);
            }
          }

          if (ptrdiff_ctx.rna_itemname_a) {
            prev_rna_itemname_a = std::move(*ptrdiff_ctx.rna_itemname_a);
          }
          else {
            prev_rna_itemname_a.reset();
          }
          prev_rna_itemid_a = ptrdiff_ctx.rna_itemid_a;

          if (!do_create && !equals) {
            abort = true; /* Early out in case we do not want to loop over whole collection. */
            break;
          }

          if (!(use_collection_insertion && !(is_id || is_valid_for_diffing))) {
            break;
          }

          if (iter_a.valid) {
            RNA_property_collection_next(&iter_a);
            idx_a++;
          }
        } while (iter_a.valid);

        if (iter_a.valid) {
          RNA_property_collection_next(&iter_a);
          idx_a++;
        }
        if (iter_b.valid) {
          RNA_property_collection_next(&iter_b);
          idx_b++;
        }
      }

      /* Not same number of items in both collections. */
      equals = equals && !(iter_a.valid || iter_b.valid) && !abort;
      RNA_property_collection_end(&iter_a);
      RNA_property_collection_end(&iter_b);

      rnadiff_ctx.comparison = (equals == false);
      return;
    }

    default:
      break;
  }

  if (op != nullptr) {
    /* In theory, if the liboverride operation already existed, it should already be of the right
     * type. However, in some rare cases a same exact RNA path can end up pointing at different
     * data of a different path than when the liboverride property was created, so just always
     * ensure the type is now valid. */
    op->rna_prop_type = rna_prop_type;
  }

  return;
}

bool rna_property_override_store_default(Main * /*bmain*/,
                                         PointerRNA *ptr_local,
                                         PointerRNA *ptr_reference,
                                         PointerRNA *ptr_storage,
                                         PropertyRNA *prop_local,
                                         PropertyRNA *prop_reference,
                                         PropertyRNA *prop_storage,
                                         const int len_local,
                                         const int len_reference,
                                         const int len_storage,
                                         IDOverrideLibraryPropertyOperation *opop)
{
  BLI_assert(len_local == len_reference && (!ptr_storage || len_local == len_storage));
  UNUSED_VARS_NDEBUG(len_reference, len_storage);

  bool changed = false;
  const bool is_array = len_local > 0;
  const int index = is_array ? opop->subitem_reference_index : 0;

  if (!ELEM(opop->operation, LIBOVERRIDE_OP_ADD, LIBOVERRIDE_OP_SUBTRACT, LIBOVERRIDE_OP_MULTIPLY))
  {
    return changed;
  }

  /* XXX TODO: About range limits.
   * Ideally, it would be great to get rid of RNA range in that specific case.
   * However, this won't be that easy and will add yet another layer of complexity in
   * generated code, not to mention that we could most likely *not* bypass custom setters anyway.
   * So for now, if needed second operand value is not in valid range, we simply fall back
   * to a mere REPLACE operation.
   * Time will say whether this is acceptable limitation or not. */
  switch (RNA_property_type(prop_local)) {
    case PROP_BOOLEAN:
      /* TODO: support boolean ops? Really doubt this would ever be useful though. */
      BLI_assert_msg(0, "Boolean properties support no override diff operation");
      break;
    case PROP_INT: {
      int prop_min, prop_max;
      RNA_property_int_range(ptr_local, prop_local, &prop_min, &prop_max);

      if (is_array && index == -1) {
        int array_stack_a[RNA_STACK_ARRAY], array_stack_b[RNA_STACK_ARRAY];
        int *array_a, *array_b;

        array_a = (len_local > RNA_STACK_ARRAY) ?
                      MEM_malloc_arrayN<int>(size_t(len_local), __func__) :
                      array_stack_a;
        RNA_property_int_get_array(ptr_reference, prop_reference, array_a);

        switch (opop->operation) {
          case LIBOVERRIDE_OP_ADD:
          case LIBOVERRIDE_OP_SUBTRACT: {
            const int fac = opop->operation == LIBOVERRIDE_OP_ADD ? 1 : -1;
            const int other_op = opop->operation == LIBOVERRIDE_OP_ADD ? LIBOVERRIDE_OP_SUBTRACT :
                                                                         LIBOVERRIDE_OP_ADD;
            bool do_set = true;
            array_b = (len_local > RNA_STACK_ARRAY) ?
                          MEM_malloc_arrayN<int>(size_t(len_local), __func__) :
                          array_stack_b;
            RNA_property_int_get_array(ptr_local, prop_local, array_b);
            for (int i = len_local; i--;) {
              array_b[i] = fac * (array_b[i] - array_a[i]);
              if (array_b[i] < prop_min || array_b[i] > prop_max) {
                opop->operation = other_op;
                for (int j = len_local; j--;) {
                  array_b[j] = j >= i ? -array_b[j] : fac * (array_a[j] - array_b[j]);
                  if (array_b[j] < prop_min || array_b[j] > prop_max) {
                    /* We failed to find a suitable diff op,
                     * fall back to plain REPLACE one. */
                    opop->operation = LIBOVERRIDE_OP_REPLACE;
                    do_set = false;
                    break;
                  }
                }
                break;
              }
            }
            if (do_set) {
              changed = true;
              RNA_property_int_set_array(ptr_storage, prop_storage, array_b);
            }
            if (array_b != array_stack_b) {
              MEM_freeN(array_b);
            }
            break;
          }
          default:
            BLI_assert_msg(0, "Unsupported RNA override diff operation on integer");
            break;
        }

        if (array_a != array_stack_a) {
          MEM_freeN(array_a);
        }
      }
      else {
        const int value = RNA_PROPERTY_GET_SINGLE(int, ptr_reference, prop_reference, index);

        switch (opop->operation) {
          case LIBOVERRIDE_OP_ADD:
          case LIBOVERRIDE_OP_SUBTRACT: {
            const int fac = opop->operation == LIBOVERRIDE_OP_ADD ? 1 : -1;
            const int other_op = opop->operation == LIBOVERRIDE_OP_ADD ? LIBOVERRIDE_OP_SUBTRACT :
                                                                         LIBOVERRIDE_OP_ADD;
            int b = fac * (RNA_PROPERTY_GET_SINGLE(int, ptr_local, prop_local, index) - value);
            if (b < prop_min || b > prop_max) {
              opop->operation = other_op;
              b = -b;
              if (b < prop_min || b > prop_max) {
                opop->operation = LIBOVERRIDE_OP_REPLACE;
                break;
              }
            }
            changed = true;
            RNA_PROPERTY_SET_SINGLE(int, ptr_storage, prop_storage, index, b);
            break;
          }
          default:
            BLI_assert_msg(0, "Unsupported RNA override diff operation on integer");
            break;
        }
      }
      break;
    }
    case PROP_FLOAT: {
      float prop_min, prop_max;
      RNA_property_float_range(ptr_local, prop_local, &prop_min, &prop_max);

      if (is_array && index == -1) {
        float array_stack_a[RNA_STACK_ARRAY], array_stack_b[RNA_STACK_ARRAY];
        float *array_a, *array_b;

        array_a = (len_local > RNA_STACK_ARRAY) ?
                      MEM_malloc_arrayN<float>(size_t(len_local), __func__) :
                      array_stack_a;

        RNA_property_float_get_array(ptr_reference, prop_reference, array_a);
        switch (opop->operation) {
          case LIBOVERRIDE_OP_ADD:
          case LIBOVERRIDE_OP_SUBTRACT: {
            const float fac = opop->operation == LIBOVERRIDE_OP_ADD ? 1.0 : -1.0;
            const int other_op = opop->operation == LIBOVERRIDE_OP_ADD ? LIBOVERRIDE_OP_SUBTRACT :
                                                                         LIBOVERRIDE_OP_ADD;
            bool do_set = true;
            array_b = (len_local > RNA_STACK_ARRAY) ?
                          MEM_malloc_arrayN<float>(size_t(len_local), __func__) :
                          array_stack_b;
            RNA_property_float_get_array(ptr_local, prop_local, array_b);
            for (int i = len_local; i--;) {
              array_b[i] = fac * (array_b[i] - array_a[i]);
              if (array_b[i] < prop_min || array_b[i] > prop_max) {
                opop->operation = other_op;
                for (int j = len_local; j--;) {
                  array_b[j] = j >= i ? -array_b[j] : fac * (array_a[j] - array_b[j]);
                  if (array_b[j] < prop_min || array_b[j] > prop_max) {
                    /* We failed to find a suitable diff op,
                     * fall back to plain REPLACE one. */
                    opop->operation = LIBOVERRIDE_OP_REPLACE;
                    do_set = false;
                    break;
                  }
                }
                break;
              }
            }
            if (do_set) {
              changed = true;
              RNA_property_float_set_array(ptr_storage, prop_storage, array_b);
            }
            if (array_b != array_stack_b) {
              MEM_freeN(array_b);
            }
            break;
          }
          case LIBOVERRIDE_OP_MULTIPLY: {
            bool do_set = true;
            array_b = (len_local > RNA_STACK_ARRAY) ?
                          MEM_malloc_arrayN<float>(size_t(len_local), __func__) :
                          array_stack_b;
            RNA_property_float_get_array(ptr_local, prop_local, array_b);
            for (int i = len_local; i--;) {
              array_b[i] = array_a[i] == 0.0f ? array_b[i] : array_b[i] / array_a[i];
              if (array_b[i] < prop_min || array_b[i] > prop_max) {
                opop->operation = LIBOVERRIDE_OP_REPLACE;
                do_set = false;
                break;
              }
            }
            if (do_set) {
              changed = true;
              RNA_property_float_set_array(ptr_storage, prop_storage, array_b);
            }
            if (array_b != array_stack_b) {
              MEM_freeN(array_b);
            }
            break;
          }
          default:
            BLI_assert_msg(0, "Unsupported RNA override diff operation on float");
            break;
        }

        if (array_a != array_stack_a) {
          MEM_freeN(array_a);
        }
      }
      else {
        const float value = RNA_PROPERTY_GET_SINGLE(float, ptr_reference, prop_reference, index);

        switch (opop->operation) {
          case LIBOVERRIDE_OP_ADD:
          case LIBOVERRIDE_OP_SUBTRACT: {
            const float fac = opop->operation == LIBOVERRIDE_OP_ADD ? 1.0f : -1.0f;
            const int other_op = opop->operation == LIBOVERRIDE_OP_ADD ? LIBOVERRIDE_OP_SUBTRACT :
                                                                         LIBOVERRIDE_OP_ADD;
            float b = fac * (RNA_PROPERTY_GET_SINGLE(float, ptr_local, prop_local, index) - value);
            if (b < prop_min || b > prop_max) {
              opop->operation = other_op;
              b = -b;
              if (b < prop_min || b > prop_max) {
                opop->operation = LIBOVERRIDE_OP_REPLACE;
                break;
              }
            }
            changed = true;
            RNA_PROPERTY_SET_SINGLE(float, ptr_storage, prop_storage, index, b);
            break;
          }
          case LIBOVERRIDE_OP_MULTIPLY: {
            const float b = RNA_property_float_get_index(ptr_local, prop_local, index) /
                            (value == 0.0f ? 1.0f : value);
            if (b < prop_min || b > prop_max) {
              opop->operation = LIBOVERRIDE_OP_REPLACE;
              break;
            }
            changed = true;
            RNA_property_float_set_index(ptr_storage, prop_storage, index, b);
            break;
          }
          default:
            BLI_assert_msg(0, "Unsupported RNA override diff operation on float");
            break;
        }
      }
      return true;
    }
    case PROP_ENUM:
      /* TODO: support add/sub, for bitflags? */
      BLI_assert_msg(0, "Enum properties support no override diff operation");
      break;
    case PROP_POINTER:
      BLI_assert_msg(0, "Pointer properties support no override diff operation");
      break;
    case PROP_STRING:
      BLI_assert_msg(0, "String properties support no override diff operation");
      break;
    case PROP_COLLECTION:
      /* XXX TODO: support this of course... */
      BLI_assert_msg(0, "Collection properties support no override diff operation");
      break;
    default:
      break;
  }

  return changed;
}

bool rna_property_override_apply_default(Main *bmain,
                                         RNAPropertyOverrideApplyContext &rnaapply_ctx)
{
  PointerRNA *ptr_dst = &rnaapply_ctx.ptr_dst;
  PointerRNA *ptr_src = &rnaapply_ctx.ptr_src;
  PointerRNA *ptr_storage = &rnaapply_ctx.ptr_storage;
  PropertyRNA *prop_dst = rnaapply_ctx.prop_dst;
  PropertyRNA *prop_src = rnaapply_ctx.prop_src;
  PropertyRNA *prop_storage = rnaapply_ctx.prop_storage;
  const int len_dst = rnaapply_ctx.len_src;
  const int len_src = rnaapply_ctx.len_src;
  const int len_storage = rnaapply_ctx.len_storage;
  IDOverrideLibraryPropertyOperation *opop = rnaapply_ctx.liboverride_operation;

  const PropertyType prop_src_type = RNA_property_type(prop_src);
  const PropertyType prop_dst_type = RNA_property_type(prop_dst);

  /* It is possible that a same exact RNA path points to a different property of a different type
   * (due to changes in the program, or in some custom data...). */
  if (prop_src_type != prop_dst_type ||
      (prop_storage && prop_src_type != RNA_property_type(prop_storage)))
  {
    std::optional<std::string> prop_rna_path = rnaapply_ctx.liboverride_property ?
                                                   rnaapply_ctx.liboverride_property->rna_path :
                                                   RNA_path_from_ID_to_property(ptr_dst, prop_dst);
    CLOG_WARN(&LOG_COMPARE_OVERRIDE,
              "%s.%s: Inconsistency between stored property type (%d) and linked reference one "
              "(%d), skipping liboverride apply",
              ptr_dst->owner_id->name,
              prop_rna_path ? prop_rna_path->c_str() :
                              fmt::format(" ... .{}", prop_dst->name).c_str(),
              prop_src_type,
              prop_dst_type);
    /* Keep the liboverride property, update its type to the new actual one. */
    if (rnaapply_ctx.liboverride_property) {
      rnaapply_ctx.liboverride_property->rna_prop_type = prop_dst_type;
    }
    return false;
  }

  BLI_assert(len_dst == len_src && (!prop_storage || len_dst == len_storage));
  UNUSED_VARS_NDEBUG(len_src, len_storage);

  const bool is_array = len_dst > 0;
  const int index = is_array ? opop->subitem_reference_index : 0;
  const short override_op = opop->operation;

  bool ret_success = true;

  switch (RNA_property_type(prop_dst)) {
    case PROP_BOOLEAN:
      if (is_array && index == -1) {
        bool array_stack_a[RNA_STACK_ARRAY];
        bool *array_a;

        array_a = (len_dst > RNA_STACK_ARRAY) ?
                      MEM_malloc_arrayN<bool>(size_t(len_dst), __func__) :
                      array_stack_a;

        RNA_property_boolean_get_array(ptr_src, prop_src, array_a);

        switch (override_op) {
          case LIBOVERRIDE_OP_REPLACE:
            RNA_property_boolean_set_array(ptr_dst, prop_dst, array_a);
            break;
          default:
            BLI_assert_msg(0, "Unsupported RNA override operation on boolean");
            return false;
        }

        if (array_a != array_stack_a) {
          MEM_freeN(array_a);
        }
      }
      else {
        const bool value = RNA_PROPERTY_GET_SINGLE(boolean, ptr_src, prop_src, index);

        switch (override_op) {
          case LIBOVERRIDE_OP_REPLACE:
            RNA_PROPERTY_SET_SINGLE(boolean, ptr_dst, prop_dst, index, value);
            break;
          default:
            BLI_assert_msg(0, "Unsupported RNA override operation on boolean");
            return false;
        }
      }
      break;
    case PROP_INT:
      if (is_array && index == -1) {
        int array_stack_a[RNA_STACK_ARRAY], array_stack_b[RNA_STACK_ARRAY];
        int *array_a, *array_b;

        array_a = (len_dst > RNA_STACK_ARRAY) ? MEM_malloc_arrayN<int>(size_t(len_dst), __func__) :
                                                array_stack_a;

        switch (override_op) {
          case LIBOVERRIDE_OP_REPLACE:
            RNA_property_int_get_array(ptr_src, prop_src, array_a);
            RNA_property_int_set_array(ptr_dst, prop_dst, array_a);
            break;
          case LIBOVERRIDE_OP_ADD:
          case LIBOVERRIDE_OP_SUBTRACT:
            RNA_property_int_get_array(ptr_dst, prop_dst, array_a);
            array_b = (len_dst > RNA_STACK_ARRAY) ?
                          MEM_malloc_arrayN<int>(size_t(len_dst), __func__) :
                          array_stack_b;
            RNA_property_int_get_array(ptr_storage, prop_storage, array_b);
            if (override_op == LIBOVERRIDE_OP_ADD) {
              for (int i = len_dst; i--;) {
                array_a[i] += array_b[i];
              }
            }
            else {
              for (int i = len_dst; i--;) {
                array_a[i] -= array_b[i];
              }
            }
            RNA_property_int_set_array(ptr_dst, prop_dst, array_a);
            if (array_b != array_stack_b) {
              MEM_freeN(array_b);
            }
            break;
          default:
            BLI_assert_msg(0, "Unsupported RNA override operation on integer");
            return false;
        }

        if (array_a != array_stack_a) {
          MEM_freeN(array_a);
        }
      }
      else {
        const int storage_value = prop_storage ? RNA_PROPERTY_GET_SINGLE(
                                                     int, ptr_storage, prop_storage, index) :
                                                 0;

        switch (override_op) {
          case LIBOVERRIDE_OP_REPLACE:
            RNA_PROPERTY_SET_SINGLE(int,
                                    ptr_dst,
                                    prop_dst,
                                    index,
                                    RNA_PROPERTY_GET_SINGLE(int, ptr_src, prop_src, index));
            break;
          case LIBOVERRIDE_OP_ADD:
            RNA_PROPERTY_SET_SINGLE(int,
                                    ptr_dst,
                                    prop_dst,
                                    index,
                                    RNA_PROPERTY_GET_SINGLE(int, ptr_dst, prop_dst, index) -
                                        storage_value);
            break;
          case LIBOVERRIDE_OP_SUBTRACT:
            RNA_PROPERTY_SET_SINGLE(int,
                                    ptr_dst,
                                    prop_dst,
                                    index,
                                    RNA_PROPERTY_GET_SINGLE(int, ptr_dst, prop_dst, index) -
                                        storage_value);
            break;
          default:
            BLI_assert_msg(0, "Unsupported RNA override operation on integer");
            return false;
        }
      }
      break;
    case PROP_FLOAT:
      if (is_array && index == -1) {
        float array_stack_a[RNA_STACK_ARRAY], array_stack_b[RNA_STACK_ARRAY];
        float *array_a, *array_b;

        array_a = (len_dst > RNA_STACK_ARRAY) ?
                      MEM_malloc_arrayN<float>(size_t(len_dst), __func__) :
                      array_stack_a;

        switch (override_op) {
          case LIBOVERRIDE_OP_REPLACE:
            RNA_property_float_get_array(ptr_src, prop_src, array_a);
            RNA_property_float_set_array(ptr_dst, prop_dst, array_a);
            break;
          case LIBOVERRIDE_OP_ADD:
          case LIBOVERRIDE_OP_SUBTRACT:
          case LIBOVERRIDE_OP_MULTIPLY:
            RNA_property_float_get_array(ptr_dst, prop_dst, array_a);
            array_b = (len_dst > RNA_STACK_ARRAY) ?
                          MEM_malloc_arrayN<float>(size_t(len_dst), __func__) :
                          array_stack_b;
            RNA_property_float_get_array(ptr_storage, prop_storage, array_b);
            if (override_op == LIBOVERRIDE_OP_ADD) {
              for (int i = len_dst; i--;) {
                array_a[i] += array_b[i];
              }
            }
            else if (override_op == LIBOVERRIDE_OP_SUBTRACT) {
              for (int i = len_dst; i--;) {
                array_a[i] -= array_b[i];
              }
            }
            else {
              for (int i = len_dst; i--;) {
                array_a[i] *= array_b[i];
              }
            }
            RNA_property_float_set_array(ptr_dst, prop_dst, array_a);
            if (array_b != array_stack_b) {
              MEM_freeN(array_b);
            }
            break;
          default:
            BLI_assert_msg(0, "Unsupported RNA override operation on float");
            return false;
        }

        if (array_a != array_stack_a) {
          MEM_freeN(array_a);
        }
      }
      else {
        const float storage_value = prop_storage ? RNA_PROPERTY_GET_SINGLE(
                                                       float, ptr_storage, prop_storage, index) :
                                                   0.0f;

        switch (override_op) {
          case LIBOVERRIDE_OP_REPLACE:
            RNA_PROPERTY_SET_SINGLE(float,
                                    ptr_dst,
                                    prop_dst,
                                    index,
                                    RNA_PROPERTY_GET_SINGLE(float, ptr_src, prop_src, index));
            break;
          case LIBOVERRIDE_OP_ADD:
            RNA_PROPERTY_SET_SINGLE(float,
                                    ptr_dst,
                                    prop_dst,
                                    index,
                                    RNA_PROPERTY_GET_SINGLE(float, ptr_dst, prop_dst, index) +
                                        storage_value);
            break;
          case LIBOVERRIDE_OP_SUBTRACT:
            RNA_PROPERTY_SET_SINGLE(float,
                                    ptr_dst,
                                    prop_dst,
                                    index,
                                    RNA_PROPERTY_GET_SINGLE(float, ptr_dst, prop_dst, index) -
                                        storage_value);
            break;
          case LIBOVERRIDE_OP_MULTIPLY:
            RNA_PROPERTY_SET_SINGLE(float,
                                    ptr_dst,
                                    prop_dst,
                                    index,
                                    RNA_PROPERTY_GET_SINGLE(float, ptr_dst, prop_dst, index) *
                                        storage_value);
            break;
          default:
            BLI_assert_msg(0, "Unsupported RNA override operation on float");
            return false;
        }
      }
      break;
    case PROP_ENUM: {
      const int value = RNA_property_enum_get(ptr_src, prop_src);

      switch (override_op) {
        case LIBOVERRIDE_OP_REPLACE:
          RNA_property_enum_set(ptr_dst, prop_dst, value);
          break;
        /* TODO: support add/sub, for bitflags? */
        default:
          BLI_assert_msg(0, "Unsupported RNA override operation on enum");
          return false;
      }
      break;
    }
    case PROP_POINTER: {
      PointerRNA value = RNA_property_pointer_get(ptr_src, prop_src);

      switch (override_op) {
        case LIBOVERRIDE_OP_REPLACE:
          RNA_property_pointer_set(ptr_dst, prop_dst, value, nullptr);
          break;
        default:
          BLI_assert_msg(0, "Unsupported RNA override operation on pointer");
          return false;
      }
      break;
    }
    case PROP_STRING: {
      char buff[256];
      char *value = RNA_property_string_get_alloc(ptr_src, prop_src, buff, sizeof(buff), nullptr);

      switch (override_op) {
        case LIBOVERRIDE_OP_REPLACE:
          RNA_property_string_set(ptr_dst, prop_dst, value);
          break;
        default:
          BLI_assert_msg(0, "Unsupported RNA override operation on string");
          return false;
      }

      if (value != buff) {
        MEM_freeN(value);
      }
      break;
    }
    case PROP_COLLECTION: {
      /* We only support IDProperty-based collection insertion here. */
      const bool is_src_idprop = (prop_src->magic != RNA_MAGIC) ||
                                 (prop_src->flag & PROP_IDPROPERTY) != 0;
      const bool is_dst_idprop = (prop_dst->magic != RNA_MAGIC) ||
                                 (prop_dst->flag & PROP_IDPROPERTY) != 0;
      if (!(is_src_idprop && is_dst_idprop)) {
        CLOG_ERROR(&LOG_COMPARE_OVERRIDE,
                   "'%s': Override operations on RNA collections require a specific override "
                   "apply callback to be defined.",
                   rnaapply_ctx.liboverride_property->rna_path);
        return false;
      }

      switch (override_op) {
        case LIBOVERRIDE_OP_INSERT_AFTER: {
          PointerRNA item_ptr_src, item_ptr_ref, item_ptr_dst;
          int item_index_dst;
          bool is_valid = false;
          if (opop->subitem_local_name != nullptr && opop->subitem_local_name[0] != '\0') {
            /* Find from name. */
            int item_index_src, item_index_ref;
            if (RNA_property_collection_lookup_string_index(
                    ptr_src, prop_src, opop->subitem_local_name, &item_ptr_src, &item_index_src) &&
                RNA_property_collection_lookup_string_index(ptr_dst,
                                                            prop_dst,
                                                            opop->subitem_reference_name,
                                                            &item_ptr_ref,
                                                            &item_index_ref))
            {
              is_valid = true;
              item_index_dst = item_index_ref + 1;
            }
          }
          if (!is_valid && opop->subitem_local_index >= 0) {
            /* Find from index. */
            if (RNA_property_collection_lookup_int(
                    ptr_src, prop_src, opop->subitem_local_index, &item_ptr_src) &&
                RNA_property_collection_lookup_int(
                    ptr_dst, prop_dst, opop->subitem_reference_index, &item_ptr_ref))
            {
              item_index_dst = opop->subitem_reference_index + 1;
              is_valid = true;
            }
          }
          if (!is_valid) {
            /* Assume it is inserted in first position. */
            if (RNA_property_collection_lookup_int(ptr_src, prop_src, 0, &item_ptr_src)) {
              item_index_dst = 0;
              is_valid = true;
            }
          }
          if (!is_valid) {
            return false;
          }

          RNA_property_collection_add(ptr_dst, prop_dst, &item_ptr_dst);
          const int item_index_added = RNA_property_collection_length(ptr_dst, prop_dst) - 1;
          BLI_assert(item_index_added >= 0);

          /* This is the section of code that makes it specific to IDProperties (the rest could be
           * used with some regular RNA/DNA data too, if `RNA_property_collection_add` where
           * actually implemented for those).
           * Currently it is close to impossible to copy arbitrary 'real' RNA data between
           * Collection items. */
          IDProperty *item_idprop_src = static_cast<IDProperty *>(item_ptr_src.data);
          IDProperty *item_idprop_dst = static_cast<IDProperty *>(item_ptr_dst.data);
          IDP_CopyPropertyContent(item_idprop_dst, item_idprop_src);

          ret_success = RNA_property_collection_move(
              ptr_dst, prop_dst, item_index_added, item_index_dst);
          break;
        }
        default:
          BLI_assert_msg(0, "Unsupported RNA override operation on collection");
          return false;
      }
      break;
    }
    default:
      BLI_assert_unreachable();
      return false;
  }

  /* Default apply callback always call property update. */
  if (ret_success) {
    RNA_property_update_main(bmain, nullptr, ptr_dst, prop_dst);
  }

  return ret_success;
}

#  undef RNA_PROPERTY_GET_SINGLE
#  undef RNA_PROPERTY_SET_SINGLE

/** \} */

/* -------------------------------------------------------------------- */
/** \name Primitive Values
 * \{ */

/* Primitive String. */

static void rna_PrimitiveString_value_get(PointerRNA *ptr, char *result)
{
  const PrimitiveStringRNA *data = static_cast<const PrimitiveStringRNA *>(ptr->data);
  strcpy(result, data->value ? data->value : "");
}

static int rna_PrimitiveString_value_length(PointerRNA *ptr)
{
  const PrimitiveStringRNA *data = static_cast<const PrimitiveStringRNA *>(ptr->data);
  return data->value ? strlen(data->value) : 0;
}

/* Primitive Int. */

static int rna_PrimitiveInt_value_get(PointerRNA *ptr)
{
  const PrimitiveIntRNA *data = static_cast<const PrimitiveIntRNA *>(ptr->data);
  return data->value;
}

/* Primitive Float. */

static float rna_PrimitiveFloat_value_get(PointerRNA *ptr)
{
  const PrimitiveFloatRNA *data = static_cast<const PrimitiveFloatRNA *>(ptr->data);
  return data->value;
}

/* Primitive Boolean. */

static bool rna_PrimitiveBoolean_value_get(PointerRNA *ptr)
{
  const PrimitiveBooleanRNA *data = static_cast<const PrimitiveBooleanRNA *>(ptr->data);
  return data->value;
}

/** \} */

#else

static void rna_def_struct(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Struct", nullptr);
  RNA_def_struct_ui_text(srna, "Struct Definition", "RNA structure definition");
  RNA_def_struct_ui_icon(srna, ICON_RNA);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(prop, "rna_Struct_name_get", "rna_Struct_name_length", nullptr);
  RNA_def_property_ui_text(prop, "Name", "Human readable name");

  prop = RNA_def_property(srna, "identifier", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(
      prop, "rna_Struct_identifier_get", "rna_Struct_identifier_length", nullptr);
  RNA_def_property_ui_text(prop, "Identifier", "Unique name used in the code and scripting");
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(
      prop, "rna_Struct_description_get", "rna_Struct_description_length", nullptr);
  RNA_def_property_ui_text(prop, "Description", "Description of the Struct's purpose");

  prop = RNA_def_property(srna, "translation_context", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(prop,
                                "rna_Struct_translation_context_get",
                                "rna_Struct_translation_context_length",
                                nullptr);
  RNA_def_property_ui_text(
      prop, "Translation Context", "Translation context of the struct's name");

  prop = RNA_def_property(srna, "base", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "Struct");
  RNA_def_property_pointer_funcs(prop, "rna_Struct_base_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Base", "Struct definition this is derived from");

  prop = RNA_def_property(srna, "nested", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "Struct");
  RNA_def_property_pointer_funcs(prop, "rna_Struct_nested_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(
      prop,
      "Nested",
      "Struct in which this struct is always nested, and to which it logically belongs");

  prop = RNA_def_property(srna, "name_property", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "StringProperty");
  RNA_def_property_pointer_funcs(prop, "rna_Struct_name_property_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Name Property", "Property that gives the name of the struct");

  prop = RNA_def_property(srna, "properties", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "Property");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Struct_properties_begin",
                                    "rna_Struct_properties_next",
                                    "rna_iterator_listbase_end",
                                    "rna_Struct_properties_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "Properties", "Properties in the struct");

  prop = RNA_def_property(srna, "functions", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "Function");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Struct_functions_begin",
                                    "rna_Struct_functions_next",
                                    "rna_iterator_listbase_end",
                                    "rna_Struct_functions_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "Functions", "");

  prop = RNA_def_property(srna, "property_tags", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "EnumPropertyItem");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Struct_property_tags_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(
      prop, "Property Tags", "Tags that properties can use to influence behavior");
}

static void rna_def_property(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  static const EnumPropertyItem dummy_prop_tags[] = {
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "Property", nullptr);
  RNA_def_struct_ui_text(srna, "Property Definition", "RNA property definition");
  RNA_def_struct_refine_func(srna, "rna_Property_refine");
  RNA_def_struct_ui_icon(srna, ICON_RNA);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(
      prop, "rna_Property_name_get", "rna_Property_name_length", nullptr);
  RNA_def_property_ui_text(prop, "Name", "Human readable name");

  prop = RNA_def_property(srna, "identifier", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(
      prop, "rna_Property_identifier_get", "rna_Property_identifier_length", nullptr);
  RNA_def_property_ui_text(prop, "Identifier", "Unique name used in the code and scripting");
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(
      prop, "rna_Property_description_get", "rna_Property_description_length", nullptr);
  RNA_def_property_ui_text(prop, "Description", "Description of the property for tooltips");

  prop = RNA_def_property(srna, "translation_context", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(prop,
                                "rna_Property_translation_context_get",
                                "rna_Property_translation_context_length",
                                nullptr);
  RNA_def_property_ui_text(
      prop, "Translation Context", "Translation context of the property's name");

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_items(prop, rna_enum_property_type_items);
  RNA_def_property_enum_funcs(prop, "rna_Property_type_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Type", "Data type of the property");

  prop = RNA_def_property(srna, "subtype", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_items(prop, rna_enum_property_subtype_items);
  RNA_def_property_enum_funcs(prop, "rna_Property_subtype_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Subtype", "Semantic interpretation of the property");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_UNIT);

  prop = RNA_def_property(srna, "srna", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "Struct");
  RNA_def_property_pointer_funcs(prop, "rna_Property_srna_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(
      prop, "Base", "Struct definition used for properties assigned to this item");

  prop = RNA_def_property(srna, "unit", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_items(prop, rna_enum_property_unit_items);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_UNIT);
  RNA_def_property_enum_funcs(prop, "rna_Property_unit_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Unit", "Type of units for this property");

  prop = RNA_def_property(srna, "icon", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_items(prop, rna_enum_icon_items);
  RNA_def_property_enum_funcs(prop, "rna_Property_icon_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Icon", "Icon of the item");

  prop = RNA_def_property(srna, "is_readonly", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Property_readonly_get", nullptr);
  RNA_def_property_ui_text(prop, "Read Only", "Property is editable through RNA");

  prop = RNA_def_property(srna, "is_animatable", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Property_animatable_get", nullptr);
  RNA_def_property_ui_text(prop, "Animatable", "Property is animatable through RNA");

  prop = RNA_def_property(srna, "is_overridable", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Property_overridable_get", nullptr);
  RNA_def_property_ui_text(prop, "Overridable", "Property is overridable through RNA");

  prop = RNA_def_property(srna, "is_required", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Property_is_required_get", nullptr);
  RNA_def_property_ui_text(
      prop, "Required", "False when this property is an optional argument in an RNA function");

  prop = RNA_def_property(srna, "is_argument_optional", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Property_is_argument_optional_get", nullptr);
  RNA_def_property_ui_text(
      prop,
      "Optional Argument",
      "True when the property is optional in a Python function implementing an RNA function");

  prop = RNA_def_property(srna, "is_never_none", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Property_is_never_none_get", nullptr);
  RNA_def_property_ui_text(prop, "Never None", "True when this value cannot be set to None");

  prop = RNA_def_property(srna, "is_hidden", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Property_is_hidden_get", nullptr);
  RNA_def_property_ui_text(prop, "Hidden", "True when the property is hidden");

  prop = RNA_def_property(srna, "is_skip_save", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Property_is_skip_save_get", nullptr);
  RNA_def_property_ui_text(prop, "Skip Save", "True when the property uses ghost values");

  prop = RNA_def_property(srna, "is_skip_preset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Property_is_skip_preset_get", nullptr);
  RNA_def_property_ui_text(prop, "Skip Preset", "True when the property is not saved in presets");

  prop = RNA_def_property(srna, "is_output", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Property_use_output_get", nullptr);
  RNA_def_property_ui_text(
      prop, "Return", "True when this property is an output value from an RNA function");

  prop = RNA_def_property(srna, "is_registered", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Property_is_registered_get", nullptr);
  RNA_def_property_ui_text(
      prop, "Registered", "Property is registered as part of type registration");

  prop = RNA_def_property(srna, "is_registered_optional", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Property_is_registered_optional_get", nullptr);
  RNA_def_property_ui_text(prop,
                           "Registered Optionally",
                           "Property is optionally registered as part of type registration");

  prop = RNA_def_property(srna, "is_runtime", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Property_is_runtime_get", nullptr);
  RNA_def_property_ui_text(prop, "Runtime", "Property has been dynamically created at runtime");

  prop = RNA_def_property(srna, "is_enum_flag", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Property_is_enum_flag_get", nullptr);
  RNA_def_property_ui_text(prop, "Enum Flag", "True when multiple enums");

  prop = RNA_def_property(srna, "is_library_editable", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Property_is_library_editable_flag_get", nullptr);
  RNA_def_property_ui_text(
      prop, "Library Editable", "Property is editable from linked instances (changes not saved)");

  prop = RNA_def_property(srna, "is_path_output", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Property_is_path_output_flag_get", nullptr);
  RNA_def_property_ui_text(
      prop, "Path Output", "Property is a filename, filepath or directory output");

  prop = RNA_def_property(srna, "is_path_supports_blend_relative", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(
      prop, "rna_Property_is_path_supports_blend_relative_flag_get", nullptr);
  RNA_def_property_ui_text(
      prop,
      "Path Relative",
      "Property is a path which supports the \"//\" prefix, "
      "signifying the location as relative to the \".blend\" file's directory");

  prop = RNA_def_property(srna, "is_path_supports_templates", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(
      prop, "rna_Property_is_path_supports_templates_flag_get", nullptr);
  RNA_def_property_ui_text(
      prop,
      "Variable Expression Support",
      "Property is a path which supports the \"{variable_name}\" variable expression syntax, "
      "which substitutes the value of the referenced variable in place of the expression");

  prop = RNA_def_property(srna, "is_deprecated", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Property_is_deprecated_get", nullptr);
  RNA_def_property_ui_text(prop, "Deprecated", "The property is deprecated");

  prop = RNA_def_property(srna, "deprecated_note", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(
      prop, "rna_Property_deprecated_note_get", "rna_Property_deprecated_note_length", nullptr);
  RNA_def_property_ui_text(prop, "Deprecated Note", "A note regarding deprecation");

  prop = RNA_def_property(srna, "deprecated_version", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  /* Use 3 values to match `bpy.app.version`. */
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Deprecated Version", "The Blender version this was deprecated");
  RNA_def_property_int_funcs(prop, "rna_Property_deprecated_version_get", nullptr, nullptr);

  prop = RNA_def_property(srna, "deprecated_removal_version", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(
      prop, "Deprecated Removal Version", "The Blender version this is expected to be removed");
  RNA_def_property_int_funcs(
      prop, "rna_Property_deprecated_removal_version_get", nullptr, nullptr);

  prop = RNA_def_property(srna, "tags", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_REGISTER_OPTIONAL | PROP_ENUM_FLAG);
  RNA_def_property_enum_items(prop, dummy_prop_tags);
  RNA_def_property_enum_funcs(prop, "rna_Property_tags_get", nullptr, "rna_Property_tags_itemf");
  RNA_def_property_ui_text(
      prop, "Tags", "Subset of tags (defined in parent struct) that are set for this property");
}

static void rna_def_function(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Function", nullptr);
  RNA_def_struct_ui_text(srna, "Function Definition", "RNA function definition");
  RNA_def_struct_ui_icon(srna, ICON_RNA);

  prop = RNA_def_property(srna, "identifier", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(
      prop, "rna_Function_identifier_get", "rna_Function_identifier_length", nullptr);
  RNA_def_property_ui_text(prop, "Identifier", "Unique name used in the code and scripting");
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(
      prop, "rna_Function_description_get", "rna_Function_description_length", nullptr);
  RNA_def_property_ui_text(prop, "Description", "Description of the Function's purpose");

  prop = RNA_def_property(srna, "parameters", PROP_COLLECTION, PROP_NONE);
  // RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "Property");
  RNA_def_property_collection_funcs(prop,
                                    "rna_Function_parameters_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "Parameters", "Parameters for the function");

  prop = RNA_def_property(srna, "is_registered", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Function_registered_get", nullptr);
  RNA_def_property_ui_text(
      prop, "Registered", "Function is registered as callback as part of type registration");

  prop = RNA_def_property(srna, "is_registered_optional", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Function_registered_optional_get", nullptr);
  RNA_def_property_ui_text(
      prop,
      "Registered Optionally",
      "Function is optionally registered as callback part of type registration");

  prop = RNA_def_property(srna, "use_self", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Function_no_self_get", nullptr);
  RNA_def_property_ui_text(
      prop,
      "No Self",
      "Function does not pass itself as an argument (becomes a static method in Python)");

  prop = RNA_def_property(srna, "use_self_type", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Function_use_self_type_get", nullptr);
  RNA_def_property_ui_text(prop,
                           "Use Self Type",
                           "Function passes itself type as an argument (becomes a class method "
                           "in Python if use_self is false)");
}

static void rna_def_number_property(StructRNA *srna, PropertyType type)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "default", type, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Default", "Default value for this number");

  switch (type) {
    case PROP_BOOLEAN:
      RNA_def_property_boolean_funcs(prop, "rna_BoolProperty_default_get", nullptr);
      break;
    case PROP_INT:
      RNA_def_property_int_funcs(prop, "rna_IntProperty_default_get", nullptr, nullptr);
      break;
    case PROP_FLOAT:
      RNA_def_property_float_funcs(prop, "rna_FloatProperty_default_get", nullptr, nullptr);
      break;
    default:
      break;
  }

  prop = RNA_def_property(srna, "default_array", type, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  /* no fixed default length, important its not 0 though. */
  RNA_def_property_array(prop, RNA_MAX_ARRAY_DIMENSION);

  RNA_def_property_flag(prop, PROP_DYNAMIC);
  RNA_def_property_dynamic_array_funcs(
      prop, "rna_NumberProperty_default_array_get_length"); /* same for all types */

  switch (type) {
    case PROP_BOOLEAN:
      RNA_def_property_boolean_funcs(prop, "rna_BoolProperty_default_array_get", nullptr);
      break;
    case PROP_INT:
      RNA_def_property_int_funcs(prop, "rna_IntProperty_default_array_get", nullptr, nullptr);
      break;
    case PROP_FLOAT:
      RNA_def_property_float_funcs(prop, "rna_FloatProperty_default_array_get", nullptr, nullptr);
      break;
    default:
      break;
  }
  RNA_def_property_ui_text(prop, "Default Array", "Default value for this array");

  prop = RNA_def_property(srna, "array_length", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_Property_array_length_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Array Length", "Maximum length of the array, 0 means unlimited");

  prop = RNA_def_property(srna, "array_dimensions", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_array(prop, RNA_MAX_ARRAY_DIMENSION);
  RNA_def_property_int_funcs(prop, "rna_Property_array_dimensions_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Array Dimensions", "Length of each dimension of the array");

  prop = RNA_def_property(srna, "is_array", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_NumberProperty_is_array_get", nullptr);
  RNA_def_property_ui_text(prop, "Is Array", "");

  if (type == PROP_BOOLEAN) {
    return;
  }

  prop = RNA_def_property(srna, "hard_min", type, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  if (type == PROP_INT) {
    RNA_def_property_int_funcs(prop, "rna_IntProperty_hard_min_get", nullptr, nullptr);
  }
  else {
    RNA_def_property_float_funcs(prop, "rna_FloatProperty_hard_min_get", nullptr, nullptr);
  }
  RNA_def_property_ui_text(prop, "Hard Minimum", "Minimum value used by buttons");

  prop = RNA_def_property(srna, "hard_max", type, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  if (type == PROP_INT) {
    RNA_def_property_int_funcs(prop, "rna_IntProperty_hard_max_get", nullptr, nullptr);
  }
  else {
    RNA_def_property_float_funcs(prop, "rna_FloatProperty_hard_max_get", nullptr, nullptr);
  }
  RNA_def_property_ui_text(prop, "Hard Maximum", "Maximum value used by buttons");

  prop = RNA_def_property(srna, "soft_min", type, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  if (type == PROP_INT) {
    RNA_def_property_int_funcs(prop, "rna_IntProperty_soft_min_get", nullptr, nullptr);
  }
  else {
    RNA_def_property_float_funcs(prop, "rna_FloatProperty_soft_min_get", nullptr, nullptr);
  }
  RNA_def_property_ui_text(prop, "Soft Minimum", "Minimum value used by buttons");

  prop = RNA_def_property(srna, "soft_max", type, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  if (type == PROP_INT) {
    RNA_def_property_int_funcs(prop, "rna_IntProperty_soft_max_get", nullptr, nullptr);
  }
  else {
    RNA_def_property_float_funcs(prop, "rna_FloatProperty_soft_max_get", nullptr, nullptr);
  }
  RNA_def_property_ui_text(prop, "Soft Maximum", "Maximum value used by buttons");

  prop = RNA_def_property(srna, "step", type, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  if (type == PROP_INT) {
    RNA_def_property_int_funcs(prop, "rna_IntProperty_step_get", nullptr, nullptr);
  }
  else {
    RNA_def_property_float_funcs(prop, "rna_FloatProperty_step_get", nullptr, nullptr);
  }
  RNA_def_property_ui_text(
      prop, "Step", "Step size used by number buttons, for floats 1/100th of the step size");

  if (type == PROP_FLOAT) {
    prop = RNA_def_property(srna, "precision", PROP_INT, PROP_UNSIGNED);
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
    RNA_def_property_int_funcs(prop, "rna_FloatProperty_precision_get", nullptr, nullptr);
    RNA_def_property_ui_text(prop,
                             "Precision",
                             "Number of digits after the dot used by buttons. Fraction is "
                             "automatically hidden for exact integer values of fields with unit "
                             "'NONE' or 'TIME' (frame count) and step divisible by 100.");
  }
}

static void rna_def_string_property(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "default", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(
      prop, "rna_StringProperty_default_get", "rna_StringProperty_default_length", nullptr);
  RNA_def_property_ui_text(prop, "Default", "String default value");

  prop = RNA_def_property(srna, "length_max", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_StringProperty_max_length_get", nullptr, nullptr);
  RNA_def_property_ui_text(
      prop, "Maximum Length", "Maximum length of the string, 0 means unlimited");
}

static void rna_def_enum_property(BlenderRNA *brna, StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "default", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_items(prop, rna_enum_dummy_DEFAULT_items);
  RNA_def_property_enum_funcs(
      prop, "rna_EnumProperty_default_get", nullptr, "rna_EnumProperty_default_itemf");
  RNA_def_property_ui_text(prop, "Default", "Default value for this enum");

  /* same 'default' but uses 'PROP_ENUM_FLAG' */
  prop = RNA_def_property(srna, "default_flag", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_ENUM_FLAG);
  RNA_def_property_enum_items(prop, rna_enum_dummy_DEFAULT_items);
  RNA_def_property_enum_funcs(
      prop, "rna_EnumProperty_default_get", nullptr, "rna_EnumProperty_default_itemf");
  RNA_def_property_ui_text(prop, "Default", "Default value for this enum");

  prop = RNA_def_property(srna, "enum_items", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "EnumPropertyItem");
  RNA_def_property_collection_funcs(prop,
                                    "rna_EnumProperty_items_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "Items", "Possible values for the property");

  prop = RNA_def_property(srna, "enum_items_static", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "EnumPropertyItem");
  RNA_def_property_collection_funcs(prop,
                                    "rna_EnumProperty_items_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(
      prop,
      "Static Items",
      "Possible values for the property (never calls optional dynamic generation of those)");

  /* Expose a UI version of `enum_items_static` to allow separator & title access,
   * needed for the tool-system to access separators from brush enums. */
  prop = RNA_def_property(srna, "enum_items_static_ui", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "EnumPropertyItem");
  RNA_def_property_collection_funcs(prop,
                                    "rna_EnumProperty_items_ui_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(
      prop,
      "Static Items with UI Elements",
      "Possible values for the property (never calls optional dynamic generation of those). "
      "Includes UI elements (separators and section headings).");

  srna = RNA_def_struct(brna, "EnumPropertyItem", nullptr);
  RNA_def_struct_ui_text(
      srna, "Enum Item Definition", "Definition of a choice in an RNA enum property");
  RNA_def_struct_ui_icon(srna, ICON_RNA);

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(
      prop, "rna_EnumPropertyItem_name_get", "rna_EnumPropertyItem_name_length", nullptr);
  RNA_def_property_ui_text(prop, "Name", "Human readable name");

  prop = RNA_def_property(srna, "description", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(prop,
                                "rna_EnumPropertyItem_description_get",
                                "rna_EnumPropertyItem_description_length",
                                nullptr);
  RNA_def_property_ui_text(prop, "Description", "Description of the item's purpose");

  prop = RNA_def_property(srna, "identifier", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(prop,
                                "rna_EnumPropertyItem_identifier_get",
                                "rna_EnumPropertyItem_identifier_length",
                                nullptr);
  RNA_def_property_ui_text(prop, "Identifier", "Unique name used in the code and scripting");
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "value", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_EnumPropertyItem_value_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Value", "Value of the item");

  prop = RNA_def_property(srna, "icon", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_items(prop, rna_enum_icon_items);
  RNA_def_property_enum_funcs(prop, "rna_EnumPropertyItem_icon_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Icon", "Icon of the item");
}

static void rna_def_pointer_property(StructRNA *srna, PropertyType type)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "fixed_type", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "Struct");
  if (type == PROP_POINTER) {
    RNA_def_property_pointer_funcs(
        prop, "rna_PointerProperty_fixed_type_get", nullptr, nullptr, nullptr);
  }
  else {
    RNA_def_property_pointer_funcs(
        prop, "rna_CollectionProperty_fixed_type_get", nullptr, nullptr, nullptr);
  }
  RNA_def_property_ui_text(prop, "Pointer Type", "Fixed pointer type, empty if variable type");
}

static void rna_def_rna_primitive(BlenderRNA *brna)
{
  /* Primitive Values, use when passing #PointerRNA is used for primitive types.
   * For the rare cases we want to pass a value as RNA which wraps a primitive data. */

  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "PrimitiveString", nullptr);
  RNA_def_struct_ui_text(srna, "String Value", "RNA wrapped string");
  prop = RNA_def_property(srna, "value", PROP_STRING, PROP_BYTESTRING);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(
      prop, "rna_PrimitiveString_value_get", "rna_PrimitiveString_value_length", nullptr);

  srna = RNA_def_struct(brna, "PrimitiveInt", nullptr);
  RNA_def_struct_ui_text(srna, "Primitive Int", "RNA wrapped int");
  prop = RNA_def_property(srna, "value", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_PrimitiveInt_value_get", nullptr, nullptr);

  srna = RNA_def_struct(brna, "PrimitiveFloat", nullptr);
  RNA_def_struct_ui_text(srna, "Primitive Float", "RNA wrapped float");
  prop = RNA_def_property(srna, "value", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_PrimitiveFloat_value_get", nullptr, nullptr);

  srna = RNA_def_struct(brna, "PrimitiveBoolean", nullptr);
  RNA_def_struct_ui_text(srna, "Primitive Boolean", "RNA wrapped boolean");
  prop = RNA_def_property(srna, "value", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_PrimitiveBoolean_value_get", nullptr);
}

void RNA_def_rna(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* Struct */
  rna_def_struct(brna);

  /* Property */
  rna_def_property(brna);

  /* BoolProperty */
  srna = RNA_def_struct(brna, "BoolProperty", "Property");
  RNA_def_struct_ui_text(srna, "Boolean Definition", "RNA boolean property definition");
  rna_def_number_property(srna, PROP_BOOLEAN);

  /* IntProperty */
  srna = RNA_def_struct(brna, "IntProperty", "Property");
  RNA_def_struct_ui_text(srna, "Int Definition", "RNA integer number property definition");
  rna_def_number_property(srna, PROP_INT);

  /* FloatProperty */
  srna = RNA_def_struct(brna, "FloatProperty", "Property");
  RNA_def_struct_ui_text(srna,
                         "Float Definition",
                         "RNA floating-point number (single precision) property definition");
  rna_def_number_property(srna, PROP_FLOAT);

  /* StringProperty */
  srna = RNA_def_struct(brna, "StringProperty", "Property");
  RNA_def_struct_ui_text(srna, "String Definition", "RNA text string property definition");
  rna_def_string_property(srna);

  /* EnumProperty */
  srna = RNA_def_struct(brna, "EnumProperty", "Property");
  RNA_def_struct_ui_text(
      srna,
      "Enum Definition",
      "RNA enumeration property definition, to choose from a number of predefined options");
  rna_def_enum_property(brna, srna);

  /* PointerProperty */
  srna = RNA_def_struct(brna, "PointerProperty", "Property");
  RNA_def_struct_ui_text(
      srna, "Pointer Definition", "RNA pointer property to point to another RNA struct");
  rna_def_pointer_property(srna, PROP_POINTER);

  /* CollectionProperty */
  srna = RNA_def_struct(brna, "CollectionProperty", "Property");
  RNA_def_struct_ui_text(srna,
                         "Collection Definition",
                         "RNA collection property to define lists, arrays and mappings");
  rna_def_pointer_property(srna, PROP_COLLECTION);

  /* Function */
  rna_def_function(brna);

  /* Blender RNA */
  srna = RNA_def_struct(brna, "BlenderRNA", nullptr);
  RNA_def_struct_ui_text(srna, "Blender RNA", "Blender RNA structure definitions");
  RNA_def_struct_ui_icon(srna, ICON_RNA);

  prop = RNA_def_property(srna, "structs", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_struct_type(prop, "Struct");
  RNA_def_property_collection_funcs(prop,
                                    "rna_BlenderRNA_structs_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
/* included for speed, can be removed */
#  if 0
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
#  else
                                    "rna_BlenderRNA_structs_length",
                                    "rna_BlenderRNA_structs_lookup_int",
                                    "rna_BlenderRNA_structs_lookup_string",
                                    nullptr);
#  endif

  RNA_def_property_ui_text(prop, "Structs", "");

  rna_def_rna_primitive(brna);
}

#endif

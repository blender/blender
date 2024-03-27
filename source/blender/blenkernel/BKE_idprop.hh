/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include <memory>

#include "BLI_compiler_attrs.h"
#include "BLI_function_ref.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_sys_types.h"

struct BlendDataReader;
struct BlendWriter;
struct ID;
struct IDProperty;
struct IDPropertyUIData;
struct IDPropertyUIDataEnumItem;
namespace blender::io::serialize {
class ArrayValue;
class Value;
}  // namespace blender::io::serialize

union IDPropertyTemplate {
  int i;
  float f;
  double d;
  struct {
    const char *str;
    /** String length (including the null byte): `strlen(str) + 1`. */
    int len;
    /** #eIDPropertySubType */
    char subtype;
  } string;
  ID *id;
  struct {
    int len;
    /** #eIDPropertyType */
    char type;
  } array;
};

/* ----------- Property Array Type ---------- */

/**
 * \note as a start to move away from the stupid #IDP_New function,
 * this type has its own allocation function.
 */
IDProperty *IDP_NewIDPArray(const char *name) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
IDProperty *IDP_CopyIDPArray(const IDProperty *array, int flag) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();

/**
 * Shallow copies item.
 */
void IDP_SetIndexArray(IDProperty *prop, int index, IDProperty *item) ATTR_NONNULL();
IDProperty *IDP_GetIndexArray(IDProperty *prop, int index) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
void IDP_AppendArray(IDProperty *prop, IDProperty *item);
void IDP_ResizeIDPArray(IDProperty *prop, int len);

/* ----------- Numeric Array Type ----------- */

/**
 * This function works for strings too!
 */
void IDP_ResizeArray(IDProperty *prop, int newlen);
void IDP_FreeArray(IDProperty *prop);

/* ---------- String Type ------------ */
/**
 * \param st: The string to assign.
 * Doesn't need to be null terminated when clamped by `maxncpy`.
 * \param name: The property name.
 * \param maxncpy: The maximum size of the string (including the `\0` terminator).
 * When zero, this is the equivalent of passing in `strlen(st) + 1`
 * \return The new string property.
 */
IDProperty *IDP_NewStringMaxSize(const char *st,
                                 size_t st_maxncpy,
                                 const char *name) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(3);
IDProperty *IDP_NewString(const char *st, const char *name) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(2);
/**
 * \param st: The string to assign.
 * Doesn't need to be null terminated when clamped by `maxncpy`.
 * \param maxncpy: The maximum size of the string (including the `\0` terminator).
 * When zero, this is the equivalent of passing in `strlen(st) + 1`
 */
void IDP_AssignStringMaxSize(IDProperty *prop, const char *st, size_t st_maxncpy) ATTR_NONNULL();
void IDP_AssignString(IDProperty *prop, const char *st) ATTR_NONNULL();
void IDP_FreeString(IDProperty *prop) ATTR_NONNULL();

/*-------- Enum Type -------*/

const IDPropertyUIDataEnumItem *IDP_EnumItemFind(const IDProperty *prop);

bool IDP_EnumItemsValidate(const IDPropertyUIDataEnumItem *items,
                           int items_num,
                           void (*error_fn)(const char *));

/*-------- ID Type -------*/

using IDPWalkFunc = void (*)(void *user_data, IDProperty *idp);

void IDP_AssignID(IDProperty *prop, ID *id, int flag);

/*-------- Group Functions -------*/

/**
 * Sync values from one group to another when values name and types match,
 * copy the values, else ignore.
 *
 * \note Was used for syncing proxies.
 */
void IDP_SyncGroupValues(IDProperty *dest, const IDProperty *src) ATTR_NONNULL();
void IDP_SyncGroupTypes(IDProperty *dest, const IDProperty *src, bool do_arraylen) ATTR_NONNULL();
/**
 * Replaces all properties with the same name in a destination group from a source group.
 */
void IDP_ReplaceGroupInGroup(IDProperty *dest, const IDProperty *src) ATTR_NONNULL();
void IDP_ReplaceInGroup(IDProperty *group, IDProperty *prop) ATTR_NONNULL();
/**
 * Checks if a property with the same name as prop exists, and if so replaces it.
 * Use this to preserve order!
 */
void IDP_ReplaceInGroup_ex(IDProperty *group, IDProperty *prop, IDProperty *prop_exist);
/**
 * If a property is missing in \a dest, add it.
 * Do it recursively.
 */
void IDP_MergeGroup(IDProperty *dest, const IDProperty *src, bool do_overwrite) ATTR_NONNULL();
/**
 * If a property is missing in \a dest, add it.
 * Do it recursively.
 */
void IDP_MergeGroup_ex(IDProperty *dest, const IDProperty *src, bool do_overwrite, int flag)
    ATTR_NONNULL();
/**
 * This function has a sanity check to make sure ID properties with the same name don't
 * get added to the group.
 *
 * The sanity check just means the property is not added to the group if another property
 * exists with the same name; the client code using ID properties then needs to detect this
 * (the function that adds new properties to groups, #IDP_AddToGroup,
 * returns false if a property can't be added to the group, and true if it can)
 * and free the property.
 */
bool IDP_AddToGroup(IDProperty *group, IDProperty *prop) ATTR_NONNULL();
/**
 * This is the same as IDP_AddToGroup, only you pass an item
 * in the group list to be inserted after.
 */
bool IDP_InsertToGroup(IDProperty *group, IDProperty *previous, IDProperty *pnew)
    ATTR_NONNULL(1 /*group*/, 3 /*pnew*/);
/**
 * \note this does not free the property!
 *
 * To free the property, you have to do:
 * #IDP_FreeProperty(prop);
 */
void IDP_RemoveFromGroup(IDProperty *group, IDProperty *prop) ATTR_NONNULL();
/**
 * Removes the property from the group and frees it.
 */
void IDP_FreeFromGroup(IDProperty *group, IDProperty *prop) ATTR_NONNULL();

IDProperty *IDP_GetPropertyFromGroup(const IDProperty *prop,
                                     const char *name) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Same as above but ensure type match.
 */
IDProperty *IDP_GetPropertyTypeFromGroup(const IDProperty *prop,
                                         const char *name,
                                         char type) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/*-------- Main Functions --------*/
/**
 * Get the Group property that contains the id properties for ID `id`.
 */
IDProperty *IDP_GetProperties(ID *id) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * Ensure the Group property that contains the id properties for ID `id` exists & return it.
 */
IDProperty *IDP_EnsureProperties(ID *id) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
IDProperty *IDP_CopyProperty(const IDProperty *prop) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
IDProperty *IDP_CopyProperty_ex(const IDProperty *prop, int flag) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
/**
 * Copy content from source #IDProperty into destination one,
 * freeing destination property's content first.
 */
void IDP_CopyPropertyContent(IDProperty *dst, const IDProperty *src) ATTR_NONNULL();

/**
 * \param is_strict: When false treat missing items as a match.
 */
bool IDP_EqualsProperties_ex(const IDProperty *prop1,
                             const IDProperty *prop2,
                             bool is_strict) ATTR_WARN_UNUSED_RESULT;

bool IDP_EqualsProperties(const IDProperty *prop1,
                          const IDProperty *prop2) ATTR_WARN_UNUSED_RESULT;

/**
 * Allocate a new ID.
 *
 * This function takes three arguments: the ID property type, a union which defines
 * its initial value, and a name.
 *
 * The union is simple to use; see the top of BKE_idprop.h for its definition.
 * An example of using this function:
 *
 * \code{.c}
 * IDPropertyTemplate val;
 * IDProperty *group, *idgroup, *color;
 * group = IDP_New(IDP_GROUP, val, "group1"); // groups don't need a template.
 *
 * val.array.len = 4
 * val.array.type = IDP_FLOAT;
 * color = IDP_New(IDP_ARRAY, val, "color1");
 *
 * idgroup = IDP_EnsureProperties(some_id);
 * IDP_AddToGroup(idgroup, color);
 * IDP_AddToGroup(idgroup, group);
 * \endcode
 *
 * Note that you MUST either attach the id property to an id property group with
 * IDP_AddToGroup or MEM_freeN the property, doing anything else might result in
 * a memory leak.
 */
IDProperty *IDP_New(char type,
                    const IDPropertyTemplate *val,
                    const char *name) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/**
 * \note This will free allocated data, all child properties of arrays and groups, and unlink IDs!
 * But it does not free the actual #IDProperty struct itself.
 */
void IDP_FreePropertyContent_ex(IDProperty *prop, bool do_id_user);
void IDP_FreePropertyContent(IDProperty *prop);
void IDP_FreeProperty_ex(IDProperty *prop, bool do_id_user);
void IDP_FreeProperty(IDProperty *prop);

void IDP_ClearProperty(IDProperty *prop);

void IDP_Reset(IDProperty *prop, const IDProperty *reference);

#define IDP_Int(prop) ((prop)->data.val)
#define IDP_Bool(prop) ((prop)->data.val)
#define IDP_Array(prop) ((prop)->data.pointer)
/* C11 const correctness for casts */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#  define IDP_Float(prop) \
    _Generic((prop), \
        IDProperty *: (*(float *)&(prop)->data.val), \
        const IDProperty *: (*(const float *)&(prop)->data.val))
#  define IDP_Double(prop) \
    _Generic((prop), \
        IDProperty *: (*(double *)&(prop)->data.val), \
        const IDProperty *: (*(const double *)&(prop)->data.val))
#  define IDP_String(prop) \
    _Generic((prop), \
        IDProperty *: ((char *)(prop)->data.pointer), \
        const IDProperty *: ((const char *)(prop)->data.pointer))
#  define IDP_IDPArray(prop) \
    _Generic((prop), \
        IDProperty *: ((IDProperty *)(prop)->data.pointer), \
        const IDProperty *: ((const IDProperty *)(prop)->data.pointer))
#  define IDP_Id(prop) \
    _Generic((prop), \
        IDProperty *: ((ID *)(prop)->data.pointer), \
        const IDProperty *: ((const ID *)(prop)->data.pointer))
#else
#  define IDP_Float(prop) (*(float *)&(prop)->data.val)
#  define IDP_Double(prop) (*(double *)&(prop)->data.val)
#  define IDP_String(prop) ((char *)(prop)->data.pointer)
#  define IDP_IDPArray(prop) ((IDProperty *)(prop)->data.pointer)
#  define IDP_Id(prop) ((ID *)(prop)->data.pointer)
#endif

/**
 * Return an int from an #IDProperty with a compatible type. This should be avoided, but
 * it's sometimes necessary, for example when legacy files have incorrect property types.
 */
int IDP_coerce_to_int_or_zero(const IDProperty *prop);
/**
 * Return a float from an #IDProperty with a compatible type. This should be avoided, but
 * it's sometimes necessary, for example when legacy files have incorrect property types.
 */
float IDP_coerce_to_float_or_zero(const IDProperty *prop);
/**
 * Return a double from an #IDProperty with a compatible type. This should be avoided, but
 * it's sometimes necessary, for example when legacy files have incorrect property types.
 */
double IDP_coerce_to_double_or_zero(const IDProperty *prop);

/**
 * Loop through all ID properties in hierarchy of given \a id_property_root included.
 *
 * \note Container types (groups and arrays) are processed after applying the callback on them.
 *
 * \param type_filter: If not 0, only apply callback on properties of matching types, see
 * IDP_TYPE_FILTER_ enum in DNA_ID.h.
 */
void IDP_foreach_property(IDProperty *id_property_root,
                          int type_filter,
                          blender::FunctionRef<void(IDProperty *id_property)> callback);

/* Format IDProperty as strings */
char *IDP_reprN(const IDProperty *prop, uint *r_len);
void IDP_repr_fn(const IDProperty *prop,
                 void (*str_append_fn)(void *user_data, const char *str, uint str_len),
                 void *user_data);
void IDP_print(const IDProperty *prop);

void IDP_BlendWrite(BlendWriter *writer, const IDProperty *prop);
void IDP_BlendReadData_impl(BlendDataReader *reader,
                            IDProperty **prop,
                            const char *caller_func_id);
#define IDP_BlendDataRead(reader, prop) IDP_BlendReadData_impl(reader, prop, __func__)

enum eIDPropertyUIDataType {
  /** Other properties types that don't support RNA UI data. */
  IDP_UI_DATA_TYPE_UNSUPPORTED = -1,
  /** IDP_INT or IDP_ARRAY with subtype IDP_INT. */
  IDP_UI_DATA_TYPE_INT = 0,
  /** IDP_FLOAT and IDP_DOUBLE or IDP_ARRAY properties with a float or double sub-types. */
  IDP_UI_DATA_TYPE_FLOAT = 1,
  /** IDP_STRING properties. */
  IDP_UI_DATA_TYPE_STRING = 2,
  /** IDP_ID. */
  IDP_UI_DATA_TYPE_ID = 3,
  /** IDP_BOOLEAN or IDP_ARRAY with subtype IDP_BOOLEAN. */
  IDP_UI_DATA_TYPE_BOOLEAN = 4,
};

bool IDP_ui_data_supported(const IDProperty *prop);
eIDPropertyUIDataType IDP_ui_data_type(const IDProperty *prop);
void IDP_ui_data_free(IDProperty *prop);
/**
 * Free allocated pointers in the UI data that isn't shared with the UI data in the `other`
 * argument. Useful for returning early on failure when updating UI data in place, or when
 * replacing a subset of the UI data's allocated pointers.
 */
void IDP_ui_data_free_unique_contents(IDPropertyUIData *ui_data,
                                      eIDPropertyUIDataType type,
                                      const IDPropertyUIData *other);
IDPropertyUIData *IDP_ui_data_ensure(IDProperty *prop);
IDPropertyUIData *IDP_ui_data_copy(const IDProperty *prop);
/**
 * Convert UI data like default arrays from the old type to the new type as possible.
 * Takes ownership of the input data; it can return it directly if the types match.
 */
IDPropertyUIData *IDP_TryConvertUIData(IDPropertyUIData *src,
                                       eIDPropertyUIDataType src_type,
                                       eIDPropertyUIDataType dst_type);

namespace blender::bke::idprop {

/**
 * \brief Convert the given `properties` to `Value` objects for serialization.
 *
 * `IDP_ID` and `IDP_IDPARRAY` are not supported and will be ignored.
 *
 * UI data such as max/min will not be serialized.
 */
std::unique_ptr<blender::io::serialize::ArrayValue> convert_to_serialize_values(
    const IDProperty *properties);

/**
 * \brief Convert the given `value` to an `IDProperty`.
 */
IDProperty *convert_from_serialize_value(const blender::io::serialize::Value &value);

class IDPropertyDeleter {
 public:
  void operator()(IDProperty *id_prop)
  {
    IDP_FreeProperty(id_prop);
  }
};

/** \brief Allocate a new IDProperty of type IDP_BOOLEAN, set its name and value. */
std::unique_ptr<IDProperty, IDPropertyDeleter> create_bool(StringRefNull prop_name, bool value);

/** \brief Allocate a new IDProperty of type IDP_INT, set its name and value. */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRefNull prop_name, int32_t value);

/** \brief Allocate a new IDProperty of type IDP_FLOAT, set its name and value. */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRefNull prop_name, float value);

/** \brief Allocate a new IDProperty of type IDP_DOUBLE, set its name and value. */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRefNull prop_name, double value);

/** \brief Allocate a new IDProperty of type IDP_STRING, set its name and value. */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRefNull prop_name,
                                                      const StringRefNull value);

/** \brief Allocate a new IDProperty of type IDP_ID, set its name and value. */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRefNull prop_name, ID *value);

/**
 * \brief Allocate a new IDProperty of type IDP_ARRAY and sub-type IDP_INT.
 *
 * \param values: The values will be copied into the IDProperty.
 */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRefNull prop_name,
                                                      Span<int32_t> values);

/**
 * \brief Allocate a new IDProperty of type IDP_ARRAY and sub-type IDP_FLOAT.
 *
 * \param values: The values will be copied into the IDProperty.
 */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRefNull prop_name, Span<float> values);

/**
 * \brief Allocate a new IDProperty of type IDP_ARRAY and sub-type IDP_DOUBLE.
 *
 * \param values: The values will be copied into the IDProperty.
 */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRefNull prop_name,
                                                      Span<double> values);

/**
 * \brief Allocate a new IDProperty of type IDP_GROUP.
 *
 * \param prop_name: The name of the newly created property.
 */

std::unique_ptr<IDProperty, IDPropertyDeleter> create_group(StringRefNull prop_name);

}  // namespace blender::bke::idprop

/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include <memory>

#include "DNA_ID.h"
#include "DNA_ID_enums.h"

#include "BLI_compiler_attrs.h"
#include "BLI_function_ref.hh"
#include "BLI_span.hh"
#include "BLI_string_ref.hh"
#include "BLI_sys_types.h"
#include "BLI_vector_set.hh"

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
IDProperty *IDP_NewIDPArray(blender::StringRef name) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * \param flag: the ID creation/copying flags (`LIB_ID_CREATE_...`), same as passed to
 * #BKE_id_copy_ex.
 */
IDProperty *IDP_CopyIDPArray(const IDProperty *array, int flag) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();

/**
 * Shallow copies item.
 */
void IDP_SetIndexArray(IDProperty *prop, int index, IDProperty *item) ATTR_NONNULL();
IDProperty *IDP_GetIndexArray(IDProperty *prop, int index) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
void IDP_AppendArray(IDProperty *prop, IDProperty *item);
void IDP_ResizeIDPArray(IDProperty *prop, int newlen);

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
                                 blender::StringRef name,
                                 eIDPropertyFlag flags = {}) ATTR_WARN_UNUSED_RESULT;
IDProperty *IDP_NewString(const char *st,
                          blender::StringRef name,
                          eIDPropertyFlag flags = {}) ATTR_WARN_UNUSED_RESULT;
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

/**
 * \param flag: the ID creation/copying flags (`LIB_ID_CREATE_...`), same as passed to
 * #BKE_id_copy_ex.
 */
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
 *
 * \param flag: the ID creation/copying flags (`LIB_ID_CREATE_...`), same as passed to
 * #BKE_id_copy_ex.
 */
void IDP_ReplaceInGroup_ex(IDProperty *group, IDProperty *prop, IDProperty *prop_exist, int flag);
/**
 * If a property is missing in \a dest, add it.
 * Do it recursively.
 */
void IDP_MergeGroup(IDProperty *dest, const IDProperty *src, bool do_overwrite) ATTR_NONNULL();
/**
 * If a property is missing in \a dest, add it.
 * Do it recursively.
 *
 * \param flag: the ID creation/copying flags (`LIB_ID_CREATE_...`), same as passed to
 * #BKE_id_copy_ex.
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
                                     blender::StringRef name) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
/** Same as above, but allows the property to be null, in which case null is returned. */
IDProperty *IDP_GetPropertyFromGroup_null(const IDProperty *prop,
                                          blender::StringRef name) ATTR_WARN_UNUSED_RESULT;

/**
 * Same as #IDP_GetPropertyFromGroup but ensure the `type` matches.
 */
IDProperty *IDP_GetPropertyTypeFromGroup(const IDProperty *prop,
                                         blender::StringRef name,
                                         char type) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/*-------- Main Functions --------*/
/**
 * Get the Group property that contains the user-defined id properties for ID `id`.
 */
IDProperty *IDP_GetProperties(ID *id) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * Ensure the Group property that contains the user-defined id properties for ID `id` exists &
 * return it.
 */
IDProperty *IDP_EnsureProperties(ID *id) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * Get the Group property that contains the system-defined id properties for ID `id`.
 */
IDProperty *IDP_ID_system_properties_get(ID *id) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);
/**
 * Ensure the Group property that contains the system-defined id properties for ID `id` exists &
 * return it.
 */
IDProperty *IDP_ID_system_properties_ensure(ID *id) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(1);

IDProperty *IDP_CopyProperty(const IDProperty *prop) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * \param flag: the ID creation/copying flags (`LIB_ID_CREATE_...`), same as passed to
 * #BKE_id_copy_ex.
 */
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
 * Allocate a new IDProperty.
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
                    blender::StringRef name,
                    eIDPropertyFlag flags = {}) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

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

#ifndef NDEBUG
const IDProperty *_IDP_assert_type(const IDProperty *prop, char ty);
const IDProperty *_IDP_assert_type_and_subtype(const IDProperty *prop, char ty, char sub_ty);
const IDProperty *_IDP_assert_type_mask(const IDProperty *prop, int ty_mask);

#else
#  define _IDP_assert_type(prop, ty) (prop)
#  define _IDP_assert_type_and_subtype(prop, ty, sub_ty) (prop)
#  define _IDP_assert_type_mask(prop, ty_mask) (prop)
#endif

#define IDP_int_get(prop) (_IDP_assert_type(prop, IDP_INT)->data.val)
#define IDP_int_set(prop, value) \
  { \
    IDProperty *prop_ = (prop); \
    BLI_assert(prop_->type == IDP_INT); \
    prop_->data.val = value; \
  } \
  ((void)0)

#define IDP_bool_get(prop) ((_IDP_assert_type(prop, IDP_BOOLEAN))->data.val)
#define IDP_bool_set(prop, value) \
  { \
    IDProperty *prop_ = (prop); \
    BLI_assert(prop_->type == IDP_BOOLEAN); \
    prop_->data.val = value; \
  } \
  ((void)0)

#define IDP_int_or_bool_get(prop) \
  (_IDP_assert_type_mask(prop, (1 << IDP_INT) | (1 << IDP_BOOLEAN))->data.val)
#define IDP_int_or_bool_set(prop, value) \
  { \
    IDProperty *prop_ = (prop); \
    BLI_assert(ELEM(prop_->type, IDP_INT, IDP_BOOLEAN)); \
    prop_->data.val = value; \
  } \
  ((void)0)

#define IDP_float_get(prop) (*(const float *)&(_IDP_assert_type(prop, IDP_FLOAT)->data.val))
#define IDP_float_set(prop, value) \
  { \
    IDProperty *prop_ = (prop); \
    BLI_assert(prop_->type == IDP_FLOAT); \
    (*(float *)&(prop_)->data.val) = value; \
  } \
  ((void)0)

#define IDP_double_get(prop) (*(const double *)&(_IDP_assert_type(prop, IDP_DOUBLE)->data.val))
#define IDP_double_set(prop, value) \
  { \
    IDProperty *prop_ = (prop); \
    BLI_assert(prop_->type == IDP_DOUBLE); \
    (*(double *)&(prop_)->data.val) = value; \
  } \
  ((void)0)

/**
 * Use when the type of the array is not known.
 *
 * Avoid using this where possible.
 */
#define IDP_array_voidp_get(prop) (_IDP_assert_type(prop, IDP_ARRAY)->data.pointer)

#define IDP_array_int_get(prop) \
  static_cast<int *>(_IDP_assert_type_and_subtype(prop, IDP_ARRAY, IDP_INT)->data.pointer)
#define IDP_array_bool_get(prop) \
  static_cast<int8_t *>(_IDP_assert_type_and_subtype(prop, IDP_ARRAY, IDP_BOOLEAN)->data.pointer)
#define IDP_array_float_get(prop) \
  static_cast<float *>(_IDP_assert_type_and_subtype(prop, IDP_ARRAY, IDP_FLOAT)->data.pointer)
#define IDP_array_double_get(prop) \
  static_cast<double *>(_IDP_assert_type_and_subtype(prop, IDP_ARRAY, IDP_DOUBLE)->data.pointer)
#define IDP_property_array_get(prop) \
  static_cast<IDProperty *>(_IDP_assert_type(prop, IDP_IDPARRAY)->data.pointer)

#define IDP_string_get(prop) ((char *)_IDP_assert_type(prop, IDP_STRING)->data.pointer)
/* No `IDP_string_set` needed. */
#define IDP_ID_get(prop) ((void)0, ((ID *)_IDP_assert_type(prop, IDP_ID)->data.pointer))
/* No `IDP_ID_set` needed. */

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

const char *IDP_type_str(eIDPropertyType type, short sub_type);
const char *IDP_type_str(const IDProperty *prop);

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

struct IDPropertyGroupChildrenSet {
  struct IDPropNameGetter {
    StringRef operator()(const IDProperty *value) const
    {
      return StringRef(value->name);
    }
  };

  CustomIDVectorSet<IDProperty *, IDPropNameGetter, 8> children;
};

/** \brief Allocate a new IDProperty of type IDP_BOOLEAN, set its name and value. */
std::unique_ptr<IDProperty, IDPropertyDeleter> create_bool(StringRef prop_name,
                                                           bool value,
                                                           eIDPropertyFlag flags = {});

/** \brief Allocate a new IDProperty of type IDP_INT, set its name and value. */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRef prop_name,
                                                      int32_t value,
                                                      eIDPropertyFlag flags = {});

/** \brief Allocate a new IDProperty of type IDP_FLOAT, set its name and value. */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRef prop_name,
                                                      float value,
                                                      eIDPropertyFlag flags = {});

/** \brief Allocate a new IDProperty of type IDP_DOUBLE, set its name and value. */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRef prop_name,
                                                      double value,
                                                      eIDPropertyFlag flags = {});

/** \brief Allocate a new IDProperty of type IDP_STRING, set its name and value. */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRef prop_name,
                                                      StringRefNull value,
                                                      eIDPropertyFlag flags = {});

/** \brief Allocate a new IDProperty of type IDP_ID, set its name and value. */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRef prop_name,
                                                      ID *value,
                                                      eIDPropertyFlag flags = {});

/**
 * \brief Allocate a new IDProperty of type IDP_ARRAY and sub-type IDP_INT.
 *
 * \param values: The values will be copied into the IDProperty.
 */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRef prop_name,
                                                      Span<int32_t> values,
                                                      eIDPropertyFlag flags = {});

/**
 * \brief Allocate a new IDProperty of type IDP_ARRAY and sub-type IDP_FLOAT.
 *
 * \param values: The values will be copied into the IDProperty.
 */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRef prop_name,
                                                      Span<float> values,
                                                      eIDPropertyFlag flags = {});

/**
 * \brief Allocate a new IDProperty of type IDP_ARRAY and sub-type IDP_DOUBLE.
 *
 * \param values: The values will be copied into the IDProperty.
 */
std::unique_ptr<IDProperty, IDPropertyDeleter> create(StringRef prop_name,
                                                      Span<double> values,
                                                      eIDPropertyFlag flags = {});

/**
 * \brief Allocate a new IDProperty of type IDP_GROUP.
 *
 * \param prop_name: The name of the newly created property.
 */

std::unique_ptr<IDProperty, IDPropertyDeleter> create_group(StringRef prop_name,
                                                            eIDPropertyFlag flags = {});

}  // namespace blender::bke::idprop

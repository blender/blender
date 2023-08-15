/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 */

#include "BLI_compiler_attrs.h"
#include "BLI_sys_types.h"

#ifdef __cplusplus
extern "C" {
#endif

struct BlendDataReader;
struct BlendExpander;
struct BlendLibReader;
struct BlendWriter;
struct ID;
struct IDProperty;
struct IDPropertyUIData;
struct Library;

typedef union IDPropertyTemplate {
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
  struct ID *id;
  struct {
    int len;
    /** #eIDPropertyType */
    char type;
  } array;
} IDPropertyTemplate;

/* ----------- Property Array Type ---------- */

/**
 * \note as a start to move away from the stupid #IDP_New function,
 * this type has its own allocation function.
 */
struct IDProperty *IDP_NewIDPArray(const char *name) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
struct IDProperty *IDP_CopyIDPArray(const struct IDProperty *array,
                                    int flag) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/**
 * Shallow copies item.
 */
void IDP_SetIndexArray(struct IDProperty *prop, int index, struct IDProperty *item) ATTR_NONNULL();
struct IDProperty *IDP_GetIndexArray(struct IDProperty *prop, int index) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
void IDP_AppendArray(struct IDProperty *prop, struct IDProperty *item);
void IDP_ResizeIDPArray(struct IDProperty *prop, int len);

/* ----------- Numeric Array Type ----------- */

/**
 * This function works for strings too!
 */
void IDP_ResizeArray(struct IDProperty *prop, int newlen);
void IDP_FreeArray(struct IDProperty *prop);

/* ---------- String Type ------------ */
/**
 * \param st: The string to assign.
 * \param name: The property name.
 * \param maxncpy: The maximum size of the string (including the `\0` terminator).
 * \return The new string property.
 */
struct IDProperty *IDP_NewStringMaxSize(const char *st,
                                        const char *name,
                                        int maxncpy) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL(2);
struct IDProperty *IDP_NewString(const char *st, const char *name) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL(2);
/**
 * \param st: The string to assign.
 * \param maxncpy: The maximum size of the string (including the `\0` terminator).
 */
void IDP_AssignStringMaxSize(struct IDProperty *prop, const char *st, int maxncpy) ATTR_NONNULL();
void IDP_AssignString(struct IDProperty *prop, const char *st) ATTR_NONNULL();
void IDP_FreeString(struct IDProperty *prop) ATTR_NONNULL();

/*-------- ID Type -------*/

typedef void (*IDPWalkFunc)(void *user_data, struct IDProperty *idp);

void IDP_AssignID(struct IDProperty *prop, struct ID *id, int flag);

/*-------- Group Functions -------*/

/**
 * Sync values from one group to another when values name and types match,
 * copy the values, else ignore.
 *
 * \note Was used for syncing proxies.
 */
void IDP_SyncGroupValues(struct IDProperty *dest, const struct IDProperty *src) ATTR_NONNULL();
void IDP_SyncGroupTypes(struct IDProperty *dest, const struct IDProperty *src, bool do_arraylen)
    ATTR_NONNULL();
/**
 * Replaces all properties with the same name in a destination group from a source group.
 */
void IDP_ReplaceGroupInGroup(struct IDProperty *dest, const struct IDProperty *src) ATTR_NONNULL();
void IDP_ReplaceInGroup(struct IDProperty *group, struct IDProperty *prop) ATTR_NONNULL();
/**
 * Checks if a property with the same name as prop exists, and if so replaces it.
 * Use this to preserve order!
 */
void IDP_ReplaceInGroup_ex(struct IDProperty *group,
                           struct IDProperty *prop,
                           struct IDProperty *prop_exist);
/**
 * If a property is missing in \a dest, add it.
 * Do it recursively.
 */
void IDP_MergeGroup(struct IDProperty *dest, const struct IDProperty *src, bool do_overwrite)
    ATTR_NONNULL();
/**
 * If a property is missing in \a dest, add it.
 * Do it recursively.
 */
void IDP_MergeGroup_ex(struct IDProperty *dest,
                       const struct IDProperty *src,
                       bool do_overwrite,
                       int flag) ATTR_NONNULL();
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
bool IDP_AddToGroup(struct IDProperty *group, struct IDProperty *prop) ATTR_NONNULL();
/**
 * This is the same as IDP_AddToGroup, only you pass an item
 * in the group list to be inserted after.
 */
bool IDP_InsertToGroup(struct IDProperty *group,
                       struct IDProperty *previous,
                       struct IDProperty *pnew) ATTR_NONNULL(1 /* group */, 3 /* pnew */);
/**
 * \note this does not free the property!
 *
 * To free the property, you have to do:
 * #IDP_FreeProperty(prop);
 */
void IDP_RemoveFromGroup(struct IDProperty *group, struct IDProperty *prop) ATTR_NONNULL();
/**
 * Removes the property from the group and frees it.
 */
void IDP_FreeFromGroup(struct IDProperty *group, struct IDProperty *prop) ATTR_NONNULL();

struct IDProperty *IDP_GetPropertyFromGroup(const struct IDProperty *prop,
                                            const char *name) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
/**
 * Same as above but ensure type match.
 */
struct IDProperty *IDP_GetPropertyTypeFromGroup(const struct IDProperty *prop,
                                                const char *name,
                                                char type) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/*-------- Main Functions --------*/
/**
 * Get the Group property that contains the id properties for ID `id`.
 *
 * \param create_if_needed: Set to create the group property and attach it to id if it doesn't
 * exist; otherwise the function will return NULL if there's no Group property attached to the ID.
 */
struct IDProperty *IDP_GetProperties(struct ID *id, bool create_if_needed) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
struct IDProperty *IDP_CopyProperty(const struct IDProperty *prop) ATTR_WARN_UNUSED_RESULT
    ATTR_NONNULL();
struct IDProperty *IDP_CopyProperty_ex(const struct IDProperty *prop,
                                       int flag) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();
/**
 * Copy content from source #IDProperty into destination one,
 * freeing destination property's content first.
 */
void IDP_CopyPropertyContent(struct IDProperty *dst, const struct IDProperty *src) ATTR_NONNULL();

/**
 * \param is_strict: When false treat missing items as a match.
 */
bool IDP_EqualsProperties_ex(const struct IDProperty *prop1,
                             const struct IDProperty *prop2,
                             bool is_strict) ATTR_WARN_UNUSED_RESULT;

bool IDP_EqualsProperties(const struct IDProperty *prop1,
                          const struct IDProperty *prop2) ATTR_WARN_UNUSED_RESULT;

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
 * idgroup = IDP_GetProperties(some_id, 1);
 * IDP_AddToGroup(idgroup, color);
 * IDP_AddToGroup(idgroup, group);
 * \endcode
 *
 * Note that you MUST either attach the id property to an id property group with
 * IDP_AddToGroup or MEM_freeN the property, doing anything else might result in
 * a memory leak.
 */
struct IDProperty *IDP_New(char type,
                           const IDPropertyTemplate *val,
                           const char *name) ATTR_WARN_UNUSED_RESULT ATTR_NONNULL();

/**
 * \note This will free allocated data, all child properties of arrays and groups, and unlink IDs!
 * But it does not free the actual #IDProperty struct itself.
 */
void IDP_FreePropertyContent_ex(struct IDProperty *prop, bool do_id_user);
void IDP_FreePropertyContent(struct IDProperty *prop);
void IDP_FreeProperty_ex(struct IDProperty *prop, bool do_id_user);
void IDP_FreeProperty(struct IDProperty *prop);

void IDP_ClearProperty(struct IDProperty *prop);

void IDP_Reset(struct IDProperty *prop, const struct IDProperty *reference);

#define IDP_Int(prop) ((prop)->data.val)
#define IDP_Bool(prop) ((prop)->data.val)
#define IDP_Array(prop) ((prop)->data.pointer)
/* C11 const correctness for casts */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 201112L)
#  define IDP_Float(prop) \
    _Generic((prop), struct IDProperty * : (*(float *)&(prop)->data.val), const struct IDProperty * : (*(const float *)&(prop)->data.val))
#  define IDP_Double(prop) \
    _Generic((prop), struct IDProperty * : (*(double *)&(prop)->data.val), const struct IDProperty * : (*(const double *)&(prop)->data.val))
#  define IDP_String(prop) \
    _Generic((prop), struct IDProperty * : ((char *)(prop)->data.pointer), const struct IDProperty * : ((const char *)(prop)->data.pointer))
#  define IDP_IDPArray(prop) \
    _Generic((prop), struct IDProperty * : ((struct IDProperty *)(prop)->data.pointer), const struct IDProperty * : ((const struct IDProperty *)(prop)->data.pointer))
#  define IDP_Id(prop) \
    _Generic((prop), struct IDProperty * : ((ID *)(prop)->data.pointer), const struct IDProperty * : ((const ID *)(prop)->data.pointer))
#else
#  define IDP_Float(prop) (*(float *)&(prop)->data.val)
#  define IDP_Double(prop) (*(double *)&(prop)->data.val)
#  define IDP_String(prop) ((char *)(prop)->data.pointer)
#  define IDP_IDPArray(prop) ((struct IDProperty *)(prop)->data.pointer)
#  define IDP_Id(prop) ((ID *)(prop)->data.pointer)
#endif

/**
 * Return an int from an #IDProperty with a compatible type. This should be avoided, but
 * it's sometimes necessary, for example when legacy files have incorrect property types.
 */
int IDP_coerce_to_int_or_zero(const struct IDProperty *prop);
/**
 * Return a float from an #IDProperty with a compatible type. This should be avoided, but
 * it's sometimes necessary, for example when legacy files have incorrect property types.
 */
float IDP_coerce_to_float_or_zero(const struct IDProperty *prop);
/**
 * Return a double from an #IDProperty with a compatible type. This should be avoided, but
 * it's sometimes necessary, for example when legacy files have incorrect property types.
 */
double IDP_coerce_to_double_or_zero(const struct IDProperty *prop);

/**
 * Call a callback for each #IDproperty in the hierarchy under given root one (included).
 */
typedef void (*IDPForeachPropertyCallback)(struct IDProperty *id_property, void *user_data);

/**
 * Loop through all ID properties in hierarchy of given \a id_property_root included.
 *
 * \note Container types (groups and arrays) are processed after applying the callback on them.
 *
 * \param type_filter: If not 0, only apply callback on properties of matching types, see
 * IDP_TYPE_FILTER_ enum in DNA_ID.h.
 */
void IDP_foreach_property(struct IDProperty *id_property_root,
                          int type_filter,
                          IDPForeachPropertyCallback callback,
                          void *user_data);

/* Format IDProperty as strings */
char *IDP_reprN(const struct IDProperty *prop, uint *r_len);
void IDP_repr_fn(const struct IDProperty *prop,
                 void (*str_append_fn)(void *user_data, const char *str, uint str_len),
                 void *user_data);
void IDP_print(const struct IDProperty *prop);

void IDP_BlendWrite(struct BlendWriter *writer, const struct IDProperty *prop);
void IDP_BlendReadData_impl(struct BlendDataReader *reader,
                            struct IDProperty **prop,
                            const char *caller_func_id);
#define IDP_BlendDataRead(reader, prop) IDP_BlendReadData_impl(reader, prop, __func__)
void IDP_BlendReadLib(struct BlendLibReader *reader, struct ID *self_id, struct IDProperty *prop);
void IDP_BlendReadExpand(struct BlendExpander *expander, struct IDProperty *prop);

typedef enum eIDPropertyUIDataType {
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
} eIDPropertyUIDataType;

bool IDP_ui_data_supported(const struct IDProperty *prop);
eIDPropertyUIDataType IDP_ui_data_type(const struct IDProperty *prop);
void IDP_ui_data_free(struct IDProperty *prop);
/**
 * Free allocated pointers in the UI data that isn't shared with the UI data in the `other`
 * argument. Useful for returning early on failure when updating UI data in place, or when
 * replacing a subset of the UI data's allocated pointers.
 */
void IDP_ui_data_free_unique_contents(struct IDPropertyUIData *ui_data,
                                      eIDPropertyUIDataType type,
                                      const struct IDPropertyUIData *other);
struct IDPropertyUIData *IDP_ui_data_ensure(struct IDProperty *prop);
struct IDPropertyUIData *IDP_ui_data_copy(const struct IDProperty *prop);

#ifdef __cplusplus
}
#endif

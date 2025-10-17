/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Use a define instead of `#pragma once` because of `rna_internal.hh` */
#ifndef __RNA_ACCESS_H__
#define __RNA_ACCESS_H__

/** \file
 * \ingroup RNA
 */

#include <optional>
#include <stdarg.h>
#include <string>

#include "RNA_types.hh"

#include "BLI_compiler_attrs.h"
#include "BLI_enum_flags.hh"
#include "BLI_function_ref.hh"
#include "BLI_string_ref.hh"

struct ID;
struct IDOverrideLibrary;
struct IDOverrideLibraryProperty;
struct IDOverrideLibraryPropertyOperation;
struct IDProperty;
struct ListBase;
struct Main;
struct ReportList;
struct Scene;
struct bContext;

/* Types */
extern BlenderRNA BLENDER_RNA;

/* Pointer
 *
 * These functions will fill in RNA pointers, this can be done in three ways:
 * - a pointer Main is created by just passing the data pointer
 * - a pointer to a datablock can be created with the type and id data pointer
 * - a pointer to data contained in a datablock can be created with the id type
 *   and id data pointer, and the data type and pointer to the struct itself.
 *
 * There is also a way to get a pointer with the information about all structs.
 */

PointerRNA RNA_main_pointer_create(Main *main);
/**
 * Create a PointerRNA for an ID.
 *
 * \note By definition, currently these are always 'discrete' (have no ancestors). See
 * #PointerRNA::ancestors for details.
 */
PointerRNA RNA_id_pointer_create(ID *id);
/**
 * Create a 'discrete', isolated PointerRNA of some data. It won't have any ancestor information
 * available.
 *
 * \param id: The owner ID, may be null, in which case the PointerRNA won't have any ownership
 * information at all.
 */
PointerRNA RNA_pointer_create_discrete(ID *id, StructRNA *type, void *data);
/**
 * Create a PointerRNA of some data, using the given `parent` as immediate ancestor.
 *
 * This allows the PointerRNA to know to which data it belongs, all the way up to the root owner
 * ID.
 */
PointerRNA RNA_pointer_create_with_parent(const PointerRNA &parent, StructRNA *type, void *data);
/**
 * Create a PointerRNA of some data, with the given `id` data-block as single ancestor.
 *
 * This assumes that given `data` is an immediate (RNA-wise) child of the relevant RNA ID struct,
 * and is a shortcut for:
 *
 *    PointerRNA id_ptr = RNA_id_pointer_create(id);
 *    PointerRNA ptr = RNA_pointer_create_with_parent(id_ptr, &RNA_Type, data);
 */
PointerRNA RNA_pointer_create_id_subdata(ID &id, StructRNA *type, void *data);

/**
 * Create a PointerRNA representing the N'th ancestor of the given PointerRNA, where `0` is the
 * root.
 *
 * \note: Typically, the root ancestor should be an ID. But depending on how the PointerRNA and its
 * ancestors have been created, only part of the ancestor chain may be available, see
 * #PointerRNA::ancestors for details.
 */
PointerRNA RNA_pointer_create_from_ancestor(const PointerRNA &ptr, const int ancestor_idx);

bool RNA_pointer_is_null(const PointerRNA *ptr);

bool RNA_path_resolved_create(PointerRNA *ptr,
                              PropertyRNA *prop,
                              int prop_index,
                              PathResolvedRNA *r_anim_rna);

PointerRNA RNA_blender_rna_pointer_create();
PointerRNA RNA_pointer_recast(PointerRNA *ptr);

/* Structs */

StructRNA *RNA_struct_find(const char *identifier);

const char *RNA_struct_identifier(const StructRNA *type);
const char *RNA_struct_ui_name(const StructRNA *type);
const char *RNA_struct_ui_name_raw(const StructRNA *type);
const char *RNA_struct_ui_description(const StructRNA *type);
const char *RNA_struct_ui_description_raw(const StructRNA *type);
const char *RNA_struct_translation_context(const StructRNA *type);
int RNA_struct_ui_icon(const StructRNA *type);

PropertyRNA *RNA_struct_name_property(const StructRNA *type);
const EnumPropertyItem *RNA_struct_property_tag_defines(const StructRNA *type);
PropertyRNA *RNA_struct_iterator_property(StructRNA *type);
StructRNA *RNA_struct_base(StructRNA *type);
/**
 * Use to find the sub-type directly below a base-type.
 *
 * So if type were `RNA_SpotLight`, `RNA_struct_base_of(type, &RNA_ID)` would return `&RNA_Light`.
 */
const StructRNA *RNA_struct_base_child_of(const StructRNA *type, const StructRNA *parent_type);

bool RNA_struct_is_ID(const StructRNA *type);
bool RNA_struct_is_a(const StructRNA *type, const StructRNA *srna);

bool RNA_struct_undo_check(const StructRNA *type);

StructRegisterFunc RNA_struct_register(StructRNA *type);
StructUnregisterFunc RNA_struct_unregister(StructRNA *type);
void **RNA_struct_instance(PointerRNA *ptr);

void *RNA_struct_py_type_get(StructRNA *srna);
void RNA_struct_py_type_set(StructRNA *srna, void *py_type);

void *RNA_struct_blender_type_get(StructRNA *srna);
void RNA_struct_blender_type_set(StructRNA *srna, void *blender_type);

IDProperty **RNA_struct_idprops_p(PointerRNA *ptr);
IDProperty *RNA_struct_idprops(PointerRNA *ptr, bool create);
bool RNA_struct_idprops_check(const StructRNA *srna);
bool RNA_struct_system_idprops_register_check(const StructRNA *type);
bool RNA_struct_idprops_datablock_allowed(const StructRNA *type);

/** Get root IDProperty for system-defined runtime properties. */
IDProperty **RNA_struct_system_idprops_p(PointerRNA *ptr);
IDProperty *RNA_struct_system_idprops(PointerRNA *ptr, bool create);
/** Return `true` if the given RNA type supports system-defined IDProperties. */
bool RNA_struct_system_idprops_check(StructRNA *srna);
/**
 * Whether given type implies datablock usage by IDProperties.
 * This is used to prevent classes allowed to have IDProperties,
 * but not datablock ones, to indirectly use some
 * (e.g. by assigning an IDP_GROUP containing some IDP_ID pointers...).
 *
 * \note This is currently giving results for both user-defined and system-defined IDProperties,
 * there is no distinction for this between both storages.
 */
bool RNA_struct_idprops_contains_datablock(const StructRNA *type);
/**
 * Remove an id-property.
 */
bool RNA_struct_system_idprops_unset(PointerRNA *ptr, const char *identifier);

PropertyRNA *RNA_struct_find_property(PointerRNA *ptr, const char *identifier);

/**
 * Same as `RNA_struct_find_property` but returns `nullptr` if the property type is no same to
 * `property_type_check`.
 */
PropertyRNA *RNA_struct_find_property_check(PointerRNA &props,
                                            const char *name,
                                            const PropertyType property_type_check);
/**
 * Same as `RNA_struct_find_property` but returns `nullptr` if the property type is not
 * #PropertyType::PROP_COLLECTION or property struct type is different to `struct_type_check`.
 */
PropertyRNA *RNA_struct_find_collection_property_check(PointerRNA &props,
                                                       const char *name,
                                                       const StructRNA *struct_type_check);

bool RNA_struct_contains_property(PointerRNA *ptr, PropertyRNA *prop_test);
unsigned int RNA_struct_count_properties(StructRNA *srna);

/**
 * Return the closest ancestor (itself included) matching the requested RNA
 * type.
 *
 * The check starts from `ptr` itself, and then works its way up to the parent,
 * then grandparent, etc. The first one that matches is returned as an
 * `AncestorPointerRNA`.
 *
 * Base types are considered matching, so e.g. an RNA pointer of type
 * `RNA_SpotLight` will also match `RNA_Light`.
 *
 * \return The matching pointer if any, or `nullopt` otherwise.
 */
std::optional<AncestorPointerRNA> RNA_struct_search_closest_ancestor_by_type(
    PointerRNA *ptr, const StructRNA *srna);

/**
 * Low level direct access to type->properties,
 * note this ignores parent classes so should be used with care.
 */
const ListBase *RNA_struct_type_properties(StructRNA *srna);
PropertyRNA *RNA_struct_type_find_property_no_base(StructRNA *srna, const char *identifier);
/**
 * \note #RNA_struct_find_property is a higher level alternative to this function
 * which takes a #PointerRNA instead of a #StructRNA.
 */
PropertyRNA *RNA_struct_type_find_property(StructRNA *srna, const char *identifier);

FunctionRNA *RNA_struct_find_function(StructRNA *srna, const char *identifier);
const ListBase *RNA_struct_type_functions(StructRNA *srna);

[[nodiscard]] char *RNA_struct_name_get_alloc_ex(
    PointerRNA *ptr, char *fixedbuf, int fixedlen, int *r_len, PropertyRNA **r_nameprop);
[[nodiscard]] char *RNA_struct_name_get_alloc(PointerRNA *ptr,
                                              char *fixedbuf,
                                              int fixedlen,
                                              int *r_len);

/**
 * Use when registering structs with the #STRUCT_PUBLIC_NAMESPACE flag.
 */
bool RNA_struct_available_or_report(ReportList *reports, const char *identifier);
bool RNA_struct_bl_idname_ok_or_report(ReportList *reports,
                                       const char *identifier,
                                       const char *sep);

/* Properties
 *
 * Access to struct properties. All this works with RNA pointers rather than
 * direct pointers to the data. */

/* Property Information */

const char *RNA_property_identifier(const PropertyRNA *prop);
const char *RNA_property_description(PropertyRNA *prop);

const DeprecatedRNA *RNA_property_deprecated(const PropertyRNA *prop);

PropertyType RNA_property_type(PropertyRNA *prop);
PropertySubType RNA_property_subtype(PropertyRNA *prop);
PropertyUnit RNA_property_unit(PropertyRNA *prop);
PropertyScaleType RNA_property_ui_scale(PropertyRNA *prop);
int RNA_property_flag(PropertyRNA *prop);
int RNA_property_override_flag(PropertyRNA *prop);
/**
 * Get the tags set for \a prop as int bit-field.
 * \note Doesn't perform any validity check on the set bits. #RNA_def_property_tags does this
 *       in debug builds (to avoid performance issues in non-debug builds), which should be
 *       the only way to set tags. Hence, at this point we assume the tag bit-field to be valid.
 */
int RNA_property_tags(PropertyRNA *prop);
PropertyPathTemplateType RNA_property_path_template_type(PropertyRNA *prop);
bool RNA_property_builtin(PropertyRNA *prop);
void *RNA_property_py_data_get(PropertyRNA *prop);

int RNA_property_array_length(PointerRNA *ptr, PropertyRNA *prop);
bool RNA_property_array_check(PropertyRNA *prop);
/**
 * Return the size of Nth dimension.
 */
int RNA_property_multi_array_length(PointerRNA *ptr, PropertyRNA *prop, int dimension);
/**
 * Used by BPY to make an array from the python object.
 */
int RNA_property_array_dimension(const PointerRNA *ptr, PropertyRNA *prop, int length[]);
char RNA_property_array_item_char(PropertyRNA *prop, int index);
int RNA_property_array_item_index(PropertyRNA *prop, char name);

/**
 * \return the maximum length including the \0 terminator. '0' is used when there is no maximum.
 */
int RNA_property_string_maxlength(PropertyRNA *prop);

const char *RNA_property_ui_name(const PropertyRNA *prop, const PointerRNA *ptr = nullptr);
const char *RNA_property_ui_name_raw(const PropertyRNA *prop, const PointerRNA *ptr = nullptr);
const char *RNA_property_ui_description(const PropertyRNA *prop);
const char *RNA_property_ui_description_raw(const PropertyRNA *prop);
const char *RNA_property_translation_context(const PropertyRNA *prop);
int RNA_property_ui_icon(const PropertyRNA *prop);

/* Dynamic Property Information */

void RNA_property_int_range(PointerRNA *ptr, PropertyRNA *prop, int *hardmin, int *hardmax);
void RNA_property_int_ui_range(
    PointerRNA *ptr, PropertyRNA *prop, int *softmin, int *softmax, int *step);

void RNA_property_float_range(PointerRNA *ptr, PropertyRNA *prop, float *hardmin, float *hardmax);
void RNA_property_float_ui_range(PointerRNA *ptr,
                                 PropertyRNA *prop,
                                 float *softmin,
                                 float *softmax,
                                 float *step,
                                 float *precision);

int RNA_property_float_clamp(PointerRNA *ptr, PropertyRNA *prop, float *value);
int RNA_property_int_clamp(PointerRNA *ptr, PropertyRNA *prop, int *value);

bool RNA_enum_identifier(const EnumPropertyItem *item, int value, const char **r_identifier);
int RNA_enum_bitflag_identifiers(const EnumPropertyItem *item,
                                 int value,
                                 const char **r_identifier);
bool RNA_enum_name(const EnumPropertyItem *item, int value, const char **r_name);
bool RNA_enum_name_gettexted(const EnumPropertyItem *item,
                             int value,
                             const char *translation_context,
                             const char **r_name);
bool RNA_enum_description(const EnumPropertyItem *item, int value, const char **r_description);
int RNA_enum_from_value(const EnumPropertyItem *item, int value);
int RNA_enum_from_identifier(const EnumPropertyItem *item, const char *identifier);
bool RNA_enum_value_from_identifier(const EnumPropertyItem *item,
                                    const char *identifier,
                                    int *r_value);
/**
 * Take care using this with translated enums,
 * prefer #RNA_enum_from_identifier where possible.
 */
int RNA_enum_from_name(const EnumPropertyItem *item, const char *name);
unsigned int RNA_enum_items_count(const EnumPropertyItem *item);

void RNA_property_enum_items_ex(bContext *C,
                                PointerRNA *ptr,
                                PropertyRNA *prop,
                                bool use_static,
                                const EnumPropertyItem **r_item,
                                int *r_totitem,
                                bool *r_free);
void RNA_property_enum_items(bContext *C,
                             PointerRNA *ptr,
                             PropertyRNA *prop,
                             const EnumPropertyItem **r_item,
                             int *r_totitem,
                             bool *r_free);
void RNA_property_enum_items_gettexted(bContext *C,
                                       PointerRNA *ptr,
                                       PropertyRNA *prop,
                                       const EnumPropertyItem **r_item,
                                       int *r_totitem,
                                       bool *r_free);
void RNA_property_enum_items_gettexted_all(bContext *C,
                                           PointerRNA *ptr,
                                           PropertyRNA *prop,
                                           const EnumPropertyItem **r_item,
                                           int *r_totitem,
                                           bool *r_free);
bool RNA_property_enum_value(
    bContext *C, PointerRNA *ptr, PropertyRNA *prop, const char *identifier, int *r_value);
bool RNA_property_enum_identifier(
    bContext *C, PointerRNA *ptr, PropertyRNA *prop, int value, const char **r_identifier);
bool RNA_property_enum_name(
    bContext *C, PointerRNA *ptr, PropertyRNA *prop, int value, const char **r_name);
bool RNA_property_enum_name_gettexted(
    bContext *C, PointerRNA *ptr, PropertyRNA *prop, int value, const char **r_name);

bool RNA_property_enum_item_from_value(
    bContext *C, PointerRNA *ptr, PropertyRNA *prop, int value, EnumPropertyItem *r_item);
bool RNA_property_enum_item_from_value_gettexted(
    bContext *C, PointerRNA *ptr, PropertyRNA *prop, int value, EnumPropertyItem *r_item);

int RNA_property_enum_bitflag_identifiers(
    bContext *C, PointerRNA *ptr, PropertyRNA *prop, int value, const char **r_identifier);

StructRNA *RNA_property_pointer_type(PointerRNA *ptr, PropertyRNA *prop);
bool RNA_property_pointer_poll(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *value);

/**
 * A property is a runtime property if the PROP_INTERN_RUNTIME flag is set on it.
 */
bool RNA_property_is_runtime(const PropertyRNA *prop);

bool RNA_property_editable(const PointerRNA *ptr, PropertyRNA *prop);
/**
 * Version of #RNA_property_editable that tries to return additional info in \a r_info
 * that can be exposed in UI.
 */
bool RNA_property_editable_info(const PointerRNA *ptr, PropertyRNA *prop, const char **r_info);
/**
 * Same as RNA_property_editable(), except this checks individual items in an array.
 */
bool RNA_property_editable_index(const PointerRNA *ptr, PropertyRNA *prop, const int index);

/**
 * Without lib check, only checks the flag.
 */
bool RNA_property_editable_flag(const PointerRNA *ptr, PropertyRNA *prop);

/**
 * A property is animateable if its ID and the RNA property itself are defined as editable.
 * It does not imply that user can _edit_ such animation though, see #RNA_property_anim_editable
 * for this.
 *
 * This check is only based on information stored in the data _types_ (IDTypeInfo and RNA property
 * definition), not on the actual data itself.
 */
bool RNA_property_animateable(const PointerRNA *ptr, PropertyRNA *prop_orig);
/**
 * A property is anim-editable if it is animateable, and the related data is editable.
 *
 * Unlike #RNA_property_animateable, this check the actual data referenced by the RNA pointer and
 * property, and not only their type info.
 *
 * Typically (with a few exceptions like the #PROP_LIB_EXCEPTION PropertyRNA flag), editable data
 * belongs to local ID.
 */
bool RNA_property_anim_editable(const PointerRNA *ptr, PropertyRNA *prop_orig);
bool RNA_property_animated(PointerRNA *ptr, PropertyRNA *prop);
/**
 * With LibOverrides, a property may be animatable and anim-editable, but not driver-editable (in
 * case the reference data already has an animation data, its Action can be an editable local ID,
 * but the drivers are directly stored in the animation-data, overriding these is not supported
 * currently).
 *
 * Like #RNA_property_anim_editable, this also checks the actual data referenced by the RNA pointer
 * and property.
 *
 * Currently, it is assumed that if an IDType and RNAProperty are animatable, they are also
 * driveable, so #RNA_property_animateable can be used for drivers as well.
 */
bool RNA_property_driver_editable(const PointerRNA *ptr, PropertyRNA *prop);
/**
 * \note Does not take into account editable status, this has to be checked separately
 * (using #RNA_property_editable_flag() usually).
 */
bool RNA_property_overridable_get(const PointerRNA *ptr, PropertyRNA *prop);
/**
 * Should only be used for custom properties.
 */
bool RNA_property_overridable_library_set(PointerRNA *ptr, PropertyRNA *prop, bool is_overridable);
bool RNA_property_overridden(PointerRNA *ptr, PropertyRNA *prop);
bool RNA_property_comparable(PointerRNA *ptr, PropertyRNA *prop);
/**
 * This function is to check if its possible to create a valid path from the ID
 * its slow so don't call in a loop.
 */
bool RNA_property_path_from_ID_check(PointerRNA *ptr, PropertyRNA *prop); /* slow, use with care */

void RNA_property_update(bContext *C, PointerRNA *ptr, PropertyRNA *prop);
/**
 * \param scene: may be NULL.
 */
void RNA_property_update_main(Main *bmain, Scene *scene, PointerRNA *ptr, PropertyRNA *prop);
/**
 * \note its possible this returns a false positive in the case of #PROP_CONTEXT_UPDATE
 * but this isn't likely to be a performance problem.
 */
bool RNA_property_update_check(PropertyRNA *prop);

/* Property Data */

bool RNA_property_boolean_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_boolean_set(PointerRNA *ptr, PropertyRNA *prop, bool value);
void RNA_property_boolean_get_array(PointerRNA *ptr, PropertyRNA *prop, bool *values);
void RNA_property_boolean_get_array_at_most(PointerRNA *ptr,
                                            PropertyRNA *prop,
                                            bool *values,
                                            int values_num);
bool RNA_property_boolean_get_index(PointerRNA *ptr, PropertyRNA *prop, int index);
void RNA_property_boolean_set_array(PointerRNA *ptr, PropertyRNA *prop, const bool *values);
void RNA_property_boolean_set_array_at_most(PointerRNA *ptr,
                                            PropertyRNA *prop,
                                            const bool *values,
                                            int values_num);
void RNA_property_boolean_set_index(PointerRNA *ptr, PropertyRNA *prop, int index, bool value);
bool RNA_property_boolean_get_default(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_boolean_get_default_array(PointerRNA *ptr, PropertyRNA *prop, bool *values);
bool RNA_property_boolean_get_default_index(PointerRNA *ptr, PropertyRNA *prop, int index);

int RNA_property_int_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_int_set(PointerRNA *ptr, PropertyRNA *prop, int value);
void RNA_property_int_get_array(PointerRNA *ptr, PropertyRNA *prop, int *values);
void RNA_property_int_get_array_at_most(PointerRNA *ptr,
                                        PropertyRNA *prop,
                                        int *values,
                                        int values_num);
void RNA_property_int_get_array_range(PointerRNA *ptr, PropertyRNA *prop, int values[2]);
int RNA_property_int_get_index(PointerRNA *ptr, PropertyRNA *prop, int index);
void RNA_property_int_set_array(PointerRNA *ptr, PropertyRNA *prop, const int *values);
void RNA_property_int_set_index(PointerRNA *ptr, PropertyRNA *prop, int index, int value);
void RNA_property_int_set_array_at_most(PointerRNA *ptr,
                                        PropertyRNA *prop,
                                        const int *values,
                                        int values_num);
int RNA_property_int_get_default(PointerRNA *ptr, PropertyRNA *prop);
bool RNA_property_int_set_default(PropertyRNA *prop, int value);
void RNA_property_int_get_default_array(PointerRNA *ptr, PropertyRNA *prop, int *values);
int RNA_property_int_get_default_index(PointerRNA *ptr, PropertyRNA *prop, int index);

float RNA_property_float_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_float_set(PointerRNA *ptr, PropertyRNA *prop, float value);
void RNA_property_float_get_array(PointerRNA *ptr, PropertyRNA *prop, float *values);
void RNA_property_float_get_array_at_most(PointerRNA *ptr,
                                          PropertyRNA *prop,
                                          float *values,
                                          int values_num);
void RNA_property_float_get_array_range(PointerRNA *ptr, PropertyRNA *prop, float values[2]);
float RNA_property_float_get_index(PointerRNA *ptr, PropertyRNA *prop, int index);
void RNA_property_float_set_array(PointerRNA *ptr, PropertyRNA *prop, const float *values);
void RNA_property_float_set_array_at_most(PointerRNA *ptr,
                                          PropertyRNA *prop,
                                          const float *values,
                                          int values_num);
void RNA_property_float_set_index(PointerRNA *ptr, PropertyRNA *prop, int index, float value);
float RNA_property_float_get_default(PointerRNA *ptr, PropertyRNA *prop);
bool RNA_property_float_set_default(PropertyRNA *prop, float value);
void RNA_property_float_get_default_array(PointerRNA *ptr, PropertyRNA *prop, float *values);
float RNA_property_float_get_default_index(PointerRNA *ptr, PropertyRNA *prop, int index);

std::string RNA_property_string_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_string_get(PointerRNA *ptr, PropertyRNA *prop, char *value);
char *RNA_property_string_get_alloc(PointerRNA *ptr,
                                    PropertyRNA *prop,
                                    char *fixedbuf,
                                    int fixedlen,
                                    int *r_len) ATTR_WARN_UNUSED_RESULT;
void RNA_property_string_set(PointerRNA *ptr, PropertyRNA *prop, const char *value);
void RNA_property_string_set_bytes(PointerRNA *ptr, PropertyRNA *prop, const char *value, int len);

eStringPropertySearchFlag RNA_property_string_search_flag(PropertyRNA *prop);
/**
 * Search candidates for string `prop` by calling `visit_fn` with each string.
 *
 * See #PropStringSearchFunc for details.
 */
void RNA_property_string_search(
    const bContext *C,
    PointerRNA *ptr,
    PropertyRNA *prop,
    const char *edit_text,
    blender::FunctionRef<void(StringPropertySearchVisitParams)> visit_fn);

/**
 * For filepath properties, get a glob pattern to filter possible files.
 * For example: `*.csv`
 */
std::optional<std::string> RNA_property_string_path_filter(const bContext *C,
                                                           PointerRNA *ptr,
                                                           PropertyRNA *prop);

/**
 * \return The final length without `\0` terminator (might differ from the length of the stored
 * string, when a `get_transform` callback is defined).
 */
int RNA_property_string_length(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_string_get_default(PropertyRNA *prop, char *value, int value_maxncpy);
char *RNA_property_string_get_default_alloc(PointerRNA *ptr,
                                            PropertyRNA *prop,
                                            char *fixedbuf,
                                            int fixedlen,
                                            int *r_len) ATTR_WARN_UNUSED_RESULT;
/**
 * \return the length without `\0` terminator.
 */
int RNA_property_string_default_length(PointerRNA *ptr, PropertyRNA *prop);

int RNA_property_enum_get(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_enum_set(PointerRNA *ptr, PropertyRNA *prop, int value);
int RNA_property_enum_get_default(PointerRNA *ptr, PropertyRNA *prop);
/**
 * Get the value of the item that is \a step items away from \a from_value.
 *
 * \param from_value: Item value to start stepping from.
 * \param step: Absolute value defines step size, sign defines direction.
 * E.g to get the next item, pass 1, for the previous -1.
 */
int RNA_property_enum_step(
    const bContext *C, PointerRNA *ptr, PropertyRNA *prop, int from_value, int step);

/**
 * WARNING: _may_ create data in IDPGroup backend storage case.
 * While creation of data itself is mutex-protected, potential concurrent _accesses_ to the same
 * property are not, so threaded calls to #RNA_property_pointer_get() remain highly unsafe.
 */
PointerRNA RNA_property_pointer_get(PointerRNA *ptr, PropertyRNA *prop) ATTR_NONNULL(1, 2);
/**
 * Same as above, but never creates an empty IDPGroup property for Pointer runtime properties that
 * are not set yet.
 *
 * Ideally this should never be done ever, as it is intrisically not threadsafe, but for the time
 * being at least provide a way to avoid this bad behavior. */
PointerRNA RNA_property_pointer_get_never_create(PointerRNA *ptr, PropertyRNA *prop)
    ATTR_NONNULL(1, 2);
void RNA_property_pointer_set(PointerRNA *ptr,
                              PropertyRNA *prop,
                              PointerRNA ptr_value,
                              ReportList *reports) ATTR_NONNULL(1, 2);
PointerRNA RNA_property_pointer_get_default(PointerRNA *ptr, PropertyRNA *prop) ATTR_NONNULL(1, 2);

void RNA_property_collection_begin(PointerRNA *ptr,
                                   PropertyRNA *prop,
                                   CollectionPropertyIterator *iter);
void RNA_property_collection_next(CollectionPropertyIterator *iter);
void RNA_property_collection_skip(CollectionPropertyIterator *iter, int num);
void RNA_property_collection_end(CollectionPropertyIterator *iter);
int RNA_property_collection_length(PointerRNA *ptr, PropertyRNA *prop);
/**
 * Return true when `RNA_property_collection_length(ptr, prop) == 0`,
 * without having to iterate over items in the collection (needed for some kinds of collections).
 */
bool RNA_property_collection_is_empty(PointerRNA *ptr, PropertyRNA *prop);
int RNA_property_collection_lookup_index(PointerRNA *ptr,
                                         PropertyRNA *prop,
                                         const PointerRNA *t_ptr);
bool RNA_property_collection_lookup_int(PointerRNA *ptr,
                                        PropertyRNA *prop,
                                        int key,
                                        PointerRNA *r_ptr);
bool RNA_property_collection_lookup_string(PointerRNA *ptr,
                                           PropertyRNA *prop,
                                           const char *key,
                                           PointerRNA *r_ptr);
bool RNA_property_collection_lookup_string_index(
    PointerRNA *ptr, PropertyRNA *prop, const char *key, PointerRNA *r_ptr, int *r_index);

bool RNA_property_collection_lookup_int_has_fn(PropertyRNA *prop);
bool RNA_property_collection_lookup_string_has_fn(PropertyRNA *prop);
bool RNA_property_collection_lookup_string_has_nameprop(PropertyRNA *prop);
/**
 * Return true when this type supports string lookups,
 * it has a lookup function or it's type has a name property.
 */
bool RNA_property_collection_lookup_string_supported(PropertyRNA *prop);

/**
 * Zero return is an assignment error.
 */
bool RNA_property_collection_assign_int(PointerRNA *ptr,
                                        PropertyRNA *prop,
                                        int key,
                                        const PointerRNA *assign_ptr);
bool RNA_property_collection_type_get(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *r_ptr);

/* efficient functions to set properties for arrays */
int RNA_property_collection_raw_array(
    PointerRNA *ptr, PropertyRNA *prop, PropertyRNA *itemprop, bool set, RawArray *array);
int RNA_property_collection_raw_get(ReportList *reports,
                                    PointerRNA *ptr,
                                    PropertyRNA *prop,
                                    const char *propname,
                                    void *array,
                                    RawPropertyType type,
                                    int len);
int RNA_property_collection_raw_set(ReportList *reports,
                                    PointerRNA *ptr,
                                    PropertyRNA *prop,
                                    const char *propname,
                                    void *array,
                                    RawPropertyType type,
                                    int len);
size_t RNA_raw_type_sizeof(RawPropertyType type);
RawPropertyType RNA_property_raw_type(PropertyRNA *prop);

/* to create ID property groups */
void RNA_property_pointer_add(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_pointer_remove(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_collection_add(PointerRNA *ptr, PropertyRNA *prop, PointerRNA *r_ptr);
bool RNA_property_collection_remove(PointerRNA *ptr, PropertyRNA *prop, int key);
void RNA_property_collection_clear(PointerRNA *ptr, PropertyRNA *prop);
bool RNA_property_collection_move(PointerRNA *ptr, PropertyRNA *prop, int key, int pos);

/* copy/reset */
bool RNA_property_copy(
    Main *bmain, PointerRNA *ptr, PointerRNA *fromptr, PropertyRNA *prop, int index);
bool RNA_property_reset(PointerRNA *ptr, PropertyRNA *prop, int index);
bool RNA_property_assign_default(PointerRNA *ptr, PropertyRNA *prop);

/* Quick name based property access
 *
 * These are just an easier way to access property values without having to
 * call RNA_struct_find_property. The names have to exist as RNA properties
 * for the type in the pointer, if they do not exist an error will be printed.
 *
 * There is no support for pointers and collections here yet, these can be
 * added when ID properties support them. */

bool RNA_boolean_get(PointerRNA *ptr, const char *name);
void RNA_boolean_set(PointerRNA *ptr, const char *name, bool value);
void RNA_boolean_get_array(PointerRNA *ptr, const char *name, bool *values);
void RNA_boolean_set_array(PointerRNA *ptr, const char *name, const bool *values);

int RNA_int_get(PointerRNA *ptr, const char *name);
void RNA_int_set(PointerRNA *ptr, const char *name, int value);
void RNA_int_get_array(PointerRNA *ptr, const char *name, int *values);
void RNA_int_set_array(PointerRNA *ptr, const char *name, const int *values);

float RNA_float_get(PointerRNA *ptr, const char *name);
void RNA_float_set(PointerRNA *ptr, const char *name, float value);
void RNA_float_get_array(PointerRNA *ptr, const char *name, float *values);
void RNA_float_set_array(PointerRNA *ptr, const char *name, const float *values);

int RNA_enum_get(PointerRNA *ptr, const char *name);
void RNA_enum_set(PointerRNA *ptr, const char *name, int value);
void RNA_enum_set_identifier(bContext *C, PointerRNA *ptr, const char *name, const char *id);
bool RNA_enum_is_equal(bContext *C, PointerRNA *ptr, const char *name, const char *enumname);

/* Lower level functions that don't use a PointerRNA. */
bool RNA_enum_value_from_id(const EnumPropertyItem *item, const char *identifier, int *r_value);
bool RNA_enum_id_from_value(const EnumPropertyItem *item, int value, const char **r_identifier);
bool RNA_enum_icon_from_value(const EnumPropertyItem *item, int value, int *r_icon);
bool RNA_enum_name_from_value(const EnumPropertyItem *item, int value, const char **r_name);

void RNA_string_get(PointerRNA *ptr, const char *name, char *value);
/**
 * Retrieve string from a string property, or an empty string if the property does not exist.
 * \note This mostly exists as a C++ replacement for #RNA_string_get_alloc or a simpler replacement
 * for the overload with a return pointer argument with easy support for arbitrary length strings.
 */
std::string RNA_string_get(PointerRNA *ptr, const char *name);
char *RNA_string_get_alloc(PointerRNA *ptr,
                           const char *name,
                           char *fixedbuf,
                           int fixedlen,
                           int *r_len) ATTR_WARN_UNUSED_RESULT;
int RNA_string_length(PointerRNA *ptr, const char *name);
void RNA_string_set(PointerRNA *ptr, const char *name, const char *value);

/**
 * Retrieve the named property from PointerRNA.
 */
PointerRNA RNA_pointer_get(PointerRNA *ptr, const char *name);
/* Set the property name of PointerRNA ptr to ptr_value */
void RNA_pointer_set(PointerRNA *ptr, const char *name, PointerRNA ptr_value);
void RNA_pointer_add(PointerRNA *ptr, const char *name);

void RNA_collection_begin(PointerRNA *ptr, const char *name, CollectionPropertyIterator *iter);
int RNA_collection_length(PointerRNA *ptr, const char *name);
bool RNA_collection_is_empty(PointerRNA *ptr, const char *name);
void RNA_collection_add(PointerRNA *ptr, const char *name, PointerRNA *r_value);
void RNA_collection_clear(PointerRNA *ptr, const char *name);

#define RNA_BEGIN(sptr, itemptr, propname) \
  { \
    CollectionPropertyIterator rna_macro_iter; \
    for (RNA_collection_begin(sptr, propname, &rna_macro_iter); rna_macro_iter.valid; \
         RNA_property_collection_next(&rna_macro_iter)) \
    { \
      PointerRNA itemptr = rna_macro_iter.ptr;

#define RNA_END \
  } \
  RNA_property_collection_end(&rna_macro_iter); \
  } \
  ((void)0)

#define RNA_PROP_BEGIN(sptr, itemptr, prop) \
  { \
    CollectionPropertyIterator rna_macro_iter; \
    for (RNA_property_collection_begin(sptr, prop, &rna_macro_iter); rna_macro_iter.valid; \
         RNA_property_collection_next(&rna_macro_iter)) \
    { \
      PointerRNA itemptr = rna_macro_iter.ptr;

#define RNA_PROP_END \
  } \
  RNA_property_collection_end(&rna_macro_iter); \
  } \
  ((void)0)

#define RNA_STRUCT_BEGIN(sptr, prop) \
  { \
    CollectionPropertyIterator rna_macro_iter{}; \
    for (RNA_property_collection_begin( \
             sptr, RNA_struct_iterator_property((sptr)->type), &rna_macro_iter); \
         rna_macro_iter.valid; \
         RNA_property_collection_next(&rna_macro_iter)) \
    { \
      PropertyRNA *prop = (PropertyRNA *)rna_macro_iter.ptr.data;

#define RNA_STRUCT_BEGIN_SKIP_RNA_TYPE(sptr, prop) \
  { \
    CollectionPropertyIterator rna_macro_iter; \
    RNA_property_collection_begin( \
        sptr, RNA_struct_iterator_property((sptr)->type), &rna_macro_iter); \
    if (rna_macro_iter.valid) { \
      RNA_property_collection_next(&rna_macro_iter); \
    } \
    for (; rna_macro_iter.valid; RNA_property_collection_next(&rna_macro_iter)) { \
      PropertyRNA *prop = (PropertyRNA *)rna_macro_iter.ptr.data;

#define RNA_STRUCT_END \
  } \
  RNA_property_collection_end(&rna_macro_iter); \
  } \
  ((void)0)

/**
 * Check if the #IDproperty exists, for operators.
 *
 * \param use_ghost: Internally an #IDProperty may exist,
 * without the RNA considering it to be "set", see #IDP_FLAG_GHOST.
 * This is used for operators, where executing an operator that has run previously
 * will re-use the last value (unless #PROP_SKIP_SAVE property is set).
 * In this case, the presence of an existing value shouldn't prevent it being initialized
 * from the context. Even though this value will be returned if it's requested,
 * it's not considered to be set (as it would if the menu item or key-map defined it's value).
 * Set `use_ghost` to true for default behavior, otherwise false to check if there is a value
 * exists internally and would be returned on request.
 */
bool RNA_property_is_set_ex(PointerRNA *ptr, PropertyRNA *prop, bool use_ghost);
bool RNA_property_is_set(PointerRNA *ptr, PropertyRNA *prop);
void RNA_property_unset(PointerRNA *ptr, PropertyRNA *prop);
/** See #RNA_property_is_set_ex documentation. */
bool RNA_struct_property_is_set_ex(PointerRNA *ptr, const char *identifier, bool use_ghost);
bool RNA_struct_property_is_set(PointerRNA *ptr, const char *identifier);
bool RNA_property_is_idprop(const PropertyRNA *prop);
/**
 * \note Mainly for the UI.
 */
bool RNA_property_is_unlink(PropertyRNA *prop);
void RNA_struct_property_unset(PointerRNA *ptr, const char *identifier);

/**
 * Python compatible string representation of this property, (must be freed!).
 */
std::string RNA_property_as_string(
    bContext *C, PointerRNA *ptr, PropertyRNA *prop, int index, int max_prop_length);
/**
 * String representation of a property, Python compatible but can be used for display too.
 * \param C: can be NULL.
 */
std::string RNA_pointer_as_string_id(bContext *C, PointerRNA *ptr);
std::optional<std::string> RNA_pointer_as_string(bContext *C,
                                                 PointerRNA *ptr,
                                                 PropertyRNA *prop_ptr,
                                                 PointerRNA *ptr_prop);
/**
 * \param C: can be NULL.
 */
std::string RNA_pointer_as_string_keywords_ex(bContext *C,
                                              PointerRNA *ptr,
                                              bool as_function,
                                              bool all_args,
                                              bool nested_args,
                                              int max_prop_length,
                                              PropertyRNA *iterprop);
std::string RNA_pointer_as_string_keywords(bContext *C,
                                           PointerRNA *ptr,
                                           bool as_function,
                                           bool all_args,
                                           bool nested_args,
                                           int max_prop_length);
std::string RNA_function_as_string_keywords(
    bContext *C, FunctionRNA *func, bool as_function, bool all_args, int max_prop_length);

/* Function */

const char *RNA_function_identifier(FunctionRNA *func);
const char *RNA_function_ui_description(FunctionRNA *func);
const char *RNA_function_ui_description_raw(FunctionRNA *func);
int RNA_function_flag(FunctionRNA *func);
int RNA_function_defined(FunctionRNA *func);

PropertyRNA *RNA_function_get_parameter(PointerRNA *ptr, FunctionRNA *func, int index);
PropertyRNA *RNA_function_find_parameter(PointerRNA *ptr,
                                         FunctionRNA *func,
                                         const char *identifier);
const ListBase *RNA_function_defined_parameters(FunctionRNA *func);

/* Utility */

int RNA_parameter_flag(PropertyRNA *prop);

ParameterList *RNA_parameter_list_create(ParameterList *parms, PointerRNA *ptr, FunctionRNA *func);
void RNA_parameter_list_free(ParameterList *parms);
int RNA_parameter_list_size(const ParameterList *parms);
int RNA_parameter_list_arg_count(const ParameterList *parms);
int RNA_parameter_list_ret_count(const ParameterList *parms);

void RNA_parameter_list_begin(ParameterList *parms, ParameterIterator *iter);
void RNA_parameter_list_next(ParameterIterator *iter);
void RNA_parameter_list_end(ParameterIterator *iter);

void RNA_parameter_get(ParameterList *parms, PropertyRNA *parm, void **r_value);
void RNA_parameter_get_lookup(ParameterList *parms, const char *identifier, void **r_value);
void RNA_parameter_set(ParameterList *parms, PropertyRNA *parm, const void *value);
void RNA_parameter_set_lookup(ParameterList *parms, const char *identifier, const void *value);

/* Only for PROP_DYNAMIC properties! */

int RNA_parameter_dynamic_length_get(ParameterList *parms, PropertyRNA *parm);
int RNA_parameter_dynamic_length_get_data(ParameterList *parms, PropertyRNA *parm, void *data);
void RNA_parameter_dynamic_length_set(ParameterList *parms, PropertyRNA *parm, int length);
void RNA_parameter_dynamic_length_set_data(ParameterList *parms,
                                           PropertyRNA *parm,
                                           void *data,
                                           int length);

int RNA_function_call(
    bContext *C, ReportList *reports, PointerRNA *ptr, FunctionRNA *func, ParameterList *parms);

std::optional<blender::StringRefNull> RNA_translate_ui_text(
    const char *text, const char *text_ctxt, StructRNA *type, PropertyRNA *prop, int translate);

/* ID */

short RNA_type_to_ID_code(const StructRNA *type);
StructRNA *ID_code_to_RNA_type(short idcode);

/* macro which inserts the function name */
#if defined __GNUC__
#  define RNA_warning(format, args...) _RNA_warning("%s: " format "\n", __func__, ##args)
#elif defined(_MSVC_TRADITIONAL) && \
    !_MSVC_TRADITIONAL /* The "new preprocessor" is enabled via `/Zc:preprocessor`. */
#  define RNA_warning(format, ...) _RNA_warning("%s: " format "\n", __FUNCTION__, ##__VA_ARGS__)
#else
#  define RNA_warning(format, ...) _RNA_warning("%s: " format "\n", __FUNCTION__, __VA_ARGS__)
#endif

/** Use to implement the #RNA_warning macro which includes `__func__` suffix. */
void _RNA_warning(const char *format, ...) ATTR_PRINTF_FORMAT(1, 2);

/* Equals test. */

/**
 * \note In practice, #EQ_STRICT and #EQ_COMPARE have same behavior currently,
 * and will yield same result.
 */
enum eRNACompareMode {
  /* Only care about equality, not full comparison. */
  /** Set/unset ignored. */
  RNA_EQ_STRICT,
  /** Unset property matches anything. */
  RNA_EQ_UNSET_MATCH_ANY,
  /** Unset property never matches set property. */
  RNA_EQ_UNSET_MATCH_NONE,
  /** Full comparison. */
  RNA_EQ_COMPARE,
};

bool RNA_property_equals(
    Main *bmain, PointerRNA *ptr_a, PointerRNA *ptr_b, PropertyRNA *prop, eRNACompareMode mode);
bool RNA_struct_equals(Main *bmain, PointerRNA *ptr_a, PointerRNA *ptr_b, eRNACompareMode mode);

/* Override. */

/** Flags for #RNA_struct_override_matches. */
enum eRNAOverrideMatch {
  /** Do not compare properties that are not overridable. */
  RNA_OVERRIDE_COMPARE_IGNORE_NON_OVERRIDABLE = 1 << 0,
  /** Do not compare properties that are already overridden. */
  RNA_OVERRIDE_COMPARE_IGNORE_OVERRIDDEN = 1 << 1,

  /** Create new property override if needed and possible. */
  RNA_OVERRIDE_COMPARE_CREATE = 1 << 16,
  /** Restore property's value(s) to reference ones, if needed and possible. */
  RNA_OVERRIDE_COMPARE_RESTORE = 1 << 17,
  /** Tag for restoration of property's value(s) to reference ones, if needed and possible. */
  RNA_OVERRIDE_COMPARE_TAG_FOR_RESTORE = 1 << 18,
};
ENUM_OPERATORS(eRNAOverrideMatch)

enum eRNAOverrideMatchResult {
  RNA_OVERRIDE_MATCH_RESULT_INIT = 0,

  /**
   * Some new property overrides were created to take into account
   * differences between local and reference.
   */
  RNA_OVERRIDE_MATCH_RESULT_CREATED = 1 << 0,
  /**
   * Some properties are illegally different from their reference values and have been tagged for
   * restoration.
   */
  RNA_OVERRIDE_MATCH_RESULT_RESTORE_TAGGED = 1 << 1,
  /** Some properties were reset to reference values. */
  RNA_OVERRIDE_MATCH_RESULT_RESTORED = 1 << 2,
};
ENUM_OPERATORS(eRNAOverrideMatchResult)

enum eRNAOverrideStatus {
  /** The property is overridable. */
  RNA_OVERRIDE_STATUS_OVERRIDABLE = 1 << 0,
  /** The property is overridden. */
  RNA_OVERRIDE_STATUS_OVERRIDDEN = 1 << 1,
  /** Overriding this property is mandatory when creating an override. */
  RNA_OVERRIDE_STATUS_MANDATORY = 1 << 2,
  /** The override status of this property is locked. */
  RNA_OVERRIDE_STATUS_LOCKED = 1 << 3,
};
ENUM_OPERATORS(eRNAOverrideStatus)

/**
 * Check whether reference and local overridden data match (are the same),
 * with respect to given restrictive sets of properties.
 * If requested, will generate needed new property overrides, and/or restore values from reference.
 *
 * \param r_report_flags: If given,
 * will be set with flags matching actions taken by the function on \a ptr_local.
 *
 * \return True if _resulting_ \a ptr_local does match \a ptr_reference.
 */
bool RNA_struct_override_matches(Main *bmain,
                                 PointerRNA *ptr_local,
                                 PointerRNA *ptr_reference,
                                 const char *root_path,
                                 size_t root_path_len,
                                 IDOverrideLibrary *liboverride,
                                 eRNAOverrideMatch flags,
                                 eRNAOverrideMatchResult *r_report_flags);

/**
 * Store needed second operands into \a storage data-block
 * for differential override operations.
 */
bool RNA_struct_override_store(Main *bmain,
                               PointerRNA *ptr_local,
                               PointerRNA *ptr_reference,
                               PointerRNA *ptr_storage,
                               IDOverrideLibrary *liboverride);

enum eRNAOverrideApplyFlag {
  RNA_OVERRIDE_APPLY_FLAG_NOP = 0,
  /**
   * Hack to work around/fix older broken overrides: Do not apply override operations affecting ID
   * pointers properties, unless the destination original value (the one being overridden) is NULL.
   */
  RNA_OVERRIDE_APPLY_FLAG_IGNORE_ID_POINTERS = 1 << 0,

  /** Do not check for liboverrides needing resync with their linked reference data. */
  RNA_OVERRIDE_APPLY_FLAG_SKIP_RESYNC_CHECK = 1 << 1,

  /** Only perform restore operations. */
  RNA_OVERRIDE_APPLY_FLAG_RESTORE_ONLY = 1 << 2,
};

/**
 * Apply given \a override operations on \a id_ptr_dst, using \a id_ptr_src
 * (and \a id_ptr_storage for differential ops) as source.
 *
 * \note Although in theory `id_ptr_dst` etc. could be any type of RNA structure, currently they
 * are always ID ones. In any case, they are the roots of the `rna_path` of all override properties
 * in the given `liboverride` data.
 */
void RNA_struct_override_apply(Main *bmain,
                               PointerRNA *id_ptr_dst,
                               PointerRNA *id_ptr_src,
                               PointerRNA *id_ptr_storage,
                               IDOverrideLibrary *liboverride,
                               eRNAOverrideApplyFlag flag);

IDOverrideLibraryProperty *RNA_property_override_property_find(Main *bmain,
                                                               PointerRNA *ptr,
                                                               PropertyRNA *prop,
                                                               ID **r_owner_id);
IDOverrideLibraryProperty *RNA_property_override_property_get(Main *bmain,
                                                              PointerRNA *ptr,
                                                              PropertyRNA *prop,
                                                              bool *r_created);

IDOverrideLibraryPropertyOperation *RNA_property_override_property_operation_find(
    Main *bmain, PointerRNA *ptr, PropertyRNA *prop, int index, bool strict, bool *r_strict);
IDOverrideLibraryPropertyOperation *RNA_property_override_property_operation_get(Main *bmain,
                                                                                 PointerRNA *ptr,
                                                                                 PropertyRNA *prop,
                                                                                 short operation,
                                                                                 int index,
                                                                                 bool strict,
                                                                                 bool *r_strict,
                                                                                 bool *r_created);

eRNAOverrideStatus RNA_property_override_library_status(Main *bmain,
                                                        PointerRNA *ptr,
                                                        PropertyRNA *prop,
                                                        int index);

void RNA_struct_state_owner_set(const char *name);
const char *RNA_struct_state_owner_get();

#endif /* __RNA_ACCESS_H__ */

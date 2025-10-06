/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup RNA
 *
 * Functions used during preprocess and runtime, for defining the RNA.
 */

#include <float.h>
#include <inttypes.h>
#include <limits.h>

#include "DNA_listBase.h"

#include "RNA_types.hh"

#ifdef UNIT_TEST
#  define RNA_MAX_ARRAY_LENGTH 64
#else
#  define RNA_MAX_ARRAY_LENGTH 64
#endif

#define RNA_MAX_ARRAY_DIMENSION 3

/* Blender RNA */

struct Scene;

BlenderRNA *RNA_create();
void RNA_define_free(BlenderRNA *brna);
void RNA_free(BlenderRNA *brna);

/**
 * Tell the RNA maker to check whether the property exists in the matching DNA structure,
 *
 * When in DNA, RNA generates automatically the accessors code. Otherwise, you
 * have to give it explicit getters/setters/etc. By default, the RNA maker will
 * error if it cannot find the corresponding DNA properties; this is what can be
 * turned off with this function.
 *
 * This is used to generate RNA structs that do not (directly) match any DNA
 * data, passing `false` as parameter at the beginning of the struct definition,
 * and then calling it again at the end with `true` to restore default 'check
 * DNA' behavior.
 */
void RNA_define_verify_sdna(bool verify);
void RNA_define_animate_sdna(bool animate);
void RNA_define_fallback_property_update(int noteflag, const char *updatefunc);
/**
 * Properties defined when this is enabled are lib-overridable by default
 * (except for Pointer ones).
 */
void RNA_define_lib_overridable(bool make_overridable);

void RNA_init();
void RNA_bpy_exit();
void RNA_exit();

/* Struct */

/**
 * Struct Definition.
 */
StructRNA *RNA_def_struct_ptr(BlenderRNA *brna, const char *identifier, StructRNA *srnafrom);
StructRNA *RNA_def_struct(BlenderRNA *brna, const char *identifier, const char *from);
void RNA_def_struct_sdna(StructRNA *srna, const char *structname);
void RNA_def_struct_sdna_from(StructRNA *srna, const char *structname, const char *propname);
void RNA_def_struct_name_property(StructRNA *srna, PropertyRNA *prop);
void RNA_def_struct_nested(BlenderRNA *brna, StructRNA *srna, const char *structname);
void RNA_def_struct_flag(StructRNA *srna, int flag);
void RNA_def_struct_clear_flag(StructRNA *srna, int flag);
void RNA_def_struct_property_tags(StructRNA *srna, const EnumPropertyItem *prop_tag_defines);
void RNA_def_struct_refine_func(StructRNA *srna, const char *refine);
void RNA_def_struct_idprops_func(StructRNA *srna, const char *idproperties);
/**
 * Define the callback to access the struct's system IDProperty root.
 */
void RNA_def_struct_system_idprops_func(StructRNA *srna, const char *system_idproperties);
void RNA_def_struct_register_funcs(StructRNA *srna,
                                   const char *reg,
                                   const char *unreg,
                                   const char *instance);
/**
 * Return an allocated string for the RNA data-path:
 *
 * - Double quotes must be used for string access, e.g: `collection["%s"]`.
 * - Strings containing arbitrary characters must be escaped using #BLI_str_escape.
 *
 * Paths must be compatible with #RNA_path_resolve & related functions.
 */
void RNA_def_struct_path_func(StructRNA *srna, const char *path);
/**
 * Only used in one case when we name the struct for the purpose of useful error messages.
 */
void RNA_def_struct_identifier_no_struct_map(StructRNA *srna, const char *identifier);
void RNA_def_struct_identifier(BlenderRNA *brna, StructRNA *srna, const char *identifier);
void RNA_def_struct_ui_text(StructRNA *srna, const char *name, const char *description);
void RNA_def_struct_ui_icon(StructRNA *srna, int icon);
void RNA_struct_free_extension(StructRNA *srna, ExtensionRNA *rna_ext);
void RNA_struct_free(BlenderRNA *brna, StructRNA *srna);

void RNA_def_struct_translation_context(StructRNA *srna, const char *context);

/* Compact Property Definitions */

typedef void StructOrFunctionRNA;

PropertyRNA *RNA_def_boolean(StructOrFunctionRNA *cont,
                             const char *identifier,
                             bool default_value,
                             const char *ui_name,
                             const char *ui_description);
PropertyRNA *RNA_def_boolean_array(StructOrFunctionRNA *cont,
                                   const char *identifier,
                                   int len,
                                   const bool *default_value,
                                   const char *ui_name,
                                   const char *ui_description);
PropertyRNA *RNA_def_boolean_layer(StructOrFunctionRNA *cont,
                                   const char *identifier,
                                   int len,
                                   const bool *default_value,
                                   const char *ui_name,
                                   const char *ui_description);
PropertyRNA *RNA_def_boolean_layer_member(StructOrFunctionRNA *cont,
                                          const char *identifier,
                                          int len,
                                          const bool *default_value,
                                          const char *ui_name,
                                          const char *ui_description);
PropertyRNA *RNA_def_boolean_vector(StructOrFunctionRNA *cont,
                                    const char *identifier,
                                    int len,
                                    const bool *default_value,
                                    const char *ui_name,
                                    const char *ui_description);

PropertyRNA *RNA_def_int(StructOrFunctionRNA *cont,
                         const char *identifier,
                         int default_value,
                         int hardmin,
                         int hardmax,
                         const char *ui_name,
                         const char *ui_description,
                         int softmin,
                         int softmax);
PropertyRNA *RNA_def_int_vector(StructOrFunctionRNA *cont,
                                const char *identifier,
                                int len,
                                const int *default_value,
                                int hardmin,
                                int hardmax,
                                const char *ui_name,
                                const char *ui_description,
                                int softmin,
                                int softmax);
PropertyRNA *RNA_def_int_array(StructOrFunctionRNA *cont,
                               const char *identifier,
                               int len,
                               const int *default_value,
                               int hardmin,
                               int hardmax,
                               const char *ui_name,
                               const char *ui_description,
                               int softmin,
                               int softmax);

PropertyRNA *RNA_def_string(StructOrFunctionRNA *cont,
                            const char *identifier,
                            const char *default_value,
                            int maxlen,
                            const char *ui_name,
                            const char *ui_description);
PropertyRNA *RNA_def_string_file_path(StructOrFunctionRNA *cont,
                                      const char *identifier,
                                      const char *default_value,
                                      int maxlen,
                                      const char *ui_name,
                                      const char *ui_description);
PropertyRNA *RNA_def_string_dir_path(StructOrFunctionRNA *cont,
                                     const char *identifier,
                                     const char *default_value,
                                     int maxlen,
                                     const char *ui_name,
                                     const char *ui_description);
PropertyRNA *RNA_def_string_file_name(StructOrFunctionRNA *cont,
                                      const char *identifier,
                                      const char *default_value,
                                      int maxlen,
                                      const char *ui_name,
                                      const char *ui_description);

PropertyRNA *RNA_def_enum(StructOrFunctionRNA *cont,
                          const char *identifier,
                          const EnumPropertyItem *items,
                          int default_value,
                          const char *ui_name,
                          const char *ui_description);
/**
 * Same as #RNA_def_enum but sets #PROP_ENUM_FLAG before setting the default value.
 */
PropertyRNA *RNA_def_enum_flag(StructOrFunctionRNA *cont,
                               const char *identifier,
                               const EnumPropertyItem *items,
                               int default_value,
                               const char *ui_name,
                               const char *ui_description);
void RNA_def_enum_funcs(PropertyRNA *prop, EnumPropertyItemFunc itemfunc);

PropertyRNA *RNA_def_float(StructOrFunctionRNA *cont,
                           const char *identifier,
                           float default_value,
                           float hardmin,
                           float hardmax,
                           const char *ui_name,
                           const char *ui_description,
                           float softmin,
                           float softmax);
PropertyRNA *RNA_def_float_vector(StructOrFunctionRNA *cont,
                                  const char *identifier,
                                  int len,
                                  const float *default_value,
                                  float hardmin,
                                  float hardmax,
                                  const char *ui_name,
                                  const char *ui_description,
                                  float softmin,
                                  float softmax);
PropertyRNA *RNA_def_float_vector_xyz(StructOrFunctionRNA *cont,
                                      const char *identifier,
                                      int len,
                                      const float *default_value,
                                      float hardmin,
                                      float hardmax,
                                      const char *ui_name,
                                      const char *ui_description,
                                      float softmin,
                                      float softmax);
PropertyRNA *RNA_def_float_color(StructOrFunctionRNA *cont,
                                 const char *identifier,
                                 int len,
                                 const float *default_value,
                                 float hardmin,
                                 float hardmax,
                                 const char *ui_name,
                                 const char *ui_description,
                                 float softmin,
                                 float softmax);
PropertyRNA *RNA_def_float_matrix(StructOrFunctionRNA *cont,
                                  const char *identifier,
                                  int rows,
                                  int columns,
                                  const float *default_value,
                                  float hardmin,
                                  float hardmax,
                                  const char *ui_name,
                                  const char *ui_description,
                                  float softmin,
                                  float softmax);
PropertyRNA *RNA_def_float_translation(StructOrFunctionRNA *cont,
                                       const char *identifier,
                                       int len,
                                       const float *default_value,
                                       float hardmin,
                                       float hardmax,
                                       const char *ui_name,
                                       const char *ui_description,
                                       float softmin,
                                       float softmax);
PropertyRNA *RNA_def_float_rotation(StructOrFunctionRNA *cont,
                                    const char *identifier,
                                    int len,
                                    const float *default_value,
                                    float hardmin,
                                    float hardmax,
                                    const char *ui_name,
                                    const char *ui_description,
                                    float softmin,
                                    float softmax);
PropertyRNA *RNA_def_float_distance(StructOrFunctionRNA *cont,
                                    const char *identifier,
                                    float default_value,
                                    float hardmin,
                                    float hardmax,
                                    const char *ui_name,
                                    const char *ui_description,
                                    float softmin,
                                    float softmax);
PropertyRNA *RNA_def_float_array(StructOrFunctionRNA *cont,
                                 const char *identifier,
                                 int len,
                                 const float *default_value,
                                 float hardmin,
                                 float hardmax,
                                 const char *ui_name,
                                 const char *ui_description,
                                 float softmin,
                                 float softmax);

#if 0
PropertyRNA *RNA_def_float_dynamic_array(StructOrFunctionRNA *cont,
                                         const char *identifier,
                                         float hardmin,
                                         float hardmax,
                                         const char *ui_name,
                                         const char *ui_description,
                                         float softmin,
                                         float softmax,
                                         unsigned int dimension,
                                         unsigned short dim_size[]);
#endif

PropertyRNA *RNA_def_float_percentage(StructOrFunctionRNA *cont,
                                      const char *identifier,
                                      float default_value,
                                      float hardmin,
                                      float hardmax,
                                      const char *ui_name,
                                      const char *ui_description,
                                      float softmin,
                                      float softmax);
PropertyRNA *RNA_def_float_factor(StructOrFunctionRNA *cont,
                                  const char *identifier,
                                  float default_value,
                                  float hardmin,
                                  float hardmax,
                                  const char *ui_name,
                                  const char *ui_description,
                                  float softmin,
                                  float softmax);

PropertyRNA *RNA_def_pointer(StructOrFunctionRNA *cont,
                             const char *identifier,
                             const char *type,
                             const char *ui_name,
                             const char *ui_description);
PropertyRNA *RNA_def_pointer_runtime(StructOrFunctionRNA *cont,
                                     const char *identifier,
                                     StructRNA *type,
                                     const char *ui_name,
                                     const char *ui_description);

PropertyRNA *RNA_def_collection(StructOrFunctionRNA *cont,
                                const char *identifier,
                                const char *type,
                                const char *ui_name,
                                const char *ui_description);
PropertyRNA *RNA_def_collection_runtime(StructOrFunctionRNA *cont,
                                        const char *identifier,
                                        StructRNA *type,
                                        const char *ui_name,
                                        const char *ui_description);

/* Extended Property Definitions */

PropertyRNA *RNA_def_property(StructOrFunctionRNA *cont,
                              const char *identifier,
                              int type,
                              int subtype);

/**
 * Define a boolean property controlling one or more bitflags in the DNA member.
 *
 * \note This can be combined to a call to #RNA_def_property_array on the same property, in case
 * the wrapped DNA member is an array of integers. Do not confuse it with defining a RNA boolean
 * array property using a single DNA member as a bitset (use
 * #RNA_def_property_boolean_bitset_array_sdna for this).
 */
void RNA_def_property_boolean_sdna(PropertyRNA *prop,
                                   const char *structname,
                                   const char *propname,
                                   int64_t booleanbit);
void RNA_def_property_boolean_negative_sdna(PropertyRNA *prop,
                                            const char *structname,
                                            const char *propname,
                                            int64_t booleanbit);
/**
 * Used to define an array of boolean values using a single int/char/etc. member of a DNA struct
 * (aka 'bitset array').
 *
 * The #booleanbit value should strictly have a single bit enabled (so typically come from a
 * bit-shift expression like `1 << 0`), and be strictly positive (i.e. the left-most bit in the
 * valid range should not be used). Multi-bit values are not valid. It will be used as first bit
 * for the `0`-indexed item of the array.
 *
 * The maximum #len depends on the type of the DNA member, and the #booleanbit value. The left-most
 * bit is not usable (because bit-shift operations over signed negative values are typically
 * 'arithmetic', and not 'bitwise', in C++). So e.g. `31` for an `int32_t` with a `booleanbit`
 * value of `1 << 0`, and so on.
 */
void RNA_def_property_boolean_bitset_array_sdna(
    PropertyRNA *prop, const char *structname, const char *propname, int64_t booleanbit, int len);
void RNA_def_property_int_sdna(PropertyRNA *prop, const char *structname, const char *propname);
void RNA_def_property_float_sdna(PropertyRNA *prop, const char *structname, const char *propname);
void RNA_def_property_string_sdna(PropertyRNA *prop, const char *structname, const char *propname);
/**
 * Define a regular, non-bitflags-aware enum property.
 *
 * The key aspect of using this call is that when setting the property, the whole underlying DNA
 * property will be overwritten.
 *
 * This should typically be used for:
 *   - Non-bitflags enums.
 *   - Bitflags enums using a callback function to define their items.
 *
 * \note This behavior is the only one available for runtime-defined enum properties. C++-defined
 * runtime properties can work around this limitation by defining their own setter to handle the
 * bitmasking.
 *
 * \note This is not related to the #PROP_ENUM_FLAG property option.
 */
void RNA_def_property_enum_sdna(PropertyRNA *prop, const char *structname, const char *propname);
/**
 * Define a bitflags-aware enum property.
 *
 * The key aspect of using this call is that when setting the property, a bitmask is used to avoid
 * overwriting unrelated bits in the underlying DNA property.
 *
 * The bitmask is computed from the values defined in the static 'items' array defined by
 * `RNA_def_property_enum_items`, so it won't be valid in case an `items` callback function is
 * defined, that may use bitflags outside of that statically computed bitmask.
 *
 * This should typically be used for bitflags enums. It is especially critical when several
 * bitflags enums and/or bitflag booleans (defined with `RNA_def_property_boolean_sdna` or
 * `RNA_def_property_boolean_negative_sdna`) share the same DNA variable. Otherwise, setting one
 * RNA property may affect unrelated bitflags.
 *
 * \note This is not related to the #PROP_ENUM_FLAG property option.
 */
void RNA_def_property_enum_bitflag_sdna(PropertyRNA *prop,
                                        const char *structname,
                                        const char *propname);
void RNA_def_property_pointer_sdna(PropertyRNA *prop,
                                   const char *structname,
                                   const char *propname);
void RNA_def_property_collection_sdna(PropertyRNA *prop,
                                      const char *structname,
                                      const char *propname,
                                      const char *lengthpropname);

void RNA_def_property_flag(PropertyRNA *prop, PropertyFlag flag);
void RNA_def_property_clear_flag(PropertyRNA *prop, PropertyFlag flag);
void RNA_def_property_override_flag(PropertyRNA *prop, PropertyOverrideFlag flag);
void RNA_def_property_override_clear_flag(PropertyRNA *prop, PropertyOverrideFlag flag);

/**
 * In some cases showing properties in the outliner crashes.
 * It's a bug that occurs when accessing a value re-allocates
 * memory which may already be referenced by other RNA.
 * See: #145877.
 */
void RNA_def_property_flag_hide_from_ui_workaround(PropertyRNA *prop);

/**
 * Add the property-tags passed as \a tags to \a prop (if valid).
 *
 * \note Multiple tags can be set by passing them within \a tags (using bit-flags).
 * \note Doesn't do any type-checking with the tags defined in the parent #StructRNA
 * of \a prop. This should be done before (e.g. see #WM_operatortype_prop_tag).
 */
void RNA_def_property_tags(PropertyRNA *prop, int tags);
void RNA_def_property_subtype(PropertyRNA *prop, PropertySubType subtype);
void RNA_def_property_array(PropertyRNA *prop, int length);
void RNA_def_property_multi_array(PropertyRNA *prop, int dimension, const int length[]);
void RNA_def_property_range(PropertyRNA *prop, double min, double max);

/**
 * \param item: An array of enum properties terminated by null members.
 * \warning take care not to reference stack memory as the reference to `item` is held by `prop`.
 */
void RNA_def_property_enum_items(PropertyRNA *prop, const EnumPropertyItem *item);
void RNA_def_property_enum_native_type(PropertyRNA *prop, const char *native_enum_type);
void RNA_def_property_string_maxlength(PropertyRNA *prop, int maxlength);
void RNA_def_property_struct_type(PropertyRNA *prop, const char *type);
void RNA_def_property_struct_runtime(StructOrFunctionRNA *cont,
                                     PropertyRNA *prop,
                                     StructRNA *type);

void RNA_def_property_boolean_default(PropertyRNA *prop, bool value);
void RNA_def_property_boolean_array_default(PropertyRNA *prop, const bool *array);
void RNA_def_property_int_default(PropertyRNA *prop, int value);
void RNA_def_property_int_array_default(PropertyRNA *prop, const int *array);
void RNA_def_property_float_default(PropertyRNA *prop, float value);
/**
 * Array must remain valid after this function finishes.
 */
void RNA_def_property_float_array_default(PropertyRNA *prop, const float *array);
void RNA_def_property_enum_default(PropertyRNA *prop, int value);
void RNA_def_property_string_default(PropertyRNA *prop, const char *value);

void RNA_def_property_ui_text(PropertyRNA *prop, const char *name, const char *description);
void RNA_def_property_ui_name_func(PropertyRNA *prop, const char *name_func);

void RNA_def_property_deprecated(PropertyRNA *prop,
                                 const char *note,
                                 short version,
                                 short removal_version);

/**
 * The values hare are a little confusing:
 *
 * \param step: Used as the value to increase/decrease when clicking on number buttons,
 * as well as scaling mouse input for click-dragging number buttons.
 * For floats this is (step * UI_PRECISION_FLOAT_SCALE), why? - nobody knows.
 * For ints, whole values are used.
 *
 * \param precision: The number of zeros to show
 * (as a whole number - common range is 1 - 6), see UI_PRECISION_FLOAT_MAX
 */
void RNA_def_property_ui_range(
    PropertyRNA *prop, double min, double max, double step, int precision);
void RNA_def_property_ui_scale_type(PropertyRNA *prop, PropertyScaleType ui_scale_type);
void RNA_def_property_ui_icon(PropertyRNA *prop, int icon, int consecutive);

void RNA_def_property_update(PropertyRNA *prop, int noteflag, const char *updatefunc);
void RNA_def_property_editable_func(PropertyRNA *prop, const char *editable);
void RNA_def_property_editable_array_func(PropertyRNA *prop, const char *editable);

/**
 * Set custom callbacks for override operations handling.
 *
 * \note \a diff callback will also be used by RNA comparison/equality functions.
 */
void RNA_def_property_override_funcs(PropertyRNA *prop,
                                     const char *diff,
                                     const char *store,
                                     const char *apply);

using RNAPropertyUpdateFunc = void (*)(Main *, Scene *, PointerRNA *);
using RNAPropertyUpdateFuncWithContextAndProperty = void (*)(bContext *C,
                                                             PointerRNA *ptr,
                                                             PropertyRNA *prop);

void RNA_def_property_update_runtime(PropertyRNA *prop, RNAPropertyUpdateFunc func);
void RNA_def_property_update_runtime_with_context_and_property(
    PropertyRNA *prop, RNAPropertyUpdateFuncWithContextAndProperty func);
void RNA_def_property_update_notifier(PropertyRNA *prop, int noteflag);
void RNA_def_property_poll_runtime(PropertyRNA *prop, const void *func);

void RNA_def_property_dynamic_array_funcs(PropertyRNA *prop, const char *getlength);
void RNA_def_property_boolean_funcs(PropertyRNA *prop, const char *get, const char *set);
void RNA_def_property_int_funcs(PropertyRNA *prop,
                                const char *get,
                                const char *set,
                                const char *range);
void RNA_def_property_float_funcs(PropertyRNA *prop,
                                  const char *get,
                                  const char *set,
                                  const char *range);
void RNA_def_property_enum_funcs(PropertyRNA *prop,
                                 const char *get,
                                 const char *set,
                                 const char *item);
void RNA_def_property_string_funcs(PropertyRNA *prop,
                                   const char *get,
                                   const char *length,
                                   const char *set);
void RNA_def_property_string_search_func(PropertyRNA *prop,
                                         const char *search,
                                         eStringPropertySearchFlag search_flag);
void RNA_def_property_string_filepath_filter_func(PropertyRNA *prop, const char *filter);
void RNA_def_property_pointer_funcs(
    PropertyRNA *prop, const char *get, const char *set, const char *type_fn, const char *poll);
void RNA_def_property_collection_funcs(PropertyRNA *prop,
                                       const char *begin,
                                       const char *next,
                                       const char *end,
                                       const char *get,
                                       const char *length,
                                       const char *lookupint,
                                       const char *lookupstring,
                                       const char *assignint);

void RNA_def_property_float_default_func(PropertyRNA *prop, const char *get_default);
void RNA_def_property_int_default_func(PropertyRNA *prop, const char *get_default);
void RNA_def_property_boolean_default_func(PropertyRNA *prop, const char *get_default);
void RNA_def_property_enum_default_func(PropertyRNA *prop, const char *get_default);

void RNA_def_property_srna(PropertyRNA *prop, const char *type);
void RNA_def_py_data(PropertyRNA *prop, void *py_data);

/* API to define callbacks for runtime-defined properties (mainly for Operators, and from the
 * Python `bpy.props` API).
 *
 * These expect 'extended' versions of the callbacks, with both the StructRNA owner and the
 * PropertyRNA as first arguments.
 *
 * The 'Transform' ones allow to add a transform step (applied after getting, or before setting the
 * value), which only modifies the value, but does not handle actual storage. Currently only used
 * by `bpy`, more details in the documentation of #BPyPropStore.
 */
void RNA_def_property_boolean_funcs_runtime(PropertyRNA *prop,
                                            BooleanPropertyGetFunc getfunc,
                                            BooleanPropertySetFunc setfunc,
                                            BooleanPropertyGetTransformFunc get_transform_fn,
                                            BooleanPropertySetTransformFunc set_transform_fn);
void RNA_def_property_boolean_array_funcs_runtime(
    PropertyRNA *prop,
    BooleanArrayPropertyGetFunc getfunc,
    BooleanArrayPropertySetFunc setfunc,
    BooleanArrayPropertyGetTransformFunc get_transform_fn,
    BooleanArrayPropertySetTransformFunc set_transform_fn);
void RNA_def_property_int_funcs_runtime(PropertyRNA *prop,
                                        IntPropertyGetFunc getfunc,
                                        IntPropertySetFunc setfunc,
                                        IntPropertyRangeFunc rangefunc,
                                        IntPropertyGetTransformFunc get_transform_fn,
                                        IntPropertySetTransformFunc set_transform_fn);
void RNA_def_property_int_array_funcs_runtime(PropertyRNA *prop,
                                              IntArrayPropertyGetFunc getfunc,
                                              IntArrayPropertySetFunc setfunc,
                                              IntPropertyRangeFunc rangefunc,
                                              IntArrayPropertyGetTransformFunc get_transform_fn,
                                              IntArrayPropertySetTransformFunc set_transform_fn);
void RNA_def_property_float_funcs_runtime(PropertyRNA *prop,
                                          FloatPropertyGetFunc getfunc,
                                          FloatPropertySetFunc setfunc,
                                          FloatPropertyRangeFunc rangefunc,
                                          FloatPropertyGetTransformFunc get_transform_fn,
                                          FloatPropertySetTransformFunc set_transform_fn);
void RNA_def_property_float_array_funcs_runtime(
    PropertyRNA *prop,
    FloatArrayPropertyGetFunc getfunc,
    FloatArrayPropertySetFunc setfunc,
    FloatPropertyRangeFunc rangefunc,
    FloatArrayPropertyGetTransformFunc get_transform_fn,
    FloatArrayPropertySetTransformFunc set_transform_fn);
void RNA_def_property_enum_funcs_runtime(PropertyRNA *prop,
                                         EnumPropertyGetFunc getfunc,
                                         EnumPropertySetFunc setfunc,
                                         EnumPropertyItemFunc itemfunc,
                                         EnumPropertyGetTransformFunc get_transform_fn,
                                         EnumPropertySetTransformFunc set_transform_fn);
void RNA_def_property_string_funcs_runtime(PropertyRNA *prop,
                                           StringPropertyGetFunc getfunc,
                                           StringPropertyLengthFunc lengthfunc,
                                           StringPropertySetFunc setfunc,
                                           StringPropertyGetTransformFunc get_transform_fn,
                                           StringPropertySetTransformFunc set_transform_fn);
void RNA_def_property_string_search_func_runtime(PropertyRNA *prop,
                                                 StringPropertySearchFunc search_fn,
                                                 eStringPropertySearchFlag search_flag);

void RNA_def_property_translation_context(PropertyRNA *prop, const char *context);

/* Function */

FunctionRNA *RNA_def_function(StructRNA *srna, const char *identifier, const char *call);
FunctionRNA *RNA_def_function_runtime(StructRNA *srna, const char *identifier, CallFunc call);
/**
 * C return value only! multiple RNA returns can be done with #RNA_def_function_output.
 */
void RNA_def_function_return(FunctionRNA *func, PropertyRNA *ret);
void RNA_def_function_output(FunctionRNA *func, PropertyRNA *ret);
void RNA_def_function_flag(FunctionRNA *func, int flag);
void RNA_def_function_ui_description(FunctionRNA *func, const char *description);

void RNA_def_parameter_flags(PropertyRNA *prop,
                             PropertyFlag flag_property,
                             ParameterFlag flag_parameter);
void RNA_def_parameter_clear_flags(PropertyRNA *prop,
                                   PropertyFlag flag_property,
                                   ParameterFlag flag_parameter);
void RNA_def_property_path_template_type(PropertyRNA *prop,
                                         PropertyPathTemplateType path_template_type);

/* Dynamic Enums
 * strings are not freed, assumed pointing to static location. */

void RNA_enum_item_add(EnumPropertyItem **items, int *totitem, const EnumPropertyItem *item);
void RNA_enum_item_add_separator(EnumPropertyItem **items, int *totitem);
void RNA_enum_items_add(EnumPropertyItem **items, int *totitem, const EnumPropertyItem *item);
void RNA_enum_items_add_value(EnumPropertyItem **items,
                              int *totitem,
                              const EnumPropertyItem *item,
                              int value);
void RNA_enum_item_end(EnumPropertyItem **items, int *totitem);

/* Memory management */

void RNA_def_struct_duplicate_pointers(BlenderRNA *brna, StructRNA *srna);
void RNA_def_struct_free_pointers(BlenderRNA *brna, StructRNA *srna);
void RNA_def_func_duplicate_pointers(FunctionRNA *func);
void RNA_def_func_free_pointers(FunctionRNA *func);
void RNA_def_property_duplicate_pointers(StructOrFunctionRNA *cont_, PropertyRNA *prop);
void RNA_def_property_free_pointers(PropertyRNA *prop);
int RNA_def_property_free_identifier(StructOrFunctionRNA *cont_, const char *identifier);

int RNA_def_property_free_identifier_deferred_prepare(StructOrFunctionRNA *cont_,
                                                      const char *identifier,
                                                      void **handle);
void RNA_def_property_free_identifier_deferred_finish(StructOrFunctionRNA *cont_, void *handle);

void RNA_def_property_free_pointers_set_py_data_callback(
    void (*py_data_clear_fn)(PropertyRNA *prop));

/* Utilities. */

const char *RNA_property_typename(PropertyType type);
#define IS_DNATYPE_FLOAT_COMPAT(_str) (strcmp(_str, "float") == 0 || strcmp(_str, "double") == 0)
#define IS_DNATYPE_INT_COMPAT(_str) \
  (strcmp(_str, "int") == 0 || strcmp(_str, "short") == 0 || strcmp(_str, "char") == 0 || \
   strcmp(_str, "uchar") == 0 || strcmp(_str, "ushort") == 0 || strcmp(_str, "int8_t") == 0)
#define IS_DNATYPE_BOOLEAN_COMPAT(_str) \
  (IS_DNATYPE_INT_COMPAT(_str) || strcmp(_str, "int64_t") == 0 || strcmp(_str, "uint64_t") == 0)

void RNA_identifier_sanitize(char *identifier, int property);

/* Common arguments for length. */

extern const int rna_matrix_dimsize_3x3[];
extern const int rna_matrix_dimsize_4x4[];
extern const int rna_matrix_dimsize_4x2[];

/* Common arguments for defaults. */

extern const float rna_default_axis_angle[4];
extern const float rna_default_quaternion[4];
extern const float rna_default_scale_3d[3];

/** Maximum size for dynamic defined type descriptors, this value is arbitrary. */
#define RNA_DYN_DESCR_MAX 1024

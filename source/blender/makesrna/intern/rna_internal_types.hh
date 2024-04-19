/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#pragma once

#include <optional>
#include <string>

#include "DNA_listBase.h"

#include "RNA_access.hh"
#include "RNA_types.hh"

struct BlenderRNA;
struct CollectionPropertyIterator;
struct ContainerRNA;
struct FunctionRNA;
struct GHash;
struct IDOverrideLibrary;
struct IDOverrideLibraryPropertyOperation;
struct IDProperty;
struct Main;
struct PointerRNA;
struct PropertyRNA;
struct ReportList;
struct Scene;
struct StructRNA;
struct bContext;

/* Function Callbacks */

/**
 * Update callback for an RNA property.
 *
 * \note This is NOT called automatically when writing into the property, it needs to be called
 * manually (through #RNA_property_update or #RNA_property_update_main) when needed.
 *
 * \param bmain: the Main data-base to which `ptr` data belongs.
 * \param active_scene: The current active scene (may be NULL in some cases).
 * \param ptr: The RNA pointer data to update.
 */
using UpdateFunc = void (*)(Main *bmain, Scene *active_scene, PointerRNA *ptr);
using ContextPropUpdateFunc = void (*)(bContext *C, PointerRNA *ptr, PropertyRNA *prop);
using ContextUpdateFunc = void (*)(bContext *C, PointerRNA *ptr);

using EditableFunc = int (*)(const PointerRNA *ptr, const char **r_info);
using ItemEditableFunc = int (*)(const PointerRNA *ptr, int index);
using IDPropertiesFunc = IDProperty **(*)(PointerRNA *ptr);
using StructRefineFunc = StructRNA *(*)(PointerRNA *ptr);
using StructPathFunc = std::optional<std::string> (*)(const PointerRNA *ptr);

using PropArrayLengthGetFunc = int (*)(const PointerRNA *ptr, int length[RNA_MAX_ARRAY_DIMENSION]);
using PropBooleanGetFunc = bool (*)(PointerRNA *ptr);
using PropBooleanSetFunc = void (*)(PointerRNA *ptr, bool value);
using PropBooleanArrayGetFunc = void (*)(PointerRNA *ptr, bool *values);
using PropBooleanArraySetFunc = void (*)(PointerRNA *ptr, const bool *values);
using PropIntGetFunc = int (*)(PointerRNA *ptr);
using PropIntSetFunc = void (*)(PointerRNA *ptr, int value);
using PropIntArrayGetFunc = void (*)(PointerRNA *ptr, int *values);
using PropIntArraySetFunc = void (*)(PointerRNA *ptr, const int *values);
using PropIntRangeFunc = void (*)(PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax);
using PropFloatGetFunc = float (*)(PointerRNA *ptr);
using PropFloatSetFunc = void (*)(PointerRNA *ptr, float value);
using PropFloatArrayGetFunc = void (*)(PointerRNA *ptr, float *values);
using PropFloatArraySetFunc = void (*)(PointerRNA *ptr, const float *values);
using PropFloatRangeFunc =
    void (*)(PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax);
using PropStringGetFunc = void (*)(PointerRNA *ptr, char *value);
using PropStringLengthFunc = int (*)(PointerRNA *ptr);
using PropStringSetFunc = void (*)(PointerRNA *ptr, const char *value);
using PropEnumGetFunc = int (*)(PointerRNA *ptr);
using PropEnumSetFunc = void (*)(PointerRNA *ptr, int value);
using PropEnumItemFunc = const EnumPropertyItem *(*)(bContext *C,
                                                     PointerRNA *ptr,
                                                     PropertyRNA *prop,
                                                     bool *r_free);
using PropPointerGetFunc = PointerRNA (*)(PointerRNA *ptr);
using PropPointerTypeFunc = StructRNA *(*)(PointerRNA *ptr);
using PropPointerSetFunc = void (*)(PointerRNA *ptr, const PointerRNA value, ReportList *reports);
using PropPointerPollFunc = bool (*)(PointerRNA *ptr, const PointerRNA value);
using PropPointerPollFuncPy = bool (*)(PointerRNA *ptr,
                                       const PointerRNA value,
                                       const PropertyRNA *prop);
using PropCollectionBeginFunc = void (*)(CollectionPropertyIterator *iter, PointerRNA *ptr);
using PropCollectionNextFunc = void (*)(CollectionPropertyIterator *iter);
using PropCollectionEndFunc = void (*)(CollectionPropertyIterator *iter);
using PropCollectionGetFunc = PointerRNA (*)(CollectionPropertyIterator *iter);
using PropCollectionLengthFunc = int (*)(PointerRNA *ptr);
using PropCollectionLookupIntFunc = bool (*)(PointerRNA *ptr, int key, PointerRNA *r_ptr);
using PropCollectionLookupStringFunc = bool (*)(PointerRNA *ptr,
                                                const char *key,
                                                PointerRNA *r_ptr);
using PropCollectionAssignIntFunc = bool (*)(PointerRNA *ptr,
                                             int key,
                                             const PointerRNA *assign_ptr);

/* extended versions with PropertyRNA argument */
using PropBooleanGetFuncEx = bool (*)(PointerRNA *ptr, PropertyRNA *prop);
using PropBooleanSetFuncEx = void (*)(PointerRNA *ptr, PropertyRNA *prop, bool value);
using PropBooleanArrayGetFuncEx = void (*)(PointerRNA *ptr, PropertyRNA *prop, bool *values);
using PropBooleanArraySetFuncEx = void (*)(PointerRNA *ptr, PropertyRNA *prop, const bool *values);
using PropIntGetFuncEx = int (*)(PointerRNA *ptr, PropertyRNA *prop);
using PropIntSetFuncEx = void (*)(PointerRNA *ptr, PropertyRNA *prop, int value);
using PropIntArrayGetFuncEx = void (*)(PointerRNA *ptr, PropertyRNA *prop, int *values);
using PropIntArraySetFuncEx = void (*)(PointerRNA *ptr, PropertyRNA *prop, const int *values);
using PropIntRangeFuncEx =
    void (*)(PointerRNA *ptr, PropertyRNA *prop, int *min, int *max, int *softmin, int *softmax);
using PropFloatGetFuncEx = float (*)(PointerRNA *ptr, PropertyRNA *prop);
using PropFloatSetFuncEx = void (*)(PointerRNA *ptr, PropertyRNA *prop, float value);
using PropFloatArrayGetFuncEx = void (*)(PointerRNA *ptr, PropertyRNA *prop, float *values);
using PropFloatArraySetFuncEx = void (*)(PointerRNA *ptr, PropertyRNA *prop, const float *values);
using PropFloatRangeFuncEx = void (*)(
    PointerRNA *ptr, PropertyRNA *prop, float *min, float *max, float *softmin, float *softmax);
using PropStringGetFuncEx = void (*)(PointerRNA *ptr, PropertyRNA *prop, char *value);
using PropStringLengthFuncEx = int (*)(PointerRNA *ptr, PropertyRNA *prop);
using PropStringSetFuncEx = void (*)(PointerRNA *ptr, PropertyRNA *prop, const char *value);
using PropEnumGetFuncEx = int (*)(PointerRNA *ptr, PropertyRNA *prop);
using PropEnumSetFuncEx = void (*)(PointerRNA *ptr, PropertyRNA *prop, int value);

/* Handling override operations, and also comparison. */

/** Structure storing all needed data to process all three kinds of RNA properties. */
struct PropertyRNAOrID {
  PointerRNA ptr;

  /**
   * The PropertyRNA passed as parameter, used to generate that structure's content:
   * - Static RNA: The RNA property (same as `rnaprop`), never NULL.
   * - Runtime RNA: The RNA property (same as `rnaprop`), never NULL.
   * - IDProperty: The IDProperty, never NULL.
   */
  PropertyRNA *rawprop;
  /**
   * The real RNA property of this property, never NULL:
   * - Static RNA: The rna property, also gives direct access to the data (from any matching
   *               PointerRNA).
   * - Runtime RNA: The rna property, does not directly gives access to the data.
   * - IDProperty: The generic PropertyRNA matching its type.
   */
  PropertyRNA *rnaprop;
  /**
   * The IDProperty storing the data of this property, may be NULL:
   * - Static RNA: Always NULL.
   * - Runtime RNA: The IDProperty storing the data of that property, may be NULL if never set yet.
   * - IDProperty: The IDProperty, never NULL.
   */
  IDProperty *idprop;
  /** The name of the property. */
  const char *identifier;

  /**
   * Whether this property is a 'pure' IDProperty or not.
   *
   * \note Mutually exclusive with #is_rna_storage_idprop.
   */
  bool is_idprop;
  /**
   * Whether this property is defined as a RNA one, but uses an IDProperty to store its value (aka
   * python-defined runtine RNA properties).
   *
   * \note: In that case, the IDProperty itself may very well not exist (yet), when it has never
   * been set.
   *
   * \note Mutually exclusive with #is_idprop.
   */
  bool is_rna_storage_idprop;
  /**
   * For runtime RNA properties (i.e. when #is_rna_storage_idprop is true), whether it is set,
   * defined, or not.
   *
   * \warning This DOES take into account the `IDP_FLAG_GHOST` flag, i.e. it matches result of
   * `RNA_property_is_set`.
   */
  bool is_set;

  bool is_array;
  uint array_len;
};

/**
 * If \a liboverride is NULL, merely do comparison between prop_a and prop_b,
 * following comparison mode given.
 * If \a liboverride and \a rna_path are not NULL, it will add a new override operation for
 * overridable properties that differ and have not yet been overridden
 * (and set accordingly \a r_override_changed if given).
 *
 * \note \a liboverride and \a rna_path may be NULL pointers.
 */
struct RNAPropertyOverrideDiffContext {
  /** General diffing parameters. */

  /* Using #PropertyRNAOrID for properties info here allows to cover all three cases ('real' RNA
   * properties, 'runtime' RNA properties created from python and stored in IDPropertoies, and
   * 'pure' IDProperties).
   *
   * This is necessary, because we cannot perform 'set/unset' checks on resolved properties
   * (unset IDProperties would merely be nullptr then). */
  PropertyRNAOrID *prop_a = nullptr;
  PropertyRNAOrID *prop_b = nullptr;

  eRNACompareMode mode = RNA_EQ_COMPARE;

  /** LibOverride specific parameters. */
  IDOverrideLibrary *liboverride = nullptr;
  const char *rna_path = nullptr;
  size_t rna_path_len = 0;
  eRNAOverrideMatch liboverride_flags = eRNAOverrideMatch(0);

  /** Results. */

  /** `0` is matching, `-1` if `prop_a < prop_b`, `1` if `prop_a > prop_b`. Note that for
   * unquantifiable properties (e.g. pointers or collections), return value should be interpreted
   * as a boolean (false == matching, true == not matching). */
  int comparison = 0;
  /** Additional flags reporting potential actions taken by the function (e.g. resetting forbidden
   * overrides to their reference value). */
  eRNAOverrideMatchResult report_flag = eRNAOverrideMatchResult(0);
};
using RNAPropOverrideDiff = void (*)(Main *bmain, RNAPropertyOverrideDiffContext &rnadiff_ctx);

/**
 * Only used for differential override (add, sub, etc.).
 * Store into storage the value needed to transform reference's value into local's value.
 *
 * \note Given PropertyRNA are final (in case of IDProps...).
 * \note In non-array cases, \a len values are 0.
 * \note Might change given override operation (e.g. change 'add' one into 'sub'),
 * in case computed storage value is out of range
 * (or even change it to basic 'set' operation if nothing else works).
 */
using RNAPropOverrideStore = bool (*)(Main *bmain,
                                      PointerRNA *ptr_local,
                                      PointerRNA *ptr_reference,
                                      PointerRNA *ptr_storage,
                                      PropertyRNA *prop_local,
                                      PropertyRNA *prop_reference,
                                      PropertyRNA *prop_storage,
                                      int len_local,
                                      int len_reference,
                                      int len_storage,
                                      IDOverrideLibraryPropertyOperation *opop);

/**
 * Apply given override operation from src to dst (using value from storage as second operand
 * for differential operations).
 *
 * \return `true` if given operation is successfully applied to given data, false otherwise.
 *
 * \note Given PropertyRNA are final, fully resolved (in case of IDProps...).
 * \note In non-array cases, \a len values are 0.
 * \note `_storage` data is currently unused.
 */
struct RNAPropertyOverrideApplyContext {
  eRNAOverrideApplyFlag flag = RNA_OVERRIDE_APPLY_FLAG_NOP;
  bool do_insert = false;

  /** Main RNA data and property pointers. */
  PointerRNA ptr_dst = {0};
  PointerRNA ptr_src = {0};
  PointerRNA ptr_storage = {0};
  PropertyRNA *prop_dst = nullptr;
  PropertyRNA *prop_src = nullptr;
  PropertyRNA *prop_storage = nullptr;

  /** Length, for array properties. */
  int len_dst = 0;
  int len_src = 0;
  int len_storage = 0;

  /** Items, for RNA collections. */
  PointerRNA ptr_item_dst = {0};
  PointerRNA ptr_item_src = {0};
  PointerRNA ptr_item_storage = {0};

  /** LibOverride data. */
  IDOverrideLibrary *liboverride = nullptr;
  IDOverrideLibraryProperty *liboverride_property = nullptr;
  IDOverrideLibraryPropertyOperation *liboverride_operation = nullptr;

  /* TODO: Add more refined/descriptive result report? */
};
using RNAPropOverrideApply = bool (*)(Main *bmain, RNAPropertyOverrideApplyContext &rnaapply_ctx);

/* Container - generic abstracted container of RNA properties */
struct ContainerRNA {
  void *next, *prev;

  struct GHash *prophash;
  ListBase properties;
};

struct FunctionRNA {
  /* structs are containers of properties */
  ContainerRNA cont;

  /* unique identifier, keep after 'cont' */
  const char *identifier;
  /* various options */
  int flag;

  /* single line description, displayed in the tooltip for example */
  const char *description;

  /* callback to execute the function */
  CallFunc call;

  /* parameter for the return value
   * NOTE: this is only the C return value, rna functions can have multiple return values. */
  PropertyRNA *c_ret;
};

struct PropertyRNA {
  PropertyRNA *next, *prev;

  /* magic bytes to distinguish with IDProperty */
  int magic;

  /* unique identifier */
  const char *identifier;
  /* various options */
  int flag;
  /* various override options */
  int flag_override;
  /* Function parameters flags. */
  short flag_parameter;
  /* Internal ("private") flags. */
  short flag_internal;
  /* The subset of StructRNA.prop_tag_defines values that applies to this property. */
  short tags;

  /* user readable name */
  const char *name;
  /* single line description, displayed in the tooltip for example */
  const char *description;
  /* icon ID */
  int icon;
  /* context for translation */
  const char *translation_context;

  /* property type as it appears to the outside */
  PropertyType type;
  /* subtype, 'interpretation' of the property */
  PropertySubType subtype;
  /* if non-NULL, overrides arraylength. Must not return 0? */
  PropArrayLengthGetFunc getlength;
  /* dimension of array */
  unsigned int arraydimension;
  /* Array lengths for all dimensions (when `arraydimension > 0`). */
  unsigned int arraylength[RNA_MAX_ARRAY_DIMENSION];
  unsigned int totarraylength;

  /* callback for updates on change */
  UpdateFunc update;
  int noteflag;

  /* Callback for testing if editable. Its r_info parameter can be used to
   * return info on editable state that might be shown to user. E.g. tooltips
   * of disabled buttons can show reason why button is disabled using this. */
  EditableFunc editable;
  /* callback for testing if array-item editable (if applicable) */
  ItemEditableFunc itemeditable;

  /* Override handling callbacks (diff is also used for comparison). */
  RNAPropOverrideDiff override_diff;
  RNAPropOverrideStore override_store;
  RNAPropOverrideApply override_apply;

  /* raw access */
  int rawoffset;
  RawPropertyType rawtype;

  /* This is used for accessing props/functions of this property
   * any property can have this but should only be used for collections and arrays
   * since python will convert int/bool/pointer's */
  StructRNA *srna; /* attributes attached directly to this collection */

  /* python handle to hold all callbacks
   * (in a pointer array at the moment, may later be a tuple) */
  void *py_data;
};

/* internal flags WARNING! 16bits only! */
enum PropertyFlagIntern {
  PROP_INTERN_BUILTIN = (1 << 0),
  PROP_INTERN_RUNTIME = (1 << 1),
  PROP_INTERN_RAW_ACCESS = (1 << 2),
  PROP_INTERN_RAW_ARRAY = (1 << 3),
  PROP_INTERN_FREE_POINTERS = (1 << 4),
  /* Negative mirror of PROP_PTR_NO_OWNERSHIP, used to prevent automatically setting that one in
   * makesrna when pointer is an ID... */
  PROP_INTERN_PTR_OWNERSHIP_FORCED = (1 << 5),
};

/* Property Types */

struct BoolPropertyRNA {
  PropertyRNA property;

  PropBooleanGetFunc get;
  PropBooleanSetFunc set;
  PropBooleanArrayGetFunc getarray;
  PropBooleanArraySetFunc setarray;

  PropBooleanGetFuncEx get_ex;
  PropBooleanSetFuncEx set_ex;
  PropBooleanArrayGetFuncEx getarray_ex;
  PropBooleanArraySetFuncEx setarray_ex;

  bool defaultvalue;
  const bool *defaultarray;
};

struct IntPropertyRNA {
  PropertyRNA property;

  PropIntGetFunc get;
  PropIntSetFunc set;
  PropIntArrayGetFunc getarray;
  PropIntArraySetFunc setarray;
  PropIntRangeFunc range;

  PropIntGetFuncEx get_ex;
  PropIntSetFuncEx set_ex;
  PropIntArrayGetFuncEx getarray_ex;
  PropIntArraySetFuncEx setarray_ex;
  PropIntRangeFuncEx range_ex;

  PropertyScaleType ui_scale_type;
  int softmin, softmax;
  int hardmin, hardmax;
  int step;

  int defaultvalue;
  const int *defaultarray;
};

struct FloatPropertyRNA {
  PropertyRNA property;

  PropFloatGetFunc get;
  PropFloatSetFunc set;
  PropFloatArrayGetFunc getarray;
  PropFloatArraySetFunc setarray;
  PropFloatRangeFunc range;

  PropFloatGetFuncEx get_ex;
  PropFloatSetFuncEx set_ex;
  PropFloatArrayGetFuncEx getarray_ex;
  PropFloatArraySetFuncEx setarray_ex;
  PropFloatRangeFuncEx range_ex;

  PropertyScaleType ui_scale_type;
  float softmin, softmax;
  float hardmin, hardmax;
  float step;
  int precision;

  float defaultvalue;
  const float *defaultarray;
};

struct StringPropertyRNA {
  PropertyRNA property;

  PropStringGetFunc get;
  PropStringLengthFunc length;
  PropStringSetFunc set;

  PropStringGetFuncEx get_ex;
  PropStringLengthFuncEx length_ex;
  PropStringSetFuncEx set_ex;

  /**
   * Optional callback to list candidates for a string.
   * This is only for use as suggestions in UI, other values may be assigned.
   *
   * \note The callback type is public, hence the difference in naming convention.
   */
  StringPropertySearchFunc search;
  eStringPropertySearchFlag search_flag;

  int maxlength; /* includes string terminator! */

  const char *defaultvalue;
};

struct EnumPropertyRNA {
  PropertyRNA property;

  PropEnumGetFunc get;
  PropEnumSetFunc set;
  PropEnumItemFunc item_fn;

  PropEnumGetFuncEx get_ex;
  PropEnumSetFuncEx set_ex;

  const EnumPropertyItem *item;
  int totitem;

  int defaultvalue;
  const char *native_enum_type;
};

struct PointerPropertyRNA {
  PropertyRNA property;

  PropPointerGetFunc get;
  PropPointerSetFunc set;
  PropPointerTypeFunc type_fn;
  /** unlike operators, 'set' can still run if poll fails, used for filtering display. */
  PropPointerPollFunc poll;

  StructRNA *type;
};

struct CollectionPropertyRNA {
  PropertyRNA property;

  PropCollectionBeginFunc begin;
  PropCollectionNextFunc next;
  PropCollectionEndFunc end; /* optional */
  PropCollectionGetFunc get;
  PropCollectionLengthFunc length;             /* optional */
  PropCollectionLookupIntFunc lookupint;       /* optional */
  PropCollectionLookupStringFunc lookupstring; /* optional */
  PropCollectionAssignIntFunc assignint;       /* optional */

  StructRNA *item_type; /* the type of this item */
};

/* changes to this struct require updating rna_generate_struct in makesrna.cc */
struct StructRNA {
  /* structs are containers of properties */
  ContainerRNA cont;

  /* unique identifier, keep after 'cont' */
  const char *identifier;

  /** Python type, this is a subtype of #pyrna_struct_Type
   * but used so each struct can have its own type which is useful for subclassing RNA. */
  void *py_type;
  void *blender_type;

  /* various options */
  int flag;
  /* Each StructRNA type can define its own tags which properties can set
   * (PropertyRNA.tags) for changed behavior based on struct-type. */
  const EnumPropertyItem *prop_tag_defines;

  /* user readable name */
  const char *name;
  /* single line description, displayed in the tooltip for example */
  const char *description;
  /* context for translation */
  const char *translation_context;
  /* icon ID */
  int icon;

  /* property that defines the name */
  PropertyRNA *nameproperty;

  /* property to iterate over properties */
  PropertyRNA *iteratorproperty;

  /** Struct this is derived from. */
  StructRNA *base;

  /* only use for nested structs, where both the parent and child access
   * the same C Struct but nesting is used for grouping properties.
   * The parent property is used so we know NULL checks are not needed,
   * and that this struct will never exist without its parent */
  StructRNA *nested;

  /* function to give the more specific type */
  StructRefineFunc refine;

  /* function to find path to this struct in an ID */
  StructPathFunc path;

  /* function to register/unregister subclasses */
  StructRegisterFunc reg;
  StructUnregisterFunc unreg;
  /**
   * Optionally support reusing Python instances for this type.
   *
   * Without this, an operator class created for #wmOperatorType.invoke (for example)
   * would have a different instance passed to the #wmOperatorType.modal callback.
   * So any variables assigned to `self` from Python would not be available to other callbacks.
   *
   * Being able to access the instance also has the advantage that we can invalidate
   * the Python instance when the data has been removed, see: #BPY_DECREF_RNA_INVALIDATE
   * so accessing the variables from Python raises an exception instead of crashing.
   */
  StructInstanceFunc instance;

  /** Return the location of the struct's pointer to the root group IDProperty. */
  IDPropertiesFunc idproperties;

  /* functions of this struct */
  ListBase functions;
};

/* Blender RNA
 *
 * Root RNA data structure that lists all struct types. */

struct BlenderRNA {
  ListBase structs;
  /* A map of structs: {StructRNA.identifier -> StructRNA}
   * These are ensured to have unique names (with STRUCT_PUBLIC_NAMESPACE enabled). */
  GHash *structs_map;
  /* Needed because types with an empty identifier aren't included in 'structs_map'. */
  unsigned int structs_len;
};

#define CONTAINER_RNA_ID(cont) (*(const char **)(((ContainerRNA *)(cont)) + 1))

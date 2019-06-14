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
 */

/** \file
 * \ingroup RNA
 */

#ifndef __RNA_INTERNAL_TYPES_H__
#define __RNA_INTERNAL_TYPES_H__

#include "DNA_listBase.h"

#include "RNA_types.h"

struct BlenderRNA;
struct CollectionPropertyIterator;
struct ContainerRNA;
struct FunctionRNA;
struct GHash;
struct IDOverrideLibrary;
struct IDOverrideLibraryProperty;
struct IDOverrideLibraryPropertyOperation;
struct IDProperty;
struct Main;
struct PointerRNA;
struct PropertyRNA;
struct ReportList;
struct Scene;
struct StructRNA;
struct bContext;

/* store local properties here */
#define RNA_IDP_UI "_RNA_UI"

/* Function Callbacks */

typedef void (*UpdateFunc)(struct Main *main, struct Scene *scene, struct PointerRNA *ptr);
typedef void (*ContextPropUpdateFunc)(struct bContext *C,
                                      struct PointerRNA *ptr,
                                      struct PropertyRNA *prop);
typedef void (*ContextUpdateFunc)(struct bContext *C, struct PointerRNA *ptr);
typedef int (*EditableFunc)(struct PointerRNA *ptr, const char **r_info);
typedef int (*ItemEditableFunc)(struct PointerRNA *ptr, int index);
typedef struct IDProperty *(*IDPropertiesFunc)(struct PointerRNA *ptr, bool create);
typedef struct StructRNA *(*StructRefineFunc)(struct PointerRNA *ptr);
typedef char *(*StructPathFunc)(struct PointerRNA *ptr);

typedef int (*PropArrayLengthGetFunc)(struct PointerRNA *ptr, int length[RNA_MAX_ARRAY_DIMENSION]);
typedef bool (*PropBooleanGetFunc)(struct PointerRNA *ptr);
typedef void (*PropBooleanSetFunc)(struct PointerRNA *ptr, bool value);
typedef void (*PropBooleanArrayGetFunc)(struct PointerRNA *ptr, bool *values);
typedef void (*PropBooleanArraySetFunc)(struct PointerRNA *ptr, const bool *values);
typedef int (*PropIntGetFunc)(struct PointerRNA *ptr);
typedef void (*PropIntSetFunc)(struct PointerRNA *ptr, int value);
typedef void (*PropIntArrayGetFunc)(struct PointerRNA *ptr, int *values);
typedef void (*PropIntArraySetFunc)(struct PointerRNA *ptr, const int *values);
typedef void (*PropIntRangeFunc)(
    struct PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax);
typedef float (*PropFloatGetFunc)(struct PointerRNA *ptr);
typedef void (*PropFloatSetFunc)(struct PointerRNA *ptr, float value);
typedef void (*PropFloatArrayGetFunc)(struct PointerRNA *ptr, float *values);
typedef void (*PropFloatArraySetFunc)(struct PointerRNA *ptr, const float *values);
typedef void (*PropFloatRangeFunc)(
    struct PointerRNA *ptr, float *min, float *max, float *softmin, float *softmax);
typedef void (*PropStringGetFunc)(struct PointerRNA *ptr, char *value);
typedef int (*PropStringLengthFunc)(struct PointerRNA *ptr);
typedef void (*PropStringSetFunc)(struct PointerRNA *ptr, const char *value);
typedef int (*PropEnumGetFunc)(struct PointerRNA *ptr);
typedef void (*PropEnumSetFunc)(struct PointerRNA *ptr, int value);
typedef const EnumPropertyItem *(*PropEnumItemFunc)(struct bContext *C,
                                                    struct PointerRNA *ptr,
                                                    struct PropertyRNA *prop,
                                                    bool *r_free);
typedef PointerRNA (*PropPointerGetFunc)(struct PointerRNA *ptr);
typedef StructRNA *(*PropPointerTypeFunc)(struct PointerRNA *ptr);
typedef void (*PropPointerSetFunc)(struct PointerRNA *ptr,
                                   const PointerRNA value,
                                   struct ReportList *reports);
typedef bool (*PropPointerPollFunc)(struct PointerRNA *ptr, const PointerRNA value);
typedef bool (*PropPointerPollFuncPy)(struct PointerRNA *ptr,
                                      const PointerRNA value,
                                      const PropertyRNA *prop);
typedef void (*PropCollectionBeginFunc)(struct CollectionPropertyIterator *iter,
                                        struct PointerRNA *ptr);
typedef void (*PropCollectionNextFunc)(struct CollectionPropertyIterator *iter);
typedef void (*PropCollectionEndFunc)(struct CollectionPropertyIterator *iter);
typedef PointerRNA (*PropCollectionGetFunc)(struct CollectionPropertyIterator *iter);
typedef int (*PropCollectionLengthFunc)(struct PointerRNA *ptr);
typedef int (*PropCollectionLookupIntFunc)(struct PointerRNA *ptr,
                                           int key,
                                           struct PointerRNA *r_ptr);
typedef int (*PropCollectionLookupStringFunc)(struct PointerRNA *ptr,
                                              const char *key,
                                              struct PointerRNA *r_ptr);
typedef int (*PropCollectionAssignIntFunc)(struct PointerRNA *ptr,
                                           int key,
                                           const struct PointerRNA *assign_ptr);

/* extended versions with PropertyRNA argument */
typedef bool (*PropBooleanGetFuncEx)(struct PointerRNA *ptr, struct PropertyRNA *prop);
typedef void (*PropBooleanSetFuncEx)(struct PointerRNA *ptr, struct PropertyRNA *prop, bool value);
typedef void (*PropBooleanArrayGetFuncEx)(struct PointerRNA *ptr,
                                          struct PropertyRNA *prop,
                                          bool *values);
typedef void (*PropBooleanArraySetFuncEx)(struct PointerRNA *ptr,
                                          struct PropertyRNA *prop,
                                          const bool *values);
typedef int (*PropIntGetFuncEx)(struct PointerRNA *ptr, struct PropertyRNA *prop);
typedef void (*PropIntSetFuncEx)(struct PointerRNA *ptr, struct PropertyRNA *prop, int value);
typedef void (*PropIntArrayGetFuncEx)(struct PointerRNA *ptr,
                                      struct PropertyRNA *prop,
                                      int *values);
typedef void (*PropIntArraySetFuncEx)(struct PointerRNA *ptr,
                                      struct PropertyRNA *prop,
                                      const int *values);
typedef void (*PropIntRangeFuncEx)(struct PointerRNA *ptr,
                                   struct PropertyRNA *prop,
                                   int *min,
                                   int *max,
                                   int *softmin,
                                   int *softmax);
typedef float (*PropFloatGetFuncEx)(struct PointerRNA *ptr, struct PropertyRNA *prop);
typedef void (*PropFloatSetFuncEx)(struct PointerRNA *ptr, struct PropertyRNA *prop, float value);
typedef void (*PropFloatArrayGetFuncEx)(struct PointerRNA *ptr,
                                        struct PropertyRNA *prop,
                                        float *values);
typedef void (*PropFloatArraySetFuncEx)(struct PointerRNA *ptr,
                                        struct PropertyRNA *prop,
                                        const float *values);
typedef void (*PropFloatRangeFuncEx)(struct PointerRNA *ptr,
                                     struct PropertyRNA *prop,
                                     float *min,
                                     float *max,
                                     float *softmin,
                                     float *softmax);
typedef void (*PropStringGetFuncEx)(struct PointerRNA *ptr, struct PropertyRNA *prop, char *value);
typedef int (*PropStringLengthFuncEx)(struct PointerRNA *ptr, struct PropertyRNA *prop);
typedef void (*PropStringSetFuncEx)(struct PointerRNA *ptr,
                                    struct PropertyRNA *prop,
                                    const char *value);
typedef int (*PropEnumGetFuncEx)(struct PointerRNA *ptr, struct PropertyRNA *prop);
typedef void (*PropEnumSetFuncEx)(struct PointerRNA *ptr, struct PropertyRNA *prop, int value);

/* Handling override operations, and also comparison. */

/**
 * If \a override is NULL, merely do comparison between prop_a from ptr_a and prop_b from ptr_b,
 * following comparison mode given.
 * If \a override and \a rna_path are not NULL, it will add a new override operation for
 * overridable properties that differ and have not yet been overridden
 * (and set accordingly \a r_override_changed if given).
 *
 * \note Given PropertyRNA are final (in case of IDProps...).
 * \note In non-array cases, \a len values are 0.
 * \note \a override, \a rna_path and \a r_override_changed may be NULL pointers.
 */
typedef int (*RNAPropOverrideDiff)(struct Main *bmain,
                                   struct PointerRNA *ptr_a,
                                   struct PointerRNA *ptr_b,
                                   struct PropertyRNA *prop_a,
                                   struct PropertyRNA *prop_b,
                                   const int len_a,
                                   const int len_b,
                                   const int mode,
                                   struct IDOverrideLibrary *override,
                                   const char *rna_path,
                                   const int flags,
                                   bool *r_override_changed);

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
typedef bool (*RNAPropOverrideStore)(struct Main *bmain,
                                     struct PointerRNA *ptr_local,
                                     struct PointerRNA *ptr_reference,
                                     struct PointerRNA *ptr_storage,
                                     struct PropertyRNA *prop_local,
                                     struct PropertyRNA *prop_reference,
                                     struct PropertyRNA *prop_storage,
                                     const int len_local,
                                     const int len_reference,
                                     const int len_storage,
                                     struct IDOverrideLibraryPropertyOperation *opop);

/**
 * Apply given override operation from src to dst (using value from storage as second operand
 * for differential operations).
 *
 * \note Given PropertyRNA are final (in case of IDProps...).
 * \note In non-array cases, \a len values are 0.
 */
typedef bool (*RNAPropOverrideApply)(struct Main *bmain,
                                     struct PointerRNA *ptr_dst,
                                     struct PointerRNA *ptr_src,
                                     struct PointerRNA *ptr_storage,
                                     struct PropertyRNA *prop_dst,
                                     struct PropertyRNA *prop_src,
                                     struct PropertyRNA *prop_storage,
                                     const int len_dst,
                                     const int len_src,
                                     const int len_storage,
                                     struct PointerRNA *ptr_item_dst,
                                     struct PointerRNA *ptr_item_src,
                                     struct PointerRNA *ptr_item_storage,
                                     struct IDOverrideLibraryPropertyOperation *opop);

/* Container - generic abstracted container of RNA properties */
typedef struct ContainerRNA {
  void *next, *prev;

  struct GHash *prophash;
  ListBase properties;
} ContainerRNA;

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
   * note: this is only the C return value, rna functions can have multiple return values */
  PropertyRNA *c_ret;
};

struct PropertyRNA {
  struct PropertyRNA *next, *prev;

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
  /* array lengths lengths for all dimensions (when arraydimension > 0) */
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
  struct StructRNA *srna; /* attributes attached directly to this collection */

  /* python handle to hold all callbacks
   * (in a pointer array at the moment, may later be a tuple) */
  void *py_data;
};

/* internal flags WARNING! 16bits only! */
typedef enum PropertyFlagIntern {
  PROP_INTERN_BUILTIN = (1 << 0),
  PROP_INTERN_RUNTIME = (1 << 1),
  PROP_INTERN_RAW_ACCESS = (1 << 2),
  PROP_INTERN_RAW_ARRAY = (1 << 3),
  PROP_INTERN_FREE_POINTERS = (1 << 4),
} PropertyFlagIntern;

/* Property Types */

typedef struct BoolPropertyRNA {
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
} BoolPropertyRNA;

typedef struct IntPropertyRNA {
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

  int softmin, softmax;
  int hardmin, hardmax;
  int step;

  int defaultvalue;
  const int *defaultarray;
} IntPropertyRNA;

typedef struct FloatPropertyRNA {
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

  float softmin, softmax;
  float hardmin, hardmax;
  float step;
  int precision;

  float defaultvalue;
  const float *defaultarray;
} FloatPropertyRNA;

typedef struct StringPropertyRNA {
  PropertyRNA property;

  PropStringGetFunc get;
  PropStringLengthFunc length;
  PropStringSetFunc set;

  PropStringGetFuncEx get_ex;
  PropStringLengthFuncEx length_ex;
  PropStringSetFuncEx set_ex;

  int maxlength; /* includes string terminator! */

  const char *defaultvalue;
} StringPropertyRNA;

typedef struct EnumPropertyRNA {
  PropertyRNA property;

  PropEnumGetFunc get;
  PropEnumSetFunc set;
  PropEnumItemFunc itemf;

  PropEnumGetFuncEx get_ex;
  PropEnumSetFuncEx set_ex;
  void *py_data; /* store py callback here */

  const EnumPropertyItem *item;
  int totitem;

  int defaultvalue;
} EnumPropertyRNA;

typedef struct PointerPropertyRNA {
  PropertyRNA property;

  PropPointerGetFunc get;
  PropPointerSetFunc set;
  PropPointerTypeFunc typef;
  /** unlike operators, 'set' can still run if poll fails, used for filtering display. */
  PropPointerPollFunc poll;

  struct StructRNA *type;
} PointerPropertyRNA;

typedef struct CollectionPropertyRNA {
  PropertyRNA property;

  PropCollectionBeginFunc begin;
  PropCollectionNextFunc next;
  PropCollectionEndFunc end; /* optional */
  PropCollectionGetFunc get;
  PropCollectionLengthFunc length;             /* optional */
  PropCollectionLookupIntFunc lookupint;       /* optional */
  PropCollectionLookupStringFunc lookupstring; /* optional */
  PropCollectionAssignIntFunc assignint;       /* optional */

  struct StructRNA *item_type; /* the type of this item */
} CollectionPropertyRNA;

/* changes to this struct require updating rna_generate_struct in makesrna.c */
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
  /* Each StructRNA type can define own tags which properties can set
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

  /* struct this is derivedfrom */
  struct StructRNA *base;

  /* only use for nested structs, where both the parent and child access
   * the same C Struct but nesting is used for grouping properties.
   * The parent property is used so we know NULL checks are not needed,
   * and that this struct will never exist without its parent */
  struct StructRNA *nested;

  /* function to give the more specific type */
  StructRefineFunc refine;

  /* function to find path to this struct in an ID */
  StructPathFunc path;

  /* function to register/unregister subclasses */
  StructRegisterFunc reg;
  StructUnregisterFunc unreg;
  StructInstanceFunc instance;

  /* callback to get id properties */
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
  struct GHash *structs_map;
  /* Needed because types with an empty identifier aren't included in 'structs_map'. */
  unsigned int structs_len;
};

#define CONTAINER_RNA_ID(cont) (*(const char **)(((ContainerRNA *)(cont)) + 1))

#endif /* __RNA_INTERNAL_TYPES_H__ */

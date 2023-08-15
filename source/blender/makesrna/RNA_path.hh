/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup RNA
 *
 * RNA paths are a way to refer to pointers and properties with a string,
 * using a syntax like: scenes[0].objects["Cube"].data.verts[7].co
 *
 * This provides a way to refer to RNA data while being detached from any
 * particular pointers, which is useful in a number of applications, like
 * UI code or Actions, though efficiency is a concern.
 */

#include "RNA_types.hh"

struct ListBase;
struct IDProperty;

char *RNA_path_append(
    const char *path, const PointerRNA *ptr, PropertyRNA *prop, int intkey, const char *strkey);
#if 0 /* UNUSED. */
char *RNA_path_back(const char *path);
#endif

/**
 * Search for the start of the 'rna array index' part of the given `rna_path`.
 *
 * Given the root RNA pointer and resolved RNA property, and the RNA path, return the first
 * character in `rna_path` that is part of the array index for the given property. Return NULL if
 * none can be found, e.g. because the property is not an RNA array.
 *
 * \param array_prop: if not NULL, the #PropertyRNA assumed to be the last one from the RNA path.
 * Only used to ensure it is a valid array property.
 */
const char *RNA_path_array_index_token_find(const char *rna_path, const PropertyRNA *array_prop);

/* RNA_path_resolve() variants only ensure that a valid pointer (and optionally property) exist. */

/**
 * Resolve the given RNA Path to find the pointer and/or property
 * indicated by fully resolving the path.
 *
 * \warning Unlike \a RNA_path_resolve_property(), that one *will* try to follow RNAPointers,
 * e.g. the path 'parent' applied to a RNAObject \a ptr will return the object.parent in \a r_ptr,
 * and a NULL \a r_prop...
 *
 * \note Assumes all pointers provided are valid
 * \return True if path can be resolved to a valid "pointer + property" OR "pointer only"
 */
bool RNA_path_resolve(const PointerRNA *ptr,
                      const char *path,
                      PointerRNA *r_ptr,
                      PropertyRNA **r_prop);

/**
 * Resolve the given RNA Path to find the pointer and/or property + array index
 * indicated by fully resolving the path.
 *
 * \note Assumes all pointers provided are valid.
 * \return True if path can be resolved to a valid "pointer + property" OR "pointer only"
 */
bool RNA_path_resolve_full(const PointerRNA *ptr,
                           const char *path,
                           PointerRNA *r_ptr,
                           PropertyRNA **r_prop,
                           int *r_index);
/**
 * A version of #RNA_path_resolve_full doesn't check the value of #PointerRNA.data.
 *
 * \note While it's correct to ignore the value of #PointerRNA.data
 * most callers need to know if the resulting pointer was found and not null.
 */
bool RNA_path_resolve_full_maybe_null(const PointerRNA *ptr,
                                      const char *path,
                                      PointerRNA *r_ptr,
                                      PropertyRNA **r_prop,
                                      int *r_index);

/* RNA_path_resolve_property() variants ensure that pointer + property both exist. */

/**
 * Resolve the given RNA Path to find both the pointer AND property
 * indicated by fully resolving the path.
 *
 * This is a convenience method to avoid logic errors and ugly syntax.
 * \note Assumes all pointers provided are valid
 * \return True only if both a valid pointer and property are found after resolving the path
 */
bool RNA_path_resolve_property(const PointerRNA *ptr,
                               const char *path,
                               PointerRNA *r_ptr,
                               PropertyRNA **r_prop);

/**
 * Resolve the given RNA Path to find the pointer AND property (as well as the array index)
 * indicated by fully resolving the path.
 *
 * This is a convenience method to avoid logic errors and ugly syntax.
 * \note Assumes all pointers provided are valid
 * \return True only if both a valid pointer and property are found after resolving the path
 */
bool RNA_path_resolve_property_full(const PointerRNA *ptr,
                                    const char *path,
                                    PointerRNA *r_ptr,
                                    PropertyRNA **r_prop,
                                    int *r_index);

/* RNA_path_resolve_property_and_item_pointer() variants ensure that pointer + property both exist,
 * and resolve last Pointer value if possible (Pointer prop or item of a Collection prop). */

/**
 * Resolve the given RNA Path to find both the pointer AND property
 * indicated by fully resolving the path, and get the value of the Pointer property
 * (or item of the collection).
 *
 * This is a convenience method to avoid logic errors and ugly syntax,
 * it combines both \a RNA_path_resolve and #RNA_path_resolve_property in a single call.
 * \note Assumes all pointers provided are valid.
 * \param r_item_ptr: The final Pointer or Collection item value.
 * You must check for its validity before use!
 * \return True only if both a valid pointer and property are found after resolving the path
 */
bool RNA_path_resolve_property_and_item_pointer(const PointerRNA *ptr,
                                                const char *path,
                                                PointerRNA *r_ptr,
                                                PropertyRNA **r_prop,
                                                PointerRNA *r_item_ptr);

/**
 * Resolve the given RNA Path to find both the pointer AND property (as well as the array index)
 * indicated by fully resolving the path,
 * and get the value of the Pointer property (or item of the collection).
 *
 * This is a convenience method to avoid logic errors and ugly syntax,
 * it combines both \a RNA_path_resolve_full and
 * \a RNA_path_resolve_property_full in a single call.
 * \note Assumes all pointers provided are valid.
 * \param r_item_ptr: The final Pointer or Collection item value.
 * You must check for its validity before use!
 * \return True only if both a valid pointer and property are found after resolving the path
 */
bool RNA_path_resolve_property_and_item_pointer_full(const PointerRNA *ptr,
                                                     const char *path,
                                                     PointerRNA *r_ptr,
                                                     PropertyRNA **r_prop,
                                                     int *r_index,
                                                     PointerRNA *r_item_ptr);

typedef struct PropertyElemRNA PropertyElemRNA;
struct PropertyElemRNA {
  PropertyElemRNA *next, *prev;
  PointerRNA ptr;
  PropertyRNA *prop;
  int index;
};
/**
 * Resolve the given RNA Path into a linked list of #PropertyElemRNA's.
 *
 * To be used when complex operations over path are needed, like e.g. get relative paths,
 * to avoid too much string operations.
 *
 * \return True if there was no error while resolving the path
 * \note Assumes all pointers provided are valid
 */
bool RNA_path_resolve_elements(PointerRNA *ptr, const char *path, struct ListBase *r_elements);

/**
 * Find the path from the structure referenced by the pointer to the runtime RNA-defined
 * #IDProperty object.
 *
 * \note Does *not* handle pure user-defined IDProperties (a.k.a. custom properties).
 *
 * \param ptr: Reference to the object owning the custom property storage.
 * \param needle: Custom property object to find.
 * \return Relative path or NULL.
 */
char *RNA_path_from_struct_to_idproperty(PointerRNA *ptr, struct IDProperty *needle);

/**
 * Find the actual ID pointer and path from it to the given ID.
 *
 * \param id: ID reference to search the global owner for.
 * \param[out] r_path: Path from the real ID to the initial ID.
 * \return The ID pointer, or NULL in case of failure.
 */
struct ID *RNA_find_real_ID_and_path(struct ID *id, const char **r_path);

char *RNA_path_from_ID_to_struct(const PointerRNA *ptr);

char *RNA_path_from_real_ID_to_struct(struct Main *bmain,
                                      const PointerRNA *ptr,
                                      struct ID **r_real);

char *RNA_path_from_ID_to_property(const PointerRNA *ptr, PropertyRNA *prop);
/**
 * \param index_dim: The dimension to show, 0 disables. 1 for 1d array, 2 for 2d. etc.
 * \param index: The *flattened* index to use when \a `index_dim > 0`,
 * this is expanded when used with multi-dimensional arrays.
 */
char *RNA_path_from_ID_to_property_index(const PointerRNA *ptr,
                                         PropertyRNA *prop,
                                         int index_dim,
                                         int index);

char *RNA_path_from_real_ID_to_property_index(struct Main *bmain,
                                              const PointerRNA *ptr,
                                              PropertyRNA *prop,
                                              int index_dim,
                                              int index,
                                              struct ID **r_real_id);

/**
 * \return the path to given ptr/prop from the closest ancestor of given type,
 * if any (else return NULL).
 */
char *RNA_path_resolve_from_type_to_property(const PointerRNA *ptr,
                                             PropertyRNA *prop,
                                             const struct StructRNA *type);

/**
 * Get the ID as a python representation, eg:
 *   bpy.data.foo["bar"]
 */
char *RNA_path_full_ID_py(struct ID *id);
/**
 * Get the ID.struct as a python representation, eg:
 *   bpy.data.foo["bar"].some_struct
 */
char *RNA_path_full_struct_py(const PointerRNA *ptr);
/**
 * Get the ID.struct.property as a python representation, eg:
 *   bpy.data.foo["bar"].some_struct.some_prop[10]
 */
char *RNA_path_full_property_py_ex(const PointerRNA *ptr,
                                   PropertyRNA *prop,
                                   int index,
                                   bool use_fallback);
char *RNA_path_full_property_py(const PointerRNA *ptr, PropertyRNA *prop, int index);
/**
 * Get the struct.property as a python representation, eg:
 *   some_struct.some_prop[10]
 */
char *RNA_path_struct_property_py(PointerRNA *ptr, PropertyRNA *prop, int index);
/**
 * Get the struct.property as a python representation, eg:
 *   some_prop[10]
 */
char *RNA_path_property_py(const PointerRNA *ptr, PropertyRNA *prop, int index);

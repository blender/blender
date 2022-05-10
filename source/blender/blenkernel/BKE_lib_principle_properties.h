/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation. All rights reserved. */

#pragma once

/** \file
 * \ingroup bke
 *
 * API to manage principle properties in data-blocks.
 *
 * Principle properties are properties that are defined as the ones most user will need to
 * edit when using this data-block.
 *
 * They current main usage in is library overrides.
 *
 * \note `BKE_lib_` files are for operations over data-blocks themselves, although they might
 * alter Main as well (when creating/renaming/deleting an ID e.g.).
 *
 * \section Function Names
 *
 *  - `BKE_lib_principleprop_` should be used for function affecting a single ID.
 *  - `BKE_lib_principleprop_main_` should be used for function affecting the whole collection
 *    of IDs in a given Main data-base.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct ID;
struct IDPrincipleProperties;
struct IDPrincipleProperty;
struct PointerRNA;
struct PropertyRNA;
struct ReportList;

/**
 * Initialize empty list of principle properties for \a id.
 */
struct IDPrincipleProperties *BKE_lib_principleprop_init(struct ID *id);
#if 0
/**
 * Shallow or deep copy of a whole principle properties from \a src_id to \a dst_id.
 */
void BKE_lib_principleprop_copy(struct ID *dst_id, const struct ID *src_id, bool do_full_copy);
#endif
/**
 * Clear any principle properties data from given \a override.
 */
void BKE_lib_principleprop_clear(struct IDPrincipleProperties *principle_props, bool do_id_user);
/**
 * Free given \a principle_props.
 */
void BKE_lib_principleprop_free(struct IDPrincipleProperties **principle_props, bool do_id_user);

/**
 * Find principle property from given RNA path, if it exists.
 */
struct IDPrincipleProperty *BKE_lib_principleprop_find(
    struct IDPrincipleProperties *principle_props, const char *rna_path);
/**
 * Find principle property from given RNA path, or create it if it does not exist.
 */
struct IDPrincipleProperty *BKE_lib_principleprop_get(
    struct IDPrincipleProperties *principle_props, const char *rna_path, bool *r_created);
/**
 * Remove and free given \a principle_prop from given ID \a principle_props.
 */
void BKE_lib_principleprop_delete(struct IDPrincipleProperties *principle_props,
                                  struct IDPrincipleProperty *principle_prop);
/**
 * Get the RNA-property matching the \a principle_prop principle property. Used for UI to query
 * additional data about the principle property (e.g. UI name).
 *
 * \param idpoin: RNA Pointer of the ID.
 * \param principle_prop: The principle property to find the matching RNA property for.
 */
bool BKE_lib_principleprop_rna_property_find(struct PointerRNA *idpoin,
                                             const struct IDPrincipleProperty *principle_prop,
                                             struct PointerRNA *r_principle_poin,
                                             struct PropertyRNA **r_principle_prop);

#ifdef __cplusplus
}
#endif

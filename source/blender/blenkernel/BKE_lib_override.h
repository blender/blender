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
 *
 * The Original Code is Copyright (C) 2016 by Blender Foundation.
 * All rights reserved.
 */

#pragma once

/** \file
 * \ingroup bke
 *
 * API to manage data-blocks inside of Blender's Main data-base, or as independent runtime-only
 * data.
 *
 * \note `BKE_lib_` files are for operations over data-blocks themselves, although they might
 * alter Main as well (when creating/renaming/deleting an ID e.g.).
 *
 * \section Function Names
 *
 * \warning Descriptions below is ideal goal, current status of naming does not yet fully follow it
 * (this is WIP).
 *
 *  - `BKE_lib_override_library_` should be used for function affecting a single ID.
 *  - `BKE_lib_override_library_main_` should be used for function affecting the whole collection
 *    of IDs in a given Main data-base.
 */

#ifdef __cplusplus
extern "C" {
#endif

struct BlendFileReadReport;
struct Collection;
struct ID;
struct IDOverrideLibrary;
struct IDOverrideLibraryProperty;
struct IDOverrideLibraryPropertyOperation;
struct Library;
struct Main;
struct Object;
struct PointerRNA;
struct PropertyRNA;
struct ReportList;
struct Scene;
struct ViewLayer;

/**
 * Initialize empty overriding of \a reference_id by \a local_id.
 */
struct IDOverrideLibrary *BKE_lib_override_library_init(struct ID *local_id,
                                                        struct ID *reference_id);
/**
 * Shallow or deep copy of a whole override from \a src_id to \a dst_id.
 */
void BKE_lib_override_library_copy(struct ID *dst_id, const struct ID *src_id, bool do_full_copy);
/**
 * Clear any overriding data from given \a override.
 */
void BKE_lib_override_library_clear(struct IDOverrideLibrary *override, bool do_id_user);
/**
 * Free given \a override.
 */
void BKE_lib_override_library_free(struct IDOverrideLibrary **override, bool do_id_user);

/**
 * Check if given ID has some override rules that actually indicate the user edited it.
 */
bool BKE_lib_override_library_is_user_edited(struct ID *id);

/**
 * Create an overridden local copy of linked reference.
 *
 * \note This function is very basic, low-level. It does not consider any hierarchical dependency,
 * and also prevents any automatic re-sync of this local override.
 */
struct ID *BKE_lib_override_library_create_from_id(struct Main *bmain,
                                                   struct ID *reference_id,
                                                   bool do_tagged_remap);
/**
 * Create overridden local copies of all tagged data-blocks in given Main.
 *
 * \note Set `id->newid` of overridden libs with newly created overrides,
 * caller is responsible to clean those pointers before/after usage as needed.
 *
 * \note By default, it will only remap newly created local overriding data-blocks between
 * themselves, to avoid 'enforcing' those overrides into all other usages of the linked data in
 * main. You can add more local IDs to be remapped to use new overriding ones by setting their
 * LIB_TAG_DOIT tag.
 *
 * \param reference_library: the library from which the linked data being overridden come from
 * (i.e. the library of the linked reference ID).
 *
 * \param do_no_main: Create the new override data outside of Main database.
 * Used for resyncing of linked overrides.
 *
 * \return \a true on success, \a false otherwise.
 */
bool BKE_lib_override_library_create_from_tag(struct Main *bmain,
                                              const struct Library *reference_library,
                                              bool do_no_main);
/**
 * Advanced 'smart' function to create fully functional overrides.
 *
 * \note Currently it only does special things if given \a id_root is an object or collection, more
 * specific behaviors may be added in the future for other ID types.
 *
 * \note It will override all IDs tagged with \a LIB_TAG_DOIT, and it does not clear that tag at
 * its beginning, so caller code can add extra data-blocks to be overridden as well.
 *
 * \param view_layer: the active view layer to search instantiated collections in, can be NULL (in
 *                    which case \a scene's master collection children hierarchy is used instead).
 * \param id_root: The root ID to create an override from.
 * \param id_reference: Some reference ID used to do some post-processing after overrides have been
 * created, may be NULL. Typically, the Empty object instantiating the linked collection we
 * override, currently.
 * \param r_id_root_override: if not NULL, the override generated for the given \a id_root.
 * \return true if override was successfully created.
 */
bool BKE_lib_override_library_create(struct Main *bmain,
                                     struct Scene *scene,
                                     struct ViewLayer *view_layer,
                                     struct ID *id_root,
                                     struct ID *id_reference,
                                     struct ID **r_id_root_override);
/**
 * Create a library override template.
 */
bool BKE_lib_override_library_template_create(struct ID *id);
/**
 * Convert a given proxy object into a library override.
 *
 * \note This is a thin wrapper around \a BKE_lib_override_library_create, only extra work is to
 * actually convert the proxy itself into an override first.
 *
 * \param view_layer: the active view layer to search instantiated collections in, can be NULL (in
 *                    which case \a scene's master collection children hierarchy is used instead).
 * \return true if override was successfully created.
 */
bool BKE_lib_override_library_proxy_convert(struct Main *bmain,
                                            struct Scene *scene,
                                            struct ViewLayer *view_layer,
                                            struct Object *ob_proxy);
/**
 * Convert all proxy objects into library overrides.
 *
 * \note Only affects local proxies, linked ones are not affected.
 */
void BKE_lib_override_library_main_proxy_convert(struct Main *bmain,
                                                 struct BlendFileReadReport *reports);
/**
 * Advanced 'smart' function to resync, re-create fully functional overrides up-to-date with linked
 * data, from an existing override hierarchy.
 *
 * \param view_layer: the active view layer to search instantiated collections in, can be NULL (in
 *                    which case \a scene's master collection children hierarchy is used instead).
 * \param id_root: The root liboverride ID to resync from.
 * \return true if override was successfully resynced.
 */
bool BKE_lib_override_library_resync(struct Main *bmain,
                                     struct Scene *scene,
                                     struct ViewLayer *view_layer,
                                     struct ID *id_root,
                                     struct Collection *override_resync_residual_storage,
                                     bool do_hierarchy_enforce,
                                     struct BlendFileReadReport *reports);
/**
 * Detect and handle required resync of overrides data, when relations between reference linked IDs
 * have changed.
 *
 * This is a fairly complex and costly operation, typically it should be called after
 * #BKE_lib_override_library_main_update, which would already detect and tag a lot of cases.
 *
 * This function will first detect the remaining cases requiring a resync (namely, either when an
 * existing linked ID that did not require to be overridden before now would be, or when new IDs
 * are added to the hierarchy).
 *
 * Then it will handle the resync of necessary IDs (through calls to
 * #BKE_lib_override_library_resync).
 *
 * \param view_layer: the active view layer to search instantiated collections in, can be NULL (in
 *                    which case \a scene's master collection children hierarchy is used instead).
 */
void BKE_lib_override_library_main_resync(struct Main *bmain,
                                          struct Scene *scene,
                                          struct ViewLayer *view_layer,
                                          struct BlendFileReadReport *reports);

/**
 * Advanced 'smart' function to delete library overrides (including their existing override
 * hierarchy) and remap their usages to their linked reference IDs.
 *
 * \note All IDs tagged with #LIB_TAG_DOIT will be deleted.
 *
 * \param id_root: The root liboverride ID to delete.
 */
void BKE_lib_override_library_delete(struct Main *bmain, struct ID *id_root);

/**
 * Make given ID fully local.
 *
 * \note Only differs from lower-level #BKE_lib_override_library_free in infamous embedded ID
 * cases.
 */
void BKE_lib_override_library_make_local(struct ID *id);

/**
 * Find override property from given RNA path, if it exists.
 */
struct IDOverrideLibraryProperty *BKE_lib_override_library_property_find(
    struct IDOverrideLibrary *override, const char *rna_path);
/**
 * Find override property from given RNA path, or create it if it does not exist.
 */
struct IDOverrideLibraryProperty *BKE_lib_override_library_property_get(
    struct IDOverrideLibrary *override, const char *rna_path, bool *r_created);
/**
 * Remove and free given \a override_property from given ID \a override.
 */
void BKE_lib_override_library_property_delete(struct IDOverrideLibrary *override,
                                              struct IDOverrideLibraryProperty *override_property);
/**
 * Get the RNA-property matching the \a library_prop override property. Used for UI to query
 * additional data about the overridden property (e.g. UI name).
 *
 * \param idpoin: Pointer to the override ID.
 * \param library_prop: The library override property to find the matching RNA property for.
 */
bool BKE_lib_override_rna_property_find(struct PointerRNA *idpoin,
                                        const struct IDOverrideLibraryProperty *library_prop,
                                        struct PointerRNA *r_override_poin,
                                        struct PropertyRNA **r_override_prop);

/**
 * Find override property operation from given sub-item(s), if it exists.
 */
struct IDOverrideLibraryPropertyOperation *BKE_lib_override_library_property_operation_find(
    struct IDOverrideLibraryProperty *override_property,
    const char *subitem_refname,
    const char *subitem_locname,
    int subitem_refindex,
    int subitem_locindex,
    bool strict,
    bool *r_strict);
/**
 * Find override property operation from given sub-item(s), or create it if it does not exist.
 */
struct IDOverrideLibraryPropertyOperation *BKE_lib_override_library_property_operation_get(
    struct IDOverrideLibraryProperty *override_property,
    short operation,
    const char *subitem_refname,
    const char *subitem_locname,
    int subitem_refindex,
    int subitem_locindex,
    bool strict,
    bool *r_strict,
    bool *r_created);
/**
 * Remove and free given \a override_property_operation from given ID \a override_property.
 */
void BKE_lib_override_library_property_operation_delete(
    struct IDOverrideLibraryProperty *override_property,
    struct IDOverrideLibraryPropertyOperation *override_property_operation);

/**
 * Validate that required data for a given operation are available.
 */
bool BKE_lib_override_library_property_operation_operands_validate(
    struct IDOverrideLibraryPropertyOperation *override_property_operation,
    struct PointerRNA *ptr_dst,
    struct PointerRNA *ptr_src,
    struct PointerRNA *ptr_storage,
    struct PropertyRNA *prop_dst,
    struct PropertyRNA *prop_src,
    struct PropertyRNA *prop_storage);

/**
 * Check against potential \a bmain.
 */
void BKE_lib_override_library_validate(struct Main *bmain,
                                       struct ID *id,
                                       struct ReportList *reports);
/**
 * Check against potential \a bmain.
 */
void BKE_lib_override_library_main_validate(struct Main *bmain, struct ReportList *reports);

/**
 * Check that status of local data-block is still valid against current reference one.
 *
 * It means that all overridable, but not overridden, properties' local values must be equal to
 * reference ones. Clears #LIB_TAG_OVERRIDE_OK if they do not.
 *
 * This is typically used to detect whether some property has been changed in local and a new
 * #IDOverrideProperty (of #IDOverridePropertyOperation) has to be added.
 *
 * \return true if status is OK, false otherwise.
 */
bool BKE_lib_override_library_status_check_local(struct Main *bmain, struct ID *local);
/**
 * Check that status of reference data-block is still valid against current local one.
 *
 * It means that all non-overridden properties' local values must be equal to reference ones.
 * Clears LIB_TAG_OVERRIDE_OK if they do not.
 *
 * This is typically used to detect whether some reference has changed and local
 * needs to be updated against it.
 *
 * \return true if status is OK, false otherwise.
 */
bool BKE_lib_override_library_status_check_reference(struct Main *bmain, struct ID *local);

/**
 * Compare local and reference data-blocks and create new override operations as needed,
 * or reset to reference values if overriding is not allowed.
 *
 * \note Defining override operations is only mandatory before saving a `.blend` file on disk
 * (not for undo!).
 * Knowing that info at runtime is only useful for UI/UX feedback.
 *
 * \note This is by far the biggest operation (the more time-consuming) of the three so far,
 * since it has to go over all properties in depth (all overridable ones at least).
 * Generating differential values and applying overrides are much cheaper.
 *
 * \return true if any library operation was created.
 */
bool BKE_lib_override_library_operations_create(struct Main *bmain, struct ID *local);
/**
 * Check all overrides from given \a bmain and create/update overriding operations as needed.
 */
bool BKE_lib_override_library_main_operations_create(struct Main *bmain, bool force_auto);

/**
 * Reset all overrides in given \a id_root, while preserving ID relations.
 */
void BKE_lib_override_library_id_reset(struct Main *bmain, struct ID *id_root);
/**
 * Reset all overrides in given \a id_root and its dependencies, while preserving ID relations.
 */
void BKE_lib_override_library_id_hierarchy_reset(struct Main *bmain, struct ID *id_root);

/**
 * Set or clear given tag in all operations in that override property data.
 */
void BKE_lib_override_library_operations_tag(struct IDOverrideLibraryProperty *override_property,
                                             short tag,
                                             bool do_set);
/**
 * Set or clear given tag in all properties and operations in that override data.
 */
void BKE_lib_override_library_properties_tag(struct IDOverrideLibrary *override,
                                             short tag,
                                             bool do_set);
/**
 * Set or clear given tag in all properties and operations in that Main's ID override data.
 */
void BKE_lib_override_library_main_tag(struct Main *bmain, short tag, bool do_set);

/**
 * Remove all tagged-as-unused properties and operations from that ID override data.
 */
void BKE_lib_override_library_id_unused_cleanup(struct ID *local);
/**
 * Remove all tagged-as-unused properties and operations from that Main's ID override data.
 */
void BKE_lib_override_library_main_unused_cleanup(struct Main *bmain);

/**
 * Update given override from its reference (re-applying overridden properties).
 */
void BKE_lib_override_library_update(struct Main *bmain, struct ID *local);
/**
 * Update all overrides from given \a bmain.
 */
void BKE_lib_override_library_main_update(struct Main *bmain);

/**
 * In case an ID is used by another liboverride ID, user may not be allowed to delete it.
 */
bool BKE_lib_override_library_id_is_user_deletable(struct Main *bmain, struct ID *id);

/* Storage (.blend file writing) part. */

/* For now, we just use a temp main list. */
typedef struct Main OverrideLibraryStorage;

/**
 * Initialize an override storage.
 */
OverrideLibraryStorage *BKE_lib_override_library_operations_store_init(void);
/**
 * Generate suitable 'write' data (this only affects differential override operations).
 *
 * Note that \a local ID is no more modified by this call,
 * all extra data are stored in its temp \a storage_id copy.
 */
struct ID *BKE_lib_override_library_operations_store_start(
    struct Main *bmain, OverrideLibraryStorage *override_storage, struct ID *local);
/**
 * Restore given ID modified by #BKE_lib_override_library_operations_store_start, to its
 * original state.
 */
void BKE_lib_override_library_operations_store_end(OverrideLibraryStorage *override_storage,
                                                   struct ID *local);
void BKE_lib_override_library_operations_store_finalize(OverrideLibraryStorage *override_storage);

#ifdef __cplusplus
}
#endif

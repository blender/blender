/* SPDX-FileCopyrightText: 2016 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

#include <optional>

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
IDOverrideLibrary *BKE_lib_override_library_init(ID *local_id, ID *reference_id);
/**
 * Shallow or deep copy of a whole override from \a src_id to \a dst_id.
 */
void BKE_lib_override_library_copy(ID *dst_id, const ID *src_id, bool do_full_copy);
/**
 * Clear any overriding data from given \a liboverride.
 */
void BKE_lib_override_library_clear(IDOverrideLibrary *liboverride, bool do_id_user);
/**
 * Free given \a liboverride.
 */
void BKE_lib_override_library_free(IDOverrideLibrary **liboverride, bool do_id_user);

/**
 * Return the actual #IDOverrideLibrary data 'controlling' the given `id`, and the actual ID owning
 * it.
 *
 * \note This is especially useful when `id` is a non-real override (e.g. embedded ID like a master
 * collection or root node tree, or a shape key).
 *
 * \param owner_id_hint: If not NULL, a potential owner for the given override-embedded `id`.
 * \param r_owner_id: If given, will be set with the actual ID owning the return liboverride data.
 */
IDOverrideLibrary *BKE_lib_override_library_get(Main *bmain,
                                                ID *id,
                                                ID *owner_id_hint,
                                                ID **r_owner_id);

/**
 * Check if given ID has some override rules that actually indicate the user edited it.
 */
bool BKE_lib_override_library_is_user_edited(const ID *id);

/**
 * Check if given ID is a system override.
 */
bool BKE_lib_override_library_is_system_defined(const Main *bmain, const ID *id);

/**
 * Check if given Override Property for given ID is animated (through a F-Curve in an Action, or
 * from a driver).
 *
 * \param liboverride_rna_prop: if not NULL, the RNA property matching the given path in the
 * `liboverride_prop`.
 * \param rnaprop_index: Array in the RNA property, 0 if unknown or irrelevant.
 */
bool BKE_lib_override_library_property_is_animated(
    const ID *id,
    const IDOverrideLibraryProperty *liboverride_prop,
    const PropertyRNA *override_rna_prop,
    const int rnaprop_index);

/**
 * Check if given ID is a leaf in its liboverride hierarchy (i.e. if it does not use any other
 * override ID).
 *
 * NOTE: Embedded IDs of override IDs are not considered as leaves.
 */
bool BKE_lib_override_library_is_hierarchy_leaf(Main *bmain, ID *id);

/**
 * Create an overridden local copy of linked reference.
 *
 * \note This function is very basic, low-level. It does not consider any hierarchical dependency,
 * and also prevents any automatic re-sync of this local override.
 */
ID *BKE_lib_override_library_create_from_id(Main *bmain, ID *reference_id, bool do_tagged_remap);
/**
 * Create overridden local copies of all tagged data-blocks in given Main.
 *
 * \note Set `id->newid` of overridden libraries with newly created overrides,
 * caller is responsible to clean those pointers before/after usage as needed.
 *
 * \note By default, it will only remap newly created local overriding data-blocks between
 * themselves, to avoid 'enforcing' those overrides into all other usages of the linked data in
 * main. You can add more local IDs to be remapped to use new overriding ones by setting their
 * LIB_TAG_DOIT tag.
 *
 * \param owner_library: the library in which the overrides should be created. Besides versioning
 * and resync code path, this should always be NULL (i.e. the local .blend file).
 *
 * \param id_root_reference: the linked ID that is considered as the root of the overridden
 * hierarchy.
 *
 * \param id_hierarchy_root: the override ID that is the root of the hierarchy. May be NULL, in
 * which case it is assumed that the given `id_root_reference` is tagged for override, and its
 * newly created override will be used as hierarchy root. Must be NULL if
 * `id_hierarchy_root_reference` is not NULL.
 *
 * \param id_hierarchy_root_reference: the linked ID that is the root of the hierarchy. Must be
 * tagged for override. May be NULL, in which case it is assumed that the given `id_root_reference`
 * is tagged for override, and its newly created override will be used as hierarchy root. Must be
 * NULL if `id_hierarchy_root` is not NULL.
 *
 * \param do_no_main: Create the new override data outside of Main database.
 * Used for resyncing of linked overrides.
 *
 * \param do_fully_editable: if true, tag all created overrides as user-editable by default.
 *
 * \return \a true on success, \a false otherwise.
 */
bool BKE_lib_override_library_create_from_tag(Main *bmain,
                                              Library *owner_library,
                                              const ID *id_root_reference,
                                              ID *id_hierarchy_root,
                                              const ID *id_hierarchy_root_reference,
                                              bool do_no_main,
                                              const bool do_fully_editable);
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
 *
 * \param owner_library: the library in which the overrides should be created. Besides versioning
 * and resync code path, this should always be NULL (i.e. the local .blend file).
 *
 * \param id_root_reference: The linked root ID to create an override from. May be a sub-root of
 * the overall hierarchy, in which case calling code is expected to have already tagged required
 * 'path' of IDs leading from the given `id_hierarchy_root` to the given `id_root`.
 *
 * \param id_hierarchy_root_reference: The ID to be used a hierarchy root of the overrides to be
 * created. Can be either the linked root ID of the whole override hierarchy, (typically the same
 * as `id_root`, unless a sub-part only of the hierarchy is overridden), or the already existing
 * override hierarchy root if part of the hierarchy is already overridden.
 *
 * \param id_instance_hint: Some ID used as hint/reference to do some post-processing after
 * overrides have been created, may be NULL. Typically, the Empty object instantiating the linked
 * collection we override, currently.
 *
 * \param r_id_root_override: if not NULL, the override generated for the given \a id_root.
 *
 * \param do_fully_editable: if true, tag all created overrides as user-editable by default.
 *
 * \return true if override was successfully created.
 */
bool BKE_lib_override_library_create(Main *bmain,
                                     Scene *scene,
                                     ViewLayer *view_layer,
                                     Library *owner_library,
                                     ID *id_root_reference,
                                     ID *id_hierarchy_root_reference,
                                     ID *id_instance_hint,
                                     ID **r_id_root_override,
                                     const bool do_fully_editable);
/**
 * Create a library override template.
 */
bool BKE_lib_override_library_template_create(ID *id);
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
bool BKE_lib_override_library_proxy_convert(Main *bmain,
                                            Scene *scene,
                                            ViewLayer *view_layer,
                                            Object *ob_proxy);
/**
 * Convert all proxy objects into library overrides.
 *
 * \note Only affects local proxies, linked ones are not affected.
 */
void BKE_lib_override_library_main_proxy_convert(Main *bmain, BlendFileReadReport *reports);

/**
 * Find and set the 'hierarchy root' ID pointer of all library overrides in given `bmain`.
 *
 * NOTE: Cannot be called from `do_versions_after_linking` as this code needs a single complete
 * Main database, not a split-by-libraries one.
 */
void BKE_lib_override_library_main_hierarchy_root_ensure(Main *bmain);

/**
 * Advanced 'smart' function to resync, re-create fully functional overrides up-to-date with linked
 * data, from an existing override hierarchy.
 *
 * \param view_layer: the active view layer to search instantiated collections in, can be NULL (in
 *                    which case \a scene's master collection children hierarchy is used instead).
 * \param id_root: The root liboverride ID to resync from.
 * \return true if override was successfully resynced.
 */
bool BKE_lib_override_library_resync(Main *bmain,
                                     Scene *scene,
                                     ViewLayer *view_layer,
                                     ID *id_root,
                                     Collection *override_resync_residual_storage,
                                     bool do_hierarchy_enforce,
                                     BlendFileReadReport *reports);
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
void BKE_lib_override_library_main_resync(Main *bmain,
                                          Scene *scene,
                                          ViewLayer *view_layer,
                                          BlendFileReadReport *reports);

/**
 * Advanced 'smart' function to delete library overrides (including their existing override
 * hierarchy) and remap their usages to their linked reference IDs.
 *
 * \note All IDs tagged with #LIB_TAG_DOIT will be deleted.
 *
 * \param id_root: The root liboverride ID to delete.
 */
void BKE_lib_override_library_delete(Main *bmain, ID *id_root);

/**
 * Make given ID fully local.
 *
 * \note Only differs from lower-level #BKE_lib_override_library_free in infamous embedded ID
 * cases.
 */
void BKE_lib_override_library_make_local(ID *id);

/**
 * Find override property from given RNA path, if it exists.
 */
IDOverrideLibraryProperty *BKE_lib_override_library_property_find(IDOverrideLibrary *override,
                                                                  const char *rna_path);
/**
 * Find override property from given RNA path, or create it if it does not exist.
 */
IDOverrideLibraryProperty *BKE_lib_override_library_property_get(IDOverrideLibrary *override,
                                                                 const char *rna_path,
                                                                 bool *r_created);
/**
 * Remove and free given \a liboverride_property from given ID \a liboverride.
 */
void BKE_lib_override_library_property_delete(IDOverrideLibrary *liboverride,
                                              IDOverrideLibraryProperty *liboverride_property);
/**
 * Delete a property override from the given ID \a liboverride, if it exists.
 *
 * \return True when the property was found (and thus deleted), false if it wasn't found.
 */
bool BKE_lib_override_library_property_search_and_delete(IDOverrideLibrary *liboverride,
                                                         const char *rna_path);

/**
 * Change the RNA path of a library override on a property.
 *
 * No-op if the property override cannot be found.
 *
 * \param from_rna_path: The RNA path of the property to change.
 * \param to_rna_path: The new RNA path.
 * The library override system will copy the string to its own memory;
 * the caller will retain ownership of the passed pointer.
 * \return True if the property was found (and thus changed), false if it wasn't found.
 */
bool BKE_lib_override_library_property_rna_path_change(IDOverrideLibrary *liboverride,
                                                       const char *old_rna_path,
                                                       const char *new_rna_path);

/**
 * Get the RNA-property matching the \a library_prop override property. Used for UI to query
 * additional data about the overridden property (e.g. UI name).
 *
 * \param idpoin: Pointer to the override ID.
 * \param library_prop: The library override property to find the matching RNA property for.
 * \param r_index: The RNA array flat index (i.e. flattened index in case of multi-dimensional
 * array properties). See #RNA_path_resolve_full family of functions for details.
 */
bool BKE_lib_override_rna_property_find(PointerRNA *idpoin,
                                        const IDOverrideLibraryProperty *library_prop,
                                        PointerRNA *r_override_poin,
                                        PropertyRNA **r_override_prop,
                                        int *r_index);

/**
 * Find override property operation from given sub-item(s), if it exists.
 *
 * \param subitem_refid:
 * \param subitem_locid: Only for RNA collections of ID pointers, the ID pointers
 * referenced by the given names. Note that both must be set, or left unset.
 */
IDOverrideLibraryPropertyOperation *BKE_lib_override_library_property_operation_find(
    IDOverrideLibraryProperty *liboverride_property,
    const char *subitem_refname,
    const char *subitem_locname,
    const std::optional<const ID *> &subitem_refid,
    const std::optional<const ID *> &subitem_locid,
    int subitem_refindex,
    int subitem_locindex,
    bool strict,
    bool *r_strict);
/**
 * Find override property operation from given sub-item(s), or create it if it does not exist.
 */
IDOverrideLibraryPropertyOperation *BKE_lib_override_library_property_operation_get(
    IDOverrideLibraryProperty *liboverride_property,
    short operation,
    const char *subitem_refname,
    const char *subitem_locname,
    const std::optional<ID *> &subitem_refid,
    const std::optional<ID *> &subitem_locid,
    int subitem_refindex,
    int subitem_locindex,
    bool strict,
    bool *r_strict,
    bool *r_created);
/**
 * Remove and free given \a liboverride_property_operation from given ID \a liboverride_property.
 */
void BKE_lib_override_library_property_operation_delete(
    IDOverrideLibraryProperty *liboverride_property,
    IDOverrideLibraryPropertyOperation *liboverride_property_operation);

/**
 * Validate that required data for a given operation are available.
 */
bool BKE_lib_override_library_property_operation_operands_validate(
    IDOverrideLibraryPropertyOperation *liboverride_property_operation,
    PointerRNA *ptr_dst,
    PointerRNA *ptr_src,
    PointerRNA *ptr_storage,
    PropertyRNA *prop_dst,
    PropertyRNA *prop_src,
    PropertyRNA *prop_storage);

/**
 * Check against potential \a bmain.
 */
void BKE_lib_override_library_validate(Main *bmain, ID *id, ReportList *reports);
/**
 * Check against potential \a bmain.
 */
void BKE_lib_override_library_main_validate(Main *bmain, ReportList *reports);

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
bool BKE_lib_override_library_status_check_local(Main *bmain, ID *local);
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
bool BKE_lib_override_library_status_check_reference(Main *bmain, ID *local);

/**
 * Compare local and reference data-blocks and create new override operations as needed,
 * or reset to reference values if overriding is not allowed.
 *
 * \param r_report_flags: #eRNAOverrideMatchResult flags giving info about the result of this call.
 *
 * \note Defining override operations is only mandatory before saving a `.blend` file on disk
 * (not for undo!).
 * Knowing that info at runtime is only useful for UI/UX feedback.
 *
 * \note This is by far the biggest operation (the more time-consuming) of the three so far,
 * since it has to go over all properties in depth (all overridable ones at least).
 * Generating differential values and applying overrides are much cheaper.
 */
void BKE_lib_override_library_operations_create(Main *bmain, ID *local, int *r_report_flags);
/**
 * Check all overrides from given \a bmain and create/update overriding operations as needed.
 *
 * \param r_report_flags: #eRNAOverrideMatchResult flags giving info about the result of this call.
 */
void BKE_lib_override_library_main_operations_create(Main *bmain,
                                                     bool force_auto,
                                                     int *r_report_flags);

/**
 * Restore forbidden modified override properties to the values of their matching properties in the
 * linked reference ID.
 *
 * \param r_report_flags: #eRNAOverrideMatchResult flags giving info about the result of this call.
 *
 * \note Typically used as part of BKE_lib_override_library_main_operations_create process, since
 * modifying RNA properties from non-main threads is not safe.
 */
void BKE_lib_override_library_operations_restore(Main *bmain, ID *local, int *r_report_flags);
/**
 * Restore forbidden modified override properties to the values of their matching properties in the
 * linked reference ID, for all liboverride IDs tagged as needing such process in given `bmain`.
 *
 * \param r_report_flags: #eRNAOverrideMatchResult flags giving info about the result of this call.
 *
 * \note Typically used as part of BKE_lib_override_library_main_operations_create process, since
 * modifying RNA properties from non-main threads is not safe.
 */
void BKE_lib_override_library_main_operations_restore(Main *bmain, int *r_report_flags);

/**
 * Reset all overrides in given \a id_root, while preserving ID relations.
 *
 * \param do_reset_system_override: If \a true, reset the given ID as a system override one (i.e.
 * non-editable).
 */
void BKE_lib_override_library_id_reset(Main *bmain, ID *id_root, bool do_reset_system_override);
/**
 * Reset all overrides in given \a id_root and its dependencies, while preserving ID relations.
 *
 * \param do_reset_system_override: If \a true, reset the given ID and all of its descendants in
 * the override hierarchy as system override ones (i.e. non-editable).
 */
void BKE_lib_override_library_id_hierarchy_reset(Main *bmain,
                                                 ID *id_root,
                                                 bool do_reset_system_override);

/**
 * Set or clear given tag in all operations in that override property data.
 */
void BKE_lib_override_library_operations_tag(IDOverrideLibraryProperty *liboverride_property,
                                             short tag,
                                             bool do_set);
/**
 * Set or clear given tag in all properties and operations in that override data.
 */
void BKE_lib_override_library_properties_tag(IDOverrideLibrary *liboverride,
                                             short tag,
                                             bool do_set);
/**
 * Set or clear given tag in all properties and operations in that Main's ID override data.
 */
void BKE_lib_override_library_main_tag(Main *bmain, short tag, bool do_set);

/**
 * Remove all tagged-as-unused properties and operations from that ID override data.
 */
void BKE_lib_override_library_id_unused_cleanup(ID *local);
/**
 * Remove all tagged-as-unused properties and operations from that Main's ID override data.
 */
void BKE_lib_override_library_main_unused_cleanup(Main *bmain);

/**
 * Update given override from its reference (re-applying overridden properties).
 */
void BKE_lib_override_library_update(Main *bmain, ID *local);
/**
 * Update all overrides from given \a bmain.
 */
void BKE_lib_override_library_main_update(Main *bmain);

/**
 * In case an ID is used by another liboverride ID, user may not be allowed to delete it.
 */
bool BKE_lib_override_library_id_is_user_deletable(Main *bmain, ID *id);

/**
 * Debugging helper to show content of given liboverride data.
 */
void BKE_lib_override_debug_print(IDOverrideLibrary *liboverride, const char *intro_txt);

/* Storage (.blend file writing) part. */

/* For now, we just use a temp main list. */
using OverrideLibraryStorage = Main;

/**
 * Initialize an override storage.
 */
OverrideLibraryStorage *BKE_lib_override_library_operations_store_init();
/**
 * Generate suitable 'write' data (this only affects differential override operations).
 *
 * Note that \a local ID is no more modified by this call,
 * all extra data are stored in its temp \a storage_id copy.
 */
ID *BKE_lib_override_library_operations_store_start(Main *bmain,
                                                    OverrideLibraryStorage *liboverride_storage,
                                                    ID *local);
/**
 * Restore given ID modified by #BKE_lib_override_library_operations_store_start, to its
 * original state.
 */
void BKE_lib_override_library_operations_store_end(OverrideLibraryStorage *liboverride_storage,
                                                   ID *local);
void BKE_lib_override_library_operations_store_finalize(
    OverrideLibraryStorage *liboverride_storage);

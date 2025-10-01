/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include <list>
#include <string>

#include "BLI_bit_vector.hh"
#include "BLI_function_ref.hh"
#include "BLI_map.hh"

#include "BLO_readfile.hh"

struct BlendHandle;
struct ID;
struct Library;
struct LibraryLink_Params;
struct MainLibraryWeakReferenceMap;
struct ReportList;

/* TODO: Rename file to `BKE_blendfile_import.hh`. */
/* TODO: Replace `BlendfileLinkAppend` prefix by `blender::bke::blendfile::import` namespace. */
/* TODO: Move these enums to scoped enum classes. */

/** Actions to apply to an item (i.e. linked ID). */
enum {
  LINK_APPEND_ACT_UNSET = 0,
  LINK_APPEND_ACT_KEEP_LINKED,
  LINK_APPEND_ACT_REUSE_LOCAL,
  LINK_APPEND_ACT_MAKE_LOCAL,
  LINK_APPEND_ACT_COPY_LOCAL,
};

/** Various status info about an item (i.e. linked ID). */
enum {
  /** An indirectly linked ID. */
  LINK_APPEND_TAG_INDIRECT = 1 << 0,
  /**
   * An ID also used as liboverride dependency (either directly, as a liboverride reference, or
   * indirectly, as data used by a liboverride reference). It should never be directly made local.
   *
   * Mutually exclusive with #LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY_ONLY.
   */
  LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY = 1 << 1,
  /**
   * An ID only used as liboverride dependency (either directly or indirectly, see
   * #LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY for precisions). It should not be considered during
   * the 'make local' process, and remain purely linked data.
   *
   * Mutually exclusive with #LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY.
   */
  LINK_APPEND_TAG_LIBOVERRIDE_DEPENDENCY_ONLY = 1 << 2,
};

/* NOTE: These three structs are currently exposed in header to allow for their usage in RNA.
 * Regular C++ code should not access their content directly.
 *
 * TODO: Refactor these three structs into classes, and integrated the whole API into them. */
struct BlendfileLinkAppendContext;

/** A data-block (ID) entry in the `items` list from #BlendfileLinkAppendContext. */
struct BlendfileLinkAppendContextItem {
  /**
   * Link/Append context owner of this item. Used in RNA API, could be removed once RNA paths are
   * functional.
   */
  BlendfileLinkAppendContext *lapp_context;

  /** Name of the ID (without the heading two-chars IDcode). */
  std::string name;
  /** All libraries (from #BlendfileLinkAppendContext.libraries) to try to load this ID from. */
  blender::BitVector<> libraries;
  /** ID type. */
  short idcode;

  /**
   * Type of action to perform on this item, and general status tag information.
   * NOTE: Mostly used by append post-linking processing.
   */
  char action;
  char tag;

  /** Newly linked ID (nullptr until it has been successfully linked). */
  ID *new_id;
  /**
   * Library ID from which the #new_id has been linked
   * (nullptr until it has been successfully linked).
   */
  Library *source_library;
  /**
   * Liboverride of the linked ID
   * (nullptr until it has been successfully created or an existing one has been found).
   */
  ID *liboverride_id;
  /**
   * Whether the item has a matching local ID that was already appended from the same source
   * before, and has not been modified. In 'Append & Reuse' case, this local ID _may_ be reused
   * instead of making linked data local again.
   */
  ID *reusable_local_id;

  /** Opaque user data pointer. */
  void *userdata;
};

/** A blend-file library entry in the `libraries` vector from #BlendfileLinkAppendContext. */
struct BlendfileLinkAppendContextLibrary {
  /** Absolute .blend file path. */
  std::string path;
  /** Blend file handle, if any. */
  BlendHandle *blo_handle;
  /** Whether the blend file handle is owned, or borrowed. */
  bool blo_handle_is_owned;
  /** The blend-file report associated with the `blo_handle`, if owned. */
  BlendFileReadReport bf_reports;
};

/**
 * General container for all relevant data for a library/linked-data related operation (linking,
 * appending, library relocating, etc.).
 */
struct BlendfileLinkAppendContext {
  /** List of library paths to search IDs in. */
  blender::Vector<BlendfileLinkAppendContextLibrary> libraries;
  /**
   * List of all ID to try to link from #libraries. This is a linked list because iterators must
   * not be invalidated when adding more items.
   */
  std::list<BlendfileLinkAppendContextItem> items;
  using items_iterator_t = std::list<BlendfileLinkAppendContextItem>::iterator;
  /** Linking/appending parameters. Including `bmain`, `scene`, `viewlayer` and `view3d`. */
  LibraryLink_Params *params = nullptr;

  /**
   * What is the current stage of the link/append process. Used mainly by the RNA wrappers for the
   * pre/post handlers currently.
   */
  enum class ProcessStage {
    /**
     * The context data is being filled with data (Libraries and IDs) to process. Nothing has been
     * linked yet.
     */
    Init = 0,
    /** The context data is being used to linked IDs. */
    Linking,
    /**
     * The context data is being used to append IDs (i.e. make local linked ones, or re-use already
     * existing local ones).
     */
    Appending,
    /**
     * The context data is being used to instantiate (loose) IDs (i.e. ensure that Collections,
     * Objects and/or ObjectData IDs are added to the current scene).
     */
    Instantiating,
    /**
     * All data has been linked or appended. The context state represents the final result of the
     * process.
     */
    Done,

    /* NOTE: For the time being, liboverride step is not considered here (#BKE_blendfile_override).
     * Mainly because it is only available through the BPY API currently. */
  };
  ProcessStage process_stage;

  /** Allows to easily find an existing items from an ID pointer. */
  blender::Map<ID *, BlendfileLinkAppendContextItem *> new_id_to_item;

  /** Runtime info used by append code to manage re-use of already appended matching IDs. */
  MainLibraryWeakReferenceMap *library_weak_reference_mapping = nullptr;

  /** Embedded blendfile and its size, if needed. */
  const void *blendfile_mem = nullptr;
  size_t blendfile_memsize = 0;
};

/**
 * Allocate and initialize a new context to link/append data-blocks.
 */
BlendfileLinkAppendContext *BKE_blendfile_link_append_context_new(LibraryLink_Params *params);
/**
 * Free a link/append context.
 */
void BKE_blendfile_link_append_context_free(BlendfileLinkAppendContext *lapp_context);
/**
 * Set or clear flags in given \a lapp_context.
 *
 * \param flag: A combination of:
 * - #eFileSel_Params_Flag from `DNA_space_types.h` &
 * - #eBLOLibLinkFlags * from `BLO_readfile.hh`.
 * \param do_set: Set the given \a flag if true, clear it otherwise.
 */
void BKE_blendfile_link_append_context_flag_set(BlendfileLinkAppendContext *lapp_context,
                                                int flag,
                                                bool do_set);

/**
 * Store reference to a Blender's embedded memfile into the context.
 *
 * \note This is required since embedded startup blender file is handled in `ED` module, which
 * cannot be linked in BKE code.
 */
void BKE_blendfile_link_append_context_embedded_blendfile_set(
    BlendfileLinkAppendContext *lapp_context, const void *blendfile_mem, int blendfile_memsize);
/** Clear reference to Blender's embedded startup file into the context. */
void BKE_blendfile_link_append_context_embedded_blendfile_clear(
    BlendfileLinkAppendContext *lapp_context);

/**
 * Add a new source library to search for items to be linked to the given link/append context.
 *
 * \param libname: the absolute path to the library blend file.
 * \param blo_handle: the blend file handle of the library, `nullptr` if not available. Note that
 *                    the ownership of this handle is always stolen, because readfile code may
 *                    forcefully clear this handle after reading in some cases (endianness
 *                    conversion, see usages of the #FD_FLAGS_SWITCH_ENDIAN flag).
 *
 * \note *Never* call #BKE_blendfile_link_append_context_library_add()
 * after having added some items.
 */
void BKE_blendfile_link_append_context_library_add(BlendfileLinkAppendContext *lapp_context,
                                                   const char *libname,
                                                   BlendHandle *blo_handle);
/**
 * Add a new item (data-block name and `idcode`) to be searched and linked/appended from libraries
 * associated to the given context.
 *
 * \param userdata: an opaque user-data pointer stored in generated link/append item.
 *
 * TODO: Add a more friendly version of this function that combines it with the call to
 * #BKE_blendfile_link_append_context_item_library_index_enable to enable the added item for all
 * added library sources.
 */
BlendfileLinkAppendContextItem *BKE_blendfile_link_append_context_item_add(
    BlendfileLinkAppendContext *lapp_context, const char *idname, short idcode, void *userdata);

#define BLENDFILE_LINK_APPEND_INVALID -1
/**
 * Search for all ID matching given `id_types_filter` in given `library_index`, and add them to
 * the list of items to process.
 *
 * \note #BKE_blendfile_link_append_context_library_add should never be called on the same
 *`lapp_context` after this function.
 *
 * \param id_types_filter: A set of `FILTER_ID` bit-flags, the types of IDs to add to the items
 *                         list.
 * \param library_index: The index of the library to look into, in given `lapp_context`.
 *
 * \return The number of items found and added to the list, or `BLENDFILE_LINK_APPEND_INVALID` if
 *         it could not open the .blend file.
 */
int BKE_blendfile_link_append_context_item_idtypes_from_library_add(
    BlendfileLinkAppendContext *lapp_context,
    ReportList *reports,
    uint64_t id_types_filter,
    int library_index);

/**
 * Enable search of the given \a item into the library stored at given index in the link/append
 * context.
 */
void BKE_blendfile_link_append_context_item_library_index_enable(
    BlendfileLinkAppendContext *lapp_context,
    BlendfileLinkAppendContextItem *item,
    int library_index);
/**
 * Check if given link/append context is empty (has no items to process) or not.
 */
bool BKE_blendfile_link_append_context_is_empty(BlendfileLinkAppendContext *lapp_context);

void *BKE_blendfile_link_append_context_item_userdata_get(BlendfileLinkAppendContext *lapp_context,
                                                          BlendfileLinkAppendContextItem *item);
ID *BKE_blendfile_link_append_context_item_newid_get(BlendfileLinkAppendContext *lapp_context,
                                                     BlendfileLinkAppendContextItem *item);
/**
 * Replace the newly linked ID by another from the same library. Rarely used, necessary e.g. in
 * some complex 'do version after setup' code when an ID is replaced by another one.
 */
void BKE_blendfile_link_append_context_item_newid_set(BlendfileLinkAppendContext *lapp_context,
                                                      BlendfileLinkAppendContextItem *item,
                                                      ID *new_id);
ID *BKE_blendfile_link_append_context_item_liboverrideid_get(
    BlendfileLinkAppendContext *lapp_context, BlendfileLinkAppendContextItem *item);
short BKE_blendfile_link_append_context_item_idcode_get(BlendfileLinkAppendContext *lapp_context,
                                                        BlendfileLinkAppendContextItem *item);

enum eBlendfileLinkAppendForeachItemFlag {
  /** Loop over directly linked items (i.e. those explicitly defined by user code). */
  BKE_BLENDFILE_LINK_APPEND_FOREACH_ITEM_FLAG_DO_DIRECT = 1 << 0,
  /**
   * Loop over indirectly linked items (i.e. those defined by internal code, as dependencies of
   * direct ones).
   *
   * IMPORTANT: Those 'indirect' items currently may not cover **all** indirectly linked data.
   * See comments in #foreach_libblock_link_append_callback.
   */
  BKE_BLENDFILE_LINK_APPEND_FOREACH_ITEM_FLAG_DO_INDIRECT = 1 << 1,
};

/**
 * Iterate over all (or a subset) of the items listed in given #BlendfileLinkAppendContext,
 * and call the `callback_function` on them.
 *
 * \param flag: Control which type of items to process (see
 * #eBlendfileLinkAppendForeachItemFlag enum flags).
 * \param userdata: An opaque void pointer passed to the `callback_function`.
 */
void BKE_blendfile_link_append_context_item_foreach(
    BlendfileLinkAppendContext *lapp_context,
    /**
     * Called over each (or a subset of each) of the items in given #BlendfileLinkAppendContext.
     *
     * \return `true` if iteration should continue, `false` otherwise.
     */
    blender::FunctionRef<bool(BlendfileLinkAppendContext *lapp_context,
                              BlendfileLinkAppendContextItem *item)> callback_function,
    eBlendfileLinkAppendForeachItemFlag flag);

/**
 * Called once the link/append process has been fully initialized (all of its data has been set).
 *
 * NOTE: Currently only used to call the matching handler.
 */
void BKE_blendfile_link_append_context_init_done(BlendfileLinkAppendContext *lapp_context);

/**
 * Perform linking operation on all items added to given `lapp_context`.
 */
void BKE_blendfile_link(BlendfileLinkAppendContext *lapp_context, ReportList *reports);

/**
 * Perform packing operation.
 *
 * The IDs processed by this functions are the one that have been linked by a previous call to
 * #BKE_blendfile_link on the same `lapp_context`.
 */
void BKE_blendfile_link_pack(BlendfileLinkAppendContext *lapp_context, ReportList *reports);

/**
 * Perform append operation, using modern ID usage looper to detect which ID should be kept
 * linked, made local, duplicated as local, re-used from local etc.
 *
 * The IDs processed by this functions are the one that have been linked by a previous call to
 * #BKE_blendfile_link on the same `lapp_context`.
 */
void BKE_blendfile_append(BlendfileLinkAppendContext *lapp_context, ReportList *reports);

/**
 * Instantiate loose data in the scene (e.g. add object to the active collection).
 */
void BKE_blendfile_link_append_instantiate_loose(BlendfileLinkAppendContext *lapp_context,
                                                 ReportList *reports);

/**
 * Finalize the link/append process.
 *
 * NOTE: Currently only used to call the matching handler..
 */
void BKE_blendfile_link_append_context_finalize(BlendfileLinkAppendContext *lapp_context);

/**
 * Options controlling the behavior of liboverrides creation.
 */
enum eBKELibLinkOverride {
  BKE_LIBLINK_OVERRIDE_INIT = 0,

  /**
   * Try to find a matching existing liboverride first, instead of always creating a new one.
   *
   * \note Takes into account the #BKE_LIBLINK_CREATE_RUNTIME flag too (i.e. only checks for
   *       runtime liboverrides if that flag is set, and vice-versa).
   */
  BKE_LIBLINK_OVERRIDE_USE_EXISTING_LIBOVERRIDES = 1 << 0,
  /**
   * Create (or return an existing) runtime liboverride, instead of a regular saved-in-blend-files
   * one. See also the #ID_TAG_RUNTIME tag of IDs in DNA_ID.h.
   *
   * \note Typically, usage of this flag implies that no linked IDs are instantiated, such that
   * their usages remain indirect.
   */
  BKE_LIBLINK_OVERRIDE_CREATE_RUNTIME = 1 << 1,
};

/**
 * Create (or find existing) liboverrides from linked data.
 *
 * The IDs processed by this functions are the one that have been linked by a previous call to
 * #BKE_blendfile_link on the same `lapp_context`.
 *
 * Control over how liboverrides are created is done through the extra #eBKELibLinkOverride flags.
 *
 * \warning Currently this function only performs very (very!) basic liboverrides, with no handling
 * of dependencies or hierarchies. It is not expected to be directly exposed to users in its
 * current state, but rather as a helper for specific use-cases like 'presets assets' handling.
 */
void BKE_blendfile_override(BlendfileLinkAppendContext *lapp_context,
                            const eBKELibLinkOverride flags,
                            ReportList *reports);

/**
 * Try to relocate all linked IDs added to `lapp_context`, belonging to the given `library`.
 *
 * This function searches for matching IDs (type and name) in all libraries added to the given
 * `lapp_context`.
 *
 * Typical usages include:
 * - Relocating a library:
 *   - Add the new target library path to `lapp_context`.
 *   - Add all IDs from the library to relocate to `lapp_context`
 *   - Mark the new target library to be considered for each ID.
 *   - Call this function.
 *
 * - Searching for (e.g.missing) linked IDs in a set or sub-set of libraries:
 *   - Add all potential library sources paths to `lapp_context`.
 *   - Add all IDs to search for to `lapp_context`.
 *   - Mark which libraries should be considered for each ID.
 *   - Call this function.
 *
 * NOTE: content of `lapp_context` after execution of that function should not be assumed valid
 * anymore, and should immediately be freed.
 */
void BKE_blendfile_library_relocate(BlendfileLinkAppendContext *lapp_context,
                                    ReportList *reports,
                                    Library *library,
                                    bool do_reload);

/**
 * Relocate a single linked ID.
 *
 * NOTE: content of `lapp_context` after execution of that function should not be assumed valid
 * anymore, and should immediately be freed.
 */
void BKE_blendfile_id_relocate(BlendfileLinkAppendContext &lapp_context, ReportList *reports);

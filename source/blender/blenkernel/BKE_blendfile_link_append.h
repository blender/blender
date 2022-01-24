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
#pragma once

/** \file
 * \ingroup bke
 */

#ifdef __cplusplus
extern "C" {
#endif

struct BlendHandle;
struct ID;
struct Library;
struct LibraryLink_Params;
struct Main;
struct ReportList;
struct Scene;
struct View3D;
struct ViewLayer;

typedef struct BlendfileLinkAppendContext BlendfileLinkAppendContext;
typedef struct BlendfileLinkAppendContextItem BlendfileLinkAppendContextItem;

/**
 * Allocate and initialize a new context to link/append data-blocks.
 */
BlendfileLinkAppendContext *BKE_blendfile_link_append_context_new(
    struct LibraryLink_Params *params);
/**
 * Free a link/append context.
 */
void BKE_blendfile_link_append_context_free(struct BlendfileLinkAppendContext *lapp_context);
/**
 * Set or clear flags in given \a lapp_context.
 *
 * \param flag: A combination of:
 * - #eFileSel_Params_Flag from `DNA_space_types.h` &
 * - #eBLOLibLinkFlags * from `BLO_readfile.h`.
 * \param do_set: Set the given \a flag if true, clear it otherwise.
 */
void BKE_blendfile_link_append_context_flag_set(struct BlendfileLinkAppendContext *lapp_context,
                                                int flag,
                                                bool do_set);

/**
 * Store reference to a Blender's embedded memfile into the context.
 *
 * \note This is required since embedded startup blender file is handled in `ED` module, which
 * cannot be linked in BKE code.
 */
void BKE_blendfile_link_append_context_embedded_blendfile_set(
    struct BlendfileLinkAppendContext *lapp_context,
    const void *blendfile_mem,
    int blendfile_memsize);
/** Clear reference to Blender's embedded startup file into the context. */
void BKE_blendfile_link_append_context_embedded_blendfile_clear(
    struct BlendfileLinkAppendContext *lapp_context);

/**
 * Add a new source library to search for items to be linked to the given link/append context.
 *
 * \param libname: the absolute path to the library blend file.
 * \param blo_handle: the blend file handle of the library, NULL is not available. Note that this
 *                    is only borrowed for linking purpose, no releasing or other management will
 *                    be performed by #BKE_blendfile_link_append code on it.
 *
 * \note *Never* call #BKE_blendfile_link_append_context_library_add()
 * after having added some items.
 */
void BKE_blendfile_link_append_context_library_add(struct BlendfileLinkAppendContext *lapp_context,
                                                   const char *libname,
                                                   struct BlendHandle *blo_handle);
/**
 * Add a new item (data-block name and `idcode`) to be searched and linked/appended from libraries
 * associated to the given context.
 *
 * \param userdata: an opaque user-data pointer stored in generated link/append item.
 *
 * TODO: Add a more friendly version of this that combines it with the call to
 * #BKE_blendfile_link_append_context_item_library_index_enable to enable the added item for all
 * added library sources.
 */
struct BlendfileLinkAppendContextItem *BKE_blendfile_link_append_context_item_add(
    struct BlendfileLinkAppendContext *lapp_context,
    const char *idname,
    short idcode,
    void *userdata);

#define BLENDFILE_LINK_APPEND_INVALID -1
/**
 * Search for all ID matching given `id_types_filter` in given `library_index`, and add them to
 * the list of items to process.
 *
 * \note #BKE_blendfile_link_append_context_library_add should never be called on the same
 *`lapp_context` after this function.
 *
 * \param id_types_filter: A set of `FILTER_ID` bitflags, the types of IDs to add to the items
 *                         list.
 * \param library_index: The index of the library to look into, in given `lapp_context`.
 *
 * \return The number of items found and added to the list, or `BLENDFILE_LINK_APPEND_INVALID` if
 *         it could not open the .blend file.
 */
int BKE_blendfile_link_append_context_item_idtypes_from_library_add(
    struct BlendfileLinkAppendContext *lapp_context,
    struct ReportList *reports,
    uint64_t id_types_filter,
    int library_index);

/**
 * Enable search of the given \a item into the library stored at given index in the link/append
 * context.
 */
void BKE_blendfile_link_append_context_item_library_index_enable(
    struct BlendfileLinkAppendContext *lapp_context,
    struct BlendfileLinkAppendContextItem *item,
    int library_index);
/**
 * Check if given link/append context is empty (has no items to process) or not.
 */
bool BKE_blendfile_link_append_context_is_empty(struct BlendfileLinkAppendContext *lapp_context);

void *BKE_blendfile_link_append_context_item_userdata_get(
    struct BlendfileLinkAppendContext *lapp_context, struct BlendfileLinkAppendContextItem *item);
struct ID *BKE_blendfile_link_append_context_item_newid_get(
    struct BlendfileLinkAppendContext *lapp_context, struct BlendfileLinkAppendContextItem *item);
short BKE_blendfile_link_append_context_item_idcode_get(
    struct BlendfileLinkAppendContext *lapp_context, struct BlendfileLinkAppendContextItem *item);

typedef enum eBlendfileLinkAppendForeachItemFlag {
  /** Loop over directly linked items (i.e. those explicitly defined by user code). */
  BKE_BLENDFILE_LINK_APPEND_FOREACH_ITEM_FLAG_DO_DIRECT = 1 << 0,
  /** Loop over indirectly linked items (i.e. those defined by internal code, as dependencies of
   * direct ones).
   *
   * IMPORTANT: Those 'indirect' items currently may not cover **all** indirectly linked data.
   * See comments in #foreach_libblock_link_append_callback. */
  BKE_BLENDFILE_LINK_APPEND_FOREACH_ITEM_FLAG_DO_INDIRECT = 1 << 1,
} eBlendfileLinkAppendForeachItemFlag;
/**
 * Callback called by #BKE_blendfile_link_append_context_item_foreach over each (or a subset of
 * each) of the items in given #BlendfileLinkAppendContext.
 *
 * \param userdata: An opaque void pointer passed to the `callback_function`.
 *
 * \return `true` if iteration should continue, `false` otherwise.
 */
typedef bool (*BKE_BlendfileLinkAppendContexteItemFunction)(
    struct BlendfileLinkAppendContext *lapp_context,
    struct BlendfileLinkAppendContextItem *item,
    void *userdata);
/**
 * Iterate over all (or a subset) of the items listed in given #BlendfileLinkAppendContext,
 * and call the `callback_function` on them.
 *
 * \param flag: Control which type of items to process (see
 * #eBlendfileLinkAppendForeachItemFlag enum flags).
 * \param userdata: An opaque void pointer passed to the `callback_function`.
 */
void BKE_blendfile_link_append_context_item_foreach(
    struct BlendfileLinkAppendContext *lapp_context,
    BKE_BlendfileLinkAppendContexteItemFunction callback_function,
    eBlendfileLinkAppendForeachItemFlag flag,
    void *userdata);

/**
 * Perform append operation, using modern ID usage looper to detect which ID should be kept
 * linked, made local, duplicated as local, re-used from local etc.
 *
 * The IDs processed by this functions are the one that have been linked by a previous call to
 * #BKE_blendfile_link on the same `lapp_context`.
 */
void BKE_blendfile_append(struct BlendfileLinkAppendContext *lapp_context,
                          struct ReportList *reports);
/**
 * Perform linking operation on all items added to given `lapp_context`.
 */
void BKE_blendfile_link(struct BlendfileLinkAppendContext *lapp_context,
                        struct ReportList *reports);

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
 */
void BKE_blendfile_library_relocate(struct BlendfileLinkAppendContext *lapp_context,
                                    struct ReportList *reports,
                                    struct Library *library,
                                    bool do_reload);

#ifdef __cplusplus
}
#endif

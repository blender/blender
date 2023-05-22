/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct ARegion;
struct ARegionType;
struct AssetShelfSettings;
struct bContext;
struct bContextDataResult;
struct BlendDataReader;
struct BlendWriter;
struct wmWindowManager;
struct RegionPollParams;

/* -------------------------------------------------------------------- */
/* Asset Shelf Regions */

bool ED_asset_shelf_regions_poll(const struct RegionPollParams *params);

/** Only needed for #RGN_TYPE_ASSET_SHELF (not #RGN_TYPE_ASSET_SHELF_FOOTER). */
void ED_asset_shelf_region_init(struct wmWindowManager *wm, struct ARegion *region);
int ED_asset_shelf_region_snap(const struct ARegion *region, int size, int axis);
void ED_asset_shelf_region_listen(const struct wmRegionListenerParams *params);
void ED_asset_shelf_region_layout(const bContext *C,
                                  struct ARegion *region,
                                  struct AssetShelfHook *shelf_hook);
void ED_asset_shelf_region_draw(const bContext *C, struct ARegion *region);
int ED_asset_shelf_default_tile_width(void);
int ED_asset_shelf_default_tile_height(void);
int ED_asset_shelf_region_prefsizey(void);

void ED_asset_shelf_footer_region_init(struct wmWindowManager *wm, struct ARegion *region);
void ED_asset_shelf_footer_region(const struct bContext *C, struct ARegion *region);
void ED_asset_shelf_footer_region_listen(const struct wmRegionListenerParams *params);
int ED_asset_shelf_footer_size(void);
void ED_asset_shelf_footer_register(struct ARegionType *region_type, const int space_type);

/* -------------------------------------------------------------------- */
/* Asset Shelf Hook */

/**
 * Deep-copies \a hook into newly allocated memory. Must be freed using
 * #ED_asset_shelf_hook_free().
 */
struct AssetShelfHook *ED_asset_shelf_hook_duplicate(const AssetShelfHook *hook);
/**
 * Frees the contained data and \a hook itself.
 */
void ED_asset_shelf_hook_free(AssetShelfHook **hook);

void ED_asset_shelf_hook_blend_write(struct BlendWriter *writer,
                                     const struct AssetShelfHook *hook);
void ED_asset_shelf_hook_blend_read_data(struct BlendDataReader *reader,
                                         struct AssetShelfHook **hook);

/* -------------------------------------------------------------------- */

/**
 * Creates an `"asset_shelf"` context member, pointing to the active shelf in \a #shelf_hook.
 */
int ED_asset_shelf_context(const struct bContext *C,
                           const char *member,
                           struct bContextDataResult *result,
                           struct AssetShelfHook *shelf_hook);

#ifdef __cplusplus
}
#endif

/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

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

bool ED_asset_shelf_poll(const struct RegionPollParams *params);

/** Only needed for #RGN_TYPE_ASSET_SHELF (not #RGN_TYPE_ASSET_SHELF_FOOTER). */
void ED_asset_shelf_region_listen(const struct wmRegionListenerParams *params);
void ED_asset_shelf_region_draw(const bContext *C, struct ARegion *region);
void ED_asset_shelf_region_register(ARegionType *region_type,
                                    const char *idname,
                                    const int space_type);

void ED_asset_shelf_footer_region_init(struct wmWindowManager *wm, struct ARegion *region);
void ED_asset_shelf_footer_region(const struct bContext *C, struct ARegion *region);
void ED_asset_shelf_footer_region_listen(const struct wmRegionListenerParams *params);
void ED_asset_shelf_footer_register(struct ARegionType *region_type,
                                    const char *idname,
                                    const int space_type);

/* -------------------------------------------------------------------- */
/* Asset Shelf Settings */

/**
 * Deep-copies \a shelf_settings into newly allocated memory. Must be freed using #MEM_freeN() or
 * #MEM_delete().
 */
AssetShelfSettings *ED_asset_shelf_settings_duplicate(const AssetShelfSettings *shelf_settings);
/**
 * Frees the contained data, not \a shelf_settings itself.
 */
void ED_asset_shelf_settings_free(AssetShelfSettings *shelf_settings);

void ED_asset_shelf_settings_blend_write(struct BlendWriter *writer,
                                         const struct AssetShelfSettings *storage);
void ED_asset_shelf_settings_blend_read_data(struct BlendDataReader *reader,
                                             struct AssetShelfSettings **storage);

/* -------------------------------------------------------------------- */

/**
 * Creates an `"asset_shelf_settings"` context member, pointing to \a shelf_settings.
 */
int ED_asset_shelf_context(const struct bContext *C,
                           const char *member,
                           struct bContextDataResult *result,
                           struct AssetShelfSettings *shelf_settings);

#ifdef __cplusplus
}
#endif

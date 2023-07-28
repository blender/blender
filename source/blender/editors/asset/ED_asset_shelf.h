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
/** \name Asset Shelf Regions
 *
 * Naming conventions:
 * - #ED_asset_shelf_regions_xxx(): Applies to both regions (#RGN_TYPE_ASSET_SHELF and
 *   #RGN_TYPE_ASSET_SHELF_HEADER).
 * - #ED_asset_shelf_region_xxx(): Applies to the main shelf region (#RGN_TYPE_ASSET_SHELF).
 * - #ED_asset_shelf_header_region_xxx(): Applies to the shelf header region
 *   (#RGN_TYPE_ASSET_SHELF_HEADER).
 *
 * \{ */

bool ED_asset_shelf_regions_poll(const struct RegionPollParams *params);

/** Only needed for #RGN_TYPE_ASSET_SHELF (not #RGN_TYPE_ASSET_SHELF_HEADER). */
void *ED_asset_shelf_region_duplicate(void *regiondata);
void ED_asset_shelf_region_free(struct ARegion *region);
void ED_asset_shelf_region_init(struct wmWindowManager *wm, struct ARegion *region);
int ED_asset_shelf_region_snap(const struct ARegion *region, int size, int axis);
void ED_asset_shelf_region_listen(const struct wmRegionListenerParams *params);
void ED_asset_shelf_region_layout(const bContext *C, struct ARegion *region);
void ED_asset_shelf_region_draw(const bContext *C, struct ARegion *region);
void ED_asset_shelf_region_blend_read_data(BlendDataReader *reader, struct ARegion *region);
void ED_asset_shelf_region_blend_write(BlendWriter *writer, struct ARegion *region);
int ED_asset_shelf_region_prefsizey(void);

void ED_asset_shelf_header_region_init(struct wmWindowManager *wm, struct ARegion *region);
void ED_asset_shelf_header_region(const struct bContext *C, struct ARegion *region);
void ED_asset_shelf_header_region_listen(const struct wmRegionListenerParams *params);
int ED_asset_shelf_header_region_size(void);
void ED_asset_shelf_header_regiontype_register(struct ARegionType *region_type,
                                               const int space_type);

/** \} */

/* -------------------------------------------------------------------- */

int ED_asset_shelf_tile_width(const struct AssetShelfSettings &settings);
int ED_asset_shelf_tile_height(const struct AssetShelfSettings &settings);

int ED_asset_shelf_context(const struct bContext *C,
                           const char *member,
                           struct bContextDataResult *result);

#ifdef __cplusplus
}
#endif

/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

struct ARegion;
struct ARegionType;
struct AssetShelfSettings;
struct AssetShelfType;
struct bContext;
struct bContextDataResult;
struct BlendDataReader;
struct BlendWriter;
struct Main;
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

bool ED_asset_shelf_regions_poll(const RegionPollParams *params);

/** Only needed for #RGN_TYPE_ASSET_SHELF (not #RGN_TYPE_ASSET_SHELF_HEADER). */
void *ED_asset_shelf_region_duplicate(void *regiondata);
void ED_asset_shelf_region_free(ARegion *region);
void ED_asset_shelf_region_init(wmWindowManager *wm, ARegion *region);
int ED_asset_shelf_region_snap(const ARegion *region, int size, int axis);
void ED_asset_shelf_region_on_user_resize(const ARegion *region);
void ED_asset_shelf_region_listen(const wmRegionListenerParams *params);
void ED_asset_shelf_region_layout(const bContext *C, ARegion *region);
void ED_asset_shelf_region_draw(const bContext *C, ARegion *region);
void ED_asset_shelf_region_blend_read_data(BlendDataReader *reader, ARegion *region);
void ED_asset_shelf_region_blend_write(BlendWriter *writer, ARegion *region);
int ED_asset_shelf_region_prefsizey(void);

void ED_asset_shelf_header_region_init(wmWindowManager *wm, ARegion *region);
void ED_asset_shelf_header_region(const bContext *C, ARegion *region);
void ED_asset_shelf_header_region_listen(const wmRegionListenerParams *params);
int ED_asset_shelf_header_region_size(void);
void ED_asset_shelf_header_regiontype_register(ARegionType *region_type, const int space_type);

/** \} */

/* -------------------------------------------------------------------- */

void ED_asset_shelf_type_unlink(const Main &bmain, const AssetShelfType &shelf_type);

int ED_asset_shelf_tile_width(const AssetShelfSettings &settings);
int ED_asset_shelf_tile_height(const AssetShelfSettings &settings);

int ED_asset_shelf_context(const bContext *C, const char *member, bContextDataResult *result);

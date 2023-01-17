/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

struct ARegionType;
struct bContext;
struct wmWindowManager;

void ED_region_asset_shelf_footer_init(struct wmWindowManager *wm, struct ARegion *region);
void ED_region_asset_shelf_footer(const struct bContext *C, struct ARegion *region);

void ED_region_asset_shelf_listen(const struct wmRegionListenerParams *params);

void ED_asset_shelf_footer_register(struct ARegionType *region_type,
                                    const char *idname,
                                    const int space_type);

#ifdef __cplusplus
}
#endif

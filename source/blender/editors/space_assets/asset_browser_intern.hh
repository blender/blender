/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spassets
 */

#pragma once

void asset_browser_operatortypes();

struct ARegion;
struct ARegionType;
struct AssetLibrary;
struct bContext;
struct PointerRNA;
struct PropertyRNA;
struct uiLayout;
struct wmMsgBus;

void asset_browser_main_region_draw(const bContext *C, ARegion *region);

void asset_browser_navigation_region_panels_register(ARegionType *art);

void asset_view_create_catalog_tree_view_in_layout(::AssetLibrary *asset_library,
                                                   uiLayout *layout,
                                                   PointerRNA *catalog_filter_owner_ptr,
                                                   PropertyRNA *catalog_filter_prop,
                                                   wmMsgBus *msg_bus);

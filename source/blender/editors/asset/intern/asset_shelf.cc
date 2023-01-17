/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edasset
 */

#include "BKE_screen.h"

#include "DNA_screen_types.h"

#include "ED_asset_list.h"
#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "ED_asset_shelf.h"

static void asset_shelf_draw(const bContext * /*C*/, Header *header)
{
  uiLayout *layout = header->layout;
  uiItemL(layout, "Fooo\n", ICON_ASSET_MANAGER);
}

void ED_region_asset_shelf_listen(const wmRegionListenerParams *params)
{
  if (ED_assetlist_listen(params->notifier)) {
    ED_region_tag_redraw_no_rebuild(params->region);
  }
}

void ED_region_asset_shelf_footer_init(wmWindowManager * /*wm*/, ARegion *region)
{
  ED_region_header_init(region);
}

void ED_region_asset_shelf_footer(const bContext *C, ARegion *region)
{
  ED_region_header(C, region);
}

void ED_asset_shelf_footer_register(ARegionType *region_type,
                                    const char *idname,
                                    const int space_type)
{
  HeaderType *ht = MEM_cnew<HeaderType>(__func__);
  strcpy(ht->idname, idname);
  ht->space_type = space_type;
  ht->region_type = RGN_TYPE_ASSET_SHELF_FOOTER;
  ht->draw = asset_shelf_draw;
  BLI_addtail(&region_type->headertypes, ht);
}

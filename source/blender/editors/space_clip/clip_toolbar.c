/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2011 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup spclip
 */

#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "WM_types.h"

#include "ED_screen.h"

#include "clip_intern.h" /* own include */

/* ************************ header area region *********************** */

/************************** properties ******************************/

ARegion *ED_clip_has_properties_region(ScrArea *area)
{
  ARegion *region, *arnew;

  region = BKE_area_find_region_type(area, RGN_TYPE_UI);
  if (region) {
    return region;
  }

  /* add subdiv level; after header */
  region = BKE_area_find_region_type(area, RGN_TYPE_HEADER);

  /* is error! */
  if (region == NULL) {
    return NULL;
  }

  arnew = MEM_callocN(sizeof(ARegion), "clip properties region");

  BLI_insertlinkafter(&area->regionbase, region, arnew);
  arnew->regiontype = RGN_TYPE_UI;
  arnew->alignment = RGN_ALIGN_RIGHT;

  arnew->flag = RGN_FLAG_HIDDEN;

  return arnew;
}

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
 *
 * The Original Code is Copyright (C) 2011 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spclip
 */

#include <string.h>

#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_screen.h"
#include "ED_undo.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "clip_intern.h" /* own include */

/* ************************ header area region *********************** */

/************************** properties ******************************/

ARegion *ED_clip_has_properties_region(ScrArea *sa)
{
  ARegion *region, *arnew;

  region = BKE_area_find_region_type(sa, RGN_TYPE_UI);
  if (region) {
    return region;
  }

  /* add subdiv level; after header */
  region = BKE_area_find_region_type(sa, RGN_TYPE_HEADER);

  /* is error! */
  if (region == NULL) {
    return NULL;
  }

  arnew = MEM_callocN(sizeof(ARegion), "clip properties region");

  BLI_insertlinkafter(&sa->regionbase, region, arnew);
  arnew->regiontype = RGN_TYPE_UI;
  arnew->alignment = RGN_ALIGN_RIGHT;

  arnew->flag = RGN_FLAG_HIDDEN;

  return arnew;
}

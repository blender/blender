/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation */

/** \file
 * \ingroup collada
 */

#include <assert.h>
#include <string.h>

#include "io_ops.h" /* own include */

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_listbase.h"

#include "BKE_context.h"
#include "BKE_screen.h"
#include "WM_api.h"

#ifdef WITH_COLLADA
#  include "io_collada.h"
#endif

#ifdef WITH_ALEMBIC
#  include "io_alembic.h"
#endif

#ifdef WITH_USD
#  include "io_usd.h"
#endif

#include "io_cache.h"
#include "io_gpencil.h"
#include "io_obj.h"
#include "io_ply_ops.h"
#include "io_stl_ops.h"


bool IO_paneltype_set_parent(struct PanelType *panel) {
  PanelType *parent = NULL;

  SpaceType *space_type = BKE_spacetype_from_id(SPACE_FILE);
  assert(space_type);

  ARegionType *region = BKE_regiontype_from_id(space_type, RGN_TYPE_TOOL_PROPS);
  assert(region);

  LISTBASE_FOREACH (PanelType *, pt, &region->paneltypes) {
    if (strcasecmp(pt->idname, panel->parent_id) == 0) {
      parent = pt;
      break;
    }
  }

  if (parent) {
    panel->parent = parent;
    LinkData *pt_child_iter = parent->children.last;
    for (; pt_child_iter; pt_child_iter = pt_child_iter->prev) {
      PanelType *pt_child = pt_child_iter->data;
      if (pt_child->order <= panel->order) {
        break;
      }
    }
    BLI_insertlinkafter(&parent->children, pt_child_iter, BLI_genericNodeN(panel));
    return true;
  }

  return false;
}


void ED_operatortypes_io(void)
{
#ifdef WITH_COLLADA
  /* Collada operators: */
  WM_operatortype_append(WM_OT_collada_export);
  WM_operatortype_append(WM_OT_collada_import);
#endif
#ifdef WITH_ALEMBIC
  WM_operatortype_append(WM_OT_alembic_import);
  WM_operatortype_append(WM_OT_alembic_export);
#endif
#ifdef WITH_USD
  WM_operatortype_append(WM_OT_usd_import);
  WM_operatortype_append(WM_OT_usd_export);

  WM_PT_USDExportPanelsRegister();
  WM_PT_USDImportPanelsRegister();
#endif

#ifdef WITH_IO_GPENCIL
  WM_operatortype_append(WM_OT_gpencil_import_svg);
#  ifdef WITH_PUGIXML
  WM_operatortype_append(WM_OT_gpencil_export_svg);
#  endif
#  ifdef WITH_HARU
  WM_operatortype_append(WM_OT_gpencil_export_pdf);
#  endif
#endif

  WM_operatortype_append(CACHEFILE_OT_open);
  WM_operatortype_append(CACHEFILE_OT_reload);

  WM_operatortype_append(CACHEFILE_OT_layer_add);
  WM_operatortype_append(CACHEFILE_OT_layer_remove);
  WM_operatortype_append(CACHEFILE_OT_layer_move);

#ifdef WITH_IO_WAVEFRONT_OBJ
  WM_operatortype_append(WM_OT_obj_export);
  WM_operatortype_append(WM_OT_obj_import);
#endif

#ifdef WITH_IO_PLY
  WM_operatortype_append(WM_OT_ply_export);
  WM_operatortype_append(WM_OT_ply_import);
#endif

#ifdef WITH_IO_STL
  WM_operatortype_append(WM_OT_stl_import);
#endif
}

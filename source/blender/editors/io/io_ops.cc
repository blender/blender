/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#include "io_ops.hh" /* own include */

#include "WM_api.hh"

#ifdef WITH_COLLADA
#  include "io_collada.hh"
#endif

#ifdef WITH_ALEMBIC
#  include "io_alembic.hh"
#endif

#ifdef WITH_USD
#  include "io_usd.hh"
#endif

#include "io_cache.hh"
#include "io_gpencil.hh"
#include "io_obj.hh"
#include "io_ply_ops.hh"
#include "io_stl_ops.hh"

void ED_operatortypes_io()
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

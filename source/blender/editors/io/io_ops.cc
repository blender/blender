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
#include "io_drop_import_file.hh"
#include "io_grease_pencil.hh"
#include "io_obj.hh"
#include "io_ply_ops.hh"
#include "io_stl_ops.hh"

void ED_operatortypes_io()
{
  using namespace blender;
#ifdef WITH_COLLADA
  /* Collada operators: */
  WM_operatortype_append(WM_OT_collada_export);
  WM_operatortype_append(WM_OT_collada_import);
  ed::io::collada_file_handler_add();
#endif
#ifdef WITH_ALEMBIC
  WM_operatortype_append(WM_OT_alembic_import);
  WM_operatortype_append(WM_OT_alembic_export);
  ed::io::alembic_file_handler_add();
#endif
#ifdef WITH_USD
  WM_operatortype_append(WM_OT_usd_import);
  WM_operatortype_append(WM_OT_usd_export);
  ed::io::usd_file_handler_add();
#endif

#ifdef WITH_IO_GREASE_PENCIL
  WM_operatortype_append(WM_OT_grease_pencil_import_svg);
  ed::io::grease_pencil_file_handler_add();
#  ifdef WITH_PUGIXML
  WM_operatortype_append(WM_OT_grease_pencil_export_svg);
#  endif
#  ifdef WITH_HARU
  WM_operatortype_append(WM_OT_grease_pencil_export_pdf);
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
  ed::io::obj_file_handler_add();
#endif

#ifdef WITH_IO_PLY
  WM_operatortype_append(WM_OT_ply_export);
  WM_operatortype_append(WM_OT_ply_import);
  ed::io::ply_file_handler_add();
#endif

#ifdef WITH_IO_STL
  WM_operatortype_append(WM_OT_stl_import);
  WM_operatortype_append(WM_OT_stl_export);
  ed::io::stl_file_handler_add();
#endif
  WM_operatortype_append(WM_OT_drop_import_file);
  ED_dropbox_drop_import_file();
}

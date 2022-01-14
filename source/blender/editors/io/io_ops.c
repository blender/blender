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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup collada
 */

#include "io_ops.h" /* own include */

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
#endif

  WM_operatortype_append(WM_OT_gpencil_import_svg);

#ifdef WITH_PUGIXML
  WM_operatortype_append(WM_OT_gpencil_export_svg);
#endif

#ifdef WITH_HARU
  WM_operatortype_append(WM_OT_gpencil_export_pdf);
#endif

  WM_operatortype_append(CACHEFILE_OT_open);
  WM_operatortype_append(CACHEFILE_OT_reload);
  WM_operatortype_append(WM_OT_obj_export);
}

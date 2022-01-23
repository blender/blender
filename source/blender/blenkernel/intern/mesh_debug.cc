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
 */

/** \file
 * \ingroup bke
 *
 * Evaluated mesh info printing function, to help track down differences output.
 *
 * Output from these functions can be evaluated as Python literals.
 * See `bmesh_debug.c` for the equivalent #BMesh functionality.
 */

#ifndef NDEBUG

#  include <stdio.h>

#  include "MEM_guardedalloc.h"

#  include "DNA_mesh_types.h"
#  include "DNA_meshdata_types.h"
#  include "DNA_object_types.h"

#  include "BLI_utildefines.h"

#  include "BKE_customdata.h"

#  include "BKE_mesh.h"

#  include "BLI_dynstr.h"

static void mesh_debug_info_from_cd_flag(const Mesh *me, DynStr *dynstr)
{
  BLI_dynstr_append(dynstr, "'cd_flag': {");
  if (me->cd_flag & ME_CDFLAG_VERT_BWEIGHT) {
    BLI_dynstr_append(dynstr, "'VERT_BWEIGHT', ");
  }
  if (me->cd_flag & ME_CDFLAG_EDGE_BWEIGHT) {
    BLI_dynstr_append(dynstr, "'EDGE_BWEIGHT', ");
  }
  if (me->cd_flag & ME_CDFLAG_EDGE_CREASE) {
    BLI_dynstr_append(dynstr, "'EDGE_CREASE', ");
  }
  BLI_dynstr_append(dynstr, "},\n");
}

char *BKE_mesh_debug_info(const Mesh *me)
{
  DynStr *dynstr = BLI_dynstr_new();
  char *ret;

  const char *indent4 = "    ";
  const char *indent8 = "        ";

  BLI_dynstr_append(dynstr, "{\n");
  BLI_dynstr_appendf(dynstr, "    'ptr': '%p',\n", (void *)me);
  BLI_dynstr_appendf(dynstr, "    'totvert': %d,\n", me->totvert);
  BLI_dynstr_appendf(dynstr, "    'totedge': %d,\n", me->totedge);
  BLI_dynstr_appendf(dynstr, "    'totface': %d,\n", me->totface);
  BLI_dynstr_appendf(dynstr, "    'totpoly': %d,\n", me->totpoly);

  BLI_dynstr_appendf(dynstr, "    'runtime.deformed_only': %d,\n", me->runtime.deformed_only);
  BLI_dynstr_appendf(dynstr, "    'runtime.is_original': %d,\n", me->runtime.is_original);

  BLI_dynstr_append(dynstr, "    'vert_layers': (\n");
  CustomData_debug_info_from_layers(&me->vdata, indent8, dynstr);
  BLI_dynstr_append(dynstr, "    ),\n");

  BLI_dynstr_append(dynstr, "    'edge_layers': (\n");
  CustomData_debug_info_from_layers(&me->edata, indent8, dynstr);
  BLI_dynstr_append(dynstr, "    ),\n");

  BLI_dynstr_append(dynstr, "    'loop_layers': (\n");
  CustomData_debug_info_from_layers(&me->ldata, indent8, dynstr);
  BLI_dynstr_append(dynstr, "    ),\n");

  BLI_dynstr_append(dynstr, "    'poly_layers': (\n");
  CustomData_debug_info_from_layers(&me->pdata, indent8, dynstr);
  BLI_dynstr_append(dynstr, "    ),\n");

  BLI_dynstr_append(dynstr, "    'tessface_layers': (\n");
  CustomData_debug_info_from_layers(&me->fdata, indent8, dynstr);
  BLI_dynstr_append(dynstr, "    ),\n");

  BLI_dynstr_append(dynstr, indent4);
  mesh_debug_info_from_cd_flag(me, dynstr);

  BLI_dynstr_append(dynstr, "}\n");

  ret = BLI_dynstr_get_cstring(dynstr);
  BLI_dynstr_free(dynstr);
  return ret;
}

void BKE_mesh_debug_print(const Mesh *me)
{
  char *str = BKE_mesh_debug_info(me);
  puts(str);
  fflush(stdout);
  MEM_freeN(str);
}

#endif /* NDEBUG */

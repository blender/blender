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
 * See `mesh_debug.cc` for the equivalent #Mesh functionality.
 */

#ifndef NDEBUG

#  include <stdio.h>

#  include "MEM_guardedalloc.h"

#  include "BLI_utildefines.h"

#  include "BKE_customdata.h"

#  include "bmesh.h"

#  include "bmesh_mesh_debug.h"

#  include "BLI_dynstr.h"

char *BM_mesh_debug_info(BMesh *bm)
{
  DynStr *dynstr = BLI_dynstr_new();
  char *ret;

  const char *indent8 = "        ";

  BLI_dynstr_append(dynstr, "{\n");
  BLI_dynstr_appendf(dynstr, "    'ptr': '%p',\n", (void *)bm);
  BLI_dynstr_appendf(dynstr, "    'totvert': %d,\n", bm->totvert);
  BLI_dynstr_appendf(dynstr, "    'totedge': %d,\n", bm->totedge);
  BLI_dynstr_appendf(dynstr, "    'totface': %d,\n", bm->totface);

  BLI_dynstr_append(dynstr, "    'vert_layers': (\n");
  CustomData_debug_info_from_layers(&bm->vdata, indent8, dynstr);
  BLI_dynstr_append(dynstr, "    ),\n");

  BLI_dynstr_append(dynstr, "    'edge_layers': (\n");
  CustomData_debug_info_from_layers(&bm->edata, indent8, dynstr);
  BLI_dynstr_append(dynstr, "    ),\n");

  BLI_dynstr_append(dynstr, "    'loop_layers': (\n");
  CustomData_debug_info_from_layers(&bm->ldata, indent8, dynstr);
  BLI_dynstr_append(dynstr, "    ),\n");

  BLI_dynstr_append(dynstr, "    'poly_layers': (\n");
  CustomData_debug_info_from_layers(&bm->pdata, indent8, dynstr);
  BLI_dynstr_append(dynstr, "    ),\n");

  BLI_dynstr_append(dynstr, "}\n");

  ret = BLI_dynstr_get_cstring(dynstr);
  BLI_dynstr_free(dynstr);
  return ret;
}

void BM_mesh_debug_print(BMesh *bm)
{
  char *str = BM_mesh_debug_info(bm);
  puts(str);
  fflush(stdout);
  MEM_freeN(str);
}

#endif /* NDEBUG */

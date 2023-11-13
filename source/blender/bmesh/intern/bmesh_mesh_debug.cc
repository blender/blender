/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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

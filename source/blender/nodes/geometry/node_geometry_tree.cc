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

#include <string.h>

#include "MEM_guardedalloc.h"

#include "NOD_geometry.h"

#include "BKE_node.h"

#include "BLT_translation.h"

#include "DNA_node_types.h"

#include "RNA_access.h"

bNodeTreeType *ntreeType_Geometry;

void register_node_tree_type_geo(void)
{
  bNodeTreeType *tt = ntreeType_Geometry = static_cast<bNodeTreeType *>(
      MEM_callocN(sizeof(bNodeTreeType), "geometry node tree type"));
  tt->type = NTREE_GEOMETRY;
  strcpy(tt->idname, "GeometryNodeTree");
  strcpy(tt->ui_name, N_("Geometry Node Editor"));
  tt->ui_icon = 0; /* defined in drawnode.c */
  strcpy(tt->ui_description, N_("Geometry nodes"));
  tt->rna_ext.srna = &RNA_GeometryNodeTree;

  ntreeTypeAdd(tt);
}

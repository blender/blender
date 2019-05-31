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
 * \ingroup collada
 */

/* COLLADABU_ASSERT, may be able to remove later */
#include "COLLADABUPlatform.h"

#include "DocumentExporter.h"
#include "DocumentImporter.h"
#include "ExportSettings.h"
#include "ImportSettings.h"
#include "collada.h"

extern "C" {
#include "BKE_scene.h"
#include "BKE_context.h"
#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

/* make dummy file */
#include "BLI_fileops.h"
#include "BLI_linklist.h"

static void print_import_header(ImportSettings &import_settings)
{
  fprintf(stderr, "+-- Collada Import parameters------\n");
  fprintf(stderr, "| input file      : %s\n", import_settings.filepath);
  fprintf(stderr, "| use units       : %s\n", (import_settings.import_units) ? "yes" : "no");
  fprintf(stderr, "| autoconnect     : %s\n", (import_settings.auto_connect) ? "yes" : "no");
  fprintf(stderr, "+-- Armature Import parameters ----\n");
  fprintf(stderr, "| find bone chains: %s\n", (import_settings.find_chains) ? "yes" : "no");
  fprintf(stderr, "| min chain len   : %d\n", import_settings.min_chain_length);
  fprintf(stderr, "| fix orientation : %s\n", (import_settings.fix_orientation) ? "yes" : "no");
  fprintf(stderr, "| keep bind info  : %s\n", (import_settings.keep_bind_info) ? "yes" : "no");
}

static void print_import_footer(int status)
{
  fprintf(stderr, "+----------------------------------\n");
  fprintf(stderr, "| Collada Import : %s\n", (status) ? "OK" : "FAIL");
  fprintf(stderr, "+----------------------------------\n");
}

int collada_import(bContext *C, ImportSettings *import_settings)
{
  print_import_header(*import_settings);
  DocumentImporter imp(C, import_settings);
  int status = imp.import() ? 1 : 0;
  print_import_footer(status);

  return status;
}

int collada_export(bContext *C, ExportSettings *export_settings)
{
  BlenderContext blender_context(C);
  ViewLayer *view_layer = blender_context.get_view_layer();

  int includeFilter = OB_REL_NONE;
  if (export_settings->include_armatures) {
    includeFilter |= OB_REL_MOD_ARMATURE;
  }
  if (export_settings->include_children) {
    includeFilter |= OB_REL_CHILDREN_RECURSIVE;
  }

  /* Fetch the complete set of exported objects
   * ATTENTION: Invisible objects will not be exported
   */
  eObjectSet objectSet = (export_settings->selected) ? OB_SET_SELECTED : OB_SET_ALL;
  export_settings->export_set = BKE_object_relational_superset(
      view_layer, objectSet, (eObRelationTypes)includeFilter);

  int export_count = BLI_linklist_count(export_settings->export_set);

  if (export_count == 0) {
    if (export_settings->selected) {
      fprintf(stderr,
              "Collada: Found no objects to export.\nPlease ensure that all objects which shall "
              "be exported are also visible in the 3D Viewport.\n");
    }
    else {
      fprintf(stderr, "Collada: Your scene seems to be empty. No Objects will be exported.\n");
    }
  }
  else {
    if (export_settings->sort_by_name) {
      bc_bubble_sort_by_Object_name(export_settings->export_set);
    }
  }

  DocumentExporter exporter(blender_context, export_settings);
  int status = exporter.exportCurrentScene();

  BLI_linklist_free(export_settings->export_set, NULL);

  return (status) ? -1 : export_count;
}

/* end extern C */
}

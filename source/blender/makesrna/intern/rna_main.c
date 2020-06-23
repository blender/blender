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
 * \ingroup RNA
 */

#include <stdlib.h>
#include <string.h>

#include "BLI_path_util.h"
#include "BLI_utildefines.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "rna_internal.h"

#ifdef RNA_RUNTIME

#  include "BKE_global.h"
#  include "BKE_main.h"
#  include "BKE_mesh.h"

/* all the list begin functions are added manually here, Main is not in SDNA */

static bool rna_Main_use_autopack_get(PointerRNA *UNUSED(ptr))
{
  if (G.fileflags & G_FILE_AUTOPACK) {
    return 1;
  }

  return 0;
}

static void rna_Main_use_autopack_set(PointerRNA *UNUSED(ptr), bool value)
{
  if (value) {
    G.fileflags |= G_FILE_AUTOPACK;
  }
  else {
    G.fileflags &= ~G_FILE_AUTOPACK;
  }
}

static bool rna_Main_is_saved_get(PointerRNA *UNUSED(ptr))
{
  return G.relbase_valid;
}

static bool rna_Main_is_dirty_get(PointerRNA *ptr)
{
  /* XXX, not totally nice to do it this way, should store in main ? */
  Main *bmain = (Main *)ptr->data;
  wmWindowManager *wm;
  if ((wm = bmain->wm.first)) {
    return !wm->file_saved;
  }

  return true;
}

static void rna_Main_filepath_get(PointerRNA *ptr, char *value)
{
  Main *bmain = (Main *)ptr->data;
  BLI_strncpy(value, bmain->name, sizeof(bmain->name));
}

static int rna_Main_filepath_length(PointerRNA *ptr)
{
  Main *bmain = (Main *)ptr->data;
  return strlen(bmain->name);
}

#  if 0
static void rna_Main_filepath_set(PointerRNA *ptr, const char *value)
{
  Main *bmain = (Main *)ptr->data;
  BLI_strncpy(bmain->name, value, sizeof(bmain->name));
}
#  endif

#  define RNA_MAIN_LISTBASE_FUNCS_DEF(_listbase_name) \
    static void rna_Main_##_listbase_name##_begin(CollectionPropertyIterator *iter, \
                                                  PointerRNA *ptr) \
    { \
      rna_iterator_listbase_begin(iter, &((Main *)ptr->data)->_listbase_name, NULL); \
    }

RNA_MAIN_LISTBASE_FUNCS_DEF(actions)
RNA_MAIN_LISTBASE_FUNCS_DEF(armatures)
RNA_MAIN_LISTBASE_FUNCS_DEF(brushes)
RNA_MAIN_LISTBASE_FUNCS_DEF(cachefiles)
RNA_MAIN_LISTBASE_FUNCS_DEF(cameras)
RNA_MAIN_LISTBASE_FUNCS_DEF(collections)
RNA_MAIN_LISTBASE_FUNCS_DEF(curves)
RNA_MAIN_LISTBASE_FUNCS_DEF(fonts)
RNA_MAIN_LISTBASE_FUNCS_DEF(gpencils)
RNA_MAIN_LISTBASE_FUNCS_DEF(hairs)
RNA_MAIN_LISTBASE_FUNCS_DEF(images)
RNA_MAIN_LISTBASE_FUNCS_DEF(lattices)
RNA_MAIN_LISTBASE_FUNCS_DEF(libraries)
RNA_MAIN_LISTBASE_FUNCS_DEF(lightprobes)
RNA_MAIN_LISTBASE_FUNCS_DEF(lights)
RNA_MAIN_LISTBASE_FUNCS_DEF(linestyles)
RNA_MAIN_LISTBASE_FUNCS_DEF(masks)
RNA_MAIN_LISTBASE_FUNCS_DEF(materials)
RNA_MAIN_LISTBASE_FUNCS_DEF(meshes)
RNA_MAIN_LISTBASE_FUNCS_DEF(metaballs)
RNA_MAIN_LISTBASE_FUNCS_DEF(movieclips)
RNA_MAIN_LISTBASE_FUNCS_DEF(nodetrees)
RNA_MAIN_LISTBASE_FUNCS_DEF(objects)
RNA_MAIN_LISTBASE_FUNCS_DEF(paintcurves)
RNA_MAIN_LISTBASE_FUNCS_DEF(palettes)
RNA_MAIN_LISTBASE_FUNCS_DEF(particles)
RNA_MAIN_LISTBASE_FUNCS_DEF(pointclouds)
RNA_MAIN_LISTBASE_FUNCS_DEF(scenes)
RNA_MAIN_LISTBASE_FUNCS_DEF(screens)
RNA_MAIN_LISTBASE_FUNCS_DEF(shapekeys)
RNA_MAIN_LISTBASE_FUNCS_DEF(simulations)
RNA_MAIN_LISTBASE_FUNCS_DEF(sounds)
RNA_MAIN_LISTBASE_FUNCS_DEF(speakers)
RNA_MAIN_LISTBASE_FUNCS_DEF(texts)
RNA_MAIN_LISTBASE_FUNCS_DEF(textures)
RNA_MAIN_LISTBASE_FUNCS_DEF(volumes)
RNA_MAIN_LISTBASE_FUNCS_DEF(wm)
RNA_MAIN_LISTBASE_FUNCS_DEF(workspaces)
RNA_MAIN_LISTBASE_FUNCS_DEF(worlds)

#  undef RNA_MAIN_LISTBASE_FUNCS_DEF

static void rna_Main_version_get(PointerRNA *ptr, int *value)
{
  Main *bmain = (Main *)ptr->data;
  value[0] = bmain->versionfile / 100;
  value[1] = bmain->versionfile % 100;
  value[2] = bmain->subversionfile;
}

#  ifdef UNIT_TEST

static PointerRNA rna_Test_test_get(PointerRNA *ptr)
{
  PointerRNA ret = *ptr;
  ret.type = &RNA_Test;

  return ret;
}

#  endif

#else

/* local convenience types */
typedef void(CollectionDefFunc)(struct BlenderRNA *brna, struct PropertyRNA *cprop);

typedef struct MainCollectionDef {
  const char *identifier;
  const char *type;
  const char *iter_begin;
  const char *name;
  const char *description;
  CollectionDefFunc *func;
} MainCollectionDef;

void RNA_def_main(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  CollectionDefFunc *func;

  /* plural must match idtypes in readblenentry.c */
  MainCollectionDef lists[] = {
      {"cameras",
       "Camera",
       "rna_Main_cameras_begin",
       "Cameras",
       "Camera data-blocks",
       RNA_def_main_cameras},
      {"scenes",
       "Scene",
       "rna_Main_scenes_begin",
       "Scenes",
       "Scene data-blocks",
       RNA_def_main_scenes},
      {"objects",
       "Object",
       "rna_Main_objects_begin",
       "Objects",
       "Object data-blocks",
       RNA_def_main_objects},
      {"materials",
       "Material",
       "rna_Main_materials_begin",
       "Materials",
       "Material data-blocks",
       RNA_def_main_materials},
      {"node_groups",
       "NodeTree",
       "rna_Main_nodetrees_begin",
       "Node Groups",
       "Node group data-blocks",
       RNA_def_main_node_groups},
      {"meshes",
       "Mesh",
       "rna_Main_meshes_begin",
       "Meshes",
       "Mesh data-blocks",
       RNA_def_main_meshes},
      {"lights",
       "Light",
       "rna_Main_lights_begin",
       "Lights",
       "Light data-blocks",
       RNA_def_main_lights},
      {"libraries",
       "Library",
       "rna_Main_libraries_begin",
       "Libraries",
       "Library data-blocks",
       RNA_def_main_libraries},
      {"screens",
       "Screen",
       "rna_Main_screens_begin",
       "Screens",
       "Screen data-blocks",
       RNA_def_main_screens},
      {"window_managers",
       "WindowManager",
       "rna_Main_wm_begin",
       "Window Managers",
       "Window manager data-blocks",
       RNA_def_main_window_managers},
      {"images",
       "Image",
       "rna_Main_images_begin",
       "Images",
       "Image data-blocks",
       RNA_def_main_images},
      {"lattices",
       "Lattice",
       "rna_Main_lattices_begin",
       "Lattices",
       "Lattice data-blocks",
       RNA_def_main_lattices},
      {"curves",
       "Curve",
       "rna_Main_curves_begin",
       "Curves",
       "Curve data-blocks",
       RNA_def_main_curves},
      {"metaballs",
       "MetaBall",
       "rna_Main_metaballs_begin",
       "Metaballs",
       "Metaball data-blocks",
       RNA_def_main_metaballs},
      {"fonts",
       "VectorFont",
       "rna_Main_fonts_begin",
       "Vector Fonts",
       "Vector font data-blocks",
       RNA_def_main_fonts},
      {"textures",
       "Texture",
       "rna_Main_textures_begin",
       "Textures",
       "Texture data-blocks",
       RNA_def_main_textures},
      {"brushes",
       "Brush",
       "rna_Main_brushes_begin",
       "Brushes",
       "Brush data-blocks",
       RNA_def_main_brushes},
      {"worlds",
       "World",
       "rna_Main_worlds_begin",
       "Worlds",
       "World data-blocks",
       RNA_def_main_worlds},
      {"collections",
       "Collection",
       "rna_Main_collections_begin",
       "Collections",
       "Collection data-blocks",
       RNA_def_main_collections},
      {"shape_keys",
       "Key",
       "rna_Main_shapekeys_begin",
       "Shape Keys",
       "Shape Key data-blocks",
       NULL},
      {"texts", "Text", "rna_Main_texts_begin", "Texts", "Text data-blocks", RNA_def_main_texts},
      {"speakers",
       "Speaker",
       "rna_Main_speakers_begin",
       "Speakers",
       "Speaker data-blocks",
       RNA_def_main_speakers},
      {"sounds",
       "Sound",
       "rna_Main_sounds_begin",
       "Sounds",
       "Sound data-blocks",
       RNA_def_main_sounds},
      {"armatures",
       "Armature",
       "rna_Main_armatures_begin",
       "Armatures",
       "Armature data-blocks",
       RNA_def_main_armatures},
      {"actions",
       "Action",
       "rna_Main_actions_begin",
       "Actions",
       "Action data-blocks",
       RNA_def_main_actions},
      {"particles",
       "ParticleSettings",
       "rna_Main_particles_begin",
       "Particles",
       "Particle data-blocks",
       RNA_def_main_particles},
      {"palettes",
       "Palette",
       "rna_Main_palettes_begin",
       "Palettes",
       "Palette data-blocks",
       RNA_def_main_palettes},
      {"grease_pencils",
       "GreasePencil",
       "rna_Main_gpencils_begin",
       "Grease Pencil",
       "Grease Pencil data-blocks",
       RNA_def_main_gpencil},
      {"movieclips",
       "MovieClip",
       "rna_Main_movieclips_begin",
       "Movie Clips",
       "Movie Clip data-blocks",
       RNA_def_main_movieclips},
      {"masks", "Mask", "rna_Main_masks_begin", "Masks", "Masks data-blocks", RNA_def_main_masks},
      {"linestyles",
       "FreestyleLineStyle",
       "rna_Main_linestyles_begin",
       "Line Styles",
       "Line Style data-blocks",
       RNA_def_main_linestyles},
      {"cache_files",
       "CacheFile",
       "rna_Main_cachefiles_begin",
       "Cache Files",
       "Cache Files data-blocks",
       RNA_def_main_cachefiles},
      {"paint_curves",
       "PaintCurve",
       "rna_Main_paintcurves_begin",
       "Paint Curves",
       "Paint Curves data-blocks",
       RNA_def_main_paintcurves},
      {"workspaces",
       "WorkSpace",
       "rna_Main_workspaces_begin",
       "Workspaces",
       "Workspace data-blocks",
       RNA_def_main_workspaces},
      {"lightprobes",
       "LightProbe",
       "rna_Main_lightprobes_begin",
       "LightProbes",
       "LightProbe data-blocks",
       RNA_def_main_lightprobes},
      {"hairs", "Hair", "rna_Main_hairs_begin", "Hairs", "Hair data-blocks", RNA_def_main_hairs},
      {"pointclouds",
       "PointCloud",
       "rna_Main_pointclouds_begin",
       "Point Clouds",
       "Point cloud data-blocks",
       RNA_def_main_pointclouds},
      {"volumes",
       "Volume",
       "rna_Main_volumes_begin",
       "Volumes",
       "Volume data-blocks",
       RNA_def_main_volumes},
      {"simulations",
       "Simulation",
       "rna_Main_simulations_begin",
       "Simulations",
       "Simulation data-blocks",
       RNA_def_main_simulations},
      {NULL, NULL, NULL, NULL, NULL, NULL},
  };

  int i;

  srna = RNA_def_struct(brna, "BlendData", NULL);
  RNA_def_struct_ui_text(srna,
                         "Blendfile Data",
                         "Main data structure representing a .blend file and all its data-blocks");
  RNA_def_struct_ui_icon(srna, ICON_BLENDER);

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_string_maxlength(prop, FILE_MAX);
  RNA_def_property_string_funcs(
      prop, "rna_Main_filepath_get", "rna_Main_filepath_length", "rna_Main_filepath_set");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Filename", "Path to the .blend file");

  prop = RNA_def_property(srna, "is_dirty", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Main_is_dirty_get", NULL);
  RNA_def_property_ui_text(
      prop, "File Has Unsaved Changes", "Have recent edits been saved to disk");

  prop = RNA_def_property(srna, "is_saved", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Main_is_saved_get", NULL);
  RNA_def_property_ui_text(
      prop, "File is Saved", "Has the current session been saved to disk as a .blend file");

  prop = RNA_def_property(srna, "use_autopack", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_Main_use_autopack_get", "rna_Main_use_autopack_set");
  RNA_def_property_ui_text(
      prop, "Use Autopack", "Automatically pack all external data into .blend file");

  prop = RNA_def_int_vector(srna,
                            "version",
                            3,
                            NULL,
                            0,
                            INT_MAX,
                            "Version",
                            "Version of Blender the .blend was saved with",
                            0,
                            INT_MAX);
  RNA_def_property_int_funcs(prop, "rna_Main_version_get", NULL, NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_THICK_WRAP);

  for (i = 0; lists[i].name; i++) {
    prop = RNA_def_property(srna, lists[i].identifier, PROP_COLLECTION, PROP_NONE);
    RNA_def_property_struct_type(prop, lists[i].type);
    RNA_def_property_collection_funcs(prop,
                                      lists[i].iter_begin,
                                      "rna_iterator_listbase_next",
                                      "rna_iterator_listbase_end",
                                      "rna_iterator_listbase_get",
                                      NULL,
                                      NULL,
                                      NULL,
                                      NULL);
    RNA_def_property_ui_text(prop, lists[i].name, lists[i].description);

    /* collection functions */
    func = lists[i].func;
    if (func) {
      func(brna, prop);
    }
  }

  RNA_api_main(srna);

#  ifdef UNIT_TEST

  RNA_define_verify_sdna(0);

  prop = RNA_def_property(srna, "test", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Test");
  RNA_def_property_pointer_funcs(prop, "rna_Test_test_get", NULL, NULL, NULL);

  RNA_define_verify_sdna(1);

#  endif
}

#endif

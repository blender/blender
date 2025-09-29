/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>
#include <cstring>

#include "BLI_path_utils.hh"

#include "BLI_string_ref.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#ifdef RNA_RUNTIME

#  include "IMB_colormanagement.hh"

#  include "DNA_windowmanager_types.h"

#  include "BKE_global.hh"
#  include "BKE_main.hh"
#  include "BKE_mesh.hh"

/* all the list begin functions are added manually here, Main is not in SDNA */

static bool rna_Main_use_autopack_get(PointerRNA * /*ptr*/)
{
  if (G.fileflags & G_FILE_AUTOPACK) {
    return 1;
  }

  return 0;
}

static void rna_Main_use_autopack_set(PointerRNA * /*ptr*/, bool value)
{
  if (value) {
    G.fileflags |= G_FILE_AUTOPACK;
  }
  else {
    G.fileflags &= ~G_FILE_AUTOPACK;
  }
}

static bool rna_Main_is_saved_get(PointerRNA *ptr)
{
  const Main *bmain = (Main *)ptr->data;
  return (bmain->filepath[0] != '\0');
}

static bool rna_Main_is_dirty_get(PointerRNA *ptr)
{
  /* XXX, not totally nice to do it this way, should store in main ? */
  Main *bmain = (Main *)ptr->data;
  wmWindowManager *wm;
  if ((wm = static_cast<wmWindowManager *>(bmain->wm.first))) {
    return !wm->file_saved;
  }

  return true;
}

static void rna_Main_filepath_get(PointerRNA *ptr, char *value)
{
  Main *bmain = (Main *)ptr->data;
  strcpy(value, bmain->filepath);
}

static int rna_Main_filepath_length(PointerRNA *ptr)
{
  Main *bmain = (Main *)ptr->data;
  return strlen(bmain->filepath);
}

#  if 0
static void rna_Main_filepath_set(PointerRNA *ptr, const char *value)
{
  Main *bmain = (Main *)ptr->data;
  STRNCPY(bmain->filepath, value);
}
#  endif

static PointerRNA rna_Main_colorspace_get(PointerRNA *ptr)
{
  Main *bmain = (Main *)ptr->data;
  return PointerRNA(nullptr, &RNA_BlendFileColorspace, &bmain->colorspace);
}

static int rna_MainColorspace_working_space_get(PointerRNA *ptr)
{
  MainColorspace *colorspace = ptr->data_as<MainColorspace>();
  return IMB_colormanagement_working_space_get_named_index(colorspace->scene_linear_name);
}

static const EnumPropertyItem *rna_MainColorspace_working_space_itemf(bContext * /*C*/,
                                                                      PointerRNA * /*ptr*/,
                                                                      PropertyRNA * /*prop*/,
                                                                      bool *r_free)
{
  EnumPropertyItem *items = nullptr;
  int totitem = 0;

  IMB_colormanagement_working_space_items_add(&items, &totitem);
  RNA_enum_item_end(&items, &totitem);

  *r_free = true;

  return items;
}

static void rna_MainColorspace_working_space_interop_id_get(PointerRNA *ptr, char *value)
{
  MainColorspace *main_colorspace = ptr->data_as<MainColorspace>();
  const ColorSpace *colorspace = IMB_colormanagement_space_get_named(
      main_colorspace->scene_linear_name);
  const auto interop_id = (colorspace) ? IMB_colormanagement_space_get_interop_id(colorspace) : "";
  strcpy(value, interop_id.c_str());
}

static int rna_MainColorspace_working_space_interop_id_length(PointerRNA *ptr)
{
  MainColorspace *main_colorspace = ptr->data_as<MainColorspace>();
  const ColorSpace *colorspace = IMB_colormanagement_space_get_named(
      main_colorspace->scene_linear_name);
  const auto interop_id = (colorspace) ? IMB_colormanagement_space_get_interop_id(colorspace) : "";
  return interop_id.size();
}

static bool rna_MainColorspace_is_missing_opencolorio_config_get(PointerRNA *ptr)
{
  MainColorspace *colorspace = ptr->data_as<MainColorspace>();
  return colorspace->is_missing_opencolorio_config;
}

#  define RNA_MAIN_LISTBASE_FUNCS_DEF(_listbase_name) \
    static void rna_Main_##_listbase_name##_begin(CollectionPropertyIterator *iter, \
                                                  PointerRNA *ptr) \
    { \
      rna_iterator_listbase_begin(iter, ptr, &((Main *)ptr->data)->_listbase_name, nullptr); \
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
RNA_MAIN_LISTBASE_FUNCS_DEF(grease_pencils)
RNA_MAIN_LISTBASE_FUNCS_DEF(hair_curves)
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
using CollectionDefFunc = void(BlenderRNA *brna, PropertyRNA *cprop);

struct MainCollectionDef {
  const char *identifier;
  const char *type;
  const char *iter_begin;
  const char *name;
  const char *description;
  CollectionDefFunc *func;
};

static void rna_def_main_colorspace(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "BlendFileColorspace", nullptr);
  RNA_def_struct_ui_text(srna,
                         "Blend-File Color Space",
                         "Information about the color space used for data-blocks in a blend file");

  prop = RNA_def_property(srna, "working_space", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_ENUM_NO_CONTEXT);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_items(prop, rna_enum_dummy_NULL_items);
  RNA_def_property_ui_text(prop,
                           "Working Space",
                           "Color space used for all scene linear colors in this file, and "
                           "for compositing, shader and geometry nodes processing");
  RNA_def_property_enum_funcs(prop,
                              "rna_MainColorspace_working_space_get",
                              nullptr,
                              "rna_MainColorspace_working_space_itemf");

  prop = RNA_def_property(srna, "working_space_interop_id", PROP_STRING, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop,
      "Working Space Interop ID",
      "Unique identifier for common color spaces, as defined by the Color Interop Forum. May be "
      "empty if there is no interop ID for the working space. Common values are lin_rec709_scene, "
      "lin_rec2020_scene and lin_ap1_scene (for ACEScg)");
  RNA_def_property_string_funcs(prop,
                                "rna_MainColorspace_working_space_interop_id_get",
                                "rna_MainColorspace_working_space_interop_id_length",
                                nullptr);
  prop = RNA_def_property(srna, "is_missing_opencolorio_config", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(
      prop, "rna_MainColorspace_is_missing_opencolorio_config_get", nullptr);
  RNA_def_property_ui_text(prop,
                           "Missing OpenColorIO Configuration",
                           "A color space, view or display was not found, which likely means the "
                           "OpenColorIO config used to create this blend file is missing");
}

void RNA_def_main(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* Plural must match ID-types in `readblenentry.cc`. */
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
       nullptr},
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
      {"annotations",
       "Annotation",
       "rna_Main_gpencils_begin",
       "Annotation",
       "Annotation data-blocks (legacy Grease Pencil)",
       RNA_def_main_annotations},
      {"grease_pencils",
       "GreasePencil",
       "rna_Main_grease_pencils_begin",
       "Grease Pencil",
       "Grease Pencil data-blocks",
       RNA_def_main_grease_pencil},
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
       "Light Probes",
       "Light Probe data-blocks",
       RNA_def_main_lightprobes},
      /**
       * \note The name `hair_curves` is chosen to be different than `curves`,
       * but they are generic curve data-blocks, not just for hair.
       */
      {"hair_curves",
       "Curves",
       "rna_Main_hair_curves_begin",
       "Hair Curves",
       "Hair curve data-blocks",
       RNA_def_main_hair_curves},
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
      {nullptr, nullptr, nullptr, nullptr, nullptr, nullptr},
  };

  int i;

  srna = RNA_def_struct(brna, "BlendData", nullptr);
  RNA_def_struct_ui_text(srna,
                         "Blend-File Data",
                         "Main data structure representing a .blend file and all its data-blocks");
  RNA_def_struct_ui_icon(srna, ICON_BLENDER);

  prop = RNA_def_property(srna, "filepath", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_string_maxlength(prop, FILE_MAX);
  RNA_def_property_string_funcs(
      prop, "rna_Main_filepath_get", "rna_Main_filepath_length", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Filename", "Path to the .blend file");

  prop = RNA_def_property(srna, "is_dirty", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Main_is_dirty_get", nullptr);
  RNA_def_property_ui_text(
      prop, "File Has Unsaved Changes", "Have recent edits been saved to disk");

  prop = RNA_def_property(srna, "is_saved", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_Main_is_saved_get", nullptr);
  RNA_def_property_ui_text(
      prop, "File is Saved", "Has the current session been saved to disk as a .blend file");

  prop = RNA_def_property(srna, "use_autopack", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_Main_use_autopack_get", "rna_Main_use_autopack_set");
  RNA_def_property_ui_text(
      prop, "Use Auto-Pack", "Automatically pack all external data into .blend file");

  prop = RNA_def_int_vector(srna,
                            "version",
                            3,
                            nullptr,
                            0,
                            INT_MAX,
                            "Version",
                            "File format version the .blend file was saved with",
                            0,
                            INT_MAX);
  RNA_def_property_int_funcs(prop, "rna_Main_version_get", nullptr, nullptr);
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
                                      nullptr,
                                      nullptr,
                                      nullptr,
                                      nullptr);
    RNA_def_property_ui_text(prop, lists[i].name, lists[i].description);

    /* collection functions */
    CollectionDefFunc *func = lists[i].func;
    if (func) {
      func(brna, prop);
    }
  }

  rna_def_main_colorspace(brna);

  prop = RNA_def_property(srna, "colorspace", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "BlendFileColorspace");
  RNA_def_property_pointer_funcs(prop, "rna_Main_colorspace_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(
      prop,
      "Color Space",
      "Information about the color space used for data-blocks in a blend file");

  RNA_api_main(srna);

#  ifdef UNIT_TEST

  RNA_define_verify_sdna(0);

  prop = RNA_def_property(srna, "test", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "Test");
  RNA_def_property_pointer_funcs(prop, "rna_Test_test_get", nullptr, nullptr, nullptr);

  RNA_define_verify_sdna(1);

#  endif
}

#endif

/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cerrno>
#include <cstdlib>

#include "DNA_ID.h"
#include "DNA_space_types.h"

#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "rna_internal.hh"

#ifdef RNA_RUNTIME

#  include "BKE_action.hh"
#  include "BKE_armature.hh"
#  include "BKE_brush.hh"
#  include "BKE_camera.h"
#  include "BKE_collection.hh"
#  include "BKE_curve.hh"
#  include "BKE_curves.h"
#  include "BKE_displist.h"
#  include "BKE_gpencil_legacy.h"
#  include "BKE_grease_pencil.hh"
#  include "BKE_icons.hh"
#  include "BKE_idtype.hh"
#  include "BKE_image.hh"
#  include "BKE_lattice.hh"
#  include "BKE_lib_remap.hh"
#  include "BKE_library.hh"
#  include "BKE_light.h"
#  include "BKE_lightprobe.h"
#  include "BKE_linestyle.h"
#  include "BKE_main_invariants.hh"
#  include "BKE_mask.h"
#  include "BKE_material.hh"
#  include "BKE_mball.hh"
#  include "BKE_mesh.hh"
#  include "BKE_movieclip.h"
#  include "BKE_node.hh"
#  include "BKE_object.hh"
#  include "BKE_paint.hh"
#  include "BKE_particle.h"
#  include "BKE_pointcloud.hh"
#  include "BKE_scene.hh"
#  include "BKE_sound.hh"
#  include "BKE_speaker.hh"
#  include "BKE_text.h"
#  include "BKE_texture.h"
#  include "BKE_vfont.hh"
#  include "BKE_volume.hh"
#  include "BKE_workspace.hh"
#  include "BKE_world.h"

#  include "DEG_depsgraph_build.hh"
#  include "DEG_depsgraph_query.hh"

#  include "DNA_anim_types.h"
#  include "DNA_armature_types.h"
#  include "DNA_brush_types.h"
#  include "DNA_camera_types.h"
#  include "DNA_collection_types.h"
#  include "DNA_curve_types.h"
#  include "DNA_curves_types.h"
#  include "DNA_gpencil_legacy_types.h"
#  include "DNA_lattice_types.h"
#  include "DNA_light_types.h"
#  include "DNA_lightprobe_types.h"
#  include "DNA_mask_types.h"
#  include "DNA_material_types.h"
#  include "DNA_mesh_types.h"
#  include "DNA_meta_types.h"
#  include "DNA_movieclip_types.h"
#  include "DNA_node_types.h"
#  include "DNA_particle_types.h"
#  include "DNA_pointcloud_types.h"
#  include "DNA_sound_types.h"
#  include "DNA_speaker_types.h"
#  include "DNA_text_types.h"
#  include "DNA_texture_types.h"
#  include "DNA_vfont_types.h"
#  include "DNA_volume_types.h"
#  include "DNA_world_types.h"

#  include "ED_node.hh"
#  include "ED_scene.hh"
#  include "ED_screen.hh"

#  include "BLT_translation.hh"

#  ifdef WITH_PYTHON
#    include "BPY_extern.hh"
#  endif

#  include "WM_api.hh"
#  include "WM_types.hh"

static void rna_idname_validate(const char *name, char *r_name)
{
  BLI_strncpy(r_name, name, MAX_ID_NAME - 2);
  BLI_str_utf8_invalid_strip(r_name, strlen(r_name));
}

static void rna_Main_ID_remove(Main *bmain,
                               ReportList *reports,
                               PointerRNA *id_ptr,
                               bool do_unlink,
                               bool do_id_user,
                               bool do_ui_user)
{
  ID *id = static_cast<ID *>(id_ptr->data);
  if (id->tag & ID_TAG_NO_MAIN) {
    BKE_reportf(reports,
                RPT_ERROR,
                "%s '%s' is outside of main database and cannot be removed from it",
                BKE_idtype_idcode_to_name(GS(id->name)),
                id->name + 2);
    return;
  }
  if (do_unlink) {
    BKE_id_delete(bmain, id);
    id_ptr->invalidate();
    /* Force full redraw, mandatory to avoid crashes when running this from UI... */
    WM_main_add_notifier(NC_WINDOW, nullptr);
  }
  else if (ID_REAL_USERS(id) <= 0) {
    const int flag = (do_id_user ? 0 : LIB_ID_FREE_NO_USER_REFCOUNT) |
                     (do_ui_user ? 0 : LIB_ID_FREE_NO_UI_USER);
    /* Still using ID flags here, this is in-between commit anyway... */
    BKE_id_free_ex(bmain, id, flag, true);
    id_ptr->invalidate();
  }
  else {
    BKE_reportf(
        reports,
        RPT_ERROR,
        "%s '%s' must have zero users to be removed, found %d (try with do_unlink=True parameter)",
        BKE_idtype_idcode_to_name(GS(id->name)),
        id->name + 2,
        ID_REAL_USERS(id));
  }
}

static ID *rna_Main_pack_linked_ids_hierarchy(struct BlendData *blenddata,
                                              ReportList *reports,
                                              ID *root_id)
{
  if (!ID_IS_LINKED(root_id)) {
    BKE_reportf(reports, RPT_ERROR, "Only linked IDs can be packed");
    return nullptr;
  }
  if (ID_IS_PACKED(root_id)) {
    /* Nothing to do. */
    return root_id;
  }

  Main *bmain = reinterpret_cast<Main *>(blenddata);
  blender::bke::library::pack_linked_id_hierarchy(*bmain, *root_id);

  ID *packed_root_id = root_id->newid;
  BKE_main_id_newptr_and_tag_clear(bmain);

  return packed_root_id;
}

static Camera *rna_Main_cameras_new(Main *bmain, const char *name)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  Camera *camera = BKE_camera_add(bmain, safe_name);
  id_us_min(&camera->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return camera;
}

static Scene *rna_Main_scenes_new(Main *bmain, const char *name)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  Scene *scene = BKE_scene_add(bmain, safe_name);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return scene;
}
static void rna_Main_scenes_remove(
    Main *bmain, bContext *C, ReportList *reports, PointerRNA *scene_ptr, bool do_unlink)
{
  /* don't call BKE_id_free(...) directly */
  Scene *scene = static_cast<Scene *>(scene_ptr->data);

  if (BKE_scene_can_be_removed(bmain, scene)) {
    if (do_unlink) {
      Scene *scene_new = BKE_scene_find_replacement(*bmain, *scene);
      if (scene_new && ED_scene_replace_active_for_deletion(*C, *bmain, *scene, scene_new)) {
        rna_Main_ID_remove(bmain, reports, scene_ptr, do_unlink, true, true);
        return;
      }
    }
    else {
      rna_Main_ID_remove(bmain, reports, scene_ptr, do_unlink, true, true);
      return;
    }
  }

  BKE_reportf(reports,
              RPT_ERROR,
              "Scene '%s' is the last local one, cannot be removed",
              scene->id.name + 2);
}

static Object *rna_Main_objects_new(Main *bmain, ReportList *reports, const char *name, ID *data)
{
  if (data != nullptr && (data->tag & ID_TAG_NO_MAIN)) {
    BKE_report(reports,
               RPT_ERROR,
               "Cannot create object in main database with an evaluated data data-block");
    return nullptr;
  }

  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  Object *ob;
  int type = OB_EMPTY;

  if (data) {
    type = BKE_object_obdata_to_type(data);
    if (type == -1) {
      const char *idname;
      if (RNA_enum_id_from_value(rna_enum_id_type_items, GS(data->name), &idname) == 0) {
        idname = "UNKNOWN";
      }

      BKE_reportf(reports, RPT_ERROR, "ID type '%s' is not valid for an object", idname);
      return nullptr;
    }

    id_us_plus(data);
  }

  ob = BKE_object_add_only_object(bmain, type, safe_name);

  ob->data = data;
  BKE_object_materials_sync_length(bmain, ob, static_cast<ID *>(ob->data));

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return ob;
}

static Material *rna_Main_materials_new(Main *bmain, const char *name)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  Material *material = BKE_material_add(bmain, safe_name);
  id_us_min(&material->id);

  ED_node_shader_default(nullptr, bmain, &material->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return material;
}

static void rna_Main_materials_gpencil_data(Main * /*bmain*/, PointerRNA *id_ptr)
{
  ID *id = static_cast<ID *>(id_ptr->data);
  Material *ma = (Material *)id;
  BKE_gpencil_material_attr_init(ma);
}

static void rna_Main_materials_gpencil_remove(Main * /*bmain*/, PointerRNA *id_ptr)
{
  ID *id = static_cast<ID *>(id_ptr->data);
  Material *ma = (Material *)id;
  if (ma->gp_style) {
    MEM_SAFE_FREE(ma->gp_style);
  }
}

static const EnumPropertyItem *rna_Main_nodetree_type_itemf(bContext * /*C*/,
                                                            PointerRNA * /*ptr*/,
                                                            PropertyRNA * /*prop*/,
                                                            bool *r_free)
{
  return rna_node_tree_type_itemf(nullptr, nullptr, r_free);
}
static bNodeTree *rna_Main_nodetree_new(Main *bmain, const char *name, int type)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  blender::bke::bNodeTreeType *typeinfo = rna_node_tree_type_from_enum(type);
  if (typeinfo) {
    bNodeTree *ntree = blender::bke::node_tree_add_tree(bmain, safe_name, typeinfo->idname);
    BKE_main_ensure_invariants(*bmain);

    id_us_min(&ntree->id);
    return ntree;
  }
  else {
    return nullptr;
  }
}

static Mesh *rna_Main_meshes_new(Main *bmain, const char *name)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  Mesh *mesh = BKE_mesh_add(bmain, safe_name);
  id_us_min(&mesh->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return mesh;
}

/* copied from Mesh_getFromObject and adapted to RNA interface */
static Mesh *rna_Main_meshes_new_from_object(Main *bmain,
                                             ReportList *reports,
                                             Object *object,
                                             bool preserve_all_data_layers,
                                             Depsgraph *depsgraph)
{
  switch (object->type) {
    case OB_FONT:
    case OB_CURVES_LEGACY:
    case OB_SURF:
    case OB_MBALL:
    case OB_MESH:
      break;
    default:
      BKE_report(reports, RPT_ERROR, "Object does not have geometry data");
      return nullptr;
  }

  Mesh *mesh = BKE_mesh_new_from_object_to_bmain(
      bmain, depsgraph, object, preserve_all_data_layers);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return mesh;
}

static Light *rna_Main_lights_new(Main *bmain, const char *name, int type)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  Light *lamp = BKE_light_add(bmain, safe_name);
  lamp->type = type;
  id_us_min(&lamp->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return lamp;
}

static Image *rna_Main_images_new(Main *bmain,
                                  const char *name,
                                  int width,
                                  int height,
                                  bool alpha,
                                  bool float_buffer,
                                  bool stereo3d,
                                  bool is_data,
                                  bool tiled)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  float color[4] = {0.0, 0.0, 0.0, 1.0};
  Image *image = BKE_image_add_generated(bmain,
                                         width,
                                         height,
                                         safe_name,
                                         alpha ? 32 : 24,
                                         float_buffer,
                                         0,
                                         color,
                                         stereo3d,
                                         is_data,
                                         tiled);
  id_us_min(&image->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return image;
}
static Image *rna_Main_images_load(Main *bmain,
                                   ReportList *reports,
                                   const char *filepath,
                                   bool check_existing)
{
  Image *ima;

  errno = 0;
  if (check_existing) {
    ima = BKE_image_load_exists(bmain, filepath);
  }
  else {
    ima = BKE_image_load(bmain, filepath);
  }

  if (!ima) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Cannot read '%s': %s",
                filepath,
                errno ? strerror(errno) : RPT_("unsupported image format"));
  }

  id_us_min((ID *)ima);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return ima;
}

static Lattice *rna_Main_lattices_new(Main *bmain, const char *name)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  Lattice *lt = BKE_lattice_add(bmain, safe_name);
  id_us_min(&lt->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return lt;
}

static Curve *rna_Main_curves_new(Main *bmain, const char *name, int type)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  Curve *cu = BKE_curve_add(bmain, safe_name, type);
  id_us_min(&cu->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return cu;
}

static MetaBall *rna_Main_metaballs_new(Main *bmain, const char *name)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  MetaBall *mb = BKE_mball_add(bmain, safe_name);
  id_us_min(&mb->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return mb;
}

static VFont *rna_Main_fonts_load(Main *bmain,
                                  ReportList *reports,
                                  const char *filepath,
                                  bool check_existing)
{
  VFont *font;
  errno = 0;

  if (check_existing) {
    font = BKE_vfont_load_exists(bmain, filepath);
  }
  else {
    font = BKE_vfont_load(bmain, filepath);
  }

  if (!font) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Cannot read '%s': %s",
                filepath,
                errno ? strerror(errno) : RPT_("unsupported font format"));
  }

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return font;
}

static Tex *rna_Main_textures_new(Main *bmain, const char *name, int type)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  Tex *tex = BKE_texture_add(bmain, safe_name);
  BKE_texture_type_set(tex, type);
  id_us_min(&tex->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return tex;
}

static Brush *rna_Main_brushes_new(Main *bmain, const char *name, int mode)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  Brush *brush = BKE_brush_add(bmain, safe_name, eObjectMode(mode));
  id_us_min(&brush->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return brush;
}

static void rna_Main_brush_gpencil_data(Main * /*bmain*/, PointerRNA *id_ptr)
{
  ID *id = static_cast<ID *>(id_ptr->data);
  Brush *brush = reinterpret_cast<Brush *>(id);
  BKE_brush_init_gpencil_settings(brush);
}

static World *rna_Main_worlds_new(Main *bmain, const char *name)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  World *world = BKE_world_add(bmain, safe_name);
  id_us_min(&world->id);

  ED_node_shader_default(nullptr, bmain, &world->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return world;
}

static Collection *rna_Main_collections_new(Main *bmain, const char *name)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  Collection *collection = BKE_collection_add(bmain, nullptr, safe_name);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return collection;
}

static Speaker *rna_Main_speakers_new(Main *bmain, const char *name)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  Speaker *speaker = BKE_speaker_add(bmain, safe_name);
  id_us_min(&speaker->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return speaker;
}

static bSound *rna_Main_sounds_load(Main *bmain, const char *name, bool check_existing)
{
  bSound *sound;

  if (check_existing) {
    sound = BKE_sound_new_file_exists(bmain, name);
  }
  else {
    sound = BKE_sound_new_file(bmain, name);
  }

  id_us_min(&sound->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return sound;
}

static Text *rna_Main_texts_new(Main *bmain, const char *name)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  Text *text = BKE_text_add(bmain, safe_name);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return text;
}

static Text *rna_Main_texts_load(Main *bmain,
                                 ReportList *reports,
                                 const char *filepath,
                                 bool is_internal)
{
  Text *txt;

  errno = 0;
  txt = BKE_text_load_ex(bmain, filepath, BKE_main_blendfile_path(bmain), is_internal);

  if (!txt) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Cannot read '%s': %s",
                filepath,
                errno ? strerror(errno) : RPT_("unable to load text"));
  }

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return txt;
}

static bArmature *rna_Main_armatures_new(Main *bmain, const char *name)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  bArmature *arm = BKE_armature_add(bmain, safe_name);
  id_us_min(&arm->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return arm;
}

static bAction *rna_Main_actions_new(Main *bmain, const char *name)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  bAction *act = BKE_action_add(bmain, safe_name);
  id_fake_user_clear(&act->id);
  id_us_min(&act->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return act;
}

static ParticleSettings *rna_Main_particles_new(Main *bmain, const char *name)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  ParticleSettings *part = BKE_particlesettings_add(bmain, safe_name);
  id_us_min(&part->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return part;
}

static Palette *rna_Main_palettes_new(Main *bmain, const char *name)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  Palette *palette = BKE_palette_add(bmain, safe_name);
  id_us_min(&palette->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return (Palette *)palette;
}

static MovieClip *rna_Main_movieclip_load(Main *bmain,
                                          ReportList *reports,
                                          const char *filepath,
                                          bool check_existing)
{
  MovieClip *clip;

  errno = 0;

  if (check_existing) {
    clip = BKE_movieclip_file_add_exists(bmain, filepath);
  }
  else {
    clip = BKE_movieclip_file_add(bmain, filepath);
  }

  if (clip != nullptr) {
    DEG_relations_tag_update(bmain);
  }
  else {
    BKE_reportf(reports,
                RPT_ERROR,
                "Cannot read '%s': %s",
                filepath,
                errno ? strerror(errno) : RPT_("unable to load movie clip"));
  }

  id_us_min((ID *)clip);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return clip;
}

static Mask *rna_Main_mask_new(Main *bmain, const char *name)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  Mask *mask = BKE_mask_new(bmain, safe_name);
  id_us_min(&mask->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return mask;
}

static FreestyleLineStyle *rna_Main_linestyles_new(Main *bmain, const char *name)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  FreestyleLineStyle *linestyle = BKE_linestyle_new(bmain, safe_name);
  id_us_min(&linestyle->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return linestyle;
}

static LightProbe *rna_Main_lightprobe_new(Main *bmain, const char *name, int type)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  LightProbe *probe = BKE_lightprobe_add(bmain, safe_name);

  BKE_lightprobe_type_set(probe, type);

  id_us_min(&probe->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return probe;
}

static bGPdata *rna_Main_annotations_new(Main *bmain, const char *name)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  bGPdata *gpd = BKE_gpencil_data_addnew(bmain, safe_name);
  id_us_min(&gpd->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return gpd;
}

static GreasePencil *rna_Main_grease_pencils_new(Main *bmain, const char *name)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  GreasePencil *grease_pencil = BKE_grease_pencil_add(bmain, safe_name);
  id_us_min(&grease_pencil->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return grease_pencil;
}

static Curves *rna_Main_hair_curves_new(Main *bmain, const char *name)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  Curves *curves = BKE_curves_add(bmain, safe_name);
  id_us_min(&curves->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return curves;
}

static PointCloud *rna_Main_pointclouds_new(Main *bmain, const char *name)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  PointCloud *pointcloud = BKE_pointcloud_add(bmain, safe_name);
  id_us_min(&pointcloud->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return pointcloud;
}

static Volume *rna_Main_volumes_new(Main *bmain, const char *name)
{
  char safe_name[MAX_ID_NAME - 2];
  rna_idname_validate(name, safe_name);

  Volume *volume = BKE_volume_add(bmain, safe_name);
  id_us_min(&volume->id);

  WM_main_add_notifier(NC_ID | NA_ADDED, nullptr);

  return volume;
}

/* tag functions, all the same */
#  define RNA_MAIN_ID_TAG_FUNCS_DEF(_func_name, _listbase_name, _id_type) \
    static void rna_Main_##_func_name##_tag(Main *bmain, bool value) \
    { \
      BKE_main_id_tag_listbase(&bmain->_listbase_name, ID_TAG_DOIT, value); \
    }

RNA_MAIN_ID_TAG_FUNCS_DEF(cameras, cameras, ID_CA)
RNA_MAIN_ID_TAG_FUNCS_DEF(scenes, scenes, ID_SCE)
RNA_MAIN_ID_TAG_FUNCS_DEF(objects, objects, ID_OB)
RNA_MAIN_ID_TAG_FUNCS_DEF(materials, materials, ID_MA)
RNA_MAIN_ID_TAG_FUNCS_DEF(node_groups, nodetrees, ID_NT)
RNA_MAIN_ID_TAG_FUNCS_DEF(meshes, meshes, ID_ME)
RNA_MAIN_ID_TAG_FUNCS_DEF(lights, lights, ID_LA)
RNA_MAIN_ID_TAG_FUNCS_DEF(libraries, libraries, ID_LI)
RNA_MAIN_ID_TAG_FUNCS_DEF(screens, screens, ID_SCR)
RNA_MAIN_ID_TAG_FUNCS_DEF(window_managers, wm, ID_WM)
RNA_MAIN_ID_TAG_FUNCS_DEF(images, images, ID_IM)
RNA_MAIN_ID_TAG_FUNCS_DEF(lattices, lattices, ID_LT)
RNA_MAIN_ID_TAG_FUNCS_DEF(curves, curves, ID_CU_LEGACY)
RNA_MAIN_ID_TAG_FUNCS_DEF(metaballs, metaballs, ID_MB)
RNA_MAIN_ID_TAG_FUNCS_DEF(fonts, fonts, ID_VF)
RNA_MAIN_ID_TAG_FUNCS_DEF(textures, textures, ID_TE)
RNA_MAIN_ID_TAG_FUNCS_DEF(brushes, brushes, ID_BR)
RNA_MAIN_ID_TAG_FUNCS_DEF(worlds, worlds, ID_WO)
RNA_MAIN_ID_TAG_FUNCS_DEF(collections, collections, ID_GR)
// RNA_MAIN_ID_TAG_FUNCS_DEF(shape_keys, key, ID_KE)
RNA_MAIN_ID_TAG_FUNCS_DEF(texts, texts, ID_TXT)
RNA_MAIN_ID_TAG_FUNCS_DEF(speakers, speakers, ID_SPK)
RNA_MAIN_ID_TAG_FUNCS_DEF(sounds, sounds, ID_SO)
RNA_MAIN_ID_TAG_FUNCS_DEF(armatures, armatures, ID_AR)
RNA_MAIN_ID_TAG_FUNCS_DEF(actions, actions, ID_AC)
RNA_MAIN_ID_TAG_FUNCS_DEF(particles, particles, ID_PA)
RNA_MAIN_ID_TAG_FUNCS_DEF(palettes, palettes, ID_PAL)
RNA_MAIN_ID_TAG_FUNCS_DEF(gpencils, gpencils, ID_GD_LEGACY)
RNA_MAIN_ID_TAG_FUNCS_DEF(grease_pencils, grease_pencils, ID_GP)
RNA_MAIN_ID_TAG_FUNCS_DEF(movieclips, movieclips, ID_MC)
RNA_MAIN_ID_TAG_FUNCS_DEF(masks, masks, ID_MSK)
RNA_MAIN_ID_TAG_FUNCS_DEF(linestyle, linestyles, ID_LS)
RNA_MAIN_ID_TAG_FUNCS_DEF(cachefiles, cachefiles, ID_CF)
RNA_MAIN_ID_TAG_FUNCS_DEF(paintcurves, paintcurves, ID_PC)
RNA_MAIN_ID_TAG_FUNCS_DEF(workspaces, workspaces, ID_WS)
RNA_MAIN_ID_TAG_FUNCS_DEF(lightprobes, lightprobes, ID_LP)
RNA_MAIN_ID_TAG_FUNCS_DEF(hair_curves, hair_curves, ID_CV)
RNA_MAIN_ID_TAG_FUNCS_DEF(pointclouds, pointclouds, ID_PT)
RNA_MAIN_ID_TAG_FUNCS_DEF(volumes, volumes, ID_VO)

#  undef RNA_MAIN_ID_TAG_FUNCS_DEF

#else

void RNA_api_main(StructRNA *srna)
{
  FunctionRNA *func;
  PropertyRNA *parm;

#  if 0
  /* maybe we want to add functions in 'bpy.data' still?
   * for now they are all in collections bpy.data.images.new(...) */
  func = RNA_def_function(srna, "add_image", "rna_Main_add_image");
  RNA_def_function_ui_description(func, "Add a new image");
  parm = RNA_def_string_file_path(
      func, "filepath", nullptr, 0, "", "File path to load image from");
  RNA_def_parameter_flags(parm, PROP_PATH_SUPPORTS_BLEND_RELATIVE, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "image", "Image", "", "New image");
  RNA_def_function_return(func, parm);
#  endif

  func = RNA_def_function(srna, "pack_linked_ids_hierarchy", "rna_Main_pack_linked_ids_hierarchy");
  RNA_def_function_ui_description(
      func, "Pack the given linked ID and its dependencies into current blendfile");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "root_id", "ID", "", "Root linked ID to pack");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "packed_id", "ID", "", "The packed ID matching the given root ID");
  RNA_def_function_return(func, parm);
}

void RNA_def_main_cameras(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataCameras");
  srna = RNA_def_struct(brna, "BlendDataCameras", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Cameras", "Collection of cameras");

  func = RNA_def_function(srna, "new", "rna_Main_cameras_new");
  RNA_def_function_ui_description(func, "Add a new camera to the main database");
  parm = RNA_def_string(func, "name", "Camera", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "camera", "Camera", "", "New camera data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a camera from the current blendfile");
  parm = RNA_def_pointer(func, "camera", "Camera", "", "Camera to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(func,
                  "do_unlink",
                  true,
                  "",
                  "Unlink all usages of this camera before deleting it "
                  "(WARNING: will also delete objects instancing that camera data)");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this camera");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this camera");

  func = RNA_def_function(srna, "tag", "rna_Main_cameras_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void RNA_def_main_scenes(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataScenes");
  srna = RNA_def_struct(brna, "BlendDataScenes", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Scenes", "Collection of scenes");

  func = RNA_def_function(srna, "new", "rna_Main_scenes_new");
  RNA_def_function_ui_description(func, "Add a new scene to the main database");
  parm = RNA_def_string(func, "name", "Scene", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "scene", "Scene", "", "New scene data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_scenes_remove");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a scene from the current blendfile");
  parm = RNA_def_pointer(func, "scene", "Scene", "", "Scene to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(
      func, "do_unlink", true, "", "Unlink all usages of this scene before deleting it");

  func = RNA_def_function(srna, "tag", "rna_Main_scenes_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void RNA_def_main_objects(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataObjects");
  srna = RNA_def_struct(brna, "BlendDataObjects", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Objects", "Collection of objects");

  func = RNA_def_function(srna, "new", "rna_Main_objects_new");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Add a new object to the main database");
  parm = RNA_def_string(func, "name", "Object", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_pointer(func, "object_data", "ID", "", "Object data or None for an empty object");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* return type */
  parm = RNA_def_pointer(func, "object", "Object", "", "New object data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_ui_description(func, "Remove an object from the current blendfile");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "object", "Object", "", "Object to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(
      func, "do_unlink", true, "", "Unlink all usages of this object before deleting it");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this object");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this object");

  func = RNA_def_function(srna, "tag", "rna_Main_objects_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void RNA_def_main_materials(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataMaterials");
  srna = RNA_def_struct(brna, "BlendDataMaterials", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Materials", "Collection of materials");

  func = RNA_def_function(srna, "new", "rna_Main_materials_new");
  RNA_def_function_ui_description(func, "Add a new material to the main database");
  parm = RNA_def_string(func, "name", "Material", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "material", "Material", "", "New material data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "create_gpencil_data", "rna_Main_materials_gpencil_data");
  RNA_def_function_ui_description(func, "Add Grease Pencil material settings");
  parm = RNA_def_pointer(func, "material", "Material", "", "Material");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);

  func = RNA_def_function(srna, "remove_gpencil_data", "rna_Main_materials_gpencil_remove");
  RNA_def_function_ui_description(func, "Remove Grease Pencil material settings");
  parm = RNA_def_pointer(func, "material", "Material", "", "Material");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a material from the current blendfile");
  parm = RNA_def_pointer(func, "material", "Material", "", "Material to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(
      func, "do_unlink", true, "", "Unlink all usages of this material before deleting it");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this material");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this material");

  func = RNA_def_function(srna, "tag", "rna_Main_materials_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}
void RNA_def_main_node_groups(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataNodeTrees");
  srna = RNA_def_struct(brna, "BlendDataNodeTrees", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Node Trees", "Collection of node trees");

  func = RNA_def_function(srna, "new", "rna_Main_nodetree_new");
  RNA_def_function_ui_description(func, "Add a new node tree to the main database");
  parm = RNA_def_string(func, "name", "NodeGroup", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum(
      func, "type", rna_enum_dummy_DEFAULT_items, 0, "Type", "The type of node_group to add");
  RNA_def_property_enum_funcs(parm, nullptr, nullptr, "rna_Main_nodetree_type_itemf");
  RNA_def_parameter_flags(parm, PROP_ENUM_NO_CONTEXT, PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "tree", "NodeTree", "", "New node tree data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a node tree from the current blendfile");
  parm = RNA_def_pointer(func, "tree", "NodeTree", "", "Node tree to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(
      func, "do_unlink", true, "", "Unlink all usages of this node tree before deleting it");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this node tree");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this node tree");

  func = RNA_def_function(srna, "tag", "rna_Main_node_groups_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}
void RNA_def_main_meshes(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataMeshes");
  srna = RNA_def_struct(brna, "BlendDataMeshes", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Meshes", "Collection of meshes");

  func = RNA_def_function(srna, "new", "rna_Main_meshes_new");
  RNA_def_function_ui_description(func, "Add a new mesh to the main database");
  parm = RNA_def_string(func, "name", "Mesh", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "mesh", "Mesh", "", "New mesh data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new_from_object", "rna_Main_meshes_new_from_object");
  RNA_def_function_ui_description(
      func,
      "Add a new mesh created from given object (undeformed geometry if object is original, and "
      "final evaluated geometry, with all modifiers etc., if object is evaluated)");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "object", "Object", "", "Object to create mesh from");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);
  RNA_def_boolean(func,
                  "preserve_all_data_layers",
                  false,
                  "",
                  "Preserve all data layers in the mesh, like UV maps and vertex groups. "
                  "By default Blender only computes the subset of data layers needed for viewport "
                  "display and rendering, for better performance.");
  RNA_def_pointer(
      func,
      "depsgraph",
      "Depsgraph",
      "Dependency Graph",
      "Evaluated dependency graph which is required when preserve_all_data_layers is true");
  parm = RNA_def_pointer(func,
                         "mesh",
                         "Mesh",
                         "",
                         "Mesh created from object, remove it if it is only used for export");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a mesh from the current blendfile");
  parm = RNA_def_pointer(func, "mesh", "Mesh", "", "Mesh to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(func,
                  "do_unlink",
                  true,
                  "",
                  "Unlink all usages of this mesh before deleting it "
                  "(WARNING: will also delete objects instancing that mesh data)");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this mesh data");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this mesh data");

  func = RNA_def_function(srna, "tag", "rna_Main_meshes_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void RNA_def_main_lights(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataLights");
  srna = RNA_def_struct(brna, "BlendDataLights", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Lights", "Collection of lights");

  func = RNA_def_function(srna, "new", "rna_Main_lights_new");
  RNA_def_function_ui_description(func, "Add a new light to the main database");
  parm = RNA_def_string(func, "name", "Light", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum(
      func, "type", rna_enum_light_type_items, 0, "Type", "The type of light to add");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "light", "Light", "", "New light data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a light from the current blendfile");
  parm = RNA_def_pointer(func, "light", "Light", "", "Light to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(func,
                  "do_unlink",
                  true,
                  "",
                  "Unlink all usages of this light before deleting it "
                  "(WARNING: will also delete objects instancing that light data)");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this light data");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this light data");

  func = RNA_def_function(srna, "tag", "rna_Main_lights_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void RNA_def_main_libraries(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataLibraries");
  srna = RNA_def_struct(brna, "BlendDataLibraries", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Libraries", "Collection of libraries");

  func = RNA_def_function(srna, "tag", "rna_Main_libraries_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a library from the current blendfile");
  parm = RNA_def_pointer(func, "library", "Library", "", "Library to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(
      func, "do_unlink", true, "", "Unlink all usages of this library before deleting it");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this library");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this library");
}

void RNA_def_main_screens(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataScreens");
  srna = RNA_def_struct(brna, "BlendDataScreens", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Screens", "Collection of screens");

  func = RNA_def_function(srna, "tag", "rna_Main_screens_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void RNA_def_main_window_managers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataWindowManagers");
  srna = RNA_def_struct(brna, "BlendDataWindowManagers", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Window Managers", "Collection of window managers");

  func = RNA_def_function(srna, "tag", "rna_Main_window_managers_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}
void RNA_def_main_images(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataImages");
  srna = RNA_def_struct(brna, "BlendDataImages", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Images", "Collection of images");

  func = RNA_def_function(srna, "new", "rna_Main_images_new");
  RNA_def_function_ui_description(func, "Add a new image to the main database");
  parm = RNA_def_string(func, "name", "Image", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "width", 1024, 1, INT_MAX, "", "Width of the image", 1, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_int(func, "height", 1024, 1, INT_MAX, "", "Height of the image", 1, INT_MAX);
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  RNA_def_boolean(func, "alpha", false, "Alpha", "Use alpha channel");
  RNA_def_boolean(
      func, "float_buffer", false, "Float Buffer", "Create an image with floating-point color");
  RNA_def_boolean(func, "stereo3d", false, "Stereo 3D", "Create left and right views");
  RNA_def_boolean(
      func, "is_data", false, "Is Data", "Create image with non-color data color space");
  RNA_def_boolean(func, "tiled", false, "Tiled", "Create a tiled image");
  /* return type */
  parm = RNA_def_pointer(func, "image", "Image", "", "New image data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "load", "rna_Main_images_load");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Load a new image into the main database");
  parm = RNA_def_string_file_path(
      func, "filepath", "File Path", 0, "", "Path of the file to load");
  RNA_def_parameter_flags(parm, PROP_PATH_SUPPORTS_BLEND_RELATIVE, PARM_REQUIRED);
  RNA_def_boolean(func,
                  "check_existing",
                  false,
                  "",
                  "Using existing data-block if this file is already loaded");
  /* return type */
  parm = RNA_def_pointer(func, "image", "Image", "", "New image data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove an image from the current blendfile");
  parm = RNA_def_pointer(func, "image", "Image", "", "Image to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(
      func, "do_unlink", true, "", "Unlink all usages of this image before deleting it");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this image");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this image");

  func = RNA_def_function(srna, "tag", "rna_Main_images_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void RNA_def_main_lattices(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataLattices");
  srna = RNA_def_struct(brna, "BlendDataLattices", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Lattices", "Collection of lattices");

  func = RNA_def_function(srna, "new", "rna_Main_lattices_new");
  RNA_def_function_ui_description(func, "Add a new lattice to the main database");
  parm = RNA_def_string(func, "name", "Lattice", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "lattice", "Lattice", "", "New lattice data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a lattice from the current blendfile");
  parm = RNA_def_pointer(func, "lattice", "Lattice", "", "Lattice to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(func,
                  "do_unlink",
                  true,
                  "",
                  "Unlink all usages of this lattice before deleting it "
                  "(WARNING: will also delete objects instancing that lattice data)");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this lattice data");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this lattice data");

  func = RNA_def_function(srna, "tag", "rna_Main_lattices_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}
void RNA_def_main_curves(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataCurves");
  srna = RNA_def_struct(brna, "BlendDataCurves", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Curves", "Collection of curves");

  func = RNA_def_function(srna, "new", "rna_Main_curves_new");
  RNA_def_function_ui_description(func, "Add a new curve to the main database");
  parm = RNA_def_string(func, "name", "Curve", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum(
      func, "type", rna_enum_object_type_curve_items, 0, "Type", "The type of curve to add");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "curve", "Curve", "", "New curve data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a curve from the current blendfile");
  parm = RNA_def_pointer(func, "curve", "Curve", "", "Curve to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(func,
                  "do_unlink",
                  true,
                  "",
                  "Unlink all usages of this curve before deleting it "
                  "(WARNING: will also delete objects instancing that curve data)");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this curve data");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this curve data");

  func = RNA_def_function(srna, "tag", "rna_Main_curves_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}
void RNA_def_main_metaballs(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataMetaBalls");
  srna = RNA_def_struct(brna, "BlendDataMetaBalls", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Metaballs", "Collection of metaballs");

  func = RNA_def_function(srna, "new", "rna_Main_metaballs_new");
  RNA_def_function_ui_description(func, "Add a new metaball to the main database");
  parm = RNA_def_string(func, "name", "MetaBall", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "metaball", "MetaBall", "", "New metaball data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a metaball from the current blendfile");
  parm = RNA_def_pointer(func, "metaball", "MetaBall", "", "Metaball to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(func,
                  "do_unlink",
                  true,
                  "",
                  "Unlink all usages of this metaball before deleting it "
                  "(WARNING: will also delete objects instancing that metaball data)");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this metaball data");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this metaball data");

  func = RNA_def_function(srna, "tag", "rna_Main_metaballs_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}
void RNA_def_main_fonts(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataFonts");
  srna = RNA_def_struct(brna, "BlendDataFonts", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Fonts", "Collection of fonts");

  func = RNA_def_function(srna, "load", "rna_Main_fonts_load");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Load a new font into the main database");
  parm = RNA_def_string_file_path(
      func, "filepath", "File Path", 0, "", "path of the font to load");
  RNA_def_parameter_flags(parm, PROP_PATH_SUPPORTS_BLEND_RELATIVE, PARM_REQUIRED);
  RNA_def_boolean(func,
                  "check_existing",
                  false,
                  "",
                  "Using existing data-block if this file is already loaded");
  /* return type */
  parm = RNA_def_pointer(func, "vfont", "VectorFont", "", "New font data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a font from the current blendfile");
  parm = RNA_def_pointer(func, "vfont", "VectorFont", "", "Font to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(
      func, "do_unlink", true, "", "Unlink all usages of this font before deleting it");
  RNA_def_boolean(
      func, "do_id_user", true, "", "Decrement user counter of all data-blocks used by this font");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this font");

  func = RNA_def_function(srna, "tag", "rna_Main_fonts_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}
void RNA_def_main_textures(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataTextures");
  srna = RNA_def_struct(brna, "BlendDataTextures", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Textures", "Collection of textures");

  func = RNA_def_function(srna, "new", "rna_Main_textures_new");
  RNA_def_function_ui_description(func, "Add a new texture to the main database");
  parm = RNA_def_string(func, "name", "Texture", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum(
      func, "type", rna_enum_texture_type_items, 0, "Type", "The type of texture to add");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "texture", "Texture", "", "New texture data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a texture from the current blendfile");
  parm = RNA_def_pointer(func, "texture", "Texture", "", "Texture to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(
      func, "do_unlink", true, "", "Unlink all usages of this texture before deleting it");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this texture");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this texture");

  func = RNA_def_function(srna, "tag", "rna_Main_textures_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}
void RNA_def_main_brushes(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataBrushes");
  srna = RNA_def_struct(brna, "BlendDataBrushes", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Brushes", "Collection of brushes");

  func = RNA_def_function(srna, "new", "rna_Main_brushes_new");
  RNA_def_function_ui_description(func, "Add a new brush to the main database");
  parm = RNA_def_string(func, "name", "Brush", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum(func,
                      "mode",
                      rna_enum_object_mode_items,
                      OB_MODE_TEXTURE_PAINT,
                      "",
                      "Paint Mode for the new brush");
  /* return type */
  parm = RNA_def_pointer(func, "brush", "Brush", "", "New brush data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a brush from the current blendfile");
  parm = RNA_def_pointer(func, "brush", "Brush", "", "Brush to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(
      func, "do_unlink", true, "", "Unlink all usages of this brush before deleting it");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this brush");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this brush");

  func = RNA_def_function(srna, "tag", "rna_Main_brushes_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "create_gpencil_data", "rna_Main_brush_gpencil_data");
  RNA_def_function_ui_description(func, "Add Grease Pencil brush settings");
  parm = RNA_def_pointer(func, "brush", "Brush", "", "Brush");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
}

void RNA_def_main_worlds(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataWorlds");
  srna = RNA_def_struct(brna, "BlendDataWorlds", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Worlds", "Collection of worlds");

  func = RNA_def_function(srna, "new", "rna_Main_worlds_new");
  RNA_def_function_ui_description(func, "Add a new world to the main database");
  parm = RNA_def_string(func, "name", "World", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "world", "World", "", "New world data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a world from the current blendfile");
  parm = RNA_def_pointer(func, "world", "World", "", "World to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(
      func, "do_unlink", true, "", "Unlink all usages of this world before deleting it");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this world");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this world");

  func = RNA_def_function(srna, "tag", "rna_Main_worlds_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void RNA_def_main_collections(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataCollections");
  srna = RNA_def_struct(brna, "BlendDataCollections", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Collections", "Collection of collections");

  func = RNA_def_function(srna, "new", "rna_Main_collections_new");
  RNA_def_function_ui_description(func, "Add a new collection to the main database");
  parm = RNA_def_string(func, "name", "Collection", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "collection", "Collection", "", "New collection data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_ui_description(func, "Remove a collection from the current blendfile");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "collection", "Collection", "", "Collection to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(
      func, "do_unlink", true, "", "Unlink all usages of this collection before deleting it");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this collection");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this collection");

  func = RNA_def_function(srna, "tag", "rna_Main_collections_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void RNA_def_main_speakers(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataSpeakers");
  srna = RNA_def_struct(brna, "BlendDataSpeakers", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Speakers", "Collection of speakers");

  func = RNA_def_function(srna, "new", "rna_Main_speakers_new");
  RNA_def_function_ui_description(func, "Add a new speaker to the main database");
  parm = RNA_def_string(func, "name", "Speaker", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "speaker", "Speaker", "", "New speaker data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a speaker from the current blendfile");
  parm = RNA_def_pointer(func, "speaker", "Speaker", "", "Speaker to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(func,
                  "do_unlink",
                  true,
                  "",
                  "Unlink all usages of this speaker before deleting it "
                  "(WARNING: will also delete objects instancing that speaker data)");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this speaker data");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this speaker data");

  func = RNA_def_function(srna, "tag", "rna_Main_speakers_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void RNA_def_main_texts(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataTexts");
  srna = RNA_def_struct(brna, "BlendDataTexts", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Texts", "Collection of texts");

  func = RNA_def_function(srna, "new", "rna_Main_texts_new");
  RNA_def_function_ui_description(func, "Add a new text to the main database");
  parm = RNA_def_string(func, "name", "Text", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "text", "Text", "", "New text data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_ui_description(func, "Remove a text from the current blendfile");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  parm = RNA_def_pointer(func, "text", "Text", "", "Text to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(
      func, "do_unlink", true, "", "Unlink all usages of this text before deleting it");
  RNA_def_boolean(
      func, "do_id_user", true, "", "Decrement user counter of all data-blocks used by this text");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this text");

  /* load func */
  func = RNA_def_function(srna, "load", "rna_Main_texts_load");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Add a new text to the main database from a file");
  parm = RNA_def_string_file_path(
      func, "filepath", "Path", FILE_MAX, "", "path for the data-block");
  RNA_def_parameter_flags(parm, PROP_PATH_SUPPORTS_BLEND_RELATIVE, PARM_REQUIRED);
  parm = RNA_def_boolean(
      func, "internal", false, "Make internal", "Make text file internal after loading");
  /* return type */
  parm = RNA_def_pointer(func, "text", "Text", "", "New text data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "tag", "rna_Main_texts_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void RNA_def_main_sounds(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataSounds");
  srna = RNA_def_struct(brna, "BlendDataSounds", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Sounds", "Collection of sounds");

  /* load func */
  func = RNA_def_function(srna, "load", "rna_Main_sounds_load");
  RNA_def_function_ui_description(func, "Add a new sound to the main database from a file");
  parm = RNA_def_string_file_path(
      func, "filepath", "Path", FILE_MAX, "", "path for the data-block");
  RNA_def_parameter_flags(parm, PROP_PATH_SUPPORTS_BLEND_RELATIVE, PARM_REQUIRED);
  RNA_def_boolean(func,
                  "check_existing",
                  false,
                  "",
                  "Using existing data-block if this file is already loaded");
  /* return type */
  parm = RNA_def_pointer(func, "sound", "Sound", "", "New text data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a sound from the current blendfile");
  parm = RNA_def_pointer(func, "sound", "Sound", "", "Sound to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(
      func, "do_unlink", true, "", "Unlink all usages of this sound before deleting it");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this sound");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this sound");

  func = RNA_def_function(srna, "tag", "rna_Main_sounds_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void RNA_def_main_armatures(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataArmatures");
  srna = RNA_def_struct(brna, "BlendDataArmatures", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Armatures", "Collection of armatures");

  func = RNA_def_function(srna, "new", "rna_Main_armatures_new");
  RNA_def_function_ui_description(func, "Add a new armature to the main database");
  parm = RNA_def_string(func, "name", "Armature", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "armature", "Armature", "", "New armature data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove an armature from the current blendfile");
  parm = RNA_def_pointer(func, "armature", "Armature", "", "Armature to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(func,
                  "do_unlink",
                  true,
                  "",
                  "Unlink all usages of this armature before deleting it "
                  "(WARNING: will also delete objects instancing that armature data)");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this armature data");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this armature data");

  func = RNA_def_function(srna, "tag", "rna_Main_armatures_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}
void RNA_def_main_actions(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataActions");
  srna = RNA_def_struct(brna, "BlendDataActions", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Actions", "Collection of actions");

  func = RNA_def_function(srna, "new", "rna_Main_actions_new");
  RNA_def_function_ui_description(func, "Add a new action to the main database");
  parm = RNA_def_string(func, "name", "Action", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "action", "Action", "", "New action data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove an action from the current blendfile");
  parm = RNA_def_pointer(func, "action", "Action", "", "Action to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(
      func, "do_unlink", true, "", "Unlink all usages of this action before deleting it");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this action");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this action");

  func = RNA_def_function(srna, "tag", "rna_Main_actions_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void RNA_def_main_particles(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataParticles");
  srna = RNA_def_struct(brna, "BlendDataParticles", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Particle Settings", "Collection of particle settings");

  func = RNA_def_function(srna, "new", "rna_Main_particles_new");
  RNA_def_function_ui_description(func,
                                  "Add a new particle settings instance to the main database");
  parm = RNA_def_string(func, "name", "ParticleSettings", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(
      func, "particle", "ParticleSettings", "", "New particle settings data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(
      func, "Remove a particle settings instance from the current blendfile");
  parm = RNA_def_pointer(func, "particle", "ParticleSettings", "", "Particle Settings to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(func,
                  "do_unlink",
                  true,
                  "",
                  "Unlink all usages of those particle settings before deleting them");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this particle settings");
  RNA_def_boolean(func,
                  "do_ui_user",
                  true,
                  "",
                  "Make sure interface does not reference this particle settings");

  func = RNA_def_function(srna, "tag", "rna_Main_particles_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void RNA_def_main_palettes(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataPalettes");
  srna = RNA_def_struct(brna, "BlendDataPalettes", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Palettes", "Collection of palettes");

  func = RNA_def_function(srna, "new", "rna_Main_palettes_new");
  RNA_def_function_ui_description(func, "Add a new palette to the main database");
  parm = RNA_def_string(func, "name", "Palette", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "palette", "Palette", "", "New palette data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a palette from the current blendfile");
  parm = RNA_def_pointer(func, "palette", "Palette", "", "Palette to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(
      func, "do_unlink", true, "", "Unlink all usages of this palette before deleting it");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this palette");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this palette");

  func = RNA_def_function(srna, "tag", "rna_Main_palettes_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}
void RNA_def_main_cachefiles(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataCacheFiles");
  srna = RNA_def_struct(brna, "BlendDataCacheFiles", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Cache Files", "Collection of cache files");

  func = RNA_def_function(srna, "tag", "rna_Main_cachefiles_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}
void RNA_def_main_paintcurves(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataPaintCurves");
  srna = RNA_def_struct(brna, "BlendDataPaintCurves", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Paint Curves", "Collection of paint curves");

  func = RNA_def_function(srna, "tag", "rna_Main_paintcurves_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}
void RNA_def_main_annotations(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataAnnotations");
  srna = RNA_def_struct(brna, "BlendDataAnnotations", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Annotations", "Collection of annotations");

  func = RNA_def_function(srna, "tag", "rna_Main_gpencils_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "new", "rna_Main_annotations_new");
  RNA_def_function_ui_description(func, "Add a new annotation data-block to the main database");
  parm = RNA_def_string(func, "name", "Annotation", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "annotation", "Annotation", "", "New annotation data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove annotation instance from the current blendfile");
  parm = RNA_def_pointer(func, "annotation", "Annotation", "", "Grease Pencil to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(
      func, "do_unlink", true, "", "Unlink all usages of this annotation before deleting it");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this annotation");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this annotation");
}

void RNA_def_main_grease_pencil(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataGreasePencilsV3");
  srna = RNA_def_struct(brna, "BlendDataGreasePencilsV3", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Grease Pencils", "Collection of Grease Pencils");

  func = RNA_def_function(srna, "tag", "rna_Main_grease_pencils_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "new", "rna_Main_grease_pencils_new");
  RNA_def_function_ui_description(func, "Add a new Grease Pencil data-block to the main database");
  parm = RNA_def_string(func, "name", "GreasePencil", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(
      func, "grease_pencil", "GreasePencil", "", "New Grease Pencil data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func,
                                  "Remove a Grease Pencil instance from the current blendfile");
  parm = RNA_def_pointer(func, "grease_pencil", "GreasePencil", "", "Grease Pencil to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(
      func, "do_unlink", true, "", "Unlink all usages of this Grease Pencil before deleting it");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this Grease Pencil");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this Grease Pencil");
}

void RNA_def_main_movieclips(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataMovieClips");
  srna = RNA_def_struct(brna, "BlendDataMovieClips", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Movie Clips", "Collection of movie clips");

  func = RNA_def_function(srna, "tag", "rna_Main_movieclips_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a movie clip from the current blendfile.");
  parm = RNA_def_pointer(func, "clip", "MovieClip", "", "Movie clip to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(
      func, "do_unlink", true, "", "Unlink all usages of this movie clip before deleting it");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this movie clip");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this movie clip");

  /* load func */
  func = RNA_def_function(srna, "load", "rna_Main_movieclip_load");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(
      func,
      "Add a new movie clip to the main database from a file "
      "(while ``check_existing`` is disabled for consistency with other load functions, "
      "behavior with multiple movie-clips using the same file may incorrectly generate proxies)");
  parm = RNA_def_string_file_path(
      func, "filepath", "Path", FILE_MAX, "", "path for the data-block");
  RNA_def_parameter_flags(parm, PROP_PATH_SUPPORTS_BLEND_RELATIVE, PARM_REQUIRED);
  RNA_def_boolean(func,
                  "check_existing",
                  false,
                  "",
                  "Using existing data-block if this file is already loaded");
  /* return type */
  parm = RNA_def_pointer(func, "clip", "MovieClip", "", "New movie clip data-block");
  RNA_def_function_return(func, parm);
}

void RNA_def_main_masks(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataMasks");
  srna = RNA_def_struct(brna, "BlendDataMasks", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Masks", "Collection of masks");

  func = RNA_def_function(srna, "tag", "rna_Main_masks_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  /* new func */
  func = RNA_def_function(srna, "new", "rna_Main_mask_new");
  RNA_def_function_ui_description(func, "Add a new mask with a given name to the main database");
  parm = RNA_def_string(
      func, "name", nullptr, MAX_ID_NAME - 2, "Mask", "Name of new mask data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "mask", "Mask", "", "New mask data-block");
  RNA_def_function_return(func, parm);

  /* remove func */
  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a mask from the current blendfile");
  parm = RNA_def_pointer(func, "mask", "Mask", "", "Mask to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(
      func, "do_unlink", true, "", "Unlink all usages of this mask before deleting it");
  RNA_def_boolean(
      func, "do_id_user", true, "", "Decrement user counter of all data-blocks used by this mask");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this mask");
}

void RNA_def_main_linestyles(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataLineStyles");
  srna = RNA_def_struct(brna, "BlendDataLineStyles", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Line Styles", "Collection of line styles");

  func = RNA_def_function(srna, "tag", "rna_Main_linestyle_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);

  func = RNA_def_function(srna, "new", "rna_Main_linestyles_new");
  RNA_def_function_ui_description(func, "Add a new line style instance to the main database");
  parm = RNA_def_string(func, "name", "FreestyleLineStyle", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "linestyle", "FreestyleLineStyle", "", "New line style data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a line style instance from the current blendfile");
  parm = RNA_def_pointer(func, "linestyle", "FreestyleLineStyle", "", "Line style to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(
      func, "do_unlink", true, "", "Unlink all usages of this line style before deleting it");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this line style");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this line style");
}

void RNA_def_main_workspaces(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataWorkSpaces");
  srna = RNA_def_struct(brna, "BlendDataWorkSpaces", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Workspaces", "Collection of workspaces");

  func = RNA_def_function(srna, "tag", "rna_Main_workspaces_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void RNA_def_main_lightprobes(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataProbes");
  srna = RNA_def_struct(brna, "BlendDataProbes", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Light Probes", "Collection of light probes");

  func = RNA_def_function(srna, "new", "rna_Main_lightprobe_new");
  RNA_def_function_ui_description(func, "Add a new light probe to the main database");
  parm = RNA_def_string(func, "name", "Probe", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  parm = RNA_def_enum(
      func, "type", rna_enum_lightprobes_type_items, 0, "Type", "The type of light probe to add");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "lightprobe", "LightProbe", "", "New light probe data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a light probe from the current blendfile");
  parm = RNA_def_pointer(func, "lightprobe", "LightProbe", "", "Light probe to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(func,
                  "do_unlink",
                  true,
                  "",
                  "Unlink all usages of this light probe before deleting it "
                  "(WARNING: will also delete objects instancing that light probe data)");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this light probe");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this light probe");

  func = RNA_def_function(srna, "tag", "rna_Main_lightprobes_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void RNA_def_main_hair_curves(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataHairCurves");
  srna = RNA_def_struct(brna, "BlendDataHairCurves", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Hair Curves", "Collection of hair curves");

  func = RNA_def_function(srna, "new", "rna_Main_hair_curves_new");
  RNA_def_function_ui_description(func, "Add a new hair to the main database");
  parm = RNA_def_string(func, "name", "Curves", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "curves", "Curves", "", "New curves data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a curves data-block from the current blendfile");
  parm = RNA_def_pointer(func, "curves", "Curves", "", "Curves data-block to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(func,
                  "do_unlink",
                  true,
                  "",
                  "Unlink all usages of this curves before deleting it "
                  "(WARNING: will also delete objects instancing that curves data)");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this curves data");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this curves data");

  func = RNA_def_function(srna, "tag", "rna_Main_hair_curves_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void RNA_def_main_pointclouds(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataPointClouds");
  srna = RNA_def_struct(brna, "BlendDataPointClouds", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Point Clouds", "Collection of point clouds");

  func = RNA_def_function(srna, "new", "rna_Main_pointclouds_new");
  RNA_def_function_ui_description(func, "Add a new point cloud to the main database");
  parm = RNA_def_string(func, "name", "PointCloud", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "pointcloud", "PointCloud", "", "New point cloud data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a point cloud from the current blendfile");
  parm = RNA_def_pointer(func, "pointcloud", "PointCloud", "", "Point cloud to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(func,
                  "do_unlink",
                  true,
                  "",
                  "Unlink all usages of this point cloud before deleting it "
                  "(WARNING: will also delete objects instancing that point cloud data)");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this point cloud data");
  RNA_def_boolean(func,
                  "do_ui_user",
                  true,
                  "",
                  "Make sure interface does not reference this point cloud data");

  func = RNA_def_function(srna, "tag", "rna_Main_pointclouds_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

void RNA_def_main_volumes(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "BlendDataVolumes");
  srna = RNA_def_struct(brna, "BlendDataVolumes", nullptr);
  RNA_def_struct_sdna(srna, "Main");
  RNA_def_struct_ui_text(srna, "Main Volumes", "Collection of volumes");

  func = RNA_def_function(srna, "new", "rna_Main_volumes_new");
  RNA_def_function_ui_description(func, "Add a new volume to the main database");
  parm = RNA_def_string(func, "name", "Volume", 0, "", "New name for the data-block");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
  /* return type */
  parm = RNA_def_pointer(func, "volume", "Volume", "", "New volume data-block");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_Main_ID_remove");
  RNA_def_function_flag(func, FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove a volume from the current blendfile");
  parm = RNA_def_pointer(func, "volume", "Volume", "", "Volume to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, ParameterFlag(0));
  RNA_def_boolean(func,
                  "do_unlink",
                  true,
                  "",
                  "Unlink all usages of this volume before deleting it "
                  "(WARNING: will also delete objects instancing that volume data)");
  RNA_def_boolean(func,
                  "do_id_user",
                  true,
                  "",
                  "Decrement user counter of all data-blocks used by this volume data");
  RNA_def_boolean(
      func, "do_ui_user", true, "", "Make sure interface does not reference this volume data");

  func = RNA_def_function(srna, "tag", "rna_Main_volumes_tag");
  parm = RNA_def_boolean(func, "value", false, "Value", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED);
}

#endif

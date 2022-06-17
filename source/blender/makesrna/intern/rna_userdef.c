/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <limits.h>
#include <stdlib.h>

#include "DNA_brush_types.h"
#include "DNA_curve_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "BLI_math_base.h"
#include "BLI_math_rotation.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "BKE_addon.h"
#include "BKE_appdir.h"
#include "BKE_sound.h"
#include "BKE_studiolight.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "UI_interface_icons.h"

#include "rna_internal.h"

#include "WM_api.h"
#include "WM_types.h"

#include "BLT_lang.h"

#ifdef WITH_OPENSUBDIV
static const EnumPropertyItem opensubdiv_compute_type_items[] = {
    {USER_OPENSUBDIV_COMPUTE_NONE, "NONE", 0, "None", ""},
    {USER_OPENSUBDIV_COMPUTE_CPU, "CPU", 0, "CPU", ""},
    {USER_OPENSUBDIV_COMPUTE_OPENMP, "OPENMP", 0, "OpenMP", ""},
    {USER_OPENSUBDIV_COMPUTE_OPENCL, "OPENCL", 0, "OpenCL", ""},
    {USER_OPENSUBDIV_COMPUTE_CUDA, "CUDA", 0, "CUDA", ""},
    {USER_OPENSUBDIV_COMPUTE_GLSL_TRANSFORM_FEEDBACK,
     "GLSL_TRANSFORM_FEEDBACK",
     0,
     "GLSL Transform Feedback",
     ""},
    {USER_OPENSUBDIV_COMPUTE_GLSL_COMPUTE, "GLSL_COMPUTE", 0, "GLSL Compute", ""},
    {0, NULL, 0, NULL, NULL},
};
#endif

const EnumPropertyItem rna_enum_preference_section_items[] = {
    {USER_SECTION_INTERFACE, "INTERFACE", 0, "Interface", ""},
    {USER_SECTION_THEME, "THEMES", 0, "Themes", ""},
    {USER_SECTION_VIEWPORT, "VIEWPORT", 0, "Viewport", ""},
    {USER_SECTION_LIGHT, "LIGHTS", 0, "Lights", ""},
    {USER_SECTION_EDITING, "EDITING", 0, "Editing", ""},
    {USER_SECTION_ANIMATION, "ANIMATION", 0, "Animation", ""},
    RNA_ENUM_ITEM_SEPR,
    {USER_SECTION_ADDONS, "ADDONS", 0, "Add-ons", ""},
#if 0 /* def WITH_USERDEF_WORKSPACES */
    RNA_ENUM_ITEM_SEPR,
    {USER_SECTION_WORKSPACE_CONFIG, "WORKSPACE_CONFIG", 0, "Configuration File", ""},
    {USER_SECTION_WORKSPACE_ADDONS, "WORKSPACE_ADDONS", 0, "Add-on Overrides", ""},
    {USER_SECTION_WORKSPACE_KEYMAPS, "WORKSPACE_KEYMAPS", 0, "Keymap Overrides", ""},
#endif
    RNA_ENUM_ITEM_SEPR,
    {USER_SECTION_INPUT, "INPUT", 0, "Input", ""},
    {USER_SECTION_NAVIGATION, "NAVIGATION", 0, "Navigation", ""},
    {USER_SECTION_KEYMAP, "KEYMAP", 0, "Keymap", ""},
    RNA_ENUM_ITEM_SEPR,
    {USER_SECTION_SYSTEM, "SYSTEM", 0, "System", ""},
    {USER_SECTION_SAVE_LOAD, "SAVE_LOAD", 0, "Save & Load", ""},
    {USER_SECTION_FILE_PATHS, "FILE_PATHS", 0, "File Paths", ""},
    RNA_ENUM_ITEM_SEPR,
    {USER_SECTION_EXPERIMENTAL, "EXPERIMENTAL", 0, "Experimental", ""},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem audio_device_items[] = {
    {0, "None", 0, "None", "No device - there will be no audio output"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_navigation_mode_items[] = {
    {VIEW_NAVIGATION_WALK,
     "WALK",
     0,
     "Walk",
     "Interactively walk or free navigate around the scene"},
    {VIEW_NAVIGATION_FLY, "FLY", 0, "Fly", "Use fly dynamics to navigate the scene"},
    {0, NULL, 0, NULL, NULL},
};

#if defined(WITH_INTERNATIONAL) || !defined(RNA_RUNTIME)
static const EnumPropertyItem rna_enum_language_default_items[] = {
    {0,
     "DEFAULT",
     0,
     "Automatic (Automatic)",
     "Automatically choose system's defined language if available, or fall-back to English"},
    {0, NULL, 0, NULL, NULL},
};
#endif

static const EnumPropertyItem rna_enum_studio_light_type_items[] = {
    {STUDIOLIGHT_TYPE_STUDIO, "STUDIO", 0, "Studio", ""},
    {STUDIOLIGHT_TYPE_WORLD, "WORLD", 0, "World", ""},
    {STUDIOLIGHT_TYPE_MATCAP, "MATCAP", 0, "MatCap", ""},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem rna_enum_userdef_viewport_aa_items[] = {
    {SCE_DISPLAY_AA_OFF,
     "OFF",
     0,
     "No Anti-Aliasing",
     "Scene will be rendering without any anti-aliasing"},
    {SCE_DISPLAY_AA_FXAA,
     "FXAA",
     0,
     "Single Pass Anti-Aliasing",
     "Scene will be rendered using a single pass anti-aliasing method (FXAA)"},
    {SCE_DISPLAY_AA_SAMPLES_5,
     "5",
     0,
     "5 Samples",
     "Scene will be rendered using 5 anti-aliasing samples"},
    {SCE_DISPLAY_AA_SAMPLES_8,
     "8",
     0,
     "8 Samples",
     "Scene will be rendered using 8 anti-aliasing samples"},
    {SCE_DISPLAY_AA_SAMPLES_11,
     "11",
     0,
     "11 Samples",
     "Scene will be rendered using 11 anti-aliasing samples"},
    {SCE_DISPLAY_AA_SAMPLES_16,
     "16",
     0,
     "16 Samples",
     "Scene will be rendered using 16 anti-aliasing samples"},
    {SCE_DISPLAY_AA_SAMPLES_32,
     "32",
     0,
     "32 Samples",
     "Scene will be rendered using 32 anti-aliasing samples"},
    {0, NULL, 0, NULL, NULL},
};

#ifdef RNA_RUNTIME

#  include "BLI_math_vector.h"

#  include "DNA_object_types.h"
#  include "DNA_screen_types.h"

#  include "BKE_blender.h"
#  include "BKE_global.h"
#  include "BKE_idprop.h"
#  include "BKE_image.h"
#  include "BKE_main.h"
#  include "BKE_mesh_runtime.h"
#  include "BKE_object.h"
#  include "BKE_paint.h"
#  include "BKE_pbvh.h"
#  include "BKE_preferences.h"
#  include "BKE_screen.h"

#  include "DEG_depsgraph.h"

#  include "GPU_capabilities.h"
#  include "GPU_select.h"
#  include "GPU_texture.h"

#  include "BLF_api.h"

#  include "BLI_path_util.h"

#  include "MEM_CacheLimiterC-Api.h"
#  include "MEM_guardedalloc.h"

#  include "UI_interface.h"

#  ifdef WITH_OPENSUBDIV
#    include "opensubdiv_capi.h"
#  endif

#  ifdef WITH_SDL_DYNLOAD
#    include "sdlew.h"
#  endif

static void rna_userdef_version_get(PointerRNA *ptr, int *value)
{
  UserDef *userdef = (UserDef *)ptr->data;
  value[0] = userdef->versionfile / 100;
  value[1] = userdef->versionfile % 100;
  value[2] = userdef->subversionfile;
}

/** Mark the preferences as being changed so they are saved on exit. */
#  define USERDEF_TAG_DIRTY rna_userdef_is_dirty_update_impl()

void rna_userdef_is_dirty_update_impl(void)
{
  /* We can't use 'ptr->data' because this update function
   * is used for themes and other nested data. */
  if (U.runtime.is_dirty == false) {
    U.runtime.is_dirty = true;
    WM_main_add_notifier(NC_WINDOW, NULL);
  }
}

void rna_userdef_is_dirty_update(Main *UNUSED(bmain),
                                 Scene *UNUSED(scene),
                                 PointerRNA *UNUSED(ptr))
{
  /* WARNING: never use 'ptr' unless its type is checked. */
  rna_userdef_is_dirty_update_impl();
}

/** Take care not to use this if we expect 'is_dirty' to be tagged. */
static void rna_userdef_ui_update(Main *UNUSED(bmain),
                                  Scene *UNUSED(scene),
                                  PointerRNA *UNUSED(ptr))
{
  WM_main_add_notifier(NC_WINDOW, NULL);
}

static void rna_userdef_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *UNUSED(ptr))
{
  WM_main_add_notifier(NC_WINDOW, NULL);
  USERDEF_TAG_DIRTY;
}

static void rna_userdef_theme_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  /* Recreate gizmos when changing themes. */
  WM_reinit_gizmomap_all(bmain);

  rna_userdef_update(bmain, scene, ptr);
}

static void rna_userdef_theme_text_style_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  const uiStyle *style = UI_style_get();
  BLF_default_size(style->widgetlabel.points);

  rna_userdef_update(bmain, scene, ptr);
}

static void rna_userdef_gizmo_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  WM_reinit_gizmomap_all(bmain);

  rna_userdef_update(bmain, scene, ptr);
}

static void rna_userdef_theme_update_icons(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  if (!G.background) {
    UI_icons_reload_internal_textures();
  }
  rna_userdef_theme_update(bmain, scene, ptr);
}

/* also used by buffer swap switching */
static void rna_userdef_dpi_update(Main *UNUSED(bmain),
                                   Scene *UNUSED(scene),
                                   PointerRNA *UNUSED(ptr))
{
  /* font's are stored at each DPI level, without this we can easy load 100's of fonts */
  BLF_cache_clear();

  WM_main_add_notifier(NC_WINDOW, NULL);             /* full redraw */
  WM_main_add_notifier(NC_SCREEN | NA_EDITED, NULL); /* refresh region sizes */
  USERDEF_TAG_DIRTY;
}

static void rna_userdef_screen_update(Main *UNUSED(bmain),
                                      Scene *UNUSED(scene),
                                      PointerRNA *UNUSED(ptr))
{
  WM_main_add_notifier(NC_WINDOW, NULL);
  WM_main_add_notifier(NC_SCREEN | NA_EDITED, NULL); /* refresh region sizes */
  USERDEF_TAG_DIRTY;
}

static void rna_userdef_screen_update_header_default(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  if (U.uiflag & USER_HEADER_FROM_PREF) {
    for (bScreen *screen = bmain->screens.first; screen; screen = screen->id.next) {
      BKE_screen_header_alignment_reset(screen);
    }
    rna_userdef_screen_update(bmain, scene, ptr);
  }
  USERDEF_TAG_DIRTY;
}

static void rna_userdef_font_update(Main *UNUSED(bmain),
                                    Scene *UNUSED(scene),
                                    PointerRNA *UNUSED(ptr))
{
  BLF_cache_clear();
  UI_reinit_font();
}

static void rna_userdef_language_update(Main *UNUSED(bmain),
                                        Scene *UNUSED(scene),
                                        PointerRNA *UNUSED(ptr))
{
  BLT_lang_set(NULL);

  const char *uilng = BLT_lang_get();
  if (STREQ(uilng, "en_US")) {
    U.transopts &= ~(USER_TR_IFACE | USER_TR_TOOLTIPS | USER_TR_NEWDATANAME);
  }
  else {
    U.transopts |= (USER_TR_IFACE | USER_TR_TOOLTIPS | USER_TR_NEWDATANAME);
  }

  USERDEF_TAG_DIRTY;
}

static void rna_userdef_asset_library_name_set(PointerRNA *ptr, const char *value)
{
  bUserAssetLibrary *library = (bUserAssetLibrary *)ptr->data;
  BKE_preferences_asset_library_name_set(&U, library, value);
}

static void rna_userdef_asset_library_path_set(PointerRNA *ptr, const char *value)
{
  bUserAssetLibrary *library = (bUserAssetLibrary *)ptr->data;
  BKE_preferences_asset_library_path_set(library, value);
}

static void rna_userdef_script_autoexec_update(Main *UNUSED(bmain),
                                               Scene *UNUSED(scene),
                                               PointerRNA *ptr)
{
  UserDef *userdef = (UserDef *)ptr->data;
  if (userdef->flag & USER_SCRIPT_AUTOEXEC_DISABLE) {
    G.f &= ~G_FLAG_SCRIPT_AUTOEXEC;
  }
  else {
    G.f |= G_FLAG_SCRIPT_AUTOEXEC;
  }

  USERDEF_TAG_DIRTY;
}

static void rna_userdef_load_ui_update(Main *UNUSED(bmain), Scene *UNUSED(scene), PointerRNA *ptr)
{
  UserDef *userdef = (UserDef *)ptr->data;
  if (userdef->flag & USER_FILENOUI) {
    G.fileflags |= G_FILE_NO_UI;
  }
  else {
    G.fileflags &= ~G_FILE_NO_UI;
  }

  USERDEF_TAG_DIRTY;
}

static void rna_userdef_anisotropic_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  GPU_samplers_update();
  rna_userdef_update(bmain, scene, ptr);
}

static void rna_userdef_gl_texture_limit_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  BKE_image_free_all_gputextures(bmain);
  rna_userdef_update(bmain, scene, ptr);
}

static void rna_userdef_undo_steps_set(PointerRNA *ptr, int value)
{
  UserDef *userdef = (UserDef *)ptr->data;

  /* Do not allow 1 undo steps, useless and breaks undo/redo process (see T42531). */
  userdef->undosteps = (value == 1) ? 2 : value;
}

static int rna_userdef_autokeymode_get(PointerRNA *ptr)
{
  UserDef *userdef = (UserDef *)ptr->data;
  short retval = userdef->autokey_mode;

  if (!(userdef->autokey_mode & AUTOKEY_ON)) {
    retval |= AUTOKEY_ON;
  }

  return retval;
}

static void rna_userdef_autokeymode_set(PointerRNA *ptr, int value)
{
  UserDef *userdef = (UserDef *)ptr->data;

  if (value == AUTOKEY_MODE_NORMAL) {
    userdef->autokey_mode |= (AUTOKEY_MODE_NORMAL - AUTOKEY_ON);
    userdef->autokey_mode &= ~(AUTOKEY_MODE_EDITKEYS - AUTOKEY_ON);
  }
  else if (value == AUTOKEY_MODE_EDITKEYS) {
    userdef->autokey_mode |= (AUTOKEY_MODE_EDITKEYS - AUTOKEY_ON);
    userdef->autokey_mode &= ~(AUTOKEY_MODE_NORMAL - AUTOKEY_ON);
  }
}

static void rna_userdef_anim_update(Main *UNUSED(bmain),
                                    Scene *UNUSED(scene),
                                    PointerRNA *UNUSED(ptr))
{
  WM_main_add_notifier(NC_SPACE | ND_SPACE_GRAPH, NULL);
  WM_main_add_notifier(NC_SPACE | ND_SPACE_DOPESHEET, NULL);
  USERDEF_TAG_DIRTY;
}

static void rna_userdef_tablet_api_update(Main *UNUSED(bmain),
                                          Scene *UNUSED(scene),
                                          PointerRNA *UNUSED(ptr))
{
  WM_init_tablet_api();
  USERDEF_TAG_DIRTY;
}

#  ifdef WITH_INPUT_NDOF
static void rna_userdef_ndof_deadzone_update(Main *UNUSED(bmain),
                                             Scene *UNUSED(scene),
                                             PointerRNA *ptr)
{
  UserDef *userdef = ptr->data;
  WM_ndof_deadzone_set(userdef->ndof_deadzone);
  USERDEF_TAG_DIRTY;
}
#  endif

static void rna_userdef_keyconfig_reload_update(bContext *C,
                                                Main *UNUSED(bmain),
                                                Scene *UNUSED(scene),
                                                PointerRNA *UNUSED(ptr))
{
  WM_keyconfig_reload(C);
  USERDEF_TAG_DIRTY;
}

static void rna_userdef_timecode_style_set(PointerRNA *ptr, int value)
{
  UserDef *userdef = (UserDef *)ptr->data;
  int required_size = userdef->v2d_min_gridsize;

  /* Set the time-code style. */
  userdef->timecode_style = value;

  /* Adjust the v2d grid-size if needed so that time-codes don't overlap
   * NOTE: most of these have been hand-picked to avoid overlaps while still keeping
   * things from getting too blown out. */
  switch (value) {
    case USER_TIMECODE_MINIMAL:
    case USER_TIMECODE_SECONDS_ONLY:
      /* 35 is great most of the time, but not that great for full-blown */
      required_size = 35;
      break;
    case USER_TIMECODE_SMPTE_MSF:
      required_size = 50;
      break;
    case USER_TIMECODE_SMPTE_FULL:
      /* the granddaddy! */
      required_size = 65;
      break;
    case USER_TIMECODE_MILLISECONDS:
      required_size = 45;
      break;
  }

  if (U.v2d_min_gridsize < required_size) {
    U.v2d_min_gridsize = required_size;
  }
}

static int rna_UserDef_mouse_emulate_3_button_modifier_get(PointerRNA *ptr)
{
#  if !defined(WIN32)
  UserDef *userdef = ptr->data;
  return userdef->mouse_emulate_3_button_modifier;
#  else
  UNUSED_VARS(ptr);
  return USER_EMU_MMB_MOD_ALT;
#  endif
}

static const EnumPropertyItem *rna_UseDef_active_section_itemf(bContext *UNUSED(C),
                                                               PointerRNA *ptr,
                                                               PropertyRNA *UNUSED(prop),
                                                               bool *r_free)
{
  UserDef *userdef = ptr->data;

  if ((userdef->flag & USER_DEVELOPER_UI) != 0) {
    *r_free = false;
    return rna_enum_preference_section_items;
  }

  EnumPropertyItem *items = NULL;
  int totitem = 0;

  for (const EnumPropertyItem *it = rna_enum_preference_section_items; it->identifier != NULL;
       it++) {
    if (it->value == USER_SECTION_EXPERIMENTAL) {
      continue;
    }
    RNA_enum_item_add(&items, &totitem, it);
  }

  RNA_enum_item_end(&items, &totitem);

  *r_free = true;
  return items;
}

static PointerRNA rna_UserDef_view_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_PreferencesView, ptr->data);
}

static PointerRNA rna_UserDef_edit_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_PreferencesEdit, ptr->data);
}

static PointerRNA rna_UserDef_input_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_PreferencesInput, ptr->data);
}

static PointerRNA rna_UserDef_keymap_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_PreferencesKeymap, ptr->data);
}

static PointerRNA rna_UserDef_filepaths_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_PreferencesFilePaths, ptr->data);
}

static PointerRNA rna_UserDef_system_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_PreferencesSystem, ptr->data);
}

static PointerRNA rna_UserDef_apps_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_PreferencesApps, ptr->data);
}

/* Reevaluate objects with a subsurf modifier as the last in their modifiers stacks. */
static void rna_UserDef_subdivision_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  Object *ob;

  for (ob = bmain->objects.first; ob; ob = ob->id.next) {
    if (BKE_object_get_last_subsurf_modifier(ob) != NULL) {
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    }
  }

  rna_userdef_update(bmain, scene, ptr);
}

static void rna_UserDef_audio_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *UNUSED(ptr))
{
  BKE_sound_init(bmain);
  USERDEF_TAG_DIRTY;
}

static void rna_Userdef_memcache_update(Main *UNUSED(bmain),
                                        Scene *UNUSED(scene),
                                        PointerRNA *UNUSED(ptr))
{
  MEM_CacheLimiter_set_maximum(((size_t)U.memcachelimit) * 1024 * 1024);
  USERDEF_TAG_DIRTY;
}

static void rna_Userdef_disk_cache_dir_update(Main *UNUSED(bmain),
                                              Scene *UNUSED(scene),
                                              PointerRNA *UNUSED(ptr))
{
  if (U.sequencer_disk_cache_dir[0] != '\0') {
    BLI_path_abs(U.sequencer_disk_cache_dir, BKE_main_blendfile_path_from_global());
    BLI_path_slash_ensure(U.sequencer_disk_cache_dir);
    BLI_path_make_safe(U.sequencer_disk_cache_dir);
  }

  USERDEF_TAG_DIRTY;
}

static void rna_UserDef_weight_color_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  Object *ob;

  for (ob = bmain->objects.first; ob; ob = ob->id.next) {
    if (ob->mode & OB_MODE_WEIGHT_PAINT) {
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    }
  }

  rna_userdef_update(bmain, scene, ptr);
}

static void rna_UserDef_viewport_lights_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  /* If all lights are off gpu_draw resets them all, see: T27627,
   * so disallow them all to be disabled. */
  if (U.light_param[0].flag == 0 && U.light_param[1].flag == 0 && U.light_param[2].flag == 0 &&
      U.light_param[3].flag == 0) {
    SolidLight *light = ptr->data;
    light->flag |= 1;
  }

  WM_main_add_notifier(NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_GPU, NULL);
  rna_userdef_update(bmain, scene, ptr);
}

static void rna_userdef_autosave_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  wmWindowManager *wm = bmain->wm.first;

  if (wm) {
    WM_file_autosave_init(wm);
  }
  rna_userdef_update(bmain, scene, ptr);
}

#  define RNA_USERDEF_EXPERIMENTAL_BOOLEAN_GET(member) \
    static bool rna_userdef_experimental_##member##_get(PointerRNA *ptr) \
    { \
      UserDef *userdef = POINTER_OFFSET(ptr->data, -offsetof(UserDef, experimental)); \
      return USER_EXPERIMENTAL_TEST(userdef, member); \
    }

static bAddon *rna_userdef_addon_new(void)
{
  ListBase *addons_list = &U.addons;
  bAddon *addon = BKE_addon_new();
  BLI_addtail(addons_list, addon);
  USERDEF_TAG_DIRTY;
  return addon;
}

static void rna_userdef_addon_remove(ReportList *reports, PointerRNA *addon_ptr)
{
  ListBase *addons_list = &U.addons;
  bAddon *addon = addon_ptr->data;
  if (BLI_findindex(addons_list, addon) == -1) {
    BKE_report(reports, RPT_ERROR, "Add-on is no longer valid");
    return;
  }
  BLI_remlink(addons_list, addon);
  BKE_addon_free(addon);
  RNA_POINTER_INVALIDATE(addon_ptr);
  USERDEF_TAG_DIRTY;
}

static bPathCompare *rna_userdef_pathcompare_new(void)
{
  bPathCompare *path_cmp = MEM_callocN(sizeof(bPathCompare), "bPathCompare");
  BLI_addtail(&U.autoexec_paths, path_cmp);
  USERDEF_TAG_DIRTY;
  return path_cmp;
}

static void rna_userdef_pathcompare_remove(ReportList *reports, PointerRNA *path_cmp_ptr)
{
  bPathCompare *path_cmp = path_cmp_ptr->data;
  if (BLI_findindex(&U.autoexec_paths, path_cmp) == -1) {
    BKE_report(reports, RPT_ERROR, "Excluded path is no longer valid");
    return;
  }

  BLI_freelinkN(&U.autoexec_paths, path_cmp);
  RNA_POINTER_INVALIDATE(path_cmp_ptr);
  USERDEF_TAG_DIRTY;
}

static void rna_userdef_temp_update(Main *UNUSED(bmain),
                                    Scene *UNUSED(scene),
                                    PointerRNA *UNUSED(ptr))
{
  BKE_tempdir_init(U.tempdir);
  USERDEF_TAG_DIRTY;
}

static void rna_userdef_text_update(Main *UNUSED(bmain),
                                    Scene *UNUSED(scene),
                                    PointerRNA *UNUSED(ptr))
{
  BLF_cache_clear();
  UI_reinit_font();
  WM_main_add_notifier(NC_WINDOW, NULL);
  USERDEF_TAG_DIRTY;
}

static PointerRNA rna_Theme_space_generic_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_ThemeSpaceGeneric, ptr->data);
}

static PointerRNA rna_Theme_gradient_colors_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_ThemeGradientColors, ptr->data);
}

static PointerRNA rna_Theme_space_gradient_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_ThemeSpaceGradient, ptr->data);
}

static PointerRNA rna_Theme_space_list_generic_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_ThemeSpaceListGeneric, ptr->data);
}

#  ifdef WITH_OPENSUBDIV
static const EnumPropertyItem *rna_userdef_opensubdiv_compute_type_itemf(bContext *UNUSED(C),
                                                                         PointerRNA *UNUSED(ptr),
                                                                         PropertyRNA *UNUSED(prop),
                                                                         bool *r_free)
{
  EnumPropertyItem *item = NULL;
  int totitem = 0;
  int evaluators = openSubdiv_getAvailableEvaluators();

  RNA_enum_items_add_value(
      &item, &totitem, opensubdiv_compute_type_items, USER_OPENSUBDIV_COMPUTE_NONE);

#    define APPEND_COMPUTE(compute) \
      if (evaluators & OPENSUBDIV_EVALUATOR_##compute) { \
        RNA_enum_items_add_value( \
            &item, &totitem, opensubdiv_compute_type_items, USER_OPENSUBDIV_COMPUTE_##compute); \
      } \
      ((void)0)

  APPEND_COMPUTE(CPU);
  APPEND_COMPUTE(OPENMP);
  APPEND_COMPUTE(OPENCL);
  APPEND_COMPUTE(CUDA);
  APPEND_COMPUTE(GLSL_TRANSFORM_FEEDBACK);
  APPEND_COMPUTE(GLSL_COMPUTE);

#    undef APPEND_COMPUTE

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static void rna_userdef_opensubdiv_update(Main *bmain,
                                          Scene *UNUSED(scene),
                                          PointerRNA *UNUSED(ptr))
{
  Object *object;

  for (object = bmain->objects.first; object; object = object->id.next) {
    DEG_id_tag_update(&object->id, ID_RECALC_TRANSFORM);
  }
  USERDEF_TAG_DIRTY;
}

#  endif

static const EnumPropertyItem *rna_userdef_audio_device_itemf(bContext *UNUSED(C),
                                                              PointerRNA *UNUSED(ptr),
                                                              PropertyRNA *UNUSED(prop),
                                                              bool *r_free)
{
  int index = 0;
  int totitem = 0;
  EnumPropertyItem *item = NULL;

  int i;

  char **names = BKE_sound_get_device_names();

  for (i = 0; names[i]; i++) {
    EnumPropertyItem new_item = {i, names[i], 0, names[i], names[i]};
    RNA_enum_item_add(&item, &totitem, &new_item);
  }

#  if !defined(NDEBUG) || !defined(WITH_AUDASPACE)
  if (i == 0) {
    EnumPropertyItem new_item = {i, "SOUND_NONE", 0, "No Sound", ""};
    RNA_enum_item_add(&item, &totitem, &new_item);
  }
#  endif

  /* may be unused */
  UNUSED_VARS(index, audio_device_items);

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

#  ifdef WITH_INTERNATIONAL
static const EnumPropertyItem *rna_lang_enum_properties_itemf(bContext *UNUSED(C),
                                                              PointerRNA *UNUSED(ptr),
                                                              PropertyRNA *UNUSED(prop),
                                                              bool *UNUSED(r_free))
{
  const EnumPropertyItem *items = BLT_lang_RNA_enum_properties();
  if (items == NULL) {
    items = rna_enum_language_default_items;
  }
  return items;
}
#  endif

static IDProperty **rna_AddonPref_idprops(PointerRNA *ptr)
{
  return (IDProperty **)&ptr->data;
}

static PointerRNA rna_Addon_preferences_get(PointerRNA *ptr)
{
  bAddon *addon = (bAddon *)ptr->data;
  bAddonPrefType *apt = BKE_addon_pref_type_find(addon->module, true);
  if (apt) {
    if (addon->prop == NULL) {
      IDPropertyTemplate val = {0};
      addon->prop = IDP_New(IDP_GROUP, &val, addon->module); /* name is unimportant. */
    }
    return rna_pointer_inherit_refine(ptr, apt->rna_ext.srna, addon->prop);
  }
  else {
    return PointerRNA_NULL;
  }
}

static void rna_AddonPref_unregister(Main *UNUSED(bmain), StructRNA *type)
{
  bAddonPrefType *apt = RNA_struct_blender_type_get(type);

  if (!apt) {
    return;
  }

  RNA_struct_free_extension(type, &apt->rna_ext);
  RNA_struct_free(&BLENDER_RNA, type);

  BKE_addon_pref_type_remove(apt);

  /* update while blender is running */
  WM_main_add_notifier(NC_WINDOW, NULL);
}

static StructRNA *rna_AddonPref_register(Main *bmain,
                                         ReportList *reports,
                                         void *data,
                                         const char *identifier,
                                         StructValidateFunc validate,
                                         StructCallbackFunc call,
                                         StructFreeFunc free)
{
  bAddonPrefType *apt, dummy_apt = {{'\0'}};
  bAddon dummy_addon = {NULL};
  PointerRNA dummy_ptr;
  // int have_function[1];

  /* Setup dummy add-on preference and it's type to store static properties in. */
  RNA_pointer_create(NULL, &RNA_AddonPreferences, &dummy_addon, &dummy_ptr);

  /* validate the python class */
  if (validate(&dummy_ptr, data, NULL /* have_function */) != 0) {
    return NULL;
  }

  BLI_strncpy(dummy_apt.idname, dummy_addon.module, sizeof(dummy_apt.idname));
  if (strlen(identifier) >= sizeof(dummy_apt.idname)) {
    BKE_reportf(reports,
                RPT_ERROR,
                "Registering add-on preferences class: '%s' is too long, maximum length is %d",
                identifier,
                (int)sizeof(dummy_apt.idname));
    return NULL;
  }

  /* Check if we have registered this add-on preference type before, and remove it. */
  apt = BKE_addon_pref_type_find(dummy_addon.module, true);
  if (apt && apt->rna_ext.srna) {
    rna_AddonPref_unregister(bmain, apt->rna_ext.srna);
  }

  /* Create a new add-on preference type. */
  apt = MEM_mallocN(sizeof(bAddonPrefType), "addonpreftype");
  memcpy(apt, &dummy_apt, sizeof(dummy_apt));
  BKE_addon_pref_type_add(apt);

  apt->rna_ext.srna = RNA_def_struct_ptr(&BLENDER_RNA, identifier, &RNA_AddonPreferences);
  apt->rna_ext.data = data;
  apt->rna_ext.call = call;
  apt->rna_ext.free = free;
  RNA_struct_blender_type_set(apt->rna_ext.srna, apt);

  //  apt->draw = (have_function[0]) ? header_draw : NULL;

  /* update while blender is running */
  WM_main_add_notifier(NC_WINDOW, NULL);

  return apt->rna_ext.srna;
}

/* placeholder, doesn't do anything useful yet */
static StructRNA *rna_AddonPref_refine(PointerRNA *ptr)
{
  return (ptr->type) ? ptr->type : &RNA_AddonPreferences;
}

static float rna_ThemeUI_roundness_get(PointerRNA *ptr)
{
  /* Remap from relative radius to 0..1 range. */
  uiWidgetColors *tui = (uiWidgetColors *)ptr->data;
  return tui->roundness * 2.0f;
}

static void rna_ThemeUI_roundness_set(PointerRNA *ptr, float value)
{
  uiWidgetColors *tui = (uiWidgetColors *)ptr->data;
  tui->roundness = value * 0.5f;
}

/* Studio Light */
static void rna_UserDef_studiolight_begin(CollectionPropertyIterator *iter,
                                          PointerRNA *UNUSED(ptr))
{
  rna_iterator_listbase_begin(iter, BKE_studiolight_listbase(), NULL);
}

static void rna_StudioLights_refresh(UserDef *UNUSED(userdef))
{
  BKE_studiolight_refresh();
}

static void rna_StudioLights_remove(UserDef *UNUSED(userdef), StudioLight *studio_light)
{
  BKE_studiolight_remove(studio_light);
}

static StudioLight *rna_StudioLights_load(UserDef *UNUSED(userdef), const char *path, int type)
{
  return BKE_studiolight_load(path, type);
}

/* TODO: Make it accept arguments. */
static StudioLight *rna_StudioLights_new(UserDef *userdef, const char *name)
{
  return BKE_studiolight_create(name, userdef->light_param, userdef->light_ambient);
}

/* StudioLight.name */
static void rna_UserDef_studiolight_name_get(PointerRNA *ptr, char *value)
{
  StudioLight *sl = (StudioLight *)ptr->data;
  BLI_strncpy(value, sl->name, FILE_MAXFILE);
}

static int rna_UserDef_studiolight_name_length(PointerRNA *ptr)
{
  StudioLight *sl = (StudioLight *)ptr->data;
  return strlen(sl->name);
}

/* StudioLight.path */
static void rna_UserDef_studiolight_path_get(PointerRNA *ptr, char *value)
{
  StudioLight *sl = (StudioLight *)ptr->data;
  BLI_strncpy(value, sl->path, FILE_MAX);
}

static int rna_UserDef_studiolight_path_length(PointerRNA *ptr)
{
  StudioLight *sl = (StudioLight *)ptr->data;
  return strlen(sl->path);
}

/* StudioLight.path_irr_cache */
static void rna_UserDef_studiolight_path_irr_cache_get(PointerRNA *ptr, char *value)
{
  StudioLight *sl = (StudioLight *)ptr->data;
  if (sl->path_irr_cache) {
    BLI_strncpy(value, sl->path_irr_cache, FILE_MAX);
  }
  else {
    value[0] = '\0';
  }
}

static int rna_UserDef_studiolight_path_irr_cache_length(PointerRNA *ptr)
{
  StudioLight *sl = (StudioLight *)ptr->data;
  if (sl->path_irr_cache) {
    return strlen(sl->path_irr_cache);
  }
  return 0;
}

/* StudioLight.path_sh_cache */
static void rna_UserDef_studiolight_path_sh_cache_get(PointerRNA *ptr, char *value)
{
  StudioLight *sl = (StudioLight *)ptr->data;
  if (sl->path_sh_cache) {
    BLI_strncpy(value, sl->path_sh_cache, FILE_MAX);
  }
  else {
    value[0] = '\0';
  }
}

static int rna_UserDef_studiolight_path_sh_cache_length(PointerRNA *ptr)
{
  StudioLight *sl = (StudioLight *)ptr->data;
  if (sl->path_sh_cache) {
    return strlen(sl->path_sh_cache);
  }
  return 0;
}

/* StudioLight.index */
static int rna_UserDef_studiolight_index_get(PointerRNA *ptr)
{
  StudioLight *sl = (StudioLight *)ptr->data;
  return sl->index;
}

/* StudioLight.is_user_defined */
static bool rna_UserDef_studiolight_is_user_defined_get(PointerRNA *ptr)
{
  StudioLight *sl = (StudioLight *)ptr->data;
  return (sl->flag & STUDIOLIGHT_USER_DEFINED) != 0;
}

/* StudioLight.is_user_defined */
static bool rna_UserDef_studiolight_has_specular_highlight_pass_get(PointerRNA *ptr)
{
  StudioLight *sl = (StudioLight *)ptr->data;
  return sl->flag & STUDIOLIGHT_SPECULAR_HIGHLIGHT_PASS;
}

/* StudioLight.type */

static int rna_UserDef_studiolight_type_get(PointerRNA *ptr)
{
  StudioLight *sl = (StudioLight *)ptr->data;
  return sl->flag & STUDIOLIGHT_FLAG_ORIENTATIONS;
}

static void rna_UserDef_studiolight_spherical_harmonics_coefficients_get(PointerRNA *ptr,
                                                                         float *values)
{
  StudioLight *sl = (StudioLight *)ptr->data;
  float *value = values;
  for (int i = 0; i < STUDIOLIGHT_SH_EFFECTIVE_COEFS_LEN; i++) {
    copy_v3_v3(value, sl->spherical_harmonics_coefs[i]);
    value += 3;
  }
}

/* StudioLight.solid_lights */

static void rna_UserDef_studiolight_solid_lights_begin(CollectionPropertyIterator *iter,
                                                       PointerRNA *ptr)
{
  StudioLight *sl = (StudioLight *)ptr->data;
  rna_iterator_array_begin(iter, sl->light, sizeof(*sl->light), ARRAY_SIZE(sl->light), 0, NULL);
}

static int rna_UserDef_studiolight_solid_lights_length(PointerRNA *UNUSED(ptr))
{
  return ARRAY_SIZE(((StudioLight *)NULL)->light);
}

/* StudioLight.light_ambient */

static void rna_UserDef_studiolight_light_ambient_get(PointerRNA *ptr, float *values)
{
  StudioLight *sl = (StudioLight *)ptr->data;
  copy_v3_v3(values, sl->light_ambient);
}

int rna_show_statusbar_vram_editable(struct PointerRNA *UNUSED(ptr), const char **UNUSED(r_info))
{
  return GPU_mem_stats_supported() ? PROP_EDITABLE : 0;
}

static int rna_userdef_experimental_use_new_curve_tools_editable(struct PointerRNA *UNUSED(ptr),
                                                                 const char **r_info)
{
  if (U.experimental.use_new_curves_type) {
    return PROP_EDITABLE;
  }
  *r_info = "Only available when new curves type is enabled";
  return 0;
}

#else

#  define USERDEF_TAG_DIRTY_PROPERTY_UPDATE_ENABLE \
    RNA_define_fallback_property_update(0, "rna_userdef_is_dirty_update")

#  define USERDEF_TAG_DIRTY_PROPERTY_UPDATE_DISABLE RNA_define_fallback_property_update(0, NULL)

/* TODO(sergey): This technically belongs to blenlib, but we don't link
 * makesrna against it.
 */

/* Get maximum addressable memory in megabytes, */
static size_t max_memory_in_megabytes(void)
{
  /* Maximum addressable bytes on this platform. */
  const size_t limit_bytes = (((size_t)1) << (sizeof(size_t[8]) - 1));
  /* Convert it to megabytes and return. */
  return (limit_bytes >> 20);
}

/* Same as above, but clipped to int capacity. */
static int max_memory_in_megabytes_int(void)
{
  const size_t limit_megabytes = max_memory_in_megabytes();
  /* NOTE: The result will fit into integer. */
  return (int)min_zz(limit_megabytes, (size_t)INT_MAX);
}

static void rna_def_userdef_theme_ui_font_style(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ThemeFontStyle", NULL);
  RNA_def_struct_sdna(srna, "uiFontStyle");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Font Style", "Theme settings for Font");

  prop = RNA_def_property(srna, "points", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_range(prop, 6.0f, 32.0f);
  RNA_def_property_ui_range(prop, 8.0f, 20.0f, 10.0f, 1);
  RNA_def_property_ui_text(prop, "Points", "Font size in points");
  RNA_def_property_update(prop, 0, "rna_userdef_dpi_update");

  prop = RNA_def_property(srna, "shadow", PROP_INT, PROP_PIXEL);
  RNA_def_property_range(prop, 0, 5);
  RNA_def_property_ui_text(prop, "Shadow Size", "Shadow size (0, 3 and 5 supported)");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_text_style_update");

  prop = RNA_def_property(srna, "shadow_offset_x", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, NULL, "shadx");
  RNA_def_property_range(prop, -10, 10);
  RNA_def_property_ui_text(prop, "Shadow X Offset", "Shadow offset in pixels");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_text_style_update");

  prop = RNA_def_property(srna, "shadow_offset_y", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, NULL, "shady");
  RNA_def_property_range(prop, -10, 10);
  RNA_def_property_ui_text(prop, "Shadow Y Offset", "Shadow offset in pixels");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_text_style_update");

  prop = RNA_def_property(srna, "shadow_alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "shadowalpha");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Shadow Alpha", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_text_style_update");

  prop = RNA_def_property(srna, "shadow_value", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "shadowcolor");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Shadow Brightness", "Shadow color in gray value");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_text_style_update");
}

static void rna_def_userdef_theme_ui_style(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  rna_def_userdef_theme_ui_font_style(brna);

  srna = RNA_def_struct(brna, "ThemeStyle", NULL);
  RNA_def_struct_sdna(srna, "uiStyle");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Style", "Theme settings for style sets");

  prop = RNA_def_property(srna, "panel_title", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "paneltitle");
  RNA_def_property_struct_type(prop, "ThemeFontStyle");
  RNA_def_property_ui_text(prop, "Panel Title Font", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "widget_label", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "widgetlabel");
  RNA_def_property_struct_type(prop, "ThemeFontStyle");
  RNA_def_property_ui_text(prop, "Widget Label Style", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "widget", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "widget");
  RNA_def_property_struct_type(prop, "ThemeFontStyle");
  RNA_def_property_ui_text(prop, "Widget Style", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
}

static void rna_def_userdef_theme_ui_wcol(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ThemeWidgetColors", NULL);
  RNA_def_struct_sdna(srna, "uiWidgetColors");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Theme Widget Color Set", "Theme settings for widget color sets");

  prop = RNA_def_property(srna, "outline", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Outline", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "inner", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Inner", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "inner_sel", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Inner Selected", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "item", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Item", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "text", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Text", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "text_sel", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Text Selected", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "show_shaded", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "shaded", 1);
  RNA_def_property_ui_text(prop, "Shaded", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "shadetop", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, -100, 100);
  RNA_def_property_ui_text(prop, "Shade Top", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "shadedown", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, -100, 100);
  RNA_def_property_ui_text(prop, "Shade Down", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "roundness", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_funcs(
      prop, "rna_ThemeUI_roundness_get", "rna_ThemeUI_roundness_set", NULL);
  RNA_def_property_ui_text(prop, "Roundness", "Amount of edge rounding");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
}

static void rna_def_userdef_theme_ui_wcol_state(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ThemeWidgetStateColors", NULL);
  RNA_def_struct_sdna(srna, "uiWidgetStateColors");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(
      srna, "Theme Widget State Color", "Theme settings for widget state colors");

  prop = RNA_def_property(srna, "inner_anim", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Animated", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "inner_anim_sel", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Animated Selected", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "inner_key", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Keyframe", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "inner_key_sel", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Keyframe Selected", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "inner_driven", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Driven", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "inner_driven_sel", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Driven Selected", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "inner_overridden", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Overridden", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "inner_overridden_sel", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Overridden Selected", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "inner_changed", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Changed", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "inner_changed_sel", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Changed Selected", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "blend", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(prop, "Blend", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
}

static void rna_def_userdef_theme_ui_panel(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ThemePanelColors", NULL);
  RNA_def_struct_sdna(srna, "uiPanelColors");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Theme Panel Color", "Theme settings for panel colors");

  prop = RNA_def_property(srna, "header", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_ui_text(prop, "Header", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "back", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_ui_text(prop, "Background", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "sub_back", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_ui_text(prop, "Sub Background", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
}

static const EnumPropertyItem rna_enum_userdef_theme_background_types_items[] = {
    {TH_BACKGROUND_SINGLE_COLOR,
     "SINGLE_COLOR",
     0,
     "Single Color",
     "Use a solid color as viewport background"},
    {TH_BACKGROUND_GRADIENT_LINEAR,
     "LINEAR",
     0,
     "Linear Gradient",
     "Use a screen space vertical linear gradient as viewport background"},
    {TH_BACKGROUND_GRADIENT_RADIAL,
     "RADIAL",
     0,
     "Vignette",
     "Use a radial gradient as viewport background"},
    {0, NULL, 0, NULL, NULL},
};

static void rna_def_userdef_theme_ui_gradient(BlenderRNA *brna)
{
  /* Fake struct, keep this for compatible theme presets. */
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ThemeGradientColors", NULL);
  RNA_def_struct_sdna(srna, "ThemeSpace");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(
      srna, "Theme Background Color", "Theme settings for background colors and gradient");

  prop = RNA_def_property(srna, "background_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "background_type");
  RNA_def_property_enum_items(prop, rna_enum_userdef_theme_background_types_items);
  RNA_def_property_ui_text(prop, "Background Type", "Type of background in the 3D viewport");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "high_gradient", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "back");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Gradient High/Off", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "gradient", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "back_grad");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Gradient Low", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
}

static void rna_def_userdef_theme_ui(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  rna_def_userdef_theme_ui_wcol(brna);
  rna_def_userdef_theme_ui_wcol_state(brna);
  rna_def_userdef_theme_ui_panel(brna);
  rna_def_userdef_theme_ui_gradient(brna);

  srna = RNA_def_struct(brna, "ThemeUserInterface", NULL);
  RNA_def_struct_sdna(srna, "ThemeUI");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(
      srna, "Theme User Interface", "Theme settings for user interface elements");

  prop = RNA_def_property(srna, "wcol_regular", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Regular Widget Colors", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wcol_tool", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Tool Widget Colors", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wcol_toolbar_item", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Toolbar Item Widget Colors", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wcol_radio", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Radio Widget Colors", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wcol_text", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Text Widget Colors", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wcol_option", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Option Widget Colors", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wcol_toggle", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Toggle Widget Colors", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wcol_num", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Number Widget Colors", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wcol_numslider", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Slider Widget Colors", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wcol_box", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Box Backdrop Colors", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wcol_menu", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Menu Widget Colors", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wcol_pulldown", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Pulldown Widget Colors", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wcol_menu_back", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Menu Backdrop Colors", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wcol_pie_menu", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Pie Menu Colors", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wcol_tooltip", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Tooltip Colors", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wcol_menu_item", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Menu Item Colors", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wcol_scroll", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Scroll Widget Colors", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wcol_progress", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Progress Bar Widget Colors", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wcol_list_item", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "List Item Colors", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wcol_view_item", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Data-View Item Colors", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wcol_state", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "State Colors", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wcol_tab", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Tab Colors", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "menu_shadow_fac", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(prop, "Menu Shadow Strength", "Blending factor for menu shadows");
  RNA_def_property_range(prop, 0.01f, 1.0f);
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "menu_shadow_width", PROP_INT, PROP_PIXEL);
  RNA_def_property_ui_text(
      prop, "Menu Shadow Width", "Width of menu shadows, set to zero to disable");
  RNA_def_property_range(prop, 0.0f, 24.0f);
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "icon_alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(
      prop, "Icon Alpha", "Transparency of icons in the interface, to reduce contrast");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "icon_saturation", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(prop, "Icon Saturation", "Saturation of icons in the interface");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "widget_emboss", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "widget_emboss");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(
      prop, "Widget Emboss", "Color of the 1px shadow line underlying widgets");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "editor_outline", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "editor_outline");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(
      prop, "Editor Outline", "Color of the outline of the editors and their round corners");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "widget_text_cursor", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "widget_text_cursor");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(
      prop, "Text Cursor", "Color of the interface widgets text insertion cursor (caret)");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "panel_roundness", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_ui_text(
      prop, "Panel Roundness", "Roundness of the corners of panels and sub-panels");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_default(prop, 0.4f);
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  /* Transparent Grid */
  prop = RNA_def_property(srna, "transparent_checker_primary", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "transparent_checker_primary");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(
      prop, "Primary Color", "Primary color of checkerboard pattern indicating transparent areas");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "transparent_checker_secondary", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "transparent_checker_secondary");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop,
                           "Secondary Color",
                           "Secondary color of checkerboard pattern indicating transparent areas");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "transparent_checker_size", PROP_INT, PROP_PIXEL);
  RNA_def_property_ui_text(
      prop, "Checkerboard Size", "Size of checkerboard pattern indicating transparent areas");
  RNA_def_property_range(prop, 2, 48);
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  /* axis */
  prop = RNA_def_property(srna, "axis_x", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "xaxis");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "X Axis", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "axis_y", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "yaxis");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Y Axis", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "axis_z", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "zaxis");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Z Axis", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  /* Generic gizmo colors. */
  prop = RNA_def_property(srna, "gizmo_hi", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "gizmo_hi");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Gizmo Highlight", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "gizmo_primary", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "gizmo_primary");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Gizmo Primary", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "gizmo_secondary", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "gizmo_secondary");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Gizmo Secondary", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "gizmo_view_align", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "gizmo_view_align");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Gizmo View Align", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "gizmo_a", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "gizmo_a");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Gizmo A", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "gizmo_b", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "gizmo_b");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Gizmo B", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  /* Icon colors. */
  prop = RNA_def_property(srna, "icon_scene", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "icon_scene");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Scene", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "icon_collection", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "icon_collection");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Collection", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "icon_object", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "icon_object");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Object", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "icon_object_data", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "icon_object_data");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Object Data", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "icon_modifier", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "icon_modifier");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Modifier", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "icon_shading", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "icon_shading");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Shading", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "icon_folder", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "icon_folder");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "File Folders", "Color of folders in the file browser");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "icon_border_intensity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "icon_border_intensity");
  RNA_def_property_ui_text(
      prop, "Icon Border", "Control the intensity of the border around themes icons");
  RNA_def_property_ui_range(prop, 0.0, 1.0, 0.1, 2);
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update_icons");
}

static void rna_def_userdef_theme_space_common(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "title", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Title", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "text", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Text", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "text_hi", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Text Highlight", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  /* header */
  prop = RNA_def_property(srna, "header", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Header", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "header_text", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Header Text", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "header_text_hi", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Header Text Highlight", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  /* panel settings */
  prop = RNA_def_property(srna, "panelcolors", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_ui_text(prop, "Panel Colors", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  /* buttons */
  /*  if (! ELEM(spacetype, SPACE_PROPERTIES, SPACE_OUTLINER)) { */
  prop = RNA_def_property(srna, "button", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Region Background", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "button_title", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Region Text Titles", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "button_text", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Region Text", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "button_text_hi", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Region Text Highlight", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "navigation_bar", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Navigation Bar Background", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "execution_buts", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Execution Region Background", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  /* tabs */
  prop = RNA_def_property(srna, "tab_active", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Tab Active", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "tab_inactive", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Tab Inactive", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "tab_back", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Tab Background", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "tab_outline", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Tab Outline", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  /*  } */
}

static void rna_def_userdef_theme_space_gradient(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ThemeSpaceGradient", NULL);
  RNA_def_struct_sdna(srna, "ThemeSpace");
  RNA_def_struct_ui_text(srna, "Theme Space Settings", "");

  /* gradient/background settings */
  prop = RNA_def_property(srna, "gradients", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "ThemeGradientColors");
  RNA_def_property_pointer_funcs(prop, "rna_Theme_gradient_colors_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "Gradient Colors", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  rna_def_userdef_theme_space_common(srna);
}

static void rna_def_userdef_theme_space_generic(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ThemeSpaceGeneric", NULL);
  RNA_def_struct_sdna(srna, "ThemeSpace");
  RNA_def_struct_ui_text(srna, "Theme Space Settings", "");

  prop = RNA_def_property(srna, "back", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Window Background", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  rna_def_userdef_theme_space_common(srna);
}

/* list / channels */
static void rna_def_userdef_theme_space_list_generic(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ThemeSpaceListGeneric", NULL);
  RNA_def_struct_sdna(srna, "ThemeSpace");
  RNA_def_struct_ui_text(srna, "Theme Space List Settings", "");

  prop = RNA_def_property(srna, "list", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Source List", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "list_title", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Source List Title", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "list_text", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Source List Text", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "list_text_hi", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Source List Text Highlight", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
}

static void rna_def_userdef_theme_spaces_main(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "space", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "ThemeSpaceGeneric");
  RNA_def_property_pointer_funcs(prop, "rna_Theme_space_generic_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "Theme Space", "Settings for space");
}

static void rna_def_userdef_theme_spaces_gradient(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "space", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "ThemeSpaceGradient");
  RNA_def_property_pointer_funcs(prop, "rna_Theme_space_gradient_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "Theme Space", "Settings for space");
}

static void rna_def_userdef_theme_spaces_list_main(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "space_list", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "ThemeSpaceListGeneric");
  RNA_def_property_pointer_funcs(prop, "rna_Theme_space_list_generic_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "Theme Space List", "Settings for space list");
}

static void rna_def_userdef_theme_spaces_vertex(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "vertex", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Vertex", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "vertex_select", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Vertex Select", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "vertex_active", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Active Vertex", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "vertex_size", PROP_INT, PROP_PIXEL);
  RNA_def_property_range(prop, 1, 32);
  RNA_def_property_ui_text(prop, "Vertex Size", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "vertex_bevel", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Vertex Bevel", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "vertex_unreferenced", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Vertex Group Unreferenced", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
}

static void rna_def_userdef_theme_spaces_edge(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "edge_select", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Edge Select", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "edge_seam", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Edge Seam", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "edge_sharp", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Edge Sharp", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "edge_crease", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Edge Crease", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "edge_bevel", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Edge Bevel", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "edge_facesel", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Edge UV Face Select", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "freestyle_edge_mark", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Freestyle Edge Mark", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
}

static void rna_def_userdef_theme_spaces_face(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "face", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Face", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "face_select", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Face Selected", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "face_dot", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Face Dot Selected", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "facedot_size", PROP_INT, PROP_PIXEL);
  RNA_def_property_range(prop, 1, 10);
  RNA_def_property_ui_text(prop, "Face Dot Size", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "freestyle_face_mark", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Freestyle Face Mark", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "face_back", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Face Orientation Back", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "face_front", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Face Orientation Front", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
}

static void rna_def_userdef_theme_spaces_paint_curves(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "paint_curve_handle", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Paint Curve Handle", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "paint_curve_pivot", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Paint Curve Pivot", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
}

static void rna_def_userdef_theme_spaces_curves(
    StructRNA *srna, bool incl_nurbs, bool incl_lastsel, bool incl_vector, bool incl_verthandle)
{
  PropertyRNA *prop;

  if (incl_nurbs) {
    prop = RNA_def_property(srna, "nurb_uline", PROP_FLOAT, PROP_COLOR_GAMMA);
    RNA_def_property_float_sdna(prop, NULL, "nurb_uline");
    RNA_def_property_array(prop, 3);
    RNA_def_property_ui_text(prop, "NURBS U Lines", "");
    RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

    prop = RNA_def_property(srna, "nurb_vline", PROP_FLOAT, PROP_COLOR_GAMMA);
    RNA_def_property_float_sdna(prop, NULL, "nurb_vline");
    RNA_def_property_array(prop, 3);
    RNA_def_property_ui_text(prop, "NURBS V Lines", "");
    RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

    prop = RNA_def_property(srna, "nurb_sel_uline", PROP_FLOAT, PROP_COLOR_GAMMA);
    RNA_def_property_float_sdna(prop, NULL, "nurb_sel_uline");
    RNA_def_property_array(prop, 3);
    RNA_def_property_ui_text(prop, "NURBS Active U Lines", "");
    RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

    prop = RNA_def_property(srna, "nurb_sel_vline", PROP_FLOAT, PROP_COLOR_GAMMA);
    RNA_def_property_float_sdna(prop, NULL, "nurb_sel_vline");
    RNA_def_property_array(prop, 3);
    RNA_def_property_ui_text(prop, "NURBS Active V Lines", "");
    RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

    prop = RNA_def_property(srna, "act_spline", PROP_FLOAT, PROP_COLOR_GAMMA);
    RNA_def_property_float_sdna(prop, NULL, "act_spline");
    RNA_def_property_array(prop, 3);
    RNA_def_property_ui_text(prop, "Active Spline", "");
    RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
  }

  prop = RNA_def_property(srna, "handle_free", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "handle_free");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Free Handle", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "handle_auto", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "handle_auto");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Auto Handle", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  if (incl_vector) {
    prop = RNA_def_property(srna, "handle_vect", PROP_FLOAT, PROP_COLOR_GAMMA);
    RNA_def_property_float_sdna(prop, NULL, "handle_vect");
    RNA_def_property_array(prop, 3);
    RNA_def_property_ui_text(prop, "Vector Handle", "");
    RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

    prop = RNA_def_property(srna, "handle_sel_vect", PROP_FLOAT, PROP_COLOR_GAMMA);
    RNA_def_property_float_sdna(prop, NULL, "handle_sel_vect");
    RNA_def_property_array(prop, 3);
    RNA_def_property_ui_text(prop, "Vector Handle Selected", "");
    RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
  }

  prop = RNA_def_property(srna, "handle_align", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "handle_align");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Align Handle", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "handle_sel_free", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "handle_sel_free");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Free Handle Selected", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "handle_sel_auto", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "handle_sel_auto");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Auto Handle Selected", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "handle_sel_align", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "handle_sel_align");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Align Handle Selected", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  if (!incl_nurbs) {
    /* assume that when nurbs are off, this is for 2D (i.e. anim) editors */
    prop = RNA_def_property(srna, "handle_auto_clamped", PROP_FLOAT, PROP_COLOR_GAMMA);
    RNA_def_property_float_sdna(prop, NULL, "handle_auto_clamped");
    RNA_def_property_array(prop, 3);
    RNA_def_property_ui_text(prop, "Auto-Clamped Handle", "");
    RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

    prop = RNA_def_property(srna, "handle_sel_auto_clamped", PROP_FLOAT, PROP_COLOR_GAMMA);
    RNA_def_property_float_sdna(prop, NULL, "handle_sel_auto_clamped");
    RNA_def_property_array(prop, 3);
    RNA_def_property_ui_text(prop, "Auto-Clamped Handle Selected", "");
    RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
  }

  if (incl_lastsel) {
    prop = RNA_def_property(srna, "lastsel_point", PROP_FLOAT, PROP_COLOR_GAMMA);
    RNA_def_property_float_sdna(prop, NULL, "lastsel_point");
    RNA_def_property_array(prop, 3);
    RNA_def_property_ui_text(prop, "Last Selected Point", "");
    RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
  }

  if (incl_verthandle) {
    prop = RNA_def_property(srna, "handle_vertex", PROP_FLOAT, PROP_COLOR_GAMMA);
    RNA_def_property_array(prop, 3);
    RNA_def_property_ui_text(prop, "Handle Vertex", "");
    RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

    prop = RNA_def_property(srna, "handle_vertex_select", PROP_FLOAT, PROP_COLOR_GAMMA);
    RNA_def_property_array(prop, 3);
    RNA_def_property_ui_text(prop, "Handle Vertex Select", "");
    RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

    prop = RNA_def_property(srna, "handle_vertex_size", PROP_INT, PROP_PIXEL);
    RNA_def_property_range(prop, 1, 100);
    RNA_def_property_ui_text(prop, "Handle Vertex Size", "");
    RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
  }
}

static void rna_def_userdef_theme_spaces_gpencil(StructRNA *srna)
{
  PropertyRNA *prop;

  prop = RNA_def_property(srna, "gp_vertex", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Grease Pencil Vertex", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "gp_vertex_select", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Grease Pencil Vertex Select", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "gp_vertex_size", PROP_INT, PROP_PIXEL);
  RNA_def_property_range(prop, 1, 10);
  RNA_def_property_ui_text(prop, "Grease Pencil Vertex Size", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
}

static void rna_def_userdef_theme_space_view3d(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* space_view3d */

  srna = RNA_def_struct(brna, "ThemeView3D", NULL);
  RNA_def_struct_sdna(srna, "ThemeSpace");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Theme 3D Viewport", "Theme settings for the 3D viewport");

  rna_def_userdef_theme_spaces_gradient(srna);

  /* General Viewport options */

  prop = RNA_def_property(srna, "grid", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Grid", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "clipping_border_3d", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Clipping Border", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wire", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Wire", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wire_edit", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(
      prop, "Wire Edit", "Color for wireframe when in edit mode, but edge selection is active");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  /* Grease Pencil */

  rna_def_userdef_theme_spaces_gpencil(srna);

  prop = RNA_def_property(srna, "text_grease_pencil", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "time_gp_keyframe");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(
      prop, "Grease Pencil Keyframe", "Color for indicating Grease Pencil keyframes");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  /* Object specific options */

  prop = RNA_def_property(srna, "object_selected", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "select");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Object Selected", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "object_active", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "active");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Active Object", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "text_keyframe", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "time_keyframe");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Object Keyframe", "Color for indicating object keyframes");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  /* Object type options */

  prop = RNA_def_property(srna, "camera", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Camera", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "empty", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Empty", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "light", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "lamp");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Light", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_LIGHT);
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "speaker", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Speaker", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  /* Mesh Object specific */

  rna_def_userdef_theme_spaces_vertex(srna);
  rna_def_userdef_theme_spaces_edge(srna);
  rna_def_userdef_theme_spaces_face(srna);

  /* Mesh Object specific curves. */

  rna_def_userdef_theme_spaces_curves(srna, true, true, true, false);

  prop = RNA_def_property(srna, "extra_edge_len", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Edge Length Text", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "extra_edge_angle", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Edge Angle Text", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "extra_face_angle", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Face Angle Text", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "extra_face_area", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Face Area Text", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "editmesh_active", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Active Vertex/Edge/Face", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "normal", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Face Normal", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "vertex_normal", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Vertex Normal", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "split_normal", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "loop_normal");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Split Normal", "");
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  /* Armature Object specific. */

  prop = RNA_def_property(srna, "bone_pose", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Bone Pose", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "bone_pose_active", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Bone Pose Active", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "bone_solid", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Bone Solid", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "bone_locked_weight", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(
      prop,
      "Bone Locked Weight",
      "Shade for bones corresponding to a locked weight group during painting");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  /* misc */

  prop = RNA_def_property(srna, "bundle_solid", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "bundle_solid");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Bundle Solid", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "camera_path", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "camera_path");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Camera Path", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "skin_root", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Skin Root", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "view_overlay", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "View Overlay", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "transform", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Transform", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "frame_current", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "cframe");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Current Frame", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  rna_def_userdef_theme_spaces_paint_curves(srna);

  prop = RNA_def_property(srna, "outline_width", PROP_INT, PROP_PIXEL);
  RNA_def_property_range(prop, 1, 5);
  RNA_def_property_ui_text(prop, "Outline Width", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "object_origin_size", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, NULL, "obcenter_dia");
  RNA_def_property_range(prop, 4, 10);
  RNA_def_property_ui_text(
      prop, "Object Origin Size", "Diameter in pixels for object/light origin display");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
}

static void rna_def_userdef_theme_space_graph(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* space_graph */
  srna = RNA_def_struct(brna, "ThemeGraphEditor", NULL);
  RNA_def_struct_sdna(srna, "ThemeSpace");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Theme Graph Editor", "Theme settings for the graph editor");

  rna_def_userdef_theme_spaces_main(srna);
  rna_def_userdef_theme_spaces_list_main(srna);

  prop = RNA_def_property(srna, "grid", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Grid", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "frame_current", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "cframe");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Current Frame", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "time_scrub_background", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Scrubbing/Markers Region", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "time_marker_line", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Marker Line", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "time_marker_line_selected", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Marker Line Selected", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "window_sliders", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "shade1");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Window Sliders", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "channels_region", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "shade2");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Channels Region", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "dopesheet_channel", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "ds_channel");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Dope Sheet Channel", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "dopesheet_subchannel", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "ds_subchannel");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Dope Sheet Sub-channel", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "channel_group", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "group");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Channel Group", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "active_channels_group", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "group_active");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Active Channel Group", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "preview_range", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "anim_preview_range");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Preview Range", "Color of preview range overlay");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  rna_def_userdef_theme_spaces_vertex(srna);
  rna_def_userdef_theme_spaces_curves(srna, false, true, true, true);
}

static void rna_def_userdef_theme_space_file(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* space_file */

  srna = RNA_def_struct(brna, "ThemeFileBrowser", NULL);
  RNA_def_struct_sdna(srna, "ThemeSpace");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Theme File Browser", "Theme settings for the File Browser");

  rna_def_userdef_theme_spaces_main(srna);

  prop = RNA_def_property(srna, "selected_file", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "hilite");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Selected File", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "row_alternate", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Alternate Rows", "Overlay color on every other row");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
}

static void rna_def_userdef_theme_space_outliner(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* space_outliner */

  srna = RNA_def_struct(brna, "ThemeOutliner", NULL);
  RNA_def_struct_sdna(srna, "ThemeSpace");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Theme Outliner", "Theme settings for the Outliner");

  rna_def_userdef_theme_spaces_main(srna);

  prop = RNA_def_property(srna, "match", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Filter Match", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "selected_highlight", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Selected Highlight", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "active", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Active Highlight", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "selected_object", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Selected Objects", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "active_object", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Active Object", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "edited_object", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Edited Object", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "row_alternate", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Alternate Rows", "Overlay color on every other row");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
}

static void rna_def_userdef_theme_space_userpref(BlenderRNA *brna)
{
  StructRNA *srna;

  /* space_userpref */

  srna = RNA_def_struct(brna, "ThemePreferences", NULL);
  RNA_def_struct_sdna(srna, "ThemeSpace");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Theme Preferences", "Theme settings for the Blender Preferences");

  rna_def_userdef_theme_spaces_main(srna);
}

static void rna_def_userdef_theme_space_console(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* space_console */

  srna = RNA_def_struct(brna, "ThemeConsole", NULL);
  RNA_def_struct_sdna(srna, "ThemeSpace");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Theme Console", "Theme settings for the Console");

  rna_def_userdef_theme_spaces_main(srna);

  prop = RNA_def_property(srna, "line_output", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "console_output");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Line Output", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "line_input", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "console_input");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Line Input", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "line_info", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "console_info");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Line Info", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "line_error", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "console_error");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Line Error", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "cursor", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "console_cursor");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Cursor", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "select", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "console_select");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Selection", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
}

static void rna_def_userdef_theme_space_info(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* space_info */

  srna = RNA_def_struct(brna, "ThemeInfo", NULL);
  RNA_def_struct_sdna(srna, "ThemeSpace");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Theme Info", "Theme settings for Info");

  rna_def_userdef_theme_spaces_main(srna);

  prop = RNA_def_property(srna, "info_selected", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Selected Line Background", "Background color of selected line");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "info_selected_text", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Selected Line Text Color", "Text color of selected line");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "info_error", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Error Icon Background", "Background color of Error icon");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "info_error_text", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Error Icon Foreground", "Foreground color of Error icon");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "info_warning", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Warning Icon Background", "Background color of Warning icon");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "info_warning_text", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Warning Icon Foreground", "Foreground color of Warning icon");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "info_info", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Info Icon Background", "Background color of Info icon");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "info_info_text", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Info Icon Foreground", "Foreground color of Info icon");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "info_debug", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Debug Icon Background", "Background color of Debug icon");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "info_debug_text", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Debug Icon Foreground", "Foreground color of Debug icon");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "info_property", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Property Icon Background", "Background color of Property icon");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "info_property_text", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Property Icon Foreground", "Foreground color of Property icon");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "info_operator", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Operator Icon Background", "Background color of Operator icon");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "info_operator_text", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Operator Icon Foreground", "Foreground color of Operator icon");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
}

static void rna_def_userdef_theme_space_text(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* space_text */

  srna = RNA_def_struct(brna, "ThemeTextEditor", NULL);
  RNA_def_struct_sdna(srna, "ThemeSpace");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Theme Text Editor", "Theme settings for the Text Editor");

  rna_def_userdef_theme_spaces_main(srna);

  prop = RNA_def_property(srna, "line_numbers", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "line_numbers");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Line Numbers", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "line_numbers_background", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "grid");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Line Numbers Background", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  /* no longer used */
#  if 0
  prop = RNA_def_property(srna, "scroll_bar", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "shade1");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Scroll Bar", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
#  endif

  prop = RNA_def_property(srna, "selected_text", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "shade2");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Selected Text", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "cursor", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "hilite");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Cursor", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "syntax_builtin", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "syntaxb");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Syntax Built-In", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "syntax_symbols", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "syntaxs");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Syntax Symbols", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "syntax_special", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "syntaxv");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Syntax Special", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "syntax_preprocessor", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "syntaxd");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Syntax Preprocessor", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "syntax_reserved", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "syntaxr");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Syntax Reserved", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "syntax_comment", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "syntaxc");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Syntax Comment", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "syntax_string", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "syntaxl");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Syntax String", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "syntax_numbers", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "syntaxn");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Syntax Numbers", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
}

static void rna_def_userdef_theme_space_node(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* space_node */

  srna = RNA_def_struct(brna, "ThemeNodeEditor", NULL);
  RNA_def_struct_sdna(srna, "ThemeSpace");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Theme Node Editor", "Theme settings for the Node Editor");

  rna_def_userdef_theme_spaces_main(srna);
  rna_def_userdef_theme_spaces_list_main(srna);

  prop = RNA_def_property(srna, "grid", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Grid", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "node_selected", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "select");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Node Selected", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "node_active", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "active");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Active Node", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wire", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "wire");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Wires", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wire_inner", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "syntaxr");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Wire Color", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wire_select", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "edge_select");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Wire Select", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "selected_text", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "shade2");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Selected Text", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "node_backdrop", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "syntaxl");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Node Backdrop", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "converter_node", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "syntaxv");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Converter Node", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "color_node", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "syntaxb");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Color Node", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "group_node", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "syntaxc");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Group Node", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "group_socket_node", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "console_output");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Group Socket Node", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "frame_node", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "movie");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Frame Node", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "matte_node", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "syntaxs");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Matte Node", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "distor_node", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "syntaxd");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Distort Node", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "noodle_curving", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "noodle_curving");
  RNA_def_property_int_default(prop, 5);
  RNA_def_property_range(prop, 0, 10);
  RNA_def_property_ui_text(prop, "Noodle Curving", "Curving of the noodle");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "grid_levels", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "grid_levels");
  RNA_def_property_int_default(prop, 3);
  RNA_def_property_range(prop, 0, 3);
  RNA_def_property_ui_text(
      prop, "Grid Levels", "Number of subdivisions for the dot grid displayed in the background");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "dash_alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_ui_text(prop, "Dashed Lines Opacity", "Opacity for the dashed lines in wires");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "input_node", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "syntaxn");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Input Node", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "output_node", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "nodeclass_output");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Output Node", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "filter_node", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "nodeclass_filter");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Filter Node", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "vector_node", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "nodeclass_vector");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Vector Node", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "texture_node", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "nodeclass_texture");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Texture Node", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "shader_node", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "nodeclass_shader");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Shader Node", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "script_node", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "nodeclass_script");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Script Node", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "pattern_node", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "nodeclass_pattern");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Pattern Node", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "layout_node", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "nodeclass_layout");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Layout Node", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "geometry_node", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "nodeclass_geometry");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Geometry Node", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "attribute_node", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "nodeclass_attribute");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Attribute Node", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
}

static void rna_def_userdef_theme_space_buts(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* space_buts */

  srna = RNA_def_struct(brna, "ThemeProperties", NULL);
  RNA_def_struct_sdna(srna, "ThemeSpace");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Theme Properties", "Theme settings for the Properties");

  prop = RNA_def_property(srna, "match", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Search Match", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "active_modifier", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "active");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Active Modifier Outline", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  rna_def_userdef_theme_spaces_main(srna);
}

static void rna_def_userdef_theme_space_image(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* space_image */

  srna = RNA_def_struct(brna, "ThemeImageEditor", NULL);
  RNA_def_struct_sdna(srna, "ThemeSpace");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Theme Image Editor", "Theme settings for the Image Editor");

  rna_def_userdef_theme_spaces_main(srna);

  prop = RNA_def_property(srna, "grid", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Grid", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  rna_def_userdef_theme_spaces_vertex(srna);
  rna_def_userdef_theme_spaces_face(srna);

  prop = RNA_def_property(srna, "editmesh_active", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Active Vertex/Edge/Face", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "wire_edit", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Wire Edit", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "edge_select", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Edge Select", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "scope_back", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "preview_back");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Scope Region Background", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "preview_stitch_face", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "preview_stitch_face");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Stitch Preview Face", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "preview_stitch_edge", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "preview_stitch_edge");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Stitch Preview Edge", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "preview_stitch_vert", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "preview_stitch_vert");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Stitch Preview Vertex", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "preview_stitch_stitchable", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "preview_stitch_stitchable");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Stitch Preview Stitchable", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "preview_stitch_unstitchable", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "preview_stitch_unstitchable");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Stitch Preview Unstitchable", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "preview_stitch_active", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "preview_stitch_active");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Stitch Preview Active Island", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "uv_shadow", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "uv_shadow");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Texture Paint/Modifier UVs", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "frame_current", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "cframe");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Current Frame", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "metadatabg", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "metadatabg");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Metadata Background", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "metadatatext", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "metadatatext");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Metadata Text", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  rna_def_userdef_theme_spaces_curves(srna, false, false, false, true);

  rna_def_userdef_theme_spaces_paint_curves(srna);
}

static void rna_def_userdef_theme_space_seq(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* space_seq */

  srna = RNA_def_struct(brna, "ThemeSequenceEditor", NULL);
  RNA_def_struct_sdna(srna, "ThemeSpace");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Theme Sequence Editor", "Theme settings for the Sequence Editor");

  rna_def_userdef_theme_spaces_main(srna);
  rna_def_userdef_theme_spaces_list_main(srna);

  prop = RNA_def_property(srna, "grid", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Grid", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "window_sliders", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "shade1");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Window Sliders", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "movie_strip", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "movie");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Movie Strip", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "movieclip_strip", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "movieclip");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Clip Strip", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "image_strip", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "image");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Image Strip", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "scene_strip", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "scene");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Scene Strip", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "audio_strip", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "audio");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Audio Strip", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "effect_strip", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "effect");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Effect Strip", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "color_strip", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Color Strip", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "meta_strip", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "meta");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Meta Strip", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "mask_strip", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "mask");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Mask Strip", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "text_strip", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Text Strip", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "active_strip", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Active Strip", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "selected_strip", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Selected Strips", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "frame_current", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "cframe");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Current Frame", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "time_scrub_background", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Scrubbing/Markers Region", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "time_marker_line", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Marker Line", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "time_marker_line_selected", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Marker Line Selected", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "keyframe", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "vertex_select");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Keyframe", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "draw_action", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "bone_pose");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Draw Action", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "preview_back", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "preview_back");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Preview Background", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "metadatabg", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "metadatabg");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Metadata Background", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "metadatatext", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "metadatatext");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Metadata Text", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "preview_range", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "anim_preview_range");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Preview Range", "Color of preview range overlay");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "row_alternate", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Alternate Rows", "Overlay color on every other row");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
}

static void rna_def_userdef_theme_space_action(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* space_action */

  srna = RNA_def_struct(brna, "ThemeDopeSheet", NULL);
  RNA_def_struct_sdna(srna, "ThemeSpace");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Theme Dope Sheet", "Theme settings for the Dope Sheet");

  rna_def_userdef_theme_spaces_main(srna);
  rna_def_userdef_theme_spaces_list_main(srna);

  prop = RNA_def_property(srna, "grid", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Grid", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "frame_current", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "cframe");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Current Frame", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "time_scrub_background", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Scrubbing/Markers Region", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "time_marker_line", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Marker Line", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "time_marker_line_selected", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Marker Line Selected", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "value_sliders", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "face");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Value Sliders", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "view_sliders", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "shade1");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "View Sliders", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "dopesheet_channel", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "ds_channel");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Dope Sheet Channel", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "dopesheet_subchannel", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "ds_subchannel");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Dope Sheet Sub-channel", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "channels", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "shade2");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Channels", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "channels_selected", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "hilite");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Channels Selected", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "channel_group", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "group");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Channel Group", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "active_channels_group", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "group_active");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Active Channel Group", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "long_key", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "strip");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Long Key", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "long_key_selected", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "strip_select");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Long Key Selected", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "keyframe", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "keytype_keyframe");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Keyframe", "Color of Keyframe");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "keyframe_selected", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "keytype_keyframe_select");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Keyframe Selected", "Color of selected keyframe");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "keyframe_extreme", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "keytype_extreme");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Extreme Keyframe", "Color of extreme keyframe");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "keyframe_extreme_selected", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "keytype_extreme_select");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(
      prop, "Extreme Keyframe Selected", "Color of selected extreme keyframe");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "keyframe_breakdown", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "keytype_breakdown");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Breakdown Keyframe", "Color of breakdown keyframe");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "keyframe_breakdown_selected", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "keytype_breakdown_select");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(
      prop, "Breakdown Keyframe Selected", "Color of selected breakdown keyframe");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "keyframe_jitter", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "keytype_jitter");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Jitter Keyframe", "Color of jitter keyframe");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "keyframe_jitter_selected", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "keytype_jitter_select");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Jitter Keyframe Selected", "Color of selected jitter keyframe");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "keyframe_movehold", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "keytype_movehold");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Moving Hold Keyframe", "Color of moving hold keyframe");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "keyframe_movehold_selected", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "keytype_movehold_select");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(
      prop, "Moving Hold Keyframe Selected", "Color of selected moving hold keyframe");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "keyframe_border", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "keyborder");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Keyframe Border", "Color of keyframe border");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "keyframe_border_selected", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "keyborder_select");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Keyframe Border Selected", "Color of selected keyframe border");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "keyframe_scale_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "keyframe_scale_fac");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(
      prop, "Keyframe Scale Factor", "Scale factor for adjusting the height of keyframes");
  /* NOTE: These limits prevent buttons overlapping (min), and excessive size... (max). */
  RNA_def_property_range(prop, 0.8f, 5.0f);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_DOPESHEET, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "summary", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "anim_active");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Summary", "Color of summary channel");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "preview_range", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "anim_preview_range");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Preview Range", "Color of preview range overlay");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "interpolation_line", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "ds_ipoline");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(
      prop, "Interpolation Line", "Color of lines showing non-bezier interpolation modes");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
}

static void rna_def_userdef_theme_space_nla(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* space_nla */
  srna = RNA_def_struct(brna, "ThemeNLAEditor", NULL);
  RNA_def_struct_sdna(srna, "ThemeSpace");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Theme Nonlinear Animation", "Theme settings for the NLA Editor");

  rna_def_userdef_theme_spaces_main(srna);
  rna_def_userdef_theme_spaces_list_main(srna);

  prop = RNA_def_property(srna, "grid", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Grid", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "view_sliders", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "shade1");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "View Sliders", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "dopesheet_channel", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "ds_channel");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Channel", "Nonlinear Animation Channel");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "nla_track", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "nla_track");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Track", "Nonlinear Animation Track");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "active_action", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "anim_active");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Active Action", "Animation data-block has active action");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "active_action_unset", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "anim_non_active");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(
      prop, "No Active Action", "Animation data-block doesn't have active action");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "preview_range", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "anim_preview_range");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Preview Range", "Color of preview range overlay");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "strips", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "strip");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Strips", "Unselected Action-Clip Strip");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "strips_selected", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "strip_select");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Strips Selected", "Selected Action-Clip Strip");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "transition_strips", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "nla_transition");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Transitions", "Unselected Transition Strip");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "transition_strips_selected", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "nla_transition_sel");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Transitions Selected", "Selected Transition Strip");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "meta_strips", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "nla_meta");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(
      prop, "Meta Strips", "Unselected Meta Strip (for grouping related strips)");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "meta_strips_selected", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "nla_meta_sel");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(
      prop, "Meta Strips Selected", "Selected Meta Strip (for grouping related strips)");
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  prop = RNA_def_property(srna, "sound_strips", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "nla_sound");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(
      prop, "Sound Strips", "Unselected Sound Strip (for timing speaker sounds)");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "sound_strips_selected", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "nla_sound_sel");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(
      prop, "Sound Strips Selected", "Selected Sound Strip (for timing speaker sounds)");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "tweak", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "nla_tweaking");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Tweak", "Color for strip/action being \"tweaked\" or edited");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "tweak_duplicate", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "nla_tweakdupli");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(
      prop,
      "Tweak Duplicate Flag",
      "Warning/error indicator color for strips referencing the strip being tweaked");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "keyframe_border", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "keyborder");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Keyframe Border", "Color of keyframe border");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "keyframe_border_selected", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "keyborder_select");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Keyframe Border Selected", "Color of selected keyframe border");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "frame_current", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "cframe");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Current Frame", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "time_scrub_background", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Scrubbing/Markers Region", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "time_marker_line", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Marker Line", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "time_marker_line_selected", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Marker Line Selected", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
}

static void rna_def_userdef_theme_colorset(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ThemeBoneColorSet", NULL);
  RNA_def_struct_sdna(srna, "ThemeWireColor");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Theme Bone Color Set", "Theme settings for bone color sets");

  prop = RNA_def_property(srna, "normal", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "solid");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Regular", "Color used for the surface of bones");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "select", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "select");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Select", "Color used for selected bones");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "active", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Active", "Color used for active bones");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "show_colored_constraints", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", TH_WIRECOLOR_CONSTCOLS);
  RNA_def_property_ui_text(
      prop, "Colored Constraints", "Allow the use of colors indicating constraints/keyed status");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
}

static void rna_def_userdef_theme_collection_color(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ThemeCollectionColor", NULL);
  RNA_def_struct_sdna(srna, "ThemeCollectionColor");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Theme Collection Color", "Theme settings for collection colors");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "color");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Color", "Collection Color Tag");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
}

static void rna_def_userdef_theme_strip_color(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ThemeStripColor", NULL);
  RNA_def_struct_sdna(srna, "ThemeStripColor");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Theme Strip Color", "Theme settings for strip colors");

  prop = RNA_def_property(srna, "color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "color");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Color", "Strip Color");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");
}

static void rna_def_userdef_theme_space_clip(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* space_clip */

  srna = RNA_def_struct(brna, "ThemeClipEditor", NULL);
  RNA_def_struct_sdna(srna, "ThemeSpace");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Theme Clip Editor", "Theme settings for the Movie Clip Editor");

  rna_def_userdef_theme_spaces_main(srna);
  rna_def_userdef_theme_spaces_list_main(srna);

  prop = RNA_def_property(srna, "grid", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Grid", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "marker_outline", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "marker_outline");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Marker Outline", "Color of marker's outline");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "marker", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "marker");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Marker", "Color of marker");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "active_marker", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "act_marker");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Active Marker", "Color of active marker");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "selected_marker", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "sel_marker");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Selected Marker", "Color of selected marker");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "disabled_marker", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "dis_marker");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Disabled Marker", "Color of disabled marker");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "locked_marker", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "lock_marker");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Locked Marker", "Color of locked marker");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "path_before", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "path_before");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Path Before", "Color of path before current frame");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "path_after", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "path_after");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Path After", "Color of path after current frame");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "path_keyframe_before", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Path Before", "Color of path before current frame");
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  prop = RNA_def_property(srna, "path_keyframe_after", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Path After", "Color of path after current frame");
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  prop = RNA_def_property(srna, "frame_current", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "cframe");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Current Frame", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "time_scrub_background", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Scrubbing/Markers Region", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "time_marker_line", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Marker Line", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "time_marker_line_selected", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Marker Line Selected", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "strips", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "strip");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Strips", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "strips_selected", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "strip_select");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Strips Selected", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "metadatabg", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "metadatabg");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Metadata Background", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  prop = RNA_def_property(srna, "metadatatext", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "metadatatext");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Metadata Text", "");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  rna_def_userdef_theme_spaces_curves(srna, false, false, false, true);
}

static void rna_def_userdef_theme_space_topbar(BlenderRNA *brna)
{
  StructRNA *srna;

  /* space_topbar */

  srna = RNA_def_struct(brna, "ThemeTopBar", NULL);
  RNA_def_struct_sdna(srna, "ThemeSpace");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Theme Top Bar", "Theme settings for the Top Bar");

  rna_def_userdef_theme_spaces_main(srna);
}

static void rna_def_userdef_theme_space_statusbar(BlenderRNA *brna)
{
  StructRNA *srna;

  /* space_statusbar */

  srna = RNA_def_struct(brna, "ThemeStatusBar", NULL);
  RNA_def_struct_sdna(srna, "ThemeSpace");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Theme Status Bar", "Theme settings for the Status Bar");

  rna_def_userdef_theme_spaces_main(srna);
}

static void rna_def_userdef_theme_space_spreadsheet(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* space_spreadsheet */

  srna = RNA_def_struct(brna, "ThemeSpreadsheet", NULL);
  RNA_def_struct_sdna(srna, "ThemeSpace");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Theme Spreadsheet", "Theme settings for the Spreadsheet");

  prop = RNA_def_property(srna, "row_alternate", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Alternate Rows", "Overlay color on every other row");
  RNA_def_property_update(prop, 0, "rna_userdef_theme_update");

  rna_def_userdef_theme_spaces_main(srna);
  rna_def_userdef_theme_spaces_list_main(srna);
}

static void rna_def_userdef_themes(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem active_theme_area[] = {
      {0, "USER_INTERFACE", ICON_WORKSPACE, "User Interface", ""},
      {19, "STYLE", ICON_FONTPREVIEW, "Text Style", ""},
      {18, "BONE_COLOR_SETS", ICON_COLOR, "Bone Color Sets", ""},
      {1, "VIEW_3D", ICON_VIEW3D, "3D Viewport", ""},
      {3, "GRAPH_EDITOR", ICON_GRAPH, "Graph Editor", ""},
      {4, "DOPESHEET_EDITOR", ICON_ACTION, "Dope Sheet", ""},
      {5, "NLA_EDITOR", ICON_NLA, "Nonlinear Animation", ""},
      {6, "IMAGE_EDITOR", ICON_IMAGE, "UV/Image Editor", ""},
      {7, "SEQUENCE_EDITOR", ICON_SEQUENCE, "Video Sequencer", ""},
      {8, "TEXT_EDITOR", ICON_TEXT, "Text Editor", ""},
      {9, "NODE_EDITOR", ICON_NODETREE, "Node Editor", ""},
      {11, "PROPERTIES", ICON_PROPERTIES, "Properties", ""},
      {12, "OUTLINER", ICON_OUTLINER, "Outliner", ""},
      {14, "PREFERENCES", ICON_PREFERENCES, "Preferences", ""},
      {15, "INFO", ICON_INFO, "Info", ""},
      {16, "FILE_BROWSER", ICON_FILEBROWSER, "File Browser", ""},
      {17, "CONSOLE", ICON_CONSOLE, "Python Console", ""},
      {20, "CLIP_EDITOR", ICON_TRACKER, "Movie Clip Editor", ""},
      {21, "TOPBAR", ICON_TOPBAR, "Top Bar", ""},
      {22, "STATUSBAR", ICON_STATUSBAR, "Status Bar", ""},
      {23, "SPREADSHEET", ICON_SPREADSHEET, "Spreadsheet"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "Theme", NULL);
  RNA_def_struct_sdna(srna, "bTheme");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Theme", "User interface styling and color settings");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Name", "Name of the theme");
  RNA_def_struct_name_property(srna, prop);
  /* XXX: for now putting this in presets is silly - its just Default */
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  prop = RNA_def_property(srna, "theme_area", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "active_theme_area");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  RNA_def_property_enum_items(prop, active_theme_area);
  RNA_def_property_ui_text(prop, "Active Theme Area", "");

  prop = RNA_def_property(srna, "user_interface", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "tui");
  RNA_def_property_struct_type(prop, "ThemeUserInterface");
  RNA_def_property_ui_text(prop, "User Interface", "");

  /* Space Types */
  prop = RNA_def_property(srna, "view_3d", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "space_view3d");
  RNA_def_property_struct_type(prop, "ThemeView3D");
  RNA_def_property_ui_text(prop, "3D Viewport", "");

  prop = RNA_def_property(srna, "graph_editor", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "space_graph");
  RNA_def_property_struct_type(prop, "ThemeGraphEditor");
  RNA_def_property_ui_text(prop, "Graph Editor", "");

  prop = RNA_def_property(srna, "file_browser", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "space_file");
  RNA_def_property_struct_type(prop, "ThemeFileBrowser");
  RNA_def_property_ui_text(prop, "File Browser", "");

  prop = RNA_def_property(srna, "nla_editor", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "space_nla");
  RNA_def_property_struct_type(prop, "ThemeNLAEditor");
  RNA_def_property_ui_text(prop, "Nonlinear Animation", "");

  prop = RNA_def_property(srna, "dopesheet_editor", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "space_action");
  RNA_def_property_struct_type(prop, "ThemeDopeSheet");
  RNA_def_property_ui_text(prop, "Dope Sheet", "");

  prop = RNA_def_property(srna, "image_editor", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "space_image");
  RNA_def_property_struct_type(prop, "ThemeImageEditor");
  RNA_def_property_ui_text(prop, "Image Editor", "");

  prop = RNA_def_property(srna, "sequence_editor", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "space_sequencer");
  RNA_def_property_struct_type(prop, "ThemeSequenceEditor");
  RNA_def_property_ui_text(prop, "Sequence Editor", "");

  prop = RNA_def_property(srna, "properties", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "space_properties");
  RNA_def_property_struct_type(prop, "ThemeProperties");
  RNA_def_property_ui_text(prop, "Properties", "");

  prop = RNA_def_property(srna, "text_editor", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "space_text");
  RNA_def_property_struct_type(prop, "ThemeTextEditor");
  RNA_def_property_ui_text(prop, "Text Editor", "");

  prop = RNA_def_property(srna, "node_editor", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "space_node");
  RNA_def_property_struct_type(prop, "ThemeNodeEditor");
  RNA_def_property_ui_text(prop, "Node Editor", "");

  prop = RNA_def_property(srna, "outliner", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "space_outliner");
  RNA_def_property_struct_type(prop, "ThemeOutliner");
  RNA_def_property_ui_text(prop, "Outliner", "");

  prop = RNA_def_property(srna, "info", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "space_info");
  RNA_def_property_struct_type(prop, "ThemeInfo");
  RNA_def_property_ui_text(prop, "Info", "");

  prop = RNA_def_property(srna, "preferences", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "space_preferences");
  RNA_def_property_struct_type(prop, "ThemePreferences");
  RNA_def_property_ui_text(prop, "Preferences", "");

  prop = RNA_def_property(srna, "console", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "space_console");
  RNA_def_property_struct_type(prop, "ThemeConsole");
  RNA_def_property_ui_text(prop, "Console", "");

  prop = RNA_def_property(srna, "clip_editor", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "space_clip");
  RNA_def_property_struct_type(prop, "ThemeClipEditor");
  RNA_def_property_ui_text(prop, "Clip Editor", "");

  prop = RNA_def_property(srna, "topbar", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "space_topbar");
  RNA_def_property_struct_type(prop, "ThemeTopBar");
  RNA_def_property_ui_text(prop, "Top Bar", "");

  prop = RNA_def_property(srna, "statusbar", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "space_statusbar");
  RNA_def_property_struct_type(prop, "ThemeStatusBar");
  RNA_def_property_ui_text(prop, "Status Bar", "");

  prop = RNA_def_property(srna, "spreadsheet", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "space_spreadsheet");
  RNA_def_property_struct_type(prop, "ThemeSpreadsheet");
  RNA_def_property_ui_text(prop, "Spreadsheet", "");
  /* end space types */

  prop = RNA_def_property(srna, "bone_color_sets", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_collection_sdna(prop, NULL, "tarm", "");
  RNA_def_property_struct_type(prop, "ThemeBoneColorSet");
  RNA_def_property_ui_text(prop, "Bone Color Sets", "");

  prop = RNA_def_property(srna, "collection_color", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_collection_sdna(prop, NULL, "collection_color", "");
  RNA_def_property_struct_type(prop, "ThemeCollectionColor");
  RNA_def_property_ui_text(prop, "Collection Color", "");

  prop = RNA_def_property(srna, "strip_color", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_collection_sdna(prop, NULL, "strip_color", "");
  RNA_def_property_struct_type(prop, "ThemeStripColor");
  RNA_def_property_ui_text(prop, "Strip Color", "");
}

static void rna_def_userdef_addon(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Addon", NULL);
  RNA_def_struct_sdna(srna, "bAddon");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Add-on", "Python add-ons to be loaded automatically");

  prop = RNA_def_property(srna, "module", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Module", "Module name");
  RNA_def_struct_name_property(srna, prop);

  /* Collection active property */
  prop = RNA_def_property(srna, "preferences", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "AddonPreferences");
  RNA_def_property_pointer_funcs(prop, "rna_Addon_preferences_get", NULL, NULL, NULL);
}

static void rna_def_userdef_studiolights(BlenderRNA *brna)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  srna = RNA_def_struct(brna, "StudioLights", NULL);
  RNA_def_struct_sdna(srna, "UserDef");
  RNA_def_struct_ui_text(srna, "Studio Lights", "Collection of studio lights");

  func = RNA_def_function(srna, "load", "rna_StudioLights_load");
  RNA_def_function_ui_description(func, "Load studiolight from file");
  parm = RNA_def_string(
      func, "path", NULL, 0, "File Path", "File path where the studio light file can be found");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_enum(func,
                      "type",
                      rna_enum_studio_light_type_items,
                      STUDIOLIGHT_TYPE_WORLD,
                      "Type",
                      "The type for the new studio light");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "studio_light", "StudioLight", "", "Newly created StudioLight");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "new", "rna_StudioLights_new");
  RNA_def_function_ui_description(func, "Create studiolight from default lighting");
  parm = RNA_def_string(
      func,
      "path",
      NULL,
      0,
      "Path",
      "Path to the file that will contain the lighting info (without extension)");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED);
  parm = RNA_def_pointer(func, "studio_light", "StudioLight", "", "Newly created StudioLight");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_StudioLights_remove");
  RNA_def_function_ui_description(func, "Remove a studio light");
  parm = RNA_def_pointer(func, "studio_light", "StudioLight", "", "The studio light to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED);

  func = RNA_def_function(srna, "refresh", "rna_StudioLights_refresh");
  RNA_def_function_ui_description(func, "Refresh Studio Lights from disk");
}

static void rna_def_userdef_studiolight(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  RNA_define_verify_sdna(false);
  srna = RNA_def_struct(brna, "StudioLight", NULL);
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Studio Light", "Studio light");

  prop = RNA_def_property(srna, "index", PROP_INT, PROP_NONE);
  RNA_def_property_int_funcs(prop, "rna_UserDef_studiolight_index_get", NULL, NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Index", "");

  prop = RNA_def_property(srna, "is_user_defined", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_UserDef_studiolight_is_user_defined_get", NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "User Defined", "");

  prop = RNA_def_property(srna, "has_specular_highlight_pass", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_UserDef_studiolight_has_specular_highlight_pass_get", NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop,
      "Has Specular Highlight",
      "Studio light image file has separate \"diffuse\" and \"specular\" passes");

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_studio_light_type_items);
  RNA_def_property_enum_funcs(prop, "rna_UserDef_studiolight_type_get", NULL, NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Type", "");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, "rna_UserDef_studiolight_name_get", "rna_UserDef_studiolight_name_length", NULL);
  RNA_def_property_ui_text(prop, "Name", "");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "path", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_funcs(
      prop, "rna_UserDef_studiolight_path_get", "rna_UserDef_studiolight_path_length", NULL);
  RNA_def_property_ui_text(prop, "Path", "");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "solid_lights", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "light_param", "");
  RNA_def_property_struct_type(prop, "UserSolidLight");
  RNA_def_property_collection_funcs(prop,
                                    "rna_UserDef_studiolight_solid_lights_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_get",
                                    "rna_UserDef_studiolight_solid_lights_length",
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_ui_text(
      prop, "Solid Lights", "Lights user to display objects in solid draw mode");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "light_ambient", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(prop, "rna_UserDef_studiolight_light_ambient_get", NULL, NULL);
  RNA_def_property_ui_text(
      prop, "Ambient Color", "Color of the ambient light that uniformly lit the scene");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "path_irr_cache", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_funcs(prop,
                                "rna_UserDef_studiolight_path_irr_cache_get",
                                "rna_UserDef_studiolight_path_irr_cache_length",
                                NULL);
  RNA_def_property_ui_text(
      prop, "Irradiance Cache Path", "Path where the irradiance cache is stored");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "path_sh_cache", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_funcs(prop,
                                "rna_UserDef_studiolight_path_sh_cache_get",
                                "rna_UserDef_studiolight_path_sh_cache_length",
                                NULL);
  RNA_def_property_ui_text(
      prop, "SH Cache Path", "Path where the spherical harmonics cache is stored");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  const int spherical_harmonics_dim[] = {STUDIOLIGHT_SH_EFFECTIVE_COEFS_LEN, 3};
  prop = RNA_def_property(srna, "spherical_harmonics_coefficients", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_multi_array(prop, 2, spherical_harmonics_dim);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(
      prop, "rna_UserDef_studiolight_spherical_harmonics_coefficients_get", NULL, NULL);

  RNA_define_verify_sdna(true);
}

static void rna_def_userdef_pathcompare(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "PathCompare", NULL);
  RNA_def_struct_sdna(srna, "bPathCompare");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Path Compare", "Match paths against this value");

  prop = RNA_def_property(srna, "path", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_ui_text(prop, "Path", "");
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "use_glob", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", USER_PATHCMP_GLOB);
  RNA_def_property_ui_text(prop, "Use Wildcard", "Enable wildcard globbing");
}

static void rna_def_userdef_addon_pref(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "AddonPreferences", NULL);
  RNA_def_struct_ui_text(srna, "Add-on Preferences", "");
  RNA_def_struct_sdna(srna, "bAddon"); /* WARNING: only a bAddon during registration */

  RNA_def_struct_refine_func(srna, "rna_AddonPref_refine");
  RNA_def_struct_register_funcs(srna, "rna_AddonPref_register", "rna_AddonPref_unregister", NULL);
  RNA_def_struct_idprops_func(srna, "rna_AddonPref_idprops");
  RNA_def_struct_flag(srna, STRUCT_NO_DATABLOCK_IDPROPERTIES); /* Mandatory! */

  USERDEF_TAG_DIRTY_PROPERTY_UPDATE_DISABLE;

  /* registration */
  RNA_define_verify_sdna(0);
  prop = RNA_def_property(srna, "bl_idname", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "module");
  RNA_def_property_flag(prop, PROP_REGISTER);
  RNA_define_verify_sdna(1);

  USERDEF_TAG_DIRTY_PROPERTY_UPDATE_ENABLE;
}

static void rna_def_userdef_dothemes(BlenderRNA *brna)
{

  rna_def_userdef_theme_ui_style(brna);
  rna_def_userdef_theme_ui(brna);

  rna_def_userdef_theme_space_generic(brna);
  rna_def_userdef_theme_space_gradient(brna);
  rna_def_userdef_theme_space_list_generic(brna);

  rna_def_userdef_theme_space_view3d(brna);
  rna_def_userdef_theme_space_graph(brna);
  rna_def_userdef_theme_space_file(brna);
  rna_def_userdef_theme_space_nla(brna);
  rna_def_userdef_theme_space_action(brna);
  rna_def_userdef_theme_space_image(brna);
  rna_def_userdef_theme_space_seq(brna);
  rna_def_userdef_theme_space_buts(brna);
  rna_def_userdef_theme_space_text(brna);
  rna_def_userdef_theme_space_node(brna);
  rna_def_userdef_theme_space_outliner(brna);
  rna_def_userdef_theme_space_info(brna);
  rna_def_userdef_theme_space_userpref(brna);
  rna_def_userdef_theme_space_console(brna);
  rna_def_userdef_theme_space_clip(brna);
  rna_def_userdef_theme_space_topbar(brna);
  rna_def_userdef_theme_space_statusbar(brna);
  rna_def_userdef_theme_space_spreadsheet(brna);
  rna_def_userdef_theme_colorset(brna);
  rna_def_userdef_theme_collection_color(brna);
  rna_def_userdef_theme_strip_color(brna);
  rna_def_userdef_themes(brna);
}

static void rna_def_userdef_solidlight(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  static float default_dir[3] = {0.0f, 0.0f, 1.0f};
  static float default_col[3] = {0.8f, 0.8f, 0.8f};

  srna = RNA_def_struct(brna, "UserSolidLight", NULL);
  RNA_def_struct_sdna(srna, "SolidLight");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(
      srna, "Solid Light", "Light used for Studio lighting in solid shading mode");

  prop = RNA_def_property(srna, "use", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", 1);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(prop, "Enabled", "Enable this light in solid shading mode");
  RNA_def_property_update(prop, 0, "rna_UserDef_viewport_lights_update");

  prop = RNA_def_property(srna, "smooth", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "smooth");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Smooth", "Smooth the lighting from this light");
  RNA_def_property_update(prop, 0, "rna_UserDef_viewport_lights_update");

  prop = RNA_def_property(srna, "direction", PROP_FLOAT, PROP_DIRECTION);
  RNA_def_property_float_sdna(prop, NULL, "vec");
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_array_default(prop, default_dir);
  RNA_def_property_ui_text(prop, "Direction", "Direction that the light is shining");
  RNA_def_property_update(prop, 0, "rna_UserDef_viewport_lights_update");

  prop = RNA_def_property(srna, "specular_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, NULL, "spec");
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_array_default(prop, default_col);
  RNA_def_property_ui_text(prop, "Specular Color", "Color of the light's specular highlight");
  RNA_def_property_update(prop, 0, "rna_UserDef_viewport_lights_update");

  prop = RNA_def_property(srna, "diffuse_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, NULL, "col");
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_array_default(prop, default_col);
  RNA_def_property_ui_text(prop, "Diffuse Color", "Color of the light's diffuse highlight");
  RNA_def_property_update(prop, 0, "rna_UserDef_viewport_lights_update");
}

static void rna_def_userdef_walk_navigation(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "WalkNavigation", NULL);
  RNA_def_struct_sdna(srna, "WalkNavigation");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Walk Navigation", "Walk navigation settings");

  prop = RNA_def_property(srna, "mouse_speed", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.01f, 10.0f);
  RNA_def_property_ui_text(
      prop,
      "Mouse Sensitivity",
      "Speed factor for when looking around, high values mean faster mouse movement");

  prop = RNA_def_property(srna, "walk_speed", PROP_FLOAT, PROP_VELOCITY);
  RNA_def_property_range(prop, 0.01f, 100.0f);
  RNA_def_property_ui_text(prop, "Walk Speed", "Base speed for walking and flying");

  prop = RNA_def_property(srna, "walk_speed_factor", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.01f, 10.0f);
  RNA_def_property_ui_text(
      prop, "Speed Factor", "Multiplication factor when using the fast or slow modifiers");

  prop = RNA_def_property(srna, "view_height", PROP_FLOAT, PROP_UNIT_LENGTH);
  RNA_def_property_ui_range(prop, 0.1f, 10.0f, 0.1, 2);
  RNA_def_property_range(prop, 0.0f, 1000.0f);
  RNA_def_property_ui_text(prop, "View Height", "View distance from the floor when walking");

  prop = RNA_def_property(srna, "jump_height", PROP_FLOAT, PROP_UNIT_LENGTH);
  RNA_def_property_ui_range(prop, 0.1f, 10.0f, 0.1, 2);
  RNA_def_property_range(prop, 0.1f, 100.0f);
  RNA_def_property_ui_text(prop, "Jump Height", "Maximum height of a jump");

  prop = RNA_def_property(srna, "teleport_time", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0f, 10.0f);
  RNA_def_property_ui_text(
      prop, "Teleport Duration", "Interval of time warp when teleporting in navigation mode");

  prop = RNA_def_property(srna, "use_gravity", PROP_BOOLEAN, PROP_BOOLEAN);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", USER_WALK_GRAVITY);
  RNA_def_property_ui_text(prop, "Gravity", "Walk with gravity, or free navigate");

  prop = RNA_def_property(srna, "use_mouse_reverse", PROP_BOOLEAN, PROP_BOOLEAN);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", USER_WALK_MOUSE_REVERSE);
  RNA_def_property_ui_text(prop, "Reverse Mouse", "Reverse the vertical movement of the mouse");
}

static void rna_def_userdef_view(BlenderRNA *brna)
{
  static const EnumPropertyItem timecode_styles[] = {
      {USER_TIMECODE_MINIMAL,
       "MINIMAL",
       0,
       "Minimal Info",
       "Most compact representation, uses '+' as separator for sub-second frame numbers, "
       "with left and right truncation of the timecode as necessary"},
      {USER_TIMECODE_SMPTE_FULL,
       "SMPTE",
       0,
       "SMPTE (Full)",
       "Full SMPTE timecode (format is HH:MM:SS:FF)"},
      {USER_TIMECODE_SMPTE_MSF,
       "SMPTE_COMPACT",
       0,
       "SMPTE (Compact)",
       "SMPTE timecode showing minutes, seconds, and frames only - "
       "hours are also shown if necessary, but not by default"},
      {USER_TIMECODE_MILLISECONDS,
       "MILLISECONDS",
       0,
       "Compact with Milliseconds",
       "Similar to SMPTE (Compact), except that instead of frames, "
       "milliseconds are shown instead"},
      {USER_TIMECODE_SECONDS_ONLY,
       "SECONDS_ONLY",
       0,
       "Only Seconds",
       "Direct conversion of frame numbers to seconds"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem color_picker_types[] = {
      {USER_CP_CIRCLE_HSV,
       "CIRCLE_HSV",
       0,
       "Circle (HSV)",
       "A circular Hue/Saturation color wheel, with "
       "Value slider"},
      {USER_CP_CIRCLE_HSL,
       "CIRCLE_HSL",
       0,
       "Circle (HSL)",
       "A circular Hue/Saturation color wheel, with "
       "Lightness slider"},
      {USER_CP_SQUARE_SV,
       "SQUARE_SV",
       0,
       "Square (SV + H)",
       "A square showing Saturation/Value, with Hue slider"},
      {USER_CP_SQUARE_HS,
       "SQUARE_HS",
       0,
       "Square (HS + V)",
       "A square showing Hue/Saturation, with Value slider"},
      {USER_CP_SQUARE_HV,
       "SQUARE_HV",
       0,
       "Square (HV + S)",
       "A square showing Hue/Value, with Saturation slider"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem zoom_frame_modes[] = {
      {ZOOM_FRAME_MODE_KEEP_RANGE, "KEEP_RANGE", 0, "Keep Range", ""},
      {ZOOM_FRAME_MODE_SECONDS, "SECONDS", 0, "Seconds", ""},
      {ZOOM_FRAME_MODE_KEYFRAMES, "KEYFRAMES", 0, "Keyframes", ""},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem line_width[] = {
      {-1, "THIN", 0, "Thin", "Thinner lines than the default"},
      {0, "AUTO", 0, "Default", "Automatic line width based on UI scale"},
      {1, "THICK", 0, "Thick", "Thicker lines than the default"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem render_display_types[] = {
      {USER_RENDER_DISPLAY_NONE,
       "NONE",
       0,
       "Keep User Interface",
       "Images are rendered without changing the user interface"},
      {USER_RENDER_DISPLAY_SCREEN,
       "SCREEN",
       0,
       "Maximized Area",
       "Images are rendered in a maximized Image Editor"},
      {USER_RENDER_DISPLAY_AREA,
       "AREA",
       0,
       "Image Editor",
       "Images are rendered in an Image Editor"},
      {USER_RENDER_DISPLAY_WINDOW,
       "WINDOW",
       0,
       "New Window",
       "Images are rendered in a new window"},
      {0, NULL, 0, NULL, NULL},
  };
  static const EnumPropertyItem temp_space_display_types[] = {
      {USER_TEMP_SPACE_DISPLAY_FULLSCREEN,
       "SCREEN", /* Could be FULLSCREEN, but keeping it consistent with render_display_types */
       0,
       "Maximized Area",
       "Open the temporary editor in a maximized screen"},
      {USER_TEMP_SPACE_DISPLAY_WINDOW,
       "WINDOW",
       0,
       "New Window",
       "Open the temporary editor in a new window"},
      {0, NULL, 0, NULL, NULL},
  };

  PropertyRNA *prop;
  StructRNA *srna;

  srna = RNA_def_struct(brna, "PreferencesView", NULL);
  RNA_def_struct_sdna(srna, "UserDef");
  RNA_def_struct_nested(brna, srna, "Preferences");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "View & Controls", "Preferences related to viewing data");

  /* View. */
  prop = RNA_def_property(srna, "ui_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "UI Scale", "Changes the size of the fonts and widgets in the interface");
  RNA_def_property_range(prop, 0.25f, 4.0f);
  RNA_def_property_ui_range(prop, 0.5f, 2.0f, 1, 2);
  RNA_def_property_update(prop, 0, "rna_userdef_dpi_update");

  prop = RNA_def_property(srna, "ui_line_width", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, line_width);
  RNA_def_property_ui_text(
      prop,
      "UI Line Width",
      "Changes the thickness of widget outlines, lines and dots in the interface");
  RNA_def_property_update(prop, 0, "rna_userdef_dpi_update");

  /* display */
  prop = RNA_def_property(srna, "show_tooltips", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", USER_TOOLTIPS);
  RNA_def_property_ui_text(
      prop, "Tooltips", "Display tooltips (when disabled, hold Alt to force display)");

  prop = RNA_def_property(srna, "show_tooltips_python", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", USER_TOOLTIPS_PYTHON);
  RNA_def_property_ui_text(prop, "Python Tooltips", "Show Python references in tooltips");

  prop = RNA_def_property(srna, "show_developer_ui", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", USER_DEVELOPER_UI);
  RNA_def_property_ui_text(
      prop,
      "Developer Extras",
      "Show options for developers (edit source in context menu, geometry indices)");
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  prop = RNA_def_property(srna, "show_object_info", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "uiflag", USER_DRAWVIEWINFO);
  RNA_def_property_ui_text(prop,
                           "Display Object Info",
                           "Include the name of the active object and the current frame number in "
                           "the text info overlay");
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  prop = RNA_def_property(srna, "show_view_name", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "uiflag", USER_SHOW_VIEWPORTNAME);
  RNA_def_property_ui_text(prop,
                           "Display View Name",
                           "Include the name of the view orientation in the text info overlay");
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  prop = RNA_def_property(srna, "show_splash", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "uiflag", USER_SPLASH_DISABLE);
  RNA_def_property_ui_text(prop, "Show Splash", "Display splash screen on startup");

  prop = RNA_def_property(srna, "show_playback_fps", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "uiflag", USER_SHOW_FPS);
  RNA_def_property_ui_text(prop,
                           "Display Playback Frame Rate (FPS)",
                           "Include the number of frames displayed per second in the text info "
                           "overlay while animation is played back");
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  USERDEF_TAG_DIRTY_PROPERTY_UPDATE_DISABLE;
  prop = RNA_def_property(srna, "show_addons_enabled_only", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, NULL, "space_data.flag", USER_SPACEDATA_ADDONS_SHOW_ONLY_ENABLED);
  RNA_def_property_ui_text(prop,
                           "Enabled Add-ons Only",
                           "Only show enabled add-ons. Un-check to see all installed add-ons");
  USERDEF_TAG_DIRTY_PROPERTY_UPDATE_ENABLE;

  static const EnumPropertyItem factor_display_items[] = {
      {USER_FACTOR_AS_FACTOR, "FACTOR", 0, "Factor", "Display factors as values between 0 and 1"},
      {USER_FACTOR_AS_PERCENTAGE, "PERCENTAGE", 0, "Percentage", "Display factors as percentages"},
      {0, NULL, 0, NULL, NULL},
  };

  prop = RNA_def_property(srna, "factor_display_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, factor_display_items);
  RNA_def_property_ui_text(prop, "Factor Display Type", "How factor values are displayed");
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  /* Weight Paint */

  prop = RNA_def_property(srna, "use_weight_color_range", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", USER_CUSTOM_RANGE);
  RNA_def_property_ui_text(
      prop,
      "Use Weight Color Range",
      "Enable color range used for weight visualization in weight painting mode");
  RNA_def_property_update(prop, 0, "rna_UserDef_weight_color_update");

  prop = RNA_def_property(srna, "weight_color_range", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "coba_weight");
  RNA_def_property_struct_type(prop, "ColorRamp");
  RNA_def_property_ui_text(prop,
                           "Weight Color Range",
                           "Color range used for weight visualization in weight painting mode");
  RNA_def_property_update(prop, 0, "rna_UserDef_weight_color_update");

  prop = RNA_def_property(srna, "show_navigate_ui", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "uiflag", USER_SHOW_GIZMO_NAVIGATE);
  RNA_def_property_ui_text(
      prop,
      "Navigation Controls",
      "Show navigation controls in 2D and 3D views which do not have scroll bars");
  RNA_def_property_update(prop, 0, "rna_userdef_gizmo_update");

  /* menus */
  prop = RNA_def_property(srna, "use_mouse_over_open", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "uiflag", USER_MENUOPENAUTO);
  RNA_def_property_ui_text(
      prop,
      "Open on Mouse Over",
      "Open menu buttons and pulldowns automatically when the mouse is hovering");

  prop = RNA_def_property(srna, "open_toplevel_delay", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "menuthreshold1");
  RNA_def_property_range(prop, 1, 40);
  RNA_def_property_ui_text(
      prop,
      "Top Level Menu Open Delay",
      "Time delay in 1/10 seconds before automatically opening top level menus");

  prop = RNA_def_property(srna, "open_sublevel_delay", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "menuthreshold2");
  RNA_def_property_range(prop, 1, 40);
  RNA_def_property_ui_text(
      prop,
      "Sub Level Menu Open Delay",
      "Time delay in 1/10 seconds before automatically opening sub level menus");

  prop = RNA_def_property(srna, "color_picker_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, color_picker_types);
  RNA_def_property_enum_sdna(prop, NULL, "color_picker_type");
  RNA_def_property_ui_text(
      prop, "Color Picker Type", "Different styles of displaying the color picker widget");
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  /* pie menus */
  prop = RNA_def_property(srna, "pie_initial_timeout", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 0, 1000);
  RNA_def_property_ui_text(
      prop,
      "Recenter Timeout",
      "Pie menus will use the initial mouse position as center for this amount of time "
      "(in 1/100ths of sec)");

  prop = RNA_def_property(srna, "pie_tap_timeout", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 0, 1000);
  RNA_def_property_ui_text(prop,
                           "Tap Key Timeout",
                           "Pie menu button held longer than this will dismiss menu on release."
                           "(in 1/100ths of sec)");

  prop = RNA_def_property(srna, "pie_animation_timeout", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 0, 1000);
  RNA_def_property_ui_text(
      prop,
      "Animation Timeout",
      "Time needed to fully animate the pie to unfolded state (in 1/100ths of sec)");

  prop = RNA_def_property(srna, "pie_menu_radius", PROP_INT, PROP_PIXEL);
  RNA_def_property_range(prop, 0, 1000);
  RNA_def_property_ui_text(prop, "Radius", "Pie menu size in pixels");

  prop = RNA_def_property(srna, "pie_menu_threshold", PROP_INT, PROP_PIXEL);
  RNA_def_property_range(prop, 0, 1000);
  RNA_def_property_ui_text(
      prop, "Threshold", "Distance from center needed before a selection can be made");

  prop = RNA_def_property(srna, "pie_menu_confirm", PROP_INT, PROP_PIXEL);
  RNA_def_property_range(prop, 0, 1000);
  RNA_def_property_ui_text(prop,
                           "Confirm Threshold",
                           "Distance threshold after which selection is made (zero to disable)");

  prop = RNA_def_property(srna, "use_save_prompt", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "uiflag", USER_SAVE_PROMPT);
  RNA_def_property_ui_text(
      prop, "Save Prompt", "Ask for confirmation when quitting with unsaved changes");

  prop = RNA_def_property(srna, "show_column_layout", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "uiflag", USER_PLAINMENUS);
  RNA_def_property_ui_text(prop, "Toolbox Column Layout", "Use a column layout for toolbox");

  prop = RNA_def_property(srna, "use_directional_menus", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "uiflag", USER_MENUFIXEDORDER);
  RNA_def_property_ui_text(prop,
                           "Contents Follow Opening Direction",
                           "Otherwise menus, etc will always be top to bottom, left to right, "
                           "no matter opening direction");

  static const EnumPropertyItem header_align_items[] = {
      {0, "NONE", 0, "Keep Existing", "Keep existing header alignment"},
      {USER_HEADER_FROM_PREF, "TOP", 0, "Top", "Top aligned on load"},
      {USER_HEADER_FROM_PREF | USER_HEADER_BOTTOM,
       "BOTTOM",
       0,
       "Bottom",
       "Bottom align on load (except for property editors)"},
      {0, NULL, 0, NULL, NULL},
  };
  prop = RNA_def_property(srna, "header_align", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, header_align_items);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "uiflag");
  RNA_def_property_ui_text(prop, "Header Position", "Default header position for new space-types");
  RNA_def_property_update(prop, 0, "rna_userdef_screen_update_header_default");

  prop = RNA_def_property(srna, "render_display_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, render_display_types);
  RNA_def_property_ui_text(
      prop, "Render Display Type", "Default location where rendered images will be displayed in");

  prop = RNA_def_property(srna, "filebrowser_display_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, temp_space_display_types);
  RNA_def_property_ui_text(prop,
                           "File Browser Display Type",
                           "Default location where the File Editor will be displayed in");

  static const EnumPropertyItem text_hinting_items[] = {
      {0, "AUTO", 0, "Auto", ""},
      {USER_TEXT_HINTING_NONE, "NONE", 0, "None", ""},
      {USER_TEXT_HINTING_SLIGHT, "SLIGHT", 0, "Slight", ""},
      {USER_TEXT_HINTING_FULL, "FULL", 0, "Full", ""},
      {0, NULL, 0, NULL, NULL},
  };

  /* mini axis */
  static const EnumPropertyItem mini_axis_type_items[] = {
      {USER_MINI_AXIS_TYPE_NONE, "NONE", 0, "Off", ""},
      {USER_MINI_AXIS_TYPE_MINIMAL, "MINIMAL", 0, "Simple Axis", ""},
      {USER_MINI_AXIS_TYPE_GIZMO, "GIZMO", 0, "Interactive Navigation", ""},
      {0, NULL, 0, NULL, NULL},
  };

  prop = RNA_def_property(srna, "mini_axis_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, mini_axis_type_items);
  RNA_def_property_ui_text(
      prop,
      "Mini Axes Type",
      "Show a small rotating 3D axes in the top right corner of the 3D viewport");
  RNA_def_property_update(prop, 0, "rna_userdef_gizmo_update");

  prop = RNA_def_property(srna, "mini_axis_size", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, NULL, "rvisize");
  RNA_def_property_range(prop, 10, 64);
  RNA_def_property_ui_text(prop, "Mini Axes Size", "The axes icon's size");
  RNA_def_property_update(prop, 0, "rna_userdef_gizmo_update");

  prop = RNA_def_property(srna, "mini_axis_brightness", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "rvibright");
  RNA_def_property_range(prop, 0, 10);
  RNA_def_property_ui_text(prop, "Mini Axes Brightness", "Brightness of the icon");
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  prop = RNA_def_property(srna, "smooth_view", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "smooth_viewtx");
  RNA_def_property_range(prop, 0, 1000);
  RNA_def_property_ui_text(
      prop, "Smooth View", "Time to animate the view in milliseconds, zero to disable");

  prop = RNA_def_property(srna, "rotation_angle", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "pad_rot_angle");
  RNA_def_property_range(prop, 0, 90);
  RNA_def_property_ui_text(
      prop, "Rotation Angle", "Rotation step for numerical pad keys (2 4 6 8)");

  /* 3D transform widget */
  prop = RNA_def_property(srna, "show_gizmo", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gizmo_flag", USER_GIZMO_DRAW);
  RNA_def_property_ui_text(prop, "Gizmos", "Use transform gizmos by default");
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  prop = RNA_def_property(srna, "gizmo_size", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, NULL, "gizmo_size");
  RNA_def_property_range(prop, 10, 200);
  RNA_def_property_ui_text(prop, "Gizmo Size", "Diameter of the gizmo");
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  prop = RNA_def_property(srna, "gizmo_size_navigate_v3d", PROP_INT, PROP_PIXEL);
  RNA_def_property_range(prop, 30, 200);
  RNA_def_property_ui_text(prop, "Navigate Gizmo Size", "The Navigate Gizmo size");
  RNA_def_property_update(prop, 0, "rna_userdef_gizmo_update");

  /* Lookdev */
  prop = RNA_def_property(srna, "lookdev_sphere_size", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, NULL, "lookdev_sphere_size");
  RNA_def_property_range(prop, 50, 400);
  RNA_def_property_ui_text(prop, "HDRI Preview Size", "Diameter of the HDRI preview spheres");
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  /* View2D Grid Displays */
  prop = RNA_def_property(srna, "view2d_grid_spacing_min", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, NULL, "v2d_min_gridsize");
  RNA_def_property_range(
      prop, 1, 500); /* XXX: perhaps the lower range should only go down to 5? */
  RNA_def_property_ui_text(prop,
                           "2D View Minimum Grid Spacing",
                           "Minimum number of pixels between each gridline in 2D Viewports");
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  /* TODO: add a setter for this, so that we can bump up the minimum size as necessary... */
  prop = RNA_def_property(srna, "timecode_style", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, timecode_styles);
  RNA_def_property_enum_sdna(prop, NULL, "timecode_style");
  RNA_def_property_enum_funcs(prop, NULL, "rna_userdef_timecode_style_set", NULL);
  RNA_def_property_ui_text(
      prop,
      "TimeCode Style",
      "Format of Time Codes displayed when not displaying timing in terms of frames");
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  prop = RNA_def_property(srna, "view_frame_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, zoom_frame_modes);
  RNA_def_property_enum_sdna(prop, NULL, "view_frame_type");
  RNA_def_property_ui_text(
      prop, "Zoom to Frame Type", "How zooming to frame focuses around current frame");

  prop = RNA_def_property(srna, "view_frame_keyframes", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 1, 500);
  RNA_def_property_ui_text(prop, "Zoom Keyframes", "Keyframes around cursor that we zoom around");

  prop = RNA_def_property(srna, "view_frame_seconds", PROP_FLOAT, PROP_TIME);
  RNA_def_property_range(prop, 0.0, 10000.0);
  RNA_def_property_ui_text(prop, "Zoom Seconds", "Seconds around cursor that we zoom around");

  /* Text. */

  prop = RNA_def_property(srna, "use_text_antialiasing", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "text_render", USER_TEXT_DISABLE_AA);
  RNA_def_property_ui_text(
      prop, "Text Anti-Aliasing", "Smooth jagged edges of user interface text");
  RNA_def_property_update(prop, 0, "rna_userdef_text_update");

  prop = RNA_def_property(srna, "text_hinting", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "text_render");
  RNA_def_property_enum_items(prop, text_hinting_items);
  RNA_def_property_ui_text(
      prop, "Text Hinting", "Method for making user interface text render sharp");
  RNA_def_property_update(prop, 0, "rna_userdef_text_update");

  prop = RNA_def_property(srna, "font_path_ui", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_string_sdna(prop, NULL, "font_path_ui");
  RNA_def_property_ui_text(prop, "Interface Font", "Path to interface font");
  RNA_def_property_update(prop, NC_WINDOW, "rna_userdef_font_update");

  prop = RNA_def_property(srna, "font_path_ui_mono", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_string_sdna(prop, NULL, "font_path_ui_mono");
  RNA_def_property_ui_text(prop, "Monospaced Font", "Path to interface monospaced Font");
  RNA_def_property_update(prop, NC_WINDOW, "rna_userdef_font_update");

  /* Language. */

  prop = RNA_def_property(srna, "language", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_language_default_items);
#  ifdef WITH_INTERNATIONAL
  RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_lang_enum_properties_itemf");
#  endif
  RNA_def_property_ui_text(prop, "Language", "Language used for translation");
  RNA_def_property_update(prop, NC_WINDOW, "rna_userdef_language_update");

  prop = RNA_def_property(srna, "use_translate_tooltips", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "transopts", USER_TR_TOOLTIPS);
  RNA_def_property_ui_text(prop,
                           "Translate Tooltips",
                           "Translate the descriptions when hovering UI elements (recommended)");
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  prop = RNA_def_property(srna, "use_translate_interface", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "transopts", USER_TR_IFACE);
  RNA_def_property_ui_text(
      prop,
      "Translate Interface",
      "Translate all labels in menus, buttons and panels "
      "(note that this might make it hard to follow tutorials or the manual)");
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  prop = RNA_def_property(srna, "use_translate_new_dataname", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "transopts", USER_TR_NEWDATANAME);
  RNA_def_property_ui_text(prop,
                           "Translate New Names",
                           "Translate the names of new data-blocks (objects, materials...)");
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  /* Status-bar. */

  prop = RNA_def_property(srna, "show_statusbar_memory", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "statusbar_flag", STATUSBAR_SHOW_MEMORY);
  RNA_def_property_ui_text(prop, "Show Memory", "Show Blender memory usage");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_INFO, "rna_userdef_update");

  prop = RNA_def_property(srna, "show_statusbar_vram", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "statusbar_flag", STATUSBAR_SHOW_VRAM);
  RNA_def_property_ui_text(prop, "Show VRAM", "Show GPU video memory usage");
  RNA_def_property_editable_func(prop, "rna_show_statusbar_vram_editable");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_INFO, "rna_userdef_update");

  prop = RNA_def_property(srna, "show_statusbar_version", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "statusbar_flag", STATUSBAR_SHOW_VERSION);
  RNA_def_property_ui_text(prop, "Show Version", "Show Blender version string");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_INFO, "rna_userdef_update");

  prop = RNA_def_property(srna, "show_statusbar_stats", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "statusbar_flag", STATUSBAR_SHOW_STATS);
  RNA_def_property_ui_text(prop, "Show Statistics", "Show scene statistics");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_INFO, "rna_userdef_update");
}

static void rna_def_userdef_edit(BlenderRNA *brna)
{
  PropertyRNA *prop;
  StructRNA *srna;

  static const EnumPropertyItem auto_key_modes[] = {
      {AUTOKEY_MODE_NORMAL, "ADD_REPLACE_KEYS", 0, "Add/Replace", ""},
      {AUTOKEY_MODE_EDITKEYS, "REPLACE_KEYS", 0, "Replace", ""},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem material_link_items[] = {
      {0,
       "OBDATA",
       0,
       "Object Data",
       "Toggle whether the material is linked to object data or the object block"},
      {USER_MAT_ON_OB,
       "OBJECT",
       0,
       "Object",
       "Toggle whether the material is linked to object data or the object block"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem object_align_items[] = {
      {0, "WORLD", 0, "World", "Align newly added objects to the world coordinate system"},
      {USER_ADD_VIEWALIGNED,
       "VIEW",
       0,
       "View",
       "Align newly added objects to the active 3D view orientation"},
      {USER_ADD_CURSORALIGNED,
       "CURSOR",
       0,
       "3D Cursor",
       "Align newly added objects to the 3D Cursor's rotation"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "PreferencesEdit", NULL);
  RNA_def_struct_sdna(srna, "UserDef");
  RNA_def_struct_nested(brna, srna, "Preferences");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Edit Methods", "Settings for interacting with Blender data");

  /* Edit Methods */

  prop = RNA_def_property(srna, "material_link", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
  RNA_def_property_enum_items(prop, material_link_items);
  RNA_def_property_ui_text(
      prop,
      "Material Link To",
      "Toggle whether the material is linked to object data or the object block");

  prop = RNA_def_property(srna, "object_align", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
  RNA_def_property_enum_items(prop, object_align_items);
  RNA_def_property_ui_text(
      prop, "Align Object To", "The default alignment for objects added from a 3D viewport menu");

  prop = RNA_def_property(srna, "use_enter_edit_mode", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", USER_ADD_EDITMODE);
  RNA_def_property_ui_text(
      prop, "Enter Edit Mode", "Enter edit mode automatically after adding a new object");

  prop = RNA_def_property(srna, "collection_instance_empty_size", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.001f, FLT_MAX);
  RNA_def_property_ui_text(prop,
                           "Collection Instance Empty Size",
                           "Display size of the empty when new collection instances are created");

  /* Text Editor */

  prop = RNA_def_property(srna, "use_text_edit_auto_close", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "text_flag", USER_TEXT_EDIT_AUTO_CLOSE);
  RNA_def_property_ui_text(
      prop,
      "Auto Close Character Pairs",
      "Automatically close relevant character pairs when typing in the text editor");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, NULL);

  /* Undo */

  prop = RNA_def_property(srna, "undo_steps", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "undosteps");
  RNA_def_property_range(prop, 0, 256);
  RNA_def_property_int_funcs(prop, NULL, "rna_userdef_undo_steps_set", NULL);
  RNA_def_property_ui_text(
      prop, "Undo Steps", "Number of undo steps available (smaller values conserve memory)");

  prop = RNA_def_property(srna, "undo_memory_limit", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "undomemory");
  RNA_def_property_range(prop, 0, max_memory_in_megabytes_int());
  RNA_def_property_ui_text(
      prop, "Undo Memory Size", "Maximum memory usage in megabytes (0 means unlimited)");

  prop = RNA_def_property(srna, "use_global_undo", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "uiflag", USER_GLOBALUNDO);
  RNA_def_property_ui_text(
      prop,
      "Global Undo",
      "Global undo works by keeping a full copy of the file itself in memory, "
      "so takes extra memory");

  /* auto keyframing */
  prop = RNA_def_property(srna, "use_auto_keying", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "autokey_mode", AUTOKEY_ON);
  RNA_def_property_ui_text(prop,
                           "Auto Keying Enable",
                           "Automatic keyframe insertion for Objects and Bones "
                           "(default setting used for new Scenes)");

  prop = RNA_def_property(srna, "auto_keying_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, auto_key_modes);
  RNA_def_property_enum_funcs(
      prop, "rna_userdef_autokeymode_get", "rna_userdef_autokeymode_set", NULL);
  RNA_def_property_ui_text(prop,
                           "Auto Keying Mode",
                           "Mode of automatic keyframe insertion for Objects and Bones "
                           "(default setting used for new Scenes)");

  prop = RNA_def_property(srna, "use_keyframe_insert_available", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "autokey_flag", AUTOKEY_FLAG_INSERTAVAIL);
  RNA_def_property_ui_text(prop,
                           "Auto Keyframe Insert Available",
                           "Automatic keyframe insertion in available F-Curves");

  prop = RNA_def_property(srna, "use_auto_keying_warning", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "autokey_flag", AUTOKEY_FLAG_NOWARNING);
  RNA_def_property_ui_text(
      prop,
      "Show Auto Keying Warning",
      "Show warning indicators when transforming objects and bones if auto keying is enabled");

  /* keyframing settings */
  prop = RNA_def_property(srna, "use_keyframe_insert_needed", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "autokey_flag", AUTOKEY_FLAG_INSERTNEEDED);
  RNA_def_property_ui_text(
      prop, "Keyframe Insert Needed", "Keyframe insertion only when keyframe needed");

  prop = RNA_def_property(srna, "use_visual_keying", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "autokey_flag", AUTOKEY_FLAG_AUTOMATKEY);
  RNA_def_property_ui_text(
      prop, "Visual Keying", "Use Visual keying automatically for constrained objects");

  prop = RNA_def_property(srna, "use_insertkey_xyz_to_rgb", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "autokey_flag", AUTOKEY_FLAG_XYZ2RGB);
  RNA_def_property_ui_text(
      prop,
      "New F-Curve Colors - XYZ to RGB",
      "Color for newly added transformation F-Curves (Location, Rotation, Scale) "
      "and also Color is based on the transform axis");

  prop = RNA_def_property(srna, "use_anim_channel_group_colors", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "animation_flag", USER_ANIM_SHOW_CHANNEL_GROUP_COLORS);
  RNA_def_property_ui_text(
      prop,
      "Channel Group Colors",
      "Use animation channel group colors; generally this is used to show bone group colors");
  RNA_def_property_update(prop, 0, "rna_userdef_anim_update");

  prop = RNA_def_property(srna, "fcurve_new_auto_smoothing", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_fcurve_auto_smoothing_items);
  RNA_def_property_enum_sdna(prop, NULL, "auto_smoothing_new");
  RNA_def_property_ui_text(prop,
                           "New Curve Smoothing Mode",
                           "Auto Handle Smoothing mode used for newly added F-Curves");

  prop = RNA_def_property(srna, "keyframe_new_interpolation_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_beztriple_interpolation_mode_items);
  RNA_def_property_enum_sdna(prop, NULL, "ipo_new");
  RNA_def_property_ui_text(prop,
                           "New Interpolation Type",
                           "Interpolation mode used for first keyframe on newly added F-Curves "
                           "(subsequent keyframes take interpolation from preceding keyframe)");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ACTION);

  prop = RNA_def_property(srna, "keyframe_new_handle_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_keyframe_handle_type_items);
  RNA_def_property_enum_sdna(prop, NULL, "keyhandles_new");
  RNA_def_property_ui_text(prop, "New Handles Type", "Handle type for handles of new keyframes");

  /* frame numbers */
  prop = RNA_def_property(srna, "use_negative_frames", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", USER_NONEGFRAMES);
  RNA_def_property_ui_text(prop,
                           "Allow Negative Frames",
                           "Current frame number can be manually set to a negative value");

  /* fcurve opacity */
  prop = RNA_def_property(srna, "fcurve_unselected_alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "fcu_inactive_alpha");
  RNA_def_property_range(prop, 0.001f, 1.0f);
  RNA_def_property_ui_text(prop,
                           "Unselected F-Curve Opacity",
                           "The opacity of unselected F-Curves against the "
                           "background of the Graph Editor");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, NULL);

  /* grease pencil */
  prop = RNA_def_property(srna, "grease_pencil_manhattan_distance", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, NULL, "gp_manhattandist");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop,
                           "Grease Pencil Manhattan Distance",
                           "Pixels moved by mouse per axis when drawing stroke");

  prop = RNA_def_property(srna, "grease_pencil_euclidean_distance", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, NULL, "gp_euclideandist");
  RNA_def_property_range(prop, 0, 100);
  RNA_def_property_ui_text(prop,
                           "Grease Pencil Euclidean Distance",
                           "Distance moved by mouse when drawing stroke to include");

  prop = RNA_def_property(srna, "grease_pencil_eraser_radius", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, NULL, "gp_eraser");
  RNA_def_property_range(prop, 1, 500);
  RNA_def_property_ui_text(prop, "Grease Pencil Eraser Radius", "Radius of eraser 'brush'");

  prop = RNA_def_property(srna, "grease_pencil_default_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "gpencil_new_layer_col");
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Annotation Default Color", "Color of new annotation layers");

  /* sculpt and paint */

  prop = RNA_def_property(srna, "sculpt_paint_overlay_color", PROP_FLOAT, PROP_COLOR_GAMMA);
  RNA_def_property_float_sdna(prop, NULL, "sculpt_paint_overlay_col");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Sculpt/Paint Overlay Color", "Color of texture overlay");

  /* duplication linking */
  prop = RNA_def_property(srna, "use_duplicate_mesh", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dupflag", USER_DUP_MESH);
  RNA_def_property_ui_text(
      prop, "Duplicate Mesh", "Causes mesh data to be duplicated with the object");

  prop = RNA_def_property(srna, "use_duplicate_surface", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dupflag", USER_DUP_SURF);
  RNA_def_property_ui_text(
      prop, "Duplicate Surface", "Causes surface data to be duplicated with the object");

  prop = RNA_def_property(srna, "use_duplicate_curve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dupflag", USER_DUP_CURVE);
  RNA_def_property_ui_text(
      prop, "Duplicate Curve", "Causes curve data to be duplicated with the object");

  prop = RNA_def_property(srna, "use_duplicate_lattice", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dupflag", USER_DUP_LATTICE);
  RNA_def_property_ui_text(
      prop, "Duplicate Lattice", "Causes lattice data to be duplicated with the object");

  prop = RNA_def_property(srna, "use_duplicate_text", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dupflag", USER_DUP_FONT);
  RNA_def_property_ui_text(
      prop, "Duplicate Text", "Causes text data to be duplicated with the object");

  prop = RNA_def_property(srna, "use_duplicate_metaball", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dupflag", USER_DUP_MBALL);
  RNA_def_property_ui_text(
      prop, "Duplicate Metaball", "Causes metaball data to be duplicated with the object");

  prop = RNA_def_property(srna, "use_duplicate_armature", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dupflag", USER_DUP_ARM);
  RNA_def_property_ui_text(
      prop, "Duplicate Armature", "Causes armature data to be duplicated with the object");

  prop = RNA_def_property(srna, "use_duplicate_camera", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dupflag", USER_DUP_CAMERA);
  RNA_def_property_ui_text(
      prop, "Duplicate Camera", "Causes camera data to be duplicated with the object");

  prop = RNA_def_property(srna, "use_duplicate_speaker", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dupflag", USER_DUP_SPEAKER);
  RNA_def_property_ui_text(
      prop, "Duplicate Speaker", "Causes speaker data to be duplicated with the object");

  prop = RNA_def_property(srna, "use_duplicate_light", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dupflag", USER_DUP_LAMP);
  RNA_def_property_ui_text(
      prop, "Duplicate Light", "Causes light data to be duplicated with the object");

  prop = RNA_def_property(srna, "use_duplicate_material", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dupflag", USER_DUP_MAT);
  RNA_def_property_ui_text(
      prop, "Duplicate Material", "Causes material data to be duplicated with the object");

  /* Not implemented, keep because this is useful functionality. */
#  if 0
  prop = RNA_def_property(srna, "use_duplicate_texture", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dupflag", USER_DUP_TEX);
  RNA_def_property_ui_text(
      prop, "Duplicate Texture", "Causes texture data to be duplicated with the object");

  prop = RNA_def_property(srna, "use_duplicate_fcurve", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dupflag", USER_DUP_FCURVE);
  RNA_def_property_ui_text(
      prop, "Duplicate F-Curve", "Causes F-Curve data to be duplicated with the object");
#  endif

  prop = RNA_def_property(srna, "use_duplicate_action", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dupflag", USER_DUP_ACT);
  RNA_def_property_ui_text(
      prop, "Duplicate Action", "Causes actions to be duplicated with the data-blocks");

  prop = RNA_def_property(srna, "use_duplicate_particle", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dupflag", USER_DUP_PSYS);
  RNA_def_property_ui_text(
      prop, "Duplicate Particle", "Causes particle systems to be duplicated with the object");

  prop = RNA_def_property(srna, "use_duplicate_lightprobe", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dupflag", USER_DUP_LIGHTPROBE);
  RNA_def_property_ui_text(
      prop, "Duplicate Light Probe", "Causes light probe data to be duplicated with the object");

  prop = RNA_def_property(srna, "use_duplicate_grease_pencil", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dupflag", USER_DUP_GPENCIL);
  RNA_def_property_ui_text(
      prop, "Duplicate GPencil", "Causes grease pencil data to be duplicated with the object");

  prop = RNA_def_property(srna, "use_duplicate_curves", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dupflag", USER_DUP_CURVES);
  RNA_def_property_ui_text(
      prop, "Duplicate Curves", "Causes curves data to be duplicated with the object");

  prop = RNA_def_property(srna, "use_duplicate_pointcloud", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dupflag", USER_DUP_POINTCLOUD);
  RNA_def_property_ui_text(
      prop, "Duplicate Point Cloud", "Causes point cloud data to be duplicated with the object");

  prop = RNA_def_property(srna, "use_duplicate_volume", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "dupflag", USER_DUP_VOLUME);
  RNA_def_property_ui_text(
      prop, "Duplicate Volume", "Causes volume data to be duplicated with the object");

  /* Currently only used for insert offset (aka auto-offset),
   * maybe also be useful for later stuff though. */
  prop = RNA_def_property(srna, "node_margin", PROP_INT, PROP_PIXEL);
  RNA_def_property_int_sdna(prop, NULL, "node_margin");
  RNA_def_property_ui_text(
      prop, "Auto-offset Margin", "Minimum distance between nodes for Auto-offsetting nodes");
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  /* cursor */
  prop = RNA_def_property(srna, "use_cursor_lock_adjust", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "uiflag", USER_LOCK_CURSOR_ADJUST);
  RNA_def_property_ui_text(
      prop,
      "Cursor Lock Adjust",
      "Place the cursor without 'jumping' to the new location (when lock-to-cursor is used)");

  prop = RNA_def_property(srna, "use_mouse_depth_cursor", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "uiflag", USER_DEPTH_CURSOR);
  RNA_def_property_ui_text(
      prop, "Cursor Surface Project", "Use the surface depth for cursor placement");
}

static void rna_def_userdef_system(BlenderRNA *brna)
{
  PropertyRNA *prop;
  StructRNA *srna;

  static const EnumPropertyItem gl_texture_clamp_items[] = {
      {0, "CLAMP_OFF", 0, "Off", ""},
      {8192, "CLAMP_8192", 0, "8192", ""},
      {4096, "CLAMP_4096", 0, "4096", ""},
      {2048, "CLAMP_2048", 0, "2048", ""},
      {1024, "CLAMP_1024", 0, "1024", ""},
      {512, "CLAMP_512", 0, "512", ""},
      {256, "CLAMP_256", 0, "256", ""},
      {128, "CLAMP_128", 0, "128", ""},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem anisotropic_items[] = {
      {1, "FILTER_0", 0, "Off", ""},
      {2, "FILTER_2", 0, "2x", ""},
      {4, "FILTER_4", 0, "4x", ""},
      {8, "FILTER_8", 0, "8x", ""},
      {16, "FILTER_16", 0, "16x", ""},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem audio_mixing_samples_items[] = {
      {256, "SAMPLES_256", 0, "256 Samples", "Set audio mixing buffer size to 256 samples"},
      {512, "SAMPLES_512", 0, "512 Samples", "Set audio mixing buffer size to 512 samples"},
      {1024, "SAMPLES_1024", 0, "1024 Samples", "Set audio mixing buffer size to 1024 samples"},
      {2048, "SAMPLES_2048", 0, "2048 Samples", "Set audio mixing buffer size to 2048 samples"},
      {4096, "SAMPLES_4096", 0, "4096 Samples", "Set audio mixing buffer size to 4096 samples"},
      {8192, "SAMPLES_8192", 0, "8192 Samples", "Set audio mixing buffer size to 8192 samples"},
      {16384,
       "SAMPLES_16384",
       0,
       "16384 Samples",
       "Set audio mixing buffer size to 16384 samples"},
      {32768,
       "SAMPLES_32768",
       0,
       "32768 Samples",
       "Set audio mixing buffer size to 32768 samples"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem audio_rate_items[] = {
#  if 0
    {8000, "RATE_8000", 0, "8 kHz", "Set audio sampling rate to 8000 samples per second"},
    {11025, "RATE_11025", 0, "11.025 kHz", "Set audio sampling rate to 11025 samples per second"},
    {16000, "RATE_16000", 0, "16 kHz", "Set audio sampling rate to 16000 samples per second"},
    {22050, "RATE_22050", 0, "22.05 kHz", "Set audio sampling rate to 22050 samples per second"},
    {32000, "RATE_32000", 0, "32 kHz", "Set audio sampling rate to 32000 samples per second"},
#  endif
    {44100, "RATE_44100", 0, "44.1 kHz", "Set audio sampling rate to 44100 samples per second"},
    {48000, "RATE_48000", 0, "48 kHz", "Set audio sampling rate to 48000 samples per second"},
#  if 0
    {88200, "RATE_88200", 0, "88.2 kHz", "Set audio sampling rate to 88200 samples per second"},
#  endif
    {96000, "RATE_96000", 0, "96 kHz", "Set audio sampling rate to 96000 samples per second"},
    {192000, "RATE_192000", 0, "192 kHz", "Set audio sampling rate to 192000 samples per second"},
    {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem audio_format_items[] = {
      {0x01, "U8", 0, "8-bit Unsigned", "Set audio sample format to 8-bit unsigned integer"},
      {0x12, "S16", 0, "16-bit Signed", "Set audio sample format to 16-bit signed integer"},
      {0x13, "S24", 0, "24-bit Signed", "Set audio sample format to 24-bit signed integer"},
      {0x14, "S32", 0, "32-bit Signed", "Set audio sample format to 32-bit signed integer"},
      {0x24, "FLOAT", 0, "32-bit Float", "Set audio sample format to 32-bit float"},
      {0x28, "DOUBLE", 0, "64-bit Float", "Set audio sample format to 64-bit float"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem audio_channel_items[] = {
      {1, "MONO", 0, "Mono", "Set audio channels to mono"},
      {2, "STEREO", 0, "Stereo", "Set audio channels to stereo"},
      {4, "SURROUND4", 0, "4 Channels", "Set audio channels to 4 channels"},
      {6, "SURROUND51", 0, "5.1 Surround", "Set audio channels to 5.1 surround sound"},
      {8, "SURROUND71", 0, "7.1 Surround", "Set audio channels to 7.1 surround sound"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem image_draw_methods[] = {
      {IMAGE_DRAW_METHOD_AUTO,
       "AUTO",
       0,
       "Automatic",
       "Automatically choose method based on GPU and image"},
      {IMAGE_DRAW_METHOD_2DTEXTURE,
       "2DTEXTURE",
       0,
       "2D Texture",
       "Use CPU for display transform and display image with 2D texture"},
      {IMAGE_DRAW_METHOD_GLSL,
       "GLSL",
       0,
       "GLSL",
       "Use GLSL shaders for display transform and display image with 2D texture"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem seq_disk_cache_compression_levels[] = {
      {USER_SEQ_DISK_CACHE_COMPRESSION_NONE,
       "NONE",
       0,
       "None",
       "Requires fast storage, but uses minimum CPU resources"},
      {USER_SEQ_DISK_CACHE_COMPRESSION_LOW,
       "LOW",
       0,
       "Low",
       "Doesn't require fast storage and uses less CPU resources"},
      {USER_SEQ_DISK_CACHE_COMPRESSION_HIGH,
       "HIGH",
       0,
       "High",
       "Works on slower storage devices and uses most CPU resources"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem seq_proxy_setup_options[] = {
      {USER_SEQ_PROXY_SETUP_MANUAL, "MANUAL", 0, "Manual", "Set up proxies manually"},
      {USER_SEQ_PROXY_SETUP_AUTOMATIC,
       "AUTOMATIC",
       0,
       "Automatic",
       "Build proxies for added movie and image strips in each preview size"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "PreferencesSystem", NULL);
  RNA_def_struct_sdna(srna, "UserDef");
  RNA_def_struct_nested(brna, srna, "Preferences");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "System & OpenGL", "Graphics driver and operating system settings");

  /* UI settings. */

  prop = RNA_def_property(srna, "ui_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_sdna(prop, NULL, "dpi_fac");
  RNA_def_property_ui_text(
      prop,
      "UI Scale",
      "Size multiplier to use when displaying custom user interface elements, so that "
      "they are scaled correctly on screens with different DPI. This value is based "
      "on operating system DPI settings and Blender display scale");

  prop = RNA_def_property(srna, "ui_line_width", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_sdna(prop, NULL, "pixelsize");
  RNA_def_property_ui_text(
      prop,
      "UI Line Width",
      "Suggested line thickness and point size in pixels, for add-ons displaying custom "
      "user interface elements, based on operating system settings and Blender UI scale");

  prop = RNA_def_property(srna, "dpi", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "pixel_size", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_sdna(prop, NULL, "pixelsize");

  /* Memory */

  prop = RNA_def_property(srna, "memory_cache_limit", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "memcachelimit");
  RNA_def_property_range(prop, 0, max_memory_in_megabytes_int());
  RNA_def_property_ui_text(prop, "Memory Cache Limit", "Memory cache limit (in megabytes)");
  RNA_def_property_update(prop, 0, "rna_Userdef_memcache_update");

  /* Sequencer disk cache */

  prop = RNA_def_property(srna, "use_sequencer_disk_cache", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, NULL, "sequencer_disk_cache_flag", SEQ_CACHE_DISK_CACHE_ENABLE);
  RNA_def_property_ui_text(prop, "Use Disk Cache", "Store cached images to disk");

  prop = RNA_def_property(srna, "sequencer_disk_cache_dir", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_sdna(prop, NULL, "sequencer_disk_cache_dir");
  RNA_def_property_update(prop, 0, "rna_Userdef_disk_cache_dir_update");
  RNA_def_property_ui_text(prop, "Disk Cache Directory", "Override default directory");

  prop = RNA_def_property(srna, "sequencer_disk_cache_size_limit", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "sequencer_disk_cache_size_limit");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_ui_text(prop, "Disk Cache Limit", "Disk cache limit (in gigabytes)");

  prop = RNA_def_property(srna, "sequencer_disk_cache_compression", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, seq_disk_cache_compression_levels);
  RNA_def_property_enum_sdna(prop, NULL, "sequencer_disk_cache_compression");
  RNA_def_property_ui_text(
      prop,
      "Disk Cache Compression Level",
      "Smaller compression will result in larger files, but less decoding overhead");

  /* Sequencer proxy setup */

  prop = RNA_def_property(srna, "sequencer_proxy_setup", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, seq_proxy_setup_options);
  RNA_def_property_enum_sdna(prop, NULL, "sequencer_proxy_setup");
  RNA_def_property_ui_text(prop, "Proxy Setup", "When and how proxies are created");

  prop = RNA_def_property(srna, "scrollback", PROP_INT, PROP_UNSIGNED);
  RNA_def_property_int_sdna(prop, NULL, "scrollback");
  RNA_def_property_range(prop, 32, 32768);
  RNA_def_property_ui_text(
      prop, "Scrollback", "Maximum number of lines to store for the console buffer");

  /* OpenGL */

  /* Viewport anti-aliasing */
  prop = RNA_def_property(srna, "use_overlay_smooth_wire", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gpu_flag", USER_GPU_FLAG_OVERLAY_SMOOTH_WIRE);
  RNA_def_property_ui_text(
      prop, "Overlay Smooth Wires", "Enable overlay smooth wires, reducing aliasing");
  RNA_def_property_update(prop, 0, "rna_userdef_dpi_update");

  prop = RNA_def_property(srna, "use_edit_mode_smooth_wire", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(
      prop, NULL, "gpu_flag", USER_GPU_FLAG_NO_EDIT_MODE_SMOOTH_WIRE);
  RNA_def_property_ui_text(
      prop,
      "Edit Mode Smooth Wires",
      "Enable edit mode edge smoothing, reducing aliasing (requires restart)");
  RNA_def_property_update(prop, 0, "rna_userdef_dpi_update");

  prop = RNA_def_property(srna, "use_region_overlap", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "uiflag2", USER_REGION_OVERLAP);
  RNA_def_property_ui_text(
      prop, "Region Overlap", "Display tool/property regions over the main region");
  RNA_def_property_update(prop, 0, "rna_userdef_dpi_update");

  prop = RNA_def_property(srna, "viewport_aa", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_userdef_viewport_aa_items);
  RNA_def_property_ui_text(
      prop, "Viewport Anti-Aliasing", "Method of anti-aliasing in 3d viewport");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  prop = RNA_def_property(srna, "solid_lights", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "light_param", "");
  RNA_def_property_struct_type(prop, "UserSolidLight");
  RNA_def_property_ui_text(
      prop, "Solid Lights", "Lights used to display objects in solid shading mode");

  prop = RNA_def_property(srna, "light_ambient", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, NULL, "light_ambient");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(
      prop, "Ambient Color", "Color of the ambient light that uniformly lit the scene");
  RNA_def_property_update(prop, 0, "rna_UserDef_viewport_lights_update");

  prop = RNA_def_property(srna, "use_studio_light_edit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "edit_studio_light", 1);
  RNA_def_property_ui_text(
      prop, "Edit Studio Light", "View the result of the studio light editor in the viewport");
  RNA_def_property_update(prop, 0, "rna_UserDef_viewport_lights_update");

  prop = RNA_def_property(srna, "gl_clip_alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "glalphaclip");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Clip Alpha", "Clip alpha below this threshold in the 3D textured view");
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  /* Textures */

  prop = RNA_def_property(srna, "image_draw_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, image_draw_methods);
  RNA_def_property_enum_sdna(prop, NULL, "image_draw_method");
  RNA_def_property_ui_text(
      prop, "Image Display Method", "Method used for displaying images on the screen");
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  prop = RNA_def_property(srna, "anisotropic_filter", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "anisotropic_filter");
  RNA_def_property_enum_items(prop, anisotropic_items);
  RNA_def_property_enum_default(prop, 1);
  RNA_def_property_ui_text(prop, "Anisotropic Filtering", "Quality of anisotropic filtering");
  RNA_def_property_update(prop, 0, "rna_userdef_anisotropic_update");

  prop = RNA_def_property(srna, "gl_texture_limit", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "glreslimit");
  RNA_def_property_enum_items(prop, gl_texture_clamp_items);
  RNA_def_property_ui_text(
      prop, "GL Texture Limit", "Limit the texture size to save graphics memory");
  RNA_def_property_update(prop, 0, "rna_userdef_gl_texture_limit_update");

  prop = RNA_def_property(srna, "texture_time_out", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "textimeout");
  RNA_def_property_range(prop, 0, 3600);
  RNA_def_property_ui_text(
      prop,
      "Texture Time Out",
      "Time since last access of a GL texture in seconds after which it is freed "
      "(set to 0 to keep textures allocated)");

  prop = RNA_def_property(srna, "texture_collection_rate", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "texcollectrate");
  RNA_def_property_range(prop, 1, 3600);
  RNA_def_property_ui_text(
      prop,
      "Texture Collection Rate",
      "Number of seconds between each run of the GL texture garbage collector");

  prop = RNA_def_property(srna, "vbo_time_out", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "vbotimeout");
  RNA_def_property_range(prop, 0, 3600);
  RNA_def_property_ui_text(
      prop,
      "VBO Time Out",
      "Time since last access of a GL Vertex buffer object in seconds after which it is freed "
      "(set to 0 to keep vbo allocated)");

  prop = RNA_def_property(srna, "vbo_collection_rate", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "vbocollectrate");
  RNA_def_property_range(prop, 1, 3600);
  RNA_def_property_ui_text(
      prop,
      "VBO Collection Rate",
      "Number of seconds between each run of the GL Vertex buffer object garbage collector");

  /* Select */

  prop = RNA_def_property(srna, "use_select_pick_depth", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "gpu_flag", USER_GPU_FLAG_NO_DEPT_PICK);
  RNA_def_property_ui_text(prop,
                           "GPU Depth Picking",
                           "When making a selection in 3D View, use the GPU depth buffer to "
                           "ensure the frontmost object is selected first");

  /* GPU subdivision evaluation. */

  prop = RNA_def_property(srna, "use_gpu_subdivision", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gpu_flag", USER_GPU_FLAG_SUBDIVISION_EVALUATION);
  RNA_def_property_ui_text(prop,
                           "GPU Subdivision",
                           "Enable GPU acceleration for evaluating the last subdivision surface "
                           "modifiers in the stack");
  RNA_def_property_update(prop, 0, "rna_UserDef_subdivision_update");

  /* Audio */

  prop = RNA_def_property(srna, "audio_mixing_buffer", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "mixbufsize");
  RNA_def_property_enum_items(prop, audio_mixing_samples_items);
  RNA_def_property_ui_text(
      prop, "Audio Mixing Buffer", "Number of samples used by the audio mixing buffer");
  RNA_def_property_update(prop, 0, "rna_UserDef_audio_update");

  prop = RNA_def_property(srna, "audio_device", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "audiodevice");
  RNA_def_property_enum_items(prop, audio_device_items);
  RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_userdef_audio_device_itemf");
  RNA_def_property_ui_text(prop, "Audio Device", "Audio output device");
  RNA_def_property_update(prop, 0, "rna_UserDef_audio_update");

  prop = RNA_def_property(srna, "audio_sample_rate", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "audiorate");
  RNA_def_property_enum_items(prop, audio_rate_items);
  RNA_def_property_ui_text(prop, "Audio Sample Rate", "Audio sample rate");
  RNA_def_property_update(prop, 0, "rna_UserDef_audio_update");

  prop = RNA_def_property(srna, "audio_sample_format", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "audioformat");
  RNA_def_property_enum_items(prop, audio_format_items);
  RNA_def_property_ui_text(prop, "Audio Sample Format", "Audio sample format");
  RNA_def_property_update(prop, 0, "rna_UserDef_audio_update");

  prop = RNA_def_property(srna, "audio_channels", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "audiochannels");
  RNA_def_property_enum_items(prop, audio_channel_items);
  RNA_def_property_ui_text(prop, "Audio Channels", "Audio channel count");
  RNA_def_property_update(prop, 0, "rna_UserDef_audio_update");

#  ifdef WITH_OPENSUBDIV
  prop = RNA_def_property(srna, "opensubdiv_compute_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_flag(prop, PROP_ENUM_NO_CONTEXT);
  RNA_def_property_enum_sdna(prop, NULL, "opensubdiv_compute_type");
  RNA_def_property_enum_items(prop, opensubdiv_compute_type_items);
  RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_userdef_opensubdiv_compute_type_itemf");
  RNA_def_property_ui_text(
      prop, "OpenSubdiv Compute Type", "Type of computer back-end used with OpenSubdiv");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_PROPERTIES, "rna_userdef_opensubdiv_update");
#  endif

#  ifdef WITH_CYCLES
  prop = RNA_def_property(srna, "legacy_compute_device_type", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "compute_device_type");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  RNA_def_property_ui_text(prop, "Legacy Compute Device Type", "For backwards compatibility only");
#  endif
}

static void rna_def_userdef_input(BlenderRNA *brna)
{
  PropertyRNA *prop;
  StructRNA *srna;

  static const EnumPropertyItem view_rotation_items[] = {
      {0, "TURNTABLE", 0, "Turntable", "Turntable keeps the Z-axis upright while orbiting"},
      {USER_TRACKBALL,
       "TRACKBALL",
       0,
       "Trackball",
       "Trackball allows you to tumble your view at any angle"},
      {0, NULL, 0, NULL, NULL},
  };

#  ifdef WITH_INPUT_NDOF
  static const EnumPropertyItem ndof_view_navigation_items[] = {
      {0, "FREE", 0, "Free", "Use full 6 degrees of freedom by default"},
      {NDOF_MODE_ORBIT, "ORBIT", 0, "Orbit", "Orbit about the view center by default"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem ndof_view_rotation_items[] = {
      {NDOF_TURNTABLE,
       "TURNTABLE",
       0,
       "Turntable",
       "Use turntable style rotation in the viewport"},
      {0, "TRACKBALL", 0, "Trackball", "Use trackball style rotation in the viewport"},
      {0, NULL, 0, NULL, NULL},
  };
#  endif /* WITH_INPUT_NDOF */

  static const EnumPropertyItem tablet_api[] = {
      {USER_TABLET_AUTOMATIC,
       "AUTOMATIC",
       0,
       "Automatic",
       "Automatically choose Wintab or Windows Ink depending on the device"},
      {USER_TABLET_NATIVE,
       "WINDOWS_INK",
       0,
       "Windows Ink",
       "Use native Windows Ink API, for modern tablet and pen devices. Requires Windows 8 or "
       "newer"},
      {USER_TABLET_WINTAB,
       "WINTAB",
       0,
       "Wintab",
       "Use Wintab driver for older tablets and Windows versions"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem view_zoom_styles[] = {
      {USER_ZOOM_CONTINUE,
       "CONTINUE",
       0,
       "Continue",
       "Continuous zooming. The zoom direction and speed depends on how far along the set Zoom "
       "Axis the mouse has moved"},
      {USER_ZOOM_DOLLY,
       "DOLLY",
       0,
       "Dolly",
       "Zoom in and out based on mouse movement along the set Zoom Axis"},
      {USER_ZOOM_SCALE,
       "SCALE",
       0,
       "Scale",
       "Zoom in and out as if you are scaling the view, mouse movements relative to center"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem view_zoom_axes[] = {
      {0, "VERTICAL", 0, "Vertical", "Zoom in and out based on vertical mouse movement"},
      {USER_ZOOM_HORIZ,
       "HORIZONTAL",
       0,
       "Horizontal",
       "Zoom in and out based on horizontal mouse movement"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "PreferencesInput", NULL);
  RNA_def_struct_sdna(srna, "UserDef");
  RNA_def_struct_nested(brna, srna, "Preferences");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Input", "Settings for input devices");

  prop = RNA_def_property(srna, "view_zoom_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "viewzoom");
  RNA_def_property_enum_items(prop, view_zoom_styles);
  RNA_def_property_ui_text(prop, "Zoom Style", "Which style to use for viewport scaling");

  prop = RNA_def_property(srna, "view_zoom_axis", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "uiflag");
  RNA_def_property_enum_items(prop, view_zoom_axes);
  RNA_def_property_ui_text(prop, "Zoom Axis", "Axis of mouse movement to zoom in or out on");

  prop = RNA_def_property(srna, "invert_mouse_zoom", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "uiflag", USER_ZOOM_INVERT);
  RNA_def_property_ui_text(
      prop, "Invert Zoom Direction", "Invert the axis of mouse movement for zooming");

  prop = RNA_def_property(srna, "use_mouse_depth_navigate", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "uiflag", USER_DEPTH_NAVIGATE);
  RNA_def_property_ui_text(
      prop,
      "Auto Depth",
      "Use the depth under the mouse to improve view pan/rotate/zoom functionality");

  /* view zoom */
  prop = RNA_def_property(srna, "use_zoom_to_mouse", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "uiflag", USER_ZOOM_TO_MOUSEPOS);
  RNA_def_property_ui_text(prop,
                           "Zoom to Mouse Position",
                           "Zoom in towards the mouse pointer's position in the 3D view, "
                           "rather than the 2D window center");

  /* view rotation */
  prop = RNA_def_property(srna, "use_auto_perspective", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "uiflag", USER_AUTOPERSP);
  RNA_def_property_ui_text(
      prop,
      "Auto Perspective",
      "Automatically switch between orthographic and perspective when changing "
      "from top/front/side views");

  prop = RNA_def_property(srna, "use_rotate_around_active", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "uiflag", USER_ORBIT_SELECTION);
  RNA_def_property_ui_text(prop, "Orbit Around Selection", "Use selection as the pivot point");

  prop = RNA_def_property(srna, "view_rotate_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
  RNA_def_property_enum_items(prop, view_rotation_items);
  RNA_def_property_ui_text(prop, "Orbit Method", "Orbit method in the viewport");

  prop = RNA_def_property(srna, "use_mouse_continuous", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "uiflag", USER_CONTINUOUS_MOUSE);
  RNA_def_property_ui_text(
      prop,
      "Continuous Grab",
      "Let the mouse wrap around the view boundaries so mouse movements are not limited by the "
      "screen size (used by transform, dragging of UI controls, etc.)");

  prop = RNA_def_property(srna, "use_drag_immediately", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", USER_RELEASECONFIRM);
  RNA_def_property_ui_text(prop,
                           "Release Confirms",
                           "Moving things with a mouse drag confirms when releasing the button");

  prop = RNA_def_property(srna, "use_numeric_input_advanced", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", USER_FLAG_NUMINPUT_ADVANCED);
  RNA_def_property_ui_text(prop,
                           "Default to Advanced Numeric Input",
                           "When entering numbers while transforming, "
                           "default to advanced mode for full math expression evaluation");

  /* View Navigation */
  prop = RNA_def_property(srna, "navigation_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "navigation_mode");
  RNA_def_property_enum_items(prop, rna_enum_navigation_mode_items);
  RNA_def_property_ui_text(prop, "View Navigation", "Which method to use for viewport navigation");

  prop = RNA_def_property(srna, "walk_navigation", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "walk_navigation");
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "WalkNavigation");
  RNA_def_property_ui_text(prop, "Walk Navigation", "Settings for walk navigation mode");

  prop = RNA_def_property(srna, "view_rotate_sensitivity_turntable", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_range(prop, DEG2RADF(0.001f), DEG2RADF(15.0f));
  RNA_def_property_ui_range(prop, DEG2RADF(0.001f), DEG2RADF(15.0f), 1.0f, 2);
  RNA_def_property_ui_text(prop,
                           "Orbit Sensitivity",
                           "Rotation amount per pixel to control how fast the viewport orbits");

  prop = RNA_def_property(srna, "view_rotate_sensitivity_trackball", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.1f, 10.0f);
  RNA_def_property_ui_range(prop, 0.1f, 2.0f, 0.01f, 2);
  RNA_def_property_ui_text(prop, "Orbit Sensitivity", "Scale trackball orbit sensitivity");

  /* Click-drag threshold for tablet & mouse. */
  prop = RNA_def_property(srna, "drag_threshold_mouse", PROP_INT, PROP_PIXEL);
  RNA_def_property_range(prop, 1, 255);
  RNA_def_property_ui_text(prop,
                           "Mouse Drag Threshold",
                           "Number of pixels to drag before a drag event is triggered "
                           "for mouse/track-pad input "
                           "(otherwise click events are detected)");

  prop = RNA_def_property(srna, "drag_threshold_tablet", PROP_INT, PROP_PIXEL);
  RNA_def_property_range(prop, 1, 255);
  RNA_def_property_ui_text(prop,
                           "Tablet Drag Threshold",
                           "Number of pixels to drag before a drag event is triggered "
                           "for tablet input "
                           "(otherwise click events are detected)");

  prop = RNA_def_property(srna, "drag_threshold", PROP_INT, PROP_PIXEL);
  RNA_def_property_range(prop, 1, 255);
  RNA_def_property_ui_text(prop,
                           "Drag Threshold",
                           "Number of pixels to drag before a drag event is triggered "
                           "for keyboard and other non mouse/tablet input "
                           "(otherwise click events are detected)");

  prop = RNA_def_property(srna, "move_threshold", PROP_INT, PROP_PIXEL);
  RNA_def_property_range(prop, 0, 255);
  RNA_def_property_ui_range(prop, 0, 10, 1, -1);
  RNA_def_property_ui_text(prop,
                           "Motion Threshold",
                           "Number of pixels to before the cursor is considered to have moved "
                           "(used for cycling selected items on successive clicks)");

  /* tablet pressure curve */
  prop = RNA_def_property(srna, "pressure_threshold_max", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 0.01f, 3);
  RNA_def_property_ui_text(
      prop, "Max Threshold", "Raw input pressure value that is interpreted as 100% by Blender");

  prop = RNA_def_property(srna, "pressure_softness", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -1.0f, 1.0f, 0.1f, 2);
  RNA_def_property_ui_text(
      prop, "Softness", "Adjusts softness of the low pressure response onset using a gamma curve");

  prop = RNA_def_property(srna, "tablet_api", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, tablet_api);
  RNA_def_property_ui_text(prop,
                           "Tablet API",
                           "Select the tablet API to use for pressure sensitivity (may require "
                           "restarting Blender for changes to take effect)");
  RNA_def_property_update(prop, 0, "rna_userdef_tablet_api_update");

#  ifdef WITH_INPUT_NDOF
  /* 3D mouse settings */
  /* global options */
  prop = RNA_def_property(srna, "ndof_sensitivity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.01f, 40.0f);
  RNA_def_property_ui_text(prop, "Sensitivity", "Overall sensitivity of the 3D Mouse for panning");

  prop = RNA_def_property(srna, "ndof_orbit_sensitivity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.01f, 40.0f);
  RNA_def_property_ui_text(
      prop, "Orbit Sensitivity", "Overall sensitivity of the 3D Mouse for orbiting");

  prop = RNA_def_property(srna, "ndof_deadzone", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(
      prop, "Deadzone", "Threshold of initial movement needed from the device's rest position");
  RNA_def_property_update(prop, 0, "rna_userdef_ndof_deadzone_update");

  prop = RNA_def_property(srna, "ndof_pan_yz_swap_axis", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "ndof_flag", NDOF_PAN_YZ_SWAP_AXIS);
  RNA_def_property_ui_text(
      prop, "Y/Z Swap Axis", "Pan using up/down on the device (otherwise forward/backward)");

  prop = RNA_def_property(srna, "ndof_zoom_invert", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "ndof_flag", NDOF_ZOOM_INVERT);
  RNA_def_property_ui_text(prop, "Invert Zoom", "Zoom using opposite direction");

  /* 3D view */
  prop = RNA_def_property(srna, "ndof_show_guide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "ndof_flag", NDOF_SHOW_GUIDE);

  /* TODO: update description when fly-mode visuals are in place
   * ("projected position in fly mode"). */
  RNA_def_property_ui_text(
      prop, "Show Navigation Guide", "Display the center and axis during rotation");

  /* 3D view */
  prop = RNA_def_property(srna, "ndof_view_navigate_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "ndof_flag");
  RNA_def_property_enum_items(prop, ndof_view_navigation_items);
  RNA_def_property_ui_text(prop, "NDOF View Navigate", "Navigation style in the viewport");

  prop = RNA_def_property(srna, "ndof_view_rotate_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "ndof_flag");
  RNA_def_property_enum_items(prop, ndof_view_rotation_items);
  RNA_def_property_ui_text(prop, "NDOF View Rotation", "Rotation style in the viewport");

  /* 3D view: yaw */
  prop = RNA_def_property(srna, "ndof_rotx_invert_axis", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "ndof_flag", NDOF_ROTX_INVERT_AXIS);
  RNA_def_property_ui_text(prop, "Invert Pitch (X) Axis", "");

  /* 3D view: pitch */
  prop = RNA_def_property(srna, "ndof_roty_invert_axis", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "ndof_flag", NDOF_ROTY_INVERT_AXIS);
  RNA_def_property_ui_text(prop, "Invert Yaw (Y) Axis", "");

  /* 3D view: roll */
  prop = RNA_def_property(srna, "ndof_rotz_invert_axis", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "ndof_flag", NDOF_ROTZ_INVERT_AXIS);
  RNA_def_property_ui_text(prop, "Invert Roll (Z) Axis", "");

  /* 3D view: pan x */
  prop = RNA_def_property(srna, "ndof_panx_invert_axis", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "ndof_flag", NDOF_PANX_INVERT_AXIS);
  RNA_def_property_ui_text(prop, "Invert X Axis", "");

  /* 3D view: pan y */
  prop = RNA_def_property(srna, "ndof_pany_invert_axis", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "ndof_flag", NDOF_PANY_INVERT_AXIS);
  RNA_def_property_ui_text(prop, "Invert Y Axis", "");

  /* 3D view: pan z */
  prop = RNA_def_property(srna, "ndof_panz_invert_axis", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "ndof_flag", NDOF_PANZ_INVERT_AXIS);
  RNA_def_property_ui_text(prop, "Invert Z Axis", "");

  /* 3D view: fly */
  prop = RNA_def_property(srna, "ndof_lock_horizon", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "ndof_flag", NDOF_LOCK_HORIZON);
  RNA_def_property_ui_text(prop, "Lock Horizon", "Keep horizon level while flying with 3D Mouse");

  prop = RNA_def_property(srna, "ndof_fly_helicopter", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "ndof_flag", NDOF_FLY_HELICOPTER);
  RNA_def_property_ui_text(prop,
                           "Helicopter Mode",
                           "Device up/down directly controls the Z position of the 3D viewport");

  prop = RNA_def_property(srna, "ndof_lock_camera_pan_zoom", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "ndof_flag", NDOF_CAMERA_PAN_ZOOM);
  RNA_def_property_ui_text(
      prop,
      "Lock Camera Pan/Zoom",
      "Pan/zoom the camera view instead of leaving the camera view when orbiting");

  /* let Python know whether NDOF is enabled */
  prop = RNA_def_boolean(srna, "use_ndof", true, "", "");
#  else
  prop = RNA_def_boolean(srna, "use_ndof", false, "", "");
#  endif /* WITH_INPUT_NDOF */
  RNA_def_property_flag(prop, PROP_IDPROPERTY);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "mouse_double_click_time", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "dbl_click_time");
  RNA_def_property_range(prop, 1, 1000);
  RNA_def_property_ui_text(prop, "Double Click Timeout", "Time/delay (in ms) for a double click");

  prop = RNA_def_property(srna, "use_mouse_emulate_3_button", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", USER_TWOBUTTONMOUSE);
  RNA_def_property_ui_text(
      prop, "Emulate 3 Button Mouse", "Emulate Middle Mouse with Alt+Left Mouse");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_userdef_keyconfig_reload_update");

  static const EnumPropertyItem mouse_emulate_3_button_modifier[] = {
      {USER_EMU_MMB_MOD_ALT, "ALT", 0, "Alt", ""},
      {USER_EMU_MMB_MOD_OSKEY, "OSKEY", 0, "OS-Key", ""},
      {0, NULL, 0, NULL, NULL},
  };

  prop = RNA_def_property(srna, "mouse_emulate_3_button_modifier", PROP_ENUM, PROP_NONE);
  /* Only needed because of WIN32 inability to support the option. */
  RNA_def_property_enum_funcs(prop, "rna_UserDef_mouse_emulate_3_button_modifier_get", NULL, NULL);
  RNA_def_property_enum_items(prop, mouse_emulate_3_button_modifier);
  RNA_def_property_ui_text(
      prop, "Emulate 3 Button Modifier", "Hold this modifier to emulate the middle mouse button");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, 0, "rna_userdef_keyconfig_reload_update");

  prop = RNA_def_property(srna, "use_emulate_numpad", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", USER_NONUMPAD);
  RNA_def_property_ui_text(
      prop, "Emulate Numpad", "Main 1 to 0 keys act as the numpad ones (useful for laptops)");

  prop = RNA_def_property(srna, "invert_zoom_wheel", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "uiflag", USER_WHEELZOOMDIR);
  RNA_def_property_ui_text(prop, "Wheel Invert Zoom", "Swap the Mouse Wheel zoom direction");
}

static void rna_def_userdef_keymap(BlenderRNA *brna)
{
  PropertyRNA *prop;

  StructRNA *srna = RNA_def_struct(brna, "PreferencesKeymap", NULL);
  RNA_def_struct_sdna(srna, "UserDef");
  RNA_def_struct_nested(brna, srna, "Preferences");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Keymap", "Shortcut setup for keyboards and other input devices");

  prop = RNA_def_property(srna, "show_ui_keyconfig", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(
      prop, NULL, "space_data.flag", USER_SPACEDATA_INPUT_HIDE_UI_KEYCONFIG);
  RNA_def_property_ui_text(prop, "Show UI Key-Config", "");

  prop = RNA_def_property(srna, "active_keyconfig", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_sdna(prop, NULL, "keyconfigstr");
  RNA_def_property_ui_text(prop, "Key Config", "The name of the active key configuration");
}

static void rna_def_userdef_filepaths_asset_library(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "UserAssetLibrary", NULL);
  RNA_def_struct_sdna(srna, "bUserAssetLibrary");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(
      srna, "Asset Library", "Settings to define a reusable library for Asset Browsers to use");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Name", "Identifier (not necessarily unique) for the asset library");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_userdef_asset_library_name_set");
  RNA_def_struct_name_property(srna, prop);
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  prop = RNA_def_property(srna, "path", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_ui_text(
      prop, "Path", "Path to a directory with .blend files to use as an asset library");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_userdef_asset_library_path_set");
  RNA_def_property_update(prop, 0, "rna_userdef_update");
}

static void rna_def_userdef_filepaths(BlenderRNA *brna)
{
  PropertyRNA *prop;
  StructRNA *srna;

  static const EnumPropertyItem anim_player_presets[] = {
      {0, "INTERNAL", 0, "Internal", "Built-in animation player"},
      {2, "DJV", 0, "DJV", "Open source frame player"},
      {3, "FRAMECYCLER", 0, "FrameCycler", "Frame player from IRIDAS"},
      {4, "RV", 0, "RV", "Frame player from Tweak Software"},
      {5, "MPLAYER", 0, "MPlayer", "Media player for video and PNG/JPEG/SGI image sequences"},
      {50, "CUSTOM", 0, "Custom", "Custom animation player executable path"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem preview_type_items[] = {
      {USER_FILE_PREVIEW_NONE, "NONE", 0, "None", "Do not create blend previews"},
      {USER_FILE_PREVIEW_AUTO, "AUTO", 0, "Auto", "Automatically select best preview type"},
      {USER_FILE_PREVIEW_SCREENSHOT, "SCREENSHOT", 0, "Screenshot", "Capture the entire window"},
      {USER_FILE_PREVIEW_CAMERA, "CAMERA", 0, "Camera View", "Workbench render of scene"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "PreferencesFilePaths", NULL);
  RNA_def_struct_sdna(srna, "UserDef");
  RNA_def_struct_nested(brna, srna, "Preferences");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "File Paths", "Default paths for external files");

  prop = RNA_def_property(srna, "show_hidden_files_datablocks", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "uiflag", USER_HIDE_DOT);
  RNA_def_property_ui_text(prop,
                           "Show Hidden Files/Data-Blocks",
                           "Show files and data-blocks that are normally hidden");

  prop = RNA_def_property(srna, "use_filter_files", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "uiflag", USER_FILTERFILEEXTS);
  RNA_def_property_ui_text(prop, "Filter Files", "Enable filtering of files in the File Browser");

  prop = RNA_def_property(srna, "show_recent_locations", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "uiflag", USER_HIDE_RECENT);
  RNA_def_property_ui_text(
      prop, "Show Recent Locations", "Show Recent locations list in the File Browser");

  prop = RNA_def_property(srna, "show_system_bookmarks", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "uiflag", USER_HIDE_SYSTEM_BOOKMARKS);
  RNA_def_property_ui_text(
      prop, "Show System Locations", "Show System locations list in the File Browser");

  prop = RNA_def_property(srna, "use_relative_paths", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", USER_RELPATHS);
  RNA_def_property_ui_text(
      prop,
      "Relative Paths",
      "Default relative path option for the file selector, when no path is defined yet");

  prop = RNA_def_property(srna, "use_file_compression", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", USER_FILECOMPRESS);
  RNA_def_property_ui_text(
      prop, "Compress File", "Enable file compression when saving .blend files");

  prop = RNA_def_property(srna, "use_load_ui", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", USER_FILENOUI);
  RNA_def_property_ui_text(prop, "Load UI", "Load user interface setup when loading .blend files");
  RNA_def_property_update(prop, 0, "rna_userdef_load_ui_update");

  prop = RNA_def_property(srna, "use_scripts_auto_execute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", USER_SCRIPT_AUTOEXEC_DISABLE);
  RNA_def_property_ui_text(prop,
                           "Auto Run Python Scripts",
                           "Allow any .blend file to run scripts automatically "
                           "(unsafe with blend files from an untrusted source)");
  RNA_def_property_update(prop, 0, "rna_userdef_script_autoexec_update");

  prop = RNA_def_property(srna, "use_tabs_as_spaces", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", USER_TXT_TABSTOSPACES_DISABLE);
  RNA_def_property_ui_text(
      prop,
      "Tabs as Spaces",
      "Automatically convert all new tabs into spaces for new and loaded text files");

  /* Directories. */

  prop = RNA_def_property(srna, "font_directory", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_sdna(prop, NULL, "fontdir");
  RNA_def_property_ui_text(
      prop, "Fonts Directory", "The default directory to search for loading fonts");

  prop = RNA_def_property(srna, "texture_directory", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_sdna(prop, NULL, "textudir");
  RNA_def_property_ui_text(
      prop, "Textures Directory", "The default directory to search for textures");

  prop = RNA_def_property(srna, "render_output_directory", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_sdna(prop, NULL, "renderdir");
  RNA_def_property_ui_text(prop,
                           "Render Output Directory",
                           "The default directory for rendering output, for new scenes");

  prop = RNA_def_property(srna, "script_directory", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_sdna(prop, NULL, "pythondir");
  RNA_def_property_ui_text(
      prop,
      "Python Scripts Directory",
      "Alternate script path, matching the default layout with subdirectories: "
      "startup, add-ons, modules, and presets (requires restart)");
  /* TODO: editing should reset sys.path! */

  prop = RNA_def_property(srna, "i18n_branches_directory", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_sdna(prop, NULL, "i18ndir");
  RNA_def_property_ui_text(
      prop,
      "Translation Branches Directory",
      "The path to the '/branches' directory of your local svn-translation copy, "
      "to allow translating from the UI");

  prop = RNA_def_property(srna, "sound_directory", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_sdna(prop, NULL, "sounddir");
  RNA_def_property_ui_text(prop, "Sounds Directory", "The default directory to search for sounds");

  prop = RNA_def_property(srna, "temporary_directory", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_sdna(prop, NULL, "tempdir");
  RNA_def_property_ui_text(
      prop, "Temporary Directory", "The directory for storing temporary save files");
  RNA_def_property_update(prop, 0, "rna_userdef_temp_update");

  prop = RNA_def_property(srna, "render_cache_directory", PROP_STRING, PROP_DIRPATH);
  RNA_def_property_string_sdna(prop, NULL, "render_cachedir");
  RNA_def_property_ui_text(prop, "Render Cache Path", "Where to cache raw render results");

  prop = RNA_def_property(srna, "image_editor", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_string_sdna(prop, NULL, "image_editor");
  RNA_def_property_ui_text(prop, "Image Editor", "Path to an image editor");

  prop = RNA_def_property(srna, "animation_player", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_string_sdna(prop, NULL, "anim_player");
  RNA_def_property_ui_text(
      prop, "Animation Player", "Path to a custom animation/frame sequence player");

  prop = RNA_def_property(srna, "animation_player_preset", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "anim_player_preset");
  RNA_def_property_enum_items(prop, anim_player_presets);
  RNA_def_property_ui_text(
      prop, "Animation Player Preset", "Preset configs for external animation players");

  /* Autosave. */

  prop = RNA_def_property(srna, "save_version", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "versions");
  RNA_def_property_range(prop, 0, 32);
  RNA_def_property_ui_text(
      prop,
      "Save Versions",
      "The number of old versions to maintain in the current directory, when manually saving");

  prop = RNA_def_property(srna, "use_auto_save_temporary_files", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", USER_AUTOSAVE);
  RNA_def_property_ui_text(prop,
                           "Auto Save Temporary Files",
                           "Automatic saving of temporary files in temp directory, "
                           "uses process ID.\n"
                           "Warning: Sculpt and edit mode data won't be saved");
  RNA_def_property_update(prop, 0, "rna_userdef_autosave_update");

  prop = RNA_def_property(srna, "auto_save_time", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "savetime");
  RNA_def_property_range(prop, 1, 60);
  RNA_def_property_ui_text(
      prop, "Auto Save Time", "The time (in minutes) to wait between automatic temporary saves");
  RNA_def_property_update(prop, 0, "rna_userdef_autosave_update");

  prop = RNA_def_property(srna, "recent_files", PROP_INT, PROP_NONE);
  RNA_def_property_range(prop, 0, 30);
  RNA_def_property_ui_text(
      prop, "Recent Files", "Maximum number of recently opened files to remember");

  prop = RNA_def_property(srna, "file_preview_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, preview_type_items);
  RNA_def_property_ui_text(prop, "File Preview Type", "What type of blend preview to create");

  rna_def_userdef_filepaths_asset_library(brna);

  prop = RNA_def_property(srna, "asset_libraries", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "UserAssetLibrary");
  RNA_def_property_ui_text(prop, "Asset Libraries", "");
}

static void rna_def_userdef_apps(BlenderRNA *brna)
{
  PropertyRNA *prop;
  StructRNA *srna;

  srna = RNA_def_struct(brna, "PreferencesApps", NULL);
  RNA_def_struct_sdna(srna, "UserDef");
  RNA_def_struct_nested(brna, srna, "Preferences");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Apps", "Preferences that work only for apps");

  prop = RNA_def_property(srna, "show_corner_split", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "app_flag", USER_APP_LOCK_CORNER_SPLIT);
  RNA_def_property_ui_text(
      prop, "Corner Splitting", "Split and join editors by dragging from corners");
  RNA_def_property_update(prop, 0, "rna_userdef_screen_update");

  prop = RNA_def_property(srna, "show_edge_resize", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "app_flag", USER_APP_LOCK_EDGE_RESIZE);
  RNA_def_property_ui_text(prop, "Edge Resize", "Resize editors by dragging from the edges");
  RNA_def_property_update(prop, 0, "rna_userdef_screen_update");

  prop = RNA_def_property(srna, "show_regions_visibility_toggle", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "app_flag", USER_APP_HIDE_REGION_TOGGLE);
  RNA_def_property_ui_text(
      prop, "Regions Visibility Toggle", "Header and side bars visibility toggles");
  RNA_def_property_update(prop, 0, "rna_userdef_screen_update");
}

static void rna_def_userdef_experimental(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "PreferencesExperimental", NULL);
  RNA_def_struct_sdna(srna, "UserDef_Experimental");
  RNA_def_struct_nested(brna, srna, "Preferences");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Experimental", "Experimental features");

  prop = RNA_def_property(srna, "use_undo_legacy", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "use_undo_legacy", 1);
  RNA_def_property_ui_text(
      prop,
      "Undo Legacy",
      "Use legacy undo (slower than the new default one, but may be more stable in some cases)");

  prop = RNA_def_property(srna, "override_auto_resync", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "no_override_auto_resync", 1);
  RNA_def_property_ui_text(
      prop,
      "Override Auto Resync",
      "Enable library overrides automatic resync detection and process on file load. Disable when "
      "dealing with older .blend files that need manual Resync (Enforce) handling");

  prop = RNA_def_property(srna, "use_new_point_cloud_type", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "use_new_point_cloud_type", 1);
  RNA_def_property_ui_text(
      prop, "New Point Cloud Type", "Enable the new point cloud type in the ui");

  prop = RNA_def_property(srna, "use_full_frame_compositor", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "use_full_frame_compositor", 1);
  RNA_def_property_ui_text(prop,
                           "Full Frame Compositor",
                           "Enable compositor full frame execution mode option (no tiling, "
                           "reduces execution time and memory usage)");
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  prop = RNA_def_property(srna, "use_new_curves_type", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "use_new_curves_type", 1);
  RNA_def_property_ui_text(prop, "New Curves Type", "Enable the new curves data type in the UI");

  prop = RNA_def_property(srna, "use_new_curves_tools", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "use_new_curves_tools", 1);
  RNA_def_property_editable_func(prop, "rna_userdef_experimental_use_new_curve_tools_editable");
  RNA_def_property_ui_text(
      prop, "New Curves Tools", "Enable additional features for the new curves data block");

  prop = RNA_def_property(srna, "use_cycles_debug", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "use_cycles_debug", 1);
  RNA_def_property_ui_text(prop, "Cycles Debug", "Enable Cycles debugging options for developers");
  RNA_def_property_update(prop, 0, "rna_userdef_update");

  prop = RNA_def_property(srna, "use_sculpt_tools_tilt", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "use_sculpt_tools_tilt", 1);
  RNA_def_property_ui_text(
      prop, "Sculpt Mode Tilt Support", "Support for pen tablet tilt events in Sculpt Mode");

  prop = RNA_def_property(srna, "use_sculpt_texture_paint", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "use_sculpt_texture_paint", 1);
  RNA_def_property_ui_text(prop, "Sculpt Texture Paint", "Use texture painting in Sculpt Mode");

  prop = RNA_def_property(srna, "use_draw_manager_acquire_lock", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "use_draw_manager_acquire_lock", 1);
  RNA_def_property_ui_text(
      prop, "Draw Manager Locking", "Don't lock UI during background rendering");

  prop = RNA_def_property(srna, "use_extended_asset_browser", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(prop,
                           "Extended Asset Browser",
                           "Enable Asset Browser editor and operators to manage regular "
                           "data-blocks as assets, not just poses");
  RNA_def_property_update(prop, 0, "rna_userdef_ui_update");

  prop = RNA_def_property(srna, "show_asset_debug_info", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(prop,
                           "Asset Debug Info",
                           "Enable some extra fields in the Asset Browser to aid in debugging");
  RNA_def_property_update(prop, 0, "rna_userdef_ui_update");

  prop = RNA_def_property(srna, "use_asset_indexing", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "no_asset_indexing", 1);
  RNA_def_property_ui_text(prop,
                           "Asset Indexing",
                           "Disabling the asset indexer forces every asset library refresh to "
                           "completely reread assets from disk");
  RNA_def_property_update(prop, 0, "rna_userdef_ui_update");

  prop = RNA_def_property(srna, "use_override_templates", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "use_override_templates", 1);
  RNA_def_property_ui_text(
      prop, "Override Templates", "Enable library override template in the python API");

  prop = RNA_def_property(srna, "enable_eevee_next", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "enable_eevee_next", 1);
  RNA_def_property_ui_text(prop, "EEVEE Next", "Enable the new EEVEE codebase, requires restart");
}

static void rna_def_userdef_addon_collection(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "Addons");
  srna = RNA_def_struct(brna, "Addons", NULL);
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "User Add-ons", "Collection of add-ons");

  func = RNA_def_function(srna, "new", "rna_userdef_addon_new");
  RNA_def_function_flag(func, FUNC_NO_SELF);
  RNA_def_function_ui_description(func, "Add a new add-on");
  /* return type */
  parm = RNA_def_pointer(func, "addon", "Addon", "", "Add-on data");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_userdef_addon_remove");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove add-on");
  parm = RNA_def_pointer(func, "addon", "Addon", "", "Add-on to remove");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);
}

static void rna_def_userdef_autoexec_path_collection(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  FunctionRNA *func;
  PropertyRNA *parm;

  RNA_def_property_srna(cprop, "PathCompareCollection");
  srna = RNA_def_struct(brna, "PathCompareCollection", NULL);
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Paths Compare", "Collection of paths");

  func = RNA_def_function(srna, "new", "rna_userdef_pathcompare_new");
  RNA_def_function_flag(func, FUNC_NO_SELF);
  RNA_def_function_ui_description(func, "Add a new path");
  /* return type */
  parm = RNA_def_pointer(func, "pathcmp", "PathCompare", "", "");
  RNA_def_function_return(func, parm);

  func = RNA_def_function(srna, "remove", "rna_userdef_pathcompare_remove");
  RNA_def_function_flag(func, FUNC_NO_SELF | FUNC_USE_REPORTS);
  RNA_def_function_ui_description(func, "Remove path");
  parm = RNA_def_pointer(func, "pathcmp", "PathCompare", "", "");
  RNA_def_parameter_flags(parm, PROP_NEVER_NULL, PARM_REQUIRED | PARM_RNAPTR);
  RNA_def_parameter_clear_flags(parm, PROP_THICK_WRAP, 0);
}

void RNA_def_userdef(BlenderRNA *brna)
{
  USERDEF_TAG_DIRTY_PROPERTY_UPDATE_ENABLE;

  StructRNA *srna;
  PropertyRNA *prop;

  rna_def_userdef_dothemes(brna);
  rna_def_userdef_solidlight(brna);
  rna_def_userdef_walk_navigation(brna);

  srna = RNA_def_struct(brna, "Preferences", NULL);
  RNA_def_struct_sdna(srna, "UserDef");
  RNA_def_struct_clear_flag(srna, STRUCT_UNDO);
  RNA_def_struct_ui_text(srna, "Preferences", "Global preferences");

  prop = RNA_def_property(srna, "active_section", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "space_data.section_active");
  RNA_def_property_enum_items(prop, rna_enum_preference_section_items);
  RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_UseDef_active_section_itemf");
  RNA_def_property_ui_text(
      prop, "Active Section", "Active section of the preferences shown in the user interface");
  RNA_def_property_update(prop, 0, "rna_userdef_ui_update");

  /* don't expose this directly via the UI, modify via an operator */
  prop = RNA_def_property(srna, "app_template", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "app_template");
  RNA_def_property_ui_text(prop, "Application Template", "");

  prop = RNA_def_property(srna, "themes", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "themes", NULL);
  RNA_def_property_struct_type(prop, "Theme");
  RNA_def_property_ui_text(prop, "Themes", "");

  prop = RNA_def_property(srna, "ui_styles", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "uistyles", NULL);
  RNA_def_property_struct_type(prop, "ThemeStyle");
  RNA_def_property_ui_text(prop, "Styles", "");

  prop = RNA_def_property(srna, "addons", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "addons", NULL);
  RNA_def_property_struct_type(prop, "Addon");
  RNA_def_property_ui_text(prop, "Add-on", "");
  rna_def_userdef_addon_collection(brna, prop);

  prop = RNA_def_property(srna, "autoexec_paths", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "autoexec_paths", NULL);
  RNA_def_property_struct_type(prop, "PathCompare");
  RNA_def_property_ui_text(prop, "Auto-Execution Paths", "");
  rna_def_userdef_autoexec_path_collection(brna, prop);

  /* nested structs */
  prop = RNA_def_property(srna, "view", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "PreferencesView");
  RNA_def_property_pointer_funcs(prop, "rna_UserDef_view_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "View & Controls", "Preferences related to viewing data");

  prop = RNA_def_property(srna, "edit", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "PreferencesEdit");
  RNA_def_property_pointer_funcs(prop, "rna_UserDef_edit_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "Edit Methods", "Settings for interacting with Blender data");

  prop = RNA_def_property(srna, "inputs", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "PreferencesInput");
  RNA_def_property_pointer_funcs(prop, "rna_UserDef_input_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "Inputs", "Settings for input devices");

  prop = RNA_def_property(srna, "keymap", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "PreferencesKeymap");
  RNA_def_property_pointer_funcs(prop, "rna_UserDef_keymap_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "Keymap", "Shortcut setup for keyboards and other input devices");

  prop = RNA_def_property(srna, "filepaths", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "PreferencesFilePaths");
  RNA_def_property_pointer_funcs(prop, "rna_UserDef_filepaths_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "File Paths", "Default paths for external files");

  prop = RNA_def_property(srna, "system", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "PreferencesSystem");
  RNA_def_property_pointer_funcs(prop, "rna_UserDef_system_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(
      prop, "System & OpenGL", "Graphics driver and operating system settings");

  prop = RNA_def_property(srna, "apps", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "PreferencesApps");
  RNA_def_property_pointer_funcs(prop, "rna_UserDef_apps_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "Apps", "Preferences that work only for apps");

  prop = RNA_def_property(srna, "experimental", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "PreferencesExperimental");
  RNA_def_property_ui_text(
      prop,
      "Experimental",
      "Settings for features that are still early in their development stage");

  prop = RNA_def_int_vector(srna,
                            "version",
                            3,
                            NULL,
                            0,
                            INT_MAX,
                            "Version",
                            "Version of Blender the userpref.blend was saved with",
                            0,
                            INT_MAX);
  RNA_def_property_int_funcs(prop, "rna_userdef_version_get", NULL, NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_flag(prop, PROP_THICK_WRAP);

  /* StudioLight Collection */
  prop = RNA_def_property(srna, "studio_lights", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "StudioLight");
  RNA_def_property_srna(prop, "StudioLights");
  RNA_def_property_collection_funcs(prop,
                                    "rna_UserDef_studiolight_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_iterator_listbase_get",
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_ui_text(prop, "Studio Lights", "");

  /* Preferences Flags */
  prop = RNA_def_property(srna, "use_preferences_save", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "pref_flag", USER_PREF_FLAG_SAVE);
  RNA_def_property_ui_text(prop,
                           "Save on Exit",
                           "Save preferences on exit when modified "
                           "(unless factory settings have been loaded)");

  prop = RNA_def_property(srna, "is_dirty", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "runtime.is_dirty", 0);
  RNA_def_property_ui_text(prop, "Dirty", "Preferences have changed");
  RNA_def_property_update(prop, 0, "rna_userdef_ui_update");

  rna_def_userdef_view(brna);
  rna_def_userdef_edit(brna);
  rna_def_userdef_input(brna);
  rna_def_userdef_keymap(brna);
  rna_def_userdef_filepaths(brna);
  rna_def_userdef_system(brna);
  rna_def_userdef_addon(brna);
  rna_def_userdef_addon_pref(brna);
  rna_def_userdef_studiolights(brna);
  rna_def_userdef_studiolight(brna);
  rna_def_userdef_pathcompare(brna);
  rna_def_userdef_apps(brna);
  rna_def_userdef_experimental(brna);

  USERDEF_TAG_DIRTY_PROPERTY_UPDATE_DISABLE;
}

#endif

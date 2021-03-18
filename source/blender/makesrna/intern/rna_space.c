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

#include "MEM_guardedalloc.h"

#include "BLT_translation.h"

#include "BKE_attribute.h"
#include "BKE_context.h"
#include "BKE_geometry_set.h"
#include "BKE_image.h"
#include "BKE_key.h"
#include "BKE_movieclip.h"
#include "BKE_node.h"
#include "BKE_studiolight.h"

#include "ED_text.h"

#include "BLI_listbase.h"
#include "BLI_math.h"

#include "DNA_action_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_key_types.h"
#include "DNA_mask_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_sequence_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"
#include "DNA_workspace_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "rna_internal.h"

#include "SEQ_proxy.h"
#include "SEQ_relations.h"
#include "SEQ_sequencer.h"

#include "WM_api.h"
#include "WM_types.h"

#include "RE_engine.h"
#include "RE_pipeline.h"

#include "RNA_enum_types.h"

const EnumPropertyItem rna_enum_space_type_items[] = {
    /* empty must be here for python, is skipped for UI */
    {SPACE_EMPTY, "EMPTY", ICON_NONE, "Empty", ""},

    /* General */
    {0, "", ICON_NONE, "General", ""},
    {SPACE_VIEW3D,
     "VIEW_3D",
     ICON_VIEW3D,
     "3D Viewport",
     "Manipulate objects in a 3D environment"},
    {SPACE_IMAGE,
     "IMAGE_EDITOR",
     ICON_IMAGE,
     "UV/Image Editor",
     "View and edit images and UV Maps"},
    {SPACE_NODE,
     "NODE_EDITOR",
     ICON_NODETREE,
     "Node Editor",
     "Editor for node-based shading and compositing tools"},
    {SPACE_SEQ, "SEQUENCE_EDITOR", ICON_SEQUENCE, "Video Sequencer", "Video editing tools"},
    {SPACE_CLIP, "CLIP_EDITOR", ICON_TRACKER, "Movie Clip Editor", "Motion tracking tools"},

    /* Animation */
    {0, "", ICON_NONE, "Animation", ""},
#if 0
    {SPACE_ACTION,
     "TIMELINE",
     ICON_TIME,
     "Timeline",
     "Timeline and playback controls (NOTE: Switch to 'Timeline' mode)"}, /* XXX */
#endif
    {SPACE_ACTION, "DOPESHEET_EDITOR", ICON_ACTION, "Dope Sheet", "Adjust timing of keyframes"},
    {SPACE_GRAPH,
     "GRAPH_EDITOR",
     ICON_GRAPH,
     "Graph Editor",
     "Edit drivers and keyframe interpolation"},
    {SPACE_NLA, "NLA_EDITOR", ICON_NLA, "Nonlinear Animation", "Combine and layer Actions"},

    /* Scripting */
    {0, "", ICON_NONE, "Scripting", ""},
    {SPACE_TEXT,
     "TEXT_EDITOR",
     ICON_TEXT,
     "Text Editor",
     "Edit scripts and in-file documentation"},
    {SPACE_CONSOLE,
     "CONSOLE",
     ICON_CONSOLE,
     "Python Console",
     "Interactive programmatic console for "
     "advanced editing and script development"},
    {SPACE_INFO, "INFO", ICON_INFO, "Info", "Log of operations, warnings and error messages"},
    /* Special case: Top-bar and Status-bar aren't supposed to be a regular editor for the user. */
    {SPACE_TOPBAR,
     "TOPBAR",
     ICON_NONE,
     "Top Bar",
     "Global bar at the top of the screen for "
     "global per-window settings"},
    {SPACE_STATUSBAR,
     "STATUSBAR",
     ICON_NONE,
     "Status Bar",
     "Global bar at the bottom of the "
     "screen for general status information"},

    /* Data */
    {0, "", ICON_NONE, "Data", ""},
    {SPACE_OUTLINER,
     "OUTLINER",
     ICON_OUTLINER,
     "Outliner",
     "Overview of scene graph and all available data-blocks"},
    {SPACE_PROPERTIES,
     "PROPERTIES",
     ICON_PROPERTIES,
     "Properties",
     "Edit properties of active object and related data-blocks"},
    {SPACE_FILE, "FILE_BROWSER", ICON_FILEBROWSER, "File Browser", "Browse for files and assets"},
    {SPACE_SPREADSHEET,
     "SPREADSHEET",
     ICON_SPREADSHEET,
     "Spreadsheet",
     "Explore geometry data in a table"},
    {SPACE_USERPREF,
     "PREFERENCES",
     ICON_PREFERENCES,
     "Preferences",
     "Edit persistent configuration settings"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_space_graph_mode_items[] = {
    {SIPO_MODE_ANIMATION,
     "FCURVES",
     ICON_GRAPH,
     "Graph Editor",
     "Edit animation/keyframes displayed as 2D curves"},
    {SIPO_MODE_DRIVERS, "DRIVERS", ICON_DRIVER, "Drivers", "Edit drivers"},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_space_sequencer_view_type_items[] = {
    {SEQ_VIEW_SEQUENCE, "SEQUENCER", ICON_SEQ_SEQUENCER, "Sequencer", ""},
    {SEQ_VIEW_PREVIEW, "PREVIEW", ICON_SEQ_PREVIEW, "Preview", ""},
    {SEQ_VIEW_SEQUENCE_PREVIEW, "SEQUENCER_PREVIEW", ICON_SEQ_SPLITVIEW, "Sequencer/Preview", ""},
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_space_file_browse_mode_items[] = {
    {FILE_BROWSE_MODE_FILES, "FILES", ICON_FILEBROWSER, "File Browser", ""},
    {FILE_BROWSE_MODE_ASSETS, "ASSETS", ICON_ASSET_MANAGER, "Asset Browser", ""},
    {0, NULL, 0, NULL, NULL},
};

#define SACT_ITEM_DOPESHEET \
  { \
    SACTCONT_DOPESHEET, "DOPESHEET", ICON_ACTION, "Dope Sheet", "Edit all keyframes in scene" \
  }
#define SACT_ITEM_TIMELINE \
  { \
    SACTCONT_TIMELINE, "TIMELINE", ICON_TIME, "Timeline", "Timeline and playback controls" \
  }
#define SACT_ITEM_ACTION \
  { \
    SACTCONT_ACTION, "ACTION", ICON_OBJECT_DATA, "Action Editor", \
        "Edit keyframes in active object's Object-level action" \
  }
#define SACT_ITEM_SHAPEKEY \
  { \
    SACTCONT_SHAPEKEY, "SHAPEKEY", ICON_SHAPEKEY_DATA, "Shape Key Editor", \
        "Edit keyframes in active object's Shape Keys action" \
  }
#define SACT_ITEM_GPENCIL \
  { \
    SACTCONT_GPENCIL, "GPENCIL", ICON_GREASEPENCIL, "Grease Pencil", \
        "Edit timings for all Grease Pencil sketches in file" \
  }
#define SACT_ITEM_MASK \
  { \
    SACTCONT_MASK, "MASK", ICON_MOD_MASK, "Mask", "Edit timings for Mask Editor splines" \
  }
#define SACT_ITEM_CACHEFILE \
  { \
    SACTCONT_CACHEFILE, "CACHEFILE", ICON_FILE, "Cache File", \
        "Edit timings for Cache File data-blocks" \
  }

#ifndef RNA_RUNTIME
/* XXX: action-editor is currently for object-level only actions,
 * so show that using object-icon hint */
static EnumPropertyItem rna_enum_space_action_mode_all_items[] = {
    SACT_ITEM_DOPESHEET,
    SACT_ITEM_TIMELINE,
    SACT_ITEM_ACTION,
    SACT_ITEM_SHAPEKEY,
    SACT_ITEM_GPENCIL,
    SACT_ITEM_MASK,
    SACT_ITEM_CACHEFILE,
    {0, NULL, 0, NULL, NULL},
};
static EnumPropertyItem rna_enum_space_action_ui_mode_items[] = {
    SACT_ITEM_DOPESHEET,
    /* SACT_ITEM_TIMELINE, */
    SACT_ITEM_ACTION,
    SACT_ITEM_SHAPEKEY,
    SACT_ITEM_GPENCIL,
    SACT_ITEM_MASK,
    SACT_ITEM_CACHEFILE,
    {0, NULL, 0, NULL, NULL},
};
#endif
/* expose as ui_mode */
const EnumPropertyItem rna_enum_space_action_mode_items[] = {
    SACT_ITEM_DOPESHEET,
    SACT_ITEM_TIMELINE,
    {0, NULL, 0, NULL, NULL},
};

#undef SACT_ITEM_DOPESHEET
#undef SACT_ITEM_TIMELINE
#undef SACT_ITEM_ACTION
#undef SACT_ITEM_SHAPEKEY
#undef SACT_ITEM_GPENCIL
#undef SACT_ITEM_MASK
#undef SACT_ITEM_CACHEFILE

#define SI_ITEM_VIEW(identifier, name, icon) \
  { \
    SI_MODE_VIEW, identifier, icon, name, "View the image" \
  }
#define SI_ITEM_UV \
  { \
    SI_MODE_UV, "UV", ICON_UV, "UV Editor", "UV edit in mesh editmode" \
  }
#define SI_ITEM_PAINT \
  { \
    SI_MODE_PAINT, "PAINT", ICON_TPAINT_HLT, "Paint", "2D image painting mode" \
  }
#define SI_ITEM_MASK \
  { \
    SI_MODE_MASK, "MASK", ICON_MOD_MASK, "Mask", "Mask editing" \
  }

const EnumPropertyItem rna_enum_space_image_mode_all_items[] = {
    SI_ITEM_VIEW("VIEW", "View", ICON_FILE_IMAGE),
    SI_ITEM_UV,
    SI_ITEM_PAINT,
    SI_ITEM_MASK,
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem rna_enum_space_image_mode_ui_items[] = {
    SI_ITEM_VIEW("VIEW", "View", ICON_FILE_IMAGE),
    SI_ITEM_PAINT,
    SI_ITEM_MASK,
    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_space_image_mode_items[] = {
    SI_ITEM_VIEW("IMAGE_EDITOR", "Image Editor", ICON_IMAGE),
    SI_ITEM_UV,
    {0, NULL, 0, NULL, NULL},
};

#undef SI_ITEM_VIEW
#undef SI_ITEM_UV
#undef SI_ITEM_PAINT
#undef SI_ITEM_MASK

#define V3D_S3D_CAMERA_LEFT {STEREO_LEFT_ID, "LEFT", ICON_RESTRICT_RENDER_OFF, "Left", ""},
#define V3D_S3D_CAMERA_RIGHT {STEREO_RIGHT_ID, "RIGHT", ICON_RESTRICT_RENDER_OFF, "Right", ""},
#define V3D_S3D_CAMERA_S3D {STEREO_3D_ID, "S3D", ICON_CAMERA_STEREO, "3D", ""},
#ifdef RNA_RUNTIME
#  define V3D_S3D_CAMERA_VIEWS {STEREO_MONO_ID, "MONO", ICON_RESTRICT_RENDER_OFF, "Views", ""},
#endif

static const EnumPropertyItem stereo3d_camera_items[] = {
    V3D_S3D_CAMERA_LEFT V3D_S3D_CAMERA_RIGHT V3D_S3D_CAMERA_S3D{0, NULL, 0, NULL, NULL},
};

#ifdef RNA_RUNTIME
static const EnumPropertyItem multiview_camera_items[] = {
    V3D_S3D_CAMERA_VIEWS V3D_S3D_CAMERA_S3D{0, NULL, 0, NULL, NULL},
};
#endif

#undef V3D_S3D_CAMERA_LEFT
#undef V3D_S3D_CAMERA_RIGHT
#undef V3D_S3D_CAMERA_S3D
#undef V3D_S3D_CAMERA_VIEWS

const EnumPropertyItem rna_enum_fileselect_params_sort_items[] = {
    {FILE_SORT_ALPHA, "FILE_SORT_ALPHA", ICON_NONE, "Name", "Sort the file list alphabetically"},
    {FILE_SORT_EXTENSION,
     "FILE_SORT_EXTENSION",
     ICON_NONE,
     "Extension",
     "Sort the file list by extension/type"},
    {FILE_SORT_TIME,
     "FILE_SORT_TIME",
     ICON_NONE,
     "Modified Date",
     "Sort files by modification time"},
    {FILE_SORT_SIZE, "FILE_SORT_SIZE", ICON_NONE, "Size", "Sort files by size"},
    {0, NULL, 0, NULL, NULL},
};

#ifndef RNA_RUNTIME
static const EnumPropertyItem stereo3d_eye_items[] = {
    {STEREO_LEFT_ID, "LEFT_EYE", ICON_NONE, "Left Eye"},
    {STEREO_RIGHT_ID, "RIGHT_EYE", ICON_NONE, "Right Eye"},
    {0, NULL, 0, NULL, NULL},
};
#endif

static const EnumPropertyItem display_channels_items[] = {
    {SI_USE_ALPHA,
     "COLOR_ALPHA",
     ICON_IMAGE_RGB_ALPHA,
     "Color and Alpha",
     "Display image with RGB colors and alpha transparency"},
    {0, "COLOR", ICON_IMAGE_RGB, "Color", "Display image with RGB colors"},
    {SI_SHOW_ALPHA, "ALPHA", ICON_IMAGE_ALPHA, "Alpha", "Display  alpha transparency channel"},
    {SI_SHOW_ZBUF,
     "Z_BUFFER",
     ICON_IMAGE_ZDEPTH,
     "Z-Buffer",
     "Display Z-buffer associated with image (mapped from camera clip start to end)"},
    {SI_SHOW_R, "RED", ICON_COLOR_RED, "Red", ""},
    {SI_SHOW_G, "GREEN", ICON_COLOR_GREEN, "Green", ""},
    {SI_SHOW_B, "BLUE", ICON_COLOR_BLUE, "Blue", ""},
    {0, NULL, 0, NULL, NULL},
};

#ifndef RNA_RUNTIME
static const EnumPropertyItem autosnap_items[] = {
    {SACTSNAP_OFF, "NONE", 0, "No Auto-Snap", ""},
    /* {-1, "", 0, "", ""}, */
    {SACTSNAP_STEP, "STEP", 0, "Frame Step", "Snap to 1.0 frame intervals"},
    {SACTSNAP_TSTEP, "TIME_STEP", 0, "Second Step", "Snap to 1.0 second intervals"},
    /* {-1, "", 0, "", ""}, */
    {SACTSNAP_FRAME, "FRAME", 0, "Nearest Frame", "Snap to actual frames (nla-action time)"},
    {SACTSNAP_SECOND, "SECOND", 0, "Nearest Second", "Snap to actual seconds (nla-action time)"},
    /* {-1, "", 0, "", ""}, */
    {SACTSNAP_MARKER, "MARKER", 0, "Nearest Marker", "Snap to nearest marker"},
    {0, NULL, 0, NULL, NULL},
};
#endif

const EnumPropertyItem rna_enum_shading_type_items[] = {
    {OB_WIRE, "WIREFRAME", ICON_SHADING_WIRE, "Wireframe", "Display the object as wire edges"},
    {OB_SOLID, "SOLID", ICON_SHADING_SOLID, "Solid", "Display in solid mode"},
    {OB_MATERIAL,
     "MATERIAL",
     ICON_SHADING_TEXTURE,
     "Material Preview",
     "Display in Material Preview mode"},
    {OB_RENDER, "RENDERED", ICON_SHADING_RENDERED, "Rendered", "Display render preview"},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem rna_enum_viewport_lighting_items[] = {
    {V3D_LIGHTING_STUDIO, "STUDIO", 0, "Studio", "Display using studio lighting"},
    {V3D_LIGHTING_MATCAP, "MATCAP", 0, "MatCap", "Display using matcap material and lighting"},
    {V3D_LIGHTING_FLAT, "FLAT", 0, "Flat", "Display using flat lighting"},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem rna_enum_shading_color_type_items[] = {
    {V3D_SHADING_MATERIAL_COLOR, "MATERIAL", 0, "Material", "Show material color"},
    {V3D_SHADING_SINGLE_COLOR, "SINGLE", 0, "Single", "Show scene in a single color"},
    {V3D_SHADING_OBJECT_COLOR, "OBJECT", 0, "Object", "Show object color"},
    {V3D_SHADING_RANDOM_COLOR, "RANDOM", 0, "Random", "Show random object color"},
    {V3D_SHADING_VERTEX_COLOR, "VERTEX", 0, "Vertex", "Show active vertex color"},
    {V3D_SHADING_TEXTURE_COLOR, "TEXTURE", 0, "Texture", "Show texture"},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem rna_enum_studio_light_items[] = {
    {0, "DEFAULT", 0, "Default", ""},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem rna_enum_view3dshading_render_pass_type_items[] = {
    {0, "", ICON_NONE, "General", ""},
    {EEVEE_RENDER_PASS_COMBINED, "COMBINED", 0, "Combined", ""},
    {EEVEE_RENDER_PASS_EMIT, "EMISSION", 0, "Emission", ""},
    {EEVEE_RENDER_PASS_ENVIRONMENT, "ENVIRONMENT", 0, "Environment", ""},
    {EEVEE_RENDER_PASS_AO, "AO", 0, "Ambient Occlusion", ""},
    {EEVEE_RENDER_PASS_SHADOW, "SHADOW", 0, "Shadow", ""},

    {0, "", ICON_NONE, "Light", ""},
    {EEVEE_RENDER_PASS_DIFFUSE_LIGHT, "DIFFUSE_LIGHT", 0, "Diffuse Light", ""},
    {EEVEE_RENDER_PASS_DIFFUSE_COLOR, "DIFFUSE_COLOR", 0, "Diffuse Color", ""},
    {EEVEE_RENDER_PASS_SPECULAR_LIGHT, "SPECULAR_LIGHT", 0, "Specular Light", ""},
    {EEVEE_RENDER_PASS_SPECULAR_COLOR, "SPECULAR_COLOR", 0, "Specular Color", ""},
    {EEVEE_RENDER_PASS_VOLUME_LIGHT, "VOLUME_LIGHT", 0, "Volume Light", ""},

    {0, "", ICON_NONE, "Effects", ""},
    {EEVEE_RENDER_PASS_BLOOM, "BLOOM", 0, "Bloom", ""},

    {0, "", ICON_NONE, "Data", ""},
    {EEVEE_RENDER_PASS_NORMAL, "NORMAL", 0, "Normal", ""},
    {EEVEE_RENDER_PASS_MIST, "MIST", 0, "Mist", ""},

    {0, "", ICON_NONE, "Shader AOV", ""},
    {EEVEE_RENDER_PASS_AOV, "AOV", 0, "AOV", ""},

    {0, NULL, 0, NULL, NULL},
};

const EnumPropertyItem rna_enum_clip_editor_mode_items[] = {
    {SC_MODE_TRACKING, "TRACKING", ICON_ANIM_DATA, "Tracking", "Show tracking and solving tools"},
    {SC_MODE_MASKEDIT, "MASK", ICON_MOD_MASK, "Mask", "Show mask editing tools"},
    {0, NULL, 0, NULL, NULL},
};

/* Actually populated dynamically through a function,
 * but helps for context-less access (e.g. doc, i18n...). */
static const EnumPropertyItem buttons_context_items[] = {
    {BCONTEXT_TOOL, "TOOL", ICON_TOOL_SETTINGS, "Tool", "Active Tool and Workspace settings"},
    {BCONTEXT_SCENE, "SCENE", ICON_SCENE_DATA, "Scene", "Scene Properties"},
    {BCONTEXT_RENDER, "RENDER", ICON_SCENE, "Render", "Render Properties"},
    {BCONTEXT_OUTPUT, "OUTPUT", ICON_OUTPUT, "Output", "Output Properties"},
    {BCONTEXT_VIEW_LAYER, "VIEW_LAYER", ICON_RENDER_RESULT, "View Layer", "View Layer Properties"},
    {BCONTEXT_WORLD, "WORLD", ICON_WORLD, "World", "World Properties"},
    {BCONTEXT_COLLECTION, "COLLECTION", ICON_GROUP, "Collection", "Collection Properties"},
    {BCONTEXT_OBJECT, "OBJECT", ICON_OBJECT_DATA, "Object", "Object Properties"},
    {BCONTEXT_CONSTRAINT,
     "CONSTRAINT",
     ICON_CONSTRAINT,
     "Constraints",
     "Object Constraint Properties"},
    {BCONTEXT_MODIFIER, "MODIFIER", ICON_MODIFIER, "Modifiers", "Modifier Properties"},
    {BCONTEXT_DATA, "DATA", ICON_NONE, "Data", "Object Data Properties"},
    {BCONTEXT_BONE, "BONE", ICON_BONE_DATA, "Bone", "Bone Properties"},
    {BCONTEXT_BONE_CONSTRAINT,
     "BONE_CONSTRAINT",
     ICON_CONSTRAINT_BONE,
     "Bone Constraints",
     "Bone Constraint Properties"},
    {BCONTEXT_MATERIAL, "MATERIAL", ICON_MATERIAL, "Material", "Material Properties"},
    {BCONTEXT_TEXTURE, "TEXTURE", ICON_TEXTURE, "Texture", "Texture Properties"},
    {BCONTEXT_PARTICLE, "PARTICLES", ICON_PARTICLES, "Particles", "Particle Properties"},
    {BCONTEXT_PHYSICS, "PHYSICS", ICON_PHYSICS, "Physics", "Physics Properties"},
    {BCONTEXT_SHADERFX, "SHADERFX", ICON_SHADERFX, "Effects", "Visual Effects Properties"},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem fileselectparams_recursion_level_items[] = {
    {0, "NONE", 0, "None", "Only list current directory's content, with no recursion"},
    {1, "BLEND", 0, "Blend File", "List .blend files' content"},
    {2, "ALL_1", 0, "One Level", "List all sub-directories' content, one level of recursion"},
    {3, "ALL_2", 0, "Two Levels", "List all sub-directories' content, two levels of recursion"},
    {4,
     "ALL_3",
     0,
     "Three Levels",
     "List all sub-directories' content, three levels of recursion"},
    {0, NULL, 0, NULL, NULL},
};

static const EnumPropertyItem rna_enum_curve_display_handle_items[] = {
    {CURVE_HANDLE_NONE, "NONE", 0, "None", ""},
    {CURVE_HANDLE_SELECTED, "SELECTED", 0, "Selected", ""},
    {CURVE_HANDLE_ALL, "ALL", 0, "All", ""},
    {0, NULL, 0, NULL, NULL},
};

#ifdef RNA_RUNTIME

#  include "DNA_anim_types.h"
#  include "DNA_scene_types.h"
#  include "DNA_screen_types.h"
#  include "DNA_userdef_types.h"

#  include "BLI_path_util.h"
#  include "BLI_string.h"

#  include "BKE_anim_data.h"
#  include "BKE_brush.h"
#  include "BKE_colortools.h"
#  include "BKE_context.h"
#  include "BKE_global.h"
#  include "BKE_icons.h"
#  include "BKE_idprop.h"
#  include "BKE_layer.h"
#  include "BKE_nla.h"
#  include "BKE_paint.h"
#  include "BKE_preferences.h"
#  include "BKE_scene.h"
#  include "BKE_screen.h"
#  include "BKE_workspace.h"

#  include "DEG_depsgraph.h"
#  include "DEG_depsgraph_build.h"

#  include "ED_anim_api.h"
#  include "ED_buttons.h"
#  include "ED_clip.h"
#  include "ED_fileselect.h"
#  include "ED_image.h"
#  include "ED_node.h"
#  include "ED_screen.h"
#  include "ED_sequencer.h"
#  include "ED_transform.h"
#  include "ED_view3d.h"

#  include "GPU_material.h"

#  include "IMB_imbuf_types.h"

#  include "UI_interface.h"
#  include "UI_view2d.h"

static StructRNA *rna_Space_refine(struct PointerRNA *ptr)
{
  SpaceLink *space = (SpaceLink *)ptr->data;

  switch ((eSpace_Type)space->spacetype) {
    case SPACE_VIEW3D:
      return &RNA_SpaceView3D;
    case SPACE_GRAPH:
      return &RNA_SpaceGraphEditor;
    case SPACE_OUTLINER:
      return &RNA_SpaceOutliner;
    case SPACE_PROPERTIES:
      return &RNA_SpaceProperties;
    case SPACE_FILE:
      return &RNA_SpaceFileBrowser;
    case SPACE_IMAGE:
      return &RNA_SpaceImageEditor;
    case SPACE_INFO:
      return &RNA_SpaceInfo;
    case SPACE_SEQ:
      return &RNA_SpaceSequenceEditor;
    case SPACE_TEXT:
      return &RNA_SpaceTextEditor;
    case SPACE_ACTION:
      return &RNA_SpaceDopeSheetEditor;
    case SPACE_NLA:
      return &RNA_SpaceNLA;
    case SPACE_NODE:
      return &RNA_SpaceNodeEditor;
    case SPACE_CONSOLE:
      return &RNA_SpaceConsole;
    case SPACE_USERPREF:
      return &RNA_SpacePreferences;
    case SPACE_CLIP:
      return &RNA_SpaceClipEditor;
    case SPACE_SPREADSHEET:
      return &RNA_SpaceSpreadsheet;

      /* Currently no type info. */
    case SPACE_SCRIPT:
    case SPACE_EMPTY:
    case SPACE_TOPBAR:
    case SPACE_STATUSBAR:
      break;
  }

  return &RNA_Space;
}

static ScrArea *rna_area_from_space(PointerRNA *ptr)
{
  bScreen *screen = (bScreen *)ptr->owner_id;
  SpaceLink *link = (SpaceLink *)ptr->data;
  return BKE_screen_find_area_from_space(screen, link);
}

static void area_region_from_regiondata(bScreen *screen,
                                        void *regiondata,
                                        ScrArea **r_area,
                                        ARegion **r_region)
{
  ScrArea *area;
  ARegion *region;

  *r_area = NULL;
  *r_region = NULL;

  for (area = screen->areabase.first; area; area = area->next) {
    for (region = area->regionbase.first; region; region = region->next) {
      if (region->regiondata == regiondata) {
        *r_area = area;
        *r_region = region;
        return;
      }
    }
  }
}

static void rna_area_region_from_regiondata(PointerRNA *ptr, ScrArea **r_area, ARegion **r_region)
{
  bScreen *screen = (bScreen *)ptr->owner_id;
  void *regiondata = ptr->data;

  area_region_from_regiondata(screen, regiondata, r_area, r_region);
}

/* -------------------------------------------------------------------- */
/** \name Generic Region Flag Access
 * \{ */

static bool rna_Space_bool_from_region_flag_get_by_type(PointerRNA *ptr,
                                                        const int region_type,
                                                        const int region_flag)
{
  ScrArea *area = rna_area_from_space(ptr);
  ARegion *region = BKE_area_find_region_type(area, region_type);
  if (region) {
    return (region->flag & region_flag);
  }
  return false;
}

static void rna_Space_bool_from_region_flag_set_by_type(PointerRNA *ptr,
                                                        const int region_type,
                                                        const int region_flag,
                                                        bool value)
{
  ScrArea *area = rna_area_from_space(ptr);
  ARegion *region = BKE_area_find_region_type(area, region_type);
  if (region && (region->alignment != RGN_ALIGN_NONE)) {
    SET_FLAG_FROM_TEST(region->flag, value, region_flag);
  }
  ED_region_tag_redraw(region);
}

static void rna_Space_bool_from_region_flag_update_by_type(bContext *C,
                                                           PointerRNA *ptr,
                                                           const int region_type,
                                                           const int region_flag)
{
  ScrArea *area = rna_area_from_space(ptr);
  ARegion *region = BKE_area_find_region_type(area, region_type);
  if (region) {
    if (region_flag == RGN_FLAG_HIDDEN) {
      /* Only support animation when the area is in the current context. */
      if (region->overlap && (area == CTX_wm_area(C))) {
        ED_region_visibility_change_update_animated(C, area, region);
      }
      else {
        ED_region_visibility_change_update(C, area, region);
      }
    }
    else if (region_flag == RGN_FLAG_HIDDEN_BY_USER) {
      if (!(region->flag & RGN_FLAG_HIDDEN_BY_USER) != !(region->flag & RGN_FLAG_HIDDEN)) {
        ED_region_toggle_hidden(C, region);

        if ((region->flag & RGN_FLAG_HIDDEN_BY_USER) == 0) {
          ED_area_type_hud_ensure(C, area);
        }
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Region Flag Access (Typed Callbacks)
 * \{ */

/* Header Region. */
static bool rna_Space_show_region_header_get(PointerRNA *ptr)
{
  return !rna_Space_bool_from_region_flag_get_by_type(ptr, RGN_TYPE_HEADER, RGN_FLAG_HIDDEN);
}
static void rna_Space_show_region_header_set(PointerRNA *ptr, bool value)
{
  rna_Space_bool_from_region_flag_set_by_type(ptr, RGN_TYPE_HEADER, RGN_FLAG_HIDDEN, !value);

  /* Special case, never show the tool properties when the header is invisible. */
  bool value_for_tool_header = value;
  if (value == true) {
    ScrArea *area = rna_area_from_space(ptr);
    ARegion *region_tool_header = BKE_area_find_region_type(area, RGN_TYPE_TOOL_HEADER);
    if (region_tool_header != NULL) {
      value_for_tool_header = !(region_tool_header->flag & RGN_FLAG_HIDDEN_BY_USER);
    }
  }
  rna_Space_bool_from_region_flag_set_by_type(
      ptr, RGN_TYPE_TOOL_HEADER, RGN_FLAG_HIDDEN, !value_for_tool_header);
}
static void rna_Space_show_region_header_update(bContext *C, PointerRNA *ptr)
{
  rna_Space_bool_from_region_flag_update_by_type(C, ptr, RGN_TYPE_HEADER, RGN_FLAG_HIDDEN);
}

/* Footer Region. */
static bool rna_Space_show_region_footer_get(PointerRNA *ptr)
{
  return !rna_Space_bool_from_region_flag_get_by_type(ptr, RGN_TYPE_FOOTER, RGN_FLAG_HIDDEN);
}
static void rna_Space_show_region_footer_set(PointerRNA *ptr, bool value)
{
  rna_Space_bool_from_region_flag_set_by_type(ptr, RGN_TYPE_FOOTER, RGN_FLAG_HIDDEN, !value);
}
static void rna_Space_show_region_footer_update(bContext *C, PointerRNA *ptr)
{
  rna_Space_bool_from_region_flag_update_by_type(C, ptr, RGN_TYPE_FOOTER, RGN_FLAG_HIDDEN);
}

/* Tool Header Region.
 *
 * This depends on the 'RGN_TYPE_TOOL_HEADER'
 */
static bool rna_Space_show_region_tool_header_get(PointerRNA *ptr)
{
  return !rna_Space_bool_from_region_flag_get_by_type(
      ptr, RGN_TYPE_TOOL_HEADER, RGN_FLAG_HIDDEN_BY_USER);
}
static void rna_Space_show_region_tool_header_set(PointerRNA *ptr, bool value)
{
  rna_Space_bool_from_region_flag_set_by_type(
      ptr, RGN_TYPE_TOOL_HEADER, RGN_FLAG_HIDDEN_BY_USER, !value);
  rna_Space_bool_from_region_flag_set_by_type(ptr, RGN_TYPE_TOOL_HEADER, RGN_FLAG_HIDDEN, !value);
}
static void rna_Space_show_region_tool_header_update(bContext *C, PointerRNA *ptr)
{
  rna_Space_bool_from_region_flag_update_by_type(C, ptr, RGN_TYPE_TOOL_HEADER, RGN_FLAG_HIDDEN);
}

/* Tools Region. */
static bool rna_Space_show_region_toolbar_get(PointerRNA *ptr)
{
  return !rna_Space_bool_from_region_flag_get_by_type(ptr, RGN_TYPE_TOOLS, RGN_FLAG_HIDDEN);
}
static void rna_Space_show_region_toolbar_set(PointerRNA *ptr, bool value)
{
  rna_Space_bool_from_region_flag_set_by_type(ptr, RGN_TYPE_TOOLS, RGN_FLAG_HIDDEN, !value);
}
static void rna_Space_show_region_toolbar_update(bContext *C, PointerRNA *ptr)
{
  rna_Space_bool_from_region_flag_update_by_type(C, ptr, RGN_TYPE_TOOLS, RGN_FLAG_HIDDEN);
}

/* UI Region */
static bool rna_Space_show_region_ui_get(PointerRNA *ptr)
{
  return !rna_Space_bool_from_region_flag_get_by_type(ptr, RGN_TYPE_UI, RGN_FLAG_HIDDEN);
}
static void rna_Space_show_region_ui_set(PointerRNA *ptr, bool value)
{
  rna_Space_bool_from_region_flag_set_by_type(ptr, RGN_TYPE_UI, RGN_FLAG_HIDDEN, !value);
}
static void rna_Space_show_region_ui_update(bContext *C, PointerRNA *ptr)
{
  rna_Space_bool_from_region_flag_update_by_type(C, ptr, RGN_TYPE_UI, RGN_FLAG_HIDDEN);
}

/* Redo (HUD) Region */
static bool rna_Space_show_region_hud_get(PointerRNA *ptr)
{
  return !rna_Space_bool_from_region_flag_get_by_type(ptr, RGN_TYPE_HUD, RGN_FLAG_HIDDEN_BY_USER);
}
static void rna_Space_show_region_hud_set(PointerRNA *ptr, bool value)
{
  rna_Space_bool_from_region_flag_set_by_type(ptr, RGN_TYPE_HUD, RGN_FLAG_HIDDEN_BY_USER, !value);
}
static void rna_Space_show_region_hud_update(bContext *C, PointerRNA *ptr)
{
  rna_Space_bool_from_region_flag_update_by_type(C, ptr, RGN_TYPE_HUD, RGN_FLAG_HIDDEN_BY_USER);
}

/** \} */

static bool rna_Space_view2d_sync_get(PointerRNA *ptr)
{
  ScrArea *area;
  ARegion *region;

  area = rna_area_from_space(ptr); /* can be NULL */
  region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
  if (region) {
    View2D *v2d = &region->v2d;
    return (v2d->flag & V2D_VIEWSYNC_SCREEN_TIME) != 0;
  }

  return false;
}

static void rna_Space_view2d_sync_set(PointerRNA *ptr, bool value)
{
  ScrArea *area;
  ARegion *region;

  area = rna_area_from_space(ptr); /* can be NULL */
  region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
  if (region) {
    View2D *v2d = &region->v2d;
    if (value) {
      v2d->flag |= V2D_VIEWSYNC_SCREEN_TIME;
    }
    else {
      v2d->flag &= ~V2D_VIEWSYNC_SCREEN_TIME;
    }
  }
}

static void rna_Space_view2d_sync_update(Main *UNUSED(bmain),
                                         Scene *UNUSED(scene),
                                         PointerRNA *ptr)
{
  ScrArea *area;
  ARegion *region;

  area = rna_area_from_space(ptr); /* can be NULL */
  region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);

  if (region) {
    bScreen *screen = (bScreen *)ptr->owner_id;
    View2D *v2d = &region->v2d;

    UI_view2d_sync(screen, area, v2d, V2D_LOCK_SET);
  }
}

static void rna_GPencil_update(Main *bmain, Scene *UNUSED(scene), PointerRNA *UNUSED(ptr))
{
  bool changed = false;
  /* need set all caches as dirty to recalculate onion skinning */
  for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
    if (ob->type == OB_GPENCIL) {
      bGPdata *gpd = (bGPdata *)ob->data;
      DEG_id_tag_update(&gpd->id, ID_RECALC_GEOMETRY);
      changed = true;
    }
  }
  if (changed) {
    WM_main_add_notifier(NC_GPENCIL | NA_EDITED, NULL);
  }
}

/* Space 3D View */
static void rna_SpaceView3D_camera_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  View3D *v3d = (View3D *)(ptr->data);
  if (v3d->scenelock) {
    wmWindowManager *wm = bmain->wm.first;

    scene->camera = v3d->camera;
    WM_windows_scene_data_sync(&wm->windows, scene);
  }
}

static void rna_SpaceView3D_use_local_camera_set(PointerRNA *ptr, bool value)
{
  View3D *v3d = (View3D *)(ptr->data);
  bScreen *screen = (bScreen *)ptr->owner_id;

  v3d->scenelock = !value;

  if (!value) {
    Scene *scene = ED_screen_scene_find(screen, G_MAIN->wm.first);
    /* NULL if the screen isn't in an active window (happens when setting from Python).
     * This could be moved to the update function, in that case the scene wont relate to the screen
     * so keep it working this way. */
    if (scene != NULL) {
      v3d->camera = scene->camera;
    }
  }
}

static float rna_View3DOverlay_GridScaleUnit_get(PointerRNA *ptr)
{
  View3D *v3d = (View3D *)(ptr->data);
  bScreen *screen = (bScreen *)ptr->owner_id;
  Scene *scene = ED_screen_scene_find(screen, G_MAIN->wm.first);
  if (scene != NULL) {
    return ED_view3d_grid_scale(scene, v3d, NULL);
  }
  else {
    /* When accessed from non-active screen. */
    return 1.0f;
  }
}

static PointerRNA rna_SpaceView3D_region_3d_get(PointerRNA *ptr)
{
  View3D *v3d = (View3D *)(ptr->data);
  ScrArea *area = rna_area_from_space(ptr);
  void *regiondata = NULL;
  if (area) {
    ListBase *regionbase = (area->spacedata.first == v3d) ? &area->regionbase : &v3d->regionbase;
    ARegion *region = regionbase->last; /* always last in list, weak .. */
    regiondata = region->regiondata;
  }

  return rna_pointer_inherit_refine(ptr, &RNA_RegionView3D, regiondata);
}

static void rna_SpaceView3D_region_quadviews_begin(CollectionPropertyIterator *iter,
                                                   PointerRNA *ptr)
{
  View3D *v3d = (View3D *)(ptr->data);
  ScrArea *area = rna_area_from_space(ptr);
  int i = 3;

  ARegion *region =
      ((area && area->spacedata.first == v3d) ? &area->regionbase : &v3d->regionbase)->last;
  ListBase lb = {NULL, NULL};

  if (region && region->alignment == RGN_ALIGN_QSPLIT) {
    while (i-- && region) {
      region = region->prev;
    }

    if (i < 0) {
      lb.first = region;
    }
  }

  rna_iterator_listbase_begin(iter, &lb, NULL);
}

static PointerRNA rna_SpaceView3D_region_quadviews_get(CollectionPropertyIterator *iter)
{
  void *regiondata = ((ARegion *)rna_iterator_listbase_get(iter))->regiondata;

  return rna_pointer_inherit_refine(&iter->parent, &RNA_RegionView3D, regiondata);
}

static void rna_RegionView3D_quadview_update(Main *UNUSED(main),
                                             Scene *UNUSED(scene),
                                             PointerRNA *ptr)
{
  ScrArea *area;
  ARegion *region;

  rna_area_region_from_regiondata(ptr, &area, &region);
  if (area && region && region->alignment == RGN_ALIGN_QSPLIT) {
    ED_view3d_quadview_update(area, region, false);
  }
}

/* same as above but call clip==true */
static void rna_RegionView3D_quadview_clip_update(Main *UNUSED(main),
                                                  Scene *UNUSED(scene),
                                                  PointerRNA *ptr)
{
  ScrArea *area;
  ARegion *region;

  rna_area_region_from_regiondata(ptr, &area, &region);
  if (area && region && region->alignment == RGN_ALIGN_QSPLIT) {
    ED_view3d_quadview_update(area, region, true);
  }
}

static void rna_RegionView3D_view_location_get(PointerRNA *ptr, float *values)
{
  RegionView3D *rv3d = (RegionView3D *)(ptr->data);
  negate_v3_v3(values, rv3d->ofs);
}

static void rna_RegionView3D_view_location_set(PointerRNA *ptr, const float *values)
{
  RegionView3D *rv3d = (RegionView3D *)(ptr->data);
  negate_v3_v3(rv3d->ofs, values);
}

static void rna_RegionView3D_view_rotation_get(PointerRNA *ptr, float *values)
{
  RegionView3D *rv3d = (RegionView3D *)(ptr->data);
  invert_qt_qt(values, rv3d->viewquat);
}

static void rna_RegionView3D_view_rotation_set(PointerRNA *ptr, const float *values)
{
  RegionView3D *rv3d = (RegionView3D *)(ptr->data);
  invert_qt_qt(rv3d->viewquat, values);
}

static void rna_RegionView3D_view_matrix_set(PointerRNA *ptr, const float *values)
{
  RegionView3D *rv3d = (RegionView3D *)(ptr->data);
  float mat[4][4];
  invert_m4_m4(mat, (float(*)[4])values);
  ED_view3d_from_m4(mat, rv3d->ofs, rv3d->viewquat, &rv3d->dist);
}

static bool rna_RegionView3D_is_orthographic_side_view_get(PointerRNA *ptr)
{
  RegionView3D *rv3d = (RegionView3D *)(ptr->data);
  return RV3D_VIEW_IS_AXIS(rv3d->view);
}

static IDProperty *rna_View3DShading_idprops(PointerRNA *ptr, bool create)
{
  View3DShading *shading = ptr->data;

  if (create && !shading->prop) {
    IDPropertyTemplate val = {0};
    shading->prop = IDP_New(IDP_GROUP, &val, "View3DShading ID properties");
  }

  return shading->prop;
}

static void rna_3DViewShading_type_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  ID *id = ptr->owner_id;
  if (GS(id->name) != ID_SCR) {
    return;
  }

  View3DShading *shading = ptr->data;
  if (shading->type == OB_MATERIAL ||
      (shading->type == OB_RENDER && !STREQ(scene->r.engine, RE_engine_id_BLENDER_WORKBENCH))) {
    /* When switching from workbench to render or material mode the geometry of any
     * active sculpt session needs to be recalculated. */
    for (Object *ob = bmain->objects.first; ob; ob = ob->id.next) {
      if (ob->sculpt) {
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      }
    }
  }

  /* Update Gpencil. */
  rna_GPencil_update(bmain, scene, ptr);

  bScreen *screen = (bScreen *)ptr->owner_id;
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
      if (sl->spacetype == SPACE_VIEW3D) {
        View3D *v3d = (View3D *)sl;
        if (&v3d->shading == shading) {
          ED_view3d_shade_update(bmain, v3d, area);
          return;
        }
      }
    }
  }
}

static Scene *rna_3DViewShading_scene(PointerRNA *ptr)
{
  /* Get scene, depends if using 3D view or OpenGL render settings. */
  ID *id = ptr->owner_id;
  if (GS(id->name) == ID_SCE) {
    return (Scene *)id;
  }
  else {
    bScreen *screen = (bScreen *)ptr->owner_id;
    return WM_windows_scene_get_from_screen(G_MAIN->wm.first, screen);
  }
}

static ViewLayer *rna_3DViewShading_view_layer(PointerRNA *ptr)
{
  /* Get scene, depends if using 3D view or OpenGL render settings. */
  ID *id = ptr->owner_id;
  if (GS(id->name) == ID_SCE) {
    return NULL;
  }
  else {
    bScreen *screen = (bScreen *)ptr->owner_id;
    return WM_windows_view_layer_get_from_screen(G_MAIN->wm.first, screen);
  }
}

static int rna_3DViewShading_type_get(PointerRNA *ptr)
{
  /* Available shading types depend on render engine. */
  Scene *scene = rna_3DViewShading_scene(ptr);
  RenderEngineType *type = (scene) ? RE_engines_find(scene->r.engine) : NULL;
  View3DShading *shading = (View3DShading *)ptr->data;

  if (scene == NULL || BKE_scene_uses_blender_eevee(scene)) {
    return shading->type;
  }
  else if (BKE_scene_uses_blender_workbench(scene)) {
    return (shading->type == OB_MATERIAL) ? OB_SOLID : shading->type;
  }
  else {
    if (shading->type == OB_RENDER && !(type && type->view_draw)) {
      return OB_MATERIAL;
    }
    else {
      return shading->type;
    }
  }
}

static void rna_3DViewShading_type_set(PointerRNA *ptr, int value)
{
  View3DShading *shading = (View3DShading *)ptr->data;
  if (value != shading->type && value == OB_RENDER) {
    shading->prev_type = shading->type;
  }
  shading->type = value;
}

static const EnumPropertyItem *rna_3DViewShading_type_itemf(bContext *UNUSED(C),
                                                            PointerRNA *ptr,
                                                            PropertyRNA *UNUSED(prop),
                                                            bool *r_free)
{
  Scene *scene = rna_3DViewShading_scene(ptr);
  RenderEngineType *type = (scene) ? RE_engines_find(scene->r.engine) : NULL;

  EnumPropertyItem *item = NULL;
  int totitem = 0;

  RNA_enum_items_add_value(&item, &totitem, rna_enum_shading_type_items, OB_WIRE);
  RNA_enum_items_add_value(&item, &totitem, rna_enum_shading_type_items, OB_SOLID);

  if (scene == NULL || BKE_scene_uses_blender_eevee(scene)) {
    RNA_enum_items_add_value(&item, &totitem, rna_enum_shading_type_items, OB_MATERIAL);
    RNA_enum_items_add_value(&item, &totitem, rna_enum_shading_type_items, OB_RENDER);
  }
  else if (BKE_scene_uses_blender_workbench(scene)) {
    RNA_enum_items_add_value(&item, &totitem, rna_enum_shading_type_items, OB_RENDER);
  }
  else {
    RNA_enum_items_add_value(&item, &totitem, rna_enum_shading_type_items, OB_MATERIAL);
    if (type && type->view_draw) {
      RNA_enum_items_add_value(&item, &totitem, rna_enum_shading_type_items, OB_RENDER);
    }
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

/* Shading.selected_studio_light */
static PointerRNA rna_View3DShading_selected_studio_light_get(PointerRNA *ptr)
{
  View3DShading *shading = (View3DShading *)ptr->data;
  StudioLight *sl;
  if (shading->type == OB_SOLID && shading->light == V3D_LIGHTING_MATCAP) {
    sl = BKE_studiolight_find(shading->matcap, STUDIOLIGHT_FLAG_ALL);
  }
  else if (shading->type == OB_SOLID && shading->light == V3D_LIGHTING_STUDIO) {
    sl = BKE_studiolight_find(shading->studio_light, STUDIOLIGHT_FLAG_ALL);
  }
  else {
    /* OB_MATERIAL and OB_RENDER */
    sl = BKE_studiolight_find(shading->lookdev_light, STUDIOLIGHT_FLAG_ALL);
  }
  return rna_pointer_inherit_refine(ptr, &RNA_StudioLight, sl);
}

/* shading.light */
static const EnumPropertyItem *rna_View3DShading_color_type_itemf(bContext *UNUSED(C),
                                                                  PointerRNA *ptr,
                                                                  PropertyRNA *UNUSED(prop),
                                                                  bool *r_free)
{
  View3DShading *shading = (View3DShading *)ptr->data;

  int totitem = 0;

  if (shading->type == OB_WIRE) {
    EnumPropertyItem *item = NULL;
    RNA_enum_items_add_value(
        &item, &totitem, rna_enum_shading_color_type_items, V3D_SHADING_SINGLE_COLOR);
    RNA_enum_items_add_value(
        &item, &totitem, rna_enum_shading_color_type_items, V3D_SHADING_OBJECT_COLOR);
    RNA_enum_items_add_value(
        &item, &totitem, rna_enum_shading_color_type_items, V3D_SHADING_RANDOM_COLOR);
    RNA_enum_item_end(&item, &totitem);
    *r_free = true;
    return item;
  }
  else {
    /* Solid mode, or lookdev mode for workbench engine. */
    *r_free = false;
    return rna_enum_shading_color_type_items;
  }
}

static void rna_View3DShading_studio_light_get_storage(View3DShading *shading,
                                                       char **dna_storage,
                                                       int *flag)
{
  *dna_storage = shading->studio_light;

  *flag = STUDIOLIGHT_TYPE_STUDIO;
  if (shading->type == OB_SOLID) {
    if (shading->light == V3D_LIGHTING_MATCAP) {
      *flag = STUDIOLIGHT_TYPE_MATCAP;
      *dna_storage = shading->matcap;
    }
  }
  else {
    *flag = STUDIOLIGHT_TYPE_WORLD;
    *dna_storage = shading->lookdev_light;
  }
}

static int rna_View3DShading_studio_light_get(PointerRNA *ptr)
{
  View3DShading *shading = (View3DShading *)ptr->data;
  char *dna_storage;
  int flag;

  rna_View3DShading_studio_light_get_storage(shading, &dna_storage, &flag);
  StudioLight *sl = BKE_studiolight_find(dna_storage, flag);
  if (sl) {
    BLI_strncpy(dna_storage, sl->name, FILE_MAXFILE);
    return sl->index;
  }
  else {
    return 0;
  }
}

static void rna_View3DShading_studio_light_set(PointerRNA *ptr, int value)
{
  View3DShading *shading = (View3DShading *)ptr->data;
  char *dna_storage;
  int flag;

  rna_View3DShading_studio_light_get_storage(shading, &dna_storage, &flag);
  StudioLight *sl = BKE_studiolight_findindex(value, flag);
  if (sl) {
    BLI_strncpy(dna_storage, sl->name, FILE_MAXFILE);
  }
}

static const EnumPropertyItem *rna_View3DShading_studio_light_itemf(bContext *UNUSED(C),
                                                                    PointerRNA *ptr,
                                                                    PropertyRNA *UNUSED(prop),
                                                                    bool *r_free)
{
  View3DShading *shading = (View3DShading *)ptr->data;
  EnumPropertyItem *item = NULL;
  int totitem = 0;

  if (shading->type == OB_SOLID && shading->light == V3D_LIGHTING_MATCAP) {
    const int flags = (STUDIOLIGHT_EXTERNAL_FILE | STUDIOLIGHT_TYPE_MATCAP);

    LISTBASE_FOREACH (StudioLight *, sl, BKE_studiolight_listbase()) {
      int icon_id = (shading->flag & V3D_SHADING_MATCAP_FLIP_X) ? sl->icon_id_matcap_flipped :
                                                                  sl->icon_id_matcap;
      if ((sl->flag & flags) == flags) {
        EnumPropertyItem tmp = {sl->index, sl->name, icon_id, sl->name, ""};
        RNA_enum_item_add(&item, &totitem, &tmp);
      }
    }
  }
  else {
    LISTBASE_FOREACH (StudioLight *, sl, BKE_studiolight_listbase()) {
      int icon_id = sl->icon_id_irradiance;
      bool show_studiolight = false;

      if (sl->flag & STUDIOLIGHT_INTERNAL) {
        /* always show internal lights for solid */
        if (shading->type == OB_SOLID) {
          show_studiolight = true;
        }
      }
      else {
        switch (shading->type) {
          case OB_SOLID:
          case OB_TEXTURE:
            show_studiolight = ((sl->flag & STUDIOLIGHT_TYPE_STUDIO) != 0);
            break;

          case OB_MATERIAL:
          case OB_RENDER:
            show_studiolight = ((sl->flag & STUDIOLIGHT_TYPE_WORLD) != 0);
            icon_id = sl->icon_id_radiance;
            break;
        }
      }

      if (show_studiolight) {
        EnumPropertyItem tmp = {sl->index, sl->name, icon_id, sl->name, ""};
        RNA_enum_item_add(&item, &totitem, &tmp);
      }
    }
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;
  return item;
}

static const EnumPropertyItem *rna_3DViewShading_render_pass_itemf(bContext *C,
                                                                   PointerRNA *UNUSED(ptr),
                                                                   PropertyRNA *UNUSED(prop),
                                                                   bool *r_free)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  const bool bloom_enabled = scene->eevee.flag & SCE_EEVEE_BLOOM_ENABLED;
  const bool aov_available = BKE_view_layer_has_valid_aov(view_layer);

  int totitem = 0;
  EnumPropertyItem *result = NULL;
  EnumPropertyItem aov_template;
  for (int i = 0; rna_enum_view3dshading_render_pass_type_items[i].identifier != NULL; i++) {
    const EnumPropertyItem *item = &rna_enum_view3dshading_render_pass_type_items[i];
    if (item->value == EEVEE_RENDER_PASS_AOV) {
      aov_template.value = item->value;
      aov_template.icon = 0;
      aov_template.description = item->description;
      LISTBASE_FOREACH (ViewLayerAOV *, aov, &view_layer->aovs) {
        if ((aov->flag & AOV_CONFLICT) != 0) {
          continue;
        }
        aov_template.name = aov->name;
        aov_template.identifier = aov->name;
        RNA_enum_item_add(&result, &totitem, &aov_template);
        aov_template.value++;
      }
    }
    else if (!((!bloom_enabled &&
                (item->value == EEVEE_RENDER_PASS_BLOOM || STREQ(item->name, "Effects"))) ||
               (!aov_available && STREQ(item->name, "Shader AOV")))) {
      RNA_enum_item_add(&result, &totitem, item);
    }
  }

  RNA_enum_item_end(&result, &totitem);
  *r_free = true;
  return result;
}
static int rna_3DViewShading_render_pass_get(PointerRNA *ptr)
{
  View3DShading *shading = (View3DShading *)ptr->data;
  eViewLayerEEVEEPassType result = shading->render_pass;
  Scene *scene = rna_3DViewShading_scene(ptr);
  ViewLayer *view_layer = rna_3DViewShading_view_layer(ptr);

  if (result == EEVEE_RENDER_PASS_BLOOM && ((scene->eevee.flag & SCE_EEVEE_BLOOM_ENABLED) == 0)) {
    return EEVEE_RENDER_PASS_COMBINED;
  }
  else if (result == EEVEE_RENDER_PASS_AOV) {
    if (!view_layer) {
      return EEVEE_RENDER_PASS_COMBINED;
    }
    const int aov_index = BLI_findstringindex(
        &view_layer->aovs, shading->aov_name, offsetof(ViewLayerAOV, name));
    if (aov_index == -1) {
      return EEVEE_RENDER_PASS_COMBINED;
    }
    return result + aov_index;
  }

  return result;
}

static void rna_3DViewShading_render_pass_set(PointerRNA *ptr, int value)
{
  View3DShading *shading = (View3DShading *)ptr->data;
  Scene *scene = rna_3DViewShading_scene(ptr);
  ViewLayer *view_layer = rna_3DViewShading_view_layer(ptr);
  shading->aov_name[0] = 0;

  if ((value & EEVEE_RENDER_PASS_AOV) != 0) {
    if (!view_layer) {
      shading->render_pass = EEVEE_RENDER_PASS_COMBINED;
      return;
    }
    const int aov_index = value & ~EEVEE_RENDER_PASS_AOV;
    ViewLayerAOV *aov = BLI_findlink(&view_layer->aovs, aov_index);
    if (!aov) {
      /* AOV not found, cannot select AOV. */
      shading->render_pass = EEVEE_RENDER_PASS_COMBINED;
      return;
    }

    shading->render_pass = EEVEE_RENDER_PASS_AOV;
    BLI_strncpy(shading->aov_name, aov->name, sizeof(aov->name));
  }
  else if (value == EEVEE_RENDER_PASS_BLOOM &&
           ((scene->eevee.flag & SCE_EEVEE_BLOOM_ENABLED) == 0)) {
    shading->render_pass = EEVEE_RENDER_PASS_COMBINED;
  }
  else {
    shading->render_pass = value;
  }
}

static void rna_SpaceView3D_use_local_collections_update(bContext *C, PointerRNA *ptr)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  View3D *v3d = (View3D *)ptr->data;

  if (ED_view3d_local_collections_set(bmain, v3d)) {
    BKE_layer_collection_local_sync(view_layer, v3d);
    DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  }
}

static const EnumPropertyItem *rna_SpaceView3D_stereo3d_camera_itemf(bContext *C,
                                                                     PointerRNA *UNUSED(ptr),
                                                                     PropertyRNA *UNUSED(prop),
                                                                     bool *UNUSED(r_free))
{
  Scene *scene = CTX_data_scene(C);

  if (scene->r.views_format == SCE_VIEWS_FORMAT_MULTIVIEW) {
    return multiview_camera_items;
  }
  else {
    return stereo3d_camera_items;
  }
}

static void rna_SpaceView3D_mirror_xr_session_update(Main *main,
                                                     Scene *UNUSED(scene),
                                                     PointerRNA *ptr)
{
#  ifdef WITH_XR_OPENXR
  const wmWindowManager *wm = main->wm.first;

  /* Handle mirror toggling while there is a session already. */
  if (WM_xr_session_exists(&wm->xr)) {
    const View3D *v3d = ptr->data;
    const ScrArea *area = rna_area_from_space(ptr);
    ED_view3d_xr_mirror_update(area, v3d, v3d->flag & V3D_XR_SESSION_MIRROR);
  }

#  else
  UNUSED_VARS(main, ptr);
#  endif
}

static int rna_SpaceView3D_icon_from_show_object_viewport_get(PointerRNA *ptr)
{
  const View3D *v3d = (View3D *)ptr->data;
  /* Ignore selection values when view is off,
   * intent is to show if visible objects aren't selectable. */
  const int view_value = (v3d->object_type_exclude_viewport != 0);
  const int select_value = (v3d->object_type_exclude_select &
                            ~v3d->object_type_exclude_viewport) != 0;
  return ICON_VIS_SEL_11 + (view_value << 1) + select_value;
}

static char *rna_View3DShading_path(PointerRNA *UNUSED(ptr))
{
  return BLI_strdup("shading");
}

static PointerRNA rna_SpaceView3D_overlay_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_View3DOverlay, ptr->data);
}

static char *rna_View3DOverlay_path(PointerRNA *UNUSED(ptr))
{
  return BLI_strdup("overlay");
}

/* Space Image Editor */

static PointerRNA rna_SpaceImage_overlay_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_SpaceImageOverlay, ptr->data);
}

static char *rna_SpaceImageOverlay_path(PointerRNA *UNUSED(ptr))
{
  return BLI_strdup("overlay");
}

static char *rna_SpaceUVEditor_path(PointerRNA *UNUSED(ptr))
{
  return BLI_strdup("uv_editor");
}

static PointerRNA rna_SpaceImageEditor_uvedit_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_SpaceUVEditor, ptr->data);
}

static void rna_SpaceImageEditor_mode_update(Main *bmain, Scene *scene, PointerRNA *UNUSED(ptr))
{
  ED_space_image_paint_update(bmain, bmain->wm.first, scene);
}

static void rna_SpaceImageEditor_show_stereo_set(PointerRNA *ptr, int value)
{
  SpaceImage *sima = (SpaceImage *)(ptr->data);

  if (value) {
    sima->iuser.flag |= IMA_SHOW_STEREO;
  }
  else {
    sima->iuser.flag &= ~IMA_SHOW_STEREO;
  }
}

static bool rna_SpaceImageEditor_show_stereo_get(PointerRNA *ptr)
{
  SpaceImage *sima = (SpaceImage *)(ptr->data);
  return (sima->iuser.flag & IMA_SHOW_STEREO) != 0;
}

static void rna_SpaceImageEditor_show_stereo_update(Main *UNUSED(bmain),
                                                    Scene *UNUSED(unused),
                                                    PointerRNA *ptr)
{
  SpaceImage *sima = (SpaceImage *)(ptr->data);
  Image *ima = sima->image;

  if (ima) {
    if (ima->rr) {
      BKE_image_multilayer_index(ima->rr, &sima->iuser);
    }
    else {
      BKE_image_multiview_index(ima, &sima->iuser);
    }
  }
}

static bool rna_SpaceImageEditor_show_render_get(PointerRNA *ptr)
{
  SpaceImage *sima = (SpaceImage *)(ptr->data);
  return ED_space_image_show_render(sima);
}

static bool rna_SpaceImageEditor_show_paint_get(PointerRNA *ptr)
{
  SpaceImage *sima = (SpaceImage *)(ptr->data);
  return ED_space_image_show_paint(sima);
}

static bool rna_SpaceImageEditor_show_uvedit_get(PointerRNA *ptr)
{
  SpaceImage *sima = ptr->data;
  bScreen *screen = (bScreen *)ptr->owner_id;
  Object *obedit = NULL;
  wmWindow *win = ED_screen_window_find(screen, G_MAIN->wm.first);
  if (win != NULL) {
    ViewLayer *view_layer = WM_window_get_active_view_layer(win);
    obedit = OBEDIT_FROM_VIEW_LAYER(view_layer);
  }
  return ED_space_image_show_uvedit(sima, obedit);
}

static bool rna_SpaceImageEditor_show_maskedit_get(PointerRNA *ptr)
{
  SpaceImage *sima = (SpaceImage *)(ptr->data);
  bScreen *screen = (bScreen *)ptr->owner_id;
  Object *obedit = NULL;
  wmWindow *win = ED_screen_window_find(screen, G_MAIN->wm.first);
  if (win != NULL) {
    ViewLayer *view_layer = WM_window_get_active_view_layer(win);
    obedit = OBEDIT_FROM_VIEW_LAYER(view_layer);
  }
  return ED_space_image_check_show_maskedit(sima, obedit);
}

static void rna_SpaceImageEditor_image_set(PointerRNA *ptr,
                                           PointerRNA value,
                                           struct ReportList *UNUSED(reports))
{
  BLI_assert(BKE_id_is_in_global_main(value.data));
  SpaceImage *sima = ptr->data;
  bScreen *screen = (bScreen *)ptr->owner_id;
  Object *obedit = NULL;
  wmWindow *win = ED_screen_window_find(screen, G_MAIN->wm.first);
  if (win != NULL) {
    ViewLayer *view_layer = WM_window_get_active_view_layer(win);
    obedit = OBEDIT_FROM_VIEW_LAYER(view_layer);
  }
  ED_space_image_set(G_MAIN, sima, obedit, (Image *)value.data, false);
}

static void rna_SpaceImageEditor_mask_set(PointerRNA *ptr,
                                          PointerRNA value,
                                          struct ReportList *UNUSED(reports))
{
  SpaceImage *sima = (SpaceImage *)(ptr->data);

  ED_space_image_set_mask(NULL, sima, (Mask *)value.data);
}

static const EnumPropertyItem *rna_SpaceImageEditor_display_channels_itemf(
    bContext *UNUSED(C), PointerRNA *ptr, PropertyRNA *UNUSED(prop), bool *r_free)
{
  SpaceImage *sima = (SpaceImage *)ptr->data;
  EnumPropertyItem *item = NULL;
  ImBuf *ibuf;
  void *lock;
  int totitem = 0;

  ibuf = ED_space_image_acquire_buffer(sima, &lock, 0);
  int mask = ED_space_image_get_display_channel_mask(ibuf);
  ED_space_image_release_buffer(sima, ibuf, lock);

  if (mask & SI_USE_ALPHA) {
    RNA_enum_items_add_value(&item, &totitem, display_channels_items, SI_USE_ALPHA);
  }
  RNA_enum_items_add_value(&item, &totitem, display_channels_items, 0);
  if (mask & SI_SHOW_ALPHA) {
    RNA_enum_items_add_value(&item, &totitem, display_channels_items, SI_SHOW_ALPHA);
  }
  if (mask & SI_SHOW_ZBUF) {
    RNA_enum_items_add_value(&item, &totitem, display_channels_items, SI_SHOW_ZBUF);
  }
  if (mask & SI_SHOW_R) {
    RNA_enum_items_add_value(&item, &totitem, display_channels_items, SI_SHOW_R);
  }
  if (mask & SI_SHOW_G) {
    RNA_enum_items_add_value(&item, &totitem, display_channels_items, SI_SHOW_G);
  }
  if (mask & SI_SHOW_B) {
    RNA_enum_items_add_value(&item, &totitem, display_channels_items, SI_SHOW_B);
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static int rna_SpaceImageEditor_display_channels_get(PointerRNA *ptr)
{
  SpaceImage *sima = (SpaceImage *)ptr->data;
  ImBuf *ibuf;
  void *lock;

  ibuf = ED_space_image_acquire_buffer(sima, &lock, 0);
  int mask = ED_space_image_get_display_channel_mask(ibuf);
  ED_space_image_release_buffer(sima, ibuf, lock);

  return sima->flag & mask;
}

static void rna_SpaceImageEditor_zoom_get(PointerRNA *ptr, float *values)
{
  SpaceImage *sima = (SpaceImage *)ptr->data;
  ScrArea *area;
  ARegion *region;

  values[0] = values[1] = 1;

  /* find aregion */
  area = rna_area_from_space(ptr); /* can be NULL */
  region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
  if (region) {
    ED_space_image_get_zoom(sima, region, &values[0], &values[1]);
  }
}

static void rna_SpaceImageEditor_cursor_location_get(PointerRNA *ptr, float *values)
{
  SpaceImage *sima = (SpaceImage *)ptr->data;

  if (sima->flag & SI_COORDFLOATS) {
    copy_v2_v2(values, sima->cursor);
  }
  else {
    int w, h;
    ED_space_image_get_size(sima, &w, &h);

    values[0] = sima->cursor[0] * w;
    values[1] = sima->cursor[1] * h;
  }
}

static void rna_SpaceImageEditor_cursor_location_set(PointerRNA *ptr, const float *values)
{
  SpaceImage *sima = (SpaceImage *)ptr->data;

  if (sima->flag & SI_COORDFLOATS) {
    copy_v2_v2(sima->cursor, values);
  }
  else {
    int w, h;
    ED_space_image_get_size(sima, &w, &h);

    sima->cursor[0] = values[0] / w;
    sima->cursor[1] = values[1] / h;
  }
}

static void rna_SpaceImageEditor_image_update(Main *UNUSED(bmain),
                                              Scene *UNUSED(scene),
                                              PointerRNA *ptr)
{
  SpaceImage *sima = (SpaceImage *)ptr->data;
  Image *ima = sima->image;

  /* make sure all the iuser settings are valid for the sima image */
  if (ima) {
    if (ima->rr) {
      if (BKE_image_multilayer_index(sima->image->rr, &sima->iuser) == NULL) {
        BKE_image_init_imageuser(sima->image, &sima->iuser);
      }
    }
    else {
      BKE_image_multiview_index(ima, &sima->iuser);
    }
  }
}

static void rna_SpaceImageEditor_scopes_update(struct bContext *C, struct PointerRNA *ptr)
{
  SpaceImage *sima = (SpaceImage *)ptr->data;
  ImBuf *ibuf;
  void *lock;

  /* TODO(lukas): Support tiles in scopes? */
  ibuf = ED_space_image_acquire_buffer(sima, &lock, 0);
  if (ibuf) {
    ED_space_image_scopes_update(C, sima, ibuf, true);
    WM_main_add_notifier(NC_IMAGE, sima->image);
  }
  ED_space_image_release_buffer(sima, ibuf, lock);
}

static const EnumPropertyItem *rna_SpaceImageEditor_pivot_itemf(bContext *UNUSED(C),
                                                                PointerRNA *ptr,
                                                                PropertyRNA *UNUSED(prop),
                                                                bool *UNUSED(r_free))
{
  static const EnumPropertyItem pivot_items[] = {
      {V3D_AROUND_CENTER_BOUNDS, "CENTER", ICON_PIVOT_BOUNDBOX, "Bounding Box Center", ""},
      {V3D_AROUND_CENTER_MEDIAN, "MEDIAN", ICON_PIVOT_MEDIAN, "Median Point", ""},
      {V3D_AROUND_CURSOR, "CURSOR", ICON_PIVOT_CURSOR, "2D Cursor", ""},
      {V3D_AROUND_LOCAL_ORIGINS,
       "INDIVIDUAL_ORIGINS",
       ICON_PIVOT_INDIVIDUAL,
       "Individual Origins",
       "Pivot around each selected island's own median point"},
      {0, NULL, 0, NULL, NULL},
  };

  SpaceImage *sima = (SpaceImage *)ptr->data;

  if (sima->mode == SI_MODE_PAINT) {
    return rna_enum_transform_pivot_items_full;
  }
  else {
    return pivot_items;
  }
}

/* Space Text Editor */

static void rna_SpaceTextEditor_word_wrap_set(PointerRNA *ptr, bool value)
{
  SpaceText *st = (SpaceText *)(ptr->data);

  st->wordwrap = value;
  st->left = 0;
}

static void rna_SpaceTextEditor_text_set(PointerRNA *ptr,
                                         PointerRNA value,
                                         struct ReportList *UNUSED(reports))
{
  SpaceText *st = (SpaceText *)(ptr->data);

  st->text = value.data;

  WM_main_add_notifier(NC_TEXT | NA_SELECTED, st->text);
}

static bool rna_SpaceTextEditor_text_is_syntax_highlight_supported(struct SpaceText *space)
{
  return ED_text_is_syntax_highlight_supported(space->text);
}

static void rna_SpaceTextEditor_updateEdited(Main *UNUSED(bmain),
                                             Scene *UNUSED(scene),
                                             PointerRNA *ptr)
{
  SpaceText *st = (SpaceText *)ptr->data;

  if (st->text) {
    WM_main_add_notifier(NC_TEXT | NA_EDITED, st->text);
  }
}

/* Space Properties */

/* note: this function exists only to avoid id refcounting */
static void rna_SpaceProperties_pin_id_set(PointerRNA *ptr,
                                           PointerRNA value,
                                           struct ReportList *UNUSED(reports))
{
  SpaceProperties *sbuts = (SpaceProperties *)(ptr->data);
  sbuts->pinid = value.data;
}

static StructRNA *rna_SpaceProperties_pin_id_typef(PointerRNA *ptr)
{
  SpaceProperties *sbuts = (SpaceProperties *)(ptr->data);

  if (sbuts->pinid) {
    return ID_code_to_RNA_type(GS(sbuts->pinid->name));
  }

  return &RNA_ID;
}

static void rna_SpaceProperties_pin_id_update(Main *UNUSED(bmain),
                                              Scene *UNUSED(scene),
                                              PointerRNA *ptr)
{
  SpaceProperties *sbuts = (SpaceProperties *)(ptr->data);
  ID *id = sbuts->pinid;

  if (id == NULL) {
    sbuts->flag &= ~SB_PIN_CONTEXT;
    return;
  }

  switch (GS(id->name)) {
    case ID_MA:
      WM_main_add_notifier(NC_MATERIAL | ND_SHADING, NULL);
      break;
    case ID_TE:
      WM_main_add_notifier(NC_TEXTURE, NULL);
      break;
    case ID_WO:
      WM_main_add_notifier(NC_WORLD, NULL);
      break;
    case ID_LA:
      WM_main_add_notifier(NC_LAMP, NULL);
      break;
    default:
      break;
  }
}

static void rna_SpaceProperties_context_set(PointerRNA *ptr, int value)
{
  SpaceProperties *sbuts = (SpaceProperties *)(ptr->data);

  sbuts->mainb = value;
  sbuts->mainbuser = value;
}

static const EnumPropertyItem *rna_SpaceProperties_context_itemf(bContext *UNUSED(C),
                                                                 PointerRNA *ptr,
                                                                 PropertyRNA *UNUSED(prop),
                                                                 bool *r_free)
{
  SpaceProperties *sbuts = (SpaceProperties *)(ptr->data);
  EnumPropertyItem *item = NULL;

  /* Although it would never reach this amount, a theoretical maximum number of tabs
   * is BCONTEXT_TOT * 2, with every tab displayed and a spacer in every other item. */
  short context_tabs_array[BCONTEXT_TOT * 2];
  int totitem = ED_buttons_tabs_list(sbuts, context_tabs_array);
  BLI_assert(totitem <= ARRAY_SIZE(context_tabs_array));

  int totitem_added = 0;
  for (int i = 0; i < totitem; i++) {
    if (context_tabs_array[i] == -1) {
      RNA_enum_item_add_separator(&item, &totitem_added);
      continue;
    }

    RNA_enum_items_add_value(&item, &totitem_added, buttons_context_items, context_tabs_array[i]);

    /* Add the object data icon dynamically for the data tab. */
    if (context_tabs_array[i] == BCONTEXT_DATA) {
      (item + totitem_added - 1)->icon = sbuts->dataicon;
    }
  }

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;

  return item;
}

static void rna_SpaceProperties_context_update(Main *UNUSED(bmain),
                                               Scene *UNUSED(scene),
                                               PointerRNA *ptr)
{
  SpaceProperties *sbuts = (SpaceProperties *)(ptr->data);
  /* XXX BCONTEXT_DATA is ugly, but required for lights... See T51318. */
  if (ELEM(sbuts->mainb, BCONTEXT_WORLD, BCONTEXT_MATERIAL, BCONTEXT_TEXTURE, BCONTEXT_DATA)) {
    sbuts->preview = 1;
  }
}

static int rna_SpaceProperties_tab_search_results_getlength(PointerRNA *ptr,
                                                            int length[RNA_MAX_ARRAY_DIMENSION])
{
  SpaceProperties *sbuts = ptr->data;

  short context_tabs_array[BCONTEXT_TOT * 2]; /* Dummy variable. */
  const int tabs_len = ED_buttons_tabs_list(sbuts, context_tabs_array);

  length[0] = tabs_len;

  return length[0];
}

static void rna_SpaceProperties_tab_search_results_get(PointerRNA *ptr, bool *values)
{
  SpaceProperties *sbuts = ptr->data;

  short context_tabs_array[BCONTEXT_TOT * 2]; /* Dummy variable. */
  const int tabs_len = ED_buttons_tabs_list(sbuts, context_tabs_array);

  for (int i = 0; i < tabs_len; i++) {
    values[i] = ED_buttons_tab_has_search_result(sbuts, i);
  }
}

static void rna_SpaceProperties_search_filter_get(PointerRNA *ptr, char *value)
{
  SpaceProperties *sbuts = ptr->data;
  const char *search_filter = ED_buttons_search_string_get(sbuts);

  strcpy(value, search_filter);
}

static int rna_SpaceProperties_search_filter_length(PointerRNA *ptr)
{
  SpaceProperties *sbuts = ptr->data;

  return ED_buttons_search_string_length(sbuts);
}

static void rna_SpaceProperties_search_filter_set(struct PointerRNA *ptr, const char *value)
{
  SpaceProperties *sbuts = ptr->data;

  ED_buttons_search_string_set(sbuts, value);
}

static void rna_SpaceProperties_search_filter_update(Main *UNUSED(bmain),
                                                     Scene *UNUSED(scene),
                                                     PointerRNA *ptr)
{
  ScrArea *area = rna_area_from_space(ptr);

  /* Update the search filter flag for the main region with the panels. */
  ARegion *main_region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
  BLI_assert(main_region != NULL);
  ED_region_search_filter_update(area, main_region);
}

/* Space Console */
static void rna_ConsoleLine_body_get(PointerRNA *ptr, char *value)
{
  ConsoleLine *ci = (ConsoleLine *)ptr->data;
  memcpy(value, ci->line, ci->len + 1);
}

static int rna_ConsoleLine_body_length(PointerRNA *ptr)
{
  ConsoleLine *ci = (ConsoleLine *)ptr->data;
  return ci->len;
}

static void rna_ConsoleLine_body_set(PointerRNA *ptr, const char *value)
{
  ConsoleLine *ci = (ConsoleLine *)ptr->data;
  int len = strlen(value);

  if ((len >= ci->len_alloc) || (len * 2 < ci->len_alloc)) { /* allocate a new string */
    MEM_freeN(ci->line);
    ci->line = MEM_mallocN((len + 1) * sizeof(char), "rna_consoleline");
    ci->len_alloc = len + 1;
  }
  memcpy(ci->line, value, len + 1);
  ci->len = len;

  if (ci->cursor > len) {
    /* clamp the cursor */
    ci->cursor = len;
  }
}

static void rna_ConsoleLine_cursor_index_range(
    PointerRNA *ptr, int *min, int *max, int *UNUSED(softmin), int *UNUSED(softmax))
{
  ConsoleLine *ci = (ConsoleLine *)ptr->data;

  *min = 0;
  *max = ci->len; /* intentionally _not_ -1 */
}

/* Space Dopesheet */

static void rna_SpaceDopeSheetEditor_action_set(PointerRNA *ptr,
                                                PointerRNA value,
                                                struct ReportList *UNUSED(reports))
{
  SpaceAction *saction = (SpaceAction *)(ptr->data);
  bAction *act = (bAction *)value.data;

  if ((act == NULL) || (act->idroot == 0)) {
    /* just set if we're clearing the action or if the action is "amorphous" still */
    saction->action = act;
  }
  else {
    /* action to set must strictly meet the mode criteria... */
    if (saction->mode == SACTCONT_ACTION) {
      /* currently, this is "object-level" only, until we have some way of specifying this */
      if (act->idroot == ID_OB) {
        saction->action = act;
      }
      else {
        printf(
            "ERROR: cannot assign Action '%s' to Action Editor, as action is not object-level "
            "animation\n",
            act->id.name + 2);
      }
    }
    else if (saction->mode == SACTCONT_SHAPEKEY) {
      /* as the name says, "shapekey-level" only... */
      if (act->idroot == ID_KE) {
        saction->action = act;
      }
      else {
        printf(
            "ERROR: cannot assign Action '%s' to Shape Key Editor, as action doesn't animate "
            "Shape Keys\n",
            act->id.name + 2);
      }
    }
    else {
      printf(
          "ACK: who's trying to set an action while not in a mode displaying a single Action "
          "only?\n");
    }
  }
}

static void rna_SpaceDopeSheetEditor_action_update(bContext *C, PointerRNA *ptr)
{
  SpaceAction *saction = (SpaceAction *)(ptr->data);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Main *bmain = CTX_data_main(C);

  Object *obact = OBACT(view_layer);
  if (obact == NULL) {
    return;
  }

  AnimData *adt = NULL;
  ID *id = NULL;
  switch (saction->mode) {
    case SACTCONT_ACTION:
      /* TODO: context selector could help decide this with more control? */
      adt = BKE_animdata_add_id(&obact->id);
      id = &obact->id;
      break;
    case SACTCONT_SHAPEKEY: {
      Key *key = BKE_key_from_object(obact);
      if (key == NULL) {
        return;
      }
      adt = BKE_animdata_add_id(&key->id);
      id = &key->id;
      break;
    }
    case SACTCONT_GPENCIL:
    case SACTCONT_DOPESHEET:
    case SACTCONT_MASK:
    case SACTCONT_CACHEFILE:
    case SACTCONT_TIMELINE:
      return;
  }

  if (adt == NULL) {
    /* No animdata was added, so the depsgraph also doesn't need tagging. */
    return;
  }

  /* Don't do anything if old and new actions are the same... */
  if (adt->action == saction->action) {
    return;
  }

  /* Exit editmode first - we cannot change actions while in tweakmode. */
  BKE_nla_tweakmode_exit(adt);

  /* To prevent data loss (i.e. if users flip between actions using the Browse menu),
   * stash this action if nothing else uses it.
   *
   * EXCEPTION:
   * This callback runs when unlinking actions. In that case, we don't want to
   * stash the action, as the user is signaling that they want to detach it.
   * This can be reviewed again later,
   * but it could get annoying if we keep these instead.
   */
  if (adt->action != NULL && adt->action->id.us <= 0 && saction->action != NULL) {
    /* XXX: Things here get dodgy if this action is only partially completed,
     *      and the user then uses the browse menu to get back to this action,
     *      assigning it as the active action (i.e. the stash strip gets out of sync)
     */
    BKE_nla_action_stash(adt, ID_IS_OVERRIDE_LIBRARY(id));
  }

  BKE_animdata_set_action(NULL, id, saction->action);

  DEG_id_tag_update(&obact->id, ID_RECALC_ANIMATION | ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);

  /* Update relations as well, so new time source dependency is added. */
  DEG_relations_tag_update(bmain);
}

static void rna_SpaceDopeSheetEditor_mode_update(bContext *C, PointerRNA *ptr)
{
  SpaceAction *saction = (SpaceAction *)(ptr->data);
  ScrArea *area = CTX_wm_area(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *obact = OBACT(view_layer);

  /* special exceptions for ShapeKey Editor mode */
  if (saction->mode == SACTCONT_SHAPEKEY) {
    Key *key = BKE_key_from_object(obact);

    /* 1) update the action stored for the editor */
    if (key) {
      saction->action = (key->adt) ? key->adt->action : NULL;
    }
    else {
      saction->action = NULL;
    }
  }
  /* make sure action stored is valid */
  else if (saction->mode == SACTCONT_ACTION) {
    /* 1) update the action stored for the editor */
    /* TODO: context selector could help decide this with more control? */
    if (obact) {
      saction->action = (obact->adt) ? obact->adt->action : NULL;
    }
    else {
      saction->action = NULL;
    }
  }

  /* Collapse (and show) summary channel and hide channel list for timeline */
  if (saction->mode == SACTCONT_TIMELINE) {
    saction->ads.flag |= ADS_FLAG_SUMMARY_COLLAPSED;
    saction->ads.filterflag |= ADS_FILTER_SUMMARY;
  }

  if (area && area->spacedata.first == saction) {
    ARegion *channels_region = BKE_area_find_region_type(area, RGN_TYPE_CHANNELS);
    if (channels_region) {
      if (saction->mode == SACTCONT_TIMELINE) {
        channels_region->flag |= RGN_FLAG_HIDDEN;
      }
      else {
        channels_region->flag &= ~RGN_FLAG_HIDDEN;
      }
      ED_region_visibility_change_update(C, area, channels_region);
    }
  }

  /* recalculate extents of channel list */
  saction->runtime.flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;

  /* store current mode as "old mode",
   * so that returning from other editors doesn't always reset to "Action Editor" */
  if (saction->mode != SACTCONT_TIMELINE) {
    saction->mode_prev = saction->mode;
  }
}

/* Space Graph Editor */

static void rna_SpaceGraphEditor_display_mode_update(bContext *C, PointerRNA *ptr)
{
  ScrArea *area = rna_area_from_space(ptr);
  SpaceGraph *sipo = (SpaceGraph *)ptr->data;

  /* for "Drivers" mode, enable all the necessary bits and pieces */
  if (sipo->mode == SIPO_MODE_DRIVERS) {
    ED_drivers_editor_init(C, area);
    ED_area_tag_redraw(area);
  }

  /* after changing view mode, must force recalculation of F-Curve colors
   * which can only be achieved using refresh as opposed to redraw
   */
  ED_area_tag_refresh(area);
}

static bool rna_SpaceGraphEditor_has_ghost_curves_get(PointerRNA *ptr)
{
  SpaceGraph *sipo = (SpaceGraph *)(ptr->data);
  return (BLI_listbase_is_empty(&sipo->runtime.ghost_curves) == false);
}

static void rna_SpaceConsole_rect_update(Main *UNUSED(bmain),
                                         Scene *UNUSED(scene),
                                         PointerRNA *ptr)
{
  SpaceConsole *sc = ptr->data;
  WM_main_add_notifier(NC_SPACE | ND_SPACE_CONSOLE | NA_EDITED, sc);
}

static void rna_SequenceEditor_update_cache(Main *UNUSED(bmain),
                                            Scene *scene,
                                            PointerRNA *UNUSED(ptr))
{
  SEQ_cache_cleanup(scene);
}

static void seq_build_proxy(bContext *C, PointerRNA *ptr)
{
  if (U.sequencer_proxy_setup != USER_SEQ_PROXY_SETUP_AUTOMATIC) {
    return;
  }

  SpaceSeq *sseq = ptr->data;
  Scene *scene = CTX_data_scene(C);
  ListBase *seqbase = SEQ_active_seqbase_get(SEQ_editing_get(scene, false));

  GSet *file_list = BLI_gset_new(BLI_ghashutil_strhash_p, BLI_ghashutil_strcmp, "file list");
  wmJob *wm_job = ED_seq_proxy_wm_job_get(C);
  ProxyJob *pj = ED_seq_proxy_job_get(C, wm_job);

  LISTBASE_FOREACH (Sequence *, seq, seqbase) {
    if (seq->type != SEQ_TYPE_MOVIE || seq->strip == NULL || seq->strip->proxy == NULL) {
      continue;
    }

    /* Add new proxy size. */
    seq->strip->proxy->build_size_flags |= SEQ_rendersize_to_proxysize(sseq->render_size);

    /* Build proxy. */
    SEQ_proxy_rebuild_context(pj->main, pj->depsgraph, pj->scene, seq, file_list, &pj->queue);
  }

  BLI_gset_free(file_list, MEM_freeN);

  if (!WM_jobs_is_running(wm_job)) {
    G.is_break = false;
    WM_jobs_start(CTX_wm_manager(C), wm_job);
  }

  ED_area_tag_redraw(CTX_wm_area(C));
}

static void rna_SequenceEditor_render_size_update(bContext *C, PointerRNA *ptr)
{
  seq_build_proxy(C, ptr);
  rna_SequenceEditor_update_cache(CTX_data_main(C), CTX_data_scene(C), ptr);
}

static void rna_Sequencer_view_type_update(Main *UNUSED(bmain),
                                           Scene *UNUSED(scene),
                                           PointerRNA *ptr)
{
  ScrArea *area = rna_area_from_space(ptr);
  ED_area_tag_refresh(area);
}

/* Space Node Editor */

static void rna_SpaceNodeEditor_node_tree_set(PointerRNA *ptr,
                                              const PointerRNA value,
                                              struct ReportList *UNUSED(reports))
{
  SpaceNode *snode = (SpaceNode *)ptr->data;
  ED_node_tree_start(snode, (bNodeTree *)value.data, NULL, NULL);
}

static bool rna_SpaceNodeEditor_node_tree_poll(PointerRNA *ptr, const PointerRNA value)
{
  SpaceNode *snode = (SpaceNode *)ptr->data;
  bNodeTree *ntree = (bNodeTree *)value.data;

  /* node tree type must match the selected type in node editor */
  return (STREQ(snode->tree_idname, ntree->idname));
}

static void rna_SpaceNodeEditor_node_tree_update(const bContext *C, PointerRNA *UNUSED(ptr))
{
  ED_node_tree_update(C);
}

static int rna_SpaceNodeEditor_tree_type_get(PointerRNA *ptr)
{
  SpaceNode *snode = (SpaceNode *)ptr->data;
  return rna_node_tree_idname_to_enum(snode->tree_idname);
}
static void rna_SpaceNodeEditor_tree_type_set(PointerRNA *ptr, int value)
{
  SpaceNode *snode = (SpaceNode *)ptr->data;
  ED_node_set_tree_type(snode, rna_node_tree_type_from_enum(value));
}
static bool rna_SpaceNodeEditor_tree_type_poll(void *Cv, bNodeTreeType *type)
{
  bContext *C = (bContext *)Cv;
  if (type->poll) {
    return type->poll(C, type);
  }
  else {
    return true;
  }
}

static void rna_SpaceNodeEditor_cursor_location_get(PointerRNA *ptr, float value[2])
{
  const SpaceNode *snode = (SpaceNode *)ptr->data;

  ED_node_cursor_location_get(snode, value);
}

static void rna_SpaceNodeEditor_cursor_location_set(PointerRNA *ptr, const float value[2])
{
  SpaceNode *snode = (SpaceNode *)ptr->data;

  ED_node_cursor_location_set(snode, value);
}

const EnumPropertyItem *RNA_enum_node_tree_types_itemf_impl(bContext *C, bool *r_free)
{
  return rna_node_tree_type_itemf(C, rna_SpaceNodeEditor_tree_type_poll, r_free);
}

static const EnumPropertyItem *rna_SpaceNodeEditor_tree_type_itemf(bContext *C,
                                                                   PointerRNA *UNUSED(ptr),
                                                                   PropertyRNA *UNUSED(prop),
                                                                   bool *r_free)
{
  return RNA_enum_node_tree_types_itemf_impl(C, r_free);
}

static void rna_SpaceNodeEditor_path_get(PointerRNA *ptr, char *value)
{
  SpaceNode *snode = ptr->data;
  ED_node_tree_path_get(snode, value);
}

static int rna_SpaceNodeEditor_path_length(PointerRNA *ptr)
{
  SpaceNode *snode = ptr->data;
  return ED_node_tree_path_length(snode);
}

static void rna_SpaceNodeEditor_path_clear(SpaceNode *snode, bContext *C)
{
  ED_node_tree_start(snode, NULL, NULL, NULL);
  ED_node_tree_update(C);
}

static void rna_SpaceNodeEditor_path_start(SpaceNode *snode, bContext *C, PointerRNA *node_tree)
{
  ED_node_tree_start(snode, (bNodeTree *)node_tree->data, NULL, NULL);
  ED_node_tree_update(C);
}

static void rna_SpaceNodeEditor_path_append(SpaceNode *snode,
                                            bContext *C,
                                            PointerRNA *node_tree,
                                            PointerRNA *node)
{
  ED_node_tree_push(snode, node_tree->data, node->data);
  ED_node_tree_update(C);
}

static void rna_SpaceNodeEditor_path_pop(SpaceNode *snode, bContext *C)
{
  ED_node_tree_pop(snode);
  ED_node_tree_update(C);
}

static void rna_SpaceNodeEditor_show_backdrop_update(Main *UNUSED(bmain),
                                                     Scene *UNUSED(scene),
                                                     PointerRNA *UNUSED(ptr))
{
  WM_main_add_notifier(NC_NODE | NA_EDITED, NULL);
  WM_main_add_notifier(NC_SCENE | ND_NODES, NULL);
}

static void rna_SpaceNodeEditor_cursor_location_from_region(SpaceNode *snode,
                                                            bContext *C,
                                                            int x,
                                                            int y)
{
  ARegion *region = CTX_wm_region(C);

  float cursor_location[2];

  UI_view2d_region_to_view(&region->v2d, x, y, &cursor_location[0], &cursor_location[1]);
  cursor_location[0] /= UI_DPI_FAC;
  cursor_location[1] /= UI_DPI_FAC;

  ED_node_cursor_location_set(snode, cursor_location);
}

static void rna_SpaceClipEditor_clip_set(PointerRNA *ptr,
                                         PointerRNA value,
                                         struct ReportList *UNUSED(reports))
{
  SpaceClip *sc = (SpaceClip *)(ptr->data);
  bScreen *screen = (bScreen *)ptr->owner_id;

  ED_space_clip_set_clip(NULL, screen, sc, (MovieClip *)value.data);
}

static void rna_SpaceClipEditor_mask_set(PointerRNA *ptr,
                                         PointerRNA value,
                                         struct ReportList *UNUSED(reports))
{
  SpaceClip *sc = (SpaceClip *)(ptr->data);

  ED_space_clip_set_mask(NULL, sc, (Mask *)value.data);
}

static void rna_SpaceClipEditor_clip_mode_update(Main *UNUSED(bmain),
                                                 Scene *UNUSED(scene),
                                                 PointerRNA *ptr)
{
  SpaceClip *sc = (SpaceClip *)(ptr->data);

  if (sc->mode == SC_MODE_MASKEDIT && sc->view != SC_VIEW_CLIP) {
    /* Make sure we are in the right view for mask editing */
    sc->view = SC_VIEW_CLIP;
    ScrArea *area = rna_area_from_space(ptr);
    ED_area_tag_refresh(area);
  }

  sc->scopes.ok = 0;
}

static void rna_SpaceClipEditor_lock_selection_update(Main *UNUSED(bmain),
                                                      Scene *UNUSED(scene),
                                                      PointerRNA *ptr)
{
  SpaceClip *sc = (SpaceClip *)(ptr->data);

  sc->xlockof = 0.0f;
  sc->ylockof = 0.0f;
}

static void rna_SpaceClipEditor_view_type_update(Main *UNUSED(bmain),
                                                 Scene *UNUSED(scene),
                                                 PointerRNA *ptr)
{
  ScrArea *area = rna_area_from_space(ptr);
  ED_area_tag_refresh(area);
}

/* File browser. */

static char *rna_FileSelectParams_path(PointerRNA *UNUSED(ptr))
{
  return BLI_strdup("params");
}

int rna_FileSelectParams_filename_editable(struct PointerRNA *ptr, const char **r_info)
{
  FileSelectParams *params = ptr->data;

  if (params && (params->flag & FILE_DIRSEL_ONLY)) {
    *r_info = "Only directories can be chosen for the current operation.";
    return 0;
  }

  return params ? PROP_EDITABLE : 0;
}

static bool rna_FileSelectParams_use_lib_get(PointerRNA *ptr)
{
  FileSelectParams *params = ptr->data;

  return params && (params->type == FILE_LOADLIB);
}

static const EnumPropertyItem *rna_FileSelectParams_recursion_level_itemf(
    bContext *UNUSED(C), PointerRNA *ptr, PropertyRNA *UNUSED(prop), bool *r_free)
{
  FileSelectParams *params = ptr->data;

  if (params && params->type != FILE_LOADLIB) {
    EnumPropertyItem *item = NULL;
    int totitem = 0;

    RNA_enum_items_add_value(&item, &totitem, fileselectparams_recursion_level_items, 0);
    RNA_enum_items_add_value(&item, &totitem, fileselectparams_recursion_level_items, 2);
    RNA_enum_items_add_value(&item, &totitem, fileselectparams_recursion_level_items, 3);
    RNA_enum_items_add_value(&item, &totitem, fileselectparams_recursion_level_items, 4);

    RNA_enum_item_end(&item, &totitem);
    *r_free = true;

    return item;
  }

  *r_free = false;
  return fileselectparams_recursion_level_items;
}

static void rna_FileSelectPrams_filter_glob_set(PointerRNA *ptr, const char *value)
{
  FileSelectParams *params = ptr->data;

  BLI_strncpy(params->filter_glob, value, sizeof(params->filter_glob));

  /* Remove stupid things like last group being a wildcard-only one. */
  BLI_path_extension_glob_validate(params->filter_glob);
}

static PointerRNA rna_FileSelectParams_filter_id_get(PointerRNA *ptr)
{
  return rna_pointer_inherit_refine(ptr, &RNA_FileSelectIDFilter, ptr->data);
}

static int rna_FileAssetSelectParams_asset_library_get(PointerRNA *ptr)
{
  FileAssetSelectParams *params = ptr->data;
  /* Just an extra sanity check to ensure this isn't somehow called for RNA_FileSelectParams. */
  BLI_assert(ptr->type == &RNA_FileAssetSelectParams);

  /* Simple case: Predefined repo, just set the value. */
  if (params->asset_library.type < FILE_ASSET_LIBRARY_CUSTOM) {
    return params->asset_library.type;
  }

  /* Note that the path isn't checked for validity here. If an invalid library path is used, the
   * Asset Browser can give a nice hint on what's wrong. */
  const bUserAssetLibrary *user_library = BKE_preferences_asset_library_find_from_index(
      &U, params->asset_library.custom_library_index);
  if (user_library) {
    return FILE_ASSET_LIBRARY_CUSTOM + params->asset_library.custom_library_index;
  }

  BLI_assert(0);
  return FILE_ASSET_LIBRARY_LOCAL;
}

static void rna_FileAssetSelectParams_asset_library_set(PointerRNA *ptr, int value)
{
  FileAssetSelectParams *params = ptr->data;

  /* Simple case: Predefined repo, just set the value. */
  if (value < FILE_ASSET_LIBRARY_CUSTOM) {
    params->asset_library.type = value;
    params->asset_library.custom_library_index = -1;
    BLI_assert(ELEM(value, FILE_ASSET_LIBRARY_LOCAL));
    return;
  }

  const bUserAssetLibrary *user_library = BKE_preferences_asset_library_find_from_index(
      &U, value - FILE_ASSET_LIBRARY_CUSTOM);

  /* Note that the path isn't checked for validity here. If an invalid library path is used, the
   * Asset Browser can give a nice hint on what's wrong. */
  const bool is_valid = (user_library->name[0] && user_library->path[0]);
  if (!user_library) {
    params->asset_library.type = FILE_ASSET_LIBRARY_LOCAL;
    params->asset_library.custom_library_index = -1;
  }
  else if (user_library && is_valid) {
    params->asset_library.custom_library_index = value - FILE_ASSET_LIBRARY_CUSTOM;
    params->asset_library.type = FILE_ASSET_LIBRARY_CUSTOM;
  }
}

static const EnumPropertyItem *rna_FileAssetSelectParams_asset_library_itemf(
    bContext *UNUSED(C), PointerRNA *UNUSED(ptr), PropertyRNA *UNUSED(prop), bool *r_free)
{
  const EnumPropertyItem predefined_items[] = {
      /* For the future. */
      // {FILE_ASSET_REPO_BUNDLED, "BUNDLED", 0, "Bundled", "Show the default user assets"},
      {FILE_ASSET_LIBRARY_LOCAL,
       "LOCAL",
       ICON_BLENDER,
       "Current File",
       "Show the assets currently available in this Blender session"},
      {0, NULL, 0, NULL, NULL},
  };

  EnumPropertyItem *item = NULL;
  int totitem = 0;

  /* Add separator if needed. */
  if (!BLI_listbase_is_empty(&U.asset_libraries)) {
    const EnumPropertyItem sepr = {0, "", 0, "Custom", NULL};
    RNA_enum_item_add(&item, &totitem, &sepr);
  }

  int i = 0;
  for (bUserAssetLibrary *user_library = U.asset_libraries.first; user_library;
       user_library = user_library->next, i++) {
    /* Note that the path itself isn't checked for validity here. If an invalid library path is
     * used, the Asset Browser can give a nice hint on what's wrong. */
    const bool is_valid = (user_library->name[0] && user_library->path[0]);
    if (!is_valid) {
      continue;
    }

    /* Use library path as description, it's a nice hint for users. */
    EnumPropertyItem tmp = {FILE_ASSET_LIBRARY_CUSTOM + i,
                            user_library->name,
                            ICON_NONE,
                            user_library->name,
                            user_library->path};
    RNA_enum_item_add(&item, &totitem, &tmp);
  }

  if (totitem) {
    const EnumPropertyItem sepr = {0, "", 0, "Built-in", NULL};
    RNA_enum_item_add(&item, &totitem, &sepr);
  }

  /* Add predefined items. */
  RNA_enum_items_add(&item, &totitem, predefined_items);

  RNA_enum_item_end(&item, &totitem);
  *r_free = true;
  return item;
}

static void rna_FileAssetSelectParams_asset_category_set(PointerRNA *ptr, uint64_t value)
{
  FileSelectParams *params = ptr->data;
  params->filter_id = value;
}

static uint64_t rna_FileAssetSelectParams_asset_category_get(PointerRNA *ptr)
{
  FileSelectParams *params = ptr->data;
  return params->filter_id;
}

static void rna_FileBrowser_FileSelectEntry_name_get(PointerRNA *ptr, char *value)
{
  const FileDirEntry *entry = ptr->data;
  strcpy(value, entry->name);
}

static int rna_FileBrowser_FileSelectEntry_name_length(PointerRNA *ptr)
{
  const FileDirEntry *entry = ptr->data;
  return (int)strlen(entry->name);
}

static int rna_FileBrowser_FileSelectEntry_preview_icon_id_get(PointerRNA *ptr)
{
  const FileDirEntry *entry = ptr->data;
  return ED_file_icon(entry);
}

static PointerRNA rna_FileBrowser_FileSelectEntry_asset_data_get(PointerRNA *ptr)
{
  const FileDirEntry *entry = ptr->data;
  return rna_pointer_inherit_refine(ptr, &RNA_AssetMetaData, entry->asset_data);
}

static StructRNA *rna_FileBrowser_params_typef(PointerRNA *ptr)
{
  SpaceFile *sfile = ptr->data;
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);

  if (params == ED_fileselect_get_file_params(sfile)) {
    return &RNA_FileSelectParams;
  }
  if (params == (void *)ED_fileselect_get_asset_params(sfile)) {
    return &RNA_FileAssetSelectParams;
  }

  BLI_assert(!"Could not identify file select parameters");
  return NULL;
}

static PointerRNA rna_FileBrowser_params_get(PointerRNA *ptr)
{
  SpaceFile *sfile = ptr->data;
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  StructRNA *params_struct = rna_FileBrowser_params_typef(ptr);

  if (params && params_struct) {
    return rna_pointer_inherit_refine(ptr, params_struct, params);
  }

  return rna_pointer_inherit_refine(ptr, NULL, NULL);
}

static void rna_FileBrowser_FSMenuEntry_path_get(PointerRNA *ptr, char *value)
{
  char *path = ED_fsmenu_entry_get_path(ptr->data);

  strcpy(value, path ? path : "");
}

static int rna_FileBrowser_FSMenuEntry_path_length(PointerRNA *ptr)
{
  char *path = ED_fsmenu_entry_get_path(ptr->data);

  return (int)(path ? strlen(path) : 0);
}

static void rna_FileBrowser_FSMenuEntry_path_set(PointerRNA *ptr, const char *value)
{
  FSMenuEntry *fsm = ptr->data;

  /* Note: this will write to file immediately.
   * Not nice (and to be fixed ultimately), but acceptable in this case for now. */
  ED_fsmenu_entry_set_path(fsm, value);
}

static void rna_FileBrowser_FSMenuEntry_name_get(PointerRNA *ptr, char *value)
{
  strcpy(value, ED_fsmenu_entry_get_name(ptr->data));
}

static int rna_FileBrowser_FSMenuEntry_name_length(PointerRNA *ptr)
{
  return (int)strlen(ED_fsmenu_entry_get_name(ptr->data));
}

static void rna_FileBrowser_FSMenuEntry_name_set(PointerRNA *ptr, const char *value)
{
  FSMenuEntry *fsm = ptr->data;

  /* Note: this will write to file immediately.
   * Not nice (and to be fixed ultimately), but acceptable in this case for now. */
  ED_fsmenu_entry_set_name(fsm, value);
}

static int rna_FileBrowser_FSMenuEntry_name_get_editable(PointerRNA *ptr,
                                                         const char **UNUSED(r_info))
{
  FSMenuEntry *fsm = ptr->data;

  return fsm->save ? PROP_EDITABLE : 0;
}

static int rna_FileBrowser_FSMenuEntry_icon_get(PointerRNA *ptr)
{
  FSMenuEntry *fsm = ptr->data;
  return ED_fsmenu_entry_get_icon(fsm);
}

static void rna_FileBrowser_FSMenuEntry_icon_set(PointerRNA *ptr, int value)
{
  FSMenuEntry *fsm = ptr->data;
  ED_fsmenu_entry_set_icon(fsm, value);
}

static bool rna_FileBrowser_FSMenuEntry_use_save_get(PointerRNA *ptr)
{
  FSMenuEntry *fsm = ptr->data;
  return fsm->save;
}

static bool rna_FileBrowser_FSMenuEntry_is_valid_get(PointerRNA *ptr)
{
  FSMenuEntry *fsm = ptr->data;
  return fsm->valid;
}

static void rna_FileBrowser_FSMenu_next(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;

  if (internal->skip) {
    do {
      internal->link = (Link *)(((FSMenuEntry *)(internal->link))->next);
      iter->valid = (internal->link != NULL);
    } while (iter->valid && internal->skip(iter, internal->link));
  }
  else {
    internal->link = (Link *)(((FSMenuEntry *)(internal->link))->next);
    iter->valid = (internal->link != NULL);
  }
}

static void rna_FileBrowser_FSMenu_begin(CollectionPropertyIterator *iter, FSMenuCategory category)
{
  ListBaseIterator *internal = &iter->internal.listbase;

  struct FSMenu *fsmenu = ED_fsmenu_get();
  struct FSMenuEntry *fsmentry = ED_fsmenu_get_category(fsmenu, category);

  internal->link = (fsmentry) ? (Link *)fsmentry : NULL;
  internal->skip = NULL;

  iter->valid = (internal->link != NULL);
}

static PointerRNA rna_FileBrowser_FSMenu_get(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;
  PointerRNA r_ptr;

  RNA_pointer_create(NULL, &RNA_FileBrowserFSMenuEntry, internal->link, &r_ptr);

  return r_ptr;
}

static void rna_FileBrowser_FSMenu_end(CollectionPropertyIterator *UNUSED(iter))
{
}

static void rna_FileBrowser_FSMenuSystem_data_begin(CollectionPropertyIterator *iter,
                                                    PointerRNA *UNUSED(ptr))
{
  rna_FileBrowser_FSMenu_begin(iter, FS_CATEGORY_SYSTEM);
}

static int rna_FileBrowser_FSMenuSystem_data_length(PointerRNA *UNUSED(ptr))
{
  struct FSMenu *fsmenu = ED_fsmenu_get();

  return ED_fsmenu_get_nentries(fsmenu, FS_CATEGORY_SYSTEM);
}

static void rna_FileBrowser_FSMenuSystemBookmark_data_begin(CollectionPropertyIterator *iter,
                                                            PointerRNA *UNUSED(ptr))
{
  rna_FileBrowser_FSMenu_begin(iter, FS_CATEGORY_SYSTEM_BOOKMARKS);
}

static int rna_FileBrowser_FSMenuSystemBookmark_data_length(PointerRNA *UNUSED(ptr))
{
  struct FSMenu *fsmenu = ED_fsmenu_get();

  return ED_fsmenu_get_nentries(fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS);
}

static void rna_FileBrowser_FSMenuBookmark_data_begin(CollectionPropertyIterator *iter,
                                                      PointerRNA *UNUSED(ptr))
{
  rna_FileBrowser_FSMenu_begin(iter, FS_CATEGORY_BOOKMARKS);
}

static int rna_FileBrowser_FSMenuBookmark_data_length(PointerRNA *UNUSED(ptr))
{
  struct FSMenu *fsmenu = ED_fsmenu_get();

  return ED_fsmenu_get_nentries(fsmenu, FS_CATEGORY_BOOKMARKS);
}

static void rna_FileBrowser_FSMenuRecent_data_begin(CollectionPropertyIterator *iter,
                                                    PointerRNA *UNUSED(ptr))
{
  rna_FileBrowser_FSMenu_begin(iter, FS_CATEGORY_RECENT);
}

static int rna_FileBrowser_FSMenuRecent_data_length(PointerRNA *UNUSED(ptr))
{
  struct FSMenu *fsmenu = ED_fsmenu_get();

  return ED_fsmenu_get_nentries(fsmenu, FS_CATEGORY_RECENT);
}

static int rna_FileBrowser_FSMenu_active_get(PointerRNA *ptr, const FSMenuCategory category)
{
  SpaceFile *sf = ptr->data;
  int actnr = -1;

  switch (category) {
    case FS_CATEGORY_SYSTEM:
      actnr = sf->systemnr;
      break;
    case FS_CATEGORY_SYSTEM_BOOKMARKS:
      actnr = sf->system_bookmarknr;
      break;
    case FS_CATEGORY_BOOKMARKS:
      actnr = sf->bookmarknr;
      break;
    case FS_CATEGORY_RECENT:
      actnr = sf->recentnr;
      break;
    case FS_CATEGORY_OTHER:
      /* pass. */
      break;
  }

  return actnr;
}

static void rna_FileBrowser_FSMenu_active_set(PointerRNA *ptr,
                                              int value,
                                              const FSMenuCategory category)
{
  SpaceFile *sf = ptr->data;
  struct FSMenu *fsmenu = ED_fsmenu_get();
  FSMenuEntry *fsm = ED_fsmenu_get_entry(fsmenu, category, value);

  if (fsm && sf->params) {
    switch (category) {
      case FS_CATEGORY_SYSTEM:
        sf->systemnr = value;
        break;
      case FS_CATEGORY_SYSTEM_BOOKMARKS:
        sf->system_bookmarknr = value;
        break;
      case FS_CATEGORY_BOOKMARKS:
        sf->bookmarknr = value;
        break;
      case FS_CATEGORY_RECENT:
        sf->recentnr = value;
        break;
      case FS_CATEGORY_OTHER:
        /* pass. */
        break;
    }

    BLI_strncpy(sf->params->dir, fsm->path, sizeof(sf->params->dir));
  }
}

static void rna_FileBrowser_FSMenu_active_range(PointerRNA *UNUSED(ptr),
                                                int *min,
                                                int *max,
                                                int *softmin,
                                                int *softmax,
                                                const FSMenuCategory category)
{
  struct FSMenu *fsmenu = ED_fsmenu_get();

  *min = *softmin = -1;
  *max = *softmax = ED_fsmenu_get_nentries(fsmenu, category) - 1;
}

static void rna_FileBrowser_FSMenu_active_update(struct bContext *C, PointerRNA *ptr)
{
  ScrArea *area = rna_area_from_space(ptr);
  ED_file_change_dir_ex(C, (bScreen *)ptr->owner_id, area);
}

static int rna_FileBrowser_FSMenuSystem_active_get(PointerRNA *ptr)
{
  return rna_FileBrowser_FSMenu_active_get(ptr, FS_CATEGORY_SYSTEM);
}

static void rna_FileBrowser_FSMenuSystem_active_set(PointerRNA *ptr, int value)
{
  rna_FileBrowser_FSMenu_active_set(ptr, value, FS_CATEGORY_SYSTEM);
}

static void rna_FileBrowser_FSMenuSystem_active_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
  rna_FileBrowser_FSMenu_active_range(ptr, min, max, softmin, softmax, FS_CATEGORY_SYSTEM);
}

static int rna_FileBrowser_FSMenuSystemBookmark_active_get(PointerRNA *ptr)
{
  return rna_FileBrowser_FSMenu_active_get(ptr, FS_CATEGORY_SYSTEM_BOOKMARKS);
}

static void rna_FileBrowser_FSMenuSystemBookmark_active_set(PointerRNA *ptr, int value)
{
  rna_FileBrowser_FSMenu_active_set(ptr, value, FS_CATEGORY_SYSTEM_BOOKMARKS);
}

static void rna_FileBrowser_FSMenuSystemBookmark_active_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
  rna_FileBrowser_FSMenu_active_range(
      ptr, min, max, softmin, softmax, FS_CATEGORY_SYSTEM_BOOKMARKS);
}

static int rna_FileBrowser_FSMenuBookmark_active_get(PointerRNA *ptr)
{
  return rna_FileBrowser_FSMenu_active_get(ptr, FS_CATEGORY_BOOKMARKS);
}

static void rna_FileBrowser_FSMenuBookmark_active_set(PointerRNA *ptr, int value)
{
  rna_FileBrowser_FSMenu_active_set(ptr, value, FS_CATEGORY_BOOKMARKS);
}

static void rna_FileBrowser_FSMenuBookmark_active_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
  rna_FileBrowser_FSMenu_active_range(ptr, min, max, softmin, softmax, FS_CATEGORY_BOOKMARKS);
}

static int rna_FileBrowser_FSMenuRecent_active_get(PointerRNA *ptr)
{
  return rna_FileBrowser_FSMenu_active_get(ptr, FS_CATEGORY_RECENT);
}

static void rna_FileBrowser_FSMenuRecent_active_set(PointerRNA *ptr, int value)
{
  rna_FileBrowser_FSMenu_active_set(ptr, value, FS_CATEGORY_RECENT);
}

static void rna_FileBrowser_FSMenuRecent_active_range(
    PointerRNA *ptr, int *min, int *max, int *softmin, int *softmax)
{
  rna_FileBrowser_FSMenu_active_range(ptr, min, max, softmin, softmax, FS_CATEGORY_RECENT);
}

static void rna_SpaceFileBrowser_browse_mode_update(Main *UNUSED(bmain),
                                                    Scene *UNUSED(scene),
                                                    PointerRNA *ptr)
{
  ScrArea *area = rna_area_from_space(ptr);
  ED_area_tag_refresh(area);
}

static void rna_SpaceSpreadsheet_pinned_id_set(PointerRNA *ptr,
                                               PointerRNA value,
                                               struct ReportList *UNUSED(reports))
{
  SpaceSpreadsheet *sspreadsheet = (SpaceSpreadsheet *)ptr->data;
  sspreadsheet->pinned_id = value.data;
}

static void rna_SpaceSpreadsheet_geometry_component_type_update(Main *UNUSED(bmain),
                                                                Scene *UNUSED(scene),
                                                                PointerRNA *ptr)
{
  SpaceSpreadsheet *sspreadsheet = (SpaceSpreadsheet *)ptr->data;
  if (sspreadsheet->geometry_component_type == GEO_COMPONENT_TYPE_POINT_CLOUD) {
    sspreadsheet->attribute_domain = ATTR_DOMAIN_POINT;
  }
}

const EnumPropertyItem *rna_SpaceSpreadsheet_attribute_domain_itemf(bContext *C,
                                                                    PointerRNA *ptr,
                                                                    PropertyRNA *UNUSED(prop),
                                                                    bool *r_free)
{
  SpaceSpreadsheet *sspreadsheet = (SpaceSpreadsheet *)ptr->data;
  GeometryComponentType component_type = sspreadsheet->geometry_component_type;
  if (sspreadsheet->object_eval_state == SPREADSHEET_OBJECT_EVAL_STATE_ORIGINAL) {
    Object *active_object = CTX_data_active_object(C);
    Object *used_object = (sspreadsheet->pinned_id && GS(sspreadsheet->pinned_id->name) == ID_OB) ?
                              (Object *)sspreadsheet->pinned_id :
                              active_object;
    if (used_object != NULL) {
      if (used_object->type == OB_POINTCLOUD) {
        component_type = GEO_COMPONENT_TYPE_POINT_CLOUD;
      }
      else {
        component_type = GEO_COMPONENT_TYPE_MESH;
      }
    }
  }

  EnumPropertyItem *item_array = NULL;
  int items_len = 0;
  for (const EnumPropertyItem *item = rna_enum_attribute_domain_items; item->identifier != NULL;
       item++) {
    if (component_type == GEO_COMPONENT_TYPE_MESH) {
      if (!ELEM(item->value,
                ATTR_DOMAIN_CORNER,
                ATTR_DOMAIN_EDGE,
                ATTR_DOMAIN_POINT,
                ATTR_DOMAIN_POLYGON)) {
        continue;
      }
    }
    if (component_type == GEO_COMPONENT_TYPE_POINT_CLOUD) {
      if (item->value != ATTR_DOMAIN_POINT) {
        continue;
      }
    }
    RNA_enum_item_add(&item_array, &items_len, item);
  }
  RNA_enum_item_end(&item_array, &items_len);

  *r_free = true;
  return item_array;
}

#else

static const EnumPropertyItem dt_uv_items[] = {
    {SI_UVDT_OUTLINE, "OUTLINE", 0, "Outline", "Display white edges with black outline"},
    {SI_UVDT_DASH, "DASH", 0, "Dash", "Display dashed black-white edges"},
    {SI_UVDT_BLACK, "BLACK", 0, "Black", "Display black edges"},
    {SI_UVDT_WHITE, "WHITE", 0, "White", "Display white edges"},
    {0, NULL, 0, NULL, NULL},
};

static void rna_def_space_generic_show_region_toggles(StructRNA *srna, int region_type_mask)
{
  PropertyRNA *prop;

#  define DEF_SHOW_REGION_PROPERTY(identifier, label, description) \
    { \
      prop = RNA_def_property(srna, STRINGIFY(identifier), PROP_BOOLEAN, PROP_NONE); \
      RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE); \
      RNA_def_property_boolean_funcs(prop, \
                                     STRINGIFY(rna_Space_##identifier##_get), \
                                     STRINGIFY(rna_Space_##identifier##_set)); \
      RNA_def_property_ui_text(prop, label, description); \
      RNA_def_property_update(prop, 0, STRINGIFY(rna_Space_##identifier##_update)); \
    } \
    ((void)0)

  if (region_type_mask & (1 << RGN_TYPE_TOOL_HEADER)) {
    region_type_mask &= ~(1 << RGN_TYPE_TOOL_HEADER);
    DEF_SHOW_REGION_PROPERTY(show_region_tool_header, "Tool Settings", "");
  }
  if (region_type_mask & (1 << RGN_TYPE_HEADER)) {
    region_type_mask &= ~(1 << RGN_TYPE_HEADER);
    DEF_SHOW_REGION_PROPERTY(show_region_header, "Header", "");
  }
  if (region_type_mask & (1 << RGN_TYPE_FOOTER)) {
    region_type_mask &= ~(1 << RGN_TYPE_FOOTER);
    DEF_SHOW_REGION_PROPERTY(show_region_footer, "Footer", "");
  }
  if (region_type_mask & (1 << RGN_TYPE_TOOLS)) {
    region_type_mask &= ~(1 << RGN_TYPE_TOOLS);
    DEF_SHOW_REGION_PROPERTY(show_region_toolbar, "Toolbar", "");
  }
  if (region_type_mask & (1 << RGN_TYPE_UI)) {
    region_type_mask &= ~(1 << RGN_TYPE_UI);
    DEF_SHOW_REGION_PROPERTY(show_region_ui, "Sidebar", "");
  }
  if (region_type_mask & (1 << RGN_TYPE_HUD)) {
    region_type_mask &= ~(1 << RGN_TYPE_HUD);
    DEF_SHOW_REGION_PROPERTY(show_region_hud, "Adjust Last Operation", "");
  }
  BLI_assert(region_type_mask == 0);
}

static void rna_def_space(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Space", NULL);
  RNA_def_struct_sdna(srna, "SpaceLink");
  RNA_def_struct_ui_text(srna, "Space", "Space data for a screen area");
  RNA_def_struct_refine_func(srna, "rna_Space_refine");

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "spacetype");
  RNA_def_property_enum_items(prop, rna_enum_space_type_items);
  /* When making this editable, take care for the special case of global areas
   * (see rna_Area_type_set). */
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Type", "Space data type");

  /* access to V2D_VIEWSYNC_SCREEN_TIME */
  prop = RNA_def_property(srna, "show_locked_time", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_Space_view2d_sync_get", "rna_Space_view2d_sync_set");
  RNA_def_property_ui_text(prop,
                           "Sync Visible Range",
                           "Synchronize the visible timeline range with other time-based editors");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, "rna_Space_view2d_sync_update");

  rna_def_space_generic_show_region_toggles(srna, (1 << RGN_TYPE_HEADER));
}

/* for all spaces that use a mask */
static void rna_def_space_mask_info(StructRNA *srna, int noteflag, const char *mask_set_func)
{
  PropertyRNA *prop;

  static const EnumPropertyItem overlay_mode_items[] = {
      {MASK_OVERLAY_ALPHACHANNEL,
       "ALPHACHANNEL",
       ICON_NONE,
       "Alpha Channel",
       "Show alpha channel of the mask"},
      {MASK_OVERLAY_COMBINED,
       "COMBINED",
       ICON_NONE,
       "Combined",
       "Combine space background image with the mask"},
      {0, NULL, 0, NULL, NULL},
  };

  prop = RNA_def_property(srna, "mask", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "mask_info.mask");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Mask", "Mask displayed and edited in this space");
  RNA_def_property_pointer_funcs(prop, NULL, mask_set_func, NULL, NULL);
  RNA_def_property_update(prop, noteflag, NULL);

  /* mask drawing */
  prop = RNA_def_property(srna, "mask_display_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "mask_info.draw_type");
  RNA_def_property_enum_items(prop, dt_uv_items);
  RNA_def_property_ui_text(prop, "Edge Display Type", "Display type for mask splines");
  RNA_def_property_update(prop, noteflag, NULL);

  prop = RNA_def_property(srna, "show_mask_smooth", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mask_info.draw_flag", MASK_DRAWFLAG_SMOOTH);
  RNA_def_property_ui_text(prop, "Display Smooth Splines", "");
  RNA_def_property_update(prop, noteflag, NULL);

  prop = RNA_def_property(srna, "show_mask_overlay", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "mask_info.draw_flag", MASK_DRAWFLAG_OVERLAY);
  RNA_def_property_ui_text(prop, "Show Mask Overlay", "");
  RNA_def_property_update(prop, noteflag, NULL);

  prop = RNA_def_property(srna, "mask_overlay_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "mask_info.overlay_mode");
  RNA_def_property_enum_items(prop, overlay_mode_items);
  RNA_def_property_ui_text(prop, "Overlay Mode", "Overlay mode of rasterized mask");
  RNA_def_property_update(prop, noteflag, NULL);
}

static void rna_def_space_image_uv(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem sticky_mode_items[] = {
      {SI_STICKY_DISABLE,
       "DISABLED",
       ICON_STICKY_UVS_DISABLE,
       "Disabled",
       "Sticky vertex selection disabled"},
      {SI_STICKY_LOC,
       "SHARED_LOCATION",
       ICON_STICKY_UVS_LOC,
       "Shared Location",
       "Select UVs that are at the same location and share a mesh vertex"},
      {SI_STICKY_VERTEX,
       "SHARED_VERTEX",
       ICON_STICKY_UVS_VERT,
       "Shared Vertex",
       "Select UVs that share a mesh vertex, whether or not they are at the same location"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem dt_uvstretch_items[] = {
      {SI_UVDT_STRETCH_ANGLE, "ANGLE", 0, "Angle", "Angular distortion between UV and 3D angles"},
      {SI_UVDT_STRETCH_AREA, "AREA", 0, "Area", "Area distortion between UV and 3D faces"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem pixel_snap_mode_items[] = {
      {SI_PIXEL_SNAP_DISABLED, "DISABLED", 0, "Disabled", "Don't snap to pixels"},
      {SI_PIXEL_SNAP_CORNER, "CORNER", 0, "Corner", "Snap to pixel corners"},
      {SI_PIXEL_SNAP_CENTER, "CENTER", 0, "Center", "Snap to pixel centers"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "SpaceUVEditor", NULL);
  RNA_def_struct_sdna(srna, "SpaceImage");
  RNA_def_struct_nested(brna, srna, "SpaceImageEditor");
  RNA_def_struct_path_func(srna, "rna_SpaceUVEditor_path");
  RNA_def_struct_ui_text(srna, "Space UV Editor", "UV editor data for the image editor space");

  /* selection */
  prop = RNA_def_property(srna, "sticky_select_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "sticky");
  RNA_def_property_enum_items(prop, sticky_mode_items);
  RNA_def_property_ui_text(
      prop, "Sticky Selection Mode", "Method for extending UV vertex selection");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

  /* drawing */
  prop = RNA_def_property(srna, "edge_display_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "dt_uv");
  RNA_def_property_enum_items(prop, dt_uv_items);
  RNA_def_property_ui_text(prop, "Display As", "Display style for UV edges");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

  prop = RNA_def_property(srna, "show_stretch", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_DRAW_STRETCH);
  RNA_def_property_ui_text(
      prop,
      "Display Stretch",
      "Display faces colored according to the difference in shape between UVs and "
      "their 3D coordinates (blue for low distortion, red for high distortion)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

  prop = RNA_def_property(srna, "display_stretch_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "dt_uvstretch");
  RNA_def_property_enum_items(prop, dt_uvstretch_items);
  RNA_def_property_ui_text(prop, "Display Stretch Type", "Type of stretch to display");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

  prop = RNA_def_property(srna, "show_modified_edges", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_DRAWSHADOW);
  RNA_def_property_ui_text(
      prop, "Display Modified Edges", "Display edges after modifiers are applied");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

  prop = RNA_def_property(srna, "show_metadata", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_DRAW_METADATA);
  RNA_def_property_ui_text(prop, "Show Metadata", "Display metadata properties of the image");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

  prop = RNA_def_property(srna, "show_texpaint", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SI_NO_DRAW_TEXPAINT);
  RNA_def_property_ui_text(
      prop, "Display Texture Paint UVs", "Display overlay of texture paint uv layer");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

  prop = RNA_def_property(srna, "show_pixel_coords", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SI_COORDFLOATS);
  RNA_def_property_ui_text(
      prop, "Pixel Coordinates", "Display UV coordinates in pixels rather than from 0.0 to 1.0");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

  prop = RNA_def_property(srna, "show_faces", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SI_NO_DRAWFACES);
  RNA_def_property_ui_text(prop, "Display Faces", "Display faces over the image");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

  prop = RNA_def_property(srna, "tile_grid_shape", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "tile_grid_shape");
  RNA_def_property_array(prop, 2);
  RNA_def_property_int_default(prop, 1);
  RNA_def_property_range(prop, 1, 10);
  RNA_def_property_ui_text(
      prop, "Tile Grid Shape", "How many tiles will be shown in the background");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

  prop = RNA_def_property(srna, "uv_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "uv_opacity");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "UV Opacity", "Opacity of UV overlays");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

  /* todo: move edge and face drawing options here from G.f */

  prop = RNA_def_property(srna, "pixel_snap_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, pixel_snap_mode_items);
  RNA_def_property_ui_text(prop, "Snap to Pixels", "Snap UVs to pixels while editing");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

  prop = RNA_def_property(srna, "lock_bounds", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_CLIP_UV);
  RNA_def_property_ui_text(prop,
                           "Constrain to Image Bounds",
                           "Constraint to stay within the image bounds while editing");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

  prop = RNA_def_property(srna, "use_live_unwrap", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_LIVE_UNWRAP);
  RNA_def_property_ui_text(
      prop,
      "Live Unwrap",
      "Continuously unwrap the selected UV island while transforming pinned vertices");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);
}

static void rna_def_space_outliner(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem display_mode_items[] = {
      {SO_SCENES,
       "SCENES",
       ICON_SCENE_DATA,
       "Scenes",
       "Display scenes and their view layers, collections and objects"},
      {SO_VIEW_LAYER,
       "VIEW_LAYER",
       ICON_RENDER_RESULT,
       "View Layer",
       "Display collections and objects in the view layer"},
      {SO_SEQUENCE,
       "SEQUENCE",
       ICON_SEQUENCE,
       "Video Sequencer",
       "Display data belonging to the Video Sequencer"},
      {SO_LIBRARIES,
       "LIBRARIES",
       ICON_FILE_BLEND,
       "Blender File",
       "Display data of current file and linked libraries"},
      {SO_DATA_API,
       "DATA_API",
       ICON_RNA,
       "Data API",
       "Display low level Blender data and its properties"},
      {SO_ID_ORPHANS,
       "ORPHAN_DATA",
       ICON_ORPHAN_DATA,
       "Orphan Data",
       "Display data-blocks which are unused and/or will be lost when the file is reloaded"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem filter_state_items[] = {
      {SO_FILTER_OB_ALL, "ALL", 0, "All", "Show all objects in the view layer"},
      {SO_FILTER_OB_VISIBLE, "VISIBLE", 0, "Visible", "Show visible objects"},
      {SO_FILTER_OB_SELECTED, "SELECTED", 0, "Selected", "Show selected objects"},
      {SO_FILTER_OB_ACTIVE, "ACTIVE", 0, "Active", "Show only the active object"},
      {SO_FILTER_OB_SELECTABLE, "SELECTABLE", 0, "Selectable", "Show only selectable objects"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "SpaceOutliner", "Space");
  RNA_def_struct_sdna(srna, "SpaceOutliner");
  RNA_def_struct_ui_text(srna, "Space Outliner", "Outliner space data");

  prop = RNA_def_property(srna, "display_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "outlinevis");
  RNA_def_property_enum_items(prop, display_mode_items);
  RNA_def_property_ui_text(prop, "Display Mode", "Type of information to display");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  prop = RNA_def_property(srna, "filter_text", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "search_string");
  RNA_def_property_ui_text(prop, "Display Filter", "Live search filtering string");
  RNA_def_property_flag(prop, PROP_TEXTEDIT_UPDATE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  prop = RNA_def_property(srna, "use_filter_case_sensitive", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "search_flags", SO_FIND_CASE_SENSITIVE);
  RNA_def_property_ui_text(
      prop, "Case Sensitive Matches Only", "Only use case sensitive matches of search string");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  prop = RNA_def_property(srna, "use_filter_complete", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "search_flags", SO_FIND_COMPLETE);
  RNA_def_property_ui_text(
      prop, "Complete Matches Only", "Only use complete matches of search string");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  prop = RNA_def_property(srna, "use_sort_alpha", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SO_SKIP_SORT_ALPHA);
  RNA_def_property_ui_text(prop, "Sort Alphabetically", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  prop = RNA_def_property(srna, "use_sync_select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SO_SYNC_SELECT);
  RNA_def_property_ui_text(
      prop, "Sync Outliner Selection", "Sync outliner selection with other editors");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  prop = RNA_def_property(srna, "show_mode_column", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SO_MODE_COLUMN);
  RNA_def_property_ui_text(
      prop, "Show Mode Column", "Show the mode column for mode toggle and activation");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  /* Granular restriction column option. */
  prop = RNA_def_property(srna, "show_restrict_column_enable", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "show_restrict_flags", SO_RESTRICT_ENABLE);
  RNA_def_property_ui_text(prop, "Exclude from View Layer", "Exclude from view layer");
  RNA_def_property_ui_icon(prop, ICON_CHECKBOX_HLT, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  prop = RNA_def_property(srna, "show_restrict_column_select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "show_restrict_flags", SO_RESTRICT_SELECT);
  RNA_def_property_ui_text(prop, "Selectable", "Selectable");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_SELECT_OFF, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  prop = RNA_def_property(srna, "show_restrict_column_hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "show_restrict_flags", SO_RESTRICT_HIDE);
  RNA_def_property_ui_text(prop, "Hide in Viewport", "Temporarily hide in viewport");
  RNA_def_property_ui_icon(prop, ICON_HIDE_OFF, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  prop = RNA_def_property(srna, "show_restrict_column_viewport", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "show_restrict_flags", SO_RESTRICT_VIEWPORT);
  RNA_def_property_ui_text(prop, "Disable in Viewports", "Globally disable in viewports");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_VIEW_OFF, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  prop = RNA_def_property(srna, "show_restrict_column_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "show_restrict_flags", SO_RESTRICT_RENDER);
  RNA_def_property_ui_text(prop, "Disable in Renders", "Globally disable in renders");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_RENDER_OFF, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  prop = RNA_def_property(srna, "show_restrict_column_holdout", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "show_restrict_flags", SO_RESTRICT_HOLDOUT);
  RNA_def_property_ui_text(prop, "Holdout", "Holdout");
  RNA_def_property_ui_icon(prop, ICON_HOLDOUT_ON, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  prop = RNA_def_property(srna, "show_restrict_column_indirect_only", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "show_restrict_flags", SO_RESTRICT_INDIRECT_ONLY);
  RNA_def_property_ui_text(prop, "Indirect Only", "Indirect only");
  RNA_def_property_ui_icon(prop, ICON_INDIRECT_ONLY_ON, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  /* Filters. */
  prop = RNA_def_property(srna, "use_filter_object", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "filter", SO_FILTER_NO_OBJECT);
  RNA_def_property_ui_text(prop, "Filter Objects", "Show objects");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  prop = RNA_def_property(srna, "use_filter_object_content", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "filter", SO_FILTER_NO_OB_CONTENT);
  RNA_def_property_ui_text(
      prop, "Show Object Contents", "Show what is inside the objects elements");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  prop = RNA_def_property(srna, "use_filter_children", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "filter", SO_FILTER_NO_CHILDREN);
  RNA_def_property_ui_text(prop, "Show Object Children", "Show children");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  prop = RNA_def_property(srna, "use_filter_collection", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "filter", SO_FILTER_NO_COLLECTION);
  RNA_def_property_ui_text(prop, "Show Collections", "Show collections");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  /* Filters object state. */
  prop = RNA_def_property(srna, "filter_state", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "filter_state");
  RNA_def_property_enum_items(prop, filter_state_items);
  RNA_def_property_ui_text(prop, "Object State Filter", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  prop = RNA_def_property(srna, "filter_invert", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "filter", SO_FILTER_OB_STATE_INVERSE);
  RNA_def_property_ui_text(prop, "Invert", "Invert the object state filter");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  /* Filters object type. */
  prop = RNA_def_property(srna, "use_filter_object_mesh", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "filter", SO_FILTER_NO_OB_MESH);
  RNA_def_property_ui_text(prop, "Show Meshes", "Show mesh objects");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  prop = RNA_def_property(srna, "use_filter_object_armature", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "filter", SO_FILTER_NO_OB_ARMATURE);
  RNA_def_property_ui_text(prop, "Show Armatures", "Show armature objects");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  prop = RNA_def_property(srna, "use_filter_object_empty", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "filter", SO_FILTER_NO_OB_EMPTY);
  RNA_def_property_ui_text(prop, "Show Empties", "Show empty objects");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  prop = RNA_def_property(srna, "use_filter_object_light", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "filter", SO_FILTER_NO_OB_LAMP);
  RNA_def_property_ui_text(prop, "Show Lights", "Show light objects");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  prop = RNA_def_property(srna, "use_filter_object_camera", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "filter", SO_FILTER_NO_OB_CAMERA);
  RNA_def_property_ui_text(prop, "Show Cameras", "Show camera objects");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  prop = RNA_def_property(srna, "use_filter_object_others", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "filter", SO_FILTER_NO_OB_OTHERS);
  RNA_def_property_ui_text(
      prop, "Show Other Objects", "Show curves, lattices, light probes, fonts, ...");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  /* Libraries filter. */
  prop = RNA_def_property(srna, "use_filter_id_type", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "filter", SO_FILTER_ID_TYPE);
  RNA_def_property_ui_text(prop, "Filter by Type", "Show only data-blocks of one type");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);

  prop = RNA_def_property(srna, "filter_id_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "filter_id_type");
  RNA_def_property_enum_items(prop, rna_enum_id_type_items);
  RNA_def_property_ui_text(prop, "Filter by Type", "Data-block type to show");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ID);

  prop = RNA_def_property(srna, "use_filter_lib_override", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "filter", SO_FILTER_NO_LIB_OVERRIDE);
  RNA_def_property_ui_text(prop,
                           "Show Library Overrides",
                           "For libraries with overrides created, show the overridden values");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, NULL);
}

static void rna_def_space_view3d_shading(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem background_type_items[] = {
      {V3D_SHADING_BACKGROUND_THEME, "THEME", 0, "Theme", "Use the theme for background color"},
      {V3D_SHADING_BACKGROUND_WORLD, "WORLD", 0, "World", "Use the world for background color"},
      {V3D_SHADING_BACKGROUND_VIEWPORT,
       "VIEWPORT",
       0,
       "Viewport",
       "Use a custom color limited to this viewport only"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem cavity_type_items[] = {
      {V3D_SHADING_CAVITY_SSAO,
       "WORLD",
       0,
       "World",
       "Cavity shading computed in world space, useful for larger-scale occlusion"},
      {V3D_SHADING_CAVITY_CURVATURE,
       "SCREEN",
       0,
       "Screen",
       "Curvature-based shading, useful for making fine details more visible"},
      {V3D_SHADING_CAVITY_BOTH, "BOTH", 0, "Both", "Use both effects simultaneously"},
      {0, NULL, 0, NULL, NULL},
  };

  /* Note these settings are used for both 3D viewport and the OpenGL render
   * engine in the scene, so can't assume to always be part of a screen. */
  srna = RNA_def_struct(brna, "View3DShading", NULL);
  RNA_def_struct_path_func(srna, "rna_View3DShading_path");
  RNA_def_struct_ui_text(
      srna, "3D View Shading Settings", "Settings for shading in the 3D viewport");
  RNA_def_struct_idprops_func(srna, "rna_View3DShading_idprops");

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_shading_type_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_3DViewShading_type_get",
                              "rna_3DViewShading_type_set",
                              "rna_3DViewShading_type_itemf");
  RNA_def_property_ui_text(
      prop, "Viewport Shading", "Method to display/shade objects in the 3D View");
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, "rna_3DViewShading_type_update");

  prop = RNA_def_property(srna, "light", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "light");
  RNA_def_property_enum_items(prop, rna_enum_viewport_lighting_items);
  RNA_def_property_ui_text(prop, "Lighting", "Lighting Method for Solid/Texture Viewport Shading");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "show_object_outline", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", V3D_SHADING_OBJECT_OUTLINE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Outline", "Show Object Outline");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "studio_light", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_studio_light_items);
  RNA_def_property_enum_default(prop, 0);
  RNA_def_property_enum_funcs(prop,
                              "rna_View3DShading_studio_light_get",
                              "rna_View3DShading_studio_light_set",
                              "rna_View3DShading_studio_light_itemf");
  RNA_def_property_ui_text(prop, "Studiolight", "Studio lighting setup");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "use_world_space_lighting", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", V3D_SHADING_WORLD_ORIENTATION);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "World Space Lighting", "Make the lighting fixed and not follow the camera");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "show_backface_culling", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", V3D_SHADING_BACKFACE_CULLING);
  RNA_def_property_ui_text(
      prop, "Backface Culling", "Use back face culling to hide the back side of faces");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "show_cavity", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", V3D_SHADING_CAVITY);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Cavity", "Show Cavity");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "cavity_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, cavity_type_items);
  RNA_def_property_ui_text(prop, "Cavity Type", "Way to display the cavity shading");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "curvature_ridge_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "curvature_ridge_factor");
  RNA_def_property_ui_text(prop, "Curvature Ridge", "Factor for the curvature ridges");
  RNA_def_property_range(prop, 0.0f, 2.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "curvature_valley_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "curvature_valley_factor");
  RNA_def_property_ui_text(prop, "Curvature Valley", "Factor for the curvature valleys");
  RNA_def_property_range(prop, 0.0f, 2.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "cavity_ridge_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "cavity_ridge_factor");
  RNA_def_property_ui_text(prop, "Cavity Ridge", "Factor for the cavity ridges");
  RNA_def_property_range(prop, 0.0f, 250.0f);
  RNA_def_property_ui_range(prop, 0.00f, 2.5f, 1, 3);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "cavity_valley_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "cavity_valley_factor");
  RNA_def_property_ui_text(prop, "Cavity Valley", "Factor for the cavity valleys");
  RNA_def_property_range(prop, 0.0f, 250.0f);
  RNA_def_property_ui_range(prop, 0.00f, 2.5f, 1, 3);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "selected_studio_light", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "StudioLight");
  RNA_define_verify_sdna(0);
  RNA_def_property_ui_text(prop, "Studio Light", "Selected StudioLight");
  RNA_def_property_pointer_funcs(
      prop, "rna_View3DShading_selected_studio_light_get", NULL, NULL, NULL);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_define_verify_sdna(1);

  prop = RNA_def_property(srna, "studiolight_rotate_z", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, NULL, "studiolight_rot_z");
  RNA_def_property_ui_text(
      prop, "Studiolight Rotation", "Rotation of the studiolight around the Z-Axis");
  RNA_def_property_range(prop, -M_PI, M_PI);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "studiolight_intensity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "studiolight_intensity");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Strength", "Strength of the studiolight");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 2.0f, 1, 3);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "studiolight_background_alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "studiolight_background");
  RNA_def_property_ui_text(prop, "World Opacity", "Show the studiolight in the background");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 1, 3);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "studiolight_background_blur", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "studiolight_blur");
  RNA_def_property_ui_text(prop, "Blur", "Blur the studiolight in the background");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 1, 2);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "use_studiolight_view_rotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(
      prop, NULL, "flag", V3D_SHADING_STUDIOLIGHT_VIEW_ROTATION);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_ui_text(
      prop, "World Space Lighting", "Make the HDR rotation fixed and not follow the camera");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "color_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "color_type");
  RNA_def_property_enum_items(prop, rna_enum_shading_color_type_items);
  RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_View3DShading_color_type_itemf");
  RNA_def_property_ui_text(prop, "Color", "Color Type");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, "rna_GPencil_update");

  prop = RNA_def_property(srna, "wireframe_color_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "wire_color_type");
  RNA_def_property_enum_items(prop, rna_enum_shading_color_type_items);
  RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_View3DShading_color_type_itemf");
  RNA_def_property_ui_text(prop, "Color", "Color Type");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "single_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, NULL, "single_color");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Color", "Color for single color mode");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "background_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, background_type_items);
  RNA_def_property_ui_text(prop, "Background", "Way to display the background");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "background_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Background Color", "Color for custom background color");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "show_shadows", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", V3D_SHADING_SHADOW);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Shadow", "Show Shadow");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "show_xray", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", V3D_SHADING_XRAY);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Show X-Ray", "Show whole scene transparent");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "show_xray_wireframe", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", V3D_SHADING_XRAY_WIREFRAME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Show X-Ray", "Show whole scene transparent");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "xray_alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "xray_alpha");
  RNA_def_property_ui_text(prop, "X-Ray Alpha", "Amount of alpha to use");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "xray_alpha_wireframe", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "xray_alpha_wire");
  RNA_def_property_ui_text(prop, "X-Ray Alpha", "Amount of alpha to use");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "use_dof", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", V3D_SHADING_DEPTH_OF_FIELD);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop,
      "Depth Of Field",
      "Use depth of field on viewport using the values from the active camera");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "use_scene_lights", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", V3D_SHADING_SCENE_LIGHTS);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Scene Lights", "Render lights and light probes of the scene");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "use_scene_world", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", V3D_SHADING_SCENE_WORLD);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Scene World", "Use scene world for lighting");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "use_scene_lights_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", V3D_SHADING_SCENE_LIGHTS_RENDER);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Scene Lights", "Render lights and light probes of the scene");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "use_scene_world_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", V3D_SHADING_SCENE_WORLD_RENDER);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Scene World", "Use scene world for lighting");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "show_specular_highlight", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", V3D_SHADING_SPECULAR_HIGHLIGHT);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Specular Highlights", "Render specular highlights");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "object_outline_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, NULL, "object_outline_color");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Outline Color", "Color for object outline");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "shadow_intensity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "shadow_intensity");
  RNA_def_property_ui_text(prop, "Shadow Intensity", "Darkness of shadows");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.00f, 1.0f, 1, 3);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "render_pass", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "render_pass");
  RNA_def_property_enum_items(prop, rna_enum_view3dshading_render_pass_type_items);
  RNA_def_property_ui_text(prop, "Render Pass", "Render Pass to show in the viewport");
  RNA_def_property_enum_funcs(prop,
                              "rna_3DViewShading_render_pass_get",
                              "rna_3DViewShading_render_pass_set",
                              "rna_3DViewShading_render_pass_itemf");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "aov_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "aov_name");
  RNA_def_property_ui_text(prop, "Shader AOV Name", "Name of the active Shader AOV");
  RNA_def_property_flag(prop, PROP_HIDDEN);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);
}

static void rna_def_space_view3d_overlay(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "View3DOverlay", NULL);
  RNA_def_struct_sdna(srna, "View3D");
  RNA_def_struct_nested(brna, srna, "SpaceView3D");
  RNA_def_struct_path_func(srna, "rna_View3DOverlay_path");
  RNA_def_struct_ui_text(
      srna, "3D View Overlay Settings", "Settings for display of overlays in the 3D viewport");

  prop = RNA_def_property(srna, "show_overlays", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag2", V3D_HIDE_OVERLAYS);
  RNA_def_property_ui_text(prop, "Show Overlays", "Display overlays like gizmos and outlines");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_GPencil_update");

  prop = RNA_def_property(srna, "show_ortho_grid", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gridflag", V3D_SHOW_ORTHO_GRID);
  RNA_def_property_ui_text(prop, "Display Grid", "Show grid in orthographic side view");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_floor", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gridflag", V3D_SHOW_FLOOR);
  RNA_def_property_ui_text(prop, "Display Grid Floor", "Show the ground plane grid");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_axis_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gridflag", V3D_SHOW_X);
  RNA_def_property_ui_text(prop, "Display X Axis", "Show the X axis line");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_axis_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gridflag", V3D_SHOW_Y);
  RNA_def_property_ui_text(prop, "Display Y Axis", "Show the Y axis line");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_axis_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gridflag", V3D_SHOW_Z);
  RNA_def_property_ui_text(prop, "Display Z Axis", "Show the Z axis line");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "grid_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "grid");
  RNA_def_property_ui_text(
      prop, "Grid Scale", "Multiplier for the distance between 3D View grid lines");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001f, 1000.0f, 0.1f, 3);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "grid_lines", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "gridlines");
  RNA_def_property_ui_text(
      prop, "Grid Lines", "Number of grid lines to display in perspective view");
  RNA_def_property_range(prop, 0, 1024);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "grid_subdivisions", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "gridsubdiv");
  RNA_def_property_ui_text(prop, "Grid Subdivisions", "Number of subdivisions between grid lines");
  RNA_def_property_range(prop, 1, 1024);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "grid_scale_unit", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_View3DOverlay_GridScaleUnit_get", NULL, NULL);
  RNA_def_property_ui_text(
      prop, "Grid Scale Unit", "Grid cell size scaled by scene unit system settings");

  prop = RNA_def_property(srna, "show_outline_selected", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", V3D_SELECT_OUTLINE);
  RNA_def_property_ui_text(
      prop, "Outline Selected", "Show an outline highlight around selected objects");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_object_origins", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(
      prop, NULL, "overlay.flag", V3D_OVERLAY_HIDE_OBJECT_ORIGINS);
  RNA_def_property_ui_text(prop, "Object Origins", "Show object center dots");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_object_origins_all", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", V3D_DRAW_CENTERS);
  RNA_def_property_ui_text(
      prop,
      "All Object Origins",
      "Show the object origin center dot for all (selected and unselected) objects");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_relationship_lines", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", V3D_HIDE_HELPLINES);
  RNA_def_property_ui_text(prop,
                           "Relationship Lines",
                           "Show dashed lines indicating parent or constraint relationships");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_cursor", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "overlay.flag", V3D_OVERLAY_HIDE_CURSOR);
  RNA_def_property_ui_text(prop, "Show 3D Cursor", "Display 3D Cursor Overlay");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_text", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "overlay.flag", V3D_OVERLAY_HIDE_TEXT);
  RNA_def_property_ui_text(prop, "Show Text", "Display overlay text");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_stats", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.flag", V3D_OVERLAY_STATS);
  RNA_def_property_ui_text(prop, "Show Statistics", "Display scene statistics overlay text");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_extras", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(
      prop, NULL, "overlay.flag", V3D_OVERLAY_HIDE_OBJECT_XTRAS);
  RNA_def_property_ui_text(
      prop, "Extras", "Object details, including empty wire, cameras and other visual guides");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_bones", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "overlay.flag", V3D_OVERLAY_HIDE_BONES);
  RNA_def_property_ui_text(
      prop, "Show Bones", "Display bones (disable to show motion paths only)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_face_orientation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.flag", V3D_OVERLAY_FACE_ORIENTATION);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Face Orientation", "Show the Face Orientation Overlay");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_fade_inactive", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.flag", V3D_OVERLAY_FADE_INACTIVE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Fade Inactive Objects", "Fade inactive geometry using the viewport background color");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "fade_inactive_alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "overlay.fade_alpha");
  RNA_def_property_ui_text(prop, "Opacity", "Strength of the fade effect");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_GPencil_update");

  prop = RNA_def_property(srna, "show_xray_bone", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.flag", V3D_OVERLAY_BONE_SELECT);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Show Bone X-Ray", "Show the bone selection overlay");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_GPencil_update");

  prop = RNA_def_property(srna, "xray_alpha_bone", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "overlay.xray_alpha_bone");
  RNA_def_property_ui_text(prop, "Opacity", "Opacity to use for bone selection");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_GPencil_update");

  prop = RNA_def_property(srna, "show_motion_paths", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(
      prop, NULL, "overlay.flag", V3D_OVERLAY_HIDE_MOTION_PATHS);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Motion Paths", "Show the Motion Paths Overlay");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_onion_skins", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.flag", V3D_OVERLAY_ONION_SKINS);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Onion Skins", "Show the Onion Skinning Overlay");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_look_dev", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.flag", V3D_OVERLAY_LOOK_DEV);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "HDRI Preview", "Show HDRI preview spheres");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_wireframes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.flag", V3D_OVERLAY_WIREFRAMES);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Wireframe", "Show face edges wires");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "wireframe_threshold", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "overlay.wireframe_threshold");
  RNA_def_property_ui_text(prop,
                           "Wireframe Threshold",
                           "Adjust the angle threshold for displaying edges "
                           "(1.0 for all)");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "wireframe_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "overlay.wireframe_opacity");
  RNA_def_property_ui_text(prop,
                           "Wireframe Opacity",
                           "Opacity of the displayed edges "
                           "(1.0 for opaque)");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_paint_wire", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.paint_flag", V3D_OVERLAY_PAINT_WIRE);
  RNA_def_property_ui_text(prop, "Show Wire", "Use wireframe display in painting modes");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_wpaint_contours", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.wpaint_flag", V3D_OVERLAY_WPAINT_CONTOURS);
  RNA_def_property_ui_text(
      prop,
      "Show Weight Contours",
      "Show contour lines formed by points with the same interpolated weight");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_weight", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.edit_flag", V3D_OVERLAY_EDIT_WEIGHT);
  RNA_def_property_ui_text(prop, "Show Weights", "Display weights in editmode");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_occlude_wire", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.edit_flag", V3D_OVERLAY_EDIT_OCCLUDE_WIRE);
  RNA_def_property_ui_text(prop, "Hidden Wire", "Use hidden wireframe display");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);

  prop = RNA_def_property(srna, "show_face_normals", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.edit_flag", V3D_OVERLAY_EDIT_FACE_NORMALS);
  RNA_def_property_ui_text(prop, "Display Normals", "Display face normals as lines");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_vertex_normals", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.edit_flag", V3D_OVERLAY_EDIT_VERT_NORMALS);
  RNA_def_property_ui_text(prop, "Display Vertex Normals", "Display vertex normals as lines");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_split_normals", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.edit_flag", V3D_OVERLAY_EDIT_LOOP_NORMALS);
  RNA_def_property_ui_text(
      prop, "Display Split Normals", "Display vertex-per-face normals as lines");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_edges", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.edit_flag", V3D_OVERLAY_EDIT_EDGES);
  RNA_def_property_ui_text(prop, "Display Edges", "Highlight selected edges");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_faces", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.edit_flag", V3D_OVERLAY_EDIT_FACES);
  RNA_def_property_ui_text(prop, "Display Faces", "Highlight selected faces");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_face_center", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.edit_flag", V3D_OVERLAY_EDIT_FACE_DOT);
  RNA_def_property_ui_text(
      prop,
      "Display Face Center",
      "Display face center when face selection is enabled in solid shading modes");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_edge_crease", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.edit_flag", V3D_OVERLAY_EDIT_CREASES);
  RNA_def_property_ui_text(
      prop, "Display Creases", "Display creases created for Subdivision Surface modifier");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_edge_bevel_weight", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.edit_flag", V3D_OVERLAY_EDIT_BWEIGHTS);
  RNA_def_property_ui_text(
      prop, "Display Bevel Weights", "Display weights created for the Bevel modifier");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_edge_seams", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.edit_flag", V3D_OVERLAY_EDIT_SEAMS);
  RNA_def_property_ui_text(prop, "Display Seams", "Display UV unwrapping seams");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_edge_sharp", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.edit_flag", V3D_OVERLAY_EDIT_SHARP);
  RNA_def_property_ui_text(
      prop, "Display Sharp", "Display sharp edges, used with the Edge Split modifier");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_freestyle_edge_marks", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.edit_flag", V3D_OVERLAY_EDIT_FREESTYLE_EDGE);
  RNA_def_property_ui_text(prop,
                           "Display Freestyle Edge Marks",
                           "Display Freestyle edge marks, used with the Freestyle renderer");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_freestyle_face_marks", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.edit_flag", V3D_OVERLAY_EDIT_FREESTYLE_FACE);
  RNA_def_property_ui_text(prop,
                           "Display Freestyle Face Marks",
                           "Display Freestyle face marks, used with the Freestyle renderer");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_statvis", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.edit_flag", V3D_OVERLAY_EDIT_STATVIS);
  RNA_def_property_ui_text(prop, "Stat Vis", "Display statistical information about the mesh");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_extra_edge_length", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.edit_flag", V3D_OVERLAY_EDIT_EDGE_LEN);
  RNA_def_property_ui_text(
      prop,
      "Edge Length",
      "Display selected edge lengths, using global values when set in the transform panel");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_extra_edge_angle", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.edit_flag", V3D_OVERLAY_EDIT_EDGE_ANG);
  RNA_def_property_ui_text(
      prop,
      "Edge Angle",
      "Display selected edge angle, using global values when set in the transform panel");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_extra_face_angle", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.edit_flag", V3D_OVERLAY_EDIT_FACE_ANG);
  RNA_def_property_ui_text(prop,
                           "Face Angles",
                           "Display the angles in the selected edges, "
                           "using global values when set in the transform panel");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_extra_face_area", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.edit_flag", V3D_OVERLAY_EDIT_FACE_AREA);
  RNA_def_property_ui_text(prop,
                           "Face Area",
                           "Display the area of selected faces, "
                           "using global values when set in the transform panel");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_extra_indices", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.edit_flag", V3D_OVERLAY_EDIT_INDICES);
  RNA_def_property_ui_text(
      prop, "Indices", "Display the index numbers of selected vertices, edges, and faces");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "display_handle", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "overlay.handle_display");
  RNA_def_property_enum_items(prop, rna_enum_curve_display_handle_items);
  RNA_def_property_ui_text(
      prop, "Display Handles", "Limit the display of curve handles in edit mode");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_curve_normals", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.edit_flag", V3D_OVERLAY_EDIT_CU_NORMALS);
  RNA_def_property_ui_text(prop, "Draw Normals", "Display 3D curve normals in editmode");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "normals_length", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "overlay.normals_length");
  RNA_def_property_ui_text(prop, "Normal Size", "Display size for normals in the 3D view");
  RNA_def_property_range(prop, 0.00001, 100000.0);
  RNA_def_property_ui_range(prop, 0.01, 2.0, 1, 2);
  RNA_def_property_float_default(prop, 0.02);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "backwire_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "overlay.backwire_opacity");
  RNA_def_property_ui_text(prop, "Backwire Opacity", "Opacity when rendering transparent wires");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "texture_paint_mode_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "overlay.texture_paint_mode_opacity");
  RNA_def_property_ui_text(
      prop, "Stencil Mask Opacity", "Opacity of the texture paint mode stencil mask overlay");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "vertex_paint_mode_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "overlay.vertex_paint_mode_opacity");
  RNA_def_property_ui_text(
      prop, "Stencil Mask Opacity", "Opacity of the texture paint mode stencil mask overlay");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "weight_paint_mode_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "overlay.weight_paint_mode_opacity");
  RNA_def_property_ui_text(
      prop, "Weight Paint Opacity", "Opacity of the weight paint mode overlay");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "sculpt_mode_mask_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "overlay.sculpt_mode_mask_opacity");
  RNA_def_property_ui_text(prop, "Sculpt Mask Opacity", "");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "sculpt_mode_face_sets_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "overlay.sculpt_mode_face_sets_opacity");
  RNA_def_property_ui_text(prop, "Sculpt Face Sets Opacity", "");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  /* grease pencil paper settings */
  prop = RNA_def_property(srna, "show_annotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag2", V3D_SHOW_ANNOTATION);
  RNA_def_property_ui_text(prop, "Show Annotation", "Show annotations for this view");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "use_gpencil_fade_objects", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gp_flag", V3D_GP_FADE_OBJECTS);
  RNA_def_property_ui_text(
      prop,
      "Fade Objects",
      "Fade all viewport objects with a full color layer to improve visibility");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "use_gpencil_grid", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gp_flag", V3D_GP_SHOW_GRID);
  RNA_def_property_ui_text(prop, "Use Grid", "Display a grid over grease pencil paper");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "use_gpencil_fade_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gp_flag", V3D_GP_FADE_NOACTIVE_LAYERS);
  RNA_def_property_ui_text(
      prop, "Fade Layers", "Toggle fading of Grease Pencil layers except the active one");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_GPencil_update");

  prop = RNA_def_property(srna, "use_gpencil_fade_gp_objects", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gp_flag", V3D_GP_FADE_NOACTIVE_GPENCIL);
  RNA_def_property_ui_text(
      prop, "Fade Grease Pencil Objects", "Fade Grease Pencil Objects, except the active one");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_GPencil_update");

  prop = RNA_def_property(srna, "use_gpencil_canvas_xray", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gp_flag", V3D_GP_SHOW_GRID_XRAY);
  RNA_def_property_ui_text(prop, "Canvas X-Ray", "Show Canvas grid in front");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_GPencil_update");

  prop = RNA_def_property(srna, "use_gpencil_show_directions", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gp_flag", V3D_GP_SHOW_STROKE_DIRECTION);
  RNA_def_property_ui_text(prop,
                           "Stroke Direction",
                           "Show stroke drawing direction with a bigger green dot (start) "
                           "and smaller red dot (end) points");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_GPencil_update");

  prop = RNA_def_property(srna, "use_gpencil_show_material_name", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gp_flag", V3D_GP_SHOW_MATERIAL_NAME);
  RNA_def_property_ui_text(
      prop, "Stroke Material Name", "Show material name assigned to each stroke");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_GPencil_update");

  prop = RNA_def_property(srna, "gpencil_grid_opacity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "overlay.gpencil_grid_opacity");
  RNA_def_property_range(prop, 0.1f, 1.0f);
  RNA_def_property_ui_text(prop, "Opacity", "Canvas grid opacity");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  /* Paper opacity factor */
  prop = RNA_def_property(srna, "gpencil_fade_objects", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "overlay.gpencil_paper_opacity");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Opacity", "Fade factor");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  /* Paper opacity factor */
  prop = RNA_def_property(srna, "gpencil_fade_layer", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "overlay.gpencil_fade_layer");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_ui_text(
      prop, "Opacity", "Fade layer opacity for Grease Pencil layers except the active one");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_GPencil_update");

  /* show edit lines */
  prop = RNA_def_property(srna, "use_gpencil_edit_lines", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gp_flag", V3D_GP_SHOW_EDIT_LINES);
  RNA_def_property_ui_text(prop, "Show Edit Lines", "Show Edit Lines when editing strokes");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_GPencil_update");

  prop = RNA_def_property(srna, "use_gpencil_multiedit_line_only", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gp_flag", V3D_GP_SHOW_MULTIEDIT_LINES);
  RNA_def_property_ui_text(prop, "Lines Only", "Show Edit Lines only in multiframe");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_GPencil_update");

  /* main grease pencil onion switch */
  prop = RNA_def_property(srna, "use_gpencil_onion_skin", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gp_flag", V3D_GP_SHOW_ONION_SKIN);
  RNA_def_property_ui_text(
      prop, "Onion Skins", "Show ghosts of the keyframes before and after the current frame");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_GPencil_update");

  /* vertex opacity */
  prop = RNA_def_property(srna, "vertex_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "vertex_opacity");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Vertex Opacity", "Opacity for edit vertices");
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, 0);
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, "rna_GPencil_update");

  /* Vertex Paint opacity factor */
  prop = RNA_def_property(srna, "gpencil_vertex_paint_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "overlay.gpencil_vertex_paint_opacity");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_ui_text(prop, "Opacity", "Vertex Paint mix factor");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_GPencil_update");
}

static void rna_def_space_view3d(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem rv3d_persp_items[] = {
      {RV3D_PERSP, "PERSP", 0, "Perspective", ""},
      {RV3D_ORTHO, "ORTHO", 0, "Orthographic", ""},
      {RV3D_CAMOB, "CAMERA", 0, "Camera", ""},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem bundle_drawtype_items[] = {
      {OB_PLAINAXES, "PLAIN_AXES", 0, "Plain Axes", ""},
      {OB_ARROWS, "ARROWS", 0, "Arrows", ""},
      {OB_SINGLE_ARROW, "SINGLE_ARROW", 0, "Single Arrow", ""},
      {OB_CIRCLE, "CIRCLE", 0, "Circle", ""},
      {OB_CUBE, "CUBE", 0, "Cube", ""},
      {OB_EMPTY_SPHERE, "SPHERE", 0, "Sphere", ""},
      {OB_EMPTY_CONE, "CONE", 0, "Cone", ""},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "SpaceView3D", "Space");
  RNA_def_struct_sdna(srna, "View3D");
  RNA_def_struct_ui_text(srna, "3D View Space", "3D View space data");

  rna_def_space_generic_show_region_toggles(srna,
                                            ((1 << RGN_TYPE_TOOL_HEADER) | (1 << RGN_TYPE_TOOLS) |
                                             (1 << RGN_TYPE_UI) | (1 << RGN_TYPE_HUD)));

  prop = RNA_def_property(srna, "camera", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_sdna(prop, NULL, "camera");
  RNA_def_property_ui_text(
      prop,
      "Camera",
      "Active camera used in this view (when unlocked from the scene's active camera)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_SpaceView3D_camera_update");

  /* render border */
  prop = RNA_def_property(srna, "use_render_border", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag2", V3D_RENDER_BORDER);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop,
                           "Render Region",
                           "Use a region within the frame size for rendered viewport "
                           "(when not viewing through the camera)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "render_border_min_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "render_border.xmin");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Region Minimum X", "Minimum X value for the render region");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "render_border_min_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "render_border.ymin");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Region Minimum Y", "Minimum Y value for the render region");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "render_border_max_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "render_border.xmax");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Region Maximum X", "Maximum X value for the render region");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "render_border_max_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "render_border.ymax");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Region Maximum Y", "Maximum Y value for the render region");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "lock_object", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_sdna(prop, NULL, "ob_center");
  RNA_def_property_ui_text(
      prop, "Lock to Object", "3D View center is locked to this object's position");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "lock_bone", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "ob_center_bone");
  RNA_def_property_ui_text(
      prop, "Lock to Bone", "3D View center is locked to this bone's position");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "lock_cursor", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "ob_center_cursor", 1);
  RNA_def_property_ui_text(
      prop, "Lock to Cursor", "3D View center is locked to the cursor's position");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "local_view", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "localvd");
  RNA_def_property_ui_text(
      prop,
      "Local View",
      "Display an isolated subset of objects, apart from the scene visibility");

  prop = RNA_def_property(srna, "lens", PROP_FLOAT, PROP_UNIT_CAMERA);
  RNA_def_property_float_sdna(prop, NULL, "lens");
  RNA_def_property_ui_text(prop, "Lens", "Viewport lens angle");
  RNA_def_property_range(prop, 1.0f, 250.0f);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "clip_start", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_range(prop, 1e-6f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 10, 3);
  RNA_def_property_ui_text(
      prop, "Clip Start", "3D View near clipping distance (perspective view only)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "clip_end", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_range(prop, 1e-6f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 10, 3);
  RNA_def_property_ui_text(prop, "Clip End", "3D View far clipping distance");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "lock_camera", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag2", V3D_LOCK_CAMERA);
  RNA_def_property_ui_text(
      prop, "Lock Camera to View", "Enable view navigation within the camera view");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_gizmo", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "gizmo_flag", V3D_GIZMO_HIDE);
  RNA_def_property_ui_text(prop, "Show Gizmo", "Show gizmos of all types");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_gizmo_navigate", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "gizmo_flag", V3D_GIZMO_HIDE_NAVIGATE);
  RNA_def_property_ui_text(prop, "Navigate Gizmo", "Viewport navigation gizmo");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_gizmo_context", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "gizmo_flag", V3D_GIZMO_HIDE_CONTEXT);
  RNA_def_property_ui_text(prop, "Context Gizmo", "Context sensitive gizmos for the active item");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_gizmo_tool", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "gizmo_flag", V3D_GIZMO_HIDE_TOOL);
  RNA_def_property_ui_text(prop, "Tool Gizmo", "Active tool gizmo");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  /* Per object type gizmo display flags. */

  prop = RNA_def_property(srna, "show_gizmo_object_translate", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gizmo_show_object", V3D_GIZMO_SHOW_OBJECT_TRANSLATE);
  RNA_def_property_ui_text(prop, "Show Object Location", "Gizmo to adjust location");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_gizmo_object_rotate", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gizmo_show_object", V3D_GIZMO_SHOW_OBJECT_ROTATE);
  RNA_def_property_ui_text(prop, "Show Object Rotation", "Gizmo to adjust rotation");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_gizmo_object_scale", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gizmo_show_object", V3D_GIZMO_SHOW_OBJECT_SCALE);
  RNA_def_property_ui_text(prop, "Show Object Scale", "Gizmo to adjust scale");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  /* Empty Object Data. */
  prop = RNA_def_property(srna, "show_gizmo_empty_image", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gizmo_show_empty", V3D_GIZMO_SHOW_EMPTY_IMAGE);
  RNA_def_property_ui_text(prop, "Show Empty Image", "Gizmo to adjust image size and position");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_gizmo_empty_force_field", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gizmo_show_empty", V3D_GIZMO_SHOW_EMPTY_FORCE_FIELD);
  RNA_def_property_ui_text(prop, "Show Empty Force Field", "Gizmo to adjust the force field");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  /* Light Object Data. */
  prop = RNA_def_property(srna, "show_gizmo_light_size", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gizmo_show_light", V3D_GIZMO_SHOW_LIGHT_SIZE);
  RNA_def_property_ui_text(prop, "Show Light Size", "Gizmo to adjust spot and area size");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_gizmo_light_look_at", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gizmo_show_light", V3D_GIZMO_SHOW_LIGHT_LOOK_AT);
  RNA_def_property_ui_text(
      prop, "Show Light Look-At", "Gizmo to adjust the direction of the light");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  /* Camera Object Data. */
  prop = RNA_def_property(srna, "show_gizmo_camera_lens", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gizmo_show_camera", V3D_GIZMO_SHOW_CAMERA_LENS);
  RNA_def_property_ui_text(
      prop, "Show Camera Lens", "Gizmo to adjust camera focal length or orthographic scale");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_gizmo_camera_dof_distance", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "gizmo_show_camera", V3D_GIZMO_SHOW_CAMERA_DOF_DIST);
  RNA_def_property_ui_text(prop,
                           "Show Camera Focus Distance",
                           "Gizmo to adjust camera focus distance "
                           "(depends on limits display)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "use_local_camera", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "scenelock", 1);
  RNA_def_property_boolean_funcs(prop, NULL, "rna_SpaceView3D_use_local_camera_set");
  RNA_def_property_ui_text(prop,
                           "Use Local Camera",
                           "Use a local camera in this view, rather than scene's active camera");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "region_3d", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "RegionView3D");
  RNA_def_property_pointer_funcs(prop, "rna_SpaceView3D_region_3d_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(
      prop, "3D Region", "3D region in this space, in case of quad view the camera region");

  prop = RNA_def_property(srna, "region_quadviews", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "RegionView3D");
  RNA_def_property_collection_funcs(prop,
                                    "rna_SpaceView3D_region_quadviews_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_SpaceView3D_region_quadviews_get",
                                    NULL,
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_ui_text(prop,
                           "Quad View Regions",
                           "3D regions (the third one defines quad view settings, "
                           "the fourth one is same as 'region_3d')");

  prop = RNA_def_property(srna, "show_reconstruction", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag2", V3D_SHOW_RECONSTRUCTION);
  RNA_def_property_ui_text(
      prop, "Show Reconstruction", "Display reconstruction data from active movie clip");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "tracks_display_size", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 5, 1, 3);
  RNA_def_property_float_sdna(prop, NULL, "bundle_size");
  RNA_def_property_ui_text(prop, "Tracks Size", "Display size of tracks from reconstructed data");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "tracks_display_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "bundle_drawtype");
  RNA_def_property_enum_items(prop, bundle_drawtype_items);
  RNA_def_property_ui_text(prop, "Tracks Display Type", "Viewport display style for tracks");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_camera_path", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag2", V3D_SHOW_CAMERAPATH);
  RNA_def_property_ui_text(prop, "Show Camera Path", "Show reconstructed camera path");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_bundle_names", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag2", V3D_SHOW_BUNDLENAME);
  RNA_def_property_ui_text(
      prop, "Show 3D Marker Names", "Show names for reconstructed tracks objects");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "use_local_collections", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", V3D_LOCAL_COLLECTIONS);
  RNA_def_property_ui_text(
      prop, "Local Collections", "Display a different set of collections in this viewport");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_SpaceView3D_use_local_collections_update");

  /* Stereo Settings */
  prop = RNA_def_property(srna, "stereo_3d_eye", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "multiview_eye");
  RNA_def_property_enum_items(prop, stereo3d_eye_items);
  RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_SpaceView3D_stereo3d_camera_itemf");
  RNA_def_property_ui_text(prop, "Stereo Eye", "Current stereo eye being displayed");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "stereo_3d_camera", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "stereo3d_camera");
  RNA_def_property_enum_items(prop, stereo3d_camera_items);
  RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_SpaceView3D_stereo3d_camera_itemf");
  RNA_def_property_ui_text(prop, "Camera", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_stereo_3d_cameras", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "stereo3d_flag", V3D_S3D_DISPCAMERAS);
  RNA_def_property_ui_text(prop, "Cameras", "Show the left and right cameras");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_stereo_3d_convergence_plane", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "stereo3d_flag", V3D_S3D_DISPPLANE);
  RNA_def_property_ui_text(prop, "Plane", "Show the stereo 3D convergence plane");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "stereo_3d_convergence_plane_alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "stereo3d_convergence_alpha");
  RNA_def_property_ui_text(prop, "Plane Alpha", "Opacity (alpha) of the convergence plane");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "show_stereo_3d_volume", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "stereo3d_flag", V3D_S3D_DISPVOLUME);
  RNA_def_property_ui_text(prop, "Volume", "Show the stereo 3D frustum volume");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "stereo_3d_volume_alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, NULL, "stereo3d_volume_alpha");
  RNA_def_property_ui_text(prop, "Volume Alpha", "Opacity (alpha) of the cameras' frustum volume");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "mirror_xr_session", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", V3D_XR_SESSION_MIRROR);
  RNA_def_property_ui_text(
      prop,
      "Mirror VR Session",
      "Synchronize the viewer perspective of virtual reality sessions with this 3D viewport");
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_SpaceView3D_mirror_xr_session_update");

  {
    struct {
      const char *name;
      int type_mask;
      const char *identifier[2];
    } info[] = {
        {"Mesh", (1 << OB_MESH), {"show_object_viewport_mesh", "show_object_select_mesh"}},
        {"Curve", (1 << OB_CURVE), {"show_object_viewport_curve", "show_object_select_curve"}},
        {"Surface", (1 << OB_SURF), {"show_object_viewport_surf", "show_object_select_surf"}},
        {"Meta", (1 << OB_MBALL), {"show_object_viewport_meta", "show_object_select_meta"}},
        {"Font", (1 << OB_FONT), {"show_object_viewport_font", "show_object_select_font"}},
        {"Hair", (1 << OB_HAIR), {"show_object_viewport_hair", "show_object_select_hair"}},
        {"Point Cloud",
         (1 << OB_POINTCLOUD),
         {"show_object_viewport_pointcloud", "show_object_select_pointcloud"}},
        {"Volume", (1 << OB_VOLUME), {"show_object_viewport_volume", "show_object_select_volume"}},
        {"Armature",
         (1 << OB_ARMATURE),
         {"show_object_viewport_armature", "show_object_select_armature"}},
        {"Lattice",
         (1 << OB_LATTICE),
         {"show_object_viewport_lattice", "show_object_select_lattice"}},
        {"Empty", (1 << OB_EMPTY), {"show_object_viewport_empty", "show_object_select_empty"}},
        {"Grease Pencil",
         (1 << OB_GPENCIL),
         {"show_object_viewport_grease_pencil", "show_object_select_grease_pencil"}},
        {"Camera", (1 << OB_CAMERA), {"show_object_viewport_camera", "show_object_select_camera"}},
        {"Light", (1 << OB_LAMP), {"show_object_viewport_light", "show_object_select_light"}},
        {"Speaker",
         (1 << OB_SPEAKER),
         {"show_object_viewport_speaker", "show_object_select_speaker"}},
        {"Light Probe",
         (1 << OB_LIGHTPROBE),
         {"show_object_viewport_light_probe", "show_object_select_light_probe"}},
    };

    const char *view_mask_member[2] = {
        "object_type_exclude_viewport",
        "object_type_exclude_select",
    };
    for (int mask_index = 0; mask_index < 2; mask_index++) {
      for (int type_index = 0; type_index < ARRAY_SIZE(info); type_index++) {
        prop = RNA_def_property(
            srna, info[type_index].identifier[mask_index], PROP_BOOLEAN, PROP_NONE);
        RNA_def_property_boolean_negative_sdna(
            prop, NULL, view_mask_member[mask_index], info[type_index].type_mask);
        RNA_def_property_ui_text(prop, info[type_index].name, "");
        RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, NULL);
      }
    }

    /* Helper for drawing the icon. */
    prop = RNA_def_property(srna, "icon_from_show_object_viewport", PROP_INT, PROP_NONE);
    RNA_def_property_int_funcs(
        prop, "rna_SpaceView3D_icon_from_show_object_viewport_get", NULL, NULL);
    RNA_def_property_clear_flag(prop, PROP_EDITABLE);
    RNA_def_property_ui_text(prop, "Visibility Icon", "");
  }

  /* Nested Structs */
  prop = RNA_def_property(srna, "shading", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "View3DShading");
  RNA_def_property_ui_text(prop, "Shading Settings", "Settings for shading in the 3D viewport");

  prop = RNA_def_property(srna, "overlay", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "View3DOverlay");
  RNA_def_property_pointer_funcs(prop, "rna_SpaceView3D_overlay_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(
      prop, "Overlay Settings", "Settings for display of overlays in the 3D viewport");

  rna_def_space_view3d_shading(brna);
  rna_def_space_view3d_overlay(brna);

  /* *** Animated *** */
  RNA_define_animate_sdna(true);
  /* region */

  srna = RNA_def_struct(brna, "RegionView3D", NULL);
  RNA_def_struct_sdna(srna, "RegionView3D");
  RNA_def_struct_ui_text(srna, "3D View Region", "3D View region data");

  prop = RNA_def_property(srna, "lock_rotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "viewlock", RV3D_LOCK_ROTATION);
  RNA_def_property_ui_text(prop, "Lock", "Lock view rotation in side views");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_RegionView3D_quadview_update");

  prop = RNA_def_property(srna, "show_sync_view", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "viewlock", RV3D_BOXVIEW);
  RNA_def_property_ui_text(prop, "Box", "Sync view position between side views");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_RegionView3D_quadview_update");

  prop = RNA_def_property(srna, "use_box_clip", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "viewlock", RV3D_BOXCLIP);
  RNA_def_property_ui_text(
      prop, "Clip", "Clip objects based on what's visible in other side views");
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_RegionView3D_quadview_clip_update");

  prop = RNA_def_property(srna, "perspective_matrix", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_float_sdna(prop, NULL, "persmat");
  RNA_def_property_clear_flag(
      prop, PROP_EDITABLE); /* XXX: for now, it's too risky for users to do this */
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(
      prop, "Perspective Matrix", "Current perspective matrix (``window_matrix * view_matrix``)");

  prop = RNA_def_property(srna, "window_matrix", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_float_sdna(prop, NULL, "winmat");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(prop, "Window Matrix", "Current window matrix");

  prop = RNA_def_property(srna, "view_matrix", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_float_sdna(prop, NULL, "viewmat");
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_float_funcs(prop, NULL, "rna_RegionView3D_view_matrix_set", NULL);
  RNA_def_property_ui_text(prop, "View Matrix", "Current view matrix");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "view_perspective", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "persp");
  RNA_def_property_enum_items(prop, rv3d_persp_items);
  RNA_def_property_ui_text(prop, "Perspective", "View Perspective");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "is_perspective", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "is_persp", 1);
  RNA_def_property_ui_text(prop, "Is Perspective", "");
  RNA_def_property_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "is_orthographic_side_view", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "view", 0);
  RNA_def_property_boolean_funcs(prop, "rna_RegionView3D_is_orthographic_side_view_get", NULL);
  RNA_def_property_ui_text(prop, "Is Axis Aligned", "Is current view an orthographic side view");

  /* This isn't directly accessible from the UI, only an operator. */
  prop = RNA_def_property(srna, "use_clip_planes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "rflag", RV3D_CLIPPING);
  RNA_def_property_ui_text(prop, "Use Clip Planes", "");

  prop = RNA_def_property(srna, "clip_planes", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "clip");
  RNA_def_property_multi_array(prop, 2, (int[]){6, 4});
  RNA_def_property_ui_text(prop, "Clip Planes", "");

  prop = RNA_def_property(srna, "view_location", PROP_FLOAT, PROP_TRANSLATION);
#  if 0
  RNA_def_property_float_sdna(prop, NULL, "ofs"); /* cant use because its negated */
#  else
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(
      prop, "rna_RegionView3D_view_location_get", "rna_RegionView3D_view_location_set", NULL);
#  endif
  RNA_def_property_ui_text(prop, "View Location", "View pivot location");
  RNA_def_property_ui_range(prop, -10000.0, 10000.0, 10, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, NC_WINDOW, NULL);

  prop = RNA_def_property(
      srna, "view_rotation", PROP_FLOAT, PROP_QUATERNION); /* cant use because its inverted */
#  if 0
  RNA_def_property_float_sdna(prop, NULL, "viewquat");
#  else
  RNA_def_property_array(prop, 4);
  RNA_def_property_float_funcs(
      prop, "rna_RegionView3D_view_rotation_get", "rna_RegionView3D_view_rotation_set", NULL);
#  endif
  RNA_def_property_ui_text(prop, "View Rotation", "Rotation in quaternions (keep normalized)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  /* not sure we need rna access to these but adding anyway */
  prop = RNA_def_property(srna, "view_distance", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, NULL, "dist");
  RNA_def_property_ui_text(prop, "Distance", "Distance to the view location");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "view_camera_zoom", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, NULL, "camzoom");
  RNA_def_property_ui_text(prop, "Camera Zoom", "Zoom factor in camera view");
  RNA_def_property_range(prop, RV3D_CAMZOOM_MIN, RV3D_CAMZOOM_MAX);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  prop = RNA_def_property(srna, "view_camera_offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "camdx");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "Camera Offset", "View shift in camera view");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, NULL);

  RNA_api_region_view3d(srna);
}

static void rna_def_space_properties(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem tab_sync_items[] = {
      {PROPERTIES_SYNC_ALWAYS,
       "ALWAYS",
       0,
       "Always",
       "Always change tabs when clicking an icon in an outliner"},
      {PROPERTIES_SYNC_NEVER,
       "NEVER",
       0,
       "Never",
       "Never change tabs when clicking an icon in an outliner"},
      {PROPERTIES_SYNC_AUTO,
       "AUTO",
       0,
       "Auto",
       "Change tabs only when this editor shares a border with an outliner"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "SpaceProperties", "Space");
  RNA_def_struct_sdna(srna, "SpaceProperties");
  RNA_def_struct_ui_text(srna, "Properties Space", "Properties space data");

  prop = RNA_def_property(srna, "context", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "mainb");
  RNA_def_property_enum_items(prop, buttons_context_items);
  RNA_def_property_enum_funcs(
      prop, NULL, "rna_SpaceProperties_context_set", "rna_SpaceProperties_context_itemf");
  RNA_def_property_ui_text(prop, "", "");
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_PROPERTIES, "rna_SpaceProperties_context_update");

  /* pinned data */
  prop = RNA_def_property(srna, "pin_id", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "pinid");
  RNA_def_property_struct_type(prop, "ID");
  /* note: custom set function is ONLY to avoid rna setting a user for this. */
  RNA_def_property_pointer_funcs(
      prop, NULL, "rna_SpaceProperties_pin_id_set", "rna_SpaceProperties_pin_id_typef", NULL);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_PROPERTIES, "rna_SpaceProperties_pin_id_update");

  prop = RNA_def_property(srna, "use_pin_id", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SB_PIN_CONTEXT);
  RNA_def_property_ui_text(prop, "Pin ID", "Use the pinned context");

  /* Property search. */

  prop = RNA_def_property(srna, "tab_search_results", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_array(prop, 0); /* Dynamic length, see next line. */
  RNA_def_property_flag(prop, PROP_DYNAMIC);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_SpaceProperties_tab_search_results_get", NULL);
  RNA_def_property_dynamic_array_funcs(prop, "rna_SpaceProperties_tab_search_results_getlength");
  RNA_def_property_ui_text(
      prop, "Tab Search Results", "Whether or not each visible tab has a search result");

  prop = RNA_def_property(srna, "search_filter", PROP_STRING, PROP_NONE);
  /* The search filter is stored in the property editor's runtime struct which
   * is only defined in an internal header, so use the getter / setter here. */
  RNA_def_property_string_funcs(prop,
                                "rna_SpaceProperties_search_filter_get",
                                "rna_SpaceProperties_search_filter_length",
                                "rna_SpaceProperties_search_filter_set");
  RNA_def_property_ui_text(prop, "Display Filter", "Live search filtering string");
  RNA_def_property_flag(prop, PROP_TEXTEDIT_UPDATE);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_PROPERTIES, "rna_SpaceProperties_search_filter_update");

  /* Outliner sync. */
  prop = RNA_def_property(srna, "outliner_sync", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "outliner_sync");
  RNA_def_property_enum_items(prop, tab_sync_items);
  RNA_def_property_ui_text(prop,
                           "Outliner Sync",
                           "Change to the corresponding tab when outliner data icons are clicked");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_PROPERTIES, NULL);
}

static void rna_def_space_image_overlay(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SpaceImageOverlay", NULL);
  RNA_def_struct_sdna(srna, "SpaceImage");
  RNA_def_struct_nested(brna, srna, "SpaceImageEditor");
  RNA_def_struct_path_func(srna, "rna_SpaceImageOverlay_path");
  RNA_def_struct_ui_text(
      srna, "Overlay Settings", "Settings for display of overlays in the UV/Image editor");

  prop = RNA_def_property(srna, "show_overlays", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overlay.flag", SI_OVERLAY_SHOW_OVERLAYS);
  RNA_def_property_ui_text(prop, "Show Overlays", "Display overlays like UV Maps and Metadata");
}

static void rna_def_space_image(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SpaceImageEditor", "Space");
  RNA_def_struct_sdna(srna, "SpaceImage");
  RNA_def_struct_ui_text(srna, "Space Image Editor", "Image and UV editor space data");

  rna_def_space_generic_show_region_toggles(srna,
                                            ((1 << RGN_TYPE_TOOL_HEADER) | (1 << RGN_TYPE_TOOLS) |
                                             (1 << RGN_TYPE_UI) | (1 << RGN_TYPE_HUD)));

  /* image */
  prop = RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_funcs(prop, NULL, "rna_SpaceImageEditor_image_set", NULL, NULL);
  RNA_def_property_ui_text(prop, "Image", "Image displayed and edited in this space");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(
      prop,
      NC_GEOM | ND_DATA,
      "rna_SpaceImageEditor_image_update"); /* is handled in image editor too */

  prop = RNA_def_property(srna, "image_user", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, NULL, "iuser");
  RNA_def_property_ui_text(
      prop,
      "Image User",
      "Parameters defining which layer, pass and frame of the image is displayed");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

  prop = RNA_def_property(srna, "scopes", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "scopes");
  RNA_def_property_struct_type(prop, "Scopes");
  RNA_def_property_ui_text(prop, "Scopes", "Scopes to visualize image statistics");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, "rna_SpaceImageEditor_scopes_update");

  prop = RNA_def_property(srna, "use_image_pin", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "pin", 0);
  RNA_def_property_ui_text(
      prop, "Image Pin", "Display current image regardless of object selection");
  RNA_def_property_ui_icon(prop, ICON_UNPINNED, 1);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

  prop = RNA_def_property(srna, "sample_histogram", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "sample_line_hist");
  RNA_def_property_struct_type(prop, "Histogram");
  RNA_def_property_ui_text(prop, "Line Sample", "Sampled colors along line");

  prop = RNA_def_property(srna, "zoom", PROP_FLOAT, PROP_NONE);
  RNA_def_property_array(prop, 2);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_SpaceImageEditor_zoom_get", NULL, NULL);
  RNA_def_property_ui_text(prop, "Zoom", "Zoom factor");

  /* image draw */
  prop = RNA_def_property(srna, "show_repeat", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_DRAW_TILE);
  RNA_def_property_ui_text(
      prop, "Display Repeated", "Display the image repeated outside of the main view");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

  prop = RNA_def_property(srna, "show_annotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SI_SHOW_GPENCIL);
  RNA_def_property_ui_text(prop, "Show Annotation", "Show annotations for this view");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

  prop = RNA_def_property(srna, "display_channels", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
  RNA_def_property_enum_items(prop, display_channels_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_SpaceImageEditor_display_channels_get",
                              NULL,
                              "rna_SpaceImageEditor_display_channels_itemf");
  RNA_def_property_ui_text(prop, "Display Channels", "Channels of the image to display");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

  prop = RNA_def_property(srna, "show_stereo_3d", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_SpaceImageEditor_show_stereo_get", "rna_SpaceImageEditor_show_stereo_set");
  RNA_def_property_ui_text(prop, "Show Stereo", "Display the image in Stereo 3D");
  RNA_def_property_ui_icon(prop, ICON_CAMERA_STEREO, 0);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_IMAGE, "rna_SpaceImageEditor_show_stereo_update");

  /* uv */
  prop = RNA_def_property(srna, "uv_editor", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "SpaceUVEditor");
  RNA_def_property_pointer_funcs(prop, "rna_SpaceImageEditor_uvedit_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(prop, "UV Editor", "UV editor settings");

  /* mode (hidden in the UI, see 'ui_mode') */
  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "mode");
  RNA_def_property_enum_items(prop, rna_enum_space_image_mode_all_items);
  RNA_def_property_ui_text(prop, "Mode", "Editing context being displayed");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, "rna_SpaceImageEditor_mode_update");

  prop = RNA_def_property(srna, "ui_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "mode");
  RNA_def_property_enum_items(prop, rna_enum_space_image_mode_ui_items);
  RNA_def_property_ui_text(prop, "Mode", "Editing context being displayed");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, "rna_SpaceImageEditor_mode_update");

  /* transform */
  prop = RNA_def_property(srna, "cursor_location", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_array(prop, 2);
  RNA_def_property_float_funcs(prop,
                               "rna_SpaceImageEditor_cursor_location_get",
                               "rna_SpaceImageEditor_cursor_location_set",
                               NULL);
  RNA_def_property_ui_text(prop, "2D Cursor Location", "2D cursor location for this view");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

  prop = RNA_def_property(srna, "pivot_point", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "around");
  RNA_def_property_enum_items(prop, rna_enum_transform_pivot_items_full);
  RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_SpaceImageEditor_pivot_itemf");
  RNA_def_property_ui_text(prop, "Pivot", "Rotation/Scaling Pivot");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

  /* grease pencil */
  prop = RNA_def_property(srna, "grease_pencil", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "gpd");
  RNA_def_property_struct_type(prop, "GreasePencil");
  RNA_def_property_pointer_funcs(
      prop, NULL, NULL, NULL, "rna_GPencil_datablocks_annotations_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_ui_text(prop, "Grease Pencil", "Grease pencil data for this space");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, NULL);

  /* update */
  prop = RNA_def_property(srna, "use_realtime_update", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "lock", 0);
  RNA_def_property_ui_text(prop,
                           "Update Automatically",
                           "Update other affected window spaces automatically to reflect changes "
                           "during interactive operations such as transform");

  /* state */
  prop = RNA_def_property(srna, "show_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_SpaceImageEditor_show_render_get", NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Show Render", "Show render related properties");

  prop = RNA_def_property(srna, "show_paint", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_SpaceImageEditor_show_paint_get", NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Show Paint", "Show paint related properties");

  prop = RNA_def_property(srna, "show_uvedit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_SpaceImageEditor_show_uvedit_get", NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Show UV Editor", "Show UV editing related properties");

  prop = RNA_def_property(srna, "show_maskedit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_SpaceImageEditor_show_maskedit_get", NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Show Mask Editor", "Show Mask editing related properties");

  /* Overlays */
  prop = RNA_def_property(srna, "overlay", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "SpaceImageOverlay");
  RNA_def_property_pointer_funcs(prop, "rna_SpaceImage_overlay_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(
      prop, "Overlay Settings", "Settings for display of overlays in the UV/Image editor");

  rna_def_space_image_uv(brna);
  rna_def_space_image_overlay(brna);

  /* mask */
  rna_def_space_mask_info(srna, NC_SPACE | ND_SPACE_IMAGE, "rna_SpaceImageEditor_mask_set");
}

static void rna_def_space_sequencer(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem display_mode_items[] = {
      {SEQ_DRAW_IMG_IMBUF, "IMAGE", ICON_SEQ_PREVIEW, "Image Preview", ""},
      {SEQ_DRAW_IMG_WAVEFORM, "WAVEFORM", ICON_SEQ_LUMA_WAVEFORM, "Luma Waveform", ""},
      {SEQ_DRAW_IMG_VECTORSCOPE, "VECTOR_SCOPE", ICON_SEQ_CHROMA_SCOPE, "Chroma Vectorscope", ""},
      {SEQ_DRAW_IMG_HISTOGRAM, "HISTOGRAM", ICON_SEQ_HISTOGRAM, "Histogram", ""},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem proxy_render_size_items[] = {
      {SEQ_RENDER_SIZE_NONE, "NONE", 0, "No display", ""},
      {SEQ_RENDER_SIZE_SCENE, "SCENE", 0, "Scene size", ""},
      {SEQ_RENDER_SIZE_PROXY_25, "PROXY_25", 0, "25%", ""},
      {SEQ_RENDER_SIZE_PROXY_50, "PROXY_50", 0, "50%", ""},
      {SEQ_RENDER_SIZE_PROXY_75, "PROXY_75", 0, "75%", ""},
      {SEQ_RENDER_SIZE_PROXY_100, "PROXY_100", 0, "100%", ""},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem overlay_type_items[] = {
      {SEQ_DRAW_OVERLAY_RECT, "RECTANGLE", 0, "Rectangle", "Show rectangle area overlay"},
      {SEQ_DRAW_OVERLAY_REFERENCE, "REFERENCE", 0, "Reference", "Show reference frame only"},
      {SEQ_DRAW_OVERLAY_CURRENT, "CURRENT", 0, "Current", "Show current frame only"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem preview_channels_items[] = {
      {SEQ_USE_ALPHA,
       "COLOR_ALPHA",
       ICON_IMAGE_RGB_ALPHA,
       "Color and Alpha",
       "Display image with RGB colors and alpha transparency"},
      {0, "COLOR", ICON_IMAGE_RGB, "Color", "Display image with RGB colors"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem waveform_type_display_items[] = {
      {SEQ_NO_WAVEFORMS,
       "NO_WAVEFORMS",
       0,
       "Waveforms Off",
       "Don't display waveforms for any sound strips"},
      {SEQ_ALL_WAVEFORMS,
       "ALL_WAVEFORMS",
       0,
       "Waveforms On",
       "Display waveforms for all sound strips"},
      {0,
       "DEFAULT_WAVEFORMS",
       0,
       "Use Strip Option",
       "Display waveforms depending on strip setting"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "SpaceSequenceEditor", "Space");
  RNA_def_struct_sdna(srna, "SpaceSeq");
  RNA_def_struct_ui_text(srna, "Space Sequence Editor", "Sequence editor space data");

  rna_def_space_generic_show_region_toggles(srna,
                                            (1 << RGN_TYPE_TOOL_HEADER) | (1 << RGN_TYPE_UI) |
                                                (1 << RGN_TYPE_TOOLS) | (1 << RGN_TYPE_HUD));

  /* view type, fairly important */
  prop = RNA_def_property(srna, "view_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "view");
  RNA_def_property_enum_items(prop, rna_enum_space_sequencer_view_type_items);
  RNA_def_property_ui_text(
      prop, "View Type", "Type of the Sequencer view (sequencer, preview or both)");
  RNA_def_property_update(prop, 0, "rna_Sequencer_view_type_update");

  /* display type, fairly important */
  prop = RNA_def_property(srna, "display_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "mainb");
  RNA_def_property_enum_items(prop, display_mode_items);
  RNA_def_property_ui_text(
      prop, "Display Mode", "View mode to use for displaying sequencer output");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  /* flags */
  prop = RNA_def_property(srna, "show_frames", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_DRAWFRAMES);
  RNA_def_property_ui_text(prop, "Display Frames", "Display frames rather than seconds");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "use_marker_sync", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_MARKER_TRANS);
  RNA_def_property_ui_text(prop, "Sync Markers", "Transform markers as well as strips");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "show_separate_color", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_DRAW_COLOR_SEPARATED);
  RNA_def_property_ui_text(prop, "Separate Colors", "Separate color channels in preview");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "show_safe_areas", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_SHOW_SAFE_MARGINS);
  RNA_def_property_ui_text(
      prop, "Safe Areas", "Show TV title safe and action safe areas in preview");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "show_safe_center", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_SHOW_SAFE_CENTER);
  RNA_def_property_ui_text(
      prop, "Center-Cut Safe Areas", "Show safe areas to fit content in a different aspect ratio");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "show_metadata", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_SHOW_METADATA);
  RNA_def_property_ui_text(prop, "Show Metadata", "Show metadata of first visible strip");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "show_seconds", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SEQ_DRAWFRAMES);
  RNA_def_property_ui_text(prop, "Show Seconds", "Show timing in seconds not frames");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "show_markers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_SHOW_MARKERS);
  RNA_def_property_ui_text(
      prop,
      "Show Markers",
      "If any exists, show markers in a separate row at the bottom of the editor");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "show_annotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_SHOW_GPENCIL);
  RNA_def_property_ui_text(prop, "Show Annotation", "Show annotations for this view");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "display_channel", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "chanshown");
  RNA_def_property_ui_text(
      prop,
      "Display Channel",
      "The channel number shown in the image preview. 0 is the result of all strips combined");
  RNA_def_property_range(prop, -5, MAXSEQ);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, "rna_SequenceEditor_update_cache");

  prop = RNA_def_property(srna, "preview_channels", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
  RNA_def_property_enum_items(prop, preview_channels_items);
  RNA_def_property_ui_text(prop, "Display Channels", "Channels of the preview to display");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, "rna_SequenceEditor_update_cache");

  prop = RNA_def_property(srna, "waveform_display_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
  RNA_def_property_enum_items(prop, waveform_type_display_items);
  RNA_def_property_ui_text(prop, "Waveform Display", "How Waveforms are displayed");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "use_zoom_to_fit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_ZOOM_TO_FIT);
  RNA_def_property_ui_text(
      prop, "Zoom to Fit", "Automatically zoom preview image to make it fully fit the region");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "show_overexposed", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "zebra");
  RNA_def_property_ui_text(prop, "Show Overexposed", "Show overexposed areas with zebra stripes");
  RNA_def_property_range(prop, 0, 110);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "proxy_render_size", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "render_size");
  RNA_def_property_enum_items(prop, proxy_render_size_items);
  RNA_def_property_ui_text(prop,
                           "Proxy Render Size",
                           "Display preview using full resolution or different proxy resolutions");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_SEQUENCER, "rna_SequenceEditor_render_size_update");

  prop = RNA_def_property(srna, "use_proxies", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_USE_PROXIES);
  RNA_def_property_ui_text(
      prop, "Use Proxies", "Use optimized files for faster scrubbing when available");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  /* grease pencil */
  prop = RNA_def_property(srna, "grease_pencil", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "gpd");
  RNA_def_property_struct_type(prop, "GreasePencil");
  RNA_def_property_pointer_funcs(
      prop, NULL, NULL, NULL, "rna_GPencil_datablocks_annotations_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_ui_text(prop, "Grease Pencil", "Grease Pencil data for this Preview region");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "overlay_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "overlay_type");
  RNA_def_property_enum_items(prop, overlay_type_items);
  RNA_def_property_ui_text(prop, "Overlay Type", "Overlay display method");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "show_backdrop", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "draw_flag", SEQ_DRAW_BACKDROP);
  RNA_def_property_ui_text(prop, "Use Backdrop", "Display result under strips");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "show_strip_offset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "draw_flag", SEQ_DRAW_OFFSET_EXT);
  RNA_def_property_ui_text(prop, "Show Offsets", "Display strip in/out offsets");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "show_fcurves", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_SHOW_FCURVES);
  RNA_def_property_ui_text(prop, "Show F-Curves", "Display strip opacity/volume curve");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "show_strip_overlay", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_SHOW_STRIP_OVERLAY);
  RNA_def_property_ui_text(prop, "Show Overlay", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "show_strip_name", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_SHOW_STRIP_NAME);
  RNA_def_property_ui_text(prop, "Show Name", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "show_strip_source", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_SHOW_STRIP_SOURCE);
  RNA_def_property_ui_text(
      prop, "Show Source", "Display path to source file, or name of source datablock");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);

  prop = RNA_def_property(srna, "show_strip_duration", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SEQ_SHOW_STRIP_DURATION);
  RNA_def_property_ui_text(prop, "Show Duration", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, NULL);
}

static void rna_def_space_text(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;
  FunctionRNA *func;

  srna = RNA_def_struct(brna, "SpaceTextEditor", "Space");
  RNA_def_struct_sdna(srna, "SpaceText");
  RNA_def_struct_ui_text(srna, "Space Text Editor", "Text editor space data");

  rna_def_space_generic_show_region_toggles(srna, (1 << RGN_TYPE_UI) | (1 << RGN_TYPE_FOOTER));

  /* text */
  prop = RNA_def_property(srna, "text", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Text", "Text displayed and edited in this space");
  RNA_def_property_pointer_funcs(prop, NULL, "rna_SpaceTextEditor_text_set", NULL, NULL);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, NULL);

  /* display */
  prop = RNA_def_property(srna, "show_word_wrap", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "wordwrap", 0);
  RNA_def_property_boolean_funcs(prop, NULL, "rna_SpaceTextEditor_word_wrap_set");
  RNA_def_property_ui_text(
      prop, "Word Wrap", "Wrap words if there is not enough horizontal space");
  RNA_def_property_ui_icon(prop, ICON_WORDWRAP_ON, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, NULL);

  prop = RNA_def_property(srna, "show_line_numbers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "showlinenrs", 0);
  RNA_def_property_ui_text(prop, "Line Numbers", "Show line numbers next to the text");
  RNA_def_property_ui_icon(prop, ICON_LINENUMBERS_ON, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, NULL);

  func = RNA_def_function(srna,
                          "is_syntax_highlight_supported",
                          "rna_SpaceTextEditor_text_is_syntax_highlight_supported");
  RNA_def_function_return(func,
                          RNA_def_boolean(func, "is_syntax_highlight_supported", false, "", ""));
  RNA_def_function_ui_description(func,
                                  "Returns True if the editor supports syntax highlighting "
                                  "for the current text datablock");

  prop = RNA_def_property(srna, "show_syntax_highlight", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "showsyntax", 0);
  RNA_def_property_ui_text(prop, "Syntax Highlight", "Syntax highlight for scripting");
  RNA_def_property_ui_icon(prop, ICON_SYNTAX_ON, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, NULL);

  prop = RNA_def_property(srna, "show_line_highlight", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "line_hlight", 0);
  RNA_def_property_ui_text(prop, "Highlight Line", "Highlight the current line");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, NULL);

  prop = RNA_def_property(srna, "tab_width", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "tabnumber");
  RNA_def_property_range(prop, 2, 8);
  RNA_def_property_ui_text(prop, "Tab Width", "Number of spaces to display tabs with");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, "rna_SpaceTextEditor_updateEdited");

  prop = RNA_def_property(srna, "font_size", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "lheight");
  RNA_def_property_range(prop, 8, 32);
  RNA_def_property_ui_text(prop, "Font Size", "Font size to use for displaying the text");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, NULL);

  prop = RNA_def_property(srna, "show_margin", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", ST_SHOW_MARGIN);
  RNA_def_property_ui_text(prop, "Show Margin", "Show right margin");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, NULL);

  prop = RNA_def_property(srna, "margin_column", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "margin_column");
  RNA_def_property_range(prop, 0, 1024);
  RNA_def_property_ui_text(prop, "Margin Column", "Column number to show right margin at");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, NULL);

  prop = RNA_def_property(srna, "top", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "top");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_ui_text(prop, "Top Line", "Top line visible");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, NULL);

  prop = RNA_def_property(srna, "visible_lines", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_sdna(prop, NULL, "runtime.viewlines");
  RNA_def_property_ui_text(
      prop, "Visible Lines", "Amount of lines that can be visible in current editor");

  /* functionality options */
  prop = RNA_def_property(srna, "use_overwrite", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "overwrite", 1);
  RNA_def_property_ui_text(
      prop, "Overwrite", "Overwrite characters when typing rather than inserting them");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, NULL);

  prop = RNA_def_property(srna, "use_live_edit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "live_edit", 1);
  RNA_def_property_ui_text(prop, "Live Edit", "Run python while editing");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, NULL);

  /* find */
  prop = RNA_def_property(srna, "use_find_all", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", ST_FIND_ALL);
  RNA_def_property_ui_text(
      prop, "Find All", "Search in all text data-blocks, instead of only the active one");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, NULL);

  prop = RNA_def_property(srna, "use_find_wrap", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", ST_FIND_WRAP);
  RNA_def_property_ui_text(
      prop, "Find Wrap", "Search again from the start of the file when reaching the end");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, NULL);

  prop = RNA_def_property(srna, "use_match_case", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flags", ST_MATCH_CASE);
  RNA_def_property_ui_text(
      prop, "Match Case", "Search string is sensitive to uppercase and lowercase letters");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, NULL);

  prop = RNA_def_property(srna, "find_text", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "findstr");
  RNA_def_property_ui_text(prop, "Find Text", "Text to search for with the find tool");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, NULL);

  prop = RNA_def_property(srna, "replace_text", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "replacestr");
  RNA_def_property_ui_text(
      prop, "Replace Text", "Text to replace selected text with using the replace tool");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, NULL);

  RNA_api_space_text(srna);
}

static void rna_def_space_dopesheet(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SpaceDopeSheetEditor", "Space");
  RNA_def_struct_sdna(srna, "SpaceAction");
  RNA_def_struct_ui_text(srna, "Space Dope Sheet Editor", "Dope Sheet space data");

  rna_def_space_generic_show_region_toggles(srna, (1 << RGN_TYPE_UI));

  /* data */
  prop = RNA_def_property(srna, "action", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(
      prop, NULL, "rna_SpaceDopeSheetEditor_action_set", NULL, "rna_Action_actedit_assign_poll");
  RNA_def_property_ui_text(prop, "Action", "Action displayed and edited in this space");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(
      prop, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, "rna_SpaceDopeSheetEditor_action_update");

  /* mode (hidden in the UI, see 'ui_mode') */
  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "mode");
  RNA_def_property_enum_items(prop, rna_enum_space_action_mode_all_items);
  RNA_def_property_ui_text(prop, "Mode", "Editing context being displayed");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_DOPESHEET, "rna_SpaceDopeSheetEditor_mode_update");

  prop = RNA_def_property(srna, "ui_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "mode");
  RNA_def_property_enum_items(prop, rna_enum_space_action_ui_mode_items);
  RNA_def_property_ui_text(prop, "Mode", "Editing context being displayed");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_DOPESHEET, "rna_SpaceDopeSheetEditor_mode_update");

  /* display */
  prop = RNA_def_property(srna, "show_seconds", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SACTION_DRAWTIME);
  RNA_def_property_ui_text(prop, "Show Seconds", "Show timing in seconds not frames");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_DOPESHEET, NULL);

  prop = RNA_def_property(srna, "show_sliders", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SACTION_SLIDERS);
  RNA_def_property_ui_text(prop, "Show Sliders", "Show sliders beside F-Curve channels");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_DOPESHEET, NULL);

  prop = RNA_def_property(srna, "show_pose_markers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SACTION_POSEMARKERS_SHOW);
  RNA_def_property_ui_text(prop,
                           "Show Pose Markers",
                           "Show markers belonging to the active action instead of Scene markers "
                           "(Action and Shape Key Editors only)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_DOPESHEET, NULL);

  prop = RNA_def_property(srna, "show_interpolation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SACTION_SHOW_INTERPOLATION);
  RNA_def_property_ui_text(prop,
                           "Show Handles and Interpolation",
                           "Display keyframe handle types and non-bezier interpolation modes");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_DOPESHEET, NULL);

  prop = RNA_def_property(srna, "show_extremes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SACTION_SHOW_EXTREMES);
  RNA_def_property_ui_text(prop,
                           "Show Curve Extremes",
                           "Mark keyframes where the key value flow changes direction, based on "
                           "comparison with adjacent keys");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_DOPESHEET, NULL);

  prop = RNA_def_property(srna, "show_markers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SACTION_SHOW_MARKERS);
  RNA_def_property_ui_text(
      prop,
      "Show Markers",
      "If any exists, show markers in a separate row at the bottom of the editor");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_DOPESHEET, NULL);

  /* editing */
  prop = RNA_def_property(srna, "use_auto_merge_keyframes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SACTION_NOTRANSKEYCULL);
  RNA_def_property_ui_text(prop, "Auto-Merge Keyframes", "Automatically merge nearby keyframes");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_DOPESHEET, NULL);

  prop = RNA_def_property(srna, "use_realtime_update", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SACTION_NOREALTIMEUPDATES);
  RNA_def_property_ui_text(
      prop,
      "Realtime Updates",
      "When transforming keyframes, changes to the animation data are flushed to other views");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_DOPESHEET, NULL);

  prop = RNA_def_property(srna, "use_marker_sync", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SACTION_MARKERS_MOVE);
  RNA_def_property_ui_text(prop, "Sync Markers", "Sync Markers with keyframe edits");

  /* dopesheet */
  prop = RNA_def_property(srna, "dopesheet", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "DopeSheet");
  RNA_def_property_pointer_sdna(prop, NULL, "ads");
  RNA_def_property_ui_text(prop, "Dope Sheet", "Settings for filtering animation data");

  /* autosnap */
  prop = RNA_def_property(srna, "auto_snap", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "autosnap");
  RNA_def_property_enum_items(prop, autosnap_items);
  RNA_def_property_ui_text(
      prop, "Auto Snap", "Automatic time snapping settings for transformations");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_DOPESHEET, NULL);

  /* displaying cache status */
  prop = RNA_def_property(srna, "show_cache", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "cache_display", TIME_CACHE_DISPLAY);
  RNA_def_property_ui_text(prop, "Show Cache", "Show the status of cached frames in the timeline");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, NULL);

  prop = RNA_def_property(srna, "cache_softbody", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "cache_display", TIME_CACHE_SOFTBODY);
  RNA_def_property_ui_text(prop, "Softbody", "Show the active object's softbody point cache");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, NULL);

  prop = RNA_def_property(srna, "cache_particles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "cache_display", TIME_CACHE_PARTICLES);
  RNA_def_property_ui_text(prop, "Particles", "Show the active object's particle point cache");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, NULL);

  prop = RNA_def_property(srna, "cache_cloth", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "cache_display", TIME_CACHE_CLOTH);
  RNA_def_property_ui_text(prop, "Cloth", "Show the active object's cloth point cache");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, NULL);

  prop = RNA_def_property(srna, "cache_smoke", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "cache_display", TIME_CACHE_SMOKE);
  RNA_def_property_ui_text(prop, "Smoke", "Show the active object's smoke cache");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, NULL);

  prop = RNA_def_property(srna, "cache_dynamicpaint", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "cache_display", TIME_CACHE_DYNAMICPAINT);
  RNA_def_property_ui_text(prop, "Dynamic Paint", "Show the active object's Dynamic Paint cache");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, NULL);

  prop = RNA_def_property(srna, "cache_rigidbody", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "cache_display", TIME_CACHE_RIGIDBODY);
  RNA_def_property_ui_text(prop, "Rigid Body", "Show the active object's Rigid Body cache");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, NULL);
}

static void rna_def_space_graph(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* this is basically the same as the one for the 3D-View, but with some entries omitted */
  static const EnumPropertyItem gpivot_items[] = {
      {V3D_AROUND_CENTER_BOUNDS,
       "BOUNDING_BOX_CENTER",
       ICON_PIVOT_BOUNDBOX,
       "Bounding Box Center",
       ""},
      {V3D_AROUND_CURSOR, "CURSOR", ICON_PIVOT_CURSOR, "2D Cursor", ""},
      {V3D_AROUND_LOCAL_ORIGINS,
       "INDIVIDUAL_ORIGINS",
       ICON_PIVOT_INDIVIDUAL,
       "Individual Centers",
       ""},
      /*{V3D_AROUND_CENTER_MEDIAN, "MEDIAN_POINT", 0, "Median Point", ""}, */
      /*{V3D_AROUND_ACTIVE, "ACTIVE_ELEMENT", 0, "Active Element", ""}, */
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "SpaceGraphEditor", "Space");
  RNA_def_struct_sdna(srna, "SpaceGraph");
  RNA_def_struct_ui_text(srna, "Space Graph Editor", "Graph Editor space data");

  rna_def_space_generic_show_region_toggles(srna, (1 << RGN_TYPE_UI) | (1 << RGN_TYPE_HUD));

  /* mode */
  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "mode");
  RNA_def_property_enum_items(prop, rna_enum_space_graph_mode_items);
  RNA_def_property_ui_text(prop, "Mode", "Editing context being displayed");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_GRAPH, "rna_SpaceGraphEditor_display_mode_update");

  /* display */
  prop = RNA_def_property(srna, "show_seconds", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SIPO_DRAWTIME);
  RNA_def_property_ui_text(prop, "Show Seconds", "Show timing in seconds not frames");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, NULL);

  prop = RNA_def_property(srna, "show_sliders", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SIPO_SLIDERS);
  RNA_def_property_ui_text(prop, "Show Sliders", "Show sliders beside F-Curve channels");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, NULL);

  prop = RNA_def_property(srna, "show_handles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SIPO_NOHANDLES);
  RNA_def_property_ui_text(prop, "Show Handles", "Show handles of Bezier control points");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, NULL);

  prop = RNA_def_property(srna, "use_only_selected_curves_handles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SIPO_SELCUVERTSONLY);
  RNA_def_property_ui_text(prop,
                           "Only Selected Curve Keyframes",
                           "Only keyframes of selected F-Curves are visible and editable");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, NULL);

  prop = RNA_def_property(srna, "use_only_selected_keyframe_handles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SIPO_SELVHANDLESONLY);
  RNA_def_property_ui_text(
      prop, "Only Selected Keyframes Handles", "Only show and edit handles of selected keyframes");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, NULL);

  prop = RNA_def_property(srna, "use_beauty_drawing", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SIPO_BEAUTYDRAW_OFF);
  RNA_def_property_ui_text(prop,
                           "Use High Quality Display",
                           "Display F-Curves using Anti-Aliasing and other fancy effects "
                           "(disable for better performance)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, NULL);

  prop = RNA_def_property(srna, "show_markers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SIPO_SHOW_MARKERS);
  RNA_def_property_ui_text(
      prop,
      "Show Markers",
      "If any exists, show markers in a separate row at the bottom of the editor");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, NULL);

  prop = RNA_def_property(srna, "show_extrapolation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SIPO_NO_DRAW_EXTRAPOLATION);
  RNA_def_property_ui_text(prop, "Show Extrapolation", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, NULL);

  /* editing */
  prop = RNA_def_property(srna, "use_auto_merge_keyframes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SIPO_NOTRANSKEYCULL);
  RNA_def_property_ui_text(prop, "AutoMerge Keyframes", "Automatically merge nearby keyframes");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, NULL);

  prop = RNA_def_property(srna, "use_realtime_update", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SIPO_NOREALTIMEUPDATES);
  RNA_def_property_ui_text(
      prop,
      "Realtime Updates",
      "When transforming keyframes, changes to the animation data are flushed to other views");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, NULL);

  /* cursor */
  prop = RNA_def_property(srna, "show_cursor", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SIPO_NODRAWCURSOR);
  RNA_def_property_ui_text(prop, "Show Cursor", "Show 2D cursor");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, NULL);

  prop = RNA_def_property(srna, "cursor_position_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "cursorTime");
  RNA_def_property_ui_text(
      prop, "Cursor X-Value", "Graph Editor 2D-Value cursor - X-Value component");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, NULL);

  prop = RNA_def_property(srna, "cursor_position_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "cursorVal");
  RNA_def_property_ui_text(
      prop, "Cursor Y-Value", "Graph Editor 2D-Value cursor - Y-Value component");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, NULL);

  prop = RNA_def_property(srna, "pivot_point", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "around");
  RNA_def_property_enum_items(prop, gpivot_items);
  RNA_def_property_ui_text(prop, "Pivot Point", "Pivot center for rotation/scaling");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, NULL);

  /* Dope-sheet. */
  prop = RNA_def_property(srna, "dopesheet", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "DopeSheet");
  RNA_def_property_pointer_sdna(prop, NULL, "ads");
  RNA_def_property_ui_text(prop, "Dope Sheet", "Settings for filtering animation data");

  /* Auto-snap. */
  prop = RNA_def_property(srna, "auto_snap", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "autosnap");
  RNA_def_property_enum_items(prop, autosnap_items);
  RNA_def_property_ui_text(
      prop, "Auto Snap", "Automatic time snapping settings for transformations");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, NULL);

  /* Read-only state info. */
  prop = RNA_def_property(srna, "has_ghost_curves", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_SpaceGraphEditor_has_ghost_curves_get", NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Has Ghost Curves", "Graph Editor instance has some ghost curves stored");

  /* Normalize curves. */
  prop = RNA_def_property(srna, "use_normalization", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SIPO_NORMALIZE);
  RNA_def_property_ui_text(prop,
                           "Use Normalization",
                           "Display curves in normalized range from -1 to 1, "
                           "for easier editing of multiple curves with different ranges");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, NULL);

  prop = RNA_def_property(srna, "use_auto_normalization", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SIPO_NORMALIZE_FREEZE);
  RNA_def_property_ui_text(prop,
                           "Auto Normalization",
                           "Automatically recalculate curve normalization on every curve edit");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, NULL);
}

static void rna_def_space_nla(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SpaceNLA", "Space");
  RNA_def_struct_sdna(srna, "SpaceNla");
  RNA_def_struct_ui_text(srna, "Space Nla Editor", "NLA editor space data");

  rna_def_space_generic_show_region_toggles(srna, (1 << RGN_TYPE_UI));

  /* display */
  prop = RNA_def_property(srna, "show_seconds", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SNLA_DRAWTIME);
  RNA_def_property_ui_text(prop, "Show Seconds", "Show timing in seconds not frames");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NLA, NULL);

  prop = RNA_def_property(srna, "show_strip_curves", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SNLA_NOSTRIPCURVES);
  RNA_def_property_ui_text(prop, "Show Control F-Curves", "Show influence F-Curves on strips");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NLA, NULL);

  prop = RNA_def_property(srna, "show_local_markers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SNLA_NOLOCALMARKERS);
  RNA_def_property_ui_text(
      prop,
      "Show Local Markers",
      "Show action-local markers on the strips, useful when synchronizing timing across strips");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NLA, NULL);

  prop = RNA_def_property(srna, "show_markers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SNLA_SHOW_MARKERS);
  RNA_def_property_ui_text(
      prop,
      "Show Markers",
      "If any exists, show markers in a separate row at the bottom of the editor");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NLA, NULL);

  /* editing */
  prop = RNA_def_property(srna, "use_realtime_update", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SNLA_NOREALTIMEUPDATES);
  RNA_def_property_ui_text(
      prop,
      "Realtime Updates",
      "When transforming strips, changes to the animation data are flushed to other views");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NLA, NULL);

  /* dopesheet */
  prop = RNA_def_property(srna, "dopesheet", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "DopeSheet");
  RNA_def_property_pointer_sdna(prop, NULL, "ads");
  RNA_def_property_ui_text(prop, "Dope Sheet", "Settings for filtering animation data");

  /* autosnap */
  prop = RNA_def_property(srna, "auto_snap", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "autosnap");
  RNA_def_property_enum_items(prop, autosnap_items);
  RNA_def_property_ui_text(
      prop, "Auto Snap", "Automatic time snapping settings for transformations");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NLA, NULL);
}

static void rna_def_console_line(BlenderRNA *brna)
{
  static const EnumPropertyItem console_line_type_items[] = {
      {CONSOLE_LINE_OUTPUT, "OUTPUT", 0, "Output", ""},
      {CONSOLE_LINE_INPUT, "INPUT", 0, "Input", ""},
      {CONSOLE_LINE_INFO, "INFO", 0, "Info", ""},
      {CONSOLE_LINE_ERROR, "ERROR", 0, "Error", ""},
      {0, NULL, 0, NULL, NULL},
  };

  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ConsoleLine", NULL);
  RNA_def_struct_ui_text(srna, "Console Input", "Input line for the interactive console");

  prop = RNA_def_property(srna, "body", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, "rna_ConsoleLine_body_get", "rna_ConsoleLine_body_length", "rna_ConsoleLine_body_set");
  RNA_def_property_ui_text(prop, "Line", "Text in the line");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CONSOLE, NULL);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_TEXT);

  prop = RNA_def_property(
      srna, "current_character", PROP_INT, PROP_NONE); /* copied from text editor */
  RNA_def_property_int_sdna(prop, NULL, "cursor");
  RNA_def_property_int_funcs(prop, NULL, NULL, "rna_ConsoleLine_cursor_index_range");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CONSOLE, NULL);

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "type");
  RNA_def_property_enum_items(prop, console_line_type_items);
  RNA_def_property_ui_text(prop, "Type", "Console line type when used in scrollback");
}

static void rna_def_space_console(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SpaceConsole", "Space");
  RNA_def_struct_sdna(srna, "SpaceConsole");
  RNA_def_struct_ui_text(srna, "Space Console", "Interactive python console");

  /* display */
  prop = RNA_def_property(srna, "font_size", PROP_INT, PROP_NONE); /* copied from text editor */
  RNA_def_property_int_sdna(prop, NULL, "lheight");
  RNA_def_property_range(prop, 8, 32);
  RNA_def_property_ui_text(prop, "Font Size", "Font size to use for displaying the text");
  RNA_def_property_update(prop, 0, "rna_SpaceConsole_rect_update");

  prop = RNA_def_property(
      srna, "select_start", PROP_INT, PROP_UNSIGNED); /* copied from text editor */
  RNA_def_property_int_sdna(prop, NULL, "sel_start");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CONSOLE, NULL);

  prop = RNA_def_property(
      srna, "select_end", PROP_INT, PROP_UNSIGNED); /* copied from text editor */
  RNA_def_property_int_sdna(prop, NULL, "sel_end");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CONSOLE, NULL);

  prop = RNA_def_property(srna, "prompt", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Prompt", "Command line prompt");

  prop = RNA_def_property(srna, "language", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Language", "Command line prompt language");

  prop = RNA_def_property(srna, "history", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "history", NULL);
  RNA_def_property_struct_type(prop, "ConsoleLine");
  RNA_def_property_ui_text(prop, "History", "Command history");

  prop = RNA_def_property(srna, "scrollback", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "scrollback", NULL);
  RNA_def_property_struct_type(prop, "ConsoleLine");
  RNA_def_property_ui_text(prop, "Output", "Command output");
}

/* Filter for datablock types in link/append. */
static void rna_def_fileselect_idfilter(BlenderRNA *brna)
{
  struct IDFilterBoolean {
    /* 64 bit, so we can't use bitflag enum. */
    const uint64_t flag;
    const char *identifier;
    const int icon;
    const char *name;
    const char *description;
  };

  static const struct IDFilterBoolean booleans[] = {
      /* Datablocks */
      {FILTER_ID_AC, "filter_action", ICON_ANIM_DATA, "Actions", "Show Action data-blocks"},
      {FILTER_ID_AR,
       "filter_armature",
       ICON_ARMATURE_DATA,
       "Armatures",
       "Show Armature data-blocks"},
      {FILTER_ID_BR, "filter_brush", ICON_BRUSH_DATA, "Brushes", "Show Brushes data-blocks"},
      {FILTER_ID_CA, "filter_camera", ICON_CAMERA_DATA, "Cameras", "Show Camera data-blocks"},
      {FILTER_ID_CF, "filter_cachefile", ICON_FILE, "Cache Files", "Show Cache File data-blocks"},
      {FILTER_ID_CU, "filter_curve", ICON_CURVE_DATA, "Curves", "Show Curve data-blocks"},
      {FILTER_ID_GD,
       "filter_grease_pencil",
       ICON_GREASEPENCIL,
       "Grease Pencil",
       "Show Grease pencil data-blocks"},
      {FILTER_ID_GR,
       "filter_group",
       ICON_OUTLINER_COLLECTION,
       "Collections",
       "Show Collection data-blocks"},
      {FILTER_ID_HA, "filter_hair", ICON_HAIR_DATA, "Hairs", "Show/hide Hair data-blocks"},
      {FILTER_ID_IM, "filter_image", ICON_IMAGE_DATA, "Images", "Show Image data-blocks"},
      {FILTER_ID_LA, "filter_light", ICON_LIGHT_DATA, "Lights", "Show Light data-blocks"},
      {FILTER_ID_LP,
       "filter_light_probe",
       ICON_OUTLINER_DATA_LIGHTPROBE,
       "Light Probes",
       "Show Light Probe data-blocks"},
      {FILTER_ID_LS,
       "filter_linestyle",
       ICON_LINE_DATA,
       "Freestyle Linestyles",
       "Show Freestyle's Line Style data-blocks"},
      {FILTER_ID_LT, "filter_lattice", ICON_LATTICE_DATA, "Lattices", "Show Lattice data-blocks"},
      {FILTER_ID_MA,
       "filter_material",
       ICON_MATERIAL_DATA,
       "Materials",
       "Show Material data-blocks"},
      {FILTER_ID_MB, "filter_metaball", ICON_META_DATA, "Metaballs", "Show Metaball data-blocks"},
      {FILTER_ID_MC,
       "filter_movie_clip",
       ICON_TRACKER_DATA,
       "Movie Clips",
       "Show Movie Clip data-blocks"},
      {FILTER_ID_ME, "filter_mesh", ICON_MESH_DATA, "Meshes", "Show Mesh data-blocks"},
      {FILTER_ID_MSK, "filter_mask", ICON_MOD_MASK, "Masks", "Show Mask data-blocks"},
      {FILTER_ID_NT,
       "filter_node_tree",
       ICON_NODETREE,
       "Node Trees",
       "Show Node Tree data-blocks"},
      {FILTER_ID_OB, "filter_object", ICON_OBJECT_DATA, "Objects", "Show Object data-blocks"},
      {FILTER_ID_PA,
       "filter_particle_settings",
       ICON_PARTICLE_DATA,
       "Particles Settings",
       "Show Particle Settings data-blocks"},
      {FILTER_ID_PAL, "filter_palette", ICON_COLOR, "Palettes", "Show Palette data-blocks"},
      {FILTER_ID_PC,
       "filter_paint_curve",
       ICON_CURVE_BEZCURVE,
       "Paint Curves",
       "Show Paint Curve data-blocks"},
      {FILTER_ID_PT,
       "filter_pointcloud",
       ICON_POINTCLOUD_DATA,
       "Point Clouds",
       "Show/hide Point Cloud data-blocks"},
      {FILTER_ID_SCE, "filter_scene", ICON_SCENE_DATA, "Scenes", "Show Scene data-blocks"},
      {FILTER_ID_SIM,
       "filter_simulation",
       ICON_PHYSICS,
       "Simulations",
       "Show Simulation data-blocks"}, /* TODO: Use correct icon. */
      {FILTER_ID_SPK, "filter_speaker", ICON_SPEAKER, "Speakers", "Show Speaker data-blocks"},
      {FILTER_ID_SO, "filter_sound", ICON_SOUND, "Sounds", "Show Sound data-blocks"},
      {FILTER_ID_TE, "filter_texture", ICON_TEXTURE_DATA, "Textures", "Show Texture data-blocks"},
      {FILTER_ID_TXT, "filter_text", ICON_TEXT, "Texts", "Show Text data-blocks"},
      {FILTER_ID_VF, "filter_font", ICON_FONT_DATA, "Fonts", "Show Font data-blocks"},
      {FILTER_ID_VO, "filter_volume", ICON_VOLUME_DATA, "Volumes", "Show/hide Volume data-blocks"},
      {FILTER_ID_WO, "filter_world", ICON_WORLD_DATA, "Worlds", "Show World data-blocks"},
      {FILTER_ID_WS,
       "filter_work_space",
       ICON_WORKSPACE,
       "Workspaces",
       "Show workspace data-blocks"},

      /* Categories */
      {FILTER_ID_SCE, "category_scene", ICON_SCENE_DATA, "Scenes", "Show scenes"},
      {FILTER_ID_AC, "category_animation", ICON_ANIM_DATA, "Animations", "Show animation data"},
      {FILTER_ID_OB | FILTER_ID_GR,
       "category_object",
       ICON_OUTLINER_COLLECTION,
       "Objects & Collections",
       "Show objects and collections"},
      {FILTER_ID_AR | FILTER_ID_CU | FILTER_ID_LT | FILTER_ID_MB | FILTER_ID_ME | FILTER_ID_HA |
           FILTER_ID_PT | FILTER_ID_VO,
       "category_geometry",
       ICON_NODETREE,
       "Geometry",
       "Show meshes, curves, lattice, armatures and metaballs data"},
      {FILTER_ID_LS | FILTER_ID_MA | FILTER_ID_NT | FILTER_ID_TE,
       "category_shading",
       ICON_MATERIAL_DATA,
       "Shading",
       "Show materials, nodetrees, textures and Freestyle's linestyles"},
      {FILTER_ID_IM | FILTER_ID_MC | FILTER_ID_MSK | FILTER_ID_SO,
       "category_image",
       ICON_IMAGE_DATA,
       "Images & Sounds",
       "Show images, movie clips, sounds and masks"},
      {FILTER_ID_CA | FILTER_ID_LA | FILTER_ID_LP | FILTER_ID_SPK | FILTER_ID_WO,
       "category_environment",
       ICON_WORLD_DATA,
       "Environment",
       "Show worlds, lights, cameras and speakers"},
      {FILTER_ID_BR | FILTER_ID_GD | FILTER_ID_PA | FILTER_ID_PAL | FILTER_ID_PC | FILTER_ID_TXT |
           FILTER_ID_VF | FILTER_ID_CF | FILTER_ID_WS,
       "category_misc",
       ICON_GREASEPENCIL,
       "Miscellaneous",
       "Show other data types"},

      {0, NULL, 0, NULL, NULL}};

  StructRNA *srna = RNA_def_struct(brna, "FileSelectIDFilter", NULL);
  RNA_def_struct_sdna(srna, "FileSelectParams");
  RNA_def_struct_nested(brna, srna, "FileSelectParams");
  RNA_def_struct_ui_text(
      srna, "File Select ID Filter", "Which ID types to show/hide, when browsing a library");

  for (int i = 0; booleans[i].identifier; i++) {
    PropertyRNA *prop = RNA_def_property(srna, booleans[i].identifier, PROP_BOOLEAN, PROP_NONE);
    RNA_def_property_boolean_sdna(prop, NULL, "filter_id", booleans[i].flag);
    RNA_def_property_ui_text(prop, booleans[i].name, booleans[i].description);
    RNA_def_property_ui_icon(prop, booleans[i].icon, 0);
    RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);
  }
}

static void rna_def_fileselect_entry(BlenderRNA *brna)
{
  PropertyRNA *prop;
  StructRNA *srna = RNA_def_struct(brna, "FileSelectEntry", NULL);
  RNA_def_struct_sdna(srna, "FileDirEntry");
  RNA_def_struct_ui_text(srna, "File Select Entry", "A file viewable in the File Browser");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_FileBrowser_FileSelectEntry_name_get",
                                "rna_FileBrowser_FileSelectEntry_name_length",
                                NULL);
  RNA_def_property_ui_text(prop, "Name", "");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_int(
      srna,
      "preview_icon_id",
      0,
      INT_MIN,
      INT_MAX,
      "Icon ID",
      "Unique integer identifying the preview of this file as an icon (zero means invalid)",
      INT_MIN,
      INT_MAX);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(
      prop, "rna_FileBrowser_FileSelectEntry_preview_icon_id_get", NULL, NULL);

  prop = RNA_def_property(srna, "asset_data", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "AssetMetaData");
  RNA_def_property_pointer_funcs(
      prop, "rna_FileBrowser_FileSelectEntry_asset_data_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(
      prop, "Asset Data", "Asset data, valid if the file represents an asset");
}

static void rna_def_fileselect_params(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem file_display_items[] = {
      {FILE_VERTICALDISPLAY,
       "LIST_VERTICAL",
       ICON_LONGDISPLAY,
       "Vertical List",
       "Display files as a vertical list"},
      {FILE_HORIZONTALDISPLAY,
       "LIST_HORIZONTAL",
       ICON_SHORTDISPLAY,
       "Horizontal List",
       "Display files as a horizontal list"},
      {FILE_IMGDISPLAY, "THUMBNAIL", ICON_IMGDISPLAY, "Thumbnails", "Display files as thumbnails"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem display_size_items[] = {
      {64, "TINY", 0, "Tiny", ""},
      {96, "SMALL", 0, "Small", ""},
      {128, "NORMAL", 0, "Regular", ""},
      {192, "LARGE", 0, "Large", ""},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "FileSelectParams", NULL);
  RNA_def_struct_path_func(srna, "rna_FileSelectParams_path");
  RNA_def_struct_ui_text(srna, "File Select Parameters", "File Select Parameters");

  prop = RNA_def_property(srna, "title", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "title");
  RNA_def_property_ui_text(prop, "Title", "Title for the file browser");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  /* Use BYTESTRING rather than DIRPATH as subtype so UI code doesn't add OT_directory_browse
   * button when displaying this prop in the file browser (it would just open a file browser). That
   * should be the only effective difference between the two. */
  prop = RNA_def_property(srna, "directory", PROP_STRING, PROP_BYTESTRING);
  RNA_def_property_string_sdna(prop, NULL, "dir");
  RNA_def_property_ui_text(prop, "Directory", "Directory displayed in the file browser");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

  prop = RNA_def_property(srna, "filename", PROP_STRING, PROP_FILENAME);
  RNA_def_property_string_sdna(prop, NULL, "file");
  RNA_def_property_ui_text(prop, "File Name", "Active file in the file browser");
  RNA_def_property_editable_func(prop, "rna_FileSelectParams_filename_editable");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

  prop = RNA_def_property(srna, "use_library_browsing", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Library Browser", "Whether we may browse blender files' content or not");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_FileSelectParams_use_lib_get", NULL);

  prop = RNA_def_property(srna, "display_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "display");
  RNA_def_property_enum_items(prop, file_display_items);
  RNA_def_property_ui_text(prop, "Display Mode", "Display mode for the file list");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

  prop = RNA_def_property(srna, "recursion_level", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, fileselectparams_recursion_level_items);
  RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_FileSelectParams_recursion_level_itemf");
  RNA_def_property_ui_text(prop, "Recursion", "Numbers of dirtree levels to show simultaneously");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

  prop = RNA_def_property(srna, "show_details_size", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "details_flags", FILE_DETAILS_SIZE);
  RNA_def_property_ui_text(prop, "File Size", "Show a column listing the size of each file");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

  prop = RNA_def_property(srna, "show_details_datetime", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "details_flags", FILE_DETAILS_DATETIME);
  RNA_def_property_ui_text(
      prop,
      "File Modification Date",
      "Show a column listing the date and time of modification for each file");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

  prop = RNA_def_property(srna, "use_filter", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", FILE_FILTER);
  RNA_def_property_ui_text(prop, "Filter Files", "Enable filtering of files");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

  prop = RNA_def_property(srna, "show_hidden", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", FILE_HIDE_DOT);
  RNA_def_property_ui_text(prop, "Show Hidden", "Show hidden dot files");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

  prop = RNA_def_property(srna, "sort_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "sort");
  RNA_def_property_enum_items(prop, rna_enum_fileselect_params_sort_items);
  RNA_def_property_ui_text(prop, "Sort", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

  prop = RNA_def_property(srna, "use_sort_invert", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", FILE_SORT_INVERT);
  RNA_def_property_ui_text(
      prop, "Reverse Sorting", "Sort items descending, from highest value to lowest");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

  prop = RNA_def_property(srna, "use_filter_image", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "filter", FILE_TYPE_IMAGE);
  RNA_def_property_ui_text(prop, "Filter Images", "Show image files");
  RNA_def_property_ui_icon(prop, ICON_FILE_IMAGE, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

  prop = RNA_def_property(srna, "use_filter_blender", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "filter", FILE_TYPE_BLENDER);
  RNA_def_property_ui_text(prop, "Filter Blender", "Show .blend files");
  RNA_def_property_ui_icon(prop, ICON_FILE_BLEND, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

  prop = RNA_def_property(srna, "use_filter_backup", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "filter", FILE_TYPE_BLENDER_BACKUP);
  RNA_def_property_ui_text(
      prop, "Filter Blender Backup Files", "Show .blend1, .blend2, etc. files");
  RNA_def_property_ui_icon(prop, ICON_FILE_BACKUP, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

  prop = RNA_def_property(srna, "use_filter_movie", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "filter", FILE_TYPE_MOVIE);
  RNA_def_property_ui_text(prop, "Filter Movies", "Show movie files");
  RNA_def_property_ui_icon(prop, ICON_FILE_MOVIE, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

  prop = RNA_def_property(srna, "use_filter_script", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "filter", FILE_TYPE_PYSCRIPT);
  RNA_def_property_ui_text(prop, "Filter Script", "Show script files");
  RNA_def_property_ui_icon(prop, ICON_FILE_SCRIPT, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

  prop = RNA_def_property(srna, "use_filter_font", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "filter", FILE_TYPE_FTFONT);
  RNA_def_property_ui_text(prop, "Filter Fonts", "Show font files");
  RNA_def_property_ui_icon(prop, ICON_FILE_FONT, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

  prop = RNA_def_property(srna, "use_filter_sound", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "filter", FILE_TYPE_SOUND);
  RNA_def_property_ui_text(prop, "Filter Sound", "Show sound files");
  RNA_def_property_ui_icon(prop, ICON_FILE_SOUND, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

  prop = RNA_def_property(srna, "use_filter_text", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "filter", FILE_TYPE_TEXT);
  RNA_def_property_ui_text(prop, "Filter Text", "Show text files");
  RNA_def_property_ui_icon(prop, ICON_FILE_TEXT, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

  prop = RNA_def_property(srna, "use_filter_volume", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "filter", FILE_TYPE_VOLUME);
  RNA_def_property_ui_text(prop, "Filter Volume", "Show 3D volume files");
  RNA_def_property_ui_icon(prop, ICON_FILE_VOLUME, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

  prop = RNA_def_property(srna, "use_filter_folder", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "filter", FILE_TYPE_FOLDER);
  RNA_def_property_ui_text(prop, "Filter Folder", "Show folders");
  RNA_def_property_ui_icon(prop, ICON_FILE_FOLDER, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

  prop = RNA_def_property(srna, "use_filter_blendid", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "filter", FILE_TYPE_BLENDERLIB);
  RNA_def_property_ui_text(
      prop, "Filter Blender IDs", "Show .blend files items (objects, materials, etc.)");
  RNA_def_property_ui_icon(prop, ICON_BLENDER, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

  prop = RNA_def_property(srna, "use_filter_asset_only", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", FILE_ASSETS_ONLY);
  RNA_def_property_ui_text(
      prop, "Only Assets", "Hide .blend files items that are not data-blocks with asset metadata");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

  prop = RNA_def_property(srna, "filter_id", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "FileSelectIDFilter");
  RNA_def_property_pointer_funcs(prop, "rna_FileSelectParams_filter_id_get", NULL, NULL, NULL);
  RNA_def_property_ui_text(
      prop, "Filter ID Types", "Which ID types to show/hide, when browsing a library");

  prop = RNA_def_property(srna, "filter_glob", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "filter_glob");
  RNA_def_property_ui_text(prop,
                           "Extension Filter",
                           "UNIX shell-like filename patterns matching, supports wildcards ('*') "
                           "and list of patterns separated by ';'");
  RNA_def_property_string_funcs(prop, NULL, NULL, "rna_FileSelectPrams_filter_glob_set");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_LIST, NULL);

  prop = RNA_def_property(srna, "filter_search", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "filter_search");
  RNA_def_property_ui_text(prop, "Name Filter", "Filter by name, supports '*' wildcard");
  RNA_def_property_flag(prop, PROP_TEXTEDIT_UPDATE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_LIST, NULL);

  prop = RNA_def_property(srna, "display_size", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "thumbnail_size");
  RNA_def_property_enum_items(prop, display_size_items);
  RNA_def_property_ui_text(prop,
                           "Display Size",
                           "Change the size of the display (width of columns or thumbnails size)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_LIST, NULL);
}

static void rna_def_fileselect_asset_params(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* XXX copied from rna_def_fileselect_idfilter. */
  static const EnumPropertyItem asset_category_items[] = {
      {FILTER_ID_SCE, "SCENES", ICON_SCENE_DATA, "Scenes", "Show scenes"},
      {FILTER_ID_AC, "ANIMATIONS", ICON_ANIM_DATA, "Animations", "Show animation data"},
      {FILTER_ID_OB | FILTER_ID_GR,
       "OBJECTS_AND_COLLECTIONS",
       ICON_GROUP,
       "Objects & Collections",
       "Show objects and collections"},
      {FILTER_ID_AR | FILTER_ID_CU | FILTER_ID_LT | FILTER_ID_MB | FILTER_ID_ME
       /* XXX avoid warning */
       // | FILTER_ID_HA | FILTER_ID_PT | FILTER_ID_VO
       ,
       "GEOMETRY",
       ICON_MESH_DATA,
       "Geometry",
       "Show meshes, curves, lattice, armatures and metaballs data"},
      {FILTER_ID_LS | FILTER_ID_MA | FILTER_ID_NT | FILTER_ID_TE,
       "SHADING",
       ICON_MATERIAL_DATA,
       "Shading",
       "Show materials, nodetrees, textures and Freestyle's linestyles"},
      {FILTER_ID_IM | FILTER_ID_MC | FILTER_ID_MSK | FILTER_ID_SO,
       "IMAGES_AND_SOUNDS",
       ICON_IMAGE_DATA,
       "Images & Sounds",
       "Show images, movie clips, sounds and masks"},
      {FILTER_ID_CA | FILTER_ID_LA | FILTER_ID_LP | FILTER_ID_SPK | FILTER_ID_WO,
       "ENVIRONMENTS",
       ICON_WORLD_DATA,
       "Environment",
       "Show worlds, lights, cameras and speakers"},
      {FILTER_ID_BR | FILTER_ID_GD | FILTER_ID_PA | FILTER_ID_PAL | FILTER_ID_PC | FILTER_ID_TXT |
           FILTER_ID_VF | FILTER_ID_CF | FILTER_ID_WS,
       "MISC",
       ICON_GREASEPENCIL,
       "Miscellaneous",
       "Show other data types"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "FileAssetSelectParams", "FileSelectParams");
  RNA_def_struct_ui_text(
      srna, "Asset Select Parameters", "Settings for the file selection in Asset Browser mode");

  prop = RNA_def_property(srna, "asset_library", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, DummyRNA_NULL_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_FileAssetSelectParams_asset_library_get",
                              "rna_FileAssetSelectParams_asset_library_set",
                              "rna_FileAssetSelectParams_asset_library_itemf");
  RNA_def_property_ui_text(prop, "Asset Library", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, NULL);

  prop = RNA_def_property(srna, "asset_category", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, asset_category_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_FileAssetSelectParams_asset_category_get",
                              "rna_FileAssetSelectParams_asset_category_set",
                              NULL);
  RNA_def_property_ui_text(prop, "Asset Category", "Determine which kind of assets to display");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_LIST, NULL);
}

static void rna_def_filemenu_entry(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "FileBrowserFSMenuEntry", NULL);
  RNA_def_struct_sdna(srna, "FSMenuEntry");
  RNA_def_struct_ui_text(srna, "File Select Parameters", "File Select Parameters");

  prop = RNA_def_property(srna, "path", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_string_funcs(prop,
                                "rna_FileBrowser_FSMenuEntry_path_get",
                                "rna_FileBrowser_FSMenuEntry_path_length",
                                "rna_FileBrowser_FSMenuEntry_path_set");
  RNA_def_property_ui_text(prop, "Path", "");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_FileBrowser_FSMenuEntry_name_get",
                                "rna_FileBrowser_FSMenuEntry_name_length",
                                "rna_FileBrowser_FSMenuEntry_name_set");
  RNA_def_property_editable_func(prop, "rna_FileBrowser_FSMenuEntry_name_get_editable");
  RNA_def_property_ui_text(prop, "Name", "");
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "icon", PROP_INT, PROP_NONE);
  RNA_def_property_int_funcs(
      prop, "rna_FileBrowser_FSMenuEntry_icon_get", "rna_FileBrowser_FSMenuEntry_icon_set", NULL);
  RNA_def_property_ui_text(prop, "Icon", "");

  prop = RNA_def_property(srna, "use_save", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_FileBrowser_FSMenuEntry_use_save_get", NULL);
  RNA_def_property_ui_text(
      prop, "Save", "Whether this path is saved in bookmarks, or generated from OS");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "is_valid", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_FileBrowser_FSMenuEntry_is_valid_get", NULL);
  RNA_def_property_ui_text(prop, "Valid", "Whether this path is currently reachable");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
}

static void rna_def_space_filebrowser(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SpaceFileBrowser", "Space");
  RNA_def_struct_sdna(srna, "SpaceFile");
  RNA_def_struct_ui_text(srna, "Space File Browser", "File browser space data");

  rna_def_space_generic_show_region_toggles(srna, (1 << RGN_TYPE_TOOLS) | (1 << RGN_TYPE_UI));

  prop = RNA_def_property(srna, "browse_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_space_file_browse_mode_items);
  RNA_def_property_ui_text(
      prop,
      "Browsing Mode",
      "Type of the File Editor view (regular file browsing or asset browsing)");
  RNA_def_property_update(prop, 0, "rna_SpaceFileBrowser_browse_mode_update");

  prop = RNA_def_property(srna, "params", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "FileSelectParams");
  RNA_def_property_pointer_funcs(
      prop, "rna_FileBrowser_params_get", NULL, "rna_FileBrowser_params_typef", NULL);
  RNA_def_property_ui_text(
      prop, "Filebrowser Parameter", "Parameters and Settings for the Filebrowser");

  prop = RNA_def_property(srna, "active_operator", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "op");
  RNA_def_property_ui_text(prop, "Active Operator", "");

  /* keep this for compatibility with existing presets,
   * not exposed in c++ api because of keyword conflict */
  prop = RNA_def_property(srna, "operator", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "op");
  RNA_def_property_ui_text(prop, "Active Operator", "");

  /* bookmarks, recent files etc. */
  prop = RNA_def_collection(srna,
                            "system_folders",
                            "FileBrowserFSMenuEntry",
                            "System Folders",
                            "System's folders (usually root, available hard drives, etc)");
  RNA_def_property_collection_funcs(prop,
                                    "rna_FileBrowser_FSMenuSystem_data_begin",
                                    "rna_FileBrowser_FSMenu_next",
                                    "rna_FileBrowser_FSMenu_end",
                                    "rna_FileBrowser_FSMenu_get",
                                    "rna_FileBrowser_FSMenuSystem_data_length",
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_int(srna,
                     "system_folders_active",
                     -1,
                     -1,
                     INT_MAX,
                     "Active System Folder",
                     "Index of active system folder (-1 if none)",
                     -1,
                     INT_MAX);
  RNA_def_property_int_sdna(prop, NULL, "systemnr");
  RNA_def_property_int_funcs(prop,
                             "rna_FileBrowser_FSMenuSystem_active_get",
                             "rna_FileBrowser_FSMenuSystem_active_set",
                             "rna_FileBrowser_FSMenuSystem_active_range");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_FILE_PARAMS, "rna_FileBrowser_FSMenu_active_update");

  prop = RNA_def_collection(srna,
                            "system_bookmarks",
                            "FileBrowserFSMenuEntry",
                            "System Bookmarks",
                            "System's bookmarks");
  RNA_def_property_collection_funcs(prop,
                                    "rna_FileBrowser_FSMenuSystemBookmark_data_begin",
                                    "rna_FileBrowser_FSMenu_next",
                                    "rna_FileBrowser_FSMenu_end",
                                    "rna_FileBrowser_FSMenu_get",
                                    "rna_FileBrowser_FSMenuSystemBookmark_data_length",
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_int(srna,
                     "system_bookmarks_active",
                     -1,
                     -1,
                     INT_MAX,
                     "Active System Bookmark",
                     "Index of active system bookmark (-1 if none)",
                     -1,
                     INT_MAX);
  RNA_def_property_int_sdna(prop, NULL, "system_bookmarknr");
  RNA_def_property_int_funcs(prop,
                             "rna_FileBrowser_FSMenuSystemBookmark_active_get",
                             "rna_FileBrowser_FSMenuSystemBookmark_active_set",
                             "rna_FileBrowser_FSMenuSystemBookmark_active_range");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_FILE_PARAMS, "rna_FileBrowser_FSMenu_active_update");

  prop = RNA_def_collection(
      srna, "bookmarks", "FileBrowserFSMenuEntry", "Bookmarks", "User's bookmarks");
  RNA_def_property_collection_funcs(prop,
                                    "rna_FileBrowser_FSMenuBookmark_data_begin",
                                    "rna_FileBrowser_FSMenu_next",
                                    "rna_FileBrowser_FSMenu_end",
                                    "rna_FileBrowser_FSMenu_get",
                                    "rna_FileBrowser_FSMenuBookmark_data_length",
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_int(srna,
                     "bookmarks_active",
                     -1,
                     -1,
                     INT_MAX,
                     "Active Bookmark",
                     "Index of active bookmark (-1 if none)",
                     -1,
                     INT_MAX);
  RNA_def_property_int_sdna(prop, NULL, "bookmarknr");
  RNA_def_property_int_funcs(prop,
                             "rna_FileBrowser_FSMenuBookmark_active_get",
                             "rna_FileBrowser_FSMenuBookmark_active_set",
                             "rna_FileBrowser_FSMenuBookmark_active_range");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_FILE_PARAMS, "rna_FileBrowser_FSMenu_active_update");

  prop = RNA_def_collection(
      srna, "recent_folders", "FileBrowserFSMenuEntry", "Recent Folders", "");
  RNA_def_property_collection_funcs(prop,
                                    "rna_FileBrowser_FSMenuRecent_data_begin",
                                    "rna_FileBrowser_FSMenu_next",
                                    "rna_FileBrowser_FSMenu_end",
                                    "rna_FileBrowser_FSMenu_get",
                                    "rna_FileBrowser_FSMenuRecent_data_length",
                                    NULL,
                                    NULL,
                                    NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_int(srna,
                     "recent_folders_active",
                     -1,
                     -1,
                     INT_MAX,
                     "Active Recent Folder",
                     "Index of active recent folder (-1 if none)",
                     -1,
                     INT_MAX);
  RNA_def_property_int_sdna(prop, NULL, "recentnr");
  RNA_def_property_int_funcs(prop,
                             "rna_FileBrowser_FSMenuRecent_active_get",
                             "rna_FileBrowser_FSMenuRecent_active_set",
                             "rna_FileBrowser_FSMenuRecent_active_range");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_FILE_PARAMS, "rna_FileBrowser_FSMenu_active_update");

  RNA_api_space_filebrowser(srna);
}

static void rna_def_space_info(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SpaceInfo", "Space");
  RNA_def_struct_sdna(srna, "SpaceInfo");
  RNA_def_struct_ui_text(srna, "Space Info", "Info space data");

  /* reporting display */
  prop = RNA_def_property(srna, "show_report_debug", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "rpt_mask", INFO_RPT_DEBUG);
  RNA_def_property_ui_text(prop, "Show Debug", "Display debug reporting info");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_INFO_REPORT, NULL);

  prop = RNA_def_property(srna, "show_report_info", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "rpt_mask", INFO_RPT_INFO);
  RNA_def_property_ui_text(prop, "Show Info", "Display general information");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_INFO_REPORT, NULL);

  prop = RNA_def_property(srna, "show_report_operator", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "rpt_mask", INFO_RPT_OP);
  RNA_def_property_ui_text(prop, "Show Operator", "Display the operator log");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_INFO_REPORT, NULL);

  prop = RNA_def_property(srna, "show_report_warning", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "rpt_mask", INFO_RPT_WARN);
  RNA_def_property_ui_text(prop, "Show Warn", "Display warnings");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_INFO_REPORT, NULL);

  prop = RNA_def_property(srna, "show_report_error", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "rpt_mask", INFO_RPT_ERR);
  RNA_def_property_ui_text(prop, "Show Error", "Display error text");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_INFO_REPORT, NULL);
}

static void rna_def_space_userpref(BlenderRNA *brna)
{
  static const EnumPropertyItem filter_type_items[] = {
      {0, "NAME", 0, "Name", "Filter based on the operator name"},
      {1, "KEY", 0, "Key-Binding", "Filter based on key bindings"},
      {0, NULL, 0, NULL, NULL},
  };

  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SpacePreferences", "Space");
  RNA_def_struct_sdna(srna, "SpaceUserPref");
  RNA_def_struct_ui_text(srna, "Space Preferences", "Blender preferences space data");

  prop = RNA_def_property(srna, "filter_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "filter_type");
  RNA_def_property_enum_items(prop, filter_type_items);
  RNA_def_property_ui_text(prop, "Filter Type", "Filter method");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE, NULL);

  prop = RNA_def_property(srna, "filter_text", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, NULL, "filter");
  RNA_def_property_flag(prop, PROP_TEXTEDIT_UPDATE);
  RNA_def_property_ui_text(prop, "Filter", "Search term for filtering in the UI");
}

static void rna_def_node_tree_path(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "NodeTreePath", NULL);
  RNA_def_struct_sdna(srna, "bNodeTreePath");
  RNA_def_struct_ui_text(srna, "Node Tree Path", "Element of the node space tree path");

  prop = RNA_def_property(srna, "node_tree", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "nodetree");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Node Tree", "Base node tree from context");
}

static void rna_def_space_node_path_api(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop, *parm;
  FunctionRNA *func;

  RNA_def_property_srna(cprop, "SpaceNodeEditorPath");
  srna = RNA_def_struct(brna, "SpaceNodeEditorPath", NULL);
  RNA_def_struct_sdna(srna, "SpaceNode");
  RNA_def_struct_ui_text(srna, "Space Node Editor Path", "History of node trees in the editor");

  prop = RNA_def_property(srna, "to_string", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, "rna_SpaceNodeEditor_path_get", "rna_SpaceNodeEditor_path_length", NULL);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_struct_ui_text(srna, "Path", "Get the node tree path as a string");

  func = RNA_def_function(srna, "clear", "rna_SpaceNodeEditor_path_clear");
  RNA_def_function_ui_description(func, "Reset the node tree path");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);

  func = RNA_def_function(srna, "start", "rna_SpaceNodeEditor_path_start");
  RNA_def_function_ui_description(func, "Set the root node tree");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(func, "node_tree", "NodeTree", "Node Tree", "");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED | PARM_RNAPTR);

  func = RNA_def_function(srna, "append", "rna_SpaceNodeEditor_path_append");
  RNA_def_function_ui_description(func, "Append a node group tree to the path");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(
      func, "node_tree", "NodeTree", "Node Tree", "Node tree to append to the node editor path");
  RNA_def_parameter_flags(parm, 0, PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_pointer(func, "node", "Node", "Node", "Group node linking to this node tree");
  RNA_def_parameter_flags(parm, 0, PARM_RNAPTR);

  func = RNA_def_function(srna, "pop", "rna_SpaceNodeEditor_path_pop");
  RNA_def_function_ui_description(func, "Remove the last node tree from the path");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
}

static void rna_def_space_node(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem texture_id_type_items[] = {
      {SNODE_TEX_WORLD, "WORLD", ICON_WORLD_DATA, "World", "Edit texture nodes from World"},
      {SNODE_TEX_BRUSH, "BRUSH", ICON_BRUSH_DATA, "Brush", "Edit texture nodes from Brush"},
#  ifdef WITH_FREESTYLE
      {SNODE_TEX_LINESTYLE,
       "LINESTYLE",
       ICON_LINE_DATA,
       "Line Style",
       "Edit texture nodes from Line Style"},
#  endif
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem shader_type_items[] = {
      {SNODE_SHADER_OBJECT, "OBJECT", ICON_OBJECT_DATA, "Object", "Edit shader nodes from Object"},
      {SNODE_SHADER_WORLD, "WORLD", ICON_WORLD_DATA, "World", "Edit shader nodes from World"},
#  ifdef WITH_FREESTYLE
      {SNODE_SHADER_LINESTYLE,
       "LINESTYLE",
       ICON_LINE_DATA,
       "Line Style",
       "Edit shader nodes from Line Style"},
#  endif
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem backdrop_channels_items[] = {
      {SNODE_USE_ALPHA,
       "COLOR_ALPHA",
       ICON_IMAGE_RGB_ALPHA,
       "Color and Alpha",
       "Display image with RGB colors and alpha transparency"},
      {0, "COLOR", ICON_IMAGE_RGB, "Color", "Display image with RGB colors"},
      {SNODE_SHOW_ALPHA, "ALPHA", ICON_IMAGE_ALPHA, "Alpha", "Display alpha transparency channel"},
      {SNODE_SHOW_R, "RED", ICON_COLOR_RED, "Red", ""},
      {SNODE_SHOW_G, "GREEN", ICON_COLOR_GREEN, "Green", ""},
      {SNODE_SHOW_B, "BLUE", ICON_COLOR_BLUE, "Blue", ""},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem insert_ofs_dir_items[] = {
      {SNODE_INSERTOFS_DIR_RIGHT, "RIGHT", 0, "Right"},
      {SNODE_INSERTOFS_DIR_LEFT, "LEFT", 0, "Left"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem dummy_items[] = {
      {0, "DUMMY", 0, "", ""},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "SpaceNodeEditor", "Space");
  RNA_def_struct_sdna(srna, "SpaceNode");
  RNA_def_struct_ui_text(srna, "Space Node Editor", "Node editor space data");

  rna_def_space_generic_show_region_toggles(srna, (1 << RGN_TYPE_TOOLS) | (1 << RGN_TYPE_UI));

  prop = RNA_def_property(srna, "tree_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, dummy_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_SpaceNodeEditor_tree_type_get",
                              "rna_SpaceNodeEditor_tree_type_set",
                              "rna_SpaceNodeEditor_tree_type_itemf");
  RNA_def_property_ui_text(prop, "Tree Type", "Node tree type to display and edit");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE, NULL);

  prop = RNA_def_property(srna, "texture_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "texfrom");
  RNA_def_property_enum_items(prop, texture_id_type_items);
  RNA_def_property_ui_text(prop, "Texture Type", "Type of data to take texture from");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE, NULL);

  prop = RNA_def_property(srna, "shader_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "shaderfrom");
  RNA_def_property_enum_items(prop, shader_type_items);
  RNA_def_property_ui_text(prop, "Shader Type", "Type of data to take shader from");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE, NULL);

  prop = RNA_def_property(srna, "id", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "ID", "Data-block whose nodes are being edited");

  prop = RNA_def_property(srna, "id_from", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "from");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "ID From", "Data-block from which the edited data-block is linked");

  prop = RNA_def_property(srna, "path", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, NULL, "treepath", NULL);
  RNA_def_property_struct_type(prop, "NodeTreePath");
  RNA_def_property_ui_text(
      prop, "Node Tree Path", "Path from the data-block to the currently edited node tree");
  rna_def_space_node_path_api(brna, prop);

  prop = RNA_def_property(srna, "node_tree", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_funcs(
      prop, NULL, "rna_SpaceNodeEditor_node_tree_set", NULL, "rna_SpaceNodeEditor_node_tree_poll");
  RNA_def_property_pointer_sdna(prop, NULL, "nodetree");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_CONTEXT_UPDATE);
  RNA_def_property_ui_text(prop, "Node Tree", "Base node tree from context");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE, "rna_SpaceNodeEditor_node_tree_update");

  prop = RNA_def_property(srna, "edit_tree", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "edittree");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Edit Tree", "Node tree being displayed and edited");

  prop = RNA_def_property(srna, "pin", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SNODE_PIN);
  RNA_def_property_ui_text(prop, "Pinned", "Use the pinned node tree");
  RNA_def_property_ui_icon(prop, ICON_UNPINNED, 1);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE, NULL);

  prop = RNA_def_property(srna, "show_backdrop", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SNODE_BACKDRAW);
  RNA_def_property_ui_text(
      prop, "Backdrop", "Use active Viewer Node output as backdrop for compositing nodes");
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_NODE_VIEW, "rna_SpaceNodeEditor_show_backdrop_update");

  prop = RNA_def_property(srna, "show_annotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SNODE_SHOW_GPENCIL);
  RNA_def_property_ui_text(prop, "Show Annotation", "Show annotations for this view");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE_VIEW, NULL);

  prop = RNA_def_property(srna, "use_auto_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SNODE_AUTO_RENDER);
  RNA_def_property_ui_text(
      prop, "Auto Render", "Re-render and composite changed layers on 3D edits");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE_VIEW, NULL);

  prop = RNA_def_property(srna, "backdrop_zoom", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "zoom");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0.01f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.01, 100, 1, 2);
  RNA_def_property_ui_text(prop, "Backdrop Zoom", "Backdrop zoom factor");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE_VIEW, NULL);

  prop = RNA_def_property(srna, "backdrop_offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, NULL, "xof");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "Backdrop Offset", "Backdrop offset");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE_VIEW, NULL);

  prop = RNA_def_property(srna, "backdrop_channels", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "flag");
  RNA_def_property_enum_items(prop, backdrop_channels_items);
  RNA_def_property_ui_text(prop, "Display Channels", "Channels of the image to draw");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE_VIEW, NULL);
  /* the mx/my "cursor" in the node editor is used only by operators to store the mouse position */
  prop = RNA_def_property(srna, "cursor_location", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_array(prop, 2);
  RNA_def_property_float_funcs(prop,
                               "rna_SpaceNodeEditor_cursor_location_get",
                               "rna_SpaceNodeEditor_cursor_location_set",
                               NULL);
  RNA_def_property_ui_text(prop, "Cursor Location", "Location for adding new nodes");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE_VIEW, NULL);

  /* insert offset (called "Auto-offset" in UI) */
  prop = RNA_def_property(srna, "use_insert_offset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SNODE_SKIP_INSOFFSET);
  RNA_def_property_ui_text(prop,
                           "Auto-offset",
                           "Automatically offset the following or previous nodes in a "
                           "chain when inserting a new node");
  RNA_def_property_ui_icon(prop, ICON_NODE_INSERT_ON, 1);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE_VIEW, NULL);

  prop = RNA_def_property(srna, "insert_offset_direction", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, NULL, "insert_ofs_dir");
  RNA_def_property_enum_items(prop, insert_ofs_dir_items);
  RNA_def_property_ui_text(
      prop, "Auto-offset Direction", "Direction to offset nodes on insertion");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE_VIEW, NULL);

  RNA_api_space_node(srna);
}

static void rna_def_space_clip(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem view_items[] = {
      {SC_VIEW_CLIP, "CLIP", ICON_SEQUENCE, "Clip", "Show editing clip preview"},
      {SC_VIEW_GRAPH, "GRAPH", ICON_GRAPH, "Graph", "Show graph view for active element"},
      {SC_VIEW_DOPESHEET,
       "DOPESHEET",
       ICON_ACTION,
       "Dopesheet",
       "Dopesheet view for tracking data"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem annotation_source_items[] = {
      {SC_GPENCIL_SRC_CLIP,
       "CLIP",
       0,
       "Clip",
       "Show annotation data-block which belongs to movie clip"},
      {SC_GPENCIL_SRC_TRACK,
       "TRACK",
       0,
       "Track",
       "Show annotation data-block which belongs to active track"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem pivot_items[] = {
      {V3D_AROUND_CENTER_BOUNDS,
       "BOUNDING_BOX_CENTER",
       ICON_PIVOT_BOUNDBOX,
       "Bounding Box Center",
       "Pivot around bounding box center of selected object(s)"},
      {V3D_AROUND_CURSOR, "CURSOR", ICON_PIVOT_CURSOR, "2D Cursor", "Pivot around the 2D cursor"},
      {V3D_AROUND_LOCAL_ORIGINS,
       "INDIVIDUAL_ORIGINS",
       ICON_PIVOT_INDIVIDUAL,
       "Individual Origins",
       "Pivot around each object's own origin"},
      {V3D_AROUND_CENTER_MEDIAN,
       "MEDIAN_POINT",
       ICON_PIVOT_MEDIAN,
       "Median Point",
       "Pivot around the median point of selected objects"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "SpaceClipEditor", "Space");
  RNA_def_struct_sdna(srna, "SpaceClip");
  RNA_def_struct_ui_text(srna, "Space Clip Editor", "Clip editor space data");

  rna_def_space_generic_show_region_toggles(
      srna, (1 << RGN_TYPE_TOOLS) | (1 << RGN_TYPE_UI) | (1 << RGN_TYPE_HUD));

  /* movieclip */
  prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Movie Clip", "Movie clip displayed and edited in this space");
  RNA_def_property_pointer_funcs(prop, NULL, "rna_SpaceClipEditor_clip_set", NULL, NULL);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* clip user */
  prop = RNA_def_property(srna, "clip_user", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "MovieClipUser");
  RNA_def_property_pointer_sdna(prop, NULL, "user");
  RNA_def_property_ui_text(
      prop, "Movie Clip User", "Parameters defining which frame of the movie clip is displayed");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* mask */
  rna_def_space_mask_info(srna, NC_SPACE | ND_SPACE_CLIP, "rna_SpaceClipEditor_mask_set");

  /* mode */
  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "mode");
  RNA_def_property_enum_items(prop, rna_enum_clip_editor_mode_items);
  RNA_def_property_ui_text(prop, "Mode", "Editing context being displayed");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, "rna_SpaceClipEditor_clip_mode_update");

  /* view */
  prop = RNA_def_property(srna, "view", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "view");
  RNA_def_property_enum_items(prop, view_items);
  RNA_def_property_ui_text(prop, "View", "Type of the clip editor view");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MOVIECLIP);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, "rna_SpaceClipEditor_view_type_update");

  /* show pattern */
  prop = RNA_def_property(srna, "show_marker_pattern", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(prop, "Show Marker Pattern", "Show pattern boundbox for markers");
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SC_SHOW_MARKER_PATTERN);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* show search */
  prop = RNA_def_property(srna, "show_marker_search", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(prop, "Show Marker Search", "Show search boundbox for markers");
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SC_SHOW_MARKER_SEARCH);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* lock to selection */
  prop = RNA_def_property(srna, "lock_selection", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Lock to Selection", "Lock viewport to selected markers during playback");
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SC_LOCK_SELECTION);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_CLIP, "rna_SpaceClipEditor_lock_selection_update");

  /* lock to time cursor */
  prop = RNA_def_property(srna, "lock_time_cursor", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Lock to Time Cursor", "Lock curves view to time cursor during playback and tracking");
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SC_LOCK_TIMECURSOR);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* show markers paths */
  prop = RNA_def_property(srna, "show_track_path", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SC_SHOW_TRACK_PATH);
  RNA_def_property_ui_text(prop, "Show Track Path", "Show path of how track moves");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* path length */
  prop = RNA_def_property(srna, "path_length", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, NULL, "path_length");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_ui_text(prop, "Path Length", "Length of displaying path, in frames");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* show tiny markers */
  prop = RNA_def_property(srna, "show_tiny_markers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(prop, "Show Tiny Markers", "Show markers in a more compact manner");
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SC_SHOW_TINY_MARKER);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* show bundles */
  prop = RNA_def_property(srna, "show_bundles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(prop, "Show Bundles", "Show projection of 3D markers into footage");
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SC_SHOW_BUNDLES);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* mute footage */
  prop = RNA_def_property(srna, "use_mute_footage", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(prop, "Mute Footage", "Mute footage and show black background instead");
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SC_MUTE_FOOTAGE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* hide disabled */
  prop = RNA_def_property(srna, "show_disabled", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(prop, "Show Disabled", "Show disabled tracks from the footage");
  RNA_def_property_boolean_negative_sdna(prop, NULL, "flag", SC_HIDE_DISABLED);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  prop = RNA_def_property(srna, "show_metadata", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SC_SHOW_METADATA);
  RNA_def_property_ui_text(prop, "Show Metadata", "Show metadata of clip");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* scopes */
  prop = RNA_def_property(srna, "scopes", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, NULL, "scopes");
  RNA_def_property_struct_type(prop, "MovieClipScopes");
  RNA_def_property_ui_text(prop, "Scopes", "Scopes to visualize movie clip statistics");

  /* show names */
  prop = RNA_def_property(srna, "show_names", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SC_SHOW_NAMES);
  RNA_def_property_ui_text(prop, "Show Names", "Show track names and status");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* show grid */
  prop = RNA_def_property(srna, "show_grid", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SC_SHOW_GRID);
  RNA_def_property_ui_text(prop, "Show Grid", "Show grid showing lens distortion");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* show stable */
  prop = RNA_def_property(srna, "show_stable", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SC_SHOW_STABLE);
  RNA_def_property_ui_text(
      prop, "Show Stable", "Show stable footage in editor (if stabilization is enabled)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* manual calibration */
  prop = RNA_def_property(srna, "use_manual_calibration", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SC_MANUAL_CALIBRATION);
  RNA_def_property_ui_text(prop, "Manual Calibration", "Use manual calibration helpers");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* show annotation */
  prop = RNA_def_property(srna, "show_annotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SC_SHOW_ANNOTATION);
  RNA_def_property_ui_text(prop, "Show Annotation", "Show annotations for this view");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* show filters */
  prop = RNA_def_property(srna, "show_filters", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SC_SHOW_FILTERS);
  RNA_def_property_ui_text(prop, "Show Filters", "Show filters for graph editor");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* show graph_frames */
  prop = RNA_def_property(srna, "show_graph_frames", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SC_SHOW_GRAPH_FRAMES);
  RNA_def_property_ui_text(
      prop,
      "Show Frames",
      "Show curve for per-frame average error (camera motion should be solved first)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* show graph tracks motion */
  prop = RNA_def_property(srna, "show_graph_tracks_motion", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SC_SHOW_GRAPH_TRACKS_MOTION);
  RNA_def_property_ui_text(
      prop,
      "Show Tracks Motion",
      "Display the speed curves (in \"x\" direction red, in \"y\" direction green) "
      "for the selected tracks");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* show graph tracks motion */
  prop = RNA_def_property(srna, "show_graph_tracks_error", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SC_SHOW_GRAPH_TRACKS_ERROR);
  RNA_def_property_ui_text(
      prop, "Show Tracks Error", "Display the reprojection error curve for selected tracks");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* show_only_selected */
  prop = RNA_def_property(srna, "show_graph_only_selected", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SC_SHOW_GRAPH_SEL_ONLY);
  RNA_def_property_ui_text(
      prop, "Only Show Selected", "Only include channels relating to selected objects and data");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_SELECT_OFF, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* show_hidden */
  prop = RNA_def_property(srna, "show_graph_hidden", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SC_SHOW_GRAPH_HIDDEN);
  RNA_def_property_ui_text(
      prop, "Display Hidden", "Include channels from objects/bone that aren't visible");
  RNA_def_property_ui_icon(prop, ICON_GHOST_ENABLED, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* ** channels ** */

  /* show_red_channel */
  prop = RNA_def_property(srna, "show_red_channel", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "postproc_flag", MOVIECLIP_DISABLE_RED);
  RNA_def_property_ui_text(prop, "Show Red Channel", "Show red channel in the frame");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* show_green_channel */
  prop = RNA_def_property(srna, "show_green_channel", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "postproc_flag", MOVIECLIP_DISABLE_GREEN);
  RNA_def_property_ui_text(prop, "Show Green Channel", "Show green channel in the frame");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* show_blue_channel */
  prop = RNA_def_property(srna, "show_blue_channel", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, NULL, "postproc_flag", MOVIECLIP_DISABLE_BLUE);
  RNA_def_property_ui_text(prop, "Show Blue Channel", "Show blue channel in the frame");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);

  /* preview_grayscale */
  prop = RNA_def_property(srna, "use_grayscale_preview", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "postproc_flag", MOVIECLIP_PREVIEW_GRAYSCALE);
  RNA_def_property_ui_text(prop, "Grayscale", "Display frame in grayscale mode");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, NULL);

  /* timeline */
  prop = RNA_def_property(srna, "show_seconds", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "flag", SC_SHOW_SECONDS);
  RNA_def_property_ui_text(prop, "Show Seconds", "Show timing in seconds not frames");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, NULL);

  /* grease pencil source */
  prop = RNA_def_property(srna, "annotation_source", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "gpencil_src");
  RNA_def_property_enum_items(prop, annotation_source_items);
  RNA_def_property_ui_text(prop, "Annotation Source", "Where the annotation comes from");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MOVIECLIP);
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, NULL);

  /* pivot point */
  prop = RNA_def_property(srna, "pivot_point", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, NULL, "around");
  RNA_def_property_enum_items(prop, pivot_items);
  RNA_def_property_ui_text(prop, "Pivot Point", "Pivot center for rotation/scaling");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, NULL);
}

static void rna_def_space_spreadsheet(BlenderRNA *brna)
{
  PropertyRNA *prop;
  StructRNA *srna;

  static const EnumPropertyItem geometry_component_type_items[] = {
      {GEO_COMPONENT_TYPE_MESH,
       "MESH",
       ICON_MESH_DATA,
       "Mesh",
       "Mesh component containing point, corner, edge and polygon data"},
      {GEO_COMPONENT_TYPE_POINT_CLOUD,
       "POINTCLOUD",
       ICON_POINTCLOUD_DATA,
       "Point Cloud",
       "Point cloud component containing only point data"},
      {0, NULL, 0, NULL, NULL},
  };

  static const EnumPropertyItem object_eval_state_items[] = {
      {SPREADSHEET_OBJECT_EVAL_STATE_FINAL,
       "FINAL",
       ICON_NONE,
       "Final",
       "Use data from object with all modifiers applied"},
      {SPREADSHEET_OBJECT_EVAL_STATE_ORIGINAL,
       "ORIGINAL",
       ICON_NONE,
       "Original",
       "Use data from original object without any modifiers applied"},
      {0, NULL, 0, NULL, NULL},
  };

  srna = RNA_def_struct(brna, "SpaceSpreadsheet", "Space");
  RNA_def_struct_ui_text(srna, "Space Spreadsheet", "Spreadsheet space data");

  rna_def_space_generic_show_region_toggles(srna, (1 << RGN_TYPE_FOOTER));

  prop = RNA_def_property(srna, "pinned_id", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_funcs(prop, NULL, "rna_SpaceSpreadsheet_pinned_id_set", NULL, NULL);
  RNA_def_property_ui_text(prop, "Pinned ID", "Data-block whose values are displayed");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, NULL);

  prop = RNA_def_property(srna, "show_only_selected", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, NULL, "filter_flag", SPREADSHEET_FILTER_SELECTED_ONLY);
  RNA_def_property_ui_text(
      prop, "Show Only Selected", "Only include rows that correspond to selected elements");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, NULL);

  prop = RNA_def_property(srna, "geometry_component_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, geometry_component_type_items);
  RNA_def_property_ui_text(
      prop, "Geometry Component", "Part of the geometry to display data from");
  RNA_def_property_update(prop,
                          NC_SPACE | ND_SPACE_SPREADSHEET,
                          "rna_SpaceSpreadsheet_geometry_component_type_update");

  prop = RNA_def_property(srna, "attribute_domain", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_attribute_domain_items);
  RNA_def_property_enum_funcs(prop, NULL, NULL, "rna_SpaceSpreadsheet_attribute_domain_itemf");
  RNA_def_property_ui_text(prop, "Attribute Domain", "Attribute domain to display");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, NULL);

  prop = RNA_def_property(srna, "object_eval_state", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, object_eval_state_items);
  RNA_def_property_ui_text(prop, "Object Evaluation State", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, NULL);
}

void RNA_def_space(BlenderRNA *brna)
{
  rna_def_space(brna);
  rna_def_space_image(brna);
  rna_def_space_sequencer(brna);
  rna_def_space_text(brna);
  rna_def_fileselect_entry(brna);
  rna_def_fileselect_params(brna);
  rna_def_fileselect_asset_params(brna);
  rna_def_fileselect_idfilter(brna);
  rna_def_filemenu_entry(brna);
  rna_def_space_filebrowser(brna);
  rna_def_space_outliner(brna);
  rna_def_space_view3d(brna);
  rna_def_space_properties(brna);
  rna_def_space_dopesheet(brna);
  rna_def_space_graph(brna);
  rna_def_space_nla(brna);
  rna_def_space_console(brna);
  rna_def_console_line(brna);
  rna_def_space_info(brna);
  rna_def_space_userpref(brna);
  rna_def_node_tree_path(brna);
  rna_def_space_node(brna);
  rna_def_space_clip(brna);
  rna_def_space_spreadsheet(brna);
}

#endif

/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup RNA
 */

#include <cstdlib>
#include <cstring>

#include "BLI_math_constants.h"
#include "BLI_string_ref.hh"
#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_geometry_set.hh"
#include "BKE_movieclip.h"

#include "ED_asset.hh"
#include "ED_buttons.hh"
#include "ED_spreadsheet.hh"

#include "BLI_string.h"
#include "BLI_sys_types.h"

#include "DNA_action_types.h"
#include "DNA_mask_types.h"
#include "DNA_object_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "RNA_define.hh"

#include "rna_internal.hh"

#include "SEQ_sequencer.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_enum_types.hh"

const EnumPropertyItem rna_enum_geometry_component_type_items[] = {
    {int(blender::bke::GeometryComponent::Type::Mesh),
     "MESH",
     ICON_MESH_DATA,
     "Mesh",
     "Mesh component containing point, corner, edge and face data"},
    {int(blender::bke::GeometryComponent::Type::PointCloud),
     "POINTCLOUD",
     ICON_POINTCLOUD_DATA,
     "Point Cloud",
     "Point cloud component containing only point data"},
    {int(blender::bke::GeometryComponent::Type::Curve),
     "CURVE",
     ICON_CURVE_DATA,
     "Curve",
     "Curve component containing spline and control point data"},
    {int(blender::bke::GeometryComponent::Type::Instance),
     "INSTANCES",
     ICON_EMPTY_AXIS,
     "Instances",
     "Instances of objects or collections"},
    {int(blender::bke::GeometryComponent::Type::GreasePencil),
     "GREASEPENCIL",
     ICON_GREASEPENCIL,
     "Grease Pencil",
     "Grease Pencil component containing layers and curves data"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_space_type_items[] = {
    /* empty must be here for python, is skipped for UI */
    {SPACE_EMPTY, "EMPTY", ICON_NONE, "Empty", ""},

    /* General. */
    RNA_ENUM_ITEM_HEADING(N_("General"), nullptr),
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
    {SPACE_SEQ,
     "SEQUENCE_EDITOR",
     ICON_SEQUENCE,
     "Video Sequencer",
     "Non-linear editor for arranging and mixing scenes, video, audio, and effects"},
    {SPACE_CLIP, "CLIP_EDITOR", ICON_TRACKER, "Movie Clip Editor", "Motion tracking tools"},

    /* Animation. */
    RNA_ENUM_ITEM_HEADING(N_("Animation"), nullptr),
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

    /* Scripting. */
    RNA_ENUM_ITEM_HEADING(N_("Scripting"), nullptr),
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

    /* Data. */
    RNA_ENUM_ITEM_HEADING(N_("Data"), nullptr),
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
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_space_graph_mode_items[] = {
    {SIPO_MODE_ANIMATION,
     "FCURVES",
     ICON_GRAPH,
     "Graph Editor",
     "Edit animation/keyframes displayed as 2D curves"},
    {SIPO_MODE_DRIVERS,
     "DRIVERS",
     ICON_DRIVER,
     "Drivers",
     "Define and edit drivers that link properties to custom functions or other data"},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_space_sequencer_view_type_items[] = {
    {SEQ_VIEW_SEQUENCE, "SEQUENCER", ICON_SEQ_SEQUENCER, "Sequencer", ""},
    {SEQ_VIEW_PREVIEW, "PREVIEW", ICON_SEQ_PREVIEW, "Preview", ""},
    {SEQ_VIEW_SEQUENCE_PREVIEW,
     "SEQUENCER_PREVIEW",
     ICON_SEQ_SPLITVIEW,
     "Sequencer & Preview",
     ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_space_file_browse_mode_items[] = {
    {FILE_BROWSE_MODE_FILES,
     "FILES",
     ICON_FILEBROWSER,
     "File Browser",
     "Built-in file manager for opening, saving, and linking data"},
    {FILE_BROWSE_MODE_ASSETS,
     "ASSETS",
     ICON_ASSET_MANAGER,
     "Asset Browser",
     "Manage assets in the current file and access linked asset libraries"},
    {0, nullptr, 0, nullptr, nullptr},
};

#define SACT_ITEM_DOPESHEET \
  {SACTCONT_DOPESHEET, "DOPESHEET", ICON_ACTION, "Dope Sheet", "Edit all keyframes in scene"}
#define SACT_ITEM_ACTION \
  {SACTCONT_ACTION, \
   "ACTION", \
   ICON_OBJECT_DATA, \
   "Action Editor", \
   "Edit keyframes in active object's Object-level action"}
#define SACT_ITEM_SHAPEKEY \
  {SACTCONT_SHAPEKEY, \
   "SHAPEKEY", \
   ICON_SHAPEKEY_DATA, \
   "Shape Key Editor", \
   "Edit keyframes in active object's Shape Keys action"}
#define SACT_ITEM_GPENCIL \
  {SACTCONT_GPENCIL, \
   "GPENCIL", \
   ICON_OUTLINER_OB_GREASEPENCIL, \
   "Grease Pencil", \
   "Edit timings for all Grease Pencil sketches in file"}
#define SACT_ITEM_MASK \
  {SACTCONT_MASK, "MASK", ICON_MOD_MASK, "Mask", "Edit timings for Mask Editor splines"}
#define SACT_ITEM_CACHEFILE \
  {SACTCONT_CACHEFILE, \
   "CACHEFILE", \
   ICON_FILE, \
   "Cache File", \
   "Edit timings for Cache File data-blocks"}
#define SACT_ITEM_TIMELINE \
  {SACTCONT_TIMELINE, \
   "TIMELINE", \
   ICON_TIME, \
   "Timeline", \
   "Simple timeline view with playback controls in the header, without channel list, " \
   "side-panel, or footer"}

#ifndef RNA_RUNTIME
/* XXX: action-editor is currently for object-level only actions,
 * so show that using object-icon hint */
static EnumPropertyItem rna_enum_space_action_mode_all_items[] = {
    SACT_ITEM_DOPESHEET,
    SACT_ITEM_ACTION,
    SACT_ITEM_SHAPEKEY,
    SACT_ITEM_GPENCIL,
    SACT_ITEM_MASK,
    SACT_ITEM_CACHEFILE,
    SACT_ITEM_TIMELINE,
    {0, nullptr, 0, nullptr, nullptr},
};
static EnumPropertyItem rna_enum_space_action_ui_mode_items[] = {
    SACT_ITEM_DOPESHEET,
    SACT_ITEM_ACTION,
    SACT_ITEM_SHAPEKEY,
    SACT_ITEM_GPENCIL,
    SACT_ITEM_MASK,
    SACT_ITEM_CACHEFILE,
    {0, nullptr, 0, nullptr, nullptr},
};
#endif

const EnumPropertyItem rna_enum_space_action_mode_items[] = {
    SACT_ITEM_DOPESHEET,
    SACT_ITEM_TIMELINE,
    {0, nullptr, 0, nullptr, nullptr},
};

#undef SACT_ITEM_DOPESHEET
#undef SACT_ITEM_ACTION
#undef SACT_ITEM_SHAPEKEY
#undef SACT_ITEM_GPENCIL
#undef SACT_ITEM_MASK
#undef SACT_ITEM_CACHEFILE
#undef SACT_ITEM_TIMELINE

#define SI_ITEM_VIEW(identifier, name, icon) \
  {SI_MODE_VIEW, identifier, icon, name, "Inspect images or render results"}
#define SI_ITEM_UV {SI_MODE_UV, "UV", ICON_UV, "UV Editor", "View and edit UVs"}
#define SI_ITEM_PAINT {SI_MODE_PAINT, "PAINT", ICON_TPAINT_HLT, "Paint", "Paint images in 2D"}
#define SI_ITEM_MASK {SI_MODE_MASK, "MASK", ICON_MOD_MASK, "Mask", "View and edit masks"}

const EnumPropertyItem rna_enum_space_image_mode_all_items[] = {
    SI_ITEM_VIEW("VIEW", "View", ICON_FILE_IMAGE),
    SI_ITEM_UV,
    SI_ITEM_PAINT,
    SI_ITEM_MASK,
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_space_image_mode_ui_items[] = {
    SI_ITEM_VIEW("VIEW", "View", ICON_FILE_IMAGE),
    SI_ITEM_PAINT,
    SI_ITEM_MASK,
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_space_image_mode_items[] = {
    SI_ITEM_VIEW("IMAGE_EDITOR", "Image Editor", ICON_IMAGE),
    SI_ITEM_UV,
    {0, nullptr, 0, nullptr, nullptr},
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
    V3D_S3D_CAMERA_LEFT V3D_S3D_CAMERA_RIGHT V3D_S3D_CAMERA_S3D{0, nullptr, 0, nullptr, nullptr},
};

#ifdef RNA_RUNTIME
static const EnumPropertyItem multiview_camera_items[] = {
    V3D_S3D_CAMERA_VIEWS V3D_S3D_CAMERA_S3D{0, nullptr, 0, nullptr, nullptr},
};
#endif

#undef V3D_S3D_CAMERA_LEFT
#undef V3D_S3D_CAMERA_RIGHT
#undef V3D_S3D_CAMERA_S3D
#undef V3D_S3D_CAMERA_VIEWS

/**
 * This will be split to give different items in file than in asset browsing mode, see
 * #rna_FileSelectParams_sort_method_itemf().
 */
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
    {FILE_SORT_ASSET_CATALOG,
     "ASSET_CATALOG",
     0,
     "Asset Catalog",
     "Sort the asset list so that assets in the same catalog are kept together. Within a single "
     "catalog, assets are ordered by name. The catalogs are in order of the flattened catalog "
     "hierarchy."},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_asset_import_method_items[] = {
    {FILE_ASSET_IMPORT_FOLLOW_PREFS,
     "FOLLOW_PREFS",
     0,
     "Follow Preferences",
     "Use the import method set in the Preferences for this asset library, don't override it "
     "for this Asset Browser"},
    {FILE_ASSET_IMPORT_LINK,
     "LINK",
     ICON_LINK_BLEND,
     "Link",
     "Import the assets as linked data-block"},
    {FILE_ASSET_IMPORT_APPEND,
     "APPEND",
     ICON_APPEND_BLEND,
     "Append",
     "Import the asset as copied data-block, with no link to the original asset data-block"},
    {FILE_ASSET_IMPORT_APPEND_REUSE,
     "APPEND_REUSE",
     ICON_APPEND_BLEND,
     "Append (Reuse Data)",
     "Import the asset as copied data-block while avoiding multiple copies of nested, "
     "typically heavy data. For example the textures of a material asset, or the mesh of an "
     "object asset, don't have to be copied every time this asset is imported. The instances of "
     "the asset share the data instead"},
    {FILE_ASSET_IMPORT_PACK,
     "PACK",
     ICON_PACKAGE,
     "Pack",
     "Import the asset as linked data-block, and pack it in the current file (ensures that it "
     "remains unchanged in case the library data is modified, is not available anymore, etc.)"},
    {0, nullptr, 0, nullptr, nullptr},
};

#ifndef RNA_RUNTIME
static const EnumPropertyItem stereo3d_eye_items[] = {
    {STEREO_LEFT_ID, "LEFT_EYE", ICON_NONE, "Left Eye"},
    {STEREO_RIGHT_ID, "RIGHT_EYE", ICON_NONE, "Right Eye"},
    {0, nullptr, 0, nullptr, nullptr},
};
#endif

static const EnumPropertyItem display_channels_items[] = {
    {SI_USE_ALPHA,
     "COLOR_ALPHA",
     ICON_IMAGE_RGB_ALPHA,
     "Color & Alpha",
     "Display image with RGB colors and alpha transparency"},
    {0, "COLOR", ICON_IMAGE_RGB, "Color", "Display image with RGB colors"},
    {SI_SHOW_ALPHA, "ALPHA", ICON_IMAGE_ALPHA, "Alpha", "Display alpha transparency channel"},
    {SI_SHOW_ZBUF,
     "Z_BUFFER",
     ICON_IMAGE_ZDEPTH,
     "Z-Buffer",
     "Display Z-buffer associated with image (mapped from camera clip start to end)"},
    {SI_SHOW_R, "RED", ICON_RGB_RED, "Red", ""},
    {SI_SHOW_G, "GREEN", ICON_RGB_GREEN, "Green", ""},
    {SI_SHOW_B, "BLUE", ICON_RGB_BLUE, "Blue", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_shading_type_items[] = {
    {OB_WIRE,
     "WIREFRAME",
     ICON_SHADING_WIRE,
     "Wireframe",
     "Display only edges of geometry without surface shading"},
    {OB_SOLID,
     "SOLID",
     ICON_SHADING_SOLID,
     "Solid",
     "Display objects with flat lighting and basic surface shading"},
    {OB_MATERIAL,
     "MATERIAL",
     ICON_SHADING_TEXTURE,
     "Material Preview",
     "Preview materials using predefined environment lights"},
    {OB_RENDER,
     "RENDERED",
     ICON_SHADING_RENDERED,
     "Rendered",
     "Preview the final scene using the active render engine"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_viewport_lighting_items[] = {
    {V3D_LIGHTING_STUDIO, "STUDIO", 0, "Studio", "Display using studio lighting"},
    {V3D_LIGHTING_MATCAP, "MATCAP", 0, "MatCap", "Display using matcap material and lighting"},
    {V3D_LIGHTING_FLAT, "FLAT", 0, "Flat", "Display using flat lighting"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_shading_color_type_items[] = {
    {V3D_SHADING_MATERIAL_COLOR, "MATERIAL", 0, "Material", "Show material color"},
    {V3D_SHADING_OBJECT_COLOR, "OBJECT", 0, "Object", "Show object color"},
    {V3D_SHADING_RANDOM_COLOR, "RANDOM", 0, "Random", "Show random object color"},
    {V3D_SHADING_VERTEX_COLOR, "VERTEX", 0, "Attribute", "Show active color attribute"},
    {V3D_SHADING_TEXTURE_COLOR,
     "TEXTURE",
     0,
     "Texture",
     "Show the texture from the active image texture node using the active UV map coordinates"},
    {V3D_SHADING_SINGLE_COLOR, "SINGLE", 0, "Custom", "Show scene in a single custom color"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_shading_wire_color_type_items[] = {
    {V3D_SHADING_SINGLE_COLOR,
     "THEME",
     0,
     "Theme",
     "Show scene wireframes with the theme's wire color"},
    {V3D_SHADING_OBJECT_COLOR, "OBJECT", 0, "Object", "Show object color on wireframe"},
    {V3D_SHADING_RANDOM_COLOR, "RANDOM", 0, "Random", "Show random object color on wireframe"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_studio_light_items[] = {
    {0, "DEFAULT", 0, "Default", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_view3dshading_render_pass_type_items[] = {
    RNA_ENUM_ITEM_HEADING(CTX_N_(BLT_I18NCONTEXT_RENDER_LAYER, "General"), nullptr),
    {EEVEE_RENDER_PASS_COMBINED, "COMBINED", 0, "Combined", ""},
    {EEVEE_RENDER_PASS_EMIT, "EMISSION", 0, "Emission", ""},
    {EEVEE_RENDER_PASS_ENVIRONMENT, "ENVIRONMENT", 0, "Environment", ""},
    {EEVEE_RENDER_PASS_AO, "AO", 0, "Ambient Occlusion", ""},
    {EEVEE_RENDER_PASS_SHADOW, "SHADOW", 0, "Shadow", ""},
    {EEVEE_RENDER_PASS_TRANSPARENT, "TRANSPARENT", 0, "Transparent", ""},

    RNA_ENUM_ITEM_HEADING(CTX_N_(BLT_I18NCONTEXT_RENDER_LAYER, "Light"), nullptr),
    {EEVEE_RENDER_PASS_DIFFUSE_LIGHT, "DIFFUSE_LIGHT", 0, "Diffuse Light", ""},
    {EEVEE_RENDER_PASS_DIFFUSE_COLOR, "DIFFUSE_COLOR", 0, "Diffuse Color", ""},
    {EEVEE_RENDER_PASS_SPECULAR_LIGHT, "SPECULAR_LIGHT", 0, "Specular Light", ""},
    {EEVEE_RENDER_PASS_SPECULAR_COLOR, "SPECULAR_COLOR", 0, "Specular Color", ""},
    {EEVEE_RENDER_PASS_VOLUME_LIGHT, "VOLUME_LIGHT", 0, "Volume Light", ""},

    RNA_ENUM_ITEM_HEADING(CTX_N_(BLT_I18NCONTEXT_RENDER_LAYER, "Data"), nullptr),
    {EEVEE_RENDER_PASS_POSITION, "POSITION", 0, "Position", ""},
    {EEVEE_RENDER_PASS_NORMAL, "NORMAL", 0, "Normal", ""},
    {EEVEE_RENDER_PASS_MIST, "MIST", 0, "Mist", ""},
    {EEVEE_RENDER_PASS_CRYPTOMATTE_OBJECT, "CryptoObject", 0, "CryptoObject", ""},
    {EEVEE_RENDER_PASS_CRYPTOMATTE_ASSET, "CryptoAsset", 0, "CryptoAsset", ""},
    {EEVEE_RENDER_PASS_CRYPTOMATTE_MATERIAL, "CryptoMaterial", 0, "CryptoMaterial", ""},

    RNA_ENUM_ITEM_HEADING(CTX_N_(BLT_I18NCONTEXT_RENDER_LAYER, "Shader AOV"), nullptr),
    {EEVEE_RENDER_PASS_AOV, "AOV", 0, "AOV", ""},

    {0, nullptr, 0, nullptr, nullptr},
};

const EnumPropertyItem rna_enum_clip_editor_mode_items[] = {
    {SC_MODE_TRACKING, "TRACKING", ICON_ANIM_DATA, "Tracking", "Show tracking and solving tools"},
    {SC_MODE_MASKEDIT, "MASK", ICON_MOD_MASK, "Mask", "Show mask editing tools"},
    {0, nullptr, 0, nullptr, nullptr},
};

/* Actually populated dynamically through a function,
 * but helps for context-less access (e.g. doc, i18n...). */
const EnumPropertyItem buttons_context_items[] = {
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
    {BCONTEXT_STRIP, "STRIP", ICON_SEQ_SEQUENCER, "Strip", "Strip Properties"},
    {BCONTEXT_STRIP_MODIFIER,
     "STRIP_MODIFIER",
     ICON_SEQ_STRIP_MODIFIER,
     "Strip Modifiers",
     "Strip Modifier Properties"},
    {0, nullptr, 0, nullptr, nullptr},
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
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem fileselectparams_display_type_items[] = {
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
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rna_enum_curve_display_handle_items[] = {
    {CURVE_HANDLE_NONE, "NONE", 0, "None", ""},
    {CURVE_HANDLE_SELECTED, "SELECTED", 0, "Selected", ""},
    {CURVE_HANDLE_ALL, "ALL", 0, "All", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem spreadsheet_object_eval_state_items[] = {
    {SPREADSHEET_OBJECT_EVAL_STATE_EVALUATED,
     "EVALUATED",
     ICON_NONE,
     "Evaluated",
     "Use data from fully or partially evaluated object"},
    {SPREADSHEET_OBJECT_EVAL_STATE_ORIGINAL,
     "ORIGINAL",
     ICON_NONE,
     "Original",
     "Use data from original object without any modifiers applied"},
    {SPREADSHEET_OBJECT_EVAL_STATE_VIEWER_NODE,
     "VIEWER_NODE",
     ICON_NONE,
     "Viewer Node",
     "Use intermediate data from viewer node"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem spreadsheet_table_id_type_items[] = {
    {SPREADSHEET_TABLE_ID_TYPE_GEOMETRY,
     "GEOMETRY",
     ICON_NONE,
     "Geometry",
     "Table contains geometry data"},
    {0, nullptr, 0, nullptr, nullptr},

};

#ifdef RNA_RUNTIME

#  include <algorithm>
#  include <fmt/format.h>

#  include "AS_asset_representation.hh"

#  include "DNA_anim_types.h"
#  include "DNA_asset_types.h"
#  include "DNA_key_types.h"
#  include "DNA_scene_types.h"
#  include "DNA_screen_types.h"
#  include "DNA_sequence_types.h"
#  include "DNA_userdef_types.h"

#  include "BLI_index_range.hh"
#  include "BLI_math_matrix.h"
#  include "BLI_math_rotation.h"
#  include "BLI_math_vector.h"
#  include "BLI_path_utils.hh"
#  include "BLI_string.h"

#  include "BKE_anim_data.hh"
#  include "BKE_brush.hh"
#  include "BKE_context.hh"
#  include "BKE_global.hh"
#  include "BKE_icons.hh"
#  include "BKE_idprop.hh"
#  include "BKE_image.hh"
#  include "BKE_key.hh"
#  include "BKE_layer.hh"
#  include "BKE_nla.hh"
#  include "BKE_node.hh"
#  include "BKE_paint.hh"
#  include "BKE_preferences.h"
#  include "BKE_scene.hh"
#  include "BKE_screen.hh"
#  include "BKE_studiolight.h"
#  include "BKE_workspace.hh"

#  include "DEG_depsgraph.hh"
#  include "DEG_depsgraph_build.hh"

#  include "ED_anim_api.hh"
#  include "ED_asset.hh"
#  include "ED_buttons.hh"
#  include "ED_clip.hh"
#  include "ED_fileselect.hh"
#  include "ED_image.hh"
#  include "ED_node.hh"
#  include "ED_screen.hh"
#  include "ED_sequencer.hh"
#  include "ED_spreadsheet.hh"
#  include "ED_text.hh"
#  include "ED_transform.hh"
#  include "ED_view3d.hh"

#  include "GPU_material.hh"

#  include "IMB_imbuf_types.hh"

#  include "UI_interface.hh"
#  include "UI_view2d.hh"

#  include "SEQ_proxy.hh"
#  include "SEQ_relations.hh"

#  include "RE_engine.h"

static StructRNA *rna_Space_refine(PointerRNA *ptr)
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

static ScrArea *rna_area_from_space(const PointerRNA *ptr)
{
  bScreen *screen = reinterpret_cast<bScreen *>(ptr->owner_id);
  SpaceLink *link = static_cast<SpaceLink *>(ptr->data);
  return BKE_screen_find_area_from_space(screen, link);
}

static void area_region_from_regiondata(bScreen *screen,
                                        void *regiondata,
                                        ScrArea **r_area,
                                        ARegion **r_region)
{
  *r_area = nullptr;
  *r_region = nullptr;

  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
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
  void *regiondata = (ptr->data);

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
      if (region->overlap && (area == CTX_wm_area(C)) && !(U.uiflag & USER_REDUCE_MOTION)) {
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
    if (region_tool_header != nullptr) {
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

static bool rna_Space_show_region_tool_props_get(PointerRNA *ptr)
{
  return !rna_Space_bool_from_region_flag_get_by_type(ptr, RGN_TYPE_TOOL_PROPS, RGN_FLAG_HIDDEN);
}
static void rna_Space_show_region_tool_props_set(PointerRNA *ptr, bool value)
{
  rna_Space_bool_from_region_flag_set_by_type(ptr, RGN_TYPE_TOOL_PROPS, RGN_FLAG_HIDDEN, !value);
}
static void rna_Space_show_region_tool_props_update(bContext *C, PointerRNA *ptr)
{
  rna_Space_bool_from_region_flag_update_by_type(C, ptr, RGN_TYPE_TOOL_PROPS, RGN_FLAG_HIDDEN);
}

/* Channels Region. */
static bool rna_Space_show_region_channels_get(PointerRNA *ptr)
{
  return !rna_Space_bool_from_region_flag_get_by_type(ptr, RGN_TYPE_CHANNELS, RGN_FLAG_HIDDEN);
}
static void rna_Space_show_region_channels_set(PointerRNA *ptr, bool value)
{
  rna_Space_bool_from_region_flag_set_by_type(ptr, RGN_TYPE_CHANNELS, RGN_FLAG_HIDDEN, !value);
}
static void rna_Space_show_region_channels_update(bContext *C, PointerRNA *ptr)
{
  rna_Space_bool_from_region_flag_update_by_type(C, ptr, RGN_TYPE_CHANNELS, RGN_FLAG_HIDDEN);
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

/* Asset Shelf Regions */
static bool rna_Space_show_region_asset_shelf_get(PointerRNA *ptr)
{
  return !rna_Space_bool_from_region_flag_get_by_type(ptr, RGN_TYPE_ASSET_SHELF, RGN_FLAG_HIDDEN);
}
static void rna_Space_show_region_asset_shelf_set(PointerRNA *ptr, bool value)
{
  rna_Space_bool_from_region_flag_set_by_type(ptr, RGN_TYPE_ASSET_SHELF, RGN_FLAG_HIDDEN, !value);
}
static int rna_Space_show_region_asset_shelf_editable(const PointerRNA *ptr, const char **r_info)
{
  ScrArea *area = rna_area_from_space(ptr);
  ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_ASSET_SHELF);

  if (!region) {
    return 0;
  }

  if (region->flag & RGN_FLAG_POLL_FAILED) {
    if (r_info) {
      *r_info = N_(
          "The asset shelf is not available in the current context (try changing the active mode "
          "or tool)");
    }
    return 0;
  }

  return PROP_EDITABLE;
}

static void rna_Space_show_region_asset_shelf_update(bContext *C, PointerRNA *ptr)
{
  rna_Space_bool_from_region_flag_update_by_type(C, ptr, RGN_TYPE_ASSET_SHELF, RGN_FLAG_HIDDEN);
}

/** \} */

static bool rna_Space_view2d_sync_get(PointerRNA *ptr)
{
  ScrArea *area;
  ARegion *region;

  area = rna_area_from_space(ptr); /* can be nullptr */
  if (area == nullptr) {
    return false;
  }

  if (area->spacetype == SPACE_CLIP) {
    region = BKE_area_find_region_type(area, RGN_TYPE_PREVIEW);
  }
  else {
    region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
  }
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

  area = rna_area_from_space(ptr); /* can be nullptr */
  if (!area) {
    return;
  }

  if (!UI_view2d_area_supports_sync(area)) {
    BKE_reportf(nullptr,
                RPT_ERROR,
                "'show_locked_time' is not supported for the '%s' editor",
                area->type->name);
    return;
  }

  if (area->spacetype == SPACE_CLIP) {
    region = BKE_area_find_region_type(area, RGN_TYPE_PREVIEW);
  }
  else {
    region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
  }
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

static void rna_Space_view2d_sync_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  ScrArea *area;
  ARegion *region;

  area = rna_area_from_space(ptr); /* can be nullptr */
  if (area == nullptr) {
    return;
  }

  if (area->spacetype == SPACE_CLIP) {
    region = BKE_area_find_region_type(area, RGN_TYPE_PREVIEW);
  }
  else {
    region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
  }

  if (region) {
    bScreen *screen = (bScreen *)ptr->owner_id;
    View2D *v2d = &region->v2d;

    UI_view2d_sync(screen, area, v2d, V2D_LOCK_SET);
  }
}

/* Space 3D View */
static void rna_SpaceView3D_camera_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  View3D *v3d = (View3D *)(ptr->data);
  if (v3d->scenelock && scene != nullptr) {
    wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);

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
    Scene *scene = ED_screen_scene_find(screen, static_cast<wmWindowManager *>(G_MAIN->wm.first));
    /* nullptr if the screen isn't in an active window (happens when setting from Python).
     * This could be moved to the update function, in that case the scene won't relate to the
     * screen so keep it working this way. */
    if (scene != nullptr) {
      v3d->camera = scene->camera;
    }
  }
}

static float rna_View3DOverlay_GridScaleUnit_get(PointerRNA *ptr)
{
  View3D *v3d = (View3D *)(ptr->data);
  bScreen *screen = (bScreen *)ptr->owner_id;
  Scene *scene = ED_screen_scene_find(screen, static_cast<wmWindowManager *>(G_MAIN->wm.first));
  if (scene != nullptr) {
    return ED_view3d_grid_scale(scene, v3d, nullptr);
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
  void *regiondata = nullptr;
  if (area) {
    ListBase *regionbase = (area->spacedata.first == v3d) ? &area->regionbase : &v3d->regionbase;
    ARegion *region = static_cast<ARegion *>(regionbase->last); /* always last in list, weak. */
    regiondata = region->regiondata;
  }

  return RNA_pointer_create_with_parent(*ptr, &RNA_RegionView3D, regiondata);
}

static void rna_SpaceView3D_object_type_visibility_update(Main * /*bmain*/,
                                                          Scene *scene,
                                                          PointerRNA * /*ptr*/)
{
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
}

static void rna_SpaceView3D_shading_use_compositor_update(Main * /*bmain*/,
                                                          Scene * /*scene*/,
                                                          PointerRNA * /*ptr*/)
{
  /* Nodes may display warnings when the compositor is enabled, so we need a redraw in that case,
   * and even when it gets disabled in order to potentially remove the warning. */
  WM_main_add_notifier(NC_SPACE | ND_SPACE_NODE, nullptr);
}

static void rna_SpaceView3D_retopology_update(Main * /*bmain*/, Scene *scene, PointerRNA * /*ptr*/)
{
  /* Retopology can change the visibility of active object.
   * There is no actual data change but we just notify the viewport engine to refresh and pickup
   * the new visibility. */
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
}

static void rna_SpaceView3D_show_overlay_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  /* If Retopology is enabled, toggling overlays can change the visibility of active object. */
  const View3D *v3d = static_cast<View3D *>(ptr->data);
  if (v3d->overlay.edit_flag & V3D_OVERLAY_EDIT_RETOPOLOGY) {
    rna_SpaceView3D_retopology_update(bmain, scene, ptr);
  }
}

static void rna_SpaceView3D_region_quadviews_begin(CollectionPropertyIterator *iter,
                                                   PointerRNA *ptr)
{
  View3D *v3d = (View3D *)(ptr->data);
  ScrArea *area = rna_area_from_space(ptr);
  int i = 3;

  ARegion *region = static_cast<ARegion *>(
      ((area && area->spacedata.first == v3d) ? &area->regionbase : &v3d->regionbase)->last);
  ListBase lb = {nullptr, nullptr};

  if (region && region->alignment == RGN_ALIGN_QSPLIT) {
    while (i-- && region) {
      region = region->prev;
    }

    if (i < 0) {
      lb.first = region;
    }
  }

  rna_iterator_listbase_begin(iter, ptr, &lb, nullptr);
}

static PointerRNA rna_SpaceView3D_region_quadviews_get(CollectionPropertyIterator *iter)
{
  void *regiondata = ((ARegion *)rna_iterator_listbase_get(iter))->regiondata;

  return RNA_pointer_create_with_parent(iter->parent, &RNA_RegionView3D, regiondata);
}

static void rna_RegionView3D_quadview_update(Main * /*main*/, Scene * /*scene*/, PointerRNA *ptr)
{
  ScrArea *area;
  ARegion *region;

  rna_area_region_from_regiondata(ptr, &area, &region);
  if (area && region && region->alignment == RGN_ALIGN_QSPLIT) {
    ED_view3d_quadview_update(area, region, false);
  }
}

/** Same as #rna_RegionView3D_quadview_update but call `clip == true`. */
static void rna_RegionView3D_quadview_clip_update(Main * /*main*/,
                                                  Scene * /*scene*/,
                                                  PointerRNA *ptr)
{
  ScrArea *area;
  ARegion *region;

  rna_area_region_from_regiondata(ptr, &area, &region);
  if (area && region && region->alignment == RGN_ALIGN_QSPLIT) {
    ED_view3d_quadview_update(area, region, true);
  }
}

/**
 * After the rotation changes, either clear the view axis
 * or update it not to be aligned to an axis, without this the viewport will show
 * text that doesn't match the rotation.
 */
static void rna_RegionView3D_view_rotation_set_validate_view_axis(RegionView3D *rv3d)
{
  /* Never rotate from a "User" view into an axis aligned view,
   * otherwise rotation could be aligned by accident - giving unexpected behavior. */
  if (!RV3D_VIEW_IS_AXIS(rv3d->view)) {
    return;
  }
  /* Keep this small as script authors wont expect the assigned value to change. */
  const float eps_quat = 1e-6f;
  ED_view3d_quat_to_axis_view_and_reset_quat(
      rv3d->viewquat, eps_quat, &rv3d->view, &rv3d->view_axis_roll);
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
  rna_RegionView3D_view_rotation_set_validate_view_axis(rv3d);
}

static void rna_RegionView3D_view_matrix_set(PointerRNA *ptr, const float *values)
{
  RegionView3D *rv3d = (RegionView3D *)(ptr->data);
  float mat[4][4];
  invert_m4_m4(mat, (float (*)[4])values);
  ED_view3d_from_m4(mat, rv3d->ofs, rv3d->viewquat, &rv3d->dist);
  rna_RegionView3D_view_rotation_set_validate_view_axis(rv3d);
}

static bool rna_RegionView3D_is_orthographic_side_view_get(PointerRNA *ptr)
{
  /* NOTE: only checks axis alignment, not orthographic,
   * we may deprecate the current name to reflect this. */
  RegionView3D *rv3d = (RegionView3D *)(ptr->data);
  return RV3D_VIEW_IS_AXIS(rv3d->view);
}

static void rna_RegionView3D_is_orthographic_side_view_set(PointerRNA *ptr, bool value)
{
  RegionView3D *rv3d = (RegionView3D *)(ptr->data);
  const bool was_axis_view = RV3D_VIEW_IS_AXIS(rv3d->view);
  if (value) {
    /* Already axis aligned, nothing to do. */
    if (was_axis_view) {
      return;
    }
    /* Use a large value as we always want to set this to the closest axis. */
    const float eps_quat = FLT_MAX;
    ED_view3d_quat_to_axis_view_and_reset_quat(
        rv3d->viewquat, eps_quat, &rv3d->view, &rv3d->view_axis_roll);
  }
  else {
    /* Only allow changing from axis-views to user view as camera view for example
     * doesn't make sense to update. */
    if (!was_axis_view) {
      return;
    }
    rv3d->view = RV3D_VIEW_USER;
  }
}

static IDProperty **rna_View3DShading_idprops(PointerRNA *ptr)
{
  View3DShading *shading = static_cast<View3DShading *>(ptr->data);
  return &shading->prop;
}

static void rna_3DViewShading_type_update(Main *bmain, Scene *scene, PointerRNA *ptr)
{
  ID *id = ptr->owner_id;
  if (GS(id->name) != ID_SCR) {
    return;
  }

  View3DShading *shading = static_cast<View3DShading *>(ptr->data);
  if (shading->type == OB_MATERIAL ||
      (shading->type == OB_RENDER && !BKE_scene_uses_blender_workbench(scene)))
  {
    /* When switching from workbench to render or material mode the geometry of any
     * active sculpt session needs to be recalculated. */
    LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
      if (ob->sculpt) {
        DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      }
    }
  }

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
    return WM_windows_scene_get_from_screen(static_cast<wmWindowManager *>(G_MAIN->wm.first),
                                            screen);
  }
}

static ViewLayer *rna_3DViewShading_view_layer(PointerRNA *ptr)
{
  /* Get scene, depends if using 3D view or OpenGL render settings. */
  ID *id = ptr->owner_id;
  if (GS(id->name) == ID_SCE) {
    return nullptr;
  }
  else {
    bScreen *screen = (bScreen *)ptr->owner_id;
    return WM_windows_view_layer_get_from_screen(static_cast<wmWindowManager *>(G_MAIN->wm.first),
                                                 screen);
  }
}

static int rna_3DViewShading_type_get(PointerRNA *ptr)
{
  /* Available shading types depend on render engine. */
  Scene *scene = rna_3DViewShading_scene(ptr);
  RenderEngineType *type = (scene) ? RE_engines_find(scene->r.engine) : nullptr;
  View3DShading *shading = (View3DShading *)ptr->data;

  if (scene == nullptr || BKE_scene_uses_blender_eevee(scene)) {
    return shading->type;
  }
  else if (BKE_scene_uses_blender_workbench(scene)) {
    return (shading->type == OB_MATERIAL) ? int(OB_SOLID) : shading->type;
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

static const EnumPropertyItem *rna_3DViewShading_type_itemf(bContext * /*C*/,
                                                            PointerRNA *ptr,
                                                            PropertyRNA * /*prop*/,
                                                            bool *r_free)
{
  Scene *scene = rna_3DViewShading_scene(ptr);
  RenderEngineType *type = (scene) ? RE_engines_find(scene->r.engine) : nullptr;

  EnumPropertyItem *item = nullptr;
  int totitem = 0;

  RNA_enum_items_add_value(&item, &totitem, rna_enum_shading_type_items, OB_WIRE);
  RNA_enum_items_add_value(&item, &totitem, rna_enum_shading_type_items, OB_SOLID);

  if (scene == nullptr || BKE_scene_uses_blender_eevee(scene)) {
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
    sl = BKE_studiolight_find(shading->matcap, STUDIOLIGHT_TYPE_MATCAP);
  }
  else if (shading->type == OB_SOLID && shading->light == V3D_LIGHTING_STUDIO) {
    sl = BKE_studiolight_find(shading->studio_light, STUDIOLIGHT_TYPE_STUDIO);
  }
  else {
    /* OB_MATERIAL and OB_RENDER */
    sl = BKE_studiolight_find(shading->lookdev_light, STUDIOLIGHT_TYPE_WORLD);
  }
  return RNA_pointer_create_with_parent(*ptr, &RNA_StudioLight, sl);
}

/* shading.light */
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

static const EnumPropertyItem *rna_View3DShading_studio_light_itemf(bContext * /*C*/,
                                                                    PointerRNA *ptr,
                                                                    PropertyRNA * /*prop*/,
                                                                    bool *r_free)
{
  View3DShading *shading = (View3DShading *)ptr->data;
  EnumPropertyItem *item = nullptr;
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
                                                                   PointerRNA * /*ptr*/,
                                                                   PropertyRNA * /*prop*/,
                                                                   bool *r_free)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  const bool aov_available = BKE_view_layer_has_valid_aov(view_layer);
  const bool eevee_active = STREQ(scene->r.engine, "BLENDER_EEVEE");

  int totitem = 0;
  EnumPropertyItem *result = nullptr;
  EnumPropertyItem aov_template;
  for (int i = 0; rna_enum_view3dshading_render_pass_type_items[i].identifier != nullptr; i++) {
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
    else if (ELEM(item->value,
                  EEVEE_RENDER_PASS_CRYPTOMATTE_OBJECT,
                  EEVEE_RENDER_PASS_CRYPTOMATTE_ASSET,
                  EEVEE_RENDER_PASS_CRYPTOMATTE_MATERIAL) &&
             !eevee_active)
    {
    }
    else if (!aov_available && STREQ(item->name, "Shader AOV")) {
      /* Don't add Shader AOV submenu when there are no AOVs defined. */
    }
    else {
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
  eViewLayerEEVEEPassType result = eViewLayerEEVEEPassType(shading->render_pass);
  ViewLayer *view_layer = rna_3DViewShading_view_layer(ptr);

  if (result == EEVEE_RENDER_PASS_AOV) {
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
  ViewLayer *view_layer = rna_3DViewShading_view_layer(ptr);
  shading->aov_name[0] = 0;

  if ((value & EEVEE_RENDER_PASS_AOV) != 0) {
    if (!view_layer) {
      shading->render_pass = EEVEE_RENDER_PASS_COMBINED;
      return;
    }
    const int aov_index = value & ~EEVEE_RENDER_PASS_AOV;
    ViewLayerAOV *aov = static_cast<ViewLayerAOV *>(BLI_findlink(&view_layer->aovs, aov_index));
    if (!aov) {
      /* AOV not found, cannot select AOV. */
      shading->render_pass = EEVEE_RENDER_PASS_COMBINED;
      return;
    }

    shading->render_pass = EEVEE_RENDER_PASS_AOV;
    STRNCPY(shading->aov_name, aov->name);
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
    BKE_layer_collection_local_sync(scene, view_layer, v3d);
    DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  }
}

static const EnumPropertyItem *rna_SpaceView3D_stereo3d_camera_itemf(bContext *C,
                                                                     PointerRNA * /*ptr*/,
                                                                     PropertyRNA * /*prop*/,
                                                                     bool * /*r_free*/)
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
                                                     Scene * /*scene*/,
                                                     PointerRNA *ptr)
{
#  ifdef WITH_XR_OPENXR
  const wmWindowManager *wm = static_cast<wmWindowManager *>(main->wm.first);

  /* Handle mirror toggling while there is a session already. */
  if (WM_xr_session_exists(&wm->xr)) {
    const View3D *v3d = static_cast<const View3D *>(ptr->data);
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
  return rna_object_type_visibility_icon_get_common(v3d->object_type_exclude_viewport,
                                                    &v3d->object_type_exclude_select);
}

static std::optional<std::string> rna_View3DShading_path(const PointerRNA *ptr)
{
  if (GS(ptr->owner_id->name) == ID_SCE) {
    return "display.shading";
  }
  else if (GS(ptr->owner_id->name) == ID_SCR) {
    const bScreen *screen = reinterpret_cast<bScreen *>(ptr->owner_id);
    const View3DShading *shading = static_cast<View3DShading *>(ptr->data);
    int area_index;
    int space_index;
    LISTBASE_FOREACH_INDEX (ScrArea *, area, &screen->areabase, area_index) {
      LISTBASE_FOREACH_INDEX (SpaceLink *, sl, &area->spacedata, space_index) {
        if (sl->spacetype == SPACE_VIEW3D) {
          View3D *v3d = reinterpret_cast<View3D *>(sl);
          if (&v3d->shading == shading) {
            return fmt::format("areas[{}].spaces[{}].shading", area_index, space_index);
          }
        }
      }
    }
  }

  return "shading";
}

static PointerRNA rna_SpaceView3D_overlay_get(PointerRNA *ptr)
{
  return RNA_pointer_create_with_parent(*ptr, &RNA_View3DOverlay, ptr->data);
}

static std::optional<std::string> rna_View3DOverlay_path(const PointerRNA *ptr)
{
  std::optional<std::string> editor_path = BKE_screen_path_from_screen_to_space(ptr);
  return fmt::format("{}{}{}", editor_path.value_or(""), editor_path ? "." : "", "overlay");
}

/* Space Image Editor */

static PointerRNA rna_SpaceImage_overlay_get(PointerRNA *ptr)
{
  return RNA_pointer_create_with_parent(*ptr, &RNA_SpaceImageOverlay, ptr->data);
}

static std::optional<std::string> rna_SpaceImageOverlay_path(const PointerRNA *ptr)
{
  std::optional<std::string> editor_path = BKE_screen_path_from_screen_to_space(ptr);
  return fmt::format("{}{}{}", editor_path.value_or(""), editor_path ? "." : "", "overlay");
}

static std::optional<std::string> rna_SpaceUVEditor_path(const PointerRNA *ptr)
{
  std::optional<std::string> editor_path = BKE_screen_path_from_screen_to_space(ptr);
  return fmt::format("{}{}{}", editor_path.value_or(""), editor_path ? "." : "", "uv_editor");
}

static PointerRNA rna_SpaceImageEditor_uvedit_get(PointerRNA *ptr)
{
  return RNA_pointer_create_with_parent(*ptr, &RNA_SpaceUVEditor, ptr->data);
}

static void rna_SpaceImageEditor_mode_update(Main *bmain, Scene *scene, PointerRNA * /*ptr*/)
{
  if (scene != nullptr) {
    ED_space_image_paint_update(bmain, static_cast<wmWindowManager *>(bmain->wm.first), scene);
  }
}

static void rna_SpaceImageEditor_show_stereo_set(PointerRNA *ptr, bool value)
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

static void rna_SpaceImageEditor_show_stereo_update(Main * /*bmain*/,
                                                    Scene * /*scene*/,
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

static void rna_SpaceImageEditor_show_sequencer_scene_set(PointerRNA *ptr, bool value)
{
  SpaceImage *sima = ptr->data_as<SpaceImage>();

  if (value) {
    sima->iuser.flag |= IMA_SHOW_SEQUENCER_SCENE;
  }
  else {
    sima->iuser.flag &= ~IMA_SHOW_SEQUENCER_SCENE;
  }
}

static bool rna_SpaceImageEditor_show_sequencer_scene_get(PointerRNA *ptr)
{
  SpaceImage *sima = ptr->data_as<SpaceImage>();
  return (sima->iuser.flag & IMA_SHOW_SEQUENCER_SCENE) != 0;
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
  SpaceImage *sima = static_cast<SpaceImage *>(ptr->data);
  bScreen *screen = (bScreen *)ptr->owner_id;
  Object *obedit = nullptr;
  wmWindow *win = ED_screen_window_find(screen, static_cast<wmWindowManager *>(G_MAIN->wm.first));
  if (win != nullptr) {
    Scene *scene = WM_window_get_active_scene(win);
    ViewLayer *view_layer = WM_window_get_active_view_layer(win);
    BKE_view_layer_synced_ensure(scene, view_layer);
    obedit = BKE_view_layer_edit_object_get(view_layer);
  }
  return ED_space_image_show_uvedit(sima, obedit);
}

static bool rna_SpaceImageEditor_show_maskedit_get(PointerRNA *ptr)
{
  SpaceImage *sima = (SpaceImage *)(ptr->data);
  bScreen *screen = (bScreen *)ptr->owner_id;
  Object *obedit = nullptr;
  wmWindow *win = ED_screen_window_find(screen, static_cast<wmWindowManager *>(G_MAIN->wm.first));
  if (win != nullptr) {
    Scene *scene = WM_window_get_active_scene(win);
    ViewLayer *view_layer = WM_window_get_active_view_layer(win);
    BKE_view_layer_synced_ensure(scene, view_layer);
    obedit = BKE_view_layer_edit_object_get(view_layer);
  }
  return ED_space_image_check_show_maskedit(sima, obedit);
}

static void rna_SpaceImageEditor_image_set(PointerRNA *ptr,
                                           PointerRNA value,
                                           ReportList * /*reports*/)
{
  BLI_assert(BKE_id_is_in_global_main(static_cast<ID *>(value.data)));
  SpaceImage *sima = static_cast<SpaceImage *>(ptr->data);
  ED_space_image_set(G_MAIN, sima, (Image *)value.data, false);
}

static void rna_SpaceImageEditor_mask_set(PointerRNA *ptr,
                                          PointerRNA value,
                                          ReportList * /*reports*/)
{
  SpaceImage *sima = (SpaceImage *)(ptr->data);

  ED_space_image_set_mask(nullptr, sima, (Mask *)value.data);
}

static const EnumPropertyItem *rna_SpaceImageEditor_display_channels_itemf(bContext * /*C*/,
                                                                           PointerRNA *ptr,
                                                                           PropertyRNA * /*prop*/,
                                                                           bool *r_free)
{
  SpaceImage *sima = (SpaceImage *)ptr->data;
  EnumPropertyItem *item = nullptr;
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

  /* Find #ARegion. */
  area = rna_area_from_space(ptr); /* can be nullptr */
  region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
  if (region) {
    ED_space_image_get_zoom(sima, region, &values[0], &values[1]);
  }
}

static float rna_SpaceImageEditor_zoom_percentage_get(PointerRNA *ptr)
{
  SpaceImage *sima = (SpaceImage *)ptr->data;
  return sima->zoom * 100.0f;
}

static void rna_SpaceImageEditor_zoom_percentage_set(PointerRNA *ptr, const float value)
{
  SpaceImage *sima = (SpaceImage *)ptr->data;
  sima->zoom = value / 100.0f;
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

static void rna_SpaceImageEditor_image_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  SpaceImage *sima = (SpaceImage *)ptr->data;
  Image *ima = sima->image;

  /* make sure all the iuser settings are valid for the sima image */
  if (ima) {
    if (ima->rr) {
      if (BKE_image_multilayer_index(sima->image->rr, &sima->iuser) == nullptr) {
        BKE_image_init_imageuser(sima->image, &sima->iuser);
      }
    }
    else {
      BKE_image_multiview_index(ima, &sima->iuser);
    }
  }
}

static void rna_SpaceImageEditor_scopes_update(bContext *C, PointerRNA *ptr)
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

static const EnumPropertyItem *rna_SpaceImageEditor_pivot_itemf(bContext * /*C*/,
                                                                PointerRNA *ptr,
                                                                PropertyRNA * /*prop*/,
                                                                bool * /*r_free*/)
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
      {0, nullptr, 0, nullptr, nullptr},
  };

  SpaceImage *sima = (SpaceImage *)ptr->data;

  if (sima->mode == SI_MODE_PAINT) {
    return rna_enum_transform_pivot_full_items;
  }
  else {
    return pivot_items;
  }
}

static void rna_SpaceUVEditor_tile_grid_shape_set(PointerRNA *ptr, const int *values)
{
  SpaceImage *data = (SpaceImage *)(ptr->data);

  int clamp[2] = {10, 100};
  for (int i = 0; i < 2; i++) {
    data->tile_grid_shape[i] = std::clamp(values[i], 1, clamp[i]);
  }
}

static void rna_SpaceUVEditor_custom_grid_subdiv_set(PointerRNA *ptr, const int *values)
{
  SpaceImage *data = (SpaceImage *)(ptr->data);

  for (int i = 0; i < 2; i++) {
    data->custom_grid_subdiv[i] = std::clamp(values[i], 1, 5000);
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
                                         ReportList * /*reports*/)
{
  SpaceText *st = (SpaceText *)(ptr->data);

  st->text = static_cast<Text *>(value.data);
  if (st->text != nullptr) {
    id_us_ensure_real((ID *)st->text);
  }

  ScrArea *area = rna_area_from_space(ptr);
  if (area) {
    ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
    if (region) {
      ED_space_text_scroll_to_cursor(st, region, true);
    }
  }
}

static bool rna_SpaceTextEditor_text_is_syntax_highlight_supported(SpaceText *space)
{
  return ED_text_is_syntax_highlight_supported(space->text);
}

static void rna_SpaceTextEditor_updateEdited(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  SpaceText *st = (SpaceText *)ptr->data;

  if (st->text) {
    WM_main_add_notifier(NC_TEXT | NA_EDITED, st->text);
  }
}

static int rna_SpaceTextEditor_visible_lines_get(PointerRNA *ptr)
{
  const SpaceText *st = static_cast<SpaceText *>(ptr->data);
  return ED_space_text_visible_lines_get(st);
}

/* Space Properties */

static StructRNA *rna_SpaceProperties_pin_id_typef(PointerRNA *ptr)
{
  SpaceProperties *sbuts = (SpaceProperties *)(ptr->data);

  if (sbuts->pinid) {
    return ID_code_to_RNA_type(GS(sbuts->pinid->name));
  }

  return &RNA_ID;
}

static void rna_SpaceProperties_pin_id_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  SpaceProperties *sbuts = (SpaceProperties *)(ptr->data);
  ID *id = sbuts->pinid;

  if (id == nullptr) {
    sbuts->flag &= ~SB_PIN_CONTEXT;
    return;
  }

  switch (GS(id->name)) {
    case ID_MA:
      WM_main_add_notifier(NC_MATERIAL | ND_SHADING, nullptr);
      break;
    case ID_TE:
      WM_main_add_notifier(NC_TEXTURE, nullptr);
      break;
    case ID_WO:
      WM_main_add_notifier(NC_WORLD, nullptr);
      break;
    case ID_LA:
      WM_main_add_notifier(NC_LAMP, nullptr);
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

static const EnumPropertyItem *rna_SpaceProperties_context_itemf(bContext * /*C*/,
                                                                 PointerRNA *ptr,
                                                                 PropertyRNA * /*prop*/,
                                                                 bool *r_free)
{
  SpaceProperties *sbuts = (SpaceProperties *)(ptr->data);
  EnumPropertyItem *item = nullptr;

  /* Although it would never reach this amount, a theoretical maximum number of tabs
   * is BCONTEXT_TOT * 2, with every tab displayed and a spacer in every other item. */
  const blender::Vector<eSpaceButtons_Context> context_tabs_array = ED_buttons_tabs_list(sbuts);

  int totitem_added = 0;
  bool add_separator = true;
  for (const eSpaceButtons_Context tab : context_tabs_array) {
    if (tab == -1) {
      if (add_separator) {
        RNA_enum_item_add_separator(&item, &totitem_added);
        add_separator = false;
      }
      continue;
    }

    RNA_enum_items_add_value(&item, &totitem_added, buttons_context_items, tab);
    add_separator = true;

    /* Add the object data icon dynamically for the data tab. */
    if (tab == BCONTEXT_DATA) {
      (item + totitem_added - 1)->icon = sbuts->dataicon;
    }
  }

  RNA_enum_item_end(&item, &totitem_added);
  *r_free = true;

  return item;
}

static void rna_SpaceProperties_context_update(Main * /*bmain*/,
                                               Scene * /*scene*/,
                                               PointerRNA *ptr)
{
  SpaceProperties *sbuts = (SpaceProperties *)(ptr->data);
  /* XXX BCONTEXT_DATA is ugly, but required for lights... See #51318. */
  if (ELEM(sbuts->mainb, BCONTEXT_WORLD, BCONTEXT_MATERIAL, BCONTEXT_TEXTURE, BCONTEXT_DATA)) {
    sbuts->preview = 1;
  }
}

static int rna_SpaceProperties_tab_search_results_getlength(const PointerRNA *ptr,
                                                            int length[RNA_MAX_ARRAY_DIMENSION])
{
  SpaceProperties *sbuts = static_cast<SpaceProperties *>(ptr->data);

  const blender::Vector<eSpaceButtons_Context> context_tabs_array = ED_buttons_tabs_list(sbuts);

  length[0] = context_tabs_array.size();

  return length[0];
}

static void rna_SpaceProperties_tab_search_results_get(PointerRNA *ptr, bool *values)
{
  SpaceProperties *sbuts = static_cast<SpaceProperties *>(ptr->data);

  const blender::Vector<eSpaceButtons_Context> context_tabs_array = ED_buttons_tabs_list(sbuts);

  for (const int i : context_tabs_array.index_range()) {
    values[i] = ED_buttons_tab_has_search_result(sbuts, i);
  }
}

static void rna_SpaceProperties_search_filter_get(PointerRNA *ptr, char *value)
{
  SpaceProperties *sbuts = static_cast<SpaceProperties *>(ptr->data);
  const char *search_filter = ED_buttons_search_string_get(sbuts);

  strcpy(value, search_filter);
}

static int rna_SpaceProperties_search_filter_length(PointerRNA *ptr)
{
  SpaceProperties *sbuts = static_cast<SpaceProperties *>(ptr->data);

  return ED_buttons_search_string_length(sbuts);
}

static void rna_SpaceProperties_search_filter_set(PointerRNA *ptr, const char *value)
{
  SpaceProperties *sbuts = static_cast<SpaceProperties *>(ptr->data);

  ED_buttons_search_string_set(sbuts, value);
}

static void rna_SpaceProperties_search_filter_update(Main * /*bmain*/,
                                                     Scene * /*scene*/,
                                                     PointerRNA *ptr)
{
  ScrArea *area = rna_area_from_space(ptr);

  /* Update the search filter flag for the main region with the panels. */
  ARegion *main_region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
  BLI_assert(main_region != nullptr);
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
  size_t len = strlen(value);

  if ((len >= size_t(ci->len_alloc)) || (len * 2 < size_t(ci->len_alloc)))
  { /* allocate a new string */
    MEM_freeN(ci->line);
    ci->line = MEM_malloc_arrayN<char>(len + 1, "rna_consoleline");
    ci->len_alloc = int(len + 1);
  }
  memcpy(ci->line, value, len + 1);
  ci->len = int(len);

  if (size_t(ci->cursor) > len) {
    /* clamp the cursor */
    ci->cursor = int(len);
  }
}

static int rna_ConsoleLine_current_character_get(PointerRNA *ptr)
{
  const ConsoleLine *ci = (ConsoleLine *)ptr->data;
  return BLI_str_utf8_offset_to_index(ci->line, ci->len, ci->cursor);
}

static void rna_ConsoleLine_current_character_set(PointerRNA *ptr, const int index)
{
  ConsoleLine *ci = (ConsoleLine *)ptr->data;
  ci->cursor = BLI_str_utf8_offset_from_index(ci->line, ci->len, index);
}

/* Space Dope-sheet */

static void rna_SpaceDopeSheetEditor_mode_update(bContext *C, PointerRNA *ptr)
{
  SpaceAction *saction = (SpaceAction *)(ptr->data);
  ScrArea *area = CTX_wm_area(C);

  if (area && area->spacedata.first == saction) {
    ARegion *channels_region = BKE_area_find_region_type(area, RGN_TYPE_CHANNELS);
    if (channels_region) {
      channels_region->flag &= ~RGN_FLAG_HIDDEN;
      ED_region_visibility_change_update(C, area, channels_region);
    }
  }

  /* recalculate extents of channel list */
  saction->runtime.flag |= SACTION_RUNTIME_FLAG_NEED_CHAN_SYNC;

  /* store current mode as "old mode",
   * so that returning from other editors doesn't always reset to "Action Editor" */
  saction->mode_prev = saction->mode;
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

static void rna_SpaceGraphEditor_normalize_update(bContext *C, PointerRNA * /*ptr*/)
{
  bAnimContext ac;

  if (ANIM_animdata_get_context(C, &ac) == 0) {
    return;
  }

  ANIM_frame_channel_y_extents(C, &ac);
  ED_area_tag_refresh(ac.area);
}

static bool rna_SpaceGraphEditor_has_ghost_curves_get(PointerRNA *ptr)
{
  SpaceGraph *sipo = (SpaceGraph *)(ptr->data);
  return (BLI_listbase_is_empty(&sipo->runtime.ghost_curves) == false);
}

static void rna_SpaceConsole_rect_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  SpaceConsole *sc = static_cast<SpaceConsole *>(ptr->data);
  WM_main_add_notifier(NC_SPACE | ND_SPACE_CONSOLE | NA_EDITED, sc);
}

static void rna_SequenceEditor_update_cache(Main * /*bmain*/, Scene *scene, PointerRNA * /*ptr*/)
{
  blender::seq::cache_cleanup(scene, blender::seq::CacheCleanup::FinalAndIntra);
}

static void seq_build_proxy(bContext *C, PointerRNA *ptr)
{
  if (U.sequencer_proxy_setup != USER_SEQ_PROXY_SETUP_AUTOMATIC) {
    return;
  }

  SpaceSeq *sseq = static_cast<SpaceSeq *>(ptr->data);
  Scene *scene = CTX_data_sequencer_scene(C);
  ListBase *seqbase = blender::seq::active_seqbase_get(blender::seq::editing_get(scene));

  blender::Set<std::string> processed_paths;
  wmJob *wm_job = blender::seq::ED_seq_proxy_wm_job_get(C);
  blender::seq::ProxyJob *pj = blender::seq::ED_seq_proxy_job_get(C, wm_job);

  LISTBASE_FOREACH (Strip *, strip, seqbase) {
    if (strip->type != STRIP_TYPE_MOVIE || strip->data == nullptr || strip->data->proxy == nullptr)
    {
      continue;
    }

    /* Add new proxy size. */
    strip->data->proxy->build_size_flags |= blender::seq::rendersize_to_proxysize(
        eSpaceSeq_Proxy_RenderSize(sseq->render_size));

    /* Build proxy. */
    blender::seq::proxy_rebuild_context(
        pj->main, pj->depsgraph, pj->scene, strip, &processed_paths, &pj->queue, true);
  }

  if (!WM_jobs_is_running(wm_job)) {
    G.is_break = false;
    WM_jobs_start(CTX_wm_manager(C), wm_job);
  }

  ED_area_tag_redraw(CTX_wm_area(C));
}

static void rna_SequenceEditor_render_size_update(bContext *C, PointerRNA *ptr)
{
  seq_build_proxy(C, ptr);
  rna_SequenceEditor_update_cache(CTX_data_main(C), CTX_data_sequencer_scene(C), ptr);
}

static bool rna_SequenceEditor_clamp_view_get(PointerRNA *ptr)
{
  SpaceSeq *sseq = static_cast<SpaceSeq *>(ptr->data);
  return (sseq->flag & SEQ_CLAMP_VIEW) != 0;
}

static void rna_SequenceEditor_clamp_view_set(PointerRNA *ptr, bool value)
{
  SpaceSeq *sseq = static_cast<SpaceSeq *>(ptr->data);
  ScrArea *area;
  ARegion *region;

  area = rna_area_from_space(ptr); /* can be nullptr */
  if (area == nullptr) {
    return;
  }

  region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
  if (region) {
    if (value) {
      sseq->flag |= SEQ_CLAMP_VIEW;
      region->v2d.align &= ~V2D_ALIGN_NO_NEG_Y;
    }
    else {
      sseq->flag &= ~SEQ_CLAMP_VIEW;
      region->v2d.align |= V2D_ALIGN_NO_NEG_Y;
    }
  }
}

static void rna_Sequencer_view_type_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  ScrArea *area = rna_area_from_space(ptr);
  ED_area_tag_refresh(area);
}

static PointerRNA rna_SpaceSequenceEditor_preview_overlay_get(PointerRNA *ptr)
{
  return RNA_pointer_create_with_parent(*ptr, &RNA_SequencerPreviewOverlay, ptr->data);
}

static std::optional<std::string> rna_SpaceSequencerPreviewOverlay_path(const PointerRNA *ptr)
{
  std::optional<std::string> editor_path = BKE_screen_path_from_screen_to_space(ptr);
  return fmt::format(
      "{}{}{}", editor_path.value_or(""), editor_path ? "." : "", "preview_overlay");
}

static PointerRNA rna_SpaceSequenceEditor_timeline_overlay_get(PointerRNA *ptr)
{
  return RNA_pointer_create_with_parent(*ptr, &RNA_SequencerTimelineOverlay, ptr->data);
}

static std::optional<std::string> rna_SpaceSequencerTimelineOverlay_path(const PointerRNA *ptr)
{
  std::optional<std::string> editor_path = BKE_screen_path_from_screen_to_space(ptr);
  return fmt::format(
      "{}{}{}", editor_path.value_or(""), editor_path ? "." : "", "timeline_overlay");
}

static PointerRNA rna_SpaceSequenceEditor_cache_overlay_get(PointerRNA *ptr)
{
  return RNA_pointer_create_with_parent(*ptr, &RNA_SequencerCacheOverlay, ptr->data);
}

static std::optional<std::string> rna_SpaceSequencerCacheOverlay_path(const PointerRNA *ptr)
{
  std::optional<std::string> editor_path = BKE_screen_path_from_screen_to_space(ptr);
  return fmt::format("{}{}{}", editor_path.value_or(""), editor_path ? "." : "", "cache_overlay");
}

static float rna_SpaceSequenceEditor_zoom_percentage_get(PointerRNA *ptr)
{
  ScrArea *area = rna_area_from_space(ptr);
  if (area == nullptr) {
    return 100.0f;
  }
  ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_PREVIEW);
  if (region == nullptr) {
    return 100.0f;
  }

  View2D *v2d = &region->v2d;
  const float zoom = 1.0f / (BLI_rctf_size_x(&v2d->cur) / float(BLI_rcti_size_x(&v2d->mask))) *
                     +100.0f;
  return zoom;
}

static void rna_SpaceSequenceEditor_zoom_percentage_set(PointerRNA *ptr, const float value)
{
  ScrArea *area = rna_area_from_space(ptr);
  if (area == nullptr) {
    return;
  }
  ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_PREVIEW);
  if (region == nullptr) {
    return;
  }

  View2D *v2d = &region->v2d;
  BLI_rctf_resize(&v2d->cur,
                  float(BLI_rcti_size_x(&v2d->mask)) / (value / 100.0f),
                  float(BLI_rcti_size_y(&v2d->mask)) / (value / 100.0f));
  ED_region_tag_redraw(region);
}

static PointerRNA rna_SpaceDopeSheet_overlay_get(PointerRNA *ptr)
{
  return RNA_pointer_create_with_parent(*ptr, &RNA_SpaceDopeSheetOverlay, ptr->data);
}

static std::optional<std::string> rna_SpaceDopeSheetOverlay_path(const PointerRNA *ptr)
{
  std::optional<std::string> editor_path = BKE_screen_path_from_screen_to_space(ptr);
  if (!editor_path) {
    return std::nullopt;
  }
  return editor_path.value() + ".overlays";
}

/* Space Node Editor */
static PointerRNA rna_SpaceNode_overlay_get(PointerRNA *ptr)
{
  return RNA_pointer_create_with_parent(*ptr, &RNA_SpaceNodeOverlay, ptr->data);
}

static bool rna_SpaceNode_supports_previews(PointerRNA *ptr)
{
  return ED_node_supports_preview(static_cast<SpaceNode *>(ptr->data));
}

static std::optional<std::string> rna_SpaceNodeOverlay_path(const PointerRNA *ptr)
{
  std::optional<std::string> editor_path = BKE_screen_path_from_screen_to_space(ptr);
  return fmt::format("{}{}{}", editor_path.value_or(""), editor_path ? "." : "", "overlay");
}

static void rna_SpaceNodeEditor_node_tree_set(PointerRNA *ptr,
                                              const PointerRNA value,
                                              ReportList * /*reports*/)
{
  SpaceNode *snode = ptr->data_as<SpaceNode>();
  ScrArea *area = BKE_screen_find_area_from_space(reinterpret_cast<const bScreen *>(ptr->owner_id),
                                                  reinterpret_cast<const SpaceLink *>(snode));
  ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW);
  ED_node_tree_start(region, snode, (bNodeTree *)value.data, nullptr, nullptr);
}

static bool rna_SpaceNodeEditor_selected_node_group_poll(PointerRNA *space_node_pointer,
                                                         const PointerRNA value)
{
  SpaceNode *space_node = space_node_pointer->data_as<SpaceNode>();
  const bNodeTree &ntree = *static_cast<const bNodeTree *>(value.data);
  if (ED_node_is_compositor(space_node)) {
    return ntree.type == NTREE_COMPOSIT;
  }

  if (ntree.type != NTREE_GEOMETRY) {
    return false;
  }
  if (!ntree.geometry_node_asset_traits) {
    return false;
  }
  if ((ntree.geometry_node_asset_traits->flag & GEO_NODE_ASSET_TOOL) == 0) {
    return false;
  }
  return true;
}

static bool space_node_node_geometry_nodes_poll(const SpaceNode &snode, const bNodeTree &ntree)
{
  switch (SpaceNodeGeometryNodesType(snode.node_tree_sub_type)) {
    case SNODE_GEOMETRY_MODIFIER:
      if (!ntree.geometry_node_asset_traits) {
        return false;
      }
      if ((ntree.geometry_node_asset_traits->flag & GEO_NODE_ASSET_MODIFIER) == 0) {
        return false;
      }
      return true;
    case SNODE_GEOMETRY_TOOL:
      if (!ntree.geometry_node_asset_traits) {
        return false;
      }
      if ((ntree.geometry_node_asset_traits->flag & GEO_NODE_ASSET_TOOL) == 0) {
        return false;
      }
      return true;
  }
  return false;
}

static bool rna_SpaceNodeEditor_node_tree_poll(PointerRNA *ptr, const PointerRNA value)
{
  SpaceNode *snode = (SpaceNode *)ptr->data;
  bNodeTree *ntree = (bNodeTree *)value.data;

  /* node tree type must match the selected type in node editor */
  if (!STREQ(snode->tree_idname, ntree->idname)) {
    return false;
  }
  if (ntree->type == NTREE_GEOMETRY) {
    if (!space_node_node_geometry_nodes_poll(*snode, *ntree)) {
      return false;
    }
  }
  return true;
}

static void rna_SpaceNodeEditor_node_tree_update(const bContext *C, PointerRNA * /*ptr*/)
{
  blender::ed::space_node::tree_update(C);
}

static const EnumPropertyItem *rna_SpaceNodeEditor_node_tree_sub_type_itemf(
    bContext * /*context*/,
    PointerRNA *space_node_pointer,
    PropertyRNA * /*property*/,
    bool * /*r_free*/)
{
  static const EnumPropertyItem geometry_nodes_sub_type_items[] = {
      {SNODE_GEOMETRY_MODIFIER,
       "MODIFIER",
       ICON_MODIFIER_DATA,
       N_("Modifier"),
       N_("Edit node group from active object's active modifier")},
      {SNODE_GEOMETRY_TOOL,
       "TOOL",
       ICON_TOOL_SETTINGS,
       N_("Tool"),
       N_("Edit any geometry node group for use as an operator")},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem compositor_sub_type_items[] = {
      {SNODE_COMPOSITOR_SCENE,
       "SCENE",
       ICON_SCENE_DATA,
       N_("Scene"),
       N_("Edit compositing node group for the current scene")},
      {SNODE_COMPOSITOR_SEQUENCER,
       "SEQUENCER",
       ICON_SEQUENCE,
       N_("Sequencer"),
       N_("Edit compositing node group for Sequencer strip modifiers")},
      {0, nullptr, 0, nullptr, nullptr},
  };

  SpaceNode *space_node = space_node_pointer->data_as<SpaceNode>();
  if (ED_node_is_geometry(space_node)) {
    return geometry_nodes_sub_type_items;
  }
  else {
    return compositor_sub_type_items;
  }
}

static void rna_SpaceNodeEditor_node_tree_sub_type_update(Main * /*main*/,
                                                          Scene * /*scene*/,
                                                          PointerRNA *space_node_pointer)
{
  SpaceNode *space_node = space_node_pointer->data_as<SpaceNode>();
  if (ED_node_is_geometry(space_node)) {
    if (space_node->node_tree_sub_type == SNODE_GEOMETRY_TOOL) {
      space_node->flag &= ~SNODE_PIN;
    }
  }
  else {
    if (space_node->node_tree_sub_type == SNODE_COMPOSITOR_SEQUENCER) {
      space_node->flag &= ~SNODE_PIN;
    }
  }
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
static bool rna_SpaceNodeEditor_tree_type_poll(void *Cv, blender::bke::bNodeTreeType *type)
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
  return rna_node_tree_type_itemf(C, C ? rna_SpaceNodeEditor_tree_type_poll : nullptr, r_free);
}

static const EnumPropertyItem *rna_SpaceNodeEditor_tree_type_itemf(bContext *C,
                                                                   PointerRNA * /*ptr*/,
                                                                   PropertyRNA * /*prop*/,
                                                                   bool *r_free)
{
  return RNA_enum_node_tree_types_itemf_impl(C, r_free);
}

static void rna_SpaceNodeEditor_path_get(PointerRNA *ptr, char *value)
{
  SpaceNode *snode = static_cast<SpaceNode *>(ptr->data);
  ED_node_tree_path_get(snode, value);
}

static int rna_SpaceNodeEditor_path_length(PointerRNA *ptr)
{
  SpaceNode *snode = static_cast<SpaceNode *>(ptr->data);
  return ED_node_tree_path_length(snode);
}

static void rna_SpaceNodeEditor_path_clear(SpaceNode *snode, bContext *C)
{
  ED_node_tree_start(nullptr, snode, nullptr, nullptr, nullptr);
  blender::ed::space_node::tree_update(C);
}

static ARegion *find_snode_region(SpaceNode *snode, bContext *C)
{
  if (wmWindowManager *wm = CTX_wm_manager(C)) {
    LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
      bScreen *screen = WM_window_get_active_screen(win);
      ScrArea *area = BKE_screen_find_area_from_space(screen,
                                                      reinterpret_cast<const SpaceLink *>(snode));
      if (ARegion *region = BKE_area_find_region_type(area, RGN_TYPE_WINDOW)) {
        return region;
      }
    }
  }
  return nullptr;
}

static void rna_SpaceNodeEditor_path_start(SpaceNode *snode, bContext *C, PointerRNA *node_tree)
{
  ARegion *region = find_snode_region(snode, C);
  ED_node_tree_start(region, snode, (bNodeTree *)node_tree->data, nullptr, nullptr);
  blender::ed::space_node::tree_update(C);
}

static void rna_SpaceNodeEditor_path_append(SpaceNode *snode,
                                            bContext *C,
                                            PointerRNA *node_tree,
                                            PointerRNA *node)
{
  ARegion *region = find_snode_region(snode, C);
  ED_node_tree_push(
      region, snode, static_cast<bNodeTree *>(node_tree->data), static_cast<bNode *>(node->data));
  blender::ed::space_node::tree_update(C);
}

static void rna_SpaceNodeEditor_path_pop(SpaceNode *snode, bContext *C)
{
  ARegion *region = find_snode_region(snode, C);
  ED_node_tree_pop(region, snode);
  blender::ed::space_node::tree_update(C);
}

static void rna_SpaceNodeEditor_show_backdrop_update(Main * /*bmain*/,
                                                     Scene * /*scene*/,
                                                     PointerRNA * /*ptr*/)
{
  WM_main_add_notifier(NC_NODE | NA_EDITED, nullptr);
  WM_main_add_notifier(NC_SCENE | ND_NODES, nullptr);
}

static void rna_SpaceNodeEditor_cursor_location_from_region(SpaceNode *snode,
                                                            bContext *C,
                                                            int x,
                                                            int y)
{
  ARegion *region = CTX_wm_region(C);

  float cursor_location[2];

  UI_view2d_region_to_view(&region->v2d, x, y, &cursor_location[0], &cursor_location[1]);
  cursor_location[0] /= UI_SCALE_FAC;
  cursor_location[1] /= UI_SCALE_FAC;

  ED_node_cursor_location_set(snode, cursor_location);
}

static void rna_SpaceClipEditor_clip_set(PointerRNA *ptr,
                                         PointerRNA value,
                                         ReportList * /*reports*/)
{
  SpaceClip *sc = (SpaceClip *)(ptr->data);
  bScreen *screen = (bScreen *)ptr->owner_id;

  ED_space_clip_set_clip(nullptr, screen, sc, (MovieClip *)value.data);
}

static void rna_SpaceClipEditor_mask_set(PointerRNA *ptr,
                                         PointerRNA value,
                                         ReportList * /*reports*/)
{
  SpaceClip *sc = (SpaceClip *)(ptr->data);

  ED_space_clip_set_mask(nullptr, sc, (Mask *)value.data);
}

static void rna_SpaceClipEditor_clip_mode_update(Main * /*bmain*/,
                                                 Scene * /*scene*/,
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

static void rna_SpaceClipEditor_lock_selection_update(Main * /*bmain*/,
                                                      Scene * /*scene*/,
                                                      PointerRNA *ptr)
{
  SpaceClip *sc = (SpaceClip *)(ptr->data);

  sc->xlockof = 0.0f;
  sc->ylockof = 0.0f;
}

static void rna_SpaceClipEditor_view_type_update(Main * /*bmain*/,
                                                 Scene * /*scene*/,
                                                 PointerRNA *ptr)
{
  ScrArea *area = rna_area_from_space(ptr);
  ED_area_tag_refresh(area);
}

static float rna_SpaceClipEditor_zoom_percentage_get(PointerRNA *ptr)
{
  SpaceClip *sc = (SpaceClip *)ptr->data;
  return sc->zoom * 100.0f;
}

static void rna_SpaceClipEditor_zoom_percentage_set(PointerRNA *ptr, const float value)
{
  SpaceClip *sc = (SpaceClip *)ptr->data;
  sc->zoom = value / 100.0f;
}

static PointerRNA rna_SpaceClip_overlay_get(PointerRNA *ptr)
{
  return RNA_pointer_create_with_parent(*ptr, &RNA_SpaceClipOverlay, ptr->data);
}

static std::optional<std::string> rna_SpaceClipOverlay_path(const PointerRNA *ptr)
{
  std::optional<std::string> editor_path = BKE_screen_path_from_screen_to_space(ptr);
  return fmt::format("{}{}{}", editor_path.value_or(""), editor_path ? "." : "", "overlay");
}

/* File browser. */

static std::optional<std::string> rna_FileSelectParams_path(const PointerRNA * /*ptr*/)
{
  return "params";
}

int rna_FileSelectParams_filename_editable(const PointerRNA *ptr, const char **r_info)
{
  FileSelectParams *params = static_cast<FileSelectParams *>(ptr->data);

  if (params && (params->flag & FILE_DIRSEL_ONLY)) {
    *r_info = N_("Only directories can be chosen for the current operation.");
    return 0;
  }

  return params ? int(PROP_EDITABLE) : 0;
}

static bool rna_FileSelectParams_use_lib_get(PointerRNA *ptr)
{
  FileSelectParams *params = static_cast<FileSelectParams *>(ptr->data);

  return params && (params->type == FILE_LOADLIB);
}

static const EnumPropertyItem *rna_FileSelectParams_display_type_itemf(bContext * /*C*/,
                                                                       PointerRNA *ptr,
                                                                       PropertyRNA * /*prop*/,
                                                                       bool *r_free)
{
  if (RNA_struct_is_a(ptr->type, &RNA_FileAssetSelectParams)) {
    EnumPropertyItem *items = nullptr;
    int totitem = 0;

    /* Only expose preview and column view for asset browsing. */
    RNA_enum_items_add_value(
        &items, &totitem, fileselectparams_display_type_items, FILE_HORIZONTALDISPLAY);
    RNA_enum_items_add_value(
        &items, &totitem, fileselectparams_display_type_items, FILE_IMGDISPLAY);

    RNA_enum_item_end(&items, &totitem);
    *r_free = true;

    return items;
  }

  *r_free = false;
  return fileselectparams_display_type_items;
}

static int rna_FileSelectParams_display_type_default(PointerRNA *ptr, PropertyRNA *prop)
{
  if (RNA_struct_is_a(ptr->type, &RNA_FileAssetSelectParams)) {
    return FILE_IMGDISPLAY;
  }

  EnumPropertyRNA *eprop = reinterpret_cast<EnumPropertyRNA *>(prop);
  return eprop->defaultvalue;
}

static const EnumPropertyItem *rna_FileSelectParams_recursion_level_itemf(bContext * /*C*/,
                                                                          PointerRNA *ptr,
                                                                          PropertyRNA * /*prop*/,
                                                                          bool *r_free)
{
  FileSelectParams *params = static_cast<FileSelectParams *>(ptr->data);

  if (params && params->type != FILE_LOADLIB) {
    EnumPropertyItem *item = nullptr;
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

static const EnumPropertyItem *rna_FileSelectParams_sort_method_itemf(bContext * /*C*/,
                                                                      PointerRNA *ptr,
                                                                      PropertyRNA * /*prop*/,
                                                                      bool *r_free)
{
  EnumPropertyItem *items = nullptr;
  int totitem = 0;

  if (RNA_struct_is_a(ptr->type, &RNA_FileAssetSelectParams)) {
    /* Only expose sorting by name and asset catalog for asset browsing. */

    RNA_enum_items_add_value(
        &items, &totitem, rna_enum_fileselect_params_sort_items, FILE_SORT_ALPHA);
    /* Address small annoyance: Tooltip talks about "file list", override to be "asset list"
     * instead. */
    items[0].description = N_("Sort the asset list alphabetically");

    RNA_enum_items_add_value(
        &items, &totitem, rna_enum_fileselect_params_sort_items, FILE_SORT_ASSET_CATALOG);
  }
  else {
    /* Remove asset catalog from the items. */
    for (const EnumPropertyItem *item = rna_enum_fileselect_params_sort_items; item->identifier;
         item++)
    {
      if (item->value != FILE_SORT_ASSET_CATALOG) {
        RNA_enum_item_add(&items, &totitem, item);
      }
    }
  }

  RNA_enum_item_end(&items, &totitem);

  *r_free = true;
  return items;
}

static void rna_FileSelectPrams_filter_glob_set(PointerRNA *ptr, const char *value)
{
  FileSelectParams *params = static_cast<FileSelectParams *>(ptr->data);

  STRNCPY(params->filter_glob, value);

  /* Remove stupid things like last group being a wildcard-only one. */
  BLI_path_extension_glob_validate(params->filter_glob);
}

static PointerRNA rna_FileSelectParams_filter_id_get(PointerRNA *ptr)
{
  return RNA_pointer_create_with_parent(*ptr, &RNA_FileSelectIDFilter, ptr->data);
}

static int rna_FileAssetSelectParams_asset_library_get(PointerRNA *ptr)
{
  FileAssetSelectParams *params = static_cast<FileAssetSelectParams *>(ptr->data);
  /* Just an extra sanity check to ensure this isn't somehow called for RNA_FileSelectParams. */
  BLI_assert(ptr->type == &RNA_FileAssetSelectParams);

  return blender::ed::asset::library_reference_to_enum_value(&params->asset_library_ref);
}

static void rna_FileAssetSelectParams_asset_library_set(PointerRNA *ptr, int value)
{
  FileAssetSelectParams *params = static_cast<FileAssetSelectParams *>(ptr->data);
  params->asset_library_ref = blender::ed::asset::library_reference_from_enum_value(value);
}

static PointerRNA rna_FileAssetSelectParams_filter_id_get(PointerRNA *ptr)
{
  return RNA_pointer_create_with_parent(*ptr, &RNA_FileAssetSelectIDFilter, ptr->data);
}

static PointerRNA rna_FileBrowser_FileSelectEntry_asset_data_get_impl(const PointerRNA *ptr)
{
  const FileDirEntry *entry = static_cast<const FileDirEntry *>(ptr->data);

  if (!entry->asset) {
    return PointerRNA_NULL;
  }

  AssetMetaData *asset_data = &entry->asset->get_metadata();

  /* Note that the owning ID of the RNA pointer (`ptr->owner_id`) has to be set carefully:
   * Local IDs (`entry->id`) own their asset metadata themselves. Asset metadata from other blend
   * files are owned by the file browser (`entry`). Only if this is set correctly, we can tell from
   * the metadata RNA pointer if the metadata is stored locally and can thus be edited or not. */

  if (entry->asset->is_local_id()) {
    PointerRNA id_ptr = RNA_id_pointer_create(entry->id);
    return RNA_pointer_create_with_parent(id_ptr, &RNA_AssetMetaData, asset_data);
  }

  return RNA_pointer_create_with_parent(*ptr, &RNA_AssetMetaData, asset_data);
}

static int rna_FileBrowser_FileSelectEntry_name_editable(const PointerRNA *ptr,
                                                         const char **r_info)
{
  const FileDirEntry *entry = static_cast<const FileDirEntry *>(ptr->data);

  /* This actually always returns 0 (the name is never editable) but we want to get a disabled
   * message returned to `r_info` in some cases. */

  if (entry->asset) {
    PointerRNA asset_data_ptr = rna_FileBrowser_FileSelectEntry_asset_data_get_impl(ptr);
    /* Get disabled hint from asset metadata polling. */
    rna_AssetMetaData_editable(&asset_data_ptr, r_info);
  }

  return 0;
}

static PointerRNA rna_FileBrowser_FileSelectEntry_asset_data_get(PointerRNA *ptr)
{
  return rna_FileBrowser_FileSelectEntry_asset_data_get_impl(ptr);
}

static void rna_FileBrowser_FileSelectEntry_name_get(PointerRNA *ptr, char *value)
{
  const FileDirEntry *entry = static_cast<const FileDirEntry *>(ptr->data);
  strcpy(value, entry->name);
}

static int rna_FileBrowser_FileSelectEntry_name_length(PointerRNA *ptr)
{
  const FileDirEntry *entry = static_cast<const FileDirEntry *>(ptr->data);
  return int(strlen(entry->name));
}

static void rna_FileBrowser_FileSelectEntry_relative_path_get(PointerRNA *ptr, char *value)
{
  const FileDirEntry *entry = static_cast<const FileDirEntry *>(ptr->data);
  strcpy(value, entry->relpath);
}

static int rna_FileBrowser_FileSelectEntry_relative_path_length(PointerRNA *ptr)
{
  const FileDirEntry *entry = static_cast<const FileDirEntry *>(ptr->data);
  return int(strlen(entry->relpath));
}

static int rna_FileBrowser_FileSelectEntry_preview_icon_id_get(PointerRNA *ptr)
{
  const FileDirEntry *entry = static_cast<const FileDirEntry *>(ptr->data);
  return ED_file_icon(entry);
}

static StructRNA *rna_FileBrowser_params_typef(PointerRNA *ptr)
{
  SpaceFile *sfile = static_cast<SpaceFile *>(ptr->data);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);

  if (params == ED_fileselect_get_file_params(sfile)) {
    return &RNA_FileSelectParams;
  }
  if (params == (void *)ED_fileselect_get_asset_params(sfile)) {
    return &RNA_FileAssetSelectParams;
  }

  BLI_assert_msg(0, "Could not identify file select parameters");
  return nullptr;
}

static PointerRNA rna_FileBrowser_params_get(PointerRNA *ptr)
{
  SpaceFile *sfile = static_cast<SpaceFile *>(ptr->data);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  StructRNA *params_struct = rna_FileBrowser_params_typef(ptr);

  if (params && params_struct) {
    return RNA_pointer_create_with_parent(*ptr, params_struct, params);
  }

  return PointerRNA_NULL;
}

static void rna_FileBrowser_FSMenuEntry_path_get(PointerRNA *ptr, char *value)
{
  char *path = ED_fsmenu_entry_get_path(static_cast<FSMenuEntry *>(ptr->data));

  strcpy(value, path ? path : "");
}

static int rna_FileBrowser_FSMenuEntry_path_length(PointerRNA *ptr)
{
  char *path = ED_fsmenu_entry_get_path(static_cast<FSMenuEntry *>(ptr->data));

  return int(path ? strlen(path) : 0);
}

static void rna_FileBrowser_FSMenuEntry_path_set(PointerRNA *ptr, const char *value)
{
  FSMenuEntry *fsm = static_cast<FSMenuEntry *>(ptr->data);

  /* NOTE: this will write to file immediately.
   * Not nice (and to be fixed ultimately), but acceptable in this case for now. */
  ED_fsmenu_entry_set_path(fsm, value);
}

static void rna_FileBrowser_FSMenuEntry_name_get(PointerRNA *ptr, char *value)
{
  strcpy(value, ED_fsmenu_entry_get_name(static_cast<FSMenuEntry *>(ptr->data)));
}

static int rna_FileBrowser_FSMenuEntry_name_length(PointerRNA *ptr)
{
  return int(strlen(ED_fsmenu_entry_get_name(static_cast<FSMenuEntry *>(ptr->data))));
}

static void rna_FileBrowser_FSMenuEntry_name_set(PointerRNA *ptr, const char *value)
{
  FSMenuEntry *fsm = static_cast<FSMenuEntry *>(ptr->data);

  /* NOTE: this will write to file immediately.
   * Not nice (and to be fixed ultimately), but acceptable in this case for now. */
  ED_fsmenu_entry_set_name(fsm, value);
}

static int rna_FileBrowser_FSMenuEntry_name_get_editable(const PointerRNA *ptr,
                                                         const char ** /*r_info*/)
{
  FSMenuEntry *fsm = static_cast<FSMenuEntry *>(ptr->data);

  return fsm->save ? int(PROP_EDITABLE) : 0;
}

static int rna_FileBrowser_FSMenuEntry_icon_get(PointerRNA *ptr)
{
  FSMenuEntry *fsm = static_cast<FSMenuEntry *>(ptr->data);
  return ED_fsmenu_entry_get_icon(fsm);
}

static void rna_FileBrowser_FSMenuEntry_icon_set(PointerRNA *ptr, int value)
{
  FSMenuEntry *fsm = static_cast<FSMenuEntry *>(ptr->data);
  ED_fsmenu_entry_set_icon(fsm, value);
}

static bool rna_FileBrowser_FSMenuEntry_use_save_get(PointerRNA *ptr)
{
  FSMenuEntry *fsm = static_cast<FSMenuEntry *>(ptr->data);
  return fsm->save;
}

static void rna_FileBrowser_FSMenu_next(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;

  if (internal->skip) {
    do {
      internal->link = (Link *)(((FSMenuEntry *)(internal->link))->next);
      iter->valid = (internal->link != nullptr);
    } while (iter->valid && internal->skip(iter, internal->link));
  }
  else {
    internal->link = (Link *)(((FSMenuEntry *)(internal->link))->next);
    iter->valid = (internal->link != nullptr);
  }
}

static void rna_FileBrowser_FSMenu_begin(CollectionPropertyIterator *iter, FSMenuCategory category)
{
  ListBaseIterator *internal = &iter->internal.listbase;

  FSMenu *fsmenu = ED_fsmenu_get();
  FSMenuEntry *fsmentry = ED_fsmenu_get_category(fsmenu, category);

  internal->link = (fsmentry) ? (Link *)fsmentry : nullptr;
  internal->skip = nullptr;

  iter->valid = (internal->link != nullptr);
}

static PointerRNA rna_FileBrowser_FSMenu_get(CollectionPropertyIterator *iter)
{
  ListBaseIterator *internal = &iter->internal.listbase;
  PointerRNA ptr_result = RNA_pointer_create_with_parent(
      iter->parent, &RNA_FileBrowserFSMenuEntry, internal->link);
  return ptr_result;
}

static void rna_FileBrowser_FSMenu_end(CollectionPropertyIterator * /*iter*/) {}

static void rna_FileBrowser_FSMenuSystem_data_begin(CollectionPropertyIterator *iter,
                                                    PointerRNA * /*ptr*/)
{
  rna_FileBrowser_FSMenu_begin(iter, FS_CATEGORY_SYSTEM);
}

static int rna_FileBrowser_FSMenuSystem_data_length(PointerRNA * /*ptr*/)
{
  FSMenu *fsmenu = ED_fsmenu_get();

  return ED_fsmenu_get_nentries(fsmenu, FS_CATEGORY_SYSTEM);
}

static void rna_FileBrowser_FSMenuSystemBookmark_data_begin(CollectionPropertyIterator *iter,
                                                            PointerRNA * /*ptr*/)
{
  rna_FileBrowser_FSMenu_begin(iter, FS_CATEGORY_SYSTEM_BOOKMARKS);
}

static int rna_FileBrowser_FSMenuSystemBookmark_data_length(PointerRNA * /*ptr*/)
{
  FSMenu *fsmenu = ED_fsmenu_get();

  return ED_fsmenu_get_nentries(fsmenu, FS_CATEGORY_SYSTEM_BOOKMARKS);
}

static void rna_FileBrowser_FSMenuBookmark_data_begin(CollectionPropertyIterator *iter,
                                                      PointerRNA * /*ptr*/)
{
  rna_FileBrowser_FSMenu_begin(iter, FS_CATEGORY_BOOKMARKS);
}

static int rna_FileBrowser_FSMenuBookmark_data_length(PointerRNA * /*ptr*/)
{
  FSMenu *fsmenu = ED_fsmenu_get();

  return ED_fsmenu_get_nentries(fsmenu, FS_CATEGORY_BOOKMARKS);
}

static void rna_FileBrowser_FSMenuRecent_data_begin(CollectionPropertyIterator *iter,
                                                    PointerRNA * /*ptr*/)
{
  rna_FileBrowser_FSMenu_begin(iter, FS_CATEGORY_RECENT);
}

static int rna_FileBrowser_FSMenuRecent_data_length(PointerRNA * /*ptr*/)
{
  FSMenu *fsmenu = ED_fsmenu_get();

  return ED_fsmenu_get_nentries(fsmenu, FS_CATEGORY_RECENT);
}

static int rna_FileBrowser_FSMenu_active_get(PointerRNA *ptr, const FSMenuCategory category)
{
  SpaceFile *sf = static_cast<SpaceFile *>(ptr->data);
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
  SpaceFile *sf = static_cast<SpaceFile *>(ptr->data);
  FSMenu *fsmenu = ED_fsmenu_get();
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

    STRNCPY(sf->params->dir, fsm->path);
  }
}

static void rna_FileBrowser_FSMenu_active_range(PointerRNA * /*ptr*/,
                                                int *min,
                                                int *max,
                                                int *softmin,
                                                int *softmax,
                                                const FSMenuCategory category)
{
  FSMenu *fsmenu = ED_fsmenu_get();

  *min = *softmin = -1;
  *max = *softmax = ED_fsmenu_get_nentries(fsmenu, category) - 1;
}

static void rna_FileBrowser_FSMenu_active_update(bContext *C, PointerRNA *ptr)
{
  ScrArea *area = rna_area_from_space(ptr);
  ED_file_change_dir_ex(C, area);
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

static void rna_SpaceFileBrowser_browse_mode_update(Main * /*bmain*/,
                                                    Scene * /*scene*/,
                                                    PointerRNA *ptr)
{
  ScrArea *area = rna_area_from_space(ptr);
  ED_area_tag_refresh(area);
}

static void rna_SpaceSpreadsheet_geometry_component_type_update(Main * /*bmain*/,
                                                                Scene * /*scene*/,
                                                                PointerRNA *ptr)
{
  using namespace blender;
  SpaceSpreadsheet *sspreadsheet = (SpaceSpreadsheet *)ptr->data;
  switch (sspreadsheet->geometry_id.geometry_component_type) {
    case int(bke::GeometryComponent::Type::Mesh): {
      if (!ELEM(bke::AttrDomain(sspreadsheet->geometry_id.attribute_domain),
                bke::AttrDomain::Point,
                bke::AttrDomain::Edge,
                bke::AttrDomain::Face,
                bke::AttrDomain::Corner))
      {
        sspreadsheet->geometry_id.attribute_domain = uint8_t(bke::AttrDomain::Point);
      }
      break;
    }
    case int(bke::GeometryComponent::Type::PointCloud): {
      sspreadsheet->geometry_id.attribute_domain = uint8_t(bke::AttrDomain::Point);
      break;
    }
    case int(bke::GeometryComponent::Type::Instance): {
      sspreadsheet->geometry_id.attribute_domain = uint8_t(bke::AttrDomain::Instance);
      break;
    }
    case int(bke::GeometryComponent::Type::Volume): {
      break;
    }
    case int(bke::GeometryComponent::Type::Curve): {
      if (!ELEM(bke::AttrDomain(sspreadsheet->geometry_id.attribute_domain),
                bke::AttrDomain::Point,
                bke::AttrDomain::Curve))
      {
        sspreadsheet->geometry_id.attribute_domain = uint8_t(bke::AttrDomain::Point);
      }
      break;
    }
  }
}

const EnumPropertyItem *rna_SpaceSpreadsheet_attribute_domain_itemf(bContext * /*C*/,
                                                                    PointerRNA *ptr,
                                                                    PropertyRNA * /*prop*/,
                                                                    bool *r_free)
{
  using namespace blender;
  SpaceSpreadsheet *sspreadsheet = (SpaceSpreadsheet *)ptr->data;
  auto component_type = bke::GeometryComponent::Type(
      sspreadsheet->geometry_id.geometry_component_type);
  if (sspreadsheet->geometry_id.object_eval_state == SPREADSHEET_OBJECT_EVAL_STATE_ORIGINAL) {
    ID *used_id = ed::spreadsheet::get_current_id(sspreadsheet);
    if (used_id != nullptr) {
      if (GS(used_id->name) == ID_OB) {
        Object *used_object = (Object *)used_id;
        if (used_object->type == OB_POINTCLOUD) {
          component_type = bke::GeometryComponent::Type::PointCloud;
        }
        else {
          component_type = bke::GeometryComponent::Type::Mesh;
        }
      }
    }
  }

  static EnumPropertyItem mesh_vertex_domain_item = {
      int(bke::AttrDomain::Point), "POINT", 0, "Vertex", "Attribute per point/vertex"};

  EnumPropertyItem *item_array = nullptr;
  int items_len = 0;
  for (const EnumPropertyItem *item = rna_enum_attribute_domain_items; item->identifier != nullptr;
       item++)
  {
    if (component_type == bke::GeometryComponent::Type::Mesh) {
      if (!ELEM(bke::AttrDomain(item->value),
                bke::AttrDomain::Corner,
                bke::AttrDomain::Edge,
                bke::AttrDomain::Point,
                bke::AttrDomain::Face))
      {
        continue;
      }
    }
    if (component_type == bke::GeometryComponent::Type::PointCloud) {
      if (bke::AttrDomain(item->value) != bke::AttrDomain::Point) {
        continue;
      }
    }
    if (component_type == bke::GeometryComponent::Type::Curve) {
      if (!ELEM(bke::AttrDomain(item->value), bke::AttrDomain::Point, bke::AttrDomain::Curve)) {
        continue;
      }
    }
    if (bke::AttrDomain(item->value) == bke::AttrDomain::Point &&
        component_type == bke::GeometryComponent::Type::Mesh)
    {
      RNA_enum_item_add(&item_array, &items_len, &mesh_vertex_domain_item);
    }
    else {
      RNA_enum_item_add(&item_array, &items_len, item);
    }
  }
  RNA_enum_item_end(&item_array, &items_len);

  *r_free = true;
  return item_array;
}

static StructRNA *rna_SpreadsheetTableID_refine(PointerRNA *ptr)
{
  SpreadsheetTableID *table_id = ptr->data_as<SpreadsheetTableID>();
  switch (eSpreadsheetTableIDType(table_id->type)) {
    case SPREADSHEET_TABLE_ID_TYPE_GEOMETRY:
      return &RNA_SpreadsheetTableIDGeometry;
  }
  return &RNA_SpreadsheetTableID;
}

static void rna_iterator_SpreadsheetTable_columns_begin(CollectionPropertyIterator *iter,
                                                        PointerRNA *ptr)
{
  SpreadsheetTable *table = ptr->data_as<SpreadsheetTable>();
  rna_iterator_array_begin(
      iter, ptr, table->columns, sizeof(SpreadsheetTable *), table->num_columns, 0, nullptr);
}

static int rna_iterator_SpreadsheetTable_columns_length(PointerRNA *ptr)
{
  SpreadsheetTable *table = ptr->data_as<SpreadsheetTable>();
  return table->num_columns;
}

static void rna_iterator_SpaceSpreadsheet_tables_begin(CollectionPropertyIterator *iter,
                                                       PointerRNA *ptr)
{
  SpaceSpreadsheet *sspreadsheet = ptr->data_as<SpaceSpreadsheet>();
  rna_iterator_array_begin(iter,
                           ptr,
                           sspreadsheet->tables,
                           sizeof(SpaceSpreadsheet *),
                           sspreadsheet->num_tables,
                           0,
                           nullptr);
}

static int rna_iterator_SpaceSpreadsheet_tables_length(PointerRNA *ptr)
{
  SpaceSpreadsheet *sspreadsheet = ptr->data_as<SpaceSpreadsheet>();
  return sspreadsheet->num_tables;
}

static PointerRNA rna_SpreadsheetTables_active_get(PointerRNA *ptr)
{
  SpaceSpreadsheet *sspreadsheet = ptr->data_as<SpaceSpreadsheet>();
  SpreadsheetTable *table = blender::ed::spreadsheet::get_active_table(*sspreadsheet);
  return RNA_pointer_create_discrete(ptr->owner_id, &RNA_SpreadsheetTable, table);
}

static StructRNA *rna_viewer_path_elem_refine(PointerRNA *ptr)
{
  ViewerPathElem *elem = static_cast<ViewerPathElem *>(ptr->data);
  switch (ViewerPathElemType(elem->type)) {
    case VIEWER_PATH_ELEM_TYPE_ID:
      return &RNA_IDViewerPathElem;
    case VIEWER_PATH_ELEM_TYPE_MODIFIER:
      return &RNA_ModifierViewerPathElem;
    case VIEWER_PATH_ELEM_TYPE_GROUP_NODE:
      return &RNA_GroupNodeViewerPathElem;
    case VIEWER_PATH_ELEM_TYPE_SIMULATION_ZONE:
      return &RNA_SimulationZoneViewerPathElem;
    case VIEWER_PATH_ELEM_TYPE_VIEWER_NODE:
      return &RNA_ViewerNodeViewerPathElem;
    case VIEWER_PATH_ELEM_TYPE_REPEAT_ZONE:
      return &RNA_RepeatZoneViewerPathElem;
    case VIEWER_PATH_ELEM_TYPE_FOREACH_GEOMETRY_ELEMENT_ZONE:
      return &RNA_ForeachGeometryElementZoneViewerPathElem;
    case VIEWER_PATH_ELEM_TYPE_EVALUATE_CLOSURE:
      return &RNA_EvaluateClosureNodeViewerPathElem;
  }
  BLI_assert_unreachable();
  return nullptr;
}

static void rna_FileAssetSelectParams_catalog_id_get(PointerRNA *ptr, char *value)
{
  const FileAssetSelectParams *params = static_cast<FileAssetSelectParams *>(ptr->data);
  BLI_uuid_format(value, params->catalog_id);
}

static int rna_FileAssetSelectParams_catalog_id_length(PointerRNA * /*ptr*/)
{
  return UUID_STRING_SIZE - 1;
}

static void rna_FileAssetSelectParams_catalog_id_set(PointerRNA *ptr, const char *value)
{
  FileAssetSelectParams *params = static_cast<FileAssetSelectParams *>(ptr->data);

  if (value[0] == '\0') {
    params->catalog_id = BLI_uuid_nil();
    params->asset_catalog_visibility = FILE_SHOW_ASSETS_ALL_CATALOGS;
    return;
  }

  bUUID new_uuid;
  if (!BLI_uuid_parse_string(&new_uuid, value)) {
    printf("UUID %s not formatted correctly, ignoring new value\n", value);
    return;
  }

  params->catalog_id = new_uuid;
  params->asset_catalog_visibility = FILE_SHOW_ASSETS_FROM_CATALOG;
}

static const EnumPropertyItem *rna_FileAssetSelectParams_import_method_itemf(
    bContext * /*C*/, PointerRNA * /*ptr*/, PropertyRNA * /*prop*/, bool *r_free)
{
  EnumPropertyItem *items = nullptr;
  int items_num = 0;
  for (const EnumPropertyItem *item = rna_enum_asset_import_method_items; item->identifier; item++)
  {
    switch (eFileAssetImportMethod(item->value)) {
      case FILE_ASSET_IMPORT_APPEND_REUSE: {
        if (U.experimental.no_data_block_packing) {
          RNA_enum_item_add(&items, &items_num, item);
        }
        break;
      }
      case FILE_ASSET_IMPORT_PACK: {
        if (!U.experimental.no_data_block_packing) {
          RNA_enum_item_add(&items, &items_num, item);
        }
        break;
      }
      default: {
        RNA_enum_item_add(&items, &items_num, item);
        break;
      }
    }
  }
  RNA_enum_item_end(&items, &items_num);
  *r_free = true;
  return items;
}

#else

static const EnumPropertyItem dt_uv_items[] = {
    {SI_UVDT_OUTLINE, "OUTLINE", 0, "Outline", "Display white edges with black outline"},
    {SI_UVDT_DASH, "DASH", 0, "Dash", "Display dashed black-white edges"},
    {SI_UVDT_BLACK, "BLACK", 0, "Black", "Display black edges"},
    {SI_UVDT_WHITE, "WHITE", 0, "White", "Display white edges"},
    {0, nullptr, 0, nullptr, nullptr},
};

static IDFilterEnumPropertyItem rna_enum_space_file_id_filter_categories[] = {
    /* Categories */
    {FILTER_ID_SCE, "category_scene", ICON_SCENE_DATA, "Scenes", "Show scenes"},
    {FILTER_ID_AC, "category_animation", ICON_ANIM_DATA, "Animations", "Show animation data"},
    {FILTER_ID_OB | FILTER_ID_GR,
     "category_object",
     ICON_OUTLINER_COLLECTION,
     "Objects & Collections",
     "Show objects and collections"},
    {FILTER_ID_AR | FILTER_ID_CU_LEGACY | FILTER_ID_LT | FILTER_ID_MB | FILTER_ID_ME |
         FILTER_ID_CV | FILTER_ID_PT | FILTER_ID_VO,
     "category_geometry",
     ICON_GEOMETRY_NODES,
     "Geometry",
     "Show meshes, curves, lattice, armatures and metaballs data"},
    {FILTER_ID_LS | FILTER_ID_MA | FILTER_ID_NT | FILTER_ID_TE,
     "category_shading",
     ICON_MATERIAL_DATA,
     "Shading",
     "Show materials, node-trees, textures and Freestyle's line-styles"},
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
    {FILTER_ID_BR | FILTER_ID_GD_LEGACY | FILTER_ID_PA | FILTER_ID_PAL | FILTER_ID_PC |
         FILTER_ID_TXT | FILTER_ID_VF | FILTER_ID_CF | FILTER_ID_WS,
     "category_misc",
     ICON_GREASEPENCIL,
     "Miscellaneous",
     "Show other data types"},
    {0, nullptr, 0, nullptr, nullptr},
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
  if (region_type_mask & (1 << RGN_TYPE_TOOL_PROPS)) {
    region_type_mask &= ~(1 << RGN_TYPE_TOOL_PROPS);
    DEF_SHOW_REGION_PROPERTY(show_region_tool_props, "Toolbar", "");
  }
  if (region_type_mask & (1 << RGN_TYPE_CHANNELS)) {
    region_type_mask &= ~(1 << RGN_TYPE_CHANNELS);
    DEF_SHOW_REGION_PROPERTY(show_region_channels, "Channels", "");
  }
  if (region_type_mask & (1 << RGN_TYPE_UI)) {
    region_type_mask &= ~(1 << RGN_TYPE_UI);
    DEF_SHOW_REGION_PROPERTY(show_region_ui, "Sidebar", "");
  }
  if (region_type_mask & (1 << RGN_TYPE_HUD)) {
    region_type_mask &= ~(1 << RGN_TYPE_HUD);
    DEF_SHOW_REGION_PROPERTY(show_region_hud, "Adjust Last Operation", "");
  }
  if (region_type_mask & ((1 << RGN_TYPE_ASSET_SHELF) | (1 << RGN_TYPE_ASSET_SHELF_HEADER))) {
    region_type_mask &= ~((1 << RGN_TYPE_ASSET_SHELF) | (1 << RGN_TYPE_ASSET_SHELF_HEADER));

    prop = RNA_def_property(srna, "show_region_asset_shelf", PROP_BOOLEAN, PROP_NONE);
    RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
    RNA_def_property_boolean_funcs(
        prop, "rna_Space_show_region_asset_shelf_get", "rna_Space_show_region_asset_shelf_set");
    RNA_def_property_editable_func(prop, "rna_Space_show_region_asset_shelf_editable");
    RNA_def_property_ui_text(
        prop,
        "Asset Shelf",
        "Display a region with assets that may currently be relevant (such as "
        "brushes in paint modes, or poses in Pose Mode)");
    RNA_def_property_update(prop, 0, "rna_Space_show_region_asset_shelf_update");
  }
  BLI_assert(region_type_mask == 0);
}

static void rna_def_space(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "Space", nullptr);
  RNA_def_struct_sdna(srna, "SpaceLink");
  RNA_def_struct_ui_text(srna, "Space", "Space data for a screen area");
  RNA_def_struct_path_func(srna, "BKE_screen_path_from_screen_to_space");
  RNA_def_struct_refine_func(srna, "rna_Space_refine");

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "spacetype");
  RNA_def_property_enum_items(prop, rna_enum_space_type_items);
  /* When making this editable, take care for the special case of global areas
   * (see rna_Area_type_set). */
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Type", "Space data type");

  /* Access to #V2D_VIEWSYNC_SCREEN_TIME. */
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
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "mask", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "mask_info.mask");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Mask", "Mask displayed and edited in this space");
  RNA_def_property_pointer_funcs(prop, nullptr, mask_set_func, nullptr, nullptr);
  RNA_def_property_update(prop, noteflag, nullptr);

  /* mask drawing */
  prop = RNA_def_property(srna, "mask_display_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mask_info.draw_type");
  RNA_def_property_enum_items(prop, dt_uv_items);
  RNA_def_property_ui_text(prop, "Edge Display Type", "Display type for mask splines");
  RNA_def_property_update(prop, noteflag, nullptr);

  prop = RNA_def_property(srna, "show_mask_spline", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mask_info.draw_flag", MASK_DRAWFLAG_SPLINE);
  RNA_def_property_ui_text(prop, "Show Mask Spline", "");
  RNA_def_property_update(prop, noteflag, nullptr);

  prop = RNA_def_property(srna, "show_mask_overlay", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "mask_info.draw_flag", MASK_DRAWFLAG_OVERLAY);
  RNA_def_property_ui_text(prop, "Show Mask Overlay", "");
  RNA_def_property_update(prop, noteflag, nullptr);

  prop = RNA_def_property(srna, "mask_overlay_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mask_info.overlay_mode");
  RNA_def_property_enum_items(prop, overlay_mode_items);
  RNA_def_property_ui_text(prop, "Overlay Mode", "Overlay mode of rasterized mask");
  RNA_def_property_update(prop, noteflag, nullptr);

  prop = RNA_def_property(srna, "blend_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "mask_info.blend_factor");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0, 1., 0.1, 1);
  RNA_def_property_ui_text(prop, "Blending Factor", "Overlay blending factor of rasterized mask");
  RNA_def_property_update(prop, noteflag, nullptr);
}

static void rna_def_space_image_uv(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem dt_uvstretch_items[] = {
      {SI_UVDT_STRETCH_ANGLE, "ANGLE", 0, "Angle", "Angular distortion between UV and 3D angles"},
      {SI_UVDT_STRETCH_AREA, "AREA", 0, "Area", "Area distortion between UV and 3D faces"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem pixel_round_mode_items[] = {
      {SI_PIXEL_ROUND_DISABLED, "DISABLED", 0, "Disabled", "Don't round to pixels"},
      {SI_PIXEL_ROUND_CORNER, "CORNER", 0, "Corner", "Round to pixel corners"},
      {SI_PIXEL_ROUND_CENTER, "CENTER", 0, "Center", "Round to pixel centers"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem grid_shape_source_items[] = {
      {SI_GRID_SHAPE_DYNAMIC, "DYNAMIC", 0, "Dynamic", "Dynamic grid"},
      {SI_GRID_SHAPE_FIXED, "FIXED", 0, "Fixed", "Manually set grid divisions"},
      {SI_GRID_SHAPE_PIXEL, "PIXEL", 0, "Pixel", "Grid aligns with pixels from image"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "SpaceUVEditor", nullptr);
  RNA_def_struct_sdna(srna, "SpaceImage");
  RNA_def_struct_nested(brna, srna, "SpaceImageEditor");
  RNA_def_struct_path_func(srna, "rna_SpaceUVEditor_path");
  RNA_def_struct_ui_text(srna, "Space UV Editor", "UV editor data for the image editor space");

  /* drawing */
  prop = RNA_def_property(srna, "edge_display_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "dt_uv");
  RNA_def_property_enum_items(prop, dt_uv_items);
  RNA_def_property_ui_text(prop, "Display As", "Display style for UV edges");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "show_stretch", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SI_DRAW_STRETCH);
  RNA_def_property_ui_text(
      prop,
      "Display Stretch",
      "Display faces colored according to the difference in shape between UVs and "
      "their 3D coordinates (blue for low distortion, red for high distortion)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "display_stretch_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "dt_uvstretch");
  RNA_def_property_enum_items(prop, dt_uvstretch_items);
  RNA_def_property_ui_text(prop, "Display Stretch Type", "Type of stretch to display");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MESH);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "show_modified_edges", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SI_DRAWSHADOW);
  RNA_def_property_ui_text(
      prop, "Display Modified Edges", "Display edges after modifiers are applied");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "show_metadata", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SI_DRAW_METADATA);
  RNA_def_property_ui_text(prop, "Show Metadata", "Display metadata properties of the image");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "show_uv", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", SI_NO_DRAW_UV_GUIDE);
  RNA_def_property_ui_text(prop, "Display UVs", "Display overlay of UV layer");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "show_pixel_coords", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", SI_COORDFLOATS);
  RNA_def_property_ui_text(
      prop, "Pixel Coordinates", "Display UV coordinates in pixels rather than from 0.0 to 1.0");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "show_faces", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", SI_NO_DRAWFACES);
  RNA_def_property_ui_text(prop, "Display Faces", "Display faces over the image");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "tile_grid_shape", PROP_INT, PROP_XYZ);
  RNA_def_property_int_sdna(prop, nullptr, "tile_grid_shape");
  RNA_def_property_array(prop, 2);
  RNA_def_property_int_default(prop, 1);
  RNA_def_property_range(prop, 1, 100);
  RNA_def_property_int_funcs(prop, nullptr, "rna_SpaceUVEditor_tile_grid_shape_set", nullptr);
  RNA_def_property_ui_text(
      prop, "Tile Grid Shape", "How many tiles will be shown in the background");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "show_grid_over_image", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SI_GRID_OVER_IMAGE);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(prop, "Grid Over Image", "Show the grid over the image");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "grid_shape_source", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, grid_shape_source_items);
  RNA_def_property_ui_text(prop, "Grid Shape Source", "Specify source for the grid shape");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "custom_grid_subdivisions", PROP_INT, PROP_XYZ);
  RNA_def_property_int_sdna(prop, nullptr, "custom_grid_subdiv");
  RNA_def_property_array(prop, 2);
  RNA_def_property_int_default(prop, 10);
  RNA_def_property_range(prop, 1, 5000);
  RNA_def_property_int_funcs(prop, nullptr, "rna_SpaceUVEditor_custom_grid_subdiv_set", nullptr);
  RNA_def_property_ui_text(
      prop, "Dynamic Grid Size", "Number of grid units in UV space that make one UV Unit");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "uv_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "uv_opacity");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "UV Opacity", "Opacity of UV overlays");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "uv_face_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "uv_face_opacity");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "UV Face Opacity", "Opacity of faces in UV overlays");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "stretch_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "stretch_opacity");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Stretch Opacity", "Opacity of the UV Stretch overlay");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "pixel_round_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, pixel_round_mode_items);
  RNA_def_property_ui_text(prop, "Round to Pixels", "Round UVs to pixels while editing");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "lock_bounds", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SI_CLIP_UV);
  RNA_def_property_ui_text(prop,
                           "Constrain to Image Bounds",
                           "Constraint to stay within the image bounds while editing");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "use_live_unwrap", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SI_LIVE_UNWRAP);
  RNA_def_property_ui_text(
      prop,
      "Live Unwrap",
      "Continuously unwrap the selected UV island while transforming pinned vertices");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);
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
      {SO_OVERRIDES_LIBRARY,
       "LIBRARY_OVERRIDES",
       ICON_LIBRARY_DATA_OVERRIDE,
       "Library Overrides",
       "Display data-blocks with library overrides and list their overridden properties"},
      {SO_ID_ORPHANS,
       "ORPHAN_DATA",
       ICON_ORPHAN_DATA,
       "Unused Data",
       "Display data that is unused and/or will be lost when the file is reloaded"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem lib_override_view_mode[] = {
      {SO_LIB_OVERRIDE_VIEW_PROPERTIES,
       "PROPERTIES",
       ICON_NONE,
       "Properties",
       "Display all local override data-blocks with their overridden properties and buttons to "
       "edit them"},
      {SO_LIB_OVERRIDE_VIEW_HIERARCHIES,
       "HIERARCHIES",
       ICON_NONE,
       "Hierarchies",
       "Display library override relationships"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem filter_state_items[] = {
      {SO_FILTER_OB_ALL, "ALL", 0, "All", "Show all objects in the view layer"},
      {SO_FILTER_OB_VISIBLE, "VISIBLE", 0, "Visible", "Show visible objects"},
      {SO_FILTER_OB_SELECTED, "SELECTED", 0, "Selected", "Show selected objects"},
      {SO_FILTER_OB_ACTIVE, "ACTIVE", 0, "Active", "Show only the active object"},
      {SO_FILTER_OB_SELECTABLE, "SELECTABLE", 0, "Selectable", "Show only selectable objects"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "SpaceOutliner", "Space");
  RNA_def_struct_sdna(srna, "SpaceOutliner");
  RNA_def_struct_ui_text(srna, "Space Outliner", "Outliner space data");

  prop = RNA_def_property(srna, "display_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "outlinevis");
  RNA_def_property_enum_items(prop, display_mode_items);
  RNA_def_property_ui_text(prop, "Display Mode", "Type of information to display");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "lib_override_view_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, lib_override_view_mode);
  RNA_def_property_ui_text(prop,
                           "Library Override View Mode",
                           "Choose different visualizations of library override data");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "filter_text", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "search_string");
  RNA_def_property_ui_text(prop, "Display Filter", "Live search filtering string");
  RNA_def_property_flag(prop, PROP_TEXTEDIT_UPDATE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "use_filter_case_sensitive", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "search_flags", SO_FIND_CASE_SENSITIVE);
  RNA_def_property_ui_text(
      prop, "Case Sensitive Matches Only", "Only use case sensitive matches of search string");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "use_filter_complete", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "search_flags", SO_FIND_COMPLETE);
  RNA_def_property_ui_text(
      prop, "Complete Matches Only", "Only use complete matches of search string");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "use_sort_alpha", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", SO_SKIP_SORT_ALPHA);
  RNA_def_property_ui_text(prop, "Sort Alphabetically", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "use_sync_select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SO_SYNC_SELECT);
  RNA_def_property_ui_text(
      prop, "Sync Outliner Selection", "Sync outliner selection with other editors");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "show_mode_column", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SO_MODE_COLUMN);
  RNA_def_property_ui_text(
      prop, "Show Mode Column", "Show the mode column for mode toggle and activation");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  /* Granular restriction column option. */
  prop = RNA_def_property(srna, "show_restrict_column_enable", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "show_restrict_flags", SO_RESTRICT_ENABLE);
  RNA_def_property_ui_text(prop, "Exclude from View Layer", "Exclude from view layer");
  RNA_def_property_ui_icon(prop, ICON_CHECKBOX_HLT, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "show_restrict_column_select", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "show_restrict_flags", SO_RESTRICT_SELECT);
  RNA_def_property_ui_text(prop, "Selectable", "Selectable");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_SELECT_OFF, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "show_restrict_column_hide", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "show_restrict_flags", SO_RESTRICT_HIDE);
  RNA_def_property_ui_text(prop, "Hide in Viewport", "Temporarily hide in viewport");
  RNA_def_property_ui_icon(prop, ICON_HIDE_OFF, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "show_restrict_column_viewport", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "show_restrict_flags", SO_RESTRICT_VIEWPORT);
  RNA_def_property_ui_text(prop, "Disable in Viewports", "Globally disable in viewports");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_VIEW_OFF, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "show_restrict_column_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "show_restrict_flags", SO_RESTRICT_RENDER);
  RNA_def_property_ui_text(prop, "Disable in Renders", "Globally disable in renders");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_RENDER_OFF, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "show_restrict_column_holdout", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "show_restrict_flags", SO_RESTRICT_HOLDOUT);
  RNA_def_property_ui_text(prop, "Holdout", "Holdout");
  RNA_def_property_ui_icon(prop, ICON_HOLDOUT_ON, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "show_restrict_column_indirect_only", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "show_restrict_flags", SO_RESTRICT_INDIRECT_ONLY);
  RNA_def_property_ui_text(prop, "Indirect Only", "Indirect only");
  RNA_def_property_ui_icon(prop, ICON_INDIRECT_ONLY_ON, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  /* Filters. */
  prop = RNA_def_property(srna, "use_filter_object", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filter", SO_FILTER_NO_OBJECT);
  RNA_def_property_ui_text(prop, "Filter Objects", "Show objects");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "use_filter_object_content", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filter", SO_FILTER_NO_OB_CONTENT);
  RNA_def_property_ui_text(
      prop, "Show Object Contents", "Show what is inside the objects elements");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "use_filter_children", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filter", SO_FILTER_NO_CHILDREN);
  RNA_def_property_ui_text(prop, "Show Object Children", "Show children");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "use_filter_collection", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filter", SO_FILTER_NO_COLLECTION);
  RNA_def_property_ui_text(prop, "Show Collections", "Show collections");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "use_filter_view_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filter", SO_FILTER_NO_VIEW_LAYERS);
  RNA_def_property_ui_text(prop, "Show All View Layers", "Show all the view layers");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  /* Filters object state. */
  prop = RNA_def_property(srna, "filter_state", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "filter_state");
  RNA_def_property_enum_items(prop, filter_state_items);
  RNA_def_property_ui_text(prop, "Object State Filter", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "filter_invert", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filter", SO_FILTER_OB_STATE_INVERSE);
  RNA_def_property_ui_text(prop, "Invert", "Invert the object state filter");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  /* Filters object type. */
  prop = RNA_def_property(srna, "use_filter_object_mesh", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filter", SO_FILTER_NO_OB_MESH);
  RNA_def_property_ui_text(prop, "Show Meshes", "Show mesh objects");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "use_filter_object_armature", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filter", SO_FILTER_NO_OB_ARMATURE);
  RNA_def_property_ui_text(prop, "Show Armatures", "Show armature objects");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "use_filter_object_empty", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filter", SO_FILTER_NO_OB_EMPTY);
  RNA_def_property_ui_text(prop, "Show Empties", "Show empty objects");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "use_filter_object_light", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filter", SO_FILTER_NO_OB_LAMP);
  RNA_def_property_ui_text(prop, "Show Lights", "Show light objects");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "use_filter_object_camera", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filter", SO_FILTER_NO_OB_CAMERA);
  RNA_def_property_ui_text(prop, "Show Cameras", "Show camera objects");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "use_filter_object_grease_pencil", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filter", SO_FILTER_NO_OB_GREASE_PENCIL);
  RNA_def_property_ui_text(prop, "Show Grease Pencil", "Show Grease Pencil objects");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "use_filter_object_others", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "filter", SO_FILTER_NO_OB_OTHERS);
  RNA_def_property_ui_text(
      prop, "Show Other Objects", "Show curves, lattices, light probes, fonts, ...");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  /* Libraries filter. */
  prop = RNA_def_property(srna, "use_filter_id_type", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filter", SO_FILTER_ID_TYPE);
  RNA_def_property_ui_text(prop, "Filter by Type", "Show only data-blocks of one type");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);

  prop = RNA_def_property(srna, "filter_id_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "filter_id_type");
  RNA_def_property_enum_items(prop, rna_enum_id_type_items);
  RNA_def_property_ui_text(prop, "Filter by Type", "Data-block type to show");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ID);

  prop = RNA_def_property(srna, "use_filter_lib_override_system", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filter", SO_FILTER_SHOW_SYSTEM_OVERRIDES);
  RNA_def_property_ui_text(
      prop,
      "Show System Overrides",
      "For libraries with overrides created, show the overridden values that are "
      "defined/controlled automatically (e.g. to make users of an overridden data-block point to "
      "the override data, not the original linked data)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_OUTLINER, nullptr);
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
       "Custom",
       "Use a custom color limited to this viewport only"},
      {0, nullptr, 0, nullptr, nullptr},
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
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem use_compositor_items[] = {
      {V3D_SHADING_USE_COMPOSITOR_DISABLED,
       "DISABLED",
       0,
       "Disabled",
       "The compositor is disabled"},
      {V3D_SHADING_USE_COMPOSITOR_CAMERA,
       "CAMERA",
       0,
       "Camera",
       "The compositor is enabled only in camera view"},
      {V3D_SHADING_USE_COMPOSITOR_ALWAYS,
       "ALWAYS",
       0,
       "Always",
       "The compositor is always enabled regardless of the view"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* Note these settings are used for both 3D viewport and the OpenGL render
   * engine in the scene, so can't assume to always be part of a screen. */
  srna = RNA_def_struct(brna, "View3DShading", nullptr);
  RNA_def_struct_path_func(srna, "rna_View3DShading_path");
  RNA_def_struct_ui_text(
      srna, "3D View Shading Settings", "Settings for shading in the 3D viewport");
  RNA_def_struct_system_idprops_func(srna, "rna_View3DShading_idprops");

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
  RNA_def_property_enum_sdna(prop, nullptr, "light");
  RNA_def_property_enum_items(prop, rna_enum_viewport_lighting_items);
  RNA_def_property_ui_text(prop, "Lighting", "Lighting Method for Solid/Texture Viewport Shading");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "show_object_outline", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", V3D_SHADING_OBJECT_OUTLINE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Outline", "Show Object Outline");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "studio_light", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_studio_light_items);
  RNA_def_property_enum_default(prop, 0);
  RNA_def_property_enum_funcs(prop,
                              "rna_View3DShading_studio_light_get",
                              "rna_View3DShading_studio_light_set",
                              "rna_View3DShading_studio_light_itemf");
  RNA_def_property_ui_text(prop, "Studiolight", "Studio lighting setup");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "use_world_space_lighting", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", V3D_SHADING_WORLD_ORIENTATION);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "World Space Lighting", "Make the lighting fixed and not follow the camera");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "show_backface_culling", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", V3D_SHADING_BACKFACE_CULLING);
  RNA_def_property_ui_text(
      prop, "Backface Culling", "Use back face culling to hide the back side of faces");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "show_cavity", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", V3D_SHADING_CAVITY);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Cavity", "Show Cavity");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "cavity_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, cavity_type_items);
  RNA_def_property_ui_text(prop, "Cavity Type", "Way to display the cavity shading");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_EDITOR_VIEW3D);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "curvature_ridge_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "curvature_ridge_factor");
  RNA_def_property_ui_text(prop, "Curvature Ridge", "Factor for the curvature ridges");
  RNA_def_property_range(prop, 0.0f, 2.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "curvature_valley_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "curvature_valley_factor");
  RNA_def_property_ui_text(prop, "Curvature Valley", "Factor for the curvature valleys");
  RNA_def_property_range(prop, 0.0f, 2.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "cavity_ridge_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "cavity_ridge_factor");
  RNA_def_property_ui_text(prop, "Cavity Ridge", "Factor for the cavity ridges");
  RNA_def_property_range(prop, 0.0f, 250.0f);
  RNA_def_property_ui_range(prop, 0.00f, 2.5f, 1, 3);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "cavity_valley_factor", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "cavity_valley_factor");
  RNA_def_property_ui_text(prop, "Cavity Valley", "Factor for the cavity valleys");
  RNA_def_property_range(prop, 0.0f, 250.0f);
  RNA_def_property_ui_range(prop, 0.00f, 2.5f, 1, 3);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "selected_studio_light", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "StudioLight");
  RNA_define_verify_sdna(false);
  RNA_def_property_ui_text(prop, "Studio Light", "Selected StudioLight");
  RNA_def_property_pointer_funcs(
      prop, "rna_View3DShading_selected_studio_light_get", nullptr, nullptr, nullptr);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE | PROP_EDITABLE);
  RNA_define_verify_sdna(true);

  prop = RNA_def_property(srna, "studiolight_rotate_z", PROP_FLOAT, PROP_ANGLE);
  RNA_def_property_float_sdna(prop, nullptr, "studiolight_rot_z");
  RNA_def_property_ui_text(
      prop, "Studiolight Rotation", "Rotation of the studiolight around the Z-Axis");
  RNA_def_property_range(prop, -M_PI, M_PI);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "studiolight_intensity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "studiolight_intensity");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Strength", "Strength of the studiolight");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_AMOUNT);
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 2.0f, 1, 3);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "studiolight_background_alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "studiolight_background");
  RNA_def_property_ui_text(prop, "World Opacity", "Show the studiolight in the background");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 1, 3);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "studiolight_background_blur", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "studiolight_blur");
  RNA_def_property_ui_text(prop, "Blur", "Blur the studiolight in the background");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 1, 2);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "use_studiolight_view_rotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(
      prop, nullptr, "flag", V3D_SHADING_STUDIOLIGHT_VIEW_ROTATION);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "World Space Lighting", "Make the HDR rotation fixed and not follow the camera");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "color_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "color_type");
  RNA_def_property_enum_items(prop, rna_enum_shading_color_type_items);
  RNA_def_property_ui_text(prop, "Color", "Color Type");
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "wireframe_color_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "wire_color_type");
  RNA_def_property_enum_items(prop, rna_enum_shading_wire_color_type_items);
  RNA_def_property_ui_text(prop, "Wire Color", "Wire Color Type");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "single_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "single_color");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Color", "Color for single color mode");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "background_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, background_type_items);
  RNA_def_property_ui_text(prop, "Background", "Way to display the background");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_EDITOR_VIEW3D);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "background_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Background Color", "Color for custom background color");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "show_shadows", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", V3D_SHADING_SHADOW);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Shadow", "Show Shadow");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "show_xray", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", V3D_SHADING_XRAY);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Show X-Ray", "Show whole scene transparent");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "show_xray_wireframe", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", V3D_SHADING_XRAY_WIREFRAME);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Show X-Ray", "Show whole scene transparent");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "xray_alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "xray_alpha");
  RNA_def_property_ui_text(prop, "X-Ray Opacity", "Amount of opacity to use");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "xray_alpha_wireframe", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "xray_alpha_wire");
  RNA_def_property_ui_text(prop, "X-Ray Opacity", "Amount of opacity to use");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "use_dof", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", V3D_SHADING_DEPTH_OF_FIELD);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop,
      "Depth Of Field",
      "Use depth of field on viewport using the values from the active camera");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "use_scene_lights", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", V3D_SHADING_SCENE_LIGHTS);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Scene Lights", "Render lights and light probes of the scene");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "use_scene_world", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", V3D_SHADING_SCENE_WORLD);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Scene World", "Use scene world for lighting");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "use_scene_lights_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", V3D_SHADING_SCENE_LIGHTS_RENDER);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Scene Lights", "Render lights and light probes of the scene");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "use_scene_world_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", V3D_SHADING_SCENE_WORLD_RENDER);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Scene World", "Use scene world for lighting");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "show_specular_highlight", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", V3D_SHADING_SPECULAR_HIGHLIGHT);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Specular Highlights", "Render specular highlights");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "object_outline_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "object_outline_color");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Outline Color", "Color for object outline");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "shadow_intensity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "shadow_intensity");
  RNA_def_property_ui_text(prop, "Shadow Intensity", "Darkness of shadows");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_range(prop, 0.00f, 1.0f, 1, 3);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "render_pass", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "render_pass");
  RNA_def_property_enum_items(prop, rna_enum_view3dshading_render_pass_type_items);
  RNA_def_property_ui_text(prop, "Render Pass", "Render Pass to show in the viewport");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_RENDER_LAYER);
  RNA_def_property_enum_funcs(prop,
                              "rna_3DViewShading_render_pass_get",
                              "rna_3DViewShading_render_pass_set",
                              "rna_3DViewShading_render_pass_itemf");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "aov_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "aov_name");
  RNA_def_property_ui_text(prop, "Shader AOV Name", "Name of the active Shader AOV");
  RNA_def_property_flag(prop, PROP_HIDDEN);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "use_compositor", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "use_compositor");
  RNA_def_property_enum_items(prop, use_compositor_items);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Compositor", "When to preview the compositor output inside the viewport");
  RNA_def_property_update(prop,
                          NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING,
                          "rna_SpaceView3D_shading_use_compositor_update");
}

static void rna_def_space_view3d_overlay(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "View3DOverlay", nullptr);
  RNA_def_struct_sdna(srna, "View3D");
  RNA_def_struct_nested(brna, srna, "SpaceView3D");
  RNA_def_struct_path_func(srna, "rna_View3DOverlay_path");
  RNA_def_struct_ui_text(
      srna, "3D View Overlay Settings", "Settings for display of overlays in the 3D viewport");

  prop = RNA_def_property(srna, "show_overlays", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag2", V3D_HIDE_OVERLAYS);
  RNA_def_property_ui_text(prop, "Show Overlays", "Display overlays like gizmos and outlines");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_SpaceView3D_show_overlay_update");

  prop = RNA_def_property(srna, "show_ortho_grid", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gridflag", V3D_SHOW_ORTHO_GRID);
  RNA_def_property_ui_text(prop, "Display Grid", "Show grid in orthographic side view");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_floor", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gridflag", V3D_SHOW_FLOOR);
  RNA_def_property_ui_text(prop, "Display Grid Floor", "Show the ground plane grid");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_axis_x", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gridflag", V3D_SHOW_X);
  RNA_def_property_ui_text(prop, "Display X Axis", "Show the X axis line");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_axis_y", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gridflag", V3D_SHOW_Y);
  RNA_def_property_ui_text(prop, "Display Y Axis", "Show the Y axis line");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_axis_z", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gridflag", V3D_SHOW_Z);
  RNA_def_property_ui_text(prop, "Display Z Axis", "Show the Z axis line");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "grid_scale", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "grid");
  RNA_def_property_ui_text(
      prop, "Grid Scale", "Multiplier for the distance between 3D View grid lines");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001f, 1000.0f, 0.1f, 3);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "grid_lines", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "gridlines");
  RNA_def_property_ui_text(
      prop, "Grid Lines", "Number of grid lines to display in perspective view");
  RNA_def_property_range(prop, 0, 1024);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "grid_subdivisions", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "gridsubdiv");
  RNA_def_property_ui_text(prop, "Grid Subdivisions", "Number of subdivisions between grid lines");
  RNA_def_property_range(prop, 1, 1024);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "grid_scale_unit", PROP_FLOAT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_View3DOverlay_GridScaleUnit_get", nullptr, nullptr);
  RNA_def_property_ui_text(
      prop, "Grid Scale Unit", "Grid cell size scaled by scene unit system settings");

  prop = RNA_def_property(srna, "show_outline_selected", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", V3D_SELECT_OUTLINE);
  RNA_def_property_ui_text(
      prop, "Outline Selected", "Show an outline highlight around selected objects");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_object_origins", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(
      prop, nullptr, "overlay.flag", V3D_OVERLAY_HIDE_OBJECT_ORIGINS);
  RNA_def_property_ui_text(prop, "Object Origins", "Show object center dots");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_object_origins_all", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", V3D_DRAW_CENTERS);
  RNA_def_property_ui_text(
      prop,
      "All Object Origins",
      "Show the object origin center dot for all (selected and unselected) objects");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_relationship_lines", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", V3D_HIDE_HELPLINES);
  RNA_def_property_ui_text(prop,
                           "Relationship Lines",
                           "Show dashed lines indicating parent or constraint relationships");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_cursor", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "overlay.flag", V3D_OVERLAY_HIDE_CURSOR);
  RNA_def_property_ui_text(prop, "Show 3D Cursor", "Display 3D Cursor Overlay");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_text", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "overlay.flag", V3D_OVERLAY_HIDE_TEXT);
  RNA_def_property_ui_text(prop, "Show Text", "Display overlay text");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_stats", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", V3D_OVERLAY_STATS);
  RNA_def_property_ui_text(prop, "Show Statistics", "Display scene statistics overlay text");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  /* show camera composition guides */
  prop = RNA_def_property(srna, "show_camera_guides", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", V3D_SHOW_CAMERA_GUIDES);
  RNA_def_property_ui_text(prop, "Show Camera Guides", "Show camera composition guides");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_camera_passepartout", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", V3D_SHOW_CAMERA_PASSEPARTOUT);
  RNA_def_property_ui_text(prop, "Show Passepartout", "Show camera passepartout");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_extras", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(
      prop, nullptr, "overlay.flag", V3D_OVERLAY_HIDE_OBJECT_XTRAS);
  RNA_def_property_ui_text(
      prop, "Extras", "Object details, including empty wire, cameras and other visual guides");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_light_colors", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", V3D_OVERLAY_SHOW_LIGHT_COLORS);
  RNA_def_property_ui_text(prop, "Light Colors", "Show light colors");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_bones", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "overlay.flag", V3D_OVERLAY_HIDE_BONES);
  RNA_def_property_ui_text(
      prop, "Show Bones", "Display bones (disable to show motion paths only)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_face_orientation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", V3D_OVERLAY_FACE_ORIENTATION);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Face Orientation", "Show the Face Orientation Overlay");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_fade_inactive", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", V3D_OVERLAY_FADE_INACTIVE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "Fade Inactive Objects", "Fade inactive geometry using the viewport background color");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "fade_inactive_alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "overlay.fade_alpha");
  RNA_def_property_ui_text(prop, "Opacity", "Strength of the fade effect");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_xray_bone", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", V3D_OVERLAY_BONE_SELECT);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Show Bone X-Ray", "Show the bone selection overlay");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "xray_alpha_bone", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "overlay.xray_alpha_bone");
  RNA_def_property_ui_text(prop, "Opacity", "Opacity to use for bone selection");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "bone_wire_alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "overlay.bone_wire_alpha");
  RNA_def_property_ui_text(
      prop, "Bone Wireframe Opacity", "Maximum opacity of bones in wireframe display mode");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 1.0f, 1, 2);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_motion_paths", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(
      prop, nullptr, "overlay.flag", V3D_OVERLAY_HIDE_MOTION_PATHS);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Motion Paths", "Show the Motion Paths Overlay");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_onion_skins", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", V3D_OVERLAY_ONION_SKINS);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Onion Skins", "Show the Onion Skinning Overlay");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_look_dev", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", V3D_OVERLAY_LOOK_DEV);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop,
                           "Reference Spheres",
                           "Show reference spheres with neutral shading that react to lighting to "
                           "assist in look development");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  prop = RNA_def_property(srna, "show_wireframes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", V3D_OVERLAY_WIREFRAMES);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Wireframe", "Show face edges wires");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "wireframe_threshold", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "overlay.wireframe_threshold");
  RNA_def_property_ui_text(prop,
                           "Wireframe Threshold",
                           "Adjust the angle threshold for displaying edges "
                           "(1.0 for all)");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "wireframe_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "overlay.wireframe_opacity");
  RNA_def_property_ui_text(prop,
                           "Wireframe Opacity",
                           "Opacity of the displayed edges "
                           "(1.0 for opaque)");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_viewer_attribute", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", V3D_OVERLAY_VIEWER_ATTRIBUTE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop, "Viewer Node", "Show attribute overlay for active viewer node");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "viewer_attribute_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "overlay.viewer_attribute_opacity");
  RNA_def_property_ui_text(
      prop, "Viewer Attribute Opacity", "Opacity of the attribute that is currently visualized");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_viewer_text", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", V3D_OVERLAY_VIEWER_ATTRIBUTE_TEXT);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(
      prop, "View Attribute Text", "Show attribute values as text in viewport");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_paint_wire", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.paint_flag", V3D_OVERLAY_PAINT_WIRE);
  RNA_def_property_ui_text(prop, "Show Wire", "Use wireframe display in painting modes");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_wpaint_contours", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.wpaint_flag", V3D_OVERLAY_WPAINT_CONTOURS);
  RNA_def_property_ui_text(
      prop,
      "Show Weight Contours",
      "Show contour lines formed by points with the same interpolated weight");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_weight", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.edit_flag", V3D_OVERLAY_EDIT_WEIGHT);
  RNA_def_property_ui_text(prop, "Show Weights", "Display weights in editmode");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_retopology", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.edit_flag", V3D_OVERLAY_EDIT_RETOPOLOGY);
  RNA_def_property_ui_text(prop,
                           "Retopology",
                           "Hide the solid mesh and offset the overlay towards the view. "
                           "Selection is occluded by inactive geometry, unless X-Ray is enabled");
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, "rna_SpaceView3D_retopology_update");

  prop = RNA_def_property(srna, "retopology_offset", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "overlay.retopology_offset");
  RNA_def_property_ui_text(
      prop, "Retopology Offset", "Offset used to draw edit mesh in front of other geometry");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.0f, 10.0f, 0.1f, 3);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_face_normals", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.edit_flag", V3D_OVERLAY_EDIT_FACE_NORMALS);
  RNA_def_property_ui_text(prop, "Display Normals", "Display face normals as lines");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_vertex_normals", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.edit_flag", V3D_OVERLAY_EDIT_VERT_NORMALS);
  RNA_def_property_ui_text(prop, "Display Vertex Normals", "Display vertex normals as lines");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_split_normals", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.edit_flag", V3D_OVERLAY_EDIT_LOOP_NORMALS);
  RNA_def_property_ui_text(
      prop, "Display Custom Normals", "Display vertex-per-face normals as lines");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_faces", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.edit_flag", V3D_OVERLAY_EDIT_FACES);
  RNA_def_property_ui_text(prop, "Display Faces", "Display a face selection overlay");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_face_center", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.edit_flag", V3D_OVERLAY_EDIT_FACE_DOT);
  RNA_def_property_ui_text(
      prop,
      "Display Face Center",
      "Display face center when face selection is enabled in solid shading modes");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_edge_crease", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.edit_flag", V3D_OVERLAY_EDIT_CREASES);
  RNA_def_property_ui_text(
      prop, "Display Creases", "Display creases created for Subdivision Surface modifier");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_edge_bevel_weight", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.edit_flag", V3D_OVERLAY_EDIT_BWEIGHTS);
  RNA_def_property_ui_text(
      prop, "Display Bevel Weights", "Display weights created for the Bevel modifier");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_edge_seams", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.edit_flag", V3D_OVERLAY_EDIT_SEAMS);
  RNA_def_property_ui_text(prop, "Display Seams", "Display UV unwrapping seams");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_edge_sharp", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.edit_flag", V3D_OVERLAY_EDIT_SHARP);
  RNA_def_property_ui_text(
      prop, "Display Sharp", "Display sharp edges, used with the Edge Split modifier");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_freestyle_edge_marks", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "overlay.edit_flag", V3D_OVERLAY_EDIT_FREESTYLE_EDGE);
  RNA_def_property_ui_text(prop,
                           "Display Freestyle Edge Marks",
                           "Display Freestyle edge marks, used with the Freestyle renderer");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_freestyle_face_marks", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "overlay.edit_flag", V3D_OVERLAY_EDIT_FREESTYLE_FACE);
  RNA_def_property_ui_text(prop,
                           "Display Freestyle Face Marks",
                           "Display Freestyle face marks, used with the Freestyle renderer");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_statvis", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.edit_flag", V3D_OVERLAY_EDIT_STATVIS);
  RNA_def_property_ui_text(
      prop, "Mesh Analysis", "Display statistical information about the mesh");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_extra_edge_length", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.edit_flag", V3D_OVERLAY_EDIT_EDGE_LEN);
  RNA_def_property_ui_text(
      prop,
      "Edge Length",
      "Display selected edge lengths, using global values when set in the transform panel");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_extra_edge_angle", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.edit_flag", V3D_OVERLAY_EDIT_EDGE_ANG);
  RNA_def_property_ui_text(
      prop,
      "Edge Angle",
      "Display selected edge angle, using global values when set in the transform panel");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_extra_face_angle", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.edit_flag", V3D_OVERLAY_EDIT_FACE_ANG);
  RNA_def_property_ui_text(prop,
                           "Face Angles",
                           "Display the angles in the selected edges, "
                           "using global values when set in the transform panel");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_extra_face_area", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.edit_flag", V3D_OVERLAY_EDIT_FACE_AREA);
  RNA_def_property_ui_text(prop,
                           "Face Area",
                           "Display the area of selected faces, "
                           "using global values when set in the transform panel");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_extra_indices", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.edit_flag", V3D_OVERLAY_EDIT_INDICES);
  RNA_def_property_ui_text(
      prop, "Indices", "Display the index numbers of selected vertices, edges, and faces");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "display_handle", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "overlay.handle_display");
  RNA_def_property_enum_items(prop, rna_enum_curve_display_handle_items);
  RNA_def_property_ui_text(
      prop, "Display Handles", "Limit the display of curve handles in edit mode");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_curve_normals", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.edit_flag", V3D_OVERLAY_EDIT_CU_NORMALS);
  RNA_def_property_ui_text(prop, "Draw Normals", "Display 3D curve normals in editmode");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "normals_length", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "overlay.normals_length");
  RNA_def_property_ui_text(prop, "Normal Size", "Display size for normals in the 3D view");
  RNA_def_property_range(prop, 0.00001, 100000.0);
  RNA_def_property_ui_range(prop, 0.01, 2.0, 1, 2);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "normals_constant_screen_size", PROP_FLOAT, PROP_PIXEL);
  RNA_def_property_float_sdna(prop, nullptr, "overlay.normals_constant_screen_size");
  RNA_def_property_ui_text(prop, "Normal Screen Size", "Screen size for normals in the 3D view");
  RNA_def_property_range(prop, 0.0, 100000.0);
  RNA_def_property_ui_range(prop, 1.0, 100.0, 50, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "use_normals_constant_screen_size", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "overlay.edit_flag", V3D_OVERLAY_EDIT_CONSTANT_SCREEN_SIZE_NORMALS);
  RNA_def_property_ui_text(prop,
                           "Constant Screen Size Normals",
                           "Keep size of normals constant in relation to 3D view");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "texture_paint_mode_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "overlay.texture_paint_mode_opacity");
  RNA_def_property_ui_text(
      prop, "Stencil Mask Opacity", "Opacity of the texture paint mode stencil mask overlay");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "vertex_paint_mode_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "overlay.vertex_paint_mode_opacity");
  RNA_def_property_ui_text(
      prop, "Stencil Mask Opacity", "Opacity of the texture paint mode stencil mask overlay");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "weight_paint_mode_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "overlay.weight_paint_mode_opacity");
  RNA_def_property_ui_text(
      prop, "Weight Paint Opacity", "Opacity of the weight paint mode overlay");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "sculpt_mode_mask_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "overlay.sculpt_mode_mask_opacity");
  RNA_def_property_ui_text(prop, "Sculpt Mask Opacity", "");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_sculpt_curves_cage", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", V3D_OVERLAY_SCULPT_CURVES_CAGE);
  RNA_def_property_ui_text(
      prop, "Sculpt Curves Cage", "Show original curves that are currently being edited");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "sculpt_curves_cage_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "overlay.sculpt_curves_cage_opacity");
  RNA_def_property_ui_text(
      prop, "Curves Sculpt Cage Opacity", "Opacity of the cage overlay in curves sculpt mode");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "sculpt_mode_face_sets_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "overlay.sculpt_mode_face_sets_opacity");
  RNA_def_property_ui_text(prop, "Sculpt Face Sets Opacity", "");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_sculpt_mask", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", V3D_OVERLAY_SCULPT_SHOW_MASK);
  RNA_def_property_ui_text(prop, "Sculpt Show Mask", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_sculpt_face_sets", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", V3D_OVERLAY_SCULPT_SHOW_FACE_SETS);
  RNA_def_property_ui_text(prop, "Sculpt Show Face Sets", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  /* grease pencil paper settings */
  prop = RNA_def_property(srna, "show_annotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", V3D_SHOW_ANNOTATION);
  RNA_def_property_ui_text(prop, "Show Annotation", "Show annotations for this view");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "use_gpencil_fade_objects", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gp_flag", V3D_GP_FADE_OBJECTS);
  RNA_def_property_ui_text(
      prop,
      "Fade Objects",
      "Fade all viewport objects with a full color layer to improve visibility");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "use_gpencil_grid", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gp_flag", V3D_GP_SHOW_GRID);
  RNA_def_property_ui_text(prop, "Use Grid", "Display a grid over Grease Pencil paper");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "use_gpencil_fade_layers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gp_flag", V3D_GP_FADE_NOACTIVE_LAYERS);
  RNA_def_property_ui_text(
      prop, "Fade Layers", "Toggle fading of Grease Pencil layers except the active one");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "use_gpencil_fade_gp_objects", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gp_flag", V3D_GP_FADE_NOACTIVE_GPENCIL);
  RNA_def_property_ui_text(
      prop, "Fade Grease Pencil Objects", "Fade Grease Pencil Objects, except the active one");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "use_gpencil_canvas_xray", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gp_flag", V3D_GP_SHOW_GRID_XRAY);
  RNA_def_property_ui_text(prop, "Canvas X-Ray", "Show Canvas grid in front");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "use_gpencil_show_directions", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gp_flag", V3D_GP_SHOW_STROKE_DIRECTION);
  RNA_def_property_ui_text(prop,
                           "Stroke Direction",
                           "Show stroke drawing direction with a bigger green dot (start) "
                           "and smaller red dot (end) points");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "use_gpencil_show_material_name", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gp_flag", V3D_GP_SHOW_MATERIAL_NAME);
  RNA_def_property_ui_text(
      prop, "Stroke Material Name", "Show material name assigned to each stroke");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "gpencil_grid_opacity", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "overlay.gpencil_grid_opacity");
  RNA_def_property_range(prop, 0.1f, 1.0f);
  RNA_def_property_ui_text(prop, "Opacity", "Canvas grid opacity");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "gpencil_grid_color", PROP_FLOAT, PROP_COLOR);
  RNA_def_property_float_sdna(prop, nullptr, "overlay.gpencil_grid_color");
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Grid Color", "Canvas grid color");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "gpencil_grid_scale", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "overlay.gpencil_grid_scale");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "Scale", "Canvas grid scale");
  RNA_def_property_range(prop, 0.0f, FLT_MAX);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "gpencil_grid_offset", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_float_sdna(prop, nullptr, "overlay.gpencil_grid_offset");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "Offset", "Canvas grid offset");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "gpencil_grid_subdivisions", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "overlay.gpencil_grid_subdivisions");
  RNA_def_property_range(prop, 1, 100);
  RNA_def_property_ui_text(prop, "Subdivisions", "Canvas grid subdivisions");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  /* Paper opacity factor */
  prop = RNA_def_property(srna, "gpencil_fade_objects", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "overlay.gpencil_paper_opacity");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Opacity", "Fade factor");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  /* Paper opacity factor */
  prop = RNA_def_property(srna, "gpencil_fade_layer", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "overlay.gpencil_fade_layer");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_ui_text(
      prop, "Opacity", "Fade layer opacity for Grease Pencil layers except the active one");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  /* show edit lines */
  prop = RNA_def_property(srna, "use_gpencil_edit_lines", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gp_flag", V3D_GP_SHOW_EDIT_LINES);
  RNA_def_property_ui_text(prop, "Show Edit Lines", "Show Edit Lines when editing strokes");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "use_gpencil_multiedit_line_only", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gp_flag", V3D_GP_SHOW_MULTIEDIT_LINES);
  RNA_def_property_ui_text(prop, "Lines Only", "Show Edit Lines only in multiframe");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  /* main grease pencil onion switch */
  prop = RNA_def_property(srna, "use_gpencil_onion_skin", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gp_flag", V3D_GP_SHOW_ONION_SKIN);
  RNA_def_property_ui_text(
      prop, "Onion Skins", "Show ghosts of the keyframes before and after the current frame");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  /* Show onion skin for active object only. */
  prop = RNA_def_property(srna, "use_gpencil_onion_skin_active_object", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gp_flag", V3D_GP_ONION_SKIN_ACTIVE_OBJECT);
  RNA_def_property_ui_text(
      prop, "Active Object Only", "Show only the onion skins of the active object");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  /* vertex opacity */
  prop = RNA_def_property(srna, "vertex_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "vertex_opacity");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Vertex Opacity", "Opacity for edit vertices");
  RNA_def_parameter_clear_flags(prop, PROP_ANIMATABLE, ParameterFlag(0));
  RNA_def_property_update(prop, NC_SCENE | ND_TOOLSETTINGS, nullptr);

  /* Vertex Paint opacity factor */
  prop = RNA_def_property(srna, "gpencil_vertex_paint_opacity", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "overlay.gpencil_vertex_paint_opacity");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Opacity", "Vertex Paint mix factor");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  /* Developer Debug overlay */

  prop = RNA_def_property(srna, "use_debug_freeze_view_culling", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "debug_flag", V3D_DEBUG_FREEZE_CULLING);
  RNA_def_property_ui_text(prop, "Freeze Culling", "Freeze view culling bounds");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);
}

static void rna_def_space_view3d(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem rv3d_persp_items[] = {
      {RV3D_PERSP, "PERSP", 0, "Perspective", ""},
      {RV3D_ORTHO, "ORTHO", 0, "Orthographic", ""},
      {RV3D_CAMOB, "CAMERA", 0, "Camera", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem bundle_drawtype_items[] = {
      {OB_PLAINAXES, "PLAIN_AXES", 0, "Plain Axes", ""},
      {OB_ARROWS, "ARROWS", 0, "Arrows", ""},
      {OB_SINGLE_ARROW, "SINGLE_ARROW", 0, "Single Arrow", ""},
      {OB_CIRCLE, "CIRCLE", 0, "Circle", ""},
      {OB_CUBE, "CUBE", 0, "Cube", ""},
      {OB_EMPTY_SPHERE, "SPHERE", 0, "Sphere", ""},
      {OB_EMPTY_CONE, "CONE", 0, "Cone", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "SpaceView3D", "Space");
  RNA_def_struct_sdna(srna, "View3D");
  RNA_def_struct_ui_text(srna, "3D View Space", "3D View space data");

  rna_def_space_generic_show_region_toggles(srna,
                                            ((1 << RGN_TYPE_TOOL_HEADER) | (1 << RGN_TYPE_TOOLS) |
                                             (1 << RGN_TYPE_UI) | (1 << RGN_TYPE_HUD) |
                                             (1 << RGN_TYPE_ASSET_SHELF)));

  prop = RNA_def_property(srna, "camera", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_sdna(prop, nullptr, "camera");
  RNA_def_property_ui_text(
      prop,
      "Camera",
      "Active camera used in this view (when unlocked from the scene's active camera)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_SpaceView3D_camera_update");

  /* render border */
  prop = RNA_def_property(srna, "use_render_border", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", V3D_RENDER_BORDER);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  RNA_def_property_ui_text(prop,
                           "Render Region",
                           "Use a region within the frame size for rendered viewport "
                           "(when not viewing through the camera)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "render_border_min_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "render_border.xmin");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Region Minimum X", "Minimum X value for the render region");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "render_border_min_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "render_border.ymin");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Region Minimum Y", "Minimum Y value for the render region");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "render_border_max_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "render_border.xmax");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Region Maximum X", "Maximum X value for the render region");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "render_border_max_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "render_border.ymax");
  RNA_def_property_range(prop, 0.0f, 1.0f);
  RNA_def_property_ui_text(prop, "Region Maximum Y", "Maximum Y value for the render region");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "lock_object", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_pointer_sdna(prop, nullptr, "ob_center");
  RNA_def_property_ui_text(
      prop, "Lock to Object", "3D View center is locked to this object's position");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "lock_bone", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "ob_center_bone");
  RNA_def_property_ui_text(
      prop, "Lock to Bone", "3D View center is locked to this bone's position");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "lock_cursor", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "ob_center_cursor", 1);
  RNA_def_property_ui_text(
      prop, "Lock to Cursor", "3D View center is locked to the cursor's position");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "local_view", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "localvd");
  RNA_def_property_ui_text(
      prop,
      "Local View",
      "Display an isolated subset of objects, apart from the scene visibility");

  prop = RNA_def_property(srna, "lens", PROP_FLOAT, PROP_UNIT_CAMERA);
  RNA_def_property_float_sdna(prop, nullptr, "lens");
  RNA_def_property_ui_text(prop, "Lens", "Viewport lens angle");
  RNA_def_property_range(prop, 1.0f, 250.0f);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "clip_start", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_range(prop, 1e-6f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 10, 3);
  RNA_def_property_ui_text(
      prop, "Clip Start", "3D View near clipping distance (perspective view only)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "clip_end", PROP_FLOAT, PROP_DISTANCE);
  RNA_def_property_range(prop, 1e-6f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 10, 3);
  RNA_def_property_ui_text(prop, "Clip End", "3D View far clipping distance");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "lock_camera", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", V3D_LOCK_CAMERA);
  RNA_def_property_ui_text(
      prop, "Lock Camera to View", "Enable view navigation within the camera view");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_gizmo", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "gizmo_flag", V3D_GIZMO_HIDE);
  RNA_def_property_ui_text(prop, "Show Gizmo", "Show gizmos of all types");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_gizmo_navigate", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "gizmo_flag", V3D_GIZMO_HIDE_NAVIGATE);
  RNA_def_property_ui_text(prop, "Navigate Gizmo", "Viewport navigation gizmo");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_gizmo_context", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "gizmo_flag", V3D_GIZMO_HIDE_CONTEXT);
  RNA_def_property_ui_text(prop, "Context Gizmo", "Context sensitive gizmos for the active item");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_gizmo_modifier", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "gizmo_flag", V3D_GIZMO_HIDE_MODIFIER);
  RNA_def_property_ui_text(prop, "Modifier Gizmo", "Gizmos for the active modifier");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_gizmo_tool", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "gizmo_flag", V3D_GIZMO_HIDE_TOOL);
  RNA_def_property_ui_text(prop, "Tool Gizmo", "Active tool gizmo");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  /* Per object type gizmo display flags. */

  prop = RNA_def_property(srna, "show_gizmo_object_translate", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "gizmo_show_object", V3D_GIZMO_SHOW_OBJECT_TRANSLATE);
  RNA_def_property_ui_text(prop, "Show Object Location", "Gizmo to adjust location");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_gizmo_object_rotate", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gizmo_show_object", V3D_GIZMO_SHOW_OBJECT_ROTATE);
  RNA_def_property_ui_text(prop, "Show Object Rotation", "Gizmo to adjust rotation");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_gizmo_object_scale", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gizmo_show_object", V3D_GIZMO_SHOW_OBJECT_SCALE);
  RNA_def_property_ui_text(prop, "Show Object Scale", "Gizmo to adjust scale");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  /* Empty Object Data. */
  prop = RNA_def_property(srna, "show_gizmo_empty_image", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gizmo_show_empty", V3D_GIZMO_SHOW_EMPTY_IMAGE);
  RNA_def_property_ui_text(prop, "Show Empty Image", "Gizmo to adjust image size and position");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_gizmo_empty_force_field", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "gizmo_show_empty", V3D_GIZMO_SHOW_EMPTY_FORCE_FIELD);
  RNA_def_property_ui_text(prop, "Show Empty Force Field", "Gizmo to adjust the force field");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  /* Light Object Data. */
  prop = RNA_def_property(srna, "show_gizmo_light_size", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gizmo_show_light", V3D_GIZMO_SHOW_LIGHT_SIZE);
  RNA_def_property_ui_text(prop, "Show Light Size", "Gizmo to adjust spot and area size");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_gizmo_light_look_at", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gizmo_show_light", V3D_GIZMO_SHOW_LIGHT_LOOK_AT);
  RNA_def_property_ui_text(
      prop, "Show Light Look-At", "Gizmo to adjust the direction of the light");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  /* Camera Object Data. */
  prop = RNA_def_property(srna, "show_gizmo_camera_lens", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "gizmo_show_camera", V3D_GIZMO_SHOW_CAMERA_LENS);
  RNA_def_property_ui_text(
      prop, "Show Camera Lens", "Gizmo to adjust camera focal length or orthographic scale");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_gizmo_camera_dof_distance", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "gizmo_show_camera", V3D_GIZMO_SHOW_CAMERA_DOF_DIST);
  RNA_def_property_ui_text(prop,
                           "Show Camera Focus Distance",
                           "Gizmo to adjust camera focus distance "
                           "(depends on limits display)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "use_local_camera", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "scenelock", 1);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_SpaceView3D_use_local_camera_set");
  RNA_def_property_ui_text(prop,
                           "Use Local Camera",
                           "Use a local camera in this view, rather than scene's active camera");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "region_3d", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "RegionView3D");
  RNA_def_property_pointer_funcs(prop, "rna_SpaceView3D_region_3d_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(
      prop,
      "3D Region",
      "3D region for this space. When the space is in quad view, the camera region");

  prop = RNA_def_property(srna, "region_quadviews", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "RegionView3D");
  RNA_def_property_collection_funcs(prop,
                                    "rna_SpaceView3D_region_quadviews_begin",
                                    "rna_iterator_listbase_next",
                                    "rna_iterator_listbase_end",
                                    "rna_SpaceView3D_region_quadviews_get",
                                    nullptr,
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop,
                           "Quad View Regions",
                           "3D regions (the third one defines quad view settings, "
                           "the fourth one is same as 'region_3d')");

  prop = RNA_def_property(srna, "show_reconstruction", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", V3D_SHOW_RECONSTRUCTION);
  RNA_def_property_ui_text(
      prop, "Show Reconstruction", "Display reconstruction data from active movie clip");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "tracks_display_size", PROP_FLOAT, PROP_NONE);
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_ui_range(prop, 0, 5, 1, 3);
  RNA_def_property_float_sdna(prop, nullptr, "bundle_size");
  RNA_def_property_ui_text(prop, "Tracks Size", "Display size of tracks from reconstructed data");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "tracks_display_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "bundle_drawtype");
  RNA_def_property_enum_items(prop, bundle_drawtype_items);
  RNA_def_property_ui_text(prop, "Tracks Display Type", "Viewport display style for tracks");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_camera_path", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", V3D_SHOW_CAMERAPATH);
  RNA_def_property_ui_text(prop, "Show Camera Path", "Show reconstructed camera path");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_bundle_names", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", V3D_SHOW_BUNDLENAME);
  RNA_def_property_ui_text(
      prop, "Show 3D Marker Names", "Show names for reconstructed tracks objects");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "use_local_collections", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", V3D_LOCAL_COLLECTIONS);
  RNA_def_property_ui_text(
      prop, "Local Collections", "Display a different set of collections in this viewport");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_SpaceView3D_use_local_collections_update");

  /* Stereo Settings */
  prop = RNA_def_property(srna, "stereo_3d_eye", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "multiview_eye");
  RNA_def_property_enum_items(prop, stereo3d_eye_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_SpaceView3D_stereo3d_camera_itemf");
  RNA_def_property_ui_text(prop, "Stereo Eye", "Current stereo eye being displayed");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "stereo_3d_camera", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "stereo3d_camera");
  RNA_def_property_enum_items(prop, stereo3d_camera_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_SpaceView3D_stereo3d_camera_itemf");
  RNA_def_property_ui_text(prop, "Camera", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_stereo_3d_cameras", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "stereo3d_flag", V3D_S3D_DISPCAMERAS);
  RNA_def_property_ui_text(prop, "Cameras", "Show the left and right cameras");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_stereo_3d_convergence_plane", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "stereo3d_flag", V3D_S3D_DISPPLANE);
  RNA_def_property_ui_text(prop, "Plane", "Show the stereo 3D convergence plane");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "stereo_3d_convergence_plane_alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "stereo3d_convergence_alpha");
  RNA_def_property_ui_text(prop, "Plane Alpha", "Opacity (alpha) of the convergence plane");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "show_stereo_3d_volume", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "stereo3d_flag", V3D_S3D_DISPVOLUME);
  RNA_def_property_ui_text(prop, "Volume", "Show the stereo 3D frustum volume");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "stereo_3d_volume_alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "stereo3d_volume_alpha");
  RNA_def_property_ui_text(prop, "Volume Alpha", "Opacity (alpha) of the cameras' frustum volume");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "mirror_xr_session", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", V3D_XR_SESSION_MIRROR);
  RNA_def_property_ui_text(
      prop,
      "Mirror VR Session",
      "Synchronize the viewer perspective of virtual reality sessions with this 3D viewport");
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_SpaceView3D_mirror_xr_session_update");

  rna_def_object_type_visibility_flags_common(srna,
                                              NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING,
                                              "rna_SpaceView3D_object_type_visibility_update");

  /* Helper for drawing the icon. */
  prop = RNA_def_property(srna, "icon_from_show_object_viewport", PROP_INT, PROP_NONE);
  RNA_def_property_int_funcs(
      prop, "rna_SpaceView3D_icon_from_show_object_viewport_get", nullptr, nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Visibility Icon", "");

  prop = RNA_def_property(srna, "show_viewer", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag2", V3D_SHOW_VIEWER);
  RNA_def_property_ui_text(prop, "Show Viewer", "Display non-final geometry from viewer nodes");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D | NS_VIEW3D_SHADING, nullptr);

  /* Nested Structs */
  prop = RNA_def_property(srna, "shading", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "View3DShading");
  RNA_def_property_ui_text(prop, "Shading Settings", "Settings for shading in the 3D viewport");

  prop = RNA_def_property(srna, "overlay", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "View3DOverlay");
  RNA_def_property_pointer_funcs(prop, "rna_SpaceView3D_overlay_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(
      prop, "Overlay Settings", "Settings for display of overlays in the 3D viewport");

  rna_def_space_view3d_shading(brna);
  rna_def_space_view3d_overlay(brna);

  /* *** Animated *** */
  RNA_define_animate_sdna(true);
  /* region */

  srna = RNA_def_struct(brna, "RegionView3D", nullptr);
  RNA_def_struct_sdna(srna, "RegionView3D");
  RNA_def_struct_ui_text(srna, "3D View Region", "3D View region data");

  prop = RNA_def_property(srna, "lock_rotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "viewlock", RV3D_LOCK_ROTATION);
  RNA_def_property_ui_text(
      prop, "Lock Rotation", "Lock view rotation of side views to Top/Front/Right");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_RegionView3D_quadview_update");

  prop = RNA_def_property(srna, "show_sync_view", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "viewlock", RV3D_BOXVIEW);
  RNA_def_property_ui_text(prop, "Sync Zoom/Pan", "Sync view position between side views");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_RegionView3D_quadview_update");

  prop = RNA_def_property(srna, "use_box_clip", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "viewlock", RV3D_BOXCLIP);
  RNA_def_property_ui_text(
      prop, "Clip Contents", "Clip view contents based on what is visible in other side views");
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_VIEW3D, "rna_RegionView3D_quadview_clip_update");

  prop = RNA_def_property(srna, "perspective_matrix", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_float_sdna(prop, nullptr, "persmat");
  RNA_def_property_clear_flag(
      prop, PROP_EDITABLE); /* XXX: for now, it's too risky for users to do this */
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(
      prop, "Perspective Matrix", "Current perspective matrix (``window_matrix * view_matrix``)");

  prop = RNA_def_property(srna, "window_matrix", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_float_sdna(prop, nullptr, "winmat");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_ui_text(prop, "Window Matrix", "Current window matrix");

  prop = RNA_def_property(srna, "view_matrix", PROP_FLOAT, PROP_MATRIX);
  RNA_def_property_float_sdna(prop, nullptr, "viewmat");
  RNA_def_property_multi_array(prop, 2, rna_matrix_dimsize_4x4);
  RNA_def_property_float_funcs(prop, nullptr, "rna_RegionView3D_view_matrix_set", nullptr);
  RNA_def_property_ui_text(prop, "View Matrix", "Current view matrix");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "view_perspective", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "persp");
  RNA_def_property_enum_items(prop, rv3d_persp_items);
  RNA_def_property_ui_text(prop, "Perspective", "View Perspective");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "is_perspective", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "is_persp", 1);
  RNA_def_property_ui_text(prop, "Is Perspective", "");
  RNA_def_property_flag(prop, PROP_EDITABLE);

  /* WARNING: Using "orthographic" in this name isn't correct and could be changed. */
  prop = RNA_def_property(srna, "is_orthographic_side_view", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "view", 0);
  RNA_def_property_boolean_funcs(prop,
                                 "rna_RegionView3D_is_orthographic_side_view_get",
                                 "rna_RegionView3D_is_orthographic_side_view_set");
  RNA_def_property_ui_text(
      prop,
      "Is Axis Aligned",
      "Whether the current view is aligned to an axis "
      "(does not check whether the view is orthographic, use \"is_perspective\" for that). "
      "Setting this will rotate the view to the closest axis");

  /* This isn't directly accessible from the UI, only an operator. */
  prop = RNA_def_property(srna, "use_clip_planes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "rflag", RV3D_CLIPPING);
  RNA_def_property_ui_text(prop, "Use Clip Planes", "");

  const int default_value[] = {6, 4};
  prop = RNA_def_property(srna, "clip_planes", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "clip");
  RNA_def_property_multi_array(prop, 2, default_value);
  RNA_def_property_ui_text(prop, "Clip Planes", "");

  prop = RNA_def_property(srna, "view_location", PROP_FLOAT, PROP_TRANSLATION);
#  if 0
  RNA_def_property_float_sdna(prop, nullptr, "ofs"); /* can't use because it's negated */
#  else
  RNA_def_property_array(prop, 3);
  RNA_def_property_float_funcs(
      prop, "rna_RegionView3D_view_location_get", "rna_RegionView3D_view_location_set", nullptr);
#  endif
  RNA_def_property_ui_text(prop, "View Location", "View pivot location");
  RNA_def_property_ui_range(prop, -10000.0, 10000.0, 10, RNA_TRANSLATION_PREC_DEFAULT);
  RNA_def_property_update(prop, NC_WINDOW, nullptr);

  prop = RNA_def_property(
      srna, "view_rotation", PROP_FLOAT, PROP_QUATERNION); /* can't use because it's inverted */
#  if 0
  RNA_def_property_float_sdna(prop, nullptr, "viewquat");
#  else
  RNA_def_property_array(prop, 4);
  RNA_def_property_float_funcs(
      prop, "rna_RegionView3D_view_rotation_get", "rna_RegionView3D_view_rotation_set", nullptr);
#  endif
  RNA_def_property_ui_text(prop, "View Rotation", "Rotation in quaternions (keep normalized)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  /* not sure we need rna access to these but adding anyway */
  prop = RNA_def_property(srna, "view_distance", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "dist");
  RNA_def_property_ui_text(prop, "Distance", "Distance to the view location");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "view_camera_zoom", PROP_FLOAT, PROP_UNSIGNED);
  RNA_def_property_float_sdna(prop, nullptr, "camzoom");
  RNA_def_property_ui_text(prop, "Camera Zoom", "Zoom factor in camera view");
  RNA_def_property_range(prop, RV3D_CAMZOOM_MIN, RV3D_CAMZOOM_MAX);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  prop = RNA_def_property(srna, "view_camera_offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "camdx");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "Camera Offset", "View shift in camera view");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_VIEW3D, nullptr);

  RNA_api_region_view3d(srna);
}

static void rna_def_space_properties_filter(StructRNA *srna)
{
  /* Order must follow `buttons_context_items`. */
  constexpr std::array<blender::StringRefNull, BCONTEXT_TOT> filter_items = {
      "show_properties_tool",
      "show_properties_scene",
      "show_properties_render",
      "show_properties_output",
      "show_properties_view_layer",
      "show_properties_world",
      "show_properties_collection",
      "show_properties_object",
      "show_properties_constraints",
      "show_properties_modifiers",
      "show_properties_data",
      "show_properties_bone",
      "show_properties_bone_constraints",
      "show_properties_material",
      "show_properties_texture",
      "show_properties_particles",
      "show_properties_physics",
      "show_properties_effects",
      "show_properties_strip",
      "show_properties_strip_modifier",
  };

  for (const int i : blender::IndexRange(BCONTEXT_TOT)) {
    EnumPropertyItem item = buttons_context_items[i];
    const int value = (1 << item.value);
    blender::StringRefNull prop_name = filter_items[i];

    PropertyRNA *prop = RNA_def_property(srna, prop_name.c_str(), PROP_BOOLEAN, PROP_NONE);
    RNA_def_property_boolean_sdna(prop, nullptr, "visible_tabs", value);
    RNA_def_property_ui_text(prop, item.name, "");
    RNA_def_property_update(
        prop, NC_SPACE | ND_SPACE_PROPERTIES, "rna_SpaceProperties_context_update");
  }
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
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "SpaceProperties", "Space");
  RNA_def_struct_sdna(srna, "SpaceProperties");
  RNA_def_struct_ui_text(srna, "Properties Space", "Properties space data");

  prop = RNA_def_property(srna, "context", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mainb");
  RNA_def_property_enum_items(prop, buttons_context_items);
  RNA_def_property_enum_funcs(
      prop, nullptr, "rna_SpaceProperties_context_set", "rna_SpaceProperties_context_itemf");
  RNA_def_property_ui_text(prop, "", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ID);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_PROPERTIES, "rna_SpaceProperties_context_update");

  rna_def_space_properties_filter(srna);

  /* pinned data */
  prop = RNA_def_property(srna, "pin_id", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "pinid");
  RNA_def_property_struct_type(prop, "ID");
  RNA_def_property_pointer_funcs(
      prop, nullptr, nullptr, "rna_SpaceProperties_pin_id_typef", nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_NEVER_UNLINK);
  RNA_def_property_clear_flag(prop, PROP_ID_REFCOUNT);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_PROPERTIES, "rna_SpaceProperties_pin_id_update");

  prop = RNA_def_property(srna, "use_pin_id", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SB_PIN_CONTEXT);
  RNA_def_property_ui_text(prop, "Pin ID", "Use the pinned context");

  /* Property search. */

  prop = RNA_def_property(srna, "tab_search_results", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_array(prop, 0); /* Dynamic length, see next line. */
  RNA_def_property_flag(prop, PROP_DYNAMIC);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_SpaceProperties_tab_search_results_get", nullptr);
  RNA_def_property_dynamic_array_funcs(prop, "rna_SpaceProperties_tab_search_results_getlength");
  RNA_def_property_ui_text(
      prop, "Tab Search Results", "Whether or not each visible tab has a search result");

  prop = RNA_def_property(srna, "search_filter", PROP_STRING, PROP_NONE);
  /* The search filter is stored in the property editor's runtime which
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
  RNA_def_property_enum_sdna(prop, nullptr, "outliner_sync");
  RNA_def_property_enum_items(prop, tab_sync_items);
  RNA_def_property_ui_text(prop,
                           "Outliner Sync",
                           "Change to the corresponding tab when outliner data icons are clicked");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_PROPERTIES, nullptr);
}

static void rna_def_space_image_overlay(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SpaceImageOverlay", nullptr);
  RNA_def_struct_sdna(srna, "SpaceImage");
  RNA_def_struct_nested(brna, srna, "SpaceImageEditor");
  RNA_def_struct_path_func(srna, "rna_SpaceImageOverlay_path");
  RNA_def_struct_ui_text(
      srna, "Overlay Settings", "Settings for display of overlays in the UV/Image editor");

  prop = RNA_def_property(srna, "show_overlays", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", SI_OVERLAY_SHOW_OVERLAYS);
  RNA_def_property_ui_text(prop, "Show Overlays", "Display overlays like UV Maps and Metadata");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "show_grid_background", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", SI_OVERLAY_SHOW_GRID_BACKGROUND);
  RNA_def_property_ui_text(prop, "Display Background", "Show the grid background and borders");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "show_render_size", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", SI_OVERLAY_DRAW_RENDER_REGION);
  RNA_def_property_ui_text(prop, "Render Region", "Display the region of the final render");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "show_text_info", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", SI_OVERLAY_DRAW_TEXT_INFO);
  RNA_def_property_ui_text(prop, "Text Info", "Display overlay text");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "passepartout_alpha", PROP_FLOAT, PROP_FACTOR);
  RNA_def_property_float_sdna(prop, nullptr, "overlay.passepartout_alpha");
  RNA_def_property_float_default(prop, 0.5f);
  RNA_def_property_ui_text(
      prop, "Passepartout Alpha", "Opacity of the darkened overlay outside the render region");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);
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
                                             (1 << RGN_TYPE_UI) | (1 << RGN_TYPE_HUD) |
                                             (1 << RGN_TYPE_ASSET_SHELF)));

  /* image */
  prop = RNA_def_property(srna, "image", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_funcs(
      prop, nullptr, "rna_SpaceImageEditor_image_set", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Image", "Image displayed and edited in this space");
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_clear_flag(prop, PROP_ID_REFCOUNT);
  RNA_def_property_update(
      prop,
      NC_GEOM | ND_DATA,
      "rna_SpaceImageEditor_image_update"); /* is handled in image editor too */

  prop = RNA_def_property(srna, "image_user", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_pointer_sdna(prop, nullptr, "iuser");
  RNA_def_property_ui_text(
      prop,
      "Image User",
      "Parameters defining which layer, pass and frame of the image is displayed");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "scopes", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "scopes");
  RNA_def_property_struct_type(prop, "Scopes");
  RNA_def_property_ui_text(prop, "Scopes", "Scopes to visualize image statistics");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, "rna_SpaceImageEditor_scopes_update");

  prop = RNA_def_property(srna, "use_image_pin", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "pin", 0);
  RNA_def_property_ui_text(
      prop, "Image Pin", "Display current image regardless of object selection");
  RNA_def_property_ui_icon(prop, ICON_UNPINNED, 1);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "sample_histogram", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "sample_line_hist");
  RNA_def_property_struct_type(prop, "Histogram");
  RNA_def_property_ui_text(prop, "Line Sample", "Sampled colors along line");

  prop = RNA_def_property(srna, "zoom", PROP_FLOAT, PROP_NONE);
  RNA_def_property_array(prop, 2);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_float_funcs(prop, "rna_SpaceImageEditor_zoom_get", nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Zoom", "Zoom factor");

  prop = RNA_def_property(srna, "zoom_percentage", PROP_FLOAT, PROP_PERCENTAGE);
  RNA_def_property_float_funcs(prop,
                               "rna_SpaceImageEditor_zoom_percentage_get",
                               "rna_SpaceImageEditor_zoom_percentage_set",
                               nullptr);
  RNA_def_property_float_default(prop, 100.0);
  RNA_def_property_range(prop, .4, 80000);
  RNA_def_property_ui_range(prop, 25, 400, 100, 0);
  RNA_def_property_ui_text(prop, "Zoom", "Zoom percentage");

  /* image draw */
  prop = RNA_def_property(srna, "show_repeat", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SI_DRAW_TILE);
  RNA_def_property_ui_text(
      prop, "Display Repeated", "Display the image repeated outside of the main view");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "show_annotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SI_SHOW_GPENCIL);
  RNA_def_property_ui_text(prop, "Show Annotation", "Show annotations for this view");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "display_channels", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flag");
  RNA_def_property_enum_items(prop, display_channels_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_SpaceImageEditor_display_channels_get",
                              nullptr,
                              "rna_SpaceImageEditor_display_channels_itemf");
  RNA_def_property_ui_text(prop, "Display Channels", "Channels of the image to display");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "show_stereo_3d", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(
      prop, "rna_SpaceImageEditor_show_stereo_get", "rna_SpaceImageEditor_show_stereo_set");
  RNA_def_property_ui_text(prop, "Show Stereo", "Display the image in Stereo 3D");
  RNA_def_property_ui_icon(prop, ICON_CAMERA_STEREO, 0);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_IMAGE, "rna_SpaceImageEditor_show_stereo_update");

  prop = RNA_def_property(srna, "show_sequencer_scene", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop,
                                 "rna_SpaceImageEditor_show_sequencer_scene_get",
                                 "rna_SpaceImageEditor_show_sequencer_scene_set");
  RNA_def_property_ui_text(
      prop,
      "Show Sequencer Scene",
      "Display the render result for the sequencer scene instead of the active scene");
  RNA_def_property_ui_icon(prop, ICON_SEQ_SEQUENCER, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  /* uv */
  prop = RNA_def_property(srna, "uv_editor", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "SpaceUVEditor");
  RNA_def_property_pointer_funcs(
      prop, "rna_SpaceImageEditor_uvedit_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "UV Editor", "UV editor settings");

  /* mode (hidden in the UI, see 'ui_mode') */
  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mode");
  RNA_def_property_enum_items(prop, rna_enum_space_image_mode_all_items);
  RNA_def_property_ui_text(prop, "Mode", "Editing context being displayed");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, "rna_SpaceImageEditor_mode_update");

  prop = RNA_def_property(srna, "ui_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mode");
  RNA_def_property_enum_items(prop, rna_enum_space_image_mode_ui_items);
  RNA_def_property_ui_text(prop, "Mode", "Editing context being displayed");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, "rna_SpaceImageEditor_mode_update");

  /* transform */
  prop = RNA_def_property(srna, "cursor_location", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_array(prop, 2);
  RNA_def_property_float_funcs(prop,
                               "rna_SpaceImageEditor_cursor_location_get",
                               "rna_SpaceImageEditor_cursor_location_set",
                               nullptr);
  RNA_def_property_ui_text(prop, "2D Cursor Location", "2D cursor location for this view");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "pivot_point", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "around");
  RNA_def_property_enum_items(prop, rna_enum_transform_pivot_full_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_SpaceImageEditor_pivot_itemf");
  RNA_def_property_ui_text(prop, "Pivot", "Rotation/Scaling Pivot");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  /* Annotations */
  prop = RNA_def_property(srna, "annotation", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "gpd");
  RNA_def_property_struct_type(prop, "Annotation");
  RNA_def_property_pointer_funcs(
      prop, nullptr, nullptr, nullptr, "rna_GPencil_datablocks_annotations_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_ui_text(prop, "Annotation", "Annotation data for this space");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  /* update */
  prop = RNA_def_property(srna, "use_realtime_update", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "lock", 0);
  RNA_def_property_ui_text(prop,
                           "Update Automatically",
                           "Update other affected window spaces automatically to reflect changes "
                           "during interactive operations such as transform");

  /* state */
  prop = RNA_def_property(srna, "show_render", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_SpaceImageEditor_show_render_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Show Render", "Show render related properties");

  prop = RNA_def_property(srna, "show_paint", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_SpaceImageEditor_show_paint_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Show Paint", "Show paint related properties");

  prop = RNA_def_property(srna, "show_uvedit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_SpaceImageEditor_show_uvedit_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Show UV Editor", "Show UV editing related properties");

  prop = RNA_def_property(srna, "show_maskedit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_SpaceImageEditor_show_maskedit_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Show Mask Editor", "Show Mask editing related properties");

  /* Gizmo Toggles. */
  prop = RNA_def_property(srna, "show_gizmo", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "gizmo_flag", SI_GIZMO_HIDE);
  RNA_def_property_ui_text(prop, "Show Gizmo", "Show gizmos of all types");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  prop = RNA_def_property(srna, "show_gizmo_navigate", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "gizmo_flag", SI_GIZMO_HIDE_NAVIGATE);
  RNA_def_property_ui_text(prop, "Navigate Gizmo", "Viewport navigation gizmo");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_IMAGE, nullptr);

  /* Overlays */
  prop = RNA_def_property(srna, "overlay", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "SpaceImageOverlay");
  RNA_def_property_pointer_funcs(prop, "rna_SpaceImage_overlay_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(
      prop, "Overlay Settings", "Settings for display of overlays in the UV/Image editor");

  rna_def_space_image_uv(brna);
  rna_def_space_image_overlay(brna);

  /* mask */
  rna_def_space_mask_info(srna, NC_SPACE | ND_SPACE_IMAGE, "rna_SpaceImageEditor_mask_set");
}

static void rna_def_space_sequencer_preview_overlay(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SequencerPreviewOverlay", nullptr);
  RNA_def_struct_sdna(srna, "SpaceSeq");
  RNA_def_struct_nested(brna, srna, "SpaceSequenceEditor");
  RNA_def_struct_path_func(srna, "rna_SpaceSequencerPreviewOverlay_path");
  RNA_def_struct_ui_text(srna, "Preview Overlay Settings", "");

  prop = RNA_def_property(srna, "show_safe_areas", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "preview_overlay.flag", SEQ_PREVIEW_SHOW_SAFE_MARGINS);
  RNA_def_property_ui_text(
      prop, "Safe Areas", "Show TV title safe and action safe areas in preview");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_safe_center", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "preview_overlay.flag", SEQ_PREVIEW_SHOW_SAFE_CENTER);
  RNA_def_property_ui_text(
      prop, "Center-Cut Safe Areas", "Show safe areas to fit content in a different aspect ratio");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_metadata", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "preview_overlay.flag", SEQ_PREVIEW_SHOW_METADATA);
  RNA_def_property_ui_text(prop, "Show Metadata", "Show metadata of first visible strip");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_annotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "preview_overlay.flag", SEQ_PREVIEW_SHOW_GPENCIL);
  RNA_def_property_ui_text(prop, "Show Annotation", "Show annotations for this view");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_image_outline", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "preview_overlay.flag", SEQ_PREVIEW_SHOW_OUTLINE_SELECTED);
  RNA_def_property_ui_text(prop, "Image Outline", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_cursor", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "preview_overlay.flag", SEQ_PREVIEW_SHOW_2D_CURSOR);
  RNA_def_property_ui_text(prop, "2D Cursor", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);
}

static void rna_def_space_sequencer_timeline_overlay(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SequencerTimelineOverlay", nullptr);
  RNA_def_struct_sdna(srna, "SpaceSeq");
  RNA_def_struct_nested(brna, srna, "SpaceSequenceEditor");
  RNA_def_struct_path_func(srna, "rna_SpaceSequencerTimelineOverlay_path");
  RNA_def_struct_ui_text(srna, "Timeline Overlay Settings", "");

  static const EnumPropertyItem waveform_type_display_items[] = {
      {SEQ_TIMELINE_ALL_WAVEFORMS,
       "ALL_WAVEFORMS",
       0,
       "On",
       "Display waveforms for all sound strips"},
      {0, "DEFAULT_WAVEFORMS", 0, "Strip", "Display waveforms depending on strip setting"},
      {SEQ_TIMELINE_NO_WAVEFORMS,
       "NO_WAVEFORMS",
       0,
       "Off",
       "Don't display waveforms for any sound strips"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "waveform_display_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "timeline_overlay.flag");
  RNA_def_property_enum_items(prop, waveform_type_display_items);
  RNA_def_property_ui_text(prop, "Waveform Display", "How Waveforms are displayed");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  static const EnumPropertyItem waveform_style_display_items[] = {
      {0, "FULL_WAVEFORMS", 0, "Full", "Display full waveform"},
      {SEQ_TIMELINE_WAVEFORMS_HALF,
       "HALF_WAVEFORMS",
       0,
       "Half",
       "Display upper half of the absolute value waveform"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  prop = RNA_def_property(srna, "waveform_display_style", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "timeline_overlay.flag");
  RNA_def_property_enum_items(prop, waveform_style_display_items);
  RNA_def_property_ui_text(prop, "Waveform Style", "How Waveforms are displayed");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_fcurves", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "timeline_overlay.flag", SEQ_TIMELINE_SHOW_FCURVES);
  RNA_def_property_ui_text(prop, "Show F-Curves", "Display strip opacity/volume curve");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_strip_name", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "timeline_overlay.flag", SEQ_TIMELINE_SHOW_STRIP_NAME);
  RNA_def_property_ui_text(prop, "Show Name", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_strip_source", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "timeline_overlay.flag", SEQ_TIMELINE_SHOW_STRIP_SOURCE);
  RNA_def_property_ui_text(
      prop, "Show Source", "Display path to source file, or name of source data-block");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_strip_duration", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "timeline_overlay.flag", SEQ_TIMELINE_SHOW_STRIP_DURATION);
  RNA_def_property_ui_text(prop, "Show Duration", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_grid", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "timeline_overlay.flag", SEQ_TIMELINE_SHOW_GRID);
  RNA_def_property_ui_text(prop, "Show Grid", "Show vertical grid lines");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_strip_offset", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "timeline_overlay.flag", SEQ_TIMELINE_SHOW_STRIP_OFFSETS);
  RNA_def_property_ui_text(prop, "Show Offsets", "Display strip in/out offsets");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_thumbnails", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "timeline_overlay.flag", SEQ_TIMELINE_SHOW_THUMBNAILS);
  RNA_def_property_ui_text(prop, "Show Thumbnails", "Show strip thumbnails");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_strip_tag_color", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "timeline_overlay.flag", SEQ_TIMELINE_SHOW_STRIP_COLOR_TAG);
  RNA_def_property_ui_text(
      prop, "Show Color Tags", "Display the strip color tags in the sequencer");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_strip_retiming", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "timeline_overlay.flag", SEQ_TIMELINE_SHOW_STRIP_RETIMING);
  RNA_def_property_ui_text(prop, "Show Retiming Keys", "Display retiming keys on top of strips");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);
}

static void rna_def_space_sequencer_cache_overlay(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SequencerCacheOverlay", nullptr);
  RNA_def_struct_sdna(srna, "SpaceSeq");
  RNA_def_struct_nested(brna, srna, "SpaceSequenceEditor");
  RNA_def_struct_path_func(srna, "rna_SpaceSequencerCacheOverlay_path");
  RNA_def_struct_ui_text(srna, "Cache Overlay Settings", "");

  prop = RNA_def_property(srna, "show_cache", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_overlay.flag", SEQ_CACHE_SHOW);
  RNA_def_property_ui_text(prop, "Show Cache", "Visualize cached images on the timeline");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_cache_final_out", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_overlay.flag", SEQ_CACHE_SHOW_FINAL_OUT);
  RNA_def_property_ui_text(prop, "Final Images", "Visualize cached complete frames");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_cache_raw", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_overlay.flag", SEQ_CACHE_SHOW_RAW);
  RNA_def_property_ui_text(prop, "Raw Images", "Visualize cached raw images");
  RNA_def_property_update(prop, NC_SCENE | ND_SEQUENCER, nullptr);
}

static void rna_def_space_sequencer(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem display_mode_items[] = {
      {SEQ_DRAW_IMG_IMBUF, "IMAGE", ICON_SEQ_PREVIEW, "Image Preview", ""},
      {SEQ_DRAW_IMG_WAVEFORM, "WAVEFORM", ICON_SEQ_LUMA_WAVEFORM, "Luma Waveform", ""},
      {SEQ_DRAW_IMG_RGBPARADE, "RGB_PARADE", ICON_RENDERLAYERS, "RGB Parade", ""},
      {SEQ_DRAW_IMG_VECTORSCOPE, "VECTOR_SCOPE", ICON_SEQ_CHROMA_SCOPE, "Chroma Vectorscope", ""},
      {SEQ_DRAW_IMG_HISTOGRAM, "HISTOGRAM", ICON_SEQ_HISTOGRAM, "Histogram", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem proxy_render_size_items[] = {
      {SEQ_RENDER_SIZE_NONE, "NONE", 0, "No display", ""},
      {SEQ_RENDER_SIZE_SCENE, "SCENE", 0, "Scene size", ""},
      {SEQ_RENDER_SIZE_PROXY_25, "PROXY_25", 0, "25%", ""},
      {SEQ_RENDER_SIZE_PROXY_50, "PROXY_50", 0, "50%", ""},
      {SEQ_RENDER_SIZE_PROXY_75, "PROXY_75", 0, "75%", ""},
      {SEQ_RENDER_SIZE_PROXY_100, "PROXY_100", 0, "100%", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem overlay_frame_type_items[] = {
      {SEQ_OVERLAY_FRAME_TYPE_RECT, "RECTANGLE", 0, "Rectangle", "Show rectangle area overlay"},
      {SEQ_OVERLAY_FRAME_TYPE_REFERENCE, "REFERENCE", 0, "Reference", "Show reference frame only"},
      {SEQ_OVERLAY_FRAME_TYPE_CURRENT, "CURRENT", 0, "Current", "Show current frame only"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem preview_channels_items[] = {
      {SEQ_USE_ALPHA,
       "COLOR_ALPHA",
       ICON_IMAGE_RGB_ALPHA,
       "Color & Alpha",
       "Display image with RGB colors and alpha transparency"},
      {0, "COLOR", ICON_IMAGE_RGB, "Color", "Display image with RGB colors"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "SpaceSequenceEditor", "Space");
  RNA_def_struct_sdna(srna, "SpaceSeq");
  RNA_def_struct_ui_text(srna, "Space Sequence Editor", "Sequence editor space data");

  rna_def_space_generic_show_region_toggles(srna,
                                            (1 << RGN_TYPE_TOOL_HEADER) | (1 << RGN_TYPE_FOOTER) |
                                                (1 << RGN_TYPE_UI) | (1 << RGN_TYPE_TOOLS) |
                                                (1 << RGN_TYPE_HUD) | (1 << RGN_TYPE_CHANNELS));

  /* view type, fairly important */
  prop = RNA_def_property(srna, "view_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "view");
  RNA_def_property_enum_items(prop, rna_enum_space_sequencer_view_type_items);
  RNA_def_property_ui_text(
      prop, "View Type", "Type of the Sequencer view (sequencer, preview or both)");
  RNA_def_property_update(prop, 0, "rna_Sequencer_view_type_update");

  /* display type, fairly important */
  prop = RNA_def_property(srna, "display_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mainb");
  RNA_def_property_enum_items(prop, display_mode_items);
  RNA_def_property_ui_text(
      prop, "Display Mode", "View mode to use for displaying sequencer output");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  /* flags */
  prop = RNA_def_property(srna, "show_frames", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_DRAWFRAMES);
  RNA_def_property_ui_text(prop, "Display Frames", "Display frames rather than seconds");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "use_marker_sync", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_MARKER_TRANS);
  RNA_def_property_ui_text(prop, "Sync Markers", "Transform markers as well as strips");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_seconds", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", SEQ_DRAWFRAMES);
  RNA_def_property_ui_text(prop, "Use Timecode", "Show timing as a timecode instead of frames");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_markers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_SHOW_MARKERS);
  RNA_def_property_ui_text(
      prop,
      "Show Markers",
      "If any exists, show markers in a separate row at the bottom of the editor");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "display_channel", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "chanshown");
  RNA_def_property_ui_text(
      prop,
      "Display Channel",
      "Preview all channels less than or equal to this value. 0 shows every channel, and negative "
      "values climb that many meta-strip levels if applicable, showing every channel there.");
  RNA_def_property_range(prop, -5, blender::seq::MAX_CHANNELS);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, "rna_SequenceEditor_update_cache");

  prop = RNA_def_property(srna, "preview_channels", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flag");
  RNA_def_property_enum_items(prop, preview_channels_items);
  RNA_def_property_ui_text(prop, "Display Channels", "Channels of the preview to display");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, "rna_SequenceEditor_update_cache");

  prop = RNA_def_property(srna, "use_zoom_to_fit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_ZOOM_TO_FIT);
  RNA_def_property_ui_text(
      prop, "Zoom to Fit", "Automatically zoom preview image to make it fully fit the region");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_overexposed", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "zebra");
  RNA_def_property_ui_text(prop, "Show Overexposed", "Show overexposed areas with zebra stripes");
  RNA_def_property_range(prop, 0, 110);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "proxy_render_size", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "render_size");
  RNA_def_property_enum_items(prop, proxy_render_size_items);
  RNA_def_property_ui_text(prop,
                           "Proxy Render Size",
                           "Display preview using full resolution or different proxy resolutions");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_SEQUENCER, "rna_SequenceEditor_render_size_update");

  prop = RNA_def_property(srna, "use_proxies", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_USE_PROXIES);
  RNA_def_property_ui_text(
      prop, "Use Proxies", "Use optimized files for faster scrubbing when available");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, "rna_SequenceEditor_update_cache");

  prop = RNA_def_property(srna, "use_clamp_view", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_CLAMP_VIEW);
  RNA_def_property_boolean_funcs(
      prop, "rna_SequenceEditor_clamp_view_get", "rna_SequenceEditor_clamp_view_set");
  RNA_def_property_ui_text(
      prop, "Limit View to Contents", "Limit timeline height to maximum used channel slot");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  /* Annotations */
  prop = RNA_def_property(srna, "annotation", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "gpd");
  RNA_def_property_struct_type(prop, "Annotation");
  RNA_def_property_pointer_funcs(
      prop, nullptr, nullptr, nullptr, "rna_GPencil_datablocks_annotations_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_ID_REFCOUNT);
  RNA_def_property_ui_text(prop, "Annotation", "Annotation data for this Preview region");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "overlay_frame_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "overlay_frame_type");
  RNA_def_property_enum_items(prop, overlay_frame_type_items);
  RNA_def_property_ui_text(prop, "Overlay Type", "Overlay display method");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_transform_preview", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "draw_flag", SEQ_DRAW_TRANSFORM_PREVIEW);
  RNA_def_property_ui_text(prop,
                           "Transform Preview",
                           "Show a preview of the start or end frame of a strip while "
                           "transforming its respective handle");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  /* Gizmo toggles. */
  prop = RNA_def_property(srna, "show_gizmo", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "gizmo_flag", SEQ_GIZMO_HIDE);
  RNA_def_property_ui_text(prop, "Show Gizmo", "Show gizmos of all types");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_gizmo_navigate", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "gizmo_flag", SEQ_GIZMO_HIDE_NAVIGATE);
  RNA_def_property_ui_text(prop, "Navigate Gizmo", "Viewport navigation gizmo");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_gizmo_context", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "gizmo_flag", SEQ_GIZMO_HIDE_CONTEXT);
  RNA_def_property_ui_text(prop, "Context Gizmo", "Context sensitive gizmos for the active item");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "show_gizmo_tool", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "gizmo_flag", SEQ_GIZMO_HIDE_TOOL);
  RNA_def_property_ui_text(prop, "Tool Gizmo", "Active tool gizmo");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  /* Overlay settings. */
  prop = RNA_def_property(srna, "show_overlays", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SEQ_SHOW_OVERLAY);
  RNA_def_property_ui_text(prop, "Show Overlays", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  prop = RNA_def_property(srna, "preview_overlay", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "SequencerPreviewOverlay");
  RNA_def_property_pointer_funcs(
      prop, "rna_SpaceSequenceEditor_preview_overlay_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Preview Overlay Settings", "Settings for display of overlays");

  prop = RNA_def_property(srna, "timeline_overlay", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "SequencerTimelineOverlay");
  RNA_def_property_pointer_funcs(
      prop, "rna_SpaceSequenceEditor_timeline_overlay_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Timeline Overlay Settings", "Settings for display of overlays");

  prop = RNA_def_property(srna, "cache_overlay", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "SequencerCacheOverlay");
  RNA_def_property_pointer_funcs(
      prop, "rna_SpaceSequenceEditor_cache_overlay_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Cache Overlay Settings", "Settings for display of overlays");
  rna_def_space_sequencer_preview_overlay(brna);
  rna_def_space_sequencer_timeline_overlay(brna);
  rna_def_space_sequencer_cache_overlay(brna);

  /* transform */
  prop = RNA_def_property(srna, "cursor_location", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "cursor");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "2D Cursor Location", "2D cursor location for this view");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SEQUENCER, nullptr);

  /* Zoom. */
  prop = RNA_def_property(srna, "zoom_percentage", PROP_FLOAT, PROP_PERCENTAGE);
  RNA_def_property_float_funcs(prop,
                               "rna_SpaceSequenceEditor_zoom_percentage_get",
                               "rna_SpaceSequenceEditor_zoom_percentage_set",
                               nullptr);
  RNA_def_property_float_default(prop, 100.0);
  RNA_def_property_range(prop, .4, 80000);
  RNA_def_property_ui_range(prop, 25, 400, 100, 0);
  RNA_def_property_ui_text(prop, "Zoom", "Zoom percentage");
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
  RNA_def_property_pointer_funcs(prop, nullptr, "rna_SpaceTextEditor_text_set", nullptr, nullptr);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, nullptr);

  /* display */
  prop = RNA_def_property(srna, "show_word_wrap", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "wordwrap", 0);
  RNA_def_property_boolean_funcs(prop, nullptr, "rna_SpaceTextEditor_word_wrap_set");
  RNA_def_property_ui_text(
      prop, "Word Wrap", "Wrap words if there is not enough horizontal space");
  RNA_def_property_ui_icon(prop, ICON_WORDWRAP_ON, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, nullptr);

  prop = RNA_def_property(srna, "show_line_numbers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "showlinenrs", 0);
  RNA_def_property_ui_text(prop, "Line Numbers", "Show line numbers next to the text");
  RNA_def_property_ui_icon(prop, ICON_LINENUMBERS_ON, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, nullptr);

  func = RNA_def_function(srna,
                          "is_syntax_highlight_supported",
                          "rna_SpaceTextEditor_text_is_syntax_highlight_supported");
  RNA_def_function_return(func,
                          RNA_def_boolean(func, "is_syntax_highlight_supported", false, "", ""));
  RNA_def_function_ui_description(func,
                                  "Returns True if the editor supports syntax highlighting "
                                  "for the current text data-block");

  prop = RNA_def_property(srna, "show_syntax_highlight", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "showsyntax", 0);
  RNA_def_property_ui_text(prop, "Syntax Highlight", "Syntax highlight for scripting");
  RNA_def_property_ui_icon(prop, ICON_SYNTAX_ON, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, nullptr);

  prop = RNA_def_property(srna, "show_line_highlight", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "line_hlight", 0);
  RNA_def_property_ui_text(prop, "Highlight Line", "Highlight the current line");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, nullptr);

  prop = RNA_def_property(srna, "tab_width", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "tabnumber");
  RNA_def_property_range(prop, 2, 8);
  RNA_def_property_ui_text(prop, "Tab Width", "Number of spaces to display tabs with");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, "rna_SpaceTextEditor_updateEdited");

  prop = RNA_def_property(srna, "font_size", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "lheight");
  RNA_def_property_range(prop, 1, 256); /* Large range since Hi-DPI scales down size. */
  RNA_def_property_ui_text(prop, "Font Size", "Font size to use for displaying the text");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, nullptr);

  prop = RNA_def_property(srna, "show_margin", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", ST_SHOW_MARGIN);
  RNA_def_property_ui_text(prop, "Show Margin", "Show right margin");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, nullptr);

  prop = RNA_def_property(srna, "margin_column", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "margin_column");
  RNA_def_property_range(prop, 0, 1024);
  RNA_def_property_ui_text(prop, "Margin Column", "Column number to show right margin at");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, nullptr);

  prop = RNA_def_property(srna, "top", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "top");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_ui_text(prop, "Top Line", "Top line visible");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, nullptr);

  prop = RNA_def_property(srna, "visible_lines", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_int_funcs(prop, "rna_SpaceTextEditor_visible_lines_get", nullptr, nullptr);
  RNA_def_property_ui_text(
      prop, "Visible Lines", "Amount of lines that can be visible in current editor");

  /* functionality options */
  prop = RNA_def_property(srna, "use_overwrite", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overwrite", 1);
  RNA_def_property_ui_text(
      prop, "Overwrite", "Overwrite characters when typing rather than inserting them");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, nullptr);

  prop = RNA_def_property(srna, "use_live_edit", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "live_edit", 1);
  RNA_def_property_ui_text(prop, "Live Edit", "Run Python while editing");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, nullptr);

  /* find */
  prop = RNA_def_property(srna, "use_find_all", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", ST_FIND_ALL);
  RNA_def_property_ui_text(
      prop, "Find All", "Search in all text data-blocks, instead of only the active one");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, nullptr);

  prop = RNA_def_property(srna, "use_find_wrap", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", ST_FIND_WRAP);
  RNA_def_property_ui_text(
      prop, "Find Wrap", "Search again from the start of the file when reaching the end");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, nullptr);

  prop = RNA_def_property(srna, "use_match_case", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flags", ST_MATCH_CASE);
  RNA_def_property_ui_text(
      prop, "Match Case", "Search string is sensitive to uppercase and lowercase letters");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, nullptr);

  prop = RNA_def_property(srna, "find_text", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "findstr");
  RNA_def_property_ui_text(prop, "Find Text", "Text to search for with the find tool");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, nullptr);

  prop = RNA_def_property(srna, "replace_text", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "replacestr");
  RNA_def_property_ui_text(
      prop, "Replace Text", "Text to replace selected text with using the replace tool");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TEXT, nullptr);

  RNA_api_space_text(srna);
}

static void rna_def_space_dopesheet_overlays(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SpaceDopeSheetOverlay", nullptr);
  RNA_def_struct_sdna(srna, "SpaceAction");
  RNA_def_struct_nested(brna, srna, "SpaceDopeSheetEditor");
  RNA_def_struct_path_func(srna, "rna_SpaceDopeSheetOverlay_path");
  RNA_def_struct_ui_text(srna, "Overlay Settings", "");

  prop = RNA_def_property(srna, "show_overlays", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlays.flag", ADS_OVERLAY_SHOW_OVERLAYS);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(prop, "Show Overlays", "Display overlays");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE, nullptr);

  prop = RNA_def_property(srna, "show_scene_strip_range", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlays.flag", ADS_SHOW_SCENE_STRIP_FRAME_RANGE);
  RNA_def_property_ui_text(prop,
                           "Show Scene Strip Range",
                           "When using scene time synchronization in the sequence editor, display "
                           "the range of the current scene strip");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_DOPESHEET, nullptr);
}

static void rna_def_space_dopesheet(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SpaceDopeSheetEditor", "Space");
  RNA_def_struct_sdna(srna, "SpaceAction");
  RNA_def_struct_ui_text(srna, "Space Dope Sheet Editor", "Dope Sheet space data");

  rna_def_space_generic_show_region_toggles(srna,
                                            (1 << RGN_TYPE_FOOTER) | (1 << RGN_TYPE_UI) |
                                                (1 << RGN_TYPE_HUD) | (1 << RGN_TYPE_CHANNELS));

  /* mode (hidden in the UI, see 'ui_mode') */
  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mode");
  RNA_def_property_enum_items(prop, rna_enum_space_action_mode_all_items);
  RNA_def_property_ui_text(prop, "Mode", "Editing context being displayed");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_DOPESHEET, "rna_SpaceDopeSheetEditor_mode_update");

  prop = RNA_def_property(srna, "ui_mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mode");
  RNA_def_property_enum_items(prop, rna_enum_space_action_ui_mode_items);
  RNA_def_property_ui_text(prop, "Mode", "Editing context being displayed");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_DOPESHEET, "rna_SpaceDopeSheetEditor_mode_update");

  /* display */
  prop = RNA_def_property(srna, "show_seconds", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SACTION_DRAWTIME);
  RNA_def_property_ui_text(prop, "Use Timecode", "Show timing as a timecode instead of frames");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_DOPESHEET, nullptr);

  prop = RNA_def_property(srna, "show_sliders", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SACTION_SLIDERS);
  RNA_def_property_ui_text(prop, "Show Sliders", "Show sliders beside F-Curve channels");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_DOPESHEET, nullptr);

  prop = RNA_def_property(srna, "show_pose_markers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SACTION_POSEMARKERS_SHOW);
  RNA_def_property_ui_text(prop,
                           "Show Pose Markers",
                           "Show markers belonging to the active action instead of Scene markers "
                           "(Action and Shape Key Editors only)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_DOPESHEET, nullptr);

  prop = RNA_def_property(srna, "show_interpolation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SACTION_SHOW_INTERPOLATION);
  RNA_def_property_ui_text(prop,
                           "Show Handles and Interpolation",
                           "Display keyframe handle types and non-Bézier interpolation modes");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_DOPESHEET, nullptr);

  prop = RNA_def_property(srna, "show_extremes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SACTION_SHOW_EXTREMES);
  RNA_def_property_ui_text(prop,
                           "Show Curve Extremes",
                           "Mark keyframes where the key value flow changes direction, based on "
                           "comparison with adjacent keys");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_DOPESHEET, nullptr);

  prop = RNA_def_property(srna, "show_markers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SACTION_SHOW_MARKERS);
  RNA_def_property_ui_text(
      prop,
      "Show Markers",
      "If any exists, show markers in a separate row at the bottom of the editor");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_DOPESHEET, nullptr);

  /* editing */
  prop = RNA_def_property(srna, "use_auto_merge_keyframes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", SACTION_NOTRANSKEYCULL);
  RNA_def_property_ui_text(prop, "Auto-Merge Keyframes", "Automatically merge nearby keyframes");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_DOPESHEET, nullptr);

  prop = RNA_def_property(srna, "use_realtime_update", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", SACTION_NOREALTIMEUPDATES);
  RNA_def_property_ui_text(
      prop,
      "Realtime Updates",
      "When transforming keyframes, changes to the animation data are flushed to other views");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_DOPESHEET, nullptr);

  prop = RNA_def_property(srna, "use_marker_sync", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SACTION_MARKERS_MOVE);
  RNA_def_property_ui_text(prop, "Sync Markers", "Sync Markers with keyframe edits");

  /* dopesheet */
  prop = RNA_def_property(srna, "dopesheet", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "DopeSheet");
  RNA_def_property_pointer_sdna(prop, nullptr, "ads");
  RNA_def_property_ui_text(prop, "Dope Sheet", "Settings for filtering animation data");

  /* displaying cache status */
  prop = RNA_def_property(srna, "show_cache", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_display", TIME_CACHE_DISPLAY);
  RNA_def_property_ui_text(prop, "Show Cache", "Show the status of cached frames in the timeline");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, nullptr);

  prop = RNA_def_property(srna, "cache_softbody", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_display", TIME_CACHE_SOFTBODY);
  RNA_def_property_ui_text(prop, "Softbody", "Show the active object's softbody point cache");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, nullptr);

  prop = RNA_def_property(srna, "cache_particles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_display", TIME_CACHE_PARTICLES);
  RNA_def_property_ui_text(prop, "Particles", "Show the active object's particle point cache");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, nullptr);

  prop = RNA_def_property(srna, "cache_cloth", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_display", TIME_CACHE_CLOTH);
  RNA_def_property_ui_text(prop, "Cloth", "Show the active object's cloth point cache");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, nullptr);

  prop = RNA_def_property(srna, "cache_smoke", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_display", TIME_CACHE_SMOKE);
  RNA_def_property_ui_text(prop, "Smoke", "Show the active object's smoke cache");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, nullptr);

  prop = RNA_def_property(srna, "cache_simulation_nodes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_display", TIME_CACHE_SIMULATION_NODES);
  RNA_def_property_ui_text(
      prop, "Simulation Nodes", "Show the active object's simulation nodes cache and bake data");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, nullptr);

  prop = RNA_def_property(srna, "cache_dynamicpaint", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_display", TIME_CACHE_DYNAMICPAINT);
  RNA_def_property_ui_text(prop, "Dynamic Paint", "Show the active object's Dynamic Paint cache");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, nullptr);

  prop = RNA_def_property(srna, "cache_rigidbody", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "cache_display", TIME_CACHE_RIGIDBODY);
  RNA_def_property_ui_text(prop, "Rigid Body", "Show the active object's Rigid Body cache");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_TIME, nullptr);

  prop = RNA_def_property(srna, "overlays", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "SpaceDopeSheetOverlay");
  RNA_def_property_pointer_funcs(
      prop, "rna_SpaceDopeSheet_overlay_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Overlay Settings", "Settings for display of overlays");

  rna_def_space_dopesheet_overlays(brna);
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
      // {V3D_AROUND_CENTER_MEDIAN, "MEDIAN_POINT", 0, "Median Point", ""},
      // {V3D_AROUND_ACTIVE, "ACTIVE_ELEMENT", 0, "Active Element", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "SpaceGraphEditor", "Space");
  RNA_def_struct_sdna(srna, "SpaceGraph");
  RNA_def_struct_ui_text(srna, "Space Graph Editor", "Graph Editor space data");

  rna_def_space_generic_show_region_toggles(srna,
                                            (1 << RGN_TYPE_FOOTER) | (1 << RGN_TYPE_UI) |
                                                (1 << RGN_TYPE_HUD) | (1 << RGN_TYPE_CHANNELS));

  /* mode */
  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mode");
  RNA_def_property_enum_items(prop, rna_enum_space_graph_mode_items);
  RNA_def_property_ui_text(prop, "Mode", "Editing context being displayed");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_GRAPH, "rna_SpaceGraphEditor_display_mode_update");

  /* display */
  prop = RNA_def_property(srna, "show_seconds", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SIPO_DRAWTIME);
  RNA_def_property_ui_text(prop, "Use Timecode", "Show timing as a timecode instead of frames");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, nullptr);

  prop = RNA_def_property(srna, "show_sliders", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SIPO_SLIDERS);
  RNA_def_property_ui_text(prop, "Show Sliders", "Show sliders beside F-Curve channels");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, nullptr);

  prop = RNA_def_property(srna, "show_handles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", SIPO_NOHANDLES);
  RNA_def_property_ui_text(prop, "Show Handles", "Show handles of Bézier control points");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, nullptr);

  prop = RNA_def_property(srna, "use_auto_lock_translation_axis", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SIPO_AUTOLOCK_AXIS);
  RNA_def_property_ui_text(prop,
                           "Auto-Lock Key Axis",
                           "Automatically locks the movement of keyframes to the dominant axis");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, nullptr);

  prop = RNA_def_property(srna, "use_only_selected_keyframe_handles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SIPO_SELVHANDLESONLY);
  RNA_def_property_ui_text(
      prop, "Only Selected Keyframes Handles", "Only show and edit handles of selected keyframes");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, nullptr);

  prop = RNA_def_property(srna, "show_markers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SIPO_SHOW_MARKERS);
  RNA_def_property_ui_text(
      prop,
      "Show Markers",
      "If any exists, show markers in a separate row at the bottom of the editor");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, nullptr);

  prop = RNA_def_property(srna, "show_extrapolation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", SIPO_NO_DRAW_EXTRAPOLATION);
  RNA_def_property_ui_text(prop, "Show Extrapolation", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, nullptr);

  /* editing */
  prop = RNA_def_property(srna, "use_auto_merge_keyframes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", SIPO_NOTRANSKEYCULL);
  RNA_def_property_ui_text(prop, "Auto-Merge Keyframes", "Automatically merge nearby keyframes");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, nullptr);

  prop = RNA_def_property(srna, "use_realtime_update", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", SIPO_NOREALTIMEUPDATES);
  RNA_def_property_ui_text(
      prop,
      "Realtime Updates",
      "When transforming keyframes, changes to the animation data are flushed to other views");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, nullptr);

  /* cursor */
  prop = RNA_def_property(srna, "show_cursor", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", SIPO_NODRAWCURSOR);
  RNA_def_property_ui_text(prop, "Show Cursor", "Show 2D cursor");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, nullptr);

  prop = RNA_def_property(srna, "cursor_position_x", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "cursorTime");
  RNA_def_property_ui_text(
      prop, "Cursor X-Value", "Graph Editor 2D-Value cursor - X-Value component");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, nullptr);

  prop = RNA_def_property(srna, "cursor_position_y", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "cursorVal");
  RNA_def_property_ui_text(
      prop, "Cursor Y-Value", "Graph Editor 2D-Value cursor - Y-Value component");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, nullptr);

  prop = RNA_def_property(srna, "pivot_point", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "around");
  RNA_def_property_enum_items(prop, gpivot_items);
  RNA_def_property_ui_text(prop, "Pivot Point", "Pivot center for rotation/scaling");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, nullptr);

  /* Dope-sheet. */
  prop = RNA_def_property(srna, "dopesheet", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "DopeSheet");
  RNA_def_property_pointer_sdna(prop, nullptr, "ads");
  RNA_def_property_ui_text(prop, "Dope Sheet", "Settings for filtering animation data");

  /* Read-only state info. */
  prop = RNA_def_property(srna, "has_ghost_curves", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_SpaceGraphEditor_has_ghost_curves_get", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "Has Ghost Curves", "Graph Editor instance has some ghost curves stored");

  /* Normalize curves. */
  prop = RNA_def_property(srna, "use_normalization", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SIPO_NORMALIZE);
  RNA_def_property_ui_text(prop,
                           "Use Normalization",
                           "Display curves in normalized range from -1 to 1, "
                           "for easier editing of multiple curves with different ranges");
  RNA_def_property_flag(prop, PROP_CONTEXT_UPDATE);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_GRAPH, "rna_SpaceGraphEditor_normalize_update");

  prop = RNA_def_property(srna, "use_auto_normalization", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", SIPO_NORMALIZE_FREEZE);
  RNA_def_property_ui_text(prop,
                           "Auto Normalization",
                           "Automatically recalculate curve normalization on every curve edit");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_GRAPH, nullptr);
}

static void rna_def_space_nla(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SpaceNLA", "Space");
  RNA_def_struct_sdna(srna, "SpaceNla");
  RNA_def_struct_ui_text(srna, "Space Nla Editor", "NLA editor space data");

  rna_def_space_generic_show_region_toggles(srna,
                                            (1 << RGN_TYPE_FOOTER) | (1 << RGN_TYPE_UI) |
                                                (1 << RGN_TYPE_HUD) | (1 << RGN_TYPE_CHANNELS));

  /* display */
  prop = RNA_def_property(srna, "show_seconds", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SNLA_DRAWTIME);
  RNA_def_property_ui_text(prop, "Use Timecode", "Show timing as a timecode instead of frames");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NLA, nullptr);

  prop = RNA_def_property(srna, "show_strip_curves", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", SNLA_NOSTRIPCURVES);
  RNA_def_property_ui_text(prop, "Show Control F-Curves", "Show influence F-Curves on strips");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NLA, nullptr);

  prop = RNA_def_property(srna, "show_local_markers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", SNLA_NOLOCALMARKERS);
  RNA_def_property_ui_text(
      prop,
      "Show Local Markers",
      "Show action-local markers on the strips, useful when synchronizing timing across strips");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NLA, nullptr);

  prop = RNA_def_property(srna, "show_markers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SNLA_SHOW_MARKERS);
  RNA_def_property_ui_text(
      prop,
      "Show Markers",
      "If any exists, show markers in a separate row at the bottom of the editor");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NLA, nullptr);

  /* editing */
  prop = RNA_def_property(srna, "use_realtime_update", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", SNLA_NOREALTIMEUPDATES);
  RNA_def_property_ui_text(
      prop,
      "Realtime Updates",
      "When transforming strips, changes to the animation data are flushed to other views");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NLA, nullptr);

  /* dopesheet */
  prop = RNA_def_property(srna, "dopesheet", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "DopeSheet");
  RNA_def_property_pointer_sdna(prop, nullptr, "ads");
  RNA_def_property_ui_text(prop, "Dope Sheet", "Settings for filtering animation data");
}

static void rna_def_console_line(BlenderRNA *brna)
{
  static const EnumPropertyItem console_line_type_items[] = {
      {CONSOLE_LINE_OUTPUT, "OUTPUT", 0, "Output", ""},
      {CONSOLE_LINE_INPUT, "INPUT", 0, "Input", ""},
      {CONSOLE_LINE_INFO, "INFO", 0, "Info", ""},
      {CONSOLE_LINE_ERROR, "ERROR", 0, "Error", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ConsoleLine", nullptr);
  RNA_def_struct_ui_text(srna, "Console Input", "Input line for the interactive console");

  prop = RNA_def_property(srna, "body", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, "rna_ConsoleLine_body_get", "rna_ConsoleLine_body_length", "rna_ConsoleLine_body_set");
  RNA_def_property_ui_text(prop, "Line", "Text in the line");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CONSOLE, nullptr);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_TEXT);

  prop = RNA_def_property(
      srna, "current_character", PROP_INT, PROP_NONE); /* copied from text editor */
  RNA_def_property_int_funcs(prop,
                             "rna_ConsoleLine_current_character_get",
                             "rna_ConsoleLine_current_character_set",
                             nullptr);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CONSOLE, nullptr);

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "type");
  RNA_def_property_enum_items(prop, console_line_type_items);
  RNA_def_property_ui_text(prop, "Type", "Console line type when used in scrollback");
}

static void rna_def_space_console(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SpaceConsole", "Space");
  RNA_def_struct_sdna(srna, "SpaceConsole");
  RNA_def_struct_ui_text(srna, "Space Console", "Interactive Python console");

  /* display */
  prop = RNA_def_property(srna, "font_size", PROP_INT, PROP_NONE); /* copied from text editor */
  RNA_def_property_int_sdna(prop, nullptr, "lheight");
  RNA_def_property_range(prop, 1, 256); /* Large range since Hi-DPI scales down size. */
  RNA_def_property_ui_text(prop, "Font Size", "Font size to use for displaying the text");
  RNA_def_property_update(prop, 0, "rna_SpaceConsole_rect_update");

  prop = RNA_def_property(
      srna, "select_start", PROP_INT, PROP_UNSIGNED); /* copied from text editor */
  RNA_def_property_int_sdna(prop, nullptr, "sel_start");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CONSOLE, nullptr);

  prop = RNA_def_property(
      srna, "select_end", PROP_INT, PROP_UNSIGNED); /* copied from text editor */
  RNA_def_property_int_sdna(prop, nullptr, "sel_end");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CONSOLE, nullptr);

  prop = RNA_def_property(srna, "prompt", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Prompt", "Command line prompt");

  prop = RNA_def_property(srna, "language", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Language", "Command line prompt language");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_EDITOR_PYTHON_CONSOLE);

  prop = RNA_def_property(srna, "history", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "history", nullptr);
  RNA_def_property_struct_type(prop, "ConsoleLine");
  RNA_def_property_ui_text(prop, "History", "Command history");

  prop = RNA_def_property(srna, "scrollback", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "scrollback", nullptr);
  RNA_def_property_struct_type(prop, "ConsoleLine");
  RNA_def_property_ui_text(prop, "Output", "Command output");
}

/* Filter for datablock types in link/append. */
static void rna_def_fileselect_idfilter(BlenderRNA *brna)
{

  StructRNA *srna = RNA_def_struct(brna, "FileSelectIDFilter", nullptr);
  RNA_def_struct_sdna(srna, "FileSelectParams");
  RNA_def_struct_nested(brna, srna, "FileSelectParams");
  RNA_def_struct_ui_text(
      srna, "File Select ID Filter", "Which ID types to show/hide, when browsing a library");

  const IDFilterEnumPropertyItem *individual_ids_and_categories[] = {
      rna_enum_id_type_filter_items,
      rna_enum_space_file_id_filter_categories,
      nullptr,
  };
  for (uint i = 0; individual_ids_and_categories[i]; i++) {
    for (int j = 0; individual_ids_and_categories[i][j].identifier; j++) {
      PropertyRNA *prop = RNA_def_property(
          srna, individual_ids_and_categories[i][j].identifier, PROP_BOOLEAN, PROP_NONE);
      RNA_def_property_boolean_sdna(
          prop, nullptr, "filter_id", individual_ids_and_categories[i][j].flag);
      RNA_def_property_ui_text(prop,
                               individual_ids_and_categories[i][j].name,
                               individual_ids_and_categories[i][j].description);
      RNA_def_property_ui_icon(prop, individual_ids_and_categories[i][j].icon, 0);
      RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);
    }
  }
}

/* Filter for datablock types in the Asset Browser. */
static void rna_def_fileselect_asset_idfilter(BlenderRNA *brna)
{
  StructRNA *srna = RNA_def_struct(brna, "FileAssetSelectIDFilter", nullptr);
  RNA_def_struct_sdna(srna, "FileSelectParams");
  RNA_def_struct_nested(brna, srna, "FileSelectParams");
  RNA_def_struct_ui_text(srna,
                         "File Select Asset Filter",
                         "Which asset types to show/hide, when browsing an asset library");

  static char experimental_prop_names[INDEX_ID_MAX][MAX_NAME];

  for (uint i = 0; rna_enum_id_type_filter_items[i].identifier; i++) {
    const IDFilterEnumPropertyItem *item = &rna_enum_id_type_filter_items[i];
    const bool is_experimental = (ED_ASSET_TYPE_IDS_NON_EXPERIMENTAL_FLAGS & item->flag) == 0;

    const char *identifier = rna_enum_id_type_filter_items[i].identifier;
    if (is_experimental) {
      /* Create name for experimental property and store in static buffer. */
      SNPRINTF(experimental_prop_names[i], "experimental_%s", identifier);
      identifier = experimental_prop_names[i];
    }

    PropertyRNA *prop = RNA_def_property(srna, identifier, PROP_BOOLEAN, PROP_NONE);
    RNA_def_property_boolean_sdna(prop, nullptr, "filter_id", item->flag);
    RNA_def_property_ui_text(prop, item->name, item->description);
    RNA_def_property_ui_icon(prop, item->icon, 0);
    RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);
  }
}

static void rna_def_fileselect_entry(BlenderRNA *brna)
{
  PropertyRNA *prop;
  StructRNA *srna = RNA_def_struct(brna, "FileSelectEntry", nullptr);
  RNA_def_struct_sdna(srna, "FileDirEntry");
  RNA_def_struct_ui_text(srna, "File Select Entry", "A file viewable in the File Browser");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_FILENAME);
  RNA_def_property_editable_func(prop, "rna_FileBrowser_FileSelectEntry_name_editable");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_string_funcs(prop,
                                "rna_FileBrowser_FileSelectEntry_name_get",
                                "rna_FileBrowser_FileSelectEntry_name_length",
                                nullptr);
  RNA_def_property_ui_text(prop, "Name", "");
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "relative_path", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_string_funcs(prop,
                                "rna_FileBrowser_FileSelectEntry_relative_path_get",
                                "rna_FileBrowser_FileSelectEntry_relative_path_length",
                                nullptr);
  RNA_def_property_ui_text(prop,
                           "Relative Path",
                           "Path relative to the directory currently displayed in the File "
                           "Browser (includes the file name)");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

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
      prop, "rna_FileBrowser_FileSelectEntry_preview_icon_id_get", nullptr, nullptr);

  prop = RNA_def_property(srna, "asset_data", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "AssetMetaData");
  RNA_def_property_pointer_funcs(
      prop, "rna_FileBrowser_FileSelectEntry_asset_data_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(
      prop, "Asset Data", "Asset data, valid if the file represents an asset");
}

static void rna_def_fileselect_params(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem display_size_items[] = {
      {32, "TINY", 0, "Tiny", ""},
      {64, "SMALL", 0, "Small", ""},
      {96, "NORMAL", 0, "Medium", ""},
      {128, "BIG", 0, "Big", ""},
      {192, "LARGE", 0, "Large", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "FileSelectParams", nullptr);
  RNA_def_struct_path_func(srna, "rna_FileSelectParams_path");
  RNA_def_struct_ui_text(srna, "File Select Parameters", "File Select Parameters");

  prop = RNA_def_property(srna, "title", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "title");
  RNA_def_property_ui_text(prop, "Title", "Title for the file browser");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  /* Use BYTESTRING rather than DIRPATH as sub-type so UI code doesn't add OT_directory_browse
   * button when displaying this prop in the file browser (it would just open a file browser). That
   * should be the only effective difference between the two. */
  prop = RNA_def_property(srna, "directory", PROP_STRING, PROP_BYTESTRING);
  RNA_def_property_string_sdna(prop, nullptr, "dir");
  RNA_def_property_ui_text(prop, "Directory", "Directory displayed in the file browser");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "filename", PROP_STRING, PROP_FILENAME);
  RNA_def_property_string_sdna(prop, nullptr, "file");
  RNA_def_property_ui_text(prop, "File Name", "Active file in the file browser");
  RNA_def_property_editable_func(prop, "rna_FileSelectParams_filename_editable");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "use_library_browsing", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Library Browser", "Whether we may browse Blender files' content or not");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_boolean_funcs(prop, "rna_FileSelectParams_use_lib_get", nullptr);

  prop = RNA_def_property(srna, "display_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "display");
  RNA_def_property_enum_items(prop, fileselectparams_display_type_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_FileSelectParams_display_type_itemf");
  RNA_def_property_enum_default_func(prop, "rna_FileSelectParams_display_type_default");
  RNA_def_property_ui_text(prop, "Display Mode", "Display mode for the file list");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "recursion_level", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, fileselectparams_recursion_level_items);
  RNA_def_property_enum_funcs(
      prop, nullptr, nullptr, "rna_FileSelectParams_recursion_level_itemf");
  RNA_def_property_ui_text(prop, "Recursion", "Numbers of dirtree levels to show simultaneously");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "show_details_size", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "details_flags", FILE_DETAILS_SIZE);
  RNA_def_property_ui_text(prop, "File Size", "Show a column listing the size of each file");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "show_details_datetime", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "details_flags", FILE_DETAILS_DATETIME);
  RNA_def_property_ui_text(
      prop,
      "File Modification Date",
      "Show a column listing the date and time of modification for each file");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "use_filter", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", FILE_FILTER);
  RNA_def_property_ui_text(prop, "Filter Files", "Enable filtering of files");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "show_hidden", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", FILE_HIDE_DOT);
  RNA_def_property_ui_text(prop, "Show Hidden", "Show hidden dot files");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "sort_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "sort");
  RNA_def_property_enum_items(prop, rna_enum_fileselect_params_sort_items);
  RNA_def_property_enum_funcs(prop, nullptr, nullptr, "rna_FileSelectParams_sort_method_itemf");
  RNA_def_property_ui_text(prop, "Sort", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "use_sort_invert", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", FILE_SORT_INVERT);
  RNA_def_property_ui_text(
      prop, "Reverse Sorting", "Sort items descending, from highest value to lowest");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "use_filter_image", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filter", FILE_TYPE_IMAGE);
  RNA_def_property_ui_text(prop, "Filter Images", "Show image files");
  RNA_def_property_ui_icon(prop, ICON_FILE_IMAGE, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "use_filter_blender", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filter", FILE_TYPE_BLENDER);
  RNA_def_property_ui_text(prop, "Filter Blender", "Show .blend files");
  RNA_def_property_ui_icon(prop, ICON_FILE_BLEND, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "use_filter_backup", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filter", FILE_TYPE_BLENDER_BACKUP);
  RNA_def_property_ui_text(
      prop, "Filter Blender Backup Files", "Show .blend1, .blend2, etc. files");
  RNA_def_property_ui_icon(prop, ICON_FILE_BACKUP, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "use_filter_movie", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filter", FILE_TYPE_MOVIE);
  RNA_def_property_ui_text(prop, "Filter Movies", "Show movie files");
  RNA_def_property_ui_icon(prop, ICON_FILE_MOVIE, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "use_filter_script", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filter", FILE_TYPE_PYSCRIPT);
  RNA_def_property_ui_text(prop, "Filter Script", "Show script files");
  RNA_def_property_ui_icon(prop, ICON_FILE_SCRIPT, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "use_filter_font", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filter", FILE_TYPE_FTFONT);
  RNA_def_property_ui_text(prop, "Filter Fonts", "Show font files");
  RNA_def_property_ui_icon(prop, ICON_FILE_FONT, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "use_filter_sound", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filter", FILE_TYPE_SOUND);
  RNA_def_property_ui_text(prop, "Filter Sound", "Show sound files");
  RNA_def_property_ui_icon(prop, ICON_FILE_SOUND, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "use_filter_text", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filter", FILE_TYPE_TEXT);
  RNA_def_property_ui_text(prop, "Filter Text", "Show text files");
  RNA_def_property_ui_icon(prop, ICON_FILE_TEXT, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "use_filter_volume", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filter", FILE_TYPE_VOLUME);
  RNA_def_property_ui_text(prop, "Filter Volume", "Show 3D volume files");
  RNA_def_property_ui_icon(prop, ICON_FILE_VOLUME, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "use_filter_folder", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filter", FILE_TYPE_FOLDER);
  RNA_def_property_ui_text(prop, "Filter Folder", "Show folders");
  RNA_def_property_ui_icon(prop, ICON_FILE_FOLDER, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "use_filter_blendid", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filter", FILE_TYPE_BLENDERLIB);
  RNA_def_property_ui_text(
      prop, "Filter Blender IDs", "Show .blend files items (objects, materials, etc.)");
  RNA_def_property_ui_icon(prop, ICON_BLENDER, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "use_filter_asset_only", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", FILE_ASSETS_ONLY);
  RNA_def_property_ui_text(
      prop, "Only Assets", "Hide .blend files items that are not data-blocks with asset metadata");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "filter_id", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "FileSelectIDFilter");
  RNA_def_property_pointer_funcs(
      prop, "rna_FileSelectParams_filter_id_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(
      prop, "Filter ID Types", "Which ID types to show/hide, when browsing a library");

  prop = RNA_def_property(srna, "filter_glob", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "filter_glob");
  RNA_def_property_ui_text(prop,
                           "Extension Filter",
                           "UNIX shell-like filename patterns matching, supports wildcards ('*') "
                           "and list of patterns separated by ';'");
  RNA_def_property_string_funcs(prop, nullptr, nullptr, "rna_FileSelectPrams_filter_glob_set");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_LIST, nullptr);

  prop = RNA_def_property(srna, "filter_search", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "filter_search");
  RNA_def_property_ui_text(
      prop, "Name or Tag Filter", "Filter by name or tag, supports '*' wildcard");
  RNA_def_property_flag(prop, PROP_TEXTEDIT_UPDATE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_LIST, nullptr);

  prop = RNA_def_property(srna, "display_size", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "thumbnail_size");
  RNA_def_property_ui_text(prop, "Display Size", "Change the size of thumbnails");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_LIST, nullptr);
  RNA_def_property_int_default(prop, 96);
  RNA_def_property_range(prop, 16, 256);
  RNA_def_property_ui_range(prop, 24, 256, 1, 0);

  prop = RNA_def_property(srna, "display_size_discrete", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "thumbnail_size");
  RNA_def_property_enum_items(prop, display_size_items);
  RNA_def_property_ui_text(
      prop, "Display Size", "Change the size of thumbnails in discrete steps");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_LIST, nullptr);

  prop = RNA_def_property(srna, "list_display_size", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "list_thumbnail_size");
  RNA_def_property_ui_text(prop, "Display Size", "Change the size of thumbnails in list views");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_LIST, nullptr);
  RNA_def_property_int_default(prop, 32);
  RNA_def_property_range(prop, 16, 128);
  RNA_def_property_ui_range(prop, 16, 128, 1, 0);

  prop = RNA_def_property(srna, "list_column_size", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Columns Size", "The width of columns in horizontal list views");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_LIST, nullptr);
  RNA_def_property_int_default(prop, 32);
  RNA_def_property_range(prop, 32, 750);
  RNA_def_property_ui_range(prop, 32, 750, 1, 0);
}

static void rna_def_fileselect_asset_params(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "FileAssetSelectParams", "FileSelectParams");
  RNA_def_struct_ui_text(
      srna, "Asset Select Parameters", "Settings for the file selection in Asset Browser mode");

  prop = rna_def_asset_library_reference_common(srna,
                                                "rna_FileAssetSelectParams_asset_library_get",
                                                "rna_FileAssetSelectParams_asset_library_set");
  RNA_def_property_ui_text(prop, "Asset Library", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "catalog_id", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(prop,
                                "rna_FileAssetSelectParams_catalog_id_get",
                                "rna_FileAssetSelectParams_catalog_id_length",
                                "rna_FileAssetSelectParams_catalog_id_set");
  RNA_def_property_ui_text(prop, "Catalog UUID", "The UUID of the catalog shown in the browser");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "filter_asset_id", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "FileAssetSelectIDFilter");
  RNA_def_property_pointer_funcs(
      prop, "rna_FileAssetSelectParams_filter_id_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop,
                           "Filter Asset Types",
                           "Which asset types to show/hide, when browsing an asset library");

  prop = RNA_def_property(srna, "import_method", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_asset_import_method_items);
  RNA_def_property_enum_funcs(
      prop, nullptr, nullptr, "rna_FileAssetSelectParams_import_method_itemf");
  RNA_def_property_ui_text(prop, "Import Method", "Determine how the asset will be imported");
  /* Asset drag info saved by buttons stores the import method, so the space must redraw when
   * import method changes. */
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_LIST, nullptr);

  prop = RNA_def_property(srna, "instance_collections_on_link", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "import_flags", FILE_ASSET_IMPORT_INSTANCE_COLLECTIONS_ON_LINK);
  RNA_def_property_ui_text(prop,
                           "Instance Collections on Linking",
                           "Create instances for collections when linking, rather than adding "
                           "them directly to the scene");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

  prop = RNA_def_property(srna, "instance_collections_on_append", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "import_flags", FILE_ASSET_IMPORT_INSTANCE_COLLECTIONS_ON_APPEND);
  RNA_def_property_ui_text(prop,
                           "Instance Collections on Appending",
                           "Create instances for collections when appending, rather than adding "
                           "them directly to the scene");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);
}

static void rna_def_filemenu_entry(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "FileBrowserFSMenuEntry", nullptr);
  RNA_def_struct_sdna(srna, "FSMenuEntry");
  RNA_def_struct_ui_text(srna, "File Select Parameters", "File Select Parameters");

  prop = RNA_def_property(srna, "path", PROP_STRING, PROP_FILEPATH);
  RNA_def_property_string_funcs(prop,
                                "rna_FileBrowser_FSMenuEntry_path_get",
                                "rna_FileBrowser_FSMenuEntry_path_length",
                                "rna_FileBrowser_FSMenuEntry_path_set");
  RNA_def_property_ui_text(prop, "Path", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_EDITOR_FILEBROWSER);

  /* Use #PROP_FILENAME sub-type so the UI can manipulate non-UTF8 names. */
  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_FILENAME);
  RNA_def_property_string_funcs(prop,
                                "rna_FileBrowser_FSMenuEntry_name_get",
                                "rna_FileBrowser_FSMenuEntry_name_length",
                                "rna_FileBrowser_FSMenuEntry_name_set");
  RNA_def_property_editable_func(prop, "rna_FileBrowser_FSMenuEntry_name_get_editable");
  RNA_def_property_ui_text(prop, "Name", "");
  RNA_def_struct_name_property(srna, prop);

  prop = RNA_def_property(srna, "icon", PROP_INT, PROP_NONE);
  RNA_def_property_int_funcs(prop,
                             "rna_FileBrowser_FSMenuEntry_icon_get",
                             "rna_FileBrowser_FSMenuEntry_icon_set",
                             nullptr);
  RNA_def_property_ui_text(prop, "Icon", "");

  prop = RNA_def_property(srna, "use_save", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_FileBrowser_FSMenuEntry_use_save_get", nullptr);
  RNA_def_property_ui_text(
      prop, "Save", "Whether this path is saved in bookmarks, or generated from OS");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
}

static void rna_def_space_filebrowser(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SpaceFileBrowser", "Space");
  RNA_def_struct_sdna(srna, "SpaceFile");
  RNA_def_struct_ui_text(srna, "Space File Browser", "File browser space data");

  rna_def_space_generic_show_region_toggles(
      srna, (1 << RGN_TYPE_TOOLS) | (1 << RGN_TYPE_UI) | (1 << RGN_TYPE_TOOL_PROPS));

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
      prop, "rna_FileBrowser_params_get", nullptr, "rna_FileBrowser_params_typef", nullptr);
  RNA_def_property_ui_text(
      prop, "Filebrowser Parameter", "Parameters and Settings for the Filebrowser");

  prop = RNA_def_property(srna, "active_operator", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "op");
  RNA_def_property_ui_text(prop, "Active Operator", "");

  /* Keep this for compatibility with existing presets,
   * not exposed in c++ API because of keyword conflict. */
  prop = RNA_def_property(srna, "operator", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "op");
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
                                    nullptr,
                                    nullptr,
                                    nullptr);
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
  RNA_def_property_int_sdna(prop, nullptr, "systemnr");
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
                                    nullptr,
                                    nullptr,
                                    nullptr);
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
  RNA_def_property_int_sdna(prop, nullptr, "system_bookmarknr");
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
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE);

  prop = RNA_def_int(srna,
                     "bookmarks_active",
                     -1,
                     -1,
                     INT_MAX,
                     "Active Bookmark",
                     "Index of active bookmark (-1 if none)",
                     -1,
                     INT_MAX);
  RNA_def_property_int_sdna(prop, nullptr, "bookmarknr");
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
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_flag(prop, PROP_EDITABLE);

  prop = RNA_def_int(srna,
                     "recent_folders_active",
                     -1,
                     -1,
                     INT_MAX,
                     "Active Recent Folder",
                     "Index of active recent folder (-1 if none)",
                     -1,
                     INT_MAX);
  RNA_def_property_int_sdna(prop, nullptr, "recentnr");
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
  RNA_def_property_boolean_sdna(prop, nullptr, "rpt_mask", INFO_RPT_DEBUG);
  RNA_def_property_ui_text(prop, "Show Debug", "Display debug reporting info");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_INFO_REPORT, nullptr);

  prop = RNA_def_property(srna, "show_report_info", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "rpt_mask", INFO_RPT_INFO);
  RNA_def_property_ui_text(prop, "Show Info", "Display general information");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_INFO_REPORT, nullptr);

  prop = RNA_def_property(srna, "show_report_operator", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "rpt_mask", INFO_RPT_OP);
  RNA_def_property_ui_text(prop, "Show Operator", "Display the operator log");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_INFO_REPORT, nullptr);

  prop = RNA_def_property(srna, "show_report_warning", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "rpt_mask", INFO_RPT_WARN);
  RNA_def_property_ui_text(prop, "Show Warn", "Display warnings");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_INFO_REPORT, nullptr);

  prop = RNA_def_property(srna, "show_report_error", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "rpt_mask", INFO_RPT_ERR);
  RNA_def_property_ui_text(prop, "Show Error", "Display error text");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_INFO_REPORT, nullptr);
}

static void rna_def_space_userpref(BlenderRNA *brna)
{
  static const EnumPropertyItem filter_type_items[] = {
      {0, "NAME", 0, "Name", "Filter based on the operator name"},
      {1, "KEY", 0, "Key-Binding", "Filter based on key bindings"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SpacePreferences", "Space");
  RNA_def_struct_sdna(srna, "SpaceUserPref");
  RNA_def_struct_ui_text(srna, "Space Preferences", "Blender preferences space data");

  rna_def_space_generic_show_region_toggles(srna, (1 << RGN_TYPE_UI));

  prop = RNA_def_property(srna, "filter_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "filter_type");
  RNA_def_property_enum_items(prop, filter_type_items);
  RNA_def_property_ui_text(prop, "Filter Type", "Filter method");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE, nullptr);

  prop = RNA_def_property(srna, "filter_text", PROP_STRING, PROP_NONE);
  RNA_def_property_string_sdna(prop, nullptr, "filter");
  RNA_def_property_flag(prop, PROP_TEXTEDIT_UPDATE);
  RNA_def_property_ui_text(prop, "Filter", "Search term for filtering in the UI");
}

static void rna_def_node_tree_path(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "NodeTreePath", nullptr);
  RNA_def_struct_sdna(srna, "bNodeTreePath");
  RNA_def_struct_ui_text(srna, "Node Tree Path", "Element of the node space tree path");

  prop = RNA_def_property(srna, "node_tree", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "nodetree");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Node Tree", "Base node tree from context");
}

static void rna_def_space_node_path_api(BlenderRNA *brna, PropertyRNA *cprop)
{
  StructRNA *srna;
  PropertyRNA *prop, *parm;
  FunctionRNA *func;

  RNA_def_property_srna(cprop, "SpaceNodeEditorPath");
  srna = RNA_def_struct(brna, "SpaceNodeEditorPath", nullptr);
  RNA_def_struct_sdna(srna, "SpaceNode");
  RNA_def_struct_ui_text(srna, "Space Node Editor Path", "History of node trees in the editor");

  prop = RNA_def_property(srna, "to_string", PROP_STRING, PROP_NONE);
  RNA_def_property_string_funcs(
      prop, "rna_SpaceNodeEditor_path_get", "rna_SpaceNodeEditor_path_length", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_struct_ui_text(srna, "Path", "Get the node tree path as a string");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_EDITOR_FILEBROWSER);

  func = RNA_def_function(srna, "clear", "rna_SpaceNodeEditor_path_clear");
  RNA_def_function_ui_description(func, "Reset the node tree path");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);

  func = RNA_def_function(srna, "start", "rna_SpaceNodeEditor_path_start");
  RNA_def_function_ui_description(func, "Set the root node tree");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(func, "node_tree", "NodeTree", "Node Tree", "");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);

  func = RNA_def_function(srna, "append", "rna_SpaceNodeEditor_path_append");
  RNA_def_function_ui_description(func, "Append a node group tree to the path");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
  parm = RNA_def_pointer(
      func, "node_tree", "NodeTree", "Node Tree", "Node tree to append to the node editor path");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_REQUIRED | PARM_RNAPTR);
  parm = RNA_def_pointer(func, "node", "Node", "Node", "Group node linking to this node tree");
  RNA_def_parameter_flags(parm, PropertyFlag(0), PARM_RNAPTR);

  func = RNA_def_function(srna, "pop", "rna_SpaceNodeEditor_path_pop");
  RNA_def_function_ui_description(func, "Remove the last node tree from the path");
  RNA_def_function_flag(func, FUNC_USE_CONTEXT);
}

static void rna_def_space_node_overlay(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem preview_shapes[] = {
      {SN_OVERLAY_PREVIEW_FLAT, "FLAT", ICON_MESH_PLANE, "Flat", "Use the default flat previews"},
      {SN_OVERLAY_PREVIEW_3D,
       "3D",
       ICON_SPHERE,
       "3D",
       "Use the material preview scene for the node previews"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "SpaceNodeOverlay", nullptr);
  RNA_def_struct_sdna(srna, "SpaceNode");
  RNA_def_struct_nested(brna, srna, "SpaceNodeEditor");
  RNA_def_struct_path_func(srna, "rna_SpaceNodeOverlay_path");
  RNA_def_struct_ui_text(
      srna, "Overlay Settings", "Settings for display of overlays in the Node Editor");

  prop = RNA_def_property(srna, "show_overlays", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", SN_OVERLAY_SHOW_OVERLAYS);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(prop, "Show Overlays", "Display overlays like colored or dashed wires");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE, nullptr);

  prop = RNA_def_property(srna, "show_wire_color", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", SN_OVERLAY_SHOW_WIRE_COLORS);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(
      prop, "Show Wire Colors", "Color node links based on their connected sockets");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE, nullptr);

  prop = RNA_def_property(srna, "show_reroute_auto_labels", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(
      prop, nullptr, "overlay.flag", SN_OVERLAY_SHOW_REROUTE_AUTO_LABELS);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_ui_text(prop,
                           "Show Reroute Auto Labels",
                           "Label reroute nodes based on the label of connected reroute nodes");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE, nullptr);

  prop = RNA_def_property(srna, "show_timing", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", SN_OVERLAY_SHOW_TIMINGS);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_ui_text(prop, "Show Timing", "Display each node's last execution time");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE, nullptr);

  prop = RNA_def_property(srna, "show_context_path", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", SN_OVERLAY_SHOW_PATH);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(prop, "Show Tree Path", "Display breadcrumbs for the editor's context");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE, nullptr);

  prop = RNA_def_property(srna, "show_named_attributes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", SN_OVERLAY_SHOW_NAMED_ATTRIBUTES);
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_ui_text(
      prop, "Show Named Attributes", "Show when nodes are using named attributes");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE, nullptr);

  prop = RNA_def_property(srna, "show_previews", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", SN_OVERLAY_SHOW_PREVIEWS);
  RNA_def_property_boolean_default(prop, false);
  RNA_def_property_ui_text(
      prop, "Show Node Previews", "Display each node's preview if node is toggled");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE, nullptr);

  prop = RNA_def_property(srna, "preview_shape", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "overlay.preview_shape");
  RNA_def_property_enum_items(prop, preview_shapes);
  RNA_def_property_enum_default(prop, SN_OVERLAY_PREVIEW_FLAT);
  RNA_def_property_ui_text(prop, "Preview Shape", "Preview shape used by the node previews");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE, nullptr);
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
      {0, nullptr, 0, nullptr, nullptr},
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
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem backdrop_channels_items[] = {
      {SNODE_USE_ALPHA,
       "COLOR_ALPHA",
       ICON_IMAGE_RGB_ALPHA,
       "Color & Alpha",
       "Display image with RGB colors and alpha transparency"},
      {0, "COLOR", ICON_IMAGE_RGB, "Color", "Display image with RGB colors"},
      {SNODE_SHOW_ALPHA, "ALPHA", ICON_IMAGE_ALPHA, "Alpha", "Display alpha transparency channel"},
      {SNODE_SHOW_R, "RED", ICON_RGB_RED, "Red", ""},
      {SNODE_SHOW_G, "GREEN", ICON_RGB_GREEN, "Green", ""},
      {SNODE_SHOW_B, "BLUE", ICON_RGB_BLUE, "Blue", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem insert_ofs_dir_items[] = {
      {SNODE_INSERTOFS_DIR_RIGHT, "RIGHT", 0, "Right"},
      {SNODE_INSERTOFS_DIR_LEFT, "LEFT", 0, "Left"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "SpaceNodeEditor", "Space");
  RNA_def_struct_sdna(srna, "SpaceNode");
  RNA_def_struct_ui_text(srna, "Space Node Editor", "Node editor space data");

  rna_def_space_generic_show_region_toggles(
      srna, (1 << RGN_TYPE_TOOLS) | (1 << RGN_TYPE_UI) | (1 << RGN_TYPE_ASSET_SHELF));

  prop = RNA_def_property(srna, "tree_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rna_enum_dummy_DEFAULT_items);
  RNA_def_property_enum_funcs(prop,
                              "rna_SpaceNodeEditor_tree_type_get",
                              "rna_SpaceNodeEditor_tree_type_set",
                              "rna_SpaceNodeEditor_tree_type_itemf");
  RNA_def_property_flag(prop, PROP_ENUM_NO_CONTEXT);
  RNA_def_property_ui_text(prop, "Tree Type", "Node tree type to display and edit");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE, nullptr);

  prop = RNA_def_property(srna, "texture_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "texfrom");
  RNA_def_property_enum_items(prop, texture_id_type_items);
  RNA_def_property_ui_text(prop, "Texture Type", "Type of data to take texture from");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ID);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE, nullptr);

  prop = RNA_def_property(srna, "shader_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "shaderfrom");
  RNA_def_property_enum_items(prop, shader_type_items);
  RNA_def_property_ui_text(prop, "Shader Type", "Type of data to take shader from");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ID);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE, nullptr);

  prop = RNA_def_property(srna, "node_tree_sub_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "node_tree_sub_type");
  RNA_def_property_enum_items(prop, rna_enum_dummy_NULL_items);
  RNA_def_property_enum_funcs(
      prop, nullptr, nullptr, "rna_SpaceNodeEditor_node_tree_sub_type_itemf");
  RNA_def_property_ui_text(prop, "Node Tree Sub-Type", "");
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_NODE, "rna_SpaceNodeEditor_node_tree_sub_type_update");

  prop = RNA_def_property(srna, "id", PROP_POINTER, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "ID", "Data-block whose nodes are being edited");

  prop = RNA_def_property(srna, "id_from", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "from");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(
      prop, "ID From", "Data-block from which the edited data-block is linked");

  prop = RNA_def_property(srna, "path", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "treepath", nullptr);
  RNA_def_property_struct_type(prop, "NodeTreePath");
  RNA_def_property_ui_text(
      prop, "Node Tree Path", "Path from the data-block to the currently edited node tree");
  rna_def_space_node_path_api(brna, prop);

  prop = RNA_def_property(srna, "node_tree", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_funcs(prop,
                                 nullptr,
                                 "rna_SpaceNodeEditor_node_tree_set",
                                 nullptr,
                                 "rna_SpaceNodeEditor_node_tree_poll");
  RNA_def_property_pointer_sdna(prop, nullptr, "nodetree");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_CONTEXT_UPDATE);
  RNA_def_property_clear_flag(prop, PROP_ID_REFCOUNT);
  RNA_def_property_ui_text(prop, "Node Tree", "Base node tree from context");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE, "rna_SpaceNodeEditor_node_tree_update");

  prop = RNA_def_property(srna, "edit_tree", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "edittree");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Edit Tree", "Node tree being displayed and edited");

  prop = RNA_def_property(srna, "pin", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SNODE_PIN);
  RNA_def_property_ui_text(prop, "Pinned", "Use the pinned node tree");
  RNA_def_property_ui_icon(prop, ICON_UNPINNED, 1);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE, nullptr);

  prop = RNA_def_property(srna, "show_backdrop", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SNODE_BACKDRAW);
  RNA_def_property_ui_text(
      prop, "Backdrop", "Use active Viewer Node output as backdrop for compositing nodes");
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_NODE_VIEW, "rna_SpaceNodeEditor_show_backdrop_update");

  prop = RNA_def_property(srna, "selected_node_group", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_funcs(
      prop, nullptr, nullptr, nullptr, "rna_SpaceNodeEditor_selected_node_group_poll");
  RNA_def_property_flag(prop, PROP_EDITABLE | PROP_CONTEXT_UPDATE);
  RNA_def_property_ui_text(prop, "Selected Node Group", "Node group to edit");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE, "rna_SpaceNodeEditor_node_tree_update");

  prop = RNA_def_property(srna, "show_annotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SNODE_SHOW_GPENCIL);
  RNA_def_property_ui_text(prop, "Show Annotation", "Show annotations for this view");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE_VIEW, nullptr);

  prop = RNA_def_property(srna, "backdrop_zoom", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "zoom");
  RNA_def_property_float_default(prop, 1.0f);
  RNA_def_property_range(prop, 0.01f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.01, 100, 1, 2);
  RNA_def_property_ui_text(prop, "Backdrop Zoom", "Backdrop zoom factor");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE_VIEW, nullptr);

  prop = RNA_def_property(srna, "backdrop_offset", PROP_FLOAT, PROP_NONE);
  RNA_def_property_float_sdna(prop, nullptr, "xof");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "Backdrop Offset", "Backdrop offset");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE_VIEW, nullptr);

  prop = RNA_def_property(srna, "backdrop_channels", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "flag");
  RNA_def_property_enum_items(prop, backdrop_channels_items);
  RNA_def_property_ui_text(prop, "Display Channels", "Channels of the image to draw");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE_VIEW, nullptr);
  /* the mx/my "cursor" in the node editor is used only by operators to store the mouse position */
  prop = RNA_def_property(srna, "cursor_location", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_array(prop, 2);
  RNA_def_property_float_funcs(prop,
                               "rna_SpaceNodeEditor_cursor_location_get",
                               "rna_SpaceNodeEditor_cursor_location_set",
                               nullptr);
  RNA_def_property_ui_text(prop, "Cursor Location", "Location for adding new nodes");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE_VIEW, nullptr);

  prop = RNA_def_property(srna, "insert_offset_direction", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_bitflag_sdna(prop, nullptr, "insert_ofs_dir");
  RNA_def_property_enum_items(prop, insert_ofs_dir_items);
  RNA_def_property_ui_text(
      prop, "Auto-offset Direction", "Direction to offset nodes on insertion");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE_VIEW, nullptr);

  /* Gizmo Toggles. */
  prop = RNA_def_property(srna, "show_gizmo", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "gizmo_flag", SNODE_GIZMO_HIDE);
  RNA_def_property_ui_text(prop, "Show Gizmo", "Show gizmos of all types");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE_VIEW, nullptr);

  prop = RNA_def_property(srna, "show_gizmo_active_node", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(
      prop, nullptr, "gizmo_flag", SNODE_GIZMO_HIDE_ACTIVE_NODE);
  RNA_def_property_ui_text(prop, "Active Node", "Context sensitive gizmo for the active node");
  RNA_def_property_boolean_default(prop, true);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_NODE_VIEW, nullptr);

  /* Overlays */
  prop = RNA_def_property(srna, "overlay", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "SpaceNodeOverlay");
  RNA_def_property_pointer_funcs(prop, "rna_SpaceNode_overlay_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(
      prop, "Overlay Settings", "Settings for display of overlays in the Node Editor");

  prop = RNA_def_property(srna, "supports_previews", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_funcs(prop, "rna_SpaceNode_supports_previews", nullptr);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop,
                           "Supports Previews",
                           "Whether the node editor's type supports displaying node previews");

  rna_def_space_node_overlay(brna);
  RNA_api_space_node(srna);
}

static void rna_def_space_clip_overlay(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SpaceClipOverlay", nullptr);
  RNA_def_struct_sdna(srna, "SpaceClip");
  RNA_def_struct_nested(brna, srna, "SpaceClipEditor");
  RNA_def_struct_path_func(srna, "rna_SpaceClipOverlay_path");
  RNA_def_struct_ui_text(
      srna, "Overlay Settings", "Settings for display of overlays in the Movie Clip editor");

  prop = RNA_def_property(srna, "show_overlays", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", SC_SHOW_OVERLAYS);
  RNA_def_property_ui_text(prop, "Show Overlays", "Display overlays like cursor and annotations");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  prop = RNA_def_property(srna, "show_cursor", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "overlay.flag", SC_SHOW_CURSOR);
  RNA_def_property_ui_text(prop, "Show Overlays", "Display 2D cursor");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);
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
       "Dope Sheet",
       "Dope Sheet view for tracking data"},
      {0, nullptr, 0, nullptr, nullptr},
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
      {0, nullptr, 0, nullptr, nullptr},
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
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "SpaceClipEditor", "Space");
  RNA_def_struct_sdna(srna, "SpaceClip");
  RNA_def_struct_ui_text(srna, "Space Clip Editor", "Clip editor space data");

  rna_def_space_generic_show_region_toggles(srna,
                                            (1 << RGN_TYPE_TOOLS) | (1 << RGN_TYPE_UI) |
                                                (1 << RGN_TYPE_HUD) | (1 << RGN_TYPE_CHANNELS));

  /* movieclip */
  prop = RNA_def_property(srna, "clip", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Movie Clip", "Movie clip displayed and edited in this space");
  RNA_def_property_pointer_funcs(prop, nullptr, "rna_SpaceClipEditor_clip_set", nullptr, nullptr);
  RNA_def_property_clear_flag(prop, PROP_ID_REFCOUNT);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* clip user */
  prop = RNA_def_property(srna, "clip_user", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "MovieClipUser");
  RNA_def_property_pointer_sdna(prop, nullptr, "user");
  RNA_def_property_ui_text(
      prop, "Movie Clip User", "Parameters defining which frame of the movie clip is displayed");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* mask */
  rna_def_space_mask_info(srna, NC_SPACE | ND_SPACE_CLIP, "rna_SpaceClipEditor_mask_set");

  /* mode */
  prop = RNA_def_property(srna, "mode", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "mode");
  RNA_def_property_enum_items(prop, rna_enum_clip_editor_mode_items);
  RNA_def_property_ui_text(prop, "Mode", "Editing context being displayed");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MOVIECLIP);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, "rna_SpaceClipEditor_clip_mode_update");

  /* view */
  prop = RNA_def_property(srna, "view", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "view");
  RNA_def_property_enum_items(prop, view_items);
  RNA_def_property_ui_text(prop, "View", "Type of the clip editor view");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MOVIECLIP);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, "rna_SpaceClipEditor_view_type_update");

  /* show pattern */
  prop = RNA_def_property(srna, "show_marker_pattern", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(prop, "Show Marker Pattern", "Show pattern boundbox for markers");
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SC_SHOW_MARKER_PATTERN);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* show search */
  prop = RNA_def_property(srna, "show_marker_search", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(prop, "Show Marker Search", "Show search boundbox for markers");
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SC_SHOW_MARKER_SEARCH);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* lock to selection */
  prop = RNA_def_property(srna, "lock_selection", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Lock to Selection", "Lock viewport to selected markers during playback");
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SC_LOCK_SELECTION);
  RNA_def_property_update(
      prop, NC_SPACE | ND_SPACE_CLIP, "rna_SpaceClipEditor_lock_selection_update");

  /* lock to time cursor */
  prop = RNA_def_property(srna, "lock_time_cursor", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "Lock to Time Cursor", "Lock curves view to time cursor during playback and tracking");
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SC_LOCK_TIMECURSOR);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* show markers paths */
  prop = RNA_def_property(srna, "show_track_path", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SC_SHOW_TRACK_PATH);
  RNA_def_property_ui_text(prop, "Show Track Path", "Show path of how track moves");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* path length */
  prop = RNA_def_property(srna, "path_length", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "path_length");
  RNA_def_property_range(prop, 0, INT_MAX);
  RNA_def_property_ui_text(prop, "Path Length", "Length of displaying path, in frames");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* show tiny markers */
  prop = RNA_def_property(srna, "show_tiny_markers", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(prop, "Show Tiny Markers", "Show markers in a more compact manner");
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SC_SHOW_TINY_MARKER);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* show bundles */
  prop = RNA_def_property(srna, "show_bundles", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(prop, "Show Bundles", "Show projection of 3D markers into footage");
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SC_SHOW_BUNDLES);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* mute footage */
  prop = RNA_def_property(srna, "use_mute_footage", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(prop, "Mute Footage", "Mute footage and show black background instead");
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SC_MUTE_FOOTAGE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* hide disabled */
  prop = RNA_def_property(srna, "show_disabled", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_ui_text(prop, "Show Disabled", "Show disabled tracks from the footage");
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "flag", SC_HIDE_DISABLED);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  prop = RNA_def_property(srna, "show_metadata", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SC_SHOW_METADATA);
  RNA_def_property_ui_text(prop, "Show Metadata", "Show metadata of clip");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* scopes */
  prop = RNA_def_property(srna, "scopes", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "scopes");
  RNA_def_property_struct_type(prop, "MovieClipScopes");
  RNA_def_property_ui_text(prop, "Scopes", "Scopes to visualize movie clip statistics");

  /* show names */
  prop = RNA_def_property(srna, "show_names", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SC_SHOW_NAMES);
  RNA_def_property_ui_text(prop, "Show Names", "Show track names and status");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* show grid */
  prop = RNA_def_property(srna, "show_grid", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SC_SHOW_GRID);
  RNA_def_property_ui_text(prop, "Show Grid", "Show grid showing lens distortion");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* show stable */
  prop = RNA_def_property(srna, "show_stable", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SC_SHOW_STABLE);
  RNA_def_property_ui_text(
      prop, "Show Stable", "Show stable footage in editor (if stabilization is enabled)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* manual calibration */
  prop = RNA_def_property(srna, "use_manual_calibration", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SC_MANUAL_CALIBRATION);
  RNA_def_property_ui_text(prop, "Manual Calibration", "Use manual calibration helpers");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* show annotation */
  prop = RNA_def_property(srna, "show_annotation", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SC_SHOW_ANNOTATION);
  RNA_def_property_ui_text(prop, "Show Annotation", "Show annotations for this view");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* show filters */
  prop = RNA_def_property(srna, "show_filters", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SC_SHOW_FILTERS);
  RNA_def_property_ui_text(prop, "Show Filters", "Show filters for graph editor");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* show graph_frames */
  prop = RNA_def_property(srna, "show_graph_frames", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SC_SHOW_GRAPH_FRAMES);
  RNA_def_property_ui_text(
      prop,
      "Show Frames",
      "Show curve for per-frame average error (camera motion should be solved first)");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* show graph tracks motion */
  prop = RNA_def_property(srna, "show_graph_tracks_motion", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SC_SHOW_GRAPH_TRACKS_MOTION);
  RNA_def_property_ui_text(
      prop, "Show Tracks Motion", "Display speed curves for the selected tracks");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* show graph tracks motion */
  prop = RNA_def_property(srna, "show_graph_tracks_error", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SC_SHOW_GRAPH_TRACKS_ERROR);
  RNA_def_property_ui_text(
      prop, "Show Tracks Error", "Display the reprojection error curve for selected tracks");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* show_only_selected */
  prop = RNA_def_property(srna, "show_graph_only_selected", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SC_SHOW_GRAPH_SEL_ONLY);
  RNA_def_property_ui_text(
      prop, "Only Show Selected", "Only include channels relating to selected objects and data");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_SELECT_OFF, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* show_hidden */
  prop = RNA_def_property(srna, "show_graph_hidden", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SC_SHOW_GRAPH_HIDDEN);
  RNA_def_property_ui_text(
      prop, "Display Hidden", "Include channels from objects/bone that are not visible");
  RNA_def_property_ui_icon(prop, ICON_GHOST_ENABLED, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* ** channels ** */

  /* show_red_channel */
  prop = RNA_def_property(srna, "show_red_channel", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "postproc_flag", MOVIECLIP_DISABLE_RED);
  RNA_def_property_ui_text(prop, "Show Red Channel", "Show red channel in the frame");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* show_green_channel */
  prop = RNA_def_property(srna, "show_green_channel", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "postproc_flag", MOVIECLIP_DISABLE_GREEN);
  RNA_def_property_ui_text(prop, "Show Green Channel", "Show green channel in the frame");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* show_blue_channel */
  prop = RNA_def_property(srna, "show_blue_channel", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "postproc_flag", MOVIECLIP_DISABLE_BLUE);
  RNA_def_property_ui_text(prop, "Show Blue Channel", "Show blue channel in the frame");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* preview_grayscale */
  prop = RNA_def_property(srna, "use_grayscale_preview", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "postproc_flag", MOVIECLIP_PREVIEW_GRAYSCALE);
  RNA_def_property_ui_text(prop, "Grayscale", "Display frame in grayscale mode");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* timeline */
  prop = RNA_def_property(srna, "show_seconds", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SC_SHOW_SECONDS);
  RNA_def_property_ui_text(prop, "Use Timecode", "Show timing as a timecode instead of frames");
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* grease pencil source */
  prop = RNA_def_property(srna, "annotation_source", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "gpencil_src");
  RNA_def_property_enum_items(prop, annotation_source_items);
  RNA_def_property_ui_text(prop, "Annotation Source", "Where the annotation comes from");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_MOVIECLIP);
  RNA_def_property_update(prop, NC_MOVIECLIP | ND_DISPLAY, nullptr);

  /* transform */
  prop = RNA_def_property(srna, "cursor_location", PROP_FLOAT, PROP_XYZ);
  RNA_def_property_float_sdna(prop, nullptr, "cursor");
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "2D Cursor Location", "2D cursor location for this view");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* pivot point */
  prop = RNA_def_property(srna, "pivot_point", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "around");
  RNA_def_property_enum_items(prop, pivot_items);
  RNA_def_property_ui_text(prop, "Pivot Point", "Pivot center for rotation/scaling");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* Gizmo Toggles. */
  prop = RNA_def_property(srna, "show_gizmo", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "gizmo_flag", SCLIP_GIZMO_HIDE);
  RNA_def_property_ui_text(prop, "Show Gizmo", "Show gizmos of all types");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  prop = RNA_def_property(srna, "show_gizmo_navigate", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_negative_sdna(prop, nullptr, "gizmo_flag", SCLIP_GIZMO_HIDE_NAVIGATE);
  RNA_def_property_ui_text(prop, "Navigate Gizmo", "Viewport navigation gizmo");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_CLIP, nullptr);

  /* Zoom. */
  prop = RNA_def_property(srna, "zoom_percentage", PROP_FLOAT, PROP_PERCENTAGE);
  RNA_def_property_float_funcs(prop,
                               "rna_SpaceClipEditor_zoom_percentage_get",
                               "rna_SpaceClipEditor_zoom_percentage_set",
                               nullptr);
  RNA_def_property_float_default(prop, 100.0);
  RNA_def_property_range(prop, .4f, 80000);
  RNA_def_property_ui_range(prop, 25, 400, 100, 0);
  RNA_def_property_ui_text(prop, "Zoom", "Zoom percentage");

  /* Overlays */
  prop = RNA_def_property(srna, "overlay", PROP_POINTER, PROP_NONE);
  RNA_def_property_flag(prop, PROP_NEVER_NULL);
  RNA_def_property_struct_type(prop, "SpaceClipOverlay");
  RNA_def_property_pointer_funcs(prop, "rna_SpaceClip_overlay_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(
      prop, "Overlay Settings", "Settings for display of overlays in the Movie Clip editor");

  rna_def_space_clip_overlay(brna);
}

static void rna_def_spreadsheet_column_id(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SpreadsheetColumnID", nullptr);
  RNA_def_struct_sdna(srna, "SpreadsheetColumnID");
  RNA_def_struct_ui_text(
      srna, "Spreadsheet Column ID", "Data used to identify a spreadsheet column");

  prop = RNA_def_property(srna, "name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Column Name", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);
}

static void rna_def_spreadsheet_column(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem data_type_items[] = {
      {SPREADSHEET_VALUE_TYPE_INT32, "INT32", ICON_NONE, "Integer", ""},
      {SPREADSHEET_VALUE_TYPE_FLOAT, "FLOAT", ICON_NONE, "Float", ""},
      {SPREADSHEET_VALUE_TYPE_BOOL, "BOOLEAN", ICON_NONE, "Boolean", ""},
      {SPREADSHEET_VALUE_TYPE_INSTANCES, "INSTANCES", ICON_NONE, "Instances", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "SpreadsheetColumn", nullptr);
  RNA_def_struct_sdna(srna, "SpreadsheetColumn");
  RNA_def_struct_ui_text(
      srna, "Spreadsheet Column", "Persistent data associated with a spreadsheet column");

  prop = RNA_def_property(srna, "data_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "data_type");
  RNA_def_property_enum_items(prop, data_type_items);
  RNA_def_property_ui_text(
      prop, "Data Type", "The data type of the corresponding column visible in the spreadsheet");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);

  rna_def_spreadsheet_column_id(brna);

  prop = RNA_def_property(srna, "id", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "SpreadsheetColumnID");
  RNA_def_property_ui_text(
      prop, "ID", "Data used to identify the corresponding data from the data source");
}

static void rna_def_spreadsheet_table_id(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SpreadsheetTableID", nullptr);
  RNA_def_struct_ui_text(
      srna, "Spreadsheet Table ID", "Data used to identify a spreadsheet table");
  RNA_def_struct_refine_func(srna, "rna_SpreadsheetTableID_refine");

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, spreadsheet_table_id_type_items);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Type", "The type of the table identifier");
}

static void rna_def_spreadsheet_table_id_geometry(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  /* The properties below are read-only, because they are used as key for a table. */
  srna = RNA_def_struct(brna, "SpreadsheetTableIDGeometry", "SpreadsheetTableID");

  prop = RNA_def_property(srna, "object_eval_state", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_items(prop, spreadsheet_object_eval_state_items);
  RNA_def_property_ui_text(prop, "Object Evaluation State", "");

  prop = RNA_def_property(srna, "geometry_component_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_items(prop, rna_enum_geometry_component_type_items);
  RNA_def_property_ui_text(
      prop, "Geometry Component", "Part of the geometry to display data from");

  prop = RNA_def_property(srna, "attribute_domain", PROP_ENUM, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_enum_items(prop, rna_enum_attribute_domain_items);
  RNA_def_property_ui_text(prop, "Attribute Domain", "Attribute domain to display");

  prop = RNA_def_property(srna, "viewer_path", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Viewer Path", "Path to the data that is displayed");

  prop = RNA_def_property(srna, "layer_index", PROP_INT, PROP_NONE);
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
  RNA_def_property_ui_text(prop, "Layer Index", "Index of the Grease Pencil layer");
}

static void rna_def_spreadsheet_table(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  rna_def_spreadsheet_table_id(brna);
  rna_def_spreadsheet_table_id_geometry(brna);
  rna_def_spreadsheet_column(brna);

  srna = RNA_def_struct(brna, "SpreadsheetTable", nullptr);
  RNA_def_struct_ui_text(srna, "Spreadsheet Table", "Persistent data associated with a table");

  prop = RNA_def_property(srna, "id", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "SpreadsheetTableID");
  RNA_def_property_ui_text(prop, "ID", "Data used to identify the table");

  prop = RNA_def_property(srna, "columns", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "SpreadsheetColumn");
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_SpreadsheetTable_columns_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    "rna_iterator_SpreadsheetTable_columns_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(prop, "Columns", "Columns within the table");
}

static void rna_def_spreadsheet_tables(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SpreadsheetTables", nullptr);
  RNA_def_struct_sdna(srna, "SpaceSpreadsheet");
  RNA_def_struct_ui_text(srna,
                         "Spreadsheet Tables",
                         "Active table and persisted state of previously displayed tables");

  prop = RNA_def_property(srna, "active", PROP_POINTER, PROP_NONE);
  RNA_def_property_struct_type(prop, "SpreadsheetTable");
  RNA_def_property_pointer_funcs(
      prop, "rna_SpreadsheetTables_active_get", nullptr, nullptr, nullptr);
  RNA_def_property_ui_text(prop, "Active Table", "");
}

static void rna_def_spreadsheet_row_filter(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  static const EnumPropertyItem rule_operation_items[] = {
      {SPREADSHEET_ROW_FILTER_EQUAL, "EQUAL", ICON_NONE, "Equal To", ""},
      {SPREADSHEET_ROW_FILTER_GREATER, "GREATER", ICON_NONE, "Greater Than", ""},
      {SPREADSHEET_ROW_FILTER_LESS, "LESS", ICON_NONE, "Less Than", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  srna = RNA_def_struct(brna, "SpreadsheetRowFilter", nullptr);
  RNA_def_struct_sdna(srna, "SpreadsheetRowFilter");
  RNA_def_struct_ui_text(srna, "Spreadsheet Row Filter", "");

  prop = RNA_def_property(srna, "enabled", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SPREADSHEET_ROW_FILTER_ENABLED);
  RNA_def_property_ui_text(prop, "Enabled", "");
  RNA_def_property_ui_icon(prop, ICON_CHECKBOX_DEHLT, 1);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);

  prop = RNA_def_property(srna, "show_expanded", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SPREADSHEET_ROW_FILTER_UI_EXPAND);
  RNA_def_property_ui_text(prop, "Show Expanded", "");
  RNA_def_property_ui_icon(prop, ICON_RIGHTARROW, 1);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);

  prop = RNA_def_property(srna, "column_name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Column Name", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);

  prop = RNA_def_property(srna, "operation", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, rule_operation_items);
  RNA_def_property_ui_text(prop, "Operation", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);

  prop = RNA_def_property(srna, "value_float", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Float Value", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);

  prop = RNA_def_property(srna, "value_float2", PROP_FLOAT, PROP_NONE);
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "2D Vector Value", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);

  prop = RNA_def_property(srna, "value_float3", PROP_FLOAT, PROP_NONE);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "Vector Value", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);

  prop = RNA_def_property(srna, "value_color", PROP_FLOAT, PROP_NONE);
  RNA_def_property_array(prop, 4);
  RNA_def_property_ui_text(prop, "Color Value", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);

  prop = RNA_def_property(srna, "value_string", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(prop, "Text Value", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);

  prop = RNA_def_property(srna, "threshold", PROP_FLOAT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Threshold", "How close float values need to be to be equal");
  RNA_def_property_range(prop, 0.0, FLT_MAX);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);

  prop = RNA_def_property(srna, "value_int", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "value_int");
  RNA_def_property_ui_text(prop, "Integer Value", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);

  prop = RNA_def_property(srna, "value_int8", PROP_INT, PROP_NONE);
  RNA_def_property_int_sdna(prop, nullptr, "value_int");
  RNA_def_property_range(prop, -128, 127);
  RNA_def_property_ui_text(prop, "8-Bit Integer Value", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);

  prop = RNA_def_property(srna, "value_int2", PROP_INT, PROP_NONE);
  RNA_def_property_array(prop, 2);
  RNA_def_property_ui_text(prop, "2D Vector Value", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);

  prop = RNA_def_property(srna, "value_int3", PROP_INT, PROP_NONE);
  RNA_def_property_array(prop, 3);
  RNA_def_property_ui_text(prop, "3D Vector Value", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);

  prop = RNA_def_property(srna, "value_boolean", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SPREADSHEET_ROW_FILTER_BOOL_VALUE);
  RNA_def_property_ui_text(prop, "Boolean Value", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);
}

static const EnumPropertyItem viewer_path_elem_type_items[] = {
    {VIEWER_PATH_ELEM_TYPE_ID, "ID", ICON_NONE, "ID", ""},
    {VIEWER_PATH_ELEM_TYPE_MODIFIER, "MODIFIER", ICON_NONE, "Modifier", ""},
    {VIEWER_PATH_ELEM_TYPE_GROUP_NODE, "GROUP_NODE", ICON_NONE, "Group Node", ""},
    {VIEWER_PATH_ELEM_TYPE_SIMULATION_ZONE, "SIMULATION_ZONE", ICON_NONE, "Simulation Zone", ""},
    {VIEWER_PATH_ELEM_TYPE_VIEWER_NODE, "VIEWER_NODE", ICON_NONE, "Viewer Node", ""},
    {VIEWER_PATH_ELEM_TYPE_REPEAT_ZONE, "REPEAT_ZONE", ICON_NONE, "Repeat", ""},
    {VIEWER_PATH_ELEM_TYPE_FOREACH_GEOMETRY_ELEMENT_ZONE,
     "FOREACH_GEOMETRY_ELEMENT_ZONE",
     ICON_NONE,
     "For Each Geometry Element",
     ""},
    {VIEWER_PATH_ELEM_TYPE_EVALUATE_CLOSURE, "EVALUATE_CLOSURE", ICON_NONE, "EvaluateClosure", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void rna_def_viewer_path_elem(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ViewerPathElem", nullptr);
  RNA_def_struct_ui_text(srna, "Viewer Path Element", "Element of a viewer path");
  RNA_def_struct_refine_func(srna, "rna_viewer_path_elem_refine");

  prop = RNA_def_property(srna, "type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_items(prop, viewer_path_elem_type_items);
  RNA_def_property_ui_text(prop, "Type", "Type of the path element");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);

  prop = RNA_def_property(srna, "ui_name", PROP_STRING, PROP_NONE);
  RNA_def_property_ui_text(
      prop, "UI Name", "Name that can be displayed in the UI for this element");
  RNA_def_property_clear_flag(prop, PROP_EDITABLE);
}

static void rna_def_id_viewer_path_elem(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "IDViewerPathElem", "ViewerPathElem");

  prop = RNA_def_property(srna, "id", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "ID", "");
}

static void rna_def_modifier_viewer_path_elem(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ModifierViewerPathElem", "ViewerPathElem");

  prop = RNA_def_property(srna, "modifier_uid", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Modifier UID", "The persistent UID of the modifier");
}

static void rna_def_group_node_viewer_path_elem(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "GroupNodeViewerPathElem", "ViewerPathElem");

  prop = RNA_def_property(srna, "node_id", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Node ID", "");
}

static void rna_def_simulation_zone_viewer_path_elem(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "SimulationZoneViewerPathElem", "ViewerPathElem");

  prop = RNA_def_property(srna, "sim_output_node_id", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Simulation Output Node ID", "");
}

static void rna_def_repeat_zone_viewer_path_elem(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "RepeatZoneViewerPathElem", "ViewerPathElem");

  prop = RNA_def_property(srna, "repeat_output_node_id", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Repeat Output Node ID", "");
}

static void rna_def_foreach_geometry_element_zone_viewer_path_elem(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ForeachGeometryElementZoneViewerPathElem", "ViewerPathElem");

  prop = RNA_def_property(srna, "zone_output_node_id", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Zone Output Node ID", "");
}

static void rna_def_evaluate_closure_node_viewer_path_elem(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "EvaluateClosureNodeViewerPathElem", "ViewerPathElem");

  prop = RNA_def_property(srna, "evaluate_node_id", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Evaluate Node ID", "");

  prop = RNA_def_property(srna, "source_output_node_id", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Closure Node ID", "");

  prop = RNA_def_property(srna, "source_node_tree", PROP_POINTER, PROP_NONE);
  RNA_def_property_ui_text(prop, "Source Tree", "");
}

static void rna_def_viewer_node_viewer_path_elem(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  srna = RNA_def_struct(brna, "ViewerNodeViewerPathElem", "ViewerPathElem");

  prop = RNA_def_property(srna, "node_id", PROP_INT, PROP_NONE);
  RNA_def_property_ui_text(prop, "Node ID", "");
}

static void rna_def_viewer_path(BlenderRNA *brna)
{
  StructRNA *srna;
  PropertyRNA *prop;

  rna_def_viewer_path_elem(brna);
  rna_def_id_viewer_path_elem(brna);
  rna_def_modifier_viewer_path_elem(brna);
  rna_def_group_node_viewer_path_elem(brna);
  rna_def_simulation_zone_viewer_path_elem(brna);
  rna_def_repeat_zone_viewer_path_elem(brna);
  rna_def_foreach_geometry_element_zone_viewer_path_elem(brna);
  rna_def_evaluate_closure_node_viewer_path_elem(brna);
  rna_def_viewer_node_viewer_path_elem(brna);

  srna = RNA_def_struct(brna, "ViewerPath", nullptr);
  RNA_def_struct_ui_text(srna, "Viewer Path", "Path to data that is viewed");

  prop = RNA_def_property(srna, "path", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "ViewerPathElem");
  RNA_def_property_ui_text(prop, "Viewer Path", nullptr);
}

static void rna_def_space_spreadsheet(BlenderRNA *brna)
{
  PropertyRNA *prop;
  StructRNA *srna;

  rna_def_spreadsheet_table(brna);
  rna_def_spreadsheet_tables(brna);

  srna = RNA_def_struct(brna, "SpaceSpreadsheet", "Space");
  RNA_def_struct_ui_text(srna, "Space Spreadsheet", "Spreadsheet space data");

  rna_def_space_generic_show_region_toggles(srna,
                                            (1 << RGN_TYPE_TOOLS) | (1 << RGN_TYPE_UI) |
                                                (1 << RGN_TYPE_CHANNELS) | (1 << RGN_TYPE_FOOTER));

  prop = RNA_def_property(srna, "is_pinned", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SPREADSHEET_FLAG_PINNED);
  RNA_def_property_ui_text(prop, "Is Pinned", "Context path is pinned");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);

  prop = RNA_def_property(srna, "show_internal_attributes", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "flag", SPREADSHEET_FLAG_SHOW_INTERNAL_ATTRIBUTES);
  RNA_def_property_ui_text(
      prop,
      "Show Internal Attributes",
      "Display attributes with names starting with a period that are meant for internal use");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);

  prop = RNA_def_property(srna, "use_filter", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filter_flag", SPREADSHEET_FILTER_ENABLE);
  RNA_def_property_ui_text(prop, "Use Filter", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);

  prop = RNA_def_property(srna, "viewer_path", PROP_POINTER, PROP_NONE);
  RNA_def_property_pointer_sdna(prop, nullptr, "geometry_id.viewer_path");
  RNA_def_property_ui_text(
      prop, "Viewer Path", "Path to the data that is displayed in the spreadsheet");

  prop = RNA_def_property(srna, "show_only_selected", PROP_BOOLEAN, PROP_NONE);
  RNA_def_property_boolean_sdna(prop, nullptr, "filter_flag", SPREADSHEET_FILTER_SELECTED_ONLY);
  RNA_def_property_ui_text(
      prop, "Show Only Selected", "Only include rows that correspond to selected elements");
  RNA_def_property_ui_icon(prop, ICON_RESTRICT_SELECT_OFF, 0);
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);

  prop = RNA_def_property(srna, "geometry_component_type", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "geometry_id.geometry_component_type");
  RNA_def_property_enum_items(prop, rna_enum_geometry_component_type_items);
  RNA_def_property_ui_text(
      prop, "Geometry Component", "Part of the geometry to display data from");
  RNA_def_property_update(prop,
                          NC_SPACE | ND_SPACE_SPREADSHEET,
                          "rna_SpaceSpreadsheet_geometry_component_type_update");

  prop = RNA_def_property(srna, "attribute_domain", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "geometry_id.attribute_domain");
  RNA_def_property_enum_items(prop, rna_enum_attribute_domain_items);
  RNA_def_property_enum_funcs(
      prop, nullptr, nullptr, "rna_SpaceSpreadsheet_attribute_domain_itemf");
  RNA_def_property_ui_text(prop, "Attribute Domain", "Attribute domain to display");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);

  prop = RNA_def_property(srna, "object_eval_state", PROP_ENUM, PROP_NONE);
  RNA_def_property_enum_sdna(prop, nullptr, "geometry_id.object_eval_state");
  RNA_def_property_enum_items(prop, spreadsheet_object_eval_state_items);
  RNA_def_property_ui_text(prop, "Object Evaluation State", "");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);

  prop = RNA_def_property(srna, "tables", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_type(prop, "SpreadsheetTable");
  RNA_def_property_srna(prop, "SpreadsheetTables");
  RNA_def_property_collection_funcs(prop,
                                    "rna_iterator_SpaceSpreadsheet_tables_begin",
                                    "rna_iterator_array_next",
                                    "rna_iterator_array_end",
                                    "rna_iterator_array_dereference_get",
                                    "rna_iterator_SpaceSpreadsheet_tables_length",
                                    nullptr,
                                    nullptr,
                                    nullptr);
  RNA_def_property_ui_text(
      prop, "Tables", "Persistent data for the tables shown in this spreadsheet editor");

  rna_def_spreadsheet_row_filter(brna);

  prop = RNA_def_property(srna, "row_filters", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_collection_sdna(prop, nullptr, "row_filters", nullptr);
  RNA_def_property_struct_type(prop, "SpreadsheetRowFilter");
  RNA_def_property_ui_text(prop, "Row Filters", "Filters to remove rows from the displayed data");
  RNA_def_property_update(prop, NC_SPACE | ND_SPACE_SPREADSHEET, nullptr);
}

void RNA_def_space(BlenderRNA *brna)
{
  rna_def_space(brna);
  rna_def_viewer_path(brna);
  rna_def_space_image(brna);
  rna_def_space_sequencer(brna);
  rna_def_space_text(brna);
  rna_def_fileselect_entry(brna);
  rna_def_fileselect_params(brna);
  rna_def_fileselect_asset_params(brna);
  rna_def_fileselect_idfilter(brna);
  rna_def_fileselect_asset_idfilter(brna);
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

/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_colorband_types.h"
#include "DNA_listBase.h"
#include "DNA_theme_types.h" /* IWYU pragma: export */
#include "DNA_userdef_enums.h"

struct ColorBand;
struct IDProperty;

typedef struct bAddon {
  struct bAddon *next, *prev;
  /**
   * 64 characters for a package prefix, 63 characters for the add-on name.
   */
  char module[128];
  /** User-Defined Properties on this add-on (for storing preferences). */
  struct IDProperty *prop;
} bAddon;

typedef struct bPathCompare {
  struct bPathCompare *next, *prev;
  char path[/*FILE_MAXDIR*/ 768];
  char flag;
  char _pad0[7];
} bPathCompare;

typedef struct bUserMenu {
  struct bUserMenu *next, *prev;
  char space_type;
  char _pad0[7];
  char context[64];
  /* bUserMenuItem */
  ListBase items;
} bUserMenu;

/** May be part of #bUserMenu or other list. */
typedef struct bUserMenuItem {
  struct bUserMenuItem *next, *prev;
  char ui_name[64];
  char type;
  char _pad0[7];
} bUserMenuItem;

typedef struct bUserMenuItem_Op {
  bUserMenuItem item;
  char op_idname[64];
  struct IDProperty *prop;
  char op_prop_enum[64];
  char opcontext; /* #blender::wm::OpCallContext */
  char _pad0[7];
} bUserMenuItem_Op;

typedef struct bUserMenuItem_Menu {
  bUserMenuItem item;
  char mt_idname[64];
} bUserMenuItem_Menu;

typedef struct bUserMenuItem_Prop {
  bUserMenuItem item;
  char context_data_path[256];
  char prop_id[64];
  int prop_index;
  char _pad0[4];
} bUserMenuItem_Prop;

enum {
  USER_MENU_TYPE_SEP = 1,
  USER_MENU_TYPE_OPERATOR = 2,
  USER_MENU_TYPE_MENU = 3,
  USER_MENU_TYPE_PROP = 4,
};

typedef struct bUserAssetLibrary {
  struct bUserAssetLibrary *next, *prev;

  char name[/*MAX_NAME*/ 64];
  char dirpath[/*FILE_MAX*/ 1024];

  short import_method; /* eAssetImportMethod */
  short flag;          /* eAssetLibrary_Flag */
  char _pad0[4];
} bUserAssetLibrary;

typedef struct bUserExtensionRepo {
  struct bUserExtensionRepo *next, *prev;
  /**
   * Unique identifier, only for display in the UI list.
   * The `module` is used for internal identifiers.
   */
  char name[/*MAX_NAME*/ 64];
  /**
   * The unique module name (sub-module) in fact.
   *
   * Use a shorter name than #NAME_MAX to leave room for a base module prefix.
   * e.g. `bl_ext.{submodule}.{add_on}` to allow this string to fit into #bAddon::module.
   */
  char module[/*MAX_NAME - 16*/ 48];

  /**
   * Secret access token for remote repositories (allocated).
   * Only use when #USER_EXTENSION_REPO_FLAG_USE_ACCESS_TOKEN is set.
   */
  char *access_token;

  /**
   * The "local" directory where extensions are stored.
   * When unset, use `{BLENDER_USER_EXTENSIONS}/{bUserExtensionRepo::module}`.
   */
  char custom_dirpath[/*FILE_MAX*/ 1024];
  char remote_url[/*FILE_MAX*/ 1024];

  /** Options for the repository (#eUserExtensionRepo_Flag). */
  uint8_t flag;
  /** The source location when the custom directory isn't used (#eUserExtensionRepo_Source). */
  uint8_t source;

  char _pad0[6];
} bUserExtensionRepo;

typedef enum eUserExtensionRepo_Flag {
  /** Maintain disk cache. */
  USER_EXTENSION_REPO_FLAG_NO_CACHE = 1 << 0,
  USER_EXTENSION_REPO_FLAG_DISABLED = 1 << 1,
  USER_EXTENSION_REPO_FLAG_USE_CUSTOM_DIRECTORY = 1 << 2,
  USER_EXTENSION_REPO_FLAG_USE_REMOTE_URL = 1 << 3,
  USER_EXTENSION_REPO_FLAG_SYNC_ON_STARTUP = 1 << 4,
  USER_EXTENSION_REPO_FLAG_USE_ACCESS_TOKEN = 1 << 5,
} eUserExtensionRepo_Flag;

/**
 * The source to use (User or System), only valid when the
 * #USER_EXTENSION_REPO_FLAG_USE_REMOTE_URL flag isn't set.
 */
typedef enum eUserExtensionRepo_Source {
  USER_EXTENSION_REPO_SOURCE_USER = 0,
  USER_EXTENSION_REPO_SOURCE_SYSTEM = 1,
} eUserExtensionRepo_Source;

typedef struct SolidLight {
  int flag;
  float smooth;
  float col[4], spec[4], vec[4];
} SolidLight;

typedef struct WalkNavigation {
  /** Speed factor for look around. */
  float mouse_speed;
  float walk_speed;
  float walk_speed_factor;
  float view_height;
  float jump_height;
  /** Duration to use for teleporting. */
  float teleport_time;
  short flag;
  char _pad0[6];
} WalkNavigation;

typedef struct UserDef_Runtime {
  /** Mark as changed so the preferences are saved on exit. */
  char is_dirty;
  char _pad0[7];
} UserDef_Runtime;

/**
 * Store UI data here instead of the space
 * since the space is typically a window which is freed.
 */
typedef struct UserDef_SpaceData {
  char section_active;
  /** #eUserPref_SpaceData_Flag UI options. */
  char flag;
  char _pad0[6];
} UserDef_SpaceData;

/**
 * Storage for UI data that to keep it even after the window was closed. (Similar to
 * #UserDef_SpaceData.)
 */
typedef struct UserDef_FileSpaceData {
  int display_type;   /* FileSelectParams.display */
  int thumbnail_size; /* FileSelectParams.thumbnail_size */
  int sort_type;      /* FileSelectParams.sort */
  int details_flags;  /* FileSelectParams.details_flags */
  int flag;           /* FileSelectParams.flag */
  int _pad0;
  uint64_t filter_id; /* FileSelectParams.filter_id */

  /** Info used when creating the file browser in a temporary window. */
  int temp_win_sizex;
  int temp_win_sizey;
} UserDef_FileSpaceData;

/**
 * Checking experimental members must use the #USER_EXPERIMENTAL_TEST() macro
 * unless the #USER_DEVELOPER_UI is known to be enabled.
 */
typedef struct UserDef_Experimental {
  /* Debug options, always available. */
  char use_undo_legacy;
  char no_override_auto_resync;
  char use_cycles_debug;
  char use_eevee_debug;
  char show_asset_debug_info;
  char no_asset_indexing;
  char use_viewport_debug;
  char use_all_linked_data_direct;
  char use_extensions_debug;
  char use_recompute_usercount_on_save_debug;
  char write_legacy_blend_file_format;
  char SANITIZE_AFTER_HERE;
  /* The following options are automatically sanitized (set to 0)
   * when the release cycle is not alpha. */
  char use_new_curves_tools;
  char use_extended_asset_browser;
  char use_sculpt_texture_paint;
  char use_new_volume_nodes;
  char use_shader_node_previews;
  char use_bundle_and_closure_nodes;
  char use_socket_structure_type;
  char use_geometry_nodes_lists;
  char _pad[4];
} UserDef_Experimental;

#define USER_EXPERIMENTAL_TEST(userdef, member) \
  (((userdef)->flag & USER_DEVELOPER_UI) && ((userdef)->experimental).member)

/**
 * Container to store multiple directory paths and a name for each as a #ListBase.
 */
typedef struct bUserScriptDirectory {
  struct bUserScriptDirectory *next, *prev;

  /** Name must be unique. */
  char name[/*MAX_NAME*/ 64];
  char dir_path[/*FILE_MAXDIR*/ 768];
} bUserScriptDirectory;

/**
 * Settings for an asset shelf, stored in the Preferences. Most settings are still stored in the
 * asset shelf instance in #AssetShelfSettings. This is just for the options that should be shared
 * as Preferences.
 */
typedef struct bUserAssetShelfSettings {
  struct bUserAssetShelfSettings *next, *prev;

  /** Identifier that matches the #AssetShelfType.idname of the shelf these settings apply to. */
  char shelf_idname[/*MAX_NAME*/ 64];

  ListBase enabled_catalog_paths; /* #AssetCatalogPathLink */
} bUserAssetShelfSettings;

/**
 * Main user preferences data, typically accessed from #U.
 * See: #BKE_blendfile_userdef_from_defaults & #BKE_blendfile_userdef_read.
 *
 * \note This is either loaded from the file #BLENDER_USERPREF_FILE or from memory, see #U_default.
 */
typedef struct UserDef {
  DNA_DEFINE_CXX_METHODS(UserDef)

  /** UserDef has separate do-version handling, and can be read from other files. */
  int versionfile, subversionfile;

  /** #eUserPref_Flag. */
  int flag;
  /** #eDupli_ID_Flags. */
  unsigned int dupflag;
  /** #eUserPref_PrefFlag preferences for the preferences. */
  char pref_flag;
  char savetime;
  char mouse_emulate_3_button_modifier;
  /**
   * Workaround for WAYLAND (at time of writing compositors don't support this info).
   * #eUserpref_TrackpadScrollDir type
   * TODO: Remove this once this API is better supported by Wayland compositors, see #107676.
   */
  char trackpad_scroll_direction;
  /**  length. */
  char tempdir[/*FILE_MAXDIR*/ 768];
  char fontdir[/*FILE_MAXDIR*/ 768];
  char renderdir[/*FILE_MAX*/ 1024];
  /* EXR cache path */
  char render_cachedir[/*FILE_MAXDIR*/ 768];
  char textudir[/*FILE_MAXDIR*/ 768];
  /* Deprecated, use #UserDef.script_directories instead. */
  char pythondir_legacy[/*FILE_MAXDIR*/ 768] DNA_DEPRECATED;
  char sounddir[/*FILE_MAXDIR*/ 768];
  char i18ndir[/*FILE_MAXDIR*/ 768];
  char image_editor[/*FILE_MAX*/ 1024];
  char text_editor[/*FILE_MAX*/ 1024];
  char text_editor_args[256];
  char anim_player[/*FILE_MAX*/ 1024];
  int anim_player_preset;

  /** Minimum spacing between grid-lines in View2D grids. */
  short v2d_min_gridsize;
  /** #eTimecodeStyles, style of time-code display. */
  short timecode_style;

  short versions;
  short dbl_click_time;

  char _pad0[2];

  /** Space around each area. Inter-editor gap width. */
  char border_width;

  char mini_axis_type;
  /** #eUserpref_UI_Flag. */
  int uiflag;
  /** #eUserpref_UI_Flag2. */
  char uiflag2;
  char gpu_flag;
  char _pad8[6];
  /* Experimental flag for app-templates to make changes to behavior
   * which are outside the scope of typical preferences. */
  char app_flag;
  char viewzoom;
  short language;

  int mixbufsize;
  int audiodevice;
  int audiorate;
  int audioformat;
  int audiochannels;

  /** Setting for UI scale (fractional), before screen DPI has been applied. */
  float ui_scale;
  /**
   * Setting for UI line width.
   *
   * In most cases this should not be used directly it is an offset used to calculate `pixelsize`
   * which should be used to define the line width.
   */
  int ui_line_width;
  /** Runtime, full DPI divided by `pixelsize`. */
  int dpi;
  /** Runtime multiplier to scale UI elements. Use macro UI_SCALE_FAC instead of this. */
  float scale_factor;
  /** Runtime, `1.0 / scale_factor` */
  float inv_scale_factor;
  /**
   * Runtime, calculated from line-width and point-size based on DPI.
   *
   * - Rounded down to an integer, clamped to a minimum of 1.0.
   * - This includes both the UI scale and windowing system's DPI.
   *   so a HI-DPI display of 200% with a UI scale of 3.0 results in a pixel-size of 6.0
   *   (when the line-width is set to auto).
   * - The line-width is added to this value, so lines & vertex drawing can be adjusted.
   *
   * \note This should never be used as a UI scale value otherwise changing the line-width
   * could double or halve the size of UI elements. Use #UI_SCALE_FAC instead.
   */
  float pixelsize;
  /** Deprecated, for forward compatibility. */
  int virtual_pixel;

  /** Console scroll-back limit. */
  int scrollback;
  /** Node insert offset (aka auto-offset) margin, but might be useful for later stuff as well. */
  char node_margin;
  char node_preview_res;
  /** #eUserpref_Translation_Flags. */
  short transopts;
  short menuthreshold1, menuthreshold2;

  /** Startup application template. */
  char app_template[64];

  /**
   * A list of themes (#bTheme), the first is only used currently.
   * But there may be multiple themes in the list.
   */
  struct ListBase themes;
  struct ListBase uifonts;
  struct ListBase uistyles;
  struct ListBase user_keymaps;
  /** #wmKeyConfigPref. */
  struct ListBase user_keyconfig_prefs;
  struct ListBase addons;
  struct ListBase autoexec_paths;
  /**
   * Optional user locations for Python scripts.
   *
   * This supports the same layout as Blender's scripts directory `scripts`.
   *
   * \note Unlike most paths, changing this is not fully supported at run-time,
   * requiring a restart to properly take effect. Supporting this would cause complications as
   * the script path can contain `startup`, `addons` & `modules` etc. properly unwinding the
   * Python environment to the state it _would_ have been in gets complicated.
   *
   * Although this is partially supported as the `sys.path` is refreshed when loading preferences.
   * This is done to support #PREFERENCES_OT_copy_prev which is available to the user when they
   * launch with a new version of Blender. In this case setting the script path on top of
   * factory settings will work without problems.
   */
  ListBase script_directories; /* #bUserScriptDirectory */
  /** #bUserMenu. */
  struct ListBase user_menus;
  /** #bUserAssetLibrary */
  struct ListBase asset_libraries;
  /** #bUserExtensionRepo */
  struct ListBase extension_repos;
  struct ListBase asset_shelves_settings; /* #bUserAssetShelfSettings */

  char keyconfigstr[64];

  /** Index of the asset library being edited in the Preferences UI. */
  short active_asset_library;

  /** Index of the extension repo in the Preferences UI. */
  short active_extension_repo;
  /** Flag for all extensions (#eUserPref_ExtensionFlag). */
  char extension_flag;

  /* Network settings, used by extensions but not specific to extensions. */

  /** Time in seconds to wait before timing out online operation (0 uses the systems default). */
  uint8_t network_timeout;
  /** Maximum number of simulations connection limit for online operations. */
  uint8_t network_connection_limit;

  char _pad14[3];

  short undosteps;
  int undomemory;
  float gpu_viewport_quality DNA_DEPRECATED;
  short gp_manhattandist, gp_euclideandist, gp_eraser;
  /** #eGP_UserdefSettings. */
  short gp_settings;
  char _pad13[4];
  struct SolidLight light_param[4];
  float light_ambient[3];
  char gizmo_flag;
  /** Generic gizmo size. */
  char gizmo_size;
  /** Navigate gizmo size. */
  char gizmo_size_navigate_v3d;
  char _pad3[5];
  short edit_studio_light;
  short lookdev_sphere_size;
  short vbotimeout, vbocollectrate;
  short textimeout, texcollectrate;
  int memcachelimit;
  /** Unused. */
  int prefetchframes;
  /** Control the rotation step of the view when PAD2, PAD4, PAD6&PAD8 is use. */
  float pad_rot_angle;
  char _pad12[4];
  /** Rotating view icon size. */
  short rvisize;
  /** Rotating view icon brightness. */
  short rvibright;
  /** Maximum number of recently used files to remember. */
  short recent_files;
  /** Milliseconds to spend spinning the view. */
  short smooth_viewtx;
  short glreslimit;
  /** #eColorPicker_Types. */
  short color_picker_type;
  /** Curve smoothing type for newly added F-Curves. */
  char auto_smoothing_new;
  /** Interpolation mode for newly added F-Curves. */
  char ipo_new;
  /** Handle types for newly added keyframes. */
  char keyhandles_new;
  char _pad11[4];
  /** #eZoomFrame_Mode. */
  char view_frame_type;

  /** Number of keyframes to zoom around current frame. */
  int view_frame_keyframes;
  /** Seconds to zoom around current frame. */
  float view_frame_seconds;

  /** Preferred device/vendor for GPU device selection. */
  int gpu_preferred_index;
  uint32_t gpu_preferred_vendor_id;
  uint32_t gpu_preferred_device_id;

  /** Max number of parallel shader compilation workers. */
  short gpu_shader_workers;
  /** eUserpref_ShaderCompileMethod (OpenGL only). */
  short shader_compilation_method;

  char _pad16[2];

  /** #eGPUBackendType */
  short gpu_backend;

  /** Number of samples for FPS display calculations. */
  short playback_fps_samples;

  /** Private, defaults to 20 for 72 DPI setting. */
  short widget_unit;
  short anisotropic_filter;

  /** Tablet API to use (Windows only). */
  short tablet_api;

  /** Raw tablet pressure that maps to 100%. */
  float pressure_threshold_max;
  /** Curve non-linearity parameter. */
  float pressure_softness;

  /** 3D mouse: overall translation sensitivity. */
  float ndof_translation_sensitivity;
  /** 3D mouse: overall rotation sensitivity. */
  float ndof_rotation_sensitivity;
  /** 3D mouse: dead-zone. */
  float ndof_deadzone;
  /** #eNdof_Flag, flags for 3D mouse. */
  int ndof_flag;
  /** #eNdof_Navigation_Mode, current navigation mode. */
  uint8_t ndof_navigation_mode;
  char _pad17[1];

  /** eImageDrawMethod, Method to be used to draw the images
   * (AUTO, GLSL, Textures or DrawPixels) */
  short image_draw_method;

  float glalphaclip;

  /** #eAutokey_Mode, auto-keying mode. */
  short autokey_mode;
  /** Flags for inserting keyframes. */
  short keying_flag;
  /** Flags for which channels to insert keys at. */
  short key_insert_channels;  // eKeyInsertChannels
  char _pad15[6];
  /** Flags for animation. */
  short animation_flag;

  /** Options for text rendering. */
  char text_render;
  char navigation_mode;

  /** Turn-table rotation amount per-pixel in radians. Scaled with DPI. */
  float view_rotate_sensitivity_turntable;
  /** Track-ball rotation scale. */
  float view_rotate_sensitivity_trackball;

  /** From texture.h. */
  struct ColorBand coba_weight;

  float sculpt_paint_overlay_col[3];
  /** Default color for newly created Grease Pencil layers. */
  float gpencil_new_layer_col[4];

  /** Drag pixels (scaled by DPI). */
  char drag_threshold_mouse;
  char drag_threshold_tablet;
  char drag_threshold;
  char move_threshold;

  char font_path_ui[/*FILE_MAX*/ 1024];
  char font_path_ui_mono[/*FILE_MAX*/ 1024];

  /** Legacy, for backwards compatibility only. */
  int compute_device_type;

  /** Opacity of inactive F-Curves in F-Curve Editor. */
  float fcu_inactive_alpha;

  /**
   * If keeping a pie menu spawn button pressed after this time,
   * it turns into a drag/release pie menu.
   */
  short pie_tap_timeout;
  /**
   * Direction in the pie menu will always be calculated from the
   * initial position within this time limit.
   */
  short pie_initial_timeout;
  short pie_animation_timeout;
  short pie_menu_confirm;
  /** Pie menu radius. */
  short pie_menu_radius;
  /** Pie menu distance from center before a direction is set. */
  short pie_menu_threshold;

  int sequencer_editor_flag; /* eUserpref_SeqEditorFlags */

  char factor_display_type;

  char viewport_aa;

  char render_display_type;      /* eUserpref_RenderDisplayType */
  char filebrowser_display_type; /* eUserpref_TempSpaceDisplayType */

  short sequencer_proxy_setup; /* eUserpref_SeqProxySetup */
  short _pad1;

  float collection_instance_empty_size;
  char text_flag;
  char _pad10[1];

  char file_preview_type; /* eUserpref_File_Preview_Type */
  char statusbar_flag;    /* eUserpref_StatusBar_Flag */

  struct WalkNavigation walk_navigation;

  /** The UI for the user preferences. */
  UserDef_SpaceData space_data;
  UserDef_FileSpaceData file_space_data;

  UserDef_Experimental experimental;

  /** Runtime data (keep last). */
  UserDef_Runtime runtime;
} UserDef;

/** From `source/blender/blenkernel/intern/blender.cc`. */
extern UserDef U;

/* ***************** USERDEF ****************** */

/* Toggles for unfinished 2.8 UserPref design. */
// #define WITH_USERDEF_WORKSPACES

/** #UserDef_SpaceData.section_active (UI active_section) */
typedef enum eUserPref_Section {
  USER_SECTION_INTERFACE = 0,
  USER_SECTION_EDITING = 1,
  USER_SECTION_SAVE_LOAD = 2,
  USER_SECTION_SYSTEM = 3,
  USER_SECTION_THEME = 4,
  USER_SECTION_INPUT = 5,
  USER_SECTION_ADDONS = 6,
  USER_SECTION_LIGHT = 7,
  USER_SECTION_KEYMAP = 8,
#ifdef WITH_USERDEF_WORKSPACES
  USER_SECTION_WORKSPACE_CONFIG = 9,
  USER_SECTION_WORKSPACE_ADDONS = 10,
  USER_SECTION_WORKSPACE_KEYMAPS = 11,
#endif
  USER_SECTION_VIEWPORT = 12,
  USER_SECTION_ANIMATION = 13,
  USER_SECTION_NAVIGATION = 14,
  USER_SECTION_FILE_PATHS = 15,
  USER_SECTION_EXPERIMENTAL = 16,
  USER_SECTION_EXTENSIONS = 17,
} eUserPref_Section;

/** #UserDef_SpaceData.flag (State of the user preferences UI). */
typedef enum eUserPref_SpaceData_Flag {
  /** Hide/expand key-map preferences. */
  USER_SPACEDATA_INPUT_HIDE_UI_KEYCONFIG = (1 << 0),
  USER_SPACEDATA_ADDONS_SHOW_ONLY_ENABLED = (1 << 1),
} eUserPref_SpaceData_Flag;

/** #UserDef.flag */
typedef enum eUserPref_Flag {
  USER_AUTOSAVE = (1 << 0),
  USER_FLAG_NUMINPUT_ADVANCED = (1 << 1),
  USER_FLAG_RECENT_SEARCHES_DISABLE = (1 << 2),
  USER_FLAG_UNUSED_3 = (1 << 3), /* cleared */
  USER_FLAG_UNUSED_4 = (1 << 4), /* cleared */
  USER_TRACKBALL = (1 << 5),
  USER_FLAG_UNUSED_6 = (1 << 6), /* cleared */
  USER_FLAG_UNUSED_7 = (1 << 7), /* cleared */
  USER_MAT_ON_OB = (1 << 8),
  USER_INTERNET_ALLOW = (1 << 9),
  USER_DEVELOPER_UI = (1 << 10),
  USER_TOOLTIPS = (1 << 11),
  USER_TWOBUTTONMOUSE = (1 << 12),
  USER_NONUMPAD = (1 << 13),
  USER_ADD_CURSORALIGNED = (1 << 14),
  USER_FILECOMPRESS = (1 << 15),
  USER_FLAG_UNUSED_5 = (1 << 16), /* dirty */
  USER_CUSTOM_RANGE = (1 << 17),
  USER_ADD_EDITMODE = (1 << 18),
  USER_ADD_VIEWALIGNED = (1 << 19),
  USER_RELPATHS = (1 << 20),
  USER_RELEASECONFIRM = (1 << 21),
  USER_SCRIPT_AUTOEXEC_DISABLE = (1 << 22),
  USER_FILENOUI = (1 << 23),
  USER_NONEGFRAMES = (1 << 24),
  USER_TXT_TABSTOSPACES_DISABLE = (1 << 25),
  USER_TOOLTIPS_PYTHON = (1 << 26),
  USER_FLAG_UNUSED_27 = (1 << 27), /* dirty */
} eUserPref_Flag;

/** #UserDef.extension_flag */
typedef enum eUserPref_ExtensionFlag {
  USER_EXTENSION_FLAG_ONLINE_ACCESS_HANDLED = 1 << 0,
} eUserPref_ExtensionFlag;

/** #UserDef.file_preview_type */
typedef enum eUserpref_File_Preview_Type {
  USER_FILE_PREVIEW_NONE = 0,
  USER_FILE_PREVIEW_AUTO,
  USER_FILE_PREVIEW_SCREENSHOT,
  USER_FILE_PREVIEW_CAMERA,
} eUserpref_File_Preview_Type;

typedef enum eUserPref_PrefFlag {
  USER_PREF_FLAG_SAVE = (1 << 0),
} eUserPref_PrefFlag;

/** #bPathCompare.flag */
typedef enum ePathCompare_Flag {
  USER_PATHCMP_GLOB = (1 << 0),
} ePathCompare_Flag;

/* Helper macro for checking frame clamping */
#define FRAMENUMBER_MIN_CLAMP(cfra) \
  { \
    if ((U.flag & USER_NONEGFRAMES) && (cfra < 0)) { \
      cfra = 0; \
    } \
  } \
  (void)0

/** #UserDef.viewzoom */
typedef enum eViewZoom_Style {
  /** Update zoom continuously with a timer while dragging the cursor. */
  USER_ZOOM_CONTINUE = 0,
  /** Map changes in distance from the view center to zoom. */
  USER_ZOOM_SCALE = 1,
  /** Map horizontal/vertical motion to zoom. */
  USER_ZOOM_DOLLY = 2,
} eViewZoom_Style;

/** #UserDef.navigation_mode */
typedef enum eViewNavigation_Method {
  VIEW_NAVIGATION_WALK = 0,
  VIEW_NAVIGATION_FLY = 1,
} eViewNavigation_Method;

/** #UserDef.uiflag */
typedef enum eUserpref_MiniAxisType {
  USER_MINI_AXIS_TYPE_GIZMO = 0,
  USER_MINI_AXIS_TYPE_MINIMAL = 1,
  USER_MINI_AXIS_TYPE_NONE = 2,
} eUserpref_MiniAxisType;

/** #UserDef.flag */
typedef enum eWalkNavigation_Flag {
  USER_WALK_GRAVITY = (1 << 0),
  USER_WALK_MOUSE_REVERSE = (1 << 1),
} eWalkNavigation_Flag;

/** #UserDef.uiflag */
typedef enum eUserpref_UI_Flag {
  USER_NO_MULTITOUCH_GESTURES = (1 << 0),
  USER_UIFLAG_UNUSED_1 = (1 << 1), /* cleared */
  USER_WHEELZOOMDIR = (1 << 2),
  USER_FILTERFILEEXTS = (1 << 3),
  USER_DRAWVIEWINFO = (1 << 4),
  USER_PLAINMENUS = (1 << 5),
  USER_LOCK_CURSOR_ADJUST = (1 << 6),
  USER_HEADER_BOTTOM = (1 << 7),
  /** Otherwise use header alignment from the file. */
  USER_HEADER_FROM_PREF = (1 << 8),
  USER_MENUOPENAUTO = (1 << 9),
  USER_DEPTH_CURSOR = (1 << 10),
  USER_AUTOPERSP = (1 << 11),
  USER_NODE_AUTO_OFFSET = (1 << 12),
  USER_GLOBALUNDO = (1 << 13),
  USER_ORBIT_SELECTION = (1 << 14),
  USER_DEPTH_NAVIGATE = (1 << 15),
  USER_HIDE_DOT = (1 << 16),
  USER_SHOW_GIZMO_NAVIGATE = (1 << 17),
  USER_SHOW_VIEWPORTNAME = (1 << 18),
  USER_UIFLAG_UNUSED_3 = (1 << 19), /* Cleared. */
  USER_ZOOM_TO_MOUSEPOS = (1 << 20),
  USER_SHOW_FPS = (1 << 21),
  USER_REGISTER_ALL_USERS = (1 << 22),
  /** Actually implemented in .py. */
  USER_FILTER_BRUSHES_BY_TOOL = (1 << 23),
  USER_CONTINUOUS_MOUSE = (1 << 24),
  USER_ZOOM_INVERT = (1 << 25),
  USER_ZOOM_HORIZ = (1 << 26), /* for CONTINUE and DOLLY zoom */
  USER_SPLASH_DISABLE = (1 << 27),
  USER_HIDE_RECENT = (1 << 28),
#ifdef DNA_DEPRECATED_ALLOW
  /* Deprecated: We're just trying if there's much desire for this feature,
   * or if we can make it go for good. Should be cleared if so - Julian, Oct. 2019. */
  USER_SHOW_THUMBNAILS = (1 << 29),
#endif
  USER_SAVE_PROMPT = (1 << 30),
  USER_HIDE_SYSTEM_BOOKMARKS = (1u << 31),
} eUserpref_UI_Flag;

/**
 * #UserDef.uiflag2
 *
 * \note don't add new flags here, use 'uiflag' which has flags free.
 */
typedef enum eUserpref_UI_Flag2 {
  USER_UIFLAG2_UNUSED_0 = (1 << 0), /* cleared */
  USER_REGION_OVERLAP = (1 << 1),
  USER_UIFLAG2_UNUSED_2 = (1 << 2),
  USER_UIFLAG2_UNUSED_3 = (1 << 3), /* dirty */
} eUserpref_UI_Flag2;

/** #UserDef.gpu_flag */
typedef enum eUserpref_GPU_Flag {
  USER_GPU_FLAG_UNUSED_0 = (1 << 0), /* Unused. To be removed. */
  USER_GPU_FLAG_NO_EDIT_MODE_SMOOTH_WIRE = (1 << 1),
  USER_GPU_FLAG_OVERLAY_SMOOTH_WIRE = (1 << 2),
  USER_GPU_FLAG_SUBDIVISION_EVALUATION = (1 << 3),
  USER_GPU_FLAG_FRESNEL_EDIT = (1 << 4),
} eUserpref_GPU_Flag;

/** #UserDef.gpu_backend
 * NOTE: Keep in sync with eGPUBackendType. */
enum eUserPref_GPUBackendType {
  USER_GPU_BACKEND_OPENGL = 1 << 0,
  USER_GPU_BACKEND_METAL = 1 << 1,
  USER_GPU_BACKEND_VULKAN = 1 << 3,
#ifdef __APPLE__
  USER_GPU_BACKEND_DEFAULT = USER_GPU_BACKEND_METAL,
#else
  USER_GPU_BACKEND_DEFAULT = USER_GPU_BACKEND_OPENGL,
#endif
};

/** #UserDef.tablet_api */
typedef enum eUserpref_TableAPI {
  USER_TABLET_AUTOMATIC = 0,
  USER_TABLET_NATIVE = 1,
  USER_TABLET_WINTAB = 2,
} eUserpref_TabletAPI;

/** #UserDef.app_flag */
typedef enum eUserpref_APP_Flag {
  USER_APP_LOCK_CORNER_SPLIT = (1 << 0),
  USER_APP_HIDE_REGION_TOGGLE = (1 << 1),
  USER_APP_LOCK_EDGE_RESIZE = (1 << 2),
} eUserpref_APP_Flag;

/** #UserDef.statusbar_flag */
typedef enum eUserpref_StatusBar_Flag {
  STATUSBAR_SHOW_MEMORY = (1 << 0),
  STATUSBAR_SHOW_VRAM = (1 << 1),
  STATUSBAR_SHOW_STATS = (1 << 2),
  STATUSBAR_SHOW_VERSION = (1 << 3),
  STATUSBAR_SHOW_SCENE_DURATION = (1 << 4),
  STATUSBAR_SHOW_EXTENSIONS_UPDATES = (1 << 5),
} eUserpref_StatusBar_Flag;

/**
 * Auto-Keying mode.
 * #UserDef.autokey_mode
 */
typedef enum eAutokey_Mode {
  /* AUTOKEY_ON is a bit-flag. */
  AUTOKEY_ON = 1,

  /**
   * AUTOKEY_ON + 2**n...  (i.e. AUTOKEY_MODE_NORMAL = AUTOKEY_ON + 2)
   * to preserve setting, even when auto-key turned off.
   */
  AUTOKEY_MODE_NORMAL = 3,
  AUTOKEY_MODE_EDITKEYS = 5,
} eAutokey_Mode;

/**
 * Zoom to frame mode.
 * #UserDef.view_frame_type
 */
typedef enum eZoomFrame_Mode {
  ZOOM_FRAME_MODE_KEEP_RANGE = 0,
  ZOOM_FRAME_MODE_SECONDS = 1,
  ZOOM_FRAME_MODE_KEYFRAMES = 2,
} eZoomFrame_Mode;

/**
 * Defines how keyframes are inserted.
 * Used for regular keying and auto-keying.
 * Not all of those flags are stored in the user preferences (U.keying_flag).
 * Some are stored on the scene (toolsettings.keying_flag).
 */
typedef enum eKeying_Flag {
  /* Settings used across manual and auto-keying. */
  KEYING_FLAG_VISUALKEY = (1 << 2),
  KEYING_FLAG_XYZ2RGB = (1 << 3),
  KEYING_FLAG_CYCLEAWARE = (1 << 8),

  /* Auto-key options. */
  AUTOKEY_FLAG_INSERTAVAILABLE = (1 << 0),
  AUTOKEY_FLAG_INSERTNEEDED = (1 << 1),
  AUTOKEY_FLAG_ONLYKEYINGSET = (1 << 6),
  AUTOKEY_FLAG_NOWARNING = (1 << 7),
  AUTOKEY_FLAG_LAYERED_RECORD = (1 << 10),

  /* Manual Keying options. */
  MANUALKEY_FLAG_INSERTNEEDED = (1 << 11),
} eKeying_Flag;

typedef enum eKeyInsertChannels {
  USER_ANIM_KEY_CHANNEL_LOCATION = (1 << 0),
  USER_ANIM_KEY_CHANNEL_ROTATION = (1 << 1),
  USER_ANIM_KEY_CHANNEL_SCALE = (1 << 2),
  USER_ANIM_KEY_CHANNEL_ROTATION_MODE = (1 << 3),
  USER_ANIM_KEY_CHANNEL_CUSTOM_PROPERTIES = (1 << 4),
} eKeyInsertChannels;

/**
 * Animation flags
 * #UserDef.animation_flag, used for animation flags that aren't covered by more specific flags
 * (like eKeying_Flag).
 */
typedef enum eUserpref_Anim_Flags {
  USER_ANIM_SHOW_CHANNEL_GROUP_COLORS = (1 << 0),
  USER_ANIM_ONLY_SHOW_SELECTED_CURVE_KEYS = (1 << 1),
  USER_ANIM_HIGH_QUALITY_DRAWING = (1 << 2),
} eUserpref_Anim_Flags;

/** #UserDef.transopts */
typedef enum eUserpref_Translation_Flags {
  USER_TR_TOOLTIPS = (1 << 0),
  USER_TR_IFACE = (1 << 1),
  USER_TR_REPORTS = (1 << 2),
  USER_TR_UNUSED_3 = (1 << 3),            /* cleared */
  USER_TR_UNUSED_4 = (1 << 4),            /* cleared */
  USER_DOTRANSLATE_DEPRECATED = (1 << 5), /* Deprecated in 2.83. */
  USER_TR_UNUSED_6 = (1 << 6),            /* cleared */
  USER_TR_UNUSED_7 = (1 << 7),            /* cleared */
  USER_TR_NEWDATANAME = (1 << 8),
} eUserpref_Translation_Flags;

/**
 * Text Editor options
 * #UserDef.text_flag
 */
typedef enum eTextEdit_Flags {
  USER_TEXT_EDIT_AUTO_CLOSE = (1 << 0),
} eTextEdit_Flags;

/**
 * Text draw options
 * #UserDef.text_render
 */
typedef enum eText_Draw_Options {
  USER_TEXT_DISABLE_AA = (1 << 0),

  USER_TEXT_HINTING_NONE = (1 << 1),
  USER_TEXT_HINTING_SLIGHT = (1 << 2),
  USER_TEXT_HINTING_FULL = (1 << 3),

  USER_TEXT_RENDER_SUBPIXELAA = (1 << 4),
} eText_Draw_Options;

/**
 * Grease Pencil Settings.
 * #UserDef.gp_settings
 */
typedef enum eGP_UserdefSettings {
  GP_PAINT_UNUSED_0 = (1 << 0),
} eGP_UserdefSettings;

enum {
  USER_GIZMO_DRAW = (1 << 0),
};

/**
 * Color Picker Types.
 * #UserDef.color_picker_type
 */
typedef enum eColorPicker_Types {
  USER_CP_CIRCLE_HSV = 0,
  USER_CP_SQUARE_SV = 1,
  USER_CP_SQUARE_HS = 2,
  USER_CP_SQUARE_HV = 3,
  USER_CP_CIRCLE_HSL = 4,
} eColorPicker_Types;

/**
 * Time-code display styles.
 * #UserDef.timecode_style
 */
typedef enum eTimecodeStyles {
  /**
   * As little info as is necessary to show relevant info with '+' to denote the frames
   * i.e. HH:MM:SS+FF, MM:SS+FF, SS+FF, or MM:SS.
   */
  USER_TIMECODE_MINIMAL = 0,
  /** Reduced SMPTE - (HH:)MM:SS:FF */
  USER_TIMECODE_SMPTE_MSF = 1,
  /** Full SMPTE - HH:MM:SS:FF */
  USER_TIMECODE_SMPTE_FULL = 2,
  /** Milliseconds for sub-frames - HH:MM:SS.sss. */
  USER_TIMECODE_MILLISECONDS = 3,
  /** Seconds only. */
  USER_TIMECODE_SECONDS_ONLY = 4,
  /**
   * Private (not exposed as generic choices) options.
   * milliseconds for sub-frames, SubRip format- HH:MM:SS,sss.
   */
  USER_TIMECODE_SUBRIP = 100,
} eTimecodeStyles;

/** #UserDef.ndof_flag (3D mouse options) */
typedef enum eNdof_Flag {
  NDOF_SHOW_GUIDE_ORBIT_AXIS = (1 << 0),
  NDOF_FLY_HELICOPTER = (1 << 1),
  NDOF_LOCK_HORIZON = (1 << 2),

  /* The following might not need to be saved between sessions,
   * but they do need to live somewhere accessible. */
  NDOF_SHOULD_PAN = (1 << 3),
  NDOF_SHOULD_ZOOM = (1 << 4),
  NDOF_SHOULD_ROTATE = (1 << 5),

  // NDOF_UNUSED_6 = (1 << 6), /* Dirty. */
  /**
   * When set translation results in zoom being up/down otherwise forward/backward
   * This also swaps Y/Z for rotation.
   */
  NDOF_SWAP_YZ_AXIS = (1 << 7),
  // NDOF_UNUSED_8 = (1 << 8), /* Dirty. */
  NDOF_ROTX_INVERT_AXIS = (1 << 9),
  NDOF_ROTY_INVERT_AXIS = (1 << 10),
  NDOF_ROTZ_INVERT_AXIS = (1 << 11),
  NDOF_PANX_INVERT_AXIS = (1 << 12),
  NDOF_PANY_INVERT_AXIS = (1 << 13),
  NDOF_PANZ_INVERT_AXIS = (1 << 14),
  NDOF_TURNTABLE = (1 << 15),
  NDOF_CAMERA_PAN_ZOOM = (1 << 16),
  NDOF_ORBIT_CENTER_AUTO = (1 << 17),
  NDOF_ORBIT_CENTER_SELECTED = (1 << 18),
  NDOF_SHOW_GUIDE_ORBIT_CENTER = (1 << 19),
} eNdof_Flag;

/**
 * NDOF Navigation Modes.
 * Each mode describes some style of navigation rather than control a single aspect of navigation.
 */
typedef enum eNdof_Navigation_Mode {
  /**
   * 3D mouse cap represents objects movement in 3D space.
   * Pulling the cap will pull the objects closer to the camera.
   */
  NDOF_NAVIGATION_MODE_OBJECT = 0,
  /**
   * 3D mouse cap controls the movement of the view window
   * and allows for flying through the scene.
   */
  NDOF_NAVIGATION_MODE_FLY = 1,
  /* TODO: implement "Target Camera Mode" and "Drone Mode" */
} eNdof_Navigation_Mode;

/**
 * Some navigation modes make use of "Auto Center" (#NDOF_ORBIT_CENTER_AUTO) and some don't.
 * Instead of testing against all possibilities use a macro.
 *
 * TODO: Add Target Camera Mode when implemented.
 */
#define NDOF_IS_ORBIT_AROUND_CENTER_MODE(userdef) \
  ((userdef)->ndof_navigation_mode == NDOF_NAVIGATION_MODE_OBJECT)

#define NDOF_PIXELS_PER_SECOND 600.0f

/** UserDef.ogl_multisamples */
typedef enum eMultiSample_Type {
  USER_MULTISAMPLE_NONE = 0,
  USER_MULTISAMPLE_2 = 2,
  USER_MULTISAMPLE_4 = 4,
  USER_MULTISAMPLE_8 = 8,
  USER_MULTISAMPLE_16 = 16,
} eMultiSample_Type;

/** #UserDef.image_draw_method */
typedef enum eImageDrawMethod {
  IMAGE_DRAW_METHOD_AUTO = 0,
  IMAGE_DRAW_METHOD_GLSL = 1,
  IMAGE_DRAW_METHOD_2DTEXTURE = 2,
} eImageDrawMethod;

/** #UserDef.virtual_pixel */
typedef enum eUserpref_VirtualPixel {
  VIRTUAL_PIXEL_NATIVE = 0,
  VIRTUAL_PIXEL_DOUBLE = 1,
} eUserpref_VirtualPixel;

/** #UserDef.factor_display_type */
typedef enum eUserpref_FactorDisplay {
  USER_FACTOR_AS_FACTOR = 0,
  USER_FACTOR_AS_PERCENTAGE = 1,
} eUserpref_FactorDisplay;

typedef enum eUserpref_RenderDisplayType {
  USER_RENDER_DISPLAY_NONE = 0,
  USER_RENDER_DISPLAY_SCREEN = 1,
  USER_RENDER_DISPLAY_AREA = 2,
  USER_RENDER_DISPLAY_WINDOW = 3
} eUserpref_RenderDisplayType;

typedef enum eUserpref_TempSpaceDisplayType {
  USER_TEMP_SPACE_DISPLAY_FULLSCREEN = 0,
  USER_TEMP_SPACE_DISPLAY_WINDOW = 1,
} eUserpref_TempSpaceDisplayType;

typedef enum eUserpref_EmulateMMBMod {
  USER_EMU_MMB_MOD_ALT = 0,
  USER_EMU_MMB_MOD_OSKEY = 1,
} eUserpref_EmulateMMBMod;

typedef enum eUserpref_TrackpadScrollDir {
  USER_TRACKPAD_SCROLL_DIR_TRADITIONAL = 0,
  USER_TRACKPAD_SCROLL_DIR_NATURAL = 1,
} eUserpref_TrackpadScrollDir;

typedef enum eUserpref_DiskCacheCompression {
  USER_SEQ_DISK_CACHE_COMPRESSION_NONE = 0,
  USER_SEQ_DISK_CACHE_COMPRESSION_LOW = 1,
  USER_SEQ_DISK_CACHE_COMPRESSION_HIGH = 2,
} eUserpref_DiskCacheCompression;

typedef enum eUserpref_SeqProxySetup {
  USER_SEQ_PROXY_SETUP_MANUAL = 0,
  USER_SEQ_PROXY_SETUP_AUTOMATIC = 1,
} eUserpref_SeqProxySetup;

typedef enum eUserpref_SeqEditorFlags {
  USER_SEQ_ED_UNUSED_0 = (1 << 0), /* Dirty. */
  USER_SEQ_ED_CONNECT_STRIPS_BY_DEFAULT = (1 << 1),
} eUserpref_SeqEditorFlags;

typedef enum eUserpref_ShaderCompileMethod {
  USER_SHADER_COMPILE_THREAD = 0,
  USER_SHADER_COMPILE_SUBPROCESS = 1,
} eUserpref_ShaderCompileMethod;

/* Locale Ids. Auto will try to get local from OS. Our default is English though. */
/** #UserDef.language */
enum {
  ULANGUAGE_AUTO = 0,
  ULANGUAGE_ENGLISH = 1,
};

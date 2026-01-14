/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "BLI_math_constants.h"

#include "DNA_ID.h"
#include "DNA_anim_enums.h"
#include "DNA_asset_types.h"
#include "DNA_colorband_types.h"
#include "DNA_curve_enums.h"
#include "DNA_defs.h"
#include "DNA_listBase.h"
#include "DNA_space_enums.h"
#include "DNA_theme_types.h" /* IWYU pragma: export */
#include "DNA_userdef_enums.h"
#include "DNA_vec_types.h"

namespace blender {

struct IDProperty;
struct bUserMenuItem;

/** #UserDef.flag */
enum eUserPref_Flag {
  USER_AUTOSAVE = (1 << 0),
  USER_FLAG_NUMINPUT_ADVANCED = (1 << 1),
  USER_FLAG_RECENT_SEARCHES_DISABLE = (1 << 2),
  USER_MENU_CLOSE_LEAVE = (1 << 3),
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
  USER_HIDE_DOT_DATABLOCK = (1 << 28),
};

/** #UserDef.extension_flag */
enum eUserPref_ExtensionFlag {
  USER_EXTENSION_FLAG_ONLINE_ACCESS_HANDLED = 1 << 0,
};

/** #UserDef.file_preview_type */
enum eUserpref_File_Preview_Type {
  USER_FILE_PREVIEW_NONE = 0,
  USER_FILE_PREVIEW_AUTO,
  USER_FILE_PREVIEW_SCREENSHOT,
  USER_FILE_PREVIEW_CAMERA,
};

enum eUserPref_PrefFlag {
  USER_PREF_FLAG_SAVE = (1 << 0),
};

/* Helper macro for checking frame clamping */
#define FRAMENUMBER_MIN_CLAMP(cfra) \
  { \
    if ((U.flag & USER_NONEGFRAMES) && (cfra < 0)) { \
      cfra = 0; \
    } \
  } \
  (void)0

/** #UserDef.viewzoom */
enum eViewZoom_Style {
  /** Update zoom continuously with a timer while dragging the cursor. */
  USER_ZOOM_CONTINUE = 0,
  /** Map changes in distance from the view center to zoom. */
  USER_ZOOM_SCALE = 1,
  /** Map horizontal/vertical motion to zoom. */
  USER_ZOOM_DOLLY = 2,
};

/** #UserDef.navigation_mode */
enum eViewNavigation_Method {
  VIEW_NAVIGATION_WALK = 0,
  VIEW_NAVIGATION_FLY = 1,
};

/** #UserDef.uiflag */
enum eUserpref_MiniAxisType {
  USER_MINI_AXIS_TYPE_GIZMO = 0,
  USER_MINI_AXIS_TYPE_MINIMAL = 1,
  USER_MINI_AXIS_TYPE_NONE = 2,
};

/** #UserDef.flag */
enum eWalkNavigation_Flag {
  USER_WALK_GRAVITY = (1 << 0),
  USER_WALK_MOUSE_REVERSE = (1 << 1),
};

/** #UserDef.uiflag */
enum eUserpref_UI_Flag {
  USER_NO_MULTITOUCH_GESTURES = (1 << 0),
  USER_REDUCE_MOTION = (1 << 1),
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
  USER_AREA_CORNER_HANDLE = (1 << 19),
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
};

/**
 * #UserDef.uiflag2
 *
 * \note don't add new flags here, use 'uiflag' which has flags free.
 */
enum eUserpref_UI_Flag2 {
  USER_ALWAYS_SHOW_NUMBER_ARROWS = (1 << 0), /* cleared */
  USER_REGION_OVERLAP = (1 << 1),
  USER_UIFLAG2_UNUSED_2 = (1 << 2),
  USER_UIFLAG2_UNUSED_3 = (1 << 3), /* dirty */
};

/** #UserDef.gpu_flag */
enum eUserpref_GPU_Flag {
  USER_GPU_FLAG_UNUSED_0 = (1 << 0), /* Unused. To be removed. */
  USER_GPU_FLAG_NO_EDIT_MODE_SMOOTH_WIRE = (1 << 1),
  USER_GPU_FLAG_OVERLAY_SMOOTH_WIRE = (1 << 2),
  USER_GPU_FLAG_SUBDIVISION_EVALUATION = (1 << 3),
  USER_GPU_FLAG_FRESNEL_EDIT = (1 << 4),
};

/** #UserDef.gpu_backend
 * NOTE: Keep in sync with GPUBackendType. */
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
enum eUserpref_TabletAPI {
  USER_TABLET_AUTOMATIC = 0,
  USER_TABLET_NATIVE = 1,
  USER_TABLET_WINTAB = 2,
};

/**
 * #UserDef.tablet_flag
 */
enum eUserPref_Tablet_Flags {
  USER_TABLET_SHOW_DEBUG_VALUES = (1 << 0),
};

/** #UserDef.app_flag */
enum eUserpref_APP_Flag {
  USER_APP_LOCK_CORNER_SPLIT = (1 << 0),
  USER_APP_HIDE_REGION_TOGGLE = (1 << 1),
  USER_APP_LOCK_EDGE_RESIZE = (1 << 2),
};

/** #UserDef.statusbar_flag */
enum eUserpref_StatusBar_Flag {
  STATUSBAR_SHOW_MEMORY = (1 << 0),
  STATUSBAR_SHOW_VRAM = (1 << 1),
  STATUSBAR_SHOW_STATS = (1 << 2),
  STATUSBAR_SHOW_VERSION = (1 << 3),
  STATUSBAR_SHOW_SCENE_DURATION = (1 << 4),
  STATUSBAR_SHOW_EXTENSIONS_UPDATES = (1 << 5),
};

/**
 * Auto-Keying mode.
 * #UserDef.autokey_mode
 */
enum eAutokey_Mode {
  /* AUTOKEY_ON is a bit-flag. */
  AUTOKEY_ON = 1,

  /**
   * AUTOKEY_ON + 2**n...  (i.e. AUTOKEY_MODE_NORMAL = AUTOKEY_ON + 2)
   * to preserve setting, even when auto-key turned off.
   */
  AUTOKEY_MODE_NORMAL = 3,
  AUTOKEY_MODE_EDITKEYS = 5,
};

/**
 * Zoom to frame mode.
 * #UserDef.view_frame_type
 */
enum eZoomFrame_Mode {
  ZOOM_FRAME_MODE_KEEP_RANGE = 0,
  ZOOM_FRAME_MODE_SECONDS = 1,
  ZOOM_FRAME_MODE_KEYFRAMES = 2,
};

/**
 * Defines how keyframes are inserted.
 * Used for regular keying and auto-keying.
 * Not all of those flags are stored in the user preferences (U.keying_flag).
 * Some are stored on the scene (toolsettings.keying_flag).
 */
enum eKeying_Flag {
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
};

enum eKeyInsertChannels {
  USER_ANIM_KEY_CHANNEL_LOCATION = (1 << 0),
  USER_ANIM_KEY_CHANNEL_ROTATION = (1 << 1),
  USER_ANIM_KEY_CHANNEL_SCALE = (1 << 2),
  USER_ANIM_KEY_CHANNEL_ROTATION_MODE = (1 << 3),
  USER_ANIM_KEY_CHANNEL_CUSTOM_PROPERTIES = (1 << 4),
};

/**
 * Animation flags
 * #UserDef.animation_flag, used for animation flags that aren't covered by more specific flags
 * (like eKeying_Flag).
 */
enum eUserpref_Anim_Flags {
  USER_ANIM_SHOW_CHANNEL_GROUP_COLORS = (1 << 0),
  USER_ANIM_ONLY_SHOW_SELECTED_CURVE_KEYS = (1 << 1),
  USER_ANIM_HIGH_QUALITY_DRAWING = (1 << 2),
};

enum eFixToCam_Flags {
  FIX_TO_CAM_FLAG_USE_LOC = (1 << 0),
  FIX_TO_CAM_FLAG_USE_ROT = (1 << 1),
  FIX_TO_CAM_FLAG_USE_SCALE = (1 << 2),
};

/** #UserDef.transopts */
enum eUserpref_Translation_Flags {
  USER_TR_TOOLTIPS = (1 << 0),
  USER_TR_IFACE = (1 << 1),
  USER_TR_REPORTS = (1 << 2),
  USER_TR_UNUSED_3 = (1 << 3),            /* cleared */
  USER_TR_UNUSED_4 = (1 << 4),            /* cleared */
  USER_DOTRANSLATE_DEPRECATED = (1 << 5), /* Deprecated in 2.83. */
  USER_TR_UNUSED_6 = (1 << 6),            /* cleared */
  USER_TR_UNUSED_7 = (1 << 7),            /* cleared */
  USER_TR_NEWDATANAME = (1 << 8),
};

/**
 * Text Editor options
 * #UserDef.text_flag
 */
enum eTextEdit_Flags {
  USER_TEXT_EDIT_AUTO_CLOSE = (1 << 0),
};

/**
 * Text draw options
 * #UserDef.text_render
 */
enum eText_Draw_Options {
  USER_TEXT_DISABLE_AA = (1 << 0),

  USER_TEXT_HINTING_NONE = (1 << 1),
  USER_TEXT_HINTING_SLIGHT = (1 << 2),
  USER_TEXT_HINTING_FULL = (1 << 3),

  USER_TEXT_RENDER_SUBPIXELAA = (1 << 4),
};

/**
 * Grease Pencil Settings.
 * #UserDef.gp_settings
 */
enum eGP_UserdefSettings {
  GP_PAINT_UNUSED_0 = (1 << 0),
};

enum {
  USER_GIZMO_DRAW = (1 << 0),
};

/**
 * Color Picker Types.
 * #UserDef.color_picker_type
 */
enum eColorPicker_Types {
  USER_CP_CIRCLE_HSV = 0,
  USER_CP_SQUARE_SV = 1,
  USER_CP_SQUARE_HS = 2,
  USER_CP_SQUARE_HV = 3,
  USER_CP_CIRCLE_HSL = 4,
};

/**
 * Time-code display styles.
 * #UserDef.timecode_style
 */
enum eTimecodeStyles {
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
};

/** #UserDef.ndof_flag (3D mouse options) */
enum eNdof_Flag {
  NDOF_SHOW_GUIDE_ORBIT_AXIS = (1 << 0),
  NDOF_FLY_HELICOPTER = (1 << 1),
  /**
   * \note In most cases this flag shouldn't be checked directly.
   * Use #NDOF_IS_HORIZON_LOCKED instead.
   */
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
  /** Must only be used when `!NDOF_IS_ORBIT_AROUND_CENTER_MODE(&U)`. */
  NDOF_FLY_SPEED_AUTO = (1 << 20),
};

/**
 * NDOF Navigation Modes.
 * Each mode describes some style of navigation rather than control a single aspect of navigation.
 */
enum eNdof_Navigation_Mode {
  /**
   * 3D mouse cap represents objects movement in 3D space.
   * Pulling the cap will pull the objects closer to the camera.
   */
  NDOF_NAVIGATION_MODE_OBJECT = 0,
  /**
   * 3D mouse cap controls the movement of the view window
   * and allows for flying through the scene.
   *
   * \note this also inverts navigation for 2D views,
   * since it's confusing for users when 2D/3D navigation is inverted, see: #144751.
   */
  NDOF_NAVIGATION_MODE_FLY = 1,
  /**
   * A "Fly Mode" style navigation but pushing the cap forward
   * while looking down will not change the altitude of the camera.
   */
  NDOF_NAVIGATION_MODE_DRONE = 2,
  /* TODO: implement "Target Camera Mode" */
};

/**
 * Some navigation modes make use of "Auto Center" (#NDOF_ORBIT_CENTER_AUTO) and some don't.
 * Instead of testing against all possibilities use a macro.
 *
 * TODO: Add Target Camera Mode when implemented.
 */
#define NDOF_IS_ORBIT_AROUND_CENTER_MODE(userdef) \
  ((userdef)->ndof_navigation_mode == NDOF_NAVIGATION_MODE_OBJECT)

#define NDOF_IS_HORIZON_LOCKED(userdef) \
  ((userdef)->ndof_navigation_mode == NDOF_NAVIGATION_MODE_DRONE) || \
      ((userdef)->ndof_flag & NDOF_LOCK_HORIZON)

#define NDOF_PIXELS_PER_SECOND 600.0f

/** UserDef.ogl_multisamples */
enum eMultiSample_Type {
  USER_MULTISAMPLE_NONE = 0,
  USER_MULTISAMPLE_2 = 2,
  USER_MULTISAMPLE_4 = 4,
  USER_MULTISAMPLE_8 = 8,
  USER_MULTISAMPLE_16 = 16,
};

/** #UserDef.image_draw_method */
enum eImageDrawMethod {
  IMAGE_DRAW_METHOD_AUTO = 0,
  IMAGE_DRAW_METHOD_GLSL = 1,
  IMAGE_DRAW_METHOD_2DTEXTURE = 2,
};

/** #UserDef.virtual_pixel */
enum eUserpref_VirtualPixel {
  VIRTUAL_PIXEL_NATIVE = 0,
  VIRTUAL_PIXEL_DOUBLE = 1,
};

/** #UserDef.factor_display_type */
enum eUserpref_FactorDisplay {
  USER_FACTOR_AS_FACTOR = 0,
  USER_FACTOR_AS_PERCENTAGE = 1,
};

/** #UserDef.xr_navigation_flag */
enum eUserpref_XrNavigationFlags {
  USER_XR_NAV_SNAP_TURN = (1 << 0),
  USER_XR_NAV_INVERT_ROTATION = (1 << 1),
};

enum eUserpref_RenderDisplayType {
  USER_RENDER_DISPLAY_NONE = 0,
  USER_RENDER_DISPLAY_SCREEN = 1,
  USER_RENDER_DISPLAY_AREA = 2,
  USER_RENDER_DISPLAY_WINDOW = 3
};

enum eUserpref_TempSpaceDisplayType {
  USER_TEMP_SPACE_DISPLAY_FULLSCREEN = 0,
  USER_TEMP_SPACE_DISPLAY_WINDOW = 1,
};

enum eUserpref_EmulateMMBMod {
  USER_EMU_MMB_MOD_ALT = 0,
  USER_EMU_MMB_MOD_OSKEY = 1,
};

enum eUserpref_TrackpadScrollDir {
  USER_TRACKPAD_SCROLL_DIR_TRADITIONAL = 0,
  USER_TRACKPAD_SCROLL_DIR_NATURAL = 1,
};

enum eUserpref_DiskCacheCompression {
  USER_SEQ_DISK_CACHE_COMPRESSION_NONE = 0,
  USER_SEQ_DISK_CACHE_COMPRESSION_LOW = 1,
  USER_SEQ_DISK_CACHE_COMPRESSION_HIGH = 2,
};

enum eUserpref_SeqProxySetup {
  USER_SEQ_PROXY_SETUP_MANUAL = 0,
  USER_SEQ_PROXY_SETUP_AUTOMATIC = 1,
};

enum eUserpref_SeqEditorFlags {
  USER_SEQ_ED_UNUSED_0 = (1 << 0), /* Dirty. */
  USER_SEQ_ED_CONNECT_STRIPS_BY_DEFAULT = (1 << 1),
};

enum eUserpref_ShaderCompileMethod {
  USER_SHADER_COMPILE_THREAD = 0,
  USER_SHADER_COMPILE_SUBPROCESS = 1,
};

/* Locale Ids. Auto will try to get local from OS. Our default is English though. */
/** #UserDef.language */
enum {
  ULANGUAGE_AUTO = 0,
  ULANGUAGE_ENGLISH = 1,
};

struct bAddon {
  struct bAddon *next = nullptr, *prev = nullptr;
  /**
   * 64 characters for a package prefix, 63 characters for the add-on name.
   */
  char module[128] = "";
  /** User-Defined Properties on this add-on (for storing preferences). */
  struct IDProperty *prop = nullptr;
};

/** #bPathCompare.flag */
enum ePathCompare_Flag {
  USER_PATHCMP_GLOB = (1 << 0),
};

struct bPathCompare {
  struct bPathCompare *next = nullptr, *prev = nullptr;
  char path[/*FILE_MAXDIR*/ 768] = "";
  char flag = 0;
  char _pad0[7] = {};
};

enum {
  USER_MENU_TYPE_SEP = 1,
  USER_MENU_TYPE_OPERATOR = 2,
  USER_MENU_TYPE_MENU = 3,
  USER_MENU_TYPE_PROP = 4,
};

struct bUserMenu {
  struct bUserMenu *next = nullptr, *prev = nullptr;
  char space_type = 0;
  char _pad0[7] = {};
  char context[64] = "";
  ListBaseT<bUserMenuItem> items = {nullptr, nullptr};
};

/** May be part of #bUserMenu or other list. */
struct bUserMenuItem {
  struct bUserMenuItem *next = nullptr, *prev = nullptr;
  char ui_name[64] = "";
  char type = 0;
  char _pad0[7] = {};
};

struct bUserMenuItem_Op {
  bUserMenuItem item;
  char op_idname[64] = "";
  struct IDProperty *prop = nullptr;
  char op_prop_enum[64] = "";
  char opcontext = 0; /* #wm::OpCallContext */
  char _pad0[7] = {};
};

struct bUserMenuItem_Menu {
  bUserMenuItem item;
  char mt_idname[64] = "";
};

struct bUserMenuItem_Prop {
  bUserMenuItem item;
  char context_data_path[256] = "";
  char prop_id[64] = "";
  int prop_index = 0;
  char _pad0[4] = {};
};

struct bUserAssetLibrary {
  struct bUserAssetLibrary *next = nullptr, *prev = nullptr;

  char name[/*MAX_NAME*/ 64] = "";
  char dirpath[/*FILE_MAX*/ 1024] = "";

  short import_method = ASSET_IMPORT_PACK;  /* eAssetImportMethod */
  short flag = ASSET_LIBRARY_RELATIVE_PATH; /* eAssetLibrary_Flag */
  char _pad0[4] = {};
};

enum eUserExtensionRepo_Flag {
  /** Maintain disk cache. */
  USER_EXTENSION_REPO_FLAG_NO_CACHE = 1 << 0,
  USER_EXTENSION_REPO_FLAG_DISABLED = 1 << 1,
  USER_EXTENSION_REPO_FLAG_USE_CUSTOM_DIRECTORY = 1 << 2,
  USER_EXTENSION_REPO_FLAG_USE_REMOTE_URL = 1 << 3,
  USER_EXTENSION_REPO_FLAG_SYNC_ON_STARTUP = 1 << 4,
  USER_EXTENSION_REPO_FLAG_USE_ACCESS_TOKEN = 1 << 5,
};

/**
 * The source to use (User or System), only valid when the
 * #USER_EXTENSION_REPO_FLAG_USE_REMOTE_URL flag isn't set.
 */
enum eUserExtensionRepo_Source {
  USER_EXTENSION_REPO_SOURCE_USER = 0,
  USER_EXTENSION_REPO_SOURCE_SYSTEM = 1,
};

struct bUserExtensionRepo {
  struct bUserExtensionRepo *next = nullptr, *prev = nullptr;
  /**
   * Unique identifier, only for display in the UI list.
   * The `module` is used for internal identifiers.
   */
  char name[/*MAX_NAME*/ 64] = "";
  /**
   * The unique module name (sub-module) in fact.
   *
   * Use a shorter name than #NAME_MAX to leave room for a base module prefix.
   * e.g. `bl_ext.{submodule}.{add_on}` to allow this string to fit into #bAddon::module.
   */
  char module[/*MAX_NAME - 16*/ 48] = "";

  /**
   * Secret access token for remote repositories (allocated).
   * Only use when #USER_EXTENSION_REPO_FLAG_USE_ACCESS_TOKEN is set.
   */
  char *access_token = nullptr;

  /**
   * The "local" directory where extensions are stored.
   * When unset, use `{BLENDER_USER_EXTENSIONS}/{bUserExtensionRepo::module}`.
   */
  char custom_dirpath[/*FILE_MAX*/ 1024] = "";
  char remote_url[/*FILE_MAX*/ 1024] = "";

  /** Options for the repository (#eUserExtensionRepo_Flag). */
  uint8_t flag = 0;
  /** The source location when the custom directory isn't used (#eUserExtensionRepo_Source). */
  uint8_t source = 0;

  char _pad0[6] = {};
};

struct SolidLight {
  int flag = 0;
  float smooth = 0;
  float col[4] = {0.8f, 0.8f, 0.8f};
  float spec[4] = {0.8f, 0.8f, 0.8f};
  float vec[4] = {0.0f, 0.0f, 1.0f};
};

struct WalkNavigation {
  /** Speed factor for look around. */
  float mouse_speed = 1;
  float walk_speed = 2.5;
  float walk_speed_factor = 5;
  float view_height = 1.6;
  float jump_height = 0.4;
  /** Duration to use for teleporting. */
  float teleport_time = 0.2;
  short flag = 0;
  char _pad0[6] = {};
};

struct XrNavigation {
  float vignette_intensity = 60;
  float turn_speed = DEG2RAD(60);
  float turn_amount = DEG2RAD(30);
  short flag = USER_XR_NAV_SNAP_TURN;
  char _pad0[2] = {};
};

struct UserDef_Runtime {
  /** Mark as changed so the preferences are saved on exit. */
  char is_dirty = 0;
  char _pad0[7] = {};
};

/* Toggles for unfinished 2.8 UserPref design. */
// #define WITH_USERDEF_WORKSPACES

/** #UserDef_SpaceData.section_active (UI active_section) */
enum eUserPref_Section {
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
  USER_SECTION_DEVELOPER_TOOLS = 18,
};

/** #UserDef_SpaceData.flag (State of the user preferences UI). */
enum eUserPref_SpaceData_Flag {
  /** Hide/expand key-map preferences. */
  USER_SPACEDATA_INPUT_HIDE_UI_KEYCONFIG = (1 << 0),
  USER_SPACEDATA_ADDONS_SHOW_ONLY_ENABLED = (1 << 1),
};

/**
 * Store UI data here instead of the space
 * since the space is typically a window which is freed.
 */
struct UserDef_SpaceData {
  char section_active = USER_SECTION_INTERFACE;
  /** #eUserPref_SpaceData_Flag UI options. */
  char flag = 0;
  char _pad0[6] = {};
};

/**
 * Storage for UI data that to keep it even after the window was closed. (Similar to
 * #UserDef_SpaceData.)
 */
struct UserDef_FileSpaceData {
  int display_type = FILE_VERTICALDISPLAY; /* FileSelectParams.display */
  int thumbnail_size = 96;                 /* FileSelectParams.thumbnail_size */
  int sort_type = FILE_SORT_ALPHA;         /* FileSelectParams.sort */
  int details_flags = FILE_DETAILS_SIZE |
                      FILE_DETAILS_DATETIME; /* FileSelectParams.details_flags */
  int flag = FILE_HIDE_DOT;                  /* FileSelectParams.flag */
  int _pad0 = {};
  uint64_t filter_id = FILTER_ID_ALL; /* FileSelectParams.filter_id */
};

struct UserDef_TempWinBounds {
  rctf file = {100.0f, 1160.0f, 350.0f, 950.0f};
  rctf userpref = {100.0f, 940.0f, 350.0f, 900.0f};
  rctf image = {50.0f, 1360.0f, 50.0f, 830.0f};
  rctf graph = {50.0f, 950.0f, 200.0f, 780.0f};
  rctf info = {100.0f, 1000.0f, 300.0f, 880.0f};
  rctf outliner = {100.0f, 550.0f, 350.0f, 800.0f};
};

/**
 * Checking experimental members must use either the #USER_EXPERIMENTAL_TEST() macro
 * or the #USER_DEVELOPER_TOOL_TEST() macro.
 */
struct UserDef_Experimental {
  /* Debug options, always available. */
  char use_undo_legacy = 0;
  char no_override_auto_resync = 0;
  char use_cycles_debug = 0;
  char use_eevee_debug = 0;
  char show_asset_debug_info = 0;
  char no_asset_indexing = 0;
  char use_viewport_debug = 0;
  char use_all_linked_data_direct = 0;
  char use_extensions_debug = 0;
  char use_recompute_usercount_on_save_debug = 0;
  char write_legacy_blend_file_format = 0;
  char no_data_block_packing = 0;
  char use_paint_debug = 0;
  char SANITIZE_AFTER_HERE = {};
  /* The following options are automatically sanitized (set to 0)
   * when the release cycle is not alpha. */
  char use_new_curves_tools = 0;
  char use_extended_asset_browser = 0;
  char use_sculpt_texture_paint = 0;
  char use_shader_node_previews = 0;
  char use_geometry_nodes_lists = 0;
  char _pad[5] = {};
};

#define USER_EXPERIMENTAL_TEST(userdef, member) (((userdef)->experimental).member)

#define USER_DEVELOPER_TOOL_TEST(userdef, member) \
  (((userdef)->flag & USER_DEVELOPER_UI) && ((userdef)->experimental).member)

/**
 * Container to store multiple directory paths and a name for each as a #ListBase.
 */
struct bUserScriptDirectory {
  struct bUserScriptDirectory *next = nullptr, *prev = nullptr;

  /** Name must be unique. */
  char name[/*MAX_NAME*/ 64] = "";
  char dir_path[/*FILE_MAXDIR*/ 768] = "";
};

/**
 * Settings for an asset shelf, stored in the Preferences. Most settings are still stored in the
 * asset shelf instance in #AssetShelfSettings. This is just for the options that should be shared
 * as Preferences.
 */
struct bUserAssetShelfSettings {
  struct bUserAssetShelfSettings *next = nullptr, *prev = nullptr;

  /** Identifier that matches the #AssetShelfType.idname of the shelf these settings apply to. */
  char shelf_idname[/*MAX_NAME*/ 64] = "";

  ListBaseT<AssetCatalogPathLink> enabled_catalog_paths = {nullptr, nullptr};
};

/**
 * Main user preferences data, typically accessed from #U.
 * See: #BKE_blendfile_userdef_from_defaults & #BKE_blendfile_userdef_read.
 *
 * \note This is either loaded from the file #BLENDER_USERPREF_FILE or from memory.
 */
struct UserDef {
  DNA_DEFINE_CXX_METHODS(UserDef)

  /** UserDef has separate do-version handling, and can be read from other files. */
  int versionfile = 0, subversionfile = 0;

  /** #eUserPref_Flag. */
  int flag = (USER_AUTOSAVE | USER_TOOLTIPS | USER_RELPATHS | USER_RELEASECONFIRM |
              USER_SCRIPT_AUTOEXEC_DISABLE | USER_NONEGFRAMES | USER_FILECOMPRESS |
              USER_HIDE_DOT_DATABLOCK);
  /** #eDupli_ID_Flags. */
  unsigned int dupflag = USER_DUP_MESH | USER_DUP_CURVE | USER_DUP_SURF | USER_DUP_LATTICE |
                         USER_DUP_FONT | USER_DUP_MBALL | USER_DUP_LAMP | USER_DUP_ARM |
                         USER_DUP_CAMERA | USER_DUP_SPEAKER | USER_DUP_ACT | USER_DUP_LIGHTPROBE |
                         USER_DUP_GPENCIL | USER_DUP_CURVES | USER_DUP_POINTCLOUD;
  /** #eUserPref_PrefFlag preferences for the preferences. */
  char pref_flag = USER_PREF_FLAG_SAVE;
  char savetime = 2;
  char mouse_emulate_3_button_modifier = 0;
  /**
   * Workaround for WAYLAND (at time of writing compositors don't support this info).
   * #eUserpref_TrackpadScrollDir type
   * TODO: Remove this once this API is better supported by Wayland compositors, see #107676.
   */
  char trackpad_scroll_direction = 0;
  /**  length. */
  char tempdir[/*FILE_MAXDIR*/ 768] = "";
  char fontdir[/*FILE_MAXDIR*/ 768] = "//";
  char renderdir[/*FILE_MAX*/ 1024] = "//";
  /* EXR cache path */
  char render_cachedir[/*FILE_MAXDIR*/ 768] = "";
  char textudir[/*FILE_MAXDIR*/ 768] = "//";
  /* Deprecated, use #UserDef.script_directories instead. */
  DNA_DEPRECATED char pythondir_legacy[/*FILE_MAXDIR*/ 768] = "";
  char sounddir[/*FILE_MAXDIR*/ 768] = "//";
  char i18ndir[/*FILE_MAXDIR*/ 768] = "";
  char image_editor[/*FILE_MAX*/ 1024] = "";
  char text_editor[/*FILE_MAX*/ 1024] = "";
  char text_editor_args[256] = "";
  char anim_player[/*FILE_MAX*/ 1024] = "";
  int anim_player_preset = 0;

  /** Minimum spacing between grid-lines in View2D grids. */
  short v2d_min_gridsize = 45;
  /** #eTimecodeStyles, style of time-code display. */
  short timecode_style = USER_TIMECODE_MINIMAL;

  short versions = 1;
  short dbl_click_time = 350;

  char _pad0[2] = {};

  /** Space around each area. Inter-editor gap width. */
  char border_width = 2;

  char mini_axis_type = USER_MINI_AXIS_TYPE_GIZMO;
  /** #eUserpref_UI_Flag. */
  int uiflag = USER_FILTERFILEEXTS | USER_DRAWVIEWINFO | USER_PLAINMENUS |
               USER_LOCK_CURSOR_ADJUST | USER_DEPTH_CURSOR | USER_AUTOPERSP |
               USER_NODE_AUTO_OFFSET | USER_GLOBALUNDO | USER_SHOW_GIZMO_NAVIGATE |
               USER_SHOW_VIEWPORTNAME | USER_SHOW_FPS | USER_CONTINUOUS_MOUSE | USER_SAVE_PROMPT;
  /** #eUserpref_UI_Flag2. */
  char uiflag2 = USER_REGION_OVERLAP;
  char gpu_flag = USER_GPU_FLAG_OVERLAY_SMOOTH_WIRE | USER_GPU_FLAG_SUBDIVISION_EVALUATION;
  char _pad8[6] = {};
  /* Experimental flag for app-templates to make changes to behavior
   * which are outside the scope of typical preferences. */
  char app_flag = 0;
  char viewzoom = USER_ZOOM_DOLLY;
  /** Default language of English (1), not Automatic (0). */
  short language = 1;

  int mixbufsize = 2048;
  int audiodevice = 0;
  int audiorate = 48000;
  int audioformat = 0x24;
  int audiochannels = 2;

  /** Setting for UI scale (fractional), before screen DPI has been applied. */
  float ui_scale = 1.0;
  /**
   * Setting for UI line width.
   *
   * In most cases this should not be used directly it is an offset used to calculate `pixelsize`
   * which should be used to define the line width.
   */
  int ui_line_width = 0;
  /** Runtime, full DPI divided by `pixelsize`. */
  int dpi = 0;
  /** Runtime multiplier to scale UI elements. Use macro UI_SCALE_FAC instead of this. */
  float scale_factor = 0;
  /** Runtime, `1.0 / scale_factor` */
  float inv_scale_factor = 0;
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
  float pixelsize = 1;
  /** Deprecated, for forward compatibility. */
  int virtual_pixel = 0;

  /** Console scroll-back limit. */
  int scrollback = 256;
  /** Node insert offset (aka auto-offset) margin, but might be useful for later stuff as well. */
  char node_margin = 40;
  char node_preview_res = 120;
  /** #eUserpref_Translation_Flags. */
  short transopts = USER_TR_TOOLTIPS | USER_TR_IFACE | USER_TR_REPORTS | USER_TR_NEWDATANAME;
  short menuthreshold1 = 5, menuthreshold2 = 2;

  /** Startup application template. */
  char app_template[64] = "";

  /**
   * A list of themes (#bTheme), the first is only used currently.
   * But there may be multiple themes in the list.
   */
  ListBaseT<bTheme> themes = {nullptr, nullptr};
  ListBaseT<uiFont> uifonts = {nullptr, nullptr};
  ListBaseT<uiStyle> uistyles = {nullptr, nullptr};
  ListBaseT<struct wmKeyMap> user_keymaps = {nullptr, nullptr};
  ListBaseT<struct wmKeyConfigPref> user_keyconfig_prefs = {nullptr, nullptr};
  ListBaseT<bAddon> addons = {nullptr, nullptr};
  ListBaseT<bPathCompare> autoexec_paths = {nullptr, nullptr};
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
  ListBaseT<bUserScriptDirectory> script_directories = {nullptr, nullptr};
  ListBaseT<bUserMenu> user_menus = {nullptr, nullptr};
  ListBaseT<bUserAssetLibrary> asset_libraries = {nullptr, nullptr};
  ListBaseT<bUserExtensionRepo> extension_repos = {nullptr, nullptr};
  ListBaseT<bUserAssetShelfSettings> asset_shelves_settings = {nullptr, nullptr};

  char keyconfigstr[64] = "Blender";

  /** Index of the asset library being edited in the Preferences UI. */
  short active_asset_library = 0;

  /** Index of the extension repo in the Preferences UI. */
  short active_extension_repo = 0;
  /** Flag for all extensions (#eUserPref_ExtensionFlag). */
  char extension_flag = 0;

  /* Network settings, used by extensions but not specific to extensions. */

  /** Time in seconds to wait before timing out online operation (0 uses the systems default). */
  uint8_t network_timeout = 10;
  /** Maximum number of simulations connection limit for online operations. */
  uint8_t network_connection_limit = 5;

  char _pad14[3] = {};

  short undosteps = 32;
  int undomemory = 0;
  DNA_DEPRECATED float gpu_viewport_quality = 0;
  short gp_manhattandist = 1, gp_euclideandist = 2, gp_eraser = 25;
  /** #eGP_UserdefSettings. */
  short gp_settings = 0;
  char _pad13[4] = {};
  struct SolidLight light_param[4];
  float light_ambient[3] = {};
  char gizmo_flag = USER_GIZMO_DRAW;
  /** Generic gizmo size. */
  char gizmo_size = 75;
  /** Navigate gizmo size. */
  char gizmo_size_navigate_v3d = 80;
  char _pad3[5] = {};
  short edit_studio_light = 0;
  short lookdev_sphere_size = 150;
  short vbotimeout = 120, vbocollectrate = 60;
  short textimeout = 120, texcollectrate = 60;
  int memcachelimit = 4096;
  /** Unused. */
  int prefetchframes = 0;
  /** Control the rotation step of the view when PAD2, PAD4, PAD6&PAD8 is use. */
  float pad_rot_angle = 15;
  char _pad12[4] = {};
  /** Rotating view icon size. */
  short rvisize = 25;
  /** Rotating view icon brightness. */
  short rvibright = 8;
  /** Maximum number of recently used files to remember. */
  short recent_files = 200;
  /** Milliseconds to spend spinning the view. */
  short smooth_viewtx = 200;
  short glreslimit = 0;
  /** #eColorPicker_Types. */
  short color_picker_type = USER_CP_CIRCLE_HSV;
  /** Curve smoothing type for newly added F-Curves. */
  char auto_smoothing_new = FCURVE_SMOOTH_CONT_ACCEL;
  /** Interpolation mode for newly added F-Curves. */
  char ipo_new = BEZT_IPO_BEZ;
  /** Handle types for newly added keyframes. */
  char keyhandles_new = HD_AUTO_ANIM;
  char _pad11[4] = {};
  /** #eZoomFrame_Mode. */
  char view_frame_type = ZOOM_FRAME_MODE_KEEP_RANGE;

  /** Number of keyframes to zoom around current frame. */
  int view_frame_keyframes = 0;
  /** Seconds to zoom around current frame. */
  float view_frame_seconds = 0;

  /** Preferred device/vendor for GPU device selection. */
  int gpu_preferred_index = 0;
  uint32_t gpu_preferred_vendor_id = 0;
  uint32_t gpu_preferred_device_id = 0;

  /** Max number of parallel shader compilation workers. */
  short gpu_shader_workers = 0;
  /** eUserpref_ShaderCompileMethod (OpenGL only). */
  short shader_compilation_method = USER_SHADER_COMPILE_THREAD;

  char _pad16[2] = {};

  /** #GPUBackendType */
  short gpu_backend = USER_GPU_BACKEND_DEFAULT;

  /** Number of samples for FPS display calculations. */
  short playback_fps_samples = 8;

  /** Private, defaults to 20 for 72 DPI setting. */
  short widget_unit = 0;
  short anisotropic_filter = 2;

  /** Tablet API to use (Windows only). */
  short tablet_api = USER_TABLET_AUTOMATIC;

  /** Raw tablet pressure that maps to 100%. */
  float pressure_threshold_max = 1.0;
  /** Curve non-linearity parameter. */
  float pressure_softness = 0;
  /** #eUserPref_Tablet_Flags */
  int tablet_flag = 0;

  /** 3D mouse: overall translation sensitivity. */
  float ndof_translation_sensitivity = 4.0;
  /** 3D mouse: overall rotation sensitivity. */
  float ndof_rotation_sensitivity = 4.0;
  /** 3D mouse: dead-zone. */
  float ndof_deadzone = 0;
  /** #eNdof_Flag, flags for 3D mouse. */
  int ndof_flag = NDOF_SHOW_GUIDE_ORBIT_CENTER | NDOF_ORBIT_CENTER_AUTO | NDOF_LOCK_HORIZON |
                  NDOF_SHOULD_PAN | NDOF_SHOULD_ZOOM | NDOF_SHOULD_ROTATE | NDOF_CAMERA_PAN_ZOOM;
  /** #eNdof_Navigation_Mode, current navigation mode. */
  uint8_t ndof_navigation_mode = 0;
  char _pad17[1] = {};

  /** eImageDrawMethod, Method to be used to draw the images
   * (AUTO, GLSL, Textures or DrawPixels) */
  short image_draw_method = IMAGE_DRAW_METHOD_AUTO;

  float glalphaclip = 0.004;

  /** #eAutokey_Mode, auto-keying mode. */
  short autokey_mode = (AUTOKEY_MODE_NORMAL & ~AUTOKEY_ON);
  /** Flags for inserting keyframes. */
  short keying_flag = KEYING_FLAG_XYZ2RGB | AUTOKEY_FLAG_INSERTNEEDED;
  /** Flags for which channels to insert keys at. */
  short key_insert_channels = USER_ANIM_KEY_CHANNEL_LOCATION | USER_ANIM_KEY_CHANNEL_ROTATION |
                              USER_ANIM_KEY_CHANNEL_SCALE |
                              USER_ANIM_KEY_CHANNEL_CUSTOM_PROPERTIES;  // eKeyInsertChannels
  char _pad15[2] = {};
  /** Flags for animation. */
  short animation_flag = USER_ANIM_HIGH_QUALITY_DRAWING;

  /** Options for text rendering. */
  char text_render = 0;
  char navigation_mode = VIEW_NAVIGATION_WALK;

  /** Turn-table rotation amount per-pixel in radians. Scaled with DPI. */
  float view_rotate_sensitivity_turntable = DEG2RAD(0.4);
  /** Track-ball rotation scale. */
  float view_rotate_sensitivity_trackball = 1.0f;

  /** From texture.h. */
  struct ColorBand coba_weight;

  float sculpt_paint_overlay_col[3] = {0, 0, 0};
  /** Default color for newly created Grease Pencil layers. */
  float gpencil_new_layer_col[4] = {0.38, 0.61, 0.78, 0.9};

  /** Drag pixels (scaled by DPI). */
  char drag_threshold_mouse = 3;
  char drag_threshold_tablet = 10;
  char drag_threshold = 30;
  char move_threshold = 2;

  char font_path_ui[/*FILE_MAX*/ 1024] = "";
  char font_path_ui_mono[/*FILE_MAX*/ 1024] = "";

  /** Legacy, for backwards compatibility only. */
  int compute_device_type = 0;

  /** Opacity of inactive F-Curves in F-Curve Editor. */
  float fcu_inactive_alpha = 0.25;

  /**
   * If keeping a pie menu spawn button pressed after this time,
   * it turns into a drag/release pie menu.
   */
  short pie_tap_timeout = 20;
  /**
   * Direction in the pie menu will always be calculated from the
   * initial position within this time limit.
   */
  short pie_initial_timeout = 0;
  short pie_animation_timeout = 6;
  short pie_menu_confirm = 0;
  /** Pie menu radius. */
  short pie_menu_radius = 100;
  /** Pie menu distance from center before a direction is set. */
  short pie_menu_threshold = 12;

  int sequencer_editor_flag = USER_SEQ_ED_CONNECT_STRIPS_BY_DEFAULT; /* eUserpref_SeqEditorFlags */

  char factor_display_type = USER_FACTOR_AS_FACTOR;

  char viewport_aa = 8;

  char render_display_type = USER_RENDER_DISPLAY_WINDOW; /* eUserpref_RenderDisplayType */
  char filebrowser_display_type =
      USER_TEMP_SPACE_DISPLAY_WINDOW; /* eUserpref_TempSpaceDisplayType */
  char preferences_display_type =
      USER_TEMP_SPACE_DISPLAY_WINDOW; /* eUserpref_TempSpaceDisplayType */
  char _pad18[7] = {};

  short sequencer_proxy_setup = USER_SEQ_PROXY_SETUP_AUTOMATIC; /* eUserpref_SeqProxySetup */
  short _pad1 = {};

  float collection_instance_empty_size = 1.0f;
  char text_flag = 0;
  char _pad10[1] = {};

  char file_preview_type = USER_FILE_PREVIEW_AUTO; /* eUserpref_File_Preview_Type */
  char statusbar_flag = STATUSBAR_SHOW_VERSION |
                        STATUSBAR_SHOW_EXTENSIONS_UPDATES; /* eUserpref_StatusBar_Flag */

  struct WalkNavigation walk_navigation;
  struct XrNavigation xr_navigation;

  /** The UI for the user preferences. */
  UserDef_SpaceData space_data;
  UserDef_FileSpaceData file_space_data;

  UserDef_TempWinBounds stored_bounds;

  UserDef_Experimental experimental;

  /** Runtime data (keep last). */
  UserDef_Runtime runtime;
};

/** From `source/blender/blenkernel/intern/blender.cc`. */
extern UserDef U;

}  // namespace blender

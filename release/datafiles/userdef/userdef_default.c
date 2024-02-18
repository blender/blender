/* SPDX-FileCopyrightText: 2019-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Preferences Data File 'U_default'. */

/* For constants. */
#include "BLI_math_base.h"

#include "DNA_anim_types.h"
#include "DNA_curve_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "BLI_math_rotation.h"

#include "BKE_blender_version.h"

#include "GPU_platform.h"

#include "BLO_userdef_default.h" /* own include */

const UserDef U_default = {
    .versionfile = BLENDER_FILE_VERSION,
    .subversionfile = BLENDER_FILE_SUBVERSION,
    .flag = (USER_AUTOSAVE | USER_TOOLTIPS | USER_RELPATHS | USER_RELEASECONFIRM |
             USER_SCRIPT_AUTOEXEC_DISABLE | USER_NONEGFRAMES),
    .dupflag = USER_DUP_MESH | USER_DUP_CURVE | USER_DUP_SURF | USER_DUP_LATTICE | USER_DUP_FONT |
               USER_DUP_MBALL | USER_DUP_LAMP | USER_DUP_ARM | USER_DUP_CAMERA | USER_DUP_SPEAKER |
               USER_DUP_ACT | USER_DUP_LIGHTPROBE | USER_DUP_GPENCIL | USER_DUP_CURVES |
               USER_DUP_POINTCLOUD,
    .pref_flag = USER_PREF_FLAG_SAVE,
    .savetime = 2,
    .tempdir = "",
    /* Overwritten by #BKE_appdir_font_folder_default(..)
     * unless the system font's cannot be found. */
    .fontdir = "//",
    .renderdir = "//",
    .render_cachedir = "",
    .textudir = "//",
    .script_directories = {NULL, NULL},
    .sounddir = "//",
    .i18ndir = "",
    .image_editor = "",
    .text_editor = "",
    .text_editor_args = "",
    .anim_player = "",
    .anim_player_preset = 0,
    .v2d_min_gridsize = 45,
    .timecode_style = USER_TIMECODE_MINIMAL,
    .versions = 1,
    .dbl_click_time = 350,
    .mini_axis_type = USER_MINI_AXIS_TYPE_GIZMO,
    .uiflag = (USER_FILTERFILEEXTS | USER_DRAWVIEWINFO | USER_PLAINMENUS |
               USER_LOCK_CURSOR_ADJUST | USER_DEPTH_CURSOR | USER_AUTOPERSP |
               USER_NODE_AUTO_OFFSET | USER_GLOBALUNDO | USER_HIDE_DOT | USER_SHOW_GIZMO_NAVIGATE |
               USER_SHOW_VIEWPORTNAME | USER_SHOW_FPS | USER_CONTINUOUS_MOUSE | USER_SAVE_PROMPT),
    .uiflag2 = USER_REGION_OVERLAP,
    .gpu_flag = USER_GPU_FLAG_OVERLAY_SMOOTH_WIRE | USER_GPU_FLAG_SUBDIVISION_EVALUATION,
    .app_flag = 0,
    /** Default language of English (1), not Automatic (0). */
    .language = 1,
    .viewzoom = USER_ZOOM_DOLLY,
    .mixbufsize = 2048,
    .audiodevice = 0,
    .audiorate = 48000,
    .audioformat = 0x24,
    .audiochannels = 2,

    .ui_scale = 1.0,
    .ui_line_width = 0,

    /** Default so DPI is detected automatically. */
    .dpi = 0,
    .scale_factor = 0.0,
    .inv_scale_factor = 0.0, /* run-time. */
    .pixelsize = 1,
    .virtual_pixel = 0,

    .scrollback = 256,
    .node_margin = 80,
    .node_preview_res = 120,
    .transopts = USER_TR_TOOLTIPS,
    .menuthreshold1 = 5,
    .menuthreshold2 = 2,
    .app_template = "",

    /** Initialized by #UI_theme_init_default. */
    .themes = {NULL},

    /** Initialized by #uiStyleInit. */
    .uifonts = {NULL},
    .uistyles = {NULL},

    .user_keymaps = {NULL},
    .user_keyconfig_prefs = {NULL},

    /** Initialized by #BKE_blendfile_userdef_from_defaults. */
    .addons = {NULL},

    .autoexec_paths = {NULL},
    .user_menus = {NULL},

    .keyconfigstr = "Blender",
    .undosteps = 32,
    .undomemory = 0,
    .gp_manhattandist = 1,
    .gp_euclideandist = 2,
    .gp_eraser = 25,
    .gp_settings = 0,
    .playback_fps_samples = 8,
#ifdef __APPLE__
    .gpu_backend = GPU_BACKEND_METAL,
#else
    .gpu_backend = GPU_BACKEND_OPENGL,
#endif

    /** Initialized by: #BKE_studiolight_default. */
    .light_param = {{0}},
    .light_ambient = {0, 0, 0},

    .gizmo_flag = USER_GIZMO_DRAW,
    .gizmo_size = 75,
    .gizmo_size_navigate_v3d = 80,
    .edit_studio_light = 0,
    .lookdev_sphere_size = 150,
    .vbotimeout = 120,
    .vbocollectrate = 60,
    .textimeout = 120,
    .texcollectrate = 60,

    /** Clamped by half the systems memory. */
    .memcachelimit = 4096,

    .prefetchframes = 0,
    .pad_rot_angle = 15,
    .rvisize = 25,
    .rvibright = 8,
    .recent_files = 20,
    .smooth_viewtx = 200,
    .glreslimit = 0,
    .color_picker_type = USER_CP_CIRCLE_HSV,
    .auto_smoothing_new = FCURVE_SMOOTH_CONT_ACCEL,
    .ipo_new = BEZT_IPO_BEZ,
    .keyhandles_new = HD_AUTO_ANIM,
    .view_frame_type = ZOOM_FRAME_MODE_KEEP_RANGE,
    .view_frame_keyframes = 0,
    .view_frame_seconds = 0.0,
    .widget_unit = 0, /* run-time initialized. */
    .anisotropic_filter = 2,
    .tablet_api = USER_TABLET_AUTOMATIC,
    .pressure_threshold_max = 1.0,
    .pressure_softness = 0.0,
    .ndof_sensitivity = 4.0,
    .ndof_orbit_sensitivity = 4.0,
    .ndof_deadzone = 0.1,
    .ndof_flag = (NDOF_MODE_ORBIT | NDOF_LOCK_HORIZON | NDOF_SHOULD_PAN | NDOF_SHOULD_ZOOM |
                  NDOF_SHOULD_ROTATE |
                  /* Software from the driver authors follows this convention
                   * so invert this by default, see: #67579. */
                  NDOF_ROTX_INVERT_AXIS | NDOF_ROTY_INVERT_AXIS | NDOF_ROTZ_INVERT_AXIS |
                  NDOF_PANX_INVERT_AXIS | NDOF_PANY_INVERT_AXIS | NDOF_PANZ_INVERT_AXIS |
                  NDOF_ZOOM_INVERT | NDOF_CAMERA_PAN_ZOOM),
    .image_draw_method = IMAGE_DRAW_METHOD_AUTO,
    .glalphaclip = 0.004,
    .autokey_mode = (AUTOKEY_MODE_NORMAL & ~AUTOKEY_ON),
    .keying_flag = KEYING_FLAG_XYZ2RGB | AUTOKEY_FLAG_INSERTNEEDED,
    .key_insert_channels = (USER_ANIM_KEY_CHANNEL_LOCATION | USER_ANIM_KEY_CHANNEL_ROTATION |
                            USER_ANIM_KEY_CHANNEL_SCALE | USER_ANIM_KEY_CHANNEL_CUSTOM_PROPERTIES),
    .animation_flag = USER_ANIM_HIGH_QUALITY_DRAWING,
    .text_render = 0,
    .navigation_mode = VIEW_NAVIGATION_WALK,
    .view_rotate_sensitivity_turntable = DEG2RAD(0.4),
    .view_rotate_sensitivity_trackball = 1.0f,

    /** Initialized by #BKE_colorband_init. */
    .coba_weight = {0},

    .sculpt_paint_overlay_col = {0, 0, 0},
    .gpencil_new_layer_col = {0.38, 0.61, 0.78, 0.9},
    .drag_threshold_mouse = 3,
    .drag_threshold_tablet = 10,
    .drag_threshold = 30,
    .move_threshold = 2,
    .font_path_ui = "",
    .font_path_ui_mono = "",
    .compute_device_type = 0,
    .fcu_inactive_alpha = 0.25,
    .pie_tap_timeout = 20,
    .pie_initial_timeout = 0,
    .pie_animation_timeout = 6,
    .pie_menu_confirm = 0,
    .pie_menu_radius = 100,
    .pie_menu_threshold = 12,
    .factor_display_type = USER_FACTOR_AS_FACTOR,
    .render_display_type = USER_RENDER_DISPLAY_WINDOW,
    .filebrowser_display_type = USER_TEMP_SPACE_DISPLAY_WINDOW,
    .viewport_aa = 8,

    .walk_navigation =
        {
            .mouse_speed = 1,
            .walk_speed = 2.5,
            .walk_speed_factor = 5,
            .view_height = 1.6,
            .jump_height = 0.4,
            .teleport_time = 0.2,
            .flag = 0,
        },

    .space_data =
        {
            .section_active = USER_SECTION_INTERFACE,
        },

    .file_space_data =
        {
            .display_type = FILE_VERTICALDISPLAY,
            .thumbnail_size = 96,
            .sort_type = FILE_SORT_ALPHA,
            .details_flags = FILE_DETAILS_SIZE | FILE_DETAILS_DATETIME,
            .flag = FILE_HIDE_DOT,
            .filter_id = FILTER_ID_ALL,

            .temp_win_sizex = 1060,
            .temp_win_sizey = 600,
        },

    .sequencer_disk_cache_dir = "",
    .sequencer_disk_cache_compression = 0,
    .sequencer_disk_cache_size_limit = 100,
    .sequencer_disk_cache_flag = 0,
    .sequencer_proxy_setup = USER_SEQ_PROXY_SETUP_AUTOMATIC,

    .collection_instance_empty_size = 1.0f,

    .statusbar_flag = STATUSBAR_SHOW_VERSION,
    .file_preview_type = USER_FILE_PREVIEW_AUTO,

    .runtime =
        {
            .is_dirty = 0,
        },
};

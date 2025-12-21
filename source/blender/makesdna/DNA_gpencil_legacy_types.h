/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup DNA
 */

#pragma once

#include "DNA_ID.h"
#include "DNA_curve_types.h"
#include "DNA_listBase.h"

struct AnimData;
struct MDeformVert;

/** #bGPDspoint.flag */
enum eGPDspoint_Flag {
  /* stroke point is selected (for editing) */
  GP_SPOINT_SELECT = (1 << 0),

  /* stroke point is tagged (for some editing operation) */
  GP_SPOINT_TAG = (1 << 1),
};

/** #bGPDpalettecolor.flag */
enum eGPDpalettecolor_Flag {
  /* color is active */
  /* PC_COLOR_ACTIVE = (1 << 0), */ /* UNUSED */
  /* don't display color */
  PC_COLOR_HIDE = (1 << 1),
  /* protected from further editing */
  PC_COLOR_LOCKED = (1 << 2),
  /* do onion skinning */
  PC_COLOR_ONIONSKIN = (1 << 3),
  /* "volumetric" strokes */
  PC_COLOR_VOLUMETRIC = (1 << 4),
};

/** #bGPDpalette.flag */
enum eGPDpalette_Flag {
  /* palette is active */
  PL_PALETTE_ACTIVE = (1 << 0),
};

/* bGPDcurve_point->flag */
enum eGPDcurve_point_Flag {
  GP_CURVE_POINT_SELECT = (1 << 0),
};

/* bGPDcurve_Flag->flag */
enum bGPDcurve_Flag {
  /* Flag to indicated that the stroke data has been changed and the curve needs to be refitted */
  GP_CURVE_NEEDS_STROKE_UPDATE = (1 << 0),
  /* Curve is selected */
  GP_CURVE_SELECT = (1 << 1),
};

/** #bGPDstroke.flag */
enum eGPDstroke_Flag {
  /* stroke is in 3d-space */
  GP_STROKE_3DSPACE = (1 << 0),
  /* stroke is in 2d-space */
  GP_STROKE_2DSPACE = (1 << 1),
  /* stroke is in 2d-space (but with special 'image' scaling) */
  GP_STROKE_2DIMAGE = (1 << 2),
  /* stroke is selected */
  GP_STROKE_SELECT = (1 << 3),
  /* Flag used to indicate that stroke is closed and draw edge between last and first point */
  GP_STROKE_CYCLIC = (1 << 7),
  /* Flag used to indicate that stroke is used for fill close and must use
   * fill color for stroke and no fill area */
  GP_STROKE_NOFILL = (1 << 8),
  /* Flag to indicated that the editcurve has been changed and the stroke needs to be updated with
   * the curve data */
  GP_STROKE_NEEDS_CURVE_UPDATE = (1 << 9),
  /* Flag to indicate that a stroke is used only for help, and will not affect rendering or fill */
  GP_STROKE_HELP = (1 << 10),
  /* Flag to indicate that a extend stroke collide (fill tool). */
  GP_STROKE_COLLIDE = (1 << 11),
  /* only for use with stroke-buffer (while drawing arrows) */
  GP_STROKE_USE_ARROW_START = (1 << 12),
  /* only for use with stroke-buffer (while drawing arrows) */
  GP_STROKE_USE_ARROW_END = (1 << 13),
  /* Tag for update geometry */
  GP_STROKE_TAG = (1 << 14),
  /* only for use with stroke-buffer (while drawing eraser) */
  GP_STROKE_ERASER = (1 << 15),
};

/** #bGPDstroke.caps */
enum eGPDstroke_Caps {
  /* type of extreme */
  GP_STROKE_CAP_ROUND = 0,
  GP_STROKE_CAP_FLAT = 1,

  /* Keep last. */
  GP_STROKE_CAP_MAX,
};

/** #bGPDataRuntime.arrowstyle */
enum eGPDstroke_Arrowstyle {
  GP_STROKE_ARROWSTYLE_NONE = 0,
  GP_STROKE_ARROWSTYLE_SEGMENT = 2,
  GP_STROKE_ARROWSTYLE_OPEN = 3,
  GP_STROKE_ARROWSTYLE_CLOSED = 4,
  GP_STROKE_ARROWSTYLE_SQUARE = 6,
};

/* bGPDframe->flag */
enum eGPDframe_Flag {
  /* frame is being painted on */
  GP_FRAME_PAINT = (1 << 0),
  /* for editing in Action Editor */
  GP_FRAME_SELECT = (1 << 1),
  /* Line Art generation */
  GP_FRAME_LRT_CLEARED = (1 << 2),
};

/* bGPDlayer_Mask->flag */
enum ebGPDlayer_Mask_Flag {
  /* Mask is hidden. */
  GP_MASK_HIDE = (1 << 0),
  /* Mask is inverted. */
  GP_MASK_INVERT = (1 << 1),
};

/* bGPDlayer->flag */
enum eGPDlayer_Flag {
  /* don't display layer */
  GP_LAYER_HIDE = (1 << 0),
  /* protected from further editing */
  GP_LAYER_LOCKED = (1 << 1),
  /* layer is 'active' layer being edited */
  GP_LAYER_ACTIVE = (1 << 2),
  /* draw points of stroke for debugging purposes */
  GP_LAYER_DRAWDEBUG = (1 << 3),
  /* Flag used to display in Paint mode only layers with keyframe */
  GP_LAYER_SOLO_MODE = (1 << 4),
  /* for editing in Action Editor */
  GP_LAYER_SELECT = (1 << 5),
  /* current frame for layer can't be changed */
  GP_LAYER_FRAMELOCK = (1 << 6),
  /* Don't render X-ray (which is default). */
  GP_LAYER_NO_XRAY = (1 << 7),
  /* "volumetric" strokes */
  GP_LAYER_VOLUMETRIC = (1 << 10),
  /* Use Scene lights */
  GP_LAYER_USE_LIGHTS = (1 << 11),
  /* Unlock color */
  GP_LAYER_UNLOCK_COLOR = (1 << 12),
  /* Mask Layer */
  GP_LAYER_USE_MASK = (1 << 13), /* TODO: DEPRECATED */
  /* Ruler Layer */
  GP_LAYER_IS_RULER = (1 << 14),
  /* Disable masks in view-layer render */
  GP_LAYER_DISABLE_MASKS_IN_VIEWLAYER = (1 << 15),
};

/** #bGPDlayer.onion_flag */
enum eGPDlayer_OnionFlag {
  /* do onion skinning */
  GP_LAYER_ONIONSKIN = (1 << 0),
  GP_LAYER_ONIONSKIN_CUSTOM_COLOR = (1 << 1),
};

/** #bGPDlayer.blend_mode */
enum eGPLayerBlendModes {
  eGplBlendMode_Regular = 0,
  eGplBlendMode_HardLight = 1,
  eGplBlendMode_Add = 2,
  eGplBlendMode_Subtract = 3,
  eGplBlendMode_Multiply = 4,
  eGplBlendMode_Divide = 5,
};

/**
 * #bGPdata.flag
 *
 * NOTE: A few flags have been deprecated since early 2.5,
 *       since they have been made redundant by interaction
 *       changes made during the porting process.
 */
enum eGPdata_Flag {
  /* data-block is used for "annotations"
   * NOTE: This flag used to be used in 2.4x, but should hardly ever have been set.
   *       We can use this freely now, as all GP data-blocks from pre-2.8 will get
   *       set on file load (as many old use cases are for "annotations" only)
   */
  GP_DATA_ANNOTATIONS = (1 << 0),

  /* show debugging info in viewport (i.e. status print) */
  GP_DATA_DISPINFO = (1 << 1),
  /* in Action Editor, show as expanded channel */
  GP_DATA_EXPAND = (1 << 2),

  /* is the block overriding all clicks? */
  /* GP_DATA_EDITPAINT = (1 << 3), */

  /* ------------------------------------------------ DEPRECATED */
  /* new strokes are added in viewport space */
  GP_DATA_VIEWALIGN = (1 << 4),

  /* Project into the screen's Z values */
  GP_DATA_DEPTH_VIEW = (1 << 5),
  GP_DATA_DEPTH_STROKE = (1 << 6),

  GP_DATA_DEPTH_STROKE_ENDPOINTS = (1 << 7),
  /* ------------------------------------------------ DEPRECATED */

  /* Stroke Editing Mode - Toggle to enable alternative keymap
   * for easier editing of stroke points */
  GP_DATA_STROKE_EDITMODE = (1 << 8),

  /* Main flag to switch onion skinning on/off */
  GP_DATA_SHOW_ONIONSKINS = (1 << 9),

  /* Batch drawing cache need to be recalculated */
  GP_DATA_CACHE_IS_DIRTY = (1 << 11),

  /* Stroke Paint Mode - Toggle paint mode */
  GP_DATA_STROKE_PAINTMODE = (1 << 12),
  /* Stroke Editing Mode - Toggle sculpt mode */
  GP_DATA_STROKE_SCULPTMODE = (1 << 13),
  /* Stroke Editing Mode - Toggle weight paint mode */
  GP_DATA_STROKE_WEIGHTMODE = (1 << 14),

  /* keep stroke thickness unchanged when zoom change */
  GP_DATA_STROKE_KEEPTHICKNESS = (1 << 15),

  /* Allow edit several frames at the same time */
  GP_DATA_STROKE_MULTIEDIT = (1 << 16),

  /* Vertex Paint Mode - Toggle paint mode */
  GP_DATA_STROKE_VERTEXMODE = (1 << 18),

  /* Auto-lock not active layers. */
  GP_DATA_AUTOLOCK_LAYERS = (1 << 20),

  /* Enable Bezier Editing Curve (a sub-mode of Edit mode). */
  GP_DATA_CURVE_EDIT_MODE = (1 << 21),
  /* Use adaptive curve resolution */
  GP_DATA_CURVE_ADAPTIVE_RESOLUTION = (1 << 22),
};

/* gpd->onion_flag */
enum eGPD_OnionFlag {
  /* use custom color for ghosts before current frame */
  GP_ONION_GHOST_PREVCOL = (1 << 0),
  /* use custom color for ghosts after current frame */
  GP_ONION_GHOST_NEXTCOL = (1 << 1),
  /* always show onion skins (i.e. even during renders/animation playback) */
  GP_ONION_GHOST_ALWAYS = (1 << 2),
  /* use fade color in onion skin */
  GP_ONION_FADE = (1 << 3),
  /* Loop showing first frame after last frame */
  GP_ONION_LOOP = (1 << 4),
};

/* gpd->onion_mode */
enum eGP_OnionModes {
  GP_ONION_MODE_ABSOLUTE = 0,
  GP_ONION_MODE_RELATIVE = 1,
  GP_ONION_MODE_SELECTED = 2,
};

/* X-ray modes (Depth Ordering). */
enum eGP_DepthOrdering {
  GP_XRAY_FRONT = 0,
  GP_XRAY_3DSPACE = 1,
};

/* draw modes (Use 2D or 3D position) */
enum eGP_DrawMode {
  GP_DRAWMODE_2D = 0,
  GP_DRAWMODE_3D = 1,
};

#define GP_DEFAULT_PIX_FACTOR 1.0f
#define GP_DEFAULT_GRID_LINES 4
#define GP_MAX_INPUT_SAMPLES 10

#define GP_DEFAULT_CURVE_RESOLUTION 32
#define GP_DEFAULT_CURVE_ERROR 0.1f
#define GP_DEFAULT_CURVE_EDIT_CORNER_ANGLE M_PI_2

#define GPENCIL_MIN_FILL_FAC 0.05f
#define GPENCIL_MAX_FILL_FAC 8.0f

/**
 * Grease-Pencil Annotations - 'Stroke Point'
 * -> Coordinates may either be 2d or 3d depending on settings at the time
 * -> Coordinates of point on stroke, in proportions of window size
 *    This assumes that the bottom-left corner is (0,0)
 */
struct bGPDspoint {
  DNA_DEFINE_CXX_METHODS(bGPDspoint)

  /** Co-ordinates of point (usually 2d, but can be 3d as well). */
  float x = 0, y = 0, z = 0;
  /** Pressure of input device (from 0 to 1) at this point. */
  float pressure = 0;
  /** Color strength (used for alpha factor). */
  float strength = 0;
  /** Seconds since start of stroke. */
  float time = 0;
  /** Additional options. */
  int flag = 0;

  /** Factor of uv along the stroke. */
  float uv_fac = 0;
  /** UV rotation for dot mode. */
  float uv_rot = 0;
  /** UV for fill mode */
  float uv_fill[2] = {};

  /** Vertex Color RGBA (A=mix factor). */
  float vert_color[4] = {};

  /** Runtime data */
  char _pad2[4] = {};
};

/* ***************************************** */
/* GP Fill - Triangle Tessellation Data */

/* Grease-Pencil Annotations - 'Triangle'
 * -> A triangle contains the index of three vertices for filling the stroke
 *    This is only used if high quality fill is enabled
 */
struct bGPDtriangle {
  /* indices for tessellated triangle used for GP Fill */
  unsigned int verts[3] = {};
};

/* ***************************************** */

/* ***************************************** */
/* GP Palettes (Deprecated - 2.78 - 2.79 only) */

/* color of palettes */
struct bGPDpalettecolor {
  DNA_DEFINE_CXX_METHODS(bGPDpalettecolor)

  struct bGPDpalettecolor *next = nullptr, *prev = nullptr;
  /** Color name. Must be unique. */
  char info[64] = "";
  float color[4] = {};
  /** Color that should be used for drawing "fills" for strokes. */
  float fill[4] = {};
  /** Settings for palette color. */
  short flag = 0;
  /** Padding for compiler alignment error. */
  char _pad[6] = {};
};

/* palette of colors */
struct bGPDpalette {
  DNA_DEFINE_CXX_METHODS(bGPDpalette)

  struct bGPDpalette *next = nullptr, *prev = nullptr;

  /** Pointer to individual colors. */
  ListBaseT<struct PaletteColor> colors = {nullptr, nullptr};
  /** Palette name. Must be unique. */
  char info[64] = "";

  short flag = 0;
  char _pad[6] = {};
};

/* ***************************************** */
/* GP Curve Point */

struct bGPDcurve_point {
  /** Bezier Triple for the handles and control points. */
  BezTriple bezt = {};
  /** Pressure of input device (from 0 to 1) at this point. */
  float pressure = 0;
  /** Color strength (used for alpha factor). */
  float strength = 0;
  /** Index of corresponding point in gps->points. */
  int point_index = 0;

  /** Additional options. */
  int flag = 0;

  /** Factor of uv along the stroke. */
  float uv_fac = 0;
  /** UV rotation for dot mode. */
  float uv_rot = 0;
  /** UV for fill mode. */
  float uv_fill[2] = {};

  /** Vertex Color RGBA (A=mix factor). */
  float vert_color[4] = {};
  char _pad[4] = {};
};

/* ***************************************** */
/* GP Curve */

/* Curve for Bezier Editing. */
struct bGPDcurve {
  DNA_DEFINE_CXX_METHODS(bGPDcurve)

  /** Array of BezTriple. */
  bGPDcurve_point *curve_points = nullptr;
  /** Total number of curve points. */
  int tot_curve_points = 0;
  /** General flag. */
  short flag = 0;
  char _pad[2] = {};
};

/* ***************************************** */
/* GP Strokes */

/* Runtime temp data for bGPDstroke */
struct bGPDstroke_Runtime {
  DNA_DEFINE_CXX_METHODS(bGPDstroke_Runtime)

  /** temporary layer name only used during copy/paste to put the stroke in the original layer */
  char tmp_layerinfo[128] = "";

  /** Runtime falloff factor (only for transform). */
  float multi_frame_falloff = 0;

  /** Triangle offset in the IBO where this stroke starts. */
  int stroke_start = 0;
  /** Triangle offset in the IBO where this fill starts. */
  int fill_start = 0;
  /** Vertex offset in the VBO where this stroke starts. */
  int vertex_start = 0;
  /** Curve Handles offset in the IBO where this handle starts. */
  int curve_start = 0;
  int _pad0 = {};

  /** Original stroke (used to dereference evaluated data) */
  struct bGPDstroke *gps_orig = nullptr;
  void *_pad2 = nullptr;
};

/**
 * Grease-Pencil Annotations - 'Stroke'
 * -> A stroke represents a (simplified version) of the curve
 *    drawn by the user in one 'mouse-down'->'mouse-up' operation
 */
struct bGPDstroke {
  DNA_DEFINE_CXX_METHODS(bGPDstroke)

  struct bGPDstroke *next = nullptr, *prev = nullptr;

  /** Array of data-points for stroke. */
  bGPDspoint *points = nullptr;
  /** Tessellated triangles for GP Fill. */
  bGPDtriangle *triangles = nullptr;
  /** Number of data-points in array. */
  int totpoints = 0;
  /** Number of triangles in array. */
  int tot_triangles = 0;

  /** Thickness of stroke. */
  short thickness = 0;
  /** Various settings about this stroke. */
  short flag = 0, _pad[2] = {};

  /** Init time of stroke. */
  double inittime = 0;

  /** Color name. */
  DNA_DEPRECATED char colorname[128] = "";

  /** Material index. */
  int mat_nr = 0;
  /** Caps mode for each stroke extreme */
  short caps[2] = {};

  /** gradient control along y for color */
  float hardness = 0;
  /** factor xy of shape for dots gradients */
  float aspect_ratio[2] = {};

  /** Factor of opacity for Fill color (used by opacity modifier). */
  float fill_opacity_fac = 0;

  /** UV rotation */
  float uv_rotation = 0;
  /** UV translation (X and Y axis) */
  float uv_translation[2] = {};
  float uv_scale = 0;

  /** Stroke selection index. */
  int select_index = 0;
  char _pad4[4] = {};

  /** Vertex weight data. */
  struct MDeformVert *dvert = nullptr;
  void *_pad3 = nullptr;

  /** Vertex Color for Fill (one for all stroke, A=mix factor). */
  float vert_color_fill[4] = {};

  /** Curve used to edit the stroke using Bezier handlers. */
  struct bGPDcurve *editcurve = nullptr;

  bGPDstroke_Runtime runtime;
  void *_pad5 = nullptr;
};

/* Arrows ----------------------- */

/* ***************************************** */
/* GP Frame */

/* Runtime temp data for bGPDframe */
struct bGPDframe_Runtime {
  DNA_DEFINE_CXX_METHODS(bGPDframe_Runtime)

  /** Index of this frame in the listbase of frames. */
  int frameid = 0;
  /** Onion offset from active frame. 0 if not onion. INT_MAX to bypass frame. */
  int onion_id = 0;
};

/**
 * Grease-Pencil Annotations - 'Frame'
 * -> Acts as storage for the 'image' formed by strokes
 */
struct bGPDframe {
  DNA_DEFINE_CXX_METHODS(bGPDframe)

  struct bGPDframe *next = nullptr, *prev = nullptr;

  /** List of the simplified 'strokes' that make up the frame's data. */
  ListBaseT<bGPDstroke> strokes = {nullptr, nullptr};

  /** Frame number of this frame. */
  int framenum = 0;

  /** Temp settings. */
  short flag = 0;
  /** Keyframe type (eBezTriple_KeyframeType). */
  short key_type = 0;

  bGPDframe_Runtime runtime;
};

/* ***************************************** */
/* GP Layer */

/* List of masking layers. */
struct bGPDlayer_Mask {
  DNA_DEFINE_CXX_METHODS(bGPDlayer_Mask)

  struct bGPDlayer_Mask *next = nullptr, *prev = nullptr;
  char name[128] = "";
  short flag = 0;
  /** Index for sorting. Only valid while sorting algorithm is running. */
  short sort_index = 0;
  char _pad[4] = {};
};

/** Runtime temp data for #bGPDlayer. */
struct bGPDlayer_Runtime {
  DNA_DEFINE_CXX_METHODS(bGPDlayer_Runtime)

  /** Id for dynamic icon used to show annotation color preview for layer. */
  int icon_id = 0;
  char _pad[4] = {};
};

/** Grease-Pencil Annotations - 'Layer'. */
struct bGPDlayer {
  DNA_DEFINE_CXX_METHODS(bGPDlayer)

  struct bGPDlayer *next = nullptr, *prev = nullptr;

  /** List of annotations to display for frames (bGPDframe list). */
  ListBaseT<bGPDframe> frames = {nullptr, nullptr};
  /** Active frame (should be the frame that is currently being displayed). */
  bGPDframe *actframe = nullptr;

  /** Settings for layer. */
  short flag = 0;
  /** Per-layer onion-skinning flags (eGPDlayer_OnionFlag). */
  short onion_flag = 0;

  /** Color for strokes in layers. Used for annotations, and for ruler
   * (which uses GPencil internally). */
  float color[4] = {};
  /** Fill color for strokes in layers. Not used anymore (was only for). */
  float fill[4] = {};

  /** Name/reference info for this layer (i.e. "director's comments, 12/.3")
   * needs to be kept unique, as it's used as the layer identifier */
  char info[128] = "";

  /** Thickness to apply to strokes (Annotations). */
  short thickness = 0;
  /** Used to filter groups of layers in modifiers. */
  short pass_index = 0;

  /** Parent object. */
  struct Object *parent = nullptr;
  /** Inverse matrix (only used if parented). */
  float inverse[4][4] = {};
  /** String describing sub-object info. */
  char parsubstr[/*MAX_NAME*/ 64] = "";
  short partype = 0;

  /** Thickness adjustment. */
  short line_change = 0;
  /** Color used to tint layer, alpha value is used as factor. */
  float tintcolor[4] = {};
  /** Opacity of the layer. */
  float opacity = 0;
  /** Name of the layer used to filter render output. */
  char viewlayername[64] = "";

  /** Blend modes. */
  int blend_mode = 0;
  /** Vertex Paint opacity by Layer. */
  float vertex_paint_opacity = 0;

  /* annotation onion skin */
  /**
   * Ghosts Before: max number of ghost frames to show between
   * active frame and the one before it (0 = only the ghost itself).
   */
  short gstep = 0;
  /**
   * Ghosts After: max number of ghost frames to show after
   * active frame and the following it    (0 = only the ghost itself).
   */
  short gstep_next = 0;

  /** Color for ghosts before the active frame. */
  float gcolor_prev[3] = {0.302f, 0.851f, 0.302f};
  /** Color for ghosts after the active frame. */
  float gcolor_next[3] = {0.250f, 0.1f, 1.0f};
  char _pad1[4] = {};

  /** Mask list (bGPDlayer_Mask). */
  ListBaseT<bGPDlayer_Mask> mask_layers = {nullptr, nullptr};
  /** Current Mask index (noted base 1). */
  int act_mask = 0;
  char _pad2[4] = {};

  /** Layer transforms. */
  float location[3] = {}, rotation[3] = {}, scale[3] = {};
  float layer_mat[4][4] = {}, layer_invmat[4][4] = {};
  char _pad3[4] = {};

  bGPDlayer_Runtime runtime;
};

/* ***************************************** */
/* GP Datablock */

/* Runtime temp data for bGPdata */
struct bGPdata_Runtime {
  DNA_DEFINE_CXX_METHODS(bGPdata_Runtime)

  /** Stroke buffer. */
  void *sbuffer = nullptr;

  /** Animation playing flag. */
  short playing = 0;

  /** Material index of the stroke. */
  short matid = 0;

  /* Stroke Buffer data (only used during paint-session)
   * - buffer must be initialized before use, but freed after
   *   whole paint operation is over
   */
  /** Flags for stroke that cache represents. */
  short sbuffer_sflag = 0;
  char _pad1[2] = {};
  /** Number of elements currently used in cache. */
  int sbuffer_used = 0;
  /** Number of total elements available in cache. */
  int sbuffer_size = 0;

  /** Vertex Color applied to Fill (while drawing). */
  float vert_color_fill[4] = {};

  /** Opacity for fills while drawing. */
  float fill_opacity_fac = 0;

  /** Arrow points for stroke corners. */
  float arrow_start[8] = {};
  float arrow_end[8] = {};
  /* Arrow style for each corner */
  int arrow_start_style = 0;
  int arrow_end_style = 0;

  char _pad[4] = {};
};

/* grid configuration */
struct bGPgrid {
  float color[3] = {};
  float scale[2] = {};
  float offset[2] = {};
  char _pad1[4] = {};

  int lines = 0;
  char _pad[4] = {};
};

/** Grease-Pencil Annotations - 'DataBlock'. */
struct bGPdata {
#ifdef __cplusplus
  DNA_DEFINE_CXX_METHODS(bGPdata)
  /** See #ID_Type comment for why this is here. */
  static constexpr ID_Type id_type = ID_GD_LEGACY;
#endif

  /** Grease Pencil data is a data-block. */
  ID id;
  /** Animation data - for animating draw settings. */
  struct AnimData *adt = nullptr;

  /* Grease-Pencil data */
  /** bGPDlayer. */
  ListBaseT<bGPDlayer> layers = {nullptr, nullptr};
  /** Settings for this data-block. */
  int flag = 0;
  /** Default resolution for generated curves using curve editing method. */
  int curve_edit_resolution = 0;
  /** Curve Editing error threshold. */
  float curve_edit_threshold = 0;
  /** Curve Editing corner angle (less or equal is treated as corner). */
  float curve_edit_corner_angle = 0;

  /* Palettes */
  /** List of bGPDpalette's   - Deprecated (2.78 - 2.79 only). */
  ListBaseT<bGPDpalette> palettes = {nullptr, nullptr};

  /** List of bDeformGroup names and flag only. */
  ListBaseT<bDeformGroup> vertex_group_names = {nullptr, nullptr};

  /* 3D Viewport/Appearance Settings */
  /** Factor to define pixel size conversion. */
  float pixfactor = 0;
  /** Color for edit line. */
  float line_color[4] = {};

  /* Onion skinning */
  /** Onion alpha factor change. */
  float onion_factor = 0;
  /** Onion skinning range (eGP_OnionModes). */
  int onion_mode = 0;
  /** Onion skinning flags (eGPD_OnionFlag). */
  int onion_flag = 0;
  /**
   * Ghosts Before: max number of ghost frames to show between
   * active frame and the one before it (0 = only the ghost itself).
   */
  short gstep = 0;
  /**
   * Ghosts After: max number of ghost frames to show after
   * active frame and the following it (0 = only the ghost itself).
   */
  short gstep_next = 0;

  /** Optional color for ghosts before the active frame. */
  float gcolor_prev[3] = {};
  /** Optional color for ghosts after the active frame. */
  float gcolor_next[3] = {};

  /** Offset for drawing over surfaces to keep strokes on top. */
  float zdepth_offset = 0;
  /** Materials array. */
  struct Material **mat = nullptr;
  /** Total materials. */
  short totcol = 0;

  /* stats */
  short totlayer = 0;
  short totframe = 0;
  char _pad2[6] = {};
  int totstroke = 0;
  int totpoint = 0;

  /** Draw mode for strokes (eGP_DrawMode). */
  short draw_mode = 0;
  /** Keyframe type for onion filter  (eBezTriple_KeyframeType plus All option) */
  short onion_keytype = 0;

  /** Stroke selection last index. Used to generate a unique selection index. */
  int select_last_index = 0;

  int vertex_group_active_index = 0;

  bGPgrid grid;

  bGPdata_Runtime runtime;
};

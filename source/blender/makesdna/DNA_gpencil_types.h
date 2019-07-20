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
 *
 * The Original Code is Copyright (C) 2008, Blender Foundation.
 * This is a new part of Blender
 */

/** \file
 * \ingroup DNA
 */

#ifndef __DNA_GPENCIL_TYPES_H__
#define __DNA_GPENCIL_TYPES_H__

#include "DNA_listBase.h"
#include "DNA_ID.h"
#include "DNA_brush_types.h"

struct ARegion;
struct AnimData;
struct CurveMapping;
struct GHash;
struct MDeformVert;

#define GP_DEFAULT_PIX_FACTOR 1.0f
#define GP_DEFAULT_GRID_LINES 4
#define GP_MAX_INPUT_SAMPLES 10

/* ***************************************** */
/* GP Stroke Points */

/* 'Control Point' data for primitives and curves */
typedef struct bGPDcontrolpoint {
  /** X and y coordinates of control point. */
  float x, y, z;
  /** Point color. */
  float color[4];
  /** Radius. */
  int size;
} bGPDcontrolpoint;

/* Grease-Pencil Annotations - 'Stroke Point'
 * -> Coordinates may either be 2d or 3d depending on settings at the time
 * -> Coordinates of point on stroke, in proportions of window size
 *    This assumes that the bottom-left corner is (0,0)
 */
typedef struct bGPDspoint {
  /** Co-ordinates of point (usually 2d, but can be 3d as well). */
  float x, y, z;
  /** Pressure of input device (from 0 to 1) at this point. */
  float pressure;
  /** Color strength (used for alpha factor). */
  float strength;
  /** Seconds since start of stroke. */
  float time;
  /** Additional options. */
  int flag;

  /** Factor of uv along the stroke. */
  float uv_fac;
  /** Uv rotation for dot mode. */
  float uv_rot;
} bGPDspoint;

/* bGPDspoint->flag */
typedef enum eGPDspoint_Flag {
  /* stroke point is selected (for editing) */
  GP_SPOINT_SELECT = (1 << 0),

  /* stroke point is tagged (for some editing operation) */
  GP_SPOINT_TAG = (1 << 1),
  /* stroke point is temp tagged (for some editing operation) */
  GP_SPOINT_TEMP_TAG = (1 << 2),
} eGPSPoint_Flag;

/* ***************************************** */
/* GP Fill - Triangle Tessellation Data */

/* Grease-Pencil Annotations - 'Triangle'
 * -> A triangle contains the index of three vertices for filling the stroke
 *    This is only used if high quality fill is enabled
 */
typedef struct bGPDtriangle {
  /* indices for tessellated triangle used for GP Fill */
  unsigned int verts[3];
  /* texture coordinates for verts */
  float uv[3][2];
} bGPDtriangle;

/* ***************************************** */

/* ***************************************** */
/* GP Palettes (Deprecated - 2.78 - 2.79 only) */

/* color of palettes */
typedef struct bGPDpalettecolor {
  struct bGPDpalettecolor *next, *prev;
  /** Color name. Must be unique. */
  char info[64];
  float color[4];
  /** Color that should be used for drawing "fills" for strokes. */
  float fill[4];
  /** Settings for palette color. */
  short flag;
  /** Padding for compiler alignment error. */
  char _pad[6];
} bGPDpalettecolor;

/* bGPDpalettecolor->flag */
typedef enum eGPDpalettecolor_Flag {
  /* color is active */
  PC_COLOR_ACTIVE = (1 << 0),
  /* don't display color */
  PC_COLOR_HIDE = (1 << 1),
  /* protected from further editing */
  PC_COLOR_LOCKED = (1 << 2),
  /* do onion skinning */
  PC_COLOR_ONIONSKIN = (1 << 3),
  /* "volumetric" strokes */
  PC_COLOR_VOLUMETRIC = (1 << 4),
} eGPDpalettecolor_Flag;

/* palette of colors */
typedef struct bGPDpalette {
  struct bGPDpalette *next, *prev;

  /** Pointer to individual colors. */
  ListBase colors;
  /** Palette name. Must be unique. */
  char info[64];

  short flag;
  char _pad[6];
} bGPDpalette;

/* bGPDpalette->flag */
typedef enum eGPDpalette_Flag {
  /* palette is active */
  A,
  PL_PALETTE_ACTIVE = (1 << 0)
} eGPDpalette_Flag;

/* ***************************************** */
/* GP Strokes */

/* Runtime temp data for bGPDstroke */
typedef struct bGPDstroke_Runtime {
  /* runtime final colors (result of original colors and modifiers) */
  float tmp_stroke_rgba[4];
  float tmp_fill_rgba[4];

  /* temporary layer name only used during copy/paste to put the stroke in the original layer */
  char tmp_layerinfo[128];

  /** Runtime falloff factor (only for transform). */
  float multi_frame_falloff;
} bGPDstroke_Runtime;

/* Grease-Pencil Annotations - 'Stroke'
 * -> A stroke represents a (simplified version) of the curve
 *    drawn by the user in one 'mouse-down'->'mouse-up' operation
 */
typedef struct bGPDstroke {
  struct bGPDstroke *next, *prev;

  /** Array of data-points for stroke. */
  bGPDspoint *points;
  /** Tessellated triangles for GP Fill. */
  bGPDtriangle *triangles;
  /** Number of data-points in array. */
  int totpoints;
  /** Number of triangles in array. */
  int tot_triangles;

  /** Thickness of stroke. */
  short thickness;
  /** Various settings about this stroke. */
  short flag, _pad[2];

  /** Init time of stroke. */
  double inittime;

  /** Color name. */
  char colorname[128] DNA_DEPRECATED;

  /** Material index. */
  int mat_nr;
  /** Caps mode for each stroke extreme */
  short caps[2];

  /** gradient control along y for color */
  float gradient_f;
  /** factor xy of shape for dots gradients */
  float gradient_s[2];
  char _pad_3[4];

  /** Vertex weight data. */
  struct MDeformVert *dvert;
  void *_pad3;

  bGPDstroke_Runtime runtime;
  char _pad2[4];
} bGPDstroke;

/* bGPDstroke->flag */
typedef enum eGPDstroke_Flag {
  /* stroke is in 3d-space */
  GP_STROKE_3DSPACE = (1 << 0),
  /* stroke is in 2d-space */
  GP_STROKE_2DSPACE = (1 << 1),
  /* stroke is in 2d-space (but with special 'image' scaling) */
  GP_STROKE_2DIMAGE = (1 << 2),
  /* stroke is selected */
  GP_STROKE_SELECT = (1 << 3),
  /* Recalculate geometry data (triangulation, UVs, Bound Box,...
   * (when true, force a new recalc) */
  GP_STROKE_RECALC_GEOMETRY = (1 << 4),
  /* Flag used to indicate that stroke is closed and draw edge between last and first point */
  GP_STROKE_CYCLIC = (1 << 7),
  /* Flag used to indicate that stroke is used for fill close and must use
   * fill color for stroke and no fill area */
  GP_STROKE_NOFILL = (1 << 8),
  /* only for use with stroke-buffer (while drawing eraser) */
  GP_STROKE_ERASER = (1 << 15),
} eGPDstroke_Flag;

/* bGPDstroke->caps */
typedef enum eGPDstroke_Caps {
  /* type of extreme */
  GP_STROKE_CAP_ROUND = 0,
  GP_STROKE_CAP_FLAT = 1,

  GP_STROKE_CAP_MAX,
} GPDstroke_Caps;

/* ***************************************** */
/* GP Frame */

/* Runtime temp data for bGPDframe */
typedef struct bGPDframe_Runtime {
  /** Parent matrix for drawing. */
  float parent_obmat[4][4];
} bGPDframe_Runtime;

/* Grease-Pencil Annotations - 'Frame'
 * -> Acts as storage for the 'image' formed by strokes
 */
typedef struct bGPDframe {
  struct bGPDframe *next, *prev;

  /** List of the simplified 'strokes' that make up the frame's data. */
  ListBase strokes;

  /** Frame number of this frame. */
  int framenum;

  /** Temp settings. */
  short flag;
  /** Keyframe type (eBezTriple_KeyframeType). */
  short key_type;

  bGPDframe_Runtime runtime;
} bGPDframe;

/* bGPDframe->flag */
typedef enum eGPDframe_Flag {
  /* frame is being painted on */
  GP_FRAME_PAINT = (1 << 0),
  /* for editing in Action Editor */
  GP_FRAME_SELECT = (1 << 1),
} eGPDframe_Flag;

/* ***************************************** */
/* GP Layer */

/* Runtime temp data for bGPDlayer */
typedef struct bGPDlayer_Runtime {
  /** Id for dynamic icon used to show annotation color preview for layer. */
  int icon_id;
  char _pad[4];
} bGPDlayer_Runtime;

/* Grease-Pencil Annotations - 'Layer' */
typedef struct bGPDlayer {
  struct bGPDlayer *next, *prev;

  /** List of annotations to display for frames (bGPDframe list). */
  ListBase frames;
  /** Active frame (should be the frame that is currently being displayed). */
  bGPDframe *actframe;

  /** Settings for layer. */
  short flag;
  /** Per-layer onion-skinning flags (eGPDlayer_OnionFlag). */
  short onion_flag;

  /** Color for strokes in layers. Used for annotations, and for ruler
   * (which uses GPencil internally). */
  float color[4];
  /** Fill color for strokes in layers. Not used anymore (was only for). */
  float fill[4];

  /** Name/reference info for this layer (i.e. "director's comments, 12/.3")
   * needs to be kept unique, as it's used as the layer identifier */
  char info[128];

  /** Thickness to apply to strokes (Annotations). */
  short thickness;
  /** Used to filter groups of layers in modifiers. */
  short pass_index;

  /** Parent object. */
  struct Object *parent;
  /** Inverse matrix (only used if parented). */
  float inverse[4][4];
  /** String describing subobject info, MAX_ID_NAME-2. */
  char parsubstr[64];
  short partype;

  /** Thickness adjustment. */
  short line_change;
  /** Color used to tint layer, alpha value is used as factor. */
  float tintcolor[4];
  /** Opacity of the layer. */
  float opacity;
  /** Name of the layer used to filter render output. */
  char viewlayername[64];

  /** Blend modes. */
  int blend_mode;
  char _pad[4];

  /* annotation onion skin */
  /**
   * Ghosts Before: max number of ghost frames to show between
   * active frame and the one before it (0 = only the ghost itself).
   */
  short gstep;
  /**
   * Ghosts After: max number of ghost frames to show after
   * active frame and the following it    (0 = only the ghost itself).
   */
  short gstep_next;

  /** Color for ghosts before the active frame. */
  float gcolor_prev[3];
  /** Color for ghosts after the active frame. */
  float gcolor_next[3];
  char _pad1[4];

  bGPDlayer_Runtime runtime;
} bGPDlayer;

/* bGPDlayer->flag */
typedef enum eGPDlayer_Flag {
  /* don't display layer */
  GP_LAYER_HIDE = (1 << 0),
  /* protected from further editing */
  GP_LAYER_LOCKED = (1 << 1),
  /* layer is 'active' layer being edited */
  GP_LAYER_ACTIVE = (1 << 2),
  /* draw points of stroke for debugging purposes */
  GP_LAYER_DRAWDEBUG = (1 << 3),
  /* for editing in Action Editor */
  GP_LAYER_SELECT = (1 << 5),
  /* current frame for layer can't be changed */
  GP_LAYER_FRAMELOCK = (1 << 6),
  /* don't render xray (which is default) */
  GP_LAYER_NO_XRAY = (1 << 7),
  /* "volumetric" strokes */
  GP_LAYER_VOLUMETRIC = (1 << 10),
  /* Unlock color */
  GP_LAYER_UNLOCK_COLOR = (1 << 12),
  /* Mask Layer */
  GP_LAYER_USE_MASK = (1 << 13),
  /* Flag used to display in Paint mode only layers with keyframe */
  GP_LAYER_SOLO_MODE = (1 << 4),
} eGPDlayer_Flag;

/* bGPDlayer->onion_flag */
typedef enum eGPDlayer_OnionFlag {
  /* do onion skinning */
  GP_LAYER_ONIONSKIN = (1 << 0),
} eGPDlayer_OnionFlag;

/* layer blend_mode */
typedef enum eGPLayerBlendModes {
  eGplBlendMode_Regular = 0,
  eGplBlendMode_Overlay = 1,
  eGplBlendMode_Add = 2,
  eGplBlendMode_Subtract = 3,
  eGplBlendMode_Multiply = 4,
  eGplBlendMode_Divide = 5,
} eGPLayerBlendModes;

/* ***************************************** */
/* GP Datablock */

/* Runtime temp data for bGPdata */
typedef struct bGPdata_Runtime {
  /** Last region where drawing was originated. */
  struct ARegion *ar;
  /** Stroke buffer. */
  void *sbuffer;

  /* GP Object drawing */
  /** Buffer stroke color. */
  float scolor[4];
  /** Buffer fill color. */
  float sfill[4];
  /** Settings for color. */
  short mode;
  /** Buffer style for drawing strokes (used to select shader type). */
  short bstroke_style;
  /** Buffer style for filling areas (used to select shader type). */
  short bfill_style;

  /* Stroke Buffer data (only used during paint-session)
   * - buffer must be initialized before use, but freed after
   *   whole paint operation is over
   */
  /** Number of elements currently used in cache. */
  short sbuffer_used;
  /** Flags for stroke that cache represents. */
  short sbuffer_sflag;
  /** Number of total elements available in cache. */
  short sbuffer_size;
  char _pad[4];

  /** Number of control-points for stroke. */
  int tot_cp_points;
  char _pad1_[4];
  /** Array of control-points for stroke. */
  bGPDcontrolpoint *cp_points;
} bGPdata_Runtime;

/* grid configuration */
typedef struct bGPgrid {
  float color[3];
  float scale[2];
  float offset[2];
  char _pad1[4];

  int lines;
  char _pad[4];
} bGPgrid;

/* Grease-Pencil Annotations - 'DataBlock' */
typedef struct bGPdata {
  /** Grease Pencil data is a data-block. */
  ID id;
  /** Animation data - for animating draw settings. */
  struct AnimData *adt;

  /* Grease-Pencil data */
  /** BGPDlayers. */
  ListBase layers;
  /** Settings for this data-block. */
  int flag;

  char _pad1[4];

  /* Palettes */
  /** List of bGPDpalette's   - Deprecated (2.78 - 2.79 only). */
  ListBase palettes DNA_DEPRECATED;

  /* 3D Viewport/Appearance Settings */
  /** Factor to define pixel size conversion. */
  float pixfactor;
  /** Color for edit line. */
  float line_color[4];

  /* Onion skinning */
  /** Onion alpha factor change. */
  float onion_factor;
  /** Onion skinning range (eGP_OnionModes). */
  int onion_mode;
  /** Onion skinning flags (eGPD_OnionFlag). */
  int onion_flag;
  /**
   * Ghosts Before: max number of ghost frames to show between
   * active frame and the one before it (0 = only the ghost itself).
   */
  short gstep;
  /** Ghosts After: max number of ghost frames to show after
   * active frame and the following it (0 = only the ghost itself).
   */
  short gstep_next;

  /** Optional color for ghosts before the active frame. */
  float gcolor_prev[3];
  /** Optional color for ghosts after the active frame. */
  float gcolor_next[3];

  /** Offset for drawing over surfaces to keep strokes on top. */
  float zdepth_offset;
  /** Materials array. */
  struct Material **mat;
  /** Total materials. */
  short totcol;

  /* stats */
  short totlayer;
  short totframe;
  char _pad2[6];
  int totstroke;
  int totpoint;

  /** Draw mode for strokes (eGP_DrawMode). */
  short draw_mode;
  /** Keyframe type for onion filter  (eBezTriple_KeyframeType plus All option) */
  short onion_keytype;

  bGPgrid grid;

  bGPdata_Runtime runtime;
} bGPdata;

/* bGPdata->flag */
/* NOTE: A few flags have been deprecated since early 2.5,
 *       since they have been made redundant by interaction
 *       changes made during the porting process.
 */
typedef enum eGPdata_Flag {
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
  /* Draw a green and red point to indicate start and end of the stroke */
  GP_DATA_SHOW_DIRECTION = (1 << 10),

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

  /* Force fill recalc if use deformation modifiers.
   * this is required if the stroke is deformed and the triangulation data is
   * not valid.
   */
  GP_DATA_STROKE_FORCE_RECALC = (1 << 17),
  /* Special mode drawing polygons */
  GP_DATA_STROKE_POLYGON = (1 << 18),
  /* Use adaptive UV scales */
  GP_DATA_UV_ADAPTIVE = (1 << 19),
  /* Autolock not active layers */
  GP_DATA_AUTOLOCK_LAYERS = (1 << 20),
  /* Internal flag for python update */
  GP_DATA_PYTHON_UPDATED = (1 << 21),
} eGPdata_Flag;

/* gpd->onion_flag */
typedef enum eGPD_OnionFlag {
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
} eGPD_OnionFlag;

/* gpd->onion_mode */
typedef enum eGP_OnionModes {
  GP_ONION_MODE_ABSOLUTE = 0,
  GP_ONION_MODE_RELATIVE = 1,
  GP_ONION_MODE_SELECTED = 2,
} eGP_OnionModes;

/* xray modes (Depth Ordering) */
typedef enum eGP_DepthOrdering {
  GP_XRAY_FRONT = 0,
  GP_XRAY_3DSPACE = 1,
} eGP_DepthOrdering;

/* draw modes (Use 2D or 3D position) */
typedef enum eGP_DrawMode {
  GP_DRAWMODE_2D = 0,
  GP_DRAWMODE_3D = 1,
} eGP_DrawMode;

/* ***************************************** */
/* Mode Checking Macros */

/* Check if 'multiedit sessions' is enabled */
#define GPENCIL_MULTIEDIT_SESSIONS_ON(gpd) \
  ((gpd) && \
   (gpd->flag & \
    (GP_DATA_STROKE_EDITMODE | GP_DATA_STROKE_SCULPTMODE | GP_DATA_STROKE_WEIGHTMODE)) && \
   (gpd->flag & GP_DATA_STROKE_MULTIEDIT))

/* Macros to check grease pencil modes */
#define GPENCIL_ANY_MODE(gpd) \
  ((gpd) && (gpd->flag & (GP_DATA_STROKE_PAINTMODE | GP_DATA_STROKE_EDITMODE | \
                          GP_DATA_STROKE_SCULPTMODE | GP_DATA_STROKE_WEIGHTMODE)))
#define GPENCIL_EDIT_MODE(gpd) ((gpd) && (gpd->flag & GP_DATA_STROKE_EDITMODE))
#define GPENCIL_ANY_EDIT_MODE(gpd) \
  ((gpd) && (gpd->flag & \
             (GP_DATA_STROKE_EDITMODE | GP_DATA_STROKE_SCULPTMODE | GP_DATA_STROKE_WEIGHTMODE)))
#define GPENCIL_PAINT_MODE(gpd) ((gpd) && (gpd->flag & (GP_DATA_STROKE_PAINTMODE)))
#define GPENCIL_SCULPT_OR_WEIGHT_MODE(gpd) \
  ((gpd) && (gpd->flag & (GP_DATA_STROKE_SCULPTMODE | GP_DATA_STROKE_WEIGHTMODE)))
#define GPENCIL_NONE_EDIT_MODE(gpd) \
  ((gpd) && ((gpd->flag & (GP_DATA_STROKE_EDITMODE | GP_DATA_STROKE_SCULPTMODE | \
                           GP_DATA_STROKE_WEIGHTMODE)) == 0))
#define GPENCIL_LAZY_MODE(brush, shift) \
  (((brush) && ((brush->gpencil_settings->flag & GP_BRUSH_STABILIZE_MOUSE) && (shift == 0))) || \
   (((brush->gpencil_settings->flag & GP_BRUSH_STABILIZE_MOUSE) == 0) && (shift == 1)))

#endif /*  __DNA_GPENCIL_TYPES_H__ */

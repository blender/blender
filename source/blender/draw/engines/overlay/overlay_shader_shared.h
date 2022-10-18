/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef GPU_SHADER
#  include "GPU_shader_shared_utils.h"

#  include "DNA_action_types.h"
#  include "DNA_view3d_types.h"

#  ifdef __cplusplus
extern "C" {
#  else
typedef enum OVERLAY_GridBits OVERLAY_GridBits;
#  endif
typedef struct OVERLAY_GridData OVERLAY_GridData;
#endif

/* TODO(fclem): Should eventually become OVERLAY_BackgroundType.
 * But there is no uint push constant functions at the moment. */
#define BG_SOLID 0
#define BG_GRADIENT 1
#define BG_CHECKER 2
#define BG_RADIAL 3
#define BG_SOLID_CHECKER 4
#define BG_MASK 5

enum OVERLAY_GridBits {
  SHOW_AXIS_X = (1u << 0u),
  SHOW_AXIS_Y = (1u << 1u),
  SHOW_AXIS_Z = (1u << 2u),
  SHOW_GRID = (1u << 3u),
  PLANE_XY = (1u << 4u),
  PLANE_XZ = (1u << 5u),
  PLANE_YZ = (1u << 6u),
  CLIP_ZPOS = (1u << 7u),
  CLIP_ZNEG = (1u << 8u),
  GRID_BACK = (1u << 9u),
  GRID_CAMERA = (1u << 10u),
  PLANE_IMAGE = (1u << 11u),
  CUSTOM_GRID = (1u << 12u),
};
#ifndef GPU_SHADER
ENUM_OPERATORS(OVERLAY_GridBits, CUSTOM_GRID)
#endif

/* Match: #SI_GRID_STEPS_LEN */
#define OVERLAY_GRID_STEPS_LEN 8

struct OVERLAY_GridData {
  float4 steps[OVERLAY_GRID_STEPS_LEN]; /* float arrays are padded to float4 in std130. */
  float4 size;                          /* float3 padded to float4. */
  float distance;
  float line_size;
  float zoom_factor; /* Only for UV editor */
  float _pad0;
};
BLI_STATIC_ASSERT_ALIGN(OVERLAY_GridData, 16)

#ifdef GPU_SHADER
/* Keep the same values as in `draw_cache_imp_curve.c` */
#  define ACTIVE_NURB (1 << 2)
#  define BEZIER_HANDLE (1 << 3)
#  define EVEN_U_BIT (1 << 4)
#  define COLOR_SHIFT 5

/* Keep the same value in `handle_display` in `DNA_view3d_types.h` */
#  define CURVE_HANDLE_SELECTED 0
#  define CURVE_HANDLE_ALL 1

#  define GP_EDIT_POINT_SELECTED 1u  /* 1 << 0 */
#  define GP_EDIT_STROKE_SELECTED 2u /* 1 << 1 */
#  define GP_EDIT_MULTIFRAME 4u      /* 1 << 2 */
#  define GP_EDIT_STROKE_START 8u    /* 1 << 3 */
#  define GP_EDIT_STROKE_END 16u     /* 1 << 4 */
#  define GP_EDIT_POINT_DIMMED 32u   /* 1 << 5 */

#  define MOTIONPATH_VERT_SEL (1 << 0)
#  define MOTIONPATH_VERT_KEY (1 << 1)

#else
/* TODO(fclem): Find a better way to share enums/defines from DNA files with GLSL. */
BLI_STATIC_ASSERT(CURVE_HANDLE_SELECTED == 0, "Ensure value is sync");
BLI_STATIC_ASSERT(CURVE_HANDLE_ALL == 1, "Ensure value is sync");
BLI_STATIC_ASSERT(MOTIONPATH_VERT_SEL == (1 << 0), "Ensure value is sync");
BLI_STATIC_ASSERT(MOTIONPATH_VERT_KEY == (1 << 1), "Ensure value is sync");
#endif

#ifndef GPU_SHADER
#  ifdef __cplusplus
}
#  endif
#endif

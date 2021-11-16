#define BRUSH_CHANNEL_INTERN

#include "MEM_guardedalloc.h"

#include "BLI_alloca.h"
#include "BLI_array.h"
#include "BLI_bitmap.h"
#include "BLI_compiler_attrs.h"
#include "BLI_compiler_compat.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_task.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "IMB_imbuf.h"

#include "DNA_brush_enums.h"
#include "DNA_brush_types.h"
#include "DNA_color_types.h"
#include "DNA_curveprofile_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_sculpt_brush_types.h"

#include "BKE_brush.h"
#include "BKE_colorband.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_node.h"
#include "BKE_paint.h"

#include "BKE_brush_engine.h"

#if defined(_MSC_VER) && !defined(__clang__)
#  pragma warning(error : 4018) /* signed/unsigned mismatch */
#  pragma warning(error : 4245) /* conversion from 'int' to 'unsigned int' */
#  pragma warning(error : 4389) /* signed/unsigned mismatch */
#  pragma warning(error : 4002) /* too many actual parameters for macro 'identifier' */
#  pragma warning(error : 4003) /* not enough actual parameters for macro 'identifier' */
#  pragma warning( \
      error : 4022) /* 'function': pointer mismatch for actual parameter 'parameter number' */
#  pragma warning(error : 4033) /* 'function' must return a value */
#endif

extern struct CurveMappingCache *brush_curve_cache;

#if 1
struct {
  char t1[32], t2[32], t3[32], t4[32];
} test[1] = {{1, 1, 1, 1}};
#endif

static bool check_builtin_init();

#if 1
#  define namestack_push(name)
#  define namestack_pop()
#else
void namestack_push(const char *name);
void namestack_pop();
#  define namestack_head_name strdup(namestack[namestack_i].tag)
#endif

/*
Instructions to add a built-in channel:

1. Add to brush_builtin_channels
2. Add to BKE_brush_builtin_patch to insert it in old brushes (without converting data)

To enable converting to/from old data:
3. If not a boolean mapping to a bitflag: Add to brush_settings_map
4. If a boolean mapping to a bitflag: Add to brush_flags_map_len.
*/
#define ICON_NONE "NONE"

/* clang-format off */
#define MAKE_FLOAT_EX_EX(idname1, name1, tooltip1, value1, min1, max1, smin1, smax1, pressure_enabled, pressure_inv, flag1) \
  {.name = name1, \
   .idname = #idname1, \
   .fvalue = value1,\
   .tooltip = tooltip1, \
   .min = min1,\
   .max = max1,\
   .soft_min = smin1,\
   .flag = flag1,\
   .soft_max = smax1,\
   .type = BRUSH_CHANNEL_TYPE_FLOAT,\
    .subtype = BRUSH_CHANNEL_FACTOR,\
   .mappings = {\
        .pressure = {.curve = CURVE_PRESET_LINE, .factor = 1.0f, .no_default=true, .blendmode = MA_RAMP_MULT, \
        .min = 0.0f, .max = 1.0f, .enabled = pressure_enabled, .inv = pressure_inv},\
    }\
},
#define MAKE_FLOAT_EX(idname1, name1, tooltip1, value1, min1, max1, smin1, smax1, pressure_enabled)\
MAKE_FLOAT_EX_EX(idname1, name1, tooltip1, value1, min1, max1, smin1, smax1, pressure_enabled, false, 0)
#define MAKE_FLOAT_EX_INV(idname1, name1, tooltip1, value1, min1, max1, smin1, smax1, pressure_enabled)\
MAKE_FLOAT_EX_EX(idname1, name1, tooltip1, value1, min1, max1, smin1, smax1, pressure_enabled, true, 0)

#define  MAKE_FLOAT_EX_FLAG( \
      idname, name, tooltip, val, min, max, smin, smax, pressure_enabled, flag) MAKE_FLOAT_EX_EX(idname, name, tooltip, val, min, max, smin, smax, pressure_enabled, false, flag)

#define MAKE_FLOAT(idname, name, tooltip, value, min, max) MAKE_FLOAT_EX(idname, name, tooltip, value, min, max, min, max, false)

#define MAKE_COLOR3(idname1, name1, tooltip1, r, g, b){\
  .idname = #idname1, \
  .name = name1, \
  .tooltip = tooltip1, \
  .type = BRUSH_CHANNEL_TYPE_VEC3,\
  .vector = {r, g, b, 1.0f},\
  .min = 0.0f, .max = 5.0f,\
  .soft_min = 0.0f, .soft_max = 1.0f,\
  .subtype = BRUSH_CHANNEL_COLOR,\
},

#define MAKE_FLOAT3_EX(idname1, name1, tooltip1, x1, y1, z1, min1, max1, smin, smax, flag1){\
  .idname = #idname1, \
  .name = name1, \
  .tooltip = tooltip1, \
  .type = BRUSH_CHANNEL_TYPE_VEC3,\
  .vector = {x1, y1, z1, 1.0f},\
  .min = min1, .max = max1,\
  .soft_min = smin, .soft_max = smax,\
  .flag = flag1,\
},

#define MAKE_FLOAT3(idname, name, tooltip, x, y, z, min, max)\
  MAKE_FLOAT3_EX(idname, name, tooltip, x, y, z, min, max, min, max, 0)

#define MAKE_COLOR4(idname1, name1, tooltip1, r, g, b, a){\
  .idname = #idname1, \
  .name = name1, \
  .tooltip = tooltip1, \
  .type = BRUSH_CHANNEL_TYPE_VEC4,\
  .vector = {r, g, b, a},\
  .min = 0.0f, .max = 5.0f,\
  .soft_min = 0.0f, .soft_max = 1.0f,\
  .subtype = BRUSH_CHANNEL_COLOR,\
},

#define MAKE_INT_EX_OPEN(idname1, name1, tooltip1, value1, min1, max1, smin1, smax1) \
  {.name = name1, \
   .idname = #idname1, \
   .tooltip = tooltip1, \
   .min = min1,\
   .max = max1,\
   .ivalue = value1,\
   .soft_min = smin1,\
   .soft_max = smax1,\
   .type = BRUSH_CHANNEL_TYPE_INT

#define MAKE_INT_EX(idname, name, tooltip, value, min, max, smin, smax) \
  MAKE_INT_EX_OPEN(idname, name, tooltip, value, min, max, smin, smax) },

#define MAKE_INT(idname, name, tooltip, value, min, max) MAKE_INT_EX(idname, name, tooltip, value, min, max, min, max)

#define MAKE_BOOL_EX(idname1, name1, tooltip1, value1, flag1)\
  {.name = name1, \
   .idname = #idname1, \
   .tooltip = tooltip1, \
   .ivalue = value1,\
   .flag = flag1,\
   .type = BRUSH_CHANNEL_TYPE_BOOL\
  },

#define MAKE_BOOL(idname, name, tooltip, value)\
  MAKE_BOOL_EX(idname, name, tooltip, value, 0)

#define MAKE_ENUM_EX(idname1, name1, tooltip1, value1, flag1, enumdef1, ...)\
{.name = name1,\
 .idname = #idname1,\
 .tooltip = tooltip1,\
 .ivalue = value1,\
 .flag = flag1,\
 .type = BRUSH_CHANNEL_TYPE_ENUM,\
 .rna_enumdef = NULL,\
 .enumdef = enumdef1\
 , __VA_ARGS__\
},
#define MAKE_FLAGS_EX(idname1, name1, tooltip1, value1, flag1, enumdef1, ...)\
{.name = name1,\
 .idname = #idname1,\
 .tooltip = tooltip1,\
 .ivalue = value1,\
 .flag = flag1,\
 .type = BRUSH_CHANNEL_TYPE_BITMASK,\
 .rna_enumdef = NULL,\
 .enumdef = enumdef1 , __VA_ARGS__\
},
#define MAKE_FLAGS(idname1, name1, tooltip1, value1, enumdef1, ...) MAKE_FLAGS_EX(idname1, name1, tooltip1, value1, 0, enumdef1) 
#define MAKE_ENUM(idname1, name1, tooltip1, value1, enumdef1, ...) MAKE_ENUM_EX(idname1, name1, tooltip1, value1, 0, enumdef1, __VA_ARGS__) 

#define MAKE_CURVE(idname1, name1, tooltip1, preset1)\
{\
  .idname = #idname1,\
  .name = name1,\
  .tooltip = tooltip1,\
  .type = BRUSH_CHANNEL_TYPE_CURVE,\
  .curve_preset = preset1,\
},

/*
This is where all the builtin brush channels are defined.
That includes per-brush enums and bitflags!
*/

/* clang-format off */

BrushChannelType brush_builtin_channels[] = {    
#include "brush_channel_define.h"
};

/* clang-format on */
const int brush_builtin_channel_len = ARRAY_SIZE(brush_builtin_channels);

static BrushChannelType *_get_def(const char *idname)
{
  for (int i = 0; i < brush_builtin_channel_len; i++) {
    if (STREQ(brush_builtin_channels[i].idname, idname)) {
      return &brush_builtin_channels[i];
    }
  }

  return NULL;
}

#define GETDEF(idname) _get_def(MAKE_BUILTIN_CH_NAME(idname))
#define SETCAT(idname, cat) \
  BLI_strncpy(GETDEF(idname)->category, cat, sizeof(GETDEF(idname)->category))
#define SUBTYPE_SET(idname, type1) GETDEF(idname)->subtype = type1

static bool do_builtin_init = true;
static bool check_builtin_init()
{
  BrushChannelType *def;

  if (!do_builtin_init || !BLI_thread_is_main()) {
    return false;
  }

  do_builtin_init = false;

  // can't do this here, since we can't lookup icon ids in blenkernel
  // for (int i = 0; i < brush_builtin_channel_len; i++) {
  // BKE_brush_channeltype_rna_check(brush_builtin_channels + i);
  //}

  SUBTYPE_SET(smooth_stroke_radius, BRUSH_CHANNEL_PIXEL);

  SUBTYPE_SET(jitter_absolute, BRUSH_CHANNEL_PIXEL);

  SUBTYPE_SET(radius, BRUSH_CHANNEL_PIXEL);
  SUBTYPE_SET(spacing, BRUSH_CHANNEL_PERCENT);

  SUBTYPE_SET(autofset_spacing, BRUSH_CHANNEL_PERCENT);
  SUBTYPE_SET(autosmooth_spacing, BRUSH_CHANNEL_PERCENT);
  SUBTYPE_SET(topology_rake_spacing, BRUSH_CHANNEL_PERCENT);

  SUBTYPE_SET(autofset_radius_scale, BRUSH_CHANNEL_PERCENT);
  SUBTYPE_SET(autosmooth_radius_scale, BRUSH_CHANNEL_PERCENT);
  SUBTYPE_SET(topology_rake_radius_scale, BRUSH_CHANNEL_PERCENT);

  const char *inherit_mappings_channels[] = {"smooth_strength_factor",
                                             "smooth_strength_projection"};

  for (int i = 0; i < ARRAY_SIZE(inherit_mappings_channels); i++) {
    def = _get_def(inherit_mappings_channels[i]);

    for (int j = 0; j < BRUSH_MAPPING_MAX; j++) {
      (&def->mappings.pressure)[j].inherit = true;
    }
  }

  def = GETDEF(hue_offset);
  for (int i = 0; i < BRUSH_MAPPING_MAX; i++) {
    BrushMappingDef *mdef = (&def->mappings.pressure) + i;

    mdef->no_default = true;
    mdef->curve = CURVE_PRESET_LINE;
    mdef->min = i == 0 ? 0.0f : -1.0f; /* have pressure use minimum of 0.0f */
    mdef->max = 1.0f;
    mdef->factor = 0.5f;
    mdef->blendmode = MA_RAMP_ADD;
  }

  SETCAT(concave_mask_factor, "Automasking");
  SETCAT(automasking, "Automasking");
  SETCAT(automasking_boundary_edges_propagation_steps, "Automasking");

  def = GETDEF(concave_mask_factor);
  def->mappings.pressure.inv = true;

  // don't group strength/radius/direction in subpanels
  // SETCAT(strength, "Basic");
  // SETCAT(radius, "Basic");
  // SETCAT(direction, "Basic");
  SETCAT(accumulate, "Basic");
  SETCAT(use_frontface, "Basic");

  SETCAT(smear_deform_type, "Smear");
  SETCAT(smear_deform_blend, "Smear");

  SETCAT(tip_roundness, "Basic");
  SETCAT(hardness, "Basic");
  SETCAT(tip_scale_x, "Basic");
  SETCAT(tip_roundness, "Basic");
  SETCAT(normal_radius_factor, "Basic");
  SETCAT(use_smoothed_rake, "Basic");

  SETCAT(plane_offset, "Clay");
  SETCAT(plane_trim, "Clay");
  SETCAT(original_normal, "Clay");
  SETCAT(original_plane, "Clay");

  SETCAT(spacing, "Stroke");
  SETCAT(use_space_attenuation, "Stroke");
  SETCAT(use_smooth_stroke, "Stroke");
  SETCAT(smooth_stroke_factor, "Stroke");
  SETCAT(smooth_stroke_radius, "Stroke");
  SETCAT(jitter_absolute, "Stroke");
  SETCAT(jitter_unit, "Stroke");
  SETCAT(jitter, "Stroke");

  SETCAT(autosmooth, "Smoothing");
  SETCAT(autosmooth_projection, "Smoothing");
  SETCAT(autosmooth_radius_scale, "Smoothing");
  SETCAT(autosmooth_spacing, "Smoothing");
  SETCAT(autosmooth_use_spacing, "Smoothing");

  SETCAT(topology_rake, "Smoothing");
  SETCAT(topology_rake_radius_scale, "Smoothing");
  SETCAT(topology_rake_projection, "Smoothing");
  SETCAT(topology_rake_use_spacing, "Smoothing");
  SETCAT(topology_rake_spacing, "Smoothing");
  SETCAT(topology_rake_mode, "Smoothing");

  SETCAT(boundary_smooth, "Smoothing");
  SETCAT(fset_slide, "Smoothing");
  SETCAT(preserve_faceset_boundary, "Smoothing");
  SETCAT(use_weighted_smooth, "Smoothing");

  SETCAT(boundary_offset, "Boundary Tool");
  SETCAT(boundary_deform_type, "Boundary Tool");
  SETCAT(boundary_falloff_type, "Boundary Tool");

  SETCAT(cloth_deform_type, "Cloth Tool");
  SETCAT(cloth_simulation_area_type, "Cloth Tool");
  SETCAT(cloth_force_falloff_type, "Cloth Tool");
  SETCAT(cloth_mass, "Cloth Tool");
  SETCAT(cloth_damping, "Cloth Tool");
  SETCAT(cloth_sim_limit, "Cloth Tool");
  SETCAT(cloth_sim_falloff, "Cloth Tool");
  SETCAT(cloth_constraint_softbody_strength, "Cloth Tool");
  SETCAT(cloth_use_collision, "Cloth Tool");
  SETCAT(cloth_pin_simulation_boundary, "Cloth Tool");
  SETCAT(cloth_solve_bending, "Cloth Tool");
  SETCAT(cloth_bending_stiffness, "Cloth Tool");

  SETCAT(pose_offset, "Pose Tool");
  SETCAT(pose_smooth_iterations, "Pose Tool");
  SETCAT(pose_ik_segments, "Pose Tool");
  SETCAT(use_pose_ik_anchored, "Pose Tool");
  SETCAT(use_pose_lock_rotation, "Pose Tool");
  SETCAT(pose_deform_type, "Pose Tool");
  SETCAT(pose_origin_type, "Pose Tool");
  SETCAT(pose_origin_type, "Pose Tool");
  SETCAT(pose_origin_type, "Pose Tool");
  SETCAT(pose_origin_type, "Pose Tool");
  SETCAT(pose_origin_type, "Pose Tool");
  SETCAT(pose_origin_type, "Pose Tool");

  SETCAT(color, "Color");
  SETCAT(secondary_color, "Color");
  SETCAT(blend, "Color");
  SETCAT(wet_mix, "Color");
  SETCAT(hue_offset, "Color");
  SETCAT(wet_persistence, "Color");
  SETCAT(density, "Color");
  SETCAT(flow, "Color");
  SETCAT(wet_paint_radius_factor, "Color");

  SETCAT(vcol_boundary_spacing, "Color Boundary Hardening");
  SETCAT(vcol_boundary_radius_scale, "Color Boundary Hardening");
  SETCAT(vcol_boundary_exponent, "Color Boundary Hardening");
  SETCAT(vcol_boundary_factor, "Color Boundary Hardening");

  return true;
}

#ifdef FLOAT
#  undef FLOAT
#endif
#ifdef INT
#  undef INT
#endif
#ifdef BOOL
#  undef BOOL
#endif

#define FLOAT BRUSH_CHANNEL_TYPE_FLOAT
#define INT BRUSH_CHANNEL_TYPE_INT
#define BOOL BRUSH_CHANNEL_TYPE_BOOL
#define FLOAT3 BRUSH_CHANNEL_TYPE_VEC3
#define FLOAT4 BRUSH_CHANNEL_TYPE_VEC4

#ifdef ADDCH
#  undef ADDCH
#endif
#ifdef GETCH
#  undef GETCH
#endif

/* TODO: replace these two macros with equivalent BRUSHSET_XXX ones */
#define ADDCH(name) \
  (BKE_brush_channelset_ensure_builtin(chset, BRUSH_BUILTIN_##name), \
   BKE_brush_channelset_lookup(chset, BRUSH_BUILTIN_##name))
#define GETCH(name) BKE_brush_channelset_lookup(chset, BRUSH_BUILTIN_##name)

/* create pointer map between brush channels and old brush settings */

/* clang-format off */
#define DEF(brush_member, channel_name, btype, ctype) \
  {offsetof(Brush, brush_member), #channel_name, btype, ctype, sizeof(((Brush){0}).brush_member)},
/* clang-format on */

typedef struct BrushSettingsMap {
  int brush_offset;
  const char *channel_name;
  int brush_type;
  int channel_type;
  int member_size;
} BrushSettingsMap;

/* clang-format off */

/* This lookup table is used convert between brush channels
   and the old settings members of Brush*/
static BrushSettingsMap brush_settings_map[] = {
  DEF(size, radius, INT, FLOAT)
  DEF(alpha, strength, FLOAT, FLOAT)
  DEF(spacing, spacing, INT, FLOAT)
  DEF(automasking_flags, automasking, INT, INT)
  DEF(autosmooth_factor, autosmooth, FLOAT, FLOAT)
  DEF(area_radius_factor, area_radius_factor, FLOAT, FLOAT)
  DEF(autosmooth_projection, autosmooth_projection, FLOAT, FLOAT)
  DEF(topology_rake_projection, topology_rake_projection, FLOAT, FLOAT)
  DEF(topology_rake_radius_factor, topology_rake_radius_scale, FLOAT, FLOAT)
  DEF(topology_rake_spacing, topology_rake_spacing, INT, FLOAT)
  DEF(topology_rake_factor, topology_rake, FLOAT, FLOAT)
  DEF(autosmooth_fset_slide, fset_slide, FLOAT, FLOAT)
  DEF(boundary_smooth_factor, boundary_smooth, FLOAT, FLOAT)
  DEF(autosmooth_radius_factor, autosmooth_radius_scale, FLOAT, FLOAT)
  DEF(normal_weight, normal_weight, FLOAT, FLOAT)
  DEF(rake_factor, rake_factor, FLOAT, FLOAT)
  DEF(weight, weight, FLOAT, FLOAT)
  DEF(multiplane_scrape_angle, multiplane_scrape_angle, FLOAT, FLOAT)
  DEF(jitter, jitter, FLOAT, FLOAT)
  DEF(jitter_absolute, jitter_absolute, INT, INT)
  DEF(smooth_stroke_radius, smooth_stroke_radius, INT, FLOAT)
  DEF(smooth_stroke_factor, smooth_stroke_factor, FLOAT, FLOAT)
  DEF(rate, rate, FLOAT, FLOAT)
  DEF(flow, flow, FLOAT, FLOAT)
  DEF(hardness, hardness, FLOAT, FLOAT)
  DEF(wet_mix, wet_mix, FLOAT, FLOAT)
  DEF(wet_paint_radius_factor, wet_paint_radius_factor, FLOAT, FLOAT)
  DEF(wet_persistence, wet_persistence, FLOAT, FLOAT)
  DEF(density, density, FLOAT, FLOAT)
  DEF(tip_scale_x, tip_scale_x, FLOAT, FLOAT)
  DEF(concave_mask_factor, concave_mask_factor, FLOAT, FLOAT)
  DEF(automasking_boundary_edges_propagation_steps, automasking_boundary_edges_propagation_steps, INT, INT)
  DEF(add_col, cursor_color_add, FLOAT4, FLOAT4)
  DEF(sub_col, cursor_color_sub, FLOAT4, FLOAT4)
  DEF(rgb, color, FLOAT3, FLOAT3)
  DEF(secondary_rgb, secondary_color, FLOAT3, FLOAT3)
  DEF(vcol_boundary_factor, vcol_boundary_factor, FLOAT, FLOAT)
  DEF(vcol_boundary_exponent, vcol_boundary_exponent, FLOAT, FLOAT)
  DEF(cloth_deform_type, cloth_deform_type, INT, INT)
  DEF(cloth_simulation_area_type, cloth_simulation_area_type, INT, INT)
  DEF(cloth_force_falloff_type, cloth_force_falloff_type, INT, INT)
  DEF(cloth_mass, cloth_mass, FLOAT, FLOAT)
  DEF(cloth_damping, cloth_damping, FLOAT, FLOAT)
  DEF(cloth_sim_limit, cloth_sim_limit, FLOAT, FLOAT)
  DEF(cloth_sim_falloff, cloth_sim_falloff, FLOAT, FLOAT)
  DEF(cloth_constraint_softbody_strength, cloth_constraint_softbody_strength, FLOAT, FLOAT)
  DEF(boundary_offset, boundary_offset, FLOAT, FLOAT)
  DEF(boundary_deform_type, boundary_deform_type, INT, INT)
  DEF(boundary_falloff_type, boundary_falloff_type, INT, INT)
  DEF(deform_target, deform_target, INT, INT)
  DEF(tilt_strength_factor, tilt_strength_factor, FLOAT, FLOAT)
  DEF(crease_pinch_factor, crease_pinch_factor, FLOAT, FLOAT)
  DEF(pose_offset, pose_offset, FLOAT, FLOAT)
  DEF(disconnected_distance_max, disconnected_distance_max, FLOAT, FLOAT)
  DEF(surface_smooth_shape_preservation, surface_smooth_shape_preservation, FLOAT, FLOAT)
  DEF(pose_smooth_iterations, pose_smooth_iterations, INT, INT)
  DEF(pose_ik_segments, pose_ik_segments, INT, INT)
  DEF(surface_smooth_shape_preservation, surface_smooth_shape_preservation, FLOAT, FLOAT)
  DEF(surface_smooth_current_vertex, surface_smooth_current_vertex, FLOAT, FLOAT)
  DEF(surface_smooth_iterations, surface_smooth_iterations, INT, INT)
  DEF(pose_deform_type, pose_deform_type, INT, INT)
  DEF(pose_origin_type, pose_origin_type, INT, INT)
  DEF(snake_hook_deform_type, snake_hook_deform_type, INT, INT)
  DEF(tip_roundness, tip_roundness, FLOAT, FLOAT)
  DEF(tip_scale_x, tip_scale_x, FLOAT, FLOAT)
  DEF(height, height, FLOAT, FLOAT)
  DEF(elastic_deform_type, elastic_deform_type, INT, INT)
  DEF(plane_offset, plane_offset, FLOAT, FLOAT)
  DEF(plane_trim, plane_trim, FLOAT, FLOAT)
  DEF(blend, blend, INT, INT)
  DEF(elastic_deform_volume_preservation, elastic_deform_volume_preservation, FLOAT, FLOAT)
  DEF(smooth_deform_type, smooth_deform_type, INT, INT)
  DEF(array_deform_type, array_deform_type, INT, INT)
  DEF(array_count, array_count, INT, INT)
  DEF(smear_deform_type, smear_deform_type, INT, INT)
  DEF(slide_deform_type, slide_deform_type, INT, INT)
};

static const int brush_settings_map_len = ARRAY_SIZE(brush_settings_map);

#undef DEF
#define DEF(brush_member, channel_name, btype, ctype) \
  {offsetof(DynTopoSettings, brush_member), \
   #channel_name, \
   btype, \
   ctype, \
   sizeof(((DynTopoSettings){0}).brush_member)},

static BrushSettingsMap dyntopo_settings_map[] = {
  DEF(detail_range, dyntopo_detail_range, FLOAT, FLOAT)
  DEF(detail_percent, dyntopo_detail_percent, FLOAT, FLOAT)
  DEF(detail_size, dyntopo_detail_size, FLOAT, FLOAT)
  DEF(constant_detail, dyntopo_constant_detail, FLOAT, FLOAT)
  DEF(spacing, dyntopo_spacing, INT, FLOAT)
  DEF(radius_scale, dyntopo_radius_scale, FLOAT, FLOAT)
  DEF(mode, dyntopo_detail_mode, INT, INT)
};

static const int dyntopo_settings_map_len = ARRAY_SIZE(dyntopo_settings_map);

/* clang-format on */

typedef struct BrushFlagMap {
  int member_offset;
  char *channel_name;
  int flag;
  int member_size;
  int bitmask_bit;
} BrushFlagMap;

/* clang-format off */
#ifdef DEF
#undef DEF
#endif

#define DEF(member, channel, flag)\
  {offsetof(Brush, member), #channel, flag, sizeof(((Brush){0}).member), 0},

#define DEFBIT(member, channel, flag, bit)\
  {offsetof(Brush, member), #channel, flag, sizeof(((Brush){0}).member), bit},

/* This lookup table is like brush_settings_map except it converts
   individual bitflags instead of whole struct members.*/
BrushFlagMap brush_flags_map[] =  {
  DEF(flag, direction, BRUSH_DIR_IN)
  DEF(flag, original_normal, BRUSH_ORIGINAL_NORMAL)
  DEF(flag, original_plane, BRUSH_ORIGINAL_PLANE)
  DEF(flag, accumulate, BRUSH_ACCUMULATE)
  DEF(flag2, use_weighted_smooth, BRUSH_SMOOTH_USE_AREA_WEIGHT)
  DEF(flag2, preserve_faceset_boundary, BRUSH_SMOOTH_PRESERVE_FACE_SETS)
  //DEF(flag2, hard_edge_mode, BRUSH_HARD_EDGE_MODE) don't convert, this only existed on temp_bmesh_multires
  DEF(flag2, grab_silhouette, BRUSH_GRAB_SILHOUETTE)
  DEF(flag, invert_to_scrape_fill, BRUSH_INVERT_TO_SCRAPE_FILL)
  DEF(flag2, use_multiplane_scrape_dynamic, BRUSH_MULTIPLANE_SCRAPE_DYNAMIC)
  DEF(flag2, show_multiplane_scrape_planes_preview, BRUSH_MULTIPLANE_SCRAPE_PLANES_PREVIEW)
  DEF(flag, use_persistent, BRUSH_PERSISTENT)
  DEF(flag, use_frontface, BRUSH_FRONTFACE)
  DEF(flag2, cloth_use_collision, BRUSH_CLOTH_USE_COLLISION)
  DEF(flag2, cloth_pin_simulation_boundary, BRUSH_CLOTH_PIN_SIMULATION_BOUNDARY)
  DEF(flag,  radius_unit, BRUSH_LOCK_SIZE)
  DEF(flag2, use_pose_ik_anchored, BRUSH_POSE_IK_ANCHORED)
  DEF(flag2, use_connected_only, BRUSH_USE_CONNECTED_ONLY)
  DEF(flag2, use_pose_lock_rotation, BRUSH_POSE_USE_LOCK_ROTATION)
  DEF(flag, use_space_attenuation, BRUSH_SPACE_ATTEN)
  DEF(flag, use_plane_trim, BRUSH_PLANE_TRIM)
  DEF(flag2, use_surface_falloff, BRUSH_USE_SURFACE_FALLOFF)
  DEF(flag2, use_grab_active_vertex, BRUSH_GRAB_ACTIVE_VERTEX)
  DEF(flag, accumulate, BRUSH_ACCUMULATE)
  DEF(flag, use_smooth_stroke, BRUSH_SMOOTH_STROKE)
  DEFBIT(flag, jitter_unit, BRUSH_ABSOLUTE_JITTER, BRUSH_ABSOLUTE_JITTER)
};

int brush_flags_map_len = ARRAY_SIZE(brush_flags_map);

/* clang-format on */

static void do_coerce(int type1, void *ptr1, int size1, int type2, void *ptr2, int size2)
{
  double val = 0;
  float vec[4];

  switch (type1) {
    case BRUSH_CHANNEL_TYPE_FLOAT:
      val = *(float *)ptr1;
      break;
    case BRUSH_CHANNEL_TYPE_INT:
    case BRUSH_CHANNEL_TYPE_ENUM:
    case BRUSH_CHANNEL_TYPE_BITMASK:
    case BRUSH_CHANNEL_TYPE_BOOL:
      switch (size1) {
        case 1:
          val = (double)*(char *)ptr1;
          break;
        case 2:
          val = (double)*(unsigned short *)ptr1;
          break;
        case 4:
          val = (double)*(int *)ptr1;
          break;
        case 8:
          val = (double)*(int64_t *)ptr1;
          break;
      }
      break;
    case BRUSH_CHANNEL_TYPE_VEC3:
      copy_v3_v3(vec, (float *)ptr1);
      break;
    case BRUSH_CHANNEL_TYPE_VEC4:
      copy_v4_v4(vec, (float *)ptr1);
      break;
  }

  switch (type2) {
    case BRUSH_CHANNEL_TYPE_FLOAT:
      *(float *)ptr2 = (float)val;
      break;
    case BRUSH_CHANNEL_TYPE_INT:
    case BRUSH_CHANNEL_TYPE_ENUM:
    case BRUSH_CHANNEL_TYPE_BITMASK:
    case BRUSH_CHANNEL_TYPE_BOOL: {
      switch (size2) {
        case 1:
          *(char *)ptr2 = (char)val;
          break;
        case 2:
          *(unsigned short *)ptr2 = (unsigned short)val;
          break;
        case 4:
          *(int *)ptr2 = (int)val;
          break;
        case 8:
          *(int64_t *)ptr2 = (int64_t)val;
          break;
      }
      break;
    }
    case BRUSH_CHANNEL_TYPE_VEC3:
      copy_v3_v3((float *)ptr2, vec);
      break;
    case BRUSH_CHANNEL_TYPE_VEC4:
      copy_v4_v4((float *)ptr2, vec);
      break;
  }
}

void *get_channel_value_pointer(BrushChannel *ch, int *r_data_size)
{
  *r_data_size = 4;

  switch (ch->type) {
    case BRUSH_CHANNEL_TYPE_FLOAT:
      return &ch->fvalue;
    case BRUSH_CHANNEL_TYPE_INT:
    case BRUSH_CHANNEL_TYPE_ENUM:
    case BRUSH_CHANNEL_TYPE_BITMASK:
    case BRUSH_CHANNEL_TYPE_BOOL:
      return &ch->ivalue;
    case BRUSH_CHANNEL_TYPE_VEC3:
      *r_data_size = sizeof(float) * 3;
      return ch->vector;
    case BRUSH_CHANNEL_TYPE_VEC4:
      *r_data_size = sizeof(float) * 4;
      return ch->vector;
  }

  return NULL;
}

static int brushflag_from_channel(BrushFlagMap *mf, int flag, int val)
{
  if (mf->bitmask_bit == 0) {
    return val ? flag | mf->flag : flag & ~mf->flag;
  }

  if (val & mf->bitmask_bit) {
    flag |= mf->flag;
  }
  else {
    flag &= ~mf->flag;
  }

  return flag;
}

static int brushflag_to_channel(BrushFlagMap *mf, int chvalue, int val)
{
  if (mf->bitmask_bit == 0) {
    return val & mf->flag ? 1 : 0;
  }

  if (val & mf->flag) {
    chvalue |= mf->bitmask_bit;
  }
  else {
    chvalue &= ~mf->bitmask_bit;
  }

  return chvalue;
}

static void brush_flags_from_channels(BrushChannelSet *chset, Brush *brush)
{
  for (int i = 0; i < brush_flags_map_len; i++) {
    BrushFlagMap *mf = brush_flags_map + i;
    BrushChannel *ch = BKE_brush_channelset_lookup(chset, mf->channel_name);

    if (!ch) {
      continue;
    }

    char *ptr = (char *)brush;
    ptr += mf->member_offset;

    switch (mf->member_size) {
      case 1: {
        char *f = (char *)ptr;
        *f = (char)brushflag_from_channel(mf, *f, ch->ivalue);
        break;
      }
      case 2: {
        ushort *f = (ushort *)ptr;
        *f = (ushort)brushflag_from_channel(mf, *f, ch->ivalue);
        break;
      }
      case 4: {
        uint *f = (uint *)ptr;
        *f = (uint)brushflag_from_channel(mf, *f, ch->ivalue);
        break;
      }
      case 8: {
        uint64_t *f = (uint64_t *)ptr;
        *f = (uint64_t)brushflag_from_channel(mf, *f, ch->ivalue);
        break;
      }
    }
  }
}

static void brush_flags_to_channels(BrushChannelSet *chset, Brush *brush)
{
  for (int i = 0; i < brush_flags_map_len; i++) {
    BrushFlagMap *mf = brush_flags_map + i;
    BrushChannel *ch = BKE_brush_channelset_lookup(chset, mf->channel_name);

    if (!ch) {
      continue;
    }

    char *ptr = (char *)brush;
    ptr += mf->member_offset;

    switch (mf->member_size) {
      case 1: {
        char *f = (char *)ptr;
        ch->ivalue = brushflag_to_channel(mf, ch->ivalue, *f);
        break;
      }
      case 2: {
        ushort *f = (ushort *)ptr;
        ch->ivalue = brushflag_to_channel(mf, ch->ivalue, *f);
        break;
      }
      case 4: {
        uint *f = (uint *)ptr;
        ch->ivalue = brushflag_to_channel(mf, ch->ivalue, *f);
        break;
      }
      case 8: {
        uint64_t *f = (uint64_t *)ptr;
        ch->ivalue = brushflag_to_channel(mf, ch->ivalue, *f);
        break;
      }
    }
  }
}

void BKE_brush_channelset_compat_load_intern(BrushChannelSet *chset,
                                             void *data,
                                             bool brush_to_channels,
                                             BrushSettingsMap *settings_map,
                                             int settings_map_len)
{
  for (int i = 0; i < settings_map_len; i++) {
    BrushSettingsMap *mp = settings_map + i;
    BrushChannel *ch = BKE_brush_channelset_lookup(chset, mp->channel_name);

    if (!ch) {
      continue;
    }

    char *bptr = (char *)data;
    bptr += mp->brush_offset;

    int csize;
    void *cptr = get_channel_value_pointer(ch, &csize);

    if (brush_to_channels) {
      do_coerce(mp->brush_type, bptr, mp->member_size, ch->type, cptr, csize);
    }
    else {
      do_coerce(ch->type, cptr, csize, mp->brush_type, bptr, mp->member_size);
    }
  }
}

void BKE_brush_channelset_compat_load(BrushChannelSet *chset, Brush *brush, bool brush_to_channels)
{
  if (brush_to_channels) {
    brush_flags_to_channels(chset, brush);
  }
  else {
    brush_flags_from_channels(chset, brush);
  }

  BKE_brush_channelset_compat_load_intern(
      chset, (void *)brush, brush_to_channels, brush_settings_map, brush_settings_map_len);
  BKE_brush_channelset_compat_load_intern(chset,
                                          (void *)&brush->dyntopo,
                                          brush_to_channels,
                                          dyntopo_settings_map,
                                          dyntopo_settings_map_len);

  // now handle dyntopo mode
  if (!brush_to_channels) {
    int flag = BKE_brush_channelset_get_int(chset, "dyntopo_mode", NULL);

    if (BKE_brush_channelset_get_int(chset, "dyntopo_disabled", NULL)) {
      flag |= DYNTOPO_DISABLED;
    }

    brush->dyntopo.flag = flag;
  }
  else {
    const int mask = DYNTOPO_COLLAPSE | DYNTOPO_SUBDIVIDE | DYNTOPO_CLEANUP |
                     DYNTOPO_LOCAL_COLLAPSE | DYNTOPO_LOCAL_SUBDIVIDE;

#ifdef SETCH
#  undef SETCH
#endif

    BKE_brush_channelset_set_int(chset, "dyntopo_mode", brush->dyntopo.flag & mask);
    BKE_brush_channelset_set_int(
        chset, "dyntopo_disabled", brush->dyntopo.flag & DYNTOPO_DISABLED ? 1 : 0);
  }

  /* pen pressure flags */
  if (brush_to_channels) {
    if (brush->flag & BRUSH_SIZE_PRESSURE) {
      BRUSHSET_LOOKUP(chset, radius)->mappings[BRUSH_MAPPING_PRESSURE].flag |=
          BRUSH_MAPPING_ENABLED;
    }
    else {
      BRUSHSET_LOOKUP(chset, autosmooth)->mappings[BRUSH_MAPPING_PRESSURE].flag &=
          ~BRUSH_MAPPING_ENABLED;
    }

    if (brush->flag & BRUSH_ALPHA_PRESSURE) {
      BRUSHSET_LOOKUP(chset, strength)->mappings[BRUSH_MAPPING_PRESSURE].flag |=
          BRUSH_MAPPING_ENABLED;
    }
    else {
      BRUSHSET_LOOKUP(chset, autosmooth)->mappings[BRUSH_MAPPING_PRESSURE].flag &=
          ~BRUSH_MAPPING_ENABLED;
    }

    if (brush->flag & BRUSH_JITTER_PRESSURE) {
      BRUSHSET_LOOKUP(chset, jitter)->mappings[BRUSH_MAPPING_PRESSURE].flag |=
          BRUSH_MAPPING_ENABLED;
    }
    else {
      BRUSHSET_LOOKUP(chset, autosmooth)->mappings[BRUSH_MAPPING_PRESSURE].flag &=
          ~BRUSH_MAPPING_ENABLED;
    }

    if (brush->flag & BRUSH_SPACING_PRESSURE) {
      BRUSHSET_LOOKUP(chset, spacing)->mappings[BRUSH_MAPPING_PRESSURE].flag |=
          BRUSH_MAPPING_ENABLED;
    }
    else {
      BRUSHSET_LOOKUP(chset, autosmooth)->mappings[BRUSH_MAPPING_PRESSURE].flag &=
          ~BRUSH_MAPPING_ENABLED;
    }

    if (brush->flag & BRUSH_INVERSE_SMOOTH_PRESSURE) {
      BRUSHSET_LOOKUP(chset, autosmooth)->mappings[BRUSH_MAPPING_PRESSURE].flag |=
          BRUSH_MAPPING_INVERT;
    }
    else {
      BRUSHSET_LOOKUP(chset, autosmooth)->mappings[BRUSH_MAPPING_PRESSURE].flag &=
          ~BRUSH_MAPPING_INVERT;
    }
  }
  else {
    if (BRUSHSET_LOOKUP(chset, radius)->mappings[BRUSH_MAPPING_PRESSURE].flag &
        BRUSH_MAPPING_ENABLED) {
      brush->flag |= BRUSH_SIZE_PRESSURE;
    }
    else {
      brush->flag &= ~BRUSH_SIZE_PRESSURE;
    }
    if (BRUSHSET_LOOKUP(chset, strength)->mappings[BRUSH_MAPPING_PRESSURE].flag &
        BRUSH_MAPPING_ENABLED) {
      brush->flag |= BRUSH_ALPHA_PRESSURE;
    }
    else {
      brush->flag &= ~BRUSH_ALPHA_PRESSURE;
    }
    if (BRUSHSET_LOOKUP(chset, jitter)->mappings[BRUSH_MAPPING_PRESSURE].flag &
        BRUSH_MAPPING_ENABLED) {
      brush->flag |= BRUSH_JITTER_PRESSURE;
    }
    else {
      brush->flag &= ~BRUSH_JITTER_PRESSURE;
    }
    if (BRUSHSET_LOOKUP(chset, spacing)->mappings[BRUSH_MAPPING_PRESSURE].flag &
        BRUSH_MAPPING_ENABLED) {
      brush->flag |= BRUSH_SPACING_PRESSURE;
    }
    else {
      brush->flag &= ~BRUSH_SPACING_PRESSURE;
    }

    if (BRUSHSET_LOOKUP(chset, autosmooth)->mappings[BRUSH_MAPPING_PRESSURE].flag &
        BRUSH_MAPPING_INVERT) {
      brush->flag |= BRUSH_INVERSE_SMOOTH_PRESSURE;
    }
    else {
      brush->flag &= ~BRUSH_INVERSE_SMOOTH_PRESSURE;
    }
  }
}

/* todo: move into BKE_brush_reset_mapping*/
void reset_clay_mappings(BrushChannelSet *chset, bool strips)
{
  BrushMapping *mp = BRUSHSET_LOOKUP(chset, radius)->mappings + BRUSH_MAPPING_PRESSURE;
  BKE_brush_mapping_ensure_write(mp);
  CurveMapping *curve = mp->curve;

  BKE_curvemapping_set_defaults(curve, 1, 0.0f, 0.0f, 1.0f, 1.0f);
  BKE_curvemap_reset(curve->cm,
                     &(struct rctf){.xmin = 0, .ymin = 0.0, .xmax = 1.0, .ymax = 1.0},
                     CURVE_PRESET_LINE,
                     1);
  BKE_curvemapping_init(curve);

  CurveMap *cuma = curve->cm;

  if (!strips) {  //[[0,0.200], [0.354,0.200], [0.595,0.210], [0.806,0.523], [1,1.000]
#if 0
    cuma->curve[0].x = 0.0f;
    cuma->curve[0].y = 0.2f;

    BKE_curvemap_insert(cuma, 0.35f, 0.2f);
    BKE_curvemap_insert(cuma, 0.6f, 0.210f);
    BKE_curvemap_insert(cuma, 0.8f, 0.525f);

    BKE_curvemapping_changed(curve, true);
#endif
  }
  else {
    //[[0,0], [0.250,0.050], [0.500,0.125], [0.750,0.422], [1,1]
    cuma->curve[0].x = 0.0f;
    cuma->curve[0].y = 0.55f;
    BKE_curvemap_insert(cuma, 0.5f, 0.7f);
    cuma->curve[2].x = 1.0f;
    cuma->curve[2].y = 1.0f;
    BKE_curvemapping_changed(curve, true);
  }

  mp = BRUSHSET_LOOKUP(chset, strength)->mappings + BRUSH_MAPPING_PRESSURE;
  BKE_brush_mapping_ensure_write(mp);
  curve = mp->curve;

  BKE_curvemapping_set_defaults(curve, 1, 0.0f, 0.0f, 1.0f, 1.0f);
  BKE_curvemap_reset(curve->cm,
                     &(struct rctf){.xmin = 0, .ymin = 0.0, .xmax = 1.0, .ymax = 1.0},
                     CURVE_PRESET_LINE,
                     1);
  BKE_curvemapping_init(curve);

  cuma = curve->cm;
  if (strips) {
    //[[0,0.000], [0.062,0.016], [0.250,0.074], [0.562,0.274], [1,1.000]
#if 0
    BKE_curvemap_insert(cuma, 0.062f, 0.016f);
    BKE_curvemap_insert(cuma, 0.25f, 0.074f);
    BKE_curvemap_insert(cuma, 0.562f, 0.274f);
#else
    BKE_curvemap_insert(cuma, 0.6f, 0.25f);
#endif
  }
  else {
  }
  BKE_curvemapping_changed(curve, true);
}

// adds any missing channels to brushes
void BKE_brush_builtin_patch(Brush *brush, int tool)
{
  check_builtin_init();

  namestack_push(__func__);

  bool setup_ui = !brush->channels || !brush->channels->totchannel;

  if (!brush->channels) {
    brush->channels = BKE_brush_channelset_create("brush");
  }

  BrushChannelSet *chset = brush->channels;

  bool set_mappings = BRUSHSET_LOOKUP(chset, radius) == NULL;

  ADDCH(radius);
  ADDCH(spacing);
  ADDCH(strength);
  ADDCH(radius_unit);
  ADDCH(unprojected_radius);
  ADDCH(use_frontface);

  ADDCH(sharp_mode);
  ADDCH(show_origco);
  ADDCH(save_temp_layers);

  ADDCH(use_surface_falloff);

  if (!BRUSHSET_LOOKUP(chset, use_smoothed_rake)) {
    BrushChannel *ch = ADDCH(use_smoothed_rake);

    if (tool == SCULPT_TOOL_CLAY_STRIPS) {
      ch->flag |= BRUSH_CHANNEL_SHOW_IN_WORKSPACE;
      ch->flag |= BRUSH_CHANNEL_SHOW_IN_CONTEXT_MENU;
    }
  }

  ADDCH(plane_offset);
  ADDCH(plane_trim);
  ADDCH(use_plane_trim);

  ADDCH(use_ctrl_invert);
  ADDCH(tilt_strength_factor);

  ADDCH(autosmooth);
  ADDCH(autosmooth_radius_scale);
  ADDCH(autosmooth_spacing);
  ADDCH(autosmooth_use_spacing);
  ADDCH(autosmooth_projection);
  ADDCH(autosmooth_falloff_curve);

  ADDCH(surface_smooth_shape_preservation);
  ADDCH(surface_smooth_current_vertex);
  ADDCH(surface_smooth_iterations);

  ADDCH(vcol_boundary_exponent);
  ADDCH(vcol_boundary_factor);
  ADDCH(vcol_boundary_radius_scale);
  ADDCH(vcol_boundary_spacing);

  ADDCH(topology_rake);
  ADDCH(topology_rake_mode);
  ADDCH(topology_rake_radius_scale);
  ADDCH(topology_rake_use_spacing);
  ADDCH(topology_rake_spacing);
  ADDCH(topology_rake_projection);
  ADDCH(topology_rake_falloff_curve);

  ADDCH(hardness);
  ADDCH(tip_roundness);
  ADDCH(normal_radius_factor);

  ADDCH(automasking);
  ADDCH(automasking_boundary_edges_propagation_steps);
  ADDCH(concave_mask_factor);

  ADDCH(dyntopo_disabled);
  ADDCH(dyntopo_disable_smooth);

  ADDCH(dyntopo_detail_mode);
  ADDCH(dyntopo_mode)->flag;
  ADDCH(dyntopo_detail_range);
  ADDCH(dyntopo_detail_percent);
  ADDCH(dyntopo_detail_size);
  ADDCH(dyntopo_constant_detail);
  ADDCH(dyntopo_spacing);
  ADDCH(dyntopo_radius_scale);

  if (!BRUSHSET_LOOKUP(chset, smooth_strength_factor)) {
    ADDCH(smooth_strength_factor)->flag |= BRUSH_CHANNEL_INHERIT;
  }

  if (!BRUSHSET_LOOKUP(chset, smooth_strength_projection)) {
    ADDCH(smooth_strength_projection)->flag |= BRUSH_CHANNEL_INHERIT;
  }

  ADDCH(accumulate);
  ADDCH(original_normal);
  ADDCH(original_plane);
  ADDCH(jitter);
  ADDCH(jitter_absolute);
  ADDCH(jitter_unit);
  ADDCH(use_weighted_smooth);
  ADDCH(preserve_faceset_boundary);
  ADDCH(hard_edge_mode);
  ADDCH(grab_silhouette);
  ADDCH(height);
  ADDCH(use_persistent);

  ADDCH(use_space_attenuation);

  ADDCH(projection);
  ADDCH(boundary_smooth);
  ADDCH(fset_slide);

  ADDCH(direction);
  ADDCH(dash_ratio);
  ADDCH(use_smooth_stroke);
  ADDCH(smooth_stroke_factor);
  ADDCH(smooth_stroke_radius);
  ADDCH(smooth_deform_type);

  ADDCH(use_autofset);
  ADDCH(autofset_radius_scale);
  ADDCH(autofset_curve);
  ADDCH(autofset_count);
  ADDCH(autofset_start);
  ADDCH(autofset_spacing);
  ADDCH(autofset_use_spacing);

  ADDCH(deform_target);

  switch (tool) {
    case SCULPT_TOOL_CLAY:
      if (set_mappings) {
        reset_clay_mappings(chset, false);
      }
      break;
    case SCULPT_TOOL_BLOB:
      ADDCH(crease_pinch_factor);
      break;
    case SCULPT_TOOL_GRAB:
      ADDCH(use_grab_active_vertex);
      break;
    case SCULPT_TOOL_ELASTIC_DEFORM:
      ADDCH(use_grab_active_vertex);
      break;
    case SCULPT_TOOL_ARRAY:
      ADDCH(array_deform_type);
      ADDCH(array_count);

      break;
    case SCULPT_TOOL_CLAY_STRIPS:
      if (set_mappings) {
        reset_clay_mappings(chset, true);
      }
      break;
    case SCULPT_TOOL_DRAW:
      break;
    case SCULPT_TOOL_PAINT:
      ADDCH(color);
      ADDCH(secondary_color);
      ADDCH(wet_mix);
      ADDCH(hue_offset);
      ADDCH(wet_paint_radius_factor);
      ADDCH(wet_persistence);
      ADDCH(density);
      ADDCH(tip_scale_x);
      ADDCH(tip_roundness);
      ADDCH(flow);
      ADDCH(rate);
      ADDCH(blend);

      break;
    case SCULPT_TOOL_SMEAR:
      ADDCH(rate);
      ADDCH(smear_deform_type);
      ADDCH(smear_deform_blend);
      break;
    case SCULPT_TOOL_SLIDE_RELAX:
      ADDCH(slide_deform_type);
      break;
    case SCULPT_TOOL_CLOTH:
      ADDCH(cloth_solve_bending);
      ADDCH(cloth_bending_stiffness);
      ADDCH(cloth_mass);
      ADDCH(cloth_damping);
      ADDCH(cloth_sim_limit);
      ADDCH(cloth_sim_falloff);
      ADDCH(cloth_deform_type);
      ADDCH(cloth_force_falloff_type);
      ADDCH(cloth_simulation_area_type);
      ADDCH(cloth_deform_type);
      ADDCH(cloth_use_collision);
      ADDCH(cloth_pin_simulation_boundary);
      ADDCH(cloth_constraint_softbody_strength);

      break;
    case SCULPT_TOOL_SNAKE_HOOK:
      ADDCH(crease_pinch_factor);
      ADDCH(rake_factor);
      ADDCH(snake_hook_deform_type);
      break;
    case SCULPT_TOOL_BOUNDARY:
      ADDCH(boundary_offset);
      ADDCH(boundary_deform_type);
      ADDCH(boundary_falloff_type);
      break;
    case SCULPT_TOOL_CREASE:
      ADDCH(crease_pinch_factor);
      break;
    case SCULPT_TOOL_POSE:
      ADDCH(deform_target);
      ADDCH(disconnected_distance_max);
      ADDCH(surface_smooth_shape_preservation);
      ADDCH(pose_smooth_iterations);
      ADDCH(pose_ik_segments);
      ADDCH(use_pose_ik_anchored);
      ADDCH(use_connected_only);
      ADDCH(use_pose_lock_rotation);
      ADDCH(pose_deform_type);
      ADDCH(pose_origin_type);

      break;
  }

  if (setup_ui) {
    BKE_brush_channelset_ui_init(brush, tool);
  }

  BKE_brush_channelset_check_radius(chset);

  namestack_pop();
}

void BKE_brush_channelset_ui_init(Brush *brush, int tool)
{
  namestack_push(__func__);

  BrushChannelSet *chset = brush->channels;

#ifdef SHOWHDR
#  undef SHOWHDR
#endif
#ifdef SHOWWRK
#  undef SHOWPROPS
#endif
#ifdef SHOWALL
#  undef SHOWALL
#endif
#ifdef SHOW_WRK_CTX
#  undef SHOW_WRK_CTX
#endif
#ifdef SETFLAG_SAFE
#  undef SETFLAG_SAFE
#endif

  BrushChannel *ch;

#define SETFLAG_SAFE(idname, flag1) \
  if ((ch = BRUSHSET_LOOKUP(chset, idname))) \
  ch->flag |= (flag1)

#define SHOWHDR(idname) SETFLAG_SAFE(idname, BRUSH_CHANNEL_SHOW_IN_HEADER)
#define SHOWWRK(idname) SETFLAG_SAFE(idname, BRUSH_CHANNEL_SHOW_IN_WORKSPACE)
#define SHOWCTX(idname) SETFLAG_SAFE(idname, BRUSH_CHANNEL_SHOW_IN_CONTEXT_MENU)

#define SHOW_WRK_CTX(idname) \
  SHOWWRK(idname); \
  SHOWCTX(idname)

#define SHOWALL(idname) \
  SETFLAG_SAFE(idname, \
               BRUSH_CHANNEL_SHOW_IN_WORKSPACE | BRUSH_CHANNEL_SHOW_IN_HEADER | \
                   BRUSH_CHANNEL_SHOW_IN_CONTEXT_MENU)

  SHOWALL(radius);
  SHOWALL(strength);
  SHOWALL(color);
  SHOWALL(secondary_color);

  if (!ELEM(tool,
            SCULPT_TOOL_DRAW_FACE_SETS,
            SCULPT_TOOL_PAINT,
            SCULPT_TOOL_SMEAR,
            SCULPT_TOOL_VCOL_BOUNDARY)) {
    SHOWWRK(hard_edge_mode);
  }

  if (!ELEM(tool,
            SCULPT_TOOL_SNAKE_HOOK,
            SCULPT_TOOL_ARRAY,
            SCULPT_TOOL_BOUNDARY,
            SCULPT_TOOL_POSE,
            SCULPT_TOOL_ROTATE,
            SCULPT_TOOL_SCENE_PROJECT,
            SCULPT_TOOL_SMEAR,
            SCULPT_TOOL_GRAB,
            SCULPT_TOOL_SLIDE_RELAX,
            SCULPT_TOOL_CLOTH,
            SCULPT_TOOL_ELASTIC_DEFORM,
            SCULPT_TOOL_FAIRING,
            SCULPT_TOOL_DRAW_FACE_SETS,
            SCULPT_TOOL_SMOOTH,
            SCULPT_TOOL_SIMPLIFY)) {

    SHOWALL(accumulate);
  }

  if (!ELEM(tool,
            SCULPT_TOOL_ARRAY,
            SCULPT_TOOL_POSE,
            SCULPT_TOOL_GRAB,
            SCULPT_TOOL_ELASTIC_DEFORM)) {
    SHOWWRK(direction);
  }

  SHOWWRK(radius_unit);
  SHOWWRK(use_frontface);

  if (!ELEM(tool, SCULPT_TOOL_PAINT, SCULPT_TOOL_SMEAR)) {
    SHOWWRK(autosmooth);
    SHOWWRK(topology_rake);
    SHOWWRK(topology_rake_mode);
    SHOWCTX(autosmooth);
  }

  if (ELEM(tool, SCULPT_TOOL_PAINT, SCULPT_TOOL_SMEAR)) {
    SHOWWRK(vcol_boundary_factor);
    SHOWWRK(vcol_boundary_exponent);
    SHOWWRK(vcol_boundary_radius_scale);
    SHOWWRK(vcol_boundary_spacing);
  }
  else if (tool == SCULPT_TOOL_VCOL_BOUNDARY) {
    SHOWWRK(vcol_boundary_exponent);
    SHOWCTX(vcol_boundary_exponent);
  }

  SHOWWRK(normal_radius_factor);
  SHOWWRK(hardness);
  SHOWWRK(dyntopo_disabled);

  switch (tool) {
    case SCULPT_TOOL_DRAW_SHARP:
      SHOWWRK(sharp_mode);
      SHOWCTX(sharp_mode);
      // SHOWWRK(plane_offset);
      // SHOWCTX(plane_offset);
      break;
    case SCULPT_TOOL_INFLATE:
    case SCULPT_TOOL_BLOB:
      SHOWCTX(crease_pinch_factor);
    case SCULPT_TOOL_DRAW:
      SHOWCTX(autosmooth);
      break;
    case SCULPT_TOOL_PINCH:
      SHOWCTX(autosmooth);
      SHOWALL(crease_pinch_factor);
      SHOWWRK(autosmooth);
    case SCULPT_TOOL_SMOOTH:
      SHOWWRK(surface_smooth_shape_preservation);
      SHOWWRK(surface_smooth_current_vertex);
      SHOWWRK(surface_smooth_iterations);
      SHOWWRK(smooth_deform_type);
      SHOWCTX(smooth_deform_type);
      SHOWWRK(projection);
      SHOWCTX(dyntopo_disabled);
      break;
    case SCULPT_TOOL_SCRAPE:
    case SCULPT_TOOL_FILL:
      SHOWWRK(plane_offset);
      SHOWWRK(plane_trim);
      SHOWWRK(invert_to_scrape_fill);
      SHOWWRK(area_radius_factor);

      SHOWCTX(autosmooth);
      SHOWCTX(plane_offset);
      SHOWCTX(plane_trim);
      SHOWCTX(use_plane_trim);
      break;
    case SCULPT_TOOL_SLIDE_RELAX:
      SHOWALL(slide_deform_type);
      break;
    case SCULPT_TOOL_GRAB:
      SHOWCTX(normal_weight);
      SHOWWRK(normal_weight);

      SHOWWRK(use_grab_active_vertex);
      SHOWCTX(use_grab_active_vertex);
      SHOWALL(grab_silhouette);
      break;
    case SCULPT_TOOL_SMEAR:
      SHOWALL(smear_deform_type);
      SHOW_WRK_CTX(spacing);

      // hrm, not sure this is such a good idea - joeedh
      // SHOWALL(smear_deform_blend);
      break;
    case SCULPT_TOOL_CLAY_STRIPS:
      SHOWWRK(area_radius_factor);
      SHOW_WRK_CTX(plane_offset);
      SHOW_WRK_CTX(plane_trim);
      SHOWWRK(tip_roundness);
      SHOW_WRK_CTX(use_plane_trim);

      SHOWWRK(use_smoothed_rake);
      SHOWCTX(use_smoothed_rake);

      SHOWCTX(autosmooth);
    case SCULPT_TOOL_CLAY:
    case SCULPT_TOOL_CLAY_THUMB:
    case SCULPT_TOOL_FLATTEN:
      SHOWWRK(area_radius_factor);
      SHOWWRK(plane_offset);
      SHOWWRK(plane_trim);
      SHOWWRK(tip_roundness);
      SHOWWRK(use_plane_trim);

      SHOWCTX(autosmooth);
      SHOWCTX(plane_offset);
      SHOWCTX(plane_trim);
      SHOWCTX(use_plane_trim);
      break;
    case SCULPT_TOOL_MULTIPLANE_SCRAPE:
      SHOWCTX(autosmooth);
      SHOWCTX(plane_offset);
      SHOWCTX(autosmooth);
      SHOWCTX(multiplane_scrape_angle);
      SHOW_WRK_CTX(use_plane_trim);

      SHOWWRK(plane_offset);
      SHOWWRK(plane_trim);
      SHOWWRK(area_radius_factor);
      SHOWWRK(use_multiplane_scrape_dynamic);
      SHOWWRK(multiplane_scrape_angle);

      break;
    case SCULPT_TOOL_SIMPLIFY:
      SHOW_WRK_CTX(autosmooth);
      SHOW_WRK_CTX(topology_rake);
      SHOW_WRK_CTX(topology_rake_mode);
      break;
    case SCULPT_TOOL_LAYER:
      SHOW_WRK_CTX(use_persistent);
      SHOWALL(height);
      SHOW_WRK_CTX(autosmooth);
      break;
    case SCULPT_TOOL_CLOTH:
      SHOW_WRK_CTX(spacing);

      SHOWWRK(cloth_solve_bending);
      SHOWWRK(cloth_bending_stiffness);

      SHOWALL(cloth_deform_type);
      SHOWALL(cloth_simulation_area_type);

      SHOW_WRK_CTX(cloth_force_falloff_type);

      SHOWWRK(cloth_mass);
      SHOWWRK(cloth_damping);
      SHOWWRK(cloth_constraint_softbody_strength);
      SHOW_WRK_CTX(cloth_sim_limit);
      SHOW_WRK_CTX(cloth_sim_falloff);
      SHOWWRK(cloth_constraint_softbody_strength);
      SHOW_WRK_CTX(cloth_use_collision);
      SHOW_WRK_CTX(cloth_pin_simulation_boundary);
      SHOWWRK(elastic_deform_type);

      break;
    case SCULPT_TOOL_BOUNDARY:
      SHOWWRK(boundary_offset);
      SHOWALL(boundary_deform_type);
      SHOWWRK(boundary_falloff_type);
      SHOWWRK(deform_target);
      SHOWCTX(boundary_offset);
      SHOWCTX(autosmooth);
      SHOWCTX(boundary_deform_type);
      SHOWCTX(boundary_falloff_type);
      break;
    case SCULPT_TOOL_CREASE:
      SHOWWRK(crease_pinch_factor);
      SHOWCTX(crease_pinch_factor);
      SHOWCTX(autosmooth);
      break;
    case SCULPT_TOOL_SNAKE_HOOK:
      SHOWCTX(autosmooth);

      SHOWWRK(crease_pinch_factor);
      SHOWWRK(rake_factor);
      SHOWWRK(snake_hook_deform_type);
      SHOWWRK(elastic_deform_type);
      SHOWWRK(normal_weight);

      SHOWCTX(crease_pinch_factor);
      SHOWCTX(normal_weight);
      SHOWCTX(rake_factor);
      break;
    case SCULPT_TOOL_PAINT:
      SHOWWRK(color);
      SHOWWRK(dyntopo_disabled);
      SHOWCTX(dyntopo_disabled);

      SHOWWRK(secondary_color);
      SHOWWRK(wet_mix);
      SHOWWRK(hue_offset);
      SHOWWRK(wet_paint_radius_factor);
      SHOWWRK(wet_persistence);
      SHOWWRK(density);
      SHOWWRK(tip_scale_x);
      SHOWWRK(hardness);
      SHOWWRK(wet_mix);
      SHOWWRK(tip_roundness);
      SHOWWRK(flow);
      SHOWWRK(rate);
      SHOWALL(blend);

      SHOWWRK(use_smoothed_rake);
      SHOWCTX(use_smoothed_rake);

      SHOWCTX(color);
      SHOWCTX(blend);
      break;
    case SCULPT_TOOL_POSE:
      SHOW_WRK_CTX(pose_ik_segments);
      SHOWWRK(pose_smooth_iterations);
      SHOWWRK(disconnected_distance_max);
      SHOW_WRK_CTX(pose_offset);
      SHOW_WRK_CTX(use_connected_only);
      SHOWWRK(use_pose_ik_anchored);
      SHOWWRK(use_pose_lock_rotation);
      SHOWWRK(pose_deform_type);

      SHOWALL(pose_origin_type);
      SHOWWRK(deform_target);
      SHOWWRK(elastic_deform_type);

      break;
    case SCULPT_TOOL_ELASTIC_DEFORM:
      SHOWALL(elastic_deform_type);
      break;
    case SCULPT_TOOL_ARRAY:
      SHOWWRK(array_deform_type);
      SHOWCTX(array_deform_type);

      SHOWALL(array_count);

      break;
  }

#undef SHOWALL
#undef SHOWHDR
#undef SHOWPROPS
  namestack_pop();
}

void BKE_brush_mapping_reset(BrushChannel *ch, int tool, int mapping)
{
  BrushMapping *mp = ch->mappings + mapping;
  BrushMappingDef *mdef = (&ch->def->mappings.pressure) + mapping;

  BKE_brush_mapping_ensure_write(mp);

  CurveMapping *curve = mp->curve;

  BKE_curvemapping_set_defaults(curve, 1, 0.0f, 0.0f, 1.0f, 1.0f);
  BKE_curvemap_reset(
      curve->cm, &(struct rctf){.xmin = 0, .ymin = 0.0, .xmax = 1.0, .ymax = 1.0}, mdef->curve, 1);
  BKE_curvemapping_init(curve);

  if (STREQ(ch->idname, "hue_offset") && mapping == BRUSH_MAPPING_PRESSURE) {
    CurveMap *cuma = curve->cm;
    cuma->curve[0].x = 0.0f;
    cuma->curve[0].y = 0.0f;

    cuma->curve[1].x = 1.0f;
    cuma->curve[1].y = 1.0f;

    BKE_curvemap_insert(cuma, 0.65f, 0.0f);
    cuma->curve[1].flag |= CUMA_HANDLE_VECTOR;
  }

  BKE_curvemapping_changed(curve, true);
}
void BKE_brush_builtin_create(Brush *brush, int tool)
{
  namestack_push(__func__);

  if (!brush->channels) {
    brush->channels = BKE_brush_channelset_create("brush 2");
  }

  if (brush->channels) {
    // forcibly reset all non-user-defined channels for this brush

    BrushChannelSet *chset = BKE_brush_channelset_create("brush 3");
    Brush tmp = *brush;
    tmp.channels = chset;

    BKE_brush_builtin_patch(&tmp, tool);

    BrushChannel *ch;
    for (ch = chset->channels.first; ch; ch = ch->next) {
      BrushChannel *ch2 = BKE_brush_channelset_lookup(brush->channels, ch->idname);

      if (ch2) {
        BKE_brush_channel_copy_data(ch, ch2, false, false);
      }
    }

    BKE_brush_channelset_free(chset);
  }

  BrushChannelSet *chset = brush->channels;

  BKE_brush_builtin_patch(brush, tool);

  GETCH(dyntopo_detail_mode)->flag |= BRUSH_CHANNEL_INHERIT;
  GETCH(dyntopo_mode)->flag |= BRUSH_CHANNEL_INHERIT;
  GETCH(dyntopo_detail_range)->flag |= BRUSH_CHANNEL_INHERIT;
  GETCH(dyntopo_detail_percent)->flag |= BRUSH_CHANNEL_INHERIT;
  GETCH(dyntopo_detail_size)->flag |= BRUSH_CHANNEL_INHERIT;
  GETCH(dyntopo_constant_detail)->flag |= BRUSH_CHANNEL_INHERIT;
  GETCH(dyntopo_spacing)->flag |= BRUSH_CHANNEL_INHERIT;
  GETCH(dyntopo_radius_scale)->flag |= BRUSH_CHANNEL_INHERIT;

  // GETCH(strength)->flag |= BRUSH_CHANNEL_INHERIT;
  GETCH(radius)->flag |= BRUSH_CHANNEL_INHERIT;

  ADDCH(area_radius_factor);

  switch (tool) {
    case SCULPT_TOOL_DRAW:
      break;
    case SCULPT_TOOL_SIMPLIFY: {
      BRUSHSET_LOOKUP(chset, strength)->mappings[BRUSH_MAPPING_PRESSURE].flag &=
          ~BRUSH_MAPPING_ENABLED;
      BRUSHSET_LOOKUP(chset, radius)->mappings[BRUSH_MAPPING_PRESSURE].flag &=
          ~BRUSH_MAPPING_ENABLED;

      BRUSHSET_SET_FLOAT(chset, strength, 0.5);
      BRUSHSET_SET_FLOAT(chset, autosmooth, 0.05);
      BRUSHSET_SET_INT(chset, topology_rake_mode, 1);  // curvature mode
      BRUSHSET_SET_FLOAT(chset, topology_rake, 0.5);

      BrushChannel *ch = BRUSHSET_LOOKUP(chset, dyntopo_mode);
      ch->flag &= ~BRUSH_CHANNEL_INHERIT;
      ch->flag |= BRUSH_CHANNEL_INHERIT_IF_UNSET;

      break;
    }
    case SCULPT_TOOL_DRAW_SHARP:
      BRUSHSET_LOOKUP(chset, spacing)->fvalue = 5;
      BRUSHSET_SET_INT(chset, direction, 1);
      BRUSHSET_LOOKUP(chset, radius)->mappings[BRUSH_MAPPING_PRESSURE].flag |=
          BRUSH_MAPPING_ENABLED;
      BRUSHSET_LOOKUP(chset, strength)->mappings[BRUSH_MAPPING_PRESSURE].flag &=
          ~BRUSH_MAPPING_ENABLED;
      break;
    case SCULPT_TOOL_DISPLACEMENT_ERASER:
    case SCULPT_TOOL_FAIRING:
    case SCULPT_TOOL_SCENE_PROJECT:
      GETCH(spacing)->fvalue = 10;
      GETCH(strength)->fvalue = 1.0f;
      GETCH(dyntopo_disabled)->ivalue = 1;
      BRUSHSET_SET_BOOL(chset, use_space_attenuation, false);
      break;
    case SCULPT_TOOL_SMEAR:
      BRUSHSET_SET_FLOAT(chset, spacing, 5.0f);
      BRUSHSET_SET_FLOAT(chset, strength, 1.0f);
      BRUSHSET_LOOKUP(chset, strength)->mappings[BRUSH_MAPPING_PRESSURE].flag &=
          ~BRUSH_MAPPING_ENABLED;
      BRUSHSET_SET_BOOL(chset, dyntopo_disabled, true);
      BRUSHSET_SET_BOOL(chset, use_space_attenuation, false);
      break;

    case SCULPT_TOOL_LAYER:
      BRUSHSET_SET_FLOAT(chset, height, 0.05f);
      BRUSHSET_SET_FLOAT(chset, hardness, 0.35f);
      BRUSHSET_SET_FLOAT(chset, strength, 1.0f);
      BRUSHSET_SET_BOOL(chset, use_space_attenuation, false);
      break;
    case SCULPT_TOOL_SLIDE_RELAX:
      GETCH(spacing)->fvalue = 10;
      GETCH(strength)->fvalue = 1.0f;
      GETCH(dyntopo_disabled)->ivalue = 1;
      GETCH(slide_deform_type)->ivalue = BRUSH_SLIDE_DEFORM_DRAG;
      break;
    case SCULPT_TOOL_PAINT: {
      BRUSHSET_SET_BOOL(chset, use_space_attenuation, false);
      BRUSHSET_SET_BOOL(chset, dyntopo_disabled, true);
      BRUSHSET_SET_FLOAT(chset, hardness, 0.4f);
      BRUSHSET_SET_FLOAT(chset, spacing, 10.0f);
      BRUSHSET_SET_FLOAT(chset, strength, 0.6f);

      BrushChannel *ch = BRUSHSET_LOOKUP(chset, hue_offset);
      BKE_brush_mapping_reset(ch, SCULPT_TOOL_PAINT, BRUSH_MAPPING_PRESSURE);
      break;
    }
    case SCULPT_TOOL_CLAY:
      GETCH(radius)->mappings[BRUSH_MAPPING_PRESSURE].flag |= BRUSH_MAPPING_ENABLED;
      GETCH(strength)->mappings[BRUSH_MAPPING_PRESSURE].flag |= BRUSH_MAPPING_ENABLED;

      GETCH(spacing)->fvalue = 3;
      GETCH(autosmooth)->fvalue = 0.25f;
      GETCH(normal_radius_factor)->fvalue = 0.75f;
      GETCH(hardness)->fvalue = 0.65;

      // ADDCH(autosmooth_falloff_curve);
      // GETCH(autosmooth_falloff_curve)->curve.preset = BRUSH_CURVE_SPHERE;

      BRUSHSET_SET_BOOL(chset, autosmooth_use_spacing, true);
      BRUSHSET_SET_FLOAT(chset, autosmooth_spacing, 7);

      reset_clay_mappings(chset, false);
      break;
    case SCULPT_TOOL_TWIST:
      GETCH(strength)->fvalue = 0.5f;
      GETCH(normal_radius_factor)->fvalue = 1.0f;
      GETCH(spacing)->fvalue = 6;
      GETCH(hardness)->fvalue = 0.5;
      BRUSHSET_SET_BOOL(chset, use_space_attenuation, false);
      break;
    case SCULPT_TOOL_CLAY_THUMB:
      BRUSHSET_SET_FLOAT(chset, strength, 1.0f);
      BRUSHSET_SET_FLOAT(chset, spacing, 6.0f);
      BRUSHSET_SET_FLOAT(chset, normal_radius_factor, 1.0f);
      BRUSHSET_SET_BOOL(chset, use_space_attenuation, false);
      BRUSHSET_LOOKUP(chset, radius)->mappings[BRUSH_MAPPING_PRESSURE].flag &=
          ~BRUSH_MAPPING_ENABLED;
      break;
    case SCULPT_TOOL_CLAY_STRIPS: {
      // GETCH(falloff_curve)->curve.preset = BRUSH_CURVE_SMOOTHER;

      GETCH(radius)->mappings[BRUSH_MAPPING_PRESSURE].flag |= BRUSH_MAPPING_ENABLED;
      GETCH(strength)->mappings[BRUSH_MAPPING_PRESSURE].flag |= BRUSH_MAPPING_ENABLED;

      GETCH(strength)->flag &= ~BRUSH_CHANNEL_INHERIT;

      BRUSHSET_SET_BOOL(chset, use_space_attenuation, false);

      GETCH(tip_roundness)->fvalue = 0.18f;
      GETCH(normal_radius_factor)->fvalue = 1.35f;
      GETCH(strength)->fvalue = 0.8f;
      GETCH(accumulate)->ivalue = 1;
      GETCH(spacing)->fvalue = 7.0f;

      BKE_brush_mapping_ensure_write(&GETCH(radius)->mappings[BRUSH_MAPPING_PRESSURE]);

      reset_clay_mappings(chset, true);

      break;
    }
    case SCULPT_TOOL_SMOOTH:
      BRUSHSET_SET_BOOL(chset, use_space_attenuation, false);
      BRUSHSET_SET_FLOAT(chset, spacing, 5.0f);
      BRUSHSET_SET_FLOAT(chset, strength, 0.3f);

      BRUSHSET_SET_BOOL(chset, dyntopo_disabled, true);

      ADDCH(surface_smooth_shape_preservation);
      ADDCH(surface_smooth_current_vertex);
      ADDCH(surface_smooth_iterations);
      break;
    case SCULPT_TOOL_THUMB:
      ADDCH(elastic_deform_type);
      ADDCH(elastic_deform_volume_preservation);
      BRUSHSET_SET_BOOL(chset, use_space_attenuation, false);
      BRUSHSET_LOOKUP(chset, strength)->mappings[BRUSH_MAPPING_PRESSURE].flag &=
          ~BRUSH_MAPPING_ENABLED;
      break;
    case SCULPT_TOOL_MULTIPLANE_SCRAPE:
      ADDCH(use_multiplane_scrape_dynamic);
      ADDCH(show_multiplane_scrape_planes_preview);

      GETCH(strength)->fvalue = 0.7f;
      GETCH(normal_radius_factor)->fvalue = 0.7f;

      ADDCH(multiplane_scrape_angle);
      GETCH(spacing)->fvalue = 5.0f;
      break;
    case SCULPT_TOOL_CREASE:
      GETCH(direction)->ivalue = true;
      GETCH(strength)->fvalue = 0.25f;
      GETCH(crease_pinch_factor)->fvalue = 0.5f;
      break;
    case SCULPT_TOOL_SCRAPE:
    case SCULPT_TOOL_FILL:
      GETCH(strength)->fvalue = 0.7f;
      GETCH(area_radius_factor)->fvalue = 0.5f;
      GETCH(spacing)->fvalue = 7.0f;
      ADDCH(invert_to_scrape_fill);
      GETCH(invert_to_scrape_fill)->ivalue = true;
      GETCH(accumulate)->ivalue = true;
      break;
    case SCULPT_TOOL_ROTATE:
      BRUSHSET_SET_BOOL(chset, use_space_attenuation, false);
      GETCH(strength)->fvalue = 1.0;
      GETCH(dyntopo_disabled)->ivalue = true;
      break;
    case SCULPT_TOOL_CLOTH:
      BRUSHSET_SET_BOOL(chset, cloth_pin_simulation_boundary, true);
      BRUSHSET_SET_BOOL(chset, use_space_attenuation, false);
      GETCH(radius)->mappings[BRUSH_MAPPING_PRESSURE].flag &= ~BRUSH_MAPPING_ENABLED;
      GETCH(strength)->mappings[BRUSH_MAPPING_PRESSURE].flag &= ~BRUSH_MAPPING_ENABLED;
      ADDCH(elastic_deform_type);
      ADDCH(elastic_deform_volume_preservation);
      break;
    case SCULPT_TOOL_ELASTIC_DEFORM:
      ADDCH(elastic_deform_type);
      ADDCH(elastic_deform_volume_preservation);
      GETCH(strength)->mappings[BRUSH_MAPPING_PRESSURE].flag &= ~BRUSH_MAPPING_ENABLED;
      BRUSHSET_SET_BOOL(chset, use_space_attenuation, false);
      break;
    case SCULPT_TOOL_GRAB:
      GETCH(strength)->mappings[BRUSH_MAPPING_PRESSURE].flag &= ~BRUSH_MAPPING_ENABLED;
      GETCH(strength)->fvalue = 0.4f;
      GETCH(radius)->fvalue = 75.0f;
      GETCH(dyntopo_disabled)->ivalue = 1;
      break;
    case SCULPT_TOOL_SNAKE_HOOK:
      GETCH(strength)->mappings[BRUSH_MAPPING_PRESSURE].flag &= ~BRUSH_MAPPING_ENABLED;

      GETCH(dyntopo_mode)->ivalue = DYNTOPO_LOCAL_COLLAPSE | DYNTOPO_SUBDIVIDE;
      GETCH(dyntopo_mode)->flag = BRUSH_CHANNEL_INHERIT_IF_UNSET;
      GETCH(strength)->fvalue = 1.0f;
      ADDCH(elastic_deform_type);
      ADDCH(elastic_deform_volume_preservation);

      break;
    case SCULPT_TOOL_DRAW_FACE_SETS:
      BRUSHSET_SET_FLOAT(chset, strength, 0.5f);
      BRUSHSET_LOOKUP(chset, radius)->flag &= ~BRUSH_CHANNEL_INHERIT;
      BRUSHSET_LOOKUP(chset, strength)->flag &= ~BRUSH_CHANNEL_INHERIT;
      BRUSHSET_LOOKUP(chset, strength)->mappings[BRUSH_MAPPING_PRESSURE].flag &=
          ~BRUSH_MAPPING_ENABLED;
      BRUSHSET_SET_BOOL(chset, use_space_attenuation, false);
      break;
    case SCULPT_TOOL_BOUNDARY:
      GETCH(strength)->mappings[BRUSH_MAPPING_PRESSURE].flag &= ~BRUSH_MAPPING_ENABLED;
      BRUSHSET_SET_BOOL(chset, use_space_attenuation, false);
      ADDCH(elastic_deform_type);
      ADDCH(elastic_deform_volume_preservation);
      break;
    case SCULPT_TOOL_POSE:
      GETCH(strength)->mappings[BRUSH_MAPPING_PRESSURE].flag &= ~BRUSH_MAPPING_ENABLED;
      BRUSHSET_SET_BOOL(chset, use_space_attenuation, false);
      ADDCH(elastic_deform_type);
      ADDCH(elastic_deform_volume_preservation);
      break;
    default: {
      // implement me!
      // BKE_brush_channelset_free(chset);
      // brush->channels = NULL;
      break;
    }
  }

  namestack_pop();

  BKE_brush_channelset_ui_init(brush, tool);
  BKE_brush_channelset_check_radius(chset);
}

void BKE_brush_init_toolsettings(Sculpt *sd)
{
  namestack_push(__func__);

  if (sd->channels) {
    BKE_brush_channelset_free(sd->channels);
  }

  sd->channels = BKE_brush_channelset_create("sd");

  BKE_brush_check_toolsettings(sd);
  BKE_brush_channelset_check_radius(sd->channels);

  namestack_pop();
}

// syncs radius and unprojected_radius's flags
void BKE_brush_channelset_check_radius(BrushChannelSet *chset)
{
  BrushChannel *ch1 = BRUSHSET_LOOKUP(chset, radius);
  BrushChannel *ch2 = BRUSHSET_LOOKUP(chset, unprojected_radius);

  if (!ch2) {
    return;
  }

  if (ch1->fvalue == 0.0 || ch2->fvalue == 0.0) {
    ch1->fvalue = 100.0f;
    ch2->fvalue = 0.1f;
  }

  int mask = BRUSH_CHANNEL_INHERIT | BRUSH_CHANNEL_INHERIT_IF_UNSET |
             /*BRUSH_CHANNEL_SHOW_IN_HEADER | BRUSH_CHANNEL_SHOW_IN_WORKSPACE |*/
             BRUSH_CHANNEL_UI_EXPANDED;

  if (ch2) {
    ch2->flag &= ~mask;
    ch2->flag |= mask & ch1->flag;
  }

  mask = BRUSH_MAPPING_ENABLED | BRUSH_MAPPING_INVERT | BRUSH_MAPPING_UI_EXPANDED;

  for (int i = 0; i < BRUSH_MAPPING_MAX; i++) {
    BrushMapping *mp1 = ch1->mappings + i;
    BrushMapping *mp2 = ch2->mappings + i;

    mp2->flag &= ~mask;
    mp2->flag |= mp1->flag & mask;
  }
}

void BKE_brush_check_toolsettings(Sculpt *sd)
{
  namestack_push(__func__);

  BrushChannelSet *chset = sd->channels;

  ADDCH(radius);
  ADDCH(strength);
  ADDCH(radius_unit);
  ADDCH(unprojected_radius);

  ADDCH(show_origco);
  ADDCH(save_temp_layers);

  ADDCH(smooth_strength_factor);
  ADDCH(smooth_strength_projection);

  ADDCH(tilt_strength_factor);
  ADDCH(automasking_boundary_edges_propagation_steps);
  ADDCH(concave_mask_factor);
  ADDCH(automasking);
  ADDCH(topology_rake_mode);

  ADDCH(plane_offset);
  ADDCH(plane_trim);

  ADDCH(autosmooth);
  ADDCH(autosmooth_projection);
  ADDCH(autosmooth_radius_scale);
  ADDCH(autosmooth_spacing);
  ADDCH(autosmooth_falloff_curve);

  ADDCH(topology_rake_radius_scale);
  ADDCH(topology_rake_projection);
  ADDCH(topology_rake_use_spacing);
  ADDCH(topology_rake_spacing);
  ADDCH(topology_rake);
  ADDCH(topology_rake_falloff_curve);

  ADDCH(vcol_boundary_exponent);
  ADDCH(vcol_boundary_factor);
  ADDCH(vcol_boundary_radius_scale);
  ADDCH(vcol_boundary_spacing);

  ADDCH(area_radius_factor);

  ADDCH(color);
  ADDCH(secondary_color);

  ADDCH(dyntopo_detail_mode);
  ADDCH(dyntopo_disabled);
  ADDCH(dyntopo_mode);
  ADDCH(dyntopo_detail_range);
  ADDCH(dyntopo_detail_percent);
  ADDCH(dyntopo_detail_size);
  ADDCH(dyntopo_constant_detail);
  ADDCH(dyntopo_spacing);
  ADDCH(dyntopo_radius_scale);

  namestack_pop();
}

void BKE_brush_tex_start(BrushTex *btex, BrushChannelSet *chset, BrushMappingData *mapdata)
{
  MTex *mtex = &btex->__mtex;

  // preserve ID pointers
  Object *ob = mtex->object;  // do I need to preserve this?
  Tex *tex = mtex->tex;

  // memset
  memset(mtex, 0, sizeof(*mtex));

  mtex->object = ob;
  mtex->tex = tex;

  float *color = &mtex->r;
  copy_v3_v3(color, BRUSHSET_LOOKUP(chset, mtex_color)->vector);
  copy_v3_v3(mtex->ofs, BRUSHSET_LOOKUP(chset, mtex_offset)->vector);
  copy_v3_v3(mtex->size, BRUSHSET_LOOKUP(chset, mtex_scale)->vector);

  mtex->brush_map_mode = BRUSHSET_GET_INT(chset, mtex_map_mode, mapdata);
  mtex->rot = BRUSHSET_GET_FLOAT(chset, mtex_angle, mapdata);

  if (BRUSHSET_GET_INT(chset, mtex_use_rake, mapdata)) {
    mtex->brush_angle_mode |= MTEX_ANGLE_RAKE;
  }

  if (BRUSHSET_GET_INT(chset, mtex_use_random, mapdata)) {
    mtex->brush_angle_mode |= MTEX_ANGLE_RANDOM;
  }
}

void BKE_brush_tex_from_mtex(BrushTex *btex, MTex *mtex)
{
  float *color = &mtex->r;
  BrushChannelSet *chset = btex->channels;

  copy_v3_v3(BRUSHSET_LOOKUP(chset, mtex_color)->vector, color);
  copy_v3_v3(BRUSHSET_LOOKUP(chset, mtex_offset)->vector, mtex->ofs);
  copy_v3_v3(BRUSHSET_LOOKUP(chset, mtex_scale)->vector, mtex->size);

  int map_mode = mtex->brush_map_mode & ~(MTEX_ANGLE_RAKE | MTEX_ANGLE_RANDOM);

  BRUSHSET_SET_INT(chset, mtex_map_mode, map_mode);
  BRUSHSET_SET_BOOL(chset, mtex_use_rake, mtex->brush_angle_mode & MTEX_ANGLE_RAKE);
  BRUSHSET_SET_BOOL(chset, mtex_use_random, mtex->brush_angle_mode & MTEX_ANGLE_RAKE);

  BRUSHSET_SET_FLOAT(chset, mtex_angle, mtex->rot);
}
void BKE_brush_tex_patch_channels(BrushTex *btex)
{
  BrushChannelSet *chset = btex->channels;

  ADDCH(mtex_offset);
  ADDCH(mtex_scale);
  ADDCH(mtex_color);
  ADDCH(mtex_map_mode);
  ADDCH(mtex_use_rake);
  ADDCH(mtex_use_random);
  ADDCH(mtex_random_angle);
  ADDCH(mtex_angle);
}

#define BRUSH_CHANNEL_DEFINE_TYPES
#include "brush_channel_define.h"

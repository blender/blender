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
#include "BLI_task.h"
#include "BLI_threads.h"

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
#define MAKE_FLOAT_EX_OPEN(idname1, name1, tooltip1, value1, min1, max1, smin1, smax1) \
  {.name = name1, \
   .idname = idname1, \
   .fvalue = value1,\
   .tooltip = tooltip1, \
   .min = min1,\
   .max = max1,\
   .soft_min = smin1,\
   .soft_max = smax1,\
   .type = BRUSH_CHANNEL_FLOAT

#define MAKE_FLOAT_EX(idname, name, tooltip, value, min, max, smin, smax) \
  MAKE_FLOAT_EX_OPEN(idname, name, tooltip, value, min, max, smin, smax) }

#define MAKE_FLOAT(idname, name, tooltip, value, min, max) MAKE_FLOAT_EX(idname, name, tooltip, value, min, max, min, max)

#define MAKE_INT_EX_OPEN(idname1, name1, tooltip1, value1, min1, max1, smin1, smax1) \
  {.name = name1, \
   .idname = idname1, \
   .tooltip = tooltip1, \
   .min = min1,\
   .max = max1,\
   .ivalue = value1,\
   .soft_min = smin1,\
   .soft_max = smax1,\
   .type = BRUSH_CHANNEL_INT

#define MAKE_INT_EX(idname, name, tooltip, value, min, max, smin, smax) \
  MAKE_INT_EX_OPEN(idname, name, tooltip, value, min, max, smin, smax) }

#define MAKE_INT(idname, name, tooltip, value, min, max) MAKE_INT_EX(idname, name, tooltip, value, min, max, min, max)

#define MAKE_BOOL_EX_OPEN(idname1, name1, tooltip1, value1)\
  {.name = name1, \
   .idname = idname1, \
   .tooltip = tooltip1, \
   .ivalue = value1,\
   .type = BRUSH_CHANNEL_BOOL

#define MAKE_BOOL(idname, name, tooltip, value)\
  MAKE_BOOL_EX_OPEN(idname, name, tooltip, value) }


/*
This is where all the builtin brush channels are defined.
That includes per-brush enums and bitflags!
*/

/* clang-format off */
BrushChannelType brush_builtin_channels[] = {
  {
    .name = "Radius",
    .idname = "radius",
    .min = 0.001f,
    .type = BRUSH_CHANNEL_FLOAT,
    .max = 2048.0f,
    .fvalue = 50.0f,
    .soft_min = 0.1f,
    .soft_max = 1024.0f,
    .mappings = {
        .pressure = {.curve = CURVE_PRESET_LINE, .factor = 1.0f, .min = 0.0f, .max = 1.0f, .enabled = false},
    }
  },

    {
    .name = "Strength",
    .idname = "strength",
    .min = -1.0f,
    .type = BRUSH_CHANNEL_FLOAT,
    .max = 4.0f,
    .fvalue = 0.5f,
    .soft_min = 0.0f,
    .soft_max = 1.0f,
    .mappings = {
        .pressure = {.curve = CURVE_PRESET_LINE, .factor = 1.0f, .min = 0.0f, .max = 1.0f, .enabled = false},
    }
  },
   {
    .name = "Alpha",
    .idname = "alpha",
    .type = BRUSH_CHANNEL_FLOAT,
    .min = 0.0f,
    .max = 1.0f,
    .fvalue = 1.0f,
    .soft_min = 0.0f,
    .soft_max = 1.0f,
    .mappings = {
        .pressure = {.curve = CURVE_PRESET_LINE, .factor = 1.0f, .min = 0.0f, .max = 1.0f, .enabled = false},
    }
   },
   {
    .name = "Spacing",
    .idname = "spacing",
    .min = 1.0f,
    .type = BRUSH_CHANNEL_INT,
    .max = 500.0f,
    .fvalue = 10.0f,
    .soft_min = 1.0f,
    .soft_max = 500.0f,
    .mappings = {
        .pressure = {.curve = CURVE_PRESET_LINE, .factor = 1.0, .min = 0.0f, .max = 1.0f, .enabled = false},
    }
  },
    {
    .name = "Autosmooth",
    .idname = "autosmooth",
    .type = BRUSH_CHANNEL_FLOAT,
    .min = -1.0f,
    .max = 4.0f,
    .soft_min = 0.0f,
    .soft_max = 1.0f,
    .mappings = {
        .pressure = {.curve = CURVE_PRESET_LINE, .factor = 1.0, .min = 0.0f, .max = 1.0f, .enabled = false, .inv = true},
    }
  },
    {
    .name = "Topology Rake",
    .idname = "topology_rake",
    .type = BRUSH_CHANNEL_FLOAT,
    .min = -1.0f,
    .max = 4.0f,
    .soft_min = 0.0f,
    .soft_max = 1.0f,
    .mappings = {
        .pressure = {.curve = CURVE_PRESET_LINE, .factor = 1.0, .min = 0.0f, .max = 1.0f, .enabled = false},
    }
  },
  {
    .name = "Autosmooth Radius Scale",
    .idname = "autosmooth_radius_scale",
    .type = BRUSH_CHANNEL_FLOAT,
    .min = 0.0001f,
    .max = 25.0f,
    .fvalue = 1.0f,
    .soft_min = 0.1f,
    .soft_max = 4.0f,
    .mappings = {
        .pressure = {.curve = CURVE_PRESET_LINE, .factor = 1.0, .min = 0.0f, .max = 1.0f, .enabled = false},
    }
  },
  {
    .name = "Rake Radius Scale",
    .idname = "topology_rake_radius_scale",
    .type = BRUSH_CHANNEL_FLOAT,
    .min = 0.0001f,
    .max = 25.0f,
    .fvalue = 1.0f,
    .soft_min = 0.1f,
    .soft_max = 4.0f,
    .mappings = {
        .pressure = {.curve = CURVE_PRESET_LINE, .factor = 1.0, .min = 0.0f, .max = 1.0f, .enabled = false},
    }
  },
  {
    .name = "Face Set Slide",
    .idname = "fset_slide",
    .type = BRUSH_CHANNEL_FLOAT,
    .min = 0.0001f,
    .max = 1.0f,
    .fvalue = 1.0f,
    .soft_min = 0.1f,
    .soft_max = 1.0f,
    .mappings = {
        .pressure = {.curve = CURVE_PRESET_LINE, .factor = 1.0, .min = 0.0f, .max = 1.0f, .enabled = false},
    }
  },
  {
    .name = "Boundary Smooth",
    .idname = "boundary_smooth",
    .type = BRUSH_CHANNEL_FLOAT,
    .min = 0.0001f,
    .max = 1.0f,
    .soft_min = 0.1f,
    .soft_max = 1.0f,
    .mappings = {
        .pressure = {.curve = CURVE_PRESET_LINE, .factor = 1.0, .min = 0.0f, .max = 1.0f, .enabled = false},
    }
  },
   {
    .name = "Projection",
    .idname = "projection",
    .type = BRUSH_CHANNEL_FLOAT,
    .min = 0.0001f,
    .max = 1.0f,
    .soft_min = 0.1f,
    .soft_max = 1.0f,
    .mappings = {
        .pressure = {.curve = CURVE_PRESET_LINE, .factor = 1.0, .min = 0.0f, .max = 1.0f, .enabled = false},
    }
  },
   {
     .name = "Use Spacing",
     .idname = "topology_rake_use_spacing",
     .type = BRUSH_CHANNEL_BOOL,
     .ivalue = 0
   },
   {
     .name = "Use Spacing",
     .idname = "autosmooth_use_spacing",
     .type = BRUSH_CHANNEL_BOOL,
     .ivalue = 0
   },
   {
    .name = "Projection",
    .idname = "autosmooth_projection",
    .type = BRUSH_CHANNEL_FLOAT,
    .min = 0.0001f,
    .max = 1.0f,
    .soft_min = 0.1f,
    .soft_max = 1.0f,
    .mappings = {
        .pressure = {.curve = CURVE_PRESET_LINE, .factor = 1.0, .min = 0.0f, .max = 1.0f, .enabled = false},
    }
  },
  {
    .name = "Projection",
    .idname = "topology_rake_projection",
    .type = BRUSH_CHANNEL_FLOAT,
    .min = 0.0001f,
    .max = 1.0f,
    .fvalue = 0.975f,
    .soft_min = 0.1f,
    .soft_max = 1.0f,
    .mappings = {
        .pressure = {.curve = CURVE_PRESET_LINE, .factor = 1.0, .min = 0.0f, .max = 1.0f, .enabled = false},
    }
  },
    {
    .name = "Spacing",
    .idname = "topology_rake_spacing",
    .type = BRUSH_CHANNEL_FLOAT,
    .min = 0.0001f,
    .max = 1.0f,
    .fvalue = 13.0f,
    .soft_min = 0.1f,
    .soft_max = 100.0f,
    .mappings = {
        .pressure = {.curve = CURVE_PRESET_LINE, .factor = 1.0, .min = 0.0f, .max = 1.0f, .enabled = false},
    }
  },
    {
    .name = "Spacing",
    .idname = "autosmooth_spacing",
    .type = BRUSH_CHANNEL_FLOAT,
    .min = 0.0001f,
    .max = 1.0f,
    .fvalue = 13.0f,
    .soft_min = 0.1f,
    .soft_max = 100.0f,
    .mappings = {
        .pressure = {.curve = CURVE_PRESET_LINE, .factor = 1.0, .min = 0.0f, .max = 1.0f, .enabled = false},
    }
  },
  
  {
    .name = "Topology Rake Mode",
    .idname = "topology_rake_mode",
    .type = BRUSH_CHANNEL_ENUM,
    .enumdef = {
      {0, "BRUSH_DIRECTION", ICON_NONE, "Stroke", "Stroke Direction"},
      {1, "CURVATURE", ICON_NONE, "Curvature", "Follow mesh curvature"},
      {-1}
    }
  },
   {
     .name = "Automasking",
     .idname = "automasking",
     .flag = BRUSH_CHANNEL_INHERIT_IF_UNSET | BRUSH_CHANNEL_INHERIT,
     .type = BRUSH_CHANNEL_BITMASK,
     .enumdef = {
         {BRUSH_AUTOMASKING_BOUNDARY_EDGES, "BOUNDARY_EDGE", ICON_NONE, "Boundary Edges", ""},
         {BRUSH_AUTOMASKING_BOUNDARY_FACE_SETS, "BOUNDARY_FACE_SETS", ICON_NONE, "Boundary Face Sets", ""},
         {BRUSH_AUTOMASKING_CONCAVITY, "CONCAVITY", ICON_NONE, "Concave", ""},
         {BRUSH_AUTOMASKING_INVERT_CONCAVITY, "INVERT_CONCAVITY", ICON_NONE, "Invert Concave", "Invert Concave Map"},
         {BRUSH_AUTOMASKING_FACE_SETS, "FACE_SETS", ICON_NONE, "Face Sets", ""},
         {BRUSH_AUTOMASKING_TOPOLOGY, "TOPOLOGY", ICON_NONE, "Topology", ""},
         {-1}
      }
   },
   {
     .name = "Disable Dyntopo",
     .idname = "dyntopo_disabled",
     .type = BRUSH_CHANNEL_BOOL,
     .flag = BRUSH_CHANNEL_NO_MAPPINGS,
     .ivalue = 0
   },
    {
      .name = "Operations",
      .idname = "dyntopo_mode",
      .type = BRUSH_CHANNEL_BITMASK,
      .flag = BRUSH_CHANNEL_INHERIT,
      .ivalue = DYNTOPO_COLLAPSE | DYNTOPO_CLEANUP | DYNTOPO_SUBDIVIDE,
      .enumdef = {
        {DYNTOPO_COLLAPSE, "COLLAPSE", ICON_NONE, "Collapse", ""},
        {DYNTOPO_SUBDIVIDE, "SUBDIVIDE", ICON_NONE, "Subdivide", ""},
        {DYNTOPO_CLEANUP, "CLEANUP", ICON_NONE, "Cleanup", ""},
        {DYNTOPO_LOCAL_COLLAPSE, "LOCAL_COLLAPSE", ICON_NONE, "Local Collapse", ""},
        {DYNTOPO_LOCAL_SUBDIVIDE, "LOCAL_SUBDIVIDE", ICON_NONE, "Local Subdivide", ""},
        {-1}
      }
    },
   {
     .name = "Slide Deform Type",
     .idname = "slide_deform_type",
     .ivalue = BRUSH_SLIDE_DEFORM_DRAG,
     .type = BRUSH_CHANNEL_ENUM,
     .enumdef = {
       {BRUSH_SLIDE_DEFORM_DRAG, "DRAG", ICON_NONE, "Drag", ""},
       {BRUSH_SLIDE_DEFORM_PINCH, "PINCH", ICON_NONE, "Pinch", ""},
       {BRUSH_SLIDE_DEFORM_EXPAND, "EXPAND", ICON_NONE, "Expand", ""},
       {-1}
      }
   },
   {
      .name = "Normal Radius",
      .idname = "normal_radius_factor",
      .tooltip = "Ratio between the brush radius and the radius that is going to be "
                            "used to sample the normal",
      .type = BRUSH_CHANNEL_FLOAT,
      .min = 0.0f,
      .max = 2.0f,
      .soft_min = 0.0f,
      .soft_max = 2.0f,
   },
   {
      .name = "Hardness",
      .idname = "hardness",
      .tooltip = "Brush hardness",
      .type = BRUSH_CHANNEL_FLOAT,
      .fvalue = 0.0f,
      .min = 0.0f,
      .max = 1.0f,
      .soft_min = 0.0f,
      .soft_max = 1.0f
   },
      {
      .name = "Tip Roundness",
      .idname = "tip_roundness",
      .tooltip = "",
      .type = BRUSH_CHANNEL_FLOAT,
      .fvalue = 0.0f,
      .min = 0.0f,
      .max = 1.0f,
      .soft_min = 0.0f,
      .soft_max = 1.0f
   },
   {
      .name = "Accumulate",
      .idname = "accumulate",
      .type = BRUSH_CHANNEL_BOOL,
      .ivalue = 0
   },
   { /*this one is weird, it's an enum pretending to be a bool,
     that was how it was in in original rna too*/

     .name = "Direction",
     .idname = "direction",
     .ivalue = 0,
     .type = BRUSH_CHANNEL_ENUM,
     .enumdef = {
        {0, "ADD", "ADD", "Add", "Add effect of brush"},
        {1, "SUBTRACT", "REMOVE", "Subtract", "Subtract effect of brush"},
        {-1}
     }
   },
    MAKE_FLOAT("normal_weight", "Normal Weight", "", 0.0f, 0.0f, 1.0f),
    MAKE_FLOAT("rake_factor", "Rake Factor",  "How much grab will follow cursor rotation", 0.0f, 0.0f, 10.0f),
    MAKE_FLOAT("weight", "Weight", "", 0.5f, 0.0f, 1.0f),
    MAKE_FLOAT("jitter", "Jitter",  "Jitter the position of the brush while painting", 0.0f, 0.0f, 1.0f),
    MAKE_INT("jitter_absolute", "Absolute Jitter", "", 0, 0.0f, 1000.0f),
    MAKE_FLOAT("smooth_stroke_radius", "Smooth Stroke Radius", "Minimum distance from last point before stroke continues", 10.0f, 10.0f, 200.0f),
    MAKE_FLOAT("smooth_stroke_factor", "Smooth Stroke Factor", "", 0.5f, 0.5f, 0.99f),
    MAKE_FLOAT_EX("rate", "Rate", "", 0.5, 0.0001f, 10000.0f, 0.01f, 1.0f),
    MAKE_FLOAT("flow", "Flow", "Amount of paint that is applied per stroke sample", 0.0f, 0.0f, 1.0f),
    MAKE_FLOAT("wet_mix", "Wet Mix", "Amount of paint that is picked from the surface into the brush color", 0.0f, 0.0f, 1.0f),
    MAKE_FLOAT("wet_persistence", "Wet Persistence", "Amount of wet paint that stays in the brush after applying paint to the surface", 0.0f, 0.0f, 1.0f),
    MAKE_FLOAT("density", "Density", "Amount of random elements that are going to be affected by the brush", 0.0f, 0.0f, 1.0f),
    MAKE_FLOAT("tip_scale_x", "Tip Scale X", "Scale of the brush tip in the X axis", 0.0f, 0.0f, 1.0f),
    MAKE_FLOAT("dash_ratio", "Dash Ratio", "Ratio of samples in a cycle that the brush is enabled", 0.0f, 0.0f, 1.0f),
    MAKE_FLOAT_EX("plane_offset", "Plane Offset", "Adjust plane on which the brush acts towards or away from the object surface", 0.0f, -2.0f, 2.0f, -0.5f, 0.5f),
    MAKE_BOOL("original_normal", "Original Normal", "When locked keep using normal of surface where stroke was initiated", false),
    MAKE_BOOL("original_plane", "Original Plane", "When locked keep using the plane origin of surface where stroke was initiated", false),
    MAKE_BOOL("use_weighted_smooth", "Weight By Area", "Weight by face area to get a smoother result", true),
    MAKE_BOOL("preserve_faceset_boundary", "Keep FSet Boundary", "Preserve face set boundaries", true),
    MAKE_BOOL("hard_edge_mode", "Hard Edge Mode", "Forces all brushes into hard edge face set mode (sets face set slide to 0)", false),
    MAKE_BOOL("grab_silhouette", "Grab Silhouette", "Grabs trying to automask the silhouette of the object", false),
    MAKE_FLOAT("dyntopo_detail_percent", "Detail Percent", "Detail Percent", 25.0f, 0.0f, 1000.0f),
    MAKE_FLOAT("dyntopo_detail_range", "Detail Range", "Detail Range", 0.45f, 0.01f, 0.99f),
    MAKE_FLOAT_EX("dyntopo_detail_size", "Detail Size", "Detail Size", 8.0f, 0.1f, 100.0f, 0.001f, 500.0f),
    MAKE_FLOAT_EX("dyntopo_constant_detail", "Constaint Detail", "", 3.0f, 0.001f, 1000.0f, 0.0001, FLT_MAX),
    MAKE_FLOAT_EX("dyntopo_spacing", "Spacing", "Dyntopo Spacing", 35.0f, 0.01f, 300.0f, 0.001f, 50000.0f),
    MAKE_FLOAT_EX("dyntopo_radius_scale", "Radius Scale", "Scale brush radius for dyntopo radius", 1.0f, 0.001f, 3.0f, 0.0001f, 100.0f)
};

/* clang-format on */
const int brush_builtin_channel_len = ARRAY_SIZE(brush_builtin_channels);

static bool do_builtin_init = true;
ATTR_NO_OPT static bool check_builtin_init()
{
  if (!do_builtin_init || !BLI_thread_is_main()) {
    return false;
  }

  do_builtin_init = false;

  // can't do this here, since we can't lookup icon ids in blenkernel
  // for (int i = 0; i < brush_builtin_channel_len; i++) {
  // BKE_brush_channeltype_rna_check(brush_builtin_channels + i);
  //}

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

#define FLOAT BRUSH_CHANNEL_FLOAT
#define INT BRUSH_CHANNEL_INT
#define BOOL BRUSH_CHANNEL_BOOL
#define FLOAT3 BRUSH_CHANNEL_VEC3
#define FLOAT4 BRUSH_CHANNEL_VEC4

#ifdef ADDCH
#  undef ADDCH
#endif
#ifdef GETCH
#  undef GETCH
#endif

#define ADDCH(name) \
  (BKE_brush_channelset_ensure_builtin(chset, name), BKE_brush_channelset_lookup(chset, name))
#define GETCH(name) BKE_brush_channelset_lookup(chset, name)

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

/* This lookup table is used convert data to/from brush channels
   and the old settings fields in Brush*/
static BrushSettingsMap brush_settings_map[] = {
  DEF(size, radius, INT, FLOAT)
  DEF(alpha, strength, FLOAT, FLOAT)
  DEF(autosmooth_factor, autosmooth, FLOAT, FLOAT)
  DEF(autosmooth_projection, SMOOTH_PROJECTION, FLOAT, FLOAT)
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
  DEF(jitter, jitter, FLOAT, FLOAT)
  DEF(jitter_absolute, JITTER_ABSOLITE, INT, INT)
  DEF(smooth_stroke_radius, smooth_stroke_radius, INT, FLOAT)
  DEF(smooth_stroke_factor, smooth_stroke_factor, FLOAT, FLOAT)
  DEF(rate, rate, FLOAT, FLOAT)
  DEF(flow, flow, FLOAT, FLOAT)
  DEF(wet_mix, wet_mix, FLOAT, FLOAT)
  DEF(wet_persistence, wet_persistence, FLOAT, FLOAT)
  DEF(density, density, FLOAT, FLOAT)
  DEF(tip_scale_x, tip_scale_x, FLOAT, FLOAT)
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
};
static const int dyntopo_settings_map_len = ARRAY_SIZE(dyntopo_settings_map);

/* clang-format on */

typedef struct BrushFlagMap {
  int member_offset;
  char *channel_name;
  int flag;
  int member_size;
} BrushFlagMap;

/* clang-format off */
#define DEF(member, channel, flag)\
  {offsetof(Brush, member), #channel, flag, sizeof(((Brush){0}).member)},

/* This lookup table is like brush_settings_map except it converts
   individual bitflags instead of whole struct members.*/
BrushFlagMap brush_flags_map[] =  {
  DEF(flag, direction, BRUSH_DIR_IN)
  DEF(flag, original_normal, BRUSH_ORIGINAL_NORMAL)
  DEF(flag, original_plane, BRUSH_ORIGINAL_PLANE)
  DEF(flag, accumulate, BRUSH_ACCUMULATE)
  DEF(flag2, use_weighted_smooth, BRUSH_SMOOTH_USE_AREA_WEIGHT)
  DEF(flag2, preserve_faceset_boundary, BRUSH_SMOOTH_PRESERVE_FACE_SETS)
  DEF(flag2, hard_edge_mode, BRUSH_HARD_EDGE_MODE)
  DEF(flag2, grab_silhouette, BRUSH_GRAB_SILHOUETTE)
};
int brush_flags_map_len = ARRAY_SIZE(brush_flags_map);

/* clang-format on */

static ATTR_NO_OPT void do_coerce(
    int type1, void *ptr1, int size1, int type2, void *ptr2, int size2)
{
  double val = 0;

  switch (type1) {
    case BRUSH_CHANNEL_FLOAT:
      val = *(float *)ptr1;
      break;
    case BRUSH_CHANNEL_INT:
    case BRUSH_CHANNEL_ENUM:
    case BRUSH_CHANNEL_BITMASK:
    case BRUSH_CHANNEL_BOOL:
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
  }

  switch (type2) {
    case BRUSH_CHANNEL_FLOAT:
      *(float *)ptr2 = (float)val;
      break;
    case BRUSH_CHANNEL_INT:
    case BRUSH_CHANNEL_ENUM:
    case BRUSH_CHANNEL_BITMASK:
    case BRUSH_CHANNEL_BOOL: {
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
  }
}

void *get_channel_value_pointer(BrushChannel *ch, int *r_data_size)
{
  *r_data_size = 4;

  switch (ch->type) {
    case BRUSH_CHANNEL_FLOAT:
      return &ch->fvalue;
    case BRUSH_CHANNEL_INT:
    case BRUSH_CHANNEL_ENUM:
    case BRUSH_CHANNEL_BITMASK:
    case BRUSH_CHANNEL_BOOL:
      return &ch->ivalue;
    case BRUSH_CHANNEL_VEC3:
      *r_data_size = sizeof(float) * 3;
      printf("implement me!\n");
      return NULL;
    case BRUSH_CHANNEL_VEC4:
      *r_data_size = sizeof(float) * 4;
      printf("implement me!\n");
      return NULL;
  }

  return NULL;
}

ATTR_NO_OPT static void brush_flags_from_channels(BrushChannelSet *chset, Brush *brush)
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
        if (ch->ivalue) {
          *f |= mf->flag;
        }
        else {
          *f &= ~mf->flag;
        }
        break;
      }
      case 2: {
        ushort *f = (ushort *)ptr;
        if (ch->ivalue) {
          *f |= mf->flag;
        }
        else {
          *f &= ~mf->flag;
        }
        break;
      }
      case 4: {
        uint *f = (uint *)ptr;
        if (ch->ivalue) {
          *f |= mf->flag;
        }
        else {
          *f &= ~mf->flag;
        }
        break;
      }
      case 8: {
        uint64_t *f = (uint64_t *)ptr;
        if (ch->ivalue) {
          *f |= mf->flag;
        }
        else {
          *f &= ~mf->flag;
        }
        break;
      }
    }
  }
}

ATTR_NO_OPT static void brush_flags_to_channels(BrushChannelSet *chset, Brush *brush)
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
        ch->ivalue = (*f & mf->flag) ? 1 : 0;
        break;
      }
      case 2: {
        ushort *f = (ushort *)ptr;
        ch->ivalue = (*f & mf->flag) ? 1 : 0;
        break;
      }
      case 4: {
        uint *f = (uint *)ptr;
        ch->ivalue = (*f & mf->flag) ? 1 : 0;
        break;
      }
      case 8: {
        uint64_t *f = (uint64_t *)ptr;
        ch->ivalue = (*f & mf->flag) ? 1 : 0;
        break;
      }
    }
  }
}

ATTR_NO_OPT void BKE_brush_channelset_compat_load_intern(BrushChannelSet *chset,
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

ATTR_NO_OPT void BKE_brush_channelset_compat_load(BrushChannelSet *chset,
                                                  Brush *brush,
                                                  bool brush_to_channels)
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
        chset, "dyntopo_mode", brush->dyntopo.flag & DYNTOPO_DISABLED ? 1 : 0);

#define SETCH(key, val) BKE_brush_channelset_set_int(chset, #key, val)
    SETCH(dyntopo_detail_range, brush->dyntopo.detail_range);
    SETCH(dyntopo_detail_percent, brush->dyntopo.detail_percent);
    SETCH(dyntopo_detail_size, brush->dyntopo.detail_size);
    SETCH(dyntopo_constant_detail, brush->dyntopo.constant_detail);
    SETCH(dyntopo_spacing, brush->dyntopo.spacing);
  }
}
#undef SETCH

// adds any missing channels to brushes
void BKE_brush_builtin_patch(Brush *brush, int tool)
{
  check_builtin_init();

  namestack_push(__func__);

  if (!brush->channels) {
    brush->channels = BKE_brush_channelset_create();
  }

  BrushChannelSet *chset = brush->channels;

  ADDCH("radius");
  ADDCH("spacing");
  ADDCH("strength");

  ADDCH("autosmooth");
  ADDCH("autosmooth_radius_scale");
  ADDCH("autosmooth_spacing");
  ADDCH("autosmooth_use_spacing");
  ADDCH("autosmooth_projection");

  ADDCH("topology_rake");
  ADDCH("topology_rake_mode");
  ADDCH("topology_rake_radius_scale");
  ADDCH("topology_rake_use_spacing");
  ADDCH("topology_rake_spacing");
  ADDCH("topology_rake_projection");

  ADDCH("hardness");
  ADDCH("tip_roundness");
  ADDCH("normal_radius_factor");

  ADDCH("automasking");

  ADDCH("dyntopo_disabled");
  ADDCH("dyntopo_mode")->flag |= BRUSH_CHANNEL_INHERIT;
  ADDCH("dyntopo_detail_range")->flag |= BRUSH_CHANNEL_INHERIT;
  ADDCH("dyntopo_detail_percent")->flag |= BRUSH_CHANNEL_INHERIT;
  ADDCH("dyntopo_detail_size")->flag |= BRUSH_CHANNEL_INHERIT;
  ADDCH("dyntopo_constant_detail")->flag |= BRUSH_CHANNEL_INHERIT;
  ADDCH("dyntopo_spacing")->flag |= BRUSH_CHANNEL_INHERIT;
  ADDCH("dyntopo_radius_scale")->flag |= BRUSH_CHANNEL_INHERIT;

  ADDCH("accumulate");
  ADDCH("original_normal");
  ADDCH("original_plane");
  ADDCH("jitter");
  ADDCH("jitter_absolute");
  ADDCH("use_weighted_smooth");
  ADDCH("preserve_faceset_boundary");
  ADDCH("hard_edge_mode");
  ADDCH("grab_silhouette");

  ADDCH("projection");
  ADDCH("boundary_smooth");
  ADDCH("fset_slide");

  ADDCH("direction");

  switch (tool) {
    case SCULPT_TOOL_DRAW: {
      break;
    }
    case SCULPT_TOOL_SLIDE_RELAX:
      ADDCH("slide_deform_type");
      break;
  }

  namestack_pop();
}

ATTR_NO_OPT void BKE_brush_builtin_create(Brush *brush, int tool)
{
  namestack_push(__func__);

  if (!brush->channels) {
    brush->channels = BKE_brush_channelset_create();
  }

  BrushChannelSet *chset = brush->channels;

  BKE_brush_builtin_patch(brush, tool);

  GETCH("strength")->flag |= BRUSH_CHANNEL_INHERIT;
  GETCH("radius")->flag |= BRUSH_CHANNEL_INHERIT;

  switch (tool) {
    case SCULPT_TOOL_DRAW: {
      break;
    }
    case SCULPT_TOOL_DRAW_SHARP:
      GETCH("spacing")->ivalue = 5;
      GETCH("radius")->mappings[BRUSH_MAPPING_PRESSURE].blendmode = true;
      break;
    case SCULPT_TOOL_DISPLACEMENT_ERASER:
    case SCULPT_TOOL_FAIRING:
    case SCULPT_TOOL_SCENE_PROJECT:
      GETCH("spacing")->ivalue = 10;
      GETCH("strength")->fvalue = 1.0f;
      GETCH("dyntopo_disabled")->ivalue = 1;
      break;
    case SCULPT_TOOL_SLIDE_RELAX:
      GETCH("spacing")->ivalue = 10;
      GETCH("strength")->fvalue = 1.0f;
      GETCH("dyntopo_disabled")->ivalue = 1;
      GETCH("slide_deform_type")->ivalue = BRUSH_SLIDE_DEFORM_DRAG;
      break;
    case SCULPT_TOOL_CLAY:
      GETCH("radius")->mappings[BRUSH_MAPPING_PRESSURE].flag |= BRUSH_MAPPING_ENABLED;
      GETCH("spacing")->ivalue = 3;
      GETCH("autosmooth")->fvalue = 0.25f;
      GETCH("normal_radius_factor")->fvalue = 0.75f;
      GETCH("hardness")->fvalue = 0.65;
      break;
    case SCULPT_TOOL_TWIST:
      GETCH("strength")->fvalue = 0.5f;
      GETCH("normal_radius_factor")->fvalue = 1.0f;
      GETCH("spacing")->ivalue = 6;
      GETCH("hardness")->fvalue = 0.5;
      break;
    case SCULPT_TOOL_CLAY_STRIPS: {
      GETCH("radius")->mappings[BRUSH_MAPPING_PRESSURE].flag |= BRUSH_MAPPING_ENABLED;
      GETCH("tip_roundness")->fvalue = 0.18f;
      GETCH("normal_radius_factor")->fvalue = 1.35f;
      GETCH("strength")->fvalue = 0.8f;
      GETCH("accumulate")->ivalue = 1;

      BKE_brush_mapping_ensure_write(&GETCH("radius")->mappings[BRUSH_MAPPING_PRESSURE]);

      CurveMapping *curve = GETCH("radius")->mappings[BRUSH_MAPPING_PRESSURE].curve;

      CurveMap *cuma = curve->cm;

      cuma->curve[0].x = 0.0f;
      cuma->curve[0].y = 0.55f;
      BKE_curvemap_insert(cuma, 0.5f, 0.7f);
      cuma->curve[2].x = 1.0f;
      cuma->curve[2].y = 1.0f;
      BKE_curvemapping_changed(curve, true);

      cuma = curve->cm;
      BKE_curvemap_insert(cuma, 0.6f, 0.25f);
      BKE_curvemapping_changed(curve, true);

      break;
    }
    default: {
      // implement me!
      // BKE_brush_channelset_free(chset);
      // brush->channels = NULL;
      break;
    }
  }

  namestack_pop();
}

void BKE_brush_init_toolsettings(Sculpt *sd)
{
  namestack_push(__func__);

  if (sd->channels) {
    BKE_brush_channelset_free(sd->channels);
  }

  BKE_brush_check_toolsettings(sd);

  namestack_pop();
}

void BKE_brush_check_toolsettings(Sculpt *sd)
{
  namestack_push(__func__);

  BrushChannelSet *chset = sd->channels;

  ADDCH("radius");
  ADDCH("strength");
  ADDCH("automasking");
  ADDCH("topology_rake_mode");

  ADDCH("dyntopo_disabled");
  ADDCH("dyntopo_mode");
  ADDCH("dyntopo_detail_range");
  ADDCH("dyntopo_detail_percent");
  ADDCH("dyntopo_detail_size");
  ADDCH("dyntopo_constant_detail");
  ADDCH("dyntopo_spacing");
  ADDCH("dyntopo_radius_scale");

  namestack_pop();
}

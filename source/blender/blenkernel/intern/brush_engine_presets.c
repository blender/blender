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

/*
Instructions to add a built-in channel:

1. Add to brush_builtin_channels
2. Add to BKE_brush_builtin_patch to insert it in old brushes (without converting data)

To enable converting to/from old data:
3. If not a boolean mapping to a bitflag: Add to brush_settings_map
4. If a boolean mapping to a bitflag: Add to brush_flags_map_len.
*/
#define ICON_NONE -1

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
    .idname = "RADIUS",
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
    .idname = "STRENGTH",
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
    .idname = "ALPHA",
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
    .idname = "SPACING",
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
    .idname = "AUTOSMOOTH",
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
    .idname = "TOPOLOGY_RAKE",
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
    .idname = "AUTOSMOOTH_RADIUS_SCALE",
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
    .idname = "TOPOLOGY_RAKE_RADIUS_SCALE",
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
    .idname = "FSET_SLIDE",
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
    .idname = "BOUNDARY_SMOOTH",
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
    .idname = "PROJECTION",
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
     .idname = "TOPOLOGY_RAKE_USE_SPACING",
     .type = BRUSH_CHANNEL_BOOL,
     .ivalue = 0
   },
   {
     .name = "Use Spacing",
     .idname = "AUTOSMOOTH_USE_SPACING",
     .type = BRUSH_CHANNEL_BOOL,
     .ivalue = 0
   },
   {
    .name = "Projection",
    .idname = "AUTOSMOOTH_PROJECTION",
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
    .idname = "TOPOLOGY_RAKE_PROJECTION",
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
    .idname = "TOPOLOGY_RAKE_SPACING",
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
    .idname = "AUTOSMOOTH_SPACING",
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
    .idname = "TOPOLOGY_RAKE_MODE",
    .type = BRUSH_CHANNEL_ENUM,
    .enumdef = {
      {0, "BRUSH_DIRECTION", ICON_NONE, "Stroke", "Stroke Direction"},
      {1, "CURVATURE", ICON_NONE, "Curvature", "Follow mesh curvature"},
      {-1, 0}
    }
  },
   {
     .name = "Automasking",
     .idname = "AUTOMASKING",
     .flag = BRUSH_CHANNEL_INHERIT_IF_UNSET | BRUSH_CHANNEL_INHERIT,
     .type = BRUSH_CHANNEL_BITMASK,
     .enumdef = {
         {BRUSH_AUTOMASKING_BOUNDARY_EDGES, "BOUNDARY_EDGE", ICON_NONE, "Boundary Edges", ""},
         {BRUSH_AUTOMASKING_BOUNDARY_FACE_SETS, "BOUNDARY_FACE_SETS", ICON_NONE, "Boundary Face Sets", ""},
         {BRUSH_AUTOMASKING_CONCAVITY, "CONCAVITY", ICON_NONE, "Concave", ""},
         {BRUSH_AUTOMASKING_INVERT_CONCAVITY, "INVERT_CONCAVITY", ICON_NONE, "Invert Concave", "Invert Concave Map"},
         {BRUSH_AUTOMASKING_FACE_SETS, "FACE_SETS", ICON_NONE, "Face Sets", ""},
         {BRUSH_AUTOMASKING_TOPOLOGY, "TOPOLOGY", ICON_NONE, "Topology", ""}
      }
   },
   {
     .name = "Disable Dyntopo",
     .idname = "DYNTOPO_DISABLED",
     .type = BRUSH_CHANNEL_BOOL,
     .flag = BRUSH_CHANNEL_NO_MAPPINGS,
     .ivalue = 0
   },
   {
     .name = "Detail Range",
     .idname = "DYNTOPO_DETAIL_RANGE",
     .type = BRUSH_CHANNEL_FLOAT,
     .min = 0.001,
     .max = 0.99,
     .flag = BRUSH_CHANNEL_INHERIT,
     .ivalue = 0
   },
    {
      .name = "Operations",
      .idname = "DYNTOPO_OPS",
      .type = BRUSH_CHANNEL_BITMASK,
      .flag = BRUSH_CHANNEL_INHERIT,
      .ivalue = DYNTOPO_COLLAPSE | DYNTOPO_CLEANUP | DYNTOPO_SUBDIVIDE,
      .enumdef = {
        {DYNTOPO_COLLAPSE, "COLLAPSE", ICON_NONE, "Collapse", ""},
        {DYNTOPO_SUBDIVIDE, "SUBDIVIDE", ICON_NONE, "Subdivide", ""},
        {DYNTOPO_CLEANUP, "CLEANUP", ICON_NONE, "Cleanup", ""},
        {DYNTOPO_LOCAL_COLLAPSE, "LOCAL_COLLAPSE", ICON_NONE, "Local Collapse", ""},
        {DYNTOPO_LOCAL_SUBDIVIDE, "LOCAL_SUBDIVIDE", ICON_NONE, "Local Subdivide", ""},
        {-1, NULL, -1, NULL, NULL}
      }
    },
   {
     .name = "Slide Deform Type",
     .idname = "SLIDE_DEFORM_TYPE",
     .ivalue = BRUSH_SLIDE_DEFORM_DRAG,
     .type = BRUSH_CHANNEL_ENUM,
     .enumdef = {
       {BRUSH_SLIDE_DEFORM_DRAG, "DRAG", ICON_NONE, "Drag", ""},
       {BRUSH_SLIDE_DEFORM_PINCH, "PINCH", ICON_NONE, "Pinch", ""},
       {BRUSH_SLIDE_DEFORM_EXPAND, "EXPAND", ICON_NONE, "Expand", ""},
       {-1, NULL, -1, NULL}
      }
   },
   {
      .name = "Normal Radius",
      .idname = "NORMAL_RADIUS_FACTOR",
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
      .idname = "HARDNESS",
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
      .idname = "TIP_ROUNDNESS",
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
      .idname = "ACCUMULATE",
      .type = BRUSH_CHANNEL_BOOL,
      .ivalue = 0
   },
    MAKE_FLOAT("NORMAL_WEIGHT", "Normal Weight", "", 0.0f, 0.0f, 1.0f),
    MAKE_FLOAT("RAKE_FACTOR", "Rake Factor",  "How much grab will follow cursor rotation", 0.0f, 0.0f, 10.0f),
    MAKE_FLOAT("WEIGHT", "Weight", "", 0.5f, 0.0f, 1.0f),
    MAKE_FLOAT("JITTER", "Jitter",  "Jitter the position of the brush while painting", 0.0f, 0.0f, 1.0f),
    MAKE_INT("JITTER_ABSOLUTE", "Absolute Jitter", "", 0, 0.0f, 1000.0f),
    MAKE_FLOAT("SMOOTH_STROKE_RADIUS", "Smooth Stroke Radius", "Minimum distance from last point before stroke continues", 10.0f, 10.0f, 200.0f),
    MAKE_FLOAT("SMOOTH_STROKE_FACTOR", "Smooth Stroke Factor", "", 0.5f, 0.5f, 0.99f),
    MAKE_FLOAT_EX("RATE", "Rate", "", 0.5, 0.0001f, 10000.0f, 0.01f, 1.0f),
    MAKE_FLOAT("FLOW", "Flow", "Amount of paint that is applied per stroke sample", 0.0f, 0.0f, 1.0f),
    MAKE_FLOAT("WET_MIX", "Wet Mix", "Amount of paint that is picked from the surface into the brush color", 0.0f, 0.0f, 1.0f),
    MAKE_FLOAT("WET_PERSISTENCE", "Wet Persistence", "Amount of wet paint that stays in the brush after applying paint to the surface", 0.0f, 0.0f, 1.0f),
    MAKE_FLOAT("DENSITY", "Density", "Amount of random elements that are going to be affected by the brush", 0.0f, 0.0f, 1.0f),
    MAKE_FLOAT("TIP_SCALE_X", "Tip Scale X", "Scale of the brush tip in the X axis", 0.0f, 0.0f, 1.0f),
    MAKE_FLOAT("DASH_RATIO", "Dash Ratio", "Ratio of samples in a cycle that the brush is enabled", 0.0f, 0.0f, 1.0f),
    MAKE_FLOAT_EX("PLANE_OFFSET", "Plane Offset", "Adjust plane on which the brush acts towards or away from the object surface", 0.0f, -2.0f, 2.0f, -0.5f, 0.5f),
    MAKE_BOOL("ORIGINAL_NORMAL", "Original Normal", "When locked keep using normal of surface where stroke was initiated", false),
    MAKE_BOOL("ORIGINAL_PLANE", "Original Plane", "When locked keep using the plane origin of surface where stroke was initiated", false),
    MAKE_BOOL("USE_WEIGHTED_SMOOTH", "Weight By Area", "Weight by face area to get a smoother result", true),
    MAKE_BOOL("PRESERVE_FACESET_BOUNDARY", "Keep FSet Boundary", "Preserve face set boundaries", true),
    MAKE_BOOL("HARD_EDGE_MODE", "Hard Edge Mode", "Forces all brushes into hard edge face set mode (sets face set slide to 0)", false),
    MAKE_BOOL("GRAB_SILHOUETTE", "Grab Silhouette", "Grabs trying to automask the silhouette of the object", false),
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

  for (int i = 0; i < brush_builtin_channel_len; i++) {
    BKE_brush_channeltype_rna_check(brush_builtin_channels + i);
  }

  return true;
}

void namestack_push(const char *name);
void namestack_pop();
#define namestack_head_name strdup(namestack[namestack_i].tag)

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

#define ADDCH(name) BKE_brush_channelset_ensure_builtin(chset, name)
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
  DEF(size, RADIUS, INT, FLOAT)
  DEF(alpha, STRENGTH, FLOAT, FLOAT)
  DEF(autosmooth_factor, AUTOSMOOTH, FLOAT, FLOAT)
  DEF(autosmooth_projection, SMOOTH_PROJECTION, FLOAT, FLOAT)
  DEF(topology_rake_projection, TOPOLOGY_RAKE_PROJECTION, FLOAT, FLOAT)
  DEF(topology_rake_radius_factor, TOPOLOGY_RAKE_RADIUS_SCALE, FLOAT, FLOAT)
  DEF(topology_rake_spacing, TOPOLOGY_RAKE_SPACING, INT, FLOAT)
  DEF(topology_rake_factor, TOPOLOGY_RAKE, FLOAT, FLOAT)
  DEF(autosmooth_fset_slide, FSET_SLIDE, FLOAT, FLOAT)
  DEF(boundary_smooth_factor, BOUNDARY_SMOOTH, FLOAT, FLOAT)
  DEF(autosmooth_radius_factor, AUTOSMOOTH_RADIUS_SCALE, FLOAT, FLOAT)
  DEF(normal_weight, NORMAL_WEIGHT, FLOAT, FLOAT)
  DEF(rake_factor, RAKE_FACTOR, FLOAT, FLOAT)
  DEF(weight, WEIGHT, FLOAT, FLOAT)
  DEF(jitter, JITTER, FLOAT, FLOAT)
  DEF(jitter_absolute, JITTER_ABSOLITE, INT, INT)
  DEF(smooth_stroke_radius, SMOOTH_STROKE_RADIUS, INT, FLOAT)
  DEF(smooth_stroke_factor, SMOOTH_STROKE_FACTOR, FLOAT, FLOAT)
  DEF(rate, RATE, FLOAT, FLOAT)
  DEF(flow, FLOW, FLOAT, FLOAT)
  DEF(wet_mix, WET_MIX, FLOAT, FLOAT)
  DEF(wet_persistence, WET_PERSISTENCE, FLOAT, FLOAT)
  DEF(density, DENSITY, FLOAT, FLOAT)
  DEF(tip_scale_x, TIP_SCALE_X, FLOAT, FLOAT)
};
static const int brush_settings_map_len = ARRAY_SIZE(brush_settings_map);

/* clang-format on */
#undef DEF

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
  DEF(flag, ORIGINAL_NORMAL, BRUSH_ORIGINAL_NORMAL)
  DEF(flag, ORIGINAL_PLANE, BRUSH_ORIGINAL_PLANE)
  DEF(flag, ACCUMULATE, BRUSH_ACCUMULATE)
  DEF(flag2, USE_WEIGHTED_SMOOTH, BRUSH_SMOOTH_USE_AREA_WEIGHT)
  DEF(flag2, PRESERVE_FACESET_BOUNDARY, BRUSH_SMOOTH_PRESERVE_FACE_SETS)
  DEF(flag2, HARD_EDGE_MODE, BRUSH_HARD_EDGE_MODE)
  DEF(flag2, GRAB_SILHOUETTE, BRUSH_GRAB_SILHOUETTE)
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

ATTR_NO_OPT void BKE_brush_channelset_compat_load(BrushChannelSet *chset,
                                                  Brush *brush,
                                                  bool brush_to_channels)
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

  for (int i = 0; i < brush_settings_map_len; i++) {
    BrushSettingsMap *mp = brush_settings_map + i;
    BrushChannel *ch = BKE_brush_channelset_lookup(chset, mp->channel_name);

    if (!ch) {
      continue;
    }

    char *bptr = (char *)brush;
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

// adds any missing channels to brushes
void BKE_brush_builtin_patch(Brush *brush, int tool)
{
  check_builtin_init();

  namestack_push(__func__);

  if (!brush->channels) {
    brush->channels = BKE_brush_channelset_create();
  }

  BrushChannelSet *chset = brush->channels;

  ADDCH("RADIUS");
  ADDCH("SPACING");
  ADDCH("STRENGTH");

  ADDCH("AUTOSMOOTH");
  ADDCH("AUTOSMOOTH_RADIUS_SCALE");
  ADDCH("AUTOSMOOTH_SPACING");
  ADDCH("AUTOSMOOTH_USE_SPACING");
  ADDCH("AUTOSMOOTH_PROJECTION");

  ADDCH("TOPOLOGY_RAKE");
  ADDCH("TOPOLOGY_RAKE_MODE");
  ADDCH("TOPOLOGY_RAKE_RADIUS_SCALE");
  ADDCH("TOPOLOGY_RAKE_USE_SPACING");
  ADDCH("TOPOLOGY_RAKE_SPACING");
  ADDCH("TOPOLOGY_RAKE_PROJECTION");

  ADDCH("HARDNESS");
  ADDCH("TIP_ROUNDNESS");
  ADDCH("NORMAL_RADIUS_FACTOR");

  ADDCH("AUTOMASKING");

  ADDCH("DYNTOPO_DISABLED");
  ADDCH("DYNTOPO_DETAIL_RANGE");
  ADDCH("DYNTOPO_OPS");

  ADDCH("ACCUMULATE");
  ADDCH("ORIGINAL_NORMAL");
  ADDCH("ORIGINAL_PLANE");
  ADDCH("JITTER");
  ADDCH("JITTER_ABSOLUTE");
  ADDCH("USE_WEIGHTED_SMOOTH");
  ADDCH("PRESERVE_FACESET_BOUNDARY");
  ADDCH("HARD_EDGE_MODE");
  ADDCH("GRAB_SILHOUETTE");

  ADDCH("PROJECTION");
  ADDCH("BOUNDARY_SMOOTH");
  ADDCH("FSET_SLIDE");

  switch (tool) {
    case SCULPT_TOOL_DRAW: {
      break;
    }
    case SCULPT_TOOL_SLIDE_RELAX:
      ADDCH("SLIDE_DEFORM_TYPE");
      break;
  }

  namestack_pop();
}

void BKE_brush_init_scene_defaults(Sculpt *sd)
{
  if (!sd->channels) {
    sd->channels = BKE_brush_channelset_create();
  }

  BrushChannelSet *chset = sd->channels;
}

void BKE_brush_builtin_create(Brush *brush, int tool)
{
  namestack_push(__func__);

  if (!brush->channels) {
    brush->channels = BKE_brush_channelset_create();
  }

  BrushChannelSet *chset = brush->channels;

  BKE_brush_builtin_patch(brush, tool);

  GETCH("STRENGTH")->flag |= BRUSH_CHANNEL_INHERIT;
  GETCH("RADIUS")->flag |= BRUSH_CHANNEL_INHERIT;

  switch (tool) {
    case SCULPT_TOOL_DRAW: {
      break;
    }
    case SCULPT_TOOL_DRAW_SHARP:
      GETCH("SPACING")->ivalue = 5;
      GETCH("RADIUS")->mappings[BRUSH_MAPPING_PRESSURE].blendmode = true;
      break;
    case SCULPT_TOOL_DISPLACEMENT_ERASER:
    case SCULPT_TOOL_FAIRING:
    case SCULPT_TOOL_SCENE_PROJECT:
      GETCH("SPACING")->ivalue = 10;
      GETCH("STRENGTH")->fvalue = 1.0f;
      GETCH("DYNTOPO_DISABLED")->ivalue = 1;
      break;
    case SCULPT_TOOL_SLIDE_RELAX:
      GETCH("SPACING")->ivalue = 10;
      GETCH("STRENGTH")->fvalue = 1.0f;
      GETCH("DYNTOPO_DISABLED")->ivalue = 1;
      GETCH("SLIDE_DEFORM_TYPE")->ivalue = BRUSH_SLIDE_DEFORM_DRAG;
      break;
    case SCULPT_TOOL_CLAY:
      GETCH("RADIUS")->mappings[BRUSH_MAPPING_PRESSURE].flag |= BRUSH_MAPPING_ENABLED;
      GETCH("SPACING")->ivalue = 3;
      GETCH("AUTOSMOOTH")->fvalue = 0.25f;
      GETCH("NORMAL_RADIUS_FACTOR")->fvalue = 0.75f;
      GETCH("HARDNESS")->fvalue = 0.65;
      break;
    case SCULPT_TOOL_TWIST:
      GETCH("STRENGTH")->fvalue = 0.5f;
      GETCH("NORMAL_RADIUS_FACTOR")->fvalue = 1.0f;
      GETCH("SPACING")->ivalue = 6;
      GETCH("HARDNESS")->fvalue = 0.5;
      break;
    case SCULPT_TOOL_CLAY_STRIPS: {
      GETCH("RADIUS")->mappings[BRUSH_MAPPING_PRESSURE].flag |= BRUSH_MAPPING_ENABLED;
      GETCH("TIP_ROUNDNESS")->fvalue = 0.18f;
      GETCH("NORMAL_RADIUS_FACTOR")->fvalue = 1.35f;
      GETCH("STRENGTH")->fvalue = 0.8f;
      GETCH("ACCUMULATE")->ivalue = 1;

      CurveMapping *curve = &GETCH("RADIUS")->mappings[BRUSH_MAPPING_PRESSURE].curve;
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

  BrushChannelSet *chset = sd->channels = BKE_brush_channelset_create();

  ADDCH("RADIUS");
  ADDCH("STRENGTH");
  ADDCH("AUTOMASKING");
  ADDCH("TOPOLOGY_RAKE_MODE");
  ADDCH("DYNTOPO_DISABLED");
  ADDCH("DYNTOPO_DETAIL_RANGE");

  namestack_pop();
}

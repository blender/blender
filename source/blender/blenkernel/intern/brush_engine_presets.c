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
#include "BKE_curveprofile.h"

#include "BLO_read_write.h"

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

/* clang-format on */

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
    .enumdef = {.items = {
      {0, "BRUSH_DIRECTION", ICON_NONE, "Stroke", "Stroke Direction"},
      {1, "CURVATURE", ICON_NONE, "Curvature", "Follow mesh curvature"},
      {-1, 0}
    }}
  },
   {
     .name = "Automasking",
     .idname = "AUTOMASKING",
     .flag = BRUSH_CHANNEL_INHERIT_IF_UNSET | BRUSH_CHANNEL_INHERIT,
     .type = BRUSH_CHANNEL_BITMASK,
     .enumdef = {.items = {
         {BRUSH_AUTOMASKING_BOUNDARY_EDGES, "BOUNDARY_EDGE", ICON_NONE, "Boundary Edges", ""},
         {BRUSH_AUTOMASKING_BOUNDARY_FACE_SETS, "BOUNDARY_FACE_SETS", ICON_NONE, "Boundary Face Sets", ""},
         {BRUSH_AUTOMASKING_CONCAVITY, "CONCAVITY", ICON_NONE, "Concave", ""},
         {BRUSH_AUTOMASKING_INVERT_CONCAVITY, "INVERT_CONCAVITY", ICON_NONE, "Invert Concave", "Invert Concave Map"},
         {BRUSH_AUTOMASKING_FACE_SETS, "FACE_SETS", ICON_NONE, "Face Sets", ""},
         {BRUSH_AUTOMASKING_TOPOLOGY, "TOPOLOGY", ICON_NONE, "Topology", ""}
      }}
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
    MAKE_BOOL("ORIGINAL_PLANE", "Original Plane", "When locked keep using the plane origin of surface where stroke was initiated", false)
};

/* clang-format on */
const int brush_builtin_channel_len = ARRAY_SIZE(brush_builtin_channels);

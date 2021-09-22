#pragma once

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
#define MAKE_FLOAT_EX_EX(idname1, name1, tooltip1, value1, min1, max1, smin1, smax1, pressure_enabled, pressure_inv) \
  {.name = name1, \
   .idname = #idname1, \
   .fvalue = value1,\
   .tooltip = tooltip1, \
   .min = min1,\
   .max = max1,\
   .soft_min = smin1,\
   .soft_max = smax1,\
   .type = BRUSH_CHANNEL_FLOAT,\
   .mappings = {\
        .pressure = {.curve = CURVE_PRESET_LINE, .factor = 1.0f, .min = 0.0f, .max = 1.0f, .enabled = pressure_enabled, .inv = pressure_inv},\
    }\
},
#define MAKE_FLOAT_EX(idname1, name1, tooltip1, value1, min1, max1, smin1, smax1, pressure_enabled)\
MAKE_FLOAT_EX_EX(idname1, name1, tooltip1, value1, min1, max1, smin1, smax1, pressure_enabled, false)
#define MAKE_FLOAT_EX_INV(idname1, name1, tooltip1, value1, min1, max1, smin1, smax1, pressure_enabled)\
MAKE_FLOAT_EX_EX(idname1, name1, tooltip1, value1, min1, max1, smin1, smax1, pressure_enabled, true)

#define MAKE_FLOAT(idname, name, tooltip, value, min, max) MAKE_FLOAT_EX(idname, name, tooltip, value, min, max, min, max, false)

#define MAKE_COLOR3(idname1, name1, tooltip1, r, g, b){\
  .idname = #idname1, \
  .name = name1, \
  .tooltip = tooltip1, \
  .type = BRUSH_CHANNEL_VEC3,\
  .vector = {r, g, b, 1.0f},\
  .min = 0.0f, .max = 5.0f,\
  .soft_min = 0.0f, .soft_max = 1.0f,\
  .flag = BRUSH_CHANNEL_COLOR,\
},

#define MAKE_COLOR4(idname1, name1, tooltip1, r, g, b, a){\
  .idname = #idname1, \
  .name = name1, \
  .tooltip = tooltip1, \
  .type = BRUSH_CHANNEL_VEC4,\
  .vector = {r, g, b, a},\
  .min = 0.0f, .max = 5.0f,\
  .soft_min = 0.0f, .soft_max = 1.0f,\
  .flag = BRUSH_CHANNEL_COLOR,\
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
   .type = BRUSH_CHANNEL_INT

#define MAKE_INT_EX(idname, name, tooltip, value, min, max, smin, smax) \
  MAKE_INT_EX_OPEN(idname, name, tooltip, value, min, max, smin, smax) },

#define MAKE_INT(idname, name, tooltip, value, min, max) MAKE_INT_EX(idname, name, tooltip, value, min, max, min, max)

#define MAKE_BOOL_EX(idname1, name1, tooltip1, value1, flag1)\
  {.name = name1, \
   .idname = #idname1, \
   .tooltip = tooltip1, \
   .ivalue = value1,\
   .flag = flag1,\
   .type = BRUSH_CHANNEL_BOOL\
  },

#define MAKE_BOOL(idname, name, tooltip, value)\
  MAKE_BOOL_EX(idname, name, tooltip, value, 0)

#define MAKE_ENUM_EX(idname1, name1, tooltip1, value1, enumdef1, flag1)\
{.name = name1,\
 .idname = #idname1,\
 .tooltip = tooltip1,\
 .ivalue = value1,\
 .flag = flag1,\
 .type = BRUSH_CHANNEL_ENUM,\
 .rna_enumdef = NULL,\
 .enumdef = enumdef1\
},
#define MAKE_FLAGS_EX(idname1, name1, tooltip1, value1, enumdef1, flag1)\
{.name = name1,\
 .idname = #idname1,\
 .tooltip = tooltip1,\
 .ivalue = value1,\
 .flag = flag1,\
 .type = BRUSH_CHANNEL_BITMASK,\
 .rna_enumdef = NULL,\
 .enumdef = enumdef1\
},
#define MAKE_FLAGS(idname1, name1, tooltip1, value1, enumdef1) MAKE_FLAGS_EX(idname1, name1, tooltip1, value1, enumdef1, 0) 
#define MAKE_ENUM(idname1, name1, tooltip1, value1, enumdef1) MAKE_ENUM_EX(idname1, name1, tooltip1, value1, enumdef1, 0) 

/*
This is where all the builtin brush channels are defined.
That includes per-brush enums and bitflags!
*/

/* clang-format off */

BrushChannelType brush_builtin_channels[] = {    
#include "brush_channel_define.h"
};

//BRUSH_INVERT_TO_SCRAPE_FILL
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
  (BKE_brush_channelset_ensure_builtin(chset, BRUSH_BUILTIN_##name), \
   BKE_brush_channelset_lookup(chset, BRUSH_BUILTIN_##name))
#define GETCH(name) BKE_brush_channelset_lookup(chset, BRUSH_BUILTIN_##name)

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
  DEF(area_radius_factor, area_radius_factor, FLOAT, FLOAT)
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
  DEF(multiplane_scrape_angle, multiplane_scrape_angle, FLOAT, FLOAT)
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
  DEF(concave_mask_factor, concave_mask_factor, FLOAT, FLOAT)
  DEF(automasking_boundary_edges_propagation_steps, automasking_boundary_edges_propagation_steps, INT, INT)
  DEF(add_col, cursor_color_add, FLOAT4, FLOAT4)
  DEF(sub_col, cursor_color_sub, FLOAT4, FLOAT4)
  DEF(rgb, color, FLOAT3, FLOAT3)
  DEF(secondary_rgb, secondary_color, FLOAT3, FLOAT3)
  DEF(vcol_boundary_factor, vcol_boundary_factor, FLOAT, FLOAT)
  DEF(vcol_boundary_exponent, vcol_boundary_exponent, FLOAT, FLOAT)
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
#ifdef DEF
#undef DEF
#endif

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
  DEF(flag, invert_to_scrape_fill, BRUSH_INVERT_TO_SCRAPE_FILL)
  DEF(flag2, use_multiplane_scrape_dynamic, BRUSH_MULTIPLANE_SCRAPE_DYNAMIC)
  DEF(flag2, show_multiplane_scrape_planes_preview, BRUSH_MULTIPLANE_SCRAPE_PLANES_PREVIEW)
};

int brush_flags_map_len = ARRAY_SIZE(brush_flags_map);

/* clang-format on */

static ATTR_NO_OPT void do_coerce(
    int type1, void *ptr1, int size1, int type2, void *ptr2, int size2)
{
  double val = 0;
  float vec[4];

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
    case BRUSH_CHANNEL_VEC3:
      copy_v3_v3(vec, (float *)ptr1);
      break;
    case BRUSH_CHANNEL_VEC4:
      copy_v4_v4(vec, (float *)ptr1);
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
    case BRUSH_CHANNEL_VEC3:
      copy_v3_v3((float *)ptr2, vec);
      break;
    case BRUSH_CHANNEL_VEC4:
      copy_v4_v4((float *)ptr2, vec);
      break;
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
      return ch->vector;
    case BRUSH_CHANNEL_VEC4:
      *r_data_size = sizeof(float) * 4;
      return ch->vector;
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

#define SETCH(key, val) BKE_brush_channelset_set_float(chset, #key, val)
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

  ADDCH(radius);
  ADDCH(spacing);
  ADDCH(strength);

  ADDCH(autosmooth);
  ADDCH(autosmooth_radius_scale);
  ADDCH(autosmooth_spacing);
  ADDCH(autosmooth_use_spacing);
  ADDCH(autosmooth_projection);

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

  ADDCH(hardness);
  ADDCH(tip_roundness);
  ADDCH(normal_radius_factor);

  ADDCH(automasking);
  ADDCH(automasking_boundary_edges_propagation_steps);
  ADDCH(concave_mask_factor);
  ADDCH(dyntopo_disabled);
  ADDCH(dyntopo_mode)->flag |= BRUSH_CHANNEL_INHERIT;
  ADDCH(dyntopo_detail_range)->flag |= BRUSH_CHANNEL_INHERIT;
  ADDCH(dyntopo_detail_percent)->flag |= BRUSH_CHANNEL_INHERIT;
  ADDCH(dyntopo_detail_size)->flag |= BRUSH_CHANNEL_INHERIT;
  ADDCH(dyntopo_constant_detail)->flag |= BRUSH_CHANNEL_INHERIT;
  ADDCH(dyntopo_spacing)->flag |= BRUSH_CHANNEL_INHERIT;
  ADDCH(dyntopo_radius_scale)->flag |= BRUSH_CHANNEL_INHERIT;

  ADDCH(accumulate);
  ADDCH(original_normal);
  ADDCH(original_plane);
  ADDCH(jitter);
  ADDCH(jitter_absolute);
  ADDCH(use_weighted_smooth);
  ADDCH(preserve_faceset_boundary);
  ADDCH(hard_edge_mode);
  ADDCH(grab_silhouette);

  ADDCH(projection);
  ADDCH(boundary_smooth);
  ADDCH(fset_slide);

  ADDCH(direction);

  switch (tool) {
    case SCULPT_TOOL_DRAW: {
      break;
    }

    case SCULPT_TOOL_PAINT: {
      ADDCH(color);
      ADDCH(secondary_color);
      break;
    }
    case SCULPT_TOOL_SLIDE_RELAX:
      ADDCH(slide_deform_type);
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

  GETCH(strength)->flag |= BRUSH_CHANNEL_INHERIT;
  GETCH(radius)->flag |= BRUSH_CHANNEL_INHERIT;

  ADDCH(area_radius_factor);

  switch (tool) {
    case SCULPT_TOOL_DRAW: {
      break;
    }
    case SCULPT_TOOL_SIMPLIFY:
      GETCH(strength)->mappings[BRUSH_MAPPING_PRESSURE].flag &= ~BRUSH_MAPPING_ENABLED;
      GETCH(radius)->mappings[BRUSH_MAPPING_PRESSURE].flag &= ~BRUSH_MAPPING_ENABLED;
      GETCH(strength)->fvalue = 0.5;
      GETCH(autosmooth)->fvalue = 0.05;
      GETCH(topology_rake_mode)->ivalue = 1;  // curvature mode
      GETCH(topology_rake)->fvalue = 0.35;
      break;
    case SCULPT_TOOL_DRAW_SHARP:
      GETCH(spacing)->ivalue = 5;
      GETCH(radius)->mappings[BRUSH_MAPPING_PRESSURE].flag |= BRUSH_MAPPING_ENABLED;
      GETCH(strength)->mappings[BRUSH_MAPPING_PRESSURE].flag &= ~BRUSH_MAPPING_ENABLED;
      break;
    case SCULPT_TOOL_DISPLACEMENT_ERASER:
    case SCULPT_TOOL_FAIRING:
    case SCULPT_TOOL_SCENE_PROJECT:
      GETCH(spacing)->ivalue = 10;
      GETCH(strength)->fvalue = 1.0f;
      GETCH(dyntopo_disabled)->ivalue = 1;
      break;
    case SCULPT_TOOL_SLIDE_RELAX:
      GETCH(spacing)->ivalue = 10;
      GETCH(strength)->fvalue = 1.0f;
      GETCH(dyntopo_disabled)->ivalue = 1;
      GETCH(slide_deform_type)->ivalue = BRUSH_SLIDE_DEFORM_DRAG;
      break;
    case SCULPT_TOOL_CLAY:
      GETCH(radius)->mappings[BRUSH_MAPPING_PRESSURE].flag |= BRUSH_MAPPING_ENABLED;
      GETCH(spacing)->ivalue = 3;
      GETCH(autosmooth)->fvalue = 0.25f;
      GETCH(normal_radius_factor)->fvalue = 0.75f;
      GETCH(hardness)->fvalue = 0.65;
      break;
    case SCULPT_TOOL_TWIST:
      GETCH(strength)->fvalue = 0.5f;
      GETCH(normal_radius_factor)->fvalue = 1.0f;
      GETCH(spacing)->ivalue = 6;
      GETCH(hardness)->fvalue = 0.5;
      break;
    case SCULPT_TOOL_CLAY_STRIPS: {
      GETCH(radius)->mappings[BRUSH_MAPPING_PRESSURE].flag |= BRUSH_MAPPING_ENABLED;
      GETCH(tip_roundness)->fvalue = 0.18f;
      GETCH(normal_radius_factor)->fvalue = 1.35f;
      GETCH(strength)->fvalue = 0.8f;
      GETCH(accumulate)->ivalue = 1;

      BKE_brush_mapping_ensure_write(&GETCH(radius)->mappings[BRUSH_MAPPING_PRESSURE]);

      CurveMapping *curve = GETCH(radius)->mappings[BRUSH_MAPPING_PRESSURE].curve;

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
      GETCH(strength)->fvalue = 0.25;
      break;
    case SCULPT_TOOL_SCRAPE:
    case SCULPT_TOOL_FILL:
      GETCH(strength)->fvalue = 0.7f;
      GETCH(area_radius_factor)->fvalue = 0.5f;
      GETCH(spacing)->fvalue = 7;
      ADDCH(invert_to_scrape_fill);
      GETCH(invert_to_scrape_fill)->ivalue = true;
      GETCH(accumulate)->ivalue = true;
      break;
    case SCULPT_TOOL_ROTATE:
      GETCH(strength)->fvalue = 1.0;
      GETCH(dyntopo_disabled)->ivalue = true;
      break;
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

  ADDCH(radius);
  ADDCH(strength);
  ADDCH(automasking_boundary_edges_propagation_steps);
  ADDCH(concave_mask_factor);
  ADDCH(automasking);
  ADDCH(topology_rake_mode);

  ADDCH(vcol_boundary_exponent);
  ADDCH(vcol_boundary_factor);
  ADDCH(vcol_boundary_radius_scale);
  ADDCH(vcol_boundary_spacing);

  ADDCH(area_radius_factor);

  ADDCH(color);
  ADDCH(secondary_color);

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

#define BRUSH_CHANNEL_DEFINE_TYPES
#include "brush_channel_define.h"

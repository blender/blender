#include "MEM_guardedalloc.h"

#include "DNA_brush_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"
#include "BKE_scene.h"
#include "BKE_screen.h"

#include "DEG_depsgraph.h"
#include "ED_view3d.h"

#include "BLI_array.h"
#include "BLI_buffer.h"
#include "BLI_compiler_attrs.h"
#include "BLI_compiler_compat.h"
#include "BLI_dynstr.h"
#include "BLI_ghash.h"
#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_mempool.h"
#include "BLI_rand.h"
#include "BLI_smallhash.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "PIL_time.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_paint.h"
#include "BKE_pbvh.h"

#include "paint_intern.h"
#include "sculpt_intern.h"

#include "WM_api.h"
#include "WM_types.h"

#include "bmesh.h"
#include <string.h>

typedef struct SculptBrushSample {
  Sculpt sd;  // copy of sd settings

  float active_vertex_co[3];
  float active_face_co[3];

  bool have_active_vertex;
  bool have_active_face;

  StrokeCache cache;
  UnifiedPaintSettings ups;
  PaintStroke stroke;

  double time;
} SculptBrushSample;

typedef struct SculptReplayLog {
  SculptBrushSample *samples;
  int totsample, samples_size;
  Tex **textures;
  int tot_textures, textures_size;
  MemArena *arena;
  SmallHash texmap;

  bool is_playing;
} SculptReplayLog;

static SculptReplayLog *current_log = NULL;

void SCULPT_replay_log_free(SculptReplayLog *log)
{
  MEM_SAFE_FREE(log->samples);
  MEM_SAFE_FREE(log->textures);

  BLI_smallhash_release(&log->texmap);
  BLI_memarena_free(log->arena);
  MEM_freeN(log);
}

SculptReplayLog *SCULPT_replay_log_create()
{
  SculptReplayLog *log = MEM_callocN(sizeof(*log), "SculptReplayLog");

  log->arena = BLI_memarena_new(1024, __func__);
  BLI_smallhash_init(&log->texmap);

  return log;
}

void SCULPT_replay_log_end()
{
  if (!current_log) {
    printf("could not find log!");
    return;
  }

  SCULPT_replay_log_free(current_log);
  current_log = NULL;
}
void SCULPT_replay_log_start()
{
  if (current_log) {
    printf("%s: recording has already started. . .\n", __func__);
    return;
  }

  current_log = MEM_callocN(sizeof(*current_log), "sculpt replay log");
  current_log->arena = BLI_memarena_new(8192, "sculpt replay log");
}

#if 0
#  define WRITE(key, fmt, ...) \
    { \
      char _buf[256], _prefix[64]; \
      if (shead >= 0) { \
        sprintf(_prefix, "%s%s%s", stack[shead].prefix, stack[shead].op, key); \
      } \
      else { \
        sprintf(_prefix, "%s", key); \
      } \
      sprintf(_buf, "%s " fmt "\n", _prefix, __VA_ARGS__); \
      BLI_dynstr_append(out, _buf); \
    } \
    ((void *)0)

#  define STACK_PUSH(key, memberop) \
    shead++; \
    sprintf(stack[shead].prefix, "%s", key); \
    stack[shead].op = memberop

#  define STACK_POP() shead--
#endif

enum {
  REPLAY_FLOAT,
  REPLAY_INT,
  REPLAY_VEC2,
  REPLAY_VEC3,
  REPLAY_VEC4,
  REPLAY_STRUCT,
  REPLAY_STRUCT_PTR,
  REPLAY_BOOL,
  REPLAY_BYTE,
  REPLAY_SHORT,
};

struct ReplaySerialStruct;
typedef struct ReplaySerialDef {
  char name[32];
  int type;  //-1 is used for sentinal ending member list
  int struct_offset;
  struct ReplaySerialStruct *sdef;
} ReplaySerialDef;

typedef struct ReplaySerialStruct {
  char name[32];
  ReplaySerialDef *members;
} ReplaySerialStruct;

#ifdef DEF
#  undef DEF
#endif

/* clang-format off */
#define DEF(key, type, structtype, ...) {#key, type, offsetof(structtype, key), __VA_ARGS__}

static ReplaySerialDef dyntopo_def[] = {
    {"detail_range", REPLAY_FLOAT, offsetof(DynTopoSettings, detail_range)},
    {"detail_percent", REPLAY_FLOAT, offsetof(DynTopoSettings, detail_percent)},
    {"detail_size", REPLAY_FLOAT, offsetof(DynTopoSettings, detail_size)},
    {"constant_detail", REPLAY_FLOAT, offsetof(DynTopoSettings, constant_detail)},
    {"flag", REPLAY_SHORT, offsetof(DynTopoSettings, flag)},
    {"mode", REPLAY_SHORT, offsetof(DynTopoSettings, mode)},
    {"inherit", REPLAY_INT, offsetof(DynTopoSettings, inherit)},
    {"spacing", REPLAY_INT, offsetof(DynTopoSettings, spacing)},
    {"", -1, -1}};
static ReplaySerialStruct DynTopoSettingsDef = {"DynTopoSettings", dyntopo_def};

static ReplaySerialDef paint_stroke_def[] = {
  DEF(last_mouse_position, REPLAY_VEC2, PaintStroke),
  DEF(last_world_space_position, REPLAY_VEC3, PaintStroke),
  DEF(stroke_over_mesh, REPLAY_BOOL, PaintStroke),
  DEF(stroke_distance, REPLAY_FLOAT, PaintStroke),
  DEF(stroke_distance_t, REPLAY_FLOAT, PaintStroke),
  DEF(stroke_started, REPLAY_BOOL, PaintStroke),
  DEF(rake_started, REPLAY_BOOL, PaintStroke),
  DEF(event_type, REPLAY_INT, PaintStroke),
  DEF(stroke_init, REPLAY_BOOL, PaintStroke),
  DEF(brush_init, REPLAY_BOOL, PaintStroke),
  DEF(initial_mouse, REPLAY_VEC2, PaintStroke),
  DEF(cached_size_pressure, REPLAY_FLOAT, PaintStroke),
  DEF(last_pressure, REPLAY_FLOAT, PaintStroke),
  DEF(stroke_mode, REPLAY_INT, PaintStroke),
  DEF(last_tablet_event_pressure, REPLAY_FLOAT, PaintStroke),
  DEF(pen_flip, REPLAY_INT, PaintStroke),
  DEF(x_tilt, REPLAY_FLOAT, PaintStroke),
  DEF(y_tilt, REPLAY_FLOAT, PaintStroke),
  DEF(spacing, REPLAY_FLOAT, PaintStroke),
  DEF(constrain_line, REPLAY_BOOL, PaintStroke),
  DEF(constrained_pos, REPLAY_VEC2, PaintStroke),
  {"", -1, -1}
};

static ReplaySerialStruct PaintStrokeDef = {"PaintStroke", paint_stroke_def};

static ReplaySerialDef brush_def[] = {
    DEF(weight, REPLAY_FLOAT, Brush),
    DEF(size, REPLAY_INT, Brush),
    DEF(dyntopo, REPLAY_STRUCT, Brush, &DynTopoSettingsDef),
    DEF(flag, REPLAY_INT, Brush),
    DEF(flag2, REPLAY_INT, Brush),
    DEF(automasking_flags, REPLAY_INT, Brush),
    DEF(normal_radius_factor, REPLAY_FLOAT, Brush),
    DEF(area_radius_factor, REPLAY_FLOAT, Brush),
    DEF(wet_paint_radius_factor, REPLAY_FLOAT, Brush),
    DEF(plane_trim, REPLAY_FLOAT, Brush),
    DEF(height, REPLAY_FLOAT, Brush),
    DEF(vcol_boundary_factor, REPLAY_FLOAT, Brush),
    DEF(vcol_boundary_exponent, REPLAY_FLOAT, Brush),
    DEF(topology_rake_factor, REPLAY_FLOAT, Brush),
    DEF(topology_rake_radius_factor, REPLAY_FLOAT, Brush),
    DEF(topology_rake_projection, REPLAY_FLOAT, Brush),
    DEF(topology_rake_spacing, REPLAY_FLOAT, Brush),
    DEF(tilt_strength_factor, REPLAY_FLOAT, Brush),
    DEF(autosmooth_factor, REPLAY_FLOAT, Brush),
    DEF(tilt_strength_factor, REPLAY_FLOAT, Brush),
    DEF(autosmooth_radius_factor, REPLAY_FLOAT, Brush),
    DEF(autosmooth_projection, REPLAY_FLOAT, Brush),
    DEF(autosmooth_spacing, REPLAY_FLOAT, Brush),
    DEF(boundary_smooth_factor, REPLAY_FLOAT, Brush),
    DEF(hard_corner_pin, REPLAY_FLOAT, Brush),
    DEF(sculpt_tool, REPLAY_BYTE, Brush),
    DEF(falloff_shape, REPLAY_BYTE, Brush),
    DEF(falloff_angle, REPLAY_FLOAT, Brush),
    DEF(paint_flags, REPLAY_INT, Brush),
    DEF(density, REPLAY_FLOAT, Brush),
    DEF(wet_persistence, REPLAY_FLOAT, Brush),
    DEF(wet_mix, REPLAY_FLOAT, Brush),
    DEF(flow, REPLAY_FLOAT, Brush),
    DEF(hardness, REPLAY_FLOAT, Brush),
    DEF(alpha, REPLAY_FLOAT, Brush),
    DEF(rgb, REPLAY_VEC3, Brush),
    DEF(rate, REPLAY_FLOAT, Brush),
    DEF(smooth_stroke_factor, REPLAY_FLOAT, Brush),
    DEF(smooth_stroke_radius, REPLAY_INT, Brush),
    DEF(spacing, REPLAY_INT, Brush),
    DEF(overlay_flags, REPLAY_INT, Brush),
    DEF(mask_pressure, REPLAY_INT, Brush),
    DEF(jitter, REPLAY_FLOAT, Brush),
    DEF(overlay_flags, REPLAY_INT, Brush),
    DEF(sampling_flag, REPLAY_INT, Brush),
    DEF(normal_weight, REPLAY_FLOAT, Brush),
    DEF(blend, REPLAY_SHORT, Brush),
    DEF(concave_mask_factor, REPLAY_FLOAT, Brush),
    {"", -1, -1}};

static ReplaySerialStruct BrushDef = {"Brush", brush_def};

static ReplaySerialDef stroke_cache_def[] = {
  DEF(bstrength, REPLAY_FLOAT, StrokeCache),
  DEF(radius, REPLAY_FLOAT, StrokeCache),
  DEF(pressure, REPLAY_FLOAT, StrokeCache),
  DEF(brush, REPLAY_STRUCT_PTR, StrokeCache, &BrushDef),
  DEF(location, REPLAY_VEC3, StrokeCache),
  DEF(view_normal, REPLAY_VEC3, StrokeCache),
  DEF(true_location, REPLAY_VEC3, StrokeCache),
  DEF(location, REPLAY_VEC3, StrokeCache),
  DEF(initial_radius, REPLAY_FLOAT, StrokeCache),
  DEF(dyntopo_pixel_radius, REPLAY_FLOAT, StrokeCache),
  DEF(radius_squared, REPLAY_FLOAT, StrokeCache),
  DEF(iteration_count, REPLAY_INT, StrokeCache),
  DEF(special_rotation, REPLAY_FLOAT, StrokeCache),
  DEF(grab_delta, REPLAY_VEC3, StrokeCache),
  DEF(grab_delta_symmetry, REPLAY_VEC3, StrokeCache),
  DEF(old_grab_location, REPLAY_VEC3, StrokeCache),
  DEF(orig_grab_location, REPLAY_VEC3, StrokeCache),
  DEF(rake_rotation, REPLAY_VEC4, StrokeCache),
  DEF(rake_rotation_symmetry, REPLAY_VEC4, StrokeCache),
  DEF(is_rake_rotation_valid, REPLAY_BOOL, StrokeCache),
  DEF(paint_face_set, REPLAY_INT, StrokeCache),
  DEF(symmetry, REPLAY_INT, StrokeCache),
  DEF(boundary_symmetry, REPLAY_INT, StrokeCache),
  DEF(mirror_symmetry_pass, REPLAY_INT, StrokeCache),
  DEF(true_view_normal, REPLAY_VEC3, StrokeCache),
  DEF(view_normal, REPLAY_VEC3, StrokeCache),
  DEF(sculpt_normal, REPLAY_VEC3, StrokeCache),
  DEF(sculpt_normal_symm, REPLAY_VEC3, StrokeCache),
  DEF(plane_offset, REPLAY_VEC3, StrokeCache),
  DEF(radial_symmetry_pass, REPLAY_INT, StrokeCache),
  DEF(last_center, REPLAY_VEC3, StrokeCache),
  DEF(original, REPLAY_BOOL, StrokeCache),
  DEF(initial_location, REPLAY_VEC3, StrokeCache),
  DEF(true_initial_location, REPLAY_VEC3, StrokeCache),
  DEF(initial_normal, REPLAY_VEC3, StrokeCache),
  DEF(true_initial_normal, REPLAY_VEC3, StrokeCache),
  DEF(vertex_rotation, REPLAY_FLOAT, StrokeCache),
  DEF(plane_trim_squared, REPLAY_FLOAT, StrokeCache),
  DEF(saved_smooth_size, REPLAY_FLOAT, StrokeCache),
  DEF(alt_smooth, REPLAY_BOOL, StrokeCache),
  DEF(density_seed, REPLAY_FLOAT, StrokeCache),
  DEF(stroke_distance, REPLAY_FLOAT, StrokeCache),
  DEF(stroke_distance_t, REPLAY_FLOAT, StrokeCache),
  DEF(last_dyntopo_t, REPLAY_FLOAT, StrokeCache),
  DEF(scale, REPLAY_VEC3, StrokeCache),
  {"", -1, -1}
};

static ReplaySerialStruct StrokeCacheDef = {"StrokeCache", stroke_cache_def};

static ReplaySerialDef paint_def[] = {
  DEF(symmetry_flags, REPLAY_INT, Paint),
  {"", -1, -1}
};
static ReplaySerialStruct PaintDef = {"Paint", paint_def};

static ReplaySerialDef sculpt_def[] = {
  DEF(paint, REPLAY_STRUCT, Sculpt, &PaintDef),
  DEF(detail_size, REPLAY_FLOAT, Sculpt),
  DEF(detail_range , REPLAY_FLOAT, Sculpt),
  DEF(constant_detail , REPLAY_FLOAT, Sculpt),
  DEF(detail_percent , REPLAY_FLOAT, Sculpt),
  DEF(dyntopo_spacing , REPLAY_INT, Sculpt),
  DEF(automasking_flags, REPLAY_INT, Sculpt),
  DEF(flags, REPLAY_INT, Sculpt),
  {"", -1, -1}
};

static ReplaySerialStruct SculptDef = {"Sculpt", sculpt_def};

static ReplaySerialDef ups_def[] = {
  DEF(size, REPLAY_INT, UnifiedPaintSettings),
  DEF(unprojected_radius, REPLAY_FLOAT, UnifiedPaintSettings),
  DEF(alpha, REPLAY_FLOAT, UnifiedPaintSettings),
  DEF(weight, REPLAY_FLOAT, UnifiedPaintSettings),
  DEF(rgb, REPLAY_VEC3, UnifiedPaintSettings),
  DEF(secondary_rgb, REPLAY_VEC3, UnifiedPaintSettings),
  DEF(flag, REPLAY_INT, UnifiedPaintSettings),
  DEF(last_rake, REPLAY_VEC2, UnifiedPaintSettings),
  DEF(last_rake_angle, REPLAY_FLOAT, UnifiedPaintSettings),
  DEF(last_stroke_valid, REPLAY_INT, UnifiedPaintSettings),
  DEF(average_stroke_accum, REPLAY_VEC3, UnifiedPaintSettings),
  DEF(unprojected_radius, REPLAY_FLOAT, UnifiedPaintSettings),
  DEF(average_stroke_counter, REPLAY_FLOAT, UnifiedPaintSettings),
  DEF(brush_rotation, REPLAY_FLOAT, UnifiedPaintSettings),
  DEF(brush_rotation_sec, REPLAY_FLOAT, UnifiedPaintSettings),
  DEF(anchored_size, REPLAY_INT, UnifiedPaintSettings),
  DEF(overlap_factor, REPLAY_FLOAT, UnifiedPaintSettings),
  DEF(draw_inverted, REPLAY_BYTE, UnifiedPaintSettings),
  DEF(stroke_active, REPLAY_BYTE, UnifiedPaintSettings),
  DEF(draw_anchored, REPLAY_BYTE, UnifiedPaintSettings),
  DEF(last_location, REPLAY_VEC3, UnifiedPaintSettings),
  DEF(last_hit, REPLAY_INT, UnifiedPaintSettings),
  DEF(anchored_initial_mouse, REPLAY_VEC2, UnifiedPaintSettings),
  DEF(pixel_radius, REPLAY_FLOAT, UnifiedPaintSettings),
  DEF(initial_pixel_radius, REPLAY_FLOAT, UnifiedPaintSettings),
  DEF(size_pressure_value, REPLAY_FLOAT, UnifiedPaintSettings),
  DEF(tex_mouse, REPLAY_VEC2, UnifiedPaintSettings),
  DEF(mask_tex_mouse, REPLAY_VEC2, UnifiedPaintSettings),
  {"", -1, -1}
};
static ReplaySerialStruct UnifiedPaintSettingsDef = {
  "UnifiedPaintSettings", ups_def
};

static ReplaySerialDef sample_def[] = {
    {"active_vertex_co", REPLAY_VEC3, offsetof(SculptBrushSample, active_vertex_co)},
    {"active_face_co", REPLAY_VEC3, offsetof(SculptBrushSample, active_face_co)},
    {"have_active_vertex", REPLAY_BOOL, offsetof(SculptBrushSample, have_active_vertex)},
    {"have_active_face", REPLAY_BOOL, offsetof(SculptBrushSample, have_active_face)},
    {"cache", REPLAY_STRUCT, offsetof(SculptBrushSample, cache), &StrokeCacheDef},
    //    {"brush", REPLAY_STRUCT, offsetof(SculptBrushSample, brush), &BrushDef},
    {"sd", REPLAY_STRUCT, offsetof(SculptBrushSample, sd), &SculptDef},
    DEF(ups, REPLAY_STRUCT, SculptBrushSample, &UnifiedPaintSettingsDef),
    DEF(stroke, REPLAY_STRUCT, SculptBrushSample, &PaintStrokeDef),
    {"", -1, -1}};

static ReplaySerialStruct SculptBrushSampleDef = {"SculptBrushSample", sample_def};

/* clang-format on */

typedef struct ReplaySerializer {
  struct {
    char prefix[256], op[32];
  } stack[16];
  int stack_head;
  DynStr *out;
} ReplaySerializer;

static void replay_samples_ensure_size(SculptReplayLog *log);

void replay_write_path(ReplaySerializer *state, char *key)
{
  char buf[512];

  if (state->stack_head >= 0) {
    sprintf(buf,
            "%s%s%s",
            state->stack[state->stack_head].prefix,
            state->stack[state->stack_head].op,
            key);
  }
  else {
    sprintf(buf, "%s", key);
  }

  BLI_dynstr_append(state->out, buf);
}

void replay_push_stack(ReplaySerializer *state, char *key, char *op)
{
  state->stack_head++;

  if (state->stack_head > 0) {
    sprintf(state->stack[state->stack_head].prefix,
            "%s%s%s",
            state->stack[state->stack_head - 1].prefix,
            state->stack[state->stack_head - 1].op,
            key);
  }
  else {
    sprintf(state->stack[state->stack_head].prefix, "%s", key);
  }

  sprintf(state->stack[state->stack_head].op, "%s", op);
}

void replay_pop_stack(ReplaySerializer *state)
{
  state->stack_head--;
}

#define SKIP_WS \
  while (i < len && ELEM(buf[i], ' ', '\t', '\r')) \
  i++

#define SKIP_ALL_WS \
  while (i < len && ELEM(buf[i], ' ', '\t', '\r', '\n')) \
  i++

#include <stdarg.h>

static int parse_replay_member(const char *buf, int len, ReplaySerialStruct *st, void *data)
{
  char *ptr = (char *)data;
  int i = 0;
  int n = 0;

  SKIP_WS;
  ReplaySerialDef *mdef = NULL;

  while (buf[i] != ':') {
    int a = strcspn(buf + i, ".-:");

    if (a < 0 || i + a >= len) {
      break;
    }

    char *name = alloca(a + 1);
    memcpy(name, buf + i, a);
    name[a] = 0;

    i += a;

    while (ELEM(buf[i], '-', '>', '.')) {
      i++;
    }

    SKIP_WS;

    ReplaySerialDef *mdef2 = st->members;
    while (mdef2->type != -1) {
      if (STREQ(mdef2->name, name)) {
        break;
      }
      mdef2++;
    }

    if (mdef2->type == -1) {
      printf("Failed to find memer \"%s!\n", name);
      return len;
    }

    SKIP_WS;

    ptr += mdef2->struct_offset;

    if (mdef2->type == REPLAY_STRUCT_PTR) {
      void **vptr = (void **)ptr;

      if (!*vptr) {
        char *line = alloca(len + 1);
        memcpy(line, buf, len);
        line[len] = 0;

        printf("error; missing memory for %s\n", line);
        return len;
      }

      ptr = (char *)*vptr;
      st = mdef2->sdef;
    }
    else if (mdef2->type == REPLAY_STRUCT) {
      st = mdef2->sdef;
    }

    mdef = mdef2;
  }

  if (!mdef) {
    printf("replay parse error\n");
    return len;
  }

  i++;
  SKIP_WS;

  switch (mdef->type) {
    case REPLAY_FLOAT: {
      float f = 0.0;

      sscanf(buf + i, "%f%n", &f, &n);
      i += n;
      *(float *)ptr = f;
      break;
    }
    case REPLAY_INT: {
      int f = 0;

      sscanf(buf + i, "%d%n", &f, &n);
      i += n;
      *(int *)ptr = f;
      break;
    }
    case REPLAY_BOOL:
    case REPLAY_BYTE: {
      int f = 0;

      sscanf(buf + i, "%d%n", &f, &n);
      i += n;
      *(unsigned char *)ptr = (unsigned char)f;
      break;
    }
    case REPLAY_VEC2: {
      float f[2];

      sscanf(buf + i, "[%f,%f]%n", &f[0], &f[1], &n);
      i += n;

      copy_v2_v2((float *)ptr, f);
      break;
    }
    case REPLAY_VEC3: {
      float f[3];

      sscanf(buf + i, "[%f,%f,%f]%n", &f[0], &f[1], &f[2], &n);
      i += n;

      copy_v3_v3((float *)ptr, f);
      break;
    }
    case REPLAY_VEC4: {
      float f[4];

      sscanf(buf + i, "[%f,%f,%f,%f]%n", &f[0], &f[1], &f[2], &f[3], &n);
      i += n;

      copy_v4_v4((float *)ptr, f);
      break;
    }
    case REPLAY_SHORT: {
      int f = 0;

      sscanf(buf + i, "%d%n", &f, &n);
      i += n;
      *(short *)ptr = (short)f;
      break;
    }
    default:
      printf("replay parse error: invalid type %d\n", mdef->type);
      break;
  }
  return i;
}

// data1 is dest, data2 is source
static void replay_load(ReplaySerialStruct *st, void *data1, void *data2)
{
  ReplaySerialDef *mdef = st->members;

  while (mdef->type != -1) {
    char *ptr1 = ((char *)data1) + mdef->struct_offset;
    char *ptr2 = ((char *)data2) + mdef->struct_offset;

    switch (mdef->type) {
      case REPLAY_STRUCT_PTR: {
        void **vptr1 = (void **)ptr1;
        void **vptr2 = (void **)ptr2;

        if (!*vptr1 || !*vptr2) {
          printf("failed to load pointers %p %p\n", *vptr1, *vptr2);
          mdef++;
          continue;
        }

        ptr1 = *vptr1;
        ptr2 = *vptr2;
      }
      case REPLAY_STRUCT:
        replay_load(mdef->sdef, ptr1, ptr2);
        break;
      case REPLAY_INT:
      case REPLAY_FLOAT:
        memcpy(ptr1, ptr2, sizeof(int));
        break;
      case REPLAY_BYTE:
      case REPLAY_BOOL:
        *ptr1 = *ptr2;
        break;
      case REPLAY_VEC2:
        memcpy(ptr1, ptr2, sizeof(float) * 2);
        break;
      case REPLAY_VEC3:
        memcpy(ptr1, ptr2, sizeof(float) * 3);
        break;
      case REPLAY_VEC4:
        memcpy(ptr1, ptr2, sizeof(float) * 4);
        break;
      case REPLAY_SHORT:
        memcpy(ptr1, ptr2, 2);
        break;
    }
    mdef++;
  }
}

void do_brush_action(struct Sculpt *sd,
                     struct Object *ob,
                     struct Brush *brush,
                     struct UnifiedPaintSettings *ups,
                     PaintModeSettings *paint_mode_settings);
void sculpt_combine_proxies(Sculpt *sd, Object *ob);
bool sculpt_tool_is_proxy_used(const char sculpt_tool);
void sculpt_stroke_update_step(bContext *C, struct PaintStroke *stroke, PointerRNA *itemptr);

static void *hashco(float fx, float fy, float fz, float fdimen)
{
  double x = (double)fx;
  double y = (double)fy;
  double z = (double)fz;
  double dimen = (double)fdimen;

  return (void *)((intptr_t)(z * dimen * dimen * dimen + y * dimen * dimen + x * dimen));
}

void SCULPT_replay_make_cube(struct bContext *C, int steps)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  if (!ss || !ss->bm) {
    return;
  }

  GHash *vhash = BLI_ghash_ptr_new("vhash");

  float df = 2.0f / (float)(steps - 1);

  int hashdimen = steps * 8;

  BMVert **grid = MEM_malloc_arrayN(steps * steps * 2, sizeof(*grid), "bmvert grid");
  BMesh *bm = ss->bm;

  BM_mesh_clear(bm);

  for (int side = 0; side < 6; side++) {
    int axis = side >= 3 ? side - 3 : side;
    float sign = side >= 3 ? -1.0f : 1.0f;

    printf("AXIS: %d\n", axis);

    float u = -1.0f;

    for (int i = 0; i < steps; i++, u += df) {
      float v = -1.0f;

      for (int j = 0; j < steps; j++, v += df) {
        float co[3];

        co[axis] = u;
        co[(axis + 1) % 3] = v;
        co[(axis + 2) % 3] = sign;

        // turn into sphere
        normalize_v3(co);
        // mul_v3_fl(co, 2.0f);

        void *key = hashco(co[0], co[1], co[2], hashdimen);

#if 0
        printf("%.3f %.3f %.3f, key: %p i: %d j: %d df: %f, u: %f v: %f\n",
               co[0],
               co[1],
               co[2],
               key,
               i,
               j,
               df,
               u,
               v);
#endif

        void **val = NULL;

        if (!BLI_ghash_ensure_p(vhash, key, &val)) {
          BMVert *v2 = BM_vert_create(bm, co, NULL, BM_CREATE_NOP);

          *val = (void *)v2;
        }

        BMVert *v2 = (BMVert *)*val;
        int idx = j * steps + i;

        grid[idx] = v2;
      }
    }

    for (int i = 0; i < steps - 1; i++) {
      for (int j = 0; j < steps - 1; j++) {
        int idx1 = j * steps + i;
        int idx2 = (j + 1) * steps + i;
        int idx3 = (j + 1) * steps + i + 1;
        int idx4 = j * steps + i + 1;

        BMVert *v1 = grid[idx1];
        BMVert *v2 = grid[idx2];
        BMVert *v3 = grid[idx3];
        BMVert *v4 = grid[idx4];

        if (v1 == v2 || v1 == v3 || v1 == v4 || v2 == v3 || v2 == v4 || v3 == v4) {
          printf("ERROR!\n");
          continue;
        }

        if (sign >= 0) {
          BMVert *vs[4] = {v4, v3, v2, v1};
          BM_face_create_verts(bm, vs, 4, NULL, BM_CREATE_NOP, true);
        }
        else {
          BMVert *vs[4] = {v1, v2, v3, v4};
          BM_face_create_verts(bm, vs, 4, NULL, BM_CREATE_NOP, true);
        }
      }
    }
  }

  MEM_SAFE_FREE(grid);
  BLI_ghash_free(vhash, NULL, NULL);

#if 1
  // randomize
  uint *rands[4];
  uint tots[4] = {bm->totvert, bm->totedge, bm->totloop, bm->totface};

  RNG *rng = BLI_rng_new(0);

  for (uint i = 0; i < 4; i++) {
    rands[i] = MEM_malloc_arrayN(tots[i], sizeof(uint), "rands[i]");

    for (uint j = 0; j < tots[i]; j++) {
      rands[i][j] = j;
    }

    for (uint j = 0; j < tots[i] >> 1; j++) {
      int j2 = BLI_rng_get_int(rng) % tots[i];
      SWAP(uint, rands[i][j], rands[i][j2]);
    }
  }

  BM_mesh_remap(bm, rands[0], rands[1], rands[2], rands[3]);

  for (int i = 0; i < 4; i++) {
    MEM_SAFE_FREE(rands[i]);
  }

  BLI_rng_free(rng);
#endif

  BKE_pbvh_free(ss->pbvh);
  ss->pbvh = NULL;

  // XXX call BKE_sculptsession_update_attr_refs here?

  /* Redraw. */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, ND_DATA | NC_OBJECT | ND_DRAW, ob);
}

void SCULPT_replay(struct bContext *C)
{
  Object *ob = CTX_data_active_object(C);

  if (!ob) {
    printf("no object\n");
    return;
  }

  Scene *scene = CTX_data_scene(C);

  if (!scene) {
    printf("no scene\n");
    return;
  }

  Sculpt *sd = scene->toolsettings->sculpt;

  if (!sd) {
    printf("no sculpt settings\n");
    return;
  }

  SculptSession *ss = ob->sculpt;

  if (!ss) {
    printf("object must be in sculpt mode\n");
    return;
  }

  if (!current_log) {
    printf("%s: no reply data\n", __func__);
    return;
  }

  SculptReplayLog *log = current_log;
  SculptBrushSample *samp = log->samples;
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  bool have_cache = ss->cache;
  ViewContext vc;

  log->is_playing = true;
  float last_dyntopo_t = 0.0f;

  SCULPT_undo_push_begin_ex(ob, "Replay");

  if (!have_cache) {
    ED_view3d_viewcontext_init(C, &vc, depsgraph);
  }

  for (int i = 0; i < log->totsample; i++, samp++) {
    if (!have_cache) {
      ss->cache = &samp->cache;
      ss->cache->vc = &vc;
    }
    else {
      replay_load(&StrokeCacheDef, &samp->cache, ss->cache);
    }

    replay_load(&SculptDef, &samp->sd, sd);
    replay_load(
        &UnifiedPaintSettingsDef, &samp->ups, &scene->toolsettings->unified_paint_settings);

    ss->cache->first_time = i == 0;
    samp->ups.last_stroke_valid = i > 0;

    Brush _brush = *ss->cache->brush;
    Brush *brush = &_brush;

    samp->stroke.brush = brush;
    samp->stroke.ups = &samp->ups;
    samp->stroke.vc = vc;
    samp->sd.paint.brush = brush;

    ss->cache->stroke = &samp->stroke;

    ss->cache->last_dyntopo_t = last_dyntopo_t;

    // XXX
    // sculpt_stroke_update_step(C, ss->cache->stroke, NULL);
    last_dyntopo_t = ss->cache->last_dyntopo_t;
    continue;
    do_brush_action(sd,
                    ob,
                    brush,
                    &scene->toolsettings->unified_paint_settings,
                    &scene->toolsettings->paint_mode);
    sculpt_combine_proxies(sd, ob);

    /* Hack to fix noise texture tearing mesh. */
    // sculpt_fix_noise_tear(sd, ob);

    /* TODO(sergey): This is not really needed for the solid shading,
     * which does use pBVH drawing anyway, but texture and wireframe
     * requires this.
     *
     * Could be optimized later, but currently don't think it's so
     * much common scenario.
     *
     * Same applies to the DEG_id_tag_update() invoked from
     * sculpt_flush_update_step().
     */
    if (ss->deform_modifiers_active) {
      SCULPT_flush_stroke_deform(sd, ob, sculpt_tool_is_proxy_used(brush->sculpt_tool));
    }
    else if (ss->shapekey_active) {
      // sculpt_update_keyblock(ob);
    }

    ss->cache->first_time = false;
    copy_v3_v3(ss->cache->true_last_location, ss->cache->true_location);

    /* Cleanup. */
    if (brush->sculpt_tool == SCULPT_TOOL_MASK) {
      SCULPT_flush_update_step(C, SCULPT_UPDATE_MASK);
    }
    else if (ELEM(brush->sculpt_tool, SCULPT_TOOL_PAINT, SCULPT_TOOL_SMEAR)) {
      SCULPT_flush_update_step(C, SCULPT_UPDATE_COLOR);
    }
    else {
      SCULPT_flush_update_step(C, SCULPT_UPDATE_COORDS);
    }

    int update = SCULPT_UPDATE_COORDS | SCULPT_UPDATE_COLOR | SCULPT_UPDATE_VISIBILITY |
                 SCULPT_UPDATE_MASK;
    SCULPT_flush_update_done(C, ob, update);
  }

  if (!have_cache) {
    ss->cache = NULL;
  }

  SCULPT_undo_push_end(ob);
  log->is_playing = false;
}

void SCULPT_replay_parse(const char *buf)
{
  if (current_log) {
    SCULPT_replay_log_end();
  }

  SculptReplayLog *log = current_log = SCULPT_replay_log_create();

  int i = 0;
  int n = 0;
  int len = strlen(buf);

  SKIP_ALL_WS;

  int version = 0;

  sscanf(buf + i, "version:%d\n%n", &version, &n);
  i += n;

  SKIP_ALL_WS;

  while (i < len) {
    // find newline

    SKIP_WS;

    int end = strcspn(buf + i, "\n");
    if (end < 0) {
      end = len - 1;  // last line?
    }

    if (end == 0) {
      // empty line
      i++;
      continue;
    }

    int nr = 0;
    if (sscanf(buf + i, "samp:%d.%n", &nr, &n) == 0) {
      i += end;
      SKIP_ALL_WS;
      continue;
    }
    i += n;

    log->totsample = MAX2(log->totsample, nr + 1);
    replay_samples_ensure_size(log);

    SculptBrushSample *samp = log->samples + nr;

    if (!samp->cache.brush) {
      samp->cache.brush = BLI_memarena_calloc(log->arena, sizeof(Brush));
    }

    i += parse_replay_member(buf + i, end, &SculptBrushSampleDef, samp);

    SKIP_ALL_WS;
  }

  return;
}

void replay_serialize_struct(ReplaySerializer *state, ReplaySerialStruct *def, void *struct_data)
{
  // DynStr *out = state->out;

  ReplaySerialDef *mdef = def->members;
  char buf[256];

  while (mdef->type >= 0) {
    char *ptr = (char *)struct_data;
    ptr += mdef->struct_offset;

    if (!ELEM(mdef->type, REPLAY_STRUCT, REPLAY_STRUCT_PTR)) {
      replay_write_path(state, mdef->name);
    }

    switch (mdef->type) {
      case REPLAY_STRUCT:
      case REPLAY_STRUCT_PTR:
        replay_push_stack(state, mdef->name, mdef->type == REPLAY_STRUCT ? "." : "->");
        // BLI_dynstr_append(state->out, " {\n");
        if (mdef->type == REPLAY_STRUCT_PTR) {
          replay_serialize_struct(state, mdef->sdef, *(void **)ptr);
        }
        else {
          replay_serialize_struct(state, mdef->sdef, ptr);
        }
        replay_pop_stack(state);
        // BLI_dynstr_append(state->out, "}\n");
        break;
      case REPLAY_INT:
        sprintf(buf, ": %d\n", *((int *)ptr));
        BLI_dynstr_append(state->out, buf);
        break;
      case REPLAY_FLOAT:
        sprintf(buf, ": %f\n", *((float *)ptr));
        BLI_dynstr_append(state->out, buf);
        break;
      case REPLAY_VEC2:
        sprintf(buf, ": [%f,%f]\n", ((float *)ptr)[0], ((float *)ptr)[1]);
        BLI_dynstr_append(state->out, buf);
        break;
      case REPLAY_VEC3:
        sprintf(buf, ": [%f,%f,%f]\n", ((float *)ptr)[0], ((float *)ptr)[1], ((float *)ptr)[2]);
        BLI_dynstr_append(state->out, buf);
        break;
      case REPLAY_VEC4:
        sprintf(buf,
                ": [%f,%f,%f,%f]\n",
                ((float *)ptr)[0],
                ((float *)ptr)[1],
                ((float *)ptr)[2],
                ((float *)ptr)[3]);
        BLI_dynstr_append(state->out, buf);
        break;
      case REPLAY_BOOL:
        sprintf(buf, ": %s\n", *ptr ? "1" : "0");
        BLI_dynstr_append(state->out, buf);
        break;
      case REPLAY_BYTE:
        sprintf(buf, ": %d\n", (int)*ptr);
        BLI_dynstr_append(state->out, buf);
        break;
      case REPLAY_SHORT:
        sprintf(buf, ": %d\n", (int)*((short *)ptr));
        BLI_dynstr_append(state->out, buf);
        break;
    }

    mdef++;
  }
}

void replay_state_init(ReplaySerializer *state)
{
  memset(state, 0, sizeof(*state));
  state->stack_head = -1;
}

char *SCULPT_replay_serialize()
{
  if (!current_log) {
    return "";
  }

  SculptReplayLog *log = current_log;
  DynStr *out = BLI_dynstr_new();

  ReplaySerializer state;

  BLI_dynstr_append(out, "version:1\n");

  replay_state_init(&state);
  state.out = out;

  for (int i = 0; i < log->totsample; i++) {
    char buf[32];

    sprintf(buf, "samp:%d", i);
    replay_push_stack(&state, buf, ".");

    replay_serialize_struct(&state, &SculptBrushSampleDef, log->samples + i);

    replay_pop_stack(&state);
  }

  char *ret = BLI_dynstr_get_cstring(out);
  BLI_dynstr_free(out);

  return ret;
}

static void SCULPT_replay_deserialize(SculptReplayLog *log) {}

static void replay_samples_ensure_size(SculptReplayLog *log)
{
  if (log->totsample >= log->samples_size) {
    int size = (2 + log->samples_size);
    size += size >> 1;

    if (!log->samples) {
      log->samples = MEM_calloc_arrayN(size, sizeof(*log->samples), "log->samples");
    }
    else {
      log->samples = MEM_recallocN(log->samples, sizeof(*log->samples) * size);
    }

    log->samples_size = size;
  }
}

static bool replay_ensure_tex(SculptReplayLog *log, MTex *tex)
{
  if (!tex->tex) {
    return true;
  }

  for (int i = 0; i < log->tot_textures; i++) {
    if (STREQ(log->textures[i]->id.name, tex->tex->id.name)) {
      return true;
    }
  }

  Tex *texcpy = (Tex *)BLI_memarena_alloc(log->arena, sizeof(Tex));
  *texcpy = *tex->tex;

  tex->tex = texcpy;

  if (texcpy->ima) {
    Image *ima = BLI_memarena_alloc(log->arena, sizeof(*ima));
    *ima = *texcpy->ima;
    texcpy->ima = ima;
  }
  // if (texcpy->ima && texcpy->ima->id);

  return false;
}

void SCULPT_replay_test()
{
  SculptSession ss = {0};
  Sculpt sd = {0};
  Object ob = {0};
  StrokeCache cache = {0};
  Brush brush = {0};

  brush.size = 1.5f;
  brush.weight = 2.0f;
  brush.autosmooth_factor = 2.0f;

  ss.cache = &cache;
  cache.bstrength = 1.0f;
  cache.radius = 1.5f;
  cache.brush = &brush;

  ss.active_vertex.i = -1LL;
  ss.active_face.i = -1LL;

  SCULPT_replay_log_start();
  SCULPT_replay_log_append(&sd, &ss, &ob);
  char *buf = SCULPT_replay_serialize();

  if (buf) {
    printf("=========result=======\n%s\n", buf);
  }

  MEM_SAFE_FREE(buf);
  SCULPT_replay_log_end();
}

void SCULPT_replay_log_append(Sculpt *sd, SculptSession *ss, Object *ob)
{
  SculptReplayLog *log = current_log;

  if (!log || log->is_playing) {
    return;
  }

  log->totsample++;
  replay_samples_ensure_size(log);

  SculptBrushSample *samp = log->samples + log->totsample - 1;

  if (!ss->cache) {
    printf("Error!!");
    return;
  }

  samp->time = PIL_check_seconds_timer();
  samp->stroke = *ss->cache->stroke;

  samp->sd = *sd;
  samp->cache = *ss->cache;

  // replay_ensure_tex(log, &samp->cache->brush.mtex);

  if (ss->active_vertex.i != -1LL) {
    samp->have_active_vertex = true;
    // copy_v3_v3(samp->active_vertex_co, SCULPT_vertex_co_get(ss, ss->active_vertex));
  }
  else {
    zero_v3(samp->active_vertex_co);
    samp->have_active_vertex = false;
  }

  // TODO: active face
  samp->have_active_face = false;
}

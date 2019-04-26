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
 */

/** \file
 * \ingroup bke
 */

#include "MEM_guardedalloc.h"

#include <math.h>
#include <stdio.h>

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_kdtree.h"
#include "BLI_string_utils.h"
#include "BLI_task.h"
#include "BLI_threads.h"
#include "BLI_utildefines.h"

#include "BLT_translation.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_dynamicpaint_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"

#include "BKE_animsys.h"
#include "BKE_armature.h"
#include "BKE_bvhutils.h" /* bvh tree */
#include "BKE_collection.h"
#include "BKE_collision.h"
#include "BKE_colorband.h"
#include "BKE_constraint.h"
#include "BKE_customdata.h"
#include "BKE_deform.h"
#include "BKE_dynamicpaint.h"
#include "BKE_effect.h"
#include "BKE_image.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_runtime.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_scene.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

/* for image output */
#include "IMB_imbuf_types.h"
#include "IMB_imbuf.h"

/* to read material/texture color */
#include "RE_render_ext.h"
#include "RE_shader_ext.h"

#include "atomic_ops.h"

#include "CLG_log.h"

/* could enable at some point but for now there are far too many conversions */
#ifdef __GNUC__
//#  pragma GCC diagnostic ignored "-Wdouble-promotion"
#endif

static CLG_LogRef LOG = {"bke.dynamicpaint"};

/* precalculated gaussian factors for 5x super sampling */
static const float gaussianFactors[5] = {
    0.996849f,
    0.596145f,
    0.596145f,
    0.596145f,
    0.524141f,
};
static const float gaussianTotal = 3.309425f;

/* UV Image neighboring pixel table x and y list */
static int neighX[8] = {1, 1, 0, -1, -1, -1, 0, 1};
static int neighY[8] = {0, 1, 1, 1, 0, -1, -1, -1};

/* Neighbor x/y list that prioritizes grid directions over diagonals */
static int neighStraightX[8] = {1, 0, -1, 0, 1, -1, -1, 1};
static int neighStraightY[8] = {0, 1, 0, -1, 1, 1, -1, -1};

/* subframe_updateObject() flags */
#define SUBFRAME_RECURSION 5
/* surface_getBrushFlags() return vals */
#define BRUSH_USES_VELOCITY (1 << 0)
/* brush mesh raycast status */
#define HIT_VOLUME 1
#define HIT_PROXIMITY 2
/* dynamicPaint_findNeighbourPixel() return codes */
#define NOT_FOUND -1
#define ON_MESH_EDGE -2
#define OUT_OF_TEXTURE -3
/* paint effect default movement per frame in global units */
#define EFF_MOVEMENT_PER_FRAME 0.05f
/* initial wave time factor */
#define WAVE_TIME_FAC (1.0f / 24.f)
#define CANVAS_REL_SIZE 5.0f
/* drying limits */
#define MIN_WETNESS 0.001f
#define MAX_WETNESS 5.0f

/* dissolve inline function */
BLI_INLINE void value_dissolve(float *r_value,
                               const float time,
                               const float scale,
                               const bool is_log)
{
  *r_value = (is_log) ? (*r_value) * (powf(MIN_WETNESS, 1.0f / (1.2f * time / scale))) :
                        (*r_value) - 1.0f / time * scale;
}

/***************************** Internal Structs ***************************/

typedef struct Bounds2D {
  float min[2], max[2];
} Bounds2D;

typedef struct Bounds3D {
  float min[3], max[3];
  bool valid;
} Bounds3D;

typedef struct VolumeGrid {
  int dim[3];
  Bounds3D grid_bounds; /* whole grid bounds */

  Bounds3D *bounds; /* (x*y*z) precalculated grid cell bounds */
  int *s_pos;       /* (x*y*z) t_index begin id */
  int *s_num;       /* (x*y*z) number of t_index points */
  int *t_index;     /* actual surface point index, access: (s_pos + s_num) */

  int *temp_t_index;
} VolumeGrid;

typedef struct Vec3f {
  float v[3];
} Vec3f;

typedef struct BakeAdjPoint {
  float dir[3]; /* vector pointing towards this neighbor */
  float dist;   /* distance to */
} BakeAdjPoint;

/* Surface data used while processing a frame */
typedef struct PaintBakeNormal {
  float invNorm[3];   /* current pixel world-space inverted normal */
  float normal_scale; /* normal directional scale for displace mapping */
} PaintBakeNormal;

/* Temp surface data used to process a frame */
typedef struct PaintBakeData {
  /* point space data */
  PaintBakeNormal *bNormal;
  int *s_pos; /* index to start reading point sample realCoord */
  int *s_num; /* num of realCoord samples */
  Vec3f *
      realCoord; /* current pixel center world-space coordinates for each sample ordered as (s_pos + s_num) */
  Bounds3D mesh_bounds;
  float dim[3];

  /* adjacency info */
  BakeAdjPoint *bNeighs; /* current global neighbor distances and directions, if required */
  double average_dist;
  /* space partitioning */
  VolumeGrid *grid; /* space partitioning grid to optimize brush checks */

  /* velocity and movement */
  Vec3f *velocity; /* speed vector in global space movement per frame, if required */
  Vec3f *prev_velocity;
  float *brush_velocity;  /* special temp data for post-p velocity based brushes like smudge
                           * 3 float dir vec + 1 float str */
  MVert *prev_verts;      /* copy of previous frame vertices. used to observe surface movement */
  float prev_obmat[4][4]; /* previous frame object matrix */
  int clear; /* flag to check if surface was cleared/reset -> have to redo velocity etc. */
} PaintBakeData;

/* UV Image sequence format point */
typedef struct PaintUVPoint {
  /* Pixel / mesh data */
  unsigned int tri_index, pixel_index; /* tri index on domain derived mesh */
  unsigned int v1, v2, v3;             /* vertex indexes */

  unsigned int
      neighbour_pixel; /* If this pixel isn't uv mapped to any face, but it's neighboring pixel is */
} PaintUVPoint;

typedef struct ImgSeqFormatData {
  PaintUVPoint *uv_p;
  Vec3f *barycentricWeights; /* b-weights for all pixel samples */
} ImgSeqFormatData;

/* adjacency data flags */
#define ADJ_ON_MESH_EDGE (1 << 0)
#define ADJ_BORDER_PIXEL (1 << 1)

typedef struct PaintAdjData {
  int *
      n_target; /* array of neighboring point indexes, for single sample use (n_index + neigh_num) */
  int *n_index;      /* index to start reading n_target for each point */
  int *n_num;        /* num of neighs for each point */
  int *flags;        /* vertex adjacency flags */
  int total_targets; /* size of n_target */
  int *border;       /* indices of border pixels (only for texture paint) */
  int total_border;  /* size of border */
} PaintAdjData;

/************************* Runtime evaluation store ***************************/

void dynamicPaint_Modifier_free_runtime(DynamicPaintRuntime *runtime_data)
{
  if (runtime_data == NULL) {
    return;
  }
  if (runtime_data->canvas_mesh) {
    BKE_id_free(NULL, runtime_data->canvas_mesh);
  }
  if (runtime_data->brush_mesh) {
    BKE_id_free(NULL, runtime_data->brush_mesh);
  }
  MEM_freeN(runtime_data);
}

static DynamicPaintRuntime *dynamicPaint_Modifier_runtime_ensure(DynamicPaintModifierData *pmd)
{
  if (pmd->modifier.runtime == NULL) {
    pmd->modifier.runtime = MEM_callocN(sizeof(DynamicPaintRuntime), "dynamic paint runtime");
  }
  return (DynamicPaintRuntime *)pmd->modifier.runtime;
}

static Mesh *dynamicPaint_canvas_mesh_get(DynamicPaintCanvasSettings *canvas)
{
  if (canvas->pmd->modifier.runtime == NULL) {
    return NULL;
  }
  DynamicPaintRuntime *runtime_data = (DynamicPaintRuntime *)canvas->pmd->modifier.runtime;
  return runtime_data->canvas_mesh;
}

static Mesh *dynamicPaint_brush_mesh_get(DynamicPaintBrushSettings *brush)
{
  if (brush->pmd->modifier.runtime == NULL) {
    return NULL;
  }
  DynamicPaintRuntime *runtime_data = (DynamicPaintRuntime *)brush->pmd->modifier.runtime;
  return runtime_data->brush_mesh;
}

/***************************** General Utils ******************************/

/* Set canvas error string to display at the bake report */
static int setError(DynamicPaintCanvasSettings *canvas, const char *string)
{
  /* Add error to canvas ui info label */
  BLI_strncpy(canvas->error, string, sizeof(canvas->error));
  CLOG_STR_ERROR(&LOG, string);
  return 0;
}

/* Get number of surface points for cached types */
static int dynamicPaint_surfaceNumOfPoints(DynamicPaintSurface *surface)
{
  if (surface->format == MOD_DPAINT_SURFACE_F_PTEX) {
    return 0; /* not supported atm */
  }
  else if (surface->format == MOD_DPAINT_SURFACE_F_VERTEX) {
    const Mesh *canvas_mesh = dynamicPaint_canvas_mesh_get(surface->canvas);
    return (canvas_mesh) ? canvas_mesh->totvert : 0;
  }

  return 0;
}

/* get currently active surface (in user interface) */
DynamicPaintSurface *get_activeSurface(DynamicPaintCanvasSettings *canvas)
{
  return BLI_findlink(&canvas->surfaces, canvas->active_sur);
}

bool dynamicPaint_outputLayerExists(struct DynamicPaintSurface *surface, Object *ob, int output)
{
  const char *name;

  if (output == 0) {
    name = surface->output_name;
  }
  else if (output == 1) {
    name = surface->output_name2;
  }
  else {
    return false;
  }

  if (surface->format == MOD_DPAINT_SURFACE_F_VERTEX) {
    if (surface->type == MOD_DPAINT_SURFACE_T_PAINT) {
      Mesh *me = ob->data;
      return (CustomData_get_named_layer_index(&me->ldata, CD_MLOOPCOL, name) != -1);
    }
    else if (surface->type == MOD_DPAINT_SURFACE_T_WEIGHT) {
      return (defgroup_name_index(ob, name) != -1);
    }
  }

  return false;
}

static bool surface_duplicateOutputExists(void *arg, const char *name)
{
  DynamicPaintSurface *t_surface = arg;
  DynamicPaintSurface *surface = t_surface->canvas->surfaces.first;

  for (; surface; surface = surface->next) {
    if (surface != t_surface && surface->type == t_surface->type &&
        surface->format == t_surface->format) {
      if ((surface->output_name[0] != '\0' && !BLI_path_cmp(name, surface->output_name)) ||
          (surface->output_name2[0] != '\0' && !BLI_path_cmp(name, surface->output_name2))) {
        return true;
      }
    }
  }
  return false;
}

static void surface_setUniqueOutputName(DynamicPaintSurface *surface, char *basename, int output)
{
  char name[64];
  BLI_strncpy(name, basename, sizeof(name)); /* in case basename is surface->name use a copy */
  if (output == 0) {
    BLI_uniquename_cb(surface_duplicateOutputExists,
                      surface,
                      name,
                      '.',
                      surface->output_name,
                      sizeof(surface->output_name));
  }
  else if (output == 1) {
    BLI_uniquename_cb(surface_duplicateOutputExists,
                      surface,
                      name,
                      '.',
                      surface->output_name2,
                      sizeof(surface->output_name2));
  }
}

static bool surface_duplicateNameExists(void *arg, const char *name)
{
  DynamicPaintSurface *t_surface = arg;
  DynamicPaintSurface *surface = t_surface->canvas->surfaces.first;

  for (; surface; surface = surface->next) {
    if (surface != t_surface && STREQ(name, surface->name)) {
      return true;
    }
  }
  return false;
}

void dynamicPaintSurface_setUniqueName(DynamicPaintSurface *surface, const char *basename)
{
  char name[64];
  BLI_strncpy(name, basename, sizeof(name)); /* in case basename is surface->name use a copy */
  BLI_uniquename_cb(
      surface_duplicateNameExists, surface, name, '.', surface->name, sizeof(surface->name));
}

/* change surface data to defaults on new type */
void dynamicPaintSurface_updateType(struct DynamicPaintSurface *surface)
{
  if (surface->format == MOD_DPAINT_SURFACE_F_IMAGESEQ) {
    surface->output_name[0] = '\0';
    surface->output_name2[0] = '\0';
    surface->flags |= MOD_DPAINT_ANTIALIAS;
    surface->depth_clamp = 1.0f;
  }
  else {
    strcpy(surface->output_name, "dp_");
    BLI_strncpy(surface->output_name2, surface->output_name, sizeof(surface->output_name2));
    surface->flags &= ~MOD_DPAINT_ANTIALIAS;
    surface->depth_clamp = 0.0f;
  }

  if (surface->type == MOD_DPAINT_SURFACE_T_PAINT) {
    strcat(surface->output_name, "paintmap");
    strcat(surface->output_name2, "wetmap");
    surface_setUniqueOutputName(surface, surface->output_name2, 1);
  }
  else if (surface->type == MOD_DPAINT_SURFACE_T_DISPLACE) {
    strcat(surface->output_name, "displace");
  }
  else if (surface->type == MOD_DPAINT_SURFACE_T_WEIGHT) {
    strcat(surface->output_name, "weight");
  }
  else if (surface->type == MOD_DPAINT_SURFACE_T_WAVE) {
    strcat(surface->output_name, "wave");
  }

  surface_setUniqueOutputName(surface, surface->output_name, 0);
}

static int surface_totalSamples(DynamicPaintSurface *surface)
{
  if (surface->format == MOD_DPAINT_SURFACE_F_IMAGESEQ && surface->flags & MOD_DPAINT_ANTIALIAS) {
    return (surface->data->total_points * 5);
  }
  if (surface->format == MOD_DPAINT_SURFACE_F_VERTEX && surface->flags & MOD_DPAINT_ANTIALIAS &&
      surface->data->adj_data) {
    return (surface->data->total_points + surface->data->adj_data->total_targets);
  }

  return surface->data->total_points;
}

static void blendColors(const float t_color[3],
                        const float t_alpha,
                        const float s_color[3],
                        const float s_alpha,
                        float result[4])
{
  /* Same thing as BLI's blend_color_mix_float(), but for non-premultiplied alpha. */
  int i;
  float i_alpha = 1.0f - s_alpha;
  float f_alpha = t_alpha * i_alpha + s_alpha;

  /* blend colors */
  if (f_alpha) {
    for (i = 0; i < 3; i++) {
      result[i] = (t_color[i] * t_alpha * i_alpha + s_color[i] * s_alpha) / f_alpha;
    }
  }
  else {
    copy_v3_v3(result, t_color);
  }
  /* return final alpha */
  result[3] = f_alpha;
}

/* Mix two alpha weighed colors by a defined ratio. output is saved at a_color */
static float mixColors(
    float a_color[3], float a_weight, const float b_color[3], float b_weight, float ratio)
{
  float weight_ratio, factor;
  if (b_weight) {
    /* if first value has no weight just use b_color */
    if (!a_weight) {
      copy_v3_v3(a_color, b_color);
      return b_weight * ratio;
    }
    weight_ratio = b_weight / (a_weight + b_weight);
  }
  else {
    return a_weight * (1.0f - ratio);
  }

  /* calculate final interpolation factor */
  if (ratio <= 0.5f) {
    factor = weight_ratio * (ratio * 2.0f);
  }
  else {
    ratio = (ratio * 2.0f - 1.0f);
    factor = weight_ratio * (1.0f - ratio) + ratio;
  }
  /* mix final color */
  interp_v3_v3v3(a_color, a_color, b_color, factor);
  return (1.0f - factor) * a_weight + factor * b_weight;
}

static void scene_setSubframe(Scene *scene, float subframe)
{
  /* dynamic paint subframes must be done on previous frame */
  scene->r.cfra -= 1;
  scene->r.subframe = subframe;
}

static int surface_getBrushFlags(DynamicPaintSurface *surface, Depsgraph *depsgraph)
{
  unsigned int numobjects;
  Object **objects = BKE_collision_objects_create(
      depsgraph, NULL, surface->brush_group, &numobjects, eModifierType_DynamicPaint);

  int flags = 0;

  for (int i = 0; i < numobjects; i++) {
    Object *brushObj = objects[i];

    ModifierData *md = modifiers_findByType(brushObj, eModifierType_DynamicPaint);
    if (md && md->mode & (eModifierMode_Realtime | eModifierMode_Render)) {
      DynamicPaintModifierData *pmd2 = (DynamicPaintModifierData *)md;

      if (pmd2->brush) {
        DynamicPaintBrushSettings *brush = pmd2->brush;

        if (brush->flags & MOD_DPAINT_USES_VELOCITY) {
          flags |= BRUSH_USES_VELOCITY;
        }
      }
    }
  }

  BKE_collision_objects_free(objects);

  return flags;
}

/* check whether two bounds intersect */
static bool boundsIntersect(Bounds3D *b1, Bounds3D *b2)
{
  if (!b1->valid || !b2->valid) {
    return false;
  }
  for (int i = 2; i--;) {
    if (!(b1->min[i] <= b2->max[i] && b1->max[i] >= b2->min[i])) {
      return false;
    }
  }
  return true;
}

/* check whether two bounds intersect inside defined proximity */
static bool boundsIntersectDist(Bounds3D *b1, Bounds3D *b2, const float dist)
{
  if (!b1->valid || !b2->valid) {
    return false;
  }
  for (int i = 2; i--;) {
    if (!(b1->min[i] <= (b2->max[i] + dist) && b1->max[i] >= (b2->min[i] - dist))) {
      return false;
    }
  }
  return true;
}

/* check whether bounds intersects a point with given radius */
static bool boundIntersectPoint(Bounds3D *b, float point[3], const float radius)
{
  if (!b->valid) {
    return false;
  }
  for (int i = 2; i--;) {
    if (!(b->min[i] <= (point[i] + radius) && b->max[i] >= (point[i] - radius))) {
      return false;
    }
  }
  return true;
}

/* expand bounds by a new point */
static void boundInsert(Bounds3D *b, float point[3])
{
  if (!b->valid) {
    copy_v3_v3(b->min, point);
    copy_v3_v3(b->max, point);
    b->valid = true;
    return;
  }

  minmax_v3v3_v3(b->min, b->max, point);
}

static float getSurfaceDimension(PaintSurfaceData *sData)
{
  Bounds3D *mb = &sData->bData->mesh_bounds;
  return max_fff((mb->max[0] - mb->min[0]), (mb->max[1] - mb->min[1]), (mb->max[2] - mb->min[2]));
}

static void freeGrid(PaintSurfaceData *data)
{
  PaintBakeData *bData = data->bData;
  VolumeGrid *grid = bData->grid;

  if (grid->bounds) {
    MEM_freeN(grid->bounds);
  }
  if (grid->s_pos) {
    MEM_freeN(grid->s_pos);
  }
  if (grid->s_num) {
    MEM_freeN(grid->s_num);
  }
  if (grid->t_index) {
    MEM_freeN(grid->t_index);
  }

  MEM_freeN(bData->grid);
  bData->grid = NULL;
}

static void grid_bound_insert_cb_ex(void *__restrict userdata,
                                    const int i,
                                    const ParallelRangeTLS *__restrict tls)
{
  PaintBakeData *bData = userdata;

  Bounds3D *grid_bound = tls->userdata_chunk;

  boundInsert(grid_bound, bData->realCoord[bData->s_pos[i]].v);
}

static void grid_bound_insert_finalize(void *__restrict userdata, void *__restrict userdata_chunk)
{
  PaintBakeData *bData = userdata;
  VolumeGrid *grid = bData->grid;

  Bounds3D *grid_bound = userdata_chunk;

  boundInsert(&grid->grid_bounds, grid_bound->min);
  boundInsert(&grid->grid_bounds, grid_bound->max);
}

static void grid_cell_points_cb_ex(void *__restrict userdata,
                                   const int i,
                                   const ParallelRangeTLS *__restrict tls)
{
  PaintBakeData *bData = userdata;
  VolumeGrid *grid = bData->grid;
  int *temp_t_index = grid->temp_t_index;
  int *s_num = tls->userdata_chunk;

  int co[3];

  for (int j = 3; j--;) {
    co[j] = (int)floorf((bData->realCoord[bData->s_pos[i]].v[j] - grid->grid_bounds.min[j]) /
                        bData->dim[j] * grid->dim[j]);
    CLAMP(co[j], 0, grid->dim[j] - 1);
  }

  temp_t_index[i] = co[0] + co[1] * grid->dim[0] + co[2] * grid->dim[0] * grid->dim[1];
  s_num[temp_t_index[i]]++;
}

static void grid_cell_points_finalize(void *__restrict userdata, void *__restrict userdata_chunk)
{
  PaintBakeData *bData = userdata;
  VolumeGrid *grid = bData->grid;
  const int grid_cells = grid->dim[0] * grid->dim[1] * grid->dim[2];

  int *s_num = userdata_chunk;

  /* calculate grid indexes */
  for (int i = 0; i < grid_cells; i++) {
    grid->s_num[i] += s_num[i];
  }
}

static void grid_cell_bounds_cb(void *__restrict userdata,
                                const int x,
                                const ParallelRangeTLS *__restrict UNUSED(tls))
{
  PaintBakeData *bData = userdata;
  VolumeGrid *grid = bData->grid;
  float *dim = bData->dim;
  int *grid_dim = grid->dim;

  for (int y = 0; y < grid_dim[1]; y++) {
    for (int z = 0; z < grid_dim[2]; z++) {
      const int b_index = x + y * grid_dim[0] + z * grid_dim[0] * grid_dim[1];
      /* set bounds */
      for (int j = 3; j--;) {
        const int s = (j == 0) ? x : ((j == 1) ? y : z);
        grid->bounds[b_index].min[j] = grid->grid_bounds.min[j] + dim[j] / grid_dim[j] * s;
        grid->bounds[b_index].max[j] = grid->grid_bounds.min[j] + dim[j] / grid_dim[j] * (s + 1);
      }
      grid->bounds[b_index].valid = true;
    }
  }
}

static void surfaceGenerateGrid(struct DynamicPaintSurface *surface)
{
  PaintSurfaceData *sData = surface->data;
  PaintBakeData *bData = sData->bData;
  VolumeGrid *grid;
  int grid_cells, axis = 3;
  int *temp_t_index = NULL;
  int *temp_s_num = NULL;

  if (bData->grid) {
    freeGrid(sData);
  }

  bData->grid = MEM_callocN(sizeof(VolumeGrid), "Surface Grid");
  grid = bData->grid;

  {
    int i, error = 0;
    float dim_factor, volume, dim[3];
    float td[3];
    float min_dim;

    /* calculate canvas dimensions */
    /* Important to init correctly our ref grid_bound... */
    boundInsert(&grid->grid_bounds, bData->realCoord[bData->s_pos[0]].v);
    {
      ParallelRangeSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      settings.use_threading = (sData->total_points > 1000);
      settings.userdata_chunk = &grid->grid_bounds;
      settings.userdata_chunk_size = sizeof(grid->grid_bounds);
      settings.func_finalize = grid_bound_insert_finalize;
      BLI_task_parallel_range(0, sData->total_points, bData, grid_bound_insert_cb_ex, &settings);
    }
    /* get dimensions */
    sub_v3_v3v3(dim, grid->grid_bounds.max, grid->grid_bounds.min);
    copy_v3_v3(td, dim);
    copy_v3_v3(bData->dim, dim);
    min_dim = max_fff(td[0], td[1], td[2]) / 1000.f;

    /* deactivate zero axises */
    for (i = 0; i < 3; i++) {
      if (td[i] < min_dim) {
        td[i] = 1.0f;
        axis--;
      }
    }

    if (axis == 0 || max_fff(td[0], td[1], td[2]) < 0.0001f) {
      MEM_freeN(bData->grid);
      bData->grid = NULL;
      return;
    }

    /* now calculate grid volume/area/width depending on num of active axis */
    volume = td[0] * td[1] * td[2];

    /* determine final grid size by trying to fit average 10.000 points per grid cell */
    dim_factor = (float)pow((double)volume / ((double)sData->total_points / 10000.0),
                            1.0 / (double)axis);

    /* define final grid size using dim_factor, use min 3 for active axises */
    for (i = 0; i < 3; i++) {
      grid->dim[i] = (int)floor(td[i] / dim_factor);
      CLAMP(grid->dim[i], (dim[i] >= min_dim) ? 3 : 1, 100);
    }
    grid_cells = grid->dim[0] * grid->dim[1] * grid->dim[2];

    /* allocate memory for grids */
    grid->bounds = MEM_callocN(sizeof(Bounds3D) * grid_cells, "Surface Grid Bounds");
    grid->s_pos = MEM_callocN(sizeof(int) * grid_cells, "Surface Grid Position");

    grid->s_num = MEM_callocN(sizeof(int) * grid_cells, "Surface Grid Points");
    temp_s_num = MEM_callocN(sizeof(int) * grid_cells, "Temp Surface Grid Points");
    grid->t_index = MEM_callocN(sizeof(int) * sData->total_points, "Surface Grid Target Ids");
    grid->temp_t_index = temp_t_index = MEM_callocN(sizeof(int) * sData->total_points,
                                                    "Temp Surface Grid Target Ids");

    /* in case of an allocation failure abort here */
    if (!grid->bounds || !grid->s_pos || !grid->s_num || !grid->t_index || !temp_s_num ||
        !temp_t_index) {
      error = 1;
    }

    if (!error) {
      /* calculate number of points within each cell */
      {
        ParallelRangeSettings settings;
        BLI_parallel_range_settings_defaults(&settings);
        settings.use_threading = (sData->total_points > 1000);
        settings.userdata_chunk = grid->s_num;
        settings.userdata_chunk_size = sizeof(*grid->s_num) * grid_cells;
        settings.func_finalize = grid_cell_points_finalize;
        BLI_task_parallel_range(0, sData->total_points, bData, grid_cell_points_cb_ex, &settings);
      }

      /* calculate grid indexes (not needed for first cell, which is zero). */
      for (i = 1; i < grid_cells; i++) {
        grid->s_pos[i] = grid->s_pos[i - 1] + grid->s_num[i - 1];
      }

      /* save point indexes to final array */
      for (i = 0; i < sData->total_points; i++) {
        int pos = grid->s_pos[temp_t_index[i]] + temp_s_num[temp_t_index[i]];
        grid->t_index[pos] = i;

        temp_s_num[temp_t_index[i]]++;
      }

      /* calculate cell bounds */
      {
        ParallelRangeSettings settings;
        BLI_parallel_range_settings_defaults(&settings);
        settings.use_threading = (grid_cells > 1000);
        BLI_task_parallel_range(0, grid->dim[0], bData, grid_cell_bounds_cb, &settings);
      }
    }

    if (temp_s_num) {
      MEM_freeN(temp_s_num);
    }
    if (temp_t_index) {
      MEM_freeN(temp_t_index);
    }
    grid->temp_t_index = NULL;

    if (error || !grid->s_num) {
      setError(surface->canvas, N_("Not enough free memory"));
      freeGrid(sData);
    }
  }
}

/***************************** Freeing data ******************************/

/* Free brush data */
void dynamicPaint_freeBrush(struct DynamicPaintModifierData *pmd)
{
  if (pmd->brush) {
    if (pmd->brush->paint_ramp) {
      MEM_freeN(pmd->brush->paint_ramp);
    }
    if (pmd->brush->vel_ramp) {
      MEM_freeN(pmd->brush->vel_ramp);
    }

    MEM_freeN(pmd->brush);
    pmd->brush = NULL;
  }
}

static void dynamicPaint_freeAdjData(PaintSurfaceData *data)
{
  if (data->adj_data) {
    if (data->adj_data->n_index) {
      MEM_freeN(data->adj_data->n_index);
    }
    if (data->adj_data->n_num) {
      MEM_freeN(data->adj_data->n_num);
    }
    if (data->adj_data->n_target) {
      MEM_freeN(data->adj_data->n_target);
    }
    if (data->adj_data->flags) {
      MEM_freeN(data->adj_data->flags);
    }
    if (data->adj_data->border) {
      MEM_freeN(data->adj_data->border);
    }
    MEM_freeN(data->adj_data);
    data->adj_data = NULL;
  }
}

static void free_bakeData(PaintSurfaceData *data)
{
  PaintBakeData *bData = data->bData;
  if (bData) {
    if (bData->bNormal) {
      MEM_freeN(bData->bNormal);
    }
    if (bData->s_pos) {
      MEM_freeN(bData->s_pos);
    }
    if (bData->s_num) {
      MEM_freeN(bData->s_num);
    }
    if (bData->realCoord) {
      MEM_freeN(bData->realCoord);
    }
    if (bData->bNeighs) {
      MEM_freeN(bData->bNeighs);
    }
    if (bData->grid) {
      freeGrid(data);
    }
    if (bData->prev_verts) {
      MEM_freeN(bData->prev_verts);
    }
    if (bData->velocity) {
      MEM_freeN(bData->velocity);
    }
    if (bData->prev_velocity) {
      MEM_freeN(bData->prev_velocity);
    }

    MEM_freeN(data->bData);
    data->bData = NULL;
  }
}

/* free surface data if it's not used anymore */
static void surface_freeUnusedData(DynamicPaintSurface *surface)
{
  if (!surface->data) {
    return;
  }

  /* free bakedata if not active or surface is baked */
  if (!(surface->flags & MOD_DPAINT_ACTIVE) ||
      (surface->pointcache && surface->pointcache->flag & PTCACHE_BAKED)) {
    free_bakeData(surface->data);
  }
}

void dynamicPaint_freeSurfaceData(DynamicPaintSurface *surface)
{
  PaintSurfaceData *data = surface->data;
  if (!data) {
    return;
  }

  if (data->format_data) {
    /* format specific free */
    if (surface->format == MOD_DPAINT_SURFACE_F_IMAGESEQ) {
      ImgSeqFormatData *format_data = (ImgSeqFormatData *)data->format_data;
      if (format_data->uv_p) {
        MEM_freeN(format_data->uv_p);
      }
      if (format_data->barycentricWeights) {
        MEM_freeN(format_data->barycentricWeights);
      }
    }
    MEM_freeN(data->format_data);
  }
  /* type data */
  if (data->type_data) {
    MEM_freeN(data->type_data);
  }
  dynamicPaint_freeAdjData(data);
  /* bake data */
  free_bakeData(data);

  MEM_freeN(surface->data);
  surface->data = NULL;
}

void dynamicPaint_freeSurface(const DynamicPaintModifierData *pmd, DynamicPaintSurface *surface)
{
  /* point cache */
  if ((pmd->modifier.flag & eModifierFlag_SharedCaches) == 0) {
    BKE_ptcache_free_list(&(surface->ptcaches));
  }
  surface->pointcache = NULL;

  if (surface->effector_weights) {
    MEM_freeN(surface->effector_weights);
  }
  surface->effector_weights = NULL;

  BLI_remlink(&(surface->canvas->surfaces), surface);
  dynamicPaint_freeSurfaceData(surface);
  MEM_freeN(surface);
}

/* Free canvas data */
void dynamicPaint_freeCanvas(DynamicPaintModifierData *pmd)
{
  if (pmd->canvas) {
    /* Free surface data */
    DynamicPaintSurface *surface = pmd->canvas->surfaces.first;
    DynamicPaintSurface *next_surface = NULL;

    while (surface) {
      next_surface = surface->next;
      dynamicPaint_freeSurface(pmd, surface);
      surface = next_surface;
    }

    MEM_freeN(pmd->canvas);
    pmd->canvas = NULL;
  }
}

/* Free whole dp modifier */
void dynamicPaint_Modifier_free(DynamicPaintModifierData *pmd)
{
  if (pmd == NULL) {
    return;
  }
  dynamicPaint_freeCanvas(pmd);
  dynamicPaint_freeBrush(pmd);
  dynamicPaint_Modifier_free_runtime(pmd->modifier.runtime);
}

/***************************** Initialize and reset ******************************/

/*
 * Creates a new surface and adds it to the list
 * If scene is null, frame range of 1-250 is used
 * A pointer to this surface is returned
 */
DynamicPaintSurface *dynamicPaint_createNewSurface(DynamicPaintCanvasSettings *canvas,
                                                   Scene *scene)
{
  DynamicPaintSurface *surface = MEM_callocN(sizeof(DynamicPaintSurface), "DynamicPaintSurface");
  if (!surface) {
    return NULL;
  }

  surface->canvas = canvas;
  surface->format = MOD_DPAINT_SURFACE_F_VERTEX;
  surface->type = MOD_DPAINT_SURFACE_T_PAINT;

  /* cache */
  surface->pointcache = BKE_ptcache_add(&(surface->ptcaches));
  surface->pointcache->flag |= PTCACHE_DISK_CACHE;
  surface->pointcache->step = 1;

  /* Set initial values */
  surface->flags = MOD_DPAINT_ANTIALIAS | MOD_DPAINT_MULALPHA | MOD_DPAINT_DRY_LOG |
                   MOD_DPAINT_DISSOLVE_LOG | MOD_DPAINT_ACTIVE | MOD_DPAINT_OUT1 |
                   MOD_DPAINT_USE_DRYING;
  surface->effect = 0;
  surface->effect_ui = 1;

  surface->diss_speed = 250;
  surface->dry_speed = 500;
  surface->color_dry_threshold = 1.0f;
  surface->depth_clamp = 0.0f;
  surface->disp_factor = 1.0f;
  surface->disp_type = MOD_DPAINT_DISP_DISPLACE;
  surface->image_fileformat = MOD_DPAINT_IMGFORMAT_PNG;

  surface->influence_scale = 1.0f;
  surface->radius_scale = 1.0f;

  surface->init_color[0] = 1.0f;
  surface->init_color[1] = 1.0f;
  surface->init_color[2] = 1.0f;
  surface->init_color[3] = 1.0f;

  surface->image_resolution = 256;
  surface->substeps = 0;

  if (scene) {
    surface->start_frame = scene->r.sfra;
    surface->end_frame = scene->r.efra;
  }
  else {
    surface->start_frame = 1;
    surface->end_frame = 250;
  }

  surface->spread_speed = 1.0f;
  surface->color_spread_speed = 1.0f;
  surface->shrink_speed = 1.0f;

  surface->wave_damping = 0.04f;
  surface->wave_speed = 1.0f;
  surface->wave_timescale = 1.0f;
  surface->wave_spring = 0.20f;
  surface->wave_smoothness = 1.0f;

  modifier_path_init(
      surface->image_output_path, sizeof(surface->image_output_path), "cache_dynamicpaint");

  /* Using ID_BRUSH i18n context, as we have no physics/dpaint one for now... */
  dynamicPaintSurface_setUniqueName(surface, CTX_DATA_(BLT_I18NCONTEXT_ID_BRUSH, "Surface"));

  surface->effector_weights = BKE_effector_add_weights(NULL);

  dynamicPaintSurface_updateType(surface);

  BLI_addtail(&canvas->surfaces, surface);

  return surface;
}

/*
 * Initialize modifier data
 */
bool dynamicPaint_createType(struct DynamicPaintModifierData *pmd, int type, struct Scene *scene)
{
  if (pmd) {
    if (type == MOD_DYNAMICPAINT_TYPE_CANVAS) {
      DynamicPaintCanvasSettings *canvas;
      if (pmd->canvas) {
        dynamicPaint_freeCanvas(pmd);
      }

      canvas = pmd->canvas = MEM_callocN(sizeof(DynamicPaintCanvasSettings),
                                         "DynamicPaint Canvas");
      if (!canvas) {
        return false;
      }
      canvas->pmd = pmd;

      /* Create one surface */
      if (!dynamicPaint_createNewSurface(canvas, scene)) {
        return false;
      }
    }
    else if (type == MOD_DYNAMICPAINT_TYPE_BRUSH) {
      DynamicPaintBrushSettings *brush;
      if (pmd->brush) {
        dynamicPaint_freeBrush(pmd);
      }

      brush = pmd->brush = MEM_callocN(sizeof(DynamicPaintBrushSettings), "DynamicPaint Paint");
      if (!brush) {
        return false;
      }
      brush->pmd = pmd;

      brush->psys = NULL;

      brush->flags = MOD_DPAINT_ABS_ALPHA | MOD_DPAINT_RAMP_ALPHA;
      brush->collision = MOD_DPAINT_COL_VOLUME;

      brush->r = 0.15f;
      brush->g = 0.4f;
      brush->b = 0.8f;
      brush->alpha = 1.0f;
      brush->wetness = 1.0f;

      brush->paint_distance = 1.0f;
      brush->proximity_falloff = MOD_DPAINT_PRFALL_SMOOTH;

      brush->particle_radius = 0.2f;
      brush->particle_smooth = 0.05f;

      brush->wave_type = MOD_DPAINT_WAVEB_CHANGE;
      brush->wave_factor = 1.0f;
      brush->wave_clamp = 0.0f;
      brush->smudge_strength = 0.3f;
      brush->max_velocity = 1.0f;

      /* Paint proximity falloff colorramp. */
      {
        CBData *ramp;

        brush->paint_ramp = BKE_colorband_add(false);
        if (!brush->paint_ramp) {
          return false;
        }
        ramp = brush->paint_ramp->data;
        /* Add default smooth-falloff ramp. */
        ramp[0].r = ramp[0].g = ramp[0].b = ramp[0].a = 1.0f;
        ramp[0].pos = 0.0f;
        ramp[1].r = ramp[1].g = ramp[1].b = ramp[1].pos = 1.0f;
        ramp[1].a = 0.0f;
        pmd->brush->paint_ramp->tot = 2;
      }

      /* Brush velocity ramp. */
      {
        CBData *ramp;

        brush->vel_ramp = BKE_colorband_add(false);
        if (!brush->vel_ramp) {
          return false;
        }
        ramp = brush->vel_ramp->data;
        ramp[0].r = ramp[0].g = ramp[0].b = ramp[0].a = ramp[0].pos = 0.0f;
        ramp[1].r = ramp[1].g = ramp[1].b = ramp[1].a = ramp[1].pos = 1.0f;
        brush->paint_ramp->tot = 2;
      }
    }
  }
  else {
    return false;
  }

  return true;
}

void dynamicPaint_Modifier_copy(const struct DynamicPaintModifierData *pmd,
                                struct DynamicPaintModifierData *tpmd,
                                int flag)
{
  /* Init modifier */
  tpmd->type = pmd->type;
  if (pmd->canvas) {
    dynamicPaint_createType(tpmd, MOD_DYNAMICPAINT_TYPE_CANVAS, NULL);
  }
  if (pmd->brush) {
    dynamicPaint_createType(tpmd, MOD_DYNAMICPAINT_TYPE_BRUSH, NULL);
  }

  /* Copy data */
  if (tpmd->canvas) {
    DynamicPaintSurface *surface;
    tpmd->canvas->pmd = tpmd;
    /* free default surface */
    if (tpmd->canvas->surfaces.first) {
      dynamicPaint_freeSurface(tpmd, tpmd->canvas->surfaces.first);
    }

    /* copy existing surfaces */
    for (surface = pmd->canvas->surfaces.first; surface; surface = surface->next) {
      DynamicPaintSurface *t_surface = dynamicPaint_createNewSurface(tpmd->canvas, NULL);
      if (flag & LIB_ID_CREATE_NO_MAIN) {
        /* TODO(sergey): Consider passing some tips to the surface
         * creation to avoid this allocate-and-free cache behavior. */
        BKE_ptcache_free_list(&t_surface->ptcaches);
        tpmd->modifier.flag |= eModifierFlag_SharedCaches;
        t_surface->ptcaches = surface->ptcaches;
        t_surface->pointcache = surface->pointcache;
      }

      /* surface settings */
      t_surface->brush_group = surface->brush_group;
      MEM_freeN(t_surface->effector_weights);
      t_surface->effector_weights = MEM_dupallocN(surface->effector_weights);

      BLI_strncpy(t_surface->name, surface->name, sizeof(t_surface->name));
      t_surface->format = surface->format;
      t_surface->type = surface->type;
      t_surface->disp_type = surface->disp_type;
      t_surface->image_fileformat = surface->image_fileformat;
      t_surface->effect_ui = surface->effect_ui;
      t_surface->init_color_type = surface->init_color_type;
      t_surface->flags = surface->flags;
      t_surface->effect = surface->effect;

      t_surface->image_resolution = surface->image_resolution;
      t_surface->substeps = surface->substeps;
      t_surface->start_frame = surface->start_frame;
      t_surface->end_frame = surface->end_frame;

      copy_v4_v4(t_surface->init_color, surface->init_color);
      t_surface->init_texture = surface->init_texture;
      BLI_strncpy(
          t_surface->init_layername, surface->init_layername, sizeof(t_surface->init_layername));

      t_surface->dry_speed = surface->dry_speed;
      t_surface->diss_speed = surface->diss_speed;
      t_surface->color_dry_threshold = surface->color_dry_threshold;
      t_surface->depth_clamp = surface->depth_clamp;
      t_surface->disp_factor = surface->disp_factor;

      t_surface->spread_speed = surface->spread_speed;
      t_surface->color_spread_speed = surface->color_spread_speed;
      t_surface->shrink_speed = surface->shrink_speed;
      t_surface->drip_vel = surface->drip_vel;
      t_surface->drip_acc = surface->drip_acc;

      t_surface->influence_scale = surface->influence_scale;
      t_surface->radius_scale = surface->radius_scale;

      t_surface->wave_damping = surface->wave_damping;
      t_surface->wave_speed = surface->wave_speed;
      t_surface->wave_timescale = surface->wave_timescale;
      t_surface->wave_spring = surface->wave_spring;
      t_surface->wave_smoothness = surface->wave_smoothness;

      BLI_strncpy(t_surface->uvlayer_name, surface->uvlayer_name, sizeof(t_surface->uvlayer_name));
      BLI_strncpy(t_surface->image_output_path,
                  surface->image_output_path,
                  sizeof(t_surface->image_output_path));
      BLI_strncpy(t_surface->output_name, surface->output_name, sizeof(t_surface->output_name));
      BLI_strncpy(t_surface->output_name2, surface->output_name2, sizeof(t_surface->output_name2));
    }
  }
  else if (tpmd->brush) {
    DynamicPaintBrushSettings *brush = pmd->brush, *t_brush = tpmd->brush;
    t_brush->pmd = tpmd;

    t_brush->flags = brush->flags;
    t_brush->collision = brush->collision;

    t_brush->r = brush->r;
    t_brush->g = brush->g;
    t_brush->b = brush->b;
    t_brush->alpha = brush->alpha;
    t_brush->wetness = brush->wetness;

    t_brush->particle_radius = brush->particle_radius;
    t_brush->particle_smooth = brush->particle_smooth;
    t_brush->paint_distance = brush->paint_distance;
    t_brush->psys = brush->psys;

    if (brush->paint_ramp) {
      memcpy(t_brush->paint_ramp, brush->paint_ramp, sizeof(ColorBand));
    }
    if (brush->vel_ramp) {
      memcpy(t_brush->vel_ramp, brush->vel_ramp, sizeof(ColorBand));
    }

    t_brush->proximity_falloff = brush->proximity_falloff;
    t_brush->wave_type = brush->wave_type;
    t_brush->ray_dir = brush->ray_dir;

    t_brush->wave_factor = brush->wave_factor;
    t_brush->wave_clamp = brush->wave_clamp;
    t_brush->max_velocity = brush->max_velocity;
    t_brush->smudge_strength = brush->smudge_strength;
  }
}

/* allocates surface data depending on surface type */
static void dynamicPaint_allocateSurfaceType(DynamicPaintSurface *surface)
{
  PaintSurfaceData *sData = surface->data;

  switch (surface->type) {
    case MOD_DPAINT_SURFACE_T_PAINT:
      sData->type_data = MEM_callocN(sizeof(PaintPoint) * sData->total_points,
                                     "DynamicPaintSurface Data");
      break;
    case MOD_DPAINT_SURFACE_T_DISPLACE:
      sData->type_data = MEM_callocN(sizeof(float) * sData->total_points,
                                     "DynamicPaintSurface DepthData");
      break;
    case MOD_DPAINT_SURFACE_T_WEIGHT:
      sData->type_data = MEM_callocN(sizeof(float) * sData->total_points,
                                     "DynamicPaintSurface WeightData");
      break;
    case MOD_DPAINT_SURFACE_T_WAVE:
      sData->type_data = MEM_callocN(sizeof(PaintWavePoint) * sData->total_points,
                                     "DynamicPaintSurface WaveData");
      break;
  }

  if (sData->type_data == NULL) {
    setError(surface->canvas, N_("Not enough free memory"));
  }
}

static bool surface_usesAdjDistance(DynamicPaintSurface *surface)
{
  return ((surface->type == MOD_DPAINT_SURFACE_T_PAINT && surface->effect) ||
          (surface->type == MOD_DPAINT_SURFACE_T_WAVE));
}

static bool surface_usesAdjData(DynamicPaintSurface *surface)
{
  return (surface_usesAdjDistance(surface) || (surface->format == MOD_DPAINT_SURFACE_F_VERTEX &&
                                               surface->flags & MOD_DPAINT_ANTIALIAS));
}

/* initialize surface adjacency data */
static void dynamicPaint_initAdjacencyData(DynamicPaintSurface *surface, const bool force_init)
{
  PaintSurfaceData *sData = surface->data;
  Mesh *mesh = dynamicPaint_canvas_mesh_get(surface->canvas);
  PaintAdjData *ad;
  int *temp_data;
  int neigh_points = 0;

  if (!force_init && !surface_usesAdjData(surface)) {
    return;
  }

  if (surface->format == MOD_DPAINT_SURFACE_F_VERTEX) {
    /* For vertex format, neighbors are connected by edges */
    neigh_points = 2 * mesh->totedge;
  }
  else if (surface->format == MOD_DPAINT_SURFACE_F_IMAGESEQ) {
    neigh_points = sData->total_points * 8;
  }

  if (!neigh_points) {
    return;
  }

  /* allocate memory */
  ad = sData->adj_data = MEM_callocN(sizeof(PaintAdjData), "Surface Adj Data");
  if (!ad) {
    return;
  }
  ad->n_index = MEM_callocN(sizeof(int) * sData->total_points, "Surface Adj Index");
  ad->n_num = MEM_callocN(sizeof(int) * sData->total_points, "Surface Adj Counts");
  temp_data = MEM_callocN(sizeof(int) * sData->total_points, "Temp Adj Data");
  ad->n_target = MEM_callocN(sizeof(int) * neigh_points, "Surface Adj Targets");
  ad->flags = MEM_callocN(sizeof(int) * sData->total_points, "Surface Adj Flags");
  ad->total_targets = neigh_points;
  ad->border = NULL;
  ad->total_border = 0;

  /* in case of allocation error, free memory */
  if (!ad->n_index || !ad->n_num || !ad->n_target || !temp_data) {
    dynamicPaint_freeAdjData(sData);
    if (temp_data) {
      MEM_freeN(temp_data);
    }
    setError(surface->canvas, N_("Not enough free memory"));
    return;
  }

  if (surface->format == MOD_DPAINT_SURFACE_F_VERTEX) {
    int i;
    int n_pos;

    /* For vertex format, count every vertex that is connected by an edge */
    int numOfEdges = mesh->totedge;
    int numOfPolys = mesh->totpoly;
    struct MEdge *edge = mesh->medge;
    struct MPoly *mpoly = mesh->mpoly;
    struct MLoop *mloop = mesh->mloop;

    /* count number of edges per vertex */
    for (i = 0; i < numOfEdges; i++) {
      ad->n_num[edge[i].v1]++;
      ad->n_num[edge[i].v2]++;

      temp_data[edge[i].v1]++;
      temp_data[edge[i].v2]++;
    }

    /* also add number of vertices to temp_data
     * to locate points on "mesh edge" */
    for (i = 0; i < numOfPolys; i++) {
      for (int j = 0; j < mpoly[i].totloop; j++) {
        temp_data[mloop[mpoly[i].loopstart + j].v]++;
      }
    }

    /* now check if total number of edges+faces for
     * each vertex is even, if not -> vertex is on mesh edge */
    for (i = 0; i < sData->total_points; i++) {
      if ((temp_data[i] % 2) || (temp_data[i] < 4)) {
        ad->flags[i] |= ADJ_ON_MESH_EDGE;
      }

      /* reset temp data */
      temp_data[i] = 0;
    }

    /* order n_index array */
    n_pos = 0;
    for (i = 0; i < sData->total_points; i++) {
      ad->n_index[i] = n_pos;
      n_pos += ad->n_num[i];
    }

    /* and now add neighbor data using that info */
    for (i = 0; i < numOfEdges; i++) {
      /* first vertex */
      int index = edge[i].v1;
      n_pos = ad->n_index[index] + temp_data[index];
      ad->n_target[n_pos] = edge[i].v2;
      temp_data[index]++;

      /* second vertex */
      index = edge[i].v2;
      n_pos = ad->n_index[index] + temp_data[index];
      ad->n_target[n_pos] = edge[i].v1;
      temp_data[index]++;
    }
  }
  else if (surface->format == MOD_DPAINT_SURFACE_F_IMAGESEQ) {
    /* for image sequences, only allocate memory.
     * bake initialization takes care of rest */
  }

  MEM_freeN(temp_data);
}

typedef struct DynamicPaintSetInitColorData {
  const DynamicPaintSurface *surface;

  const MLoop *mloop;
  const MLoopUV *mloopuv;
  const MLoopTri *mlooptri;
  const MLoopCol *mloopcol;
  struct ImagePool *pool;

  const bool scene_color_manage;
} DynamicPaintSetInitColorData;

static void dynamic_paint_set_init_color_tex_to_vcol_cb(
    void *__restrict userdata, const int i, const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintSetInitColorData *data = userdata;

  const PaintSurfaceData *sData = data->surface->data;
  PaintPoint *pPoint = (PaintPoint *)sData->type_data;

  const MLoop *mloop = data->mloop;
  const MLoopTri *mlooptri = data->mlooptri;
  const MLoopUV *mloopuv = data->mloopuv;
  struct ImagePool *pool = data->pool;
  Tex *tex = data->surface->init_texture;

  const bool scene_color_manage = data->scene_color_manage;

  float uv[3] = {0.0f};

  for (int j = 3; j--;) {
    TexResult texres = {0};
    const unsigned int vert = mloop[mlooptri[i].tri[j]].v;

    /* remap to [-1.0, 1.0] */
    uv[0] = mloopuv[mlooptri[i].tri[j]].uv[0] * 2.0f - 1.0f;
    uv[1] = mloopuv[mlooptri[i].tri[j]].uv[1] * 2.0f - 1.0f;

    multitex_ext_safe(tex, uv, &texres, pool, scene_color_manage, false);

    if (texres.tin > pPoint[vert].color[3]) {
      copy_v3_v3(pPoint[vert].color, &texres.tr);
      pPoint[vert].color[3] = texres.tin;
    }
  }
}

static void dynamic_paint_set_init_color_tex_to_imseq_cb(
    void *__restrict userdata, const int i, const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintSetInitColorData *data = userdata;

  const PaintSurfaceData *sData = data->surface->data;
  PaintPoint *pPoint = (PaintPoint *)sData->type_data;

  const MLoopTri *mlooptri = data->mlooptri;
  const MLoopUV *mloopuv = data->mloopuv;
  Tex *tex = data->surface->init_texture;
  ImgSeqFormatData *f_data = (ImgSeqFormatData *)sData->format_data;
  const int samples = (data->surface->flags & MOD_DPAINT_ANTIALIAS) ? 5 : 1;

  const bool scene_color_manage = data->scene_color_manage;

  float uv[9] = {0.0f};
  float uv_final[3] = {0.0f};

  TexResult texres = {0};

  /* collect all uvs */
  for (int j = 3; j--;) {
    copy_v2_v2(&uv[j * 3], mloopuv[mlooptri[f_data->uv_p[i].tri_index].tri[j]].uv);
  }

  /* interpolate final uv pos */
  interp_v3_v3v3v3(uv_final, &uv[0], &uv[3], &uv[6], f_data->barycentricWeights[i * samples].v);
  /* remap to [-1.0, 1.0] */
  uv_final[0] = uv_final[0] * 2.0f - 1.0f;
  uv_final[1] = uv_final[1] * 2.0f - 1.0f;

  multitex_ext_safe(tex, uv_final, &texres, NULL, scene_color_manage, false);

  /* apply color */
  copy_v3_v3(pPoint[i].color, &texres.tr);
  pPoint[i].color[3] = texres.tin;
}

static void dynamic_paint_set_init_color_vcol_to_imseq_cb(
    void *__restrict userdata, const int i, const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintSetInitColorData *data = userdata;

  const PaintSurfaceData *sData = data->surface->data;
  PaintPoint *pPoint = (PaintPoint *)sData->type_data;

  const MLoopTri *mlooptri = data->mlooptri;
  const MLoopCol *mloopcol = data->mloopcol;
  ImgSeqFormatData *f_data = (ImgSeqFormatData *)sData->format_data;
  const int samples = (data->surface->flags & MOD_DPAINT_ANTIALIAS) ? 5 : 1;

  const int tri_idx = f_data->uv_p[i].tri_index;
  float colors[3][4];
  float final_color[4];

  /* collect color values */
  for (int j = 3; j--;) {
    rgba_uchar_to_float(colors[j], (const unsigned char *)&mloopcol[mlooptri[tri_idx].tri[j]].r);
  }

  /* interpolate final color */
  interp_v4_v4v4v4(final_color, UNPACK3(colors), f_data->barycentricWeights[i * samples].v);

  copy_v4_v4(pPoint[i].color, final_color);
}

static void dynamicPaint_setInitialColor(const Scene *scene, DynamicPaintSurface *surface)
{
  PaintSurfaceData *sData = surface->data;
  PaintPoint *pPoint = (PaintPoint *)sData->type_data;
  Mesh *mesh = dynamicPaint_canvas_mesh_get(surface->canvas);
  int i;
  const bool scene_color_manage = BKE_scene_check_color_management_enabled(scene);

  if (surface->type != MOD_DPAINT_SURFACE_T_PAINT) {
    return;
  }

  if (surface->init_color_type == MOD_DPAINT_INITIAL_NONE) {
    return;
  }

  /* Single color */
  if (surface->init_color_type == MOD_DPAINT_INITIAL_COLOR) {
    /* apply color to every surface point */
    for (i = 0; i < sData->total_points; i++) {
      copy_v4_v4(pPoint[i].color, surface->init_color);
    }
  }
  /* UV mapped texture */
  else if (surface->init_color_type == MOD_DPAINT_INITIAL_TEXTURE) {
    Tex *tex = surface->init_texture;

    const MLoop *mloop = mesh->mloop;
    const MLoopTri *mlooptri = BKE_mesh_runtime_looptri_ensure(mesh);
    const int tottri = BKE_mesh_runtime_looptri_len(mesh);
    const MLoopUV *mloopuv = NULL;

    char uvname[MAX_CUSTOMDATA_LAYER_NAME];

    if (!tex) {
      return;
    }

    /* get uv map */
    CustomData_validate_layer_name(&mesh->ldata, CD_MLOOPUV, surface->init_layername, uvname);
    mloopuv = CustomData_get_layer_named(&mesh->ldata, CD_MLOOPUV, uvname);
    if (!mloopuv) {
      return;
    }

    /* for vertex surface loop through tfaces and find uv color
     * that provides highest alpha */
    if (surface->format == MOD_DPAINT_SURFACE_F_VERTEX) {
      struct ImagePool *pool = BKE_image_pool_new();

      DynamicPaintSetInitColorData data = {
          .surface = surface,
          .mloop = mloop,
          .mlooptri = mlooptri,
          .mloopuv = mloopuv,
          .pool = pool,
          .scene_color_manage = scene_color_manage,
      };
      ParallelRangeSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      settings.use_threading = (tottri > 1000);
      BLI_task_parallel_range(
          0, tottri, &data, dynamic_paint_set_init_color_tex_to_vcol_cb, &settings);
      BKE_image_pool_free(pool);
    }
    else if (surface->format == MOD_DPAINT_SURFACE_F_IMAGESEQ) {
      DynamicPaintSetInitColorData data = {
          .surface = surface,
          .mlooptri = mlooptri,
          .mloopuv = mloopuv,
          .scene_color_manage = scene_color_manage,
      };
      ParallelRangeSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      settings.use_threading = (sData->total_points > 1000);
      BLI_task_parallel_range(
          0, sData->total_points, &data, dynamic_paint_set_init_color_tex_to_imseq_cb, &settings);
    }
  }
  /* vertex color layer */
  else if (surface->init_color_type == MOD_DPAINT_INITIAL_VERTEXCOLOR) {

    /* for vertex surface, just copy colors from mcol */
    if (surface->format == MOD_DPAINT_SURFACE_F_VERTEX) {
      const MLoop *mloop = mesh->mloop;
      const int totloop = mesh->totloop;
      const MLoopCol *col = CustomData_get_layer_named(
          &mesh->ldata, CD_MLOOPCOL, surface->init_layername);
      if (!col) {
        return;
      }

      for (i = 0; i < totloop; i++) {
        rgba_uchar_to_float(pPoint[mloop[i].v].color, (const unsigned char *)&col[mloop[i].v].r);
      }
    }
    else if (surface->format == MOD_DPAINT_SURFACE_F_IMAGESEQ) {
      const MLoopTri *mlooptri = BKE_mesh_runtime_looptri_ensure(mesh);
      MLoopCol *col = CustomData_get_layer_named(
          &mesh->ldata, CD_MLOOPCOL, surface->init_layername);
      if (!col) {
        return;
      }

      DynamicPaintSetInitColorData data = {
          .surface = surface,
          .mlooptri = mlooptri,
          .mloopcol = col,
      };
      ParallelRangeSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      settings.use_threading = (sData->total_points > 1000);
      BLI_task_parallel_range(
          0, sData->total_points, &data, dynamic_paint_set_init_color_vcol_to_imseq_cb, &settings);
    }
  }
}

/* clears surface data back to zero */
void dynamicPaint_clearSurface(const Scene *scene, DynamicPaintSurface *surface)
{
  PaintSurfaceData *sData = surface->data;
  if (sData && sData->type_data) {
    unsigned int data_size;

    if (surface->type == MOD_DPAINT_SURFACE_T_PAINT) {
      data_size = sizeof(PaintPoint);
    }
    else if (surface->type == MOD_DPAINT_SURFACE_T_WAVE) {
      data_size = sizeof(PaintWavePoint);
    }
    else {
      data_size = sizeof(float);
    }

    memset(sData->type_data, 0, data_size * sData->total_points);

    /* set initial color */
    if (surface->type == MOD_DPAINT_SURFACE_T_PAINT) {
      dynamicPaint_setInitialColor(scene, surface);
    }

    if (sData->bData) {
      sData->bData->clear = 1;
    }
  }
}

/* completely (re)initializes surface (only for point cache types)*/
bool dynamicPaint_resetSurface(const Scene *scene, DynamicPaintSurface *surface)
{
  int numOfPoints = dynamicPaint_surfaceNumOfPoints(surface);
  /* free existing data */
  if (surface->data) {
    dynamicPaint_freeSurfaceData(surface);
  }

  /* don't reallocate for image sequence types. they get handled only on bake */
  if (surface->format == MOD_DPAINT_SURFACE_F_IMAGESEQ) {
    return true;
  }
  if (numOfPoints < 1) {
    return false;
  }

  /* allocate memory */
  surface->data = MEM_callocN(sizeof(PaintSurfaceData), "PaintSurfaceData");
  if (!surface->data) {
    return false;
  }

  /* allocate data depending on surface type and format */
  surface->data->total_points = numOfPoints;
  dynamicPaint_allocateSurfaceType(surface);
  dynamicPaint_initAdjacencyData(surface, false);

  /* set initial color */
  if (surface->type == MOD_DPAINT_SURFACE_T_PAINT) {
    dynamicPaint_setInitialColor(scene, surface);
  }

  return true;
}

/* make sure allocated surface size matches current requirements */
static bool dynamicPaint_checkSurfaceData(const Scene *scene, DynamicPaintSurface *surface)
{
  if (!surface->data ||
      ((dynamicPaint_surfaceNumOfPoints(surface) != surface->data->total_points))) {
    return dynamicPaint_resetSurface(scene, surface);
  }
  return true;
}

/***************************** Modifier processing ******************************/

typedef struct DynamicPaintModifierApplyData {
  const DynamicPaintSurface *surface;
  Object *ob;

  MVert *mvert;
  const MLoop *mloop;
  const MPoly *mpoly;

  float (*fcolor)[4];
  MLoopCol *mloopcol;
  MLoopCol *mloopcol_wet;
} DynamicPaintModifierApplyData;

static void dynamic_paint_apply_surface_displace_cb(void *__restrict userdata,
                                                    const int i,
                                                    const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintModifierApplyData *data = userdata;

  const DynamicPaintSurface *surface = data->surface;
  MVert *mvert = data->mvert;

  float normal[3];
  const float *value = (float *)surface->data->type_data;
  const float val = value[i] * surface->disp_factor;

  normal_short_to_float_v3(normal, mvert[i].no);

  /* same as 'mvert[i].co[0] -= normal[0] * val' etc. */
  madd_v3_v3fl(mvert[i].co, normal, -val);
}

/* apply displacing vertex surface to the derived mesh */
static void dynamicPaint_applySurfaceDisplace(DynamicPaintSurface *surface, Mesh *result)
{
  PaintSurfaceData *sData = surface->data;

  if (!sData || surface->format != MOD_DPAINT_SURFACE_F_VERTEX) {
    return;
  }

  /* displace paint */
  if (surface->type == MOD_DPAINT_SURFACE_T_DISPLACE) {
    MVert *mvert = result->mvert;

    DynamicPaintModifierApplyData data = {
        .surface = surface,
        .mvert = mvert,
    };
    ParallelRangeSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    settings.use_threading = (sData->total_points > 10000);
    BLI_task_parallel_range(
        0, sData->total_points, &data, dynamic_paint_apply_surface_displace_cb, &settings);
  }
}

static void dynamic_paint_apply_surface_vpaint_blend_cb(
    void *__restrict userdata, const int i, const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintModifierApplyData *data = userdata;

  PaintPoint *pPoint = (PaintPoint *)data->surface->data->type_data;
  float(*fcolor)[4] = data->fcolor;

  /* blend dry and wet layer */
  blendColors(
      pPoint[i].color, pPoint[i].color[3], pPoint[i].e_color, pPoint[i].e_color[3], fcolor[i]);
}

static void dynamic_paint_apply_surface_vpaint_cb(void *__restrict userdata,
                                                  const int p_index,
                                                  const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintModifierApplyData *data = userdata;

  const MLoop *mloop = data->mloop;
  const MPoly *mpoly = data->mpoly;

  const DynamicPaintSurface *surface = data->surface;
  PaintPoint *pPoint = (PaintPoint *)surface->data->type_data;
  float(*fcolor)[4] = data->fcolor;

  MLoopCol *mloopcol = data->mloopcol;
  MLoopCol *mloopcol_wet = data->mloopcol_wet;

  for (int j = 0; j < mpoly[p_index].totloop; j++) {
    const int l_index = mpoly[p_index].loopstart + j;
    const int v_index = mloop[l_index].v;

    /* save layer data to output layer */
    /* apply color */
    if (mloopcol) {
      rgba_float_to_uchar((unsigned char *)&mloopcol[l_index].r, fcolor[v_index]);
    }
    /* apply wetness */
    if (mloopcol_wet) {
      const char c = unit_float_to_uchar_clamp(pPoint[v_index].wetness);
      mloopcol_wet[l_index].r = c;
      mloopcol_wet[l_index].g = c;
      mloopcol_wet[l_index].b = c;
      mloopcol_wet[l_index].a = 255;
    }
  }
}

static void dynamic_paint_apply_surface_wave_cb(void *__restrict userdata,
                                                const int i,
                                                const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintModifierApplyData *data = userdata;

  PaintWavePoint *wPoint = (PaintWavePoint *)data->surface->data->type_data;
  MVert *mvert = data->mvert;
  float normal[3];

  normal_short_to_float_v3(normal, mvert[i].no);
  madd_v3_v3fl(mvert[i].co, normal, wPoint[i].height);
}

/*
 * Apply canvas data to the object derived mesh
 */
static Mesh *dynamicPaint_Modifier_apply(DynamicPaintModifierData *pmd, Object *ob, Mesh *mesh)
{
  Mesh *result = BKE_mesh_copy_for_eval(mesh, false);

  if (pmd->canvas && !(pmd->canvas->flags & MOD_DPAINT_BAKING)) {

    DynamicPaintSurface *surface;
    bool update_normals = false;

    /* loop through surfaces */
    for (surface = pmd->canvas->surfaces.first; surface; surface = surface->next) {
      PaintSurfaceData *sData = surface->data;

      if (surface->format != MOD_DPAINT_SURFACE_F_IMAGESEQ && sData) {
        if (!(surface->flags & MOD_DPAINT_ACTIVE)) {
          continue;
        }

        /* process vertex surface previews */
        if (surface->format == MOD_DPAINT_SURFACE_F_VERTEX) {

          /* vertex color paint */
          if (surface->type == MOD_DPAINT_SURFACE_T_PAINT) {
            MLoop *mloop = result->mloop;
            const int totloop = result->totloop;
            MPoly *mpoly = result->mpoly;
            const int totpoly = result->totpoly;

            /* paint is stored on dry and wet layers, so mix final color first */
            float(*fcolor)[4] = MEM_callocN(sizeof(*fcolor) * sData->total_points,
                                            "Temp paint color");

            DynamicPaintModifierApplyData data = {
                .surface = surface,
                .fcolor = fcolor,
            };
            {
              ParallelRangeSettings settings;
              BLI_parallel_range_settings_defaults(&settings);
              settings.use_threading = (sData->total_points > 1000);
              BLI_task_parallel_range(0,
                                      sData->total_points,
                                      &data,
                                      dynamic_paint_apply_surface_vpaint_blend_cb,
                                      &settings);
            }

            /* paint layer */
            MLoopCol *mloopcol = CustomData_get_layer_named(
                &result->ldata, CD_MLOOPCOL, surface->output_name);
            /* if output layer is lost from a constructive modifier, re-add it */
            if (!mloopcol && dynamicPaint_outputLayerExists(surface, ob, 0)) {
              mloopcol = CustomData_add_layer_named(
                  &result->ldata, CD_MLOOPCOL, CD_CALLOC, NULL, totloop, surface->output_name);
            }

            /* wet layer */
            MLoopCol *mloopcol_wet = CustomData_get_layer_named(
                &result->ldata, CD_MLOOPCOL, surface->output_name2);
            /* if output layer is lost from a constructive modifier, re-add it */
            if (!mloopcol_wet && dynamicPaint_outputLayerExists(surface, ob, 1)) {
              mloopcol_wet = CustomData_add_layer_named(
                  &result->ldata, CD_MLOOPCOL, CD_CALLOC, NULL, totloop, surface->output_name2);
            }

            data.ob = ob;
            data.mloop = mloop;
            data.mpoly = mpoly;
            data.mloopcol = mloopcol;
            data.mloopcol_wet = mloopcol_wet;

            {
              ParallelRangeSettings settings;
              BLI_parallel_range_settings_defaults(&settings);
              settings.use_threading = (totpoly > 1000);
              BLI_task_parallel_range(
                  0, totpoly, &data, dynamic_paint_apply_surface_vpaint_cb, &settings);
            }

            MEM_freeN(fcolor);

            /* Mark tessellated CD layers as dirty. */
            //result->dirty |= DM_DIRTY_TESS_CDLAYERS;
          }
          /* vertex group paint */
          else if (surface->type == MOD_DPAINT_SURFACE_T_WEIGHT) {
            int defgrp_index = defgroup_name_index(ob, surface->output_name);
            MDeformVert *dvert = CustomData_get_layer(&result->vdata, CD_MDEFORMVERT);
            float *weight = (float *)sData->type_data;

            /* apply weights into a vertex group, if doesn't exists add a new layer */
            if (defgrp_index != -1 && !dvert && (surface->output_name[0] != '\0')) {
              dvert = CustomData_add_layer(
                  &result->vdata, CD_MDEFORMVERT, CD_CALLOC, NULL, sData->total_points);
            }
            if (defgrp_index != -1 && dvert) {
              int i;
              for (i = 0; i < sData->total_points; i++) {
                MDeformVert *dv = &dvert[i];
                MDeformWeight *def_weight = defvert_find_index(dv, defgrp_index);

                /* skip if weight value is 0 and no existing weight is found */
                if ((def_weight != NULL) || (weight[i] != 0.0f)) {
                  /* if not found, add a weight for it */
                  if (def_weight == NULL) {
                    def_weight = defvert_verify_index(dv, defgrp_index);
                  }

                  /* set weight value */
                  def_weight->weight = weight[i];
                }
              }
            }
          }
          /* wave simulation */
          else if (surface->type == MOD_DPAINT_SURFACE_T_WAVE) {
            MVert *mvert = result->mvert;

            DynamicPaintModifierApplyData data = {
                .surface = surface,
                .mvert = mvert,
            };
            ParallelRangeSettings settings;
            BLI_parallel_range_settings_defaults(&settings);
            settings.use_threading = (sData->total_points > 1000);
            BLI_task_parallel_range(
                0, sData->total_points, &data, dynamic_paint_apply_surface_wave_cb, &settings);
            update_normals = true;
          }

          /* displace */
          if (surface->type == MOD_DPAINT_SURFACE_T_DISPLACE) {
            dynamicPaint_applySurfaceDisplace(surface, result);
            update_normals = true;
          }
        }
      }
    }

    if (update_normals) {
      //result->dirty |= DM_DIRTY_NORMALS;
    }
  }
  /* make a copy of mesh to use as brush data */
  if (pmd->brush) {
    DynamicPaintRuntime *runtime_data = dynamicPaint_Modifier_runtime_ensure(pmd);
    if (runtime_data->brush_mesh != NULL) {
      BKE_id_free(NULL, runtime_data->brush_mesh);
    }
    runtime_data->brush_mesh = BKE_mesh_copy_for_eval(result, false);
  }

  return result;
}

/* update cache frame range */
void dynamicPaint_cacheUpdateFrames(DynamicPaintSurface *surface)
{
  if (surface->pointcache) {
    surface->pointcache->startframe = surface->start_frame;
    surface->pointcache->endframe = surface->end_frame;
  }
}

static void canvas_copyMesh(DynamicPaintCanvasSettings *canvas, Mesh *mesh)
{
  DynamicPaintRuntime *runtime = dynamicPaint_Modifier_runtime_ensure(canvas->pmd);
  if (runtime->canvas_mesh != NULL) {
    BKE_id_free(NULL, runtime->canvas_mesh);
  }

  runtime->canvas_mesh = BKE_mesh_copy_for_eval(mesh, false);
}

/*
 * Updates derived mesh copy and processes dynamic paint step / caches.
 */
static void dynamicPaint_frameUpdate(DynamicPaintModifierData *pmd,
                                     struct Depsgraph *depsgraph,
                                     Scene *scene,
                                     Object *ob,
                                     Mesh *mesh)
{
  if (pmd->canvas) {
    DynamicPaintCanvasSettings *canvas = pmd->canvas;
    DynamicPaintSurface *surface = canvas->surfaces.first;

    /* update derived mesh copy */
    canvas_copyMesh(canvas, mesh);

    /* in case image sequence baking, stop here */
    if (canvas->flags & MOD_DPAINT_BAKING) {
      return;
    }

    /* loop through surfaces */
    for (; surface; surface = surface->next) {
      int current_frame = (int)scene->r.cfra;
      bool no_surface_data;

      /* free bake data if not required anymore */
      surface_freeUnusedData(surface);

      /* image sequences are handled by bake operator */
      if ((surface->format == MOD_DPAINT_SURFACE_F_IMAGESEQ) ||
          !(surface->flags & MOD_DPAINT_ACTIVE)) {
        continue;
      }

      /* make sure surface is valid */
      no_surface_data = surface->data == NULL;
      if (!dynamicPaint_checkSurfaceData(scene, surface)) {
        continue;
      }

      /* limit frame range */
      CLAMP(current_frame, surface->start_frame, surface->end_frame);

      if (no_surface_data || current_frame != surface->current_frame ||
          (int)scene->r.cfra == surface->start_frame) {
        PointCache *cache = surface->pointcache;
        PTCacheID pid;
        surface->current_frame = current_frame;

        /* read point cache */
        BKE_ptcache_id_from_dynamicpaint(&pid, ob, surface);
        pid.cache->startframe = surface->start_frame;
        pid.cache->endframe = surface->end_frame;
        BKE_ptcache_id_time(&pid, scene, (float)scene->r.cfra, NULL, NULL, NULL);

        /* reset non-baked cache at first frame */
        if ((int)scene->r.cfra == surface->start_frame && !(cache->flag & PTCACHE_BAKED)) {
          cache->flag |= PTCACHE_REDO_NEEDED;
          BKE_ptcache_id_reset(scene, &pid, PTCACHE_RESET_OUTDATED);
          cache->flag &= ~PTCACHE_REDO_NEEDED;
        }

        /* try to read from cache */
        bool can_simulate = ((int)scene->r.cfra == current_frame) &&
                            !(cache->flag & PTCACHE_BAKED);

        if (BKE_ptcache_read(&pid, (float)scene->r.cfra, can_simulate)) {
          BKE_ptcache_validate(cache, (int)scene->r.cfra);
        }
        /* if read failed and we're on surface range do recalculate */
        else if (can_simulate) {
          /* calculate surface frame */
          canvas->flags |= MOD_DPAINT_BAKING;
          dynamicPaint_calculateFrame(surface, depsgraph, scene, ob, current_frame);
          canvas->flags &= ~MOD_DPAINT_BAKING;

          /* restore canvas mesh if required */
          if (surface->type == MOD_DPAINT_SURFACE_T_DISPLACE &&
              surface->flags & MOD_DPAINT_DISP_INCREMENTAL && surface->next) {
            canvas_copyMesh(canvas, mesh);
          }

          BKE_ptcache_validate(cache, surface->current_frame);
          BKE_ptcache_write(&pid, surface->current_frame);
        }
      }
    }
  }
}

/* Modifier call. Processes dynamic paint modifier step. */
Mesh *dynamicPaint_Modifier_do(DynamicPaintModifierData *pmd,
                               struct Depsgraph *depsgraph,
                               Scene *scene,
                               Object *ob,
                               Mesh *mesh)
{
  if (pmd->canvas) {
    Mesh *ret;

    /* Update canvas data for a new frame */
    dynamicPaint_frameUpdate(pmd, depsgraph, scene, ob, mesh);

    /* Return output mesh */
    ret = dynamicPaint_Modifier_apply(pmd, ob, mesh);

    return ret;
  }
  else {
    /* Update canvas data for a new frame */
    dynamicPaint_frameUpdate(pmd, depsgraph, scene, ob, mesh);

    /* Return output mesh */
    return dynamicPaint_Modifier_apply(pmd, ob, mesh);
  }
}

/***************************** Image Sequence / UV Image Surface Calls ******************************/

/*
 * Create a surface for uv image sequence format
 */
#define JITTER_SAMPLES \
  { \
    0.0f, 0.0f, -0.2f, -0.4f, 0.2f, 0.4f, 0.4f, -0.2f, -0.4f, 0.3f, \
  }

typedef struct DynamicPaintCreateUVSurfaceData {
  const DynamicPaintSurface *surface;

  PaintUVPoint *tempPoints;
  Vec3f *tempWeights;

  const MLoopTri *mlooptri;
  const MLoopUV *mloopuv;
  const MLoop *mloop;
  const int tottri;

  const Bounds2D *faceBB;
  uint32_t *active_points;
} DynamicPaintCreateUVSurfaceData;

static void dynamic_paint_create_uv_surface_direct_cb(
    void *__restrict userdata, const int ty, const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintCreateUVSurfaceData *data = userdata;

  const DynamicPaintSurface *surface = data->surface;
  PaintUVPoint *tempPoints = data->tempPoints;
  Vec3f *tempWeights = data->tempWeights;

  const MLoopTri *mlooptri = data->mlooptri;
  const MLoopUV *mloopuv = data->mloopuv;
  const MLoop *mloop = data->mloop;
  const int tottri = data->tottri;

  const Bounds2D *faceBB = data->faceBB;

  const float jitter5sample[10] = JITTER_SAMPLES;
  const int aa_samples = (surface->flags & MOD_DPAINT_ANTIALIAS) ? 5 : 1;
  const int w = surface->image_resolution;
  const int h = w;

  for (int tx = 0; tx < w; tx++) {
    const int index = tx + w * ty;
    PaintUVPoint *tPoint = &tempPoints[index];
    float point[5][2];

    /* Init per pixel settings */
    tPoint->tri_index = -1;
    tPoint->neighbour_pixel = -1;
    tPoint->pixel_index = index;

    /* Actual pixel center, used when collision is found */
    point[0][0] = ((float)tx + 0.5f) / w;
    point[0][1] = ((float)ty + 0.5f) / h;

    /*
     * A pixel middle sample isn't enough to find very narrow polygons
     * So using 4 samples of each corner too
     */
    point[1][0] = ((float)tx) / w;
    point[1][1] = ((float)ty) / h;

    point[2][0] = ((float)tx + 1) / w;
    point[2][1] = ((float)ty) / h;

    point[3][0] = ((float)tx) / w;
    point[3][1] = ((float)ty + 1) / h;

    point[4][0] = ((float)tx + 1) / w;
    point[4][1] = ((float)ty + 1) / h;

    /* Loop through samples, starting from middle point */
    for (int sample = 0; sample < 5; sample++) {
      /* Loop through every face in the mesh */
      /* XXX TODO This is *horrible* with big meshes, should use a 2D BVHTree over UV tris here! */
      for (int i = 0; i < tottri; i++) {
        /* Check uv bb */
        if ((faceBB[i].min[0] > point[sample][0]) || (faceBB[i].min[1] > point[sample][1]) ||
            (faceBB[i].max[0] < point[sample][0]) || (faceBB[i].max[1] < point[sample][1])) {
          continue;
        }

        const float *uv1 = mloopuv[mlooptri[i].tri[0]].uv;
        const float *uv2 = mloopuv[mlooptri[i].tri[1]].uv;
        const float *uv3 = mloopuv[mlooptri[i].tri[2]].uv;

        /* If point is inside the face */
        if (isect_point_tri_v2(point[sample], uv1, uv2, uv3) != 0) {
          float uv[2];

          /* Add b-weights per anti-aliasing sample */
          for (int j = 0; j < aa_samples; j++) {
            uv[0] = point[0][0] + jitter5sample[j * 2] / w;
            uv[1] = point[0][1] + jitter5sample[j * 2 + 1] / h;

            barycentric_weights_v2(uv1, uv2, uv3, uv, tempWeights[index * aa_samples + j].v);
          }

          /* Set surface point face values */
          tPoint->tri_index = i;

          /* save vertex indexes */
          tPoint->v1 = mloop[mlooptri[i].tri[0]].v;
          tPoint->v2 = mloop[mlooptri[i].tri[1]].v;
          tPoint->v3 = mloop[mlooptri[i].tri[2]].v;

          sample = 5; /* make sure we exit sample loop as well */
          break;
        }
      }
    }
  }
}

static void dynamic_paint_create_uv_surface_neighbor_cb(
    void *__restrict userdata, const int ty, const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintCreateUVSurfaceData *data = userdata;

  const DynamicPaintSurface *surface = data->surface;
  PaintUVPoint *tempPoints = data->tempPoints;
  Vec3f *tempWeights = data->tempWeights;

  const MLoopTri *mlooptri = data->mlooptri;
  const MLoopUV *mloopuv = data->mloopuv;
  const MLoop *mloop = data->mloop;

  uint32_t *active_points = data->active_points;

  const float jitter5sample[10] = JITTER_SAMPLES;
  const int aa_samples = (surface->flags & MOD_DPAINT_ANTIALIAS) ? 5 : 1;
  const int w = surface->image_resolution;
  const int h = w;

  for (int tx = 0; tx < w; tx++) {
    const int index = tx + w * ty;
    PaintUVPoint *tPoint = &tempPoints[index];

    /* If point isn't on canvas mesh */
    if (tPoint->tri_index == -1) {
      float point[2];

      /* get loop area */
      const int u_min = (tx > 0) ? -1 : 0;
      const int u_max = (tx < (w - 1)) ? 1 : 0;
      const int v_min = (ty > 0) ? -1 : 0;
      const int v_max = (ty < (h - 1)) ? 1 : 0;

      point[0] = ((float)tx + 0.5f) / w;
      point[1] = ((float)ty + 0.5f) / h;

      /* search through defined area for neighbor, checking grid directions first */
      for (int ni = 0; ni < 8; ni++) {
        int u = neighStraightX[ni];
        int v = neighStraightY[ni];

        if (u >= u_min && u <= u_max && v >= v_min && v <= v_max) {
          /* if not this pixel itself */
          if (u != 0 || v != 0) {
            const int ind = (tx + u) + w * (ty + v);

            /* if neighbor has index */
            if (tempPoints[ind].neighbour_pixel == -1 && tempPoints[ind].tri_index != -1) {
              float uv[2];
              const int i = tempPoints[ind].tri_index;
              const float *uv1 = mloopuv[mlooptri[i].tri[0]].uv;
              const float *uv2 = mloopuv[mlooptri[i].tri[1]].uv;
              const float *uv3 = mloopuv[mlooptri[i].tri[2]].uv;

              /* tri index */
              /* There is a low possibility of actually having a neighbor point which tri is
               * already set from another neighbor in a separate thread here.
               * Checking for both tri_index and neighbour_pixel above reduces that probability
               * but it remains possible.
               * That atomic op (and its memory fence) ensures tPoint->neighbour_pixel is set
               * to non--1 *before* its tri_index is set (i.e. that it cannot be used a neighbour).
               */
              tPoint->neighbour_pixel = ind - 1;
              atomic_add_and_fetch_uint32(&tPoint->neighbour_pixel, 1);
              tPoint->tri_index = i;

              /* Now calculate pixel data for this pixel as it was on polygon surface */
              /* Add b-weights per anti-aliasing sample */
              for (int j = 0; j < aa_samples; j++) {
                uv[0] = point[0] + jitter5sample[j * 2] / w;
                uv[1] = point[1] + jitter5sample[j * 2 + 1] / h;
                barycentric_weights_v2(uv1, uv2, uv3, uv, tempWeights[index * aa_samples + j].v);
              }

              /* save vertex indexes */
              tPoint->v1 = mloop[mlooptri[i].tri[0]].v;
              tPoint->v2 = mloop[mlooptri[i].tri[1]].v;
              tPoint->v3 = mloop[mlooptri[i].tri[2]].v;

              break;
            }
          }
        }
      }
    }

    /* Increase the final number of active surface points if relevant. */
    if (tPoint->tri_index != -1) {
      atomic_add_and_fetch_uint32(active_points, 1);
    }
  }
}

#undef JITTER_SAMPLES

static float dist_squared_to_looptri_uv_edges(const MLoopTri *mlooptri,
                                              const MLoopUV *mloopuv,
                                              int tri_index,
                                              const float point[2])
{
  float min_distance = FLT_MAX;

  for (int i = 0; i < 3; i++) {
    const float dist_squared = dist_squared_to_line_segment_v2(
        point,
        mloopuv[mlooptri[tri_index].tri[(i + 0)]].uv,
        mloopuv[mlooptri[tri_index].tri[(i + 1) % 3]].uv);

    if (dist_squared < min_distance) {
      min_distance = dist_squared;
    }
  }

  return min_distance;
}

typedef struct DynamicPaintFindIslandBorderData {
  const MeshElemMap *vert_to_looptri_map;
  int w, h, px, py;

  int best_index;
  float best_weight;
} DynamicPaintFindIslandBorderData;

static void dynamic_paint_find_island_border(const DynamicPaintCreateUVSurfaceData *data,
                                             DynamicPaintFindIslandBorderData *bdata,
                                             int tri_index,
                                             const float pixel[2],
                                             int in_edge,
                                             int depth);

/* Tries to find the neighboring pixel in given (uv space) direction.
 * Result is used by effect system to move paint on the surface.
 *
 * px, py : origin pixel x and y
 * n_index : lookup direction index (use neighX, neighY to get final index)
 */
static int dynamic_paint_find_neighbour_pixel(const DynamicPaintCreateUVSurfaceData *data,
                                              const MeshElemMap *vert_to_looptri_map,
                                              const int w,
                                              const int h,
                                              const int px,
                                              const int py,
                                              const int n_index)
{
  /* Note: Current method only uses polygon edges to detect neighboring pixels.
   *       -> It doesn't always lead to the optimum pixel but is accurate enough
   *          and faster/simpler than including possible face tip point links)
   */

  /* shift position by given n_index */
  const int x = px + neighX[n_index];
  const int y = py + neighY[n_index];

  if (x < 0 || x >= w || y < 0 || y >= h) {
    return OUT_OF_TEXTURE;
  }

  const PaintUVPoint *tempPoints = data->tempPoints;
  const PaintUVPoint *tPoint = &tempPoints[x + w * y];   /* UV neighbor */
  const PaintUVPoint *cPoint = &tempPoints[px + w * py]; /* Origin point */

  /* Check if shifted point is on same face -> it's a correct neighbor (and if it isn't marked as an "edge pixel") */
  if ((tPoint->tri_index == cPoint->tri_index) && (tPoint->neighbour_pixel == -1)) {
    return (x + w * y);
  }

  /* Even if shifted point is on another face
   * -> use this point.
   *
   * !! Replace with "is uv faces linked" check !!
   * This should work fine as long as uv island margin is > 1 pixel.
   */
  if ((tPoint->tri_index != -1) && (tPoint->neighbour_pixel == -1)) {
    return (x + w * y);
  }

  /* If we get here, the actual neighboring pixel is located on a non-linked uv face,
   * and we have to find its "real" position.
   *
   * Simple neighboring face finding algorithm:
   *   - find closest uv edge to shifted pixel and get the another face that shares that edge
   *   - find corresponding position of that new face edge in uv space
   *
   * TODO: Implement something more accurate / optimized?
   */
  {
    DynamicPaintFindIslandBorderData bdata = {
        .vert_to_looptri_map = vert_to_looptri_map,
        .w = w,
        .h = h,
        .px = px,
        .py = py,
        .best_index = NOT_FOUND,
        .best_weight = 1.0f,
    };

    float pixel[2];

    pixel[0] = ((float)(px + neighX[n_index]) + 0.5f) / (float)w;
    pixel[1] = ((float)(py + neighY[n_index]) + 0.5f) / (float)h;

    /* Do a small recursive search for the best island edge. */
    dynamic_paint_find_island_border(data, &bdata, cPoint->tri_index, pixel, -1, 5);

    return bdata.best_index;
  }
}

static void dynamic_paint_find_island_border(const DynamicPaintCreateUVSurfaceData *data,
                                             DynamicPaintFindIslandBorderData *bdata,
                                             int tri_index,
                                             const float pixel[2],
                                             int in_edge,
                                             int depth)
{
  const MLoop *mloop = data->mloop;
  const MLoopTri *mlooptri = data->mlooptri;
  const MLoopUV *mloopuv = data->mloopuv;

  const unsigned int *loop_idx = mlooptri[tri_index].tri;

  /* Enumerate all edges of the triangle, rotating the vertex list accordingly. */
  for (int edge_idx = 0; edge_idx < 3; edge_idx++) {
    /* but not the edge we have just recursed through */
    if (edge_idx == in_edge) {
      continue;
    }

    float uv0[2], uv1[2], uv2[2];

    copy_v2_v2(uv0, mloopuv[loop_idx[(edge_idx + 0)]].uv);
    copy_v2_v2(uv1, mloopuv[loop_idx[(edge_idx + 1) % 3]].uv);
    copy_v2_v2(uv2, mloopuv[loop_idx[(edge_idx + 2) % 3]].uv);

    /* Verify the target point is on the opposite side of the edge from the third triangle
     * vertex, to ensure that we always move closer to the goal point. */
    const float sidep = line_point_side_v2(uv0, uv1, pixel);
    const float side2 = line_point_side_v2(uv0, uv1, uv2);

    if (side2 == 0.0f) {
      continue;
    }

    /* Hack: allow all edges of the original triangle */
    const bool correct_side = (in_edge == -1) || (sidep < 0 && side2 > 0) ||
                              (sidep > 0 && side2 < 0);

    /* Allow exactly on edge for the non-recursive case */
    if (!correct_side && sidep != 0.0f) {
      continue;
    }

    /* Now find another face that is linked to that edge. */
    const int vert0 = mloop[loop_idx[(edge_idx + 0)]].v;
    const int vert1 = mloop[loop_idx[(edge_idx + 1) % 3]].v;

    /* Use a pre-computed vert-to-looptri mapping, speeds up things a lot compared to looping over all loopti. */
    const MeshElemMap *map = &bdata->vert_to_looptri_map[vert0];

    bool found_other = false;
    int target_tri = -1;
    int target_edge = -1;

    float ouv0[2], ouv1[2];

    for (int i = 0; i < map->count && !found_other; i++) {
      const int lt_index = map->indices[i];

      if (lt_index == tri_index) {
        continue;
      }

      const unsigned int *other_loop_idx = mlooptri[lt_index].tri;

      /* Check edges for match, looping in the same order as the outer loop. */
      for (int j = 0; j < 3; j++) {
        const int overt0 = mloop[other_loop_idx[(j + 0)]].v;
        const int overt1 = mloop[other_loop_idx[(j + 1) % 3]].v;

        /* Allow for swapped vertex order */
        if (overt0 == vert0 && overt1 == vert1) {
          found_other = true;
          copy_v2_v2(ouv0, mloopuv[other_loop_idx[(j + 0)]].uv);
          copy_v2_v2(ouv1, mloopuv[other_loop_idx[(j + 1) % 3]].uv);
        }
        else if (overt0 == vert1 && overt1 == vert0) {
          found_other = true;
          copy_v2_v2(ouv1, mloopuv[other_loop_idx[(j + 0)]].uv);
          copy_v2_v2(ouv0, mloopuv[other_loop_idx[(j + 1) % 3]].uv);
        }

        if (found_other) {
          target_tri = lt_index;
          target_edge = j;
          break;
        }
      }
    }

    if (!found_other) {
      if (bdata->best_index < 0) {
        bdata->best_index = ON_MESH_EDGE;
      }

      continue;
    }

    /* If this edge is connected in UV space too, recurse */
    if (equals_v2v2(uv0, ouv0) && equals_v2v2(uv1, ouv1)) {
      if (depth > 0 && correct_side) {
        dynamic_paint_find_island_border(data, bdata, target_tri, pixel, target_edge, depth - 1);
      }

      continue;
    }

    /* Otherwise try to map to the other side of the edge.
     * First check if there already is a better solution. */
    const float dist_squared = dist_squared_to_line_segment_v2(pixel, uv0, uv1);

    if (bdata->best_index >= 0 && dist_squared >= bdata->best_weight) {
      continue;
    }

    /*
     * Find a point that is relatively at same edge position
     * on this other face UV
     */
    float closest_point[2], dir_vec[2], tgt_pixel[2];

    float lambda = closest_to_line_v2(closest_point, pixel, uv0, uv1);
    CLAMP(lambda, 0.0f, 1.0f);

    sub_v2_v2v2(dir_vec, ouv1, ouv0);
    madd_v2_v2v2fl(tgt_pixel, ouv0, dir_vec, lambda);

    int w = bdata->w, h = bdata->h, px = bdata->px, py = bdata->py;

    int final_pixel[2] = {(int)floorf(tgt_pixel[0] * w), (int)floorf(tgt_pixel[1] * h)};

    /* If current pixel uv is outside of texture */
    if (final_pixel[0] < 0 || final_pixel[0] >= w || final_pixel[1] < 0 || final_pixel[1] >= h) {
      if (bdata->best_index == NOT_FOUND) {
        bdata->best_index = OUT_OF_TEXTURE;
      }

      continue;
    }

    const PaintUVPoint *tempPoints = data->tempPoints;
    int final_index = final_pixel[0] + w * final_pixel[1];

    /* If we ended up to our origin point ( mesh has smaller than pixel sized faces) */
    if (final_index == (px + w * py)) {
      continue;
    }

    /* If final point is an "edge pixel", use it's "real" neighbor instead */
    if (tempPoints[final_index].neighbour_pixel != -1) {
      final_index = tempPoints[final_index].neighbour_pixel;

      /* If we ended up to our origin point */
      if (final_index == (px + w * py)) {
        continue;
      }
    }

    /* If found pixel still lies on wrong face ( mesh has smaller than pixel sized faces) */
    if (tempPoints[final_index].tri_index != target_tri) {
      /* Check if it's close enough to likely touch the intended triangle. Any triangle
       * becomes thinner than a pixel at its vertices, so robustness requires some margin. */
      const float final_pt[2] = {((final_index % w) + 0.5f) / w, ((final_index / w) + 0.5f) / h};
      const float threshold = SQUARE(0.7f) / (w * h);

      if (dist_squared_to_looptri_uv_edges(
              mlooptri, mloopuv, tempPoints[final_index].tri_index, final_pt) > threshold) {
        continue;
      }
    }

    bdata->best_index = final_index;
    bdata->best_weight = dist_squared;
  }
}

static bool dynamicPaint_pointHasNeighbor(PaintAdjData *ed, int index, int neighbor)
{
  const int idx = ed->n_index[index];

  for (int i = 0; i < ed->n_num[index]; i++) {
    if (ed->n_target[idx + i] == neighbor) {
      return true;
    }
  }

  return false;
}

/* Makes the adjacency data symmetric, except for border pixels. I.e. if A is neighbor of B, B is neighbor of A. */
static bool dynamicPaint_symmetrizeAdjData(PaintAdjData *ed, int active_points)
{
  int *new_n_index = MEM_callocN(sizeof(int) * active_points, "Surface Adj Index");
  int *new_n_num = MEM_callocN(sizeof(int) * active_points, "Surface Adj Counts");

  if (new_n_num && new_n_index) {
    /* Count symmetrized neigbors */
    int total_targets = 0;

    for (int index = 0; index < active_points; index++) {
      total_targets += ed->n_num[index];
      new_n_num[index] = ed->n_num[index];
    }

    for (int index = 0; index < active_points; index++) {
      if (ed->flags[index] & ADJ_BORDER_PIXEL) {
        continue;
      }

      for (int i = 0, idx = ed->n_index[index]; i < ed->n_num[index]; i++) {
        const int target = ed->n_target[idx + i];

        assert(!(ed->flags[target] & ADJ_BORDER_PIXEL));

        if (!dynamicPaint_pointHasNeighbor(ed, target, index)) {
          new_n_num[target]++;
          total_targets++;
        }
      }
    }

    /* Allocate a new target map */
    int *new_n_target = MEM_callocN(sizeof(int) * total_targets, "Surface Adj Targets");

    if (new_n_target) {
      /* Copy existing neighbors to the new map */
      int n_pos = 0;

      for (int index = 0; index < active_points; index++) {
        new_n_index[index] = n_pos;
        memcpy(&new_n_target[n_pos],
               &ed->n_target[ed->n_index[index]],
               sizeof(int) * ed->n_num[index]);

        /* Reset count to old, but advance position by new, leaving a gap to fill below. */
        n_pos += new_n_num[index];
        new_n_num[index] = ed->n_num[index];
      }

      assert(n_pos == total_targets);

      /* Add symmetrized - this loop behavior must exactly match the count pass above */
      for (int index = 0; index < active_points; index++) {
        if (ed->flags[index] & ADJ_BORDER_PIXEL) {
          continue;
        }

        for (int i = 0, idx = ed->n_index[index]; i < ed->n_num[index]; i++) {
          const int target = ed->n_target[idx + i];

          if (!dynamicPaint_pointHasNeighbor(ed, target, index)) {
            const int num = new_n_num[target]++;
            new_n_target[new_n_index[target] + num] = index;
          }
        }
      }

      /* Swap maps */
      MEM_freeN(ed->n_target);
      ed->n_target = new_n_target;

      MEM_freeN(ed->n_index);
      ed->n_index = new_n_index;

      MEM_freeN(ed->n_num);
      ed->n_num = new_n_num;

      ed->total_targets = total_targets;
      return true;
    }
  }

  if (new_n_index) {
    MEM_freeN(new_n_index);
  }
  if (new_n_num) {
    MEM_freeN(new_n_num);
  }

  return false;
}

int dynamicPaint_createUVSurface(Scene *scene,
                                 DynamicPaintSurface *surface,
                                 float *progress,
                                 short *do_update)
{
  /* Antialias jitter point relative coords */
  const int aa_samples = (surface->flags & MOD_DPAINT_ANTIALIAS) ? 5 : 1;
  char uvname[MAX_CUSTOMDATA_LAYER_NAME];
  uint32_t active_points = 0;
  bool error = false;

  PaintSurfaceData *sData;
  DynamicPaintCanvasSettings *canvas = surface->canvas;
  Mesh *mesh = dynamicPaint_canvas_mesh_get(canvas);

  PaintUVPoint *tempPoints = NULL;
  Vec3f *tempWeights = NULL;
  const MLoopTri *mlooptri = NULL;
  const MLoopUV *mloopuv = NULL;
  const MLoop *mloop = NULL;

  Bounds2D *faceBB = NULL;
  int *final_index;

  *progress = 0.0f;
  *do_update = true;

  if (!mesh) {
    return setError(canvas, N_("Canvas mesh not updated"));
  }
  if (surface->format != MOD_DPAINT_SURFACE_F_IMAGESEQ) {
    return setError(canvas, N_("Cannot bake non-'image sequence' formats"));
  }

  mloop = mesh->mloop;
  mlooptri = BKE_mesh_runtime_looptri_ensure(mesh);
  const int tottri = BKE_mesh_runtime_looptri_len(mesh);

  /* get uv map */
  if (CustomData_has_layer(&mesh->ldata, CD_MLOOPUV)) {
    CustomData_validate_layer_name(&mesh->ldata, CD_MLOOPUV, surface->uvlayer_name, uvname);
    mloopuv = CustomData_get_layer_named(&mesh->ldata, CD_MLOOPUV, uvname);
  }

  /* Check for validity */
  if (!mloopuv) {
    return setError(canvas, N_("No UV data on canvas"));
  }
  if (surface->image_resolution < 16 || surface->image_resolution > 8192) {
    return setError(canvas, N_("Invalid resolution"));
  }

  const int w = surface->image_resolution;
  const int h = w;

  /*
   * Start generating the surface
   */
  CLOG_INFO(&LOG, 1, "Preparing UV surface of %ix%i pixels and %i tris.", w, h, tottri);

  /* Init data struct */
  if (surface->data) {
    dynamicPaint_freeSurfaceData(surface);
  }
  sData = surface->data = MEM_callocN(sizeof(PaintSurfaceData), "PaintSurfaceData");
  if (!surface->data) {
    return setError(canvas, N_("Not enough free memory"));
  }

  tempPoints = MEM_callocN(w * h * sizeof(*tempPoints), "Temp PaintUVPoint");
  if (!tempPoints) {
    error = true;
  }

  final_index = MEM_callocN(w * h * sizeof(*final_index), "Temp UV Final Indexes");
  if (!final_index) {
    error = true;
  }

  tempWeights = MEM_mallocN(w * h * aa_samples * sizeof(*tempWeights), "Temp bWeights");
  if (!tempWeights) {
    error = true;
  }

  /*
   * Generate a temporary bounding box array for UV faces to optimize
   * the pixel-inside-a-face search.
   */
  if (!error) {
    faceBB = MEM_mallocN(tottri * sizeof(*faceBB), "MPCanvasFaceBB");
    if (!faceBB) {
      error = true;
    }
  }

  *progress = 0.01f;
  *do_update = true;

  if (!error) {
    for (int i = 0; i < tottri; i++) {
      copy_v2_v2(faceBB[i].min, mloopuv[mlooptri[i].tri[0]].uv);
      copy_v2_v2(faceBB[i].max, mloopuv[mlooptri[i].tri[0]].uv);

      for (int j = 1; j < 3; j++) {
        minmax_v2v2_v2(faceBB[i].min, faceBB[i].max, mloopuv[mlooptri[i].tri[j]].uv);
      }
    }

    *progress = 0.02f;
    *do_update = true;

    /* Loop through every pixel and check if pixel is uv-mapped on a canvas face. */
    DynamicPaintCreateUVSurfaceData data = {
        .surface = surface,
        .tempPoints = tempPoints,
        .tempWeights = tempWeights,
        .mlooptri = mlooptri,
        .mloopuv = mloopuv,
        .mloop = mloop,
        .tottri = tottri,
        .faceBB = faceBB,
    };
    {
      ParallelRangeSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      settings.use_threading = (h > 64 || tottri > 1000);
      BLI_task_parallel_range(0, h, &data, dynamic_paint_create_uv_surface_direct_cb, &settings);
    }

    *progress = 0.04f;
    *do_update = true;

    /*
     * Now loop through every pixel that was left without index
     * and find if they have neighboring pixels that have an index.
     * If so use that polygon as pixel surface.
     * (To avoid seams on uv island edges)
     */
    data.active_points = &active_points;
    {
      ParallelRangeSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      settings.use_threading = (h > 64);
      BLI_task_parallel_range(0, h, &data, dynamic_paint_create_uv_surface_neighbor_cb, &settings);
    }

    *progress = 0.06f;
    *do_update = true;

    /* Generate surface adjacency data. */
    {
      int cursor = 0;

      /* Create a temporary array of final indexes (before unassigned
       * pixels have been dropped) */
      for (int i = 0; i < w * h; i++) {
        if (tempPoints[i].tri_index != -1) {
          final_index[i] = cursor;
          cursor++;
        }
      }
      /* allocate memory */
      sData->total_points = w * h;
      dynamicPaint_initAdjacencyData(surface, true);

      if (sData->adj_data) {
        PaintAdjData *ed = sData->adj_data;
        int n_pos = 0;

        MeshElemMap *vert_to_looptri_map;
        int *vert_to_looptri_map_mem;

        BKE_mesh_vert_looptri_map_create(&vert_to_looptri_map,
                                         &vert_to_looptri_map_mem,
                                         mesh->mvert,
                                         mesh->totvert,
                                         mlooptri,
                                         tottri,
                                         mloop,
                                         mesh->totloop);

        int total_border = 0;

        for (int ty = 0; ty < h; ty++) {
          for (int tx = 0; tx < w; tx++) {
            const int index = tx + w * ty;

            if (tempPoints[index].tri_index != -1) {
              ed->n_index[final_index[index]] = n_pos;
              ed->n_num[final_index[index]] = 0;

              if (tempPoints[index].neighbour_pixel != -1) {
                ed->flags[final_index[index]] |= ADJ_BORDER_PIXEL;
                total_border++;
              }

              for (int i = 0; i < 8; i++) {
                /* Try to find a neighboring pixel in defined direction. If not found, -1 is returned */
                const int n_target = dynamic_paint_find_neighbour_pixel(
                    &data, vert_to_looptri_map, w, h, tx, ty, i);

                if (n_target >= 0 && n_target != index) {
                  if (!dynamicPaint_pointHasNeighbor(
                          ed, final_index[index], final_index[n_target])) {
                    ed->n_target[n_pos] = final_index[n_target];
                    ed->n_num[final_index[index]]++;
                    n_pos++;
                  }
                }
                else if (n_target == ON_MESH_EDGE || n_target == OUT_OF_TEXTURE) {
                  ed->flags[final_index[index]] |= ADJ_ON_MESH_EDGE;
                }
              }
            }
          }
        }

        MEM_freeN(vert_to_looptri_map);
        MEM_freeN(vert_to_looptri_map_mem);

        /* Make neighbors symmetric */
        if (!dynamicPaint_symmetrizeAdjData(ed, active_points)) {
          error = true;
        }

        /* Create a list of border pixels */
        ed->border = MEM_callocN(sizeof(int) * total_border, "Border Pixel Index");

        if (ed->border) {
          ed->total_border = total_border;

          for (int i = 0, next = 0; i < active_points; i++) {
            if (ed->flags[i] & ADJ_BORDER_PIXEL) {
              ed->border[next++] = i;
            }
          }
        }

#if 0
        /* -----------------------------------------------------------------
         * For debug, write a dump of adjacency data to a file.
         * -----------------------------------------------------------------*/
        FILE *dump_file = fopen("dynpaint-adj-data.txt", "w");
        int *tmp = MEM_callocN(sizeof(int) * active_points, "tmp");
        for (int ty = 0; ty < h; ty++) {
          for (int tx = 0; tx < w; tx++) {
            const int index = tx + w * ty;
            if (tempPoints[index].tri_index != -1)
              tmp[final_index[index]] = index;
          }
        }
        for (int ty = 0; ty < h; ty++) {
          for (int tx = 0; tx < w; tx++) {
            const int index = tx + w * ty;
            const int fidx = final_index[index];

            if (tempPoints[index].tri_index != -1) {
              int nidx = tempPoints[index].neighbour_pixel;
              fprintf(dump_file,
                      "%d\t%d,%d\t%u\t%d,%d\t%d\t",
                      fidx,
                      tx,
                      h - 1 - ty,
                      tempPoints[index].tri_index,
                      nidx < 0 ? -1 : (nidx % w),
                      nidx < 0 ? -1 : h - 1 - (nidx / w),
                      ed->flags[fidx]);
              for (int i = 0; i < ed->n_num[fidx]; i++) {
                int tgt = tmp[ed->n_target[ed->n_index[fidx] + i]];
                fprintf(dump_file, "%s%d,%d", i ? " " : "", tgt % w, h - 1 - tgt / w);
              }
              fprintf(dump_file, "\n");
            }
          }
        }
        MEM_freeN(tmp);
        fclose(dump_file);
#endif
      }
    }

    *progress = 0.08f;
    *do_update = true;

    /* Create final surface data without inactive points */
    ImgSeqFormatData *f_data = MEM_callocN(sizeof(*f_data), "ImgSeqFormatData");
    if (f_data) {
      f_data->uv_p = MEM_callocN(active_points * sizeof(*f_data->uv_p), "PaintUVPoint");
      f_data->barycentricWeights = MEM_callocN(
          active_points * aa_samples * sizeof(*f_data->barycentricWeights), "PaintUVPoint");

      if (!f_data->uv_p || !f_data->barycentricWeights) {
        error = 1;
      }
    }
    else {
      error = 1;
    }

    /* in case of allocation error, free everything */
    if (error) {
      if (f_data) {
        if (f_data->uv_p) {
          MEM_freeN(f_data->uv_p);
        }
        if (f_data->barycentricWeights) {
          MEM_freeN(f_data->barycentricWeights);
        }
        MEM_freeN(f_data);
      }
      sData->total_points = 0;
    }
    else {
      sData->total_points = (int)active_points;
      sData->format_data = f_data;

      for (int index = 0, cursor = 0; index < (w * h); index++) {
        if (tempPoints[index].tri_index != -1) {
          memcpy(&f_data->uv_p[cursor], &tempPoints[index], sizeof(PaintUVPoint));
          memcpy(&f_data->barycentricWeights[cursor * aa_samples],
                 &tempWeights[index * aa_samples],
                 sizeof(*tempWeights) * aa_samples);
          cursor++;
        }
      }
    }
  }
  if (error == 1) {
    setError(canvas, N_("Not enough free memory"));
  }

  if (faceBB) {
    MEM_freeN(faceBB);
  }
  if (tempPoints) {
    MEM_freeN(tempPoints);
  }
  if (tempWeights) {
    MEM_freeN(tempWeights);
  }
  if (final_index) {
    MEM_freeN(final_index);
  }

  /* Init surface type data */
  if (!error) {
    dynamicPaint_allocateSurfaceType(surface);

#if 0
    /* -----------------------------------------------------------------
     * For debug, output pixel statuses to the color map
     * -----------------------------------------------------------------*/
    for (index = 0; index < sData->total_points; index++) {
      ImgSeqFormatData *f_data = (ImgSeqFormatData *)sData->format_data;
      PaintUVPoint *uvPoint = &((PaintUVPoint *)f_data->uv_p)[index];
      PaintPoint *pPoint = &((PaintPoint *)sData->type_data)[index];
      pPoint->alpha = 1.0f;

      /* Every pixel that is assigned as "edge pixel" gets blue color */
      if (uvPoint->neighbour_pixel != -1)
        pPoint->color[2] = 1.0f;
      /* and every pixel that finally got an polygon gets red color */
      /* green color shows pixel face index hash */
      if (uvPoint->tri_index != -1) {
        pPoint->color[0] = 1.0f;
        pPoint->color[1] = (float)(uvPoint->tri_index % 255) / 256.0f;
      }
    }
#endif

    dynamicPaint_setInitialColor(scene, surface);
  }

  *progress = 0.09f;
  *do_update = true;

  return (error == 0);
}

/*
 * Outputs an image file from uv surface data.
 */
typedef struct DynamicPaintOutputSurfaceImageData {
  const DynamicPaintSurface *surface;
  ImBuf *ibuf;
} DynamicPaintOutputSurfaceImageData;

static void dynamic_paint_output_surface_image_paint_cb(
    void *__restrict userdata, const int index, const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintOutputSurfaceImageData *data = userdata;

  const DynamicPaintSurface *surface = data->surface;
  const PaintPoint *point = &((PaintPoint *)surface->data->type_data)[index];

  ImBuf *ibuf = data->ibuf;
  /* image buffer position */
  const int pos = ((ImgSeqFormatData *)(surface->data->format_data))->uv_p[index].pixel_index * 4;

  /* blend wet and dry layers */
  blendColors(
      point->color, point->color[3], point->e_color, point->e_color[3], &ibuf->rect_float[pos]);

  /* Multiply color by alpha if enabled */
  if (surface->flags & MOD_DPAINT_MULALPHA) {
    mul_v3_fl(&ibuf->rect_float[pos], ibuf->rect_float[pos + 3]);
  }
}

static void dynamic_paint_output_surface_image_displace_cb(
    void *__restrict userdata, const int index, const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintOutputSurfaceImageData *data = userdata;

  const DynamicPaintSurface *surface = data->surface;
  float depth = ((float *)surface->data->type_data)[index];

  ImBuf *ibuf = data->ibuf;
  /* image buffer position */
  const int pos = ((ImgSeqFormatData *)(surface->data->format_data))->uv_p[index].pixel_index * 4;

  if (surface->depth_clamp) {
    depth /= surface->depth_clamp;
  }

  if (surface->disp_type == MOD_DPAINT_DISP_DISPLACE) {
    depth = (0.5f - depth / 2.0f);
  }

  CLAMP(depth, 0.0f, 1.0f);

  copy_v3_fl(&ibuf->rect_float[pos], depth);
  ibuf->rect_float[pos + 3] = 1.0f;
}

static void dynamic_paint_output_surface_image_wave_cb(
    void *__restrict userdata, const int index, const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintOutputSurfaceImageData *data = userdata;

  const DynamicPaintSurface *surface = data->surface;
  const PaintWavePoint *wPoint = &((PaintWavePoint *)surface->data->type_data)[index];
  float depth = wPoint->height;

  ImBuf *ibuf = data->ibuf;
  /* image buffer position */
  const int pos = ((ImgSeqFormatData *)(surface->data->format_data))->uv_p[index].pixel_index * 4;

  if (surface->depth_clamp) {
    depth /= surface->depth_clamp;
  }

  depth = (0.5f + depth / 2.0f);
  CLAMP(depth, 0.0f, 1.0f);

  copy_v3_fl(&ibuf->rect_float[pos], depth);
  ibuf->rect_float[pos + 3] = 1.0f;
}

static void dynamic_paint_output_surface_image_wetmap_cb(
    void *__restrict userdata, const int index, const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintOutputSurfaceImageData *data = userdata;

  const DynamicPaintSurface *surface = data->surface;
  const PaintPoint *point = &((PaintPoint *)surface->data->type_data)[index];

  ImBuf *ibuf = data->ibuf;
  /* image buffer position */
  const int pos = ((ImgSeqFormatData *)(surface->data->format_data))->uv_p[index].pixel_index * 4;

  copy_v3_fl(&ibuf->rect_float[pos], (point->wetness > 1.0f) ? 1.0f : point->wetness);
  ibuf->rect_float[pos + 3] = 1.0f;
}

void dynamicPaint_outputSurfaceImage(DynamicPaintSurface *surface,
                                     char *filename,
                                     short output_layer)
{
  ImBuf *ibuf = NULL;
  PaintSurfaceData *sData = surface->data;
  /* OpenEXR or PNG */
  int format = (surface->image_fileformat & MOD_DPAINT_IMGFORMAT_OPENEXR) ? R_IMF_IMTYPE_OPENEXR :
                                                                            R_IMF_IMTYPE_PNG;
  char output_file[FILE_MAX];

  if (!sData->type_data) {
    setError(surface->canvas, N_("Image save failed: invalid surface"));
    return;
  }
  /* if selected format is openexr, but current build doesn't support one */
#ifndef WITH_OPENEXR
  if (format == R_IMF_IMTYPE_OPENEXR) {
    format = R_IMF_IMTYPE_PNG;
  }
#endif
  BLI_strncpy(output_file, filename, sizeof(output_file));
  BKE_image_path_ensure_ext_from_imtype(output_file, format);

  /* Validate output file path */
  BLI_path_abs(output_file, BKE_main_blendfile_path_from_global());
  BLI_make_existing_file(output_file);

  /* Init image buffer */
  ibuf = IMB_allocImBuf(surface->image_resolution, surface->image_resolution, 32, IB_rectfloat);
  if (ibuf == NULL) {
    setError(surface->canvas, N_("Image save failed: not enough free memory"));
    return;
  }

  DynamicPaintOutputSurfaceImageData data = {
      .surface = surface,
      .ibuf = ibuf,
  };
  switch (surface->type) {
    case MOD_DPAINT_SURFACE_T_PAINT:
      switch (output_layer) {
        case 0: {
          ParallelRangeSettings settings;
          BLI_parallel_range_settings_defaults(&settings);
          settings.use_threading = (sData->total_points > 10000);
          BLI_task_parallel_range(0,
                                  sData->total_points,
                                  &data,
                                  dynamic_paint_output_surface_image_paint_cb,
                                  &settings);
          break;
        }
        case 1: {
          ParallelRangeSettings settings;
          BLI_parallel_range_settings_defaults(&settings);
          settings.use_threading = (sData->total_points > 10000);
          BLI_task_parallel_range(0,
                                  sData->total_points,
                                  &data,
                                  dynamic_paint_output_surface_image_wetmap_cb,
                                  &settings);
          break;
        }
        default:
          BLI_assert(0);
          break;
      }
      break;
    case MOD_DPAINT_SURFACE_T_DISPLACE:
      switch (output_layer) {
        case 0: {
          ParallelRangeSettings settings;
          BLI_parallel_range_settings_defaults(&settings);
          settings.use_threading = (sData->total_points > 10000);
          BLI_task_parallel_range(0,
                                  sData->total_points,
                                  &data,
                                  dynamic_paint_output_surface_image_displace_cb,
                                  &settings);
          break;
        }
        case 1:
          break;
        default:
          BLI_assert(0);
          break;
      }
      break;
    case MOD_DPAINT_SURFACE_T_WAVE:
      switch (output_layer) {
        case 0: {
          ParallelRangeSettings settings;
          BLI_parallel_range_settings_defaults(&settings);
          settings.use_threading = (sData->total_points > 10000);
          BLI_task_parallel_range(0,
                                  sData->total_points,
                                  &data,
                                  dynamic_paint_output_surface_image_wave_cb,
                                  &settings);
          break;
        }
        case 1:
          break;
        default:
          BLI_assert(0);
          break;
      }
      break;
    default:
      BLI_assert(0);
      break;
  }

    /* Set output format, png in case exr isn't supported */
#ifdef WITH_OPENEXR
  if (format == R_IMF_IMTYPE_OPENEXR) { /* OpenEXR 32-bit float */
    ibuf->ftype = IMB_FTYPE_OPENEXR;
    ibuf->foptions.flag |= OPENEXR_COMPRESS;
  }
  else
#endif
  {
    ibuf->ftype = IMB_FTYPE_PNG;
    ibuf->foptions.quality = 15;
  }

  /* Save image */
  IMB_saveiff(ibuf, output_file, IB_rectfloat);
  IMB_freeImBuf(ibuf);
}

/***************************** Ray / Nearest Point Utils ******************************/

/*  A modified callback to bvh tree raycast. The tree must have been built using bvhtree_from_mesh_looptri.
 *   userdata must be a BVHMeshCallbackUserdata built from the same mesh as the tree.
 *
 * To optimize brush detection speed this doesn't calculate hit coordinates or normal.
 */
static void mesh_tris_spherecast_dp(void *userdata,
                                    int index,
                                    const BVHTreeRay *ray,
                                    BVHTreeRayHit *hit)
{
  const BVHTreeFromMesh *data = (BVHTreeFromMesh *)userdata;
  const MVert *vert = data->vert;
  const MLoopTri *mlooptri = data->looptri;
  const MLoop *mloop = data->loop;

  const float *t0, *t1, *t2;
  float dist;

  t0 = vert[mloop[mlooptri[index].tri[0]].v].co;
  t1 = vert[mloop[mlooptri[index].tri[1]].v].co;
  t2 = vert[mloop[mlooptri[index].tri[2]].v].co;

  dist = bvhtree_ray_tri_intersection(ray, hit->dist, t0, t1, t2);

  if (dist >= 0 && dist < hit->dist) {
    hit->index = index;
    hit->dist = dist;
    hit->no[0] = 0.0f;
  }
}

/* A modified callback to bvh tree nearest point. The tree must have been built using bvhtree_from_mesh_looptri.
 * userdata must be a BVHMeshCallbackUserdata built from the same mesh as the tree.
 *
 * To optimize brush detection speed this doesn't calculate hit normal.
 */
static void mesh_tris_nearest_point_dp(void *userdata,
                                       int index,
                                       const float co[3],
                                       BVHTreeNearest *nearest)
{
  const BVHTreeFromMesh *data = (BVHTreeFromMesh *)userdata;
  const MVert *vert = data->vert;
  const MLoopTri *mlooptri = data->looptri;
  const MLoop *mloop = data->loop;
  float nearest_tmp[3], dist_sq;

  const float *t0, *t1, *t2;
  t0 = vert[mloop[mlooptri[index].tri[0]].v].co;
  t1 = vert[mloop[mlooptri[index].tri[1]].v].co;
  t2 = vert[mloop[mlooptri[index].tri[2]].v].co;

  closest_on_tri_to_point_v3(nearest_tmp, co, t0, t1, t2);
  dist_sq = len_squared_v3v3(co, nearest_tmp);

  if (dist_sq < nearest->dist_sq) {
    nearest->index = index;
    nearest->dist_sq = dist_sq;
    copy_v3_v3(nearest->co, nearest_tmp);
    nearest->no[0] = 0.0f;
  }
}

/***************************** Brush Painting Calls ******************************/

/**
 * Mix color values to canvas point.
 *
 * \param surface: Canvas surface
 * \param index: Surface point index
 * \param paintFlags: paint object flags
 * \param paintColor,paintAlpha,paintWetness: To be mixed paint values
 * \param timescale: Value used to adjust time dependent
 * operations when using substeps
 */
static void dynamicPaint_mixPaintColors(const DynamicPaintSurface *surface,
                                        const int index,
                                        const int paintFlags,
                                        const float paintColor[3],
                                        const float paintAlpha,
                                        const float paintWetness,
                                        const float timescale)
{
  PaintPoint *pPoint = &((PaintPoint *)surface->data->type_data)[index];

  /* Add paint */
  if (!(paintFlags & MOD_DPAINT_ERASE)) {
    float mix[4];
    float temp_alpha = paintAlpha * ((paintFlags & MOD_DPAINT_ABS_ALPHA) ? 1.0f : timescale);

    /* mix brush color with wet layer color */
    blendColors(pPoint->e_color, pPoint->e_color[3], paintColor, temp_alpha, mix);
    copy_v3_v3(pPoint->e_color, mix);

    /* mix wetness and alpha depending on selected alpha mode */
    if (paintFlags & MOD_DPAINT_ABS_ALPHA) {
      /* update values to the brush level unless they're higher already */
      CLAMP_MIN(pPoint->e_color[3], paintAlpha);
      CLAMP_MIN(pPoint->wetness, paintWetness);
    }
    else {
      float wetness = paintWetness;
      CLAMP(wetness, 0.0f, 1.0f);
      pPoint->e_color[3] = mix[3];
      pPoint->wetness = pPoint->wetness * (1.0f - wetness) + wetness;
    }

    CLAMP_MIN(pPoint->wetness, MIN_WETNESS);

    pPoint->state = DPAINT_PAINT_NEW;
  }
  /* Erase paint */
  else {
    float a_ratio, a_highest;
    float wetness;
    float invFact = 1.0f - paintAlpha;

    /*
     * Make highest alpha to match erased value
     * but maintain alpha ratio
     */
    if (paintFlags & MOD_DPAINT_ABS_ALPHA) {
      a_highest = max_ff(pPoint->color[3], pPoint->e_color[3]);
      if (a_highest > invFact) {
        a_ratio = invFact / a_highest;

        pPoint->e_color[3] *= a_ratio;
        pPoint->color[3] *= a_ratio;
      }
    }
    else {
      pPoint->e_color[3] -= paintAlpha * timescale;
      CLAMP_MIN(pPoint->e_color[3], 0.0f);
      pPoint->color[3] -= paintAlpha * timescale;
      CLAMP_MIN(pPoint->color[3], 0.0f);
    }

    wetness = (1.0f - paintWetness) * pPoint->e_color[3];
    CLAMP_MAX(pPoint->wetness, wetness);
  }
}

/* applies given brush intersection value for wave surface */
static void dynamicPaint_mixWaveHeight(PaintWavePoint *wPoint,
                                       const DynamicPaintBrushSettings *brush,
                                       float isect_height)
{
  const float isect_change = isect_height - wPoint->brush_isect;
  const float wave_factor = brush->wave_factor;
  bool hit = false;

  /* intersection marked regardless of brush type or hit */
  wPoint->brush_isect = isect_height;
  wPoint->state = DPAINT_WAVE_ISECT_CHANGED;

  isect_height *= wave_factor;

  /* determine hit depending on wave_factor */
  if (wave_factor > 0.0f && wPoint->height > isect_height) {
    hit = true;
  }
  else if (wave_factor < 0.0f && wPoint->height < isect_height) {
    hit = true;
  }

  if (hit) {
    switch (brush->wave_type) {
      case MOD_DPAINT_WAVEB_DEPTH:
        wPoint->height = isect_height;
        wPoint->state = DPAINT_WAVE_OBSTACLE;
        wPoint->velocity = 0.0f;
        break;
      case MOD_DPAINT_WAVEB_FORCE:
        wPoint->velocity = isect_height;
        break;
      case MOD_DPAINT_WAVEB_REFLECT:
        wPoint->state = DPAINT_WAVE_REFLECT_ONLY;
        break;
      case MOD_DPAINT_WAVEB_CHANGE:
        if (isect_change < 0.0f) {
          wPoint->height += isect_change * wave_factor;
        }
        break;
      default:
        BLI_assert(0);
        break;
    }
  }
}

/*
 * add brush results to the surface data depending on surface type
 */
static void dynamicPaint_updatePointData(const DynamicPaintSurface *surface,
                                         const int index,
                                         const DynamicPaintBrushSettings *brush,
                                         float paint[3],
                                         float influence,
                                         float depth,
                                         float vel_factor,
                                         const float timescale)
{
  PaintSurfaceData *sData = surface->data;
  float strength;

  /* apply influence scale */
  influence *= surface->influence_scale;
  depth *= surface->influence_scale;

  strength = influence * brush->alpha;
  CLAMP(strength, 0.0f, 1.0f);

  /* Sample velocity colorband if required */
  if (brush->flags &
      (MOD_DPAINT_VELOCITY_ALPHA | MOD_DPAINT_VELOCITY_COLOR | MOD_DPAINT_VELOCITY_DEPTH)) {
    float coba_res[4];
    vel_factor /= brush->max_velocity;
    CLAMP(vel_factor, 0.0f, 1.0f);

    if (BKE_colorband_evaluate(brush->vel_ramp, vel_factor, coba_res)) {
      if (brush->flags & MOD_DPAINT_VELOCITY_COLOR) {
        copy_v3_v3(paint, coba_res);
      }
      if (brush->flags & MOD_DPAINT_VELOCITY_ALPHA) {
        strength *= coba_res[3];
      }
      if (brush->flags & MOD_DPAINT_VELOCITY_DEPTH) {
        depth *= coba_res[3];
      }
    }
  }

  /* mix paint surface */
  if (surface->type == MOD_DPAINT_SURFACE_T_PAINT) {
    float paintWetness = brush->wetness * strength;
    float paintAlpha = strength;

    dynamicPaint_mixPaintColors(
        surface, index, brush->flags, paint, paintAlpha, paintWetness, timescale);
  }
  /* displace surface */
  else if (surface->type == MOD_DPAINT_SURFACE_T_DISPLACE) {
    float *value = (float *)sData->type_data;

    if (surface->flags & MOD_DPAINT_DISP_INCREMENTAL) {
      depth = value[index] + depth;
    }

    if (surface->depth_clamp) {
      CLAMP(depth, 0.0f - surface->depth_clamp, surface->depth_clamp);
    }

    if (brush->flags & MOD_DPAINT_ERASE) {
      value[index] *= (1.0f - strength);
      CLAMP_MIN(value[index], 0.0f);
    }
    else {
      CLAMP_MIN(value[index], depth);
    }
  }
  /* vertex weight group surface */
  else if (surface->type == MOD_DPAINT_SURFACE_T_WEIGHT) {
    float *value = (float *)sData->type_data;

    if (brush->flags & MOD_DPAINT_ERASE) {
      value[index] *= (1.0f - strength);
      CLAMP_MIN(value[index], 0.0f);
    }
    else {
      CLAMP_MIN(value[index], strength);
    }
  }
  /* wave surface */
  else if (surface->type == MOD_DPAINT_SURFACE_T_WAVE) {
    if (brush->wave_clamp) {
      CLAMP(depth, 0.0f - brush->wave_clamp, brush->wave_clamp);
    }

    dynamicPaint_mixWaveHeight(&((PaintWavePoint *)sData->type_data)[index], brush, 0.0f - depth);
  }

  /* doing velocity based painting */
  if (sData->bData->brush_velocity) {
    sData->bData->brush_velocity[index * 4 + 3] *= influence;
  }
}

/* checks whether surface and brush bounds intersect depending on brush type */
static bool meshBrush_boundsIntersect(Bounds3D *b1,
                                      Bounds3D *b2,
                                      DynamicPaintBrushSettings *brush,
                                      float brush_radius)
{
  if (brush->collision == MOD_DPAINT_COL_VOLUME) {
    return boundsIntersect(b1, b2);
  }
  else if (brush->collision == MOD_DPAINT_COL_DIST || brush->collision == MOD_DPAINT_COL_VOLDIST) {
    return boundsIntersectDist(b1, b2, brush_radius);
  }
  return true;
}

/* calculate velocity for mesh vertices */
typedef struct DynamicPaintBrushVelocityData {
  Vec3f *brush_vel;

  const MVert *mvert_p;
  const MVert *mvert_c;

  float (*obmat)[4];
  float (*prev_obmat)[4];

  const float timescale;
} DynamicPaintBrushVelocityData;

static void dynamic_paint_brush_velocity_compute_cb(void *__restrict userdata,
                                                    const int i,
                                                    const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintBrushVelocityData *data = userdata;

  Vec3f *brush_vel = data->brush_vel;

  const MVert *mvert_p = data->mvert_p;
  const MVert *mvert_c = data->mvert_c;

  float(*obmat)[4] = data->obmat;
  float(*prev_obmat)[4] = data->prev_obmat;

  const float timescale = data->timescale;

  float p1[3], p2[3];

  copy_v3_v3(p1, mvert_p[i].co);
  mul_m4_v3(prev_obmat, p1);

  copy_v3_v3(p2, mvert_c[i].co);
  mul_m4_v3(obmat, p2);

  sub_v3_v3v3(brush_vel[i].v, p2, p1);
  mul_v3_fl(brush_vel[i].v, 1.0f / timescale);
}

static void dynamicPaint_brushMeshCalculateVelocity(Depsgraph *depsgraph,
                                                    Scene *scene,
                                                    Object *ob,
                                                    DynamicPaintBrushSettings *brush,
                                                    Vec3f **brushVel,
                                                    float timescale)
{
  float prev_obmat[4][4];
  Mesh *mesh_p, *mesh_c;
  MVert *mvert_p, *mvert_c;
  int numOfVerts_p, numOfVerts_c;

  float cur_sfra = scene->r.subframe;
  int cur_fra = scene->r.cfra;
  float prev_sfra = cur_sfra - timescale;
  int prev_fra = cur_fra;

  if (prev_sfra < 0.0f) {
    prev_sfra += 1.0f;
    prev_fra = cur_fra - 1;
  }

  /* previous frame mesh */
  scene->r.cfra = prev_fra;
  scene->r.subframe = prev_sfra;

  BKE_object_modifier_update_subframe(depsgraph,
                                      scene,
                                      ob,
                                      true,
                                      SUBFRAME_RECURSION,
                                      BKE_scene_frame_get(scene),
                                      eModifierType_DynamicPaint);
  mesh_p = BKE_mesh_copy_for_eval(dynamicPaint_brush_mesh_get(brush), false);
  numOfVerts_p = mesh_p->totvert;
  mvert_p = mesh_p->mvert;
  copy_m4_m4(prev_obmat, ob->obmat);

  /* current frame mesh */
  scene->r.cfra = cur_fra;
  scene->r.subframe = cur_sfra;

  BKE_object_modifier_update_subframe(depsgraph,
                                      scene,
                                      ob,
                                      true,
                                      SUBFRAME_RECURSION,
                                      BKE_scene_frame_get(scene),
                                      eModifierType_DynamicPaint);
  mesh_c = dynamicPaint_brush_mesh_get(brush);
  numOfVerts_c = mesh_c->totvert;
  mvert_c = mesh_c->mvert;

  (*brushVel) = (struct Vec3f *)MEM_mallocN(numOfVerts_c * sizeof(Vec3f),
                                            "Dynamic Paint brush velocity");
  if (!(*brushVel)) {
    return;
  }

  /* if mesh is constructive -> num of verts has changed, only use current frame derived mesh */
  if (numOfVerts_p != numOfVerts_c) {
    mvert_p = mvert_c;
  }

  /* calculate speed */
  DynamicPaintBrushVelocityData data = {
      .brush_vel = *brushVel,
      .mvert_p = mvert_p,
      .mvert_c = mvert_c,
      .obmat = ob->obmat,
      .prev_obmat = prev_obmat,
      .timescale = timescale,
  };
  ParallelRangeSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (numOfVerts_c > 10000);
  BLI_task_parallel_range(
      0, numOfVerts_c, &data, dynamic_paint_brush_velocity_compute_cb, &settings);

  BKE_id_free(NULL, mesh_p);
}

/* calculate velocity for object center point */
static void dynamicPaint_brushObjectCalculateVelocity(
    Depsgraph *depsgraph, Scene *scene, Object *ob, Vec3f *brushVel, float timescale)
{
  float prev_obmat[4][4];
  float cur_loc[3] = {0.0f}, prev_loc[3] = {0.0f};

  float cur_sfra = scene->r.subframe;
  int cur_fra = scene->r.cfra;
  float prev_sfra = cur_sfra - timescale;
  int prev_fra = cur_fra;

  if (prev_sfra < 0.0f) {
    prev_sfra += 1.0f;
    prev_fra = cur_fra - 1;
  }

  /* previous frame mesh */
  scene->r.cfra = prev_fra;
  scene->r.subframe = prev_sfra;
  BKE_object_modifier_update_subframe(depsgraph,
                                      scene,
                                      ob,
                                      false,
                                      SUBFRAME_RECURSION,
                                      BKE_scene_frame_get(scene),
                                      eModifierType_DynamicPaint);
  copy_m4_m4(prev_obmat, ob->obmat);

  /* current frame mesh */
  scene->r.cfra = cur_fra;
  scene->r.subframe = cur_sfra;
  BKE_object_modifier_update_subframe(depsgraph,
                                      scene,
                                      ob,
                                      false,
                                      SUBFRAME_RECURSION,
                                      BKE_scene_frame_get(scene),
                                      eModifierType_DynamicPaint);

  /* calculate speed */
  mul_m4_v3(prev_obmat, prev_loc);
  mul_m4_v3(ob->obmat, cur_loc);

  sub_v3_v3v3(brushVel->v, cur_loc, prev_loc);
  mul_v3_fl(brushVel->v, 1.0f / timescale);
}

typedef struct DynamicPaintPaintData {
  const DynamicPaintSurface *surface;
  const DynamicPaintBrushSettings *brush;
  Object *brushOb;
  const Scene *scene;
  const float timescale;
  const int c_index;

  Mesh *mesh;
  const MVert *mvert;
  const MLoop *mloop;
  const MLoopTri *mlooptri;
  const float brush_radius;
  const float *avg_brushNor;
  const Vec3f *brushVelocity;

  const ParticleSystem *psys;
  const float solidradius;

  void *treeData;

  float *pointCoord;
} DynamicPaintPaintData;

/*
 * Paint a brush object mesh to the surface
 */
static void dynamic_paint_paint_mesh_cell_point_cb_ex(
    void *__restrict userdata, const int id, const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintPaintData *data = userdata;

  const DynamicPaintSurface *surface = data->surface;
  const PaintSurfaceData *sData = surface->data;
  const PaintBakeData *bData = sData->bData;
  VolumeGrid *grid = bData->grid;

  const DynamicPaintBrushSettings *brush = data->brush;

  const float timescale = data->timescale;
  const int c_index = data->c_index;

  const MVert *mvert = data->mvert;
  const MLoop *mloop = data->mloop;
  const MLoopTri *mlooptri = data->mlooptri;
  const float brush_radius = data->brush_radius;
  const float *avg_brushNor = data->avg_brushNor;
  const Vec3f *brushVelocity = data->brushVelocity;

  BVHTreeFromMesh *treeData = data->treeData;

  const int index = grid->t_index[grid->s_pos[c_index] + id];
  const int samples = bData->s_num[index];
  int ss;
  float total_sample = (float)samples;
  float brushStrength = 0.0f; /* brush influence factor */
  float depth = 0.0f;         /* brush intersection depth */
  float velocity_val = 0.0f;

  float paintColor[3] = {0.0f};
  int numOfHits = 0;

  /* for image sequence anti-aliasing, use gaussian factors */
  if (samples > 1 && surface->format == MOD_DPAINT_SURFACE_F_IMAGESEQ) {
    total_sample = gaussianTotal;
  }

  /* Supersampling */
  for (ss = 0; ss < samples; ss++) {
    float ray_start[3], ray_dir[3];
    float sample_factor = 0.0f;
    float sampleStrength = 0.0f;
    BVHTreeRayHit hit;
    BVHTreeNearest nearest;
    short hit_found = 0;

    /* volume sample */
    float volume_factor = 0.0f;
    /* proximity sample */
    float proximity_factor = 0.0f;
    float prox_colorband[4] = {0.0f};
    const bool inner_proximity = (brush->flags & MOD_DPAINT_INVERSE_PROX &&
                                  brush->collision == MOD_DPAINT_COL_VOLDIST);

    /* hit data */
    float hitCoord[3];
    int hitTri = -1;

    /* Supersampling factor */
    if (samples > 1 && surface->format == MOD_DPAINT_SURFACE_F_IMAGESEQ) {
      sample_factor = gaussianFactors[ss];
    }
    else {
      sample_factor = 1.0f;
    }

    /* Get current sample position in world coordinates */
    copy_v3_v3(ray_start, bData->realCoord[bData->s_pos[index] + ss].v);
    copy_v3_v3(ray_dir, bData->bNormal[index].invNorm);

    /* a simple hack to minimize chance of ray leaks at identical ray <-> edge locations */
    add_v3_fl(ray_start, 0.001f);

    hit.index = -1;
    hit.dist = BVH_RAYCAST_DIST_MAX;
    nearest.index = -1;
    nearest.dist_sq = brush_radius * brush_radius; /* find_nearest uses squared distance */

    /* Check volume collision */
    if (ELEM(brush->collision, MOD_DPAINT_COL_VOLUME, MOD_DPAINT_COL_VOLDIST)) {
      BLI_bvhtree_ray_cast(
          treeData->tree, ray_start, ray_dir, 0.0f, &hit, mesh_tris_spherecast_dp, treeData);
      if (hit.index != -1) {
        /* We hit a triangle, now check if collision point normal is facing the point */

        /* For optimization sake, hit point normal isn't calculated in ray cast loop */
        const int vtri[3] = {
            mloop[mlooptri[hit.index].tri[0]].v,
            mloop[mlooptri[hit.index].tri[1]].v,
            mloop[mlooptri[hit.index].tri[2]].v,
        };
        float dot;

        normal_tri_v3(hit.no, mvert[vtri[0]].co, mvert[vtri[1]].co, mvert[vtri[2]].co);
        dot = dot_v3v3(ray_dir, hit.no);

        /* If ray and hit face normal are facing same direction
         * hit point is inside a closed mesh. */
        if (dot >= 0.0f) {
          const float dist = hit.dist;
          const int f_index = hit.index;

          /* Also cast a ray in opposite direction to make sure
           * point is at least surrounded by two brush faces */
          negate_v3(ray_dir);
          hit.index = -1;
          hit.dist = BVH_RAYCAST_DIST_MAX;

          BLI_bvhtree_ray_cast(
              treeData->tree, ray_start, ray_dir, 0.0f, &hit, mesh_tris_spherecast_dp, treeData);

          if (hit.index != -1) {
            /* Add factor on supersample filter */
            volume_factor = 1.0f;
            hit_found = HIT_VOLUME;

            /* Mark hit info */
            madd_v3_v3v3fl(
                hitCoord, ray_start, ray_dir, hit.dist); /* Calculate final hit coordinates */
            depth += dist * sample_factor;
            hitTri = f_index;
          }
        }
      }
    }

    /* Check proximity collision */
    if (ELEM(brush->collision, MOD_DPAINT_COL_DIST, MOD_DPAINT_COL_VOLDIST) &&
        (!hit_found || (brush->flags & MOD_DPAINT_INVERSE_PROX))) {
      float proxDist = -1.0f;
      float hitCo[3] = {0.0f, 0.0f, 0.0f};
      int tri = 0;

      /* if inverse prox and no hit found, skip this sample */
      if (inner_proximity && !hit_found) {
        continue;
      }

      /* If pure distance proximity, find the nearest point on the mesh */
      if (!(brush->flags & MOD_DPAINT_PROX_PROJECT)) {
        BLI_bvhtree_find_nearest(
            treeData->tree, ray_start, &nearest, mesh_tris_nearest_point_dp, treeData);
        if (nearest.index != -1) {
          proxDist = sqrtf(nearest.dist_sq);
          copy_v3_v3(hitCo, nearest.co);
          tri = nearest.index;
        }
      }
      else { /* else cast a ray in defined projection direction */
        float proj_ray[3] = {0.0f};

        if (brush->ray_dir == MOD_DPAINT_RAY_CANVAS) {
          copy_v3_v3(proj_ray, bData->bNormal[index].invNorm);
          negate_v3(proj_ray);
        }
        else if (brush->ray_dir == MOD_DPAINT_RAY_BRUSH_AVG) {
          copy_v3_v3(proj_ray, avg_brushNor);
        }
        else { /* MOD_DPAINT_RAY_ZPLUS */
          proj_ray[2] = 1.0f;
        }
        hit.index = -1;
        hit.dist = brush_radius;

        /* Do a face normal directional raycast, and use that distance */
        BLI_bvhtree_ray_cast(
            treeData->tree, ray_start, proj_ray, 0.0f, &hit, mesh_tris_spherecast_dp, treeData);
        if (hit.index != -1) {
          proxDist = hit.dist;
          madd_v3_v3v3fl(
              hitCo, ray_start, proj_ray, hit.dist); /* Calculate final hit coordinates */
          tri = hit.index;
        }
      }

      /* If a hit was found, calculate required values */
      if (proxDist >= 0.0f && proxDist <= brush_radius) {
        proximity_factor = proxDist / brush_radius;
        CLAMP(proximity_factor, 0.0f, 1.0f);
        if (!inner_proximity) {
          proximity_factor = 1.0f - proximity_factor;
        }

        hit_found = HIT_PROXIMITY;

        /* if no volume hit, use prox point face info */
        if (hitTri == -1) {
          copy_v3_v3(hitCoord, hitCo);
          hitTri = tri;
        }
      }
    }

    /* mix final sample strength depending on brush settings */
    if (hit_found) {
      /* if "negate volume" enabled, negate all factors within volume*/
      if (brush->collision == MOD_DPAINT_COL_VOLDIST && brush->flags & MOD_DPAINT_NEGATE_VOLUME) {
        volume_factor = 1.0f - volume_factor;
        if (inner_proximity) {
          proximity_factor = 1.0f - proximity_factor;
        }
      }

      /* apply final sample depending on final hit type */
      if (hit_found == HIT_VOLUME) {
        sampleStrength = volume_factor;
      }
      else if (hit_found == HIT_PROXIMITY) {
        /* apply falloff curve to the proximity_factor */
        if (brush->proximity_falloff == MOD_DPAINT_PRFALL_RAMP &&
            BKE_colorband_evaluate(brush->paint_ramp, (1.0f - proximity_factor), prox_colorband)) {
          proximity_factor = prox_colorband[3];
        }
        else if (brush->proximity_falloff == MOD_DPAINT_PRFALL_CONSTANT) {
          proximity_factor = (!inner_proximity || brush->flags & MOD_DPAINT_NEGATE_VOLUME) ? 1.0f :
                                                                                             0.0f;
        }
        /* apply sample */
        sampleStrength = proximity_factor;
      }

      sampleStrength *= sample_factor;
    }
    else {
      continue;
    }

    /* velocity brush, only do on main sample */
    if (brush->flags & MOD_DPAINT_USES_VELOCITY && ss == 0 && brushVelocity) {
      float weights[3];
      float brushPointVelocity[3];
      float velocity[3];

      const int v1 = mloop[mlooptri[hitTri].tri[0]].v;
      const int v2 = mloop[mlooptri[hitTri].tri[1]].v;
      const int v3 = mloop[mlooptri[hitTri].tri[2]].v;

      /* calculate barycentric weights for hit point */
      interp_weights_tri_v3(weights, mvert[v1].co, mvert[v2].co, mvert[v3].co, hitCoord);

      /* simple check based on brush surface velocity,
       * todo: perhaps implement something that handles volume movement as well. */

      /* interpolate vertex speed vectors to get hit point velocity */
      interp_v3_v3v3v3(brushPointVelocity,
                       brushVelocity[v1].v,
                       brushVelocity[v2].v,
                       brushVelocity[v3].v,
                       weights);

      /* substract canvas point velocity */
      if (bData->velocity) {
        sub_v3_v3v3(velocity, brushPointVelocity, bData->velocity[index].v);
      }
      else {
        copy_v3_v3(velocity, brushPointVelocity);
      }
      velocity_val = normalize_v3(velocity);

      /* if brush has smudge enabled store brush velocity */
      if (surface->type == MOD_DPAINT_SURFACE_T_PAINT && brush->flags & MOD_DPAINT_DO_SMUDGE &&
          bData->brush_velocity) {
        copy_v3_v3(&bData->brush_velocity[index * 4], velocity);
        bData->brush_velocity[index * 4 + 3] = velocity_val;
      }
    }

    /*
     * Process hit color and alpha
     */
    if (surface->type == MOD_DPAINT_SURFACE_T_PAINT) {
      float sampleColor[3];
      float alpha_factor = 1.0f;

      sampleColor[0] = brush->r;
      sampleColor[1] = brush->g;
      sampleColor[2] = brush->b;

      /* Sample proximity colorband if required */
      if ((hit_found == HIT_PROXIMITY) && (brush->proximity_falloff == MOD_DPAINT_PRFALL_RAMP)) {
        if (!(brush->flags & MOD_DPAINT_RAMP_ALPHA)) {
          sampleColor[0] = prox_colorband[0];
          sampleColor[1] = prox_colorband[1];
          sampleColor[2] = prox_colorband[2];
        }
      }

      /* Add AA sample */
      paintColor[0] += sampleColor[0];
      paintColor[1] += sampleColor[1];
      paintColor[2] += sampleColor[2];
      sampleStrength *= alpha_factor;
      numOfHits++;
    }

    /* apply sample strength */
    brushStrength += sampleStrength;
  }  // end supersampling

  /* if any sample was inside paint range */
  if (brushStrength > 0.0f || depth > 0.0f) {
    /* apply supersampling results */
    if (samples > 1) {
      brushStrength /= total_sample;
    }
    CLAMP(brushStrength, 0.0f, 1.0f);

    if (surface->type == MOD_DPAINT_SURFACE_T_PAINT) {
      /* Get final pixel color and alpha */
      paintColor[0] /= numOfHits;
      paintColor[1] /= numOfHits;
      paintColor[2] /= numOfHits;
    }
    /* get final object space depth */
    else if (ELEM(surface->type, MOD_DPAINT_SURFACE_T_DISPLACE, MOD_DPAINT_SURFACE_T_WAVE)) {
      depth /= bData->bNormal[index].normal_scale * total_sample;
    }

    dynamicPaint_updatePointData(
        surface, index, brush, paintColor, brushStrength, depth, velocity_val, timescale);
  }
}

static int dynamicPaint_paintMesh(Depsgraph *depsgraph,
                                  DynamicPaintSurface *surface,
                                  DynamicPaintBrushSettings *brush,
                                  Object *brushOb,
                                  Scene *scene,
                                  float timescale)
{
  PaintSurfaceData *sData = surface->data;
  PaintBakeData *bData = sData->bData;
  Mesh *mesh = NULL;
  Vec3f *brushVelocity = NULL;
  MVert *mvert = NULL;
  const MLoopTri *mlooptri = NULL;
  const MLoop *mloop = NULL;

  if (brush->flags & MOD_DPAINT_USES_VELOCITY) {
    dynamicPaint_brushMeshCalculateVelocity(
        depsgraph, scene, brushOb, brush, &brushVelocity, timescale);
  }

  Mesh *brush_mesh = dynamicPaint_brush_mesh_get(brush);
  if (brush_mesh == NULL) {
    return 0;
  }

  {
    BVHTreeFromMesh treeData = {NULL};
    float avg_brushNor[3] = {0.0f};
    const float brush_radius = brush->paint_distance * surface->radius_scale;
    int numOfVerts;
    int ii;
    Bounds3D mesh_bb = {{0}};
    VolumeGrid *grid = bData->grid;

    mesh = BKE_mesh_copy_for_eval(brush_mesh, false);
    mvert = mesh->mvert;
    mlooptri = BKE_mesh_runtime_looptri_ensure(mesh);
    mloop = mesh->mloop;
    numOfVerts = mesh->totvert;

    /* Transform collider vertices to global space
     * (Faster than transforming per surface point
     * coordinates and normals to object space) */
    for (ii = 0; ii < numOfVerts; ii++) {
      mul_m4_v3(brushOb->obmat, mvert[ii].co);
      boundInsert(&mesh_bb, mvert[ii].co);

      /* for proximity project calculate average normal */
      if (brush->flags & MOD_DPAINT_PROX_PROJECT && brush->collision != MOD_DPAINT_COL_VOLUME) {
        float nor[3];
        normal_short_to_float_v3(nor, mvert[ii].no);
        mul_mat3_m4_v3(brushOb->obmat, nor);
        normalize_v3(nor);

        add_v3_v3(avg_brushNor, nor);
      }
    }

    if (brush->flags & MOD_DPAINT_PROX_PROJECT && brush->collision != MOD_DPAINT_COL_VOLUME) {
      mul_v3_fl(avg_brushNor, 1.0f / (float)numOfVerts);
      /* instead of null vector use positive z */
      if (UNLIKELY(normalize_v3(avg_brushNor) == 0.0f)) {
        avg_brushNor[2] = 1.0f;
      }
    }

    /* check bounding box collision */
    if (grid && meshBrush_boundsIntersect(&grid->grid_bounds, &mesh_bb, brush, brush_radius)) {
      /* Build a bvh tree from transformed vertices */
      if (BKE_bvhtree_from_mesh_get(&treeData, mesh, BVHTREE_FROM_LOOPTRI, 4)) {
        int c_index;
        int total_cells = grid->dim[0] * grid->dim[1] * grid->dim[2];

        /* loop through space partitioning grid */
        for (c_index = 0; c_index < total_cells; c_index++) {
          /* check grid cell bounding box */
          if (!grid->s_num[c_index] ||
              !meshBrush_boundsIntersect(&grid->bounds[c_index], &mesh_bb, brush, brush_radius)) {
            continue;
          }

          /* loop through cell points and process brush */
          DynamicPaintPaintData data = {
              .surface = surface,
              .brush = brush,
              .brushOb = brushOb,
              .scene = scene,
              .timescale = timescale,
              .c_index = c_index,
              .mesh = mesh,
              .mvert = mvert,
              .mloop = mloop,
              .mlooptri = mlooptri,
              .brush_radius = brush_radius,
              .avg_brushNor = avg_brushNor,
              .brushVelocity = brushVelocity,
              .treeData = &treeData,
          };
          ParallelRangeSettings settings;
          BLI_parallel_range_settings_defaults(&settings);
          settings.use_threading = (grid->s_num[c_index] > 250);
          BLI_task_parallel_range(0,
                                  grid->s_num[c_index],
                                  &data,
                                  dynamic_paint_paint_mesh_cell_point_cb_ex,
                                  &settings);
        }
      }
    }
    /* free bvh tree */
    free_bvhtree_from_mesh(&treeData);
    BKE_id_free(NULL, mesh);
  }

  /* free brush velocity data */
  if (brushVelocity) {
    MEM_freeN(brushVelocity);
  }

  return 1;
}

/*
 * Paint a particle system to the surface
 */
static void dynamic_paint_paint_particle_cell_point_cb_ex(
    void *__restrict userdata, const int id, const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintPaintData *data = userdata;

  const DynamicPaintSurface *surface = data->surface;
  const PaintSurfaceData *sData = surface->data;
  const PaintBakeData *bData = sData->bData;
  VolumeGrid *grid = bData->grid;

  const DynamicPaintBrushSettings *brush = data->brush;

  const ParticleSystem *psys = data->psys;

  const float timescale = data->timescale;
  const int c_index = data->c_index;

  KDTree_3d *tree = data->treeData;

  const float solidradius = data->solidradius;
  const float smooth = brush->particle_smooth * surface->radius_scale;
  const float range = solidradius + smooth;
  const float particle_timestep = 0.04f * psys->part->timetweak;

  const int index = grid->t_index[grid->s_pos[c_index] + id];
  float disp_intersect = 0.0f;
  float radius = 0.0f;
  float strength = 0.0f;
  int part_index = -1;

  /*
   * With predefined radius, there is no variation between particles.
   * It's enough to just find the nearest one.
   */
  {
    KDTreeNearest_3d nearest;
    float smooth_range, part_solidradius;

    /* Find nearest particle and get distance to it */
    BLI_kdtree_3d_find_nearest(tree, bData->realCoord[bData->s_pos[index]].v, &nearest);
    /* if outside maximum range, no other particle can influence either */
    if (nearest.dist > range) {
      return;
    }

    if (brush->flags & MOD_DPAINT_PART_RAD) {
      /* use particles individual size */
      ParticleData *pa = psys->particles + nearest.index;
      part_solidradius = pa->size;
    }
    else {
      part_solidradius = solidradius;
    }
    radius = part_solidradius + smooth;
    if (nearest.dist < radius) {
      /* distances inside solid radius has maximum influence -> dist = 0 */
      smooth_range = max_ff(0.0f, (nearest.dist - part_solidradius));
      /* do smoothness if enabled */
      if (smooth) {
        smooth_range /= smooth;
      }

      strength = 1.0f - smooth_range;
      disp_intersect = radius - nearest.dist;
      part_index = nearest.index;
    }
  }
  /* If using random per particle radius and closest particle didn't give max influence */
  if (brush->flags & MOD_DPAINT_PART_RAD && strength < 1.0f && psys->part->randsize > 0.0f) {
    /*
     * If we use per particle radius, we have to sample all particles
     * within max radius range
     */
    KDTreeNearest_3d *nearest;

    float smooth_range = smooth * (1.0f - strength), dist;
    /* calculate max range that can have particles with higher influence than the nearest one */
    const float max_range = smooth - strength * smooth + solidradius;
    /* Make gcc happy! */
    dist = max_range;

    const int particles = BLI_kdtree_3d_range_search(
        tree, bData->realCoord[bData->s_pos[index]].v, &nearest, max_range);

    /* Find particle that produces highest influence */
    for (int n = 0; n < particles; n++) {
      ParticleData *pa = &psys->particles[nearest[n].index];

      /* skip if out of range */
      if (nearest[n].dist > (pa->size + smooth)) {
        continue;
      }

      /* update hit data */
      const float s_range = nearest[n].dist - pa->size;
      /* skip if higher influence is already found */
      if (smooth_range < s_range) {
        continue;
      }

      /* update hit data */
      smooth_range = s_range;
      dist = nearest[n].dist;
      part_index = nearest[n].index;

      /* If inside solid range and no disp depth required, no need to seek further */
      if ((s_range < 0.0f) &&
          !ELEM(surface->type, MOD_DPAINT_SURFACE_T_DISPLACE, MOD_DPAINT_SURFACE_T_WAVE)) {
        break;
      }
    }

    if (nearest) {
      MEM_freeN(nearest);
    }

    /* now calculate influence for this particle */
    const float rad = radius + smooth;
    if ((rad - dist) > disp_intersect) {
      disp_intersect = radius - dist;
      radius = rad;
    }

    /* do smoothness if enabled */
    CLAMP_MIN(smooth_range, 0.0f);
    if (smooth) {
      smooth_range /= smooth;
    }

    const float str = 1.0f - smooth_range;
    /* if influence is greater, use this one */
    if (str > strength) {
      strength = str;
    }
  }

  if (strength > 0.001f) {
    float paintColor[4] = {0.0f};
    float depth = 0.0f;
    float velocity_val = 0.0f;

    /* apply velocity */
    if ((brush->flags & MOD_DPAINT_USES_VELOCITY) && (part_index != -1)) {
      float velocity[3];
      ParticleData *pa = psys->particles + part_index;
      mul_v3_v3fl(velocity, pa->state.vel, particle_timestep);

      /* substract canvas point velocity */
      if (bData->velocity) {
        sub_v3_v3(velocity, bData->velocity[index].v);
      }
      velocity_val = normalize_v3(velocity);

      /* store brush velocity for smudge */
      if ((surface->type == MOD_DPAINT_SURFACE_T_PAINT) &&
          (brush->flags & MOD_DPAINT_DO_SMUDGE && bData->brush_velocity)) {
        copy_v3_v3(&bData->brush_velocity[index * 4], velocity);
        bData->brush_velocity[index * 4 + 3] = velocity_val;
      }
    }

    if (surface->type == MOD_DPAINT_SURFACE_T_PAINT) {
      copy_v3_v3(paintColor, &brush->r);
    }
    else if (ELEM(surface->type, MOD_DPAINT_SURFACE_T_DISPLACE, MOD_DPAINT_SURFACE_T_WAVE)) {
      /* get displace depth */
      disp_intersect = (1.0f - sqrtf(disp_intersect / radius)) * radius;
      depth = max_ff(0.0f, (radius - disp_intersect) / bData->bNormal[index].normal_scale);
    }

    dynamicPaint_updatePointData(
        surface, index, brush, paintColor, strength, depth, velocity_val, timescale);
  }
}

static int dynamicPaint_paintParticles(DynamicPaintSurface *surface,
                                       ParticleSystem *psys,
                                       DynamicPaintBrushSettings *brush,
                                       float timescale)
{
  ParticleSettings *part = psys->part;
  PaintSurfaceData *sData = surface->data;
  PaintBakeData *bData = sData->bData;
  VolumeGrid *grid = bData->grid;

  KDTree_3d *tree;
  int particlesAdded = 0;
  int invalidParticles = 0;
  int p = 0;

  const float solidradius = surface->radius_scale * ((brush->flags & MOD_DPAINT_PART_RAD) ?
                                                         part->size :
                                                         brush->particle_radius);
  const float smooth = brush->particle_smooth * surface->radius_scale;

  const float range = solidradius + smooth;

  Bounds3D part_bb = {{0}};

  if (psys->totpart < 1) {
    return 1;
  }

  /*
   * Build a kd-tree to optimize distance search
   */
  tree = BLI_kdtree_3d_new(psys->totpart);

  /* loop through particles and insert valid ones to the tree */
  p = 0;
  for (ParticleData *pa = psys->particles; p < psys->totpart; p++, pa++) {
    /* Proceed only if particle is active */
    if ((pa->alive == PARS_UNBORN && (part->flag & PART_UNBORN) == 0) ||
        (pa->alive == PARS_DEAD && (part->flag & PART_DIED) == 0) || (pa->flag & PARS_UNEXIST)) {
      continue;
    }

    /* for debug purposes check if any NAN particle proceeds
     * For some reason they get past activity check, this should rule most of them out */
    if (isnan(pa->state.co[0]) || isnan(pa->state.co[1]) || isnan(pa->state.co[2])) {
      invalidParticles++;
      continue;
    }

    /* make sure particle is close enough to canvas */
    if (!boundIntersectPoint(&grid->grid_bounds, pa->state.co, range)) {
      continue;
    }

    BLI_kdtree_3d_insert(tree, p, pa->state.co);

    /* calc particle system bounds */
    boundInsert(&part_bb, pa->state.co);

    particlesAdded++;
  }
  if (invalidParticles) {
    CLOG_WARN(&LOG, "Invalid particle(s) found!");
  }

  /* If no suitable particles were found, exit */
  if (particlesAdded < 1) {
    BLI_kdtree_3d_free(tree);
    return 1;
  }

  /* begin thread safe malloc */
  BLI_threaded_malloc_begin();

  /* only continue if particle bb is close enough to canvas bb */
  if (boundsIntersectDist(&grid->grid_bounds, &part_bb, range)) {
    int c_index;
    int total_cells = grid->dim[0] * grid->dim[1] * grid->dim[2];

    /* balance tree */
    BLI_kdtree_3d_balance(tree);

    /* loop through space partitioning grid */
    for (c_index = 0; c_index < total_cells; c_index++) {
      /* check cell bounding box */
      if (!grid->s_num[c_index] || !boundsIntersectDist(&grid->bounds[c_index], &part_bb, range)) {
        continue;
      }

      /* loop through cell points */
      DynamicPaintPaintData data = {
          .surface = surface,
          .brush = brush,
          .psys = psys,
          .solidradius = solidradius,
          .timescale = timescale,
          .c_index = c_index,
          .treeData = tree,
      };
      ParallelRangeSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      settings.use_threading = (grid->s_num[c_index] > 250);
      BLI_task_parallel_range(0,
                              grid->s_num[c_index],
                              &data,
                              dynamic_paint_paint_particle_cell_point_cb_ex,
                              &settings);
    }
  }
  BLI_threaded_malloc_end();
  BLI_kdtree_3d_free(tree);

  return 1;
}

/* paint a single point of defined proximity radius to the surface */
static void dynamic_paint_paint_single_point_cb_ex(void *__restrict userdata,
                                                   const int index,
                                                   const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintPaintData *data = userdata;

  const DynamicPaintSurface *surface = data->surface;
  const PaintSurfaceData *sData = surface->data;
  const PaintBakeData *bData = sData->bData;

  const DynamicPaintBrushSettings *brush = data->brush;

  const float timescale = data->timescale;

  const float brush_radius = data->brush_radius;
  const Vec3f *brushVelocity = data->brushVelocity;

  float *pointCoord = data->pointCoord;

  const float distance = len_v3v3(pointCoord, bData->realCoord[bData->s_pos[index]].v);
  float colorband[4] = {0.0f};
  float strength;

  if (distance > brush_radius) {
    return;
  }

  /* Smooth range or color ramp */
  if (brush->proximity_falloff == MOD_DPAINT_PRFALL_SMOOTH ||
      brush->proximity_falloff == MOD_DPAINT_PRFALL_RAMP) {
    strength = 1.0f - distance / brush_radius;
    CLAMP(strength, 0.0f, 1.0f);
  }
  else {
    strength = 1.0f;
  }

  if (strength >= 0.001f) {
    float paintColor[3] = {0.0f};
    float depth = 0.0f;
    float velocity_val = 0.0f;

    /* color ramp */
    if (brush->proximity_falloff == MOD_DPAINT_PRFALL_RAMP &&
        BKE_colorband_evaluate(brush->paint_ramp, (1.0f - strength), colorband)) {
      strength = colorband[3];
    }

    if (brush->flags & MOD_DPAINT_USES_VELOCITY) {
      float velocity[3];

      /* substract canvas point velocity */
      if (bData->velocity) {
        sub_v3_v3v3(velocity, brushVelocity->v, bData->velocity[index].v);
      }
      else {
        copy_v3_v3(velocity, brushVelocity->v);
      }
      velocity_val = len_v3(velocity);

      /* store brush velocity for smudge */
      if (surface->type == MOD_DPAINT_SURFACE_T_PAINT && brush->flags & MOD_DPAINT_DO_SMUDGE &&
          bData->brush_velocity) {
        mul_v3_v3fl(&bData->brush_velocity[index * 4], velocity, 1.0f / velocity_val);
        bData->brush_velocity[index * 4 + 3] = velocity_val;
      }
    }

    if (surface->type == MOD_DPAINT_SURFACE_T_PAINT) {
      if (brush->proximity_falloff == MOD_DPAINT_PRFALL_RAMP &&
          !(brush->flags & MOD_DPAINT_RAMP_ALPHA)) {
        paintColor[0] = colorband[0];
        paintColor[1] = colorband[1];
        paintColor[2] = colorband[2];
      }
      else {
        paintColor[0] = brush->r;
        paintColor[1] = brush->g;
        paintColor[2] = brush->b;
      }
    }
    else if (ELEM(surface->type, MOD_DPAINT_SURFACE_T_DISPLACE, MOD_DPAINT_SURFACE_T_WAVE)) {
      /* get displace depth */
      const float disp_intersect = (1.0f - sqrtf((brush_radius - distance) / brush_radius)) *
                                   brush_radius;
      depth = max_ff(0.0f, (brush_radius - disp_intersect) / bData->bNormal[index].normal_scale);
    }
    dynamicPaint_updatePointData(
        surface, index, brush, paintColor, strength, depth, velocity_val, timescale);
  }
}

static int dynamicPaint_paintSinglePoint(Depsgraph *depsgraph,
                                         DynamicPaintSurface *surface,
                                         float *pointCoord,
                                         DynamicPaintBrushSettings *brush,
                                         Object *brushOb,
                                         Scene *scene,
                                         float timescale)
{
  PaintSurfaceData *sData = surface->data;
  float brush_radius = brush->paint_distance * surface->radius_scale;
  Vec3f brushVel;

  if (brush->flags & MOD_DPAINT_USES_VELOCITY) {
    dynamicPaint_brushObjectCalculateVelocity(depsgraph, scene, brushOb, &brushVel, timescale);
  }

  const Mesh *brush_mesh = dynamicPaint_brush_mesh_get(brush);
  const MVert *mvert = brush_mesh->mvert;

  /*
   * Loop through every surface point
   */
  DynamicPaintPaintData data = {
      .surface = surface,
      .brush = brush,
      .brushOb = brushOb,
      .scene = scene,
      .timescale = timescale,
      .mvert = mvert,
      .brush_radius = brush_radius,
      .brushVelocity = &brushVel,
      .pointCoord = pointCoord,
  };
  ParallelRangeSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (sData->total_points > 1000);
  BLI_task_parallel_range(
      0, sData->total_points, &data, dynamic_paint_paint_single_point_cb_ex, &settings);

  return 1;
}

/***************************** Dynamic Paint Step / Baking ******************************/

/*
 * Calculate current frame distances and directions for adjacency data
 */

static void dynamic_paint_prepare_adjacency_cb(void *__restrict userdata,
                                               const int index,
                                               const ParallelRangeTLS *__restrict UNUSED(tls))
{
  PaintSurfaceData *sData = userdata;
  PaintBakeData *bData = sData->bData;
  BakeAdjPoint *bNeighs = bData->bNeighs;
  PaintAdjData *adj_data = sData->adj_data;
  Vec3f *realCoord = bData->realCoord;

  const int num_neighs = adj_data->n_num[index];

  for (int i = 0; i < num_neighs; i++) {
    const int n_index = adj_data->n_index[index] + i;
    const int t_index = adj_data->n_target[n_index];

    /* dir vec */
    sub_v3_v3v3(bNeighs[n_index].dir,
                realCoord[bData->s_pos[t_index]].v,
                realCoord[bData->s_pos[index]].v);
    /* dist */
    bNeighs[n_index].dist = normalize_v3(bNeighs[n_index].dir);
  }
}

static void dynamicPaint_prepareAdjacencyData(DynamicPaintSurface *surface, const bool force_init)
{
  PaintSurfaceData *sData = surface->data;
  PaintBakeData *bData = sData->bData;
  BakeAdjPoint *bNeighs;
  PaintAdjData *adj_data = sData->adj_data;

  int index;

  if ((!surface_usesAdjDistance(surface) && !force_init) || !sData->adj_data) {
    return;
  }

  if (bData->bNeighs) {
    MEM_freeN(bData->bNeighs);
  }
  bNeighs = bData->bNeighs = MEM_mallocN(sData->adj_data->total_targets * sizeof(*bNeighs),
                                         "PaintEffectBake");
  if (!bNeighs) {
    return;
  }

  ParallelRangeSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (sData->total_points > 1000);
  BLI_task_parallel_range(
      0, sData->total_points, sData, dynamic_paint_prepare_adjacency_cb, &settings);

  /* calculate average values (single thread).
   * Note: tried to put this in threaded callback (using _finalize feature), but gave ~30% slower result! */
  bData->average_dist = 0.0;
  for (index = 0; index < sData->total_points; index++) {
    int numOfNeighs = adj_data->n_num[index];

    for (int i = 0; i < numOfNeighs; i++) {
      bData->average_dist += (double)bNeighs[adj_data->n_index[index] + i].dist;
    }
  }
  bData->average_dist /= adj_data->total_targets;
}

/* find two adjacency points (closest_id) and influence (closest_d) to move paint towards when affected by a force  */
static void surface_determineForceTargetPoints(const PaintSurfaceData *sData,
                                               const int index,
                                               const float force[3],
                                               float closest_d[2],
                                               int closest_id[2])
{
  BakeAdjPoint *bNeighs = sData->bData->bNeighs;
  const int numOfNeighs = sData->adj_data->n_num[index];
  int i;

  closest_id[0] = closest_id[1] = -1;
  closest_d[0] = closest_d[1] = -1.0f;

  /* find closest neigh */
  for (i = 0; i < numOfNeighs; i++) {
    const int n_index = sData->adj_data->n_index[index] + i;
    const float dir_dot = dot_v3v3(bNeighs[n_index].dir, force);

    if (dir_dot > closest_d[0] && dir_dot > 0.0f) {
      closest_d[0] = dir_dot;
      closest_id[0] = n_index;
    }
  }

  if (closest_d[0] < 0.0f) {
    return;
  }

  /* find second closest neigh */
  for (i = 0; i < numOfNeighs; i++) {
    const int n_index = sData->adj_data->n_index[index] + i;

    if (n_index == closest_id[0]) {
      continue;
    }

    const float dir_dot = dot_v3v3(bNeighs[n_index].dir, force);
    const float closest_dot = dot_v3v3(bNeighs[n_index].dir, bNeighs[closest_id[0]].dir);

    /* only accept neighbor at "other side" of the first one in relation to force dir
     * so make sure angle between this and closest neigh is greater than first angle. */
    if (dir_dot > closest_d[1] && closest_dot < closest_d[0] && dir_dot > 0.0f) {
      closest_d[1] = dir_dot;
      closest_id[1] = n_index;
    }
  }

  /* if two valid neighs found, calculate how force effect is divided evenly between them
   * (so that d[0] + d[1] = 1.0) */
  if (closest_id[1] != -1) {
    float force_proj[3];
    float tangent[3];
    const float neigh_diff = acosf(
        dot_v3v3(bNeighs[closest_id[0]].dir, bNeighs[closest_id[1]].dir));
    float force_intersect;
    float temp;

    /* project force vector on the plane determined by these two neighbor points
     * and calculate relative force angle from it. */
    cross_v3_v3v3(tangent, bNeighs[closest_id[0]].dir, bNeighs[closest_id[1]].dir);
    normalize_v3(tangent);
    force_intersect = dot_v3v3(force, tangent);
    madd_v3_v3v3fl(force_proj, force, tangent, (-1.0f) * force_intersect);
    normalize_v3(force_proj);

    /* get drip factor based on force dir in relation to angle between those neighbors */
    temp = dot_v3v3(bNeighs[closest_id[0]].dir, force_proj);
    CLAMP(temp, -1.0f, 1.0f); /* float precision might cause values > 1.0f that return infinite */
    closest_d[1] = acosf(temp) / neigh_diff;
    closest_d[0] = 1.0f - closest_d[1];

    /* and multiply depending on how deeply force intersects surface */
    temp = fabsf(force_intersect);
    CLAMP(temp, 0.0f, 1.0f);
    mul_v2_fl(closest_d, acosf(temp) / (float)M_PI_2);
  }
  else {
    /* if only single neighbor, still linearize force intersection effect */
    closest_d[0] = 1.0f - acosf(closest_d[0]) / (float)M_PI_2;
  }
}

static void dynamicPaint_doSmudge(DynamicPaintSurface *surface,
                                  DynamicPaintBrushSettings *brush,
                                  float timescale)
{
  PaintSurfaceData *sData = surface->data;
  PaintBakeData *bData = sData->bData;
  BakeAdjPoint *bNeighs = sData->bData->bNeighs;
  int index, steps, step;
  float eff_scale, max_velocity = 0.0f;

  if (!sData->adj_data) {
    return;
  }

  /* find max velocity */
  for (index = 0; index < sData->total_points; index++) {
    float vel = bData->brush_velocity[index * 4 + 3];
    CLAMP_MIN(max_velocity, vel);
  }

  steps = (int)ceil((double)max_velocity / bData->average_dist * (double)timescale);
  CLAMP(steps, 0, 12);
  eff_scale = brush->smudge_strength / (float)steps * timescale;

  for (step = 0; step < steps; step++) {
    for (index = 0; index < sData->total_points; index++) {
      int i;

      if (sData->adj_data->flags[index] & ADJ_BORDER_PIXEL) {
        continue;
      }

      PaintPoint *pPoint = &((PaintPoint *)sData->type_data)[index];
      float smudge_str = bData->brush_velocity[index * 4 + 3];

      /* force targets */
      int closest_id[2];
      float closest_d[2];

      if (!smudge_str) {
        continue;
      }

      /* get force affect points */
      surface_determineForceTargetPoints(
          sData, index, &bData->brush_velocity[index * 4], closest_d, closest_id);

      /* Apply movement towards those two points */
      for (i = 0; i < 2; i++) {
        int n_index = closest_id[i];
        if (n_index != -1 && closest_d[i] > 0.0f) {
          float dir_dot = closest_d[i], dir_factor;
          float speed_scale = eff_scale * smudge_str / bNeighs[n_index].dist;
          PaintPoint *ePoint = &(
              (PaintPoint *)sData->type_data)[sData->adj_data->n_target[n_index]];

          /* just skip if angle is too extreme */
          if (dir_dot <= 0.0f) {
            continue;
          }

          dir_factor = dir_dot * speed_scale;
          CLAMP_MAX(dir_factor, brush->smudge_strength);

          /* mix new color and alpha */
          mixColors(ePoint->color, ePoint->color[3], pPoint->color, pPoint->color[3], dir_factor);
          ePoint->color[3] = ePoint->color[3] * (1.0f - dir_factor) +
                             pPoint->color[3] * dir_factor;

          /* smudge "wet layer" */
          mixColors(ePoint->e_color,
                    ePoint->e_color[3],
                    pPoint->e_color,
                    pPoint->e_color[3],
                    dir_factor);
          ePoint->e_color[3] = ePoint->e_color[3] * (1.0f - dir_factor) +
                               pPoint->e_color[3] * dir_factor;
          pPoint->wetness *= (1.0f - dir_factor);
        }
      }
    }
  }
}

typedef struct DynamicPaintEffectData {
  const DynamicPaintSurface *surface;
  Scene *scene;

  float *force;
  ListBase *effectors;
  const void *prevPoint;
  const float eff_scale;

  uint8_t *point_locks;

  const float wave_speed;
  const float wave_scale;
  const float wave_max_slope;

  const float dt;
  const float min_dist;
  const float damp_factor;
  const bool reset_wave;
} DynamicPaintEffectData;

/*
 * Prepare data required by effects for current frame.
 * Returns number of steps required
 */
static void dynamic_paint_prepare_effect_cb(void *__restrict userdata,
                                            const int index,
                                            const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintEffectData *data = userdata;

  const DynamicPaintSurface *surface = data->surface;
  const PaintSurfaceData *sData = surface->data;
  const PaintBakeData *bData = sData->bData;
  Vec3f *realCoord = bData->realCoord;

  Scene *scene = data->scene;

  float *force = data->force;
  ListBase *effectors = data->effectors;

  float forc[3] = {0};
  float vel[3] = {0};

  /* apply force fields */
  if (effectors) {
    EffectedPoint epoint;
    pd_point_from_loc(scene, realCoord[bData->s_pos[index]].v, vel, index, &epoint);
    epoint.vel_to_sec = 1.0f;
    BKE_effectors_apply(effectors, NULL, surface->effector_weights, &epoint, forc, NULL);
  }

  /* if global gravity is enabled, add it too */
  if (scene->physics_settings.flag & PHYS_GLOBAL_GRAVITY) {
    /* also divide by 10 to about match default grav
     * with default force strength (1.0). */
    madd_v3_v3fl(forc,
                 scene->physics_settings.gravity,
                 surface->effector_weights->global_gravity * surface->effector_weights->weight[0] /
                     10.f);
  }

  /* add surface point velocity and acceleration if enabled */
  if (bData->velocity) {
    if (surface->drip_vel) {
      madd_v3_v3fl(forc, bData->velocity[index].v, surface->drip_vel * (-1.0f));
    }

    /* acceleration */
    if (bData->prev_velocity && surface->drip_acc) {
      float acc[3];
      copy_v3_v3(acc, bData->velocity[index].v);
      sub_v3_v3(acc, bData->prev_velocity[index].v);
      madd_v3_v3fl(forc, acc, surface->drip_acc * (-1.0f));
    }
  }

  /* force strength, and normalize force vec */
  force[index * 4 + 3] = normalize_v3_v3(&force[index * 4], forc);
}

static int dynamicPaint_prepareEffectStep(struct Depsgraph *depsgraph,
                                          DynamicPaintSurface *surface,
                                          Scene *scene,
                                          Object *ob,
                                          float **force,
                                          float timescale)
{
  double average_force = 0.0f;
  float shrink_speed = 0.0f, spread_speed = 0.0f;
  float fastest_effect, avg_dist;
  int steps;
  PaintSurfaceData *sData = surface->data;
  PaintBakeData *bData = sData->bData;

  /* Init force data if required */
  if (surface->effect & MOD_DPAINT_EFFECT_DO_DRIP) {
    ListBase *effectors = BKE_effectors_create(depsgraph, ob, NULL, surface->effector_weights);

    /* allocate memory for force data (dir vector + strength) */
    *force = MEM_mallocN(sData->total_points * 4 * sizeof(float), "PaintEffectForces");

    if (*force) {
      DynamicPaintEffectData data = {
          .surface = surface,
          .scene = scene,
          .force = *force,
          .effectors = effectors,
      };
      ParallelRangeSettings settings;
      BLI_parallel_range_settings_defaults(&settings);
      settings.use_threading = (sData->total_points > 1000);
      BLI_task_parallel_range(
          0, sData->total_points, &data, dynamic_paint_prepare_effect_cb, &settings);

      /* calculate average values (single thread) */
      for (int index = 0; index < sData->total_points; index++) {
        average_force += (double)(*force)[index * 4 + 3];
      }
      average_force /= sData->total_points;
    }
    BKE_effectors_free(effectors);
  }

  /* Get number of required steps using average point distance
   * so that just a few ultra close pixels wont up substeps to max. */

  /* adjust number of required substep by fastest active effect */
  if (surface->effect & MOD_DPAINT_EFFECT_DO_SPREAD) {
    spread_speed = surface->spread_speed;
  }
  if (surface->effect & MOD_DPAINT_EFFECT_DO_SHRINK) {
    shrink_speed = surface->shrink_speed;
  }

  fastest_effect = max_fff(spread_speed, shrink_speed, average_force);
  avg_dist = bData->average_dist * (double)CANVAS_REL_SIZE / (double)getSurfaceDimension(sData);

  steps = (int)ceilf(1.5f * EFF_MOVEMENT_PER_FRAME * fastest_effect / avg_dist * timescale);
  CLAMP(steps, 1, 20);

  return steps;
}

/**
 * Processes active effect step.
 */
static void dynamic_paint_effect_spread_cb(void *__restrict userdata,
                                           const int index,
                                           const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintEffectData *data = userdata;

  const DynamicPaintSurface *surface = data->surface;
  const PaintSurfaceData *sData = surface->data;

  if (sData->adj_data->flags[index] & ADJ_BORDER_PIXEL) {
    return;
  }

  const int numOfNeighs = sData->adj_data->n_num[index];
  BakeAdjPoint *bNeighs = sData->bData->bNeighs;
  PaintPoint *pPoint = &((PaintPoint *)sData->type_data)[index];
  const PaintPoint *prevPoint = data->prevPoint;
  const float eff_scale = data->eff_scale;

  const int *n_index = sData->adj_data->n_index;
  const int *n_target = sData->adj_data->n_target;

  /* Loop through neighboring points */
  for (int i = 0; i < numOfNeighs; i++) {
    const int n_idx = n_index[index] + i;
    float w_factor;
    const PaintPoint *pPoint_prev = &prevPoint[n_target[n_idx]];
    const float speed_scale = (bNeighs[n_idx].dist < eff_scale) ? 1.0f :
                                                                  eff_scale / bNeighs[n_idx].dist;
    const float color_mix = min_fff(pPoint_prev->wetness, pPoint->wetness, 1.0f) * 0.25f *
                            surface->color_spread_speed;

    /* do color mixing */
    if (color_mix) {
      mixColors(pPoint->e_color,
                pPoint->e_color[3],
                pPoint_prev->e_color,
                pPoint_prev->e_color[3],
                color_mix);
    }

    /* Only continue if surrounding point has higher wetness */
    if (pPoint_prev->wetness < pPoint->wetness || pPoint_prev->wetness < MIN_WETNESS) {
      continue;
    }

    w_factor = 1.0f / numOfNeighs * min_ff(pPoint_prev->wetness, 1.0f) * speed_scale;
    CLAMP(w_factor, 0.0f, 1.0f);

    /* mix new wetness and color */
    pPoint->wetness = pPoint->wetness + w_factor * (pPoint_prev->wetness - pPoint->wetness);
    pPoint->e_color[3] = mixColors(pPoint->e_color,
                                   pPoint->e_color[3],
                                   pPoint_prev->e_color,
                                   pPoint_prev->e_color[3],
                                   w_factor);
  }
}

static void dynamic_paint_effect_shrink_cb(void *__restrict userdata,
                                           const int index,
                                           const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintEffectData *data = userdata;

  const DynamicPaintSurface *surface = data->surface;
  const PaintSurfaceData *sData = surface->data;

  if (sData->adj_data->flags[index] & ADJ_BORDER_PIXEL) {
    return;
  }

  const int numOfNeighs = sData->adj_data->n_num[index];
  BakeAdjPoint *bNeighs = sData->bData->bNeighs;
  PaintPoint *pPoint = &((PaintPoint *)sData->type_data)[index];
  const PaintPoint *prevPoint = data->prevPoint;
  const float eff_scale = data->eff_scale;
  float totalAlpha = 0.0f;

  const int *n_index = sData->adj_data->n_index;
  const int *n_target = sData->adj_data->n_target;

  /* Loop through neighboring points */
  for (int i = 0; i < numOfNeighs; i++) {
    const int n_idx = n_index[index] + i;
    const float speed_scale = (bNeighs[n_idx].dist < eff_scale) ? 1.0f :
                                                                  eff_scale / bNeighs[n_idx].dist;
    const PaintPoint *pPoint_prev = &prevPoint[n_target[n_idx]];
    float a_factor, ea_factor, w_factor;

    totalAlpha += pPoint_prev->e_color[3];

    /* Check if neighboring point has lower alpha,
     * if so, decrease this point's alpha as well. */
    if (pPoint->color[3] <= 0.0f && pPoint->e_color[3] <= 0.0f && pPoint->wetness <= 0.0f) {
      continue;
    }

    /* decrease factor for dry paint alpha */
    a_factor = max_ff((1.0f - pPoint_prev->color[3]) / numOfNeighs *
                          (pPoint->color[3] - pPoint_prev->color[3]) * speed_scale,
                      0.0f);
    /* decrease factor for wet paint alpha */
    ea_factor = max_ff((1.0f - pPoint_prev->e_color[3]) / 8 *
                           (pPoint->e_color[3] - pPoint_prev->e_color[3]) * speed_scale,
                       0.0f);
    /* decrease factor for paint wetness */
    w_factor = max_ff((1.0f - pPoint_prev->wetness) / 8 *
                          (pPoint->wetness - pPoint_prev->wetness) * speed_scale,
                      0.0f);

    pPoint->color[3] -= a_factor;
    CLAMP_MIN(pPoint->color[3], 0.0f);
    pPoint->e_color[3] -= ea_factor;
    CLAMP_MIN(pPoint->e_color[3], 0.0f);
    pPoint->wetness -= w_factor;
    CLAMP_MIN(pPoint->wetness, 0.0f);
  }
}

static void dynamic_paint_effect_drip_cb(void *__restrict userdata,
                                         const int index,
                                         const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintEffectData *data = userdata;

  const DynamicPaintSurface *surface = data->surface;
  const PaintSurfaceData *sData = surface->data;

  if (sData->adj_data->flags[index] & ADJ_BORDER_PIXEL) {
    return;
  }

  BakeAdjPoint *bNeighs = sData->bData->bNeighs;
  PaintPoint *pPoint = &((PaintPoint *)sData->type_data)[index];
  const PaintPoint *prevPoint = data->prevPoint;
  const PaintPoint *pPoint_prev = &prevPoint[index];
  const float *force = data->force;
  const float eff_scale = data->eff_scale;

  const int *n_target = sData->adj_data->n_target;

  uint8_t *point_locks = data->point_locks;

  int closest_id[2];
  float closest_d[2];

  /* adjust drip speed depending on wetness */
  float w_factor = pPoint_prev->wetness - 0.025f;
  if (w_factor <= 0) {
    return;
  }
  CLAMP(w_factor, 0.0f, 1.0f);

  float ppoint_wetness_diff = 0.0f;

  /* get force affect points */
  surface_determineForceTargetPoints(sData, index, &force[index * 4], closest_d, closest_id);

  /* Apply movement towards those two points */
  for (int i = 0; i < 2; i++) {
    const int n_idx = closest_id[i];
    if (n_idx != -1 && closest_d[i] > 0.0f) {
      const float dir_dot = closest_d[i];

      /* just skip if angle is too extreme */
      if (dir_dot <= 0.0f) {
        continue;
      }

      float dir_factor, a_factor;
      const float speed_scale = eff_scale * force[index * 4 + 3] / bNeighs[n_idx].dist;

      const unsigned int n_trgt = (unsigned int)n_target[n_idx];

      /* Sort of spinlock, but only for given ePoint.
       * Since the odds a same ePoint is modified at the same time by several threads is very low, this is
       * much more efficient than a global spin lock. */
      const unsigned int epointlock_idx = n_trgt / 8;
      const uint8_t epointlock_bitmask = 1 << (n_trgt & 7); /* 7 == 0b111 */
      while (atomic_fetch_and_or_uint8(&point_locks[epointlock_idx], epointlock_bitmask) &
             epointlock_bitmask) {
        ;
      }

      PaintPoint *ePoint = &((PaintPoint *)sData->type_data)[n_trgt];
      const float e_wet = ePoint->wetness;

      dir_factor = min_ff(0.5f, dir_dot * min_ff(speed_scale, 1.0f) * w_factor);

      /* mix new wetness */
      ePoint->wetness += dir_factor;
      CLAMP(ePoint->wetness, 0.0f, MAX_WETNESS);

      /* mix new color */
      a_factor = dir_factor / pPoint_prev->wetness;
      CLAMP(a_factor, 0.0f, 1.0f);
      mixColors(ePoint->e_color,
                ePoint->e_color[3],
                pPoint_prev->e_color,
                pPoint_prev->e_color[3],
                a_factor);
      /* dripping is supposed to preserve alpha level */
      if (pPoint_prev->e_color[3] > ePoint->e_color[3]) {
        ePoint->e_color[3] += a_factor * pPoint_prev->e_color[3];
        CLAMP_MAX(ePoint->e_color[3], pPoint_prev->e_color[3]);
      }

      /* Decrease paint wetness on current point
       * (just store diff here, that way we can only lock current point once at the end to apply it). */
      ppoint_wetness_diff += (ePoint->wetness - e_wet);

#ifndef NDEBUG
      {
        uint8_t ret = atomic_fetch_and_and_uint8(&point_locks[epointlock_idx],
                                                 ~epointlock_bitmask);
        BLI_assert(ret & epointlock_bitmask);
      }
#else
      atomic_fetch_and_and_uint8(&point_locks[epointlock_idx], ~epointlock_bitmask);
#endif
    }
  }

  {
    const unsigned int ppointlock_idx = index / 8;
    const uint8_t ppointlock_bitmask = 1 << (index & 7); /* 7 == 0b111 */
    while (atomic_fetch_and_or_uint8(&point_locks[ppointlock_idx], ppointlock_bitmask) &
           ppointlock_bitmask) {
      ;
    }

    pPoint->wetness -= ppoint_wetness_diff;
    CLAMP(pPoint->wetness, 0.0f, MAX_WETNESS);

#ifndef NDEBUG
    {
      uint8_t ret = atomic_fetch_and_and_uint8(&point_locks[ppointlock_idx], ~ppointlock_bitmask);
      BLI_assert(ret & ppointlock_bitmask);
    }
#else
    atomic_fetch_and_and_uint8(&point_locks[ppointlock_idx], ~ppointlock_bitmask);
#endif
  }
}

static void dynamicPaint_doEffectStep(DynamicPaintSurface *surface,
                                      float *force,
                                      PaintPoint *prevPoint,
                                      float timescale,
                                      float steps)
{
  PaintSurfaceData *sData = surface->data;

  const float distance_scale = getSurfaceDimension(sData) / CANVAS_REL_SIZE;
  timescale /= steps;

  if (!sData->adj_data) {
    return;
  }

  /*
   * Spread Effect
   */
  if (surface->effect & MOD_DPAINT_EFFECT_DO_SPREAD) {
    const float eff_scale = distance_scale * EFF_MOVEMENT_PER_FRAME * surface->spread_speed *
                            timescale;

    /* Copy current surface to the previous points array to read unmodified values */
    memcpy(prevPoint, sData->type_data, sData->total_points * sizeof(struct PaintPoint));

    DynamicPaintEffectData data = {
        .surface = surface,
        .prevPoint = prevPoint,
        .eff_scale = eff_scale,
    };
    ParallelRangeSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    settings.use_threading = (sData->total_points > 1000);
    BLI_task_parallel_range(
        0, sData->total_points, &data, dynamic_paint_effect_spread_cb, &settings);
  }

  /*
   * Shrink Effect
   */
  if (surface->effect & MOD_DPAINT_EFFECT_DO_SHRINK) {
    const float eff_scale = distance_scale * EFF_MOVEMENT_PER_FRAME * surface->shrink_speed *
                            timescale;

    /* Copy current surface to the previous points array to read unmodified values */
    memcpy(prevPoint, sData->type_data, sData->total_points * sizeof(struct PaintPoint));

    DynamicPaintEffectData data = {
        .surface = surface,
        .prevPoint = prevPoint,
        .eff_scale = eff_scale,
    };
    ParallelRangeSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    settings.use_threading = (sData->total_points > 1000);
    BLI_task_parallel_range(
        0, sData->total_points, &data, dynamic_paint_effect_shrink_cb, &settings);
  }

  /*
   * Drip Effect
   */
  if (surface->effect & MOD_DPAINT_EFFECT_DO_DRIP && force) {
    const float eff_scale = distance_scale * EFF_MOVEMENT_PER_FRAME * timescale / 2.0f;

    /* Same as BLI_bitmask, but handled atomicaly as 'ePoint' locks. */
    const size_t point_locks_size = (sData->total_points / 8) + 1;
    uint8_t *point_locks = MEM_callocN(sizeof(*point_locks) * point_locks_size, __func__);

    /* Copy current surface to the previous points array to read unmodified values */
    memcpy(prevPoint, sData->type_data, sData->total_points * sizeof(struct PaintPoint));

    DynamicPaintEffectData data = {
        .surface = surface,
        .prevPoint = prevPoint,
        .eff_scale = eff_scale,
        .force = force,
        .point_locks = point_locks,
    };
    ParallelRangeSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    settings.use_threading = (sData->total_points > 1000);
    BLI_task_parallel_range(
        0, sData->total_points, &data, dynamic_paint_effect_drip_cb, &settings);

    MEM_freeN(point_locks);
  }
}

static void dynamic_paint_border_cb(void *__restrict userdata,
                                    const int b_index,
                                    const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintEffectData *data = userdata;

  const DynamicPaintSurface *surface = data->surface;
  const PaintSurfaceData *sData = surface->data;

  const int index = sData->adj_data->border[b_index];

  const int numOfNeighs = sData->adj_data->n_num[index];
  PaintPoint *pPoint = &((PaintPoint *)sData->type_data)[index];

  const int *n_index = sData->adj_data->n_index;
  const int *n_target = sData->adj_data->n_target;

  /* Average neighboring points. Intermediaries use premultiplied alpha. */
  float mix_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float mix_e_color[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  float mix_wetness = 0.0f;

  for (int i = 0; i < numOfNeighs; i++) {
    const int n_idx = n_index[index] + i;
    const int target = n_target[n_idx];

    PaintPoint *pPoint2 = &((PaintPoint *)sData->type_data)[target];

    assert(!(sData->adj_data->flags[target] & ADJ_BORDER_PIXEL));

    madd_v3_v3fl(mix_color, pPoint2->color, pPoint2->color[3]);
    mix_color[3] += pPoint2->color[3];

    madd_v3_v3fl(mix_e_color, pPoint2->e_color, pPoint2->e_color[3]);
    mix_e_color[3] += pPoint2->e_color[3];

    mix_wetness += pPoint2->wetness;
  }

  const float divisor = 1.0f / numOfNeighs;

  if (mix_color[3]) {
    pPoint->color[3] = mix_color[3] * divisor;
    mul_v3_v3fl(pPoint->color, mix_color, divisor / pPoint->color[3]);
  }
  else {
    pPoint->color[3] = 0.0f;
  }

  if (mix_e_color[3]) {
    pPoint->e_color[3] = mix_e_color[3] * divisor;
    mul_v3_v3fl(pPoint->e_color, mix_e_color, divisor / pPoint->e_color[3]);
  }
  else {
    pPoint->e_color[3] = 0.0f;
  }

  pPoint->wetness = mix_wetness / numOfNeighs;
}

static void dynamicPaint_doBorderStep(DynamicPaintSurface *surface)
{
  PaintSurfaceData *sData = surface->data;

  if (!sData->adj_data || !sData->adj_data->border) {
    return;
  }

  /* Don't use prevPoint, relying on the condition that neighbors are never border pixels. */
  DynamicPaintEffectData data = {
      .surface = surface,
  };

  ParallelRangeSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (sData->adj_data->total_border > 1000);
  BLI_task_parallel_range(
      0, sData->adj_data->total_border, &data, dynamic_paint_border_cb, &settings);
}

static void dynamic_paint_wave_step_cb(void *__restrict userdata,
                                       const int index,
                                       const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintEffectData *data = userdata;

  const DynamicPaintSurface *surface = data->surface;
  const PaintSurfaceData *sData = surface->data;
  BakeAdjPoint *bNeighs = sData->bData->bNeighs;
  const PaintWavePoint *prevPoint = data->prevPoint;

  const float wave_speed = data->wave_speed;
  const float wave_scale = data->wave_scale;
  const float wave_max_slope = data->wave_max_slope;

  const float dt = data->dt;
  const float min_dist = data->min_dist;
  const float damp_factor = data->damp_factor;

  PaintWavePoint *wPoint = &((PaintWavePoint *)sData->type_data)[index];
  const int numOfNeighs = sData->adj_data->n_num[index];
  float force = 0.0f, avg_dist = 0.0f, avg_height = 0.0f, avg_n_height = 0.0f;
  int numOfN = 0, numOfRN = 0;

  if (wPoint->state > 0) {
    return;
  }

  const int *n_index = sData->adj_data->n_index;
  const int *n_target = sData->adj_data->n_target;
  const int *adj_flags = sData->adj_data->flags;

  /* calculate force from surrounding points */
  for (int i = 0; i < numOfNeighs; i++) {
    const int n_idx = n_index[index] + i;
    float dist = bNeighs[n_idx].dist * wave_scale;
    const PaintWavePoint *tPoint = &prevPoint[n_target[n_idx]];

    if (!dist || tPoint->state > 0) {
      continue;
    }

    CLAMP_MIN(dist, min_dist);
    avg_dist += dist;
    numOfN++;

    /* count average height for edge points for open borders */
    if (!(adj_flags[n_target[n_idx]] & ADJ_ON_MESH_EDGE)) {
      avg_n_height += tPoint->height;
      numOfRN++;
    }

    force += (tPoint->height - wPoint->height) / (dist * dist);
    avg_height += tPoint->height;
  }
  avg_dist = (numOfN) ? avg_dist / numOfN : 0.0f;

  if (surface->flags & MOD_DPAINT_WAVE_OPEN_BORDERS && adj_flags[index] & ADJ_ON_MESH_EDGE) {
    /* if open borders, apply a fake height to keep waves going on */
    avg_n_height = (numOfRN) ? avg_n_height / numOfRN : 0.0f;
    wPoint->height = (dt * wave_speed * avg_n_height + wPoint->height * avg_dist) /
                     (avg_dist + dt * wave_speed);
  }
  /* else do wave eq */
  else {
    /* add force towards zero height based on average dist */
    if (avg_dist) {
      force += (0.0f - wPoint->height) * surface->wave_spring / (avg_dist * avg_dist) / 2.0f;
    }

    /* change point velocity */
    wPoint->velocity += force * dt * wave_speed * wave_speed;
    /* damping */
    wPoint->velocity *= damp_factor;
    /* and new height */
    wPoint->height += wPoint->velocity * dt;

    /* limit wave slope steepness */
    if (wave_max_slope && avg_dist) {
      const float max_offset = wave_max_slope * avg_dist;
      const float offset = (numOfN) ? (avg_height / numOfN - wPoint->height) : 0.0f;
      if (offset > max_offset) {
        wPoint->height += offset - max_offset;
      }
      else if (offset < -max_offset) {
        wPoint->height += offset + max_offset;
      }
    }
  }

  if (data->reset_wave) {
    /* if there wasn't any brush intersection, clear isect height */
    if (wPoint->state == DPAINT_WAVE_NONE) {
      wPoint->brush_isect = 0.0f;
    }
    wPoint->state = DPAINT_WAVE_NONE;
  }
}

static void dynamicPaint_doWaveStep(DynamicPaintSurface *surface, float timescale)
{
  PaintSurfaceData *sData = surface->data;
  BakeAdjPoint *bNeighs = sData->bData->bNeighs;
  int index;
  int steps, ss;
  float dt, min_dist, damp_factor;
  const float wave_speed = surface->wave_speed;
  const float wave_max_slope = (surface->wave_smoothness >= 0.01f) ?
                                   (0.5f / surface->wave_smoothness) :
                                   0.0f;
  double average_dist = 0.0f;
  const float canvas_size = getSurfaceDimension(sData);
  const float wave_scale = CANVAS_REL_SIZE / canvas_size;

  /* allocate memory */
  PaintWavePoint *prevPoint = MEM_mallocN(sData->total_points * sizeof(PaintWavePoint), __func__);
  if (!prevPoint) {
    return;
  }

  /* calculate average neigh distance (single thread) */
  for (index = 0; index < sData->total_points; index++) {
    int i;
    int numOfNeighs = sData->adj_data->n_num[index];

    for (i = 0; i < numOfNeighs; i++) {
      average_dist += (double)bNeighs[sData->adj_data->n_index[index] + i].dist;
    }
  }
  average_dist *= (double)wave_scale / sData->adj_data->total_targets;

  /* determine number of required steps */
  steps = (int)ceil((double)(WAVE_TIME_FAC * timescale * surface->wave_timescale) /
                    (average_dist / (double)wave_speed / 3));
  CLAMP(steps, 1, 20);
  timescale /= steps;

  /* apply simulation values for final timescale */
  dt = WAVE_TIME_FAC * timescale * surface->wave_timescale;
  min_dist = wave_speed * dt * 1.5f;
  damp_factor = pow((1.0f - surface->wave_damping), timescale * surface->wave_timescale);

  for (ss = 0; ss < steps; ss++) {
    /* copy previous frame data */
    memcpy(prevPoint, sData->type_data, sData->total_points * sizeof(PaintWavePoint));

    DynamicPaintEffectData data = {
        .surface = surface,
        .prevPoint = prevPoint,
        .wave_speed = wave_speed,
        .wave_scale = wave_scale,
        .wave_max_slope = wave_max_slope,
        .dt = dt,
        .min_dist = min_dist,
        .damp_factor = damp_factor,
        .reset_wave = (ss == steps - 1),
    };
    ParallelRangeSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    settings.use_threading = (sData->total_points > 1000);
    BLI_task_parallel_range(0, sData->total_points, &data, dynamic_paint_wave_step_cb, &settings);
  }

  MEM_freeN(prevPoint);
}

/* Do dissolve and fading effects */
static bool dynamic_paint_surface_needs_dry_dissolve(DynamicPaintSurface *surface)
{
  return (((surface->type == MOD_DPAINT_SURFACE_T_PAINT) &&
           (surface->flags & (MOD_DPAINT_USE_DRYING | MOD_DPAINT_DISSOLVE))) ||
          (ELEM(surface->type, MOD_DPAINT_SURFACE_T_DISPLACE, MOD_DPAINT_SURFACE_T_WEIGHT) &&
           (surface->flags & MOD_DPAINT_DISSOLVE)));
}

typedef struct DynamicPaintDissolveDryData {
  const DynamicPaintSurface *surface;
  const float timescale;
} DynamicPaintDissolveDryData;

static void dynamic_paint_surface_pre_step_cb(void *__restrict userdata,
                                              const int index,
                                              const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintDissolveDryData *data = userdata;

  const DynamicPaintSurface *surface = data->surface;
  const PaintSurfaceData *sData = surface->data;
  const float timescale = data->timescale;

  /* Do drying dissolve effects */
  if (surface->type == MOD_DPAINT_SURFACE_T_PAINT) {
    PaintPoint *pPoint = &((PaintPoint *)sData->type_data)[index];
    /* drying */
    if (surface->flags & MOD_DPAINT_USE_DRYING) {
      if (pPoint->wetness >= MIN_WETNESS) {
        int i;
        float dry_ratio, f_color[4];
        float p_wetness = pPoint->wetness;

        value_dissolve(&pPoint->wetness,
                       surface->dry_speed,
                       timescale,
                       (surface->flags & MOD_DPAINT_DRY_LOG) != 0);
        CLAMP_MIN(pPoint->wetness, 0.0f);

        if (pPoint->wetness < surface->color_dry_threshold) {
          dry_ratio = pPoint->wetness / p_wetness;

          /*
           * Slowly "shift" paint from wet layer to dry layer as it drys:
           */
          /* make sure alpha values are within proper range */
          CLAMP(pPoint->color[3], 0.0f, 1.0f);
          CLAMP(pPoint->e_color[3], 0.0f, 1.0f);

          /* get current final blended color of these layers */
          blendColors(
              pPoint->color, pPoint->color[3], pPoint->e_color, pPoint->e_color[3], f_color);
          /* reduce wet layer alpha by dry factor */
          pPoint->e_color[3] *= dry_ratio;

          /* now calculate new alpha for dry layer that keeps final blended color unchanged */
          pPoint->color[3] = (f_color[3] - pPoint->e_color[3]) / (1.0f - pPoint->e_color[3]);
          /* for each rgb component, calculate a new dry layer color that keeps the final blend color
           * with these new alpha values. (wet layer color doesn't change)*/
          if (pPoint->color[3]) {
            for (i = 0; i < 3; i++) {
              pPoint->color[i] = (f_color[i] * f_color[3] -
                                  pPoint->e_color[i] * pPoint->e_color[3]) /
                                 (pPoint->color[3] * (1.0f - pPoint->e_color[3]));
            }
          }
        }

        pPoint->state = DPAINT_PAINT_WET;
      }
      /* in case of just dryed paint, just mix it to the dry layer and mark it empty */
      else if (pPoint->state > 0) {
        float f_color[4];
        blendColors(pPoint->color, pPoint->color[3], pPoint->e_color, pPoint->e_color[3], f_color);
        copy_v4_v4(pPoint->color, f_color);
        /* clear wet layer */
        pPoint->wetness = 0.0f;
        pPoint->e_color[3] = 0.0f;
        pPoint->state = DPAINT_PAINT_DRY;
      }
    }

    if (surface->flags & MOD_DPAINT_DISSOLVE) {
      value_dissolve(&pPoint->color[3],
                     surface->diss_speed,
                     timescale,
                     (surface->flags & MOD_DPAINT_DISSOLVE_LOG) != 0);
      CLAMP_MIN(pPoint->color[3], 0.0f);

      value_dissolve(&pPoint->e_color[3],
                     surface->diss_speed,
                     timescale,
                     (surface->flags & MOD_DPAINT_DISSOLVE_LOG) != 0);
      CLAMP_MIN(pPoint->e_color[3], 0.0f);
    }
  }
  /* dissolve for float types */
  else if (surface->flags & MOD_DPAINT_DISSOLVE &&
           (surface->type == MOD_DPAINT_SURFACE_T_DISPLACE ||
            surface->type == MOD_DPAINT_SURFACE_T_WEIGHT)) {
    float *point = &((float *)sData->type_data)[index];
    /* log or linear */
    value_dissolve(
        point, surface->diss_speed, timescale, (surface->flags & MOD_DPAINT_DISSOLVE_LOG) != 0);
    CLAMP_MIN(*point, 0.0f);
  }
}

static bool dynamicPaint_surfaceHasMoved(DynamicPaintSurface *surface, Object *ob)
{
  PaintSurfaceData *sData = surface->data;
  PaintBakeData *bData = sData->bData;
  Mesh *mesh = dynamicPaint_canvas_mesh_get(surface->canvas);
  MVert *mvert = mesh->mvert;

  int numOfVerts = mesh->totvert;
  int i;

  if (!bData->prev_verts) {
    return true;
  }

  /* matrix comparison */
  if (!equals_m4m4(bData->prev_obmat, ob->obmat)) {
    return true;
  }

  /* vertices */
  for (i = 0; i < numOfVerts; i++) {
    if (!equals_v3v3(bData->prev_verts[i].co, mvert[i].co)) {
      return true;
    }
  }

  return false;
}

/* Prepare for surface step by creating PaintBakeNormal data */
typedef struct DynamicPaintGenerateBakeData {
  const DynamicPaintSurface *surface;
  Object *ob;

  const MVert *mvert;
  const Vec3f *canvas_verts;

  const bool do_velocity_data;
  const bool new_bdata;
} DynamicPaintGenerateBakeData;

static void dynamic_paint_generate_bake_data_cb(void *__restrict userdata,
                                                const int index,
                                                const ParallelRangeTLS *__restrict UNUSED(tls))
{
  const DynamicPaintGenerateBakeData *data = userdata;

  const DynamicPaintSurface *surface = data->surface;
  const PaintSurfaceData *sData = surface->data;
  const PaintAdjData *adj_data = sData->adj_data;
  const PaintBakeData *bData = sData->bData;

  Object *ob = data->ob;

  const MVert *mvert = data->mvert;
  const Vec3f *canvas_verts = data->canvas_verts;

  const bool do_velocity_data = data->do_velocity_data;
  const bool new_bdata = data->new_bdata;

  float prev_point[3] = {0.0f, 0.0f, 0.0f};
  float temp_nor[3];

  if (do_velocity_data && !new_bdata) {
    copy_v3_v3(prev_point, bData->realCoord[bData->s_pos[index]].v);
  }

  /*
   * Calculate current 3D-position and normal of each surface point
   */
  if (surface->format == MOD_DPAINT_SURFACE_F_IMAGESEQ) {
    float n1[3], n2[3], n3[3];
    const ImgSeqFormatData *f_data = (ImgSeqFormatData *)sData->format_data;
    const PaintUVPoint *tPoint = &((PaintUVPoint *)f_data->uv_p)[index];

    bData->s_num[index] = (surface->flags & MOD_DPAINT_ANTIALIAS) ? 5 : 1;
    bData->s_pos[index] = index * bData->s_num[index];

    /* per sample coordinates */
    for (int ss = 0; ss < bData->s_num[index]; ss++) {
      interp_v3_v3v3v3(bData->realCoord[bData->s_pos[index] + ss].v,
                       canvas_verts[tPoint->v1].v,
                       canvas_verts[tPoint->v2].v,
                       canvas_verts[tPoint->v3].v,
                       f_data->barycentricWeights[index * bData->s_num[index] + ss].v);
    }

    /* Calculate current pixel surface normal */
    normal_short_to_float_v3(n1, mvert[tPoint->v1].no);
    normal_short_to_float_v3(n2, mvert[tPoint->v2].no);
    normal_short_to_float_v3(n3, mvert[tPoint->v3].no);

    interp_v3_v3v3v3(
        temp_nor, n1, n2, n3, f_data->barycentricWeights[index * bData->s_num[index]].v);
    normalize_v3(temp_nor);
    if (ELEM(surface->type, MOD_DPAINT_SURFACE_T_DISPLACE, MOD_DPAINT_SURFACE_T_WAVE)) {
      /* Prepare surface normal directional scale to easily convert
       * brush intersection amount between global and local space */
      float scaled_nor[3];
      mul_v3_v3v3(scaled_nor, temp_nor, ob->scale);
      bData->bNormal[index].normal_scale = len_v3(scaled_nor);
    }
    mul_mat3_m4_v3(ob->obmat, temp_nor);
    normalize_v3(temp_nor);
    negate_v3_v3(bData->bNormal[index].invNorm, temp_nor);
  }
  else if (surface->format == MOD_DPAINT_SURFACE_F_VERTEX) {
    int ss;
    if (surface->flags & MOD_DPAINT_ANTIALIAS && adj_data) {
      bData->s_num[index] = adj_data->n_num[index] + 1;
      bData->s_pos[index] = adj_data->n_index[index] + index;
    }
    else {
      bData->s_num[index] = 1;
      bData->s_pos[index] = index;
    }

    /* calculate position for each sample */
    for (ss = 0; ss < bData->s_num[index]; ss++) {
      /* first sample is always point center */
      copy_v3_v3(bData->realCoord[bData->s_pos[index] + ss].v, canvas_verts[index].v);
      if (ss > 0) {
        int t_index = adj_data->n_index[index] + (ss - 1);
        /* get vertex position at 1/3 of each neigh edge */
        mul_v3_fl(bData->realCoord[bData->s_pos[index] + ss].v, 2.0f / 3.0f);
        madd_v3_v3fl(bData->realCoord[bData->s_pos[index] + ss].v,
                     canvas_verts[adj_data->n_target[t_index]].v,
                     1.0f / 3.0f);
      }
    }

    /* normal */
    normal_short_to_float_v3(temp_nor, mvert[index].no);
    if (ELEM(surface->type, MOD_DPAINT_SURFACE_T_DISPLACE, MOD_DPAINT_SURFACE_T_WAVE)) {
      /* Prepare surface normal directional scale to easily convert
       * brush intersection amount between global and local space */
      float scaled_nor[3];
      mul_v3_v3v3(scaled_nor, temp_nor, ob->scale);
      bData->bNormal[index].normal_scale = len_v3(scaled_nor);
    }
    mul_mat3_m4_v3(ob->obmat, temp_nor);
    normalize_v3(temp_nor);
    negate_v3_v3(bData->bNormal[index].invNorm, temp_nor);
  }

  /* calculate speed vector */
  if (do_velocity_data && !new_bdata && !bData->clear) {
    sub_v3_v3v3(bData->velocity[index].v, bData->realCoord[bData->s_pos[index]].v, prev_point);
  }
}

static int dynamicPaint_generateBakeData(DynamicPaintSurface *surface,
                                         Depsgraph *depsgraph,
                                         Object *ob)
{
  PaintSurfaceData *sData = surface->data;
  PaintBakeData *bData = sData->bData;
  Mesh *mesh = dynamicPaint_canvas_mesh_get(surface->canvas);
  int index;
  bool new_bdata = false;
  const bool do_velocity_data = ((surface->effect & MOD_DPAINT_EFFECT_DO_DRIP) ||
                                 (surface_getBrushFlags(surface, depsgraph) &
                                  BRUSH_USES_VELOCITY));
  const bool do_accel_data = (surface->effect & MOD_DPAINT_EFFECT_DO_DRIP) != 0;

  int canvasNumOfVerts = mesh->totvert;
  MVert *mvert = mesh->mvert;
  Vec3f *canvas_verts;

  if (bData) {
    const bool surface_moved = dynamicPaint_surfaceHasMoved(surface, ob);

    /* get previous speed for accelertaion */
    if (do_accel_data && bData->prev_velocity && bData->velocity) {
      memcpy(bData->prev_velocity, bData->velocity, sData->total_points * sizeof(Vec3f));
    }

    /* reset speed vectors */
    if (do_velocity_data && bData->velocity && (bData->clear || !surface_moved)) {
      memset(bData->velocity, 0, sData->total_points * sizeof(Vec3f));
    }

    /* if previous data exists and mesh hasn't moved, no need to recalc */
    if (!surface_moved) {
      return 1;
    }
  }

  canvas_verts = (struct Vec3f *)MEM_mallocN(canvasNumOfVerts * sizeof(struct Vec3f),
                                             "Dynamic Paint transformed canvas verts");
  if (!canvas_verts) {
    return 0;
  }

  /* allocate memory if required */
  if (!bData) {
    sData->bData = bData = (struct PaintBakeData *)MEM_callocN(sizeof(struct PaintBakeData),
                                                               "Dynamic Paint bake data");
    if (!bData) {
      if (canvas_verts) {
        MEM_freeN(canvas_verts);
      }
      return 0;
    }

    /* Init bdata */
    bData->bNormal = (struct PaintBakeNormal *)MEM_mallocN(
        sData->total_points * sizeof(struct PaintBakeNormal), "Dynamic Paint step data");
    bData->s_pos = MEM_mallocN(sData->total_points * sizeof(unsigned int),
                               "Dynamic Paint bData s_pos");
    bData->s_num = MEM_mallocN(sData->total_points * sizeof(unsigned int),
                               "Dynamic Paint bData s_num");
    bData->realCoord = (struct Vec3f *)MEM_mallocN(surface_totalSamples(surface) * sizeof(Vec3f),
                                                   "Dynamic Paint point coords");
    bData->prev_verts = MEM_mallocN(canvasNumOfVerts * sizeof(MVert),
                                    "Dynamic Paint bData prev_verts");

    /* if any allocation failed, free everything */
    if (!bData->bNormal || !bData->s_pos || !bData->s_num || !bData->realCoord || !canvas_verts) {
      if (bData->bNormal) {
        MEM_freeN(bData->bNormal);
      }
      if (bData->s_pos) {
        MEM_freeN(bData->s_pos);
      }
      if (bData->s_num) {
        MEM_freeN(bData->s_num);
      }
      if (bData->realCoord) {
        MEM_freeN(bData->realCoord);
      }
      if (canvas_verts) {
        MEM_freeN(canvas_verts);
      }

      return setError(surface->canvas, N_("Not enough free memory"));
    }

    new_bdata = true;
  }

  if (do_velocity_data && !bData->velocity) {
    bData->velocity = (struct Vec3f *)MEM_callocN(sData->total_points * sizeof(Vec3f),
                                                  "Dynamic Paint velocity");
  }
  if (do_accel_data && !bData->prev_velocity) {
    bData->prev_velocity = (struct Vec3f *)MEM_mallocN(sData->total_points * sizeof(Vec3f),
                                                       "Dynamic Paint prev velocity");
    /* copy previous vel */
    if (bData->prev_velocity && bData->velocity) {
      memcpy(bData->prev_velocity, bData->velocity, sData->total_points * sizeof(Vec3f));
    }
  }

  /*
   * Make a transformed copy of canvas derived mesh vertices to avoid recalculation.
   */
  bData->mesh_bounds.valid = false;
  for (index = 0; index < canvasNumOfVerts; index++) {
    copy_v3_v3(canvas_verts[index].v, mvert[index].co);
    mul_m4_v3(ob->obmat, canvas_verts[index].v);
    boundInsert(&bData->mesh_bounds, canvas_verts[index].v);
  }

  /*
   * Prepare each surface point for a new step
   */
  DynamicPaintGenerateBakeData data = {
      .surface = surface,
      .ob = ob,
      .mvert = mvert,
      .canvas_verts = canvas_verts,
      .do_velocity_data = do_velocity_data,
      .new_bdata = new_bdata,
  };
  ParallelRangeSettings settings;
  BLI_parallel_range_settings_defaults(&settings);
  settings.use_threading = (sData->total_points > 1000);
  BLI_task_parallel_range(
      0, sData->total_points, &data, dynamic_paint_generate_bake_data_cb, &settings);

  MEM_freeN(canvas_verts);

  /* generate surface space partitioning grid */
  surfaceGenerateGrid(surface);
  /* calculate current frame adjacency point distances and global dirs */
  dynamicPaint_prepareAdjacencyData(surface, false);

  /* Copy current frame vertices to check against in next frame */
  copy_m4_m4(bData->prev_obmat, ob->obmat);
  memcpy(bData->prev_verts, mvert, canvasNumOfVerts * sizeof(MVert));

  bData->clear = 0;

  return 1;
}

/*
 * Do Dynamic Paint step. Paints scene brush objects of current state/frame to the surface.
 */
static int dynamicPaint_doStep(Depsgraph *depsgraph,
                               Scene *scene,
                               Object *ob,
                               DynamicPaintSurface *surface,
                               float timescale,
                               float subframe)
{
  PaintSurfaceData *sData = surface->data;
  PaintBakeData *bData = sData->bData;
  DynamicPaintCanvasSettings *canvas = surface->canvas;
  const bool for_render = (DEG_get_mode(depsgraph) == DAG_EVAL_RENDER);
  int ret = 1;

  if (sData->total_points < 1) {
    return 0;
  }

  if (dynamic_paint_surface_needs_dry_dissolve(surface)) {
    DynamicPaintDissolveDryData data = {
        .surface = surface,
        .timescale = timescale,
    };
    ParallelRangeSettings settings;
    BLI_parallel_range_settings_defaults(&settings);
    settings.use_threading = (sData->total_points > 1000);
    BLI_task_parallel_range(
        0, sData->total_points, &data, dynamic_paint_surface_pre_step_cb, &settings);
  }

  /*
   * Loop through surface's target paint objects and do painting
   */
  {
    unsigned int numobjects;
    Object **objects = BKE_collision_objects_create(
        depsgraph, NULL, surface->brush_group, &numobjects, eModifierType_DynamicPaint);

    /* backup current scene frame */
    int scene_frame = scene->r.cfra;
    float scene_subframe = scene->r.subframe;

    for (int i = 0; i < numobjects; i++) {
      Object *brushObj = objects[i];

      /* check if target has an active dp modifier */
      ModifierData *md = modifiers_findByType(brushObj, eModifierType_DynamicPaint);
      if (md && md->mode & (eModifierMode_Realtime | eModifierMode_Render)) {
        DynamicPaintModifierData *pmd2 = (DynamicPaintModifierData *)md;
        /* make sure we're dealing with a brush */
        if (pmd2->brush) {
          DynamicPaintBrushSettings *brush = pmd2->brush;

          /* calculate brush speed vectors if required */
          if (surface->type == MOD_DPAINT_SURFACE_T_PAINT && brush->flags & MOD_DPAINT_DO_SMUDGE) {
            bData->brush_velocity = MEM_callocN(sData->total_points * sizeof(float) * 4,
                                                "Dynamic Paint brush velocity");
            /* init adjacency data if not already */
            if (!sData->adj_data) {
              dynamicPaint_initAdjacencyData(surface, true);
            }
            if (!bData->bNeighs) {
              dynamicPaint_prepareAdjacencyData(surface, true);
            }
          }

          /* update object data on this subframe */
          if (subframe) {
            scene_setSubframe(scene, subframe);
            BKE_object_modifier_update_subframe(depsgraph,
                                                scene,
                                                brushObj,
                                                true,
                                                SUBFRAME_RECURSION,
                                                BKE_scene_frame_get(scene),
                                                eModifierType_DynamicPaint);
          }

          /* Apply brush on the surface depending on it's collision type */
          if (brush->psys && brush->psys->part &&
              ELEM(brush->psys->part->type, PART_EMITTER, PART_FLUID) &&
              psys_check_enabled(brushObj, brush->psys, for_render)) {
            /* Paint a particle system */
            dynamicPaint_paintParticles(surface, brush->psys, brush, timescale);
          }
          /* Object center distance: */
          if (brush->collision == MOD_DPAINT_COL_POINT && brushObj != ob) {
            dynamicPaint_paintSinglePoint(
                depsgraph, surface, brushObj->loc, brush, brushObj, scene, timescale);
          }
          /* Mesh volume/proximity: */
          else if (brushObj != ob) {
            dynamicPaint_paintMesh(depsgraph, surface, brush, brushObj, scene, timescale);
          }

          /* reset object to it's original state */
          if (subframe) {
            scene->r.cfra = scene_frame;
            scene->r.subframe = scene_subframe;
            BKE_object_modifier_update_subframe(depsgraph,
                                                scene,
                                                brushObj,
                                                true,
                                                SUBFRAME_RECURSION,
                                                BKE_scene_frame_get(scene),
                                                eModifierType_DynamicPaint);
          }

          /* process special brush effects, like smudge */
          if (bData->brush_velocity) {
            if (surface->type == MOD_DPAINT_SURFACE_T_PAINT &&
                brush->flags & MOD_DPAINT_DO_SMUDGE) {
              dynamicPaint_doSmudge(surface, brush, timescale);
            }
            MEM_freeN(bData->brush_velocity);
            bData->brush_velocity = NULL;
          }
        }
      }
    }

    BKE_collision_objects_free(objects);
  }

  /* surfaces operations that use adjacency data */
  if (sData->adj_data && bData->bNeighs) {
    /* wave type surface simulation step */
    if (surface->type == MOD_DPAINT_SURFACE_T_WAVE) {
      dynamicPaint_doWaveStep(surface, timescale);
    }

    /* paint surface effects */
    if (surface->effect && surface->type == MOD_DPAINT_SURFACE_T_PAINT) {
      int steps = 1, s;
      PaintPoint *prevPoint;
      float *force = NULL;

      /* Allocate memory for surface previous points to read unchanged values from */
      prevPoint = MEM_mallocN(sData->total_points * sizeof(struct PaintPoint),
                              "PaintSurfaceDataCopy");
      if (!prevPoint) {
        return setError(canvas, N_("Not enough free memory"));
      }

      /* Prepare effects and get number of required steps */
      steps = dynamicPaint_prepareEffectStep(depsgraph, surface, scene, ob, &force, timescale);
      for (s = 0; s < steps; s++) {
        dynamicPaint_doEffectStep(surface, force, prevPoint, timescale, (float)steps);
      }

      /* Free temporary effect data */
      if (prevPoint) {
        MEM_freeN(prevPoint);
      }
      if (force) {
        MEM_freeN(force);
      }
    }

    /* paint island border pixels */
    if (surface->type == MOD_DPAINT_SURFACE_T_PAINT) {
      dynamicPaint_doBorderStep(surface);
    }
  }

  return ret;
}

/*
 * Calculate a single frame and included subframes for surface
 */
int dynamicPaint_calculateFrame(DynamicPaintSurface *surface,
                                struct Depsgraph *depsgraph,
                                Scene *scene,
                                Object *cObject,
                                int frame)
{
  float timescale = 1.0f;

  /* apply previous displace on derivedmesh if incremental surface */
  if (surface->flags & MOD_DPAINT_DISP_INCREMENTAL) {
    dynamicPaint_applySurfaceDisplace(surface, dynamicPaint_canvas_mesh_get(surface->canvas));
  }

  /* update bake data */
  dynamicPaint_generateBakeData(surface, depsgraph, cObject);

  /* don't do substeps for first frame */
  if (surface->substeps && (frame != surface->start_frame)) {
    int st;
    timescale = 1.0f / (surface->substeps + 1);

    for (st = 1; st <= surface->substeps; st++) {
      float subframe = ((float)st) / (surface->substeps + 1);
      if (!dynamicPaint_doStep(depsgraph, scene, cObject, surface, timescale, subframe)) {
        return 0;
      }
    }
  }

  return dynamicPaint_doStep(depsgraph, scene, cObject, surface, timescale, 0.0f);
}

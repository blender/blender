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
 * The Original Code is Copyright (C) 2006 by Nicholas Bishop
 * All rights reserved.
 * Implements the Sculpt Mode tools
 */

/** \file
 * \ingroup edsculpt
 */

#include "MEM_guardedalloc.h"

#include "BLI_array.h"
#include "BLI_blenlib.h"
#include "BLI_dial_2d.h"
#include "BLI_ghash.h"
#include "BLI_gsqueue.h"
#include "BLI_hash.h"
#include "BLI_link_utils.h"
#include "BLI_linklist.h"
#include "BLI_linklist_stack.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_math_color_blend.h"
#include "BLI_rand.h"
#include "BLI_task.h"
#include "BLI_utildefines.h"
#include "atomic_ops.h"

#include "BLT_translation.h"

#include "PIL_time.h"

#include "DNA_brush_types.h"
#include "DNA_customdata_types.h"
#include "DNA_listBase.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.h"
#include "BKE_brush.h"
#include "BKE_brush_engine.h"
#include "BKE_ccg.h"
#include "BKE_colortools.h"
#include "BKE_context.h"
#include "BKE_image.h"
#include "BKE_kelvinlet.h"
#include "BKE_key.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_mesh_fair.h"
#include "BKE_mesh_mapping.h"
#include "BKE_mesh_mirror.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_node.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_particle.h"
#include "BKE_pbvh.h"
#include "BKE_pointcache.h"
#include "BKE_report.h"
#include "BKE_scene.h"
#include "BKE_screen.h"
#include "BKE_subdiv_ccg.h"
#include "BKE_subsurf.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "IMB_colormanagement.h"

#include "GPU_batch.h"
#include "GPU_batch_presets.h"
#include "GPU_immediate.h"
#include "GPU_immediate_util.h"
#include "GPU_matrix.h"
#include "GPU_state.h"

#include "WM_api.h"
#include "WM_message.h"
#include "WM_toolsystem.h"
#include "WM_types.h"

#include "ED_object.h"
#include "ED_screen.h"
#include "ED_sculpt.h"
#include "ED_space_api.h"
#include "ED_transform_snap_object_context.h"
#include "ED_view3d.h"
#include "paint_intern.h"
#include "sculpt_intern.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static bool sculpt_check_boundary_vertex_in_base_mesh(const SculptSession *ss,
                                                      const SculptVertRef index);
typedef void (*BrushActionFunc)(Sculpt *sd, Object *ob, Brush *brush, UnifiedPaintSettings *ups);

void sculpt_combine_proxies(Sculpt *sd, Object *ob);
static void SCULPT_run_command_list(
    Sculpt *sd, Object *ob, Brush *brush, BrushCommandList *list, UnifiedPaintSettings *ups);
static void do_symmetrical_brush_actions(Sculpt *sd,
                                         Object *ob,
                                         BrushActionFunc action,
                                         UnifiedPaintSettings *ups);

/* Sculpt API to get brush channel data
  If ss->cache exists then ss->cache->channels_final
  will be used, otherwise brush and tool settings channels
  will be used (taking inheritence into account).
*/

float SCULPT_get_float_intern(const SculptSession *ss,
                              const char *idname,
                              const Sculpt *sd,
                              const Brush *br)
{
  BrushMappingData *mapdata = ss->cache ? &ss->cache->input_mapping : NULL;

  if (ss->cache && ss->cache->channels_final) {
    return BKE_brush_channelset_get_float(ss->cache->channels_final, idname, mapdata);
  }
  else if (br && sd && br->channels && sd->channels) {
    return BKE_brush_channelset_get_final_float(br->channels, sd->channels, idname, mapdata);
  }
  else if (br && br->channels) {
    return BKE_brush_channelset_get_float(br->channels, idname, mapdata);
  }
  else if (sd && sd->channels) {
    return BKE_brush_channelset_get_float(sd->channels, idname, mapdata);
  }
  else {
    // should not happen!
    return 0.0f;
  }
}

int SCULPT_get_int_intern(const SculptSession *ss,
                          const char *idname,
                          const Sculpt *sd,
                          const Brush *br)
{
  BrushMappingData *mapdata = ss->cache ? &ss->cache->input_mapping : NULL;

  if (ss->cache && ss->cache->channels_final) {
    return BKE_brush_channelset_get_int(ss->cache->channels_final, idname, mapdata);
  }
  else if (br && br->channels && sd && sd->channels) {
    return BKE_brush_channelset_get_final_int(br->channels, sd->channels, idname, mapdata);
  }
  else if (br && br->channels) {
    return BKE_brush_channelset_get_int(br->channels, idname, mapdata);
  }
  else if (sd && sd->channels) {
    return BKE_brush_channelset_get_int(sd->channels, idname, mapdata);
  }
  else {
    // should not happen!
    return 0;
  }
}

int SCULPT_get_vector_intern(
    const SculptSession *ss, const char *idname, float out[4], const Sculpt *sd, const Brush *br)
{
  BrushMappingData *mapdata = ss->cache ? &ss->cache->input_mapping : NULL;

  if (ss->cache && ss->cache->channels_final) {

    return BKE_brush_channelset_get_vector(ss->cache->channels_final, idname, out, mapdata);
  }
  else if (br && br->channels && sd && sd->channels) {
    return BKE_brush_channelset_get_final_vector(br->channels, sd->channels, idname, out, mapdata);
  }
  else if (br && br->channels) {
    return BKE_brush_channelset_get_vector(br->channels, idname, out, mapdata);
  }
  else if (sd && sd->channels) {
    return BKE_brush_channelset_get_vector(sd->channels, idname, out, mapdata);
  }
  else {
    // should not happen!
    return 0;
  }
}

/* Sculpt PBVH abstraction API
 *
 * This is read-only, for writing use PBVH vertex iterators. There vd.index matches
 * the indices used here.
 *
 * For multi-resolution, the same vertex in multiple grids is counted multiple times, with
 * different index for each grid. */

void SCULPT_vertex_random_access_ensure(SculptSession *ss)
{
  if (ss->bm) {
    ss->totfaces = ss->totpoly = ss->bm->totface;
    ss->totvert = ss->bm->totvert;

    BM_mesh_elem_index_ensure(ss->bm, BM_VERT | BM_EDGE | BM_FACE);
    BM_mesh_elem_table_ensure(ss->bm, BM_VERT | BM_EDGE | BM_FACE);
  }
}

void SCULPT_face_normal_get(SculptSession *ss, SculptFaceRef face, float no[3])
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      BMFace *f = (BMFace *)face.i;

      copy_v3_v3(no, f->no);
      break;
    }

    case PBVH_FACES:
    case PBVH_GRIDS: {
      MPoly *mp = ss->mpoly + face.i;
      BKE_mesh_calc_poly_normal(mp, ss->mloop + mp->loopstart, ss->mvert, no);
      break;
    }
    default:  // failed
      zero_v3(no);
      break;
  }
}

/* Sculpt PBVH abstraction API
 *
 * This is read-only, for writing use PBVH vertex iterators. There vd.index matches
 * the indices used here.
 *
 * For multi-resolution, the same vertex in multiple grids is counted multiple times, with
 * different index for each grid. */

void SCULPT_face_random_access_ensure(SculptSession *ss)
{
  if (ss->bm) {
    ss->totfaces = ss->totpoly = ss->bm->totface;
    ss->totvert = ss->bm->totvert;

    BM_mesh_elem_index_ensure(ss->bm, BM_FACE);
    BM_mesh_elem_table_ensure(ss->bm, BM_FACE);
  }
}

int SCULPT_vertex_count_get(SculptSession *ss)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      return ss->totvert;
    case PBVH_BMESH:
      return BM_mesh_elem_count(BKE_pbvh_get_bmesh(ss->pbvh), BM_VERT);
    case PBVH_GRIDS:
      return BKE_pbvh_get_grid_num_vertices(ss->pbvh);
  }

  return 0;
}

MDynTopoVert *SCULPT_vertex_get_mdyntopo(SculptSession *ss, SculptVertRef vertex)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      BMVert *v = (BMVert *)vertex.i;
      return BKE_PBVH_DYNVERT(ss->cd_dyn_vert, v);
    }

    case PBVH_GRIDS:
    case PBVH_FACES: {
      return ss->mdyntopo_verts + vertex.i;
    }
  }

  return NULL;
}

float *SCULPT_vertex_origco_get(SculptSession *ss, SculptVertRef vertex)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      BMVert *v = (BMVert *)vertex.i;
      return BKE_PBVH_DYNVERT(ss->cd_dyn_vert, v)->origco;
    }

    case PBVH_GRIDS:
    case PBVH_FACES: {
      return ss->mdyntopo_verts[vertex.i].origco;
    }
  }

  return NULL;
}

const float *SCULPT_vertex_co_get(SculptSession *ss, SculptVertRef index)
{
  if (ss->bm) {
    return ((BMVert *)index.i)->co;
  }

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      if (ss->shapekey_active || ss->deform_modifiers_active) {
        const MVert *mverts = BKE_pbvh_get_verts(ss->pbvh);
        return mverts[index.i].co;
      }
      return ss->mvert[index.i].co;
    }
    case PBVH_BMESH: {
      BMVert *v = (BMVert *)index.i;
      return v->co;
    }
    case PBVH_GRIDS: {
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = index.i / key->grid_area;
      const int vertex_index = index.i - grid_index * key->grid_area;
      CCGElem *elem = BKE_pbvh_get_grids(ss->pbvh)[grid_index];
      return CCG_elem_co(key, CCG_elem_offset(key, elem, vertex_index));
    }
  }
  return NULL;
}

const float *SCULPT_vertex_color_get(SculptSession *ss, SculptVertRef index)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      if (ss->vcol) {
        return ss->vcol[index.i].color;
      }
      break;
    case PBVH_BMESH: {
      BMVert *v = (BMVert *)index.i;

      if (ss->cd_vcol_offset >= 0) {
        MPropCol *col = BM_ELEM_CD_GET_VOID_P(v, ss->cd_vcol_offset);
        return col->color;
      }

      break;
    }
    case PBVH_GRIDS:
      break;
  }
  return NULL;
}

void SCULPT_vertex_normal_get(SculptSession *ss, SculptVertRef index, float no[3])
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      if (ss->shapekey_active || ss->deform_modifiers_active) {
        const MVert *mverts = BKE_pbvh_get_verts(ss->pbvh);
        normal_short_to_float_v3(no, mverts[index.i].no);
      }
      else {
        normal_short_to_float_v3(no, ss->mvert[index.i].no);
      }
      break;
    }
    case PBVH_BMESH: {
      BMVert *v = (BMVert *)index.i;

      copy_v3_v3(no, v->no);
      break;
    }
    case PBVH_GRIDS: {
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = index.i / key->grid_area;
      const int vertex_index = index.i - grid_index * key->grid_area;
      CCGElem *elem = BKE_pbvh_get_grids(ss->pbvh)[grid_index];
      copy_v3_v3(no, CCG_elem_no(key, CCG_elem_offset(key, elem, vertex_index)));
      break;
    }
  }
}

bool SCULPT_has_persistent_base(SculptSession *ss)
{
  if (!ss->pbvh) {
    // just see if ss->custom_layer entries exist
    return ss->custom_layers[SCULPT_SCL_PERS_CO] != NULL;
  }

  int idx = -1;

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH:
      idx = CustomData_get_named_layer_index(&ss->bm->vdata, CD_PROP_FLOAT3, SCULPT_LAYER_PERS_CO);
      break;
    case PBVH_FACES:
      idx = CustomData_get_named_layer_index(ss->vdata, CD_PROP_FLOAT3, SCULPT_LAYER_PERS_CO);
      break;
    case PBVH_GRIDS:
      return ss->custom_layers[SCULPT_SCL_PERS_CO];
  }

  return idx >= 0;
}

const float *SCULPT_vertex_persistent_co_get(SculptSession *ss, SculptVertRef index)
{
  if (ss->custom_layers[SCULPT_SCL_PERS_CO]) {
    return (float *)SCULPT_temp_cdata_get(index, ss->custom_layers[SCULPT_SCL_PERS_CO]);
  }

  return SCULPT_vertex_co_get(ss, index);
}

const float *SCULPT_vertex_co_for_grab_active_get(SculptSession *ss, SculptVertRef vertex)
{
  /* Always grab active shape key if the sculpt happens on shapekey. */
  if (ss->shapekey_active) {
    const MVert *mverts = BKE_pbvh_get_verts(ss->pbvh);
    return mverts[BKE_pbvh_vertex_index_to_table(ss->pbvh, vertex)].co;
  }

  /* Sculpting on the base mesh. */
  if (ss->mvert) {
    return ss->mvert[BKE_pbvh_vertex_index_to_table(ss->pbvh, vertex)].co;
  }

  /* Everything else, such as sculpting on multires. */
  return SCULPT_vertex_co_get(ss, vertex);
}

void SCULPT_vertex_limit_surface_get(SculptSession *ss, SculptVertRef vertex, float r_co[3])
{
  if (BKE_pbvh_type(ss->pbvh) != PBVH_GRIDS) {
    if (ss->limit_surface) {
      float *f = SCULPT_temp_cdata_get(vertex, ss->limit_surface);
      copy_v3_v3(r_co, f);
    }
    else {
      copy_v3_v3(r_co, SCULPT_vertex_co_get(ss, vertex));
    }

    return;
  }

  const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
  const int grid_index = vertex.i / key->grid_area;
  const int vertex_index = vertex.i - grid_index * key->grid_area;

  SubdivCCGCoord coord = {.grid_index = grid_index,
                          .x = vertex_index % key->grid_size,
                          .y = vertex_index / key->grid_size};
  BKE_subdiv_ccg_eval_limit_point(ss->subdiv_ccg, &coord, r_co);
}

void SCULPT_vertex_persistent_normal_get(SculptSession *ss, SculptVertRef vertex, float no[3])
{
  if (ss->custom_layers[SCULPT_SCL_PERS_NO]) {
    float *no2 = SCULPT_temp_cdata_get(vertex, ss->custom_layers[SCULPT_SCL_PERS_NO]);
    copy_v3_v3(no, no2);
  }
  else {
    SCULPT_vertex_normal_get(ss, vertex, no);
  }
}

static bool sculpt_temp_customlayer_get(SculptSession *ss,
                                        AttributeDomain domain,
                                        int proptype,
                                        const char *name,
                                        SculptCustomLayer *out,
                                        bool autocreate,
                                        SculptLayerParams *params)
{
  bool simple_array = params->simple_array;
  bool permanent = params->permanent;

  out->params = *params;
  out->proptype = proptype;
  out->domain = domain;
  BLI_strncpy(out->name, name, sizeof(out->name));

  if (ss->pbvh && BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
    if (permanent) {
      printf(
          "%s: error: tried to make permanent customdata in multires mode; will make local array "
          "instead.\n",
          __func__);
      permanent = false;
    }

    // customdata attribute layers not supported for grids
    simple_array = true;
    out->params.simple_array = true;
  }

  BLI_assert(!(simple_array && permanent));

  if (simple_array) {
    CustomData *cdata = NULL;
    int totelem = 0;

    PBVHType pbvhtype = ss->pbvh ? BKE_pbvh_type(ss->pbvh) : (ss->bm ? PBVH_BMESH : PBVH_FACES);

    switch (pbvhtype) {
      case PBVH_BMESH: {
        switch (domain) {
          case ATTR_DOMAIN_POINT:
            cdata = &ss->bm->vdata;
            totelem = ss->bm->totvert;
            break;
          case ATTR_DOMAIN_FACE:
            cdata = &ss->bm->pdata;
            totelem = ss->bm->totface;
            break;
          default:
            return false;
        }
        break;
      }
      case PBVH_GRIDS: {
        switch (domain) {
          case ATTR_DOMAIN_POINT:
            cdata = ss->vdata;
            totelem = SCULPT_vertex_count_get(ss);
            break;
          case ATTR_DOMAIN_FACE:
            cdata = ss->pdata;
            totelem = ss->totfaces;
            break;
          default:
            return false;
        }
        break;
      }
      case PBVH_FACES: {
        switch (domain) {
          case ATTR_DOMAIN_POINT:
            cdata = ss->vdata;
            totelem = ss->totvert;
            break;
          case ATTR_DOMAIN_FACE:
            cdata = ss->pdata;
            totelem = ss->totfaces;
            break;
          default:
            return false;
        }
        break;
      }
    }

    CustomData dummy = {0};
    CustomData_reset(&dummy);
    CustomData_add_layer(&dummy, proptype, CD_ASSIGN, NULL, 0);
    int elemsize = (int)CustomData_get_elem_size(dummy.layers);

    CustomData_free(&dummy, 0);

    out->data = MEM_calloc_arrayN(totelem, elemsize, name);

    out->is_cdlayer = false;
    out->from_bmesh = ss->bm != NULL;
    out->cd_offset = -1;
    out->layer = NULL;
    out->domain = domain;
    out->proptype = proptype;
    out->elemsize = elemsize;

    /*grids cannot store normal customdata layers, and thus
      we cannot rely on the customdata api to keep track of
      and free their memory for us.

      so instead we queue them in a dynamic array inside of
      SculptSession.
      */
    if (ss->pbvh && BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
      ss->tot_layers_to_free++;

      if (!ss->layers_to_free) {
        ss->layers_to_free = MEM_calloc_arrayN(
            ss->tot_layers_to_free, sizeof(void *), "ss->layers_to_free");
      }
      else {
        ss->layers_to_free = MEM_recallocN(ss->layers_to_free,
                                           sizeof(void *) * ss->tot_layers_to_free);
      }

      SculptCustomLayer *cpy = MEM_callocN(sizeof(SculptCustomLayer), "SculptCustomLayer cpy");
      *cpy = *out;

      ss->layers_to_free[ss->tot_layers_to_free - 1] = cpy;
    }

    return true;
  }

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      CustomData *cdata = NULL;
      out->from_bmesh = true;

      if (!ss->bm) {
        return false;
      }

      switch (domain) {
        case ATTR_DOMAIN_POINT:
          cdata = &ss->bm->vdata;
          break;
        case ATTR_DOMAIN_FACE:
          cdata = &ss->bm->pdata;
          break;
        default:
          return false;
      }

      int idx = CustomData_get_named_layer_index(cdata, proptype, name);

      if (idx < 0) {
        if (!autocreate) {
          return false;
        }

        BM_data_layer_add_named(ss->bm, cdata, proptype, name);
        idx = CustomData_get_named_layer_index(cdata, proptype, name);

        SCULPT_dyntopo_node_layers_update_offsets(ss);
      }

      if (!permanent) {
        cdata->layers[idx].flag |= CD_FLAG_TEMPORARY | CD_FLAG_NOCOPY;
      }

      out->data = NULL;
      out->is_cdlayer = true;
      out->layer = cdata->layers + idx;
      out->cd_offset = out->layer->offset;
      out->elemsize = CustomData_get_elem_size(out->layer);

      break;
    }
    case PBVH_FACES: {
      CustomData *cdata = NULL;
      int totelem = 0;

      out->from_bmesh = false;

      switch (domain) {
        case ATTR_DOMAIN_POINT:
          totelem = ss->totvert;
          cdata = ss->vdata;
          break;
        case ATTR_DOMAIN_FACE:
          totelem = ss->totfaces;
          cdata = ss->pdata;
          break;
        default:
          return false;
      }

      int idx = CustomData_get_named_layer_index(cdata, proptype, name);

      if (idx < 0) {
        if (!autocreate) {
          return false;
        }

        CustomData_add_layer_named(cdata, proptype, CD_CALLOC, NULL, totelem, name);
        idx = CustomData_get_named_layer_index(cdata, proptype, name);
      }

      if (!permanent) {
        cdata->layers[idx].flag |= CD_FLAG_TEMPORARY | CD_FLAG_NOCOPY;
      }

      out->data = NULL;
      out->is_cdlayer = true;
      out->layer = cdata->layers + idx;
      out->cd_offset = -1;
      out->data = out->layer->data;
      out->elemsize = CustomData_get_elem_size(out->layer);
      break;
    }
    case PBVH_GRIDS: {
      CustomData *cdata = NULL;
      int totelem = 0;

      out->from_bmesh = false;

      switch (domain) {
        case ATTR_DOMAIN_POINT:
          totelem = BKE_pbvh_get_grid_num_vertices(ss->pbvh);
          cdata = &ss->temp_vdata;
          break;
        case ATTR_DOMAIN_FACE:
          // not supported
          return false;
        default:
          return false;
      }

      int idx = CustomData_get_named_layer_index(cdata, proptype, name);

      if (idx < 0) {
        if (!autocreate) {
          return false;
        }

        CustomData_add_layer_named(cdata, proptype, CD_CALLOC, NULL, totelem, name);
        idx = CustomData_get_named_layer_index(cdata, proptype, name);

        if (!permanent) {
          cdata->layers[idx].flag |= CD_FLAG_TEMPORARY | CD_FLAG_NOCOPY;
        }
      }

      out->data = NULL;
      out->is_cdlayer = true;
      out->layer = cdata->layers + idx;
      out->cd_offset = -1;
      out->data = out->layer->data;
      out->elemsize = CustomData_get_elem_size(out->layer);

      break;
    }
  }

  return true;
}

void SCULPT_update_customdata_refs(SculptSession *ss)
{
  /* run twice, in case sculpt_temp_customlayer_get had to recreate a layer and
     messed up the ordering. */
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < SCULPT_SCL_LAYER_MAX; j++) {
      SculptCustomLayer *scl = ss->custom_layers[j];

      if (scl && !scl->released && !scl->params.simple_array) {
        sculpt_temp_customlayer_get(
            ss, scl->domain, scl->proptype, scl->name, scl, true, &scl->params);
      }
    }
  }

  if (ss->bm) {
    SCULPT_dyntopo_node_layers_update_offsets(ss);
  }
}

float SCULPT_vertex_mask_get(SculptSession *ss, SculptVertRef index)
{
  BMVert *v;
  float *mask;
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      return ss->vmask[index.i];
    case PBVH_BMESH:
      v = (BMVert *)index.i;
      mask = BM_ELEM_CD_GET_VOID_P(v, ss->cd_vert_mask_offset);
      return *mask;
    case PBVH_GRIDS: {
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = index.i / key->grid_area;
      const int vertex_index = index.i - grid_index * key->grid_area;
      CCGElem *elem = BKE_pbvh_get_grids(ss->pbvh)[grid_index];
      return *CCG_elem_mask(key, CCG_elem_offset(key, elem, vertex_index));
    }
  }

  return 0.0f;
}

bool SCULPT_temp_customlayer_ensure(SculptSession *ss,
                                    AttributeDomain domain,
                                    int proptype,
                                    const char *name,
                                    SculptLayerParams *params)
{
  SculptCustomLayer scl;
  bool ret = sculpt_temp_customlayer_get(ss, domain, proptype, name, &scl, true, params);

  SCULPT_update_customdata_refs(ss);
  return ret;
}

bool SCULPT_temp_customlayer_release(SculptSession *ss, SculptCustomLayer *scl)
{
  int proptype = scl->proptype;
  AttributeDomain domain = scl->domain;

  if (scl->released) {
    return false;
  }

  // remove from layers_to_free list if necassary
  for (int i = 0; scl->data && i < ss->tot_layers_to_free; i++) {
    if (ss->layers_to_free[i] && ss->layers_to_free[i]->data == scl->data) {
      MEM_freeN(ss->layers_to_free[i]);
      ss->layers_to_free[i] = NULL;
    }
  }

  scl->released = true;

  if (!scl->from_bmesh) {
    // for now, don't clean up bmesh temp layers
    if (scl->is_cdlayer && BKE_pbvh_type(ss->pbvh) != PBVH_GRIDS) {
      CustomData *cdata = NULL;
      int totelem = 0;

      switch (domain) {
        case ATTR_DOMAIN_POINT:
          cdata = ss->vdata;
          totelem = ss->totvert;
          break;
        case ATTR_DOMAIN_FACE:
          cdata = ss->pdata;
          totelem = ss->totfaces;
          break;
        default:
          printf("error, unknown domain in %s\n", __func__);
          return false;
      }

      CustomData_free_layer(cdata, scl->layer->type, totelem, scl->layer - cdata->layers);
      SCULPT_update_customdata_refs(ss);
    }
    else {
      MEM_SAFE_FREE(scl->data);
    }

    scl->data = NULL;
  }
  return true;
}

bool SCULPT_temp_customlayer_get(SculptSession *ss,
                                 AttributeDomain domain,
                                 int proptype,
                                 const char *name,
                                 SculptCustomLayer *scl,
                                 SculptLayerParams *params)
{
  bool ret = sculpt_temp_customlayer_get(ss, domain, proptype, name, scl, true, params);
  SCULPT_update_customdata_refs(ss);

  return ret;
}

SculptVertRef SCULPT_active_vertex_get(SculptSession *ss)
{
  if (ELEM(BKE_pbvh_type(ss->pbvh), PBVH_FACES, PBVH_BMESH, PBVH_GRIDS)) {
    return ss->active_vertex_index;
  }
  return BKE_pbvh_make_vref(0);
}

const float *SCULPT_active_vertex_co_get(SculptSession *ss)
{
  return SCULPT_vertex_co_get(ss, SCULPT_active_vertex_get(ss));
}

void SCULPT_active_vertex_normal_get(SculptSession *ss, float normal[3])
{
  SCULPT_vertex_normal_get(ss, SCULPT_active_vertex_get(ss), normal);
}

MVert *SCULPT_mesh_deformed_mverts_get(SculptSession *ss)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      if (ss->shapekey_active || ss->deform_modifiers_active) {
        return BKE_pbvh_get_verts(ss->pbvh);
      }
      return ss->mvert;
    case PBVH_BMESH:
    case PBVH_GRIDS:
      return NULL;
  }
  return NULL;
}

float *SCULPT_brush_deform_target_vertex_co_get(SculptSession *ss,
                                                const int deform_target,
                                                PBVHVertexIter *iter)
{
  switch (deform_target) {
    case BRUSH_DEFORM_TARGET_GEOMETRY:
      return iter->co;
    case BRUSH_DEFORM_TARGET_CLOTH_SIM:
      return ss->cache->cloth_sim->deformation_pos[iter->index];
  }
  return iter->co;
}

char SCULPT_mesh_symmetry_xyz_get(Object *object)
{
  const Mesh *mesh = BKE_mesh_from_object(object);
  return mesh->symmetry;
}

/* Sculpt Face Sets and Visibility. */

int SCULPT_active_face_set_get(SculptSession *ss)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      return ss->face_sets[ss->active_face_index.i];
    case PBVH_GRIDS: {
      const int face_index = BKE_subdiv_ccg_grid_to_face_index(ss->subdiv_ccg,
                                                               ss->active_grid_index);
      return ss->face_sets[face_index];
    }
    case PBVH_BMESH:
      if (ss->cd_faceset_offset && ss->active_face_index.i) {
        BMFace *f = (BMFace *)ss->active_face_index.i;
        return BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset);
      }

      return SCULPT_FACE_SET_NONE;
  }
  return SCULPT_FACE_SET_NONE;
}

void SCULPT_vertex_visible_set(SculptSession *ss, SculptVertRef index, bool visible)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      SET_FLAG_FROM_TEST(ss->mvert[index.i].flag, !visible, ME_HIDE);
      ss->mvert[index.i].flag |= ME_VERT_PBVH_UPDATE;
      break;
    case PBVH_BMESH:
      BM_elem_flag_set((BMVert *)index.i, BM_ELEM_HIDDEN, !visible);
      break;
    case PBVH_GRIDS:
      break;
  }
}

bool SCULPT_vertex_visible_get(SculptSession *ss, SculptVertRef index)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      return !(ss->mvert[index.i].flag & ME_HIDE);
    case PBVH_BMESH:
      return !BM_elem_flag_test(((BMVert *)index.i), BM_ELEM_HIDDEN);
    case PBVH_GRIDS: {
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = index.i / key->grid_area;
      const int vertex_index = index.i - grid_index * key->grid_area;
      BLI_bitmap **grid_hidden = BKE_pbvh_get_grid_visibility(ss->pbvh);
      if (grid_hidden && grid_hidden[grid_index]) {
        return !BLI_BITMAP_TEST(grid_hidden[grid_index], vertex_index);
      }
    }
  }
  return true;
}

void SCULPT_face_set_visibility_set(SculptSession *ss, int face_set, bool visible)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
    case PBVH_GRIDS:
      for (int i = 0; i < ss->totfaces; i++) {
        if (abs(ss->face_sets[i]) != face_set) {
          continue;
        }
        if (visible) {
          ss->face_sets[i] = abs(ss->face_sets[i]);
        }
        else {
          ss->face_sets[i] = -abs(ss->face_sets[i]);
        }
      }
      break;
    case PBVH_BMESH: {
      BMIter iter;
      BMFace *f;

      BM_ITER_MESH (f, &iter, ss->bm, BM_FACES_OF_MESH) {
        int fset = BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset);

        if (abs(fset) != face_set) {
          continue;
        }

        if (visible) {
          fset = abs(fset);
        }
        else {
          fset = -abs(fset);
        }

        BM_ELEM_CD_SET_INT(f, ss->cd_faceset_offset, fset);
      }
      break;
    }
  }
}

void SCULPT_face_sets_visibility_invert(SculptSession *ss)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
    case PBVH_GRIDS:
      for (int i = 0; i < ss->totfaces; i++) {
        ss->face_sets[i] *= -1;
      }
      break;
    case PBVH_BMESH: {
      BMIter iter;
      BMFace *f;

      BM_ITER_MESH (f, &iter, ss->bm, BM_FACES_OF_MESH) {
        int fset = BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset);

        fset = -fset;

        BM_ELEM_CD_SET_INT(f, ss->cd_faceset_offset, fset);
      }
      break;
    }
  }
}

void SCULPT_face_sets_visibility_all_set(SculptSession *ss, bool visible)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
    case PBVH_GRIDS:
      for (int i = 0; i < ss->totfaces; i++) {

        /* This can run on geometry without a face set assigned, so its ID sign can't be changed to
         * modify the visibility. Force that geometry to the ID 1 to enable changing the visibility
         * here. */
        if (ss->face_sets[i] == SCULPT_FACE_SET_NONE) {
          ss->face_sets[i] = 1;
        }

        if (visible) {
          ss->face_sets[i] = abs(ss->face_sets[i]);
        }
        else {
          ss->face_sets[i] = -abs(ss->face_sets[i]);
        }
      }
      break;
    case PBVH_BMESH: {
      BMIter iter;
      BMFace *f;

      if (!ss->bm) {
        return;
      }

      // paranoia check of cd_faceset_offset
      if (ss->cd_faceset_offset < 0) {
        ss->cd_faceset_offset = CustomData_get_offset(&ss->bm->pdata, CD_SCULPT_FACE_SETS);
      }
      if (ss->cd_faceset_offset < 0) {
        return;
      }

      BM_ITER_MESH (f, &iter, ss->bm, BM_FACES_OF_MESH) {
        int fset = BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset);

        /* This can run on geometry without a face set assigned, so its ID sign can't be changed to
         * modify the visibility. Force that geometry to the ID 1 to enable changing the visibility
         * here. */

        if (fset == SCULPT_FACE_SET_NONE) {
          fset = 1;
        }

        if (visible) {
          fset = abs(fset);
        }
        else {
          fset = -abs(fset);
        }

        BM_ELEM_CD_SET_INT(f, ss->cd_faceset_offset, fset);
      }
      break;
    }
  }
}

bool SCULPT_vertex_any_face_set_visible_get(SculptSession *ss, SculptVertRef index)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      MeshElemMap *vert_map = &ss->pmap[index.i];
      for (int j = 0; j < ss->pmap[index.i].count; j++) {
        if (ss->face_sets[vert_map->indices[j]] > 0) {
          return true;
        }
      }
      return false;
    }
    case PBVH_BMESH: {
      BMIter iter;
      BMLoop *l;
      BMVert *v = (BMVert *)index.i;

      BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
        int fset = BM_ELEM_CD_GET_INT(l->f, ss->cd_faceset_offset);
        if (fset >= 0) {
          return true;
        }
      }

      return false;
    }
    case PBVH_GRIDS:
      return true;
  }
  return true;
}

bool SCULPT_vertex_all_face_sets_visible_get(const SculptSession *ss, SculptVertRef index)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      MeshElemMap *vert_map = &ss->pmap[index.i];
      for (int j = 0; j < ss->pmap[index.i].count; j++) {
        if (ss->face_sets[vert_map->indices[j]] < 0) {
          return false;
        }
      }
      return true;
    }
    case PBVH_BMESH: {
      BMIter iter;
      BMLoop *l;
      BMVert *v = (BMVert *)index.i;

      BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
        int fset = BM_ELEM_CD_GET_INT(l->f, ss->cd_faceset_offset);
        if (fset < 0) {
          return false;
        }
      }

      return true;
    }
    case PBVH_GRIDS: {
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = index.i / key->grid_area;
      const int face_index = BKE_subdiv_ccg_grid_to_face_index(ss->subdiv_ccg, grid_index);
      return ss->face_sets[face_index] > 0;
    }
  }
  return true;
}

void SCULPT_vertex_face_set_set(SculptSession *ss, SculptVertRef index, int face_set)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      MeshElemMap *vert_map = &ss->pmap[index.i];
      for (int j = 0; j < ss->pmap[index.i].count; j++) {
        if (ss->face_sets[vert_map->indices[j]] > 0) {
          ss->face_sets[vert_map->indices[j]] = abs(face_set);
        }
      }
    } break;
    case PBVH_BMESH: {
      BMIter iter;
      BMLoop *l;
      BMVert *v = (BMVert *)index.i;

      BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
        int fset = BM_ELEM_CD_GET_INT(l->f, ss->cd_faceset_offset);
        if (fset >= 0 && fset != abs(face_set)) {
          MDynTopoVert *mv = BKE_PBVH_DYNVERT(ss->cd_dyn_vert, v);

          mv->flag |= DYNVERT_NEED_BOUNDARY;
          BM_ELEM_CD_SET_INT(l->f, ss->cd_faceset_offset, abs(face_set));
        }
      }

      break;
    }
    case PBVH_GRIDS: {
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = index.i / key->grid_area;
      const int face_index = BKE_subdiv_ccg_grid_to_face_index(ss->subdiv_ccg, grid_index);
      if (ss->face_sets[face_index] > 0) {
        ss->face_sets[face_index] = abs(face_set);
      }

    } break;
  }
}

void SCULPT_vertex_face_set_increase(SculptSession *ss, SculptVertRef vertex, const int increase)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      int index = (int)vertex.i;
      MeshElemMap *vert_map = &ss->pmap[index];
      for (int j = 0; j < ss->pmap[index].count; j++) {
        if (ss->face_sets[vert_map->indices[j]] > 0) {
          ss->face_sets[vert_map->indices[j]] += increase;
        }
      }
    } break;
    case PBVH_BMESH: {
      BMVert *v = (BMVert *)vertex.i;
      BMIter iter;
      BMFace *f;

      BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
        int fset = BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset);

        if (fset <= 0) {
          continue;
        }

        fset += increase;

        BM_ELEM_CD_SET_INT(f, ss->cd_faceset_offset, fset);
      }
      break;
    }
    case PBVH_GRIDS: {
      int index = (int)vertex.i;
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = index / key->grid_area;
      const int face_index = BKE_subdiv_ccg_grid_to_face_index(ss->subdiv_ccg, grid_index);
      if (ss->face_sets[face_index] > 0) {
        ss->face_sets[face_index] += increase;
      }

    } break;
  }
}

int SCULPT_vertex_face_set_get(SculptSession *ss, SculptVertRef index)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      MeshElemMap *vert_map = &ss->pmap[index.i];
      int face_set = 0;
      for (int i = 0; i < ss->pmap[index.i].count; i++) {
        if (ss->face_sets[vert_map->indices[i]] > face_set) {
          face_set = abs(ss->face_sets[vert_map->indices[i]]);
        }
      }
      return face_set;
    }
    case PBVH_BMESH: {
      BMIter iter;
      BMLoop *l;
      BMVert *v = (BMVert *)index.i;
      int ret = -1;

      BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
        int fset = BM_ELEM_CD_GET_INT(l->f, ss->cd_faceset_offset);
        fset = abs(fset);

        if (fset > ret) {
          ret = fset;
        }
      }

      return ret;
    }
    case PBVH_GRIDS: {
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = index.i / key->grid_area;
      const int face_index = BKE_subdiv_ccg_grid_to_face_index(ss->subdiv_ccg, grid_index);
      return ss->face_sets[face_index];
    }
  }
  return 0;
}

bool SCULPT_vertex_has_face_set(SculptSession *ss, SculptVertRef index, int face_set)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      MeshElemMap *vert_map = &ss->pmap[index.i];
      for (int i = 0; i < ss->pmap[index.i].count; i++) {
        if (ss->face_sets[vert_map->indices[i]] == face_set) {
          return true;
        }
      }
      return false;
    }
    case PBVH_BMESH: {
      BMEdge *e;
      BMVert *v = (BMVert *)index.i;

      if (ss->cd_faceset_offset == -1) {
        return false;
      }

      e = v->e;

      if (UNLIKELY(!e)) {
        return false;
      }

      do {
        BMLoop *l = e->l;

        if (UNLIKELY(!l)) {
          continue;
        }

        do {
          BMFace *f = l->f;

          if (abs(BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset)) == abs(face_set)) {
            return true;
          }
        } while ((l = l->radial_next) != e->l);
      } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

      return false;
    }
    case PBVH_GRIDS: {
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = index.i / key->grid_area;
      const int face_index = BKE_subdiv_ccg_grid_to_face_index(ss->subdiv_ccg, grid_index);
      return ss->face_sets[face_index] == face_set;
    }
  }
  return true;
}

/*
calcs visibility state based on face sets.
todo: also calc a face set boundary flag.
*/
void sculpt_vertex_faceset_update_bmesh(SculptSession *ss, SculptVertRef vert)
{
  if (!ss->bm) {
    return;
  }

  BMVert *v = (BMVert *)vert.i;
  BMEdge *e = v->e;
  bool ok = false;
  const int cd_faceset_offset = ss->cd_faceset_offset;

  if (!e) {
    return;
  }

  do {
    BMLoop *l = e->l;
    if (l) {
      do {
        if (BM_ELEM_CD_GET_INT(l->f, cd_faceset_offset) > 0) {
          ok = true;
          break;
        }

        l = l->radial_next;
      } while (l != e->l);

      if (ok) {
        break;
      }
    }
    e = v == e->v1 ? e->v1_disk_link.next : e->v2_disk_link.next;
  } while (e != v->e);

  MDynTopoVert *mv = BM_ELEM_CD_GET_VOID_P(v, ss->cd_dyn_vert);

  if (ok) {
    mv->flag &= ~DYNVERT_VERT_FSET_HIDDEN;
  }
  else {
    mv->flag |= DYNVERT_VERT_FSET_HIDDEN;
  }
}

void SCULPT_visibility_sync_all_face_sets_to_vertices(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = BKE_object_get_original_mesh(ob);
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      BKE_sculpt_sync_face_sets_visibility_to_base_mesh(mesh);
      break;
    }
    case PBVH_GRIDS: {
      BKE_sculpt_sync_face_sets_visibility_to_base_mesh(mesh);
      BKE_sculpt_sync_face_sets_visibility_to_grids(mesh, ss->subdiv_ccg);
      break;
    }
    case PBVH_BMESH: {
      BMIter iter;
      BMFace *f;
      BMVert *v;

      BM_ITER_MESH (f, &iter, ss->bm, BM_FACES_OF_MESH) {
        int fset = BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset);

        if (fset < 0) {
          BM_elem_flag_enable(f, BM_ELEM_HIDDEN);
        }
        else {
          BM_elem_flag_disable(f, BM_ELEM_HIDDEN);
        }
      }

      BM_ITER_MESH (v, &iter, ss->bm, BM_VERTS_OF_MESH) {
        MDynTopoVert *mv = BM_ELEM_CD_GET_VOID_P(v, ss->cd_dyn_vert);

        BMIter iter2;
        BMLoop *l;

        int visible = false;

        BM_ITER_ELEM (l, &iter2, v, BM_LOOPS_OF_VERT) {
          if (!BM_elem_flag_test(l->f, BM_ELEM_HIDDEN)) {
            visible = true;
            break;
          }
        }

        if (!visible) {
          mv->flag |= DYNVERT_VERT_FSET_HIDDEN;
          BM_elem_flag_enable(v, BM_ELEM_HIDDEN);
        }
        else {
          mv->flag &= ~DYNVERT_VERT_FSET_HIDDEN;
          BM_elem_flag_disable(v, BM_ELEM_HIDDEN);
        }
      }
      break;
    }
  }
}

static void UNUSED_FUNCTION(sculpt_visibility_sync_vertex_to_face_sets)(SculptSession *ss,
                                                                        SculptVertRef vertex)
{
  int index = (int)vertex.i;
  MeshElemMap *vert_map = &ss->pmap[index];
  const bool visible = SCULPT_vertex_visible_get(ss, vertex);

  for (int i = 0; i < ss->pmap[index].count; i++) {
    if (visible) {
      ss->face_sets[vert_map->indices[i]] = abs(ss->face_sets[vert_map->indices[i]]);
    }
    else {
      ss->face_sets[vert_map->indices[i]] = -abs(ss->face_sets[vert_map->indices[i]]);
    }
  }
  ss->mvert[index].flag |= ME_VERT_PBVH_UPDATE;
}

void SCULPT_visibility_sync_all_vertex_to_face_sets(SculptSession *ss)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      for (int i = 0; i < ss->totfaces; i++) {
        MPoly *poly = &ss->mpoly[i];
        bool poly_visible = true;
        for (int l = 0; l < poly->totloop; l++) {
          MLoop *loop = &ss->mloop[poly->loopstart + l];
          if (!SCULPT_vertex_visible_get(ss, BKE_pbvh_make_vref(loop->v))) {
            poly_visible = false;
          }
        }
        if (poly_visible) {
          ss->face_sets[i] = abs(ss->face_sets[i]);
        }
        else {
          ss->face_sets[i] = -abs(ss->face_sets[i]);
        }
      }
      break;
    }
    case PBVH_GRIDS:
      break;
    case PBVH_BMESH: {
      BMIter iter;
      BMFace *f;

      if (!ss->bm) {
        return;
      }

      BM_ITER_MESH (f, &iter, ss->bm, BM_FACES_OF_MESH) {
        BMLoop *l = f->l_first;
        bool visible = true;

        do {
          if (BM_elem_flag_test(l->v, BM_ELEM_HIDDEN)) {
            visible = false;
            break;
          }
          l = l->next;
        } while (l != f->l_first);

        int fset = BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset);
        if (visible) {
          fset = abs(fset);
        }
        else {
          fset = -abs(fset);
        }

        BM_ELEM_CD_SET_INT(f, ss->cd_faceset_offset, fset);
      }

      break;
    }
  }
}

static SculptCornerType sculpt_check_corner_in_base_mesh(const SculptSession *ss,
                                                         SculptVertRef vertex,
                                                         bool check_facesets)
{
  int index = BKE_pbvh_vertex_index_to_table(ss->pbvh, vertex);

  MeshElemMap *vert_map = &ss->pmap[index];
  int face_set = -1;
  int last1 = -1;
  int last2 = -1;

  int ret = 0;

  if (sculpt_check_boundary_vertex_in_base_mesh(ss, vertex) && ss->pmap[index].count < 4) {
    ret |= SCULPT_CORNER_MESH;
  }

  if (check_facesets) {
    for (int i = 0; i < ss->pmap[index].count; i++) {
      if (check_facesets) {
        if (last2 != last1) {
          last2 = last1;
        }
        if (last1 != face_set) {
          last1 = face_set;
        }
        face_set = abs(ss->face_sets[vert_map->indices[i]]);

        bool corner = last1 != -1 && last2 != -1 && face_set != -1;
        corner = corner && last1 != last2 && last1 != face_set;

        if (corner) {
          ret |= SCULPT_CORNER_FACE_SET;
        }
      }
    }
  }

  return ret;
}

static bool sculpt_check_unique_face_set_in_base_mesh(const SculptSession *ss,
                                                      SculptVertRef vertex)
{
  int index = BKE_pbvh_vertex_index_to_table(ss->pbvh, vertex);

  MeshElemMap *vert_map = &ss->pmap[index];
  int face_set = -1;
  for (int i = 0; i < ss->pmap[index].count; i++) {
    if (face_set == -1) {
      face_set = abs(ss->face_sets[vert_map->indices[i]]);
    }
    else {
      if (abs(ss->face_sets[vert_map->indices[i]]) != face_set) {
        return false;
      }
    }
  }
  return true;
}

/**
 * Checks if the face sets of the adjacent faces to the edge between \a v1 and \a v2
 * in the base mesh are equal.
 */
static bool sculpt_check_unique_face_set_for_edge_in_base_mesh(const SculptSession *ss,
                                                               int v1,
                                                               int v2)
{
  MeshElemMap *vert_map = &ss->pmap[v1];
  int p1 = -1, p2 = -1;
  for (int i = 0; i < ss->pmap[v1].count; i++) {
    MPoly *p = &ss->mpoly[vert_map->indices[i]];
    for (int l = 0; l < p->totloop; l++) {
      MLoop *loop = &ss->mloop[p->loopstart + l];
      if (loop->v == v2) {
        if (p1 == -1) {
          p1 = vert_map->indices[i];
          break;
        }

        if (p2 == -1) {
          p2 = vert_map->indices[i];
          break;
        }
      }
    }
  }

  if (p1 != -1 && p2 != -1) {
    return abs(ss->face_sets[p1]) == (ss->face_sets[p2]);
  }
  return true;
}

bool SCULPT_vertex_has_unique_face_set(const SculptSession *ss, SculptVertRef index)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      return sculpt_check_unique_face_set_in_base_mesh(ss, index);
    }
    case PBVH_BMESH: {
      BMVert *v = (BMVert *)index.i;
      MDynTopoVert *mv = BKE_PBVH_DYNVERT(ss->cd_dyn_vert, v);

      if (mv->flag & DYNVERT_NEED_BOUNDARY) {
        BKE_pbvh_update_vert_boundary(ss->cd_dyn_vert,
                                      ss->cd_faceset_offset,
                                      ss->cd_vert_node_offset,
                                      ss->cd_face_node_offset,
                                      v,
                                      ss->boundary_symmetry);
      }

      return !(mv->flag & DYNVERT_FSET_BOUNDARY);

#if 0
      int face_set = 0;
      bool first = true;
      if (ss->cd_faceset_offset == -1) {
        return false;
      }

      BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
        BMFace *f = l->f;
        int face_set2 = BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset);

        if (!first && abs(face_set2) != abs(face_set)) {
          return false;
        }

        first = false;
        face_set = face_set2;
      }
      return true;
#endif
    }
    case PBVH_GRIDS: {
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = index.i / key->grid_area;
      const int vertex_index = index.i - grid_index * key->grid_area;
      const SubdivCCGCoord coord = {.grid_index = grid_index,
                                    .x = vertex_index % key->grid_size,
                                    .y = vertex_index / key->grid_size};
      int v1, v2;
      const SubdivCCGAdjacencyType adjacency = BKE_subdiv_ccg_coarse_mesh_adjacency_info_get(
          ss->subdiv_ccg, &coord, ss->mloop, ss->mpoly, &v1, &v2);
      switch (adjacency) {
        case SUBDIV_CCG_ADJACENT_VERTEX:
          return sculpt_check_unique_face_set_in_base_mesh(ss, BKE_pbvh_make_vref(v1));
        case SUBDIV_CCG_ADJACENT_EDGE:
          return sculpt_check_unique_face_set_for_edge_in_base_mesh(ss, v1, v2);
        case SUBDIV_CCG_ADJACENT_NONE:
          return true;
      }
    }
  }
  return false;
}

int SCULPT_face_set_next_available_get(SculptSession *ss)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
    case PBVH_GRIDS: {
      int next_face_set = 0;
      for (int i = 0; i < ss->totfaces; i++) {
        if (abs(ss->face_sets[i]) > next_face_set) {
          next_face_set = abs(ss->face_sets[i]);
        }
      }
      next_face_set++;
      return next_face_set;
    }
    case PBVH_BMESH: {
      int next_face_set = 0;
      BMIter iter;
      BMFace *f;
      if (!ss->cd_faceset_offset) {
        return 0;
      }

      BM_ITER_MESH (f, &iter, ss->bm, BM_FACES_OF_MESH) {
        int fset = abs(BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset));
        if (fset > next_face_set) {
          next_face_set = fset;
        }
      }

      next_face_set++;
      return next_face_set;
    }
  }
  return 0;
}

/* Sculpt Neighbor Iterators */

#define SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY 256

static void sculpt_vertex_neighbor_add(SculptVertexNeighborIter *iter,
                                       SculptVertRef neighbor,
                                       SculptEdgeRef edge,
                                       int neighbor_index)
{
  for (int i = 0; i < iter->size; i++) {
    if (iter->neighbors[i].vertex.i == neighbor.i) {
      return;
    }
  }

  if (iter->size >= iter->capacity) {
    iter->capacity += SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY;

    if (iter->neighbors == iter->neighbors_fixed) {
      iter->neighbors = MEM_mallocN(iter->capacity * sizeof(struct _SculptNeighborRef),
                                    "neighbor array");
      iter->neighbor_indices = MEM_mallocN(iter->capacity * sizeof(int), "neighbor array");

      memcpy(
          iter->neighbors, iter->neighbors_fixed, sizeof(struct _SculptNeighborRef) * iter->size);
      memcpy(iter->neighbor_indices, iter->neighbor_indices_fixed, sizeof(int) * iter->size);
    }
    else {
      iter->neighbors = MEM_reallocN_id(
          iter->neighbors, iter->capacity * sizeof(struct _SculptNeighborRef), "neighbor array");
      iter->neighbor_indices = MEM_reallocN_id(
          iter->neighbor_indices, iter->capacity * sizeof(int), "neighbor array");
    }
  }

  iter->neighbors[iter->size].vertex = neighbor;
  iter->neighbors[iter->size].edge = edge;
  iter->neighbor_indices[iter->size] = neighbor_index;
  iter->size++;
}

static void sculpt_vertex_neighbor_add_nocheck(SculptVertexNeighborIter *iter,
                                               SculptVertRef neighbor,
                                               SculptEdgeRef edge,
                                               int neighbor_index)
{
  if (iter->size >= iter->capacity) {
    iter->capacity += SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY;

    if (iter->neighbors == iter->neighbors_fixed) {
      iter->neighbors = MEM_mallocN(iter->capacity * sizeof(struct _SculptNeighborRef),
                                    "neighbor array");
      iter->neighbor_indices = MEM_mallocN(iter->capacity * sizeof(int), "neighbor array");

      memcpy(
          iter->neighbors, iter->neighbors_fixed, sizeof(struct _SculptNeighborRef) * iter->size);
      memcpy(iter->neighbor_indices, iter->neighbor_indices_fixed, sizeof(int) * iter->size);
    }
    else {
      iter->neighbors = MEM_reallocN_id(
          iter->neighbors, iter->capacity * sizeof(struct _SculptNeighborRef), "neighbor array");
      iter->neighbor_indices = MEM_reallocN_id(
          iter->neighbor_indices, iter->capacity * sizeof(int), "neighbor array");
    }
  }

  iter->neighbors[iter->size].vertex = neighbor;
  iter->neighbors[iter->size].edge = edge;
  iter->neighbor_indices[iter->size] = neighbor_index;
  iter->size++;
}

static void sculpt_vertex_neighbors_get_bmesh(const SculptSession *ss,
                                              SculptVertRef index,
                                              SculptVertexNeighborIter *iter)
{
  BMVert *v = (BMVert *)index.i;

  iter->is_duplicate = false;
  iter->size = 0;
  iter->num_duplicates = 0;
  iter->has_edge = true;
  iter->capacity = SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY;
  iter->neighbors = iter->neighbors_fixed;
  iter->neighbor_indices = iter->neighbor_indices_fixed;
  iter->i = 0;

  // cache profiling revealed a hotspot here, don't use BM_ITER
  BMEdge *e = v->e;

  if (!v->e) {
    return;
  }

  BMEdge *e2 = NULL;

  do {
    BMVert *v2;
    e2 = BM_DISK_EDGE_NEXT(e, v);
    v2 = v == e->v1 ? e->v2 : e->v1;

    MDynTopoVert *mv = BKE_PBVH_DYNVERT(ss->cd_dyn_vert, v2);

    if (!(mv->flag & DYNVERT_VERT_FSET_HIDDEN)) {  // && (e->head.hflag & BM_ELEM_DRAW)) {
      sculpt_vertex_neighbor_add_nocheck(iter,
                                         BKE_pbvh_make_vref((intptr_t)v2),
                                         BKE_pbvh_make_eref((intptr_t)e),
                                         BM_elem_index_get(v2));
    }
  } while ((e = e2) != v->e);

  if (ss->fake_neighbors.use_fake_neighbors) {
    int index = BM_elem_index_get(v);

    BLI_assert(ss->fake_neighbors.fake_neighbor_index != NULL);
    if (ss->fake_neighbors.fake_neighbor_index[index].i != FAKE_NEIGHBOR_NONE) {
      sculpt_vertex_neighbor_add(iter,
                                 ss->fake_neighbors.fake_neighbor_index[index],
                                 BKE_pbvh_make_eref(SCULPT_REF_NONE),
                                 ss->fake_neighbors.fake_neighbor_index[index].i);
    }
  }
}

static void sculpt_vertex_neighbors_get_faces(const SculptSession *ss,
                                              SculptVertRef vertex,
                                              SculptVertexNeighborIter *iter)
{
  int index = BKE_pbvh_vertex_index_to_table(ss->pbvh, vertex);

  MeshElemMap *vert_map = &ss->pmap[index];
  iter->size = 0;
  iter->num_duplicates = 0;
  iter->capacity = SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY;
  iter->neighbors = iter->neighbors_fixed;
  iter->neighbor_indices = iter->neighbor_indices_fixed;
  iter->is_duplicate = false;
  iter->has_edge = true;

  for (int i = 0; i < ss->pmap[index].count; i++) {
    if (ss->face_sets[vert_map->indices[i]] < 0) {
      /* Skip connectivity from hidden faces. */
      continue;
    }
    const MPoly *p = &ss->mpoly[vert_map->indices[i]];
    uint f_adj_v[2];

    MLoop *l = &ss->mloop[p->loopstart];
    int e1, e2;

    bool ok = false;

    for (int j = 0; j < p->totloop; j++, l++) {
      if (l->v == index) {
        f_adj_v[0] = ME_POLY_LOOP_PREV(ss->mloop, p, j)->v;
        f_adj_v[1] = ME_POLY_LOOP_NEXT(ss->mloop, p, j)->v;

        e1 = ME_POLY_LOOP_PREV(ss->mloop, p, j)->e;
        e2 = l->e;
        ok = true;
        break;
      }
    }

    if (ok) {
      for (int j = 0; j < 2; j += 1) {
        int e = 0;

        if (f_adj_v[j] != index) {
          sculpt_vertex_neighbor_add(
              iter, BKE_pbvh_make_vref(f_adj_v[j]), BKE_pbvh_make_eref(j ? e2 : e1), f_adj_v[j]);
        }
      }
    }
  }

  if (ss->fake_neighbors.use_fake_neighbors) {
    BLI_assert(ss->fake_neighbors.fake_neighbor_index != NULL);
    if (ss->fake_neighbors.fake_neighbor_index[index].i != FAKE_NEIGHBOR_NONE) {
      sculpt_vertex_neighbor_add(iter,
                                 ss->fake_neighbors.fake_neighbor_index[index],
                                 BKE_pbvh_make_eref(SCULPT_REF_NONE),
                                 ss->fake_neighbors.fake_neighbor_index[index].i);
    }
  }
}

static void sculpt_vertex_neighbors_get_faces_vemap(const SculptSession *ss,
                                                    SculptVertRef vertex,
                                                    SculptVertexNeighborIter *iter)
{
  int index = BKE_pbvh_vertex_index_to_table(ss->pbvh, vertex);

  MeshElemMap *vert_map = &ss->vemap[index];
  iter->size = 0;
  iter->num_duplicates = 0;
  iter->capacity = SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY;
  iter->neighbors = iter->neighbors_fixed;
  iter->neighbor_indices = iter->neighbor_indices_fixed;
  iter->is_duplicate = false;

  for (int i = 0; i < vert_map->count; i++) {
    const MEdge *me = &ss->medge[vert_map->indices[i]];

    unsigned int v = me->v1 == (unsigned int)vertex.i ? me->v2 : me->v1;
    MDynTopoVert *mv = ss->mdyntopo_verts + v;

    if (mv->flag & DYNVERT_VERT_FSET_HIDDEN) {
      /* Skip connectivity from hidden faces. */
      continue;
    }

    sculpt_vertex_neighbor_add(
        iter, BKE_pbvh_make_vref(v), BKE_pbvh_make_eref(vert_map->indices[i]), v);
  }

  if (ss->fake_neighbors.use_fake_neighbors) {
    BLI_assert(ss->fake_neighbors.fake_neighbor_index != NULL);
    if (ss->fake_neighbors.fake_neighbor_index[index].i != FAKE_NEIGHBOR_NONE) {
      sculpt_vertex_neighbor_add(iter,
                                 ss->fake_neighbors.fake_neighbor_index[index],
                                 BKE_pbvh_make_eref(SCULPT_REF_NONE),
                                 ss->fake_neighbors.fake_neighbor_index[index].i);
    }
  }
}

static void sculpt_vertex_neighbors_get_grids(const SculptSession *ss,
                                              const SculptVertRef vertex,
                                              const bool include_duplicates,
                                              SculptVertexNeighborIter *iter)
{
  int index = (int)vertex.i;

  /* TODO: optimize this. We could fill #SculptVertexNeighborIter directly,
   * maybe provide coordinate and mask pointers directly rather than converting
   * back and forth between #CCGElem and global index. */
  const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
  const int grid_index = index / key->grid_area;
  const int vertex_index = index - grid_index * key->grid_area;

  SubdivCCGCoord coord = {.grid_index = grid_index,
                          .x = vertex_index % key->grid_size,
                          .y = vertex_index / key->grid_size};

  SubdivCCGNeighbors neighbors;
  BKE_subdiv_ccg_neighbor_coords_get(ss->subdiv_ccg, &coord, include_duplicates, &neighbors);

  iter->is_duplicate = include_duplicates;

  iter->size = 0;
  iter->num_duplicates = neighbors.num_duplicates;
  iter->capacity = SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY;
  iter->neighbors = iter->neighbors_fixed;
  iter->neighbor_indices = iter->neighbor_indices_fixed;

  for (int i = 0; i < neighbors.size; i++) {
    int idx = neighbors.coords[i].grid_index * key->grid_area +
              neighbors.coords[i].y * key->grid_size + neighbors.coords[i].x;

    sculpt_vertex_neighbor_add(
        iter, BKE_pbvh_make_vref(idx), BKE_pbvh_make_eref(SCULPT_REF_NONE), idx);
  }

  if (ss->fake_neighbors.use_fake_neighbors) {
    BLI_assert(ss->fake_neighbors.fake_neighbor_index != NULL);
    if (ss->fake_neighbors.fake_neighbor_index[index].i != FAKE_NEIGHBOR_NONE) {
      sculpt_vertex_neighbor_add(iter,
                                 ss->fake_neighbors.fake_neighbor_index[index],
                                 BKE_pbvh_make_eref(SCULPT_REF_NONE),
                                 ss->fake_neighbors.fake_neighbor_index[index].i);
    }
  }

  if (neighbors.coords != neighbors.coords_fixed) {
    MEM_freeN(neighbors.coords);
  }
}

void SCULPT_vertex_neighbors_get(const SculptSession *ss,
                                 const SculptVertRef vertex,
                                 const bool include_duplicates,
                                 SculptVertexNeighborIter *iter)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      // use vemap if it exists, so result is in disk cycle order
      if (ss->vemap) {
        sculpt_vertex_neighbors_get_faces_vemap(ss, vertex, iter);
      }
      else {
        sculpt_vertex_neighbors_get_faces(ss, vertex, iter);
      }
      return;
    case PBVH_BMESH:
      sculpt_vertex_neighbors_get_bmesh(ss, vertex, iter);
      return;
    case PBVH_GRIDS:
      sculpt_vertex_neighbors_get_grids(ss, vertex, include_duplicates, iter);
      return;
  }
}

SculptBoundaryType SCULPT_edge_is_boundary(const SculptSession *ss,
                                           const SculptEdgeRef edge,
                                           SculptBoundaryType typemask)
{

  int ret = 0;

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      BMEdge *e = (BMEdge *)edge.i;

      if (typemask & SCULPT_BOUNDARY_MESH) {
        ret |= (!e->l || e->l == e->l->radial_next) ? SCULPT_BOUNDARY_MESH : 0;
      }

      if ((typemask & SCULPT_BOUNDARY_FACE_SET) && e->l && e->l != e->l->radial_next) {
        if (ss->boundary_symmetry) {
          // TODO: calc and cache this properly
          MDynTopoVert *mv1 = BKE_PBVH_DYNVERT(ss->cd_dyn_vert, e->v1);
          MDynTopoVert *mv2 = BKE_PBVH_DYNVERT(ss->cd_dyn_vert, e->v2);

          return (mv1->flag & DYNVERT_FSET_BOUNDARY) && (mv2->flag & DYNVERT_FSET_BOUNDARY);
        }
        else {
          int fset1 = BM_ELEM_CD_GET_INT(e->l->f, ss->cd_faceset_offset);
          int fset2 = BM_ELEM_CD_GET_INT(e->l->radial_next->f, ss->cd_faceset_offset);

          bool ok = (fset1 < 0) != (fset2 < 0);

          ok = ok || fset1 != fset2;

          ret |= ok ? SCULPT_BOUNDARY_FACE_SET : 0;
        }
      }

      if (typemask & SCULPT_BOUNDARY_SHARP) {
        ret |= !BM_elem_flag_test(e, BM_ELEM_SMOOTH) ? SCULPT_BOUNDARY_SHARP : 0;
      }

      if (typemask & SCULPT_BOUNDARY_SEAM) {
        ret |= BM_elem_flag_test(e, BM_ELEM_SEAM) ? SCULPT_BOUNDARY_SEAM : 0;
      }

      break;
    }
    case PBVH_FACES: {
      int mask = typemask & (SCULPT_BOUNDARY_MESH | SCULPT_BOUNDARY_FACE_SET);
      SculptVertRef v1, v2;

      SCULPT_edge_get_verts(ss, edge, &v1, &v2);

      if (mask) {  // use less accurate approximation for now
        SculptBoundaryType a = SCULPT_vertex_is_boundary(ss, v1, mask);
        SculptBoundaryType b = SCULPT_vertex_is_boundary(ss, v2, mask);

        ret |= a & b;
      }

      if (typemask & SCULPT_BOUNDARY_SHARP) {
        ret |= ss->medge[edge.i].flag & ME_SHARP ? SCULPT_BOUNDARY_SHARP : 0;
      }

      if (typemask & SCULPT_BOUNDARY_SEAM) {
        ret |= ss->medge[edge.i].flag & ME_SEAM ? SCULPT_BOUNDARY_SEAM : 0;
      }

      break;
    }
    case PBVH_GRIDS: {
      // not implemented
      break;
    }
  }

  return (SculptBoundaryType)ret;
}

void SCULPT_edge_get_verts(const SculptSession *ss,
                           const SculptEdgeRef edge,
                           SculptVertRef *r_v1,
                           SculptVertRef *r_v2)

{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      BMEdge *e = (BMEdge *)edge.i;
      r_v1->i = (intptr_t)e->v1;
      r_v2->i = (intptr_t)e->v2;
      break;
    }

    case PBVH_FACES: {
      r_v1->i = (intptr_t)ss->medge[edge.i].v1;
      r_v2->i = (intptr_t)ss->medge[edge.i].v2;
      break;
    }
    case PBVH_GRIDS:
      // not supported yet
      r_v1->i = r_v2->i = SCULPT_REF_NONE;
      break;
  }
}

SculptVertRef SCULPT_edge_other_vertex(const SculptSession *ss,
                                       const SculptEdgeRef edge,
                                       const SculptVertRef vertex)
{
  SculptVertRef v1, v2;

  SCULPT_edge_get_verts(ss, edge, &v1, &v2);

  return v1.i == vertex.i ? v2 : v1;
}

static bool sculpt_check_boundary_vertex_in_base_mesh(const SculptSession *ss,
                                                      const SculptVertRef index)
{
  BLI_assert(ss->vertex_info.boundary);
  return BLI_BITMAP_TEST(ss->vertex_info.boundary,
                         BKE_pbvh_vertex_index_to_table(ss->pbvh, index));
}
static void faces_update_boundary_flags(const SculptSession *ss, const SculptVertRef vertex)
{
  BKE_pbvh_update_vert_boundary_faces(ss->face_sets,
                                      ss->mvert,
                                      ss->medge,
                                      ss->mloop,
                                      ss->mpoly,
                                      ss->mdyntopo_verts,
                                      ss->pmap,
                                      vertex);
  // have to handle boundary here
  MDynTopoVert *mv = ss->mdyntopo_verts + vertex.i;

  mv->flag &= ~(DYNVERT_CORNER | DYNVERT_BOUNDARY);

  if (sculpt_check_boundary_vertex_in_base_mesh(ss, vertex)) {
    mv->flag |= DYNVERT_BOUNDARY;

    if (ss->pmap[vertex.i].count < 4) {
      bool ok = true;

      for (int i = 0; i < ss->pmap[vertex.i].count; i++) {
        MPoly *mp = ss->mpoly + ss->pmap[vertex.i].indices[i];
        if (mp->totloop < 4) {
          ok = false;
        }
      }
      if (ok) {
        mv->flag |= DYNVERT_CORNER;
      }
      else {
        mv->flag &= ~DYNVERT_CORNER;
      }
    }
  }
}
SculptCornerType SCULPT_vertex_is_corner(const SculptSession *ss,
                                         const SculptVertRef vertex,
                                         SculptCornerType cornertype)
{
  bool check_facesets = cornertype & SCULPT_CORNER_FACE_SET;
  SculptCornerType ret = 0;
  MDynTopoVert *mv = NULL;

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      BMVert *v = (BMVert *)vertex.i;
      mv = BKE_PBVH_DYNVERT(ss->cd_dyn_vert, v);

      if (mv->flag & DYNVERT_NEED_BOUNDARY) {
        BKE_pbvh_update_vert_boundary(ss->cd_dyn_vert,
                                      ss->cd_faceset_offset,
                                      ss->cd_vert_node_offset,
                                      ss->cd_face_node_offset,
                                      (BMVert *)vertex.i,
                                      ss->boundary_symmetry);
      }

      break;
    }
    case PBVH_FACES:
      mv = ss->mdyntopo_verts + vertex.i;

      if (mv->flag & DYNVERT_NEED_BOUNDARY) {
        faces_update_boundary_flags(ss, vertex);
      }
      break;
    case PBVH_GRIDS: {
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = vertex.i / key->grid_area;
      const int vertex_index = vertex.i - grid_index * key->grid_area;
      const SubdivCCGCoord coord = {.grid_index = grid_index,
                                    .x = vertex_index % key->grid_size,
                                    .y = vertex_index / key->grid_size};
      int v1, v2;
      const SubdivCCGAdjacencyType adjacency = BKE_subdiv_ccg_coarse_mesh_adjacency_info_get(
          ss->subdiv_ccg, &coord, ss->mloop, ss->mpoly, &v1, &v2);
      switch (adjacency) {
        case SUBDIV_CCG_ADJACENT_VERTEX:
          return sculpt_check_corner_in_base_mesh(ss, BKE_pbvh_make_vref(v1), check_facesets);
        case SUBDIV_CCG_ADJACENT_EDGE:
          return false;  // sculpt_check_unique_face_set_for_edge_in_base_mesh(ss, v1, v2);
        case SUBDIV_CCG_ADJACENT_NONE:
          return false;
      }

      return 0;
    }
  }

  ret = 0;

  if (cornertype & SCULPT_CORNER_MESH) {
    ret |= (mv->flag & DYNVERT_CORNER) ? SCULPT_CORNER_MESH : 0;
  }
  if (cornertype & SCULPT_CORNER_FACE_SET) {
    ret |= (mv->flag & DYNVERT_FSET_CORNER) ? SCULPT_CORNER_FACE_SET : 0;
  }
  if (cornertype & SCULPT_CORNER_SEAM) {
    ret |= (mv->flag & DYNVERT_SEAM_CORNER) ? SCULPT_CORNER_SEAM : 0;
  }
  if (cornertype & SCULPT_CORNER_SHARP) {
    ret |= (mv->flag & DYNVERT_SHARP_CORNER) ? SCULPT_CORNER_SHARP : 0;
  }

  return ret;
}

SculptBoundaryType SCULPT_vertex_is_boundary(const SculptSession *ss,
                                             const SculptVertRef vertex,
                                             SculptBoundaryType boundary_types)
{
  bool check_facesets = boundary_types & SCULPT_BOUNDARY_FACE_SET;
  MDynTopoVert *mv = NULL;

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      mv = BKE_PBVH_DYNVERT(ss->cd_dyn_vert, ((BMVert *)(vertex.i)));

      if (mv->flag & DYNVERT_NEED_BOUNDARY) {
        BKE_pbvh_update_vert_boundary(ss->cd_dyn_vert,
                                      ss->cd_faceset_offset,
                                      ss->cd_vert_node_offset,
                                      ss->cd_face_node_offset,
                                      (BMVert *)vertex.i,
                                      ss->boundary_symmetry);
      }

      break;
    }
    case PBVH_FACES: {
      mv = ss->mdyntopo_verts + vertex.i;

      if (mv->flag & DYNVERT_NEED_BOUNDARY) {
        faces_update_boundary_flags(ss, vertex);
      }
      break;
    }

    case PBVH_GRIDS: {
      int index = (int)vertex.i;
      const CCGKey *key = BKE_pbvh_get_grid_key(ss->pbvh);
      const int grid_index = index / key->grid_area;
      const int vertex_index = index - grid_index * key->grid_area;
      const SubdivCCGCoord coord = {.grid_index = grid_index,
                                    .x = vertex_index % key->grid_size,
                                    .y = vertex_index / key->grid_size};
      int v1, v2;
      const SubdivCCGAdjacencyType adjacency = BKE_subdiv_ccg_coarse_mesh_adjacency_info_get(
          ss->subdiv_ccg, &coord, ss->mloop, ss->mpoly, &v1, &v2);
      switch (adjacency) {
        case SUBDIV_CCG_ADJACENT_VERTEX:
          return sculpt_check_boundary_vertex_in_base_mesh(ss, BKE_pbvh_make_vref(v1)) ?
                     SCULPT_BOUNDARY_MESH :
                     0;
        case SUBDIV_CCG_ADJACENT_EDGE:
          if (sculpt_check_boundary_vertex_in_base_mesh(ss, BKE_pbvh_make_vref(v1)) &&
              sculpt_check_boundary_vertex_in_base_mesh(ss, BKE_pbvh_make_vref(v2))) {
            return SCULPT_BOUNDARY_MESH;
          }
        case SUBDIV_CCG_ADJACENT_NONE:
          return 0;
      }
    }
  }

  int flag = 0;
  if (boundary_types & SCULPT_BOUNDARY_MESH) {
    flag |= (mv->flag & DYNVERT_BOUNDARY) ? SCULPT_BOUNDARY_MESH : 0;
  }
  if (boundary_types & SCULPT_BOUNDARY_FACE_SET) {
    flag |= (mv->flag & DYNVERT_FSET_BOUNDARY) ? SCULPT_BOUNDARY_FACE_SET : 0;
  }
  if (boundary_types & SCULPT_BOUNDARY_SHARP) {
    flag |= (mv->flag & DYNVERT_SHARP_BOUNDARY) ? SCULPT_BOUNDARY_SHARP : 0;
  }
  if (boundary_types & SCULPT_BOUNDARY_SEAM) {
    flag |= (mv->flag & DYNVERT_SEAM_BOUNDARY) ? SCULPT_BOUNDARY_SEAM : 0;
  }

  return flag;
  return 0;
}

/* Utilities */

/**
 * Returns true when the step belongs to the stroke that is directly performed by the brush and
 * not by one of the symmetry passes.
 */
bool SCULPT_stroke_is_main_symmetry_pass(StrokeCache *cache)
{
  return cache->mirror_symmetry_pass == 0 && cache->radial_symmetry_pass == 0 &&
         cache->tile_pass == 0;
}

/**
 * Return true only once per stroke on the first symmetry pass, regardless of the symmetry passes
 * enabled.
 *
 * This should be used for functionality that needs to be computed once per stroke of a
 * particular tool (allocating memory, updating random seeds...).
 */
bool SCULPT_stroke_is_first_brush_step(StrokeCache *cache)
{
  return cache->first_time && cache->mirror_symmetry_pass == 0 &&
         cache->radial_symmetry_pass == 0 && cache->tile_pass == 0;
}

/**
 * Returns true on the first brush step of each symmetry pass.
 */
bool SCULPT_stroke_is_first_brush_step_of_symmetry_pass(StrokeCache *cache)
{
  return cache->first_time;
}

bool SCULPT_check_vertex_pivot_symmetry(const float vco[3], const float pco[3], const char symm)
{
  bool is_in_symmetry_area = true;
  for (int i = 0; i < 3; i++) {
    char symm_it = 1 << i;
    if (symm & symm_it) {
      if (pco[i] == 0.0f) {
        if (vco[i] > 0.0f) {
          is_in_symmetry_area = false;
        }
      }
      if (vco[i] * pco[i] < 0.0f) {
        is_in_symmetry_area = false;
      }
    }
  }
  return is_in_symmetry_area;
}

typedef struct NearestVertexTLSData {
  int nearest_vertex_index;
  SculptVertRef nearest_vertex;
  float nearest_vertex_distance_squared;
} NearestVertexTLSData;

static void do_nearest_vertex_get_task_cb(void *__restrict userdata,
                                          const int n,
                                          const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  NearestVertexTLSData *nvtd = tls->userdata_chunk;
  PBVHVertexIter vd;

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    float distance_squared = len_squared_v3v3(vd.co, data->nearest_vertex_search_co);
    if (distance_squared < nvtd->nearest_vertex_distance_squared &&
        distance_squared < data->max_distance_squared) {
      nvtd->nearest_vertex_index = vd.index;
      nvtd->nearest_vertex = vd.vertex;
      nvtd->nearest_vertex_distance_squared = distance_squared;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void nearest_vertex_get_reduce(const void *__restrict UNUSED(userdata),
                                      void *__restrict chunk_join,
                                      void *__restrict chunk)
{
  NearestVertexTLSData *join = chunk_join;
  NearestVertexTLSData *nvtd = chunk;
  if (join->nearest_vertex_index == -1) {
    join->nearest_vertex_index = nvtd->nearest_vertex_index;
    join->nearest_vertex = nvtd->nearest_vertex;
    join->nearest_vertex_distance_squared = nvtd->nearest_vertex_distance_squared;
  }
  else if (nvtd->nearest_vertex_distance_squared < join->nearest_vertex_distance_squared) {
    join->nearest_vertex_index = nvtd->nearest_vertex_index;
    join->nearest_vertex = nvtd->nearest_vertex;
    join->nearest_vertex_distance_squared = nvtd->nearest_vertex_distance_squared;
  }
}

SculptVertRef SCULPT_nearest_vertex_get(
    Sculpt *sd, Object *ob, const float co[3], float max_distance, bool use_original)
{
  SculptSession *ss = ob->sculpt;
  PBVHNode **nodes = NULL;
  int totnode;
  SculptSearchSphereData data = {
      .ss = ss,
      .sd = sd,
      .radius_squared = max_distance * max_distance,
      .original = use_original,
      .center = co,
  };
  BKE_pbvh_search_gather(ss->pbvh, SCULPT_search_sphere_cb, &data, &nodes, &totnode);
  if (totnode == 0) {
    return BKE_pbvh_make_vref(-1);
  }

  SculptThreadedTaskData task_data = {
      .sd = sd,
      .ob = ob,
      .nodes = nodes,
      .max_distance_squared = max_distance * max_distance,
  };

  copy_v3_v3(task_data.nearest_vertex_search_co, co);
  NearestVertexTLSData nvtd;
  nvtd.nearest_vertex_index = -1;
  nvtd.nearest_vertex.i = -1;
  nvtd.nearest_vertex_distance_squared = FLT_MAX;

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  settings.func_reduce = nearest_vertex_get_reduce;
  settings.userdata_chunk = &nvtd;
  settings.userdata_chunk_size = sizeof(NearestVertexTLSData);
  BLI_task_parallel_range(0, totnode, &task_data, do_nearest_vertex_get_task_cb, &settings);

  MEM_SAFE_FREE(nodes);

  return nvtd.nearest_vertex;
}

bool SCULPT_is_symmetry_iteration_valid(char i, char symm)
{
  return i == 0 || (symm & i && (symm != 5 || i != 3) && (symm != 6 || (i != 3 && i != 5)));
}

/* Checks if a vertex is inside the brush radius from any of its mirrored axis. */
bool SCULPT_is_vertex_inside_brush_radius_symm(const float vertex[3],
                                               const float br_co[3],
                                               float radius,
                                               char symm)
{
  for (char i = 0; i <= symm; ++i) {
    if (!SCULPT_is_symmetry_iteration_valid(i, symm)) {
      continue;
    }
    float location[3];
    flip_v3_v3(location, br_co, (char)i);
    if (len_squared_v3v3(location, vertex) < radius * radius) {
      return true;
    }
  }
  return false;
}

void SCULPT_tag_update_overlays(bContext *C)
{
  ARegion *region = CTX_wm_region(C);
  ED_region_tag_redraw(region);

  Object *ob = CTX_data_active_object(C);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  DEG_id_tag_update(&ob->id, ID_RECALC_SHADING);
  View3D *v3d = CTX_wm_view3d(C);
  if (!BKE_sculptsession_use_pbvh_draw(ob, v3d)) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }
}

/* Sculpt Flood Fill API
 *
 * Iterate over connected vertices, starting from one or more initial vertices. */

void SCULPT_floodfill_init(SculptSession *ss, SculptFloodFill *flood)
{
  int vertex_count = SCULPT_vertex_count_get(ss);
  SCULPT_vertex_random_access_ensure(ss);

  flood->queue = BLI_gsqueue_new(sizeof(SculptVertRef));
  flood->visited_vertices = BLI_BITMAP_NEW(vertex_count, "visited vertices");
}

void SCULPT_floodfill_add_initial(SculptFloodFill *flood, SculptVertRef vertex)
{
  BLI_gsqueue_push(flood->queue, &vertex);
}

void SCULPT_floodfill_add_and_skip_initial(SculptSession *ss,
                                           SculptFloodFill *flood,
                                           SculptVertRef vertex)
{
  BLI_gsqueue_push(flood->queue, &vertex);
  BLI_BITMAP_ENABLE(flood->visited_vertices, BKE_pbvh_vertex_index_to_table(ss->pbvh, vertex));
}

void SCULPT_floodfill_add_initial_with_symmetry(Sculpt *sd,
                                                Object *ob,
                                                SculptSession *ss,
                                                SculptFloodFill *flood,
                                                SculptVertRef vertex,
                                                float radius)
{
  /* Add active vertex and symmetric vertices to the queue. */
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  for (char i = 0; i <= symm; ++i) {
    if (!SCULPT_is_symmetry_iteration_valid(i, symm)) {
      continue;
    }
    SculptVertRef v = {-1};
    if (i == 0) {
      v = vertex;
    }
    else if (radius > 0.0f) {
      float radius_squared = (radius == FLT_MAX) ? FLT_MAX : radius * radius;
      float location[3];
      flip_v3_v3(location, SCULPT_vertex_co_get(ss, vertex), i);
      v = SCULPT_nearest_vertex_get(sd, ob, location, radius_squared, false);
    }

    if (v.i != -1) {
      SCULPT_floodfill_add_initial(flood, v);
    }
  }
}

void SCULPT_floodfill_add_active(
    Sculpt *sd, Object *ob, SculptSession *ss, SculptFloodFill *flood, float radius)
{
  /* Add active vertex and symmetric vertices to the queue. */
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);
  for (char i = 0; i <= symm; ++i) {
    if (!SCULPT_is_symmetry_iteration_valid(i, symm)) {
      continue;
    }

    SculptVertRef v = {-1};

    if (i == 0) {
      v = SCULPT_active_vertex_get(ss);
    }
    else if (radius > 0.0f) {
      float location[3];
      flip_v3_v3(location, SCULPT_active_vertex_co_get(ss), i);
      v = SCULPT_nearest_vertex_get(sd, ob, location, radius, false);
    }

    if (v.i != -1) {
      SCULPT_floodfill_add_initial(flood, v);
    }
  }
}

void SCULPT_floodfill_execute(SculptSession *ss,
                              SculptFloodFill *flood,
                              bool (*func)(SculptSession *ss,
                                           SculptVertRef from_v,
                                           SculptVertRef to_v,
                                           bool is_duplicate,
                                           void *userdata),
                              void *userdata)
{
  while (!BLI_gsqueue_is_empty(flood->queue)) {
    SculptVertRef from_v;
    BLI_gsqueue_pop(flood->queue, &from_v);
    SculptVertexNeighborIter ni;
    SCULPT_VERTEX_DUPLICATES_AND_NEIGHBORS_ITER_BEGIN (ss, from_v, ni) {
      const SculptVertRef to_v = ni.vertex;

      const int to_index = BKE_pbvh_vertex_index_to_table(ss->pbvh, to_v);

      if (BLI_BITMAP_TEST(flood->visited_vertices, to_index)) {
        continue;
      }

      if (!SCULPT_vertex_visible_get(ss, to_v)) {
        continue;
      }

      BLI_BITMAP_ENABLE(flood->visited_vertices, to_index);

      if (func(ss, from_v, to_v, ni.is_duplicate, userdata)) {
        BLI_gsqueue_push(flood->queue, &to_v);
      }
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  }
}

void SCULPT_floodfill_free(SculptFloodFill *flood)
{
  MEM_SAFE_FREE(flood->visited_vertices);
  BLI_gsqueue_free(flood->queue);
  flood->queue = NULL;
}

/* -------------------------------------------------------------------- */
/** \name Tool Capabilities
 *
 * Avoid duplicate checks, internal logic only,
 * share logic with #rna_def_sculpt_capabilities where possible.
 *
 * \{ */

static bool sculpt_tool_needs_original(const char sculpt_tool)
{
  return ELEM(sculpt_tool,
              SCULPT_TOOL_GRAB,
              SCULPT_TOOL_ROTATE,
              SCULPT_TOOL_THUMB,
              SCULPT_TOOL_LAYER,
              SCULPT_TOOL_DRAW_SHARP,
              SCULPT_TOOL_ELASTIC_DEFORM,
              SCULPT_TOOL_SMOOTH,
              SCULPT_TOOL_PAINT,
              SCULPT_TOOL_VCOL_BOUNDARY,
              SCULPT_TOOL_BOUNDARY,
              SCULPT_TOOL_FAIRING,
              SCULPT_TOOL_POSE);
}

bool sculpt_tool_is_proxy_used(const char sculpt_tool)
{
  return ELEM(sculpt_tool,
              SCULPT_TOOL_SMOOTH,
              SCULPT_TOOL_LAYER,
              SCULPT_TOOL_FAIRING,
              SCULPT_TOOL_SCENE_PROJECT,
              SCULPT_TOOL_POSE,
              SCULPT_TOOL_ARRAY,
              SCULPT_TOOL_TWIST,
              SCULPT_TOOL_DISPLACEMENT_SMEAR,
              SCULPT_TOOL_BOUNDARY,
              SCULPT_TOOL_CLOTH,
              SCULPT_TOOL_PAINT,
              SCULPT_TOOL_SMEAR,
              SCULPT_TOOL_SYMMETRIZE,
              SCULPT_TOOL_DRAW_FACE_SETS);
}

static bool sculpt_brush_use_topology_rake(const SculptSession *ss, const Brush *brush)
{
  return SCULPT_TOOL_HAS_TOPOLOGY_RAKE(brush->sculpt_tool) &&
         (brush->topology_rake_factor > 0.0f) && (ss->bm != NULL);
}

/**
 * Test whether the #StrokeCache.sculpt_normal needs update in #do_brush_action
 */
static int sculpt_brush_needs_normal(const SculptSession *ss, const Brush *brush)
{
  return ((SCULPT_TOOL_HAS_NORMAL_WEIGHT(brush->sculpt_tool) &&
           (ss->cache->normal_weight > 0.0f)) ||

          ELEM(brush->sculpt_tool,
               SCULPT_TOOL_BLOB,
               SCULPT_TOOL_CREASE,
               SCULPT_TOOL_DRAW,
               SCULPT_TOOL_DRAW_SHARP,
               SCULPT_TOOL_SCENE_PROJECT,
               SCULPT_TOOL_CLOTH,
               SCULPT_TOOL_LAYER,
               SCULPT_TOOL_NUDGE,
               SCULPT_TOOL_ROTATE,
               SCULPT_TOOL_ELASTIC_DEFORM,
               SCULPT_TOOL_THUMB) ||

          (brush->mtex.brush_map_mode == MTEX_MAP_MODE_AREA)) ||
         sculpt_brush_use_topology_rake(ss, brush);
}
/** \} */

static bool sculpt_brush_needs_rake_rotation(const Brush *brush)
{
  return SCULPT_TOOL_HAS_RAKE(brush->sculpt_tool) && (brush->rake_factor != 0.0f);
}

typedef enum StrokeFlags {
  CLIP_X = 1,
  CLIP_Y = 2,
  CLIP_Z = 4,
} StrokeFlags;

/**
 * Initialize a #SculptOrigVertData for accessing original vertex data;
 * handles #BMesh, #Mesh, and multi-resolution.
 */
void SCULPT_orig_vert_data_unode_init(SculptOrigVertData *data, Object *ob, SculptUndoNode *unode)
{
  SculptSession *ss = ob->sculpt;

  // do nothing

  BMesh *bm = ss->bm;

  memset(data, 0, sizeof(*data));
  data->unode = unode;

  data->pbvh = ss->pbvh;
  data->ss = ss;

  if (bm) {
    data->bm_log = ss->bm_log;
  }

  else {
    // data->datatype = data->unode->type;
  }
}

/**
 * Initialize a #SculptOrigVertData for accessing original vertex data;
 * handles #BMesh, #Mesh, and multi-resolution.
 */
void SCULPT_orig_vert_data_init(SculptOrigVertData *data,
                                Object *ob,
                                PBVHNode *node,
                                SculptUndoType type)
{
  SculptUndoNode *unode = NULL;
  data->ss = ob->sculpt;

  // don't need undo node here anymore
  if (!ob->sculpt->bm) {
    // unode = SCULPT_undo_push_node(ob, node, type);
  }

  SCULPT_orig_vert_data_unode_init(data, ob, unode);
  data->datatype = type;
}

void SCULPT_vertex_check_origdata(SculptSession *ss, SculptVertRef vertex)
{
  // check if we need to update original data for current stroke
  MDynTopoVert *mv = ss->bm ? BKE_PBVH_DYNVERT(ss->cd_dyn_vert, (BMVert *)vertex.i) :
                              ss->mdyntopo_verts + vertex.i;

  if (mv->stroke_id != ss->stroke_id) {
    mv->stroke_id = ss->stroke_id;

    copy_v3_v3(mv->origco, SCULPT_vertex_co_get(ss, vertex));
    SCULPT_vertex_normal_get(ss, vertex, mv->origno);

    const float *color = SCULPT_vertex_color_get(ss, vertex);
    if (color) {
      copy_v4_v4(mv->origcolor, color);
    }

    mv->origmask = SCULPT_vertex_mask_get(ss, vertex);
  }
}

/**
 * DEPRECATED use Update a #SculptOrigVertData for a particular vertex from the PBVH iterator.
 */
void SCULPT_orig_vert_data_update(SculptOrigVertData *orig_data, SculptVertRef vertex)
{
  // check if we need to update original data for current stroke
  MDynTopoVert *mv = SCULPT_vertex_get_mdyntopo(orig_data->ss, vertex);

  SCULPT_vertex_check_origdata(orig_data->ss, vertex);

  if (orig_data->datatype == SCULPT_UNDO_COORDS) {
    float *no = mv->origno;
    normal_float_to_short_v3(orig_data->_no, no);

    orig_data->no = orig_data->_no;
    orig_data->co = mv->origco;
  }
  else if (orig_data->datatype == SCULPT_UNDO_COLOR) {
    orig_data->col = mv->origcolor;
  }
  else if (orig_data->datatype == SCULPT_UNDO_MASK) {
    orig_data->mask = mv->origmask;
  }
}

static void sculpt_rake_data_update(struct SculptRakeData *srd, const float co[3])
{
  float rake_dist = len_v3v3(srd->follow_co, co);
  if (rake_dist > srd->follow_dist) {
    interp_v3_v3v3(srd->follow_co, srd->follow_co, co, rake_dist - srd->follow_dist);
  }
}

static void sculpt_rake_rotate(const SculptSession *ss,
                               const float sculpt_co[3],
                               const float v_co[3],
                               float factor,
                               float r_delta[3])
{
  float vec_rot[3];

#if 0
  /* lerp */
  sub_v3_v3v3(vec_rot, v_co, sculpt_co);
  mul_qt_v3(ss->cache->rake_rotation_symmetry, vec_rot);
  add_v3_v3(vec_rot, sculpt_co);
  sub_v3_v3v3(r_delta, vec_rot, v_co);
  mul_v3_fl(r_delta, factor);
#else
  /* slerp */
  float q_interp[4];
  sub_v3_v3v3(vec_rot, v_co, sculpt_co);

  copy_qt_qt(q_interp, ss->cache->rake_rotation_symmetry);
  pow_qt_fl_normalized(q_interp, factor);
  mul_qt_v3(q_interp, vec_rot);

  add_v3_v3(vec_rot, sculpt_co);
  sub_v3_v3v3(r_delta, vec_rot, v_co);
#endif
}

/**
 * Align the grab delta to the brush normal.
 *
 * \param grab_delta: Typically from `ss->cache->grab_delta_symmetry`.
 */
static void sculpt_project_v3_normal_align(SculptSession *ss,
                                           const float normal_weight,
                                           float grab_delta[3])
{
  /* Signed to support grabbing in (to make a hole) as well as out. */
  const float len_signed = dot_v3v3(ss->cache->sculpt_normal_symm, grab_delta);

  /* This scale effectively projects the offset so dragging follows the cursor,
   * as the normal points towards the view, the scale increases. */
  float len_view_scale;
  {
    float view_aligned_normal[3];
    project_plane_v3_v3v3(
        view_aligned_normal, ss->cache->sculpt_normal_symm, ss->cache->view_normal);
    len_view_scale = fabsf(dot_v3v3(view_aligned_normal, ss->cache->sculpt_normal_symm));
    len_view_scale = (len_view_scale > FLT_EPSILON) ? 1.0f / len_view_scale : 1.0f;
  }

  mul_v3_fl(grab_delta, 1.0f - normal_weight);
  madd_v3_v3fl(
      grab_delta, ss->cache->sculpt_normal_symm, (len_signed * normal_weight) * len_view_scale);
}

/* -------------------------------------------------------------------- */
/** \name SculptProjectVector
 *
 * Fast-path for #project_plane_v3_v3v3
 *
 * \{ */

typedef struct SculptProjectVector {
  float plane[3];
  float len_sq;
  float len_sq_inv_neg;
  bool is_valid;

} SculptProjectVector;

/**
 * \param plane: Direction, can be any length.
 */
static void sculpt_project_v3_cache_init(SculptProjectVector *spvc, const float plane[3])
{
  copy_v3_v3(spvc->plane, plane);
  spvc->len_sq = len_squared_v3(spvc->plane);
  spvc->is_valid = (spvc->len_sq > FLT_EPSILON);
  spvc->len_sq_inv_neg = (spvc->is_valid) ? -1.0f / spvc->len_sq : 0.0f;
}

/**
 * Calculate the projection.
 */
static void sculpt_project_v3(const SculptProjectVector *spvc, const float vec[3], float r_vec[3])
{
#if 0
  project_plane_v3_v3v3(r_vec, vec, spvc->plane);
#else
  /* inline the projection, cache `-1.0 / dot_v3_v3(v_proj, v_proj)` */
  madd_v3_v3fl(r_vec, spvc->plane, dot_v3v3(vec, spvc->plane) * spvc->len_sq_inv_neg);
#endif
}

/** \} */

/**********************************************************************/

/* Returns true if the stroke will use dynamic topology, false
 * otherwise.
 *
 * Factors: some brushes like grab cannot do dynamic topology.
 * Others, like smooth, are better without.
 * Same goes for alt-key smoothing. */
bool SCULPT_stroke_is_dynamic_topology(const SculptSession *ss, const Brush *brush)
{
  return (
      (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) &&

      (!ss->cache || (!ss->cache->alt_smooth)) &&

      /* Requires mesh restore, which doesn't work with
       * dynamic-topology. */
      !(brush->flag & BRUSH_ANCHORED) && !(brush->flag & BRUSH_DRAG_DOT) &&
      (brush->cached_dyntopo.flag & (DYNTOPO_SUBDIVIDE | DYNTOPO_COLLAPSE | DYNTOPO_CLEANUP)) &&
      !(brush->cached_dyntopo.flag & DYNTOPO_DISABLED) &&
      SCULPT_TOOL_HAS_DYNTOPO(brush->sculpt_tool));
}

/*** paint mesh ***/

static void paint_mesh_restore_co_task_cb(void *__restrict userdata,
                                          const int n,
                                          const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;

  SculptUndoNode *unode;
  SculptUndoType type = (data->brush->sculpt_tool == SCULPT_TOOL_MASK ? SCULPT_UNDO_MASK :
                                                                        SCULPT_UNDO_COORDS);

  SculptUndoNode tmp = {0};
  if (ss->bm) {
    unode = &tmp;
    tmp.type = type;
    // unode = SCULPT_undo_push_node(data->ob, data->nodes[n], type);
  }
  else {
    unode = SCULPT_undo_get_node(data->nodes[n], type);

    if (!unode) {
      return;
    }
  }

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;

  SCULPT_orig_vert_data_unode_init(&orig_data, data->ob, unode);

  bool modified = false;

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);

    if (orig_data.unode->type == SCULPT_UNDO_COORDS) {
      if (len_squared_v3v3(vd.co, orig_data.co) > FLT_EPSILON) {
        modified = true;
      }

      copy_v3_v3(vd.co, orig_data.co);

      if (vd.no) {
        copy_v3_v3_short(vd.no, orig_data.no);
      }
      else {
        normal_short_to_float_v3(vd.fno, orig_data.no);
      }
    }
    else if (orig_data.unode->type == SCULPT_UNDO_MASK) {
      if ((*vd.mask - orig_data.mask) * (*vd.mask - orig_data.mask) > FLT_EPSILON) {
        modified = true;
      }

      *vd.mask = orig_data.mask;
    }
    else if (orig_data.unode->type == SCULPT_UNDO_COLOR && vd.col && orig_data.col) {
      if (len_squared_v4v4(vd.col, orig_data.col) > FLT_EPSILON) {
        modified = true;
      }

      copy_v4_v4(vd.col, orig_data.col);
    }

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;

  if (modified) {
    BKE_pbvh_node_mark_update(data->nodes[n]);
  }
}

static void paint_mesh_restore_co(Sculpt *sd, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = ss->cache ? ss->cache->brush : BKE_paint_brush(&sd->paint);

  PBVHNode **nodes;
  int totnode;

  BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, paint_mesh_restore_co_task_cb, &settings);

  BKE_pbvh_node_color_buffer_free(ss->pbvh);

  MEM_SAFE_FREE(nodes);
}

/*** BVH Tree ***/

static void sculpt_extend_redraw_rect_previous(Object *ob, rcti *rect)
{
  /* Expand redraw \a rect with redraw \a rect from previous step to
   * prevent partial-redraw issues caused by fast strokes. This is
   * needed here (not in sculpt_flush_update) as it was before
   * because redraw rectangle should be the same in both of
   * optimized PBVH draw function and 3d view redraw, if not -- some
   * mesh parts could disappear from screen (sergey). */
  SculptSession *ss = ob->sculpt;

  if (!ss->cache) {
    return;
  }

  if (BLI_rcti_is_empty(&ss->cache->previous_r)) {
    return;
  }

  BLI_rcti_union(rect, &ss->cache->previous_r);
}

/* Get a screen-space rectangle of the modified area. */
bool SCULPT_get_redraw_rect(ARegion *region, RegionView3D *rv3d, Object *ob, rcti *rect)
{
  PBVH *pbvh = ob->sculpt->pbvh;
  float bb_min[3], bb_max[3];

  if (!pbvh) {
    return false;
  }

  BKE_pbvh_redraw_BB(pbvh, bb_min, bb_max);

  /* Convert 3D bounding box to screen space. */
  if (!paint_convert_bb_to_rect(rect, bb_min, bb_max, region, rv3d, ob)) {
    return false;
  }

  return true;
}

void ED_sculpt_redraw_planes_get(float planes[4][4], ARegion *region, Object *ob)
{
  PBVH *pbvh = ob->sculpt->pbvh;
  /* Copy here, original will be used below. */
  rcti rect = ob->sculpt->cache->current_r;

  sculpt_extend_redraw_rect_previous(ob, &rect);

  paint_calc_redraw_planes(planes, region, ob, &rect);

  /* We will draw this \a rect, so now we can set it as the previous partial \a rect.
   * Note that we don't update with the union of previous/current (\a rect), only with
   * the current. Thus we avoid the rectangle needlessly growing to include
   * all the stroke area. */
  ob->sculpt->cache->previous_r = ob->sculpt->cache->current_r;

  /* Clear redraw flag from nodes. */
  if (pbvh) {
    BKE_pbvh_update_bounds(pbvh, PBVH_UpdateRedraw);
  }
}

/************************ Brush Testing *******************/

void SCULPT_brush_test_init(SculptSession *ss, SculptBrushTest *test)
{
  RegionView3D *rv3d = ss->cache ? ss->cache->vc->rv3d : ss->rv3d;
  View3D *v3d = ss->cache ? ss->cache->vc->v3d : ss->v3d;

  test->radius_squared = ss->cache ? ss->cache->radius_squared :
                                     ss->cursor_radius * ss->cursor_radius;
  test->radius = sqrtf(test->radius_squared);

  if (ss->cache) {
    copy_v3_v3(test->location, ss->cache->location);
    test->mirror_symmetry_pass = ss->cache->mirror_symmetry_pass;
    test->radial_symmetry_pass = ss->cache->radial_symmetry_pass;
    copy_m4_m4(test->symm_rot_mat_inv, ss->cache->symm_rot_mat_inv);
  }
  else {
    copy_v3_v3(test->location, ss->cursor_location);
    test->mirror_symmetry_pass = 0;
    test->radial_symmetry_pass = 0;
    unit_m4(test->symm_rot_mat_inv);
  }

  /* Just for initialize. */
  test->dist = 0.0f;

  /* Only for 2D projection. */
  zero_v4(test->plane_view);
  zero_v4(test->plane_tool);

  if (RV3D_CLIPPING_ENABLED(v3d, rv3d)) {
    test->clip_rv3d = rv3d;
  }
  else {
    test->clip_rv3d = NULL;
  }
}

BLI_INLINE bool sculpt_brush_test_clipping(const SculptBrushTest *test, const float co[3])
{
  RegionView3D *rv3d = test->clip_rv3d;
  if (!rv3d) {
    return false;
  }
  float symm_co[3];
  flip_v3_v3(symm_co, co, test->mirror_symmetry_pass);
  if (test->radial_symmetry_pass) {
    mul_m4_v3(test->symm_rot_mat_inv, symm_co);
  }
  return ED_view3d_clipping_test(rv3d, symm_co, true);
}

bool SCULPT_brush_test_sphere(SculptBrushTest *test, const float co[3])
{
  float distsq = len_squared_v3v3(co, test->location);

  if (distsq > test->radius_squared) {
    return false;
  }

  if (sculpt_brush_test_clipping(test, co)) {
    return false;
  }

  test->dist = sqrtf(distsq);
  return true;
}

bool SCULPT_brush_test_sphere_sq(SculptBrushTest *test, const float co[3])
{
  float distsq = len_squared_v3v3(co, test->location);

  if (distsq > test->radius_squared) {
    return false;
  }
  if (sculpt_brush_test_clipping(test, co)) {
    return false;
  }
  test->dist = distsq;
  return true;
}

bool SCULPT_brush_test_sphere_fast(const SculptBrushTest *test, const float co[3])
{
  if (sculpt_brush_test_clipping(test, co)) {
    return false;
  }
  return len_squared_v3v3(co, test->location) <= test->radius_squared;
}

bool SCULPT_brush_test_circle_sq(SculptBrushTest *test, const float co[3])
{
  float co_proj[3];
  closest_to_plane_normalized_v3(co_proj, test->plane_view, co);
  float distsq = len_squared_v3v3(co_proj, test->location);

  if (distsq > test->radius_squared) {
    return false;
  }

  if (sculpt_brush_test_clipping(test, co)) {
    return false;
  }

  test->dist = distsq;
  return true;
}

bool SCULPT_brush_test_cube(SculptBrushTest *test,
                            const float co[3],
                            const float local[4][4],
                            const float roundness)
{
  float side = M_SQRT1_2;
  float local_co[3];

  if (sculpt_brush_test_clipping(test, co)) {
    return false;
  }

  mul_v3_m4v3(local_co, local, co);

  local_co[0] = fabsf(local_co[0]);
  local_co[1] = fabsf(local_co[1]);
  local_co[2] = fabsf(local_co[2]);

  /* Keep the square and circular brush tips the same size. */
  side += (1.0f - side) * roundness;

  const float hardness = 1.0f - roundness;
  const float constant_side = hardness * side;
  const float falloff_side = roundness * side;

  if (!(local_co[0] <= side && local_co[1] <= side && local_co[2] <= side)) {
    /* Outside the square. */
    return false;
  }
  if (min_ff(local_co[0], local_co[1]) > constant_side) {
    /* Corner, distance to the center of the corner circle. */
    float r_point[3];
    copy_v3_fl(r_point, constant_side);
    test->dist = len_v2v2(r_point, local_co) / falloff_side;
    return true;
  }
  if (max_ff(local_co[0], local_co[1]) > constant_side) {
    /* Side, distance to the square XY axis. */
    test->dist = (max_ff(local_co[0], local_co[1]) - constant_side) / falloff_side;
    return true;
  }

  /* Inside the square, constant distance. */
  test->dist = 0.0f;
  return true;
}

SculptBrushTestFn SCULPT_brush_test_init_with_falloff_shape(SculptSession *ss,
                                                            SculptBrushTest *test,
                                                            char falloff_shape)
{
  SCULPT_brush_test_init(ss, test);
  SculptBrushTestFn sculpt_brush_test_sq_fn;
  if (falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
    sculpt_brush_test_sq_fn = SCULPT_brush_test_sphere_sq;
  }
  else {
    /* PAINT_FALLOFF_SHAPE_TUBE */
    plane_from_point_normal_v3(test->plane_view, test->location, ss->cache->view_normal);
    sculpt_brush_test_sq_fn = SCULPT_brush_test_circle_sq;
  }
  return sculpt_brush_test_sq_fn;
}

const float *SCULPT_brush_frontface_normal_from_falloff_shape(SculptSession *ss,
                                                              char falloff_shape)
{
  if (falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
    return ss->cache->sculpt_normal_symm;
  }
  /* PAINT_FALLOFF_SHAPE_TUBE */
  return ss->cache->view_normal;
}

static float frontface(const Brush *br,
                       const float sculpt_normal[3],
                       const short no[3],
                       const float fno[3])
{
  if (!(br->flag & BRUSH_FRONTFACE)) {
    return 1.0f;
  }

  float dot;
  if (no) {
    float tmp[3];

    normal_short_to_float_v3(tmp, no);
    dot = dot_v3v3(tmp, sculpt_normal);
  }
  else {
    dot = dot_v3v3(fno, sculpt_normal);
  }
  return dot > 0.0f ? dot : 0.0f;
}

#if 0

static bool sculpt_brush_test_cyl(SculptBrushTest *test,
                                  float co[3],
                                  float location[3],
                                  const float area_no[3])
{
  if (sculpt_brush_test_sphere_fast(test, co)) {
    float t1[3], t2[3], t3[3], dist;

    sub_v3_v3v3(t1, location, co);
    sub_v3_v3v3(t2, x2, location);

    cross_v3_v3v3(t3, area_no, t1);

    dist = len_v3(t3) / len_v3(t2);

    test->dist = dist;

    return true;
  }

  return false;
}

#endif

/* ===== Sculpting =====
 */
static void flip_v3(float v[3], const ePaintSymmetryFlags symm)
{
  flip_v3_v3(v, v, symm);
}

static void flip_qt(float quat[4], const ePaintSymmetryFlags symm)
{
  flip_qt_qt(quat, quat, symm);
}

static float calc_overlap(StrokeCache *cache, const char symm, const char axis, const float angle)
{
  float mirror[3];
  float distsq;

  flip_v3_v3(mirror, cache->true_location, symm);

  if (axis != 0) {
    float mat[3][3];
    axis_angle_to_mat3_single(mat, axis, angle);
    mul_m3_v3(mat, mirror);
  }

  distsq = len_squared_v3v3(mirror, cache->true_location);

  if (distsq <= 4.0f * (cache->radius_squared)) {
    return (2.0f * (cache->radius) - sqrtf(distsq)) / (2.0f * (cache->radius));
  }
  return 0.0f;
}

static float calc_radial_symmetry_feather(Sculpt *sd,
                                          StrokeCache *cache,
                                          const char symm,
                                          const char axis)
{
  float overlap = 0.0f;

  for (int i = 1; i < sd->radial_symm[axis - 'X']; i++) {
    const float angle = 2.0f * M_PI * i / sd->radial_symm[axis - 'X'];
    overlap += calc_overlap(cache, symm, axis, angle);
  }

  return overlap;
}

static float calc_symmetry_feather(Sculpt *sd, StrokeCache *cache)
{
  if (!(sd->paint.symmetry_flags & PAINT_SYMMETRY_FEATHER)) {
    return 1.0f;
  }
  float overlap;
  const int symm = cache->symmetry;

  overlap = 0.0f;
  for (int i = 0; i <= symm; i++) {
    if (!SCULPT_is_symmetry_iteration_valid(i, symm)) {
      continue;
    }

    overlap += calc_overlap(cache, i, 0, 0);

    overlap += calc_radial_symmetry_feather(sd, cache, i, 'X');
    overlap += calc_radial_symmetry_feather(sd, cache, i, 'Y');
    overlap += calc_radial_symmetry_feather(sd, cache, i, 'Z');
  }
  return 1.0f / overlap;
}

/* -------------------------------------------------------------------- */
/** \name Calculate Normal and Center
 *
 * Calculate geometry surrounding the brush center.
 * (optionally using original coordinates).
 *
 * Functions are:
 * - #calc_area_center
 * - #calc_area_normal
 * - #calc_area_normal_and_center
 *
 * \note These are all _very_ similar, when changing one, check others.
 * \{ */

typedef struct AreaNormalCenterTLSData {
  /* 0 = towards view, 1 = flipped */
  float area_cos[2][3];
  float area_nos[2][3];
  int count_no[2];
  int count_co[2];
} AreaNormalCenterTLSData;

static void calc_area_normal_and_center_task_cb(void *__restrict userdata,
                                                const int n,
                                                const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  AreaNormalCenterTLSData *anctd = tls->userdata_chunk;
  const bool use_area_nos = data->use_area_nos;
  const bool use_area_cos = data->use_area_cos;

  PBVHVertexIter vd;
  SculptUndoNode *unode = NULL;

  bool use_original = false;
  bool normal_test_r, area_test_r;

  if (ss->cache && ss->cache->original) {
    unode = SCULPT_undo_push_node(data->ob, data->nodes[n], SCULPT_UNDO_COORDS);
    use_original = (unode->co || unode->bm_entry);
  }

  SculptBrushTest normal_test;
  SculptBrushTestFn sculpt_brush_normal_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &normal_test, data->brush->falloff_shape);

  /* Update the test radius to sample the normal using the normal radius of the brush. */
  if (data->brush->ob_mode == OB_MODE_SCULPT) {
    float test_radius = sqrtf(normal_test.radius_squared);
    test_radius *= data->brush->normal_radius_factor;
    normal_test.radius = test_radius;
    normal_test.radius_squared = test_radius * test_radius;
  }

  SculptBrushTest area_test;
  SculptBrushTestFn sculpt_brush_area_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &area_test, data->brush->falloff_shape);

  if (data->brush->ob_mode == OB_MODE_SCULPT) {
    float test_radius = sqrtf(area_test.radius_squared);
    /* Layer brush produces artifacts with normal and area radius */
    /* Enable area radius control only on Scrape for now */
    if (ELEM(data->brush->sculpt_tool, SCULPT_TOOL_SCRAPE, SCULPT_TOOL_FILL) &&
        data->brush->area_radius_factor > 0.0f) {
      test_radius *= data->brush->area_radius_factor;
      if (ss->cache && data->brush->flag2 & BRUSH_AREA_RADIUS_PRESSURE) {
        test_radius *= ss->cache->pressure;
      }
    }
    else {
      test_radius *= data->brush->normal_radius_factor;
    }
    area_test.radius = test_radius;
    area_test.radius_squared = test_radius * test_radius;
  }

  /* When the mesh is edited we can't rely on original coords
   * (original mesh may not even have verts in brush radius). */
  if (use_original && data->has_bm_orco) {
    PBVHTriBuf *tribuf = BKE_pbvh_bmesh_get_tris(ss->pbvh, data->nodes[n]);

    for (int i = 0; i < tribuf->tottri; i++) {
      PBVHTri *tri = tribuf->tris + i;
      SculptVertRef v1 = tribuf->verts[tri->v[0]];
      SculptVertRef v2 = tribuf->verts[tri->v[1]];
      SculptVertRef v3 = tribuf->verts[tri->v[2]];

      const float(*co_tri[3]) = {
          SCULPT_vertex_origco_get(ss, v1),
          SCULPT_vertex_origco_get(ss, v2),
          SCULPT_vertex_origco_get(ss, v3),
      };
      float co[3];

      closest_on_tri_to_point_v3(co, normal_test.location, UNPACK3(co_tri));

      normal_test_r = sculpt_brush_normal_test_sq_fn(&normal_test, co);
      area_test_r = sculpt_brush_area_test_sq_fn(&area_test, co);

      if (!normal_test_r && !area_test_r) {
        continue;
      }

      float no[3];
      int flip_index;

      normal_tri_v3(no, UNPACK3(co_tri));

      flip_index = (dot_v3v3(ss->cache->view_normal, no) <= 0.0f);
      if (use_area_cos && area_test_r) {
        /* Weight the coordinates towards the center. */
        float p = 1.0f - (sqrtf(area_test.dist) / area_test.radius);
        const float afactor = clamp_f(3.0f * p * p - 2.0f * p * p * p, 0.0f, 1.0f);

        float disp[3];
        sub_v3_v3v3(disp, co, area_test.location);
        mul_v3_fl(disp, 1.0f - afactor);
        add_v3_v3v3(co, area_test.location, disp);
        add_v3_v3(anctd->area_cos[flip_index], co);

        anctd->count_co[flip_index] += 1;
      }
      if (use_area_nos && normal_test_r) {
        /* Weight the normals towards the center. */
        float p = 1.0f - (sqrtf(normal_test.dist) / normal_test.radius);
        const float nfactor = clamp_f(3.0f * p * p - 2.0f * p * p * p, 0.0f, 1.0f);
        mul_v3_fl(no, nfactor);

        add_v3_v3(anctd->area_nos[flip_index], no);
        anctd->count_no[flip_index] += 1;
      }
    }
  }
  else {
    BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
      float co[3];

      /* For bm_vert only. */
      short no_s[3];

      if (use_original) {
        if (unode->bm_entry) {
          BMVert *v = vd.bm_vert;
          MDynTopoVert *mv = BKE_PBVH_DYNVERT(vd.cd_dyn_vert, v);

          normal_float_to_short_v3(no_s, mv->origno);
          copy_v3_v3(co, mv->origco);
        }
        else {
          copy_v3_v3(co, unode->co[vd.i]);
          copy_v3_v3_short(no_s, unode->no[vd.i]);
        }
      }
      else {
        copy_v3_v3(co, vd.co);
      }

      normal_test_r = sculpt_brush_normal_test_sq_fn(&normal_test, co);
      area_test_r = sculpt_brush_area_test_sq_fn(&area_test, co);

      if (!normal_test_r && !area_test_r) {
        continue;
      }

      float no[3];
      int flip_index;

      data->any_vertex_sampled = true;

      if (use_original) {
        normal_short_to_float_v3(no, no_s);
      }
      else {
        if (vd.no) {
          normal_short_to_float_v3(no, vd.no);
        }
        else {
          copy_v3_v3(no, vd.fno);
        }
      }

      flip_index = (dot_v3v3(ss->cache ? ss->cache->view_normal : ss->cursor_view_normal, no) <=
                    0.0f);

      if (use_area_cos && area_test_r) {
        /* Weight the coordinates towards the center. */
        float p = 1.0f - (sqrtf(area_test.dist) / area_test.radius);
        const float afactor = clamp_f(3.0f * p * p - 2.0f * p * p * p, 0.0f, 1.0f);

        float disp[3];
        sub_v3_v3v3(disp, co, area_test.location);
        mul_v3_fl(disp, 1.0f - afactor);
        add_v3_v3v3(co, area_test.location, disp);

        add_v3_v3(anctd->area_cos[flip_index], co);
        anctd->count_co[flip_index] += 1;
      }
      if (use_area_nos && normal_test_r) {
        /* Weight the normals towards the center. */
        float p = 1.0f - (sqrtf(normal_test.dist) / normal_test.radius);
        const float nfactor = clamp_f(3.0f * p * p - 2.0f * p * p * p, 0.0f, 1.0f);
        mul_v3_fl(no, nfactor);

        add_v3_v3(anctd->area_nos[flip_index], no);
        anctd->count_no[flip_index] += 1;
      }
    }
    BKE_pbvh_vertex_iter_end;
  }
}

static void calc_area_normal_and_center_reduce(const void *__restrict UNUSED(userdata),
                                               void *__restrict chunk_join,
                                               void *__restrict chunk)
{
  AreaNormalCenterTLSData *join = chunk_join;
  AreaNormalCenterTLSData *anctd = chunk;

  /* For flatten center. */
  add_v3_v3(join->area_cos[0], anctd->area_cos[0]);
  add_v3_v3(join->area_cos[1], anctd->area_cos[1]);

  /* For area normal. */
  add_v3_v3(join->area_nos[0], anctd->area_nos[0]);
  add_v3_v3(join->area_nos[1], anctd->area_nos[1]);

  /* Weights. */
  add_v2_v2_int(join->count_no, anctd->count_no);
  add_v2_v2_int(join->count_co, anctd->count_co);
}

static void calc_area_center(
    Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float r_area_co[3])
{
  SculptSession *ss = ob->sculpt;
  const Brush *brush = ss->cache ? ss->cache->brush : BKE_paint_brush(&sd->paint);
  const bool has_bm_orco = ss->bm && SCULPT_stroke_is_dynamic_topology(ss, brush);
  int n;

  /* Intentionally set 'sd' to NULL since we share logic with vertex paint. */
  SculptThreadedTaskData data = {
      .sd = NULL,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .totnode = totnode,
      .has_bm_orco = has_bm_orco,
      .use_area_cos = true,
  };

  AreaNormalCenterTLSData anctd = {{{0}}};

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  settings.func_reduce = calc_area_normal_and_center_reduce;
  settings.userdata_chunk = &anctd;
  settings.userdata_chunk_size = sizeof(AreaNormalCenterTLSData);
  BLI_task_parallel_range(0, totnode, &data, calc_area_normal_and_center_task_cb, &settings);

  /* For flatten center. */
  for (n = 0; n < ARRAY_SIZE(anctd.area_cos); n++) {
    if (anctd.count_co[n] == 0) {
      continue;
    }

    mul_v3_v3fl(r_area_co, anctd.area_cos[n], 1.0f / anctd.count_co[n]);
    break;
  }

  if (n == 2) {
    zero_v3(r_area_co);
  }

  if (anctd.count_co[0] == 0 && anctd.count_co[1] == 0) {
    if (ss->cache) {
      copy_v3_v3(r_area_co, ss->cache->location);
    }
  }
}

void SCULPT_calc_area_normal(
    Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float r_area_no[3])
{
  SculptSession *ss = ob->sculpt;

  const Brush *brush = ss->cache ? ss->cache->brush : BKE_paint_brush(&sd->paint);
  SCULPT_pbvh_calc_area_normal(brush, ob, nodes, totnode, true, r_area_no);
}

/* Expose 'calc_area_normal' externally. */
bool SCULPT_pbvh_calc_area_normal(const Brush *brush,
                                  Object *ob,
                                  PBVHNode **nodes,
                                  int totnode,
                                  bool use_threading,
                                  float r_area_no[3])
{
  SculptSession *ss = ob->sculpt;
  const bool has_bm_orco = ss->bm && SCULPT_stroke_is_dynamic_topology(ss, brush);

  /* Intentionally set 'sd' to NULL since this is used for vertex paint too. */
  SculptThreadedTaskData data = {
      .sd = NULL,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .totnode = totnode,
      .has_bm_orco = has_bm_orco,
      .use_area_nos = true,
      .any_vertex_sampled = false,
  };

  AreaNormalCenterTLSData anctd = {{{0}}};

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, use_threading, totnode);
  settings.func_reduce = calc_area_normal_and_center_reduce;
  settings.userdata_chunk = &anctd;
  settings.userdata_chunk_size = sizeof(AreaNormalCenterTLSData);
  BLI_task_parallel_range(0, totnode, &data, calc_area_normal_and_center_task_cb, &settings);

  /* For area normal. */
  for (int i = 0; i < ARRAY_SIZE(anctd.area_nos); i++) {
    if (normalize_v3_v3(r_area_no, anctd.area_nos[i]) != 0.0f) {
      break;
    }
  }

  return data.any_vertex_sampled;
}

/* This calculates flatten center and area normal together,
 * amortizing the memory bandwidth and loop overhead to calculate both at the same time. */
static void calc_area_normal_and_center(
    Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float r_area_no[3], float r_area_co[3])
{
  SculptSession *ss = ob->sculpt;
  const Brush *brush = ss->cache ? ss->cache->brush : BKE_paint_brush(&sd->paint);
  const bool has_bm_orco = ss->bm && SCULPT_stroke_is_dynamic_topology(ss, brush);
  int n;

  /* Intentionally set 'sd' to NULL since this is used for vertex paint too. */
  SculptThreadedTaskData data = {
      .sd = NULL,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .totnode = totnode,
      .has_bm_orco = has_bm_orco,
      .use_area_cos = true,
      .use_area_nos = true,
  };

  AreaNormalCenterTLSData anctd = {{{0}}};

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  settings.func_reduce = calc_area_normal_and_center_reduce;
  settings.userdata_chunk = &anctd;
  settings.userdata_chunk_size = sizeof(AreaNormalCenterTLSData);
  BLI_task_parallel_range(0, totnode, &data, calc_area_normal_and_center_task_cb, &settings);

  /* For flatten center. */
  for (n = 0; n < ARRAY_SIZE(anctd.area_cos); n++) {
    if (anctd.count_co[n] == 0) {
      continue;
    }

    mul_v3_v3fl(r_area_co, anctd.area_cos[n], 1.0f / anctd.count_co[n]);
    break;
  }

  if (n == 2) {
    zero_v3(r_area_co);
  }

  if (anctd.count_co[0] == 0 && anctd.count_co[1] == 0) {
    if (ss->cache) {
      copy_v3_v3(r_area_co, ss->cache->location);
    }
  }

  /* For area normal. */
  for (n = 0; n < ARRAY_SIZE(anctd.area_nos); n++) {
    if (normalize_v3_v3(r_area_no, anctd.area_nos[n]) != 0.0f) {
      break;
    }
  }
}

/** \} */

/**
 * Return modified brush strength. Includes the direction of the brush, positive
 * values pull vertices, negative values push. Uses tablet pressure and a
 * special multiplier found experimentally to scale the strength factor.
 */
static float brush_strength(const Sculpt *sd,
                            const StrokeCache *cache,
                            const float feather,
                            const UnifiedPaintSettings *ups)
{
  const Scene *scene = cache->vc->scene;
  const Brush *brush = cache->brush;  // BKE_paint_brush((Paint *)&sd->paint);

  /* Primary strength input; square it to make lower values more sensitive. */
  const float root_alpha = brush->alpha;  // BKE_brush_alpha_get(scene, brush);
  const float alpha = root_alpha * root_alpha;
  const float dir = (brush->flag & BRUSH_DIR_IN) ? -1.0f : 1.0f;
  const float pen_flip = cache->pen_flip ? -1.0f : 1.0f;
  const float invert = cache->invert ? -1.0f : 1.0f;
  float overlap = ups->overlap_factor;
  /* Spacing is integer percentage of radius, divide by 50 to get
   * normalized diameter. */

  float flip = dir * invert * pen_flip;
  if (brush->flag & BRUSH_INVERT_TO_SCRAPE_FILL) {
    flip = 1.0f;
  }

  // float pressure = BKE_brush_use_alpha_pressure(brush) ? cache->pressure : 1.0f;
  float pressure = 1.0f;

  /* Pressure final value after being tweaked depending on the brush. */
  float final_pressure = pressure;

  switch (brush->sculpt_tool) {
    case SCULPT_TOOL_CLAY:
      // final_pressure = pow4f(pressure);
      overlap = (1.0f + overlap) / 2.0f;
      return 0.25f * alpha * flip * pressure * overlap * feather;
    case SCULPT_TOOL_DRAW:
    case SCULPT_TOOL_DRAW_SHARP:
    case SCULPT_TOOL_LAYER:
    case SCULPT_TOOL_SYMMETRIZE:
      return alpha * flip * pressure * overlap * feather;
    case SCULPT_TOOL_DISPLACEMENT_ERASER:
      return alpha * pressure * overlap * feather;
    case SCULPT_TOOL_FAIRING:
    case SCULPT_TOOL_SCENE_PROJECT:
      return alpha * pressure * overlap * feather;
    case SCULPT_TOOL_CLOTH:
      if (brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_GRAB) {
        /* Grab deform uses the same falloff as a regular grab brush. */
        return root_alpha * feather;
      }
      else if (brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_SNAKE_HOOK) {
        return root_alpha * feather * pressure * overlap;
      }
      else if (brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_EXPAND) {
        /* Expand is more sensible to strength as it keeps expanding the cloth when sculpting
         * over the same vertices. */
        return 0.1f * alpha * flip * pressure * overlap * feather;
      }
      else {
        /* Multiply by 10 by default to get a larger range of strength depending on the size of
         * the brush and object. */
        return 10.0f * alpha * flip * pressure * overlap * feather;
      }
    case SCULPT_TOOL_DRAW_FACE_SETS:
      return alpha * pressure * overlap * feather;
    case SCULPT_TOOL_SLIDE_RELAX:
      return alpha * pressure * overlap * feather * 2.0f;
    case SCULPT_TOOL_PAINT:
      final_pressure = pressure * pressure;
      return alpha * final_pressure * overlap * feather;
    case SCULPT_TOOL_SMEAR:
    case SCULPT_TOOL_DISPLACEMENT_SMEAR:
      return alpha * pressure * overlap * feather;
    case SCULPT_TOOL_CLAY_STRIPS:
      /* Clay Strips needs less strength to compensate the curve. */
      // final_pressure = powf(pressure, 1.5f);
      return alpha * flip * pressure * overlap * feather * 0.3f;
    case SCULPT_TOOL_TWIST:
      return alpha * flip * pressure * overlap * feather * 0.3f;
    case SCULPT_TOOL_CLAY_THUMB:
      // final_pressure = pressure * pressure;
      return alpha * flip * pressure * overlap * feather * 1.3f;

    case SCULPT_TOOL_MASK:
      overlap = (1.0f + overlap) / 2.0f;
      switch ((BrushMaskTool)brush->mask_tool) {
        case BRUSH_MASK_DRAW:
          return alpha * flip * pressure * overlap * feather;
        case BRUSH_MASK_SMOOTH:
          return alpha * pressure * feather;
      }
      BLI_assert_msg(0, "Not supposed to happen");
      return 0.0f;

    case SCULPT_TOOL_CREASE:
    case SCULPT_TOOL_BLOB:
      return alpha * flip * pressure * overlap * feather;

    case SCULPT_TOOL_INFLATE:
      if (flip > 0.0f) {
        return 0.250f * alpha * flip * pressure * overlap * feather;
      }
      else {
        return 0.125f * alpha * flip * pressure * overlap * feather;
      }

    case SCULPT_TOOL_MULTIPLANE_SCRAPE:
      overlap = (1.0f + overlap) / 2.0f;
      return alpha * flip * pressure * overlap * feather;

    case SCULPT_TOOL_FILL:
    case SCULPT_TOOL_SCRAPE:
    case SCULPT_TOOL_FLATTEN:
      if (flip > 0.0f) {
        overlap = (1.0f + overlap) / 2.0f;
        return alpha * flip * pressure * overlap * feather;
      }
      else {
        /* Reduce strength for DEEPEN, PEAKS, and CONTRAST. */
        return 0.5f * alpha * flip * pressure * overlap * feather;
      }

    case SCULPT_TOOL_SMOOTH: {
      const float smooth_strength_base = flip * pressure * feather;
      // if (cache->alt_smooth) {
      //  return smooth_strength_base * sd->smooth_strength_factor;
      //}
      return smooth_strength_base * alpha;
    }

    case SCULPT_TOOL_VCOL_BOUNDARY:
      return flip * alpha * pressure * feather;
    case SCULPT_TOOL_UV_SMOOTH:
      return flip * alpha * pressure * feather;
    case SCULPT_TOOL_PINCH:
      if (flip > 0.0f) {
        return alpha * flip * pressure * overlap * feather;
      }
      else {
        return 0.25f * alpha * flip * pressure * overlap * feather;
      }

    case SCULPT_TOOL_NUDGE:
      overlap = (1.0f + overlap) / 2.0f;
      return alpha * pressure * overlap * feather;

    case SCULPT_TOOL_THUMB:
      return alpha * pressure * feather;

    case SCULPT_TOOL_SNAKE_HOOK:
      return root_alpha * feather;

    case SCULPT_TOOL_GRAB:
      return root_alpha * feather;

    case SCULPT_TOOL_ARRAY:
      // return root_alpha * feather;
      return alpha * pressure;

    case SCULPT_TOOL_ROTATE:
      return alpha * pressure * feather;

    case SCULPT_TOOL_ELASTIC_DEFORM:
    case SCULPT_TOOL_POSE:
    case SCULPT_TOOL_BOUNDARY:
      return root_alpha * feather;
    case SCULPT_TOOL_TOPOLOGY_RAKE:
      return root_alpha;
    default:
      return alpha * flip * overlap * feather;
      ;
  }
}

/* Return a multiplier for brush strength on a particular vertex. */
float SCULPT_brush_strength_factor(SculptSession *ss,
                                   const Brush *br,
                                   const float brush_point[3],
                                   const float len,
                                   const short vno[3],
                                   const float fno[3],
                                   const float mask,
                                   const SculptVertRef vertex_index,
                                   const int thread_id)
{
  StrokeCache *cache = ss->cache;
  const Scene *scene = cache->vc->scene;
  const MTex *mtex = &br->mtex;
  float avg = 1.0f;
  float rgba[4];
  float point[3];

  sub_v3_v3v3(point, brush_point, cache->plane_offset);

  if (!mtex->tex) {
    avg = 1.0f;
  }
  else if (mtex->brush_map_mode == MTEX_MAP_MODE_3D) {
    /* Get strength by feeding the vertex location directly into a texture. */
    avg = BKE_brush_sample_tex_3d(scene, br, point, rgba, 0, ss->tex_pool);
  }
  else if (ss->texcache) {
    float symm_point[3], point_2d[2];
    /* Quite warnings. */
    float x = 0.0f, y = 0.0f;

    /* If the active area is being applied for symmetry, flip it
     * across the symmetry axis and rotate it back to the original
     * position in order to project it. This insures that the
     * brush texture will be oriented correctly. */
    if (cache->radial_symmetry_pass) {
      mul_m4_v3(cache->symm_rot_mat_inv, point);
    }
    flip_v3_v3(symm_point, point, cache->mirror_symmetry_pass);

    ED_view3d_project_float_v2_m4(cache->vc->region, symm_point, point_2d, cache->projection_mat);

    /* Still no symmetry supported for other paint modes.
     * Sculpt does it DIY. */
    if (mtex->brush_map_mode == MTEX_MAP_MODE_AREA) {
      /* Similar to fixed mode, but projects from brush angle
       * rather than view direction. */

      mul_m4_v3(cache->brush_local_mat, symm_point);

      x = symm_point[0];
      y = symm_point[1];

      x *= br->mtex.size[0];
      y *= br->mtex.size[1];

      x += br->mtex.ofs[0];
      y += br->mtex.ofs[1];

      avg = paint_get_tex_pixel(&br->mtex, x, y, ss->tex_pool, thread_id);

      avg += br->texture_sample_bias;
    }
    else {
      const float point_3d[3] = {point_2d[0], point_2d[1], 0.0f};
      avg = BKE_brush_sample_tex_3d(scene, br, point_3d, rgba, 0, ss->tex_pool);
    }
  }

  /* Hardness. */
  float final_len = len;
  const float hardness = cache->paint_brush.hardness;
  float p = len / cache->radius;
  if (p < hardness) {
    final_len = 0.0f;
  }
  else if (hardness == 1.0f) {
    final_len = cache->radius;
  }
  else {
    p = (p - hardness) / (1.0f - hardness);
    final_len = p * cache->radius;
  }

  /* Falloff curve. */
  avg *= BKE_brush_curve_strength(br, final_len, cache->radius);
  avg *= frontface(br, cache->view_normal, vno, fno);

  /* Paint mask. */
  avg *= 1.0f - mask;

  /* Auto-masking. */
  avg *= SCULPT_automasking_factor_get(cache->automasking, ss, vertex_index);

  return avg;
}

/* Test AABB against sphere. */
ATTR_NO_OPT bool SCULPT_search_sphere_cb(PBVHNode *node, void *data_v)
{
  SculptSearchSphereData *data = data_v;
  const float *center;
  float nearest[3];
  if (data->center) {
    center = data->center;
  }
  else {
    center = data->ss->cache ? data->ss->cache->location : data->ss->cursor_location;
  }
  float t[3], bb_min[3], bb_max[3];

  if (data->ignore_fully_ineffective) {
    if (BKE_pbvh_node_fully_hidden_get(node)) {
      return false;
    }
    if (BKE_pbvh_node_fully_masked_get(node)) {
      return false;
    }
  }

  if (data->original) {
    BKE_pbvh_node_get_original_BB(node, bb_min, bb_max);
  }
  else {
    BKE_pbvh_node_get_BB(node, bb_min, bb_max);
  }

  for (int i = 0; i < 3; i++) {
    if (bb_min[i] > center[i]) {
      nearest[i] = bb_min[i];
    }
    else if (bb_max[i] < center[i]) {
      nearest[i] = bb_max[i];
    }
    else {
      nearest[i] = center[i];
    }
  }

  sub_v3_v3v3(t, center, nearest);

  return len_squared_v3(t) < data->radius_squared;
}

/* 2D projection (distance to line). */
bool SCULPT_search_circle_cb(PBVHNode *node, void *data_v)
{
  SculptSearchCircleData *data = data_v;
  float bb_min[3], bb_max[3];

  if (data->ignore_fully_ineffective) {
    if (BKE_pbvh_node_fully_masked_get(node)) {
      return false;
    }
  }

  if (data->original) {
    BKE_pbvh_node_get_original_BB(node, bb_min, bb_max);
  }
  else {
    BKE_pbvh_node_get_BB(node, bb_min, bb_min);
  }

  float dummy_co[3], dummy_depth;
  const float dist_sq = dist_squared_ray_to_aabb_v3(
      data->dist_ray_to_aabb_precalc, bb_min, bb_max, dummy_co, &dummy_depth);

  /* Seems like debug code.
   * Maybe this function can just return true if the node is not fully masked. */
  return dist_sq < data->radius_squared || true;
}

/**
 * Handles clipping against a mirror modifier and #SCULPT_LOCK_X/Y/Z axis flags.
 */
void SCULPT_clip(Sculpt *sd, SculptSession *ss, float co[3], const float val[3])
{
  for (int i = 0; i < 3; i++) {
    if (sd->flags & (SCULPT_LOCK_X << i)) {
      continue;
    }

    if (ss->cache && (ss->cache->flag & (CLIP_X << i)) &&
        (fabsf(co[i]) <= ss->cache->clip_tolerance[i])) {
      co[i] = 0.0f;
    }
    else {
      co[i] = val[i];
    }
  }
}

static PBVHNode **sculpt_pbvh_gather_cursor_update(Object *ob,
                                                   Sculpt *sd,
                                                   bool use_original,
                                                   int *r_totnode)
{
  SculptSession *ss = ob->sculpt;
  PBVHNode **nodes = NULL;
  SculptSearchSphereData data = {
      .ss = ss,
      .sd = sd,
      .radius_squared = ss->cursor_radius,
      .original = use_original,
      .ignore_fully_ineffective = false,
      .center = NULL,
  };
  BKE_pbvh_search_gather(ss->pbvh, SCULPT_search_sphere_cb, &data, &nodes, r_totnode);
  return nodes;
}

static PBVHNode **sculpt_pbvh_gather_generic(Object *ob,
                                             Sculpt *sd,
                                             const Brush *brush,
                                             bool use_original,
                                             float radius_scale,
                                             int *r_totnode)
{
  SculptSession *ss = ob->sculpt;
  PBVHNode **nodes = NULL;

  /* Build a list of all nodes that are potentially within the cursor or brush's area of
   * influence.
   */
  if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) {
    SculptSearchSphereData data = {
        .ss = ss,
        .sd = sd,
        .radius_squared = square_f(ss->cache->radius * radius_scale),
        .original = use_original,
        .ignore_fully_ineffective = brush->sculpt_tool != SCULPT_TOOL_MASK,
        .center = NULL,
    };
    BKE_pbvh_search_gather(ss->pbvh, SCULPT_search_sphere_cb, &data, &nodes, r_totnode);
  }
  else {
    struct DistRayAABB_Precalc dist_ray_to_aabb_precalc;
    dist_squared_ray_to_aabb_v3_precalc(
        &dist_ray_to_aabb_precalc, ss->cache->location, ss->cache->view_normal);
    SculptSearchCircleData data = {
        .ss = ss,
        .sd = sd,
        .radius_squared = ss->cache ? square_f(ss->cache->radius * radius_scale) :
                                      ss->cursor_radius,
        .original = use_original,
        .dist_ray_to_aabb_precalc = &dist_ray_to_aabb_precalc,
        .ignore_fully_ineffective = brush->sculpt_tool != SCULPT_TOOL_MASK,
    };
    BKE_pbvh_search_gather(ss->pbvh, SCULPT_search_circle_cb, &data, &nodes, r_totnode);
  }
  return nodes;
}

/* Calculate primary direction of movement for many brushes. */
static void calc_sculpt_normal(
    Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float r_area_no[3])
{
  const SculptSession *ss = ob->sculpt;
  const Brush *brush = ss->cache ? ss->cache->brush : BKE_paint_brush(&sd->paint);

  switch (brush->sculpt_plane) {
    case SCULPT_DISP_DIR_VIEW:
      copy_v3_v3(r_area_no, ss->cache->true_view_normal);
      break;

    case SCULPT_DISP_DIR_X:
      ARRAY_SET_ITEMS(r_area_no, 1.0f, 0.0f, 0.0f);
      break;

    case SCULPT_DISP_DIR_Y:
      ARRAY_SET_ITEMS(r_area_no, 0.0f, 1.0f, 0.0f);
      break;

    case SCULPT_DISP_DIR_Z:
      ARRAY_SET_ITEMS(r_area_no, 0.0f, 0.0f, 1.0f);
      break;

    case SCULPT_DISP_DIR_AREA:
      SCULPT_calc_area_normal(sd, ob, nodes, totnode, r_area_no);
      break;

    default:
      break;
  }
}

static void update_sculpt_normal(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  StrokeCache *cache = ob->sculpt->cache;
  const Brush *brush = cache->brush;  // BKE_paint_brush(&sd->paint);
  /* Grab brush does not update the sculpt normal during a stroke. */
  const bool update_normal =
      !(brush->flag & BRUSH_ORIGINAL_NORMAL) && !(brush->sculpt_tool == SCULPT_TOOL_GRAB) &&
      !(brush->sculpt_tool == SCULPT_TOOL_THUMB && !(brush->flag & BRUSH_ANCHORED)) &&
      !(brush->sculpt_tool == SCULPT_TOOL_ELASTIC_DEFORM) &&
      !(brush->sculpt_tool == SCULPT_TOOL_SNAKE_HOOK && cache->normal_weight > 0.0f);

  if (cache->mirror_symmetry_pass == 0 && cache->radial_symmetry_pass == 0 &&
      (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(cache) || update_normal)) {
    calc_sculpt_normal(sd, ob, nodes, totnode, cache->sculpt_normal);
    if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
      project_plane_v3_v3v3(cache->sculpt_normal, cache->sculpt_normal, cache->view_normal);
      normalize_v3(cache->sculpt_normal);
    }
    copy_v3_v3(cache->sculpt_normal_symm, cache->sculpt_normal);
  }
  else {
    copy_v3_v3(cache->sculpt_normal_symm, cache->sculpt_normal);
    flip_v3(cache->sculpt_normal_symm, cache->mirror_symmetry_pass);
    mul_m4_v3(cache->symm_rot_mat, cache->sculpt_normal_symm);
  }
}

static void calc_local_y(ViewContext *vc, const float center[3], float y[3])
{
  Object *ob = vc->obact;
  float loc[3], mval_f[2] = {0.0f, 1.0f};
  float zfac;

  mul_v3_m4v3(loc, ob->imat, center);
  zfac = ED_view3d_calc_zfac(vc->rv3d, loc, NULL);

  ED_view3d_win_to_delta(vc->region, mval_f, y, zfac);
  normalize_v3(y);

  add_v3_v3(y, ob->loc);
  mul_m4_v3(ob->imat, y);
}

static void calc_brush_local_mat(const Brush *brush, Object *ob, float local_mat[4][4])
{
  const StrokeCache *cache = ob->sculpt->cache;
  float tmat[4][4];
  float mat[4][4];
  float scale[4][4];
  float angle, v[3];
  float up[3];

  /* Ensure `ob->imat` is up to date. */
  invert_m4_m4(ob->imat, ob->obmat);

  /* Initialize last column of matrix. */
  mat[0][3] = 0.0f;
  mat[1][3] = 0.0f;
  mat[2][3] = 0.0f;
  mat[3][3] = 1.0f;

  /* Get view's up vector in object-space. */
  calc_local_y(cache->vc, cache->location, up);

  /* Calculate the X axis of the local matrix. */
  cross_v3_v3v3(v, up, cache->sculpt_normal);
  /* Apply rotation (user angle, rake, etc.) to X axis. */
  angle = brush->mtex.rot - cache->special_rotation;
  rotate_v3_v3v3fl(mat[0], v, cache->sculpt_normal, angle);

  /* Get other axes. */
  cross_v3_v3v3(mat[1], cache->sculpt_normal, mat[0]);
  copy_v3_v3(mat[2], cache->sculpt_normal);

  /* Set location. */
  copy_v3_v3(mat[3], cache->location);

  /* Scale by brush radius. */
  normalize_m4(mat);
  scale_m4_fl(scale, cache->radius);
  mul_m4_m4m4(tmat, mat, scale);

  /* Return inverse (for converting from model-space coords to local area coords). */
  invert_m4_m4(local_mat, tmat);
}

#define SCULPT_TILT_SENSITIVITY 0.7f
void SCULPT_tilt_apply_to_normal(float r_normal[3], StrokeCache *cache, const float tilt_strength)
{
  if (!U.experimental.use_sculpt_tools_tilt) {
    return;
  }
  const float rot_max = M_PI_2 * tilt_strength * SCULPT_TILT_SENSITIVITY;
  mul_v3_mat3_m4v3(r_normal, cache->vc->obact->obmat, r_normal);
  float normal_tilt_y[3];
  rotate_v3_v3v3fl(normal_tilt_y, r_normal, cache->vc->rv3d->viewinv[0], cache->y_tilt * rot_max);
  float normal_tilt_xy[3];
  rotate_v3_v3v3fl(
      normal_tilt_xy, normal_tilt_y, cache->vc->rv3d->viewinv[1], cache->x_tilt * rot_max);
  mul_v3_mat3_m4v3(r_normal, cache->vc->obact->imat, normal_tilt_xy);
  normalize_v3(r_normal);
}

void SCULPT_tilt_effective_normal_get(const SculptSession *ss, const Brush *brush, float r_no[3])
{
  copy_v3_v3(r_no, ss->cache->sculpt_normal_symm);
  SCULPT_tilt_apply_to_normal(r_no, ss->cache, brush->tilt_strength_factor);
}

static void update_brush_local_mat(Sculpt *sd, Object *ob)
{
  StrokeCache *cache = ob->sculpt->cache;

  if (cache->mirror_symmetry_pass == 0 && cache->radial_symmetry_pass == 0) {
    calc_brush_local_mat(cache->brush, ob, cache->brush_local_mat);
  }
}

typedef struct {
  SculptSession *ss;
  const float *ray_start;
  const float *ray_normal;
  bool hit;
  int hit_count;
  bool back_hit;
  float depth;
  bool original;

  /* Depth of the second raycast hit. */
  float back_depth;

  /* When the back depth is not needed, this can be set to false to avoid traversing unnecesary
   * nodes. */
  bool use_back_depth;

  SculptVertRef active_vertex_index;
  float *face_normal;

  SculptFaceRef active_face_grid_index;

  struct IsectRayPrecalc isect_precalc;
} SculptRaycastData;

typedef struct {
  SculptSession *ss;
  const float *ray_start, *ray_normal;
  bool hit;
  float depth;
  float dist_sq_to_ray;
  bool original;
} SculptFindNearestToRayData;

static void do_topology_rake_bmesh_task_cb_ex(void *__restrict userdata,
                                              const int n,
                                              const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  Sculpt *sd = data->sd;
  const Brush *brush = data->brush;
  PBVHNode *node = data->nodes[n];

  float direction[3];
  copy_v3_v3(direction, ss->cache->grab_delta_symmetry);

  float tmp[3];
  mul_v3_v3fl(
      tmp, ss->cache->sculpt_normal_symm, dot_v3v3(ss->cache->sculpt_normal_symm, direction));
  sub_v3_v3(direction, tmp);
  normalize_v3(direction);

  /* Cancel if there's no grab data. */
  if (is_zero_v3(direction)) {
    return;
  }

  const float bstrength = clamp_f(data->strength, 0.0f, 1.0f);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  const bool use_curvature = data->use_curvature;
  int check_fsets = ss->cache->brush->flag2 & BRUSH_SMOOTH_PRESERVE_FACE_SETS;
  check_fsets = check_fsets ? SCULPT_BOUNDARY_FACE_SET : 0;

  if (use_curvature) {
    SCULPT_curvature_begin(ss, node, false);
  }

  const bool weighted = ss->cache->brush->flag2 & BRUSH_SMOOTH_USE_AREA_WEIGHT;
  if (weighted || ss->cache->brush->boundary_smooth_factor > 0.0f) {
    BKE_pbvh_check_tri_areas(ss->pbvh, data->nodes[n]);
  }

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

/* ignore boundary verts
  might want to call normal smooth with
  rake's projection in this case, I'm not entirely sure
  - joeedh
*/
#if 0
    if (have_bmesh) {
      BMVert *v = (BMVert *)vd.vertex.i;
      MDynTopoVert *mv = BKE_PBVH_DYNVERT(ss->cd_dyn_vert, v);

       if (mv->flag & (DYNVERT_BOUNDARY | DYNVERT_FSET_BOUNDARY)) {
        continue;
      }
    }
#endif

    float direction2[3];
    const float fade =
        bstrength *
        SCULPT_brush_strength_factor(
            ss, brush, vd.co, sqrtf(test.dist), vd.no, vd.fno, *vd.mask, vd.vertex, thread_id);

    float avg[3], val[3];

    if (use_curvature) {
      SCULPT_curvature_dir_get(ss, vd.vertex, direction2, false);
    }
    else {
      copy_v3_v3(direction2, direction);
    }

#if 0
    if (SCULPT_vertex_is_boundary(
            ss, vd.vertex, SCULPT_BOUNDARY_SHARP | SCULPT_BOUNDARY_MESH | check_fsets)) {
      continue;
    }

    MDynTopoVert *mv = BKE_PBVH_DYNVERT(ss->cd_dyn_vert, vd.bm_vert);
    if (check_fsets && (mv->flag & (DYNVERT_FSET_CORNER))) {
      continue;
    }

    if (mv->flag & (DYNVERT_CORNER | DYNVERT_SHARP_CORNER)) {
      continue;
    }
#endif

    // check origdata to be sure we don't mess it up
    SCULPT_vertex_check_origdata(ss, vd.vertex);

    float *co = vd.co;

    float oldco[3];
    copy_v3_v3(oldco, co);

    SCULPT_bmesh_four_neighbor_average(ss,
                                       avg,
                                       direction2,
                                       vd.bm_vert,
                                       data->rake_projection,
                                       check_fsets,
                                       data->cd_temp,
                                       data->cd_dyn_vert,
                                       0);

    sub_v3_v3v3(val, avg, co);

    float tan[3];
    copy_v3_v3(tan, val);
    madd_v3_v3fl(tan, vd.bm_vert->no, -dot_v3v3(tan, vd.bm_vert->no));

    MDynTopoVert *mv = BKE_PBVH_DYNVERT(ss->cd_dyn_vert, vd.bm_vert);
    madd_v3_v3v3fl(mv->origco, mv->origco, tan, fade * 0.5);

    madd_v3_v3v3fl(val, co, val, fade);
    SCULPT_clip(sd, ss, co, val);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;

  BKE_pbvh_node_mark_normals_update(data->nodes[n]);
}

static void bmesh_topology_rake(Sculpt *sd,
                                Object *ob,
                                PBVHNode **nodes,
                                const int totnode,
                                float bstrength,
                                bool needs_origco)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = ss->cache ? ss->cache->brush : BKE_paint_brush(&sd->paint);
  const float strength = bstrength;  // clamp_f(bstrength, 0.0f, 1.0f);

  Brush local_brush;

  // vector4, nto color
  SCULPT_dyntopo_ensure_templayer(ss, CD_PROP_COLOR, "_rake_temp", false);
  int cd_temp = SCULPT_dyntopo_get_templayer(ss, CD_PROP_COLOR, "_rake_temp");

#ifdef SCULPT_DIAGONAL_EDGE_MARKS
  // reset edge flags, single threaded
  for (int i = 0; i < totnode; i++) {
    PBVHNode *node = nodes[i];
    PBVHVertexIter vd;

    SculptBrushTest test;
    SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
        ss, &test, brush->falloff_shape);

    BKE_pbvh_vertex_iter_begin (ss->pbvh, node, vd, PBVH_ITER_UNIQUE) {
      if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
        continue;
      }

      BMVert *v = vd.bm_vert;
      BMEdge *e = v->e;

      if (!e) {
        continue;
      }

      do {
        e->head.hflag |= BM_ELEM_DRAW;
      } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);
    }
    BKE_pbvh_vertex_iter_end;
  }
#endif

  if (SCULPT_stroke_is_first_brush_step(ss->cache) &&
      (ss->cache->brush->flag2 & BRUSH_SMOOTH_USE_AREA_WEIGHT)) {
    BKE_pbvh_update_all_tri_areas(ss->pbvh);
  }

  if (brush->flag2 & BRUSH_TOPOLOGY_RAKE_IGNORE_BRUSH_FALLOFF) {
    local_brush = *brush;
    brush = &local_brush;

    brush->curve_preset = BRUSH_CURVE_SMOOTH;

    /*note that brush hardness is calculated from ss->cache->paint_brush,
      we can't override it by changing the brush here.
      this seems desirably though?*/
  }
  /* Iterations increase both strength and quality. */
  const int iterations = 3 + ((int)bstrength) * 2;

  int iteration;
  const int count = iterations * strength + 1;
  const float factor = iterations * strength / count * 0.25;

  for (iteration = 0; iteration <= count; iteration++) {

    SculptThreadedTaskData data = {
        .sd = sd,
        .ob = ob,
        .brush = brush,
        .nodes = nodes,
        .strength = factor,
        .cd_temp = cd_temp,
        .use_curvature = SCULPT_get_int(ss, topology_rake_mode, sd, brush),
        .cd_dyn_vert = ss->cd_dyn_vert,
        .rake_projection = brush->topology_rake_projection,
        .do_origco = needs_origco};
    TaskParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, true, totnode);

    BLI_task_parallel_range(0, totnode, &data, do_topology_rake_bmesh_task_cb_ex, &settings);
    BKE_pbvh_update_normals(ss->pbvh, ss->subdiv_ccg);
  }
}

static void do_mask_brush_draw_task_cb_ex(void *__restrict userdata,
                                          const int n,
                                          const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float bstrength = ss->cache->bstrength;

  PBVHVertexIter vd;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    const float fade = SCULPT_brush_strength_factor(
        ss, brush, vd.co, sqrtf(test.dist), vd.no, vd.fno, 0.0f, vd.vertex, thread_id);

    if (bstrength > 0.0f) {
      (*vd.mask) += fade * bstrength * (1.0f - *vd.mask);
    }
    else {
      (*vd.mask) += fade * bstrength * (*vd.mask);
    }
    *vd.mask = clamp_f(*vd.mask, 0.0f, 1.0f);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
    BKE_pbvh_vertex_iter_end;
  }
}

static void do_mask_brush_draw(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_mask_brush_draw_task_cb_ex, &settings);
}

static void do_mask_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  switch ((BrushMaskTool)brush->mask_tool) {
    case BRUSH_MASK_DRAW:
      do_mask_brush_draw(sd, ob, nodes, totnode);
      break;
    case BRUSH_MASK_SMOOTH:
      SCULPT_smooth(sd, ob, nodes, totnode, ss->cache->bstrength, true, 0.0f, false);
      break;
  }
}

/* -------------------------------------------------------------------- */
/** \name Sculpt Multires Displacement Eraser Brush
 * \{ */

static void do_displacement_eraser_brush_task_cb_ex(void *__restrict userdata,
                                                    const int n,
                                                    const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float bstrength = clamp_f(ss->cache->bstrength, 0.0f, 1.0f);

  float(*proxy)[3] = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    float limit_co[3];
    float disp[3];
    SCULPT_vertex_limit_surface_get(ss, vd.vertex, limit_co);
    sub_v3_v3v3(disp, limit_co, vd.co);
    mul_v3_v3fl(proxy[vd.i], disp, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_displacement_eraser_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  Brush *brush = BKE_paint_brush(&sd->paint);
  BKE_curvemapping_init(brush->curve);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_displacement_eraser_brush_task_cb_ex, &settings);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Multires Displacement Eraser Brush
 * \{ */

static void do_fairing_brush_tag_store_task_cb_ex(void *__restrict userdata,
                                                  const int n,
                                                  const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  PBVHVertexIter vd;

  const float bstrength = clamp_f(ss->cache->bstrength, 0.0f, 1.0f);
  const Brush *brush = data->brush;

  const int thread_id = BLI_task_parallel_thread_id(tls);
  const int boundflag = SCULPT_BOUNDARY_MESH | SCULPT_BOUNDARY_SHARP;

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    if (SCULPT_vertex_is_boundary(ss, vd.vertex, boundflag)) {
      continue;
    }

    float *prefair = SCULPT_temp_cdata_get(vd.vertex, ss->custom_layers[SCULPT_SCL_PREFAIRING_CO]);

    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                prefair,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    if (fade == 0.0f) {
      continue;
    }

    float *fairing_fade = SCULPT_temp_cdata_get(vd.vertex,
                                                ss->custom_layers[SCULPT_SCL_FAIRING_FADE]);
    bool *fairing_mask = SCULPT_temp_cdata_get(vd.vertex,
                                               ss->custom_layers[SCULPT_SCL_FAIRING_MASK]);

    *fairing_fade = max_ff(fade, *fairing_fade);
    *fairing_mask = true;
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_fairing_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  const int totvert = SCULPT_vertex_count_get(ss);

  if (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
    return;
  }

  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_face_random_access_ensure(ss);

  if (!ss->custom_layers[SCULPT_SCL_FAIRING_MASK]) {
    // SCULPT_temp_customlayer_ensure(ss, ATTR_DOMAIN_POINT, CD_PROP_BOOL, "fairing_mask");
    // SCULPT_temp_customlayer_ensure(ss, ATTR_DOMAIN_POINT, CD_PROP_FLOAT, "fairing_fade");
    // SCULPT_temp_customlayer_ensure(ss, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, "prefairing_co");

    ss->custom_layers[SCULPT_SCL_FAIRING_MASK] = MEM_callocN(sizeof(SculptCustomLayer),
                                                             "ss->Cache->fairing_mask");
    ss->custom_layers[SCULPT_SCL_FAIRING_FADE] = MEM_callocN(sizeof(SculptCustomLayer),
                                                             "ss->Cache->fairing_fade");
    ss->custom_layers[SCULPT_SCL_PREFAIRING_CO] = MEM_callocN(sizeof(SculptCustomLayer),
                                                              "ss->Cache->prefairing_co");

    SculptLayerParams params = {.permanent = false, .simple_array = true};

    sculpt_temp_customlayer_get(ss,
                                ATTR_DOMAIN_POINT,
                                CD_PROP_BOOL,
                                "fairing_mask",
                                ss->custom_layers[SCULPT_SCL_FAIRING_MASK],
                                true,
                                &params);

    sculpt_temp_customlayer_get(ss,
                                ATTR_DOMAIN_POINT,
                                CD_PROP_FLOAT,
                                "fairing_fade",
                                ss->custom_layers[SCULPT_SCL_FAIRING_FADE],
                                true,
                                &params);

    sculpt_temp_customlayer_get(ss,
                                ATTR_DOMAIN_POINT,
                                CD_PROP_FLOAT3,
                                "prefairing_co",
                                ss->custom_layers[SCULPT_SCL_PREFAIRING_CO],
                                true,
                                &params);

    SCULPT_update_customdata_refs(ss);
  }

  if (SCULPT_stroke_is_main_symmetry_pass(ss->cache)) {
    for (int i = 0; i < totvert; i++) {
      SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

      *(bool *)SCULPT_temp_cdata_get(vertex, ss->custom_layers[SCULPT_SCL_FAIRING_MASK]) = false;
      *(float *)SCULPT_temp_cdata_get(vertex, ss->custom_layers[SCULPT_SCL_FAIRING_FADE]) = 0.0f;
      copy_v3_v3(
          (float *)SCULPT_temp_cdata_get(vertex, ss->custom_layers[SCULPT_SCL_PREFAIRING_CO]),
          SCULPT_vertex_co_get(ss, vertex));
    }
  }

  SCULPT_boundary_info_ensure(ob);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_fairing_brush_tag_store_task_cb_ex, &settings);
}

static void do_fairing_brush_displace_task_cb_ex(void *__restrict userdata,
                                                 const int n,
                                                 const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!*(bool *)SCULPT_temp_cdata_get(vd.vertex, ss->custom_layers[SCULPT_SCL_FAIRING_MASK])) {
      continue;
    }
    float disp[3];
    sub_v3_v3v3(disp,
                vd.co,
                SCULPT_temp_cdata_get(vd.vertex, ss->custom_layers[SCULPT_SCL_PREFAIRING_CO]));
    mul_v3_fl(
        disp,
        *(float *)SCULPT_temp_cdata_get(vd.vertex, ss->custom_layers[SCULPT_SCL_FAIRING_FADE]));
    copy_v3_v3(vd.co,
               SCULPT_temp_cdata_get(vd.vertex, ss->custom_layers[SCULPT_SCL_PREFAIRING_CO]));
    add_v3_v3(vd.co, disp);
    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void sculpt_fairing_brush_exec_fairing_for_cache(Sculpt *sd, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  BLI_assert(BKE_pbvh_type(ss->pbvh) != PBVH_GRIDS);
  BLI_assert(ss->cache);
  Brush *brush = BKE_paint_brush(&sd->paint);
  Mesh *mesh = ob->data;

  if (!ss->custom_layers[SCULPT_SCL_FAIRING_MASK]) {
    return;
  }

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES: {
      MVert *mvert = SCULPT_mesh_deformed_mverts_get(ss);
      BKE_mesh_prefair_and_fair_vertices(mesh,
                                         mvert,
                                         ss->custom_layers[SCULPT_SCL_FAIRING_MASK]->data,
                                         MESH_FAIRING_DEPTH_TANGENCY);
    } break;
    case PBVH_BMESH: {
      // note that we allocated fairing_mask.data in simple array mode
      BKE_bmesh_prefair_and_fair_vertices(
          ss->bm, ss->custom_layers[SCULPT_SCL_FAIRING_MASK]->data, MESH_FAIRING_DEPTH_TANGENCY);
    } break;
    case PBVH_GRIDS:
      BLI_assert(false);
  }

  PBVHNode **nodes;
  int totnode;
  BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_fairing_brush_displace_task_cb_ex, &settings);
  MEM_freeN(nodes);
}

/** \} */

/** \name Sculpt Multires Displacement Smear Brush
 * \{ */

static void do_displacement_smear_brush_task_cb_ex(void *__restrict userdata,
                                                   const int n,
                                                   const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float bstrength = clamp_f(ss->cache->bstrength, 0.0f, 1.0f);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    float current_disp[3];
    float current_disp_norm[3];
    float interp_limit_surface_disp[3];

    copy_v3_v3(interp_limit_surface_disp, ss->cache->prev_displacement[vd.index]);

    switch (brush->smear_deform_type) {
      case BRUSH_SMEAR_DEFORM_DRAG:
        sub_v3_v3v3(current_disp, ss->cache->location, ss->cache->last_location);
        break;
      case BRUSH_SMEAR_DEFORM_PINCH:
        sub_v3_v3v3(current_disp, ss->cache->location, vd.co);
        break;
      case BRUSH_SMEAR_DEFORM_EXPAND:
        sub_v3_v3v3(current_disp, vd.co, ss->cache->location);
        break;
    }

    normalize_v3_v3(current_disp_norm, current_disp);
    mul_v3_v3fl(current_disp, current_disp_norm, ss->cache->bstrength);

    float weights_accum = 1.0f;

    SculptVertexNeighborIter ni;
    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd.vertex, ni) {
      float vertex_disp[3];
      float vertex_disp_norm[3];
      float neighbor_limit_co[3];
      SCULPT_vertex_limit_surface_get(ss, ni.vertex, neighbor_limit_co);
      sub_v3_v3v3(vertex_disp,
                  ss->cache->limit_surface_co[ni.index],
                  ss->cache->limit_surface_co[vd.index]);
      const float *neighbor_limit_surface_disp = ss->cache->prev_displacement[ni.index];
      normalize_v3_v3(vertex_disp_norm, vertex_disp);

      if (dot_v3v3(current_disp_norm, vertex_disp_norm) >= 0.0f) {
        continue;
      }

      const float disp_interp = clamp_f(
          -dot_v3v3(current_disp_norm, vertex_disp_norm), 0.0f, 1.0f);
      madd_v3_v3fl(interp_limit_surface_disp, neighbor_limit_surface_disp, disp_interp);
      weights_accum += disp_interp;
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

    mul_v3_fl(interp_limit_surface_disp, 1.0f / weights_accum);

    float new_co[3];
    add_v3_v3v3(new_co, ss->cache->limit_surface_co[vd.index], interp_limit_surface_disp);
    interp_v3_v3v3(vd.co, vd.co, new_co, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_displacement_smear_store_prev_disp_task_cb_ex(
    void *__restrict userdata, const int n, const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    sub_v3_v3v3(ss->cache->prev_displacement[vd.index],
                SCULPT_vertex_co_get(ss, vd.vertex),
                ss->cache->limit_surface_co[vd.index]);
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_displacement_smear_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  Brush *brush = BKE_paint_brush(&sd->paint);
  SculptSession *ss = ob->sculpt;

  BKE_curvemapping_init(brush->curve);
  SCULPT_vertex_random_access_ensure(ss);

  const int totvert = SCULPT_vertex_count_get(ss);
  if (!ss->cache->prev_displacement) {
    ss->cache->prev_displacement = MEM_malloc_arrayN(
        totvert, sizeof(float[3]), "prev displacement");
    ss->cache->limit_surface_co = MEM_malloc_arrayN(totvert, sizeof(float[3]), "limit surface co");

    for (int i = 0; i < totvert; i++) {
      SculptVertRef vref = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

      SCULPT_vertex_limit_surface_get(ss, vref, ss->cache->limit_surface_co[i]);
      sub_v3_v3v3(ss->cache->prev_displacement[i],
                  SCULPT_vertex_co_get(ss, vref),
                  ss->cache->limit_surface_co[i]);
    }
  }
  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(
      0, totnode, &data, do_displacement_smear_store_prev_disp_task_cb_ex, &settings);
  BLI_task_parallel_range(0, totnode, &data, do_displacement_smear_brush_task_cb_ex, &settings);
}

/** \} */

static void do_draw_brush_task_cb_ex(void *__restrict userdata,
                                     const int n,
                                     const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *offset = data->offset;

  PBVHVertexIter vd;
  float(*proxy)[3];

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    /* Offset vertex. */
    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    vd.co,
                                                    sqrtf(test.dist),
                                                    vd.no,
                                                    vd.fno,
                                                    vd.mask ? *vd.mask : 0.0f,
                                                    vd.vertex,
                                                    thread_id);

    mul_v3_v3fl(proxy[vd.i], offset, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_draw_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
#if 0
  if (BKE_pbvh_type(ob->sculpt->pbvh) == PBVH_BMESH) {
    void cxx_do_draw_brush(Sculpt * sd, Object * ob, PBVHNode * *nodes, int totnode);

    cxx_do_draw_brush(sd, ob, nodes, totnode);
    return;
  }
#endif

  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float offset[3];
  const float bstrength = ss->cache->bstrength;

  /* Offset with as much as possible factored in already. */
  float effective_normal[3];
  SCULPT_tilt_effective_normal_get(ss, brush, effective_normal);
  mul_v3_v3fl(offset, effective_normal, ss->cache->radius);
  mul_v3_v3(offset, ss->cache->scale);
  mul_v3_fl(offset, bstrength);

  /* XXX: this shouldn't be necessary, but sculpting crashes in blender2.8 otherwise
   * initialize before threads so they can do curve mapping. */
  BKE_curvemapping_init(brush->curve);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .offset = offset,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_draw_brush_task_cb_ex, &settings);
}

static void do_draw_sharp_brush_task_cb_ex(void *__restrict userdata,
                                           const int n,
                                           const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *offset = data->offset;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  float(*proxy)[3];

  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);

    if (!sculpt_brush_test_sq_fn(&test, orig_data.co)) {
      continue;
    }
    /* Offset vertex. */
    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    orig_data.co,
                                                    sqrtf(test.dist),
                                                    orig_data.no,
                                                    NULL,
                                                    vd.mask ? *vd.mask : 0.0f,
                                                    vd.vertex,
                                                    thread_id);

    mul_v3_v3fl(proxy[vd.i], offset, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_draw_sharp_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float offset[3];
  const float bstrength = ss->cache->bstrength;

  /* Offset with as much as possible factored in already. */
  float effective_normal[3];
  SCULPT_tilt_effective_normal_get(ss, brush, effective_normal);
  mul_v3_v3fl(offset, effective_normal, ss->cache->radius);
  mul_v3_v3(offset, ss->cache->scale);
  mul_v3_fl(offset, bstrength);

  /* XXX: this shouldn't be necessary, but sculpting crashes in blender2.8 otherwise
   * initialize before threads so they can do curve mapping. */
  BKE_curvemapping_init(brush->curve);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .offset = offset,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_draw_sharp_brush_task_cb_ex, &settings);
}

/* -------------------------------------------------------------------- */
/** \name Sculpt Scene Project Brush
 * \{ */

static void sculpt_stroke_cache_snap_context_init(bContext *C, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;

  if (ss->cache && ss->cache->snap_context) {
    return;
  }

  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  ARegion *region = CTX_wm_region(C);
  View3D *v3d = CTX_wm_view3d(C);

  cache->snap_context = ED_transform_snap_object_context_create_view3d(scene, 0, region, v3d);
  cache->depsgraph = depsgraph;
}

static void sculpt_scene_project_view_ray_init(Object *ob,
                                               const SculptVertRef vertex,
                                               float r_ray_normal[3],
                                               float r_ray_origin[3])
{
  SculptSession *ss = ob->sculpt;

  float world_space_vertex_co[3];
  mul_v3_m4v3(world_space_vertex_co, ob->obmat, SCULPT_vertex_co_get(ss, vertex));
  if (ss->cache->vc->rv3d->is_persp) {
    sub_v3_v3v3(r_ray_normal, world_space_vertex_co, ss->cache->view_origin);
    normalize_v3(r_ray_normal);
    copy_v3_v3(r_ray_origin, ss->cache->view_origin);
  }
  else {
    mul_v3_mat3_m4v3(r_ray_normal, ob->obmat, ss->cache->view_normal);
    sub_v3_v3v3(r_ray_origin, world_space_vertex_co, r_ray_normal);
  }
}

static void sculpt_scene_project_vertex_normal_ray_init(Object *ob,
                                                        const SculptVertRef vertex,
                                                        const float original_normal[3],
                                                        float r_ray_normal[3],
                                                        float r_ray_origin[3])
{
  SculptSession *ss = ob->sculpt;
  mul_v3_m4v3(r_ray_normal, ob->obmat, original_normal);
  normalize_v3(r_ray_normal);

  mul_v3_m4v3(r_ray_origin, ob->obmat, SCULPT_vertex_co_get(ss, vertex));
}

static void sculpt_scene_project_brush_normal_ray_init(Object *ob,
                                                       const SculptVertRef vertex,
                                                       float r_ray_normal[3],
                                                       float r_ray_origin[3])
{
  SculptSession *ss = ob->sculpt;
  mul_v3_m4v3(r_ray_origin, ob->obmat, SCULPT_vertex_co_get(ss, vertex));
  mul_v3_m4v3(r_ray_normal, ob->obmat, ss->cache->sculpt_normal);
  normalize_v3(r_ray_normal);
}

static bool sculpt_scene_project_raycast(SculptSession *ss,
                                         const float ray_normal[3],
                                         const float ray_origin[3],
                                         const bool use_both_directions,
                                         float r_loc[3])
{
  float hit_co[2][3];
  float hit_len_squared[2];
  bool any_hit = false;
  bool hit = false;
  hit = ED_transform_snap_object_project_ray(ss->cache->snap_context,
                                             ss->cache->depsgraph,
                                             &(const struct SnapObjectParams){
                                                 .snap_select = SNAP_NOT_ACTIVE,
                                             },
                                             ray_origin,
                                             ray_normal,
                                             NULL,
                                             hit_co[0],
                                             NULL);
  if (hit) {
    hit_len_squared[0] = len_squared_v3v3(hit_co[0], ray_origin);
    any_hit |= hit;
  }
  else {
    hit_len_squared[0] = FLT_MAX;
  }

  if (!use_both_directions) {
    copy_v3_v3(r_loc, hit_co[0]);
    return any_hit;
  }

  float ray_normal_flip[3];
  mul_v3_v3fl(ray_normal_flip, ray_normal, -1.0f);

  hit = ED_transform_snap_object_project_ray(ss->cache->snap_context,
                                             ss->cache->depsgraph,
                                             &(const struct SnapObjectParams){
                                                 .snap_select = SNAP_NOT_ACTIVE,
                                             },
                                             ray_origin,
                                             ray_normal_flip,
                                             NULL,
                                             hit_co[1],
                                             NULL);
  if (hit) {
    hit_len_squared[1] = len_squared_v3v3(hit_co[1], ray_origin);
    any_hit |= hit;
  }
  else {
    hit_len_squared[1] = FLT_MAX;
  }

  if (hit_len_squared[0] <= hit_len_squared[1]) {
    copy_v3_v3(r_loc, hit_co[0]);
  }
  else {
    copy_v3_v3(r_loc, hit_co[1]);
  }
  return any_hit;
}

static void do_scene_project_brush_task_cb_ex(void *__restrict userdata,
                                              const int n,
                                              const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);

  const float bstrength = clamp_f(ss->cache->bstrength, 0.0f, 1.0f);
  const Brush *brush = data->brush;

  const int thread_id = BLI_task_parallel_thread_id(tls);

  SculptOrigVertData orig_data;
  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);

    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    if (fade == 0.0f) {
      continue;
    }

    float ray_normal[3];
    float ray_origin[3];
    bool use_both_directions = false;
    switch (brush->scene_project_direction_type) {
      case BRUSH_SCENE_PROJECT_DIRECTION_VIEW:
        sculpt_scene_project_view_ray_init(data->ob, vd.vertex, ray_normal, ray_origin);
        break;
      case BRUSH_SCENE_PROJECT_DIRECTION_VERTEX_NORMAL: {
        float normal[3];
        normal_short_to_float_v3(normal, orig_data.no);
        sculpt_scene_project_vertex_normal_ray_init(
            data->ob, vd.vertex, normal, ray_normal, ray_origin);
        use_both_directions = true;
      } break;
      case BRUSH_SCENE_PROJECT_DIRECTION_BRUSH_NORMAL:
        sculpt_scene_project_brush_normal_ray_init(data->ob, vd.vertex, ray_normal, ray_origin);
        use_both_directions = true;
        break;
    }

    float world_space_hit_co[3];
    float hit_co[3];
    const bool hit = sculpt_scene_project_raycast(
        ss, ray_normal, ray_origin, use_both_directions, world_space_hit_co);
    if (!hit) {
      continue;
    }

    mul_v3_m4v3(hit_co, data->ob->imat, world_space_hit_co);

    float disp[3];
    sub_v3_v3v3(disp, hit_co, vd.co);
    mul_v3_fl(disp, fade);
    add_v3_v3(vd.co, disp);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_scene_project_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  SCULPT_vertex_random_access_ensure(ob->sculpt);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, false, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_scene_project_brush_task_cb_ex, &settings);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sculpt Topology Brush
 * \{ */

static void do_topology_slide_task_cb_ex(void *__restrict userdata,
                                         const int n,
                                         const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  float(*proxy)[3];

  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);
    if (!sculpt_brush_test_sq_fn(&test, orig_data.co)) {
      continue;
    }
    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    orig_data.co,
                                                    sqrtf(test.dist),
                                                    orig_data.no,
                                                    NULL,
                                                    vd.mask ? *vd.mask : 0.0f,
                                                    vd.vertex,
                                                    thread_id);
    float current_disp[3];
    float current_disp_norm[3];
    float final_disp[3] = {0.0f, 0.0f, 0.0f};

    switch (brush->slide_deform_type) {
      case BRUSH_SLIDE_DEFORM_DRAG:
        sub_v3_v3v3(current_disp, ss->cache->location, ss->cache->last_location);
        break;
      case BRUSH_SLIDE_DEFORM_PINCH:
        sub_v3_v3v3(current_disp, ss->cache->location, vd.co);
        break;
      case BRUSH_SLIDE_DEFORM_EXPAND:
        sub_v3_v3v3(current_disp, vd.co, ss->cache->location);
        break;
    }

    normalize_v3_v3(current_disp_norm, current_disp);
    mul_v3_v3fl(current_disp, current_disp_norm, ss->cache->bstrength);

    SculptVertexNeighborIter ni;
    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd.vertex, ni) {
      float vertex_disp[3];
      float vertex_disp_norm[3];
      sub_v3_v3v3(vertex_disp, SCULPT_vertex_co_get(ss, ni.vertex), vd.co);
      normalize_v3_v3(vertex_disp_norm, vertex_disp);
      if (dot_v3v3(current_disp_norm, vertex_disp_norm) > 0.0f) {
        madd_v3_v3fl(final_disp, vertex_disp_norm, dot_v3v3(current_disp, vertex_disp));
      }
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

    mul_v3_v3fl(proxy[vd.i], final_disp, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

void SCULPT_relax_vertex(SculptSession *ss,
                         PBVHVertexIter *vd,
                         float factor,
                         bool filter_boundary_face_sets,
                         float *r_final_pos)
{
  float smooth_pos[3];
  float final_disp[3];
  float boundary_normal[3];
  int avg_count = 0;
  int neighbor_count = 0;
  zero_v3(smooth_pos);
  zero_v3(boundary_normal);

  int bset = SCULPT_BOUNDARY_MESH | SCULPT_BOUNDARY_SHARP;

  // forcibly enable if no ss->cache
  if (!ss->cache || (ss->cache->brush->flag2 & BRUSH_SMOOTH_PRESERVE_FACE_SETS)) {
    bset |= SCULPT_BOUNDARY_FACE_SET;
  }

  if (SCULPT_vertex_is_corner(ss, vd->vertex, (SculptCornerType)bset)) {
    copy_v3_v3(r_final_pos, vd->co);
    return;
  }

  const int is_boundary = SCULPT_vertex_is_boundary(ss, vd->vertex, bset);

  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vd->vertex, ni) {
    neighbor_count++;
    if (!filter_boundary_face_sets ||
        (filter_boundary_face_sets && !SCULPT_vertex_has_unique_face_set(ss, ni.vertex))) {

      /* When the vertex to relax is boundary, use only connected boundary vertices for the
       * average position. */
      if (is_boundary) {
        if (!SCULPT_vertex_is_boundary(ss, ni.vertex, bset)) {
          continue;
        }
        add_v3_v3(smooth_pos, SCULPT_vertex_co_get(ss, ni.vertex));
        avg_count++;

        /* Calculate a normal for the constraint plane using the edges of the boundary. */
        float to_neighbor[3];
        sub_v3_v3v3(to_neighbor, SCULPT_vertex_co_get(ss, ni.vertex), vd->co);
        normalize_v3(to_neighbor);
        add_v3_v3(boundary_normal, to_neighbor);
      }
      else {
        add_v3_v3(smooth_pos, SCULPT_vertex_co_get(ss, ni.vertex));
        avg_count++;
      }
    }
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  /* Don't modify corner vertices. */
  if (neighbor_count <= 2) {
    copy_v3_v3(r_final_pos, vd->co);
    return;
  }

  if (avg_count > 0) {
    mul_v3_fl(smooth_pos, 1.0f / avg_count);
  }
  else {
    copy_v3_v3(r_final_pos, vd->co);
    return;
  }

  float plane[4];
  float smooth_closest_plane[3];
  float vno[3];

  if ((is_boundary & SCULPT_BOUNDARY_MESH) && avg_count == 2) {
    normalize_v3_v3(vno, boundary_normal);
  }
  else {
    SCULPT_vertex_normal_get(ss, vd->vertex, vno);
  }

  if (is_zero_v3(vno)) {
    copy_v3_v3(r_final_pos, vd->co);
    return;
  }

  plane_from_point_normal_v3(plane, vd->co, vno);
  closest_to_plane_v3(smooth_closest_plane, plane, smooth_pos);
  sub_v3_v3v3(final_disp, smooth_closest_plane, vd->co);

  mul_v3_fl(final_disp, factor);
  add_v3_v3v3(r_final_pos, vd->co, final_disp);
}

static void do_topology_relax_task_cb_ex(void *__restrict userdata,
                                         const int n,
                                         const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float bstrength = ss->cache->bstrength;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;

  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);

  BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n]);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);

    if (!sculpt_brush_test_sq_fn(&test, orig_data.co)) {
      continue;
    }
    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    orig_data.co,
                                                    sqrtf(test.dist),
                                                    orig_data.no,
                                                    NULL,
                                                    vd.mask ? *vd.mask : 0.0f,
                                                    vd.vertex,
                                                    thread_id);

    SCULPT_relax_vertex(ss, &vd, fade * bstrength, false, vd.co);
    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_slide_relax_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
    return;
  }

  BKE_curvemapping_init(brush->curve);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  if (ss->cache->alt_smooth) {
    SCULPT_boundary_info_ensure(ob);
    for (int i = 0; i < 4; i++) {
      BLI_task_parallel_range(0, totnode, &data, do_topology_relax_task_cb_ex, &settings);
    }
  }
  else {
    BLI_task_parallel_range(0, totnode, &data, do_topology_slide_task_cb_ex, &settings);
  }
}

static void calc_sculpt_plane(
    Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float r_area_no[3], float r_area_co[3])
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (SCULPT_stroke_is_main_symmetry_pass(ss->cache) &&
      (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache) ||
       !(brush->flag & BRUSH_ORIGINAL_PLANE) || !(brush->flag & BRUSH_ORIGINAL_NORMAL))) {
    switch (brush->sculpt_plane) {
      case SCULPT_DISP_DIR_VIEW:
        copy_v3_v3(r_area_no, ss->cache->true_view_normal);
        break;

      case SCULPT_DISP_DIR_X:
        ARRAY_SET_ITEMS(r_area_no, 1.0f, 0.0f, 0.0f);
        break;

      case SCULPT_DISP_DIR_Y:
        ARRAY_SET_ITEMS(r_area_no, 0.0f, 1.0f, 0.0f);
        break;

      case SCULPT_DISP_DIR_Z:
        ARRAY_SET_ITEMS(r_area_no, 0.0f, 0.0f, 1.0f);
        break;

      case SCULPT_DISP_DIR_AREA:
        calc_area_normal_and_center(sd, ob, nodes, totnode, r_area_no, r_area_co);
        if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
          project_plane_v3_v3v3(r_area_no, r_area_no, ss->cache->view_normal);
          normalize_v3(r_area_no);
        }
        break;

      default:
        break;
    }

    /* For flatten center. */
    /* Flatten center has not been calculated yet if we are not using the area normal. */
    if (brush->sculpt_plane != SCULPT_DISP_DIR_AREA) {
      calc_area_center(sd, ob, nodes, totnode, r_area_co);
    }

    /* For area normal. */
    if ((!SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) &&
        (brush->flag & BRUSH_ORIGINAL_NORMAL)) {
      copy_v3_v3(r_area_no, ss->cache->sculpt_normal);
    }
    else {
      copy_v3_v3(ss->cache->sculpt_normal, r_area_no);
    }

    /* For flatten center. */
    if ((!SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) &&
        (brush->flag & BRUSH_ORIGINAL_PLANE)) {
      copy_v3_v3(r_area_co, ss->cache->last_center);
    }
    else {
      copy_v3_v3(ss->cache->last_center, r_area_co);
    }
  }
  else {
    /* For area normal. */
    copy_v3_v3(r_area_no, ss->cache->sculpt_normal);

    /* For flatten center. */
    copy_v3_v3(r_area_co, ss->cache->last_center);

    /* For area normal. */
    flip_v3(r_area_no, ss->cache->mirror_symmetry_pass);

    /* For flatten center. */
    flip_v3(r_area_co, ss->cache->mirror_symmetry_pass);

    /* For area normal. */
    mul_m4_v3(ss->cache->symm_rot_mat, r_area_no);

    /* For flatten center. */
    mul_m4_v3(ss->cache->symm_rot_mat, r_area_co);

    /* Shift the plane for the current tile. */
    add_v3_v3(r_area_co, ss->cache->plane_offset);
  }
}

/** \} */

/**
 * Used for 'SCULPT_TOOL_CREASE' and 'SCULPT_TOOL_BLOB'
 */
static void do_crease_brush_task_cb_ex(void *__restrict userdata,
                                       const int n,
                                       const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  SculptProjectVector *spvc = data->spvc;
  const float flippedbstrength = data->flippedbstrength;
  const float *offset = data->offset;

  PBVHVertexIter vd;
  float(*proxy)[3];

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    /* Offset vertex. */
    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    vd.co,
                                                    sqrtf(test.dist),
                                                    vd.no,
                                                    vd.fno,
                                                    vd.mask ? *vd.mask : 0.0f,
                                                    vd.vertex,
                                                    thread_id);
    float val1[3];
    float val2[3];

    /* First we pinch. */
    sub_v3_v3v3(val1, test.location, vd.co);
    if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
      project_plane_v3_v3v3(val1, val1, ss->cache->view_normal);
    }

    mul_v3_fl(val1, fade * flippedbstrength);

    sculpt_project_v3(spvc, val1, val1);

    /* Then we draw. */
    mul_v3_v3fl(val2, offset, fade);

    add_v3_v3v3(proxy[vd.i], val1, val2);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_crease_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float offset[3];
  float bstrength = ss->cache->bstrength;
  float flippedbstrength, crease_correction;
  float brush_alpha;

  SculptProjectVector spvc;

  /* Offset with as much as possible factored in already. */
  mul_v3_v3fl(offset, ss->cache->sculpt_normal_symm, ss->cache->radius);
  mul_v3_v3(offset, ss->cache->scale);
  mul_v3_fl(offset, bstrength);

  /* We divide out the squared alpha and multiply by the squared crease
   * to give us the pinch strength. */
  crease_correction = SCULPT_get_float(ss, crease_pinch_factor, sd, brush);
  crease_correction = crease_correction * crease_correction * 2.0f;

  brush_alpha = SCULPT_get_float(ss, strength, sd, brush);  // BKE_brush_alpha_get(scene, brush);
  if (brush_alpha > 0.0f) {
    crease_correction /= brush_alpha * brush_alpha;
  }

  /* We always want crease to pinch or blob to relax even when draw is negative. */
  flippedbstrength = (bstrength < 0.0f) ? -crease_correction * bstrength :
                                          crease_correction * bstrength;

  if (brush->sculpt_tool == SCULPT_TOOL_BLOB) {
    flippedbstrength *= -1.0f;
  }

  /* Use surface normal for 'spvc', so the vertices are pinched towards a line instead of a
   * single point. Without this we get a 'flat' surface surrounding the pinch. */
  sculpt_project_v3_cache_init(&spvc, ss->cache->sculpt_normal_symm);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .spvc = &spvc,
      .offset = offset,
      .crease_pinch_factor = SCULPT_get_float(ss, crease_pinch_factor, sd, brush),
      .flippedbstrength = flippedbstrength,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_crease_brush_task_cb_ex, &settings);
}

static void do_pinch_brush_task_cb_ex(void *__restrict userdata,
                                      const int n,
                                      const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  float(*stroke_xz)[3] = data->stroke_xz;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  float x_object_space[3];
  float z_object_space[3];
  copy_v3_v3(x_object_space, stroke_xz[0]);
  copy_v3_v3(z_object_space, stroke_xz[1]);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);
    float disp_center[3];
    float x_disp[3];
    float z_disp[3];
    /* Calculate displacement from the vertex to the brush center. */
    sub_v3_v3v3(disp_center, test.location, vd.co);

    /* Project the displacement into the X vector (aligned to the stroke). */
    mul_v3_v3fl(x_disp, x_object_space, dot_v3v3(disp_center, x_object_space));

    /* Project the displacement into the Z vector (aligned to the surface normal). */
    mul_v3_v3fl(z_disp, z_object_space, dot_v3v3(disp_center, z_object_space));

    /* Add the two projected vectors to calculate the final displacement.
     * The Y component is removed. */
    add_v3_v3v3(disp_center, x_disp, z_disp);

    if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
      project_plane_v3_v3v3(disp_center, disp_center, ss->cache->view_normal);
    }
    mul_v3_v3fl(proxy[vd.i], disp_center, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_pinch_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  float area_no[3];
  float area_co[3];

  float mat[4][4];
  calc_sculpt_plane(sd, ob, nodes, totnode, area_no, area_co);

  /* delay the first daub because grab delta is not setup */
  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
    return;
  }

  if (is_zero_v3(ss->cache->grab_delta_symmetry)) {
    return;
  }

  /* Initialize `mat`. */
  cross_v3_v3v3(mat[0], area_no, ss->cache->grab_delta_symmetry);
  mat[0][3] = 0.0f;
  cross_v3_v3v3(mat[1], area_no, mat[0]);
  mat[1][3] = 0.0f;
  copy_v3_v3(mat[2], area_no);
  mat[2][3] = 0.0f;
  copy_v3_v3(mat[3], ss->cache->location);
  mat[3][3] = 1.0f;
  normalize_m4(mat);

  float stroke_xz[2][3];
  normalize_v3_v3(stroke_xz[0], mat[0]);
  normalize_v3_v3(stroke_xz[1], mat[2]);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .stroke_xz = stroke_xz,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_pinch_brush_task_cb_ex, &settings);
}

static void do_grab_brush_task_cb_ex(void *__restrict userdata,
                                     const int n,
                                     const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *grab_delta = data->grab_delta;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  const bool grab_silhouette = brush->flag2 & BRUSH_GRAB_SILHOUETTE;
  const bool use_geodesic_dists = brush->flag2 & BRUSH_USE_SURFACE_FALLOFF;

  int i = 0;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);

    if (!sculpt_brush_test_sq_fn(&test, orig_data.co)) {
      continue;
    }

    float dist;
    if (use_geodesic_dists) {
      dist = ss->cache->geodesic_dists[ss->cache->mirror_symmetry_pass][vd.index];
    }
    else {
      dist = sqrtf(test.dist);
    }

    float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                          brush,
                                                          orig_data.co,
                                                          dist,
                                                          orig_data.no,
                                                          NULL,
                                                          vd.mask ? *vd.mask : 0.0f,
                                                          vd.vertex,
                                                          thread_id);

    if (grab_silhouette) {
      float silhouette_test_dir[3];
      normalize_v3_v3(silhouette_test_dir, grab_delta);
      if (dot_v3v3(ss->cache->initial_normal, ss->cache->grab_delta_symmetry) < 0.0f) {
        mul_v3_fl(silhouette_test_dir, -1.0f);
      }
      float vno[3];
      normal_short_to_float_v3(vno, orig_data.no);
      fade *= max_ff(dot_v3v3(vno, silhouette_test_dir), 0.0f);
    }

    mul_v3_v3fl(proxy[vd.i], grab_delta, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_grab_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = ss->cache ? ss->cache->brush : BKE_paint_brush(&sd->paint);
  float grab_delta[3];

  copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

  if (ss->cache->normal_weight > 0.0f) {
    sculpt_project_v3_normal_align(ss, ss->cache->normal_weight, grab_delta);
  }

  if (brush->flag2 & BRUSH_USE_SURFACE_FALLOFF) {
    if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
      const int symm_pass = ss->cache->mirror_symmetry_pass;
      float location[3];
      flip_v3_v3(location, SCULPT_active_vertex_co_get(ss), symm_pass);
      SculptVertRef v = SCULPT_nearest_vertex_get(sd, ob, location, ss->cache->radius, false);
      ss->cache->geodesic_dists[symm_pass] = SCULPT_geodesic_from_vertex(
          ob, v, ss->cache->initial_radius);
    }
  }

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .grab_delta = grab_delta,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_grab_brush_task_cb_ex, &settings);
}

static void do_elastic_deform_brush_task_cb_ex(void *__restrict userdata,
                                               const int n,
                                               const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *grab_delta = data->grab_delta;
  const float *location = ss->cache->location;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  float(*proxy)[3];

  const float bstrength = ss->cache->bstrength;

  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  float dir;
  if (ss->cache->mouse[0] > ss->cache->initial_mouse[0]) {
    dir = 1.0f;
  }
  else {
    dir = -1.0f;
  }

  if (brush->elastic_deform_type == BRUSH_ELASTIC_DEFORM_TWIST) {
    int symm = ss->cache->mirror_symmetry_pass;
    if (ELEM(symm, 1, 2, 4, 7)) {
      dir = -dir;
    }
  }

  KelvinletParams params;
  float force = len_v3(grab_delta) * dir * bstrength;
  BKE_kelvinlet_init_params(
      &params, ss->cache->radius, force, 1.0f, brush->elastic_deform_volume_preservation);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);
    float final_disp[3];

    float orig_co[3];
    if (brush->flag2 & BRUSH_USE_SURFACE_FALLOFF) {
      const float geodesic_dist =
          ss->cache->geodesic_dists[ss->cache->mirror_symmetry_pass][vd.index];

      if (geodesic_dist == FLT_MAX) {
        continue;
      }

      float disp[3];
      sub_v3_v3v3(disp, orig_data.co, ss->cache->initial_location);
      normalize_v3(disp);
      mul_v3_fl(disp, geodesic_dist);
      add_v3_v3v3(orig_co, ss->cache->initial_location, disp);
    }
    else {
      copy_v3_v3(orig_co, orig_data.co);
    }

    switch (brush->elastic_deform_type) {
      case BRUSH_ELASTIC_DEFORM_GRAB:
        BKE_kelvinlet_grab(final_disp, &params, orig_co, location, grab_delta);
        mul_v3_fl(final_disp, bstrength * 20.0f);
        break;
      case BRUSH_ELASTIC_DEFORM_GRAB_BISCALE: {
        BKE_kelvinlet_grab_biscale(final_disp, &params, orig_co, location, grab_delta);
        mul_v3_fl(final_disp, bstrength * 20.0f);
        break;
      }
      case BRUSH_ELASTIC_DEFORM_GRAB_TRISCALE: {
        BKE_kelvinlet_grab_triscale(final_disp, &params, orig_co, location, grab_delta);
        mul_v3_fl(final_disp, bstrength * 20.0f);
        break;
      }
      case BRUSH_ELASTIC_DEFORM_SCALE:
        BKE_kelvinlet_scale(final_disp, &params, orig_co, location, ss->cache->sculpt_normal_symm);
        break;
      case BRUSH_ELASTIC_DEFORM_TWIST:
        BKE_kelvinlet_twist(final_disp, &params, orig_co, location, ss->cache->sculpt_normal_symm);
        break;
    }

    if (vd.mask) {
      mul_v3_fl(final_disp, 1.0f - *vd.mask);
    }

    mul_v3_fl(final_disp, SCULPT_automasking_factor_get(ss->cache->automasking, ss, vd.vertex));

    if (dot_v3v3(final_disp, final_disp) > 0.0000001) {
      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }

    copy_v3_v3(proxy[vd.i], final_disp);
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_elastic_deform_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float grab_delta[3];

  copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

  if (ss->cache->normal_weight > 0.0f) {
    sculpt_project_v3_normal_align(ss, ss->cache->normal_weight, grab_delta);
  }

  if (brush->flag2 & BRUSH_USE_SURFACE_FALLOFF) {
    if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
      const int symm_pass = ss->cache->mirror_symmetry_pass;
      float location[3];
      flip_v3_v3(location, SCULPT_active_vertex_co_get(ss), symm_pass);
      SculptVertRef v = SCULPT_nearest_vertex_get(
          sd, ob, location, ss->cache->initial_radius, false);
      ss->cache->geodesic_dists[symm_pass] = SCULPT_geodesic_from_vertex(ob, v, FLT_MAX);
    }
  }

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .grab_delta = grab_delta,
  };

  TaskParallelSettings settings;

  SCULPT_vertex_random_access_ensure(ss);

  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_elastic_deform_brush_task_cb_ex, &settings);
}

ePaintSymmetryAreas SCULPT_get_vertex_symm_area(const float co[3])
{
  ePaintSymmetryAreas symm_area = PAINT_SYMM_AREA_DEFAULT;
  if (co[0] < 0.0f) {
    symm_area |= PAINT_SYMM_AREA_X;
  }
  if (co[1] < 0.0f) {
    symm_area |= PAINT_SYMM_AREA_Y;
  }
  if (co[2] < 0.0f) {
    symm_area |= PAINT_SYMM_AREA_Z;
  }
  return symm_area;
}

void SCULPT_flip_v3_by_symm_area(float v[3],
                                 const ePaintSymmetryFlags symm,
                                 const ePaintSymmetryAreas symmarea,
                                 const float pivot[3])
{
  for (int i = 0; i < 3; i++) {
    ePaintSymmetryFlags symm_it = 1 << i;
    if (!(symm & symm_it)) {
      continue;
    }
    if (symmarea & symm_it) {
      flip_v3(v, symm_it);
    }
    if (pivot[i] < 0.0f) {
      flip_v3(v, symm_it);
    }
  }
}

void SCULPT_flip_quat_by_symm_area(float quat[4],
                                   const ePaintSymmetryFlags symm,
                                   const ePaintSymmetryAreas symmarea,
                                   const float pivot[3])
{
  for (int i = 0; i < 3; i++) {
    ePaintSymmetryFlags symm_it = 1 << i;
    if (!(symm & symm_it)) {
      continue;
    }
    if (symmarea & symm_it) {
      flip_qt(quat, symm_it);
    }
    if (pivot[i] < 0.0f) {
      flip_qt(quat, symm_it);
    }
  }
}

void SCULPT_calc_brush_plane(
    Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float r_area_no[3], float r_area_co[3])
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  zero_v3(r_area_co);
  zero_v3(r_area_no);

  if (SCULPT_stroke_is_main_symmetry_pass(ss->cache) &&
      (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache) ||
       !(brush->flag & BRUSH_ORIGINAL_PLANE) || !(brush->flag & BRUSH_ORIGINAL_NORMAL))) {
    switch (brush->sculpt_plane) {
      case SCULPT_DISP_DIR_VIEW:
        copy_v3_v3(r_area_no, ss->cache->true_view_normal);
        break;

      case SCULPT_DISP_DIR_X:
        ARRAY_SET_ITEMS(r_area_no, 1.0f, 0.0f, 0.0f);
        break;

      case SCULPT_DISP_DIR_Y:
        ARRAY_SET_ITEMS(r_area_no, 0.0f, 1.0f, 0.0f);
        break;

      case SCULPT_DISP_DIR_Z:
        ARRAY_SET_ITEMS(r_area_no, 0.0f, 0.0f, 1.0f);
        break;

      case SCULPT_DISP_DIR_AREA:
        calc_area_normal_and_center(sd, ob, nodes, totnode, r_area_no, r_area_co);
        if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
          project_plane_v3_v3v3(r_area_no, r_area_no, ss->cache->view_normal);
          normalize_v3(r_area_no);
        }
        break;

      default:
        break;
    }

    /* For flatten center. */
    /* Flatten center has not been calculated yet if we are not using the area normal. */
    if (brush->sculpt_plane != SCULPT_DISP_DIR_AREA) {
      calc_area_center(sd, ob, nodes, totnode, r_area_co);
    }

    /* For area normal. */
    if ((!SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) &&
        (brush->flag & BRUSH_ORIGINAL_NORMAL)) {
      copy_v3_v3(r_area_no, ss->cache->sculpt_normal);
    }
    else {
      copy_v3_v3(ss->cache->sculpt_normal, r_area_no);
    }

    /* For flatten center. */
    if ((!SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) &&
        (brush->flag & BRUSH_ORIGINAL_PLANE)) {
      copy_v3_v3(r_area_co, ss->cache->last_center);
    }
    else {
      copy_v3_v3(ss->cache->last_center, r_area_co);
    }
  }
  else {
    /* For area normal. */
    copy_v3_v3(r_area_no, ss->cache->sculpt_normal);

    /* For flatten center. */
    copy_v3_v3(r_area_co, ss->cache->last_center);

    /* For area normal. */
    flip_v3(r_area_no, ss->cache->mirror_symmetry_pass);

    /* For flatten center. */
    flip_v3(r_area_co, ss->cache->mirror_symmetry_pass);

    /* For area normal. */
    mul_m4_v3(ss->cache->symm_rot_mat, r_area_no);

    /* For flatten center. */
    mul_m4_v3(ss->cache->symm_rot_mat, r_area_co);

    /* Shift the plane for the current tile. */
    add_v3_v3(r_area_co, ss->cache->plane_offset);
  }
}

static void do_nudge_brush_task_cb_ex(void *__restrict userdata,
                                      const int n,
                                      const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *cono = data->cono;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    mul_v3_v3fl(proxy[vd.i], cono, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_nudge_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float grab_delta[3];
  float tmp[3], cono[3];

  copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

  cross_v3_v3v3(tmp, ss->cache->sculpt_normal_symm, grab_delta);
  cross_v3_v3v3(cono, tmp, ss->cache->sculpt_normal_symm);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .cono = cono,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_nudge_brush_task_cb_ex, &settings);
}

static void do_snake_hook_brush_task_cb_ex(void *__restrict userdata,
                                           const int n,
                                           const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  SculptProjectVector *spvc = data->spvc;
  const float *grab_delta = data->grab_delta;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;
  const bool do_rake_rotation = ss->cache->is_rake_rotation_valid;
  const bool do_pinch = (data->crease_pinch_factor != 0.5f);
  const float pinch = do_pinch ? (2.0f * (0.5f - data->crease_pinch_factor) *
                                  (len_v3(grab_delta) / ss->cache->radius)) :
                                 0.0f;

  const bool do_elastic = brush->snake_hook_deform_type == BRUSH_SNAKE_HOOK_DEFORM_ELASTIC;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  KelvinletParams params;
  BKE_kelvinlet_init_params(&params, ss->cache->radius, bstrength, 1.0f, 0.4f);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!do_elastic && !sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    float fade;
    if (do_elastic) {
      fade = 1.0f;
    }
    else {
      fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                      brush,
                                                      vd.co,
                                                      sqrtf(test.dist),
                                                      vd.no,
                                                      vd.fno,
                                                      vd.mask ? *vd.mask : 0.0f,
                                                      vd.vertex,
                                                      thread_id);
    }

    mul_v3_v3fl(proxy[vd.i], grab_delta, fade);

    /* Negative pinch will inflate, helps maintain volume. */
    if (do_pinch) {
      float delta_pinch_init[3], delta_pinch[3];

      sub_v3_v3v3(delta_pinch, vd.co, test.location);
      if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
        project_plane_v3_v3v3(delta_pinch, delta_pinch, ss->cache->true_view_normal);
      }

      /* Important to calculate based on the grabbed location
       * (intentionally ignore fade here). */
      add_v3_v3(delta_pinch, grab_delta);

      sculpt_project_v3(spvc, delta_pinch, delta_pinch);

      copy_v3_v3(delta_pinch_init, delta_pinch);

      float pinch_fade = pinch * fade;
      /* When reducing, scale reduction back by how close to the center we are,
       * so we don't pinch into nothingness. */
      if (pinch > 0.0f) {
        /* Square to have even less impact for close vertices. */
        pinch_fade *= pow2f(min_ff(1.0f, len_v3(delta_pinch) / ss->cache->radius));
      }
      mul_v3_fl(delta_pinch, 1.0f + pinch_fade);
      sub_v3_v3v3(delta_pinch, delta_pinch_init, delta_pinch);
      add_v3_v3(proxy[vd.i], delta_pinch);
    }

    if (do_rake_rotation) {
      float delta_rotate[3];
      sculpt_rake_rotate(ss, test.location, vd.co, fade, delta_rotate);
      add_v3_v3(proxy[vd.i], delta_rotate);
    }

    if (do_elastic) {
      float disp[3];
      BKE_kelvinlet_grab_triscale(disp, &params, vd.co, ss->cache->location, proxy[vd.i]);
      mul_v3_fl(disp, bstrength * 20.0f);
      if (vd.mask) {
        mul_v3_fl(disp, 1.0f - *vd.mask);
      }
      mul_v3_fl(disp, SCULPT_automasking_factor_get(ss->cache->automasking, ss, vd.vertex));
      copy_v3_v3(proxy[vd.i], disp);
    }

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_snake_hook_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  const float bstrength = ss->cache->bstrength;
  float grab_delta[3];

  SculptProjectVector spvc;

  copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

  if (bstrength < 0.0f) {
    negate_v3(grab_delta);
  }

  if (ss->cache->normal_weight > 0.0f) {
    sculpt_project_v3_normal_align(ss, ss->cache->normal_weight, grab_delta);
  }

  float crease_pinch_factor = SCULPT_get_float(ss, crease_pinch_factor, sd, brush);

  /* Optionally pinch while painting. */
  if (crease_pinch_factor != 0.5f) {
    sculpt_project_v3_cache_init(&spvc, grab_delta);
  }

  SculptThreadedTaskData data = {.sd = sd,
                                 .ob = ob,
                                 .brush = brush,
                                 .nodes = nodes,
                                 .spvc = &spvc,
                                 .grab_delta = grab_delta,
                                 .crease_pinch_factor = crease_pinch_factor};

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_snake_hook_brush_task_cb_ex, &settings);
}

static void do_thumb_brush_task_cb_ex(void *__restrict userdata,
                                      const int n,
                                      const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *cono = data->cono;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);

    if (!sculpt_brush_test_sq_fn(&test, orig_data.co)) {
      continue;
    }
    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                orig_data.co,
                                                                sqrtf(test.dist),
                                                                orig_data.no,
                                                                NULL,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    mul_v3_v3fl(proxy[vd.i], cono, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_thumb_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  float grab_delta[3];
  float tmp[3], cono[3];

  copy_v3_v3(grab_delta, ss->cache->grab_delta_symmetry);

  cross_v3_v3v3(tmp, ss->cache->sculpt_normal_symm, grab_delta);
  cross_v3_v3v3(cono, tmp, ss->cache->sculpt_normal_symm);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .cono = cono,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_thumb_brush_task_cb_ex, &settings);
}

static void do_rotate_brush_task_cb_ex(void *__restrict userdata,
                                       const int n,
                                       const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float angle = data->angle;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);

    if (!sculpt_brush_test_sq_fn(&test, orig_data.co)) {
      continue;
    }
    float vec[3], rot[3][3];
    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                orig_data.co,
                                                                sqrtf(test.dist),
                                                                orig_data.no,
                                                                NULL,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    sub_v3_v3v3(vec, orig_data.co, ss->cache->location);
    axis_angle_normalized_to_mat3(rot, ss->cache->sculpt_normal_symm, angle * fade);
    mul_v3_m3v3(proxy[vd.i], rot, vec);
    add_v3_v3(proxy[vd.i], ss->cache->location);
    sub_v3_v3(proxy[vd.i], orig_data.co);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_rotate_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  static const int flip[8] = {1, -1, -1, 1, -1, 1, 1, -1};
  const float angle = ss->cache->vertex_rotation * flip[ss->cache->mirror_symmetry_pass];

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .angle = angle,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_rotate_brush_task_cb_ex, &settings);
}

//#define LAYER_FACE_SET_MODE

static void do_layer_brush_task_cb_ex(void *__restrict userdata,
                                      const int n,
                                      const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  Sculpt *sd = data->sd;
  const Brush *brush = data->brush;

  bool use_persistent_base = brush->flag & BRUSH_PERSISTENT;
  const bool is_bmesh = BKE_pbvh_type(ss->pbvh) == PBVH_BMESH;

  if (is_bmesh) {
    use_persistent_base = use_persistent_base && ss->custom_layers[SCULPT_SCL_PERS_CO];

#if 0
    // check if we need to zero displacement factor
    // in first run of brush stroke
    if (!use_persistent_base) {
      int nidx = BKE_pbvh_get_node_index(ss->pbvh, data->nodes[n]);

      bool reset_disp = !BLI_BITMAP_TEST(ss->cache->layer_disp_map, nidx);
      if (reset_disp) {
        PBVHVertexIter vd;

        BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
          BMVert *v = (BMVert *)vd.vertex.i;
          float *disp_factor = BM_ELEM_CD_GET_VOID_P(v, data->cd_layer_disp);

          *disp_factor = 0.0f;

          BLI_BITMAP_SET(ss->cache->layer_disp_map, nidx, true);
        }
        BKE_pbvh_vertex_iter_end;
      }
    }
#endif
  }
  else {
    use_persistent_base = use_persistent_base && ss->custom_layers[SCULPT_SCL_PERS_CO];
  }

  SculptCustomLayer *scl_disp = data->scl;
  SculptCustomLayer *scl_stroke_id = data->scl2;

  PBVHVertexIter vd;
  SculptOrigVertData orig_data;
  const float bstrength = ss->cache->bstrength;
  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);

    if (!sculpt_brush_test_sq_fn(&test, orig_data.co)) {
      continue;
    }

    if (!use_persistent_base) {
      int *stroke_id = SCULPT_temp_cdata_get(vd.vertex, scl_stroke_id);

      if (*stroke_id != ss->stroke_id) {
        *((float *)SCULPT_temp_cdata_get(vd.vertex, scl_disp)) = 0.0f;
        *stroke_id = ss->stroke_id;
      }
    }

    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    vd.co,
                                                    sqrtf(test.dist),
                                                    vd.no,
                                                    vd.fno,
                                                    vd.mask ? *vd.mask : 0.0f,
                                                    vd.vertex,
                                                    thread_id);

    const int vi = vd.index;
    float *disp_factor;

    if (use_persistent_base) {
      disp_factor = (float *)SCULPT_temp_cdata_get(vd.vertex,
                                                   ss->custom_layers[SCULPT_SCL_PERS_DISP]);
    }
    else {
      disp_factor = (float *)SCULPT_temp_cdata_get(vd.vertex, scl_disp);
    }

    /* When using persistent base, the layer brush (holding Control) invert mode resets the
     * height of the layer to 0. This makes possible to clean edges of previously added layers
     * on top of the base. */
    /* The main direction of the layers is inverted using the regular brush strength with the
     * brush direction property. */
    if (use_persistent_base && ss->cache->invert) {
      (*disp_factor) += fabsf(fade * bstrength * (*disp_factor)) *
                        ((*disp_factor) > 0.0f ? -1.0f : 1.0f);
    }
    else {
      (*disp_factor) += fade * bstrength * (1.05f - fabsf(*disp_factor));
    }
    if (vd.mask) {
      const float clamp_mask = 1.0f - *vd.mask;
      *disp_factor = clamp_f(*disp_factor, -clamp_mask, clamp_mask);
    }
    else {
      *disp_factor = clamp_f(*disp_factor, -1.0f, 1.0f);
    }

    float final_co[3];
    float normal[3];

    if (use_persistent_base) {
      SCULPT_vertex_persistent_normal_get(ss, vd.vertex, normal);
      mul_v3_fl(normal, brush->height);
      madd_v3_v3v3fl(
          final_co, SCULPT_vertex_persistent_co_get(ss, vd.vertex), normal, *disp_factor);
    }
    else {
      normal_short_to_float_v3(normal, orig_data.no);
      mul_v3_fl(normal, brush->height);
      madd_v3_v3v3fl(final_co, orig_data.co, normal, *disp_factor);
    }

    float vdisp[3];
    sub_v3_v3v3(vdisp, final_co, vd.co);
    mul_v3_fl(vdisp, fabsf(fade));
    add_v3_v3v3(final_co, vd.co, vdisp);

    SCULPT_clip(sd, ss, vd.co, final_co);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;

#ifdef LAYER_FACE_SET_MODE
  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    TableGSet *bm_faces = BKE_pbvh_bmesh_node_faces(data->nodes[n]);
    TableGSet *bm_unique_verts = BKE_pbvh_bmesh_node_unique_verts(data->nodes[n]);
    BMFace *f;

    const int cd_vcol = ss->cd_vcol_offset;

    int fset2 = 1;  // data->face_set;
    const int cd_disp = use_persistent_base ? data->cd_pers_disp : data->cd_layer_disp;
    int f_ni = BKE_pbvh_get_node_index(ss->pbvh, data->nodes[n]);
    BMVert *v;

    TGSET_ITER (v, bm_unique_verts) {
      BMIter iter;
      BM_ITER_ELEM (f, &iter, v, BM_FACES_OF_VERT) {
        if (BM_ELEM_CD_GET_INT(f, ss->cd_face_node_offset) != f_ni) {
          continue;
        }
        if (BM_ELEM_CD_GET_INT(f, ss->cd_faceset_offset) < 0) {
          // continue;
        }

        fset2 = 1;
        float height = 0.0;
        int tot = 0;
        BMLoop *l = f->l_first;

        int inside = 0;

        do {
          int v_ni = BM_ELEM_CD_GET_INT(l->v, ss->cd_vert_node_offset);

          float *disp_factor = BM_ELEM_CD_GET_VOID_P(l->v, cd_disp);
          MDynTopoVert *mv = BKE_PBVH_DYNVERT(ss->cd_dyn_vert, l->v);
          mv->flag |= DYNVERT_NEED_BOUNDARY;

          if (cd_vcol >= 0) {
            MPropCol *col = BM_ELEM_CD_GET_VOID_P(l->v, cd_vcol);
            col->color[0] = MAX2(*disp_factor, 0.0f);
            col->color[1] = MAX2(-(*disp_factor), 0.0f);
            col->color[2] = 0.0f;
            col->color[3] = 1.0f;
          }

          if (sculpt_brush_test_sq_fn(&test, l->v->co)) {  // mv->origco)) {
            inside++;
          }

          if (*disp_factor < 0.9) {
            fset2 = data->face_set2;
          }

          height += *disp_factor;
          tot++;
        } while ((l = l->next) != f->l_first);

        if (!inside) {
          continue;
        }

        int *ptr = BM_ELEM_CD_GET_VOID_P(f, ss->cd_faceset_offset);
        int old = *ptr;
        atomic_cas_int32(ptr, old, fset2);

        // BM_ELEM_CD_SET_INT(f, ss->cd_faceset_offset, fset2);
      }
    }
    TGSET_ITER_END;
  }

#endif
}

void SCULPT_ensure_persistent_layers(SculptSession *ss)
{
  if (!ss->custom_layers[SCULPT_SCL_PERS_CO]) {
    SculptLayerParams params = {.permanent = true, .simple_array = false};

    ss->custom_layers[SCULPT_SCL_PERS_CO] = MEM_callocN(sizeof(SculptCustomLayer), "scl_pers_co");
    SCULPT_temp_customlayer_get(ss,
                                ATTR_DOMAIN_POINT,
                                CD_PROP_FLOAT3,
                                SCULPT_LAYER_PERS_CO,
                                ss->custom_layers[SCULPT_SCL_PERS_CO],
                                &params);

    ss->custom_layers[SCULPT_SCL_PERS_NO] = MEM_callocN(sizeof(SculptCustomLayer), "scl_pers_no");
    SCULPT_temp_customlayer_get(ss,
                                ATTR_DOMAIN_POINT,
                                CD_PROP_FLOAT3,
                                SCULPT_LAYER_PERS_NO,
                                ss->custom_layers[SCULPT_SCL_PERS_NO],
                                &params);

    ss->custom_layers[SCULPT_SCL_PERS_DISP] = MEM_callocN(sizeof(SculptCustomLayer),
                                                          "scl_pers_disp");
    SCULPT_temp_customlayer_get(ss,
                                ATTR_DOMAIN_POINT,
                                CD_PROP_FLOAT,
                                SCULPT_LAYER_PERS_DISP,
                                ss->custom_layers[SCULPT_SCL_PERS_DISP],
                                &params);
  }
}

static void do_layer_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

#ifdef LAYER_FACE_SET_MODE
  if (SCULPT_stroke_is_first_brush_step(ss->cache)) {
    ss->cache->paint_face_set = SCULPT_face_set_next_available_get(ss);
  }

  const int fset = ss->cache->paint_face_set;
#endif

  if ((brush->flag & BRUSH_PERSISTENT) && SCULPT_has_persistent_base(ss)) {
    SCULPT_ensure_persistent_layers(ss);
  }

  if (!ss->custom_layers[SCULPT_SCL_LAYER_DISP]) {
    ss->custom_layers[SCULPT_SCL_LAYER_DISP] = MEM_callocN(sizeof(SculptCustomLayer),
                                                           "layer disp scl");
    SCULPT_temp_customlayer_get(ss,
                                ATTR_DOMAIN_POINT,
                                CD_PROP_FLOAT,
                                SCULPT_LAYER_DISP,
                                ss->custom_layers[SCULPT_SCL_LAYER_DISP],
                                &((SculptLayerParams){.permanent = false, .simple_array = false}));
  }

  if (!ss->custom_layers[SCULPT_SCL_LAYER_STROKE_ID]) {
    ss->custom_layers[SCULPT_SCL_LAYER_STROKE_ID] = MEM_callocN(sizeof(SculptCustomLayer),
                                                                "layer disp scl");
    SCULPT_temp_customlayer_get(ss,
                                ATTR_DOMAIN_POINT,
                                CD_PROP_INT32,
                                SCULPT_LAYER_STROKE_ID,
                                ss->custom_layers[SCULPT_SCL_LAYER_STROKE_ID],
                                &((SculptLayerParams){.permanent = false, .simple_array = false}));
  }

  if (BKE_pbvh_type(ss->pbvh) != PBVH_BMESH) {
    ss->cache->layer_displacement_factor = ss->custom_layers[SCULPT_SCL_LAYER_DISP]->data;
    ss->cache->layer_stroke_id = ss->custom_layers[SCULPT_SCL_LAYER_STROKE_ID]->data;
  }

  SCULPT_vertex_random_access_ensure(ss);

  SculptThreadedTaskData data = {.sd = sd,
                                 .ob = ob,
                                 .brush = brush,
                                 .nodes = nodes,
                                 .scl = ss->custom_layers[SCULPT_SCL_LAYER_DISP],
                                 .scl2 = ss->custom_layers[SCULPT_SCL_LAYER_STROKE_ID],
#ifdef LAYER_FACE_SET_MODE
                                 .face_set = fset,
                                 .face_set2 = fset + 1

#endif
  };

  TaskParallelSettings settings;
#ifdef LAYER_FACE_SET_MODE
  BKE_pbvh_parallel_range_settings(&settings, false, totnode);
#else
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
#endif
  BLI_task_parallel_range(0, totnode, &data, do_layer_brush_task_cb_ex, &settings);
}

static void do_inflate_brush_task_cb_ex(void *__restrict userdata,
                                        const int n,
                                        const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);
    float val[3];

    if (vd.fno) {
      copy_v3_v3(val, vd.fno);
    }
    else {
      normal_short_to_float_v3(val, vd.no);
    }

    mul_v3_fl(val, fade * ss->cache->radius);
    mul_v3_v3v3(proxy[vd.i], val, ss->cache->scale);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_inflate_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_inflate_brush_task_cb_ex, &settings);
}

int SCULPT_plane_trim(const StrokeCache *cache, const Brush *brush, const float val[3])
{
  return (!(cache->use_plane_trim) ||
          ((dot_v3v3(val, val) <= cache->radius_squared * cache->plane_trim_squared)));
}

static bool plane_point_side_flip(const float co[3], const float plane[4], const bool flip)
{
  float d = plane_point_side_v3(plane, co);
  if (flip) {
    d = -d;
  }
  return d <= 0.0f;
}

int SCULPT_plane_point_side(const float co[3], const float plane[4])
{
  float d = plane_point_side_v3(plane, co);
  return d <= 0.0f;
}

float SCULPT_brush_plane_offset_get(Sculpt *sd, SculptSession *ss)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  float rv = brush->plane_offset;

  if (brush->flag & BRUSH_OFFSET_PRESSURE) {
    rv *= ss->cache->pressure;
  }

  return rv;
}

static void do_flatten_brush_task_cb_ex(void *__restrict userdata,
                                        const int n,
                                        const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *area_no = data->area_no;
  const float *area_co = data->area_co;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  plane_from_point_normal_v3(test.plane_tool, area_co, area_no);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    float intr[3];
    float val[3];

    closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);

    sub_v3_v3v3(val, intr, vd.co);

    if (SCULPT_plane_trim(ss->cache, brush, val)) {
      const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                  brush,
                                                                  vd.co,
                                                                  sqrtf(test.dist),
                                                                  vd.no,
                                                                  vd.fno,
                                                                  vd.mask ? *vd.mask : 0.0f,
                                                                  vd.vertex,
                                                                  thread_id);
      mul_v3_v3fl(proxy[vd.i], val, fade);

      if (vd.mvert) {
        vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_flatten_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const float radius = ss->cache->radius;

  float area_no[3];
  float area_co[3];

  float offset = SCULPT_brush_plane_offset_get(sd, ss);
  float displace;
  float temp[3];

  SCULPT_calc_brush_plane(sd, ob, nodes, totnode, area_no, area_co);

  SCULPT_tilt_apply_to_normal(area_no, ss->cache, brush->tilt_strength_factor);

  displace = radius * offset;

  mul_v3_v3v3(temp, area_no, ss->cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .area_no = area_no,
      .area_co = area_co,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_flatten_brush_task_cb_ex, &settings);
}

/* -------------------------------------------------------------------- */
/** \name Sculpt Clay Brush
 * \{ */

typedef struct ClaySampleData {
  float plane_dist[2];
} ClaySampleData;

static void calc_clay_surface_task_cb(void *__restrict userdata,
                                      const int n,
                                      const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  ClaySampleData *csd = tls->userdata_chunk;
  const float *area_no = data->area_no;
  const float *area_co = data->area_co;
  float plane[4];

  PBVHVertexIter vd;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, brush->falloff_shape);

  /* Apply the brush normal radius to the test before sampling. */
  float test_radius = sqrtf(test.radius_squared);
  test_radius *= brush->normal_radius_factor;
  test.radius_squared = test_radius * test_radius;
  plane_from_point_normal_v3(plane, area_co, area_no);

  if (is_zero_v4(plane)) {
    return;
  }

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    float plane_dist = dist_signed_to_plane_v3(vd.co, plane);
    float plane_dist_abs = fabsf(plane_dist);
    if (plane_dist > 0.0f) {
      csd->plane_dist[0] = MIN2(csd->plane_dist[0], plane_dist_abs);
    }
    else {
      csd->plane_dist[1] = MIN2(csd->plane_dist[1], plane_dist_abs);
    }
    BKE_pbvh_vertex_iter_end;
  }
}

static void calc_clay_surface_reduce(const void *__restrict UNUSED(userdata),
                                     void *__restrict chunk_join,
                                     void *__restrict chunk)
{
  ClaySampleData *join = chunk_join;
  ClaySampleData *csd = chunk;
  join->plane_dist[0] = MIN2(csd->plane_dist[0], join->plane_dist[0]);
  join->plane_dist[1] = MIN2(csd->plane_dist[1], join->plane_dist[1]);
}

static void do_clay_brush_task_cb_ex(void *__restrict userdata,
                                     const int n,
                                     const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *area_no = data->area_no;
  const float *area_co = data->area_co;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = fabsf(ss->cache->bstrength);

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  plane_from_point_normal_v3(test.plane_tool, area_co, area_no);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    SCULPT_vertex_check_origdata(ss, vd.vertex);

    float intr[3];
    float val[3];
    closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);

    sub_v3_v3v3(val, intr, vd.co);

    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    mul_v3_v3fl(proxy[vd.i], val, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;

  BKE_pbvh_node_mark_update(data->nodes[n]);
}

static void do_clay_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const float radius = fabsf(ss->cache->radius);
  const float initial_radius = fabsf(ss->cache->initial_radius);
  bool flip = ss->cache->bstrength < 0.0f;

  float offset = SCULPT_brush_plane_offset_get(sd, ss);
  float displace;

  float area_no[3];
  float area_co[3];
  float temp[3];

  SCULPT_calc_brush_plane(sd, ob, nodes, totnode, area_no, area_co);

  SculptThreadedTaskData sample_data = {
      .sd = NULL,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .totnode = totnode,
      .area_no = area_no,
      .area_co = ss->cache->location,
  };

  ClaySampleData csd = {{0}};

  TaskParallelSettings sample_settings;
  BKE_pbvh_parallel_range_settings(&sample_settings, true, totnode);
  sample_settings.func_reduce = calc_clay_surface_reduce;
  sample_settings.userdata_chunk = &csd;
  sample_settings.userdata_chunk_size = sizeof(ClaySampleData);

  BLI_task_parallel_range(0, totnode, &sample_data, calc_clay_surface_task_cb, &sample_settings);

  float d_offset = (csd.plane_dist[0] + csd.plane_dist[1]);
  d_offset = min_ff(radius, d_offset);
  d_offset = d_offset / radius;
  d_offset = 1.0f - d_offset;
  displace = fabsf(initial_radius * (0.25f + offset + (d_offset * 0.15f)));
  if (flip) {
    displace = -displace;
  }

  mul_v3_v3v3(temp, area_no, ss->cache->scale);
  mul_v3_fl(temp, displace);
  copy_v3_v3(area_co, ss->cache->location);
  add_v3_v3(area_co, temp);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .area_no = area_no,
      .area_co = area_co,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_clay_brush_task_cb_ex, &settings);
}

static void do_clay_strips_brush_task_cb_ex(void *__restrict userdata,
                                            const int n,
                                            const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  float(*mat)[4] = data->mat;
  const float *area_no_sp = data->area_no_sp;
  const float *area_co = data->area_co;

  PBVHVertexIter vd;
  SculptBrushTest test;
  float(*proxy)[3];
  const bool flip = (ss->cache->bstrength < 0.0f);
  const float bstrength = flip ? -ss->cache->bstrength : ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SCULPT_brush_test_init(ss, &test);
  plane_from_point_normal_v3(test.plane_tool, area_co, area_no_sp);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!SCULPT_brush_test_cube(&test, vd.co, mat, brush->tip_roundness)) {
      continue;
    }

    if (!plane_point_side_flip(vd.co, test.plane_tool, flip)) {
      continue;
    }

    float vertex_no[3];
    SCULPT_vertex_normal_get(ss, vd.vertex, vertex_no);
    if (dot_v3v3(area_no_sp, vertex_no) <= -0.1f) {
      continue;
    }

    float intr[3];
    float val[3];
    closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);
    sub_v3_v3v3(val, intr, vd.co);

    if (!SCULPT_plane_trim(ss->cache, brush, val)) {
      continue;
    }

    /* The normal from the vertices is ignored, it causes glitch with planes, see: T44390. */
    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                ss->cache->radius * test.dist,
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    mul_v3_v3fl(proxy[vd.i], val, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;

  BKE_pbvh_node_mark_update(data->nodes[n]);
}

static void do_clay_strips_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const bool flip = (ss->cache->bstrength < 0.0f);
  const float radius = flip ? -ss->cache->radius : ss->cache->radius;
  const float offset = SCULPT_brush_plane_offset_get(sd, ss);
  const float displace = radius * (0.18f + offset);

  SCULPT_vertex_random_access_ensure(ss);

  /* The sculpt-plane normal (whatever its set to). */
  float area_no_sp[3];

  /* Geometry normal */
  float area_no[3];
  float area_co[3];

  float temp[3];
  float mat[4][4];
  float scale[4][4];
  float tmat[4][4];

  SCULPT_calc_brush_plane(sd, ob, nodes, totnode, area_no_sp, area_co);
  SCULPT_tilt_apply_to_normal(area_no_sp, ss->cache, brush->tilt_strength_factor);

  if (brush->sculpt_plane != SCULPT_DISP_DIR_AREA || (brush->flag & BRUSH_ORIGINAL_NORMAL)) {
    SCULPT_calc_area_normal(sd, ob, nodes, totnode, area_no);
  }
  else {
    copy_v3_v3(area_no, area_no_sp);
  }

  /* Delay the first daub because grab delta is not setup. */
  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
    return;
  }

  if (is_zero_v3(ss->cache->grab_delta_symmetry)) {
    return;
  }

  mul_v3_v3v3(temp, area_no_sp, ss->cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  /* Clay Strips uses a cube test with falloff in the XY axis (not in Z) and a plane to deform the
   * vertices. When in Add mode, vertices that are below the plane and inside the cube are move
   * towards the plane. In this situation, there may be cases where a vertex is outside the cube
   * but below the plane, so won't be deformed, causing artifacts. In order to prevent these
   * artifacts, this displaces the test cube space in relation to the plane in order to
   * deform more vertices that may be below it. */
  /* The 0.7 and 1.25 factors are arbitrary and don't have any relation between them, they were set
   * by doing multiple tests using the default "Clay Strips" brush preset. */
  float area_co_displaced[3];
  madd_v3_v3v3fl(area_co_displaced, area_co, area_no, -radius * 0.7f);

  /* Initialize brush local-space matrix. */
  cross_v3_v3v3(mat[0], area_no, ss->cache->grab_delta_symmetry);
  mat[0][3] = 0.0f;
  cross_v3_v3v3(mat[1], area_no, mat[0]);
  mat[1][3] = 0.0f;
  copy_v3_v3(mat[2], area_no);
  mat[2][3] = 0.0f;
  copy_v3_v3(mat[3], area_co_displaced);
  mat[3][3] = 1.0f;
  normalize_m4(mat);

  /* Scale brush local space matrix. */
  scale_m4_fl(scale, ss->cache->radius);
  mul_m4_m4m4(tmat, mat, scale);

  /* Deform the local space in Z to scale the test cube. As the test cube does not have falloff in
   * Z this does not produce artifacts in the falloff cube and allows to deform extra vertices
   * during big deformation while keeping the surface as uniform as possible. */
  mul_v3_fl(tmat[2], 1.25f);

  invert_m4_m4(mat, tmat);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .area_no_sp = area_no_sp,
      .area_co = area_co,
      .mat = mat,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_clay_strips_brush_task_cb_ex, &settings);
}

/****** Twist Brush **********/

static void do_twist_brush_task_cb_ex(void *__restrict userdata,
                                      const int n,
                                      const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  float(*mat)[4] = data->mat;

  PBVHVertexIter vd;
  const bool flip = (ss->cache->bstrength < 0.0f);
  const float bstrength = flip ? -ss->cache->bstrength : ss->cache->bstrength;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  float stroke_direction[3];
  float stroke_line[2][3];
  normalize_v3_v3(stroke_direction, ss->cache->grab_delta_symmetry);
  copy_v3_v3(stroke_line[0], ss->cache->location);
  add_v3_v3v3(stroke_line[1], stroke_line[0], stroke_direction);

  SculptOrigVertData orig_data;
  SCULPT_orig_vert_data_init(&orig_data, data->ob, data->nodes[n], SCULPT_UNDO_COORDS);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {

    SCULPT_orig_vert_data_update(&orig_data, vd.vertex);
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    vd.co,
                                                    sqrtf(test.dist),
                                                    vd.no,
                                                    vd.fno,
                                                    vd.mask ? *vd.mask : 0.0f,
                                                    vd.vertex,
                                                    thread_id);

    if (fade == 0.0f) {
      continue;
    }

    float local_vert_co[3];
    float rotation_axis[3] = {0.0, 1.0, 0.0};
    float origin[3] = {0.0, 0.0, 0.0f};
    float vertex_in_line[3];
    float scaled_mat[4][4];
    float scaled_mat_inv[4][4];

    copy_m4_m4(scaled_mat, mat);
    invert_m4(scaled_mat);
    mul_v3_fl(scaled_mat[2], 0.7f * fade * (1.0f - bstrength));
    invert_m4(scaled_mat);

    invert_m4_m4(scaled_mat_inv, scaled_mat);

    mul_v3_m4v3(local_vert_co, scaled_mat, vd.co);
    closest_to_line_v3(vertex_in_line, local_vert_co, rotation_axis, origin);
    float p_to_rotate[3];
    sub_v3_v3v3(p_to_rotate, local_vert_co, vertex_in_line);
    float p_rotated[3];
    rotate_v3_v3v3fl(p_rotated, p_to_rotate, rotation_axis, 2.0f * bstrength * fade);
    add_v3_v3(p_rotated, vertex_in_line);
    mul_v3_m4v3(p_rotated, scaled_mat_inv, p_rotated);

    float disp[3];
    sub_v3_v3v3(disp, p_rotated, vd.co);
    mul_v3_fl(disp, bstrength * fade);
    add_v3_v3(vd.co, disp);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_twist_brush_post_smooth_task_cb_ex(void *__restrict userdata,
                                                  const int n,
                                                  const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  float(*mat)[4] = data->mat;

  PBVHVertexIter vd;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    float local_vert_co[3];
    float scaled_mat[4][4];
    copy_m4_m4(scaled_mat, mat);
    invert_m4(scaled_mat);
    invert_m4(scaled_mat);
    mul_v3_m4v3(local_vert_co, scaled_mat, vd.co);

    const float brush_fade = SCULPT_brush_strength_factor(ss,
                                                          brush,
                                                          vd.co,
                                                          sqrtf(test.dist),
                                                          vd.no,
                                                          vd.fno,
                                                          vd.mask ? *vd.mask : 0.0f,
                                                          vd.vertex,
                                                          thread_id);

    float smooth_fade = SCULPT_brush_strength_factor(ss,
                                                     brush,
                                                     vd.co,
                                                     local_vert_co[0],
                                                     vd.no,
                                                     vd.fno,
                                                     vd.mask ? *vd.mask : 0.0f,
                                                     vd.vertex,
                                                     thread_id);

    if (brush_fade == 0.0f) {
      // continue;
    }

    if (smooth_fade == 0.0f) {
      // continue;
    }

    smooth_fade = 1.0f - min_ff(fabsf(local_vert_co[0]), 1.0f);
    smooth_fade = pow3f(smooth_fade);

    float disp[3];

    /*
            SCULPT_neighbor_coords_average(ss, avg, vd.index);

            sub_v3_v3v3(disp, avg, vd.co);
            mul_v3_fl(disp, 1.0f - smooth_fade);
            add_v3_v3(vd.co, disp);
            */

    float final_co[3];
    SCULPT_relax_vertex(ss, &vd, clamp_f(smooth_fade, 0.0f, 1.0f), false, final_co);

    sub_v3_v3v3(disp, final_co, vd.co);
    add_v3_v3(vd.co, disp);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_twist_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  SCULPT_vertex_random_access_ensure(ss);
  SCULPT_boundary_info_ensure(ob);

  /* The sculpt-plane normal (whatever its set to). */
  float area_no_sp[3];

  /* Geometry normal */
  float area_no[3];
  float area_co[3];

  float scale[4][4];
  float tmat[4][4];
  float mat[4][4];

  SCULPT_calc_brush_plane(sd, ob, nodes, totnode, area_no_sp, area_co);
  SCULPT_tilt_apply_to_normal(area_no_sp, ss->cache, brush->tilt_strength_factor);

  if (brush->sculpt_plane != SCULPT_DISP_DIR_AREA || (brush->flag & BRUSH_ORIGINAL_NORMAL)) {
    SCULPT_calc_area_normal(sd, ob, nodes, totnode, area_no);
  }
  else {
    copy_v3_v3(area_no, area_no_sp);
  }

  /* Delay the first daub because grab delta is not setup. */
  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
    return;
  }

  if (is_zero_v3(ss->cache->grab_delta_symmetry)) {
    return;
  }

  /*
    mul_v3_v3v3(temp, area_no_sp, ss->cache->scale);
    mul_v3_fl(temp, displace);
    add_v3_v3(area_co, temp);
    */

  /* Initialize brush local-space matrix. */
  cross_v3_v3v3(mat[0], area_no, ss->cache->grab_delta_symmetry);
  mat[0][3] = 0.0f;
  cross_v3_v3v3(mat[1], area_no, mat[0]);
  mat[1][3] = 0.0f;
  copy_v3_v3(mat[2], area_no);
  mat[2][3] = 0.0f;
  copy_v3_v3(mat[3], area_co);
  mat[3][3] = 1.0f;
  normalize_m4(mat);

  /* Scale brush local space matrix. */
  scale_m4_fl(scale, ss->cache->radius * 0.5f);
  mul_m4_m4m4(tmat, mat, scale);

  /* Scale rotation space. */
  // mul_v3_fl(tmat[2], 0.5f);

  float twist_mat[4][4];
  invert_m4_m4(twist_mat, tmat);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .area_no_sp = area_no_sp,
      .area_co = area_co,
      .mat = twist_mat,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_twist_brush_task_cb_ex, &settings);

  scale_m4_fl(scale, ss->cache->radius);
  mul_m4_m4m4(tmat, mat, scale);
  float smooth_mat[4][4];
  invert_m4_m4(smooth_mat, tmat);
  data.mat = smooth_mat;

  for (int i = 0; i < 2; i++) {
    BLI_task_parallel_range(0, totnode, &data, do_twist_brush_post_smooth_task_cb_ex, &settings);
  }
}

static void do_fill_brush_task_cb_ex(void *__restrict userdata,
                                     const int n,
                                     const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *area_no = data->area_no;
  const float *area_co = data->area_co;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  plane_from_point_normal_v3(test.plane_tool, area_co, area_no);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    if (!SCULPT_plane_point_side(vd.co, test.plane_tool)) {
      continue;
    }

    float intr[3];
    float val[3];
    closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);
    sub_v3_v3v3(val, intr, vd.co);

    if (!SCULPT_plane_trim(ss->cache, brush, val)) {
      continue;
    }

    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    mul_v3_v3fl(proxy[vd.i], val, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_fill_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const float radius = ss->cache->radius;

  float area_no[3];
  float area_co[3];
  float offset = SCULPT_brush_plane_offset_get(sd, ss);

  float displace;

  float temp[3];

  SCULPT_calc_brush_plane(sd, ob, nodes, totnode, area_no, area_co);

  SCULPT_tilt_apply_to_normal(area_no, ss->cache, brush->tilt_strength_factor);

  displace = radius * offset;

  mul_v3_v3v3(temp, area_no, ss->cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .area_no = area_no,
      .area_co = area_co,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_fill_brush_task_cb_ex, &settings);
}

static void do_scrape_brush_task_cb_ex(void *__restrict userdata,
                                       const int n,
                                       const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  const float *area_no = data->area_no;
  const float *area_co = data->area_co;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = ss->cache->bstrength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);
  plane_from_point_normal_v3(test.plane_tool, area_co, area_no);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }

    if (SCULPT_plane_point_side(vd.co, test.plane_tool)) {
      continue;
    }

    float intr[3];
    float val[3];
    closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);
    sub_v3_v3v3(val, intr, vd.co);

    if (!SCULPT_plane_trim(ss->cache, brush, val)) {
      continue;
    }

    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    mul_v3_v3fl(proxy[vd.i], val, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_scrape_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const float radius = ss->cache->radius;

  float area_no[3];
  float area_co[3];
  float offset = SCULPT_brush_plane_offset_get(sd, ss);

  float displace;

  float temp[3];

  SCULPT_calc_brush_plane(sd, ob, nodes, totnode, area_no, area_co);

  SCULPT_tilt_apply_to_normal(area_no, ss->cache, brush->tilt_strength_factor);

  displace = -radius * offset;

  mul_v3_v3v3(temp, area_no, ss->cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .area_no = area_no,
      .area_co = area_co,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_scrape_brush_task_cb_ex, &settings);
}

/* -------------------------------------------------------------------- */
/** \name Sculpt Clay Thumb Brush
 * \{ */

static void do_clay_thumb_brush_task_cb_ex(void *__restrict userdata,
                                           const int n,
                                           const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  float(*mat)[4] = data->mat;
  const float *area_no_sp = data->area_no_sp;
  const float *area_co = data->area_co;

  PBVHVertexIter vd;
  float(*proxy)[3];
  const float bstrength = data->clay_strength;

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  float plane_tilt[4];
  float normal_tilt[3];
  float imat[4][4];

  invert_m4_m4(imat, mat);
  rotate_v3_v3v3fl(normal_tilt, area_no_sp, imat[0], DEG2RADF(-ss->cache->clay_thumb_front_angle));

  /* Plane aligned to the geometry normal (back part of the brush). */
  plane_from_point_normal_v3(test.plane_tool, area_co, area_no_sp);
  /* Tilted plane (front part of the brush). */
  plane_from_point_normal_v3(plane_tilt, area_co, normal_tilt);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    float local_co[3];
    mul_v3_m4v3(local_co, mat, vd.co);
    float intr[3], intr_tilt[3];
    float val[3];

    closest_to_plane_normalized_v3(intr, test.plane_tool, vd.co);
    closest_to_plane_normalized_v3(intr_tilt, plane_tilt, vd.co);

    /* Mix the deformation of the aligned and the tilted plane based on the brush space vertex
     * coordinates. */
    /* We can also control the mix with a curve if it produces noticeable artifacts in the center
     * of the brush. */
    const float tilt_mix = local_co[1] > 0.0f ? 0.0f : 1.0f;
    interp_v3_v3v3(intr, intr, intr_tilt, tilt_mix);
    sub_v3_v3v3(val, intr_tilt, vd.co);

    const float fade = bstrength * SCULPT_brush_strength_factor(ss,
                                                                brush,
                                                                vd.co,
                                                                sqrtf(test.dist),
                                                                vd.no,
                                                                vd.fno,
                                                                vd.mask ? *vd.mask : 0.0f,
                                                                vd.vertex,
                                                                thread_id);

    mul_v3_v3fl(proxy[vd.i], val, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static float sculpt_clay_thumb_get_stabilized_pressure(StrokeCache *cache)
{
  float final_pressure = 0.0f;
  for (int i = 0; i < SCULPT_CLAY_STABILIZER_LEN; i++) {
    final_pressure += cache->clay_pressure_stabilizer[i];
  }
  return final_pressure / SCULPT_CLAY_STABILIZER_LEN;
}

static void do_clay_thumb_brush(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  const float radius = ss->cache->radius;
  const float offset = SCULPT_brush_plane_offset_get(sd, ss);
  const float displace = radius * (0.25f + offset);

  /* Sampled geometry normal and area center. */
  float area_no_sp[3];
  float area_no[3];
  float area_co[3];

  float temp[3];
  float mat[4][4];
  float scale[4][4];
  float tmat[4][4];

  SCULPT_calc_brush_plane(sd, ob, nodes, totnode, area_no_sp, area_co);

  if (brush->sculpt_plane != SCULPT_DISP_DIR_AREA || (brush->flag & BRUSH_ORIGINAL_NORMAL)) {
    SCULPT_calc_area_normal(sd, ob, nodes, totnode, area_no);
  }
  else {
    copy_v3_v3(area_no, area_no_sp);
  }

  /* Delay the first daub because grab delta is not setup. */
  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
    ss->cache->clay_thumb_front_angle = 0.0f;
    return;
  }

  /* Simulate the clay accumulation by increasing the plane angle as more samples are added to
   * the stroke. */
  if (SCULPT_stroke_is_main_symmetry_pass(ss->cache)) {
    ss->cache->clay_thumb_front_angle += 0.8f;
    ss->cache->clay_thumb_front_angle = clamp_f(ss->cache->clay_thumb_front_angle, 0.0f, 60.0f);
  }

  if (is_zero_v3(ss->cache->grab_delta_symmetry)) {
    return;
  }

  /* Displace the brush planes. */
  copy_v3_v3(area_co, ss->cache->location);
  mul_v3_v3v3(temp, area_no_sp, ss->cache->scale);
  mul_v3_fl(temp, displace);
  add_v3_v3(area_co, temp);

  /* Initialize brush local-space matrix. */
  cross_v3_v3v3(mat[0], area_no, ss->cache->grab_delta_symmetry);
  mat[0][3] = 0.0f;
  cross_v3_v3v3(mat[1], area_no, mat[0]);
  mat[1][3] = 0.0f;
  copy_v3_v3(mat[2], area_no);
  mat[2][3] = 0.0f;
  copy_v3_v3(mat[3], ss->cache->location);
  mat[3][3] = 1.0f;
  normalize_m4(mat);

  /* Scale brush local space matrix. */
  scale_m4_fl(scale, ss->cache->radius);
  mul_m4_m4m4(tmat, mat, scale);
  invert_m4_m4(mat, tmat);

  float clay_strength = ss->cache->bstrength *
                        sculpt_clay_thumb_get_stabilized_pressure(ss->cache);

  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .area_no_sp = area_no_sp,
      .area_co = ss->cache->location,
      .mat = mat,
      .clay_strength = clay_strength,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_clay_thumb_brush_task_cb_ex, &settings);
}

/** \} */

static void do_gravity_task_cb_ex(void *__restrict userdata,
                                  const int n,
                                  const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  const Brush *brush = data->brush;
  float *offset = data->offset;

  PBVHVertexIter vd;
  float(*proxy)[3];

  proxy = BKE_pbvh_node_add_proxy(ss->pbvh, data->nodes[n])->co;

  SculptBrushTest test;
  SculptBrushTestFn sculpt_brush_test_sq_fn = SCULPT_brush_test_init_with_falloff_shape(
      ss, &test, data->brush->falloff_shape);
  const int thread_id = BLI_task_parallel_thread_id(tls);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    if (!sculpt_brush_test_sq_fn(&test, vd.co)) {
      continue;
    }
    const float fade = SCULPT_brush_strength_factor(ss,
                                                    brush,
                                                    vd.co,
                                                    sqrtf(test.dist),
                                                    vd.no,
                                                    vd.fno,
                                                    vd.mask ? *vd.mask : 0.0f,
                                                    vd.vertex,
                                                    thread_id);

    mul_v3_v3fl(proxy[vd.i], offset, fade);

    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void do_gravity(Sculpt *sd, Object *ob, PBVHNode **nodes, int totnode, float bstrength)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  float offset[3];
  float gravity_vector[3];

  mul_v3_v3fl(gravity_vector, ss->cache->gravity_direction, -ss->cache->radius_squared);

  /* Offset with as much as possible factored in already. */
  mul_v3_v3v3(offset, gravity_vector, ss->cache->scale);
  mul_v3_fl(offset, bstrength);

  /* Threaded loop over nodes. */
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .offset = offset,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_gravity_task_cb_ex, &settings);
}

void SCULPT_vertcos_to_key(Object *ob, KeyBlock *kb, const float (*vertCos)[3])
{
  Mesh *me = (Mesh *)ob->data;
  float(*ofs)[3] = NULL;
  int a;
  const int kb_act_idx = ob->shapenr - 1;
  KeyBlock *currkey;

  /* For relative keys editing of base should update other keys. */
  if (BKE_keyblock_is_basis(me->key, kb_act_idx)) {
    ofs = BKE_keyblock_convert_to_vertcos(ob, kb);

    /* Calculate key coord offsets (from previous location). */
    for (a = 0; a < me->totvert; a++) {
      sub_v3_v3v3(ofs[a], vertCos[a], ofs[a]);
    }

    /* Apply offsets on other keys. */
    for (currkey = me->key->block.first; currkey; currkey = currkey->next) {
      if ((currkey != kb) && (currkey->relative == kb_act_idx)) {
        BKE_keyblock_update_from_offset(ob, currkey, ofs);
      }
    }

    MEM_freeN(ofs);
  }

  /* Modifying of basis key should update mesh. */
  if (kb == me->key->refkey) {
    MVert *mvert = me->mvert;

    for (a = 0; a < me->totvert; a++, mvert++) {
      copy_v3_v3(mvert->co, vertCos[a]);
    }

    BKE_mesh_calc_normals(me);
  }

  /* Apply new coords on active key block, no need to re-allocate kb->data here! */
  BKE_keyblock_update_from_vertcos(ob, kb, vertCos);
}

static void topology_undopush_cb(PBVHNode *node, void *data)
{
  SculptSearchSphereData *sdata = (SculptSearchSphereData *)data;

  SCULPT_ensure_dyntopo_node_undo(
      sdata->ob,
      node,
      sdata->brush->sculpt_tool == SCULPT_TOOL_MASK ? SCULPT_UNDO_MASK : SCULPT_UNDO_COORDS,
      0);

  BKE_pbvh_node_mark_update(node);
}

int SCULPT_get_symmetry_pass(const SculptSession *ss)
{
  int symidx = ss->cache->mirror_symmetry_pass + (ss->cache->radial_symmetry_pass * 8);

  if (symidx >= SCULPT_MAX_SYMMETRY_PASSES) {
    symidx = SCULPT_MAX_SYMMETRY_PASSES - 1;
  }

  return symidx;
}

typedef struct DynTopoAutomaskState {
  AutomaskingCache *cache;
  SculptSession *ss;
  AutomaskingCache _fixed;
  bool free_automasking;
} DynTopoAutomaskState;

static float sculpt_topology_automasking_cb(SculptVertRef vertex, void *vdata)
{
  DynTopoAutomaskState *state = (DynTopoAutomaskState *)vdata;
  float mask = SCULPT_automasking_factor_get(state->cache, state->ss, vertex);
  float mask2 = 1.0f - SCULPT_vertex_mask_get(state->ss, vertex);

  return mask * mask2;
}

static float sculpt_topology_automasking_mask_cb(SculptVertRef vertex, void *vdata)
{
  DynTopoAutomaskState *state = (DynTopoAutomaskState *)vdata;
  return 1.0f - SCULPT_vertex_mask_get(state->ss, vertex);
}

bool SCULPT_dyntopo_automasking_init(const SculptSession *ss,
                                     Sculpt *sd,
                                     const Brush *br,
                                     Object *ob,
                                     DyntopoMaskCB *r_mask_cb,
                                     void **r_mask_cb_data)
{
  if (!SCULPT_is_automasking_enabled(sd, ss, br)) {
    if (CustomData_has_layer(&ss->bm->vdata, CD_PAINT_MASK)) {
      DynTopoAutomaskState *state = MEM_callocN(sizeof(DynTopoAutomaskState),
                                                "DynTopoAutomaskState");

      if (!ss->cache) {
        state->cache = SCULPT_automasking_cache_init(sd, br, ob);
      }
      else {
        state->cache = ss->cache->automasking;
      }

      state->ss = (SculptSession *)ss;

      *r_mask_cb_data = (void *)state;
      *r_mask_cb = sculpt_topology_automasking_mask_cb;

      return true;
    }
    else {
      *r_mask_cb = NULL;
      *r_mask_cb_data = NULL;
      return false;
    }
  }

  DynTopoAutomaskState *state = MEM_callocN(sizeof(DynTopoAutomaskState), "DynTopoAutomaskState");
  if (!ss->cache) {
    state->cache = SCULPT_automasking_cache_init(sd, br, ob);
    state->free_automasking = true;
  }
  else {
    state->cache = ss->cache->automasking;
  }

  state->ss = (SculptSession *)ss;

  *r_mask_cb_data = (void *)state;
  *r_mask_cb = sculpt_topology_automasking_cb;

  return true;
}

void SCULPT_dyntopo_automasking_end(void *mask_data)
{
  MEM_SAFE_FREE(mask_data);
}

/* Note: we do the topology update before any brush actions to avoid
 * issues with the proxies. The size of the proxy can't change, so
 * topology must be updated first. */
static void sculpt_topology_update(Sculpt *sd,
                                   Object *ob,
                                   Brush *brush,
                                   UnifiedPaintSettings *UNUSED(ups))
{
  SculptSession *ss = ob->sculpt;

  /* build brush radius scale */
  float radius_scale = 1.0f;

  /* Build a list of all nodes that are potentially within the brush's area of influence. */
  const bool use_original = sculpt_tool_needs_original(brush->sculpt_tool) ? true :
                                                                             ss->cache->original;

  /* Free index based vertex info as it will become invalid after modifying the topology during
   * the stroke. */
  MEM_SAFE_FREE(ss->vertex_info.boundary);
  MEM_SAFE_FREE(ss->vertex_info.symmetrize_map);
  MEM_SAFE_FREE(ss->vertex_info.connected_component);

  PBVHTopologyUpdateMode mode = 0;
  float location[3];

  int dyntopo_mode = SCULPT_get_int(ss, dyntopo_mode, sd, brush);
  int dyntopo_detail_mode = SCULPT_get_int(ss, dyntopo_detail_mode, sd, brush);

  if (dyntopo_detail_mode != DYNTOPO_DETAIL_MANUAL) {
    if (dyntopo_mode & DYNTOPO_SUBDIVIDE) {
      mode |= PBVH_Subdivide;
    }
    else if (dyntopo_mode & DYNTOPO_LOCAL_SUBDIVIDE) {
      mode |= PBVH_LocalSubdivide | PBVH_Subdivide;
    }

    if (dyntopo_mode & DYNTOPO_COLLAPSE) {
      mode |= PBVH_Collapse;
    }
    else if (dyntopo_mode & DYNTOPO_LOCAL_COLLAPSE) {
      mode |= PBVH_LocalCollapse | PBVH_Collapse;
    }
  }

  if (dyntopo_mode & DYNTOPO_CLEANUP) {
    mode |= PBVH_Cleanup;
  }

  SculptSearchSphereData sdata = {
      .ss = ss,
      .sd = sd,
      .ob = ob,
      .radius_squared = square_f(ss->cache->radius * radius_scale * 1.25f),
      .original = use_original,
      .ignore_fully_ineffective = brush->sculpt_tool != SCULPT_TOOL_MASK,
      .center = NULL,
      .brush = brush};

  int symidx = SCULPT_get_symmetry_pass(ss);

  bool modified;
  void *mask_cb_data;
  DyntopoMaskCB mask_cb;

  BKE_pbvh_set_bm_log(ss->pbvh, ss->bm_log);

  SCULPT_dyntopo_automasking_init(ss, sd, brush, ob, &mask_cb, &mask_cb_data);

  /* do nodes under the brush cursor */
  modified = BKE_pbvh_bmesh_update_topology_nodes(
      ss->pbvh,
      SCULPT_search_sphere_cb,
      topology_undopush_cb,
      &sdata,
      mode,
      ss->cache->location,
      ss->cache->view_normal,
      ss->cache->radius * radius_scale,
      (brush->flag & BRUSH_FRONTFACE) != 0,
      (brush->falloff_shape != PAINT_FALLOFF_SHAPE_SPHERE),
      symidx,
      DYNTOPO_HAS_DYNAMIC_SPLIT(brush->sculpt_tool),
      mask_cb,
      mask_cb_data,
      SCULPT_get_int(ss, dyntopo_disable_smooth, sd, brush));

  SCULPT_dyntopo_automasking_end(mask_cb_data);

  /* Update average stroke position. */
  copy_v3_v3(location, ss->cache->true_location);
  mul_m4_v3(ob->obmat, location);

  ss->totfaces = ss->totpoly = ss->bm->totface;
  ss->totvert = ss->bm->totvert;
}

static void do_brush_action_task_cb(void *__restrict userdata,
                                    const int n,
                                    const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;

  /* Face Sets modifications do a single undo push */
  if (data->brush->sculpt_tool == SCULPT_TOOL_DRAW_FACE_SETS) {
    BKE_pbvh_node_mark_redraw(data->nodes[n]);
    /* Draw face sets in smooth mode moves the vertices. */
    if (ss->cache->alt_smooth) {
      SCULPT_undo_push_node(data->ob, data->nodes[n], SCULPT_UNDO_COORDS);
      BKE_pbvh_node_mark_update(data->nodes[n]);
    }
  }
  else if (data->brush->sculpt_tool == SCULPT_TOOL_ARRAY) {
    /* Do nothing, array brush does a single geometry undo push. */
  }
  else if (data->brush->sculpt_tool == SCULPT_TOOL_MASK) {
    SCULPT_undo_push_node(data->ob, data->nodes[n], SCULPT_UNDO_MASK);
    BKE_pbvh_node_mark_update_mask(data->nodes[n]);
  }
  else if (ELEM(data->brush->sculpt_tool, SCULPT_TOOL_PAINT, SCULPT_TOOL_SMEAR)) {
    if (!ss->bm) {
      if (data->brush->vcol_boundary_factor > 0.0f) {
        SCULPT_undo_push_node(data->ob, data->nodes[n], SCULPT_UNDO_COORDS);
      }

      SCULPT_undo_push_node(data->ob, data->nodes[n], SCULPT_UNDO_COLOR);
    }

    BKE_pbvh_node_mark_update_color(data->nodes[n]);
  }
  else {
    if (!ss->bm) {
      SCULPT_undo_push_node(data->ob, data->nodes[n], SCULPT_UNDO_COORDS);
    }
    BKE_pbvh_node_mark_update(data->nodes[n]);
  }
}

void do_brush_action(Sculpt *sd, Object *ob, Brush *brush, UnifiedPaintSettings *ups)
{
  SculptSession *ss = ob->sculpt;

  // poor ELEM macro. . .
  bool ok = false;

  switch (brush->sculpt_tool) {
    case SCULPT_TOOL_DRAW:
    case SCULPT_TOOL_DRAW_SHARP:
    case SCULPT_TOOL_CLAY_STRIPS:
    case SCULPT_TOOL_CLAY:
    case SCULPT_TOOL_CREASE:
    case SCULPT_TOOL_ROTATE:
    case SCULPT_TOOL_ELASTIC_DEFORM:
    case SCULPT_TOOL_FAIRING:
    case SCULPT_TOOL_FILL:
    case SCULPT_TOOL_BLOB:
    case SCULPT_TOOL_FLATTEN:
    case SCULPT_TOOL_GRAB:
    case SCULPT_TOOL_LAYER:
    case SCULPT_TOOL_DRAW_FACE_SETS:
    case SCULPT_TOOL_CLOTH:
    case SCULPT_TOOL_SMOOTH:
    case SCULPT_TOOL_SIMPLIFY:
    case SCULPT_TOOL_SNAKE_HOOK:
    case SCULPT_TOOL_INFLATE:
    case SCULPT_TOOL_PAINT:
    case SCULPT_TOOL_SMEAR:
      ok = true;
      break;
  }

  ok = ok && !(brush->flag & (BRUSH_ANCHORED | BRUSH_DRAG_DOT));

  if (ss->cache->alt_smooth && brush->sculpt_tool == SCULPT_TOOL_SMOOTH) {
    float factor = BRUSHSET_GET_FLOAT(ss->cache->channels_final, smooth_strength_factor, NULL);
    float projection = BRUSHSET_GET_FLOAT(
        ss->cache->channels_final, smooth_strength_projection, NULL);

    BRUSHSET_SET_FLOAT(ss->cache->channels_final, strength, factor);
    BRUSHSET_SET_FLOAT(ss->cache->channels_final, projection, projection);
  }

  if (ok) {
    if (SCULPT_stroke_is_first_brush_step(ss->cache)) {
      if (ss->cache->commandlist) {
        BKE_brush_commandlist_free(ss->cache->commandlist);
      }

      BrushCommandList *list = ss->cache->commandlist = BKE_brush_commandlist_create();

      BKE_builtin_commandlist_create(
          brush, ss->cache->channels_final, list, brush->sculpt_tool, &ss->cache->input_mapping);
    }

    SCULPT_run_command_list(sd, ob, brush, ss->cache->commandlist, ups);

    float location[3];

    /* Update average stroke position. */
    copy_v3_v3(location, ss->cache->true_location);
    mul_m4_v3(ob->obmat, location);

    add_v3_v3(ups->average_stroke_accum, location);
    ups->average_stroke_counter++;
    /* Update last stroke position. */
    ups->last_stroke_valid = true;

    return;
  }

  int totnode;
  PBVHNode **nodes;

  float radius_scale = 1.0f;

  if (!ELEM(brush->sculpt_tool, SCULPT_TOOL_SMOOTH, SCULPT_TOOL_MASK) &&
      brush->autosmooth_factor > 0 && brush->autosmooth_radius_factor != 1.0f) {
    radius_scale = MAX2(radius_scale, brush->autosmooth_radius_factor);
  }

  if (sculpt_brush_use_topology_rake(ss, brush)) {
    radius_scale = MAX2(radius_scale, brush->topology_rake_radius_factor);
  }

  /* Check for unsupported features. */
  PBVHType type = BKE_pbvh_type(ss->pbvh);
  if (ELEM(brush->sculpt_tool, SCULPT_TOOL_PAINT, SCULPT_TOOL_SMEAR) &&
      !ELEM(type, PBVH_BMESH, PBVH_FACES)) {
    return;
  }

  if (brush->sculpt_tool == SCULPT_TOOL_ARRAY && !ELEM(type, PBVH_FACES, PBVH_BMESH)) {
    return;
  }

  /* Build a list of all nodes that are potentially within the brush's area of influence */
  const bool use_original = sculpt_tool_needs_original(brush->sculpt_tool) ? true :
                                                                             ss->cache->original;

  if (SCULPT_tool_needs_all_pbvh_nodes(brush)) {
    /* These brushes need to update all nodes as they are not constrained by the brush radius */
    BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);
  }
  else if (brush->sculpt_tool == SCULPT_TOOL_CLOTH) {
    nodes = SCULPT_cloth_brush_affected_nodes_gather(ss, brush, &totnode);
  }
  else {
    /* With these options enabled not all required nodes are inside the original brush radius, so
     * the brush can produce artifacts in some situations. */
    if (brush->sculpt_tool == SCULPT_TOOL_DRAW && brush->flag & BRUSH_ORIGINAL_NORMAL) {
      radius_scale = MAX2(radius_scale, 2.0f);
    }
    nodes = sculpt_pbvh_gather_generic(ob, sd, brush, use_original, radius_scale * 1.2, &totnode);
  }

  /* Draw Face Sets in draw mode makes a single undo push, in alt-smooth mode deforms the
   * vertices and uses regular coords undo. */
  /* It also assigns the paint_face_set here as it needs to be done regardless of the stroke type
   * and the number of nodes under the brush influence. */
  if (brush->sculpt_tool == SCULPT_TOOL_DRAW_FACE_SETS &&
      SCULPT_stroke_is_first_brush_step(ss->cache) && !ss->cache->alt_smooth) {

    // faceset undo node is created below for pbvh_bmesh
    if (BKE_pbvh_type(ss->pbvh) != PBVH_BMESH) {
      SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_FACE_SETS);
    }

    if (ss->cache->invert) {
      /* When inverting the brush, pick the paint face mask ID from the mesh. */
      ss->cache->paint_face_set = SCULPT_active_face_set_get(ss);
    }
    else {
      /* By default create a new Face Sets. */
      ss->cache->paint_face_set = SCULPT_face_set_next_available_get(ss);
    }
  }

  /*
   * Check that original data is for anchored and drag dot modes
   */
  if (brush->flag & (BRUSH_ANCHORED | BRUSH_DRAG_DOT)) {
    if (SCULPT_stroke_is_first_brush_step(ss->cache) &&
        brush->sculpt_tool == SCULPT_TOOL_DRAW_FACE_SETS) {

      SCULPT_face_ensure_original(ss);

      for (int i = 0; i < ss->totfaces; i++) {
        SculptFaceRef face = BKE_pbvh_table_index_to_face(ss->pbvh, i);
        SCULPT_face_check_origdata(ss, face);
      }
    }

    for (int i = 0; i < totnode; i++) {
      PBVHVertexIter vd;

      BKE_pbvh_vertex_iter_begin (ss->pbvh, nodes[i], vd, PBVH_ITER_UNIQUE) {
        SCULPT_vertex_check_origdata(ss, vd.vertex);
      }
      BKE_pbvh_vertex_iter_end;
    }
  }

  /* Initialize automasking cache. For anchored brushes with spherical falloff, we start off with
   * zero radius, thus we have no pbvh nodes on the first brush step. */
  if (totnode ||
      ((brush->falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) && (brush->flag & BRUSH_ANCHORED))) {
    if (SCULPT_stroke_is_first_brush_step(ss->cache)) {
      if (SCULPT_is_automasking_enabled(sd, ss, brush)) {
        ss->cache->automasking = SCULPT_automasking_cache_init(sd, brush, ob);
      }
    }
  }

  /* Only act if some verts are inside the brush area. */
  if (totnode == 0) {
    return;
  }
  float location[3];

  // dyntopo can't push undo nodes inside a thread
  if (ss->bm) {
    if (ELEM(brush->sculpt_tool, SCULPT_TOOL_PAINT, SCULPT_TOOL_SMEAR)) {
      for (int i = 0; i < totnode; i++) {
        int other = brush->vcol_boundary_factor > 0.0f ? SCULPT_UNDO_COORDS : -1;

        SCULPT_ensure_dyntopo_node_undo(ob, nodes[i], SCULPT_UNDO_COLOR, other);
        BKE_pbvh_node_mark_update_color(nodes[i]);
      }
    }
    else if (brush->sculpt_tool == SCULPT_TOOL_DRAW_FACE_SETS) {
      for (int i = 0; i < totnode; i++) {
        if (ss->cache->alt_smooth) {
          SCULPT_ensure_dyntopo_node_undo(ob, nodes[i], SCULPT_UNDO_FACE_SETS, SCULPT_UNDO_COORDS);
        }
        else {
          SCULPT_ensure_dyntopo_node_undo(ob, nodes[i], SCULPT_UNDO_FACE_SETS, -1);
        }

        BKE_pbvh_node_mark_update(nodes[i]);
      }
    }
    else if (brush->sculpt_tool != SCULPT_TOOL_ARRAY) {
      for (int i = 0; i < totnode; i++) {
        SCULPT_ensure_dyntopo_node_undo(ob, nodes[i], SCULPT_UNDO_COORDS, -1);

        BKE_pbvh_node_mark_update(nodes[i]);
      }
    }
  }
  else {
    SculptThreadedTaskData task_data = {
        .sd = sd,
        .ob = ob,
        .brush = brush,
        .nodes = nodes,
    };

    TaskParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, true, totnode);
    BLI_task_parallel_range(0, totnode, &task_data, do_brush_action_task_cb, &settings);
  }

  if (sculpt_brush_needs_normal(ss, brush)) {
    update_sculpt_normal(sd, ob, nodes, totnode);
  }

  if (brush->mtex.brush_map_mode == MTEX_MAP_MODE_AREA) {
    update_brush_local_mat(sd, ob);
  }

  if (brush->sculpt_tool == SCULPT_TOOL_POSE && SCULPT_stroke_is_first_brush_step(ss->cache)) {
    SCULPT_pose_brush_init(sd, ob, ss, brush);
  }

  if (brush->deform_target == BRUSH_DEFORM_TARGET_CLOTH_SIM) {
    if (!ss->cache->cloth_sim) {
      ss->cache->cloth_sim = SCULPT_cloth_brush_simulation_create(
          ss, 1.0f, 1.0f, 0.0f, false, true);
      SCULPT_cloth_brush_simulation_init(ss, ss->cache->cloth_sim);
    }
    SCULPT_cloth_brush_store_simulation_state(ss, ss->cache->cloth_sim);
    SCULPT_cloth_brush_ensure_nodes_constraints(
        sd, ob, nodes, totnode, ss->cache->cloth_sim, ss->cache->location, FLT_MAX);
  }

  bool invert = ss->cache->pen_flip || ss->cache->invert || brush->flag & BRUSH_DIR_IN;

  SCULPT_replay_log_append(sd, ss, ob);

  /* Apply one type of brush action. */
  switch (brush->sculpt_tool) {
    case SCULPT_TOOL_DRAW:
      do_draw_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_SMOOTH:
      if (brush->smooth_deform_type == BRUSH_SMOOTH_DEFORM_LAPLACIAN) {
        SCULPT_do_smooth_brush(
            sd,
            ob,
            nodes,
            totnode,
            brush->autosmooth_projection,
            SCULPT_stroke_needs_original(
                brush));  // TODO: extract need original from commandlist and not parent brush
      }
      else if (brush->smooth_deform_type == BRUSH_SMOOTH_DEFORM_SURFACE) {
        SCULPT_do_surface_smooth_brush(sd, ob, nodes, totnode);
      }
      else if (brush->smooth_deform_type == BRUSH_SMOOTH_DEFORM_DIRECTIONAL) {
        SCULPT_do_directional_smooth_brush(sd, ob, nodes, totnode);
      }
      else if (brush->smooth_deform_type == BRUSH_SMOOTH_DEFORM_UNIFORM_WEIGHTS) {
        SCULPT_do_uniform_weights_smooth_brush(sd, ob, nodes, totnode);
      }
      break;
    case SCULPT_TOOL_CREASE:
      do_crease_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_BLOB:
      do_crease_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_PINCH:
      do_pinch_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_INFLATE:
      do_inflate_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_GRAB:
      do_grab_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_ROTATE:
      do_rotate_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_SNAKE_HOOK:
      do_snake_hook_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_NUDGE:
      do_nudge_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_THUMB:
      do_thumb_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_LAYER:
      do_layer_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_FLATTEN:
      do_flatten_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_CLAY:
      do_clay_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_CLAY_STRIPS:
      do_clay_strips_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_TWIST:
      do_twist_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_MULTIPLANE_SCRAPE:
      SCULPT_do_multiplane_scrape_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_CLAY_THUMB:
      do_clay_thumb_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_FILL:
      if (invert && brush->flag & BRUSH_INVERT_TO_SCRAPE_FILL) {
        do_scrape_brush(sd, ob, nodes, totnode);
      }
      else {
        do_fill_brush(sd, ob, nodes, totnode);
      }
      break;
    case SCULPT_TOOL_SCRAPE:
      if (invert && brush->flag & BRUSH_INVERT_TO_SCRAPE_FILL) {
        do_fill_brush(sd, ob, nodes, totnode);
      }
      else {
        do_scrape_brush(sd, ob, nodes, totnode);
      }
      break;
    case SCULPT_TOOL_MASK:
      do_mask_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_POSE:
      SCULPT_do_pose_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_DRAW_SHARP:
      do_draw_sharp_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_ELASTIC_DEFORM:
      do_elastic_deform_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_SLIDE_RELAX:
      do_slide_relax_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_BOUNDARY:
      SCULPT_do_boundary_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_CLOTH:
      SCULPT_do_cloth_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_DRAW_FACE_SETS:
      SCULPT_do_draw_face_sets_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_DISPLACEMENT_ERASER:
      do_displacement_eraser_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_DISPLACEMENT_SMEAR:
      do_displacement_smear_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_PAINT:
      SCULPT_do_paint_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_SMEAR:
      SCULPT_do_smear_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_FAIRING:
      do_fairing_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_SCENE_PROJECT:
      do_scene_project_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_SYMMETRIZE:
      SCULPT_do_symmetrize_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_ARRAY:
      SCULPT_do_array_brush(sd, ob, nodes, totnode);
    case SCULPT_TOOL_VCOL_BOUNDARY:
      SCULPT_smooth_vcol_boundary(sd, ob, nodes, totnode, ss->cache->bstrength);
      break;
    case SCULPT_TOOL_UV_SMOOTH:
      SCULPT_uv_brush(sd, ob, nodes, totnode);
      break;
  }

  bool apply_autosmooth =
      !ELEM(brush->sculpt_tool, SCULPT_TOOL_BOUNDARY, SCULPT_TOOL_SMOOTH, SCULPT_TOOL_MASK) &&
      brush->autosmooth_factor > 0;

  if (brush->flag2 & BRUSH_CUSTOM_AUTOSMOOTH_SPACING) {
    float spacing = (float)brush->autosmooth_spacing / 100.0f;

    apply_autosmooth = apply_autosmooth &&
                       paint_stroke_apply_subspacing(
                           ss->cache->stroke,
                           spacing,
                           PAINT_MODE_SCULPT,
                           &ss->cache->last_smooth_t[SCULPT_get_symmetry_pass(ss)]);
  }

  if (apply_autosmooth) {
    float start_radius = ss->cache->radius;

    if (brush->autosmooth_radius_factor != 1.0f) {
      // note that we expanded the pbvh node search radius earlier in this function
      // so we just have to adjust the brush radius that's inside ss->cache

      ss->cache->radius *= brush->autosmooth_radius_factor;
      ss->cache->radius_squared = ss->cache->radius * ss->cache->radius;
    }

    if (brush->flag & BRUSH_INVERSE_SMOOTH_PRESSURE) {
      SCULPT_smooth(sd,
                    ob,
                    nodes,
                    totnode,
                    brush->autosmooth_factor * (1.0f - ss->cache->pressure),
                    false,
                    brush->autosmooth_projection,
                    false);
    }
    else {
      SCULPT_smooth(sd,
                    ob,
                    nodes,
                    totnode,
                    brush->autosmooth_factor,
                    false,
                    brush->autosmooth_projection,
                    false);
    }

    if (brush->autosmooth_radius_factor != 1.0f) {
      ss->cache->radius = start_radius;
      ss->cache->radius_squared = ss->cache->radius * ss->cache->radius;
    }
  }

  bool use_topology_rake = sculpt_brush_use_topology_rake(ss, brush);

  if (brush->flag2 & BRUSH_CUSTOM_TOPOLOGY_RAKE_SPACING) {
    float spacing = (float)brush->topology_rake_spacing / 100.0f;

    use_topology_rake = use_topology_rake &&
                        paint_stroke_apply_subspacing(
                            ss->cache->stroke,
                            spacing,
                            PAINT_MODE_SCULPT,
                            &ss->cache->last_rake_t[SCULPT_get_symmetry_pass(ss)]);
  }

  if (use_topology_rake) {
    float start_radius = ss->cache->radius;

    if (brush->topology_rake_radius_factor != 1.0f) {
      // note that we expanded the pbvh node search radius earlier in this function
      // so we just have to adjust the brush radius that's inside ss->cache

      ss->cache->radius *= brush->topology_rake_radius_factor;
      ss->cache->radius_squared = ss->cache->radius * ss->cache->radius;
    }

    bmesh_topology_rake(
        sd, ob, nodes, totnode, brush->topology_rake_factor, SCULPT_stroke_needs_original(brush));

    if (brush->topology_rake_radius_factor != 1.0f) {
      ss->cache->radius = start_radius;
      ss->cache->radius_squared = ss->cache->radius * ss->cache->radius;
    }
  }

  /* The cloth brush adds the gravity as a regular force and it is processed in the solver. */
  if (ss->cache->supports_gravity && !ELEM(brush->sculpt_tool,
                                           SCULPT_TOOL_CLOTH,
                                           SCULPT_TOOL_DRAW_FACE_SETS,
                                           SCULPT_TOOL_BOUNDARY)) {
    do_gravity(sd, ob, nodes, totnode, sd->gravity_factor);
  }

  if (brush->deform_target == BRUSH_DEFORM_TARGET_CLOTH_SIM) {
    if (SCULPT_stroke_is_main_symmetry_pass(ss->cache)) {
      SCULPT_cloth_sim_activate_nodes(ss->cache->cloth_sim, nodes, totnode);
      SCULPT_cloth_brush_do_simulation_step(sd, ob, ss->cache->cloth_sim, nodes, totnode);
    }
  }

  MEM_SAFE_FREE(nodes);

  /* Update average stroke position. */
  copy_v3_v3(location, ss->cache->true_location);
  mul_m4_v3(ob->obmat, location);

  add_v3_v3(ups->average_stroke_accum, location);
  ups->average_stroke_counter++;
  /* Update last stroke position. */
  ups->last_stroke_valid = true;
}

void BKE_brush_commandlist_start(BrushCommandList *list,
                                 Brush *brush,
                                 BrushChannelSet *chset_final);

static void SCULPT_run_command_list(
    Sculpt *sd, Object *ob, Brush *brush, BrushCommandList *list, UnifiedPaintSettings *ups)
{
  SculptSession *ss = ob->sculpt;

  int totnode;
  PBVHNode **nodes;

  float start_radius = ss->cache->radius;

  float radius_scale = 1.0f;
  float radius_max = 0.0f;

  BKE_brush_commandlist_start(list, brush, ss->cache->channels_final);

  // this does a more high-level check then SCULPT_TOOL_HAS_DYNTOPO;
  bool has_dyntopo = ss->bm && SCULPT_stroke_is_dynamic_topology(ss, brush);
  bool all_nodes_undo = false;
  bool cloth_nodes_undo = false;

  // get maximum radius
  for (int i = 0; i < list->totcommand; i++) {
    BrushCommand *cmd = list->commands + i;

    Brush brush2 = *brush;
    brush2.sculpt_tool = cmd->tool;

    // Load parameters into brush2 for compatibility with old code
    BKE_brush_channelset_compat_load(cmd->params_final, &brush2, false);

    ss->cache->brush = &brush2;

    if (cmd->tool == SCULPT_TOOL_SMOOTH) {
      ss->cache->bstrength = brush2.alpha;

      if (ss->cache->invert && BRUSHSET_GET_INT(cmd->params_final, use_ctrl_invert, NULL)) {
        ss->cache->bstrength = -ss->cache->bstrength;
      }
    }
    else {
      ss->cache->bstrength = brush_strength(
          sd, ss->cache, calc_symmetry_feather(sd, ss->cache), ups);
    }

    brush2.alpha = fabs(ss->cache->bstrength);

    /* With these options enabled not all required nodes are inside the original brush radius, so
     * the brush can produce artifacts in some situations. */
    if (cmd->tool == SCULPT_TOOL_DRAW && BKE_brush_channelset_get_int(cmd->params_final,
                                                                      "original_normal",
                                                                      &ss->cache->input_mapping)) {
      radius_scale = MAX2(radius_scale, 2.0f);
    }

    if (SCULPT_tool_needs_all_pbvh_nodes(&brush2)) {
      all_nodes_undo = true;
    }
    else if (cmd->tool == SCULPT_TOOL_CLOTH) {
      cloth_nodes_undo = true;
    }

    if (!SCULPT_TOOL_HAS_DYNTOPO(cmd->tool)) {
      has_dyntopo = false;
    }

    float radius = BRUSHSET_GET_FLOAT(
        ss->cache->channels_final, radius, &ss->cache->input_mapping);
    radius = paint_calc_object_space_radius(ss->cache->vc, ss->cache->location, radius);

    radius_max = max_ff(radius_max, radius);
    ss->cache->brush = brush;
  }

  ss->cache->radius = radius_max;
  ss->cache->radius_squared = radius_max * radius_max;

  /* Check for unsupported features. */
  PBVHType type = BKE_pbvh_type(ss->pbvh);
  if (ELEM(brush->sculpt_tool, SCULPT_TOOL_PAINT, SCULPT_TOOL_SMEAR) &&
      !ELEM(type, PBVH_BMESH, PBVH_FACES)) {
    return;
  }

  if (brush->sculpt_tool == SCULPT_TOOL_ARRAY && !ELEM(type, PBVH_FACES, PBVH_BMESH)) {
    return;
  }

  /* Build a list of all nodes that are potentially within the brush's area of influence */
  const bool use_original = sculpt_tool_needs_original(brush->sculpt_tool) ? true :
                                                                             ss->cache->original;

  if (all_nodes_undo) {
    /* These brushes need to update all nodes as they are not constrained by the brush radius */
    BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);
  }
  else if (cloth_nodes_undo) {
    //
    float initial_radius = ss->cache->initial_radius;
    float old_radius = ss->cache->radius;

    float radius = ss->cache->initial_radius;

    radius = max_ff(radius, ss->cache->radius);
    radius *= radius_scale;

    ss->cache->initial_radius = radius;
    ss->cache->radius = radius;
    ss->cache->radius_squared = radius * radius;

    nodes = SCULPT_cloth_brush_affected_nodes_gather(ss, brush, &totnode);

    ss->cache->initial_radius = initial_radius;
    ss->cache->radius = old_radius;
    ss->cache->radius_squared = old_radius * old_radius;
  }
  else {
    nodes = sculpt_pbvh_gather_generic(ob, sd, brush, use_original, radius_scale * 1.2, &totnode);
  }

  /* Draw Face Sets in draw mode makes a single undo push, in alt-smooth mode deforms the
   * vertices and uses regular coords undo. */
  /* It also assigns the paint_face_set here as it needs to be done regardless of the stroke type
   * and the number of nodes under the brush influence. */
  if (brush->sculpt_tool == SCULPT_TOOL_DRAW_FACE_SETS &&
      SCULPT_stroke_is_first_brush_step(ss->cache) && !ss->cache->alt_smooth) {

    // faceset undo node is created below for pbvh_bmesh
    if (BKE_pbvh_type(ss->pbvh) != PBVH_BMESH) {
      SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_FACE_SETS);
    }

    if (ss->cache->invert) {
      /* When inverting the brush, pick the paint face mask ID from the mesh. */
      ss->cache->paint_face_set = SCULPT_active_face_set_get(ss);
    }
    else {
      /* By default create a new Face Sets. */
      ss->cache->paint_face_set = SCULPT_face_set_next_available_get(ss);
    }
  }

  /* Initialize automasking cache. For anchored brushes with spherical falloff, we start off with
   * zero radius, thus we have no pbvh nodes on the first brush step. */
  if (totnode ||
      ((brush->falloff_shape == PAINT_FALLOFF_SHAPE_SPHERE) && (brush->flag & BRUSH_ANCHORED))) {
    if (SCULPT_stroke_is_first_brush_step(ss->cache)) {
      if (SCULPT_is_automasking_enabled(sd, ss, brush)) {
        ss->cache->automasking = SCULPT_automasking_cache_init(sd, brush, ob);
      }
    }
  }

  /* Only act if some verts are inside the brush area. */
  if (totnode == 0) {
    ss->cache->radius = start_radius;
    ss->cache->radius_squared = start_radius * start_radius;

    return;
  }

  // dyntopo can't push undo nodes inside a thread
  if (ss->bm) {
    if (ELEM(brush->sculpt_tool, SCULPT_TOOL_PAINT, SCULPT_TOOL_SMEAR)) {
      for (int i = 0; i < totnode; i++) {
        int other = brush->vcol_boundary_factor > 0.0f ? SCULPT_UNDO_COORDS : -1;

        SCULPT_ensure_dyntopo_node_undo(ob, nodes[i], SCULPT_UNDO_COLOR, other);
        BKE_pbvh_node_mark_update_color(nodes[i]);
      }
    }
    else if (brush->sculpt_tool == SCULPT_TOOL_DRAW_FACE_SETS) {
      for (int i = 0; i < totnode; i++) {
        if (ss->cache->alt_smooth) {
          SCULPT_ensure_dyntopo_node_undo(ob, nodes[i], SCULPT_UNDO_FACE_SETS, SCULPT_UNDO_COORDS);
        }
        else {
          SCULPT_ensure_dyntopo_node_undo(ob, nodes[i], SCULPT_UNDO_FACE_SETS, -1);
        }

        BKE_pbvh_node_mark_update(nodes[i]);
      }
    }
    else {
      for (int i = 0; i < totnode; i++) {
        SCULPT_ensure_dyntopo_node_undo(ob, nodes[i], SCULPT_UNDO_COORDS, -1);

        BKE_pbvh_node_mark_update(nodes[i]);
      }
    }
  }
  else {
    SculptThreadedTaskData task_data = {
        .sd = sd,
        .ob = ob,
        .brush = brush,
        .nodes = nodes,
    };

    TaskParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, true, totnode);
    BLI_task_parallel_range(0, totnode, &task_data, do_brush_action_task_cb, &settings);
  }

  Brush *oldbrush = ss->cache->brush;
  Brush _dummy = *brush, *brush2 = &_dummy;

  for (int step = 0; step < list->totcommand; step++) {
    BrushCommand *cmd = list->commands + step;

    BKE_brush_channelset_free(cmd->params_mapped);
    cmd->params_mapped = BKE_brush_channelset_copy(cmd->params_final);
    BKE_brush_channelset_apply_mapping(cmd->params_mapped, &ss->cache->input_mapping);
    BKE_brush_channelset_clear_inherit(cmd->params_mapped);

    float radius = BRUSHSET_GET_FLOAT(cmd->params_mapped, radius, NULL);
    radius = paint_calc_object_space_radius(ss->cache->vc, ss->cache->location, radius);

    ss->cache->radius = radius;
    ss->cache->radius_squared = radius * radius;
    ss->cache->initial_radius = radius;

    radius_scale = 1.0f;

    float spacing = BRUSHSET_GET_FLOAT(cmd->params_mapped, spacing, NULL) / 100.0f;

#if 0
    printf("p %f s %f\n",
           ss->cache->input_mapping.pressure,
           spacing);  //// cmd->last_spacing_t[SCULPT_get_symmetry_pass(ss)]);
#endif
    bool noskip = paint_stroke_apply_subspacing(
        ss->cache->stroke,
        spacing,
        PAINT_MODE_SCULPT,
        &cmd->last_spacing_t[SCULPT_get_symmetry_pass(ss)]);

    if (!noskip) {
      continue;
    }

    *brush2 = *brush;

    BrushChannel *ch = BRUSHSET_LOOKUP(cmd->params_final, falloff_curve);
    if (ch) {
      brush2->curve_preset = ch->curve.preset;
      brush2->curve = ch->curve.curve;
    }

    // Load parameters into brush2 for compatibility with old code
    // Make sure to remove all pen pressure/tilt old code
    BKE_brush_channelset_compat_load(cmd->params_mapped, brush2, false);

    ss->cache->use_plane_trim = BRUSHSET_GET_INT(cmd->params_mapped, use_plane_trim, NULL);
    float plane_trim = BRUSHSET_GET_FLOAT(cmd->params_mapped, plane_trim, NULL);
    ss->cache->plane_trim_squared = plane_trim * plane_trim;

    brush2->flag &= ~(BRUSH_ALPHA_PRESSURE | BRUSH_SIZE_PRESSURE | BRUSH_SPACING_PRESSURE |
                      BRUSH_JITTER_PRESSURE | BRUSH_OFFSET_PRESSURE |
                      BRUSH_INVERSE_SMOOTH_PRESSURE);
    brush2->flag2 &= ~BRUSH_AREA_RADIUS_PRESSURE;

    brush2->sculpt_tool = cmd->tool;
    BrushChannelSet *channels_final = ss->cache->channels_final;
    ss->cache->channels_final = cmd->params_mapped;

    ss->cache->brush = brush2;

    ups->alpha = BRUSHSET_GET_FLOAT(cmd->params_mapped, strength, NULL);
    if (cmd->tool == SCULPT_TOOL_SMOOTH) {
      ss->cache->bstrength = brush2->alpha;
    }
    else {
      ss->cache->bstrength = brush_strength(
          sd, ss->cache, calc_symmetry_feather(sd, ss->cache), ups);
    }

    if (!BRUSHSET_GET_INT(cmd->params_mapped, use_ctrl_invert, NULL)) {
      ss->cache->bstrength = fabsf(ss->cache->bstrength);
    }
    // brush2->alpha = fabs(ss->cache->bstrength);

    // printf("brush2->alpha: %f\n", brush2->alpha);
    // printf("ss->cache->bstrength: %f\n", ss->cache->bstrength);

    /*Search PBVH*/
    if (step > 0) {
      MEM_SAFE_FREE(nodes);

      if (SCULPT_tool_needs_all_pbvh_nodes(brush)) {
        /* These brushes need to update all nodes as they are not constrained by the brush radius
         */
        BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);
      }
      else if (brush->sculpt_tool == SCULPT_TOOL_CLOTH) {
        nodes = SCULPT_cloth_brush_affected_nodes_gather(ss, brush, &totnode);
      }
      else {
        /* With these options enabled not all required nodes are inside the original brush radius,
         * so the brush can produce artifacts in some situations. */
        if (brush->sculpt_tool == SCULPT_TOOL_DRAW && brush->flag & BRUSH_ORIGINAL_NORMAL) {
          radius_scale = MAX2(radius_scale, 2.0f);
        }
        nodes = sculpt_pbvh_gather_generic(ob, sd, brush, use_original, radius_scale, &totnode);
      }
    }
    if (sculpt_brush_needs_normal(ss, brush2)) {
      update_sculpt_normal(sd, ob, nodes, totnode);
    }
    if (brush2->mtex.brush_map_mode == MTEX_MAP_MODE_AREA) {
      update_brush_local_mat(sd, ob);
    }
    if (brush2->sculpt_tool == SCULPT_TOOL_POSE && SCULPT_stroke_is_first_brush_step(ss->cache)) {
      SCULPT_pose_brush_init(sd, ob, ss, brush2);
    }

    if (brush2->deform_target == BRUSH_DEFORM_TARGET_CLOTH_SIM) {
      if (!ss->cache->cloth_sim) {
        ss->cache->cloth_sim = SCULPT_cloth_brush_simulation_create(
            ss, 1.0f, 1.0f, 0.0f, false, true);
        SCULPT_cloth_brush_simulation_init(ss, ss->cache->cloth_sim);
      }
      SCULPT_cloth_brush_store_simulation_state(ss, ss->cache->cloth_sim);
      SCULPT_cloth_brush_ensure_nodes_constraints(
          sd, ob, nodes, totnode, ss->cache->cloth_sim, ss->cache->location, FLT_MAX);
    }

    bool invert = ss->cache->pen_flip || ss->cache->invert || brush2->flag & BRUSH_DIR_IN;
    SCULPT_replay_log_append(sd, ss, ob);

    /* Apply one type of brush action. */
    switch (brush2->sculpt_tool) {
      case SCULPT_TOOL_DRAW:
        do_draw_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_SMOOTH:
        if (brush2->smooth_deform_type == BRUSH_SMOOTH_DEFORM_LAPLACIAN) {
          SCULPT_do_smooth_brush(sd,
                                 ob,
                                 nodes,
                                 totnode,
                                 BRUSHSET_GET_FLOAT(cmd->params_mapped, projection, NULL),
                                 SCULPT_stroke_needs_original(brush));
        }
        else if (brush2->smooth_deform_type == BRUSH_SMOOTH_DEFORM_SURFACE) {
          SCULPT_do_surface_smooth_brush(sd, ob, nodes, totnode);
        }
        else if (brush2->smooth_deform_type == BRUSH_SMOOTH_DEFORM_DIRECTIONAL) {
          SCULPT_do_directional_smooth_brush(sd, ob, nodes, totnode);
        }
        else if (brush2->smooth_deform_type == BRUSH_SMOOTH_DEFORM_UNIFORM_WEIGHTS) {
          SCULPT_do_uniform_weights_smooth_brush(sd, ob, nodes, totnode);
        }
        break;
      case SCULPT_TOOL_CREASE:
        do_crease_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_BLOB:
        do_crease_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_PINCH:
        do_pinch_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_INFLATE:
        do_inflate_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_GRAB:
        do_grab_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_ROTATE:
        do_rotate_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_SNAKE_HOOK:
        do_snake_hook_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_NUDGE:
        do_nudge_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_THUMB:
        do_thumb_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_LAYER:
        do_layer_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_FLATTEN:
        do_flatten_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_CLAY:
        do_clay_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_CLAY_STRIPS:
        do_clay_strips_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_TWIST:
        do_twist_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_MULTIPLANE_SCRAPE:
        SCULPT_do_multiplane_scrape_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_CLAY_THUMB:
        do_clay_thumb_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_FILL:
        if (invert && brush2->flag & BRUSH_INVERT_TO_SCRAPE_FILL) {
          do_scrape_brush(sd, ob, nodes, totnode);
        }
        else {
          do_fill_brush(sd, ob, nodes, totnode);
        }
        break;
      case SCULPT_TOOL_SCRAPE:
        if (invert && brush2->flag & BRUSH_INVERT_TO_SCRAPE_FILL) {
          do_fill_brush(sd, ob, nodes, totnode);
        }
        else {
          do_scrape_brush(sd, ob, nodes, totnode);
        }
        break;
      case SCULPT_TOOL_MASK:
        do_mask_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_POSE:
        SCULPT_do_pose_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_DRAW_SHARP:
        do_draw_sharp_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_ELASTIC_DEFORM:
        do_elastic_deform_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_SLIDE_RELAX:
        do_slide_relax_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_BOUNDARY:
        SCULPT_do_boundary_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_CLOTH:
        SCULPT_do_cloth_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_DRAW_FACE_SETS:
        SCULPT_do_draw_face_sets_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_DISPLACEMENT_ERASER:
        do_displacement_eraser_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_DISPLACEMENT_SMEAR:
        do_displacement_smear_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_PAINT:
        SCULPT_do_paint_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_SMEAR:
        SCULPT_do_smear_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_FAIRING:
        do_fairing_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_SCENE_PROJECT:
        do_scene_project_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_SYMMETRIZE:
        SCULPT_do_symmetrize_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_ARRAY:
        SCULPT_do_array_brush(sd, ob, nodes, totnode);
      case SCULPT_TOOL_VCOL_BOUNDARY:
        SCULPT_smooth_vcol_boundary(sd, ob, nodes, totnode, ss->cache->bstrength);
        break;
      case SCULPT_TOOL_UV_SMOOTH:
        SCULPT_uv_brush(sd, ob, nodes, totnode);
        break;
      case SCULPT_TOOL_TOPOLOGY_RAKE:
        if (ss->bm) {
          bmesh_topology_rake(
              sd, ob, nodes, totnode, ss->cache->bstrength, SCULPT_stroke_needs_original(brush));
        }
        break;
      case SCULPT_TOOL_DYNTOPO:
        if (has_dyntopo) {
          sculpt_topology_update(sd, ob, brush, ups);
          // do_symmetrical_brush_actions(sd, ob, sculpt_topology_update, ups);
        }
        break;
    }
    if (ss->needs_pbvh_rebuild) {
      bContext *C = ss->cache->vc->C;

      /* The mesh was modified, rebuild the PBVH. */
      BKE_particlesystem_reset_all(ob);
      BKE_ptcache_object_reset(CTX_data_scene(C), ob, PTCACHE_RESET_OUTDATED);
      DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
      BKE_scene_graph_update_tagged(CTX_data_ensure_evaluated_depsgraph(C), CTX_data_main(C));
      SCULPT_pbvh_clear(ob);
      Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
      BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false, false);
      if (brush->sculpt_tool == SCULPT_TOOL_ARRAY) {
        SCULPT_tag_update_overlays(C);
      }
      ss->needs_pbvh_rebuild = false;
    }

    // brushes with origdata and large brush radii can't apply
    // proxies here without interfering with each other.
    if (!ELEM(brush->sculpt_tool,
              SCULPT_TOOL_GRAB,
              SCULPT_TOOL_ELASTIC_DEFORM,
              SCULPT_TOOL_ROTATE,
              SCULPT_TOOL_TWIST)) {
      sculpt_combine_proxies(sd, ob);
    }

    BKE_pbvh_update_bounds(ss->pbvh, PBVH_UpdateBB | PBVH_UpdateOriginalBB);

    ss->cache->channels_final = channels_final;
  }

  /*
                           paint_stroke_apply_subspacing(
                               ss->cache->stroke,
                               spacing,
                               PAINT_MODE_SCULPT,
                               &ss->cache->last_smooth_t[SCULPT_get_symmetry_pass(ss)]);

    */

  /* The cloth brush adds the gravity as a regular force and it is processed in the solver. */
  if (ss->cache->supports_gravity && !ELEM(brush->sculpt_tool,
                                           SCULPT_TOOL_CLOTH,
                                           SCULPT_TOOL_DRAW_FACE_SETS,
                                           SCULPT_TOOL_BOUNDARY)) {
    do_gravity(sd, ob, nodes, totnode, sd->gravity_factor);
  }

  if (brush->deform_target == BRUSH_DEFORM_TARGET_CLOTH_SIM) {
    if (SCULPT_stroke_is_main_symmetry_pass(ss->cache)) {
      SCULPT_cloth_sim_activate_nodes(ss->cache->cloth_sim, nodes, totnode);
      SCULPT_cloth_brush_do_simulation_step(sd, ob, ss->cache->cloth_sim, nodes, totnode);
    }
  }
  ss->cache->brush = oldbrush;
  ss->cache->radius = start_radius;
  ss->cache->radius_squared = start_radius * start_radius;

  MEM_SAFE_FREE(nodes);
}

/* Flush displacement from deformed PBVH vertex to original mesh. */
static void sculpt_flush_pbvhvert_deform(Object *ob, PBVHVertexIter *vd)
{
  SculptSession *ss = ob->sculpt;
  Mesh *me = ob->data;
  float disp[3], newco[3];
  int index = vd->vert_indices[vd->i];

  sub_v3_v3v3(disp, vd->co, ss->deform_cos[index]);
  mul_m3_v3(ss->deform_imats[index], disp);
  add_v3_v3v3(newco, disp, ss->orig_cos[index]);

  copy_v3_v3(ss->deform_cos[index], vd->co);
  copy_v3_v3(ss->orig_cos[index], newco);

  if (!ss->shapekey_active) {
    copy_v3_v3(me->mvert[index].co, newco);
  }
}

static void sculpt_combine_proxies_task_cb(void *__restrict userdata,
                                           const int n,
                                           const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  Sculpt *sd = data->sd;
  Object *ob = data->ob;
  const bool use_orco = data->use_proxies_orco;

  PBVHVertexIter vd;
  PBVHProxyNode *proxies;
  int proxy_count;
  float(*orco)[3] = NULL;

  if (use_orco && !ss->bm) {
    orco = SCULPT_undo_push_node(data->ob, data->nodes[n], SCULPT_UNDO_COORDS)->co;
  }

  BKE_pbvh_node_get_proxies(data->nodes[n], &proxies, &proxy_count);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    float val[3];

    if (use_orco) {
      if (ss->bm) {
        float *co = BKE_PBVH_DYNVERT(ss->cd_dyn_vert, vd.bm_vert)->origco;
        copy_v3_v3(val, co);
        // copy_v3_v3(val, BM_log_original_vert_co(ss->bm_log, vd.bm_vert));
      }
      else {
        copy_v3_v3(val, orco[vd.i]);
      }
    }
    else {
      copy_v3_v3(val, vd.co);
    }

    for (int p = 0; p < proxy_count; p++) {
      add_v3_v3(val, proxies[p].co[vd.i]);
    }

    if (ss->filter_cache && ss->filter_cache->cloth_sim) {
      /* When there is a simulation running in the filter cache that was created by a tool, combine
       * the proxies into the simulation instead of directly into the mesh. */
      SCULPT_clip(sd, ss, ss->filter_cache->cloth_sim->pos[vd.index], val);
    }
    else {
      SCULPT_clip(sd, ss, vd.co, val);
    }

    if (ss->deform_modifiers_active) {
      sculpt_flush_pbvhvert_deform(ob, &vd);
    }
  }
  BKE_pbvh_vertex_iter_end;

  BKE_pbvh_node_free_proxies(data->nodes[n]);
}

void sculpt_combine_proxies(Sculpt *sd, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  PBVHNode **nodes;
  int totnode;

  if (!ss->cache->supports_gravity && sculpt_tool_is_proxy_used(brush->sculpt_tool)) {
    /* First line is tools that don't support proxies. */
    return;
  }

  BKE_pbvh_gather_proxies(ss->pbvh, &nodes, &totnode);

  const bool use_orco = ELEM(brush->sculpt_tool,
                             SCULPT_TOOL_GRAB,
                             SCULPT_TOOL_ROTATE,
                             SCULPT_TOOL_THUMB,
                             SCULPT_TOOL_BOUNDARY,
                             SCULPT_TOOL_ELASTIC_DEFORM,

                             SCULPT_TOOL_POSE);
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .brush = brush,
      .nodes = nodes,
      .use_proxies_orco = use_orco,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, sculpt_combine_proxies_task_cb, &settings);
  MEM_SAFE_FREE(nodes);
}

void SCULPT_combine_transform_proxies(Sculpt *sd, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  PBVHNode **nodes;
  int totnode;

  BKE_pbvh_gather_proxies(ss->pbvh, &nodes, &totnode);
  SculptThreadedTaskData data = {
      .sd = sd,
      .ob = ob,
      .nodes = nodes,
      .use_proxies_orco = false,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, sculpt_combine_proxies_task_cb, &settings);

  MEM_SAFE_FREE(nodes);
}

/**
 * Copy the modified vertices from the #PBVH to the active key.
 */
static void sculpt_update_keyblock(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  float(*vertCos)[3];

  /* Key-block update happens after handling deformation caused by modifiers,
   * so ss->orig_cos would be updated with new stroke. */
  if (ss->orig_cos) {
    vertCos = ss->orig_cos;
  }
  else {
    vertCos = BKE_pbvh_vert_coords_alloc(ss->pbvh);
  }

  if (!vertCos) {
    return;
  }

  SCULPT_vertcos_to_key(ob, ss->shapekey_active, vertCos);

  if (vertCos != ss->orig_cos) {
    MEM_freeN(vertCos);
  }
}

static void SCULPT_flush_stroke_deform_task_cb(void *__restrict userdata,
                                               const int n,
                                               const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  Object *ob = data->ob;
  float(*vertCos)[3] = data->vertCos;

  PBVHVertexIter vd;

  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    BM_mesh_elem_index_ensure(ss->bm, BM_VERT);
  }

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    sculpt_flush_pbvhvert_deform(ob, &vd);

    if (!vertCos) {
      continue;
    }

    int index = vd.vert_indices[vd.i];
    copy_v3_v3(vertCos[index], ss->orig_cos[index]);
  }
  BKE_pbvh_vertex_iter_end;
}

/* Flush displacement from deformed PBVH to original layer. */
void SCULPT_flush_stroke_deform(Sculpt *sd, Object *ob, bool is_proxy_used)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (is_proxy_used && ss->deform_modifiers_active) {
    /* This brushes aren't using proxies, so sculpt_combine_proxies() wouldn't propagate needed
     * deformation to original base. */

    int totnode;
    Mesh *me = (Mesh *)ob->data;
    PBVHNode **nodes;
    float(*vertCos)[3] = NULL;

    if (ss->shapekey_active) {
      vertCos = MEM_mallocN(sizeof(*vertCos) * me->totvert, "flushStrokeDeofrm keyVerts");

      /* Mesh could have isolated verts which wouldn't be in BVH, to deal with this we copy old
       * coordinates over new ones and then update coordinates for all vertices from BVH. */
      memcpy(vertCos, ss->orig_cos, sizeof(*vertCos) * me->totvert);
    }

    BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

    SculptThreadedTaskData data = {
        .sd = sd,
        .ob = ob,
        .brush = brush,
        .nodes = nodes,
        .vertCos = vertCos,
    };

    TaskParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, true, totnode);
    BLI_task_parallel_range(0, totnode, &data, SCULPT_flush_stroke_deform_task_cb, &settings);

    if (vertCos) {
      SCULPT_vertcos_to_key(ob, ss->shapekey_active, vertCos);
      MEM_freeN(vertCos);
    }

    MEM_SAFE_FREE(nodes);

    /* Modifiers could depend on mesh normals, so we should update them.
     * NOTE: then if sculpting happens on locked key, normals should be re-calculate after
     * applying coords from key-block on base mesh. */
    BKE_mesh_calc_normals(me);
  }
  else if (ss->shapekey_active) {
    sculpt_update_keyblock(ob);
  }
}

/**
 * Flip all the edit-data across the axis/axes specified by \a symm.
 * Used to calculate multiple modifications to the mesh when symmetry is enabled.
 */
void SCULPT_cache_calc_brushdata_symm(StrokeCache *cache,
                                      const char symm,
                                      const char axis,
                                      const float angle)
{
  flip_v3_v3(cache->location, cache->true_location, symm);
  flip_v3_v3(cache->last_location, cache->true_last_location, symm);
  flip_v3_v3(cache->grab_delta_symmetry, cache->grab_delta, symm);
  flip_v3_v3(cache->view_normal, cache->true_view_normal, symm);
  flip_v3_v3(cache->view_origin, cache->true_view_origin, symm);

  flip_v3_v3(cache->initial_location, cache->true_initial_location, symm);
  flip_v3_v3(cache->initial_normal, cache->true_initial_normal, symm);

  /* XXX This reduces the length of the grab delta if it approaches the line of symmetry
   * XXX However, a different approach appears to be needed. */
#if 0
  if (sd->paint.symmetry_flags & PAINT_SYMMETRY_FEATHER) {
    float frac = 1.0f / max_overlap_count(sd);
    float reduce = (feather - frac) / (1.0f - frac);

    printf("feather: %f frac: %f reduce: %f\n", feather, frac, reduce);

    if (frac < 1.0f) {
      mul_v3_fl(cache->grab_delta_symmetry, reduce);
    }
  }
#endif

  unit_m4(cache->symm_rot_mat);
  unit_m4(cache->symm_rot_mat_inv);
  zero_v3(cache->plane_offset);

  /* Expects XYZ. */
  if (axis) {
    rotate_m4(cache->symm_rot_mat, axis, angle);
    rotate_m4(cache->symm_rot_mat_inv, axis, -angle);
  }

  mul_m4_v3(cache->symm_rot_mat, cache->location);
  mul_m4_v3(cache->symm_rot_mat, cache->grab_delta_symmetry);

  if (cache->supports_gravity) {
    flip_v3_v3(cache->gravity_direction, cache->true_gravity_direction, symm);
    mul_m4_v3(cache->symm_rot_mat, cache->gravity_direction);
  }

  if (cache->is_rake_rotation_valid) {
    flip_qt_qt(cache->rake_rotation_symmetry, cache->rake_rotation, symm);
  }
}

static void do_tiled(
    Sculpt *sd, Object *ob, Brush *brush, UnifiedPaintSettings *ups, BrushActionFunc action)
{
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;
  const float radius = cache->radius;
  BoundBox *bb = BKE_object_boundbox_get(ob);
  const float *bbMin = bb->vec[0];
  const float *bbMax = bb->vec[6];
  const float *step = sd->paint.tile_offset;

  /* These are integer locations, for real location: multiply with step and add orgLoc.
   * So 0,0,0 is at orgLoc. */
  int start[3];
  int end[3];
  int cur[3];

  /* Position of the "prototype" stroke for tiling. */
  float orgLoc[3];
  float original_initial_location[3];
  copy_v3_v3(orgLoc, cache->location);
  copy_v3_v3(original_initial_location, cache->initial_location);

  for (int dim = 0; dim < 3; dim++) {
    if ((sd->paint.symmetry_flags & (PAINT_TILE_X << dim)) && step[dim] > 0) {
      start[dim] = (bbMin[dim] - orgLoc[dim] - radius) / step[dim];
      end[dim] = (bbMax[dim] - orgLoc[dim] + radius) / step[dim];
    }
    else {
      start[dim] = end[dim] = 0;
    }
  }

  /* First do the "un-tiled" position to initialize the stroke for this location. */
  cache->tile_pass = 0;
  action(sd, ob, brush, ups);

  /* Now do it for all the tiles. */
  copy_v3_v3_int(cur, start);
  for (cur[0] = start[0]; cur[0] <= end[0]; cur[0]++) {
    for (cur[1] = start[1]; cur[1] <= end[1]; cur[1]++) {
      for (cur[2] = start[2]; cur[2] <= end[2]; cur[2]++) {
        if (!cur[0] && !cur[1] && !cur[2]) {
          /* Skip tile at orgLoc, this was already handled before all others. */
          continue;
        }

        ++cache->tile_pass;

        for (int dim = 0; dim < 3; dim++) {
          cache->location[dim] = cur[dim] * step[dim] + orgLoc[dim];
          cache->plane_offset[dim] = cur[dim] * step[dim];
          cache->initial_location[dim] = cur[dim] * step[dim] + original_initial_location[dim];
        }
        action(sd, ob, brush, ups);
      }
    }
  }
}

static void do_radial_symmetry(Sculpt *sd,
                               Object *ob,
                               Brush *brush,
                               UnifiedPaintSettings *ups,
                               BrushActionFunc action,
                               const char symm,
                               const int axis,
                               const float UNUSED(feather))
{
  SculptSession *ss = ob->sculpt;

  for (int i = 1; i < sd->radial_symm[axis - 'X']; i++) {
    const float angle = 2.0f * M_PI * i / sd->radial_symm[axis - 'X'];
    ss->cache->radial_symmetry_pass = i;
    SCULPT_cache_calc_brushdata_symm(ss->cache, symm, axis, angle);
    do_tiled(sd, ob, brush, ups, action);
  }
}

/**
 * Noise texture gives different values for the same input coord; this
 * can tear a multi-resolution mesh during sculpting so do a stitch in this case.
 */
static void sculpt_fix_noise_tear(Sculpt *sd, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  MTex *mtex = &brush->mtex;

  if (ss->multires.active && mtex->tex && mtex->tex->type == TEX_NOISE) {
    multires_stitch_grids(ob);
  }
}

static void do_symmetrical_brush_actions(Sculpt *sd,
                                         Object *ob,
                                         BrushActionFunc action,
                                         UnifiedPaintSettings *ups)
{
  Brush *brush = BKE_paint_brush(&sd->paint);
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;
  const char symm = SCULPT_mesh_symmetry_xyz_get(ob);

  float feather = calc_symmetry_feather(sd, ss->cache);

  cache->bstrength = brush_strength(sd, cache, feather, ups);
  cache->symmetry = symm;

  /* `symm` is a bit combination of XYZ -
   * 1 is mirror X; 2 is Y; 3 is XY; 4 is Z; 5 is XZ; 6 is YZ; 7 is XYZ */
  for (int i = 0; i <= symm; i++) {
    if (!SCULPT_is_symmetry_iteration_valid(i, symm)) {
      continue;
    }
    cache->mirror_symmetry_pass = i;
    cache->radial_symmetry_pass = 0;

    SCULPT_cache_calc_brushdata_symm(cache, i, 0, 0);
    do_tiled(sd, ob, brush, ups, action);

    do_radial_symmetry(sd, ob, brush, ups, action, i, 'X', feather);
    do_radial_symmetry(sd, ob, brush, ups, action, i, 'Y', feather);
    do_radial_symmetry(sd, ob, brush, ups, action, i, 'Z', feather);
  }
}

static void sculpt_update_tex(const Scene *scene, Sculpt *sd, SculptSession *ss)
{
  Brush *brush = BKE_paint_brush(&sd->paint);
  const int radius = BKE_brush_size_get(scene, brush, true);

  MEM_SAFE_FREE(ss->texcache);

  if (ss->tex_pool) {
    BKE_image_pool_free(ss->tex_pool);
    ss->tex_pool = NULL;
  }

  /* Need to allocate a bigger buffer for bigger brush size. */
  ss->texcache_side = 2 * radius;
  if (!ss->texcache || ss->texcache_side > ss->texcache_actual) {
    ss->texcache = BKE_brush_gen_texture_cache(brush, radius, false);
    ss->texcache_actual = ss->texcache_side;
    ss->tex_pool = BKE_image_pool_new();
  }
}

bool SCULPT_mode_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  return ob && ob->mode & OB_MODE_SCULPT;
}

bool SCULPT_vertex_colors_poll(bContext *C)
{
  if (!U.experimental.use_sculpt_vertex_colors) {
    return false;
  }

  return SCULPT_mode_poll(C);
}

bool SCULPT_vertex_colors_poll_no_bmesh(bContext *C)
{
  if (!U.experimental.use_sculpt_vertex_colors) {
    return false;
  }

  Object *ob = CTX_data_active_object(C);

  if (ob && ob->sculpt && ob->sculpt->bm) {
    return false;
  }

  return SCULPT_mode_poll(C);
}

bool SCULPT_mode_poll_view3d(bContext *C)
{
  return (SCULPT_mode_poll(C) && CTX_wm_region_view3d(C));
}

bool SCULPT_poll_view3d(bContext *C)
{
  return (SCULPT_poll(C) && CTX_wm_region_view3d(C));
}

bool SCULPT_poll(bContext *C)
{
  return SCULPT_mode_poll(C) && PAINT_brush_tool_poll(C);
}

static const char *sculpt_tool_name(Sculpt *sd)
{
  Brush *brush = BKE_paint_brush(&sd->paint);

  switch ((eBrushSculptTool)brush->sculpt_tool) {
    case SCULPT_TOOL_DRAW:
      return "Draw Brush";
    case SCULPT_TOOL_SMOOTH:
      return "Smooth Brush";
    case SCULPT_TOOL_CREASE:
      return "Crease Brush";
    case SCULPT_TOOL_BLOB:
      return "Blob Brush";
    case SCULPT_TOOL_PINCH:
      return "Pinch Brush";
    case SCULPT_TOOL_INFLATE:
      return "Inflate Brush";
    case SCULPT_TOOL_GRAB:
      return "Grab Brush";
    case SCULPT_TOOL_NUDGE:
      return "Nudge Brush";
    case SCULPT_TOOL_THUMB:
      return "Thumb Brush";
    case SCULPT_TOOL_LAYER:
      return "Layer Brush";
    case SCULPT_TOOL_FLATTEN:
      return "Flatten Brush";
    case SCULPT_TOOL_CLAY:
      return "Clay Brush";
    case SCULPT_TOOL_CLAY_STRIPS:
      return "Clay Strips Brush";
    case SCULPT_TOOL_CLAY_THUMB:
      return "Clay Thumb Brush";
    case SCULPT_TOOL_FILL:
      return "Fill Brush";
    case SCULPT_TOOL_SCRAPE:
      return "Scrape Brush";
    case SCULPT_TOOL_SNAKE_HOOK:
      return "Snake Hook Brush";
    case SCULPT_TOOL_ROTATE:
      return "Rotate Brush";
    case SCULPT_TOOL_MASK:
      return "Mask Brush";
    case SCULPT_TOOL_SIMPLIFY:
      return "Simplify Brush";
    case SCULPT_TOOL_DRAW_SHARP:
      return "Draw Sharp Brush";
    case SCULPT_TOOL_ELASTIC_DEFORM:
      return "Elastic Deform Brush";
    case SCULPT_TOOL_POSE:
      return "Pose Brush";
    case SCULPT_TOOL_MULTIPLANE_SCRAPE:
      return "Multi-plane Scrape Brush";
    case SCULPT_TOOL_SLIDE_RELAX:
      return "Slide/Relax Brush";
    case SCULPT_TOOL_BOUNDARY:
      return "Boundary Brush";
    case SCULPT_TOOL_CLOTH:
      return "Cloth Brush";
    case SCULPT_TOOL_DRAW_FACE_SETS:
      return "Draw Face Sets";
    case SCULPT_TOOL_DISPLACEMENT_ERASER:
      return "Multires Displacement Eraser";
    case SCULPT_TOOL_DISPLACEMENT_SMEAR:
      return "Multires Displacement Smear";
    case SCULPT_TOOL_PAINT:
      return "Paint Brush";
    case SCULPT_TOOL_SMEAR:
      return "Smear Brush";
    case SCULPT_TOOL_FAIRING:
      return "Fairing Brush";
    case SCULPT_TOOL_SCENE_PROJECT:
      return "Scene Project";
    case SCULPT_TOOL_SYMMETRIZE:
      return "Symmetrize Brush";
    case SCULPT_TOOL_TWIST:
      return "Clay Strips Brush";
    case SCULPT_TOOL_ARRAY:
      return "Array Brush";
    case SCULPT_TOOL_VCOL_BOUNDARY:
      return "Color Boundary";
    case SCULPT_TOOL_UV_SMOOTH:
      return "UV Smooth";
    case SCULPT_TOOL_TOPOLOGY_RAKE:
      return "Topology Rake";
    case SCULPT_TOOL_DYNTOPO:
      return "DynTopo";
  }

  return "Sculpting";
}

/**
 * Operator for applying a stroke (various attributes including mouse path)
 * using the current brush. */

void SCULPT_cache_free(SculptSession *ss, StrokeCache *cache)
{
  MEM_SAFE_FREE(cache->dial);
  MEM_SAFE_FREE(cache->surface_smooth_laplacian_disp);

  if (ss->custom_layers[SCULPT_SCL_LAYER_DISP]) {
    SCULPT_temp_customlayer_release(ss, ss->custom_layers[SCULPT_SCL_LAYER_DISP]);
    MEM_freeN(ss->custom_layers[SCULPT_SCL_LAYER_DISP]);
    ss->custom_layers[SCULPT_SCL_LAYER_DISP] = NULL;
  }

  if (ss->custom_layers[SCULPT_SCL_LAYER_STROKE_ID]) {
    SCULPT_temp_customlayer_release(ss, ss->custom_layers[SCULPT_SCL_LAYER_STROKE_ID]);
    MEM_freeN(ss->custom_layers[SCULPT_SCL_LAYER_STROKE_ID]);
    ss->custom_layers[SCULPT_SCL_LAYER_STROKE_ID] = NULL;
  }

  MEM_SAFE_FREE(cache->prev_colors);
  MEM_SAFE_FREE(cache->detail_directions);

  if (ss->cache->commandlist) {
    BKE_brush_commandlist_free(ss->cache->commandlist);
  }

  if (ss->custom_layers[SCULPT_SCL_FAIRING_MASK]) {
    SCULPT_temp_customlayer_release(ss, ss->custom_layers[SCULPT_SCL_FAIRING_MASK]);
    MEM_freeN(ss->custom_layers[SCULPT_SCL_FAIRING_MASK]);
    ss->custom_layers[SCULPT_SCL_FAIRING_MASK] = NULL;
  }

  if (ss->custom_layers[SCULPT_SCL_FAIRING_FADE]) {
    SCULPT_temp_customlayer_release(ss, ss->custom_layers[SCULPT_SCL_FAIRING_FADE]);

    MEM_freeN(ss->custom_layers[SCULPT_SCL_FAIRING_FADE]);
    ss->custom_layers[SCULPT_SCL_FAIRING_FADE] = NULL;
  }

  if (ss->custom_layers[SCULPT_SCL_PREFAIRING_CO]) {
    SCULPT_temp_customlayer_release(ss, ss->custom_layers[SCULPT_SCL_PREFAIRING_CO]);

    MEM_freeN(ss->custom_layers[SCULPT_SCL_PREFAIRING_CO]);
    ss->custom_layers[SCULPT_SCL_PREFAIRING_CO] = NULL;
  }

  if (ss->cache->channels_final) {
    BKE_brush_channelset_free(ss->cache->channels_final);
  }

  MEM_SAFE_FREE(cache->prev_displacement);
  MEM_SAFE_FREE(cache->limit_surface_co);

  if (cache->snap_context) {
    ED_transform_snap_object_context_destroy(cache->snap_context);
  }

  MEM_SAFE_FREE(cache->layer_disp_map);
  cache->layer_disp_map = NULL;
  cache->layer_disp_map_size = 0;

  if (cache->pose_ik_chain) {
    SCULPT_pose_ik_chain_free(cache->pose_ik_chain);
  }

  for (int i = 0; i < PAINT_SYMM_AREAS; i++) {
    if (cache->boundaries[i]) {
      SCULPT_boundary_data_free(cache->boundaries[i]);
      cache->boundaries[i] = NULL;
    }
    if (cache->geodesic_dists[i]) {
      MEM_SAFE_FREE(cache->geodesic_dists[i]);
    }
  }

  if (cache->cloth_sim) {
    SCULPT_cloth_simulation_free(cache->cloth_sim);
  }

  MEM_freeN(cache);
}

void SCULPT_clear_scl_pointers(SculptSession *ss)
{
  for (int i = 0; i < SCULPT_SCL_LAYER_MAX; i++) {
    MEM_SAFE_FREE(ss->custom_layers[i]);
    ss->custom_layers[i] = NULL;
  }
}

/* Initialize mirror modifier clipping. */
static void sculpt_init_mirror_clipping(Object *ob, SculptSession *ss)
{
  ModifierData *md;

  for (md = ob->modifiers.first; md; md = md->next) {
    if (!(md->type == eModifierType_Mirror && (md->mode & eModifierMode_Realtime))) {
      continue;
    }
    MirrorModifierData *mmd = (MirrorModifierData *)md;

    if (!(mmd->flag & MOD_MIR_CLIPPING)) {
      continue;
    }
    /* Check each axis for mirroring. */
    for (int i = 0; i < 3; i++) {
      if (!(mmd->flag & (MOD_MIR_AXIS_X << i))) {
        continue;
      }
      /* Enable sculpt clipping. */
      ss->cache->flag |= CLIP_X << i;

      /* Update the clip tolerance. */
      if (mmd->tolerance > ss->cache->clip_tolerance[i]) {
        ss->cache->clip_tolerance[i] = mmd->tolerance;
      }
    }
  }
}

/* Initialize the stroke cache invariants from operator properties. */
static void sculpt_update_cache_invariants(
    bContext *C, Sculpt *sd, SculptSession *ss, wmOperator *op, const float mouse[2])
{
  StrokeCache *cache = MEM_callocN(sizeof(StrokeCache), "stroke cache");
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
  Brush *brush = BKE_paint_brush(&sd->paint);
  ViewContext *vc = paint_stroke_view_context(op->customdata);
  Object *ob = CTX_data_active_object(C);
  float mat[3][3];
  float viewDir[3] = {0.0f, 0.0f, 1.0f};
  float max_scale;
  int mode;

  cache->C = C;
  ss->cache = cache;

  /* Set scaling adjustment. */
  max_scale = 0.0f;
  for (int i = 0; i < 3; i++) {
    max_scale = max_ff(max_scale, fabsf(ob->scale[i]));
  }
  cache->scale[0] = max_scale / ob->scale[0];
  cache->scale[1] = max_scale / ob->scale[1];
  cache->scale[2] = max_scale / ob->scale[2];

  float plane_trim = BRUSHSET_GET_FINAL_FLOAT(sd->channels, brush->channels, plane_trim, NULL);
  cache->plane_trim_squared = plane_trim * plane_trim;

  cache->flag = 0;

  sculpt_init_mirror_clipping(ob, ss);

  /* Initial mouse location. */
  if (mouse) {
    copy_v2_v2(cache->initial_mouse, mouse);
  }
  else {
    zero_v2(cache->initial_mouse);
  }

  copy_v3_v3(cache->initial_location, ss->cursor_location);
  copy_v3_v3(cache->true_initial_location, ss->cursor_location);

  copy_v3_v3(cache->initial_normal, ss->cursor_normal);
  copy_v3_v3(cache->true_initial_normal, ss->cursor_normal);

  mode = RNA_enum_get(op->ptr, "mode");
  cache->invert = mode == BRUSH_STROKE_INVERT;
  cache->alt_smooth = mode == BRUSH_STROKE_SMOOTH;
  cache->normal_weight = brush->normal_weight;

  /* Interpret invert as following normal, for grab brushes. */
  if (SCULPT_TOOL_HAS_NORMAL_WEIGHT(brush->sculpt_tool)) {
    if (cache->invert) {
      cache->invert = false;
      cache->normal_weight = (cache->normal_weight == 0.0f);
    }
  }

  /* Not very nice, but with current events system implementation
   * we can't handle brush appearance inversion hotkey separately (sergey). */
  if (cache->invert) {
    ups->draw_inverted = true;
  }
  else {
    ups->draw_inverted = false;
  }

  /* Alt-Smooth. */
  if (cache->alt_smooth) {
    if (brush->sculpt_tool == SCULPT_TOOL_MASK) {
      cache->saved_mask_brush_tool = brush->mask_tool;
      brush->mask_tool = BRUSH_MASK_SMOOTH;
    }
    else if (ELEM(brush->sculpt_tool,
                  SCULPT_TOOL_SLIDE_RELAX,
                  SCULPT_TOOL_DRAW_FACE_SETS,
                  SCULPT_TOOL_PAINT,
                  SCULPT_TOOL_SMEAR)) {
      /* Do nothing, this tool has its own smooth mode. */
    }
    else {
      Paint *p = &sd->paint;
      Brush *br;
      int size = BKE_brush_size_get(scene, brush, true);

      BLI_strncpy(cache->saved_active_brush_name,
                  brush->id.name + 2,
                  sizeof(cache->saved_active_brush_name));

      br = (Brush *)BKE_libblock_find_name(bmain, ID_BR, "Smooth");
      if (br) {
        BKE_paint_brush_set(p, br);
        brush = br;
        cache->saved_smooth_size = BKE_brush_size_get(scene, brush, true);
        BKE_brush_size_set(scene, brush, size, paint_use_channels(C));
        BKE_curvemapping_init(brush->curve);
      }
    }
  }

  copy_v2_v2(cache->mouse, cache->initial_mouse);
  copy_v2_v2(cache->mouse_event, cache->initial_mouse);
  copy_v2_v2(ups->tex_mouse, cache->initial_mouse);

  /* Truly temporary data that isn't stored in properties. */

  cache->vc = vc;

  cache->brush = brush;

  /* Cache projection matrix. */
  ED_view3d_ob_project_mat_get(cache->vc->rv3d, ob, cache->projection_mat);

  invert_m4_m4(ob->imat, ob->obmat);
  copy_m3_m4(mat, cache->vc->rv3d->viewinv);
  mul_m3_v3(mat, viewDir);
  copy_m3_m4(mat, ob->imat);
  mul_m3_v3(mat, viewDir);
  normalize_v3_v3(cache->true_view_normal, viewDir);

  copy_v3_v3(cache->true_view_origin, cache->vc->rv3d->viewinv[3]);

  cache->supports_gravity = (!ELEM(brush->sculpt_tool,
                                   SCULPT_TOOL_MASK,
                                   SCULPT_TOOL_SMOOTH,
                                   SCULPT_TOOL_SIMPLIFY,
                                   SCULPT_TOOL_DISPLACEMENT_SMEAR,
                                   SCULPT_TOOL_DISPLACEMENT_ERASER) &&
                             (sd->gravity_factor > 0.0f));
  /* Get gravity vector in world space. */
  if (cache->supports_gravity) {
    if (sd->gravity_object) {
      Object *gravity_object = sd->gravity_object;

      copy_v3_v3(cache->true_gravity_direction, gravity_object->obmat[2]);
    }
    else {
      cache->true_gravity_direction[0] = cache->true_gravity_direction[1] = 0.0f;
      cache->true_gravity_direction[2] = 1.0f;
    }

    /* Transform to sculpted object space. */
    mul_m3_v3(mat, cache->true_gravity_direction);
    normalize_v3(cache->true_gravity_direction);
  }

  /* Make copies of the mesh vertex locations and normals for some tools. */
  if (brush->flag & BRUSH_ANCHORED) {
    cache->original = true;
  }

  /* Draw sharp does not need the original coordinates to produce the accumulate effect, so it
   * should work the opposite way. */
  if (brush->sculpt_tool == SCULPT_TOOL_DRAW_SHARP) {
    cache->original = true;
  }

  if (SCULPT_TOOL_HAS_ACCUMULATE(brush->sculpt_tool)) {
    if (!(BRUSHSET_GET_INT(brush->channels, accumulate, &ss->cache->input_mapping))) {
      cache->original = true;
      if (brush->sculpt_tool == SCULPT_TOOL_DRAW_SHARP) {
        cache->original = false;
      }
    }
  }

  cache->first_time = true;

#define PIXEL_INPUT_THRESHHOLD 5
  if (brush->sculpt_tool == SCULPT_TOOL_ROTATE) {
    cache->dial = BLI_dial_init(cache->initial_mouse, PIXEL_INPUT_THRESHHOLD);
  }

#undef PIXEL_INPUT_THRESHHOLD
}

static float sculpt_brush_dynamic_size_get(Brush *brush, StrokeCache *cache, float initial_size)
{
  return initial_size;
#if 0
  if (brush->pressure_size_curve) {
    return initial_size *
           BKE_curvemapping_evaluateF(brush->pressure_size_curve, 0, cache->pressure);
  }

  switch (brush->sculpt_tool) {
    case SCULPT_TOOL_CLAY:
      return max_ff(initial_size * 0.20f, initial_size * pow3f(cache->pressure));
    case SCULPT_TOOL_CLAY_STRIPS:
      return max_ff(initial_size * 0.30f, initial_size * powf(cache->pressure, 1.5f));
    case SCULPT_TOOL_CLAY_THUMB: {
      float clay_stabilized_pressure = sculpt_clay_thumb_get_stabilized_pressure(cache);
      return initial_size * clay_stabilized_pressure;
    }
    default:
      return initial_size * cache->pressure;
  }
#endif
}

/* In these brushes the grab delta is calculated always from the initial stroke location, which
 * is generally used to create grab deformations. */
static bool sculpt_needs_delta_from_anchored_origin(Brush *brush)
{
  if (ELEM(brush->sculpt_tool,
           SCULPT_TOOL_GRAB,
           SCULPT_TOOL_POSE,
           SCULPT_TOOL_BOUNDARY,
           SCULPT_TOOL_ARRAY,
           SCULPT_TOOL_THUMB,
           SCULPT_TOOL_ELASTIC_DEFORM)) {
    return true;
  }
  if (brush->sculpt_tool == SCULPT_TOOL_CLOTH &&
      brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_GRAB) {
    return true;
  }
  return false;
}

/* In these brushes the grab delta is calculated from the previous stroke location, which is used
 * to calculate to orientate the brush tip and deformation towards the stroke direction. */
static bool sculpt_needs_delta_for_tip_orientation(Brush *brush)
{
  if (brush->sculpt_tool == SCULPT_TOOL_CLOTH) {
    return brush->cloth_deform_type != BRUSH_CLOTH_DEFORM_GRAB;
  }
  return ELEM(brush->sculpt_tool,
              SCULPT_TOOL_CLAY_STRIPS,
              SCULPT_TOOL_TWIST,
              SCULPT_TOOL_PINCH,
              SCULPT_TOOL_MULTIPLANE_SCRAPE,
              SCULPT_TOOL_CLAY_THUMB,
              SCULPT_TOOL_NUDGE,
              SCULPT_TOOL_SNAKE_HOOK);
}

static void sculpt_update_brush_delta(UnifiedPaintSettings *ups, Object *ob, Brush *brush)
{
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;
  const float mouse[2] = {
      cache->mouse_event[0],
      cache->mouse_event[1],
  };
  int tool = brush->sculpt_tool;

  if (!ELEM(tool,
            SCULPT_TOOL_PAINT,
            SCULPT_TOOL_GRAB,
            SCULPT_TOOL_ELASTIC_DEFORM,
            SCULPT_TOOL_CLOTH,
            SCULPT_TOOL_NUDGE,
            SCULPT_TOOL_CLAY_STRIPS,
            SCULPT_TOOL_TWIST,
            SCULPT_TOOL_PINCH,
            SCULPT_TOOL_MULTIPLANE_SCRAPE,
            SCULPT_TOOL_CLAY_THUMB,
            SCULPT_TOOL_SNAKE_HOOK,
            SCULPT_TOOL_POSE,
            SCULPT_TOOL_BOUNDARY,
            SCULPT_TOOL_ARRAY,
            SCULPT_TOOL_THUMB) &&
      !sculpt_brush_use_topology_rake(ss, brush)) {
    return;
  }
  float grab_location[3], imat[4][4], delta[3], loc[3];

  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
    if (tool == SCULPT_TOOL_GRAB && brush->flag & BRUSH_GRAB_ACTIVE_VERTEX) {
      copy_v3_v3(cache->orig_grab_location,
                 SCULPT_vertex_co_for_grab_active_get(ss, SCULPT_active_vertex_get(ss)));
    }
    else {
      copy_v3_v3(cache->orig_grab_location, cache->true_location);
    }
  }
  else if (tool == SCULPT_TOOL_SNAKE_HOOK ||
           (tool == SCULPT_TOOL_CLOTH &&
            brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_SNAKE_HOOK)) {
    add_v3_v3(cache->true_location, cache->grab_delta);
  }

  /* Compute 3d coordinate at same z from original location + mouse. */
  mul_v3_m4v3(loc, ob->obmat, cache->orig_grab_location);
  ED_view3d_win_to_3d(cache->vc->v3d, cache->vc->region, loc, mouse, grab_location);

  /* Compute delta to move verts by. */
  if (!SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
    if (sculpt_needs_delta_from_anchored_origin(brush)) {
      sub_v3_v3v3(delta, grab_location, cache->old_grab_location);
      invert_m4_m4(imat, ob->obmat);
      mul_mat3_m4_v3(imat, delta);
      add_v3_v3(cache->grab_delta, delta);
    }
    else if (sculpt_needs_delta_for_tip_orientation(brush)) {
      if (brush->flag & (BRUSH_ANCHORED | BRUSH_DRAG_DOT)) {
        float orig[3];
        mul_v3_m4v3(orig, ob->obmat, cache->orig_grab_location);
        sub_v3_v3v3(cache->grab_delta, grab_location, orig);
      }
      else {
        if (SCULPT_get_int(ss, use_smoothed_rake, NULL, brush)) {
          float tmp1[3];
          float tmp2[3];

          sub_v3_v3v3(tmp1, grab_location, cache->old_grab_location);
          copy_v3_v3(tmp2, ss->cache->grab_delta);

          normalize_v3(tmp1);
          normalize_v3(tmp2);

          bool bad = len_v3v3(grab_location, cache->old_grab_location) < 0.0001f;
          bad = bad || saacos(dot_v3v3(tmp1, tmp2) > 0.35f);

          float t = bad ? 0.1f : 0.5f;

          sub_v3_v3v3(tmp1, grab_location, cache->old_grab_location);
          interp_v3_v3v3(cache->grab_delta, cache->grab_delta, tmp1, t);
        }
        else {
          sub_v3_v3v3(ss->cache->grab_delta, grab_location, cache->old_grab_location);
        }
        // cache->grab_delta
      }
      invert_m4_m4(imat, ob->obmat);
      mul_mat3_m4_v3(imat, cache->grab_delta);
    }
    else {
      /* Use for 'Brush.topology_rake_factor'. */
      sub_v3_v3v3(cache->grab_delta, grab_location, cache->old_grab_location);
    }
  }
  else {
    zero_v3(cache->grab_delta);
  }

  if (brush->falloff_shape == PAINT_FALLOFF_SHAPE_TUBE) {
    project_plane_v3_v3v3(cache->grab_delta, cache->grab_delta, ss->cache->true_view_normal);
  }

  copy_v3_v3(cache->old_grab_location, grab_location);

  if (tool == SCULPT_TOOL_GRAB) {
    if (brush->flag & BRUSH_GRAB_ACTIVE_VERTEX) {
      copy_v3_v3(cache->anchored_location, cache->orig_grab_location);
    }
    else {
      copy_v3_v3(cache->anchored_location, cache->true_location);
    }
  }
  else if (tool == SCULPT_TOOL_ELASTIC_DEFORM || SCULPT_is_cloth_deform_brush(brush)) {
    copy_v3_v3(cache->anchored_location, cache->true_location);
  }
  else if (tool == SCULPT_TOOL_THUMB) {
    copy_v3_v3(cache->anchored_location, cache->orig_grab_location);
  }

  if (sculpt_needs_delta_from_anchored_origin(brush)) {
    /* Location stays the same for finding vertices in brush radius. */
    copy_v3_v3(cache->true_location, cache->orig_grab_location);

    ups->draw_anchored = true;
    copy_v2_v2(ups->anchored_initial_mouse, cache->initial_mouse);
    ups->anchored_size = ups->pixel_radius;
  }

  /* Handle 'rake' */
  cache->is_rake_rotation_valid = false;

  invert_m4_m4(imat, ob->obmat);
  mul_mat3_m4_v3(imat, grab_location);

  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
    copy_v3_v3(cache->rake_data.follow_co, grab_location);
  }

  if (!sculpt_brush_needs_rake_rotation(brush)) {
    return;
  }
  cache->rake_data.follow_dist = cache->radius * SCULPT_RAKE_BRUSH_FACTOR;

  if (!is_zero_v3(cache->grab_delta)) {
    const float eps = 0.00001f;

    float v1[3], v2[3];

    copy_v3_v3(v1, cache->rake_data.follow_co);
    copy_v3_v3(v2, cache->rake_data.follow_co);
    sub_v3_v3(v2, cache->grab_delta);

    sub_v3_v3(v1, grab_location);
    sub_v3_v3(v2, grab_location);

    if ((normalize_v3(v2) > eps) && (normalize_v3(v1) > eps) && (len_squared_v3v3(v1, v2) > eps)) {
      const float rake_dist_sq = len_squared_v3v3(cache->rake_data.follow_co, grab_location);
      const float rake_fade = (rake_dist_sq > square_f(cache->rake_data.follow_dist)) ?
                                  1.0f :
                                  sqrtf(rake_dist_sq) / cache->rake_data.follow_dist;

      float axis[3], angle;
      float tquat[4];

      rotation_between_vecs_to_quat(tquat, v1, v2);

      /* Use axis-angle to scale rotation since the factor may be above 1. */
      quat_to_axis_angle(axis, &angle, tquat);
      normalize_v3(axis);

      angle *= brush->rake_factor * rake_fade;
      axis_angle_normalized_to_quat(cache->rake_rotation, axis, angle);
      cache->is_rake_rotation_valid = true;
    }
  }
  sculpt_rake_data_update(&cache->rake_data, grab_location);
}

static void sculpt_update_cache_paint_variants(StrokeCache *cache, const Brush *brush)
{
  cache->paint_brush.hardness = brush->hardness;
  if (brush->paint_flags & BRUSH_PAINT_HARDNESS_PRESSURE) {
    cache->paint_brush.hardness *= brush->paint_flags & BRUSH_PAINT_HARDNESS_PRESSURE_INVERT ?
                                       1.0f - cache->pressure :
                                       cache->pressure;
  }

  cache->paint_brush.flow = brush->flow;
  if (brush->paint_flags & BRUSH_PAINT_FLOW_PRESSURE) {
    cache->paint_brush.flow *= brush->paint_flags & BRUSH_PAINT_FLOW_PRESSURE_INVERT ?
                                   1.0f - cache->pressure :
                                   cache->pressure;
  }

  cache->paint_brush.wet_mix = brush->wet_mix;
  if (brush->paint_flags & BRUSH_PAINT_WET_MIX_PRESSURE) {
    cache->paint_brush.wet_mix *= brush->paint_flags & BRUSH_PAINT_WET_MIX_PRESSURE_INVERT ?
                                      1.0f - cache->pressure :
                                      cache->pressure;

    /* This makes wet mix more sensible in higher values, which allows to create brushes that
     * have a wider pressure range were they only blend colors without applying too much of the
     * brush color. */
    cache->paint_brush.wet_mix = 1.0f - pow2f(1.0f - cache->paint_brush.wet_mix);
  }

  cache->paint_brush.wet_persistence = brush->wet_persistence;
  if (brush->paint_flags & BRUSH_PAINT_WET_PERSISTENCE_PRESSURE) {
    cache->paint_brush.wet_persistence = brush->paint_flags &
                                                 BRUSH_PAINT_WET_PERSISTENCE_PRESSURE_INVERT ?
                                             1.0f - cache->pressure :
                                             cache->pressure;
  }

  cache->paint_brush.density = brush->density;
  if (brush->paint_flags & BRUSH_PAINT_DENSITY_PRESSURE) {
    cache->paint_brush.density = brush->paint_flags & BRUSH_PAINT_DENSITY_PRESSURE_INVERT ?
                                     1.0f - cache->pressure :
                                     cache->pressure;
  }
}

/* Initialize the stroke cache variants from operator properties. */
static void sculpt_update_cache_variants(bContext *C, Sculpt *sd, Object *ob, PointerRNA *ptr)
{
  Scene *scene = CTX_data_scene(C);
  UnifiedPaintSettings *ups = &scene->toolsettings->unified_paint_settings;
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache) ||
      !((brush->flag & BRUSH_ANCHORED) || (brush->sculpt_tool == SCULPT_TOOL_SNAKE_HOOK) ||
        (brush->sculpt_tool == SCULPT_TOOL_ROTATE) || SCULPT_is_cloth_deform_brush(brush))) {
    RNA_float_get_array(ptr, "location", cache->true_location);
  }

  cache->pen_flip = RNA_boolean_get(ptr, "pen_flip");
  RNA_float_get_array(ptr, "mouse", cache->mouse);
  RNA_float_get_array(ptr, "mouse_event", cache->mouse_event);

  /* XXX: Use pressure value from first brush step for brushes which don't support strokes (grab,
   * thumb). They depends on initial state and brush coord/pressure/etc.
   * It's more an events design issue, which doesn't split coordinate/pressure/angle changing
   * events. We should avoid this after events system re-design. */
  if (paint_supports_dynamic_size(brush, PAINT_MODE_SCULPT) || cache->first_time) {
    cache->pressure = RNA_float_get(ptr, "pressure");
    cache->input_mapping.pressure = sqrtf(cache->pressure);
    // printf("pressure: %f\n", cache->pressure);
  }

  cache->x_tilt = RNA_float_get(ptr, "x_tilt");
  cache->y_tilt = RNA_float_get(ptr, "y_tilt");
  cache->input_mapping.xtilt = cache->x_tilt;
  cache->input_mapping.ytilt = cache->y_tilt;

  {
    float direction[4];
    copy_v3_v3(direction, ss->cache->grab_delta_symmetry);

    float tmp[3];
    mul_v3_v3fl(
        tmp, ss->cache->sculpt_normal_symm, dot_v3v3(ss->cache->sculpt_normal_symm, direction));
    sub_v3_v3(direction, tmp);
    normalize_v3(direction);

    /* If the active area is being applied for symmetry, flip it
     * across the symmetry axis and rotate it back to the original
     * position in order to project it. This insures that the
     * brush texture will be oriented correctly. */
    direction[3] = 0.0f;
    mul_v4_m4v4(direction, cache->projection_mat, direction);

    cache->input_mapping.angle = atan2(direction[1], direction[0]);

    // cache->vc
  }

  /* Truly temporary data that isn't stored in properties. */
  if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
    if (!BKE_brush_use_locked_size(scene, brush, true)) {
      cache->initial_radius = paint_calc_object_space_radius(
          cache->vc, cache->true_location, BKE_brush_size_get(scene, brush, true));
      BKE_brush_unprojected_radius_set(scene, brush, cache->initial_radius, true);
    }
    else {
      cache->initial_radius = BKE_brush_unprojected_radius_get(scene, brush, true);
    }
  }

  /* Clay stabilized pressure. */
  if (brush->sculpt_tool == SCULPT_TOOL_CLAY_THUMB) {
    if (SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
      for (int i = 0; i < SCULPT_CLAY_STABILIZER_LEN; i++) {
        ss->cache->clay_pressure_stabilizer[i] = 0.0f;
      }
      ss->cache->clay_pressure_stabilizer_index = 0;
    }
    else {
      cache->clay_pressure_stabilizer[cache->clay_pressure_stabilizer_index] = cache->pressure;
      cache->clay_pressure_stabilizer_index += 1;
      if (cache->clay_pressure_stabilizer_index >= SCULPT_CLAY_STABILIZER_LEN) {
        cache->clay_pressure_stabilizer_index = 0;
      }
    }
  }

  if (BKE_brush_use_size_pressure(brush) &&
      paint_supports_dynamic_size(brush, PAINT_MODE_SCULPT)) {
    cache->radius = sculpt_brush_dynamic_size_get(brush, cache, cache->initial_radius);
    cache->dyntopo_pixel_radius = sculpt_brush_dynamic_size_get(
        brush, cache, ups->initial_pixel_radius);
  }
  else {
    cache->radius = cache->initial_radius;
    cache->dyntopo_pixel_radius = ups->initial_pixel_radius;
  }

  sculpt_update_cache_paint_variants(cache, brush);

  cache->radius_squared = cache->radius * cache->radius;

  if (brush->flag & BRUSH_ANCHORED) {
    /* True location has been calculated as part of the stroke system already here. */
    if (brush->flag & BRUSH_EDGE_TO_EDGE) {
      RNA_float_get_array(ptr, "location", cache->true_location);
    }

    cache->radius = paint_calc_object_space_radius(
        cache->vc, cache->true_location, ups->pixel_radius);
    cache->radius_squared = cache->radius * cache->radius;

    copy_v3_v3(cache->anchored_location, cache->true_location);
  }

  sculpt_update_brush_delta(ups, ob, brush);

  if (brush->sculpt_tool == SCULPT_TOOL_ROTATE) {
    cache->vertex_rotation = -BLI_dial_angle(cache->dial, cache->mouse) * cache->bstrength;

    ups->draw_anchored = true;
    copy_v2_v2(ups->anchored_initial_mouse, cache->initial_mouse);
    copy_v3_v3(cache->anchored_location, cache->true_location);
    ups->anchored_size = ups->pixel_radius;
  }

  cache->special_rotation = ups->brush_rotation;

  cache->iteration_count++;
}

/* Returns true if any of the smoothing modes are active (currently
 * one of smooth brush, autosmooth, mask smooth, or shift-key
 * smooth). */
static bool sculpt_needs_connectivity_info(Sculpt *sd,
                                           const Brush *brush,
                                           SculptSession *ss,
                                           int stroke_mode)
{
  return true;
#if 0
  if (ss && ss->pbvh && SCULPT_is_automasking_enabled(sd, ss, brush)) {
    return true;
  }
  return ((stroke_mode == BRUSH_STROKE_SMOOTH) || (ss && ss->cache && ss->cache->alt_smooth) ||
          (brush->sculpt_tool == SCULPT_TOOL_SMOOTH) || (brush->autosmooth_factor > 0) ||
          ((brush->sculpt_tool == SCULPT_TOOL_MASK) && (brush->mask_tool == BRUSH_MASK_SMOOTH)) ||
          (brush->sculpt_tool == SCULPT_TOOL_POSE) ||
          (brush->sculpt_tool == SCULPT_TOOL_VCOL_BOUNDARY) ||
          (brush->sculpt_tool == SCULPT_TOOL_UV_SMOOTH) ||
          (brush->sculpt_tool == SCULPT_TOOL_PAINT && brush->vcol_boundary_factor > 0.0f) ||
          (brush->sculpt_tool == SCULPT_TOOL_BOUNDARY) ||
          (brush->sculpt_tool == SCULPT_TOOL_FAIRING) ||
          (brush->sculpt_tool == SCULPT_TOOL_TWIST) ||
          (brush->sculpt_tool == SCULPT_TOOL_SLIDE_RELAX) ||
          (brush->sculpt_tool == SCULPT_TOOL_CLOTH) || (brush->sculpt_tool == SCULPT_TOOL_SMEAR) ||
          (brush->sculpt_tool == SCULPT_TOOL_DRAW_FACE_SETS) ||
          (brush->sculpt_tool == SCULPT_TOOL_DISPLACEMENT_SMEAR));
#endif
}

void SCULPT_stroke_modifiers_check(const bContext *C, Object *ob, const Brush *brush)
{
  SculptSession *ss = ob->sculpt;
  View3D *v3d = CTX_wm_view3d(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  bool need_pmap = sculpt_needs_connectivity_info(sd, brush, ss, 0);
  if (ss->shapekey_active || ss->deform_modifiers_active ||
      (!BKE_sculptsession_use_pbvh_draw(ob, v3d) && need_pmap)) {
    Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
    BKE_sculpt_update_object_for_edit(depsgraph, ob, need_pmap, false, false);
  }
}

static void sculpt_raycast_cb(PBVHNode *node, void *data_v, float *tmin)
{
  SculptRaycastData *srd = data_v;
  if (!srd->use_back_depth && BKE_pbvh_node_get_tmin(node) >= *tmin) {
    return;
  }

  float(*origco)[3] = NULL;
  bool use_origco = false;

  if (srd->original && srd->ss->cache) {
    if (BKE_pbvh_type(srd->ss->pbvh) == PBVH_BMESH) {
      use_origco = true;
    }
    else {
      /* Intersect with coordinates from before we started stroke. */
      SculptUndoNode *unode = SCULPT_undo_get_node(node, SCULPT_UNDO_COORDS);
      origco = (unode) ? unode->co : NULL;
      use_origco = origco ? true : false;
    }
  }

  if (BKE_pbvh_node_raycast(srd->ss->pbvh,
                            node,
                            origco,
                            use_origco,
                            srd->ray_start,
                            srd->ray_normal,
                            &srd->isect_precalc,
                            &srd->hit_count,
                            &srd->depth,
                            &srd->back_depth,
                            &srd->active_vertex_index,
                            &srd->active_face_grid_index,
                            srd->face_normal,
                            srd->ss->stroke_id)) {
    srd->hit = true;
    *tmin = srd->depth;
  }

  if (srd->hit_count >= 2) {
    srd->back_hit = true;
  }
}

static void sculpt_find_nearest_to_ray_cb(PBVHNode *node, void *data_v, float *tmin)
{
  if (BKE_pbvh_node_get_tmin(node) >= *tmin) {
    return;
  }
  SculptFindNearestToRayData *srd = data_v;
  float(*origco)[3] = NULL;
  bool use_origco = false;

  if (srd->original && srd->ss->cache) {
    if (BKE_pbvh_type(srd->ss->pbvh) == PBVH_BMESH) {
      use_origco = true;
    }
    else {
      /* Intersect with coordinates from before we started stroke. */
      SculptUndoNode *unode = SCULPT_undo_get_node(node, SCULPT_UNDO_COORDS);
      origco = (unode) ? unode->co : NULL;
      use_origco = origco ? true : false;
    }
  }

  if (BKE_pbvh_node_find_nearest_to_ray(srd->ss->pbvh,
                                        node,
                                        origco,
                                        use_origco,
                                        srd->ray_start,
                                        srd->ray_normal,
                                        &srd->depth,
                                        &srd->dist_sq_to_ray,
                                        srd->ss->stroke_id)) {
    srd->hit = true;
    *tmin = srd->dist_sq_to_ray;
  }
}

float SCULPT_raycast_init(ViewContext *vc,
                          const float mouse[2],
                          float ray_start[3],
                          float ray_end[3],
                          float ray_normal[3],
                          bool original)
{
  float obimat[4][4];
  float dist;
  Object *ob = vc->obact;
  RegionView3D *rv3d = vc->region->regiondata;
  View3D *v3d = vc->v3d;

  /* TODO: what if the segment is totally clipped? (return == 0). */
  ED_view3d_win_to_segment_clipped(
      vc->depsgraph, vc->region, vc->v3d, mouse, ray_start, ray_end, true);

  invert_m4_m4(obimat, ob->obmat);
  mul_m4_v3(obimat, ray_start);
  mul_m4_v3(obimat, ray_end);

  sub_v3_v3v3(ray_normal, ray_end, ray_start);
  dist = normalize_v3(ray_normal);

  if ((rv3d->is_persp == false) &&
      /* If the ray is clipped, don't adjust its start/end. */
      !RV3D_CLIPPING_ENABLED(v3d, rv3d)) {
    BKE_pbvh_raycast_project_ray_root(ob->sculpt->pbvh, original, ray_start, ray_end, ray_normal);

    /* rRecalculate the normal. */
    sub_v3_v3v3(ray_normal, ray_end, ray_start);
    dist = normalize_v3(ray_normal);
  }

  return dist;
}

/* Gets the normal, location and active vertex location of the geometry under the cursor. This
 * also updates the active vertex and cursor related data of the SculptSession using the mouse
 * position
 */
bool SCULPT_cursor_geometry_info_update(bContext *C,
                                        SculptCursorGeometryInfo *out,
                                        const float mouse[2],
                                        bool use_sampled_normal,
                                        bool use_back_depth)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Scene *scene = CTX_data_scene(C);
  Sculpt *sd = scene->toolsettings->sculpt;
  Object *ob;
  SculptSession *ss;
  ViewContext vc;
  const Brush *brush = BKE_paint_brush(BKE_paint_get_active_from_context(C));
  float ray_start[3], ray_end[3], ray_normal[3], depth, face_normal[3], sampled_normal[3],
      mat[3][3];
  float viewDir[3] = {0.0f, 0.0f, 1.0f};
  int totnode;
  bool original = false;

  ED_view3d_viewcontext_init(C, &vc, depsgraph);

  ob = vc.obact;
  ss = ob->sculpt;

  if (!ss->pbvh) {
    zero_v3(out->location);
    zero_v3(out->normal);
    zero_v3(out->active_vertex_co);
    return false;
  }

  /* PBVH raycast to get active vertex and face normal. */
  depth = SCULPT_raycast_init(&vc, mouse, ray_start, ray_end, ray_normal, original);
  SCULPT_stroke_modifiers_check(C, ob, brush);
  float back_depth = depth;

  SculptRaycastData srd = {
      .original = original,
      .ss = ob->sculpt,
      .hit = false,
      .back_hit = false,
      .ray_start = ray_start,
      .ray_normal = ray_normal,
      .depth = depth,
      .back_depth = back_depth,
      .hit_count = 0,
      .use_back_depth = use_back_depth,
      .face_normal = face_normal,
  };
  isect_ray_tri_watertight_v3_precalc(&srd.isect_precalc, ray_normal);
  BKE_pbvh_raycast(
      ss->pbvh, sculpt_raycast_cb, &srd, ray_start, ray_normal, srd.original, srd.ss->stroke_id);

  /* Cursor is not over the mesh, return default values. */
  if (!srd.hit) {
    zero_v3(out->location);
    zero_v3(out->normal);
    zero_v3(out->active_vertex_co);
    return false;
  }

  /* Update the active vertex of the SculptSession. */
  ss->active_vertex_index = srd.active_vertex_index;

  SCULPT_vertex_random_access_ensure(ss);
  copy_v3_v3(out->active_vertex_co, SCULPT_active_vertex_co_get(ss));

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      ss->active_face_index = srd.active_face_grid_index;
      ss->active_grid_index = 0;
      break;
    case PBVH_GRIDS:
      ss->active_face_index.i = 0;
      ss->active_grid_index = srd.active_face_grid_index.i;
      break;
    case PBVH_BMESH:
      ss->active_face_index = srd.active_face_grid_index;
      ss->active_grid_index = 0;
      break;
  }

  copy_v3_v3(out->location, ray_normal);
  mul_v3_fl(out->location, srd.depth);
  add_v3_v3(out->location, ray_start);

  if (use_back_depth) {
    copy_v3_v3(out->back_location, ray_normal);
    if (srd.back_hit) {
      mul_v3_fl(out->back_location, srd.back_depth);
    }
    else {
      mul_v3_fl(out->back_location, srd.depth);
    }
    add_v3_v3(out->back_location, ray_start);
  }

  /* Option to return the face normal directly for performance o accuracy reasons. */
  if (!use_sampled_normal) {
    copy_v3_v3(out->normal, srd.face_normal);
    return srd.hit;
  }

  /* Sampled normal calculation. */
  float radius;

  /* Update cursor data in SculptSession. */
  invert_m4_m4(ob->imat, ob->obmat);
  copy_m3_m4(mat, vc.rv3d->viewinv);
  mul_m3_v3(mat, viewDir);
  copy_m3_m4(mat, ob->imat);
  mul_m3_v3(mat, viewDir);
  normalize_v3_v3(ss->cursor_view_normal, viewDir);
  copy_v3_v3(ss->cursor_normal, srd.face_normal);
  copy_v3_v3(ss->cursor_location, out->location);
  ss->rv3d = vc.rv3d;
  ss->v3d = vc.v3d;

  if (!BKE_brush_use_locked_size(scene, brush, true)) {
    radius = paint_calc_object_space_radius(
        &vc, out->location, BKE_brush_size_get(scene, brush, true));
  }
  else {
    radius = BKE_brush_unprojected_radius_get(scene, brush, true);
  }
  ss->cursor_radius = radius;

  PBVHNode **nodes = sculpt_pbvh_gather_cursor_update(ob, sd, original, &totnode);

  /* In case there are no nodes under the cursor, return the face normal. */
  if (!totnode) {
    MEM_SAFE_FREE(nodes);
    copy_v3_v3(out->normal, srd.face_normal);
    return true;
  }

  /* Calculate the sampled normal. */
  if (SCULPT_pbvh_calc_area_normal(brush, ob, nodes, totnode, true, sampled_normal)) {
    copy_v3_v3(out->normal, sampled_normal);
    copy_v3_v3(ss->cursor_sampled_normal, sampled_normal);
  }
  else {
    /* Use face normal when there are no vertices to sample inside the cursor radius. */
    copy_v3_v3(out->normal, srd.face_normal);
  }
  MEM_SAFE_FREE(nodes);
  return true;
}

/* Do a raycast in the tree to find the 3d brush location
 * (This allows us to ignore the GL depth buffer)
 * Returns 0 if the ray doesn't hit the mesh, non-zero otherwise. */
bool SCULPT_stroke_get_location(bContext *C, float out[3], const float mouse[2])
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob;
  SculptSession *ss;
  StrokeCache *cache;
  float ray_start[3], ray_end[3], ray_normal[3], depth, face_normal[3];
  bool original;
  ViewContext vc;

  ED_view3d_viewcontext_init(C, &vc, depsgraph);

  ob = vc.obact;

  ss = ob->sculpt;
  cache = ss->cache;
  original = (cache) ? cache->original : false;

  const Brush *brush = BKE_paint_brush(BKE_paint_get_active_from_context(C));

  SCULPT_stroke_modifiers_check(C, ob, brush);

  depth = SCULPT_raycast_init(&vc, mouse, ray_start, ray_end, ray_normal, original);

  bool hit = false;
  {
    SculptRaycastData srd;
    srd.ss = ob->sculpt;
    srd.ray_start = ray_start;
    srd.ray_normal = ray_normal;
    srd.hit = false;
    srd.depth = depth;
    srd.original = original;
    srd.face_normal = face_normal;
    isect_ray_tri_watertight_v3_precalc(&srd.isect_precalc, ray_normal);

    BKE_pbvh_raycast(
        ss->pbvh, sculpt_raycast_cb, &srd, ray_start, ray_normal, srd.original, srd.ss->stroke_id);
    if (srd.hit) {
      hit = true;
      copy_v3_v3(out, ray_normal);
      mul_v3_fl(out, srd.depth);
      add_v3_v3(out, ray_start);
    }
  }

  if (hit) {
    return hit;
  }

  if (!ELEM(brush->falloff_shape, PAINT_FALLOFF_SHAPE_TUBE)) {
    return hit;
  }

  SculptFindNearestToRayData srd = {
      .original = original,
      .ss = ob->sculpt,
      .hit = false,
      .ray_start = ray_start,
      .ray_normal = ray_normal,
      .depth = FLT_MAX,
      .dist_sq_to_ray = FLT_MAX,
  };
  BKE_pbvh_find_nearest_to_ray(
      ss->pbvh, sculpt_find_nearest_to_ray_cb, &srd, ray_start, ray_normal, srd.original);
  if (srd.hit) {
    hit = true;
    copy_v3_v3(out, ray_normal);
    mul_v3_fl(out, srd.depth);
    add_v3_v3(out, ray_start);
  }

  return hit;
}

static void sculpt_brush_init_tex(const Scene *scene, Sculpt *sd, SculptSession *ss)
{
  Brush *brush = BKE_paint_brush(&sd->paint);
  MTex *mtex = &brush->mtex;

  /* Init mtex nodes. */
  if (mtex->tex && mtex->tex->nodetree) {
    /* Has internal flag to detect it only does it once. */
    ntreeTexBeginExecTree(mtex->tex->nodetree);
  }

  /* TODO: Shouldn't really have to do this at the start of every stroke, but sculpt would need
   * some sort of notification when changes are made to the texture. */
  sculpt_update_tex(scene, sd, ss);
}

static void sculpt_brush_stroke_init(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  SculptSession *ss = CTX_data_active_object(C)->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);
  int mode = RNA_enum_get(op->ptr, "mode");
  bool is_smooth, needs_colors;
  bool need_mask = false;

  if (brush->sculpt_tool == SCULPT_TOOL_MASK) {
    need_mask = true;
  }

  if (brush->sculpt_tool == SCULPT_TOOL_CLOTH ||
      brush->deform_target == BRUSH_DEFORM_TARGET_CLOTH_SIM) {
    need_mask = true;
  }

  view3d_operator_needs_opengl(C);
  sculpt_brush_init_tex(scene, sd, ss);

  is_smooth = sculpt_needs_connectivity_info(sd, brush, ss, mode);
  needs_colors = ELEM(brush->sculpt_tool, SCULPT_TOOL_PAINT, SCULPT_TOOL_SMEAR);

  if (needs_colors) {
    BKE_sculpt_color_layer_create_if_needed(ob);
  }

  /* CTX_data_ensure_evaluated_depsgraph should be used at the end to include the updates of
   * earlier steps modifying the data. */
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, is_smooth, need_mask, needs_colors);
}

static void sculpt_restore_mesh(Sculpt *sd, Object *ob)
{
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  /* For the cloth brush it makes more sense to not restore the mesh state to keep running the
   * simulation from the previous state. */
  if (brush->sculpt_tool == SCULPT_TOOL_CLOTH) {
    return;
  }

  /* Restore the mesh before continuing with anchored stroke. */
  if ((brush->flag & BRUSH_ANCHORED) ||
      ((brush->sculpt_tool == SCULPT_TOOL_GRAB ||
        brush->sculpt_tool == SCULPT_TOOL_ELASTIC_DEFORM) &&
       BKE_brush_use_size_pressure(brush)) ||
      (brush->flag & BRUSH_DRAG_DOT)) {

    for (int i = 0; i < ss->totfaces; i++) {
      SculptFaceRef face = BKE_pbvh_table_index_to_face(ss->pbvh, i);
      int origf = SCULPT_face_set_original_get(ss, face);

      SCULPT_face_set_set(ss, face, origf);
    }

    paint_mesh_restore_co(sd, ob);
  }
}

/* Copy the PBVH bounding box into the object's bounding box. */
void SCULPT_update_object_bounding_box(Object *ob)
{
  if (ob->runtime.bb) {
    float bb_min[3], bb_max[3];

    BKE_pbvh_bounding_box(ob->sculpt->pbvh, bb_min, bb_max);
    BKE_boundbox_init_from_minmax(ob->runtime.bb, bb_min, bb_max);
  }
}

void SCULPT_flush_update_step(bContext *C, SculptUpdateType update_flags)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  ARegion *region = CTX_wm_region(C);
  MultiresModifierData *mmd = ss->multires.modifier;
  View3D *v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);

  if (rv3d) {
    /* Mark for faster 3D viewport redraws. */
    rv3d->rflag |= RV3D_PAINTING;
  }

  if (mmd != NULL) {
    multires_mark_as_modified(depsgraph, ob, MULTIRES_COORDS_MODIFIED);
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_SHADING);

  /* Only current viewport matters, slower update for all viewports will
   * be done in sculpt_flush_update_done. */
  if (!BKE_sculptsession_use_pbvh_draw(ob, v3d)) {
    /* Slow update with full dependency graph update and all that comes with it.
     * Needed when there are modifiers or full shading in the 3D viewport. */
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
    Brush *brush = BKE_paint_brush(&sd->paint);
    if (brush->sculpt_tool == SCULPT_TOOL_ARRAY) {
      BKE_pbvh_update_bounds(ss->pbvh, PBVH_UpdateBB);
      SCULPT_update_object_bounding_box(ob);
    }
    ED_region_tag_redraw(region);
  }
  else {
    /* Fast path where we just update the BVH nodes that changed, and redraw
     * only the part of the 3D viewport where changes happened. */
    rcti r;

    if (update_flags & SCULPT_UPDATE_COORDS) {
      BKE_pbvh_update_bounds(ss->pbvh, PBVH_UpdateBB);
      /* Update the object's bounding box too so that the object
       * doesn't get incorrectly clipped during drawing in
       * draw_mesh_object(). T33790. */
      SCULPT_update_object_bounding_box(ob);
    }

    if (CTX_wm_region_view3d(C) &&
        SCULPT_get_redraw_rect(region, CTX_wm_region_view3d(C), ob, &r)) {
      if (ss->cache) {
        ss->cache->current_r = r;
      }

      /* previous is not set in the current cache else
       * the partial rect will always grow */
      sculpt_extend_redraw_rect_previous(ob, &r);

      r.xmin += region->winrct.xmin - 2;
      r.xmax += region->winrct.xmin + 2;
      r.ymin += region->winrct.ymin - 2;
      r.ymax += region->winrct.ymin + 2;
      ED_region_tag_redraw_partial(region, &r, true);
    }
  }
}

bool all_nodes_callback(PBVHNode *node, void *data)
{
  return true;
}

void sculpt_undo_print_nodes(void *active);

void SCULPT_flush_update_done(const bContext *C, Object *ob, SculptUpdateType update_flags)
{
  /* After we are done drawing the stroke, check if we need to do a more
   * expensive depsgraph tag to update geometry. */
  wmWindowManager *wm = CTX_wm_manager(C);
  View3D *current_v3d = CTX_wm_view3d(C);
  RegionView3D *rv3d = CTX_wm_region_view3d(C);
  SculptSession *ss = ob->sculpt;
  Mesh *mesh = ob->data;

  /* Always needed for linked duplicates. */
  bool need_tag = (ID_REAL_USERS(&mesh->id) > 1);

  if (rv3d) {
    rv3d->rflag &= ~RV3D_PAINTING;
  }

  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    bScreen *screen = WM_window_get_active_screen(win);
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      SpaceLink *sl = area->spacedata.first;
      if (sl->spacetype != SPACE_VIEW3D) {
        continue;
      }
      View3D *v3d = (View3D *)sl;
      if (v3d != current_v3d) {
        need_tag |= !BKE_sculptsession_use_pbvh_draw(ob, v3d);
      }

      /* Tag all 3D viewports for redraw now that we are done. Others
       * viewports did not get a full redraw, and anti-aliasing for the
       * current viewport was deactivated. */
      LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
        if (region->regiontype == RGN_TYPE_WINDOW) {
          ED_region_tag_redraw(region);
        }
      }
    }
  }

  if (update_flags & SCULPT_UPDATE_COORDS) {
    BKE_pbvh_update_bounds(ss->pbvh, PBVH_UpdateOriginalBB);

    /* Coordinates were modified, so fake neighbors are not longer valid. */
    SCULPT_fake_neighbors_free(ob);
  }

  if (update_flags & SCULPT_UPDATE_MASK) {
    BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_UpdateMask);
  }

  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    BKE_pbvh_bmesh_after_stroke(ss->pbvh, false);
#if 0
    if (update_flags & SCULPT_UPDATE_COLOR) {
      PBVHNode **nodes;
      int totnode = 0;

      // BKE_pbvh_get_nodes(ss->pbvh, PBVH_UpdateColor, &nodes, &totnode);
      BKE_pbvh_search_gather(ss->pbvh, all_nodes_callback, NULL, &nodes, &totnode);

      for (int i = 0; i < totnode; i++) {
        SCULPT_undo_push_node(ob, nodes[i], SCULPT_UNDO_COLOR);
      }

      if (nodes) {
        MEM_freeN(nodes);
      }
    }
#endif

    sculpt_undo_print_nodes(NULL);
  }

  if (update_flags & SCULPT_UPDATE_COLOR) {
    BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_UpdateColor);
  }

  /* Optimization: if there is locked key and active modifiers present in */
  /* the stack, keyblock is updating at each step. otherwise we could update */
  /* keyblock only when stroke is finished. */
  if (ss->shapekey_active && !ss->deform_modifiers_active) {
    sculpt_update_keyblock(ob);
  }

  if (need_tag) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }
}

/* Returns whether the mouse/stylus is over the mesh (1)
 * or over the background (0). */
static bool over_mesh(bContext *C, struct wmOperator *UNUSED(op), float x, float y)
{
  float mouse[2], co[3];

  mouse[0] = x;
  mouse[1] = y;

  return SCULPT_stroke_get_location(C, co, mouse);
}

static bool sculpt_stroke_test_start(bContext *C, struct wmOperator *op, const float mouse[2])
{
  /* Don't start the stroke until mouse goes over the mesh.
   * NOTE: mouse will only be null when re-executing the saved stroke.
   * We have exception for 'exec' strokes since they may not set 'mouse',
   * only 'location', see: T52195. */
  if (((op->flag & OP_IS_INVOKE) == 0) || (mouse == NULL) ||
      over_mesh(C, op, mouse[0], mouse[1])) {
    Object *ob = CTX_data_active_object(C);
    SculptSession *ss = ob->sculpt;
    Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

    // increment stroke_id to flag origdata update
    ss->stroke_id++;

    if (ss->pbvh) {
      BKE_pbvh_set_stroke_id(ss->pbvh, ss->stroke_id);
    }

    ED_view3d_init_mats_rv3d(ob, CTX_wm_region_view3d(C));

    sculpt_update_cache_invariants(C, sd, ss, op, mouse);

    SculptCursorGeometryInfo sgi;
    SCULPT_cursor_geometry_info_update(C, &sgi, mouse, false, false);

    SCULPT_undo_push_begin(ob, sculpt_tool_name(sd));

    Brush *brush = BKE_paint_brush(&sd->paint);
    if (brush->sculpt_tool == SCULPT_TOOL_ARRAY) {
      SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_GEOMETRY);
    }

    return true;
  }
  return false;
}

void sculpt_stroke_update_step(bContext *C, struct PaintStroke *stroke, PointerRNA *itemptr)
{

  UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  if (ss->cache->channels_final) {
    BKE_brush_channelset_free(ss->cache->channels_final);
  }

  if (!brush->channels) {
    // should not happen!
    printf("had to create brush->channels for brush '%s'!", brush->id.name + 2);

    brush->channels = BKE_brush_channelset_create();
    BKE_brush_builtin_patch(brush, brush->sculpt_tool);
    BKE_brush_channelset_compat_load(brush->channels, brush, true);
  }

  if (brush->channels && sd->channels) {
    ss->cache->channels_final = BKE_brush_channelset_create();
    BKE_brush_channelset_merge(ss->cache->channels_final, brush->channels, sd->channels);
  }
  else if (brush->channels) {
    ss->cache->channels_final = BKE_brush_channelset_copy(brush->channels);
  }

  ss->cache->use_plane_trim = BRUSHSET_GET_INT(
      ss->cache->channels_final, use_plane_trim, &ss->cache->input_mapping);

  if (ss->cache->alt_smooth) {
    Brush *brush = (Brush *)BKE_libblock_find_name(
        CTX_data_main(C), ID_BR, ss->cache->saved_active_brush_name);

    if (brush) {
      // some settings should not be overridden

      bool hard_edge = BRUSHSET_GET_FINAL_INT(
          brush->channels, sd->channels ? sd->channels : NULL, hard_edge_mode, NULL);
      float smooth_factor = BRUSHSET_GET_FINAL_FLOAT(
          brush->channels, sd->channels ? sd->channels : NULL, smooth_strength_factor, NULL);

      BRUSHSET_SET_INT(ss->cache->channels_final, hard_edge_mode, hard_edge);
      BRUSHSET_SET_FLOAT(ss->cache->channels_final, smooth_strength_factor, smooth_factor);
    }
  }

  // load settings into brush and unified paint settings
  BKE_brush_channelset_compat_load(ss->cache->channels_final, brush, false);

  if (!(brush->flag & (BRUSH_ANCHORED | BRUSH_DRAG_DOT))) {
    BKE_brush_channelset_to_unified_settings(ss->cache->channels_final, ups);
  }

  sd->smooth_strength_factor = BRUSHSET_GET_FLOAT(
      ss->cache->channels_final, smooth_strength_factor, NULL);

  ss->cache->bstrength = brush_strength(sd, ss->cache, calc_symmetry_feather(sd, ss->cache), ups);

  // we have to evaluate channel mappings here manually
  BrushChannel *ch = BRUSHSET_LOOKUP_FINAL(brush->channels, sd->channels, strength);
  ss->cache->bstrength = BKE_brush_channel_eval_mappings(
      ch, &ss->cache->input_mapping, (double)ss->cache->bstrength, 0);

  if (ss->cache->invert) {
    brush->alpha = fabs(brush->alpha);
    ss->cache->bstrength = -fabs(ss->cache->bstrength);

    // BKE_brush_channelset_set_float(ss->cache->channels_final, "strength", ss->cache->bstrength);
  }

  ss->cache->stroke_distance = stroke->stroke_distance;
  ss->cache->stroke_distance_t = stroke->stroke_distance_t;
  ss->cache->stroke = stroke;

  if (SCULPT_stroke_is_first_brush_step(ss->cache)) {
    ss->cache->last_dyntopo_t = 0.0f;

    memset((void *)ss->cache->last_smooth_t, 0, sizeof(ss->cache->last_smooth_t));
    memset((void *)ss->cache->last_rake_t, 0, sizeof(ss->cache->last_rake_t));
  }

  BKE_brush_get_dyntopo(brush, sd, &brush->cached_dyntopo);

  if (brush->sculpt_tool == SCULPT_TOOL_SCENE_PROJECT) {
    sculpt_stroke_cache_snap_context_init(C, ob);
  }

  SCULPT_stroke_modifiers_check(C, ob, brush);
  if (itemptr) {
    sculpt_update_cache_variants(C, sd, ob, itemptr);
  }
  sculpt_restore_mesh(sd, ob);

  int boundsym = BKE_get_fset_boundary_symflag(ob);
  ss->cache->boundary_symmetry = boundsym;
  ss->boundary_symmetry = boundsym;

  if (ss->pbvh) {
    BKE_pbvh_set_symmetry(ss->pbvh, SCULPT_mesh_symmetry_xyz_get(ob), boundsym);
  }

  int detail_mode = SCULPT_get_int(ss, dyntopo_detail_mode, sd, brush);

  float detail_size = SCULPT_get_float(ss, dyntopo_detail_size, sd, brush);
  float detail_percent = SCULPT_get_float(ss, dyntopo_detail_percent, sd, brush);
  float detail_range = SCULPT_get_float(ss, dyntopo_detail_range, sd, brush);
  float constant_detail = SCULPT_get_float(ss, dyntopo_constant_detail, sd, brush);

  float dyntopo_pixel_radius = ss->cache->radius;
  float dyntopo_radius = paint_calc_object_space_radius(
      ss->cache->vc, ss->cache->true_location, dyntopo_pixel_radius);

  if (detail_mode == DYNTOPO_DETAIL_CONSTANT || detail_mode == DYNTOPO_DETAIL_MANUAL) {
    float object_space_constant_detail = 1.0f / (constant_detail * mat4_to_scale(ob->obmat));

    BKE_pbvh_bmesh_detail_size_set(ss->pbvh, object_space_constant_detail, detail_range);
  }
  else if (detail_mode == DYNTOPO_DETAIL_BRUSH) {
    BKE_pbvh_bmesh_detail_size_set(
        ss->pbvh, ss->cache->radius * detail_percent / 100.0f, detail_range);
  }
  else {
    BKE_pbvh_bmesh_detail_size_set(ss->pbvh,
                                   (dyntopo_radius / dyntopo_pixel_radius) *
                                       (detail_size * U.pixelsize) / 0.4f,
                                   detail_range);
  }

  if (0 && !ss->cache->commandlist && SCULPT_stroke_is_dynamic_topology(ss, brush)) {
    float spacing = (float)brush->cached_dyntopo.spacing / 100.0f;

    if (paint_stroke_apply_subspacing(
            ss->cache->stroke, spacing, PAINT_MODE_SCULPT, &ss->cache->last_dyntopo_t)) {
      do_symmetrical_brush_actions(sd, ob, sculpt_topology_update, ups);

      if (brush->sculpt_tool == SCULPT_TOOL_SNAKE_HOOK) {
        /* run dyntopo again for snake hook */
        do_symmetrical_brush_actions(sd, ob, sculpt_topology_update, ups);
      }
    }
  }

  do_symmetrical_brush_actions(sd, ob, do_brush_action, ups);

  if (ss->needs_pbvh_rebuild) {
    /* The mesh was modified, rebuild the PBVH. */
    BKE_particlesystem_reset_all(ob);
    BKE_ptcache_object_reset(CTX_data_scene(C), ob, PTCACHE_RESET_OUTDATED);

    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    BKE_scene_graph_update_tagged(CTX_data_ensure_evaluated_depsgraph(C), CTX_data_main(C));
    SCULPT_pbvh_clear(ob);
    Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false, false);

    if (brush->sculpt_tool == SCULPT_TOOL_ARRAY) {
      SCULPT_tag_update_overlays(C);
    }
    ss->needs_pbvh_rebuild = false;
  }

  sculpt_combine_proxies(sd, ob);

  if (brush->sculpt_tool == SCULPT_TOOL_FAIRING) {
    sculpt_fairing_brush_exec_fairing_for_cache(sd, ob);
  }

  /* Hack to fix noise texture tearing mesh. */
  sculpt_fix_noise_tear(sd, ob);

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
    sculpt_update_keyblock(ob);
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
}

static void sculpt_brush_exit_tex(Sculpt *sd)
{
  Brush *brush = BKE_paint_brush(&sd->paint);
  MTex *mtex = &brush->mtex;

  if (mtex->tex && mtex->tex->nodetree) {
    ntreeTexEndExecTree(mtex->tex->nodetree->execdata);
  }
}

static void sculpt_stroke_done(const bContext *C, struct PaintStroke *UNUSED(stroke))
{
  Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);
  Scene *scene = CTX_data_scene(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  /* Finished. */
  if (!ss->cache) {
    sculpt_brush_exit_tex(sd);
    return;
  }
  UnifiedPaintSettings *ups = &CTX_data_tool_settings(C)->unified_paint_settings;
  Brush *brush = BKE_paint_brush(&sd->paint);
  BLI_assert(brush == ss->cache->brush); /* const, so we shouldn't change. */
  ups->draw_inverted = false;

  SCULPT_stroke_modifiers_check(C, ob, brush);

  /* Alt-Smooth. */
  if (ss->cache->alt_smooth) {
    if (brush->sculpt_tool == SCULPT_TOOL_MASK) {
      brush->mask_tool = ss->cache->saved_mask_brush_tool;
    }
    else if (ELEM(brush->sculpt_tool,
                  SCULPT_TOOL_SLIDE_RELAX,
                  SCULPT_TOOL_DRAW_FACE_SETS,
                  SCULPT_TOOL_PAINT,
                  SCULPT_TOOL_SMEAR)) {
      /* Do nothing. */
    }
    else {
      BKE_brush_size_set(scene, brush, ss->cache->saved_smooth_size, true);
      brush = (Brush *)BKE_libblock_find_name(bmain, ID_BR, ss->cache->saved_active_brush_name);
      if (brush) {
        BKE_paint_brush_set(&sd->paint, brush);
      }
    }
  }

  if (SCULPT_is_automasking_enabled(sd, ss, brush)) {
    SCULPT_automasking_cache_free(ss, ss->cache->automasking);
  }

  BKE_pbvh_node_color_buffer_free(ss->pbvh);
  SCULPT_cache_free(ss, ss->cache);
  ss->cache = NULL;

  if (brush->sculpt_tool == SCULPT_TOOL_ARRAY) {
    SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_GEOMETRY);
    SCULPT_array_datalayers_free(ss->array, ob);
  }

  SCULPT_undo_push_end();

  if (brush->sculpt_tool == SCULPT_TOOL_MASK) {
    SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_MASK);
  }
  else {
    SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_COORDS);
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);
  sculpt_brush_exit_tex(sd);
}

static int sculpt_brush_stroke_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  struct PaintStroke *stroke;
  int ignore_background_click;
  int retval;

  sculpt_brush_stroke_init(C, op);

  stroke = paint_stroke_new(C,
                            op,
                            SCULPT_stroke_get_location,
                            sculpt_stroke_test_start,
                            sculpt_stroke_update_step,
                            NULL,
                            sculpt_stroke_done,
                            event->type);

  op->customdata = stroke;

  /* For tablet rotation. */
  ignore_background_click = RNA_boolean_get(op->ptr, "ignore_background_click");

  if (ignore_background_click && !over_mesh(C, op, event->x, event->y)) {
    paint_stroke_free(C, op);
    return OPERATOR_PASS_THROUGH;
  }

  if ((retval = op->type->modal(C, op, event)) == OPERATOR_FINISHED) {
    paint_stroke_free(C, op);
    return OPERATOR_FINISHED;
  }
  /* Add modal handler. */
  WM_event_add_modal_handler(C, op);

  OPERATOR_RETVAL_CHECK(retval);
  BLI_assert(retval == OPERATOR_RUNNING_MODAL);

  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_brush_stroke_exec(bContext *C, wmOperator *op)
{
  sculpt_brush_stroke_init(C, op);

  op->customdata = paint_stroke_new(C,
                                    op,
                                    SCULPT_stroke_get_location,
                                    sculpt_stroke_test_start,
                                    sculpt_stroke_update_step,
                                    NULL,
                                    sculpt_stroke_done,
                                    0);

  /* Frees op->customdata. */
  paint_stroke_exec(C, op);

  return OPERATOR_FINISHED;
}

static void sculpt_brush_stroke_cancel(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  const Brush *brush = BKE_paint_brush(&sd->paint);

  /* XXX Canceling strokes that way does not work with dynamic topology,
   *     user will have to do real undo for now. See T46456. */
  if (ss->cache && !SCULPT_stroke_is_dynamic_topology(ss, brush)) {
    paint_mesh_restore_co(sd, ob);
  }

  paint_stroke_cancel(C, op);

  if (ss->cache) {
    SCULPT_cache_free(ss, ss->cache);
    ss->cache = NULL;
  }

  sculpt_brush_exit_tex(sd);
}

static void SCULPT_OT_brush_stroke(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Sculpt";
  ot->idname = "SCULPT_OT_brush_stroke";
  ot->description = "Sculpt a stroke into the geometry";

  /* API callbacks. */
  ot->invoke = sculpt_brush_stroke_invoke;
  ot->modal = paint_stroke_modal;
  ot->exec = sculpt_brush_stroke_exec;
  ot->poll = SCULPT_poll;
  ot->cancel = sculpt_brush_stroke_cancel;

  /* Flags (sculpt does own undo? (ton)). */
  ot->flag = OPTYPE_BLOCKING;

  /* Properties. */

  paint_stroke_operator_properties(ot);

  RNA_def_boolean(ot->srna,
                  "ignore_background_click",
                  0,
                  "Ignore Background Click",
                  "Clicks on the background do not start the stroke");
}

/* Reset the copy of the mesh that is being sculpted on (currently just for the layer brush). */

static int sculpt_set_persistent_base_exec(bContext *C, wmOperator *UNUSED(op))
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  if (!ss) {
    return OPERATOR_FINISHED;
  }
  SCULPT_vertex_random_access_ensure(ss);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, false, false, false);

  SculptCustomLayer *scl_co, *scl_no, *scl_disp;

  SculptLayerParams params = {.permanent = true, .simple_array = false};

  SCULPT_ensure_persistent_layers(ss);

  scl_co = ss->custom_layers[SCULPT_SCL_PERS_CO];
  scl_no = ss->custom_layers[SCULPT_SCL_PERS_NO];
  scl_disp = ss->custom_layers[SCULPT_SCL_PERS_DISP];

  const int totvert = SCULPT_vertex_count_get(ss);

  for (int i = 0; i < totvert; i++) {
    SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

    float *co = SCULPT_temp_cdata_get(vertex, scl_co);
    float *no = SCULPT_temp_cdata_get(vertex, scl_no);
    float *disp = SCULPT_temp_cdata_get(vertex, scl_disp);

    copy_v3_v3(co, SCULPT_vertex_co_get(ss, vertex));
    SCULPT_vertex_normal_get(ss, vertex, no);
    *disp = 0.0f;
  }

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_set_persistent_base(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Set Persistent Base";
  ot->idname = "SCULPT_OT_set_persistent_base";
  ot->description = "Reset the copy of the mesh that is being sculpted on";

  /* API callbacks. */
  ot->exec = sculpt_set_persistent_base_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/************************* SCULPT_OT_optimize *************************/

static int sculpt_optimize_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = CTX_data_active_object(C);

  SCULPT_pbvh_clear(ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

/* The BVH gets less optimal more quickly with dynamic topology than
 * regular sculpting. There is no doubt more clever stuff we can do to
 * optimize it on the fly, but for now this gives the user a nicer way
 * to recalculate it than toggling modes. */
static void SCULPT_OT_optimize(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Rebuild BVH";
  ot->idname = "SCULPT_OT_optimize";
  ot->description = "Recalculate the sculpt BVH to improve performance";

  /* API callbacks. */
  ot->exec = sculpt_optimize_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/********************* Dynamic topology symmetrize ********************/

static bool sculpt_no_multires_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (SCULPT_mode_poll(C) && ob->sculpt && ob->sculpt->pbvh) {
    return BKE_pbvh_type(ob->sculpt->pbvh) != PBVH_GRIDS;
  }
  return false;
}

static bool sculpt_only_bmesh_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (SCULPT_mode_poll(C) && ob->sculpt && ob->sculpt->pbvh) {
    return BKE_pbvh_type(ob->sculpt->pbvh) == PBVH_BMESH;
  }
  return false;
}

static int sculpt_spatial_sort_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;
  PBVH *pbvh = ss->pbvh;

  if (!pbvh) {
    return OPERATOR_CANCELLED;
  }

  switch (BKE_pbvh_type(pbvh)) {
    case PBVH_BMESH:
      SCULPT_undo_push_begin(ob, "Dynamic topology symmetrize");
      SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_GEOMETRY);

      BKE_pbvh_reorder_bmesh(ss->pbvh);

      BKE_pbvh_bmesh_on_mesh_change(ss->pbvh);
      BM_log_full_mesh(ss->bm, ss->bm_log);

      ss->active_vertex_index.i = 0;
      ss->active_face_index.i = 0;

      BKE_pbvh_free(ss->pbvh);
      ss->pbvh = NULL;

      /* Finish undo. */
      SCULPT_undo_push_end();

      break;
    case PBVH_FACES:
      return OPERATOR_CANCELLED;
    case PBVH_GRIDS:
      return OPERATOR_CANCELLED;
  }

  /* Redraw. */
  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, ND_DATA | NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}
static void SCULPT_OT_spatial_sort_mesh(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Spatially Sort Mesh";
  ot->idname = "SCULPT_OT_spatial_sort_mesh";
  ot->description = "Spatially sort mesh to improve memory coherency";

  /* API callbacks. */
  ot->exec = sculpt_spatial_sort_exec;
  ot->poll = sculpt_only_bmesh_poll;
}

static int sculpt_symmetrize_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);
  const Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  SculptSession *ss = ob->sculpt;
  PBVH *pbvh = ss->pbvh;
  const float dist = RNA_float_get(op->ptr, "merge_tolerance");

  if (!pbvh) {
    return OPERATOR_CANCELLED;
  }

  switch (BKE_pbvh_type(pbvh)) {
    case PBVH_BMESH:
      /* Dyntopo Symmetrize. */

      /* To simplify undo for symmetrize, all BMesh elements are logged
       * as deleted, then after symmetrize operation all BMesh elements
       * are logged as added (as opposed to attempting to store just the
       * parts that symmetrize modifies). */
      SCULPT_undo_push_begin(ob, "Dynamic topology symmetrize");
      SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_DYNTOPO_SYMMETRIZE);

      BM_mesh_toolflags_set(ss->bm, true);

      /* Symmetrize and re-triangulate. */
      BMO_op_callf(ss->bm,
                   (BMO_FLAG_DEFAULTS & ~BMO_FLAG_RESPECT_HIDE),
                   "symmetrize input=%avef direction=%i dist=%f use_shapekey=%b",
                   sd->symmetrize_direction,
                   dist,
                   true);
#ifndef DYNTOPO_DYNAMIC_TESS
      SCULPT_dynamic_topology_triangulate(ss, ss->bm);
#endif
      /* Bisect operator flags edges (keep tags clean for edge queue). */
      BM_mesh_elem_hflag_disable_all(ss->bm, BM_EDGE, BM_ELEM_TAG, false);

      BM_mesh_toolflags_set(ss->bm, false);

      BKE_pbvh_recalc_bmesh_boundary(ss->pbvh);
      SCULT_dyntopo_flag_all_disk_sort(ss);

      // symmetrize is messing up ids, regenerate them from scratch
      BM_reassign_ids(ss->bm);
      BM_log_full_mesh(ss->bm, ss->bm_log);

      /* Finish undo. */
      SCULPT_undo_push_end();

      break;
    case PBVH_FACES:
      /* Mesh Symmetrize. */
      ED_sculpt_undo_geometry_begin(ob, "mesh symmetrize");
      Mesh *mesh = ob->data;

      BKE_mesh_mirror_apply_mirror_on_axis(bmain, mesh, sd->symmetrize_direction, dist);

      ED_sculpt_undo_geometry_end(ob);
      BKE_mesh_calc_normals(ob->data);
      BKE_mesh_batch_cache_dirty_tag(ob->data, BKE_MESH_BATCH_DIRTY_ALL);

      break;
    case PBVH_GRIDS:
      return OPERATOR_CANCELLED;
  }

  /* Redraw. */
  SCULPT_pbvh_clear(ob);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_symmetrize(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Symmetrize";
  ot->idname = "SCULPT_OT_symmetrize";
  ot->description = "Symmetrize the topology modifications";

  /* API callbacks. */
  ot->exec = sculpt_symmetrize_exec;
  ot->poll = sculpt_no_multires_poll;

  RNA_def_float(ot->srna,
                "merge_tolerance",
                0.0002f,
                0.0f,
                FLT_MAX,
                "Merge Distance",
                "Distance within which symmetrical vertices are merged",
                0.0f,
                1.0f);
}

/**** Toggle operator for turning sculpt mode on or off ****/

static void sculpt_init_session(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  /* Create persistent sculpt mode data. */
  BKE_sculpt_toolsettings_data_ensure(scene);

  /* Create sculpt mode session data. */
  if (ob->sculpt != NULL) {
    BKE_sculptsession_free(ob);
  }
  ob->sculpt = MEM_callocN(sizeof(SculptSession), "sculpt session");
  ob->sculpt->mode_type = OB_MODE_SCULPT;

  BKE_sculpt_ensure_orig_mesh_data(scene, ob);

  BKE_scene_graph_evaluated_ensure(depsgraph, bmain);

  /* This function expects a fully evaluated depsgraph. */
  BKE_sculpt_update_object_for_edit(depsgraph, ob, false, false, false);

  /* Here we can detect geometry that was just added to Sculpt Mode as it has the
   * SCULPT_FACE_SET_NONE assigned, so we can create a new Face Set for it. */
  /* In sculpt mode all geometry that is assigned to SCULPT_FACE_SET_NONE is considered as not
   * initialized, which is used is some operators that modify the mesh topology to perform
   * certain actions in the new polys. After these operations are finished, all polys should have
   * a valid face set ID assigned (different from SCULPT_FACE_SET_NONE) to manage their
   * visibility correctly. */
  /* TODO(pablodp606): Based on this we can improve the UX in future tools for creating new
   * objects, like moving the transform pivot position to the new area or masking existing
   * geometry. */
  SculptSession *ss = ob->sculpt;
  const int new_face_set = SCULPT_face_set_next_available_get(ss);
  for (int i = 0; i < ss->totfaces; i++) {
    if (ss->face_sets[i] == SCULPT_FACE_SET_NONE) {
      ss->face_sets[i] = new_face_set;
    }
  }
}

void ED_object_sculptmode_enter_ex(Main *bmain,
                                   Depsgraph *depsgraph,
                                   Scene *scene,
                                   Object *ob,
                                   const bool force_dyntopo,
                                   ReportList *reports,
                                   bool do_undo)
{
  const int mode_flag = OB_MODE_SCULPT;
  Mesh *me = BKE_mesh_from_object(ob);

  /* Enter sculpt mode. */
  ob->mode |= mode_flag;

  sculpt_init_session(bmain, depsgraph, scene, ob);

  if (!(fabsf(ob->scale[0] - ob->scale[1]) < 1e-4f &&
        fabsf(ob->scale[1] - ob->scale[2]) < 1e-4f)) {
    BKE_report(
        reports, RPT_WARNING, "Object has non-uniform scale, sculpting may be unpredictable");
  }
  else if (is_negative_m4(ob->obmat)) {
    BKE_report(reports, RPT_WARNING, "Object has negative scale, sculpting may be unpredictable");
  }

  Paint *paint = BKE_paint_get_active_from_paintmode(scene, PAINT_MODE_SCULPT);
  BKE_paint_init(bmain, scene, PAINT_MODE_SCULPT, PAINT_CURSOR_SCULPT);

  paint_cursor_start(paint, SCULPT_mode_poll_view3d);

  bool has_multires = false;

  /* Check dynamic-topology flag; re-enter dynamic-topology mode when changing modes,
   * As long as no data was added that is not supported. */
  if (me->flag & ME_SCULPT_DYNAMIC_TOPOLOGY) {
    MultiresModifierData *mmd = BKE_sculpt_multires_active(scene, ob);

    const char *message_unsupported = NULL;
    if (mmd != NULL) {
      message_unsupported = TIP_("multi-res modifier");
      has_multires = true;
    }
    else {
      enum eDynTopoWarnFlag flag = SCULPT_dynamic_topology_check(scene, ob);
      if (flag == 0) {
        /* pass */
      }
      else if (flag & DYNTOPO_WARN_EDATA) {
        message_unsupported = TIP_("edge data");
      }
      else if (flag & DYNTOPO_WARN_MODIFIER) {
        message_unsupported = TIP_("constructive modifier");
      }
      else {
        BLI_assert(0);
      }
    }

    if (!has_multires && ((message_unsupported == NULL) || force_dyntopo)) {
      /* Needed because we may be entering this mode before the undo system loads. */
      wmWindowManager *wm = bmain->wm.first;
      bool has_undo = do_undo && wm->undo_stack != NULL;

      /* Undo push is needed to prevent memory leak. */
      if (has_undo) {
        SCULPT_undo_push_begin(ob, "Dynamic topology enable");
      }

      bool need_bmlog = !ob->sculpt->bm_log;

      SCULPT_dynamic_topology_enable_ex(bmain, depsgraph, scene, ob);

      if (has_undo) {
        SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_DYNTOPO_BEGIN);
        SCULPT_undo_push_end();
      }
      else if (need_bmlog) {
        if (ob->sculpt->bm_log) {
          BM_log_free(ob->sculpt->bm_log, true);
          ob->sculpt->bm_log = NULL;
        }

        SCULPT_undo_ensure_bmlog(ob);

        // SCULPT_undo_ensure_bmlog failed to find a sculpt undo step
        if (!ob->sculpt->bm_log) {
          ob->sculpt->bm_log = BM_log_create(ob->sculpt->bm, ob->sculpt->cd_dyn_vert);
        }
      }
    }
    else {
      BKE_reportf(
          reports, RPT_WARNING, "Dynamic Topology found: %s, disabled", message_unsupported);
      me->flag &= ~ME_SCULPT_DYNAMIC_TOPOLOGY;
    }
  }

  /* Flush object mode. */
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
}

void ED_object_sculptmode_enter(struct bContext *C, Depsgraph *depsgraph, ReportList *reports)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  ED_object_sculptmode_enter_ex(bmain, depsgraph, scene, ob, false, reports, true);
}

void ED_object_sculptmode_exit_ex(Main *bmain, Depsgraph *depsgraph, Scene *scene, Object *ob)
{
  const int mode_flag = OB_MODE_SCULPT;
  Mesh *me = BKE_mesh_from_object(ob);

  multires_flush_sculpt_updates(ob);

  /* Not needed for now. */
#if 0
  MultiresModifierData *mmd = BKE_sculpt_multires_active(scene, ob);
  const int flush_recalc = ed_object_sculptmode_flush_recalc_flag(scene, ob, mmd);
#endif

  /* Always for now, so leaving sculpt mode always ensures scene is in
   * a consistent state. */
  if (true || /* flush_recalc || */ (ob->sculpt && ob->sculpt->bm)) {
    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  }

  if (me->flag & ME_SCULPT_DYNAMIC_TOPOLOGY) {
    /* Dynamic topology must be disabled before exiting sculpt
     * mode to ensure the undo stack stays in a consistent
     * state. */
    sculpt_dynamic_topology_disable_with_undo(bmain, depsgraph, scene, ob);

    /* Store so we know to re-enable when entering sculpt mode. */
    me->flag |= ME_SCULPT_DYNAMIC_TOPOLOGY;
  }

  /* Leave sculpt mode. */
  ob->mode &= ~mode_flag;

  BKE_sculptsession_free(ob);

  paint_cursor_delete_textures();

  /* Never leave derived meshes behind. */
  BKE_object_free_derived_caches(ob);

  /* Flush object mode. */
  DEG_id_tag_update(&ob->id, ID_RECALC_COPY_ON_WRITE);
}

void ED_object_sculptmode_exit(bContext *C, Depsgraph *depsgraph)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  ED_object_sculptmode_exit_ex(bmain, depsgraph, scene, ob);
}

static int sculpt_mode_toggle_exec(bContext *C, wmOperator *op)
{
  struct wmMsgBus *mbus = CTX_wm_message_bus(C);
  Main *bmain = CTX_data_main(C);
  Depsgraph *depsgraph = CTX_data_depsgraph_on_load(C);
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = scene->toolsettings;
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *ob = OBACT(view_layer);
  const int mode_flag = OB_MODE_SCULPT;
  const bool is_mode_set = (ob->mode & mode_flag) != 0;

  if (!is_mode_set) {
    if (!ED_object_mode_compat_set(C, ob, mode_flag, op->reports)) {
      return OPERATOR_CANCELLED;
    }
  }

  if (is_mode_set) {
    ED_object_sculptmode_exit_ex(bmain, depsgraph, scene, ob);
  }
  else {
    if (depsgraph) {
      depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    }
    ED_object_sculptmode_enter_ex(bmain, depsgraph, scene, ob, false, op->reports, true);
    BKE_paint_toolslots_brush_validate(bmain, &ts->sculpt->paint);

    if (ob->mode & mode_flag) {
      Mesh *me = ob->data;
      /* Dyntopo adds its own undo step. */
      if ((me->flag & ME_SCULPT_DYNAMIC_TOPOLOGY) == 0) {
        /* Without this the memfile undo step is used,
         * while it works it causes lag when undoing the first undo step, see T71564. */
        wmWindowManager *wm = CTX_wm_manager(C);
        if (wm->op_undo_depth <= 1) {
          SCULPT_undo_push_begin(ob, op->type->name);
        }
      }
    }
  }

  WM_event_add_notifier(C, NC_SCENE | ND_MODE, scene);

  WM_msg_publish_rna_prop(mbus, &ob->id, ob, Object, mode);

  WM_toolsystem_update_from_context_view3d(C);

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_sculptmode_toggle(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Sculpt Mode";
  ot->idname = "SCULPT_OT_sculptmode_toggle";
  ot->description = "Toggle sculpt mode in 3D view";

  /* API callbacks. */
  ot->exec = sculpt_mode_toggle_exec;
  ot->poll = ED_operator_object_active_editable_mesh;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void SCULPT_geometry_preview_lines_update(bContext *C, SculptSession *ss, float radius)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = CTX_data_active_object(C);

  ss->preview_vert_index_count = 0;
  int totpoints = 0;

  /* This function is called from the cursor drawing code, so the PBVH may not be build yet. */
  if (!ss->pbvh) {
    return;
  }

  if (!ss->deform_modifiers_active) {
    return;
  }

  if (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
    return;
  }

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, false);

  if (!ss->pmap) {
    return;
  }

  float brush_co[3];
  copy_v3_v3(brush_co, SCULPT_active_vertex_co_get(ss));

  BLI_bitmap *visited_vertices = BLI_BITMAP_NEW(SCULPT_vertex_count_get(ss), "visited_vertices");

  /* Assuming an average of 6 edges per vertex in a triangulated mesh. */
  const int max_preview_vertices = SCULPT_vertex_count_get(ss) * 3 * 2;

  if (ss->preview_vert_index_list == NULL) {
    ss->preview_vert_index_list = MEM_callocN(max_preview_vertices * sizeof(SculptVertRef),
                                              "preview lines");
  }

  GSQueue *not_visited_vertices = BLI_gsqueue_new(sizeof(SculptVertRef));
  SculptVertRef active_v = SCULPT_active_vertex_get(ss);
  BLI_gsqueue_push(not_visited_vertices, &active_v);

  while (!BLI_gsqueue_is_empty(not_visited_vertices)) {
    SculptVertRef from_v;

    BLI_gsqueue_pop(not_visited_vertices, &from_v);

    SculptVertexNeighborIter ni;
    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, from_v, ni) {
      if (totpoints + (ni.size * 2) < max_preview_vertices) {
        SculptVertRef to_v = ni.vertex;
        int to_v_i = BKE_pbvh_vertex_index_to_table(ss->pbvh, to_v);

        ss->preview_vert_index_list[totpoints] = from_v;
        totpoints++;
        ss->preview_vert_index_list[totpoints] = to_v;
        totpoints++;

        if (BLI_BITMAP_TEST(visited_vertices, to_v_i)) {
          continue;
        }

        BLI_BITMAP_ENABLE(visited_vertices, to_v_i);

        const float *co = SCULPT_vertex_co_for_grab_active_get(ss, to_v);

        if (len_squared_v3v3(brush_co, co) < radius * radius) {
          BLI_gsqueue_push(not_visited_vertices, &to_v);
        }
      }
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);
  }

  BLI_gsqueue_free(not_visited_vertices);

  MEM_freeN(visited_vertices);

  ss->preview_vert_index_count = totpoints;
}

static int vertex_to_loop_colors_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = CTX_data_active_object(C);

  ID *data;
  data = ob->data;
  if (data && ID_IS_LINKED(data)) {
    return OPERATOR_CANCELLED;
  }

  if (ob->type != OB_MESH) {
    return OPERATOR_CANCELLED;
  }

  Mesh *mesh = ob->data;

  const int mloopcol_layer_n = CustomData_get_active_layer(&mesh->ldata, CD_MLOOPCOL);
  if (mloopcol_layer_n == -1) {
    return OPERATOR_CANCELLED;
  }
  MLoopCol *loopcols = CustomData_get_layer_n(&mesh->ldata, CD_MLOOPCOL, mloopcol_layer_n);

  const int MPropCol_layer_n = CustomData_get_active_layer(&mesh->vdata, CD_PROP_COLOR);
  if (MPropCol_layer_n == -1) {
    return OPERATOR_CANCELLED;
  }
  MPropCol *vertcols = CustomData_get_layer_n(&mesh->vdata, CD_PROP_COLOR, MPropCol_layer_n);

  MLoop *loops = CustomData_get_layer(&mesh->ldata, CD_MLOOP);
  MPoly *polys = CustomData_get_layer(&mesh->pdata, CD_MPOLY);

  for (int i = 0; i < mesh->totpoly; i++) {
    MPoly *c_poly = &polys[i];
    for (int j = 0; j < c_poly->totloop; j++) {
      int loop_index = c_poly->loopstart + j;
      MLoop *c_loop = &loops[c_poly->loopstart + j];
      loopcols[loop_index].r = (char)(vertcols[c_loop->v].color[0] * 255);
      loopcols[loop_index].g = (char)(vertcols[c_loop->v].color[1] * 255);
      loopcols[loop_index].b = (char)(vertcols[c_loop->v].color[2] * 255);
      loopcols[loop_index].a = (char)(vertcols[c_loop->v].color[3] * 255);
    }
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_vertex_to_loop_colors(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Sculpt Vertex Color to Vertex Color";
  ot->description = "Copy the Sculpt Vertex Color to a regular color layer";
  ot->idname = "SCULPT_OT_vertex_to_loop_colors";

  /* api callbacks */
  ot->poll = SCULPT_vertex_colors_poll_no_bmesh;
  ot->exec = vertex_to_loop_colors_exec;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int loop_to_vertex_colors_exec(bContext *C, wmOperator *UNUSED(op))
{
  Object *ob = CTX_data_active_object(C);

  ID *data;
  data = ob->data;
  if (data && ID_IS_LINKED(data)) {
    return OPERATOR_CANCELLED;
  }

  if (ob->type != OB_MESH) {
    return OPERATOR_CANCELLED;
  }

  Mesh *mesh = ob->data;

  const int mloopcol_layer_n = CustomData_get_active_layer(&mesh->ldata, CD_MLOOPCOL);
  if (mloopcol_layer_n == -1) {
    return OPERATOR_CANCELLED;
  }
  MLoopCol *loopcols = CustomData_get_layer_n(&mesh->ldata, CD_MLOOPCOL, mloopcol_layer_n);

  const int MPropCol_layer_n = CustomData_get_active_layer(&mesh->vdata, CD_PROP_COLOR);
  if (MPropCol_layer_n == -1) {
    return OPERATOR_CANCELLED;
  }
  MPropCol *vertcols = CustomData_get_layer_n(&mesh->vdata, CD_PROP_COLOR, MPropCol_layer_n);

  MLoop *loops = CustomData_get_layer(&mesh->ldata, CD_MLOOP);
  MPoly *polys = CustomData_get_layer(&mesh->pdata, CD_MPOLY);

  for (int i = 0; i < mesh->totpoly; i++) {
    MPoly *c_poly = &polys[i];
    for (int j = 0; j < c_poly->totloop; j++) {
      int loop_index = c_poly->loopstart + j;
      MLoop *c_loop = &loops[c_poly->loopstart + j];
      vertcols[c_loop->v].color[0] = (loopcols[loop_index].r / 255.0f);
      vertcols[c_loop->v].color[1] = (loopcols[loop_index].g / 255.0f);
      vertcols[c_loop->v].color[2] = (loopcols[loop_index].b / 255.0f);
      vertcols[c_loop->v].color[3] = (loopcols[loop_index].a / 255.0f);
    }
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_loop_to_vertex_colors(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Vertex Color to Sculpt Vertex Color";
  ot->description = "Copy the active loop color layer to the vertex color";
  ot->idname = "SCULPT_OT_loop_to_vertex_colors";

  /* api callbacks */
  ot->poll = SCULPT_vertex_colors_poll_no_bmesh;
  ot->exec = loop_to_vertex_colors_exec;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

#define SAMPLE_COLOR_PREVIEW_SIZE 60
#define SAMPLE_COLOR_OFFSET_X -15
#define SAMPLE_COLOR_OFFSET_Y -15
typedef struct SampleColorCustomData {
  void *draw_handle;
  Object *active_object;

  float mval[2];

  float initial_color[4];
  float sampled_color[4];
} SampleColorCustomData;

static void sculpt_sample_color_draw(const bContext *UNUSED(C), ARegion *UNUSED(ar), void *arg)
{
  SampleColorCustomData *sccd = (SampleColorCustomData *)arg;
  GPU_line_width(2.0f);
  GPU_line_smooth(true);
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_2D_UNIFORM_COLOR);

  const float origin_x = sccd->mval[0] + SAMPLE_COLOR_OFFSET_X;
  const float origin_y = sccd->mval[1] + SAMPLE_COLOR_OFFSET_Y;

  immUniformColor3fvAlpha(sccd->sampled_color, 1.0f);
  immRectf(pos,
           origin_x,
           origin_y,
           origin_x - SAMPLE_COLOR_PREVIEW_SIZE,
           origin_y - SAMPLE_COLOR_PREVIEW_SIZE);

  immUniformColor3fvAlpha(sccd->initial_color, 1.0f);
  immRectf(pos,
           origin_x - SAMPLE_COLOR_PREVIEW_SIZE,
           origin_y,
           origin_x - 2.0f * SAMPLE_COLOR_PREVIEW_SIZE,
           origin_y - SAMPLE_COLOR_PREVIEW_SIZE);

  immUnbindProgram();
  GPU_line_smooth(false);
}

static bool sculpt_sample_color_update_from_base(bContext *C,
                                                 const wmEvent *event,
                                                 SampleColorCustomData *sccd)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Base *base_sample = ED_view3d_give_base_under_cursor(C, event->mval);
  if (base_sample == NULL) {
    return false;
  }

  Object *object_sample = base_sample->object;
  if (object_sample->type != OB_MESH) {
    return false;
  }

  Object *ob_eval = DEG_get_evaluated_object(depsgraph, object_sample);
  Mesh *me_eval = BKE_object_get_evaluated_mesh(ob_eval);
  MPropCol *vcol = CustomData_get_layer(&me_eval->vdata, CD_PROP_COLOR);

  if (!vcol) {
    return false;
  }

  ARegion *region = CTX_wm_region(C);
  float global_loc[3];
  if (!ED_view3d_autodist_simple(region, event->mval, global_loc, 0, NULL)) {
    return false;
  }

  float object_loc[3];
  mul_v3_m4v3(object_loc, ob_eval->imat, global_loc);

  BVHTreeFromMesh bvh;
  BKE_bvhtree_from_mesh_get(&bvh, me_eval, BVHTREE_FROM_VERTS, 2);
  BVHTreeNearest nearest;
  nearest.index = -1;
  nearest.dist_sq = FLT_MAX;
  BLI_bvhtree_find_nearest(bvh.tree, object_loc, &nearest, bvh.nearest_callback, &bvh);
  if (nearest.index == -1) {
    return false;
  }
  free_bvhtree_from_mesh(&bvh);

  copy_v4_v4(sccd->sampled_color, vcol[nearest.index].color);
  IMB_colormanagement_scene_linear_to_srgb_v3(sccd->sampled_color);
  return true;
}

static int sculpt_sample_color_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Scene *scene = CTX_data_scene(C);
  Brush *brush = BKE_paint_brush(&sd->paint);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  SampleColorCustomData *sccd = (SampleColorCustomData *)op->customdata;

  ePaintMode mode = BKE_paintmode_get_active_from_context(C);
  bool use_channels = mode == PAINT_MODE_SCULPT;

  /* Finish operation on release. */
  if (event->val == KM_RELEASE) {
    float color_srgb[3];
    copy_v3_v3(color_srgb, sccd->sampled_color);
    BKE_brush_color_set(scene, brush, sccd->sampled_color, use_channels);
    WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, brush);
    ED_region_draw_cb_exit(region->type, sccd->draw_handle);
    ED_region_tag_redraw(region);
    MEM_freeN(sccd);
    ss->draw_faded_cursor = false;
    return OPERATOR_FINISHED;
  }

  SculptCursorGeometryInfo sgi;
  sccd->mval[0] = event->mval[0];
  sccd->mval[1] = event->mval[1];

  const bool over_mesh = SCULPT_cursor_geometry_info_update(C, &sgi, sccd->mval, false, false);
  if (over_mesh) {
    SculptVertRef active_vertex = SCULPT_active_vertex_get(ss);
    copy_v4_v4(sccd->sampled_color, SCULPT_vertex_color_get(ss, active_vertex));
    IMB_colormanagement_scene_linear_to_srgb_v3(sccd->sampled_color);
  }
  else {
    sculpt_sample_color_update_from_base(C, event, sccd);
  }

  ss->draw_faded_cursor = true;
  ED_region_tag_redraw(region);

  return OPERATOR_RUNNING_MODAL;
}

static int sculpt_sample_color_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(e))
{
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);
  Brush *brush = BKE_paint_brush(&sd->paint);
  ARegion *region = CTX_wm_region(C);
  SculptSession *ss = ob->sculpt;

  if (!ss->bm && !ss->vcol) {
    return OPERATOR_CANCELLED;
  }
  else {
    const SculptVertRef active_vertex = SCULPT_active_vertex_get(ss);
    const float *active_vertex_color = SCULPT_vertex_color_get(ss, active_vertex);

    if (!active_vertex_color) {
      return OPERATOR_CANCELLED;
    }

    SampleColorCustomData *sccd = MEM_callocN(sizeof(SampleColorCustomData),
                                              "Sample Color Custom Data");
    copy_v4_v4(sccd->sampled_color, SCULPT_vertex_color_get(ss, active_vertex));
    copy_v4_v4(sccd->initial_color, BKE_brush_color_get(scene, brush));

    sccd->draw_handle = ED_region_draw_cb_activate(
        region->type, sculpt_sample_color_draw, sccd, REGION_DRAW_POST_PIXEL);

    op->customdata = sccd;

    WM_event_add_modal_handler(C, op);
    ED_region_tag_redraw(region);

    return OPERATOR_RUNNING_MODAL;
  }
}

static void SCULPT_OT_sample_color(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Sample Color";
  ot->idname = "SCULPT_OT_sample_color";
  ot->description = "Sample the vertex color of the active vertex";

  /* api callbacks */
  ot->invoke = sculpt_sample_color_invoke;
  ot->modal = sculpt_sample_color_modal;
  ot->poll = SCULPT_vertex_colors_poll;

  ot->flag = OPTYPE_REGISTER;
}

/* Fake Neighbors. */
/* This allows the sculpt tools to work on meshes with multiple connected components as they had
 * only one connected component. When initialized and enabled, the sculpt API will return extra
 * connectivity neighbors that are not in the real mesh. These neighbors are calculated for each
 * vertex using the minimum distance to a vertex that is in a different connected component. */

/* The fake neighbors first need to be ensured to be initialized.
 * After that tools which needs fake neighbors functionality need to
 * temporarily enable it:
 *
 *   void my_awesome_sculpt_tool() {
 *     SCULPT_fake_neighbors_ensure(sd, object, brush->disconnected_distance_max);
 *     SCULPT_fake_neighbors_enable(ob);
 *
 *     ... Logic of the tool ...
 *     SCULPT_fake_neighbors_disable(ob);
 *   }
 *
 * Such approach allows to keep all the connectivity information ready for reuse
 * (without having lag prior to every stroke), but also makes it so the affect
 * is localized to a specific brushes and tools only. */

enum {
  SCULPT_TOPOLOGY_ID_NONE,
  SCULPT_TOPOLOGY_ID_DEFAULT,
};

static int SCULPT_vertex_get_connected_component(SculptSession *ss, SculptVertRef vertex)
{
  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    vertex.i = BM_elem_index_get((BMVert *)vertex.i);
  }

  if (ss->vertex_info.connected_component) {
    return ss->vertex_info.connected_component[vertex.i];
  }
  return SCULPT_TOPOLOGY_ID_DEFAULT;
}

static void SCULPT_fake_neighbor_init(SculptSession *ss, const float max_dist)
{
  const int totvert = SCULPT_vertex_count_get(ss);
  ss->fake_neighbors.fake_neighbor_index = MEM_malloc_arrayN(
      totvert, sizeof(SculptVertRef), "fake neighbor");
  for (int i = 0; i < totvert; i++) {
    ss->fake_neighbors.fake_neighbor_index[i].i = FAKE_NEIGHBOR_NONE;
  }

  ss->fake_neighbors.current_max_distance = max_dist;
}

static void SCULPT_fake_neighbor_add(SculptSession *ss,
                                     SculptVertRef v_index_a,
                                     SculptVertRef v_index_b)
{
  int tablea = (int)v_index_a.i, tableb = (int)v_index_b.i;

  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    tablea = BM_elem_index_get((BMVert *)v_index_a.i);
    tableb = BM_elem_index_get((BMVert *)v_index_b.i);
  }

  if (ss->fake_neighbors.fake_neighbor_index[tablea].i == FAKE_NEIGHBOR_NONE) {
    ss->fake_neighbors.fake_neighbor_index[tablea] = v_index_b;
    ss->fake_neighbors.fake_neighbor_index[tableb] = v_index_a;
  }
}

static void sculpt_pose_fake_neighbors_free(SculptSession *ss)
{
  MEM_SAFE_FREE(ss->fake_neighbors.fake_neighbor_index);
}

typedef struct NearestVertexFakeNeighborTLSData {
  int nearest_vertex_index;
  SculptVertRef nearest_vertex;
  float nearest_vertex_distance_squared;
  int current_topology_id;
} NearestVertexFakeNeighborTLSData;

static void do_fake_neighbor_search_task_cb(void *__restrict userdata,
                                            const int n,
                                            const TaskParallelTLS *__restrict tls)
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  NearestVertexFakeNeighborTLSData *nvtd = tls->userdata_chunk;
  PBVHVertexIter vd;

  SCULPT_vertex_random_access_ensure(ss);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    int vd_topology_id = SCULPT_vertex_get_connected_component(ss, vd.vertex);
    if (vd_topology_id != nvtd->current_topology_id &&
        ss->fake_neighbors.fake_neighbor_index[vd.index].i == FAKE_NEIGHBOR_NONE) {
      float distance_squared = len_squared_v3v3(vd.co, data->nearest_vertex_search_co);
      if (distance_squared < nvtd->nearest_vertex_distance_squared &&
          distance_squared < data->max_distance_squared) {
        nvtd->nearest_vertex_index = vd.index;
        nvtd->nearest_vertex = vd.vertex;
        nvtd->nearest_vertex_distance_squared = distance_squared;
      }
    }
  }
  BKE_pbvh_vertex_iter_end;
}

static void fake_neighbor_search_reduce(const void *__restrict UNUSED(userdata),
                                        void *__restrict chunk_join,
                                        void *__restrict chunk)
{
  NearestVertexFakeNeighborTLSData *join = chunk_join;
  NearestVertexFakeNeighborTLSData *nvtd = chunk;

  if (join->nearest_vertex_index == -1) {
    join->nearest_vertex_index = nvtd->nearest_vertex_index;
    join->nearest_vertex = nvtd->nearest_vertex;
    join->nearest_vertex_distance_squared = nvtd->nearest_vertex_distance_squared;
  }
  else if (nvtd->nearest_vertex_distance_squared < join->nearest_vertex_distance_squared) {
    join->nearest_vertex_index = nvtd->nearest_vertex_index;
    join->nearest_vertex = nvtd->nearest_vertex;
    join->nearest_vertex_distance_squared = nvtd->nearest_vertex_distance_squared;
  }
}

static SculptVertRef SCULPT_fake_neighbor_search(Sculpt *sd,
                                                 Object *ob,
                                                 const SculptVertRef index,
                                                 float max_distance)
{
  SculptSession *ss = ob->sculpt;
  PBVHNode **nodes = NULL;
  int totnode;
  SculptSearchSphereData data = {
      .ss = ss,
      .sd = sd,
      .radius_squared = max_distance * max_distance,
      .original = false,
      .center = SCULPT_vertex_co_get(ss, index),
  };
  BKE_pbvh_search_gather(ss->pbvh, SCULPT_search_sphere_cb, &data, &nodes, &totnode);

  if (totnode == 0) {
    return BKE_pbvh_make_vref(-1);
  }

  SculptThreadedTaskData task_data = {
      .sd = sd,
      .ob = ob,
      .nodes = nodes,
      .max_distance_squared = max_distance * max_distance,
  };

  copy_v3_v3(task_data.nearest_vertex_search_co, SCULPT_vertex_co_get(ss, index));

  NearestVertexFakeNeighborTLSData nvtd;
  nvtd.nearest_vertex_index = -1;
  nvtd.nearest_vertex.i = -1;
  nvtd.nearest_vertex_distance_squared = FLT_MAX;
  nvtd.current_topology_id = SCULPT_vertex_get_connected_component(ss, index);

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  settings.func_reduce = fake_neighbor_search_reduce;
  settings.userdata_chunk = &nvtd;
  settings.userdata_chunk_size = sizeof(NearestVertexFakeNeighborTLSData);
  BLI_task_parallel_range(0, totnode, &task_data, do_fake_neighbor_search_task_cb, &settings);

  MEM_SAFE_FREE(nodes);

  return nvtd.nearest_vertex;
}

typedef struct SculptTopologyIDFloodFillData {
  int next_id;
} SculptTopologyIDFloodFillData;

static bool SCULPT_connected_components_floodfill_cb(SculptSession *ss,
                                                     SculptVertRef from_v,
                                                     SculptVertRef to_v,
                                                     bool UNUSED(is_duplicate),
                                                     void *userdata)
{
  SculptTopologyIDFloodFillData *data = userdata;
  ss->vertex_info.connected_component[BKE_pbvh_vertex_index_to_table(ss->pbvh, from_v)] =
      data->next_id;
  ss->vertex_info.connected_component[BKE_pbvh_vertex_index_to_table(ss->pbvh, to_v)] =
      data->next_id;
  return true;
}

void SCULPT_connected_components_ensure(Object *ob)
{
  SculptSession *ss = ob->sculpt;

  SCULPT_vertex_random_access_ensure(ss);

  /* Topology IDs already initialized. They only need to be recalculated when the PBVH is
   * rebuild.
   */
  if (ss->vertex_info.connected_component) {
    return;
  }

  const int totvert = SCULPT_vertex_count_get(ss);
  ss->vertex_info.connected_component = MEM_malloc_arrayN(totvert, sizeof(int), "topology ID");

  for (int i = 0; i < totvert; i++) {
    ss->vertex_info.connected_component[i] = SCULPT_TOPOLOGY_ID_NONE;
  }

  int next_id = 0;
  for (int i = 0; i < totvert; i++) {
    SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

    if (!SCULPT_vertex_visible_get(ss, vertex)) {
      continue;
    }

    if (ss->vertex_info.connected_component[i] == SCULPT_TOPOLOGY_ID_NONE) {
      SculptFloodFill flood;
      SCULPT_floodfill_init(ss, &flood);
      SCULPT_floodfill_add_initial(&flood, vertex);
      SculptTopologyIDFloodFillData data;
      data.next_id = next_id;
      SCULPT_floodfill_execute(ss, &flood, SCULPT_connected_components_floodfill_cb, &data);
      SCULPT_floodfill_free(&flood);
      next_id++;
    }
  }
}

void SCULPT_boundary_info_ensure(Object *object)
{
  SculptSession *ss = object->sculpt;

  // PBVH_BMESH now handles itself
  if (ss->bm) {
    return;
  }
  else {
    if (ss->vertex_info.boundary) {
      return;
    }

    Mesh *base_mesh = BKE_mesh_from_object(object);
    ss->vertex_info.boundary = BLI_BITMAP_NEW(base_mesh->totvert, "Boundary info");
    int *adjacent_faces_edge_count = MEM_calloc_arrayN(
        base_mesh->totedge, sizeof(int), "Adjacent face edge count");

    for (int p = 0; p < base_mesh->totpoly; p++) {
      MPoly *poly = &base_mesh->mpoly[p];
      for (int l = 0; l < poly->totloop; l++) {
        MLoop *loop = &base_mesh->mloop[l + poly->loopstart];
        adjacent_faces_edge_count[loop->e]++;
      }
    }

    for (int e = 0; e < base_mesh->totedge; e++) {
      if (adjacent_faces_edge_count[e] < 2) {
        MEdge *edge = &base_mesh->medge[e];
        BLI_BITMAP_SET(ss->vertex_info.boundary, edge->v1, true);
        BLI_BITMAP_SET(ss->vertex_info.boundary, edge->v2, true);
      }
    }

    MEM_freeN(adjacent_faces_edge_count);
  }
}

void SCULPT_fake_neighbors_ensure(Sculpt *sd, Object *ob, const float max_dist)
{
  SculptSession *ss = ob->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);

  /* Fake neighbors were already initialized with the same distance, so no need to be
   * recalculated.
   */
  if (ss->fake_neighbors.fake_neighbor_index &&
      ss->fake_neighbors.current_max_distance == max_dist) {
    return;
  }

  SCULPT_connected_components_ensure(ob);
  SCULPT_fake_neighbor_init(ss, max_dist);

  for (int i = 0; i < totvert; i++) {
    const SculptVertRef from_v = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);

    /* This vertex does not have a fake neighbor yet, seach one for it. */
    if (ss->fake_neighbors.fake_neighbor_index[i].i == FAKE_NEIGHBOR_NONE) {
      const SculptVertRef to_v = SCULPT_fake_neighbor_search(sd, ob, from_v, max_dist);

      if (to_v.i != -1) {
        /* Add the fake neighbor if available. */
        SCULPT_fake_neighbor_add(ss, from_v, to_v);
      }
    }
  }
}

void SCULPT_fake_neighbors_enable(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  BLI_assert(ss->fake_neighbors.fake_neighbor_index != NULL);
  ss->fake_neighbors.use_fake_neighbors = true;
}

void SCULPT_fake_neighbors_disable(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  BLI_assert(ss->fake_neighbors.fake_neighbor_index != NULL);
  ss->fake_neighbors.use_fake_neighbors = false;
}

void SCULPT_fake_neighbors_free(Object *ob)
{
  SculptSession *ss = ob->sculpt;
  sculpt_pose_fake_neighbors_free(ss);
}

/**
 * #sculpt_mask_by_color_delta_get returns values in the (0,1) range that are used to generate the
 * mask based on the difference between two colors (the active color and the color of any other
 * vertex). Ideally, a threshold of 0 should mask only the colors that are equal to the active
 * color and threshold of 1 should mask all colors. In order to avoid artifacts and produce softer
 * falloffs in the mask, the MASK_BY_COLOR_SLOPE defines the size of the transition values between
 * masked and unmasked vertices. The smaller this value is, the sharper the generated mask is going
 * to be.
 */
#define MASK_BY_COLOR_SLOPE 0.25f

static float sculpt_mask_by_color_delta_get(const float *color_a,
                                            const float *color_b,
                                            const float threshold,
                                            const bool invert)
{
  float len = len_v3v3(color_a, color_b);
  /* Normalize len to the (0, 1) range. */
  len = len / M_SQRT3;

  if (len < threshold - MASK_BY_COLOR_SLOPE) {
    len = 1.0f;
  }
  else if (len >= threshold) {
    len = 0.0f;
  }
  else {
    len = (-len + threshold) / MASK_BY_COLOR_SLOPE;
  }

  if (invert) {
    return 1.0f - len;
  }
  return len;
}

static float sculpt_mask_by_color_final_mask_get(const float current_mask,
                                                 const float new_mask,
                                                 const bool invert,
                                                 const bool preserve_mask)
{
  if (preserve_mask) {
    if (invert) {
      return min_ff(current_mask, new_mask);
    }
    return max_ff(current_mask, new_mask);
  }
  return new_mask;
}

typedef struct MaskByColorContiguousFloodFillData {
  float threshold;
  bool invert;
  float *new_mask;
  float initial_color[3];
} MaskByColorContiguousFloodFillData;

static void do_mask_by_color_contiguous_update_nodes_cb(
    void *__restrict userdata, const int n, const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;

  SCULPT_undo_push_node(data->ob, data->nodes[n], SCULPT_UNDO_MASK);
  bool update_node = false;

  const bool invert = data->mask_by_color_invert;
  const bool preserve_mask = data->mask_by_color_preserve_mask;

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    const float current_mask = *vd.mask;
    const float new_mask = data->mask_by_color_floodfill[vd.index];
    *vd.mask = sculpt_mask_by_color_final_mask_get(current_mask, new_mask, invert, preserve_mask);
    if (current_mask == *vd.mask) {
      continue;
    }
    update_node = true;
    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
  if (update_node) {
    BKE_pbvh_node_mark_redraw(data->nodes[n]);
  }
}

static bool sculpt_mask_by_color_contiguous_floodfill_cb(
    SculptSession *ss, SculptVertRef from_v, SculptVertRef to_v, bool is_duplicate, void *userdata)
{
  MaskByColorContiguousFloodFillData *data = userdata;
  int to_v_i = BKE_pbvh_vertex_index_to_table(ss->pbvh, to_v);
  int from_v_i = BKE_pbvh_vertex_index_to_table(ss->pbvh, from_v);

  const float *current_color = SCULPT_vertex_color_get(ss, to_v);
  float new_vertex_mask = sculpt_mask_by_color_delta_get(
      current_color, data->initial_color, data->threshold, data->invert);
  data->new_mask[to_v_i] = new_vertex_mask;

  if (is_duplicate) {
    data->new_mask[to_v_i] = data->new_mask[from_v_i];
  }

  float len = len_v3v3(current_color, data->initial_color);
  len = len / M_SQRT3;
  return len <= data->threshold;
}

static void sculpt_mask_by_color_contiguous(Object *object,
                                            const SculptVertRef vertex,
                                            const float threshold,
                                            const bool invert,
                                            const bool preserve_mask)
{
  SculptSession *ss = object->sculpt;
  const int totvert = SCULPT_vertex_count_get(ss);

  float *new_mask = MEM_calloc_arrayN(totvert, sizeof(float), "new mask");

  if (invert) {
    for (int i = 0; i < totvert; i++) {
      new_mask[i] = 1.0f;
    }
  }

  SculptFloodFill flood;
  SCULPT_floodfill_init(ss, &flood);
  SCULPT_floodfill_add_initial(&flood, vertex);

  MaskByColorContiguousFloodFillData ffd;
  ffd.threshold = threshold;
  ffd.invert = invert;
  ffd.new_mask = new_mask;
  copy_v3_v3(ffd.initial_color, SCULPT_vertex_color_get(ss, vertex));

  SCULPT_floodfill_execute(ss, &flood, sculpt_mask_by_color_contiguous_floodfill_cb, &ffd);
  SCULPT_floodfill_free(&flood);

  int totnode;
  PBVHNode **nodes;
  BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

  SculptThreadedTaskData data = {
      .ob = object,
      .nodes = nodes,
      .mask_by_color_floodfill = new_mask,
      .mask_by_color_vertex = vertex,
      .mask_by_color_threshold = threshold,
      .mask_by_color_invert = invert,
      .mask_by_color_preserve_mask = preserve_mask,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(
      0, totnode, &data, do_mask_by_color_contiguous_update_nodes_cb, &settings);

  MEM_SAFE_FREE(nodes);

  MEM_freeN(new_mask);
}

static void do_mask_by_color_task_cb(void *__restrict userdata,
                                     const int n,
                                     const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;

  SCULPT_undo_push_node(data->ob, data->nodes[n], SCULPT_UNDO_MASK);
  bool update_node = false;

  const float threshold = data->mask_by_color_threshold;
  const bool invert = data->mask_by_color_invert;
  const bool preserve_mask = data->mask_by_color_preserve_mask;
  const float *active_color = SCULPT_vertex_color_get(ss, data->mask_by_color_vertex);

  PBVHVertexIter vd;
  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    const float current_mask = *vd.mask;
    const float new_mask = sculpt_mask_by_color_delta_get(active_color, vd.col, threshold, invert);
    *vd.mask = sculpt_mask_by_color_final_mask_get(current_mask, new_mask, invert, preserve_mask);

    if (current_mask == *vd.mask) {
      continue;
    }
    update_node = true;
    if (vd.mvert) {
      vd.mvert->flag |= ME_VERT_PBVH_UPDATE;
    }
  }
  BKE_pbvh_vertex_iter_end;
  if (update_node) {
    BKE_pbvh_node_mark_redraw(data->nodes[n]);
  }
}

static void sculpt_mask_by_color_full_mesh(Object *object,
                                           const SculptVertRef vertex,
                                           const float threshold,
                                           const bool invert,
                                           const bool preserve_mask)
{
  SculptSession *ss = object->sculpt;

  int totnode;
  PBVHNode **nodes;
  BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);

  SculptThreadedTaskData data = {
      .ob = object,
      .nodes = nodes,
      .mask_by_color_vertex = vertex,
      .mask_by_color_threshold = threshold,
      .mask_by_color_invert = invert,
      .mask_by_color_preserve_mask = preserve_mask,
  };

  TaskParallelSettings settings;
  BKE_pbvh_parallel_range_settings(&settings, true, totnode);
  BLI_task_parallel_range(0, totnode, &data, do_mask_by_color_task_cb, &settings);

  MEM_SAFE_FREE(nodes);
}

static int sculpt_mask_by_color_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, false);

  /* Color data is not available in Multires. */
  if (BKE_pbvh_type(ss->pbvh) != PBVH_FACES) {
    return OPERATOR_CANCELLED;
  }

  if (!ss->vcol) {
    return OPERATOR_CANCELLED;
  }

  SCULPT_vertex_random_access_ensure(ss);

  /* Tools that are not brushes do not have the brush gizmo to update the vertex as the mouse
   * move, so it needs to be updated here. */
  SculptCursorGeometryInfo sgi;
  float mouse[2];
  mouse[0] = event->mval[0];
  mouse[1] = event->mval[1];
  SCULPT_cursor_geometry_info_update(C, &sgi, mouse, false, false);

  SCULPT_undo_push_begin(ob, "Mask by color");

  const SculptVertRef active_vertex = SCULPT_active_vertex_get(ss);
  const float threshold = RNA_float_get(op->ptr, "threshold");
  const bool invert = RNA_boolean_get(op->ptr, "invert");
  const bool preserve_mask = RNA_boolean_get(op->ptr, "preserve_previous_mask");

  if (RNA_boolean_get(op->ptr, "contiguous")) {
    sculpt_mask_by_color_contiguous(ob, active_vertex, threshold, invert, preserve_mask);
  }
  else {
    sculpt_mask_by_color_full_mesh(ob, active_vertex, threshold, invert, preserve_mask);
  }

  BKE_pbvh_update_vertex_data(ss->pbvh, PBVH_UpdateMask);
  SCULPT_undo_push_end();

  SCULPT_flush_update_done(C, ob, SCULPT_UPDATE_MASK);

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_mask_by_color(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Mask by Color";
  ot->idname = "SCULPT_OT_mask_by_color";
  ot->description = "Creates a mask based on the sculpt vertex colors";

  /* api callbacks */
  ot->invoke = sculpt_mask_by_color_invoke;
  ot->poll = SCULPT_vertex_colors_poll;

  ot->flag = OPTYPE_REGISTER;

  ot->prop = RNA_def_boolean(
      ot->srna, "contiguous", false, "Contiguous", "Mask only contiguous color areas");

  ot->prop = RNA_def_boolean(ot->srna, "invert", false, "Invert", "Invert the generated mask");
  ot->prop = RNA_def_boolean(
      ot->srna,
      "preserve_previous_mask",
      false,
      "Preserve Previous Mask",
      "Preserve the previous mask and add or subtract the new one generated by the colors");

  RNA_def_float(ot->srna,
                "threshold",
                0.35f,
                0.0f,
                1.0f,
                "Threshold",
                "How much changes in color affect the mask generation",
                0.0f,
                1.0f);
}

static int sculpt_reset_brushes_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);

  LISTBASE_FOREACH (Brush *, br, &bmain->brushes) {
    if (br->ob_mode != OB_MODE_SCULPT) {
      continue;
    }
    BKE_brush_sculpt_reset(br);
    WM_event_add_notifier(C, NC_BRUSH | NA_EDITED, br);
  }

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_reset_brushes(struct wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Reset Sculpt Brushes";
  ot->idname = "SCULPT_OT_reset_brushes";
  ot->description = "Resets all sculpt brushes to their default value";

  /* API callbacks. */
  ot->exec = sculpt_reset_brushes_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER;
}

static int sculpt_set_limit_surface_exec(bContext *C, wmOperator *UNUSED(op))
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Object *ob = CTX_data_active_object(C);
  BKE_sculpt_update_object_for_edit(depsgraph, ob, true, true, false);
  SculptSession *ss = ob->sculpt;

  if (!ss) {
    return OPERATOR_FINISHED;
  }

  SCULPT_vertex_random_access_ensure(ss);

  if (ss->limit_surface) {
    SCULPT_temp_customlayer_release(ss, ss->limit_surface);
  }

  MEM_SAFE_FREE(ss->limit_surface);

  ss->limit_surface = MEM_callocN(sizeof(SculptCustomLayer), "ss->limit_surface");
  SculptLayerParams params = {.permanent = false, .simple_array = false};

  SCULPT_temp_customlayer_ensure(
      ss, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, "_sculpt_limit_surface", &params);
  SCULPT_temp_customlayer_get(
      ss, ATTR_DOMAIN_POINT, CD_PROP_FLOAT3, "_sculpt_limit_surface", ss->limit_surface, &params);

  const int totvert = SCULPT_vertex_count_get(ss);
  const bool weighted = false;
  for (int i = 0; i < totvert; i++) {
    SculptVertRef vertex = BKE_pbvh_table_index_to_vertex(ss->pbvh, i);
    float *f = SCULPT_temp_cdata_get(vertex, ss->limit_surface);

    SCULPT_neighbor_coords_average(ss, f, vertex, 0.0, true, weighted);
  }

  return OPERATOR_FINISHED;
}

static void SCULPT_OT_set_limit_surface(wmOperatorType *ot)
{
  /* Identifiers. */
  ot->name = "Set Limit Surface";
  ot->idname = "SCULPT_OT_set_limit_surface";
  ot->description = "Calculates and stores a limit surface from the current mesh";

  /* API callbacks. */
  ot->exec = sculpt_set_limit_surface_exec;
  ot->poll = SCULPT_mode_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

#if 0
/* -------------------------------------------------------------------- */
/** \name Dyntopo Detail Size Edit Operator
 * \{ */

/* Defines how much the mouse movement will modify the detail size value. */
#  define DETAIL_SIZE_DELTA_SPEED 0.08f
#  define DETAIL_SIZE_DELTA_ACCURATE_SPEED 0.004f

typedef struct DyntopoDetailSizeEditCustomData {
  void *draw_handle;
  Object *active_object;

  float init_mval[2];
  float accurate_mval[2];

  float outline_col[4];

  bool accurate_mode;
  bool sample_mode;

  float init_detail_size;
  float accurate_detail_size;
  float detail_size;
  float radius;

  float preview_tri[3][3];
  float gizmo_mat[4][4];
} DyntopoDetailSizeEditCustomData;

static void dyntopo_detail_size_parallel_lines_draw(uint pos3d,
                                                    DyntopoDetailSizeEditCustomData *cd,
                                                    const float start_co[3],
                                                    const float end_co[3],
                                                    bool flip,
                                                    const float angle)
{
  float object_space_constant_detail = 1.0f /
                                       (cd->detail_size * mat4_to_scale(cd->active_object->obmat));

  /* The constant detail represents the maximum edge length allowed before subdividing it. If the
   * triangle grid preview is created with this value it will represent an ideal mesh density where
   * all edges have the exact maximum length, which never happens in practice. As the minimum edge
   * length for dyntopo is 0.4 * max_edge_length, this adjust the detail size to the average
   * between max and min edge length so the preview is more accurate. */
  object_space_constant_detail *= 0.7f;

  const float total_len = len_v3v3(cd->preview_tri[0], cd->preview_tri[1]);
  const int tot_lines = (int)(total_len / object_space_constant_detail) + 1;
  const float tot_lines_fl = total_len / object_space_constant_detail;
  float spacing_disp[3];
  sub_v3_v3v3(spacing_disp, end_co, start_co);
  normalize_v3(spacing_disp);

  float line_disp[3];
  rotate_v2_v2fl(line_disp, spacing_disp, DEG2RAD(angle));
  mul_v3_fl(spacing_disp, total_len / tot_lines_fl);

  immBegin(GPU_PRIM_LINES, (uint)tot_lines * 2);
  for (int i = 0; i < tot_lines; i++) {
    float line_length;
    if (flip) {
      line_length = total_len * ((float)i / (float)tot_lines_fl);
    }
    else {
      line_length = total_len * (1.0f - ((float)i / (float)tot_lines_fl));
    }
    float line_start[3];
    copy_v3_v3(line_start, start_co);
    madd_v3_v3v3fl(line_start, line_start, spacing_disp, i);
    float line_end[3];
    madd_v3_v3v3fl(line_end, line_start, line_disp, line_length);
    immVertex3fv(pos3d, line_start);
    immVertex3fv(pos3d, line_end);
  }
  immEnd();
}

static void dyntopo_detail_size_edit_draw(const bContext *UNUSED(C),
                                          ARegion *UNUSED(ar),
                                          void *arg)
{
  DyntopoDetailSizeEditCustomData *cd = arg;
  GPU_blend(GPU_BLEND_ALPHA);
  GPU_line_smooth(true);

  uint pos3d = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 3, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  GPU_matrix_push();
  GPU_matrix_mul(cd->gizmo_mat);

  /* Draw Cursor */
  immUniformColor4fv(cd->outline_col);
  GPU_line_width(3.0f);

  imm_draw_circle_wire_3d(pos3d, 0, 0, cd->radius, 80);

  /* Draw Triangle. */
  immUniformColor4f(0.9f, 0.9f, 0.9f, 0.8f);
  immBegin(GPU_PRIM_LINES, 6);
  immVertex3fv(pos3d, cd->preview_tri[0]);
  immVertex3fv(pos3d, cd->preview_tri[1]);

  immVertex3fv(pos3d, cd->preview_tri[1]);
  immVertex3fv(pos3d, cd->preview_tri[2]);

  immVertex3fv(pos3d, cd->preview_tri[2]);
  immVertex3fv(pos3d, cd->preview_tri[0]);
  immEnd();

  /* Draw Grid */
  GPU_line_width(1.0f);
  dyntopo_detail_size_parallel_lines_draw(
      pos3d, cd, cd->preview_tri[0], cd->preview_tri[1], false, 60.0f);
  dyntopo_detail_size_parallel_lines_draw(
      pos3d, cd, cd->preview_tri[0], cd->preview_tri[1], true, 120.0f);
  dyntopo_detail_size_parallel_lines_draw(
      pos3d, cd, cd->preview_tri[0], cd->preview_tri[2], false, -60.0f);

  immUnbindProgram();
  GPU_matrix_pop();
  GPU_blend(GPU_BLEND_NONE);
  GPU_line_smooth(false);
}

static void dyntopo_detail_size_edit_cancel(bContext *C, wmOperator *op)
{
  Object *active_object = CTX_data_active_object(C);
  SculptSession *ss = active_object->sculpt;
  ARegion *region = CTX_wm_region(C);
  DyntopoDetailSizeEditCustomData *cd = op->customdata;
  ED_region_draw_cb_exit(region->type, cd->draw_handle);
  ss->draw_faded_cursor = false;
  MEM_freeN(op->customdata);
  ED_workspace_status_text(C, NULL);
}

static void dyntopo_detail_size_sample_from_surface(Object *ob,
                                                    DyntopoDetailSizeEditCustomData *cd)
{
  SculptSession *ss = ob->sculpt;
  const SculptVertRef active_vertex = SCULPT_active_vertex_get(ss);

  float len_accum = 0;
  int num_neighbors = 0;
  SculptVertexNeighborIter ni;
  SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, active_vertex, ni) {
    len_accum += len_v3v3(SCULPT_vertex_co_get(ss, active_vertex),
                          SCULPT_vertex_co_get(ss, ni.vertex));
    num_neighbors++;
  }
  SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

  if (num_neighbors > 0) {
    const float avg_edge_len = len_accum / num_neighbors;
    /* Use 0.7 as the average of min and max dyntopo edge length. */
    const float detail_size = 0.7f / (avg_edge_len * mat4_to_scale(cd->active_object->obmat));
    cd->detail_size = clamp_f(detail_size, 1.0f, 500.0f);
  }
}

static void dyntopo_detail_size_update_from_mouse_delta(DyntopoDetailSizeEditCustomData *cd,
                                                        const wmEvent *event)
{
  const float mval[2] = {event->mval[0], event->mval[1]};

  float detail_size_delta;
  if (cd->accurate_mode) {
    detail_size_delta = mval[0] - cd->accurate_mval[0];
    cd->detail_size = cd->accurate_detail_size +
                      detail_size_delta * DETAIL_SIZE_DELTA_ACCURATE_SPEED;
  }
  else {
    detail_size_delta = mval[0] - cd->init_mval[0];
    cd->detail_size = cd->init_detail_size + detail_size_delta * DETAIL_SIZE_DELTA_SPEED;
  }

  if (event->type == EVT_LEFTSHIFTKEY && event->val == KM_PRESS) {
    cd->accurate_mode = true;
    copy_v2_v2(cd->accurate_mval, mval);
    cd->accurate_detail_size = cd->detail_size;
  }
  if (event->type == EVT_LEFTSHIFTKEY && event->val == KM_RELEASE) {
    cd->accurate_mode = false;
    cd->accurate_detail_size = 0.0f;
  }

  cd->detail_size = clamp_f(cd->detail_size, 1.0f, 500.0f);
}

static int dyntopo_detail_size_edit_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  Object *active_object = CTX_data_active_object(C);
  SculptSession *ss = active_object->sculpt;
  ARegion *region = CTX_wm_region(C);
  DyntopoDetailSizeEditCustomData *cd = op->customdata;
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  /* Cancel modal operator */
  if ((event->type == EVT_ESCKEY && event->val == KM_PRESS) ||
      (event->type == RIGHTMOUSE && event->val == KM_PRESS)) {
    dyntopo_detail_size_edit_cancel(C, op);
    ED_region_tag_redraw(region);
    return OPERATOR_FINISHED;
  }

  /* Finish modal operator */
  if ((event->type == LEFTMOUSE && event->val == KM_RELEASE) ||
      (event->type == EVT_RETKEY && event->val == KM_PRESS) ||
      (event->type == EVT_PADENTER && event->val == KM_PRESS)) {
    ED_region_draw_cb_exit(region->type, cd->draw_handle);
    sd->constant_detail = cd->detail_size;
    ss->draw_faded_cursor = false;
    MEM_freeN(op->customdata);
    ED_region_tag_redraw(region);
    ED_workspace_status_text(C, NULL);
    return OPERATOR_FINISHED;
  }

  ED_region_tag_redraw(region);

  if (event->type == EVT_LEFTCTRLKEY && event->val == KM_PRESS) {
    cd->sample_mode = true;
  }
  if (event->type == EVT_LEFTCTRLKEY && event->val == KM_RELEASE) {
    cd->sample_mode = false;
  }

  /* Sample mode sets the detail size sampling the average edge length under the surface. */
  if (cd->sample_mode) {
    dyntopo_detail_size_sample_from_surface(active_object, cd);
    return OPERATOR_RUNNING_MODAL;
  }
  /* Regular mode, changes the detail size by moving the cursor. */
  dyntopo_detail_size_update_from_mouse_delta(cd, event);

  return OPERATOR_RUNNING_MODAL;
}

static int dyntopo_detail_size_edit_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  ARegion *region = CTX_wm_region(C);
  Object *active_object = CTX_data_active_object(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
  Brush *brush = BKE_paint_brush(&sd->paint);

  DyntopoDetailSizeEditCustomData *cd = MEM_callocN(sizeof(DyntopoDetailSizeEditCustomData),
                                                    "Dyntopo Detail Size Edit OP Custom Data");

  /* Initial operator Custom Data setup. */
  cd->draw_handle = ED_region_draw_cb_activate(
      region->type, dyntopo_detail_size_edit_draw, cd, REGION_DRAW_POST_VIEW);
  cd->active_object = active_object;
  cd->init_mval[0] = event->mval[0];
  cd->init_mval[1] = event->mval[1];
  cd->detail_size = sd->constant_detail;
  cd->init_detail_size = sd->constant_detail;
  copy_v4_v4(cd->outline_col, brush->add_col);
  op->customdata = cd;

  SculptSession *ss = active_object->sculpt;
  cd->radius = ss->cursor_radius;

  /* Generates the matrix to position the gizmo in the surface of the mesh using the same location
   * and orientation as the brush cursor. */
  float cursor_trans[4][4], cursor_rot[4][4];
  const float z_axis[4] = {0.0f, 0.0f, 1.0f, 0.0f};
  float quat[4];
  copy_m4_m4(cursor_trans, active_object->obmat);
  translate_m4(
      cursor_trans, ss->cursor_location[0], ss->cursor_location[1], ss->cursor_location[2]);

  float cursor_normal[3];
  if (!is_zero_v3(ss->cursor_sampled_normal)) {
    copy_v3_v3(cursor_normal, ss->cursor_sampled_normal);
  }
  else {
    copy_v3_v3(cursor_normal, ss->cursor_normal);
  }

  rotation_between_vecs_to_quat(quat, z_axis, cursor_normal);
  quat_to_mat4(cursor_rot, quat);
  copy_m4_m4(cd->gizmo_mat, cursor_trans);
  mul_m4_m4_post(cd->gizmo_mat, cursor_rot);

  /* Initialize the position of the triangle vertices. */
  const float y_axis[3] = {0.0f, cd->radius, 0.0f};
  for (int i = 0; i < 3; i++) {
    zero_v3(cd->preview_tri[i]);
    rotate_v2_v2fl(cd->preview_tri[i], y_axis, DEG2RAD(120.0f * i));
  }

  SCULPT_vertex_random_access_ensure(ss);

  WM_event_add_modal_handler(C, op);
  ED_region_tag_redraw(region);

  ss->draw_faded_cursor = true;

  const char *status_str = TIP_(
      "Move the mouse to change the dyntopo detail size. LMB: confirm size, ESC/RMB: cancel");
  ED_workspace_status_text(C, status_str);

  return OPERATOR_RUNNING_MODAL;
}

static bool dyntopo_detail_size_edit_poll(bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  Sculpt *sd = CTX_data_tool_settings(C)->sculpt;

  return SCULPT_mode_poll(C) && ob->sculpt->bm && (sd->flags & SCULPT_DYNTOPO_DETAIL_CONSTANT);
}

static void SCULPT_OT_dyntopo_detail_size_edit(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Edit Dyntopo Detail Size";
  ot->description = "Modify the constant detail size of dyntopo interactively";
  ot->idname = "SCULPT_OT_dyntopo_detail_size_edit";

  /* api callbacks */
  ot->poll = dyntopo_detail_size_edit_poll;
  ot->invoke = dyntopo_detail_size_edit_invoke;
  ot->modal = dyntopo_detail_size_edit_modal;
  ot->cancel = dyntopo_detail_size_edit_cancel;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

#endif

int SCULPT_vertex_valence_get(const struct SculptSession *ss, SculptVertRef vertex)
{
  SculptVertexNeighborIter ni;
  int tot = 0;
  int mval = -1;

#if 0
  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    BMVert *v = (BMVert *)vertex.i;
    MDynTopoVert *mv = BKE_PBVH_DYNVERT(ss->cd_dyn_vert, v);

    if (mv->flag & DYNVERT_NEED_VALENCE) {
      BKE_pbvh_bmesh_update_valence(ss->cd_dyn_vert, vertex);
    }

    mval = mv->valence;

#  ifdef NDEBUG
    return mval;
#  endif
  }
#else
  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    BMVert *v = (BMVert *)vertex.i;

    return BM_vert_edge_count(v);
  }
#endif

  MDynTopoVert *mv = ss->mdyntopo_verts + vertex.i;

  if (mv->flag & DYNVERT_NEED_VALENCE) {
    mv->flag &= ~DYNVERT_NEED_VALENCE;

    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
      tot++;
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

    mv->valence = tot;
  }

  return mv->valence;
}

typedef struct BMLinkItem {
  struct BMLinkItem *next, *prev;
  BMVert *item;
  int depth;
} BMLinkItem;

static int sculpt_regularize_rake_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  SculptSession *ss = ob->sculpt;

  Object *sculpt_get_vis_object(bContext * C, SculptSession * ss, char *name);
  void sculpt_end_vis_object(bContext * C, SculptSession * ss, Object * ob, BMesh * bm);

  Object *visob = sculpt_get_vis_object(C, ss, "rakevis");
  BMesh *visbm = BM_mesh_create(
      &bm_mesh_allocsize_default,
      &((struct BMeshCreateParams){.create_unique_ids = false, .use_toolflags = false}));

  if (!ss) {
    printf("mising sculpt session\n");
    return OPERATOR_CANCELLED;
  }
  if (!ss->bm) {
    printf("bmesh only!\n");
    return OPERATOR_CANCELLED;
  }

  BMesh *bm = ss->bm;
  BMIter iter;
  BMVert *v;

  PBVHNode **nodes = NULL;
  int totnode;

  int idx = CustomData_get_named_layer_index(&bm->vdata, CD_PROP_COLOR, "_rake_temp");
  if (idx < 0) {
    printf("no rake temp\n");
    return OPERATOR_CANCELLED;
  }

  int cd_vcol = bm->vdata.layers[idx].offset;
  int cd_vcol_vis = -1;

  idx = CustomData_get_named_layer_index(&bm->vdata, CD_PROP_COLOR, "_rake_vis");
  if (idx >= 0) {
    cd_vcol_vis = bm->vdata.layers[idx].offset;
  }

  for (int step = 0; step < 33; step++) {
    BLI_mempool *nodepool = BLI_mempool_create(sizeof(BMLinkItem), bm->totvert, 4196, 0);

    BKE_pbvh_get_nodes(ss->pbvh, PBVH_Leaf, &nodes, &totnode);

    SCULPT_undo_push_begin(ob, "Regularized Rake Directions");
    for (int i = 0; i < totnode; i++) {
      SCULPT_ensure_dyntopo_node_undo(ob, nodes[i], SCULPT_UNDO_COLOR, -1);
      BKE_pbvh_node_mark_update_color(nodes[i]);
    }
    SCULPT_undo_push_end();

    MEM_SAFE_FREE(nodes);

    BMVert **stack = NULL;
    BLI_array_declare(stack);

    bm->elem_index_dirty |= BM_VERT;
    BM_mesh_elem_index_ensure(bm, BM_VERT);

    BLI_bitmap *visit = BLI_BITMAP_NEW(bm->totvert, "regularize rake visit bitmap");

    BMVert **verts = MEM_malloc_arrayN(bm->totvert, sizeof(*verts), "verts");
    BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
      verts[v->head.index] = v;
      v->head.hflag &= ~BM_ELEM_SELECT;
    }

    RNG *rng = BLI_rng_new((uint)BLI_thread_rand(0));
    BLI_rng_shuffle_array(rng, verts, sizeof(void *), bm->totvert);

    for (int i = 0; i < bm->totvert; i++) {
      BMVert *v = verts[i];

      MDynTopoVert *mv = BKE_PBVH_DYNVERT(ss->cd_dyn_vert, v);
      if (mv->flag &
          (DYNVERT_CORNER | DYNVERT_FSET_CORNER | DYNVERT_SHARP_CORNER | DYNVERT_SEAM_CORNER)) {
        continue;
      }

      if (BLI_BITMAP_TEST(visit, v->head.index)) {
        continue;
      }

      // v->head.hflag |= BM_ELEM_SELECT;

      float *dir = BM_ELEM_CD_GET_VOID_P(v, cd_vcol);
      normalize_v3(dir);

      BMLinkItem *node = BLI_mempool_alloc(nodepool);
      node->next = node->prev = NULL;
      node->item = v;
      node->depth = 0;

      BLI_BITMAP_SET(visit, v->head.index, true);

      ListBase queue = {node, node};
      const int boundflag = DYNVERT_BOUNDARY | DYNVERT_FSET_BOUNDARY | DYNVERT_SEAM_BOUNDARY |
                            DYNVERT_SHARP_BOUNDARY;
      while (queue.first) {
        BMLinkItem *node2 = BLI_poptail(&queue);
        BMVert *v2 = node2->item;

        float *dir2 = BM_ELEM_CD_GET_VOID_P(v2, cd_vcol);

        if (cd_vcol_vis >= 0) {
          float *color = BM_ELEM_CD_GET_VOID_P(v2, cd_vcol_vis);
          color[0] = color[1] = color[2] = (float)(node2->depth % 5) / 5.0f;
          color[3] = 1.0f;
        }

        if (step % 5 != 0 && node2->depth > 15) {
          // break;
        }
        // dir2[0] = dir2[1] = dir2[2] = (float)(node2->depth % 5) / 5.0f;
        // dir2[3] = 1.0f;

        BMIter viter;
        BMEdge *e;

        int closest_vec_to_perp(
            float dir[3], float r_dir2[3], float no[3], float *buckets, float w);

        // float buckets[8] = {0};
        float tmp[3];
        float dir32[3];
        float avg[3] = {0.0f};
        float tot = 0.0f;

        // angle_on_axis_v3v3v3_v3
        float tco[3];
        zero_v3(tco);
        add_v3_fl(tco, 1000.0f);
        // madd_v3_v3fl(tco, v2->no, -dot_v3v3(v2->no, tco));

        float tanco[3];
        add_v3_v3v3(tanco, v2->co, dir2);

        SCULPT_dyntopo_check_disk_sort(ss, (SculptVertRef){.i = (intptr_t)v2});

        float lastdir3[3];
        float firstdir3[3];
        bool first = true;
        float thsum = 0.0f;

        // don't propegate across singularities

        BM_ITER_ELEM (e, &viter, v2, BM_EDGES_OF_VERT) {
          // e = l->e;
          BMVert *v3 = BM_edge_other_vert(e, v2);
          float *dir3 = BM_ELEM_CD_GET_VOID_P(v3, cd_vcol);
          float dir32[3];

          copy_v3_v3(dir32, dir3);

          if (first) {
            first = false;
            copy_v3_v3(firstdir3, dir32);
          }
          else {
            float th = saacos(dot_v3v3(dir32, lastdir3));
            thsum += th;
          }

          copy_v3_v3(lastdir3, dir32);

          add_v3_v3(avg, dir32);
          tot += 1.0f;
        }

        thsum += saacos(dot_v3v3(lastdir3, firstdir3));
        bool sing = thsum >= M_PI * 0.5f;

        // still apply smoothing even with singularity?
        if (tot > 0.0f && !(mv->flag & boundflag)) {
          mul_v3_fl(avg, 1.0 / tot);
          interp_v3_v3v3(dir2, dir2, avg, sing ? 0.15 : 0.25);
          normalize_v3(dir2);
        }

        if (sing) {
          v2->head.hflag |= BM_ELEM_SELECT;

          if (node2->depth == 0) {
            continue;
          }
        }

        BM_ITER_ELEM (e, &viter, v2, BM_EDGES_OF_VERT) {
          BMVert *v3 = BM_edge_other_vert(e, v2);
          float *dir3 = BM_ELEM_CD_GET_VOID_P(v3, cd_vcol);

          if (BLI_BITMAP_TEST(visit, v3->head.index)) {
            continue;
          }

          copy_v3_v3(dir32, dir3);
          madd_v3_v3fl(dir32, v2->no, -dot_v3v3(dir3, v2->no));
          normalize_v3(dir32);

          if (dot_v3v3(dir32, dir2) < 0) {
            negate_v3(dir32);
          }

          cross_v3_v3v3(tmp, dir32, v2->no);
          normalize_v3(tmp);

          if (dot_v3v3(tmp, dir2) < 0) {
            negate_v3(tmp);
          }

          float th1 = fabsf(saacos(dot_v3v3(dir2, dir32)));
          float th2 = fabsf(saacos(dot_v3v3(dir2, tmp)));

          if (th2 < th1) {
            copy_v3_v3(dir32, tmp);
          }

          madd_v3_v3fl(dir32, v3->no, -dot_v3v3(dir32, v3->no));
          normalize_v3(dir32);
          copy_v3_v3(dir3, dir32);

          // int bits = closest_vec_to_perp(dir2, dir32, v2->no, buckets, 1.0f);

          MDynTopoVert *mv3 = BKE_PBVH_DYNVERT(ss->cd_dyn_vert, v3);
          if (mv3->flag & boundflag) {
            // continue;
          }

          BLI_BITMAP_SET(visit, v3->head.index, true);

          BMLinkItem *node3 = BLI_mempool_alloc(nodepool);
          node3->next = node3->prev = NULL;
          node3->item = v3;
          node3->depth = node2->depth + 1;

          BLI_addhead(&queue, node3);
        }

        BLI_mempool_free(nodepool, node2);
      }
    }

    MEM_SAFE_FREE(verts);
    BLI_array_free(stack);
    BLI_mempool_destroy(nodepool);
    MEM_SAFE_FREE(visit);
  }

  BMVert *v3;
  BM_ITER_MESH (v3, &iter, bm, BM_VERTS_OF_MESH) {
    float visco[3];
    float *dir3 = BM_ELEM_CD_GET_VOID_P(v3, cd_vcol);

    madd_v3_v3v3fl(visco, v3->co, v3->no, 0.001);
    BMVert *vis1 = BM_vert_create(visbm, visco, NULL, BM_CREATE_NOP);

    madd_v3_v3v3fl(visco, visco, dir3, 0.003);
    BMVert *vis2 = BM_vert_create(visbm, visco, NULL, BM_CREATE_NOP);
    BM_edge_create(visbm, vis1, vis2, NULL, BM_CREATE_NOP);

    float tan[3];
    cross_v3_v3v3(tan, dir3, v3->no);
    madd_v3_v3fl(visco, tan, 0.001);

    vis1 = BM_vert_create(visbm, visco, NULL, BM_CREATE_NOP);
    BM_edge_create(visbm, vis1, vis2, NULL, BM_CREATE_NOP);
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

  sculpt_end_vis_object(C, ss, visob, visbm);

  return OPERATOR_FINISHED;
}

void SCULPT_OT_regularize_rake_directions(wmOperatorType *ot)
{
  ot->name = "Regularize Rake Directions";
  ot->idname = "SCULPT_OT_regularize_rake_directions";
  ot->description = "Development operator";

  /* API callbacks. */
  ot->poll = SCULPT_mode_poll;
  ot->exec = sculpt_regularize_rake_exec;
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

void ED_operatortypes_sculpt(void)
{
  WM_operatortype_append(SCULPT_OT_brush_stroke);
  WM_operatortype_append(SCULPT_OT_sculptmode_toggle);
  WM_operatortype_append(SCULPT_OT_set_persistent_base);
  WM_operatortype_append(SCULPT_OT_set_limit_surface);
  WM_operatortype_append(SCULPT_OT_dynamic_topology_toggle);
  WM_operatortype_append(SCULPT_OT_optimize);
  WM_operatortype_append(SCULPT_OT_symmetrize);
  WM_operatortype_append(SCULPT_OT_detail_flood_fill);
  WM_operatortype_append(SCULPT_OT_sample_detail_size);
  WM_operatortype_append(SCULPT_OT_set_detail_size);
  WM_operatortype_append(SCULPT_OT_mesh_filter);
  WM_operatortype_append(SCULPT_OT_mask_filter);
  WM_operatortype_append(SCULPT_OT_dirty_mask);
  WM_operatortype_append(SCULPT_OT_mask_expand);
  WM_operatortype_append(SCULPT_OT_set_pivot_position);
  WM_operatortype_append(SCULPT_OT_face_sets_create);
  WM_operatortype_append(SCULPT_OT_face_sets_change_visibility);
  WM_operatortype_append(SCULPT_OT_face_sets_randomize_colors);
  WM_operatortype_append(SCULPT_OT_cloth_filter);
  WM_operatortype_append(SCULPT_OT_face_sets_edit);
  WM_operatortype_append(SCULPT_OT_face_set_lasso_gesture);
  WM_operatortype_append(SCULPT_OT_face_set_box_gesture);
  WM_operatortype_append(SCULPT_OT_trim_box_gesture);
  WM_operatortype_append(SCULPT_OT_trim_lasso_gesture);
  WM_operatortype_append(SCULPT_OT_project_line_gesture);
  WM_operatortype_append(SCULPT_OT_project_lasso_gesture);
  WM_operatortype_append(SCULPT_OT_project_box_gesture);

  WM_operatortype_append(SCULPT_OT_sample_color);
  WM_operatortype_append(SCULPT_OT_loop_to_vertex_colors);
  WM_operatortype_append(SCULPT_OT_vertex_to_loop_colors);
  WM_operatortype_append(SCULPT_OT_color_filter);
  WM_operatortype_append(SCULPT_OT_mask_by_color);
  WM_operatortype_append(SCULPT_OT_dyntopo_detail_size_edit);
  WM_operatortype_append(SCULPT_OT_mask_init);

  WM_operatortype_append(SCULPT_OT_face_sets_init);
  WM_operatortype_append(SCULPT_OT_reset_brushes);
  WM_operatortype_append(SCULPT_OT_ipmask_filter);
  WM_operatortype_append(SCULPT_OT_face_set_by_topology);

  WM_operatortype_append(SCULPT_OT_spatial_sort_mesh);

  WM_operatortype_append(SCULPT_OT_expand);
  WM_operatortype_append(SCULPT_OT_regularize_rake_directions);
}

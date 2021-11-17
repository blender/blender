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
#include "BLI_math_color.h"
#include "BLI_math_color_blend.h"
#include "BLI_memarena.h"
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
typedef void (*BrushActionFunc)(
    Sculpt *sd, Object *ob, Brush *brush, UnifiedPaintSettings *ups, void *data);

void sculpt_combine_proxies(Sculpt *sd, Object *ob);
static void SCULPT_run_commandlist(
    Sculpt *sd, Object *ob, Brush *brush, BrushCommandList *list, UnifiedPaintSettings *ups);
static void do_symmetrical_brush_actions(
    Sculpt *sd, Object *ob, BrushActionFunc action, UnifiedPaintSettings *ups, void *userdata);

/* Sculpt API to get brush channel data
  If ss->cache exists then ss->cache->channels_final
  will be used, otherwise brush and tool settings channels
  will be used (taking inheritence into account).
*/

static BrushChannelSet *sculpt_get_brush_channels(const SculptSession *ss, const Brush *br)
{
  if (ss->cache && ss->cache->tool_override_channels) {
    return ss->cache->tool_override_channels;
  }
  else {
    return br->channels;
  }
}

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
    return BKE_brush_channelset_get_final_float(
        sculpt_get_brush_channels(ss, br), sd->channels, idname, mapdata);
  }
  else if (br && br->channels) {
    return BKE_brush_channelset_get_float(sculpt_get_brush_channels(ss, br), idname, mapdata);
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
    return BKE_brush_channelset_get_final_int(
        sculpt_get_brush_channels(ss, br), sd->channels, idname, mapdata);
  }
  else if (br && br->channels) {
    return BKE_brush_channelset_get_int(sculpt_get_brush_channels(ss, br), idname, mapdata);
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
    return BKE_brush_channelset_get_final_vector(
        sculpt_get_brush_channels(ss, br), sd->channels, idname, out, mapdata);
  }
  else if (br && br->channels) {
    return BKE_brush_channelset_get_vector(
        sculpt_get_brush_channels(ss, br), idname, out, mapdata);
  }
  else if (sd && sd->channels) {
    return BKE_brush_channelset_get_vector(sd->channels, idname, out, mapdata);
  }
  else {
    // should not happen!
    return 0;
  }
}

BrushChannel *SCULPT_get_final_channel_intern(const SculptSession *ss,
                                              const char *idname,
                                              const Sculpt *sd,
                                              const Brush *br)
{
  BrushChannel *ch = NULL;

  if (ss->cache && ss->cache->channels_final) {
    ch = BKE_brush_channelset_lookup(ss->cache->channels_final, idname);
  }
  else if (br && br->channels && sd && sd->channels) {
    ch = BKE_brush_channelset_lookup(sculpt_get_brush_channels(ss, br), idname);
    BrushChannel *ch2 = BKE_brush_channelset_lookup(sd->channels, idname);

    if (ch2 && (!ch || (ch->flag & BRUSH_CHANNEL_INHERIT))) {
      ch = ch2;
    }
  }
  else if (br && br->channels) {
    ch = BKE_brush_channelset_lookup(sculpt_get_brush_channels(ss, br), idname);
  }
  else if (sd && sd->channels) {
    ch = BKE_brush_channelset_lookup(sd->channels, idname);
  }

  return ch;
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

int SCULPT_vertex_count_get(const SculptSession *ss)
{
  return BKE_sculptsession_get_totvert(ss);
}

MSculptVert *SCULPT_vertex_get_mdyntopo(const SculptSession *ss, SculptVertRef vertex)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      BMVert *v = (BMVert *)vertex.i;
      return BKE_PBVH_SCULPTVERT(ss->cd_sculpt_vert, v);
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
      return BKE_PBVH_SCULPTVERT(ss->cd_sculpt_vert, v)->origco;
    }

    case PBVH_GRIDS:
    case PBVH_FACES: {
      return ss->mdyntopo_verts[vertex.i].origco;
    }
  }

  return NULL;
}

float *SCULPT_vertex_origno_get(SculptSession *ss, SculptVertRef vertex)
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      BMVert *v = (BMVert *)vertex.i;
      return BKE_PBVH_SCULPTVERT(ss->cd_sculpt_vert, v)->origno;
    }

    case PBVH_GRIDS:
    case PBVH_FACES: {
      return ss->mdyntopo_verts[vertex.i].origno;
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

bool SCULPT_has_colors(const SculptSession *ss)
{
  return ss->vcol_type != -1;
}

bool SCULPT_vertex_color_get(const SculptSession *ss, SculptVertRef vertex, float out[4])
{
  if (vertex.i == SCULPT_REF_NONE) {
    return false;
  }

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      if (!(ss->vcol || ss->mcol)) {
        zero_v4(out);
        return false;
      }

      if (ss->vcol_domain == ATTR_DOMAIN_CORNER) {
        zero_v4(out);

        int count = ss->pmap[vertex.i].count;
        for (int i = 0; i < count; i++) {
          MPoly *mp = ss->mpoly + ss->pmap[vertex.i].indices[i];
          MLoop *ml = ss->mloop + mp->loopstart;
          int li = mp->loopstart;

          for (int j = 0; j < mp->totloop; j++, li++, ml++) {
            if (ml->v != vertex.i) {
              continue;
            }

            if (ss->vcol_type == CD_MLOOPCOL) {
              MLoopCol *col = ss->mcol + li;

              float temp[4];

              rgba_uchar_to_float(temp, (const char *)col);
              srgb_to_linearrgb_v3_v3(temp, temp);

              add_v4_v4(out, temp);
            }
            else if (ss->vcol_type == CD_PROP_COLOR) {
              add_v4_v4(out, ss->vcol[li].color);
            }
          }
        }

        if (count) {
          mul_v4_fl(out, 1.0f / (float)count);
        }
      }
      else {
        if (ss->vcol_type == CD_MLOOPCOL) {
          MLoopCol *col = ss->mcol + vertex.i;

          float temp[4];
          rgba_uchar_to_float(temp, (const char *)col);
          srgb_to_linearrgb_v3_v3(temp, temp);

          copy_v4_v4(out, temp);
        }
        if (ss->vcol_type == CD_PROP_COLOR) {
          copy_v4_v4(out, ss->vcol[vertex.i].color);
        }
      }

      return ss->vcol || ss->mcol;
    case PBVH_BMESH: {
      BMVert *v = (BMVert *)vertex.i;

      if (ss->vcol_type == -1) {
        return false;
      }

      if (ss->vcol_domain == ATTR_DOMAIN_POINT) {
        switch (ss->vcol_type) {
          case CD_PROP_COLOR:
            copy_v4_v4(out, (float *)BM_ELEM_CD_GET_VOID_P(v, ss->cd_vcol_offset));
            break;

          case CD_MLOOPCOL: {
            MLoopCol *mp = (MLoopCol *)BM_ELEM_CD_GET_VOID_P(v, ss->cd_vcol_offset);

            float temp[4];
            rgba_uchar_to_float(temp, (const char *)mp);
            srgb_to_linearrgb_v3_v3(temp, temp);

            copy_v4_v4(out, temp);

            break;
          }
        }
      }
      else {
        int tot = 0;

        BMEdge *e = v->e;

        if (!e) {
          return false;
        }

        zero_v4(out);

        do {
          if (!e->l) {
            continue;
          }

          BMLoop *l = e->l;
          do {
            BMLoop *l2 = l->v != v ? l->next : l;

            switch (ss->vcol_type) {
              case CD_PROP_COLOR:
                add_v4_v4(out, (float *)BM_ELEM_CD_GET_VOID_P(l2, ss->cd_vcol_offset));
                tot++;
                break;

              case CD_MLOOPCOL: {
                MLoopCol *mp = (MLoopCol *)BM_ELEM_CD_GET_VOID_P(l2, ss->cd_vcol_offset);

                float temp[4];
                rgba_uchar_to_float(temp, (const char *)mp);

                srgb_to_linearrgb_v3_v3(temp, temp);

                add_v4_v4(out, temp);
                tot++;

                break;
              }
            }
          } while ((l = l->radial_next) != e->l);
        } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);

        if (tot > 0) {
          mul_v4_fl(out, 1.0f / (float)tot);
        }
      }

      return true;
    }

    case PBVH_GRIDS:
      break;
  }

  return false;
}

void SCULPT_vertex_color_set(const SculptSession *ss, SculptVertRef vertex, float color[4])
{
  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_FACES:
      if (!(ss->vcol || ss->mcol)) {
        return;
      }

      if (ss->vcol_domain == ATTR_DOMAIN_CORNER) {
        int count = ss->pmap[vertex.i].count;
        for (int i = 0; i < count; i++) {
          MPoly *mp = ss->mpoly + ss->pmap[vertex.i].indices[i];
          MLoop *ml = ss->mloop + mp->loopstart;
          int li = mp->loopstart;

          for (int j = 0; j < mp->totloop; j++, li++, ml++) {
            if (ml->v != vertex.i) {
              continue;
            }

            if (ss->vcol_type == CD_MLOOPCOL) {
              float temp[4];

              MLoopCol *col = ss->mcol + li;

              linearrgb_to_srgb_v3_v3(temp, color);
              temp[3] = color[3];

              rgba_float_to_uchar((char *)col, temp);
            }
            else if (ss->vcol_type == CD_PROP_COLOR) {
              copy_v4_v4(ss->vcol[li].color, color);
            }
          }
        }
      }
      else {
        if (ss->vcol_type == CD_MLOOPCOL) {
          MLoopCol *col = ss->mcol + vertex.i;
          float temp[4];

          linearrgb_to_srgb_v3_v3(temp, color);
          temp[3] = color[3];

          rgba_float_to_uchar((char *)col, temp);
        }
        else if (ss->vcol_type == CD_PROP_COLOR) {
          copy_v4_v4(ss->vcol[vertex.i].color, color);
        }
      }

      break;
    case PBVH_BMESH: {
      BMVert *v = (BMVert *)vertex.i;

      if (ss->vcol_type == -1) {
        return;
      }

      if (ss->vcol_domain == ATTR_DOMAIN_POINT) {
        switch (ss->vcol_type) {
          case CD_PROP_COLOR:
            copy_v4_v4((float *)BM_ELEM_CD_GET_VOID_P(v, ss->cd_vcol_offset), color);
            break;

          case CD_MLOOPCOL: {
            MLoopCol *mp = (MLoopCol *)BM_ELEM_CD_GET_VOID_P(v, ss->cd_vcol_offset);

            float temp[4];
            linearrgb_to_srgb_v3_v3(temp, color);
            temp[3] = color[3];

            rgba_float_to_uchar((char *)mp, temp);

            break;
          }
        }
      }
      else {
        BMEdge *e = v->e;

        if (!e) {
          return;
        }

        do {
          if (!e->l) {
            continue;
          }

          BMLoop *l = e->l;
          do {
            BMLoop *l2 = l->v != v ? l->next : l;

            switch (ss->vcol_type) {
              case CD_PROP_COLOR:
                copy_v4_v4((float *)BM_ELEM_CD_GET_VOID_P(l2, ss->cd_vcol_offset), color);
                break;

              case CD_MLOOPCOL: {
                MLoopCol *mp = (MLoopCol *)BM_ELEM_CD_GET_VOID_P(l2, ss->cd_vcol_offset);
                float temp[4];

                linearrgb_to_srgb_v3_v3(temp, color);
                temp[3] = color[3];
                rgba_float_to_uchar((char *)mp, temp);

                break;
              }
            }
          } while ((l = l->radial_next) != e->l);
        } while ((e = BM_DISK_EDGE_NEXT(e, v)) != v->e);
      }

      break;
    }
    case PBVH_GRIDS:
      break;
  }
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

void SCULPT_update_customdata_refs(SculptSession *ss, Object *ob)
{
  BKE_sculptsession_update_attr_refs(ob);
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
                                    Object *ob,
                                    AttributeDomain domain,
                                    int proptype,
                                    const char *name,
                                    SculptLayerParams *params)
{
  SculptCustomLayer scl;

  // call SCULPT_update_customdata_refs before and after,
  // thoeretically it can allocate new layers
  SCULPT_update_customdata_refs(ss, ob);

  bool ret = SCULPT_temp_customlayer_get(ss, ob, domain, proptype, name, &scl, params);

  SCULPT_update_customdata_refs(ss, ob);

  return ret;
}

/* TODO: thoroughly test this function */
bool SCULPT_temp_customlayer_has(SculptSession *ss,
                                 AttributeDomain domain,
                                 int proptype,
                                 const char *name)
{
  CustomData *vdata = NULL, *pdata = NULL, *data = NULL;

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH:
      vdata = &ss->bm->vdata;
      pdata = &ss->bm->pdata;
      break;
    case PBVH_FACES:
      pdata = ss->pdata;
      vdata = ss->vdata;
      break;
    case PBVH_GRIDS:
      pdata = ss->pdata;
      break;
  }

  switch (domain) {
    case ATTR_DOMAIN_POINT:
      data = vdata;
      break;
    case ATTR_DOMAIN_FACE:
      data = pdata;
      break;
    default:
      return false;
  }

  if (data) {
    return CustomData_get_named_layer_index(data, proptype, name) >= 0;
  }

  return false;
}

bool SCULPT_temp_customlayer_release(SculptSession *ss, Object *ob, SculptCustomLayer *scl)
{
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
      SCULPT_update_customdata_refs(ss, ob);
    }
    else {
      MEM_SAFE_FREE(scl->data);
    }

    scl->data = NULL;
  }
  return true;
}

bool SCULPT_temp_customlayer_get(SculptSession *ss,
                                 Object *ob,
                                 AttributeDomain domain,
                                 int proptype,
                                 const char *name,
                                 SculptCustomLayer *scl,
                                 SculptLayerParams *params)
{
  return BKE_sculptsession_customlayer_get(ob, domain, proptype, name, scl, params);
}

SculptVertRef SCULPT_active_vertex_get(SculptSession *ss)
{
  if (ELEM(BKE_pbvh_type(ss->pbvh), PBVH_FACES, PBVH_BMESH, PBVH_GRIDS)) {
    return ss->active_vertex_index;
  }
  return BKE_pbvh_make_vref(SCULPT_REF_NONE);
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
  if (ss->active_face_index.i == SCULPT_REF_NONE) {
    return SCULPT_FACE_SET_NONE;
  }

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
        int node = BM_ELEM_CD_GET_INT(f, ss->cd_face_node_offset);

        if (abs(fset) != face_set) {
          continue;
        }

        if (visible) {
          fset = abs(fset);
        }
        else {
          fset = -abs(fset);
        }

        if (node != DYNTOPO_NODE_NONE) {
          BKE_pbvh_node_mark_update_triangulation(BKE_pbvh_node_from_index(ss->pbvh, node));
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

        /* This can run on geometry without a face set assigned, so its ID sign can't be changed
         * to modify the visibility. Force that geometry to the ID 1 to enable changing the
         * visibility here. */
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
        int node = BM_ELEM_CD_GET_INT(f, ss->cd_face_node_offset);

        if (node != DYNTOPO_NODE_NONE) {
          BKE_pbvh_node_mark_update_triangulation(BKE_pbvh_node_from_index(ss->pbvh, node));
        }

        /* This can run on geometry without a face set assigned, so its ID sign can't be changed
         * to modify the visibility. Force that geometry to the ID 1 to enable changing the
         * visibility here. */

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
      MSculptVert *mv = ss->mdyntopo_verts + index.i;

      MV_ADD_FLAG(mv, SCULPTVERT_NEED_BOUNDARY);

      for (int j = 0; j < ss->pmap[index.i].count; j++) {
        MPoly *mp = ss->mpoly + vert_map->indices[j];
        MLoop *ml = ss->mloop + mp->loopstart;

        for (int k = 0; k < mp->totloop; k++, ml++) {
          MSculptVert *mv2 = ss->mdyntopo_verts + ml->v;
          MV_ADD_FLAG(mv2, SCULPTVERT_NEED_BOUNDARY);
        }

        if (ss->face_sets[vert_map->indices[j]] > 0) {
          ss->face_sets[vert_map->indices[j]] = abs(face_set);
        }
      }
    } break;
    case PBVH_BMESH: {
      BMIter iter;
      BMLoop *l;
      BMVert *v = (BMVert *)index.i;

      MSculptVert *mv = BKE_PBVH_SCULPTVERT(ss->cd_sculpt_vert, v);
      MV_ADD_FLAG(mv, SCULPTVERT_NEED_BOUNDARY);

      BM_ITER_ELEM (l, &iter, v, BM_LOOPS_OF_VERT) {
        int fset = BM_ELEM_CD_GET_INT(l->f, ss->cd_faceset_offset);
        if (fset >= 0 && fset != abs(face_set)) {
          BM_ELEM_CD_SET_INT(l->f, ss->cd_faceset_offset, abs(face_set));
        }

        MSculptVert *mv_l = BKE_PBVH_SCULPTVERT(ss->cd_sculpt_vert, l->v);
        MV_ADD_FLAG(mv_l, SCULPTVERT_NEED_BOUNDARY);
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

  MSculptVert *mv = BM_ELEM_CD_GET_VOID_P(v, ss->cd_sculpt_vert);

  if (ok) {
    mv->flag &= ~SCULPTVERT_VERT_FSET_HIDDEN;
  }
  else {
    mv->flag |= SCULPTVERT_VERT_FSET_HIDDEN;
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
        MSculptVert *mv = BM_ELEM_CD_GET_VOID_P(v, ss->cd_sculpt_vert);

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
          mv->flag |= SCULPTVERT_VERT_FSET_HIDDEN;
          BM_elem_flag_enable(v, BM_ELEM_HIDDEN);
        }
        else {
          mv->flag &= ~SCULPTVERT_VERT_FSET_HIDDEN;
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

bool SCULPT_vertex_has_unique_face_set(const SculptSession *ss, SculptVertRef vertex)
{
  return !SCULPT_vertex_is_boundary(ss, vertex, SCULPT_BOUNDARY_FACE_SET);
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
      iter->neighbors = MEM_mallocN(iter->capacity * sizeof(*iter->neighbors), "neighbor array");
      iter->neighbor_indices = MEM_mallocN(iter->capacity * sizeof(*iter->neighbor_indices),
                                           "neighbor array");

      memcpy(iter->neighbors, iter->neighbors_fixed, sizeof(*iter->neighbors) * iter->size);
      memcpy(iter->neighbor_indices,
             iter->neighbor_indices_fixed,
             sizeof(*iter->neighbor_indices) * iter->size);
    }
    else {
      iter->neighbors = MEM_reallocN_id(
          iter->neighbors, iter->capacity * sizeof(*iter->neighbors), "neighbor array");
      iter->neighbor_indices = MEM_reallocN_id(iter->neighbor_indices,
                                               iter->capacity * sizeof(*iter->neighbor_indices),
                                               "neighbor array");
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
      iter->neighbors = MEM_mallocN(iter->capacity * sizeof(*iter->neighbors), "neighbor array");
      iter->neighbor_indices = MEM_mallocN(iter->capacity * sizeof(*iter->neighbor_indices),
                                           "neighbor array");

      memcpy(iter->neighbors, iter->neighbors_fixed, sizeof(*iter->neighbors) * iter->size);

      memcpy(iter->neighbor_indices,
             iter->neighbor_indices_fixed,
             sizeof(*iter->neighbor_indices) * iter->size);
    }
    else {
      iter->neighbors = MEM_reallocN_id(
          iter->neighbors, iter->capacity * sizeof(*iter->neighbors), "neighbor array");
      iter->neighbor_indices = MEM_reallocN_id(iter->neighbor_indices,
                                               iter->capacity * sizeof(*iter->neighbor_indices),
                                               "neighbor array");
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
  iter->no_free = false;

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

    MSculptVert *mv = BKE_PBVH_SCULPTVERT(ss->cd_sculpt_vert, v2);

    if (!(mv->flag & SCULPTVERT_VERT_FSET_HIDDEN)) {  // && (e->head.hflag & BM_ELEM_DRAW)) {
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
  iter->no_free = false;

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
  iter->no_free = false;

  for (int i = 0; i < vert_map->count; i++) {
    const MEdge *me = &ss->medge[vert_map->indices[i]];

    unsigned int v = me->v1 == (unsigned int)vertex.i ? me->v2 : me->v1;
    MSculptVert *mv = ss->mdyntopo_verts + v;

    if (mv->flag & SCULPTVERT_VERT_FSET_HIDDEN) {
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
  iter->no_free = false;

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

#define SCULPT_NEIGHBORS_CACHE

#ifdef SCULPT_NEIGHBORS_CACHE
typedef struct NeighborCacheItem {
  struct _SculptNeighborRef *neighbors;
  int *neighbors_indices;
  short num_duplicates, valence;
  bool has_edge;
} NeighborCacheItem;

typedef struct NeighborCache {
  MemArena **arenas;
  NeighborCacheItem **cache;
  NeighborCacheItem **duplicates;
  int totvert, totthread;
} NeighborCache;

static void neighbor_cache_free(NeighborCache *ncache)
{
  for (int i = 0; i < ncache->totthread; i++) {
    BLI_memarena_free(ncache->arenas[i]);
  }

  MEM_SAFE_FREE(ncache->arenas);
  MEM_SAFE_FREE(ncache->cache);
  MEM_SAFE_FREE(ncache->duplicates);
  MEM_SAFE_FREE(ncache);
}

static bool neighbor_cache_begin(const SculptSession *ss)
{
  if (!ss->cache) {
    return false;
  }

  // return false;
  Brush *brush = ss->cache->brush;

  if (brush && SCULPT_stroke_is_dynamic_topology(ss, brush)) {
    return false;
  }

  if (ss->cache->ncache) {
    return true;
  }

  int totvert = SCULPT_vertex_count_get(ss);

  NeighborCache *ncache = MEM_callocN(sizeof(NeighborCache), "NeighborCache");
  ncache->cache = MEM_calloc_arrayN(totvert, sizeof(void *), "Cache Items");

  if (BKE_pbvh_type(ss->pbvh) == PBVH_GRIDS) {
    ncache->duplicates = MEM_calloc_arrayN(totvert, sizeof(void *), "Cache Items");
  }

  int totthread = BLI_task_scheduler_num_threads() * 4;
  ncache->arenas = MEM_malloc_arrayN(totthread, sizeof(void *), "neighbor cache->arenas");
  ncache->totthread = totthread;

  for (int i = 0; i < totthread; i++) {
    ncache->arenas[i] = BLI_memarena_new(1 << 17, "neighbor cache");
  }

  ncache->totvert = totvert;

  atomic_cas_ptr((void **)&ss->cache->ncache, NULL, ncache);

  if (ss->cache->ncache != ncache) {
    // another thread got here first?

    neighbor_cache_free(ncache);

    return false;
  }

  return true;
}

static NeighborCacheItem *neighbor_cache_get(const SculptSession *ss,
                                             SculptVertRef vertex,
                                             const bool include_duplicates)
{
  int i = BKE_pbvh_vertex_index_to_table(ss->pbvh, vertex);

  NeighborCache *ncache = ss->cache->ncache;

  if (include_duplicates && !ncache->duplicates) {
    ncache->duplicates = MEM_calloc_arrayN(ncache->totvert, sizeof(void *), "ncache->duplicages");
  }

  NeighborCacheItem **cache = include_duplicates ? ncache->duplicates : ncache->cache;
  int thread_nr = BLI_task_parallel_thread_id(NULL);

  if (!cache[i]) {
    NeighborCacheItem *item = BLI_memarena_calloc(ncache->arenas[thread_nr], sizeof(*item));
    SculptVertexNeighborIter ni = {0};

    ni.neighbors = ni.neighbors_fixed;
    ni.neighbor_indices = ni.neighbor_indices_fixed;
    ni.capacity = SCULPT_VERTEX_NEIGHBOR_FIXED_CAPACITY;

    switch (BKE_pbvh_type(ss->pbvh)) {
      case PBVH_FACES:
        // use vemap if it exists, so result is in disk cycle order
        if (ss->vemap) {
          sculpt_vertex_neighbors_get_faces_vemap(ss, vertex, &ni);
        }
        else {
          sculpt_vertex_neighbors_get_faces(ss, vertex, &ni);
        }
        break;
      case PBVH_BMESH:
        sculpt_vertex_neighbors_get_bmesh(ss, vertex, &ni);
        break;
      case PBVH_GRIDS:
        sculpt_vertex_neighbors_get_grids(ss, vertex, include_duplicates, &ni);
        break;
    }

    item->num_duplicates = ni.num_duplicates;
    item->has_edge = ni.has_edge;
    item->valence = ni.size;

    item->neighbors = BLI_memarena_alloc(ncache->arenas[thread_nr],
                                         sizeof(*item->neighbors) * ni.size);
    item->neighbors_indices = BLI_memarena_alloc(ncache->arenas[thread_nr],
                                                 sizeof(*item->neighbors_indices) * ni.size);

    memcpy(item->neighbors, ni.neighbors, sizeof(*item->neighbors) * ni.size);
    memcpy(
        item->neighbors_indices, ni.neighbor_indices, sizeof(*item->neighbors_indices) * ni.size);

    // if (atomic_cas_ptr(&cache[i], NULL, item) != item) {
    // another thread got here first
    //}

    atomic_cas_ptr((void **)&cache[i], NULL, item);
  }

  return cache[i];
}
#endif

void SCULPT_vertex_neighbors_get(const SculptSession *ss,
                                 const SculptVertRef vertex,
                                 const bool include_duplicates,
                                 SculptVertexNeighborIter *iter)
{
#ifdef SCULPT_NEIGHBORS_CACHE
  if (neighbor_cache_begin(ss)) {
    NeighborCacheItem *item = neighbor_cache_get(ss, vertex, include_duplicates);

    // memset(iter, 0, sizeof(*iter));

    iter->no_free = true;
    iter->neighbors = item->neighbors;
    iter->neighbor_indices = item->neighbors_indices;
    iter->num_duplicates = item->num_duplicates;
    iter->size = iter->capacity = item->valence;
    iter->has_edge = item->has_edge;
    iter->is_duplicate = false;

    return;
  }
#endif

  iter->no_free = false;

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
          MSculptVert *mv1 = BKE_PBVH_SCULPTVERT(ss->cd_sculpt_vert, e->v1);
          MSculptVert *mv2 = BKE_PBVH_SCULPTVERT(ss->cd_sculpt_vert, e->v2);

          return (mv1->flag & SCULPTVERT_FSET_BOUNDARY) && (mv2->flag & SCULPTVERT_FSET_BOUNDARY);
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

static void grids_update_boundary_flags(const SculptSession *ss, SculptVertRef vertex)
{
  MSculptVert *mv = ss->mdyntopo_verts + vertex.i;

  mv->flag &= ~(SCULPTVERT_CORNER | SCULPTVERT_BOUNDARY | SCULPTVERT_NEED_BOUNDARY |
                SCULPTVERT_FSET_BOUNDARY | SCULPTVERT_FSET_CORNER);

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
      if (sculpt_check_unique_face_set_in_base_mesh(ss, BKE_pbvh_make_vref(v1))) {
        mv->flag |= SCULPTVERT_FSET_BOUNDARY;
      }
      if (sculpt_check_boundary_vertex_in_base_mesh(ss, BKE_pbvh_make_vref(v1))) {
        mv->flag |= SCULPTVERT_BOUNDARY;
      }
      break;
    case SUBDIV_CCG_ADJACENT_EDGE:
      if (sculpt_check_unique_face_set_for_edge_in_base_mesh(ss, v1, v2)) {
        mv->flag |= SCULPTVERT_FSET_BOUNDARY;
      }

      if (sculpt_check_boundary_vertex_in_base_mesh(ss, BKE_pbvh_make_vref(v1)) &&
          sculpt_check_boundary_vertex_in_base_mesh(ss, BKE_pbvh_make_vref(v2))) {
        mv->flag |= SCULPTVERT_BOUNDARY;
      }
      break;
    case SUBDIV_CCG_ADJACENT_NONE:
      break;
  }
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
  MSculptVert *mv = ss->mdyntopo_verts + vertex.i;

  mv->flag &= ~(SCULPTVERT_CORNER | SCULPTVERT_BOUNDARY);

  if (sculpt_check_boundary_vertex_in_base_mesh(ss, vertex)) {
    mv->flag |= SCULPTVERT_BOUNDARY;

    if (ss->pmap[vertex.i].count < 4) {
      bool ok = true;

      for (int i = 0; i < ss->pmap[vertex.i].count; i++) {
        MPoly *mp = ss->mpoly + ss->pmap[vertex.i].indices[i];
        if (mp->totloop < 4) {
          ok = false;
        }
      }
      if (ok) {
        mv->flag |= SCULPTVERT_CORNER;
      }
      else {
        mv->flag &= ~SCULPTVERT_CORNER;
      }
    }
  }
}
SculptCornerType SCULPT_vertex_is_corner(const SculptSession *ss,
                                         const SculptVertRef vertex,
                                         SculptCornerType cornertype)
{
  SculptCornerType ret = 0;
  MSculptVert *mv = NULL;

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      BMVert *v = (BMVert *)vertex.i;
      mv = BKE_PBVH_SCULPTVERT(ss->cd_sculpt_vert, v);

      if (mv->flag & SCULPTVERT_NEED_BOUNDARY) {
        BKE_pbvh_update_vert_boundary(ss->cd_sculpt_vert,
                                      ss->cd_faceset_offset,
                                      ss->cd_vert_node_offset,
                                      ss->cd_face_node_offset,
                                      -1,
                                      (BMVert *)vertex.i,
                                      ss->boundary_symmetry);
      }

      break;
    }
    case PBVH_FACES:
      mv = ss->mdyntopo_verts + vertex.i;

      if (mv->flag & SCULPTVERT_NEED_BOUNDARY) {
        faces_update_boundary_flags(ss, vertex);
      }
      break;
    case PBVH_GRIDS: {
      mv = ss->mdyntopo_verts + vertex.i;

      if (mv->flag & SCULPTVERT_NEED_BOUNDARY) {
        grids_update_boundary_flags(ss, vertex);
      }
      break;
    }
  }

  ret = 0;

  if (cornertype & SCULPT_CORNER_MESH) {
    ret |= (mv->flag & SCULPTVERT_CORNER) ? SCULPT_CORNER_MESH : 0;
  }
  if (cornertype & SCULPT_CORNER_FACE_SET) {
    ret |= (mv->flag & SCULPTVERT_FSET_CORNER) ? SCULPT_CORNER_FACE_SET : 0;
  }
  if (cornertype & SCULPT_CORNER_SEAM) {
    ret |= (mv->flag & SCULPTVERT_SEAM_CORNER) ? SCULPT_CORNER_SEAM : 0;
  }
  if (cornertype & SCULPT_CORNER_SHARP) {
    ret |= (mv->flag & SCULPTVERT_SHARP_CORNER) ? SCULPT_CORNER_SHARP : 0;
  }

  return ret;
}

SculptBoundaryType SCULPT_vertex_is_boundary(const SculptSession *ss,
                                             const SculptVertRef vertex,
                                             SculptBoundaryType boundary_types)
{
  MSculptVert *mv = NULL;

  switch (BKE_pbvh_type(ss->pbvh)) {
    case PBVH_BMESH: {
      mv = BKE_PBVH_SCULPTVERT(ss->cd_sculpt_vert, ((BMVert *)(vertex.i)));

      if (mv->flag & SCULPTVERT_NEED_BOUNDARY) {
        BKE_pbvh_update_vert_boundary(ss->cd_sculpt_vert,
                                      ss->cd_faceset_offset,
                                      ss->cd_vert_node_offset,
                                      ss->cd_face_node_offset,
                                      -1,
                                      (BMVert *)vertex.i,
                                      ss->boundary_symmetry);
      }

      break;
    }
    case PBVH_FACES: {
      mv = ss->mdyntopo_verts + vertex.i;

      if (mv->flag & SCULPTVERT_NEED_BOUNDARY) {
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
    flag |= (mv->flag & SCULPTVERT_BOUNDARY) ? SCULPT_BOUNDARY_MESH : 0;
  }
  if (boundary_types & SCULPT_BOUNDARY_FACE_SET) {
    flag |= (mv->flag & SCULPTVERT_FSET_BOUNDARY) ? SCULPT_BOUNDARY_FACE_SET : 0;
  }
  if (boundary_types & SCULPT_BOUNDARY_SHARP) {
    flag |= (mv->flag & SCULPTVERT_SHARP_BOUNDARY) ? SCULPT_BOUNDARY_SHARP : 0;
  }
  if (boundary_types & SCULPT_BOUNDARY_SEAM) {
    flag |= (mv->flag & SCULPTVERT_SEAM_BOUNDARY) ? SCULPT_BOUNDARY_SEAM : 0;
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
  return SCULPT_TOOL_HAS_TOPOLOGY_RAKE(SCULPT_get_tool(ss, brush)) &&
         (brush->topology_rake_factor > 0.0f) && (ss->bm != NULL);
}

/**
 * Test whether the #StrokeCache.sculpt_normal needs update in #do_brush_action
 */
static int sculpt_brush_needs_normal(const SculptSession *ss, const Brush *brush)
{
  return ((SCULPT_TOOL_HAS_NORMAL_WEIGHT(SCULPT_get_tool(ss, brush)) &&
           (ss->cache->normal_weight > 0.0f)) ||

          ELEM(SCULPT_get_tool(ss, brush),
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

static bool sculpt_brush_needs_rake_rotation(const SculptSession *ss, const Brush *brush)
{
  return SCULPT_TOOL_HAS_RAKE(SCULPT_get_tool(ss, brush)) &&
         (SCULPT_get_float(ss, rake_factor, NULL, brush) != 0.0f);
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
  data->datatype = unode ? unode->type : SCULPT_UNDO_COORDS;

  data->pbvh = ss->pbvh;
  data->ss = ss;

  if (bm) {
    data->bm_log = ss->bm_log;
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

bool SCULPT_vertex_check_origdata(SculptSession *ss, SculptVertRef vertex)
{
  // check if we need to update original data for current stroke
  MSculptVert *mv = ss->bm ? BKE_PBVH_SCULPTVERT(ss->cd_sculpt_vert, (BMVert *)vertex.i) :
                             ss->mdyntopo_verts + vertex.i;

  if (mv->stroke_id != ss->stroke_id) {
    mv->stroke_id = ss->stroke_id;

    copy_v3_v3(mv->origco, SCULPT_vertex_co_get(ss, vertex));
    SCULPT_vertex_normal_get(ss, vertex, mv->origno);

    SCULPT_vertex_color_get(ss, vertex, mv->origcolor);

    mv->origmask = unit_float_to_ushort_clamp(SCULPT_vertex_mask_get(ss, vertex));

    return false;
  }

  return true;
}

/**
 * DEPRECATED use Update a #SculptOrigVertData for a particular vertex from the PBVH iterator.
 */
void SCULPT_orig_vert_data_update(SculptOrigVertData *orig_data, SculptVertRef vertex)
{
  // check if we need to update original data for current stroke
  MSculptVert *mv = SCULPT_vertex_get_mdyntopo(orig_data->ss, vertex);

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
    orig_data->mask = (float)mv->origmask / 65535.0f;
  }
}

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
      SCULPT_TOOL_HAS_DYNTOPO(SCULPT_get_tool(ss, brush)));
}

/*** paint mesh ***/

static void paint_mesh_restore_co_task_cb(void *__restrict userdata,
                                          const int n,
                                          const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;

  SculptUndoType type = 0;

  switch (SCULPT_get_tool(ss, data->brush)) {
    case SCULPT_TOOL_MASK:
      type |= SCULPT_UNDO_MASK;
      break;
    case SCULPT_TOOL_PAINT:
    case SCULPT_TOOL_SMEAR:
      type |= SCULPT_UNDO_COLOR;
      break;
    case SCULPT_TOOL_VCOL_BOUNDARY:
      type |= SCULPT_UNDO_COLOR | SCULPT_UNDO_COORDS;
      break;
    default:
      type |= SCULPT_UNDO_COORDS;
      break;
  }

  PBVHVertexIter vd;

  bool modified = false;

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    SCULPT_vertex_check_origdata(ss, vd.vertex);
    MSculptVert *mv = SCULPT_vertex_get_mdyntopo(ss, vd.vertex);

    if (type & SCULPT_UNDO_COORDS) {
      if (len_squared_v3v3(vd.co, mv->origco) > FLT_EPSILON) {
        modified = true;
      }

      copy_v3_v3(vd.co, mv->origco);

      if (vd.no) {
        normal_float_to_short_v3(vd.no, mv->origno);
      }
      else {
        copy_v3_v3(vd.fno, mv->origno);
      }
    }

    if (type & SCULPT_UNDO_MASK) {
      if ((*vd.mask - mv->origmask) * (*vd.mask - mv->origmask) > FLT_EPSILON) {
        modified = true;
      }

      *vd.mask = mv->origmask;
    }

    if (type & SCULPT_UNDO_COLOR) {
      float color[4];

      if (SCULPT_vertex_color_get(ss, vd.vertex, color)) {
        if (len_squared_v4v4(color, mv->origcolor) > FLT_EPSILON) {
          modified = true;
        }

        SCULPT_vertex_color_set(ss, vd.vertex, mv->origcolor);
      }
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
void flip_v3(float v[3], const ePaintSymmetryFlags symm)
{
  flip_v3_v3(v, v, symm);
}

void flip_qt(float quat[4], const ePaintSymmetryFlags symm)
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

  if (cache->radius > 0.0f && distsq <= 4.0f * (cache->radius_squared)) {
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

  /* mathwise divice by zero is infinity, so use maximum value (1) in that case? */
  return overlap != 0.0f ? 1.0f / overlap : 1.0f;
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
    if (ELEM(SCULPT_get_tool(ss, data->brush), SCULPT_TOOL_SCRAPE, SCULPT_TOOL_FILL) &&
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
          MSculptVert *mv = BKE_PBVH_SCULPTVERT(vd.cd_sculpt_vert, v);

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

void SCULPT_calc_area_center(
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
void SCULPT_calc_area_normal_and_center(
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

  int tool = cache->tool_override ? cache->tool_override : brush->sculpt_tool;

  switch (tool) {
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
    case SCULPT_TOOL_RELAX:
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
bool SCULPT_search_sphere_cb(PBVHNode *node, void *data_v)
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
        .ignore_fully_ineffective = SCULPT_get_tool(ss, brush) != SCULPT_TOOL_MASK,
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
        .ignore_fully_ineffective = SCULPT_get_tool(ss, brush) != SCULPT_TOOL_MASK,
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
  int tool = SCULPT_get_tool(ob->sculpt, brush);

  /* Grab brush does not update the sculpt normal during a stroke. */
  const bool update_normal = !(brush->flag & BRUSH_ORIGINAL_NORMAL) &&
                             !(tool == SCULPT_TOOL_GRAB) &&
                             !(tool == SCULPT_TOOL_THUMB && !(brush->flag & BRUSH_ANCHORED)) &&
                             !(tool == SCULPT_TOOL_ELASTIC_DEFORM) &&
                             !(tool == SCULPT_TOOL_SNAKE_HOOK && cache->normal_weight > 0.0f);

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
        SCULPT_calc_area_normal_and_center(sd, ob, nodes, totnode, r_area_no, r_area_co);
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
      SCULPT_calc_area_center(sd, ob, nodes, totnode, r_area_co);
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

int SCULPT_plane_trim(const StrokeCache *cache, const Brush *brush, const float val[3])
{
  return (!(cache->use_plane_trim) ||
          ((dot_v3v3(val, val) <= cache->radius_squared * cache->plane_trim_squared)));
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
      SCULPT_get_tool(sdata->ob->sculpt, sdata->brush) == SCULPT_TOOL_MASK ? SCULPT_UNDO_MASK :
                                                                             SCULPT_UNDO_COORDS,
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
                                   UnifiedPaintSettings *UNUSED(ups),
                                   void *UNUSED(userdata))
{
  SculptSession *ss = ob->sculpt;

  /* build brush radius scale */
  float radius_scale = 1.0f;

  /* Build a list of all nodes that are potentially within the brush's area of influence. */
  const bool use_original = sculpt_tool_needs_original(SCULPT_get_tool(ss, brush)) ?
                                true :
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
      .ignore_fully_ineffective = SCULPT_get_tool(ss, brush) != SCULPT_TOOL_MASK,
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
      DYNTOPO_HAS_DYNAMIC_SPLIT(SCULPT_get_tool(ss, brush)),
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

static void do_check_origco_cb(void *__restrict userdata,
                               const int n,
                               const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;
  PBVHVertexIter vd;

  bool modified = false;

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    modified |= SCULPT_vertex_check_origdata(ss, vd.vertex);
  }
  BKE_pbvh_vertex_iter_end;

  if (modified) {
    BKE_pbvh_node_mark_original_update(data->nodes[n]);
  }
}

static void do_brush_action_task_cb(void *__restrict userdata,
                                    const int n,
                                    const TaskParallelTLS *__restrict UNUSED(tls))
{
  SculptThreadedTaskData *data = userdata;
  SculptSession *ss = data->ob->sculpt;

  /* Face Sets modifications do a single undo push */
  if (ELEM(SCULPT_get_tool(ss, data->brush), SCULPT_TOOL_DRAW_FACE_SETS, SCULPT_TOOL_AUTO_FSET)) {
    BKE_pbvh_node_mark_redraw(data->nodes[n]);
    /* Draw face sets in smooth mode moves the vertices. */
    if (ss->cache->alt_smooth) {
      SCULPT_undo_push_node(data->ob, data->nodes[n], SCULPT_UNDO_COORDS);
      BKE_pbvh_node_mark_update(data->nodes[n]);
    }
  }
  else if (SCULPT_get_tool(ss, data->brush) == SCULPT_TOOL_ARRAY) {
    /* Do nothing, array brush does a single geometry undo push. */
  }
  else if (SCULPT_get_tool(ss, data->brush) == SCULPT_TOOL_MASK) {
    SCULPT_undo_push_node(data->ob, data->nodes[n], SCULPT_UNDO_MASK);
    BKE_pbvh_node_mark_update_mask(data->nodes[n]);
  }
  else if (ELEM(SCULPT_get_tool(ss, data->brush), SCULPT_TOOL_PAINT, SCULPT_TOOL_SMEAR)) {
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

static bool brush_uses_commandlist(Brush *brush, int tool)
{
  bool ok = false;

  switch (tool) {
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
    case SCULPT_TOOL_PINCH:
    case SCULPT_TOOL_SIMPLIFY:
    case SCULPT_TOOL_SNAKE_HOOK:
    case SCULPT_TOOL_SLIDE_RELAX:
    case SCULPT_TOOL_RELAX:
    case SCULPT_TOOL_INFLATE:
    case SCULPT_TOOL_PAINT:
    case SCULPT_TOOL_SMEAR:
      ok = true;
      break;
  }

  ok = ok && !(brush->flag & (BRUSH_ANCHORED | BRUSH_DRAG_DOT));

  return ok;
}

void do_brush_action(
    Sculpt *sd, Object *ob, Brush *brush, UnifiedPaintSettings *ups, void *UNUSED(userdata))
{
  SculptSession *ss = ob->sculpt;

  int totnode;
  PBVHNode **nodes;

  float radius_scale = 1.0f;

  if (!ELEM(SCULPT_get_tool(ss, brush), SCULPT_TOOL_SMOOTH, SCULPT_TOOL_MASK) &&
      brush->autosmooth_factor > 0 && brush->autosmooth_radius_factor != 1.0f) {
    radius_scale = MAX2(radius_scale, brush->autosmooth_radius_factor);
  }

  if (sculpt_brush_use_topology_rake(ss, brush)) {
    radius_scale = MAX2(radius_scale, brush->topology_rake_radius_factor);
  }

  /* Check for unsupported features. */
  PBVHType type = BKE_pbvh_type(ss->pbvh);
  if (ELEM(SCULPT_get_tool(ss, brush), SCULPT_TOOL_PAINT, SCULPT_TOOL_SMEAR) &&
      !ELEM(type, PBVH_BMESH, PBVH_FACES)) {
    return;
  }

  if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_ARRAY && !ELEM(type, PBVH_FACES, PBVH_BMESH)) {
    return;
  }

  /* Build a list of all nodes that are potentially within the brush's area of influence */
  const bool use_original = sculpt_tool_needs_original(SCULPT_get_tool(ss, brush)) ?
                                true :
                                ss->cache->original;

  if (SCULPT_tool_needs_all_pbvh_nodes(brush)) {
    /* These brushes need to update all nodes as they are not constrained by the brush radius */
    BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);
  }
  else if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_CLOTH) {
    nodes = SCULPT_cloth_brush_affected_nodes_gather(ss, brush, &totnode);
  }
  else {
    /* With these options enabled not all required nodes are inside the original brush radius, so
     * the brush can produce artifacts in some situations. */
    if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_DRAW && brush->flag & BRUSH_ORIGINAL_NORMAL) {
      radius_scale = MAX2(radius_scale, 2.0f);
    }
    nodes = sculpt_pbvh_gather_generic(ob, sd, brush, use_original, radius_scale * 1.2, &totnode);
  }

  /* Draw Face Sets in draw mode makes a single undo push, in alt-smooth mode deforms the
   * vertices and uses regular coords undo. */
  /* It also assigns the paint_face_set here as it needs to be done regardless of the stroke type
   * and the number of nodes under the brush influence. */
  if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_DRAW_FACE_SETS &&
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
        SCULPT_get_tool(ss, brush) == SCULPT_TOOL_DRAW_FACE_SETS) {

      SCULPT_face_ensure_original(ss, ob);

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
    if (ELEM(SCULPT_get_tool(ss, brush), SCULPT_TOOL_PAINT, SCULPT_TOOL_SMEAR)) {
      for (int i = 0; i < totnode; i++) {
        int other = brush->vcol_boundary_factor > 0.0f ? SCULPT_UNDO_COORDS : -1;

        SCULPT_ensure_dyntopo_node_undo(ob, nodes[i], SCULPT_UNDO_COLOR, other);
        BKE_pbvh_node_mark_update_color(nodes[i]);
      }
    }
    else if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_DRAW_FACE_SETS) {
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
    else if (SCULPT_get_tool(ss, brush) != SCULPT_TOOL_ARRAY) {
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

  if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_POSE &&
      SCULPT_stroke_is_first_brush_step(ss->cache)) {
    SCULPT_pose_brush_init(sd, ob, ss, brush);
  }

  if (brush->deform_target == BRUSH_DEFORM_TARGET_CLOTH_SIM) {
    if (!ss->cache->cloth_sim) {
      ss->cache->cloth_sim = SCULPT_cloth_brush_simulation_create(
          ss, ob, 1.0f, 1.0f, 0.0f, false, true);
      SCULPT_cloth_brush_simulation_init(ss, ss->cache->cloth_sim);
    }
    SCULPT_cloth_brush_store_simulation_state(ss, ss->cache->cloth_sim);
    SCULPT_cloth_brush_ensure_nodes_constraints(
        sd, ob, nodes, totnode, ss->cache->cloth_sim, ss->cache->location, FLT_MAX);
  }

  bool invert = ss->cache->pen_flip || ss->cache->invert || brush->flag & BRUSH_DIR_IN;

  SCULPT_replay_log_append(sd, ss, ob);

  /* Apply one type of brush action. */
  switch (SCULPT_get_tool(ss, brush)) {
    case SCULPT_TOOL_DRAW:
      SCULPT_do_draw_brush(sd, ob, nodes, totnode);
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
      SCULPT_do_crease_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_BLOB:
      SCULPT_do_crease_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_PINCH:
      SCULPT_do_pinch_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_INFLATE:
      SCULPT_do_inflate_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_GRAB:
      SCULPT_do_grab_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_ROTATE:
      SCULPT_do_rotate_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_SNAKE_HOOK:
      SCULPT_do_snake_hook_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_NUDGE:
      SCULPT_do_nudge_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_THUMB:
      SCULPT_do_thumb_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_LAYER:
      SCULPT_do_layer_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_FLATTEN:
      SCULPT_do_flatten_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_CLAY:
      SCULPT_do_clay_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_CLAY_STRIPS:
      SCULPT_do_clay_strips_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_TWIST:
      SCULPT_do_twist_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_MULTIPLANE_SCRAPE:
      SCULPT_do_multiplane_scrape_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_CLAY_THUMB:
      SCULPT_do_clay_thumb_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_FILL:
      if (invert && brush->flag & BRUSH_INVERT_TO_SCRAPE_FILL) {
        SCULPT_do_scrape_brush(sd, ob, nodes, totnode);
      }
      else {
        SCULPT_do_fill_brush(sd, ob, nodes, totnode);
      }
      break;
    case SCULPT_TOOL_SCRAPE:
      if (invert && brush->flag & BRUSH_INVERT_TO_SCRAPE_FILL) {
        SCULPT_do_fill_brush(sd, ob, nodes, totnode);
      }
      else {
        SCULPT_do_scrape_brush(sd, ob, nodes, totnode);
      }
      break;
    case SCULPT_TOOL_MASK:
      SCULPT_do_mask_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_POSE:
      SCULPT_do_pose_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_DRAW_SHARP:
      SCULPT_do_draw_sharp_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_ELASTIC_DEFORM:
      SCULPT_do_elastic_deform_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_SLIDE_RELAX:
      SCULPT_do_slide_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_RELAX:
      SCULPT_do_relax_brush(sd, ob, nodes, totnode);
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
      SCULPT_do_displacement_eraser_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_DISPLACEMENT_SMEAR:
      SCULPT_do_displacement_smear_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_PAINT:
      SCULPT_do_paint_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_SMEAR:
      SCULPT_do_smear_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_FAIRING:
      SCULPT_do_fairing_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_SCENE_PROJECT:
      SCULPT_do_scene_project_brush(sd, ob, nodes, totnode);
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

  bool apply_autosmooth = !ELEM(SCULPT_get_tool(ss, brush),
                                SCULPT_TOOL_BOUNDARY,
                                SCULPT_TOOL_SMOOTH,
                                SCULPT_TOOL_MASK) &&
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

    SCULPT_bmesh_topology_rake(
        sd, ob, nodes, totnode, brush->topology_rake_factor, SCULPT_stroke_needs_original(brush));

    if (brush->topology_rake_radius_factor != 1.0f) {
      ss->cache->radius = start_radius;
      ss->cache->radius_squared = ss->cache->radius * ss->cache->radius;
    }
  }

  /* The cloth brush adds the gravity as a regular force and it is processed in the solver. */
  if (ss->cache->supports_gravity && !ELEM(SCULPT_get_tool(ss, brush),
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

typedef struct BrushRunCommandData {
  BrushCommand *cmd;
  PBVHNode **nodes;
  int totnode;
  float radius_max;
} BrushRunCommandData;

static void get_nodes_undo(Sculpt *sd,
                           Object *ob,
                           Brush *brush,
                           UnifiedPaintSettings *ups,
                           BrushRunCommandData *data,
                           int tool)
{
  PBVHNode **nodes = NULL;
  int totnode = 0;
  BrushCommand *cmd = data->cmd;
  SculptSession *ss = ob->sculpt;
  float start_radius = ss->cache->radius;
  float radius_scale = 1.0f;
  const bool use_original = sculpt_tool_needs_original(cmd->tool) ? true : ss->cache->original;

  if (SCULPT_tool_needs_all_pbvh_nodes(brush)) {
    /* These brushes need to update all nodes as they are not constrained by the brush radius
     */
    BKE_pbvh_search_gather(ss->pbvh, NULL, NULL, &nodes, &totnode);
  }
  else if (tool == SCULPT_TOOL_CLOTH) {
    nodes = SCULPT_cloth_brush_affected_nodes_gather(ss, brush, &totnode);
  }
  else {
    /* With these options enabled not all required nodes are inside the original brush radius,
     * so the brush can produce artifacts in some situations. */
    if (tool == SCULPT_TOOL_DRAW && brush->flag & BRUSH_ORIGINAL_NORMAL) {
      radius_scale = MAX2(radius_scale, 2.0f);
    }
    nodes = sculpt_pbvh_gather_generic(ob, sd, brush, use_original, radius_scale, &totnode);
  }

  /* Draw Face Sets in draw mode makes a single undo push, in alt-smooth mode deforms the
   * vertices and uses regular coords undo. */
  /* It also assigns the paint_face_set here as it needs to be done regardless of the stroke type
   * and the number of nodes under the brush influence. */
  if (tool == SCULPT_TOOL_DRAW_FACE_SETS && SCULPT_stroke_is_first_brush_step(ss->cache) &&
      !ss->cache->alt_smooth) {

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
    if (SCULPT_is_automasking_enabled(sd, ss, brush)) {
      if (SCULPT_stroke_is_first_brush_step(ss->cache)) {
        ss->cache->automasking = SCULPT_automasking_cache_init(sd, brush, ob);
      }
      else {
        SCULPT_automasking_step_update(ss->cache->automasking, ss, sd, brush);
      }
    }
  }

  data->nodes = nodes;
  data->totnode = totnode;

  /* Only act if some verts are inside the brush area. */
  if (totnode == 0) {
    ss->cache->radius = start_radius;
    ss->cache->radius_squared = start_radius * start_radius;

    return;
  }

  // dyntopo can't push undo nodes inside a thread
  if (ss->bm) {
    if (ELEM(tool, SCULPT_TOOL_PAINT, SCULPT_TOOL_SMEAR)) {
      for (int i = 0; i < totnode; i++) {
        int other = brush->vcol_boundary_factor > 0.0f ? SCULPT_UNDO_COORDS : -1;

        SCULPT_ensure_dyntopo_node_undo(ob, nodes[i], SCULPT_UNDO_COLOR, other);
        BKE_pbvh_node_mark_update_color(nodes[i]);
      }
    }
    else if (ELEM(tool, SCULPT_TOOL_DRAW_FACE_SETS, SCULPT_TOOL_AUTO_FSET)) {
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

  if (ss->cache->original) {
    SculptThreadedTaskData task_data = {
        .sd = sd,
        .ob = ob,
        .brush = brush,
        .nodes = nodes,
    };

    TaskParallelSettings settings;
    BKE_pbvh_parallel_range_settings(&settings, true, totnode);
    BLI_task_parallel_range(0, totnode, &task_data, do_check_origco_cb, &settings);

    BKE_pbvh_update_bounds(ss->pbvh, PBVH_UpdateOriginalBB);
  }

  data->nodes = nodes;
  data->totnode = totnode;
}

static void SCULPT_run_command(
    Sculpt *sd, Object *ob, Brush *brush, UnifiedPaintSettings *ups, void *userdata)
{
  SculptSession *ss = ob->sculpt;
  BrushRunCommandData *data = userdata;
  BrushCommand *cmd = data->cmd;

  float radius = BRUSHSET_GET_FLOAT(cmd->params_mapped, radius, NULL);
  radius = paint_calc_object_space_radius(ss->cache->vc, ss->cache->true_location, radius);

  ss->cache->radius = radius;
  ss->cache->radius_squared = radius * radius;
  ss->cache->initial_radius = radius;

  get_nodes_undo(sd, ob, ss->cache->brush, ups, data, cmd->tool);

  PBVHNode **nodes = data->nodes;
  int totnode = data->totnode;

  Brush _brush2, *brush2 = &_brush2;
  float radius_scale;

  /*create final, input mapped parameter list*/

  radius_scale = 1.0f;

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
                    BRUSH_JITTER_PRESSURE | BRUSH_OFFSET_PRESSURE | BRUSH_INVERSE_SMOOTH_PRESSURE);
  brush2->flag2 &= ~BRUSH_AREA_RADIUS_PRESSURE;

  brush2->sculpt_tool = cmd->tool;
  BrushChannelSet *channels_final = ss->cache->channels_final;
  ss->cache->channels_final = cmd->params_mapped;

  ss->cache->brush = brush2;

  ups->alpha = BRUSHSET_GET_FLOAT(cmd->params_final, strength, NULL);

  if (cmd->tool == SCULPT_TOOL_SMOOTH) {
    ss->cache->bstrength = BRUSHSET_GET_FLOAT(cmd->params_mapped, strength, NULL);
  }
  else {
    ss->cache->bstrength = brush_strength(
        sd, ss->cache, calc_symmetry_feather(sd, ss->cache), ups);
  }

  // do not pressure map brush2->alpha now that we've used it to build ss->cache->bstrength
  brush2->alpha = BRUSHSET_GET_FLOAT(cmd->params_final, strength, NULL);

  if (!BRUSHSET_GET_INT(cmd->params_mapped, use_ctrl_invert, NULL)) {
    ss->cache->bstrength = fabsf(ss->cache->bstrength);
  }
  // brush2->alpha = fabs(ss->cache->bstrength);

  // printf("brush2->alpha: %f\n", brush2->alpha);
  // printf("ss->cache->bstrength: %f\n", ss->cache->bstrength);

  /*Search PBVH*/

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
          ss, ob, 1.0f, 1.0f, 0.0f, false, true);
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
      SCULPT_do_draw_brush(sd, ob, nodes, totnode);
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
      SCULPT_do_crease_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_BLOB:
      SCULPT_do_crease_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_PINCH:
      SCULPT_do_pinch_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_INFLATE:
      SCULPT_do_inflate_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_GRAB:
      SCULPT_do_grab_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_ROTATE:
      SCULPT_do_rotate_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_SNAKE_HOOK:
      SCULPT_do_snake_hook_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_NUDGE:
      SCULPT_do_nudge_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_THUMB:
      SCULPT_do_thumb_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_LAYER:
      SCULPT_do_layer_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_FLATTEN:
      SCULPT_do_flatten_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_CLAY:
      SCULPT_do_clay_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_CLAY_STRIPS:
      SCULPT_do_clay_strips_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_TWIST:
      SCULPT_do_twist_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_MULTIPLANE_SCRAPE:
      SCULPT_do_multiplane_scrape_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_CLAY_THUMB:
      SCULPT_do_clay_thumb_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_FILL:
      if (invert && brush2->flag & BRUSH_INVERT_TO_SCRAPE_FILL) {
        SCULPT_do_scrape_brush(sd, ob, nodes, totnode);
      }
      else {
        SCULPT_do_fill_brush(sd, ob, nodes, totnode);
      }
      break;
    case SCULPT_TOOL_SCRAPE:
      if (invert && brush2->flag & BRUSH_INVERT_TO_SCRAPE_FILL) {
        SCULPT_do_fill_brush(sd, ob, nodes, totnode);
      }
      else {
        SCULPT_do_scrape_brush(sd, ob, nodes, totnode);
      }
      break;
    case SCULPT_TOOL_MASK:
      SCULPT_do_mask_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_POSE:
      SCULPT_do_pose_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_DRAW_SHARP:
      SCULPT_do_draw_sharp_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_ELASTIC_DEFORM:
      SCULPT_do_elastic_deform_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_SLIDE_RELAX:
      SCULPT_do_slide_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_RELAX:
      SCULPT_do_relax_brush(sd, ob, nodes, totnode);
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
      SCULPT_do_displacement_eraser_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_DISPLACEMENT_SMEAR:
      SCULPT_do_displacement_smear_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_PAINT:
      SCULPT_do_paint_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_SMEAR:
      SCULPT_do_smear_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_FAIRING:
      SCULPT_do_fairing_brush(sd, ob, nodes, totnode);
      break;
    case SCULPT_TOOL_SCENE_PROJECT:
      SCULPT_do_scene_project_brush(sd, ob, nodes, totnode);
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
        SCULPT_bmesh_topology_rake(
            sd, ob, nodes, totnode, ss->cache->bstrength, SCULPT_stroke_needs_original(brush));
      }
      break;
    case SCULPT_TOOL_DYNTOPO:
      sculpt_topology_update(sd, ob, brush, ups, NULL);
      // do_symmetrical_brush_actions(sd, ob, sculpt_topology_update, ups);
      break;
    case SCULPT_TOOL_AUTO_FSET:
      SCULPT_do_auto_face_set(sd, ob, nodes, totnode);
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
    if (cmd->tool == SCULPT_TOOL_ARRAY) {
      SCULPT_tag_update_overlays(C);
    }
    ss->needs_pbvh_rebuild = false;
  }

  BKE_pbvh_update_bounds(ss->pbvh, PBVH_UpdateBB | PBVH_UpdateOriginalBB);

  ss->cache->channels_final = channels_final;
  ss->cache->brush = brush;

  MEM_SAFE_FREE(nodes);
}

static void SCULPT_run_commandlist(
    Sculpt *sd, Object *ob, Brush *brush, BrushCommandList *list, UnifiedPaintSettings *ups)
{
  SculptSession *ss = ob->sculpt;
  Brush *oldbrush = ss->cache->brush;

  if (ss->cache->alt_smooth && SCULPT_get_tool(ss, brush) == SCULPT_TOOL_SMOOTH) {
    float factor = BRUSHSET_GET_FLOAT(ss->cache->channels_final, smooth_strength_factor, NULL);
    float projection = BRUSHSET_GET_FLOAT(
        ss->cache->channels_final, smooth_strength_projection, NULL);

    BRUSHSET_SET_FLOAT(ss->cache->channels_final, strength, factor);
    BRUSHSET_SET_FLOAT(ss->cache->channels_final, projection, projection);

    BrushChannel *ch = BRUSHSET_LOOKUP(ss->cache->channels_final, smooth_strength_factor);
    BKE_brush_channel_copy_data(
        BRUSHSET_LOOKUP(ss->cache->channels_final, strength), ch, false, true);

    ch = BRUSHSET_LOOKUP(ss->cache->channels_final, smooth_strength_projection);
    BKE_brush_channel_copy_data(
        BRUSHSET_LOOKUP(ss->cache->channels_final, projection), ch, false, true);
  }

  int totnode = 0;
  PBVHNode **nodes = NULL;

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

    if (!SCULPT_TOOL_HAS_DYNTOPO(cmd->tool) || SCULPT_get_int(ss, dyntopo_disabled, sd, brush)) {
      has_dyntopo = false;
    }

    float radius = BRUSHSET_GET_FLOAT(
        ss->cache->channels_final, radius, &ss->cache->input_mapping);
    radius = paint_calc_object_space_radius(ss->cache->vc, ss->cache->true_location, radius);

    radius_max = max_ff(radius_max, radius);
    ss->cache->brush = brush;
  }

  /* Check for unsupported features. */
  PBVHType type = BKE_pbvh_type(ss->pbvh);
  if (ELEM(SCULPT_get_tool(ss, brush), SCULPT_TOOL_PAINT, SCULPT_TOOL_SMEAR) &&
      !ELEM(type, PBVH_BMESH, PBVH_FACES)) {
    return;
  }

  if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_ARRAY && !ELEM(type, PBVH_FACES, PBVH_BMESH)) {
    return;
  }

  for (int step = 0; step < list->totcommand; step++) {
    BrushCommand *cmd = list->commands + step;

    if (cmd->tool == SCULPT_TOOL_DYNTOPO && !has_dyntopo) {
      continue;
    }

    /* clang-format off */
    float spacing = BRUSHSET_GET_FINAL_FLOAT(cmd->params,
                                             ss->cache->channels_final,
                                             spacing,
                                             &ss->cache->input_mapping) / 100.0f;
    /* clang-format on */

    bool noskip = paint_stroke_apply_subspacing(
        ss->cache->stroke,
        spacing,
        PAINT_MODE_SCULPT,
        &cmd->last_spacing_t[SCULPT_get_symmetry_pass(ss)]);

    if (!noskip) {
      continue;
    }

    BrushRunCommandData data = {
        .cmd = cmd,
        .nodes = NULL,
        .totnode = 0,
        .radius_max = radius_max};  //, .nodes = nodes, .totnode = totnode};

    if (cmd->params_mapped) {
      BKE_brush_channelset_free(cmd->params_mapped);
    }

    cmd->params_mapped = BKE_brush_channelset_copy(cmd->params_final);
    BKE_brush_channelset_apply_mapping(cmd->params_mapped, &ss->cache->input_mapping);
    BKE_brush_channelset_clear_inherit(cmd->params_mapped);

    do_symmetrical_brush_actions(sd, ob, SCULPT_run_command, ups, &data);

    sculpt_combine_proxies(sd, ob);
  }

  /*
                           paint_stroke_apply_subspacing(
                               ss->cache->stroke,
                               spacing,
                               PAINT_MODE_SCULPT,
                               &ss->cache->last_smooth_t[SCULPT_get_symmetry_pass(ss)]);

    */

  /* The cloth brush adds the gravity as a regular force and it is processed in the solver. */
  if (ss->cache->supports_gravity && !ELEM(SCULPT_get_tool(ss, brush),
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

  BKE_pbvh_node_get_proxies(data->nodes[n], &proxies, &proxy_count);

  BKE_pbvh_vertex_iter_begin (ss->pbvh, data->nodes[n], vd, PBVH_ITER_UNIQUE) {
    float val[3];

    if (use_orco) {
      if (ss->bm) {
        float *co = BKE_PBVH_SCULPTVERT(ss->cd_sculpt_vert, vd.bm_vert)->origco;
        copy_v3_v3(val, co);
      }
      else {
        float *co = ss->mdyntopo_verts[vd.index].origco;
        copy_v3_v3(val, co);
      }
    }
    else {
      copy_v3_v3(val, vd.co);
    }

    for (int p = 0; p < proxy_count; p++) {
      add_v3_v3(val, proxies[p].co[vd.i]);
    }

    PBVH_CHECK_NAN(val);

    if (ss->filter_cache && ss->filter_cache->cloth_sim) {
      /* When there is a simulation running in the filter cache that was created by a tool,
       * combine the proxies into the simulation instead of directly into the mesh. */
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
  Brush *brush = ss->cache ? ss->cache->brush : BKE_paint_brush(&sd->paint);
  PBVHNode **nodes;
  int totnode;

  if (!ss->cache ||
      !ss->cache->supports_gravity && sculpt_tool_is_proxy_used(brush->sculpt_tool)) {
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

  flip_v3_v3(cache->prev_grab_delta_symmetry, cache->prev_grab_delta, symm);
  flip_v3_v3(cache->next_grab_delta_symmetry, cache->next_grab_delta, symm);

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

static void do_tiled(Sculpt *sd,
                     Object *ob,
                     Brush *brush,
                     UnifiedPaintSettings *ups,
                     BrushActionFunc action,
                     void *userdata)
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
  action(sd, ob, brush, ups, userdata);

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
        action(sd, ob, brush, ups, userdata);
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
                               const float UNUSED(feather),
                               void *userdata)
{
  SculptSession *ss = ob->sculpt;

  for (int i = 1; i < sd->radial_symm[axis - 'X']; i++) {
    const float angle = 2.0f * M_PI * i / sd->radial_symm[axis - 'X'];
    ss->cache->radial_symmetry_pass = i;
    SCULPT_cache_calc_brushdata_symm(ss->cache, symm, axis, angle);
    do_tiled(sd, ob, brush, ups, action, userdata);
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

static void do_symmetrical_brush_actions(
    Sculpt *sd, Object *ob, BrushActionFunc action, UnifiedPaintSettings *ups, void *userdata)
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
    do_tiled(sd, ob, brush, ups, action, userdata);

    do_radial_symmetry(sd, ob, brush, ups, action, i, 'X', feather, userdata);
    do_radial_symmetry(sd, ob, brush, ups, action, i, 'Y', feather, userdata);
    do_radial_symmetry(sd, ob, brush, ups, action, i, 'Z', feather, userdata);
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
    case SCULPT_TOOL_AUTO_FSET:
      return "Auto Face Set";
    case SCULPT_TOOL_RELAX:
      return "Relax";
  }

  return "Sculpting";
}

/**
 * Operator for applying a stroke (various attributes including mouse path)
 * using the current brush. */

void SCULPT_cache_free(SculptSession *ss, Object *ob, StrokeCache *cache)
{
  MEM_SAFE_FREE(cache->dial);
  MEM_SAFE_FREE(cache->surface_smooth_laplacian_disp);

#ifdef SCULPT_NEIGHBORS_CACHE
  if (ss->cache->ncache) {
    neighbor_cache_free(ss->cache->ncache);
    ss->cache->ncache = NULL;
  }
#endif

  if (ss->custom_layers[SCULPT_SCL_LAYER_DISP]) {
    SCULPT_temp_customlayer_release(ss, ob, ss->custom_layers[SCULPT_SCL_LAYER_DISP]);
    MEM_freeN(ss->custom_layers[SCULPT_SCL_LAYER_DISP]);
    ss->custom_layers[SCULPT_SCL_LAYER_DISP] = NULL;
  }

  if (ss->custom_layers[SCULPT_SCL_LAYER_STROKE_ID]) {
    SCULPT_temp_customlayer_release(ss, ob, ss->custom_layers[SCULPT_SCL_LAYER_STROKE_ID]);
    MEM_freeN(ss->custom_layers[SCULPT_SCL_LAYER_STROKE_ID]);
    ss->custom_layers[SCULPT_SCL_LAYER_STROKE_ID] = NULL;
  }

  MEM_SAFE_FREE(cache->prev_colors);
  MEM_SAFE_FREE(cache->detail_directions);

  if (ss->cache->commandlist) {
    BKE_brush_commandlist_free(ss->cache->commandlist);
  }

  if (ss->custom_layers[SCULPT_SCL_FAIRING_MASK]) {
    SCULPT_temp_customlayer_release(ss, ob, ss->custom_layers[SCULPT_SCL_FAIRING_MASK]);
    MEM_freeN(ss->custom_layers[SCULPT_SCL_FAIRING_MASK]);
    ss->custom_layers[SCULPT_SCL_FAIRING_MASK] = NULL;
  }

  if (ss->custom_layers[SCULPT_SCL_FAIRING_FADE]) {
    SCULPT_temp_customlayer_release(ss, ob, ss->custom_layers[SCULPT_SCL_FAIRING_FADE]);

    MEM_freeN(ss->custom_layers[SCULPT_SCL_FAIRING_FADE]);
    ss->custom_layers[SCULPT_SCL_FAIRING_FADE] = NULL;
  }

  if (ss->custom_layers[SCULPT_SCL_PREFAIRING_CO]) {
    SCULPT_temp_customlayer_release(ss, ob, ss->custom_layers[SCULPT_SCL_PREFAIRING_CO]);

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

  if (cache->tool_override_channels) {
    BKE_brush_channelset_free(cache->tool_override_channels);
  }

  MEM_freeN(cache);
}

void SCULPT_release_customlayers(SculptSession *ss, Object *ob, bool non_customdata_only)
{
  for (int i = 0; i < SCULPT_SCL_LAYER_MAX; i++) {
    if (ss->custom_layers[i]) {
      if (non_customdata_only && !ss->custom_layers[i]->params.simple_array) {
        continue;
      }

      SCULPT_temp_customlayer_release(ss, ob, ss->custom_layers[i]);

      MEM_freeN(ss->custom_layers[i]);
      ss->custom_layers[i] = NULL;
    }
  }
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

static BrushChannelSet *sculpt_init_tool_override_channels(Sculpt *sd, SculptSession *ss, int tool)
{
  BrushChannelSet *chset = NULL;
  Brush *newbrush = NULL;

  for (int i = 0; i < sd->paint.tool_slots_len; i++) {
    if (sd->paint.tool_slots[i].brush && sd->paint.tool_slots[i].brush->sculpt_tool == tool) {
      newbrush = sd->paint.tool_slots[i].brush;
    }
  }

  if (!newbrush) {
    Brush dummy = {.sculpt_tool = tool};

    BKE_brush_builtin_create(&dummy, tool);
    chset = dummy.channels;
  }
  else {
    chset = BKE_brush_channelset_copy(newbrush->channels);
  }

  /* paranoid check, make sure all needed channels exist */
  Brush dummy2 = {.sculpt_tool = tool, .channels = chset};

  BKE_brush_builtin_patch(&dummy2, tool);

  return chset;
}

int SCULPT_get_tool(const SculptSession *ss, const Brush *br)
{
  if (ss->cache && ss->cache->tool_override) {
    return ss->cache->tool_override;
  }

  return br->sculpt_tool;
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

  cache->tool_override = RNA_enum_get(op->ptr, "tool_override");

  if (cache->tool_override) {
    cache->tool_override_channels = sculpt_init_tool_override_channels(
        sd, ss, cache->tool_override);
  }

  BrushChannelSet *channels = cache->tool_override ? cache->tool_override_channels :
                                                     brush->channels;

  if (!sd->channels) {
    BKE_brush_init_toolsettings(sd);
  }

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

  float plane_trim = BRUSHSET_GET_FINAL_FLOAT(sd->channels, channels, plane_trim, NULL);
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

  /* initialize speed moving average */
  for (int i = 0; i < SCULPT_SPEED_MA_SIZE; i++) {
    cache->speed_avg[i] = -1.0f;
  }
  cache->last_speed_time = PIL_check_seconds_timer();

  copy_v3_v3(cache->initial_location, ss->cursor_location);
  copy_v3_v3(cache->true_initial_location, ss->cursor_location);

  copy_v3_v3(cache->initial_normal, ss->cursor_normal);
  copy_v3_v3(cache->true_initial_normal, ss->cursor_normal);

  mode = RNA_enum_get(op->ptr, "mode");
  cache->invert = mode == BRUSH_STROKE_INVERT;
  cache->alt_smooth = mode == BRUSH_STROKE_SMOOTH;
  cache->normal_weight = brush->normal_weight;

  /* Interpret invert as following normal, for grab brushes. */
  if (SCULPT_TOOL_HAS_NORMAL_WEIGHT(SCULPT_get_tool(ss, brush))) {
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
    if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_MASK) {
      cache->saved_mask_brush_tool = brush->mask_tool;
      brush->mask_tool = BRUSH_MASK_SMOOTH;
    }
    else if (ELEM(SCULPT_get_tool(ss, brush),
                  SCULPT_TOOL_SLIDE_RELAX,
                  SCULPT_TOOL_RELAX,
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

  cache->supports_gravity = (!ELEM(SCULPT_get_tool(ss, brush),
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
  if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_DRAW_SHARP) {
    cache->original = true;
  }

  if (SCULPT_TOOL_HAS_ACCUMULATE(SCULPT_get_tool(ss, brush))) {
    if (!(BRUSHSET_GET_INT(channels, accumulate, &ss->cache->input_mapping))) {
      cache->original = true;
      if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_DRAW_SHARP) {
        cache->original = false;
      }
    }
  }

  cache->first_time = true;

#define PIXEL_INPUT_THRESHHOLD 5
  if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_ROTATE) {
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
static bool sculpt_needs_delta_from_anchored_origin(SculptSession *ss, Brush *brush)
{
  if (ELEM(SCULPT_get_tool(ss, brush),
           SCULPT_TOOL_GRAB,
           SCULPT_TOOL_POSE,
           SCULPT_TOOL_BOUNDARY,
           SCULPT_TOOL_ARRAY,
           SCULPT_TOOL_THUMB,
           SCULPT_TOOL_ELASTIC_DEFORM)) {
    return true;
  }
  if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_CLOTH &&
      brush->cloth_deform_type == BRUSH_CLOTH_DEFORM_GRAB) {
    return true;
  }
  return false;
}

/* In these brushes the grab delta is calculated from the previous stroke location, which is used
 * to calculate to orientate the brush tip and deformation towards the stroke direction. */
static bool sculpt_needs_delta_for_tip_orientation(SculptSession *ss, Brush *brush)
{
  if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_CLOTH) {
    return SCULPT_get_int(ss, cloth_deform_type, NULL, brush) != BRUSH_CLOTH_DEFORM_GRAB;
  }
  return ELEM(SCULPT_get_tool(ss, brush),
              SCULPT_TOOL_CLAY_STRIPS,
              SCULPT_TOOL_TWIST,
              SCULPT_TOOL_PINCH,
              SCULPT_TOOL_MULTIPLANE_SCRAPE,
              SCULPT_TOOL_CLAY_THUMB,
              SCULPT_TOOL_NUDGE,
              SCULPT_TOOL_SNAKE_HOOK);
}

static void sculpt_rake_data_update(struct SculptRakeData *srd, const float co[3])
{
  float rake_dist = len_v3v3(srd->follow_co, co);
  if (rake_dist > srd->follow_dist) {
    interp_v3_v3v3(srd->follow_co, srd->follow_co, co, rake_dist - srd->follow_dist);
  }
}

static void sculpt_update_brush_delta(UnifiedPaintSettings *ups, Object *ob, Brush *brush)
{
  SculptSession *ss = ob->sculpt;
  StrokeCache *cache = ss->cache;
  const float mouse[2] = {
      cache->mouse_event[0],
      cache->mouse_event[1],
  };

  int tool = SCULPT_get_tool(ss, brush);

  bool bad = !ELEM(tool,
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
                   SCULPT_TOOL_THUMB);

  bad = bad && !sculpt_brush_use_topology_rake(ss, brush);
  bad = bad && !SCULPT_get_bool(ss, use_autofset, NULL, brush);

  if (bad) {
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

  copy_v3_v3(cache->prev_grab_delta, cache->grab_delta);

  /* Compute 3d coordinate at same z from original location + mouse. */
  mul_v3_m4v3(loc, ob->obmat, cache->orig_grab_location);
  ED_view3d_win_to_3d(cache->vc->v3d, cache->vc->region, loc, mouse, grab_location);

  /* Compute delta to move verts by. */
  if (!SCULPT_stroke_is_first_brush_step_of_symmetry_pass(ss->cache)) {
    if (sculpt_needs_delta_from_anchored_origin(ss, brush)) {
      sub_v3_v3v3(delta, grab_location, cache->old_grab_location);
      invert_m4_m4(imat, ob->obmat);
      mul_mat3_m4_v3(imat, delta);
      add_v3_v3(cache->grab_delta, delta);
    }
    else if (sculpt_needs_delta_for_tip_orientation(ss, brush)) {
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

  if (sculpt_needs_delta_from_anchored_origin(ss, brush)) {
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

  if (SCULPT_stroke_is_first_brush_step(cache)) {
    copy_v3_v3(cache->prev_grab_delta, cache->grab_delta);

    for (int i = 0; i < GRAB_DELTA_MA_SIZE; i++) {
      copy_v3_v3(cache->grab_delta_avg[i], cache->grab_delta);
    }
  }

  // XXX implement me

  if (SCULPT_get_int(ss, use_smoothed_rake, NULL, brush)) {
    // delay by one so we can have a useful value for next_grab_delta
    float grab_delta[3] = {0.0f, 0.0f, 0.0f};

    for (int i = 0; i < GRAB_DELTA_MA_SIZE; i++) {
      add_v3_v3(grab_delta, cache->grab_delta_avg[i]);
    }

    mul_v3_fl(grab_delta, 1.0f / (float)GRAB_DELTA_MA_SIZE);

    copy_v3_v3(cache->grab_delta_avg[cache->grab_delta_avg_cur], cache->grab_delta);
    cache->grab_delta_avg_cur = (cache->grab_delta_avg_cur + 1) % GRAB_DELTA_MA_SIZE;
    copy_v3_v3(cache->grab_delta, grab_delta);

    zero_v3(cache->next_grab_delta);

    for (int i = 0; i < GRAB_DELTA_MA_SIZE; i++) {
      add_v3_v3(cache->next_grab_delta, cache->grab_delta_avg[i]);
    }
    mul_v3_fl(cache->next_grab_delta, 1.0f / (float)GRAB_DELTA_MA_SIZE);
  }
  else {
    copy_v3_v3(cache->next_grab_delta, cache->grab_delta);
  }

  if (!sculpt_brush_needs_rake_rotation(ss, brush)) {
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

static float sculpt_update_speed_average(SculptSession *ss, float speed)
{
  int tot = 0.0;
  bool found = false;

  for (int i = 0; i < SCULPT_SPEED_MA_SIZE; i++) {
    tot++;

    if (ss->cache->speed_avg[i] == -1.0f) {
      ss->cache->speed_avg[i] = speed;
      found = true;
      break;
    }
  }

  if (!found) {
    ss->cache->speed_avg[ss->cache->speed_avg_cur] = speed;
    ss->cache->speed_avg_cur = (ss->cache->speed_avg_cur + 1) % SCULPT_SPEED_MA_SIZE;
  }

  speed = 0.0f;
  tot = 0;
  for (int i = 0; i < SCULPT_SPEED_MA_SIZE; i++) {
    if (ss->cache->speed_avg[i] != -1.0f) {
      speed += ss->cache->speed_avg[i];
      tot++;
    }
  }

  return speed / (float)tot;
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
      !((brush->flag & BRUSH_ANCHORED) || (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_SNAKE_HOOK) ||
        (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_ROTATE) ||
        SCULPT_is_cloth_deform_brush(brush))) {
    RNA_float_get_array(ptr, "location", cache->true_location);
  }

  float last_mouse[2];
  copy_v2_v2(last_mouse, cache->mouse);

  cache->pen_flip = RNA_boolean_get(ptr, "pen_flip");
  RNA_float_get_array(ptr, "mouse", cache->mouse);
  RNA_float_get_array(ptr, "mouse_event", cache->mouse_event);

  float delta_mouse[2];

  sub_v2_v2v2(delta_mouse, cache->mouse, cache->mouse_event);
  float speed = len_v2(delta_mouse) / (800000.0f); /*get a reasonably usable value*/
  speed /= PIL_check_seconds_timer() - cache->last_speed_time;

  cache->input_mapping.speed = sculpt_update_speed_average(ss, speed);
  cache->last_speed_time = PIL_check_seconds_timer();

  /* XXX: Use pressure value from first brush step for brushes which don't support strokes (grab,
   * thumb). They depends on initial state and brush coord/pressure/etc.
   * It's more an events design issue, which doesn't split coordinate/pressure/angle changing
   * events. We should avoid this after events system re-design. */
  if (paint_supports_dynamic_size(brush, PAINT_MODE_SCULPT) || cache->first_time) {
    cache->pressure = RNA_float_get(ptr, "pressure");
    cache->input_mapping.pressure = sqrtf(cache->pressure);
    // printf("pressure: %f\n", cache->pressure);
  }

  cache->input_mapping.random = BLI_thread_frand(0);

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

    cache->input_mapping.angle = (atan2(direction[1], direction[0]) / (float)M_PI) * 0.5 + 0.5;
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
  if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_CLAY_THUMB) {
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

  if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_ROTATE) {
    cache->vertex_rotation = -BLI_dial_angle(cache->dial, cache->mouse) * cache->bstrength;

    ups->draw_anchored = true;
    copy_v2_v2(ups->anchored_initial_mouse, cache->initial_mouse);
    copy_v3_v3(cache->anchored_location, cache->true_location);
    ups->anchored_size = ups->pixel_radius;
  }

  cache->special_rotation = ups->brush_rotation;
  cache->iteration_count++;

  cache->input_mapping.stroke_t = cache->stroke_distance_t /
                                  10.0f; /*scale to a more user-friendly value*/
}

/* Returns true if any of the smoothing modes are active (currently
 * one of smooth brush, autosmooth, mask smooth, or shift-key
 * smooth). */
static bool sculpt_needs_connectivity_info(Sculpt *sd,
                                           const Brush *brush,
                                           SculptSession *ss,
                                           int stroke_mode)
{
  //  if (ss && ss->pbvh && SCULPT_is_automasking_enabled(sd, ss, brush)) {
  return true;
  //  }
  return ((stroke_mode == BRUSH_STROKE_SMOOTH) || (ss && ss->cache && ss->cache->alt_smooth) ||
          (brush->sculpt_tool == SCULPT_TOOL_SMOOTH) || (brush->autosmooth_factor > 0) ||
          ((brush->sculpt_tool == SCULPT_TOOL_MASK) && (brush->mask_tool == BRUSH_MASK_SMOOTH)) ||
          (brush->sculpt_tool == SCULPT_TOOL_POSE) || (brush->sculpt_tool == SCULPT_TOOL_PAINT) ||
          (brush->sculpt_tool == SCULPT_TOOL_SMEAR) ||
          (brush->sculpt_tool == SCULPT_TOOL_BOUNDARY) ||
          (brush->sculpt_tool == SCULPT_TOOL_SLIDE_RELAX) ||
          (brush->sculpt_tool == SCULPT_TOOL_CLOTH) || (brush->sculpt_tool == SCULPT_TOOL_SMEAR) ||
          (brush->sculpt_tool == SCULPT_TOOL_DRAW_FACE_SETS) ||
          (brush->sculpt_tool == SCULPT_TOOL_DISPLACEMENT_SMEAR));
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

  if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_MASK) {
    need_mask = true;
  }

  if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_CLOTH ||
      SCULPT_get_int(ss, deform_target, sd, brush) == BRUSH_DEFORM_TARGET_CLOTH_SIM) {
    need_mask = true;
  }

  view3d_operator_needs_opengl(C);
  sculpt_brush_init_tex(scene, sd, ss);

  is_smooth = sculpt_needs_connectivity_info(sd, brush, ss, mode);
  needs_colors = ELEM(SCULPT_get_tool(ss, brush), SCULPT_TOOL_PAINT, SCULPT_TOOL_SMEAR);

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
  if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_CLOTH) {
    return;
  }

  /* Restore the mesh before continuing with anchored stroke. */
  if ((brush->flag & BRUSH_ANCHORED) ||
      ((ELEM(SCULPT_get_tool(ss, brush), SCULPT_TOOL_GRAB, SCULPT_TOOL_ELASTIC_DEFORM)) &&
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
    if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_ARRAY) {
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
  if (BKE_paintmode_get_active_from_context(C) == PAINT_MODE_SCULPT) {
    /* load brush settings into old Brush fields so the
       paint API can get at then */
    Sculpt *sd = CTX_data_tool_settings(C)->sculpt;
    Brush *brush = BKE_paint_brush(&sd->paint);
    Object *ob = CTX_data_active_object(C);

    if (brush && brush->channels) {
      int tool = RNA_enum_get(op->ptr, "tool_override");
      BrushChannelSet *channels = brush->channels;

      if (tool) { /* note that ss->cache does not exist at this point */
        channels = sculpt_init_tool_override_channels(sd, ob->sculpt, tool);
      }

      BKE_brush_channelset_compat_load(sculpt_get_brush_channels(ob->sculpt, brush), brush, false);

      if (tool) {
        BKE_brush_channelset_free(channels);
      }
    }
  }

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
    if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_ARRAY) {
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

    brush->channels = BKE_brush_channelset_create("brush 0");
    BKE_brush_builtin_patch(brush, brush->sculpt_tool);
    BKE_brush_channelset_compat_load(brush->channels, brush, true);
  }

  if (brush->channels && sd->channels) {
    ss->cache->channels_final = BKE_brush_channelset_create("channels_final");
    BKE_brush_channelset_merge(
        ss->cache->channels_final, sculpt_get_brush_channels(ss, brush), sd->channels);
  }
  else if (brush->channels) {
    ss->cache->channels_final = BKE_brush_channelset_copy(sculpt_get_brush_channels(ss, brush));
  }

  // bad debug global
  extern bool pbvh_show_orig_co;
  pbvh_show_orig_co = BRUSHSET_GET_INT(ss->cache->channels_final, show_origco, NULL);

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

    // BKE_brush_channelset_set_float(ss->cache->channels_final, "strength",
    // ss->cache->bstrength);
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

  if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_SCENE_PROJECT) {
    SCULPT_stroke_cache_snap_context_init(C, ob);
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
      do_symmetrical_brush_actions(sd, ob, sculpt_topology_update, ups, NULL);

      if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_SNAKE_HOOK) {
        /* run dyntopo again for snake hook */
        do_symmetrical_brush_actions(sd, ob, sculpt_topology_update, ups, NULL);
      }
    }
  }

  bool run_commandlist = brush_uses_commandlist(brush, SCULPT_get_tool(ss, brush));

  if (run_commandlist) {
    if (SCULPT_stroke_is_first_brush_step(ss->cache)) {
      if (ss->cache->commandlist) {
        BKE_brush_commandlist_free(ss->cache->commandlist);
      }

      BrushCommandList *list = ss->cache->commandlist = BKE_brush_commandlist_create();
      int tool = ss->cache && ss->cache->tool_override ? ss->cache->tool_override :
                                                         brush->sculpt_tool;

      if (tool == SCULPT_TOOL_SLIDE_RELAX && ss->cache->alt_smooth) {
        tool = SCULPT_TOOL_RELAX;
      }

      BKE_builtin_commandlist_create(
          brush, ss->cache->channels_final, list, tool, &ss->cache->input_mapping);
    }

    SCULPT_run_commandlist(sd, ob, brush, ss->cache->commandlist, ups);

    float location[3];

    /* Update average stroke position. */
    copy_v3_v3(location, ss->cache->true_location);
    mul_m4_v3(ob->obmat, location);

    add_v3_v3(ups->average_stroke_accum, location);
    ups->average_stroke_counter++;
    /* Update last stroke position. */
    ups->last_stroke_valid = true;

    // copy_v3_v3(ss->cache->true_last_location, ss->cache->true_location);
  }
  else {
    do_symmetrical_brush_actions(sd, ob, do_brush_action, ups, NULL);
  }

  if (ss->needs_pbvh_rebuild) {
    /* The mesh was modified, rebuild the PBVH. */
    BKE_particlesystem_reset_all(ob);
    BKE_ptcache_object_reset(CTX_data_scene(C), ob, PTCACHE_RESET_OUTDATED);

    DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
    BKE_scene_graph_update_tagged(CTX_data_ensure_evaluated_depsgraph(C), CTX_data_main(C));
    SCULPT_pbvh_clear(ob);
    Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
    BKE_sculpt_update_object_for_edit(depsgraph, ob, true, false, false);

    if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_ARRAY) {
      SCULPT_tag_update_overlays(C);
    }
    ss->needs_pbvh_rebuild = false;
  }

  if (!run_commandlist) {
    sculpt_combine_proxies(sd, ob);
  }

  if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_FAIRING) {
    SCULPT_fairing_brush_exec_fairing_for_cache(sd, ob);
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
    SCULPT_flush_stroke_deform(sd, ob, sculpt_tool_is_proxy_used(SCULPT_get_tool(ss, brush)));
  }
  else if (ss->shapekey_active) {
    sculpt_update_keyblock(ob);
  }

  ss->cache->first_time = false;
  copy_v3_v3(ss->cache->true_last_location, ss->cache->true_location);

  /* Cleanup. */
  if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_MASK) {
    SCULPT_flush_update_step(C, SCULPT_UPDATE_MASK);
  }
  else if (ELEM(SCULPT_get_tool(ss, brush), SCULPT_TOOL_PAINT, SCULPT_TOOL_SMEAR)) {
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
    if (SCULPT_get_tool(ss, brush) == SCULPT_TOOL_MASK) {
      brush->mask_tool = ss->cache->saved_mask_brush_tool;
    }
    else if (ELEM(SCULPT_get_tool(ss, brush),
                  SCULPT_TOOL_SLIDE_RELAX,
                  SCULPT_TOOL_RELAX,
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
    SCULPT_automasking_cache_free(ss, ob, ss->cache->automasking);
  }

  int tool = SCULPT_get_tool(ss, brush);  // save tool for after we've freed ss->cache

  BKE_pbvh_node_color_buffer_free(ss->pbvh);
  SCULPT_cache_free(ss, ob, ss->cache);
  ss->cache = NULL;

  if (tool == SCULPT_TOOL_ARRAY) {
    SCULPT_undo_push_node(ob, NULL, SCULPT_UNDO_GEOMETRY);
    SCULPT_array_datalayers_free(ss->array, ob);
  }

  SCULPT_undo_push_end(ob);

  if (tool == SCULPT_TOOL_MASK) {
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

  if (ignore_background_click && !over_mesh(C, op, event->xy[0], event->xy[1])) {
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
    SCULPT_cache_free(ss, ob, ss->cache);
    ss->cache = NULL;
  }

  sculpt_brush_exit_tex(sd);
}

extern const EnumPropertyItem rna_enum_brush_sculpt_tool_items[];
static EnumPropertyItem *stroke_tool_items = NULL;

void SCULPT_OT_brush_stroke(wmOperatorType *ot)
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

  paint_stroke_operator_properties(ot, true);

  RNA_def_boolean(ot->srna,
                  "ignore_background_click",
                  0,
                  "Ignore Background Click",
                  "Clicks on the background do not start the stroke");

  if (!stroke_tool_items) {
    int count = 0;
    while (rna_enum_brush_sculpt_tool_items[count++].identifier) {
    }

    stroke_tool_items = calloc(count + 1, sizeof(*stroke_tool_items));

    stroke_tool_items[0].identifier = "NONE";
    stroke_tool_items[0].icon = ICON_NONE;
    stroke_tool_items[0].value = 0;
    stroke_tool_items[0].name = "None";
    stroke_tool_items[0].description = "Unset";
    memcpy(stroke_tool_items + 1,
           rna_enum_brush_sculpt_tool_items,
           sizeof(*stroke_tool_items) * count);
  }

  PropertyRNA *prop = RNA_def_enum(
      ot->srna, "tool_override", stroke_tool_items, 0, "Tool Override", "Set custom brush tool");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
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
  ob->sculpt->active_face_index.i = SCULPT_REF_NONE;
  ob->sculpt->active_vertex_index.i = SCULPT_REF_NONE;

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
        SCULPT_undo_push_end(ob);
      }
      else if (need_bmlog) {
        if (ob->sculpt->bm_log) {
          BM_log_free(ob->sculpt->bm_log, true);
          ob->sculpt->bm_log = NULL;
        }

        SCULPT_undo_ensure_bmlog(ob);

        // SCULPT_undo_ensure_bmlog failed to find a sculpt undo step
        if (!ob->sculpt->bm_log) {
          ob->sculpt->bm_log = BM_log_create(ob->sculpt->bm, ob->sculpt->cd_sculpt_vert);
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

void SCULPT_ensure_epmap(SculptSession *ss)
{
  if (BKE_pbvh_type(ss->pbvh) != PBVH_BMESH && !ss->epmap) {
    BKE_mesh_edge_poly_map_create(&ss->epmap,
                                  &ss->epmap_mem,
                                  ss->medge,
                                  ss->totedges,
                                  ss->mpoly,
                                  ss->totfaces,
                                  ss->mloop,
                                  ss->totloops);
  }
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

#if 0
  if (BKE_pbvh_type(ss->pbvh) == PBVH_BMESH) {
    BMVert *v = (BMVert *)vertex.i;
    MSculptVert *mv = BKE_PBVH_SCULPTVERT(ss->cd_sculpt_vert, v);

    if (mv->flag & SCULPTVERT_NEED_VALENCE) {
      BKE_pbvh_bmesh_update_valence(ss->cd_sculpt_vert, vertex);
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

  MSculptVert *mv = ss->mdyntopo_verts + vertex.i;

  if (mv->flag & SCULPTVERT_NEED_VALENCE) {
    mv->flag &= ~SCULPTVERT_NEED_VALENCE;

    SCULPT_VERTEX_NEIGHBORS_ITER_BEGIN (ss, vertex, ni) {
      tot++;
    }
    SCULPT_VERTEX_NEIGHBORS_ITER_END(ni);

    mv->valence = tot;
  }

  return mv->valence;
}

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
 * Copyright 2012, Blender Foundation.
 */

#include "COM_KeyingScreenOperation.h"

#include "BKE_movieclip.h"
#include "BKE_tracking.h"

#include "IMB_imbuf.h"
#include "IMB_imbuf_types.h"

namespace blender::compositor {

KeyingScreenOperation::KeyingScreenOperation()
{
  this->addOutputSocket(DataType::Color);
  m_movieClip = nullptr;
  m_framenumber = 0;
  m_trackingObject[0] = 0;
  flags.complex = true;
  m_cachedTriangulation = nullptr;
}

void KeyingScreenOperation::initExecution()
{
  initMutex();
  if (execution_model_ == eExecutionModel::FullFrame) {
    BLI_assert(m_cachedTriangulation == nullptr);
    if (m_movieClip) {
      m_cachedTriangulation = buildVoronoiTriangulation();
    }
  }
  else {
    m_cachedTriangulation = nullptr;
  }
}

void KeyingScreenOperation::deinitExecution()
{
  if (m_cachedTriangulation) {
    TriangulationData *triangulation = m_cachedTriangulation;

    if (triangulation->triangulated_points) {
      MEM_freeN(triangulation->triangulated_points);
    }

    if (triangulation->triangles) {
      MEM_freeN(triangulation->triangles);
    }

    if (triangulation->triangles_AABB) {
      MEM_freeN(triangulation->triangles_AABB);
    }

    MEM_freeN(m_cachedTriangulation);

    m_cachedTriangulation = nullptr;
  }
}

KeyingScreenOperation::TriangulationData *KeyingScreenOperation::buildVoronoiTriangulation()
{
  MovieClipUser user = {0};
  TriangulationData *triangulation;
  MovieTracking *tracking = &m_movieClip->tracking;
  MovieTrackingTrack *track;
  VoronoiSite *sites, *site;
  ImBuf *ibuf;
  ListBase *tracksbase;
  ListBase edges = {nullptr, nullptr};
  int sites_total;
  int i;
  int width = this->getWidth();
  int height = this->getHeight();
  int clip_frame = BKE_movieclip_remap_scene_to_clip_frame(m_movieClip, m_framenumber);

  if (m_trackingObject[0]) {
    MovieTrackingObject *object = BKE_tracking_object_get_named(tracking, m_trackingObject);

    if (!object) {
      return nullptr;
    }

    tracksbase = BKE_tracking_object_get_tracks(tracking, object);
  }
  else {
    tracksbase = BKE_tracking_get_active_tracks(tracking);
  }

  /* count sites */
  for (track = (MovieTrackingTrack *)tracksbase->first, sites_total = 0; track;
       track = track->next) {
    MovieTrackingMarker *marker = BKE_tracking_marker_get(track, clip_frame);
    float pos[2];

    if (marker->flag & MARKER_DISABLED) {
      continue;
    }

    add_v2_v2v2(pos, marker->pos, track->offset);

    if (!IN_RANGE_INCL(pos[0], 0.0f, 1.0f) || !IN_RANGE_INCL(pos[1], 0.0f, 1.0f)) {
      continue;
    }

    sites_total++;
  }

  if (!sites_total) {
    return nullptr;
  }

  BKE_movieclip_user_set_frame(&user, clip_frame);
  ibuf = BKE_movieclip_get_ibuf(m_movieClip, &user);

  if (!ibuf) {
    return nullptr;
  }

  triangulation = (TriangulationData *)MEM_callocN(sizeof(TriangulationData),
                                                   "keying screen triangulation data");

  sites = (VoronoiSite *)MEM_callocN(sizeof(VoronoiSite) * sites_total,
                                     "keyingscreen voronoi sites");
  track = (MovieTrackingTrack *)tracksbase->first;
  for (track = (MovieTrackingTrack *)tracksbase->first, site = sites; track; track = track->next) {
    MovieTrackingMarker *marker = BKE_tracking_marker_get(track, clip_frame);
    ImBuf *pattern_ibuf;
    int j;
    float pos[2];

    if (marker->flag & MARKER_DISABLED) {
      continue;
    }

    add_v2_v2v2(pos, marker->pos, track->offset);

    if (!IN_RANGE_INCL(pos[0], 0.0f, 1.0f) || !IN_RANGE_INCL(pos[1], 0.0f, 1.0f)) {
      continue;
    }

    pattern_ibuf = BKE_tracking_get_pattern_imbuf(ibuf, track, marker, true, false);

    zero_v3(site->color);

    if (pattern_ibuf) {
      for (j = 0; j < pattern_ibuf->x * pattern_ibuf->y; j++) {
        if (pattern_ibuf->rect_float) {
          add_v3_v3(site->color, &pattern_ibuf->rect_float[4 * j]);
        }
        else {
          unsigned char *rrgb = (unsigned char *)pattern_ibuf->rect;

          site->color[0] += srgb_to_linearrgb((float)rrgb[4 * j + 0] / 255.0f);
          site->color[1] += srgb_to_linearrgb((float)rrgb[4 * j + 1] / 255.0f);
          site->color[2] += srgb_to_linearrgb((float)rrgb[4 * j + 2] / 255.0f);
        }
      }

      mul_v3_fl(site->color, 1.0f / (pattern_ibuf->x * pattern_ibuf->y));
      IMB_freeImBuf(pattern_ibuf);
    }

    site->co[0] = pos[0] * width;
    site->co[1] = pos[1] * height;

    site++;
  }

  IMB_freeImBuf(ibuf);

  BLI_voronoi_compute(sites, sites_total, width, height, &edges);

  BLI_voronoi_triangulate(sites,
                          sites_total,
                          &edges,
                          width,
                          height,
                          &triangulation->triangulated_points,
                          &triangulation->triangulated_points_total,
                          &triangulation->triangles,
                          &triangulation->triangles_total);

  MEM_freeN(sites);
  BLI_freelistN(&edges);

  if (triangulation->triangles_total) {
    rcti *rect;
    rect = triangulation->triangles_AABB = (rcti *)MEM_callocN(
        sizeof(rcti) * triangulation->triangles_total, "voronoi triangulation AABB");

    for (i = 0; i < triangulation->triangles_total; i++, rect++) {
      int *triangle = triangulation->triangles[i];
      VoronoiTriangulationPoint *a = &triangulation->triangulated_points[triangle[0]],
                                *b = &triangulation->triangulated_points[triangle[1]],
                                *c = &triangulation->triangulated_points[triangle[2]];

      float min[2], max[2];

      INIT_MINMAX2(min, max);

      minmax_v2v2_v2(min, max, a->co);
      minmax_v2v2_v2(min, max, b->co);
      minmax_v2v2_v2(min, max, c->co);

      rect->xmin = (int)min[0];
      rect->ymin = (int)min[1];

      rect->xmax = (int)max[0] + 1;
      rect->ymax = (int)max[1] + 1;
    }
  }

  return triangulation;
}

KeyingScreenOperation::TileData *KeyingScreenOperation::triangulate(const rcti *rect)
{
  TileData *tile_data;
  TriangulationData *triangulation;
  int triangles_allocated = 0;
  int chunk_size = 20;
  int i;

  triangulation = m_cachedTriangulation;

  if (!triangulation) {
    return nullptr;
  }

  tile_data = (TileData *)MEM_callocN(sizeof(TileData), "keying screen tile data");

  for (i = 0; i < triangulation->triangles_total; i++) {
    if (BLI_rcti_isect(rect, &triangulation->triangles_AABB[i], nullptr)) {
      tile_data->triangles_total++;

      if (tile_data->triangles_total > triangles_allocated) {
        if (!tile_data->triangles) {
          tile_data->triangles = (int *)MEM_mallocN(sizeof(int) * chunk_size,
                                                    "keying screen tile triangles chunk");
        }
        else {
          tile_data->triangles = (int *)MEM_reallocN(
              tile_data->triangles, sizeof(int) * (triangles_allocated + chunk_size));
        }

        triangles_allocated += chunk_size;
      }

      tile_data->triangles[tile_data->triangles_total - 1] = i;
    }
  }

  return tile_data;
}

void *KeyingScreenOperation::initializeTileData(rcti *rect)
{
  if (m_movieClip == nullptr) {
    return nullptr;
  }

  if (!m_cachedTriangulation) {
    lockMutex();
    if (m_cachedTriangulation == nullptr) {
      m_cachedTriangulation = buildVoronoiTriangulation();
    }
    unlockMutex();
  }

  return triangulate(rect);
}

void KeyingScreenOperation::deinitializeTileData(rcti * /*rect*/, void *data)
{
  TileData *tile_data = (TileData *)data;

  if (tile_data->triangles) {
    MEM_freeN(tile_data->triangles);
  }

  MEM_freeN(tile_data);
}

void KeyingScreenOperation::determine_canvas(const rcti &preferred_area, rcti &r_area)
{
  r_area = COM_AREA_NONE;

  if (m_movieClip) {
    MovieClipUser user = {0};
    int width, height;
    int clip_frame = BKE_movieclip_remap_scene_to_clip_frame(m_movieClip, m_framenumber);

    BKE_movieclip_user_set_frame(&user, clip_frame);
    BKE_movieclip_get_size(m_movieClip, &user, &width, &height);
    r_area = preferred_area;
    r_area.xmax = r_area.xmin + width;
    r_area.ymax = r_area.ymin + height;
  }
}

void KeyingScreenOperation::executePixel(float output[4], int x, int y, void *data)
{
  output[0] = 0.0f;
  output[1] = 0.0f;
  output[2] = 0.0f;
  output[3] = 1.0f;

  if (m_movieClip && data) {
    TriangulationData *triangulation = m_cachedTriangulation;
    TileData *tile_data = (TileData *)data;
    int i;
    float co[2] = {(float)x, (float)y};

    for (i = 0; i < tile_data->triangles_total; i++) {
      int triangle_idx = tile_data->triangles[i];
      rcti *rect = &triangulation->triangles_AABB[triangle_idx];

      if (IN_RANGE_INCL(x, rect->xmin, rect->xmax) && IN_RANGE_INCL(y, rect->ymin, rect->ymax)) {
        int *triangle = triangulation->triangles[triangle_idx];
        VoronoiTriangulationPoint *a = &triangulation->triangulated_points[triangle[0]],
                                  *b = &triangulation->triangulated_points[triangle[1]],
                                  *c = &triangulation->triangulated_points[triangle[2]];
        float w[3];

        if (barycentric_coords_v2(a->co, b->co, c->co, co, w)) {
          if (barycentric_inside_triangle_v2(w)) {
            output[0] = a->color[0] * w[0] + b->color[0] * w[1] + c->color[0] * w[2];
            output[1] = a->color[1] * w[0] + b->color[1] * w[1] + c->color[1] * w[2];
            output[2] = a->color[2] * w[0] + b->color[2] * w[1] + c->color[2] * w[2];

            break;
          }
        }
      }
    }
  }
}

void KeyingScreenOperation::update_memory_buffer_partial(MemoryBuffer *output,
                                                         const rcti &area,
                                                         Span<MemoryBuffer *> inputs)
{
  if (m_movieClip == nullptr) {
    output->fill(area, COM_COLOR_BLACK);
    return;
  }

  TileData *tri_area = this->triangulate(&area);
  BLI_assert(tri_area != nullptr);

  const int *triangles = tri_area->triangles;
  const int num_triangles = tri_area->triangles_total;
  const TriangulationData *triangulation = m_cachedTriangulation;
  for (BuffersIterator<float> it = output->iterate_with(inputs, area); !it.is_end(); ++it) {
    copy_v4_v4(it.out, COM_COLOR_BLACK);

    const float co[2] = {(float)it.x, (float)it.y};
    for (int i = 0; i < num_triangles; i++) {
      const int triangle_idx = triangles[i];
      const rcti *rect = &triangulation->triangles_AABB[triangle_idx];

      if (!BLI_rcti_isect_pt(rect, it.x, it.y)) {
        continue;
      }

      const int *triangle = triangulation->triangles[triangle_idx];
      const VoronoiTriangulationPoint &a = triangulation->triangulated_points[triangle[0]];
      const VoronoiTriangulationPoint &b = triangulation->triangulated_points[triangle[1]];
      const VoronoiTriangulationPoint &c = triangulation->triangulated_points[triangle[2]];

      float w[3];
      if (!barycentric_coords_v2(a.co, b.co, c.co, co, w)) {
        continue;
      }

      if (barycentric_inside_triangle_v2(w)) {
        it.out[0] = a.color[0] * w[0] + b.color[0] * w[1] + c.color[0] * w[2];
        it.out[1] = a.color[1] * w[0] + b.color[1] * w[1] + c.color[1] * w[2];
        it.out[2] = a.color[2] * w[0] + b.color[2] * w[1] + c.color[2] * w[2];
        break;
      }
    }
  }

  if (tri_area->triangles) {
    MEM_freeN(tri_area->triangles);
  }

  MEM_freeN(tri_area);
}

}  // namespace blender::compositor

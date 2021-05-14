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
 * The Original Code is Copyright (C) 2020 Blender Foundation
 * All rights reserved.
 */

/** \file
 * \ingroup bgpencil
 */

#include "BLI_float3.hh"
#include "BLI_math.h"
#include "BLI_span.hh"

#include "DNA_gpencil_types.h"

#include "BKE_gpencil.h"
#include "BKE_gpencil_geom.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_query.h"

#include "ED_gpencil.h"

#include "gpencil_io.h"
#include "gpencil_io_import_svg.hh"

/* Custom flags for NanoSVG. */
#define NANOSVG_ALL_COLOR_KEYWORDS
#define NANOSVG_IMPLEMENTATION

#include "nanosvg/nanosvg.h"

using blender::MutableSpan;

namespace blender::io::gpencil {

/* Constructor. */
GpencilImporterSVG::GpencilImporterSVG(const char *filename, const GpencilIOParams *iparams)
    : GpencilImporter(iparams)
{
  filename_set(filename);
}

bool GpencilImporterSVG::read()
{
  bool result = true;
  NSVGimage *svg_data = nullptr;
  svg_data = nsvgParseFromFile(filename_, "mm", 96.0f);
  if (svg_data == nullptr) {
    std::cout << " Could not open SVG.\n ";
    return false;
  }

  /* Create grease pencil object. */
  params_.ob = create_object();
  if (params_.ob == nullptr) {
    std::cout << "Unable to create new object.\n";
    if (svg_data) {
      nsvgDelete(svg_data);
    }

    return false;
  }
  gpd_ = (bGPdata *)params_.ob->data;

  /* Grease pencil is rotated 90 degrees in X axis by default. */
  float matrix[4][4];
  const float3 scale = float3(params_.scale);
  unit_m4(matrix);
  rotate_m4(matrix, 'X', DEG2RADF(-90.0f));
  rescale_m4(matrix, scale);

  /* Loop all shapes. */
  char prv_id[70] = {"*"};
  int prefix = 0;
  for (NSVGshape *shape = svg_data->shapes; shape; shape = shape->next) {
    char *layer_id = (shape->id_parent[0] == '\0') ? BLI_sprintfN("Layer_%03d", prefix) :
                                                     BLI_sprintfN("%s", shape->id_parent);
    if (!STREQ(prv_id, layer_id)) {
      prefix++;
      MEM_freeN(layer_id);
      layer_id = (shape->id_parent[0] == '\0') ? BLI_sprintfN("Layer_%03d", prefix) :
                                                 BLI_sprintfN("%s", shape->id_parent);
      strcpy(prv_id, layer_id);
    }

    /* Check if the layer exist and create if needed. */
    bGPDlayer *gpl = (bGPDlayer *)BLI_findstring(
        &gpd_->layers, layer_id, offsetof(bGPDlayer, info));
    if (gpl == nullptr) {
      gpl = BKE_gpencil_layer_addnew(gpd_, layer_id, true, false);
      /* Disable lights. */
      gpl->flag &= ~GP_LAYER_USE_LIGHTS;
    }
    MEM_freeN(layer_id);

    /* Check frame. */
    bGPDframe *gpf = BKE_gpencil_layer_frame_get(gpl, cfra_, GP_GETFRAME_ADD_NEW);
    /* Create materials. */
    bool is_stroke = (bool)shape->stroke.type;
    bool is_fill = (bool)shape->fill.type;
    if ((!is_stroke) && (!is_fill)) {
      is_stroke = true;
    }

    /* Create_shape materials. */
    const char *const mat_names[] = {"Stroke", "Fill", "Both"};
    int index = 0;
    if ((is_stroke) && (!is_fill)) {
      index = 0;
    }
    else if ((!is_stroke) && (is_fill)) {
      index = 1;
    }
    else if ((is_stroke) && (is_fill)) {
      index = 2;
    }
    int32_t mat_index = create_material(mat_names[index], is_stroke, is_fill);

    /* Loop all paths to create the stroke data. */
    for (NSVGpath *path = shape->paths; path; path = path->next) {
      create_stroke(gpd_, gpf, shape, path, mat_index, matrix);
    }
  }

  /* Free SVG memory. */
  nsvgDelete(svg_data);

  /* Calculate bounding box and move all points to new origin center. */
  float gp_center[3];
  BKE_gpencil_centroid_3d(gpd_, gp_center);

  LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd_->layers) {
    LISTBASE_FOREACH (bGPDframe *, gpf, &gpl->frames) {
      LISTBASE_FOREACH (bGPDstroke *, gps, &gpf->strokes) {
        for (bGPDspoint &pt : MutableSpan(gps->points, gps->totpoints)) {
          sub_v3_v3(&pt.x, gp_center);
        }
      }
    }
  }

  return result;
}

void GpencilImporterSVG::create_stroke(bGPdata *gpd,
                                       bGPDframe *gpf,
                                       NSVGshape *shape,
                                       NSVGpath *path,
                                       const int32_t mat_index,
                                       const float matrix[4][4])
{
  const bool is_stroke = (bool)shape->stroke.type;
  const bool is_fill = (bool)shape->fill.type;

  const int edges = params_.resolution;
  const float step = 1.0f / (float)(edges - 1);

  const int totpoints = (path->npts / 3) * params_.resolution;

  bGPDstroke *gps = BKE_gpencil_stroke_new(mat_index, totpoints, 1.0f);
  BLI_addtail(&gpf->strokes, gps);

  if (path->closed == '1') {
    gps->flag |= GP_STROKE_CYCLIC;
  }
  if (is_stroke) {
    gps->thickness = shape->strokeWidth * params_.scale;
  }
  /* Apply Fill vertex color. */
  if (is_fill) {
    NSVGpaint fill = shape->fill;
    convert_color(fill.color, gps->vert_color_fill);
    gps->fill_opacity_fac = gps->vert_color_fill[3];
    gps->vert_color_fill[3] = 1.0f;
  }

  int start_index = 0;
  for (int i = 0; i < path->npts - 1; i += 3) {
    float *p = &path->pts[i * 2];
    float a = 0.0f;
    for (int v = 0; v < edges; v++) {
      bGPDspoint *pt = &gps->points[start_index];
      pt->strength = shape->opacity;
      pt->pressure = 1.0f;
      pt->z = 0.0f;
      /* TODO(antoniov): Can be improved loading curve data instead of loading strokes. */
      interp_v2_v2v2v2v2_cubic(&pt->x, &p[0], &p[2], &p[4], &p[6], a);

      /* Scale from millimeters. */
      mul_v3_fl(&pt->x, 0.001f);
      mul_m4_v3(matrix, &pt->x);

      /* Apply color to vertex color. */
      if (is_fill) {
        NSVGpaint fill = shape->fill;
        convert_color(fill.color, pt->vert_color);
      }
      if (is_stroke) {
        NSVGpaint stroke = shape->stroke;
        convert_color(stroke.color, pt->vert_color);
        gps->fill_opacity_fac = pt->vert_color[3];
      }
      pt->vert_color[3] = 1.0f;

      a += step;
      start_index++;
    }
  }

  /* Cleanup and recalculate geometry. */
  BKE_gpencil_stroke_merge_distance(gpd, gpf, gps, 0.001f, true);
  BKE_gpencil_stroke_geometry_update(gpd, gps);
}

/* Unpack internal NanoSVG color. */
static void unpack_nano_color(const unsigned int pack, float r_col[4])
{
  unsigned char rgb_u[4];

  rgb_u[0] = ((pack) >> 0) & 0xFF;
  rgb_u[1] = ((pack) >> 8) & 0xFF;
  rgb_u[2] = ((pack) >> 16) & 0xFF;
  rgb_u[3] = ((pack) >> 24) & 0xFF;

  r_col[0] = (float)rgb_u[0] / 255.0f;
  r_col[1] = (float)rgb_u[1] / 255.0f;
  r_col[2] = (float)rgb_u[2] / 255.0f;
  r_col[3] = (float)rgb_u[3] / 255.0f;
}

void GpencilImporterSVG::convert_color(const int32_t color, float r_linear_rgba[4])
{
  float rgba[4];
  unpack_nano_color(color, rgba);

  srgb_to_linearrgb_v3_v3(r_linear_rgba, rgba);
  r_linear_rgba[3] = rgba[3];
}

}  // namespace blender::io::gpencil

/* SPDX-FileCopyrightText: 2017 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "draw_curves_private.hh"
#include "draw_pass.hh"

namespace blender::bke {
class CurvesGeometry;
}

namespace blender::draw {

struct CurvesEvalCache;

class CurveRefinePass : public PassSimple {
 public:
  CurveRefinePass(const char *name) : PassSimple(name) {};
};

using CurvesInfosBuf = UniformBuffer<CurvesInfos>;

struct CurvesUniformBufPool {
  Vector<std::unique_ptr<CurvesInfosBuf>> ubos;
  int used = 0;

  void reset()
  {
    used = 0;
    /* Allocate dummy. */
    alloc();
    ubos.first()->push_update();
  }

  CurvesInfosBuf &dummy_get()
  {
    return *ubos.first();
  }

  CurvesInfosBuf &alloc();
};

struct CurvesModule {
  CurvesUniformBufPool ubo_pool;
  CurveRefinePass refine = {"CurvesEvalPass"};
  /* Contains all transient input buffers contained inside `refine`.
   * Cleared after update. */
  Vector<gpu::VertBufPtr> transient_buffers;

  gpu::VertBuf *dummy_vbo = drw_curves_ensure_dummy_vbo();

  ~CurvesModule()
  {
    GPU_VERTBUF_DISCARD_SAFE(dummy_vbo);
  }

  void init()
  {
    ubo_pool.reset();

    refine.init();
    refine.state_set(DRW_STATE_NO_DRAW);
  }

  /* Record evaluation inside `refine`.
   * Output will be ready once `refine` pass has been submitted. */
  void evaluate_curve_attribute(bool has_catmull,
                                bool has_bezier,
                                bool has_poly,
                                bool has_nurbs,
                                bool has_cyclic,
                                int curve_count,
                                CurvesEvalCache &cache,
                                CurvesEvalShader shader_type,
                                gpu::VertBufPtr input_buf,
                                gpu::VertBufPtr &output_buf,
                                /* For radius during position evaluation. */
                                gpu::VertBuf *input2_buf = nullptr,
                                /* For baking a transform during position evaluation. */
                                float4x4 transform = float4x4::identity());

  void evaluate_positions(bool has_catmull,
                          bool has_bezier,
                          bool has_poly,
                          bool has_nurbs,
                          bool has_cyclic,
                          int curve_count,
                          CurvesEvalCache &cache,
                          gpu::VertBufPtr input_pos_buf,
                          gpu::VertBufPtr input_rad_buf,
                          gpu::VertBufPtr &output_pos_buf,
                          float4x4 transform = float4x4::identity())
  {
    evaluate_curve_attribute(has_catmull,
                             has_bezier,
                             has_poly,
                             has_nurbs,
                             has_cyclic,
                             curve_count,
                             cache,
                             CURVES_EVAL_POSITION,
                             std::move(input_pos_buf),
                             output_pos_buf,
                             /* Transfer ownership through optional argument. */
                             input_rad_buf.release(),
                             transform);
  }

  void evaluate_curve_length_intercept(bool has_cyclic, int curve_count, CurvesEvalCache &cache);

  gpu::VertBufPtr evaluate_topology_indirection(const int curve_count,
                                                const int point_count,
                                                CurvesEvalCache &cache,
                                                bool is_ribbon,
                                                bool has_cyclic);

 private:
  gpu::VertBuf *drw_curves_ensure_dummy_vbo();

  void dispatch(int curve_count, PassSimple::Sub &pass);
};

}  // namespace blender::draw

#define MAX_LAYER_NAME_CT 4 /* `u0123456789, u, au, a0123456789`. */
#define MAX_LAYER_NAME_LEN (GPU_MAX_SAFE_ATTR_NAME + 2)
#define MAX_THICKRES 2    /* see eHairType */
#define MAX_HAIR_SUBDIV 4 /* see hair_subdiv rna */

enum ParticleRefineShader {
  PART_REFINE_CATMULL_ROM = 0,
  PART_REFINE_MAX_SHADER,
};

struct ModifierData;
struct Object;
struct ParticleHairCache;
struct ParticleSystem;

namespace blender::draw {

void drw_particle_update_ptcache(Object *object_eval, ParticleSystem *psys);
ParticleDrawSource drw_particle_get_hair_source(Object *object,
                                                ParticleSystem *psys,
                                                ModifierData *md,
                                                PTCacheEdit *edit,
                                                int additional_subdivision);

CurvesEvalCache &hair_particle_get_eval_cache(ParticleDrawSource &src);

}  // namespace blender::draw

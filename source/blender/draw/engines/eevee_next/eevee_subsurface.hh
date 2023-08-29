/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup eevee
 *
 * Postprocess diffuse radiance output from the diffuse evaluation pass to mimic subsurface
 * transmission.
 *
 * This implementation follows the technique described in the siggraph presentation:
 * "Efficient screen space subsurface scattering Siggraph 2018"
 * by Evgenii Golubev
 *
 * But, instead of having all the precomputed weights for all three color primaries,
 * we precompute a weight profile texture to be able to support per pixel AND per channel radius.
 */

#pragma once

#include "eevee_shader.hh"
#include "eevee_shader_shared.hh"

namespace blender::eevee {

/* -------------------------------------------------------------------- */
/** \name Subsurface
 *
 * \{ */

class Instance;

struct SubsurfaceModule {
 private:
  Instance &inst_;
  /** Contains samples locations. */
  SubsurfaceDataBuf data_;
  /** Scene diffuse irradiance. Pointer binded at sync time, set at render time. */
  GPUTexture *diffuse_light_tx_;
  /** Subsurface eval pass. Runs after the deferred pass. */
  PassSimple subsurface_ps_ = {"Subsurface"};

 public:
  SubsurfaceModule(Instance &inst) : inst_(inst)
  {
    /* Force first update. */
    data_.sample_len = -1;
  };

  ~SubsurfaceModule(){};

  /** Contains translucence profile for a single color channel. */
  static const Vector<float> &transmittance_profile();

  void end_sync();

  void render(View &view, Framebuffer &fb, Texture &diffuse_light_tx);

  template<typename T> void bind_resources(draw::detail::PassBase<T> *pass)
  {
    pass->bind_ubo("sss_buf", data_);
  }

 private:
  void precompute_samples_location();

  /** Christensen-Burley implementation. */
  static float burley_setup(float radius, float albedo);
  static float burley_sample(float d, float x_rand);
  static float burley_eval(float d, float r);
  static float burley_pdf(float d, float r);
};

/** \} */

}  // namespace blender::eevee

/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#ifndef __FILM_H__
#define __FILM_H__

#include "scene/pass.h"
#include "util/string.h"
#include "util/vector.h"

#include "kernel/types.h"

#include "graph/node.h"

CCL_NAMESPACE_BEGIN

class Device;
class DeviceScene;
class Scene;

typedef enum FilterType {
  FILTER_BOX,
  FILTER_GAUSSIAN,
  FILTER_BLACKMAN_HARRIS,

  FILTER_NUM_TYPES,
} FilterType;

class Film : public Node {
 public:
  NODE_DECLARE

  NODE_SOCKET_API(float, exposure)
  NODE_SOCKET_API(float, pass_alpha_threshold)

  NODE_SOCKET_API(PassType, display_pass)
  NODE_SOCKET_API(bool, show_active_pixels)

  NODE_SOCKET_API(FilterType, filter_type)
  NODE_SOCKET_API(float, filter_width)

  NODE_SOCKET_API(float, mist_start)
  NODE_SOCKET_API(float, mist_depth)
  NODE_SOCKET_API(float, mist_falloff)

  NODE_SOCKET_API(CryptomatteType, cryptomatte_passes)
  NODE_SOCKET_API(int, cryptomatte_depth)

  /* Approximate shadow catcher pass into its matte pass, so that both artificial objects and
   * shadows can be alpha-overed onto a backdrop. */
  NODE_SOCKET_API(bool, use_approximate_shadow_catcher)

 private:
  size_t filter_table_offset_;
  bool prev_have_uv_pass = false;
  bool prev_have_motion_pass = false;
  bool prev_have_ao_pass = false;

 public:
  Film();
  ~Film();

  /* add default passes to scene */
  static void add_default(Scene *scene);

  void device_update(Device *device, DeviceScene *dscene, Scene *scene);
  void device_free(Device *device, DeviceScene *dscene, Scene *scene);

  int get_aov_offset(Scene *scene, string name, bool &is_color);

  bool update_lightgroups(Scene *scene);

  /* Update passes so that they contain all passes required for the configured functionality.
   *
   * If `add_sample_count_pass` is true then the SAMPLE_COUNT pass is ensured to be added. */
  void update_passes(Scene *scene, bool add_sample_count_pass);

  uint get_kernel_features(const Scene *scene) const;

 private:
  void add_auto_pass(Scene *scene, PassType type, const char *name = nullptr);
  void add_auto_pass(Scene *scene, PassType type, PassMode mode, const char *name = nullptr);
  void remove_auto_passes(Scene *scene);
  void finalize_passes(Scene *scene, const bool use_denoise);
};

CCL_NAMESPACE_END

#endif /* __FILM_H__ */

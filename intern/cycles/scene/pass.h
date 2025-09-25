/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include <iosfwd>

#include "util/string.h"
#include "util/unique_ptr_vector.h"

#include "kernel/types.h"

#include "graph/node.h"

CCL_NAMESPACE_BEGIN

const char *pass_type_as_string(const PassType type);

enum class PassMode {
  NOISY,
  DENOISED,
};
const char *pass_mode_as_string(PassMode mode);
std::ostream &operator<<(std::ostream &os, PassMode mode);

struct PassInfo {
  int num_components = -1;
  bool use_filter = false;
  bool use_exposure = false;
  bool is_written = true;
  float scale = 1.0f;
  PassType divide_type = PASS_NONE;
  PassType direct_type = PASS_NONE;
  PassType indirect_type = PASS_NONE;

  /* Pass access for read can not happen directly and needs some sort of compositing (for example,
   * light passes due to divide_type, or shadow catcher pass. */
  bool use_compositing = false;

  /* Used to disable albedo pass for denoising.
   * Light and shadow catcher passes should not have discontinuity in the denoised result based on
   * the underlying albedo. */
  bool use_denoising_albedo = true;

  /* Pass supports denoising. */
  bool support_denoise = false;
};

class Pass : public Node {
 public:
  NODE_DECLARE

  NODE_SOCKET_API(PassType, type)
  NODE_SOCKET_API(PassMode, mode)
  NODE_SOCKET_API(ustring, name)
  NODE_SOCKET_API(bool, include_albedo)
  NODE_SOCKET_API(ustring, lightgroup)

  Pass();

  PassInfo get_info() const;

  /* The pass is written by the render pipeline (kernel or denoiser). If the pass is written it
   * will have pixels allocated in a RenderBuffer. Passes which are not written do not have their
   * pixels allocated to save memory. */
  bool is_written() const;

 protected:
  /* This has been created automatically as a requirement to various rendering functionality
   * (such as adaptive sampling). */
  bool is_auto_;

 public:
  static const NodeEnum *get_type_enum();
  static const NodeEnum *get_mode_enum();

  static PassInfo get_info(PassType type,
                           const PassMode mode = PassMode::DENOISED,
                           const bool include_albedo = false,
                           const bool is_lightgroup = false);

  static bool contains(const unique_ptr_vector<Pass> &passes, PassType type);

  /* Returns nullptr if there is no pass with the given name or type+mode. */
  static const Pass *find(const unique_ptr_vector<Pass> &passes, const string &name);
  static const Pass *find(const unique_ptr_vector<Pass> &passes,
                          PassType type,
                          PassMode mode = PassMode::NOISY,
                          const ustring &lightgroup = ustring());

  /* Returns PASS_UNUSED if there is no corresponding pass. */
  static int get_offset(const unique_ptr_vector<Pass> &passes, const Pass *pass);

  friend class Film;
};

std::ostream &operator<<(std::ostream &os, const Pass &pass);

bool is_volume_guiding_pass(const PassType pass_type);

CCL_NAMESPACE_END

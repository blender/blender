/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#ifdef WITH_OPENIMAGEDENOISE

#  include <cstdint>
#  include <memory>
#  include <string>

#  include "BLI_map.hh"

#  include <OpenImageDenoise/oidn.hpp>

namespace blender::compositor {

class Context;
class Result;

enum class DenoisedAuxiliaryPassType : uint8_t {
  Albedo,
  Normal,
};

/* ------------------------------------------------------------------------------------------------
 * Denoised Auxiliary Pass Key.
 */
class DenoisedAuxiliaryPassKey {
 public:
  DenoisedAuxiliaryPassType type;
  oidn::Quality quality;

  DenoisedAuxiliaryPassKey(const DenoisedAuxiliaryPassType type, const oidn::Quality quality);

  uint64_t hash() const;
};

bool operator==(const DenoisedAuxiliaryPassKey &a, const DenoisedAuxiliaryPassKey &b);

/* -------------------------------------------------------------------------------------------------
 * Denoised Auxiliary Pass.
 *
 * A derived result that stores a denoised version of the auxiliary pass of the given type using
 * the given quality. */
class DenoisedAuxiliaryPass {
 public:
  float *denoised_buffer = nullptr;

 public:
  DenoisedAuxiliaryPass(Context &context,
                        const Result &pass,
                        const DenoisedAuxiliaryPassType type,
                        const oidn::Quality quality);

  ~DenoisedAuxiliaryPass();
};

/* ------------------------------------------------------------------------------------------------
 * Denoised Auxiliary Pass Container.
 */
class DenoisedAuxiliaryPassContainer {
 private:
  Map<DenoisedAuxiliaryPassKey, std::unique_ptr<DenoisedAuxiliaryPass>> map_;

 public:
  /* Check if there is an available DenoisedAuxiliaryPass derived resource with the given
   * parameters in the container, if one exists, return it, otherwise, return a newly created one
   * and add it to the container. */
  DenoisedAuxiliaryPass &get(Context &context,
                             const Result &pass,
                             const DenoisedAuxiliaryPassType type,
                             const oidn::Quality quality);
};

}  // namespace blender::compositor

#else

namespace blender::compositor {

/* Building without OIDN, define a dummy container. User is not expected to use it if OIDN is not
 * available. */
class DenoisedAuxiliaryPassContainer {};

}  // namespace blender::compositor

#endif

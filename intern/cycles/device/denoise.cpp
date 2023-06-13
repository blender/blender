/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2011-2022 Blender Foundation */

#include "device/denoise.h"

CCL_NAMESPACE_BEGIN

const char *denoiserTypeToHumanReadable(DenoiserType type)
{
  switch (type) {
    case DENOISER_OPTIX:
      return "OptiX";
    case DENOISER_OPENIMAGEDENOISE:
      return "OpenImageDenoise";

    case DENOISER_NUM:
    case DENOISER_NONE:
    case DENOISER_ALL:
      return "UNKNOWN";
  }

  return "UNKNOWN";
}

const NodeEnum *DenoiseParams::get_type_enum()
{
  static NodeEnum type_enum;

  if (type_enum.empty()) {
    type_enum.insert("optix", DENOISER_OPTIX);
    type_enum.insert("openimageio", DENOISER_OPENIMAGEDENOISE);
  }

  return &type_enum;
}

const NodeEnum *DenoiseParams::get_prefilter_enum()
{
  static NodeEnum prefilter_enum;

  if (prefilter_enum.empty()) {
    prefilter_enum.insert("none", DENOISER_PREFILTER_NONE);
    prefilter_enum.insert("fast", DENOISER_PREFILTER_FAST);
    prefilter_enum.insert("accurate", DENOISER_PREFILTER_ACCURATE);
  }

  return &prefilter_enum;
}

NODE_DEFINE(DenoiseParams)
{
  NodeType *type = NodeType::add("denoise_params", create);

  const NodeEnum *type_enum = get_type_enum();
  const NodeEnum *prefilter_enum = get_prefilter_enum();

  SOCKET_BOOLEAN(use, "Use", false);

  SOCKET_ENUM(type, "Type", *type_enum, DENOISER_OPENIMAGEDENOISE);

  SOCKET_INT(start_sample, "Start Sample", 0);

  SOCKET_BOOLEAN(use_pass_albedo, "Use Pass Albedo", true);
  SOCKET_BOOLEAN(use_pass_normal, "Use Pass Normal", false);

  SOCKET_BOOLEAN(temporally_stable, "Temporally Stable", false);

  SOCKET_ENUM(prefilter, "Prefilter", *prefilter_enum, DENOISER_PREFILTER_FAST);

  return type;
}

DenoiseParams::DenoiseParams() : Node(get_node_type()) {}

CCL_NAMESPACE_END

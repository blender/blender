/*
 * Copyright 2011-2021 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

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

  SOCKET_ENUM(prefilter, "Prefilter", *prefilter_enum, DENOISER_PREFILTER_FAST);

  return type;
}

DenoiseParams::DenoiseParams() : Node(get_node_type())
{
}

CCL_NAMESPACE_END

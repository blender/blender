/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_armature_info.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_armature_envelope_solid)

#include "select_lib.glsl"

void main()
{
  float n = normalize(normalView).z;
  if (isDistance) {
    n = 1.0f - clamp(-n, 0.0f, 1.0f);
    fragColor = float4(1.0f, 1.0f, 1.0f, 0.33f * alpha) * n;
  }
  else {
    /* Smooth lighting factor. */
    constexpr float s = 0.2f; /* [0.0f-0.5f] range */
    float fac = clamp((n * (1.0f - s)) + s, 0.0f, 1.0f);
    fragColor.rgb = mix(finalStateColor, finalBoneColor, fac * fac);
    fragColor.a = alpha;
  }
  lineOutput = float4(0.0f);

  select_id_output(select_id);
}

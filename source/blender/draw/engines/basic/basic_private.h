/* SPDX-FileCopyrightText: 2020 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw_engine
 */

#ifdef __cplusplus
extern "C" {
#endif

GPUShader *BASIC_shaders_depth_sh_get(eGPUShaderConfig config);
GPUShader *BASIC_shaders_pointcloud_depth_sh_get(eGPUShaderConfig config);
GPUShader *BASIC_shaders_curves_depth_sh_get(eGPUShaderConfig config);
GPUShader *BASIC_shaders_depth_conservative_sh_get(eGPUShaderConfig config);
GPUShader *BASIC_shaders_pointcloud_depth_conservative_sh_get(eGPUShaderConfig config);
void BASIC_shaders_free(void);

#ifdef __cplusplus
}
#endif

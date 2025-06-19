/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#define SRD_STRUCT_BEGIN(srd) \
  namespace srd { \
  namespace gl_VertexShader { \
  } \
  namespace gl_FragmentShader { \
  } \
  namespace gl_ComputeShader { \
  }
#define SRD_STRUCT_END(srd) }

#define SRD_VERTEX_IN_BEGIN(srd) SRD_STRUCT_BEGIN(srd)
#define SRD_VERTEX_IN_END(srd) SRD_STRUCT_END(srd)
#define SRD_VERTEX_IN(srd, binding, type, name) const type name = {};

#define SRD_VERTEX_OUT_BEGIN(srd) GPU_SHADER_INTERFACE_INFO(srd)
#define SRD_VERTEX_OUT_END(srd) GPU_SHADER_INTERFACE_END()
#define SRD_VERTEX_OUT(srd, qual, type, name) type name = {};

#define SRD_FRAGMENT_IN_BEGIN(srd) SRD_STRUCT_BEGIN(srd)
#define SRD_FRAGMENT_IN_END(srd) SRD_STRUCT_END(srd)
#define SRD_FRAGMENT_IN(srd, qual, type, name) const type name = {};
#define SRD_FRAGMENT_IN_ROG(srd, slot, type, img_type, name, rog) const type name = {};

#define SRD_FRAGMENT_OUT_BEGIN(srd) SRD_STRUCT_BEGIN(srd)
#define SRD_FRAGMENT_OUT_END(srd) SRD_STRUCT_END(srd)
#define SRD_FRAGMENT_OUT(srd, binding, type, name) type name = {};
#define SRD_FRAGMENT_OUT_DUAL(srd, slot, type, name, blend) type name = {};
#define SRD_FRAGMENT_OUT_ROG(srd, slot, type, name, rog) type name = {};

#define SRD_RESOURCE_BEGIN(srd) SRD_STRUCT_BEGIN(srd)
#define SRD_RESOURCE_END(srd) SRD_STRUCT_END(srd)
#define SRD_RESOURCE_SPECIALIZATION_CONSTANT(srd, type, name, default) type name = {};
#define SRD_RESOURCE_PUSH_CONSTANT(srd, type, name) type name = {};
#define SRD_RESOURCE_PUSH_CONSTANT_ARRAY(srd, type, name, array) type(*name) = {};
#define SRD_RESOURCE_SAMPLER(srd, binding, type, name) type name = {};
#define SRD_RESOURCE_SAMPLER_FREQ(srd, binding, type, name, freq) type name = {};
#define SRD_RESOURCE_IMAGE(srd, binding, format, access, type, name) _##access type name;
#define SRD_RESOURCE_IMAGE_FREQ(srd, binding, format, access, type, name, freq) \
  _##access type name;
#define SRD_RESOURCE_STORAGE_BUF(srd, binding, access, type, name, array) type(*name) array = {};
#define SRD_RESOURCE_STORAGE_BUF_FREQ(srd, binding, access, type, name, array, freq) \
  type(*name) array = {};
#define SRD_RESOURCE_UNIFORM_BUF(srd, binding, type, name, array) type(*name) array = {};
#define SRD_RESOURCE_UNIFORM_BUF_FREQ(srd, binding, type, name, array, freq) \
  type(*name) array = {};
#define SRD_RESOURCE_STRUCT(srd, type, name) type name = {};
/* Temporary solution while everything is being ported.
 * Should be replace by SRD_RESOURCE_STRUCT. */
#define SRD_RESOURCE_ADDITIONAL_INFO(srd, type) using namespace type;

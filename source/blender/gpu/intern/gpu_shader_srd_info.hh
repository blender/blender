/* SPDX-FileCopyrightText: 2021 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#define SRD_STRUCT_BEGIN(srd) \
  struct srd { \
    static void populate(blender::gpu::shader::ShaderCreateInfo &info) \
    {

#define SRD_STRUCT_END(srd) \
  } \
  } \
  ;

#define SRD_VERTEX_IN_BEGIN(srd) SRD_STRUCT_BEGIN(srd)
#define SRD_VERTEX_IN_END(srd) SRD_STRUCT_END(srd)
#define SRD_VERTEX_IN(srd, binding, type, name) info.vertex_in(binding, Type::type##_t, #name);

#define SRD_VERTEX_OUT_BEGIN(srd) GPU_SHADER_INTERFACE_INFO(srd)
#define SRD_VERTEX_OUT_END(srd) GPU_SHADER_INTERFACE_END()
#define SRD_VERTEX_OUT(srd, qual, type, name) .qual(Type::type##_t, #name)

#define SRD_FRAGMENT_IN_BEGIN(srd) SRD_STRUCT_BEGIN(srd)
#define SRD_FRAGMENT_IN_END(srd) SRD_STRUCT_END(srd)
#define SRD_FRAGMENT_IN(srd, qual, type, name) info.qual(Type::type##_t, #name);
#define SRD_FRAGMENT_IN_ROG(srd, slot, type, img_type, name, rog) \
  info.subpass_in(slot, type, img_type, name, rog)

#define SRD_FRAGMENT_OUT_BEGIN(srd) SRD_STRUCT_BEGIN(srd)
#define SRD_FRAGMENT_OUT_END(srd) SRD_STRUCT_END(srd)
#define SRD_FRAGMENT_OUT(srd, binding, type, name) \
  info.fragment_out(binding, Type::type##_t, #name);
#define SRD_FRAGMENT_OUT_DUAL(srd, slot, type, name, blend) \
  info.fragment_out(slot, Type::type##_t, #name, DualBlend::blend)
#define SRD_FRAGMENT_OUT_ROG(srd, slot, type, name, rog) \
  info.fragment_out(slot, Type::type##_t, #name, DualBlend::NONE, rog)

#define SRD_RESOURCE_BEGIN(srd) SRD_STRUCT_BEGIN(srd)
#define SRD_RESOURCE_END(srd) SRD_STRUCT_END(srd)
#define SRD_RESOURCE_SPECIALIZATION_CONSTANT(srd, type, name, default) \
  info.specialization_constant(Type::type##_t, #name, default);
#define SRD_RESOURCE_PUSH_CONSTANT(srd, type, name) info.push_constant(Type::type##_t, #name);
#define SRD_RESOURCE_PUSH_CONSTANT_ARRAY(srd, type, name, array) \
  info.push_constant(Type::type##_t, #name, array);
#define SRD_RESOURCE_SAMPLER(srd, binding, type, name) \
  info.sampler(binding, ImageType::type, #name);
#define SRD_RESOURCE_SAMPLER_FREQ(srd, binding, type, name, freq) \
  info.sampler(binding, ImageType::type, #name, Frequency::freq);
#define SRD_RESOURCE_IMAGE(srd, binding, format, access, type, name) \
  info.image(binding, format, Qualifier::access, ImageReadWriteType::type, #name);
#define SRD_RESOURCE_IMAGE_FREQ(srd, binding, format, access, type, name, freq) \
  info.image(binding, format, Qualifier::access, ImageReadWriteType::type, #name, Frequency::freq);
#define SRD_RESOURCE_STORAGE_BUF(srd, binding, access, type, name, array) \
  info.storage_buf(binding, Qualifier::access, #type, #name #array);
#define SRD_RESOURCE_STORAGE_BUF_FREQ(srd, binding, access, type, name, array, freq) \
  info.storage_buf(binding, Qualifier::access, #type, #name #array, Frequency::freq);
#define SRD_RESOURCE_UNIFORM_BUF(srd, binding, type, name, array) \
  info.uniform_buf(binding, #type, #name #array);
#define SRD_RESOURCE_UNIFORM_BUF_FREQ(srd, binding, type, name, array, freq) \
  info.uniform_buf(binding, #type, #name #array, Frequency::freq);
#define SRD_RESOURCE_STRUCT(srd, type, name) type::populate(info);
/* Temporary solution while everything is being ported.
 * Should be replace by SRD_RESOURCE_STRUCT. */
#define SRD_RESOURCE_ADDITIONAL_INFO(srd, type) info.additional_info(#type);

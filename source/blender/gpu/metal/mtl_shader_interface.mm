/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * GPU shader interface (C --> GLSL)
 */

#include "BLI_bitmap.h"

#include "GPU_capabilities.h"

#include "mtl_common.hh"
#include "mtl_debug.hh"
#include "mtl_shader_interface.hh"
#include "mtl_shader_interface_type.hh"

#include "BLI_blenlib.h"
#include "BLI_math_base.h"
#include "BLI_utildefines.h"
#include "MEM_guardedalloc.h"

namespace blender::gpu {

MTLShaderInterface::MTLShaderInterface(const char *name)
{
  /* Shared ShaderInputs array is populated later on in `prepare_common_shader_inputs`
   * after Metal Shader Interface preparation. */
  inputs_ = nullptr;

  if (name != nullptr) {
    strcpy(this->name, name);
  }

  /* Ensure #ShaderInterface parameters are cleared. */
  this->init();
}

MTLShaderInterface::~MTLShaderInterface()
{
  for (const int i : IndexRange(ARGUMENT_ENCODERS_CACHE_SIZE)) {
    if (arg_encoders_[i].encoder != nil) {
      id<MTLArgumentEncoder> enc = arg_encoders_[i].encoder;
      [enc release];
    }
  }
}

const char *MTLShaderInterface::get_name_at_offset(uint32_t offset) const
{
  return name_buffer_ + offset;
}

void MTLShaderInterface::init()
{
  total_attributes_ = 0;
  total_uniform_blocks_ = 0;
  max_uniformbuf_index_ = 0;
  total_uniforms_ = 0;
  total_textures_ = 0;
  max_texture_index_ = -1;
  enabled_attribute_mask_ = 0;
  total_vert_stride_ = 0;
  sampler_use_argument_buffer_ = false;
  sampler_argument_buffer_bind_index_vert_ = -1;
  sampler_argument_buffer_bind_index_frag_ = -1;

  /* NULL initialize uniform location markers for builtins. */
  for (const int u : IndexRange(GPU_NUM_UNIFORMS)) {
    builtins_[u] = -1;
  }
  for (const int ubo : IndexRange(GPU_NUM_UNIFORM_BLOCKS)) {
    builtin_blocks_[ubo] = -1;
  }
  for (const int tex : IndexRange(MTL_MAX_TEXTURE_SLOTS)) {
    textures_[tex].used = false;
    textures_[tex].slot_index = -1;
  }

  /* Null initialization for argument encoders. */
  for (const int i : IndexRange(ARGUMENT_ENCODERS_CACHE_SIZE)) {
    arg_encoders_[i].encoder = nil;
    arg_encoders_[i].buffer_index = -1;
  }
}

void MTLShaderInterface::add_input_attribute(uint32_t name_offset,
                                             uint32_t attribute_location,
                                             MTLVertexFormat format,
                                             uint32_t buffer_index,
                                             uint32_t size,
                                             uint32_t offset,
                                             int matrix_element_count)
{
  MTLShaderInputAttribute &input_attr = attributes_[total_attributes_];
  input_attr.name_offset = name_offset;
  input_attr.format = format;
  input_attr.location = attribute_location;
  input_attr.size = size;
  input_attr.buffer_index = buffer_index;
  input_attr.offset = offset;
  input_attr.matrix_element_count = matrix_element_count;
  input_attr.index = total_attributes_;
  total_attributes_++;
  total_vert_stride_ = max_ii(total_vert_stride_, offset + size);
  enabled_attribute_mask_ |= (1 << attribute_location);
}

uint32_t MTLShaderInterface::add_uniform_block(uint32_t name_offset,
                                               uint32_t buffer_index,
                                               uint32_t size,
                                               ShaderStage stage_mask)
{
  /* Ensure Size is 16 byte aligned to guarantees alignment rules are satisfied. */
  if ((size % 16) != 0) {
    size += 16 - (size % 16);
  }

  MTLShaderUniformBlock &uni_block = ubos_[total_uniform_blocks_];
  uni_block.name_offset = name_offset;
  uni_block.buffer_index = buffer_index;
  uni_block.size = size;
  uni_block.current_offset = 0;
  uni_block.stage_mask = ShaderStage::BOTH;
  max_uniformbuf_index_ = max_ii(max_uniformbuf_index_, buffer_index);
  return (total_uniform_blocks_++);
}

void MTLShaderInterface::add_push_constant_block(uint32_t name_offset)
{
  push_constant_block_.name_offset = name_offset;
  /* Push constant data block is always uniform buffer index 0. */
  push_constant_block_.buffer_index = 0;
  /* Size starts at zero and grows as uniforms are added. */
  push_constant_block_.size = 0;

  push_constant_block_.current_offset = 0;
  push_constant_block_.stage_mask = ShaderStage::BOTH;
}

void MTLShaderInterface::add_uniform(uint32_t name_offset, eMTLDataType type, int array_len)
{
  BLI_assert(array_len > 0);
  BLI_assert(total_uniforms_ < MTL_MAX_UNIFORMS_PER_BLOCK);
  if (total_uniforms_ >= MTL_MAX_UNIFORMS_PER_BLOCK) {
    MTL_LOG_WARNING(
        "[Warning] Cannot add uniform '%s' to shader interface '%s' as the uniform limit of %d "
        "has been reached.\n",
        name,
        name,
        MTL_MAX_UNIFORMS_PER_BLOCK);
    return;
  }
  MTLShaderUniform &uniform = uniforms_[total_uniforms_];
  uniform.name_offset = name_offset;

  /* Determine size and offset alignment -- C++ struct alignment rules: Base address of value must
   * match alignment of type. GLSL follows minimum type alignment of 4. */
  int data_type_size = mtl_get_data_type_size(type) * array_len;
  int data_type_alignment = max_ii(mtl_get_data_type_alignment(type), 4);
  int current_offset = push_constant_block_.current_offset;
  if ((current_offset % data_type_alignment) != 0) {
    current_offset += data_type_alignment - (current_offset % data_type_alignment);
  }

  uniform.size_in_bytes = data_type_size;
  uniform.byte_offset = current_offset;
  uniform.type = type;
  uniform.array_len = array_len;
  total_uniforms_++;

  /* Update Push constant block-- update offset, re-size and re-align total memory requirement to
   * be 16-byte aligned. Following GLSL std140. */
  push_constant_block_.current_offset = current_offset + data_type_size;
  if (push_constant_block_.current_offset > push_constant_block_.size) {
    push_constant_block_.size = push_constant_block_.current_offset;
    if ((push_constant_block_.size % 16) != 0) {
      push_constant_block_.size += 16 - (push_constant_block_.size % 16);
    }
  }

  /* Validate properties. */
  BLI_assert(uniform.size_in_bytes > 0);
  BLI_assert_msg(
      current_offset + data_type_size <= push_constant_block_.size,
      "Uniform size and offset sits outside the specified size range for the uniform block");
}

void MTLShaderInterface::add_texture(uint32_t name_offset,
                                     uint32_t texture_slot,
                                     eGPUTextureType tex_binding_type,
                                     eGPUSamplerFormat sampler_format,
                                     ShaderStage stage_mask)
{
  BLI_assert(texture_slot >= 0 && texture_slot < GPU_max_textures());
  BLI_assert(sampler_format < GPU_SAMPLER_TYPE_MAX);
  if (texture_slot >= 0 && texture_slot < GPU_max_textures()) {

    MTLShaderTexture &tex = textures_[texture_slot];
    BLI_assert_msg(tex.used == false, "Texture slot already in-use by another binding");
    tex.name_offset = name_offset;
    tex.slot_index = texture_slot;
    tex.type = tex_binding_type;
    tex.sampler_format = sampler_format;
    tex.stage_mask = stage_mask;
    tex.used = true;
    total_textures_++;
    max_texture_index_ = max_ii(max_texture_index_, texture_slot);
  }
  else {
    BLI_assert_msg(false, "Exceeding maximum supported texture count.");
    MTL_LOG_WARNING(
        "Could not add additional texture with index %d to shader interface. Maximum "
        "supported texture count is %d\n",
        texture_slot,
        GPU_max_textures());
  }
}

void MTLShaderInterface::map_builtins()
{
  /* Clear builtin arrays to NULL locations. */
  for (const int u : IndexRange(GPU_NUM_UNIFORMS)) {
    builtins_[u] = -1;
  }
  for (const int ubo : IndexRange(GPU_NUM_UNIFORM_BLOCKS)) {
    builtin_blocks_[ubo] = -1;
  }

  /* Resolve and cache uniform locations for builtin uniforms. */
  for (const int u : IndexRange(GPU_NUM_UNIFORMS)) {
    const ShaderInput *uni = this->uniform_get(builtin_uniform_name((GPUUniformBuiltin)u));
    if (uni != nullptr) {
      BLI_assert(uni->location >= 0);
      if (uni->location >= 0) {
        builtins_[u] = uni->location;
        MTL_LOG_INFO("Mapped builtin uniform '%s' NB: '%s' to location: %d\n",
                     builtin_uniform_name((GPUUniformBuiltin)u),
                     get_name_at_offset(uni->name_offset),
                     uni->location);
      }
    }
  }

  /* Resolve and cache uniform locations for builtin uniform blocks. */
  for (const int u : IndexRange(GPU_NUM_UNIFORM_BLOCKS)) {
    const ShaderInput *uni = this->ubo_get(builtin_uniform_block_name((GPUUniformBlockBuiltin)u));

    if (uni != nullptr) {
      BLI_assert(uni->location >= 0);
      if (uni->location >= 0) {
        builtin_blocks_[u] = uni->binding;
        MTL_LOG_INFO("Mapped builtin uniform block '%s' to location %d\n",
                     builtin_uniform_block_name((GPUUniformBlockBuiltin)u),
                     uni->location);
      }
    }
  }
}

/* Populate #ShaderInput struct based on interface. */
void MTLShaderInterface::prepare_common_shader_inputs()
{
  /* `ShaderInput inputs_` maps a uniform name to an external
   * uniform location, which is used as an array index to look-up
   * information in the local #MTLShaderInterface input structs.
   *
   * #ShaderInput population follows the ordering rules in #gpu_shader_interface. */

  /* Populate #ShaderInterface counts. */
  attr_len_ = this->get_total_attributes();
  ubo_len_ = this->get_total_uniform_blocks();
  uniform_len_ = this->get_total_uniforms() + this->get_total_textures();

  /* TODO(Metal): Support storage buffer bindings. Pending compute shader support. */
  ssbo_len_ = 0;

  /* Calculate total inputs and allocate #ShaderInput array. */
  /* NOTE: We use the existing `name_buffer_` allocated for internal input structs. */
  int input_tot_len = attr_len_ + ubo_len_ + uniform_len_ + ssbo_len_;
  inputs_ = (ShaderInput *)MEM_callocN(sizeof(ShaderInput) * input_tot_len, __func__);
  ShaderInput *current_input = inputs_;

  /* Attributes. */
  for (const int attr_index : IndexRange(total_attributes_)) {
    MTLShaderInputAttribute &shd_attr = attributes_[attr_index];
    current_input->name_offset = shd_attr.name_offset;
    current_input->name_hash = BLI_hash_string(this->get_name_at_offset(shd_attr.name_offset));
    /* For Metal, we flatten the vertex attribute indices within the shader in order to minimize
     * complexity.  ShaderInput "Location" contains the original attribute location, as can be
     * fetched using `GPU_shader_get_attribute_info`. ShaderInput binding contains the array index
     * into the MTLShaderInterface `attributes_` array. */
    current_input->location = shd_attr.location;
    current_input->binding = attr_index;
    current_input++;
  }

  /* UBOs. */
  BLI_assert(&inputs_[attr_len_] >= current_input);
  current_input = &inputs_[attr_len_];
  for (const int ubo_index : IndexRange(total_uniform_blocks_)) {
    MTLShaderUniformBlock &shd_ubo = ubos_[ubo_index];
    current_input->name_offset = shd_ubo.name_offset;
    current_input->name_hash = BLI_hash_string(this->get_name_at_offset(shd_ubo.name_offset));
    /* Location refers to the index in the ubos_ array. */
    current_input->location = ubo_index;
    /* Binding location refers to the UBO bind slot in
     * #MTLContextGlobalShaderPipelineState::ubo_bindings. The buffer bind index [[buffer(N)]]
     * within the shader will apply an offset for bound vertex buffers and the default uniform
     * PushConstantBlock. */
    current_input->binding = shd_ubo.buffer_index;
    current_input++;
  }

  /* Uniforms. */
  BLI_assert(&inputs_[attr_len_ + ubo_len_] >= current_input);
  current_input = &inputs_[attr_len_ + ubo_len_];
  for (const int uniform_index : IndexRange(total_uniforms_)) {
    MTLShaderUniform &shd_uni = uniforms_[uniform_index];
    current_input->name_offset = shd_uni.name_offset;
    current_input->name_hash = BLI_hash_string(this->get_name_at_offset(shd_uni.name_offset));
    current_input->location = uniform_index;
    current_input->binding = uniform_index;
    current_input++;
  }

  /* Textures.
   * NOTE(Metal): Textures are externally treated as uniforms in #gpu_shader_interface.
   * Location for textures resolved as `binding` value. This
   * is the index into the local `MTLShaderTexture textures[]` array.
   *
   * In MSL, we cannot trivially remap which texture slot a given texture
   * handle points to, unlike in GLSL, where a uniform sampler/image can be updated
   * and queried as both a texture and a uniform. */
  for (int texture_index = 0; texture_index <= max_texture_index_; texture_index++) {
    const MTLShaderTexture &shd_tex = textures_[texture_index];

    /* Not all texture entries are used when explicit texture locations are specified. */
    if (shd_tex.used) {
      BLI_assert_msg(shd_tex.slot_index == texture_index,
                     "Texture binding slot should match array index for texture.");
      current_input->name_offset = shd_tex.name_offset;
      current_input->name_hash = BLI_hash_string(this->get_name_at_offset(shd_tex.name_offset));

      /* Location represents look-up address.
       * For Metal, this location is a unique value offset by
       * total_uniforms such that it does not overlap.
       *
       * This range offset allows a check in the uniform look-up
       * to ensure texture handles are not treated as standard uniforms in Metal. */
      current_input->location = texture_index + total_uniforms_;

      /* Binding represents texture slot `[[texture(n)]]`. */
      current_input->binding = shd_tex.slot_index;
      current_input++;
    }
  }

  /* SSBO bindings.
   * TODO(Metal): Support SSBOs. Pending compute support. */
  BLI_assert(&inputs_[attr_len_ + ubo_len_ + uniform_len_] >= current_input);
  current_input = &inputs_[attr_len_ + ubo_len_ + uniform_len_];

  /* Map builtin uniform indices to uniform binding locations. */
  this->map_builtins();
}

void MTLShaderInterface::set_sampler_properties(bool use_argument_buffer,
                                                uint32_t argument_buffer_bind_index_vert,
                                                uint32_t argument_buffer_bind_index_frag)
{
  sampler_use_argument_buffer_ = use_argument_buffer;
  sampler_argument_buffer_bind_index_vert_ = argument_buffer_bind_index_vert;
  sampler_argument_buffer_bind_index_frag_ = argument_buffer_bind_index_frag;
}

/* Attributes. */
const MTLShaderInputAttribute &MTLShaderInterface::get_attribute(uint index) const
{
  BLI_assert(index < MTL_MAX_VERTEX_INPUT_ATTRIBUTES);
  BLI_assert(index < get_total_attributes());
  return attributes_[index];
}

uint32_t MTLShaderInterface::get_total_attributes() const
{
  return total_attributes_;
}

uint32_t MTLShaderInterface::get_total_vertex_stride() const
{
  return total_vert_stride_;
}

uint32_t MTLShaderInterface::get_enabled_attribute_mask() const
{
  return enabled_attribute_mask_;
}

/* Uniforms. */
const MTLShaderUniform &MTLShaderInterface::get_uniform(uint index) const
{
  BLI_assert(index < MTL_MAX_UNIFORMS_PER_BLOCK);
  BLI_assert(index < get_total_uniforms());
  return uniforms_[index];
}

uint32_t MTLShaderInterface::get_total_uniforms() const
{
  return total_uniforms_;
}

/* Uniform Blocks. */
const MTLShaderUniformBlock &MTLShaderInterface::get_uniform_block(uint index) const
{
  BLI_assert(index < MTL_MAX_UNIFORM_BUFFER_BINDINGS);
  BLI_assert(index < get_total_uniform_blocks());
  return ubos_[index];
}

const MTLShaderUniformBlock &MTLShaderInterface::get_push_constant_block() const
{
  return push_constant_block_;
}

uint32_t MTLShaderInterface::get_total_uniform_blocks() const
{
  return total_uniform_blocks_;
}

uint32_t MTLShaderInterface::get_max_ubo_index() const
{
  return max_uniformbuf_index_;
}

bool MTLShaderInterface::has_uniform_block(uint32_t block_index) const
{
  return (block_index < total_uniform_blocks_);
}

uint32_t MTLShaderInterface::get_uniform_block_size(uint32_t block_index) const
{
  return (block_index < total_uniform_blocks_) ? ubos_[block_index].size : 0;
}

/* Textures. */
const MTLShaderTexture &MTLShaderInterface::get_texture(uint index) const
{
  BLI_assert(index < MTL_MAX_TEXTURE_SLOTS);
  BLI_assert(index <= get_max_texture_index());
  return textures_[index];
}

uint32_t MTLShaderInterface::get_total_textures() const
{
  return total_textures_;
}

uint32_t MTLShaderInterface::get_max_texture_index() const
{
  return max_texture_index_;
}

bool MTLShaderInterface::get_use_argument_buffer_for_samplers(
    int *vertex_arg_buffer_bind_index, int *fragment_arg_buffer_bind_index) const
{
  /* Returns argument buffer binding slot for each shader stage.
   * The exact bind slot may be different, as each stage has different buffer inputs. */
  *vertex_arg_buffer_bind_index = sampler_argument_buffer_bind_index_vert_;
  *fragment_arg_buffer_bind_index = sampler_argument_buffer_bind_index_frag_;
  return sampler_use_argument_buffer_;
}

id<MTLArgumentEncoder> MTLShaderInterface::find_argument_encoder(int buffer_index) const
{
  id encoder = nil;
  for (const int i : IndexRange(ARGUMENT_ENCODERS_CACHE_SIZE)) {
    encoder = arg_encoders_[i].buffer_index == buffer_index ? arg_encoders_[i].encoder : encoder;
  }
  return encoder;
}

void MTLShaderInterface::insert_argument_encoder(int buffer_index, id encoder)
{
  for (const int i : IndexRange(ARGUMENT_ENCODERS_CACHE_SIZE)) {
    if (arg_encoders_[i].encoder == nil) {
      arg_encoders_[i].encoder = encoder;
      arg_encoders_[i].buffer_index = buffer_index;
      return;
    }
  }
  MTL_LOG_WARNING("could not insert encoder into cache!");
}

MTLVertexFormat mtl_datatype_to_vertex_type(eMTLDataType type)
{
  switch (type) {
    case MTL_DATATYPE_CHAR:
      return MTLVertexFormatChar;
    case MTL_DATATYPE_UCHAR:
      return MTLVertexFormatUChar;
    case MTL_DATATYPE_BOOL:
      return MTLVertexFormatUChar;
    case MTL_DATATYPE_CHAR2:
      return MTLVertexFormatChar2;
    case MTL_DATATYPE_UCHAR2:
      return MTLVertexFormatUChar2;
    case MTL_DATATYPE_BOOL2:
      return MTLVertexFormatUChar2;
    case MTL_DATATYPE_SHORT:
      return MTLVertexFormatShort;
    case MTL_DATATYPE_USHORT:
      return MTLVertexFormatUShort;
    case MTL_DATATYPE_CHAR3:
      return MTLVertexFormatChar3;
    case MTL_DATATYPE_UCHAR3:
      return MTLVertexFormatUChar3;
    case MTL_DATATYPE_BOOL3:
      return MTLVertexFormatUChar3;
    case MTL_DATATYPE_CHAR4:
      return MTLVertexFormatChar4;
    case MTL_DATATYPE_UCHAR4:
      return MTLVertexFormatUChar4;
    case MTL_DATATYPE_INT:
      return MTLVertexFormatInt;
    case MTL_DATATYPE_UINT:
      return MTLVertexFormatUInt;
    case MTL_DATATYPE_BOOL4:
      return MTLVertexFormatUChar4;
    case MTL_DATATYPE_SHORT2:
      return MTLVertexFormatShort2;
    case MTL_DATATYPE_USHORT2:
      return MTLVertexFormatUShort2;
    case MTL_DATATYPE_FLOAT:
      return MTLVertexFormatFloat;
    case MTL_DATATYPE_HALF2x2:
    case MTL_DATATYPE_HALF3x2:
    case MTL_DATATYPE_HALF4x2:
      BLI_assert_msg(false, "Unsupported raw vertex attribute types in Blender.");
      return MTLVertexFormatInvalid;

    case MTL_DATATYPE_SHORT3:
      return MTLVertexFormatShort3;
    case MTL_DATATYPE_USHORT3:
      return MTLVertexFormatUShort3;
    case MTL_DATATYPE_SHORT4:
      return MTLVertexFormatShort4;
    case MTL_DATATYPE_USHORT4:
      return MTLVertexFormatUShort4;
    case MTL_DATATYPE_INT2:
      return MTLVertexFormatInt2;
    case MTL_DATATYPE_UINT2:
      return MTLVertexFormatUInt2;
    case MTL_DATATYPE_FLOAT2:
      return MTLVertexFormatFloat2;
    case MTL_DATATYPE_LONG:
      return MTLVertexFormatInt;
    case MTL_DATATYPE_ULONG:
      return MTLVertexFormatUInt;
    case MTL_DATATYPE_HALF2x3:
    case MTL_DATATYPE_HALF2x4:
    case MTL_DATATYPE_HALF3x3:
    case MTL_DATATYPE_HALF3x4:
    case MTL_DATATYPE_HALF4x3:
    case MTL_DATATYPE_HALF4x4:
    case MTL_DATATYPE_FLOAT2x2:
    case MTL_DATATYPE_FLOAT3x2:
    case MTL_DATATYPE_FLOAT4x2:
      BLI_assert_msg(false, "Unsupported raw vertex attribute types in Blender.");
      return MTLVertexFormatInvalid;

    case MTL_DATATYPE_INT3:
      return MTLVertexFormatInt3;
    case MTL_DATATYPE_INT4:
      return MTLVertexFormatInt4;
    case MTL_DATATYPE_UINT3:
      return MTLVertexFormatUInt3;
    case MTL_DATATYPE_UINT4:
      return MTLVertexFormatUInt4;
    case MTL_DATATYPE_FLOAT3:
      return MTLVertexFormatFloat3;
    case MTL_DATATYPE_FLOAT4:
      return MTLVertexFormatFloat4;
    case MTL_DATATYPE_LONG2:
      return MTLVertexFormatInt2;
    case MTL_DATATYPE_ULONG2:
      return MTLVertexFormatUInt2;
    case MTL_DATATYPE_FLOAT2x3:
    case MTL_DATATYPE_FLOAT2x4:
    case MTL_DATATYPE_FLOAT3x3:
    case MTL_DATATYPE_FLOAT3x4:
    case MTL_DATATYPE_FLOAT4x3:
    case MTL_DATATYPE_FLOAT4x4:
      BLI_assert_msg(false, "Unsupported raw vertex attribute types in Blender.");
      return MTLVertexFormatInvalid;

    case MTL_DATATYPE_LONG3:
      return MTLVertexFormatInt3;
    case MTL_DATATYPE_LONG4:
      return MTLVertexFormatInt4;
    case MTL_DATATYPE_ULONG3:
      return MTLVertexFormatUInt3;
    case MTL_DATATYPE_ULONG4:
      return MTLVertexFormatUInt4;

    /* Special Types */
    case MTL_DATATYPE_UINT1010102_NORM:
      return MTLVertexFormatUInt1010102Normalized;
    case MTL_DATATYPE_INT1010102_NORM:
      return MTLVertexFormatInt1010102Normalized;

    default:
      BLI_assert(false);
      return MTLVertexFormatInvalid;
  };
}

}  // namespace blender::gpu

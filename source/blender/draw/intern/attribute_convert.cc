/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BLI_array_utils.hh"

#include "BKE_attribute_math.hh"

#include "GPU_vertex_buffer.hh"

#include "attribute_convert.hh"

namespace blender::draw {

GPUVertFormat init_format_for_attribute(const eCustomDataType data_type,
                                        const StringRefNull vbo_name)
{
  GPUVertFormat format{};
  bke::attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
    using T = decltype(dummy);
    using Converter = AttributeConverter<T>;
    if constexpr (!std::is_void_v<typename Converter::VBOType>) {
      GPU_vertformat_attr_add(&format,
                              vbo_name.c_str(),
                              Converter::gpu_component_type,
                              Converter::gpu_component_len,
                              Converter::gpu_fetch_mode);
    }
  });
  return format;
}

void vertbuf_data_extract_direct(const GSpan attribute, GPUVertBuf &vbo)
{
  bke::attribute_math::convert_to_static_type(attribute.type(), [&](auto dummy) {
    using T = decltype(dummy);
    using Converter = AttributeConverter<T>;
    using VBOType = typename Converter::VBOType;
    if constexpr (!std::is_void_v<VBOType>) {
      const Span<T> src = attribute.typed<T>();
      MutableSpan<VBOType> data(static_cast<VBOType *>(GPU_vertbuf_get_data(&vbo)),
                                attribute.size());
      if constexpr (std::is_same_v<T, VBOType>) {
        array_utils::copy(src, data);
      }
      else {
        threading::parallel_for(src.index_range(), 8192, [&](const IndexRange range) {
          for (const int i : range) {
            data[i] = Converter::convert(src[i]);
          }
        });
      }
    }
  });
}

}  // namespace blender::draw

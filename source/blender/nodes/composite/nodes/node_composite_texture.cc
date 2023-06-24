/* SPDX-FileCopyrightText: 2006 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "COM_cached_texture.hh"
#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

/* **************** TEXTURE ******************** */

namespace blender::nodes::node_composite_texture_cc {

static void cmp_node_texture_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Vector>("Offset")
      .min(-2.0f)
      .max(2.0f)
      .subtype(PROP_TRANSLATION)
      .compositor_expects_single_value();
  b.add_input<decl::Vector>("Scale")
      .default_value({1.0f, 1.0f, 1.0f})
      .min(-10.0f)
      .max(10.0f)
      .subtype(PROP_XYZ)
      .compositor_expects_single_value();
  b.add_output<decl::Float>("Value");
  b.add_output<decl::Color>("Color");
}

using namespace blender::realtime_compositor;

class TextureOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Result &color_result = get_result("Color");
    Result &value_result = get_result("Value");
    if (!get_texture()) {
      if (color_result.should_compute()) {
        color_result.allocate_invalid();
      }
      if (value_result.should_compute()) {
        value_result.allocate_invalid();
      }
      return;
    }

    const Domain domain = compute_domain();
    CachedTexture &cached_texture = context().cache_manager().cached_textures.get(
        context(),
        get_texture(),
        context().use_texture_color_management(),
        domain.size,
        get_input("Offset").get_vector_value_default(float4(0.0f)).xy(),
        get_input("Scale").get_vector_value_default(float4(0.0f)).xy());

    if (color_result.should_compute()) {
      color_result.allocate_texture(domain);
      GPU_texture_copy(color_result.texture(), cached_texture.color_texture());
    }

    if (value_result.should_compute()) {
      value_result.allocate_texture(domain);
      GPU_texture_copy(value_result.texture(), cached_texture.value_texture());
    }
  }

  Domain compute_domain() override
  {
    return Domain(context().get_compositing_region_size());
  }

  Tex *get_texture()
  {
    return reinterpret_cast<Tex *>(bnode().id);
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new TextureOperation(context, node);
}

}  // namespace blender::nodes::node_composite_texture_cc

void register_node_type_cmp_texture()
{
  namespace file_ns = blender::nodes::node_composite_texture_cc;

  static bNodeType ntype;

  cmp_node_type_base(&ntype, CMP_NODE_TEXTURE, "Texture", NODE_CLASS_INPUT);
  ntype.declare = file_ns::cmp_node_texture_declare;
  ntype.flag |= NODE_PREVIEW;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  nodeRegisterType(&ntype);
}

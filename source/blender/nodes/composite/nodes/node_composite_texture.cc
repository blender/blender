/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup cmpnodes
 */

#include "COM_cached_texture.hh"
#include "COM_node_operation.hh"

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

using namespace blender::compositor;

class TextureOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    Tex *texture = get_texture();
    if (!texture || !context().is_valid_compositing_region()) {
      execute_invalid();
      return;
    }

    if (texture->use_nodes) {
      execute_invalid();
      context().set_info_message("Viewport compositor setup not fully supported");
      return;
    }

    const Domain domain = compute_domain();
    CachedTexture &cached_texture = context().cache_manager().cached_textures.get(
        context(),
        texture,
        true,
        domain.size,
        get_input("Offset").get_single_value_default(float4(0.0f)).xyz(),
        get_input("Scale").get_single_value_default(float4(1.0f)).xyz());

    Result &color_result = get_result("Color");
    if (color_result.should_compute()) {
      color_result.wrap_external(cached_texture.color_result);
    }

    Result &value_result = get_result("Value");
    if (value_result.should_compute()) {
      value_result.wrap_external(cached_texture.value_result);
    }
  }

  void execute_invalid()
  {
    Result &color_result = get_result("Color");
    if (color_result.should_compute()) {
      color_result.allocate_invalid();
    }

    Result &value_result = get_result("Value");
    if (value_result.should_compute()) {
      value_result.allocate_invalid();
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

  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeTexture", CMP_NODE_TEXTURE);
  ntype.ui_name = "Texture";
  ntype.ui_description = "Generate texture pattern from texture datablock";
  ntype.enum_name_legacy = "TEXTURE";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.declare = file_ns::cmp_node_texture_declare;
  ntype.compositor_unsupported_message = N_(
      "Texture nodes not supported in the Viewport compositor");
  ntype.flag |= NODE_PREVIEW;
  ntype.get_compositor_operation = file_ns::get_compositor_operation;

  blender::bke::node_register_type(&ntype);
}

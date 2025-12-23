/* SPDX-FileCopyrightText: 2006 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_assert.h"
#include "BLI_listbase.h"
#include "BLI_math_vector_types.hh"
#include "BLI_memory_utils.hh"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"

#include "BKE_compositor.hh"
#include "BKE_context.hh"
#include "BKE_image.hh"
#include "BKE_lib_id.hh"
#include "BKE_scene.hh"

#include "DNA_layer_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "RE_engine.h"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "GPU_shader.hh"

#include "NOD_node_extra_info.hh"

#include "COM_node_operation.hh"
#include "COM_utilities.hh"

#include "node_composite_util.hh"

namespace blender::nodes::node_composite_render_layer_cc {

static void node_init(const bContext *context, PointerRNA *node_pointer)
{
  Scene *scene = CTX_data_scene(context);
  bNode *node = node_pointer->data_as<bNode>();

  node->id = &scene->id;
  id_us_plus(node->id);
}

/* Default declaration for contextless static declarations, when no scene is assigned, or when the
 * engine has no extra passes. */
static void declare_default(NodeDeclarationBuilder &b)
{
  b.add_output<decl::Color>("Image").structure_type(StructureType::Dynamic);
  b.add_output<decl::Float>("Alpha").structure_type(StructureType::Dynamic);
}

/* Declares an already existing output. */
static BaseSocketDeclarationBuilder &declare_existing_output(NodeDeclarationBuilder &b,
                                                             const bNodeSocket *output)
{
  if (output->type == SOCK_VECTOR) {
    const int dimensions = output->default_value_typed<bNodeSocketValueVector>()->dimensions;
    return b.add_output<decl::Vector>(output->identifier)
        .dimensions(dimensions)
        .structure_type(StructureType::Dynamic);
  }
  return b.add_output(eNodeSocketDatatype(output->type), output->identifier)
      .structure_type(StructureType::Dynamic);
}

/* Declares the already existing outputs. This is done in cases where the scene references an
 * engine that is not registered or a view layer that does not exist. Which gives the user the
 * opportunity to register the engine or update the view layer while maintaining sockets and out
 * going links. */
static void declare_existing(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();
  LISTBASE_FOREACH (const bNodeSocket *, output, &node->outputs) {
    declare_existing_output(b, output);
  }
}

/* Declares an output that matches the type of the given pass. */
static void declare_pass_callback(void *user_data,
                                  Scene * /*scene*/,
                                  ViewLayer * /*view_layer*/,
                                  const char *pass_name,
                                  int channels_count,
                                  const char * /*channel_id*/,
                                  eNodeSocketDatatype socket_type)
{
  NodeDeclarationBuilder &b = *static_cast<NodeDeclarationBuilder *>(user_data);

  /* The combined pass is aliased as Image. */
  const char *name = StringRef(pass_name) == RE_PASSNAME_COMBINED ? "Image" : pass_name;
  if (socket_type == SOCK_VECTOR) {
    b.add_output<decl::Vector>(name)
        .dimensions(channels_count)
        .structure_type(StructureType::Dynamic);
  }
  else {
    b.add_output(socket_type, name).structure_type(StructureType::Dynamic);
  }

  /* The Alpha pass is generated based on the combined pass. */
  if (StringRef(pass_name) == RE_PASSNAME_COMBINED) {
    b.add_output<decl::Float>("Alpha").structure_type(StructureType::Dynamic);
  }
}

static void declare_extra_passes(NodeDeclarationBuilder &b,
                                 const Scene *scene,
                                 const ViewLayer *view_layer)
{
  if ((scene->r.mode & R_EDGE_FRS) &&
      (view_layer->freestyle_config.flags & FREESTYLE_AS_RENDER_PASS))
  {
    b.add_output<decl::Color>(RE_PASSNAME_FREESTYLE).structure_type(StructureType::Dynamic);
  }

  if (view_layer->grease_pencil_flags & GREASE_PENCIL_AS_SEPARATE_PASS) {
    b.add_output<decl::Color>(RE_PASSNAME_GREASE_PENCIL).structure_type(StructureType::Dynamic);
  }
}

/* Declares outputs that are linked and existed in the previous state of the node but no longer
 * exist in the new state. The outputs are set as unavailable, so they are not accessible to the
 * user. This is useful to retain links if the user changed the render engine and thus the passes
 * changed. */
static void declare_old_linked_outputs(NodeDeclarationBuilder &b)
{
  Set<std::string> added_outputs_identifiers;
  for (const SocketDeclaration *output_declaration : b.declaration().sockets(SOCK_OUT)) {
    added_outputs_identifiers.add_new(output_declaration->identifier);
  }
  const bNodeTree *node_tree = b.tree_or_null();
  const bNode *node = b.node_or_null();
  node_tree->ensure_topology_cache();
  for (const bNodeSocket *output : node->output_sockets()) {
    if (added_outputs_identifiers.contains(output->identifier)) {
      continue;
    }
    if (!output->is_directly_linked()) {
      continue;
    }
    declare_existing_output(b, output).available(false);
  }
}

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();
  if (!node) {
    declare_default(b);
    return;
  }

  const bNodeTree *node_tree = b.tree_or_null();
  if (!node_tree) {
    declare_default(b);
    return;
  }

  BLI_SCOPED_DEFER([&]() { declare_old_linked_outputs(b); });

  Scene *scene = reinterpret_cast<Scene *>(node->id);
  if (!scene) {
    declare_default(b);
    return;
  }

  if (!RE_engines_is_registered(scene->r.engine)) {
    declare_existing(b);
    return;
  }

  RenderEngineType *engine_type = RE_engines_find(scene->r.engine);
  if (!engine_type->update_render_passes) {
    declare_default(b);
    return;
  }

  ViewLayer *view_layer = static_cast<ViewLayer *>(
      BLI_findlink(&scene->view_layers, node->custom1));
  if (!view_layer) {
    declare_existing(b);
    return;
  }

  RenderEngine *engine = RE_engine_create(engine_type);
  RE_engine_update_render_passes(engine, scene, view_layer, declare_pass_callback, &b);
  RE_engine_free(engine);

  declare_extra_passes(b, scene, view_layer);
}

static void node_draw(ui::Layout &layout, bContext *context, PointerRNA *node_pointer)
{
  template_id(&layout, context, node_pointer, "scene", nullptr, nullptr, nullptr);

  const bNode *node = node_pointer->data_as<bNode>();
  if (!node->id) {
    return;
  }

  ui::Layout &column = layout.column(false);
  ui::Layout &row = column.row(true);
  row.prop(node_pointer, "layer", ui::ITEM_R_SPLIT_EMPTY_NAME, "", ICON_NONE);

  PropertyRNA *layer_property = RNA_struct_find_property(node_pointer, "layer");
  const char *layer_name;
  if (!RNA_property_enum_identifier(context,
                                    node_pointer,
                                    layer_property,
                                    RNA_property_enum_get(node_pointer, layer_property),
                                    &layer_name))
  {
    return;
  }

  char scene_name[MAX_ID_NAME - 2];
  PointerRNA scene_pointer = RNA_pointer_get(node_pointer, "scene");
  RNA_string_get(&scene_pointer, "name", scene_name);

  PointerRNA render_operator = row.op(
      "RENDER_OT_render", "", ICON_RENDER_STILL, wm::OpCallContext::InvokeDefault, UI_ITEM_NONE);
  RNA_string_set(&render_operator, "layer", layer_name);
  RNA_string_set(&render_operator, "scene", scene_name);
}

static void node_extra_info(NodeExtraInfoParams &parameters)
{
  SpaceNode *space_node = CTX_wm_space_node(&parameters.C);
  if (space_node->node_tree_sub_type != SNODE_COMPOSITOR_SCENE) {
    NodeExtraInfoRow row;
    row.text = RPT_("Node Unsupported");
    row.tooltip = TIP_("The Render Layers node is only supported for scene compositing");
    row.icon = ICON_ERROR;
    parameters.rows.append(std::move(row));
  }

  /* EEVEE supports passes. */
  const Scene *scene = CTX_data_scene(&parameters.C);
  if (StringRef(scene->r.engine) == RE_engine_id_BLENDER_EEVEE) {
    return;
  }

  if (!bke::compositor::is_viewport_compositor_used(parameters.C)) {
    return;
  }

  bool is_any_pass_used = false;
  for (const bNodeSocket *output : parameters.node.output_sockets()) {
    /* Combined pass is always available. */
    if (StringRef(output->name) == "Image" || StringRef(output->name) == "Alpha") {
      continue;
    }
    if (output->is_logically_linked()) {
      is_any_pass_used = true;
      break;
    }
  }

  if (!is_any_pass_used) {
    return;
  }

  NodeExtraInfoRow row;
  row.text = RPT_("Passes Not Supported");
  row.tooltip = TIP_("Render passes in the Viewport compositor are only supported in EEVEE");
  row.icon = ICON_ERROR;
  parameters.rows.append(std::move(row));
}

using namespace blender::compositor;

class RenderLayerOperation : public NodeOperation {
 public:
  using NodeOperation::NodeOperation;

  void execute() override
  {
    const Scene *scene = reinterpret_cast<const Scene *>(this->node().id);
    const int view_layer = this->node().custom1;

    Result &image_result = this->get_result("Image");
    Result &alpha_result = this->get_result("Alpha");

    if (image_result.should_compute() || alpha_result.should_compute()) {
      const Result combined_pass = this->context().get_pass(
          scene, view_layer, RE_PASSNAME_COMBINED);
      if (image_result.should_compute()) {
        this->execute_pass(combined_pass, image_result);
      }
      if (alpha_result.should_compute()) {
        this->execute_pass(combined_pass, alpha_result);
      }
    }

    for (const bNodeSocket *output : this->node().output_sockets()) {
      if (!is_socket_available(output)) {
        continue;
      }

      if (STR_ELEM(output->identifier, "Image", "Alpha")) {
        continue;
      }

      Result &result = this->get_result(output->identifier);
      if (!result.should_compute()) {
        continue;
      }

      const bool is_generated_alpha = StringRef(output->identifier) == "Alpha";
      const char *pass_name = is_generated_alpha ? RE_PASSNAME_COMBINED : output->identifier;
      this->context().populate_meta_data_for_pass(scene, view_layer, pass_name, result.meta_data);

      const Result pass = this->context().get_pass(scene, view_layer, pass_name);
      this->execute_pass(pass, result);
    }
  }

  void execute_pass(const Result &pass, Result &result)
  {
    if (!pass.is_allocated()) {
      /* Pass not rendered yet, or not supported by viewport. */
      result.allocate_invalid();
      return;
    }

    result.set_precision(pass.precision());

    if (this->context().use_gpu()) {
      this->execute_pass_gpu(pass, result);
    }
    else {
      this->execute_pass_cpu(pass, result);
    }
  }

  void execute_pass_gpu(const Result &pass, Result &result)
  {
    gpu::Shader *shader = this->context().get_shader(this->get_shader_name(pass, result),
                                                     result.precision());
    GPU_shader_bind(shader);

    /* The compositing space might be limited to a subset of the pass texture, so only read that
     * compositing region into an appropriately sized result. */
    const int2 lower_bound = this->context().get_input_region().min;
    GPU_shader_uniform_2iv(shader, "lower_bound", lower_bound);

    pass.bind_as_texture(shader, "input_tx");

    result.allocate_texture(this->context().get_compositing_domain());
    result.bind_as_image(shader, "output_img");

    compute_dispatch_threads_at_least(shader, result.domain().data_size);

    GPU_shader_unbind();
    pass.unbind_as_texture();
    result.unbind_as_image();
  }

  const char *get_shader_name(const Result &pass, const Result &result)
  {
    /* Special case for alpha output. */
    if (pass.type() == ResultType::Color && result.type() == ResultType::Float) {
      return "compositor_read_input_alpha";
    }

    switch (pass.type()) {
      case ResultType::Float:
        return "compositor_read_input_float";
      case ResultType::Float3:
      case ResultType::Color:
      case ResultType::Float4:
        return "compositor_read_input_float4";
      case ResultType::Int:
      case ResultType::Int2:
      case ResultType::Float2:
      case ResultType::Bool:
      case ResultType::Menu:
        /* Not supported. */
        break;
      case ResultType::String:
        /* Single only types do not support GPU code path. */
        BLI_assert(Result::is_single_value_only_type(pass.type()));
        BLI_assert_unreachable();
        break;
    }

    BLI_assert_unreachable();
    return nullptr;
  }

  void execute_pass_cpu(const Result &pass, Result &result)
  {
    /* The compositing space might be limited to a subset of the pass texture, so only read that
     * compositing region into an appropriately sized result. */
    const int2 lower_bound = this->context().get_input_region().min;

    result.allocate_texture(this->context().get_compositing_domain());

    if (pass.type() == ResultType::Color && result.type() == ResultType::Float) {
      /* Special case for alpha output. */
      parallel_for(result.domain().data_size, [&](const int2 texel) {
        result.store_pixel(texel, pass.load_pixel<Color>(texel + lower_bound).a);
      });
    }
    else if (pass.type() == ResultType::Float3 && result.type() == ResultType::Color) {
      /* Color passes with no alpha could be stored in a Float3 type. */
      parallel_for(result.domain().data_size, [&](const int2 texel) {
        result.store_pixel(texel,
                           Color(float4(pass.load_pixel<float3>(texel + lower_bound), 1.0f)));
      });
    }
    else {
      pass.get_cpp_type().to_static_type_tag<float, float3, float4, Color>([&](auto type_tag) {
        using T = typename decltype(type_tag)::type;
        if constexpr (std::is_same_v<T, void>) {
          /* Unsupported type. */
          BLI_assert_unreachable();
        }
        else {
          parallel_for(result.domain().data_size, [&](const int2 texel) {
            result.store_pixel(texel, pass.load_pixel<T>(texel + lower_bound));
          });
        }
      });
    }
  }
};

static NodeOperation *get_compositor_operation(Context &context, DNode node)
{
  return new RenderLayerOperation(context, node);
}

static void register_node()
{
  static blender::bke::bNodeType ntype;

  cmp_node_type_base(&ntype, "CompositorNodeRLayers", CMP_NODE_R_LAYERS);
  ntype.ui_name = "Render Layers";
  ntype.ui_description = "Input render passes from a scene render";
  ntype.enum_name_legacy = "R_LAYERS";
  ntype.nclass = NODE_CLASS_INPUT;
  ntype.flag |= NODE_PREVIEW;
  ntype.initfunc_api = node_init;
  ntype.declare = node_declare;
  ntype.draw_buttons = node_draw;
  ntype.get_compositor_operation = get_compositor_operation;
  ntype.get_extra_info = node_extra_info;
  blender::bke::node_type_size_preset(ntype, blender::bke::eNodeSizePreset::Large);

  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(register_node)

}  // namespace blender::nodes::node_composite_render_layer_cc

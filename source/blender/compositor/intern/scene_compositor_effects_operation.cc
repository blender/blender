/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <memory>

#include "BLI_listbase.hh"
#include "BLI_math_euler.hh"

#include "DNA_scene_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "BKE_compositor.hh"
#include "BKE_compute_contexts.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "COM_node_group_operation.hh"
#include "COM_scene_compositor_effects_operation.hh"
#include "COM_scheduler.hh"
#include "COM_utilities.hh"

namespace blender::compositor {

SceneCompositorEffectsOperation::SceneCompositorEffectsOperation(Context &context)
    : SimpleOperation(context)
{
  this->declare_input_descriptor(InputDescriptor{ResultType::Color});
  this->populate_result(ResultType::Color);
}

static bke::compositor::ExecutionMode get_execution_mode(const Context &context)
{
  if (context.render_context()) {
    return bke::compositor::ExecutionMode::Render;
  }
  return bke::compositor::ExecutionMode::Preview;
}

static Result *get_effect_input(Context &context,
                                PointerRNA &effect_inputs_ptr,
                                const bNodeTreeInterfaceSocket &input_socket)
{
  PointerRNA input_ptr = RNA_pointer_get(&effect_inputs_ptr, input_socket.identifier);
  compositor::Result *result = new compositor::Result(
      context.create_result(get_node_interface_socket_result_type(input_socket)));
  result->allocate_single_value();
  switch (input_socket.socket_typeinfo()->type) {
    case SOCK_FLOAT: {
      const float value = RNA_float_get(&input_ptr, "value");
      result->set_single_value(value);
      break;
    }
    case SOCK_VECTOR: {
      switch (static_cast<bNodeSocketValueVector *>(input_socket.socket_data)->dimensions) {
        case 2: {
          float2 value;
          RNA_float_get_array(&input_ptr, "value", value);
          result->set_single_value(value);
          break;
        }
        case 3: {
          float3 value;
          RNA_float_get_array(&input_ptr, "value", value);
          result->set_single_value(value);
          break;
        }
        case 4: {
          float4 value;
          RNA_float_get_array(&input_ptr, "value", value);
          result->set_single_value(value);
          break;
        }
        default:
          BLI_assert_unreachable();
      }
      break;
    }
    case SOCK_RGBA: {
      ColorGeometry4f value;
      RNA_float_get_array(&input_ptr, "value", value);
      result->set_single_value(value);
      break;
    }
    case SOCK_BOOLEAN: {
      const bool value = RNA_boolean_get(&input_ptr, "value");
      result->set_single_value(value);
      break;
    }
    case SOCK_INT: {
      const int value = RNA_int_get(&input_ptr, "value");
      result->set_single_value(value);
      break;
    }
    case SOCK_ROTATION: {
      float3 value_euler;
      RNA_float_get_array(&input_ptr, "value", value_euler);
      math::Quaternion value_rotation = math::to_quaternion(math::EulerXYZ(value_euler));
      result->set_single_value(value_rotation);
      break;
    }
    case SOCK_MENU: {
      const nodes::MenuValue value = nodes::MenuValue(RNA_enum_get(&input_ptr, "value"));
      result->set_single_value(value);
      break;
    }
    case SOCK_STRING: {
      const std::string value = RNA_string_get(&input_ptr, "value");
      result->set_single_value(value);
      break;
    }
    case SOCK_INT_VECTOR: {
      switch (static_cast<bNodeSocketValueIntVector *>(input_socket.socket_data)->dimensions) {
        case 2: {
          int2 value;
          RNA_int_get_array(&input_ptr, "value", value);
          result->set_single_value(value);
          break;
        }
        case 3: {
          int3 value;
          RNA_int_get_array(&input_ptr, "value", value);
          result->set_single_value(value);
          break;
        }
        default:
          BLI_assert_unreachable();
      }
      break;
    }
    case SOCK_OBJECT: {
      Object *value = RNA_pointer_get(&input_ptr, "value").data_as<Object>();
      result->set_single_value(value);
      break;
    }
    case SOCK_FONT: {
      VFont *value = RNA_pointer_get(&input_ptr, "value").data_as<VFont>();
      result->set_single_value(value);
      break;
    }
    case SOCK_IMAGE:
    case SOCK_COLLECTION:
    case SOCK_TEXTURE:
    case SOCK_MATERIAL:
    case SOCK_SCENE:
    case SOCK_TEXT_ID:
    case SOCK_MASK:
    case SOCK_SOUND:
    case SOCK_GEOMETRY:
    case SOCK_MATRIX:
    case SOCK_BUNDLE:
    case SOCK_CLOSURE:
    case SOCK_SHADER:
    case SOCK_CUSTOM:
      break;
  }

  return result;
}

void SceneCompositorEffectsOperation::execute()
{
  const Scene &scene = this->context().get_scene();
  const bool needs_viewer_output = flag_is_set(this->context().needed_side_effect_output_types(),
                                               SideEffectOutputTypes::ViewerNode);
  const bke::DataBlockComputeContext scene_compute_context(nullptr, scene.id);

  std::unique_ptr<NodeGroupOperation> last_operation;
  const bke::compositor::ExecutionMode execution_mode = get_execution_mode(this->context());
  for (const SceneCompositorEffect &effect : scene.compositor_effects) {
    if (this->context().is_canceled()) {
      break;
    }

    if (!bke::compositor::is_effect_enabled(effect, execution_mode)) {
      continue;
    }

    const bke::SceneCompositorEffectComputeContext effect_compute_context(&scene_compute_context,
                                                                          effect);

    const bNodeTree &node_group = *effect.node_group;
    NodeGroupOperation *effect_operation = new NodeGroupOperation(
        this->context(), node_group, effect_compute_context);

    /* If the node group has no viewer node in the active context, and the context requires a
     * viewer output, we use the group output as a viewer. */
    if (needs_viewer_output && has_viewer_node(node_group,
                                               effect_compute_context,
                                               this->context().get_active_compute_context_hash()))
    {
      has_viewer_output_ = true;
    }

    /* We need the output of the effect if we are rendering or do not have a viewer, in which
     * case, the viewer will be in a later effect which needs the output of this one, or the
     * viewer result will be the last operation. */
    const bool is_effect_output_needed = this->context().render_context() || !has_viewer_output_;

    /* Set the reference count for the outputs, only the first color output is actually needed,
     * while the rest are ignored. */
    node_group.ensure_interface_cache();
    for (const bNodeTreeInterfaceSocket *output_socket : node_group.interface_outputs()) {
      const bool is_first_output = output_socket == node_group.interface_outputs().first();
      Result &output_result = effect_operation->get_result(output_socket->identifier);
      const bool is_color = output_result.type() == ResultType::Color;
      const bool is_needed = is_effect_output_needed && is_first_output && is_color;
      output_result.set_reference_count(is_needed ? 1 : 0);
    }

    PointerRNA effect_ptr = RNA_pointer_create_discrete(
        const_cast<ID *>(&scene.id),
        RNA_SceneCompositorEffect,
        const_cast<SceneCompositorEffect *>(&effect));
    PointerRNA effect_properties_ptr = RNA_pointer_get(&effect_ptr, "properties");
    PointerRNA effect_inputs_ptr = RNA_pointer_get(&effect_properties_ptr, "inputs");

    /* Map the inputs to the operation. */
    Vector<std::unique_ptr<Result>> temporary_inputs;
    for (const bNodeTreeInterfaceSocket *input_socket : node_group.interface_inputs()) {
      if (input_socket != node_group.interface_inputs().first()) {
        Result *input_result = get_effect_input(this->context(), effect_inputs_ptr, *input_socket);
        effect_operation->map_input_to_result(input_socket->identifier, input_result);
        temporary_inputs.append(std::unique_ptr<Result>(input_result));
        continue;
      }

      /* If a last operation exists, link its output. */
      if (last_operation) {
        /* Last operation had no output, so link an invalid result. */
        if (last_operation->node_group().interface_outputs().is_empty()) {
          Result *invalid_result = new Result(this->context().create_result(ResultType::Color));
          invalid_result->allocate_invalid();
          effect_operation->map_input_to_result(input_socket->identifier, invalid_result);
          temporary_inputs.append(std::unique_ptr<Result>(invalid_result));
          continue;
        }
        const bNodeTreeInterfaceSocket *last_operation_output =
            last_operation->node_group().interface_outputs().first();
        Result &output_result = last_operation->get_result(last_operation_output->identifier);
        effect_operation->map_input_to_result(input_socket->identifier, &output_result);
        continue;
      }

      /* Otherwise, we link the input of the operation. */
      Result &base_input = this->get_input();
      Result *operation_input = new Result(this->context().create_result(base_input.type()));
      operation_input->share_data(base_input);
      effect_operation->map_input_to_result(input_socket->identifier, operation_input);
      temporary_inputs.append(std::unique_ptr<Result>(operation_input));
    }

    effect_operation->evaluate();

    last_operation.reset(effect_operation);

    /* If the effect output is not needed, then we needn't compute later effects. */
    if (!is_effect_output_needed) {
      break;
    }
  }

  /* If no last operation exist or if it has no outputs, then there is nothing to do. The last
   * operation can be nullptr because execution may have been canceled before any operation was
   * evaluated. */
  if (!last_operation || last_operation->node_group().interface_outputs().is_empty()) {
    this->allocate_default_remaining_outputs();
    return;
  }

  /* Execution was canceled, so there is nothing to do, but we make sure to free any results. */
  if (this->context().is_canceled()) {
    last_operation->free_results();
    this->allocate_default_remaining_outputs();
    return;
  }

  /* The output is not actually needed, so default allocate the operation output. */
  Result &output_result = last_operation->get_result(
      last_operation->node_group().interface_outputs().first()->identifier);
  if (!output_result.should_compute()) {
    this->allocate_default_remaining_outputs();
    return;
  }

  /* Share the first output of the last operation with the operation result. */
  this->get_result().share_data(output_result);
  output_result.release();
  has_output_ = true;
}

}  // namespace blender::compositor

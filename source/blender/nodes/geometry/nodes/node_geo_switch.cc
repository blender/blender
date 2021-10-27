/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "node_geometry_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_material.h"

#include "FN_multi_function_signature.hh"

namespace blender::nodes {

static void geo_node_switch_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Bool>("Switch").default_value(false).supports_field();
  b.add_input<decl::Bool>("Switch", "Switch_001").default_value(false);

  b.add_input<decl::Float>("False").supports_field();
  b.add_input<decl::Float>("True").supports_field();
  b.add_input<decl::Int>("False", "False_001").min(-100000).max(100000).supports_field();
  b.add_input<decl::Int>("True", "True_001").min(-100000).max(100000).supports_field();
  b.add_input<decl::Bool>("False", "False_002").default_value(false).hide_value().supports_field();
  b.add_input<decl::Bool>("True", "True_002").default_value(true).hide_value().supports_field();
  b.add_input<decl::Vector>("False", "False_003").supports_field();
  b.add_input<decl::Vector>("True", "True_003").supports_field();
  b.add_input<decl::Color>("False", "False_004")
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .supports_field();
  b.add_input<decl::Color>("True", "True_004")
      .default_value({0.8f, 0.8f, 0.8f, 1.0f})
      .supports_field();
  b.add_input<decl::String>("False", "False_005").supports_field();
  b.add_input<decl::String>("True", "True_005").supports_field();

  b.add_input<decl::Geometry>("False", "False_006");
  b.add_input<decl::Geometry>("True", "True_006");
  b.add_input<decl::Object>("False", "False_007");
  b.add_input<decl::Object>("True", "True_007");
  b.add_input<decl::Collection>("False", "False_008");
  b.add_input<decl::Collection>("True", "True_008");
  b.add_input<decl::Texture>("False", "False_009");
  b.add_input<decl::Texture>("True", "True_009");
  b.add_input<decl::Material>("False", "False_010");
  b.add_input<decl::Material>("True", "True_010");
  b.add_input<decl::Image>("False", "False_011");
  b.add_input<decl::Image>("True", "True_011");

  b.add_output<decl::Float>("Output").dependent_field();
  b.add_output<decl::Int>("Output", "Output_001").dependent_field();
  b.add_output<decl::Bool>("Output", "Output_002").dependent_field();
  b.add_output<decl::Vector>("Output", "Output_003").dependent_field();
  b.add_output<decl::Color>("Output", "Output_004").dependent_field();
  b.add_output<decl::String>("Output", "Output_005").dependent_field();
  b.add_output<decl::Geometry>("Output", "Output_006");
  b.add_output<decl::Object>("Output", "Output_007");
  b.add_output<decl::Collection>("Output", "Output_008");
  b.add_output<decl::Texture>("Output", "Output_009");
  b.add_output<decl::Material>("Output", "Output_010");
  b.add_output<decl::Image>("Output", "Output_011");
}

static void geo_node_switch_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "input_type", 0, "", ICON_NONE);
}

static void geo_node_switch_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeSwitch *data = (NodeSwitch *)MEM_callocN(sizeof(NodeSwitch), __func__);
  data->input_type = SOCK_GEOMETRY;
  node->storage = data;
}

static void geo_node_switch_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeSwitch *node_storage = (NodeSwitch *)node->storage;
  int index = 0;
  bNodeSocket *field_switch = (bNodeSocket *)node->inputs.first;
  bNodeSocket *non_field_switch = (bNodeSocket *)field_switch->next;

  const bool fields_type = ELEM((eNodeSocketDatatype)node_storage->input_type,
                                SOCK_FLOAT,
                                SOCK_INT,
                                SOCK_BOOLEAN,
                                SOCK_VECTOR,
                                SOCK_RGBA,
                                SOCK_STRING);

  nodeSetSocketAvailability(field_switch, fields_type);
  nodeSetSocketAvailability(non_field_switch, !fields_type);

  LISTBASE_FOREACH_INDEX (bNodeSocket *, socket, &node->inputs, index) {
    if (index <= 1) {
      continue;
    }
    nodeSetSocketAvailability(socket,
                              socket->type == (eNodeSocketDatatype)node_storage->input_type);
  }

  LISTBASE_FOREACH (bNodeSocket *, socket, &node->outputs) {
    nodeSetSocketAvailability(socket,
                              socket->type == (eNodeSocketDatatype)node_storage->input_type);
  }
}

template<typename T> class SwitchFieldsFunction : public fn::MultiFunction {
 public:
  SwitchFieldsFunction()
  {
    static fn::MFSignature signature = create_signature();
    this->set_signature(&signature);
  }
  static fn::MFSignature create_signature()
  {
    fn::MFSignatureBuilder signature{"Switch"};
    signature.single_input<bool>("Switch");
    signature.single_input<T>("False");
    signature.single_input<T>("True");
    signature.single_output<T>("Output");
    return signature.build();
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    const VArray<bool> &switches = params.readonly_single_input<bool>(0, "Switch");
    const VArray<T> &falses = params.readonly_single_input<T>(1, "False");
    const VArray<T> &trues = params.readonly_single_input<T>(2, "True");
    MutableSpan<T> values = params.uninitialized_single_output_if_required<T>(3, "Output");
    for (int64_t i : mask) {
      new (&values[i]) T(switches[i] ? trues[i] : falses[i]);
    }
  }
};

template<typename T> void switch_fields(GeoNodeExecParams &params, const StringRef suffix)
{
  if (params.lazy_require_input("Switch")) {
    return;
  }

  const std::string name_false = "False" + suffix;
  const std::string name_true = "True" + suffix;
  const std::string name_output = "Output" + suffix;

  Field<bool> switches_field = params.get_input<Field<bool>>("Switch");
  if (switches_field.node().depends_on_input()) {
    /* The switch has to be incorporated into the field. Both inputs have to be evaluated. */
    const bool require_false = params.lazy_require_input(name_false);
    const bool require_true = params.lazy_require_input(name_true);
    if (require_false | require_true) {
      return;
    }

    Field<T> falses_field = params.extract_input<Field<T>>(name_false);
    Field<T> trues_field = params.extract_input<Field<T>>(name_true);

    auto switch_fn = std::make_unique<SwitchFieldsFunction<T>>();
    auto switch_op = std::make_shared<FieldOperation>(FieldOperation(
        std::move(switch_fn),
        {std::move(switches_field), std::move(falses_field), std::move(trues_field)}));

    params.set_output(name_output, Field<T>(switch_op, 0));
  }
  else {
    /* The switch input is constant, so just evaluate and forward one of the inputs. */
    const bool switch_value = fn::evaluate_constant_field(switches_field);
    if (switch_value) {
      params.set_input_unused(name_false);
      if (params.lazy_require_input(name_true)) {
        return;
      }
      params.set_output(name_output, params.extract_input<Field<T>>(name_true));
    }
    else {
      params.set_input_unused(name_true);
      if (params.lazy_require_input(name_false)) {
        return;
      }
      params.set_output(name_output, params.extract_input<Field<T>>(name_false));
    }
  }
}

template<typename T> void switch_no_fields(GeoNodeExecParams &params, const StringRef suffix)
{
  if (params.lazy_require_input("Switch_001")) {
    return;
  }
  bool switch_value = params.get_input<bool>("Switch_001");

  const std::string name_false = "False" + suffix;
  const std::string name_true = "True" + suffix;
  const std::string name_output = "Output" + suffix;

  if (switch_value) {
    params.set_input_unused(name_false);
    if (params.lazy_require_input(name_true)) {
      return;
    }
    params.set_output(name_output, params.extract_input<T>(name_true));
  }
  else {
    params.set_input_unused(name_true);
    if (params.lazy_require_input(name_false)) {
      return;
    }
    params.set_output(name_output, params.extract_input<T>(name_false));
  }
}

static void geo_node_switch_exec(GeoNodeExecParams params)
{
  const NodeSwitch &storage = *(const NodeSwitch *)params.node().storage;
  const eNodeSocketDatatype data_type = static_cast<eNodeSocketDatatype>(storage.input_type);

  switch (data_type) {

    case SOCK_FLOAT: {
      switch_fields<float>(params, "");
      break;
    }
    case SOCK_INT: {
      switch_fields<int>(params, "_001");
      break;
    }
    case SOCK_BOOLEAN: {
      switch_fields<bool>(params, "_002");
      break;
    }
    case SOCK_VECTOR: {
      switch_fields<float3>(params, "_003");
      break;
    }
    case SOCK_RGBA: {
      switch_fields<ColorGeometry4f>(params, "_004");
      break;
    }
    case SOCK_STRING: {
      switch_fields<std::string>(params, "_005");
      break;
    }
    case SOCK_GEOMETRY: {
      switch_no_fields<GeometrySet>(params, "_006");
      break;
    }
    case SOCK_OBJECT: {
      switch_no_fields<Object *>(params, "_007");
      break;
    }
    case SOCK_COLLECTION: {
      switch_no_fields<Collection *>(params, "_008");
      break;
    }
    case SOCK_TEXTURE: {
      switch_no_fields<Tex *>(params, "_009");
      break;
    }
    case SOCK_MATERIAL: {
      switch_no_fields<Material *>(params, "_010");
      break;
    }
    case SOCK_IMAGE: {
      switch_no_fields<Image *>(params, "_011");
      break;
    }
    default:
      BLI_assert_unreachable();
      break;
  }
}

}  // namespace blender::nodes

void register_node_type_geo_switch()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SWITCH, "Switch", NODE_CLASS_CONVERTER, 0);
  ntype.declare = blender::nodes::geo_node_switch_declare;
  node_type_init(&ntype, blender::nodes::geo_node_switch_init);
  node_type_update(&ntype, blender::nodes::geo_node_switch_update);
  node_type_storage(&ntype, "NodeSwitch", node_free_standard_storage, node_copy_standard_storage);
  ntype.geometry_node_execute = blender::nodes::geo_node_switch_exec;
  ntype.geometry_node_execute_supports_laziness = true;
  ntype.draw_buttons = blender::nodes::geo_node_switch_layout;
  nodeRegisterType(&ntype);
}

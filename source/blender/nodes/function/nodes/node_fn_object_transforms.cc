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

#include "node_function_util.hh"

#include "BKE_persistent_data_handle.hh"

static bNodeSocketTemplate fn_node_object_transforms_in[] = {
    {SOCK_OBJECT, N_("Object")},
    {-1, ""},
};

static bNodeSocketTemplate fn_node_object_transforms_out[] = {
    {SOCK_VECTOR, N_("Location")},
    {-1, ""},
};

class ObjectTransformsFunction : public blender::fn::MultiFunction {
 public:
  ObjectTransformsFunction()
  {
    blender::fn::MFSignatureBuilder signature = this->get_builder("Object Transforms");
    signature.depends_on_context();
    signature.single_input<blender::bke::PersistentObjectHandle>("Object");
    signature.single_output<blender::float3>("Location");
  }

  void call(blender::IndexMask mask,
            blender::fn::MFParams params,
            blender::fn::MFContext context) const override
  {
    blender::fn::VSpan handles =
        params.readonly_single_input<blender::bke::PersistentObjectHandle>(0, "Object");
    blender::MutableSpan locations = params.uninitialized_single_output<blender::float3>(
        1, "Location");

    const blender::bke::PersistentDataHandleMap *handle_map =
        context.get_global_context<blender::bke::PersistentDataHandleMap>(
            "PersistentDataHandleMap");
    if (handle_map == nullptr) {
      locations.fill_indices(mask, {0, 0, 0});
      return;
    }

    for (int64_t i : mask) {
      blender::bke::PersistentObjectHandle handle = handles[i];
      const Object *object = handle_map->lookup(handle);
      blender::float3 location;
      if (object == nullptr) {
        location = {0, 0, 0};
      }
      else {
        location = object->loc;
      }
      locations[i] = location;
    }
  }
};

static void fn_node_object_transforms_expand_in_mf_network(
    blender::nodes::NodeMFNetworkBuilder &builder)
{
  static ObjectTransformsFunction fn;
  builder.set_matching_fn(fn);
}

void register_node_type_fn_object_transforms()
{
  static bNodeType ntype;

  fn_node_type_base(&ntype, FN_NODE_OBJECT_TRANSFORMS, "Object Transforms", 0, 0);
  node_type_socket_templates(&ntype, fn_node_object_transforms_in, fn_node_object_transforms_out);
  ntype.expand_in_mf_network = fn_node_object_transforms_expand_in_mf_network;
  nodeRegisterType(&ntype);
}

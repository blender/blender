/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_bvhutils.h"
#include "BKE_mesh_sample.hh"
#include "BKE_rigidbody.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "NOD_particles.h"
#include "NOD_geometry_exec.hh"
#include "NOD_socket_search_link.hh"

#include "node_particles_util.hh"

namespace blender::nodes::node_particles_set_shape_cc {

NODE_STORAGE_FUNCS(NodeParticlesSetShape)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Particles"));
  b.add_input<decl::Object>(N_("Object"));

  b.add_output<decl::Geometry>(N_("Particles"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "shape_type", 0, "", ICON_NONE);
}

static void node_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeParticlesSetShape *data = MEM_cnew<NodeParticlesSetShape>(__func__);
  data->shape_type = PARTICLE_SHAPE_SPHERE;
  node->storage = data;
}

static void try_capture_field_on_geometry(GeometryComponent &component,
                                          const StringRef name,
                                          const eAttrDomain domain,
                                          const GField &field)
{
  GeometryComponentFieldContext field_context{component, domain};
  const int domain_num = component.attribute_domain_num(domain);
  const IndexMask mask{IndexMask(domain_num)};

  const CPPType &type = field.cpp_type();
  const eCustomDataType data_type = bke::cpp_type_to_custom_data_type(type);

  /* Could avoid allocating a new buffer if:
   * - We are writing to an attribute that exists already.
   * - The field does not depend on that attribute (we can't easily check for that yet). */
  void *buffer = MEM_mallocN(type.size() * domain_num, __func__);

  fn::FieldEvaluator evaluator{field_context, &mask};
  evaluator.add_with_destination(field, GMutableSpan{type, buffer, domain_num});
  evaluator.evaluate();

  component.attribute_try_delete(name);
  if (component.attribute_exists(name)) {
    WriteAttributeLookup write_attribute = component.attribute_try_get_for_write(name);
    if (write_attribute && write_attribute.domain == domain &&
        write_attribute.varray.type() == type) {
      write_attribute.varray.set_all(buffer);
      write_attribute.tag_modified_fn();
    }
    else {
      /* Cannot change type of built-in attribute. */
    }
    type.destruct_n(buffer, domain_num);
    MEM_freeN(buffer);
  }
  else {
    component.attribute_try_create(name, domain, data_type, AttributeInitMove{buffer});
  }
}

static void node_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Particles");

  params.used_named_attribute(particles::shape_index_attribute_name, eNamedAttrUsage::Write);

  const NodeParticlesSetShape &storage = node_storage(params.node());
  const ParticleNodeShapeType shape_type = static_cast<ParticleNodeShapeType>(storage.shape_type);

  const int shape_index = 12345;
  Field<int> shape_index_field{std::make_shared<fn::FieldConstant>(CPPType::get<int>(), &shape_index)};

  /* Run on the instances component separately to only affect the top level of instances. */
  if (geometry_set.has_instances()) {
    GeometryComponent &component = geometry_set.get_component_for_write(
        GEO_COMPONENT_TYPE_INSTANCES);
    try_capture_field_on_geometry(
        component, particles::shape_index_attribute_name, ATTR_DOMAIN_INSTANCE, shape_index_field);
  }
  else {
    geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
      for (const GeometryComponentType type :
           {GEO_COMPONENT_TYPE_MESH, GEO_COMPONENT_TYPE_POINT_CLOUD}) {
        if (geometry_set.has(type)) {
          GeometryComponent &component = geometry_set.get_component_for_write(type);
          try_capture_field_on_geometry(component,
                                        particles::shape_index_attribute_name,
                                        ATTR_DOMAIN_POINT,
                                        shape_index_field);
        }
      }
    });
  }

  params.set_output("Particles", std::move(geometry_set));
}

}  // namespace blender::nodes::node_particles_set_shape_cc

void register_node_type_particles_add_shape()
{
  namespace file_ns = blender::nodes::node_particles_set_shape_cc;

  static bNodeType ntype;

  particle_node_type_base(&ntype, PARTICLE_NODE_SET_SHAPE, "Rigid Body Physics", NODE_CLASS_GEOMETRY);
  node_type_init(&ntype, file_ns::node_init);
  node_type_storage(
      &ntype, "NodeParticlesSetShape", node_free_standard_storage, node_copy_standard_storage);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_exec;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}

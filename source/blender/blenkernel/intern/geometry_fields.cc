/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"

#include "BKE_attribute.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_fields.hh"
#include "BKE_geometry_set.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_mesh.hh"

#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"

#include "BLT_translation.hh"

#include <fmt/format.h>

namespace blender::bke {

MeshFieldContext::MeshFieldContext(const Mesh &mesh, const AttrDomain domain)
    : mesh_(mesh), domain_(domain)
{
  BLI_assert(mesh.attributes().domain_supported(domain_));
}

CurvesFieldContext::CurvesFieldContext(const CurvesGeometry &curves, const AttrDomain domain)
    : curves_(curves), domain_(domain)
{
  BLI_assert(curves.attributes().domain_supported(domain));
}

CurvesFieldContext::CurvesFieldContext(const Curves &curves_id, const AttrDomain domain)
    : CurvesFieldContext(curves_id.geometry.wrap(), domain)
{
  curves_id_ = &curves_id;
}

GVArray GreasePencilLayerFieldContext::get_varray_for_input(const fn::FieldInput &field_input,
                                                            const IndexMask &mask,
                                                            ResourceScope &scope) const
{
  if (const CurvesFieldInput *curves_field_input = dynamic_cast<const CurvesFieldInput *>(
          &field_input))
  {
    if (const bke::greasepencil::Drawing *drawing = this->grease_pencil().get_eval_drawing(
            this->grease_pencil().layer(this->layer_index())))
    {
      if (drawing->strokes().attributes().domain_supported(this->domain())) {
        const CurvesFieldContext context{drawing->strokes(), this->domain()};
        return curves_field_input->get_varray_for_context(context, mask, scope);
      }
    }
    return {};
  }
  return field_input.get_varray_for_context(*this, mask, scope);
}

GeometryFieldContext::GeometryFieldContext(const GeometryFieldContext &other,
                                           const AttrDomain domain)
    : geometry_(other.geometry_),
      type_(other.type_),
      domain_(domain),
      curves_id_(other.curves_id_),
      grease_pencil_layer_index_(other.grease_pencil_layer_index_)
{
}

GeometryFieldContext::GeometryFieldContext(const void *geometry,
                                           const GeometryComponent::Type type,
                                           const AttrDomain domain,
                                           const int grease_pencil_layer_index)
    : geometry_(geometry),
      type_(type),
      domain_(domain),
      grease_pencil_layer_index_(grease_pencil_layer_index)
{
  BLI_assert(ELEM(type,
                  GeometryComponent::Type::Mesh,
                  GeometryComponent::Type::Curve,
                  GeometryComponent::Type::PointCloud,
                  GeometryComponent::Type::GreasePencil,
                  GeometryComponent::Type::Instance));
}

GeometryFieldContext::GeometryFieldContext(const GeometryComponent &component,
                                           const AttrDomain domain)
    : type_(component.type()), domain_(domain)
{
  switch (component.type()) {
    case GeometryComponent::Type::Mesh: {
      const MeshComponent &mesh_component = static_cast<const MeshComponent &>(component);
      geometry_ = mesh_component.get();
      break;
    }
    case GeometryComponent::Type::Curve: {
      const CurveComponent &curve_component = static_cast<const CurveComponent &>(component);
      const Curves *curves = curve_component.get();
      geometry_ = curves ? &curves->geometry.wrap() : nullptr;
      curves_id_ = curve_component.get();
      break;
    }
    case GeometryComponent::Type::PointCloud: {
      const PointCloudComponent &pointcloud_component = static_cast<const PointCloudComponent &>(
          component);
      geometry_ = pointcloud_component.get();
      break;
    }
    case GeometryComponent::Type::GreasePencil: {
      const GreasePencilComponent &grease_pencil_component =
          static_cast<const GreasePencilComponent &>(component);
      geometry_ = grease_pencil_component.get();
      /* Need to use another constructor for other domains. */
      BLI_assert(domain == AttrDomain::Layer);
      break;
    }
    case GeometryComponent::Type::Instance: {
      const InstancesComponent &instances_component = static_cast<const InstancesComponent &>(
          component);
      geometry_ = instances_component.get();
      break;
    }
    case GeometryComponent::Type::Volume:
    case GeometryComponent::Type::Edit:
      BLI_assert_unreachable();
      break;
  }
}

GeometryFieldContext::GeometryFieldContext(const Mesh &mesh, AttrDomain domain)
    : geometry_(&mesh), type_(GeometryComponent::Type::Mesh), domain_(domain)
{
}
GeometryFieldContext::GeometryFieldContext(const CurvesGeometry &curves, AttrDomain domain)
    : geometry_(&curves), type_(GeometryComponent::Type::Curve), domain_(domain)
{
}
GeometryFieldContext::GeometryFieldContext(const Curves &curves_id, AttrDomain domain)
    : geometry_(&curves_id.geometry.wrap()),
      type_(GeometryComponent::Type::Curve),
      domain_(domain),
      curves_id_(&curves_id)
{
}
GeometryFieldContext::GeometryFieldContext(const PointCloud &points)
    : geometry_(&points), type_(GeometryComponent::Type::PointCloud), domain_(AttrDomain::Point)
{
}
GeometryFieldContext::GeometryFieldContext(const GreasePencil &grease_pencil)
    : geometry_(&grease_pencil),
      type_(GeometryComponent::Type::GreasePencil),
      domain_(AttrDomain::Layer)
{
}
GeometryFieldContext::GeometryFieldContext(const GreasePencil &grease_pencil,
                                           const AttrDomain domain,
                                           const int layer_index)
    : geometry_(&grease_pencil),
      type_(GeometryComponent::Type::GreasePencil),
      domain_(domain),
      grease_pencil_layer_index_(layer_index)
{
}
GeometryFieldContext::GeometryFieldContext(const Instances &instances)
    : geometry_(&instances),
      type_(GeometryComponent::Type::Instance),
      domain_(AttrDomain::Instance)
{
}

std::optional<AttributeAccessor> GeometryFieldContext::attributes() const
{
  if (const Mesh *mesh = this->mesh()) {
    return mesh->attributes();
  }
  if (const CurvesGeometry *curves = this->curves()) {
    return curves->attributes();
  }
  if (const PointCloud *pointcloud = this->pointcloud()) {
    return pointcloud->attributes();
  }
  if (const GreasePencil *grease_pencil = this->grease_pencil()) {
    if (domain_ == AttrDomain::Layer) {
      return grease_pencil->attributes();
    }
    if (const greasepencil::Drawing *drawing = grease_pencil->get_eval_drawing(
            grease_pencil->layer(grease_pencil_layer_index_)))
    {
      return drawing->strokes().attributes();
    }
  }
  if (const Instances *instances = this->instances()) {
    return instances->attributes();
  }
  return {};
}

const Mesh *GeometryFieldContext::mesh() const
{
  return this->type() == GeometryComponent::Type::Mesh ? static_cast<const Mesh *>(geometry_) :
                                                         nullptr;
}
const CurvesGeometry *GeometryFieldContext::curves() const
{
  return this->type() == GeometryComponent::Type::Curve ?
             static_cast<const CurvesGeometry *>(geometry_) :
             nullptr;
}
const PointCloud *GeometryFieldContext::pointcloud() const
{
  return this->type() == GeometryComponent::Type::PointCloud ?
             static_cast<const PointCloud *>(geometry_) :
             nullptr;
}
const GreasePencil *GeometryFieldContext::grease_pencil() const
{
  return this->type() == GeometryComponent::Type::GreasePencil ?
             static_cast<const GreasePencil *>(geometry_) :
             nullptr;
}
const greasepencil::Drawing *GeometryFieldContext::grease_pencil_layer_drawing() const
{
  if (!(this->type() == GeometryComponent::Type::GreasePencil) ||
      !ELEM(domain_, AttrDomain::Curve, AttrDomain::Point))
  {
    return nullptr;
  }
  return this->grease_pencil()->get_eval_drawing(
      this->grease_pencil()->layer(this->grease_pencil_layer_index_));
}
const CurvesGeometry *GeometryFieldContext::curves_or_strokes() const
{
  if (const CurvesGeometry *curves = this->curves()) {
    return curves;
  }
  if (const greasepencil::Drawing *drawing = this->grease_pencil_layer_drawing()) {
    return &drawing->strokes();
  }
  return nullptr;
}
const Curves *GeometryFieldContext::curves_id() const
{
  return curves_id_;
}
const Instances *GeometryFieldContext::instances() const
{
  return this->type() == GeometryComponent::Type::Instance ?
             static_cast<const Instances *>(geometry_) :
             nullptr;
}

GVArray GeometryFieldInput::get_varray_for_context(const fn::FieldContext &context,
                                                   const IndexMask &mask,
                                                   ResourceScope & /*scope*/) const
{
  if (const GeometryFieldContext *geometry_context = dynamic_cast<const GeometryFieldContext *>(
          &context))
  {
    return this->get_varray_for_context(*geometry_context, mask);
  }
  if (const MeshFieldContext *mesh_context = dynamic_cast<const MeshFieldContext *>(&context)) {
    return this->get_varray_for_context({mesh_context->mesh(), mesh_context->domain()}, mask);
  }
  if (const CurvesFieldContext *curve_context = dynamic_cast<const CurvesFieldContext *>(&context))
  {
    if (const Curves *curves_id = curve_context->curves_id()) {
      return this->get_varray_for_context({*curves_id, curve_context->domain()}, mask);
    }
    return this->get_varray_for_context({curve_context->curves(), curve_context->domain()}, mask);
  }
  if (const PointCloudFieldContext *point_context = dynamic_cast<const PointCloudFieldContext *>(
          &context))
  {
    return this->get_varray_for_context({point_context->pointcloud()}, mask);
  }
  if (const GreasePencilFieldContext *grease_pencil_context =
          dynamic_cast<const GreasePencilFieldContext *>(&context))
  {
    return this->get_varray_for_context({grease_pencil_context->grease_pencil()}, mask);
  }
  if (const GreasePencilLayerFieldContext *grease_pencil_context =
          dynamic_cast<const GreasePencilLayerFieldContext *>(&context))
  {
    return this->get_varray_for_context({grease_pencil_context->grease_pencil(),
                                         grease_pencil_context->domain(),
                                         grease_pencil_context->layer_index()},
                                        mask);
  }
  if (const InstancesFieldContext *instances_context = dynamic_cast<const InstancesFieldContext *>(
          &context))
  {
    return this->get_varray_for_context({instances_context->instances()}, mask);
  }
  return {};
}

std::optional<AttrDomain> GeometryFieldInput::preferred_domain(
    const GeometryComponent & /*component*/) const
{
  return std::nullopt;
}

GVArray MeshFieldInput::get_varray_for_context(const fn::FieldContext &context,
                                               const IndexMask &mask,
                                               ResourceScope & /*scope*/) const
{
  if (const GeometryFieldContext *geometry_context = dynamic_cast<const GeometryFieldContext *>(
          &context))
  {
    if (const Mesh *mesh = geometry_context->mesh()) {
      return this->get_varray_for_context(*mesh, geometry_context->domain(), mask);
    }
  }
  if (const MeshFieldContext *mesh_context = dynamic_cast<const MeshFieldContext *>(&context)) {
    return this->get_varray_for_context(mesh_context->mesh(), mesh_context->domain(), mask);
  }
  return {};
}

std::optional<AttrDomain> MeshFieldInput::preferred_domain(const Mesh & /*mesh*/) const
{
  return std::nullopt;
}

GVArray CurvesFieldInput::get_varray_for_context(const fn::FieldContext &context,
                                                 const IndexMask &mask,
                                                 ResourceScope & /*scope*/) const
{
  if (const GeometryFieldContext *geometry_context = dynamic_cast<const GeometryFieldContext *>(
          &context))
  {
    if (const CurvesGeometry *curves = geometry_context->curves_or_strokes()) {
      return this->get_varray_for_context(*curves, geometry_context->domain(), mask);
    }
  }
  if (const CurvesFieldContext *curves_context = dynamic_cast<const CurvesFieldContext *>(
          &context))
  {
    return this->get_varray_for_context(curves_context->curves(), curves_context->domain(), mask);
  }
  return {};
}

std::optional<AttrDomain> CurvesFieldInput::preferred_domain(
    const CurvesGeometry & /*curves*/) const
{
  return std::nullopt;
}

GVArray PointCloudFieldInput::get_varray_for_context(const fn::FieldContext &context,
                                                     const IndexMask &mask,
                                                     ResourceScope & /*scope*/) const
{
  if (const GeometryFieldContext *geometry_context = dynamic_cast<const GeometryFieldContext *>(
          &context))
  {
    if (const PointCloud *pointcloud = geometry_context->pointcloud()) {
      return this->get_varray_for_context(*pointcloud, mask);
    }
  }
  if (const PointCloudFieldContext *point_context = dynamic_cast<const PointCloudFieldContext *>(
          &context))
  {
    return this->get_varray_for_context(point_context->pointcloud(), mask);
  }
  return {};
}

GVArray InstancesFieldInput::get_varray_for_context(const fn::FieldContext &context,
                                                    const IndexMask &mask,
                                                    ResourceScope & /*scope*/) const
{
  if (const GeometryFieldContext *geometry_context = dynamic_cast<const GeometryFieldContext *>(
          &context))
  {
    if (const Instances *instances = geometry_context->instances()) {
      return this->get_varray_for_context(*instances, mask);
    }
  }
  if (const InstancesFieldContext *instances_context = dynamic_cast<const InstancesFieldContext *>(
          &context))
  {
    return this->get_varray_for_context(instances_context->instances(), mask);
  }
  return {};
}

GVArray AttributeFieldInput::get_varray_for_context(const GeometryFieldContext &context,
                                                    const IndexMask & /*mask*/) const
{
  const bke::AttrType data_type = cpp_type_to_attribute_type(*type_);
  const AttrDomain domain = context.domain();
  if (const GreasePencil *grease_pencil = context.grease_pencil()) {
    const AttributeAccessor layer_attributes = grease_pencil->attributes();
    if (domain == AttrDomain::Layer) {
      return *layer_attributes.lookup(name_, data_type);
    }
    if (ELEM(domain, AttrDomain::Point, AttrDomain::Curve)) {
      const int layer_index = context.grease_pencil_layer_index();
      const AttributeAccessor curves_attributes = *context.attributes();
      if (const GAttributeReader reader = curves_attributes.lookup(name_, domain, data_type)) {
        return *reader;
      }
      /* Lookup attribute on the layer domain if it does not exist on points or curves. */
      if (const GAttributeReader reader = layer_attributes.lookup(name_)) {
        const CPPType &cpp_type = reader.varray.type();
        BUFFER_FOR_CPP_TYPE_VALUE(cpp_type, value);
        BLI_SCOPED_DEFER([&]() { cpp_type.destruct(value); });
        reader.varray.get_to_uninitialized(layer_index, value);
        const int domain_size = curves_attributes.domain_size(domain);
        return GVArray::from_single(cpp_type, domain_size, value);
      }
    }
  }
  else if (context.domain() == bke::AttrDomain::Instance && name_ == "position") {
    /* Special case for "position" which is no longer an attribute on instances. */
    return bke::instance_position_varray(*context.instances());
  }
  else if (auto attributes = context.attributes()) {
    return *attributes->lookup(name_, domain, data_type);
  }

  return {};
}

GVArray AttributeExistsFieldInput::get_varray_for_context(const bke::GeometryFieldContext &context,
                                                          const IndexMask & /*mask*/) const
{
  const AttrDomain domain = context.domain();
  if (context.type() == GeometryComponent::Type::GreasePencil) {
    const AttributeAccessor layer_attributes = context.grease_pencil()->attributes();
    if (context.domain() == AttrDomain::Layer) {
      const bool exists = layer_attributes.contains(name_);
      const int domain_size = layer_attributes.domain_size(AttrDomain::Layer);
      return VArray<bool>::from_single(exists, domain_size);
    }
    const greasepencil::Drawing *drawing = context.grease_pencil_layer_drawing();
    const AttributeAccessor curve_attributes = drawing->strokes().attributes();
    const bool exists = layer_attributes.contains(name_) || curve_attributes.contains(name_);
    const int domain_size = curve_attributes.domain_size(domain);
    return VArray<bool>::from_single(exists, domain_size);
  }
  const bool exists = context.attributes()->contains(name_);
  const int domain_size = context.attributes()->domain_size(domain);
  return VArray<bool>::from_single(exists, domain_size);
}

std::string AttributeFieldInput::socket_inspection_name() const
{
  if (socket_inspection_name_) {
    return *socket_inspection_name_;
  }
  return fmt::format(fmt::runtime(TIP_("\"{}\" attribute from geometry")), name_);
}

uint64_t AttributeFieldInput::hash() const
{
  return get_default_hash(name_, type_);
}

bool AttributeFieldInput::is_equal_to(const fn::FieldNode &other) const
{
  if (const AttributeFieldInput *other_typed = dynamic_cast<const AttributeFieldInput *>(&other)) {
    return name_ == other_typed->name_ && type_ == other_typed->type_;
  }
  return false;
}

std::optional<AttrDomain> AttributeFieldInput::preferred_domain(
    const GeometryComponent &component) const
{
  const std::optional<AttributeAccessor> attributes = component.attributes();
  if (!attributes.has_value()) {
    return std::nullopt;
  }
  const std::optional<AttributeMetaData> meta_data = attributes->lookup_meta_data(name_);
  if (!meta_data.has_value()) {
    return std::nullopt;
  }
  return meta_data->domain;
}

static StringRef get_random_id_attribute_name(const AttrDomain domain)
{
  switch (domain) {
    case AttrDomain::Point:
    case AttrDomain::Instance:
      return "id";
    default:
      return "";
  }
}

GVArray IDAttributeFieldInput::get_varray_for_context(const GeometryFieldContext &context,
                                                      const IndexMask &mask) const
{

  const StringRef name = get_random_id_attribute_name(context.domain());
  if (auto attributes = context.attributes()) {
    if (GVArray attribute = *attributes->lookup<int>(name, context.domain())) {
      return attribute;
    }
  }

  /* Use the index as the fallback if no random ID attribute exists. */
  return fn::IndexFieldInput::get_index_varray(mask);
}

std::string IDAttributeFieldInput::socket_inspection_name() const
{
  return TIP_("ID / Index");
}

uint64_t IDAttributeFieldInput::hash() const
{
  /* All random ID attribute inputs are the same within the same evaluation context. */
  return 92386459827;
}

bool IDAttributeFieldInput::is_equal_to(const fn::FieldNode &other) const
{
  /* All random ID attribute inputs are the same within the same evaluation context. */
  return dynamic_cast<const IDAttributeFieldInput *>(&other) != nullptr;
}

GVArray NamedLayerSelectionFieldInput::get_varray_for_context(
    const bke::GeometryFieldContext &context, const IndexMask &mask) const
{
  using namespace bke::greasepencil;
  const AttrDomain domain = context.domain();
  if (!ELEM(domain, AttrDomain::Point, AttrDomain::Curve, AttrDomain::Layer)) {
    return {};
  }

  const GreasePencil &grease_pencil = *context.grease_pencil();
  if (!context.grease_pencil()) {
    return {};
  }

  auto layer_is_selected = [selection_name = StringRef(layer_name_),
                            &grease_pencil,
                            size = mask.min_array_size()](const int layer_i) {
    if (layer_i < 0 || layer_i >= grease_pencil.layers().size()) {
      return false;
    }
    const Layer &layer = grease_pencil.layer(layer_i);
    return layer.name() == selection_name;
  };

  if (ELEM(domain, AttrDomain::Point, AttrDomain::Curve)) {
    const int layer_i = context.grease_pencil_layer_index();
    const bool selected = layer_is_selected(layer_i);
    return VArray<bool>::from_single(selected, mask.min_array_size());
  }

  return VArray<bool>::from_func(mask.min_array_size(), layer_is_selected);
}

uint64_t NamedLayerSelectionFieldInput::hash() const
{
  return get_default_hash(layer_name_, type_);
}

bool NamedLayerSelectionFieldInput::is_equal_to(const fn::FieldNode &other) const
{
  if (const NamedLayerSelectionFieldInput *other_named_layer =
          dynamic_cast<const NamedLayerSelectionFieldInput *>(&other))
  {
    return layer_name_ == other_named_layer->layer_name_;
  }
  return false;
}

std::optional<AttrDomain> NamedLayerSelectionFieldInput::preferred_domain(
    const bke::GeometryComponent & /*component*/) const
{
  return AttrDomain::Layer;
}

template<typename T>
void copy_with_checked_indices(const VArray<T> &src,
                               const VArray<int> &indices,
                               const IndexMask &mask,
                               MutableSpan<T> dst)
{
  const IndexRange src_range = src.index_range();
  devirtualize_varray2(src, indices, [&](const auto src, const auto indices) {
    mask.foreach_index(GrainSize(4096), [&](const int i) {
      const int index = indices[i];
      if (src_range.contains(index)) {
        dst[i] = src[index];
      }
      else {
        dst[i] = {};
      }
    });
  });
}

void copy_with_checked_indices(const GVArray &src,
                               const VArray<int> &indices,
                               const IndexMask &mask,
                               GMutableSpan dst)
{
  bke::attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    copy_with_checked_indices(src.typed<T>(), indices, mask, dst.typed<T>());
  });
}

EvaluateAtIndexInput::EvaluateAtIndexInput(fn::Field<int> index_field,
                                           fn::GField value_field,
                                           AttrDomain value_field_domain)
    : bke::GeometryFieldInput(value_field.cpp_type(), "Evaluate at Index"),
      index_field_(std::move(index_field)),
      value_field_(std::move(value_field)),
      value_field_domain_(value_field_domain)
{
}

GVArray EvaluateAtIndexInput::get_varray_for_context(const bke::GeometryFieldContext &context,
                                                     const IndexMask &mask) const
{
  const std::optional<AttributeAccessor> attributes = context.attributes();
  if (!attributes) {
    return {};
  }

  const bke::GeometryFieldContext value_context{context, value_field_domain_};
  fn::FieldEvaluator value_evaluator{value_context, attributes->domain_size(value_field_domain_)};
  value_evaluator.add(value_field_);
  value_evaluator.evaluate();
  const GVArray &values = value_evaluator.get_evaluated(0);

  fn::FieldEvaluator index_evaluator{context, &mask};
  index_evaluator.add(index_field_);
  index_evaluator.evaluate();
  const VArray<int> indices = index_evaluator.get_evaluated<int>(0);

  GArray<> dst_array(values.type(), mask.min_array_size());
  copy_with_checked_indices(values, indices, mask, dst_array);
  return GVArray::from_garray(std::move(dst_array));
}

EvaluateOnDomainInput::EvaluateOnDomainInput(fn::GField field, AttrDomain domain)
    : bke::GeometryFieldInput(field.cpp_type(), "Evaluate on Domain"),
      src_field_(std::move(field)),
      src_domain_(domain)
{
}

GVArray EvaluateOnDomainInput::get_varray_for_context(const bke::GeometryFieldContext &context,
                                                      const IndexMask & /*mask*/) const
{
  const AttrDomain dst_domain = context.domain();
  const int dst_domain_size = context.attributes()->domain_size(dst_domain);
  const CPPType &cpp_type = src_field_.cpp_type();

  if (context.type() == GeometryComponent::Type::GreasePencil &&
      (src_domain_ == AttrDomain::Layer) != (dst_domain == AttrDomain::Layer))
  {
    /* Evaluate field just for the current layer. */
    if (src_domain_ == AttrDomain::Layer) {
      const bke::GeometryFieldContext src_domain_context{context, AttrDomain::Layer};
      const int layer_index = context.grease_pencil_layer_index();

      const IndexMask single_layer_mask = IndexRange(layer_index, 1);
      fn::FieldEvaluator value_evaluator{src_domain_context, &single_layer_mask};
      value_evaluator.add(src_field_);
      value_evaluator.evaluate();

      const GVArray &values = value_evaluator.get_evaluated(0);

      BUFFER_FOR_CPP_TYPE_VALUE(cpp_type, value);
      BLI_SCOPED_DEFER([&]() { cpp_type.destruct(value); });
      values.get_to_uninitialized(layer_index, value);
      return GVArray::from_single(cpp_type, dst_domain_size, value);
    }
    /* We don't adapt from curve to layer domain currently. */
    return GVArray::from_single_default(cpp_type, dst_domain_size);
  }

  const bke::AttributeAccessor attributes = *context.attributes();

  const bke::GeometryFieldContext other_domain_context{context, src_domain_};
  const int64_t src_domain_size = attributes.domain_size(src_domain_);
  GArray<> values(cpp_type, src_domain_size);
  fn::FieldEvaluator value_evaluator{other_domain_context, src_domain_size};
  value_evaluator.add_with_destination(src_field_, values.as_mutable_span());
  value_evaluator.evaluate();
  return attributes.adapt_domain(GVArray::from_garray(std::move(values)), src_domain_, dst_domain);
}

void EvaluateOnDomainInput::for_each_field_input_recursive(
    FunctionRef<void(const FieldInput &)> fn) const
{
  src_field_.node().for_each_field_input_recursive(fn);
}

std::optional<AttrDomain> EvaluateOnDomainInput::preferred_domain(
    const GeometryComponent & /*component*/) const
{
  return src_domain_;
}

}  // namespace blender::bke

/* -------------------------------------------------------------------- */
/** \name Mesh and Curve Normals Field Input
 * \{ */

namespace blender::bke {

GVArray NormalFieldInput::get_varray_for_context(const GeometryFieldContext &context,
                                                 const IndexMask &mask) const
{
  if (const Mesh *mesh = context.mesh()) {
    return mesh_normals_varray(
        *mesh, mask, context.domain(), legacy_corner_normals_, true_normals_);
  }
  if (const CurvesGeometry *curves = context.curves_or_strokes()) {
    return curve_normals_varray(*curves, context.domain());
  }
  return {};
}

std::string NormalFieldInput::socket_inspection_name() const
{
  return true_normals_ ? TIP_("True Normal") : TIP_("Normal");
}

uint64_t NormalFieldInput::hash() const
{
  return get_default_hash(2980541, legacy_corner_normals_, true_normals_);
}

bool NormalFieldInput::is_equal_to(const fn::FieldNode &other) const
{
  if (const NormalFieldInput *other_typed = dynamic_cast<const NormalFieldInput *>(&other)) {
    return legacy_corner_normals_ == other_typed->legacy_corner_normals_ &&
           true_normals_ == other_typed->true_normals_;
  }
  return false;
}

static std::optional<StringRefNull> try_get_field_direct_attribute_id(const fn::GField &any_field)
{
  if (const auto *field = dynamic_cast<const AttributeFieldInput *>(&any_field.node())) {
    return field->attribute_name();
  }
  return {};
}

static bool attribute_kind_matches(const AttributeMetaData meta_data,
                                   const AttrDomain domain,
                                   const bke::AttrType data_type)
{
  return meta_data.domain == domain && meta_data.data_type == data_type;
}

/**
 * Some fields reference attributes directly. When the referenced attribute has the requested type
 * and domain, use implicit sharing to avoid duplication when creating the captured attribute.
 */
static bool try_add_shared_field_attribute(MutableAttributeAccessor attributes,
                                           const StringRef id_to_create,
                                           const AttrDomain domain,
                                           const fn::GField &field)
{
  const std::optional<StringRef> field_id = try_get_field_direct_attribute_id(field);
  if (!field_id) {
    return false;
  }
  const std::optional<AttributeMetaData> meta_data = attributes.lookup_meta_data(*field_id);
  if (!meta_data) {
    return false;
  }
  const bke::AttrType data_type = bke::cpp_type_to_attribute_type(field.cpp_type());
  if (!attribute_kind_matches(*meta_data, domain, data_type)) {
    /* Avoid costly domain and type interpolation, which would make sharing impossible. */
    return false;
  }
  const GAttributeReader attribute = attributes.lookup(*field_id, domain, data_type);
  if (!attribute.sharing_info || !attribute.varray.is_span()) {
    return false;
  }
  const AttributeInitShared init(attribute.varray.get_internal_span().data(),
                                 *attribute.sharing_info);
  return attributes.add(id_to_create, domain, data_type, init);
}

static bool attribute_data_matches_varray(const GAttributeReader &attribute, const GVArray &varray)
{
  const CommonVArrayInfo varray_info = varray.common_info();
  if (varray_info.type != CommonVArrayInfo::Type::Span) {
    return false;
  }
  const CommonVArrayInfo attribute_info = attribute.varray.common_info();
  if (attribute_info.type != CommonVArrayInfo::Type::Span) {
    return false;
  }
  return varray_info.data == attribute_info.data;
}

static void initialize_new_data(MutableAttributeAccessor &attributes,
                                const AttrDomain domain,
                                const int domain_size,
                                const StringRef name,
                                const CPPType &type,
                                const bke::AttrType data_type,
                                void *buffer)
{
  /* NOTE: It's unnecessary to fill the values for elements that will be selected and also set
   * during field evaluation. A future optimization could evaluate the selection separately and use
   * its inverse here. */

  if (attributes.is_builtin(name)) {
    if (const GPointer value = attributes.get_builtin_default(name)) {
      type.fill_construct_n(value.get(), buffer, domain_size);
      return;
    }
  }
  if (const GAttributeReader old_attribute = attributes.lookup(name, domain, data_type)) {
    old_attribute.varray.materialize(buffer);
    return;
  }
  type.fill_construct_n(type.default_value(), buffer, domain_size);
}

bool try_capture_fields_on_geometry(MutableAttributeAccessor attributes,
                                    const fn::FieldContext &field_context,
                                    const Span<StringRef> attribute_ids,
                                    const AttrDomain domain,
                                    const fn::Field<bool> &selection,
                                    const Span<fn::GField> fields)
{
  BLI_assert(attribute_ids.size() == fields.size());
  const int domain_size = attributes.domain_size(domain);
  if (domain_size == 0) {
    bool all_added = true;
    for (const int i : attribute_ids.index_range()) {
      const bke::AttrType data_type = bke::cpp_type_to_attribute_type(fields[i].cpp_type());
      all_added &= attributes.add(attribute_ids[i], domain, data_type, AttributeInitConstruct{});
    }
    return all_added;
  }

  fn::FieldEvaluator evaluator{field_context, domain_size};
  evaluator.set_selection(selection);

  const bool selection_is_full = !selection.node().depends_on_input() &&
                                 fn::evaluate_constant_field(selection);

  struct StoreResult {
    int input_index;
    int evaluator_index;
  };
  Vector<StoreResult> results_to_store;

  struct AddResult {
    int input_index;
    int evaluator_index;
    void *buffer;
  };
  Vector<AddResult> results_to_add;

  bool success = true;

  for (const int input_index : attribute_ids.index_range()) {
    const StringRef id = attribute_ids[input_index];
    const CPPType &type = fields[input_index].cpp_type();
    const bke::AttrType data_type = bke::cpp_type_to_attribute_type(type);

    /* Avoid adding or writing to builtin attributes with an incorrect type or domain. */
    if (const std::optional<AttributeDomainAndType> meta_data =
            attributes.get_builtin_domain_and_type(id))
    {
      if (*meta_data != AttributeDomainAndType{domain, data_type}) {
        success = false;
        continue;
      }
    }

    const AttributeValidator validator = attributes.lookup_validator(id);
    const fn::GField field = validator.validate_field_if_necessary(fields[input_index]);

    /* We are writing to an attribute that exists already with the correct domain and type. */
    if (const GAttributeReader dst = attributes.lookup(id)) {
      if (dst.domain == domain && dst.varray.type() == field.cpp_type()) {
        const int evaluator_index = evaluator.add(field);
        results_to_store.append({input_index, evaluator_index});
        continue;
      }
    }

    if (!validator && selection_is_full) {
      if (try_add_shared_field_attribute(attributes, id, domain, field)) {
        continue;
      }
    }

    /* Could avoid allocating a new buffer if:
     * - The field does not depend on that attribute (we can't easily check for that yet). */
    void *buffer = MEM_mallocN_aligned(type.size * domain_size, type.alignment, __func__);
    if (!selection_is_full) {
      initialize_new_data(attributes, domain, domain_size, id, type, data_type, buffer);
    }

    GMutableSpan dst(type, buffer, domain_size);
    const int evaluator_index = evaluator.add_with_destination(field, dst);
    results_to_add.append({input_index, evaluator_index, buffer});
  }

  evaluator.evaluate();
  const IndexMask &mask = evaluator.get_evaluated_selection_as_mask();

  for (const StoreResult &result : results_to_store) {
    const StringRef id = attribute_ids[result.input_index];
    const GVArray &result_data = evaluator.get_evaluated(result.evaluator_index);
    const GAttributeReader dst = attributes.lookup(id);
    if (!attribute_data_matches_varray(dst, result_data)) {
      GSpanAttributeWriter dst_mut = attributes.lookup_for_write_span(id);
      array_utils::copy(result_data, mask, dst_mut.span);
      dst_mut.finish();
    }
  }

  for (const AddResult &result : results_to_add) {
    const StringRef id = attribute_ids[result.input_index];
    attributes.remove(id);
    const CPPType &type = fields[result.input_index].cpp_type();
    const bke::AttrType data_type = bke::cpp_type_to_attribute_type(type);
    if (!attributes.add(id, domain, data_type, AttributeInitMoveArray(result.buffer))) {
      /* If the name corresponds to a builtin attribute, removing the attribute might fail if
       * it's required, adding the attribute might fail if the domain or type is incorrect. */
      type.destruct_n(result.buffer, domain_size);
      MEM_freeN(result.buffer);
      success = false;
    }
  }

  return success;
}

bool try_capture_fields_on_geometry(GeometryComponent &component,
                                    const Span<StringRef> attribute_ids,
                                    const AttrDomain domain,
                                    const fn::Field<bool> &selection,
                                    const Span<fn::GField> fields)
{
  const GeometryComponent::Type component_type = component.type();
  if (component_type == GeometryComponent::Type::GreasePencil &&
      ELEM(domain, AttrDomain::Point, AttrDomain::Curve))
  {
    /* Capture the field on every layer individually. */
    auto &grease_pencil_component = static_cast<GreasePencilComponent &>(component);
    GreasePencil *grease_pencil = grease_pencil_component.get_for_write();
    if (grease_pencil == nullptr) {
      return false;
    }
    bool any_success = false;
    threading::parallel_for(grease_pencil->layers().index_range(), 8, [&](const IndexRange range) {
      for (const int layer_index : range) {
        if (greasepencil::Drawing *drawing = grease_pencil->get_eval_drawing(
                grease_pencil->layer(layer_index)))
        {
          const GeometryFieldContext field_context{*grease_pencil, domain, layer_index};
          const bool success = try_capture_fields_on_geometry(
              drawing->strokes_for_write().attributes_for_write(),
              field_context,
              attribute_ids,
              domain,
              selection,
              fields);
          if (success & !any_success) {
            any_success = true;
          }
        }
      }
    });
    return any_success;
  }
  if (component_type == GeometryComponent::Type::GreasePencil && domain != AttrDomain::Layer) {
    /* The remaining code only handles the layer domain for grease pencil geometries. */
    return false;
  }

  MutableAttributeAccessor attributes = *component.attributes_for_write();
  const GeometryFieldContext field_context{component, domain};
  return try_capture_fields_on_geometry(
      attributes, field_context, attribute_ids, domain, selection, fields);
}

bool try_capture_fields_on_geometry(GeometryComponent &component,
                                    const Span<StringRef> attribute_ids,
                                    const AttrDomain domain,
                                    const Span<fn::GField> fields)
{
  const fn::Field<bool> selection = fn::make_constant_field<bool>(true);
  return try_capture_fields_on_geometry(component, attribute_ids, domain, selection, fields);
}

std::optional<AttrDomain> try_detect_field_domain(const GeometryComponent &component,
                                                  const fn::GField &field)
{
  const GeometryComponent::Type component_type = component.type();
  if (component_type == GeometryComponent::Type::PointCloud) {
    return AttrDomain::Point;
  }
  if (component_type == GeometryComponent::Type::GreasePencil) {
    return AttrDomain::Layer;
  }
  if (component_type == GeometryComponent::Type::Instance) {
    return AttrDomain::Instance;
  }
  const std::shared_ptr<const fn::FieldInputs> &field_inputs = field.node().field_inputs();
  if (!field_inputs) {
    return std::nullopt;
  }
  std::optional<AttrDomain> output_domain;
  auto handle_domain = [&](const std::optional<AttrDomain> domain) {
    if (!domain.has_value()) {
      return false;
    }
    if (output_domain.has_value()) {
      if (*output_domain != *domain) {
        return false;
      }
      return true;
    }
    output_domain = domain;
    return true;
  };
  if (component_type == GeometryComponent::Type::Mesh) {
    const MeshComponent &mesh_component = static_cast<const MeshComponent &>(component);
    const Mesh *mesh = mesh_component.get();
    if (mesh == nullptr) {
      return std::nullopt;
    }
    for (const fn::FieldInput &field_input : field_inputs->deduplicated_nodes) {
      if (const auto *geometry_field_input = dynamic_cast<const GeometryFieldInput *>(
              &field_input))
      {
        if (!handle_domain(geometry_field_input->preferred_domain(component))) {
          return std::nullopt;
        }
      }
      else if (const auto *mesh_field_input = dynamic_cast<const MeshFieldInput *>(&field_input)) {
        if (!handle_domain(mesh_field_input->preferred_domain(*mesh))) {
          return std::nullopt;
        }
      }
      else {
        return std::nullopt;
      }
    }
  }
  if (component_type == GeometryComponent::Type::Curve) {
    const CurveComponent &curve_component = static_cast<const CurveComponent &>(component);
    const Curves *curves = curve_component.get();
    if (curves == nullptr) {
      return std::nullopt;
    }
    for (const fn::FieldInput &field_input : field_inputs->deduplicated_nodes) {
      if (const auto *geometry_field_input = dynamic_cast<const GeometryFieldInput *>(
              &field_input))
      {
        if (!handle_domain(geometry_field_input->preferred_domain(component))) {
          return std::nullopt;
        }
      }
      else if (const auto *curves_field_input = dynamic_cast<const CurvesFieldInput *>(
                   &field_input))
      {
        if (!handle_domain(curves_field_input->preferred_domain(curves->geometry.wrap()))) {
          return std::nullopt;
        }
      }
      else {
        return std::nullopt;
      }
    }
  }
  return output_domain;
}

}  // namespace blender::bke

/** \} */

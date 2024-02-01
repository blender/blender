/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 *
 * Common field utilities and field definitions for geometry components.
 */

#include "BKE_geometry_set.hh"

#include "FN_field.hh"

struct Mesh;
struct PointCloud;

namespace blender::bke {

class CurvesGeometry;
class GeometryFieldInput;
namespace greasepencil {
class Drawing;
}

class MeshFieldContext : public fn::FieldContext {
 private:
  const Mesh &mesh_;
  AttrDomain domain_;

 public:
  MeshFieldContext(const Mesh &mesh, AttrDomain domain);
  const Mesh &mesh() const
  {
    return mesh_;
  }

  AttrDomain domain() const
  {
    return domain_;
  }
};

class CurvesFieldContext : public fn::FieldContext {
 private:
  const CurvesGeometry &curves_;
  AttrDomain domain_;

 public:
  CurvesFieldContext(const CurvesGeometry &curves, AttrDomain domain);

  const CurvesGeometry &curves() const
  {
    return curves_;
  }

  AttrDomain domain() const
  {
    return domain_;
  }
};

class PointCloudFieldContext : public fn::FieldContext {
 private:
  const PointCloud &pointcloud_;

 public:
  PointCloudFieldContext(const PointCloud &pointcloud) : pointcloud_(pointcloud) {}

  const PointCloud &pointcloud() const
  {
    return pointcloud_;
  }
};

class GreasePencilFieldContext : public fn::FieldContext {
 private:
  const GreasePencil &grease_pencil_;

 public:
  GreasePencilFieldContext(const GreasePencil &grease_pencil) : grease_pencil_(grease_pencil) {}

  const GreasePencil &grease_pencil() const
  {
    return grease_pencil_;
  }
};

class GreasePencilLayerFieldContext : public fn::FieldContext {
 private:
  const GreasePencil &grease_pencil_;
  AttrDomain domain_;
  int layer_index_;

 public:
  GreasePencilLayerFieldContext(const GreasePencil &grease_pencil,
                                AttrDomain domain,
                                int layer_index)
      : grease_pencil_(grease_pencil), domain_(domain), layer_index_(layer_index)
  {
  }

  const GreasePencil &grease_pencil() const
  {
    return grease_pencil_;
  }

  AttrDomain domain() const
  {
    return domain_;
  }

  int layer_index() const
  {
    return layer_index_;
  }

  GVArray get_varray_for_input(const fn::FieldInput &field_input,
                               const IndexMask &mask,
                               ResourceScope &scope) const;
};

class InstancesFieldContext : public fn::FieldContext {
 private:
  const Instances &instances_;

 public:
  InstancesFieldContext(const Instances &instances) : instances_(instances) {}

  const Instances &instances() const
  {
    return instances_;
  }
};

/**
 * A field context that can represent meshes, curves, point clouds, instances or grease pencil
 * layers, used for field inputs that can work for multiple geometry types.
 */
class GeometryFieldContext : public fn::FieldContext {
 private:
  /**
   * Store the geometry as a void pointer instead of a #GeometryComponent to allow referencing data
   * that doesn't correspond directly to a geometry component type, in this case #CurvesGeometry
   * instead of #Curves.
   */
  const void *geometry_;
  const GeometryComponent::Type type_;
  AttrDomain domain_;
  /**
   * Only used when the type is grease pencil and the domain is either points or curves
   * (not layers).
   */
  int grease_pencil_layer_index_;

  friend GeometryFieldInput;

 public:
  GeometryFieldContext(const GeometryFieldContext &other, AttrDomain domain);
  GeometryFieldContext(const GeometryComponent &component, AttrDomain domain);
  GeometryFieldContext(const void *geometry,
                       GeometryComponent::Type type,
                       AttrDomain domain,
                       int grease_pencil_layer_index);
  GeometryFieldContext(const Mesh &mesh, AttrDomain domain);
  GeometryFieldContext(const CurvesGeometry &curves, AttrDomain domain);
  GeometryFieldContext(const GreasePencil &grease_pencil);
  GeometryFieldContext(const GreasePencil &grease_pencil, AttrDomain domain, int layer_index);
  GeometryFieldContext(const PointCloud &points);
  GeometryFieldContext(const Instances &instances);

  const void *geometry() const
  {
    return geometry_;
  }

  GeometryComponent::Type type() const
  {
    return type_;
  }

  AttrDomain domain() const
  {
    return domain_;
  }

  int grease_pencil_layer_index() const
  {
    BLI_assert(this->type_ == GeometryComponent::Type::GreasePencil);
    BLI_assert(ELEM(this->domain_, AttrDomain::Layer, AttrDomain::Curve, AttrDomain::Point));
    return grease_pencil_layer_index_;
  }

  std::optional<AttributeAccessor> attributes() const;
  const Mesh *mesh() const;
  const CurvesGeometry *curves() const;
  const PointCloud *pointcloud() const;
  const GreasePencil *grease_pencil() const;
  const greasepencil::Drawing *grease_pencil_layer_drawing() const;
  const Instances *instances() const;
  const CurvesGeometry *curves_or_strokes() const;
};

class GeometryFieldInput : public fn::FieldInput {
 public:
  using fn::FieldInput::FieldInput;
  GVArray get_varray_for_context(const fn::FieldContext &context,
                                 const IndexMask &mask,
                                 ResourceScope &scope) const override;
  virtual GVArray get_varray_for_context(const GeometryFieldContext &context,
                                         const IndexMask &mask) const = 0;
  virtual std::optional<AttrDomain> preferred_domain(const GeometryComponent &component) const;
};

class MeshFieldInput : public fn::FieldInput {
 public:
  using fn::FieldInput::FieldInput;
  GVArray get_varray_for_context(const fn::FieldContext &context,
                                 const IndexMask &mask,
                                 ResourceScope &scope) const override;
  virtual GVArray get_varray_for_context(const Mesh &mesh,
                                         AttrDomain domain,
                                         const IndexMask &mask) const = 0;
  virtual std::optional<AttrDomain> preferred_domain(const Mesh &mesh) const;
};

class CurvesFieldInput : public fn::FieldInput {
 public:
  using fn::FieldInput::FieldInput;
  GVArray get_varray_for_context(const fn::FieldContext &context,
                                 const IndexMask &mask,
                                 ResourceScope &scope) const override;
  virtual GVArray get_varray_for_context(const CurvesGeometry &curves,
                                         AttrDomain domain,
                                         const IndexMask &mask) const = 0;
  virtual std::optional<AttrDomain> preferred_domain(const CurvesGeometry &curves) const;
};

class PointCloudFieldInput : public fn::FieldInput {
 public:
  using fn::FieldInput::FieldInput;
  GVArray get_varray_for_context(const fn::FieldContext &context,
                                 const IndexMask &mask,
                                 ResourceScope &scope) const override;
  virtual GVArray get_varray_for_context(const PointCloud &pointcloud,
                                         const IndexMask &mask) const = 0;
};

class InstancesFieldInput : public fn::FieldInput {
 public:
  using fn::FieldInput::FieldInput;
  GVArray get_varray_for_context(const fn::FieldContext &context,
                                 const IndexMask &mask,
                                 ResourceScope &scope) const override;
  virtual GVArray get_varray_for_context(const Instances &instances,
                                         const IndexMask &mask) const = 0;
};

class AttributeFieldInput : public GeometryFieldInput {
 private:
  std::string name_;

 public:
  AttributeFieldInput(std::string name, const CPPType &type)
      : GeometryFieldInput(type, name), name_(std::move(name))
  {
    category_ = Category::NamedAttribute;
  }

  static fn::GField Create(std::string name, const CPPType &type)
  {
    auto field_input = std::make_shared<AttributeFieldInput>(std::move(name), type);
    return fn::GField(field_input);
  }
  template<typename T> static fn::Field<T> Create(std::string name)
  {
    return fn::Field<T>(Create(std::move(name), CPPType::get<T>()));
  }

  StringRefNull attribute_name() const
  {
    return name_;
  }

  GVArray get_varray_for_context(const GeometryFieldContext &context,
                                 const IndexMask &mask) const override;

  std::string socket_inspection_name() const override;

  uint64_t hash() const override;
  bool is_equal_to(const fn::FieldNode &other) const override;
  std::optional<AttrDomain> preferred_domain(const GeometryComponent &component) const override;
};

class AttributeExistsFieldInput final : public bke::GeometryFieldInput {
 private:
  std::string name_;

 public:
  AttributeExistsFieldInput(std::string name, const CPPType &type)
      : GeometryFieldInput(type, name), name_(std::move(name))
  {
    category_ = Category::Generated;
  }

  static fn::Field<bool> Create(std::string name)
  {
    const CPPType &type = CPPType::get<bool>();
    auto field_input = std::make_shared<AttributeExistsFieldInput>(std::move(name), type);
    return fn::Field<bool>(field_input);
  }

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask &mask) const final;
};

class NamedLayerSelectionFieldInput final : public bke::GeometryFieldInput {
 private:
  std::string layer_name_;

 public:
  NamedLayerSelectionFieldInput(std::string layer_name)
      : bke::GeometryFieldInput(CPPType::get<bool>(), "Named Layer node"),
        layer_name_(std::move(layer_name))
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask &mask) const final;
  uint64_t hash() const override;
  bool is_equal_to(const fn::FieldNode &other) const override;
  std::optional<AttrDomain> preferred_domain(const GeometryComponent &component) const override;
};

class IDAttributeFieldInput : public GeometryFieldInput {
 public:
  IDAttributeFieldInput() : GeometryFieldInput(CPPType::get<int>())
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const GeometryFieldContext &context,
                                 const IndexMask &mask) const override;

  std::string socket_inspection_name() const override;

  uint64_t hash() const override;
  bool is_equal_to(const fn::FieldNode &other) const override;
};

VArray<float3> curve_normals_varray(const CurvesGeometry &curves, AttrDomain domain);

VArray<float3> mesh_normals_varray(const Mesh &mesh, const IndexMask &mask, AttrDomain domain);

class NormalFieldInput : public GeometryFieldInput {
 public:
  NormalFieldInput() : GeometryFieldInput(CPPType::get<float3>())
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const GeometryFieldContext &context,
                                 const IndexMask &mask) const override;

  std::string socket_inspection_name() const override;

  uint64_t hash() const override;
  bool is_equal_to(const fn::FieldNode &other) const override;
};

class AnonymousAttributeFieldInput : public GeometryFieldInput {
 private:
  AnonymousAttributeIDPtr anonymous_id_;
  std::string producer_name_;

 public:
  AnonymousAttributeFieldInput(AnonymousAttributeIDPtr anonymous_id,
                               const CPPType &type,
                               std::string producer_name)
      : GeometryFieldInput(type, anonymous_id->user_name()),
        anonymous_id_(std::move(anonymous_id)),
        producer_name_(std::move(producer_name))
  {
    category_ = Category::AnonymousAttribute;
  }

  template<typename T>
  static fn::Field<T> Create(AnonymousAttributeIDPtr anonymous_id, std::string producer_name)
  {
    const CPPType &type = CPPType::get<T>();
    auto field_input = std::make_shared<AnonymousAttributeFieldInput>(
        std::move(anonymous_id), type, std::move(producer_name));
    return fn::Field<T>{field_input};
  }

  const AnonymousAttributeIDPtr &anonymous_id() const
  {
    return anonymous_id_;
  }

  GVArray get_varray_for_context(const GeometryFieldContext &context,
                                 const IndexMask &mask) const override;

  std::string socket_inspection_name() const override;

  uint64_t hash() const override;
  bool is_equal_to(const fn::FieldNode &other) const override;
  std::optional<AttrDomain> preferred_domain(const GeometryComponent &component) const override;
};

class CurveLengthFieldInput final : public CurvesFieldInput {
 public:
  CurveLengthFieldInput();
  GVArray get_varray_for_context(const CurvesGeometry &curves,
                                 AttrDomain domain,
                                 const IndexMask &mask) const final;
  uint64_t hash() const override;
  bool is_equal_to(const fn::FieldNode &other) const override;
  std::optional<AttrDomain> preferred_domain(const bke::CurvesGeometry &curves) const final;
};

class EvaluateAtIndexInput final : public bke::GeometryFieldInput {
 private:
  fn::Field<int> index_field_;
  fn::GField value_field_;
  AttrDomain value_field_domain_;

 public:
  EvaluateAtIndexInput(fn::Field<int> index_field,
                       fn::GField value_field,
                       AttrDomain value_field_domain);

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask &mask) const final;

  std::optional<AttrDomain> preferred_domain(const GeometryComponent & /*component*/) const final
  {
    return value_field_domain_;
  }
};

void copy_with_checked_indices(const GVArray &src,
                               const VArray<int> &indices,
                               const IndexMask &mask,
                               GMutableSpan dst);

class EvaluateOnDomainInput final : public bke::GeometryFieldInput {
 private:
  fn::GField src_field_;
  AttrDomain src_domain_;

 public:
  EvaluateOnDomainInput(fn::GField field, AttrDomain domain);

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask & /*mask*/) const final;
  void for_each_field_input_recursive(FunctionRef<void(const FieldInput &)> fn) const override;

  std::optional<AttrDomain> preferred_domain(
      const GeometryComponent & /*component*/) const override;
};

bool try_capture_field_on_geometry(MutableAttributeAccessor attributes,
                                   const fn::FieldContext &field_context,
                                   const AttributeIDRef &attribute_id,
                                   AttrDomain domain,
                                   const fn::Field<bool> &selection,
                                   const fn::GField &field);

bool try_capture_field_on_geometry(GeometryComponent &component,
                                   const AttributeIDRef &attribute_id,
                                   AttrDomain domain,
                                   const fn::GField &field);

bool try_capture_field_on_geometry(GeometryComponent &component,
                                   const AttributeIDRef &attribute_id,
                                   AttrDomain domain,
                                   const fn::Field<bool> &selection,
                                   const fn::GField &field);

/**
 * Try to find the geometry domain that the field should be evaluated on. If it is not obvious
 * which domain is correct, none is returned.
 */
std::optional<AttrDomain> try_detect_field_domain(const GeometryComponent &component,
                                                  const fn::GField &field);

}  // namespace blender::bke

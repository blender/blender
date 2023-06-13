/* SPDX-FileCopyrightText: 2023 Blender Foundation
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

class MeshFieldContext : public fn::FieldContext {
 private:
  const Mesh &mesh_;
  const eAttrDomain domain_;

 public:
  MeshFieldContext(const Mesh &mesh, const eAttrDomain domain);
  const Mesh &mesh() const
  {
    return mesh_;
  }

  eAttrDomain domain() const
  {
    return domain_;
  }
};

class CurvesFieldContext : public fn::FieldContext {
 private:
  const CurvesGeometry &curves_;
  const eAttrDomain domain_;

 public:
  CurvesFieldContext(const CurvesGeometry &curves, const eAttrDomain domain);

  const CurvesGeometry &curves() const
  {
    return curves_;
  }

  eAttrDomain domain() const
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
 * A field context that can represent meshes, curves, point clouds, or instances,
 * used for field inputs that can work for multiple geometry types.
 */
class GeometryFieldContext : public fn::FieldContext {
 private:
  /**
   * Store the geometry as a void pointer instead of a #GeometryComponent to allow referencing data
   * that doesn't correspond directly to a geometry component type, in this case #CurvesGeometry
   * instead of #Curves.
   */
  const void *geometry_;
  const GeometryComponentType type_;
  const eAttrDomain domain_;

  friend GeometryFieldInput;

 public:
  GeometryFieldContext(const GeometryComponent &component, eAttrDomain domain);
  GeometryFieldContext(const void *geometry, GeometryComponentType type, eAttrDomain domain);

  const void *geometry() const
  {
    return geometry_;
  }

  GeometryComponentType type() const
  {
    return type_;
  }

  eAttrDomain domain() const
  {
    return domain_;
  }

  std::optional<AttributeAccessor> attributes() const;
  const Mesh *mesh() const;
  const CurvesGeometry *curves() const;
  const PointCloud *pointcloud() const;
  const Instances *instances() const;

 private:
  GeometryFieldContext(const Mesh &mesh, eAttrDomain domain);
  GeometryFieldContext(const CurvesGeometry &curves, eAttrDomain domain);
  GeometryFieldContext(const PointCloud &points);
  GeometryFieldContext(const Instances &instances);
};

class GeometryFieldInput : public fn::FieldInput {
 public:
  using fn::FieldInput::FieldInput;
  GVArray get_varray_for_context(const fn::FieldContext &context,
                                 const IndexMask &mask,
                                 ResourceScope &scope) const override;
  virtual GVArray get_varray_for_context(const GeometryFieldContext &context,
                                         const IndexMask &mask) const = 0;
  virtual std::optional<eAttrDomain> preferred_domain(const GeometryComponent &component) const;
};

class MeshFieldInput : public fn::FieldInput {
 public:
  using fn::FieldInput::FieldInput;
  GVArray get_varray_for_context(const fn::FieldContext &context,
                                 const IndexMask &mask,
                                 ResourceScope &scope) const override;
  virtual GVArray get_varray_for_context(const Mesh &mesh,
                                         eAttrDomain domain,
                                         const IndexMask &mask) const = 0;
  virtual std::optional<eAttrDomain> preferred_domain(const Mesh &mesh) const;
};

class CurvesFieldInput : public fn::FieldInput {
 public:
  using fn::FieldInput::FieldInput;
  GVArray get_varray_for_context(const fn::FieldContext &context,
                                 const IndexMask &mask,
                                 ResourceScope &scope) const override;
  virtual GVArray get_varray_for_context(const CurvesGeometry &curves,
                                         eAttrDomain domain,
                                         const IndexMask &mask) const = 0;
  virtual std::optional<eAttrDomain> preferred_domain(const CurvesGeometry &curves) const;
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
  std::optional<eAttrDomain> preferred_domain(const GeometryComponent &component) const override;
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

VArray<float3> curve_normals_varray(const CurvesGeometry &curves, const eAttrDomain domain);

VArray<float3> mesh_normals_varray(const Mesh &mesh, const IndexMask &mask, eAttrDomain domain);

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
        producer_name_(producer_name)
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
  std::optional<eAttrDomain> preferred_domain(const GeometryComponent &component) const override;
};

class CurveLengthFieldInput final : public CurvesFieldInput {
 public:
  CurveLengthFieldInput();
  GVArray get_varray_for_context(const CurvesGeometry &curves,
                                 eAttrDomain domain,
                                 const IndexMask &mask) const final;
  uint64_t hash() const override;
  bool is_equal_to(const fn::FieldNode &other) const override;
  std::optional<eAttrDomain> preferred_domain(const bke::CurvesGeometry &curves) const final;
};

bool try_capture_field_on_geometry(GeometryComponent &component,
                                   const AttributeIDRef &attribute_id,
                                   const eAttrDomain domain,
                                   const fn::GField &field);

bool try_capture_field_on_geometry(GeometryComponent &component,
                                   const AttributeIDRef &attribute_id,
                                   const eAttrDomain domain,
                                   const fn::Field<bool> &selection,
                                   const fn::GField &field);

/**
 * Try to find the geometry domain that the field should be evaluated on. If it is not obvious
 * which domain is correct, none is returned.
 */
std::optional<eAttrDomain> try_detect_field_domain(const GeometryComponent &component,
                                                   const fn::GField &field);

}  // namespace blender::bke

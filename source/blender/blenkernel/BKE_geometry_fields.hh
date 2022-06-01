/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 *
 * Common field utilities and field definitions for geometry components.
 */

#include "BKE_geometry_set.hh"

#include "FN_field.hh"

namespace blender::bke {

class GeometryComponentFieldContext : public fn::FieldContext {
 private:
  const GeometryComponent &component_;
  const eAttrDomain domain_;

 public:
  GeometryComponentFieldContext(const GeometryComponent &component, const eAttrDomain domain)
      : component_(component), domain_(domain)
  {
  }

  const GeometryComponent &geometry_component() const
  {
    return component_;
  }

  eAttrDomain domain() const
  {
    return domain_;
  }
};

class GeometryFieldInput : public fn::FieldInput {
 public:
  using fn::FieldInput::FieldInput;

  GVArray get_varray_for_context(const fn::FieldContext &context,
                                 IndexMask mask,
                                 ResourceScope &scope) const override;

  virtual GVArray get_varray_for_context(const GeometryComponent &component,
                                         eAttrDomain domain,
                                         IndexMask mask) const = 0;
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

  template<typename T> static fn::Field<T> Create(std::string name)
  {
    const CPPType &type = CPPType::get<T>();
    auto field_input = std::make_shared<AttributeFieldInput>(std::move(name), type);
    return fn::Field<T>{field_input};
  }

  StringRefNull attribute_name() const
  {
    return name_;
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 eAttrDomain domain,
                                 IndexMask mask) const override;

  std::string socket_inspection_name() const override;

  uint64_t hash() const override;
  bool is_equal_to(const fn::FieldNode &other) const override;
};

class IDAttributeFieldInput : public GeometryFieldInput {
 public:
  IDAttributeFieldInput() : GeometryFieldInput(CPPType::get<int>())
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 eAttrDomain domain,
                                 IndexMask mask) const override;

  std::string socket_inspection_name() const override;

  uint64_t hash() const override;
  bool is_equal_to(const fn::FieldNode &other) const override;
};

VArray<float3> curve_normals_varray(const CurveComponent &component, const eAttrDomain domain);

VArray<float3> mesh_normals_varray(const MeshComponent &mesh_component,
                                   const Mesh &mesh,
                                   const IndexMask mask,
                                   eAttrDomain domain);

class NormalFieldInput : public GeometryFieldInput {
 public:
  NormalFieldInput() : GeometryFieldInput(CPPType::get<float3>())
  {
    category_ = Category::Generated;
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 const eAttrDomain domain,
                                 IndexMask mask) const override;

  std::string socket_inspection_name() const override;

  uint64_t hash() const override;
  bool is_equal_to(const fn::FieldNode &other) const override;
};

class AnonymousAttributeFieldInput : public GeometryFieldInput {
 private:
  /**
   * A strong reference is required to make sure that the referenced attribute is not removed
   * automatically.
   */
  StrongAnonymousAttributeID anonymous_id_;
  std::string producer_name_;

 public:
  AnonymousAttributeFieldInput(StrongAnonymousAttributeID anonymous_id,
                               const CPPType &type,
                               std::string producer_name)
      : GeometryFieldInput(type, anonymous_id.debug_name()),
        anonymous_id_(std::move(anonymous_id)),
        producer_name_(producer_name)
  {
    category_ = Category::AnonymousAttribute;
  }

  template<typename T>
  static fn::Field<T> Create(StrongAnonymousAttributeID anonymous_id, std::string producer_name)
  {
    const CPPType &type = CPPType::get<T>();
    auto field_input = std::make_shared<AnonymousAttributeFieldInput>(
        std::move(anonymous_id), type, std::move(producer_name));
    return fn::Field<T>{field_input};
  }

  GVArray get_varray_for_context(const GeometryComponent &component,
                                 eAttrDomain domain,
                                 IndexMask mask) const override;

  std::string socket_inspection_name() const override;

  uint64_t hash() const override;
  bool is_equal_to(const fn::FieldNode &other) const override;
};

class CurveLengthFieldInput final : public GeometryFieldInput {
 public:
  CurveLengthFieldInput();
  GVArray get_varray_for_context(const GeometryComponent &component,
                                 eAttrDomain domain,
                                 IndexMask mask) const final;
  uint64_t hash() const override;
  bool is_equal_to(const fn::FieldNode &other) const override;
};

}  // namespace blender::bke

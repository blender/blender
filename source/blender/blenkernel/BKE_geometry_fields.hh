/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup bke
 *
 * Common field utilities and field definitions for geometry components.
 */

#include "BKE_anonymous_attribute_id.hh"
#include "BKE_geometry_set.hh"

#include "BLI_fixed_string.hh"

#include "FN_field_evaluation.hh"

namespace blender {

struct Mesh;
struct PointCloud;

namespace bke {

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
  const Curves *curves_id_ = nullptr;

 public:
  CurvesFieldContext(const CurvesGeometry &curves, AttrDomain domain);
  CurvesFieldContext(const Curves &curves_id, AttrDomain domain);

  const CurvesGeometry &curves() const
  {
    return curves_;
  }

  const Curves *curves_id() const
  {
    return curves_id_;
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
                               ResourceScope &scope) const override;
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
  const Curves *curves_id_ = nullptr;
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
  GeometryFieldContext(const Curves &curves_id, AttrDomain domain);
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
  const Curves *curves_id() const;
};

/**
 * Information about a field input's relationship with the domain of the data it represents.
 *
 * If the native field domain is `Point/Face`, evaluating the field on the corner domain is
 * identical to evaluating it on the native domain and copying the values to corners afterwards. If
 * the native field domain is `Curve`, evaluating it on the point domain is identical to evaluating
 * it on the curve domain and copying the values to the points afterwards. Of course, copying the
 * values to the more complex domain can be skipped if the algorithm can be optimized to use the
 * data directly from the smaller domain.
 */
struct NativeFieldDomain {
  /**
   * The input may depend on the order of the domain. For example, the index field, can't be
   * transparently evaluated on a different domain unlike attribute fields which are more flexible
   * because of domain interpolation.
   */
  struct None {};
  /** The input represents data on a specific domain. */
  struct Domain {
    AttrDomain domain;
  };
  /** Interpolating the field between domains does not change the value. */
  struct Constant {};

  std::variant<None, Domain, Constant> variant;

  NativeFieldDomain(const None & /*tag*/) : variant(None{}) {}
  NativeFieldDomain(const Domain &domain) : variant(domain) {}
  NativeFieldDomain(const Constant & /*tag*/) : variant(Constant{}) {}
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
  virtual NativeFieldDomain native_domain_info(const GeometryComponent &component) const;
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
  virtual NativeFieldDomain native_domain_info(const Mesh &mesh) const;
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
  std::optional<std::string> socket_inspection_name_;

 public:
  AttributeFieldInput(std::string name,
                      const CPPType &type,
                      std::optional<std::string> socket_inspection_name = std::nullopt)
      : GeometryFieldInput(type, name),
        name_(std::move(name)),
        socket_inspection_name_(std::move(socket_inspection_name))
  {
  }

  static fn::GField from(std::string name,
                         const CPPType &type,
                         std::optional<std::string> socket_inspection_name = std::nullopt)
  {
    return fn::GField::from_input<AttributeFieldInput>(
        std::move(name), type, std::move(socket_inspection_name));
  }
  template<typename T>
  static fn::Field<T> from(std::string name,
                           std::optional<std::string> socket_inspection_name = std::nullopt)
  {
    return from(std::move(name), CPPType::get<T>(), std::move(socket_inspection_name))
        .template typed<T>();
  }

  StringRefNull attribute_name() const
  {
    return name_;
  }

  GVArray get_varray_for_context(const GeometryFieldContext &context,
                                 const IndexMask &mask) const override;

  std::string socket_inspection_name() const override;

  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep &deep_hash_cache) const override;
  std::optional<AttrDomain> preferred_domain(const GeometryComponent &component) const override;
  NativeFieldDomain native_domain_info(const GeometryComponent &component) const override;

  template<typename T, FixedString FStr> static const fn::Field<T> &get_field()
  {
    static const auto field = fn::Field<T>::template from_input<AttributeFieldInput>(
        FStr.data, CPPType::get<T>());
    /* Use a non-owning wrapper to avoid unnecessary reference counting of a static field. */
    static const auto field_ref = fn::Field<T>::from_non_owning_ref(field);
    return field_ref;
  }
};

class AttributeExistsFieldInput final : public bke::GeometryFieldInput {
 private:
  std::string name_;

 public:
  AttributeExistsFieldInput(std::string name, const CPPType &type)
      : GeometryFieldInput(type, name), name_(std::move(name))
  {
  }

  static fn::Field<bool> from(std::string name)
  {
    const CPPType &type = CPPType::get<bool>();
    return fn::GField::from_input<AttributeExistsFieldInput>(std::move(name), type).typed<bool>();
  }

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask &mask) const final;
  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep &deep_hash_cache) const override;
  NativeFieldDomain native_domain_info(const GeometryComponent &component) const override;
};

class NamedLayerSelectionFieldInput final : public bke::GeometryFieldInput {
 private:
  std::string layer_name_;

 public:
  NamedLayerSelectionFieldInput(std::string layer_name)
      : bke::GeometryFieldInput(CPPType::get<bool>(), "Named Layer node"),
        layer_name_(std::move(layer_name))
  {
  }

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask &mask) const final;
  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep &deep_hash_cache) const override;
  std::optional<AttrDomain> preferred_domain(const GeometryComponent &component) const override;
};

class IDAttributeFieldInput : public GeometryFieldInput {
 public:
  IDAttributeFieldInput() : GeometryFieldInput(CPPType::get<int>()) {}

  GVArray get_varray_for_context(const GeometryFieldContext &context,
                                 const IndexMask &mask) const override;

  std::string socket_inspection_name() const override;

  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep &deep_hash_cache) const override;

  /** Cached  field to avoid allocating a new one every time. */
  static const fn::Field<int> &get_field();
};

VArray<float3> curve_normals_varray(const CurvesGeometry &curves, AttrDomain domain);

VArray<float3> mesh_normals_varray(const Mesh &mesh,
                                   const IndexMask &mask,
                                   AttrDomain domain,
                                   bool no_corner_normals = false,
                                   bool true_normals = false);

class NormalFieldInput : public GeometryFieldInput {
  bool legacy_corner_normals_ = false;
  bool true_normals_ = false;

 public:
  NormalFieldInput(const bool legacy_corner_normals = false, const bool true_normals = false)
      : GeometryFieldInput(CPPType::get<float3>()),
        legacy_corner_normals_(legacy_corner_normals),
        true_normals_(true_normals)
  {
  }

  GVArray get_varray_for_context(const GeometryFieldContext &context,
                                 const IndexMask &mask) const override;

  std::string socket_inspection_name() const override;

  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep &deep_hash_cache) const override;

  NativeFieldDomain native_domain_info(const GeometryComponent &component) const override;

  /** Cached normal field to avoid allocating a new one every time. */
  static const fn::Field<float3> &get_field();
};

class CurveLengthFieldInput final : public CurvesFieldInput {
 public:
  CurveLengthFieldInput();
  GVArray get_varray_for_context(const CurvesGeometry &curves,
                                 AttrDomain domain,
                                 const IndexMask &mask) const final;
  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep &deep_hash_cache) const override;
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
  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep &deep_hash_cache) const override;
  std::optional<AttrDomain> preferred_domain(const GeometryComponent & /*component*/) const final
  {
    return value_field_domain_;
  }
};

class SampleIndexFunction : public mf::MultiFunction {
  GeometrySet src_geometry_;
  fn::GField src_field_;
  AttrDomain domain_;

  mf::Signature signature_;

  mutable CacheMutex mutex_;
  mutable std::optional<bke::GeometryFieldContext> geometry_context_;
  mutable std::unique_ptr<fn::FieldEvaluator> evaluator_;
  mutable const GVArray *src_data_ = nullptr;

 public:
  SampleIndexFunction(GeometrySet geometry, fn::GField src_field, AttrDomain domain);
  void prepare_for_execution() const override;
  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override;
  void hash_unique(UniqueHashBytes &hash) const override;

  static const GeometryComponent *find_source_component(const GeometrySet &geometry,
                                                        AttrDomain domain);
};

class EvaluateOnDomainInput final : public bke::GeometryFieldInput {
 private:
  fn::GField src_field_;
  AttrDomain src_domain_;

 public:
  EvaluateOnDomainInput(fn::GField field, AttrDomain domain);

  GVArray get_varray_for_context(const bke::GeometryFieldContext &context,
                                 const IndexMask & /*mask*/) const final;
  void hash_unique(UniqueHashBytes &hash, fn::FieldHashDeep &deep_hash_cache) const override;
  void foreach_recursive_field(FunctionRef<void(const fn::GField &)> fn) const override;

  std::optional<AttrDomain> preferred_domain(
      const GeometryComponent & /*component*/) const override;
  NativeFieldDomain native_domain_info(const GeometryComponent & /*component*/) const override;
};

bool try_capture_fields_on_geometry(MutableAttributeAccessor attributes,
                                    const fn::FieldContext &field_context,
                                    Span<StringRef> names,
                                    AttrDomain domain,
                                    const fn::Field<bool> &selection,
                                    Span<fn::GField> fields);

inline bool try_capture_field_on_geometry(MutableAttributeAccessor attributes,
                                          const fn::FieldContext &field_context,
                                          const StringRef name,
                                          AttrDomain domain,
                                          const fn::Field<bool> &selection,
                                          const fn::GField &field)
{
  return try_capture_fields_on_geometry(
      attributes, field_context, {name}, domain, selection, {field});
}

bool try_capture_fields_on_geometry(GeometryComponent &component,
                                    Span<StringRef> names,
                                    AttrDomain domain,
                                    Span<fn::GField> fields);

inline bool try_capture_field_on_geometry(GeometryComponent &component,
                                          const StringRef name,
                                          AttrDomain domain,
                                          const fn::GField &field)
{
  return try_capture_fields_on_geometry(component, {name}, domain, {field});
}

bool try_capture_fields_on_geometry(GeometryComponent &component,
                                    Span<StringRef> names,
                                    AttrDomain domain,
                                    const fn::Field<bool> &selection,
                                    Span<fn::GField> fields);

inline bool try_capture_field_on_geometry(GeometryComponent &component,
                                          const StringRef name,
                                          AttrDomain domain,
                                          const fn::Field<bool> &selection,
                                          const fn::GField &field)
{
  return try_capture_fields_on_geometry(component, {name}, domain, selection, {field});
}

/**
 * Try to find the geometry domain that the field should be evaluated on. If it is not obvious
 * which domain is correct, none is returned.
 */
std::optional<AttrDomain> try_detect_field_domain(const GeometryComponent &component,
                                                  const fn::GField &field);

/**
 * Try to detect the domain that the field's inputs represent. If any field may depend on the
 * order of the domain, none is returned, and if the fields will give the same value regardless of
 * the domain, none is also returned.
 */
std::optional<AttrDomain> try_detect_native_field_domain(const GeometryComponent &component,
                                                         const fn::GField &field);

}  // namespace bke
}  // namespace blender

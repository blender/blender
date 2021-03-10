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

#include "BLI_listbase.h"
#include "BLI_threads.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_attribute_access.hh"
#include "BKE_attribute_math.hh"
#include "BKE_deform.h"
#include "BKE_geometry_set.hh"
#include "BKE_lib_id.h"
#include "BKE_mesh.h"

#include "attribute_access_intern.hh"

/* Can't include BKE_object_deform.h right now, due to an enum forward declaration.  */
extern "C" MDeformVert *BKE_object_defgroup_data_create(ID *id);

using blender::bke::ReadAttributePtr;

/* -------------------------------------------------------------------- */
/** \name Geometry Component Implementation
 * \{ */

MeshComponent::MeshComponent() : GeometryComponent(GEO_COMPONENT_TYPE_MESH)
{
}

MeshComponent::~MeshComponent()
{
  this->clear();
}

GeometryComponent *MeshComponent::copy() const
{
  MeshComponent *new_component = new MeshComponent();
  if (mesh_ != nullptr) {
    new_component->mesh_ = BKE_mesh_copy_for_eval(mesh_, false);
    new_component->ownership_ = GeometryOwnershipType::Owned;
    new_component->vertex_group_names_ = blender::Map(vertex_group_names_);
  }
  return new_component;
}

void MeshComponent::clear()
{
  BLI_assert(this->is_mutable());
  if (mesh_ != nullptr) {
    if (ownership_ == GeometryOwnershipType::Owned) {
      BKE_id_free(nullptr, mesh_);
    }
    mesh_ = nullptr;
  }
  vertex_group_names_.clear();
}

bool MeshComponent::has_mesh() const
{
  return mesh_ != nullptr;
}

/* Clear the component and replace it with the new mesh. */
void MeshComponent::replace(Mesh *mesh, GeometryOwnershipType ownership)
{
  BLI_assert(this->is_mutable());
  this->clear();
  mesh_ = mesh;
  ownership_ = ownership;
}

/* This function exists for the same reason as #vertex_group_names_. Non-nodes modifiers need to
 * be able to replace the mesh data without losing the vertex group names, which may have come
 * from another object. */
void MeshComponent::replace_mesh_but_keep_vertex_group_names(Mesh *mesh,
                                                             GeometryOwnershipType ownership)
{
  BLI_assert(this->is_mutable());
  if (mesh_ != nullptr) {
    if (ownership_ == GeometryOwnershipType::Owned) {
      BKE_id_free(nullptr, mesh_);
    }
    mesh_ = nullptr;
  }
  mesh_ = mesh;
  ownership_ = ownership;
}

/* Return the mesh and clear the component. The caller takes over responsibility for freeing the
 * mesh (if the component was responsible before). */
Mesh *MeshComponent::release()
{
  BLI_assert(this->is_mutable());
  Mesh *mesh = mesh_;
  mesh_ = nullptr;
  return mesh;
}

void MeshComponent::copy_vertex_group_names_from_object(const Object &object)
{
  BLI_assert(this->is_mutable());
  vertex_group_names_.clear();
  int index = 0;
  LISTBASE_FOREACH (const bDeformGroup *, group, &object.defbase) {
    vertex_group_names_.add(group->name, index);
    index++;
  }
}

const blender::Map<std::string, int> &MeshComponent::vertex_group_names() const
{
  return vertex_group_names_;
}

/* This is only exposed for the internal attribute API. */
blender::Map<std::string, int> &MeshComponent::vertex_group_names()
{
  return vertex_group_names_;
}

/* Get the mesh from this component. This method can be used by multiple threads at the same
 * time. Therefore, the returned mesh should not be modified. No ownership is transferred. */
const Mesh *MeshComponent::get_for_read() const
{
  return mesh_;
}

/* Get the mesh from this component. This method can only be used when the component is mutable,
 * i.e. it is not shared. The returned mesh can be modified. No ownership is transferred. */
Mesh *MeshComponent::get_for_write()
{
  BLI_assert(this->is_mutable());
  if (ownership_ == GeometryOwnershipType::ReadOnly) {
    mesh_ = BKE_mesh_copy_for_eval(mesh_, false);
    ownership_ = GeometryOwnershipType::Owned;
  }
  return mesh_;
}

bool MeshComponent::is_empty() const
{
  return mesh_ == nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Attribute Access
 * \{ */

int MeshComponent::attribute_domain_size(const AttributeDomain domain) const
{
  BLI_assert(this->attribute_domain_supported(domain));
  if (mesh_ == nullptr) {
    return 0;
  }
  switch (domain) {
    case ATTR_DOMAIN_CORNER:
      return mesh_->totloop;
    case ATTR_DOMAIN_POINT:
      return mesh_->totvert;
    case ATTR_DOMAIN_EDGE:
      return mesh_->totedge;
    case ATTR_DOMAIN_POLYGON:
      return mesh_->totpoly;
    default:
      BLI_assert(false);
      break;
  }
  return 0;
}

namespace blender::bke {

template<typename T>
static void adapt_mesh_domain_corner_to_point_impl(const Mesh &mesh,
                                                   const TypedReadAttribute<T> &attribute,
                                                   MutableSpan<T> r_values)
{
  BLI_assert(r_values.size() == mesh.totvert);
  attribute_math::DefaultMixer<T> mixer(r_values);

  for (const int loop_index : IndexRange(mesh.totloop)) {
    const T value = attribute[loop_index];
    const MLoop &loop = mesh.mloop[loop_index];
    const int point_index = loop.v;
    mixer.mix_in(point_index, value);
  }
  mixer.finalize();
}

static ReadAttributePtr adapt_mesh_domain_corner_to_point(const Mesh &mesh,
                                                          ReadAttributePtr attribute)
{
  ReadAttributePtr new_attribute;
  const CustomDataType data_type = attribute->custom_data_type();
  attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<attribute_math::DefaultMixer<T>>) {
      /* We compute all interpolated values at once, because for this interpolation, one has to
       * iterate over all loops anyway. */
      Array<T> values(mesh.totvert);
      adapt_mesh_domain_corner_to_point_impl<T>(mesh, *attribute, values);
      new_attribute = std::make_unique<OwnedArrayReadAttribute<T>>(ATTR_DOMAIN_POINT,
                                                                   std::move(values));
    }
  });
  return new_attribute;
}

template<typename T>
static void adapt_mesh_domain_point_to_corner_impl(const Mesh &mesh,
                                                   const TypedReadAttribute<T> &attribute,
                                                   MutableSpan<T> r_values)
{
  BLI_assert(r_values.size() == mesh.totloop);

  for (const int loop_index : IndexRange(mesh.totloop)) {
    const int vertex_index = mesh.mloop[loop_index].v;
    r_values[loop_index] = attribute[vertex_index];
  }
}

static ReadAttributePtr adapt_mesh_domain_point_to_corner(const Mesh &mesh,
                                                          ReadAttributePtr attribute)
{
  ReadAttributePtr new_attribute;
  const CustomDataType data_type = attribute->custom_data_type();
  attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
    using T = decltype(dummy);
    /* It is not strictly necessary to compute the value for all corners here. Instead one could
     * lazily lookup the mesh topology when a specific index accessed. This can be more efficient
     * when an algorithm only accesses very few of the corner values. However, for the algorithms
     * we currently have, precomputing the array is fine. Also, it is easier to implement. */
    Array<T> values(mesh.totloop);
    adapt_mesh_domain_point_to_corner_impl<T>(mesh, *attribute, values);
    new_attribute = std::make_unique<OwnedArrayReadAttribute<T>>(ATTR_DOMAIN_CORNER,
                                                                 std::move(values));
  });
  return new_attribute;
}

/**
 * \note Theoretically this interpolation does not need to compute all values at once.
 * However, doing that makes the implementation simpler, and this can be optimized in the future if
 * only some values are required.
 */
template<typename T>
static void adapt_mesh_domain_corner_to_polygon_impl(const Mesh &mesh,
                                                     Span<T> old_values,
                                                     MutableSpan<T> r_values)
{
  BLI_assert(r_values.size() == mesh.totpoly);
  attribute_math::DefaultMixer<T> mixer(r_values);

  for (const int poly_index : IndexRange(mesh.totpoly)) {
    const MPoly &poly = mesh.mpoly[poly_index];
    for (const int loop_index : IndexRange(poly.loopstart, poly.totloop)) {
      const T value = old_values[loop_index];
      mixer.mix_in(poly_index, value);
    }
  }

  mixer.finalize();
}

static ReadAttributePtr adapt_mesh_domain_corner_to_polygon(const Mesh &mesh,
                                                            ReadAttributePtr attribute)
{
  ReadAttributePtr new_attribute;
  const CustomDataType data_type = attribute->custom_data_type();
  attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<attribute_math::DefaultMixer<T>>) {
      Array<T> values(mesh.totpoly);
      adapt_mesh_domain_corner_to_polygon_impl<T>(mesh, attribute->get_span<T>(), values);
      new_attribute = std::make_unique<OwnedArrayReadAttribute<T>>(ATTR_DOMAIN_POINT,
                                                                   std::move(values));
    }
  });
  return new_attribute;
}

template<typename T>
void adapt_mesh_domain_polygon_to_point_impl(const Mesh &mesh,
                                             Span<T> old_values,
                                             MutableSpan<T> r_values)
{
  BLI_assert(r_values.size() == mesh.totvert);
  attribute_math::DefaultMixer<T> mixer(r_values);

  for (const int poly_index : IndexRange(mesh.totpoly)) {
    const MPoly &poly = mesh.mpoly[poly_index];
    const T value = old_values[poly_index];
    for (const int loop_index : IndexRange(poly.loopstart, poly.totloop)) {
      const MLoop &loop = mesh.mloop[loop_index];
      const int point_index = loop.v;
      mixer.mix_in(point_index, value);
    }
  }

  mixer.finalize();
}

static ReadAttributePtr adapt_mesh_domain_polygon_to_point(const Mesh &mesh,
                                                           ReadAttributePtr attribute)
{
  ReadAttributePtr new_attribute;
  const CustomDataType data_type = attribute->custom_data_type();
  attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<attribute_math::DefaultMixer<T>>) {
      Array<T> values(mesh.totvert);
      adapt_mesh_domain_polygon_to_point_impl<T>(mesh, attribute->get_span<T>(), values);
      new_attribute = std::make_unique<OwnedArrayReadAttribute<T>>(ATTR_DOMAIN_POINT,
                                                                   std::move(values));
    }
  });
  return new_attribute;
}

template<typename T>
void adapt_mesh_domain_polygon_to_corner_impl(const Mesh &mesh,
                                              const Span<T> old_values,
                                              MutableSpan<T> r_values)
{
  BLI_assert(r_values.size() == mesh.totloop);

  for (const int poly_index : IndexRange(mesh.totpoly)) {
    const MPoly &poly = mesh.mpoly[poly_index];
    MutableSpan<T> poly_corner_values = r_values.slice(poly.loopstart, poly.totloop);
    poly_corner_values.fill(old_values[poly_index]);
  }
}

static ReadAttributePtr adapt_mesh_domain_polygon_to_corner(const Mesh &mesh,
                                                            ReadAttributePtr attribute)
{
  ReadAttributePtr new_attribute;
  const CustomDataType data_type = attribute->custom_data_type();
  attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<attribute_math::DefaultMixer<T>>) {
      Array<T> values(mesh.totloop);
      adapt_mesh_domain_polygon_to_corner_impl<T>(mesh, attribute->get_span<T>(), values);
      new_attribute = std::make_unique<OwnedArrayReadAttribute<T>>(ATTR_DOMAIN_POINT,
                                                                   std::move(values));
    }
  });
  return new_attribute;
}

/**
 * \note Theoretically this interpolation does not need to compute all values at once.
 * However, doing that makes the implementation simpler, and this can be optimized in the future if
 * only some values are required.
 */
template<typename T>
static void adapt_mesh_domain_point_to_polygon_impl(const Mesh &mesh,
                                                    const Span<T> old_values,
                                                    MutableSpan<T> r_values)
{
  BLI_assert(r_values.size() == mesh.totpoly);
  attribute_math::DefaultMixer<T> mixer(r_values);

  for (const int poly_index : IndexRange(mesh.totpoly)) {
    const MPoly &poly = mesh.mpoly[poly_index];
    for (const int loop_index : IndexRange(poly.loopstart, poly.totloop)) {
      MLoop &loop = mesh.mloop[loop_index];
      const int point_index = loop.v;
      mixer.mix_in(poly_index, old_values[point_index]);
    }
  }
  mixer.finalize();
}

static ReadAttributePtr adapt_mesh_domain_point_to_polygon(const Mesh &mesh,
                                                           ReadAttributePtr attribute)
{
  ReadAttributePtr new_attribute;
  const CustomDataType data_type = attribute->custom_data_type();
  attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<attribute_math::DefaultMixer<T>>) {
      Array<T> values(mesh.totpoly);
      adapt_mesh_domain_point_to_polygon_impl<T>(mesh, attribute->get_span<T>(), values);
      new_attribute = std::make_unique<OwnedArrayReadAttribute<T>>(ATTR_DOMAIN_POINT,
                                                                   std::move(values));
    }
  });
  return new_attribute;
}

}  // namespace blender::bke

ReadAttributePtr MeshComponent::attribute_try_adapt_domain(ReadAttributePtr attribute,
                                                           const AttributeDomain new_domain) const
{
  if (!attribute) {
    return {};
  }
  if (attribute->size() == 0) {
    return {};
  }
  const AttributeDomain old_domain = attribute->domain();
  if (old_domain == new_domain) {
    return attribute;
  }

  switch (old_domain) {
    case ATTR_DOMAIN_CORNER: {
      switch (new_domain) {
        case ATTR_DOMAIN_POINT:
          return blender::bke::adapt_mesh_domain_corner_to_point(*mesh_, std::move(attribute));
        case ATTR_DOMAIN_POLYGON:
          return blender::bke::adapt_mesh_domain_corner_to_polygon(*mesh_, std::move(attribute));
        default:
          break;
      }
      break;
    }
    case ATTR_DOMAIN_POINT: {
      switch (new_domain) {
        case ATTR_DOMAIN_CORNER:
          return blender::bke::adapt_mesh_domain_point_to_corner(*mesh_, std::move(attribute));
        case ATTR_DOMAIN_POLYGON:
          return blender::bke::adapt_mesh_domain_point_to_polygon(*mesh_, std::move(attribute));
        default:
          break;
      }
      break;
    }
    case ATTR_DOMAIN_POLYGON: {
      switch (new_domain) {
        case ATTR_DOMAIN_POINT:
          return blender::bke::adapt_mesh_domain_polygon_to_point(*mesh_, std::move(attribute));
        case ATTR_DOMAIN_CORNER:
          return blender::bke::adapt_mesh_domain_polygon_to_corner(*mesh_, std::move(attribute));
        default:
          break;
      }
      break;
    }
    default:
      break;
  }

  return {};
}

static Mesh *get_mesh_from_component_for_write(GeometryComponent &component)
{
  BLI_assert(component.type() == GEO_COMPONENT_TYPE_MESH);
  MeshComponent &mesh_component = static_cast<MeshComponent &>(component);
  return mesh_component.get_for_write();
}

static const Mesh *get_mesh_from_component_for_read(const GeometryComponent &component)
{
  BLI_assert(component.type() == GEO_COMPONENT_TYPE_MESH);
  const MeshComponent &mesh_component = static_cast<const MeshComponent &>(component);
  return mesh_component.get_for_read();
}

namespace blender::bke {

static float3 get_vertex_position(const MVert &vert)
{
  return float3(vert.co);
}

static void set_vertex_position(MVert &vert, const float3 &position)
{
  copy_v3_v3(vert.co, position);
}

static ReadAttributePtr make_vertex_position_read_attribute(const void *data,
                                                            const int domain_size)
{
  return std::make_unique<DerivedArrayReadAttribute<MVert, float3, get_vertex_position>>(
      ATTR_DOMAIN_POINT, Span<MVert>((const MVert *)data, domain_size));
}

static WriteAttributePtr make_vertex_position_write_attribute(void *data, const int domain_size)
{
  return std::make_unique<
      DerivedArrayWriteAttribute<MVert, float3, get_vertex_position, set_vertex_position>>(
      ATTR_DOMAIN_POINT, MutableSpan<MVert>((MVert *)data, domain_size));
}

static void tag_normals_dirty_when_writing_position(GeometryComponent &component)
{
  Mesh *mesh = get_mesh_from_component_for_write(component);
  if (mesh != nullptr) {
    mesh->runtime.cd_dirty_vert |= CD_MASK_NORMAL;
  }
}

static int get_material_index(const MPoly &mpoly)
{
  return static_cast<int>(mpoly.mat_nr);
}

static void set_material_index(MPoly &mpoly, const int &index)
{
  mpoly.mat_nr = static_cast<short>(std::clamp(index, 0, SHRT_MAX));
}

static ReadAttributePtr make_material_index_read_attribute(const void *data, const int domain_size)
{
  return std::make_unique<DerivedArrayReadAttribute<MPoly, int, get_material_index>>(
      ATTR_DOMAIN_POLYGON, Span<MPoly>((const MPoly *)data, domain_size));
}

static WriteAttributePtr make_material_index_write_attribute(void *data, const int domain_size)
{
  return std::make_unique<
      DerivedArrayWriteAttribute<MPoly, int, get_material_index, set_material_index>>(
      ATTR_DOMAIN_POLYGON, MutableSpan<MPoly>((MPoly *)data, domain_size));
}

static float3 get_vertex_normal(const MVert &vert)
{
  float3 result;
  normal_short_to_float_v3(result, vert.no);
  return result;
}

static ReadAttributePtr make_vertex_normal_read_attribute(const void *data, const int domain_size)
{
  return std::make_unique<DerivedArrayReadAttribute<MVert, float3, get_vertex_normal>>(
      ATTR_DOMAIN_POINT, Span<MVert>((const MVert *)data, domain_size));
}

static void update_vertex_normals_when_dirty(const GeometryComponent &component)
{
  const Mesh *mesh = get_mesh_from_component_for_read(component);
  if (mesh == nullptr) {
    return;
  }

  /* Since normals are derived data, const write access to them is okay. However, ensure that
   * two threads don't use write normals to a mesh at the same time. Note that this relies on
   * the idempotence of the operation; calculating the normals just fills the MVert struct
   * rather than allocating new memory. */
  if (mesh->runtime.cd_dirty_vert & CD_MASK_NORMAL) {
    ThreadMutex *mesh_eval_mutex = (ThreadMutex *)mesh->runtime.eval_mutex;
    BLI_mutex_lock(mesh_eval_mutex);

    /* Check again to avoid a second thread needlessly recalculating the same normals. */
    if (mesh->runtime.cd_dirty_vert & CD_MASK_NORMAL) {
      BKE_mesh_calc_normals(const_cast<Mesh *>(mesh));
    }

    BLI_mutex_unlock(mesh_eval_mutex);
  }
}

static bool get_shade_smooth(const MPoly &mpoly)
{
  return mpoly.flag & ME_SMOOTH;
}

static void set_shade_smooth(MPoly &mpoly, const bool &value)
{
  SET_FLAG_FROM_TEST(mpoly.flag, value, ME_SMOOTH);
}

static ReadAttributePtr make_shade_smooth_read_attribute(const void *data, const int domain_size)
{
  return std::make_unique<DerivedArrayReadAttribute<MPoly, bool, get_shade_smooth>>(
      ATTR_DOMAIN_POLYGON, Span<MPoly>((const MPoly *)data, domain_size));
}

static WriteAttributePtr make_shade_smooth_write_attribute(void *data, const int domain_size)
{
  return std::make_unique<
      DerivedArrayWriteAttribute<MPoly, bool, get_shade_smooth, set_shade_smooth>>(
      ATTR_DOMAIN_POLYGON, MutableSpan<MPoly>((MPoly *)data, domain_size));
}

static float2 get_loop_uv(const MLoopUV &uv)
{
  return float2(uv.uv);
}

static void set_loop_uv(MLoopUV &uv, const float2 &co)
{
  copy_v2_v2(uv.uv, co);
}

static ReadAttributePtr make_uvs_read_attribute(const void *data, const int domain_size)
{
  return std::make_unique<DerivedArrayReadAttribute<MLoopUV, float2, get_loop_uv>>(
      ATTR_DOMAIN_CORNER, Span((const MLoopUV *)data, domain_size));
}

static WriteAttributePtr make_uvs_write_attribute(void *data, const int domain_size)
{
  return std::make_unique<DerivedArrayWriteAttribute<MLoopUV, float2, get_loop_uv, set_loop_uv>>(
      ATTR_DOMAIN_CORNER, MutableSpan((MLoopUV *)data, domain_size));
}

static Color4f get_loop_color(const MLoopCol &col)
{
  Color4f value;
  rgba_uchar_to_float(value, &col.r);
  return value;
}

static void set_loop_color(MLoopCol &col, const Color4f &value)
{
  rgba_float_to_uchar(&col.r, value);
}

static ReadAttributePtr make_vertex_color_read_attribute(const void *data, const int domain_size)
{
  return std::make_unique<DerivedArrayReadAttribute<MLoopCol, Color4f, get_loop_color>>(
      ATTR_DOMAIN_CORNER, Span((const MLoopCol *)data, domain_size));
}

static WriteAttributePtr make_vertex_color_write_attribute(void *data, const int domain_size)
{
  return std::make_unique<
      DerivedArrayWriteAttribute<MLoopCol, Color4f, get_loop_color, set_loop_color>>(
      ATTR_DOMAIN_CORNER, MutableSpan((MLoopCol *)data, domain_size));
}

class VertexWeightWriteAttribute final : public WriteAttribute {
 private:
  MDeformVert *dverts_;
  const int dvert_index_;

 public:
  VertexWeightWriteAttribute(MDeformVert *dverts, const int totvert, const int dvert_index)
      : WriteAttribute(ATTR_DOMAIN_POINT, CPPType::get<float>(), totvert),
        dverts_(dverts),
        dvert_index_(dvert_index)
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    get_internal(dverts_, dvert_index_, index, r_value);
  }

  void set_internal(const int64_t index, const void *value) override
  {
    MDeformWeight *weight = BKE_defvert_ensure_index(&dverts_[index], dvert_index_);
    weight->weight = *reinterpret_cast<const float *>(value);
  }

  static void get_internal(const MDeformVert *dverts,
                           const int dvert_index,
                           const int64_t index,
                           void *r_value)
  {
    if (dverts == nullptr) {
      *(float *)r_value = 0.0f;
      return;
    }
    const MDeformVert &dvert = dverts[index];
    for (const MDeformWeight &weight : Span(dvert.dw, dvert.totweight)) {
      if (weight.def_nr == dvert_index) {
        *(float *)r_value = weight.weight;
        return;
      }
    }
    *(float *)r_value = 0.0f;
  }
};

class VertexWeightReadAttribute final : public ReadAttribute {
 private:
  const MDeformVert *dverts_;
  const int dvert_index_;

 public:
  VertexWeightReadAttribute(const MDeformVert *dverts, const int totvert, const int dvert_index)
      : ReadAttribute(ATTR_DOMAIN_POINT, CPPType::get<float>(), totvert),
        dverts_(dverts),
        dvert_index_(dvert_index)
  {
  }

  void get_internal(const int64_t index, void *r_value) const override
  {
    VertexWeightWriteAttribute::get_internal(dverts_, dvert_index_, index, r_value);
  }
};

/**
 * This provider makes vertex groups available as float attributes.
 */
class VertexGroupsAttributeProvider final : public DynamicAttributesProvider {
 public:
  ReadAttributePtr try_get_for_read(const GeometryComponent &component,
                                    const StringRef attribute_name) const final
  {
    BLI_assert(component.type() == GEO_COMPONENT_TYPE_MESH);
    const MeshComponent &mesh_component = static_cast<const MeshComponent &>(component);
    const Mesh *mesh = mesh_component.get_for_read();
    const int vertex_group_index = mesh_component.vertex_group_names().lookup_default_as(
        attribute_name, -1);
    if (vertex_group_index < 0) {
      return {};
    }
    if (mesh == nullptr || mesh->dvert == nullptr) {
      static const float default_value = 0.0f;
      return std::make_unique<ConstantReadAttribute>(
          ATTR_DOMAIN_POINT, mesh->totvert, CPPType::get<float>(), &default_value);
    }
    return std::make_unique<VertexWeightReadAttribute>(
        mesh->dvert, mesh->totvert, vertex_group_index);
  }

  WriteAttributePtr try_get_for_write(GeometryComponent &component,
                                      const StringRef attribute_name) const final
  {
    BLI_assert(component.type() == GEO_COMPONENT_TYPE_MESH);
    MeshComponent &mesh_component = static_cast<MeshComponent &>(component);
    Mesh *mesh = mesh_component.get_for_write();
    if (mesh == nullptr) {
      return {};
    }
    const int vertex_group_index = mesh_component.vertex_group_names().lookup_default_as(
        attribute_name, -1);
    if (vertex_group_index < 0) {
      return {};
    }
    if (mesh->dvert == nullptr) {
      BKE_object_defgroup_data_create(&mesh->id);
    }
    else {
      /* Copy the data layer if it is shared with some other mesh. */
      mesh->dvert = (MDeformVert *)CustomData_duplicate_referenced_layer(
          &mesh->vdata, CD_MDEFORMVERT, mesh->totvert);
    }
    return std::make_unique<blender::bke::VertexWeightWriteAttribute>(
        mesh->dvert, mesh->totvert, vertex_group_index);
  }

  bool try_delete(GeometryComponent &component, const StringRef attribute_name) const final
  {
    BLI_assert(component.type() == GEO_COMPONENT_TYPE_MESH);
    MeshComponent &mesh_component = static_cast<MeshComponent &>(component);

    const int vertex_group_index = mesh_component.vertex_group_names().pop_default_as(
        attribute_name, -1);
    if (vertex_group_index < 0) {
      return false;
    }
    Mesh *mesh = mesh_component.get_for_write();
    if (mesh == nullptr) {
      return true;
    }
    if (mesh->dvert == nullptr) {
      return true;
    }
    for (MDeformVert &dvert : MutableSpan(mesh->dvert, mesh->totvert)) {
      MDeformWeight *weight = BKE_defvert_find_index(&dvert, vertex_group_index);
      BKE_defvert_remove_group(&dvert, weight);
    }
    return true;
  }

  bool foreach_attribute(const GeometryComponent &component,
                         const AttributeForeachCallback callback) const final
  {
    BLI_assert(component.type() == GEO_COMPONENT_TYPE_MESH);
    const MeshComponent &mesh_component = static_cast<const MeshComponent &>(component);
    for (const auto item : mesh_component.vertex_group_names().items()) {
      const StringRefNull name = item.key;
      const int vertex_group_index = item.value;
      if (vertex_group_index >= 0) {
        AttributeMetaData meta_data{ATTR_DOMAIN_POINT, CD_PROP_FLOAT};
        if (!callback(name, meta_data)) {
          return false;
        }
      }
    }
    return true;
  }

  void supported_domains(Vector<AttributeDomain> &r_domains) const final
  {
    r_domains.append_non_duplicates(ATTR_DOMAIN_POINT);
  }
};

/**
 * In this function all the attribute providers for a mesh component are created. Most data in this
 * function is statically allocated, because it does not change over time.
 */
static ComponentAttributeProviders create_attribute_providers_for_mesh()
{
  static auto update_custom_data_pointers = [](GeometryComponent &component) {
    Mesh *mesh = get_mesh_from_component_for_write(component);
    if (mesh != nullptr) {
      BKE_mesh_update_customdata_pointers(mesh, false);
    }
  };

#define MAKE_MUTABLE_CUSTOM_DATA_GETTER(NAME) \
  [](GeometryComponent &component) -> CustomData * { \
    Mesh *mesh = get_mesh_from_component_for_write(component); \
    return mesh ? &mesh->NAME : nullptr; \
  }
#define MAKE_CONST_CUSTOM_DATA_GETTER(NAME) \
  [](const GeometryComponent &component) -> const CustomData * { \
    const Mesh *mesh = get_mesh_from_component_for_read(component); \
    return mesh ? &mesh->NAME : nullptr; \
  }

  static CustomDataAccessInfo corner_access = {MAKE_MUTABLE_CUSTOM_DATA_GETTER(ldata),
                                               MAKE_CONST_CUSTOM_DATA_GETTER(ldata),
                                               update_custom_data_pointers};
  static CustomDataAccessInfo point_access = {MAKE_MUTABLE_CUSTOM_DATA_GETTER(vdata),
                                              MAKE_CONST_CUSTOM_DATA_GETTER(vdata),
                                              update_custom_data_pointers};
  static CustomDataAccessInfo edge_access = {MAKE_MUTABLE_CUSTOM_DATA_GETTER(edata),
                                             MAKE_CONST_CUSTOM_DATA_GETTER(edata),
                                             update_custom_data_pointers};
  static CustomDataAccessInfo polygon_access = {MAKE_MUTABLE_CUSTOM_DATA_GETTER(pdata),
                                                MAKE_CONST_CUSTOM_DATA_GETTER(pdata),
                                                update_custom_data_pointers};

#undef MAKE_CONST_CUSTOM_DATA_GETTER
#undef MAKE_MUTABLE_CUSTOM_DATA_GETTER

  static BuiltinCustomDataLayerProvider position("position",
                                                 ATTR_DOMAIN_POINT,
                                                 CD_PROP_FLOAT3,
                                                 CD_MVERT,
                                                 BuiltinAttributeProvider::NonCreatable,
                                                 BuiltinAttributeProvider::Writable,
                                                 BuiltinAttributeProvider::NonDeletable,
                                                 point_access,
                                                 make_vertex_position_read_attribute,
                                                 make_vertex_position_write_attribute,
                                                 nullptr,
                                                 tag_normals_dirty_when_writing_position);

  static BuiltinCustomDataLayerProvider material_index("material_index",
                                                       ATTR_DOMAIN_POLYGON,
                                                       CD_PROP_INT32,
                                                       CD_MPOLY,
                                                       BuiltinAttributeProvider::NonCreatable,
                                                       BuiltinAttributeProvider::Writable,
                                                       BuiltinAttributeProvider::NonDeletable,
                                                       polygon_access,
                                                       make_material_index_read_attribute,
                                                       make_material_index_write_attribute,
                                                       nullptr,
                                                       nullptr);

  static BuiltinCustomDataLayerProvider shade_smooth("shade_smooth",
                                                     ATTR_DOMAIN_POLYGON,
                                                     CD_PROP_BOOL,
                                                     CD_MPOLY,
                                                     BuiltinAttributeProvider::NonCreatable,
                                                     BuiltinAttributeProvider::Writable,
                                                     BuiltinAttributeProvider::NonDeletable,
                                                     polygon_access,
                                                     make_shade_smooth_read_attribute,
                                                     make_shade_smooth_write_attribute,
                                                     nullptr,
                                                     nullptr);

  static BuiltinCustomDataLayerProvider vertex_normal("vertex_normal",
                                                      ATTR_DOMAIN_POINT,
                                                      CD_PROP_FLOAT3,
                                                      CD_MVERT,
                                                      BuiltinAttributeProvider::NonCreatable,
                                                      BuiltinAttributeProvider::Readonly,
                                                      BuiltinAttributeProvider::NonDeletable,
                                                      point_access,
                                                      make_vertex_normal_read_attribute,
                                                      nullptr,
                                                      update_vertex_normals_when_dirty,
                                                      nullptr);

  static NamedLegacyCustomDataProvider uvs(ATTR_DOMAIN_CORNER,
                                           CD_PROP_FLOAT2,
                                           CD_MLOOPUV,
                                           corner_access,
                                           make_uvs_read_attribute,
                                           make_uvs_write_attribute);

  static NamedLegacyCustomDataProvider vertex_colors(ATTR_DOMAIN_CORNER,
                                                     CD_PROP_COLOR,
                                                     CD_MLOOPCOL,
                                                     corner_access,
                                                     make_vertex_color_read_attribute,
                                                     make_vertex_color_write_attribute);

  static VertexGroupsAttributeProvider vertex_groups;
  static CustomDataAttributeProvider corner_custom_data(ATTR_DOMAIN_CORNER, corner_access);
  static CustomDataAttributeProvider point_custom_data(ATTR_DOMAIN_POINT, point_access);
  static CustomDataAttributeProvider edge_custom_data(ATTR_DOMAIN_EDGE, edge_access);
  static CustomDataAttributeProvider polygon_custom_data(ATTR_DOMAIN_POLYGON, polygon_access);

  return ComponentAttributeProviders({&position, &material_index, &vertex_normal, &shade_smooth},
                                     {&uvs,
                                      &vertex_colors,
                                      &corner_custom_data,
                                      &vertex_groups,
                                      &point_custom_data,
                                      &edge_custom_data,
                                      &polygon_custom_data});
}

}  // namespace blender::bke

const blender::bke::ComponentAttributeProviders *MeshComponent::get_attribute_providers() const
{
  static blender::bke::ComponentAttributeProviders providers =
      blender::bke::create_attribute_providers_for_mesh();
  return &providers;
}

/** \} */

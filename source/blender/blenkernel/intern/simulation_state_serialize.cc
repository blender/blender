/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"
#include "BKE_instances.hh"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_mesh.hh"
#include "BKE_pointcloud.h"
#include "BKE_simulation_state_serialize.hh"

#include "DNA_material_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"

#include "BLI_endian_defines.h"
#include "BLI_endian_switch.h"
#include "BLI_fileops.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_quaternion_types.hh"
#include "BLI_path_util.h"

#include "RNA_access.h"
#include "RNA_enum_types.h"

namespace blender::bke::sim {

/**
 * Turn the name into something that can be used as file name. It does not necessarily have to be
 * human readable, but it can help if it is at least partially readable.
 */
static std::string escape_name(const StringRef name)
{
  std::stringstream ss;
  for (const char c : name) {
    /* Only some letters allowed. Digits are not because they could lead to name collisions. */
    if (('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z')) {
      ss << c;
    }
    else {
      ss << int(c);
    }
  }
  return ss.str();
}

static std::string get_blend_file_name(const Main &bmain)
{
  const StringRefNull blend_file_path = BKE_main_blendfile_path(&bmain);
  char blend_name[FILE_MAX];

  BLI_path_split_file_part(blend_file_path.c_str(), blend_name, sizeof(blend_name));
  const int64_t type_start_index = StringRef(blend_name).rfind(".");
  if (type_start_index == StringRef::not_found) {
    return "";
  }
  blend_name[type_start_index] = '\0';
  return "blendcache_" + StringRef(blend_name);
}

static std::string get_modifier_sim_name(const Object &object, const ModifierData &md)
{
  const std::string object_name_escaped = escape_name(object.id.name + 2);
  const std::string modifier_name_escaped = escape_name(md.name);
  return "sim_" + object_name_escaped + "_" + modifier_name_escaped;
}

std::string get_default_modifier_bake_directory(const Main &bmain,
                                                const Object &object,
                                                const ModifierData &md)
{
  char dir[FILE_MAX];
  /* Make path that's relative to the .blend file. */
  BLI_path_join(dir,
                sizeof(dir),
                "//",
                get_blend_file_name(bmain).c_str(),
                get_modifier_sim_name(object, md).c_str());
  return dir;
}

std::shared_ptr<DictionaryValue> BDataSlice::serialize() const
{
  auto io_slice = std::make_shared<DictionaryValue>();
  io_slice->append_str("name", this->name);
  io_slice->append_int("start", range.start());
  io_slice->append_int("size", range.size());
  return io_slice;
}

std::optional<BDataSlice> BDataSlice::deserialize(const DictionaryValue &io_slice)
{
  const std::optional<StringRefNull> name = io_slice.lookup_str("name");
  const std::optional<int64_t> start = io_slice.lookup_int("start");
  const std::optional<int64_t> size = io_slice.lookup_int("size");
  if (!name || !start || !size) {
    return std::nullopt;
  }

  return BDataSlice{*name, {*start, *size}};
}

static StringRefNull get_endian_io_name(const int endian)
{
  if (endian == L_ENDIAN) {
    return "little";
  }
  BLI_assert(endian == B_ENDIAN);
  return "big";
}

static StringRefNull get_domain_io_name(const eAttrDomain domain)
{
  const char *io_name = "unknown";
  RNA_enum_id_from_value(rna_enum_attribute_domain_items, domain, &io_name);
  return io_name;
}

static StringRefNull get_data_type_io_name(const eCustomDataType data_type)
{
  const char *io_name = "unknown";
  RNA_enum_id_from_value(rna_enum_attribute_type_items, data_type, &io_name);
  return io_name;
}

static std::optional<eAttrDomain> get_domain_from_io_name(const StringRefNull io_name)
{
  int domain;
  if (!RNA_enum_value_from_identifier(rna_enum_attribute_domain_items, io_name.c_str(), &domain)) {
    return std::nullopt;
  }
  return eAttrDomain(domain);
}

static std::optional<eCustomDataType> get_data_type_from_io_name(const StringRefNull io_name)
{
  int domain;
  if (!RNA_enum_value_from_identifier(rna_enum_attribute_type_items, io_name.c_str(), &domain)) {
    return std::nullopt;
  }
  return eCustomDataType(domain);
}

/**
 * Write the data and remember which endianness the data had.
 */
static std::shared_ptr<DictionaryValue> write_bdata_raw_data_with_endian(
    BDataWriter &bdata_writer, const void *data, const int64_t size_in_bytes)
{
  auto io_data = bdata_writer.write(data, size_in_bytes).serialize();
  if (ENDIAN_ORDER == B_ENDIAN) {
    io_data->append_str("endian", get_endian_io_name(ENDIAN_ORDER));
  }
  return io_data;
}

/**
 * Read data of an into an array and optionally perform an endian switch if necessary.
 */
[[nodiscard]] static bool read_bdata_raw_data_with_endian(const BDataReader &bdata_reader,
                                                          const DictionaryValue &io_data,
                                                          const int64_t element_size,
                                                          const int64_t elements_num,
                                                          void *r_data)
{
  const std::optional<BDataSlice> slice = BDataSlice::deserialize(io_data);
  if (!slice) {
    return false;
  }
  if (slice->range.size() != element_size * elements_num) {
    return false;
  }
  if (!bdata_reader.read(*slice, r_data)) {
    return false;
  }
  const StringRefNull stored_endian = io_data.lookup_str("endian").value_or("little");
  const StringRefNull current_endian = get_endian_io_name(ENDIAN_ORDER);
  const bool need_endian_switch = stored_endian != current_endian;
  if (need_endian_switch) {
    switch (element_size) {
      case 1:
        break;
      case 2:
        BLI_endian_switch_uint16_array(static_cast<uint16_t *>(r_data), elements_num);
        break;
      case 4:
        BLI_endian_switch_uint32_array(static_cast<uint32_t *>(r_data), elements_num);
        break;
      case 8:
        BLI_endian_switch_uint64_array(static_cast<uint64_t *>(r_data), elements_num);
        break;
      default:
        return false;
    }
  }
  return true;
}

/** Write bytes ignoring endianness. */
static std::shared_ptr<DictionaryValue> write_bdata_raw_bytes(BDataWriter &bdata_writer,
                                                              const void *data,
                                                              const int64_t size_in_bytes)
{
  return bdata_writer.write(data, size_in_bytes).serialize();
}

/** Read bytes ignoring endianness. */
[[nodiscard]] static bool read_bdata_raw_bytes(const BDataReader &bdata_reader,
                                               const DictionaryValue &io_data,
                                               const int64_t bytes_num,
                                               void *r_data)
{
  const std::optional<BDataSlice> slice = BDataSlice::deserialize(io_data);
  if (!slice) {
    return false;
  }
  if (slice->range.size() != bytes_num) {
    return false;
  }
  return bdata_reader.read(*slice, r_data);
}

static std::shared_ptr<DictionaryValue> write_bdata_simple_gspan(BDataWriter &bdata_writer,
                                                                 const GSpan data)
{
  const CPPType &type = data.type();
  BLI_assert(type.is_trivial());
  if (type.size() == 1 || type.is<ColorGeometry4b>()) {
    return write_bdata_raw_bytes(bdata_writer, data.data(), data.size_in_bytes());
  }
  return write_bdata_raw_data_with_endian(bdata_writer, data.data(), data.size_in_bytes());
}

[[nodiscard]] static bool read_bdata_simple_gspan(const BDataReader &bdata_reader,
                                                  const DictionaryValue &io_data,
                                                  GMutableSpan r_data)
{
  const CPPType &type = r_data.type();
  BLI_assert(type.is_trivial());
  if (type.size() == 1 || type.is<ColorGeometry4b>()) {
    return read_bdata_raw_bytes(bdata_reader, io_data, r_data.size_in_bytes(), r_data.data());
  }
  if (type.is_any<int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float>()) {
    return read_bdata_raw_data_with_endian(
        bdata_reader, io_data, type.size(), r_data.size(), r_data.data());
  }
  if (type.is_any<float2, int2>()) {
    return read_bdata_raw_data_with_endian(
        bdata_reader, io_data, sizeof(int32_t), r_data.size() * 2, r_data.data());
  }
  if (type.is<float3>()) {
    return read_bdata_raw_data_with_endian(
        bdata_reader, io_data, sizeof(float), r_data.size() * 3, r_data.data());
  }
  if (type.is<float4x4>()) {
    return read_bdata_raw_data_with_endian(
        bdata_reader, io_data, sizeof(float), r_data.size() * 16, r_data.data());
  }
  if (type.is<ColorGeometry4f>()) {
    return read_bdata_raw_data_with_endian(
        bdata_reader, io_data, sizeof(float), r_data.size() * 4, r_data.data());
  }
  return false;
}

static std::shared_ptr<DictionaryValue> write_bdata_shared_simple_gspan(
    BDataWriter &bdata_writer,
    BDataSharing &bdata_sharing,
    const GSpan data,
    const ImplicitSharingInfo *sharing_info)
{
  return bdata_sharing.write_shared(
      sharing_info, [&]() { return write_bdata_simple_gspan(bdata_writer, data); });
}

[[nodiscard]] static const void *read_bdata_shared_simple_gspan(
    const DictionaryValue &io_data,
    const BDataReader &bdata_reader,
    const BDataSharing &bdata_sharing,
    const CPPType &cpp_type,
    const int size,
    const ImplicitSharingInfo **r_sharing_info)
{
  const std::optional<ImplicitSharingInfoAndData> sharing_info_and_data =
      bdata_sharing.read_shared(io_data, [&]() -> std::optional<ImplicitSharingInfoAndData> {
        void *data_mem = MEM_mallocN_aligned(
            size * cpp_type.size(), cpp_type.alignment(), __func__);
        if (!read_bdata_simple_gspan(bdata_reader, io_data, {cpp_type, data_mem, size})) {
          MEM_freeN(data_mem);
          return std::nullopt;
        }
        return ImplicitSharingInfoAndData{implicit_sharing::info_for_mem_free(data_mem), data_mem};
      });
  if (!sharing_info_and_data) {
    *r_sharing_info = nullptr;
    return nullptr;
  }
  *r_sharing_info = sharing_info_and_data->sharing_info;
  return sharing_info_and_data->data;
}

template<typename T>
[[nodiscard]] static bool read_bdata_shared_simple_span(const DictionaryValue &io_data,
                                                        const BDataReader &bdata_reader,
                                                        const BDataSharing &bdata_sharing,
                                                        const int size,
                                                        T **r_data,
                                                        const ImplicitSharingInfo **r_sharing_info)
{
  *r_data = const_cast<T *>(static_cast<const T *>(read_bdata_shared_simple_gspan(
      io_data, bdata_reader, bdata_sharing, CPPType::get<T>(), size, r_sharing_info)));
  return *r_data != nullptr;
}

[[nodiscard]] static bool load_attributes(const io::serialize::ArrayValue &io_attributes,
                                          bke::MutableAttributeAccessor &attributes,
                                          const BDataReader &bdata_reader,
                                          const BDataSharing &bdata_sharing)
{
  for (const auto &io_attribute_value : io_attributes.elements()) {
    const auto *io_attribute = io_attribute_value->as_dictionary_value();
    if (!io_attribute) {
      return false;
    }
    const std::optional<StringRefNull> name = io_attribute->lookup_str("name");
    const std::optional<StringRefNull> domain_str = io_attribute->lookup_str("domain");
    const std::optional<StringRefNull> type_str = io_attribute->lookup_str("type");
    auto io_data = io_attribute->lookup_dict("data");
    if (!name || !domain_str || !type_str || !io_data) {
      return false;
    }

    const std::optional<eAttrDomain> domain = get_domain_from_io_name(*domain_str);
    const std::optional<eCustomDataType> data_type = get_data_type_from_io_name(*type_str);
    if (!domain || !data_type) {
      return false;
    }
    const CPPType *cpp_type = custom_data_type_to_cpp_type(*data_type);
    if (!cpp_type) {
      return false;
    }
    const int domain_size = attributes.domain_size(*domain);
    const ImplicitSharingInfo *attribute_sharing_info;
    const void *attribute_data = read_bdata_shared_simple_gspan(
        *io_data, bdata_reader, bdata_sharing, *cpp_type, domain_size, &attribute_sharing_info);
    if (!attribute_data) {
      return false;
    }
    BLI_SCOPED_DEFER([&]() { attribute_sharing_info->remove_user_and_delete_if_last(); });

    if (attributes.contains(*name)) {
      /* If the attribute exists already, copy the values over to the existing array. */
      bke::GSpanAttributeWriter attribute = attributes.lookup_or_add_for_write_only_span(
          *name, *domain, *data_type);
      if (!attribute) {
        return false;
      }
      cpp_type->copy_assign_n(attribute_data, attribute.span.data(), domain_size);
      attribute.finish();
    }
    else {
      /* Add a new attribute that shares the data. */
      if (!attributes.add(*name,
                          *domain,
                          *data_type,
                          AttributeInitShared(attribute_data, *attribute_sharing_info)))
      {
        return false;
      }
    }
  }
  return true;
}

static PointCloud *try_load_pointcloud(const DictionaryValue &io_geometry,
                                       const BDataReader &bdata_reader,
                                       const BDataSharing &bdata_sharing)
{
  const DictionaryValue *io_pointcloud = io_geometry.lookup_dict("pointcloud");
  if (!io_pointcloud) {
    return nullptr;
  }
  const io::serialize::ArrayValue *io_attributes = io_pointcloud->lookup_array("attributes");
  if (!io_attributes) {
    return nullptr;
  }
  PointCloud *pointcloud = BKE_pointcloud_new_nomain(0);
  CustomData_free_layer_named(&pointcloud->pdata, "position", 0);
  pointcloud->totpoint = io_pointcloud->lookup_int("num_points").value_or(0);

  auto cancel = [&]() {
    BKE_id_free(nullptr, pointcloud);
    return nullptr;
  };

  bke::MutableAttributeAccessor attributes = pointcloud->attributes_for_write();
  if (!load_attributes(*io_attributes, attributes, bdata_reader, bdata_sharing)) {
    return cancel();
  }
  return pointcloud;
}

static Curves *try_load_curves(const DictionaryValue &io_geometry,
                               const BDataReader &bdata_reader,
                               const BDataSharing &bdata_sharing)
{
  const DictionaryValue *io_curves = io_geometry.lookup_dict("curves");
  if (!io_curves) {
    return nullptr;
  }

  const io::serialize::ArrayValue *io_attributes = io_curves->lookup_array("attributes");
  if (!io_attributes) {
    return nullptr;
  }

  Curves *curves_id = bke::curves_new_nomain(0, 0);
  bke::CurvesGeometry &curves = curves_id->geometry.wrap();
  CustomData_free_layer_named(&curves.point_data, "position", 0);
  curves.point_num = io_curves->lookup_int("num_points").value_or(0);
  curves.curve_num = io_curves->lookup_int("num_curves").value_or(0);

  auto cancel = [&]() {
    BKE_id_free(nullptr, curves_id);
    return nullptr;
  };

  if (curves.curves_num() > 0) {
    const auto io_curve_offsets = io_curves->lookup_dict("curve_offsets");
    if (!io_curve_offsets) {
      return cancel();
    }
    if (!read_bdata_shared_simple_span(*io_curve_offsets,
                                       bdata_reader,
                                       bdata_sharing,
                                       curves.curves_num() + 1,
                                       &curves.curve_offsets,
                                       &curves.runtime->curve_offsets_sharing_info))
    {
      return cancel();
    }
  }

  bke::MutableAttributeAccessor attributes = curves.attributes_for_write();
  if (!load_attributes(*io_attributes, attributes, bdata_reader, bdata_sharing)) {
    return cancel();
  }

  return curves_id;
}

static Mesh *try_load_mesh(const DictionaryValue &io_geometry,
                           const BDataReader &bdata_reader,
                           const BDataSharing &bdata_sharing)
{
  const DictionaryValue *io_mesh = io_geometry.lookup_dict("mesh");
  if (!io_mesh) {
    return nullptr;
  }

  const io::serialize::ArrayValue *io_attributes = io_mesh->lookup_array("attributes");
  if (!io_attributes) {
    return nullptr;
  }

  Mesh *mesh = BKE_mesh_new_nomain(0, 0, 0, 0);
  CustomData_free_layer_named(&mesh->vdata, "position", 0);
  CustomData_free_layer_named(&mesh->edata, ".edge_verts", 0);
  CustomData_free_layer_named(&mesh->ldata, ".corner_vert", 0);
  CustomData_free_layer_named(&mesh->ldata, ".corner_edge", 0);
  mesh->totvert = io_mesh->lookup_int("num_vertices").value_or(0);
  mesh->totedge = io_mesh->lookup_int("num_edges").value_or(0);
  mesh->totpoly = io_mesh->lookup_int("num_polygons").value_or(0);
  mesh->totloop = io_mesh->lookup_int("num_corners").value_or(0);

  auto cancel = [&]() {
    BKE_id_free(nullptr, mesh);
    return nullptr;
  };

  if (mesh->totpoly > 0) {
    const auto io_poly_offsets = io_mesh->lookup_dict("poly_offsets");
    if (!io_poly_offsets) {
      return cancel();
    }
    if (!read_bdata_shared_simple_span(*io_poly_offsets,
                                       bdata_reader,
                                       bdata_sharing,
                                       mesh->totpoly + 1,
                                       &mesh->poly_offset_indices,
                                       &mesh->runtime->poly_offsets_sharing_info))
    {
      return cancel();
    }
  }

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  if (!load_attributes(*io_attributes, attributes, bdata_reader, bdata_sharing)) {
    return cancel();
  }

  return mesh;
}

static GeometrySet load_geometry(const DictionaryValue &io_geometry,
                                 const BDataReader &bdata_reader,
                                 const BDataSharing &bdata_sharing);

static std::unique_ptr<bke::Instances> try_load_instances(const DictionaryValue &io_geometry,
                                                          const BDataReader &bdata_reader,
                                                          const BDataSharing &bdata_sharing)
{
  const DictionaryValue *io_instances = io_geometry.lookup_dict("instances");
  if (!io_instances) {
    return nullptr;
  }
  const int num_instances = io_instances->lookup_int("num_instances").value_or(0);
  if (num_instances == 0) {
    return nullptr;
  }
  const io::serialize::ArrayValue *io_attributes = io_instances->lookup_array("attributes");
  if (!io_attributes) {
    return nullptr;
  }
  const io::serialize::ArrayValue *io_references = io_instances->lookup_array("references");
  if (!io_references) {
    return nullptr;
  }

  std::unique_ptr<bke::Instances> instances = std::make_unique<bke::Instances>();
  instances->resize(num_instances);

  for (const auto &io_reference_value : io_references->elements()) {
    const DictionaryValue *io_reference = io_reference_value->as_dictionary_value();
    GeometrySet reference_geometry;
    if (io_reference) {
      reference_geometry = load_geometry(*io_reference, bdata_reader, bdata_sharing);
    }
    instances->add_reference(std::move(reference_geometry));
  }

  const auto io_transforms = io_instances->lookup_dict("transforms");
  if (!io_transforms) {
    return {};
  }
  if (!read_bdata_simple_gspan(bdata_reader, *io_transforms, instances->transforms())) {
    return {};
  }

  const auto io_handles = io_instances->lookup_dict("handles");
  if (!io_handles) {
    return {};
  }
  if (!read_bdata_simple_gspan(bdata_reader, *io_handles, instances->reference_handles())) {
    return {};
  }

  bke::MutableAttributeAccessor attributes = instances->attributes_for_write();
  if (!load_attributes(*io_attributes, attributes, bdata_reader, bdata_sharing)) {
    return {};
  }

  return instances;
}

static GeometrySet load_geometry(const DictionaryValue &io_geometry,
                                 const BDataReader &bdata_reader,
                                 const BDataSharing &bdata_sharing)
{
  GeometrySet geometry;
  geometry.replace_mesh(try_load_mesh(io_geometry, bdata_reader, bdata_sharing));
  geometry.replace_pointcloud(try_load_pointcloud(io_geometry, bdata_reader, bdata_sharing));
  geometry.replace_curves(try_load_curves(io_geometry, bdata_reader, bdata_sharing));
  geometry.replace_instances(
      try_load_instances(io_geometry, bdata_reader, bdata_sharing).release());
  return geometry;
}

static std::shared_ptr<io::serialize::ArrayValue> serialize_material_slots(
    const Span<const Material *> material_slots)
{
  auto io_materials = std::make_shared<io::serialize::ArrayValue>();
  for (const Material *material : material_slots) {
    if (material == nullptr) {
      io_materials->append_null();
    }
    else {
      auto io_material = io_materials->append_dict();
      io_material->append_str("name", material->id.name + 2);
      if (material->id.lib != nullptr) {
        io_material->append_str("lib_name", material->id.lib->id.name + 2);
      }
    }
  }
  return io_materials;
}

static std::shared_ptr<io::serialize::ArrayValue> serialize_attributes(
    const bke::AttributeAccessor &attributes,
    BDataWriter &bdata_writer,
    BDataSharing &bdata_sharing,
    const Set<std::string> &attributes_to_ignore)
{
  auto io_attributes = std::make_shared<io::serialize::ArrayValue>();
  attributes.for_all(
      [&](const bke::AttributeIDRef &attribute_id, const bke::AttributeMetaData &meta_data) {
        BLI_assert(!attribute_id.is_anonymous());
        if (attributes_to_ignore.contains_as(attribute_id.name())) {
          return true;
        }

        auto io_attribute = io_attributes->append_dict();

        io_attribute->append_str("name", attribute_id.name());

        const StringRefNull domain_name = get_domain_io_name(meta_data.domain);
        io_attribute->append_str("domain", domain_name);

        const StringRefNull type_name = get_data_type_io_name(meta_data.data_type);
        io_attribute->append_str("type", type_name);

        const bke::GAttributeReader attribute = attributes.lookup(attribute_id);
        const GVArraySpan attribute_span(attribute.varray);
        io_attribute->append("data",
                             write_bdata_shared_simple_gspan(
                                 bdata_writer,
                                 bdata_sharing,
                                 attribute_span,
                                 attribute.varray.is_span() ? attribute.sharing_info : nullptr));
        return true;
      });
  return io_attributes;
}

static std::shared_ptr<DictionaryValue> serialize_geometry_set(const GeometrySet &geometry,
                                                               BDataWriter &bdata_writer,
                                                               BDataSharing &bdata_sharing)
{
  auto io_geometry = std::make_shared<DictionaryValue>();
  if (geometry.has_mesh()) {
    const Mesh &mesh = *geometry.get_mesh_for_read();
    auto io_mesh = io_geometry->append_dict("mesh");

    io_mesh->append_int("num_vertices", mesh.totvert);
    io_mesh->append_int("num_edges", mesh.totedge);
    io_mesh->append_int("num_polygons", mesh.totpoly);
    io_mesh->append_int("num_corners", mesh.totloop);

    if (mesh.totpoly > 0) {
      io_mesh->append("poly_offsets",
                      write_bdata_shared_simple_gspan(bdata_writer,
                                                      bdata_sharing,
                                                      mesh.poly_offsets(),
                                                      mesh.runtime->poly_offsets_sharing_info));
    }

    auto io_materials = serialize_material_slots({mesh.mat, mesh.totcol});
    io_mesh->append("materials", io_materials);

    auto io_attributes = serialize_attributes(mesh.attributes(), bdata_writer, bdata_sharing, {});
    io_mesh->append("attributes", io_attributes);
  }
  if (geometry.has_pointcloud()) {
    const PointCloud &pointcloud = *geometry.get_pointcloud_for_read();
    auto io_pointcloud = io_geometry->append_dict("pointcloud");

    io_pointcloud->append_int("num_points", pointcloud.totpoint);

    auto io_materials = serialize_material_slots({pointcloud.mat, pointcloud.totcol});
    io_pointcloud->append("materials", io_materials);

    auto io_attributes = serialize_attributes(
        pointcloud.attributes(), bdata_writer, bdata_sharing, {});
    io_pointcloud->append("attributes", io_attributes);
  }
  if (geometry.has_curves()) {
    const Curves &curves_id = *geometry.get_curves_for_read();
    const bke::CurvesGeometry &curves = curves_id.geometry.wrap();

    auto io_curves = io_geometry->append_dict("curves");

    io_curves->append_int("num_points", curves.point_num);
    io_curves->append_int("num_curves", curves.curve_num);

    if (curves.curve_num > 0) {
      io_curves->append(
          "curve_offsets",
          write_bdata_shared_simple_gspan(bdata_writer,
                                          bdata_sharing,
                                          curves.offsets(),
                                          curves.runtime->curve_offsets_sharing_info));
    }

    auto io_materials = serialize_material_slots({curves_id.mat, curves_id.totcol});
    io_curves->append("materials", io_materials);

    auto io_attributes = serialize_attributes(
        curves.attributes(), bdata_writer, bdata_sharing, {});
    io_curves->append("attributes", io_attributes);
  }
  if (geometry.has_instances()) {
    const bke::Instances &instances = *geometry.get_instances_for_read();
    auto io_instances = io_geometry->append_dict("instances");

    io_instances->append_int("num_instances", instances.instances_num());

    auto io_references = io_instances->append_array("references");
    for (const bke::InstanceReference &reference : instances.references()) {
      BLI_assert(reference.type() == bke::InstanceReference::Type::GeometrySet);
      io_references->append(
          serialize_geometry_set(reference.geometry_set(), bdata_writer, bdata_sharing));
    }

    io_instances->append("transforms",
                         write_bdata_simple_gspan(bdata_writer, instances.transforms()));
    io_instances->append("handles",
                         write_bdata_simple_gspan(bdata_writer, instances.reference_handles()));

    auto io_attributes = serialize_attributes(
        instances.attributes(), bdata_writer, bdata_sharing, {"position"});
    io_instances->append("attributes", io_attributes);
  }
  return io_geometry;
}

static std::shared_ptr<io::serialize::ArrayValue> serialize_float_array(const Span<float> values)
{
  auto io_value = std::make_shared<io::serialize::ArrayValue>();
  for (const float value : values) {
    io_value->append_double(value);
  }
  return io_value;
}

static std::shared_ptr<io::serialize::ArrayValue> serialize_int_array(const Span<int> values)
{
  auto io_value = std::make_shared<io::serialize::ArrayValue>();
  for (const int value : values) {
    io_value->append_int(value);
  }
  return io_value;
}

static std::shared_ptr<io::serialize::Value> serialize_primitive_value(
    const eCustomDataType data_type, const void *value_ptr)
{
  switch (data_type) {
    case CD_PROP_FLOAT: {
      const float value = *static_cast<const float *>(value_ptr);
      return std::make_shared<io::serialize::DoubleValue>(value);
    }
    case CD_PROP_FLOAT2: {
      const float2 value = *static_cast<const float2 *>(value_ptr);
      return serialize_float_array({&value.x, 2});
    }
    case CD_PROP_FLOAT3: {
      const float3 value = *static_cast<const float3 *>(value_ptr);
      return serialize_float_array({&value.x, 3});
    }
    case CD_PROP_BOOL: {
      const bool value = *static_cast<const bool *>(value_ptr);
      return std::make_shared<io::serialize::BooleanValue>(value);
    }
    case CD_PROP_INT32: {
      const int value = *static_cast<const int *>(value_ptr);
      return std::make_shared<io::serialize::IntValue>(value);
    }
    case CD_PROP_INT32_2D: {
      const int2 value = *static_cast<const int2 *>(value_ptr);
      return serialize_int_array({&value.x, 2});
    }
    case CD_PROP_BYTE_COLOR: {
      const ColorGeometry4b value = *static_cast<const ColorGeometry4b *>(value_ptr);
      const int4 value_int{&value.r};
      return serialize_int_array({&value_int.x, 4});
    }
    case CD_PROP_COLOR: {
      const ColorGeometry4f value = *static_cast<const ColorGeometry4f *>(value_ptr);
      return serialize_float_array({&value.r, 4});
    }
    case CD_PROP_QUATERNION: {
      const math::Quaternion value = *static_cast<const math::Quaternion *>(value_ptr);
      return serialize_float_array({&value.x, 4});
    }
    default:
      break;
  }
  BLI_assert_unreachable();
  return {};
}

void serialize_modifier_simulation_state(const ModifierSimulationState &state,
                                         BDataWriter &bdata_writer,
                                         BDataSharing &bdata_sharing,
                                         DictionaryValue &r_io_root)
{
  r_io_root.append_int("version", 1);
  auto io_zones = r_io_root.append_array("zones");

  for (const auto item : state.zone_states_.items()) {
    const SimulationZoneID &zone_id = item.key;
    const SimulationZoneState &zone_state = *item.value;

    auto io_zone = io_zones->append_dict();

    auto io_zone_id = io_zone->append_array("zone_id");

    for (const int node_id : zone_id.node_ids) {
      io_zone_id->append_int(node_id);
    }

    auto io_state_items = io_zone->append_array("state_items");
    for (const MapItem<int, std::unique_ptr<SimulationStateItem>> &state_item_with_id :
         zone_state.item_by_identifier.items())
    {
      auto io_state_item = io_state_items->append_dict();

      io_state_item->append_int("id", state_item_with_id.key);

      if (const GeometrySimulationStateItem *geometry_state_item =
              dynamic_cast<const GeometrySimulationStateItem *>(state_item_with_id.value.get()))
      {
        io_state_item->append_str("type", "GEOMETRY");

        const GeometrySet &geometry = geometry_state_item->geometry;
        auto io_geometry = serialize_geometry_set(geometry, bdata_writer, bdata_sharing);
        io_state_item->append("data", io_geometry);
      }
      else if (const AttributeSimulationStateItem *attribute_state_item =
                   dynamic_cast<const AttributeSimulationStateItem *>(
                       state_item_with_id.value.get()))
      {
        io_state_item->append_str("type", "ATTRIBUTE");
        io_state_item->append_str("name", attribute_state_item->name());
      }
      else if (const StringSimulationStateItem *string_state_item =
                   dynamic_cast<const StringSimulationStateItem *>(state_item_with_id.value.get()))
      {
        io_state_item->append_str("type", "STRING");
        const StringRefNull str = string_state_item->value();
        /* Small strings are inlined, larger strings are stored separately. */
        const int64_t bdata_threshold = 100;
        if (str.size() < bdata_threshold) {
          io_state_item->append_str("data", string_state_item->value());
        }
        else {
          io_state_item->append("data",
                                write_bdata_raw_bytes(bdata_writer, str.data(), str.size()));
        }
      }
      else if (const PrimitiveSimulationStateItem *primitive_state_item =
                   dynamic_cast<const PrimitiveSimulationStateItem *>(
                       state_item_with_id.value.get()))
      {
        const eCustomDataType data_type = cpp_type_to_custom_data_type(
            primitive_state_item->type());
        io_state_item->append_str("type", get_data_type_io_name(data_type));
        auto io_data = serialize_primitive_value(data_type, primitive_state_item->value());
        io_state_item->append("data", std::move(io_data));
      }
    }
  }
}

template<typename T>
[[nodiscard]] static bool deserialize_typed_array(
    const io::serialize::Value &io_value,
    FunctionRef<std::optional<T>(const io::serialize::Value &io_element)> fn,
    MutableSpan<T> r_values)
{
  const io::serialize::ArrayValue *io_array = io_value.as_array_value();
  if (!io_array) {
    return false;
  }
  if (io_array->elements().size() != r_values.size()) {
    return false;
  }
  for (const int i : r_values.index_range()) {
    const io::serialize::Value &io_element = *io_array->elements()[i];
    std::optional<T> element = fn(io_element);
    if (!element) {
      return false;
    }
    r_values[i] = std::move(*element);
  }
  return true;
}

template<typename T> static std::optional<T> deserialize_int(const io::serialize::Value &io_value)
{
  const io::serialize::IntValue *io_int = io_value.as_int_value();
  if (!io_int) {
    return std::nullopt;
  }
  const int64_t value = io_int->value();
  if (value < std::numeric_limits<T>::min()) {
    return std::nullopt;
  }
  if (value > std::numeric_limits<T>::max()) {
    return std::nullopt;
  }
  return value;
}

static std::optional<float> deserialize_float(const io::serialize::Value &io_value)
{
  if (const io::serialize::DoubleValue *io_double = io_value.as_double_value()) {
    return io_double->value();
  }
  if (const io::serialize::IntValue *io_int = io_value.as_int_value()) {
    return io_int->value();
  }
  return std::nullopt;
}

[[nodiscard]] static bool deserialize_float_array(const io::serialize::Value &io_value,
                                                  MutableSpan<float> r_values)
{
  return deserialize_typed_array<float>(io_value, deserialize_float, r_values);
}

template<typename T>
[[nodiscard]] static bool deserialize_int_array(const io::serialize::Value &io_value,
                                                MutableSpan<T> r_values)
{
  static_assert(std::is_integral_v<T>);
  return deserialize_typed_array<T>(io_value, deserialize_int<T>, r_values);
}

[[nodiscard]] static bool deserialize_primitive_value(const io::serialize::Value &io_value,
                                                      const eCustomDataType type,
                                                      void *r_value)
{
  switch (type) {
    case CD_PROP_FLOAT: {
      const std::optional<float> value = deserialize_float(io_value);
      if (!value) {
        return false;
      }
      *static_cast<float *>(r_value) = *value;
      return true;
    }
    case CD_PROP_FLOAT2: {
      return deserialize_float_array(io_value, {static_cast<float *>(r_value), 2});
    }
    case CD_PROP_FLOAT3: {
      return deserialize_float_array(io_value, {static_cast<float *>(r_value), 3});
    }
    case CD_PROP_BOOL: {
      if (const io::serialize::BooleanValue *io_value_boolean = io_value.as_boolean_value()) {
        *static_cast<bool *>(r_value) = io_value_boolean->value();
        return true;
      }
      return false;
    }
    case CD_PROP_INT32: {
      const std::optional<int> value = deserialize_int<int>(io_value);
      if (!value) {
        return false;
      }
      *static_cast<int *>(r_value) = *value;
      return true;
    }
    case CD_PROP_INT32_2D: {
      return deserialize_int_array<int>(io_value, {static_cast<int *>(r_value), 2});
    }
    case CD_PROP_BYTE_COLOR: {
      return deserialize_int_array<uint8_t>(io_value, {static_cast<uint8_t *>(r_value), 4});
    }
    case CD_PROP_COLOR: {
      return deserialize_float_array(io_value, {static_cast<float *>(r_value), 4});
    }
    case CD_PROP_QUATERNION: {
      return deserialize_float_array(io_value, {static_cast<float *>(r_value), 4});
    }
    default:
      break;
  }
  return false;
}

void deserialize_modifier_simulation_state(const DictionaryValue &io_root,
                                           const BDataReader &bdata_reader,
                                           const BDataSharing &bdata_sharing,
                                           ModifierSimulationState &r_state)
{
  io::serialize::JsonFormatter formatter;
  const std::optional<int> version = io_root.lookup_int("version");
  if (!version) {
    return;
  }
  if (*version != 1) {
    return;
  }
  const io::serialize::ArrayValue *io_zones = io_root.lookup_array("zones");
  if (!io_zones) {
    return;
  }
  for (const auto &io_zone_value : io_zones->elements()) {
    const DictionaryValue *io_zone = io_zone_value->as_dictionary_value();
    if (!io_zone) {
      continue;
    }
    const io::serialize::ArrayValue *io_zone_id = io_zone->lookup_array("zone_id");
    bke::sim::SimulationZoneID zone_id;
    for (const auto &io_zone_id_element : io_zone_id->elements()) {
      const io::serialize::IntValue *io_node_id = io_zone_id_element->as_int_value();
      if (!io_node_id) {
        continue;
      }
      zone_id.node_ids.append(io_node_id->value());
    }

    const io::serialize::ArrayValue *io_state_items = io_zone->lookup_array("state_items");
    if (!io_state_items) {
      continue;
    }

    auto zone_state = std::make_unique<bke::sim::SimulationZoneState>();

    for (const auto &io_state_item_value : io_state_items->elements()) {
      const DictionaryValue *io_state_item = io_state_item_value->as_dictionary_value();
      if (!io_state_item) {
        continue;
      }
      const std::optional<int> state_item_id = io_state_item->lookup_int("id");
      if (!state_item_id) {
        continue;
      }
      const std::optional<StringRefNull> state_item_type = io_state_item->lookup_str("type");
      if (!state_item_type) {
        continue;
      }
      std::unique_ptr<SimulationStateItem> new_state_item;
      if (*state_item_type == StringRef("GEOMETRY")) {
        const DictionaryValue *io_geometry = io_state_item->lookup_dict("data");
        if (!io_geometry) {
          continue;
        }
        GeometrySet geometry = load_geometry(*io_geometry, bdata_reader, bdata_sharing);
        new_state_item = std::make_unique<bke::sim::GeometrySimulationStateItem>(
            std::move(geometry));
      }
      else if (*state_item_type == StringRef("ATTRIBUTE")) {
        const DictionaryValue *io_attribute = io_state_item;
        if (!io_attribute) {
          continue;
        }
        std::optional<StringRefNull> name = io_attribute->lookup_str("name");
        if (!name) {
          continue;
        }
        new_state_item = std::make_unique<AttributeSimulationStateItem>(std::move(*name));
      }
      else if (*state_item_type == StringRef("STRING")) {
        const std::shared_ptr<io::serialize::Value> *io_data = io_state_item->lookup("data");
        if (!io_data) {
          continue;
        }
        if (io_data->get()->type() == io::serialize::eValueType::String) {
          const io::serialize::StringValue &io_string = *io_data->get()->as_string_value();
          new_state_item = std::make_unique<bke::sim::StringSimulationStateItem>(
              io_string.value());
        }
        else if (const io::serialize::DictionaryValue *io_string =
                     io_data->get()->as_dictionary_value()) {
          const std::optional<int64_t> size = io_string->lookup_int("size");
          if (!size) {
            continue;
          }
          std::string str;
          str.resize(*size);
          if (!read_bdata_raw_bytes(bdata_reader, *io_string, *size, str.data())) {
            continue;
          }
          new_state_item = std::make_unique<bke::sim::StringSimulationStateItem>(std::move(str));
        }
      }
      else {
        const std::shared_ptr<io::serialize::Value> *io_data = io_state_item->lookup("data");
        if (!io_data) {
          continue;
        }
        const std::optional<eCustomDataType> data_type = get_data_type_from_io_name(
            *state_item_type);
        if (data_type) {
          const CPPType &cpp_type = *custom_data_type_to_cpp_type(*data_type);
          BUFFER_FOR_CPP_TYPE_VALUE(cpp_type, buffer);
          if (!deserialize_primitive_value(**io_data, *data_type, buffer)) {
            continue;
          }
          BLI_SCOPED_DEFER([&]() { cpp_type.destruct(buffer); });
          new_state_item = std::make_unique<PrimitiveSimulationStateItem>(cpp_type, buffer);
        }
      }
      BLI_assert(new_state_item);
      zone_state->item_by_identifier.add(*state_item_id, std::move(new_state_item));
    }

    r_state.zone_states_.add_overwrite(zone_id, std::move(zone_state));
  }
}

DiskBDataReader::DiskBDataReader(std::string bdata_dir) : bdata_dir_(std::move(bdata_dir)) {}

[[nodiscard]] bool DiskBDataReader::read(const BDataSlice &slice, void *r_data) const
{
  if (slice.range.is_empty()) {
    return true;
  }

  char bdata_path[FILE_MAX];
  BLI_path_join(bdata_path, sizeof(bdata_path), bdata_dir_.c_str(), slice.name.c_str());

  std::lock_guard lock{mutex_};
  std::unique_ptr<fstream> &bdata_file = open_input_streams_.lookup_or_add_cb_as(
      bdata_path,
      [&]() { return std::make_unique<fstream>(bdata_path, std::ios::in | std::ios::binary); });
  bdata_file->seekg(slice.range.start());
  bdata_file->read(static_cast<char *>(r_data), slice.range.size());
  if (bdata_file->gcount() != slice.range.size()) {
    return false;
  }
  return true;
}

DiskBDataWriter::DiskBDataWriter(std::string bdata_name,
                                 std::ostream &bdata_file,
                                 const int64_t current_offset)
    : bdata_name_(std::move(bdata_name)), bdata_file_(bdata_file), current_offset_(current_offset)
{
}

BDataSlice DiskBDataWriter::write(const void *data, const int64_t size)
{
  const int64_t old_offset = current_offset_;
  bdata_file_.write(static_cast<const char *>(data), size);
  current_offset_ += size;
  return {bdata_name_, {old_offset, size}};
}

BDataSharing::~BDataSharing()
{
  for (const ImplicitSharingInfo *sharing_info : stored_by_runtime_.keys()) {
    sharing_info->remove_weak_user_and_delete_if_last();
  }
  for (const ImplicitSharingInfoAndData &value : runtime_by_stored_.values()) {
    if (value.sharing_info) {
      value.sharing_info->remove_user_and_delete_if_last();
    }
  }
}

DictionaryValuePtr BDataSharing::write_shared(const ImplicitSharingInfo *sharing_info,
                                              FunctionRef<DictionaryValuePtr()> write_fn)
{
  if (sharing_info == nullptr) {
    return write_fn();
  }
  return stored_by_runtime_.add_or_modify(
      sharing_info,
      /* Create new value. */
      [&](StoredByRuntimeValue *value) {
        new (value) StoredByRuntimeValue();
        value->io_data = write_fn();
        value->sharing_info_version = sharing_info->version();
        sharing_info->add_weak_user();
        return value->io_data;
      },
      /* Potentially modify existing value. */
      [&](StoredByRuntimeValue *value) {
        const int64_t new_version = sharing_info->version();
        BLI_assert(value->sharing_info_version <= new_version);
        if (value->sharing_info_version < new_version) {
          value->io_data = write_fn();
          value->sharing_info_version = new_version;
        }
        return value->io_data;
      });
}

std::optional<ImplicitSharingInfoAndData> BDataSharing::read_shared(
    const DictionaryValue &io_data,
    FunctionRef<std::optional<ImplicitSharingInfoAndData>()> read_fn) const
{
  std::lock_guard lock{mutex_};

  io::serialize::JsonFormatter formatter;
  std::stringstream ss;
  formatter.serialize(ss, io_data);
  const std::string key = ss.str();

  if (const ImplicitSharingInfoAndData *shared_data = runtime_by_stored_.lookup_ptr(key)) {
    shared_data->sharing_info->add_user();
    return *shared_data;
  }
  std::optional<ImplicitSharingInfoAndData> data = read_fn();
  if (!data) {
    return std::nullopt;
  }
  if (data->sharing_info != nullptr) {
    data->sharing_info->add_user();
    runtime_by_stored_.add_new(key, *data);
  }
  return data;
}

}  // namespace blender::bke::sim

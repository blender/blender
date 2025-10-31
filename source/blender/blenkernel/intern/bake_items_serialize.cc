/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_anonymous_attribute_id.hh"
#include "BKE_attribute_legacy_convert.hh"
#include "BKE_bake_items.hh"
#include "BKE_bake_items_serialize.hh"
#include "BKE_curves.hh"
#include "BKE_customdata.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_pointcloud.hh"
#include "BKE_volume.hh"

#include "BLI_endian_defines.h"
#include "BLI_listbase.h"
#include "BLI_math_matrix_types.hh"
#include "BLI_path_utils.hh"
#include "BLI_string_utf8.h"

#include "DNA_object_types.h"
#include "DNA_volume_types.h"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "NOD_geometry_nodes_list.hh"

#include <fmt/format.h>
#include <sstream>
#include <xxhash.h>

#ifdef WITH_OPENVDB
#  include <openvdb/io/Stream.h>
#  include <openvdb/openvdb.h>

#  include "BKE_volume_grid.hh"
#endif

namespace blender::bke::bake {

using namespace io::serialize;
using DictionaryValuePtr = std::shared_ptr<DictionaryValue>;

static std::unique_ptr<BakeItem> deserialize_bake_item(const DictionaryValue &io_item,
                                                       const BlobReader &blob_reader,
                                                       const BlobReadSharing &blob_sharing);

std::shared_ptr<DictionaryValue> BlobSlice::serialize() const
{
  auto io_slice = std::make_shared<DictionaryValue>();
  io_slice->append_str("name", this->name);
  io_slice->append_int("start", range.start());
  io_slice->append_int("size", range.size());
  return io_slice;
}

std::optional<BlobSlice> BlobSlice::deserialize(const DictionaryValue &io_slice)
{
  const std::optional<StringRefNull> name = io_slice.lookup_str("name");
  const std::optional<int64_t> start = io_slice.lookup_int("start");
  const std::optional<int64_t> size = io_slice.lookup_int("size");
  if (!name || !start || !size) {
    return std::nullopt;
  }

  return BlobSlice{*name, {*start, *size}};
}

BlobSlice BlobWriter::write_as_stream(const StringRef /*file_extension*/,
                                      const FunctionRef<void(std::ostream &)> fn)
{
  std::ostringstream stream{std::ios::binary};
  fn(stream);
  std::string data = stream.rdbuf()->str();
  return this->write(data.data(), data.size());
}

bool BlobReader::read_as_stream(const BlobSlice &slice, FunctionRef<bool(std::istream &)> fn) const
{
  const int64_t size = slice.range.size();
  std::string buffer;
  buffer.resize(size);
  if (!this->read(slice, buffer.data())) {
    return false;
  }
  std::istringstream stream{buffer, std::ios::binary};
  if (!fn(stream)) {
    return false;
  }
  return true;
}

DiskBlobReader::DiskBlobReader(std::string blobs_dir) : blobs_dir_(std::move(blobs_dir)) {}

[[nodiscard]] bool DiskBlobReader::read(const BlobSlice &slice, void *r_data) const
{
  if (slice.range.is_empty()) {
    return true;
  }

  char blob_path[FILE_MAX];
  BLI_path_join(blob_path, sizeof(blob_path), blobs_dir_.c_str(), slice.name.c_str());

  std::lock_guard lock{mutex_};
  std::unique_ptr<fstream> &blob_file = open_input_streams_.lookup_or_add_cb_as(blob_path, [&]() {
    return std::make_unique<fstream>(blob_path, std::ios::in | std::ios::binary);
  });
  blob_file->seekg(slice.range.start());
  blob_file->read(static_cast<char *>(r_data), slice.range.size());
  if (blob_file->gcount() != slice.range.size()) {
    return false;
  }
  return true;
}

DiskBlobWriter::DiskBlobWriter(std::string blob_dir, std::string base_name)
    : blob_dir_(std::move(blob_dir)), base_name_(std::move(base_name))
{
  blob_name_ = base_name_ + ".blob";
}

BlobSlice DiskBlobWriter::write(const void *data, const int64_t size)
{
  if (!blob_stream_.is_open()) {
    char blob_path[FILE_MAX];
    BLI_path_join(blob_path, sizeof(blob_path), blob_dir_.c_str(), blob_name_.c_str());
    BLI_file_ensure_parent_dir_exists(blob_path);
    blob_stream_.open(blob_path, std::ios::out | std::ios::binary);
  }

  const int64_t old_offset = current_offset_;
  blob_stream_.write(static_cast<const char *>(data), size);
  current_offset_ += size;
  total_written_size_ += size;
  return {blob_name_, {old_offset, size}};
}

static std::string make_independent_file_name(const StringRef base_name,
                                              const int file_index,
                                              const StringRef extension)
{
  return fmt::format("{}_file_{}{}", base_name, file_index, extension);
}

BlobSlice DiskBlobWriter::write_as_stream(const StringRef file_extension,
                                          const FunctionRef<void(std::ostream &)> fn)
{
  BLI_assert(file_extension.startswith("."));
  independent_file_count_++;
  const std::string file_name = make_independent_file_name(
      base_name_, independent_file_count_, file_extension);

  char path[FILE_MAX];
  BLI_path_join(path, sizeof(path), blob_dir_.c_str(), file_name.c_str());
  BLI_file_ensure_parent_dir_exists(path);
  std::fstream stream{path, std::ios::out | std::ios::binary};
  fn(stream);
  const int64_t written_bytes_num = stream.tellg();
  total_written_size_ += written_bytes_num;
  return {file_name, {0, written_bytes_num}};
}

void MemoryBlobReader::add(const StringRef name, const Span<std::byte> blob)
{
  blob_by_name_.add(name, blob);
}

bool MemoryBlobReader::read(const BlobSlice &slice, void *r_data) const
{
  if (slice.range.is_empty()) {
    return true;
  }
  const Span<std::byte> blob_data = blob_by_name_.lookup_default(slice.name, {});
  if (!blob_data.index_range().contains(slice.range)) {
    return false;
  }
  const void *copy_src = blob_data.slice(slice.range).data();
  memcpy(r_data, copy_src, slice.range.size());
  return true;
}

MemoryBlobWriter::MemoryBlobWriter(std::string base_name) : base_name_(std::move(base_name))
{
  blob_name_ = base_name_ + ".blob";
  stream_by_name_.add(blob_name_, {std::make_unique<std::ostringstream>(std::ios::binary)});
}

BlobSlice MemoryBlobWriter::write(const void *data, int64_t size)
{
  OutputStream &stream = stream_by_name_.lookup(blob_name_);
  const int64_t old_offset = stream.offset;
  stream.stream->write(static_cast<const char *>(data), size);
  stream.offset += size;
  total_written_size_ += size;
  return {blob_name_, IndexRange::from_begin_size(old_offset, size)};
}

BlobSlice MemoryBlobWriter::write_as_stream(const StringRef file_extension,
                                            const FunctionRef<void(std::ostream &)> fn)
{
  BLI_assert(file_extension.startswith("."));
  independent_file_count_++;
  const std::string name = make_independent_file_name(
      base_name_, independent_file_count_, file_extension);
  OutputStream stream{std::make_unique<std::ostringstream>(std::ios::binary)};
  fn(*stream.stream);
  const int64_t size = stream.stream->tellp();
  stream_by_name_.add_new(name, std::move(stream));
  total_written_size_ += size;
  return {name, IndexRange(size)};
}

BlobWriteSharing::~BlobWriteSharing()
{
  for (const ImplicitSharingInfo *sharing_info : stored_by_runtime_.keys()) {
    sharing_info->remove_weak_user_and_delete_if_last();
  }
}

BlobReadSharing::~BlobReadSharing()
{
  for (const ImplicitSharingInfoAndData &value : runtime_by_stored_.values()) {
    if (value.sharing_info) {
      value.sharing_info->remove_user_and_delete_if_last();
    }
  }
}

DictionaryValuePtr BlobWriteSharing::write_implicitly_shared(
    const ImplicitSharingInfo *sharing_info, FunctionRef<DictionaryValuePtr()> write_fn)
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

std::shared_ptr<io::serialize::DictionaryValue> BlobWriteSharing::write_deduplicated(
    BlobWriter &writer, const void *data, const int64_t size_in_bytes)
{
  const uint64_t content_hash = XXH3_64bits(data, size_in_bytes);
  const BlobSlice slice = slice_by_content_hash_.lookup_or_add_cb(
      content_hash, [&]() { return writer.write(data, size_in_bytes); });
  return slice.serialize();
}

std::optional<ImplicitSharingInfoAndData> BlobReadSharing::read_shared(
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

static StringRefNull get_endian_io_name(const int endian)
{
  BLI_assert(endian == L_ENDIAN);
  UNUSED_VARS_NDEBUG(endian);
  return "little";
}

static StringRefNull get_domain_io_name(const AttrDomain domain)
{
  const char *io_name = "unknown";
  RNA_enum_id_from_value(rna_enum_attribute_domain_items, int(domain), &io_name);
  return io_name;
}

static StringRefNull get_data_type_io_name(const eCustomDataType data_type)
{
  const char *io_name = "unknown";
  RNA_enum_id_from_value(rna_enum_attribute_type_items, data_type, &io_name);
  return io_name;
}

static std::optional<AttrDomain> get_domain_from_io_name(const StringRefNull io_name)
{
  int domain;
  if (!RNA_enum_value_from_identifier(rna_enum_attribute_domain_items, io_name.c_str(), &domain)) {
    return std::nullopt;
  }
  return AttrDomain(domain);
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
 * Write the data, always in little endian.
 */
static std::shared_ptr<DictionaryValue> write_blob_raw_data_with_endian(
    BlobWriter &blob_writer,
    BlobWriteSharing &blob_sharing,
    const void *data,
    const int64_t size_in_bytes)
{
  auto io_data = blob_sharing.write_deduplicated(blob_writer, data, size_in_bytes);
  BLI_STATIC_ASSERT(ENDIAN_ORDER == L_ENDIAN, "Blender only builds on little endian systems")
  return io_data;
}

/**
 * Read data of an into an array.
 *
 * \returns True if successful, false if reading fails, or endian switch would be needed.
 */
[[nodiscard]] static bool read_blob_raw_data_with_endian(const BlobReader &blob_reader,
                                                         const DictionaryValue &io_data,
                                                         const int64_t element_size,
                                                         const int64_t elements_num,
                                                         void *r_data)
{
  const std::optional<BlobSlice> slice = BlobSlice::deserialize(io_data);
  if (!slice) {
    return false;
  }
  if (slice->range.size() != element_size * elements_num) {
    return false;
  }
  if (!blob_reader.read(*slice, r_data)) {
    return false;
  }
  const StringRefNull stored_endian = io_data.lookup_str("endian").value_or("little");
  const StringRefNull current_endian = get_endian_io_name(ENDIAN_ORDER);
  const bool need_endian_switch = stored_endian != current_endian;
  if (need_endian_switch) {
    /* NOTE: this is endianness-sensitive. */
    /* Blender only builds on little endian systems, and reads little endian data here. */
    return false;
  }
  return true;
}

/** Write bytes ignoring endianness. */
static std::shared_ptr<DictionaryValue> write_blob_raw_bytes(BlobWriter &blob_writer,
                                                             BlobWriteSharing &blob_sharing,
                                                             const void *data,
                                                             const int64_t size_in_bytes)
{
  return blob_sharing.write_deduplicated(blob_writer, data, size_in_bytes);
}

/** Read bytes ignoring endianness. */
[[nodiscard]] static bool read_blob_raw_bytes(const BlobReader &blob_reader,
                                              const DictionaryValue &io_data,
                                              const int64_t bytes_num,
                                              void *r_data)
{
  const std::optional<BlobSlice> slice = BlobSlice::deserialize(io_data);
  if (!slice) {
    return false;
  }
  if (slice->range.size() != bytes_num) {
    return false;
  }
  return blob_reader.read(*slice, r_data);
}

static std::shared_ptr<DictionaryValue> write_blob_simple_gspan(BlobWriter &blob_writer,
                                                                BlobWriteSharing &blob_sharing,
                                                                const GSpan data)
{
  const CPPType &type = data.type();
  BLI_assert(type.is_trivial);
  if (type.size == 1 || type.is<ColorGeometry4b>()) {
    return write_blob_raw_bytes(blob_writer, blob_sharing, data.data(), data.size_in_bytes());
  }
  return write_blob_raw_data_with_endian(
      blob_writer, blob_sharing, data.data(), data.size_in_bytes());
}

[[nodiscard]] static bool read_blob_simple_gspan(const BlobReader &blob_reader,
                                                 const DictionaryValue &io_data,
                                                 GMutableSpan r_data)
{
  const CPPType &type = r_data.type();
  BLI_assert(type.is_trivial);
  if (type.size == 1 || type.is<ColorGeometry4b>()) {
    return read_blob_raw_bytes(blob_reader, io_data, r_data.size_in_bytes(), r_data.data());
  }
  if (type.is_any<int16_t, uint16_t, int32_t, uint32_t, int64_t, uint64_t, float>()) {
    return read_blob_raw_data_with_endian(
        blob_reader, io_data, type.size, r_data.size(), r_data.data());
  }
  if (type.is_any<float2, int2>()) {
    return read_blob_raw_data_with_endian(
        blob_reader, io_data, sizeof(int32_t), r_data.size() * 2, r_data.data());
  }
  if (type.is_any<short2>()) {
    return read_blob_raw_data_with_endian(
        blob_reader, io_data, sizeof(short), r_data.size() * 2, r_data.data());
  }
  if (type.is<float3>()) {
    return read_blob_raw_data_with_endian(
        blob_reader, io_data, sizeof(float), r_data.size() * 3, r_data.data());
  }
  if (type.is<float4x4>()) {
    return read_blob_raw_data_with_endian(
        blob_reader, io_data, sizeof(float), r_data.size() * 16, r_data.data());
  }
  if (type.is<ColorGeometry4f>()) {
    return read_blob_raw_data_with_endian(
        blob_reader, io_data, sizeof(float), r_data.size() * 4, r_data.data());
  }
  if (type.is<math::Quaternion>()) {
    return read_blob_raw_data_with_endian(
        blob_reader, io_data, sizeof(float), r_data.size() * 4, r_data.data());
  }
  return false;
}

static std::shared_ptr<DictionaryValue> write_blob_shared_simple_gspan(
    BlobWriter &blob_writer,
    BlobWriteSharing &blob_sharing,
    const GSpan data,
    const ImplicitSharingInfo *sharing_info)
{
  return blob_sharing.write_implicitly_shared(
      sharing_info, [&]() { return write_blob_simple_gspan(blob_writer, blob_sharing, data); });
}

[[nodiscard]] static const void *read_blob_shared_simple_gspan(
    const DictionaryValue &io_data,
    const BlobReader &blob_reader,
    const BlobReadSharing &blob_sharing,
    const CPPType &cpp_type,
    const int size,
    const ImplicitSharingInfo **r_sharing_info)
{
  const char *func = __func__;
  const std::optional<ImplicitSharingInfoAndData> sharing_info_and_data = blob_sharing.read_shared(
      io_data, [&]() -> std::optional<ImplicitSharingInfoAndData> {
        void *data_mem = MEM_mallocN_aligned(size * cpp_type.size, cpp_type.alignment, func);
        if (!read_blob_simple_gspan(blob_reader, io_data, {cpp_type, data_mem, size})) {
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
[[nodiscard]] static bool read_blob_shared_simple_span(const DictionaryValue &io_data,
                                                       const BlobReader &blob_reader,
                                                       const BlobReadSharing &blob_sharing,
                                                       const int size,
                                                       T **r_data,
                                                       const ImplicitSharingInfo **r_sharing_info)
{
  *r_data = const_cast<T *>(static_cast<const T *>(read_blob_shared_simple_gspan(
      io_data, blob_reader, blob_sharing, CPPType::get<T>(), size, r_sharing_info)));
  return *r_data != nullptr;
}

[[nodiscard]] static bool load_materials(const io::serialize::ArrayValue &io_materials,
                                         std::unique_ptr<BakeMaterialsList> &materials)
{
  if (io_materials.elements().is_empty()) {
    return true;
  }
  materials = std::make_unique<BakeMaterialsList>();
  for (const auto &io_material_value : io_materials.elements()) {
    if (io_material_value->type() == io::serialize::eValueType::Null) {
      materials->append(std::nullopt);
      continue;
    }
    const auto *io_material = io_material_value->as_dictionary_value();
    if (!io_material) {
      return false;
    }
    std::optional<std::string> id_name = io_material->lookup_str("name");
    if (!id_name) {
      return false;
    }
    std::string lib_name = io_material->lookup_str("lib_name").value_or("");
    materials->append(BakeDataBlockID(ID_MA, std::move(*id_name), std::move(lib_name)));
  }
  return true;
}

[[nodiscard]] static bool load_attributes(const io::serialize::ArrayValue &io_attributes,
                                          MutableAttributeAccessor &attributes,
                                          const BlobReader &blob_reader,
                                          const BlobReadSharing &blob_sharing)
{
  for (const auto &io_attribute_value : io_attributes.elements()) {
    const auto *io_attribute = io_attribute_value->as_dictionary_value();
    if (!io_attribute) {
      return false;
    }
    const std::optional<StringRefNull> name = io_attribute->lookup_str("name");
    const std::optional<StringRefNull> domain_str = io_attribute->lookup_str("domain");
    const std::optional<StringRefNull> type_str = io_attribute->lookup_str("type");
    const auto *io_data = io_attribute->lookup_dict("data");
    if (!name || !domain_str || !type_str || !io_data) {
      return false;
    }

    const std::optional<AttrDomain> domain = get_domain_from_io_name(*domain_str);
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
    const void *attribute_data = read_blob_shared_simple_gspan(
        *io_data, blob_reader, blob_sharing, *cpp_type, domain_size, &attribute_sharing_info);
    if (!attribute_data) {
      return false;
    }
    BLI_SCOPED_DEFER([&]() { attribute_sharing_info->remove_user_and_delete_if_last(); });

    if (attributes.contains(*name)) {
      /* If the attribute exists already, copy the values over to the existing array. */
      GSpanAttributeWriter attribute = attributes.lookup_or_add_for_write_only_span(
          *name, *domain, *custom_data_type_to_attr_type(*data_type));
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
                          *custom_data_type_to_attr_type(*data_type),
                          AttributeInitShared(attribute_data, *attribute_sharing_info)))
      {
        return false;
      }
    }
  }
  return true;
}

static PointCloud *try_load_pointcloud(const DictionaryValue &io_geometry,
                                       const BlobReader &blob_reader,
                                       const BlobReadSharing &blob_sharing)
{
  const DictionaryValue *io_pointcloud = io_geometry.lookup_dict("pointcloud");
  if (!io_pointcloud) {
    return nullptr;
  }
  const io::serialize::ArrayValue *io_attributes = io_pointcloud->lookup_array("attributes");
  if (!io_attributes) {
    return nullptr;
  }
  const int points_num = io_pointcloud->lookup_int("num_points").value_or(0);
  PointCloud *pointcloud = bke::pointcloud_new_no_attributes(points_num);

  auto cancel = [&]() {
    BKE_id_free(nullptr, pointcloud);
    return nullptr;
  };

  MutableAttributeAccessor attributes = pointcloud->attributes_for_write();
  if (!load_attributes(*io_attributes, attributes, blob_reader, blob_sharing)) {
    return cancel();
  }

  if (const io::serialize::ArrayValue *io_materials = io_pointcloud->lookup_array("materials")) {
    if (!load_materials(*io_materials, pointcloud->runtime->bake_materials)) {
      return cancel();
    }
  }
  return pointcloud;
}

static std::optional<CurvesGeometry> try_load_curves_geometry(const DictionaryValue &io_curves,
                                                              const BlobReader &blob_reader,
                                                              const BlobReadSharing &blob_sharing)
{
  const io::serialize::ArrayValue *io_attributes = io_curves.lookup_array("attributes");
  if (!io_attributes) {
    return std::nullopt;
  }

  CurvesGeometry curves;
  curves.attribute_storage.wrap().remove("position");
  curves.point_num = io_curves.lookup_int("num_points").value_or(0);
  curves.curve_num = io_curves.lookup_int("num_curves").value_or(0);

  if (curves.curves_num() > 0) {
    const auto *io_curve_offsets = io_curves.lookup_dict("curve_offsets");
    if (!io_curve_offsets) {
      return std::nullopt;
    }
    if (!read_blob_shared_simple_span(*io_curve_offsets,
                                      blob_reader,
                                      blob_sharing,
                                      curves.curves_num() + 1,
                                      &curves.curve_offsets,
                                      &curves.runtime->curve_offsets_sharing_info))
    {
      return std::nullopt;
    }
  }

  MutableAttributeAccessor attributes = curves.attributes_for_write();
  if (!load_attributes(*io_attributes, attributes, blob_reader, blob_sharing)) {
    return std::nullopt;
  }

  if (const io::serialize::ArrayValue *io_materials = io_curves.lookup_array("materials")) {
    if (!load_materials(*io_materials, curves.runtime->bake_materials)) {
      return std::nullopt;
    }
  }

  curves.update_curve_types();
  return curves;
}

static Curves *try_load_curves(const DictionaryValue &io_geometry,
                               const BlobReader &blob_reader,
                               const BlobReadSharing &blob_sharing)
{
  const DictionaryValue *io_curves = io_geometry.lookup_dict("curves");
  if (!io_curves) {
    return nullptr;
  }

  const io::serialize::ArrayValue *io_attributes = io_curves->lookup_array("attributes");
  if (!io_attributes) {
    return nullptr;
  }

  std::optional<CurvesGeometry> curves_opt = try_load_curves_geometry(
      *io_curves, blob_reader, blob_sharing);
  if (!curves_opt) {
    return nullptr;
  }

  Curves *curves_id = curves_new_nomain(std::move(*curves_opt));
  CurvesGeometry &curves = curves_id->geometry.wrap();

  auto cancel = [&]() {
    BKE_id_free(nullptr, curves_id);
    return nullptr;
  };

  if (const io::serialize::ArrayValue *io_materials = io_curves->lookup_array("materials")) {
    if (!load_materials(*io_materials, curves.runtime->bake_materials)) {
      return cancel();
    }
  }

  return curves_id;
}

static GreasePencil *try_load_grease_pencil(const DictionaryValue &io_geometry,
                                            const BlobReader &blob_reader,
                                            const BlobReadSharing &blob_sharing)
{
  const DictionaryValue *io_grease_pencil = io_geometry.lookup_dict("grease_pencil");
  if (!io_grease_pencil) {
    return nullptr;
  }

  const io::serialize::ArrayValue *io_layers = io_grease_pencil->lookup_array("layers");
  if (!io_layers) {
    return nullptr;
  }

  const io::serialize::ArrayValue *io_layer_attributes = io_grease_pencil->lookup_array(
      "layer_attributes");
  if (!io_layer_attributes) {
    return nullptr;
  }

  GreasePencil *grease_pencil = BKE_grease_pencil_new_nomain();
  auto cancel = [&]() {
    BKE_id_free(nullptr, grease_pencil);
    return nullptr;
  };

  const int layers_num = io_layers->elements().size();
  grease_pencil->add_layers_with_empty_drawings_for_eval(layers_num);

  for (const int layer_i : io_layers->elements().index_range()) {
    const auto &io_layer_value = io_layers->elements()[layer_i];
    const io::serialize::DictionaryValue *io_layer = io_layer_value->as_dictionary_value();
    if (!io_layer) {
      return cancel();
    }
    const io::serialize::DictionaryValue *io_strokes = io_layer->lookup_dict("strokes");
    if (!io_strokes) {
      return cancel();
    }
    const std::optional<std::string> layer_name = io_layer->lookup_str("name");
    if (!layer_name) {
      return cancel();
    }
    greasepencil::Layer &layer = grease_pencil->layer(layer_i);
    layer.set_name(*layer_name);
    std::optional<CurvesGeometry> curves_opt = try_load_curves_geometry(
        *io_strokes, blob_reader, blob_sharing);
    if (!curves_opt) {
      return cancel();
    }
    greasepencil::Drawing &drawing = *grease_pencil->get_eval_drawing(layer);
    drawing.strokes_for_write() = std::move(*curves_opt);
  }

  MutableAttributeAccessor attributes = grease_pencil->attributes_for_write();
  if (!load_attributes(*io_layer_attributes, attributes, blob_reader, blob_sharing)) {
    return cancel();
  }

  const DictionaryValue *io_layer_opacities = io_grease_pencil->lookup_dict("opacities");
  Array<float> layer_opacities(layers_num);
  if (!io_layer_opacities ||
      !read_blob_simple_gspan(blob_reader, *io_layer_opacities, layer_opacities.as_mutable_span()))
  {
    return cancel();
  }

  const DictionaryValue *io_layer_blend_modes = io_grease_pencil->lookup_dict("blend_modes");
  Array<int8_t> layer_blend_modes(layers_num);
  if (!io_layer_opacities || !read_blob_simple_gspan(blob_reader,
                                                     *io_layer_blend_modes,
                                                     layer_blend_modes.as_mutable_span()))
  {
    return cancel();
  }

  const DictionaryValue *io_layer_transforms = io_grease_pencil->lookup_dict("transforms");
  Array<float4x4> layer_transforms(layers_num);
  if (!io_layer_transforms || !read_blob_simple_gspan(blob_reader,
                                                      *io_layer_transforms,
                                                      layer_transforms.as_mutable_span()))
  {
    return cancel();
  }

  for (const int layer_i : IndexRange(layers_num)) {
    greasepencil::Layer &layer = grease_pencil->layer(layer_i);
    layer.opacity = layer_opacities[layer_i];
    layer.blend_mode = layer_blend_modes[layer_i];
    layer.set_local_transform(layer_transforms[layer_i]);
  }

  if (const io::serialize::ArrayValue *io_materials = io_grease_pencil->lookup_array("materials"))
  {
    if (!load_materials(*io_materials, grease_pencil->runtime->bake_materials)) {
      return cancel();
    }
  }

  return grease_pencil;
}

static Mesh *try_load_mesh(const DictionaryValue &io_geometry,
                           const BlobReader &blob_reader,
                           const BlobReadSharing &blob_sharing)
{
  const DictionaryValue *io_mesh = io_geometry.lookup_dict("mesh");
  if (!io_mesh) {
    return nullptr;
  }

  const io::serialize::ArrayValue *io_attributes = io_mesh->lookup_array("attributes");
  if (!io_attributes) {
    return nullptr;
  }

  Mesh *mesh = bke::mesh_new_no_attributes(0, 0, 0, 0);
  mesh->verts_num = io_mesh->lookup_int("num_vertices").value_or(0);
  mesh->edges_num = io_mesh->lookup_int("num_edges").value_or(0);
  mesh->faces_num = io_mesh->lookup_int("num_polygons").value_or(0);
  mesh->corners_num = io_mesh->lookup_int("num_corners").value_or(0);

  auto cancel = [&]() {
    BKE_id_free(nullptr, mesh);
    return nullptr;
  };

  if (mesh->faces_num > 0) {
    const auto *io_poly_offsets = io_mesh->lookup_dict("poly_offsets");
    if (!io_poly_offsets) {
      return cancel();
    }
    if (!read_blob_shared_simple_span(*io_poly_offsets,
                                      blob_reader,
                                      blob_sharing,
                                      mesh->faces_num + 1,
                                      &mesh->face_offset_indices,
                                      &mesh->runtime->face_offsets_sharing_info))
    {
      return cancel();
    }
  }

  /* Create the vertex group name list, then later on when processing generic attributes, these
   * names will be stored as vertex groups. */
  if (const auto *io_attributes = io_mesh->lookup_array("vertex_group_names")) {
    for (const std::shared_ptr<Value> &value : io_attributes->elements()) {
      if (value->type() != io::serialize::eValueType::String) {
        return cancel();
      }
      bDeformGroup *defgroup = MEM_callocN<bDeformGroup>(__func__);
      STRNCPY_UTF8(defgroup->name, value->as_string_value()->value().c_str());
      BLI_addtail(&mesh->vertex_group_names, defgroup);
    }
  }

  MutableAttributeAccessor attributes = mesh->attributes_for_write();
  if (!load_attributes(*io_attributes, attributes, blob_reader, blob_sharing)) {
    return cancel();
  }

  if (const io::serialize::ArrayValue *io_materials = io_mesh->lookup_array("materials")) {
    if (!load_materials(*io_materials, mesh->runtime->bake_materials)) {
      return cancel();
    }
  }

  return mesh;
}

static GeometrySet load_geometry(const DictionaryValue &io_geometry,
                                 const BlobReader &blob_reader,
                                 const BlobReadSharing &blob_sharing);

static std::unique_ptr<Instances> try_load_instances(const DictionaryValue &io_geometry,
                                                     const BlobReader &blob_reader,
                                                     const BlobReadSharing &blob_sharing)
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

  std::unique_ptr<Instances> instances = std::make_unique<Instances>();
  instances->resize(num_instances);

  for (const auto &io_reference_value : io_references->elements()) {
    const DictionaryValue *io_reference = io_reference_value->as_dictionary_value();
    GeometrySet reference_geometry;
    if (io_reference) {
      reference_geometry = load_geometry(*io_reference, blob_reader, blob_sharing);
    }
    instances->add_new_reference(std::move(reference_geometry));
  }

  MutableAttributeAccessor attributes = instances->attributes_for_write();
  if (!load_attributes(*io_attributes, attributes, blob_reader, blob_sharing)) {
    return {};
  }

  if (!attributes.contains(".reference_index")) {
    /* Try reading the reference index attribute from the old bake format from before it was an
     * attribute. */
    const auto *io_handles = io_instances->lookup_dict("handles");
    if (!io_handles) {
      return {};
    }
    if (!read_blob_simple_gspan(
            blob_reader, *io_handles, instances->reference_handles_for_write()))
    {
      return {};
    }
  }

  if (!attributes.contains("instance_transform")) {
    /* Try reading the transform attribute from the old bake format from before it was an
     * attribute. */
    const auto *io_handles = io_instances->lookup_dict("transforms");
    if (!io_handles) {
      return {};
    }
    if (!read_blob_simple_gspan(blob_reader, *io_handles, instances->transforms_for_write())) {
      return {};
    }
  }

  return instances;
}

#ifdef WITH_OPENVDB
static Volume *try_load_volume(const DictionaryValue &io_geometry, const BlobReader &blob_reader)
{
  const DictionaryValue *io_volume = io_geometry.lookup_dict("volume");
  if (!io_volume) {
    return nullptr;
  }
  const auto *io_vdb = io_volume->lookup_dict("vdb");
  if (!io_vdb) {
    return nullptr;
  }
  openvdb::GridPtrVecPtr vdb_grids;
  if (std::optional<BlobSlice> vdb_slice = BlobSlice::deserialize(*io_vdb)) {
    if (!blob_reader.read_as_stream(*vdb_slice, [&](std::istream &stream) {
          try {
            openvdb::io::Stream vdb_stream{stream};
            vdb_grids = vdb_stream.getGrids();
            return true;
          }
          catch (...) {
            return false;
          }
        }))
    {
      return nullptr;
    }
  }
  Volume *volume = BKE_id_new_nomain<Volume>(nullptr);
  auto cancel = [&]() {
    BKE_id_free(nullptr, volume);
    return nullptr;
  };

  for (openvdb::GridBase::Ptr &vdb_grid : *vdb_grids) {
    if (vdb_grid) {
      bke::GVolumeGrid grid{std::move(vdb_grid)};
      BKE_volume_grid_add(volume, *grid.release());
    }
  }
  if (const io::serialize::ArrayValue *io_materials = io_volume->lookup_array("materials")) {
    if (!load_materials(*io_materials, volume->runtime->bake_materials)) {
      return cancel();
    }
  }
  return volume;
}
#endif

static GeometrySet load_geometry(const DictionaryValue &io_geometry,
                                 const BlobReader &blob_reader,
                                 const BlobReadSharing &blob_sharing)
{
  GeometrySet geometry;
  geometry.replace_mesh(try_load_mesh(io_geometry, blob_reader, blob_sharing));
  geometry.replace_pointcloud(try_load_pointcloud(io_geometry, blob_reader, blob_sharing));
  geometry.replace_curves(try_load_curves(io_geometry, blob_reader, blob_sharing));
  geometry.replace_grease_pencil(try_load_grease_pencil(io_geometry, blob_reader, blob_sharing));
  geometry.replace_instances(try_load_instances(io_geometry, blob_reader, blob_sharing).release());
#ifdef WITH_OPENVDB
  geometry.replace_volume(try_load_volume(io_geometry, blob_reader));
#endif
  return geometry;
}

static std::shared_ptr<io::serialize::ArrayValue> serialize_materials(
    const std::unique_ptr<BakeMaterialsList> &materials)
{
  auto io_materials = std::make_shared<io::serialize::ArrayValue>();
  if (!materials) {
    return io_materials;
  }
  for (const std::optional<BakeDataBlockID> &material : *materials) {
    if (material) {
      auto io_material = io_materials->append_dict();
      io_material->append_str("name", material->id_name);
      if (!material->lib_name.empty()) {
        io_material->append_str("lib_name", material->lib_name);
      }
    }
    else {
      io_materials->append_null();
    }
  }
  return io_materials;
}

static std::shared_ptr<io::serialize::ArrayValue> serialize_attributes(
    const AttributeAccessor &attributes,
    BlobWriter &blob_writer,
    BlobWriteSharing &blob_sharing,
    const Set<std::string> &attributes_to_ignore)
{
  auto io_attributes = std::make_shared<io::serialize::ArrayValue>();
  attributes.foreach_attribute([&](const AttributeIter &iter) {
    BLI_assert(!bke::attribute_name_is_anonymous(iter.name));
    if (attributes_to_ignore.contains_as(iter.name)) {
      return;
    }

    auto io_attribute = io_attributes->append_dict();

    io_attribute->append_str("name", iter.name);

    const StringRefNull domain_name = get_domain_io_name(iter.domain);
    io_attribute->append_str("domain", domain_name);

    const StringRefNull type_name = get_data_type_io_name(
        *attr_type_to_custom_data_type(iter.data_type));
    io_attribute->append_str("type", type_name);

    const GAttributeReader attribute = iter.get();
    const GVArraySpan attribute_span(attribute.varray);
    io_attribute->append("data",
                         write_blob_shared_simple_gspan(
                             blob_writer,
                             blob_sharing,
                             attribute_span,
                             attribute.varray.is_span() ? attribute.sharing_info : nullptr));
  });
  return io_attributes;
}

static void serialize_curves_geometry(DictionaryValue &io_curves,
                                      const CurvesGeometry &curves,
                                      BlobWriter &blob_writer,
                                      BlobWriteSharing &blob_sharing)
{
  io_curves.append_int("num_points", curves.point_num);
  io_curves.append_int("num_curves", curves.curve_num);

  if (curves.curve_num > 0) {
    io_curves.append("curve_offsets",
                     write_blob_shared_simple_gspan(blob_writer,
                                                    blob_sharing,
                                                    curves.offsets(),
                                                    curves.runtime->curve_offsets_sharing_info));
  }

  auto io_attributes = serialize_attributes(curves.attributes(), blob_writer, blob_sharing, {});
  io_curves.append("attributes", io_attributes);
}

static std::shared_ptr<DictionaryValue> serialize_geometry_set(const GeometrySet &geometry,
                                                               BlobWriter &blob_writer,
                                                               BlobWriteSharing &blob_sharing)
{
  auto io_geometry = std::make_shared<DictionaryValue>();
  if (geometry.has_mesh()) {
    const Mesh &mesh = *geometry.get_mesh();
    auto io_mesh = io_geometry->append_dict("mesh");

    io_mesh->append_int("num_vertices", mesh.verts_num);
    io_mesh->append_int("num_edges", mesh.edges_num);
    io_mesh->append_int("num_polygons", mesh.faces_num);
    io_mesh->append_int("num_corners", mesh.corners_num);

    if (mesh.faces_num > 0) {
      io_mesh->append("poly_offsets",
                      write_blob_shared_simple_gspan(blob_writer,
                                                     blob_sharing,
                                                     mesh.face_offsets(),
                                                     mesh.runtime->face_offsets_sharing_info));
    }

    auto io_materials = serialize_materials(mesh.runtime->bake_materials);
    io_mesh->append("materials", io_materials);

    if (!BLI_listbase_is_empty(&mesh.vertex_group_names)) {
      auto io_vertex_group_names = io_mesh->append_array("vertex_group_names");
      LISTBASE_FOREACH (bDeformGroup *, defgroup, &mesh.vertex_group_names) {
        io_vertex_group_names->append_str(defgroup->name);
      }
    }

    auto io_attributes = serialize_attributes(mesh.attributes(), blob_writer, blob_sharing, {});
    io_mesh->append("attributes", io_attributes);
  }
  if (geometry.has_pointcloud()) {
    const PointCloud &pointcloud = *geometry.get_pointcloud();
    auto io_pointcloud = io_geometry->append_dict("pointcloud");

    io_pointcloud->append_int("num_points", pointcloud.totpoint);

    auto io_materials = serialize_materials(pointcloud.runtime->bake_materials);
    io_pointcloud->append("materials", io_materials);

    auto io_attributes = serialize_attributes(
        pointcloud.attributes(), blob_writer, blob_sharing, {});
    io_pointcloud->append("attributes", io_attributes);
  }
  if (geometry.has_curves()) {
    const Curves &curves_id = *geometry.get_curves();
    const CurvesGeometry &curves = curves_id.geometry.wrap();

    auto io_curves = io_geometry->append_dict("curves");

    serialize_curves_geometry(*io_curves, curves, blob_writer, blob_sharing);

    auto io_materials = serialize_materials(curves.runtime->bake_materials);
    io_curves->append("materials", io_materials);
  }
  if (geometry.has_grease_pencil()) {
    const GreasePencil &grease_pencil = *geometry.get_grease_pencil();
    auto io_grease_pencil = io_geometry->append_dict("grease_pencil");
    auto io_layers = io_grease_pencil->append_array("layers");

    Vector<float> layer_opacities;
    Vector<int8_t> layer_blend_modes;
    Vector<float4x4> layer_transforms;
    for (const greasepencil::Layer *layer : grease_pencil.layers()) {
      auto io_layer = io_layers->append_dict();
      io_layer->append_str("name", layer->name());
      auto io_strokes = io_layer->append_dict("strokes");
      const greasepencil::Drawing *drawing = grease_pencil.get_eval_drawing(*layer);
      if (drawing) {
        serialize_curves_geometry(*io_strokes, drawing->strokes(), blob_writer, blob_sharing);
      }
      else {
        serialize_curves_geometry(*io_strokes, CurvesGeometry(), blob_writer, blob_sharing);
      }

      layer_opacities.append(layer->opacity);
      layer_blend_modes.append(layer->blend_mode);
      layer_transforms.append(layer->local_transform());
    }

    io_grease_pencil->append(
        "opacities",
        write_blob_simple_gspan(blob_writer, blob_sharing, layer_opacities.as_span()));
    io_grease_pencil->append(
        "blend_modes",
        write_blob_simple_gspan(blob_writer, blob_sharing, layer_blend_modes.as_span()));
    io_grease_pencil->append(
        "transforms",
        write_blob_simple_gspan(blob_writer, blob_sharing, layer_transforms.as_span()));

    auto io_layer_attributes = serialize_attributes(
        grease_pencil.attributes(), blob_writer, blob_sharing, {});
    io_grease_pencil->append("layer_attributes", io_layer_attributes);

    auto io_materials = serialize_materials(grease_pencil.runtime->bake_materials);
    io_grease_pencil->append("materials", io_materials);
  }
#ifdef WITH_OPENVDB
  if (geometry.has_volume()) {
    const Volume &volume = *geometry.get_volume();
    const int grids_num = BKE_volume_num_grids(&volume);

    auto io_volume = io_geometry->append_dict("volume");
    auto io_vdb = blob_writer
                      .write_as_stream(".vdb",
                                       [&](std::ostream &stream) {
                                         openvdb::GridCPtrVec vdb_grids;
                                         Vector<bke::VolumeTreeAccessToken> tree_tokens;
                                         for (const int i : IndexRange(grids_num)) {
                                           const bke::VolumeGridData *grid = BKE_volume_grid_get(
                                               &volume, i);
                                           tree_tokens.append_as();
                                           vdb_grids.push_back(grid->grid_ptr(tree_tokens.last()));
                                         }

                                         openvdb::io::Stream vdb_stream(stream);
                                         vdb_stream.write(vdb_grids);
                                       })
                      .serialize();
    io_volume->append("vdb", std::move(io_vdb));

    auto io_materials = serialize_materials(volume.runtime->bake_materials);
    io_volume->append("materials", io_materials);
  }
#endif
  if (geometry.has_instances()) {
    const Instances &instances = *geometry.get_instances();
    auto io_instances = io_geometry->append_dict("instances");

    io_instances->append_int("num_instances", instances.instances_num());

    auto io_references = io_instances->append_array("references");
    for (const InstanceReference &reference : instances.references()) {
      if (reference.type() == InstanceReference::Type::GeometrySet) {
        const GeometrySet &geometry = reference.geometry_set();
        io_references->append(serialize_geometry_set(geometry, blob_writer, blob_sharing));
      }
      else {
        /* TODO: Support serializing object and collection references. */
        io_references->append(serialize_geometry_set({}, blob_writer, blob_sharing));
      }
    }

    auto io_attributes = serialize_attributes(
        instances.attributes(), blob_writer, blob_sharing, {});
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
    case CD_PROP_INT16_2D: {
      const int2 value = int2(*static_cast<const short2 *>(value_ptr));
      return serialize_int_array({&value.x, 2});
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
      return serialize_float_array({&value.w, 4});
    }
    case CD_PROP_FLOAT4X4: {
      const float4x4 value = *static_cast<const float4x4 *>(value_ptr);
      return serialize_float_array({value.base_ptr(), float4x4::col_len * float4x4::row_len});
    }
    default:
      break;
  }
  BLI_assert_unreachable();
  return {};
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
  if (value < std::numeric_limits<T>::lowest()) {
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
    case CD_PROP_INT16_2D: {
      return deserialize_int_array<int16_t>(io_value, {static_cast<int16_t *>(r_value), 2});
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
    case CD_PROP_FLOAT4X4: {
      return deserialize_float_array(io_value, {static_cast<float *>(r_value), 4 * 4});
    }
    default:
      break;
  }
  return false;
}

[[nodiscard]] static bool deserialize_bundle(const io::serialize::DictionaryValue &io_bundle,
                                             const BlobReader &blob_reader,
                                             const BlobReadSharing &blob_sharing,
                                             BundleBakeItem &r_bake_item)
{
  const ArrayValue *io_items = io_bundle.lookup_array("items");
  if (!io_items) {
    return false;
  }
  for (const auto &io_item_ : io_items->elements()) {
    const DictionaryValue *io_item = io_item_->as_dictionary_value();
    if (!io_item) {
      return false;
    }
    const std::optional<std::string> key = io_item->lookup_str("key");
    if (!key) {
      return false;
    }
    const std::optional<StringRefNull> socket_idname = io_item->lookup_str("socket_idname");
    if (!socket_idname) {
      return false;
    }
    const DictionaryValue *io_item_value = io_item->lookup_dict("value");
    std::unique_ptr<BakeItem> value = deserialize_bake_item(
        *io_item_value, blob_reader, blob_sharing);
    if (!value) {
      return false;
    }
    r_bake_item.items.append(
        BundleBakeItem::Item{*key, BundleBakeItem::SocketValue{*socket_idname, std::move(value)}});
  }
  return true;
}

static void serialize_bake_item(const BakeItem &item,
                                BlobWriter &blob_writer,
                                BlobWriteSharing &blob_sharing,
                                DictionaryValue &r_io_item)
{
  if (!item.name.empty()) {
    r_io_item.append_str("name", item.name);
  }
  if (const auto *geometry_state_item = dynamic_cast<const GeometryBakeItem *>(&item)) {
    r_io_item.append_str("type", "GEOMETRY");

    const GeometrySet &geometry = geometry_state_item->geometry;
    auto io_geometry = serialize_geometry_set(geometry, blob_writer, blob_sharing);
    r_io_item.append("data", io_geometry);
  }
  else if (const auto *attribute_state_item = dynamic_cast<const AttributeBakeItem *>(&item)) {
    r_io_item.append_str("type", "ATTRIBUTE");
    r_io_item.append_str("name", attribute_state_item->name());
  }
#ifdef WITH_OPENVDB
  else if (const auto *grid_state_item = dynamic_cast<const VolumeGridBakeItem *>(&item)) {
    r_io_item.append_str("type", "GRID");
    const GVolumeGrid &grid = *grid_state_item->grid;
    auto io_vdb = blob_writer
                      .write_as_stream(".vdb",
                                       [&](std::ostream &stream) {
                                         openvdb::GridCPtrVec vdb_grids;
                                         bke::VolumeTreeAccessToken tree_token;
                                         vdb_grids.push_back(grid->grid_ptr(tree_token));
                                         openvdb::io::Stream vdb_stream(stream);
                                         vdb_stream.write(vdb_grids);
                                       })
                      .serialize();
    r_io_item.append("vdb", std::move(io_vdb));
  }
#endif
  else if (const auto *string_state_item = dynamic_cast<const StringBakeItem *>(&item)) {
    r_io_item.append_str("type", "STRING");
    const StringRefNull str = string_state_item->value();
    /* Small strings are inlined, larger strings are stored separately. */
    const int64_t blob_threshold = 100;
    if (str.size() < blob_threshold) {
      r_io_item.append_str("data", string_state_item->value());
    }
    else {
      r_io_item.append("data",
                       write_blob_raw_bytes(blob_writer, blob_sharing, str.data(), str.size()));
    }
  }
  else if (const auto *primitive_state_item = dynamic_cast<const PrimitiveBakeItem *>(&item)) {
    const eCustomDataType data_type = cpp_type_to_custom_data_type(primitive_state_item->type());
    r_io_item.append_str("type", get_data_type_io_name(data_type));
    auto io_data = serialize_primitive_value(data_type, primitive_state_item->value());
    r_io_item.append("data", std::move(io_data));
  }
  else if (const auto *bundle_state_item = dynamic_cast<const BundleBakeItem *>(&item)) {
    r_io_item.append_str("type", "BUNDLE");
    ArrayValue &io_items = *r_io_item.append_array("items");
    for (const BundleBakeItem::Item &item : bundle_state_item->items) {
      if (const auto *socket_value = std::get_if<BundleBakeItem::SocketValue>(&item.value)) {
        DictionaryValue &io_bundle_item = *io_items.append_dict();
        io_bundle_item.append_str("key", item.key);
        io_bundle_item.append_str("socket_idname", socket_value->socket_idname);
        io::serialize::DictionaryValue &io_bundle_item_value = *io_bundle_item.append_dict(
            "value");
        serialize_bake_item(*socket_value->value, blob_writer, blob_sharing, io_bundle_item_value);
      }
    }
  }
  else if (const auto *list_state_item = dynamic_cast<const ListBakeItem *>(&item)) {
    r_io_item.append_str("type", "LIST");
    if (const nodes::ListPtr *simple_list = std::get_if<nodes::ListPtr>(&list_state_item->value)) {
      if (*simple_list) {
        const nodes::List &list = **simple_list;
        if (list.cpp_type() == CPPType::get<std::string>()) {
          /* TODO Not supported yet, can't be constructed by users. */
          BLI_assert_unreachable();
        }
        else {
          const eCustomDataType data_type = cpp_type_to_custom_data_type(list.cpp_type());
          r_io_item.append_str("item_type", get_data_type_io_name(data_type));
          r_io_item.append_int("num_items", list.size());
          if (const auto *single_data = std::get_if<nodes::List::SingleData>(&list.data())) {
            r_io_item.append("value", serialize_primitive_value(data_type, single_data->value));
          }
          else if (const auto *array_data = std::get_if<nodes::List::ArrayData>(&list.data())) {
            const GSpan array_span = {list.cpp_type(), array_data->data, list.size()};
            r_io_item.append(
                "data",
                write_blob_shared_simple_gspan(
                    blob_writer, blob_sharing, array_span, array_data->sharing_info.get()));
          }
        }
      }
    }
    if (const auto *bundle_list = std::get_if<ListBakeItem::BundleList>(&list_state_item->value)) {
      r_io_item.append_str("item_type", "BUNDLE");
      ArrayValue &io_items = *r_io_item.append_array("items");
      for (const BundleBakeItem &bundle_bake_item : *bundle_list) {
        DictionaryValue &io_bundle_item = *io_items.append_dict();
        serialize_bake_item(bundle_bake_item, blob_writer, blob_sharing, io_bundle_item);
      }
    }
  }
}

static std::unique_ptr<BakeItem> deserialize_bake_item(const DictionaryValue &io_item,
                                                       const BlobReader &blob_reader,
                                                       const BlobReadSharing &blob_sharing)
{

  const std::optional<StringRefNull> state_item_type = io_item.lookup_str("type");
  if (!state_item_type) {
    return {};
  }
  if (*state_item_type == StringRef("GEOMETRY")) {
    const DictionaryValue *io_geometry = io_item.lookup_dict("data");
    if (!io_geometry) {
      return {};
    }
    GeometrySet geometry = load_geometry(*io_geometry, blob_reader, blob_sharing);
    return std::make_unique<GeometryBakeItem>(std::move(geometry));
  }
  if (*state_item_type == StringRef("ATTRIBUTE")) {
    const DictionaryValue *io_attribute = &io_item;
    if (!io_attribute) {
      return {};
    }
    std::optional<StringRefNull> name = io_attribute->lookup_str("name");
    if (!name) {
      return {};
    }
    return std::make_unique<AttributeBakeItem>(std::move(*name));
  }
#ifdef WITH_OPENVDB
  if (*state_item_type == StringRef("GRID")) {
    const DictionaryValue &io_grid = io_item;
    const auto *io_vdb = io_grid.lookup_dict("vdb");
    if (!io_vdb) {
      return {};
    }
    std::optional<BlobSlice> vdb_slice = BlobSlice::deserialize(*io_vdb);
    if (!vdb_slice) {
      return {};
    }
    openvdb::GridPtrVecPtr vdb_grids;
    if (!blob_reader.read_as_stream(*vdb_slice, [&](std::istream &stream) {
          try {
            openvdb::io::Stream vdb_stream{stream};
            vdb_grids = vdb_stream.getGrids();
            return true;
          }
          catch (...) {
            return false;
          }
        }))
    {
      return {};
    }
    if (vdb_grids->size() != 1) {
      return {};
    }
    std::shared_ptr<openvdb::GridBase> vdb_grid = std::move((*vdb_grids)[0]);
    GVolumeGrid grid{std::move(vdb_grid)};
    return std::make_unique<VolumeGridBakeItem>(std::make_unique<GVolumeGrid>(grid));
  }
#endif
  if (*state_item_type == StringRef("STRING")) {
    const std::shared_ptr<io::serialize::Value> *io_data = io_item.lookup("data");
    if (!io_data) {
      return {};
    }
    if (io_data->get()->type() == io::serialize::eValueType::String) {
      const io::serialize::StringValue &io_string = *io_data->get()->as_string_value();
      return std::make_unique<StringBakeItem>(io_string.value());
    }
    if (const io::serialize::DictionaryValue *io_string = io_data->get()->as_dictionary_value()) {
      const std::optional<int64_t> size = io_string->lookup_int("size");
      if (!size) {
        return {};
      }
      std::string str;
      str.resize(*size);
      if (!read_blob_raw_bytes(blob_reader, *io_string, *size, str.data())) {
        return {};
      }
      return std::make_unique<StringBakeItem>(std::move(str));
    }
  }
  if (*state_item_type == StringRef("BUNDLE")) {
    auto bundle = std::make_unique<BundleBakeItem>();
    if (!deserialize_bundle(io_item, blob_reader, blob_sharing, *bundle)) {
      return {};
    }
    return bundle;
  }
  if (*state_item_type == StringRef("LIST")) {
    const std::optional<StringRefNull> io_list_item_type = io_item.lookup_str("item_type");
    if (const std::optional<eCustomDataType> data_type = get_data_type_from_io_name(
            *io_list_item_type))
    {
      const std::optional<int> num_items = io_item.lookup_int("num_items");
      if (!num_items) {
        return {};
      }
      const CPPType *cpp_type = custom_data_type_to_cpp_type(*data_type);
      BLI_assert(cpp_type);
      if (const std::shared_ptr<io::serialize::Value> *io_value = io_item.lookup("value")) {
        BUFFER_FOR_CPP_TYPE_VALUE(*cpp_type, buffer);
        if (!deserialize_primitive_value(**io_value, *data_type, buffer)) {
          return {};
        }
        auto list = nodes::List::create(
            *cpp_type, nodes::List::SingleData::ForValue(GPointer{cpp_type, buffer}), *num_items);
        return std::make_unique<ListBakeItem>(std::move(list));
      }
      if (const io::serialize::DictionaryValue *io_data = io_item.lookup_dict("data")) {
        GArray<> buffer(*cpp_type, *num_items);
        if (!read_blob_simple_gspan(
                blob_reader, *io_data, GMutableSpan{cpp_type, buffer.data(), *num_items}))
        {
          return {};
        }
        nodes::List::ArrayData array_data;
        const auto *sharing_info = new ImplicitSharedValue<GArray<>>(std::move(buffer));
        array_data.data = const_cast<void *>(sharing_info->data.data());
        array_data.sharing_info = ImplicitSharingPtr<>(sharing_info);
        auto list = nodes::List::create(*cpp_type, std::move(array_data), *num_items);
        return std::make_unique<ListBakeItem>(std::move(list));
      }
    }
    if (*io_list_item_type == "BUNDLE") {
      const ArrayValue *io_items = io_item.lookup_array("items");
      if (!io_items) {
        return {};
      }
      const Span<std::shared_ptr<Value>> io_elements = io_items->elements();
      Vector<BundleBakeItem> bundle_list(io_elements.size());
      for (const int i : io_elements.index_range()) {
        if (!io_elements[i]) {
          return {};
        }
        const DictionaryValue *io_bundle_dict = io_elements[i]->as_dictionary_value();
        if (!io_bundle_dict) {
          return {};
        }
        if (!deserialize_bundle(*io_bundle_dict, blob_reader, blob_sharing, bundle_list[i])) {
          return {};
        }
      }
      return std::make_unique<ListBakeItem>(std::move(bundle_list));
    }
  }
  const std::shared_ptr<io::serialize::Value> *io_data = io_item.lookup("data");
  if (!io_data) {
    return {};
  }
  const std::optional<eCustomDataType> data_type = get_data_type_from_io_name(*state_item_type);
  if (data_type) {
    const CPPType &cpp_type = *custom_data_type_to_cpp_type(*data_type);
    BUFFER_FOR_CPP_TYPE_VALUE(cpp_type, buffer);
    if (!deserialize_primitive_value(**io_data, *data_type, buffer)) {
      return {};
    }
    BLI_SCOPED_DEFER([&]() { cpp_type.destruct(buffer); });
    return std::make_unique<PrimitiveBakeItem>(cpp_type, buffer);
  }
  return {};
}

static constexpr int bake_file_version = 3;

void serialize_bake(const BakeState &bake_state,
                    BlobWriter &blob_writer,
                    BlobWriteSharing &blob_sharing,
                    std::ostream &r_stream)
{
  io::serialize::DictionaryValue io_root;
  io_root.append_int("version", bake_file_version);
  io::serialize::DictionaryValue &io_items = *io_root.append_dict("items");
  for (auto item : bake_state.items_by_id.items()) {
    io::serialize::DictionaryValue &io_item = *io_items.append_dict(std::to_string(item.key));
    serialize_bake_item(*item.value, blob_writer, blob_sharing, io_item);
  }

  io::serialize::JsonFormatter formatter;
  formatter.serialize(r_stream, io_root);
}

std::optional<BakeState> deserialize_bake(std::istream &stream,
                                          const BlobReader &blob_reader,
                                          const BlobReadSharing &blob_sharing)
{
  JsonFormatter formatter;
  std::unique_ptr<io::serialize::Value> io_root_value;
  try {
    io_root_value = formatter.deserialize(stream);
  }
  catch (...) {
    return std::nullopt;
  }
  if (!io_root_value) {
    return std::nullopt;
  }
  const io::serialize::DictionaryValue *io_root = io_root_value->as_dictionary_value();
  if (!io_root) {
    return std::nullopt;
  }
  const std::optional<int> version = io_root->lookup_int("version");
  if (!version.has_value() || *version != bake_file_version) {
    return std::nullopt;
  }
  const io::serialize::DictionaryValue *io_items = io_root->lookup_dict("items");
  if (!io_items) {
    return std::nullopt;
  }
  BakeState bake_state;
  for (const auto &io_item_value : io_items->elements()) {
    const io::serialize::DictionaryValue *io_item = io_item_value.second->as_dictionary_value();
    if (!io_item) {
      return std::nullopt;
    }
    int id;
    try {
      id = std::stoi(io_item_value.first);
    }
    catch (...) {
      return std::nullopt;
    }
    if (bake_state.items_by_id.contains(id)) {
      return std::nullopt;
    }
    std::unique_ptr<BakeItem> bake_item = deserialize_bake_item(
        *io_item, blob_reader, blob_sharing);
    if (!bake_item) {
      return std::nullopt;
    }
    bake_state.items_by_id.add_new(id, std::move(bake_item));
  }
  return bake_state;
}

}  // namespace blender::bke::bake

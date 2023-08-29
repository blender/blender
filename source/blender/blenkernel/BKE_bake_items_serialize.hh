/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BLI_fileops.hh"
#include "BLI_serialize.hh"

#include "BKE_bake_items.hh"

namespace blender::bke {

/**
 * Reference to a slice of memory typically stored on disk.
 */
struct BDataSlice {
  std::string name;
  IndexRange range;

  std::shared_ptr<io::serialize::DictionaryValue> serialize() const;
  static std::optional<BDataSlice> deserialize(const io::serialize::DictionaryValue &io_slice);
};

/**
 * Abstract base class for loading binary data.
 */
class BDataReader {
 public:
  /**
   * Read the data from the given slice into the provided memory buffer.
   * \return True on success, otherwise false.
   */
  [[nodiscard]] virtual bool read(const BDataSlice &slice, void *r_data) const = 0;
};

/**
 * Abstract base class for writing binary data.
 */
class BDataWriter {
 public:
  /**
   * Write the provided binary data.
   * \return Slice where the data has been written to.
   */
  virtual BDataSlice write(const void *data, int64_t size) = 0;
};

/**
 * Allows for simple data deduplication when writing or reading data by making use of implicit
 * sharing.
 */
class BDataSharing {
 private:
  struct StoredByRuntimeValue {
    /**
     * Version of the shared data that was written before. This is needed because the data might
     * be changed later without changing the #ImplicitSharingInfo pointer.
     */
    int64_t sharing_info_version;
    /**
     * Identifier of the stored data. This includes information for where the data is stored (a
     * #BDataSlice) and optionally information for how it is loaded (e.g. endian information).
     */
    std::shared_ptr<io::serialize::DictionaryValue> io_data;
  };

  /**
   * Map used to detect when some data has already been written. It keeps a weak reference to
   * #ImplicitSharingInfo, allowing it to check for equality of two arrays just by comparing the
   * sharing info's pointer and version.
   */
  Map<const ImplicitSharingInfo *, StoredByRuntimeValue> stored_by_runtime_;

  /**
   * Use a mutex so that #read_shared can be implemented in a thread-safe way.
   */
  mutable std::mutex mutex_;
  /**
   * Map used to detect when some data has been previously loaded. This keeps strong
   * references to #ImplicitSharingInfo.
   */
  mutable Map<std::string, ImplicitSharingInfoAndData> runtime_by_stored_;

 public:
  ~BDataSharing();

  /**
   * Check if the data referenced by `sharing_info` has been written before. If yes, return the
   * identifier for the previously written data. Otherwise, write the data now and store the
   * identifier for later use.
   * \return Identifier that indicates from where the data has been written.
   */
  [[nodiscard]] std::shared_ptr<io::serialize::DictionaryValue> write_shared(
      const ImplicitSharingInfo *sharing_info,
      FunctionRef<std::shared_ptr<io::serialize::DictionaryValue>()> write_fn);

  /**
   * Check if the data identified by `io_data` has been read before or load it now.
   * \return Shared ownership to the read data, or none if there was an error.
   */
  [[nodiscard]] std::optional<ImplicitSharingInfoAndData> read_shared(
      const io::serialize::DictionaryValue &io_data,
      FunctionRef<std::optional<ImplicitSharingInfoAndData>()> read_fn) const;
};

/**
 * A specific #BDataReader that reads from disk.
 */
class DiskBDataReader : public BDataReader {
 private:
  const std::string bdata_dir_;
  mutable std::mutex mutex_;
  mutable Map<std::string, std::unique_ptr<fstream>> open_input_streams_;

 public:
  DiskBDataReader(std::string bdata_dir);
  [[nodiscard]] bool read(const BDataSlice &slice, void *r_data) const override;
};

/**
 * A specific #BDataWriter that writes to a file on disk.
 */
class DiskBDataWriter : public BDataWriter {
 private:
  /** Name of the file that data is written to. */
  std::string bdata_name_;
  /** File handle. */
  std::ostream &bdata_file_;
  /** Current position in the file. */
  int64_t current_offset_;

 public:
  DiskBDataWriter(std::string bdata_name, std::ostream &bdata_file, int64_t current_offset);

  BDataSlice write(const void *data, int64_t size) override;
};

/**
 * Writes the bake item into `r_io_item`.
 */
void serialize_bake_item(const BakeItem &item,
                         BDataWriter &bdata_writer,
                         BDataSharing &bdata_sharing,
                         io::serialize::DictionaryValue &r_io_item);
/**
 * Creates a bake item from `io_item`.
 */
std::unique_ptr<BakeItem> deserialize_bake_item(const io::serialize::DictionaryValue &io_item,
                                                const BDataReader &bdata_reader,
                                                const BDataSharing &bdata_sharing);

}  // namespace blender::bke

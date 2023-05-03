/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "BKE_simulation_state.hh"

#include "BLI_serialize.hh"

struct Main;
struct ModifierData;

namespace blender {
class fstream;
}

namespace blender::bke::sim {

using DictionaryValue = io::serialize::DictionaryValue;
using DictionaryValuePtr = std::shared_ptr<DictionaryValue>;

/**
 * Reference to a slice of memory typically stored on disk.
 */
struct BDataSlice {
  std::string name;
  IndexRange range;

  DictionaryValuePtr serialize() const;
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
    DictionaryValuePtr io_data;
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
  [[nodiscard]] DictionaryValuePtr write_shared(const ImplicitSharingInfo *sharing_info,
                                                FunctionRef<DictionaryValuePtr()> write_fn);

  /**
   * Check if the data identified by `io_data` has been read before or load it now.
   * \return Shared ownership to the read data, or none if there was an error.
   */
  [[nodiscard]] std::optional<ImplicitSharingInfoAndData> read_shared(
      const DictionaryValue &io_data,
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
 * Get the directory that contains all baked simulation data for the given modifier. This is a
 * parent directory of the two directories below.
 */
std::string get_bake_directory(const Main &bmain, const Object &object, const ModifierData &md);
std::string get_bdata_directory(const Main &bmain, const Object &object, const ModifierData &md);
std::string get_meta_directory(const Main &bmain, const Object &object, const ModifierData &md);

/**
 * Encode the simulation state in a #DictionaryValue which also contains references to external
 * binary data that has been written using #bdata_writer.
 */
void serialize_modifier_simulation_state(const ModifierSimulationState &state,
                                         BDataWriter &bdata_writer,
                                         BDataSharing &bdata_sharing,
                                         DictionaryValue &r_io_root);
/**
 * Fill the simulation state by parsing the provided #DictionaryValue which also contains
 * references to external binary data that is read using #bdata_reader.
 */
void deserialize_modifier_simulation_state(const DictionaryValue &io_root,
                                           const BDataReader &bdata_reader,
                                           const BDataSharing &bdata_sharing,
                                           ModifierSimulationState &r_state);

}  // namespace blender::bke::sim

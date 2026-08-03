/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blenloader
 *
 * This file contains an API that allows different parts of Blender to define what data is stored
 * in .blend files.
 *
 * Four callbacks have to be provided to fully implement .blend I/O for a piece of data. One of
 * those is related to file writing and three for file reading. Reading requires multiple
 * callbacks, due to the way linking between files works.
 *
 * Brief description of the individual callbacks:
 *  - Blend Write: Define which structs and memory buffers are saved.
 *  - Blend Read Data: Loads structs and memory buffers from file and updates pointers them.
 *  - Blend Read Lib: Updates pointers to ID data blocks.
 *  - Blend Expand: Defines which other data blocks should be loaded (possibly from other files).
 *      Note, this is now handled as part of the foreach-id iteration. This needs to be implemented
 *      for DNA data that has references to data-blocks.
 *
 * Each of these callbacks uses a different API functions.
 *
 * Some parts of Blender, e.g. modifiers, don't require you to implement all four callbacks.
 * Instead only the first two are necessary. The other two are handled by general ID management. In
 * the future, we might want to get rid of those two callbacks entirely, but for now they are
 * necessary.
 */

#pragma once

#include <type_traits>

#include "DNA_ID.h"
#include "DNA_listBase.h"
#include "DNA_sdna_type_ids.hh"

#include "BLI_dynamic_stack_buffer.hh"
#include "BLI_function_ref.hh"
#include "BLI_implicit_sharing.hh"
#include "BLI_map.hh"

namespace blender {

class ImplicitSharingInfo;
struct BlendFileReadReport;
struct BlendLibReader;
struct ID;
struct ListBase;
struct Main;
struct WriteData;
struct FileData;
enum eReportType : uint16_t;

/**
 * Allows code using #BlendWriter to customize how a specific struct is written. Often, small
 * changes to the struct data are done before it is written (e.g. zeroing runtime pointers and
 * setting generated pointers).
 */
class BlendStructWriter {
 private:
  WriteData *wd_;
  int struct_nr_;
  /** This is a shallow copy of the struct being written. */
  MutableSpan<char> data_;

 public:
  BlendStructWriter(WriteData &wd, const int struct_nr, MutableSpan<char> data)
      : wd_(&wd), struct_nr_(struct_nr), data_(data)
  {
  }

  /**
   * Mark the pointer at the given offset as purely runtime. That means that it will be zeroed.
   */
  void runtime_ptr(int64_t offset);

  /**
   * Tag the pointer at the given offset as "generated". That implies that it may be remapped
   * to a stable pointer. It's expected that the pointer has been tagged with
   * #BLO_write_generated_pointer_tag before.
   */
  void generated_ptr(int64_t offset);
};

using BlendStructWriterFn = FunctionRef<void(BlendStructWriter &struct_writer)>;

struct BlendWriter {
  WriteData *wd = nullptr;

  void write_struct_by_name(const char *struct_name,
                            const void *data,
                            BlendStructWriterFn fn = nullptr);
  void write_struct_by_id(int struct_id, const void *data, BlendStructWriterFn fn = nullptr);
  void write_struct_at_address_by_id(int struct_id,
                                     const void *address,
                                     const void *data,
                                     BlendStructWriterFn fn = nullptr);
  void write_struct_at_address_by_id_with_filecode(int filecode,
                                                   int struct_id,
                                                   const void *address,
                                                   const void *data,
                                                   BlendStructWriterFn fn = nullptr);
  void write_struct_array_by_name(const char *struct_name,
                                  int64_t array_size,
                                  const void *data,
                                  BlendStructWriterFn fn = nullptr);
  void write_struct_array_by_id(int struct_id,
                                int64_t array_size,
                                const void *data,
                                BlendStructWriterFn fn = nullptr);
  void write_struct_array_at_address_by_id(int struct_id,
                                           int64_t array_size,
                                           const void *address,
                                           const void *data,
                                           BlendStructWriterFn fn = nullptr);
  void write_struct_list_by_name(const char *struct_name,
                                 ListBase *list,
                                 BlendStructWriterFn fn = nullptr);
  void write_struct_list_by_id(int struct_id,
                               const ListBase *list,
                               BlendStructWriterFn fn = nullptr);

  /**
   * Write raw data.
   *
   * \warning Avoid using this method if possible. There are only a very few cases in current
   * code where it is actually needed (e.g. the ShapeKey's data, since its items size varies
   * depending on the type of geometry owning it, see #shapekey_blend_write).
   *
   * \warning Data written with this call have no type information attached to them
   * in the blend-file. The main consequence is that there will be no handling of endianness
   * conversion for them in readfile code.
   * Basic typed array methods (like #write_int8_array etc.) also use this
   * internally, but if their matching read function is used to load the data (like
   * #BLO_read_array), the read function will take care of endianness conversion.
   */
  void write_raw(size_t size_in_bytes, const void *data);

  /** Write typed arrays. */
  void write_char_array(int64_t num, const char *data);
  void write_int8_array(int64_t num, const int8_t *data);
  void write_int16_array(int64_t num, const int16_t *data);
  void write_uint8_array(int64_t num, const uint8_t *data);
  void write_int32_array(int64_t num, const int32_t *data);
  void write_uint32_array(int64_t num, const uint32_t *data);
  void write_float_array(int64_t num, const float *data);
  void write_double_array(int64_t num, const double *data);
  void write_float3_array(int64_t num, const float *data);
  void write_pointer_array(int64_t num, const void *data);

  /** Write a null terminated string. */
  void write_string(const char *data);

  int struct_id_by_name(const char *struct_name) const;

  template<typename T> void write_struct(const T *data, const BlendStructWriterFn fn = nullptr)
  {
    this->write_struct_by_id(dna::sdna_struct_id_get<T>(), data, fn);
  }

  template<typename T>
  void write_struct_cast(const void *data, const BlendStructWriterFn fn = nullptr)
  {
    this->write_struct_by_id(dna::sdna_struct_id_get<T>(), data, fn);
  }

  template<typename T>
  void write_struct_at_address(const void *address,
                               const T *data,
                               const BlendStructWriterFn fn = nullptr)
  {
    this->write_struct_at_address_by_id(dna::sdna_struct_id_get<T>(), address, data, fn);
  }

  template<typename T>
  void write_struct_at_address_cast(const void *address,
                                    const void *data,
                                    const BlendStructWriterFn fn = nullptr)
  {
    this->write_struct_at_address_by_id(dna::sdna_struct_id_get<T>(), address, data, fn);
  }

  template<typename T>
  void write_struct_array(const int64_t array_size,
                          const T *data,
                          const BlendStructWriterFn fn = nullptr)
  {
    this->write_struct_array_by_id(dna::sdna_struct_id_get<T>(), array_size, data, fn);
  }

  template<typename T>
  void write_struct_array_cast(const int64_t array_size,
                               const void *data,
                               const BlendStructWriterFn fn = nullptr)
  {
    this->write_struct_array_by_id(dna::sdna_struct_id_get<T>(), array_size, data, fn);
  }

  template<typename T>
  void write_struct_array_at_address(const int64_t array_size,
                                     const void *address,
                                     const T *data,
                                     const BlendStructWriterFn fn = nullptr)
  {
    this->write_struct_array_at_address_by_id(
        dna::sdna_struct_id_get<T>(), array_size, address, data, fn);
  }

  template<typename T>
  void write_struct_list(const ListBaseT<T> *list, const BlendStructWriterFn fn = nullptr)
  {
    this->write_struct_list_by_id(dna::sdna_struct_id_get<T>(), list, fn);
  }

  template<typename T>
  void write_id_struct(const void *id_address, const T *id, const BlendStructWriterFn fn = nullptr)
  {
    this->write_struct_at_address_by_id_with_filecode(
        GS(id_cast<const ID *>(id)->name), dna::sdna_struct_id_get<T>(), id_address, id, fn);
  }
};

struct BlendDataReader {
  /** Pointer to private #FileData in readfile.cc. */
  FileData *fd = nullptr;

  /**
   * The key is the old address id referencing shared data that's written to a file, typically an
   * array. The corresponding value is the shared data at run-time.
   */
  Map<uint64_t, ImplicitSharingInfoAndData> shared_data_by_stored_address;
};

struct BlendLibReader {
  FileData *fd;
  Main *main;
};

/* -------------------------------------------------------------------- */
/** \name Blend Write API
 *
 * Most functions fall into one of two categories. Either they write a DNA struct or a raw memory
 * buffer to the .blend file.
 *
 * It is safe to pass NULL as data_ptr. In this case nothing will be stored.
 *
 * DNA Struct Writing
 * ------------------
 *
 * Functions dealing with DNA structs begin with `BLO_write_struct_*`.
 *
 * DNA struct types can be identified in different ways:
 * - Run-time Name: The name is provided as `const char *`.
 * - Compile-time Name: The name is provided at compile time. This is more efficient.
 * - Struct ID: Every DNA struct type has an integer ID that can be queried with
 *   #BlendWriter::struct_id_by_name. Providing this ID can be a useful optimization when many
 *   structs of the same type are stored AND if those structs are not in a continuous array.
 *
 * Often only a single instance of a struct is written at once. However, sometimes it is necessary
 * to write arrays or linked lists. Separate functions for that are provided as well.
 *
 * There is a special macro for writing id structs: #BLO_write_id_struct.
 * Those are handled differently from other structs.
 *
 * Raw Data Writing
 * ----------------
 *
 * At the core there is #BlendWriter::write_raw, which can write arbitrary memory buffers to the
 * file. The code that reads this data might have to correct its byte-order. For the common cases
 * there are convenience functions that write and read arrays of simple types such as `int32`.
 * Those will correct endianness automatically.
 * \{ */

/**
 * Specific code to prepare IDs to be written.
 *
 * Required for writing properly embedded IDs currently.
 *
 * \note Once there is a better generic handling of embedded IDs,
 * this may go back to private code in `writefile.cc`.
 */
struct BLO_Write_IDBuffer {
 private:
  static constexpr int static_size = 8192;
  DynamicStackBuffer<static_size> buffer_;

 public:
  BLO_Write_IDBuffer(ID &id, bool is_undo, bool is_placeholder);
  BLO_Write_IDBuffer(ID &id, BlendWriter *writer);

  ID *get()
  {
    return static_cast<ID *>(buffer_.buffer());
  };
};

/* Misc. */

/**
 * Check if the data can be written more efficiently by making use of implicit-sharing. If yes, the
 * user count of the sharing-info is increased making the data immutable. The provided callback
 * should serialize the potentially shared data. It is only called when necessary.
 *
 * \param approximate_size_in_bytes: Used to be able to approximate how large the undo step is in
 * total.
 * \param write_fn: Use the #BlendWrite to serialize the potentially shared data.
 */
void BLO_write_shared(BlendWriter *writer,
                      const void *data,
                      size_t approximate_size_in_bytes,
                      const ImplicitSharingInfo *sharing_info,
                      FunctionRef<void()> write_fn);

/**
 * Needs to be called for all pointers that _need_ to be 'stabilized' when writing undo steps,
 * _before_ any of these pointers are actually written (so typically at the very start of a write
 * function)..
 *
 * Typically required for data dynamically generated as part of the write process, see e.g.
 * AttributeStorage::dna_attributes.
 */
void BLO_write_generated_pointer_tag(BlendWriter *writer, const void *data);

/**
 * Sometimes different data is written depending on whether the file is saved to disk or used for
 * undo. This function returns true when the current file-writing is done for undo.
 */
bool BLO_write_is_undo(BlendWriter *writer);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Blend Read Data API
 *
 * Generally, for every BLO_write_* call there should be a corresponding BLO_read_* call.
 *
 * Most BLO_read_* functions get a pointer to a pointer as argument. That allows the function to
 * update the pointer to its new value.
 *
 * When the given pointer points to a memory buffer that was not stored in the file, the pointer is
 * updated to be NULL. When it was pointing to NULL before, it will stay that way.
 *
 * Examples of matching calls:
 *
 * \code{.c}
 * writer->write_struct(clmd->sim_parms);
 * BLO_read_struct(reader, ClothSimSettings, &clmd->sim_parms);
 *
 * writer->write_struct_list(&action->markers);
 * BLO_read_struct_list(reader, TimeMarker, &action->markers);
 *
 * writer->write_int32_array(hmd->totindex, hmd->indexar);
 * if (!BLO_read_array(reader, &hmd->indexar, hmd->totindex)) {
 *   hmd->totindex = 0;
 * }
 * \endcode
 *
 * Avoid using the generic #BLO_read_raw_address when possible, use the typed functions instead.
 * Only data written with #BlendWriter::write_raw should typically be read with
 * #BLO_read_raw_address.
 * \{ */

void *blo_read_raw_address_impl(BlendDataReader *reader, const void *old_address);
#define BLO_read_raw_address(reader, ptr_p) \
  *((void **)ptr_p) = blo_read_raw_address_impl((reader), *(ptr_p))

/**
 * Read function for pointers to structs.
 *
 * NOTE: Currently the usage of the type info is very minimal/basic, it does a loose check on
 * the data size and marks the blend file as invalid when it's mismatched.
 */
void *blo_read_struct_impl(BlendDataReader *reader, const void *old_address, size_t expected_size);
#define BLO_read_struct(reader, struct_name, ptr_p) \
  (*((void **)ptr_p) = blo_read_struct_impl(reader, *((void **)ptr_p), sizeof(struct_name)))

/**
 * Like #BLO_read_struct, but mark the blend file as invalid (with an error report) when the
 * pointer was non-null but failed to resolve.
 */
void *blo_read_struct_nonnull_impl(BlendDataReader *reader,
                                   const void *old_address,
                                   size_t expected_size);
#define BLO_read_struct_nonnull(reader, struct_name, ptr_p) \
  (*((void **)ptr_p) = blo_read_struct_nonnull_impl( \
       reader, *((void **)ptr_p), sizeof(struct_name)))

/**
 * Like #BLO_read_struct, but does not consider the read data as 'used'. It will still be freed
 * by readfile code at the end of the reading process, if no other 'real' usage was detected.
 *
 * Typical valid usages include:
 * - Restoring pointers to a specific item in an array or list (usually 'active' item e.g.). The
 *   found item is expected to also be read as part of its array/list storage reading.
 * - Doing temporary access to deprecated data as part of some versioning code.
 */
void *blo_read_struct_no_us_impl(BlendDataReader *reader,
                                 const void *old_address,
                                 size_t expected_size);

#define BLO_read_struct_no_us(reader, struct_name, ptr_p) \
  (*((void **)ptr_p) = blo_read_struct_no_us_impl(reader, *((void **)ptr_p), sizeof(struct_name)))

#define BLO_read_struct_array_no_us(reader, struct_name, ptr_p, array_size) \
  (*((void **)ptr_p) = blo_read_struct_no_us_impl( \
       reader, *((void **)ptr_p), sizeof(struct_name) * size_t(array_size)))

/**
 * Like #BLO_read_struct_no_us, but with the same nonnull semantics as #BLO_read_struct_nonnull.
 */
void *blo_read_struct_no_us_nonnull_impl(BlendDataReader *reader,
                                         const void *old_address,
                                         size_t expected_size);

#define BLO_read_struct_no_us_nonnull(reader, struct_name, ptr_p) \
  (*((void **)ptr_p) = blo_read_struct_no_us_nonnull_impl( \
       reader, *((void **)ptr_p), sizeof(struct_name)))

/**
 * Similar to #BLO_read_struct, but can use a (DNA) type name instead of the type
 * itself to find the expected data size.
 *
 * Somewhat mirrors #BlendWriter::write_struct_array_by_name.
 */
void *BLO_read_struct_by_name_array(BlendDataReader *reader,
                                    StringRef struct_name,
                                    int64_t items_num,
                                    const void *old_address);

/* Read all elements in list
 *
 * Updates all `->prev` and `->next` pointers of the list elements.
 * Updates the `list->first` and `list->last` pointers.
 */
void BLO_read_struct_list_with_size(BlendDataReader *reader,
                                    size_t expected_elem_size,
                                    ListBase *list);

#define BLO_read_struct_list(reader, struct_name, list) \
  BLO_read_struct_list_with_size(reader, sizeof(struct_name), list)

/**
 * Read an array of typed elements (struct or primitive type) from the file.
 *
 * With corrupt blend files the size may not match the array memory allocation.
 * This must be handled either by using #BLO_read_array_and_validate_size to
 * automatically set the size to zero, or checking the return value of
 * #BLO_read_array to manually handle invalid data.
 *
 * Typically #BLO_read_array_and_validate_size should be used for cases where
 * a size member is only for the array pointer, while a size member shared
 * between multiple array needs particular handling.
 */
[[nodiscard]] bool blo_read_array_impl(
    BlendDataReader *reader, int64_t array_size, int elems, size_t elem_size, void **ptr_p);

template<typename T, typename SizeT>
  requires(!std::is_void_v<T> && !std::is_pointer_v<T>)
[[nodiscard]] bool BLO_read_array(BlendDataReader *reader,
                                  T **ptr_p,
                                  const SizeT array_size,
                                  const int elems = 1)
{
  return blo_read_array_impl(
      reader, int64_t(array_size), elems, sizeof(T), reinterpret_cast<void **>(ptr_p));
}

template<typename T, typename SizeT>
  requires(!std::is_void_v<T> && !std::is_pointer_v<T>)
void BLO_read_array_and_validate_size(BlendDataReader *reader,
                                      T **ptr_p,
                                      SizeT *array_size,
                                      const int elems = 1)
{
  if (!blo_read_array_impl(
          reader, int64_t(*array_size), elems, sizeof(T), reinterpret_cast<void **>(ptr_p)))
  {
    *array_size = 0;
  }
}

/**
 * Read an array of pointers, converting between 32/64-bit pointer sizes if needed.
 * Same size mismatch handling as #BLO_read_array.
 */
[[nodiscard]] bool blo_read_pointer_array_impl(BlendDataReader *reader,
                                               int64_t array_size,
                                               void **ptr_p);

template<typename T, typename SizeT>
  requires(std::is_pointer_v<T> || std::is_void_v<T>)
[[nodiscard]] bool BLO_read_pointer_array(BlendDataReader *reader,
                                          T **ptr_p,
                                          const SizeT array_size)
{
  return blo_read_pointer_array_impl(reader, array_size, reinterpret_cast<void **>(ptr_p));
}

template<typename T, typename SizeT>
  requires(std::is_pointer_v<T> || std::is_void_v<T>)
void BLO_read_pointer_array_and_validate_size(BlendDataReader *reader,
                                              T **ptr_p,
                                              SizeT *array_size)
{
  if (!blo_read_pointer_array_impl(reader, *array_size, reinterpret_cast<void **>(ptr_p))) {
    *array_size = 0;
  }
}

/* Read null terminated string. */

void BLO_read_string(BlendDataReader *reader, char **ptr_p);
void BLO_read_string(BlendDataReader *reader, char *const *ptr_p);
void BLO_read_string(BlendDataReader *reader, const char **ptr_p);

/* Misc. */

ImplicitSharingInfoAndData blo_read_shared_impl(
    BlendDataReader *reader,
    const void **ptr_p,
    FunctionRef<const ImplicitSharingInfo *()> read_fn);

/**
 * Check if there is any shared data for the given data pointer. If yes, return the existing
 * sharing-info. If not, call the provided function to actually read the data now.
 */
template<typename T>
const ImplicitSharingInfo *BLO_read_shared(BlendDataReader *reader,
                                           T **data_ptr,
                                           FunctionRef<const ImplicitSharingInfo *()> read_fn)
{
  ImplicitSharingInfoAndData shared_data = blo_read_shared_impl(
      reader, (const void **)data_ptr, read_fn);
  /* Need const-cast here, because not all DNA members that reference potentially shared data are
   * const yet. */
  *data_ptr = const_cast<T *>(static_cast<const T *>(shared_data.data));
  return shared_data.sharing_info;
}

int BLO_read_fileversion_get(BlendDataReader *reader);
bool BLO_read_data_is_undo(BlendDataReader *reader);
void BLO_read_data_globmap_add(BlendDataReader *reader, void *oldaddr, void *newaddr);
void BLO_read_glob_list(BlendDataReader *reader, ListBase *list);
BlendFileReadReport *BLO_read_data_reports(BlendDataReader *reader);
struct Library *BLO_read_data_current_library(BlendDataReader *reader);
void BLO_read_data_set_need_preview_render_restart(BlendDataReader *reader);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Blend Read Lib API
 *
 * This API does almost the same as the Blend Read Data API.
 * However, now only pointers to ID data blocks are updated.
 * \{ */

/**
 * Search for the new address of given `id`,
 * during library linking part of blend-file reading process.
 *
 * \param self_id: the ID owner of the given `id` pointer. Note that it may be an embedded ID.
 * \param is_linked_only: If `true`, only return found pointer if it is a linked ID. Used to
 * prevent linked data to point to local IDs.
 * \return the new address of the given ID pointer, or null if not found.
 */
ID *BLO_read_get_new_id_address(BlendLibReader *reader,
                                ID *self_id,
                                const bool is_linked_only,
                                ID *id) ATTR_NONNULL(2);

/**
 * Search for the new address of the ID for the given `session_uid`.
 *
 * Only IDs existing in the newly read Main will be returned. If no matching `session_uid` in new
 * main can be found, `nullptr` is returned.
 *
 * This expected to be used during library-linking and/or 'undo_preserve' processes in undo case
 * (i.e. memfile reading), typically to find a valid value (or nullptr) for ID pointers values
 * coming from the previous, existing Main data, when it is preserved in newly read Main.
 * See e.g. the #scene_undo_preserve code-path.
 */
ID *BLO_read_get_new_id_address_from_session_uid(BlendLibReader *reader, uint session_uid)
    ATTR_NONNULL(1);

/* Misc. */

bool BLO_read_lib_is_undo(BlendLibReader *reader);
Main *BLO_read_lib_get_main(BlendLibReader *reader);
BlendFileReadReport *BLO_read_lib_reports(BlendLibReader *reader);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Report API
 * \{ */

/**
 * This function ensures that reports are printed,
 * in the case of library linking errors this is important!
 *
 * NOTE(@ideasman42) a kludge but better than doubling up on prints,
 * we could alternatively have a versions of a report function which forces printing.
 */
void BLO_reportf_wrap(BlendFileReadReport *reports, eReportType type, const char *format, ...)
    ATTR_PRINTF_FORMAT(3, 4);

/** \} */

}  // namespace blender

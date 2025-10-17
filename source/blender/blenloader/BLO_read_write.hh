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

#include "DNA_sdna_type_ids.hh"

#include "BLI_function_ref.hh"
#include "BLI_implicit_sharing.hh"
#include "BLI_memory_utils.hh"

namespace blender {
class ImplicitSharingInfo;
}
struct BlendDataReader;
struct BlendFileReadReport;
struct BlendLibReader;
struct BlendWriter;
struct ID;
struct ListBase;
struct Main;
enum eReportType : uint16_t;

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
 *   #BLO_get_struct_id_by_name. Providing this ID can be a useful optimization when many
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
 * At the core there is #BLO_write_raw, which can write arbitrary memory buffers to the file.
 * The code that reads this data might have to correct its byte-order. For the common cases
 * there are convenience functions that write and read arrays of simple types such as `int32`.
 * Those will correct endianness automatically.
 * \{ */

/**
 * Mapping between names and ids.
 */
int BLO_get_struct_id_by_name(const BlendWriter *writer, const char *struct_name);

/**
 * Write single struct.
 */
void BLO_write_struct_by_name(BlendWriter *writer, const char *struct_name, const void *data_ptr);
void BLO_write_struct_by_id(BlendWriter *writer, int struct_id, const void *data_ptr);
#define BLO_write_struct(writer, struct_name, data_ptr) \
  BLO_write_struct_by_id(writer, blender::dna::sdna_struct_id_get<struct_name>(), data_ptr)

/**
 * Write single struct at address.
 */
void BLO_write_struct_at_address_by_id(BlendWriter *writer,
                                       int struct_id,
                                       const void *address,
                                       const void *data_ptr);
#define BLO_write_struct_at_address(writer, struct_name, address, data_ptr) \
  BLO_write_struct_at_address_by_id( \
      writer, blender::dna::sdna_struct_id_get<struct_name>(), address, data_ptr)

/**
 * Write single struct at address and specify a file-code.
 */
void BLO_write_struct_at_address_by_id_with_filecode(
    BlendWriter *writer, int filecode, int struct_id, const void *address, const void *data_ptr);
#define BLO_write_struct_at_address_with_filecode( \
    writer, filecode, struct_name, address, data_ptr) \
  BLO_write_struct_at_address_by_id_with_filecode( \
      writer, filecode, blender::dna::sdna_struct_id_get<struct_name>(), address, data_ptr)

/**
 * Write struct array.
 */
void BLO_write_struct_array_by_name(BlendWriter *writer,
                                    const char *struct_name,
                                    int64_t array_size,
                                    const void *data_ptr);
void BLO_write_struct_array_by_id(BlendWriter *writer,
                                  int struct_id,
                                  int64_t array_size,
                                  const void *data_ptr);
#define BLO_write_struct_array(writer, struct_name, array_size, data_ptr) \
  BLO_write_struct_array_by_id( \
      writer, blender::dna::sdna_struct_id_get<struct_name>(), array_size, data_ptr)

/**
 * Write struct array at address.
 */
void BLO_write_struct_array_at_address_by_id(BlendWriter *writer,
                                             int struct_id,
                                             int64_t array_size,
                                             const void *address,
                                             const void *data_ptr);
#define BLO_write_struct_array_at_address(writer, struct_name, array_size, address, data_ptr) \
  BLO_write_struct_array_at_address_by_id( \
      writer, blender::dna::sdna_struct_id_get<struct_name>(), array_size, address, data_ptr)

/**
 * Write struct list.
 */
void BLO_write_struct_list_by_name(BlendWriter *writer, const char *struct_name, ListBase *list);
void BLO_write_struct_list_by_id(BlendWriter *writer, int struct_id, const ListBase *list);
#define BLO_write_struct_list(writer, struct_name, list_ptr) \
  BLO_write_struct_list_by_id(writer, blender::dna::sdna_struct_id_get<struct_name>(), list_ptr)

/**
 * Write id struct.
 */
void blo_write_id_struct(BlendWriter *writer, int struct_id, const void *id_address, const ID *id);
#define BLO_write_id_struct(writer, struct_name, id_address, id) \
  blo_write_id_struct(writer, blender::dna::sdna_struct_id_get<struct_name>(), id_address, id)

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
  blender::DynamicStackBuffer<static_size> buffer_;

 public:
  BLO_Write_IDBuffer(ID &id, bool is_undo, bool is_placeholder);
  BLO_Write_IDBuffer(ID &id, BlendWriter *writer);

  ID *get()
  {
    return static_cast<ID *>(buffer_.buffer());
  };
};

/**
 * Write raw data.
 *
 * \warning Avoid using this function if possible. There are only a very few cases in current code
 * where it is actually needed (e.g. the ShapeKey's data, since its items size varies depending on
 * the type of geometry owning it, see #shapekey_blend_write).
 *
 * \warning Data written with this call have no type information attached to them
 * in the blend-file. The main consequence is that there will be no handling of endianness
 * conversion for them in readfile code.
 * Basic types array functions (like #BLO_write_int8_array etc.) also use #BLO_write_raw
 * internally, but if their matching read function is used to load the data (like
 * #BLO_read_int8_array), the read function will take care of endianness conversion.
 */
void BLO_write_raw(BlendWriter *writer, size_t size_in_bytes, const void *data_ptr);
/**
 * Slightly 'safer' code to write arrays of basic types data.
 */
void BLO_write_char_array(BlendWriter *writer, int64_t num, const char *data_ptr);
void BLO_write_int8_array(BlendWriter *writer, int64_t num, const int8_t *data_ptr);
void BLO_write_int16_array(BlendWriter *writer, int64_t num, const int16_t *data_ptr);
void BLO_write_uint8_array(BlendWriter *writer, int64_t num, const uint8_t *data_ptr);
void BLO_write_int32_array(BlendWriter *writer, int64_t num, const int32_t *data_ptr);
void BLO_write_uint32_array(BlendWriter *writer, int64_t num, const uint32_t *data_ptr);
void BLO_write_float_array(BlendWriter *writer, int64_t num, const float *data_ptr);
void BLO_write_double_array(BlendWriter *writer, int64_t num, const double *data_ptr);
void BLO_write_float3_array(BlendWriter *writer, int64_t num, const float *data_ptr);
void BLO_write_pointer_array(BlendWriter *writer, int64_t num, const void *data_ptr);
/**
 * Write a null terminated string.
 */
void BLO_write_string(BlendWriter *writer, const char *data_ptr);

/* Misc. */

/**
 * Check if the data can be written more efficiently by making use of implicit-sharing. If yes, the
 * user count of the sharing-info is increased making the data immutable. The provided callback
 * should serialize the potentially shared data. It is only called when necessary.
 *
 * This should be called before the data is referenced in other written data (there is an assert
 * that checks for this). If that's not possible, at least #BLO_write_shared_tag needs to be called
 * before the pointer is first written.
 *
 * \param approximate_size_in_bytes: Used to be able to approximate how large the undo step is in
 * total.
 * \param write_fn: Use the #BlendWrite to serialize the potentially shared data.
 */
void BLO_write_shared(BlendWriter *writer,
                      const void *data,
                      size_t approximate_size_in_bytes,
                      const blender::ImplicitSharingInfo *sharing_info,
                      blender::FunctionRef<void()> write_fn);

/**
 * Needs to be called if the pointer is somewhere written before the call to #BLO_write_shared.
 */
void BLO_write_shared_tag(BlendWriter *writer, const void *data);

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
 * BLO_write_struct(writer, ClothSimSettings, clmd->sim_parms);
 * BLO_read_struct(reader, ClothSimSettings, &clmd->sim_parms);
 *
 * BLO_write_struct_list(writer, TimeMarker, &action->markers);
 * BLO_read_struct_list(reader, TimeMarker, &action->markers);
 *
 * BLO_write_int32_array(writer, hmd->totindex, hmd->indexar);
 * BLO_read_int32_array(reader, hmd->totindex, &hmd->indexar);
 * \endcode
 *
 * Avoid using the generic #BLO_read_data_address
 * (and low-level API like #BLO_read_get_new_data_address)
 * when possible, use the typed functions instead.
 * Only data written with #BLO_write_raw should typically be read with #BLO_read_data_address.
 * \{ */

void *BLO_read_get_new_data_address(BlendDataReader *reader, const void *old_address);
#define BLO_read_data_address(reader, ptr_p) \
  *((void **)ptr_p) = BLO_read_get_new_data_address((reader), *(ptr_p))

/**
 * Does not consider the read data as 'used'. It will still be freed by readfile code at the
 * end of the reading process, if no other 'real' usage was detected for it.
 *
 * Typical valid usages include:
 * - Restoring pointers to a specific item in an array or list (usually 'active' item e.g.). The
 *   found item is expected to also be read as part of its array/list storage reading.
 * - Doing temporary access to deprecated data as part of some versioning code.
 */
void *BLO_read_get_new_data_address_no_us(BlendDataReader *reader,
                                          const void *old_address,
                                          size_t expected_size);

/**
 * The 'main' read function and helper macros for non-basic data types.
 *
 * NOTE: Currently the usage of the type info is very minimal/basic, it merely does a lose check on
 * the data size.
 */
void *BLO_read_struct_array_with_size(BlendDataReader *reader,
                                      const void *old_address,
                                      size_t expected_size);
#define BLO_read_struct(reader, struct_name, ptr_p) \
  *((void **)ptr_p) = BLO_read_struct_array_with_size( \
      reader, *((void **)ptr_p), sizeof(struct_name))
#define BLO_read_struct_array(reader, struct_name, array_size, ptr_p) \
  *((void **)ptr_p) = BLO_read_struct_array_with_size( \
      reader, *((void **)ptr_p), sizeof(struct_name) * (array_size))

/**
 * Similar to #BLO_read_struct_array_with_size, but can use a (DNA) type name instead of the type
 * itself to find the expected data size.
 *
 * Somewhat mirrors #BLO_write_struct_array_by_name.
 */
void *BLO_read_struct_by_name_array(BlendDataReader *reader,
                                    const char *struct_name,
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

/* Update data pointers and correct byte-order if necessary. */

void BLO_read_char_array(BlendDataReader *reader, int64_t array_size, char **ptr_p);
void BLO_read_int8_array(BlendDataReader *reader, int64_t array_size, int8_t **ptr_p);
void BLO_read_uint8_array(BlendDataReader *reader, int64_t array_size, uint8_t **ptr_p);
void BLO_read_int16_array(BlendDataReader *reader, const int64_t array_size, int16_t **ptr_p);
void BLO_read_int32_array(BlendDataReader *reader, int64_t array_size, int32_t **ptr_p);
void BLO_read_uint32_array(BlendDataReader *reader, int64_t array_size, uint32_t **ptr_p);
void BLO_read_float_array(BlendDataReader *reader, int64_t array_size, float **ptr_p);
void BLO_read_float3_array(BlendDataReader *reader, int64_t array_size, float **ptr_p);
void BLO_read_double_array(BlendDataReader *reader, int64_t array_size, double **ptr_p);
void BLO_read_pointer_array(BlendDataReader *reader, int64_t array_size, void **ptr_p);

/* Read null terminated string. */

void BLO_read_string(BlendDataReader *reader, char **ptr_p);
void BLO_read_string(BlendDataReader *reader, char *const *ptr_p);
void BLO_read_string(BlendDataReader *reader, const char **ptr_p);

/* Misc. */

blender::ImplicitSharingInfoAndData blo_read_shared_impl(
    BlendDataReader *reader,
    const void **ptr_p,
    blender::FunctionRef<const blender::ImplicitSharingInfo *()> read_fn);

/**
 * Check if there is any shared data for the given data pointer. If yes, return the existing
 * sharing-info. If not, call the provided function to actually read the data now.
 */
template<typename T>
const blender::ImplicitSharingInfo *BLO_read_shared(
    BlendDataReader *reader,
    T **data_ptr,
    blender::FunctionRef<const blender::ImplicitSharingInfo *()> read_fn)
{
  blender::ImplicitSharingInfoAndData shared_data = blo_read_shared_impl(
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

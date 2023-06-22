/* SPDX-FileCopyrightText: 2023 Blender Foundation
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
 *
 * Each of these callbacks uses a different API functions.
 *
 * Some parts of Blender, e.g. modifiers, don't require you to implement all four callbacks.
 * Instead only the first two are necessary. The other two are handled by general ID management. In
 * the future, we might want to get rid of those two callbacks entirely, but for now they are
 * necessary.
 */

#pragma once

/* for SDNA_TYPE_FROM_STRUCT() macro */
#include "dna_type_offsets.h"

#include "DNA_windowmanager_types.h" /* for eReportType */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BlendDataReader BlendDataReader;
typedef struct BlendExpander BlendExpander;
typedef struct BlendLibReader BlendLibReader;
typedef struct BlendWriter BlendWriter;

struct BlendFileReadReport;
struct Main;

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
int BLO_get_struct_id_by_name(BlendWriter *writer, const char *struct_name);
#define BLO_get_struct_id(writer, struct_name) SDNA_TYPE_FROM_STRUCT(struct_name)

/**
 * Write single struct.
 */
void BLO_write_struct_by_name(BlendWriter *writer, const char *struct_name, const void *data_ptr);
void BLO_write_struct_by_id(BlendWriter *writer, int struct_id, const void *data_ptr);
#define BLO_write_struct(writer, struct_name, data_ptr) \
  BLO_write_struct_by_id(writer, BLO_get_struct_id(writer, struct_name), data_ptr)

/**
 * Write single struct at address.
 */
void BLO_write_struct_at_address_by_id(BlendWriter *writer,
                                       int struct_id,
                                       const void *address,
                                       const void *data_ptr);
#define BLO_write_struct_at_address(writer, struct_name, address, data_ptr) \
  BLO_write_struct_at_address_by_id( \
      writer, BLO_get_struct_id(writer, struct_name), address, data_ptr)

/**
 * Write single struct at address and specify a file-code.
 */
void BLO_write_struct_at_address_by_id_with_filecode(
    BlendWriter *writer, int filecode, int struct_id, const void *address, const void *data_ptr);
#define BLO_write_struct_at_address_with_filecode( \
    writer, filecode, struct_name, address, data_ptr) \
  BLO_write_struct_at_address_by_id_with_filecode( \
      writer, filecode, BLO_get_struct_id(writer, struct_name), address, data_ptr)

/**
 * Write struct array.
 */
void BLO_write_struct_array_by_name(BlendWriter *writer,
                                    const char *struct_name,
                                    int array_size,
                                    const void *data_ptr);
void BLO_write_struct_array_by_id(BlendWriter *writer,
                                  int struct_id,
                                  int array_size,
                                  const void *data_ptr);
#define BLO_write_struct_array(writer, struct_name, array_size, data_ptr) \
  BLO_write_struct_array_by_id( \
      writer, BLO_get_struct_id(writer, struct_name), array_size, data_ptr)

/**
 * Write struct array at address.
 */
void BLO_write_struct_array_at_address_by_id(
    BlendWriter *writer, int struct_id, int array_size, const void *address, const void *data_ptr);
#define BLO_write_struct_array_at_address(writer, struct_name, array_size, address, data_ptr) \
  BLO_write_struct_array_at_address_by_id( \
      writer, BLO_get_struct_id(writer, struct_name), array_size, address, data_ptr)

/**
 * Write struct list.
 */
void BLO_write_struct_list_by_name(BlendWriter *writer,
                                   const char *struct_name,
                                   struct ListBase *list);
void BLO_write_struct_list_by_id(BlendWriter *writer, int struct_id, struct ListBase *list);
#define BLO_write_struct_list(writer, struct_name, list_ptr) \
  BLO_write_struct_list_by_id(writer, BLO_get_struct_id(writer, struct_name), list_ptr)

/**
 * Write id struct.
 */
void blo_write_id_struct(BlendWriter *writer,
                         int struct_id,
                         const void *id_address,
                         const struct ID *id);
#define BLO_write_id_struct(writer, struct_name, id_address, id) \
  blo_write_id_struct(writer, BLO_get_struct_id(writer, struct_name), id_address, id)

/**
 * Specific code to prepare IDs to be written.
 *
 * Required for writing properly embedded IDs currently.
 *
 * \note Once there is a better generic handling of embedded IDs,
 * this may go back to private code in `writefile.c`.
 */
typedef struct BLO_Write_IDBuffer BLO_Write_IDBuffer;

BLO_Write_IDBuffer *BLO_write_allocate_id_buffer(void);
void BLO_write_init_id_buffer_from_id(BLO_Write_IDBuffer *id_buffer,
                                      struct ID *id,
                                      const bool is_undo);
struct ID *BLO_write_get_id_buffer_temp_id(BLO_Write_IDBuffer *id_buffer);
void BLO_write_destroy_id_buffer(BLO_Write_IDBuffer **id_buffer);

/**
 * Write raw data.
 */
void BLO_write_raw(BlendWriter *writer, size_t size_in_bytes, const void *data_ptr);
void BLO_write_int8_array(BlendWriter *writer, uint num, const int8_t *data_ptr);
void BLO_write_int32_array(BlendWriter *writer, uint num, const int32_t *data_ptr);
void BLO_write_uint32_array(BlendWriter *writer, uint num, const uint32_t *data_ptr);
void BLO_write_float_array(BlendWriter *writer, uint num, const float *data_ptr);
void BLO_write_double_array(BlendWriter *writer, uint num, const double *data_ptr);
void BLO_write_float3_array(BlendWriter *writer, uint num, const float *data_ptr);
void BLO_write_pointer_array(BlendWriter *writer, uint num, const void *data_ptr);
/**
 * Write a null terminated string.
 */
void BLO_write_string(BlendWriter *writer, const char *data_ptr);

/* Misc. */

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
 * BLO_read_data_address(reader, &clmd->sim_parms);
 *
 * BLO_write_struct_list(writer, TimeMarker, &action->markers);
 * BLO_read_list(reader, &action->markers);
 *
 * BLO_write_int32_array(writer, hmd->totindex, hmd->indexar);
 * BLO_read_int32_array(reader, hmd->totindex, &hmd->indexar);
 * \endcode
 * \{ */

void *BLO_read_get_new_data_address(BlendDataReader *reader, const void *old_address);
void *BLO_read_get_new_data_address_no_us(BlendDataReader *reader, const void *old_address);
void *BLO_read_get_new_packed_address(BlendDataReader *reader, const void *old_address);

#define BLO_read_data_address(reader, ptr_p) \
  *((void **)ptr_p) = BLO_read_get_new_data_address((reader), *(ptr_p))
#define BLO_read_packed_address(reader, ptr_p) \
  *((void **)ptr_p) = BLO_read_get_new_packed_address((reader), *(ptr_p))

typedef void (*BlendReadListFn)(BlendDataReader *reader, void *data);
/**
 * Updates all `->prev` and `->next` pointers of the list elements.
 * Updates the `list->first` and `list->last` pointers.
 * When not NULL, calls the callback on every element.
 */
void BLO_read_list_cb(BlendDataReader *reader, struct ListBase *list, BlendReadListFn callback);
void BLO_read_list(BlendDataReader *reader, struct ListBase *list);

/* Update data pointers and correct byte-order if necessary. */

void BLO_read_int8_array(BlendDataReader *reader, int array_size, int8_t **ptr_p);
void BLO_read_int32_array(BlendDataReader *reader, int array_size, int32_t **ptr_p);
void BLO_read_uint32_array(BlendDataReader *reader, int array_size, uint32_t **ptr_p);
void BLO_read_float_array(BlendDataReader *reader, int array_size, float **ptr_p);
void BLO_read_float3_array(BlendDataReader *reader, int array_size, float **ptr_p);
void BLO_read_double_array(BlendDataReader *reader, int array_size, double **ptr_p);
void BLO_read_pointer_array(BlendDataReader *reader, void **ptr_p);

/* Misc. */

int BLO_read_fileversion_get(BlendDataReader *reader);
bool BLO_read_requires_endian_switch(BlendDataReader *reader);
bool BLO_read_data_is_undo(BlendDataReader *reader);
void BLO_read_data_globmap_add(BlendDataReader *reader, void *oldaddr, void *newaddr);
void BLO_read_glob_list(BlendDataReader *reader, struct ListBase *list);
struct BlendFileReadReport *BLO_read_data_reports(BlendDataReader *reader);

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
 * \param do_linked_only: If `true`, only return found pointer if it is a linked ID. Used to
 * prevent linked data to point to local IDs.
 * \return the new address of the given ID pointer, or null if not found.
 */
struct ID *BLO_read_get_new_id_address(BlendLibReader *reader,
                                       struct ID *self_id,
                                       const bool do_linked_only,
                                       struct ID *id) ATTR_NONNULL(2);

#define BLO_read_id_address(reader, self_id, id_ptr_p) \
  *((void **)id_ptr_p) = (void *)BLO_read_get_new_id_address( \
      (reader), (self_id), (self_id) && ID_IS_LINKED(self_id), (ID *)*(id_ptr_p))

/**
 * Search for the new address of the ID for the given `session_uuid`.
 *
 * Only IDs existing in the newly read Main will be returned. If no matching `session_uuid` in new
 * main can be found, `nullptr` is returned.
 *
 * This expected to be used during library-linking and/or 'undo_preserve' processes in undo case
 * (i.e. memfile reading), typically to find a valid value (or nullptr) for ID pointers values
 * coming from the previous, existing Main data, when it is preserved in newly read Main.
 * See e.g. the #scene_undo_preserve code-path.
 */
struct ID *BLO_read_get_new_id_address_from_session_uuid(BlendLibReader *reader, uint session_uuid)
    ATTR_NONNULL(1);

/* Misc. */

bool BLO_read_lib_is_undo(BlendLibReader *reader);
struct Main *BLO_read_lib_get_main(BlendLibReader *reader);
struct BlendFileReadReport *BLO_read_lib_reports(BlendLibReader *reader);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Blend Expand API
 *
 * BLO_expand has to be called for every data block that should be loaded. If the data block is in
 * a separate `.blend` file, it will be pulled from there.
 * \{ */

void BLO_expand_id(BlendExpander *expander, struct ID *id);

#define BLO_expand(expander, id) BLO_expand_id(expander, (struct ID *)id)

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
void BLO_reportf_wrap(struct BlendFileReadReport *reports,
                      eReportType type,
                      const char *format,
                      ...) ATTR_PRINTF_FORMAT(3, 4);

/** \} */

#ifdef __cplusplus
}
#endif

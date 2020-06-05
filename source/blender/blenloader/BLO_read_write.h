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

#ifndef __BLO_READ_WRITE_H__
#define __BLO_READ_WRITE_H__

/* for SDNA_TYPE_FROM_STRUCT() macro */
#include "dna_type_offsets.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct BlendWriter BlendWriter;
typedef struct BlendDataReader BlendDataReader;
typedef struct BlendLibReader BlendLibReader;
typedef struct BlendExpander BlendExpander;

/* Blend Write API
 * ===============
 *
 * Most functions fall into one of two categories. Either they write a DNA struct or a raw memory
 * buffer to the .blend file.
 *
 * It is safe to pass NULL as data_ptr. In this case nothing will be stored.
 *
 * DNA Struct Writing
 * ------------------
 *
 * Functions dealing with DNA structs begin with BLO_write_struct_*.
 *
 * DNA struct types can be identified in different ways:
 *  - Run-time Name: The name is provided as const char *.
 *  - Compile-time Name: The name is provided at compile time. This is more efficient.
 *  - Struct ID: Every DNA struct type has an integer ID that can be queried with
 *      BLO_get_struct_id_by_name. Providing this ID can be a useful optimization when many structs
 *      of the same type are stored AND if those structs are not in a continuous array.
 *
 * Often only a single instance of a struct is written at once. However, sometimes it is necessary
 * to write arrays or linked lists. Separate functions for that are provided as well.
 *
 * There is a special macro for writing id structs: BLO_write_id_struct. Those are handled
 * differently from other structs.
 *
 * Raw Data Writing
 * ----------------
 *
 * At the core there is BLO_write_raw, which can write arbitrary memory buffers to the file. The
 * code that reads this data might have to correct its byte-order. For the common cases there are
 * convenience functions that write and read arrays of simple types such as int32. Those will
 * correct endianness automatically.
 */

/* Mapping between names and ids. */
int BLO_get_struct_id_by_name(BlendWriter *writer, const char *struct_name);
#define BLO_get_struct_id(writer, struct_name) SDNA_TYPE_FROM_STRUCT(struct_name)

/* Write single struct. */
void BLO_write_struct_by_name(BlendWriter *writer, const char *struct_name, const void *data_ptr);
void BLO_write_struct_by_id(BlendWriter *writer, int struct_id, const void *data_ptr);
#define BLO_write_struct(writer, struct_name, data_ptr) \
  BLO_write_struct_by_id(writer, BLO_get_struct_id(writer, struct_name), data_ptr)

/* Write single struct at address. */
void BLO_write_struct_at_address_by_id(BlendWriter *writer,
                                       int struct_id,
                                       const void *address,
                                       const void *data_ptr);
#define BLO_write_struct_at_address(writer, struct_name, address, data_ptr) \
  BLO_write_struct_at_address_by_id( \
      writer, BLO_get_struct_id(writer, struct_name), address, data_ptr)

/* Write struct array. */
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

/* Write struct array at address. */
void BLO_write_struct_array_at_address_by_id(
    BlendWriter *writer, int struct_id, int array_size, const void *address, const void *data_ptr);
#define BLO_write_struct_array_at_address(writer, struct_name, array_size, address, data_ptr) \
  BLO_write_struct_array_at_address_by_id( \
      writer, BLO_get_struct_id(writer, struct_name), array_size, address, data_ptr)

/* Write struct list. */
void BLO_write_struct_list_by_name(BlendWriter *writer,
                                   const char *struct_name,
                                   struct ListBase *list);
void BLO_write_struct_list_by_id(BlendWriter *writer, int struct_id, struct ListBase *list);
#define BLO_write_struct_list(writer, struct_name, list_ptr) \
  BLO_write_struct_list_by_id(writer, BLO_get_struct_id(writer, struct_name), list_ptr)

/* Write id struct. */
void blo_write_id_struct(BlendWriter *writer,
                         int struct_id,
                         const void *id_address,
                         const struct ID *id);
#define BLO_write_id_struct(writer, struct_name, id_address, id) \
  blo_write_id_struct(writer, BLO_get_struct_id(writer, struct_name), id_address, id)

/* Write raw data. */
void BLO_write_raw(BlendWriter *writer, int size_in_bytes, const void *data_ptr);
void BLO_write_int32_array(BlendWriter *writer, int size, const int32_t *data_ptr);
void BLO_write_uint32_array(BlendWriter *writer, int size, const uint32_t *data_ptr);
void BLO_write_float_array(BlendWriter *writer, int size, const float *data_ptr);
void BLO_write_float3_array(BlendWriter *writer, int size, const float *data_ptr);
void BLO_write_pointer_array(BlendWriter *writer, int size, const void *data_ptr);
void BLO_write_string(BlendWriter *writer, const char *data_ptr);

/* Misc. */
bool BLO_write_is_undo(BlendWriter *writer);

/* Blend Read Data API
 * ===================
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
 *   BLO_write_struct(writer, ClothSimSettings, clmd->sim_parms);
 *   BLO_read_data_address(reader, &clmd->sim_parms);
 *
 *   BLO_write_struct_list(writer, TimeMarker, &action->markers);
 *   BLO_read_list(reader, &action->markers, NULL);
 *
 *   BLO_write_int32_array(writer, hmd->totindex, hmd->indexar);
 *   BLO_read_int32_array(reader, hmd->totindex, &hmd->indexar);
 */

void *BLO_read_get_new_data_address(BlendDataReader *reader, const void *old_address);

#define BLO_read_data_address(reader, ptr_p) \
  *(ptr_p) = BLO_read_get_new_data_address((reader), *(ptr_p))

typedef void (*BlendReadListFn)(BlendDataReader *reader, void *data);
void BLO_read_list(BlendDataReader *reader, struct ListBase *list, BlendReadListFn callback);

/* Update data pointers and correct byte-order if necessary. */
void BLO_read_int32_array(BlendDataReader *reader, int array_size, int32_t **ptr_p);
void BLO_read_uint32_array(BlendDataReader *reader, int array_size, uint32_t **ptr_p);
void BLO_read_float_array(BlendDataReader *reader, int array_size, float **ptr_p);
void BLO_read_float3_array(BlendDataReader *reader, int array_size, float **ptr_p);
void BLO_read_double_array(BlendDataReader *reader, int array_size, double **ptr_p);
void BLO_read_pointer_array(BlendDataReader *reader, void **ptr_p);

/* Misc. */
bool BLO_read_requires_endian_switch(BlendDataReader *reader);

/* Blend Read Lib API
 * ===================
 *
 * This API does almost the same as the Blend Read Data API. However, now only pointers to ID data
 * blocks are updated.
 */

ID *BLO_read_get_new_id_address(BlendLibReader *reader, struct Library *lib, struct ID *id);

#define BLO_read_id_address(reader, lib, id_ptr_p) \
  *(id_ptr_p) = (void *)BLO_read_get_new_id_address((reader), (lib), (ID *)*(id_ptr_p))

/* Blend Expand API
 * ===================
 *
 * BLO_expand has to be called for every data block that should be loaded. If the data block is in
 * a separate .blend file, it will be pulled from there.
 */

void BLO_expand_id(BlendExpander *expander, struct ID *id);

#define BLO_expand(expander, id) BLO_expand_id(expander, (struct ID *)id)

#ifdef __cplusplus
}
#endif

#endif /* __BLO_READ_WRITE_H__ */

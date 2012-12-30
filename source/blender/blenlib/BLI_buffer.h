/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_BUFFER_H__
#define __BLI_BUFFER_H__

/* Note: this more or less fills same purpose as BLI_array, but makes
 * it much easier to resize the array outside of the function it was
 * declared in since */

/* Usage examples:
 *
 * { 
 *     BLI_buffer_declare(int, my_int_array, 32);
 *
 *     BLI_buffer_append(my_int_array, int, 42);
 *     assert(my_int_array.count == 1);
 *     assert(BLI_buffer_at(my_int_array, int, 0) == 42);
 *
 *     BLI_buffer_free(&my_int_array);
 * }
 */

typedef struct {
	void *data;
	const int elem_size;
	int count, alloc_count;
	int using_static;
} BLI_Buffer;

#define BLI_buffer_declare(type_, name_, static_count_) \
	type_ *name_ ## _static_[static_count_]; \
	BLI_Buffer name_ = {name_ ## _static_, \
						sizeof(type_), \
						0, \
						static_count_, \
						TRUE}

#define BLI_buffer_at(buffer_, type_, index_) \
	(((type_*)(buffer_)->data)[index_])

#define BLI_buffer_append(buffer_, type_, val_) \
	BLI_buffer_resize(buffer_, (buffer_)->count + 1); \
	BLI_buffer_at(buffer_, type_, (buffer_)->count - 1) = val_

/* Never decreases the amount of memory allocated */
void BLI_buffer_resize(BLI_Buffer *buffer, int new_count);

/* Does not free the buffer structure itself */
void BLI_buffer_free(BLI_Buffer *buffer);

#endif

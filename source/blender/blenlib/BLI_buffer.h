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

/** \file BLI_buffer.h
 *  \ingroup bli
 *
 * \note this more or less fills same purpose as BLI_array, but makes
 * it much easier to resize the array outside of the function it was
 * declared in since.
 *
 * Usage examples:
 * \code
 * BLI_buffer_declare_static(int, my_int_array, BLI_BUFFER_NOP, 32);
 *
 * BLI_buffer_append(my_int_array, int, 42);
 * assert(my_int_array.count == 1);
 * assert(BLI_buffer_at(my_int_array, int, 0) == 42);
 *
 * BLI_buffer_free(&my_int_array);
 * \endcode
 */

typedef struct {
	void *data;
	const int elem_size;
	int count, alloc_count;
	int flag;
} BLI_Buffer;

enum {
	BLI_BUFFER_NOP        = 0,
	BLI_BUFFER_USE_STATIC = (1 << 0),
	BLI_BUFFER_USE_CALLOC = (1 << 1),  /* ensure the array is always calloc'd */
};

#define BLI_buffer_declare_static(type_, name_, flag_, static_count_) \
	char name_ ## user;  /* warn for free only */ \
	type_ name_ ## _static_[static_count_]; \
	BLI_Buffer name_ = { \
	/* clear the static memory if this is a calloc'd array */ \
	((void)((flag_ & BLI_BUFFER_USE_CALLOC) ? \
	          memset(name_ ## _static_, 0, sizeof(name_ ## _static_)) : NULL \
	), /* memset-end */ \
	                    name_ ## _static_), \
	                    sizeof(type_), \
	                    0, \
	                    static_count_, \
	                    BLI_BUFFER_USE_STATIC | flag_}

/* never use static*/
#define BLI_buffer_declare(type_, name_, flag_) \
	bool name_ ## user;  /* warn for free only */ \
	BLI_Buffer name_ = {NULL, \
	                    sizeof(type_), \
	                    0, \
	                    0, \
	                    flag_}

#define BLI_buffer_at(buffer_, type_, index_) ( \
	(((type_ *)(buffer_)->data)[ \
	        (BLI_assert(sizeof(type_) == (buffer_)->elem_size)), \
	        (BLI_assert(index_ >= 0 && index_ < (buffer_)->count)), \
	        index_]))

#define BLI_buffer_array(buffer_, type_) ( \
	&(BLI_buffer_at(buffer_, type_, 0)))

#define BLI_buffer_resize_data(buffer_, type_, new_count_) ( \
	(BLI_buffer_resize(buffer_, new_count_), new_count_ ? BLI_buffer_array(buffer_, type_) : NULL))



#define BLI_buffer_append(buffer_, type_, val_)  ( \
	BLI_buffer_resize(buffer_, (buffer_)->count + 1), \
	(BLI_buffer_at(buffer_, type_, (buffer_)->count - 1) = val_) \
)

/* Never decreases the amount of memory allocated */
void BLI_buffer_resize(BLI_Buffer *buffer, int new_count);

/* Does not free the buffer structure itself */
void _bli_buffer_free(BLI_Buffer *buffer);
#define BLI_buffer_free(name_) { \
	_bli_buffer_free(name_); \
	(void)name_ ## user;  /* ensure we free */ \
} (void)0

#endif  /* __BLI_BUFFER_H__ */

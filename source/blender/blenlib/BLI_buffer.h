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
 */

typedef struct {
	void *data;
	const size_t elem_size;
	size_t count, alloc_count;
	int flag;
} BLI_Buffer;

enum {
	BLI_BUFFER_NOP        = 0,
	BLI_BUFFER_USE_STATIC = (1 << 0),
};

#define BLI_buffer_declare_static(type_, name_, flag_, static_count_) \
	char name_ ## user;  /* warn for free only */ \
	type_ name_ ## _static_[static_count_]; \
	BLI_Buffer name_ = { \
	        (name_ ## _static_), \
	        sizeof(type_), \
	        0, \
	        static_count_, \
	        BLI_BUFFER_USE_STATIC | (flag_)}

/* never use static*/
#define BLI_buffer_declare(type_, name_, flag_) \
	bool name_ ## user;  /* warn for free only */ \
	BLI_Buffer name_ = { \
	        NULL, \
	        sizeof(type_), \
	        0, \
	        0, \
	        (flag_)}

#define BLI_buffer_at(buffer_, type_, index_) ( \
	(((type_ *)(buffer_)->data)[ \
	        (BLI_assert(sizeof(type_) == (buffer_)->elem_size)), \
	        (BLI_assert((int)(index_) >= 0 && (size_t)(index_) < (buffer_)->count)), \
	        index_]))

#define BLI_buffer_array(buffer_, type_) ( \
	&(BLI_buffer_at(buffer_, type_, 0)))

#define BLI_buffer_resize_data(buffer_, type_, new_count_) ( \
	(BLI_buffer_resize(buffer_, new_count_), new_count_ ? BLI_buffer_array(buffer_, type_) : NULL))

#define BLI_buffer_reinit_data(buffer_, type_, new_count_) ( \
	(BLI_buffer_reinit(buffer_, new_count_), new_count_ ? BLI_buffer_array(buffer_, type_) : NULL))

#define BLI_buffer_append(buffer_, type_, val_)  ( \
	BLI_buffer_resize(buffer_, (buffer_)->count + 1), \
	(BLI_buffer_at(buffer_, type_, (buffer_)->count - 1) = val_) \
)

#define BLI_buffer_empty(buffer_) { \
	(buffer_)->count = 0; \
} (void)0

/* Never decreases the amount of memory allocated */
void BLI_buffer_resize(BLI_Buffer *buffer, const size_t new_count);

/* Ensure size, throwing away old data, respecting BLI_BUFFER_USE_CALLOC */
void BLI_buffer_reinit(BLI_Buffer *buffer, const size_t new_count);

/* Does not free the buffer structure itself */
void _bli_buffer_free(BLI_Buffer *buffer);
#define BLI_buffer_free(name_) { \
	_bli_buffer_free(name_); \
	(void)name_ ## user;  /* ensure we free */ \
} (void)0

#endif  /* __BLI_BUFFER_H__ */

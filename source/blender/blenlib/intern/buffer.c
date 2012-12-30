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

#include "MEM_guardedalloc.h"

#include "BLI_buffer.h"
#include "BLI_utildefines.h"

#include <string.h>

void BLI_buffer_resize(BLI_Buffer *buffer, int new_count)
{
	if (new_count > buffer->alloc_count) {
		if (buffer->using_static) {
			void *orig = buffer->data;
			buffer->data = MEM_callocN(buffer->elem_size * new_count,
									   "BLI_Buffer.data");
			memcpy(buffer->data, orig, buffer->elem_size * buffer->count);
			buffer->alloc_count = new_count;
			buffer->using_static = FALSE;
		}
		else {
			if (new_count < buffer->alloc_count * 2)
				buffer->alloc_count *= 2;
			else
				buffer->alloc_count = new_count;
			buffer->data = MEM_reallocN(buffer->data,
										(buffer->elem_size *
										 buffer->alloc_count));
		}
	}

	buffer->count = new_count;
}

void BLI_buffer_free(BLI_Buffer *buffer)
{
	if (!buffer->using_static)
		MEM_freeN(buffer->data);
	memset(buffer, 0, sizeof(*buffer));
}

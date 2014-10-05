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

/** \file blender/blenlib/intern/buffer.c
 *  \ingroup bli
 */

#include "MEM_guardedalloc.h"

#include "BLI_buffer.h"
#include "BLI_utildefines.h"

#include <string.h>

static void *buffer_alloc(BLI_Buffer *buffer, int len)
{
	return ((buffer->flag & BLI_BUFFER_USE_CALLOC) ?
	        MEM_callocN : MEM_mallocN)
	        (buffer->elem_size * len, "BLI_Buffer.data");
}

static void *buffer_realloc(BLI_Buffer *buffer, int len)
{
	return ((buffer->flag & BLI_BUFFER_USE_CALLOC) ?
	        MEM_recallocN_id : MEM_reallocN_id)
	        (buffer->data, buffer->elem_size * len, "BLI_Buffer.data");
}

void BLI_buffer_resize(BLI_Buffer *buffer, int new_count)
{
	if (UNLIKELY(new_count > buffer->alloc_count)) {
		if (buffer->flag & BLI_BUFFER_USE_STATIC) {
			void *orig = buffer->data;

			buffer->data = buffer_alloc(buffer, new_count);
			memcpy(buffer->data, orig, buffer->elem_size * buffer->count);
			buffer->alloc_count = new_count;
			buffer->flag &= ~BLI_BUFFER_USE_STATIC;
		}
		else {
			if (buffer->alloc_count && (new_count < buffer->alloc_count * 2)) {
				buffer->alloc_count *= 2;
			}
			else {
				buffer->alloc_count = new_count;
			}

			buffer->data = buffer_realloc(buffer, buffer->alloc_count);
		}
	}

	buffer->count = new_count;
}

/* callers use BLI_buffer_free */
void _bli_buffer_free(BLI_Buffer *buffer)
{
	if ((buffer->flag & BLI_BUFFER_USE_STATIC) == 0) {
		if (buffer->data) {
			MEM_freeN(buffer->data);
		}
	}
	memset(buffer, 0, sizeof(*buffer));
}

/*
 * $Id$
 *
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
 * Contributor(s): Blender Foundation, 2010.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "IMB_imbuf.h"
#include "IMB_filetype.h"

void IMB_init(void)
{
	imb_filetypes_init();
	imb_tile_cache_init();
}

void IMB_exit(void)
{
	IMB_free_cache_limiter();
	imb_tile_cache_exit();
	imb_filetypes_exit();
}


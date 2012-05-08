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
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file collada.h
 *  \ingroup collada
 */

#ifndef __COLLADA_H__
#define __COLLADA_H__

struct bContext;
struct Scene;

#ifdef __cplusplus
extern "C" {
#endif
	/*
	 * both return 1 on success, 0 on error
	 */
	int collada_import(bContext *C, const char *filepath);
	int collada_export(Scene *sce, const char *filepath, int selected, int second_life);
#ifdef __cplusplus
}
#endif

#endif

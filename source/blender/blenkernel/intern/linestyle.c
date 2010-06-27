/* linestyle.c
 *
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
 * The Original Code is Copyright (C) 2010 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <stdlib.h>

#include "MEM_guardedalloc.h"

#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_linestyle.h"
#include "BKE_main.h"

static void default_linestyle_settings(FreestyleLineStyle *linestyle)
{

}

FreestyleLineStyle *FRS_new_linestyle(char *name, struct Main *main)
{
	FreestyleLineStyle *linestyle;

	if (!main)
		main = G.main;

	linestyle = (FreestyleLineStyle *)alloc_libblock(&main->linestyle, ID_LS, name);
	
	default_linestyle_settings(linestyle);

	return linestyle;
}

void FRS_free_linestyle(FreestyleLineStyle *linestyle)
{

}

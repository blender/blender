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
 * Contributor(s): Benoit Bolsee
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Ketsji/KX_PythonMain.cpp
 *  \ingroup ketsji
 */

#include "KX_PythonMain.h"

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "MEM_guardedalloc.h"

#include "BLI_string.h"
#include "BLI_listbase.h"

#include "BKE_text.h"
#include "BKE_main.h"
#include "BKE_idprop.h"


#ifdef __cplusplus
}
#endif

extern "C" char *KX_GetPythonMain(struct Scene *scene)
{
	/* examine custom scene properties */
	if (scene->id.properties) {
		IDProperty *item = IDP_GetPropertyTypeFromGroup(scene->id.properties, "__main__", IDP_STRING);
		if (item) {
			return BLI_strdup(IDP_String(item));
		}
	}

	return NULL;
}

extern "C" char *KX_GetPythonCode(Main *bmain, char *python_main)
{
	Text *text;

	if ((text = (Text *)BLI_findstring(&bmain->text, python_main, offsetof(ID, name) + 2))) {
		return txt_to_buf(text);
	}

	return NULL;
}

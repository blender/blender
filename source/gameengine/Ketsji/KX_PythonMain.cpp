/* 
 * $Id: KX_PythonMain.cpp 37750 2011-06-27 09:27:56Z sjoerd $
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
/** \file gameengine/Ketsji/KX_KetsjiPythonMain.cpp
 *  \ingroup ketsji
 */

#include "KX_PythonMain.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "RNA_access.h"
#include "MEM_guardedalloc.h"
#include "BKE_text.h"
#include "BKE_main.h"

#ifdef __cplusplus
}
#endif

extern "C" char *KX_GetPythonMain(struct Scene* scene)
{
    //examine custom scene properties

    PointerRNA sceneptr;
    RNA_id_pointer_create(&scene->id, &sceneptr);

    PropertyRNA *pymain = RNA_struct_find_property(&sceneptr, "[\"__main__\"]");
    if (pymain == NULL) return NULL;
    char *python_main;
    int len;
    python_main = RNA_property_string_get_alloc(&sceneptr, pymain, NULL, 0, &len);
    return python_main;
}

extern "C" char *KX_GetPythonCode(Main *main, char *python_main)
{
    PointerRNA mainptr, txtptr;
    PropertyRNA *texts;

    RNA_main_pointer_create(main, &mainptr);
    texts = RNA_struct_find_property(&mainptr, "texts");
    char *python_code = NULL;
    int ok = RNA_property_collection_lookup_string(&mainptr, texts, python_main, &txtptr);
    if (ok) {
        Text *text = (Text *) txtptr.data;
        python_code = txt_to_buf(text);
    }
    return python_code;
}


/* 
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * This is a new part of Blender.
 *
 * Contributor(s): Michel Selten
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include <Python.h>

#include <DNA_ID.h>
#include <DNA_listBase.h>

#define DBP_TYPE_NON 0          /* No item                                        */
#define DBP_TYPE_CHA 1          /* Char item                                      */
#define DBP_TYPE_SHO 2          /* Short item                                     */
#define DBP_TYPE_INT 3          /* Int item                                       */
#define DBP_TYPE_FLO 4          /* Float item                                     */
#define DBP_TYPE_VEC 5          /* Float vector object                            */
#define DBP_TYPE_FUN 6          /* funcPtrToObj hold function to convert ptr->ob  */
                                /* funcObjToPtr holds function to convert ob->ptr */

#define DBP_HANDLING_NONE	0	/* No special handling required                   */
#define DBP_HANDLING_FUNC	1	/* Extra1 is used to retrieve ptr                 */
#define DBP_HANDLING_NENM	2	/* Extra1 holds named enum to resolve             */
                                /* values from/to.                                */

typedef struct
{
	char                * public_name;
	char                * struct_name;
	int                   type;
	int                   stype;
	float                 min;         /* Minimum allowed value */
	float                 max;         /* Maximum allowed value */
	int                   idx[4];
	int                   dlist[4];
	int                   handling;
	void                * extra1;
	void                * funcPtrToObj;
	void                * funcObjToPtr;
} DataBlockProperty;

typedef struct
{
	PyObject_HEAD
	void                * data;
	char                * type;
	ListBase            * type_list;
	DataBlockProperty   * properties;
} DataBlock;

PyObject * DataBlockFromID (ID * data);


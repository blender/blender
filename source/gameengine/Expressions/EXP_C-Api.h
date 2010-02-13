/**
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __EXPRESSION_INCLUDE
#define __EXPRESSION_INCLUDE

#define EXP_DECLARE_HANDLE(name) typedef struct name##__ { int unused; } *name

EXP_DECLARE_HANDLE(EXP_ValueHandle);
EXP_DECLARE_HANDLE(EXP_ExpressionHandle);


#ifdef __cplusplus
extern "C" {
#endif

extern EXP_ValueHandle		EXP_CreateInt(int innie);
extern EXP_ValueHandle		EXP_CreateBool(int innie);
extern EXP_ValueHandle		EXP_CreateString(const char* str);
extern void					EXP_SetName(EXP_ValueHandle,const char* newname);

/* calculate expression from inputtext */
extern EXP_ValueHandle		EXP_ParseInput(const char* inputtext);
extern void					EXP_ReleaseValue(EXP_ValueHandle);
extern int					EXP_IsValid(EXP_ValueHandle);

/* assign property 'propval' to 'destinationval' */
extern void					EXP_SetProperty(EXP_ValueHandle propval,EXP_ValueHandle destinationval);

/* returns NULL if property doesn't exist */
extern EXP_ValueHandle		EXP_GetProperty(EXP_ValueHandle inval,const char* propname);

const char*					EXP_GetText(EXP_ValueHandle);

#ifdef __cplusplus
}
#endif

#endif //__EXPRESSION_INCLUDE


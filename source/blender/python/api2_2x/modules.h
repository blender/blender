/* 
 * $Id: modules.h 4803 2005-07-18 03:50:37Z ascotan $
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
 * Contributor(s): Michel Selten, Willian P. Germano, Nathan Letwory,
 *                 Stephen Swaney
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#ifndef EXPP_modules_h
#define EXPP_modules_h

/************************************************************
Certain bpy module files ( BPY_interface.c, Blender.c, and Object.c )
need to know about almost every other module.  This file is a
convenient way to include almost all the necessary declarations.

The #includes commented out below currently do not exist.
Their *_Init() method declarations are hacked in down below.
************************************************************/

#include <Python.h>

/*****************************************************************************/
/* Global variables                                                          */
/*****************************************************************************/

/****************************************************************************
Module Init functions for modules without a .h file.
BGL is a special case.  It still has data declarations in the .h file
and cannot be #included until it is cleaned up.
****************************************************************************/

PyObject *BGL_Init( void );

PyObject *Library_Init( void );
PyObject *Noise_Init( void );



#endif				/* EXPP_modules_h */

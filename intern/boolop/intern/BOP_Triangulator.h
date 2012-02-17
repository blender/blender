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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file boolop/intern/BOP_Triangulator.h
 *  \ingroup boolopintern
 */

 
#ifndef __BOP_TRIANGULATOR_H__
#define __BOP_TRIANGULATOR_H__

#include "BOP_MathUtils.h"
#include "BOP_Mesh.h"

void BOP_triangulateA(BOP_Mesh *mesh, BOP_Faces *faces, BOP_Face * face, BOP_Index v, unsigned int e);
void BOP_triangulateB(BOP_Mesh * mesh, BOP_Faces *faces, BOP_Face * face, BOP_Index v);
void BOP_triangulateC(BOP_Mesh * mesh, BOP_Faces *faces, BOP_Face * face, BOP_Index v1, BOP_Index v2);
void BOP_triangulateD(BOP_Mesh * mesh, BOP_Faces *faces, BOP_Face * face, BOP_Index v1, BOP_Index v2, unsigned int e);
void BOP_triangulateE(BOP_Mesh *mesh, BOP_Faces *faces, BOP_Face * face, BOP_Index v1, BOP_Index v2, unsigned int e1, unsigned int e2);
void BOP_triangulateF(BOP_Mesh *mesh, BOP_Faces *faces, BOP_Face * face, BOP_Index v1, BOP_Index v2, unsigned int e);

#endif

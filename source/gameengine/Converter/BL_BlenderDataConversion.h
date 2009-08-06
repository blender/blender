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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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
#ifndef __BLENDER_CONVERT
#define __BLENDER_CONVERT

#include "GEN_HashedPtr.h"
#include "STR_String.h"
#include "KX_Python.h"
#include "KX_PhysicsEngineEnums.h"

class RAS_MeshObject* BL_ConvertMesh(struct Mesh* mesh,struct Object* lightobj,class KX_Scene* scene, class KX_BlenderSceneConverter *converter);

void BL_ConvertBlenderObjects(struct Main* maggie,
							  class KX_Scene* kxscene,
							  class KX_KetsjiEngine* ketsjiEngine,
							  e_PhysicsEngine	physics_engine,
							  PyObject* pythondictionary,
							  class RAS_IRenderTools* rendertools,
							  class RAS_ICanvas* canvas, 
							  class KX_BlenderSceneConverter* sceneconverter, 
							  bool alwaysUseExpandFraming
							  );

#endif // __BLENDER_CONVERT


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

/** \file BL_BlenderDataConversion.h
 *  \ingroup bgeconv
 */

#ifndef __BL_BLENDERDATACONVERSION_H__
#define __BL_BLENDERDATACONVERSION_H__

#include "CTR_HashedPtr.h"
#include "STR_String.h"
#include "KX_Python.h"
#include "KX_PhysicsEngineEnums.h"
#include "SCA_IInputDevice.h"

class RAS_MeshObject* BL_ConvertMesh(struct Mesh* mesh,struct Object* lightobj,class KX_Scene* scene, class KX_BlenderSceneConverter *converter, bool libloading);

void BL_ConvertBlenderObjects(struct Main* maggie,
							  class KX_Scene* kxscene,
							  class KX_KetsjiEngine* ketsjiEngine,
							  e_PhysicsEngine	physics_engine,
							  class RAS_IRasterizer* rendertools,
							  class RAS_ICanvas* canvas, 
							  class KX_BlenderSceneConverter* sceneconverter, 
							  bool alwaysUseExpandFraming,
							  bool libloading=false
							  );

SCA_IInputDevice::KX_EnumInputs ConvertKeyCode(int key_code);

#endif  /* __BL_BLENDERDATACONVERSION_H__ */

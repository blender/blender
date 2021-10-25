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

/** \file KX_ISceneConverter.h
 *  \ingroup ketsji
 */

#ifndef __KX_ISCENECONVERTER_H__
#define __KX_ISCENECONVERTER_H__

#include "STR_String.h"
#include "EXP_Python.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

struct Scene;

class KX_ISceneConverter 
{

public:
	KX_ISceneConverter() {}
	virtual ~KX_ISceneConverter () {}

	/*
	 * scenename: name of the scene to be converted,
	 * if the scenename is empty, convert the 'default' scene (whatever this means)
	 * destinationscene: pass an empty scene, everything goes into this
	 * dictobj: python dictionary (for pythoncontrollers)
	 */
	virtual void ConvertScene(
		class KX_Scene* destinationscene,
		class RAS_IRasterizer* rendertools,
		class RAS_ICanvas*  canvas,
		bool libloading=false)=0;

	virtual void RemoveScene(class KX_Scene *scene)=0;

	// handle any pending merges from asynchronous loads
	virtual void MergeAsyncLoads()=0;
	virtual void FinalizeAsyncLoads() = 0;

	virtual void	SetAlwaysUseExpandFraming(bool to_what) = 0;

	virtual void	SetNewFileName(const STR_String& filename) = 0;
	virtual bool	TryAndLoadNewFile() = 0;

	virtual void	ResetPhysicsObjectsAnimationIpo(bool clearIpo) = 0;

	///this generates ipo curves for position, rotation, allowing to use game physics in animation
	virtual void	WritePhysicsObjectToAnimationIpo(int frameNumber) = 0;
	virtual void	TestHandlesPhysicsObjectToAnimationIpo() = 0;

	///this is for reseting the position,rotation and scale of the gameobjet that is not dynamic
	virtual void	resetNoneDynamicObjectToIpo()=0;

	// use blender materials
	virtual void SetMaterials(bool val) =0;
	virtual bool GetMaterials()=0;

	// use blender glsl materials
	virtual void SetGLSLMaterials(bool val) =0;
	virtual bool GetGLSLMaterials()=0;

	// cache materials during conversion
	virtual void SetCacheMaterials(bool val) =0;
	virtual bool GetCacheMaterials()=0;

	virtual struct Scene* GetBlenderSceneForName(const STR_String& name)=0;
	
	
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:KX_ISceneConverter")
#endif
};

#endif  /* __KX_ISCENECONVERTER_H__ */

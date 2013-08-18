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
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file BL_BlenderShader.h
 *  \ingroup ketsji
 */

#ifndef __BL_BLENDERSHADER_H__
#define __BL_BLENDERSHADER_H__

#include "GPU_material.h"

#include "MT_Matrix4x4.h"
#include "MT_Matrix3x3.h"
#include "MT_Tuple2.h"
#include "MT_Tuple3.h"
#include "MT_Tuple4.h"

#include "RAS_IPolygonMaterial.h"

#include "KX_Scene.h"

struct Material;
struct Scene;
class BL_Material;

#define BL_MAX_ATTRIB	16

/**
 * BL_BlenderShader
 *  Blender GPU shader material
 */
class BL_BlenderShader
{
private:
	struct Scene	*mBlenderScene;
	struct Material	*mMat;
	int				mLightLayer;
	int				mAlphaBlend;
	GPUMaterial     *mGPUMat;

	bool			VerifyShader() 
	{
		return (NULL != mGPUMat);
	}

public:
	BL_BlenderShader(KX_Scene *scene, struct Material *ma, int lightlayer);
	virtual ~BL_BlenderShader();

	bool				Ok()
	{
		// same as VerifyShared
		return (NULL != mGPUMat);
	}
	void				SetProg(bool enable, double time=0.0, RAS_IRasterizer* rasty=NULL);

	int GetAttribNum();
	void SetAttribs(class RAS_IRasterizer* ras, const BL_Material *mat);
	void Update(const class RAS_MeshSlot & ms, class RAS_IRasterizer* rasty);
	void ReloadMaterial();
	int GetAlphaBlend();

	bool Equals(BL_BlenderShader *blshader);
	
	
#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:BL_BlenderShader")
#endif
};

#endif /* __BL_BLENDERSHADER_H__ */

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

/** \file BL_ShapeDeformer.h
 *  \ingroup bgeconv
 */

#ifndef __BL_SHAPEDEFORMER_H__
#define __BL_SHAPEDEFORMER_H__

#ifdef _MSC_VER
#  pragma warning (disable:4786)  /* get rid of stupid stl-visual compiler debug warning */
#endif

#include "BL_SkinDeformer.h"
#include "BL_DeformableGameObject.h"
#include <vector>

class BL_ShapeDeformer : public BL_SkinDeformer  
{
public:
	BL_ShapeDeformer(BL_DeformableGameObject *gameobj,
	                 Object *bmeshobj,
	                 RAS_MeshObject *mesh);

	/* this second constructor is needed for making a mesh deformable on the fly. */
	BL_ShapeDeformer(BL_DeformableGameObject *gameobj,
					struct Object *bmeshobj_old,
					struct Object *bmeshobj_new,
					class RAS_MeshObject *mesh,
					bool release_object,
					bool recalc_normal,
					BL_ArmatureObject* arma = NULL);

	virtual RAS_Deformer *GetReplica();
	virtual void ProcessReplica();
	virtual ~BL_ShapeDeformer();

	bool Update (void);
	bool LoadShapeDrivers(KX_GameObject* parent);
	bool ExecuteShapeDrivers(void);

	struct Key *GetKey();

	void ForceUpdate()
	{
		m_lastShapeUpdate = -1.0;
	};

protected:
	bool			m_useShapeDrivers;
	double			m_lastShapeUpdate;
	struct Key*		m_key;


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:BL_ShapeDeformer")
#endif
};

#endif


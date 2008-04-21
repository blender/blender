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

#ifndef BL_DEFORMABLEGAMEOBJECT
#define BL_DEFORMABLEGAMEOBJECT

#ifdef WIN32
#pragma warning (disable:4786) // get rid of stupid stl-visual compiler debug warning
#endif //WIN32

#include "KX_GameObject.h"
#include "RAS_Deformer.h"

class BL_DeformableGameObject : public KX_GameObject  
{
public:

	RAS_Deformer		*m_pDeformer;	
	CValue*		GetReplica();
	virtual void Relink(GEN_Map<GEN_HashedPtr, void*>*map)
	{
		if (m_pDeformer)
			m_pDeformer->Relink (map);
	};
	void ProcessReplica(KX_GameObject* replica);

	BL_DeformableGameObject(void* sgReplicationInfo, SG_Callbacks callbacks) :
		KX_GameObject(sgReplicationInfo,callbacks),
		m_pDeformer(NULL)
	{
		m_isDeformable = true;
	};
	virtual ~BL_DeformableGameObject();

};

#endif


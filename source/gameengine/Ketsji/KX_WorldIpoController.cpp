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

/** \file gameengine/Ketsji/KX_WorldIpoController.cpp
 *  \ingroup ketsji
 */


#include "KX_WorldIpoController.h"
#include "KX_ScalarInterpolator.h"
#include "KX_WorldInfo.h"
#include "KX_PythonInit.h"
#include "KX_Scene.h"

#if defined(_WIN64)
typedef unsigned __int64 uint_ptr;
#else
typedef unsigned long uint_ptr;
#endif

bool KX_WorldIpoController::Update(double currentTime)
{
	if (m_modified) {
		T_InterpolatorList::iterator i;
		for (i = m_interpolators.begin(); !(i == m_interpolators.end()); ++i) {
			(*i)->Execute(m_ipotime);
		}

		KX_WorldInfo *world = KX_GetActiveScene()->GetWorldInfo();

		if (m_modify_mist_start) {
			world->setMistStart(m_mist_start);
		}

		if (m_modify_mist_dist) {
			world->setMistDistance(m_mist_dist);
		}

		if (m_modify_mist_intensity) {
			world->setMistIntensity(m_mist_intensity);
		}

		if (m_modify_horizon_color) {
			world->setBackColor(m_hori_rgb[0], m_hori_rgb[1], m_hori_rgb[2]);
			world->setMistColor(m_hori_rgb[0], m_hori_rgb[1], m_hori_rgb[2]);
		}

		if (m_modify_ambient_color) {
			world->setAmbientColor(m_ambi_rgb[0], m_ambi_rgb[1], m_ambi_rgb[2]);
		}

		m_modified = false;
	}
	return false;
}


void KX_WorldIpoController::AddInterpolator(KX_IInterpolator* interp)
{
	this->m_interpolators.push_back(interp);
}


SG_Controller*	KX_WorldIpoController::GetReplica(class SG_Node* destnode)
{
	KX_WorldIpoController* iporeplica = new KX_WorldIpoController(*this);
	// clear object that ipo acts on
	iporeplica->ClearObject();

	// dirty hack, ask Gino for a better solution in the ipo implementation
	// hacken en zagen, in what we call datahiding, not written for replication :(

	T_InterpolatorList oldlist = m_interpolators;
	iporeplica->m_interpolators.clear();

	T_InterpolatorList::iterator i;
	for (i = oldlist.begin(); !(i == oldlist.end()); ++i) {
		KX_ScalarInterpolator* copyipo = new KX_ScalarInterpolator(*((KX_ScalarInterpolator*)*i));
		iporeplica->AddInterpolator(copyipo);

		MT_Scalar* scaal = ((KX_ScalarInterpolator*)*i)->GetTarget();
		uint_ptr orgbase = (uint_ptr)this;
		uint_ptr orgloc = (uint_ptr)scaal;
		uint_ptr offset = orgloc-orgbase;
		uint_ptr newaddrbase = (uint_ptr)iporeplica + offset;
		MT_Scalar* blaptr = (MT_Scalar*) newaddrbase;
		copyipo->SetNewTarget((MT_Scalar*)blaptr);
	}
	
	return iporeplica;
}

KX_WorldIpoController::~KX_WorldIpoController()
{

	T_InterpolatorList::iterator i;
	for (i = m_interpolators.begin(); !(i == m_interpolators.end()); ++i) {
		delete (*i);
	}
	
}

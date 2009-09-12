/**
 * $Id$
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32

// don't show stl-warnings
#pragma warning (disable:4786)
#endif

#include "BKE_material.h" /* give_current_material */

#include "KX_GameObject.h"
#include "KX_IpoConvert.h"
#include "KX_IInterpolator.h"
#include "KX_ScalarInterpolator.h"

#include "KX_BlenderScalarInterpolator.h"
#include "KX_BlenderSceneConverter.h"


/* This little block needed for linking to Blender... */
#ifdef WIN32
#include "BLI_winstuff.h"
#endif

#include "DNA_object_types.h"
#include "DNA_ipo_types.h"
#include "DNA_lamp_types.h"
#include "DNA_world_types.h"
#include "DNA_camera_types.h"
#include "DNA_material_types.h"
/* end of blender include block */

#include "KX_IPO_SGController.h"
#include "KX_LightIpoSGController.h"
#include "KX_CameraIpoSGController.h"
#include "KX_WorldIpoController.h"
#include "KX_ObColorIpoSGController.h"
#include "KX_MaterialIpoController.h"

#include "SG_Node.h"

#include "STR_HashedString.h"

static BL_InterpolatorList *GetAdtList(struct AnimData *for_adt, KX_BlenderSceneConverter *converter) {
	BL_InterpolatorList *adtList= converter->FindInterpolatorList(for_adt);

	if (!adtList) {		
		adtList = new BL_InterpolatorList(for_adt);
		converter->RegisterInterpolatorList(adtList, for_adt);
	}
			
	return adtList;	
}

void BL_ConvertIpos(struct Object* blenderobject,KX_GameObject* gameobj,KX_BlenderSceneConverter *converter)
{
	if (blenderobject->adt) {

		KX_IpoSGController* ipocontr = new KX_IpoSGController();
		gameobj->GetSGNode()->AddSGController(ipocontr);
		ipocontr->SetObject(gameobj->GetSGNode());
		
		// For ipo_as_force, we need to know which SM object and Scene the
		// object associated with this ipo is in. Is this already known here?
		// I think not.... then it must be done later :(
//		ipocontr->SetSumoReference(gameobj->GetSumoScene(), 
//								   gameobj->GetSumoObject());

		ipocontr->SetGameObject(gameobj);

		ipocontr->GetIPOTransform().SetPosition(
			MT_Point3(
			blenderobject->loc[0]/*+blenderobject->dloc[0]*/,
			blenderobject->loc[1]/*+blenderobject->dloc[1]*/,
			blenderobject->loc[2]/*+blenderobject->dloc[2]*/
			)
		);
		ipocontr->GetIPOTransform().SetEulerAngles(
			MT_Vector3(
			blenderobject->rot[0],
			blenderobject->rot[1],
			blenderobject->rot[2]
			)
		);
		ipocontr->GetIPOTransform().SetScaling(
			MT_Vector3(
			blenderobject->size[0],
			blenderobject->size[1],
			blenderobject->size[2]
			)
		);

		BL_InterpolatorList *adtList= GetAdtList(blenderobject->adt, converter);
		
		// For each active channel in the adtList add an
		// interpolator to the game object.
		
		KX_IInterpolator *interpolator;
		KX_IScalarInterpolator *interp;
		
		for(int i=0; i<3; i++) {
			if ((interp = adtList->GetScalarInterpolator("location", i))) {
				interpolator= new KX_ScalarInterpolator(&(ipocontr->GetIPOTransform().GetPosition()[i]), interp);
				ipocontr->AddInterpolator(interpolator);
				ipocontr->SetIPOChannelActive(OB_LOC_X+i, true);
			}
		}
		for(int i=0; i<3; i++) {
			if ((interp = adtList->GetScalarInterpolator("delta_location", i))) {
				interpolator= new KX_ScalarInterpolator(&(ipocontr->GetIPOTransform().GetDeltaPosition()[i]), interp);
				ipocontr->AddInterpolator(interpolator);
				ipocontr->SetIPOChannelActive(OB_DLOC_X+i, true);
			}
		}
		for(int i=0; i<3; i++) {
			if ((interp = adtList->GetScalarInterpolator("rotation", i))) {
				interpolator= new KX_ScalarInterpolator(&(ipocontr->GetIPOTransform().GetEulerAngles()[i]), interp);
				ipocontr->AddInterpolator(interpolator);
				ipocontr->SetIPOChannelActive(OB_ROT_X+i, true);
			}
		}
		for(int i=0; i<3; i++) {
			if ((interp = adtList->GetScalarInterpolator("delta_rotation", i))) {
				interpolator= new KX_ScalarInterpolator(&(ipocontr->GetIPOTransform().GetDeltaEulerAngles()[i]), interp);
				ipocontr->AddInterpolator(interpolator);
				ipocontr->SetIPOChannelActive(OB_DROT_X+i, true);
			}
		}
		for(int i=0; i<3; i++) {
			if ((interp = adtList->GetScalarInterpolator("scale", i))) {
				interpolator= new KX_ScalarInterpolator(&(ipocontr->GetIPOTransform().GetScaling()[i]), interp);
				ipocontr->AddInterpolator(interpolator);
				ipocontr->SetIPOChannelActive(OB_SIZE_X+i, true);
			}
		}
		for(int i=0; i<3; i++) {
			if ((interp = adtList->GetScalarInterpolator("delta_scale", i))) {
				interpolator= new KX_ScalarInterpolator(&(ipocontr->GetIPOTransform().GetDeltaScaling()[i]), interp);
				ipocontr->AddInterpolator(interpolator);
				ipocontr->SetIPOChannelActive(OB_DSIZE_X+i, true);
			}
		}
		
		{
			KX_ObColorIpoSGController* ipocontr_obcol=NULL;
			
			for(int i=0; i<4; i++) {
				if ((interp = adtList->GetScalarInterpolator("color", i))) {
					if (!ipocontr_obcol) {
						ipocontr_obcol = new KX_ObColorIpoSGController();
						gameobj->GetSGNode()->AddSGController(ipocontr_obcol);
						ipocontr_obcol->SetObject(gameobj->GetSGNode());
					}
					interpolator= new KX_ScalarInterpolator(&ipocontr_obcol->m_rgba[i], interp);
					ipocontr_obcol->AddInterpolator(interpolator);
				}
			}
		}
	}
}

void BL_ConvertLampIpos(struct Lamp* blenderlamp, KX_GameObject *lightobj,KX_BlenderSceneConverter *converter)
{

	if (blenderlamp->adt) {

		KX_LightIpoSGController* ipocontr = new KX_LightIpoSGController();
		lightobj->GetSGNode()->AddSGController(ipocontr);
		ipocontr->SetObject(lightobj->GetSGNode());
		
		ipocontr->m_energy = blenderlamp->energy;
		ipocontr->m_col_rgb[0] = blenderlamp->r;
		ipocontr->m_col_rgb[1] = blenderlamp->g;
		ipocontr->m_col_rgb[2] = blenderlamp->b;
		ipocontr->m_dist = blenderlamp->dist;

		BL_InterpolatorList *adtList= GetAdtList(blenderlamp->adt, converter);

		// For each active channel in the adtList add an
		// interpolator to the game object.
		
		KX_IInterpolator *interpolator;
		KX_IScalarInterpolator *interp;
		
		if ((interp= adtList->GetScalarInterpolator("energy", 0))) {
			interpolator= new KX_ScalarInterpolator(&ipocontr->m_energy, interp);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetModifyEnergy(true);
		}

		if ((interp = adtList->GetScalarInterpolator("distance", 0))) {
			interpolator= new KX_ScalarInterpolator(&ipocontr->m_dist, interp);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetModifyDist(true);
		}
		
		for(int i=0; i<3; i++) {
			if ((interp = adtList->GetScalarInterpolator("color", i))) {
				interpolator= new KX_ScalarInterpolator(&ipocontr->m_col_rgb[i], interp);
				ipocontr->AddInterpolator(interpolator);
				ipocontr->SetModifyColor(true);
			}
		}
	}
}




void BL_ConvertCameraIpos(struct Camera* blendercamera, KX_GameObject *cameraobj,KX_BlenderSceneConverter *converter)
{

	if (blendercamera->adt) {

		KX_CameraIpoSGController* ipocontr = new KX_CameraIpoSGController();
		cameraobj->GetSGNode()->AddSGController(ipocontr);
		ipocontr->SetObject(cameraobj->GetSGNode());
		
		ipocontr->m_lens = blendercamera->lens;
		ipocontr->m_clipstart = blendercamera->clipsta;
		ipocontr->m_clipend = blendercamera->clipend;

		BL_InterpolatorList *adtList= GetAdtList(blendercamera->adt, converter);

		// For each active channel in the adtList add an
		// interpolator to the game object.
		
		KX_IInterpolator *interpolator;
		KX_IScalarInterpolator *interp;
		
		if ((interp = adtList->GetScalarInterpolator("lens", 0))) {
			interpolator= new KX_ScalarInterpolator(&ipocontr->m_lens, interp);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetModifyLens(true);
		}

		if ((interp = adtList->GetScalarInterpolator("clip_start", 0))) {
			interpolator= new KX_ScalarInterpolator(&ipocontr->m_clipstart, interp);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetModifyClipStart(true);
		}

		if ((interp = adtList->GetScalarInterpolator("clip_end", 0))) {
			interpolator= new KX_ScalarInterpolator(&ipocontr->m_clipend, interp);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetModifyClipEnd(true);
		}

	}
}


void BL_ConvertWorldIpos(struct World* blenderworld,KX_BlenderSceneConverter *converter)
{

	if (blenderworld->adt) {

		KX_WorldIpoController* ipocontr = new KX_WorldIpoController();

// Erwin, hook up the world ipo controller here
// Gino: hook it up to what ?
// is there a userinterface element for that ?
// for now, we have some new python hooks to access the data, for a work-around
		
		ipocontr->m_mist_start  = blenderworld->miststa;
		ipocontr->m_mist_dist   = blenderworld->mistdist;
		ipocontr->m_mist_rgb[0] = blenderworld->horr;
		ipocontr->m_mist_rgb[1] = blenderworld->horg;
		ipocontr->m_mist_rgb[2] = blenderworld->horb;

		BL_InterpolatorList *adtList= GetAdtList(blenderworld->adt, converter);

		// For each active channel in the adtList add an
		// interpolator to the game object.
		
		KX_IInterpolator *interpolator;
		KX_IScalarInterpolator *interp;
		
		for(int i=0; i<3; i++) {
			if ((interp = adtList->GetScalarInterpolator("horizon_color", i))) {
				interpolator= new KX_ScalarInterpolator(&ipocontr->m_mist_rgb[i], interp);
				ipocontr->AddInterpolator(interpolator);
				ipocontr->SetModifyMistColor(true);
			}
		}

		if ((interp = adtList->GetScalarInterpolator("mist.depth", 0))) {
			interpolator= new KX_ScalarInterpolator(&ipocontr->m_mist_dist, interp);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetModifyMistDist(true);
		}

		if ((interp = adtList->GetScalarInterpolator("mist.start", 0))) {
			interpolator= new KX_ScalarInterpolator(&ipocontr->m_mist_start, interp);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetModifyMistStart(true);
		}
	}
}

static void ConvertMaterialIpos(
	Material* blendermaterial,
	dword matname_hash,
	KX_GameObject* gameobj,  
	KX_BlenderSceneConverter *converter
	)
{
	if (blendermaterial->adt) {
		KX_MaterialIpoController* ipocontr = new KX_MaterialIpoController(matname_hash);
		gameobj->GetSGNode()->AddSGController(ipocontr);
		ipocontr->SetObject(gameobj->GetSGNode());
		
		BL_InterpolatorList *adtList= GetAdtList(blendermaterial->adt, converter);


		ipocontr->m_rgba[0]	= blendermaterial->r;
		ipocontr->m_rgba[1]	= blendermaterial->g;
		ipocontr->m_rgba[2]	= blendermaterial->b;
		ipocontr->m_rgba[3]	= blendermaterial->alpha;

		ipocontr->m_specrgb[0]	= blendermaterial->specr;
		ipocontr->m_specrgb[1]	= blendermaterial->specg;
		ipocontr->m_specrgb[2]	= blendermaterial->specb;
		
		ipocontr->m_hard		= blendermaterial->har;
		ipocontr->m_spec		= blendermaterial->spec;
		ipocontr->m_ref			= blendermaterial->ref;
		ipocontr->m_emit		= blendermaterial->emit;
		ipocontr->m_alpha		= blendermaterial->alpha;
		
		KX_IInterpolator *interpolator;
		KX_IScalarInterpolator *sinterp;
		
		// --
		for(int i=0; i<3; i++) {
			if ((sinterp = adtList->GetScalarInterpolator("diffuse_color", i))) {
				if (!ipocontr) {
					ipocontr = new KX_MaterialIpoController(matname_hash);
					gameobj->GetSGNode()->AddSGController(ipocontr);
					ipocontr->SetObject(gameobj->GetSGNode());
				}
				interpolator= new KX_ScalarInterpolator(&ipocontr->m_rgba[i], sinterp);
				ipocontr->AddInterpolator(interpolator);
			}
		}
		
		if ((sinterp = adtList->GetScalarInterpolator("alpha", 0))) {
			if (!ipocontr) {
				ipocontr = new KX_MaterialIpoController(matname_hash);
				gameobj->GetSGNode()->AddSGController(ipocontr);
				ipocontr->SetObject(gameobj->GetSGNode());
			}
			interpolator= new KX_ScalarInterpolator(&ipocontr->m_rgba[3], sinterp);
			ipocontr->AddInterpolator(interpolator);
		}

		for(int i=0; i<3; i++) {
			if ((sinterp = adtList->GetScalarInterpolator("specular_color", i))) {
				if (!ipocontr) {
					ipocontr = new KX_MaterialIpoController(matname_hash);
					gameobj->GetSGNode()->AddSGController(ipocontr);
					ipocontr->SetObject(gameobj->GetSGNode());
				}
				interpolator= new KX_ScalarInterpolator(&ipocontr->m_specrgb[i], sinterp);
				ipocontr->AddInterpolator(interpolator);
			}
		}
		
		if ((sinterp = adtList->GetScalarInterpolator("specular_hardness", 0))) {
			if (!ipocontr) {
				ipocontr = new KX_MaterialIpoController(matname_hash);
				gameobj->GetSGNode()->AddSGController(ipocontr);
				ipocontr->SetObject(gameobj->GetSGNode());
			}
			interpolator= new KX_ScalarInterpolator(&ipocontr->m_hard, sinterp);
			ipocontr->AddInterpolator(interpolator);
		}

		if ((sinterp = adtList->GetScalarInterpolator("specularity", 0))) {
			if (!ipocontr) {
				ipocontr = new KX_MaterialIpoController(matname_hash);
				gameobj->GetSGNode()->AddSGController(ipocontr);
				ipocontr->SetObject(gameobj->GetSGNode());
			}
			interpolator= new KX_ScalarInterpolator(&ipocontr->m_spec, sinterp);
			ipocontr->AddInterpolator(interpolator);
		}
		
		if ((sinterp = adtList->GetScalarInterpolator("diffuse_reflection", 0))) {
			if (!ipocontr) {
				ipocontr = new KX_MaterialIpoController(matname_hash);
				gameobj->GetSGNode()->AddSGController(ipocontr);
				ipocontr->SetObject(gameobj->GetSGNode());
			}
			interpolator= new KX_ScalarInterpolator(&ipocontr->m_ref, sinterp);
			ipocontr->AddInterpolator(interpolator);
		}	
		
		if ((sinterp = adtList->GetScalarInterpolator("emit", 0))) {
			if (!ipocontr) {
				ipocontr = new KX_MaterialIpoController(matname_hash);
				gameobj->GetSGNode()->AddSGController(ipocontr);
				ipocontr->SetObject(gameobj->GetSGNode());
			}
			interpolator= new KX_ScalarInterpolator(&ipocontr->m_emit, sinterp);
			ipocontr->AddInterpolator(interpolator);
		}
	}		
}

void BL_ConvertMaterialIpos(
	struct Object* blenderobject,
	KX_GameObject* gameobj,  
	KX_BlenderSceneConverter *converter
	)
{
	if (blenderobject->totcol==1)
	{
		Material *mat = give_current_material(blenderobject, 1);
		// if there is only one material attached to the mesh then set material_index in BL_ConvertMaterialIpos to NULL
		// --> this makes the UpdateMaterialData function in KX_GameObject.cpp use the old hack of using SetObjectColor
		// because this yields a better performance as not all the vertex colors need to be edited
		if(mat) ConvertMaterialIpos(mat, 0, gameobj, converter);
	}
	else
	{
		for (int material_index=1; material_index <= blenderobject->totcol; material_index++)
		{
			Material *mat = give_current_material(blenderobject, material_index);
			STR_HashedString matname;
			if(mat) {
				matname= mat->id.name;
				ConvertMaterialIpos(mat, matname.hash(), gameobj, converter);
			}
		}
	}
}


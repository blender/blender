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

static BL_InterpolatorList *GetIpoList(struct Ipo *for_ipo, KX_BlenderSceneConverter *converter) {
	BL_InterpolatorList *ipoList= converter->FindInterpolatorList(for_ipo);

	if (!ipoList) {		
		ipoList = new BL_InterpolatorList(for_ipo);
		converter->RegisterInterpolatorList(ipoList, for_ipo);
	}
			
	return ipoList;	
}

void BL_ConvertIpos(struct Object* blenderobject,KX_GameObject* gameobj,KX_BlenderSceneConverter *converter)
{
	if (blenderobject->ipo) {

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

		BL_InterpolatorList *ipoList= GetIpoList(blenderobject->ipo, converter);
		
		// For each active channel in the ipoList add an
		// interpolator to the game object.
		
		KX_IScalarInterpolator *ipo;
		
		ipo = ipoList->GetScalarInterpolator(OB_LOC_X);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(
					&(ipocontr->GetIPOTransform().GetPosition()[0]),
					ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetIPOChannelActive(OB_LOC_X, true);
	
		}
		
		ipo = ipoList->GetScalarInterpolator(OB_LOC_Y);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(
					&(ipocontr->GetIPOTransform().GetPosition()[1]),
					ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetIPOChannelActive(OB_LOC_Y, true);
		}
		
		ipo = ipoList->GetScalarInterpolator(OB_LOC_Z);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(
					&(ipocontr->GetIPOTransform().GetPosition()[2]),
					ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetIPOChannelActive(OB_LOC_Z, true);
		}
		
		// Master the art of cut & paste programming...
		
		ipo = ipoList->GetScalarInterpolator(OB_DLOC_X);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(
					&(ipocontr->GetIPOTransform().GetDeltaPosition()[0]),
					ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetIPOChannelActive(OB_DLOC_X, true);
		}
		
		ipo = ipoList->GetScalarInterpolator(OB_DLOC_Y);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(
					&(ipocontr->GetIPOTransform().GetDeltaPosition()[1]),
					ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetIPOChannelActive(OB_DLOC_Y, true);
		}
		
		ipo = ipoList->GetScalarInterpolator(OB_DLOC_Z);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(
					&(ipocontr->GetIPOTransform().GetDeltaPosition()[2]),
					ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetIPOChannelActive(OB_DLOC_Z, true);
		}
		
		// Explore the finesse of reuse and slight modification
		
		ipo = ipoList->GetScalarInterpolator(OB_ROT_X);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(
					&(ipocontr->GetIPOTransform().GetEulerAngles()[0]),
					ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetIPOChannelActive(OB_ROT_X, true);
		}
		ipo = ipoList->GetScalarInterpolator(OB_ROT_Y);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(
					&(ipocontr->GetIPOTransform().GetEulerAngles()[1]),
					ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetIPOChannelActive(OB_ROT_Y, true);
		}
		ipo = ipoList->GetScalarInterpolator(OB_ROT_Z);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(
					&(ipocontr->GetIPOTransform().GetEulerAngles()[2]),
					ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetIPOChannelActive(OB_ROT_Z, true);
		}

		// Hmmm, the need for a macro comes to mind... 
		
		ipo = ipoList->GetScalarInterpolator(OB_DROT_X);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(
					&(ipocontr->GetIPOTransform().GetDeltaEulerAngles()[0]),
					ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetIPOChannelActive(OB_DROT_X, true);
		}
		ipo = ipoList->GetScalarInterpolator(OB_DROT_Y);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(
					&(ipocontr->GetIPOTransform().GetDeltaEulerAngles()[1]),
					ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetIPOChannelActive(OB_DROT_Y, true);
		}
		ipo = ipoList->GetScalarInterpolator(OB_DROT_Z);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(
					&(ipocontr->GetIPOTransform().GetDeltaEulerAngles()[2]),
					ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetIPOChannelActive(OB_DROT_Z, true);
		}

		// Hang on, almost there... 
		
		ipo = ipoList->GetScalarInterpolator(OB_SIZE_X);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(
					&(ipocontr->GetIPOTransform().GetScaling()[0]),
					ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetIPOChannelActive(OB_SIZE_X, true);
		}
		ipo = ipoList->GetScalarInterpolator(OB_SIZE_Y);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(
					&(ipocontr->GetIPOTransform().GetScaling()[1]),
					ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetIPOChannelActive(OB_SIZE_Y, true);
		}
		ipo = ipoList->GetScalarInterpolator(OB_SIZE_Z);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(
					&(ipocontr->GetIPOTransform().GetScaling()[2]),
					ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetIPOChannelActive(OB_SIZE_Z, true);
		}

		// The last few... 
		
		ipo = ipoList->GetScalarInterpolator(OB_DSIZE_X);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(
					&(ipocontr->GetIPOTransform().GetDeltaScaling()[0]),
					ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetIPOChannelActive(OB_DSIZE_X, true);
		}
		ipo = ipoList->GetScalarInterpolator(OB_DSIZE_Y);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(
					&(ipocontr->GetIPOTransform().GetDeltaScaling()[1]),
					ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetIPOChannelActive(OB_DSIZE_Y, true);
		}
		ipo = ipoList->GetScalarInterpolator(OB_DSIZE_Z);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(
					&(ipocontr->GetIPOTransform().GetDeltaScaling()[2]),
					ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetIPOChannelActive(OB_DSIZE_Z, true);
		}
		
		{
			KX_ObColorIpoSGController* ipocontr=NULL;

			ipo = ipoList->GetScalarInterpolator(OB_COL_R);
			if (ipo)
			{
				if (!ipocontr)
				{
					ipocontr = new KX_ObColorIpoSGController();
					gameobj->GetSGNode()->AddSGController(ipocontr);
					ipocontr->SetObject(gameobj->GetSGNode());
				}
				KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(
					&ipocontr->m_rgba[0],
					ipo);
				ipocontr->AddInterpolator(interpolator);
			}
			ipo = ipoList->GetScalarInterpolator(OB_COL_G);
			if (ipo)
			{
				if (!ipocontr)
				{
					ipocontr = new KX_ObColorIpoSGController();
					gameobj->GetSGNode()->AddSGController(ipocontr);
					ipocontr->SetObject(gameobj->GetSGNode());
				}
				KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(
					&ipocontr->m_rgba[1],
					ipo);
				ipocontr->AddInterpolator(interpolator);
			}
			ipo = ipoList->GetScalarInterpolator(OB_COL_B);
			if (ipo)
			{
				if (!ipocontr)
				{
					ipocontr = new KX_ObColorIpoSGController();
					gameobj->GetSGNode()->AddSGController(ipocontr);
					ipocontr->SetObject(gameobj->GetSGNode());
				}
				KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(
					&ipocontr->m_rgba[2],
					ipo);
				ipocontr->AddInterpolator(interpolator);
			}
			ipo = ipoList->GetScalarInterpolator(OB_COL_A);
			if (ipo)
			{
				if (!ipocontr)
				{
					ipocontr = new KX_ObColorIpoSGController();
					gameobj->GetSGNode()->AddSGController(ipocontr);
					ipocontr->SetObject(gameobj->GetSGNode());
				}
				KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(
					&ipocontr->m_rgba[3],
					ipo);
				ipocontr->AddInterpolator(interpolator);
			}
		}

		
	}


}

void BL_ConvertLampIpos(struct Lamp* blenderlamp, KX_GameObject *lightobj,KX_BlenderSceneConverter *converter)
{

	if (blenderlamp->ipo) {

		KX_LightIpoSGController* ipocontr = new KX_LightIpoSGController();
		lightobj->GetSGNode()->AddSGController(ipocontr);
		ipocontr->SetObject(lightobj->GetSGNode());
		
		ipocontr->m_energy = blenderlamp->energy;
		ipocontr->m_col_rgb[0] = blenderlamp->r;
		ipocontr->m_col_rgb[1] = blenderlamp->g;
		ipocontr->m_col_rgb[2] = blenderlamp->b;
		ipocontr->m_dist = blenderlamp->dist;

		BL_InterpolatorList *ipoList= GetIpoList(blenderlamp->ipo, converter);

		// For each active channel in the ipoList add an
		// interpolator to the game object.
		
		KX_IScalarInterpolator *ipo;
		
		ipo = ipoList->GetScalarInterpolator(LA_ENERGY);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(&ipocontr->m_energy, ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetModifyEnergy(true);
		}

		ipo = ipoList->GetScalarInterpolator(LA_DIST);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(&ipocontr->m_dist, ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetModifyDist(true);
		}

		ipo = ipoList->GetScalarInterpolator(LA_COL_R);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(&ipocontr->m_col_rgb[0], ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetModifyColor(true);
		}

		ipo = ipoList->GetScalarInterpolator(LA_COL_G);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(&ipocontr->m_col_rgb[1], ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetModifyColor(true);
		}

		ipo = ipoList->GetScalarInterpolator(LA_COL_B);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(&ipocontr->m_col_rgb[2], ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetModifyColor(true);
		}
	}
}




void BL_ConvertCameraIpos(struct Camera* blendercamera, KX_GameObject *cameraobj,KX_BlenderSceneConverter *converter)
{

	if (blendercamera->ipo) {

		KX_CameraIpoSGController* ipocontr = new KX_CameraIpoSGController();
		cameraobj->GetSGNode()->AddSGController(ipocontr);
		ipocontr->SetObject(cameraobj->GetSGNode());
		
		ipocontr->m_lens = blendercamera->lens;
		ipocontr->m_clipstart = blendercamera->clipsta;
		ipocontr->m_clipend = blendercamera->clipend;

		BL_InterpolatorList *ipoList= GetIpoList(blendercamera->ipo, converter);

		// For each active channel in the ipoList add an
		// interpolator to the game object.
		
		KX_IScalarInterpolator *ipo;
		
		ipo = ipoList->GetScalarInterpolator(CAM_LENS);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(&ipocontr->m_lens, ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetModifyLens(true);
		}

		ipo = ipoList->GetScalarInterpolator(CAM_STA);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(&ipocontr->m_clipstart, ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetModifyClipStart(true);
		}

		ipo = ipoList->GetScalarInterpolator(CAM_END);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(&ipocontr->m_clipend, ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetModifyClipEnd(true);
		}

	}
}


void BL_ConvertWorldIpos(struct World* blenderworld,KX_BlenderSceneConverter *converter)
{

	if (blenderworld->ipo) {

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

		BL_InterpolatorList *ipoList= GetIpoList(blenderworld->ipo, converter);

		// For each active channel in the ipoList add an
		// interpolator to the game object.
		
		KX_IScalarInterpolator *ipo;
		
		ipo = ipoList->GetScalarInterpolator(WO_HOR_R);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(&ipocontr->m_mist_rgb[0], ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetModifyMistColor(true);
		}

		ipo = ipoList->GetScalarInterpolator(WO_HOR_G);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(&ipocontr->m_mist_rgb[1], ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetModifyMistColor(true);
		}

		ipo = ipoList->GetScalarInterpolator(WO_HOR_B);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(&ipocontr->m_mist_rgb[2], ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetModifyMistColor(true);
		}

		ipo = ipoList->GetScalarInterpolator(WO_MISTDI);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(&ipocontr->m_mist_dist, ipo);
			ipocontr->AddInterpolator(interpolator);
			ipocontr->SetModifyMistDist(true);
		}

		ipo = ipoList->GetScalarInterpolator(WO_MISTSTA);
		if (ipo) {
			KX_IInterpolator *interpolator =
				new KX_ScalarInterpolator(&ipocontr->m_mist_start, ipo);
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
	if (blendermaterial->ipo) {
		KX_MaterialIpoController* ipocontr = new KX_MaterialIpoController(matname_hash);
		gameobj->GetSGNode()->AddSGController(ipocontr);
		ipocontr->SetObject(gameobj->GetSGNode());
		
		BL_InterpolatorList *ipoList= GetIpoList(blendermaterial->ipo, converter);


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
		KX_IScalarInterpolator *ipo;
		
		// --
		ipo = ipoList->GetScalarInterpolator(MA_COL_R);
		if (ipo) {
			if (!ipocontr) {
				ipocontr = new KX_MaterialIpoController(matname_hash);
				gameobj->GetSGNode()->AddSGController(ipocontr);
				ipocontr->SetObject(gameobj->GetSGNode());
			}
			KX_IInterpolator *interpolator =
			new KX_ScalarInterpolator(
				&ipocontr->m_rgba[0],
				ipo);
			ipocontr->AddInterpolator(interpolator);
		}
		
		ipo = ipoList->GetScalarInterpolator(MA_COL_G);
		if (ipo) {
			if (!ipocontr) {
				ipocontr = new KX_MaterialIpoController(matname_hash);
				gameobj->GetSGNode()->AddSGController(ipocontr);
				ipocontr->SetObject(gameobj->GetSGNode());
			}
			KX_IInterpolator *interpolator =
			new KX_ScalarInterpolator(
				&ipocontr->m_rgba[1],
				ipo);
			ipocontr->AddInterpolator(interpolator);
		}
		
		ipo = ipoList->GetScalarInterpolator(MA_COL_B);
		if (ipo) {
			if (!ipocontr) {
				ipocontr = new KX_MaterialIpoController(matname_hash);
				gameobj->GetSGNode()->AddSGController(ipocontr);
				ipocontr->SetObject(gameobj->GetSGNode());
			}
			KX_IInterpolator *interpolator =
			new KX_ScalarInterpolator(
				&ipocontr->m_rgba[2],
				ipo);
			ipocontr->AddInterpolator(interpolator);
		}
		
		ipo = ipoList->GetScalarInterpolator(MA_ALPHA);
		if (ipo) {
			if (!ipocontr) {
				ipocontr = new KX_MaterialIpoController(matname_hash);
				gameobj->GetSGNode()->AddSGController(ipocontr);
				ipocontr->SetObject(gameobj->GetSGNode());
			}
			KX_IInterpolator *interpolator =
			new KX_ScalarInterpolator(
				&ipocontr->m_rgba[3],
				ipo);
			ipocontr->AddInterpolator(interpolator);
		}
		// --

		ipo = ipoList->GetScalarInterpolator(MA_SPEC_R );
		if (ipo) {
			if (!ipocontr) {
				ipocontr = new KX_MaterialIpoController(matname_hash);
				gameobj->GetSGNode()->AddSGController(ipocontr);
				ipocontr->SetObject(gameobj->GetSGNode());
			}
			KX_IInterpolator *interpolator =
			new KX_ScalarInterpolator(
				&ipocontr->m_specrgb[0],
				ipo);
			ipocontr->AddInterpolator(interpolator);
		}
		
		ipo = ipoList->GetScalarInterpolator(MA_SPEC_G);
		if (ipo) {
			if (!ipocontr) {
				ipocontr = new KX_MaterialIpoController(matname_hash);
				gameobj->GetSGNode()->AddSGController(ipocontr);
				ipocontr->SetObject(gameobj->GetSGNode());
			}
			KX_IInterpolator *interpolator =
			new KX_ScalarInterpolator(
				&ipocontr->m_specrgb[1],
				ipo);
			ipocontr->AddInterpolator(interpolator);
		}
		
		ipo = ipoList->GetScalarInterpolator(MA_SPEC_B);
		if (ipo) {
			if (!ipocontr) {
				ipocontr = new KX_MaterialIpoController(matname_hash);
				gameobj->GetSGNode()->AddSGController(ipocontr);
				ipocontr->SetObject(gameobj->GetSGNode());
			}
			KX_IInterpolator *interpolator =
			new KX_ScalarInterpolator(
				&ipocontr->m_specrgb[2],
				ipo);
			ipocontr->AddInterpolator(interpolator);
		}
			
		// --
		ipo = ipoList->GetScalarInterpolator(MA_HARD);
		if (ipo) {
			if (!ipocontr) {
				ipocontr = new KX_MaterialIpoController(matname_hash);
				gameobj->GetSGNode()->AddSGController(ipocontr);
				ipocontr->SetObject(gameobj->GetSGNode());
			}
			KX_IInterpolator *interpolator =
			new KX_ScalarInterpolator(
				&ipocontr->m_hard,
				ipo);
			ipocontr->AddInterpolator(interpolator);
		}

		ipo = ipoList->GetScalarInterpolator(MA_SPEC);
		if (ipo) {
			if (!ipocontr) {
				ipocontr = new KX_MaterialIpoController(matname_hash);
				gameobj->GetSGNode()->AddSGController(ipocontr);
				ipocontr->SetObject(gameobj->GetSGNode());
			}
			KX_IInterpolator *interpolator =
			new KX_ScalarInterpolator(
				&ipocontr->m_spec,
				ipo);
			ipocontr->AddInterpolator(interpolator);
		}
		
		
		ipo = ipoList->GetScalarInterpolator(MA_REF);
		if (ipo) {
			if (!ipocontr) {
				ipocontr = new KX_MaterialIpoController(matname_hash);
				gameobj->GetSGNode()->AddSGController(ipocontr);
				ipocontr->SetObject(gameobj->GetSGNode());
			}
			KX_IInterpolator *interpolator =
			new KX_ScalarInterpolator(
				&ipocontr->m_ref,
				ipo);
			ipocontr->AddInterpolator(interpolator);
		}	
		
		ipo = ipoList->GetScalarInterpolator(MA_EMIT);
		if (ipo) {
			if (!ipocontr) {
				ipocontr = new KX_MaterialIpoController(matname_hash);
				gameobj->GetSGNode()->AddSGController(ipocontr);
				ipocontr->SetObject(gameobj->GetSGNode());
			}
			KX_IInterpolator *interpolator =
			new KX_ScalarInterpolator(
				&ipocontr->m_emit,
				ipo);
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
		if(mat) ConvertMaterialIpos(mat, NULL, gameobj, converter);
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


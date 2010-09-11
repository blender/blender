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

#ifndef NAN_INCLUDED_LOD_DecimationClass_h
#define NAN_INCLUDED_LOD_DecimationClass_h

#include "MEM_SmartPtr.h"
#include "MEM_NonCopyable.h"

#include "LOD_ManMesh2.h"
#include "LOD_QSDecimator.h"
#include "LOD_ExternNormalEditor.h"
#include "../extern/LOD_decimation.h"
#include "LOD_ExternBufferEditor.h"


class LOD_DecimationClass : public MEM_NonCopyable
{
public :

	enum {
		e_not_loaded,
		e_loaded,
		e_preprocessed
	} m_e_decimation_state;


	static
		LOD_DecimationClass *
	New(
		LOD_Decimation_InfoPtr extern_info
	) {
		// create everything 
	
		MEM_SmartPtr<LOD_DecimationClass> output(new LOD_DecimationClass());
		MEM_SmartPtr<LOD_ManMesh2> mesh(LOD_ManMesh2::New());
		MEM_SmartPtr<LOD_ExternBufferEditor> extern_editor(LOD_ExternBufferEditor::New(extern_info));

		if (mesh == NULL || extern_editor == NULL) return NULL;
		MEM_SmartPtr<LOD_ExternNormalEditor> normals(LOD_ExternNormalEditor::New(extern_info,mesh.Ref()));

		if (normals == NULL) return NULL;
		MEM_SmartPtr<LOD_QSDecimator> decimator(LOD_QSDecimator::New(
			mesh.Ref(),
			normals.Ref(),
			extern_editor.Ref()
		));		
		if (decimator == NULL || output == NULL) return NULL;

		output->m_mesh = mesh.Release();
		output->m_decimator = decimator.Release();
		output->m_normals = normals.Release();
		output->m_extern_editor = extern_editor.Release();

		return output.Release();
	}

		LOD_ManMesh2 &
	Mesh(
	){
		return m_mesh.Ref();
	}

		LOD_QSDecimator &
	Decimator(
	) {
		return m_decimator.Ref();
	}

		LOD_ExternNormalEditor &
	FaceEditor(
	){
		return m_normals.Ref();
	}
			
private :

	LOD_DecimationClass(
	) : m_e_decimation_state(e_not_loaded) {
	};

	MEM_SmartPtr<LOD_ManMesh2> m_mesh;
	MEM_SmartPtr<LOD_QSDecimator> m_decimator;
	MEM_SmartPtr<LOD_ExternNormalEditor> m_normals;
	MEM_SmartPtr<LOD_ExternBufferEditor> m_extern_editor;
};

#endif


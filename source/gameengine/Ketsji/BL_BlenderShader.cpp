
#include "DNA_customdata_types.h"

#include "BL_BlenderShader.h"

#if 0
#include "GPU_extensions.h"
#include "GPU_material.h"
#endif

#include "RAS_MeshObject.h"
#include "RAS_IRasterizer.h"

const bool BL_BlenderShader::Ok()const
{
#if 0
	return (mGPUMat != 0);
#endif

	return false;
}

BL_BlenderShader::BL_BlenderShader(struct Material *ma)
:
#if 0
	mGPUMat(0),
#endif
	mBound(false)
{
#if 0
	if(ma)
		mGPUMat = GPU_material_from_blender(ma, GPU_PROFILE_DERIVEDMESH);
#endif
}

BL_BlenderShader::~BL_BlenderShader()
{
#if 0
	if(mGPUMat) {
		GPU_material_unbind(mGPUMat);
		mGPUMat = 0;
	}
#endif
}

void BL_BlenderShader::ApplyShader()
{
}

void BL_BlenderShader::SetProg(bool enable)
{
#if 0
	if(mGPUMat) {
		if(enable) {
			GPU_material_bind(mGPUMat);
			mBound = true;
		}
		else {
			GPU_material_unbind(mGPUMat);
			mBound = false;
		}
	}
#endif
}

int BL_BlenderShader::GetAttribNum()
{
#if 0
	GPUVertexAttribs attribs;
	int i, enabled = 0;

	if(!mGPUMat)
		return enabled;

	GPU_material_vertex_attributes(mGPUMat, &attribs);

    for(i = 0; i < attribs.totlayer; i++)
		if(attribs.layer[i].glindex+1 > enabled)
			enabled= attribs.layer[i].glindex+1;
	
	if(enabled > BL_MAX_ATTRIB)
		enabled = BL_MAX_ATTRIB;

	return enabled;
#endif

	return 0;
}

void BL_BlenderShader::SetTexCoords(RAS_IRasterizer* ras)
{
#if 0
	GPUVertexAttribs attribs;
	int i, attrib_num;

	if(!mGPUMat)
		return;

	if(ras->GetDrawingMode() == RAS_IRasterizer::KX_TEXTURED) {
		GPU_material_vertex_attributes(mGPUMat, &attribs);
		attrib_num = GetAttribNum();

		ras->SetTexCoordNum(0);
		ras->SetAttribNum(attrib_num);
		for(i=0; i<attrib_num; i++)
			ras->SetAttrib(RAS_IRasterizer::RAS_TEXCO_DISABLE, i);

		for(i = 0; i < attribs.totlayer; i++) {
			if(attribs.layer[i].glindex > attrib_num)
				continue;

			if(attribs.layer[i].type == CD_MTFACE)
				ras->SetAttrib(RAS_IRasterizer::RAS_TEXCO_UV1, attribs.layer[i].glindex);
			else if(attribs.layer[i].type == CD_TANGENT)
				ras->SetAttrib(RAS_IRasterizer::RAS_TEXTANGENT, attribs.layer[i].glindex);
			else if(attribs.layer[i].type == CD_ORCO)
				ras->SetAttrib(RAS_IRasterizer::RAS_TEXCO_ORCO, attribs.layer[i].glindex);
			else if(attribs.layer[i].type == CD_NORMAL)
				ras->SetAttrib(RAS_IRasterizer::RAS_TEXCO_NORM, attribs.layer[i].glindex);
		}

		ras->EnableTextures(true);
	}
	else
		ras->EnableTextures(false);
#endif
}

void BL_BlenderShader::Update( const KX_MeshSlot & ms, RAS_IRasterizer* rasty )
{
#if 0
	float obmat[4][4], viewmat[4][4];

	if(!mGPUMat || !mBound)
		return;

	MT_Matrix4x4 model;
	model.setValue(ms.m_OpenGLMatrix);
	MT_Matrix4x4 view;
	rasty->GetViewMatrix(view);

	model.getValue((float*)obmat);
	view.getValue((float*)viewmat);

	GPU_material_bind_uniforms(mGPUMat, obmat, viewmat);
#endif
}

// eof

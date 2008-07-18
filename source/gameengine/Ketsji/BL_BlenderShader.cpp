
#include "DNA_customdata_types.h"
#include "DNA_material_types.h"

#include "BL_BlenderShader.h"
#include "BL_Material.h"

#ifdef BLENDER_GLSL
#include "GPU_extensions.h"
#include "GPU_material.h"
#endif

#include "RAS_MeshObject.h"
#include "RAS_IRasterizer.h"

const bool BL_BlenderShader::Ok()const
{
#ifdef BLENDER_GLSL
	return (mGPUMat != 0);
#else
	return 0;
#endif
}

BL_BlenderShader::BL_BlenderShader(struct Material *ma, int lightlayer)
:
#ifdef BLENDER_GLSL
	mGPUMat(0),
#endif
	mBound(false),
	mLightLayer(lightlayer)
{
#ifdef BLENDER_GLSL
	if(ma) {
		GPU_material_from_blender(ma);
		mGPUMat = ma->gpumaterial;
	}
#endif
}

BL_BlenderShader::~BL_BlenderShader()
{
#ifdef BLENDER_GLSL
	if(mGPUMat) {
		GPU_material_unbind(mGPUMat);
		mGPUMat = 0;
	}
#endif
}

void BL_BlenderShader::SetProg(bool enable)
{
#ifdef BLENDER_GLSL
	if(mGPUMat) {
		if(enable) {
			GPU_material_bind(mGPUMat, mLightLayer);
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
#ifdef BLENDER_GLSL
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
#else
	return 0;
#endif
}

void BL_BlenderShader::SetAttribs(RAS_IRasterizer* ras, const BL_Material *mat)
{
#ifdef BLENDER_GLSL
	GPUVertexAttribs attribs;
	int i, attrib_num;

	ras->SetAttribNum(0);

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

			if(attribs.layer[i].type == CD_MTFACE) {
				if(!mat->uvName.IsEmpty() && strcmp(mat->uvName.ReadPtr(), attribs.layer[i].name) == 0)
					ras->SetAttrib(RAS_IRasterizer::RAS_TEXCO_UV1, attribs.layer[i].glindex);
				else if(!mat->uv2Name.IsEmpty() && strcmp(mat->uv2Name.ReadPtr(), attribs.layer[i].name) == 0)
					ras->SetAttrib(RAS_IRasterizer::RAS_TEXCO_UV2, attribs.layer[i].glindex);
				else
					ras->SetAttrib(RAS_IRasterizer::RAS_TEXCO_UV1, attribs.layer[i].glindex);
			}
			else if(attribs.layer[i].type == CD_TANGENT)
				ras->SetAttrib(RAS_IRasterizer::RAS_TEXTANGENT, attribs.layer[i].glindex);
			else if(attribs.layer[i].type == CD_ORCO)
				ras->SetAttrib(RAS_IRasterizer::RAS_TEXCO_ORCO, attribs.layer[i].glindex);
			else if(attribs.layer[i].type == CD_NORMAL)
				ras->SetAttrib(RAS_IRasterizer::RAS_TEXCO_NORM, attribs.layer[i].glindex);
			else if(attribs.layer[i].type == CD_MCOL)
				ras->SetAttrib(RAS_IRasterizer::RAS_TEXCO_VCOL, attribs.layer[i].glindex);
			else
				ras->SetAttrib(RAS_IRasterizer::RAS_TEXCO_DISABLE, attribs.layer[i].glindex);
		}

		ras->EnableTextures(true);
	}
	else
		ras->EnableTextures(false);
#endif
}

void BL_BlenderShader::Update( const KX_MeshSlot & ms, RAS_IRasterizer* rasty )
{
#ifdef BLENDER_GLSL
	float obmat[4][4], viewmat[4][4], viewinvmat[4][4];

	if(!mGPUMat || !mBound)
		return;

	MT_Matrix4x4 model;
	model.setValue(ms.m_OpenGLMatrix);
	MT_Matrix4x4 view;
	rasty->GetViewMatrix(view);

	model.getValue((float*)obmat);
	view.getValue((float*)viewmat);

	view.invert();
	view.getValue((float*)viewinvmat);

	GPU_material_bind_uniforms(mGPUMat, obmat, viewmat, viewinvmat);
#endif
}

bool BL_BlenderShader::Equals(BL_BlenderShader *blshader)
{
#ifdef BLENDER_GLSL
	/* to avoid unneeded state switches */
	return (blshader && mGPUMat == blshader->mGPUMat && mLightLayer == blshader->mLightLayer);
#else
	return true;
#endif
}

// eof

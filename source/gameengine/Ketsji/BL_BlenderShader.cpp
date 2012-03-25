/** \file gameengine/Ketsji/BL_BlenderShader.cpp
 *  \ingroup ketsji
 */

#include "DNA_customdata_types.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"

#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_DerivedMesh.h"

#include "BL_BlenderShader.h"
#include "BL_Material.h"

#include "GPU_extensions.h"
#include "GPU_material.h"

#include "RAS_BucketManager.h"
#include "RAS_MeshObject.h"
#include "RAS_IRasterizer.h"
 
BL_BlenderShader::BL_BlenderShader(KX_Scene *scene, struct Material *ma, int lightlayer)
:
	mScene(scene),
	mMat(ma),
	mLightLayer(lightlayer),
	mGPUMat(NULL)
{
	mBlenderScene = scene->GetBlenderScene();
	mAlphaBlend = GPU_BLEND_SOLID;

	ReloadMaterial();
}

BL_BlenderShader::~BL_BlenderShader()
{
	if (mGPUMat)
		GPU_material_unbind(mGPUMat);
}

void BL_BlenderShader::ReloadMaterial()
{
	mGPUMat = (mMat) ? GPU_material_from_blender(mBlenderScene, mMat) : NULL;
}

void BL_BlenderShader::SetProg(bool enable, double time)
{
	if (VerifyShader()) {
		if (enable)
			GPU_material_bind(mGPUMat, mLightLayer, mBlenderScene->lay, time, 1);
		else
			GPU_material_unbind(mGPUMat);
	}
}

int BL_BlenderShader::GetAttribNum()
{
	GPUVertexAttribs attribs;
	int i, enabled = 0;

	if (!VerifyShader())
		return enabled;

	GPU_material_vertex_attributes(mGPUMat, &attribs);

	for (i = 0; i < attribs.totlayer; i++)
		if (attribs.layer[i].glindex+1 > enabled)
			enabled= attribs.layer[i].glindex+1;
	
	if (enabled > BL_MAX_ATTRIB)
		enabled = BL_MAX_ATTRIB;

	return enabled;
}

void BL_BlenderShader::SetAttribs(RAS_IRasterizer* ras, const BL_Material *mat)
{
	GPUVertexAttribs attribs;
	GPUMaterial *gpumat;
	int i, attrib_num;

	ras->SetAttribNum(0);

	if (!VerifyShader())
		return;
	
	gpumat = mGPUMat;

	if (ras->GetDrawingMode() == RAS_IRasterizer::KX_TEXTURED) {
		GPU_material_vertex_attributes(gpumat, &attribs);
		attrib_num = GetAttribNum();

		ras->SetTexCoordNum(0);
		ras->SetAttribNum(attrib_num);
		for (i=0; i<attrib_num; i++)
			ras->SetAttrib(RAS_IRasterizer::RAS_TEXCO_DISABLE, i);

		for (i = 0; i < attribs.totlayer; i++) {
			if (attribs.layer[i].glindex > attrib_num)
				continue;

			if (attribs.layer[i].type == CD_MTFACE) {
				if (!mat->uvName.IsEmpty() && strcmp(mat->uvName.ReadPtr(), attribs.layer[i].name) == 0)
					ras->SetAttrib(RAS_IRasterizer::RAS_TEXCO_UV1, attribs.layer[i].glindex);
				else if (!mat->uv2Name.IsEmpty() && strcmp(mat->uv2Name.ReadPtr(), attribs.layer[i].name) == 0)
					ras->SetAttrib(RAS_IRasterizer::RAS_TEXCO_UV2, attribs.layer[i].glindex);
				else
					ras->SetAttrib(RAS_IRasterizer::RAS_TEXCO_UV1, attribs.layer[i].glindex);
			}
			else if (attribs.layer[i].type == CD_TANGENT)
				ras->SetAttrib(RAS_IRasterizer::RAS_TEXTANGENT, attribs.layer[i].glindex);
			else if (attribs.layer[i].type == CD_ORCO)
				ras->SetAttrib(RAS_IRasterizer::RAS_TEXCO_ORCO, attribs.layer[i].glindex);
			else if (attribs.layer[i].type == CD_NORMAL)
				ras->SetAttrib(RAS_IRasterizer::RAS_TEXCO_NORM, attribs.layer[i].glindex);
			else if (attribs.layer[i].type == CD_MCOL)
				ras->SetAttrib(RAS_IRasterizer::RAS_TEXCO_VCOL, attribs.layer[i].glindex);
			else
				ras->SetAttrib(RAS_IRasterizer::RAS_TEXCO_DISABLE, attribs.layer[i].glindex);
		}
	}
}

void BL_BlenderShader::Update(const RAS_MeshSlot & ms, RAS_IRasterizer* rasty )
{
	float obmat[4][4], viewmat[4][4], viewinvmat[4][4], obcol[4];
	GPUMaterial *gpumat;

	gpumat = mGPUMat;

	if (!gpumat || !GPU_material_bound(gpumat))
		return;

	MT_Matrix4x4 model;
	model.setValue(ms.m_OpenGLMatrix);
	const MT_Matrix4x4& view = rasty->GetViewMatrix();
	const MT_Matrix4x4& viewinv = rasty->GetViewInvMatrix();

	// note: getValue gives back column major as needed by OpenGL
	model.getValue((float*)obmat);
	view.getValue((float*)viewmat);
	viewinv.getValue((float*)viewinvmat);

	if (ms.m_bObjectColor)
		ms.m_RGBAcolor.getValue((float*)obcol);
	else
		obcol[0]= obcol[1]= obcol[2]= obcol[3]= 1.0f;

	float auto_bump_scale = ms.m_pDerivedMesh!=0 ? ms.m_pDerivedMesh->auto_bump_scale : 1.0f;
	GPU_material_bind_uniforms(gpumat, obmat, viewmat, viewinvmat, obcol, auto_bump_scale);

	mAlphaBlend = GPU_material_alpha_blend(gpumat, obcol);
}

int BL_BlenderShader::GetAlphaBlend()
{
	return mAlphaBlend;
}

bool BL_BlenderShader::Equals(BL_BlenderShader *blshader)
{
	/* to avoid unneeded state switches */
	return (blshader && mMat == blshader->mMat && mLightLayer == blshader->mLightLayer);
}

// eof

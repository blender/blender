
#include "DNA_customdata_types.h"
#include "DNA_material_types.h"
#include "DNA_scene_types.h"

#include "BKE_global.h"
#include "BKE_main.h"

#include "BL_BlenderShader.h"
#include "BL_Material.h"

#ifdef BLENDER_GLSL
#include "GPU_extensions.h"
#include "GPU_material.h"
#endif

#include "RAS_BucketManager.h"
#include "RAS_MeshObject.h"
#include "RAS_IRasterizer.h"
 
 /* this is evil, but we need the scene to create materials with
  * lights from the correct scene .. */
static struct Scene *GetSceneForName(const STR_String& scenename)
{
	Scene *sce;

	for (sce= (Scene*)G.main->scene.first; sce; sce= (Scene*)sce->id.next)
		if (scenename == (sce->id.name+2))
			return sce;

	return (Scene*)G.main->scene.first;
}

bool BL_BlenderShader::Ok()
{
#ifdef BLENDER_GLSL
	VerifyShader();

	return (mMat && mMat->gpumaterial);
#else
	return 0;
#endif
}

BL_BlenderShader::BL_BlenderShader(KX_Scene *scene, struct Material *ma, int lightlayer)
:
#ifdef BLENDER_GLSL
	mScene(scene),
	mMat(ma),
	mGPUMat(NULL),
#endif
	mBound(false),
	mLightLayer(lightlayer)
{
#ifdef BLENDER_GLSL
	mBlenderScene = GetSceneForName(scene->GetName());
	mBlendMode = GPU_BLEND_SOLID;

	if(mMat) {
		GPU_material_from_blender(mBlenderScene, mMat);
		mGPUMat = mMat->gpumaterial;
	}
#endif
}

BL_BlenderShader::~BL_BlenderShader()
{
#ifdef BLENDER_GLSL
	if(mMat && mMat->gpumaterial)
		GPU_material_unbind(mMat->gpumaterial);
#endif
}

bool BL_BlenderShader::VerifyShader()
{
#ifdef BLENDER_GLSL
	if(mMat && !mMat->gpumaterial)
		GPU_material_from_blender(mBlenderScene, mMat);

	mGPUMat = mMat->gpumaterial;
	
	return (mMat && mGPUMat);
#else
	return false;
#endif
}

void BL_BlenderShader::SetProg(bool enable)
{
#ifdef BLENDER_GLSL
	if(VerifyShader()) {
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

	if(!VerifyShader())
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

	if(!VerifyShader())
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
	float obmat[4][4], viewmat[4][4], viewinvmat[4][4], obcol[4];

	VerifyShader();

	if(!mGPUMat) // || !mBound)
		return;

	MT_Matrix4x4 model;
	model.setValue(ms.m_OpenGLMatrix);
	MT_Matrix4x4 view;
	rasty->GetViewMatrix(view);

	model.getValue((float*)obmat);
	view.getValue((float*)viewmat);

	view.invert();
	view.getValue((float*)viewinvmat);

	if(ms.m_bObjectColor)
		ms.m_RGBAcolor.getValue((float*)obcol);
	else
		obcol[0]= obcol[1]= obcol[2]= obcol[3]= 1.0f;

	GPU_material_bind_uniforms(mGPUMat, obmat, viewmat, viewinvmat, obcol);

	mBlendMode = GPU_material_blend_mode(mGPUMat, obcol);
#endif
}

int BL_BlenderShader::GetBlendMode()
{
	return mBlendMode;
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

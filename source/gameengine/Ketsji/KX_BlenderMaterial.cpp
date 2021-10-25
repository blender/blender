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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Ketsji/KX_BlenderMaterial.cpp
 *  \ingroup ketsji
 */

#include "GPU_glew.h"

#include "KX_BlenderMaterial.h"
#include "BL_Material.h"
#include "KX_Scene.h"
#include "KX_Light.h"
#include "KX_GameObject.h"
#include "KX_MeshProxy.h"
#include "KX_PyMath.h"

#include "MT_Vector3.h"
#include "MT_Vector4.h"
#include "MT_Matrix4x4.h"

#include "RAS_BucketManager.h"
#include "RAS_MeshObject.h"
#include "RAS_IRasterizer.h"

#include "GPU_draw.h"

#include "STR_HashedString.h"

// ------------------------------------
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_image_types.h"
#include "DNA_meshdata_types.h"
#include "BKE_mesh.h"
// ------------------------------------
#include "BLI_utildefines.h"
#include "BLI_math.h"

#define spit(x) std::cout << x << std::endl;

BL_Shader *KX_BlenderMaterial::mLastShader = NULL;
BL_BlenderShader *KX_BlenderMaterial::mLastBlenderShader = NULL;

//static PyObject *gTextureDict = 0;

KX_BlenderMaterial::KX_BlenderMaterial()
:	PyObjectPlus(),
	RAS_IPolyMaterial(),
	mMaterial(NULL),
	mShader(0),
	mBlenderShader(0),
	mScene(NULL),
	mUserDefBlend(0),
	mModified(0),
	mConstructed(false),
	mPass(0)
{
}

void KX_BlenderMaterial::Initialize(
	KX_Scene *scene,
	BL_Material *data,
	GameSettings *game,
	int lightlayer)
{
	RAS_IPolyMaterial::Initialize(
		data->texname[0],
		data->matname,
		data->materialindex,
		data->tile,
		data->tilexrep[0],
		data->tileyrep[0],
		data->alphablend,
		((data->ras_mode &ALPHA)!=0),
		((data->ras_mode &ZSORT)!=0),
		((data->ras_mode &USE_LIGHT)!=0),
		((data->ras_mode &TEX)),
		game
	);
	Material *ma = data->material;

	// Save material data to restore on exit
	mSavedData.r = ma->r;
	mSavedData.g = ma->g;
	mSavedData.b = ma->b;
	mSavedData.a = ma->alpha;
	mSavedData.specr = ma->specr;
	mSavedData.specg = ma->specg;
	mSavedData.specb = ma->specb;
	mSavedData.spec = ma->spec;
	mSavedData.ref = ma->ref;
	mSavedData.hardness = ma->har;
	mSavedData.emit = ma->emit;

	mMaterial = data;
	mShader = 0;
	mBlenderShader = 0;
	mScene = scene;
	mUserDefBlend = 0;
	mModified = 0;
	mConstructed = false;
	mPass = 0;
	mLightLayer = lightlayer;
	// --------------------------------
	// RAS_IPolyMaterial variables...
	m_flag |= RAS_BLENDERMAT;
	m_flag |= (mMaterial->IdMode>=ONETEX)? RAS_MULTITEX: 0;
	m_flag |= ((mMaterial->ras_mode & USE_LIGHT)!=0)? RAS_MULTILIGHT: 0;
	m_flag |= (mMaterial->glslmat)? RAS_BLENDERGLSL: 0;
	m_flag |= ((mMaterial->ras_mode & CAST_SHADOW)!=0)? RAS_CASTSHADOW: 0;
	m_flag |= ((mMaterial->ras_mode & ONLY_SHADOW)!=0)? RAS_ONLYSHADOW: 0;

	// test the sum of the various modes for equality
	// so we can ether accept or reject this material
	// as being equal, this is rather important to
	// prevent material bleeding
	for (int i=0; i<BL_Texture::GetMaxUnits(); i++) {
		m_multimode	+= (mMaterial->flag[i] + mMaterial->blend_mode[i]);
	}
	m_multimode += mMaterial->IdMode+ (mMaterial->ras_mode & ~(USE_LIGHT));
}

KX_BlenderMaterial::~KX_BlenderMaterial()
{
	Material *ma = mMaterial->material;
	// Restore Blender material data
	ma->r = mSavedData.r;
	ma->g = mSavedData.g;
	ma->b = mSavedData.b;
	ma->alpha = mSavedData.a;
	ma->specr = mSavedData.specr;
	ma->specg = mSavedData.specg;
	ma->specb = mSavedData.specb;
	ma->spec = mSavedData.spec;
	ma->ref = mSavedData.ref;
	ma->har = mSavedData.hardness;
	ma->emit = mSavedData.emit;

	// cleanup work
	if (mConstructed)
		// clean only if material was actually used
		OnExit();
}

MTexPoly *KX_BlenderMaterial::GetMTexPoly() const
{
	// fonts on polys
	return &mMaterial->mtexpoly;
}

unsigned int* KX_BlenderMaterial::GetMCol() const
{
	// fonts on polys
	return mMaterial->rgb;
}

void KX_BlenderMaterial::GetMaterialRGBAColor(unsigned char *rgba) const
{
	if (mMaterial) {
		*rgba++ = (unsigned char)(mMaterial->matcolor[0] * 255.0f);
		*rgba++ = (unsigned char)(mMaterial->matcolor[1] * 255.0f);
		*rgba++ = (unsigned char)(mMaterial->matcolor[2] * 255.0f);
		*rgba++ = (unsigned char)(mMaterial->matcolor[3] * 255.0f);
	} else
		RAS_IPolyMaterial::GetMaterialRGBAColor(rgba);
}

Material *KX_BlenderMaterial::GetBlenderMaterial() const
{
	return mMaterial->material;
}

Image *KX_BlenderMaterial::GetBlenderImage() const
{
	return mMaterial->mtexpoly.tpage;
}

Scene* KX_BlenderMaterial::GetBlenderScene() const
{
	return mScene->GetBlenderScene();
}

void KX_BlenderMaterial::ReleaseMaterial()
{
	if (mBlenderShader)
		mBlenderShader->ReloadMaterial();
}

void KX_BlenderMaterial::InitTextures()
{
	// for each unique material...
	int i;
	for (i=0; i<BL_Texture::GetMaxUnits(); i++) {
		if ( mMaterial->mapping[i].mapping & USEENV ) {
			if (!GLEW_ARB_texture_cube_map) {
				spit("CubeMap textures not supported");
				continue;
			}
			if (!mTextures[i].InitCubeMap(i, mMaterial->cubemap[i] ) )
				spit("unable to initialize image("<<i<<") in "<<
				     mMaterial->matname<< ", image will not be available");
		}
		/* If we're using glsl materials, the textures are handled by bf_gpu, so don't load them twice!
		 * However, if we're using a custom shader, then we still need to load the textures ourselves. */
		else if (!mMaterial->glslmat || mShader) {
			if ( mMaterial->img[i] ) {
				if ( ! mTextures[i].InitFromImage(i, mMaterial->img[i], (mMaterial->flag[i] &MIPMAP)!=0 ))
					spit("unable to initialize image("<<i<<") in "<< 
						mMaterial->matname<< ", image will not be available");
			}
		}
	}
}

void KX_BlenderMaterial::OnConstruction()
{
	if (mConstructed)
		// when material are reused between objects
		return;
	
	if (mMaterial->glslmat)
		SetBlenderGLSLShader();

	InitTextures();

	mBlendFunc[0] =0;
	mBlendFunc[1] =0;
	mConstructed = true;
}

void KX_BlenderMaterial::EndFrame()
{
	if (mLastBlenderShader) {
		mLastBlenderShader->SetProg(false);
		mLastBlenderShader = NULL;
	}

	if (mLastShader) {
		mLastShader->SetProg(false);
		mLastShader = NULL;
	}
}

void KX_BlenderMaterial::OnExit()
{
	if ( mShader ) {
		//note, the shader here is allocated, per unique material
		//and this function is called per face
		if (mShader == mLastShader) {
			mShader->SetProg(false);
			mLastShader = NULL;
		}

		delete mShader;
		mShader = 0;
	}

	if ( mBlenderShader ) {
		if (mBlenderShader == mLastBlenderShader) {
			mBlenderShader->SetProg(false);
			mLastBlenderShader = NULL;
		}

		delete mBlenderShader;
		mBlenderShader = 0;
	}

	BL_Texture::ActivateFirst();
	for (int i=0; i<BL_Texture::GetMaxUnits(); i++) {
		if (!mTextures[i].Ok()) continue;
		BL_Texture::ActivateUnit(i);
		mTextures[i].DeleteTex();
		mTextures[i].DisableUnit();
	}

	/* used to call with 'mMaterial->tface' but this can be a freed array,
	 * see: [#30493], so just call with NULL, this is best since it clears
	 * the 'lastface' pointer in GPU too - campbell */
	GPU_set_tpage(NULL, 1, mMaterial->alphablend);
}


void KX_BlenderMaterial::setShaderData( bool enable, RAS_IRasterizer *ras)
{
	MT_assert(GLEW_ARB_shader_objects && mShader);

	int i;
	if ( !enable || !mShader->Ok() ) {
		// frame cleanup.
		if (mShader == mLastShader) {
			mShader->SetProg(false);
			mLastShader = NULL;
		}

		ras->SetAlphaBlend(TF_SOLID);
		BL_Texture::DisableAllTextures();
		return;
	}

	BL_Texture::DisableAllTextures();
	mShader->SetProg(true);
	mLastShader = mShader;
	
	BL_Texture::ActivateFirst();

	mShader->ApplyShader();

	// for each enabled unit
	for (i=0; i<BL_Texture::GetMaxUnits(); i++) {
		if (!mTextures[i].Ok()) continue;
		mTextures[i].ActivateTexture();
		mTextures[0].SetMapping(mMaterial->mapping[i].mapping);
	}

	if (!mUserDefBlend) {
		ras->SetAlphaBlend(mMaterial->alphablend);
	}
	else {
		ras->SetAlphaBlend(TF_SOLID);
		ras->SetAlphaBlend(-1); // indicates custom mode

		// tested to be valid enums
		glEnable(GL_BLEND);
		glBlendFunc(mBlendFunc[0], mBlendFunc[1]);
	}
}

void KX_BlenderMaterial::setBlenderShaderData( bool enable, RAS_IRasterizer *ras)
{
	if ( !enable || !mBlenderShader->Ok() ) {
		ras->SetAlphaBlend(TF_SOLID);

		// frame cleanup.
		if (mLastBlenderShader) {
			mLastBlenderShader->SetProg(false);
			mLastBlenderShader= NULL;
		}
		else
			BL_Texture::DisableAllTextures();

		return;
	}

	if (!mBlenderShader->Equals(mLastBlenderShader)) {
		ras->SetAlphaBlend(mMaterial->alphablend);

		if (mLastBlenderShader)
			mLastBlenderShader->SetProg(false);
		else
			BL_Texture::DisableAllTextures();

		mBlenderShader->SetProg(true, ras->GetTime(), ras);
		mLastBlenderShader= mBlenderShader;
	}
}

void KX_BlenderMaterial::setTexData( bool enable, RAS_IRasterizer *ras)
{
	BL_Texture::DisableAllTextures();

	if ( !enable ) {
		ras->SetAlphaBlend(TF_SOLID);
		return;
	}

	BL_Texture::ActivateFirst();

	if ( mMaterial->IdMode == DEFAULT_BLENDER ) {
		ras->SetAlphaBlend(mMaterial->alphablend);
		return;
	}

	if ( mMaterial->IdMode == TEXFACE ) {
		// no material connected to the object
		if ( mTextures[0].Ok() ) {
			mTextures[0].ActivateTexture();
			mTextures[0].setTexEnv(0, true);
			mTextures[0].SetMapping(mMaterial->mapping[0].mapping);
			ras->SetAlphaBlend(mMaterial->alphablend);
		}
		return;
	}

	int mode = 0,i=0;
	for (i=0; i<BL_Texture::GetMaxUnits(); i++) {
		if ( !mTextures[i].Ok() ) continue;

		mTextures[i].ActivateTexture();
		mTextures[i].setTexEnv(mMaterial);
		mode = mMaterial->mapping[i].mapping;

		if (mode &USEOBJ)
			setObjectMatrixData(i, ras);
		else
			mTextures[i].SetMapping(mode);
		
		if (!(mode &USEOBJ))
			setTexMatrixData( i );
	}

	if (!mUserDefBlend) {
		ras->SetAlphaBlend(mMaterial->alphablend);
	}
	else {
		ras->SetAlphaBlend(TF_SOLID);
		ras->SetAlphaBlend(-1); // indicates custom mode

		glEnable(GL_BLEND);
		glBlendFunc(mBlendFunc[0], mBlendFunc[1]);
	}
}

void
KX_BlenderMaterial::ActivatShaders(
	RAS_IRasterizer* rasty, 
	TCachingInfo& cachingInfo)const
{
	KX_BlenderMaterial *tmp = const_cast<KX_BlenderMaterial*>(this);

	// reset... 
	if (tmp->mMaterial->IsShared()) 
		cachingInfo =0;

	if (mLastBlenderShader) {
		mLastBlenderShader->SetProg(false);
		mLastBlenderShader= NULL;
	}

	if (GetCachingInfo() != cachingInfo) {

		if (!cachingInfo)
			tmp->setShaderData(false, rasty);
		
		cachingInfo = GetCachingInfo();
	
		if (rasty->GetDrawingMode() == RAS_IRasterizer::KX_TEXTURED)
			tmp->setShaderData(true, rasty);
		else if (rasty->GetDrawingMode() == RAS_IRasterizer::KX_SHADOW && mMaterial->alphablend != GEMAT_SOLID && !rasty->GetUsingOverrideShader())
			tmp->setShaderData(true, rasty);
		else
			tmp->setShaderData(false, rasty);

		if (mMaterial->ras_mode &TWOSIDED)
			rasty->SetCullFace(false);
		else
			rasty->SetCullFace(true);

		if ((mMaterial->ras_mode &WIRE) ||
		    (rasty->GetDrawingMode() <= RAS_IRasterizer::KX_WIREFRAME))
		{
			if (mMaterial->ras_mode &WIRE) 
				rasty->SetCullFace(false);
			rasty->SetLines(true);
		}
		else
			rasty->SetLines(false);
		ActivatGLMaterials(rasty);
		ActivateTexGen(rasty);
	}

	//ActivatGLMaterials(rasty);
	//ActivateTexGen(rasty);
}

void
KX_BlenderMaterial::ActivateBlenderShaders(
	RAS_IRasterizer* rasty, 
	TCachingInfo& cachingInfo)const
{
	KX_BlenderMaterial *tmp = const_cast<KX_BlenderMaterial*>(this);

	if (mLastShader) {
		mLastShader->SetProg(false);
		mLastShader= NULL;
	}

	if (GetCachingInfo() != cachingInfo) {
		if (!cachingInfo)
			tmp->setBlenderShaderData(false, rasty);
		
		cachingInfo = GetCachingInfo();
	
		if (rasty->GetDrawingMode() == RAS_IRasterizer::KX_TEXTURED)
			tmp->setBlenderShaderData(true, rasty);
		else if (rasty->GetDrawingMode() == RAS_IRasterizer::KX_SHADOW && mMaterial->alphablend != GEMAT_SOLID && !rasty->GetUsingOverrideShader())
			tmp->setBlenderShaderData(true, rasty);
		else
			tmp->setBlenderShaderData(false, rasty);

		if (mMaterial->ras_mode &TWOSIDED)
			rasty->SetCullFace(false);
		else
			rasty->SetCullFace(true);

		if ((mMaterial->ras_mode &WIRE) ||
		    (rasty->GetDrawingMode() <= RAS_IRasterizer::KX_WIREFRAME))
		{
			if (mMaterial->ras_mode &WIRE) 
				rasty->SetCullFace(false);
			rasty->SetLines(true);
		}
		else
			rasty->SetLines(false);

		ActivatGLMaterials(rasty);
		mBlenderShader->SetAttribs(rasty, mMaterial);
	}
}

void
KX_BlenderMaterial::ActivateMat( 
	RAS_IRasterizer* rasty,  
	TCachingInfo& cachingInfo
	)const
{
	KX_BlenderMaterial *tmp = const_cast<KX_BlenderMaterial*>(this);

	if (mLastShader) {
		mLastShader->SetProg(false);
		mLastShader= NULL;
	}

	if (mLastBlenderShader) {
		mLastBlenderShader->SetProg(false);
		mLastBlenderShader= NULL;
	}

	if (GetCachingInfo() != cachingInfo) {
		if (!cachingInfo) 
			tmp->setTexData( false,rasty );
		
		cachingInfo = GetCachingInfo();

		if (rasty->GetDrawingMode() == RAS_IRasterizer::KX_TEXTURED)
			tmp->setTexData( true,rasty  );
		else if (rasty->GetDrawingMode() == RAS_IRasterizer::KX_SHADOW && mMaterial->alphablend != GEMAT_SOLID && !rasty->GetUsingOverrideShader())
			tmp->setTexData(true, rasty);
		else
			tmp->setTexData( false,rasty);

		if (mMaterial->ras_mode &TWOSIDED)
			rasty->SetCullFace(false);
		else
			rasty->SetCullFace(true);

		if ((mMaterial->ras_mode &WIRE) ||
		    (rasty->GetDrawingMode() <= RAS_IRasterizer::KX_WIREFRAME))
		{
			if (mMaterial->ras_mode &WIRE) 
				rasty->SetCullFace(false);
			rasty->SetLines(true);
		}
		else
			rasty->SetLines(false);
		ActivatGLMaterials(rasty);
		ActivateTexGen(rasty);
	}

	//ActivatGLMaterials(rasty);
	//ActivateTexGen(rasty);
}

bool 
KX_BlenderMaterial::Activate( 
	RAS_IRasterizer* rasty,  
	TCachingInfo& cachingInfo
	)const
{
	if (GLEW_ARB_shader_objects && (mShader && mShader->Ok())) {
		if ((mPass++) < mShader->getNumPass() ) {
			ActivatShaders(rasty, cachingInfo);
			return true;
		}
		else {
			if (mShader == mLastShader) {
				mShader->SetProg(false);
				mLastShader = NULL;
			}
			mPass = 0;
			return false;
		}
	}
	else if ( GLEW_ARB_shader_objects && (mBlenderShader && mBlenderShader->Ok() ) ) {
		if (mPass++ == 0) {
			ActivateBlenderShaders(rasty, cachingInfo);
			return true;
		}
		else {
			mPass = 0;
			return false;
		}
	}
	else {
		if (mPass++ == 0) {
			ActivateMat(rasty, cachingInfo);
			return true;
		}
		else {
			mPass = 0;
			return false;
		}
	}
}

bool KX_BlenderMaterial::UsesLighting(RAS_IRasterizer *rasty) const
{
	if (!RAS_IPolyMaterial::UsesLighting(rasty))
		return false;

	if (mShader && mShader->Ok())
		return true;
	else if (mBlenderShader && mBlenderShader->Ok())
		return false;
	else
		return true;
}

void KX_BlenderMaterial::ActivateMeshSlot(const RAS_MeshSlot & ms, RAS_IRasterizer* rasty) const
{
	if (mShader && GLEW_ARB_shader_objects) {
		mShader->Update(ms, rasty);
	}
	else if (mBlenderShader && GLEW_ARB_shader_objects) {
		int alphablend;

		mBlenderShader->Update(ms, rasty);

		/* we do blend modes here, because they can change per object
		 * with the same material due to obcolor/obalpha */
		alphablend = mBlenderShader->GetAlphaBlend();
		if (ELEM(alphablend, GEMAT_SOLID, GEMAT_ALPHA, GEMAT_ALPHA_SORT) && mMaterial->alphablend != GEMAT_SOLID)
			alphablend = mMaterial->alphablend;

		rasty->SetAlphaBlend(alphablend);
	}
}

void KX_BlenderMaterial::ActivatGLMaterials( RAS_IRasterizer* rasty )const
{
	if (mShader || !mBlenderShader) {
		rasty->SetSpecularity(
			mMaterial->speccolor[0]*mMaterial->spec_f,
			mMaterial->speccolor[1]*mMaterial->spec_f,
			mMaterial->speccolor[2]*mMaterial->spec_f,
			mMaterial->spec_f
		);

		rasty->SetShinyness( mMaterial->hard );

		rasty->SetDiffuse(
			mMaterial->matcolor[0]*mMaterial->ref+mMaterial->emit, 
			mMaterial->matcolor[1]*mMaterial->ref+mMaterial->emit,
			mMaterial->matcolor[2]*mMaterial->ref+mMaterial->emit,
			1.0f);

		rasty->SetEmissive(
			mMaterial->matcolor[0]*mMaterial->emit,
			mMaterial->matcolor[1]*mMaterial->emit,
			mMaterial->matcolor[2]*mMaterial->emit,
			1.0f );

		rasty->SetAmbient(mMaterial->amb);
	}

	if (mMaterial->material)
		rasty->SetPolygonOffset(-mMaterial->material->zoffs, 0.0f);
}


void KX_BlenderMaterial::ActivateTexGen(RAS_IRasterizer *ras) const
{
	if (ras->GetDrawingMode() == RAS_IRasterizer::KX_TEXTURED || 
		(ras->GetDrawingMode() == RAS_IRasterizer::KX_SHADOW && mMaterial->alphablend != GEMAT_SOLID && !ras->GetUsingOverrideShader())) {
		ras->SetAttribNum(0);
		if (mShader && GLEW_ARB_shader_objects) {
			if (mShader->GetAttribute() == BL_Shader::SHD_TANGENT) {
				ras->SetAttrib(RAS_IRasterizer::RAS_TEXCO_DISABLE, 0);
				ras->SetAttrib(RAS_IRasterizer::RAS_TEXTANGENT, 1);
				ras->SetAttribNum(2);
			}
		}

		ras->SetTexCoordNum(mMaterial->num_enabled);

		for (int i=0; i<BL_Texture::GetMaxUnits(); i++) {
			int mode = mMaterial->mapping[i].mapping;

			if ( mode &(USEREFL|USEOBJ))
				ras->SetTexCoord(RAS_IRasterizer::RAS_TEXCO_GEN, i);
			else if (mode &USEORCO)
				ras->SetTexCoord(RAS_IRasterizer::RAS_TEXCO_ORCO, i);
			else if (mode &USENORM)
				ras->SetTexCoord(RAS_IRasterizer::RAS_TEXCO_NORM, i);
			else if (mode &USEUV)
				ras->SetTexCoord(RAS_IRasterizer::RAS_TEXCO_UV, i);
			else if (mode &USETANG)
				ras->SetTexCoord(RAS_IRasterizer::RAS_TEXTANGENT, i);
			else 
				ras->SetTexCoord(RAS_IRasterizer::RAS_TEXCO_DISABLE, i);
		}
	}
}

void KX_BlenderMaterial::setTexMatrixData(int i)
{
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();

	if ( GLEW_ARB_texture_cube_map && 
		mTextures[i].GetTextureType() == GL_TEXTURE_CUBE_MAP_ARB && 
		mMaterial->mapping[i].mapping & USEREFL) {
		glScalef( 
			mMaterial->mapping[i].scale[0], 
			-mMaterial->mapping[i].scale[1], 
			-mMaterial->mapping[i].scale[2]
		);
	}
	else
	{
		glScalef( 
			mMaterial->mapping[i].scale[0], 
			mMaterial->mapping[i].scale[1], 
			mMaterial->mapping[i].scale[2]
		);
	}
	glTranslatef(
		mMaterial->mapping[i].offsets[0],
		mMaterial->mapping[i].offsets[1], 
		mMaterial->mapping[i].offsets[2]
	);

	glMatrixMode(GL_MODELVIEW);

}

static void GetProjPlane(BL_Material *mat, int index,int num, float*param)
{
	param[0]=param[1]=param[2]=param[3]=0.f;
	if ( mat->mapping[index].projplane[num] == PROJX )
		param[0] = 1.f;
	else if ( mat->mapping[index].projplane[num] == PROJY )
		param[1] = 1.f;
	else if ( mat->mapping[index].projplane[num] == PROJZ)
		param[2] = 1.f;
}

void KX_BlenderMaterial::setObjectMatrixData(int i, RAS_IRasterizer *ras)
{
	KX_GameObject *obj = 
		(KX_GameObject*)
		mScene->GetObjectList()->FindValue(mMaterial->mapping[i].objconame);

	if (!obj) return;

	glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR );
	glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR );
	glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_EYE_LINEAR );

	GLenum plane = GL_EYE_PLANE;

	// figure plane gen
	float proj[4] = {0.f,0.f,0.f,0.f};
	GetProjPlane(mMaterial, i, 0, proj);
	glTexGenfv(GL_S, plane, proj);
	
	GetProjPlane(mMaterial, i, 1, proj);
	glTexGenfv(GL_T, plane, proj);

	GetProjPlane(mMaterial, i, 2, proj);
	glTexGenfv(GL_R, plane, proj);

	glEnable(GL_TEXTURE_GEN_S);
	glEnable(GL_TEXTURE_GEN_T);
	glEnable(GL_TEXTURE_GEN_R);

	const MT_Matrix4x4& mvmat = ras->GetViewMatrix();

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glScalef( 
		mMaterial->mapping[i].scale[0], 
		mMaterial->mapping[i].scale[1], 
		mMaterial->mapping[i].scale[2]
	);

	MT_Point3 pos = obj->NodeGetWorldPosition();
	MT_Vector4 matmul = MT_Vector4(pos[0], pos[1], pos[2], 1.f);
	MT_Vector4 t = mvmat*matmul;

	glTranslatef( (float)(-t[0]), (float)(-t[1]), (float)(-t[2]) );

	glMatrixMode(GL_MODELVIEW);

}

// ------------------------------------
void KX_BlenderMaterial::UpdateIPO(
	MT_Vector4 rgba,
	MT_Vector3 specrgb,
	MT_Scalar hard,
	MT_Scalar spec,
	MT_Scalar ref,
	MT_Scalar emit,
	MT_Scalar alpha
	)
{
	// only works one deep now

	// GLSL							Multitexture				Input
	mMaterial->material->specr	= mMaterial->speccolor[0]	= (float)(specrgb)[0];
	mMaterial->material->specg	= mMaterial->speccolor[1]	= (float)(specrgb)[1];
	mMaterial->material->specb	= mMaterial->speccolor[2]	= (float)(specrgb)[2];
	mMaterial->material->r		= mMaterial->matcolor[0]	= (float)(rgba[0]);
	mMaterial->material->g		= mMaterial->matcolor[1]	= (float)(rgba[1]);
	mMaterial->material->b		= mMaterial->matcolor[2]	= (float)(rgba[2]);
	mMaterial->material->alpha	= mMaterial->alpha			= (float)(rgba[3]);
	mMaterial->material->har	= mMaterial->hard			= (float)(hard);
	mMaterial->material->emit	= mMaterial->emit			= (float)(emit);
	mMaterial->material->spec	= mMaterial->spec_f			= (float)(spec);
	mMaterial->material->ref	= mMaterial->ref			= (float)(ref);
}

void KX_BlenderMaterial::Replace_IScene(SCA_IScene *val)
{
	mScene= static_cast<KX_Scene *>(val);

	OnConstruction();
}

BL_Material *KX_BlenderMaterial::GetBLMaterial()
{
	return mMaterial;
}

void KX_BlenderMaterial::SetBlenderGLSLShader()
{
	if (!mBlenderShader)
		mBlenderShader = new BL_BlenderShader(mScene, mMaterial->material, mLightLayer);

	if (!mBlenderShader->Ok()) {
		delete mBlenderShader;
		mBlenderShader = 0;
	}
}

#ifdef USE_MATHUTILS

#define MATHUTILS_COL_CB_MATERIAL_SPECULAR_COLOR 1
#define MATHUTILS_COL_CB_MATERIAL_DIFFUSE_COLOR 2

static unsigned char mathutils_kxblendermaterial_color_cb_index = -1; /* index for our callbacks */

static int mathutils_kxblendermaterial_generic_check(BaseMathObject *bmo)
{
	KX_BlenderMaterial *self = static_cast<KX_BlenderMaterial *>BGE_PROXY_REF(bmo->cb_user);
	if (!self)
		return -1;

	return 0;
}

static int mathutils_kxblendermaterial_color_get(BaseMathObject *bmo, int subtype)
{
	KX_BlenderMaterial *self = static_cast<KX_BlenderMaterial*>BGE_PROXY_REF(bmo->cb_user);
	if (!self)
		return -1;

	switch (subtype) {
		case MATHUTILS_COL_CB_MATERIAL_DIFFUSE_COLOR:
		{
			copy_v3_v3(bmo->data, self->GetBLMaterial()->matcolor);
			break;
		}
		case MATHUTILS_COL_CB_MATERIAL_SPECULAR_COLOR:
		{
			copy_v3_v3(bmo->data, self->GetBLMaterial()->speccolor);
			break;
		}
	}

	return 0;
}

static int mathutils_kxblendermaterial_color_set(BaseMathObject *bmo, int subtype)
{
	KX_BlenderMaterial *self = static_cast<KX_BlenderMaterial *>BGE_PROXY_REF(bmo->cb_user);
	if (!self)
		return -1;

	switch (subtype) {
		case MATHUTILS_COL_CB_MATERIAL_DIFFUSE_COLOR:
		{
			BL_Material *mat = self->GetBLMaterial();
			copy_v3_v3(mat->matcolor, bmo->data);
			mat->material->r = bmo->data[0];
			mat->material->g = bmo->data[1];
			mat->material->b = bmo->data[2];
			break;
		}
		case MATHUTILS_COL_CB_MATERIAL_SPECULAR_COLOR:
		{
			BL_Material *mat = self->GetBLMaterial();
			copy_v3_v3(mat->speccolor, bmo->data);
			mat->material->specr = bmo->data[0];
			mat->material->specg = bmo->data[1];
			mat->material->specb = bmo->data[2];
			break;
		}
	}

	return 0;
}

static int mathutils_kxblendermaterial_color_get_index(BaseMathObject *bmo, int subtype, int index)
{
	/* lazy, avoid repeteing the case statement */
	if (mathutils_kxblendermaterial_color_get(bmo, subtype) == -1)
		return -1;
	return 0;
}

static int mathutils_kxblendermaterial_color_set_index(BaseMathObject *bmo, int subtype, int index)
{
	float f = bmo->data[index];

	/* lazy, avoid repeateing the case statement */
	if (mathutils_kxblendermaterial_color_get(bmo, subtype) == -1)
		return -1;

	bmo->data[index] = f;
	return mathutils_kxblendermaterial_color_set(bmo, subtype);
}

static Mathutils_Callback mathutils_kxblendermaterial_color_cb = {
	mathutils_kxblendermaterial_generic_check,
	mathutils_kxblendermaterial_color_get,
	mathutils_kxblendermaterial_color_set,
	mathutils_kxblendermaterial_color_get_index,
	mathutils_kxblendermaterial_color_set_index
};


void KX_BlenderMaterial_Mathutils_Callback_Init()
{
	// register mathutils callbacks, ok to run more than once.
	mathutils_kxblendermaterial_color_cb_index = Mathutils_RegisterCallback(&mathutils_kxblendermaterial_color_cb);
}

#endif // USE_MATHUTILS

#ifdef WITH_PYTHON

PyMethodDef KX_BlenderMaterial::Methods[] = 
{
	KX_PYMETHODTABLE( KX_BlenderMaterial, getShader ),
	KX_PYMETHODTABLE( KX_BlenderMaterial, getMaterialIndex ),
	KX_PYMETHODTABLE( KX_BlenderMaterial, getTextureBindcode ),
	KX_PYMETHODTABLE( KX_BlenderMaterial, setBlending ),
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_BlenderMaterial::Attributes[] = {
	KX_PYATTRIBUTE_RO_FUNCTION("shader", KX_BlenderMaterial, pyattr_get_shader),
	KX_PYATTRIBUTE_RO_FUNCTION("material_index", KX_BlenderMaterial, pyattr_get_materialIndex),
	KX_PYATTRIBUTE_RW_FUNCTION("blending", KX_BlenderMaterial, pyattr_get_blending, pyattr_set_blending),
	KX_PYATTRIBUTE_RW_FUNCTION("alpha", KX_BlenderMaterial, pyattr_get_alpha, pyattr_set_alpha),
	KX_PYATTRIBUTE_RW_FUNCTION("hardness", KX_BlenderMaterial, pyattr_get_hardness, pyattr_set_hardness),
	KX_PYATTRIBUTE_RW_FUNCTION("specularIntensity", KX_BlenderMaterial, pyattr_get_specular_intensity, pyattr_set_specular_intensity),
	KX_PYATTRIBUTE_RW_FUNCTION("specularColor", KX_BlenderMaterial, pyattr_get_specular_color, pyattr_set_specular_color),
	KX_PYATTRIBUTE_RW_FUNCTION("diffuseIntensity", KX_BlenderMaterial, pyattr_get_diffuse_intensity, pyattr_set_diffuse_intensity),
	KX_PYATTRIBUTE_RW_FUNCTION("diffuseColor", KX_BlenderMaterial, pyattr_get_diffuse_color, pyattr_set_diffuse_color),
	KX_PYATTRIBUTE_RW_FUNCTION("emit", KX_BlenderMaterial, pyattr_get_emit, pyattr_set_emit),

	{ NULL }	//Sentinel
};

PyTypeObject KX_BlenderMaterial::Type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"KX_BlenderMaterial",
	sizeof(PyObjectPlus_Proxy),
	0,
	py_base_dealloc,
	0,
	0,
	0,
	0,
	py_base_repr,
	0,0,0,0,0,0,0,0,0,
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	0,0,0,0,0,0,0,
	Methods,
	0,
	0,
	&PyObjectPlus::Type,
	0,0,0,0,0,0,
	py_base_new
};

PyObject *KX_BlenderMaterial::pyattr_get_shader(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_BlenderMaterial* self = static_cast<KX_BlenderMaterial*>(self_v);
	return self->PygetShader(NULL, NULL);
}

PyObject *KX_BlenderMaterial::pyattr_get_materialIndex(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_BlenderMaterial* self = static_cast<KX_BlenderMaterial*>(self_v);
	return PyLong_FromLong(self->GetMaterialIndex());
}

PyObject *KX_BlenderMaterial::pyattr_get_blending(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_BlenderMaterial* self = static_cast<KX_BlenderMaterial*>(self_v);
	unsigned int* bfunc = self->getBlendFunc();
	return Py_BuildValue("(ll)", (long int)bfunc[0], (long int)bfunc[1]);
}

PyObject *KX_BlenderMaterial::pyattr_get_alpha(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_BlenderMaterial *self = static_cast<KX_BlenderMaterial *>(self_v);
	return PyFloat_FromDouble(self->GetBLMaterial()->alpha);
}

int KX_BlenderMaterial::pyattr_set_alpha(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_BlenderMaterial *self = static_cast<KX_BlenderMaterial *>(self_v);
	float val = PyFloat_AsDouble(value);

	if (val == -1 && PyErr_Occurred()) {
		PyErr_Format(PyExc_AttributeError, "material.%s = float: KX_BlenderMaterial, expected a float", attrdef->m_name);
		return PY_SET_ATTR_FAIL;
	}

	CLAMP(val, 0.0f, 1.0f);

	BL_Material *mat = self->GetBLMaterial();
	mat->alpha = mat->material->alpha = val;
	return PY_SET_ATTR_SUCCESS;
}
PyObject *KX_BlenderMaterial::pyattr_get_hardness(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_BlenderMaterial *self = static_cast<KX_BlenderMaterial *>(self_v);
	return PyLong_FromLong(self->GetBLMaterial()->hard);
}

int KX_BlenderMaterial::pyattr_set_hardness(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_BlenderMaterial *self = static_cast<KX_BlenderMaterial *>(self_v);
	int val = PyLong_AsLong(value);

	if (val == -1 && PyErr_Occurred()) {
		PyErr_Format(PyExc_AttributeError, "material.%s = int: KX_BlenderMaterial, expected a int", attrdef->m_name);
		return PY_SET_ATTR_FAIL;
	}

	CLAMP(val, 1, 511);

	BL_Material *mat = self->GetBLMaterial();
	mat->hard = mat->material->har = val;
	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_BlenderMaterial::pyattr_get_specular_intensity(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_BlenderMaterial *self = static_cast<KX_BlenderMaterial *>(self_v);
	return PyFloat_FromDouble(self->GetBLMaterial()->spec_f);
}

int KX_BlenderMaterial::pyattr_set_specular_intensity(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_BlenderMaterial *self = static_cast<KX_BlenderMaterial *>(self_v);
	float val = PyFloat_AsDouble(value);

	if (val == -1 && PyErr_Occurred()) {
		PyErr_Format(PyExc_AttributeError, "material.%s = float: KX_BlenderMaterial, expected a float", attrdef->m_name);
		return PY_SET_ATTR_FAIL;
	}

	CLAMP(val, 0.0f, 1.0f);

	BL_Material *mat = self->GetBLMaterial();
	mat->spec_f = mat->material->spec = val;
	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_BlenderMaterial::pyattr_get_specular_color(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
#ifdef USE_MATHUTILS
	return Color_CreatePyObject_cb(BGE_PROXY_FROM_REF(self_v), mathutils_kxblendermaterial_color_cb_index, MATHUTILS_COL_CB_MATERIAL_SPECULAR_COLOR);
#else
	KX_BlenderMaterial *self = static_cast<KX_BlenderMaterial *>(self_v);
	return PyColorFromVector(MT_Vector3(self->GetBLMaterial()->speccolor));
#endif
}

int KX_BlenderMaterial::pyattr_set_specular_color(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_BlenderMaterial *self = static_cast<KX_BlenderMaterial *>(self_v);
	MT_Vector3 color;
	if (!PyVecTo(value, color))
		return PY_SET_ATTR_FAIL;

	BL_Material *mat = self->GetBLMaterial();
	color.getValue(mat->speccolor);
	mat->material->specr = color[0];
	mat->material->specg = color[1];
	mat->material->specb = color[2];
	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_BlenderMaterial::pyattr_get_diffuse_intensity(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_BlenderMaterial *self = static_cast<KX_BlenderMaterial *>(self_v);
	return PyFloat_FromDouble(self->GetBLMaterial()->ref);
}

int KX_BlenderMaterial::pyattr_set_diffuse_intensity(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_BlenderMaterial *self = static_cast<KX_BlenderMaterial *>(self_v);
	float val = PyFloat_AsDouble(value);

	if (val == -1 && PyErr_Occurred()) {
		PyErr_Format(PyExc_AttributeError, "material.%s = float: KX_BlenderMaterial, expected a float", attrdef->m_name);
		return PY_SET_ATTR_FAIL;
	}

	CLAMP(val, 0.0f, 1.0f);

	BL_Material *mat = self->GetBLMaterial();
	mat->ref = mat->material->ref = val;
	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_BlenderMaterial::pyattr_get_diffuse_color(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
#ifdef USE_MATHUTILS
	return Color_CreatePyObject_cb(BGE_PROXY_FROM_REF(self_v), mathutils_kxblendermaterial_color_cb_index, MATHUTILS_COL_CB_MATERIAL_DIFFUSE_COLOR);
#else
	KX_BlenderMaterial *self = static_cast<KX_BlenderMaterial *>(self_v);
	return PyColorFromVector(MT_Vector3(self->GetBLMaterial()->matcolor));
#endif
}

int KX_BlenderMaterial::pyattr_set_diffuse_color(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_BlenderMaterial *self = static_cast<KX_BlenderMaterial *>(self_v);
	MT_Vector3 color;
	if (!PyVecTo(value, color))
		return PY_SET_ATTR_FAIL;

	BL_Material *mat = self->GetBLMaterial();
	color.getValue(mat->matcolor);
	mat->material->r = color[0];
	mat->material->g = color[1];
	mat->material->b = color[2];
	return PY_SET_ATTR_SUCCESS;
}

PyObject *KX_BlenderMaterial::pyattr_get_emit(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_BlenderMaterial *self = static_cast<KX_BlenderMaterial *>(self_v);
	return PyFloat_FromDouble(self->GetBLMaterial()->emit);
}

int KX_BlenderMaterial::pyattr_set_emit(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_BlenderMaterial *self = static_cast<KX_BlenderMaterial *>(self_v);
	float val = PyFloat_AsDouble(value);

	if (val == -1 && PyErr_Occurred()) {
		PyErr_Format(PyExc_AttributeError, "material.%s = float: KX_BlenderMaterial, expected a float", attrdef->m_name);
		return PY_SET_ATTR_FAIL;
	}

	CLAMP(val, 0.0f, 2.0f);

	BL_Material *mat = self->GetBLMaterial();
	mat->emit = mat->material->emit = val;
	return PY_SET_ATTR_SUCCESS;
}

int KX_BlenderMaterial::pyattr_set_blending(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_BlenderMaterial* self = static_cast<KX_BlenderMaterial*>(self_v);
	PyObject *obj = self->PysetBlending(value, NULL);
	if (obj)
	{
		Py_DECREF(obj);
		return 0;
	}
	return -1;
}

KX_PYMETHODDEF_DOC( KX_BlenderMaterial, getShader , "getShader()")
{
	if ( !GLEW_ARB_fragment_shader) {
		if (!mModified)
			spit("Fragment shaders not supported");
	
		mModified = true;
		Py_RETURN_NONE;
	}

	if ( !GLEW_ARB_vertex_shader) {
		if (!mModified)
			spit("Vertex shaders not supported");

		mModified = true;
		Py_RETURN_NONE;
	}

	if (!GLEW_ARB_shader_objects) {
		if (!mModified)
			spit("GLSL not supported");
		mModified = true;
		Py_RETURN_NONE;
	}
	else {
		// returns Py_None on error
		// the calling script will need to check

		if (!mShader && !mModified) {
			mShader = new BL_Shader();
			mModified = true;

			// Using a custom shader, make sure to initialize textures
			InitTextures();
		}

		if (mShader && !mShader->GetError()) {
			m_flag &= ~RAS_BLENDERGLSL;
			mMaterial->SetSharedMaterial(true);
			mScene->GetBucketManager()->ReleaseDisplayLists(this);
			return mShader->GetProxy();
		}
		else {
			// decref all references to the object
			// then delete it!
			// We will then go back to fixed functionality
			// for this material
			if (mShader) {
				delete mShader; /* will handle python de-referencing */
				mShader=0;
			}
		}
		Py_RETURN_NONE;
	}
	PyErr_SetString(PyExc_ValueError, "material.getShader(): KX_BlenderMaterial, GLSL Error");
	return NULL;
}

KX_PYMETHODDEF_DOC( KX_BlenderMaterial, getMaterialIndex, "getMaterialIndex()")
{
	return PyLong_FromLong(GetMaterialIndex());
}

KX_PYMETHODDEF_DOC( KX_BlenderMaterial, getTexture, "getTexture( index )" )
{
	// TODO: enable python switching
	return NULL;
}

KX_PYMETHODDEF_DOC( KX_BlenderMaterial, setTexture , "setTexture( index, tex)")
{
	// TODO: enable python switching
	return NULL;
}

static const unsigned int GL_array[11] = {
	GL_ZERO,
	GL_ONE,
	GL_SRC_COLOR,
	GL_ONE_MINUS_SRC_COLOR,
	GL_DST_COLOR,
	GL_ONE_MINUS_DST_COLOR,
	GL_SRC_ALPHA,
	GL_ONE_MINUS_SRC_ALPHA,
	GL_DST_ALPHA,
	GL_ONE_MINUS_DST_ALPHA,
	GL_SRC_ALPHA_SATURATE
};

KX_PYMETHODDEF_DOC( KX_BlenderMaterial, setBlending , "setBlending( bge.logic.src, bge.logic.dest)")
{
	unsigned int b[2];
	if (PyArg_ParseTuple(args, "ii:setBlending", &b[0], &b[1]))
	{
		bool value_found[2] = {false, false};
		for (int i=0; i<11; i++)
		{
			if (b[0] == GL_array[i]) {
				value_found[0] = true;
				mBlendFunc[0] = b[0];
			}
			if (b[1] == GL_array[i]) {
				value_found[1] = true;
				mBlendFunc[1] = b[1];
			}
			if (value_found[0] && value_found[1]) break;
		}
		if (!value_found[0] || !value_found[1]) {
			PyErr_SetString(PyExc_ValueError, "material.setBlending(int, int): KX_BlenderMaterial, invalid enum.");
			return NULL;
		}
		mUserDefBlend = true;
		Py_RETURN_NONE;
	}
	return NULL;
}

KX_PYMETHODDEF_DOC(KX_BlenderMaterial, getTextureBindcode, "getTextureBindcode(texslot)")
{
	unsigned int texslot;
	if (!PyArg_ParseTuple(args, "i:texslot", &texslot)) {
		PyErr_SetString(PyExc_ValueError, "material.getTextureBindcode(texslot): KX_BlenderMaterial, expected an int.");
		return NULL;
	}
	Image *ima = getImage(texslot);
	if (ima) {
		unsigned int *bindcode = ima->bindcode;
		return PyLong_FromLong(*bindcode);
	}
	PyErr_SetString(PyExc_ValueError, "material.getTextureBindcode(texslot): KX_BlenderMaterial, invalid texture slot.");
	return NULL;
}

#endif // WITH_PYTHON

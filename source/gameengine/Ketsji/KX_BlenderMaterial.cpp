/** \file gameengine/Ketsji/KX_BlenderMaterial.cpp
 *  \ingroup ketsji
 */

// ------------------------------------
// ...
// ------------------------------------
#include "GL/glew.h"

#include "KX_BlenderMaterial.h"
#include "BL_Material.h"
#include "KX_Scene.h"
#include "KX_Light.h"
#include "KX_GameObject.h"
#include "KX_MeshProxy.h"

#include "MT_Vector3.h"
#include "MT_Vector4.h"
#include "MT_Matrix4x4.h"

#include "RAS_BucketManager.h"
#include "RAS_MeshObject.h"
#include "RAS_IRasterizer.h"
#include "RAS_OpenGLRasterizer/RAS_GLExtensionManager.h"

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
	GameSettings *game)
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
	mMaterial = data;
	mShader = 0;
	mBlenderShader = 0;
	mScene = scene;
	mUserDefBlend = 0;
	mModified = 0;
	mConstructed = false;
	mPass = 0;
	// --------------------------------
	// RAS_IPolyMaterial variables...
	m_flag |= RAS_BLENDERMAT;
	m_flag |= (mMaterial->IdMode>=ONETEX)? RAS_MULTITEX: 0;
	m_flag |= ((mMaterial->ras_mode & USE_LIGHT)!=0)? RAS_MULTILIGHT: 0;
	m_flag |= (mMaterial->glslmat)? RAS_BLENDERGLSL: 0;
	m_flag |= ((mMaterial->ras_mode & CAST_SHADOW)!=0)? RAS_CASTSHADOW: 0;

	// figure max
	int enabled = mMaterial->num_enabled;
	int max = BL_Texture::GetMaxUnits();
	mMaterial->num_enabled = enabled>=max?max:enabled;

	// test the sum of the various modes for equality
	// so we can ether accept or reject this material
	// as being equal, this is rather important to
	// prevent material bleeding
	for (int i=0; i<mMaterial->num_enabled; i++) {
		m_multimode	+= (mMaterial->flag[i] + mMaterial->blend_mode[i]);
	}
	m_multimode += mMaterial->IdMode+ (mMaterial->ras_mode & ~(USE_LIGHT));
}

KX_BlenderMaterial::~KX_BlenderMaterial()
{
	// cleanup work
	if (mConstructed)
		// clean only if material was actually used
		OnExit();
}

MTFace* KX_BlenderMaterial::GetMTFace(void) const 
{
	// fonts on polys
	return &mMaterial->tface;
}

unsigned int* KX_BlenderMaterial::GetMCol(void) const 
{
	// fonts on polys
	return mMaterial->rgb;
}

void KX_BlenderMaterial::GetMaterialRGBAColor(unsigned char *rgba) const
{
	if (mMaterial) {
		*rgba++ = (unsigned char) (mMaterial->matcolor[0]*255.0);
		*rgba++ = (unsigned char) (mMaterial->matcolor[1]*255.0);
		*rgba++ = (unsigned char) (mMaterial->matcolor[2]*255.0);
		*rgba++ = (unsigned char) (mMaterial->matcolor[3]*255.0);
	} else
		RAS_IPolyMaterial::GetMaterialRGBAColor(rgba);
}

Material *KX_BlenderMaterial::GetBlenderMaterial() const
{
	return mMaterial->material;
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

void KX_BlenderMaterial::OnConstruction(int layer)
{
	if (mConstructed)
		// when material are reused between objects
		return;
	
	if (mMaterial->glslmat)
		SetBlenderGLSLShader(layer);

	// for each unique material...
	int i;
	for (i=0; i<mMaterial->num_enabled; i++) {
		if ( mMaterial->mapping[i].mapping & USEENV ) {
			if (!GLEW_ARB_texture_cube_map) {
				spit("CubeMap textures not supported");
				continue;
			}
			if (!mTextures[i].InitCubeMap(i, mMaterial->cubemap[i] ) )
				spit("unable to initialize image("<<i<<") in "<< 
						 mMaterial->matname<< ", image will not be available");
		} 
		// If we're using glsl materials, the textures are handled by bf_gpu, so don't load them twice!
		// However, if we're using a custom shader, then we still need to load the textures ourselves.
		else if (!mMaterial->glslmat || mBlenderShader) {
			if ( mMaterial->img[i] ) {
				if ( ! mTextures[i].InitFromImage(i, mMaterial->img[i], (mMaterial->flag[i] &MIPMAP)!=0 ))
					spit("unable to initialize image("<<i<<") in "<< 
						mMaterial->matname<< ", image will not be available");
			}
		}
	}

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
	for (int i=0; i<mMaterial->num_enabled; i++) {
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
	for (i=0; i<mMaterial->num_enabled; i++) {
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

		mBlenderShader->SetProg(true, ras->GetTime());
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
	for (i=0; (i<mMaterial->num_enabled && i<MAXTEX); i++) {
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
		if (ELEM3(alphablend, GEMAT_SOLID, GEMAT_ALPHA, GEMAT_ALPHA_SORT) && mMaterial->alphablend != GEMAT_SOLID)
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
			1.0 );

		rasty->SetAmbient(mMaterial->amb);
	}

	if (mMaterial->material)
		rasty->SetPolygonOffset(-mMaterial->material->zoffs, 0.0);
}


void KX_BlenderMaterial::ActivateTexGen(RAS_IRasterizer *ras) const
{
	if (ras->GetDrawingMode() == RAS_IRasterizer::KX_TEXTURED) {
		ras->SetAttribNum(0);
		if (mShader && GLEW_ARB_shader_objects) {
			if (mShader->GetAttribute() == BL_Shader::SHD_TANGENT) {
				ras->SetAttrib(RAS_IRasterizer::RAS_TEXCO_DISABLE, 0);
				ras->SetAttrib(RAS_IRasterizer::RAS_TEXTANGENT, 1);
				ras->SetAttribNum(2);
			}
		}

		ras->SetTexCoordNum(mMaterial->num_enabled);

		for (int i=0; i<mMaterial->num_enabled; i++) {
			int mode = mMaterial->mapping[i].mapping;

			if (mode &USECUSTOMUV)
			{
				if (!mMaterial->mapping[i].uvCoName.IsEmpty())
					ras->SetTexCoord(RAS_IRasterizer::RAS_TEXCO_UV2, i);
				continue;
			}

			if ( mode &(USEREFL|USEOBJ))
				ras->SetTexCoord(RAS_IRasterizer::RAS_TEXCO_GEN, i);
			else if (mode &USEORCO)
				ras->SetTexCoord(RAS_IRasterizer::RAS_TEXCO_ORCO, i);
			else if (mode &USENORM)
				ras->SetTexCoord(RAS_IRasterizer::RAS_TEXCO_NORM, i);
			else if (mode &USEUV)
				ras->SetTexCoord(RAS_IRasterizer::RAS_TEXCO_UV1, i);
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
	float proj[4]= {0.f,0.f,0.f,0.f};
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
	mMaterial->speccolor[0]	= (float)(specrgb)[0];
	mMaterial->speccolor[1]	= (float)(specrgb)[1];
	mMaterial->speccolor[2]	= (float)(specrgb)[2];
	mMaterial->matcolor[0]	= (float)(rgba[0]);
	mMaterial->matcolor[1]	= (float)(rgba[1]);
	mMaterial->matcolor[2]	= (float)(rgba[2]);
	mMaterial->alpha		= (float)(alpha);
	mMaterial->hard			= (float)(hard);
	mMaterial->emit			= (float)(emit);
	mMaterial->spec_f		= (float)(spec);
	mMaterial->ref			= (float)(ref);
}

void KX_BlenderMaterial::SetBlenderGLSLShader(int layer)
{
	if (!mBlenderShader)
		mBlenderShader = new BL_BlenderShader(mScene, mMaterial->material, layer);

	if (!mBlenderShader->Ok()) {
		delete mBlenderShader;
		mBlenderShader = 0;
	}
}

#ifdef WITH_PYTHON

PyMethodDef KX_BlenderMaterial::Methods[] = 
{
	KX_PYMETHODTABLE( KX_BlenderMaterial, getShader ),
	KX_PYMETHODTABLE( KX_BlenderMaterial, getMaterialIndex ),
	KX_PYMETHODTABLE( KX_BlenderMaterial, setBlending ),
	{NULL,NULL} //Sentinel
};

PyAttributeDef KX_BlenderMaterial::Attributes[] = {
	KX_PYATTRIBUTE_RO_FUNCTION("shader", KX_BlenderMaterial, pyattr_get_shader),
	KX_PYATTRIBUTE_RO_FUNCTION("material_index", KX_BlenderMaterial, pyattr_get_materialIndex),
	KX_PYATTRIBUTE_RW_FUNCTION("blending", KX_BlenderMaterial, pyattr_get_blending, pyattr_set_blending),
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

PyObject* KX_BlenderMaterial::pyattr_get_shader(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_BlenderMaterial* self= static_cast<KX_BlenderMaterial*>(self_v);
	return self->PygetShader(NULL, NULL);
}

PyObject* KX_BlenderMaterial::pyattr_get_materialIndex(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_BlenderMaterial* self= static_cast<KX_BlenderMaterial*>(self_v);
	return PyLong_FromSsize_t(self->GetMaterialIndex());
}

PyObject* KX_BlenderMaterial::pyattr_get_blending(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef)
{
	KX_BlenderMaterial* self= static_cast<KX_BlenderMaterial*>(self_v);
	unsigned int* bfunc = self->getBlendFunc();
	return Py_BuildValue("(ll)", (long int)bfunc[0], (long int)bfunc[1]);
}

int KX_BlenderMaterial::pyattr_set_blending(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value)
{
	KX_BlenderMaterial* self= static_cast<KX_BlenderMaterial*>(self_v);
	PyObject* obj = self->PysetBlending(value, NULL);
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

	if (!GLEW_ARB_shader_objects)  {
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
		}

		if (mShader && !mShader->GetError()) {
			m_flag &= ~RAS_BLENDERGLSL;
			mMaterial->SetSharedMaterial(true);
			mScene->GetBucketManager()->ReleaseDisplayLists(this);
			return mShader->GetProxy();
		}else
		{
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
	return PyLong_FromSsize_t( GetMaterialIndex() );
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

static unsigned int GL_array[11] = {
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

#endif // WITH_PYTHON

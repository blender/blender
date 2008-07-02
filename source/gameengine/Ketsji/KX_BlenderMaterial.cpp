
// ------------------------------------
// ...
// ------------------------------------
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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

#include "RAS_MeshObject.h"
#include "RAS_IRasterizer.h"
#include "RAS_OpenGLRasterizer/RAS_GLExtensionManager.h"

extern "C" {
#include "BDR_drawmesh.h"
}

#include "STR_HashedString.h"

// ------------------------------------
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_image_types.h"
#include "DNA_meshdata_types.h"
#include "BKE_mesh.h"
// ------------------------------------
#define spit(x) std::cout << x << std::endl;

//static PyObject *gTextureDict = 0;

KX_BlenderMaterial::KX_BlenderMaterial(
    KX_Scene *scene,
	BL_Material *data,
	bool skin,
	int lightlayer,
	void *clientobject,
	PyTypeObject *T
	)
:	PyObjectPlus(T),
	RAS_IPolyMaterial(
		STR_String( data->texname[0] ),
		STR_String( data->matname ), // needed for physics!
		data->tile,
		data->tilexrep[0],
		data->tileyrep[0],
		data->mode,
		((data->ras_mode &TRANSP)!=0),
		((data->ras_mode &ZSORT)!=0),
		lightlayer,
		((data->ras_mode &TRIANGLE)!=0),
		clientobject
	),
	mMaterial(data),
	mShader(0),
	mBlenderShader(0),
	mScene(scene),
	mUserDefBlend(0),
	mModified(0),
	mConstructed(false),
	mPass(0)

{
	// --------------------------------
	// RAS_IPolyMaterial variables... 
	m_flag |=RAS_BLENDERMAT;
	m_flag |=(mMaterial->IdMode>=ONETEX)?RAS_MULTITEX:0;
	m_flag |=(mMaterial->ras_mode & USE_LIGHT)!=0?RAS_MULTILIGHT:0;
	m_flag |=(mMaterial->ras_mode &ALPHA_TEST)!=0?RAS_FORCEALPHA:0;

	// figure max
	int enabled = mMaterial->num_enabled;
	int max = BL_Texture::GetMaxUnits();
	mMaterial->num_enabled = enabled>=max?max:enabled;

	// test the sum of the various modes for equality
	// so we can ether accept or reject this material 
	// as being equal, this is rather important to 
	// prevent material bleeding
	for(int i=0; i<mMaterial->num_enabled; i++) {
		m_multimode	+=
			( mMaterial->flag[i]	+
			  mMaterial->blend_mode[i]
			 );
	}
	m_multimode += mMaterial->IdMode+mMaterial->ras_mode;

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
	MT_assert(mMaterial->tface);
	return mMaterial->tface;
}

unsigned int* KX_BlenderMaterial::GetMCol(void) const 
{
	// fonts on polys
	return mMaterial->rgb;
}

void KX_BlenderMaterial::OnConstruction()
{
	if (mConstructed)
		// when material are reused between objects
		return;
	
	if(mMaterial->glslmat) {
		SetBlenderGLSLShader();
	}
	else {
		// for each unique material...
		int i;
		for(i=0; i<mMaterial->num_enabled; i++) {
			if( mMaterial->mapping[i].mapping & USEENV ) {
				if(!GLEW_ARB_texture_cube_map) {
					spit("CubeMap textures not supported");
					continue;
				}
				if(!mTextures[i].InitCubeMap(i, mMaterial->cubemap[i] ) )
					spit("unable to initialize image("<<i<<") in "<< 
							 mMaterial->matname<< ", image will not be available");
			} 
		
			else {
				if( mMaterial->img[i] ) {
					if( ! mTextures[i].InitFromImage(i, mMaterial->img[i], (mMaterial->flag[i] &MIPMAP)!=0 ))
						spit("unable to initialize image("<<i<<") in "<< 
							mMaterial->matname<< ", image will not be available");
				}
			}
		}
	}
	mBlendFunc[0] =0;
	mBlendFunc[1] =0;
	mConstructed = true;
}

void KX_BlenderMaterial::OnExit()
{
	if( mShader ) {
		 //note, the shader here is allocated, per unique material
		 //and this function is called per face
		mShader->SetProg(false);
		delete mShader;
		mShader = 0;
	}

	if( mBlenderShader ) {
		mBlenderShader->SetProg(false);
		delete mBlenderShader;
		mBlenderShader = 0;
	}

	BL_Texture::ActivateFirst();
	for(int i=0; i<mMaterial->num_enabled; i++) {
		BL_Texture::ActivateUnit(i);
		mTextures[i].DeleteTex();
		mTextures[i].DisableUnit();
	}

	if( mMaterial->tface ) 
		set_tpage(mMaterial->tface);
}


void KX_BlenderMaterial::setShaderData( bool enable, RAS_IRasterizer *ras)
{
	MT_assert(GLEW_ARB_shader_objects && mShader);

	int i;
	if( !enable || !mShader->Ok() ) {
		// frame cleanup.
		mShader->SetProg(false);
		BL_Texture::DisableAllTextures();
		return;
	}

	BL_Texture::DisableAllTextures();
	mShader->SetProg(true);
	
	BL_Texture::ActivateFirst();

	mShader->ApplyShader();

	// for each enabled unit
	for(i=0; i<mMaterial->num_enabled; i++) {
		if(!mTextures[i].Ok()) continue;
		mTextures[i].ActivateTexture();
		mTextures[0].SetMapping(mMaterial->mapping[i].mapping);
	}

	if(!mUserDefBlend) {
		setDefaultBlending();
	}
	else {
		// tested to be valid enums
		glEnable(GL_BLEND);
		glBlendFunc(mBlendFunc[0], mBlendFunc[1]);
	}
}

void KX_BlenderMaterial::setBlenderShaderData( bool enable, RAS_IRasterizer *ras)
{
	if( !enable || !mBlenderShader->Ok() ) {
		// frame cleanup.
		mBlenderShader->SetProg(false);
		BL_Texture::DisableAllTextures();
		return;
	}

	BL_Texture::DisableAllTextures();
	mBlenderShader->SetProg(true);
	mBlenderShader->ApplyShader();
}

void KX_BlenderMaterial::setTexData( bool enable, RAS_IRasterizer *ras)
{
	if(GLEW_ARB_shader_objects && mShader) 
		mShader->SetProg(false);

	BL_Texture::DisableAllTextures();
	if( !enable )
		return;

	BL_Texture::ActivateFirst();

	if( mMaterial->IdMode == DEFAULT_BLENDER ) {
		setDefaultBlending();
		return;
	}

	if( mMaterial->IdMode == TEXFACE ) {
		// no material connected to the object
		if( mTextures[0].Ok() ) {
			mTextures[0].ActivateTexture();
			mTextures[0].setTexEnv(0, true);
			mTextures[0].SetMapping(mMaterial->mapping[0].mapping);
			setDefaultBlending(); 
		}
		return;
	}

	int mode = 0,i=0;
	for(i=0; (i<mMaterial->num_enabled && i<MAXTEX); i++) {
		if( !mTextures[i].Ok() ) continue;

		mTextures[i].ActivateTexture();
		mTextures[i].setTexEnv(mMaterial);
		mode = mMaterial->mapping[i].mapping;

		if(mode &USEOBJ)
			setObjectMatrixData(i, ras);
		else
			mTextures[i].SetMapping(mode);
		
		if(!(mode &USEOBJ))
			setTexMatrixData( i );
	}

	if(!mUserDefBlend) {
		setDefaultBlending();
	}
	else {
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
	if(tmp->mMaterial->IsShared()) 
		cachingInfo =0;
	
	if (GetCachingInfo() != cachingInfo) {

		if (!cachingInfo)
			tmp->setShaderData( false, rasty);
		
		cachingInfo = GetCachingInfo();
	
		if(rasty->GetDrawingMode() == RAS_IRasterizer::KX_TEXTURED)
			tmp->setShaderData( true, rasty);
		else
			tmp->setShaderData( false, rasty);

		if(mMaterial->mode & RAS_IRasterizer::KX_TWOSIDE)
			rasty->SetCullFace(false);
		else
			rasty->SetCullFace(true);

		if (((mMaterial->ras_mode &WIRE)!=0) || mMaterial->mode & RAS_IRasterizer::KX_LINES)
		{		
			if((mMaterial->ras_mode &WIRE)!=0) 
				rasty->SetCullFace(false);
			rasty->SetLines(true);
		}
		else
			rasty->SetLines(false);
	}

	ActivatGLMaterials(rasty);
	ActivateTexGen(rasty);
}

void
KX_BlenderMaterial::ActivateBlenderShaders(
	RAS_IRasterizer* rasty, 
	TCachingInfo& cachingInfo)const
{
	KX_BlenderMaterial *tmp = const_cast<KX_BlenderMaterial*>(this);

	// reset... 
	if(tmp->mMaterial->IsShared()) 
		cachingInfo =0;
	
	if (GetCachingInfo() != cachingInfo) {
		if (!cachingInfo)
			tmp->setBlenderShaderData(false, rasty);
		
		cachingInfo = GetCachingInfo();
	
		if(rasty->GetDrawingMode() == RAS_IRasterizer::KX_TEXTURED) {
			tmp->setBlenderShaderData(true, rasty);
			rasty->EnableTextures(true);
		}
		else {
			tmp->setBlenderShaderData(false, rasty);
			rasty->EnableTextures(false);
		}

		if(mMaterial->mode & RAS_IRasterizer::KX_TWOSIDE)
			rasty->SetCullFace(false);
		else
			rasty->SetCullFace(true);

		if (((mMaterial->ras_mode &WIRE)!=0) || mMaterial->mode & RAS_IRasterizer::KX_LINES)
		{		
			if((mMaterial->ras_mode &WIRE)!=0) 
				rasty->SetCullFace(false);
			rasty->SetLines(true);
		}
		else
			rasty->SetLines(false);
	}

	ActivatGLMaterials(rasty);
	mBlenderShader->SetTexCoords(rasty);
}

void
KX_BlenderMaterial::ActivateMat( 
	RAS_IRasterizer* rasty,  
	TCachingInfo& cachingInfo
	)const
{
	KX_BlenderMaterial *tmp = const_cast<KX_BlenderMaterial*>(this);
	if (GetCachingInfo() != cachingInfo) {
		if (!cachingInfo) 
			tmp->setTexData( false,rasty );
		
		cachingInfo = GetCachingInfo();

		if (rasty->GetDrawingMode() == RAS_IRasterizer::KX_TEXTURED)
			tmp->setTexData( true,rasty  );
		else
			tmp->setTexData( false,rasty);

		if(mMaterial->mode & RAS_IRasterizer::KX_TWOSIDE)
			rasty->SetCullFace(false);
		else
			rasty->SetCullFace(true);

		if (((mMaterial->ras_mode &WIRE)!=0) || mMaterial->mode & RAS_IRasterizer::KX_LINES)
		{		
			if((mMaterial->ras_mode &WIRE)!=0) 
				rasty->SetCullFace(false);
			rasty->SetLines(true);
		}
		else
			rasty->SetLines(false);
	}

	ActivatGLMaterials(rasty);
	ActivateTexGen(rasty);
}

bool 
KX_BlenderMaterial::Activate( 
	RAS_IRasterizer* rasty,  
	TCachingInfo& cachingInfo
	)const
{
	bool dopass = false;
	if( GLEW_ARB_shader_objects && ( mShader && mShader->Ok() ) ) {
		if( (mPass++) < mShader->getNumPass() ) {
			ActivatShaders(rasty, cachingInfo);
			dopass = true;
			return dopass;
		}
		else {
			mShader->SetProg(false);
			mPass = 0;
			dopass = false;
			return dopass;
		}
	}
	else if( GLEW_ARB_shader_objects && ( mBlenderShader && mBlenderShader->Ok() ) ) {
		if( (mPass++) == 0 ) {
			ActivateBlenderShaders(rasty, cachingInfo);
			dopass = true;
			return dopass;
		}
		else {
			mPass = 0;
			dopass = false;
			return dopass;
		}
	}
	else {
		switch (mPass++)
		{
			case 0:
				ActivateMat(rasty, cachingInfo);
				dopass = true;
				break;
			default:
				mPass = 0;
				dopass = false;
				break;
		}
	}
	return dopass;
}

void KX_BlenderMaterial::ActivateMeshSlot(const KX_MeshSlot & ms, RAS_IRasterizer* rasty) const
{
	if(mShader && GLEW_ARB_shader_objects)
		mShader->Update(ms, rasty);
	if(mBlenderShader && GLEW_ARB_shader_objects)
		mBlenderShader->Update(ms, rasty);
}

void KX_BlenderMaterial::ActivatGLMaterials( RAS_IRasterizer* rasty )const
{
	if(!mBlenderShader) {
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
	if(ras->GetDrawingMode() == RAS_IRasterizer::KX_TEXTURED) {
		ras->SetAttribNum(0);
		if(mShader && GLEW_ARB_shader_objects) {
			if(mShader->GetAttribute() == BL_Shader::SHD_TANGENT) {
				ras->SetAttrib(RAS_IRasterizer::RAS_TEXTANGENT, 1);
				ras->SetAttribNum(2);
			}
		}

		ras->SetTexCoordNum(mMaterial->num_enabled);

		for(int i=0; i<mMaterial->num_enabled; i++) {
			int mode = mMaterial->mapping[i].mapping;

			if (mode &USECUSTOMUV)
			{
				STR_String str = mMaterial->mapping[i].uvCoName;
				if (!str.IsEmpty())
					ras->SetTexCoord(RAS_IRasterizer::RAS_TEXCO_UV2, i);
				continue;
			}

			if( mode &(USEREFL|USEOBJ))
				ras->SetTexCoord(RAS_IRasterizer::RAS_TEXCO_GEN, i);
			else if(mode &USEORCO)
				ras->SetTexCoord(RAS_IRasterizer::RAS_TEXCO_ORCO, i);
			else if(mode &USENORM)
				ras->SetTexCoord(RAS_IRasterizer::RAS_TEXCO_NORM, i);
			else if(mode &USEUV)
				ras->SetTexCoord(RAS_IRasterizer::RAS_TEXCO_UV1, i);
			else if(mode &USETANG)
				ras->SetTexCoord(RAS_IRasterizer::RAS_TEXTANGENT, i);
			else 
				ras->SetTexCoord(RAS_IRasterizer::RAS_TEXCO_DISABLE, i);
		}

		ras->EnableTextures(true);
	}
	else
		ras->EnableTextures(false);
}

bool KX_BlenderMaterial::setDefaultBlending()
{
	if( mMaterial->transp &TF_ADD) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);
		glDisable ( GL_ALPHA_TEST );
		return true;
	}
	
	if( mMaterial->transp & TF_ALPHA ) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glDisable ( GL_ALPHA_TEST );
		return true;
	}
	
	if( mMaterial->transp & TF_CLIP ) {
		glDisable(GL_BLEND); 
		glEnable ( GL_ALPHA_TEST );
		glAlphaFunc(GL_GREATER, 0.5f);
		return false;
	}
	return false;
}

void KX_BlenderMaterial::setTexMatrixData(int i)
{
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();

	if( GLEW_ARB_texture_cube_map && 
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
	if( mat->mapping[index].projplane[num] == PROJX )
		param[0] = 1.f;
	else if( mat->mapping[index].projplane[num] == PROJY )
		param[1] = 1.f;
	else if( mat->mapping[index].projplane[num] == PROJZ)
		param[2] = 1.f;
}

void KX_BlenderMaterial::setObjectMatrixData(int i, RAS_IRasterizer *ras)
{
	KX_GameObject *obj = 
		(KX_GameObject*)
		mScene->GetObjectList()->FindValue(mMaterial->mapping[i].objconame);

	if(!obj) return;

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

	MT_Matrix4x4 mvmat;
	ras->GetViewMatrix(mvmat);

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
}


PyMethodDef KX_BlenderMaterial::Methods[] = 
{
	KX_PYMETHODTABLE( KX_BlenderMaterial, getShader ),
	KX_PYMETHODTABLE( KX_BlenderMaterial, getMaterialIndex ),
	KX_PYMETHODTABLE( KX_BlenderMaterial, setBlending ),
	{NULL,NULL} //Sentinel
};


PyTypeObject KX_BlenderMaterial::Type = {
	PyObject_HEAD_INIT(&PyType_Type)
		0,
		"KX_BlenderMaterial",
		sizeof(KX_BlenderMaterial),
		0,
		PyDestructor,
		0,
		__getattr,
		__setattr,
		0,
		__repr,
		0
};


PyParentObject KX_BlenderMaterial::Parents[] = {
	&PyObjectPlus::Type,
	&KX_BlenderMaterial::Type,
	NULL
};


PyObject* KX_BlenderMaterial::_getattr(const STR_String& attr)
{
	_getattr_up(PyObjectPlus);
}

int KX_BlenderMaterial::_setattr(const STR_String& attr, PyObject *pyvalue)
{
	return PyObjectPlus::_setattr(attr, pyvalue);
}


KX_PYMETHODDEF_DOC( KX_BlenderMaterial, getShader , "getShader()")
{
	if( !GLEW_ARB_fragment_shader) {
		if(!mModified)
			spit("Fragment shaders not supported");
	
		mModified = true;
		Py_Return;
	}

	if( !GLEW_ARB_vertex_shader) {
		if(!mModified)
			spit("Vertex shaders not supported");

		mModified = true;
		Py_Return;
	}

	if(!GLEW_ARB_shader_objects)  {
		if(!mModified)
			spit("GLSL not supported");
		mModified = true;
		Py_Return;
	}
	else {
		// returns Py_None on error
		// the calling script will need to check

		if(!mShader && !mModified) {
			mShader = new BL_Shader();
			mModified = true;
		}

		if(mShader && !mShader->GetError()) {
			mMaterial->SetSharedMaterial(true);
			Py_INCREF(mShader);
			return mShader;
		}else
		{
			// decref all references to the object
			// then delete it!
			// We will then go back to fixed functionality
			// for this material
			if(mShader) {
				if(mShader->ob_refcnt > 1) {
					Py_DECREF(mShader);
				}
				else {
					delete mShader;
					mShader=0;
				}
			}
		}
		Py_Return;
	}
	PyErr_Format(PyExc_ValueError, "GLSL Error");
	return NULL;
}


void KX_BlenderMaterial::SetBlenderGLSLShader(void)
{
	if(!mBlenderShader)
		mBlenderShader = new BL_BlenderShader(mMaterial->material);

	if(!mBlenderShader->Ok()) {
		delete mBlenderShader;
		mBlenderShader = 0;
	}
}

KX_PYMETHODDEF_DOC( KX_BlenderMaterial, getMaterialIndex, "getMaterialIndex()")
{
	return PyInt_FromLong( mMaterial->material_index );
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

KX_PYMETHODDEF_DOC( KX_BlenderMaterial, setBlending , "setBlending( GameLogic.src, GameLogic.dest)")
{
	unsigned int b[2];
	if(PyArg_ParseTuple(args, "ii", &b[0], &b[1]))
	{
		bool value_found[2] = {false, false};
		for(int i=0; i<11; i++)
		{
			if(b[0] == GL_array[i]) {
				value_found[0] = true;
				mBlendFunc[0] = b[0];
			}
			if(b[1] == GL_array[i]) {
				value_found[1] = true;
				mBlendFunc[1] = b[1];
			}
			if(value_found[0] && value_found[1]) break;
		}
		if(!value_found[0] || !value_found[1]) {
			PyErr_Format(PyExc_ValueError, "invalid enum.");
			return NULL;
		}
		mUserDefBlend = true;
		Py_Return;
	}
	return NULL;
}



// ------------------------------------
// ...
// ------------------------------------
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include <windows.h>
#endif // WIN32
#ifdef __APPLE__
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include "KX_BlenderMaterial.h"
#include "BL_Material.h"
#include "KX_Scene.h"
#include "KX_Light.h"
#include "KX_GameObject.h"

#include "MT_Vector3.h"
#include "MT_Vector4.h"
#include "MT_Matrix4x4.h"

#include "RAS_MeshObject.h"
#include "RAS_IRasterizer.h"
#include "RAS_OpenGLRasterizer/RAS_GLExtensionManager.h"
#include "RAS_OpenGLRasterizer/ARB_multitexture.h"

extern "C" {
#include "BDR_drawmesh.h"
}

#include "STR_HashedString.h"

// ------------------------------------
#include "DNA_object_types.h"
#include "DNA_material_types.h"
#include "DNA_image_types.h"
#include "DNA_mesh_types.h"
#include "BKE_mesh.h"
// ------------------------------------
using namespace bgl;
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
	mScene(scene),
	mPass(0)
{
	///RAS_EXT_support._ARB_multitexture == true if were here

	// --------------------------------
	// RAS_IPolyMaterial variables... 
	m_flag |=RAS_BLENDERMAT;
	m_flag |=(mMaterial->IdMode>=ONETEX)?RAS_MULTITEX:0;
	m_flag |=(mMaterial->ras_mode & USE_LIGHT)!=0?RAS_MULTILIGHT:0;
	
	// figure max
	#ifdef GL_ARB_multitexture
	int enabled = mMaterial->num_enabled;
	mMaterial->num_enabled = enabled>=bgl::max_texture_units?bgl::max_texture_units:enabled;
	#else
	mMaterial->num_enabled=0;
	#endif

	m_enabled = mMaterial->num_enabled;

	// test the sum of the various modes for equality
	// so we can ether accept or reject this material 
	// as being equal, this is rather important to 
	// prevent material bleeding
	for(int i=0; i<mMaterial->num_enabled; i++) {
		m_multimode	+=
			(mMaterial->flag[i]	+
			 mMaterial->blend_mode[i]
			 );
	}
	m_multimode += mMaterial->IdMode+mMaterial->ras_mode;

}


KX_BlenderMaterial::~KX_BlenderMaterial()
{
	// cleanup work
	OnExit();
}


TFace* KX_BlenderMaterial::GetTFace(void) const 
{
	// fonts on polys
	MT_assert(mMaterial->tface);
	return mMaterial->tface;
}

void KX_BlenderMaterial::OnConstruction()
{
	// for each unique material...
	#ifdef GL_ARB_multitexture
/*	will be used to switch textures
	if(!gTextureDict)
		gTextureDict = PyDict_New();
*/
	#ifdef GL_ARB_shader_objects
	if( RAS_EXT_support._ARB_shader_objects )
		mShader = new BL_Shader( mMaterial->num_enabled );
	#endif

	int i;
	for(i=0; i<mMaterial->num_enabled; i++) {
	glActiveTextureARB(GL_TEXTURE0_ARB+i);
		#ifdef GL_ARB_texture_cube_map
		if( mMaterial->mapping[i].mapping & USEENV ) {
			if(!RAS_EXT_support._ARB_texture_cube_map) {
				spit("CubeMap textures not supported");
				continue;
			}
			if(!mTextures[i].InitCubeMap( mMaterial->cubemap[i] ) )
				spit("unable to initialize image("<<i<<") in "<< 
						mMaterial->matname<< ", image will not be available");

			if( RAS_EXT_support._ARB_shader_objects )
				mShader->InitializeSampler(SAMP_CUBE, i, 0, mTextures[i]);
		} 
	
		else {
		#endif//GL_ARB_texture_cube_map
			if( mMaterial->img[i] ) {
				if( ! mTextures[i].InitFromImage(mMaterial->img[i], (mMaterial->flag[i] &MIPMAP)!=0 ))
					spit("unable to initialize image("<<i<<") in "<< 
						 mMaterial->matname<< ", image will not be available");
			
				if( RAS_EXT_support._ARB_shader_objects )
					mShader->InitializeSampler(SAMP_2D, i, 0, mTextures[i]);
			}
		#ifdef GL_ARB_texture_cube_map
		}
		#endif//GL_ARB_texture_cube_map
		/*PyDict_SetItemString(gTextureDict, mTextures[i].GetName().Ptr(), PyInt_FromLong(mTextures[i]));*/
	}
	#endif//GL_ARB_multitexture
}

void KX_BlenderMaterial::OnExit()
{
	#ifdef GL_ARB_multitexture

	#ifdef GL_ARB_shader_objects
	if( RAS_EXT_support._ARB_shader_objects && mShader ) {
		 //note, the shader here is allocated, per unique material
		 //and this function is called per face
		glUseProgramObjectARB(0);
		delete mShader;
		mShader = 0;
	}
	#endif //GL_ARB_shader_objects

	for(int i=0; i<mMaterial->num_enabled; i++) {
		glActiveTextureARB(GL_TEXTURE0_ARB+i);

		mTextures[i].DeleteTex();

		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);

		#ifdef GL_ARB_texture_cube_map
		if(RAS_EXT_support._ARB_texture_cube_map)
			glDisable(GL_TEXTURE_CUBE_MAP_ARB);
		#endif//GL_ARB_texture_cube_map

		glDisable(GL_TEXTURE_2D);
		glDisable(GL_TEXTURE_GEN_S);
		glDisable(GL_TEXTURE_GEN_T);
		glDisable(GL_TEXTURE_GEN_R);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	}
	
	/*if (gTextureDict) {
		PyDict_Clear(gTextureDict);
		Py_DECREF(gTextureDict);
		gTextureDict = 0;
	}*/

	glActiveTextureARB(GL_TEXTURE0_ARB);

	#ifdef GL_ARB_texture_cube_map
	if(RAS_EXT_support._ARB_texture_cube_map)
		glDisable(GL_TEXTURE_CUBE_MAP_ARB);
	#endif//GL_ARB_texture_cube_map

	glDisable(GL_TEXTURE_2D);

	#endif//GL_ARB_multitexture

	// make sure multi texture units 
	// revert back to blender...
	// --
	if( mMaterial->tface ) 
		set_tpage(mMaterial->tface);
}


void KX_BlenderMaterial::DisableTexData()
{
	glDisable(GL_BLEND);
	#ifdef GL_ARB_multitexture
	int i=(MAXTEX>=bgl::max_texture_units?bgl::max_texture_units:MAXTEX)-1;
	for(; i>=0; i--) {
		glActiveTextureARB(GL_TEXTURE0_ARB+i);
		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);

		#ifdef GL_ARB_texture_cube_map
		if(RAS_EXT_support._ARB_texture_cube_map)
			glDisable(GL_TEXTURE_CUBE_MAP_ARB);
		#endif//GL_ARB_texture_cube_map

		glDisable(GL_TEXTURE_2D);	
		glDisable(GL_TEXTURE_GEN_S);
		glDisable(GL_TEXTURE_GEN_T);
		glDisable(GL_TEXTURE_GEN_R);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	}
	#endif//GL_ARB_multitexture
}


void KX_BlenderMaterial::setShaderData( bool enable )
{
	#ifdef GL_ARB_multitexture 
	#ifdef GL_ARB_shader_objects 

	MT_assert(RAS_EXT_support._ARB_shader_objects && mShader);

	int i;
	if( !enable || !mShader->Ok() ) {
		// frame cleanup.
		glUseProgramObjectARB( 0 );
		DisableTexData();
		return;
	}

	DisableTexData();
	glUseProgramObjectARB( mShader->GetProg() );
	
	// for each enabled unit
	for(i=0; i<mMaterial->num_enabled; i++) {

		const uSampler *samp = mShader->getSampler(i);
		if( samp->loc == -1 || samp->glTexture == 0 ) continue;

		glActiveTextureARB(GL_TEXTURE0_ARB+i);

		#ifdef GL_ARB_texture_cube_map
		if( mMaterial->mapping[i].mapping &USEENV ) {
			glBindTexture( GL_TEXTURE_CUBE_MAP_ARB, samp->glTexture	/* mTextures[i]*/ );	
			glEnable( GL_TEXTURE_CUBE_MAP_ARB );
		} 
		else {
		#endif//GL_ARB_texture_cube_map
			glBindTexture( GL_TEXTURE_2D, samp->glTexture	/*mTextures[i]*/ );	
			glEnable( GL_TEXTURE_2D );
		#ifdef GL_ARB_texture_cube_map
		}
		#endif//GL_ARB_texture_cube_map
		// use a sampler
		glUniform1iARB(samp->loc, i );
	}
	glDisable(GL_BLEND);

	#endif//GL_ARB_shader_objects
	#endif//GL_ARB_multitexture
}


void KX_BlenderMaterial::setTexData( bool enable )
{
	#ifdef GL_ARB_multitexture
	int i;

	#ifdef GL_ARB_shader_objects
	if(RAS_EXT_support._ARB_shader_objects) {
		// switch back to fixed func
		glUseProgramObjectARB( 0 );
	}
	#endif//GL_ARB_shader_objects

	if( !enable ) {
		 // frame cleanup.
		DisableTexData();
		return;
	}
	
	DisableTexData();

	if( mMaterial->IdMode == DEFAULT_BLENDER ) {
		setDefaultBlending();
		return;
	}

	if( mMaterial->IdMode == TEXFACE ) {

		// no material connected to the object
		if( mTextures[0] ) {
			if( !mTextures[0].Ok() ) return;
			glActiveTextureARB(GL_TEXTURE0_ARB);
			glBindTexture( GL_TEXTURE_2D, mTextures[0] );	
			glEnable(GL_TEXTURE_2D);
			setTextureEnvironment( -1 ); // modulate
			setEnvMap( (mMaterial->mapping[0].mapping &USEREFL)!=0 );
			setDefaultBlending(); 
		}
		return;
	}

	int lastblend = 0;

	// for each enabled unit
	for(i=0; (i<mMaterial->num_enabled); i++) {
		if( !mTextures[i].Ok() ) continue;

		glActiveTextureARB(GL_TEXTURE0_ARB+i);

		#ifdef GL_ARB_texture_cube_map
		// use environment maps
		if( mMaterial->mapping[i].mapping &USEENV && RAS_EXT_support._ARB_texture_cube_map ) {
			glBindTexture( GL_TEXTURE_CUBE_MAP_ARB, mTextures[i] );	
			glEnable(GL_TEXTURE_CUBE_MAP_ARB);
			setTextureEnvironment( i );

			if( mMaterial->mapping[i].mapping &USEREFL )
				setEnvMap( true, true );
			else if(mMaterial->mapping[i].mapping &USEOBJ)
				setObjectMatrixData(i);
			else
				setTexMatrixData( i );
		} 
		// 2d textures
		else { 
		#endif//GL_ARB_texture_cube_map
			glBindTexture( GL_TEXTURE_2D, mTextures[i] );	
			glEnable( GL_TEXTURE_2D );
			setTextureEnvironment( i );
			
			if( mMaterial->mapping[i].mapping &USEREFL ){
				setEnvMap( true );
			}
			else if(mMaterial->mapping[i].mapping &USEOBJ){
				setObjectMatrixData(i);
			}
			else {
				setTexMatrixData( i );
			}

		#ifdef GL_ARB_texture_cube_map
		}
		#endif//GL_ARB_texture_cube_map

		// if either unit has set blending
		// and its the last pass
		lastblend += setBlending( i ); // dry run
		if(lastblend >0 && i==mMaterial->num_enabled-1)
			setBlending( i, true );
		else if(lastblend == 0 && i==mMaterial->num_enabled-1)
			glDisable(GL_BLEND);
	}
	#endif//GL_ARB_multitexture
}

void
KX_BlenderMaterial::ActivatShaders(
	RAS_IRasterizer* rasty, 
	TCachingInfo& cachingInfo)const
{
	if (GetCachingInfo() != cachingInfo) {
		KX_BlenderMaterial *tmp = const_cast<KX_BlenderMaterial*>(this);

		if (!cachingInfo)
			tmp->setShaderData( false );
		
		cachingInfo = GetCachingInfo();
	
		if (rasty->GetDrawingMode() == RAS_IRasterizer::KX_TEXTURED ) {
			tmp->setShaderData( true );
			rasty->EnableTextures(true);
		}
		else {
			tmp->setShaderData( false );
			rasty->EnableTextures(false);
		}

		if(mMaterial->mode & RAS_IRasterizer::KX_TWOSIDE)
			rasty->SetCullFace(false);
		else
			rasty->SetCullFace(true);

		if (((mMaterial->ras_mode &WIRE)!=0) || mMaterial->mode & RAS_IRasterizer::KX_LINES)
			rasty->SetLines(true);
		else
			rasty->SetLines(false);
	}
	
	// shaders have access to the variables set here
	// via builtin GLSL variables
	// eg: gl_FrontMaterial.diffuse
	// --
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
		1.0
		);

	// Lagan's patch...
	// added material factor
	rasty->SetAmbient(mMaterial->amb);

	if (mMaterial->material)
		rasty->SetPolygonOffset(-mMaterial->material->zoffs, 0.0);
}

void
KX_BlenderMaterial::ActivateMat( 
	RAS_IRasterizer* rasty,  
	TCachingInfo& cachingInfo
	)const
{
	if (GetCachingInfo() != cachingInfo) {
		KX_BlenderMaterial *tmp = const_cast<KX_BlenderMaterial*>(this);

		if (!cachingInfo) 
			tmp->setTexData( false );
		
		cachingInfo = GetCachingInfo();

		if (rasty->GetDrawingMode() == RAS_IRasterizer::KX_TEXTURED) {
			tmp->setTexData( true );
			rasty->EnableTextures(true);
		}
		else{
			tmp->setTexData( false );
			rasty->EnableTextures(false);
		}

		if(mMaterial->mode & RAS_IRasterizer::KX_TWOSIDE)
			rasty->SetCullFace(false);
		else
			rasty->SetCullFace(true);

		if (((mMaterial->ras_mode &WIRE)!=0) || mMaterial->mode & RAS_IRasterizer::KX_LINES)
			rasty->SetLines(true);
		else
			rasty->SetLines(false);
	}
		
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
		1.0
		);
	
	// Lagan's patch...
	// added material factor
	rasty->SetAmbient(mMaterial->amb);

	if (mMaterial->material)
		rasty->SetPolygonOffset(-mMaterial->material->zoffs, 0.0);
}

bool 
KX_BlenderMaterial::Activate( 
	RAS_IRasterizer* rasty,  
	TCachingInfo& cachingInfo
	)const
{
	bool dopass = false;
	#ifdef GL_ARB_shader_objects
	if( RAS_EXT_support._ARB_shader_objects &&
		( mShader && mShader->Ok() ) ) {

		if( (mPass++) < mShader->getNumPass() ) {
			ActivatShaders(rasty, cachingInfo);
			dopass = true;
			return dopass;
		}
		else {
			glUseProgramObjectARB( 0 );
			mPass = 0;
			dopass = false;
			return dopass;
		}
	}
	else {
	#endif//GL_ARB_shader_objects
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
	#ifdef GL_ARB_shader_objects
	}
	#endif//GL_ARB_shader_objects
	return dopass;
}

void KX_BlenderMaterial::setTextureEnvironment( int textureIndex )
{
#ifndef GL_ARB_texture_env_combine
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	return;
#else
	if(textureIndex == -1 || !RAS_EXT_support._ARB_texture_env_combine){
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		return;
	}

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB );

	GLfloat blend_operand		= GL_SRC_COLOR;
	GLfloat blend_operand_prev	= GL_SRC_COLOR;

	// all sources here are RGB by default
	GLenum combiner = GL_COMBINE_RGB_ARB;
	GLenum source0 = GL_SOURCE0_RGB_ARB;
	GLenum source1 = GL_SOURCE1_RGB_ARB;
	GLenum source2 = GL_SOURCE2_RGB_ARB;
	GLenum op0 = GL_OPERAND0_RGB_ARB;
	GLenum op1 = GL_OPERAND1_RGB_ARB;
	GLenum op2 = GL_OPERAND2_RGB_ARB;

	// switch to alpha combiners
	if( (mMaterial->flag[textureIndex] &TEXALPHA) ) {
		combiner = GL_COMBINE_ALPHA_ARB;
		source0	 = GL_SOURCE0_ALPHA_ARB;
		source1 = GL_SOURCE1_ALPHA_ARB;
		source2 = GL_SOURCE2_ALPHA_ARB;
		op0 = GL_OPERAND0_ALPHA_ARB;
		op1 = GL_OPERAND1_ALPHA_ARB;
		op2 = GL_OPERAND2_ALPHA_ARB;
		blend_operand = GL_SRC_ALPHA;
		blend_operand_prev = GL_SRC_ALPHA;
		
		// invert
		if(mMaterial->flag[textureIndex] &TEXNEG) {
			blend_operand_prev = GL_ONE_MINUS_SRC_ALPHA;
			blend_operand = GL_ONE_MINUS_SRC_ALPHA;
		}
	}
	else {
		if(mMaterial->flag[textureIndex] &TEXNEG) {
			blend_operand_prev = GL_ONE_MINUS_SRC_COLOR;
			blend_operand = GL_ONE_MINUS_SRC_COLOR;
		}
	}
	// on Texture0 GL_PREVIOUS_ARB is the primary color
	// on Texture1 GL_PREVIOUS_ARB is Texture0 env
	switch( mMaterial->blend_mode[textureIndex] ) {
		case BLEND_MIX:
			{
				// ------------------------------
				GLfloat base_col[4];
				base_col[0]	 = base_col[1]  = base_col[2]  = 0.f;
				base_col[3]	 = 1.f-mMaterial->color_blend[textureIndex];
				glTexEnvfv( GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR,base_col );
				glTexEnvf(	GL_TEXTURE_ENV, combiner,	GL_INTERPOLATE_ARB);
				glTexEnvf(	GL_TEXTURE_ENV, source0,	GL_PREVIOUS_ARB);
				glTexEnvf(	GL_TEXTURE_ENV, op0,		blend_operand_prev );
				glTexEnvf(	GL_TEXTURE_ENV, source1,	GL_TEXTURE );
				glTexEnvf(	GL_TEXTURE_ENV, op1,		blend_operand);
				glTexEnvf(	GL_TEXTURE_ENV, source2,	GL_CONSTANT_ARB );
				glTexEnvf(	GL_TEXTURE_ENV, op2,		GL_SRC_ALPHA);
			}break;
		case BLEND_MUL: 
			{
				// ------------------------------
				glTexEnvf(	GL_TEXTURE_ENV, combiner,	GL_MODULATE);
				glTexEnvf(	GL_TEXTURE_ENV, source0,	GL_PREVIOUS_ARB);
				glTexEnvf(	GL_TEXTURE_ENV, op0,		blend_operand_prev);
				glTexEnvf(	GL_TEXTURE_ENV, source1,	GL_TEXTURE );
				glTexEnvf(	GL_TEXTURE_ENV, op1,		blend_operand);
			}break;
		case BLEND_ADD: 
			{
				// ------------------------------
				glTexEnvf(	GL_TEXTURE_ENV, combiner,	GL_ADD_SIGNED_ARB);
				glTexEnvf(	GL_TEXTURE_ENV, source0,	GL_PREVIOUS_ARB );
				glTexEnvf(	GL_TEXTURE_ENV, op0,		blend_operand_prev );
				glTexEnvf(	GL_TEXTURE_ENV, source1,	GL_TEXTURE );
				glTexEnvf(	GL_TEXTURE_ENV, op1,		blend_operand );
			}break;
		case BLEND_SUB: 
			{
				// ------------------------------
				glTexEnvf(	GL_TEXTURE_ENV, combiner,	GL_SUBTRACT_ARB);
				glTexEnvf(	GL_TEXTURE_ENV, source0,	GL_PREVIOUS_ARB );
				glTexEnvf(	GL_TEXTURE_ENV, op0,		blend_operand_prev );
				glTexEnvf(	GL_TEXTURE_ENV, source1,	GL_TEXTURE );
				glTexEnvf(	GL_TEXTURE_ENV, op1,		blend_operand);
			}break;
		case BLEND_SCR: 
			{
				// ------------------------------
				glTexEnvf(	GL_TEXTURE_ENV, combiner,	GL_ADD);
				glTexEnvf(	GL_TEXTURE_ENV, source0,	GL_PREVIOUS_ARB );
				glTexEnvf(	GL_TEXTURE_ENV, op0,		blend_operand_prev );
				glTexEnvf(	GL_TEXTURE_ENV, source1,	GL_TEXTURE );
				glTexEnvf(	GL_TEXTURE_ENV, op1,		blend_operand);
			} break;
	}
#endif //!GL_ARB_texture_env_combine
}

bool KX_BlenderMaterial::setBlending( int ind, bool enable) 
{
	if(!enable) {
		if(mMaterial->flag[ind] &CALCALPHA )	return true;
		else if(mMaterial->flag[ind] &USEALPHA )	return true;
		return false;
	}
	else {
		// additive
		if(mMaterial->flag[ind] &CALCALPHA ) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_ONE, GL_ONE);
			return true;
		}

		// use alpha channel
		else if(mMaterial->flag[ind] &USEALPHA ) {
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			return true;
		}
	}
	return false;
}

bool KX_BlenderMaterial::setDefaultBlending()
{
	if( mMaterial->transp &TF_ADD) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);
		return true;
	}
	
	if( mMaterial->transp & TF_ALPHA ) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		return true;
	}
	
	glDisable(GL_BLEND);
	return false;
}

void KX_BlenderMaterial::setEnvMap(bool val, bool cube)
{
	#ifdef GL_ARB_texture_cube_map
	if( cube && RAS_EXT_support._ARB_texture_cube_map ) 
	{
		glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB );
		glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB);
		glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB);

		glEnable(GL_TEXTURE_GEN_S);
		glEnable(GL_TEXTURE_GEN_T);
		glEnable(GL_TEXTURE_GEN_R);
	}
	else {
	#endif//GL_ARB_texture_cube_map
		if( val ) {
			glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP );
			glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP);
			
			glEnable(GL_TEXTURE_GEN_S);
			glEnable(GL_TEXTURE_GEN_T);
			glEnable(GL_TEXTURE_GEN_R);
		}
		else {
			glDisable(GL_TEXTURE_GEN_S);
			glDisable(GL_TEXTURE_GEN_T);
			glDisable(GL_TEXTURE_GEN_R);
		}
	#ifdef GL_ARB_texture_cube_map
	}
	#endif//GL_ARB_texture_cube_map
}


void KX_BlenderMaterial::setTexMatrixData(int i)
{
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();

	glScalef( 
		mMaterial->mapping[i].scale[0], 
		mMaterial->mapping[i].scale[1], 
		mMaterial->mapping[i].scale[2]
	);
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


void KX_BlenderMaterial::setObjectMatrixData(int i)
{
	// will work without cubemaps
	// but a cubemap will look the best
	KX_GameObject *obj = 
		(KX_GameObject*)
		mScene->GetObjectList()->FindValue(mMaterial->mapping[i].objconame);

	if(!obj)
		return;

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

	float matr[16];
	glGetFloatv(GL_MODELVIEW_MATRIX, matr);
	MT_Matrix4x4 mvmat(matr);

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
//	KX_PYMETHODTABLE( KX_BlenderMaterial, getTexture ),
//	KX_PYMETHODTABLE( KX_BlenderMaterial, setTexture ),

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
	// nodda ?
	_getattr_up(PyObjectPlus);
}

int KX_BlenderMaterial::_setattr(const STR_String& attr, PyObject *pyvalue)
{
	return PyObjectPlus::_setattr(attr, pyvalue);
}

KX_PYMETHODDEF_DOC( KX_BlenderMaterial, getShader , "getShader()")
{
	#ifdef GL_ARB_shader_objects
	if(!RAS_EXT_support._ARB_shader_objects) {
		PyErr_Format(PyExc_SystemError, "GLSL not supported");
		return NULL;
	}
	else {
		Py_INCREF(mShader);
		return mShader;
	}
	#else
	Py_Return;
	#endif//GL_ARB_shader_objects
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


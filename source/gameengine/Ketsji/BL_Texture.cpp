// ------------------------------------
#ifdef WIN32
#include <windows.h>
#endif // WIN32
#ifdef __APPLE__
#define GL_GLEXT_LEGACY 1
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#else
#include <GL/gl.h>
#include <GL/glu.h>
#endif

#include <iostream>

#include "BL_Material.h"
#include "BL_Texture.h"
#include "MT_assert.h"

#include "DNA_texture_types.h"
#include "DNA_image_types.h"
#include "IMB_imbuf_types.h"
#include "BKE_image.h"
#include "BLI_blenlib.h"

#include "RAS_OpenGLRasterizer/RAS_GLExtensionManager.h"
#include "RAS_OpenGLRasterizer/ARB_multitexture.h"
#include "RAS_ICanvas.h"
#include "RAS_Rect.h"

#include "KX_GameObject.h"

using namespace bgl;

#define spit(x) std::cout << x << std::endl;

#include "MEM_guardedalloc.h"

extern "C" {
	// envmaps
	#include "IMB_imbuf.h"
	void my_envmap_split_ima(EnvMap *env);
	void my_free_envmapdata(EnvMap *env);
}

// (n&(n-1)) zeros the least significant bit of n 
static int is_pow2(int num) {
	return ((num)&(num-1))==0;
}
static int smaller_pow2(int num) {
	while (!is_pow2(num))
		num= num&(num-1);
	return num;	
}

BL_Texture::BL_Texture()
:	mTexture(0),
	mError(0),
	mOk(0),
	mNeedsDeleted(0),
	mType(0),
	mName(""),
	mUnit(0)
{
	// --
}

BL_Texture::~BL_Texture()
{
	// --
}

void BL_Texture::DeleteTex()
{
	if( mNeedsDeleted ) {
		glDeleteTextures(1, (GLuint*)&mTexture);
		mNeedsDeleted = 0;
		mOk = 0;
	}
}


bool BL_Texture::InitFromImage(int unit,  Image *img, bool mipmap)
{
	if(!img || img->ok==0 ) {
		mError = true;
		mOk = false;
		return mOk;
	}
	if( img->ibuf==0 ) {
		load_image(img, IB_rect, "", 0);
		if(img->ibuf==0) {
			img->ok = 0;
			mError = true;
			mOk = false;
			return mOk;
		} 
	}
	mTexture = img->bindcode;

	mName = img->id.name;
	mType = GL_TEXTURE_2D;
	mUnit = unit;

	// smoke em if we got em
	if (mTexture != 0) {
		glBindTexture(GL_TEXTURE_2D, mTexture );
		Validate();
		return mOk;
	}
	mNeedsDeleted = 1;
	glGenTextures(1, (GLuint*)&mTexture);
	InitGLTex(img->ibuf->rect, img->ibuf->x, img->ibuf->y, mipmap);
	Validate();
	return mOk;
}

void BL_Texture::InitGLTex(unsigned int *pix,int x,int y,bool mipmap)
{
	if (!is_pow2(x) || !is_pow2(y) ) {
		InitNonPow2Tex(pix, x,y,mipmap);
		return;
	}

	glBindTexture(GL_TEXTURE_2D, mTexture );
	if( mipmap ) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		gluBuild2DMipmaps( GL_TEXTURE_2D, GL_RGBA, x, y, GL_RGBA, GL_UNSIGNED_BYTE, pix );
	} 
	else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, pix );
	}

	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}


void BL_Texture::InitNonPow2Tex(unsigned int *pix,int x,int y,bool mipmap)
{
	int nx= smaller_pow2(x);
	int ny= smaller_pow2(y);

	unsigned int *newPixels = (unsigned int *)malloc(nx*ny*sizeof(unsigned int));
	
	gluScaleImage(GL_RGBA, x, y, GL_UNSIGNED_BYTE, pix, nx,ny, GL_UNSIGNED_BYTE, newPixels);
	glBindTexture(GL_TEXTURE_2D, mTexture );

	if( mipmap ) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		gluBuild2DMipmaps( GL_TEXTURE_2D, GL_RGBA, nx, ny, GL_RGBA, GL_UNSIGNED_BYTE, newPixels );
	}
	else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, nx, ny, 0, GL_RGBA, GL_UNSIGNED_BYTE, newPixels );
	}
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	free(newPixels);
}


bool BL_Texture::InitCubeMap(int unit,  EnvMap *cubemap )
{
#ifdef GL_ARB_texture_cube_map
	if(!RAS_EXT_support._ARB_texture_cube_map) {
		spit("cubemaps not supported");
		mError = true;
		mOk = false;
		return mOk;
	}
	
	else if(!cubemap || cubemap->ima->ok==0 ) {
		mError = true;
		mOk = false;
		return mOk;
	}

	if( cubemap->ima->ibuf==0 )  {
		load_image(cubemap->ima, IB_rect, "", 0);
		if(cubemap->ima->ibuf==0) {
			cubemap->ima->ok = 0;
			mError = true;
			mOk = false;
			return mOk;
		}
	}

	EnvMap *CubeMap = cubemap;
	mNeedsDeleted =	1;
	mType = GL_TEXTURE_CUBE_MAP_ARB;
	mTexture = 0;
	mName = CubeMap->ima->id.name;
	mUnit = unit;


	glGenTextures(1, (GLuint*)&mTexture);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, mTexture );
	bool needs_split = false;

	if(!CubeMap->cube[0]) needs_split = true; 

	if(needs_split){
		// split it
		my_envmap_split_ima(CubeMap);
	}

	int x = CubeMap->ima->ibuf->x;
	int y = CubeMap->ima->ibuf->y;

	// -----------------------------------
	x	= CubeMap->cube[0]->ibuf->x;
	y	= CubeMap->cube[0]->ibuf->y;

	// check the first image, and assume the rest
	if (!is_pow2(x) || !is_pow2(y)) {
		spit("invalid envmap size please render with CubeRes @ power of two");
		my_free_envmapdata(CubeMap);
		mError = true;
		mOk = false;
		return mOk;
	}
	/* 
	*/

#define SetCubeMapFace(face, num)	\
	glTexImage2D(face, 0,GL_RGBA,	\
	CubeMap->cube[num]->ibuf->x,	\
	CubeMap->cube[num]->ibuf->y,	\
	0, GL_RGBA, GL_UNSIGNED_BYTE,	\
	CubeMap->cube[num]->ibuf->rect)

	SetCubeMapFace(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB, 5);
	SetCubeMapFace(GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB, 3);
	SetCubeMapFace(GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB, 0);
	SetCubeMapFace(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB, 1);
	SetCubeMapFace(GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB, 2);
	SetCubeMapFace(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB, 4);

	glTexParameteri( GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_S,	 GL_REPEAT );
	glTexParameteri( GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_T,	 GL_REPEAT );
	
		
	if(needs_split) {
		cubemap->ima = CubeMap->ima;
		my_free_envmapdata(CubeMap);
	}

	mOk = IsValid();
	return mOk;

#else

	mError = true;
	mOk = false;
	return mOk;

#endif//GL_ARB_texture_cube_map
}


STR_String BL_Texture::GetName() const
{
	return mName;
}


bool BL_Texture::IsValid()
{
	return (mTexture!= 0)?glIsTexture(mTexture)!=0:false;
}


void BL_Texture::Validate()
{
	mOk = IsValid();
}


bool BL_Texture::Ok()
{
	return  (mTexture!= 0); 
}


unsigned int BL_Texture::GetTextureType() const
{
	return mType;
}

int BL_Texture::GetMaxUnits()
{
	GLint unit=0;
#ifdef GL_ARB_multitexture
	if(RAS_EXT_support._ARB_multitexture) {
		glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &unit);
		return (MAXTEX>=unit?unit:MAXTEX);
	}
#endif
	return 0;
}

void BL_Texture::ActivateFirst()
{
#ifdef GL_ARB_multitexture
	if(RAS_EXT_support._ARB_multitexture)
		bgl::blActiveTextureARB(GL_TEXTURE0_ARB);
#endif
}

void BL_Texture::ActivateUnit(int unit)
{
#ifdef GL_ARB_multitexture
	if(RAS_EXT_support._ARB_multitexture) {
		if(unit <= MAXTEX)
			bgl::blActiveTextureARB(GL_TEXTURE0_ARB+unit);
	}
#endif
}


void BL_Texture::DisableUnit()
{
#ifdef GL_ARB_multitexture
	if(RAS_EXT_support._ARB_multitexture)
		bgl::blActiveTextureARB(GL_TEXTURE0_ARB+mUnit);
#endif
	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);

#ifdef GL_ARB_texture_cube_map
	if(RAS_EXT_support._ARB_texture_cube_map)
		glDisable(GL_TEXTURE_CUBE_MAP_ARB);
#endif
	glDisable(GL_TEXTURE_2D);
	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
	glDisable(GL_TEXTURE_GEN_R);
	glDisable(GL_TEXTURE_GEN_Q);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
}


void BL_Texture::DisableAllTextures()
{
#ifdef GL_ARB_multitexture
	glDisable(GL_BLEND);
	for(int i=0; i<MAXTEX; i++) {
		if(RAS_EXT_support._ARB_multitexture)
			bgl::blActiveTextureARB(GL_TEXTURE0_ARB+i);
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
		glDisable(GL_TEXTURE_GEN_Q);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	}
#endif
}


void BL_Texture::ActivateTexture()
{
#ifdef GL_ARB_multitexture
	if(RAS_EXT_support._ARB_multitexture)
		bgl::blActiveTextureARB(GL_TEXTURE0_ARB+mUnit);
#ifdef GL_ARB_texture_cube_map
	if(mType == GL_TEXTURE_CUBE_MAP_ARB && RAS_EXT_support._ARB_texture_cube_map ) {
		glDisable(GL_TEXTURE_2D);
		glBindTexture( GL_TEXTURE_CUBE_MAP_ARB, mTexture );	
		glEnable(GL_TEXTURE_CUBE_MAP_ARB);
	} else
#endif
	{
		glDisable(GL_TEXTURE_CUBE_MAP_ARB);
		glBindTexture( GL_TEXTURE_2D, mTexture );	
		glEnable(GL_TEXTURE_2D);
	}
#endif
}

void BL_Texture::SetMapping(int mode)
{

	if(!(mode &USEREFL)) {
		glDisable(GL_TEXTURE_GEN_S);
		glDisable(GL_TEXTURE_GEN_T);
		glDisable(GL_TEXTURE_GEN_R);
		glDisable(GL_TEXTURE_GEN_Q);
		return;
	}

#ifdef GL_ARB_texture_cube_map
	if( mType == GL_TEXTURE_CUBE_MAP_ARB && 
		RAS_EXT_support._ARB_texture_cube_map &&
		mode &USEREFL) 
	{
		glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB );
		glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB );
		glTexGeni(GL_R, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP_ARB );
		
		glEnable(GL_TEXTURE_GEN_S);
		glEnable(GL_TEXTURE_GEN_T);
		glEnable(GL_TEXTURE_GEN_R);
		glDisable(GL_TEXTURE_GEN_Q);
		return;
	}
	else
#endif
	{
		glTexGeni(GL_S, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP );
		glTexGeni(GL_T, GL_TEXTURE_GEN_MODE, GL_SPHERE_MAP );

		glEnable(GL_TEXTURE_GEN_S);
		glEnable(GL_TEXTURE_GEN_T);
		glDisable(GL_TEXTURE_GEN_R);
		glDisable(GL_TEXTURE_GEN_Q);
	}
}


void BL_Texture::setTexEnv(BL_Material *mat, bool modulate)
{
#ifndef GL_ARB_texture_env_combine
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	return;
#else
	if(modulate || !RAS_EXT_support._ARB_texture_env_combine){
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		return;
	}

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB );

	GLfloat blend_operand		= GL_SRC_COLOR;
	GLfloat blend_operand_prev  = GL_SRC_COLOR;
	GLfloat alphaOp				= GL_SRC_ALPHA;

	GLenum combiner	= GL_COMBINE_RGB_ARB;
	GLenum source0	= GL_SOURCE0_RGB_ARB;
	GLenum source1	= GL_SOURCE1_RGB_ARB;
	GLenum source2	= GL_SOURCE2_RGB_ARB;
	GLenum op0		= GL_OPERAND0_RGB_ARB;
	GLenum op1		= GL_OPERAND1_RGB_ARB;
	GLenum op2		= GL_OPERAND2_RGB_ARB;

	// switch to alpha combiners
	if( mat->flag[mUnit]  &TEXALPHA ) {
		combiner = GL_COMBINE_ALPHA_ARB;
		source0	= GL_SOURCE0_ALPHA_ARB;
		source1 = GL_SOURCE1_ALPHA_ARB;
		source2 = GL_SOURCE2_ALPHA_ARB;
		op0 = GL_OPERAND0_ALPHA_ARB;
		op1 = GL_OPERAND1_ALPHA_ARB;
		op2 = GL_OPERAND2_ALPHA_ARB;
		blend_operand = GL_SRC_ALPHA;
		blend_operand_prev = GL_SRC_ALPHA;
		// invert
		if(mat->flag[mUnit] &TEXNEG) {
			blend_operand_prev = GL_ONE_MINUS_SRC_ALPHA;
			blend_operand = GL_ONE_MINUS_SRC_ALPHA;
		}
	}
	else {
		if(mat->flag[mUnit] &TEXNEG) {
			blend_operand_prev=GL_ONE_MINUS_SRC_COLOR;
			blend_operand = GL_ONE_MINUS_SRC_COLOR;
		}
	}
	bool using_alpha = false;

	if(mat->flag[mUnit]  &USEALPHA){
		alphaOp = GL_ONE_MINUS_SRC_ALPHA;
		using_alpha=true;
	}
	else if(mat->flag[mUnit]  &USENEGALPHA){
		alphaOp = GL_SRC_ALPHA;
		using_alpha = true;
	}

	switch( mat->blend_mode[mUnit] ) {
		case BLEND_MIX:
			{
				// ------------------------------
				if(!using_alpha) {
					GLfloat base_col[4];
					base_col[0]	 = base_col[1]  = base_col[2]  = 0.f;
					base_col[3]	 = 1.f-mat->color_blend[mUnit];
					glTexEnvfv( GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR,base_col );
				}
				glTexEnvf(	GL_TEXTURE_ENV, combiner,	GL_INTERPOLATE_ARB);
				glTexEnvf(	GL_TEXTURE_ENV, source0,	GL_PREVIOUS_ARB);
				glTexEnvf(	GL_TEXTURE_ENV, op0,		blend_operand_prev );
				glTexEnvf(	GL_TEXTURE_ENV, source1,	GL_TEXTURE );
				glTexEnvf(	GL_TEXTURE_ENV, op1,		blend_operand);
				if(!using_alpha)
					glTexEnvf(	GL_TEXTURE_ENV, source2,	GL_CONSTANT_ARB );
				else
					glTexEnvf(	GL_TEXTURE_ENV, source2,	GL_TEXTURE );

				glTexEnvf(	GL_TEXTURE_ENV, op2,		alphaOp);
			}break;
		case BLEND_MUL: 
			{
				// ------------------------------
				glTexEnvf(	GL_TEXTURE_ENV, combiner,	GL_MODULATE);
				glTexEnvf(	GL_TEXTURE_ENV, source0,	GL_PREVIOUS_ARB);
				glTexEnvf(	GL_TEXTURE_ENV, op0,		blend_operand_prev);
				glTexEnvf(	GL_TEXTURE_ENV, source1,	GL_TEXTURE );
				if(using_alpha)
					glTexEnvf(	GL_TEXTURE_ENV, op1,		alphaOp);
				else
					glTexEnvf(	GL_TEXTURE_ENV, op1,		blend_operand);
			}break;
		case BLEND_ADD: 
			{
				// ------------------------------
				glTexEnvf(	GL_TEXTURE_ENV, combiner,	GL_ADD_SIGNED_ARB);
				glTexEnvf(	GL_TEXTURE_ENV, source0,	GL_PREVIOUS_ARB );
				glTexEnvf(	GL_TEXTURE_ENV, op0,		blend_operand_prev );
				glTexEnvf(	GL_TEXTURE_ENV, source1,	GL_TEXTURE );
				if(using_alpha)
					glTexEnvf(	GL_TEXTURE_ENV, op1,		alphaOp);
				else
					glTexEnvf(	GL_TEXTURE_ENV, op1,		blend_operand);
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
				if(using_alpha)
					glTexEnvf(	GL_TEXTURE_ENV, op1,		alphaOp);
				else
					glTexEnvf(	GL_TEXTURE_ENV, op1,		blend_operand);
			} break;
	}
	glTexEnvf(	GL_TEXTURE_ENV, GL_RGB_SCALE_ARB,	1.0);
#endif //!GL_ARB_texture_env_combine
}

int BL_Texture::GetPow2(int n)
{
	if(!is_pow2(n))
		n = smaller_pow2(n);

	return n;
}

extern "C" {

void my_envmap_split_ima(EnvMap *env)
{
	ImBuf *ibuf;
	Image *ima;
	int dx, part;
	
	my_free_envmapdata(env);	
	
	dx= env->ima->ibuf->y;
	dx/= 2;
	if(3*dx != env->ima->ibuf->x) {
		printf("Incorrect envmap size\n");
		env->ok= 0;
		env->ima->ok= 0;
	}
	else {
		for(part=0; part<6; part++) {
			ibuf= IMB_allocImBuf(dx, dx, 24, IB_rect, 0);
			ima= (Image*)MEM_callocN(sizeof(Image), "image");
			ima->ibuf= ibuf;
			ima->ok= 1;
			env->cube[part]= ima;
		}
		IMB_rectcpy(env->cube[0]->ibuf, env->ima->ibuf, 
			0, 0, 0, 0, dx, dx);
		IMB_rectcpy(env->cube[1]->ibuf, env->ima->ibuf, 
			0, 0, dx, 0, dx, dx);
		IMB_rectcpy(env->cube[2]->ibuf, env->ima->ibuf, 
			0, 0, 2*dx, 0, dx, dx);
		IMB_rectcpy(env->cube[3]->ibuf, env->ima->ibuf, 
			0, 0, 0, dx, dx, dx);
		IMB_rectcpy(env->cube[4]->ibuf, env->ima->ibuf, 
			0, 0, dx, dx, dx, dx);
		IMB_rectcpy(env->cube[5]->ibuf, env->ima->ibuf, 
			0, 0, 2*dx, dx, dx, dx);
		env->ok= 2;
	}
}


void my_free_envmapdata(EnvMap *env)
{
	Image *ima;
	unsigned int a, part;
	
	for(part=0; part<6; part++) {
		ima= env->cube[part];
		if(ima) {
			if(ima->ibuf) IMB_freeImBuf(ima->ibuf);

			for(a=0; a<BLI_ARRAY_NELEMS(ima->mipmap); a++) {
				if(ima->mipmap[a]) IMB_freeImBuf(ima->mipmap[a]);
			}
			MEM_freeN(ima);
			env->cube[part]= 0;
		}
	}
	env->ok= 0;
}


} // extern C


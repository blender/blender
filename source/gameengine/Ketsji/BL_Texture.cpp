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

#include "RAS_GLExtensionManager.h"
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
	mName("")
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
		glDeleteTextures(1, (GLuint*)&(*mTexture));
		mNeedsDeleted = 0;
		mOk = 0;
	}
}


bool BL_Texture::InitFromImage( Image *img, bool mipmap)
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
	mTexture = &img->bindcode;
	mName = img->id.name;
	mType = BL_TEX2D;
	
	// smoke em if we got em
	if (*mTexture != 0) {
		glBindTexture(GL_TEXTURE_2D, *mTexture );
		Validate();
		return mOk;
	}
	glGenTextures(1, (GLuint*)mTexture);
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

	glBindTexture(GL_TEXTURE_2D, *mTexture );
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
	glBindTexture(GL_TEXTURE_2D, *mTexture );

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


bool BL_Texture::InitCubeMap( EnvMap *cubemap )
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
	mBlankTexture =	0;
	mType = BL_TEXCUBE;
	mTexture = &mBlankTexture;
	mName = CubeMap->ima->id.name;

	glGenTextures(1, (GLuint*)(mTexture));
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, *mTexture );
	bool needs_split = false;

	if(!CubeMap->cube[0]) needs_split = true; 

	if(needs_split){
		// split it
		my_envmap_split_ima(CubeMap);
	}

	int x = cubemap->ima->ibuf->x;
	int y = cubemap->ima->ibuf->y;
	unsigned int *data= (unsigned int *)malloc(x*y*sizeof(unsigned int));

	// -----------------------------------
	x	= CubeMap->cube[0]->ibuf->x;
	y	= CubeMap->cube[0]->ibuf->y;

	// check the first image, and assume the rest
	if (!is_pow2(x) || !is_pow2(y))
	{
		spit("invalid envmap size please render with CubeRes @ power of two");
		free(data);
		data = 0;
		mError = true;
		mOk = false;
		return mOk;
	}
	memcpy(data, CubeMap->cube[0]->ibuf->rect, (x*y*sizeof(unsigned int)));
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	
	// -----------------------------------
	x	= CubeMap->cube[1]->ibuf->x;
	y	= CubeMap->cube[1]->ibuf->y;
	memcpy(data, CubeMap->cube[1]->ibuf->rect, (x*y*sizeof(unsigned int)));
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	
	// -----------------------------------
	x	= CubeMap->cube[2]->ibuf->x;
	y	= CubeMap->cube[2]->ibuf->y;
	memcpy(data, CubeMap->cube[2]->ibuf->rect, (x*y*sizeof(unsigned int)));
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	
	// -----------------------------------
	x	= CubeMap->cube[3]->ibuf->x;
	y	= CubeMap->cube[3]->ibuf->y;
	memcpy(data, CubeMap->cube[3]->ibuf->rect, (x*y*sizeof(unsigned int)));
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	
	// -----------------------------------
	x	= CubeMap->cube[4]->ibuf->x;
	y	= CubeMap->cube[4]->ibuf->y;
	memcpy(data, CubeMap->cube[4]->ibuf->rect, (x*y*sizeof(unsigned int)));
	glTexImage2D(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	
	// -----------------------------------
	x	= CubeMap->cube[5]->ibuf->x;
	y	= CubeMap->cube[5]->ibuf->y;
	memcpy(data, CubeMap->cube[5]->ibuf->rect, (x*y*sizeof(unsigned int)));
	glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	glTexParameteri( GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_S,	 GL_REPEAT );
	glTexParameteri( GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_T,	 GL_REPEAT );

	if(data) {
		free(data);
		data = 0;
	}
	
	if(needs_split) {
		// okay we allocated, swap back to orig and free used
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
	return (mTexture && *mTexture!= 0)?glIsTexture(*mTexture)!=0:false;
}


void BL_Texture::Validate()
{
	mOk = IsValid();
}


bool BL_Texture::Ok()
{
	return  ( mTexture?((!mError || mOk ) && *mTexture!= 0):0 ); 
}


unsigned int BL_Texture::GetTextureType() const
{
	return mType;
}


BL_Texture::operator const unsigned int () const
{
	return mTexture? *mTexture:0;
}

bool BL_Texture::SetGLTex(unsigned int tex)
{
	return false;
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


}

unsigned int BL_Texture::mBlankTexture = 0;


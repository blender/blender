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

/** \file gameengine/Ketsji/BL_Texture.cpp
 *  \ingroup ketsji
 */

#include "GPU_glew.h"

#include <iostream>
#include <map>
#include <stdlib.h>

#include "BL_Material.h"
#include "BL_Texture.h"
#include "MT_assert.h"

#include "DNA_texture_types.h"
#include "DNA_image_types.h"
#include "IMB_imbuf_types.h"
#include "BKE_image.h"
#include "BLI_blenlib.h"

#include "RAS_ICanvas.h"
#include "RAS_Rect.h"

#include "KX_GameObject.h"

#define spit(x) std::cout << x << std::endl;

#include "MEM_guardedalloc.h"
#include "GPU_draw.h"
#include "GPU_extensions.h"

extern "C" {
	// envmaps
	#include "IMB_imbuf.h"
	void my_envmap_split_ima(EnvMap *env, ImBuf *ibuf);
	void my_free_envmapdata(EnvMap *env);
}

// (n&(n-1)) zeros the least significant bit of n 
static int is_power_of_2_i(int num)
{
	return ((num)&(num-1))==0;
}
static int power_of_2_min_i(int num)
{
	while (!is_power_of_2_i(num))
		num= num&(num-1);
	return num;
}

// Place holder for a full texture manager
class BL_TextureObject
{
public:
	unsigned int	gl_texture;
	void*			ref_buffer;
};

typedef std::map<char*, BL_TextureObject> BL_TextureMap;
static BL_TextureMap g_textureManager;
static GLint g_max_units = -1;


BL_Texture::BL_Texture()
:	mTexture(0),
	mOk(0),
	mNeedsDeleted(0),
	mType(0),
	mUnit(0),
	mEnvState(0)
{
	// --
}

BL_Texture::~BL_Texture()
{
	// --
}

void BL_Texture::DeleteTex()
{
	if ( mNeedsDeleted ) {
		glDeleteTextures(1, (GLuint*)&mTexture);
		mNeedsDeleted = 0;
		mOk = 0;
	}

	if (mEnvState) {
		glDeleteLists((GLuint)mEnvState, 1);
		mEnvState =0;
	}

	if (mDisableState) {
		glDeleteLists((GLuint)mDisableState, 1);
		mDisableState =0;
	}
	g_textureManager.clear();
}


bool BL_Texture::InitFromImage(int unit,  Image *img, bool mipmap)
{

	ImBuf *ibuf;
	if (!img || img->ok==0) 
	{
		mOk = false;
		return mOk;
	}

	ibuf= BKE_image_acquire_ibuf(img, NULL, NULL);
	if (ibuf==NULL)
	{
		img->ok = 0;
		mOk = false;
		return mOk;
	}

	mipmap = mipmap && GPU_get_mipmap();

	mTexture = img->bindcode[TEXTARGET_TEXTURE_2D];
	mType = GL_TEXTURE_2D;
	mUnit = unit;

	ActivateUnit(mUnit);

	if (mTexture != 0) {
		glBindTexture(GL_TEXTURE_2D, mTexture );
		Validate();
		BKE_image_release_ibuf(img, ibuf, NULL);
		return mOk;
	}

	// look for an existing gl image
	BL_TextureMap::iterator mapLook = g_textureManager.find(img->id.name);
	if (mapLook != g_textureManager.end())
	{
		if (mapLook->second.gl_texture != 0)
		{
			mTexture = mapLook->second.gl_texture;
			glBindTexture(GL_TEXTURE_2D, mTexture);
			mOk = IsValid();
			BKE_image_release_ibuf(img, ibuf, NULL);
			return mOk;
		}
	}

	mNeedsDeleted = 1;
	glGenTextures(1, (GLuint*)&mTexture);

#ifdef WITH_DDS
	if (ibuf->ftype == IMB_FTYPE_DDS)
		InitGLCompressedTex(ibuf, mipmap);
	else
		InitGLTex(ibuf->rect, ibuf->x, ibuf->y, mipmap);
#else
	InitGLTex(ibuf->rect, ibuf->x, ibuf->y, mipmap);
#endif

	// track created units
	BL_TextureObject obj;
	obj.gl_texture = mTexture;
	obj.ref_buffer = img;
	g_textureManager.insert(std::pair<char*, BL_TextureObject>((char*)img->id.name, obj));


	glDisable(GL_TEXTURE_2D);
	ActivateUnit(0);
	Validate();

	BKE_image_release_ibuf(img, ibuf, NULL);

	return mOk;
}

void BL_Texture::InitGLTex(unsigned int *pix,int x,int y,bool mipmap)
{
	if (!GPU_full_non_power_of_two_support() && (!is_power_of_2_i(x) || !is_power_of_2_i(y)) ) {
		InitNonPow2Tex(pix, x,y,mipmap);
		return;
	}

	glBindTexture(GL_TEXTURE_2D, mTexture );
	if ( mipmap ) {
		int i;
		ImBuf *ibuf;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		ibuf = IMB_allocFromBuffer(pix, NULL, x, y);

		IMB_makemipmap(ibuf, true);

		for (i = 0; i < ibuf->miptot; i++) {
			ImBuf *mip = IMB_getmipmap(ibuf, i);

			glTexImage2D(GL_TEXTURE_2D, i,  GL_RGBA,  mip->x, mip->y, 0, GL_RGBA, GL_UNSIGNED_BYTE, mip->rect);
		}
		IMB_freeImBuf(ibuf);
	} 
	else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, pix );
	}

	if (GLEW_EXT_texture_filter_anisotropic)
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, GPU_get_anisotropic());
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

void BL_Texture::InitGLCompressedTex(ImBuf *ibuf, bool mipmap)
{
#ifndef WITH_DDS
	// Fall back to uncompressed if DDS isn't enabled
	InitGLTex(ibuf->rect, ibuf->x, ibuf->y, mipmap);
	return;
#else
	glBindTexture(GL_TEXTURE_2D, mTexture);
	
	if (GPU_upload_dxt_texture(ibuf) == 0) {
		InitGLTex(ibuf->rect, ibuf->x, ibuf->y, mipmap);
		return;
	}
#endif
}

void BL_Texture::InitNonPow2Tex(unsigned int *pix,int x,int y,bool mipmap)
{
	int nx= power_of_2_min_i(x);
	int ny= power_of_2_min_i(y);

	ImBuf *ibuf = IMB_allocFromBuffer(pix, NULL, x, y);
	IMB_scaleImBuf(ibuf, nx, ny);

	glBindTexture(GL_TEXTURE_2D, mTexture );

	if ( mipmap ) {
		int i;
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		IMB_makemipmap(ibuf, true);

		for (i = 0; i < ibuf->miptot; i++) {
			ImBuf *mip = IMB_getmipmap(ibuf, i);

			glTexImage2D(GL_TEXTURE_2D, i,  GL_RGBA,  mip->x, mip->y, 0, GL_RGBA, GL_UNSIGNED_BYTE, mip->rect);
		}
	}
	else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, nx, ny, 0, GL_RGBA, GL_UNSIGNED_BYTE, ibuf->rect );
	}

	if (GLEW_EXT_texture_filter_anisotropic)
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, GPU_get_anisotropic());
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	IMB_freeImBuf(ibuf);
}


bool BL_Texture::InitCubeMap(int unit,  EnvMap *cubemap)
{
	if (!GLEW_ARB_texture_cube_map)
	{
		spit("cubemaps not supported");
		mOk = false;
		return mOk;
	}
	else if (!cubemap || cubemap->ima->ok==0) 
	{
		mOk = false;
		return mOk;
	}

	ImBuf *ibuf= BKE_image_acquire_ibuf(cubemap->ima, NULL, NULL);
	if (ibuf==0)
	{
		cubemap->ima->ok = 0;
		mOk = false;
		return mOk;
	}

	mNeedsDeleted =	1;
	mType = GL_TEXTURE_CUBE_MAP_ARB;
	mTexture = 0;
	mUnit = unit;

	ActivateUnit(mUnit);

	BL_TextureMap::iterator mapLook = g_textureManager.find(cubemap->ima->id.name);
	if (mapLook != g_textureManager.end())
	{
		if (mapLook->second.gl_texture != 0 && mapLook->second.ref_buffer == cubemap->ima)
		{
			mTexture = mapLook->second.gl_texture;
			glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, mTexture);
			mOk = IsValid();
			BKE_image_release_ibuf(cubemap->ima, ibuf, NULL);
			return mOk;
		}
	}


	glGenTextures(1, (GLuint*)&mTexture);
	glBindTexture(GL_TEXTURE_CUBE_MAP_ARB, mTexture);


	// track created units
	BL_TextureObject obj;
	obj.gl_texture = mTexture;
	obj.ref_buffer = cubemap->ima;
	g_textureManager.insert(std::pair<char*, BL_TextureObject>((char*)cubemap->ima->id.name, obj));


	bool needs_split = false;
	if (!cubemap->cube[0]) 
	{
		needs_split = true;
		spit ("Re-Generating texture buffer");
	}

	if (needs_split)
		my_envmap_split_ima(cubemap, ibuf);


	if (!is_power_of_2_i(cubemap->cube[0]->x) || !is_power_of_2_i(cubemap->cube[0]->y))
	{
		spit("invalid envmap size please render with CubeRes @ power of two");

		my_free_envmapdata(cubemap);
		mOk = false;
		BKE_image_release_ibuf(cubemap->ima, ibuf, NULL);
		return mOk;
	}


#define SetCubeMapFace(face, num)   \
	glTexImage2D(face, 0,GL_RGBA,	\
	cubemap->cube[num]->x,          \
	cubemap->cube[num]->y,          \
	0, GL_RGBA, GL_UNSIGNED_BYTE,   \
	cubemap->cube[num]->rect)

	SetCubeMapFace(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB, 5);
	SetCubeMapFace(GL_TEXTURE_CUBE_MAP_NEGATIVE_X_ARB, 3);
	SetCubeMapFace(GL_TEXTURE_CUBE_MAP_POSITIVE_Y_ARB, 0);
	SetCubeMapFace(GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_ARB, 1);
	SetCubeMapFace(GL_TEXTURE_CUBE_MAP_POSITIVE_Z_ARB, 2);
	SetCubeMapFace(GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_ARB, 4);

	glTexParameteri( GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
	glTexParameteri( GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_S,	 GL_CLAMP_TO_EDGE );
	glTexParameteri( GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_T,	 GL_CLAMP_TO_EDGE );
	if (GLEW_VERSION_1_2)
		glTexParameteri( GL_TEXTURE_CUBE_MAP_ARB, GL_TEXTURE_WRAP_R,	 GL_CLAMP_TO_EDGE );

	if (needs_split)
		my_free_envmapdata(cubemap);



	glDisable(GL_TEXTURE_CUBE_MAP_ARB);
	ActivateUnit(0);

	mOk = IsValid();

	BKE_image_release_ibuf(cubemap->ima, ibuf, NULL);

	return mOk;
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
	if (g_max_units < 0) {
		GLint unit = 0;
		glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS_ARB, &unit);
		g_max_units = (MAXTEX >= unit) ? unit : MAXTEX;
	}

	return g_max_units;
}

void BL_Texture::ActivateFirst()
{
	if (GLEW_ARB_multitexture)
		glActiveTextureARB(GL_TEXTURE0_ARB);
}

void BL_Texture::ActivateUnit(int unit)
{
	if (GLEW_ARB_multitexture)
		if (unit <= MAXTEX)
			glActiveTextureARB(GL_TEXTURE0_ARB+unit);
}


void BL_Texture::DisableUnit()
{
	if (GLEW_ARB_multitexture)
		glActiveTextureARB(GL_TEXTURE0_ARB+mUnit);

	glMatrixMode(GL_TEXTURE);
	glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);

	if (GLEW_ARB_texture_cube_map && glIsEnabled(GL_TEXTURE_CUBE_MAP_ARB))
		glDisable(GL_TEXTURE_CUBE_MAP_ARB);
	else
	{
		if (glIsEnabled(GL_TEXTURE_2D))
			glDisable(GL_TEXTURE_2D);
	}

	glDisable(GL_TEXTURE_GEN_S);
	glDisable(GL_TEXTURE_GEN_T);
	glDisable(GL_TEXTURE_GEN_R);
	glDisable(GL_TEXTURE_GEN_Q);
	glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
}


void BL_Texture::DisableAllTextures()
{
	for (int i=0; i<MAXTEX; i++) {
		if (GLEW_ARB_multitexture)
			glActiveTextureARB(GL_TEXTURE0_ARB+i);

		glMatrixMode(GL_TEXTURE);
		glLoadIdentity();
		glMatrixMode(GL_MODELVIEW);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_TEXTURE_GEN_S);
		glDisable(GL_TEXTURE_GEN_T);
		glDisable(GL_TEXTURE_GEN_R);
		glDisable(GL_TEXTURE_GEN_Q);
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
	}

	if (GLEW_ARB_multitexture)
		glActiveTextureARB(GL_TEXTURE0_ARB);
}


void BL_Texture::ActivateTexture()
{
	if (GLEW_ARB_multitexture)
		glActiveTextureARB(GL_TEXTURE0_ARB+mUnit);

	if (mType == GL_TEXTURE_CUBE_MAP_ARB && GLEW_ARB_texture_cube_map)
	{
		glBindTexture( GL_TEXTURE_CUBE_MAP_ARB, mTexture );
		glEnable(GL_TEXTURE_CUBE_MAP_ARB);
	}
	else {
		if (GLEW_ARB_texture_cube_map )
			glDisable(GL_TEXTURE_CUBE_MAP_ARB);

		glBindTexture( GL_TEXTURE_2D, mTexture );
		glEnable(GL_TEXTURE_2D);
	}
}

void BL_Texture::SetMapping(int mode)
{

	if (!(mode &USEREFL)) {
		glDisable(GL_TEXTURE_GEN_S);
		glDisable(GL_TEXTURE_GEN_T);
		glDisable(GL_TEXTURE_GEN_R);
		glDisable(GL_TEXTURE_GEN_Q);
		return;
	}

	if ( mType == GL_TEXTURE_CUBE_MAP_ARB && 
		GLEW_ARB_texture_cube_map &&
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
	if (modulate || !GLEW_ARB_texture_env_combine) {
		glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );
		return;
	}

	if (glIsList(mEnvState))
	{
		glCallList(mEnvState);
		return;
	}
	if (!mEnvState)
		mEnvState = glGenLists(1);

	glNewList(mEnvState, GL_COMPILE_AND_EXECUTE);

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
	if ( mat->flag[mUnit]  &TEXALPHA ) {
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
		if (mat->flag[mUnit] &TEXNEG) {
			blend_operand_prev = GL_ONE_MINUS_SRC_ALPHA;
			blend_operand = GL_ONE_MINUS_SRC_ALPHA;
		}
	}
	else {
		if (mat->flag[mUnit] &TEXNEG) {
			blend_operand_prev=GL_ONE_MINUS_SRC_COLOR;
			blend_operand = GL_ONE_MINUS_SRC_COLOR;
		}
	}
	bool using_alpha = false;

	if (mat->flag[mUnit]  &USEALPHA) {
		alphaOp = GL_ONE_MINUS_SRC_ALPHA;
		using_alpha=true;
	}
	else if (mat->flag[mUnit]  &USENEGALPHA) {
		alphaOp = GL_SRC_ALPHA;
		using_alpha = true;
	}

	switch (mat->blend_mode[mUnit]) {
		case BLEND_MIX:
			{
				// ------------------------------
				if (!using_alpha) {
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
				if (!using_alpha)
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
				if (using_alpha)
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
				if (using_alpha)
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
				if (using_alpha)
					glTexEnvf(	GL_TEXTURE_ENV, op1,		alphaOp);
				else
					glTexEnvf(	GL_TEXTURE_ENV, op1,		blend_operand);
			} break;
	}
	glTexEnvf(	GL_TEXTURE_ENV, GL_RGB_SCALE_ARB,	1.0f);

	glEndList();
}

int BL_Texture::GetPow2(int n)
{
	if (!is_power_of_2_i(n))
		n = power_of_2_min_i(n);

	return n;
}

void BL_Texture::SplitEnvMap(EnvMap *map)
{
	if (!map || !map->ima || (map->ima && !map->ima->ok)) return;
	ImBuf *ibuf= BKE_image_acquire_ibuf(map->ima, NULL, NULL);
	if (ibuf) {
		my_envmap_split_ima(map, ibuf);
		BKE_image_release_ibuf(map->ima, ibuf, NULL);
	}
}

unsigned int BL_Texture::mDisableState = 0;

extern "C" {

void my_envmap_split_ima(EnvMap *env, ImBuf *ibuf)
{
	int dx, part;
	
	my_free_envmapdata(env);
	
	dx= ibuf->y;
	dx/= 2;
	if (3*dx != ibuf->x) {
		printf("Incorrect envmap size\n");
		env->ok= 0;
		env->ima->ok= 0;
	}
	else {
		for (part=0; part<6; part++) {
			env->cube[part] = IMB_allocImBuf(dx, dx, 24, IB_rect);
		}
		IMB_rectcpy(env->cube[0], ibuf, 
			0, 0, 0, 0, dx, dx);
		IMB_rectcpy(env->cube[1], ibuf, 
			0, 0, dx, 0, dx, dx);
		IMB_rectcpy(env->cube[2], ibuf, 
			0, 0, 2*dx, 0, dx, dx);
		IMB_rectcpy(env->cube[3], ibuf, 
			0, 0, 0, dx, dx, dx);
		IMB_rectcpy(env->cube[4], ibuf, 
			0, 0, dx, dx, dx, dx);
		IMB_rectcpy(env->cube[5], ibuf, 
			0, 0, 2*dx, dx, dx, dx);

		env->ok= 2;// ENV_OSA
	}
}


void my_free_envmapdata(EnvMap *env)
{
	unsigned int part;
	
	for (part=0; part<6; part++) {
		ImBuf *ibuf= env->cube[part];
		if (ibuf) {
			IMB_freeImBuf(ibuf);
			env->cube[part] = NULL;
		}
	}
	env->ok= 0;
}


} // extern C


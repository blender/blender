#ifndef __BL_TEXTURE_H__
#define __BL_TEXTURE_H__
#include <vector>
// --
struct Image;
struct EnvMap;
// --
#include "STR_String.h"

class BL_Texture
{
private:
	// -----------------------------------
	unsigned int*		mTexture;
	bool				mError;
	bool				mOk;
	bool				mNeedsDeleted;
	unsigned int		mType;
	STR_String			mName;
	static unsigned int mBlankTexture;
	std::vector<EnvMap*>mCubeMem;
	// -----------------------------------
	void InitNonPow2Tex(unsigned int *p,int x,int y,bool mipmap );
	void InitGLTex(unsigned int *p,int x,int y,bool mipmap );

public:
	BL_Texture();
	~BL_Texture( );
	
	operator const unsigned int () const;
	bool Ok();

	STR_String GetName() const;

	unsigned int GetTextureType() const;
	void	DeleteTex();
	bool	InitFromImage( Image *img, bool mipmap);
	bool	InitCubeMap( EnvMap *cubemap );
	// 
	bool	SetGLTex(unsigned int tex);

	void	PopCubeMap();
	bool	IsValid();
	void	Validate();
};

enum TexType{
	BL_TEX2D	= 1,
	BL_TEXCUBE	= 2
};


#endif//__BL_TEXTURE_H__

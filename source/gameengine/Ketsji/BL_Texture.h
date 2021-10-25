
/** \file BL_Texture.h
 *  \ingroup ketsji
 */

#ifndef __BL_TEXTURE_H__
#define __BL_TEXTURE_H__

//	#include <vector>
//	#include <map>

#include "MT_Matrix4x4.h"
#include "KX_Camera.h"

// --
struct Image;
struct EnvMap;
class BL_Material;
class RAS_Rect;
class RAS_ICanvas;
//class RTData;

#include "STR_String.h"

class BL_Texture
{
private:
	unsigned int		mTexture;		// Bound texture unit data
	bool				mOk;			// ...
	bool				mNeedsDeleted;	// If generated
	unsigned int		mType;			// enum TEXTURE_2D | CUBE_MAP 
	int					mUnit;			// Texture unit associated with mTexture
	unsigned int		mEnvState;		// cache textureEnv
	static unsigned int	mDisableState;	// speed up disabling calls

	void InitNonPow2Tex(unsigned int *p,int x,int y,bool mipmap );
	void InitGLTex(unsigned int *p,int x,int y,bool mipmap );
	void InitGLCompressedTex(struct ImBuf *p, bool mipmap);
public:
	BL_Texture();
	~BL_Texture( );

	bool Ok();
	int	 GetUnit()			{return mUnit;}
	void SetUnit(int unit)	{mUnit = unit;}

	unsigned int GetTextureType() const;
	void DeleteTex();

	bool InitFromImage(int unit, Image *img, bool mipmap);
	bool InitCubeMap(int unit,EnvMap *cubemap );

	bool IsValid();
	void Validate();

	static void ActivateFirst();
	static void DisableAllTextures();
	static void ActivateUnit(int unit);
	static int GetMaxUnits();
	static int GetPow2(int x);
	static void SplitEnvMap(EnvMap *map);


	void ActivateTexture();
	void SetMapping(int mode);
	void DisableUnit();
	void setTexEnv(BL_Material *mat, bool modulate=false);
	unsigned int swapTexture (unsigned int newTex) {
		// swap texture codes
		unsigned int tmp = mTexture;
		mTexture = newTex;
		// return original texture code
		return tmp;
	}


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:BL_Texture")
#endif
};

#endif /* __BL_TEXTURE_H__ */

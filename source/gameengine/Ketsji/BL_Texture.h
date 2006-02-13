#ifndef __BL_TEXTURE_H__
#define __BL_TEXTURE_H__

#include <vector>
#include <map>

#include "MT_Matrix4x4.h"
#include "KX_Camera.h"

// --
struct Image;
struct EnvMap;
class BL_Material;
class RTData;
class RAS_Rect;
class RAS_ICanvas;

// --
#include "STR_String.h"

class BL_Texture
{
private:
	// -----------------------------------
	unsigned int		mTexture;
	bool				mError;
	bool				mOk;
	bool				mNeedsDeleted;
	unsigned int		mType;
	STR_String			mName;
	int					mUnit;
	// -----------------------------------
	void InitNonPow2Tex(unsigned int *p,int x,int y,bool mipmap );
	void InitGLTex(unsigned int *p,int x,int y,bool mipmap );

public:
	BL_Texture();
	~BL_Texture( );
	
	//operator const unsigned int () const;
	bool Ok();
	int	 GetUnit()			{return mUnit;}
	void SetUnit(int unit)	{mUnit = unit;}

	STR_String GetName() const;

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

	/** todo
	void CreateRenderTexture(RAS_Rect r, RTData d);
	void ReadDepth(RAS_Rect r, RTData d);
	static void BeginDepth(RAS_ICanvas *can, RTData d);
	static void EndDepth(RAS_ICanvas *can,RTData d);
	void SetDepthMapping(MT_Matrix4x4& p, MT_Matrix4x4& m);
	*/

	void ActivateTexture();
	void SetMapping(int mode);
	void DisableUnit();
	void setTexEnv(BL_Material *mat, bool modulate=false);
};

/* Render to texture support, managed by the scene
	TODO
*/


#endif//__BL_TEXTURE_H__

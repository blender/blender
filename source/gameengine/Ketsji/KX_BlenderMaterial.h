#ifndef __KX_BLENDER_MATERIAL_H__
#define __KX_BLENDER_MATERIAL_H__

#include <vector>


#include "RAS_IPolygonMaterial.h"
#include "BL_Material.h"
#include "BL_Texture.h"
#include "BL_Shader.h"

#include "PyObjectPlus.h"

#include "MT_Vector3.h"
#include "MT_Vector4.h"

struct TFace;
class KX_Scene;

class KX_BlenderMaterial :  public PyObjectPlus, public RAS_IPolyMaterial
{
	Py_Header;
public:
	// --------------------------------
	KX_BlenderMaterial(
		class KX_Scene*	scene, // light/obj list
		BL_Material*	mat,
		bool			skin,
		int				lightlayer,
		void*			clientobject,
		PyTypeObject*	T=&Type
	);

	virtual ~KX_BlenderMaterial();

	// --------------------------------
	virtual TCachingInfo GetCachingInfo(void) const
	{
		// --
		return (void*) this;
	}

	// --------------------------------
	virtual bool Activate(
		RAS_IRasterizer* rasty, 
		TCachingInfo& cachingInfo) const;
	
	void ActivateMat(
		RAS_IRasterizer* rasty, 
		TCachingInfo& cachingInfo)const;
		
	void ActivatShaders(
		RAS_IRasterizer* rasty, 
		TCachingInfo& cachingInfo)const;
	// --------------------------------

	TFace* GetTFace(void) const;

	// for ipos
	void UpdateIPO(
		MT_Vector4 rgba, MT_Vector3 specrgb,
		MT_Scalar hard, MT_Scalar spec,
		MT_Scalar ref, MT_Scalar emit, MT_Scalar alpha
	);
	
	// --------------------------------
	virtual PyObject* _getattr(const STR_String& attr);
	virtual int       _setattr(const STR_String& attr, PyObject *pyvalue);

	KX_PYMETHOD_DOC( KX_BlenderMaterial, getShader );
	KX_PYMETHOD_DOC( KX_BlenderMaterial, getMaterialIndex );
	KX_PYMETHOD_DOC( KX_BlenderMaterial, getTexture );
	KX_PYMETHOD_DOC( KX_BlenderMaterial, setTexture );

	// --------------------------------
	// pre calculate to avoid pops/lag at startup
	virtual void OnConstruction( );

private:
	BL_Material*	mMaterial;
	BL_Shader*		mShader;
	KX_Scene*		mScene;
	BL_Texture		mTextures[MAXTEX];		// texture array
	
	// message centers
	void	setTexData( bool enable );
	void	setShaderData( bool enable );

	void	setTextureEnvironment( int textureIndex );
	void	setEnvMap( bool val, bool cube=false);
	void	setTexMatrixData(int i);
	bool	setDefaultBlending();
	bool	setBlending( int ind, bool enable=false );
	void	setObjectMatrixData(int i);

	// cleanup stuff
	void	DisableTexData();
	void	OnExit();

	//void	DisableNonEnabled();
	// --
	mutable int	mPass;
};


#endif

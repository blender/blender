#ifndef __KX_BLENDER_MATERIAL_H__
#define __KX_BLENDER_MATERIAL_H__

#include <vector>


#include "RAS_IPolygonMaterial.h"
#include "BL_Material.h"
#include "BL_Texture.h"
#include "BL_Shader.h"
#include "BL_BlenderShader.h"

#include "PyObjectPlus.h"

#include "MT_Vector3.h"
#include "MT_Vector4.h"

struct MTFace;
class KX_Scene;

class KX_BlenderMaterial :  public PyObjectPlus, public RAS_IPolyMaterial
{
	Py_Header;
public:
	// --------------------------------
	KX_BlenderMaterial(
		class KX_Scene*	scene,
		BL_Material*	mat,
		bool			skin,
		int				lightlayer,
		void*			clientobject,
		PyTypeObject*	T=&Type
	);

	virtual ~KX_BlenderMaterial();

	// --------------------------------
	virtual TCachingInfo GetCachingInfo(void) const {
		return (void*) this;
	}

	virtual 
	bool Activate(
		RAS_IRasterizer* rasty, 
		TCachingInfo& cachingInfo
	) const;
	
	virtual 
	void ActivateMeshSlot(
		const KX_MeshSlot & ms, 
		RAS_IRasterizer* rasty 
	) const;
	
	void ActivateMat(
		RAS_IRasterizer* rasty,
		TCachingInfo& cachingInfo
	)const;

	void ActivatShaders(
		RAS_IRasterizer* rasty, 
		TCachingInfo& cachingInfo
	)const;

	void ActivateBlenderShaders(
		RAS_IRasterizer* rasty, 
		TCachingInfo& cachingInfo
	)const;

	MTFace* GetMTFace(void) const;
	unsigned int* GetMCol(void) const;

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

	KX_PYMETHOD_DOC( KX_BlenderMaterial, setBlending );

	// --------------------------------
	// pre calculate to avoid pops/lag at startup
	virtual void OnConstruction( );
private:
	BL_Material*		mMaterial;
	BL_Shader*			mShader;
	BL_BlenderShader*	mBlenderShader;
	KX_Scene*		mScene;
	BL_Texture		mTextures[MAXTEX];		// texture array
	bool			mUserDefBlend;
	unsigned int	mBlendFunc[2];
	bool			mModified;
	bool			mConstructed;			// if false, don't clean on exit

	void SetBlenderGLSLShader();

	void ActivatGLMaterials( RAS_IRasterizer* rasty )const;
	void ActivateTexGen( RAS_IRasterizer *ras ) const;


	// message centers
	void	setTexData( bool enable,RAS_IRasterizer *ras);
	void	setBlenderShaderData( bool enable, RAS_IRasterizer *ras);
	void	setShaderData( bool enable, RAS_IRasterizer *ras);

	bool	setDefaultBlending();
	void	setObjectMatrixData(int i, RAS_IRasterizer *ras);
	void	setTexMatrixData(int i);

	void	setLightData();

	// cleanup stuff
	void	OnExit();

	mutable int	mPass;
};


#endif

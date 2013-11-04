
/** \file KX_BlenderMaterial.h
 *  \ingroup ketsji
 */

#ifndef __KX_BLENDERMATERIAL_H__
#define __KX_BLENDERMATERIAL_H__

#include <vector>


#include "RAS_IPolygonMaterial.h"
#include "BL_Material.h"
#include "BL_Texture.h"
#include "BL_Shader.h"
#include "BL_BlenderShader.h"

#include "PyObjectPlus.h"

#include "MT_Vector3.h"
#include "MT_Vector4.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

#include "SCA_IScene.h" /* only for Replace_IScene */
#include "KX_Scene.h"

struct MTFace;
class KX_Scene;

class KX_BlenderMaterial :  public PyObjectPlus, public RAS_IPolyMaterial
{
	Py_Header
public:
	// --------------------------------
	KX_BlenderMaterial();
	void Initialize(
		class KX_Scene*	scene,
		BL_Material*	mat,
		GameSettings*	game,
		int				lightlayer
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
		const RAS_MeshSlot & ms, 
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

	Material* GetBlenderMaterial() const;
	Image* GetBlenderImage() const;
	MTFace* GetMTFace() const;
	unsigned int* GetMCol() const;
	BL_Texture * getTex (unsigned int idx) { 
		return (idx < MAXTEX) ? mTextures + idx : NULL; 
	}
	Image * getImage (unsigned int idx) { 
		return (idx < MAXTEX && mMaterial) ? mMaterial->img[idx] : NULL; 
	}
	unsigned int* getBlendFunc() {
		return mBlendFunc;
	}
	// for ipos
	void UpdateIPO(
		MT_Vector4 rgba, MT_Vector3 specrgb,
		MT_Scalar hard, MT_Scalar spec,
		MT_Scalar ref, MT_Scalar emit, MT_Scalar alpha
	);
	
	virtual void Replace_IScene(SCA_IScene *val);

#ifdef WITH_PYTHON
	// --------------------------------
	virtual PyObject *py_repr(void) { return PyUnicode_From_STR_String(mMaterial->matname); }

	static PyObject *pyattr_get_shader(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject *pyattr_get_materialIndex(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject *pyattr_get_blending(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static int       pyattr_set_blending(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);

	KX_PYMETHOD_DOC(KX_BlenderMaterial, getShader);
	KX_PYMETHOD_DOC(KX_BlenderMaterial, getMaterialIndex);
	KX_PYMETHOD_DOC(KX_BlenderMaterial, getTexture);
	KX_PYMETHOD_DOC(KX_BlenderMaterial, setTexture);

	KX_PYMETHOD_DOC(KX_BlenderMaterial, setBlending);
#endif  /* WITH_PYTHON */

	// --------------------------------
	// pre calculate to avoid pops/lag at startup
	virtual void OnConstruction();

	static void	EndFrame();

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
	int				mLightLayer;

	void InitTextures();

	void SetBlenderGLSLShader();

	void ActivatGLMaterials( RAS_IRasterizer* rasty )const;
	void ActivateTexGen( RAS_IRasterizer *ras ) const;

	bool UsesLighting(RAS_IRasterizer *rasty) const;
	void GetMaterialRGBAColor(unsigned char *rgba) const;
	Scene* GetBlenderScene() const;
	void ReleaseMaterial();

	// message centers
	void	setTexData( bool enable,RAS_IRasterizer *ras);
	void	setBlenderShaderData( bool enable, RAS_IRasterizer *ras);
	void	setShaderData( bool enable, RAS_IRasterizer *ras);

	void	setObjectMatrixData(int i, RAS_IRasterizer *ras);
	void	setTexMatrixData(int i);

	void	setLightData();

	// cleanup stuff
	void	OnExit();

	// shader chacing
	static BL_BlenderShader *mLastBlenderShader;
	static BL_Shader		*mLastShader;

	mutable int	mPass;
};


#endif

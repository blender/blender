
#ifndef __BL_GPUSHADER_H__
#define __BL_GPUSHADER_H__

#ifdef BLENDER_GLSL
#include "GPU_material.h"
#endif

#include "MT_Matrix4x4.h"
#include "MT_Matrix3x3.h"
#include "MT_Tuple2.h"
#include "MT_Tuple3.h"
#include "MT_Tuple4.h"

#include "RAS_IPolygonMaterial.h"

#include "KX_Scene.h"

struct Material;
struct Scene;
class BL_Material;

#define BL_MAX_ATTRIB	16

/**
 * BL_BlenderShader
 *  Blender GPU shader material
 */
class BL_BlenderShader
{
private:
#ifdef BLENDER_GLSL
	KX_Scene		*mScene;
	struct Scene	*mBlenderScene;
	struct Material	*mMat;
	GPUMaterial		*mGPUMat;
#endif
	bool			mBound;
	int				mLightLayer;
	int				mBlendMode;

	bool			VerifyShader();

public:
	BL_BlenderShader(KX_Scene *scene, struct Material *ma, int lightlayer);
	virtual ~BL_BlenderShader();

	bool				Ok();
	void				SetProg(bool enable);

	int GetAttribNum();
	void SetAttribs(class RAS_IRasterizer* ras, const BL_Material *mat);
	void Update(const class KX_MeshSlot & ms, class RAS_IRasterizer* rasty);
	int GetBlendMode();

	bool Equals(BL_BlenderShader *blshader);
};

#endif//__BL_GPUSHADER_H__

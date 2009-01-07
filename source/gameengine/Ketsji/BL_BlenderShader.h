
#ifndef __BL_GPUSHADER_H__
#define __BL_GPUSHADER_H__

#include "GPU_material.h"

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
	KX_Scene		*mScene;
	struct Scene	*mBlenderScene;
	struct Material	*mMat;
	int				mLightLayer;
	int				mBlendMode;

	bool			VerifyShader();

public:
	BL_BlenderShader(KX_Scene *scene, struct Material *ma, int lightlayer);
	virtual ~BL_BlenderShader();

	bool				Ok();
	void				SetProg(bool enable, double time=0.0);

	int GetAttribNum();
	void SetAttribs(class RAS_IRasterizer* ras, const BL_Material *mat);
	void Update(const class RAS_MeshSlot & ms, class RAS_IRasterizer* rasty);
	int GetBlendMode();

	bool Equals(BL_BlenderShader *blshader);
};

#endif//__BL_GPUSHADER_H__

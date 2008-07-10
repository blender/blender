
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

struct Material;
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
	GPUMaterial		*mGPUMat;
#endif
	bool			mBound;
	int				mLightLayer;

public:
	BL_BlenderShader(struct Material *ma, int lightlayer);
	virtual ~BL_BlenderShader();

	const bool			Ok()const;
	void				SetProg(bool enable);

	int GetAttribNum();
	void SetAttribs(class RAS_IRasterizer* ras, const BL_Material *mat);
	void Update(const class KX_MeshSlot & ms, class RAS_IRasterizer* rasty);

	bool Equals(BL_BlenderShader *blshader);
};

#endif//__BL_GPUSHADER_H__

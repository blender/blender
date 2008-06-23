
#ifndef __BL_GPUSHADER_H__
#define __BL_GPUSHADER_H__

#if 0
#include "GPU_material.h"
#endif

#include "MT_Matrix4x4.h"
#include "MT_Matrix3x3.h"
#include "MT_Tuple2.h"
#include "MT_Tuple3.h"
#include "MT_Tuple4.h"

struct Material;

#define BL_MAX_ATTRIB	16

/**
 * BL_BlenderShader
 *  Blender GPU shader material
 */
class BL_BlenderShader
{
private:
#if 0
	GPUMaterial		*mGPUMat;
#endif
	bool			mBound;

public:
	BL_BlenderShader(struct Material *ma);
	virtual ~BL_BlenderShader();

	const bool			Ok()const;
	void				SetProg(bool enable);

	void ApplyShader();
	void SetTexCoords(class RAS_IRasterizer* ras);
	int GetAttribNum();
	void Update(const class KX_MeshSlot & ms, class RAS_IRasterizer* rasty);
};

#endif//__BL_GPUSHADER_H__

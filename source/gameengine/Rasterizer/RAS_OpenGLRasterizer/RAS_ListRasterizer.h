#ifndef __RAS_LISTRASTERIZER_H__
#define __RAS_LISTRASTERIZER_H__

#include "RAS_MaterialBucket.h"
#include "RAS_VAOpenGLRasterizer.h"
#include <vector>

class RAS_ListRasterizer;
class RAS_ListSlot : public KX_ListSlot
{
	unsigned int m_list;
	unsigned int m_flag;
	RAS_ListRasterizer* m_rasty;
public:
	RAS_ListSlot(RAS_ListRasterizer* rasty);
	virtual ~RAS_ListSlot();
	virtual void SetModified(bool mod);
	virtual int Release();

	void RemoveList();
	void DrawList();
	void EndList();
	bool End();

};

enum RAS_ListSlotFlags	{
	LIST_CREATE		=1,
	LIST_MODIFY		=2,
	LIST_STREAM		=4,
	LIST_NOCREATE	=8,
	LIST_BEGIN		=16,
	LIST_END		=32,
	LIST_REGEN		=64
};

typedef std::map<const vecVertexArray, RAS_ListSlot*> RAS_Lists;

class RAS_ListRasterizer : public RAS_VAOpenGLRasterizer
{
	bool mUseVertexArrays;
	RAS_Lists mLists;

	RAS_ListSlot* FindOrAdd(const vecVertexArray& vertexarrays, KX_ListSlot** slot);
	void ReleaseAlloc();

public:
	void RemoveListSlot(RAS_ListSlot* list);
	RAS_ListRasterizer(RAS_ICanvas* canvas, bool useVertexArrays=false, bool lock=false);
	virtual ~RAS_ListRasterizer();

	virtual void IndexPrimitives(
			const vecVertexArray& vertexarrays,
			const vecIndexArrays & indexarrays,
			DrawMode mode,
			bool useObjectColor,
			const MT_Vector4& rgbacolor,
			class KX_ListSlot** slot
	);

	virtual void IndexPrimitivesMulti(
			const vecVertexArray& vertexarrays,
			const vecIndexArrays & indexarrays,
			DrawMode mode,
			bool useObjectColor,
			const MT_Vector4& rgbacolor,
			class KX_ListSlot** slot
	);

	virtual bool	Init();
	virtual void	Exit();

	virtual void	SetDrawingMode(int drawingmode);

	virtual bool	QueryLists(){return true;}
};

#endif

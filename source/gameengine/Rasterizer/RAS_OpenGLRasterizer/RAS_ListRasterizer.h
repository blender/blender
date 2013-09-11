
/** \file RAS_ListRasterizer.h
 *  \ingroup bgerastogl
 */

#ifndef __RAS_LISTRASTERIZER_H__
#define __RAS_LISTRASTERIZER_H__

#include "RAS_MaterialBucket.h"
#include "RAS_OpenGLRasterizer.h"
#include <vector>
#include <map>

class RAS_ListRasterizer;
class RAS_ListSlot : public KX_ListSlot
{
	friend class RAS_ListRasterizer;
	unsigned int m_list;
	unsigned int m_flag;
	unsigned int m_matnr;
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
	LIST_REGEN		=64,
	LIST_DERIVEDMESH=128,
};

struct DerivedMesh;

typedef std::map<RAS_DisplayArrayList, RAS_ListSlot*> RAS_ArrayLists;
typedef std::vector<RAS_ListSlot*>					  RAS_ListSlots;	// indexed by material slot number
typedef std::map<DerivedMesh*, RAS_ListSlots*>		  RAS_DerivedMeshLists;

class RAS_ListRasterizer : public RAS_OpenGLRasterizer
{
	RAS_ArrayLists mArrayLists;
	RAS_DerivedMeshLists mDerivedMeshLists;

	RAS_ListSlot* FindOrAdd(class RAS_MeshSlot& ms);
	void ReleaseAlloc();

public:
	void RemoveListSlot(RAS_ListSlot* list);
	RAS_ListRasterizer(RAS_ICanvas* canvas, bool lock=false, int storage=RAS_AUTO_STORAGE);
	virtual ~RAS_ListRasterizer();

	virtual void	IndexPrimitives(class RAS_MeshSlot& ms);
	virtual void	IndexPrimitivesMulti(class RAS_MeshSlot& ms);

	virtual bool	Init();
	virtual void	Exit();

	virtual void	SetDrawingMode(int drawingmode);

	virtual bool	QueryLists() {return true;}


#ifdef WITH_CXX_GUARDEDALLOC
	MEM_CXX_CLASS_ALLOC_FUNCS("GE:RAS_ListRasterizer")
#endif
};

#endif

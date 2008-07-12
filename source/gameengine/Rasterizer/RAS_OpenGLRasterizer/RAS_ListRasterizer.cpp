//
#include <iostream>

#include "RAS_ListRasterizer.h"

#ifdef WIN32
#include <windows.h>
#endif // WIN32

#include "GL/glew.h"

#include "RAS_TexVert.h"
#include "MT_assert.h"

//#ifndef NDEBUG
//#ifdef WIN32
//#define spit(x) std::cout << x << std::endl;
//#endif //WIN32
//#else
#define spit(x)
//#endif

RAS_ListSlot::RAS_ListSlot(RAS_ListRasterizer* rasty)
:	KX_ListSlot(),
	m_list(0),
	m_flag(LIST_MODIFY|LIST_CREATE),
	m_rasty(rasty)
{
}

int RAS_ListSlot::Release()
{
	if (--m_refcount > 0)
		return m_refcount;
	m_rasty->RemoveListSlot(this);
	delete this;
	return 0;
}

RAS_ListSlot::~RAS_ListSlot()
{
	RemoveList();
}


void RAS_ListSlot::RemoveList()
{
	if(m_list != 0) {
		spit("Releasing display list (" << m_list << ")");
		glDeleteLists((GLuint)m_list, 1);
		m_list =0;
	}
}

void RAS_ListSlot::DrawList()
{
	if(m_flag &LIST_STREAM || m_flag& LIST_NOCREATE) {
		RemoveList();
		return;
	}
	if(m_flag &LIST_MODIFY) {
		if(m_flag &LIST_CREATE) {
			if(m_list == 0) {
				m_list = (unsigned int)glGenLists(1);
				m_flag =  m_flag &~ LIST_CREATE;
				spit("Created display list (" << m_list << ")");
			}
		}
		if(m_list != 0)
			glNewList((GLuint)m_list, GL_COMPILE);
	
		m_flag |= LIST_BEGIN;
		return;
	}
	glCallList(m_list);
}

void RAS_ListSlot::EndList()
{
	if(m_flag & LIST_BEGIN) {
		glEndList();
		m_flag = m_flag &~(LIST_BEGIN|LIST_MODIFY);
		m_flag |= LIST_END;
		glCallList(m_list);
	}
}

void RAS_ListSlot::SetModified(bool mod)
{
	if(mod && !(m_flag & LIST_MODIFY)) {
		spit("Modifying list (" << m_list << ")");
		m_flag = m_flag &~ LIST_END;
		m_flag |= LIST_STREAM;
	}
}

bool RAS_ListSlot::End()
{
	return (m_flag &LIST_END)!=0;
}



RAS_ListRasterizer::RAS_ListRasterizer(RAS_ICanvas* canvas, bool useVertexArrays, bool lock)
:	RAS_VAOpenGLRasterizer(canvas, lock),
	mUseVertexArrays(useVertexArrays)
{
	// --
}

RAS_ListRasterizer::~RAS_ListRasterizer() 
{
	ReleaseAlloc();
}

void RAS_ListRasterizer::RemoveListSlot(RAS_ListSlot* list)
{
	RAS_Lists::iterator it = mLists.begin();
	while(it != mLists.end()) {
		if (it->second == list) {
			mLists.erase(it);
			break;
		}
		it++;
	}
}

RAS_ListSlot* RAS_ListRasterizer::FindOrAdd(const vecVertexArray& vertexarrays, KX_ListSlot** slot)
{
	/*
	 Keep a copy of constant lists submitted for rendering,
		this guards against (replicated)new...delete every frame,
		and we can reuse lists!
		:: sorted by vertex array
	*/
	RAS_ListSlot* localSlot = (RAS_ListSlot*)*slot;
	if(!localSlot) {
		RAS_Lists::iterator it = mLists.find(vertexarrays);
		if(it == mLists.end()) {
			localSlot = new RAS_ListSlot(this);
			mLists.insert(std::pair<vecVertexArray, RAS_ListSlot*>(vertexarrays, localSlot));
		} else {
			localSlot = static_cast<RAS_ListSlot*>(it->second->AddRef());
		}
	}
	MT_assert(localSlot);
	return localSlot;
}

void RAS_ListRasterizer::ReleaseAlloc()
{
	RAS_Lists::iterator it = mLists.begin();
	while(it != mLists.end()) {
		delete it->second;
		it++;
	}
	mLists.clear();
}


void RAS_ListRasterizer::IndexPrimitives(
	const vecVertexArray & vertexarrays,
	const vecIndexArrays & indexarrays,
	DrawMode mode,
	bool useObjectColor,
	const MT_Vector4& rgbacolor,
	class KX_ListSlot** slot)
{
	RAS_ListSlot* localSlot =0;

	// useObjectColor(are we updating every frame?)
	if(!useObjectColor && slot) {
		localSlot = FindOrAdd(vertexarrays, slot);
		localSlot->DrawList();
		if(localSlot->End()) {
			// save slot here too, needed for replicas and object using same mesh
			// => they have the same vertexarray but different mesh slot
			*slot = localSlot;
			return;
		}
	}
	
	if (mUseVertexArrays) {
		RAS_VAOpenGLRasterizer::IndexPrimitives(
				vertexarrays, indexarrays,
				mode, useObjectColor,
				rgbacolor,slot
		);
	} else {
		RAS_OpenGLRasterizer::IndexPrimitives(
				vertexarrays, indexarrays,
				mode, useObjectColor,
				rgbacolor,slot
		);
	}

	if(!useObjectColor && slot) {
		localSlot->EndList();
		*slot = localSlot;
	}
}


void RAS_ListRasterizer::IndexPrimitivesMulti(
		const vecVertexArray& vertexarrays,
		const vecIndexArrays & indexarrays,
		DrawMode mode,
		bool useObjectColor,
		const MT_Vector4& rgbacolor,
		class KX_ListSlot** slot)
{
	RAS_ListSlot* localSlot =0;

	// useObjectColor(are we updating every frame?)
	if(!useObjectColor && slot) {
		localSlot = FindOrAdd(vertexarrays, slot);
		localSlot->DrawList();

		if(localSlot->End()) {
			// save slot here too, needed for replicas and object using same mesh
			// => they have the same vertexarray but different mesh slot
			*slot = localSlot;
			return;
		}
	}

	// workaround: note how we do not use vertex arrays for making display
	// lists, since glVertexAttribPointerARB doesn't seem to work correct
	// in display lists on ATI? either a bug in the driver or in Blender ..
	if (mUseVertexArrays && !localSlot) {
		RAS_VAOpenGLRasterizer::IndexPrimitivesMulti(
				vertexarrays, indexarrays,
				mode, useObjectColor,
				rgbacolor,slot
		);
	} else {
		RAS_OpenGLRasterizer::IndexPrimitivesMulti(
				vertexarrays, indexarrays,
				mode, useObjectColor,
				rgbacolor,slot
		);
	}

	if(!useObjectColor && slot) {
		localSlot->EndList();
		*slot = localSlot;
	}
}

bool RAS_ListRasterizer::Init(void)
{
	if (mUseVertexArrays) {
		return RAS_VAOpenGLRasterizer::Init();
	} else {
		return RAS_OpenGLRasterizer::Init();
	}
}

void RAS_ListRasterizer::SetDrawingMode(int drawingmode)
{
	if (mUseVertexArrays) {
		RAS_VAOpenGLRasterizer::SetDrawingMode(drawingmode);
	} else {
		RAS_OpenGLRasterizer::SetDrawingMode(drawingmode);
	}
}

void RAS_ListRasterizer::Exit()
{
	if (mUseVertexArrays) {
		RAS_VAOpenGLRasterizer::Exit();
	} else {
		RAS_OpenGLRasterizer::Exit();
	}
}

// eof

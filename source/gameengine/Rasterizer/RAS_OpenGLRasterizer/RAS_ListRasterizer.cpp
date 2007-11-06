//
#include <iostream>

#include "RAS_ListRasterizer.h"

#ifdef WIN32
#include <windows.h>
#endif // WIN32
#ifdef __APPLE__
#define GL_GLEXT_LEGACY 1
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "RAS_TexVert.h"
#include "RAS_GLExtensionManager.h"
#include "MT_assert.h"

//#ifndef NDEBUG
//#ifdef WIN32
//#define spit(x) std::cout << x << std::endl;
//#endif //WIN32
//#else
#define spit(x)
//#endif

RAS_ListSlot::RAS_ListSlot()
:	KX_ListSlot(),
	m_flag(LIST_MODIFY|LIST_CREATE),
	m_list(0)
{
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



RAS_ListRasterizer::RAS_ListRasterizer(RAS_ICanvas* canvas)
:	RAS_OpenGLRasterizer(canvas)
{
	// --
}

RAS_ListRasterizer::~RAS_ListRasterizer() 
{
	ReleaseAlloc();
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
			localSlot = new RAS_ListSlot();
			mLists.insert(std::pair<vecVertexArray, RAS_ListSlot*>(vertexarrays, localSlot));
		} else {
			localSlot = it->second;
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
	int mode,
	class RAS_IPolyMaterial* polymat,
	class RAS_IRenderTools* rendertools,
	bool useObjectColor,
	const MT_Vector4& rgbacolor,
	class KX_ListSlot** slot)
{
	RAS_ListSlot* localSlot =0;

	// useObjectColor(are we updating every frame?)
	if(!useObjectColor) {
		localSlot = FindOrAdd(vertexarrays, slot);
		localSlot->DrawList();
		if(localSlot->End()) 
			return;
	}

	RAS_OpenGLRasterizer::IndexPrimitives(
			vertexarrays, indexarrays,
			mode, polymat,
			rendertools, useObjectColor,
			rgbacolor,slot
	);

	if(!useObjectColor) {
		localSlot->EndList();
		*slot = localSlot;
	}
}


void RAS_ListRasterizer::IndexPrimitivesMulti(
		const vecVertexArray& vertexarrays,
		const vecIndexArrays & indexarrays,
		int mode,
		class RAS_IPolyMaterial* polymat,
		class RAS_IRenderTools* rendertools,
		bool useObjectColor,
		const MT_Vector4& rgbacolor,
		class KX_ListSlot** slot)
{
	RAS_ListSlot* localSlot =0;

	// useObjectColor(are we updating every frame?)
	if(!useObjectColor) {
		localSlot = FindOrAdd(vertexarrays, slot);
		localSlot->DrawList();

		if(localSlot->End()) 
			return;
	}

	RAS_OpenGLRasterizer::IndexPrimitivesMulti(
			vertexarrays, indexarrays,
			mode, polymat,
			rendertools, useObjectColor,
			rgbacolor,slot
	);
	if(!useObjectColor) {
		localSlot->EndList();
		*slot = localSlot;
	}
}

// eof

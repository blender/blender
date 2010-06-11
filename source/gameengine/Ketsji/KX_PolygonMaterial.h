/**
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __KX_POLYGONMATERIAL_H__
#define __KX_POLYGONMATERIAL_H__

#include "PyObjectPlus.h"

#include "RAS_MaterialBucket.h"
#include "RAS_IRasterizer.h"
#include "DNA_ID.h"

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

struct MTFace;
struct Material;
struct MTex;

/**
 *  Material class.
 *
 *  This holds the shader, textures and python methods for setting the render state before
 *  rendering.
 */
class KX_PolygonMaterial : public PyObjectPlus, public RAS_IPolyMaterial
{
	Py_Header;
private:
	/** Blender texture face structure. */
	MTFace*			m_tface;
	unsigned int*	m_mcol;
	Material*		m_material;

#ifndef DISABLE_PYTHON
	PyObject*		m_pymaterial;
#endif

	mutable int		m_pass;
public:

	KX_PolygonMaterial();
	void Initialize(const STR_String &texname,
		Material* ma,
		int materialindex,
		int tile,
		int tilexrep,
		int tileyrep,
		int mode,
		int transp,
		bool alpha,
		bool zsort,
		int lightlayer,
		struct MTFace* tface,
		unsigned int* mcol);

	virtual ~KX_PolygonMaterial();
	
	/**
	 * Returns the caching information for this material,
	 * This can be used to speed up the rasterizing process.
	 * @return The caching information.
	 */
	virtual TCachingInfo GetCachingInfo(void) const
	{
		return (void*) this;
	}

	/**
	 * Activates the material in the (OpenGL) rasterizer.
	 * On entry, the cachingInfo contains info about the last activated material.
	 * On exit, the cachingInfo should contain updated info about this material.
	 * @param rasty			The rasterizer in which the material should be active.
	 * @param cachingInfo	The information about the material used to speed up rasterizing.
	 */
	void DefaultActivate(RAS_IRasterizer* rasty, TCachingInfo& cachingInfo) const;
	virtual bool Activate(RAS_IRasterizer* rasty, TCachingInfo& cachingInfo) const;

	Material *GetBlenderMaterial() const
	{
		return m_material;
	}

	/**
	 * Returns the Blender texture face structure that is used for this material.
	 * @return The material's texture face.
	 */
	MTFace* GetMTFace(void) const
	{
		return m_tface;
	}

	unsigned int* GetMCol(void) const
	{
		return m_mcol;
	}
	virtual void GetMaterialRGBAColor(unsigned char *rgba) const;

#ifndef DISABLE_PYTHON
	KX_PYMETHOD_DOC(KX_PolygonMaterial, updateTexture);
	KX_PYMETHOD_DOC(KX_PolygonMaterial, setTexture);
	KX_PYMETHOD_DOC(KX_PolygonMaterial, activate);
	
	KX_PYMETHOD_DOC(KX_PolygonMaterial, setCustomMaterial);
	KX_PYMETHOD_DOC(KX_PolygonMaterial, loadProgram);

	virtual PyObject* py_repr(void) { return PyUnicode_FromString(m_material ? ((ID *)m_material)->name+2 : ""); }
	
	static PyObject*	pyattr_get_texture(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject*	pyattr_get_material(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	
	static PyObject*	pyattr_get_tface(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	static PyObject*	pyattr_get_gl_texture(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef);
	
	static PyObject*	pyattr_get_diffuse(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef);	
	static int			pyattr_set_diffuse(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
	static PyObject*	pyattr_get_specular(void* self_v, const KX_PYATTRIBUTE_DEF *attrdef);	
	static int			pyattr_set_specular(void *self_v, const KX_PYATTRIBUTE_DEF *attrdef, PyObject *value);
#endif

};

#endif // __KX_POLYGONMATERIAL_H__


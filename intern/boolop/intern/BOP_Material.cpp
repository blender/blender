/**
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */
 
#include "BOP_Material.h"
#include <iostream>
using namespace std;

/**
 * Constructs a new material. 
 * @param faceWidth face material size in bytes.
 * @param faceVertexWidth verex face material size in bytes.
 */
BOP_Material::BOP_Material(int faceWidth, int faceVertexWidth)
{
	m_faceWidth = faceWidth;
	m_faceVertexWidth = faceVertexWidth;
	
	m_faceMaterial = new char[m_faceWidth];
	m_faceVertexMaterial = new char[N_FACE_VERTEX*m_faceVertexWidth];
}

/**
 * Constructs a new material duplicating the other object data.
 * @param other the other object to copy the data.
 */
BOP_Material::BOP_Material(const BOP_Material& other)
{
	m_faceWidth = other.getFaceWidth();
	m_faceVertexWidth = other.getFaceVertexWidth();
	
	m_faceMaterial = new char[m_faceWidth];
	m_faceVertexMaterial = new char[N_FACE_VERTEX*m_faceVertexWidth];
	
	duplicate(other);
}

/**
 * Destroys a material. 
 */
BOP_Material::~BOP_Material()
{
	delete[] m_faceMaterial;
	delete[] m_faceVertexMaterial;
}

/**
 * Duplicates the face material passed by argument.
 * @param faceMaterial pointer to face material data.
 */
void BOP_Material::setFaceMaterial(char* faceMaterial)
{
	memcpy(m_faceMaterial, faceMaterial, m_faceWidth);
}

/**
 * Duplicates the all face vertex materials passed by argument. It's supossed
 * that all face vertex materials positions are consecutive.
 * @param faceVertexMaterial pointer to firts vertex face material.
 */
void BOP_Material::setFaceVertexMaterial(char* faceVertexMaterial)
{
	memcpy(m_faceVertexMaterial, faceVertexMaterial, N_FACE_VERTEX*m_faceVertexWidth);
}

/**
 * Duplicates on i-position the face vertex material passed by argument.
 * @param faceMaterial pointer to face vertex material.
 * @param i destination position of new face vertex material (0<=i<4)
 */
void BOP_Material::setFaceVertexMaterial(char* faceVertexMaterial, int i)
{
	if (i>=0&&i<N_FACE_VERTEX)
		memcpy(m_faceVertexMaterial+i*m_faceVertexWidth, faceVertexMaterial, m_faceVertexWidth);
}

/**
 * Duplicates the other material object data.
 * @param other the other material object.
 */
void BOP_Material::duplicate(const BOP_Material& other)
{
	setOriginalFace(other.getOriginalFace());
	setIsQuad(other.isQuad());
	for (int i=0;i<N_FACE_VERTEX;++i)
		setOriginalFaceVertex(other.getOriginalFaceVertex(i),i);
	setFaceMaterial(other.getFaceMaterial());
	setFaceVertexMaterial(other.getFaceVertexMaterial(0));
}

/**
 * Implements operator =
 */
BOP_Material& BOP_Material::operator = (const BOP_Material& other)
{
	if (other.getFaceWidth() == m_faceWidth && other.getFaceVertexWidth() == m_faceVertexWidth)
		duplicate(other);
	return (*this);
}

/**
 * Returns the original face vertex material using a input vtx id. The input vtx IDs
 * are mapped to output ids, this one is used to obtain the original face vertex 
 * material.
 * @param originalFaceVertex input vertex id (0..3)
 * @return pointer to original face vertex material if it exist, NULL otherwise.
 */
char* BOP_Material::getOriginalFaceVertexMaterial(int originalFaceVertex)
{
	int  N = isQuad() ? 4 : 3;
	int  i = 0;
	bool b = false;
	while (i<N&&!b){
		if (m_originalFaceVertices[i]==originalFaceVertex) b = true;
		else i++;
	}
	return b ? getFaceVertexMaterial(i) : NULL;
}

/**
 * Returns the face material pointer.
 * @return pointer to face material.
 */
char* BOP_Material::getFaceMaterial() const
{
	return m_faceMaterial;
}

/**
 * Returns the face vertex material at i position.
 * @param i index of face vertex material.
 * @return pointer to face vertex material.
 */
inline char* BOP_Material::getFaceVertexMaterial(int i) const
{
	return i>=0&&i<N_FACE_VERTEX ? m_faceVertexMaterial + i*m_faceVertexWidth : NULL;
}

/**
 * Implements operator <<
 */
ostream &operator<<(ostream &stream, BOP_Material *m)
{
	cout << "(" << m->getOriginalFace() << ") < ";
	int N  = m->isQuad() ? 4 : 3;
	for (int i=0;i<N;++i) cout << m->getOriginalFaceVertex(i) << " ";
	cout << ">" << endl;

	return stream;
}

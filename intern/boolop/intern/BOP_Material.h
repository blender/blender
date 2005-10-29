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
 
#ifndef BOP_MATERIAL_H
#define BOP_MATERIAL_H

#include <iostream>
using namespace std;

#define N_FACE_VERTEX 4

class BOP_Material 
{
private:
	char* m_faceMaterial;
	char* m_faceVertexMaterial;
	int   m_faceWidth;
	int   m_faceVertexWidth;
	int   m_originalFace;
	int   m_originalFaceVertices[N_FACE_VERTEX];
	bool  m_isQuad;

public:
	BOP_Material(int faceWidth, int faceVertexWidth);
	BOP_Material(const BOP_Material& other);
	~BOP_Material();
	void setFaceMaterial(char* faceMaterial);
	void setFaceVertexMaterial(char* faceVertexMaterial);
	void setFaceVertexMaterial(char* faceVertexMaterial, int i);
	void duplicate(const BOP_Material& other);
	BOP_Material& operator = (const BOP_Material& other);
	char* getFaceMaterial() const;
	char* getFaceVertexMaterial(int i) const;
	int getFaceWidth() const { return m_faceWidth; };
	int getFaceVertexWidth() const { return m_faceVertexWidth; };

	void setOriginalFace(int originalFace) {m_originalFace = originalFace;};
	int getOriginalFace() const {return m_originalFace;};
	void setOriginalFaceVertex(int originalFaceVertex, int i) {
		if (0<=i&&i<N_FACE_VERTEX) m_originalFaceVertices[i] = originalFaceVertex;
	};
	int getOriginalFaceVertex(int i) const {
		if (0<=i&&i<N_FACE_VERTEX) return m_originalFaceVertices[i];
		else return -1;
	};
	char* getOriginalFaceVertexMaterial(int originalFaceVertex);
	void setIsQuad(bool quad) {m_isQuad = quad;};
	bool isQuad() const {return m_isQuad;};

	friend ostream &operator<<(ostream &stream, BOP_Material *m);
};

#endif

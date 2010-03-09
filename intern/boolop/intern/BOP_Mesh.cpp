/**
 *
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
 
#include "BOP_Mesh.h"
#include "BOP_MathUtils.h"
#include <iostream>
#include <fstream>

#include "MEM_guardedalloc.h"
#include "BLI_blenlib.h"

BOP_Mesh::BOP_Mesh()
{
#ifdef HASH
#ifdef HASH_PRINTF_DEBUG
	printf ("has hashing\n");
#endif
	hash = NULL;
	hashsize = 0;
#endif
}

/**
 * Destroys a mesh.
 */
BOP_Mesh::~BOP_Mesh()
{
	const BOP_IT_Vertexs vertexsEnd = m_vertexs.end();
	for(BOP_IT_Vertexs itv=m_vertexs.begin();itv!=vertexsEnd;itv++){
		delete *itv;
	}
	m_vertexs.clear();
  
	const BOP_IT_Edges edgesEnd = m_edges.end();
	for(BOP_IT_Edges ite=m_edges.begin();ite!=edgesEnd;ite++){
		delete *ite;
	}
	m_edges.clear();
  
	const BOP_IT_Faces facesEnd = m_faces.end();
	for(BOP_IT_Faces itf=m_faces.begin();itf!=facesEnd;itf++){
		delete *itf;
	}
	m_faces.clear();

#ifdef HASH
	while( hashsize ) {
		--hashsize;
		BLI_freelistN( &hash[hashsize] );
	}
	MEM_freeN( hash );
	hash = NULL;
#endif
}

/**
 * Adds a new vertex.
 * @param p vertex point
 * @return mesh vertex index
 */
BOP_Index BOP_Mesh::addVertex(MT_Point3 p)
{
	m_vertexs.push_back(new BOP_Vertex(p));
	return m_vertexs.size()-1;
}

/**
 * Adds a new edge.
 * @param v1 mesh vertex index
 * @param v2 mesh vertex index
 * @return mesh edge index
 */
BOP_Index BOP_Mesh::addEdge(BOP_Index v1, BOP_Index v2)
{
#ifdef HASH
	/* prepare a new hash entry for the edge */
	int minv;
	EdgeEntry *h = (EdgeEntry *)MEM_callocN( sizeof( EdgeEntry ), "edgehash" );

	/* store sorted, smallest vert first */
	if( v1 < v2 ) {
		minv = HASH(v1);
		h->v1 = v1;
		h->v2 = v2;
	} else {
		minv = HASH(v2);
		h->v1 = v2;
		h->v2 = v1;
	}
	h->index = m_edges.size();

	/* if hash index larger than hash list, extend the list */
	if( minv >= hashsize ) {
		int newsize = (minv + 8) & ~7;
		ListBase *nhash = (ListBase *)MEM_mallocN( 
				newsize * sizeof( ListBase ),
				"edgehashtable" );
		/* copy old entries */
		memcpy( nhash, hash, sizeof( ListBase ) * hashsize );
		/* clear new ones */
		while( hashsize < newsize ) {
			nhash[hashsize].first = nhash[hashsize].last = NULL;
			++hashsize;
		}
		if( hash )
			MEM_freeN( hash );
		hash = nhash;
	}

	/* add the entry to tail of the right hash list */
	BLI_addtail( &hash[minv], h );
#endif
	m_edges.push_back(new BOP_Edge(v1,v2));
	return m_edges.size()-1;
}

#ifdef HASH
/**
 * replace one vertex with another in the hash list
 * @param o old mesh vertex index
 * @param n new mesh vertex index
 * @param x edge's other mesh vertex index
 */
void BOP_Mesh::rehashVertex(BOP_Index o, BOP_Index n, BOP_Index x)
{
	EdgeEntry *edge;
	int minv = HASH(o);
	BOP_Index v1, v2;

	/* figure out where and what to look for */
	if( o < x ) {
		minv = HASH(o);
		v1 = o; v2 = x;
	} else {
		minv = HASH(x);
		v1 = x; v2 = o;
	}

	/* if hash is valid, search for the match */
	if( minv < hashsize ) {
		for(edge = (EdgeEntry *)hash[minv].first;
				edge; edge = edge->next ) {
			if(edge->v1 == v1 && edge->v2 == v2)
				break;
		}

		/* NULL edge == no match */
		if(!edge) {
#ifdef HASH_PRINTF_DEBUG
			printf ("OOPS! didn't find edge (%d %d)\n",v1,v2);
#endif
			return;
		}
#ifdef HASH_PRINTF_DEBUG
		printf ("found edge (%d %d)\n",v1,v2);
#endif
		/* remove the edge from the old hash list */
		BLI_remlink( &hash[minv], edge );

		/* decide where the new edge should go */
		if( n < x ) {
			minv = HASH(n);
			v1 = n; v2 = x;
		} else {
			minv = HASH(x);
			edge->v1 = x; edge->v2 = n;
		}

		/* if necessary, extend the hash list */
		if( minv >= hashsize ) {
#ifdef HASH_PRINTF_DEBUG
			printf ("OOPS! new vertex too large! (%d->%d)\n",o,n);
#endif
			int newsize = (minv + 8) & ~7;
			ListBase *nhash = (ListBase *)MEM_mallocN( 
					newsize * sizeof( ListBase ),
					"edgehashtable" );
			memcpy( nhash, hash, sizeof( ListBase ) * hashsize );
			while( hashsize < newsize ) {
				nhash[hashsize].first = nhash[hashsize].last = NULL;
				++hashsize;
			}
			if( hash )
				MEM_freeN( hash );
			hash = nhash;
		}

		/* add the entry to tail of the right hash list */
		BLI_addtail( &hash[minv], edge );
	} else {
#ifdef HASH_PRINTF_DEBUG
		printf ("OOPS! hash not large enough for (%d %d)\n",minv,hashsize);
#endif
	}
}
#endif

/**
 * Adds a new face.
 * @param face mesh face
 * @return mesh face index
 */
BOP_Index BOP_Mesh::addFace(BOP_Face *face)
{
	if (face->size()==3)
		return addFace((BOP_Face3 *)face);
	else
		return addFace((BOP_Face4 *)face);
}

/**
 * Adds a new triangle.
 * @param face mesh triangle
 * @return mesh face index
 */
BOP_Index BOP_Mesh::addFace(BOP_Face3 *face)
{
	BOP_Index indexface = m_faces.size();
	
	BOP_Index index1 = face->getVertex(0);
	BOP_Index index2 = face->getVertex(1);
	BOP_Index index3 = face->getVertex(2);

	m_faces.push_back((BOP_Face *)face);  
		
	BOP_Index edge;

	if (!getIndexEdge(index1,index2,edge)) {
		edge = addEdge(index1,index2);
		getVertex(index1)->addEdge(edge);
		getVertex(index2)->addEdge(edge);
	}
    		
	getEdge(edge)->addFace(indexface);
	
	if (!getIndexEdge(index2,index3,edge)) {
		edge = addEdge(index2,index3);
		getVertex(index2)->addEdge(edge);
		getVertex(index3)->addEdge(edge);
	}
    
	getEdge(edge)->addFace(indexface);

	if (!getIndexEdge(index3,index1,edge)) {
		edge = addEdge(index3,index1);
		getVertex(index3)->addEdge(edge);
		getVertex(index1)->addEdge(edge);
	}
    
	getEdge(edge)->addFace(indexface);
	
	if ((index1 == index2) || (index1 == index3) || (index2 == index3))
		face->setTAG(BROKEN);

	return indexface;  
}

/**
 * Adds a new quad.
 * @param face mesh quad
 * @return mesh face index
 */
BOP_Index BOP_Mesh::addFace(BOP_Face4 *face)
{
	m_faces.push_back((BOP_Face *)face);
	BOP_Index indexface = m_faces.size()-1;
  
	BOP_Index index1 = face->getVertex(0);
	BOP_Index index2 = face->getVertex(1);
	BOP_Index index3 = face->getVertex(2);
	BOP_Index index4 = face->getVertex(3);
  
	BOP_Index edge;
  
	if (!getIndexEdge(index1,index2,edge)) {
		edge = addEdge(index1,index2);
		getVertex(index1)->addEdge(edge);
		getVertex(index2)->addEdge(edge);
	}
  
	getEdge(edge)->addFace(indexface);
  
	if (!getIndexEdge(index2,index3,edge)) {
		edge = addEdge(index2,index3);
		getVertex(index2)->addEdge(edge);
		getVertex(index3)->addEdge(edge);
	}
  
	getEdge(edge)->addFace(indexface);    	
  
	if (!getIndexEdge(index3,index4,edge)) {
		edge = addEdge(index3,index4);
		getVertex(index3)->addEdge(edge);
		getVertex(index4)->addEdge(edge);
	}
  
	getEdge(edge)->addFace(indexface);	
  
	if (!getIndexEdge(index4,index1,edge)) {
		edge = addEdge(index4,index1);
		getVertex(index4)->addEdge(edge);
		getVertex(index1)->addEdge(edge);
	}
  
	getEdge(edge)->addFace(indexface);
  
	if ((index1 == index2) || (index1 == index3) || (index1 == index4) || 
		(index2 == index3) || (index2 == index4) || (index3 == index4))
		face->setTAG(BROKEN);

	return m_faces.size()-1;
}

/**
 * Returns if a faces set contains the specified face.
 * @param faces faces set
 * @param face face
 * @return true if the set contains the specified face
 */
bool BOP_Mesh::containsFace(BOP_Faces *faces, BOP_Face *face) 
{
  const BOP_IT_Faces facesEnd = faces->end();
	for(BOP_IT_Faces it = faces->begin();it!=facesEnd;it++) {
		if (face == *it)
			return true;
	}
	
	return false;
}
/**
 * Returns the first edge with the specified vertex index from a list of edge indexs.
 * @param edges edge indexs
 * @param v vertex index
 * @return first edge with the specified vertex index, NULL otherwise
 */
BOP_Edge* BOP_Mesh::getEdge(BOP_Indexs edges, BOP_Index v)
{
  const BOP_IT_Indexs edgesEnd = edges.end();
	for(BOP_IT_Indexs it=edges.begin();it!=edgesEnd;it++){
		BOP_Edge *edge = m_edges[*it];
		if ((edge->getVertex1() == v) || (edge->getVertex2() == v))
			return edge;
	}
	return NULL;
}

/**
 * Returns the mesh edge with the specified vertex indexs.
 * @param v1 vertex index
 * @param v2 vertex index
 * @return mesh edge with the specified vertex indexs, NULL otherwise
 */
BOP_Edge* BOP_Mesh::getEdge(BOP_Index v1, BOP_Index v2)
{
#ifdef HASH
	int minv;
	EdgeEntry *edge;

	/* figure out where and what to search for */
	if( v1 < v2 ) {
		minv = HASH(v1);
	} else {
		minv = HASH(v2);
		BOP_Index tmp = v1;
		v1 = v2;
		v2 = tmp;
	}

	/* if hash index valid, search the list and return match if found */
	if( minv < hashsize ) {
		for(edge = (EdgeEntry *)hash[minv].first;
				edge; edge = edge->next ) {
			if(edge->v1 == v1 && edge->v2 == v2)
				return m_edges[edge->index];
		}
	}
#else
	const BOP_IT_Edges edgesEnd = m_edges.end();
	for(BOP_IT_Edges edge=m_edges.begin();edge!=edgesEnd;edge++) {
		if (((*edge)->getVertex1() == v1 && (*edge)->getVertex2() == v2) ||
			((*edge)->getVertex1() == v2 && (*edge)->getVertex2() == v1)) 
			return *edge;
	} 
#endif
	return NULL;
}

/**
 * Returns the mesh edge index with the specified vertex indexs.
 * @param v1 vertex index
 * @param v2 vertex index
 * @param e edge index with the specified vertex indexs
 * @return true if there is a mesh edge with the specified vertex indexs, false otherwise
 */
bool BOP_Mesh::getIndexEdge(BOP_Index v1, BOP_Index v2, BOP_Index &e)
{
#ifdef HASH
	int minv;
	EdgeEntry *edge;

	/* figure out what and where to look */
	if( v1 < v2 ) {
		minv = HASH(v1);
	} else {
		minv = HASH(v2);
		BOP_Index tmp = v1;
		v1 = v2;
		v2 = tmp;
	}

	/* if hash index is valid, look for a match */
	if( minv < hashsize ) {
		for(edge = (EdgeEntry *)hash[minv].first;
				edge; edge = edge->next ) {
			if(edge->v1 == v1 && edge->v2 == v2)
				break;
		}

		/* edge != NULL means match */
		if(edge) {
#ifdef HASH_PRINTF_DEBUG
			printf ("found edge (%d %d)\n",v1,v2);
#endif
		  	e = edge->index;
#ifdef BOP_NEW_MERGE
			if( m_edges[e]->getUsed() == false ) {
				m_edges[e]->setUsed(true);
				m_vertexs[v1]->addEdge(e);
				m_vertexs[v2]->addEdge(e);
			}
#endif
			return true;
		}
#ifdef HASH_PRINTF_DEBUG
		else
			printf ("didn't find edge (%d %d)\n",v1,v2);
#endif
	}
#else
	BOP_Index pos=0;
	const BOP_IT_Edges edgesEnd = m_edges.end();
	for(BOP_IT_Edges edge=m_edges.begin();edge!=edgesEnd;edge++,pos++) {
		if (((*edge)->getVertex1() == v1 && (*edge)->getVertex2() == v2) ||
		    ((*edge)->getVertex1() == v2 && (*edge)->getVertex2() == v1)){
		  e = pos;
		  return true;
		}
	} 
#endif
	return false;
}

/**
 * Returns the mesh edge on the specified face and relative edge index.
 * @param face mesh face
 * @param edge face relative edge index
 * @return mesh edge on the specified face and relative index, NULL otherwise
 */
BOP_Edge* BOP_Mesh::getEdge(BOP_Face *face, unsigned int edge)
{
	if (face->size()==3)
		return getEdge((BOP_Face3 *)face,edge);
	else
		return getEdge((BOP_Face4 *)face,edge);
}

/**
 * Returns the mesh edge on the specified triangle and relative edge index.
 * @param face mesh triangle
 * @param edge face relative edge index
 * @return mesh edge on the specified triangle and relative index, NULL otherwise
 */
BOP_Edge* BOP_Mesh::getEdge(BOP_Face3 *face, unsigned int edge)
{
	switch(edge){
	case 1:
		return getEdge(m_vertexs[face->getVertex(0)]->getEdges(),face->getVertex(1));
	case 2:
		return getEdge(m_vertexs[face->getVertex(1)]->getEdges(),face->getVertex(2));
	case 3:
		return getEdge(m_vertexs[face->getVertex(2)]->getEdges(),face->getVertex(0));
	};
  
	return NULL;
}

/**
 * Returns the mesh edge on the specified quad and relative edge index.
 * @param face mesh quad
 * @param edge face relative edge index
 * @return mesh edge on the specified quad and relative index, NULL otherwise
 */
BOP_Edge * BOP_Mesh::getEdge(BOP_Face4 *face, unsigned int edge)
{
	switch(edge){
	case 1:
		return getEdge(m_vertexs[face->getVertex(0)]->getEdges(),face->getVertex(1));
	case 2:
		return getEdge(m_vertexs[face->getVertex(1)]->getEdges(),face->getVertex(2));
	case 3:
		return getEdge(m_vertexs[face->getVertex(2)]->getEdges(),face->getVertex(3));
	case 4:
		return getEdge(m_vertexs[face->getVertex(3)]->getEdges(),face->getVertex(0));
	};
	
	return NULL;
}

/**
 * Returns the mesh face with the specified vertex indexs.
 * @param v1 vertex index
 * @param v2 vertex index
 * @param v3 vertex index
 * @return mesh edge with the specified vertex indexs, NULL otherwise
 */
BOP_Face* BOP_Mesh::getFace(BOP_Index v1, BOP_Index v2, BOP_Index v3) 
{
	const BOP_IT_Faces facesEnd = m_faces.end();
	for(BOP_IT_Faces face=m_faces.begin();face!=facesEnd;face++) {
		if ((*face)->containsVertex(v1) && (*face)->containsVertex(v2) && 
			(*face)->containsVertex(v3))
			return (*face);
	} 
	return NULL;
}

/**
 * Returns the mesh face index with the specified vertex indexs.
 * @param v1 vertex index
 * @param v2 vertex index
 * @param v3 vertex index
 * @param f face index with the specified vertex indexs
 * @return true if there is a mesh face with the specified vertex indexs, false otherwise
 */
bool BOP_Mesh::getIndexFace(BOP_Index v1, BOP_Index v2, BOP_Index v3, BOP_Index &f) 
{
	BOP_Index pos=0;
	const BOP_IT_Faces facesEnd = m_faces.end();
	for(BOP_IT_Faces face=m_faces.begin();face!=facesEnd;face++,pos++) {
		if ((*face)->containsVertex(v1) && (*face)->containsVertex(v2) && 
		    (*face)->containsVertex(v3)){
		  f = pos;
		  return true;
		}
	} 
	return false;
}

/**
 * Returns the vertices set of this mesh.
 * @return vertices set
 */
BOP_Vertexs &BOP_Mesh::getVertexs() 
{
	return m_vertexs;
} 

/**
 * Returns the edges set of this mesh.
 * @return edges set
 */
BOP_Edges &BOP_Mesh::getEdges() 
{
	return m_edges;
} 

/**
 * Returns the faces set of this mesh.
 * @return faces set
 */
BOP_Faces &BOP_Mesh::getFaces() 
{
	return m_faces;
} 

/**
 * Returns the mesh vertex with the specified index.
 * @param i vertex index
 * @return vertex with the specified index
 */
BOP_Vertex* BOP_Mesh::getVertex(BOP_Index i)
{
	return m_vertexs[i];
}

/**
 * Returns the mesh edge with the specified index.
 * @param i edge index
 * @return edge with the specified index
 */
BOP_Edge* BOP_Mesh::getEdge(BOP_Index i) 
{
	return m_edges[i];
}

/**
 * Returns the mesh face with the specified index.
 * @param i face index
 * @return face with the specified index
 */
BOP_Face* BOP_Mesh::getFace(BOP_Index i)
{
	return m_faces[i];
}

/**
 * Returns the number of vertices of this mesh.
 * @return number of vertices
 */
unsigned int BOP_Mesh::getNumVertexs() 
{
	return m_vertexs.size();
}

/**
 * Returns the number of edges of this mesh.
 * @return number of edges
 */
unsigned int BOP_Mesh::getNumEdges() 
{
	return m_edges.size();
}

/**
 * Returns the number of faces of this mesh.
 * @return number of faces
 */
unsigned int BOP_Mesh::getNumFaces() 
{
	return m_faces.size();
}

/**
 * Returns the number of vertices of this mesh with the specified tag.
 * @return number of vertices with the specified tag
 */
unsigned int BOP_Mesh::getNumVertexs(BOP_TAG tag) 
{
	unsigned int count = 0;
	const BOP_IT_Vertexs vertexsEnd = m_vertexs.end();
	for(BOP_IT_Vertexs vertex=m_vertexs.begin();vertex!=vertexsEnd;vertex++) {
		if ((*vertex)->getTAG() == tag) count++;
	}
	return count;
}
/**
 * Returns the number of faces of this mesh with the specified tag.
 * @return number of faces with the specified tag
 */
unsigned int BOP_Mesh::getNumFaces(BOP_TAG tag) 
{
	unsigned int count = 0;
	const BOP_IT_Faces facesEnd = m_faces.end();
	for(BOP_IT_Faces face=m_faces.begin();face!=facesEnd;face++) {
		if ((*face)->getTAG() == tag) count++;
	}
	return count;
}

/**
 * Marks faces which bad edges as BROKEN (invalid face, no further processing).
 * @param edge edge which is being replaced
 * @param mesh mesh containing faces
 */

static void removeBrokenFaces( BOP_Edge *edge, BOP_Mesh *mesh )
{
	BOP_Faces m_faces = mesh->getFaces();

	BOP_Indexs edgeFaces = edge->getFaces();
	const BOP_IT_Indexs edgeFacesEnd = edgeFaces.end();
	for(BOP_IT_Indexs idxFace=edgeFaces.begin();idxFace!=edgeFacesEnd;
			   idxFace++)
		m_faces[*idxFace]->setTAG(BROKEN);
}

/**
 * Replaces a vertex index.
 * @param oldIndex old vertex index
 * @param newIndex new vertex index
 */
BOP_Index BOP_Mesh::replaceVertexIndex(BOP_Index oldIndex, BOP_Index newIndex) 
{
	BOP_IT_Indexs oldEdgeIndex;
	if (oldIndex==newIndex) return newIndex;
  
	// Update faces, edges and vertices  
	BOP_Vertex *oldVertex = m_vertexs[oldIndex];
	BOP_Vertex *newVertex = m_vertexs[newIndex];
	BOP_Indexs oldEdges = oldVertex->getEdges();

	// Update faces to the newIndex
	BOP_IT_Indexs oldEdgesEnd = oldEdges.end();
	for(oldEdgeIndex=oldEdges.begin();oldEdgeIndex!=oldEdgesEnd;
		   oldEdgeIndex++) {
		BOP_Edge *edge = m_edges[*oldEdgeIndex];
		if ((edge->getVertex1()==oldIndex && edge->getVertex2()==newIndex) ||
			(edge->getVertex2()==oldIndex && edge->getVertex1()==newIndex)) {
			// Remove old edge  ==> set edge faces to BROKEN      
			removeBrokenFaces( edge, this );
			oldVertex->removeEdge(*oldEdgeIndex);
			newVertex->removeEdge(*oldEdgeIndex);
		}
		else {
			BOP_Indexs faces = edge->getFaces();
			const BOP_IT_Indexs facesEnd = faces.end();
			for(BOP_IT_Indexs face=faces.begin();face!=facesEnd;face++) {
				if (m_faces[*face]->getTAG()!=BROKEN)
					m_faces[*face]->replaceVertexIndex(oldIndex,newIndex);
			}
		}
	} 

	oldEdgesEnd = oldEdges.end();
	for(oldEdgeIndex=oldEdges.begin();oldEdgeIndex!=oldEdgesEnd;
		   oldEdgeIndex++) {
		BOP_Edge * edge = m_edges[*oldEdgeIndex];
		BOP_Edge * edge2;
		BOP_Index v1 = edge->getVertex1();
    
		v1 = (v1==oldIndex?edge->getVertex2():v1);      
		if ((edge2 = getEdge(newIndex,v1)) == NULL) {
			edge->replaceVertexIndex(oldIndex,newIndex);
			if ( edge->getVertex1() == edge->getVertex2() ) {
				removeBrokenFaces( edge, this );
				oldVertex->removeEdge(*oldEdgeIndex);
			}
#ifdef HASH
			rehashVertex(oldIndex,newIndex,v1);
#endif
			newVertex->addEdge(*oldEdgeIndex);
		}
		else {
			BOP_Indexs faces = edge->getFaces();
			const BOP_IT_Indexs facesEnd = faces.end();
			for(BOP_IT_Indexs f=faces.begin();f!=facesEnd;f++) {
				if (m_faces[*f]->getTAG()!=BROKEN)
				edge2->addFace(*f);
			}
			BOP_Vertex *oppositeVertex = m_vertexs[v1];
			oppositeVertex->removeEdge(*oldEdgeIndex);
			edge->replaceVertexIndex(oldIndex,newIndex);
			if ( edge->getVertex1() == edge->getVertex2() ) {
				removeBrokenFaces( edge, this );
				oldVertex->removeEdge(*oldEdgeIndex);
				newVertex->removeEdge(*oldEdgeIndex);
			}
#ifdef HASH
			rehashVertex(oldIndex,newIndex,v1);
#endif
		}
	}
	oldVertex->setTAG(BROKEN);

	return newIndex;
}

bool BOP_Mesh::isClosedMesh()
{
	 for(unsigned int i=0; i<m_edges.size(); i++) {
			 BOP_Edge *edge = m_edges[i];
			 BOP_Indexs faces = edge->getFaces();
			 unsigned int count = 0;
			 const BOP_IT_Indexs facesEnd = faces.end();
			 for(BOP_IT_Indexs it = faces.begin();it!=facesEnd;it++) {
					 if (m_faces[*it]->getTAG()!=BROKEN)
							 count++;
			 }

			 if ((count%2)!=0) return false;
	 }

	 return true;
}


#ifdef BOP_DEBUG
/******************************************************************************
 * DEBUG METHODS                                                              * 
 * This functions are used to test the mesh state and debug program errors.   *
 ******************************************************************************/

/**
 * print
 */
void BOP_Mesh::print() 
{
	unsigned int i;
	cout << "--Faces--" << endl;
	for(i=0;i<m_faces.size();i++){
		cout << "Face " << i << ": " << m_faces[i] << endl;
	}

	cout << "--Vertices--" << endl;
	for(i=0;i<m_vertexs.size();i++){
		cout << "Point " << i << ": " << m_vertexs[i]->getPoint() << endl;
	}
}

/**
 * printFormat
 */
void BOP_Mesh::printFormat(BOP_Faces *faces)
{
	if (faces->size()) {
		for(unsigned int it=1;it<faces->size();it++) {
			if ((*faces)[it]->getTAG()!=BROKEN) {
				cout << m_vertexs[(*faces)[it]->getVertex(0)]->getPoint() << " ";
				cout << m_vertexs[(*faces)[it]->getVertex(1)]->getPoint() << " ";
				cout << m_vertexs[(*faces)[it]->getVertex(2)]->getPoint() << endl;
			}
		}
	}
}

/**
 * saveFormat
 */
void BOP_Mesh::saveFormat(BOP_Faces *faces,char *filename)
{
	ofstream fout(filename);
  
	if (!fout.is_open()) {
		cerr << "BOP_Mesh::saveFormat Error: Could not create file." << endl;
		return;
	}

	unsigned int count = 0;
	if (faces->size()) {
		for(unsigned int it=0;it<faces->size();it++) {
			if ((*faces)[it]->getTAG()!=BROKEN) {
				count++;
			}
		}
	}

	fout << count << endl;
	if (faces->size()) {
		for(unsigned int it=0;it<faces->size();it++) {
			if ((*faces)[it]->getTAG()!=BROKEN){
				fout << m_vertexs[(*faces)[it]->getVertex(0)]->getPoint() << " ";
				fout << m_vertexs[(*faces)[it]->getVertex(1)]->getPoint() << " ";
				fout << m_vertexs[(*faces)[it]->getVertex(2)]->getPoint() << endl;
			}
		}
	}

	fout.close();
}

/**
 * printFormat
 */
void BOP_Mesh::printFormat()
{
	cout << "--Vertices--" << endl;
	if (m_vertexs.size()>0) {
		cout << "{" << m_vertexs[0]->getPoint().x() << ",";
		cout << m_vertexs[0]->getPoint().y() << ",";
		cout << m_vertexs[0]->getPoint().z() << "}";
		for(unsigned int i=1;i<m_vertexs.size();i++) {
			cout << ",{" << m_vertexs[i]->getPoint().x() << ",";
			cout << m_vertexs[i]->getPoint().y() << ",";
			cout << m_vertexs[i]->getPoint().z() << "}";
		}
		cout << endl;
	}

	cout << "--Faces--" << endl;
	if (m_faces.size()>0) {
		cout << "{" << m_faces[0]->getVertex(0) << ",";
		cout << m_faces[0]->getVertex(1) << "," << m_faces[0]->getVertex(2) << "}";
		for(unsigned int i=1;i<m_faces.size();i++) {
			cout << ",{" << m_faces[i]->getVertex(0) << ",";
			cout << m_faces[i]->getVertex(1) << "," << m_faces[i]->getVertex(2) << "}";
		}
		cout << endl;
	}
}

/**
 * printFace
 */
void BOP_Mesh::printFace(BOP_Face *face, int col)
{
  cout << "--Face" << endl;
	cout << m_vertexs[face->getVertex(0)]->getPoint();
	cout << " " << m_vertexs[face->getVertex(1)]->getPoint();
	cout << " " << m_vertexs[face->getVertex(2)]->getPoint();
	if (face->size()==4)
	  cout << " " << m_vertexs[face->getVertex(3)]->getPoint();
	cout << " " << col << endl;
}

/**
 * testMesh
 */
void BOP_Mesh::testMesh()
{

	BOP_Face* cares[10];
	unsigned int nedges=0,i;
	for(i=0;i<m_edges.size();i++) {
		BOP_Edge *edge = m_edges[i];
		BOP_Indexs faces = edge->getFaces();
		unsigned int count = 0;
		const BOP_IT_Indexs facesEnd = faces.end();
		for(BOP_IT_Indexs it = faces.begin();it!=facesEnd;it++) {
			if (m_faces[*it]->getTAG()!=BROKEN) {
				cares[count] = m_faces[*it];
				count++;
				
			}
		}

		if ((count%2)!=0) nedges++;
	}
	if (nedges)
	  cout << nedges << " wrong edges." << endl;
	else
	  cout << "well edges." << endl;

	unsigned int duplFaces = 0;
	unsigned int wrongFaces = 0;
	for(i=0;i<m_faces.size();i++){
	  BOP_Face *faceI = m_faces[i];
	  if (faceI->getTAG()==BROKEN)
	    continue;

	  if (testFace(faceI)){
	    wrongFaces++;
	    cout << "Wrong Face: " << faceI << endl;
	  }

	  for(unsigned int j=i+1;j<m_faces.size();j++){
	    BOP_Face *faceJ = m_faces[j];

	    if (faceJ->getTAG()==BROKEN)
	      continue;

	    if (testFaces(faceI,faceJ)){
	      duplFaces++;
	      cout << "Duplicate FaceI: " << faceI << endl;
	      cout << "Duplicate FaceJ: " << faceJ << endl;
	    }
	  }
	}

	cout << duplFaces << " duplicate faces." << endl;
	cout << wrongFaces << " wrong faces." << endl;
}

/**
 * testFace
 */
bool BOP_Mesh::testFace(BOP_Face *face){
  
  for(unsigned int i=0;i<face->size();i++){
    for(unsigned int j=i+1;j<face->size();j++){
      if (face->getVertex(i)==face->getVertex(j))
	return true;
    }
  }

  return false;
}

/**
 * testFaces
 */
bool BOP_Mesh::testFaces(BOP_Face *faceI, BOP_Face *faceJ){

  if (faceI->size()<faceJ->size()){
    for(unsigned int i=0;i<faceI->size();i++){
      if (!faceJ->containsVertex(faceI->getVertex(i)))
	return false;
    }
    //faceI->setTAG(BROKEN);
  }
  else{
    for(unsigned int i=0;i<faceJ->size();i++){
      if (!faceI->containsVertex(faceJ->getVertex(i)))
	return false;
    }
    //faceJ->setTAG(BROKEN);
  }

  return true;
}

/**
 * testPlane
 */
void BOP_Mesh::testPlane(BOP_Face *face)
{
	MT_Plane3 plane1(m_vertexs[face->getVertex(0)]->getPoint(), 
					 m_vertexs[face->getVertex(1)]->getPoint(),
					 m_vertexs[face->getVertex(2)]->getPoint());

	if (BOP_orientation(plane1,face->getPlane()) < 0) {	  
		cout << "Test Plane " << face << " v1: ";
		cout << m_vertexs[face->getVertex(0)]->getPoint() << " v2: ";
		cout << m_vertexs[face->getVertex(1)]->getPoint() << " v3: ";
		cout << m_vertexs[face->getVertex(2)]->getPoint() << " plane: ";
		cout << face->getPlane() << endl;
		cout << "Incorrect vertices order!!! plane1: " << plane1 << " (";
		cout << BOP_orientation(plane1,face->getPlane()) << ") " <<  " invert ";
		cout <<  MT_Plane3(m_vertexs[face->getVertex(2)]->getPoint(),
						   m_vertexs[face->getVertex(1)]->getPoint(),
						   m_vertexs[face->getVertex(0)]->getPoint()) << endl;
		if (BOP_collinear(m_vertexs[face->getVertex(0)]->getPoint(),
						  m_vertexs[face->getVertex(1)]->getPoint(),
						  m_vertexs[face->getVertex(2)]->getPoint())) {
			cout << " COLLINEAR!!!" << endl;
		}
		else {
			cout << endl;
		}
	}
}

/**
 * testEdges
 */
bool BOP_Mesh::testEdges(BOP_Faces *facesObj)
{
	for(unsigned int i=0;i<m_edges.size();i++) {
		BOP_Edge *edge = m_edges[i];
		BOP_Indexs faces = edge->getFaces();
		unsigned int count = 0;
		const BOP_IT_Indexs facesEnd = faces.end();
		for(BOP_IT_Indexs it = faces.begin();it!=facesEnd;it++) {
			if ((m_faces[*it]->getTAG()!=BROKEN) && containsFace(facesObj,m_faces[*it]))
				count++;
		}
		if ((count%2)!=0) {
			return false;
		}
	}
	
	return true;
}

/**
 * updatePlanes
 */
void BOP_Mesh::updatePlanes() 
{
  const BOP_IT_Faces facesEnd = m_faces.end();
	for(BOP_IT_Faces it = m_faces.begin();it!=facesEnd;it++) {
	  BOP_Face *face = *it;
	  MT_Plane3 plane(m_vertexs[face->getVertex(0)]->getPoint(), 
			  m_vertexs[face->getVertex(1)]->getPoint(),
			  m_vertexs[face->getVertex(2)]->getPoint());
	  face->setPlane(plane);
	}
}

#endif

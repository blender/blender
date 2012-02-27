/*
 *
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
 * Contributor(s): Marc Freixas, Ken Hughes
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file boolop/intern/BOP_Merge2.cpp
 *  \ingroup boolopintern
 */

 
#include "BOP_Merge2.h"

#ifdef BOP_NEW_MERGE

static void deleteFace(BOP_Mesh *m, BOP_Face *face);

/**
 * SINGLETON (use method BOP_Merge2.getInstance).
 */
BOP_Merge2 BOP_Merge2::SINGLETON;

#ifdef BOP_DEBUG
void dumpmesh ( BOP_Mesh *m, bool force )
{
	unsigned int nonmanifold = 0;
	{
	BOP_Edges edges = m->getEdges();
	int count = 0;
    for (BOP_IT_Edges edge = edges.begin(); edge != edges.end();
		++count, ++edge) {
		if (!(*edge)->getUsed() && (*edge)->getFaces().size() == 0 ) continue;
		BOP_Vertex * v1 = m->getVertex((*edge)->getVertex1());
		BOP_Vertex * v2 = m->getVertex((*edge)->getVertex2());

		if(v1->getTAG()!= BROKEN || v2->getTAG()!= BROKEN ) {
			int fcount = 0;
			BOP_Indexs faces = (*edge)->getFaces();
			for (BOP_IT_Indexs face = faces.begin(); face != faces.end(); face++) {
				BOP_Face *f = m->getFace(*face);
				if(f->getTAG()== UNCLASSIFIED) ++fcount;
			}


			if(fcount !=0 && fcount !=2 ) {
				++nonmanifold;
			}
		}
	}
	if (!force && nonmanifold == 0) return;
	}
	if( nonmanifold )
		cout << nonmanifold << " edges detected" << endl;
#ifdef BOP_DEBUG
	cout << "---------------------------" << endl;

	BOP_Edges edges = m->getEdges();
	int count = 0;
    for (BOP_IT_Edges edge = edges.begin(); edge != edges.end();
		++count, ++edge) {
		BOP_Vertex * v1 = m->getVertex((*edge)->getVertex1());
		BOP_Vertex * v2 = m->getVertex((*edge)->getVertex2());

		if(v1->getTAG()!= BROKEN || v2->getTAG()!= BROKEN ) {
			int fcount = 0;
			BOP_Indexs faces = (*edge)->getFaces();
			cout << count << ", " << (*edge) << ", " << faces.size() << endl;
			for (BOP_IT_Indexs face = faces.begin(); face != faces.end(); face++) {
				BOP_Face *f = m->getFace(*face);
				if(f->getTAG()== UNCLASSIFIED) ++fcount;
				cout << "  face " << f << endl;
			}


			if(fcount !=0 && fcount !=2 )
				cout << "    NON-MANIFOLD" << endl;
		}
	}

	BOP_Faces faces = m->getFaces();
	count = 0;
    for (BOP_IT_Faces face = faces.begin(); face != faces.end(); face++) {
		if( count < 12*2 || (*face)->getTAG() != BROKEN ) {
			cout << count << ", " << *face << endl;
		}
		++count;
	}

	BOP_Vertexs verts = m->getVertexs();
	count = 0;
    for (BOP_IT_Vertexs vert = verts.begin(); vert != verts.end(); vert++) {
		cout << count++ << ", " << *vert << " " << (*vert)->getNumEdges() << endl;
		BOP_Indexs edges = (*vert)->getEdges();
	    for( BOP_IT_Indexs it = edges.begin(); it != edges.end(); ++it) {
			BOP_Edge *edge = m->getEdge(*it);
			cout << "   " << edge << endl;
		}
	}
	cout << "===========================" << endl;
#endif
}
#endif

/**
 * Simplifies a mesh, merging its faces.
 * @param m mesh
 * @param v index of the first mergeable vertex (can be removed by merge) 
 */

void BOP_Merge2::mergeFaces(BOP_Mesh *m, BOP_Index v)
{
	m_mesh = m;

#ifdef BOP_DEBUG
	cout << "##############################" << endl;
#endif
	cleanup( );

	m_firstVertex = v;
	bool cont = false;

	// Merge faces
	mergeFaces();	

	do {
		// Add quads ...
		cont = createQuads();
		// ... and merge new faces
		if( cont ) cont = mergeFaces();

#ifdef BOP_DEBUG
		cout << "called mergeFaces " << cont << endl;
#endif
		// ... until the merge is not succesful
	} while(cont);
}

void clean_nonmanifold( BOP_Mesh *m )
{
	return;

	BOP_Edges nme;
	BOP_Edges e = m->getEdges();
	for( BOP_IT_Edges it = e.begin(); it != e.end(); ++it ) {
		BOP_Indexs faces = (*it)->getFaces();
		if( faces.size() & ~2 )
			nme.push_back(*it);
	}
	if (nme.size() == 0) return;
	for( BOP_IT_Edges it = nme.begin(); it != nme.end(); ++it ) {
		if( (*it)->getFaces().size() > 1 ) {
			BOP_Indexs faces = (*it)->getFaces();
			for( BOP_IT_Indexs face = faces.begin(); face != faces.end(); ++face ) {
				MT_Point3 vertex1 = m->getVertex(m->getFace(*face)->getVertex(0))->getPoint();
				MT_Point3 vertex2 = m->getVertex(m->getFace(*face)->getVertex(1))->getPoint();
				MT_Point3 vertex3 = m->getVertex(m->getFace(*face)->getVertex(2))->getPoint();
				if (BOP_collinear(vertex1,vertex2,vertex3)) // collinear triangle
					deleteFace(m,m->getFace(*face));
			}
			continue;
		}
		BOP_Face *oface1 = m->getFace((*it)->getFaces().front());
		BOP_Face *oface2, *tmpface;
		BOP_Index first =(*it)->getVertex1();
		BOP_Index next =(*it)->getVertex2();
		BOP_Index last = first;
		unsigned short facecount = 0;
		bool found = false;
		BOP_Indexs vertList;
#ifdef BOP_DEBUG
		cout << "  first edge is " << (*it) << endl;
#endif
		vertList.push_back(first);
		BOP_Edge *edge;
		while(true) {
			BOP_Vertex *vert = m->getVertex(next);
			BOP_Indexs edges = vert->getEdges();
			edge = NULL;
			for( BOP_IT_Indexs eit = edges.begin(); eit != edges.end(); ++eit) {
				edge = m->getEdge(*eit);
				if( edge->getFaces().size() > 1) {
					edge = NULL;
					continue;
				}
				if( edge->getVertex1() == next && edge->getVertex2() != last ) {
					last = next;
					next = edge->getVertex2();
					break;
				}
				if( edge->getVertex2() == next && edge->getVertex1() != last ) {
					last = next;
					next = edge->getVertex1();
					break;
				}
				edge = NULL;
			}
			if( !edge ) break;
#ifdef BOP_DEBUG
			cout << "   next edge is " << edge << endl;
#endif
			tmpface = m->getFace(edge->getFaces().front());
			if( oface1->getOriginalFace() != tmpface->getOriginalFace() )
				oface2 = tmpface;
			else
				++facecount;
			vertList.push_back(last);
			if( vertList.size() > 3 ) break;
			if( next == first ) {
				found = true;
				break;
			}
		}
		if(found) {
			edge = *it;
#ifdef BOP_DEBUG
			cout << "   --> found a loop" << endl;
#endif
			if( vertList.size() == 3 ) {
				BOP_Face3 *face = (BOP_Face3 *)m->getFace(edge->getFaces().front());
				face->getNeighbours(first,last,next);
			} else if( vertList.size() == 4 ) {
				BOP_Face4 *face = (BOP_Face4 *)m->getFace(edge->getFaces().front());
				face->getNeighbours(first,last,next,last);
			} else {
#ifdef BOP_DEBUG
				cout << "loop has " << vertList.size() << "verts"; 
#endif
				continue;
			}
			if(facecount == 1) oface1 = oface2;
			next = vertList[1];
			last = vertList[2];
			if( edge->getVertex2() == next ) { 
				BOP_Face3 *f = new BOP_Face3(next,first,last,
					oface1->getPlane(),oface1->getOriginalFace());
				m->addFace( f );
#ifdef BOP_DEBUG
				cout << "   face is backward: " << f << endl;
#endif
				
			} else {
				BOP_Face3 *f = new BOP_Face3(last,first,next,
					oface1->getPlane(),oface1->getOriginalFace());
				m->addFace( f );
#ifdef BOP_DEBUG
				cout << "   face is forward: " << f << endl;
#endif
			}
		}
	}
}

/**
 * Runs through mesh and makes sure vert/face/edge data is consistent.  Most
 * importantly:
 * (1) mark edges which are no longer used
 * (2) remove broken faces from edges
 * (3) remove faces from mesh which have a single edge belonging to no other
 *     face (non-manifold edges)
 */

void BOP_Merge2::cleanup( void )
{
	BOP_Edges edges = m_mesh->getEdges();
	for (BOP_IT_Edges edge = edges.begin(); edge != edges.end(); ++edge) {
		BOP_Indexs faces = (*edge)->getFaces();
		for (BOP_IT_Indexs face = faces.begin(); face != faces.end(); ++face) {
			BOP_Face *f = m_mesh->getFace(*face);
			if (f->getTAG()== UNCLASSIFIED);
			else (*edge)->removeFace(*face);
		}
		if( (*edge)->getFaces().size() == 0) (*edge)->setUsed(false);
	}

	BOP_Vertexs v = m_mesh->getVertexs();
	for( BOP_IT_Vertexs it = v.begin(); it != v.end(); ++it ) {
		if( (*it)->getTAG() != BROKEN) {
			BOP_Indexs iedges = (*it)->getEdges();
			for(BOP_IT_Indexs i = iedges.begin();i!=iedges.end();i++)
				if( m_mesh->getEdge((*i))->getUsed( ) == false) (*it)->removeEdge( *i );
			if( (*it)->getEdges().size() == 0 ) (*it)->setTAG(BROKEN);
		}
	}
	// clean_nonmanifold( m_mesh );
}

/**
 * Simplifies a mesh, merging its faces.
 */
bool BOP_Merge2::mergeFaces()
{
	BOP_Indexs mergeVertices;
	BOP_Vertexs vertices = m_mesh->getVertexs();
	BOP_IT_Vertexs v = vertices.begin();
	const BOP_IT_Vertexs verticesEnd = vertices.end();

	// Advance to first mergeable vertex
	advance(v,m_firstVertex);
	BOP_Index pos = m_firstVertex;

	// Add unbroken vertices to the list
	while(v!=verticesEnd) {
		if ((*v)->getTAG() != BROKEN) {
			mergeVertices.push_back(pos);
		}

		v++;
		pos++;
	}

	// Merge faces with that vertices
	return mergeFaces(mergeVertices);
}

/**
 * remove edges from vertices when the vertex is removed
 */
void BOP_Merge2::freeVerts(BOP_Index v, BOP_Vertex *vert)
{
	BOP_Indexs edges = vert->getEdges();
	BOP_Vertex *other;

	for( BOP_IT_Indexs it = edges.begin(); it != edges.end(); ++it) {
		BOP_Edge *edge = m_mesh->getEdge(*it);
		BOP_Indexs edges2;
		if( edge->getVertex1() != v )
			other = m_mesh->getVertex( edge->getVertex1() );
		else
			other = m_mesh->getVertex( edge->getVertex2() );
		other->removeEdge(*it);
		vert->removeEdge(*it);
	}
}

/**
 * Simplifies a mesh, merging the faces with the specified vertices.
 * @param mergeVertices vertices to test
 * @return true if a face merge was performed
 */
bool BOP_Merge2::mergeFaces(BOP_Indexs &mergeVertices)
{
	// Check size > 0!
	if (mergeVertices.size() == 0) return false;
	bool didMerge = false;

	for( BOP_Index i = 0; i < mergeVertices.size(); ++i ) {
		BOP_LFaces facesByOriginalFace;
		BOP_Index v = mergeVertices[i];
		BOP_Vertex *vert = m_mesh->getVertex(v);
#ifdef BOP_DEBUG
		cout << "i = " << i << ", v = " << v << ", vert = " << vert << endl;
		if (v==48)
			cout << "found vert 48" << endl;
#endif
		if ( vert->getTAG() != BROKEN ) {
			getFaces(facesByOriginalFace,v);

			switch (facesByOriginalFace.size()) {
			case 0:
				// v has no unbroken faces (so it's a new BROKEN vertex)
				freeVerts( v, vert );
				vert->setTAG(BROKEN);
				break;
			case 2: {
#ifdef BOP_DEBUG
				cout << "size of fBOF = " << facesByOriginalFace.size() << endl;
#endif
				BOP_Faces ff = facesByOriginalFace.front();
				BOP_Faces fb = facesByOriginalFace.back();
				BOP_Index eindexs[2];
				int ecount = 0;

				// look for two edges adjacent to v which contain both ofaces
				BOP_Indexs edges = vert->getEdges();
#ifdef BOP_DEBUG
				cout << "   ff has " << ff.size() << " faces" << endl;
				cout << "   fb has " << fb.size() << " faces" << endl;
				cout << "   v  has " << edges.size() << " edges" << endl;
#endif
				for(BOP_IT_Indexs it = edges.begin(); it != edges.end(); 
						++it ) {
					BOP_Edge *edge = m_mesh->getEdge(*it);
					BOP_Indexs faces = edge->getFaces();
#ifdef BOP_DEBUG
					cout << "  " << edge << " has " << edge->getFaces().size() << " faces" << endl;
#endif
					if( faces.size() == 2 ) {
						BOP_Face *f0 = m_mesh->getFace(faces[0]);
						BOP_Face *f1 = m_mesh->getFace(faces[1]);
						if( f0->getOriginalFace() != f1->getOriginalFace() ) {
#ifdef BOP_DEBUG
							cout << "   " << f0 << endl;
							cout << "   " << f1 << endl;
#endif
							eindexs[ecount++] = (*it);
						}
					}
				}
				if(ecount == 2) {
#ifdef BOP_DEBUG
					cout << "   edge indexes are " << eindexs[0];
					cout << " and " << eindexs[1] << endl;
#endif
					BOP_Edge *edge = m_mesh->getEdge(eindexs[0]);
					BOP_Index N = edge->getVertex1();
					if(N == v) N = edge->getVertex2();
#ifdef BOP_DEBUG
					cout << "    ## OK, replace "<<v<<" with "<<N << endl;
#endif
					mergeVertex(ff , v, N );
					mergeVertex(fb , v, N );
// now remove v and its edges
					vert->setTAG(BROKEN);
					for(BOP_IT_Indexs it = edges.begin(); it != edges.end(); 
							++it ) {
						BOP_Edge *tedge = m_mesh->getEdge(*it);
						tedge->setUsed(false);
					}
					didMerge = true;
				}	
#ifdef BOP_DEBUG
				else {
					cout << "   HUH: ecount was " << ecount << endl;
				}
#endif
				}
				break;
			default:
				break;
			}
		}
	}

	return didMerge;
}

void BOP_Merge2::mergeVertex(BOP_Faces &faces, BOP_Index v1, BOP_Index v2)
{
	for(BOP_IT_Faces face=faces.begin();face!=faces.end();face++) {
		if( (*face)->size() == 3)
			mergeVertex((BOP_Face3 *) *face, v1, v2);
		else
			mergeVertex((BOP_Face4 *) *face, v1, v2);
		(*face)->setTAG(BROKEN);
#ifdef BOP_DEBUG
		cout << "  breaking " << (*face) << endl;
#endif
	}
}

/*
 * Remove a face from the mesh and from each edges's face list
 */

static void deleteFace(BOP_Mesh *m, BOP_Face *face)
{
	BOP_Index l2 = face->getVertex(0);
	BOP_Faces faces = m->getFaces();
	for(int i = face->size(); i-- ; ) {
		BOP_Indexs edges = m->getVertex(l2)->getEdges();
		BOP_Index l1 = face->getVertex(i);
		for(BOP_IT_Indexs it1 = edges.begin(); it1 != edges.end(); ++it1 ) {
			BOP_Edge *edge = m->getEdge(*it1);
			if( ( edge->getVertex1() == l1 && edge->getVertex2() == l2 ) ||
				( edge->getVertex1() == l2 && edge->getVertex2() == l1 ) ) {
				BOP_Indexs ef = edge->getFaces();
				for(BOP_IT_Indexs it = ef.begin(); it != ef.end(); ++it ) {
					if( m->getFace(*it) == face) {
						edge->removeFace(*it);
						break;
					}
				}
				break;
			}
		}
		l2 = l1;
	}
	face->setTAG(BROKEN);
}

void BOP_Merge2::mergeVertex(BOP_Face3 *face, BOP_Index v1, BOP_Index v2)
{
	BOP_Index next, prev;
	face->getNeighbours(v1,prev,next);

	// if new vertex is not already in the tri, make a new tri
	if( prev != v2 && next != v2 ) {
		m_mesh->addFace( new BOP_Face3(prev,v2,next,
					face->getPlane(),face->getOriginalFace()) );
#ifdef BOP_DEBUG
		cout << "mv3: add " << prev << "," << v2 << "," << next << endl;
	} else {
		cout << "mv3: vertex already in tri: doing nothing" << endl;
#endif
	}
	deleteFace(m_mesh, face);
}

void BOP_Merge2::mergeVertex(BOP_Face4 *face, BOP_Index v1, BOP_Index v2)
{
	BOP_Index next, prev, opp;
	face->getNeighbours(v1,prev,next,opp);

	// if new vertex is already in the quad, replace quad with new tri
	if( prev == v2 || next == v2 ) {
		m_mesh->addFace( new BOP_Face3(prev,next,opp,
					face->getPlane(),face->getOriginalFace()) );
#ifdef BOP_DEBUG
		cout << "mv4a: add " << prev << "," << next << "," << opp << endl;
#endif
	}
	// otherwise make a new quad
	else {
		m_mesh->addFace( new BOP_Face4(prev,v2,next,opp,
					face->getPlane(),face->getOriginalFace()) );
#ifdef BOP_DEBUG
		cout << "mv4b: add "<<prev<<","<<v2<<","<<next<<","<<opp<<endl;
#endif
	}
	deleteFace(m_mesh, face);
}

// #define OLD_QUAD

/** 
 * Simplifies the mesh, merging the pairs of triangles that come frome the
 * same original face and define a quad.
 * @return true if a quad was added, false otherwise
 */
bool BOP_Merge2::createQuads()
{
  
	BOP_Faces quads;
	
	// Get mesh faces
	BOP_Faces faces = m_mesh->getFaces();
	
    // Merge mesh triangles
	const BOP_IT_Faces facesIEnd = (faces.end()-1);
	const BOP_IT_Faces facesJEnd = faces.end();
	for(BOP_IT_Faces faceI=faces.begin();faceI!=facesIEnd;faceI++) {
#ifdef OLD_QUAD
		if ((*faceI)->getTAG() == BROKEN || (*faceI)->size() != 3) continue;
		for(BOP_IT_Faces faceJ=(faceI+1);faceJ!=facesJEnd;faceJ++) {
			if ((*faceJ)->getTAG() == BROKEN || (*faceJ)->size() != 3 ||
				(*faceJ)->getOriginalFace() != (*faceI)->getOriginalFace()) continue;


			BOP_Face *faceK = createQuad((BOP_Face3*)*faceI,(BOP_Face3*)*faceJ);
			if (faceK != NULL) {
				// Set triangles to BROKEN
				deleteFace(m_mesh, *faceI);
				deleteFace(m_mesh, *faceJ);
#ifdef BOP_DEBUG
			cout << "createQuad: del " << *faceI << endl;
			cout << "createQuad: del " << *faceJ << endl;
			cout << "createQuad: add " << faceK << endl;
#endif
				quads.push_back(faceK);
				break;
			}
		}
#else
		if ((*faceI)->getTAG() == BROKEN ) continue;
		for(BOP_IT_Faces faceJ=(faceI+1);faceJ!=facesJEnd;faceJ++) {
			if ((*faceJ)->getTAG() == BROKEN ||
				(*faceJ)->getOriginalFace() != (*faceI)->getOriginalFace()) continue;

			BOP_Face *faceK = NULL;
			if((*faceI)->size() == 3) {
				if((*faceJ)->size() == 3)
					faceK = createQuad((BOP_Face3*)*faceI,(BOP_Face3*)*faceJ);
				else
					faceK = createQuad((BOP_Face3*)*faceI,(BOP_Face4*)*faceJ);
			} else {
				if((*faceJ)->size() == 3)
					faceK = createQuad((BOP_Face3*)*faceJ,(BOP_Face4*)*faceI);
				else
					faceK = createQuad((BOP_Face4*)*faceI,(BOP_Face4*)*faceJ);
			}

			if (faceK != NULL) {
				// Set triangles to BROKEN
				deleteFace(m_mesh, *faceI);
				deleteFace(m_mesh, *faceJ);
#ifdef BOP_DEBUG
			cout << "createQuad: del " << *faceI << endl;
			cout << "createQuad: del " << *faceJ << endl;
			cout << "createQuad: add " << faceK << endl;
#endif
				quads.push_back(faceK);
				break;
			}
		}
#endif
	}

    // Add quads to mesh
	const BOP_IT_Faces quadsEnd = quads.end();
	for(BOP_IT_Faces quad=quads.begin();quad!=quadsEnd;quad++) m_mesh->addFace(*quad);
	return (quads.size() > 0);
}

/** 
 * Returns a new quad (convex) from the merge of two triangles that share the
 * vertex index v.
 * @param faceI mesh triangle
 * @param faceJ mesh triangle
 * @param v vertex index shared by both triangles
 * @return a new convex quad if the merge is possible
 */
BOP_Face* BOP_Merge2::createQuad(BOP_Face3 *faceI, BOP_Face3 *faceJ)
{
	// Test if both triangles share a vertex index
	BOP_Index v;
	unsigned int i;
	for(i=0;i<3 ;i++) {
		v = faceI->getVertex(i);
		if( faceJ->containsVertex(v) ) break;
	}
	if (i == 3) return NULL;

	BOP_Face *faceK = NULL;

	// Get faces data
	BOP_Index prevI, nextI, prevJ, nextJ;
	faceI->getNeighbours(v,prevI,nextI);
	faceJ->getNeighbours(v,prevJ,nextJ);
	MT_Point3 vertex = m_mesh->getVertex(v)->getPoint();
	MT_Point3 vPrevI = m_mesh->getVertex(prevI)->getPoint();
	MT_Point3 vNextI = m_mesh->getVertex(nextI)->getPoint();
	MT_Point3 vPrevJ = m_mesh->getVertex(prevJ)->getPoint();
	MT_Point3 vNextJ = m_mesh->getVertex(nextJ)->getPoint();

	// Quad test
	if (prevI == nextJ) {
		if (!BOP_collinear(vNextI,vertex,vPrevJ) && !BOP_collinear(vNextI,vPrevI,vPrevJ) &&
			BOP_convex(vertex,vNextI,vPrevI,vPrevJ)) {
				faceK = new BOP_Face4(v,nextI,prevI,prevJ,faceI->getPlane(),faceI->getOriginalFace());
				faceK->setTAG(faceI->getTAG());
				BOP_Index edge;
				m_mesh->getIndexEdge(v,prevI,edge);
				m_mesh->getVertex(v)->removeEdge(edge);
				m_mesh->getVertex(prevI)->removeEdge(edge);
		}
	}
	else if (nextI == prevJ) {
		if (!BOP_collinear(vPrevI,vertex,vNextJ) && !BOP_collinear(vPrevI,vNextI,vNextJ) &&
			BOP_convex(vertex,vNextJ,vNextI,vPrevI)) {
				faceK = new BOP_Face4(v,nextJ,nextI,prevI,faceI->getPlane(),faceI->getOriginalFace());
				faceK->setTAG(faceI->getTAG());
				BOP_Index edge;
				m_mesh->getIndexEdge(v,nextI,edge);
				m_mesh->getVertex(v)->removeEdge(edge);
				m_mesh->getVertex(nextI)->removeEdge(edge);
			}
	}
	return faceK;
}

/** 
 * Returns a new quad (convex) from the merge of two triangles that share the
 * vertex index v.
 * @param faceI mesh triangle
 * @param faceJ mesh triangle
 * @param v vertex index shared by both triangles
 * @return a new convex quad if the merge is possible
 */
BOP_Face* BOP_Merge2::createQuad(BOP_Face3 *faceI, BOP_Face4 *faceJ)
{
	// Test if triangle and quad share a vertex index
	BOP_Index v;
	unsigned int i;
	for(i=0;i<3 ;i++) {
		v = faceI->getVertex(i);
		if( faceJ->containsVertex(v) ) break;
	}
	if (i == 3) return NULL;

	BOP_Face *faceK = NULL;

	// Get faces data
	BOP_Index prevI, nextI, prevJ, nextJ, oppJ;
	faceI->getNeighbours(v,prevI,nextI);
	faceJ->getNeighbours(v,prevJ,nextJ,oppJ);

	// Quad test
	BOP_Index edge;
	if (nextI == prevJ) {
		if (prevI == nextJ) {	// v is in center
			faceK = new BOP_Face3(nextJ,oppJ,prevJ,faceI->getPlane(),faceI->getOriginalFace());
			faceK->setTAG(faceI->getTAG());
			m_mesh->getIndexEdge(v,prevI,edge);
			m_mesh->getVertex(v)->removeEdge(edge);
			m_mesh->getVertex(prevI)->removeEdge(edge);
			m_mesh->getIndexEdge(v,nextI,edge);
			m_mesh->getVertex(v)->removeEdge(edge);
			m_mesh->getVertex(nextI)->removeEdge(edge);
			freeVerts(v, m_mesh->getVertex(v));
		} else if (prevI == oppJ) {	// nextI is in center
			faceK = new BOP_Face3(v,nextJ,oppJ,faceI->getPlane(),faceI->getOriginalFace());
			faceK->setTAG(faceI->getTAG());
			m_mesh->getIndexEdge(v,nextI,edge);
			m_mesh->getVertex(v)->removeEdge(edge);
			m_mesh->getVertex(nextI)->removeEdge(edge);
			m_mesh->getIndexEdge(prevI,nextI,edge);
			m_mesh->getVertex(prevI)->removeEdge(edge);
			m_mesh->getVertex(nextI)->removeEdge(edge);
			freeVerts(nextI, m_mesh->getVertex(nextI));
		}
	} else if (nextI == oppJ && prevI == nextJ ) { // prevI is in center
		faceK = new BOP_Face3(prevJ,v,oppJ,faceI->getPlane(),faceI->getOriginalFace());
		faceK->setTAG(faceI->getTAG());
		m_mesh->getIndexEdge(v,prevI,edge);
		m_mesh->getVertex(v)->removeEdge(edge);
		m_mesh->getVertex(prevI)->removeEdge(edge);
		m_mesh->getIndexEdge(nextI,prevI,edge);
		m_mesh->getVertex(nextI)->removeEdge(edge);
		m_mesh->getVertex(prevI)->removeEdge(edge);
		freeVerts(prevI, m_mesh->getVertex(prevI));
	}
	return faceK;
}

/** 
 * Returns a new quad (convex) from the merge of two triangles that share the
 * vertex index v.
 * @param faceI mesh triangle
 * @param faceJ mesh triangle
 * @param v vertex index shared by both triangles
 * @return a new convex quad if the merge is possible
 */
BOP_Face* BOP_Merge2::createQuad(BOP_Face4 *faceI, BOP_Face4 *faceJ)
{
	BOP_Face *faceK = NULL;
	//
	// Test if both quads share a vertex index
	//
	BOP_Index v;
	unsigned int i;
	for(i=0;i<4 ;i++) {
		v = faceI->getVertex(i);
		if( faceJ->containsVertex(v) ) break;
	}
	if (i == 3) return NULL;


	// Get faces data
	BOP_Index prevI, nextI, oppI, prevJ, nextJ, oppJ;
	faceI->getNeighbours(v,prevI,nextI,oppI);
	faceJ->getNeighbours(v,prevJ,nextJ,oppJ);

	// Quad test
	BOP_Index edge;
	if (nextI == prevJ) {
		if (prevI == nextJ) {	// v is in center
			faceK = new BOP_Face4(nextI,oppI,nextJ,oppJ,faceI->getPlane(),faceI->getOriginalFace());
			faceK->setTAG(faceI->getTAG());
			m_mesh->getIndexEdge(v,prevI,edge);
			m_mesh->getVertex(v)->removeEdge(edge);
			m_mesh->getVertex(prevI)->removeEdge(edge);
			m_mesh->getIndexEdge(v,nextI,edge);
			m_mesh->getVertex(v)->removeEdge(edge);
			m_mesh->getVertex(nextI)->removeEdge(edge);
			freeVerts(v, m_mesh->getVertex(v));
		} else if (oppI == oppJ) {	// nextI is in center
			faceK = new BOP_Face4(v,nextJ,oppJ,prevI,faceI->getPlane(),faceI->getOriginalFace());
			faceK->setTAG(faceI->getTAG());
			m_mesh->getIndexEdge(v,nextI,edge);
			m_mesh->getVertex(v)->removeEdge(edge);
			m_mesh->getVertex(nextI)->removeEdge(edge);
			m_mesh->getIndexEdge(prevI,nextI,edge);
			m_mesh->getVertex(prevI)->removeEdge(edge);
			m_mesh->getVertex(nextI)->removeEdge(edge);
			freeVerts(nextI, m_mesh->getVertex(nextI));
		}
	} else if (prevI == nextJ && oppI == oppJ) { // prevI is in center
		faceK = new BOP_Face4(v,nextI,oppJ,prevJ,faceI->getPlane(),faceI->getOriginalFace());
		faceK->setTAG(faceI->getTAG());
		m_mesh->getIndexEdge(v,prevI,edge);
		m_mesh->getVertex(v)->removeEdge(edge);
		m_mesh->getVertex(prevI)->removeEdge(edge);
		m_mesh->getIndexEdge(nextI,prevI,edge);
		m_mesh->getVertex(nextI)->removeEdge(edge);
		m_mesh->getVertex(prevI)->removeEdge(edge);
		freeVerts(prevI, m_mesh->getVertex(prevI));
	}
	return faceK;
}

/**
 * Returns if a index is inside a set of indexs.
 * @param indexs set of indexs
 * @param i index
 * @return true if the index is inside the set, false otherwise
 */
bool BOP_Merge2::containsIndex(BOP_Indexs indexs, BOP_Index i)
{
  const BOP_IT_Indexs indexsEnd = indexs.end();
	for(BOP_IT_Indexs it=indexs.begin();it!=indexsEnd;it++) {
		if (*it == i) return true;
	}
	return false;
}

/**
 * Creates a list of lists L1, L2, ... LN where
 *   LX = mesh faces with vertex v that come from the same original face
 * @param facesByOriginalFace list of faces lists
 * @param v vertex index
 */
void BOP_Merge2::getFaces(BOP_LFaces &facesByOriginalFace, BOP_Index v)
{
	// Get edges with vertex v

	BOP_Indexs edgeIndexs = m_mesh->getVertex(v)->getEdges();
	const BOP_IT_Indexs edgeEnd = edgeIndexs.end();
	for(BOP_IT_Indexs edgeIndex = edgeIndexs.begin();edgeIndex != edgeEnd;edgeIndex++) {	
		// For each edge, add its no broken faces to the output list
		BOP_Edge* edge = m_mesh->getEdge(*edgeIndex);
		BOP_Indexs faceIndexs = edge->getFaces();
		const BOP_IT_Indexs faceEnd = faceIndexs.end();
		for(BOP_IT_Indexs faceIndex=faceIndexs.begin();faceIndex!=faceEnd;faceIndex++) {
			BOP_Face* face = m_mesh->getFace(*faceIndex);
			if (face->getTAG() != BROKEN) {
				bool found = false;
				// Search if we already have created a list for the 
				// faces that come from the same original face
				const BOP_IT_LFaces lfEnd = facesByOriginalFace.end();
				for(BOP_IT_LFaces facesByOriginalFaceX=facesByOriginalFace.begin();
				facesByOriginalFaceX!=lfEnd; facesByOriginalFaceX++) {
					if (((*facesByOriginalFaceX)[0])->getOriginalFace() == face->getOriginalFace()) {
						// Search that the face has not been added to the list before
						for(unsigned int i = 0;i<(*facesByOriginalFaceX).size();i++) {
							if ((*facesByOriginalFaceX)[i] == face) {
								found = true;
								break;
							}
						}
						if (!found) {
							// Add the face to the list
						  if (face->getTAG()==OVERLAPPED) facesByOriginalFaceX->insert(facesByOriginalFaceX->begin(),face);
						  else facesByOriginalFaceX->push_back(face);
						  found = true;
						}
						break;
					}
				}
				if (!found) {
					// Create a new list and add the current face
					BOP_Faces facesByOriginalFaceX;
					facesByOriginalFaceX.push_back(face);
					facesByOriginalFace.push_back(facesByOriginalFaceX);
				}
			}
		}
	}
}

/**
 * Creates a list of lists L1, L2, ... LN where
 *   LX = mesh faces with vertex v that come from the same original face
 *        and without any of the vertices that appear before v in vertices
 * @param facesByOriginalFace list of faces lists
 * @param vertices vector with vertices indexs that contains v
 * @param v vertex index
 */
void BOP_Merge2::getFaces(BOP_LFaces &facesByOriginalFace, BOP_Indexs vertices, BOP_Index v)
{
	// Get edges with vertex v
	BOP_Indexs edgeIndexs = m_mesh->getVertex(v)->getEdges();
	const BOP_IT_Indexs edgeEnd = edgeIndexs.end();
	for(BOP_IT_Indexs edgeIndex = edgeIndexs.begin();edgeIndex != edgeEnd;edgeIndex++) {	
		// Foreach edge, add its no broken faces to the output list
		BOP_Edge* edge = m_mesh->getEdge(*edgeIndex);
		BOP_Indexs faceIndexs = edge->getFaces();
		const BOP_IT_Indexs faceEnd = faceIndexs.end();
		for(BOP_IT_Indexs faceIndex=faceIndexs.begin();faceIndex!=faceEnd;faceIndex++) {
			BOP_Face* face = m_mesh->getFace(*faceIndex);
			if (face->getTAG() != BROKEN) {
				// Search if the face contains any of the forbidden vertices
				bool found = false;
				for(BOP_IT_Indexs vertex = vertices.begin();*vertex!= v;vertex++) {
					if (face->containsVertex(*vertex)) {
						// face contains a forbidden vertex!
						found = true;
						break;
				}
			}
			if (!found) {
				// Search if we already have created a list with the 
				// faces that come from the same original face
			  const BOP_IT_LFaces lfEnd = facesByOriginalFace.end();
				for(BOP_IT_LFaces facesByOriginalFaceX=facesByOriginalFace.begin();
					facesByOriginalFaceX!=lfEnd; facesByOriginalFaceX++) {
					if (((*facesByOriginalFaceX)[0])->getOriginalFace() == face->getOriginalFace()) {
						// Search that the face has not been added to the list before
						for(unsigned int i = 0;i<(*facesByOriginalFaceX).size();i++) {
							if ((*facesByOriginalFaceX)[i] == face) {
								found = true;
								break;
							}
						}
						if (!found) {
						  // Add face to the list
						  if (face->getTAG()==OVERLAPPED) facesByOriginalFaceX->insert(facesByOriginalFaceX->begin(),face);
						  else facesByOriginalFaceX->push_back(face);
						  found = true;
						}
						break;
					}
				}
				if (!found) {
					// Create a new list and add the current face
					BOP_Faces facesByOriginalFaceX;
					facesByOriginalFaceX.push_back(face);
					facesByOriginalFace.push_back(facesByOriginalFaceX);
				}
			}
		}
	}
	}
}

#endif  /* BOP_NEW_MERGE */

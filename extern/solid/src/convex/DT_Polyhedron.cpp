/*
 * SOLID - Software Library for Interference Detection
 * 
 * Copyright (C) 2001-2003  Dtecta.  All rights reserved.
 *
 * This library may be distributed under the terms of the Q Public License
 * (QPL) as defined by Trolltech AS of Norway and appearing in the file
 * LICENSE.QPL included in the packaging of this file.
 *
 * This library may be distributed and/or modified under the terms of the
 * GNU General Public License (GPL) version 2 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.
 *
 * This library is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING THE
 * WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Commercial use or any other use of this library not covered by either 
 * the QPL or the GPL requires an additional license from Dtecta. 
 * Please contact info@dtecta.com for enquiries about the terms of commercial
 * use of this library.
 */

#include "DT_Polyhedron.h"

#ifdef QHULL

extern "C" {
#include <qhull/qhull_a.h>
}

#include <vector>
#include <new>  

typedef std::vector<MT_Point3> T_VertexBuf;
typedef std::vector<DT_Index> T_IndexBuf;
typedef std::vector<T_IndexBuf> T_MultiIndexBuf;

static char options[] = "qhull Qts i Tv";

#define DK_HIERARCHY

T_IndexBuf *adjacency_graph(DT_Count count, const MT_Point3 *verts, const char *flags)
{
	int curlong, totlong, exitcode;
	
    facetT *facet;
    vertexT *vertex;
    vertexT **vertexp;
    
    std::vector<MT::Tuple3<coordT> > array;
	T_IndexBuf index;
    DT_Index i;
    for (i = 0; i != count; ++i) 
	{
		if (flags == 0 || flags[i])
		{
            array.push_back(MT::Tuple3<coordT>(verts[i]));
			index.push_back(i);
		}
    }

    qh_init_A(stdin, stdout, stderr, 0, NULL);
    if ((exitcode = setjmp(qh errexit))) 
	{
		exit(exitcode);
	}
    qh_initflags(options);
    qh_init_B(array[0], array.size(), 3, False);
    qh_qhull();
    qh_check_output();
    
    T_IndexBuf *indexBuf = new T_IndexBuf[count];
    FORALLfacets 
	{
		setT *vertices = qh_facet3vertex(facet);
		
		T_IndexBuf  facetIndices;

		FOREACHvertex_(vertices) 
		{
			facetIndices.push_back(index[qh_pointid(vertex->point)]);
		}
		int i, j;
		for (i = 0, j = facetIndices.size()-1; i < (int)facetIndices.size(); j = i++)
		{
			indexBuf[facetIndices[j]].push_back(facetIndices[i]);
		}
    }

    
    qh NOerrexit = True;
    qh_freeqhull(!qh_ALL);
    qh_memfreeshort(&curlong, &totlong);

	return indexBuf;
}

T_IndexBuf *simplex_adjacency_graph(DT_Count count, const char *flags)
{
	T_IndexBuf *indexBuf = new T_IndexBuf[count];

	DT_Index index[4];
	
	DT_Index k = 0;
	DT_Index i;
	for (i = 0; i != count; ++i) 
	{
		if (flags == 0 || flags[i])
		{
			index[k++] = i;
		}
	}

	assert(k <= 4);

	for (i = 0; i != k; ++i)
	{
		DT_Index j;
		for (j = 0; j != k; ++j)
		{
			if (i != j)
			{
				indexBuf[index[i]].push_back(index[j]);
			}
		}
	}

	return indexBuf;
}

#ifdef DK_HIERARCHY

void prune(DT_Count count, T_MultiIndexBuf *cobound)
{
	DT_Index i;
	for (i = 0; i != count; ++i)
	{
		assert(cobound[i].size());

		DT_Index j;
		for (j = 0; j != cobound[i].size() - 1; ++j)
		{
			T_IndexBuf::iterator it = cobound[i][j].begin();
			while (it != cobound[i][j].end())
			{
				T_IndexBuf::iterator jt = 
					std::find(cobound[i][j+1].begin(), cobound[i][j+1].end(), *it);

				if (jt != cobound[i][j+1].end())
				{
					std::swap(*it, cobound[i][j].back());
					cobound[i][j].pop_back();
				}
				else
				{
					++it;
				}
			}
		}
	}	
}

#endif

DT_Polyhedron::DT_Polyhedron(const DT_VertexBase *base, DT_Count count, const DT_Index *indices)
{
	assert(count);

	std::vector<MT_Point3> vertexBuf;
	DT_Index i;
	for (i = 0; i != count; ++i) 
	{
		vertexBuf.push_back((*base)[indices[i]]);
	}

	T_IndexBuf *indexBuf = count > 4 ? adjacency_graph(count, &vertexBuf[0], 0) : simplex_adjacency_graph(count, 0);
	
	std::vector<MT_Point3> pointBuf;
	
	for (i = 0; i != count; ++i) 
	{
		if (!indexBuf[i].empty()) 
		{
			pointBuf.push_back(vertexBuf[i]);
		}
	}
			
	delete [] indexBuf;

	m_count = pointBuf.size();
	m_verts = new MT_Point3[m_count];	
	std::copy(pointBuf.begin(), pointBuf.end(), &m_verts[0]);

	T_MultiIndexBuf *cobound = new T_MultiIndexBuf[m_count];
    char *flags = new char[m_count];
	std::fill(&flags[0], &flags[m_count], 1);

	DT_Count num_layers = 0;
	DT_Count layer_count = m_count;
	while (layer_count > 4)
	{
		T_IndexBuf *indexBuf = adjacency_graph(m_count, m_verts, flags);
		
		DT_Index i;
		for (i = 0; i != m_count; ++i) 
		{
			if (flags[i])
			{
				assert(!indexBuf[i].empty());
				cobound[i].push_back(indexBuf[i]);
			}
		}
			
		++num_layers;

		delete [] indexBuf;

		std::fill(&flags[0], &flags[m_count], 0);

		for (i = 0; i != m_count; ++i)
		{
			if (cobound[i].size() == num_layers) 
			{
				T_IndexBuf& curr_cobound = cobound[i].back();	
				if (!flags[i] && curr_cobound.size() <= 8)
				{	
					DT_Index j;
					for (j  = 0; j != curr_cobound.size(); ++j)
					{
						flags[curr_cobound[j]] = 1;
					}
				}
			}
		}
		
		layer_count = 0;
		
		for (i = 0; i != m_count; ++i)
		{
			if (flags[i])
			{
				++layer_count;
			}
		}	
	}
	
	indexBuf = simplex_adjacency_graph(m_count, flags);
		
	for (i = 0; i != m_count; ++i) 
	{
		if (flags[i])
		{
			assert(!indexBuf[i].empty());
			cobound[i].push_back(indexBuf[i]);
		}
	}
	
	++num_layers;

	delete [] indexBuf;
	delete [] flags;
		


#ifdef DK_HIERARCHY
	prune(m_count, cobound);
#endif

	m_cobound = new T_MultiIndexArray[m_count];

	for (i = 0; i != m_count; ++i)
	{
		new (&m_cobound[i]) T_MultiIndexArray(cobound[i].size());
		
		DT_Index j;
		for (j = 0; j != cobound[i].size(); ++j)
		{
			new (&m_cobound[i][j]) DT_IndexArray(cobound[i][j].size(), &cobound[i][j][0]);
		}
	}
		
	delete [] cobound;

	m_start_vertex = 0;
	while (m_cobound[m_start_vertex].size() != num_layers) 
	{
		++m_start_vertex;
		assert(m_start_vertex < m_count);
	}

	m_curr_vertex = m_start_vertex;
} 


DT_Polyhedron::~DT_Polyhedron() 
{
	delete [] m_verts;
    delete [] m_cobound;
}

#ifdef DK_HIERARCHY

MT_Scalar DT_Polyhedron::supportH(const MT_Vector3& v) const 
{
    m_curr_vertex = m_start_vertex;
    MT_Scalar d = (*this)[m_curr_vertex].dot(v);
    MT_Scalar h = d;
	int curr_layer;
	for (curr_layer = m_cobound[m_start_vertex].size(); curr_layer != 0; --curr_layer)
	{
		const DT_IndexArray& curr_cobound = m_cobound[m_curr_vertex][curr_layer-1];
        DT_Index i;
		for (i = 0; i != curr_cobound.size(); ++i) 
		{
			d = (*this)[curr_cobound[i]].dot(v);
			if (d > h)
			{
				m_curr_vertex = curr_cobound[i];
				h = d;
			}
		}
	}
	
    return h;
}

MT_Point3 DT_Polyhedron::support(const MT_Vector3& v) const 
{
	m_curr_vertex = m_start_vertex;
    MT_Scalar d = (*this)[m_curr_vertex].dot(v);
    MT_Scalar h = d;
	int curr_layer;
	for (curr_layer = m_cobound[m_start_vertex].size(); curr_layer != 0; --curr_layer)
	{
		const DT_IndexArray& curr_cobound = m_cobound[m_curr_vertex][curr_layer-1];
        DT_Index i;
		for (i = 0; i != curr_cobound.size(); ++i) 
		{
			d = (*this)[curr_cobound[i]].dot(v);
			if (d > h)
			{
				m_curr_vertex = curr_cobound[i];
				h = d;
			}
		}
	}
	
    return (*this)[m_curr_vertex];
}

#else

MT_Scalar DT_Polyhedron::supportH(const MT_Vector3& v) const 
{
    int last_vertex = -1;
    MT_Scalar d = (*this)[m_curr_vertex].dot(v);
    MT_Scalar h = d;
	
	for (;;) 
	{
        DT_IndexArray& curr_cobound = m_cobound[m_curr_vertex][0];
        int i = 0, n = curr_cobound.size(); 
        while (i != n && 
               (curr_cobound[i] == last_vertex || 
				(d = (*this)[curr_cobound[i]].dot(v)) - h <= MT_abs(h) * MT_EPSILON)) 
		{
            ++i;
		}
		
        if (i == n) 
		{
			break;
		}
		
        last_vertex = m_curr_vertex;
        m_curr_vertex = curr_cobound[i];
        h = d;
    }
    return h;
}

MT_Point3 DT_Polyhedron::support(const MT_Vector3& v) const 
{
	int last_vertex = -1;
    MT_Scalar d = (*this)[m_curr_vertex].dot(v);
    MT_Scalar h = d;
	
    for (;;)
	{
        DT_IndexArray& curr_cobound = m_cobound[m_curr_vertex][0];
        int i = 0, n = curr_cobound.size();
        while (i != n && 
               (curr_cobound[i] == last_vertex || 
				(d = (*this)[curr_cobound[i]].dot(v)) - h <= MT_abs(h) * MT_EPSILON)) 
		{
            ++i;
		}
		
        if (i == n)
		{
			break;
		}
		
		last_vertex = m_curr_vertex;
        m_curr_vertex = curr_cobound[i];
        h = d;
    }
    return (*this)[m_curr_vertex];
}

#endif

#endif


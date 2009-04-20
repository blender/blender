/**
 * $Id$
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "BSP_PlyLoader.h"

#include "MT_Vector3.h"
#include "ply.h"

struct LoadVertex {
  float x,y,z;             /* the usual 3-space position of a vertex */
};

struct LoadFace {
  unsigned char intensity; /* this user attaches intensity to faces */
  unsigned char nverts;    /* number of vertex indices in list */
  int *verts;              /* vertex index list */
};


	MEM_SmartPtr<BSP_TMesh>
BSP_PlyLoader::
NewMeshFromFile(
	char * file_name,
	MT_Vector3 &min,
	MT_Vector3 &max

) {

	min = MT_Vector3(MT_INFINITY,MT_INFINITY,MT_INFINITY);
	max = MT_Vector3(-MT_INFINITY,-MT_INFINITY,-MT_INFINITY);
	
	PlyProperty vert_props[] = { /* list of property information for a vertex */
	  {"x", PLY_FLOAT, PLY_FLOAT, offsetof(LoadVertex,x), 0, 0, 0, 0},
	  {"y", PLY_FLOAT, PLY_FLOAT, offsetof(LoadVertex,y), 0, 0, 0, 0},
	  {"z", PLY_FLOAT, PLY_FLOAT, offsetof(LoadVertex,z), 0, 0, 0, 0},
	};

	PlyProperty face_props[] = { /* list of property information for a vertex */
	  {"vertex_indices", PLY_INT, PLY_INT, offsetof(LoadFace,verts),
	   1, PLY_UCHAR, PLY_UCHAR, offsetof(LoadFace,nverts)},
	};

	MEM_SmartPtr<BSP_TMesh> mesh = new BSP_TMesh;

	if (mesh == NULL) return NULL;

	int i,j;
	PlyFile *ply;
	int nelems;
	char **elist;
	int file_type;
	float version;
	int nprops;
	int num_elems;
	PlyProperty **plist;

	char *elem_name;

	LoadVertex load_vertex;
	LoadFace load_face;

	/* open a PLY file for reading */
	ply = ply_open_for_reading(
		file_name,
		&nelems,
		&elist, 
		&file_type, 
		&version
	);

	if (ply == NULL) return NULL;

	/* go through each kind of element that we learned is in the file */
	/* and read them */

	for (i = 0; i < nelems; i++) {

		/* get the description of the first element */

		elem_name = elist[i];
		plist = ply_get_element_description (ply, elem_name, &num_elems, &nprops);

		/* print the name of the element, for debugging */

		/* if we're on vertex elements, read them in */

		if (equal_strings ("vertex", elem_name)) {

			/* set up for getting vertex elements */

			ply_get_property (ply, elem_name, &vert_props[0]);
			ply_get_property (ply, elem_name, &vert_props[1]);
			ply_get_property (ply, elem_name, &vert_props[2]);

			// make some memory for the vertices		
			mesh->VertexSet().reserve(num_elems);

			/* grab all the vertex elements */
			for (j = 0; j < num_elems; j++) {

				/* grab and element from the file */
				ply_get_element (ply, (void *)&load_vertex);
				// pass the vertex into the mesh builder.
					
				if (load_vertex.x < min.x()) {
					min.x() = load_vertex.x;
				} else
				if (load_vertex.x > max.x()) {
					max.x()= load_vertex.x;
				}

				if (load_vertex.y < min.y()) {
					min.y() = load_vertex.y;
				} else
				if (load_vertex.y > max.y()) {
					max.y()= load_vertex.y;
				}

				if (load_vertex.z < min.z()) {
					min.z() = load_vertex.z;
				} else
				if (load_vertex.z > max.z()) {
					max.z()= load_vertex.z;
				}

				BSP_TVertex my_vert;
				my_vert.m_pos = MT_Vector3(load_vertex.x,load_vertex.y,load_vertex.z);
				mesh->VertexSet().push_back(my_vert);
			}
		

		}

		/* if we're on face elements, read them in */
		if (equal_strings ("face", elem_name)) {

			/* set up for getting face elements */

			ply_get_property (ply, elem_name, &face_props[0]);

			/* grab all the face elements */
			for (j = 0; j < num_elems; j++) {

				ply_get_element (ply, (void *)&load_face);

				int v;
				for (v = 2; v< load_face.nverts; v++) {

					BSP_TFace f;

					f.m_verts[0] = load_face.verts[0];
					f.m_verts[1] = load_face.verts[v-1];
					f.m_verts[2] = load_face.verts[v];

					mesh->BuildNormal(f);	
					mesh->FaceSet().push_back(f);
				}
				// free up the memory this pile of shit used to allocate the polygon's vertices
				free (load_face.verts);
			}

		}
	}
  /* close the PLY file */
  ply_close (ply);

 return mesh;
}

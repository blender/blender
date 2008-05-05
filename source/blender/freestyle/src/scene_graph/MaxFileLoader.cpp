
//
//  Copyright (C) : Please refer to the COPYRIGHT file distributed 
//   with this source distribution. 
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
///////////////////////////////////////////////////////////////////////////////

#include "MaxFileLoader.h"

MaxFileLoader::MaxFileLoader()
{
  _FileName = NULL;
  _3dsFile = NULL;
  _Scene = NULL;
  _numFacesRead = 0;
  _minEdgeSize = DBL_MAX;
}

MaxFileLoader::MaxFileLoader(const char *iFileName)
{
  _FileName = new char[strlen(iFileName)+1];
  strcpy(_FileName, iFileName);

  _3dsFile = NULL;
  _Scene = NULL;
  _numFacesRead = 0;
  _minEdgeSize = DBL_MAX;
}

MaxFileLoader::~MaxFileLoader()
{
  if(NULL != _FileName)
  {
    delete [] _FileName;
    _FileName = NULL;
  }

  if(NULL != _3dsFile)
  {
    lib3ds_file_free(_3dsFile);
    _3dsFile = NULL;
  }

  _Scene = NULL;
}

void MaxFileLoader::SetFileName(const char *iFileName)
{
  if(NULL != _FileName)
    delete [] _FileName;

  _FileName = new char[strlen(iFileName)+1];
  strcpy(_FileName, iFileName);
}

NodeGroup* MaxFileLoader::Load()
{
  _3dsFile=lib3ds_file_load(_FileName);
  if(NULL == _3dsFile)
    return NULL;

	/* No nodes?  Fabricate nodes to display all the meshes. */
  if( !_3dsFile->nodes )
  {
    Lib3dsMesh *mesh;
    Lib3dsNode *node;

    for(mesh = _3dsFile->meshes; mesh != NULL; mesh = mesh->next)
    {
      node = lib3ds_node_new_object();
      strcpy(node->name, mesh->name);
      node->parent_id = LIB3DS_NO_PARENT;
      node->data.object.scl_track.keyL = lib3ds_lin3_key_new();
      node->data.object.scl_track.keyL->value[0] = 1.;
      node->data.object.scl_track.keyL->value[1] = 1.;
      node->data.object.scl_track.keyL->value[2] = 1.;
      lib3ds_file_insert_node(_3dsFile, node);
    }
  }

  lib3ds_file_eval(_3dsFile, 0);
  
  // creation of the scene root node
  _Scene = new NodeGroup;

  // converts the 3ds format to the scene format
  // the RenderNode method modifies _Scene.
  Lib3dsNode *p;
  for (p=_3dsFile->nodes; p!=0; p=p->next) {
    RenderNode(p);
  }
  //Returns the built scene.
  return _Scene;
}

void lib3ds_normal_transform(Lib3dsVector c, Lib3dsMatrix m, Lib3dsVector a)
{
  c[0]= (m[0][0]*a[0] + m[1][0]*a[1] + m[2][0]*a[2]);
  c[1]= (m[0][1]*a[0] + m[1][1]*a[1] + m[2][1]*a[2]);
  c[2]= (m[0][2]*a[0] + m[1][2]*a[1] + m[2][2]*a[2]);

  //  c[0]= (m[0][0]*a[0] + m[1][0]*a[1] + m[2][0]*a[2])/m[0][0];
  //  c[1]= (m[0][1]*a[0] + m[1][1]*a[1] + m[2][1]*a[2])/m[1][1];
  //  c[2]= (m[0][2]*a[0] + m[1][2]*a[1] + m[2][2]*a[2])/m[2][2];

  //lib3ds_vector_normalize(c);

  //  c[0] = c[0]*m[0][0];
  //  c[1] = c[1]*m[1][1];
  //  c[2] = c[2]*m[2][2];

}



void MaxFileLoader::RenderNode(Lib3dsNode *iNode)
{
  Lib3dsNode *p;
  for (p=iNode->childs; p!=0; p=p->next) 
    RenderNode(p);

  float minBBox[3];
  float maxBBox[3];
  if (iNode->type==LIB3DS_OBJECT_NODE) 
  {
    if (strcmp(iNode->name,"$$$DUMMY")==0) 
      return;

    NodeTransform *currentMesh = new NodeTransform;
    NodeShape * shape;
    
    if (!iNode->user.d) // If the shape is not built yet, just do it !
    {
      Lib3dsMesh *mesh=lib3ds_file_mesh_by_name(_3dsFile, iNode->name);
      ASSERT(mesh);
      if (!mesh) 
        return;

      // builds the shape:
      shape = new NodeShape;
      iNode->user.d=(unsigned long)shape; // We store as user data the NodeShape address

      // We invert the matrix in order to 
      // be able to retrieve the shape's coordinates 
      // in its local coordinates system (origin is the iNode pivot)
      Lib3dsMatrix M;
      lib3ds_matrix_copy(M, mesh->matrix);
      lib3ds_matrix_inv(M);
      
      // We compute a normal per vertex and manages the smoothing of the shape:
      Lib3dsVector *normalL=(Lib3dsVector*)malloc(3*sizeof(Lib3dsVector)*mesh->faces);
      lib3ds_mesh_calculate_normals(mesh, normalL);

      // We build the rep:
      IndexedFaceSet *rep;
      unsigned numFaces = mesh->faces;
      
      unsigned vSize = 3*3*numFaces;
      float *vertices = new float[vSize];
      unsigned nSize = vSize;
      float *normals = new float[nSize];
      unsigned *numVertexPerFaces = new unsigned[numFaces];
      vector<Material> meshMaterials;

      IndexedFaceSet::TRIANGLES_STYLE *faceStyle = new IndexedFaceSet::TRIANGLES_STYLE[numFaces];
      unsigned i;
      for (i = 0; i <numFaces; i++) {
        faceStyle[i] = IndexedFaceSet::TRIANGLES;
        numVertexPerFaces[i] = 3;
      }
      
      unsigned viSize = 3*numFaces;
      unsigned *VIndices = new unsigned[viSize];
      unsigned niSize = viSize;
      unsigned *NIndices = new unsigned[niSize];
      unsigned *MIndices = new unsigned[viSize]; // Material Indices
     

      float *pv = vertices;
      float *pn = normals;
      unsigned *pvi = VIndices;
      unsigned *pni = NIndices;
      unsigned *pmi = MIndices;
      
      unsigned currentIndex = 0;
      unsigned currentMIndex = 0;

      Material tmpMat;
      
      // we want to find the min and max coordinates as we build the rep. 
      // We initialize the min and max values whith the first vertex.
      float pvtmp[3];
      lib3ds_vector_transform(pvtmp, M, mesh->pointL[mesh->faceL[0].points[0]].pos);
      minBBox[0] = pvtmp[0];
      maxBBox[0] = pvtmp[0];
      minBBox[1] = pvtmp[1];
      maxBBox[1] = pvtmp[1];
      minBBox[2] = pvtmp[2];
      maxBBox[2] = pvtmp[2];

      unsigned p;
      real vert[3][3];
      real norm;
      for(p=0; p<mesh->faces; ++p) // we parse the faces of the mesh
      {
        Lib3dsFace *f=&mesh->faceL[p];
        Lib3dsMaterial *mat=0;
        if (f->material[0]) 
          mat=lib3ds_file_material_by_name(_3dsFile, f->material);

        if (mat) 
        {
          tmpMat.SetDiffuse(mat->diffuse[0], mat->diffuse[1], mat->diffuse[2], mat->diffuse[3]);
          tmpMat.SetSpecular(mat->specular[0], mat->specular[1], mat->specular[2], mat->specular[3]);
          float s = (float)pow(2.0, 10.0*mat->shininess);
          if(s > 128.f)
            s = 128.f;
          tmpMat.SetShininess(s);
        }
        
        if(meshMaterials.empty()){
          meshMaterials.push_back(tmpMat);
          shape->SetMaterial(tmpMat);
        }else{
          // find if the material is aleady in the list
          unsigned i=0;
          bool found = false;
          for(vector<Material>::iterator it=meshMaterials.begin(), itend=meshMaterials.end();
          it!=itend;
          ++it){
            if(*it == tmpMat){
              currentMIndex = i;
              found = true;
              break;
            }
            ++i;
          }
          if(!found){
            meshMaterials.push_back(tmpMat);
            currentMIndex = meshMaterials.size()-1;
          }
        }
        

        for(i=0; i<3; ++i) // we parse the vertices of the face f
        {
          unsigned j;
          lib3ds_vector_transform(pv, M, mesh->pointL[f->points[i]].pos); //fills the cells of the pv array
          for(j=0; j<3; j++) // we parse the xyz coordinates of the vertex i
          {
            if(minBBox[j] > pv[j])
              minBBox[j] = pv[j];

            if(maxBBox[j] < pv[j])
              maxBBox[j] = pv[j];

            vert[i][j] = pv[j];
          }
          
          for(j=0; j<3; j++)
            pn[j] = f->normal[j];

          lib3ds_normal_transform(pn, M, normalL[3*p+i]); //fills the cells of the pv array
          //lib3ds_vector_normalize(pn);

          
          *pvi = currentIndex;
          *pni = currentIndex;
          *pmi = currentMIndex;
          

          currentIndex +=3;
          pv += 3;
          pn += 3;
          pvi++;
          pni++;
          pmi++;
          
        }

        for(i=0; i<3; i++)
        {
          norm = 0.0;
          for (unsigned j = 0; j < 3; j++)
            norm += (vert[i][j] - vert[(i+1)%3][j])*(vert[i][j] - vert[(i+1)%3][j]);
          norm = sqrt(norm);
          if(_minEdgeSize > norm)
            _minEdgeSize = norm;
        }

        _numFacesRead++;
      }

      free(normalL);

      // We might have several times the same vertex. We want a clean 
      // shape with no real-vertex. Here, we are making a cleaning 
      // pass.
      real *cleanVertices = NULL;
      unsigned   cvSize;
      unsigned   *cleanVIndices = NULL;
      
      GeomCleaner::CleanIndexedVertexArray(
        vertices, vSize, 
        VIndices, viSize,
        &cleanVertices, &cvSize, 
        &cleanVIndices);

      real *cleanNormals = NULL;
      unsigned   cnSize;
      unsigned   *cleanNIndices = NULL;

      GeomCleaner::CleanIndexedVertexArray(
        normals, nSize, 
        NIndices, niSize,
        &cleanNormals, &cnSize, 
        &cleanNIndices);

      // format materials array
      Material** marray = new Material*[meshMaterials.size()];
      unsigned mindex=0;
      for(vector<Material>::iterator m=meshMaterials.begin(), mend=meshMaterials.end();
          m!=mend;
          ++m){
        marray[mindex] = new Material(*m);
        ++mindex;
      }
      // deallocates memory:
      delete [] vertices;
      delete [] normals;
      delete [] VIndices;
      delete [] NIndices;
      
      // Create the IndexedFaceSet with the retrieved attributes
      rep = new IndexedFaceSet(cleanVertices, cvSize, 
                               cleanNormals, cnSize,
                               marray, meshMaterials.size(),
                               0, 0,
                               numFaces, numVertexPerFaces, faceStyle,
                               cleanVIndices, viSize,
                               cleanNIndices, niSize,
                               MIndices, viSize,
                               0,0,
                               0);
	  // sets the id of the rep
	  rep->SetId(Id(iNode->node_id, 0));


      const BBox<Vec3r> bbox = BBox<Vec3r>(Vec3r(minBBox[0], minBBox[1], minBBox[2]), 
                                           Vec3r(maxBBox[0], maxBBox[1], maxBBox[2]));
      rep->SetBBox(bbox);
      shape->AddRep(rep);
    }

    if (iNode->user.d) 
    {
      if(NULL != iNode->matrix)
      {
        Lib3dsObjectData *d = &iNode->data.object;
        Matrix44r M44f;
        for(unsigned i=0; i<4; i++)
          for(unsigned j=0; j<4; j++)
            M44f(i,j) = iNode->matrix[j][i];
          
          currentMesh->SetMatrix(Matrix44r(M44f));
          currentMesh->Translate(-d->pivot[0], -d->pivot[1], -d->pivot[2]);
      }
      shape = (NodeShape*)iNode->user.d;
      currentMesh->AddChild(shape);
      _Scene->AddChild(currentMesh);
    }
  }

}

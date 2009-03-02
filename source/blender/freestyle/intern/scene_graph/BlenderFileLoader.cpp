#include "BlenderFileLoader.h"

BlenderFileLoader::BlenderFileLoader(Render *re)
{
	_re = re;
  _Scene = NULL;
  _numFacesRead = 0;
  _minEdgeSize = DBL_MAX;
}

BlenderFileLoader::~BlenderFileLoader()
{
  _Scene = NULL;
}

NodeGroup* BlenderFileLoader::Load()
{
	ObjectInstanceRen *obi;
	ObjectRen *obr;

	cout << "\n===  Importing triangular meshes into Blender  ===" << endl;
	
	SceneRenderLayer *srl, *active_srl = NULL;
	int count = 0;
	for(srl= (SceneRenderLayer *)_re->scene->r.layers.first; srl; srl= srl->next) {
		if(srl->layflag & SCE_LAY_FRS) {
			if (!active_srl) active_srl = srl;
			count++;
		}
	}
	if (count > 1) {
		cout << "Warning: Freestyle is enabled in the following " << count << " scene render layers:" << endl;
		for(srl= (SceneRenderLayer *)_re->scene->r.layers.first; srl; srl= srl->next)
			if(srl->layflag & SCE_LAY_FRS)
				cout << "  \"" << srl->name << "\"" << ((active_srl == srl) ? " (only this is taken into account)" : "") << endl;
	}

  // creation of the scene root node
  _Scene = new NodeGroup;

	int id = 0;
	for(obi= (ObjectInstanceRen *) _re->instancetable.first; obi; obi=obi->next) {
		if (!(obi->lay & _re->scene->lay & active_srl->lay))
			continue;

		obr= obi->obr;
		
		if( obr->totvlak > 0)
			insertShapeNode(obr, ++id);
		else
			cout << "  Sorry, only vlak-based shapes are supported." << endl;
	}

  //Returns the built scene.
  return _Scene;
}

void BlenderFileLoader::insertShapeNode(ObjectRen *obr, int id)
{
		VlakRen *vlr;
	
	float minBBox[3];
	float maxBBox[3];
	
	NodeTransform *currentMesh = new NodeTransform;
	NodeShape * shape;
	
	// Mesh *mesh = (Mesh *)ob->data;
	//---------------------
	// mesh => obr
	
	// builds the shape:
	shape = new NodeShape;
	
	// We invert the matrix in order to be able to retrieve the shape's coordinates in its local coordinates system (origin is the iNode pivot)
	// Lib3dsMatrix M;
	// lib3ds_matrix_copy(M, mesh->matrix);
	// lib3ds_matrix_inv(M);
	//---------------------
	// M allows to recover world coordinates from camera coordinates
	// M => obr->ob->imat * obr->obmat  (multiplication from left to right)
	float M[4][4];
	MTC_Mat4MulMat4(M, obr->ob->imat, obr->ob->obmat); 
	
	// We compute a normal per vertex and manages the smoothing of the shape:
	// Lib3dsVector *normalL=(Lib3dsVector*)malloc(3*sizeof(Lib3dsVector)*mesh->faces);
	// lib3ds_mesh_calculate_normals(mesh, normalL);
	// mesh_calc_normals(mesh->mvert, mesh->totvert, mesh->mface, mesh->totface, NULL);
	//---------------------
	// already calculated and availabe in vlak ?	
	
	// We build the rep:
	IndexedFaceSet *rep;
	unsigned numFaces = 0;
	for(int a=0; a < obr->totvlak; a++) {
		if((a & 255)==0) vlr= obr->vlaknodes[a>>8].vlak;
		else vlr++;
	
		if(vlr->v4)
			numFaces += 2;
		else
			numFaces++;
	}
	
	unsigned vSize = 3*3*numFaces;
	float *vertices = new float[vSize];
	unsigned nSize = vSize;
	float *normals = new float[nSize];
	unsigned *numVertexPerFaces = new unsigned[numFaces];
	vector<FrsMaterial> meshFrsMaterials;
	
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
	
	FrsMaterial tmpMat;
	
	// we want to find the min and max coordinates as we build the rep. 
	// We initialize the min and max values whith the first vertex.
	//lib3ds_vector_transform(pvtmp, M, mesh->pointL[mesh->faceL[0].points[0]].pos);
	float pvtmp[3];
	pvtmp[0] = obr->vertnodes[0].vert->co[0];
	pvtmp[1] = obr->vertnodes[0].vert->co[1];
	pvtmp[2] = obr->vertnodes[0].vert->co[2];
	
	MTC_Mat4MulVecfl( M, pvtmp);
	
	minBBox[0] = pvtmp[0];
	maxBBox[0] = pvtmp[0];
	minBBox[1] = pvtmp[1];
	maxBBox[1] = pvtmp[1];
	minBBox[2] = pvtmp[2];
	maxBBox[2] = pvtmp[2];
	
	int p;
	real vert[3][3];
	real norm;
	for(p=0; p < obr->totvlak; ++p) // we parse the faces of the mesh
	{
			VertRen * fv[3];
		
		  // Lib3dsFace *f=&mesh->faceL[p];
		  // Lib3dsMaterial *mat=0;
			if((p & 255)==0) vlr = obr->vlaknodes[p>>8].vlak;
			else vlr++;
			Material *mat = vlr->mat;
	
			if (mat) 
			{
			    tmpMat.setDiffuse( mat->r, mat->g, mat->b, mat->alpha );
			    tmpMat.setSpecular( mat->specr, mat->specg, mat->specb, mat->spectra);
			    float s = 1.0 * (mat->har + 1) / 4 ; // in Blender: [1;511] => in OpenGL: [0;128]
			    if(s > 128.f)
			      s = 128.f;
			    tmpMat.setShininess(s);
			}
	  
			if(meshFrsMaterials.empty())
			{
				meshFrsMaterials.push_back(tmpMat);
		    	shape->setFrsMaterial(tmpMat);
		  	} else {
		    	// find if the material is aleady in the list
		    	unsigned i=0;
		    	bool found = false;
		
		    	for(vector<FrsMaterial>::iterator it=meshFrsMaterials.begin(), itend=meshFrsMaterials.end();
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
		      		meshFrsMaterials.push_back(tmpMat);
		      		currentMIndex = meshFrsMaterials.size()-1;
		    	}
	  		}
	
			unsigned j;
			fv[0] = vlr->v1;
			fv[1] = vlr->v2;
			fv[2] = vlr->v3;
			float *pv_ptr[3];
			for(i=0; i<3; ++i) // we parse the vertices of the face f
			{
	
				//lib3ds_vector_transform(pv, M, mesh->pointL[f->points[i]].pos); //fills the cells of the pv array
				for(j=0; j<3; j++)
					pv[j] = fv[i]->co[j];
				MTC_Mat4MulVecfl( M, pv);
	
				for(j=0; j<3; j++) // we parse the xyz coordinates of the vertex i
				{
			  		if(minBBox[j] > pv[j])
			    		minBBox[j] = pv[j];
	
			  		if(maxBBox[j] < pv[j])
			    		maxBBox[j] = pv[j];
	
			  		vert[i][j] = pv[j];
				}
				
				pv_ptr[i] = pv;
				*pvi = currentIndex;
				*pmi = currentMIndex;

				currentIndex +=3;
				pv += 3;

				pvi++;
				pmi++;
			}
			
			currentIndex -= 9;
						
			float vec01[3];
			vec01[0] = pv_ptr[1][0] - pv_ptr[0][0]; 
			vec01[1] = pv_ptr[1][1] - pv_ptr[0][1];
			vec01[2] = pv_ptr[1][2] - pv_ptr[0][2];
			
			float vec02[3];
			vec02[0] = pv_ptr[2][0] - pv_ptr[0][0]; 
			vec02[1] = pv_ptr[2][1] - pv_ptr[0][1];
			vec02[2] = pv_ptr[2][2] - pv_ptr[0][2];
			
			float n[3];
			MTC_cross3Float(n, vec01, vec02);
			MTC_normalize3DF(n);
			
			for(i=0; i<3; ++i) {
				for(j=0; j<3; ++j) {
					pn[j] = n[j];
				}
				*pni = currentIndex;

				pn += 3;
				pni++;

				currentIndex +=3;
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
			
			++_numFacesRead;

			
			if(vlr->v4){

				unsigned j;
				fv[0] = vlr->v1;
				fv[1] = vlr->v3;
				fv[2] = vlr->v4;
				float *pv_ptr[3];
				for(i=0; i<3; ++i) // we parse the vertices of the face f
				{

					//lib3ds_vector_transform(pv, M, mesh->pointL[f->points[i]].pos); //fills the cells of the pv array
					for(j=0; j<3; j++)
						pv[j] = fv[i]->co[j];
					MTC_Mat4MulVecfl( M, pv);

					for(j=0; j<3; j++) // we parse the xyz coordinates of the vertex i
					{
				  		if(minBBox[j] > pv[j])
				    		minBBox[j] = pv[j];

				  		if(maxBBox[j] < pv[j])
				    		maxBBox[j] = pv[j];

				  		vert[i][j] = pv[j];
					}

					pv_ptr[i] = pv;
					*pvi = currentIndex;
					*pmi = currentMIndex;

					currentIndex +=3;
					pv += 3;

					pvi++;
					pmi++;
				}

				currentIndex -= 9;

				float vec01[3];
				vec01[0] = pv_ptr[1][0] - pv_ptr[0][0]; 
				vec01[1] = pv_ptr[1][1] - pv_ptr[0][1];
				vec01[2] = pv_ptr[1][2] - pv_ptr[0][2];

				float vec02[3];
				vec02[0] = pv_ptr[2][0] - pv_ptr[0][0]; 
				vec02[1] = pv_ptr[2][1] - pv_ptr[0][1];
				vec02[2] = pv_ptr[2][2] - pv_ptr[0][2];

				float n[3];
				MTC_cross3Float(n, vec01, vec02);
				MTC_normalize3DF(n);
				
				for(i=0; i<3; ++i) {
					for(j=0; j<3; ++j) {
						pn[j] = n[j];
					}
					*pni = currentIndex;

					pn += 3;
					pni++;

					currentIndex +=3;
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

				++_numFacesRead;



			}
	
	}
	
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
	FrsMaterial** marray = new FrsMaterial*[meshFrsMaterials.size()];
	unsigned mindex=0;
	for(vector<FrsMaterial>::iterator m=meshFrsMaterials.begin(), mend=meshFrsMaterials.end();
	    m!=mend;
	    ++m){
	  marray[mindex] = new FrsMaterial(*m);
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
	                         marray, meshFrsMaterials.size(),
	                         0, 0,
	                         numFaces, numVertexPerFaces, faceStyle,
	                         cleanVIndices, viSize,
	                         cleanNIndices, niSize,
	                         MIndices, viSize,
	                         0,0,
	                         0);
	// sets the id of the rep
	rep->setId(Id(id, 0));
	
	const BBox<Vec3r> bbox = BBox<Vec3r>(Vec3r(minBBox[0], minBBox[1], minBBox[2]), 
	                                     Vec3r(maxBBox[0], maxBBox[1], maxBBox[2]));
	rep->setBBox(bbox);
	shape->AddRep(rep);
	
	Matrix44r meshMat = Matrix44r::identity();
	currentMesh->setMatrix(meshMat);
	currentMesh->Translate(0,0,0);
	
	currentMesh->AddChild(shape);
	_Scene->AddChild(currentMesh);
	
}

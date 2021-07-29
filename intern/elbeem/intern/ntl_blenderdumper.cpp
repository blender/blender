/** \file elbeem/intern/ntl_blenderdumper.cpp
 *  \ingroup elbeem
 */
/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003-2006 Nils Thuerey
 *
 * Replaces std. raytracer, and only dumps time dep. objects to disc
 *
 *****************************************************************************/

#include <fstream>
#include <sys/types.h>

#include "utilities.h"
#include "ntl_matrices.h"
#include "ntl_blenderdumper.h"
#include "ntl_world.h"
#include "solver_interface.h"
#include "globals.h"

#include <zlib.h>



/******************************************************************************
 * Constructor
 *****************************************************************************/
ntlBlenderDumper::ntlBlenderDumper() : ntlWorld()
{
	// same as normal constructor here
}
ntlBlenderDumper::ntlBlenderDumper(string filename, bool commandlineMode) :
	ntlWorld(filename,commandlineMode)
{
	// init world
}



/******************************************************************************
 * Destructor
 *****************************************************************************/
ntlBlenderDumper::~ntlBlenderDumper()
{
	debMsgStd("ntlBlenderDumper",DM_NOTIFY, "ntlBlenderDumper done", 10);
}

/******************************************************************************
 * Only dump time dep. objects to file
 *****************************************************************************/
int ntlBlenderDumper::renderScene( void )
{
	char nrStr[5];								/* nr conversion */
  ntlRenderGlobals *glob = mpGlob;
  ntlScene *scene = mpGlob->getSimScene();
	bool debugOut = false;
	bool debugRender = false;
#if ELBEEM_PLUGIN==1
	debugOut = false;
#endif // ELBEEM_PLUGIN==1

	vector<string> gmName; 	 // gm names
	vector<string> gmMat;    // materials for gm
	int numGMs = 0;					 // no. of .obj models created

	if(debugOut) debMsgStd("ntlBlenderDumper::renderScene",DM_NOTIFY,"Dumping geometry data", 1);
  long startTime = getTime();
	snprintf(nrStr, 5, "%04d", glob->getAniCount() );

  // local scene vars
  vector<ntlTriangle> Triangles;
  vector<ntlVec3Gfx>  Vertices;
  vector<ntlVec3Gfx>  VertNormals;

	// check geo objects
	int idCnt = 0;          // give IDs to objects
	for (vector<ntlGeometryClass*>::iterator iter = scene->getGeoClasses()->begin();
			iter != scene->getGeoClasses()->end(); iter++) {
		if(!(*iter)->getVisible()) continue;
		int tid = (*iter)->getTypeId();

		if(tid & GEOCLASSTID_OBJECT) {
			// normal geom. objects -> ignore
		}
		if(tid & GEOCLASSTID_SHADER) {
			ntlGeometryShader *geoshad = (ntlGeometryShader*)(*iter); //dynamic_cast<ntlGeometryShader*>(*iter);
			string outname = geoshad->getOutFilename();
			if(outname.length()<1) outname = mpGlob->getOutFilename();
			geoshad->notifyShaderOfDump(DUMP_FULLGEOMETRY, glob->getAniCount(),nrStr,outname);

			for (vector<ntlGeometryObject*>::iterator siter = geoshad->getObjectsBegin();
					siter != geoshad->getObjectsEnd();
					siter++) {
				if(debugOut) debMsgStd("ntlBlenderDumper::BuildScene",DM_MSG,"added shader geometry "<<(*siter)->getName(), 8);

				(*siter)->notifyOfDump(DUMP_FULLGEOMETRY, glob->getAniCount(),nrStr,outname, this->mSimulationTime);
				bool doDump = false;
				bool isPreview = false;
				// only dump final&preview surface meshes
				if( (*siter)->getName().find( "final" ) != string::npos) {
					doDump = true;
				} else if( (*siter)->getName().find( "preview" ) != string::npos) {
					doDump = true;
					isPreview = true;
				}
				if(!doDump) continue;

				// dont quit, some objects need notifyOfDump call
				if((glob_mpactive) && (glob_mpindex>0)) {
					continue; //return 0;
				}
				
				// only dump geo shader objects
				Triangles.clear();
				Vertices.clear();
				VertNormals.clear();
				(*siter)->initialize( mpGlob );
				(*siter)->getTriangles(this->mSimulationTime, &Triangles, &Vertices, &VertNormals, idCnt);
				idCnt ++;
				
				// WARNING - this is dirty, but simobjs are the only geoshaders right now
				SimulationObject *sim = (SimulationObject *)geoshad;
				LbmSolverInterface *lbm = sim->getSolver();


				// always dump mesh, even empty ones...

				// dump to binary file
				std::ostringstream boutfilename("");
				//boutfilename << ecrpath.str() << outname <<"_"<< (*siter)->getName() <<"_" << nrStr << ".obj";
				boutfilename << outname <<"_"<< (*siter)->getName() <<"_" << nrStr;
				if(debugOut) debMsgStd("ntlBlenderDumper::renderScene",DM_MSG,"B-Dumping: "<< (*siter)->getName() 
						<<", triangles:"<<Triangles.size()<<", vertices:"<<Vertices.size()<<
						" to "<<boutfilename.str() , 7);
				gzFile gzf;

				// output velocities if desired
				if((!isPreview) && (lbm->getDumpVelocities())) {
					std::ostringstream bvelfilename;
					bvelfilename << boutfilename.str();
					bvelfilename << ".bvel.gz";
					gzf = gzopen(bvelfilename.str().c_str(), "wb9");
					if(gzf) {
						int numVerts;
						if(sizeof(numVerts)!=4) { errMsg("ntlBlenderDumper::renderScene","Invalid int size"); return 1; }
						numVerts = Vertices.size();
						gzwrite(gzf, &numVerts, sizeof(numVerts));
						for(size_t i=0; i<Vertices.size(); i++) {
							// returns smoothed velocity, scaled by frame time
							ntlVec3Gfx v = lbm->getVelocityAt( Vertices[i][0], Vertices[i][1], Vertices[i][2] );
							// translation not necessary, test rotation & scaling?
							for(int j=0; j<3; j++) {
								float vertp = v[j];
								//if(i<20) errMsg("ntlBlenderDumper","DUMP_VEL final "<<i<<" = "<<v);
								gzwrite(gzf, &vertp, sizeof(vertp)); }
						}
						gzclose( gzf );
					}
				}

				// compress all bobj's 
				boutfilename << ".bobj.gz";
				gzf = gzopen(boutfilename.str().c_str(), "wb1"); // wb9 is slow for large meshes!
				if (!gzf) {
					errMsg("ntlBlenderDumper::renderScene","Unable to open output '" + boutfilename.str() + "' ");
					return 1; }

				// dont transform velocity output, this is handled in blender
				// current transform matrix
				ntlMatrix4x4<gfxReal> *trafo;
				trafo = lbm->getDomainTrafo();
				if(trafo) {
					// transform into source space
					for(size_t i=0; i<Vertices.size(); i++) {
						Vertices[i] = (*trafo) * Vertices[i];
					}
				}
				// rotate vertnormals
				ntlMatrix4x4<gfxReal> rottrafo;
				rottrafo.initId();
				if(lbm->getDomainTrafo()) {
					// dont modifiy original!
					rottrafo = *lbm->getDomainTrafo();
					ntlVec3Gfx rTrans,rScale,rRot,rShear;
					rottrafo.decompose(rTrans,rScale,rRot,rShear);
					rottrafo.initRotationXYZ(rRot[0],rRot[1],rRot[2]);
					// only rotate here...
					for(size_t i=0; i<Vertices.size(); i++) {
						VertNormals[i] = rottrafo * VertNormals[i];
						normalize(VertNormals[i]); // remove scaling etc.
					}
				}

				
				// write to file
				int numVerts;
				if(sizeof(numVerts)!=4) { errMsg("ntlBlenderDumper::renderScene","Invalid int size"); return 1; }
				numVerts = Vertices.size();
				gzwrite(gzf, &numVerts, sizeof(numVerts));
				for(size_t i=0; i<Vertices.size(); i++) {
					for(int j=0; j<3; j++) {
						float vertp = Vertices[i][j];
						gzwrite(gzf, &vertp, sizeof(vertp)); }
				}

				// should be the same as Vertices.size
				if(VertNormals.size() != (size_t)numVerts) {
					errMsg("ntlBlenderDumper::renderScene","Normals have to have same size as vertices!");
					VertNormals.resize( Vertices.size() );
				}
				gzwrite(gzf, &numVerts, sizeof(numVerts));
				for(size_t i=0; i<VertNormals.size(); i++) {
					for(int j=0; j<3; j++) {
						float normp = VertNormals[i][j];
						gzwrite(gzf, &normp, sizeof(normp)); }
				}

				int numTris = Triangles.size();
				gzwrite(gzf, &numTris, sizeof(numTris));
				for(size_t i=0; i<Triangles.size(); i++) {
					for(int j=0; j<3; j++) {
						int triIndex = Triangles[i].getPoints()[j];
						gzwrite(gzf, &triIndex, sizeof(triIndex)); }
				}
				gzclose( gzf );
				debMsgStd("ntlBlenderDumper::renderScene",DM_NOTIFY," Wrote: '"<<boutfilename.str()<<"' ", 2);
				numGMs++;
			}
		}

	}

	// output ecr config file
	if(numGMs>0) {
		if(debugOut) debMsgStd("ntlBlenderDumper::renderScene",DM_MSG,"Objects dumped: "<<numGMs, 10);
	} else {
		if((glob_mpactive) && (glob_mpindex>0)) {
			// ok, nothing to do anyway...
		} else {
			errFatal("ntlBlenderDumper::renderScene","No objects to dump! Aborting...",SIMWORLD_INITERROR);
			return 1;
		}
	}

	// debug timing
	long stopTime = getTime();
	debMsgStd("ntlBlenderDumper::renderScene",DM_MSG,"Scene #"<<nrStr<<" dump time: "<< getTimeString(stopTime-startTime) <<" ", 10);

	// still render for preview...
	if(debugRender) {
		debMsgStd("ntlBlenderDumper::renderScene",DM_NOTIFY,"Performing preliminary render", 1);
		ntlWorld::renderScene(); }
	else {
		// next frame 
		glob->setAniCount( glob->getAniCount() +1 );
	}

	return 0;
}




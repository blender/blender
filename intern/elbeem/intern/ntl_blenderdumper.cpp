/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * Replaces std. raytracer, and only dumps time dep. objects to disc
 *
 *****************************************************************************/


#include <fstream>
#include <sys/types.h>

#include "utilities.h"
#include "ntl_matrices.h"
#include "ntl_blenderdumper.h"
#include "ntl_scene.h"

#include <zlib.h>



/******************************************************************************
 * Constructor
 *****************************************************************************/
ntlBlenderDumper::ntlBlenderDumper(string filename, bool commandlineMode) :
	ntlRaytracer(filename,commandlineMode),
	mpTrafo(NULL)
{
  ntlRenderGlobals *glob = mpGlob;
	AttributeList *pAttrs = glob->getBlenderAttributes();
	mpTrafo = new ntlMat4Gfx(0.0);
	mpTrafo->initId();
	(*mpTrafo) = pAttrs->readMat4Gfx("transform" , (*mpTrafo), "ntlBlenderDumper","mpTrafo", false ); 
	
	//for(int i=0; i<4;i++) { for(int j=0; j<4;j++) { errMsg("T"," "<<i<<","<<j<<" "<<mpTrafo->value[i][j]); } } // DEBUG
}



/******************************************************************************
 * Destructor
 *****************************************************************************/
ntlBlenderDumper::~ntlBlenderDumper()
{
	delete mpTrafo;
}

/******************************************************************************
 * Only dump time dep. objects to file
 *****************************************************************************/
int ntlBlenderDumper::renderScene( void )
{
	char nrStr[5];								/* nr conversion */
  ntlRenderGlobals *glob = mpGlob;
  ntlScene *scene = mpGlob->getScene();
	bool debugOut = true;
#if ELBEEM_BLENDER==1
	debugOut = false;
#endif // ELBEEM_BLENDER==1

	// output path
	/*std::ostringstream ecrpath("");
	ecrpath << "/tmp/ecr_" << getpid() <<"/";
	// make sure the dir exists
	std::ostringstream ecrpath_create("");
	ecrpath_create << "mkdir " << ecrpath.str();
	system( ecrpath_create.str().c_str() );
	// */

	vector<string> hideObjs; // geom shaders to hide 
	vector<string> gmName; 	 // gm names
	vector<string> gmMat;    // materials for gm
	int numGMs = 0;					 // no. of .obj models created

	if(debugOut) debMsgStd("ntlBlenderDumper::renderScene",DM_NOTIFY,"Dumping geometry data", 1);
  long startTime = getTime();

	/* check if picture already exists... */
	snprintf(nrStr, 5, "%04d", glob->getAniCount() );

  // local scene vars
  vector<ntlTriangle> Triangles;
  vector<ntlVec3Gfx>  Vertices;
  vector<ntlVec3Gfx>  VertNormals;

	/* init geometry array, first all standard objects */
	int idCnt = 0;          // give IDs to objects
	for (vector<ntlGeometryClass*>::iterator iter = scene->getGeoClasses()->begin();
			iter != scene->getGeoClasses()->end(); iter++) {
		if(!(*iter)->getVisible()) continue;
		int tid = (*iter)->getTypeId();

		if(tid & GEOCLASSTID_OBJECT) {
			// normal geom. objects dont change... -> ignore
			//if(buildInfo) debMsgStd("ntlBlenderDumper::BuildScene",DM_MSG,"added GeoObj "<<geoobj->getName(), 8 );
		}
		if(tid & GEOCLASSTID_SHADER) {
			ntlGeometryShader *geoshad = (ntlGeometryShader*)(*iter); //dynamic_cast<ntlGeometryShader*>(*iter);
			hideObjs.push_back( (*iter)->getName() );
			for (vector<ntlGeometryObject*>::iterator siter = geoshad->getObjectsBegin();
					siter != geoshad->getObjectsEnd();
					siter++) {
				if(debugOut) debMsgStd("ntlBlenderDumper::BuildScene",DM_MSG,"added shader geometry "<<(*siter)->getName(), 8);
				
				// only dump geo shader objects
				Triangles.clear();
				Vertices.clear();
				VertNormals.clear();
				(*siter)->initialize( mpGlob );
				(*siter)->getTriangles(&Triangles, &Vertices, &VertNormals, idCnt);
				idCnt ++;

				// always dump mesh, even empty ones...
				//if(Vertices.size() <= 0) continue;
				//if(Triangles.size() <= 0) continue;

				for(size_t i=0; i<Vertices.size(); i++) {
					Vertices[i] = (*mpTrafo) * Vertices[i];
				}

				// dump to binary file
				std::ostringstream boutfilename("");
				//boutfilename << ecrpath.str() << glob->getOutFilename() <<"_"<< (*siter)->getName() <<"_" << nrStr << ".obj";
				boutfilename << glob->getOutFilename() <<"_"<< (*siter)->getName() <<"_" << nrStr << ".bobj";
				if(debugOut) debMsgStd("ntlBlenderDumper::renderScene",DM_MSG,"B-Dumping: "<< (*siter)->getName() 
						<<", triangles:"<<Triangles.size()<<", vertices:"<<Vertices.size()<<
						" to "<<boutfilename.str() , 7);
				bool isPreview = false;
				if( (*siter)->getName().find( "preview" ) != string::npos) {
					isPreview = true;
				}
				boutfilename << ".gz";

				// compress all bobj's except for preview ones...
				gzFile gzf;
				if(isPreview) {
					gzf = gzopen(boutfilename.str().c_str(), "wb1");
				} else {
					gzf = gzopen(boutfilename.str().c_str(), "wb9");
				}
				if (!gzf) {
					errMsg("ntlBlenderDumper::renderScene","Unable to open output '"<<boutfilename<<"' ");
					return 1; }
				
				int wri;
				float wrf;
				if(sizeof(wri)!=4) { errMsg("ntlBlenderDumper::renderScene","Invalid int size"); return 1; }
				wri = Vertices.size();
				gzwrite(gzf, &wri, sizeof(wri));
				for(size_t i=0; i<Vertices.size(); i++) {
					for(int j=0; j<3; j++) {
						wrf = Vertices[i][j];
						gzwrite(gzf, &wrf, sizeof(wrf)); }
				}

				// should be the same as Vertices.size
				wri = VertNormals.size();
				gzwrite(gzf, &wri, sizeof(wri));
				for(size_t i=0; i<VertNormals.size(); i++) {
					for(int j=0; j<3; j++) {
						wrf = VertNormals[i][j];
						gzwrite(gzf, &wrf, sizeof(wrf)); }
				}

				wri = Triangles.size();
				gzwrite(gzf, &wri, sizeof(wri));
				for(size_t i=0; i<Triangles.size(); i++) {
					for(int j=0; j<3; j++) {
						wri = Triangles[i].getPoints()[j];
						gzwrite(gzf, &wri, sizeof(wri)); }
				}
				gzclose( gzf );
				debMsgDirect(" Wrote: '"<<boutfilename.str()<<"'. ");
				numGMs++;
			}
		}

	}

	// output ecr config file
	if(numGMs>0) {
		if(debugOut) debMsgStd("ntlBlenderDumper::renderScene",DM_MSG,"Objects dumped: "<<numGMs, 10);
	} else {
		errFatal("ntlBlenderDumper::renderScene","No objects to dump! Aborting...",SIMWORLD_INITERROR);
		return 1;
	}

	/* next frame */
	//glob->setAniCount( glob->getAniCount() +1 );
	long stopTime = getTime();
	debMsgStd("ntlBlenderDumper::renderScene",DM_MSG,"Scene #"<<nrStr<<" dump time: "<< getTimeString(stopTime-startTime) <<" ", 10);

	// still render for preview...
	if(debugOut) {
		debMsgStd("ntlBlenderDumper::renderScene",DM_NOTIFY,"Performing preliminary render", 1);
		ntlRaytracer::renderScene(); }
	else {
		// next frame 
		glob->setAniCount( glob->getAniCount() +1 );
	}

	return 0;
}




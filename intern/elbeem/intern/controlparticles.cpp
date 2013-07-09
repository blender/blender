/** \file elbeem/intern/controlparticles.cpp
 *  \ingroup elbeem
 */
// --------------------------------------------------------------------------
//
// El'Beem - the visual lattice boltzmann freesurface simulator
// All code distributed as part of El'Beem is covered by the version 2 of the 
// GNU General Public License. See the file COPYING for details.  
//
// Copyright 2008 Nils Thuerey , Richard Keiser, Mark Pauly, Ulrich Ruede
//
// implementation of control particle handling
//
// --------------------------------------------------------------------------

// indicator for LBM inclusion
#include "ntl_geometrymodel.h"
#include "ntl_world.h"
#include "solver_class.h"
#include "controlparticles.h"
#include "mvmcoords.h"
#include <zlib.h>

#ifndef sqrtf
#define sqrtf sqrt
#endif

// brute force circle test init in initTimeArray
// replaced by mDebugInit
//#define CP_FORCECIRCLEINIT 0


void ControlParticles::initBlenderTest() {
	mPartSets.clear();

	ControlParticleSet cps;
	mPartSets.push_back(cps);
	int setCnt = mPartSets.size()-1;
	ControlParticle p; 

	// set for time zero
	mPartSets[setCnt].time = 0.;

	// add single particle 
	p.reset();
	p.pos = LbmVec(0.5, 0.5, -0.5);
	mPartSets[setCnt].particles.push_back(p);

	// add second set for animation
	mPartSets.push_back(cps);
	setCnt = mPartSets.size()-1;
	mPartSets[setCnt].time = 0.15;

	// insert new position
	p.reset();
	p.pos = LbmVec(-0.5, -0.5, 0.5);
	mPartSets[setCnt].particles.push_back(p);

	// applyTrafos();
	initTime(0. , 1.);
}

// blender control object gets converted to mvm flui control object
int ControlParticles::initFromObject(ntlGeometryObjModel *model) {
	vector<ntlTriangle> triangles;
	vector<ntlVec3Gfx> vertices;
	vector<ntlVec3Gfx> normals;
	
	/*
	model->loadBobjModel(string(infile));
	
	model->setLoaded(true);
	
	model->setGeoInitId(gid);
	
	
	printf("a animated? %d\n", model->getIsAnimated());
	printf("b animated? %d\n", model->getMeshAnimated());
	*/
	
	model->setGeoInitType(FGI_FLUID);
	
	model->getTriangles(mCPSTimeStart, &triangles, &vertices, &normals, 1 ); 
	// model->applyTransformation(mCPSTimeStart, &vertices, &normals, 0, vertices.size(), true);
	
	// valid mesh?
	if(triangles.size() <= 0) {
		return 0;
	}

	ntlRenderGlobals *glob = new ntlRenderGlobals;
	ntlScene *genscene = new ntlScene( glob, false );
	genscene->addGeoClass(model);
	genscene->addGeoObject(model);
	genscene->buildScene(0., false);
	char treeFlag = (1<<(4+model->getGeoInitId()));

	ntlTree *tree = new ntlTree( 
	15, 8,  // TREEwarning - fixed values for depth & maxtriangles here...
	genscene, treeFlag );

	// TODO? use params
	ntlVec3Gfx start,end;
	model->getExtends(start,end);
	/*
	printf("start - x: %f, y: %f, z: %f\n", start[0], start[1], start[2]);
	printf("end   - x: %f, y: %f, z: %f\n", end[0], end[1], end[2]);
	printf("mCPSWidth: %f\n");
*/
	LbmFloat width = mCPSWidth;
	if(width<=LBM_EPSILON) { errMsg("ControlParticles::initFromMVMCMesh","Invalid mCPSWidth! "<<mCPSWidth); width=mCPSWidth=0.1; }
	ntlVec3Gfx org = start+ntlVec3Gfx(width*0.5);
	gfxReal distance = -1.;
	vector<ntlVec3Gfx> inspos;
	
	// printf("distance: %f, width: %f\n", distance, width);
	
	while(org[2]<end[2]) {
		while(org[1]<end[1]) {
			while(org[0]<end[0]) {
				if(checkPointInside(tree, org, distance)) {
					inspos.push_back(org);
				}
				// TODO optimize, use distance
				org[0] += width;
			}
			org[1] += width;
			org[0] = start[0];
		}
		org[2] += width;
		org[1] = start[1];
	}
	
	// printf("inspos.size(): %d\n", inspos.size());

	MeanValueMeshCoords mvm;
	mvm.calculateMVMCs(vertices,triangles, inspos, mCPSWeightFac);
	vector<ntlVec3Gfx> ninspos;
	mvm.transfer(vertices, ninspos);

	// init first set, check dist
	ControlParticleSet firstcps; //T
	mPartSets.push_back(firstcps);
	mPartSets[mPartSets.size()-1].time = mCPSTimeStart;
	vector<bool> useCP;

	for(int i=0; i<(int)inspos.size(); i++) {
		ControlParticle p; p.reset();
		p.pos = vec2L(inspos[i]);
		
		bool usecpv = true;

		mPartSets[mPartSets.size()-1].particles.push_back(p);
		useCP.push_back(usecpv);
	}

	// init further sets, temporal mesh sampling
	double tsampling = mCPSTimestep;
	// printf("tsampling: %f, ninspos.size(): %d, mCPSTimeEnd: %f\n", tsampling, ninspos.size(), mCPSTimeEnd);
	
	int tcnt=0;
	for(double t=mCPSTimeStart+tsampling; ((t<mCPSTimeEnd) && (ninspos.size()>0.)); t+=tsampling) {
		ControlParticleSet nextcps; //T
		mPartSets.push_back(nextcps);
		mPartSets[mPartSets.size()-1].time = (gfxReal)t;

		vertices.clear(); triangles.clear(); normals.clear();
		model->getTriangles(t, &triangles, &vertices, &normals, 1 );
		mvm.transfer(vertices, ninspos);
		
		tcnt++;
		for(size_t i=0; i < ninspos.size(); i++) {
			
			if(useCP[i]) {
				ControlParticle p; p.reset();
				p.pos = vec2L(ninspos[i]);
				mPartSets[mPartSets.size()-1].particles.push_back(p);
			}
		}
	}
	
	model->setGeoInitType(FGI_CONTROL);

	delete tree;
	delete genscene;
	delete glob;
	
	// do reverse here
	if(model->getGeoPartSlipValue())
	{
		mirrorTime();
	}
	
	return 1;
}


// init all zero / defaults for a single particle
void ControlParticle::reset() {
	pos = LbmVec(0.,0.,0.);
	vel = LbmVec(0.,0.,0.);
	influence = 1.;
	size = 1.;
#ifndef LBMDIM
#ifdef MAIN_2D
	rotaxis = LbmVec(0.,1.,0.); // SPH xz
#else // MAIN_2D
	// 3d - roate in xy plane, vortex
	rotaxis = LbmVec(0.,0.,1.);
	// 3d - rotate for wave
	//rotaxis = LbmVec(0.,1.,0.);
#endif // MAIN_2D
#else // LBMDIM
	rotaxis = LbmVec(0.,1.,0.); // LBM xy , is swapped afterwards
#endif // LBMDIM

	density = 0.;
	densityWeight = 0.;
	avgVelAcc = avgVel = LbmVec(0.);
	avgVelWeight = 0.;
}


// default preset/empty init
ControlParticles::ControlParticles() :
	_influenceTangential(0.f),
	_influenceAttraction(0.f),
	_influenceVelocity(0.f),
	_influenceMaxdist(0.f),
	_radiusAtt(1.0f),
	_radiusVel(1.0f),
	_radiusMinMaxd(2.0f),
	_radiusMaxd(3.0f),
	_currTime(-1.0), _currTimestep(1.),
	_initTimeScale(1.), 
	_initPartOffset(0.), _initPartScale(1.),
	_initLastPartOffset(0.), _initLastPartScale(1.),
	_initMirror(""),
	_fluidSpacing(1.), _kernelWeight(-1.),
	_charLength(1.), _charLengthInv(1.),
	mvCPSStart(-10000.), mvCPSEnd(10000.),
	mCPSWidth(0.1), mCPSTimestep(0.02), // was 0.05
	mCPSTimeStart(0.), mCPSTimeEnd(0.5), mCPSWeightFac(1.),
	mDebugInit(0)
{
	_radiusAtt = 0.15f;
	_radiusVel = 0.15f;
	_radiusMinMaxd = 0.16f;
	_radiusMaxd = 0.3;

	_influenceAttraction = 0.f;
	_influenceTangential = 0.f;
	_influenceVelocity = 0.f;
	// 3d tests */
}


 
ControlParticles::~ControlParticles() {
	// nothing to do...
}

LbmFloat ControlParticles::getControlTimStart() {
	if(mPartSets.size()>0) { return mPartSets[0].time; }
	return -1000.;
}
LbmFloat ControlParticles::getControlTimEnd() {
	if(mPartSets.size()>0) { return mPartSets[mPartSets.size()-1].time; }
	return -1000.;
}

// calculate for delta t
void ControlParticles::setInfluenceVelocity(LbmFloat set, LbmFloat dt) {
	const LbmFloat dtInter = 0.01;
	LbmFloat facFv = 1.-set; //cparts->getInfluenceVelocity();
	// mLevel[mMaxRefine].timestep
	LbmFloat facNv = (LbmFloat)( 1.-pow( (double)facFv, (double)(dt/dtInter)) );
	//errMsg("vwcalc","ts:"<<dt<< " its:"<<(dt/dtInter) <<" fv"<<facFv<<" nv"<<facNv<<" test:"<< pow( (double)(1.-facNv),(double)(dtInter/dt))	);
	_influenceVelocity = facNv;
}

int ControlParticles::initExampleSet()
{
	// unused
	return 0;
}

int ControlParticles::getTotalSize()
{
	int s=0;
	for(int i=0; i<(int)mPartSets.size(); i++) {
		s+= mPartSets[i].particles.size();
	}
	return s;
}

// --------------------------------------------------------------------------
// load positions & timing from text file
// WARNING - make sure file has unix format, no win/dos linefeeds...
#define LINE_LEN 100
int ControlParticles::initFromTextFile(string filename)
{
	/*
	const bool debugRead = false;
	char line[LINE_LEN];
	line[LINE_LEN-1] = '\0';
	mPartSets.clear();
	if(filename.size()<2) return 0;

	// HACK , use "cparts" suffix as old
	// e.g. "cpart2" as new
	if(filename[ filename.size()-1 ]=='s') {
		return initFromTextFileOld(filename);
	}

	FILE *infile = fopen(filename.c_str(), "r");
	if(!infile) {
		errMsg("ControlParticles::initFromTextFile","unable to open '"<<filename<<"' " );
		// try to open as gz sequence
		if(initFromBinaryFile(filename)) { return 1; }
		// try mesh MVCM generation
		if(initFromMVCMesh(filename)) { return 1; }
		// failed...
		return 0;
	}

	int haveNo = false;
	int haveScale = false;
	int haveTime = false;
	int noParts = -1;
	int partCnt = 0;
	int setCnt = 0;
	//ControlParticle p; p.reset();
	// scale times by constant factor while reading
	LbmFloat timeScale= 1.0;
	int lineCnt = 0;
	bool abortParse = false;
#define LASTCP mPartSets[setCnt].particles[ mPartSets[setCnt].particles.size()-1 ]

	while( (!feof(infile)) && (!abortParse)) {
		lineCnt++;
		fgets(line, LINE_LEN, infile);

		//if(debugRead) printf("\nDEBUG%d r '%s'\n",lineCnt, line);
		if(!line) continue;
		size_t len = strlen(line);

		// skip empty lines and comments (#,//)
		if(len<1) continue;
		if( (line[0]=='#') || (line[0]=='\n') ) continue;
		if((len>1) && (line[0]=='/' && line[1]=='/')) continue;

		// debug remove newline
		if((len>=1)&&(line[len-1]=='\n')) line[len-1]='\0';

		switch(line[0]) {

		case 'N': { // total number of particles, more for debugging...
			noParts = atoi(line+2);
			if(noParts<=0) {
				errMsg("ControlParticles::initFromTextFile","file '"<<filename<<"' - invalid no of particles "<<noParts);
				mPartSets.clear(); fclose(infile); return 0;
			}
			if(debugRead) printf("CPDEBUG%d no parts '%d'\n",lineCnt, noParts );
			haveNo = true;
			} break;

		case 'T': { // global time scale
			timeScale *= (LbmFloat)atof(line+2);
			if(debugRead) printf("ControlParticles::initFromTextFile - line %d , set timescale '%f', org %f\n",lineCnt, timeScale , _initTimeScale);
			if(timeScale==0.) { fprintf(stdout,"ControlParticles::initFromTextFile - line %d ,error: timescale = 0.! reseting to 1 ...\n",lineCnt); timeScale=1.; }
			haveScale = true;
			} break;

		case 'I': { // influence settings, overrides others as of now...
			float val = (LbmFloat)atof(line+3);
			const char *setvar = "[invalid]";
			switch(line[1]) {
				//case 'f': { _influenceFalloff = val; setvar = "falloff"; } break;
				case 't': { _influenceTangential = val; setvar = "tangential"; } break;
				case 'a': { _influenceAttraction = val; setvar = "attraction"; } break;
				case 'v': { _influenceVelocity = val; setvar = "velocity"; } break;
				case 'm': { _influenceMaxdist = val; setvar = "maxdist"; } break;
				default: 
					fprintf(stdout,"ControlParticles::initFromTextFile (%s) - line %d , invalid influence setting %c, %f\n",filename.c_str() ,lineCnt, line[1], val);
			}
			if(debugRead) printf("CPDEBUG%d set influence '%s'=%f \n",lineCnt, setvar, val);
			} break;

		case 'R': { // radius settings, overrides others as of now...
			float val = (LbmFloat)atof(line+3);
			const char *setvar = "[invalid]";
			switch(line[1]) {
				case 'a': { _radiusAtt = val; setvar = "r_attraction"; } break;
				case 'v': { _radiusVel = val; setvar = "r_velocity"; } break;
				case 'm': { _radiusMaxd = val; setvar = "r_maxdist"; } break;
				default: 
					fprintf(stdout,"ControlParticles::initFromTextFile (%s) - line %d , invalid influence setting %c, %f\n",filename.c_str() ,lineCnt, line[1], val);
			}
			if(debugRead) printf("CPDEBUG%d set influence '%s'=%f \n",lineCnt, setvar, val);
			} break;

		case 'S': { // new particle set at time T
			ControlParticleSet cps;
			mPartSets.push_back(cps);
			setCnt = (int)mPartSets.size()-1;

			LbmFloat val = (LbmFloat)atof(line+2);
			mPartSets[setCnt].time = val * timeScale;
			if(debugRead) printf("CPDEBUG%d new set, time '%f', %d\n",lineCnt, mPartSets[setCnt].time, setCnt );
			haveTime = true;
			partCnt = -1;
			} break;

		case 'P':   // new particle with pos
		case 'n': { // new particle without pos
				if((!haveTime)||(setCnt<0)) { fprintf(stdout,"ControlParticles::initFromTextFile - line %d ,error: set missing!\n",lineCnt); abortParse=true; break; }
				partCnt++;
				if(partCnt>=noParts) {
					if(debugRead) printf("CPDEBUG%d partset done \n",lineCnt);
					haveTime = false;
				} else {
					ControlParticle p; p.reset();
					mPartSets[setCnt].particles.push_back(p);
				}
			} 
			// only new part, or new with pos?
			if(line[0] == 'n') break;

		// particle properties

		case 'p': { // new particle set at time T
			if((!haveTime)||(setCnt<0)||(mPartSets[setCnt].particles.size()<1)) { fprintf(stdout,"ControlParticles::initFromTextFile - line %d ,error|p: particle missing!\n",lineCnt); abortParse=true; break; }
			float px=0.,py=0.,pz=0.;
			if( sscanf(line+2,"%f %f %f",&px,&py,&pz) != 3) {
				fprintf(stdout,"CPDEBUG%d, unable to parse position!\n",lineCnt); abortParse=true; break; 
			}
			if(!(finite(px)&&finite(py)&&finite(pz))) { px=py=pz=0.; }
			LASTCP.pos[0] = px;
			LASTCP.pos[1] = py;
			LASTCP.pos[2] = pz; 
			if(debugRead) printf("CPDEBUG%d part%d,%d: position %f,%f,%f \n",lineCnt,setCnt,partCnt, px,py,pz);
			} break;

		case 's': { // particle size
			if((!haveTime)||(setCnt<0)||(mPartSets[setCnt].particles.size()<1)) { fprintf(stdout,"ControlParticles::initFromTextFile - line %d ,error|s: particle missing!\n",lineCnt); abortParse=true; break; }
			float ps=1.;
			if( sscanf(line+2,"%f",&ps) != 1) {
				fprintf(stdout,"CPDEBUG%d, unable to parse size!\n",lineCnt); abortParse=true; break; 
			}
			if(!(finite(ps))) { ps=0.; }
			LASTCP.size = ps;
			if(debugRead) printf("CPDEBUG%d part%d,%d: size %f \n",lineCnt,setCnt,partCnt, ps);
			} break;

		case 'i': { // particle influence
			if((!haveTime)||(setCnt<0)||(mPartSets[setCnt].particles.size()<1)) { fprintf(stdout,"ControlParticles::initFromTextFile - line %d ,error|i: particle missing!\n",lineCnt); abortParse=true; break; }
			float pinf=1.;
			if( sscanf(line+2,"%f",&pinf) != 1) {
				fprintf(stdout,"CPDEBUG%d, unable to parse size!\n",lineCnt); abortParse=true; break; 
			}
			if(!(finite(pinf))) { pinf=0.; }
			LASTCP.influence = pinf;
			if(debugRead) printf("CPDEBUG%d part%d,%d: influence %f \n",lineCnt,setCnt,partCnt, pinf);
			} break;

		case 'a': { // rotation axis
			if((!haveTime)||(setCnt<0)||(mPartSets[setCnt].particles.size()<1)) { fprintf(stdout,"ControlParticles::initFromTextFile - line %d ,error|a: particle missing!\n",lineCnt); abortParse=true; break; }
			float px=0.,py=0.,pz=0.;
			if( sscanf(line+2,"%f %f %f",&px,&py,&pz) != 3) {
				fprintf(stdout,"CPDEBUG%d, unable to parse rotaxis!\n",lineCnt); abortParse=true; break; 
			}
			if(!(finite(px)&&finite(py)&&finite(pz))) { px=py=pz=0.; }
			LASTCP.rotaxis[0] = px;
			LASTCP.rotaxis[1] = py;
			LASTCP.rotaxis[2] = pz; 
			if(debugRead) printf("CPDEBUG%d part%d,%d: rotaxis %f,%f,%f \n",lineCnt,setCnt,partCnt, px,py,pz);
			} break;


		default:
			if(debugRead) printf("CPDEBUG%d ignored: '%s'\n",lineCnt, line );
			break;
		}
	}
	if(debugRead && abortParse) printf("CPDEBUG aborted parsing after set... %d\n",(int)mPartSets.size() );

	// sanity check
	for(int i=0; i<(int)mPartSets.size(); i++) {
		if( (int)mPartSets[i].particles.size()!=noParts) {
			fprintf(stdout,"ControlParticles::initFromTextFile (%s) - invalid no of particles in set %d, is:%d, shouldbe:%d \n",filename.c_str() ,i,(int)mPartSets[i].particles.size(), noParts);
			mPartSets.clear();
			fclose(infile);
			return 0;
		}
	}

	// print stats
	printf("ControlParticles::initFromTextFile (%s): Read %d sets, each %d particles\n",filename.c_str() ,
			(int)mPartSets.size(), noParts );
	if(mPartSets.size()>0) {
		printf("ControlParticles::initFromTextFile (%s): Time: %f,%f\n",filename.c_str() ,mPartSets[0].time, mPartSets[mPartSets.size()-1].time );
	}
	
	// done...
	fclose(infile);
	applyTrafos();
	*/
	return 1;
}


int ControlParticles::initFromTextFileOld(string filename)
{
	/*
	const bool debugRead = false;
	char line[LINE_LEN];
	line[LINE_LEN-1] = '\0';
	mPartSets.clear();
	if(filename.size()<1) return 0;

	FILE *infile = fopen(filename.c_str(), "r");
	if(!infile) {
		fprintf(stdout,"ControlParticles::initFromTextFileOld - unable to open '%s'\n",filename.c_str() );
		return 0;
	}

	int haveNo = false;
	int haveScale = false;
	int haveTime = false;
	int noParts = -1;
	int coordCnt = 0;
	int partCnt = 0;
	int setCnt = 0;
	ControlParticle p; p.reset();
	// scale times by constant factor while reading
	LbmFloat timeScale= 1.0;
	int lineCnt = 0;

	while(!feof(infile)) {
		lineCnt++;
		fgets(line, LINE_LEN, infile);

		if(debugRead) printf("\nDEBUG%d r '%s'\n",lineCnt, line);

		if(!line) continue;
		size_t len = strlen(line);

		// skip empty lines and comments (#,//)
		if(len<1) continue;
		if( (line[0]=='#') || (line[0]=='\n') ) continue;
		if((len>1) && (line[0]=='/' && line[1]=='/')) continue;

		// debug remove newline
		if((len>=1)&&(line[len-1]=='\n')) line[len-1]='\0';

		// first read no. of particles
		if(!haveNo) {
			noParts = atoi(line);
			if(noParts<=0) {
				fprintf(stdout,"ControlParticles::initFromTextFileOld - invalid no of particles %d\n",noParts);
				mPartSets.clear();
				fclose(infile);
				return 0;
			}
			if(debugRead) printf("DEBUG%d noparts '%d'\n",lineCnt, noParts );
			haveNo = true;
		} 

		// then read time scale
		else if(!haveScale) {
			timeScale *= (LbmFloat)atof(line);
			if(debugRead) printf("DEBUG%d tsc '%f', org %f\n",lineCnt, timeScale , _initTimeScale);
			haveScale = true;
		} 

		// then get set time
		else if(!haveTime) {
			ControlParticleSet cps;
			mPartSets.push_back(cps);
			setCnt = (int)mPartSets.size()-1;

			LbmFloat val = (LbmFloat)atof(line);
			mPartSets[setCnt].time = val * timeScale;
			if(debugRead) printf("DEBUG%d time '%f', %d\n",lineCnt, mPartSets[setCnt].time, setCnt );
			haveTime = true;
		}

		// default read all parts
		else {
			LbmFloat val = (LbmFloat)atof(line);
			if(debugRead) printf("DEBUG: l%d s%d,particle%d '%f' %d,%d/%d\n",lineCnt,(int)mPartSets.size(),(int)mPartSets[setCnt].particles.size(), val ,coordCnt,partCnt,noParts);
			p.pos[coordCnt] = val;
			coordCnt++;
			if(coordCnt>=3) {
				mPartSets[setCnt].particles.push_back(p);
				p.reset();
				coordCnt=0;
				partCnt++;
			}
			if(partCnt>=noParts) {
				partCnt = 0;
				haveTime = false;
			}
			//if(debugRead) printf("DEBUG%d par2 %d,%d/%d\n",lineCnt, coordCnt,partCnt,noParts);
		}
		//read pos, vel ...
	}

	// sanity check
	for(int i=0; i<(int)mPartSets.size(); i++) {
		if( (int)mPartSets[i].particles.size()!=noParts) {
			fprintf(stdout,"ControlParticles::initFromTextFileOld - invalid no of particles in set %d, is:%d, shouldbe:%d \n",i,(int)mPartSets[i].particles.size(), noParts);
			mPartSets.clear();
			fclose(infile);
			return 0;
		}
	}
	// print stats
	printf("ControlParticles::initFromTextFileOld: Read %d sets, each %d particles\n",
			(int)mPartSets.size(), noParts );
	if(mPartSets.size()>0) {
		printf("ControlParticles::initFromTextFileOld: Time: %f,%f\n",mPartSets[0].time, mPartSets[mPartSets.size()-1].time );
	}
	
	// done...
	fclose(infile);
	applyTrafos();
	*/
	return 1;
}

// load positions & timing from gzipped binary file
int ControlParticles::initFromBinaryFile(string filename) {
	mPartSets.clear();
	if(filename.size()<1) return 0;
	int fileNotFound=0;
	int fileFound=0;
	char ofile[256];

	for(int set=0; ((set<10000)&&(fileNotFound<10)); set++) {
		snprintf(ofile,256,"%s%04d.gz",filename.c_str(),set);
		//errMsg("ControlParticle::initFromBinaryFile","set"<<set<<" notf"<<fileNotFound<<" ff"<<fileFound);

		gzFile gzf;
		gzf = gzopen(ofile, "rb");
		if (!gzf) {
			//errMsg("ControlParticles::initFromBinaryFile","Unable to open file for reading '"<<ofile<<"' "); 
			fileNotFound++;
			continue;
		}
		fileNotFound=0;
		fileFound++;

		ControlParticleSet cps;
		mPartSets.push_back(cps);
		int setCnt = (int)mPartSets.size()-1;
		//LbmFloat val = (LbmFloat)atof(line+2);
		mPartSets[setCnt].time = (gfxReal)set;

		int totpart = 0;
		gzread(gzf, &totpart, sizeof(totpart));

		for(int a=0; a<totpart; a++) {
			int ptype=0;
			float psize=0.0;
			ntlVec3Gfx ppos,pvel;
			gzread(gzf, &ptype, sizeof(ptype));
			gzread(gzf, &psize, sizeof(float));

			for (int j=0; j<3; j++) { gzread(gzf, &ppos[j], sizeof(float)); }
			for (int j=0; j<3; j++) { gzread(gzf, &pvel[j], sizeof(float)); }

			ControlParticle p; 
			p.reset();
			p.pos = vec2L(ppos);
			mPartSets[setCnt].particles.push_back(p);
		} 

		gzclose(gzf);
		//errMsg("ControlParticle::initFromBinaryFile","Read set "<<ofile<<", #"<<mPartSets[setCnt].particles.size() ); // DEBUG
	} // sets

	if(fileFound==0) return 0;
	applyTrafos();
	return 1;
}

int globCPIProblems =0;
bool ControlParticles::checkPointInside(ntlTree *tree, ntlVec3Gfx org, gfxReal &distance) {
	// warning - stripped down version of geoInitCheckPointInside
	const int globGeoInitDebug = 0;
	const int  flags = FGI_FLUID;
	org += ntlVec3Gfx(0.0001);
	ntlVec3Gfx dir = ntlVec3Gfx(1.0, 0.0, 0.0);
	int OId = -1;
	ntlRay ray(org, dir, 0, 1.0, NULL);
	bool done = false;
	bool inside = false;
	int mGiObjInside = 0; 
	LbmFloat mGiObjDistance = -1.0; 
	LbmFloat giObjFirstHistSide = 0; 
	
	// if not inside, return distance to first hit
	gfxReal firstHit=-1.0;
	int     firstOId = -1;
	if(globGeoInitDebug) errMsg("IIIstart"," isect "<<org);

	while(!done) {
		// find first inside intersection
		ntlTriangle *triIns = NULL;
		distance = -1.0;
		ntlVec3Gfx normal(0.0);
		tree->intersectX(ray,distance,normal, triIns, flags, true);
		if(triIns) {
			ntlVec3Gfx norg = ray.getOrigin() + ray.getDirection()*distance;
			LbmFloat orientation = dot(normal, dir);
			OId = triIns->getObjectId();
			if(orientation<=0.0) {
				// outside hit
				normal *= -1.0;
				mGiObjInside++;
				if(giObjFirstHistSide==0) giObjFirstHistSide = 1;
				if(globGeoInitDebug) errMsg("IIO"," oid:"<<OId<<" org"<<org<<" norg"<<norg<<" orient:"<<orientation);
			} else {
				// inside hit
				mGiObjInside++;
				if(mGiObjDistance<0.0) mGiObjDistance = distance;
				if(globGeoInitDebug) errMsg("III"," oid:"<<OId<<" org"<<org<<" norg"<<norg<<" orient:"<<orientation);
				if(giObjFirstHistSide==0) giObjFirstHistSide = -1;
			}
			norg += normal * getVecEpsilon();
			ray = ntlRay(norg, dir, 0, 1.0, NULL);
			// remember first hit distance, in case we're not 
			// inside anything
			if(firstHit<0.0) {
				firstHit = distance;
				firstOId = OId;
			}
		} else {
			// no more intersections... return false
			done = true;
		}
	}

	distance = -1.0;
	if(mGiObjInside>0) {
		bool mess = false;
		if((mGiObjInside%2)==1) {
			if(giObjFirstHistSide != -1) mess=true;
		} else {
			if(giObjFirstHistSide !=  1) mess=true;
		}
		if(mess) {
			// ?
			//errMsg("IIIproblem","At "<<org<<" obj  inside:"<<mGiObjInside<<" firstside:"<<giObjFirstHistSide );
			globCPIProblems++;
			mGiObjInside++; // believe first hit side...
		}
	}

	if(globGeoInitDebug) errMsg("CHIII"," ins="<<mGiObjInside<<" t"<<mGiObjDistance<<" d"<<distance);
	if(((mGiObjInside%2)==1)&&(mGiObjDistance>0.0)) {
		if(  (distance<0.0)                             || // first intersection -> good
				((distance>0.0)&&(distance>mGiObjDistance)) // more than one intersection -> use closest one
			) {						
			distance = mGiObjDistance;
			OId = 0;
			inside = true;
		} 
	}

	if(!inside) {
		distance = firstHit;
		OId = firstOId;
	}
	if(globGeoInitDebug) errMsg("CHIII","ins"<<inside<<"  fh"<<firstHit<<" fo"<<firstOId<<" - h"<<distance<<" o"<<OId);

	return inside;
}
int ControlParticles::initFromMVCMesh(string filename) {
	myTime_t mvmstart = getTime(); 
	ntlGeometryObjModel *model = new ntlGeometryObjModel();
	int gid=1;
	char infile[256];
	vector<ntlTriangle> triangles;
	vector<ntlVec3Gfx> vertices;
	vector<ntlVec3Gfx> normals;
	snprintf(infile,256,"%s.bobj.gz", filename.c_str() );
	model->loadBobjModel(string(infile));
	model->setLoaded(true);
	model->setGeoInitId(gid);
	model->setGeoInitType(FGI_FLUID);
	debMsgStd("ControlParticles::initFromMVMCMesh",DM_MSG,"infile:"<<string(infile) ,4);

	//getTriangles(double t,  vector<ntlTriangle> *triangles, vector<ntlVec3Gfx> *vertices, vector<ntlVec3Gfx> *normals, int objectId );
	model->getTriangles(mCPSTimeStart, &triangles, &vertices, &normals, 1 ); 
	debMsgStd("ControlParticles::initFromMVMCMesh",DM_MSG," tris:"<<triangles.size()<<" verts:"<<vertices.size()<<" norms:"<<normals.size() , 2);
	
	// valid mesh?
	if(triangles.size() <= 0) {
		return 0;
	}

	ntlRenderGlobals *glob = new ntlRenderGlobals;
 	ntlScene *genscene = new ntlScene( glob, false );
	genscene->addGeoClass(model);
	genscene->addGeoObject(model);
	genscene->buildScene(0., false);
	char treeFlag = (1<<(4+gid));

	ntlTree *tree = new ntlTree( 
			15, 8,  // TREEwarning - fixed values for depth & maxtriangles here...
			genscene, treeFlag );

	// TODO? use params
	ntlVec3Gfx start,end;
	model->getExtends(start,end);

	LbmFloat width = mCPSWidth;
	if(width<=LBM_EPSILON) { errMsg("ControlParticles::initFromMVMCMesh","Invalid mCPSWidth! "<<mCPSWidth); width=mCPSWidth=0.1; }
	ntlVec3Gfx org = start+ntlVec3Gfx(width*0.5);
	gfxReal distance = -1.;
	vector<ntlVec3Gfx> inspos;
	int approxmax = (int)( ((end[0]-start[0])/width)*((end[1]-start[1])/width)*((end[2]-start[2])/width) );

	debMsgStd("ControlParticles::initFromMVMCMesh",DM_MSG,"start"<<start<<" end"<<end<<" w="<<width<<" maxp:"<<approxmax, 5);
	while(org[2]<end[2]) {
		while(org[1]<end[1]) {
			while(org[0]<end[0]) {
				if(checkPointInside(tree, org, distance)) {
					inspos.push_back(org);
					//inspos.push_back(org+ntlVec3Gfx(width));
					//inspos.push_back(start+end*0.5);
				}
				// TODO optimize, use distance
				org[0] += width;
			}
			org[1] += width;
			org[0] = start[0];
		}
		org[2] += width;
		org[1] = start[1];
	}
	debMsgStd("ControlParticles::initFromMVMCMesh",DM_MSG,"points: "<<inspos.size()<<" initproblems: "<<globCPIProblems,5 );

	MeanValueMeshCoords mvm;
	mvm.calculateMVMCs(vertices,triangles, inspos, mCPSWeightFac);
	vector<ntlVec3Gfx> ninspos;
	mvm.transfer(vertices, ninspos);

	// init first set, check dist
	ControlParticleSet firstcps; //T
	mPartSets.push_back(firstcps);
	mPartSets[mPartSets.size()-1].time = (gfxReal)0.;
	vector<bool> useCP;
	bool debugPos=false;

	for(int i=0; i<(int)inspos.size(); i++) {
		ControlParticle p; p.reset();
		p.pos = vec2L(inspos[i]);
		//errMsg("COMP "," "<<inspos[i]<<" vs "<<ninspos[i] );
		double cpdist = norm(inspos[i]-ninspos[i]);
		bool usecpv = true;
		if(debugPos) errMsg("COMP "," "<<cpdist<<usecpv);

		mPartSets[mPartSets.size()-1].particles.push_back(p);
		useCP.push_back(usecpv);
	}

	// init further sets, temporal mesh sampling
	double tsampling = mCPSTimestep;
	int totcnt = (int)( (mCPSTimeEnd-mCPSTimeStart)/tsampling ), tcnt=0;
	for(double t=mCPSTimeStart+tsampling; ((t<mCPSTimeEnd) && (ninspos.size()>0.)); t+=tsampling) {
		ControlParticleSet nextcps; //T
		mPartSets.push_back(nextcps);
		mPartSets[mPartSets.size()-1].time = (gfxReal)t;

		vertices.clear(); triangles.clear(); normals.clear();
		model->getTriangles(t, &triangles, &vertices, &normals, 1 );
		mvm.transfer(vertices, ninspos);
		if(tcnt%(totcnt/10)==1) debMsgStd("MeanValueMeshCoords::calculateMVMCs",DM_MSG,"Transferring animation, frame: "<<tcnt<<"/"<<totcnt,5 );
		tcnt++;
		for(int i=0; i<(int)ninspos.size(); i++) {
			if(debugPos) errMsg("COMP "," "<<norm(inspos[i]-ninspos[i]) );
			if(useCP[i]) {
				ControlParticle p; p.reset();
				p.pos = vec2L(ninspos[i]);
				mPartSets[mPartSets.size()-1].particles.push_back(p);
			}
		}
	}

	applyTrafos();

	myTime_t mvmend = getTime(); 
	debMsgStd("ControlParticle::initFromMVMCMesh",DM_MSG,"t:"<<getTimeString(mvmend-mvmstart)<<" ",7 );
	delete tree;
	delete genscene;
	delete glob;
//exit(1); // DEBUG
	return 1;
}

#define TRISWAP(v,a,b) { LbmFloat tmp = (v)[b]; (v)[b]=(v)[a]; (v)[a]=tmp; }
#define TRISWAPALL(v,a,b) {  \
			TRISWAP( (v).pos     ,a,b ); \
			TRISWAP( (v).vel     ,a,b ); \
			TRISWAP( (v).rotaxis ,a,b ); }

// helper function for LBM 2D -> swap Y and Z components everywhere
void ControlParticles::swapCoords(int a, int b) {
	//return;
	for(int i=0; i<(int)mPartSets.size(); i++) {
		for(int j=0; j<(int)mPartSets[i].particles.size(); j++) {
			TRISWAPALL( mPartSets[i].particles[j],a,b );
		}
	}
}

// helper function for LBM 2D -> mirror time
void ControlParticles::mirrorTime() {
	LbmFloat maxtime = mPartSets[mPartSets.size()-1].time;
	const bool debugTimeswap = false;
	
	for(int i=0; i<(int)mPartSets.size(); i++) {
		mPartSets[i].time = maxtime - mPartSets[i].time;
	}

	for(int i=0; i<(int)mPartSets.size()/2; i++) {
		ControlParticleSet cps = mPartSets[i];
		if(debugTimeswap) errMsg("TIMESWAP", " s"<<i<<","<<mPartSets[i].time<<"  and s"<<(mPartSets.size()-1-i)<<","<< mPartSets[mPartSets.size()-1-i].time <<"  mt:"<<maxtime );
		mPartSets[i] = mPartSets[mPartSets.size()-1-i];
		mPartSets[mPartSets.size()-1-i] = cps;
	}

	for(int i=0; i<(int)mPartSets.size(); i++) {
		if(debugTimeswap) errMsg("TIMESWAP", "done: s"<<i<<","<<mPartSets[i].time<<"  "<<mPartSets[i].particles.size() );
	}
}

// apply init transformations
void ControlParticles::applyTrafos() {
	// apply trafos
	for(int i=0; i<(int)mPartSets.size(); i++) {
		mPartSets[i].time *= _initTimeScale;
		/*for(int j=0; j<(int)mPartSets[i].particles.size(); j++) {
			for(int k=0; k<3; k++) {
				mPartSets[i].particles[j].pos[k] *= _initPartScale[k];
				mPartSets[i].particles[j].pos[k] += _initPartOffset[k];
			}
		} now done in initarray */
	}

	// mirror coords...
	for(int l=0; l<(int)_initMirror.length(); l++) {
		switch(_initMirror[l]) {
		case 'X':
		case 'x':
			//printf("ControlParticles::applyTrafos - mirror x\n");
			swapCoords(1,2);
			break;
		case 'Y':
		case 'y':
			//printf("ControlParticles::applyTrafos - mirror y\n");
			swapCoords(0,2);
			break;
		case 'Z':
		case 'z':
			//printf("ControlParticles::applyTrafos - mirror z\n");
			swapCoords(0,1);
			break;
		case 'T':
		case 't':
			//printf("ControlParticles::applyTrafos - mirror time\n");
			mirrorTime();
			break;
		case ' ':
		case '-':
		case '\n':
			break;
		default:
			//printf("ControlParticles::applyTrafos - mirror unknown %c !?\n", _initMirror[l] );
			break;
		}
	}

	// reset 2d positions
#if (CP_PROJECT2D==1) && ( defined(MAIN_2D) || LBMDIM==2 )
	for(size_t j=0; j<mPartSets.size(); j++) 
		for(size_t i=0; i<mPartSets[j].particles.size(); i++) {
			// DEBUG 
			mPartSets[j].particles[i].pos[1] = 0.f;
		}
#endif

#if defined(LBMDIM) 
	//? if( (getenv("ELBEEM_CPINFILE")) || (getenv("ELBEEM_CPOUTFILE")) ){ 
		// gui control test, don swap...
	//? } else {
		//? swapCoords(1,2); // LBM 2D -> swap Y and Z components everywhere
	//? }
#endif

	initTime(0.f, 0.f);
}

#undef TRISWAP

// --------------------------------------------------------------------------
// init for a given time
void ControlParticles::initTime(LbmFloat t, LbmFloat dt) 
{
	//fprintf(stdout, "CPINITTIME init %f\n",t);
	_currTime = t;
	if(mPartSets.size()<1) return;

	// init zero velocities
	initTimeArray(t, _particles);

	// calculate velocities from prev. timestep?
	if(dt>0.) {
		_currTimestep = dt;
		std::vector<ControlParticle> prevparts;
		initTimeArray(t-dt, prevparts);
		LbmFloat invdt = 1.0/dt;
		for(size_t j=0; j<_particles.size(); j++) {
			ControlParticle &p = _particles[j];
			ControlParticle &prevp = prevparts[j];
			for(int k=0; k<3; k++) {
				p.pos[k] *= _initPartScale[k];
				p.pos[k] += _initPartOffset[k];
				prevp.pos[k] *= _initLastPartScale[k];
				prevp.pos[k] += _initLastPartOffset[k];
			}
			p.vel = (p.pos - prevp.pos)*invdt;
		}

		if(0) {
			LbmVec avgvel(0.);
			for(size_t j=0; j<_particles.size(); j++) {
				avgvel += _particles[j].vel;
			}
			avgvel /= (LbmFloat)_particles.size();
			//fprintf(stdout," AVGVEL %f,%f,%f \n",avgvel[0],avgvel[1],avgvel[2]); // DEBUG
		}
	}
}

// helper, init given array
void ControlParticles::initTimeArray(LbmFloat t, std::vector<ControlParticle> &parts) {
	if(mPartSets.size()<1) return;

	if(parts.size()!=mPartSets[0].particles.size()) {
		//fprintf(stdout,"PRES \n");
		parts.resize(mPartSets[0].particles.size());
		// TODO reset all?
		for(size_t j=0; j<parts.size(); j++) {
			parts[j].reset();
		}
	}
	if(parts.size()<1) return;

	// debug inits
	if(mDebugInit==1) {
		// hard coded circle init
		for(size_t j=0; j<mPartSets[0].particles.size(); j++) {
			ControlParticle p = mPartSets[0].particles[j];
			// remember old
			p.density = parts[j].density;
			p.densityWeight = parts[j].densityWeight;
			p.avgVel = parts[j].avgVel;
			p.avgVelAcc = parts[j].avgVelAcc;
			p.avgVelWeight = parts[j].avgVelWeight;
			LbmVec ppos(0.); { // DEBUG
			const float tscale=10.;
			const float tprevo = 0.33;
			const LbmVec toff(50,50,0);
			const LbmVec oscale(30,30,0);
			ppos[0] =  cos(tscale* t - tprevo*(float)j + M_PI -0.1) * oscale[0] + toff[0];
			ppos[1] = -sin(tscale* t - tprevo*(float)j + M_PI -0.1) * oscale[1] + toff[1];
			ppos[2] =                               toff[2]; } // DEBUG
			p.pos = ppos;
			parts[j] = p;
			//errMsg("ControlParticle::initTimeArray","j:"<<j<<" p:"<<parts[j].pos );
		}
		return;
	}
	else if(mDebugInit==2) {
		// hard coded spiral init
		const float tscale=-10.;
		const float tprevo = 0.33;
		LbmVec   toff(50,0,-50);
		const LbmVec oscale(20,20,0);
		toff[2] += 30. * t +30.;
		for(size_t j=0; j<mPartSets[0].particles.size(); j++) {
			ControlParticle p = mPartSets[0].particles[j];
			// remember old
			p.density = parts[j].density;
			p.densityWeight = parts[j].densityWeight;
			p.avgVel = parts[j].avgVel;
			p.avgVelAcc = parts[j].avgVelAcc;
			p.avgVelWeight = parts[j].avgVelWeight;
			LbmVec ppos(0.); 
			ppos[1] =                               toff[2]; 
			LbmFloat zscal = (ppos[1]+100.)/200.;
			ppos[0] =  cos(tscale* t - tprevo*(float)j + M_PI -0.1) * oscale[0]*zscal + toff[0];
			ppos[2] = -sin(tscale* t - tprevo*(float)j + M_PI -0.1) * oscale[1]*zscal + toff[1];
			p.pos = ppos;
			parts[j] = p;

			toff[2] += 0.25;
		}
		return;
	}

	// use first set
	if((t<=mPartSets[0].time)||(mPartSets.size()==1)) {
		//fprintf(stdout,"PINI %f \n", t);
		//parts = mPartSets[0].particles;
		const int i=0;
		for(size_t j=0; j<mPartSets[i].particles.size(); j++) {
			ControlParticle p = mPartSets[i].particles[j];
			// remember old
			p.density = parts[j].density;
			p.densityWeight = parts[j].densityWeight;
			p.avgVel = parts[j].avgVel;
			p.avgVelAcc = parts[j].avgVelAcc;
			p.avgVelWeight = parts[j].avgVelWeight;
			parts[j] = p;
		}
		return;
	}

	for(int i=0; i<(int)mPartSets.size()-1; i++) {
		if((mPartSets[i].time<=t) && (mPartSets[i+1].time>t)) {
			LbmFloat d = mPartSets[i+1].time-mPartSets[i].time;
			LbmFloat f = (t-mPartSets[i].time)/d;
			LbmFloat omf = 1.0f - f;
	
			for(size_t j=0; j<mPartSets[i].particles.size(); j++) {
				ControlParticle *src1=&mPartSets[i  ].particles[j];
				ControlParticle *src2=&mPartSets[i+1].particles[j];
				ControlParticle &p = parts[j];
				// do linear interpolation
				p.pos     = src1->pos * omf     + src2->pos *f;
				p.vel     = LbmVec(0.); // reset, calculated later on src1->vel * omf     + src2->vel *f;
				p.rotaxis = src1->rotaxis * omf + src2->rotaxis *f;
				p.influence = src1->influence * omf + src2->influence *f;
				p.size    = src1->size * omf    + src2->size *f;
				// dont modify: density, densityWeight
			}
		}
	}

	// after last?
	if(t>=mPartSets[ mPartSets.size() -1 ].time) {
		//parts = mPartSets[ mPartSets.size() -1 ].particles;
		const int i= (int)mPartSets.size() -1;
		for(size_t j=0; j<mPartSets[i].particles.size(); j++) {
			ControlParticle p = mPartSets[i].particles[j];
			// restore
			p.density = parts[j].density;
			p.densityWeight = parts[j].densityWeight;
			p.avgVel = parts[j].avgVel;
			p.avgVelAcc = parts[j].avgVelAcc;
			p.avgVelWeight = parts[j].avgVelWeight;
			parts[j] = p;
		}
	}
}




// --------------------------------------------------------------------------

#define DEBUG_MODVEL 0

// recalculate 
void ControlParticles::calculateKernelWeight() {
	const bool debugKernel = true;

	// calculate kernel area with respect to particlesize/cellsize
	LbmFloat kernelw = -1.;
	LbmFloat kernelnorm = -1.;
	LbmFloat krad = (_radiusAtt*0.75); // FIXME  use real cone approximation...?
	//krad = (_influenceFalloff*1.);
#if (CP_PROJECT2D==1) && (defined(MAIN_2D) || LBMDIM==2)
	kernelw = CP_PI*krad*krad;
	kernelnorm = 1.0 / (_fluidSpacing * _fluidSpacing);
#else // 2D
	kernelw = CP_PI*krad*krad*krad* (4./3.);
	kernelnorm = 1.0 / (_fluidSpacing * _fluidSpacing * _fluidSpacing);
#endif // MAIN_2D

	if(debugKernel) debMsgStd("ControlParticles::calculateKernelWeight",DM_MSG,"kw"<<kernelw<<", norm"<<
			kernelnorm<<", w*n="<<(kernelw*kernelnorm)<<", rad"<<krad<<", sp"<<_fluidSpacing<<"  ", 7);
	LbmFloat kernelws = kernelw*kernelnorm;
	_kernelWeight = kernelws;
	if(debugKernel) debMsgStd("ControlParticles::calculateKernelWeight",DM_MSG,"influence f="<<_radiusAtt<<" t="<<
			_influenceTangential<<" a="<<_influenceAttraction<<" v="<<_influenceVelocity<<" kweight="<<_kernelWeight, 7);
	if(_kernelWeight<=0.) {
		errMsg("ControlParticles::calculateKernelWeight", "invalid kernel! "<<_kernelWeight<<", resetting");
		_kernelWeight = 1.;
	}
}

void 
ControlParticles::prepareControl(LbmFloat simtime, LbmFloat dt, ControlParticles *motion) {
	debMsgStd("ControlParticle::prepareControl",DM_MSG," simtime="<<simtime<<" dt="<<dt<<" ", 5);

	//fprintf(stdout,"PREPARE \n");
	LbmFloat avgdw = 0.;
	for(size_t i=0; i<_particles.size(); i++) {
		ControlParticle *cp = &_particles[i];

		if(this->getInfluenceAttraction()<0.) {
			cp->density= 
			cp->densityWeight = 1.0;
			continue;
		} 

		// normalize by kernel 
		//cp->densityWeight = (1.0 - (cp->density / _kernelWeight)); // store last
#if (CP_PROJECT2D==1) && (defined(MAIN_2D) || LBMDIM==2)
		cp->densityWeight = (1.0 - (cp->density / (_kernelWeight*cp->size*cp->size) )); // store last
#else // 2D
		cp->densityWeight = (1.0 - (cp->density / (_kernelWeight*cp->size*cp->size*cp->size) )); // store last
#endif // MAIN_2D

		if(i<10) debMsgStd("ControlParticle::prepareControl",DM_MSG,"kernelDebug i="<<i<<" densWei="<<cp->densityWeight<<" 1/kw"<<(1.0/_kernelWeight)<<" cpdensity="<<cp->density, 9 );
		if(cp->densityWeight<0.) cp->densityWeight=0.;
		if(cp->densityWeight>1.) cp->densityWeight=1.;
		
		avgdw += cp->densityWeight;
		// reset for next step
		cp->density = 0.; 

		if(cp->avgVelWeight>0.) {
		 cp->avgVel	= cp->avgVelAcc/cp->avgVelWeight; 
		 cp->avgVelWeight	= 0.; 
		 cp->avgVelAcc	= LbmVec(0.,0.,0.); 
		}
	}
	//if(debugKernel) for(size_t i=0; i<_particles.size(); i++) { ControlParticle *cp = &_particles[i]; fprintf(stdout,"A %f,%f \n",cp->density,cp->densityWeight); }
	avgdw /= (LbmFloat)(_particles.size());
	//if(motion) { printf("ControlParticle::kernel: avgdw:%f,  kw%f, sp%f \n", avgdw, _kernelWeight, _fluidSpacing); }
	
	//if((simtime>=0.) && (simtime != _currTime)) 
	initTime(simtime, dt);

	if((motion) && (motion->getSize()>0)){
		ControlParticle *motionp = motion->getParticle(0);
		//printf("ControlParticle::prepareControl motion: pos[%f,%f,%f] vel[%f,%f,%f] \n", motionp->pos[0], motionp->pos[1], motionp->pos[2], motionp->vel[0], motionp->vel[1], motionp->vel[2] );
		for(size_t i=0; i<_particles.size(); i++) {
			ControlParticle *cp = &_particles[i];
			cp->pos = cp->pos + motionp->pos;
			cp->vel = cp->vel + motionp->vel;
			cp->size = cp->size * motionp->size;
			cp->influence = cp->size * motionp->influence;
		}
	}
	
	// reset to radiusAtt by default
	if(_radiusVel==0.) _radiusVel = _radiusAtt;
	if(_radiusMinMaxd==0.) _radiusMinMaxd = _radiusAtt;
	if(_radiusMaxd==0.) _radiusMaxd = 2.*_radiusAtt;
	// has to be radiusVel<radiusAtt<radiusMinMaxd<radiusMaxd
	if(_radiusVel>_radiusAtt) _radiusVel = _radiusAtt;
	if(_radiusAtt>_radiusMinMaxd) _radiusAtt = _radiusMinMaxd;
	if(_radiusMinMaxd>_radiusMaxd) _radiusMinMaxd = _radiusMaxd;

	//printf("ControlParticle::radii vel:%f att:%f min:%f max:%f \n", _radiusVel,_radiusAtt,_radiusMinMaxd,_radiusMaxd);
	// prepareControl done
}

void ControlParticles::finishControl(std::vector<ControlForces> &forces, LbmFloat iatt, LbmFloat ivel, LbmFloat imaxd) {

	//const LbmFloat iatt  = this->getInfluenceAttraction() * this->getCurrTimestep();
	//const LbmFloat ivel  = this->getInfluenceVelocity();
	//const LbmFloat imaxd = this->getInfluenceMaxdist() * this->getCurrTimestep();
	// prepare for usage
	iatt  *= this->getCurrTimestep();
	ivel  *= 1.; // not necessary!
	imaxd *= this->getCurrTimestep();

	// skip when size=0
	for(int i=0; i<(int)forces.size(); i++) {
		if(DEBUG_MODVEL) fprintf(stdout, "CPFORGF %d , wf:%f,f:%f,%f,%f  ,  v:%f,%f,%f   \n",i, forces[i].weightAtt, forces[i].forceAtt[0],forces[i].forceAtt[1],forces[i].forceAtt[2],   forces[i].forceVel[0], forces[i].forceVel[1], forces[i].forceVel[2] );
		LbmFloat cfweight = forces[i].weightAtt; // always normalize
		if((cfweight!=0.)&&(iatt!=0.)) {
			// multiple kernels, normalize - note this does not normalize in d>r/2 region
			if(ABS(cfweight)>1.) { cfweight = 1.0/cfweight; }
			// multiply iatt afterwards to allow stronger force
			cfweight *= iatt;
			forces[i].forceAtt *= cfweight;
		} else {
			forces[i].weightAtt =  0.;
			forces[i].forceAtt = LbmVec(0.);
		}

		if( (cfweight==0.) && (imaxd>0.) && (forces[i].maxDistance>0.) ) {
			forces[i].forceMaxd *= imaxd;
		} else {
			forces[i].maxDistance=  0.;
			forces[i].forceMaxd = LbmVec(0.);
		}

		LbmFloat cvweight = forces[i].weightVel; // always normalize
		if(cvweight>0.) {
			forces[i].forceVel /= cvweight;
			forces[i].compAv /= cvweight;
			// now modify cvweight, and write back
			// important, cut at 1 - otherwise strong vel. influences...
			if(cvweight>1.) { cvweight = 1.; }
			// thus cvweight is in the range of 0..influenceVelocity, currently not normalized by numCParts
			cvweight *= ivel;
			if(cvweight<0.) cvweight=0.; if(cvweight>1.) cvweight=1.;
			// LBM, FIXME todo use relaxation factor
			//pvel = (cvel*0.5 * cvweight) + (pvel * (1.0-cvweight)); 
			forces[i].weightVel = cvweight;

			//errMsg("COMPAV","i"<<i<<" compav"<<forces[i].compAv<<" forcevel"<<forces[i].forceVel<<" ");
		} else {
			forces[i].weightVel = 0.;
			if(forces[i].maxDistance==0.) forces[i].forceVel = LbmVec(0.);
			forces[i].compAvWeight = 0.;
			forces[i].compAv = LbmVec(0.);
		}
		if(DEBUG_MODVEL) fprintf(stdout, "CPFINIF %d , wf:%f,f:%f,%f,%f  ,  v:%f,%f,%f   \n",i, forces[i].weightAtt, forces[i].forceAtt[0],forces[i].forceAtt[1],forces[i].forceAtt[2],  forces[i].forceVel[0],forces[i].forceVel[1],forces[i].forceVel[2] );
	}

	// unused...
	if(DEBUG_MODVEL) fprintf(stdout,"MFC iatt:%f,%f ivel:%f,%f ifmd:%f,%f \n", iatt,_radiusAtt, ivel,_radiusVel, imaxd, _radiusMaxd);
	//for(size_t i=0; i<_particles.size(); i++) { ControlParticle *cp = &_particles[i]; fprintf(stdout," %f,%f,%f ",cp->density,cp->densityWeight, (1.0 - (12.0*cp->densityWeight))); }
	//fprintf(stdout,"\n\nCP DONE \n\n\n");
}


// --------------------------------------------------------------------------
// calculate forces at given position, and modify velocity
// according to timestep
void ControlParticles::calculateCpInfluenceOpt(ControlParticle *cp, LbmVec fluidpos, LbmVec fluidvel, ControlForces *force, LbmFloat fillFactor) {
	// dont reset, only add...
	// test distance, simple squared distance reject
	const LbmFloat cpfo = _radiusAtt*cp->size;

	LbmVec posDelta;
	if(DEBUG_MODVEL) fprintf(stdout, "CP at %f,%f,%f bef fw:%f, f:%f,%f,%f  , vw:%f, v:%f,%f,%f   \n",fluidpos[0],fluidpos[1],fluidpos[2], force->weightAtt, force->forceAtt[0], force->forceAtt[1], force->forceAtt[2], force->weightVel, force->forceVel[0], force->forceVel[1], force->forceVel[2]);
 	posDelta	= cp->pos - fluidpos;
#if LBMDIM==2 && (CP_PROJECT2D==1)
	posDelta[2] = 0.; // project to xy plane, z-velocity should already be gone...
#endif

	const LbmFloat distsqr = posDelta[0]*posDelta[0]+posDelta[1]*posDelta[1]+posDelta[2]*posDelta[2];
	if(DEBUG_MODVEL) fprintf(stdout, " Pd at %f,%f,%f d%f   \n",posDelta[0],posDelta[1],posDelta[2], distsqr);
	// cut at influence=0.5 , scaling not really makes sense
	if(cpfo*cpfo < distsqr) {
		/*if(cp->influence>0.5) {
			if(force->weightAtt == 0.) {
				if(force->maxDistance*force->maxDistance > distsqr) {
				const LbmFloat dis = sqrtf((float)distsqr);
				const LbmFloat sc = dis-cpfo;
				force->maxDistance = dis;
				force->forceMaxd = (posDelta)*(sc/dis);
				}
			} } */
		return;
	}
	force->weightAtt += 1e-6; // for distance
	force->maxDistance = 0.; // necessary for SPH?

	const LbmFloat pdistance = MAGNITUDE(posDelta);
	LbmFloat pdistinv = 0.;
	if(ABS(pdistance)>0.) pdistinv = 1./pdistance;
	posDelta *= pdistinv;

	LbmFloat falloffAtt = 0.; //CPKernel::kernel(cpfo * 1.0, pdistance);
	const LbmFloat qac = pdistance / cpfo ;
	if (qac < 1.0){ // return 0.;
		if(qac < 0.5) falloffAtt =  1.0f;
		else         falloffAtt = (1.0f - qac) * 2.0f;
	}

	// vorticity force:
	// - //LbmVec forceVort; 
	// - //CROSS(forceVort, posDelta, cp->rotaxis);
	// - //NORMALIZE(forceVort);
	// - if(falloffAtt>1.0) falloffAtt=1.0;

#if (CP_PROJECT2D==1) && (defined(MAIN_2D) || LBMDIM==2)
	// fillFactor *= 2.0 *0.75 * pdistance; // 2d>3d sampling
#endif // (CP_PROJECT2D==1) && (defined(MAIN_2D) || LBMDIM==2)
	
	LbmFloat signum = getInfluenceAttraction() > 0.0 ? 1.0 : -1.0;	
	cp->density += falloffAtt * fillFactor;
	force->forceAtt +=  posDelta *cp->densityWeight *cp->influence *signum; 
	force->weightAtt += falloffAtt*cp->densityWeight *cp->influence;
	
	LbmFloat falloffVel = 0.; //CPKernel::kernel(cpfo * 1.0, pdistance);
	const LbmFloat cpfv = _radiusVel*cp->size;
	if(cpfv*cpfv < distsqr) { return; }
	const LbmFloat qvc = pdistance / cpfo ;
	//if (qvc < 1.0){ 
		//if(qvc < 0.5) falloffVel =  1.0f;
		//else         falloffVel = (1.0f - qvc) * 2.0f;
	//}
	falloffVel = 1.-qvc;

	LbmFloat pvWeight; // = (1.0-cp->densityWeight) * _currTimestep * falloffVel;
	pvWeight = falloffVel *cp->influence; // std, without density influence
	//pvWeight *= (1.0-cp->densityWeight); // use inverse density weight
	//pvWeight *=      cp->densityWeight; // test, use density weight
	LbmVec modvel(0.);
	modvel += cp->vel * pvWeight;
	//pvWeight = 1.; modvel = partVel; // DEBUG!?

	if(pvWeight>0.) {
		force->forceVel += modvel;
		force->weightVel += pvWeight;

		cp->avgVelWeight += falloffVel;
		cp->avgVel += fluidvel;
	} 
	if(DEBUG_MODVEL) fprintf(stdout, "CP at %f,%f,%f aft fw:%f, f:%f,%f,%f  , vw:%f, v:%f,%f,%f   \n",fluidpos[0],fluidpos[1],fluidpos[2], force->weightAtt, force->forceAtt[0], force->forceAtt[1], force->forceAtt[2], force->weightVel, force->forceVel[0], force->forceVel[1], force->forceVel[2]);
	return;
}

void ControlParticles::calculateMaxdForce(ControlParticle *cp, LbmVec fluidpos, ControlForces *force) {
	if(force->weightAtt != 0.) return; // maxd force off
	if(cp->influence <= 0.5) return;   // ignore

	LbmVec posDelta;
	//if(DEBUG_MODVEL) fprintf(stdout, "CP at %f,%f,%f bef fw:%f, f:%f,%f,%f  , vw:%f, v:%f,%f,%f   \n",fluidpos[0],fluidpos[1],fluidpos[2], force->weightAtt, force->forceAtt[0], force->forceAtt[1], force->forceAtt[2], force->weightVel, force->forceVel[0], force->forceVel[1], force->forceVel[2]);
 	posDelta	= cp->pos - fluidpos;
#if LBMDIM==2 && (CP_PROJECT2D==1)
	posDelta[2] = 0.; // project to xy plane, z-velocity should already be gone...
#endif

	// dont reset, only add...
	// test distance, simple squared distance reject
	const LbmFloat distsqr = posDelta[0]*posDelta[0]+posDelta[1]*posDelta[1]+posDelta[2]*posDelta[2];
	
	// closer cp found
	if(force->maxDistance*force->maxDistance < distsqr) return;
	
	const LbmFloat dmin = _radiusMinMaxd*cp->size;
	if(distsqr<dmin*dmin) return; // inside min
	const LbmFloat dmax = _radiusMaxd*cp->size;
	if(distsqr>dmax*dmax) return; // outside


	if(DEBUG_MODVEL) fprintf(stdout, " Pd at %f,%f,%f d%f   \n",posDelta[0],posDelta[1],posDelta[2], distsqr);
	// cut at influence=0.5 , scaling not really makes sense
	const LbmFloat dis = sqrtf((float)distsqr);
	//const LbmFloat sc = dis - dmin;
	const LbmFloat sc = (dis-dmin)/(dmax-dmin); // scale from 0-1
	force->maxDistance = dis;
	force->forceMaxd = (posDelta/dis) * sc;
	//debug errMsg("calculateMaxdForce","pos"<<fluidpos<<" dis"<<dis<<" sc"<<sc<<" dmin"<<dmin<<" maxd"<< force->maxDistance <<" fmd"<<force->forceMaxd );
	return;
}


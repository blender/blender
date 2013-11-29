/** \file elbeem/intern/particletracer.cpp
 *  \ingroup elbeem
 */
/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003-2006 Nils Thuerey
 *
 * Particle Viewer/Tracer
 *
 *****************************************************************************/

#include <stdio.h>
//#include "../libs/my_gl.h"
//#include "../libs/my_glu.h"

/* own lib's */
#include "particletracer.h"
#include "ntl_matrices.h"
#include "ntl_ray.h"
#include "ntl_matrices.h"
#include "globals.h"

#include <zlib.h>


// particle object id counter
int ParticleObjectIdCnt = 1;

/******************************************************************************
 * Standard constructor
 *****************************************************************************/
ParticleTracer::ParticleTracer() :
	ntlGeometryObject(),
	mParts(),
	//mTrailLength(1), mTrailInterval(1),mTrailIntervalCounter(0),
	mPartSize(0.01),
	mStart(-1.0), mEnd(1.0),
	mSimStart(-1.0), mSimEnd(1.0),
	mPartScale(0.1) , mPartHeadDist( 0.1 ), mPartTailDist( -0.1 ), mPartSegments( 4 ),
	mValueScale(0),
	mValueCutoffTop(0.0), mValueCutoffBottom(0.0),
	mDumpParts(0), //mDumpText(0), 
	mDumpTextFile(""), 
	mDumpTextInterval(0.), mDumpTextLastTime(0.), mDumpTextCount(0),
	mShowOnly(0), 
	mNumInitialParts(0), mpTrafo(NULL),
	mInitStart(-1.), mInitEnd(-1.),
	mPrevs(), mTrailTimeLast(0.), mTrailInterval(-1.), mTrailLength(0)
{
	debMsgStd("ParticleTracer::ParticleTracer",DM_MSG,"inited",10);
};

ParticleTracer::~ParticleTracer() {
	debMsgStd("ParticleTracer::~ParticleTracer",DM_MSG,"destroyed",10);
	if(mpTrafo) delete mpTrafo;
}

/*****************************************************************************/
//! parse settings from attributes (dont use own list!)
/*****************************************************************************/
void ParticleTracer::parseAttrList(AttributeList *att) 
{
	AttributeList *tempAtt = mpAttrs; 
	mpAttrs = att;

	mNumInitialParts = mpAttrs->readInt("particles",mNumInitialParts, "ParticleTracer","mNumInitialParts", false);
	//errMsg(" NUMP"," "<<mNumInitialParts);
	mPartScale    = mpAttrs->readFloat("part_scale",mPartScale, "ParticleTracer","mPartScale", false);
	mPartHeadDist = mpAttrs->readFloat("part_headdist",mPartHeadDist, "ParticleTracer","mPartHeadDist", false);
	mPartTailDist = mpAttrs->readFloat("part_taildist",mPartTailDist, "ParticleTracer","mPartTailDist", false);
	mPartSegments = mpAttrs->readInt  ("part_segments",mPartSegments, "ParticleTracer","mPartSegments", false);
	mValueScale   = mpAttrs->readInt  ("part_valscale",mValueScale, "ParticleTracer","mValueScale", false);
	mValueCutoffTop = mpAttrs->readFloat("part_valcutofftop",mValueCutoffTop, "ParticleTracer","mValueCutoffTop", false);
	mValueCutoffBottom = mpAttrs->readFloat("part_valcutoffbottom",mValueCutoffBottom, "ParticleTracer","mValueCutoffBottom", false);

	mDumpParts   = mpAttrs->readInt  ("part_dump",mDumpParts, "ParticleTracer","mDumpParts", false);
	// mDumpText deprecatd, use mDumpTextInterval>0. instead
	mShowOnly    = mpAttrs->readInt  ("part_showonly",mShowOnly, "ParticleTracer","mShowOnly", false);
	mDumpTextFile= mpAttrs->readString("part_textdumpfile",mDumpTextFile, "ParticleTracer","mDumpTextFile", false);
	mDumpTextInterval= mpAttrs->readFloat("part_textdumpinterval",mDumpTextInterval, "ParticleTracer","mDumpTextInterval", false);

	string matPart;
	matPart = mpAttrs->readString("material_part", "default", "ParticleTracer","material", false);
	setMaterialName( matPart );

	mInitStart = mpAttrs->readFloat("part_initstart",mInitStart, "ParticleTracer","mInitStart", false);
	mInitEnd   = mpAttrs->readFloat("part_initend",  mInitEnd, "ParticleTracer","mInitEnd", false);

	// unused...
	//int mTrailLength  = 0; // UNUSED
	//int mTrailInterval= 0; // UNUSED
	mTrailLength  = mpAttrs->readInt("traillength",mTrailLength, "ParticleTracer","mTrailLength", false);
	mTrailInterval= mpAttrs->readFloat("trailinterval",mTrailInterval, "ParticleTracer","mTrailInterval", false);

	// restore old list
	mpAttrs = tempAtt;
}

/******************************************************************************
 * draw the particle array
 *****************************************************************************/
void ParticleTracer::draw()
{
}

/******************************************************************************
 * init trafo matrix
 *****************************************************************************/
void ParticleTracer::initTrafoMatrix() {
	ntlVec3Gfx scale = ntlVec3Gfx(
			(mEnd[0]-mStart[0])/(mSimEnd[0]-mSimStart[0]),
			(mEnd[1]-mStart[1])/(mSimEnd[1]-mSimStart[1]),
			(mEnd[2]-mStart[2])/(mSimEnd[2]-mSimStart[2])
			);
	ntlVec3Gfx trans = mStart;
	if(!mpTrafo) mpTrafo = new ntlMat4Gfx(0.0);
	mpTrafo->initId();
	for(int i=0; i<3; i++) { mpTrafo->value[i][i] = scale[i]; }
	for(int i=0; i<3; i++) { mpTrafo->value[i][3] = trans[i]; }
}

/******************************************************************************
 * adapt time step by rescaling velocities
 *****************************************************************************/
void ParticleTracer::adaptPartTimestep(float factor) {
	for(size_t i=0; i<mParts.size(); i++) {
		mParts[i].setVel( mParts[i].getVel() * factor );
	}
} 


/******************************************************************************
 * add a particle at this position
 *****************************************************************************/
void ParticleTracer::addParticle(float x, float y, float z) {
	ntlVec3Gfx p(x,y,z);
	ParticleObject part( p );
	mParts.push_back( part );
}
void ParticleTracer::addFullParticle(ParticleObject &np) {
	mParts.push_back( np );
}


void ParticleTracer::cleanup() {
	// cleanup
	int last = (int)mParts.size()-1;
	if(mDumpTextInterval>0.) { errMsg("ParticleTracer::cleanup","Skipping cleanup due to text dump..."); return; }

	for(int i=0; i<=last; i++) {
		if( mParts[i].getActive()==false ) {
			ParticleObject *p = &mParts[i];
			ParticleObject *p2 = &mParts[last];
			*p = *p2; last--; mParts.pop_back();
		}
	}
}
		
/******************************************************************************
 *! dump particles if desired 
 *****************************************************************************/
void ParticleTracer::notifyOfDump(int dumptype, int frameNr,char *frameNrStr,string outfilename, double simtime) {
	debMsgStd("ParticleTracer::notifyOfDump",DM_MSG,"obj:"<<this->getName()<<" frame:"<<frameNrStr<<" dumpp"<<mDumpParts<<" t"<<simtime, 10); // DEBUG

	if(
			(dumptype==DUMP_FULLGEOMETRY)&&
			(mDumpParts>0)) {
		// dump to binary file
		std::ostringstream boutfilename("");
		boutfilename << outfilename <<"_particles_" << frameNrStr;
		if(glob_mpactive) {
			if(glob_mpindex>0) { boutfilename << "mp"<<glob_mpindex; }
		}
		boutfilename << ".gz";
		debMsgStd("ParticleTracer::notifyOfDump",DM_MSG,"B-Dumping: "<< this->getName() <<", particles:"<<mParts.size()<<" "<< " to "<<boutfilename.str()<<" #"<<frameNr , 7);
		//debMsgStd("ParticleTracer::notifyOfDump",DM_MSG,"B-Dumping: partgeodeb sim:"<<mSimStart<<","<<mSimEnd<<" geosize:"<<mStart<<","<<mEnd,2 );

		// output to zipped file
		gzFile gzf;
		gzf = gzopen(boutfilename.str().c_str(), "wb1");
		if(gzf) {
			int numParts;
			if(sizeof(numParts)!=4) { errMsg("ParticleTracer::notifyOfDump","Invalid int size"); return; }
			// only dump active particles
			numParts = 0;
			for(size_t i=0; i<mParts.size(); i++) {
				if(!mParts[i].getActive()) continue;
				numParts++;
			}
			gzwrite(gzf, &numParts, sizeof(numParts));
			for(size_t i=0; i<mParts.size(); i++) {
				if(!mParts[i].getActive()) { continue; }
				ParticleObject *p = &mParts[i];
				//int type = p->getType();  // export whole type info
				int type = p->getFlags(); // debug export whole type & status info
				ntlVec3Gfx pos = p->getPos();
				float size = p->getSize();

				if(type&PART_FLOAT) { // WARNING same handling for dump!
					// add one gridcell offset
					//pos[2] += 1.0; 
				} 
				// display as drop for now externally
				//else if(type&PART_TRACER) { type |= PART_DROP; }

				pos = (*mpTrafo) * pos;

				ntlVec3Gfx v = p->getVel();
				v[0] *= mpTrafo->value[0][0];
				v[1] *= mpTrafo->value[1][1];
				v[2] *= mpTrafo->value[2][2];
				// FIXME check: pos = (*mpTrafo) * pos;
				gzwrite(gzf, &type, sizeof(type)); 
				gzwrite(gzf, &size, sizeof(size)); 
				for(int j=0; j<3; j++) { gzwrite(gzf, &pos[j], sizeof(float)); }
				for(int j=0; j<3; j++) { gzwrite(gzf, &v[j], sizeof(float)); }
			}
			gzclose( gzf );
		}
	} // dump?
}

void ParticleTracer::checkDumpTextPositions(double simtime) {
	// dfor partial & full dump
	if(mDumpTextInterval>0.) {
		debMsgStd("ParticleTracer::checkDumpTextPositions",DM_MSG,"t="<<simtime<<" last:"<<mDumpTextLastTime<<" inter:"<<mDumpTextInterval,7);
	}

	if((mDumpTextInterval>0.) && (simtime>mDumpTextLastTime+mDumpTextInterval)) {
		// dump to binary file
		std::ostringstream boutfilename("");
		if(mDumpTextFile.length()>1) {   
			boutfilename << mDumpTextFile <<  ".cpart2"; 
		} else {                           
			boutfilename << "_particles" <<  ".cpart2"; 
		}
		debMsgStd("ParticleTracer::checkDumpTextPositions",DM_MSG,"T-Dumping: "<< this->getName() <<", particles:"<<mParts.size()<<" "<< " to "<<boutfilename.str()<<" " , 7);

		int numParts = 0;
		// only dump bubble particles
		for(size_t i=0; i<mParts.size(); i++) {
			//if(!mParts[i].getActive()) continue;
			//if(!(mParts[i].getType()&PART_BUBBLE)) continue;
			numParts++;
		}

		// output to text file
		//gzFile gzf;
		FILE *stf;
		if(mDumpTextCount==0) {
			//gzf = gzopen(boutfilename.str().c_str(), "w0");
			stf = fopen(boutfilename.str().c_str(), "w");

			fprintf( stf, "\n\n# cparts generated by elbeem \n# no. of parts \nN %d \n\n",numParts);
			// fixed time scale for now
			fprintf( stf, "T %f \n\n", 1.0);
		} else {
			//gzf = gzopen(boutfilename.str().c_str(), "a+0");
			stf = fopen(boutfilename.str().c_str(), "a+");
		}

		// add current set
		if(stf) {
			fprintf( stf, "\n\n# new set at frame %d,t%f,p%d --------------------------------- \n\n", mDumpTextCount, simtime, numParts );
			fprintf( stf, "S %f \n\n", simtime );
			
			for(size_t i=0; i<mParts.size(); i++) {
				ParticleObject *p = &mParts[i];
				ntlVec3Gfx pos = p->getPos();
				float size = p->getSize();
				float infl = 1.;
				//if(!mParts[i].getActive()) { size=0.; } // switch "off"
				if(!mParts[i].getActive()) { infl=0.; } // switch "off"
				if(!mParts[i].getInFluid()) { infl=0.; } // switch "off"
				if(mParts[i].getLifeTime()<0.) { infl=0.; } // not yet active...

				pos = (*mpTrafo) * pos;
				ntlVec3Gfx v = p->getVel();
				v[0] *= mpTrafo->value[0][0];
				v[1] *= mpTrafo->value[1][1];
				v[2] *= mpTrafo->value[2][2];
				
				fprintf( stf, "P %f %f %f \n", pos[0],pos[1],pos[2] );
				if(size!=1.0) fprintf( stf, "s %f \n", size );
				if(infl!=1.0) fprintf( stf, "i %f \n", infl );
				fprintf( stf, "\n" );
			}

			fprintf( stf, "# %d end  ", mDumpTextCount );
			//gzclose( gzf );
			fclose( stf );

			mDumpTextCount++;
		}

		mDumpTextLastTime += mDumpTextInterval;
	}

}


void ParticleTracer::checkTrails(double time) {
	if(mTrailLength<1) return;
	if(time-mTrailTimeLast > mTrailInterval) {

		if( (int)mPrevs.size() < mTrailLength) mPrevs.resize( mTrailLength );
		for(int i=mPrevs.size()-1; i>0; i--) {
			mPrevs[i] = mPrevs[i-1];
			//errMsg("TRAIL"," from "<<i<<" to "<<(i-1) );
		}
		mPrevs[0] = mParts;

		mTrailTimeLast += mTrailInterval;
	}
}


/******************************************************************************
 * Get triangles for rendering
 *****************************************************************************/
void ParticleTracer::getTriangles(double time, vector<ntlTriangle> *triangles, 
													 vector<ntlVec3Gfx> *vertices, 
													 vector<ntlVec3Gfx> *normals, int objectId )
{
#ifdef ELBEEM_PLUGIN
	// suppress warnings...
	vertices = NULL; triangles = NULL;
	normals = NULL; objectId = 0;
	time = 0.;
#else // ELBEEM_PLUGIN
	int pcnt = 0;
	// currently not used in blender
	objectId = 0; // remove, deprecated
	if(mDumpParts>1) { 
		return; // only dump, no tri-gen
	}

	const bool debugParts = false;
	int tris = 0;
	int segments = mPartSegments;
	ntlVec3Gfx scale = ntlVec3Gfx( (mEnd[0]-mStart[0])/(mSimEnd[0]-mSimStart[0]), (mEnd[1]-mStart[1])/(mSimEnd[1]-mSimStart[1]), (mEnd[2]-mStart[2])/(mSimEnd[2]-mSimStart[2]));
	ntlVec3Gfx trans = mStart;
	time = 0.; // doesnt matter

	for(size_t t=0; t<mPrevs.size()+1; t++) {
		vector<ParticleObject> *dparts;
		if(t==0) {
			dparts = &mParts;
		} else {
			dparts = &mPrevs[t-1];
		}
		//errMsg("TRAILT","prevs"<<t<<"/"<<mPrevs.size()<<" parts:"<<dparts->size() );

	gfxReal partscale = mPartScale;
	if(t>1) { 
		partscale *= (gfxReal)(mPrevs.size()+1-t) / (gfxReal)(mPrevs.size()+1); 
	}
	gfxReal partNormSize = 0.01 * partscale;
	//for(size_t i=0; i<mParts.size(); i++) {
	for(size_t i=0; i<dparts->size(); i++) {
		ParticleObject *p = &( (*dparts)[i] ); //  mParts[i];

		if(mShowOnly!=10) {
			// 10=show only deleted
			if( p->getActive()==false ) continue;
		} else {
			if( p->getActive()==true ) continue;
		}
		int type = p->getType();
		if(mShowOnly>0) {
			switch(mShowOnly) {
			case 1: if(!(type&PART_BUBBLE)) continue; break;
			case 2: if(!(type&PART_DROP))   continue; break;
			case 3: if(!(type&PART_INTER))  continue; break;
			case 4: if(!(type&PART_FLOAT))  continue; break;
			case 5: if(!(type&PART_TRACER))  continue; break;
			}
		} else {
			// by default dont display inter
			if(type&PART_INTER) continue;
		}

		pcnt++;
		ntlVec3Gfx pnew = p->getPos();
		if(type&PART_FLOAT) { // WARNING same handling for dump!
			if(p->getStatus()&PART_IN) { pnew[2] += 0.8; } // offset for display
			// add one gridcell offset
			//pnew[2] += 1.0; 
		}
#if LBMDIM==2
		pnew[2] += 0.001; // DEBUG
		pnew[2] += 0.009; // DEBUG
#endif 

		ntlVec3Gfx pdir = p->getVel();
		gfxReal plen = normalize( pdir );
		if( plen < 1e-05) pdir = ntlVec3Gfx(-1.0 ,0.0 ,0.0);
		ntlVec3Gfx pos = (*mpTrafo) * pnew;
		gfxReal partsize = 0.0;
		if(debugParts) errMsg("DebugParts"," i"<<i<<" new"<<pnew<<" vel"<<pdir<<"   pos="<<pos );
		//if(i==0 &&(debugParts)) errMsg("DebugParts"," i"<<i<<" new"<<pnew[0]<<" pos="<<pos[0]<<" scale="<<scale[0]<<"  t="<<trans[0] );
		
		// value length scaling?
		if(mValueScale==1) {
			partsize = partscale * plen;
		} else if(mValueScale==2) {
			// cut off scaling
			if(plen > mValueCutoffTop) continue;
			if(plen < mValueCutoffBottom) continue;
			partsize = partscale * plen;
		} else {
			partsize = partscale; // no length scaling
		}
		//if(type&(PART_DROP|PART_BUBBLE)) 
		partsize *= p->getSize()/5.0;

		ntlVec3Gfx pstart( mPartHeadDist *partsize, 0.0, 0.0 );
		ntlVec3Gfx pend  ( mPartTailDist *partsize, 0.0, 0.0 );
		gfxReal phi = 0.0;
		gfxReal phiD = 2.0*M_PI / (gfxReal)segments;

		ntlMat4Gfx cvmat; 
		cvmat.initId();
		pdir *= -1.0;
		ntlVec3Gfx cv1 = pdir;
		ntlVec3Gfx cv2 = ntlVec3Gfx(pdir[1], -pdir[0], 0.0);
		ntlVec3Gfx cv3 = cross( cv1, cv2);
		//? for(int l=0; l<3; l++) { cvmat.value[l][0] = cv1[l]; cvmat.value[l][1] = cv2[l]; cvmat.value[l][2] = cv3[l]; }
		pstart = (cvmat * pstart);
		pend = (cvmat * pend);

		for(int s=0; s<segments; s++) {
			ntlVec3Gfx p1( 0.0 );
			ntlVec3Gfx p2( 0.0 );

			gfxReal radscale = partNormSize;
			radscale = (partsize+partNormSize)*0.5;
			p1[1] += cos(phi) * radscale;
			p1[2] += sin(phi) * radscale;
			p2[1] += cos(phi + phiD) * radscale;
			p2[2] += sin(phi + phiD) * radscale;
			ntlVec3Gfx n1 = ntlVec3Gfx( 0.0, cos(phi), sin(phi) );
			ntlVec3Gfx n2 = ntlVec3Gfx( 0.0, cos(phi + phiD), sin(phi + phiD) );
			ntlVec3Gfx ns = n1*0.5 + n2*0.5;

			p1 = (cvmat * p1);
			p2 = (cvmat * p2);

			sceneAddTriangle( pos+pstart, pos+p1, pos+p2, 
					ns,n1,n2, ntlVec3Gfx(0.0), 1, triangles,vertices,normals ); 
			sceneAddTriangle( pos+pend  , pos+p2, pos+p1, 
					ns,n2,n1, ntlVec3Gfx(0.0), 1, triangles,vertices,normals ); 

			phi += phiD;
			tris += 2;
		}
	}

	} // t

	debMsgStd("ParticleTracer::getTriangles",DM_MSG,"Dumped "<<pcnt<<"/"<<mParts.size()<<" parts, tris:"<<tris<<", showonly:"<<mShowOnly,10);
	return; // DEBUG

#endif // ELBEEM_PLUGIN
}





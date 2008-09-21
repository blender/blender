/******************************************************************************
 *
// El'Beem - the visual lattice boltzmann freesurface simulator
// All code distributed as part of El'Beem is covered by the version 2 of the 
// GNU General Public License. See the file COPYING for details.  
//
// Copyright 2008 Nils Thuerey , Richard Keiser, Mark Pauly, Ulrich Ruede
//
 *
 * Mean Value Mesh Coords class
 *
 *****************************************************************************/

#include "mvmcoords.h"
#include <algorithm>
using std::vector;

void MeanValueMeshCoords::clear() 
{
	mVertices.resize(0);
	mNumVerts = 0;
}

void MeanValueMeshCoords::calculateMVMCs(vector<ntlVec3Gfx> &reference_vertices, vector<ntlTriangle> &tris, 
		vector<ntlVec3Gfx> &points, gfxReal numweights)
{
	clear();
	mvmTransferPoint tds;
	int mem = 0;
	int i = 0;

	mNumVerts = (int)reference_vertices.size();

	for (vector<ntlVec3Gfx>::iterator iter = points.begin(); iter != points.end(); ++iter, ++i) {
		/*
		if(i%(points.size()/10)==1) debMsgStd("MeanValueMeshCoords::calculateMVMCs",DM_MSG,"Computing weights, points: "<<i<<"/"<<points.size(),5 );
		*/
		tds.lastpos = *iter;
		tds.weights.resize(0);	// clear
		computeWeights(reference_vertices, tris, tds, numweights);
		mem += (int)tds.weights.size();
		mVertices.push_back(tds);
	}    
	int mbmem = mem * sizeof(mvmFloat) / (1024*1024);
	debMsgStd("MeanValueMeshCoords::calculateMVMCs",DM_MSG,"vertices:"<<mNumVerts<<" points:"<<points.size()<<" weights:"<<mem<<", wmem:"<<mbmem<<"MB ",7 );
}

// from: mean value coordinates for closed triangular meshes
// attention: fails if a point is exactly (or very close) to a vertex
void MeanValueMeshCoords::computeWeights(vector<ntlVec3Gfx> &reference_vertices, vector<ntlTriangle>& tris,
		mvmTransferPoint& tds, gfxReal numweights)
{
	const bool mvmFullDebug=false;
	//const ntlVec3Gfx cEPS = 1.0e-6;
	const mvmFloat cEPS = 1.0e-14;

	//mvmFloat d[3], s[3], phi[3],c[3];
	ntlVec3d u[3],c,d,s,phi;
	int indices[3];

	for (int i = 0; i < (int)reference_vertices.size(); ++i) {
		tds.weights.push_back(mvmIndexWeight(i, 0.0));
	}

	// for each triangle
	//for (vector<ntlTriangle>::iterator iter = tris.begin(); iter != tris.end();) {
	for(int t=0; t<(int)tris.size(); t++) {

		for (int i = 0; i < 3; ++i) { //, ++iter) {
			indices[i] = tris[t].getPoints()[i];
			u[i] = vec2D(reference_vertices[ indices[i] ]-tds.lastpos);
			d[i] = normalize(u[i]); //.normalize();        
			//assert(d[i] != 0.);
			if(mvmFullDebug) errMsg("MeanValueMeshCoords::computeWeights","t"<<t<<" i"<<indices[i] //<<" lp"<<tds.lastpos
					<<" v"<<reference_vertices[indices[i]]<<" u"<<u[i]<<" ");
			// on vertex!
			//? if(d[i]<=0.) continue;
		}
		//for (int i = 0; i < 3; ++i) { errMsg("III"," "<<i		<<" i"<<indices[i]<<reference_vertices[ indices[i] ] ); }

		// arcsin is not needed, see paper
		phi[0] = 2.*asin( (mvmFloat)(0.5* norm(u[1]-u[2]) )  );
		phi[1] = 2.*asin( (mvmFloat)(0.5* norm(u[0]-u[2]) )  );
		phi[2] = 2.*asin( (mvmFloat)(0.5* norm(u[0]-u[1]) )  );
		mvmFloat h = (phi[0] + phi[1] + phi[2])*0.5;
		if (M_PI-h < cEPS) {
			if(mvmFullDebug) errMsg("MeanValueMeshCoords::computeWeights","point on triangle");
			tds.weights.resize(0);
			tds.weights.push_back( mvmIndexWeight(indices[0], sin(phi[0])*d[1]*d[2]));
			tds.weights.push_back( mvmIndexWeight(indices[1], sin(phi[1])*d[0]*d[2]));
			tds.weights.push_back( mvmIndexWeight(indices[2], sin(phi[2])*d[1]*d[0]));
			break;
		}
		mvmFloat sinh = 2.*sin(h);
		c[0] = (sinh*sin(h-phi[0]))/(sin(phi[1])*sin(phi[2]))-1.;
		c[1] = (sinh*sin(h-phi[1]))/(sin(phi[0])*sin(phi[2]))-1.;    
		c[2] = (sinh*sin(h-phi[2]))/(sin(phi[0])*sin(phi[1]))-1.;   
		if(mvmFullDebug) errMsg("MeanValueMeshCoords::computeWeights","c="<<c<<" phi="<<phi<<" d="<<d);
		//if (c[0] > 1. || c[0] < 0. || c[1] > 1. || c[1] < 0. || c[2] > 1. || c[2] < 0.) continue;

		s[0] = sqrtf((float)(1.-c[0]*c[0]));
		s[1] = sqrtf((float)(1.-c[1]*c[1]));
		s[2] = sqrtf((float)(1.-c[2]*c[2]));

		if(mvmFullDebug) errMsg("MeanValueMeshCoords::computeWeights","s");
		if (s[0] <= cEPS || s[1] <= cEPS || s[2] <= cEPS) {
			//MSG("position lies outside the triangle on the same plane -> ignore it");
			continue;
		}
		const mvmFloat u0x = u[0][0];
		const mvmFloat u0y = u[0][1];
		const mvmFloat u0z = u[0][2];
		const mvmFloat u1x = u[1][0];
		const mvmFloat u1y = u[1][1];
		const mvmFloat u1z = u[1][2];
		const mvmFloat u2x = u[2][0];
		const mvmFloat u2y = u[2][1];
		const mvmFloat u2z = u[2][2];
		mvmFloat det = u0x*u1y*u2z - u0x*u1z*u2y + u0y*u1z*u2x - u0y*u1x*u2z + u0z*u1x*u2y - u0z*u1y*u2x;
		//assert(det != 0.);
		if (det < 0.) {
			s[0] = -s[0];
			s[1] = -s[1];
			s[2] = -s[2];
		}

		tds.weights[indices[0]].weight += (phi[0]-c[1]*phi[2]-c[2]*phi[1])/(d[0]*sin(phi[1])*s[2]);
		tds.weights[indices[1]].weight += (phi[1]-c[2]*phi[0]-c[0]*phi[2])/(d[1]*sin(phi[2])*s[0]);
		tds.weights[indices[2]].weight += (phi[2]-c[0]*phi[1]-c[1]*phi[0])/(d[2]*sin(phi[0])*s[1]);
		if(mvmFullDebug) { errMsg("MeanValueMeshCoords::computeWeights","i"<<indices[0]<<" o"<<tds.weights[indices[0]].weight);
		errMsg("MeanValueMeshCoords::computeWeights","i"<<indices[1]<<" o"<<tds.weights[indices[1]].weight);
		errMsg("MeanValueMeshCoords::computeWeights","i"<<indices[2]<<" o"<<tds.weights[indices[2]].weight);
		errMsg("MeanValueMeshCoords::computeWeights","\n\n\n"); }
	}

	//sort weights
	if((numweights>0.)&& (numweights<1.) ) {
	//if( ((int)tds.weights.size() > maxNumWeights) && (maxNumWeights > 0) ) {
	  int maxNumWeights = (int)(tds.weights.size()*numweights);
		if(maxNumWeights<=0) maxNumWeights = 1;
		std::sort(tds.weights.begin(), tds.weights.end(), std::greater<mvmIndexWeight>());
		// only use maxNumWeights-th largest weights
		tds.weights.resize(maxNumWeights);
	}

	// normalize weights
	mvmFloat totalWeight = 0.;
	for (vector<mvmIndexWeight>::const_iterator witer = tds.weights.begin();
			witer != tds.weights.end(); ++witer) {
		totalWeight += witer->weight;
	}
	mvmFloat invTotalWeight;
	if (totalWeight == 0.) {
		if(mvmFullDebug) errMsg("MeanValueMeshCoords::computeWeights","totalWeight == 0");
		invTotalWeight = 0.0;
	} else {
		invTotalWeight = 1.0/totalWeight;
	}

	for (vector<mvmIndexWeight>::iterator viter = tds.weights.begin();
			viter != tds.weights.end(); ++viter) {
		viter->weight *= invTotalWeight;  
		//assert(finite(viter->weight) != 0);
		if(!finite(viter->weight)) viter->weight=0.;
	}
}

void MeanValueMeshCoords::transfer(vector<ntlVec3Gfx> &vertices, vector<ntlVec3Gfx>& displacements) 
{
	displacements.resize(0);

	//debMsgStd("MeanValueMeshCoords::transfer",DM_MSG,"vertices:"<<mNumVerts<<" curr_verts:"<<vertices.size()<<" ",7 );
	if((int)vertices.size() != mNumVerts) {
		errMsg("MeanValueMeshCoords::transfer","Different no of verts: "<<vertices.size()<<" vs "<<mNumVerts);
		return;
	}

	for (vector<mvmTransferPoint>::iterator titer = mVertices.begin(); titer != mVertices.end(); ++titer) {
		mvmTransferPoint &tds = *titer;
		ntlVec3Gfx newpos(0.0);

		for (vector<mvmIndexWeight>::iterator witer = tds.weights.begin();
				witer != tds.weights.end(); ++witer) {
			newpos += vertices[witer->index] * witer->weight;
			//errMsg("transfer","np"<<newpos<<" v"<<vertices[witer->index]<<" w"<< witer->weight);
		}

		displacements.push_back(newpos);
		//displacements.push_back(newpos - tds.lastpos);
		//tds.lastpos = newpos;
	}
}


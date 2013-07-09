/** \file elbeem/intern/attributes.cpp
 *  \ingroup elbeem
 */
/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003-2006 Nils Thuerey
 *
 * DEPRECATED - replaced by elbeem API, only channels are still used
 *
 *****************************************************************************/

#include "attributes.h"
#include "ntl_matrices.h"
#include "elbeem.h"



/******************************************************************************
 * attribute conversion functions
 *****************************************************************************/

bool Attribute::initChannel(int elemSize) {
	elemSize=0; // remove warning
	return false;
}
string Attribute::getAsString(bool debug) {
	debug=false; // remove warning
	return string("");
}
int Attribute::getAsInt() {
	return 0;
}
bool Attribute::getAsBool() {
	return false;
}
double Attribute::getAsFloat() {
	return 0.;
}
ntlVec3d Attribute::getAsVec3d() {
	return ntlVec3d(0.);
}
void Attribute::getAsMat4Gfx(ntlMat4Gfx *mat) {
	mat=NULL; // remove warning
}
string Attribute::getCompleteString() {
	return string("");
}


/******************************************************************************
 * channel returns
 *****************************************************************************/

AnimChannel<double> Attribute::getChannelFloat() {
	return AnimChannel<double>();
}
AnimChannel<int> Attribute::getChannelInt() { 
	return AnimChannel<int>();
}
AnimChannel<ntlVec3d> Attribute::getChannelVec3d() { 
	return AnimChannel<ntlVec3d>();
}
AnimChannel<ntlSetVec3f> 
Attribute::getChannelSetVec3f() {
	return AnimChannel<ntlSetVec3f>();
}

/******************************************************************************
 * check if there were unknown params
 *****************************************************************************/
bool AttributeList::checkUnusedParams() {
	return false;
}
void AttributeList::setAllUsed() {
}

/******************************************************************************
 * Attribute list read functions
 *****************************************************************************/
int AttributeList::readInt(string name, int defaultValue, string source,string target, bool needed) {
	name=source=target=string(""); needed=false; // remove warning
	return defaultValue;
}
bool AttributeList::readBool(string name, bool defaultValue, string source,string target, bool needed) {
	name=source=target=string(""); needed=false; // remove warning
	return defaultValue;
}
double AttributeList::readFloat(string name, double defaultValue, string source,string target, bool needed) {
	name=source=target=string(""); needed=false; // remove warning
	return defaultValue;
}
string AttributeList::readString(string name, string defaultValue, string source,string target, bool needed) {
	name=source=target=string(""); needed=false; // remove warning
	return defaultValue;
}
ntlVec3d AttributeList::readVec3d(string name, ntlVec3d defaultValue, string source,string target, bool needed) {
	name=source=target=string(""); needed=false; // remove warning
	return defaultValue;
}

void AttributeList::readMat4Gfx(string name, ntlMat4Gfx defaultValue, string source,string target, bool needed, ntlMat4Gfx *mat) {
	*mat = defaultValue;
	name=source=target=string(""); needed=false; mat=NULL; // remove warning
}

// set that a parameter can be given, and will be ignored...
bool AttributeList::ignoreParameter(string name, string source) {
	name = source = ("");
	return false;
}
		
// read channels
AnimChannel<int> AttributeList::readChannelInt(string name, int defaultValue, string source, string target, bool needed) {
	name=source=target=string(""); needed=false; // remove warning
	return AnimChannel<int>(defaultValue);
}
AnimChannel<double> AttributeList::readChannelFloat(string name, double defaultValue, string source, string target, bool needed ) {
	name=source=target=string(""); needed=false; // remove warning
	return AnimChannel<double>(defaultValue);
}
AnimChannel<ntlVec3d> AttributeList::readChannelVec3d(string name, ntlVec3d defaultValue, string source, string target, bool needed ) {
	name=source=target=string(""); needed=false; // remove warning
	return AnimChannel<ntlVec3d>(defaultValue);
}
AnimChannel<ntlSetVec3f> AttributeList::readChannelSetVec3f(string name, ntlSetVec3f defaultValue, string source, string target, bool needed) {
	name=source=target=string(""); needed=false; // remove warning
	return AnimChannel<ntlSetVec3f>(defaultValue);
}
AnimChannel<float> AttributeList::readChannelSinglePrecFloat(string name, float defaultValue, string source, string target, bool needed ) {
	name=source=target=string(""); needed=false; // remove warning
	return AnimChannel<float>(defaultValue);
}
AnimChannel<ntlVec3f> AttributeList::readChannelVec3f(string name, ntlVec3f defaultValue, string source, string target, bool needed) {
	name=source=target=string(""); needed=false; // remove warning
	return AnimChannel<ntlVec3f>(defaultValue);
}

/******************************************************************************
 * destructor
 *****************************************************************************/
AttributeList::~AttributeList() { 
};


/******************************************************************************
 * debugging
 *****************************************************************************/

//! debug function, prints value 
void Attribute::print() {
}
		
//! debug function, prints all attribs 
void AttributeList::print() {
}


/******************************************************************************
 * import attributes from other attribute list
 *****************************************************************************/
void AttributeList::import(AttributeList *oal) {
	oal=NULL; // remove warning
}


/******************************************************************************
 * channel max finding
 *****************************************************************************/
ntlVec3f channelFindMaxVf (AnimChannel<ntlVec3f> channel) {
	ntlVec3f ret(0.0);
	float maxLen = 0.0;
	for(size_t i=0; i<channel.accessValues().size(); i++) {
		float nlen = normNoSqrt(channel.accessValues()[i]);
		if(nlen>maxLen) { ret=channel.accessValues()[i]; maxLen=nlen; }
	}
	return ret;
}
ntlVec3d channelFindMaxVd (AnimChannel<ntlVec3d> channel) {
	ntlVec3d ret(0.0);
	float maxLen = 0.0;
	for(size_t i=0; i<channel.accessValues().size(); i++) {
		float nlen = normNoSqrt(channel.accessValues()[i]);
		if(nlen>maxLen) { ret=channel.accessValues()[i]; maxLen=nlen; }
	}
	return ret;
}
int      channelFindMaxi  (AnimChannel<float   > channel) {
	int ret = 0;
	float maxLen = 0.0;
	for(size_t i=0; i<channel.accessValues().size(); i++) {
		float nlen = ABS(channel.accessValues()[i]);
		if(nlen>maxLen) { ret= (int)channel.accessValues()[i]; maxLen=nlen; }
	}
	return ret;
}
float    channelFindMaxf  (AnimChannel<float   > channel) {
	float ret = 0.0;
	float maxLen = 0.0;
	for(size_t i=0; i<channel.accessValues().size(); i++) {
		float nlen = ABS(channel.accessValues()[i]);
		if(nlen>maxLen) { ret=channel.accessValues()[i]; maxLen=nlen; }
	}
	return ret;
}
double   channelFindMaxd  (AnimChannel<double  > channel) {
	double ret = 0.0;
	float maxLen = 0.0;
	for(size_t i=0; i<channel.accessValues().size(); i++) {
		float nlen = ABS(channel.accessValues()[i]);
		if(nlen>maxLen) { ret=channel.accessValues()[i]; maxLen=nlen; }
	}
	return ret;
}

/******************************************************************************
 // unoptimized channel simplification functions, use elbeem.cpp functions
 // warning - currently only with single precision
 *****************************************************************************/

template<class SCALAR>
static bool channelSimplifyScalarT(AnimChannel<SCALAR> &channel) {
	int   size = channel.getSize();
	if(size<=1) return false;
	float *nchannel = new float[2*size];
	// convert to array
	for(size_t i=0; i<channel.accessValues().size(); i++) {
		nchannel[i*2 + 0] = (float)channel.accessValues()[i];
		nchannel[i*2 + 1] = (float)channel.accessTimes()[i];
	}
	bool ret = elbeemSimplifyChannelFloat(nchannel, &size);
	if(ret) {
		vector<SCALAR> vals;
		vector<double> times;
		for(int i=0; i<size; i++) {
			vals.push_back(  (SCALAR)(nchannel[i*2 + 0]) );
			times.push_back( (double)(nchannel[i*2 + 1]) );
		}
		channel = AnimChannel<SCALAR>(vals, times);
	}
	delete [] nchannel;
	return ret;
}
bool channelSimplifyi  (AnimChannel<int   > &channel) { return channelSimplifyScalarT<int>(channel); }
bool channelSimplifyf  (AnimChannel<float> &channel) { return channelSimplifyScalarT<float>(channel); }
bool channelSimplifyd  (AnimChannel<double  > &channel) { return channelSimplifyScalarT<double>(channel); }
template<class VEC>
static bool channelSimplifyVecT(AnimChannel<VEC> &channel) {
	int   size = channel.getSize();
	if(size<=1) return false;
	float *nchannel = new float[4*size];
	// convert to array
	for(size_t i=0; i<channel.accessValues().size(); i++) {
		nchannel[i*4 + 0] = (float)channel.accessValues()[i][0];
		nchannel[i*4 + 1] = (float)channel.accessValues()[i][1];
		nchannel[i*4 + 2] = (float)channel.accessValues()[i][2];
		nchannel[i*4 + 3] = (float)channel.accessTimes()[i];
	}
	bool ret = elbeemSimplifyChannelVec3(nchannel, &size);
	if(ret) {
		vector<VEC> vals;
		vector<double> times;
		for(int i=0; i<size; i++) {
			vals.push_back(  VEC(nchannel[i*4 + 0], nchannel[i*4 + 1], nchannel[i*4 + 2] ) );
			times.push_back( (double)(nchannel[i*4 + 3]) );
		}
		channel = AnimChannel<VEC>(vals, times);
	}
	delete [] nchannel;
	return ret;
}
bool channelSimplifyVf (AnimChannel<ntlVec3f> &channel) {
	return channelSimplifyVecT<ntlVec3f>(channel);
}
bool channelSimplifyVd (AnimChannel<ntlVec3d> &channel) {
	return channelSimplifyVecT<ntlVec3d>(channel);
}

//! debug function, prints channel as string
template<class Scalar>
string AnimChannel<Scalar>::printChannel() {
	std::ostringstream ostr;
	ostr << " CHANNEL #"<<  mValue.size() <<" = { ";
	for(size_t i=0;i<mValue.size();i++) {
		ostr <<"'"<< mValue[i]<<"' ";
		ostr << "@"<<mTimes[i]<<"; ";
	}
	ostr << " } ";
	return ostr.str();
} // */

// is now in header file: debugPrintChannel() 
// hack to force instantiation
void __forceAnimChannelInstantiation() {
	AnimChannel< float > tmp1;
	AnimChannel< double > tmp2;
	AnimChannel< string > tmp3;
	AnimChannel< ntlVector3Dim<float> > tmp4;
	AnimChannel< ntlVector3Dim<double> > tmp5;
	tmp1.debugPrintChannel();
	tmp2.debugPrintChannel();
	tmp3.debugPrintChannel();
	tmp4.debugPrintChannel();
	tmp5.debugPrintChannel();
}


ntlSetVec3f::ntlSetVec3f(double v ) {
	mVerts.clear();
	mVerts.push_back( ntlVec3f(v) );
}
const ntlSetVec3f& 
ntlSetVec3f::operator=(double v ) {
	mVerts.clear();
	mVerts.push_back( ntlVec3f(v) );
	return *this;
}

std::ostream& operator<<( std::ostream& os, const ntlSetVec3f& vs ) {
	os<< "{";
	for(int j=0;j<(int)vs.mVerts.size();j++)  os<<vs.mVerts[j];
	os<< "}";
  return os;
}

ntlSetVec3f& 
ntlSetVec3f::operator+=( double v )
{
	for(int j=0;j<(int)(mVerts.size()) ;j++) {
		mVerts[j] += v;
	}
	return *this;
}

ntlSetVec3f& 
ntlSetVec3f::operator+=( const ntlSetVec3f &v )
{
	for(int j=0;j<(int)MIN(mVerts.size(),v.mVerts.size()) ;j++) {
		mVerts[j] += v.mVerts[j];
	}
	return *this;
}

ntlSetVec3f& 
ntlSetVec3f::operator*=( double v )
{
	for(int j=0;j<(int)(mVerts.size()) ;j++) {
		mVerts[j] *= v;
	}
	return *this;
}

ntlSetVec3f& 
ntlSetVec3f::operator*=( const ntlSetVec3f &v )
{
	for(int j=0;j<(int)MIN(mVerts.size(),v.mVerts.size()) ;j++) {
		mVerts[j] *= v.mVerts[j];
	}
	return *this;
}



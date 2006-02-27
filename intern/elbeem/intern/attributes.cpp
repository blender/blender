/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * configuration attribute storage class and attribute class
 *
 *****************************************************************************/

#include "attributes.h"
#include "ntl_matrices.h"
#include "elbeem.h"
#include <sstream>


//! output attribute values? on=1/off=0
#define DEBUG_ATTRIBUTES 0

//! output channel values? on=1/off=0
#define DEBUG_CHANNELS 0


/******************************************************************************
 * attribute conversion functions
 *****************************************************************************/

bool Attribute::initChannel(int elemSize) {
	if(!mIsChannel) return true;
	if(mChannelInited==elemSize) {
		// already inited... ok
		return true;
	} else {
		// sanity check
		if(mChannelInited>0) {
			errMsg("Attribute::initChannel","Duplicate channel init!? ("<<mChannelInited<<" vs "<<elemSize<<")...");
			return false;
		}
	}
	
	if((mValue.size() % (elemSize+1)) !=  0) {
		errMsg("Attribute::initChannel","Invalid # elements in Attribute...");
		return false;
	}
	
	int numElems = mValue.size()/(elemSize+1);
	vector<string> newvalue;
	for(int i=0; i<numElems; i++) {
	//errMsg("ATTR"," i"<<i<<" "<<mName); // debug

		vector<string> elem(elemSize);
		for(int j=0; j<elemSize; j++) {
		//errMsg("ATTR"," j"<<j<<" "<<mValue[i*(elemSize+1)+j]  ); // debug
			elem[j] = mValue[i*(elemSize+1)+j];
		}
		mChannel.push_back(elem);
		// use first value as default
		if(i==0) newvalue = elem;
		
		double t = 0.0; // similar to getAsFloat
		const char *str = mValue[i*(elemSize+1)+elemSize].c_str();
		char *endptr;
		t = strtod(str, &endptr);
		if((str!=endptr) && (*endptr != '\0')) return false;
		mTimes.push_back(t);
		//errMsg("ATTR"," t"<<t<<" "); // debug
	}
	for(int i=0; i<numElems-1; i++) {
		if(mTimes[i]>mTimes[i+1]) {
			errMsg("Attribute::initChannel","Invalid time at entry "<<i<<" setting to "<<mTimes[i]);
			mTimes[i+1] = mTimes[i];
		}
	}

	// dont change until done with parsing, and everythings ok
	mValue = newvalue;

	mChannelInited = elemSize;
	if(DEBUG_CHANNELS) print();
	return true;
}

// get value as string 
string Attribute::getAsString(bool debug)
{
	if(mIsChannel && (!debug)) {
		errMsg("Attribute::getAsString", "Attribute \"" << mName << "\" used as string is a channel! Not allowed...");
		print();
		return string("");
	}
	if(mValue.size()!=1) {
		// for directories etc. , this might be valid! cutoff "..." first
		string comp = getCompleteString();
		if(comp.size()<2) return string("");
		return comp.substr(1, comp.size()-2);
	}
	return mValue[0];
}

// get value as integer value
int Attribute::getAsInt()
{
	bool success = true;
	int ret = 0;

	if(!initChannel(1)) success=false; 
	if(success) {
		if(mValue.size()!=1) success = false;
		else {
			const char *str = mValue[0].c_str();
			char *endptr;
			ret = strtol(str, &endptr, 10);
			if( (str==endptr) ||
					((str!=endptr) && (*endptr != '\0')) )success = false;
		}
	}

	if(!success) {
		errMsg("Attribute::getAsInt", "Attribute \"" << mName << "\" used as int has invalid value '"<< getCompleteString() <<"' ");
		errMsg("Attribute::getAsInt", "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" );
		errMsg("Attribute::getAsInt", "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" );
#if ELBEEM_PLUGIN!=1
		gElbeemState = -4; // parse error
#endif
		return 0;
	}
	return ret;
}


// get value as integer value
bool Attribute::getAsBool() 
{
	int val = getAsInt();
	if(val==0) return false;
	else 			 return true;
}


// get value as double value
double Attribute::getAsFloat()
{
	bool success = true;
	double ret = 0.0;

	if(!initChannel(1)) success=false; 
	if(success) {
		if(mValue.size()!=1) success = false;
		else {
			const char *str = mValue[0].c_str();
			char *endptr;
			ret = strtod(str, &endptr);
			if((str!=endptr) && (*endptr != '\0')) success = false;
		}
	}

	if(!success) {
		print();
		errMsg("Attribute::getAsFloat", "Attribute \"" << mName << "\" used as double has invalid value '"<< getCompleteString() <<"' ");
		errMsg("Attribute::getAsFloat", "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" );
		errMsg("Attribute::getAsFloat", "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" );
#if ELBEEM_PLUGIN!=1
		gElbeemState = -4; // parse error
#endif
		return 0.0;
	}
	return ret;
}

// get value as 3d vector 
ntlVec3d Attribute::getAsVec3d()
{
	bool success = true;
	ntlVec3d ret(0.0);

	if(!initChannel(3)) success=false; 
	if(success) {
		if(mValue.size()==1) {
			const char *str = mValue[0].c_str();
			char *endptr;
			double rval = strtod(str, &endptr);
			if( (str==endptr) ||
					((str!=endptr) && (*endptr != '\0')) )success = false;
			if(success) ret = ntlVec3d( rval );
		} else if(mValue.size()==3) {
			char *endptr;
			const char *str = NULL;

			str = mValue[0].c_str();
			double rval1 = strtod(str, &endptr);
			if( (str==endptr) ||
					((str!=endptr) && (*endptr != '\0')) )success = false;

			str = mValue[1].c_str();
			double rval2 = strtod(str, &endptr);
			if( (str==endptr) ||
					((str!=endptr) && (*endptr != '\0')) )success = false;

			str = mValue[2].c_str();
			double rval3 = strtod(str, &endptr);
			if( (str==endptr) ||
					((str!=endptr) && (*endptr != '\0')) )success = false;

			if(success) ret = ntlVec3d( rval1, rval2, rval3 );
		} else {
			success = false;
		}
	}

	if(!success) {
		errMsg("Attribute::getAsVec3d", "Attribute \"" << mName << "\" used as Vec3d has invalid value '"<< getCompleteString() <<"' ");
		errMsg("Attribute::getAsVec3d", "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" );
		errMsg("Attribute::getAsVec3d", "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" );
#if ELBEEM_PLUGIN!=1
		gElbeemState = -4; // parse error
#endif
		return ntlVec3d(0.0);
	}
	return ret;
}
		
// get value as 4x4 matrix 
void Attribute::getAsMat4Gfx(ntlMat4Gfx *mat)
{
	bool success = true;
	ntlMat4Gfx ret(0.0);
	char *endptr;

	if(mValue.size()==1) {
		const char *str = mValue[0].c_str();
		double rval = strtod(str, &endptr);
		if( (str==endptr) ||
				((str!=endptr) && (*endptr != '\0')) )success = false;
		if(success) {
			ret = ntlMat4Gfx( 0.0 );
			ret.value[0][0] = rval;
			ret.value[1][1] = rval;
			ret.value[2][2] = rval;
			ret.value[3][3] = 1.0;
		}
	} else if(mValue.size()==9) {
		// 3x3
		for(int i=0; i<3;i++) {
			for(int j=0; j<3;j++) {
				const char *str = mValue[i*3+j].c_str();
				ret.value[i][j] = strtod(str, &endptr);
				if( (str==endptr) ||
						((str!=endptr) && (*endptr != '\0')) ) success = false;
			}
		}
	} else if(mValue.size()==16) {
		// 4x4
		for(int i=0; i<4;i++) {
			for(int j=0; j<4;j++) {
				const char *str = mValue[i*4+j].c_str();
				ret.value[i][j] = strtod(str, &endptr);
				if( (str==endptr) ||
						((str!=endptr) && (*endptr != '\0')) ) success = false;
			}
		}

	} else {
		success = false;
	}

	if(!success) {
		errMsg("Attribute::getAsMat4Gfx", "Attribute \"" << mName << "\" used as Mat4x4 has invalid value '"<< getCompleteString() <<"' ");
		errMsg("Attribute::getAsMat4Gfx", "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" );
		errMsg("Attribute::getAsMat4Gfx", "!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!" );
#if ELBEEM_PLUGIN!=1
		gElbeemState = -4; // parse error
#endif
		*mat = ntlMat4Gfx(0.0);
		return;
	}
	*mat = ret;
}
		

// get the concatenated string of all value string
string Attribute::getCompleteString()
{
	string ret;
	for(size_t i=0;i<mValue.size();i++) {
		ret += mValue[i];
		if(i<mValue.size()-1) ret += " ";
	}
	return ret;
}


/******************************************************************************
 * channel returns
 *****************************************************************************/

//! get channel as double value
AnimChannel<double> Attribute::getChannelFloat() {
	vector<double> timeVec;
	vector<double> valVec;
	
	if((!initChannel(1)) || (!mIsChannel)) {
		timeVec.push_back( 0.0 );
		valVec.push_back( getAsFloat() );
	} else {
	for(size_t i=0; i<mChannel.size(); i++) {
		mValue = mChannel[i];
		double val = getAsFloat();
		timeVec.push_back( mTimes[i] );
		valVec.push_back( val );
	}}

	return AnimChannel<double>(valVec,timeVec);
}

//! get channel as integer value
AnimChannel<int> Attribute::getChannelInt() { 
	vector<double> timeVec;
	vector<int> valVec;
	
	if((!initChannel(1)) || (!mIsChannel)) {
		timeVec.push_back( 0.0 );
		valVec.push_back( getAsInt() );
	} else {
	for(size_t i=0; i<mChannel.size(); i++) {
		mValue = mChannel[i];
		int val = getAsInt();
		timeVec.push_back( mTimes[i] );
		valVec.push_back( val );
	}}

	return AnimChannel<int>(valVec,timeVec);
}

//! get channel as integer value
AnimChannel<ntlVec3d> Attribute::getChannelVec3d() { 
	vector<double> timeVec;
	vector<ntlVec3d> valVec;
	
	if((!initChannel(3)) || (!mIsChannel)) {
		timeVec.push_back( 0.0 );
		valVec.push_back( getAsVec3d() );
	} else {
	for(size_t i=0; i<mChannel.size(); i++) {
		mValue = mChannel[i];
		ntlVec3d val = getAsVec3d();
		timeVec.push_back( mTimes[i] );
		valVec.push_back( val );
	}}

	return AnimChannel<ntlVec3d>(valVec,timeVec);
}


/******************************************************************************
 * check if there were unknown params
 *****************************************************************************/
bool AttributeList::checkUnusedParams()
{
	bool found = false;
	for(map<string, Attribute*>::iterator i=mAttrs.begin();
			i != mAttrs.end(); i++) {
		if((*i).second) {
			if(!(*i).second->getUsed()) {
				errMsg("AttributeList::checkUnusedParams", "List "<<mName<<" has unknown parameter '"<<(*i).first<<"' = '"<< mAttrs[(*i).first]->getAsString(true) <<"' ");
				found = true;
			}
		}
	}
	return found;
}
//! set all params to used, for invisible objects
void AttributeList::setAllUsed() {
	for(map<string, Attribute*>::iterator i=mAttrs.begin();
			i != mAttrs.end(); i++) {
		if((*i).second) {
			(*i).second->setUsed(true);
		}
	}
}

/******************************************************************************
 * Attribute list read functions
 *****************************************************************************/
int AttributeList::readInt(string name, int defaultValue, string source,string target, bool needed) {
	if(!exists(name)) {
		if(needed) { errFatal("AttributeList::readInt","Required attribute '"<<name<<"' for "<< source <<"  not set! ", SIMWORLD_INITERROR); }
		return defaultValue;
	} 
	if(DEBUG_ATTRIBUTES==1) { debugOut( source << " Var '"<< target <<"' set to '"<< find(name)->getCompleteString() <<"' as type int " , 3); }
	find(name)->setUsed(true);
	return find(name)->getAsInt(); 
}
bool AttributeList::readBool(string name, bool defaultValue, string source,string target, bool needed) {
	if(!exists(name)) {
		if(needed) { errFatal("AttributeList::readBool","Required attribute '"<<name<<"' for "<< source <<"  not set! ", SIMWORLD_INITERROR); }
		return defaultValue;
	} 
	if(DEBUG_ATTRIBUTES==1) { debugOut( source << " Var '"<< target <<"' set to '"<< find(name)->getCompleteString() <<"' as type int " , 3); }
	find(name)->setUsed(true);
	return find(name)->getAsBool(); 
}
double AttributeList::readFloat(string name, double defaultValue, string source,string target, bool needed) {
	if(!exists(name)) {
		if(needed) { errFatal("AttributeList::readFloat","Required attribute '"<<name<<"' for "<< source <<"  not set! ", SIMWORLD_INITERROR); }
		return defaultValue;
	} 
	if(DEBUG_ATTRIBUTES==1) { debugOut( source << " Var '"<< target <<"' set to '"<< find(name)->getCompleteString() <<"' as type int " , 3); }
	find(name)->setUsed(true);
	return find(name)->getAsFloat(); 
}
string AttributeList::readString(string name, string defaultValue, string source,string target, bool needed) {
	if(!exists(name)) {
		if(needed) { errFatal("AttributeList::readInt","Required attribute '"<<name<<"' for "<< source <<"  not set! ", SIMWORLD_INITERROR); }
		return defaultValue;
	} 
	if(DEBUG_ATTRIBUTES==1) { debugOut( source << " Var '"<< target <<"' set to '"<< find(name)->getCompleteString() <<"' as type int " , 3); }
	find(name)->setUsed(true);
	return find(name)->getAsString(false); 
}
ntlVec3d AttributeList::readVec3d(string name, ntlVec3d defaultValue, string source,string target, bool needed) {
	if(!exists(name)) {
		if(needed) { errFatal("AttributeList::readInt","Required attribute '"<<name<<"' for "<< source <<"  not set! ", SIMWORLD_INITERROR); }
		return defaultValue;
	} 
	if(DEBUG_ATTRIBUTES==1) { debugOut( source << " Var '"<< target <<"' set to '"<< find(name)->getCompleteString() <<"' as type int " , 3); }
	find(name)->setUsed(true);
	return find(name)->getAsVec3d(); 
}

void AttributeList::readMat4Gfx(string name, ntlMat4Gfx defaultValue, string source,string target, bool needed, ntlMat4Gfx *mat) {
	if(!exists(name)) {
		if(needed) { errFatal("AttributeList::readInt","Required attribute '"<<name<<"' for "<< source <<"  not set! ", SIMWORLD_INITERROR); }
	 	*mat = defaultValue;
		return;
	} 
	if(DEBUG_ATTRIBUTES==1) { debugOut( source << " Var '"<< target <<"' set to '"<< find(name)->getCompleteString() <<"' as type int " , 3); }
	find(name)->setUsed(true);
	find(name)->getAsMat4Gfx( mat ); 
	return;
}

// set that a parameter can be given, and will be ignored...
bool AttributeList::ignoreParameter(string name, string source) {
	if(!exists(name)) return false;
	find(name)->setUsed(true);
	if(DEBUG_ATTRIBUTES==1) { debugOut( source << " Param '"<< name <<"' set but ignored... " , 3); }
	return true;
}
		
// read channels
AnimChannel<double> AttributeList::readChannelFloat(string name) {
	if(!exists(name)) { return AnimChannel<double>(0.0); } 
	AnimChannel<double> ret = find(name)->getChannelFloat(); 
	find(name)->setUsed(true);
	channelSimplifyd(ret);
	return ret;
}
AnimChannel<int> AttributeList::readChannelInt(string name) {
	if(!exists(name)) { return AnimChannel<int>(0); } 
	AnimChannel<int> ret = find(name)->getChannelInt(); 
	find(name)->setUsed(true);
	channelSimplifyi(ret);
	return ret;
}
AnimChannel<ntlVec3d> AttributeList::readChannelVec3d(string name) {
	if(!exists(name)) { return AnimChannel<ntlVec3d>(0.0); } 
	AnimChannel<ntlVec3d> ret = find(name)->getChannelVec3d(); 
	find(name)->setUsed(true);
	channelSimplifyVd(ret);
	return ret;
}
AnimChannel<ntlVec3f> AttributeList::readChannelVec3f(string name) {
	if(!exists(name)) { return AnimChannel<ntlVec3f>(0.0); } 

	AnimChannel<ntlVec3d> convert = find(name)->getChannelVec3d(); 
	// convert to float
	vector<ntlVec3f> vals;
	for(size_t i=0; i<convert.accessValues().size(); i++) {
		vals.push_back( vec2F(convert.accessValues()[i]) );
	}
	vector<double> times = convert.accessTimes();
	AnimChannel<ntlVec3f> ret(vals, times);
	find(name)->setUsed(true);
	channelSimplifyVf(ret);
	return ret;
}

/******************************************************************************
 * destructor
 *****************************************************************************/
AttributeList::~AttributeList() { 
	for(map<string, Attribute*>::iterator i=mAttrs.begin();
			i != mAttrs.end(); i++) {
		if((*i).second) {
			delete (*i).second;
			(*i).second = NULL;
		}
	}
};


/******************************************************************************
 * debugging
 *****************************************************************************/

//! debug function, prints value 
void Attribute::print()
{
	std::ostringstream ostr;
	ostr << "ATTR "<< mName <<"= ";
	for(size_t i=0;i<mValue.size();i++) {
		ostr <<"'"<< mValue[i]<<"' ";
	}
	if(mIsChannel) {
		ostr << " CHANNEL: ";
		if(mChannelInited>0) {
		for(size_t i=0;i<mChannel.size();i++) {
			for(size_t j=0;j<mChannel[i].size();j++) {
				ostr <<"'"<< mChannel[i][j]<<"' ";
			}
			ostr << "@"<<mTimes[i]<<"; ";
		}
		} else {
			ostr <<" -nyi- ";
		}			
	}
	ostr <<" (at line "<<mLine<<") "; //<< std::endl;
	debugOut( ostr.str(), 10);
}
		
//! debug function, prints all attribs 
void AttributeList::print()
{
	debugOut("Attribute "<<mName<<" values:", 10);
	for(map<string, Attribute*>::iterator i=mAttrs.begin();
			i != mAttrs.end(); i++) {
		if((*i).second) {
			(*i).second->print();
		}
	}
}


/******************************************************************************
 * import attributes from other attribute list
 *****************************************************************************/
void AttributeList::import(AttributeList *oal)
{
	for(map<string, Attribute*>::iterator i=oal->mAttrs.begin();
			i !=oal->mAttrs.end(); i++) {
		// FIXME - check freeing of copyied attributes
		if((*i).second) {
			Attribute *newAttr = new Attribute( *(*i).second );
			mAttrs[ (*i).first ] = newAttr;
		}
	}
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
	if(DEBUG_CHANNELS) errMsg("channelSimplifyf","S" << channel.printChannel() );
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
		if(DEBUG_CHANNELS) errMsg("channelSimplifyf","C" << channel.printChannel() );
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
	if(DEBUG_CHANNELS) errMsg("channelSimplifyf","S" << channel.printChannel() );
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
		if(DEBUG_CHANNELS) errMsg("channelSimplifyf","C" << channel.printChannel() );
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




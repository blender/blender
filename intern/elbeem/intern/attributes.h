/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003-2006 Nils Thuerey
 *
 * DEPRECATED - replaced by elbeem API, only channels are still used
 *
 *****************************************************************************/


#ifndef NTL_ATTRIBUTES_H

#include "utilities.h"
template<class T> class ntlMatrix4x4;
class ntlSetVec3f;
std::ostream& operator<<( std::ostream& os, const ntlSetVec3f& i );



//! An animated attribute channel
template<class Scalar>
class AnimChannel
{
	public:
		// default constructor
		AnimChannel() : 
			mValue(), mTimes() { mInited = false; debugPrintChannel(); }

		// null init constructor
		AnimChannel(Scalar null) : 
			mValue(1), mTimes(1) { mValue[0]=null; mTimes[0]=0.0; mInited = true; debugPrintChannel(); }

		// proper init
		AnimChannel(vector<Scalar> &v, vector<double> &t) : 
			mValue(v), mTimes(t) { mInited = true; debugPrintChannel(); }

		// desctructor, nothing to do
		~AnimChannel() { };

		// get interpolated value at time t
		Scalar get(double t) const {
			if(!mInited) { Scalar null; null=(Scalar)(0.0); return null; }
			if(t<=mTimes[0])               { return mValue[0]; }
			if(t>=mTimes[mTimes.size()-1]) { return mValue[mTimes.size()-1]; }
			for(size_t i=0; i<mTimes.size()-1; i++) {
				// find first time thats in between
				if((mTimes[i]<=t)&&(mTimes[i+1]>t)) { 
					// interpolate
					double d = mTimes[i+1]-mTimes[i];
					double f = (t-mTimes[i])/d;
					//return (Scalar)(mValue[i] * (1.0-f) + mValue[i+1] * f);
					Scalar ret,tmp;
					ret = mValue[i];
					ret *= 1.-f;
					tmp = mValue[i+1];
					tmp *= f;
					ret += tmp;
					return ret;
				}
			}
			// whats this...?
			return mValue[0];
		};

		// get uninterpolated value at time t
		Scalar getConstant(double t) const {
			//errMsg("DEBB","getc"<<t<<" ");
			if(!mInited) { Scalar null; null=(Scalar)0.0; return null; }
			if(t<=mTimes[0])               { return mValue[0]; }
			if(t>=mTimes[mTimes.size()-1]) { return mValue[mTimes.size()-1]; }
			for(size_t i=0; i<mTimes.size()-1; i++) {
				//errMsg("DEBB","getc i"<<i<<" "<<mTimes[i]);
				// find first time thats in between
				if((mTimes[i]<=t)&&(mTimes[i+1]>t)) { return mValue[i]; }
			}
			// whats this...?
			return mValue[0];
		};

		// reset to null value
		void reset(Scalar null) {
			mValue.clear();
			mTimes.clear();
			mValue.push_back(null);
			mTimes.push_back(0.0);
		}

		//! debug function, prints channel as string
		string printChannel();
		//! debug function, prints to stdout if DEBUG_CHANNELS flag is enabled, used in constructors
		void debugPrintChannel();
		//! valid init?
		bool isInited() const { return mInited; }

		//! get number of entries (value and time sizes have to be equal)
		int getSize() const { return mValue.size(); };
		//! raw access of value vector
		vector<Scalar> &accessValues() { return mValue; }
		//! raw access of time vector
		vector<double> &accessTimes() { return mTimes; }
		
	protected:

		/*! inited at least once? */
		bool mInited;
		/*! anim channel attribute values */
		vector<Scalar> mValue;
		/*! anim channel attr times */
		vector<double> mTimes;
};


// helper class (not templated) for animated meshes
class ntlSetVec3f {
	public:
		ntlSetVec3f(): mVerts() {};
		ntlSetVec3f(double v);
		ntlSetVec3f(vector<ntlVec3f> &v) { mVerts = v; };

		const ntlSetVec3f& operator=(double v );
		ntlSetVec3f& operator+=( double v );
		ntlSetVec3f& operator+=( const ntlSetVec3f &v );
		ntlSetVec3f& operator*=( double v );
		ntlSetVec3f& operator*=( const ntlSetVec3f &v );

		vector<ntlVec3f> mVerts;
};


// warning: DEPRECATED - replaced by elbeem API
class Attribute
{
	public:
  	Attribute(string mn, vector<string> &value, int setline,bool channel) { 
			mn = string(""); setline=0; channel=false; value.clear(); // remove warnings
		};
  	Attribute(Attribute &a) { a.getCompleteString(); };
  	~Attribute() { };

		void setUsed(bool set){ set=false; }
		bool getUsed() { return true; }
		void setIsChannel(bool set){ set=false;  }
		bool getIsChannel() { return false; }

		string getAsString(bool debug=false);
		int getAsInt();
		bool getAsBool();
		double getAsFloat();
		ntlVec3d getAsVec3d();
		void getAsMat4Gfx(ntlMatrix4x4<gfxReal> *mat);

		AnimChannel<int> getChannelInt();
		AnimChannel<double> getChannelFloat();
		AnimChannel<ntlVec3d> getChannelVec3d();
		AnimChannel<ntlSetVec3f> getChannelSetVec3f();

		string getCompleteString();
		void print();
		
	protected:

		bool initChannel(int elemSize);
};


// warning: DEPRECATED - replaced by elbeem API
//! The list of configuration attributes
class AttributeList
{
	public:
  	AttributeList(string name) { name=string(""); };
  	~AttributeList();
		void addAttr(string name, vector<string> &value, int line, bool isChannel) { 
			name=string(""); value.clear(); line=0; isChannel=false; // remove warnings
		};
		bool exists(string name) { name=string(""); return false; }
		void setAllUsed();
		bool checkUnusedParams();
		void import(AttributeList *oal);
		int      readInt(string name, int defaultValue,         string source,string target, bool needed);
		bool     readBool(string name, bool defaultValue,       string source,string target, bool needed);
		double   readFloat(string name, double defaultValue,   string source,string target, bool needed);
		string   readString(string name, string defaultValue,   string source,string target, bool needed);
		ntlVec3d readVec3d(string name, ntlVec3d defaultValue,  string source,string target, bool needed);
		void readMat4Gfx(string name, ntlMatrix4x4<gfxReal> defaultValue,  string source,string target, bool needed, ntlMatrix4x4<gfxReal> *mat);
		AnimChannel<int>     readChannelInt(         string name, int defaultValue=0, string source=string("src"), string target=string("dst"), bool needed=false );
		AnimChannel<double>  readChannelFloat(       string name, double defaultValue=0, string source=string("src"), string target=string("dst"), bool needed=false );
		AnimChannel<ntlVec3d> readChannelVec3d(      string name, ntlVec3d defaultValue=ntlVec3d(0.), string source=string("src"), string target=string("dst"), bool needed=false );
		AnimChannel<ntlSetVec3f> readChannelSetVec3f(string name, ntlSetVec3f defaultValue=ntlSetVec3f(0.), string source=string("src"), string target=string("dst"), bool needed=false );
		AnimChannel<ntlVec3f> readChannelVec3f(           string name, ntlVec3f defaultValue=ntlVec3f(0.), string source=string("src"), string target=string("dst"), bool needed=false );
		AnimChannel<float>    readChannelSinglePrecFloat( string name, float defaultValue=0., string source=string("src"), string target=string("dst"), bool needed=false );
		bool ignoreParameter(string name, string source);
		void print();
	protected:
};

ntlVec3f channelFindMaxVf (AnimChannel<ntlVec3f> channel);
ntlVec3d channelFindMaxVd (AnimChannel<ntlVec3d> channel);
int      channelFindMaxi  (AnimChannel<int     > channel);
float    channelFindMaxf  (AnimChannel<float   > channel);
double   channelFindMaxd  (AnimChannel<double  > channel);

// unoptimized channel simplification functions, use elbeem.cpp functions
bool channelSimplifyVf (AnimChannel<ntlVec3f> &channel);
bool channelSimplifyVd (AnimChannel<ntlVec3d> &channel);
bool channelSimplifyi  (AnimChannel<int     > &channel);
bool channelSimplifyf  (AnimChannel<float   > &channel);
bool channelSimplifyd  (AnimChannel<double  > &channel);

//! output channel values? on=1/off=0
#define DEBUG_PCHANNELS 0

//! debug function, prints to stdout if DEBUG_PCHANNELS flag is enabled, used in constructors
template<class Scalar>
void AnimChannel<Scalar>::debugPrintChannel() { }


#define NTL_ATTRIBUTES_H
#endif


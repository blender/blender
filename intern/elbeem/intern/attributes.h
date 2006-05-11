/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * configuration attribute storage class and attribute class
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
		Scalar get(double t) {
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
		Scalar getConstant(double t) {
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
		bool isInited() { return mInited; }

		//! get number of entries (value and time sizes have to be equal)
		int getSize() { return mValue.size(); };
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


//! A single attribute
class Attribute
{
	public:
  	//! Standard constructor
  	Attribute(string mn, vector<string> &value, int setline,bool channel) :
			mName(mn), mValue(value), 
			mLine(setline), mUsed(false), mIsChannel(channel),
	 		mChannelInited(-1)	{ };
  	//! Copy constructor
  	Attribute(Attribute &a) :
			mName(a.mName), mValue(a.mValue), 
			mLine(a.mLine), mUsed(false), mIsChannel(a.mIsChannel),
	 		mChannelInited(a.mChannelInited),
			mChannel(a.mChannel), mTimes(a.mTimes)	{ };
  	//! Destructor
  	~Attribute() { /* empty */ };

		//! set used flag
		void setUsed(bool set){ mUsed = set; }
		//! get used flag
		bool getUsed() { return mUsed; }

		//! set channel flag
		void setIsChannel(bool set){ mIsChannel = set; }
		//! get channel flag
		bool getIsChannel() { return mIsChannel; }

		//! get value as string 
		string getAsString(bool debug=false);
		//! get value as integer value
		int getAsInt();
		//! get value as boolean
		bool getAsBool();
		//! get value as double value
		double getAsFloat();
		//! get value as 3d vector 
		ntlVec3d getAsVec3d();
		//! get value as 4x4 matrix
		void getAsMat4Gfx(ntlMatrix4x4<gfxReal> *mat);

		//! get channel as integer value
		AnimChannel<int> getChannelInt();
		//! get channel as double value
		AnimChannel<double> getChannelFloat();
		//! get channel as double vector
		AnimChannel<ntlVec3d> getChannelVec3d();
		//! get channel as float vector set
		AnimChannel<ntlSetVec3f> getChannelSetVec3f();

		//! get the concatenated string of all value string
		string getCompleteString();

		//! debug function, prints value 
		void print();
		
	protected:

		/*! internal - init channel before access */
		bool initChannel(int elemSize);

		/*! the attr name */
		string mName;

		/*! the attr value */
		vector<string> mValue;

		/*! line where the value was defined in the config file (for error messages) */
		int mLine;

		/*! was this attribute used? */
		bool mUsed;

		/*! does this attribute have a channel? */
		bool mIsChannel;
		/*! does this attribute have a channel? */
		int mChannelInited;

		/*! channel attribute values (first one equals mValue) */
		vector< vector< string > > mChannel;
		/*! channel attr times */
		vector< double > mTimes;
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

//! The list of configuration attributes
class AttributeList
{
	public:
  	//! Standard constructor
  	AttributeList(string name) :
			mName(name), mAttrs() { };
  	//! Destructor , delete all contained attribs
  	~AttributeList();

		/*! add an attribute to this list */
		void addAttr(string name, vector<string> &value, int line, bool isChannel) {
			if(exists(name)) delete mAttrs[name];
			mAttrs[name] = new Attribute(name,value,line, isChannel);
		}

		/*! check if an attribute is set */
		bool exists(string name) {
			if(mAttrs.find(name) == mAttrs.end()) return false;
			return true;
		}

		/*! get an attribute */
		Attribute *find(string name) {
			if(mAttrs.find(name) == mAttrs.end()) { 
				errFatal("AttributeList::find","Invalid attribute '"<<name<<"' , not found...",SIMWORLD_INITERROR );
				// just create a new empty one (warning: small memory leak!), and exit as soon as possible
				vector<string> empty;
				return new Attribute(name,empty, -1, 0);
			}
			return mAttrs[name];
		}

		//! set all params to used, for invisible objects
		void setAllUsed();
		//! check if there were unknown params
		bool checkUnusedParams();

		//! import attributes from other attribute list
		void import(AttributeList *oal);

		//! read attributes for object initialization
		int      readInt(string name, int defaultValue,         string source,string target, bool needed);
		bool     readBool(string name, bool defaultValue,       string source,string target, bool needed);
		double   readFloat(string name, double defaultValue,   string source,string target, bool needed);
		string   readString(string name, string defaultValue,   string source,string target, bool needed);
		ntlVec3d readVec3d(string name, ntlVec3d defaultValue,  string source,string target, bool needed);
		void readMat4Gfx(string name, ntlMatrix4x4<gfxReal> defaultValue,  string source,string target, bool needed, ntlMatrix4x4<gfxReal> *mat);
		//! read attributes channels (attribute should be inited before)
		AnimChannel<int>     readChannelInt(         string name, int defaultValue=0, string source=string("src"), string target=string("dst"), bool needed=false );
		AnimChannel<double>  readChannelFloat(       string name, double defaultValue=0, string source=string("src"), string target=string("dst"), bool needed=false );
		AnimChannel<ntlVec3d> readChannelVec3d(      string name, ntlVec3d defaultValue=ntlVec3d(0.), string source=string("src"), string target=string("dst"), bool needed=false );
		AnimChannel<ntlSetVec3f> readChannelSetVec3f(string name, ntlSetVec3f defaultValue=ntlSetVec3f(0.), string source=string("src"), string target=string("dst"), bool needed=false );
		// channels with conversion
		AnimChannel<ntlVec3f> readChannelVec3f(           string name, ntlVec3f defaultValue=ntlVec3f(0.), string source=string("src"), string target=string("dst"), bool needed=false );
		AnimChannel<float>    readChannelSinglePrecFloat( string name, float defaultValue=0., string source=string("src"), string target=string("dst"), bool needed=false );

		//! set that a parameter can be given, and will be ignored...
		bool ignoreParameter(string name, string source);

		//! debug function, prints all attribs 
		void print();
		
	protected:

		/*! attribute name (form config file) */
		string mName;

		/*! the global attribute storage */
		map<string, Attribute*> mAttrs;

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

#define NTL_ATTRIBUTES_H
#endif


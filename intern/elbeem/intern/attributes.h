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
#include "ntl_matrices.h"


//! An animated attribute channel
template<class Scalar>
class AnimChannel
{
	public:
		// null init constructor
		AnimChannel(Scalar null) : 
			mValue(1), mTimes(1) { mValue[0]=null; mTimes[0]=0.0; };

		// proper init
		AnimChannel(vector<Scalar> v, vector<double> t) : 
			mValue(v), mTimes(t) { };

		// desctructor, nothing to do
		~AnimChannel() { };

		// get interpolated value at time t
		Scalar get(double t) {
			if(t<=mTimes[0])               { return mValue[0]; }
			if(t>=mTimes[mTimes.size()-1]) { return mValue[mTimes.size()-1]; }
			for(size_t i=0; i<mTimes.size()-1; i++) {
				// find first time thats in between
				if((mTimes[i]<=t)&&(mTimes[i+1]>t)) { 
					// interpolate
					double d = mTimes[i+1]-mTimes[i];
					double f = (t-mTimes[i])/d;
					return mValue[i] * (1.0-f) + mValue[i+1] * f;
				}
			}
			// whats this...?
			return mValue[0];
		};

		// get uninterpolated value at time t
		Scalar getConstant(double t) {
			//errMsg("DEBB","getc"<<t<<" ");
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

	protected:

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
		string getAsString();
		//! get value as integer value
		int getAsInt();
		//! get value as boolean
		bool getAsBool();
		//! get value as double value
		double getAsFloat();
		//! get value as 3d vector 
		ntlVec3d getAsVec3d();
		//! get value as 4x4 matrix
		ntlMat4Gfx getAsMat4Gfx();

		//! get channel as integer value
		AnimChannel<int> getChannelInt();
		//! get channel as double value
		AnimChannel<double> getChannelFloat();
		//! get channel as double value
		AnimChannel<ntlVec3d> getChannelVec3d();

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
		ntlMat4Gfx readMat4Gfx(string name, ntlMat4Gfx defaultValue,  string source,string target, bool needed);
		//! read attributes channels (attribute should be inited before)
		AnimChannel<int>     readChannelInt(string name);
		AnimChannel<double>  readChannelFloat(string name);
		AnimChannel<ntlVec3d> readChannelVec3d(string name);

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

#define NTL_ATTRIBUTES_H
#endif


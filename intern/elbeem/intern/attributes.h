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


//! A single attribute
class Attribute
{
	public:
  	//! Standard constructor
  	Attribute(string mn, vector<string> &value, int setline) :
			mName(mn), mValue(value), 
			mLine(setline), mUsed(false) { };
  	//! Copy constructor
  	Attribute(Attribute &a) :
			mName(a.mName), mValue(a.mValue), 
			mLine(a.mLine), mUsed(false) { };
  	//! Destructor
  	~Attribute() { /* empty */ };

		//! set used flag
		void setUsed(bool set){ mUsed = set; }
		//! get used flag
		bool getUsed() { return mUsed; }

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

		//! get the concatenated string of all value string
		string getCompleteString();

		//! debug function, prints value 
		void print();
		
	protected:

		/*! the attr name */
		string mName;

		/*! the attr value */
		vector<string> mValue;

		/*! line where the value was defined in the config file (for error messages) */
		int mLine;

		/*! was this attribute used? */
		bool mUsed;
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
		void addAttr(string name, vector<string> &value, int line) {
			if(exists(name)) delete mAttrs[name];
			mAttrs[name] = new Attribute(name,value,line);
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
				return new Attribute(name,empty, -1);
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


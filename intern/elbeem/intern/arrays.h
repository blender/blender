/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * Array class definitions 
 * 
 *****************************************************************************/
#ifndef ARRAYS_H
#include <string>
#include <sstream>
#include <fstream>


/*****************************************************************************/
/* array handling "cutting off" access along the border */
template<class T>
class ArrayCutoffBc {
	public:
		//! constructor
		ArrayCutoffBc() :
			mpVal( NULL ), mElemSize( sizeof(T) ),
			mAllocSize(0),
  		mSizex(0), mSizey(0), mSizez(0)
			{ };
		//! destructor
		virtual ~ArrayCutoffBc() {
			if((mpVal)&&(mAllocSize>0)) delete[] mpVal;
			mpVal = NULL;
		}

		//! init sizes
		void initializeArray(int setx, int sety, int setz) {
			mSizex = setx;
			mSizey = sety;
			mSizez = setz;
		}

		//! allocate a new array
		inline void allocate() { 
			int size = mSizex*mSizey*mSizez;
			if(size == mAllocSize) return; // dont reallocate
			T* newval = new T[size];
			for(int i=0;i<size;i++) newval[i] = (T)(0.0);
			mpVal = (unsigned char *)newval;
			mAllocSize = size;
		};

		//! set the scalar field pointer 
		inline void setValuePointer(T *pnt, int elem) { mpVal = (unsigned char *)pnt; mElemSize = elem; };

		//! internal array index calculator
		inline int arrayIndex(int x, int y, int z) { 
			if(x<0) x=0;
			if(y<0) y=0;
			if(z<0) z=0;
			if(x>mSizex-1) x=mSizex-1;
			if(y>mSizey-1) y=mSizey-1;
			if(z>mSizez-1) z=mSizez-1;
			return  z*mSizex*mSizey + y*mSizex + x;
		}
		//! phi access function
		inline T& getValue(int x, int y, int z) { 
			unsigned char *bpnt = &mpVal[ arrayIndex(x,y,z)*mElemSize ];
			return *((T*)bpnt);
			//return mpPhi[ z*mSizex*mSizey + y*mSizex + x]; 
		}
		//! return relative offset in direction dir (x=0,y=1,z=2)
		inline T& getOffset(T *base,int off, int dir) { 
			unsigned char *basep = (unsigned char *)base;
			int multiplier = 1;
			if(dir==1) multiplier=mSizex;
			if(dir==2) multiplier=mSizex*mSizey;
			// check boundary
			unsigned char *bpnt = (basep+ ((off*multiplier)*mElemSize) );
			if(bpnt<mpVal) bpnt = basep;
			if(bpnt>= (unsigned char *)&getValue(mSizex-1,mSizey-1,mSizez-1) ) bpnt = basep;
			return *((T*)bpnt);
		}

		//! perform trilinear interpolation of array values
		inline T interpolateValueAt(LbmFloat x, LbmFloat y, LbmFloat z) { 
			const LbmFloat gsx=1.0, gsy=1.0, gsz=1.0; 
			int i= (int)x;
			int j= (int)y;
			int k= (int)z;

			int in = i+1;
			int jn = j+1;
			int kn = k+1;
			if(in>=mSizex) in = mSizex-1;
			if(jn>=mSizey) jn = mSizey-1;
			if(kn>=mSizez) kn = mSizez-1;

			LbmVec mStart(0.0); // TODO remove?
			LbmFloat x1 = mStart[0]+ (LbmFloat)(i)*gsx;
			LbmFloat x2 = mStart[0]+ (LbmFloat)(in)*gsx;
			LbmFloat y1 = mStart[1]+ (LbmFloat)(j)*gsy;
			LbmFloat y2 = mStart[1]+ (LbmFloat)(jn)*gsy;
			LbmFloat z1 = mStart[2]+ (LbmFloat)(k)*gsz;
			LbmFloat z2 = mStart[2]+ (LbmFloat)(kn)*gsz;

			if(mSizez==1) { 
				z1=0.0; z2=1.0;
				k = kn = 0;
			}

			T v1, v2, v3, v4, v5, v6, v7, v8;
			v1 = getValue(i  ,j  ,k  );
			v2 = getValue(in ,j  ,k  );
			v3 = getValue(i  ,jn ,k  );
			v4 = getValue(in ,jn ,k  );
			v5 = getValue(i  ,j  ,kn );
			v6 = getValue(in ,j  ,kn );
			v7 = getValue(i  ,jn ,kn );
			v8 = getValue(in ,jn ,kn );

			T val =
				( v1 *(x2-x)* (y2-y)* (z2-z) +
					v2 *(x-x1)* (y2-y)* (z2-z) +
					v3 *(x2-x)* (y-y1)* (z2-z) +
					v4 *(x-x1)* (y-y1)* (z2-z) +
					v5 *(x2-x)* (y2-y)* (z-z1) +
					v6 *(x-x1)* (y2-y)* (z-z1) +
					v7 *(x2-x)* (y-y1)* (z-z1) +
					v8 *(x-x1)* (y-y1)* (z-z1) 
				) * (1.0/(gsx*gsy*gsz)) ;
			return val;
		}

		//! get size of an element
		inline int getElementSize(){ return mElemSize; }
		//! get array sizes
		inline int getSizeX(){ return mSizex; }
		inline int getSizeY(){ return mSizey; }
		inline int getSizeZ(){ return mSizez; }
		//! get array pointer
		inline T* getPointer(){ return (T*)mpVal; }

		//! testing, gnuplot dump (XY plane for k=Z/2)
		void dumpToFile(std::string filebase, int id, int nr) {
			std::ostringstream filename;
			filename << filebase << "_"<< id <<"_"<< nr <<".dump";
			std::ofstream outfile( filename.str().c_str() );
			for(int k=mSizez/2; k<=mSizez/2; k++) {
				for(int j=0; j<mSizey; j++) {
					for(int i=0; i<mSizex; i++) {
						outfile <<i<<" "<<j<<" " << getValue(i,j,k)<<" " <<std::endl;
					}
					outfile << std::endl;
				}
			}
		}
		//! testing, grid text dump (XY plane for k=Z/2)
		void dumpToGridFile(std::string filebase, int id, int nr) {
			std::ostringstream filename;
			filename << filebase << "_"<< id <<"_"<< nr <<".dump";
			std::ofstream outfile( filename.str().c_str() );
			for(int k=mSizez/2; k<=mSizez/2; k++) {
				for(int j=0; j<mSizey; j++) {
					for(int i=0; i<mSizex; i++) {
						outfile <<getValue(i,j,k)<<"\t";
					}
					outfile << std::endl;
				}
			}
		}

	protected:
		//! pointer for the value field (unsigned char for adding element size) 
		unsigned char *mpVal;
		//! element offset in array 
		int mElemSize;
		//! store allocated size
		int mAllocSize;

		//! Sizes of the scal array in each dimension 
		int mSizex,mSizey,mSizez;
};

/*****************************************************************************/
/* array handling "cutting off" access along the border */
template<class T>
class ArrayPlain {
	public:
		//! constructor
		ArrayPlain() :
			mpVal( NULL ), mElemSize( sizeof(T) ),
			mAllocSize(0),
  		mSizex(0), mSizey(0), mSizez(0)
			{ };
		//! destructor
		virtual ~ArrayPlain() {
			if((mpVal)&&(mAllocSize>0)) delete[] mpVal;
			mpVal = NULL;
		}

		//! init sizes
		void initializeArray(int setx, int sety, int setz) {
			mSizex = setx;
			mSizey = sety;
			mSizez = setz;
		}

		//! allocate a new array
		inline void allocate() { 
			int size = mSizex*mSizey*mSizez;
			if(size == mAllocSize) return; // dont reallocate
			T* newval = new T[size];
			for(int i=0;i<size;i++) newval[i] = (T)(0.0);
			mpVal = (unsigned char *)newval;
			mAllocSize = size;
		};

		//! set the scalar field pointer 
		inline void setValuePointer(T *pnt, int elem) { mpVal = (unsigned char *)pnt; mElemSize = elem; };

		//! phi access function
		inline T& getValue(const int x, const int y, const int z) const { 
			unsigned char *bpnt = &mpVal[ (z*mSizex*mSizey + y*mSizex + x)*mElemSize ];
			return *((T*)bpnt);
		}
		//! return relative offset in direction dir (x=0,y=1,z=2)
		inline T& getOffset(T *base,int off, int dir) { 
			unsigned char *basep = (unsigned char *)base;
			int multiplier = 1;
			if(dir==1) multiplier=mSizex;
			if(dir==2) multiplier=mSizex*mSizey;
			// check boundary
			unsigned char *bpnt = (basep+ ((off*multiplier)*mElemSize) );
			if(bpnt<mpVal) bpnt = basep;
			if(bpnt>= (unsigned char *)&getValue(mSizex-1,mSizey-1,mSizez-1) ) bpnt = basep;
			return *((T*)bpnt);
		}

		//! get size of an element
		inline int getElementSize(){ return mElemSize; }
		//! get array sizes
		inline int getSizeX(){ return mSizex; }
		inline int getSizeY(){ return mSizey; }
		inline int getSizeZ(){ return mSizez; }
		//! get array pointer
		inline T* getPointer(){ return (T*)mpVal; }

		//! testing, gnuplot dump (XY plane for k=Z/2)
		void dumpToFile(std::string filebase, int id, int nr) {
			std::ostringstream filename;
			filename << filebase << "_"<< id <<"_"<< nr <<".dump";
			std::ofstream outfile( filename.str().c_str() );
			for(int k=mSizez/2; k<=mSizez/2; k++) {
				for(int j=0; j<mSizey; j++) {
					for(int i=0; i<mSizex; i++) {
						outfile <<i<<" "<<j<<" " << getValue(i,j,k)<<" " <<std::endl;
					}
					outfile << std::endl;
				}
			}
		}
		//! testing, grid text dump (XY plane for k=Z/2)
		void dumpToGridFile(std::string filebase, int id, int nr) {
			std::ostringstream filename;
			filename << filebase << "_"<< id <<"_"<< nr <<".dump";
			std::ofstream outfile( filename.str().c_str() );
			for(int k=mSizez/2; k<=mSizez/2; k++) {
				for(int j=0; j<mSizey; j++) {
					for(int i=0; i<mSizex; i++) {
						outfile <<getValue(i,j,k)<<"\t";
					}
					outfile << std::endl;
				}
			}
		}

	protected:
		//! pointer for the value field (unsigned char for adding element size) 
		unsigned char *mpVal;
		//! element offset in array 
		int mElemSize;
		//! store allocated size
		int mAllocSize;

		//! Sizes of the scal array in each dimension 
		int mSizex,mSizey,mSizez;
};



#define ARRAYS_H
#endif


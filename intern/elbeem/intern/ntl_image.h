/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * a templated image class
 *
 *****************************************************************************/

#ifndef NTL_IMAGE_HH
#define NTL_IMAGE_HH

#include "ntl_vector3dim.h"
#include "utilities.h"


template<class Value>
class ntlImage
{
public:
	/*! Default constructor */
	ntlImage();
	/*! Init constructor */
	ntlImage(int x,int y, Value def);
	/*! Destructor, delete contents */
	~ntlImage();

	/*! Write the image to a ppm file */
	void writePpm(const char* name);
	
	/*! normalize values into range 0..1 */
	void normalize( void );

	/*! Get a pixel from the image */
	inline Value get(int x, int y) { return mpC[y*mSizex+x]; }
	
	/*! Set a pixel in the image */
	inline void set(int x, int y, Value set) { mpC[y*mSizex+x] = set; }
	
protected:
private:
	
	/*! size in x dimension */
	int mSizex;

	/*! size in y dimension */
	int mSizey;

	/*! Image contents */
	Value *mpC;

};


/*! Default constructor */
template<class Value>
ntlImage<Value>::ntlImage()
{
	mSizex = 0;
	mSizey = 0;
	mpC = NULL;
}

/*! Init constructor */
template<class Value>
ntlImage<Value>::ntlImage(int x,int y,Value def)
{
	mSizex = x;
	mSizey = y;
	mpC = new Value[x*y];

	for(int i=0;i<(x*y);i++) mpC[i] = def;
}

/*! Destructor, delete contents */
template<class Value>
ntlImage<Value>::~ntlImage()
{
	if(mpC != NULL) {
		delete [] mpC;
	}
}


/*! Write the image to a ppm file */
template<class Value>
void ntlImage<Value>::writePpm(const char* name)
{
	/* write file */
	/* open output file */
	FILE *outfile;
	if ( (outfile = fopen(name,"w")) == NULL ) {
		errorOut( "ntlImage::writePpm ERROR: Open out file failed "<<name<<"!\n" );
		return;
	}

	int maxColVal = 255;

	/* write ppm header */
	fprintf(outfile,"P3\n%d %d\n%d\n",mSizex,mSizey, maxColVal );

	/* get min max values */
	Value min = mpC[0];
	Value max = mpC[0];
	for(int j=0;j<(mSizey*mSizex);j++) {
		if(mpC[j]<min) min = mpC[j];
		if(mpC[j]>max) max = mpC[j];
	}

	/* check colors for overflow */
	for(int j=0;j<mSizey;j++) {
		for(int i=0;i<mSizex;i++) {

			Value grey;
			if(max-min>0) {
				grey = 1.0-(mpC[j*mSizex+i]-min)/(max-min);
			} else { grey = 1.0; }
			//mpC[j*mSizex+i] /= max;
			ntlColor col = ntlColor(grey,grey,grey);
			unsigned int cCol[3];
			for (unsigned int cc=0; cc<3; cc++) {
				if(col[cc] <= 0.0)
					cCol[cc] = 0;
				else if(col[cc] >= 1.0)
					cCol[cc] = maxColVal;
				else
					cCol[cc] = (unsigned int)(maxColVal * col[cc]);
			}
	
			/* write pixel to ppm file */
			fprintf(outfile,"%4d %4d %4d ",cCol[0],cCol[1],cCol[2]);
      
		} /* foreach x */

		fprintf(outfile,"\n");
	} /* foreach y */
  
	/* clean up */
	fclose(outfile);
}



/*! Write the image to a ppm file */
template<class Value>
void ntlImage<Value>::normalize( void )
{
	/* get min max values */
	Value min = mpC[0];
	Value max = mpC[0];
	for(int j=0;j<(mSizey*mSizex);j++) {
		if(mpC[j]<min) min = mpC[j];
		if(mpC[j]>max) max = mpC[j];
	}

	/* check colors for overflow */
	for(int j=0;j<mSizey;j++) {
		for(int i=0;i<mSizex;i++) {
			Value grey = 1.0-(mpC[j*mSizex+i]-min)/(max-min);
			mpC[j*mSizex+i] = grey;
		} /* foreach x */
	} /* foreach y */
}


#endif


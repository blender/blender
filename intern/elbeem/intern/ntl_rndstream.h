/******************************************************************************
 *
 * El'Beem - Free Surface Fluid Simulation with the Lattice Boltzmann Method
 * Copyright 2003,2004 Nils Thuerey
 *
 * A seperate random number stream (e.g. for photon tracing)
 * (cf. Numerical recipes in C, sec. ed., p 283, Knuth method)
 *
 *****************************************************************************/
#ifndef NTL_RANDOMSTREAM_H


//! some big number
#define MBIG 1000000000

//! modify initial seed
#define MSEED 161803398

//! minimum value - no idea why this is a define?
#define MZ 0

//! for normalization to 0..1
#define FAC (1.0/MBIG)


/*! a stream of random numbers using Knuth's portable method */
class ntlRandomStream
{
public:
  /*! Default constructor */
  inline ntlRandomStream(long seed);
  /*! Destructor */
  ~ntlRandomStream() {}

	/*! get a random number from the stream */
	inline double getDouble( void );

#ifdef HAVE_GFXTYPES
	//! gfx random functions
	
	/*! get a random number from the stream */
	inline gfxReal getGfxReal( void );
#endif

private:

	/*! random number state */
	long idnum;

	/*! pointers into number table */ 
	int inext, inextp;
	/*! store seed and number for subtraction */
	long ma[56];

};


/* init random stream tables */
inline ntlRandomStream::ntlRandomStream(long seed)
{
	idnum = seed;

	long mj = MSEED - (idnum < 0 ? -idnum : idnum);
	mj %= MBIG;
	ma[55] = mj;
	long mk = 1;

	// init table once, otherwise strange results...
	for(int i=0;i<=55;i++) ma[i] = (i*i+seed); 

	// init table in random order
	for(int i=1;i<=54;i++) {
		int ii = (21*i) % 56;
		ma[ii] = mk;
		mk = mj - mk;
		if(mk < MZ) mk += MBIG;
		mj = ma[ii];
	}

	// "warm up" generator
	for(int k=1;k<=4;k++) 
		for(int i=1;i<=55;i++) {
			ma[i] -= ma[1+ (i+30) % 55];
			if(ma[i] < MZ) ma[i] += MBIG;
		}

	inext = 0;
	inextp = 31; // the special "31"
	idnum = 1;
}


/* return one random value */
inline double ntlRandomStream::getDouble( void )
{
	if( ++inext == 56) inext = 1;
	if( ++inextp == 56) inextp = 1;

	// generate by subtaction
	long mj = ma[inext] - ma[inextp];

	// check range
	if(mj < MZ) mj += MBIG;
	ma[ inext ] = mj;
	return (double)(mj * FAC);
}

#ifdef HAVE_GFXTYPES
/* return one random value */
inline gfxReal ntlRandomStream::getGfxReal( void )
{
	if( ++inext == 56) inext = 1;
	if( ++inextp == 56) inextp = 1;

	// generate by subtaction
	long mj = ma[inext] - ma[inextp];

	// check range
	if(mj < MZ) mj += MBIG;
	ma[ inext ] = mj;
	return (gfxReal)(mj * FAC);
}
#endif

#define NTL_RANDOMSTREAM_H
#endif


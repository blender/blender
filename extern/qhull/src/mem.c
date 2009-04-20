/*<html><pre>  -<a                             href="qh-mem.htm"
  >-------------------------------</a><a name="TOP">-</a>

  mem.c 
    memory management routines for qhull

  This is a standalone program.
   
  To initialize memory:

    qh_meminit (stderr);  
    qh_meminitbuffers (qh IStracing, qh_MEMalign, 7, qh_MEMbufsize,qh_MEMinitbuf);
    qh_memsize(sizeof(facetT));
    qh_memsize(sizeof(facetT));
    ...
    qh_memsetup();
    
  To free up all memory buffers:
    qh_memfreeshort (&curlong, &totlong);
         
  if qh_NOmem, 
    malloc/free is used instead of mem.c

  notes: 
    uses Quickfit algorithm (freelists for commonly allocated sizes)
    assumes small sizes for freelists (it discards the tail of memory buffers)
   
  see:
    qh-mem.htm and mem.h
    global.c (qh_initbuffers) for an example of using mem.c 
   
  copyright (c) 1993-2002 The Geometry Center
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mem.h"

#ifndef qhDEFqhull
typedef struct ridgeT ridgeT;
typedef struct facetT facetT;
void    qh_errexit(int exitcode, facetT *, ridgeT *);
#endif

/*============ -global data structure ==============
    see mem.h for definition
*/

qhmemT qhmem= {0};     /* remove "= {0}" if this causes a compiler error */

#ifndef qh_NOmem

/*============= internal functions ==============*/
  
static int qh_intcompare(const void *i, const void *j);

/*========== functions in alphabetical order ======== */

/*-<a                             href="qh-mem.htm#TOC"
  >-------------------------------</a><a name="intcompare">-</a>
  
  qh_intcompare( i, j )
    used by qsort and bsearch to compare two integers
*/
static int qh_intcompare(const void *i, const void *j) {
  return(*((int *)i) - *((int *)j));
} /* intcompare */


/*-<a                             href="qh-mem.htm#TOC"
  >--------------------------------</a><a name="memalloc">-</a>
   
  qh_memalloc( insize )  
    returns object of insize bytes
    qhmem is the global memory structure 
    
  returns:
    pointer to allocated memory 
    errors if insufficient memory

  notes:
    use explicit type conversion to avoid type warnings on some compilers
    actual object may be larger than insize
    use qh_memalloc_() for inline code for quick allocations
    logs allocations if 'T5'
  
  design:
    if size < qhmem.LASTsize
      if qhmem.freelists[size] non-empty
        return first object on freelist
      else
        round up request to size of qhmem.freelists[size]
        allocate new allocation buffer if necessary
        allocate object from allocation buffer
    else
      allocate object with malloc()
*/
void *qh_memalloc(int insize) {
  void **freelistp, *newbuffer;
  int index, size;
  int outsize, bufsize;
  void *object;

  if ((unsigned) insize <= (unsigned) qhmem.LASTsize) {
    index= qhmem.indextable[insize];
    freelistp= qhmem.freelists+index;
    if ((object= *freelistp)) {
      qhmem.cntquick++;  
      *freelistp= *((void **)*freelistp);  /* replace freelist with next object */
      return (object);
    }else {
      outsize= qhmem.sizetable[index];
      qhmem.cntshort++;
      if (outsize > qhmem .freesize) {
	if (!qhmem.curbuffer)
	  bufsize= qhmem.BUFinit;
        else
	  bufsize= qhmem.BUFsize;
        qhmem.totshort += bufsize;
	if (!(newbuffer= malloc(bufsize))) {
	  fprintf(qhmem.ferr, "qhull error (qh_memalloc): insufficient memory\n");
	  qh_errexit(qhmem_ERRmem, NULL, NULL);
	} 
	*((void **)newbuffer)= qhmem.curbuffer;  /* prepend newbuffer to curbuffer 
						    list */
	qhmem.curbuffer= newbuffer;
        size= (sizeof(void **) + qhmem.ALIGNmask) & ~qhmem.ALIGNmask;
	qhmem.freemem= (void *)((char *)newbuffer+size);
	qhmem.freesize= bufsize - size;
      }
      object= qhmem.freemem;
      qhmem.freemem= (void *)((char *)qhmem.freemem + outsize);
      qhmem.freesize -= outsize;
      return object;
    }
  }else {                     /* long allocation */
    if (!qhmem.indextable) {
      fprintf (qhmem.ferr, "qhull internal error (qh_memalloc): qhmem has not been initialized.\n");
      qh_errexit(qhmem_ERRqhull, NULL, NULL);
    }
    outsize= insize;
    qhmem .cntlong++;
    qhmem .curlong++;
    qhmem .totlong += outsize;
    if (qhmem.maxlong < qhmem.totlong)
      qhmem.maxlong= qhmem.totlong;
    if (!(object= malloc(outsize))) {
      fprintf(qhmem.ferr, "qhull error (qh_memalloc): insufficient memory\n");
      qh_errexit(qhmem_ERRmem, NULL, NULL);
    }
    if (qhmem.IStracing >= 5)
      fprintf (qhmem.ferr, "qh_memalloc long: %d bytes at %p\n", outsize, object);
  }
  return (object);
} /* memalloc */


/*-<a                             href="qh-mem.htm#TOC"
  >--------------------------------</a><a name="memfree">-</a>
   
  qh_memfree( object, size ) 
    free up an object of size bytes
    size is insize from qh_memalloc

  notes:
    object may be NULL
    type checking warns if using (void **)object
    use qh_memfree_() for quick free's of small objects
 
  design:
    if size <= qhmem.LASTsize
      append object to corresponding freelist
    else
      call free(object)
*/
void qh_memfree(void *object, int size) {
  void **freelistp;

  if (!object)
    return;
  if (size <= qhmem.LASTsize) {
    qhmem .freeshort++;
    freelistp= qhmem.freelists + qhmem.indextable[size];
    *((void **)object)= *freelistp;
    *freelistp= object;
  }else {
    qhmem .freelong++;
    qhmem .totlong -= size;
    free (object);
    if (qhmem.IStracing >= 5)
      fprintf (qhmem.ferr, "qh_memfree long: %d bytes at %p\n", size, object);
  }
} /* memfree */


/*-<a                             href="qh-mem.htm#TOC"
  >-------------------------------</a><a name="memfreeshort">-</a>
  
  qh_memfreeshort( curlong, totlong )
    frees up all short and qhmem memory allocations

  returns:
    number and size of current long allocations
*/
void qh_memfreeshort (int *curlong, int *totlong) {
  void *buffer, *nextbuffer;

  *curlong= qhmem .cntlong - qhmem .freelong;
  *totlong= qhmem .totlong;
  for(buffer= qhmem.curbuffer; buffer; buffer= nextbuffer) {
    nextbuffer= *((void **) buffer);
    free(buffer);
  }
  qhmem.curbuffer= NULL;
  if (qhmem .LASTsize) {
    free (qhmem .indextable);
    free (qhmem .freelists);
    free (qhmem .sizetable);
  }
  memset((char *)&qhmem, 0, sizeof qhmem);  /* every field is 0, FALSE, NULL */
} /* memfreeshort */


/*-<a                             href="qh-mem.htm#TOC"
  >--------------------------------</a><a name="meminit">-</a>
   
  qh_meminit( ferr )
    initialize qhmem and test sizeof( void*)
*/
void qh_meminit (FILE *ferr) {
  
  memset((char *)&qhmem, 0, sizeof qhmem);  /* every field is 0, FALSE, NULL */
  qhmem.ferr= ferr;
  if (sizeof(void*) < sizeof(int)) {
    fprintf (ferr, "qhull internal error (qh_meminit): sizeof(void*) < sizeof(int).  qset.c will not work\n");
    exit (1);  /* can not use qh_errexit() */
  }
} /* meminit */

/*-<a                             href="qh-mem.htm#TOC"
  >-------------------------------</a><a name="meminitbuffers">-</a>
  
  qh_meminitbuffers( tracelevel, alignment, numsizes, bufsize, bufinit )
    initialize qhmem
    if tracelevel >= 5, trace memory allocations
    alignment= desired address alignment for memory allocations
    numsizes= number of freelists
    bufsize=  size of additional memory buffers for short allocations
    bufinit=  size of initial memory buffer for short allocations
*/
void qh_meminitbuffers (int tracelevel, int alignment, int numsizes, int bufsize, int bufinit) {

  qhmem.IStracing= tracelevel;
  qhmem.NUMsizes= numsizes;
  qhmem.BUFsize= bufsize;
  qhmem.BUFinit= bufinit;
  qhmem.ALIGNmask= alignment-1;
  if (qhmem.ALIGNmask & ~qhmem.ALIGNmask) {
    fprintf (qhmem.ferr, "qhull internal error (qh_meminit): memory alignment %d is not a power of 2\n", alignment);
    qh_errexit (qhmem_ERRqhull, NULL, NULL);
  }
  qhmem.sizetable= (int *) calloc (numsizes, sizeof(int));
  qhmem.freelists= (void **) calloc (numsizes, sizeof(void *));
  if (!qhmem.sizetable || !qhmem.freelists) {
    fprintf(qhmem.ferr, "qhull error (qh_meminit): insufficient memory\n");
    qh_errexit (qhmem_ERRmem, NULL, NULL);
  }
  if (qhmem.IStracing >= 1)
    fprintf (qhmem.ferr, "qh_meminitbuffers: memory initialized with alignment %d\n", alignment);
} /* meminitbuffers */

/*-<a                             href="qh-mem.htm#TOC"
  >-------------------------------</a><a name="memsetup">-</a>
  
  qh_memsetup()
    set up memory after running memsize()
*/
void qh_memsetup (void) {
  int k,i;

  qsort(qhmem.sizetable, qhmem.TABLEsize, sizeof(int), qh_intcompare);
  qhmem.LASTsize= qhmem.sizetable[qhmem.TABLEsize-1];
  if (qhmem .LASTsize >= qhmem .BUFsize || qhmem.LASTsize >= qhmem .BUFinit) {
    fprintf (qhmem.ferr, "qhull error (qh_memsetup): largest mem size %d is >= buffer size %d or initial buffer size %d\n",
            qhmem .LASTsize, qhmem .BUFsize, qhmem .BUFinit);
    qh_errexit(qhmem_ERRmem, NULL, NULL);
  }
  if (!(qhmem.indextable= (int *)malloc((qhmem.LASTsize+1) * sizeof(int)))) {
    fprintf(qhmem.ferr, "qhull error (qh_memsetup): insufficient memory\n");
    qh_errexit(qhmem_ERRmem, NULL, NULL);
  }
  for(k=qhmem.LASTsize+1; k--; )
    qhmem.indextable[k]= k;
  i= 0;
  for(k= 0; k <= qhmem.LASTsize; k++) {
    if (qhmem.indextable[k] <= qhmem.sizetable[i])
      qhmem.indextable[k]= i;
    else
      qhmem.indextable[k]= ++i;
  }
} /* memsetup */

/*-<a                             href="qh-mem.htm#TOC"
  >-------------------------------</a><a name="memsize">-</a>
  
  qh_memsize( size )
    define a free list for this size
*/
void qh_memsize(int size) {
  int k;

  if (qhmem .LASTsize) {
    fprintf (qhmem .ferr, "qhull error (qh_memsize): called after qhmem_setup\n");
    qh_errexit (qhmem_ERRqhull, NULL, NULL);
  }
  size= (size + qhmem.ALIGNmask) & ~qhmem.ALIGNmask;
  for(k= qhmem.TABLEsize; k--; ) {
    if (qhmem.sizetable[k] == size)
      return;
  }
  if (qhmem.TABLEsize < qhmem.NUMsizes)
    qhmem.sizetable[qhmem.TABLEsize++]= size;
  else
    fprintf(qhmem.ferr, "qhull warning (memsize): free list table has room for only %d sizes\n", qhmem.NUMsizes);
} /* memsize */


/*-<a                             href="qh-mem.htm#TOC"
  >-------------------------------</a><a name="memstatistics">-</a>
  
  qh_memstatistics( fp )
    print out memory statistics

  notes:
    does not account for wasted memory at the end of each block
*/
void qh_memstatistics (FILE *fp) {
  int i, count, totfree= 0;
  void *object;
  
  for (i=0; i < qhmem.TABLEsize; i++) {
    count=0;
    for (object= qhmem .freelists[i]; object; object= *((void **)object))
      count++;
    totfree += qhmem.sizetable[i] * count;
  }
  fprintf (fp, "\nmemory statistics:\n\
%7d quick allocations\n\
%7d short allocations\n\
%7d long allocations\n\
%7d short frees\n\
%7d long frees\n\
%7d bytes of short memory in use\n\
%7d bytes of short memory in freelists\n\
%7d bytes of long memory allocated (except for input)\n\
%7d bytes of long memory in use (in %d pieces)\n\
%7d bytes per memory buffer (initially %d bytes)\n",
	   qhmem .cntquick, qhmem.cntshort, qhmem.cntlong,
	   qhmem .freeshort, qhmem.freelong, 
	   qhmem .totshort - qhmem .freesize - totfree,
	   totfree,
	   qhmem .maxlong, qhmem .totlong, qhmem .cntlong - qhmem .freelong,
	   qhmem .BUFsize, qhmem .BUFinit);
  if (qhmem.cntlarger) {
    fprintf (fp, "%7d calls to qh_setlarger\n%7.2g     average copy size\n",
	   qhmem.cntlarger, ((float) qhmem.totlarger)/ qhmem.cntlarger);
    fprintf (fp, "  freelists (bytes->count):");
  }
  for (i=0; i < qhmem.TABLEsize; i++) {
    count=0;
    for (object= qhmem .freelists[i]; object; object= *((void **)object))
      count++;
    fprintf (fp, " %d->%d", qhmem.sizetable[i], count);
  }
  fprintf (fp, "\n\n");
} /* memstatistics */


/*-<a                             href="qh-mem.htm#TOC"
  >-------------------------------</a><a name="NOmem">-</a>
  
  qh_NOmem
    turn off quick-fit memory allocation

  notes:
    uses malloc() and free() instead
*/
#else /* qh_NOmem */

void *qh_memalloc(int insize) {
  void *object;

  if (!(object= malloc(insize))) {
    fprintf(qhmem.ferr, "qhull error (qh_memalloc): insufficient memory\n");
    qh_errexit(qhmem_ERRmem, NULL, NULL);
  }
  if (qhmem.IStracing >= 5)
    fprintf (qhmem.ferr, "qh_memalloc long: %d bytes at %p\n", insize, object);
  return object;
}

void qh_memfree(void *object, int size) {

  if (!object)
    return;
  free (object);
  if (qhmem.IStracing >= 5)
    fprintf (qhmem.ferr, "qh_memfree long: %d bytes at %p\n", size, object);
}

void qh_memfreeshort (int *curlong, int *totlong) {

  memset((char *)&qhmem, 0, sizeof qhmem);  /* every field is 0, FALSE, NULL */
  *curlong= 0;
  *totlong= 0;
}

void qh_meminit (FILE *ferr) {

  memset((char *)&qhmem, 0, sizeof qhmem);  /* every field is 0, FALSE, NULL */
  qhmem.ferr= ferr;
  if (sizeof(void*) < sizeof(int)) {
    fprintf (ferr, "qhull internal error (qh_meminit): sizeof(void*) < sizeof(int).  qset.c will not work\n");
    qh_errexit (qhmem_ERRqhull, NULL, NULL);
  }
}

void qh_meminitbuffers (int tracelevel, int alignment, int numsizes, int bufsize, int bufinit) {

  qhmem.IStracing= tracelevel;

}

void qh_memsetup (void) {

}

void qh_memsize(int size) {

}

void qh_memstatistics (FILE *fp) {

}

#endif /* qh_NOmem */

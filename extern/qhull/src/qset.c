/*<html><pre>  -<a                             href="qh-set.htm"
  >-------------------------------</a><a name="TOP">-</a>

   qset.c 
   implements set manipulations needed for quickhull 

   see qh-set.htm and qset.h

   copyright (c) 1993-2002 The Geometry Center        
*/

#include <stdio.h>
#include <string.h>
/*** uncomment here and qhull_a.h 
     if string.h does not define memcpy()
#include <memory.h>
*/
#include "qset.h"
#include "mem.h"

#ifndef qhDEFqhull
typedef struct ridgeT ridgeT;
typedef struct facetT facetT;
void    qh_errexit(int exitcode, facetT *, ridgeT *);
#endif

/*=============== internal macros ===========================*/

/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="SETsizeaddr_">-</a>
   
  SETsizeaddr_(set) 
    return pointer to actual size+1 of set (set CANNOT be NULL!!)
      
  notes:
    *SETsizeaddr==NULL or e[*SETsizeaddr-1].p==NULL
*/
#define SETsizeaddr_(set) (&((set)->e[(set)->maxsize].i))

/*============ functions in alphabetical order ===================*/
  
/*-<a                             href="qh-set.htm#TOC"
  >--------------------------------<a name="setaddnth">-</a>
   
  qh_setaddnth( setp, nth, newelem)
    adds newelem as n'th element of sorted or unsorted *setp
      
  notes:
    *setp and newelem must be defined
    *setp may be a temp set
    nth=0 is first element
    errors if nth is out of bounds
   
  design:
    expand *setp if empty or full
    move tail of *setp up one
    insert newelem
*/
void qh_setaddnth(setT **setp, int nth, void *newelem) {
  int *sizep, oldsize, i;
  void **oldp, **newp;

  if (!*setp || !*(sizep= SETsizeaddr_(*setp))) {
    qh_setlarger(setp);
    sizep= SETsizeaddr_(*setp);
  }
  oldsize= *sizep - 1;
  if (nth < 0 || nth > oldsize) {
    fprintf (qhmem.ferr, "qhull internal error (qh_setaddnth): nth %d is out-of-bounds for set:\n", nth);
    qh_setprint (qhmem.ferr, "", *setp);
    qh_errexit (qhmem_ERRqhull, NULL, NULL);
  }
  (*sizep)++;
  oldp= SETelemaddr_(*setp, oldsize, void);   /* NULL */
  newp= oldp+1;
  for (i= oldsize-nth+1; i--; )  /* move at least NULL  */
    *(newp--)= *(oldp--);       /* may overwrite *sizep */
  *newp= newelem;
} /* setaddnth */


/*-<a                              href="qh-set.htm#TOC"
  >--------------------------------<a name="setaddsorted">-</a>
   
  setaddsorted( setp, newelem )
    adds an newelem into sorted *setp
      
  notes:
    *setp and newelem must be defined
    *setp may be a temp set
    nop if newelem already in set
  
  design:
    find newelem's position in *setp
    insert newelem
*/
void qh_setaddsorted(setT **setp, void *newelem) {
  int newindex=0;
  void *elem, **elemp;

  FOREACHelem_(*setp) {          /* could use binary search instead */
    if (elem < newelem)
      newindex++;
    else if (elem == newelem)
      return;
    else
      break;
  }
  qh_setaddnth(setp, newindex, newelem);
} /* setaddsorted */


/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setappend">-</a>
  
  qh_setappend( setp, newelem)
    append newelem to *setp

  notes:
    *setp may be a temp set
    *setp and newelem may be NULL

  design:
    expand *setp if empty or full
    append newelem to *setp
    
*/
void qh_setappend(setT **setp, void *newelem) {
  int *sizep;
  void **endp;

  if (!newelem)
    return;
  if (!*setp || !*(sizep= SETsizeaddr_(*setp))) {
    qh_setlarger(setp);
    sizep= SETsizeaddr_(*setp);
  }
  *(endp= &((*setp)->e[(*sizep)++ - 1].p))= newelem;
  *(++endp)= NULL;
} /* setappend */

/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setappend_set">-</a>
  
  qh_setappend_set( setp, setA) 
    appends setA to *setp

  notes:
    *setp can not be a temp set
    *setp and setA may be NULL

  design:
    setup for copy
    expand *setp if it is too small
    append all elements of setA to *setp 
*/
void qh_setappend_set(setT **setp, setT *setA) {
  int *sizep, sizeA, size;
  setT *oldset;

  if (!setA)
    return;
  SETreturnsize_(setA, sizeA);
  if (!*setp)
    *setp= qh_setnew (sizeA);
  sizep= SETsizeaddr_(*setp);
  if (!(size= *sizep))
    size= (*setp)->maxsize;
  else
    size--;
  if (size + sizeA > (*setp)->maxsize) {
    oldset= *setp;
    *setp= qh_setcopy (oldset, sizeA);
    qh_setfree (&oldset);
    sizep= SETsizeaddr_(*setp);
  }
  *sizep= size+sizeA+1;   /* memcpy may overwrite */
  if (sizeA > 0) 
    memcpy((char *)&((*setp)->e[size].p), (char *)&(setA->e[0].p), SETelemsize *(sizeA+1));
} /* setappend_set */


/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setappend2ndlast">-</a>
  
  qh_setappend2ndlast( setp, newelem )
    makes newelem the next to the last element in *setp

  notes:
    *setp must have at least one element
    newelem must be defined
    *setp may be a temp set

  design:
    expand *setp if empty or full
    move last element of *setp up one
    insert newelem
*/
void qh_setappend2ndlast(setT **setp, void *newelem) {
  int *sizep;
  void **endp, **lastp;
  
  if (!*setp || !*(sizep= SETsizeaddr_(*setp))) {
    qh_setlarger(setp);
    sizep= SETsizeaddr_(*setp);
  }
  endp= SETelemaddr_(*setp, (*sizep)++ -1, void); /* NULL */
  lastp= endp-1;
  *(endp++)= *lastp;
  *endp= NULL;    /* may overwrite *sizep */
  *lastp= newelem;
} /* setappend2ndlast */


/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setcheck">-</a>
  
  qh_setcheck( set, typename, id ) 
    check set for validity
    report errors with typename and id

  design:
    checks that maxsize, actual size, and NULL terminator agree
*/
void qh_setcheck(setT *set, char *tname, int id) {
  int maxsize, size;
  int waserr= 0;

  if (!set)
    return;
  SETreturnsize_(set, size);
  maxsize= set->maxsize;
  if (size > maxsize || !maxsize) {
    fprintf (qhmem.ferr, "qhull internal error (qh_setcheck): actual size %d of %s%d is greater than max size %d\n",
	     size, tname, id, maxsize);
    waserr= 1;
  }else if (set->e[size].p) {
    fprintf (qhmem.ferr, "qhull internal error (qh_setcheck): %s%d (size %d max %d) is not null terminated.\n",
	     tname, id, maxsize, size-1);
    waserr= 1;
  }
  if (waserr) {
    qh_setprint (qhmem.ferr, "ERRONEOUS", set);
    qh_errexit (qhmem_ERRqhull, NULL, NULL);
  }
} /* setcheck */


/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setcompact">-</a>
  
  qh_setcompact( set )
    remove internal NULLs from an unsorted set

  returns:
    updated set

  notes:
    set may be NULL
    it would be faster to swap tail of set into holes, like qh_setdel

  design:
    setup pointers into set
    skip NULLs while copying elements to start of set 
    update the actual size
*/
void qh_setcompact(setT *set) {
  int size;
  void **destp, **elemp, **endp, **firstp;

  if (!set)
    return;
  SETreturnsize_(set, size);
  destp= elemp= firstp= SETaddr_(set, void);
  endp= destp + size;
  while (1) {
    if (!(*destp++ = *elemp++)) {
      destp--;
      if (elemp > endp)
	break;
    }
  }
  qh_settruncate (set, destp-firstp);
} /* setcompact */


/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setcopy">-</a>
  
  qh_setcopy( set, extra )
    make a copy of a sorted or unsorted set with extra slots

  returns:
    new set

  design:
    create a newset with extra slots
    copy the elements to the newset
    
*/
setT *qh_setcopy(setT *set, int extra) {
  setT *newset;
  int size;

  if (extra < 0)
    extra= 0;
  SETreturnsize_(set, size);
  newset= qh_setnew(size+extra);
  *SETsizeaddr_(newset)= size+1;    /* memcpy may overwrite */
  memcpy((char *)&(newset->e[0].p), (char *)&(set->e[0].p), SETelemsize *(size+1));
  return (newset);
} /* setcopy */


/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setdel">-</a>
  
  qh_setdel( set, oldelem )
    delete oldelem from an unsorted set

  returns:
    returns oldelem if found
    returns NULL otherwise
    
  notes:
    set may be NULL
    oldelem must not be NULL;
    only deletes one copy of oldelem in set
     
  design:
    locate oldelem
    update actual size if it was full
    move the last element to the oldelem's location
*/
void *qh_setdel(setT *set, void *oldelem) {
  void **elemp, **lastp;
  int *sizep;

  if (!set)
    return NULL;
  elemp= SETaddr_(set, void);
  while (*elemp != oldelem && *elemp)
    elemp++;
  if (*elemp) {
    sizep= SETsizeaddr_(set);
    if (!(*sizep)--)         /*  if was a full set */
      *sizep= set->maxsize;  /*     *sizep= (maxsize-1)+ 1 */
    lastp= SETelemaddr_(set, *sizep-1, void);
    *elemp= *lastp;      /* may overwrite itself */
    *lastp= NULL;
    return oldelem;
  }
  return NULL;
} /* setdel */


/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setdellast">-</a>
  
  qh_setdellast( set) 
    return last element of set or NULL

  notes:
    deletes element from set
    set may be NULL

  design:
    return NULL if empty
    if full set
      delete last element and set actual size
    else
      delete last element and update actual size 
*/
void *qh_setdellast(setT *set) {
  int setsize;  /* actually, actual_size + 1 */
  int maxsize;
  int *sizep;
  void *returnvalue;
  
  if (!set || !(set->e[0].p))
    return NULL;
  sizep= SETsizeaddr_(set);
  if ((setsize= *sizep)) {
    returnvalue= set->e[setsize - 2].p;
    set->e[setsize - 2].p= NULL;
    (*sizep)--;
  }else {
    maxsize= set->maxsize;
    returnvalue= set->e[maxsize - 1].p;
    set->e[maxsize - 1].p= NULL;
    *sizep= maxsize;
  }
  return returnvalue;
} /* setdellast */


/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setdelnth">-</a>
  
  qh_setdelnth( set, nth )
    deletes nth element from unsorted set 
    0 is first element

  returns:
    returns the element (needs type conversion)

  notes:
    errors if nth invalid

  design:
    setup points and check nth
    delete nth element and overwrite with last element
*/
void *qh_setdelnth(setT *set, int nth) {
  void **elemp, **lastp, *elem;
  int *sizep;


  elemp= SETelemaddr_(set, nth, void);
  sizep= SETsizeaddr_(set);
  if (!(*sizep)--)         /*  if was a full set */
    *sizep= set->maxsize;  /*     *sizep= (maxsize-1)+ 1 */
  if (nth < 0 || nth >= *sizep) {
    fprintf (qhmem.ferr, "qhull internal error (qh_setaddnth): nth %d is out-of-bounds for set:\n", nth);
    qh_setprint (qhmem.ferr, "", set);
    qh_errexit (qhmem_ERRqhull, NULL, NULL);
  }
  lastp= SETelemaddr_(set, *sizep-1, void);
  elem= *elemp;
  *elemp= *lastp;      /* may overwrite itself */
  *lastp= NULL;
  return elem;
} /* setdelnth */

/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setdelnthsorted">-</a>
  
  qh_setdelnthsorted( set, nth )
    deletes nth element from sorted set

  returns:
    returns the element (use type conversion)
  
  notes:
    errors if nth invalid
    
  see also: 
    setnew_delnthsorted

  design:
    setup points and check nth
    copy remaining elements down one
    update actual size  
*/
void *qh_setdelnthsorted(setT *set, int nth) {
  void **newp, **oldp, *elem;
  int *sizep;

  sizep= SETsizeaddr_(set);
  if (nth < 0 || (*sizep && nth >= *sizep-1) || nth >= set->maxsize) {
    fprintf (qhmem.ferr, "qhull internal error (qh_setaddnth): nth %d is out-of-bounds for set:\n", nth);
    qh_setprint (qhmem.ferr, "", set);
    qh_errexit (qhmem_ERRqhull, NULL, NULL);
  }
  newp= SETelemaddr_(set, nth, void);
  elem= *newp;
  oldp= newp+1;
  while ((*(newp++)= *(oldp++)))
    ; /* copy remaining elements and NULL */
  if (!(*sizep)--)         /*  if was a full set */
    *sizep= set->maxsize;  /*     *sizep= (max size-1)+ 1 */
  return elem;
} /* setdelnthsorted */


/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setdelsorted">-</a>
  
  qh_setdelsorted( set, oldelem )
    deletes oldelem from sorted set

  returns:
    returns oldelem if it was deleted
  
  notes:
    set may be NULL

  design:
    locate oldelem in set
    copy remaining elements down one
    update actual size  
*/
void *qh_setdelsorted(setT *set, void *oldelem) {
  void **newp, **oldp;
  int *sizep;

  if (!set)
    return NULL;
  newp= SETaddr_(set, void);
  while(*newp != oldelem && *newp)
    newp++;
  if (*newp) {
    oldp= newp+1;
    while ((*(newp++)= *(oldp++)))
      ; /* copy remaining elements */
    sizep= SETsizeaddr_(set);
    if (!(*sizep)--)    /*  if was a full set */
      *sizep= set->maxsize;  /*     *sizep= (max size-1)+ 1 */
    return oldelem;
  }
  return NULL;
} /* setdelsorted */


/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setduplicate">-</a>
  
  qh_setduplicate( set, elemsize )
    duplicate a set of elemsize elements

  notes:
    use setcopy if retaining old elements

  design:
    create a new set
    for each elem of the old set
      create a newelem
      append newelem to newset
*/
setT *qh_setduplicate (setT *set, int elemsize) {
  void		*elem, **elemp, *newElem;
  setT		*newSet;
  int		size;
  
  if (!(size= qh_setsize (set)))
    return NULL;
  newSet= qh_setnew (size);
  FOREACHelem_(set) {
    newElem= qh_memalloc (elemsize);
    memcpy (newElem, elem, elemsize);
    qh_setappend (&newSet, newElem);
  }
  return newSet;
} /* setduplicate */


/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setequal">-</a>
  
  qh_setequal(  )
    returns 1 if two sorted sets are equal, otherwise returns 0

  notes:
    either set may be NULL

  design:
    check size of each set
    setup pointers
    compare elements of each set
*/
int qh_setequal(setT *setA, setT *setB) {
  void **elemAp, **elemBp;
  int sizeA, sizeB;
  
  SETreturnsize_(setA, sizeA);
  SETreturnsize_(setB, sizeB);
  if (sizeA != sizeB)
    return 0;
  if (!sizeA)
    return 1;
  elemAp= SETaddr_(setA, void);
  elemBp= SETaddr_(setB, void);
  if (!memcmp((char *)elemAp, (char *)elemBp, sizeA*SETelemsize))
    return 1;
  return 0;
} /* setequal */


/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setequal_except">-</a>
  
  qh_setequal_except( setA, skipelemA, setB, skipelemB )
    returns 1 if sorted setA and setB are equal except for skipelemA & B

  returns:
    false if either skipelemA or skipelemB are missing
  
  notes:
    neither set may be NULL

    if skipelemB is NULL, 
      can skip any one element of setB

  design:
    setup pointers
    search for skipelemA, skipelemB, and mismatches
    check results
*/
int qh_setequal_except (setT *setA, void *skipelemA, setT *setB, void *skipelemB) {
  void **elemA, **elemB;
  int skip=0;

  elemA= SETaddr_(setA, void);
  elemB= SETaddr_(setB, void);
  while (1) {
    if (*elemA == skipelemA) {
      skip++;
      elemA++;
    }
    if (skipelemB) {
      if (*elemB == skipelemB) {
        skip++;
        elemB++;
      }
    }else if (*elemA != *elemB) {
      skip++;
      if (!(skipelemB= *elemB++))
        return 0;
    }
    if (!*elemA)
      break;
    if (*elemA++ != *elemB++) 
      return 0;
  }
  if (skip != 2 || *elemB)
    return 0;
  return 1;
} /* setequal_except */
  

/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setequal_skip">-</a>
  
  qh_setequal_skip( setA, skipA, setB, skipB )
    returns 1 if sorted setA and setB are equal except for elements skipA & B

  returns:
    false if different size

  notes:
    neither set may be NULL

  design:
    setup pointers
    search for mismatches while skipping skipA and skipB
*/
int qh_setequal_skip (setT *setA, int skipA, setT *setB, int skipB) {
  void **elemA, **elemB, **skipAp, **skipBp;

  elemA= SETaddr_(setA, void);
  elemB= SETaddr_(setB, void);
  skipAp= SETelemaddr_(setA, skipA, void);
  skipBp= SETelemaddr_(setB, skipB, void);
  while (1) {
    if (elemA == skipAp)
      elemA++;
    if (elemB == skipBp)
      elemB++;
    if (!*elemA)
      break;
    if (*elemA++ != *elemB++) 
      return 0;
  }
  if (*elemB)
    return 0;
  return 1;
} /* setequal_skip */
  

/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setfree">-</a>
  
  qh_setfree( setp )
    frees the space occupied by a sorted or unsorted set

  returns:
    sets setp to NULL
    
  notes:
    set may be NULL

  design:
    free array
    free set
*/
void qh_setfree(setT **setp) {
  int size;
  void **freelistp;  /* used !qh_NOmem */
  
  if (*setp) {
    size= sizeof(setT) + ((*setp)->maxsize)*SETelemsize; 
    if (size <= qhmem.LASTsize) {
      qh_memfree_(*setp, size, freelistp);
    }else
      qh_memfree (*setp, size);
    *setp= NULL;
  }
} /* setfree */


/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setfree2">-</a>
  
  qh_setfree2( setp, elemsize )
    frees the space occupied by a set and its elements

  notes:
    set may be NULL

  design:
    free each element
    free set 
*/
void qh_setfree2 (setT **setp, int elemsize) {
  void		*elem, **elemp;
  
  FOREACHelem_(*setp)
    qh_memfree (elem, elemsize);
  qh_setfree (setp);
} /* setfree2 */


      
/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setfreelong">-</a>
  
  qh_setfreelong( setp )
    frees a set only if it's in long memory

  returns:
    sets setp to NULL if it is freed
    
  notes:
    set may be NULL

  design:
    if set is large
      free it    
*/
void qh_setfreelong(setT **setp) {
  int size;
  
  if (*setp) {
    size= sizeof(setT) + ((*setp)->maxsize)*SETelemsize; 
    if (size > qhmem.LASTsize) {
      qh_memfree (*setp, size);
      *setp= NULL;
    }
  }
} /* setfreelong */


/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setin">-</a>
  
  qh_setin( set, setelem )
    returns 1 if setelem is in a set, 0 otherwise

  notes:
    set may be NULL or unsorted

  design:
    scans set for setelem
*/
int qh_setin(setT *set, void *setelem) {
  void *elem, **elemp;

  FOREACHelem_(set) {
    if (elem == setelem)
      return 1;
  }
  return 0;
} /* setin */


/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setindex">-</a>
  
  qh_setindex( set, atelem )
    returns the index of atelem in set.   
    returns -1, if not in set or maxsize wrong

  notes:
    set may be NULL and may contain nulls.

  design:
    checks maxsize
    scans set for atelem
*/
int qh_setindex(setT *set, void *atelem) {
  void **elem;
  int size, i;

  SETreturnsize_(set, size);
  if (size > set->maxsize)
    return -1;
  elem= SETaddr_(set, void);
  for (i=0; i < size; i++) {
    if (*elem++ == atelem)
      return i;
  }
  return -1;
} /* setindex */


/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setlarger">-</a>
  
  qh_setlarger( oldsetp )
    returns a larger set that contains all elements of *oldsetp

  notes:
    the set is at least twice as large
    if temp set, updates qhmem.tempstack

  design:
    creates a new set
    copies the old set to the new set
    updates pointers in tempstack
    deletes the old set
*/
void qh_setlarger(setT **oldsetp) {
  int size= 1, *sizep;
  setT *newset, *set, **setp, *oldset;
  void **oldp, **newp;

  if (*oldsetp) {
    oldset= *oldsetp;
    SETreturnsize_(oldset, size);
    qhmem.cntlarger++;
    qhmem.totlarger += size+1;
    newset= qh_setnew(2 * size);
    oldp= SETaddr_(oldset, void);
    newp= SETaddr_(newset, void);
    memcpy((char *)newp, (char *)oldp, (size+1) * SETelemsize);
    sizep= SETsizeaddr_(newset);
    *sizep= size+1;
    FOREACHset_((setT *)qhmem.tempstack) {
      if (set == oldset)
	*(setp-1)= newset;
    }
    qh_setfree(oldsetp);
  }else 
    newset= qh_setnew(3);
  *oldsetp= newset;
} /* setlarger */


/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setlast">-</a>
  
  qh_setlast(  )
    return last element of set or NULL (use type conversion)

  notes:
    set may be NULL

  design:
    return last element  
*/
void *qh_setlast(setT *set) {
  int size;

  if (set) {
    size= *SETsizeaddr_(set);
    if (!size) 
      return SETelem_(set, set->maxsize - 1);
    else if (size > 1)
      return SETelem_(set, size - 2);
  }
  return NULL;
} /* setlast */


/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setnew">-</a>
  
  qh_setnew( setsize )
    creates and allocates space for a set

  notes:
    setsize means the number of elements (NOT including the NULL terminator)
    use qh_settemp/qh_setfreetemp if set is temporary

  design:
    allocate memory for set
    roundup memory if small set
    initialize as empty set
*/
setT *qh_setnew(int setsize) {
  setT *set;
  int sizereceived; /* used !qh_NOmem */
  int size;
  void **freelistp; /* used !qh_NOmem */

  if (!setsize)
    setsize++;
  size= sizeof(setT) + setsize * SETelemsize;
  if ((unsigned) size <= (unsigned) qhmem.LASTsize) {
    qh_memalloc_(size, freelistp, set, setT);
#ifndef qh_NOmem
    sizereceived= qhmem.sizetable[ qhmem.indextable[size]];
    if (sizereceived > size) 
      setsize += (sizereceived - size)/SETelemsize;
#endif
  }else
    set= (setT*)qh_memalloc (size);
  set->maxsize= setsize;
  set->e[setsize].i= 1;
  set->e[0].p= NULL;
  return (set);
} /* setnew */


/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setnew_delnthsorted">-</a>
  
  qh_setnew_delnthsorted( set, size, nth, prepend )
    creates a sorted set not containing nth element
    if prepend, the first prepend elements are undefined

  notes:
    set must be defined
    checks nth
    see also: setdelnthsorted

  design:
    create new set
    setup pointers and allocate room for prepend'ed entries
    append head of old set to new set
    append tail of old set to new set
*/
setT *qh_setnew_delnthsorted(setT *set, int size, int nth, int prepend) {
  setT *newset;
  void **oldp, **newp;
  int tailsize= size - nth -1, newsize;

  if (tailsize < 0) {
    fprintf (qhmem.ferr, "qhull internal error (qh_setaddnth): nth %d is out-of-bounds for set:\n", nth);
    qh_setprint (qhmem.ferr, "", set);
    qh_errexit (qhmem_ERRqhull, NULL, NULL);
  }
  newsize= size-1 + prepend;
  newset= qh_setnew(newsize);
  newset->e[newset->maxsize].i= newsize+1;  /* may be overwritten */
  oldp= SETaddr_(set, void);
  newp= SETaddr_(newset, void) + prepend;
  switch (nth) {
  case 0:
    break;
  case 1:
    *(newp++)= *oldp++;
    break;
  case 2:
    *(newp++)= *oldp++;
    *(newp++)= *oldp++;
    break;
  case 3:
    *(newp++)= *oldp++;
    *(newp++)= *oldp++;
    *(newp++)= *oldp++;
    break;
  case 4:
    *(newp++)= *oldp++;
    *(newp++)= *oldp++;
    *(newp++)= *oldp++;
    *(newp++)= *oldp++;
    break;
  default:
    memcpy((char *)newp, (char *)oldp, nth * SETelemsize);
    newp += nth;
    oldp += nth;
    break;
  }
  oldp++;
  switch (tailsize) {
  case 0:
    break;
  case 1:
    *(newp++)= *oldp++;
    break;
  case 2:
    *(newp++)= *oldp++;
    *(newp++)= *oldp++;
    break;
  case 3:
    *(newp++)= *oldp++;
    *(newp++)= *oldp++;
    *(newp++)= *oldp++;
    break;
  case 4:
    *(newp++)= *oldp++;
    *(newp++)= *oldp++;
    *(newp++)= *oldp++;
    *(newp++)= *oldp++;
    break;
  default:
    memcpy((char *)newp, (char *)oldp, tailsize * SETelemsize);
    newp += tailsize;
  }
  *newp= NULL;
  return(newset);
} /* setnew_delnthsorted */


/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setprint">-</a>
  
  qh_setprint( fp, string, set )
    print set elements to fp with identifying string

  notes:
    never errors
*/
void qh_setprint(FILE *fp, char* string, setT *set) {
  int size, k;

  if (!set)
    fprintf (fp, "%s set is null\n", string);
  else {
    SETreturnsize_(set, size);
    fprintf (fp, "%s set=%p maxsize=%d size=%d elems=",
	     string, set, set->maxsize, size);
    if (size > set->maxsize)
      size= set->maxsize+1;
    for (k=0; k < size; k++)
      fprintf(fp, " %p", set->e[k].p);
    fprintf(fp, "\n");
  }
} /* setprint */

/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setreplace">-</a>
  
  qh_setreplace( set, oldelem, newelem )
    replaces oldelem in set with newelem

  notes:
    errors if oldelem not in the set
    newelem may be NULL, but it turns the set into an indexed set (no FOREACH)

  design:
    find oldelem
    replace with newelem
*/
void qh_setreplace(setT *set, void *oldelem, void *newelem) {
  void **elemp;
  
  elemp= SETaddr_(set, void);
  while(*elemp != oldelem && *elemp)
    elemp++;
  if (*elemp)
    *elemp= newelem;
  else {
    fprintf (qhmem.ferr, "qhull internal error (qh_setreplace): elem %p not found in set\n",
       oldelem);
    qh_setprint (qhmem.ferr, "", set);
    qh_errexit (qhmem_ERRqhull, NULL, NULL);
  }
} /* setreplace */


/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setsize">-</a>
  
  qh_setsize( set )
    returns the size of a set

  notes:
    errors if set's maxsize is incorrect
    same as SETreturnsize_(set)

  design:
    determine actual size of set from maxsize
*/
int qh_setsize(setT *set) {
  int size, *sizep;
  
  if (!set)
    return (0);
  sizep= SETsizeaddr_(set);
  if ((size= *sizep)) {
    size--;
    if (size > set->maxsize) {
      fprintf (qhmem.ferr, "qhull internal error (qh_setsize): current set size %d is greater than maximum size %d\n",
	       size, set->maxsize);
      qh_setprint (qhmem.ferr, "set: ", set);
      qh_errexit (qhmem_ERRqhull, NULL, NULL);
    }
  }else
    size= set->maxsize;
  return size;
} /* setsize */

/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="settemp">-</a>
  
  qh_settemp( setsize )
    return a stacked, temporary set of upto setsize elements

  notes:
    use settempfree or settempfree_all to release from qhmem.tempstack
    see also qh_setnew

  design:
    allocate set
    append to qhmem.tempstack
    
*/
setT *qh_settemp(int setsize) {
  setT *newset;
  
  newset= qh_setnew (setsize);
  qh_setappend ((setT **)&qhmem.tempstack, newset);
  if (qhmem.IStracing >= 5)
    fprintf (qhmem.ferr, "qh_settemp: temp set %p of %d elements, depth %d\n",
       newset, newset->maxsize, qh_setsize ((setT*)qhmem.tempstack));
  return newset;
} /* settemp */

/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="settempfree">-</a>
  
  qh_settempfree( set )
    free temporary set at top of qhmem.tempstack

  notes:
    nop if set is NULL
    errors if set not from previous   qh_settemp
  
  to locate errors:
    use 'T2' to find source and then find mis-matching qh_settemp

  design:
    check top of qhmem.tempstack
    free it
*/
void qh_settempfree(setT **set) {
  setT *stackedset;

  if (!*set)
    return;
  stackedset= qh_settemppop ();
  if (stackedset != *set) {
    qh_settemppush(stackedset);
    fprintf (qhmem.ferr, "qhull internal error (qh_settempfree): set %p (size %d) was not last temporary allocated (depth %d, set %p, size %d)\n",
	     *set, qh_setsize(*set), qh_setsize((setT*)qhmem.tempstack)+1,
	     stackedset, qh_setsize(stackedset));
    qh_errexit (qhmem_ERRqhull, NULL, NULL);
  }
  qh_setfree (set);
} /* settempfree */

/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="settempfree_all">-</a>
  
  qh_settempfree_all(  )
    free all temporary sets in qhmem.tempstack

  design:
    for each set in tempstack
      free set
    free qhmem.tempstack
*/
void qh_settempfree_all(void) {
  setT *set, **setp;

  FOREACHset_((setT *)qhmem.tempstack) 
    qh_setfree(&set);
  qh_setfree((setT **)&qhmem.tempstack);
} /* settempfree_all */

/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="settemppop">-</a>
  
  qh_settemppop(  )
    pop and return temporary set from qhmem.tempstack 

  notes:
    the returned set is permanent
    
  design:
    pop and check top of qhmem.tempstack
*/
setT *qh_settemppop(void) {
  setT *stackedset;
  
  stackedset= (setT*)qh_setdellast((setT *)qhmem.tempstack);
  if (!stackedset) {
    fprintf (qhmem.ferr, "qhull internal error (qh_settemppop): pop from empty temporary stack\n");
    qh_errexit (qhmem_ERRqhull, NULL, NULL);
  }
  if (qhmem.IStracing >= 5)
    fprintf (qhmem.ferr, "qh_settemppop: depth %d temp set %p of %d elements\n",
       qh_setsize((setT*)qhmem.tempstack)+1, stackedset, qh_setsize(stackedset));
  return stackedset;
} /* settemppop */

/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="settemppush">-</a>
  
  qh_settemppush( set )
    push temporary set unto qhmem.tempstack (makes it temporary)

  notes:
    duplicates settemp() for tracing

  design:
    append set to tempstack  
*/
void qh_settemppush(setT *set) {
  
  qh_setappend ((setT**)&qhmem.tempstack, set);
  if (qhmem.IStracing >= 5)
    fprintf (qhmem.ferr, "qh_settemppush: depth %d temp set %p of %d elements\n",
    qh_setsize((setT*)qhmem.tempstack), set, qh_setsize(set));
} /* settemppush */

 
/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="settruncate">-</a>
  
  qh_settruncate( set, size )
    truncate set to size elements

  notes:
    set must be defined
  
  see:
    SETtruncate_

  design:
    check size
    update actual size of set
*/
void qh_settruncate (setT *set, int size) {

  if (size < 0 || size > set->maxsize) {
    fprintf (qhmem.ferr, "qhull internal error (qh_settruncate): size %d out of bounds for set:\n", size);
    qh_setprint (qhmem.ferr, "", set);
    qh_errexit (qhmem_ERRqhull, NULL, NULL);
  }
  set->e[set->maxsize].i= size+1;   /* maybe overwritten */
  set->e[size].p= NULL;
} /* settruncate */
    
/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setunique">-</a>
  
  qh_setunique( set, elem )
    add elem to unsorted set unless it is already in set

  notes:
    returns 1 if it is appended

  design:
    if elem not in set
      append elem to set
*/
int qh_setunique (setT **set, void *elem) {

  if (!qh_setin (*set, elem)) {
    qh_setappend (set, elem);
    return 1;
  }
  return 0;
} /* setunique */
    
/*-<a                             href="qh-set.htm#TOC"
  >-------------------------------<a name="setzero">-</a>
  
  qh_setzero( set, index, size )
    zero elements from index on
    set actual size of set to size

  notes:
    set must be defined
    the set becomes an indexed set (can not use FOREACH...)
  
  see also:
    qh_settruncate
    
  design:
    check index and size
    update actual size
    zero elements starting at e[index]   
*/
void qh_setzero (setT *set, int index, int size) {
  int count;

  if (index < 0 || index >= size || size > set->maxsize) {
    fprintf (qhmem.ferr, "qhull internal error (qh_setzero): index %d or size %d out of bounds for set:\n", index, size);
    qh_setprint (qhmem.ferr, "", set);
    qh_errexit (qhmem_ERRqhull, NULL, NULL);
  }
  set->e[set->maxsize].i=  size+1;  /* may be overwritten */
  count= size - index + 1;   /* +1 for NULL terminator */
  memset ((char *)SETelemaddr_(set, index, void), 0, count * SETelemsize);
} /* setzero */

    

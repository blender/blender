/** \file opennl/superlu/mmd.c
 *  \ingroup opennl
 */

typedef int shortint;


/* prototypes -------------------- */
int  genmmd_(int *, int *, int *, int *, int *, int *, int *, 
					int *, int *, int *, int *, int *);
int mmdint_(int *, int *, shortint *, shortint *, shortint *, shortint *, shortint *, 
			shortint *, shortint *);
int mmdelm_(int *, int *, shortint *, shortint *, shortint *, shortint *, shortint *, 
			shortint *, shortint *, int *, int *);
int mmdupd_(int *, int *, int *, shortint *, int *, int *, shortint *, 
			shortint *, shortint *, shortint *, shortint *, shortint *, int *, int *);
int mmdnum_(int *, shortint *, shortint *, shortint *);


/* *************************************************************** */
/* *************************************************************** */
/* ****     GENMMD ..... MULTIPLE MINIMUM EXTERNAL DEGREE     **** */
/* *************************************************************** */
/* *************************************************************** */

/*     AUTHOR - JOSEPH W.H. LIU */
/*              DEPT OF COMPUTER SCIENCE, YORK UNIVERSITY. */

/*     PURPOSE - THIS ROUTINE IMPLEMENTS THE MINIMUM DEGREE */
/*        ALGORITHM.  IT MAKES USE OF THE IMPLICIT REPRESENTATION */
/*        OF ELIMINATION GRAPHS BY QUOTIENT GRAPHS, AND THE */
/*        NOTION OF INDISTINGUISHABLE NODES.  IT ALSO IMPLEMENTS */
/*        THE MODIFICATIONS BY MULTIPLE ELIMINATION AND MINIMUM */
/*        EXTERNAL DEGREE. */
/*        --------------------------------------------- */
/*        CAUTION - THE ADJACENCY VECTOR ADJNCY WILL BE */
/*        DESTROYED. */
/*        --------------------------------------------- */

/*     INPUT PARAMETERS - */
/*        NEQNS  - NUMBER OF EQUATIONS. */
/*        (XADJ,ADJNCY) - THE ADJACENCY STRUCTURE. */
/*        DELTA  - TOLERANCE VALUE FOR MULTIPLE ELIMINATION. */
/*        MAXINT - MAXIMUM MACHINE REPRESENTABLE (SHORT) INTEGER */
/*                 (ANY SMALLER ESTIMATE WILL DO) FOR MARKING */
/*                 NODES. */

/*     OUTPUT PARAMETERS - */
/*        PERM   - THE MINIMUM DEGREE ORDERING. */
/*        INVP   - THE INVERSE OF PERM. */
/*        NOFSUB - AN UPPER BOUND ON THE NUMBER OF NONZERO */
/*                 SUBSCRIPTS FOR THE COMPRESSED STORAGE SCHEME. */

/*     WORKING PARAMETERS - */
/*        DHEAD  - VECTOR FOR HEAD OF DEGREE LISTS. */
/*        INVP   - USED TEMPORARILY FOR DEGREE FORWARD LINK. */
/*        PERM   - USED TEMPORARILY FOR DEGREE BACKWARD LINK. */
/*        QSIZE  - VECTOR FOR SIZE OF SUPERNODES. */
/*        LLIST  - VECTOR FOR TEMPORARY LINKED LISTS. */
/*        MARKER - A TEMPORARY MARKER VECTOR. */

/*     PROGRAM SUBROUTINES - */
/*        MMDELM, MMDINT, MMDNUM, MMDUPD. */

/* *************************************************************** */

/* Subroutine */ int genmmd_(int *neqns, int *xadj, shortint *adjncy, 
	shortint *invp, shortint *perm, int *delta, shortint *dhead, 
	shortint *qsize, shortint *llist, shortint *marker, int *maxint, 
	int *nofsub)
{
    /* System generated locals */
    int i__1;

    /* Local variables */
    static int mdeg, ehead, i, mdlmt, mdnode;
    extern /* Subroutine */ int mmdelm_(int *, int *, shortint *, 
	    shortint *, shortint *, shortint *, shortint *, shortint *, 
	    shortint *, int *, int *), mmdupd_(int *, int *, 
	    int *, shortint *, int *, int *, shortint *, shortint 
	    *, shortint *, shortint *, shortint *, shortint *, int *, 
	    int *), mmdint_(int *, int *, shortint *, shortint *, 
	    shortint *, shortint *, shortint *, shortint *, shortint *), 
	    mmdnum_(int *, shortint *, shortint *, shortint *);
    static int nextmd, tag, num;


/* *************************************************************** */


/* *************************************************************** */

    /* Parameter adjustments */
    --marker;
    --llist;
    --qsize;
    --dhead;
    --perm;
    --invp;
    --adjncy;
    --xadj;

    /* Function Body */
    if (*neqns <= 0) {
	return 0;
    }

/*        ------------------------------------------------ */
/*        INITIALIZATION FOR THE MINIMUM DEGREE ALGORITHM. */
/*        ------------------------------------------------ */
    *nofsub = 0;
    mmdint_(neqns, &xadj[1], &adjncy[1], &dhead[1], &invp[1], &perm[1], &
	    qsize[1], &llist[1], &marker[1]);

/*        ---------------------------------------------- */
/*        NUM COUNTS THE NUMBER OF ORDERED NODES PLUS 1. */
/*        ---------------------------------------------- */
    num = 1;

/*        ----------------------------- */
/*        ELIMINATE ALL ISOLATED NODES. */
/*        ----------------------------- */
    nextmd = dhead[1];
L100:
    if (nextmd <= 0) {
	goto L200;
    }
    mdnode = nextmd;
    nextmd = invp[mdnode];
    marker[mdnode] = *maxint;
    invp[mdnode] = -num;
    ++num;
    goto L100;

L200:
/*        ---------------------------------------- */
/*        SEARCH FOR NODE OF THE MINIMUM DEGREE. */
/*        MDEG IS THE CURRENT MINIMUM DEGREE; */
/*        TAG IS USED TO FACILITATE MARKING NODES. */
/*        ---------------------------------------- */
    if (num > *neqns) {
	goto L1000;
    }
    tag = 1;
    dhead[1] = 0;
    mdeg = 2;
L300:
    if (dhead[mdeg] > 0) {
	goto L400;
    }
    ++mdeg;
    goto L300;
L400:
/*            ------------------------------------------------- */
/*            USE VALUE OF DELTA TO SET UP MDLMT, WHICH GOVERNS */
/*            WHEN A DEGREE UPDATE IS TO BE PERFORMED. */
/*            ------------------------------------------------- */
    mdlmt = mdeg + *delta;
    ehead = 0;

L500:
    mdnode = dhead[mdeg];
    if (mdnode > 0) {
	goto L600;
    }
    ++mdeg;
    if (mdeg > mdlmt) {
	goto L900;
    }
    goto L500;
L600:
/*                ---------------------------------------- */
/*                REMOVE MDNODE FROM THE DEGREE STRUCTURE. */
/*                ---------------------------------------- */
    nextmd = invp[mdnode];
    dhead[mdeg] = nextmd;
    if (nextmd > 0) {
	perm[nextmd] = -mdeg;
    }
    invp[mdnode] = -num;
    *nofsub = *nofsub + mdeg + qsize[mdnode] - 2;
    if (num + qsize[mdnode] > *neqns) {
	goto L1000;
    }
/*                ---------------------------------------------- */
/*                ELIMINATE MDNODE AND PERFORM QUOTIENT GRAPH */
/*                TRANSFORMATION.  RESET TAG VALUE IF NECESSARY. */
/*                ---------------------------------------------- */
    ++tag;
    if (tag < *maxint) {
	goto L800;
    }
    tag = 1;
    i__1 = *neqns;
    for (i = 1; i <= i__1; ++i) {
	if (marker[i] < *maxint) {
	    marker[i] = 0;
	}
/* L700: */
    }
L800:
    mmdelm_(&mdnode, &xadj[1], &adjncy[1], &dhead[1], &invp[1], &perm[1], &
	    qsize[1], &llist[1], &marker[1], maxint, &tag);
    num += qsize[mdnode];
    llist[mdnode] = ehead;
    ehead = mdnode;
    if (*delta >= 0) {
	goto L500;
    }
L900:
/*            ------------------------------------------- */
/*            UPDATE DEGREES OF THE NODES INVOLVED IN THE */
/*            MINIMUM DEGREE NODES ELIMINATION. */
/*            ------------------------------------------- */
    if (num > *neqns) {
	goto L1000;
    }
    mmdupd_(&ehead, neqns, &xadj[1], &adjncy[1], delta, &mdeg, &dhead[1], &
	    invp[1], &perm[1], &qsize[1], &llist[1], &marker[1], maxint, &tag)
	    ;
    goto L300;

L1000:
    mmdnum_(neqns, &perm[1], &invp[1], &qsize[1]);
    return 0;

} /* genmmd_ */

/* *************************************************************** */
/* *************************************************************** */
/* ***     MMDINT ..... MULT MINIMUM DEGREE INITIALIZATION     *** */
/* *************************************************************** */
/* *************************************************************** */

/*     AUTHOR - JOSEPH W.H. LIU */
/*              DEPT OF COMPUTER SCIENCE, YORK UNIVERSITY. */

/*     PURPOSE - THIS ROUTINE PERFORMS INITIALIZATION FOR THE */
/*        MULTIPLE ELIMINATION VERSION OF THE MINIMUM DEGREE */
/*        ALGORITHM. */

/*     INPUT PARAMETERS - */
/*        NEQNS  - NUMBER OF EQUATIONS. */
/*        (XADJ,ADJNCY) - ADJACENCY STRUCTURE. */

/*     OUTPUT PARAMETERS - */
/*        (DHEAD,DFORW,DBAKW) - DEGREE DOUBLY LINKED STRUCTURE. */
/*        QSIZE  - SIZE OF SUPERNODE (INITIALIZED TO ONE). */
/*        LLIST  - LINKED LIST. */
/*        MARKER - MARKER VECTOR. */

/* *************************************************************** */

/* Subroutine */ int mmdint_(int *neqns, int *xadj, shortint *adjncy, 
	shortint *dhead, shortint *dforw, shortint *dbakw, shortint *qsize, 
	shortint *llist, shortint *marker)
{
    /* System generated locals */
    int i__1;

    /* Local variables */
    static int ndeg, node, fnode;


/* *************************************************************** */


/* *************************************************************** */

    /* Parameter adjustments */
    --marker;
    --llist;
    --qsize;
    --dbakw;
    --dforw;
    --dhead;
    --adjncy;
    --xadj;

    /* Function Body */
    i__1 = *neqns;
    for (node = 1; node <= i__1; ++node) {
	dhead[node] = 0;
	qsize[node] = 1;
	marker[node] = 0;
	llist[node] = 0;
/* L100: */
    }
/*        ------------------------------------------ */
/*        INITIALIZE THE DEGREE DOUBLY LINKED LISTS. */
/*        ------------------------------------------ */
    i__1 = *neqns;
    for (node = 1; node <= i__1; ++node) {
	ndeg = xadj[node + 1] - xadj[node] + 1;
	fnode = dhead[ndeg];
	dforw[node] = fnode;
	dhead[ndeg] = node;
	if (fnode > 0) {
	    dbakw[fnode] = node;
	}
	dbakw[node] = -ndeg;
/* L200: */
    }
    return 0;

} /* mmdint_ */

/* *************************************************************** */
/* *************************************************************** */
/* **     MMDELM ..... MULTIPLE MINIMUM DEGREE ELIMINATION     *** */
/* *************************************************************** */
/* *************************************************************** */

/*     AUTHOR - JOSEPH W.H. LIU */
/*              DEPT OF COMPUTER SCIENCE, YORK UNIVERSITY. */

/*     PURPOSE - THIS ROUTINE ELIMINATES THE NODE MDNODE OF */
/*        MINIMUM DEGREE FROM THE ADJACENCY STRUCTURE, WHICH */
/*        IS STORED IN THE QUOTIENT GRAPH FORMAT.  IT ALSO */
/*        TRANSFORMS THE QUOTIENT GRAPH REPRESENTATION OF THE */
/*        ELIMINATION GRAPH. */

/*     INPUT PARAMETERS - */
/*        MDNODE - NODE OF MINIMUM DEGREE. */
/*        MAXINT - ESTIMATE OF MAXIMUM REPRESENTABLE (SHORT) */
/*                 INT. */
/*        TAG    - TAG VALUE. */

/*     UPDATED PARAMETERS - */
/*        (XADJ,ADJNCY) - UPDATED ADJACENCY STRUCTURE. */
/*        (DHEAD,DFORW,DBAKW) - DEGREE DOUBLY LINKED STRUCTURE. */
/*        QSIZE  - SIZE OF SUPERNODE. */
/*        MARKER - MARKER VECTOR. */
/*        LLIST  - TEMPORARY LINKED LIST OF ELIMINATED NABORS. */

/* *************************************************************** */

/* Subroutine */ int mmdelm_(int *mdnode, int *xadj, shortint *adjncy,
	 shortint *dhead, shortint *dforw, shortint *dbakw, shortint *qsize, 
	shortint *llist, shortint *marker, int *maxint, int *tag)
{
    /* System generated locals */
    int i__1, i__2;

    /* Local variables */
    static int node, link, rloc, rlmt, i, j, nabor, rnode, elmnt, xqnbr, 
	    istop, jstop, istrt, jstrt, nxnode, pvnode, nqnbrs, npv;


/* *************************************************************** */


/* *************************************************************** */

/*        ----------------------------------------------- */
/*        FIND REACHABLE SET AND PLACE IN DATA STRUCTURE. */
/*        ----------------------------------------------- */
    /* Parameter adjustments */
    --marker;
    --llist;
    --qsize;
    --dbakw;
    --dforw;
    --dhead;
    --adjncy;
    --xadj;

    /* Function Body */
    marker[*mdnode] = *tag;
    istrt = xadj[*mdnode];
    istop = xadj[*mdnode + 1] - 1;
/*        ------------------------------------------------------- */
/*        ELMNT POINTS TO THE BEGINNING OF THE LIST OF ELIMINATED */
/*        NABORS OF MDNODE, AND RLOC GIVES THE STORAGE LOCATION */
/*        FOR THE NEXT REACHABLE NODE. */
/*        ------------------------------------------------------- */
    elmnt = 0;
    rloc = istrt;
    rlmt = istop;
    i__1 = istop;
    for (i = istrt; i <= i__1; ++i) {
	nabor = adjncy[i];
	if (nabor == 0) {
	    goto L300;
	}
	if (marker[nabor] >= *tag) {
	    goto L200;
	}
	marker[nabor] = *tag;
	if (dforw[nabor] < 0) {
	    goto L100;
	}
	adjncy[rloc] = nabor;
	++rloc;
	goto L200;
L100:
	llist[nabor] = elmnt;
	elmnt = nabor;
L200:
	;
    }
L300:
/*            ----------------------------------------------------- */
/*            MERGE WITH REACHABLE NODES FROM GENERALIZED ELEMENTS. */
/*            ----------------------------------------------------- */
    if (elmnt <= 0) {
	goto L1000;
    }
    adjncy[rlmt] = -elmnt;
    link = elmnt;
L400:
    jstrt = xadj[link];
    jstop = xadj[link + 1] - 1;
    i__1 = jstop;
    for (j = jstrt; j <= i__1; ++j) {
	node = adjncy[j];
	link = -node;
	if (node < 0) {
	    goto L400;
	} else if (node == 0) {
	    goto L900;
	} else {
	    goto L500;
	}
L500:
	if (marker[node] >= *tag || dforw[node] < 0) {
	    goto L800;
	}
	marker[node] = *tag;
/*                            --------------------------------- */
/*                            USE STORAGE FROM ELIMINATED NODES */
/*                            IF NECESSARY. */
/*                            --------------------------------- */
L600:
	if (rloc < rlmt) {
	    goto L700;
	}
	link = -adjncy[rlmt];
	rloc = xadj[link];
	rlmt = xadj[link + 1] - 1;
	goto L600;
L700:
	adjncy[rloc] = node;
	++rloc;
L800:
	;
    }
L900:
    elmnt = llist[elmnt];
    goto L300;
L1000:
    if (rloc <= rlmt) {
	adjncy[rloc] = 0;
    }
/*        -------------------------------------------------------- */
/*        FOR EACH NODE IN THE REACHABLE SET, DO THE FOLLOWING ... */
/*        -------------------------------------------------------- */
    link = *mdnode;
L1100:
    istrt = xadj[link];
    istop = xadj[link + 1] - 1;
    i__1 = istop;
    for (i = istrt; i <= i__1; ++i) {
	rnode = adjncy[i];
	link = -rnode;
	if (rnode < 0) {
	    goto L1100;
	} else if (rnode == 0) {
	    goto L1800;
	} else {
	    goto L1200;
	}
L1200:
/*                -------------------------------------------- */
/*                IF RNODE IS IN THE DEGREE LIST STRUCTURE ... */
/*                -------------------------------------------- */
	pvnode = dbakw[rnode];
	if (pvnode == 0 || pvnode == -(*maxint)) {
	    goto L1300;
	}
/*                    ------------------------------------- */
/*                    THEN REMOVE RNODE FROM THE STRUCTURE. */
/*                    ------------------------------------- */
	nxnode = dforw[rnode];
	if (nxnode > 0) {
	    dbakw[nxnode] = pvnode;
	}
	if (pvnode > 0) {
	    dforw[pvnode] = nxnode;
	}
	npv = -pvnode;
	if (pvnode < 0) {
	    dhead[npv] = nxnode;
	}
L1300:
/*                ---------------------------------------- */
/*                PURGE INACTIVE QUOTIENT NABORS OF RNODE. */
/*                ---------------------------------------- */
	jstrt = xadj[rnode];
	jstop = xadj[rnode + 1] - 1;
	xqnbr = jstrt;
	i__2 = jstop;
	for (j = jstrt; j <= i__2; ++j) {
	    nabor = adjncy[j];
	    if (nabor == 0) {
		goto L1500;
	    }
	    if (marker[nabor] >= *tag) {
		goto L1400;
	    }
	    adjncy[xqnbr] = nabor;
	    ++xqnbr;
L1400:
	    ;
	}
L1500:
/*                ---------------------------------------- */
/*                IF NO ACTIVE NABOR AFTER THE PURGING ... */
/*                ---------------------------------------- */
	nqnbrs = xqnbr - jstrt;
	if (nqnbrs > 0) {
	    goto L1600;
	}
/*                    ----------------------------- */
/*                    THEN MERGE RNODE WITH MDNODE. */
/*                    ----------------------------- */
	qsize[*mdnode] += qsize[rnode];
	qsize[rnode] = 0;
	marker[rnode] = *maxint;
	dforw[rnode] = -(*mdnode);
	dbakw[rnode] = -(*maxint);
	goto L1700;
L1600:
/*                -------------------------------------- */
/*                ELSE FLAG RNODE FOR DEGREE UPDATE, AND */
/*                ADD MDNODE AS A NABOR OF RNODE. */
/*                -------------------------------------- */
	dforw[rnode] = nqnbrs + 1;
	dbakw[rnode] = 0;
	adjncy[xqnbr] = *mdnode;
	++xqnbr;
	if (xqnbr <= jstop) {
	    adjncy[xqnbr] = 0;
	}

L1700:
	;
    }
L1800:
    return 0;

} /* mmdelm_ */

/* *************************************************************** */
/* *************************************************************** */
/* *****     MMDUPD ..... MULTIPLE MINIMUM DEGREE UPDATE     ***** */
/* *************************************************************** */
/* *************************************************************** */

/*     AUTHOR - JOSEPH W.H. LIU */
/*              DEPT OF COMPUTER SCIENCE, YORK UNIVERSITY. */

/*     PURPOSE - THIS ROUTINE UPDATES THE DEGREES OF NODES */
/*        AFTER A MULTIPLE ELIMINATION STEP. */

/*     INPUT PARAMETERS - */
/*        EHEAD  - THE BEGINNING OF THE LIST OF ELIMINATED */
/*                 NODES (I.E., NEWLY FORMED ELEMENTS). */
/*        NEQNS  - NUMBER OF EQUATIONS. */
/*        (XADJ,ADJNCY) - ADJACENCY STRUCTURE. */
/*        DELTA  - TOLERANCE VALUE FOR MULTIPLE ELIMINATION. */
/*        MAXINT - MAXIMUM MACHINE REPRESENTABLE (SHORT) */
/*                 INTEGER. */

/*     UPDATED PARAMETERS - */
/*        MDEG   - NEW MINIMUM DEGREE AFTER DEGREE UPDATE. */
/*        (DHEAD,DFORW,DBAKW) - DEGREE DOUBLY LINKED STRUCTURE. */
/*        QSIZE  - SIZE OF SUPERNODE. */
/*        LLIST  - WORKING LINKED LIST. */
/*        MARKER - MARKER VECTOR FOR DEGREE UPDATE. */
/*        TAG    - TAG VALUE. */

/* *************************************************************** */

/* Subroutine */ int mmdupd_(int *ehead, int *neqns, int *xadj, 
	shortint *adjncy, int *delta, int *mdeg, shortint *dhead, 
	shortint *dforw, shortint *dbakw, shortint *qsize, shortint *llist, 
	shortint *marker, int *maxint, int *tag)
{
    /* System generated locals */
    int i__1, i__2;

    /* Local variables */
    static int node, mtag, link, mdeg0, i, j, enode, fnode, nabor, elmnt, 
	    istop, jstop, q2head, istrt, jstrt, qxhead, iq2, deg, deg0;


/* *************************************************************** */


/* *************************************************************** */

    /* Parameter adjustments */
    --marker;
    --llist;
    --qsize;
    --dbakw;
    --dforw;
    --dhead;
    --adjncy;
    --xadj;

    /* Function Body */
    mdeg0 = *mdeg + *delta;
    elmnt = *ehead;
L100:
/*            ------------------------------------------------------- */
/*            FOR EACH OF THE NEWLY FORMED ELEMENT, DO THE FOLLOWING. */
/*            (RESET TAG VALUE IF NECESSARY.) */
/*            ------------------------------------------------------- */
    if (elmnt <= 0) {
	return 0;
    }
    mtag = *tag + mdeg0;
    if (mtag < *maxint) {
	goto L300;
    }
    *tag = 1;
    i__1 = *neqns;
    for (i = 1; i <= i__1; ++i) {
	if (marker[i] < *maxint) {
	    marker[i] = 0;
	}
/* L200: */
    }
    mtag = *tag + mdeg0;
L300:
/*            --------------------------------------------- */
/*            CREATE TWO LINKED LISTS FROM NODES ASSOCIATED */
/*            WITH ELMNT: ONE WITH TWO NABORS (Q2HEAD) IN */
/*            ADJACENCY STRUCTURE, AND THE OTHER WITH MORE */
/*            THAN TWO NABORS (QXHEAD).  ALSO COMPUTE DEG0, */
/*            NUMBER OF NODES IN THIS ELEMENT. */
/*            --------------------------------------------- */
    q2head = 0;
    qxhead = 0;
    deg0 = 0;
    link = elmnt;
L400:
    istrt = xadj[link];
    istop = xadj[link + 1] - 1;
    i__1 = istop;
    for (i = istrt; i <= i__1; ++i) {
	enode = adjncy[i];
	link = -enode;
	if (enode < 0) {
	    goto L400;
	} else if (enode == 0) {
	    goto L800;
	} else {
	    goto L500;
	}

L500:
	if (qsize[enode] == 0) {
	    goto L700;
	}
	deg0 += qsize[enode];
	marker[enode] = mtag;
/*                        ---------------------------------- */
/*                        IF ENODE REQUIRES A DEGREE UPDATE, */
/*                        THEN DO THE FOLLOWING. */
/*                        ---------------------------------- */
	if (dbakw[enode] != 0) {
	    goto L700;
	}
/*                            --------------------------------------- 
*/
/*                            PLACE EITHER IN QXHEAD OR Q2HEAD LISTS. 
*/
/*                            --------------------------------------- 
*/
	if (dforw[enode] == 2) {
	    goto L600;
	}
	llist[enode] = qxhead;
	qxhead = enode;
	goto L700;
L600:
	llist[enode] = q2head;
	q2head = enode;
L700:
	;
    }
L800:
/*            -------------------------------------------- */
/*            FOR EACH ENODE IN Q2 LIST, DO THE FOLLOWING. */
/*            -------------------------------------------- */
    enode = q2head;
    iq2 = 1;
L900:
    if (enode <= 0) {
	goto L1500;
    }
    if (dbakw[enode] != 0) {
	goto L2200;
    }
    ++(*tag);
    deg = deg0;
/*                    ------------------------------------------ */
/*                    IDENTIFY THE OTHER ADJACENT ELEMENT NABOR. */
/*                    ------------------------------------------ */
    istrt = xadj[enode];
    nabor = adjncy[istrt];
    if (nabor == elmnt) {
	nabor = adjncy[istrt + 1];
    }
/*                    ------------------------------------------------ */
/*                    IF NABOR IS UNELIMINATED, INCREASE DEGREE COUNT. */
/*                    ------------------------------------------------ */
    link = nabor;
    if (dforw[nabor] < 0) {
	goto L1000;
    }
    deg += qsize[nabor];
    goto L2100;
L1000:
/*                        -------------------------------------------- */
/*                        OTHERWISE, FOR EACH NODE IN THE 2ND ELEMENT, */
/*                        DO THE FOLLOWING. */
/*                        -------------------------------------------- */
    istrt = xadj[link];
    istop = xadj[link + 1] - 1;
    i__1 = istop;
    for (i = istrt; i <= i__1; ++i) {
	node = adjncy[i];
	link = -node;
	if (node == enode) {
	    goto L1400;
	}
	if (node < 0) {
	    goto L1000;
	} else if (node == 0) {
	    goto L2100;
	} else {
	    goto L1100;
	}

L1100:
	if (qsize[node] == 0) {
	    goto L1400;
	}
	if (marker[node] >= *tag) {
	    goto L1200;
	}
/*                                -----------------------------------
-- */
/*                                CASE WHEN NODE IS NOT YET CONSIDERED
. */
/*                                -----------------------------------
-- */
	marker[node] = *tag;
	deg += qsize[node];
	goto L1400;
L1200:
/*                            ----------------------------------------
 */
/*                            CASE WHEN NODE IS INDISTINGUISHABLE FROM
 */
/*                            ENODE.  MERGE THEM INTO A NEW SUPERNODE.
 */
/*                            ----------------------------------------
 */
	if (dbakw[node] != 0) {
	    goto L1400;
	}
	if (dforw[node] != 2) {
	    goto L1300;
	}
	qsize[enode] += qsize[node];
	qsize[node] = 0;
	marker[node] = *maxint;
	dforw[node] = -enode;
	dbakw[node] = -(*maxint);
	goto L1400;
L1300:
/*                            -------------------------------------- 
*/
/*                            CASE WHEN NODE IS OUTMATCHED BY ENODE. 
*/
/*                            -------------------------------------- 
*/
	if (dbakw[node] == 0) {
	    dbakw[node] = -(*maxint);
	}
L1400:
	;
    }
    goto L2100;
L1500:
/*                ------------------------------------------------ */
/*                FOR EACH ENODE IN THE QX LIST, DO THE FOLLOWING. */
/*                ------------------------------------------------ */
    enode = qxhead;
    iq2 = 0;
L1600:
    if (enode <= 0) {
	goto L2300;
    }
    if (dbakw[enode] != 0) {
	goto L2200;
    }
    ++(*tag);
    deg = deg0;
/*                        --------------------------------- */
/*                        FOR EACH UNMARKED NABOR OF ENODE, */
/*                        DO THE FOLLOWING. */
/*                        --------------------------------- */
    istrt = xadj[enode];
    istop = xadj[enode + 1] - 1;
    i__1 = istop;
    for (i = istrt; i <= i__1; ++i) {
	nabor = adjncy[i];
	if (nabor == 0) {
	    goto L2100;
	}
	if (marker[nabor] >= *tag) {
	    goto L2000;
	}
	marker[nabor] = *tag;
	link = nabor;
/*                                ------------------------------ */
/*                                IF UNELIMINATED, INCLUDE IT IN */
/*                                DEG COUNT. */
/*                                ------------------------------ */
	if (dforw[nabor] < 0) {
	    goto L1700;
	}
	deg += qsize[nabor];
	goto L2000;
L1700:
/*                                    ------------------------------- 
*/
/*                                    IF ELIMINATED, INCLUDE UNMARKED 
*/
/*                                    NODES IN THIS ELEMENT INTO THE 
*/
/*                                    DEGREE COUNT. */
/*                                    ------------------------------- 
*/
	jstrt = xadj[link];
	jstop = xadj[link + 1] - 1;
	i__2 = jstop;
	for (j = jstrt; j <= i__2; ++j) {
	    node = adjncy[j];
	    link = -node;
	    if (node < 0) {
		goto L1700;
	    } else if (node == 0) {
		goto L2000;
	    } else {
		goto L1800;
	    }

L1800:
	    if (marker[node] >= *tag) {
		goto L1900;
	    }
	    marker[node] = *tag;
	    deg += qsize[node];
L1900:
	    ;
	}
L2000:
	;
    }
L2100:
/*                    ------------------------------------------- */
/*                    UPDATE EXTERNAL DEGREE OF ENODE IN DEGREE */
/*                    STRUCTURE, AND MDEG (MIN DEG) IF NECESSARY. */
/*                    ------------------------------------------- */
    deg = deg - qsize[enode] + 1;
    fnode = dhead[deg];
    dforw[enode] = fnode;
    dbakw[enode] = -deg;
    if (fnode > 0) {
	dbakw[fnode] = enode;
    }
    dhead[deg] = enode;
    if (deg < *mdeg) {
	*mdeg = deg;
    }
L2200:
/*                    ---------------------------------- */
/*                    GET NEXT ENODE IN CURRENT ELEMENT. */
/*                    ---------------------------------- */
    enode = llist[enode];
    if (iq2 == 1) {
	goto L900;
    }
    goto L1600;
L2300:
/*            ----------------------------- */
/*            GET NEXT ELEMENT IN THE LIST. */
/*            ----------------------------- */
    *tag = mtag;
    elmnt = llist[elmnt];
    goto L100;

} /* mmdupd_ */

/* *************************************************************** */
/* *************************************************************** */
/* *****     MMDNUM ..... MULTI MINIMUM DEGREE NUMBERING     ***** */
/* *************************************************************** */
/* *************************************************************** */

/*     AUTHOR - JOSEPH W.H. LIU */
/*              DEPT OF COMPUTER SCIENCE, YORK UNIVERSITY. */

/*     PURPOSE - THIS ROUTINE PERFORMS THE FINAL STEP IN */
/*        PRODUCING THE PERMUTATION AND INVERSE PERMUTATION */
/*        VECTORS IN THE MULTIPLE ELIMINATION VERSION OF THE */
/*        MINIMUM DEGREE ORDERING ALGORITHM. */

/*     INPUT PARAMETERS - */
/*        NEQNS  - NUMBER OF EQUATIONS. */
/*        QSIZE  - SIZE OF SUPERNODES AT ELIMINATION. */

/*     UPDATED PARAMETERS - */
/*        INVP   - INVERSE PERMUTATION VECTOR.  ON INPUT, */
/*                 IF QSIZE(NODE)=0, THEN NODE HAS BEEN MERGED */
/*                 INTO THE NODE -INVP(NODE); OTHERWISE, */
/*                 -INVP(NODE) IS ITS INVERSE LABELLING. */

/*     OUTPUT PARAMETERS - */
/*        PERM   - THE PERMUTATION VECTOR. */

/* *************************************************************** */

/* Subroutine */ int mmdnum_(int *neqns, shortint *perm, shortint *invp, 
	shortint *qsize)
{
    /* System generated locals */
    int i__1;

    /* Local variables */
    static int node, root, nextf, father, nqsize, num;


/* *************************************************************** */


/* *************************************************************** */

    /* Parameter adjustments */
    --qsize;
    --invp;
    --perm;

    /* Function Body */
    i__1 = *neqns;
    for (node = 1; node <= i__1; ++node) {
	nqsize = qsize[node];
	if (nqsize <= 0) {
	    perm[node] = invp[node];
	}
	if (nqsize > 0) {
	    perm[node] = -invp[node];
	}
/* L100: */
    }
/*        ------------------------------------------------------ */
/*        FOR EACH NODE WHICH HAS BEEN MERGED, DO THE FOLLOWING. */
/*        ------------------------------------------------------ */
    i__1 = *neqns;
    for (node = 1; node <= i__1; ++node) {
	if (perm[node] > 0) {
	    goto L500;
	}
/*                ----------------------------------------- */
/*                TRACE THE MERGED TREE UNTIL ONE WHICH HAS */
/*                NOT BEEN MERGED, CALL IT ROOT. */
/*                ----------------------------------------- */
	father = node;
L200:
	if (perm[father] > 0) {
	    goto L300;
	}
	father = -perm[father];
	goto L200;
L300:
/*                ----------------------- */
/*                NUMBER NODE AFTER ROOT. */
/*                ----------------------- */
	root = father;
	num = perm[root] + 1;
	invp[node] = -num;
	perm[root] = num;
/*                ------------------------ */
/*                SHORTEN THE MERGED TREE. */
/*                ------------------------ */
	father = node;
L400:
	nextf = -perm[father];
	if (nextf <= 0) {
	    goto L500;
	}
	perm[father] = -root;
	father = nextf;
	goto L400;
L500:
	;
    }
/*        ---------------------- */
/*        READY TO COMPUTE PERM. */
/*        ---------------------- */
    i__1 = *neqns;
    for (node = 1; node <= i__1; ++node) {
	num = -invp[node];
	invp[node] = num;
	perm[num] = node;
/* L600: */
    }
    return 0;

} /* mmdnum_ */


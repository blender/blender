
#ifndef EXPP_NURB_H
#define EXPP_NURB_H


PyObject* CurNurb_getPoint( BPy_CurNurb* self, int index );
PyObject* CurNurb_pointAtIndex( Nurb* nurb, int index );

PyObject* CurNurb_appendPointToNurb( Nurb* nurb, PyObject* args );

#endif /* EXPP_NURB_H */

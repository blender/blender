#ifndef _BMESH_ERROR_H
#define _BMESH_ERROR_H

/*----------- bmop error system ----------*/

/*pushes an error onto the bmesh error stack.
  if msg is null, then the default message for the errorcode is used.*/
void BMO_RaiseError(BMesh *bm, BMOperator *owner, int errcode, char *msg);

/*gets the topmost error from the stack.
  returns error code or 0 if no error.*/
int BMO_GetError(BMesh *bm, char **msg, BMOperator **op);
int BMO_HasError(BMesh *bm);

/*same as geterror, only pops the error off the stack as well*/
int BMO_PopError(BMesh *bm, char **msg, BMOperator **op);
void BMO_ClearStack(BMesh *bm);

#if 0
//this is meant for handling errors, like self-intersection test failures.
//it's dangerous to handle errors in general though, so disabled for now.

/*catches an error raised by the op pointed to by catchop.
  errorcode is either the errorcode, or BMERR_ALL for any 
  error.*/
int BMO_CatchOpError(BMesh *bm, BMOperator *catchop, int errorcode, char **msg);
#endif

/*------ error code defines -------*/

/*error messages*/
#define BMERR_SELF_INTERSECTING			1
#define BMERR_DISSOLVEDISK_FAILED		2
#define BMERR_CONNECTVERT_FAILED		3
#define BMERR_WALKER_FAILED			4
#define BMERR_DISSOLVEFACES_FAILED		5
#define BMERR_DISSOLVEVERTS_FAILED		6
#define BMERR_TESSELATION			7

static char *bmop_error_messages[] = {
       0,
       "Self intersection error",
       "Could not dissolve vert",
       "Could not connect verts",
       "Could not traverse mesh",
       "Could not dissolve faces",
       "Could not dissolve vertices",
       "Tesselation error",
};

#endif /* _BMESH_ERROR_H */
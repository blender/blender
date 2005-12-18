#ifndef EXPP_POSE_H
#define EXPP_POSE_H

#include <Python.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <DNA_action_types.h>

/* EXPP PyType Objects */
extern PyTypeObject Pose_Type;

struct BPy_Pose;

typedef struct {
	PyObject_HEAD		/* required python macro   */
	bPose * pose;
} BPy_Pose;			/* a pose */

PyObject *Pose_CreatePyObject( bPose * p );

#endif /* EXPP_KEY_H */

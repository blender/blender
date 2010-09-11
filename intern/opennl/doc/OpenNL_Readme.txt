
This is OpenNL, a library to easily construct and solve sparse linear systems.
* OpenNL is supplied with a set of iterative solvers (Conjugate gradient,
  BICGSTAB, GMRes) and preconditioners (Jacobi, SSOR). 
* OpenNL can also use other solvers (SuperLU 3.0 supported as an OpenNL
  extension)

Note that to be compatible with OpenNL, SuperLU 3.0 needs to be compiled with
the following flag (see make.inc in SuperLU3.0):
CDEFS = -DAdd_ (the default is -DAdd__, just remove the second underscore)

OpenNL was modified for Blender to be used only as a wrapper for SuperLU.


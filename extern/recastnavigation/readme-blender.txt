The version of Recast is 1.5.0, from:
https://github.com/recastnavigation/recastnavigation
Changes made:
  * Recast/Source/RecastMesh.cpp: made buildMeshAdjacency() non-static so it can be used with recast-capi
  * Recast/Include/Recast.h: Added forward declaration for buildMeshAdjacency()

The following additional files were added:
  * recast-capi.cpp
  * recast-capi.h
These expose a C interface to the Recast library, which has only C++ headers.

The version of Detour is 1.4, from:
https://code.google.com/archive/p/recastnavigation/downloads
Changes made:
  * DetourStatNavMesh.h: use more portable definition of DT_STAT_NAVMESH_MAGIC
  * DetourStatNavMesh.cpp: comment out some unused variables to avoid compiler warnings
  * DetourStatNavMeshBuilder.h: add forward declaration for createBVTree
  * DetourStatNavMeshBuilder.cpp: made createBVTree non-static for use with recast-capi

The CMakeLists.txt file has been added, since the original software does not include build files for the libraries.

~rdb

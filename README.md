# Blender_Reaticle

 An open-source DEM simulation system named ***REATICLE*** for granular materials, is a fork of open-source impulse-based physics engine [**Bullet3**](https://github.com/bulletphysics/bullet3), which is carried out on open-source 3D creation suite [**Blender**](https://github.com/sobotka/blender) platform
  The main features are: 
* arbitrarily shaped particles’ frictions and collisions were unified in one linear complementarity problem. 
* **Real-time AABB Tree contact detection is updated with mirror-enhanced algorithm.** 
* **Multiple-data operation system is improved with Python API including velocity, force, location, orientation and contact number.**
* advanced computer technology is adopted for fast and stable simulation, that is, CPU Streaming SIMD Extensions 2 (SS2) for core dynamics computation and GPU compute unified device architecture (CUDA 8.0) for 3D creating and rendering.

[![DOI](https://zenodo.org/badge/352937464.svg)](https://zenodo.org/badge/latestdoi/352937464)

# Buiding Notation

## Versions are important
Main sets are like,
* Windows: 10 (A tip is [here](http://blog.reaticle.com/2020/06/22/system-language.html))
* Visual Studio: 2013
* Blender: ```2.79```
* Bullet3 : ```2.83```
* TortoiseSVN :``` ~>1.10.1```
* CMake:``` ~>3.9.6```
* Building  libraries: /bf-blender/tags/blender-2.79-release/lib>>>```[link here]```( https://svn.blender.org/svnroot/bf-blender/tags/blender-2.79-release/lib/win64_vc14/)


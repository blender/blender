Sculpt-Blender
==============

Sculpt-Blender is an independent branch of Blender focused on sculpting.
It's purpose is to develop and maintain key improvements needed to make 
Blender usable for a wider variety of sculpt workflows.

It arose out of my personal artistic need for a better dynamic topology
system.  

This is not a hard fork.  The branch will be regularly synced with official Blender.  

Main supported features:
* Improved DynTopo that preserves attributes.
* Various edge boundaries (e.g. marked seams, face set boundaries, UV island boundaries, etc)
  are preserved.
* Better hard surface modelling. 

Changes will be limited to "needed" functional improvements.
Features like these are off the table for now:

* Sculpt layers.
* Node/stack based brush composer.
* Brush properties.

For developers:
* We'll try to avoid refactors.
* There is a copy of a new sculpt brush API in the branch, but only smooth brushes are allowed to use it
  (it fixes a performance bug that particularly affects smooth brushes).
  

License
-------

Blender as a whole is licensed under the GNU General Public License, Version 3.
Individual files may have a different, but compatible license.

See [blender.org/about/license](https://www.blender.org/about/license) for details.

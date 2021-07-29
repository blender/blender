Useful 3D printing tools
========================

Statistics
----------

- volume
- surface area
  *(if you gold plate for eg, or use expensive paint :))*


Checks
------

- *degenerate geometry*
- - zero area faces
- - zero length edges
- - bow-tie quads.
- *solid geometry*
- - self intersections
- - non-manifold
- - non-contiguous normals (bad flipping)


Mesh Cleanup
------------

- basics - stray verts, loose edges
- degenerate faces, bow-tie quads


Visualisation
-------------

- visualize areas of low wall thickness.
- visualize overhangs (some printers have this as s limit).
- areas of low wall thickness.
- sharp/pointy surface.


Utilities
---------

- add text on an object *(common tasks - lots of people want this to add a name to personalize items)*
- Rig sizes (rings also common item to make)
- others???


Exporters
---------

- nice UI with format select and output paths for the print.
  *no need to recode re-use existing exporters, maybe recode some in C if too slow.*


Integration with toolplating
----------------------------
*(the thing that gets the model into printer commands)*

- http://slic3r.org
- https://github.com/makerbot/Miracle-Grue/blob/master/README.md
- Use a sliver, like slicer, skeinforge, cura, kissslicer, netfabb, ....

...not sure yet exactly how this would work, but we could have a `Print` button and it would send the file off and print :).


Notes
-----

- Normals are important
- Self intersections _can_ be ok.
- Some printer software already prevents solid areas from taking too much space by filling with non-solid grid.
  *(So we may not have to care about solid shapes so much)*

- For extrusion printers like makerbots it is really hard to print "overhangs"...
  because they build "from the bottom up" they can't for instance make an arm sticking out of a character sideways
- Check on http://www.shapeways.com/tutorials/

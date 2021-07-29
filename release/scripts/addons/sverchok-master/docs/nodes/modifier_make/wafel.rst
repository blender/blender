Wafel
=====

Functionality
-------------

This node make section possible to make as manufactured wafel structure. There is always pair of wafel nodes, two directions.    

If you want start with this node - try open json first.    

1. Import from import sverchok panel waffel_minimal.json to new layout.      

2. Make some order in layout and developt to needed condition.      

Inputs
------

**vecLine** - lines sected between sections of solid object. Form factor of object. each object has only two certices defining this section.     

**vecPlane** - vectors of one side section.     

**edgPlane** - closed edges (not planes) of one side section.     

**thickness** - thickness of material to use in thickness of waffel slots.     
  
Properties  
----------  
  
**threshold** - threshold of line length from **vecLine** to limit cut material when producing.     

**Up/Down** - direction of slots, there is only two cases, up or down. Not left and right and no XY directed vecLines never.  Remember this.     
  
Properties_extended  
-------------------  
  
**rounded** - rounded edges.     

**Bind2** - circles to bind.      

**Contra** - contrplane to define where to flip Up and Down direction. It is same as **vecPlane**.     

**Tube** - case of section lines, making holes in body. It is same as **vecLine**.     
  
Outputs  
-------  
  
**vert** - vertices of output.     

**edge** - edges of output.     

**centers** - polygons centers.     
  
Notes
-----

.. note::
  
Always make matrices rotations not orthogonal, it will not work 100%. Making something like (1.0,0.001,0) will work for matrix sections.     

Always use Cross section nodes, not bisect, it will not work.      

  
Examples  
--------  
  
.. image:: https://cloud.githubusercontent.com/assets/5783432/5235611/25661e04-7812-11e4-9dba-c05f9733e966.png  
  :alt: noalt  


.. image:: https://cloud.githubusercontent.com/assets/5783432/5235612/258da21c-7812-11e4-91cf-6da1dbe395b4.png  
  :alt: noalt  

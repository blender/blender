Float
=====

Functionality
-------------

Float digit. Has maximum/minimum values and flexible labeling. Cached in Sverchok 3Dtoolbox.


Inputs & Parameters
-------------------

**float**

Extended parameters
-------------------

**to-3d** - boolean flag makes float catched in 3D toolbox

**minimum** - defines minimum value

**maximum** - defines maximum value

Outputs
-------

**float** - only one digit.

Examples
--------

Three cases of float. With output, as router and as input (not functional). Only first case will be catched in 'scan for propertyes' in 3Dtoolbox of sverchok.

.. image:: https://cloud.githubusercontent.com/assets/5783432/4505625/26ac1a58-4af8-11e4-90c7-161736cfe43e.png
  :alt: float.PNG

This is 3D toolbox scanned float. Max and min values appears in 3d and node toolboxes (last in extended interface in propertyes panel). Label of node will apear in 3Dtoolbox and sorted depend on it. Flag 'to_3d' makes node catchable in 3D.

.. image:: https://cloud.githubusercontent.com/assets/5783432/4505626/26b5021c-4af8-11e4-9e5b-8ad09846cb08.png
  :alt: float2.PNG

Notes
-----

Float output only one digit, for ranges and lists reroute use route node.

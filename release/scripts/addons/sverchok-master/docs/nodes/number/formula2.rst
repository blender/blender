Formula
=======

Functionality
-------------

**Formula2 - support next operations:**
  - vector*vector, define hierarhy and calculate respectfully to it. 
  - Vector*scalar, the same. And output to vector.
  - Moreover, you can define lists with formula, i.e. ```0,1,2,3,4,5``` for series or ```(1,2,3),(1,2,3)``` for vertices.
  - Supporting expressions beside * / - +:
      - acos()
      - acosh()
      - asin()
      - asinh()
      - atan()
      - atan2()
      - atanh()
      - ceil()
      - copysign()
      - cos()
      - cosh()
      - degrees()
      - e
      - erf()
      - erfc()
      - exp()
      - expm1()
      - fabs()
      - factorial()
      - floor()
      - fmod()
      - frexp()
      - fsum()
      - gamma()
      - hypot()
      - isfinite()
      - isinf()
      - isnan()
      - ldexp()
      - lgamma()
      - log()
      - log10()
      - log1p()
      - log2()
      - modf()
      - pi
      - pow()
      - radians()
      - sin()
      - sinh()
      - sqrt()
      - str()
      - tan()
      - tanh()
      - trunc()
      - ==
      - !=
      - <, >
      - for, in, if, else
      - []

Inputs
------

**X** - main x that defines sequence. it can be range of vertices or range of floats/integers. 
If x == one number, than other veriables will be the same - one number, if two - two.  

**n[0,1,2,3,4]** - multisocket for veriables.  

Parameters
----------

**Formula** - the string line, defining formula, i.e. ```x>n[0]``` or ```x**n[0]+(n[1]/n[2])``` are expressions.  
May have ```x if x>n[0] else n[1]```  

Outputs
-------

**Result** - what we got as result.  

Usage
-----

.. image:: https://cloud.githubusercontent.com/assets/7894950/4606629/51fd445c-5228-11e4-815b-d12866da7794.png

.. image:: https://cloud.githubusercontent.com/assets/7894950/4688830/e1e9680a-5694-11e4-9062-6ee03356b533.png

.. image:: https://cloud.githubusercontent.com/assets/5783432/4689948/6d4c9fcc-56d4-11e4-9628-22eeffed4eed.png

.. image:: https://cloud.githubusercontent.com/assets/5783432/4689947/6d4be5e6-56d4-11e4-911f-86494b69f182.png

.. image:: https://cloud.githubusercontent.com/assets/5783432/4689951/6d590226-56d4-11e4-9519-67b2c871e9c8.png

.. image:: https://cloud.githubusercontent.com/assets/5783432/4689950/6d58321a-56d4-11e4-8ba9-1d28426b8307.png

.. image:: https://cloud.githubusercontent.com/assets/5783432/4689949/6d57503e-56d4-11e4-9df9-9224b8f645cb.png

.. image:: https://cloud.githubusercontent.com/assets/7894950/4732337/74f49ce6-59ba-11e4-8406-77d55c55ff02.png

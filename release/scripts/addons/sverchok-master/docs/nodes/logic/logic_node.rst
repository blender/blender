Logic
=====

Functionality
-------------

This node offers a variety of logic gates to evaluate any boolean inputs
It also has different operations to evaluate a pair of numbers, like minor than or greater than.


Input and Output
----------------

Depending on the mode you choose the sockets are automatically changed to
accommodate the expected inputs. 
Output is always going to be a boolean.


Parameters
----------

Most operations are self explanatory,
but in case they aren't then here is a quick overview:

=================== ========= ========= =================================
Tables              inputs     type      description
=================== ========= ========= =================================
And                  x, y      integer   True if X and Y are True
Or                   x, y      integer   True if X or Y are True
Nand                 x, y      integer   True if X or Y are False
Nor                  x, y      integer   True if X and Y are False
Xor                  x, y      integer   True if X and Y are opposite
Xnor                 x, y      integer   True if X and Y are equals

If                   x         integer   True if X is True
Not                  x         integer   True if X is False

<                    x, y      float     True if X < Y
>                    x, y      float     True if X > Y
==                   x, y      float     True if X = Y
!=                   x, y      float     True if X not = Y
<=                   x, y      float     True if X <= Y
>=                   x, y      float     True if X >= Y

True                 none      none      Always True
False                none      none      Always False
=================== ========= ========= =================================


Example of usage
----------------

.. image:: https://cloud.githubusercontent.com/assets/5990821/4333087/6b040a3e-3fdc-11e4-9693-7a00b0ce03bc.jpg

In this example we use Logic with Switch Node to choose between two vectors depending on the logic output.
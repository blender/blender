List Split
==========
Functionality
-------------

Split list into chuncks. The node is *data type agnostic*, meaning it makes no assumptions about the data you feed it. It shoudld accepts any type of data native to Sverchok.

Inputs
------

+--------------+---------------------------------------------------+
| Input        | Description                                       |
+==============+===================================================+
| Data         | The data - can be anything                        | 
+--------------+---------------------------------------------------+
| Split size   | Size of individual chuncks                        |
+--------------+---------------------------------------------------+

Parameters
----------


**Level**

It is essentially how many chained element look-ups you do on a list. If ``SomeList`` has a considerable *nestedness* then you might access the most atomic element of the list doing ``SomeList[0][0][0][0]``. Levels in this case would be 4.

**unwrap**

Unrwap the list if possible, this generally what you want.
[[1, 2, 3, 4]] size 2.
+--------+-------------------+
| Unwrap | Result            |
+========+===================+
| On     | [[1, 2], [3, 4]]  |
+--------+-------------------+
| Off    | [[[1, 2], [3, 4]]]|
+--------+-------------------+

**Split size**

Size of output chuncks.

Outputs
-------

**Split**

The list split on the selected level into chuncks, the last chunck will be what is left over.    

Examples
--------

Trying various inputs, adjusting the parameters, and piping the output to a *Debug Print* (or stethoscope) node will be the fastest way to acquaint yourself with the inner workings of the *List Item* Node.

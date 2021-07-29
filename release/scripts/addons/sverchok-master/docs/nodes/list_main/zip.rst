List Zip
========

Functionality
-------------

Making pares of data to mix togather as zip function.

x = [[[1,2],[3,4]]]
y = [[[5,6],[7,8]]]


out level 1 =       [[[[1, 2], [5, 6]], [[3, 4], [7, 8]]]]
out level 1 unwrap = [[[1, 2], [5, 6]], [[3, 4], [7, 8]]]
out level 2 =       [[[[1, 3], [2, 4]], [[5, 7], [6, 8]]]]
out level 2 unwrap = [[[1, 3], [2, 4]], [[5, 7], [6, 8]]]
out level 3 =       [[[[], []], [[], []]]]

Inputs
------

**data** multysocket

Properties
----------

**level** integer to operate level of conjunction
**unwrap** boolean to unwrap from additional level, added when zipped 

Outputs
-------

**data** adaptable socket



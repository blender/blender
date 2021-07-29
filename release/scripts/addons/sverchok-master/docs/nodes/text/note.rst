Note
====

Functionality
-------------
Note node allow show custom text and convert custom text to format naming indeces of viewer_INDX. For last writing words separated with spaces will be i.e.:
from **A B C Name index_** output will be **[ ['A'], ['B'], ['C'], ['Name'], ['index_'] ]**.

Inputs
------


+-----------------+--------------------------------------------------------------------------+
| Input           | Description                                                              |
+=================+==========================================================================+
| Text_in         | Input text to show or convert to mark indeces                            | 
+-----------------+--------------------------------------------------------------------------+


Parameters extended
-------------------

+--------------------+---------------+--------------------------------------------------------------------------+
| Param              | Type          | Description                                                              |  
+====================+===============+==========================================================================+
| **text**           | String        | Line to write custom text, will be shown in node                         | 
+--------------------+---------------+--------------------------------------------------------------------------+
| **show_text**      | Bool, toggle  | Show text line string field on node                                      |  
+--------------------+---------------+--------------------------------------------------------------------------+
| **Output socket**  | Bool, toggle  | Use or not output socket                                                 | 
+--------------------+---------------+--------------------------------------------------------------------------+
| **Input socket**   | Bool, toggle  | Use or not input socket                                                  |
+--------------------+---------------+--------------------------------------------------------------------------+
| **From clipboard** | Button        | Use data, stored in clipboard to fill the node                           |
+--------------------+---------------+--------------------------------------------------------------------------+
| **To text editor** | Button        | Sent text to text editor                                                 |
+--------------------+---------------+--------------------------------------------------------------------------+


Outputs
-------

+-----------------+--------------------------------------------------------------------------+
| Output          | Description                                                              |
+=================+==========================================================================+
| Text_out        | Output text formatted to INDX viewer                                     | 
+-----------------+--------------------------------------------------------------------------+

Examples
--------
.. image:: https://cloud.githubusercontent.com/assets/5783432/4328133/5968227a-3f81-11e4-8caa-9291cff8e3ff.png
Using hidden power of note node - output socket to name indeces in INDX viewer for any part. Vertices called as strings, can be any text.

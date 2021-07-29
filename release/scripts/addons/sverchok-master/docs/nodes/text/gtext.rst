GText
=====

Functionality
-------------

Creates Text in NodeView using GreasePencil strokes. 
It has full basic English and Cyrillic Character map and several extended character types::

    [ ] \ / ( ) ~ ! ? @ # $ % & ^ > < | 1234567890 - + * = _

Inputs
------

Gtext will display whatever text is currently in the system buffer / clipboard. 


UI & Parameters
---------------

**Node UI**

+------------+---------------------------------------------------------------------------------+
| Parameter  | Function                                                                        |
+============+=================================================================================+
| Set        | If clipboard has text, then Set will display that text beside the GText node.   |
+------------+---------------------------------------------------------------------------------+
| Clear      | This erases the GreasePencil strokes                                            |
+------------+---------------------------------------------------------------------------------+

GText Node will display the context of the clipboard after pressing the `Set` button. If no text is found in the clipboard
it will write placeholder 'your text here'.

**N-panel**

+---------------------+-------------------------------------------------------------------------------------------------+
| Parameter           | Function                                                                                        |
+=====================+=================================================================================================+
| Get from Clipboard  | Gets and sets in one action, takes text from clipboard and writes to NodeView with GreasePencil |
+---------------------+-------------------------------------------------------------------------------------------------+
| Thickness           | sets pixel width of the GreasePencil strokes                                                    | 
+---------------------+-------------------------------------------------------------------------------------------------+
| Font Size           | Scales up text and line-heights                                                                 |
+---------------------+-------------------------------------------------------------------------------------------------+


**Moving GText**

To move GText strokes around in NodeView you must move the GText Node then press Set again. This may not be entirely intuitive but it hasn't bugged us enough to do anything about it.


Outputs
-------

Outputs only to NodeView

Examples
--------

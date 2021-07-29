Text Out
========

Functionality
-------------

Inserting and outputting data to text editor with preformatting in csv, json or plain sverchok text.

Inputs
------

**Col** - for csv type column definition multysocket
**Data** - for sverchok single socket
**Data** - for json multysocket

Properties
----------

**Select** - Select text from blender text editor
**Select output format** - Property to choose between csv, plain sverchok and json data format
  **csv**:
    **Dialect** - to choose dialect of table
  **Sverchok** and **json**:
    **Compact** and **Pretty** - overall view of data presented readable or not so much readable
**Dump** - sent your data to text editor to selected text
**Append** - to add text not deleting old text
***********************
Panels of Sverchok
***********************


Node Tree Panel
===============

.. image:: https://cloud.githubusercontent.com/assets/5783432/4512957/866dacd8-4b46-11e4-9cfa-2b78d2a2f8a9.png
  :alt: nodetreepanel.ng

This panel allows to manage sverchok layouts as easy as you press buttons on an elevator.

Update
------

.. image:: https://cloud.githubusercontent.com/assets/5783432/4512960/868c837e-4b46-11e4-9fba-a5062fd5434f.png
  :alt: nodetreeupdate.png

+-------------------+---------------------------------+  
| Update            | description                     |
+===================+=================================+
| all               | Updates all trees of sverchok.  |
+-------------------+---------------------------------+  
| Update layout     | Updates currently active layout |
+-------------------+---------------------------------+  

Layout manager
--------------

.. image:: https://cloud.githubusercontent.com/assets/5783432/4512959/867d395a-4b46-11e4-9419-95ed1479ac72.png
  :alt: manager.png

Box to quickly pick layout, switch between them with buttons instead of popup list. Also have settins:


+--------------------+----------------------------------------------------------------------------------------+
| Button             | Function                                                                               |  
+====================+========================================================================================+
| **B**              | bake this layout - will gather all layout's viewer draw and viewer text to bake them.  |
|                    | Bake only if bakeable button is active on node, else ignore.                           |   
+--------------------+----------------------------------------------------------------------------------------+
| **Show layout**    | Controlls all OpenGL viewer of this layout. Viewer, Stethoscop and Viewer Indices      |
+--------------------+----------------------------------------------------------------------------------------+
| **Animate layout** | to animate the layout (or not) - may preserve you time.                                |
+--------------------+----------------------------------------------------------------------------------------+
| **Process layout** | Automaticlly evaluate layout while editing, disable for large or complex layouts       |
+--------------------+----------------------------------------------------------------------------------------+
| **Fake User**      | Sets fake user so layout isn't deleted on save                                         |
+--------------------+----------------------------------------------------------------------------------------+


Check for updates
-----------------

.. image:: https://cloud.githubusercontent.com/assets/5783432/4512958/8671953c-4b46-11e4-898d-e09eec52b464.png
  :alt: upgradenewversion.png

**Check for updates** - finds if master branch on github has new version of sverchok. In future there will be releases, but now dangerouse update.

**Upgrade Sverchok** - upgrades Sverchok from github with new version - button appears only if 'check for updates' finds a new version.


Tools Panel
===========

Blender's node editor, as well as other types of editor, has a Tools panel (on the left side), which is toggled by pressing the *T* key.
Sverchok uses it to present all nodes available, organized in categories. Each category has a special tab for it.

.. image:: https://user-images.githubusercontent.com/284644/34564322-08f15328-f178-11e7-8b0d-76f49c7e3afe.png
  :alt: t-panel.png

You can add node to the tree by clicking corresponding button in the *T* panel.

Presets Panel
=============

This is a second tab under the *T* panel. This is how it looks by default:

.. image:: https://user-images.githubusercontent.com/284644/34566374-19623d6e-f180-11e7-840a-ec5bb8972e64.png
  :alt: empty-presets.png

Introduction to Presets
-----------------------

Preset is a named set of stored settings of one of several nodes. You can:

* Save settings of one or several selected nodes (links between nodes are saved
  too) under specific name.
* Use saved preset in another node tree later.
* Import and export presets as `.json` files.
* Share presets with other users via gist.github.com service, or import presets
  made by other users.

Presets are saved as `.json` files under Blender configuration directory, in `datafiles/sverchok/presets`.

It can be good idea to store as a preset (and maybe share) the following things:

* One node with a lot of settings, if you think this is "good" settings and you
  are going to use the same settings many times.
* Scripted node, or "Mesh Expression" node, or one of other nodes that use
  Blender's text blocks as settings. Such nodes are stored together with
  corresponding text.
* Group (monad) node. It is saved with all contents.
* Several linked nodes, that do some completed thing, for later usage "as-is".
* Several linked nodes, that are "sort of going to be needed in this
  combination often", so that later you can start with this preset and add some
  nodes to it or tweak it somehow.

Panel Buttons
-------------

The Presets panel has the following buttons:

* **Save Preset**. This button is only shown when there are some nodes selected
  in the tree. When you press this button, it asks you for the name under which
  this preset should be known. You need to enter some descriptive name and
  press Ok. After that, the preset will become available in the lower part of
  the panel.
* **Manage Presets**. This is a toggle button. It switches you between "presets
  usage mode" (which is the default, when button is not pressed) and "presets
  management mode" (when the button is pressed).

Contents of lower part of the panel depend on whether the **Manage Presets** button is pressed.

When management mode is disabled, there is a button shown for each preset that you already have:

.. image:: https://user-images.githubusercontent.com/284644/34495416-df370a76-f016-11e7-98e7-97c01969fe74.png

By pressing such button, you add nodes saved in corresponding preset into
current tree. New nodes are automatically selected, so that you can move them
to another part of the node view.

When management mode is enabled, there are more buttons:

.. image:: https://user-images.githubusercontent.com/284644/34495429-f1b9a6d6-f016-11e7-9d2c-ffd6432c27db.png

* **Import preset from Gist**. You will be asked for Gist ID or full URL of the
  gist, and preset name. If you have gist URL in the clibpoard, it will be
  pasted automatically.
* **Import preset from file**. File browser will appear to allow you to select
  a `.json` file to import. In the left bottom part of this file browser, there
  is mandatory text field asking you to enter preset name.

The following buttons (in this order) are shown for each preset you have:

* **Export preset to Gist**. Preset will be exported to gist service. Gist URL will be automatically copied into clipboard.
* **Export preset to outer file**. File browser will appear asking you to select where to save the preset.
* **Edit preset properties**. A dialog will appear allowing you to change the following properties of preset: Name, Description, Author, License. The Description attribute will be used as a tooltip for preset button. 

  .. image:: https://user-images.githubusercontent.com/284644/34521620-7ca698dc-f0b0-11e7-94a9-757975ec1ec7.png

* **Delete preset**. You will be asked for confirmation.

3D Panel
========

.. image:: https://cloud.githubusercontent.com/assets/5783432/4512953/865c3962-4b46-11e4-8dbd-df445f10b808.png
  :alt: panel3d.png

With this panel your layout becomes addon itself. So, you making your life easy.

Scan for props
--------------

.. image:: https://cloud.githubusercontent.com/assets/5783432/4512955/866461fa-4b46-11e4-8caf-d650d15f5c5f.png
  :alt: scanprops.png


When layout is in, check for next nodes to embad them as properties:
 - float node
 - int node
 - object in node
 
Sorting them by label, that user defined in node tree panel or if no label, the name of the node is used.

Update all
----------

.. image:: https://cloud.githubusercontent.com/assets/5783432/4512955/866461fa-4b46-11e4-8caf-d650d15f5c5f.png
  :alt: updateall.png

Forces update of all layouts.

Clean
-----

.. image:: https://cloud.githubusercontent.com/assets/5783432/4512954/8662fbf8-4b46-11e4-8f67-243a56c48856.png
  :alt: cleanlayout.png

Button to remove sverchok layouts, that has not users (0)

**hard clean**  - boolean flag to remove layouts even if it has users (1,2...), but not fake user (F). Fake user layout will be left.

**Clean layouts** - remove layouts. Button active only if no node tree windiw around. Better to make active layout nothing or fake user layout to prevent blender crash. Easyest way - activate your Faked user layout, on 3D press **ctrl+UP** and press button. than again **ctrl+UP** to go back. No wastes left after sverchok in scene.

Use with care.


Properties
----------

.. image:: https://cloud.githubusercontent.com/assets/5783432/4512956/8666aeba-4b46-11e4-9c13-651e3826f111.png
  :alt: properties.png

Layouts by box. Every layout has buttons:

+--------------------+----------------------------------------------------------------------------------------+
| Button             | Function                                                                               |  
+====================+========================================================================================+
| **B**              | bake this layout - will gather all layout's viewer draw and viewer text to bake them.  |
|                    | Bake only if bakeable button is active on node, else ignore.                           |   
+--------------------+----------------------------------------------------------------------------------------+
| **Show layout**    | show or hide all viewers - to draw or not to draw OpenGL in window, but bmesh viewer   |
|                    | not handled for now.                                                                   |
+--------------------+----------------------------------------------------------------------------------------+
| **Animate layout** | to animate the layout (or not) - may preserve you time.                                |
+--------------------+----------------------------------------------------------------------------------------+
| **P**              | Process layout, allows safely manupilate monsterouse layouts.                          |
+--------------------+----------------------------------------------------------------------------------------+
| **F**              | Fake user of layout to preserve from removing with reloading file or                   |
|                    | with **clean layouts** button.                                                         |
+--------------------+----------------------------------------------------------------------------------------+

Properties has also gathered values:

**floats and integers** - digit itself, maximum and minimum vaues.

**object in** - button for object in node to collect selected objects.


Import Export Panel
===================

.. image:: https://cloud.githubusercontent.com/assets/5783432/4519324/9e11b7be-4cb6-11e4-86c9-ee5e136ed088.png
  :alt: panelio.png

location: N panel of any Sverchok Tree.

Import and export of the current state of a Sverchok Tree. This tool stores 

 - Node state: location, hidden, frame parent
 - Node parameters: (internal state) like booleans, enum toggles and strings
 - connections and connection order (order is important for dynamic-socket nodes)

Export
------

.. image:: https://cloud.githubusercontent.com/assets/5783432/4519326/9e4320f6-4cb6-11e4-88ba-b6dc3ce48d5a.png
  :alt: panelexport.png

+---------+-------------------------------------------------------------------------------------------------+
| feature | description                                                                                     | 
+=========+=================================================================================================+
| Zip     | When toggled to *on* this will perform an extra zip operation when you press Export. The zip    |
|         | can sometimes be a lot smaller that the json. These files can also be read by the import        |  
|         | feature.                                                                                        |
+---------+-------------------------------------------------------------------------------------------------+
| Export  | Export to file, opens file browser in blender to let you type the name of the file, Sverchok    |
|         | will auto append the .json or .zip file extention - trust it.                                   | 
+---------+-------------------------------------------------------------------------------------------------+

Import
------

.. image:: https://cloud.githubusercontent.com/assets/5783432/4519325/9e2f2c40-4cb6-11e4-8b03-479a411ead3d.png
  :alt: panelimport.png

+-------------+-------------------------------------------------------------------------------------------------+
| feature     | description                                                                                     | 
+=============+=================================================================================================+
| Layout name | name of layout to use, has a default but you might want to force a name                         |
+-------------+-------------------------------------------------------------------------------------------------+
| Import      | import to new layout with name (described above). Can import directly from zip file if there is |
|             | only one .json in the zip. Warning to the descerned reader, only import from zip if the source  |
|             | is trusted. If you are not sure, resist the urge and take the time to learn a little bit about  |
|             | what you are doing.                                                                             |
+-------------+-------------------------------------------------------------------------------------------------+

**Warnings**

Consider this whole IO feature experimental for the time being. You use it at your own risk and don't be surprised if certain node trees won't export or import (See bug reporting below). The concept of importing and exporting a node tree is not complicated, but the practical implementation of a working IO which supports dynamic nodes requires a bit of extra work behind the scenes. Certain nodes will not work yet, including (but not limited to) :


+-------------+---------------------------------------------------------------------------------------+
| Node        | Issue                                                                                 |
+=============+=======================================================================================+
| Object In   | the json currently doesn't store geometry but an empty shell without object           | 
|             | references instead                                                                    |  
+-------------+---------------------------------------------------------------------------------------+
| SN MK1      | currently this auto imports by design, but perhaps some interruption of the import    |
|             | process will be implemented                                                           |
+-------------+---------------------------------------------------------------------------------------+


**Why make it if it's so limited?**

Primarily this is for sharing quick setups, for showing people how to achieve a general result. The decision to not include geometry in the Object In references may change, until then consider it a challenge to avoid it. The way to exchange large complex setups will always be the ``.blend``, this loads faster and stores anything your Tree may reference. 

**While importing I see lots of messages in the console!**

Relax, most of these warnings can be ignored, unless the Tree fails to import, then the last couple of lines of the warning will explain the failure.

**Bug Reporting**

By all means if you like using this feature, file issues in `this thread <https://github.com/nortikin/sverchok/issues/422>`_. The best way to solve issues is to share with us a screenshot of the last few lines of the error if we need more then we will ask for a copy of the `.blend`.

Groups Panel
============

Crete a node group (Monad) from selection.
It can have vectorized inputs, adding or removing sockets.
Sverchok groups is a beta feature, use a your own risk and please report bugs. Also while it is in beta old node groups may break.
`Bug reports <https://github.com/nortikin/sverchok/issues/462>`_.

Templates in menu panel of nodes area
=====================================

You can use embedded templates in Sverchok. They are stored in json folder as jsons for import to Sverchok.

.. image:: https://cloud.githubusercontent.com/assets/5783432/19623205/245bcab2-98d2-11e6-810c-ace33de8499b.gif
  :alt: templates.gif

Generative Art
==================

*destination after Beta: Generators*

Functionality
-------------

This node can be used to produce recursive three dimensional structures following a design specified in a separate xml file. These structures are similar to fractals or lsystems.

The xml file defines a set of transformation rules and instructions to place objects. A simple set of rules can often generate surprising and complex structures.

Inputs & Parameters
-------------------

This node has the following parameters:

- **xml file** - Required. This specifies the LSystem design and should be a linked text block in the .blend file.  
- **r seed** - Integer to initialize python's random number generator. If the design includes a choice of multiple rules, changing this will change the appearance of the design
- **max mats** - To avoid long delays or lock ups the output of the node is limited to this number of matrices

This node has the following inputs:

- **Vertices** - Optional. A list of vertices to be joined in a ring and used as the basis for a tube structure. Typically the output of a Circle or NGon node.
- **data** - Optional. The xml file can have optional variables defined using {myvar} type format notation. Extra named data inputs are generated for each of these these variables. These variables can be used to control animations.


Outputs
-------

- **Vertices, Edges and Faces** - If the Vertices input is connected, these outputs will define the mesh of a tube that skins the structure defined in the xml file. 

- **Shapes Matrices** - For each *shape* atribute defined in the xml file a named output will be generated. This output is a list  list of matrices that define the structure.


Examples of usage
------------------

A simplified description of the algorithm for the evaluation of a design.

The xml file (see below for examples and descriptions) consist of a set of rules, each rule has a list of instructions, each instruction defines a transform and either a call to a rule or an instruction to place an instance. 

The system is implemented by a stack where each item in the stack consists of the next rule to call, the current depth of the system  and the current state of the system. At each iteration of the processor the last item is removed from the stack and processed. 

Each instruction in the rule removed from the stack is processed in turn. The current state of this system is set to that of the item removed from the stack. Any transform in the instruction is applied to the system state. If the instruction is a call to a rule, a new item is added to the stack with the new rule, the depth increased by one, and the new system state. If the instruction is to place an instance, the matrix representing the new system state is added to the output matrix list for that type of shape. The processor then proceeds to what is now the last item on the stack.

If the  max_depth for the current rule is reached  or the max_depth for overall design is reached then the processor goes back and processes what is now the last item on the stack without taking any other action. If the stack is empty or the maximum number of matrices has been reached the processor stops.


A simple example of an xml design file:

6 Spirals
::

	<rules max_depth="150">
		<rule name="entry">
		    <call count="3" transforms="rz 120" rule="R1"/>
		    <call count="3" transforms="rz 120" rule="R2"/>
		</rule>
		<rule name="R1">
		    <call transforms="tx 2.6 rx 3.14 rz 12 ry 6 sa 0.97" rule="R1"/>
		    <instance  transforms="sa 2.6" shape="box"/>
		</rule>
		<rule name="R2">
		    <call transforms="tx -2.6 rz 12 ry 6 sa 0.97" rule="R2"/>
		    <instance transforms="sx 2.6" shape="box"/>
		</rule>
	</rules>

This specifies the following design with 6 spirals.  

.. image:: https://cloud.githubusercontent.com/assets/7930130/13376231/cb79f476-de1b-11e5-90e9-845f3c201228.png
  :alt: 6 spiral screen shot with node diagram and text file and structure

The xml file consists of a list of rules. There must be at least one rule called entry. This is the starting point for the processor. Each rule consists of a list of instructions. These instructions can either be a call to another rule or an instruction to place an instance of an object. 

Calls can be recursive. For the example above the first instruction in rule R1 also calls rule R1. This recursion stops when the max_depth value is reached or the max_mats value set in the node is reached. The max_depth can also be set separately for each rule and is added as an attribute eg ``<rule name="R1" max_depth="10">``.

Each of these instructions can be modified with a set of transforms. If the transform is omitted it defaults to the identity transform.

A transform consist of translations, rotations and scaling operations. For example ``tx 1.3``  means translated 1.3 units in the ``x`` direction, ``rz 6``  means rotate 6 degrees about the ``z`` axis and ``sa 0.99`` means scale all axes by 0.99.

The full list of transforms that take one argument : ``tx ty tz rx ry rz sx sy sz sa``  
In addition all three axes values for either a translation or scale can be applied at once with a triplet of values. 
For example: ``t 1.1 2.2 3.3  s 0.9 0.9 0.7``

Instead of using a single *transform* attribute, each transform can be specified individually. For example ``transforms="tx 1 rz 90 sa 0.75"`` can be replaced with ``tx="1" rz="90" sa="0.75"``.

The count attribute specifies how many times that instruction is repeated.  if count is omitted it defaults to 1. For example the instruction ``<call count="3" transforms="rz 120" rule="R1"/>`` calls rule ``R1`` applying a 120 degree rotation about ``z`` in between each call.

An instance instruction tells the processor to add a matrix to the output list defining the state of the system at that point. The names used in the shape attribute are used as the names for the node's output sockets. If there is more than one type of shape each will have its own output socket.


Multiple Rule Definition Example
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

There can be multiple definitions of the same rule in an xml file.

For example

Tree
::

	<rules max_depth="100">
	    <rule name="entry">
		<call  rule="spiral"/>
	    </rule>
	    <rule name="spiral" weight="100">
		<call transforms="tz 0.1 rx 1 sa 0.995" rule="spiral"/>
		<instance transforms="s 0.1 0.1 0.15" shape="tubey"/>
	    </rule>
	    <rule name="spiral" weight="100">
		<call transforms="tz 0.1 rx 1 ry 4 sa 0.995" rule="spiral"/>
		<instance transforms="s 0.1 0.1 0.15" shape="tubey"/>
	    </rule>
	    <rule name="spiral" weight="100">
		<call transforms="tz 0.1 rx 1 rz -4 sa 0.995" rule="spiral"/>
		<instance transforms="s 0.1 0.1 0.15" shape="tubey"/>
	    </rule>
	    <rule name="spiral" weight="20">
		<call transforms="rx 15" rule="spiral"/>
		<call transforms="rz 180" rule="spiral"/>
	    </rule>
	</rules>

.. image:: https://cloud.githubusercontent.com/assets/7930130/7782285/25164a80-0162-11e5-9feb-32c4f3908f1e.png
  :alt: tree structure image

In the above xml file there are four definitions of the ``spiral`` rule. Each rule version has a weight attribute. The processor will call each version of the ``spiral`` rule in a random manner. The weight attribute will determine the probability a particular rule version is called. 

The first three definitions of the ``spiral`` rule all place an object instance and then call the ``spiral`` rule with the same translation along the ``z`` axis and rotation about the ``x`` axis but different amounts of rotation about the ``y`` and ``z`` axis. The fourth definition calls the ``spiral`` rule twice without placing an instance. This causes the branches in the tree structure. Changing the value of the weight attribute for this rule version will change how often the tree branches. For a larger weight value, the rule gets called more often and there are more branches.

If the weight attribute is omitted each version will have equal weight. Changing the value of ``r seed`` in the node interface will change the generated structure for xml files with multiple rule definitions. This example had rseed = 1.


Successor Rule Example
~~~~~~~~~~~~~~~~~~~~~~~~~

Normally when the ``max_depth`` for a rule is reached that 'arm' of the structure is finished. If a rule defines a successor rule then this rule will be called when the ``max_depth`` is reached. In the following example when the ``y180`` rule gets called it will be called  90 times in succession and produce a 180 degree turn about the y axis. When it finishes the successor rule ``r`` will be called and either produce a 180 degree turn about the ``y`` axis or the ``z`` axis.

Nouveau variation
::

    <rules max_depth="1000">
        <rule name="entry">
            <call count="2" transforms="rz 60" rule="r"/>
        </rule>
        <rule name="r"><call rule="y180"/></rule>
        <rule name="r"><call rule="z180"/></rule>
        <rule name="y180" max_depth="90" successor="r">
            <call rule="dbox"/>
            <call transforms="ry -2 tx 0.1 sa 0.996" rule="y180"/>
        </rule>
        <rule name="z180" max_depth="90" successor="r">
            <call rule="dbox"/>
            <call transforms="rz 2 tx 0.1 sa 0.996" rule="z180"/>
        </rule>
        <rule name="dbox">
            <instance transforms="s 0.55 2.0 1.25 ry 90 rz 45" shape="box"/>
        </rule>
    </rules>

.. image:: https://cloud.githubusercontent.com/assets/7930130/7629793/cb2d4a30-fa83-11e4-8c75-2fa6488f65fe.png
  :alt: nouveau variation structure

This example needs "max matrices" set to 5000 to get the above result.


Mesh Mode Example
~~~~~~~~~~~~~~~~~~

Using the matrices output allows a separate object to be placed at each location. The vertices input and the mesh (vertices, edges, faces) output "skins" the mesh into a much smaller number of objects. The vertices input should be a list of vertices such as that generated by the "Circle" node or "NGon" node. It could also be a circle type object taking from the scene using the "Objects In" node. The list of vertices should be in order so they can be made into a ring with the last vertex joined to the first. That ring dosen't have to be planar.

.. image:: https://cloud.githubusercontent.com/assets/7930130/13376232/d20249ce-de1b-11e5-968d-727f0038305e.png
  :alt: node and result picture for 6 spiral in mesh mode

The output will not always be one mesh. If the rule set ends one 'arm' and goes back to start another 'arm' these two sub-parts will be separate meshes. Sometimes the mesh does not turn out how you would like. This can often be fixed by changing the rule set.

Often a mesh tube will turn out flat rather than being tube like. This can usually be fixed by either rotating the vertex ring in the scene or by adding a rotation transform to the "instance" commands in the rule set.

For example change ``<instance shape="s1"/>`` to ``<instance transforms="ry 90" shape="s1"/>``

In other cases the mesh can be connected in the wrong order.

For example the following two xml files will look the same when the matrix output is used to place objects, but have different output when they are used in mesh mode. Both sets of xml rules produce the same list of matrices just in a different order.

Fern 1 
::

	<rules max_depth="2000">
	    <rule name="entry">
		<call  rule="curl" />      
	    </rule>
	    
	    <rule name="curl" max_depth="80">
		<call transforms="rx 12.5 tz 0.9 s 0.98 0.95 1.0" rule="curl"/>
		<instance shape="box"/>       
		<call transforms="tx 0.1 ty -0.45 ry 40 sa 0.25" rule="curlsmall" />  
	    </rule>
		
	    <rule name="curlsmall" max_depth="80">
		<call transforms="rx 25 tz 1.2 s 0.9 0.9 1.0" rule="curlsmall"/>
		<instance shape="box"/>     
	    </rule>
	    
	</rules>

.. image:: https://cloud.githubusercontent.com/assets/7930130/7629779/b6553802-fa83-11e4-8390-aa9ba2a0c44d.png
  :alt: image fern wrong

Fern 2
::

	<rules max_depth="2000">
	    <rule name="entry">
		<call  rule="curl1" />  
		<call  rule="curl2" />      
	    </rule>
	    
	    <rule name="curl1" max_depth="80">
		<call transforms="rx 12.5 tz 0.9 s 0.98 0.95 1.0" rule="curl1"/>
		<instance shape="box"/>        
	    </rule>
	    
	    <rule name="curl2" max_depth="80">
		<call transforms="rx 12.5 tz 0.9 s 0.95 0.95 1.0" rule="curl2"/>
		<call transforms="tx 0.1 ty -0.45 ry 40 sa 0.25" rule="curlsmall" />     
	    </rule>    
	    
	    <rule name="curlsmall" max_depth="80">
		<call transforms="rx 25 tz 1.2 s 0.9 0.9 1.0" rule="curlsmall"/>
		<instance shape="box"/>     
	    </rule>    
	</rules>

.. image:: https://cloud.githubusercontent.com/assets/7930130/7629783/bbe99588-fa83-11e4-8d70-92cc2909675e.png
  :alt: image fern right

Again these were both done with max mats set to 5000.

Constants and Variables Example
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Constants and variables can be included in the xml file by replacing a numerical value with a pair of braces. 
::

    transforms = "tx 0.5 rx 20 sa 0.9"

becomes
::

    transforms = "tx {x_const} rx 20 sa 0.9"

Constants are defined within the xml as follows:
::

    <constants	x_const="0.5" />

Multiple constants can be defined within one element and several *constants* elements can be used as required in the xml file.

If a field name in between curly brackets is not given a value in a *constants* element then a named input socket will be added to the node. A *Float*, *Integer* or similar node input can be wired up to this input variable.

The example below uses a variable ({curl_angle}) to animate the amount of curl on the fern structure shown in the mesh mode example and two constants to fix the the value of the ``tz`` transform in the large curl and the scale ({sxy}) in all the curls.

Fern 3
::

    <rules max_depth="2000">
	<constants zd="1.5" sxy="0.9" />
        <rule name="entry">
           <call  rule="curl1" />  
           <call  rule="curl2" />      
        </rule>
    
        <rule name="curl1" max_depth="60">
            <call transforms="rx {curl_angle} tz {zd} s {sxy} {sxy} 1.0" rule="curl1"/>
            <instance shape="box"/>        
        </rule>
    
        <rule name="curl2" max_depth="40">
            <call transforms="rx {curl_angle} tz {zd} s {sxy} {sxy} 1.0" rule="curl2"/>
            <call transforms="tx 0.1 ty -0.45 ry 40 sa 0.25" rule="curlsmall" />     
        </rule>    
    
        <rule name="curlsmall" max_depth="40">
            <call transforms="rx 2*{curl_angle} tz 2.7 s {sxy} {sxy} 1.0" rule="curlsmall"/>
            <instance shape="box"/>     
        </rule>    
    </rules>
    
.. image:: https://cloud.githubusercontent.com/assets/7930130/13376233/d7303744-de1b-11e5-8c91-1d56f412b27d.png
  :alt: image fern animation

For this animation the index number of the current frame in the animation is translated from the range 1 to 250 to the range 16 to 6 via the "Map Range" node and wired into the ``curl_angle`` input of the "Generative Art" node. This cause the fern to unwind as the animation proceeds.

Simple maths can also be use in the transforms definition. This has been used above in the ``curlsmall`` rule. The ``rx`` rotation of the transform will always be twice that of the ``rx`` rotation in the ``curl1`` and ``curl2`` rules. There cannot be any spaces in any maths expressions for the rotation, translation or scale parameters when using a single transforms attribute string. To allow for more complicated expressions each transform can be separated out into its own attribute. 

transforms as single attribute (no spaces allowed in maths expression)
::

    <call transforms="tx 1 rz -1*{v1} ry {v2}" rule="R1"/>

each transform with its own attribute (can have spaces)
::

    <call tx="1" rz="-1 * {v1}" ry="{v2}" rule="R1"/>


All this is implemented by first using python's string ``format`` method to substitute in the variable value from the node data input. Then the resulting string is passed to python's ``eval()`` function. The string must evaluate to a single number (float or integer). Using ``eval()`` is a potential security problem as in theory someone could put some malicious code inside an xml lsystem definition. As always don't run code from a source you don't trust.

The python ``math`` and ``random`` modules exist in the namespace of the "Generative Art" node so for example a transform could be defined as:
::

    tx="2**0.5"

or:
::

    tx="math.sqrt(2)"

Only the transforms that take a single number that is ``tx, ty, tz, rx, ry, rz, sx, sy, sz`` and ``sa`` have been implemented using individual attributes. The ones that use triplets to specify all three translations or scales at once (``t`` and ``s``) can only be used in a transform string.


References
----------

This node is closely based on `Structure Synth`_ but the xml design format and most of the code comes from `Philip Rideout's`_ `lsystem`_ repository on github.


.. _Structure Synth: http://structuresynth.sourceforge.net/
.. _lsystem: https://github.com/prideout/lsystem
.. _Philip Rideout's: http://prideout.net/





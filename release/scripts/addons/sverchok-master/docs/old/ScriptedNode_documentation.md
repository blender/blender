## Scripted Node (Generator)

aka Script Node or SN. (iteration 1)

- Introduction
- Features
- Examples
- Techniques to improve Python performance
- Limitations
- Future

### Introduction

When you want to express an idea in written form, if the concept is suitable for a one line Python expression then you can use the Formula nodes. They require little setup just [plug and play](). However, they are not intended for multi-line python statements, and sometimes that's exactly what you want.

ScriptNode (SN) allows you to write multi-line python programs, it's possible to use the node as a Sandbox for writing full nodes. The only real limitation will be your familiarty with Python and `bpy`. It's a prototype so bug reports are welcome.

### Features

allows:
- Loading/Reloading scripts currently in TextEditor
- imports and aliasing, ie anything you can import from console works in SN
- nested functions and lambdas
- named inputs and outputs
- named operators (buttons to action something upon button press)

At present all scripts for SN must (strict list - general): 
- have 1 `sv_main` function as the main workhorse
- `sv_main` must take 1 or more arguments (even if you don't use it)
- all function arguments for `sv_main` must have defaults.
- each script shall define `in_sockets` and `out_sockets`
- `ui_operators` is an optional third output parameter
- TextEditor has automatic `in_sockets` list creation (`Ctrl+I -> Generate in_sockets`) when the key cursor is over `sv_main`. (please note: it doesn't attempt to detect if you want nested verts or edge/polygon so it assumes you want 'v') 
    ![showit](https://cloud.githubusercontent.com/assets/619340/2854040/e6351180-d14b-11e3-8055-b3d8c707675d.gif) 


#### `sv_main()`

`sv_main()` can take int, float and list or nested list. Here are some legal examples:

```python
sv_main(vecs_in_multi=[[]], vecs_in_flat=[], some_var=1, some_ratio=1.2):

[[]]        # for nested input (lists of lists of any data type currently supported)
[]          # for flat (one list)
int, float  # for single value input

```

#### `in_sockets`

```python
in_sockets = [
    [type, 'socket name on ui', input_variable],
    [type, 'socket name on ui 2', input_variable2],
    # ...
]
```

#### `out_sockets`

```python
out_sockets = [
    [type, 'socket name on ui', output_variable],
    [type, 'socket name on ui 2', output_variable2],
    # ...
]
```

#### `in_sockets and out_sockets`

- Each `"socket name on ui"` string shall be unique.
- `type` are currently limited to
   - 's' : floats, ints, edges, faces
   - 'v' : vertices, vectors
   - 'm' : matrices

#### `ui_operators`

```python
ui_operators = [
    ['button_name', func1]
] 
```
- Here `func1` is the function you want to call when pressing the button.
- Each `"button_name"` is the text you want to appear on the button. For simplicity it must be unique and a valid variable name. Use alphanumerics only and separate words with single underscores if you need.

#### `return`

Simple, only two flavours are allowed at the moment.
```python
return in_sockets, out_sockets
# or
return in_sockets, out_sockets, ui_operators
```

### Examples

The best way to get familiarity with SN is to go through the templates folder. They are intended to be lightweight and educational, but some of them will show
advanced use cases. The [thread on github](https://github.com/nortikin/sverchok/issues/85) may also provide some pictorial insights and animations.

Sverchok includes a plugin in TextEditor which conveniently adds `sv NodeScripts` to the Templates menu.

A typical nodescript may look like this:

```python
from math import sin, cos, radians, pi
from mathutils import Vector, Euler


def sv_main(n_petals=8, vp_petal=20, profile_radius=1.3, amp=1.0):

    in_sockets = [
        ['s', 'Num Petals',  n_petals],
        ['s', 'Verts per Petal',  vp_petal],
        ['s', 'Profile Radius', profile_radius],
        ['s', 'Amp',  amp],
    ]

    # variables
    z_float = 0.0
    n_verts = n_petals * vp_petal
    section_angle = 360.0 / n_verts
    position = (2 * (pi / (n_verts / n_petals)))

    # consumables
    Verts = []

    # makes vertex coordinates
    for i in range(n_verts):
        # difference is a function of the position on the circumference
        difference = amp * cos(i * position)
        arm = profile_radius + difference
        ampline = Vector((arm, 0.0, 0.0))

        rad_angle = radians(section_angle * i)
        myEuler = Euler((0.0, 0.0, rad_angle), 'XYZ')

        # changes the vector in place, successive calls are accumulative
        # we reset at the start of the loop.
        ampline.rotate(myEuler)
        x_float = ampline.x
        y_float = ampline.y
        Verts.append((x_float, y_float, z_float))

    # makes edge keys, ensure cyclic
    Edges = [[i, i + 1] for i in range(n_verts - 1)]
    Edges.append([i, 0])

    out_sockets = [
        ['v', 'Verts', [Verts]],
        ['s', 'Edges', [Edges]],
    ]

    return in_sockets, out_sockets

```

Here's a `ui_operator` example, it acts like a throughput (because in and out are still needed by design). You'll notice that inside `func1` the node's input socket is accessed using `SvGetSockeyAnyType(...)`. It is probably more logical if we could access the input data directly from the variable `items_in`, currently this is not possible -- therefor the solution is to use what sverchok nodes use in their internal code too. The upshot, is that this exposes you to how you might access the socket content of other nodes. Experiment :)

```python

def sv_main(items_in=[[]]):
 
    in_sockets = [
        ['v', 'items_in', items_in]]
 
    def func1():
        # directly from incoming Object_in socket.
        sn = bpy.context.node
        
        # safe? or return early
        if not (sn.inputs and sn.inputs[0].links):
            return

        verts = SvGetSocketAnyType(sn, sn.inputs['items_in'])
        print(verts)
 
    out_sockets = [['v', 'Verts', items_in]]
    ui_operators = [['print_names', func1]]
 
    return in_sockets, out_sockets, ui_operators
```

####Breakout Scripts
For lack of a better term, SN scripts written in this style let you pass variables to a script located in `/sverchok-master/..` or `/sverchok-master/your_module_name/some_library`. To keep your sverchok-master folder organized I recommend using a module folder. In the example below I made a folder inside sverchok-master called `sv_modules` and inside that I have a file called `sv_curve_utils`, which contains a function `loft`. This way of coding requires a bit of setup work, but then you can focus purely on the algorithm inside `loft`.
```python
from mathutils import Vector, Euler, Matrix
import sv_modules
from sv_modules.sv_curve_utils import loft
 
def sv_main(verts_p=[], edges_p=[], verts_t=[], edges_t=[]):
 
    in_sockets = [
        ['v', 'verts_p', verts_p],
        ['s', 'edges_p', edges_p],
        ['v', 'verts_t', verts_t],
        ['s', 'edges_t', edges_t]]
 
    verts_out = []

    def out_sockets():
        return [['v', 'verts_out', verts_out]]
 
    if not all([verts_p, edges_p, verts_t, edges_t]):
        return in_sockets, out_sockets()

    # while developing, it can be useful to uncomment this 
    if 'loft' in globals():
        import imp
        imp.reload(sv_modules.sv_curve_utils)
        from sv_modules.sv_curve_utils import loft
 
    verts_out = loft(verts_p[0], verts_t[0])  #  this is your break-out code
 
    # here the call to out_sockets() will pick up verts_out 
    return in_sockets, out_sockets()
```

### Techniques to improve Python performance

There are many ways to speed up python code. Some slowness will be down to innefficient algorithm design, other slowness is caused purely by how much processing is minimally required to solve a problem. A decent read regarding general methods to improve python code performancecan be found on [python.org](https://wiki.python.org/moin/PythonSpeed/PerformanceTips). If you don't know where the cycles are being consumed, then you don't know if your efforts to optimize will have any significant impact.  

### Limitations

Mostly limitations are imaginary barriers which an increase in Python Skills will bypass.

### Future

SN iteration 1 is itself a prototype and is a testing ground for iteration 2. The intention was always to provide multiple programming language interfaces, initially coffeescript because it's a lightweight language with crazy expressive capacity. iteration 2 might work a little different, perhaps working from within a class but trying to do extra introspection to reduce boilerplate.

The only reason in_sockets needs to be declared at the moment is if you want to have socket names that are different than the function arguments. It would be possible to allow `sv_main()` to take zero arguments too. So possible configurations should be:

```text

sv_main()
sv_main() + in_sockets
sv_main() + out_sockets
sv_main(a=[],..)
sv_main(a=[],..) + in_sockets
sv_main(a=[],..) + out_sockets
sv_main(a=[],..) + in_socket + out_sockets

..etc, with ui_operators optional to all combinations
```

That's it for now.

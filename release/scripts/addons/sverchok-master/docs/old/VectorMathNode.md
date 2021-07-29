## Vector Math Node

This is a versatile node. You can perform 1 operation on 1000's of list-elements, or perform operations pairwise on two lists of 1000's of elements, even if they are nested. It is therefore what we call a _Vectorized_ node, for an elaborate explanation of what this means see this [introduction]().

#### Input and Output
The node expects correct input for the chosen operation (called mode), but it will fail gracefully with a message in the console if the input is not right for the selected mode.

Some modes accept a Vector (U) and a Scalar (S), while other accepts two Vectors (U, V). Some modes will output a Scalar (out), others with output a Vector (W).

#### Modes
Most operations are self explanatory, but in case they aren't then here is a quick overview:

| Tables        | inputs | outputs | description |
| ------------- |:------:|:-----:| -----:|
| Cross product | u, v | s | u cross v |
| Dot product | u, v | s | u dot v |
| Add | u, v | w | u + v |
| Sub | u, v | w | u - v |
| Length | u | s | distance(u, origin) |
| Distance | u, v | s | distance(u, v) |
| Normalize | u | w | scale vector to length 1 |
| Negate | u | w | reverse sign of components |
| Noise Vector | u | w | [see mathutils]() |
| Noise Scalar | u | s | [see mathutils]() |
| Scalar Cell noise | u | s | [see mathutils]() |
| Vector Cell noise | u | w | [see mathutils]() |
| Project | u, v | w | u project v |
| Reflect | u, v | w | u reflect v |
| Multiply Scalar | u, s | w | multiply(vector, scalar) |
| Multiply 1/Scalar | u, s | w | multiply(vector, 1/scalar) |
| Angle Degrees | u, v | s | angle(u, origin, v) |
| Angle Radians | u, v | s | angle(u, origin, v) |
| Round s digits | u, s | v | reduce precision of components |

PolyMender program for robustly repairing a polygonal model.

Author: Tao Ju (jutao@cs.wustl.edu)

Version: 1.6 (Updated: Oct. 12, 2006)

Platform: Windows


I. What's new in v1.6:


> Removal of disconnected components

> Topologically manifold dual contouring

> Output signed octree with geometry



II. Introduction


PolyMender is based on the algorithm presented in the paper "Robust Repair of Polygonal Models" (SIGGRAPH 2004). The program reads in a polygonal model (i.e., a bag of polygons) and produces a closed surface that approximates the original model. PolyMender consumes a small amount of time and memory space, and can accurately reproduce the original geometry. PolyMender is suitable for repairing CAD models and gigantic polygonal models. Alternatively, PolyMender can also be used to generate a signed volume from any polygonal models.



III. How to run


The executable package contains three programs:

1. PolyMender, PolyMender-clean

Purpose: repairs general purpose models, such as those created from range scanners. The repaired surface is constructed using Marching Cubes. Consumes minimal memory and time and generates closed, manifold triangular surfaces. The -clean option removes isolated pieces.

2. PolyMender-qd, PolyMender-qd-clean

Purpose: same as PolyMender and PolyMender-clean, but outputs a quad-mesh.

3. PolyMender-dc, PolyMender-dc-clean

Purpose: repairs models containing sharp features, such as CAD models. The repaired surface is constructed using Dual Contouring with a manifold topology, which is capable of reproducing sharp edges and corners. However, more memory is required. Generates closed triangular and quadrilateral surfaces. The -clean option removes isolated pieces.


Type the program names (e.g., PolyMender) on the DOS prompt and you will see their usages:

Usage:   PolyMender <input_file> <octree_depth> <scale> <output_file>

Example: PolyMender bunny.ply 6 0.9 closedbunny.ply

Description:

<input_file>    Polygonal file of format STL (binary only), ASC, or PLY.

<octree_depth>  Integer depth of octree. The dimension of the volumetric
                grid is 2^<octree_depth> on each side.

<scale>         Floating point number between 0 and 1 denoting the ratio of
                the largest dimension of the model over the size of the grid.

<output_file>   Output in polygonal format PLY or signed-octree format SOF (or SOG).


Additional notes:

1. STL(binary) is preferred input format, since the program does not need to store the model in memory at all. ASC or PLY formats require additional storage of vertices, due to their topology-geometry file structure.

2. The running time and memory consumption of the program depends on several factors: the number of input polygons, the depth of the octree, and the surface area of the model (hence the number of leaf nodes on the octree). To give an idea, processing the David model with 56 million triangles at depth 13 takes 45 minutes using 500 MB RAM (excluding the mem allocated for storing vertices when reading PLY format) on a PC with AMD 1.5Hz CPU.

3. The number of output polygons can be finely controlled using the scale argument. The large the scale, the more polygons are generated, since the model occupies a larger portion of the volume grid.

4. As an alternative of output repaired models, the intermediate signed octree can be generated as a SOF or SOG file. The signed octree can be used for generating signed distance field, extracting isosurfaces, or multiresolution spatial representation of the polygonal model.


IV SOF format

SOF (Signed Octree Format) records an octree grid with signes attached to the 8 corners of each leaf node. All leaf nodes appear at the same depth that is specified by the <octree_depth> argument to the program. The tree is recorded in SOF file using pre-order traversal. Here is the structure of a SOF file (binary):

<header>

<node>

<header> is a 4-bytes integer that equals 2 ^ octree_depth. The first byte of a <node> is either 0 (denoting an intermediate node) or 1 (denoting an empty node) or 2 (denoting a leaf node). After the first byte, an intermediate node <node> contains (after the first byte) eight <node> structures for its eight children; an empty node <node> contains one byte of value 0 or 1 denoting if it is inside or outside; and a leaf node contains one byte whose eight bits correspond to the signs at its eight corners (0 for inside and 1 for outside). The order of enumeration of the eight children nodes in an intermediate nodeis the following (expressed in coordinates <x,y,z> ): <0,0,0>,<0,0,1>,<0,1,0>,<0,1,1>,<1,0,0>,<1,0,1>,<1,1,0>,<1,1,1>. The enumeration of the eight corners in a leaf node follows the same order (e.g., the lowest bit records the sign at <0,0,0>).



V SOG format

SOF (Signed Octree with Geometry) has the same data structure with SOG, with the addition of following features:

1. The file starts with a 128-byte long header. Currently, the header begins with the string "SOG.Format 1.0" followed by 3 floats representing the lower-left-near corner of the octree follwed by 1 float denoting the length of the octree (in one direction). The locations and lengths are in the input model's coordinate space. The rest of the header is left empty.

2. Each leaf node has additioanl three floats {x,y,z} (following the signs) denoting the geometric location of a feature vertex within the cell.



VI Test data

Three models are included in the testmodels package. (Suggested arguments are provided in the parathesis).

bunny.ply (octree depth: 7, scale: 0.9)

- The Stanford Bunny (containing big holes at the bottom)

horse.stl (octree depth: 8, scale: 0.9)

- The horse model with 1/3 of all polygons removed and vertices randomly perturbed.

mechanic.asc (octree depth: 6, scale: 0.9)

- A mechanic part with hanging triangles

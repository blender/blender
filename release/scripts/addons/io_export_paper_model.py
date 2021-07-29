# -*- coding: utf-8 -*-
# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software: you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation, either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but without any warranty; without even the implied warranty of
#  merchantability or fitness for a particular purpose.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program.  If not, see <http://www.gnu.org/licenses/>.
#
# ##### END GPL LICENSE BLOCK #####

bl_info = {
    "name": "Export Paper Model",
    "author": "Addam Dominec",
    "version": (0, 9),
    "blender": (2, 73, 0),
    "location": "File > Export > Paper Model",
    "warning": "",
    "description": "Export printable net of the active mesh",
    "category": "Import-Export",
    "wiki_url": "http://wiki.blender.org/index.php/Extensions:2.6/Py/"
                "Scripts/Import-Export/Paper_Model",
    "tracker_url": "https://developer.blender.org/T38441"
}

# TODO:
# sanitize the constructors Edge, Face, UVFace so that they don't edit their parent object
# The Exporter classes should take parameters as a whole pack, and parse it themselves
# remember objects selected before baking (except selected to active)
# add 'estimated number of pages' to the export UI
# profile QuickSweepline vs. BruteSweepline with/without blist: for which nets is it faster?
# rotate islands to minimize area -- and change that only if necessary to fill the page size
# Sticker.vertices should be of type Vector

# check conflicts in island naming and either:
#  * append a number to the conflicting names or
#  * enumerate faces uniquely within all islands of the same name (requires a check that both label and abbr. equals)


"""

Additional links:
    e-mail: adominec {at} gmail {dot} com

"""
import bpy
import bl_operators
import bgl
import mathutils as M
from re import compile as re_compile
from itertools import chain, repeat
from math import pi, ceil

try:
    import os.path as os_path
except ImportError:
    os_path = None

try:
    from blist import blist
except ImportError:
    blist = list

default_priority_effect = {
    'CONVEX': 0.5,
    'CONCAVE': 1,
    'LENGTH': -0.05
}

global_paper_sizes = [
    ('USER', "User defined", "User defined paper size"),
    ('A4', "A4", "International standard paper size"),
    ('A3', "A3", "International standard paper size"),
    ('US_LETTER', "Letter", "North American paper size"),
    ('US_LEGAL', "Legal", "North American paper size")
]


def first_letters(text):
    """Iterator over the first letter of each word"""
    for match in first_letters.pattern.finditer(text):
        yield text[match.start()]
first_letters.pattern = re_compile("((?<!\w)\w)|\d")


def is_upsidedown_wrong(name):
    """Tell if the string would get a different meaning if written upside down"""
    chars = set(name)
    mistakable = set("69NZMWpbqd")
    rotatable = set("80oOxXIl").union(mistakable)
    return chars.issubset(rotatable) and not chars.isdisjoint(mistakable)


def pairs(sequence):
    """Generate consecutive pairs throughout the given sequence; at last, it gives elements last, first."""
    i = iter(sequence)
    previous = first = next(i)
    for this in i:
        yield previous, this
        previous = this
    yield this, first


def argmax_pair(array, key):
    """Find an (unordered) pair of indices that maximize the given function"""
    n = len(array)
    mi, mj, m = None, None, None
    for i in range(n):
        for j in range(i+1, n):
            k = key(array[i], array[j])
            if not m or k > m:
                mi, mj, m = i, j, k
    return mi, mj


def fitting_matrix(v1, v2):
    """Get a matrix that rotates v1 to the same direction as v2"""
    return (1 / v1.length_squared) * M.Matrix((
        (v1.x*v2.x + v1.y*v2.y, v1.y*v2.x - v1.x*v2.y),
        (v1.x*v2.y - v1.y*v2.x, v1.x*v2.x + v1.y*v2.y)))


def z_up_matrix(n):
    """Get a rotation matrix that aligns given vector upwards."""
    b = n.xy.length
    s = n.length
    if b > 0:
        return M.Matrix((
            (n.x*n.z/(b*s), n.y*n.z/(b*s), -b/s),
            (-n.y/b, n.x/b, 0),
            (0, 0, 0)
        ))
    else:
        # no need for rotation
        return M.Matrix((
            (1, 0, 0),
            (0, (-1 if n.z < 0 else 1), 0),
            (0, 0, 0)
        ))


def create_blank_image(image_name, dimensions, alpha=1):
    """Create a new image and assign white color to all its pixels"""
    image_name = image_name[:64]
    width, height = int(dimensions.x), int(dimensions.y)
    image = bpy.data.images.new(image_name, width, height, alpha=True)
    if image.users > 0:
        raise UnfoldError(
            "There is something wrong with the material of the model. "
            "Please report this on the BlenderArtists forum. Export failed.")
    image.pixels = [1, 1, 1, alpha] * (width * height)
    image.file_format = 'PNG'
    return image


def bake(face_indices, uvmap, image):
    import bpy
    is_cycles = (bpy.context.scene.render.engine == 'CYCLES')
    if is_cycles:
        # please excuse the following mess. Cycles baking API does not seem to allow better.
        ob = bpy.context.active_object
        me = ob.data
        # add a disconnected image node that defines the bake target
        temp_nodes = dict()
        for mat in me.materials:
            mat.use_nodes = True
            img = mat.node_tree.nodes.new('ShaderNodeTexImage')
            img.image = image
            temp_nodes[mat] = img
            mat.node_tree.nodes.active = img
            uvmap.active = True
        # move all excess faces to negative numbers (that is the only way to disable them)
        loop = me.uv_layers[me.uv_layers.active_index].data
        face_indices = set(face_indices)
        ignored_uvs = [
            face.loop_start + i
            for face in me.polygons if face.index not in face_indices
            for i, v in enumerate(face.vertices)]
        for vid in ignored_uvs:
            loop[vid].uv *= -1
        bake_type = bpy.context.scene.cycles.bake_type
        sta = bpy.context.scene.render.bake.use_selected_to_active
        try:
            bpy.ops.object.bake(type=bake_type, margin=0, use_selected_to_active=sta, cage_extrusion=100, use_clear=False)
        except RuntimeError as e:
            raise UnfoldError(*e.args)
        finally:
            for mat, node in temp_nodes.items():
                mat.node_tree.nodes.remove(node)
        for vid in ignored_uvs:
            loop[vid].uv *= -1
    else:
        texfaces = uvmap.data
        for fid in face_indices:
            texfaces[fid].image = image
        bpy.ops.object.bake_image()
        for fid in face_indices:
            texfaces[fid].image = None


class UnfoldError(ValueError):
    pass


class Unfolder:
    def __init__(self, ob):
        self.ob = ob
        self.mesh = Mesh(ob.data, ob.matrix_world)
        self.mesh.check_correct()
        self.tex = None

    def prepare(self, cage_size=None, create_uvmap=False, mark_seams=False, priority_effect=default_priority_effect, scale=1):
        """Create the islands of the net"""
        self.mesh.generate_cuts(cage_size / scale if cage_size else None, priority_effect)
        is_landscape = cage_size and cage_size.x > cage_size.y
        self.mesh.finalize_islands(is_landscape)
        self.mesh.enumerate_islands()
        if create_uvmap:
            self.tex = self.mesh.save_uv()
        if mark_seams:
            self.mesh.mark_cuts()

    def copy_island_names(self, island_list):
        """Copy island label and abbreviation from the best matching island in the list"""
        orig_islands = [{face.id for face in item.faces} for item in island_list]
        matching = list()
        for i, island in enumerate(self.mesh.islands):
            islfaces = {uvface.face.index for uvface in island.faces}
            matching.extend((len(islfaces.intersection(item)), i, j) for j, item in enumerate(orig_islands))
        matching.sort(reverse=True)
        available_new = [True for island in self.mesh.islands]
        available_orig = [True for item in island_list]
        for face_count, i, j in matching:
            if available_new[i] and available_orig[j]:
                available_new[i] = available_orig[j] = False
                self.mesh.islands[i].label = island_list[j].label
                self.mesh.islands[i].abbreviation = island_list[j].abbreviation

    def save(self, properties):
        """Export the document"""
        # Note about scale: input is direcly in blender length
        # Mesh.scale_islands multiplies everything by a user-defined ratio
        # exporters (SVG or PDF) multiply everything by 1000 (output in millimeters)
        Exporter = SVG if properties.file_format == 'SVG' else PDF
        filepath = properties.filepath
        extension = properties.file_format.lower()
        filepath = bpy.path.ensure_ext(filepath, "." + extension)
        # page size in meters
        page_size = M.Vector((properties.output_size_x, properties.output_size_y))
        # printable area size in meters
        printable_size = page_size - 2 * properties.output_margin * M.Vector((1, 1))
        unit_scale = bpy.context.scene.unit_settings.scale_length
        ppm = properties.output_dpi * 100 / 2.54  # pixels per meter

        # after this call, all dimensions will be in meters
        self.mesh.scale_islands(unit_scale/properties.scale)
        if properties.do_create_stickers:
            self.mesh.generate_stickers(properties.sticker_width, properties.do_create_numbers)
        elif properties.do_create_numbers:
            self.mesh.generate_numbers_alone(properties.sticker_width)

        text_height = properties.sticker_width if (properties.do_create_numbers and len(self.mesh.islands) > 1) else 0
        aspect_ratio = printable_size.x / printable_size.y
        # title height must be somewhat larger that text size, glyphs go below the baseline
        self.mesh.finalize_islands(is_landscape=(printable_size.x > printable_size.y), title_height=text_height * 1.2)
        self.mesh.fit_islands(cage_size=printable_size)

        if properties.output_type != 'NONE':
            # bake an image and save it as a PNG to disk or into memory
            image_packing = properties.image_packing if properties.file_format == 'SVG' else 'ISLAND_EMBED'
            use_separate_images = image_packing in ('ISLAND_LINK', 'ISLAND_EMBED')
            tex = self.mesh.save_uv(cage_size=printable_size, separate_image=use_separate_images, tex=self.tex)
            if not tex:
                raise UnfoldError("The mesh has no UV Map slots left. Either delete a UV Map or export the net without textures.")

            sce = bpy.context.scene
            rd = sce.render
            bk = rd.bake
            if rd.engine == 'CYCLES':
                recall = sce.cycles.bake_type, bk.use_selected_to_active, bk.margin, bk.cage_extrusion, bk.use_cage, bk.use_clear, bk.use_pass_direct, bk.use_pass_indirect
                # recall use_pass...
                lookup = {'TEXTURE': 'DIFFUSE', 'AMBIENT_OCCLUSION': 'AO', 'RENDER': 'COMBINED', 'SELECTED_TO_ACTIVE': 'COMBINED'}
                sce.cycles.bake_type = lookup[properties.output_type]
                bk.use_pass_direct = bk.use_pass_indirect = (properties.output_type != 'TEXTURE')
                bk.use_selected_to_active = (properties.output_type == 'SELECTED_TO_ACTIVE')
                bk.margin, bk.cage_extrusion, bk.use_cage, bk.use_clear = 0, 10, False, False
            else:
                recall = rd.engine, rd.bake_type, rd.use_bake_to_vertex_color, rd.use_bake_selected_to_active, rd.bake_distance, rd.bake_bias, rd.bake_margin, rd.use_bake_clear
                rd.engine = 'BLENDER_RENDER'
                lookup = {'TEXTURE': 'TEXTURE', 'AMBIENT_OCCLUSION': 'AO', 'RENDER': 'FULL', 'SELECTED_TO_ACTIVE': 'FULL'}
                rd.bake_type = lookup[properties.output_type]
                rd.use_bake_selected_to_active = (properties.output_type == 'SELECTED_TO_ACTIVE')
                rd.bake_margin, rd.bake_distance, rd.bake_bias, rd.use_bake_to_vertex_color, rd.use_bake_clear = 0, 0, 0.001, False, False

            if image_packing == 'PAGE_LINK':
                self.mesh.save_image(tex, printable_size * ppm, filepath)
            elif image_packing == 'ISLAND_LINK':
                image_dir = filepath[:filepath.rfind(".")]
                self.mesh.save_separate_images(tex, ppm, image_dir)
            elif image_packing == 'ISLAND_EMBED':
                self.mesh.save_separate_images(tex, ppm, filepath, embed=Exporter.encode_image)

            # revoke settings
            if rd.engine == 'CYCLES':
                sce.cycles.bake_type, bk.use_selected_to_active, bk.margin, bk.cage_extrusion, bk.use_cage, bk.use_clear, bk.use_pass_direct, bk.use_pass_indirect = recall
            else:
                rd.engine, rd.bake_type, rd.use_bake_to_vertex_color, rd.use_bake_selected_to_active, rd.bake_distance, rd.bake_bias, rd.bake_margin, rd.use_bake_clear = recall
            if not properties.do_create_uvmap:
                tex.active = True
                bpy.ops.mesh.uv_texture_remove()

        exporter = Exporter(page_size, properties.style, properties.output_margin, (properties.output_type == 'NONE'), properties.angle_epsilon)
        exporter.do_create_stickers = properties.do_create_stickers
        exporter.text_size = properties.sticker_width
        exporter.write(self.mesh, filepath)


class Mesh:
    """Wrapper for Bpy Mesh"""

    def __init__(self, mesh, matrix):
        self.vertices = dict()
        self.edges = dict()
        self.edges_by_verts_indices = dict()
        self.faces = dict()
        self.islands = list()
        self.data = mesh
        self.pages = list()
        for bpy_vertex in mesh.vertices:
            self.vertices[bpy_vertex.index] = Vertex(bpy_vertex, matrix)
        for bpy_edge in mesh.edges:
            edge = Edge(bpy_edge, self, matrix)
            self.edges[bpy_edge.index] = edge
            self.edges_by_verts_indices[(edge.va.index, edge.vb.index)] = edge
            self.edges_by_verts_indices[(edge.vb.index, edge.va.index)] = edge
        for bpy_face in mesh.polygons:
            face = Face(bpy_face, self)
            self.faces[bpy_face.index] = face
        for edge in self.edges.values():
            edge.choose_main_faces()
            if edge.main_faces:
                edge.calculate_angle()

    def check_correct(self, epsilon=1e-6):
        """Check for invalid geometry"""
        null_edges = {i for i, e in self.edges.items() if e.vector.length < epsilon and e.faces}
        null_faces = {i for i, f in self.faces.items() if f.normal.length_squared < epsilon}
        twisted_faces = {i for i, f in self.faces.items() if f.is_twisted()}
        if not (null_edges or null_faces or twisted_faces):
            return
        bpy.context.tool_settings.mesh_select_mode = False, bool(null_edges), bool(null_faces or twisted_faces)
        for vertex in self.data.vertices:
            vertex.select = False
        for edge in self.data.edges:
            edge.select = (edge.index in null_edges)
        for face in self.data.polygons:
            face.select = (face.index in null_faces or face.index in twisted_faces)
        cure = ("Remove Doubles and Triangulate" if (null_edges or null_faces) and twisted_faces
            else "Triangulate" if twisted_faces
            else "Remove Doubles")
        raise UnfoldError(
            "The model contains:\n" +
            (" {} zero-length edge(s)\n".format(len(null_edges)) if null_edges else "") +
            (" {} zero-area face(s)\n".format(len(null_faces)) if null_faces else "") +
            (" {} twisted polygon(s)\n".format(len(twisted_faces)) if twisted_faces else "") +
            "The offenders are selected and you can use {} to fix them. Export failed.".format(cure))

    def generate_cuts(self, page_size, priority_effect):
        """Cut the mesh so that it can be unfolded to a flat net."""
        # warning: this constructor modifies its parameter (face)
        islands = {Island(face) for face in self.faces.values()}
        # check for edges that are cut permanently
        edges = [edge for edge in self.edges.values() if not edge.force_cut and len(edge.faces) > 1]

        if edges:
            average_length = sum(edge.vector.length for edge in edges) / len(edges)
            for edge in edges:
                edge.generate_priority(priority_effect, average_length)
            edges.sort(reverse=False, key=lambda edge: edge.priority)
            for edge in edges:
                if edge.vector.length_squared == 0:
                    continue
                face_a, face_b = edge.main_faces
                island_a, island_b = face_a.uvface.island, face_b.uvface.island
                if island_a is not island_b:
                    if len(island_b.faces) > len(island_a.faces):
                        island_a, island_b = island_b, island_a
                    if island_a.join(island_b, edge, size_limit=page_size):
                        islands.remove(island_b)

        self.islands = sorted(islands, reverse=True, key=lambda island: len(island.faces))

        for edge in self.edges.values():
            # some edges did not know until now whether their angle is convex or concave
            if edge.main_faces and (edge.main_faces[0].uvface.flipped or edge.main_faces[1].uvface.flipped):
                edge.calculate_angle()
            # ensure that the order of faces corresponds to the order of uvedges
            if edge.main_faces:
                reordered = [None, None]
                for uvedge in edge.uvedges:
                    try:
                        index = edge.main_faces.index(uvedge.uvface.face)
                        reordered[index] = uvedge
                    except ValueError:
                        reordered.append(uvedge)
                edge.uvedges = reordered

        for island in self.islands:
            # if the normals are ambiguous, flip them so that there are more convex edges than concave ones
            if any(uvface.flipped for uvface in island.faces):
                island_edges = {uvedge.edge for uvedge in island.edges if not uvedge.edge.is_cut(uvedge.uvface.face)}
                balance = sum((+1 if edge.angle > 0 else -1) for edge in island_edges)
                if balance < 0:
                    island.is_inside_out = True

            # construct a linked list from each island's boundary
            # uvedge.neighbor_right is clockwise = forward = via uvedge.vb if not uvface.flipped
            neighbor_lookup, conflicts = dict(), dict()
            for uvedge in island.boundary:
                uvvertex = uvedge.va if uvedge.uvface.flipped else uvedge.vb
                if uvvertex not in neighbor_lookup:
                    neighbor_lookup[uvvertex] = uvedge
                else:
                    if uvvertex not in conflicts:
                        conflicts[uvvertex] = [neighbor_lookup[uvvertex], uvedge]
                    else:
                        conflicts[uvvertex].append(uvedge)

            for uvedge in island.boundary:
                uvvertex = uvedge.vb if uvedge.uvface.flipped else uvedge.va
                if uvvertex not in conflicts:
                    # using the 'get' method so as to handle single-connected vertices properly
                    uvedge.neighbor_right = neighbor_lookup.get(uvvertex, uvedge)
                    uvedge.neighbor_right.neighbor_left = uvedge
                else:
                    conflicts[uvvertex].append(uvedge)

            # resolve merged vertices with more boundaries crossing
            def direction_to_float(vector):
                return (1 - vector.x/vector.length) if vector.y > 0 else (vector.x/vector.length - 1)
            for uvvertex, uvedges in conflicts.items():
                def is_inwards(uvedge):
                    return uvedge.uvface.flipped == (uvedge.va is uvvertex)

                def uvedge_sortkey(uvedge):
                    if is_inwards(uvedge):
                        return direction_to_float(uvedge.va.co - uvedge.vb.co)
                    else:
                        return direction_to_float(uvedge.vb.co - uvedge.va.co)

                uvedges.sort(key=uvedge_sortkey)
                for right, left in (
                        zip(uvedges[:-1:2], uvedges[1::2]) if is_inwards(uvedges[0])
                        else zip([uvedges[-1]] + uvedges[1::2], uvedges[:-1:2])):
                    left.neighbor_right = right
                    right.neighbor_left = left
        return True

    def mark_cuts(self):
        """Mark cut edges in the original mesh so that the user can see"""
        for bpy_edge in self.data.edges:
            edge = self.edges[bpy_edge.index]
            bpy_edge.use_seam = len(edge.uvedges) > 1 and edge.is_main_cut

    def generate_stickers(self, default_width, do_create_numbers=True):
        """Add sticker faces where they are needed."""
        def uvedge_priority(uvedge):
            """Retuns whether it is a good idea to stick something on this edge's face"""
            # TODO: it should take into account overlaps with faces and with other stickers
            return uvedge.uvface.face.area / sum((vb.co - va.co).length for (va, vb) in pairs(uvedge.uvface.vertices))

        def add_sticker(uvedge, index, target_island):
            uvedge.sticker = Sticker(uvedge, default_width, index, target_island)
            uvedge.island.add_marker(uvedge.sticker)

        for edge in self.edges.values():
            if edge.is_main_cut and len(edge.uvedges) >= 2 and edge.vector.length_squared > 0:
                uvedge_a, uvedge_b = edge.uvedges[:2]
                if uvedge_priority(uvedge_a) < uvedge_priority(uvedge_b):
                    uvedge_a, uvedge_b = uvedge_b, uvedge_a
                target_island = uvedge_a.island
                left_edge, right_edge = uvedge_a.neighbor_left.edge, uvedge_a.neighbor_right.edge
                if do_create_numbers:
                    for uvedge in [uvedge_b] + edge.uvedges[2:]:
                        if ((uvedge.neighbor_left.edge is not right_edge or uvedge.neighbor_right.edge is not left_edge) and
                                uvedge not in (uvedge_a.neighbor_left, uvedge_a.neighbor_right)):
                            # it will not be clear to see that these uvedges should be sticked together
                            # So, create an arrow and put the index on all stickers
                            target_island.sticker_numbering += 1
                            index = str(target_island.sticker_numbering)
                            if is_upsidedown_wrong(index):
                                index += "."
                            target_island.add_marker(Arrow(uvedge_a, default_width, index))
                            break
                    else:
                        # if all uvedges to be sticked are easy to see, create no numbers
                        index = None
                else:
                    index = None
                add_sticker(uvedge_b, index, target_island)
            elif len(edge.uvedges) > 2:
                index = None
                target_island = edge.uvedges[0].island
            if len(edge.uvedges) > 2:
                for uvedge in edge.uvedges[2:]:
                    add_sticker(uvedge, index, target_island)

    def generate_numbers_alone(self, size):
        global_numbering = 0
        for edge in self.edges.values():
            if edge.is_main_cut and len(edge.uvedges) >= 2:
                global_numbering += 1
                index = str(global_numbering)
                if is_upsidedown_wrong(index):
                    index += "."
                for uvedge in edge.uvedges:
                    uvedge.island.add_marker(NumberAlone(uvedge, index, size))

    def enumerate_islands(self):
        for num, island in enumerate(self.islands, 1):
            island.number = num
            island.generate_label()

    def scale_islands(self, scale):
        for island in self.islands:
            for point in chain((vertex.co for vertex in island.vertices), island.fake_vertices):
                point *= scale

    def finalize_islands(self, is_landscape=False, title_height=0):
        for island in self.islands:
            if title_height:
                island.title = "[{}] {}".format(island.abbreviation, island.label)
            points = list(vertex.co for vertex in island.vertices) + island.fake_vertices
            angle = M.geometry.box_fit_2d(points)
            rot = M.Matrix.Rotation(angle, 2)
            # ensure that the island matches page orientation (portrait/landscape)
            dimensions = M.Vector(max(r * v for v in points) - min(r * v for v in points) for r in rot)
            if dimensions.x > dimensions.y != is_landscape:
                rot = M.Matrix.Rotation(angle + pi / 2, 2)
            for point in points:
                # note: we need an in-place operation, and Vector.rotate() seems to work for 3d vectors only
                point[:] = rot * point
            for marker in island.markers:
                marker.rot = rot * marker.rot
            bottom_left = M.Vector((min(v.x for v in points), min(v.y for v in points) - title_height))
            for point in points:
                point -= bottom_left
            island.bounding_box = M.Vector((max(v.x for v in points), max(v.y for v in points)))

    def largest_island_ratio(self, page_size):
        return max(i / p for island in self.islands for (i, p) in zip(island.bounding_box, page_size))

    def fit_islands(self, cage_size):
        """Move islands so that they fit onto pages, based on their bounding boxes"""

        def try_emplace(island, page_islands, cage_size, stops_x, stops_y, occupied_cache):
            """Tries to put island to each pair from stops_x, stops_y
            and checks if it overlaps with any islands present on the page.
            Returns True and positions the given island on success."""
            bbox_x, bbox_y = island.bounding_box.xy
            for x in stops_x:
                if x + bbox_x > cage_size.x:
                    continue
                for y in stops_y:
                    if y + bbox_y > cage_size.y or (x, y) in occupied_cache:
                        continue
                    for i, obstacle in enumerate(page_islands):
                        # if this obstacle overlaps with the island, try another stop
                        if (x + bbox_x > obstacle.pos.x and
                                obstacle.pos.x + obstacle.bounding_box.x > x and
                                y + bbox_y > obstacle.pos.y and
                                obstacle.pos.y + obstacle.bounding_box.y > y):
                            if x >= obstacle.pos.x and y >= obstacle.pos.y:
                                occupied_cache.add((x, y))
                            # just a stupid heuristic to make subsequent searches faster
                            if i > 0:
                                page_islands[1:i+1] = page_islands[:i]
                                page_islands[0] = obstacle
                            break
                    else:
                        # if no obstacle called break, this position is okay
                        island.pos.xy = x, y
                        page_islands.append(island)
                        stops_x.append(x + bbox_x)
                        stops_y.append(y + bbox_y)
                        return True
            return False

        def drop_portion(stops, border, divisor):
            stops.sort()
            # distance from left neighbor to the right one, excluding the first stop
            distances = [right - left for left, right in zip(stops, chain(stops[2:], [border]))]
            quantile = sorted(distances)[len(distances) // divisor]
            return [stop for stop, distance in zip(stops, chain([quantile], distances)) if distance >= quantile]

        if any(island.bounding_box.x > cage_size.x or island.bounding_box.y > cage_size.y for island in self.islands):
            raise UnfoldError(
                "An island is too big to fit onto page of the given size. "
                "Either downscale the model or find and split that island manually.\n"
                "Export failed, sorry.")
        # sort islands by their diagonal... just a guess
        remaining_islands = sorted(self.islands, reverse=True, key=lambda island: island.bounding_box.length_squared)
        page_num = 1

        while remaining_islands:
            # create a new page and try to fit as many islands onto it as possible
            page = Page(page_num)
            page_num += 1
            occupied_cache = set()
            stops_x, stops_y = [0], [0]
            for island in remaining_islands:
                try_emplace(island, page.islands, cage_size, stops_x, stops_y, occupied_cache)
                # if overwhelmed with stops, drop a quarter of them
                if len(stops_x)**2 > 4 * len(self.islands) + 100:
                    stops_x = drop_portion(stops_x, cage_size.x, 4)
                    stops_y = drop_portion(stops_y, cage_size.y, 4)
            remaining_islands = [island for island in remaining_islands if island not in page.islands]
            self.pages.append(page)

    def save_uv(self, cage_size=M.Vector((1, 1)), separate_image=False, tex=None):
        # TODO: mode switching should be handled by higher-level code
        bpy.ops.object.mode_set()
        # note: assuming that the active object's data is self.mesh
        if not tex:
            tex = self.data.uv_textures.new()
            if not tex:
                return None
        tex.name = "Unfolded"
        tex.active = True
        # TODO: this is somewhat dirty, but I do not see a nicer way in the API
        loop = self.data.uv_layers[self.data.uv_layers.active_index]
        if separate_image:
            for island in self.islands:
                island.save_uv_separate(loop)
        else:
            for island in self.islands:
                island.save_uv(loop, cage_size)
        return tex

    def save_image(self, tex, page_size_pixels: M.Vector, filename):
        for page in self.pages:
            image = create_blank_image("{} {} Unfolded".format(self.data.name[:14], page.name), page_size_pixels, alpha=1)
            image.filepath_raw = page.image_path = "{}_{}.png".format(filename, page.name)
            faces = [uvface.face.index for island in page.islands for uvface in island.faces]
            bake(faces, tex, image)
            image.save()
            image.user_clear()
            bpy.data.images.remove(image)

    def save_separate_images(self, tex, scale, filepath, embed=None):
        # omitting this may cause a "Circular reference in texture stack" error
        recall = {texface: texface.image for texface in tex.data}
        for texface in tex.data:
            texface.image = None
        for i, island in enumerate(self.islands, 1):
            image_name = "{} isl{}".format(self.data.name[:15], i)
            image = create_blank_image(image_name, island.bounding_box * scale, alpha=0)
            bake([uvface.face.index for uvface in island.faces], tex, image)
            if embed:
                island.embedded_image = embed(image)
            else:
                from os import makedirs
                image_dir = filepath
                makedirs(image_dir, exist_ok=True)
                image_path = os_path.join(image_dir, "island{}.png".format(i))
                image.filepath_raw = image_path
                image.save()
                island.image_path = image_path
            image.user_clear()
            bpy.data.images.remove(image)
        for texface, img in recall.items():
            texface.image = img


class Vertex:
    """BPy Vertex wrapper"""
    __slots__ = ('index', 'co', 'edges', 'uvs')

    def __init__(self, bpy_vertex, matrix):
        self.index = bpy_vertex.index
        self.co = matrix * bpy_vertex.co
        self.edges = list()
        self.uvs = list()

    def __hash__(self):
        return hash(self.index)

    def __eq__(self, other):
        return self.index == other.index


class Edge:
    """Wrapper for BPy Edge"""
    __slots__ = ('va', 'vb', 'faces', 'main_faces', 'uvedges',
        'vector', 'angle',
        'is_main_cut', 'force_cut', 'priority', 'freestyle')

    def __init__(self, edge, mesh, matrix=1):
        self.va = mesh.vertices[edge.vertices[0]]
        self.vb = mesh.vertices[edge.vertices[1]]
        self.vector = self.vb.co - self.va.co
        self.faces = list()
        # if self.main_faces is set, then self.uvedges[:2] must correspond to self.main_faces, in their order
        # this constraint is assured at the time of finishing mesh.generate_cuts
        self.uvedges = list()

        self.force_cut = edge.use_seam  # such edges will always be cut
        self.main_faces = None  # two faces that may be connected in the island
        # is_main_cut defines whether the two main faces are connected
        # all the others will be assumed to be cut
        self.is_main_cut = True
        self.priority = None
        self.angle = None
        self.freestyle = getattr(edge, "use_freestyle_mark", False)  # freestyle edges will be highlighted
        self.va.edges.append(self)  # FIXME: editing foreign attribute
        self.vb.edges.append(self)  # FIXME: editing foreign attribute

    def choose_main_faces(self):
        """Choose two main faces that might get connected in an island"""
        if len(self.faces) == 2:
            self.main_faces = self.faces
        elif len(self.faces) > 2:
            # find (with brute force) the pair of indices whose faces have the most similar normals
            i, j = argmax_pair(self.faces, key=lambda a, b: abs(a.normal.dot(b.normal)))
            self.main_faces = [self.faces[i], self.faces[j]]

    def calculate_angle(self):
        """Calculate the angle between the main faces"""
        face_a, face_b = self.main_faces
        if face_a.normal.length_squared == 0 or face_b.normal.length_squared == 0:
            self.angle = -3  # just a very sharp angle
            return
        # correction if normals are flipped
        a_is_clockwise = ((face_a.vertices.index(self.va) - face_a.vertices.index(self.vb)) % len(face_a.vertices) == 1)
        b_is_clockwise = ((face_b.vertices.index(self.va) - face_b.vertices.index(self.vb)) % len(face_b.vertices) == 1)
        is_equal_flip = True
        if face_a.uvface and face_b.uvface:
            a_is_clockwise ^= face_a.uvface.flipped
            b_is_clockwise ^= face_b.uvface.flipped
            is_equal_flip = (face_a.uvface.flipped == face_b.uvface.flipped)
            # TODO: maybe this need not be true in _really_ ugly cases: assert(a_is_clockwise != b_is_clockwise)
        if a_is_clockwise != b_is_clockwise:
            if (a_is_clockwise == (face_b.normal.cross(face_a.normal).dot(self.vector) > 0)) == is_equal_flip:
                # the angle is convex
                self.angle = face_a.normal.angle(face_b.normal)
            else:
                # the angle is concave
                self.angle = -face_a.normal.angle(face_b.normal)
        else:
            # normals are flipped, so we know nothing
            # so let us assume the angle be convex
            self.angle = face_a.normal.angle(-face_b.normal)

    def generate_priority(self, priority_effect, average_length):
        """Calculate the priority value for cutting"""
        angle = self.angle
        if angle > 0:
            self.priority = priority_effect['CONVEX'] * angle / pi
        else:
            self.priority = priority_effect['CONCAVE'] * (-angle) / pi
        self.priority += (self.vector.length / average_length) * priority_effect['LENGTH']

    def is_cut(self, face):
        """Return False if this edge will the given face to another one in the resulting net
        (useful for edges with more than two faces connected)"""
        # Return whether there is a cut between the two main faces
        if self.main_faces and face in self.main_faces:
            return self.is_main_cut
        # All other faces (third and more) are automatically treated as cut
        else:
            return True

    def other_uvedge(self, this):
        """Get an uvedge of this edge that is not the given one
        causes an IndexError if case of less than two adjacent edges"""
        return self.uvedges[1] if this is self.uvedges[0] else self.uvedges[0]


class Face:
    """Wrapper for BPy Face"""
    __slots__ = ('index', 'edges', 'vertices', 'uvface',
        'loop_start', 'area', 'normal')

    def __init__(self, bpy_face, mesh):
        self.index = bpy_face.index
        self.edges = list()
        self.vertices = [mesh.vertices[i] for i in bpy_face.vertices]
        self.loop_start = bpy_face.loop_start
        self.area = bpy_face.area
        self.uvface = None
        self.normal = M.geometry.normal(v.co for v in self.vertices)
        for verts_indices in bpy_face.edge_keys:
            edge = mesh.edges_by_verts_indices[verts_indices]
            self.edges.append(edge)
            edge.faces.append(self)  # FIXME: editing foreign attribute

    def is_twisted(self):
        if len(self.vertices) > 3:
            center = sum((vertex.co for vertex in self.vertices), M.Vector((0, 0, 0))) / len(self.vertices)
            plane_d = center.dot(self.normal)
            diameter = max((center - vertex.co).length for vertex in self.vertices)
            for vertex in self.vertices:
                # check coplanarity
                if abs(vertex.co.dot(self.normal) - plane_d) > diameter * 0.01:
                    return True
        return False

    def __hash__(self):
        return hash(self.index)


class Island:
    """Part of the net to be exported"""
    __slots__ = ('faces', 'edges', 'vertices', 'fake_vertices', 'uvverts_by_id', 'boundary', 'markers',
        'pos', 'bounding_box',
        'image_path', 'embedded_image',
        'number', 'label', 'abbreviation', 'title',
        'has_safe_geometry', 'is_inside_out',
        'sticker_numbering')

    def __init__(self, face):
        """Create an Island from a single Face"""
        self.faces = list()
        self.edges = set()
        self.vertices = set()
        self.fake_vertices = list()
        self.markers = list()
        self.label = None
        self.abbreviation = None
        self.title = None
        self.pos = M.Vector((0, 0))
        self.image_path = None
        self.embedded_image = None
        self.is_inside_out = False  # swaps concave <-> convex edges
        self.has_safe_geometry = True
        self.sticker_numbering = 0
        uvface = UVFace(face, self)
        self.vertices.update(uvface.vertices)
        self.edges.update(uvface.edges)
        self.faces.append(uvface)
        # speedup for Island.join
        self.uvverts_by_id = {uvvertex.vertex.index: [uvvertex] for uvvertex in self.vertices}
        # UVEdges on the boundary
        self.boundary = list(self.edges)

    def join(self, other, edge: Edge, size_limit=None, epsilon=1e-6) -> bool:
        """
        Try to join other island on given edge
        Returns False if they would overlap
        """

        class Intersection(Exception):
            pass

        class GeometryError(Exception):
            pass

        def is_below(self, other, correct_geometry=True):
            if self is other:
                return False
            if self.top < other.bottom:
                return True
            if other.top < self.bottom:
                return False
            if self.max.tup <= other.min.tup:
                return True
            if other.max.tup <= self.min.tup:
                return False
            self_vector = self.max.co - self.min.co
            min_to_min = other.min.co - self.min.co
            cross_b1 = self_vector.cross(min_to_min)
            cross_b2 = self_vector.cross(other.max.co - self.min.co)
            if cross_b2 < cross_b1:
                cross_b1, cross_b2 = cross_b2, cross_b1
            if cross_b2 > 0 and (cross_b1 > 0 or (cross_b1 == 0 and not self.is_uvface_upwards())):
                return True
            if cross_b1 < 0 and (cross_b2 < 0 or (cross_b2 == 0 and self.is_uvface_upwards())):
                return False
            other_vector = other.max.co - other.min.co
            cross_a1 = other_vector.cross(-min_to_min)
            cross_a2 = other_vector.cross(self.max.co - other.min.co)
            if cross_a2 < cross_a1:
                cross_a1, cross_a2 = cross_a2, cross_a1
            if cross_a2 > 0 and (cross_a1 > 0 or (cross_a1 == 0 and not other.is_uvface_upwards())):
                return False
            if cross_a1 < 0 and (cross_a2 < 0 or (cross_a2 == 0 and other.is_uvface_upwards())):
                return True
            if cross_a1 == cross_b1 == cross_a2 == cross_b2 == 0:
                if correct_geometry:
                    raise GeometryError
                elif self.is_uvface_upwards() == other.is_uvface_upwards():
                    raise Intersection
                return False
            if self.min.tup == other.min.tup or self.max.tup == other.max.tup:
                return cross_a2 > cross_b2
            raise Intersection

        class QuickSweepline:
            """Efficient sweepline based on binary search, checking neighbors only"""
            def __init__(self):
                self.children = blist()

            def add(self, item, cmp=is_below):
                low, high = 0, len(self.children)
                while low < high:
                    mid = (low + high) // 2
                    if cmp(self.children[mid], item):
                        low = mid + 1
                    else:
                        high = mid
                self.children.insert(low, item)

            def remove(self, item, cmp=is_below):
                index = self.children.index(item)
                self.children.pop(index)
                if index > 0 and index < len(self.children):
                    # check for intersection
                    if cmp(self.children[index], self.children[index-1]):
                        raise GeometryError

        class BruteSweepline:
            """Safe sweepline which checks all its members pairwise"""
            def __init__(self):
                self.children = set()
                self.last_min = None, []
                self.last_max = None, []

            def add(self, item, cmp=is_below):
                for child in self.children:
                    if child.min is not item.min and child.max is not item.max:
                        cmp(item, child, False)
                self.children.add(item)

            def remove(self, item):
                self.children.remove(item)

        def sweep(sweepline, segments):
            """Sweep across the segments and raise an exception if necessary"""
            # careful, 'segments' may be a use-once iterator
            events_add = sorted(segments, reverse=True, key=lambda uvedge: uvedge.min.tup)
            events_remove = sorted(events_add, reverse=True, key=lambda uvedge: uvedge.max.tup)
            while events_remove:
                while events_add and events_add[-1].min.tup <= events_remove[-1].max.tup:
                    sweepline.add(events_add.pop())
                sweepline.remove(events_remove.pop())

        def root_find(value, tree):
            """Find the root of a given value in a forest-like dictionary
            also updates the dictionary using path compression"""
            parent, relink = tree.get(value), list()
            while parent is not None:
                relink.append(value)
                value, parent = parent, tree.get(parent)
            tree.update(dict.fromkeys(relink, value))
            return value

        def slope_from(position):
            def slope(uvedge):
                vec = (uvedge.vb.co - uvedge.va.co) if uvedge.va.tup == position else (uvedge.va.co - uvedge.vb.co)
                return (vec.y / vec.length + 1) if ((vec.x, vec.y) > (0, 0)) else (-1 - vec.y / vec.length)
            return slope

        # find edge in other and in self
        for uvedge in edge.uvedges:
            if uvedge.uvface.face in uvedge.edge.main_faces:
                if uvedge.uvface.island is self and uvedge in self.boundary:
                    uvedge_a = uvedge
                elif uvedge.uvface.island is other and uvedge in other.boundary:
                    uvedge_b = uvedge
                else:
                    return False

        # check if vertices and normals are aligned correctly
        verts_flipped = uvedge_b.va.vertex is uvedge_a.va.vertex
        flipped = verts_flipped ^ uvedge_a.uvface.flipped ^ uvedge_b.uvface.flipped
        # determine rotation
        # NOTE: if the edges differ in length, the matrix will involve uniform scaling.
        # Such situation may occur in the case of twisted n-gons
        first_b, second_b = (uvedge_b.va, uvedge_b.vb) if not verts_flipped else (uvedge_b.vb, uvedge_b.va)
        if not flipped:
            rot = fitting_matrix(first_b.co - second_b.co, uvedge_a.vb.co - uvedge_a.va.co)
        else:
            flip = M.Matrix(((-1, 0), (0, 1)))
            rot = fitting_matrix(flip * (first_b.co - second_b.co), uvedge_a.vb.co - uvedge_a.va.co) * flip
        trans = uvedge_a.vb.co - rot * first_b.co
        # extract and transform island_b's boundary
        phantoms = {uvvertex: UVVertex(rot*uvvertex.co + trans, uvvertex.vertex) for uvvertex in other.vertices}

        # check the size of the resulting island
        if size_limit:
            # first check: bounding box
            left = min(min(seg.min.co.x for seg in self.boundary), min(vertex.co.x for vertex in phantoms))
            right = max(max(seg.max.co.x for seg in self.boundary), max(vertex.co.x for vertex in phantoms))
            bottom = min(min(seg.bottom for seg in self.boundary), min(vertex.co.y for vertex in phantoms))
            top = max(max(seg.top for seg in self.boundary), max(vertex.co.y for vertex in phantoms))
            bbox_width = right - left
            bbox_height = top - bottom
            if min(bbox_width, bbox_height)**2 > size_limit.x**2 + size_limit.y**2:
                return False
            if (bbox_width > size_limit.x or bbox_height > size_limit.y) and (bbox_height > size_limit.x or bbox_width > size_limit.y):
                # further checks (TODO!)
                # for the time being, just throw this piece away
                return False

        distance_limit = edge.vector.length_squared * epsilon
        # try and merge UVVertices closer than sqrt(distance_limit)
        merged_uvedges = set()
        merged_uvedge_pairs = list()

        # merge all uvvertices that are close enough using a union-find structure
        # uvvertices will be merged only in cases other->self and self->self
        # all resulting groups are merged together to a uvvertex of self
        is_merged_mine = False
        shared_vertices = self.uvverts_by_id.keys() & other.uvverts_by_id.keys()
        for vertex_id in shared_vertices:
            uvs = self.uvverts_by_id[vertex_id] + other.uvverts_by_id[vertex_id]
            len_mine = len(self.uvverts_by_id[vertex_id])
            merged = dict()
            for i, a in enumerate(uvs[:len_mine]):
                i = root_find(i, merged)
                for j, b in enumerate(uvs[i+1:], i+1):
                    b = b if j < len_mine else phantoms[b]
                    j = root_find(j, merged)
                    if i == j:
                        continue
                    i, j = (j, i) if j < i else (i, j)
                    if (a.co - b.co).length_squared < distance_limit:
                        merged[j] = i
            for source, target in merged.items():
                target = root_find(target, merged)
                phantoms[uvs[source]] = uvs[target]
                is_merged_mine |= (source < len_mine)  # remember that a vertex of this island has been merged

        for uvedge in (chain(self.boundary, other.boundary) if is_merged_mine else other.boundary):
            for partner in uvedge.edge.uvedges:
                if partner is not uvedge:
                    paired_a, paired_b = phantoms.get(partner.vb, partner.vb), phantoms.get(partner.va, partner.va)
                    if (partner.uvface.flipped ^ flipped) != uvedge.uvface.flipped:
                        paired_a, paired_b = paired_b, paired_a
                    if phantoms.get(uvedge.va, uvedge.va) is paired_a and phantoms.get(uvedge.vb, uvedge.vb) is paired_b:
                        # if these two edges will get merged, add them both to the set
                        merged_uvedges.update((uvedge, partner))
                        merged_uvedge_pairs.append((uvedge, partner))
                        break

        if uvedge_b not in merged_uvedges:
            raise UnfoldError("Export failed. Please report this error, including the model if you can.")

        boundary_other = [
            PhantomUVEdge(phantoms[uvedge.va], phantoms[uvedge.vb], flipped ^ uvedge.uvface.flipped)
            for uvedge in other.boundary if uvedge not in merged_uvedges]
        # TODO: if is_merged_mine, it might make sense to create a similar list from self.boundary as well

        incidence = {vertex.tup for vertex in phantoms.values()}.intersection(vertex.tup for vertex in self.vertices)
        incidence = {position: list() for position in incidence}  # from now on, 'incidence' is a dict
        for uvedge in chain(boundary_other, self.boundary):
            if uvedge.va.co == uvedge.vb.co:
                continue
            for vertex in (uvedge.va, uvedge.vb):
                site = incidence.get(vertex.tup)
                if site is not None:
                    site.append(uvedge)
        for position, segments in incidence.items():
            if len(segments) <= 2:
                continue
            segments.sort(key=slope_from(position))
            for right, left in pairs(segments):
                is_left_ccw = left.is_uvface_upwards() ^ (left.max.tup == position)
                is_right_ccw = right.is_uvface_upwards() ^ (right.max.tup == position)
                if is_right_ccw and not is_left_ccw and type(right) is not type(left) and right not in merged_uvedges and left not in merged_uvedges:
                    return False
                if (not is_right_ccw and right not in merged_uvedges) ^ (is_left_ccw and left not in merged_uvedges):
                    return False

        # check for self-intersections
        try:
            try:
                sweepline = QuickSweepline() if self.has_safe_geometry and other.has_safe_geometry else BruteSweepline()
                sweep(sweepline, (uvedge for uvedge in chain(boundary_other, self.boundary)))
                self.has_safe_geometry &= other.has_safe_geometry
            except GeometryError:
                sweep(BruteSweepline(), (uvedge for uvedge in chain(boundary_other, self.boundary)))
                self.has_safe_geometry = False
        except Intersection:
            return False

        # mark all edges that connect the islands as not cut
        for uvedge in merged_uvedges:
            uvedge.edge.is_main_cut = False

        # include all trasformed vertices as mine
        self.vertices.update(phantoms.values())

        # update the uvverts_by_id dictionary
        for source, target in phantoms.items():
            present = self.uvverts_by_id.get(target.vertex.index)
            if not present:
                self.uvverts_by_id[target.vertex.index] = [target]
            else:
                # emulation of set behavior... sorry, it is faster
                if source in present:
                    present.remove(source)
                if target not in present:
                    present.append(target)

        # re-link uvedges and uvfaces to their transformed locations
        for uvedge in other.edges:
            uvedge.island = self
            uvedge.va = phantoms[uvedge.va]
            uvedge.vb = phantoms[uvedge.vb]
            uvedge.update()
        if is_merged_mine:
            for uvedge in self.edges:
                uvedge.va = phantoms.get(uvedge.va, uvedge.va)
                uvedge.vb = phantoms.get(uvedge.vb, uvedge.vb)
        self.edges.update(other.edges)

        for uvface in other.faces:
            uvface.island = self
            uvface.vertices = [phantoms[uvvertex] for uvvertex in uvface.vertices]
            uvface.uvvertex_by_id = {
                index: phantoms[uvvertex]
                for index, uvvertex in uvface.uvvertex_by_id.items()}
            uvface.flipped ^= flipped
        if is_merged_mine:
            # there may be own uvvertices that need to be replaced by phantoms
            for uvface in self.faces:
                if any(uvvertex in phantoms for uvvertex in uvface.vertices):
                    uvface.vertices = [phantoms.get(uvvertex, uvvertex) for uvvertex in uvface.vertices]
                    uvface.uvvertex_by_id = {
                        index: phantoms.get(uvvertex, uvvertex)
                        for index, uvvertex in uvface.uvvertex_by_id.items()}
        self.faces.extend(other.faces)

        self.boundary = [
            uvedge for uvedge in chain(self.boundary, other.boundary)
            if uvedge not in merged_uvedges]

        for uvedge, partner in merged_uvedge_pairs:
            # make sure that main faces are the ones actually merged (this changes nothing in most cases)
            uvedge.edge.main_faces[:] = uvedge.uvface.face, partner.uvface.face

        # everything seems to be OK
        return True

    def add_marker(self, marker):
        self.fake_vertices.extend(marker.bounds)
        self.markers.append(marker)

    def generate_label(self, label=None, abbreviation=None):
        """Assign a name to this island automatically"""
        abbr = abbreviation or self.abbreviation or str(self.number)
        # TODO: dots should be added in the last instant when outputting any text
        if is_upsidedown_wrong(abbr):
            abbr += "."
        self.label = label or self.label or "Island {}".format(self.number)
        self.abbreviation = abbr

    def save_uv(self, tex, cage_size):
        """Save UV Coordinates of all UVFaces to a given UV texture
        tex: UV Texture layer to use (BPy MeshUVLoopLayer struct)
        page_size: size of the page in pixels (vector)"""
        texface = tex.data
        for uvface in self.faces:
            for i, uvvertex in enumerate(uvface.vertices):
                uv = uvvertex.co + self.pos
                texface[uvface.face.loop_start + i].uv[0] = uv.x / cage_size.x
                texface[uvface.face.loop_start + i].uv[1] = uv.y / cage_size.y

    def save_uv_separate(self, tex):
        """Save UV Coordinates of all UVFaces to a given UV texture, spanning from 0 to 1
        tex: UV Texture layer to use (BPy MeshUVLoopLayer struct)
        page_size: size of the page in pixels (vector)"""
        texface = tex.data
        scale_x, scale_y = 1 / self.bounding_box.x, 1 / self.bounding_box.y
        for uvface in self.faces:
            for i, uvvertex in enumerate(uvface.vertices):
                texface[uvface.face.loop_start + i].uv[0] = uvvertex.co.x * scale_x
                texface[uvface.face.loop_start + i].uv[1] = uvvertex.co.y * scale_y


class Page:
    """Container for several Islands"""
    __slots__ = ('islands', 'name', 'image_path')

    def __init__(self, num=1):
        self.islands = list()
        self.name = "page{}".format(num)
        self.image_path = None


class UVVertex:
    """Vertex in 2D"""
    __slots__ = ('co', 'vertex', 'tup')

    def __init__(self, vector, vertex=None):
        self.co = vector.xy
        self.vertex = vertex
        self.tup = tuple(self.co)

    def __repr__(self):
        if self.vertex:
            return "UV {} [{:.3f}, {:.3f}]".format(self.vertex.index, self.co.x, self.co.y)
        else:
            return "UV * [{:.3f}, {:.3f}]".format(self.co.x, self.co.y)


class UVEdge:
    """Edge in 2D"""
    # Every UVEdge is attached to only one UVFace
    # UVEdges are doubled as needed because they both have to point clockwise around their faces
    __slots__ = ('va', 'vb', 'island', 'uvface', 'edge',
        'min', 'max', 'bottom', 'top',
        'neighbor_left', 'neighbor_right', 'sticker')

    def __init__(self, vertex1: UVVertex, vertex2: UVVertex, island: Island, uvface, edge):
        self.va = vertex1
        self.vb = vertex2
        self.update()
        self.island = island
        self.uvface = uvface
        self.sticker = None
        self.edge = edge

    def update(self):
        """Update data if UVVertices have moved"""
        self.min, self.max = (self.va, self.vb) if (self.va.tup < self.vb.tup) else (self.vb, self.va)
        y1, y2 = self.va.co.y, self.vb.co.y
        self.bottom, self.top = (y1, y2) if y1 < y2 else (y2, y1)

    def is_uvface_upwards(self):
        return (self.va.tup < self.vb.tup) ^ self.uvface.flipped

    def __repr__(self):
        return "({0.va} - {0.vb})".format(self)


class PhantomUVEdge:
    """Temporary 2D Segment for calculations"""
    __slots__ = ('va', 'vb', 'min', 'max', 'bottom', 'top')

    def __init__(self, vertex1: UVVertex, vertex2: UVVertex, flip):
        self.va, self.vb = (vertex2, vertex1) if flip else (vertex1, vertex2)
        self.min, self.max = (self.va, self.vb) if (self.va.tup < self.vb.tup) else (self.vb, self.va)
        y1, y2 = self.va.co.y, self.vb.co.y
        self.bottom, self.top = (y1, y2) if y1 < y2 else (y2, y1)

    def is_uvface_upwards(self):
        return self.va.tup < self.vb.tup

    def __repr__(self):
        return "[{0.va} - {0.vb}]".format(self)


class UVFace:
    """Face in 2D"""
    __slots__ = ('vertices', 'edges', 'face', 'island', 'flipped', 'uvvertex_by_id')

    def __init__(self, face: Face, island: Island):
        """Creace an UVFace from a Face and a fixed edge.
        face: Face to take coordinates from
        island: Island to register itself in
        fixed_edge: Edge to connect to (that already has UV coordinates)"""
        self.vertices = list()
        self.face = face
        face.uvface = self
        self.island = island
        self.flipped = False  # a flipped UVFace has edges clockwise

        rot = z_up_matrix(face.normal)
        self.uvvertex_by_id = dict()  # link vertex id -> UVVertex
        for vertex in face.vertices:
            uvvertex = UVVertex(rot * vertex.co, vertex)
            self.vertices.append(uvvertex)
            self.uvvertex_by_id[vertex.index] = uvvertex
        self.edges = list()
        edge_by_verts = dict()
        for edge in face.edges:
            edge_by_verts[(edge.va.index, edge.vb.index)] = edge
            edge_by_verts[(edge.vb.index, edge.va.index)] = edge
        for va, vb in pairs(self.vertices):
            edge = edge_by_verts[(va.vertex.index, vb.vertex.index)]
            uvedge = UVEdge(va, vb, island, self, edge)
            self.edges.append(uvedge)
            edge.uvedges.append(uvedge)  # FIXME: editing foreign attribute


class Arrow:
    """Mark in the document: an arrow denoting the number of the edge it points to"""
    __slots__ = ('bounds', 'center', 'rot', 'text', 'size')

    def __init__(self, uvedge, size, index):
        self.text = str(index)
        edge = (uvedge.vb.co - uvedge.va.co) if not uvedge.uvface.flipped else (uvedge.va.co - uvedge.vb.co)
        self.center = (uvedge.va.co + uvedge.vb.co) / 2
        self.size = size
        tangent = edge.normalized()
        cos, sin = tangent
        self.rot = M.Matrix(((cos, -sin), (sin, cos)))
        normal = M.Vector((sin, -cos))
        self.bounds = [self.center, self.center + (1.2*normal + tangent)*size, self.center + (1.2*normal - tangent)*size]


class Sticker:
    """Mark in the document: sticker tab"""
    __slots__ = ('bounds', 'center', 'rot', 'text', 'width', 'vertices')

    def __init__(self, uvedge, default_width=0.005, index=None, target_island=None):
        """Sticker is directly attached to the given UVEdge"""
        first_vertex, second_vertex = (uvedge.va, uvedge.vb) if not uvedge.uvface.flipped else (uvedge.vb, uvedge.va)
        edge = first_vertex.co - second_vertex.co
        sticker_width = min(default_width, edge.length / 2)
        other = uvedge.edge.other_uvedge(uvedge)  # This is the other uvedge - the sticking target

        other_first, other_second = (other.va, other.vb) if not other.uvface.flipped else (other.vb, other.va)
        other_edge = other_second.co - other_first.co

        # angle a is at vertex uvedge.va, b is at uvedge.vb
        cos_a = cos_b = 0.5
        sin_a = sin_b = 0.75**0.5
        # len_a is length of the side adjacent to vertex a, len_b likewise
        len_a = len_b = sticker_width / sin_a

        # fix overlaps with the most often neighbour - its sticking target
        if first_vertex == other_second:
            cos_a = max(cos_a, (edge*other_edge) / (edge.length**2))  # angles between pi/3 and 0
        elif second_vertex == other_first:
            cos_b = max(cos_b, (edge*other_edge) / (edge.length**2))  # angles between pi/3 and 0

        # Fix tabs for sticking targets with small angles
        # Index of other uvedge in its face (not in its island)
        other_idx = other.uvface.edges.index(other)
        # Left and right neighbors in the face
        other_face_neighbor_left = other.uvface.edges[(other_idx+1) % len(other.uvface.edges)]
        other_face_neighbor_right = other.uvface.edges[(other_idx-1) % len(other.uvface.edges)]
        other_edge_neighbor_a = other_face_neighbor_left.vb.co - other.vb.co
        other_edge_neighbor_b = other_face_neighbor_right.va.co - other.va.co
        # Adjacent angles in the face
        cos_a = max(cos_a, (-other_edge*other_edge_neighbor_a) / (other_edge.length*other_edge_neighbor_a.length))
        cos_b = max(cos_b, (other_edge*other_edge_neighbor_b) / (other_edge.length*other_edge_neighbor_b.length))

        # Calculate the lengths of the glue tab edges using the possibly smaller angles
        sin_a = abs(1 - cos_a**2)**0.5
        len_b = min(len_a, (edge.length * sin_a) / (sin_a * cos_b + sin_b * cos_a))
        len_a = 0 if sin_a == 0 else min(sticker_width / sin_a, (edge.length - len_b*cos_b) / cos_a)

        sin_b = abs(1 - cos_b**2)**0.5
        len_a = min(len_a, (edge.length * sin_b) / (sin_a * cos_b + sin_b * cos_a))
        len_b = 0 if sin_b == 0 else min(sticker_width / sin_b, (edge.length - len_a * cos_a) / cos_b)

        v3 = UVVertex(second_vertex.co + M.Matrix(((cos_b, -sin_b), (sin_b, cos_b))) * edge * len_b / edge.length)
        v4 = UVVertex(first_vertex.co + M.Matrix(((-cos_a, -sin_a), (sin_a, -cos_a))) * edge * len_a / edge.length)
        if v3.co != v4.co:
            self.vertices = [second_vertex, v3, v4, first_vertex]
        else:
            self.vertices = [second_vertex, v3, first_vertex]

        sin, cos = edge.y / edge.length, edge.x / edge.length
        self.rot = M.Matrix(((cos, -sin), (sin, cos)))
        self.width = sticker_width * 0.9
        if index and target_island is not uvedge.island:
            self.text = "{}:{}".format(target_island.abbreviation, index)
        else:
            self.text = index
        self.center = (uvedge.va.co + uvedge.vb.co) / 2 + self.rot*M.Vector((0, self.width*0.2))
        self.bounds = [v3.co, v4.co, self.center] if v3.co != v4.co else [v3.co, self.center]


class NumberAlone:
    """Mark in the document: numbering inside the island denoting edges to be sticked"""
    __slots__ = ('bounds', 'center', 'rot', 'text', 'size')

    def __init__(self, uvedge, index, default_size=0.005):
        """Sticker is directly attached to the given UVEdge"""
        edge = (uvedge.va.co - uvedge.vb.co) if not uvedge.uvface.flipped else (uvedge.vb.co - uvedge.va.co)

        self.size = default_size
        sin, cos = edge.y / edge.length, edge.x / edge.length
        self.rot = M.Matrix(((cos, -sin), (sin, cos)))
        self.text = index
        self.center = (uvedge.va.co + uvedge.vb.co) / 2 - self.rot*M.Vector((0, self.size*1.2))
        self.bounds = [self.center]


class SVG:
    """Simple SVG exporter"""

    def __init__(self, page_size: M.Vector, style, margin, pure_net=True, angle_epsilon=0.01):
        """Initialize document settings.
        page_size: document dimensions in meters
        pure_net: if True, do not use image"""
        self.page_size = page_size
        self.pure_net = pure_net
        self.style = style
        self.margin = margin
        self.text_size = 12
        self.angle_epsilon = angle_epsilon

    @classmethod
    def encode_image(cls, bpy_image):
        import tempfile
        import base64
        with tempfile.TemporaryDirectory() as directory:
            filename = directory + "/i.png"
            bpy_image.filepath_raw = filename
            bpy_image.save()
            return base64.encodebytes(open(filename, "rb").read()).decode('ascii')

    def format_vertex(self, vector, pos=M.Vector((0, 0))):
        """Return a string with both coordinates of the given vertex."""
        x, y = vector + pos
        return "{:.6f} {:.6f}".format((x + self.margin) * 1000, (self.page_size.y - y - self.margin) * 1000)

    def write(self, mesh, filename):
        """Write data to a file given by its name."""
        line_through = " L ".join  # used for formatting of SVG path data
        rows = "\n".join

        dl = ["{:.2f}".format(length * self.style.line_width * 1000) for length in (2, 5, 10)]
        format_style = {
            'SOLID': "none", 'DOT': "{0},{1}".format(*dl), 'DASH': "{1},{2}".format(*dl),
            'LONGDASH': "{2},{1}".format(*dl), 'DASHDOT': "{2},{1},{0},{1}".format(*dl)}

        def format_color(vec):
            return "#{:02x}{:02x}{:02x}".format(round(vec[0] * 255), round(vec[1] * 255), round(vec[2] * 255))

        def format_matrix(matrix):
            return " ".join("{:.6f}".format(cell) for column in matrix for cell in column)

        def path_convert(string, relto=os_path.dirname(filename)):
            assert(os_path)  # check the module was imported
            string = os_path.relpath(string, relto)
            if os_path.sep != '/':
                string = string.replace(os_path.sep, '/')
            return string

        styleargs = {
            name: format_color(getattr(self.style, name)) for name in (
                "outer_color", "outbg_color", "convex_color", "concave_color", "freestyle_color",
                "inbg_color", "sticker_fill", "text_color")}
        styleargs.update({
            name: format_style[getattr(self.style, name)] for name in
            ("outer_style", "convex_style", "concave_style", "freestyle_style")})
        styleargs.update({
            name: getattr(self.style, attr)[3] for name, attr in (
                ("outer_alpha", "outer_color"), ("outbg_alpha", "outbg_color"),
                ("convex_alpha", "convex_color"), ("concave_alpha", "concave_color"),
                ("freestyle_alpha", "freestyle_color"),
                ("inbg_alpha", "inbg_color"), ("sticker_alpha", "sticker_fill"),
                ("text_alpha", "text_color"))})
        styleargs.update({
            name: getattr(self.style, name) * self.style.line_width * 1000 for name in
            ("outer_width", "convex_width", "concave_width", "freestyle_width", "outbg_width", "inbg_width")})
        for num, page in enumerate(mesh.pages):
            page_filename = "{}_{}.svg".format(filename[:filename.rfind(".svg")], page.name) if len(mesh.pages) > 1 else filename
            with open(page_filename, 'w') as f:
                print(self.svg_base.format(width=self.page_size.x*1000, height=self.page_size.y*1000), file=f)
                print(self.css_base.format(**styleargs), file=f)
                if page.image_path:
                    print(
                        self.image_linked_tag.format(
                            pos="{0:.6f} {0:.6f}".format(self.margin*1000),
                            width=(self.page_size.x - 2 * self.margin)*1000,
                            height=(self.page_size.y - 2 * self.margin)*1000,
                            path=path_convert(page.image_path)),
                        file=f)
                if len(page.islands) > 1:
                    print("<g>", file=f)

                for island in page.islands:
                    print("<g>", file=f)
                    if island.image_path:
                        print(
                            self.image_linked_tag.format(
                                pos=self.format_vertex(island.pos + M.Vector((0, island.bounding_box.y))),
                                width=island.bounding_box.x*1000,
                                height=island.bounding_box.y*1000,
                                path=path_convert(island.image_path)),
                            file=f)
                    elif island.embedded_image:
                        print(
                            self.image_embedded_tag.format(
                                pos=self.format_vertex(island.pos + M.Vector((0, island.bounding_box.y))),
                                width=island.bounding_box.x*1000,
                                height=island.bounding_box.y*1000,
                                path=island.image_path),
                            island.embedded_image, "'/>",
                            file=f, sep="")
                    if island.title:
                        print(
                            self.text_tag.format(
                                size=1000 * self.text_size,
                                x=1000 * (island.bounding_box.x*0.5 + island.pos.x + self.margin),
                                y=1000 * (self.page_size.y - island.pos.y - self.margin - 0.2 * self.text_size),
                                label=island.title),
                            file=f)

                    data_markers, data_stickerfill, data_outer, data_convex, data_concave, data_freestyle = (list() for i in range(6))
                    for marker in island.markers:
                        if isinstance(marker, Sticker):
                            data_stickerfill.append("M {} Z".format(
                                line_through(self.format_vertex(vertex.co, island.pos) for vertex in marker.vertices)))
                            if marker.text:
                                data_markers.append(self.text_transformed_tag.format(
                                    label=marker.text,
                                    pos=self.format_vertex(marker.center, island.pos),
                                    mat=format_matrix(marker.rot),
                                    size=marker.width * 1000))
                        elif isinstance(marker, Arrow):
                            size = marker.size * 1000
                            position = marker.center + marker.rot*marker.size*M.Vector((0, -0.9))
                            data_markers.append(self.arrow_marker_tag.format(
                                index=marker.text,
                                arrow_pos=self.format_vertex(marker.center, island.pos),
                                scale=size,
                                pos=self.format_vertex(position, island.pos - marker.size*M.Vector((0, 0.4))),
                                mat=format_matrix(size * marker.rot)))
                        elif isinstance(marker, NumberAlone):
                            data_markers.append(self.text_transformed_tag.format(
                                label=marker.text,
                                pos=self.format_vertex(marker.center, island.pos),
                                mat=format_matrix(marker.rot),
                                size=marker.size * 1000))
                    if data_stickerfill and self.style.sticker_fill[3] > 0:
                        print("<path class='sticker' d='", rows(data_stickerfill), "'/>", file=f)

                    outer_edges = set(island.boundary)
                    while outer_edges:
                        data_loop = list()
                        uvedge = outer_edges.pop()
                        while 1:
                            if uvedge.sticker:
                                data_loop.extend(self.format_vertex(vertex.co, island.pos) for vertex in uvedge.sticker.vertices[1:])
                            else:
                                vertex = uvedge.vb if uvedge.uvface.flipped else uvedge.va
                                data_loop.append(self.format_vertex(vertex.co, island.pos))
                            uvedge = uvedge.neighbor_right
                            try:
                                outer_edges.remove(uvedge)
                            except KeyError:
                                break
                        data_outer.append("M {} Z".format(line_through(data_loop)))

                    for uvedge in island.edges:
                        edge = uvedge.edge
                        if edge.is_cut(uvedge.uvface.face) and not uvedge.sticker:
                            continue
                        data_uvedge = "M {}".format(
                            line_through(self.format_vertex(vertex.co, island.pos) for vertex in (uvedge.va, uvedge.vb)))
                        if edge.freestyle:
                            data_freestyle.append(data_uvedge)
                        # each uvedge is in two opposite-oriented variants; we want to add each only once
                        if uvedge.sticker or uvedge.uvface.flipped != (uvedge.va.vertex.index > uvedge.vb.vertex.index):
                            if edge.angle > self.angle_epsilon:
                                data_convex.append(data_uvedge)
                            elif edge.angle < -self.angle_epsilon:
                                data_concave.append(data_uvedge)
                    if island.is_inside_out:
                        data_convex, data_concave = data_concave, data_convex

                    if data_freestyle:
                        print("<path class='freestyle' d='", rows(data_freestyle), "'/>", file=f)
                    if (data_convex or data_concave) and not self.pure_net and self.style.use_inbg:
                        print("<path class='inner_background' d='", rows(data_convex + data_concave), "'/>", file=f)
                    if data_convex:
                        print("<path class='convex' d='", rows(data_convex), "'/>", file=f)
                    if data_concave:
                        print("<path class='concave' d='", rows(data_concave), "'/>", file=f)
                    if data_outer:
                        if not self.pure_net and self.style.use_outbg:
                            print("<path class='outer_background' d='", rows(data_outer), "'/>", file=f)
                        print("<path class='outer' d='", rows(data_outer), "'/>", file=f)
                    if data_markers:
                        print(rows(data_markers), file=f)
                    print("</g>", file=f)

                if len(page.islands) > 1:
                    print("</g>", file=f)
                print("</svg>", file=f)

    image_linked_tag = "<image transform='translate({pos})' width='{width:.6f}' height='{height:.6f}' xlink:href='{path}'/>"
    image_embedded_tag = "<image transform='translate({pos})' width='{width:.6f}' height='{height:.6f}' xlink:href='data:image/png;base64,"
    text_tag = "<text transform='translate({x} {y})' style='font-size:{size:.2f}'><tspan>{label}</tspan></text>"
    text_transformed_tag = "<text transform='matrix({mat} {pos})' style='font-size:{size:.2f}'><tspan>{label}</tspan></text>"
    arrow_marker_tag = "<g><path transform='matrix({mat} {arrow_pos})' class='arrow' d='M 0 0 L 1 1 L 0 0.25 L -1 1 Z'/>" \
        "<text transform='translate({pos})' style='font-size:{scale:.2f}'><tspan>{index}</tspan></text></g>"

    svg_base = """<?xml version='1.0' encoding='UTF-8' standalone='no'?>
    <svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' version='1.1'
    width='{width:.2f}mm' height='{height:.2f}mm' viewBox='0 0 {width:.2f} {height:.2f}'>"""

    css_base = """<style type="text/css">
    path {{
        fill: none;
        stroke-linecap: butt;
        stroke-linejoin: bevel;
        stroke-dasharray: none;
    }}
    path.outer {{
        stroke: {outer_color};
        stroke-dasharray: {outer_style};
        stroke-dashoffset: 0;
        stroke-width: {outer_width:.2};
        stroke-opacity: {outer_alpha:.2};
    }}
    path.convex {{
        stroke: {convex_color};
        stroke-dasharray: {convex_style};
        stroke-dashoffset:0;
        stroke-width:{convex_width:.2};
        stroke-opacity: {convex_alpha:.2}
    }}
    path.concave {{
        stroke: {concave_color};
        stroke-dasharray: {concave_style};
        stroke-dashoffset: 0;
        stroke-width: {concave_width:.2};
        stroke-opacity: {concave_alpha:.2}
    }}
    path.freestyle {{
        stroke: {freestyle_color};
        stroke-dasharray: {freestyle_style};
        stroke-dashoffset: 0;
        stroke-width: {freestyle_width:.2};
        stroke-opacity: {freestyle_alpha:.2}
    }}
    path.outer_background {{
        stroke: {outbg_color};
        stroke-opacity: {outbg_alpha};
        stroke-width: {outbg_width:.2}
    }}
    path.inner_background {{
        stroke: {inbg_color};
        stroke-opacity: {inbg_alpha};
        stroke-width: {inbg_width:.2}
    }}
    path.sticker {{
        fill: {sticker_fill};
        stroke: none;
        fill-opacity: {sticker_alpha:.2};
    }}
    path.arrow {{
        fill: #000;
    }}
    text {{
        font-style: normal;
        fill: {text_color};
        fill-opacity: {text_alpha:.2};
        stroke: none;
    }}
    text, tspan {{
        text-anchor:middle;
    }}
    </style>"""


class PDF:
    """Simple PDF exporter"""

    mm_to_pt = 72 / 25.4
    character_width_packed = {
        191: "'", 222: 'ijl\x82\x91\x92', 278: '|¦\x00\x01\x02\x03\x04\x05\x06\x07\x08\t\n\x0b\x0c\r\x0e\x0f\x10\x11\x12\x13\x14\x15\x16\x17\x18\x19\x1a\x1b\x1c\x1d\x1e\x1f !,./:;I[\\]ft\xa0·ÌÍÎÏìíîï',
        333: '()-`r\x84\x88\x8b\x93\x94\x98\x9b¡¨\xad¯²³´¸¹{}', 350: '\x7f\x81\x8d\x8f\x90\x95\x9d', 365: '"ºª*°', 469: '^', 500: 'Jcksvxyz\x9a\x9eçýÿ', 584: '¶+<=>~¬±×÷', 611: 'FTZ\x8e¿ßø',
        667: '&ABEKPSVXY\x8a\x9fÀÁÂÃÄÅÈÉÊËÝÞ', 722: 'CDHNRUwÇÐÑÙÚÛÜ', 737: '©®', 778: 'GOQÒÓÔÕÖØ', 833: 'Mm¼½¾', 889: '%æ', 944: 'W\x9c', 1000: '\x85\x89\x8c\x97\x99Æ', 1015: '@', }
    character_width = {c: value for (value, chars) in character_width_packed.items() for c in chars}

    def __init__(self, page_size: M.Vector, style, margin, pure_net=True, angle_epsilon=0.01):
        self.page_size = page_size
        self.style = style
        self.margin = M.Vector((margin, margin))
        self.pure_net = pure_net
        self.angle_epsilon = angle_epsilon

    def text_width(self, text, scale=None):
        return (scale or self.text_size) * sum(self.character_width.get(c, 556) for c in text) / 1000

    @classmethod
    def encode_image(cls, bpy_image):
        data = bytes(int(255 * px) for (i, px) in enumerate(bpy_image.pixels) if i % 4 != 3)
        image = {
            "Type": "XObject", "Subtype": "Image", "Width": bpy_image.size[0], "Height": bpy_image.size[1],
            "ColorSpace": "DeviceRGB", "BitsPerComponent": 8, "Interpolate": True,
            "Filter": ["ASCII85Decode", "FlateDecode"], "stream": data}
        return image

    def write(self, mesh, filename):
        def format_dict(obj, refs=tuple()):
            return "<< " + "".join("/{} {}\n".format(key, format_value(value, refs)) for (key, value) in obj.items()) + ">>"

        def line_through(seq):
            return "".join("{0.x:.6f} {0.y:.6f} {1} ".format(1000*v.co, c) for (v, c) in zip(seq, chain("m", repeat("l"))))

        def format_value(value, refs=tuple()):
            if value in refs:
                return "{} 0 R".format(refs.index(value) + 1)
            elif type(value) is dict:
                return format_dict(value, refs)
            elif type(value) in (list, tuple):
                return "[ " + " ".join(format_value(item, refs) for item in value) + " ]"
            elif type(value) is int:
                return str(value)
            elif type(value) is float:
                return "{:.6f}".format(value)
            elif type(value) is bool:
                return "true" if value else "false"
            else:
                return "/{}".format(value)  # this script can output only PDF names, no strings

        def write_object(index, obj, refs, f, stream=None):
            byte_count = f.write("{} 0 obj\n".format(index))
            if type(obj) is not dict:
                stream, obj = obj, dict()
            elif "stream" in obj:
                stream = obj.pop("stream")
            if stream:
                if True or type(stream) is bytes:
                    obj["Filter"] = ["ASCII85Decode", "FlateDecode"]
                    stream = encode(stream)
                obj["Length"] = len(stream)
            byte_count += f.write(format_dict(obj, refs))
            if stream:
                byte_count += f.write("\nstream\n")
                byte_count += f.write(stream)
                byte_count += f.write("\nendstream")
            return byte_count + f.write("\nendobj\n")

        def encode(data):
            from base64 import a85encode
            from zlib import compress
            if hasattr(data, "encode"):
                data = data.encode()
            return a85encode(compress(data), adobe=True, wrapcol=250)[2:].decode()

        page_size_pt = 1000 * self.mm_to_pt * self.page_size
        root = {"Type": "Pages", "MediaBox": [0, 0, page_size_pt.x, page_size_pt.y], "Kids": list()}
        catalog = {"Type": "Catalog", "Pages": root}
        font = {
            "Type": "Font", "Subtype": "Type1", "Name": "F1",
            "BaseFont": "Helvetica", "Encoding": "MacRomanEncoding"}

        dl = [length * self.style.line_width * 1000 for length in (1, 4, 9)]
        format_style = {
            'SOLID': list(), 'DOT': [dl[0], dl[1]], 'DASH': [dl[1], dl[2]],
            'LONGDASH': [dl[2], dl[1]], 'DASHDOT': [dl[2], dl[1], dl[0], dl[1]]}
        styles = {
            "Gtext": {"ca": self.style.text_color[3], "Font": [font, 1000 * self.text_size]},
            "Gsticker": {"ca": self.style.sticker_fill[3]}}
        for name in ("outer", "convex", "concave", "freestyle"):
            gs = {
                "LW": self.style.line_width * 1000 * getattr(self.style, name + "_width"),
                "CA": getattr(self.style, name + "_color")[3],
                "D": [format_style[getattr(self.style, name + "_style")], 0]}
            styles["G" + name] = gs
        for name in ("outbg", "inbg"):
            gs = {
                "LW": self.style.line_width * 1000 * getattr(self.style, name + "_width"),
                "CA": getattr(self.style, name + "_color")[3],
                "D": [format_style['SOLID'], 0]}
            styles["G" + name] = gs

        objects = [root, catalog, font]
        objects.extend(styles.values())

        for page in mesh.pages:
            commands = ["{0:.6f} 0 0 {0:.6f} 0 0 cm".format(self.mm_to_pt)]
            resources = {"Font": {"F1": font}, "ExtGState": styles, "XObject": dict()}
            for island in page.islands:
                commands.append("q 1 0 0 1 {0.x:.6f} {0.y:.6f} cm".format(1000*(self.margin + island.pos)))
                if island.embedded_image:
                    identifier = "Im{}".format(len(resources["XObject"]) + 1)
                    commands.append(self.command_image.format(1000 * island.bounding_box, identifier))
                    objects.append(island.embedded_image)
                    resources["XObject"][identifier] = island.embedded_image

                if island.title:
                    commands.append(self.command_label.format(
                        size=1000*self.text_size,
                        x=500 * (island.bounding_box.x - self.text_width(island.title)),
                        y=1000 * 0.2 * self.text_size,
                        label=island.title))

                data_markers, data_stickerfill, data_outer, data_convex, data_concave, data_freestyle = (list() for i in range(6))
                for marker in island.markers:
                    if isinstance(marker, Sticker):
                        data_stickerfill.append(line_through(marker.vertices) + "f")
                        if marker.text:
                            data_markers.append(self.command_sticker.format(
                                label=marker.text,
                                pos=1000*marker.center,
                                mat=marker.rot,
                                align=-500 * self.text_width(marker.text, marker.width),
                                size=1000*marker.width))
                    elif isinstance(marker, Arrow):
                        size = 1000 * marker.size
                        position = 1000 * (marker.center + marker.rot*marker.size*M.Vector((0, -0.9)))
                        data_markers.append(self.command_arrow.format(
                            index=marker.text,
                            arrow_pos=1000 * marker.center,
                            pos=position - 1000 * M.Vector((0.5 * self.text_width(marker.text), 0.4 * self.text_size)),
                            mat=size * marker.rot,
                            size=size))
                    elif isinstance(marker, NumberAlone):
                        data_markers.append(self.command_number.format(
                            label=marker.text,
                            pos=1000*marker.center,
                            mat=marker.rot,
                            size=1000*marker.size))

                outer_edges = set(island.boundary)
                while outer_edges:
                    data_loop = list()
                    uvedge = outer_edges.pop()
                    while 1:
                        if uvedge.sticker:
                            data_loop.extend(uvedge.sticker.vertices[1:])
                        else:
                            vertex = uvedge.vb if uvedge.uvface.flipped else uvedge.va
                            data_loop.append(vertex)
                        uvedge = uvedge.neighbor_right
                        try:
                            outer_edges.remove(uvedge)
                        except KeyError:
                            break
                    data_outer.append(line_through(data_loop) + "s")

                for uvedge in island.edges:
                    edge = uvedge.edge
                    if edge.is_cut(uvedge.uvface.face) and not uvedge.sticker:
                        continue
                    data_uvedge = line_through((uvedge.va, uvedge.vb)) + "S"
                    if edge.freestyle:
                        data_freestyle.append(data_uvedge)
                    # each uvedge is in two opposite-oriented variants; we want to add each only once
                    if uvedge.sticker or uvedge.uvface.flipped != (uvedge.va.vertex.index > uvedge.vb.vertex.index):
                        if edge.angle > self.angle_epsilon:
                            data_convex.append(data_uvedge)
                        elif edge.angle < -self.angle_epsilon:
                            data_concave.append(data_uvedge)
                if island.is_inside_out:
                    data_convex, data_concave = data_concave, data_convex

                if data_stickerfill and self.style.sticker_fill[3] > 0:
                    commands.append("/Gsticker gs {0[0]:.3f} {0[1]:.3f} {0[2]:.3f} rg".format(self.style.sticker_fill))
                    commands.extend(data_stickerfill)
                if data_freestyle:
                    commands.append("/Gfreestyle gs {0[0]:.3f} {0[1]:.3f} {0[2]:.3f} RG".format(self.style.freestyle_color))
                    commands.extend(data_freestyle)
                if (data_convex or data_concave) and not self.pure_net and self.style.use_inbg:
                    commands.append("/Ginbg gs {0[0]:.3f} {0[1]:.3f} {0[2]:.3f} RG".format(self.style.inbg_color))
                    commands.extend(chain(data_convex, data_concave))
                if data_convex:
                    commands.append("/Gconvex gs {0[0]:.3f} {0[1]:.3f} {0[2]:.3f} RG".format(self.style.convex_color))
                    commands.extend(data_convex)
                if data_concave:
                    commands.append("/Gconcave gs {0[0]:.3f} {0[1]:.3f} {0[2]:.3f} RG".format(self.style.concave_color))
                    commands.extend(data_concave)
                if data_outer:
                    if not self.pure_net and self.style.use_outbg:
                        commands.append("/Goutbg gs {0[0]:.3f} {0[1]:.3f} {0[2]:.3f} RG".format(self.style.outbg_color))
                        commands.extend(data_outer)
                    commands.append("/Gouter gs {0[0]:.3f} {0[1]:.3f} {0[2]:.3f} RG".format(self.style.outer_color))
                    commands.extend(data_outer)
                commands.append("/Gtext gs {0[0]:.3f} {0[1]:.3f} {0[2]:.3f} rg".format(self.style.text_color))
                commands.extend(data_markers)
                commands.append("Q")
            content = "\n".join(commands)
            page = {"Type": "Page", "Parent": root, "Contents": content, "Resources": resources}
            root["Kids"].append(page)
            objects.extend((page, content))

        root["Count"] = len(root["Kids"])
        with open(filename, "w+") as f:
            xref_table = list()
            position = f.write("%PDF-1.4\n")
            for index, obj in enumerate(objects, 1):
                xref_table.append(position)
                position += write_object(index, obj, objects, f)
            xref_pos = position
            f.write("xref_table\n0 {}\n".format(len(xref_table) + 1))
            f.write("{:010} {:05} f\n".format(0, 65536))
            for position in xref_table:
                f.write("{:010} {:05} n\n".format(position, 0))
            f.write("trailer\n")
            f.write(format_dict({"Size": len(xref_table), "Root": catalog}, objects))
            f.write("\nstartxref\n{}\n%%EOF\n".format(xref_pos))

    command_label = "/Gtext gs BT {x:.6f} {y:.6f} Td ({label}) Tj ET"
    command_image = "q {0.x:.6f} 0 0 {0.y:.6f} 0 0 cm 1 0 0 -1 0 1 cm /{1} Do Q"
    command_sticker = "q {mat[0][0]:.6f} {mat[1][0]:.6f} {mat[0][1]:.6f} {mat[1][1]:.6f} {pos.x:.6f} {pos.y:.6f} cm BT {align:.6f} 0 Td /F1 {size:.6f} Tf ({label}) Tj ET Q"
    command_arrow = "q BT {pos.x:.6f} {pos.y:.6f} Td /F1 {size:.6f} Tf ({index}) Tj ET {mat[0][0]:.6f} {mat[1][0]:.6f} {mat[0][1]:.6f} {mat[1][1]:.6f} {arrow_pos.x:.6f} {arrow_pos.y:.6f} cm 0 0 m 1 -1 l 0 -0.25 l -1 -1 l f Q"
    command_number = "q {mat[0][0]:.6f} {mat[1][0]:.6f} {mat[0][1]:.6f} {mat[1][1]:.6f} {pos.x:.6f} {pos.y:.6f} cm BT /F1 {size:.6f} Tf ({label}) Tj ET Q"


class Unfold(bpy.types.Operator):
    """Blender Operator: unfold the selected object."""

    bl_idname = "mesh.unfold"
    bl_label = "Unfold"
    bl_description = "Mark seams so that the mesh can be exported as a paper model"
    bl_options = {'REGISTER', 'UNDO'}
    edit = bpy.props.BoolProperty(default=False, options={'HIDDEN'})
    priority_effect_convex = bpy.props.FloatProperty(
        name="Priority Convex", description="Priority effect for edges in convex angles",
        default=default_priority_effect['CONVEX'], soft_min=-1, soft_max=10, subtype='FACTOR')
    priority_effect_concave = bpy.props.FloatProperty(
        name="Priority Concave", description="Priority effect for edges in concave angles",
        default=default_priority_effect['CONCAVE'], soft_min=-1, soft_max=10, subtype='FACTOR')
    priority_effect_length = bpy.props.FloatProperty(
        name="Priority Length", description="Priority effect of edge length",
        default=default_priority_effect['LENGTH'], soft_min=-10, soft_max=1, subtype='FACTOR')
    do_create_uvmap = bpy.props.BoolProperty(
        name="Create UVMap", description="Create a new UV Map showing the islands and page layout", default=False)
    object = None

    @classmethod
    def poll(cls, context):
        return context.active_object and context.active_object.type == "MESH"

    def draw(self, context):
        layout = self.layout
        col = layout.column()
        col.active = not self.object or len(self.object.data.uv_textures) < 8
        col.prop(self.properties, "do_create_uvmap")
        layout.label(text="Edge Cutting Factors:")
        col = layout.column(align=True)
        col.label(text="Face Angle:")
        col.prop(self.properties, "priority_effect_convex", text="Convex")
        col.prop(self.properties, "priority_effect_concave", text="Concave")
        layout.prop(self.properties, "priority_effect_length", text="Edge Length")

    def execute(self, context):
        sce = bpy.context.scene
        settings = sce.paper_model
        recall_mode = context.object.mode
        bpy.ops.object.mode_set(mode='OBJECT')
        recall_display_islands, sce.paper_model.display_islands = sce.paper_model.display_islands, False

        self.object = context.active_object
        mesh = self.object.data

        cage_size = M.Vector((settings.output_size_x, settings.output_size_y)) if settings.limit_by_page else None
        priority_effect = {
            'CONVEX': self.priority_effect_convex,
            'CONCAVE': self.priority_effect_concave,
            'LENGTH': self.priority_effect_length}
        try:
            unfolder = Unfolder(self.object)
            unfolder.prepare(
                cage_size, self.do_create_uvmap, mark_seams=True,
                priority_effect=priority_effect, scale=sce.unit_settings.scale_length/settings.scale)
        except UnfoldError as error:
            self.report(type={'ERROR_INVALID_INPUT'}, message=error.args[0])
            bpy.ops.object.mode_set(mode=recall_mode)
            sce.paper_model.display_islands = recall_display_islands
            return {'CANCELLED'}
        if mesh.paper_island_list:
            unfolder.copy_island_names(mesh.paper_island_list)

        island_list = mesh.paper_island_list
        attributes = {item.label: (item.abbreviation, item.auto_label, item.auto_abbrev) for item in island_list}
        island_list.clear()  # remove previously defined islands
        for island in unfolder.mesh.islands:
            # add islands to UI list and set default descriptions
            list_item = island_list.add()
            # add faces' IDs to the island
            for uvface in island.faces:
                lface = list_item.faces.add()
                lface.id = uvface.face.index

            list_item["label"] = island.label
            list_item["abbreviation"], list_item["auto_label"], list_item["auto_abbrev"] = attributes.get(
                island.label,
                (island.abbreviation, True, True))
            island_item_changed(list_item, context)

        mesh.paper_island_index = -1
        mesh.show_edge_seams = True

        bpy.ops.object.mode_set(mode=recall_mode)
        sce.paper_model.display_islands = recall_display_islands
        return {'FINISHED'}


class ClearAllSeams(bpy.types.Operator):
    """Blender Operator: clear all seams of the active Mesh and all its unfold data"""

    bl_idname = "mesh.clear_all_seams"
    bl_label = "Clear All Seams"
    bl_description = "Clear all the seams and unfolded islands of the active object"

    @classmethod
    def poll(cls, context):
        return context.active_object and context.active_object.type == 'MESH'

    def execute(self, context):
        ob = context.active_object
        mesh = ob.data

        for edge in mesh.edges:
            edge.use_seam = False
        mesh.paper_island_list.clear()

        return {'FINISHED'}


def page_size_preset_changed(self, context):
    """Update the actual document size to correct values"""
    if hasattr(self, "limit_by_page") and not self.limit_by_page:
        return
    if self.page_size_preset == 'A4':
        self.output_size_x = 0.210
        self.output_size_y = 0.297
    elif self.page_size_preset == 'A3':
        self.output_size_x = 0.297
        self.output_size_y = 0.420
    elif self.page_size_preset == 'US_LETTER':
        self.output_size_x = 0.216
        self.output_size_y = 0.279
    elif self.page_size_preset == 'US_LEGAL':
        self.output_size_x = 0.216
        self.output_size_y = 0.356


class PaperModelStyle(bpy.types.PropertyGroup):
    line_styles = [
        ('SOLID', "Solid (----)", "Solid line"),
        ('DOT', "Dots (. . .)", "Dotted line"),
        ('DASH', "Short Dashes (- - -)", "Solid line"),
        ('LONGDASH', "Long Dashes (-- --)", "Solid line"),
        ('DASHDOT', "Dash-dotted (-- .)", "Solid line")
    ]
    outer_color = bpy.props.FloatVectorProperty(
        name="Outer Lines", description="Color of net outline",
        default=(0.0, 0.0, 0.0, 1.0), min=0, max=1, subtype='COLOR', size=4)
    outer_style = bpy.props.EnumProperty(
        name="Outer Lines Drawing Style", description="Drawing style of net outline",
        default='SOLID', items=line_styles)
    line_width = bpy.props.FloatProperty(
        name="Base Lines Thickness", description="Base thickness of net lines, each actual value is a multiple of this length",
        default=1e-4, min=0, soft_max=5e-3, precision=5, step=1e-2, subtype="UNSIGNED", unit="LENGTH")
    outer_width = bpy.props.FloatProperty(
        name="Outer Lines Thickness", description="Relative thickness of net outline",
        default=3, min=0, soft_max=10, precision=1, step=10, subtype='FACTOR')
    use_outbg = bpy.props.BoolProperty(
        name="Highlight Outer Lines", description="Add another line below every line to improve contrast",
        default=True)
    outbg_color = bpy.props.FloatVectorProperty(
        name="Outer Highlight", description="Color of the highlight for outer lines",
        default=(1.0, 1.0, 1.0, 1.0), min=0, max=1, subtype='COLOR', size=4)
    outbg_width = bpy.props.FloatProperty(
        name="Outer Highlight Thickness", description="Relative thickness of the highlighting lines",
        default=5, min=0, soft_max=10, precision=1, step=10, subtype='FACTOR')

    convex_color = bpy.props.FloatVectorProperty(
        name="Inner Convex Lines", description="Color of lines to be folded to a convex angle",
        default=(0.0, 0.0, 0.0, 1.0), min=0, max=1, subtype='COLOR', size=4)
    convex_style = bpy.props.EnumProperty(
        name="Convex Lines Drawing Style", description="Drawing style of lines to be folded to a convex angle",
        default='DASH', items=line_styles)
    convex_width = bpy.props.FloatProperty(
        name="Convex Lines Thickness", description="Relative thickness of concave lines",
        default=2, min=0, soft_max=10, precision=1, step=10, subtype='FACTOR')
    concave_color = bpy.props.FloatVectorProperty(
        name="Inner Concave Lines", description="Color of lines to be folded to a concave angle",
        default=(0.0, 0.0, 0.0, 1.0), min=0, max=1, subtype='COLOR', size=4)
    concave_style = bpy.props.EnumProperty(
        name="Concave Lines Drawing Style", description="Drawing style of lines to be folded to a concave angle",
        default='DASHDOT', items=line_styles)
    concave_width = bpy.props.FloatProperty(
        name="Concave Lines Thickness", description="Relative thickness of concave lines",
        default=2, min=0, soft_max=10, precision=1, step=10, subtype='FACTOR')
    freestyle_color = bpy.props.FloatVectorProperty(
        name="Freestyle Edges", description="Color of lines marked as Freestyle Edge",
        default=(0.0, 0.0, 0.0, 1.0), min=0, max=1, subtype='COLOR', size=4)
    freestyle_style = bpy.props.EnumProperty(
        name="Freestyle Edges Drawing Style", description="Drawing style of Freestyle Edges",
        default='SOLID', items=line_styles)
    freestyle_width = bpy.props.FloatProperty(
        name="Freestyle Edges Thickness", description="Relative thickness of Freestyle edges",
        default=2, min=0, soft_max=10, precision=1, step=10, subtype='FACTOR')
    use_inbg = bpy.props.BoolProperty(
        name="Highlight Inner Lines", description="Add another line below every line to improve contrast",
        default=True)
    inbg_color = bpy.props.FloatVectorProperty(
        name="Inner Highlight", description="Color of the highlight for inner lines",
        default=(1.0, 1.0, 1.0, 1.0), min=0, max=1, subtype='COLOR', size=4)
    inbg_width = bpy.props.FloatProperty(
        name="Inner Highlight Thickness", description="Relative thickness of the highlighting lines",
        default=2, min=0, soft_max=10, precision=1, step=10, subtype='FACTOR')

    sticker_fill = bpy.props.FloatVectorProperty(
        name="Tabs Fill", description="Fill color of sticking tabs",
        default=(0.9, 0.9, 0.9, 1.0), min=0, max=1, subtype='COLOR', size=4)
    text_color = bpy.props.FloatVectorProperty(
        name="Text Color", description="Color of all text used in the document",
        default=(0.0, 0.0, 0.0, 1.0), min=0, max=1, subtype='COLOR', size=4)
bpy.utils.register_class(PaperModelStyle)


class ExportPaperModel(bpy.types.Operator):
    """Blender Operator: save the selected object's net and optionally bake its texture"""

    bl_idname = "export_mesh.paper_model"
    bl_label = "Export Paper Model"
    bl_description = "Export the selected object's net and optionally bake its texture"
    filepath = bpy.props.StringProperty(
        name="File Path", description="Target file to save the SVG", options={'SKIP_SAVE'})
    filename = bpy.props.StringProperty(
        name="File Name", description="Name of the file", options={'SKIP_SAVE'})
    directory = bpy.props.StringProperty(
        name="Directory", description="Directory of the file", options={'SKIP_SAVE'})
    page_size_preset = bpy.props.EnumProperty(
        name="Page Size", description="Size of the exported document",
        default='A4', update=page_size_preset_changed, items=global_paper_sizes)
    output_size_x = bpy.props.FloatProperty(
        name="Page Width", description="Width of the exported document",
        default=0.210, soft_min=0.105, soft_max=0.841, subtype="UNSIGNED", unit="LENGTH")
    output_size_y = bpy.props.FloatProperty(
        name="Page Height", description="Height of the exported document",
        default=0.297, soft_min=0.148, soft_max=1.189, subtype="UNSIGNED", unit="LENGTH")
    output_margin = bpy.props.FloatProperty(
        name="Page Margin", description="Distance from page borders to the printable area",
        default=0.005, min=0, soft_max=0.1, step=0.1, subtype="UNSIGNED", unit="LENGTH")
    output_type = bpy.props.EnumProperty(
        name="Textures", description="Source of a texture for the model",
        default='NONE', items=[
            ('NONE', "No Texture", "Export the net only"),
            ('TEXTURE', "From Materials", "Render the diffuse color and all painted textures"),
            ('AMBIENT_OCCLUSION', "Ambient Occlusion", "Render the Ambient Occlusion pass"),
            ('RENDER', "Full Render", "Render the material in actual scene illumination"),
            ('SELECTED_TO_ACTIVE', "Selected to Active", "Render all selected surrounding objects as a texture")
        ])
    do_create_stickers = bpy.props.BoolProperty(
        name="Create Tabs", description="Create gluing tabs around the net (useful for paper)",
        default=True)
    do_create_numbers = bpy.props.BoolProperty(
        name="Create Numbers", description="Enumerate edges to make it clear which edges should be sticked together",
        default=True)
    sticker_width = bpy.props.FloatProperty(
        name="Tabs and Text Size", description="Width of gluing tabs and their numbers",
        default=0.005, soft_min=0, soft_max=0.05, step=0.1, subtype="UNSIGNED", unit="LENGTH")
    angle_epsilon = bpy.props.FloatProperty(
        name="Hidden Edge Angle", description="Folds with angle below this limit will not be drawn",
        default=pi/360, min=0, soft_max=pi/4, step=0.01, subtype="ANGLE", unit="ROTATION")
    output_dpi = bpy.props.FloatProperty(
        name="Resolution (DPI)", description="Resolution of images in pixels per inch",
        default=90, min=1, soft_min=30, soft_max=600, subtype="UNSIGNED")
    file_format = bpy.props.EnumProperty(
        name="Document Format", description="File format of the exported net",
        default='PDF', items=[
            ('PDF', "PDF", "Adobe Portable Document Format 1.4"),
            ('SVG', "SVG", "W3C Scalable Vector Graphics"),
        ])
    image_packing = bpy.props.EnumProperty(
        name="Image Packing Method", description="Method of attaching baked image(s) to the SVG",
        default='ISLAND_EMBED', items=[
            ('PAGE_LINK', "Single Linked", "Bake one image per page of output and save it separately"),
            ('ISLAND_LINK', "Linked", "Bake images separately for each island and save them in a directory"),
            ('ISLAND_EMBED', "Embedded", "Bake images separately for each island and embed them into the SVG")
        ])
    scale = bpy.props.FloatProperty(
        name="Scale", description="Divisor of all dimensions when exporting",
        default=1, soft_min=1.0, soft_max=10000.0, step=100, subtype='UNSIGNED', precision=1)
    do_create_uvmap = bpy.props.BoolProperty(
        name="Create UVMap", description="Create a new UV Map showing the islands and page layout",
        default=False, options={'SKIP_SAVE'})
    ui_expanded_document = bpy.props.BoolProperty(
        name="Show Document Settings Expanded", description="Shows the box 'Document Settings' expanded in user interface",
        default=True, options={'SKIP_SAVE'})
    ui_expanded_style = bpy.props.BoolProperty(
        name="Show Style Settings Expanded", description="Shows the box 'Colors and Style' expanded in user interface",
        default=False, options={'SKIP_SAVE'})
    style = bpy.props.PointerProperty(type=PaperModelStyle)

    unfolder = None
    largest_island_ratio = 0

    @classmethod
    def poll(cls, context):
        return context.active_object and context.active_object.type == 'MESH'

    def execute(self, context):
        try:
            if self.object.data.paper_island_list:
                self.unfolder.copy_island_names(self.object.data.paper_island_list)
            self.unfolder.save(self.properties)
            self.report({'INFO'}, "Saved a {}-page document".format(len(self.unfolder.mesh.pages)))
            return {'FINISHED'}
        except UnfoldError as error:
            self.report(type={'ERROR_INVALID_INPUT'}, message=error.args[0])
            return {'CANCELLED'}

    def get_scale_ratio(self, sce):
        margin = self.output_margin + self.sticker_width + 1e-5
        if min(self.output_size_x, self.output_size_y) <= 2 * margin:
            return False
        output_inner_size = M.Vector((self.output_size_x - 2*margin, self.output_size_y - 2*margin))
        ratio = self.unfolder.mesh.largest_island_ratio(output_inner_size)
        return ratio * sce.unit_settings.scale_length / self.scale

    def invoke(self, context, event):
        sce = context.scene
        recall_mode = context.object.mode
        bpy.ops.object.mode_set(mode='OBJECT')

        self.scale = sce.paper_model.scale
        self.object = context.active_object
        cage_size = M.Vector((sce.paper_model.output_size_x, sce.paper_model.output_size_y)) if sce.paper_model.limit_by_page else None
        try:
            self.unfolder = Unfolder(self.object)
            self.unfolder.prepare(
                cage_size, create_uvmap=self.do_create_uvmap,
                scale=sce.unit_settings.scale_length/self.scale)
        except UnfoldError as error:
            self.report(type={'ERROR_INVALID_INPUT'}, message=error.args[0])
            bpy.ops.object.mode_set(mode=recall_mode)
            return {'CANCELLED'}
        scale_ratio = self.get_scale_ratio(sce)
        if scale_ratio > 1:
            self.scale = ceil(self.scale * scale_ratio)
        wm = context.window_manager
        wm.fileselect_add(self)

        bpy.ops.object.mode_set(mode=recall_mode)
        return {'RUNNING_MODAL'}

    def draw(self, context):
        layout = self.layout

        layout.prop(self.properties, "do_create_uvmap")

        row = layout.row(align=True)
        row.menu("VIEW3D_MT_paper_model_presets", text=bpy.types.VIEW3D_MT_paper_model_presets.bl_label)
        row.operator("export_mesh.paper_model_preset_add", text="", icon='ZOOMIN')
        row.operator("export_mesh.paper_model_preset_add", text="", icon='ZOOMOUT').remove_active = True

        # a little hack: this prints out something like "Scale: 1: 72"
        layout.prop(self.properties, "scale", text="Scale: 1")
        scale_ratio = self.get_scale_ratio(context.scene)
        if scale_ratio > 1:
            layout.label(
                text="An island is roughly {:.1f}x bigger than page".format(scale_ratio),
                icon="ERROR")
        elif scale_ratio > 0:
            layout.label(text="Largest island is roughly 1/{:.1f} of page".format(1 / scale_ratio))

        if context.scene.unit_settings.scale_length != 1:
            layout.label(
                text="Unit scale {:.1f} makes page size etc. not display correctly".format(
                    context.scene.unit_settings.scale_length), icon="ERROR")
        box = layout.box()
        row = box.row(align=True)
        row.prop(
            self.properties, "ui_expanded_document", text="",
            icon=('TRIA_DOWN' if self.ui_expanded_document else 'TRIA_RIGHT'), emboss=False)
        row.label(text="Document Settings")

        if self.ui_expanded_document:
            box.prop(self.properties, "file_format", text="Format")
            box.prop(self.properties, "page_size_preset")
            col = box.column(align=True)
            col.active = self.page_size_preset == 'USER'
            col.prop(self.properties, "output_size_x")
            col.prop(self.properties, "output_size_y")
            box.prop(self.properties, "output_margin")
            col = box.column()
            col.prop(self.properties, "do_create_stickers")
            col.prop(self.properties, "do_create_numbers")
            col = box.column()
            col.active = self.do_create_stickers or self.do_create_numbers
            col.prop(self.properties, "sticker_width")
            box.prop(self.properties, "angle_epsilon")

            box.prop(self.properties, "output_type")
            col = box.column()
            col.active = (self.output_type != 'NONE')
            if len(self.object.data.uv_textures) == 8:
                col.label(text="No UV slots left, No Texture is the only option.", icon='ERROR')
            elif context.scene.render.engine not in ('BLENDER_RENDER', 'CYCLES') and self.output_type != 'NONE':
                col.label(text="Blender Internal engine will be used for texture baking.", icon='ERROR')
            col.prop(self.properties, "output_dpi")
            row = col.row()
            row.active = self.file_format == 'SVG'
            row.prop(self.properties, "image_packing", text="Images")

        box = layout.box()
        row = box.row(align=True)
        row.prop(
            self.properties, "ui_expanded_style", text="",
            icon=('TRIA_DOWN' if self.ui_expanded_style else 'TRIA_RIGHT'), emboss=False)
        row.label(text="Colors and Style")

        if self.ui_expanded_style:
            box.prop(self.style, "line_width", text="Default line width")
            col = box.column()
            col.prop(self.style, "outer_color")
            col.prop(self.style, "outer_width", text="Relative width")
            col.prop(self.style, "outer_style", text="Style")
            col = box.column()
            col.active = self.output_type != 'NONE'
            col.prop(self.style, "use_outbg", text="Outer Lines Highlight:")
            sub = col.column()
            sub.active = self.output_type != 'NONE' and self.style.use_outbg
            sub.prop(self.style, "outbg_color", text="")
            sub.prop(self.style, "outbg_width", text="Relative width")
            col = box.column()
            col.prop(self.style, "convex_color")
            col.prop(self.style, "convex_width", text="Relative width")
            col.prop(self.style, "convex_style", text="Style")
            col = box.column()
            col.prop(self.style, "concave_color")
            col.prop(self.style, "concave_width", text="Relative width")
            col.prop(self.style, "concave_style", text="Style")
            col = box.column()
            col.prop(self.style, "freestyle_color")
            col.prop(self.style, "freestyle_width", text="Relative width")
            col.prop(self.style, "freestyle_style", text="Style")
            col = box.column()
            col.active = self.output_type != 'NONE'
            col.prop(self.style, "use_inbg", text="Inner Lines Highlight:")
            sub = col.column()
            sub.active = self.output_type != 'NONE' and self.style.use_inbg
            sub.prop(self.style, "inbg_color", text="")
            sub.prop(self.style, "inbg_width", text="Relative width")
            col = box.column()
            col.active = self.do_create_stickers
            col.prop(self.style, "sticker_fill")
            box.prop(self.style, "text_color")


def menu_func(self, context):
    self.layout.operator("export_mesh.paper_model", text="Paper Model (.svg)")


class VIEW3D_MT_paper_model_presets(bpy.types.Menu):
    bl_label = "Paper Model Presets"
    preset_subdir = "export_mesh"
    preset_operator = "script.execute_preset"
    draw = bpy.types.Menu.draw_preset


class AddPresetPaperModel(bl_operators.presets.AddPresetBase, bpy.types.Operator):
    """Add or remove a Paper Model Preset"""
    bl_idname = "export_mesh.paper_model_preset_add"
    bl_label = "Add Paper Model Preset"
    preset_menu = "VIEW3D_MT_paper_model_presets"
    preset_subdir = "export_mesh"
    preset_defines = ["op = bpy.context.active_operator"]

    @property
    def preset_values(self):
        op = bpy.ops.export_mesh.paper_model
        properties = op.get_rna().bl_rna.properties.items()
        blacklist = bpy.types.Operator.bl_rna.properties.keys()
        return [
            "op.{}".format(prop_id) for (prop_id, prop) in properties
            if not (prop.is_hidden or prop.is_skip_save or prop_id in blacklist)]


class VIEW3D_PT_paper_model_tools(bpy.types.Panel):
    bl_label = "Tools"
    bl_space_type = "VIEW_3D"
    bl_region_type = "TOOLS"
    bl_category = "Paper Model"

    def draw(self, context):
        layout = self.layout
        sce = context.scene
        obj = context.active_object
        mesh = obj.data if obj and obj.type == 'MESH' else None

        layout.operator("export_mesh.paper_model")

        col = layout.column(align=True)
        col.label("Customization:")
        col.operator("mesh.unfold")

        if context.mode == 'EDIT_MESH':
            row = layout.row(align=True)
            row.operator("mesh.mark_seam", text="Mark Seam").clear = False
            row.operator("mesh.mark_seam", text="Clear Seam").clear = True
        else:
            layout.operator("mesh.clear_all_seams")

        props = sce.paper_model
        layout.prop(props, "scale", text="Model Scale: 1")

        layout.prop(props, "limit_by_page")
        col = layout.column()
        col.active = props.limit_by_page
        col.prop(props, "page_size_preset")
        sub = col.column(align=True)
        sub.active = props.page_size_preset == 'USER'
        sub.prop(props, "output_size_x")
        sub.prop(props, "output_size_y")


class VIEW3D_PT_paper_model_islands(bpy.types.Panel):
    bl_label = "Islands"
    bl_space_type = "VIEW_3D"
    bl_region_type = "TOOLS"
    bl_category = "Paper Model"

    def draw(self, context):
        layout = self.layout
        sce = context.scene
        obj = context.active_object
        mesh = obj.data if obj and obj.type == 'MESH' else None

        if mesh and mesh.paper_island_list:
            layout.label(
                text="1 island:" if len(mesh.paper_island_list) == 1 else
                "{} islands:".format(len(mesh.paper_island_list)))
            layout.template_list(
                'UI_UL_list', 'paper_model_island_list', mesh,
                'paper_island_list', mesh, 'paper_island_index', rows=1, maxrows=5)
            if mesh.paper_island_index >= 0:
                list_item = mesh.paper_island_list[mesh.paper_island_index]
                sub = layout.column(align=True)
                sub.prop(list_item, "auto_label")
                sub.prop(list_item, "label")
                sub.prop(list_item, "auto_abbrev")
                row = sub.row()
                row.active = not list_item.auto_abbrev
                row.prop(list_item, "abbreviation")
        else:
            layout.label(text="Not unfolded")
            layout.box().label("Use the 'Unfold' tool")
        sub = layout.column(align=True)
        sub.active = bool(mesh and mesh.paper_island_list)
        sub.prop(sce.paper_model, "display_islands", icon='RESTRICT_VIEW_OFF')
        row = sub.row(align=True)
        row.active = bool(sce.paper_model.display_islands and mesh and mesh.paper_island_list)
        row.prop(sce.paper_model, "islands_alpha", slider=True)


def display_islands(self, context):
    # TODO: save the vertex positions and don't recalculate them always?
    ob = context.active_object
    if not ob or ob.type != 'MESH':
        return
    mesh = ob.data
    if not mesh.paper_island_list or mesh.paper_island_index == -1:
        return

    bgl.glMatrixMode(bgl.GL_PROJECTION)
    perspMatrix = context.space_data.region_3d.perspective_matrix
    perspBuff = bgl.Buffer(bgl.GL_FLOAT, (4, 4), perspMatrix.transposed())
    bgl.glLoadMatrixf(perspBuff)
    bgl.glMatrixMode(bgl.GL_MODELVIEW)
    objectBuff = bgl.Buffer(bgl.GL_FLOAT, (4, 4), ob.matrix_world.transposed())
    bgl.glLoadMatrixf(objectBuff)
    bgl.glEnable(bgl.GL_BLEND)
    bgl.glBlendFunc(bgl.GL_SRC_ALPHA, bgl.GL_ONE_MINUS_SRC_ALPHA)
    bgl.glEnable(bgl.GL_POLYGON_OFFSET_FILL)
    bgl.glPolygonOffset(0, -10)  # offset in Zbuffer to remove flicker
    bgl.glPolygonMode(bgl.GL_FRONT_AND_BACK, bgl.GL_FILL)
    bgl.glColor4f(1.0, 0.4, 0.0, self.islands_alpha)
    island = mesh.paper_island_list[mesh.paper_island_index]
    for lface in island.faces:
        face = mesh.polygons[lface.id]
        bgl.glBegin(bgl.GL_POLYGON)
        for vertex_id in face.vertices:
            vertex = mesh.vertices[vertex_id]
            bgl.glVertex4f(*vertex.co.to_4d())
        bgl.glEnd()
    bgl.glPolygonOffset(0.0, 0.0)
    bgl.glDisable(bgl.GL_POLYGON_OFFSET_FILL)
    bgl.glLoadIdentity()
display_islands.handle = None


def display_islands_changed(self, context):
    """Switch highlighting islands on/off"""
    if self.display_islands:
        if not display_islands.handle:
            display_islands.handle = bpy.types.SpaceView3D.draw_handler_add(
                display_islands, (self, context), 'WINDOW', 'POST_VIEW')
    else:
        if display_islands.handle:
            bpy.types.SpaceView3D.draw_handler_remove(display_islands.handle, 'WINDOW')
            display_islands.handle = None


def label_changed(self, context):
    """The label of an island was changed"""
    # accessing properties via [..] to avoid a recursive call after the update
    self["auto_label"] = not self.label or self.label.isspace()
    island_item_changed(self, context)


def island_item_changed(self, context):
    """The labelling of an island was changed"""
    def increment(abbrev, collisions):
        letters = "ABCDEFGHIJKLMNPQRSTUVWXYZ123456789"
        while abbrev in collisions:
            abbrev = abbrev.rstrip(letters[-1])
            abbrev = abbrev[:2] + letters[letters.find(abbrev[-1]) + 1 if len(abbrev) == 3 else 0]
        return abbrev

    # accessing properties via [..] to avoid a recursive call after the update
    island_list = context.active_object.data.paper_island_list
    if self.auto_label:
        self["label"] = ""  # avoid self-conflict
        number = 1
        while any(item.label == "Island {}".format(number) for item in island_list):
            number += 1
        self["label"] = "Island {}".format(number)
    if self.auto_abbrev:
        self["abbreviation"] = ""  # avoid self-conflict
        abbrev = "".join(first_letters(self.label))[:3].upper()
        self["abbreviation"] = increment(abbrev, {item.abbreviation for item in island_list})
    elif len(self.abbreviation) > 3:
        self["abbreviation"] = self.abbreviation[:3]
    self.name = "[{}] {} ({} {})".format(
        self.abbreviation, self.label, len(self.faces), "faces" if len(self.faces) > 1 else "face")


class FaceList(bpy.types.PropertyGroup):
    id = bpy.props.IntProperty(name="Face ID")
bpy.utils.register_class(FaceList)


class IslandList(bpy.types.PropertyGroup):
    faces = bpy.props.CollectionProperty(
        name="Faces", description="Faces belonging to this island", type=FaceList)
    label = bpy.props.StringProperty(
        name="Label", description="Label on this island",
        default="", update=label_changed)
    abbreviation = bpy.props.StringProperty(
        name="Abbreviation", description="Three-letter label to use when there is not enough space",
        default="", update=island_item_changed)
    auto_label = bpy.props.BoolProperty(
        name="Auto Label", description="Generate the label automatically",
        default=True, update=island_item_changed)
    auto_abbrev = bpy.props.BoolProperty(
        name="Auto Abbreviation", description="Generate the abbreviation automatically",
        default=True, update=island_item_changed)
bpy.utils.register_class(IslandList)


class PaperModelSettings(bpy.types.PropertyGroup):
    display_islands = bpy.props.BoolProperty(
        name="Highlight selected island", description="Highlight faces corresponding to the selected island in the 3D View",
        options={'SKIP_SAVE'}, update=display_islands_changed)
    islands_alpha = bpy.props.FloatProperty(
        name="Opacity", description="Opacity of island highlighting",
        min=0.0, max=1.0, default=0.3)
    limit_by_page = bpy.props.BoolProperty(
        name="Limit Island Size", description="Do not create islands larger than given dimensions",
        default=False, update=page_size_preset_changed)
    page_size_preset = bpy.props.EnumProperty(
        name="Page Size", description="Maximal size of an island",
        default='A4', update=page_size_preset_changed, items=global_paper_sizes)
    output_size_x = bpy.props.FloatProperty(
        name="Width", description="Maximal width of an island",
        default=0.2, soft_min=0.105, soft_max=0.841, subtype="UNSIGNED", unit="LENGTH")
    output_size_y = bpy.props.FloatProperty(
        name="Height", description="Maximal height of an island",
        default=0.29, soft_min=0.148, soft_max=1.189, subtype="UNSIGNED", unit="LENGTH")
    scale = bpy.props.FloatProperty(
        name="Scale", description="Divisor of all dimensions when exporting",
        default=1, soft_min=1.0, soft_max=10000.0, step=100, subtype='UNSIGNED', precision=1)
bpy.utils.register_class(PaperModelSettings)


def register():
    bpy.utils.register_module(__name__)

    bpy.types.Scene.paper_model = bpy.props.PointerProperty(
        name="Paper Model", description="Settings of the Export Paper Model script",
        type=PaperModelSettings, options={'SKIP_SAVE'})
    bpy.types.Mesh.paper_island_list = bpy.props.CollectionProperty(
        name="Island List", type=IslandList)
    bpy.types.Mesh.paper_island_index = bpy.props.IntProperty(
        name="Island List Index",
        default=-1, min=-1, max=100, options={'SKIP_SAVE'})
    bpy.types.INFO_MT_file_export.append(menu_func)


def unregister():
    bpy.utils.unregister_module(__name__)
    bpy.types.INFO_MT_file_export.remove(menu_func)
    if display_islands.handle:
        bpy.types.SpaceView3D.draw_handler_remove(display_islands.handle, 'WINDOW')
        display_islands.handle = None


if __name__ == "__main__":
    register()

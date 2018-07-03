import unittest

__all__ = (
    "Clay",
    "MoveLayerCollectionTesting",
    "MoveSceneCollectionSyncTesting",
    "MoveSceneCollectionTesting",
    "ViewLayerTesting",
    "compare_files",
    "dump",
    "get_layers",
    "get_scene_collections",
    "query_scene",
    "setup_extra_arguments",
)

# ############################################################
# Layer Collection Crawler
# ############################################################


def listbase_iter(data, struct, listbase):
    element = data.get_pointer((struct, listbase, b'first'))
    while element is not None:
        yield element
        element = element.get_pointer(b'next')


def linkdata_iter(collection, data):
    element = collection.get_pointer((data, b'first'))
    while element is not None:
        yield element
        element = element.get_pointer(b'next')


def get_layer_collection(layer_collection):
    data = {}
    flag = layer_collection.get(b'flag')

    data['is_visible'] = (flag & (1 << 0)) != 0
    data['is_selectable'] = (flag & (1 << 1)) != 0
    data['is_disabled'] = (flag & (1 << 2)) != 0

    scene_collection = layer_collection.get_pointer(b'scene_collection')
    if scene_collection is None:
        name = 'Fail!'
    else:
        name = scene_collection.get(b'name')
    data['name'] = name

    objects = []
    for link in linkdata_iter(layer_collection, b'object_bases'):
        ob_base = link.get_pointer(b'data')
        ob = ob_base.get_pointer(b'object')
        objects.append(ob.get((b'id', b'name'))[2:])
    data['objects'] = objects

    collections = {}
    for nested_layer_collection in linkdata_iter(layer_collection, b'layer_collections'):
        subname, subdata = get_layer_collection(nested_layer_collection)
        collections[subname] = subdata
    data['collections'] = collections

    return name, data


def get_layer(scene, layer):
    data = {}
    name = layer.get(b'name')

    data['name'] = name
    data['engine'] = scene.get((b'r', b'engine'))

    active_base = layer.get_pointer(b'basact')
    if active_base:
        ob = active_base.get_pointer(b'object')
        data['active_object'] = ob.get((b'id', b'name'))[2:]
    else:
        data['active_object'] = ""

    objects = []
    for link in linkdata_iter(layer, b'object_bases'):
        ob = link.get_pointer(b'object')
        objects.append(ob.get((b'id', b'name'))[2:])
    data['objects'] = objects

    collections = {}
    for layer_collection in linkdata_iter(layer, b'layer_collections'):
        subname, subdata = get_layer_collection(layer_collection)
        collections[subname] = subdata
    data['collections'] = collections

    return name, data


def get_layers(scene):
    """Return all the render layers and their data"""
    layers = {}
    for layer in linkdata_iter(scene, b'view_layers'):
        name, data = get_layer(scene, layer)
        layers[name] = data
    return layers


def get_scene_collection_objects(collection, listbase):
    objects = []
    for link in linkdata_iter(collection, listbase):
        ob = link.get_pointer(b'data')
        if ob is None:
            name = 'Fail!'
        else:
            name = ob.get((b'id', b'name'))[2:]
        objects.append(name)
    return objects


def get_scene_collection(collection):
    """"""
    data = {}
    name = collection.get(b'name')

    data['name'] = name
    data['objects'] = get_scene_collection_objects(collection, b'objects')

    collections = {}
    for nested_collection in linkdata_iter(collection, b'scene_collections'):
        subname, subdata = get_scene_collection(nested_collection)
        collections[subname] = subdata
    data['collections'] = collections

    return name, data


def get_scene_collections(scene):
    """Return all the scene collections ahd their data"""
    master_collection = scene.get_pointer(b'collection')
    return get_scene_collection(master_collection)


def query_scene(filepath, name, callbacks):
    """Return the equivalent to bpy.context.scene"""
    from io_blend_utils.blend import blendfile

    with blendfile.open_blend(filepath) as blend:
        scenes = [block for block in blend.blocks if block.code == b'SC']
        for scene in scenes:
            if scene.get((b'id', b'name'))[2:] != name:
                continue

            return [callback(scene) for callback in callbacks]


# ############################################################
# Utils
# ############################################################

def dump(data):
    import json
    return json.dumps(
        data,
        sort_keys=True,
        indent=4,
        separators=(',', ': '),
    )


# ############################################################
# Tests
# ############################################################

PDB = False
DUMP_DIFF = True
UPDATE_DIFF = False  # HACK used to update tests when something change


def compare_files(file_a, file_b):
    import filecmp

    if not filecmp.cmp(
            file_a,
            file_b):

        if DUMP_DIFF:
            import subprocess
            subprocess.call(["diff", "-u", file_b, file_a])

        if UPDATE_DIFF:
            import subprocess
            subprocess.call(["cp", "-u", file_a, file_b])

        if PDB:
            import pdb
            print("Files differ:", file_b, file_a)
            pdb.set_trace()

        return False

    return True


class ViewLayerTesting(unittest.TestCase):
    _test_simple = False
    _extra_arguments = []

    @classmethod
    def setUpClass(cls):
        """Runs once"""
        cls.pretest_parsing()

    @classmethod
    def get_root(cls):
        """
        return the folder with the test files
        """
        arguments = {}
        for argument in cls._extra_arguments:
            name, value = argument.split('=')
            cls.assertTrue(name and name.startswith("--"), "Invalid argument \"{0}\"".format(argument))
            cls.assertTrue(value, "Invalid argument \"{0}\"".format(argument))
            arguments[name[2:]] = value.strip('"')

        return arguments.get('testdir')

    @classmethod
    def pretest_parsing(cls):
        """
        Test if the arguments are properly set, and store ROOT
        name has extra _ because we need this test to run first
        """
        root = cls.get_root()
        cls.assertTrue(root, "Testdir not set")

    def setUp(self):
        """Runs once per test"""
        import bpy
        bpy.ops.wm.read_factory_settings()

    def path_exists(self, filepath):
        import os
        self.assertTrue(
            os.path.exists(filepath),
            "Test file \"{0}\" not found".format(filepath))

    def do_object_add(self, filepath_json, add_mode):
        """
        Testing for adding objects and see if they
        go to the right collection
        """
        import bpy
        import os
        import tempfile
        import filecmp

        ROOT = self.get_root()
        with tempfile.TemporaryDirectory() as dirpath:
            filepath_layers = os.path.join(ROOT, 'layers.blend')

            # open file
            bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)
            self.rename_collections()

            # create sub-collections
            three_b = bpy.data.objects.get('T.3b')
            three_c = bpy.data.objects.get('T.3c')

            scene = bpy.context.scene
            subzero = scene.master_collection.collections['1'].collections.new('sub-zero')
            scorpion = subzero.collections.new('scorpion')
            subzero.objects.link(three_b)
            scorpion.objects.link(three_c)
            layer = scene.view_layers.new('Fresh new Layer')
            layer.collections.link(subzero)

            # change active collection
            layer.collections.active_index = 3
            self.assertEqual(layer.collections.active.name, 'scorpion', "Run: test_syncing_object_add")

            # change active layer
            override = bpy.context.copy()
            override["view_layer"] = layer
            override["scene_collection"] = layer.collections.active.collection

            # add new objects
            if add_mode == 'EMPTY':
                bpy.ops.object.add(override)  # 'Empty'

            elif add_mode == 'CYLINDER':
                bpy.ops.mesh.primitive_cylinder_add(override)  # 'Cylinder'

            elif add_mode == 'TORUS':
                bpy.ops.mesh.primitive_torus_add(override)  # 'Torus'

            # save file
            filepath_objects = os.path.join(dirpath, 'objects.blend')
            bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath_objects)

            # get the generated json
            datas = query_scene(filepath_objects, 'Main', (get_scene_collections, get_layers))
            self.assertTrue(datas, "Data is not valid")

            filepath_objects_json = os.path.join(dirpath, "objects.json")
            with open(filepath_objects_json, "w") as f:
                for data in datas:
                    f.write(dump(data))

            self.assertTrue(compare_files(
                filepath_objects_json,
                filepath_json,
            ),
                "Scene dump files differ")

    def do_object_add_no_collection(self, add_mode):
        """
        Test for adding objects when no collection
        exists in render layer
        """
        import bpy

        # empty layer of collections

        layer = bpy.context.view_layer
        while layer.collections:
            layer.collections.unlink(layer.collections[0])

        # add new objects
        if add_mode == 'EMPTY':
            bpy.ops.object.add()  # 'Empty'

        elif add_mode == 'CYLINDER':
            bpy.ops.mesh.primitive_cylinder_add()  # 'Cylinder'

        elif add_mode == 'TORUS':
            bpy.ops.mesh.primitive_torus_add()  # 'Torus'

        self.assertEqual(len(layer.collections), 1, "New collection not created")
        collection = layer.collections[0]
        self.assertEqual(len(collection.objects), 1, "New collection is empty")

    def do_object_link(self, master_collection):
        import bpy
        self.assertEqual(master_collection.name, "Master Collection")
        self.assertEqual(master_collection, bpy.context.scene.master_collection)
        master_collection.objects.link(bpy.data.objects.new('object', None))

    def do_scene_copy(self, filepath_json_reference, copy_mode, data_callbacks):
        import bpy
        import os
        import tempfile
        import filecmp

        ROOT = self.get_root()
        with tempfile.TemporaryDirectory() as dirpath:
            filepath_layers = os.path.join(ROOT, 'layers.blend')

            (self.path_exists(f) for f in (
                filepath_layers,
                filepath_json_reference,
            ))

            filepath_saved = os.path.join(dirpath, '{0}.blend'.format(copy_mode))
            filepath_json = os.path.join(dirpath, "{0}.json".format(copy_mode))

            bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)
            self.rename_collections()
            bpy.ops.scene.new(type=copy_mode)
            bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath_saved)

            datas = query_scene(filepath_saved, 'Main.001', data_callbacks)
            self.assertTrue(datas, "Data is not valid")

            with open(filepath_json, "w") as f:
                for data in datas:
                    f.write(dump(data))

            self.assertTrue(compare_files(
                filepath_json,
                filepath_json_reference,
            ),
                "Scene copy \"{0}\" test failed".format(copy_mode.title()))

    def do_object_delete(self, del_mode):
        import bpy
        import os
        import tempfile
        import filecmp

        ROOT = self.get_root()
        with tempfile.TemporaryDirectory() as dirpath:
            filepath_layers = os.path.join(ROOT, 'layers.blend')
            filepath_reference_json = os.path.join(ROOT, 'layers_object_delete.json')

            # open file
            bpy.ops.wm.open_mainfile('EXEC_DEFAULT', filepath=filepath_layers)
            self.rename_collections()

            # create sub-collections
            three_b = bpy.data.objects.get('T.3b')
            three_d = bpy.data.objects.get('T.3d')

            scene = bpy.context.scene

            # mangle the file a bit with some objects linked across collections
            subzero = scene.master_collection.collections['1'].collections.new('sub-zero')
            scorpion = subzero.collections.new('scorpion')
            subzero.objects.link(three_d)
            scorpion.objects.link(three_b)
            scorpion.objects.link(three_d)

            # object to delete
            ob = three_d

            # delete object
            if del_mode == 'DATA':
                bpy.data.objects.remove(ob, do_unlink=True)

            elif del_mode == 'OPERATOR':
                bpy.context.scene.update()  # update depsgraph
                bpy.ops.object.select_all(action='DESELECT')
                ob.select_set(action='SELECT')
                self.assertTrue(ob.select_get())
                bpy.ops.object.delete()

            # save file
            filepath_generated = os.path.join(dirpath, 'generated.blend')
            bpy.ops.wm.save_mainfile('EXEC_DEFAULT', filepath=filepath_generated)

            # get the generated json
            datas = query_scene(filepath_generated, 'Main', (get_scene_collections, get_layers))
            self.assertTrue(datas, "Data is not valid")

            filepath_generated_json = os.path.join(dirpath, "generated.json")
            with open(filepath_generated_json, "w") as f:
                for data in datas:
                    f.write(dump(data))

            self.assertTrue(compare_files(
                filepath_generated_json,
                filepath_reference_json,
            ),
                "Scene dump files differ")

    def do_visibility_object_add(self, add_mode):
        import bpy

        scene = bpy.context.scene

        # delete all objects of the file
        for ob in bpy.data.objects:
            bpy.data.objects.remove(ob, do_unlink=True)

        # real test
        layer = scene.view_layers.new('Visibility Test')
        layer.collections.unlink(layer.collections[0])

        scene_collection = scene.master_collection.collections.new("Collection")
        layer.collections.link(scene_collection)

        bpy.context.scene.update()  # update depsgraph

        self.assertEqual(len(bpy.data.objects), 0)

        # add new objects
        if add_mode == 'EMPTY':
            bpy.ops.object.add()  # 'Empty'

        elif add_mode == 'CYLINDER':
            bpy.ops.mesh.primitive_cylinder_add()  # 'Cylinder'

        elif add_mode == 'TORUS':
            bpy.ops.mesh.primitive_torus_add()  # 'Torus'

        self.assertEqual(len(bpy.data.objects), 1)

        new_ob = bpy.data.objects[0]
        self.assertTrue(new_ob.visible_get(), "Object should be visible")

    def cleanup_tree(self):
        """
        Remove any existent layer and collections,
        leaving only the one view_layer we can't remove
        """
        import bpy
        scene = bpy.context.scene
        while len(scene.view_layers) > 1:
            scene.view_layers.remove(scene.view_layers[1])

        layer = scene.view_layers[0]
        while layer.collections:
            layer.collections.unlink(layer.collections[0])

        master_collection = scene.master_collection
        while master_collection.collections:
            master_collection.collections.remove(master_collection.collections[0])

    def rename_collections(self, collection=None):
        """
        Rename 'Collection 1' to '1'
        """
        def strip_name(collection):
            import re
            if collection.name.startswith("Default Collection"):
                collection.name = '1'
            else:
                collection.name = re.findall(r'\d+', collection.name)[0]

        if collection is None:
            import bpy
            collection = bpy.context.scene.master_collection

        for nested_collection in collection.collections:
            strip_name(nested_collection)
            self.rename_collections(nested_collection)


class MoveSceneCollectionTesting(ViewLayerTesting):
    """
    To be used by tests of view_layer_move_into_scene_collection
    """

    def get_initial_scene_tree_map(self):
        collections_map = [
            ['A', [
                ['i', None],
                ['ii', None],
                ['iii', None],
            ]],
            ['B', None],
            ['C', [
                ['1', None],
                ['2', None],
                ['3', [
                    ['dog', None],
                    ['cat', None],
                ]],
            ]],
        ]
        return collections_map

    def build_scene_tree(self, tree_map, collection=None, ret_dict=None):
        """
        Returns a flat dictionary with new scene collections
        created from a nested tuple of nested tuples (name, tuple)
        """
        import bpy

        if collection is None:
            collection = bpy.context.scene.master_collection

        if ret_dict is None:
            ret_dict = {collection.name: collection}
            self.assertEqual(collection.name, "Master Collection")

        for name, nested_collections in tree_map:
            new_collection = collection.collections.new(name)
            ret_dict[name] = new_collection

            if nested_collections:
                self.build_scene_tree(nested_collections, new_collection, ret_dict)

        return ret_dict

    def setup_tree(self):
        """
        Cleanup file, and populate it with class scene tree map
        """
        self.cleanup_tree()
        self.assertTrue(
            hasattr(self, "get_initial_scene_tree_map"),
            "Test class has no get_initial_scene_tree_map method implemented")

        return self.build_scene_tree(self.get_initial_scene_tree_map())

    def get_scene_tree_map(self, collection=None, ret_list=None):
        """
        Extract the scene collection tree from scene
        Return as a nested list of nested lists (name, list)
        """
        import bpy

        if collection is None:
            scene = bpy.context.scene
            collection = scene.master_collection

        if ret_list is None:
            ret_list = []

        for nested_collection in collection.collections:
            new_collection = [nested_collection.name, None]
            ret_list.append(new_collection)

            if nested_collection.collections:
                new_collection[1] = list()
                self.get_scene_tree_map(nested_collection, new_collection[1])

        return ret_list

    def compare_tree_maps(self):
        """
        Compare scene with expected (class defined) data
        """
        self.assertEqual(self.get_scene_tree_map(), self.get_reference_scene_tree_map())


class MoveSceneCollectionSyncTesting(MoveSceneCollectionTesting):
    """
    To be used by tests of view_layer_move_into_scene_collection_sync
    """

    def get_initial_layers_tree_map(self):
        layers_map = [
            ['Layer 1', [
                'Master Collection',
                'C',
                '3',
            ]],
            ['Layer 2', [
                'C',
                '3',
                'dog',
                'cat',
            ]],
        ]
        return layers_map

    def get_reference_layers_tree_map(self):
        """
        For those classes we don't expect any changes in the layer tree
        """
        return self.get_initial_layers_tree_map()

    def setup_tree(self):
        tree = super(MoveSceneCollectionSyncTesting, self).setup_tree()

        import bpy
        scene = bpy.context.scene

        self.assertTrue(
            hasattr(self, "get_initial_layers_tree_map"),
            "Test class has no get_initial_layers_tree_map method implemented")

        layers_map = self.get_initial_layers_tree_map()

        for layer_name, collections_names in layers_map:
            layer = scene.view_layers.new(layer_name)
            layer.collections.unlink(layer.collections[0])

            for collection_name in collections_names:
                layer.collections.link(tree[collection_name])

        return tree

    def compare_tree_maps(self):
        """
        Compare scene with expected (class defined) data
        """
        super(MoveSceneCollectionSyncTesting, self).compare_tree_maps()

        import bpy
        scene = bpy.context.scene
        layers_map = self.get_reference_layers_tree_map()

        for layer_name, collections_names in layers_map:
            layer = scene.view_layers.get(layer_name)
            self.assertTrue(layer)
            self.assertEqual(len(collections_names), len(layer.collections))

            for i, collection_name in enumerate(collections_names):
                self.assertEqual(collection_name, layer.collections[i].name)
                self.verify_collection_tree(layer.collections[i])

    def verify_collection_tree(self, layer_collection):
        """
        Check if the LayerCollection mimics the SceneLayer tree
        """
        scene_collection = layer_collection.collection
        self.assertEqual(len(layer_collection.collections), len(scene_collection.collections))

        for i, nested_collection in enumerate(layer_collection.collections):
            self.assertEqual(nested_collection.collection.name, scene_collection.collections[i].name)
            self.assertEqual(nested_collection.collection, scene_collection.collections[i])
            self.verify_collection_tree(nested_collection)


class MoveLayerCollectionTesting(MoveSceneCollectionSyncTesting):
    """
    To be used by tests of view_layer_move_into_layer_collection
    """

    def parse_move(self, path, sep='.'):
        """
        convert 'Layer 1.C.2' into:
        bpy.context.scene.view_layers['Layer 1'].collections['C'].collections['2']
        """
        import bpy

        paths = path.split(sep)
        layer = bpy.context.scene.view_layers[paths[0]]
        collections = layer.collections

        for subpath in paths[1:]:
            collection = collections[subpath]
            collections = collection.collections

        return collection

    def move_into(self, src, dst):
        layer_collection_src = self.parse_move(src)
        layer_collection_dst = self.parse_move(dst)
        return layer_collection_src.move_into(layer_collection_dst)

    def move_above(self, src, dst):
        layer_collection_src = self.parse_move(src)
        layer_collection_dst = self.parse_move(dst)
        return layer_collection_src.move_above(layer_collection_dst)

    def move_below(self, src, dst):
        layer_collection_src = self.parse_move(src)
        layer_collection_dst = self.parse_move(dst)
        return layer_collection_src.move_below(layer_collection_dst)


class Clay:
    def __init__(self, extra_kid_layer=False):
        import bpy

        self._scene = bpy.context.scene
        self._layer = self._fresh_layer()
        self._object = bpy.data.objects.new('guinea pig', bpy.data.meshes.new('mesh'))

        # update depsgraph
        self._scene.update()

        scene_collection_grandma = self._scene.master_collection.collections.new("Grandma")
        scene_collection_mom = scene_collection_grandma.collections.new("Mom")
        scene_collection_kid = scene_collection_mom.collections.new("Kid")
        scene_collection_kid.objects.link(self._object)

        layer_collection_grandma = self._layer.collections.link(scene_collection_grandma)
        layer_collection_mom = layer_collection_grandma.collections[0]
        layer_collection_kid = layer_collection_mom.collections[0]

        # store the variables
        self._scene_collections = {
            'grandma': scene_collection_grandma,
            'mom': scene_collection_mom,
            'kid': scene_collection_kid,
        }
        self._layer_collections = {
            'grandma': layer_collection_grandma,
            'mom': layer_collection_mom,
            'kid': layer_collection_kid,
        }

        if extra_kid_layer:
            layer_collection_extra = self._layer.collections.link(scene_collection_kid)
            self._layer_collections['extra'] = layer_collection_extra

        self._update()

    def _fresh_layer(self):
        import bpy

        # remove all other objects
        while bpy.data.objects:
            bpy.data.objects.remove(bpy.data.objects[0])

        # remove all the other collections
        while self._scene.master_collection.collections:
            self._scene.master_collection.collections.remove(
                self._scene.master_collection.collections[0])

        layer = self._scene.view_layers.new('Evaluation Test')
        layer.collections.unlink(layer.collections[0])
        bpy.context.window.view_layer = layer

        # remove all other layers
        for layer_iter in self._scene.view_layers:
            if layer_iter != layer:
                self._scene.view_layers.remove(layer_iter)

        return layer

    def _update(self):
        """
        Force depsgrpah evaluation
        and update pointers to IDProperty collections
        """
        ENGINE = 'BLENDER_CLAY'

        self._scene.update()  # update depsgraph
        self._layer.update()  # flush depsgraph evaluation

        # change scene settings
        self._properties = {
            'scene': self._scene.collection_properties[ENGINE],
            'object': self._object.collection_properties[ENGINE],
        }

        for key, value in self._layer_collections.items():
            self._properties[key] = self._layer_collections[key].engine_overrides[ENGINE]

    def get(self, name, data_path):
        self._update()
        return getattr(self._properties[name], data_path)

    def set(self, name, data_path, value):
        self._update()
        self._properties[name].use(data_path)
        setattr(self._properties[name], data_path, value)


def setup_extra_arguments(filepath):
    """
    Create a value which is assigned to: ``UnitTesting._extra_arguments``
    """
    import sys

    extra_arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    sys.argv = [filepath] + extra_arguments[1:]

    return extra_arguments

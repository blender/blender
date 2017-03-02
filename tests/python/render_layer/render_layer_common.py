import unittest

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

    data['is_visible']    = (flag & (1 << 0)) != 0;
    data['is_selectable'] = (flag & (1 << 1)) != 0;
    data['is_folded']     = (flag & (1 << 2)) != 0;

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


def get_layer(layer):
    data = {}
    name = layer.get(b'name')

    data['name'] = name
    data['active_object'] = layer.get((b'basact', b'object', b'id', b'name'))[2:]
    data['engine'] = layer.get(b'engine')

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
    for layer in linkdata_iter(scene, b'render_layers'):
        name, data = get_layer(layer)
        layers[name] = data
    return layers


def get_scene_collection_objects(collection, listbase):
    objects = []
    for link in linkdata_iter(collection, listbase):
        ob = link.get_pointer(b'data')
        if ob is None:
            name  = 'Fail!'
        else:
            name = ob.get((b'id', b'name'))[2:]
        objects.append(name)
    return objects


def get_scene_collection(collection):
    """"""
    data = {}
    name = collection.get(b'name')

    data['name'] = name
    data['filter'] = collection.get(b'filter')

    data['objects'] = get_scene_collection_objects(collection, b'objects')
    data['filter_objects'] = get_scene_collection_objects(collection, b'filter_objects')

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
    import blendfile
    with blendfile.open_blend(filepath) as blend:
        scenes = [block for block in blend.blocks if block.code == b'SC']
        for scene in scenes:
            if scene.get((b'id', b'name'))[2:] == name:
                output = []
                for callback in callbacks:
                    output.append(callback(scene))
                return output


# ############################################################
# Utils
# ############################################################

def import_blendfile():
    import bpy
    import os, sys
    path = os.path.join(
            bpy.utils.resource_path('LOCAL'),
            'scripts',
            'addons',
            'io_blend_utils',
            'blend',
            )

    if path not in sys.path:
        sys.path.append(path)


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

def compare_files(file_a, file_b):
    import filecmp

    if not filecmp.cmp(
        file_a,
        file_b):

        if DUMP_DIFF:
            import subprocess
            subprocess.call(["diff", "-u", file_a, file_b])

        if PDB:
            import pdb
            print("Files differ:", file_a, file_b)
            pdb.set_trace()

        return False

    return True


class RenderLayerTesting(unittest.TestCase):
    _test_simple = False
    _extra_arguments = []

    @classmethod
    def setUpClass(cls):
        """Runs once"""
        cls.pretest_import_blendfile()
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

    @staticmethod
    def pretest_import_blendfile():
        """
        Make sure blendfile imports with no problems
        name has extra _ because we need this test to run first
        """
        import_blendfile()
        import blendfile

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

            # create sub-collections
            three_b = bpy.data.objects.get('T.3b')
            three_c = bpy.data.objects.get('T.3c')

            scene = bpy.context.scene
            subzero = scene.master_collection.collections['1'].collections.new('sub-zero')
            scorpion = subzero.collections.new('scorpion')
            subzero.objects.link(three_b)
            scorpion.objects.link(three_c)
            layer = scene.render_layers.new('Fresh new Layer')
            layer.collections.link(subzero)

            # change active collection
            layer.collections.active_index = 3
            self.assertEqual(layer.collections.active.name, 'scorpion', "Run: test_syncing_object_add")

            # change active layer
            override = bpy.context.copy()
            override["render_layer"] = layer
            override["scene_collection"] = layer.collections.active.collection

            # add new objects
            if add_mode == 'EMPTY':
                bpy.ops.object.add(override) # 'Empty'

            elif add_mode == 'CYLINDER':
                bpy.ops.mesh.primitive_cylinder_add(override) # 'Cylinder'

            elif add_mode == 'TORUS':
                bpy.ops.mesh.primitive_torus_add(override) # 'Torus'

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

        layer = bpy.context.render_layer
        while layer.collections:
            layer.collections.unlink(layer.collections[0])

        # add new objects
        if add_mode == 'EMPTY':
            bpy.ops.object.add() # 'Empty'

        elif add_mode == 'CYLINDER':
            bpy.ops.mesh.primitive_cylinder_add() # 'Cylinder'

        elif add_mode == 'TORUS':
            bpy.ops.mesh.primitive_torus_add() # 'Torus'

        self.assertEqual(len(layer.collections), 1, "New collection not created")
        collection = layer.collections[0]
        self.assertEqual(len(collection.objects), 1, "New collection is empty")

    def do_object_link(self, master_collection):
        import bpy
        self.assertEqual(master_collection.name, "Master Collection")
        self.assertEqual(master_collection, bpy.context.scene.master_collection)
        master_collection.objects.link(bpy.data.objects.new('object', None))


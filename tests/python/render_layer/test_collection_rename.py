# ./blender.bin --background -noaudio --python tests/python/render_layer/test_collection_rename.py -- --testdir="/data/lib/tests/"

# ############################################################
# Importing - Same For All Render Layer Tests
# ############################################################

import unittest

import os, sys
sys.path.append(os.path.dirname(__file__))

from render_layer_common import *


# ############################################################
# Testing
# ############################################################

class UnitTesting(RenderLayerTesting):
    def setup_family(self):
        import bpy
        scene = bpy.context.scene

        # Just add a bunch of collections on which we can do various tests.
        grandma = scene.master_collection.collections.new('grandma')
        grandpa = scene.master_collection.collections.new('grandpa')
        mom = grandma.collections.new('mom')
        son = mom.collections.new('son')
        daughter = mom.collections.new('daughter')
        uncle = grandma.collections.new('uncle')
        cousin = uncle.collections.new('cousin')

        lookup = {c.name: c for c in (grandma, grandpa, mom, son, daughter, uncle, cousin)}
        return lookup

    def test_rename_a(self):
        family = self.setup_family()

        family['mom'].name = family['daughter'].name
        self.assertNotEqual(family['mom'].name, family['daughter'].name)

    def test_rename_b(self):
        family = self.setup_family()

        family['grandma'].name = family['grandpa'].name
        self.assertNotEqual(family['grandma'].name, family['grandpa'].name)

    def test_rename_c(self):
        family = self.setup_family()

        family['cousin'].name = family['daughter'].name
        self.assertNotEqual(family['cousin'].name, family['daughter'].name)

    def test_rename_d(self):
        family = self.setup_family()

        family['son'].name = family['daughter'].name
        self.assertNotEqual(family['son'].name, family['daughter'].name)

    def test_add_equal_name_a(self):
        family = self.setup_family()

        other_daughter = family['mom'].collections.new(family['daughter'].name)
        self.assertNotEqual(other_daughter.name, family['daughter'].name)

    def test_add_equal_name_b(self):
        family = self.setup_family()

        other_aunt = family['grandma'].collections.new(family['daughter'].name)
        self.assertNotEqual(other_aunt.name, family['daughter'].name)


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    import sys

    extra_arguments = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    sys.argv = [__file__] + (sys.argv[sys.argv.index("--") + 2:] if "--" in sys.argv else [])

    UnitTesting._extra_arguments = extra_arguments
    unittest.main()

# ############################################################
# Importing - Same For All Render Layer Tests
# ############################################################

import unittest
import os
import sys

from view_layer_common import *


# ############################################################
# Testing
# ############################################################

class UnitTesting(ViewLayerTesting):
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
        # Since they are not siblings, we allow them to have the same name.
        self.assertEqual(family['mom'].name, family['daughter'].name)

    def test_rename_b(self):
        family = self.setup_family()

        family['grandma'].name = family['grandpa'].name
        self.assertNotEqual(family['grandma'].name, family['grandpa'].name)

    def test_rename_c(self):
        family = self.setup_family()

        family['cousin'].name = family['daughter'].name
        # Since they are not siblings, we allow them to have the same name.
        self.assertEqual(family['cousin'].name, family['daughter'].name)

    def test_rename_d(self):
        family = self.setup_family()

        family['son'].name = family['daughter'].name
        self.assertNotEqual(family['son'].name, family['daughter'].name)

    def test_rename_e(self):
        family = self.setup_family()

        family['grandma'].name = family['grandpa'].name
        self.assertNotEqual(family['grandma'].name, family['grandpa'].name)

    def test_add_equal_name_a(self):
        family = self.setup_family()

        other_daughter = family['mom'].collections.new(family['daughter'].name)
        self.assertNotEqual(other_daughter.name, family['daughter'].name)

    def test_add_equal_name_b(self):
        family = self.setup_family()

        other_aunt = family['grandma'].collections.new(family['daughter'].name)
        # Since they are not siblings, we allow them to have the same name.
        self.assertEqual(other_aunt.name, family['daughter'].name)

    def test_add_equal_name_c(self):
        family = self.setup_family()

        other_aunt = family['grandma'].collections.new(family['mom'].name)
        self.assertNotEqual(other_aunt.name, family['mom'].name)


# ############################################################
# Main - Same For All Render Layer Tests
# ############################################################

if __name__ == '__main__':
    UnitTesting._extra_arguments = setup_extra_arguments(__file__)
    unittest.main()

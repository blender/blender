# Apache License, Version 2.0

import unittest
import random

test= bpy.data.test

# farr - 1-dimensional array of float
# fdarr - dynamic 1-dimensional array of float
# fmarr - 3-dimensional ([3][4][5]) array of float
# fdmarr - dynamic 3-dimensional (ditto size) array of float

# same as above for other types except that the first letter is "i" for int and "b" for bool

class TestArray(unittest.TestCase):
    # test that assignment works by: assign -> test value
    # - rvalue = list of float
    # - rvalue = list of numbers
    # test.object
    # bpy.data.test.farr[3], iarr[3], barr[...], fmarr, imarr, bmarr

    def setUp(self):
        test.farr= (1.0, 2.0, 3.0)
        test.iarr= (7, 8, 9)
        test.barr= (False, True, False)
    
    # test access
    # test slice access, negative indices
    def test_access(self):
        rvals= ([1.0, 2.0, 3.0], [7, 8, 9], [False, True, False])
        for arr, rval in zip((test.farr, test.iarr, test.barr), rvals):
            self.assertEqual(prop_to_list(arr), rval)
            self.assertEqual(arr[0:3], rval)
            self.assertEqual(arr[1:2], rval[1:2])
            self.assertEqual(arr[-1], arr[2])
            self.assertEqual(arr[-2], arr[1])
            self.assertEqual(arr[-3], arr[0])

    # fail when index out of bounds
    def test_access_fail(self):
        for arr in (test.farr, test.iarr, test.barr):
            self.assertRaises(IndexError, lambda : arr[4])
    
    # test assignment of a whole array
    def test_assign_array(self):
        # should accept int as float
        test.farr= (1, 2, 3)

    # fail when: unexpected no. of items, invalid item type
    def test_assign_array_fail(self):
        def assign_empty_list(arr):
            setattr(test, arr, ())

        for arr in ("farr", "iarr", "barr"):
            self.assertRaises(ValueError, assign_empty_list, arr)

        def assign_invalid_float():
            test.farr= (1.0, 2.0, "3.0")

        def assign_invalid_int():
            test.iarr= ("1", 2, 3)

        def assign_invalid_bool():
            test.barr= (True, 0.123, False)

        for func in [assign_invalid_float, assign_invalid_int, assign_invalid_bool]:
            self.assertRaises(TypeError, func)

        # shouldn't accept float as int
        def assign_float_as_int():
            test.iarr= (1, 2, 3.0)
        self.assertRaises(TypeError, assign_float_as_int)

        # non-dynamic arrays cannot change size
        def assign_different_size(arr, val):
            setattr(test, arr, val)
        for arr, val in zip(("iarr", "farr", "barr"), ((1, 2), (1.0, 2.0), (True, False))):
            self.assertRaises(ValueError, assign_different_size, arr, val)

    # test assignment of specific items
    def test_assign_item(self):
        for arr, rand_func in zip((test.farr, test.iarr, test.barr), (rand_float, rand_int, rand_bool)):
            for i in range(len(arr)):
                val= rand_func()
                arr[i] = val
                
                self.assertEqual(arr[i], val)

        # float prop should accept also int
        for i in range(len(test.farr)):
            val= rand_int()
            test.farr[i] = val
            self.assertEqual(test.farr[i], float(val))

        # 

    def test_assign_item_fail(self):
        def assign_bad_index(arr):
            arr[4] = 1.0

        def assign_bad_type(arr):
            arr[1] = "123"
            
        for arr in [test.farr, test.iarr, test.barr]:
            self.assertRaises(IndexError, assign_bad_index, arr)

        # not testing bool because bool allows not only (True|False)
        for arr in [test.farr, test.iarr]:    
            self.assertRaises(TypeError, assign_bad_type, arr)

    def test_dynamic_assign_array(self):
        # test various lengths here
        for arr, rand_func in zip(("fdarr", "idarr", "bdarr"), (rand_float, rand_int, rand_bool)):
            for length in range(1, 64):
                rval= make_random_array(length, rand_func)
                setattr(test, arr, rval)
                self.assertEqual(prop_to_list(getattr(test, arr)), rval)

    def test_dynamic_assign_array_fail(self):
        # could also test too big length here
        
        def assign_empty_list(arr):
            setattr(test, arr, ())

        for arr in ("fdarr", "idarr", "bdarr"):
            self.assertRaises(ValueError, assign_empty_list, arr)


class TestMArray(unittest.TestCase):
    def setUp(self):
        # reset dynamic array sizes
        for arr, func in zip(("fdmarr", "idmarr", "bdmarr"), (rand_float, rand_int, rand_bool)):
            setattr(test, arr, make_random_3d_array((3, 4, 5), func))

    # test assignment
    def test_assign_array(self):
        for arr, func in zip(("fmarr", "imarr", "bmarr"), (rand_float, rand_int, rand_bool)):
            # assignment of [3][4][5]
            rval= make_random_3d_array((3, 4, 5), func)
            setattr(test, arr, rval)
            self.assertEqual(prop_to_list(getattr(test, arr)), rval)

        # test assignment of [2][4][5], [1][4][5] should work on dynamic arrays

    def test_assign_array_fail(self):
        def assign_empty_array():
            test.fmarr= ()
        self.assertRaises(ValueError, assign_empty_array)

        def assign_invalid_size(arr, rval):
            setattr(test, arr, rval)

        # assignment of 3,4,4 or 3,3,5 should raise ex
        for arr, func in zip(("fmarr", "imarr", "bmarr"), (rand_float, rand_int, rand_bool)):
            rval= make_random_3d_array((3, 4, 4), func)
            self.assertRaises(ValueError, assign_invalid_size, arr, rval)

            rval= make_random_3d_array((3, 3, 5), func)
            self.assertRaises(ValueError, assign_invalid_size, arr, rval)

            rval= make_random_3d_array((3, 3, 3), func)
            self.assertRaises(ValueError, assign_invalid_size, arr, rval)

    def test_assign_item(self):
        # arr[i] = x
        for arr, func in zip(("fmarr", "imarr", "bmarr", "fdmarr", "idmarr", "bdmarr"), (rand_float, rand_int, rand_bool) * 2):
            rval= make_random_2d_array((4, 5), func)

            for i in range(3):
                getattr(test, arr)[i] = rval
                self.assertEqual(prop_to_list(getattr(test, arr)[i]), rval)

        # arr[i][j] = x
        for arr, func in zip(("fmarr", "imarr", "bmarr", "fdmarr", "idmarr", "bdmarr"), (rand_float, rand_int, rand_bool) * 2):

            arr= getattr(test, arr)
            rval= make_random_array(5, func)

            for i in range(3):
                for j in range(4):
                    arr[i][j] = rval
                    self.assertEqual(prop_to_list(arr[i][j]), rval)


    def test_assign_item_fail(self):
        def assign_wrong_size(arr, i, rval):
            getattr(test, arr)[i] = rval

        # assign wrong size at level 2
        for arr, func in zip(("fmarr", "imarr", "bmarr"), (rand_float, rand_int, rand_bool)):
            rval1= make_random_2d_array((3, 5), func)
            rval2= make_random_2d_array((4, 3), func)

            for i in range(3):
                self.assertRaises(ValueError, assign_wrong_size, arr, i, rval1)
                self.assertRaises(ValueError, assign_wrong_size, arr, i, rval2)

    def test_dynamic_assign_array(self):
        for arr, func in zip(("fdmarr", "idmarr", "bdmarr"), (rand_float, rand_int, rand_bool)):
            # assignment of [3][4][5]
            rval= make_random_3d_array((3, 4, 5), func)
            setattr(test, arr, rval)
            self.assertEqual(prop_to_list(getattr(test, arr)), rval)

            # [2][4][5]
            rval= make_random_3d_array((2, 4, 5), func)
            setattr(test, arr, rval)
            self.assertEqual(prop_to_list(getattr(test, arr)), rval)

            # [1][4][5]
            rval= make_random_3d_array((1, 4, 5), func)
            setattr(test, arr, rval)
            self.assertEqual(prop_to_list(getattr(test, arr)), rval)


    # test access
    def test_access(self):
        pass

    # test slice access, negative indices
    def test_access_fail(self):
        pass

random.seed()

def rand_int():
    return random.randint(-1000, 1000)

def rand_float():
    return float(rand_int())

def rand_bool():
    return bool(random.randint(0, 1))

def make_random_array(len, rand_func):
    arr= []
    for i in range(len):
        arr.append(rand_func())
        
    return arr

def make_random_2d_array(dimsize, rand_func):
    marr= []
    for i in range(dimsize[0]):
        marr.append([])

        for j in range(dimsize[1]):
            marr[-1].append(rand_func())

    return marr

def make_random_3d_array(dimsize, rand_func):
    marr= []
    for i in range(dimsize[0]):
        marr.append([])

        for j in range(dimsize[1]):
            marr[-1].append([])

            for k in range(dimsize[2]):
                marr[-1][-1].append(rand_func())

    return marr

def prop_to_list(prop):
    ret= []

    for x in prop:
        if type(x) not in {bool, int, float}:
            ret.append(prop_to_list(x))
        else:
            ret.append(x)

    return ret

def suite():
    return unittest.TestSuite([unittest.TestLoader().loadTestsFromTestCase(TestArray), unittest.TestLoader().loadTestsFromTestCase(TestMArray)])

if __name__ == "__main__":
    unittest.TextTestRunner(verbosity=2).run(suite())


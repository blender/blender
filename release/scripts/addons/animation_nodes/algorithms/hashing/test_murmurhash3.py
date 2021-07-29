from unittest import TestCase
from . murmurhash3 import strToInt

class TestMurmurHash3(TestCase):
    def testNormal(self):
        self.assertEqual(strToInt(""), 0)
        self.assertEqual(strToInt("a"), 1009084850)
        self.assertEqual(strToInt("abc"), 3017643002)
        self.assertEqual(strToInt("This is a test."), 2040885872)

    def testSpecialCharacters(self):
        self.assertEqual(strToInt("äöü!§$%&/()[]?ß"), 4172822797)

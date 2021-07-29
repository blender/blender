import unittest
from . utils.handlers import eventHandler
from . preferences import testsAreEnabled

@eventHandler("ADDON_LOAD_POST")
def autoRunTestsInDebugMode():
    if testsAreEnabled():
        runTests()

def runTests():
    print("\n" * 2)
    print("Start running the Animation Nodes test suite.")
    print("Can be disabled in the user preferences of the addon.\n")

    testLoader = unittest.TestLoader()
    testLoader.testMethodPrefix = "test" # <- change to only run selected tests
    allTests = testLoader.discover("animation_nodes", pattern = "test*")
    unittest.TextTestRunner(verbosity = 1).run(allTests)

    print("\n" * 2)

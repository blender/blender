paths = {}

def setup(utils):
    global paths
    paths = {
        "implementation" : utils.changeFileName(__file__, "__virtual_clist_implementation.src"),
        "declaration" : utils.changeFileName(__file__, "__virtual_clist_declaration.src"),
        "types" : utils.changeFileName(__file__, "virtual_clist_types.json")
    }

def getPyPreprocessTasks(PyPreprocessTask, utils):
    dependencies = [
        __file__,
        paths["implementation"],
        paths["declaration"],
        paths["types"]
    ]
    pyxTask = PyPreprocessTask(
        target = utils.changeFileName(__file__, "virtual_clists.pyx"),
        dependencies = dependencies,
        function = generatePyx
    )
    pxdTask = PyPreprocessTask(
        target = utils.changeFileName(__file__, "virtual_clists.pxd"),
        dependencies = dependencies,
        function = generatePxd
    )
    return [pyxTask, pxdTask]

def generatePyx(target, utils):
    source = utils.readTextFile(paths["implementation"])
    types = utils.readJsonFile(paths["types"])

    parts = []
    parts.append("cimport cython")

    for listName, info in types.items():
        code = utils.multiReplace(source,
            LISTNAME = listName,
            TYPE = info["Type"],
            OPTIONAL_STAR = "*" if info["Return"] == "Pointer" else "",
            OPTIONAL_DEREF = "" if info["Return"] == "Pointer" else "[0]",
            OPTIONAL_INV_DEREF = "[0]" if info["Return"] == "Pointer" else "")
        parts.append(code)

    utils.writeTextFile(target, "\n\n".join(parts))

def generatePxd(target, utils):
    source = utils.readTextFile(paths["declaration"])
    types = utils.readJsonFile(paths["types"])

    parts = []
    parts.append("from . virtual_list cimport VirtualList")

    for listName, info in types.items():
        parts.append("from .. lists.base_lists cimport " + listName)
        parts.append(info["Import"])
        code = utils.multiReplace(source,
            LISTNAME = listName,
            TYPE = info["Type"],
            OPTIONAL_STAR = "*" if info["Return"] == "Pointer" else "")
        parts.append(code)

    utils.writeTextFile(target, "\n\n".join(parts))

import textwrap

paths = {}

def setup(utils):
    global paths
    paths = {
        "implementation" : utils.changeFileName(__file__, "__types_implementation.src"),
        "declaration" : utils.changeFileName(__file__, "__types_declaration.src"),
        "pyx imports" : utils.changeFileName(__file__, "__pyx_imports.src"),
        "pxd imports" : utils.changeFileName(__file__, "__pxd_imports.src")
    }

def getPyPreprocessTasks(PyPreprocessTask, utils):
    dependencies = [
        __file__,
        paths["implementation"],
        paths["pyx imports"],
        paths["pxd imports"]
    ]

    pyxTask = PyPreprocessTask(
        target = utils.changeFileName(__file__, "action_types.pyx"),
        dependencies = dependencies,
        function = generate_pyx
    )

    pxdTask = PyPreprocessTask(
        target = utils.changeFileName(__file__, "action_types.pxd"),
        dependencies = dependencies,
        function = generate_pxd
    )

    return [pyxTask, pxdTask]

def generate_pyx(target, utils):
    parts = []

    parts.append(utils.readTextFile(paths["pyx imports"]))

    source = utils.readTextFile(paths["implementation"])
    parts.append(process(utils, source, {"BOUNDTYPE" : "Bounded"}))
    parts.append(process(utils, source, {"BOUNDTYPE" : "Unbounded"}))

    utils.writeTextFile(target, "\n\n".join(parts))

def generate_pxd(target, utils):
    parts = []

    parts.append(utils.readTextFile(paths["pxd imports"]))

    source = utils.readTextFile(paths["declaration"])
    parts.append(process(utils, source, {"BOUNDTYPE" : "Bounded"}))
    parts.append(process(utils, source, {"BOUNDTYPE" : "Unbounded"}))

    utils.writeTextFile(target, "\n\n".join(parts))

def process(utils, source, replacements):
    source = utils.multiReplace(source, **replacements)

    ignore = False

    lines = []
    for line in source.splitlines():
        if line.startswith("#ENDIF"):
            ignore = False
        elif line.startswith("#IF"):
            ignore = not eval(line[3:])
        elif not ignore:
            lines.append(line)

    return "\n".join(lines)

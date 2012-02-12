import os

def FindPython():
    all_abi_flags = ['m', 'mu', '']

    python = "/usr"
    abi_flags = "m"  # Most common for linux distros
    version = "3.2"

    # Determine ABI flags used on this system
    include = os.path.join(python, "include")
    for cur_flags in all_abi_flags:
        inc = os.path.join(include, "python" + version + cur_flags, "Python.h")
        if os.path.exists(inc):
            abi_flags = cur_flags
            break

    # Determine whether python is in /usr/lib or /usr/lib64
    lib32 = os.path.join(python, "lib", "python" + version, "sysconfig.py")
    lib64 = os.path.join(python, "lib64", "python" + version, "sysconfig.py")
    if os.path.exists(lib32):
        libpath = "${BF_PYTHON}/lib"
    elif os.path.exists(lib64):
        libpath = "${BF_PYTHON}/lib64"
    else:
        # roll back to default value
        libpath = "${BF_PYTHON}/lib"

    return {'PYTHON': python,
            "VERSION": version,
            'LIBPATH': libpath,
            'ABI_FLAGS': abi_flags}

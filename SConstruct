import os

from SCons.Script import ARGUMENTS, Alias, AlwaysBuild, Default, Environment, Help

env = Environment(ENV=os.environ.copy())

MAKE = ARGUMENTS.get("MAKE", os.environ.get("MAKE", "make"))
MAKEFILE = ARGUMENTS.get("MAKEFILE", os.environ.get("MAKEFILE", "Makefile"))

FORWARDED_VARS = [
    "CC",
    "CXX",
    "LD",
    "AS",
    "HOST_CC",
    "XORRISO",
    "QEMU",
    "PYCOREOS_LIMINE_DIR",
    "BUILD_DIR",
    "ISO_ROOT",
    "TEST_TIMEOUT_SEC",
    "EXTRA_CFLAGS",
    "EXTRA_CXXFLAGS",
    "EXTRA_DOOM_CFLAGS",
    "EXTRA_DOOMPAL_CFLAGS",
]


def make_prefix():
    args = [MAKE, "-f", MAKEFILE]
    for key in FORWARDED_VARS:
        value = ARGUMENTS.get(key, os.environ.get(key))
        if value:
            args.append(f"{key}={value}")
    return " ".join(args)


def alias_make(name, target=None):
    mk_target = target or name
    node = Alias(name, [], f"{make_prefix()} {mk_target}")
    AlwaysBuild(node)
    return node


alias_make("help")
alias_make("show-config")
alias_make("build")
alias_make("kernel", "build")
iso_alias = alias_make("iso")
alias_make("run")
alias_make("test")
alias_make("beta")
alias_make("release")
alias_make("clean")
alias_make("install-deps")
alias_make("install-deps-scons")

Default(iso_alias)

Help(
    "PyCoreOS SCons frontend (delegates to Makefile targets)\n\n"
    "Usage:\n"
    "  scons build\n"
    "  scons iso\n"
    "  scons run\n"
    "  scons test\n"
    "  scons beta\n"
    "  scons clean\n\n"
    "SCons is optional; default builds run directly through Make with no Python dependency.\n\n"
    "Variable overrides (forwarded to make):\n"
    "  scons iso CC=gcc CXX=g++ LD=ld AS=as\n"
    "  scons iso PYCOREOS_LIMINE_DIR=/absolute/path/to/limine-assets\n"
)

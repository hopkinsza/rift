rift
====

The `rift` project implements an init system targeted at
modern linux and the BSDs.

It includes:

- simple `init`
- portable `halt`, `poweroff`, `reboot`
- standalone process supervisor, `fsv`
- `ttyd` daemon to supervise `getty` processes

build deps
----------

You will need a modern C compiler, `bmake`,
and the `rf-mk` include files (https://github.com/hopkinsza/rf-mk).

On linux, this should just require the
`build-essential` (or equivalent)
and `bmake` packages.

build/install
-------------

Substitute `doas` for your privilege-escalation flavor of choice (e.g. `sudo`)
if necessary.

```sh
# normal build
bmake
# parallel build
bmake -j4

# installation to the default /usr/local
doas bmake install
# installation to /opt
doas bmake install PREFIX=/opt
# installation to "the system" (split between / and /usr)
doas bmake install PREFIX=/

# clean
bmake clean
# extra squeaky-clean
bmake cleandir
```

internals
---------

If C code needs to test what platform it's being compiled for,
it should include `<sys/param.h>` to be able to test `#ifdef BSD`.
To check for linux or a specific BSD,
it should test the CPP predefined macros like `#ifdef __linux__`.

If the build system needs to test what platform it's compiling for,
for example to compile different `.c` files,
it will use the `TARGET_OS` variable,
which should be in the format `uname -s`.
This is normally auto-detected via the `bmake` variable `.MAKE.OS`;
you should never have to specify it manually unless you're cross-compiling.

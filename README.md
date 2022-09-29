rift
====

The `rift` project currently targets linux and the BSDs.

build
-----

If C code needs to test what platform it's being compiled for,
it should include `<sys/param.h>` to be able to test `#ifdef BSD`.
To check for linux or a specific BSD,
it should test the CPP predefined macros like `#ifdef __linux__`.

If the build system needs to test what platform it's compiling for,
for example to compile different `.c` files,
it will use the `TARGET_OS` variable.
It should be like `uname -s`.
This is normally auto-detected via the (b)`make` variable `.MAKE.OS`;
you should never have to specify it manually unless you're cross-compiling.

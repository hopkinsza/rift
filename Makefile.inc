#
# Support for __dead function attribute.
# This currently uses the GCC attribute syntax, but could use C11 _Noreturn,
# or C23 attribute [[noreturn]] in the future.
# Rift code will probably be updated to use [[noreturn]] once that is commonly
# available.
#

CPPFLAGS += "-D__dead=__attribute__((noreturn))"

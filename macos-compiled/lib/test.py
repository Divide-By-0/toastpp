from ctypes import cdll

# Load the library
lib0 = cdll.LoadLibrary('libfdot.dylib')
lib1 = cdll.LoadLibrary('libfe.dylib')
lib2 = cdll.LoadLibrary('liblbfgs.dylib')
lib3 = cdll.LoadLibrary('libmath.dylib')
lib4 = cdll.LoadLibrary('libstoast.dylib')
lib5 = cdll.LoadLibrary('libsuperlu.dylib')

# Now lib is a reference to the library, and you can call its functions
# For example, if the library has a function 'foo', you can call it like this:
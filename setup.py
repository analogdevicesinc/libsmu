from distutils.core import setup, Extension

# the c++ extension module
extension_mod = Extension("libsmu", ["test.cpp"], extra_compile_args=['-std=c++11'], language="c++")

setup(name = "libsmu", ext_modules=[extension_mod])

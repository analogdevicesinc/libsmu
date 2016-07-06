#!/usr/bin/env python

import glob
import os
import subprocess
import sys

from distutils.command.build_ext import build_ext
from setuptools import setup, find_packages, Extension
from Cython.Build import cythonize

# top level bindings directory
BINDINGS_DIR = os.path.dirname(os.path.abspath(__file__))
# top level repo directory
TOPDIR = os.path.dirname(os.path.dirname(BINDINGS_DIR))


class build_ext_compiler_check(build_ext):
    """Add custom compile flags for compiled extensions."""

    def build_extensions(self):
        compiler = self.compiler.compiler_type
        cxxflags = []
        if compiler != 'msvc':
            cxxflags.append('-std=c++11')
        for ext in self.extensions:
            ext.extra_compile_args.extend(cxxflags)
        return build_ext.build_extensions(self)


def pkgconfig(*packages, **kw):
    """Translate pkg-config data to compatible Extension parameters."""
    flag_map = {'-I': 'include_dirs', '-L': 'library_dirs', '-l': 'libraries'}

    try:
        tokens = subprocess.check_output(
            ['pkg-config', '--libs', '--cflags'] + list(packages)).split()
    except OSError as e:
        sys.stderr.write('running pkg-config failed: {}\n'.format(e.strerror))
        sys.exit(1)

    for token in tokens:
        if token[:2] in flag_map:
            kw.setdefault(flag_map.get(token[:2]), []).append(token[2:])
        else:
            kw.setdefault('extra_compile_args', []).append(token)
    return kw

ext_kwargs = {}

if sys.platform == 'win32':
    ext_kwargs['libraries'] = ['libsmu']
else:
    ext_kwargs['libraries'] = ['smu']
    ext_kwargs = pkgconfig('libusb-1.0', **ext_kwargs)

extensions = []
extensions.extend([
    Extension(
        'pysmu.libsmu',
        [os.path.join(BINDINGS_DIR, 'pysmu', 'libsmu.pyx')], **ext_kwargs),
    ])

setup(
    name='pysmu',
    version='0.88',
    description='python library for the m1k device',
    url='https://github.com/analogdevicesinc/libsmu',
    license='BSD',
    maintainer='Analog Devices, Inc.',
    packages=find_packages(),
    ext_modules=cythonize(extensions),
    scripts=glob.glob('bin/*'),
    cmdclass={'build_ext': build_ext_compiler_check},
    classifiers=[
        'Intended Audience :: Developers',
        'License :: OSI Approved :: BSD License',
        'Programming Language :: Python :: 2.7',
    ],
)

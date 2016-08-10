#!/usr/bin/env python

import copy
from io import open
import glob
import os
import re
import subprocess
import sys

from distutils.command.build_ext import build_ext as dst_build_ext
from distutils.command.sdist import sdist as dst_sdist
from setuptools import setup, find_packages, Extension

# top level bindings directory
BINDINGS_DIR = os.path.dirname(os.path.abspath(__file__))
# top level repo directory
TOPDIR = os.path.dirname(os.path.dirname(BINDINGS_DIR))


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
        token = token.decode()
        if token[:2] in flag_map:
            kw.setdefault(flag_map.get(token[:2]), []).append(token[2:])
        else:
            kw.setdefault('extra_compile_args', []).append(token)
    return kw


# configure various required compile flags
ext_kwargs = {'include_dirs': [os.path.join(TOPDIR, 'include')]}
if sys.platform == 'win32':
    ext_kwargs['libraries'] = ['libsmu']
else:
    ext_kwargs['libraries'] = ['smu']
    ext_kwargs = pkgconfig('libusb-1.0', **ext_kwargs)

# cython extensions to generate/build
extensions = []
extensions.extend([
    Extension(
        'pysmu.libsmu',
        [os.path.join(BINDINGS_DIR, 'pysmu', 'libsmu.pyx')], **ext_kwargs),
    ])


def no_cythonize(extensions, **_ignore):
    extensions = copy.deepcopy(extensions)
    for extension in extensions:
        sources = []
        for sfile in extension.sources:
            path, ext = os.path.splitext(sfile)
            if ext in ('.pyx', '.py'):
                sfile = path + '.cpp'
            sources.append(sfile)
        extension.sources[:] = sources
    return extensions


# only regenerate cython extensions if requested or required
USE_CYTHON = (
    os.environ.get('USE_CYTHON', False) or
    any(not os.path.exists(x) for ext in no_cythonize(extensions) for x in ext.sources))


class sdist(dst_sdist):
    """Make sure generated cython extensions are included."""

    def run(self):
        from Cython.Build import cythonize
        build_ext = self.reinitialize_command('build_ext')
        build_ext.ensure_finalized()
        cythonize(build_ext.extensions)
        dst_sdist.run(self)


class build_ext(dst_build_ext):
    """Add custom compile flags for compiled extensions."""

    def build_extensions(self):
        compiler = self.compiler.compiler_type
        cxxflags = []
        if compiler != 'msvc':
            cxxflags.append('-std=c++11')
        for ext in self.extensions:
            ext.extra_compile_args.extend(cxxflags)
        return dst_build_ext.build_extensions(self)

    def run(self):
        if USE_CYTHON:
            from Cython.Build import cythonize
            cythonize(self.extensions)
        self.extensions = no_cythonize(self.extensions)
        return dst_build_ext.run(self)


version = ''
with open('pysmu/__init__.py', 'r') as fd:
    reg = re.compile(r'__version__\s*=\s*[\'"]([^\'"]*)[\'"]')
    for line in fd:
        m = reg.match(line)
        if m:
            version = m.group(1)
            break

if not version:
    raise RuntimeError('Cannot find version information')

setup(
    name='pysmu',
    version=version,
    description='python library for the ADALM1000 device',
    url='https://github.com/analogdevicesinc/libsmu',
    license='BSD',
    author='Tim Harder',
    author_email='radhermit@gmail.com',
    packages=find_packages(),
    ext_modules=extensions,
    scripts=glob.glob('bin/*'),
    test_suite='tests',
    cmdclass={
        'build_ext': build_ext,
        'sdist': sdist,
    },
    classifiers=[
        'Intended Audience :: Developers',
        'License :: OSI Approved :: BSD License',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3.4',
        'Programming Language :: Python :: 3.5',
    ],
)

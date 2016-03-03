#!/usr/bin/env python

import os
import subprocess
import sys

from setuptools import setup, find_packages, Extension

# top level repo directory
BINDINGS_DIR = os.path.dirname(os.path.abspath(__file__))
# top level bindings directory
TOPDIR = os.path.dirname(BINDINGS_DIR)

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

ext_kwargs = dict(
    include_dirs=[TOPDIR],
    extra_compile_args=['-std=c++11'],
    library_dirs=[TOPDIR],
    libraries=['smu'],
)

extensions = []
extensions.extend([
    Extension(
        'pysmu._pysmu',
        [os.path.join(BINDINGS_DIR, 'src', 'pysmu.cpp')], **pkgconfig('libusb-1.0', **ext_kwargs)),
    ])

setup(
    name='pysmu',
    version='0.88',
    description='python library for the m1k device',
    url='https://github.com/analogdevicesinc/libsmu',
    license='BSD',
    packages=find_packages(),
    ext_modules=extensions,
    classifiers=[
        'Intended Audience :: Developers',
        'License :: OSI Approved :: BSD License',
        'Programming Language :: Python :: 2.7',
    ],
)

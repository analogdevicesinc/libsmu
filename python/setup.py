#!/usr/bin/env python

import os

from setuptools import setup, find_packages, Extension

# top level bindings directory
TOPDIR = os.path.dirname(os.path.abspath(__file__))

ext_kwargs = dict(
    include_dirs=[os.path.dirname(TOPDIR)],
    extra_compile_args = ['-std=c++11'],
)

extensions = []
extensions.extend([
    Extension(
        'pysmu._pysmu',
        [os.path.join(TOPDIR, 'src', 'pysmu.cpp')], **ext_kwargs),
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

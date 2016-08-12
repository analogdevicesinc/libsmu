#!/usr/bin/env python

import copy
from io import open
import glob
import os
import re
import shlex
import shutil
import subprocess
import sys

from distutils.command.build_ext import build_ext as dst_build_ext
from distutils.command.sdist import sdist as dst_sdist
from setuptools import setup, find_packages, Extension, Command

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

    @staticmethod
    def determine_ext_lang(ext_path):
        """Determine file extensions for generated cython extensions."""
        with open(ext_path) as f:
            for line in f:
                line = line.lstrip()
                if not line:
                    continue
                elif line[0] != '#':
                    return None
                line = line[1:].lstrip()
                if line[:10] == 'distutils:':
                    key, _, value = [s.strip() for s in line[10:].partition('=')]
                    if key == 'language':
                        return value
            else:
                return None

    @staticmethod
    def no_cythonize(extensions, **_ignore):
        """Determine file paths for generated cython extensions."""
        extensions = copy.deepcopy(extensions)
        for extension in extensions:
            sources = []
            for sfile in extension.sources:
                path, ext = os.path.splitext(sfile)
                if ext in ('.pyx', '.py'):
                    lang = build_ext.determine_ext_lang(sfile)
                    if lang == 'c++':
                        ext = '.cpp'
                    else:
                        ext = '.c'
                    sfile = path + ext
                sources.append(sfile)
            extension.sources[:] = sources
        return extensions

    def build_extensions(self):
        compiler = self.compiler.compiler_type
        cxxflags = []
        if compiler != 'msvc':
            cxxflags.append('-std=c++11')
        for ext in self.extensions:
            ext.extra_compile_args.extend(cxxflags)
        return dst_build_ext.build_extensions(self)

    def run(self):
        # only regenerate cython extensions if requested or required
        USE_CYTHON = (
            os.environ.get('USE_CYTHON', False) or
            any(not os.path.exists(x) for ext in self.no_cythonize(self.extensions) for x in ext.sources))
        if USE_CYTHON:
            from Cython.Build import cythonize
            cythonize(self.extensions)

        self.extensions = self.no_cythonize(self.extensions)
        return dst_build_ext.run(self)


class PyTest(Command):
    """Run tests using pytest against a built copy."""

    user_options = [
        ('pytest-args=', 'a', 'arguments to pass to py.test'),
        ('coverage', 'c', 'generate coverage info'),
        ('report=', 'r', 'generate and/or show a coverage report'),
        ('match=', 'k', 'run only tests that match the provided expressions'),
    ]

    default_test_dir = os.path.join(BINDINGS_DIR, 'tests')

    def initialize_options(self):
        self.pytest_args = ''
        self.coverage = False
        self.match = None
        self.report = None

    def finalize_options(self):
        self.test_args = ['-s', self.default_test_dir]
        self.coverage = bool(self.coverage)
        if self.match is not None:
            self.test_args.extend(['-k', self.match])

        if self.coverage:
            try:
                import pytest_cov
                self.test_args.extend(['--cov', 'pysmu'])
            except ImportError:
                sys.stderr.write('error: install pytest-cov for coverage support\n')
                sys.exit(1)

            if self.report is None:
                # disable coverage report output
                self.test_args.extend(['--cov-report='])
            else:
                self.test_args.extend(['--cov-report', self.report])

        # add custom pytest args
        self.test_args.extend(shlex.split(self.pytest_args))

    def run(self):
        try:
            import pytest
        except ImportError:
            sys.stderr.write('error: pytest is not installed\n')
            sys.exit(1)

        # build extensions and byte-compile python
        build_ext = self.reinitialize_command('build_ext')
        build_py = self.reinitialize_command('build_py')
        build_ext.ensure_finalized()
        build_py.ensure_finalized()
        self.run_command('build_ext')
        self.run_command('build_py')

        # Change the current working directory to the builddir during testing
        # so coverage paths are correct.
        builddir = os.path.abspath(build_py.build_lib)
        if self.coverage and os.path.exists(os.path.join(TOPDIR, '.coveragerc')):
            shutil.copyfile(os.path.join(TOPDIR, '.coveragerc'),
                            os.path.join(builddir, '.coveragerc'))
        ret = subprocess.call([sys.executable, '-m', 'pytest'] + self.test_args, cwd=builddir)
        sys.exit(ret)


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


test_requirements = ['pytest']
if sys.hexversion < 0x03030000:
        test_requirements.append('mock')

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
    tests_require=test_requirements,
    cmdclass={
        'build_ext': build_ext,
        'sdist': sdist,
        'test': PyTest,
    },
    classifiers=[
        'Intended Audience :: Developers',
        'License :: OSI Approved :: BSD License',
        'Programming Language :: Python :: 2.7',
        'Programming Language :: Python :: 3.4',
        'Programming Language :: Python :: 3.5',
    ],
)

# Released under the terms of the BSD License
# (C) 2016, Analog Devices, Inc.

__version__ = '1.0.4'

import os

#Import DLLs for Python versions >= 3.8
for path in os.environ.get("PATH", "").split(os.pathsep):
	if path and os.path.isabs(path):
		try:
			os.add_dll_directory(path)
		except (OSError, AttributeError):
			continue

from .libsmu import *
from .exceptions import *

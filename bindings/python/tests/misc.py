import sys
import tempfile

try:
    from urllib import urlretrieve
except ImportError:
    from urllib.request import urlretrieve

OLD_FW_URL = 'https://github.com/analogdevicesinc/m1k-fw/releases/download/v2.02/m1000.bin'
NEW_FW_URL = 'https://github.com/analogdevicesinc/m1k-fw/releases/download/v2.06/m1000.bin'

# input = raw_input in py3, copy this for py2
if sys.hexversion < 0x03000000:
    input = raw_input


def prompt(s):
    """Prompt the user to verify test setup before continuing."""
    input('ACTION: {} (hit Enter to continue)'.format(s))


# assumes an internet connection is available and github is up
# fetch old/new firmware files from github
OLD_FW = tempfile.NamedTemporaryFile().name
NEW_FW = tempfile.NamedTemporaryFile().name
urlretrieve(OLD_FW_URL, OLD_FW)
urlretrieve(NEW_FW_URL, NEW_FW)

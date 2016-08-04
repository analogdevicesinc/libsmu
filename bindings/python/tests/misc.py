import sys

# input = raw_input in py3, copy this for py2
if sys.hexversion < 0x03000000:
    input = raw_input


def prompt(s):
    """Prompt the user to verify test setup before continuing."""
    input('ACTION: {} (hit Enter to continue)'.format(s))

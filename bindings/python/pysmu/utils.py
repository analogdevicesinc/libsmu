from collections import Iterable


def iterify(x):
    """Return an iterable form of a given value."""
    if isinstance(x, Iterable):
        return x
    else:
        return (x,)

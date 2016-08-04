import os


class SessionError(Exception):
    """Generic session exception."""

    def __init__(self, msg=None, errcode=None):
        if msg is not None:
            self.msg = msg
        self.errcode = abs(errcode) if errcode is not None else None

    def __str__(self):
        if self.msg is not None:
            msg = self.msg
            if self.errcode is not None:
                msg += ': {}'.format(os.strerror(self.errcode))
            return msg
        return repr(self)

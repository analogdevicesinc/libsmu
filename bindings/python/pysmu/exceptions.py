import os


class LibsmuError(Exception):
    """Generic libsmu exception."""

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


class SessionError(LibsmuError):
    """Generic libsmu session error."""
    pass


class DeviceError(LibsmuError):
    """Generic libsmu device error."""
    pass


class DataflowError(LibsmuError):
    """Generic data flow error."""
    pass


class BufferOverflow(DataflowError):
    """The incoming read buffer has overflowed."""
    pass


class BufferTimeout(DataflowError):
    """An outgoing write buffer has timed out."""
    pass

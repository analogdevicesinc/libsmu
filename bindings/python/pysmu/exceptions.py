class SessionError(Exception):
    """Generic session exception."""

    def __init__(self, msg=None):
        if msg is not None:
            self.msg = msg

    def __str__(self):
        if self.msg is not None:
            return self.msg
        return repr(self)

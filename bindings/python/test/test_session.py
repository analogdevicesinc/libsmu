import time

try:
    from unittest import mock
except ImportError:
    import mock

from pytest import raises

from pysmu import Session


def test_empty():
    session = Session()
    assert not session.devices

def test_add_all():
    session = Session()
    session.add_all()
    assert session.devices

def test_hotplug():
    session = Session()

    attach = mock.Mock()
    detach = mock.Mock()
    session.hotplug_attach(attach)
    session.hotplug_detach(detach)

    start = time.time()
    while (True):
        time.sleep(1)
        end = time.time()
        elapsed = end - start
        if elapsed > 10 or (attach.called and detach.called):
            break

    assert attach.called
    assert detach.called

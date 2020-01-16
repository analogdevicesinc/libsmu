#!/usr/bin/env python
#
# Simple script showing how custom functions can be executed during hotplug
# events, use Ctrl-C to exit.

from __future__ import print_function

from signal import signal, SIG_DFL, SIGINT
import time

from pysmu import Session

session = Session()
last_devices = session.available_devices

while True:
    time.sleep(2)
    
    session.scan()
    available_devices = session.available_devices
    
    for other_device in last_devices:
        found = False
        
        for device in available_devices:
            if other_device.serial == device.serial:
                found = True
                break
               
        if not found:
            print("Device detached!")
            tmp = list(last_devices)
            tmp.remove(other_device)
            last_devices = tuple(tmp)
            
    for device in available_devices:
        found = False
        
        for other_device in last_devices:
            if other_device.serial == device.serial:
                found = True
                break
                
        if not found:
            print("Device attached")            
            
    last_devices = available_devices
    print("Number of available devices: " + str(len(last_devices)))

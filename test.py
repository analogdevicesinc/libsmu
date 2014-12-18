import libsmu
libsmu.setup()
libsmu.get_dev_info()
libsmu.set_mode(0,0,1)
print libsmu.set_output(0, 0, 0, 1.2, 100)

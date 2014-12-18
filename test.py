import pysmu
pysmu.setup()
pysmu.get_dev_info()
pysmu.set_mode(0,0,1)
print pysmu.set_output(0, 0, 1, 1.2, 10)

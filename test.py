import pysmu
pysmu.setup()
pysmu.get_dev_info()
pysmu.set_mode(0,0,1)
print pysmu.set_output_constant(0, 0, 1, 1.2)
print pysmu.get_inputs(0, 0, 10)
print pysmu.get_all_inputs(0, 10)

s = pysmu.iterate_inputs(0)
import itertools
print(list(itertools.islice(s, 10)))


import os
from common import *

# Test 0 - run all algorithms on fixed size problem, one process per node

node_counts = [1, 4, 9, 16]

for nc in node_counts:
    os.system(run_batch_string(nc, 1, "1gb", scripts_dir + "t0_executor.py"))
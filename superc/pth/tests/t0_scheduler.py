
import os
from common import *

# Test 0 - run all algorithms on fixed size problem, one process per node
TEST_ID = 0

node_counts = [1, 4, 9, 16]

log_dir, rdir = prepare_log_and_result_dirs_for_test(TEST_ID)

for nc in node_counts:
    os.system(run_batch_string(nc, scripts_dir + "t{}_executor.py".format(TEST_ID),
                               log_prefix=log_dir + "{}_nodes".format(nc),
                               results_dir=rdir))
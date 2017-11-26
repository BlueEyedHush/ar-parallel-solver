
import os
from common import *

# Test 1 - check communication impact by keeping workload constant, but manipulating N/ts ratio
TEST_ID = 1

node_counts = [1, 4, 9, 16]

log_dir, rdir = prepare_log_and_result_dirs_for_test(TEST_ID)

for nc in node_counts:
    os.system(run_batch_string(nc, scripts_dir + "t{}_executor.py".format(TEST_ID),
                               log_prefix=log_dir + "{}_nodes".format(nc),
                               time="00:25:00",
                               results_dir=rdir))
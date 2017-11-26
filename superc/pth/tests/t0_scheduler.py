
import os
from common import *

# Test 0 - run all algorithms on fixed size problem, one process per node

node_counts = [1, 4, 9, 16]

log_dir = logs_dir + "t0/"
ensure_dir_exists(log_dir)
for nc in node_counts:
    os.system(run_batch_string(nc,
                               1,
                               "1gb",
                               scripts_dir + "t0_executor.py",
                               queue="plgrid-short",
                               log_prefix=log_dir + "{}_nodes".format(nc),
                               time="00:20:00",
                               repetition_no=1))
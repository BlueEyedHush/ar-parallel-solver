#!/usr/bin/python

from common import *

n = 7200
ts = 625

rdir = results_dir + "/t0/"
ensure_dir_exists(results_dir)
r(import_modules_string())

result_path = results_dir + "{}_{}_{}".format(ts, n, get_node_id())
r(run_algo_string("seq", ts, n, result_path))
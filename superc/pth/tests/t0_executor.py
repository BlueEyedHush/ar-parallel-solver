#!/usr/bin/python

from common import *

n = 7200
ts = 25

result_path = get_results_dir() + "{}_{}_{}_{}".format(ts, n, get_node_id(), get_process_num())

algo_list = all_algorithms if get_process_num() == 1 else parallel_algos
full_cli = []
for algo_name in algo_list:
    for i in range(0, get_repetition_no()):
        full_cli.append(algo_cli(algo_name, ts, n, result_path))

cli = "; ".join(full_cli)
err(cli)
run_algo(cli)
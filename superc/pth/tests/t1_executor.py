#!/usr/bin/python

from common import *

# (n, ts)
workloads = [
    (7200, 25),
    (3600, 100),
    (1800, 400),
    (900, 1600),
]

cmd_list = []
for n, ts in workloads:
    cmds = build_cmd_sequence(parallel_algos, n, ts)
    cmd_list = cmd_list + cmds

run_commands(cmd_list)
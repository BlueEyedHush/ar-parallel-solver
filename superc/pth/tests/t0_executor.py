#!/usr/bin/python

from common import *

cmds = build_cmd_sequence(parallel_algos, n = 7200, ts = 25)
run_commands(cmds)
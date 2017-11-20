
import os
import sys

base_dir = "/net/people/plgblueeyedhush/ar-lab1/"
scripts_dir = base_dir + "superc/pth/tests/"
build_dir = base_dir + "cmake-build-release/"
results_dir = base_dir + "results/"

parallel_algos = map(lambda postfix: "parallel{}".format(postfix), ["", "_async", "_gap", "_lb", "_ts"])
all_algorithms = ["seq"] + parallel_algos

def run_batch_string(nodes, tasks_per_node, mem_per_task, script):
    cmd = ("sbatch"
    " -J ar-1"
    " -N " + str(nodes) +
    " --ntasks-per-node " + str(tasks_per_node) +
    " --mem-per-cpu " + mem_per_task +
    " --time=00:10:00"
    " -A ccbmc6"
    " -p plgrid-testing"
    " --output ar.so"
    " --error ar.se"
    " --mail-type=END,FAIL"
    " --mail-user=knawara112@gmail.com " + script)

    print cmd
    return cmd

def algo_cli(name, time_steps, grid_size, result_file = "", output=False):
    base = "{}/{} -t {} -n {} {}".format(build_dir, name, time_steps, grid_size, "-o" if output else "")
    cmd = (base + " >> " + result_file) if result_file else base
    return cmd

def import_modules_string():
    return (
        "module load tools/impi/2018;"
        "module load plgrid/tools/cmake/3.7.2;"
    )

def run_algo(cmd):
    cli = import_modules_string() + " " + cmd
    os.system(cli)

def get_node_num():
    return os,environ["SLURM_JOB_NUM_NODES"]

def get_node_id():
    return os.environ["SLURM_PROCID"]

def ensure_dir_exists(dir):
    os.system("mkdir -p " + dir)

def err(msg):
    sys.stderr.write(msg + "\n")
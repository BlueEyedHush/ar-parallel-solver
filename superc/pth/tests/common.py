
import os
import sys

base_dir = "/net/people/plgblueeyedhush/ar-lab1/"
scripts_dir = base_dir + "superc/pth/tests/"
build_dir = base_dir + "cmake-build-release/"
results_dir = base_dir + "results/"
logs_dir = base_dir + "logs/"

parallel_algos = map(lambda postfix: "parallel{}".format(postfix), ["", "_async", "_gap", "_lb", "_ts"])
all_algorithms = ["seq"] + parallel_algos

def run_batch_string(nodes,
                     tasks_per_node,
                     mem_per_task,
                     script,
                     queue="plgrid-testing",
                     log_prefix="ar",
                     time="00:10:00",
                     repetition_no=3,
                     results_dir="./"):
    process_count = nodes * tasks_per_node
    cmd = ("sbatch"
    " -J ar-1"
    " -N " + str(nodes) +
    " --ntasks-per-node " + str(tasks_per_node) +
    " --mem-per-cpu " + mem_per_task +
    " --time " + time +
    " -A ccbmc6"
    " -p " + queue +
    " --output " + log_prefix + ".so"
    " --error " + log_prefix + ".se"
    " --mail-type=END,FAIL"
    " --mail-user=knawara112@gmail.com " + script + " {} {} {}").format(process_count, repetition_no, results_dir)

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

def get_process_num():
    return int(sys.argv[1])

def get_repetition_no():
    return int(sys.argv[2])

def get_results_dir():
    return sys.argv[3]

def get_node_id():
    return os.environ["SLURM_PROCID"]

def ensure_dir_exists(dir):
    os.system("mkdir -p " + dir)

def err(msg):
    sys.stderr.write(msg + "\n")

def insert_separator_into_result_files(result_dir):
    separator = '\\n~~~~ `date "+%H:%M:%S %d-%m-%Y"` ~~~~\\n'
    os.system("for f in {}*; do echo -e >> \"{}\"; done".format(result_dir, separator))

import os
import sys

base_dir = "/net/people/plgblueeyedhush/ar-lab1/"
scripts_dir = base_dir + "superc/pth/tests/"
build_dir = base_dir + "cmake-build-release/"
results_dir = base_dir + "results/"
logs_dir = base_dir + "logs/"
mpiexec_prefix = "mpiexec " #"mpiexec -ordered-output -prepend-rank "


parallel_algos = map(lambda postfix: "parallel{}".format(postfix), ["", "_async", "_gap", "_lb", "_ts"])

# -------------------
# Environment agnostic
# -------------------

def ensure_dir_exists(dir):
    os.system("mkdir -p " + dir)

def err(msg):
    sys.stderr.write(msg + "\n")

# -------------------
# Meant for scheduler
# -------------------

def run_batch_string(nodes,
                     script,
                     tasks_per_node = 1,
                     mem_per_task = "1gb",
                     queue="plgrid-short",
                     log_prefix="ar",
                     time="00:20:00",
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

def insert_separator_into_result_files(result_dir):
    separator = '\\n~~~~ `date "+%H:%M:%S %d-%m-%Y"` ~~~~\\n'
    os.system("for f in {}*; do echo -e >> \"{}\"; done".format(result_dir, separator))

def prepare_log_and_result_dirs_for_test(test_id):
    log_dir = logs_dir + "t{}/".format(test_id)
    ensure_dir_exists(log_dir)

    rdir = results_dir + "/t{}/".format(test_id)
    ensure_dir_exists(rdir)
    insert_separator_into_result_files(rdir)

    return log_dir, rdir

# -------------------
# Meant for executor
# -------------------

def import_modules_string():
    return (
        "module load tools/impi/2018;"
        "module load plgrid/tools/cmake/3.7.2;"
    )

def algo_cli(name, time_steps, grid_size, result_file = "", output=False):
    if name == 'seq':
        prefix = ""
    else:
        prefix = mpiexec_prefix

    base = prefix + "{}/{} -t {} -n {} {}".format(build_dir, name, time_steps, grid_size, "-o" if output else "")
    cmd = (base + " >> " + result_file) if result_file else base
    return cmd

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

# makes decision whether we should include seq in the list
def get_algorithm_list(parallels):
    return  ["seq"] + parallels if get_process_num() == 1 else parallels

def get_result_path(ts, n):
    return get_results_dir() + "{}_{}_{}_{}".format(ts, n, get_node_id(), get_process_num())

def run_algos(parallels, n, ts):
    result_path = get_result_path(ts, n)

    full_cli = []
    for algo_name in get_algorithm_list(parallels):
        for i in range(0, get_repetition_no()):
            full_cli.append(algo_cli(algo_name, ts, n, result_path))

    cli = "; ".join(full_cli)
    err(cli)
    run_algo(cli)


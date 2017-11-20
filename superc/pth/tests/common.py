
base_dir = "/net/people/plgblueeyedhush/ar-lab1/"
scripts_dir = base_dir + "superc/pth/tests/"
build_dir = base_dir + "/cmake-build/release/"

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

def run_algo_string(name, time_steps, grid_size, output=False):
    return "{}/{} -ts {} -n {} {}".format(build_dir, name, time_steps, grid_size, "-o" if output else "")
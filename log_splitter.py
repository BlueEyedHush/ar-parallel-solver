
import sys
import re

READ_STDIN = True

def splitter(sentence, mapping):
    split = re.split("\[[0-9]+?\]", sentence)


    if len(split) > 1:
        groups = []
        for m in re.finditer("\[([0-9]+?)\]", sentence):
            groups.append(m.group(1))

        if split[0]:
            # if first is non-empty, line didn't start with node id indicator
            mapping["_"].append(split[0])
            mapping["_"].append(" (???) \n")

        for node_id, str in zip(groups, split[1:]):
            if node_id not in mapping:
                mapping[node_id] = []

            mapping[node_id].append(str)



    else:
        mapping["_"].append(sentence)

    return mapping

if READ_STDIN:
    lines = sys.stdin.readlines()
else:
    f = open("ar.se")
    lines = f.readlines()
    f.close()

node_to_log_mapping = {"_": []}
for line in lines:
    splitter(line, node_to_log_mapping)

for k in sorted(node_to_log_mapping.iterkeys()):
    sys.stdout.write("~~~~ {} ~~~~\n".format(k))
    sys.stdout.write("".join(node_to_log_mapping[k]))
    sys.stdout.write("\n\n")
import numpy as np
import pandas as pd
import StringIO

NODE_COUNT = 1

def pread(filename):
    return pd.read_csv("cmake-build-release/results-parts/" + filename, 
                delim_whitespace=True, header=None)

concated = []
for i in range(0, 100):
    stime = []
    for n in range(0,NODE_COUNT):
        stime.append(pread("{}_t_{}".format(n, i)))
        
    c = pd.concat(stime)
    concated.append(c)

for i, c in zip(range(0, len(concated)), concated):
    dses = []

    dx = concated[0].sort_values([0,1,2,3])
    for key in dx[0].unique():
        dw = dx[dx[0] == key]
        dses.append(dw)
        
    strings = []
    for d in dses:
        s = StringIO.StringIO()
        d.to_csv(s, header=None, sep=' ', index=False)
        strings.append(s.getvalue())

    c = "\n".join(strings)
    f = open('cmake-build-release/results/t_{}'.format(i), 'w')
    f.write(c)
    f.close()
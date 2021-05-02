import argparse
import subprocess
from typing import List
import numpy as np
from geneticalgorithm import geneticalgorithm as ga

parser = argparse.ArgumentParser()
parser.add_argument('bitcode', help='Specify LLVM bitcode')
parser.add_argument('miso', help='Specify MISO file')
parser.add_argument('bcconf', nargs='?', help='Specify conf file for bitcode')

ga_param = {
    'max_num_iteration': 500,
    'population_size': 50,
    'mutation_probability': 0.1,
    'elit_ratio': 0.01,
    'crossover_probability': 0.5,
    'parents_portion': 0.3,
    'crossover_type': 'uniform',
    'max_iteration_without_improv': None,
}
miso_path = '/tmp/miso.txt'


def read_miso(path: str) -> List[str]:
    with open(path) as f:
        lines = f.readlines()
    lines = map(lambda x: x.strip(), lines)
    return [i for i in lines if i]


def run_isel(x: np.array) -> float:
    inputs = []
    instr_num = 0

    with open(miso_path, 'w') as f:
        for i, v in enumerate(x):
            if v == 1.:
                f.write(misos[i])
                f.write('\n')
                instr_num += 1
            inputs.append(str(int(v)))
    print(''.join(inputs), end=' ', flush=True)

    cmd = ['./main', 'isel', args.bitcode, miso_path]
    if args.bcconf:
        cmd.append(args.bcconf)
    p = subprocess.Popen(cmd, stdout=subprocess.PIPE, encoding='utf-8')
    if p.wait():
        raise RuntimeError(p.returncode)

    result = p.stdout.read().strip()
    assert result.startswith('STA: ')
    sta = float(result[len('STA: '):])
    sta += sta * instr_num / 100.

    print(sta)
    return sta


if __name__ == '__main__':
    args = parser.parse_args()
    misos = read_miso(args.miso)

    model = ga(function=run_isel,
               dimension=len(misos),
               variable_type='bool',
               algorithm_parameters=ga_param)
    model.run()

import argparse
from subprocess import Popen, PIPE
from typing import List, Tuple, Any
import numpy as np
import random
from geneticalgorithm import geneticalgorithm as ga

np.random.seed(0)
random.seed(0)

parser = argparse.ArgumentParser()
parser.add_argument('bitcode', help='Specify LLVM bitcode')
parser.add_argument('miso', help='Specify MISO file')
parser.add_argument('bcconf', nargs='?', help='Specify conf file for bitcode')

GA_PARAMS = {
    'max_num_iteration': 150,
    'population_size': 100,
    'mutation_probability': 0.1,
    'elit_ratio': 0.01,
    'crossover_probability': 0.5,
    'parents_portion': 0.3,
    'crossover_type': 'uniform',
    'max_iteration_without_improv': None,
}
MAIN_PATH = './main'
MISO_PATH = '/tmp/miso.txt'


def read_miso(path: str) -> List[str]:
    with open(path) as f:
        lines = f.readlines()
    lines = map(lambda x: x.strip(), lines)
    return [i for i in lines if i]


def get_area(miso_path: str) -> int:
    cmd = [MAIN_PATH, 'area', miso_path]
    p = Popen(cmd, stdout=PIPE, encoding='utf-8')
    if p.wait():
        raise RuntimeError(p.returncode)
    result = p.stdout.read().strip()
    assert result.startswith('Area: ')
    return int(result[len('Area: '):])


def get_sta(miso_path: str) -> int:
    cmd = [MAIN_PATH, 'isel', args.bitcode, miso_path]
    if args.bcconf:
        cmd.append(args.bcconf)
    p = Popen(cmd, stdout=PIPE, encoding='utf-8')
    if p.wait():
        raise RuntimeError(p.returncode)
    result = p.stdout.read().strip()
    assert result.startswith('STA: ')
    return int(result[len('STA: '):])


def run_isel(x: np.array) -> Tuple[float, float]:
    inputs = (1 << x.shape[0]) - 1
    instr_num = 0

    # write miso
    with open(MISO_PATH, 'w') as f:
        for i, v in enumerate(x):
            if v == 1.:
                f.write(misos[i])
                f.write('\n')
                instr_num += 1
            else:
                inputs = inputs - (1 << i)

    input_str = ('%x' % inputs).rjust((x.shape[0] + 7) // 8, '0')
    print('\r' + input_str, end='\t', flush=True)

    # compute area and STA
    area = get_area(MISO_PATH)
    sta = get_sta(MISO_PATH)
    return area, sta


def do_gene_and_rand(x: np.array) -> float:
    # do random
    rand_x = (np.random.rand(*x.shape) > .5).astype(float)
    area, sta = run_isel(rand_x)
    area_ratio = area / AREA_ALL
    sta_ratio = sta / STA_BASE
    loss = area_ratio + sta_ratio
    db_rand.add_loss(loss)

    # do GA
    area, sta = run_isel(x)
    area_ratio = area / AREA_ALL
    sta_ratio = sta / STA_BASE
    loss = area_ratio + sta_ratio
    db_gene.add_point(-area_ratio, -sta_ratio)
    db_gene.add_loss(loss)
    print('%.3f %.3f %.2f' % (area_ratio, sta_ratio, loss))

    return loss


class DB():

    def __init__(self):
        self._pts = []
        self._iter = 0
        self._min_loss = None
        self._loss = []

    def add_point(self, x: Any, y: Any):
        '''Save skyline'''
        new_pts = []
        for x1, y1 in self._pts:
            if x <= x1 and y <= y1:
                return
            if x < x1 or y < y1:
                new_pts.append((x1, y1))
        new_pts.append((x, y))
        self._pts = new_pts

    def add_loss(self, loss: Any):
        self._iter += 1
        if self._min_loss is None or loss < self._min_loss:
            self._min_loss = loss
            self._loss.append((self._iter, loss))

    def print_db(self):
        print('[')
        for x, y in reversed(sorted(self._pts)):
            print('  ' + repr((-x, -y)) + ',')
        print(']')

        print('[')
        for i, loss in self._loss:
            print('  ' + repr((i, loss)) + ',')
        print(']')


if __name__ == '__main__':
    args = parser.parse_args()
    misos = read_miso(args.miso)

    AREA_ALL = get_area(args.miso)
    STA_BASE = get_sta('/dev/null')
    db_gene = DB()
    db_rand = DB()

    model = ga(function=do_gene_and_rand,
               dimension=len(misos),
               variable_type='bool',
               algorithm_parameters=GA_PARAMS)
    try:
        model.run()
    except KeyboardInterrupt:
        pass

    print('\n')
    db_gene.print_db()
    print('\n')
    db_rand.print_db()

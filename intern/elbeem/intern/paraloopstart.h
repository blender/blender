
#pragma omp parallel section num_threads(MAX_THREADS) \
reduction(+: calcCurrentMass, calcCurrentVolume, calcCellsFilled, calcCellsEmptied, calcNumUsedCells)

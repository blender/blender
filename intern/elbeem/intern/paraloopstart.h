
#pragma omp parallel num_threads(MAX_THREADS) \
reduction(+: calcCurrentMass, calcCurrentVolume, calcCellsFilled, calcCellsEmptied, calcNumUsedCells) 

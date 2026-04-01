# SkolemFC: An Approximate Skolem Function Counter

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
<!-- ![build](https://github.com/meelgroup/SkolemFC/workflows/build/badge.svg)
[![Docker Hub](https://img.shields.io/badge/docker-latest-blue.svg)](https://hub.docker.com/r/msoos/SkolemFC/) -->

SkolemFC takes in a F(X,Y) formula as input and returns the number of Boolean functions G(X) such that ∃Y F(X, Y) = F(X, G(X)). SkolemFC *counts the number of functions without even generating a single function.*

To learn more about SkolemFC, please have a look at our [AAAI-24 paper](https://arxiv.org/abs/2312.12026).


## How to Build a Binary

### Dependencies

**Linux:**
```
sudo apt install build-essential cmake git libubsan1 libasan8 \
    zlib1g-dev libmlpack-dev libensmallen-dev libmpfr-dev \
    libboost-program-options-dev libboost-serialization-dev libgmp3-dev
```

**macOS:**
```
brew install cmake gmp boost mpfr mlpack
```

### Build

Clone the repository (including submodules) and configure:

```
git clone --recurse-submodules https://github.com/meelgroup/skolemfc/
cd skolemfc
./configure.sh
cd build && make -j10
```

### configure.sh options

Run `./configure.sh --help` for the full usage message.


## How to Use the Binary
First, you must translate your problem to QDIMACS and just pass your file as input to SkolemFC, it will print the number of funcitons satisfying of the given QDIMACS formula.

### Running SkolemFC


```
$ ./skolemfc myfile.qdimacs

c [sklfc] SkolemFC Version: 36cf66a9ae
c [sklfc] executed with command line: ./skolemfc myfile.qdimacs
c [sklfc] using epsilon: 0.8 delta: 0.4 seed: 0
...
s fc 2 ** 4.00
...
c [sklfc] finished T: 0.25
c [sklfc] iterations: 729

```
SkolemFC reports that we have approximately `16 (=2 ** 4)` functions satisfying the QDIMACS specification.

### Guarantees
SkolemFC provides so-called "PAC", or Probably Approximately Correct, guarantees. In less fancy words, the system guarantees that the solution found is within a certain tolerance (called "epsilon") with a certain probability (called "delta"). The default tolerance and probability, i.e. epsilon and delta values, are set to 0.8 and 0.4, respectively. Both values are configurable.


### Issues, questions, bugs, etc.
Please click on "issues" at the top and [create a new issue](https://github.com/meelgroup/skolemfc/issues/new). All issues are responded to promptly.

## How to Cite

This work is by Arijit Shaw, Brendan Juba, and Kuldeep S. Meel, as [published in AAAI-24](https://arxiv.org/abs/2312.12026).

The benchmarks used in our evaluation can be found [here](https://zenodo.org/records/10689839).

## Exact Counter
An exact counter (termed "Baseline" in the paper) for Skolem Functions is available in the folder [`utils/baseline`](https://github.com/meelgroup/skolemfc/tree/main/utils/baseline). Please follow instructions in the README inside that folder for installing tools for that.
<!-- The old version, is available under the branch "paper". Please read the README of the old release to know how to compile the code. Old releases should easily compile. -->

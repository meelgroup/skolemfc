/******************************************
 SkolemFC

 Copyright (C) 2024, Arijit Shaw, Brendan Juba, and Kuldeep S. Meel.

 All rights reserved.

 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
***********************************************/

#pragma once

#include <gmp.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <vector>

#include "skolemfc.h"

using CMSat::Lit;
using std::cout;
using std::endl;
using std::vector;

#define release_assert(a)                                    \
  do                                                         \
  {                                                          \
    if (!(a))                                                \
    {                                                        \
      fprintf(stderr,                                        \
              "*** ASSERTION FAILURE in %s() [%s:%d]: %s\n", \
              __FUNCTION__,                                  \
              __FILE__,                                      \
              __LINE__,                                      \
              #a);                                           \
      abort();                                               \
    }                                                        \
  } while (0)

namespace SkolemFCInt {

typedef uint16_t WeightPrec;

#define print_verb(n, X) \
  if (verbosity >= n) cout << "c [sklfc] " << X << endl

typedef unsigned char value;

struct SklFCInt
{
  SklFCInt(const double _epsilon,
           const double _delta,
           const uint32_t seed,
           const uint32_t verbosity = 1);
  ~SklFCInt();

  uint32_t new_vars(uint32_t n)
  {
    release_assert(nvars == 0);

    nvars = n;

    return nvars;
  }

  bool add_clause(const vector<Lit>& cl);

  bool add_exists_var(uint32_t e_var);
  bool add_forall_var(uint32_t a_var);

  void check_ready() const;

  uint32_t nVars() const { return nvars; }
  uint32_t nGVars() const { return n_g_vars; }

  void set_n_cls(uint32_t n_cls);
  const char* get_version_info() const;
  const char* get_compilation_env() const;
  void create_g_formula();
  void print_formula(const vector<vector<Lit>>& formula);

  uint32_t nvars = 0;
  uint32_t n_g_vars = 0;
  uint32_t n_cls_declared = 0;
  uint32_t seed = 1;
  double epsilon;
  double delta;
  double thresh = 0;
  double sum_logcount = 0;
  mpq_t sampl_prob;
  uint32_t sampl_prob_expbit = 0;
  uint32_t sampl_prob_expbit_before_approx =
      std::numeric_limits<uint32_t>::max();
  uint32_t sampl_prob_expbit_before_magic =
      std::numeric_limits<uint32_t>::max();
  mpz_t prod_precision;
  uint64_t num_cl_added = 0;
  uint32_t verbosity;
  vector<vector<Lit>> clauses;
  vector<vector<Lit>> g_formula_clauses;
  vector<uint32_t> exists_vars;
  vector<uint32_t> forall_vars;
  std::vector<Lit> new_clause, diff_clause;
  uint64_t logcount = 0;
};

}  // namespace SkolemFCInt

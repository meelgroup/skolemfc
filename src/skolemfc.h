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

#ifndef SKOLEMFC_H__
#define SKOLEMFC_H__

#include <cstdint>
#include <string>
#include <tuple>
#include <utility>
#include <vector>
#ifdef CMS_LOCAL_BUILD
#include "cryptominisat.h"
#else
#include <cryptominisat5/cryptominisat.h>
#endif

#include <approxmc/approxmc.h>
#include <gmpxx.h>

#include <mutex>
#include <thread>

using CMSat::Lit;
using std::string;
using std::vector;

namespace SkolemFC {

struct SklFCPrivate;

struct SklFC
{
 public:
  SklFC(const double epsilon = 0.8,
        const double delta = 0.8,
        const uint32_t seed = 1,
        const uint32_t verbosity = 0);
  ~SklFC();
  static const char* get_version_info();
  static const char* get_compilation_env();

  // Adding CNF
  uint32_t nVars();
  void new_var();
  void new_vars(uint32_t num);
  bool add_clause(const std::vector<CMSat::Lit>& lits);
  bool add_forall_var(uint32_t var);
  bool add_exists_var(uint32_t var);

  void check_ready();
  void set_num_threads(int nthreads) { numthreads = nthreads; }
  void set_constants();
  string print_cnf(uint64_t num_vars,
                   vector<vector<Lit>> clauses,
                   vector<uint> projection_vars);
  mpz_class get_est0();
  mpz_class get_g_count();
  mpz_class get_g_count_approxmc();
  mpz_class get_g_count_ganak();
  void get_samples(uint64_t samples_needed = 0, int seed = 1);
  void get_samples_multithread(uint64_t samples_needed = 0);
  void get_and_add_count_for_a_sample();
  void get_and_add_count_multithred();
  void get_and_add_count_onethred(vector<vector<int>> samples);
  mpf_class get_est1(mpz_class s1size);
  bool check_if_approxmc_error_exceeds(mpf_class count,
                                       mpz_class s2size,
                                       double maxerror);
  mpf_class get_current_estimate();
  double get_progress();
  void get_sample_num_est();
  vector<vector<Lit>> create_formula_from_sample(vector<vector<int>> samples,
                                                 int sample_num);
  ApproxMC::SolCount count_using_approxmc(
      uint64_t, vector<vector<Lit>>, vector<uint>, double, double);
  mpz_class absolute_count_from_appmc(ApproxMC::SolCount);
  mpz_class count_using_ganak(uint64_t,
                              vector<vector<Lit>>,
                              vector<uint>,
                              uint32_t);
  ApproxMC::SolCount log_count_from_absolute(mpz_class);

  void count();

  bool show_count();
  uint64_t get_iteration() { return iteration; }

  // Set config
  void set_parameters();
  void set_dklr_parameters(double, double, double);
  void set_oracles(bool, bool, bool);
  void set_g_counter_parameters(double, double);
  void set_ignore_unsat(bool _ignore_unsat);
  void set_static_samp(bool _static_samp);
  void set_noguarntee_mode(bool _noguarnatee);
  static void handle_alarm(int sig)
  {
    std::cout << "c Ganak Timeout occurred! Singal:" << sig << std::endl;
  }

 private:
  SklFCPrivate* skolemfc = NULL;
  std::vector<std::thread> threads;
  std::mutex cout_mutex, vec_mutex, iter_mutex;
  uint64_t iteration = 0;
  mpf_class log_skolemcount = 0;
  mpf_class thresh = 1;
  mpz_class s2size;
  uint numthreads;
  bool use_unisamp = false;
  bool exactcount_s0 = true;
  bool exactcount_s2 = false;
  bool ignore_unsat = true;
  bool static_samp = false;
  bool noguarnatee = false;
  double epsilon_gc = 0.2, delta_gc = 0.4;
  double epsilon = 0, delta = 0;
  double start_time_skolemfc, start_time_this;
  double epsilon_f, delta_f;
  double epsilon_s, delta_c, epsilon_c;
  double max_error_logcounter;
  uint64_t sample_num_est;
  uint64_t next_iter_to_show_output = 1;
  bool okay = true;
  uint32_t seed = 1;
  uint verb = 0;
  uint verb_oracle = 0;
  uint approxmc_threshold = 35;
  void unigen_callback(const std::vector<int>& solution, void*);
  vector<vector<int>> samples_from_unisamp;
  uint32_t sample_clearance_iteration = 0;
  bool ganak_timeout;
};

}  // namespace SkolemFC

#endif

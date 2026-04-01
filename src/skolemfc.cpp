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

#include "skolemfc.h"

#include <arjun/arjun.h>
#include <gmpxx.h>
#include <sys/wait.h>
#include <threads.h>
#include <unigen/unigen.h>

#include <cmath>
#include <iomanip>
#include <sstream>

#include "GitSHA1.h"
#include "skolemfc-int.h"
#include "time_mem.h"

using namespace SkolemFCInt;
using CMSat::lbool;
using CMSat::Lit;
using std::string;
using std::stringstream;
using std::thread;

struct SkolemFC::SklFCPrivate
{
  SklFCPrivate(SkolemFCInt::SklFCInt* _p) : p(_p) {}
  ~SklFCPrivate() { delete p; }
  SkolemFCInt::SklFCInt* p = NULL;
};

SkolemFC::SklFC::SklFC(const double epsilon_i,
                       const double delta_i,
                       const uint32_t seed_i,
                       const uint32_t verbosity)
{
  skolemfc = new SklFCPrivate(
      new SkolemFCInt::SklFCInt(epsilon_i, delta_i, seed_i, verbosity));
}
SkolemFC::SklFC::~SklFC() { delete skolemfc; }

uint32_t SkolemFC::SklFC::nVars() { return skolemfc->p->nvars; }
void SkolemFC::SklFC::new_vars(uint32_t num) { skolemfc->p->nvars += num; }

bool SkolemFC::SklFC::add_clause(const std::vector<Lit>& cl)
{
  return skolemfc->p->add_clause(cl);
}

bool SkolemFC::SklFC::add_exists_var(uint32_t var)
{
  return skolemfc->p->add_exists_var(var);
}

bool SkolemFC::SklFC::add_forall_var(uint32_t var)
{
  return skolemfc->p->add_forall_var(var);
}

void SkolemFC::SklFC::check_ready() { skolemfc->p->check_ready(); }

void SkolemFC::SklFC::set_constants()
{
  start_time_skolemfc = cpuTime();

  epsilon = skolemfc->p->epsilon;
  delta = skolemfc->p->delta;

  cout << "c\nc ---- [ estimating ] "
          "--------------------------------------------------------\nc\n";

  cout << "c [sklfc] running with epsilon: " << epsilon
       << " and delta: " << delta << endl;

  epsilon_f = 0.6 * epsilon;
  delta_f = 0.5 * delta;

  thresh = 4.0 * log(2 / delta_f) * (1 + epsilon_f) / (epsilon_f * epsilon_f);
  thresh *= (double)skolemfc->p->exists_vars.size();

  cout << "c [sklfc] threshold (x |Y|) is set to: " << thresh << endl;
}

bool SkolemFC::SklFC::show_count()
{
  if (skolemfc->p->verbosity < 1)
    return false;
  else if (iteration == next_iter_to_show_output)
  {
    if (iteration < 10)
      next_iter_to_show_output += 1;
    else
      next_iter_to_show_output += 10;
    return true;
  }
  return false;
}

mpf_class SkolemFC::SklFC::get_current_estimate()
{
  return (log_skolemcount / (double)iteration) * (mpf_class)s2size;
}

double SkolemFC::SklFC::get_progress()
{
  mpf_class x = 100 * log_skolemcount / thresh;
  return x.get_d();
}

// void SkolemFC::SklFC::get_est0_gpmc()
// {
//   std::stringstream ss;
//
//   ss << "p cnf " << nVars() << " " << skolemfc->p->clauses.size() << endl;
//
//   ss << "c p show";
//   for (uint var : skolemfc->p->forall_vars)
//   {
//     ss << " " << var + 1;
//   }
//   ss << " 0" << endl;
//
//   for (const auto& clause : skolemfc->p->clauses)
//   {
//     for (const Lit& lit : clause)
//     {
//       ss << lit << " ";
//     }
//     ss << "0" << endl;
//   }
//
//   string cnfContent = ss.str();
//
//   int toGPMC[2];    // Parent to child
//   int fromGPMC[2];  // Child to parent
//   pid_t pid;
//   mpz_t result;
//
//   if (pipe(toGPMC) == -1 || pipe(fromGPMC) == -1)
//   {
//     perror("pipe");
//     exit(EXIT_FAILURE);
//   }
//
//   pid = fork();
//   if (pid == -1)
//   {
//     perror("fork");
//     exit(EXIT_FAILURE);
//   }
//   cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
//        << (cpuTime() - start_time_skolemfc)
//        << "] counting for F formula using GPMC" << endl;
//
//   if (pid == 0)
//   {                                 // Child process
//     dup2(toGPMC[0], STDIN_FILENO);  // Connect the read-end of toGPMC to
//     stdin dup2(fromGPMC[1],
//          STDOUT_FILENO);  // Connect the write-end of fromGPMC to stdout
//     close(toGPMC[0]);     // These are being used by the child now
//     close(toGPMC[1]);
//     close(fromGPMC[0]);
//     close(fromGPMC[1]);
//
//     // Execute gpmc
//     execlp("./ganak", "./ganak", (char*)NULL);
//     perror("execlp");  // execlp only returns on error
//     exit(EXIT_FAILURE);
//   }
//   else
//   {                      // Parent process
//     close(toGPMC[0]);    // Parent does not read from toGPMC
//     close(fromGPMC[1]);  // Parent does not write to fromGPMC
//
//     // Write CNF to gpmc
//     write(toGPMC[1], cnfContent.c_str(), cnfContent.size());
//     close(toGPMC[1]);  // Send EOF to child
//
//     // Read output from gpmc
//     char buffer[128];
//     string gpmcOutput;
//     ssize_t count;
//     while ((count = read(fromGPMC[0], buffer, sizeof(buffer) - 1)) > 0)
//     {
//       buffer[count] = '\0';
//       gpmcOutput += buffer;
//     }
//     close(fromGPMC[0]);
//
//     // Wait for child process to finish
//     waitpid(pid, NULL, 0);
//
//     mpz_init(result);
//     // Parse gpmcOutput to find the required number and store it in result
//     std::istringstream iss(gpmcOutput);
//     string line;
//     mpz_class result;  // Use mpz_class instead of mpz_t
//     bool ganak_count_line_found = false;
//     while (getline(iss, line))
//     {
//       cout << line << endl;
//       if (line.rfind("c s exact arb int ", 0) == 0)
//       {  // Line starts with the pattern
//         string numberStr = line.substr(strlen("c s exact arb int "));
//         result.set_str(
//             numberStr,
//             10);  // Set the value of result, 10 is the base for decimal
//             numbers
//         break;
//         ganak_count_line_found = true;
//       }
//     }
//     if (!ganak_count_line_found) {
//       okay =  false;
//       cout << "c [sklfc] ERROR: No Count line found from Ganak" << endl;
//       exit(0);
//     }
//     if (result == 0)
//     {
//       cout << "c [sklfc] F is UNSAT. Est1 = 0" << endl;
//       okay = false;
//     }
//     cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
//          << (cpuTime() - start_time_skolemfc)
//          << "]  F formula has exact (projected) count: " << result << endl;
//     mpz_pow_ui(
//       value_est0.get_mpz_t(), mpz_class(2).get_mpz_t(),
//       skolemfc->p->forall_vars.size());
//     value_est0 -= result;
//     value_est0 *= skolemfc->p->exists_vars.size();  // TODO missing 2^n
//
//     value_est0 *= result;
//   }
//   cout << "c [sklfc] Est0 = " << value_est0 << endl;
// }

mpz_class SkolemFC::SklFC::get_est0()
{
  mpz_class est0;

  mpz_pow_ui(est0.get_mpz_t(),
             mpz_class(2).get_mpz_t(),
             skolemfc->p->forall_vars.size());

  if (ignore_unsat)
    return 0;

  else if (exactcount_s0)
  {
    cout << "c [sklfc] Employing Ganak to count F formula" << endl;
    est0 -= count_using_ganak(skolemfc->p->nVars(),
                              skolemfc->p->clauses,
                              skolemfc->p->forall_vars,
                              1);
  }
  else
  {
    cout << "c [sklfc] Employing ApproxMC to count F formula" << endl;
    ApproxMC::SolCount c;
    c = count_using_approxmc(skolemfc->p->nVars(),
                             skolemfc->p->clauses,
                             skolemfc->p->forall_vars,
                             epsilon_gc,
                             delta_gc);
    est0 -= absolute_count_from_appmc(c);
  }
  cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
       << (cpuTime() - start_time_skolemfc) << "]  Size of set S0: " << est0
       << endl;

  est0 *= skolemfc->p->exists_vars.size();

  cout << "c [sklfc] Value for Est0: " << est0 << endl;

  cout << "c Pass Est0: " << std::setprecision(2) << std::fixed
       << (cpuTime() - start_time_skolemfc) << endl;

  return est0;
}

mpz_class SkolemFC::SklFC::get_g_count()
{
  mpz_class s1size;
  if (exactcount_s2)
  {
    cout << "c [sklfc] Employing Ganak to count G formula" << endl;
    s1size = count_using_ganak(skolemfc->p->nGVars(),
                               skolemfc->p->g_formula_clauses,
                               skolemfc->p->forall_vars,
                               1);
  }
  else
  {
    cout << "c [sklfc] Employing ApproxMC to count G formula" << endl;
    ApproxMC::SolCount c;
    c = count_using_approxmc(skolemfc->p->nGVars(),
                             skolemfc->p->g_formula_clauses,
                             skolemfc->p->forall_vars,
                             epsilon_gc,
                             delta_gc);
    s1size = absolute_count_from_appmc(c);
  }
  cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
       << (cpuTime() - start_time_skolemfc)
       << "]  G formula has (projected) count: " << s1size << endl;
  cout << "c Pass Gcount: " << std::setprecision(2) << std::fixed
       << (cpuTime() - start_time_skolemfc) << endl;
  if (s1size == 0) okay = false;
  return s1size;
}

string SkolemFC::SklFC::print_cnf(uint64_t num_vars,
                                  vector<vector<Lit>> clauses,
                                  vector<uint> projection_vars)
{
  string tempfile;
  std::stringstream ss;

  ss << "p cnf " << num_vars << " " << clauses.size() << endl;
  if (projection_vars.size() > 0)
  {
    ss << "c p show";
    for (uint var : projection_vars)
    {
      ss << " " << var + 1;
    }
    ss << " 0" << endl;
  }
  for (const auto& clause : clauses)
  {
    for (const Lit& lit : clause)
    {
      ss << lit << " ";
    }
    ss << "0" << endl;
  }

  string cnfContent = ss.str();

  // Create a temporary file
  char tmpFilename[] = "/tmp/ganak_input_XXXXXX";
  tempfile = (string)tmpFilename;
  int fd = mkstemp(tmpFilename);  // Creates a unique temporary file
  if (fd == -1)
  {
    perror("mkstemp");
    exit(EXIT_FAILURE);
  }

  // Write CNF to the temporary file
  write(fd, cnfContent.c_str(), cnfContent.size());
  close(fd);  // Close the file descriptor

  return tempfile;
}

mpz_class SkolemFC::SklFC::count_using_ganak(uint64_t nvars,
                                             vector<vector<Lit>> clauses,
                                             vector<uint> projection,
                                             uint32_t timeout)
{
  mpz_class ganak_count;

  std::stringstream ss;
  if (timeout == 0)
  {
    return 0;
  }

  ss << "p cnf " << nvars << " " << clauses.size() << endl;
  if (projection.size() > 0)
  {
    ss << "c p show";
    for (uint var : projection)
    {
      ss << " " << var + 1;
    }
    ss << " 0" << endl;
  }
  for (const auto& clause : clauses)
  {
    for (const Lit& lit : clause)
    {
      ss << lit << " ";
    }
    ss << "0" << endl;
  }

  string cnfContent = ss.str();

  // Create a temporary file
  char tmpFilename[] = "/tmp/ganak_input_XXXXXX";
  int fd = mkstemp(tmpFilename);  // Creates a unique temporary file
  if (fd == -1)
  {
    perror("mkstemp");
    exit(EXIT_FAILURE);
  }

  // Write CNF to the temporary file
  write(fd, cnfContent.c_str(), cnfContent.size());
  close(fd);  // Close the file descriptor

  pid_t pid;

  // Create a pipe for communication between parent and child
  int toParent[2];
  if (pipe(toParent) == -1)
  {
    std::cerr << "Fork failed" << std::endl;
    return 1;
  }

  pid = fork();
  if (pid == -1)
  {
    perror("fork");
    _exit(EXIT_FAILURE);
  }

  cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
       << (cpuTime() - start_time_skolemfc) << "] counting formula using ganak"
       << endl;

  if (pid == 0)
  {  // Child process
    // Redirect stdout to the write-end of the pipe
    dup2(toParent[1], STDOUT_FILENO);
    close(toParent[0]);  // Close the read-end as it's not used by the child
    close(toParent[1]);  // Close the write-end after duplicating

    // Execute ganak with the temporary file as input
    execlp("./ganak", "./ganak", tmpFilename, (char*)NULL);
    perror("execlp");  // execlp only returns on error
    exit(EXIT_FAILURE);
  }
  else
  {  // Parent process
     //     if (timeout > 0) alarm(timeout);
    ganak_timeout = false;
    int status;
    close(toParent[1]);  // Close the write-end as it's not used by the parent

    // Read output from ganak
    char buffer[128];
    string ganakOutput;
    ssize_t count;
    while ((count = read(toParent[0], buffer, sizeof(buffer) - 1)) > 0)
    {
      buffer[count] = '\0';
      ganakOutput += buffer;
    }
    close(toParent[0]);  // Close the read-end after reading

    // Wait for child process to finish
    waitpid(pid, &status, 0);

    // Parse ganakOutput to find the required number and store it in ganak_count
    std::istringstream iss(ganakOutput);
    string line;
    bool ganak_count_line_found = false;
    while (getline(iss, line))
    {
      if (line.rfind("c s exact arb int ", 0) == 0)
      {  // Line starts with the pattern
        string numberStr = line.substr(strlen("c s exact arb int "));
        ganak_count.set_str(numberStr, 10);  // Set the value of ganak_count, 10
                                             // is the base for decimal numbers
        ganak_count_line_found = true;
        break;
      }
    }
    if (!ganak_count_line_found)
    {
      cout << "c [sklfc] ERROR: No Count line found from ganak" << endl;
      exit(0);
    }
    if (ganak_count == 0)
    {
      cout << "c [sklfc] G is UNSAT. Est1 = 0" << endl;
    }
    if (verb >= 2) cout << "c Ganak Count " << ganak_count << endl;
    // Remove the temporary file
    unlink(tmpFilename);
  }

  return ganak_count;
}

void SkolemFC::SklFC::get_sample_num_est()
{
  if (static_samp)
  {
    sample_num_est = 500;
    return;
  }

  cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
       << (cpuTime() - start_time_skolemfc)
       << "] estimating number of samples needed" << endl;

  CMSat::SATSolver cms;
  vector<vector<Lit>> clauses;

  cms.new_vars(skolemfc->p->nGVars());
  vector<uint> empty;

  for (auto& clause : skolemfc->p->g_formula_clauses)
  {
    clauses.push_back(clause);
    cms.add_clause(clause);
  }

  auto res = cms.solve();

  vector<lbool> model = cms.get_model();
  vector<Lit> new_clause;

  for (auto var : skolemfc->p->forall_vars)
  {
    new_clause.clear();
    if (model[var] == CMSat::l_True)
    {
      new_clause.push_back(Lit(var, false));
    }
    else
    {
      assert(model[var] == CMSat::l_False);
      new_clause.push_back(Lit(var, true));
    }
    clauses.push_back(new_clause);
  }

  if (res == CMSat::l_False)
  {
    cout << "c Unsat G" << endl;
  }

  cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
       << (cpuTime() - start_time_skolemfc)
       << "] got a solution by CMS for estimating, now counting that" << endl;

  ApproxMC::SolCount c;
  //   c = log_count_from_absolute(
  //       count_using_ganak(skolemfc->p->nGVars(), clauses, empty, 0));
  //   if (c.cellSolCount == 0 )
  c = count_using_approxmc(skolemfc->p->nGVars(), clauses, empty, 4.66, 0.7);

  cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
       << (cpuTime() - start_time_skolemfc)
       << "] Estimated count from each it: " << c.cellSolCount << " * 2 ** "
       << c.hashCount << endl;

  int multisample = 500;

  sample_num_est = (int)(thresh.get_d() / (c.hashCount + log2(c.cellSolCount)));

  cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
       << (cpuTime() - start_time_skolemfc)
       << "] approximated number of iterations: " << sample_num_est << endl;

  sample_num_est =
      (((int)(thresh.get_d() / (c.hashCount + log2(c.cellSolCount)))
        + multisample)
       / multisample)
      * multisample;

  cout << "c Pass SizeEst: " << std::setprecision(2) << std::fixed
       << (cpuTime() - start_time_skolemfc) << endl;
}

void SkolemFC::SklFC::unigen_callback(const vector<int>& solution, void*)
{
  // std::lock_guard<std::mutex> lock(vec_mutex);
  if (verb > 2)
    cout << "c Generated Sample size now:" << samples_from_unisamp.size()
         << endl;
  samples_from_unisamp.push_back(solution);
}

void SkolemFC::SklFC::get_samples_multithread(uint64_t samples_needed)
{
  for (uint i = 0; i < numthreads; ++i)
  {
    threads.push_back(
        std::thread(&SklFC::get_samples, this, samples_needed / numthreads, i));
  }
  for (auto& thread : threads)
  {
    thread.join();
  }
}

void SkolemFC::SklFC::get_samples(uint64_t samples_needed, int _seed)
{
  //{std::lock_guard<std::mutex> lock(cout_mutex); }

  if (iteration < 2)
    cout << "c\nc ---- [ sampling ] "
            "----------------------------------------------------------\nc\n";

  cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
       << (cpuTime() - start_time_skolemfc) << "] starting to get "
       << samples_needed << " samples" << endl;

  int oracle_verb = std::max(0, (int)verb - 2);

  std::unique_ptr<CMSat::FieldGen> ug_fg = std::make_unique<ArjunNS::FGenMpq>();
  ApproxMC::AppMC* ug_appmc = new ApproxMC::AppMC(ug_fg);
  UniGen::UniG* unigen = new UniGen::UniG(ug_appmc);

  ug_appmc->set_verbosity(oracle_verb);
  ug_appmc->set_seed(iteration * _seed);

  ug_appmc->set_reuse_models(1);
  ug_appmc->set_sparse(0);
  ug_appmc->set_simplify(1);
  ug_appmc->set_var_elim_ratio(1.6);

  unigen->set_verbosity(oracle_verb);

  if (use_unisamp)
  {
    ug_appmc->set_epsilon(0.414);
    ug_appmc->set_delta(0.1);
  }

  // Build the formula in a SimplifiedCNF, minimize the independent set via
  // Arjun, then hand the result off to appmc/unigen.
  ArjunNS::SimplifiedCNF ug_cnf(ug_fg);
  ug_cnf.new_vars(skolemfc->p->nGVars());
  for (auto clause : skolemfc->p->g_formula_clauses) ug_cnf.add_clause(clause);
  ug_cnf.set_sampl_vars(skolemfc->p->forall_vars);

  ArjunNS::Arjun ug_arjun;
  ug_arjun.set_verb(0);
  ug_arjun.standalone_minimize_indep(ug_cnf, false);

  ug_appmc->new_vars(ug_cnf.nVars());
  for (const auto& cl : ug_cnf.clauses) ug_appmc->add_clause(cl);
  for (const auto& cl : ug_cnf.red_clauses) ug_appmc->add_red_clause(cl);
  vector<uint32_t> sampling_vars = ug_cnf.sampl_vars;

  unigen->set_callback([this](const vector<int>& solution,
                              void*) { this->unigen_callback(solution, NULL); },
                       NULL);

  ug_appmc->set_sampl_vars(sampling_vars);

  ApproxMC::SolCount c = ug_appmc->count();
  unigen->set_verb_sampler_cls(0);
  unigen->set_full_sampling_vars(skolemfc->p->forall_vars);
  unigen->sample(&c, samples_needed);

  delete unigen;
  delete ug_appmc;

  cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
       << (cpuTime() - start_time_skolemfc) << "] generated "
       << samples_from_unisamp.size() << " samples" << endl;

  if (iteration < 2)
    cout << "c Pass Sampling: " << std::setprecision(2) << std::fixed
         << (cpuTime() - start_time_skolemfc) << endl;
}

mpf_class SkolemFC::SklFC::get_est1(mpz_class s1size)
{
  if (!okay) return 0;
  return (thresh / (double)iteration) * (mpf_class)s1size;
}
bool SkolemFC::SklFC::check_if_approxmc_error_exceeds(
    mpf_class count, mpz_class _s2size, double _max_error_logcounter)
{
  bool check = false;
  if (_s2size * log(1 + epsilon_c) > _max_error_logcounter * count)
  {
    if (count / _s2size < approxmc_threshold)
      check = false;
    else
    {
      check = true;
      cout << "c [sklfc] error by model counting oracle exceeded by "
              "guarantees, this run failed "
           << endl;
      cout << "c [sklfc] try increasing  --max-error-logcounter, currently "
           << _max_error_logcounter << endl;
    }
  }
  return check;
}

vector<vector<Lit>> SkolemFC::SklFC::create_formula_from_sample(
    vector<vector<int>> samples, int sample_num)
{
  vector<vector<Lit>> formula = skolemfc->p->clauses;
  vector<Lit> new_clause;
  for (auto int_lit : samples[sample_num - sample_clearance_iteration])
  {
    bool isNegated = int_lit < 0;
    uint32_t varIndex =
        isNegated ? -int_lit - 1 : int_lit - 1;  // Convert to 0-based index
    Lit literal = isNegated ? ~Lit(varIndex, false) : Lit(varIndex, false);
    new_clause.clear();
    new_clause.push_back(literal);
    formula.push_back(new_clause);
  }
  if (skolemfc->p->verbosity > 3) skolemfc->p->print_formula(formula);
  return formula;
}

void SkolemFC::SklFC::get_and_add_count_onethred(vector<vector<int>> samples)
{
  cout << "This thread has samples: " << samples.size() << endl;
  for (uint it = 0; it < samples.size(); it++)
  {
    vector<vector<Lit>> sampling_formula =
        create_formula_from_sample(samples, it);
    std::unique_ptr<CMSat::FieldGen> th_fg = std::make_unique<ArjunNS::FGenMpq>();
    ApproxMC::AppMC* appmc = new ApproxMC::AppMC(th_fg);
    appmc->new_vars(skolemfc->p->nVars());
    for (auto& clause : sampling_formula)
    {
      appmc->add_clause(clause);
    }
    ApproxMC::SolCount c = appmc->count();
    double logcount_this_it = (double)c.hashCount + log2(c.cellSolCount);
    {
      std::lock_guard<std::mutex> lock(iter_mutex);
      if (log_skolemcount <= thresh)
      {
        iteration++;
        log_skolemcount += logcount_this_it;
      }
    }
    if (skolemfc->p->verbosity > 1 && iteration % 10 == 0)
    {
      std::lock_guard<std::mutex> lock(cout_mutex);

      cout << "c [sklfc] logcount at iteration " << iteration << ": "
           << logcount_this_it << " log_skolemcount: " << log_skolemcount
           << endl;
    }
  }
}
void SkolemFC::SklFC::get_and_add_count_multithred()
{
  vector<vector<vector<int>>> partitioned_solutions(numthreads);
  size_t partition_size =
      std::ceil(samples_from_unisamp.size() / static_cast<double>(numthreads));

  auto it = samples_from_unisamp.begin();
  for (uint i = 0; i < numthreads; ++i)
  {
    auto end = ((i + 1) * partition_size < samples_from_unisamp.size())
                   ? it + partition_size
                   : samples_from_unisamp.end();
    partitioned_solutions[i] = vector<vector<int>>(it, end);
    it = end;
  }
  for (uint i = 0; i < numthreads; ++i)
  {
    {
      threads.push_back(std::thread(
          &SklFC::get_and_add_count_onethred, this, partitioned_solutions[i]));
    }
    for (auto& thread : threads)
    {
      thread.join();
    }
  }
}

ApproxMC::SolCount SkolemFC::SklFC::count_using_approxmc(
    uint64_t nvars,
    vector<vector<Lit>> clauses,
    vector<uint> proj_vars,
    double _epsilon,
    double _delta)
{
  int oracle_verb = std::max(0, (int)verb - 2);

  std::unique_ptr<CMSat::FieldGen> ac_fg = std::make_unique<ArjunNS::FGenMpq>();
  ApproxMC::AppMC* appmc = new ApproxMC::AppMC(ac_fg);

  if (verb > 1)
  {
    cout << "c Running ApproxMC on vars:" << nvars
         << " clauses: " << clauses.size() << " with epsilon " << _epsilon
         << " delta " << std::setprecision(15) << _delta << endl;
  }

  ArjunNS::SimplifiedCNF ac_cnf(ac_fg);
  ac_cnf.new_vars(nvars);
  for (auto& clause : clauses) ac_cnf.add_clause(clause);
  if (proj_vars.empty())
    ac_cnf.start_with_clean_sampl_vars();
  else
    ac_cnf.set_sampl_vars(proj_vars);

  ArjunNS::Arjun ac_arjun;
  ac_arjun.set_verb(oracle_verb);
  ac_arjun.set_simp(1);
  ac_arjun.standalone_minimize_indep(ac_cnf, false);

  if (skolemfc->p->verbosity >= 2)
  {
    cout << "c [sklfc->arjun] Arjun returned formula with " << ac_cnf.nVars()
         << " variables " << ac_cnf.clauses.size() << " clauses and "
         << ac_cnf.sampl_vars.size() << " sized ind set" << endl;
  }

  vector<uint32_t> sampling_vars = ac_cnf.sampl_vars;
  appmc->new_vars(ac_cnf.nVars());
  for (const auto& cl : ac_cnf.clauses) appmc->add_clause(cl);
  for (const auto& cl : ac_cnf.red_clauses) appmc->add_red_clause(cl);
  appmc->set_sampl_vars(sampling_vars);
  appmc->set_epsilon(_epsilon);
  appmc->set_delta(_delta);
  appmc->set_verbosity(oracle_verb);

  ApproxMC::SolCount c;
  if (!sampling_vars.empty())
  {
    c = appmc->count();
  }
  else
  {
    c.hashCount = 0;
    c.cellSolCount = 1;
  }

  delete appmc;

  if (skolemfc->p->verbosity >= 2)
  {
    cout << "c [sklfc->appmc] ApproxMC returned count: " << c.cellSolCount
         << " * 2 ** " << c.hashCount << endl;
  }

  return c;
}

void SkolemFC::SklFC::get_and_add_count_for_a_sample()
{
  if (iteration >= samples_from_unisamp.size() + sample_clearance_iteration
      && iteration > 1)
  {
    if (numthreads > 1)
      get_samples_multithread(sample_num_est * 0.25);
    else
    {
      int multisample = 500;
      if (verb > 1)
      {
        cout << "c last sampling generated " << samples_from_unisamp.size()
             << " samples. but we need more" << endl;
        cout << "c current estimated number of iterations: "
             << (thresh.get_d() * iteration / log_skolemcount.get_d()) << endl;
        cout << "c asking unisamp to generate: "
             << (uint64_t)((thresh.get_d() * iteration
                            / log_skolemcount.get_d())
                           - samples_from_unisamp.size())
             << endl;
      }
      uint64_t new_total_sample_number =
          (((thresh.get_d() * iteration / log_skolemcount.get_d() + multisample)
            / multisample)
           * multisample);
      sample_num_est = new_total_sample_number;
      if (new_total_sample_number > 1000) new_total_sample_number = 1000;
      samples_from_unisamp.clear();
      sample_clearance_iteration = iteration;
      get_samples(new_total_sample_number, seed);
    }
  }

  vector<vector<Lit>> sampling_formula =
      create_formula_from_sample(samples_from_unisamp, iteration);

  ApproxMC::SolCount c;
  vector<uint> empty;
  double _delta;
  if (iteration == 0 || log_skolemcount < 0.0001)
    _delta = delta_c / thresh.get_d();
  else
    _delta = 0.5 * delta_c * log_skolemcount.get_d()
             / ((double)iteration * thresh.get_d());
  double _epsilon = 4.657;

  c = count_using_approxmc(
      skolemfc->p->nVars(), sampling_formula, empty, _epsilon, _delta);

  double logcount_this_it = (double)(c.hashCount) + log2(c.cellSolCount);

  iteration++;
  log_skolemcount += logcount_this_it;

  if (show_count())
  {
    printf("c %10.2f %10lu %15.1f     %.2f \n",
           (cpuTime() - start_time_skolemfc),
           iteration,
           get_progress(),
           get_current_estimate().get_d());
    //     cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
    //          << (cpuTime() - start_time_skolemfc) << "] iteration:   " <<
    //          iteration
    //          << " [mc " << c.cellSolCount << " * 2 ** " << c.hashCount << "
    //          ]"
    //          << endl;
  }
}

mpz_class SkolemFC::SklFC::absolute_count_from_appmc(ApproxMC::SolCount c)
{
  mpz_class s1size;
  mpz_pow_ui(s1size.get_mpz_t(), mpz_class(2).get_mpz_t(), c.hashCount);
  s1size *= c.cellSolCount;
  return s1size;
}

ApproxMC::SolCount SkolemFC::SklFC::log_count_from_absolute(mpz_class count)
{
  ApproxMC::SolCount c;
  if (count > 0)
    c.cellSolCount = 1;
  else
    c.cellSolCount = 0;
  c.hashCount = (uint32_t)mpz_sizeinbase(count.get_mpz_t(), 2) - 1;
  return c;
}

void SkolemFC::SklFC::count()
{
  mpf_class count;

  set_constants();

  count = get_est0();

  skolemfc->p->create_g_formula();

  s2size = get_g_count();

  if (okay) get_sample_num_est();

  if (numthreads > 1)
  {
    if (okay) get_samples_multithread(sample_num_est);
    get_and_add_count_multithred();
  }
  else if (okay)
  {
    get_samples(sample_num_est);
    cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
         << (cpuTime() - start_time_skolemfc)
         << "] Starting to get count for each assignment" << endl;

    cout << "c\nc ---- [ counting ] "
            "----------------------------------------------------------\nc\n";
    cout << "c\nc   seconds    iterations      progress         estimate \nc\n";

    while ((log_skolemcount <= thresh) && okay)
    {
      get_and_add_count_for_a_sample();
    }
  }
  count += get_est1(s2size);

  if (check_if_approxmc_error_exceeds(count, s2size, max_error_logcounter))
    exit(0);

  cout << "c\nc ---- [ result ] "
          "------------------------------------------------------------\nc\n";

  cout << "s fc 2 ** " << count << endl;
}

const char* SkolemFC::SklFC::get_version_info()
{
  return SkolemFCInt::get_version_sha1();
}
const char* SkolemFC::SklFC::get_compilation_env()
{
  return SkolemFCInt::get_compilation_env();
}

void SkolemFC::SklFC::set_parameters()
{
  epsilon = skolemfc->p->epsilon;
  delta = skolemfc->p->delta;
  seed = skolemfc->p->seed;
  verb = skolemfc->p->verbosity;
}

void SkolemFC::SklFC::set_g_counter_parameters(double _epsilon, double _delta)
{
  epsilon_gc = _epsilon;
  delta_gc = _delta;
  if (!exactcount_s2)
  {
    double epsilon_ = skolemfc->p->epsilon;
    skolemfc->p->epsilon =
        std::min(((1.0 + epsilon_) / (1 + epsilon_gc) - 1),
                 (epsilon_ + epsilon_gc * epsilon_ - epsilon_gc));
    skolemfc->p->delta = (skolemfc->p->delta - delta_gc) / (1 + delta_gc);
  }
  if (noguarnatee)
  {
    epsilon_gc = 4.8;
    delta_gc = 0.8;
  }
}

void SkolemFC::SklFC::set_dklr_parameters(double epsilon_w,
                                          double delta_w,
                                          double _max_error_logcounter)
{
  assert(epsilon > 0);
  epsilon_f = (epsilon - _max_error_logcounter) * epsilon_w;
  max_error_logcounter = _max_error_logcounter;
  delta_f = delta * delta_w;
  epsilon_s = epsilon - epsilon_f - 0.1;
  delta_c = (delta - delta_f);
  epsilon_c = 4.658;
}

void SkolemFC::SklFC::set_oracles(bool _use_unisamp,
                                  bool _exactcount_s0,
                                  bool _exactcount_s2)
{
  use_unisamp = _use_unisamp;
  exactcount_s0 = _exactcount_s0;
  exactcount_s2 = _exactcount_s2;
}

void SkolemFC::SklFC::set_ignore_unsat(bool _ignore_unsat)
{
  ignore_unsat = _ignore_unsat;
}

void SkolemFC::SklFC::set_static_samp(bool _static_samp)
{
  static_samp = _static_samp;
}

void SkolemFC::SklFC::set_noguarntee_mode(bool _noguarnatee)
{
  noguarnatee = _noguarnatee;
}

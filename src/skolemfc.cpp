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
  if (skolemfc->p->verbosity <= 1)
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
  if (use_appmc_for_est0)
  {
    est0 = get_est0_approxmc();
  }
  else
  {
    est0 = get_est0_ganak();
  }
  return est0;
}

string SkolemFC::SklFC::print_cnf(uint64_t num_clauses,
                                  vector<vector<Lit>> clauses,
                                  vector<uint> projection_vars)
{
  string tempfile;
  std::stringstream ss;

  ss << "p cnf " << num_clauses << " " << clauses.size() << endl;
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

mpz_class SkolemFC::SklFC::get_est0_ganak()
{
  mpz_class est0;
  mpz_class ganak_count;

  std::stringstream ss;

  ss << "p cnf " << nVars() << " " << skolemfc->p->clauses.size() << endl;

  ss << "c p show";
  for (uint var : skolemfc->p->forall_vars)
  {
    ss << " " << var + 1;
  }
  ss << " 0" << endl;

  for (const auto& clause : skolemfc->p->clauses)
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
    perror("pipe");
    exit(EXIT_FAILURE);
  }

  pid = fork();
  if (pid == -1)
  {
    perror("fork");
    exit(EXIT_FAILURE);
  }

  cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
       << (cpuTime() - start_time_skolemfc)
       << "] counting for F formula using ganak" << endl;

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
  {                      // Parent process
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
    waitpid(pid, NULL, 0);

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
      cout << "c [sklfc] F is UNSAT. Est1 = 0" << endl;
    }
    cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
         << (cpuTime() - start_time_skolemfc)
         << "]  F formula has exact (projected) count: " << ganak_count << endl;
    mpz_pow_ui(est0.get_mpz_t(),
               mpz_class(2).get_mpz_t(),
               skolemfc->p->forall_vars.size());
    est0 -= ganak_count;
    cout << "c [sklfc] Est0 = " << est0 << endl;

    est0 *= skolemfc->p->exists_vars.size();  // TODO missing 2^n

    est0 *= ganak_count;
    cout << "c [sklfc] Est0 = " << est0 << endl;

    // Remove the temporary file
    unlink(tmpFilename);
  }
  return est0;
}

mpz_class SkolemFC::SklFC::get_est0_approxmc()
{
  mpz_class num_sol, est0;

  ApproxMC::AppMC appmc;

  appmc.new_vars(skolemfc->p->nVars());

  for (auto& clause : skolemfc->p->clauses)
  {
    appmc.add_clause(clause);
  }

  appmc.set_projection_set(skolemfc->p->forall_vars);

  cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
       << (cpuTime() - start_time_skolemfc)
       << "] counting for F formula using ApproxMC. SkolemFC will not give any "
          "guarantee on count!!"
       << endl;

  ApproxMC::SolCount c = appmc.count();

  if (c.cellSolCount == 0)
  {
    cout << "c [sklfc] F is UNSAT. Est1 = 0" << endl;
    okay = false;
  }

  mpz_pow_ui(est0.get_mpz_t(),
             mpz_class(2).get_mpz_t(),
             skolemfc->p->forall_vars.size());
  mpz_pow_ui(
      num_sol.get_mpz_t(), mpz_class(2).get_mpz_t(), c.hashCount);  // a^b

  num_sol *= c.cellSolCount;
  est0 -= num_sol;

  cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
       << (cpuTime() - start_time_skolemfc)
       << "]  F formula has approximated (projected) count: " << num_sol
       << endl;

  cout << "c [sklfc] Approximate Est0 = " << est0 << endl;
  return est0;
}

mpz_class SkolemFC::SklFC::get_g_count_approxmc()
{
  mpz_class num_sol;
  ApproxMC::SolCount c;
  skolemfc->p->create_g_formula();

  appmc_g.new_vars(skolemfc->p->nGVars());
  for (auto& clause : skolemfc->p->g_formula_clauses)
  {
    appmc_g.add_clause(clause);
  }
  appmc_g.set_projection_set(skolemfc->p->forall_vars);

  cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
       << (cpuTime() - start_time_skolemfc) << "] counting for G formula"
       << endl;

  c = appmc_g.count();

  mpz_pow_ui(
      num_sol.get_mpz_t(), mpz_class(2).get_mpz_t(), c.hashCount);  // a^b
  num_sol *= c.cellSolCount;

  cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
       << (cpuTime() - start_time_skolemfc)
       << "] G formula has (projected) count: " << num_sol << endl;
  if (num_sol == 0)
  {
    cout << "c [sklfc] G is UNSAT. Est1 = 0" << endl;
    okay = false;
  }
  return num_sol;
}

mpz_class SkolemFC::SklFC::get_g_count_ganak()
{
  mpz_class ganak_count;  // Use mpz_class instead of mpz_t

  skolemfc->p->create_g_formula();

  std::stringstream ss;

  ss << "p cnf " << skolemfc->p->nGVars() << " "
     << skolemfc->p->g_formula_clauses.size() << endl;

  ss << "c p show";
  for (uint var : skolemfc->p->forall_vars)
  {
    ss << " " << var + 1;
  }
  ss << " 0" << endl;

  for (const auto& clause : skolemfc->p->g_formula_clauses)
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
    perror("pipe");
    exit(EXIT_FAILURE);
  }

  pid = fork();
  if (pid == -1)
  {
    perror("fork");
    exit(EXIT_FAILURE);
  }

  cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
       << (cpuTime() - start_time_skolemfc)
       << "] counting for G formula using ganak" << endl;

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
  {                      // Parent process
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
    waitpid(pid, NULL, 0);

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
    cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
         << (cpuTime() - start_time_skolemfc)
         << "]  G formula has exact (projected) count: " << ganak_count << endl;

    // Remove the temporary file
    unlink(tmpFilename);
  }
  return ganak_count;
}

void SkolemFC::SklFC::get_sample_num_est()
{
  cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
       << (cpuTime() - start_time_skolemfc)
       << "] estimating number of samples needed" << endl;

  CMSat::SATSolver cms;
  ApproxMC::AppMC appmc;
  ArjunNS::Arjun arjun;

  arjun.set_seed(123);
  arjun.set_verbosity(0);
  arjun.set_simp(1);

  arjun.new_vars(skolemfc->p->nGVars());
  cms.new_vars(skolemfc->p->nGVars());

  for (auto& clause : skolemfc->p->g_formula_clauses)
  {
    arjun.add_clause(clause);
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
    arjun.add_clause(new_clause);
  }

  cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
       << (cpuTime() - start_time_skolemfc)
       << "] got a solution by CMS for estimating, now counting that" << endl;

  if (res == CMSat::l_False)
  {
    cout << "c Unsat G" << endl;
  }

  vector<uint32_t> sampling_vars;
  vector<uint32_t> empty_occ_sampl_vars;

  arjun.start_with_clean_sampling_set();
  empty_occ_sampl_vars = arjun.get_empty_occ_sampl_vars();
  sampling_vars = arjun.get_indep_set();
  const auto ret =
      arjun.get_fully_simplified_renumbered_cnf(sampling_vars, false, true);

  if (skolemfc->p->verbosity > 1)
  {
    cout << "c [sklfc->arjun] Arjun returned formula with " << ret.nvars
         << " variables " << ret.cnf.size() << " clauses and "
         << ret.sampling_vars.size() << " sized ind set" << endl;
  }

  appmc.new_vars(ret.nvars);
  for (const auto& cl : ret.cnf) appmc.add_clause(cl);
  sampling_vars = ret.sampling_vars;
  uint32_t offset_count_by_2_pow = ret.empty_occs;
  appmc.set_projection_set(sampling_vars);
  appmc.set_verbosity(0);

  ApproxMC::SolCount c;
  if (!sampling_vars.empty())
  {
    appmc.set_projection_set(sampling_vars);
    c = appmc.count();
  }
  else
  {
    c.hashCount = 0;
    c.cellSolCount = 1;
  }

  cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
       << (cpuTime() - start_time_skolemfc)
       << "] Estimated count from each it: " << c.cellSolCount << " * 2 ** "
       << c.hashCount + offset_count_by_2_pow << endl;
  sample_num_est =
      1.0 * thresh
      / (c.hashCount + offset_count_by_2_pow + log2(c.cellSolCount));
  //  sample_num_est = 1320;
  cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
       << (cpuTime() - start_time_skolemfc)
       << "] approximated number of iterations: " << sample_num_est << endl;
}

void SkolemFC::SklFC::unigen_callback(const vector<int>& solution, void*)
{
  for (uint32_t i = 0; i < solution.size(); i++)
  {
    std::lock_guard<std::mutex> lock(vec_mutex);
    samples_from_unisamp.push_back(solution);
  }
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
  {
    std::lock_guard<std::mutex> lock(cout_mutex);
    cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
         << (cpuTime() - start_time_skolemfc) << "] starting to get "
         << samples_needed << " samples" << endl;
  }

  auto ug_appmc = new ApproxMC::AppMC;
  auto unigen = new UniGen::UniG(ug_appmc);

  ug_appmc->set_verbosity(0);
  //   ug_appmc->set_epsilon(0.414);
  //   ug_appmc->set_delta(0.1);
  ug_appmc->set_seed(iteration * _seed);
  ug_appmc->new_vars(skolemfc->p->nGVars());
  ug_appmc->set_projection_set(skolemfc->p->forall_vars);

  unigen->set_verbosity(0);
  //   unigen->set_unisamp(1);
  //   unigen->set_unisamp_epsilon(0.2);
  unigen->set_callback([this](const vector<int>& solution,
                              void*) { this->unigen_callback(solution, NULL); },
                       NULL);

  for (auto& clause : skolemfc->p->g_formula_clauses)
  {
    ug_appmc->add_clause(clause);
  }

  ApproxMC::SolCount c = ug_appmc->count();

  unigen->set_full_sampling_vars(skolemfc->p->forall_vars);
  unigen->sample(&c, samples_needed);

  cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
       << (cpuTime() - start_time_skolemfc) << "] generated " << samples_needed
       << " samples" << endl;
}

mpf_class SkolemFC::SklFC::get_est1(mpz_class s1size)
{
  return (thresh / (double)iteration) * (mpf_class)s1size;
}

vector<vector<Lit>> SkolemFC::SklFC::create_formula_from_sample(
    vector<vector<int>> samples, int sample_num)
{
  // TODO if already made samples are not enough, then create a new set
  vector<vector<Lit>> formula = skolemfc->p->clauses;
  vector<Lit> new_clause;
  for (auto int_lit : samples[sample_num])
  {
    bool isNegated = int_lit < 0;
    uint32_t varIndex =
        isNegated ? -int_lit - 1 : int_lit - 1;  // Convert to 0-based index
    Lit literal = isNegated ? ~Lit(varIndex, false) : Lit(varIndex, false);
    new_clause.clear();
    new_clause.push_back(literal);
    formula.push_back(new_clause);
  }
  if (skolemfc->p->verbosity > 2) skolemfc->p->print_formula(formula);
  return formula;
}

void SkolemFC::SklFC::get_and_add_count_onethred(vector<vector<int>> samples)
{
  cout << "This thread has samples: " << samples.size() << endl;
  for (uint it = 0; it < samples.size(); it++)
  {
    vector<vector<Lit>> sampling_formula =
        create_formula_from_sample(samples, it);
    ApproxMC::AppMC appmc;
    appmc.new_vars(skolemfc->p->nVars());
    for (auto& clause : sampling_formula)
    {
      appmc.add_clause(clause);
    }
    ApproxMC::SolCount c = appmc.count();
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

void SkolemFC::SklFC::get_and_add_count_for_a_sample()
{
  if (iteration >= sample_num_est)
  {
    if (numthreads > 1)
      get_samples_multithread(sample_num_est * 0.25);
    else
      get_samples(sample_num_est * 0.25);
    sample_num_est += sample_num_est * 0.25;
  }

  vector<vector<Lit>> sampling_formula =
      create_formula_from_sample(samples_from_unisamp, iteration);

  ApproxMC::AppMC appmc;
  ArjunNS::Arjun arjun;

  arjun.set_seed(123);
  arjun.set_verbosity(0);
  arjun.set_simp(1);
  arjun.new_vars(skolemfc->p->nVars());

  for (auto& clause : sampling_formula) arjun.add_clause(clause);

  vector<uint32_t> sampling_vars;
  vector<uint32_t> empty_occ_sampl_vars;

  arjun.start_with_clean_sampling_set();
  empty_occ_sampl_vars = arjun.get_empty_occ_sampl_vars();
  sampling_vars = arjun.get_indep_set();
  const auto ret =
      arjun.get_fully_simplified_renumbered_cnf(sampling_vars, false, true);

  if (skolemfc->p->verbosity > 2)
  {
    cout << "c [sklfc->arjun] Arjun returned formula with " << ret.nvars
         << " variables " << ret.cnf.size() << " clauses and "
         << ret.sampling_vars.size() << " sized ind set" << endl;
  }

  appmc.new_vars(ret.nvars);
  for (const auto& cl : ret.cnf) appmc.add_clause(cl);
  sampling_vars = ret.sampling_vars;
  uint32_t offset_count_by_2_pow = ret.empty_occs;
  appmc.set_projection_set(sampling_vars);
  appmc.set_pivot_by_sqrt2(0);
  appmc.set_verbosity(0);

  if (skolemfc->p->verbosity > 3)
  {
    vector<uint> projection_vars;
    string tmpfile = print_cnf(nVars(), sampling_formula, projection_vars);
    cout << "c [sklfc] CNF file strored in: " << tmpfile << endl;
  }

  appmc.set_delta(1.0 / (double)sample_num_est);
  ApproxMC::SolCount c;
  if (!sampling_vars.empty())
  {
    appmc.set_projection_set(sampling_vars);
    c = appmc.count();
  }
  else
  {
    c.hashCount = 0;
    c.cellSolCount = 1;
  }
  //= appmc.count();

  double logcount_this_it =
      (double)(c.hashCount + offset_count_by_2_pow) + log2(c.cellSolCount);

  iteration++;
  log_skolemcount += logcount_this_it;

  if (show_count())
  {
    cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
         << (cpuTime() - start_time_skolemfc) << "] logcount at iteration "
         << iteration << ": " << c.cellSolCount << "* 2 **"
         << c.hashCount + offset_count_by_2_pow << " " << logcount_this_it
         << " log_skolemcount: " << log_skolemcount << endl;
  }
}

void SkolemFC::SklFC::count()
{
  mpf_class count;
  mpz_class s1size;
  set_constants();

  count = get_est0();
  s1size = get_g_count_ganak();

  if (okay) get_sample_num_est();

  if (numthreads > 1)
  {
    if (okay) get_samples_multithread(sample_num_est);
    get_and_add_count_multithred();
  }
  else
  {
    get_samples(sample_num_est);
    cout << "c [sklfc] [" << std::setprecision(2) << std::fixed
         << (cpuTime() - start_time_skolemfc)
         << "] Starting to get count for each assignment" << endl;

    while ((log_skolemcount <= thresh) && okay)
    {
      get_and_add_count_for_a_sample();
    }
  }
  count += get_est1(s1size);

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

void SkolemFC::SklFC::set_dklr_parameters(double epsilon_w, double delta_w)
{
  assert(epsilon > 0);
  epsilon_f = epsilon * epsilon_w;
  delta_f = delta * delta_w;
  epsilon_s = epsilon - epsilon_f - 0.1;
  delta_c = delta - delta_f;
  epsilon_c = 4.658;
}

void SkolemFC::SklFC::set_oracles(bool use_unisamp_i,
                                  bool exactcount_s0_i,
                                  bool exactcount_s1_i)
{
  use_unisamp = use_unisamp_i;
  exactcount_s0 = exactcount_s0_i;
  exactcount_s1 = exactcount_s1_i;
}

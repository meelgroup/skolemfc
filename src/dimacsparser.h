/*****************************************************************************
MiniSat -- Copyright (c) 2003-2006, Niklas Een, Niklas Sorensson
CryptoMiniSat -- Copyright (C) 2009-2020 Authors of CryptoMiniSat, see AUTHORS
file SkolemFC -- Copyright (C) 2024, Arijit Shaw, Brendan Juba and Kuldeep S.
Meel

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
******************************************************************************/

#pragma once

#include <string.h>

#include <cassert>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

#include <cryptominisat5/solvertypesmini.h>
#include <cryptominisat5/streambuffer.h>

using std::cout;
using std::endl;
using std::vector;

using CMSat::Lit;
namespace SkolemFC {

template <class C, class S>
class DimacsParser
{
 public:
  DimacsParser(S* solver, const std::string* debugLib, unsigned _verbosity);

  template <class T>
  bool parse_DIMACS(T input_stpeam,
                    const bool strict_header,
                    uint32_t offset_vars = 0);
  uint64_t max_var = numeric_limits<uint64_t>::max();
  vector<uint32_t> sampling_vars;
  vector<uint32_t> forall_vars, exists_vars;
  bool sampling_vars_found = false;
  vector<double> weights;
  uint32_t must_mult_exp2 = 0;
  const std::string dimacs_spec = "https://www.qbflib.org/qdimacs.html";
  const std::string please_read_dimacs =
      "\nPlease read DIMACS specification at "
      "https://www.qbflib.org/qdimacs.html";

 private:
  bool parse_DIMACS_main(C& in);
  bool readClause(C& in);
  bool parse_and_add_clause(C& in);
  bool parse_and_add_xor_clause(C& in);
  bool match(C& in, const char* str);
  bool parse_header(C& in);
  bool parseComments(C& in, const std::string& str);
  std::string stringify(uint32_t x) const;
  bool check_var(const uint32_t var);
  bool parseWeight(C& in);
  bool parseForallVars(C& in);
  bool parseExistsVars(C& in);
  std::string get_debuglib_fname() const;

  S* solver;
  std::string debugLib;
  unsigned verbosity;

  // Stat
  size_t lineNum;

  // Printing partial solutions to debugLibPart1..N.output when "debugLib" is
  // set to TRUE
  uint32_t debugLibPart = 1;

  // check header strictly
  bool strict_header = false;
  bool header_found = false;
  int num_header_vars = 0;
  int num_header_cls = 0;
  uint32_t offset_vars = 0;

  // Reduce temp overhead
  vector<Lit> lits;
  vector<uint32_t> vars;

  size_t norm_clauses_added = 0;
  size_t forall_vars_added = 0;
  size_t exists_vars_added = 0;
};

template <class C, class S>
DimacsParser<C, S>::DimacsParser(S* _solver,
                                 const std::string* _debugLib,
                                 unsigned _verbosity)
    : solver(_solver), verbosity(_verbosity), lineNum(0)
{
  if (_debugLib)
  {
    debugLib = *_debugLib;
  }
}

template <class C, class S>
std::string DimacsParser<C, S>::stringify(uint32_t x) const
{
  std::ostringstream o;
  o << x;
  return o.str();
}

template <class C, class S>
bool DimacsParser<C, S>::check_var(const uint32_t var)
{
  if (var > max_var)
  {
    std::cerr << "ERROR! "
              << "Variable requested is too large for DIMACS parser parameter: "
              << var << endl
              << "--> At line " << lineNum + 1 << please_read_dimacs << endl;
    return false;
  }

  if (var >= (1ULL << 28))
  {
    std::cerr << "ERROR! "
              << "Variable requested is far too large: " << var + 1 << endl
              << "--> At line " << lineNum + 1 << please_read_dimacs << endl;
    return false;
  }

  if (strict_header && !header_found)
  {
    std::cerr << "ERROR! "
              << "DIMACS header ('p cnf vars cls') never found!" << endl;
    return false;
  }

  if ((int)var >= num_header_vars && strict_header)
  {
    std::cerr << "ERROR! "
              << "Variable requested is larger than the header told us." << endl
              << " -> var is : " << var + 1 << endl
              << " -> header told us maximum will be : " << num_header_vars
              << endl
              << " -> At line " << lineNum + 1 << endl;
    return false;
  }

  if (var >= solver->nVars())
  {
    assert(!strict_header);
    solver->new_vars(var - solver->nVars() + 1);
  }

  return true;
}

template <class C, class S>
bool DimacsParser<C, S>::readClause(C& in)
{
  int32_t parsed_lit;
  uint32_t var;
  for (;;)
  {
    if (!in.parseInt(parsed_lit, lineNum))
    {
      return false;
    }
    if (parsed_lit == 0)
    {
      break;
    }

    var = std::abs(parsed_lit) - 1;
    var += offset_vars;

    if (!check_var(var))
    {
      return false;
    }

    lits.push_back((parsed_lit > 0) ? Lit(var, false) : Lit(var, true));
    if (*in != ' ')
    {
      std::cerr << "ERROR! "
                << "After last element on the line must be 0" << endl
                << "--> At line " << lineNum + 1 << please_read_dimacs << endl
                << endl;
      return false;
    }
  }

  return true;
}

template <class C, class S>
bool DimacsParser<C, S>::match(C& in, const char* str)
{
  for (; *str != 0; ++str, ++in)
    if (*str != *in) return false;
  return true;
}

template <class C, class S>
bool DimacsParser<C, S>::parse_header(C& in)
{
  ++in;
  in.skipWhitespace();
  std::string str;
  in.parseString(str);
  if (str == "cnf")
  {
    if (header_found && strict_header)
    {
      std::cerr << "ERROR: CNF header ('p cnf vars cls') found twice in file! "
                   "Exiting."
                << endl;
      exit(-1);
    }
    header_found = true;

    if (!in.parseInt(num_header_vars, lineNum)
        || !in.parseInt(num_header_cls, lineNum))
    {
      return false;
    }
    if (verbosity)
    {
      cout << "c -- header says num vars:   " << std::setw(12)
           << num_header_vars << endl;
      cout << "c -- header says num clauses:" << std::setw(12) << num_header_cls
           << endl;
    }
    if (num_header_vars < 0)
    {
      std::cerr << "ERROR: Number of variables in header cannot be less than 0"
                << endl;
      return false;
    }
    if (num_header_cls < 0)
    {
      std::cerr << "ERROR: Number of clauses in header cannot be less than 0"
                << endl;
      return false;
    }
    num_header_vars += offset_vars;

    if (solver->nVars() < (size_t)num_header_vars)
    {
      solver->new_vars(num_header_vars - solver->nVars());
    }
  }
  else
  {
    std::cerr << "PARSE ERROR! Unexpected char (hex: " << std::hex
              << std::setw(2) << std::setfill('0') << "0x" << *in
              << std::setfill(' ') << std::dec << ")"
              << " At line " << lineNum + 1 << "' in the header"
              << please_read_dimacs << endl;
    return false;
  }

  return true;
}

template <class C, class S>
std::string DimacsParser<C, S>::get_debuglib_fname() const
{
  std::string sol_fname =
      debugLib + "-debugLibPart" + stringify(debugLibPart) + ".output";
  return sol_fname;
}

template <class C, class S>
bool DimacsParser<C, S>::parse_and_add_clause(C& in)
{
  lits.clear();
  if (!readClause(in))
  {
    return false;
  }
  in.skipWhitespace();
  if (!in.skipEOL(lineNum))
  {
    return false;
  }
  lineNum++;
  solver->add_clause(lits);
  norm_clauses_added++;
  return true;
}

template <class C, class S>
bool DimacsParser<C, S>::parse_DIMACS_main(C& in)
{
  cout << "c\nc ---- [ parsing ] "
          "-----------------------------------------------------------\nc\n";

  std::string str;

  for (;;)
  {
    in.skipWhitespace();
    switch (*in)
    {
      case EOF: return true;
      case 'p':
        if (!parse_header(in))
        {
          return false;
        }
        in.skipLine();
        lineNum++;
        break;
      case 'c':
        ++in;
        in.parseString(str);
        if (!parseComments(in, str))
        {
          return false;
        }
        break;
      case 'a':
        ++in;
        if (!parseForallVars(in))
        {
          return false;
        }
        break;
      case 'e':
        ++in;
        if (!parseExistsVars(in))
        {
          return false;
        }
        break;
      case '\n':
        if (verbosity)
        {
          std::cout << "c WARNING: Empty line at line number " << lineNum + 1
                    << " -- this is not part of the QDIMACS specifications ("
                    << dimacs_spec << "). Ignoring." << endl;
        }
        in.skipLine();
        lineNum++;
        break;
      default:
        if (!parse_and_add_clause(in))
        {
          return false;
        }
        break;
    }
  }

  return true;
}

template <class C, class S>
template <class T>
bool DimacsParser<C, S>::parse_DIMACS(T input_stream,
                                      const bool _strict_header,
                                      uint32_t _offset_vars)
{
  debugLibPart = 1;
  strict_header = _strict_header;
  offset_vars = _offset_vars;
  const uint32_t origNumVars = solver->nVars();

  C in(input_stream);
  if (!parse_DIMACS_main(in))
  {
    return false;
  }

  if (verbosity)
  {
    cout << "c -- clauses added: " << norm_clauses_added << endl
         << "c -- vars added " << (solver->nVars() - origNumVars) << endl
         << "c -- forall vars added: " << forall_vars_added << endl
         << "c -- exists vars added: " << exists_vars_added << endl;
  }

  return true;
}

template <class C, class S>
bool DimacsParser<C, S>::parseComments(C& in, const std::string& str)
{
  if (!debugLib.empty() && str == "Solver::new_vars(")
  {
    in.skipWhitespace();
    int n;
    if (!in.parseInt(n, lineNum))
    {
      return false;
    }
    solver->new_vars(n);

    if (verbosity >= 6)
    {
      cout << "c Parsed Solver::new_vars( " << n << " )" << endl;
    }
  }
  else
  {
    if (verbosity >= 6)
    {
      cout << "didn't understand in CNF file comment line:"
           << "'c " << str << "'" << endl;
    }
  }
  in.skipLine();
  lineNum++;
  return true;
}

template <class C, class S>
bool DimacsParser<C, S>::parseForallVars(C& in)
{
  int32_t parsed_lit;
  for (;;)
  {
    if (!in.parseInt(parsed_lit, lineNum))
    {
      return false;
    }
    if (parsed_lit == 0)
    {
      break;
    }
    uint32_t var = std::abs(parsed_lit) - 1;
    solver->add_forall_var(var);
    forall_vars_added++;
  }
  in.skipLine();
  lineNum++;
  return true;
}

template <class C, class S>
bool DimacsParser<C, S>::parseExistsVars(C& in)
{
  int32_t parsed_lit;
  for (;;)
  {
    if (!in.parseInt(parsed_lit, lineNum))
    {
      return false;
    }
    if (parsed_lit == 0)
    {
      break;
    }
    uint32_t var = std::abs(parsed_lit) - 1;
    solver->add_exists_var(var);
    exists_vars_added++;
  }
  in.skipLine();
  lineNum++;
  return true;
}

}  // namespace SkolemFC

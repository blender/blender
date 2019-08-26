/* -*- mode: C++; indent-tabs-mode: nil; -*-
 *
 * This file is a part of LEMON, a generic C++ optimization library.
 *
 * Copyright (C) 2003-2013
 * Egervary Jeno Kombinatorikus Optimalizalasi Kutatocsoport
 * (Egervary Research Group on Combinatorial Optimization, EGRES).
 *
 * Permission to use, modify and distribute this software is granted
 * provided that this copyright notice appears in all copies. For
 * precise terms see the accompanying LICENSE file.
 *
 * This software is provided "AS IS" with no warranty of any kind,
 * express or implied, and with no claim as to its suitability for any
 * purpose.
 *
 */

///\file
///\brief Implementation of the LEMON GLPK LP and MIP solver interface.

#include <lemon/glpk.h>
#include <glpk.h>

#include <lemon/assert.h>

namespace lemon {

  // GlpkBase members

  GlpkBase::GlpkBase() : LpBase() {
    lp = glp_create_prob();
    glp_create_index(lp);
    messageLevel(MESSAGE_NOTHING);
  }

  GlpkBase::GlpkBase(const GlpkBase &other) : LpBase() {
    lp = glp_create_prob();
    glp_copy_prob(lp, other.lp, GLP_ON);
    glp_create_index(lp);
    rows = other.rows;
    cols = other.cols;
    messageLevel(MESSAGE_NOTHING);
  }

  GlpkBase::~GlpkBase() {
    glp_delete_prob(lp);
  }

  int GlpkBase::_addCol() {
    int i = glp_add_cols(lp, 1);
    glp_set_col_bnds(lp, i, GLP_FR, 0.0, 0.0);
    return i;
  }

  int GlpkBase::_addRow() {
    int i = glp_add_rows(lp, 1);
    glp_set_row_bnds(lp, i, GLP_FR, 0.0, 0.0);
    return i;
  }

  int GlpkBase::_addRow(Value lo, ExprIterator b,
                        ExprIterator e, Value up) {
    int i = glp_add_rows(lp, 1);

    if (lo == -INF) {
      if (up == INF) {
        glp_set_row_bnds(lp, i, GLP_FR, lo, up);
      } else {
        glp_set_row_bnds(lp, i, GLP_UP, lo, up);
      }
    } else {
      if (up == INF) {
        glp_set_row_bnds(lp, i, GLP_LO, lo, up);
      } else if (lo != up) {
        glp_set_row_bnds(lp, i, GLP_DB, lo, up);
      } else {
        glp_set_row_bnds(lp, i, GLP_FX, lo, up);
      }
    }

    std::vector<int> indexes;
    std::vector<Value> values;

    indexes.push_back(0);
    values.push_back(0);

    for(ExprIterator it = b; it != e; ++it) {
      indexes.push_back(it->first);
      values.push_back(it->second);
    }

    glp_set_mat_row(lp, i, values.size() - 1,
                    &indexes.front(), &values.front());
    return i;
  }

  void GlpkBase::_eraseCol(int i) {
    int ca[2];
    ca[1] = i;
    glp_del_cols(lp, 1, ca);
  }

  void GlpkBase::_eraseRow(int i) {
    int ra[2];
    ra[1] = i;
    glp_del_rows(lp, 1, ra);
  }

  void GlpkBase::_eraseColId(int i) {
    cols.eraseIndex(i);
    cols.shiftIndices(i);
  }

  void GlpkBase::_eraseRowId(int i) {
    rows.eraseIndex(i);
    rows.shiftIndices(i);
  }

  void GlpkBase::_getColName(int c, std::string& name) const {
    const char *str = glp_get_col_name(lp, c);
    if (str) name = str;
    else name.clear();
  }

  void GlpkBase::_setColName(int c, const std::string & name) {
    glp_set_col_name(lp, c, const_cast<char*>(name.c_str()));

  }

  int GlpkBase::_colByName(const std::string& name) const {
    int k = glp_find_col(lp, const_cast<char*>(name.c_str()));
    return k > 0 ? k : -1;
  }

  void GlpkBase::_getRowName(int r, std::string& name) const {
    const char *str = glp_get_row_name(lp, r);
    if (str) name = str;
    else name.clear();
  }

  void GlpkBase::_setRowName(int r, const std::string & name) {
    glp_set_row_name(lp, r, const_cast<char*>(name.c_str()));

  }

  int GlpkBase::_rowByName(const std::string& name) const {
    int k = glp_find_row(lp, const_cast<char*>(name.c_str()));
    return k > 0 ? k : -1;
  }

  void GlpkBase::_setRowCoeffs(int i, ExprIterator b, ExprIterator e) {
    std::vector<int> indexes;
    std::vector<Value> values;

    indexes.push_back(0);
    values.push_back(0);

    for(ExprIterator it = b; it != e; ++it) {
      indexes.push_back(it->first);
      values.push_back(it->second);
    }

    glp_set_mat_row(lp, i, values.size() - 1,
                    &indexes.front(), &values.front());
  }

  void GlpkBase::_getRowCoeffs(int ix, InsertIterator b) const {
    int length = glp_get_mat_row(lp, ix, 0, 0);

    std::vector<int> indexes(length + 1);
    std::vector<Value> values(length + 1);

    glp_get_mat_row(lp, ix, &indexes.front(), &values.front());

    for (int i = 1; i <= length; ++i) {
      *b = std::make_pair(indexes[i], values[i]);
      ++b;
    }
  }

  void GlpkBase::_setColCoeffs(int ix, ExprIterator b,
                                     ExprIterator e) {

    std::vector<int> indexes;
    std::vector<Value> values;

    indexes.push_back(0);
    values.push_back(0);

    for(ExprIterator it = b; it != e; ++it) {
      indexes.push_back(it->first);
      values.push_back(it->second);
    }

    glp_set_mat_col(lp, ix, values.size() - 1,
                    &indexes.front(), &values.front());
  }

  void GlpkBase::_getColCoeffs(int ix, InsertIterator b) const {
    int length = glp_get_mat_col(lp, ix, 0, 0);

    std::vector<int> indexes(length + 1);
    std::vector<Value> values(length + 1);

    glp_get_mat_col(lp, ix, &indexes.front(), &values.front());

    for (int i = 1; i  <= length; ++i) {
      *b = std::make_pair(indexes[i], values[i]);
      ++b;
    }
  }

  void GlpkBase::_setCoeff(int ix, int jx, Value value) {

    if (glp_get_num_cols(lp) < glp_get_num_rows(lp)) {

      int length = glp_get_mat_row(lp, ix, 0, 0);

      std::vector<int> indexes(length + 2);
      std::vector<Value> values(length + 2);

      glp_get_mat_row(lp, ix, &indexes.front(), &values.front());

      //The following code does not suppose that the elements of the
      //array indexes are sorted
      bool found = false;
      for (int i = 1; i  <= length; ++i) {
        if (indexes[i] == jx) {
          found = true;
          values[i] = value;
          break;
        }
      }
      if (!found) {
        ++length;
        indexes[length] = jx;
        values[length] = value;
      }

      glp_set_mat_row(lp, ix, length, &indexes.front(), &values.front());

    } else {

      int length = glp_get_mat_col(lp, jx, 0, 0);

      std::vector<int> indexes(length + 2);
      std::vector<Value> values(length + 2);

      glp_get_mat_col(lp, jx, &indexes.front(), &values.front());

      //The following code does not suppose that the elements of the
      //array indexes are sorted
      bool found = false;
      for (int i = 1; i <= length; ++i) {
        if (indexes[i] == ix) {
          found = true;
          values[i] = value;
          break;
        }
      }
      if (!found) {
        ++length;
        indexes[length] = ix;
        values[length] = value;
      }

      glp_set_mat_col(lp, jx, length, &indexes.front(), &values.front());
    }

  }

  GlpkBase::Value GlpkBase::_getCoeff(int ix, int jx) const {

    int length = glp_get_mat_row(lp, ix, 0, 0);

    std::vector<int> indexes(length + 1);
    std::vector<Value> values(length + 1);

    glp_get_mat_row(lp, ix, &indexes.front(), &values.front());

    for (int i = 1; i  <= length; ++i) {
      if (indexes[i] == jx) {
        return values[i];
      }
    }

    return 0;
  }

  void GlpkBase::_setColLowerBound(int i, Value lo) {
    LEMON_ASSERT(lo != INF, "Invalid bound");

    int b = glp_get_col_type(lp, i);
    double up = glp_get_col_ub(lp, i);
    if (lo == -INF) {
      switch (b) {
      case GLP_FR:
      case GLP_LO:
        glp_set_col_bnds(lp, i, GLP_FR, lo, up);
        break;
      case GLP_UP:
        break;
      case GLP_DB:
      case GLP_FX:
        glp_set_col_bnds(lp, i, GLP_UP, lo, up);
        break;
      default:
        break;
      }
    } else {
      switch (b) {
      case GLP_FR:
      case GLP_LO:
        glp_set_col_bnds(lp, i, GLP_LO, lo, up);
        break;
      case GLP_UP:
      case GLP_DB:
      case GLP_FX:
        if (lo == up)
          glp_set_col_bnds(lp, i, GLP_FX, lo, up);
        else
          glp_set_col_bnds(lp, i, GLP_DB, lo, up);
        break;
      default:
        break;
      }
    }
  }

  GlpkBase::Value GlpkBase::_getColLowerBound(int i) const {
    int b = glp_get_col_type(lp, i);
    switch (b) {
    case GLP_LO:
    case GLP_DB:
    case GLP_FX:
      return glp_get_col_lb(lp, i);
    default:
      return -INF;
    }
  }

  void GlpkBase::_setColUpperBound(int i, Value up) {
    LEMON_ASSERT(up != -INF, "Invalid bound");

    int b = glp_get_col_type(lp, i);
    double lo = glp_get_col_lb(lp, i);
    if (up == INF) {
      switch (b) {
      case GLP_FR:
      case GLP_LO:
        break;
      case GLP_UP:
        glp_set_col_bnds(lp, i, GLP_FR, lo, up);
        break;
      case GLP_DB:
      case GLP_FX:
        glp_set_col_bnds(lp, i, GLP_LO, lo, up);
        break;
      default:
        break;
      }
    } else {
      switch (b) {
      case GLP_FR:
        glp_set_col_bnds(lp, i, GLP_UP, lo, up);
        break;
      case GLP_UP:
        glp_set_col_bnds(lp, i, GLP_UP, lo, up);
        break;
      case GLP_LO:
      case GLP_DB:
      case GLP_FX:
        if (lo == up)
          glp_set_col_bnds(lp, i, GLP_FX, lo, up);
        else
          glp_set_col_bnds(lp, i, GLP_DB, lo, up);
        break;
      default:
        break;
      }
    }

  }

  GlpkBase::Value GlpkBase::_getColUpperBound(int i) const {
    int b = glp_get_col_type(lp, i);
      switch (b) {
      case GLP_UP:
      case GLP_DB:
      case GLP_FX:
        return glp_get_col_ub(lp, i);
      default:
        return INF;
      }
  }

  void GlpkBase::_setRowLowerBound(int i, Value lo) {
    LEMON_ASSERT(lo != INF, "Invalid bound");

    int b = glp_get_row_type(lp, i);
    double up = glp_get_row_ub(lp, i);
    if (lo == -INF) {
      switch (b) {
      case GLP_FR:
      case GLP_LO:
        glp_set_row_bnds(lp, i, GLP_FR, lo, up);
        break;
      case GLP_UP:
        break;
      case GLP_DB:
      case GLP_FX:
        glp_set_row_bnds(lp, i, GLP_UP, lo, up);
        break;
      default:
        break;
      }
    } else {
      switch (b) {
      case GLP_FR:
      case GLP_LO:
        glp_set_row_bnds(lp, i, GLP_LO, lo, up);
        break;
      case GLP_UP:
      case GLP_DB:
      case GLP_FX:
        if (lo == up)
          glp_set_row_bnds(lp, i, GLP_FX, lo, up);
        else
          glp_set_row_bnds(lp, i, GLP_DB, lo, up);
        break;
      default:
        break;
      }
    }

  }

  GlpkBase::Value GlpkBase::_getRowLowerBound(int i) const {
    int b = glp_get_row_type(lp, i);
    switch (b) {
    case GLP_LO:
    case GLP_DB:
    case GLP_FX:
      return glp_get_row_lb(lp, i);
    default:
      return -INF;
    }
  }

  void GlpkBase::_setRowUpperBound(int i, Value up) {
    LEMON_ASSERT(up != -INF, "Invalid bound");

    int b = glp_get_row_type(lp, i);
    double lo = glp_get_row_lb(lp, i);
    if (up == INF) {
      switch (b) {
      case GLP_FR:
      case GLP_LO:
        break;
      case GLP_UP:
        glp_set_row_bnds(lp, i, GLP_FR, lo, up);
        break;
      case GLP_DB:
      case GLP_FX:
        glp_set_row_bnds(lp, i, GLP_LO, lo, up);
        break;
      default:
        break;
      }
    } else {
      switch (b) {
      case GLP_FR:
        glp_set_row_bnds(lp, i, GLP_UP, lo, up);
        break;
      case GLP_UP:
        glp_set_row_bnds(lp, i, GLP_UP, lo, up);
        break;
      case GLP_LO:
      case GLP_DB:
      case GLP_FX:
        if (lo == up)
          glp_set_row_bnds(lp, i, GLP_FX, lo, up);
        else
          glp_set_row_bnds(lp, i, GLP_DB, lo, up);
        break;
      default:
        break;
      }
    }
  }

  GlpkBase::Value GlpkBase::_getRowUpperBound(int i) const {
    int b = glp_get_row_type(lp, i);
    switch (b) {
    case GLP_UP:
    case GLP_DB:
    case GLP_FX:
      return glp_get_row_ub(lp, i);
    default:
      return INF;
    }
  }

  void GlpkBase::_setObjCoeffs(ExprIterator b, ExprIterator e) {
    for (int i = 1; i <= glp_get_num_cols(lp); ++i) {
      glp_set_obj_coef(lp, i, 0.0);
    }
    for (ExprIterator it = b; it != e; ++it) {
      glp_set_obj_coef(lp, it->first, it->second);
    }
  }

  void GlpkBase::_getObjCoeffs(InsertIterator b) const {
    for (int i = 1; i <= glp_get_num_cols(lp); ++i) {
      Value val = glp_get_obj_coef(lp, i);
      if (val != 0.0) {
        *b = std::make_pair(i, val);
        ++b;
      }
    }
  }

  void GlpkBase::_setObjCoeff(int i, Value obj_coef) {
    //i = 0 means the constant term (shift)
    glp_set_obj_coef(lp, i, obj_coef);
  }

  GlpkBase::Value GlpkBase::_getObjCoeff(int i) const {
    //i = 0 means the constant term (shift)
    return glp_get_obj_coef(lp, i);
  }

  void GlpkBase::_setSense(GlpkBase::Sense sense) {
    switch (sense) {
    case MIN:
      glp_set_obj_dir(lp, GLP_MIN);
      break;
    case MAX:
      glp_set_obj_dir(lp, GLP_MAX);
      break;
    }
  }

  GlpkBase::Sense GlpkBase::_getSense() const {
    switch(glp_get_obj_dir(lp)) {
    case GLP_MIN:
      return MIN;
    case GLP_MAX:
      return MAX;
    default:
      LEMON_ASSERT(false, "Wrong sense");
      return GlpkBase::Sense();
    }
  }

  void GlpkBase::_clear() {
    glp_erase_prob(lp);
  }

  void GlpkBase::freeEnv() {
    glp_free_env();
  }

  void GlpkBase::_messageLevel(MessageLevel level) {
    switch (level) {
    case MESSAGE_NOTHING:
      _message_level = GLP_MSG_OFF;
      break;
    case MESSAGE_ERROR:
      _message_level = GLP_MSG_ERR;
      break;
    case MESSAGE_WARNING:
      _message_level = GLP_MSG_ERR;
      break;
    case MESSAGE_NORMAL:
      _message_level = GLP_MSG_ON;
      break;
    case MESSAGE_VERBOSE:
      _message_level = GLP_MSG_ALL;
      break;
    }
  }

  void GlpkBase::_write(std::string file, std::string format) const
  {
    if(format == "MPS")
      glp_write_mps(lp, GLP_MPS_FILE, 0, file.c_str());
    else if(format == "LP")
      glp_write_lp(lp, 0, file.c_str());
    else throw UnsupportedFormatError(format);
  }

  GlpkBase::FreeEnvHelper GlpkBase::freeEnvHelper;

  // GlpkLp members

  GlpkLp::GlpkLp()
    : LpBase(), LpSolver(), GlpkBase() {
    presolver(false);
  }

  GlpkLp::GlpkLp(const GlpkLp& other)
    : LpBase(other), LpSolver(other), GlpkBase(other) {
    presolver(false);
  }

  GlpkLp* GlpkLp::newSolver() const { return new GlpkLp; }
  GlpkLp* GlpkLp::cloneSolver() const { return new GlpkLp(*this); }

  const char* GlpkLp::_solverName() const { return "GlpkLp"; }

  void GlpkLp::_clear_temporals() {
    _primal_ray.clear();
    _dual_ray.clear();
  }

  GlpkLp::SolveExitStatus GlpkLp::_solve() {
    return solvePrimal();
  }

  GlpkLp::SolveExitStatus GlpkLp::solvePrimal() {
    _clear_temporals();

    glp_smcp smcp;
    glp_init_smcp(&smcp);

    smcp.msg_lev = _message_level;
    smcp.presolve = _presolve;

    // If the basis is not valid we get an error return value.
    // In this case we can try to create a new basis.
    switch (glp_simplex(lp, &smcp)) {
    case 0:
      break;
    case GLP_EBADB:
    case GLP_ESING:
    case GLP_ECOND:
      glp_term_out(false);
      glp_adv_basis(lp, 0);
      glp_term_out(true);
      if (glp_simplex(lp, &smcp) != 0) return UNSOLVED;
      break;
    default:
      return UNSOLVED;
    }

    return SOLVED;
  }

  GlpkLp::SolveExitStatus GlpkLp::solveDual() {
    _clear_temporals();

    glp_smcp smcp;
    glp_init_smcp(&smcp);

    smcp.msg_lev = _message_level;
    smcp.meth = GLP_DUAL;
    smcp.presolve = _presolve;

    // If the basis is not valid we get an error return value.
    // In this case we can try to create a new basis.
    switch (glp_simplex(lp, &smcp)) {
    case 0:
      break;
    case GLP_EBADB:
    case GLP_ESING:
    case GLP_ECOND:
      glp_term_out(false);
      glp_adv_basis(lp, 0);
      glp_term_out(true);
      if (glp_simplex(lp, &smcp) != 0) return UNSOLVED;
      break;
    default:
      return UNSOLVED;
    }
    return SOLVED;
  }

  GlpkLp::Value GlpkLp::_getPrimal(int i) const {
    return glp_get_col_prim(lp, i);
  }

  GlpkLp::Value GlpkLp::_getDual(int i) const {
    return glp_get_row_dual(lp, i);
  }

  GlpkLp::Value GlpkLp::_getPrimalValue() const {
    return glp_get_obj_val(lp);
  }

  GlpkLp::VarStatus GlpkLp::_getColStatus(int i) const {
    switch (glp_get_col_stat(lp, i)) {
    case GLP_BS:
      return BASIC;
    case GLP_UP:
      return UPPER;
    case GLP_LO:
      return LOWER;
    case GLP_NF:
      return FREE;
    case GLP_NS:
      return FIXED;
    default:
      LEMON_ASSERT(false, "Wrong column status");
      return GlpkLp::VarStatus();
    }
  }

  GlpkLp::VarStatus GlpkLp::_getRowStatus(int i) const {
    switch (glp_get_row_stat(lp, i)) {
    case GLP_BS:
      return BASIC;
    case GLP_UP:
      return UPPER;
    case GLP_LO:
      return LOWER;
    case GLP_NF:
      return FREE;
    case GLP_NS:
      return FIXED;
    default:
      LEMON_ASSERT(false, "Wrong row status");
      return GlpkLp::VarStatus();
    }
  }

  GlpkLp::Value GlpkLp::_getPrimalRay(int i) const {
    if (_primal_ray.empty()) {
      int row_num = glp_get_num_rows(lp);
      int col_num = glp_get_num_cols(lp);

      _primal_ray.resize(col_num + 1, 0.0);

      int index = glp_get_unbnd_ray(lp);
      if (index != 0) {
        // The primal ray is found in primal simplex second phase
        LEMON_ASSERT((index <= row_num ? glp_get_row_stat(lp, index) :
                      glp_get_col_stat(lp, index - row_num)) != GLP_BS,
                     "Wrong primal ray");

        bool negate = glp_get_obj_dir(lp) == GLP_MAX;

        if (index > row_num) {
          _primal_ray[index - row_num] = 1.0;
          if (glp_get_col_dual(lp, index - row_num) > 0) {
            negate = !negate;
          }
        } else {
          if (glp_get_row_dual(lp, index) > 0) {
            negate = !negate;
          }
        }

        std::vector<int> ray_indexes(row_num + 1);
        std::vector<Value> ray_values(row_num + 1);
        int ray_length = glp_eval_tab_col(lp, index, &ray_indexes.front(),
                                          &ray_values.front());

        for (int i = 1; i <= ray_length; ++i) {
          if (ray_indexes[i] > row_num) {
            _primal_ray[ray_indexes[i] - row_num] = ray_values[i];
          }
        }

        if (negate) {
          for (int i = 1; i <= col_num; ++i) {
            _primal_ray[i] = - _primal_ray[i];
          }
        }
      } else {
        for (int i = 1; i <= col_num; ++i) {
          _primal_ray[i] = glp_get_col_prim(lp, i);
        }
      }
    }
    return _primal_ray[i];
  }

  GlpkLp::Value GlpkLp::_getDualRay(int i) const {
    if (_dual_ray.empty()) {
      int row_num = glp_get_num_rows(lp);

      _dual_ray.resize(row_num + 1, 0.0);

      int index = glp_get_unbnd_ray(lp);
      if (index != 0) {
        // The dual ray is found in dual simplex second phase
        LEMON_ASSERT((index <= row_num ? glp_get_row_stat(lp, index) :
                      glp_get_col_stat(lp, index - row_num)) == GLP_BS,

                     "Wrong dual ray");

        int idx;
        bool negate = false;

        if (index > row_num) {
          idx = glp_get_col_bind(lp, index - row_num);
          if (glp_get_col_prim(lp, index - row_num) >
              glp_get_col_ub(lp, index - row_num)) {
            negate = true;
          }
        } else {
          idx = glp_get_row_bind(lp, index);
          if (glp_get_row_prim(lp, index) > glp_get_row_ub(lp, index)) {
            negate = true;
          }
        }

        _dual_ray[idx] = negate ?  - 1.0 : 1.0;

        glp_btran(lp, &_dual_ray.front());
      } else {
        double eps = 1e-7;
        // The dual ray is found in primal simplex first phase
        // We assume that the glpk minimizes the slack to get feasible solution
        for (int i = 1; i <= row_num; ++i) {
          int index = glp_get_bhead(lp, i);
          if (index <= row_num) {
            double res = glp_get_row_prim(lp, index);
            if (res > glp_get_row_ub(lp, index) + eps) {
              _dual_ray[i] = -1;
            } else if (res < glp_get_row_lb(lp, index) - eps) {
              _dual_ray[i] = 1;
            } else {
              _dual_ray[i] = 0;
            }
            _dual_ray[i] *= glp_get_rii(lp, index);
          } else {
            double res = glp_get_col_prim(lp, index - row_num);
            if (res > glp_get_col_ub(lp, index - row_num) + eps) {
              _dual_ray[i] = -1;
            } else if (res < glp_get_col_lb(lp, index - row_num) - eps) {
              _dual_ray[i] = 1;
            } else {
              _dual_ray[i] = 0;
            }
            _dual_ray[i] /= glp_get_sjj(lp, index - row_num);
          }
        }

        glp_btran(lp, &_dual_ray.front());

        for (int i = 1; i <= row_num; ++i) {
          _dual_ray[i] /= glp_get_rii(lp, i);
        }
      }
    }
    return _dual_ray[i];
  }

  GlpkLp::ProblemType GlpkLp::_getPrimalType() const {
    if (glp_get_status(lp) == GLP_OPT)
      return OPTIMAL;
    switch (glp_get_prim_stat(lp)) {
    case GLP_UNDEF:
      return UNDEFINED;
    case GLP_FEAS:
    case GLP_INFEAS:
      if (glp_get_dual_stat(lp) == GLP_NOFEAS) {
        return UNBOUNDED;
      } else {
        return UNDEFINED;
      }
    case GLP_NOFEAS:
      return INFEASIBLE;
    default:
      LEMON_ASSERT(false, "Wrong primal type");
      return  GlpkLp::ProblemType();
    }
  }

  GlpkLp::ProblemType GlpkLp::_getDualType() const {
    if (glp_get_status(lp) == GLP_OPT)
      return OPTIMAL;
    switch (glp_get_dual_stat(lp)) {
    case GLP_UNDEF:
      return UNDEFINED;
    case GLP_FEAS:
    case GLP_INFEAS:
      if (glp_get_prim_stat(lp) == GLP_NOFEAS) {
        return UNBOUNDED;
      } else {
        return UNDEFINED;
      }
    case GLP_NOFEAS:
      return INFEASIBLE;
    default:
      LEMON_ASSERT(false, "Wrong primal type");
      return  GlpkLp::ProblemType();
    }
  }

  void GlpkLp::presolver(bool presolve) {
    _presolve = presolve;
  }

  // GlpkMip members

  GlpkMip::GlpkMip()
    : LpBase(), MipSolver(), GlpkBase() {
  }

  GlpkMip::GlpkMip(const GlpkMip& other)
    : LpBase(), MipSolver(), GlpkBase(other) {
  }

  void GlpkMip::_setColType(int i, GlpkMip::ColTypes col_type) {
    switch (col_type) {
    case INTEGER:
      glp_set_col_kind(lp, i, GLP_IV);
      break;
    case REAL:
      glp_set_col_kind(lp, i, GLP_CV);
      break;
    }
  }

  GlpkMip::ColTypes GlpkMip::_getColType(int i) const {
    switch (glp_get_col_kind(lp, i)) {
    case GLP_IV:
    case GLP_BV:
      return INTEGER;
    default:
      return REAL;
    }

  }

  GlpkMip::SolveExitStatus GlpkMip::_solve() {
    glp_smcp smcp;
    glp_init_smcp(&smcp);

    smcp.msg_lev = _message_level;
    smcp.meth = GLP_DUAL;

    // If the basis is not valid we get an error return value.
    // In this case we can try to create a new basis.
    switch (glp_simplex(lp, &smcp)) {
    case 0:
      break;
    case GLP_EBADB:
    case GLP_ESING:
    case GLP_ECOND:
      glp_term_out(false);
      glp_adv_basis(lp, 0);
      glp_term_out(true);
      if (glp_simplex(lp, &smcp) != 0) return UNSOLVED;
      break;
    default:
      return UNSOLVED;
    }

    if (glp_get_status(lp) != GLP_OPT) return SOLVED;

    glp_iocp iocp;
    glp_init_iocp(&iocp);

    iocp.msg_lev = _message_level;

    if (glp_intopt(lp, &iocp) != 0) return UNSOLVED;
    return SOLVED;
  }


  GlpkMip::ProblemType GlpkMip::_getType() const {
    switch (glp_get_status(lp)) {
    case GLP_OPT:
      switch (glp_mip_status(lp)) {
      case GLP_UNDEF:
        return UNDEFINED;
      case GLP_NOFEAS:
        return INFEASIBLE;
      case GLP_FEAS:
        return FEASIBLE;
      case GLP_OPT:
        return OPTIMAL;
      default:
        LEMON_ASSERT(false, "Wrong problem type.");
        return GlpkMip::ProblemType();
      }
    case GLP_NOFEAS:
      return INFEASIBLE;
    case GLP_INFEAS:
    case GLP_FEAS:
      if (glp_get_dual_stat(lp) == GLP_NOFEAS) {
        return UNBOUNDED;
      } else {
        return UNDEFINED;
      }
    default:
      LEMON_ASSERT(false, "Wrong problem type.");
      return GlpkMip::ProblemType();
    }
  }

  GlpkMip::Value GlpkMip::_getSol(int i) const {
    return glp_mip_col_val(lp, i);
  }

  GlpkMip::Value GlpkMip::_getSolValue() const {
    return glp_mip_obj_val(lp);
  }

  GlpkMip* GlpkMip::newSolver() const { return new GlpkMip; }
  GlpkMip* GlpkMip::cloneSolver() const {return new GlpkMip(*this); }

  const char* GlpkMip::_solverName() const { return "GlpkMip"; }



} //END OF NAMESPACE LEMON

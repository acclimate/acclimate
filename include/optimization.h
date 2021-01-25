/*
  Copyright (C) 2014-2020 Sven Willner <sven.willner@pik-potsdam.de>
                          Christian Otto <christian.otto@pik-potsdam.de>

  This file is part of Acclimate.

  Acclimate is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as
  published by the Free Software Foundation, either version 3 of
  the License, or (at your option) any later version.

  Acclimate is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with Acclimate.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef OPTIMIZATION_H
#define OPTIMIZATION_H

#include <stdexcept>

#include "nlopt.h"  // IWYU pragma: export
#include "types.h"

namespace acclimate::optimization {

static int get_algorithm(const hashed_string& name) {
    switch (name) {
        case hash("slsqp"):
            return NLOPT_LD_SLSQP;
        case hash("mma"):
            return NLOPT_LD_MMA;
        case hash("ccsaq"):
            return NLOPT_LD_CCSAQ;
        case hash("lbfgs"):
            return NLOPT_LD_LBFGS;
        case hash("tnewton_precond_restart"):
            return NLOPT_LD_TNEWTON_PRECOND_RESTART;
        case hash("tnewton_precond"):
            return NLOPT_LD_TNEWTON_PRECOND;
        case hash("tnewton_restart"):
            return NLOPT_LD_TNEWTON_RESTART;
        case hash("tnewton"):
            return NLOPT_LD_TNEWTON;
        case hash("var1"):
            return NLOPT_LD_VAR1;
        case hash("var2"):
            return NLOPT_LD_VAR2;
        default:
            throw log::error("unknown optimization alorithm '", name, "'");
    }
}

static const char* get_result_description(nlopt_result result, nlopt_opt opt) {
    switch (result) {
        case NLOPT_SUCCESS:
            return "Generic success";
        case NLOPT_STOPVAL_REACHED:
            return "Optimization stopped because stopval was reached";
        case NLOPT_FTOL_REACHED:
            return "Optimization stopped because ftol_rel or ftol_abs was reached";
        case NLOPT_XTOL_REACHED:
            return "Optimization stopped because xtol_rel or xtol_abs was reached";
        case NLOPT_MAXEVAL_REACHED:
            return "Optimization stopped because maxeval was reached";
        case NLOPT_MAXTIME_REACHED:
            return "Optimization stopped because maxtime was reached";
        case NLOPT_FAILURE: {
            const auto err = opt == nullptr ? nullptr : nlopt_get_errmsg(opt);
            return err == nullptr ? "Generic failure" : err;
        }
        case NLOPT_INVALID_ARGS: {
            const auto err = opt == nullptr ? nullptr : nlopt_get_errmsg(opt);
            return err == nullptr ? "Invalid arguments" : err;
        }
        case NLOPT_OUT_OF_MEMORY:
            return "Out of memory";
        case NLOPT_ROUNDOFF_LIMITED:
            return "Roundoff limited";  // "Halted because roundoff errors limited progress. (In this case, the optimization still typically returns a useful
                                        // result.)"
        case NLOPT_FORCED_STOP:
            return "Forced stop";
        default:
            return "Unknown optimization result";
    }
}

class failure : public std::runtime_error {
  public:
    explicit failure(const std::string& s) : std::runtime_error(s) {}
    explicit failure(const char* s) : std::runtime_error(s) {}
};

class Optimization {
  private:
    nlopt_opt opt;
    nlopt_result last_result;
    double optimized_value_m;
    unsigned int dim_m;

    void check(nlopt_result result) {
        if (result < NLOPT_SUCCESS && result != NLOPT_ROUNDOFF_LIMITED) {
            throw failure(get_result_description(result, opt));
        }
    }

  public:
    Optimization(nlopt_algorithm algorithm, unsigned int dim_p) : opt(nlopt_create(algorithm, dim_p)), dim_m(dim_p) {}
    ~Optimization() { nlopt_destroy(opt); }

    // double& xtol(std::size_t i) { return opt->xtol_abs[i]; }
    // double& lower_bounds(std::size_t i) { return opt->lb[i]; }
    // double& upper_bounds(std::size_t i) { return opt->ub[i]; }
    void xtol(const std::vector<double>& v) { check(nlopt_set_xtol_abs(opt, &v[0])); }
    void lower_bounds(const std::vector<double>& v) { check(nlopt_set_lower_bounds(opt, &v[0])); }
    void upper_bounds(const std::vector<double>& v) { check(nlopt_set_upper_bounds(opt, &v[0])); }
    void maxeval(int v) { check(nlopt_set_maxeval(opt, v)); }
    void maxtime(double v) { check(nlopt_set_maxtime(opt, v)); }  // timeout given in sec

    unsigned int dim() const { return dim_m; }
    double optimized_value() const { return optimized_value_m; }
    bool roundoff_limited() const { return last_result == NLOPT_ROUNDOFF_LIMITED; }
    bool stopval_reached() const { return last_result == NLOPT_STOPVAL_REACHED; }
    bool ftol_reached() const { return last_result == NLOPT_FTOL_REACHED; }
    bool xtol_reached() const { return last_result == NLOPT_XTOL_REACHED; }
    bool maxeval_reached() const { return last_result == NLOPT_MAXEVAL_REACHED; }
    bool maxtime_reached() const { return last_result == NLOPT_MAXTIME_REACHED; }

    const char* last_result_description() const { return get_result_description(last_result, opt); }

    bool optimize(std::vector<double>& x) {  // returns true for "generic success" and false otherwise (for "real" errors, an exception is thrown)
        last_result = nlopt_optimize(opt, &x[0], &optimized_value_m);
        check(last_result);
        return last_result == NLOPT_SUCCESS;
    }

    template<class Handler>
    void add_equality_constraint(Handler* handler, double precision = 0) {
        check(nlopt_add_equality_constraint(
            opt, [](unsigned /* n */, const double* x, double* grad, void* data) { return static_cast<Handler*>(data)->equality_constraint(x, grad); }, handler,
            precision));
    }

    template<class Handler>
    void add_max_objective(Handler* handler) {
        check(nlopt_set_max_objective(
            opt, [](unsigned /* n */, const double* x, double* grad, void* data) { return static_cast<Handler*>(data)->max_objective(x, grad); }, handler));
    }
};

}  // namespace acclimate::optimization

#endif

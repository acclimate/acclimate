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

inline int get_algorithm(const hashed_string& name) {
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
        case hash("bobyqa"):
            return NLOPT_LN_BOBYQA;
        case hash("cobyla"):
            return NLOPT_LN_COBYLA;
        case hash("isres"):
            return NLOPT_GN_ISRES;
        case hash("direct"):
            return NLOPT_GN_DIRECT;
        case hash("direct_local"):
            return NLOPT_GN_DIRECT_L;
        case hash("crs"):
            return NLOPT_GN_CRS2_LM;
        case hash("esch"):
            return NLOPT_GN_ESCH;
        case hash("mlsl"):
            return NLOPT_G_MLSL;
        case hash("mlsl_low_discrepancy"):
            return NLOPT_G_MLSL_LDS;
        case hash("stogo"):
            return NLOPT_GD_STOGO;
        case hash("stogo_rand"):
            return NLOPT_GD_STOGO_RAND;
        case hash("augmented_lagrangian"):
            return NLOPT_AUGLAG;

        default:
            throw log::error("unknown optimization algorithm '", name, "'");
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
    nlopt_result last_result = NLOPT_SUCCESS;
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

    void set_local_algorithm(nlopt_opt local_algorithm) { nlopt_set_local_optimizer(opt, local_algorithm); }

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
    void add_inequality_constraint(Handler* handler, double precision = 0) {
        check(nlopt_add_inequality_constraint(
            opt, [](unsigned /* n */, const double* x, double* grad, void* data) { return static_cast<Handler*>(data)->inequality_constraint(x, grad); },
            handler, precision));
    }

    template<class Handler>
    void add_max_objective(Handler* handler) {
        check(nlopt_set_max_objective(
            opt, [](unsigned /* n */, const double* x, double* grad, void* data) { return static_cast<Handler*>(data)->max_objective(x, grad); }, handler));
    }

    nlopt_opt get_optimizer() { return opt; }
};
template<typename Func>
void check_gradient(double* x, std::vector<double> grad, Func&& objective_function) {
    double tolerance = 0.00001;
    double difference = 0.001;  // TODO: make parameters
    int dimension = std::size(grad);
    std::vector<double> x_values(dimension, x[0]);
    std::vector<double> shifted_values(dimension, grad[0]);
    for (int dim = 0; dim < dimension; dim++) {
        x_values[dim] = x[dim];
    }
    shifted_values = x_values;
    for (int dim = 0; dim < dimension; dim++) {
        shifted_values[dim] += difference;

        double finite_difference_approximation = (objective_function(shifted_values) - objective_function(x_values)) / difference;
        if (finite_difference_approximation - grad[dim] > tolerance) {
            log::warning("gradient not matching finite difference approximation.");
        }
    }
}

}  // namespace acclimate::optimization

#endif

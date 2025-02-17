// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

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
    nlopt_opt opt_;
    nlopt_result last_result_ = NLOPT_SUCCESS;
    double optimized_value_;
    unsigned int dim_;

    void check(nlopt_result result) {
        if (result < NLOPT_SUCCESS && result != NLOPT_ROUNDOFF_LIMITED) {
            throw failure(get_result_description(result, opt_));
        }
    }

  public:
    Optimization(nlopt_algorithm algorithm, unsigned int dim) : opt_(nlopt_create(algorithm, dim)), dim_(dim) {}
    ~Optimization() { nlopt_destroy(opt_); }

    // double& xtol(std::size_t i) { return opt_->xtol_abs[i]; }
    // double& lower_bounds(std::size_t i) { return opt_->lb[i]; }
    // double& upper_bounds(std::size_t i) { return opt_->ub[i]; }
    void xtol(const std::vector<double>& v) { check(nlopt_set_xtol_abs(opt_, &v[0])); }
    void lower_bounds(const std::vector<double>& v) { check(nlopt_set_lower_bounds(opt_, &v[0])); }
    void upper_bounds(const std::vector<double>& v) { check(nlopt_set_upper_bounds(opt_, &v[0])); }
    void maxeval(int v) { check(nlopt_set_maxeval(opt_, v)); }
    void maxtime(double v) { check(nlopt_set_maxtime(opt_, v)); }  // timeout given in sec

    void set_local_algorithm(nlopt_opt local_algorithm) { nlopt_set_local_optimizer(opt_, local_algorithm); }

    unsigned int dim() const { return dim_; }
    double optimized_value() const { return optimized_value_; }
    bool roundoff_limited() const { return last_result_ == NLOPT_ROUNDOFF_LIMITED; }
    bool stopval_reached() const { return last_result_ == NLOPT_STOPVAL_REACHED; }
    bool ftol_reached() const { return last_result_ == NLOPT_FTOL_REACHED; }
    bool xtol_reached() const { return last_result_ == NLOPT_XTOL_REACHED; }
    bool maxeval_reached() const { return last_result_ == NLOPT_MAXEVAL_REACHED; }
    bool maxtime_reached() const { return last_result_ == NLOPT_MAXTIME_REACHED; }

    void reset_last_result() { last_result_ = NLOPT_SUCCESS; }

    const char* last_result_description() const { return get_result_description(last_result_, opt_); }

    bool optimize(std::vector<double>& x) {  // returns true for "generic success" and false otherwise (for "real" errors, an exception is thrown)
        last_result_ = nlopt_optimize(opt_, &x[0], &optimized_value_);
        check(last_result_);
        return last_result_ == NLOPT_SUCCESS;
    }

    template<class Handler>
    void add_equality_constraint(Handler* handler, double precision = 0) {
        check(nlopt_add_equality_constraint(
            opt_, [](unsigned /* n */, const double* x, double* grad, void* data) { return static_cast<Handler*>(data)->equality_constraint(x, grad); },
            handler, precision));
    }
    template<class Handler>
    void add_inequality_constraint(Handler* handler, double precision = 0) {
        check(nlopt_add_inequality_constraint(
            opt_, [](unsigned /* n */, const double* x, double* grad, void* data) { return static_cast<Handler*>(data)->inequality_constraint(x, grad); },
            handler, precision));
    }

    template<class Handler>
    void add_max_objective(Handler* handler) {
        check(nlopt_set_max_objective(
            opt_, [](unsigned /* n */, const double* x, double* grad, void* data) { return static_cast<Handler*>(data)->max_objective(x, grad); }, handler));
    }

    nlopt_opt get_optimizer() { return opt_; }
};
}  // namespace acclimate::optimization

#endif

// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "model/TransportChainLink.h"

#include "acclimate.h"
#include "model/BusinessConnection.h"
#include "model/EconomicAgent.h"
#include "model/GeoEntity.h"
#include "model/PurchasingManager.h"
#include "model/SalesManager.h"
#include "model/Storage.h"

namespace acclimate {

TransportChainLink::TransportChainLink(BusinessConnection* business_connection_p,
                                       const TransportDelay& transport_delay,
                                       const Flow& baseline_flow,
                                       GeoEntity* geo_entity_p)
    : baseline_transport_delay(transport_delay),
      business_connection(business_connection_p),
      geo_entity_(geo_entity_p),
      overflow_(0.0),
      baseline_flow_quantity_(baseline_flow.get_quantity()),
      transport_queue_(transport_delay, AnnotatedFlow(baseline_flow, baseline_flow_quantity_)),
      pos_(0),
      forcing_(-1) {
    if (geo_entity_.valid()) {
        geo_entity_->transport_chain_links.add(this);
    }
}

TransportChainLink::~TransportChainLink() {
    if (geo_entity_.valid()) {
        geo_entity_->transport_chain_links.remove(this);
    }
}

auto TransportChainLink::get_passage() const -> FloatType { return forcing_; }

void TransportChainLink::push_flow(const Flow& flow, const FlowQuantity& baseline_flow) {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    outflow_ = Flow(0.0);
    if (!transport_queue_.empty()) {
        auto front_flow = transport_queue_[pos_];
        transport_queue_[pos_] = AnnotatedFlow(flow, baseline_flow);
        pos_ = (pos_ + 1) % transport_queue_.size();

        if (forcing_ < 0) {
            outflow_ = overflow_ + front_flow.current;
        } else {
            outflow_ = std::min(overflow_ + front_flow.current, Flow(forcing_ * front_flow.baseline, front_flow.current.get_price()));
        }
        overflow_ = overflow_ + front_flow.current - outflow_;
        if (next_transport_chain_link_) {
            next_transport_chain_link_->push_flow(outflow_, front_flow.baseline);
        } else {
            business_connection->deliver_flow(outflow_);
        }
    } else {
        if (forcing_ < 0) {
            outflow_ = overflow_ + flow;
        } else {
            outflow_ = std::min(overflow_ + flow, Flow(forcing_ * baseline_flow, flow.get_price()));
        }
        overflow_ = overflow_ + flow - outflow_;
        if (next_transport_chain_link_) {
            next_transport_chain_link_->push_flow(outflow_, baseline_flow);
        } else {
            business_connection->deliver_flow(outflow_);
        }
    }
}

void TransportChainLink::set_forcing(Forcing forcing) {
    debug::assertstep(this, IterationStep::SCENARIO);
    forcing_ = forcing;
}

auto TransportChainLink::get_total_flow() const -> Flow {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return overflow_
           + std::accumulate(std::begin(transport_queue_), std::end(transport_queue_), Flow(0.0), [](const Flow& f, const auto& q) { return f + q.current; });
}

auto TransportChainLink::get_disequilibrium() const -> Flow {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    Flow res = Flow(0.0);
    for (const auto& f : transport_queue_) {
        res.add_possibly_negative(absdiff(f.current, f.baseline));
    }
    return res;
}

auto TransportChainLink::get_stddeviation() const -> FloatType {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return std::accumulate(std::begin(transport_queue_), std::end(transport_queue_), 0.0, [](FloatType v, const auto& q) {
        return v + to_float((absdiff(q.current, q.baseline)).get_quantity()) * to_float((absdiff(q.current, q.baseline)).get_quantity());
    });
}

auto TransportChainLink::get_flow_deficit() const -> FlowQuantity {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return round(std::accumulate(std::begin(transport_queue_), std::end(transport_queue_), FlowQuantity(0.0),
                                 [](FlowQuantity v, const auto& q) { return std::move(v) + round(q.baseline - q.current.get_quantity()); })
                 - overflow_.get_quantity());
}

auto TransportChainLink::model() const -> const Model* { return business_connection->model(); }

auto TransportChainLink::name() const -> std::string {
    return (business_connection->seller.valid() ? business_connection->seller->name() : "INVALID") + "-" + std::to_string(business_connection->get_id(this))
           + "->" + (business_connection->buyer.valid() ? business_connection->buyer->storage->economic_agent->name() : "INVALID");
}

}  // namespace acclimate

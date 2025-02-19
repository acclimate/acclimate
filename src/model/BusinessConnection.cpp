// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "model/BusinessConnection.h"

#include "acclimate.h"
#include "model/EconomicAgent.h"
#include "model/Firm.h"
#include "model/GeoEntity.h"
#include "model/GeoRoute.h"
#include "model/Model.h"
#include "model/PurchasingManager.h"
#include "model/Region.h"
#include "model/SalesManager.h"
#include "model/Sector.h"
#include "model/Storage.h"
#include "model/TransportChainLink.h"

namespace acclimate {

BusinessConnection::BusinessConnection(PurchasingManager* buyer_, SalesManager* seller_, const Flow& baseline_flow)
    : buyer(buyer_),
      baseline_flow_(baseline_flow),
      last_delivery_(baseline_flow, baseline_flow.get_quantity()),
      last_demand_request_(baseline_flow),
      seller(seller_),
      last_shipment_(baseline_flow),
      time_(seller_->model()->time()) {
    if (seller->firm->sector->transport_type == Sector::transport_type_t::IMMEDIATE || buyer->storage->economic_agent->region == seller->firm->region) {
        first_transport_link_.reset(new TransportChainLink(this, 0, baseline_flow, nullptr));
    } else {
        auto& route = seller->firm->region->find_path_to(buyer->storage->economic_agent->region, seller->firm->sector->transport_type);
        assert(route.path.size() > 0);
        TransportChainLink* link = nullptr;
        for (std::size_t i = 0; i < route.path.size(); ++i) {
            auto* p = route.path[i];
            auto* new_link = new TransportChainLink(this, p->delay, baseline_flow, p);
            if (i == 0) {
                first_transport_link_.reset(new_link);
            } else {
                link->next_transport_chain_link_.reset(new_link);
            }
            link = new_link;
        }
    }
}

BusinessConnection::~BusinessConnection() = default;  // needed to use forward declares for std::unique_ptr

auto BusinessConnection::get_minimum_passage() const -> FloatType {
    TransportChainLink* link = first_transport_link_.get();
    FloatType minimum_passage = 1.0;
    while (link != nullptr) {
        FloatType const link_passage = link->get_passage();
        if (link_passage >= 0.0 && link_passage < minimum_passage) {
            minimum_passage = link_passage;
        }
        link = link->next_transport_chain_link_.get();
    }
    if (minimum_passage > 1.0 || minimum_passage < 0.0) {
        minimum_passage = 1.0;
    }
    return minimum_passage;
}

auto BusinessConnection::get_domestic() const -> bool { return (buyer->storage->economic_agent->region == seller->firm->region); }

void BusinessConnection::push_flow(const Flow& flow) {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    last_shipment_ = round(flow);
    first_transport_link_->push_flow(AnnotatedFlow(last_shipment_, baseline_flow_.get_quantity()));
    if (!get_domestic()) {
        seller->firm->region->add_export(last_shipment_);
    }
}

auto BusinessConnection::get_transport_delay() const -> TransportDelay {
    TransportChainLink* link = first_transport_link_.get();
    TransportDelay res = 0;
    while (link != nullptr) {
        res += link->transport_delay();
        link = link->next_transport_chain_link_.get();
    }
    return res;
}

void BusinessConnection::deliver_flow(const AnnotatedFlow& flow) {
    debug::assertstep(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    buyer->storage->push_flow(flow);
    last_delivery_ = flow;
    if (!get_domestic()) {
        buyer->storage->economic_agent->region->add_import(flow.current);
    }
}

auto BusinessConnection::get_id(const TransportChainLink* transport_chain_link) const -> std::size_t {
    std::size_t id = 0;
    if (first_transport_link_) {
        const TransportChainLink* link = first_transport_link_.get();
        while (link->next_transport_chain_link_ && transport_chain_link != link->next_transport_chain_link_.get()) {
            link = link->next_transport_chain_link_.get();
            id++;
        }
    }
    return id;
}

void BusinessConnection::send_demand_request(const Demand& demand_request) {
    debug::assertstep(this, IterationStep::PURCHASE);
    last_demand_request_ = round(demand_request);
    seller->add_demand_request(last_demand_request_);
}

auto BusinessConnection::get_flow_mean() const -> Flow {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    TransportChainLink* link = first_transport_link_.get();
    Flow res = last_delivery_.current;
    TransportDelay delay = 0;
    while (link != nullptr) {
        res += link->get_total_flow();
        delay += link->transport_delay();
        link = link->next_transport_chain_link_.get();
    }
    return round(res / static_cast<Ratio>(delay));
}

auto BusinessConnection::get_flow_deficit() const -> FlowQuantity {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    TransportChainLink* link = first_transport_link_.get();
    FlowQuantity res = last_delivery_.get_deficit();
    while (link != nullptr) {
        res += link->get_flow_deficit();
        link = link->next_transport_chain_link_.get();
    }
    return round(res);
}

auto BusinessConnection::get_total_flow() const -> Flow {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    return round(get_transport_flow() + last_delivery_.current);
}

auto BusinessConnection::get_transport_flow() const -> Flow {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    TransportChainLink* link = first_transport_link_.get();
    Flow res = Flow(0.0);
    while (link != nullptr) {
        res += link->get_total_flow();
        link = link->next_transport_chain_link_.get();
    }
    return round(res);
}

auto BusinessConnection::get_disequilibrium() const -> Flow {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    TransportChainLink* link = first_transport_link_.get();
    Flow res = Flow(0.0);
    while (link != nullptr) {
        res.add_possibly_negative(link->get_disequilibrium());
        link = link->next_transport_chain_link_.get();
    }
    return res;
}

auto BusinessConnection::get_stddeviation() const -> FloatType {
    debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
    TransportChainLink* link = first_transport_link_.get();
    FloatType res = 0.0;
    while (link != nullptr) {
        res += link->get_stddeviation();
        link = link->next_transport_chain_link_.get();
    }
    return res;
}

auto BusinessConnection::model() const -> const Model* { return buyer->model(); }

auto BusinessConnection::name() const -> std::string {
    return (seller.valid() ? seller->name() : "INVALID") + "->" + (buyer.valid() ? buyer->storage->economic_agent->name() : "INVALID");
}

auto BusinessConnection::last_shipment(const SalesManager* caller) const -> const Flow& {
    if constexpr (Options::DEBUGGING) {
        if (caller != seller) {
            debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
        }
    }
    return last_shipment_;
}

auto BusinessConnection::last_delivery(const SalesManager* caller) const -> const Flow& {
    if constexpr (Options::DEBUGGING) {
        if (caller != seller) {
            debug::assertstepnot(this, IterationStep::CONSUMPTION_AND_PRODUCTION);
        }
    }
    return last_delivery_.current;
}

auto BusinessConnection::last_demand_request(const PurchasingManager* caller) const -> const Demand& {
    if constexpr (Options::DEBUGGING) {
        if (caller != buyer) {
            debug::assertstepnot(this, IterationStep::PURCHASE);
        }
    }
    return last_demand_request_;
}

void BusinessConnection::iterate_investment() {
    debug::assertstep(this, IterationStep::INVESTMENT);
    baseline_flow_ += (last_shipment_ - baseline_flow_) * model()->delta_t() / buyer->storage->sector->parameters().transport_investment_adjustment_time;
}

}  // namespace acclimate

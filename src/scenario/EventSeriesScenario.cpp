// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "scenario/EventSeriesScenario.h"

#include "acclimate.h"
#include "model/Firm.h"
#include "model/GeoLocation.h"
#include "model/Model.h"
#include "model/Sector.h"
#include "netcdfpp.h"
#include "settingsnode.h"

namespace acclimate {

EventSeriesScenario::EventSeriesScenario(const settings::SettingsNode& settings, settings::SettingsNode scenario_node, Model* model)
    : ExternalScenario(settings, std::move(scenario_node), model) {}

auto EventSeriesScenario::read_forcing_file(const std::string& filename, const std::string& variable_name) -> ExternalForcing* {
    return new EventForcing(filename, variable_name, model());
}

void EventSeriesScenario::read_forcings() {
    {
        auto* forcing_l = static_cast<EventForcing*>(forcing_.get());  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        for (std::size_t i = 0; i < forcing_l->agents_.size(); ++i) {
            if (forcing_l->agents_[i] != nullptr) {
                if (std::isnan(forcing_l->forcings_[i])) {
                    forcing_l->agents_[i]->set_forcing(1.0);
                } else {
                    forcing_l->agents_[i]->set_forcing(forcing_l->forcings_[i]);
                }
            }
        }
    }
    {
        auto* forcing_l = static_cast<EventForcing*>(forcing_.get());  // NOLINT(cppcoreguidelines-pro-type-static-cast-downcast)
        for (std::size_t i = 0; i < forcing_l->locations_.size(); ++i) {
            if (forcing_l->locations_[i] != nullptr) {
                if (forcing_l->forcings_[i] < 0.0) {
                    forcing_l->locations_[i]->set_forcing_nu(-1.);
                } else {
                    forcing_l->locations_[i]->set_forcing_nu(forcing_l->forcings_[i]);
                }
            }
        }
    }
}

void EventSeriesScenario::EventForcing::read_data() {
    if (!agents_.empty()) {
        variable_->read<Forcing, 3>(forcings_.data(), {time_index_, 0, 0}, {1, sectors_count_, regions_count_});
    } else if (!locations_.empty()) {
        variable_->read<Forcing, 3>(forcings_.data(), {time_index_, 0}, {1, sea_routes_count_});
    }
}

EventSeriesScenario::EventForcing::EventForcing(const std::string& filename, const std::string& variable_name, Model* model)
    : ExternalForcing(filename, variable_name) {
    if (file_.variable("region") && file_.variable("sector")) {
        const auto regions = file_.variable("region").require().get<std::string>();
        const auto sectors = file_.variable("sector").require().get<std::string>();
        regions_count_ = regions.size();
        sectors_count_ = sectors.size();
        agents_.reserve(regions_count_ * sectors_count_);
        forcings_.reserve(regions_count_ * sectors_count_);
        for (const auto& sector_name : sectors) {
            const auto* sector = model->sectors.find(sector_name);
            if (sector == nullptr) {
                log::warning(this, "Sector '", sector_name, "' not found");
            }
            for (const auto& region_name : regions) {
                const auto name = sector_name + ":" + region_name;
                auto* agent = model->economic_agents.find(name);
                if (sector != nullptr && agent == nullptr) {
                    log::warning(this, "Agent '", name, "' not found");
                }
                agents_.push_back(agent);
                forcings_.push_back(1);
            }
        }
    }
    if (file_.variable("sea_route")) {
        const auto sea_routes = file_.variable("sea_route").require().get<std::string>();
        sea_routes_count_ = sea_routes.size();
        locations_.reserve(sea_routes_count_);
        forcings_.reserve(sea_routes_count_);
        for (const auto& sea_route_name : sea_routes) {
            GeoLocation* location = model->other_locations.find(sea_route_name);
            if (location == nullptr) {
                log::warning(this, "Sea object '", sea_route_name, "' not found");
            }
            locations_.push_back(location);
            forcings_.push_back(-1);
        }
    }
}

}  // namespace acclimate

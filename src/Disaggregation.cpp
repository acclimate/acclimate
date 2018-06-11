/*
  Copyright (C) 2014-2017 Sven Willner <sven.willner@pik-potsdam.de>

  This file is part of libmrio.

  libmrio is free software: you can redistribute it and/or modify
  it under the terms of the GNU Affero General Public License as
  published by the Free Software Foundation, either version 3 of
  the License, or (at your option) any later version.

  libmrio is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Affero General Public License for more details.

  You should have received a copy of the GNU Affero General Public License
  along with libmrio.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "Disaggregation.h"
#include <cmath>
#include <fstream>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include "MRIOTable.h"
#include "csv-parser.h"
#include "settingsnode.h"

namespace mrio {

template<typename T, typename I>
Disaggregation<T, I>::Disaggregation(const mrio::Table<T, I>* basetable_p) : basetable(basetable_p) {
    table.reset(new Table<T, I>(*basetable));
}

template<typename T, typename I>
void Disaggregation<T, I>::initialize(const settings::SettingsNode& settings) {
    for (const auto& d : settings.as_sequence()) {
        const std::string& type = d["type"].as<std::string>();
        const std::string& id = d["id"].as<std::string>();
        std::vector<std::string> subs;
        for (const auto& sub : d["into"].as_sequence()) {
            subs.push_back(sub.as<std::string>());
        }
        if (type == "sector") {
            try {
                table->insert_subsectors(id, subs);
            } catch (std::out_of_range& ex) {
                throw std::runtime_error("Sector '" + id + "' not found");
            }
        } else if (type == "region") {
            try {
                table->insert_subregions(id, subs);
            } catch (std::out_of_range& ex) {
                throw std::runtime_error("Region '" + id + "' not found");
            }
        } else {
            throw std::runtime_error("Unknown type");
        }
    }
    for (const auto& d : settings.as_sequence()) {
        for (const auto& proxy : d["proxies"].as_sequence()) {
            read_proxy_file(proxy["file"].as<std::string>(), proxy["level"].as<int>(), proxy["year"].as<I>());
        }
    }
}

template<typename T, typename I>
const Sector<I>* Disaggregation<T, I>::read_sector(csv::Parser& in) {
    std::string str;
    try {
        str = in.read_and_next<std::string>();
        return table->index_set().sector(str);
    } catch (std::out_of_range& ex) {
        throw std::runtime_error("Sector '" + str + "' not found");
    }
}

template<typename T, typename I>
const Region<I>* Disaggregation<T, I>::read_region(csv::Parser& in) {
    std::string str;
    try {
        str = in.read_and_next<std::string>();
        return table->index_set().region(str);
    } catch (std::out_of_range& ex) {
        throw std::runtime_error("Region '" + str + "' not found");
    }
}

template<typename T, typename I>
const Sector<I>* Disaggregation<T, I>::read_subsector(csv::Parser& in) {
    std::string str;
    try {
        str = in.read_and_next<std::string>();
        return table->index_set().sector(str);
    } catch (std::out_of_range& ex) {
        throw std::runtime_error("Sector '" + str + "' not found");
    }
}

template<typename T, typename I>
const Region<I>* Disaggregation<T, I>::read_subregion(csv::Parser& in) {
    std::string str;
    try {
        str = in.read_and_next<std::string>();
        return table->index_set().region(str);
    } catch (std::out_of_range& ex) {
        throw std::runtime_error("Region '" + str + "' not found");
    }
}

template<typename T, typename I>
void Disaggregation<T, I>::read_proxy_file(const std::string& filename, int d, I year) {
    std::ifstream file(filename);
    if (!file) {
        throw std::runtime_error("Could not open proxy file");
    }
    const I& subsectors_count = table->index_set().subsectors().size();
    const I& subregions_count = table->index_set().subregions().size();
    const I& sectors_count = table->index_set().supersectors().size();
    const I& regions_count = table->index_set().superregions().size();
    try {
        csv::Parser in(file);
        while (in.next_row()) {  // also skips header line
            if (year != in.read_and_next<I>()) {
                continue;
            }
            switch (d) {
                case LEVEL_POPULATION_1:
                case LEVEL_GDP_SUBREGION_2: {
                    const Region<I>* r_lambda = read_subregion(in);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(subregions_count));
                        proxy_sums[d].reset(new ProxyData(regions_count));
                    }
                    (*proxies[d])(r_lambda) = in.read_and_next<T>();
                    if (!in.eol()) {
                        (*proxy_sums[d])(r_lambda->parent()) = in.read_and_next<T>();
                    }
                } break;
                case LEVEL_GDP_SUBSECTOR_3: {
                    const Sector<I>* i_mu = read_subsector(in);
                    const Region<I>* r = read_region(in);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(subsectors_count, regions_count));
                        proxy_sums[d].reset(new ProxyData(sectors_count, regions_count));
                    }
                    (*proxies[d])(i_mu, r) = in.read_and_next<T>();
                    if (!in.eol()) {
                        (*proxy_sums[d])(i_mu->parent(), r) = in.read_and_next<T>();
                    }
                } break;
                case LEVEL_GDP_SUBREGIONAL_SUBSECTOR_4: {
                    const Sector<I>* i_mu = read_subsector(in);
                    const Region<I>* r_lambda = read_subregion(in);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(subsectors_count, subregions_count));
                        proxy_sums[d].reset(new ProxyData(sectors_count, regions_count));
                    }
                    (*proxies[d])(i_mu, r_lambda) = in.read_and_next<T>();
                    if (!in.eol()) {
                        (*proxy_sums[d])(i_mu->parent(), r_lambda->parent()) = in.read_and_next<T>();
                    }
                } break;
                case LEVEL_IMPORT_SUBSECTOR_5: {
                    const Sector<I>* i_mu = read_subsector(in);
                    const Region<I>* s = read_region(in);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(subsectors_count, regions_count));
                    }
                    (*proxies[d])(i_mu, s) = in.read_and_next<T>();
                } break;
                case LEVEL_IMPORT_SUBREGION_6: {
                    const Sector<I>* j = read_sector(in);
                    const Region<I>* r_lambda = read_subregion(in);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(sectors_count, subregions_count));
                    }
                    (*proxies[d])(j, r_lambda) = in.read_and_next<T>();
                } break;
                case LEVEL_INTERREGIONAL_SUBSECTOR_INPUT_7: {
                    const Sector<I>* i_mu = read_subsector(in);
                    const Region<I>* r_lambda = read_subregion(in);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(subsectors_count, subregions_count));
                    }
                    (*proxies[d])(i_mu, r_lambda) = in.read_and_next<T>();
                } break;
                case LEVEL_EXPORT_SUBREGIONAL_SUBSECTOR_8: {
                    const Sector<I>* i_mu = read_subsector(in);
                    const Region<I>* r_lambda = read_subregion(in);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(subsectors_count, subregions_count));
                    }
                    (*proxies[d])(i_mu, r_lambda) = in.read_and_next<T>();
                } break;
                case LEVEL_IMPORT_SUBSECTOR_BY_REGIONAL_SECTOR_9: {
                    const Sector<I>* i_mu = read_subsector(in);
                    const Sector<I>* j = read_sector(in);
                    const Region<I>* s = read_region(in);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(subsectors_count, sectors_count, regions_count));
                    }
                    (*proxies[d])(i_mu, j, s) = in.read_and_next<T>();
                } break;
                case LEVEL_EXPORT_SUBREGION_10: {
                    const Sector<I>* j = read_sector(in);
                    const Region<I>* s = read_region(in);
                    const Region<I>* r_lambda = read_subregion(in);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(sectors_count, regions_count, subregions_count));
                    }
                    (*proxies[d])(j, s, r_lambda) = in.read_and_next<T>();
                } break;
                case LEVEL_SUBREGIONAL_SUBSECTOR_INPUT_11: {
                    const Sector<I>* i_mu1 = read_subsector(in);
                    const Sector<I>* i_mu2 = read_subsector(in);
                    const Region<I>* r_lambda = read_subregion(in);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(subsectors_count, subsectors_count, subregions_count));
                    }
                    (*proxies[d])(i_mu1, i_mu2, r_lambda) = in.read_and_next<T>();
                } break;
                case LEVEL_EXPORT_SUBREGIONAL_SUBSECTOR_TO_REGION_12: {
                    const Sector<I>* i_mu = read_subsector(in);
                    const Region<I>* r_lambda = read_subregion(in);
                    const Region<I>* s = read_region(in);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(subsectors_count, subregions_count, regions_count));
                    }
                    (*proxies[d])(i_mu, r_lambda, s) = in.read_and_next<T>();
                } break;
                case LEVEL_IMPORT_SUBREGIONAL_SUBSECTOR_13: {
                    const Sector<I>* j = read_sector(in);
                    const Sector<I>* i_mu = read_subsector(in);
                    const Region<I>* r_lambda = read_subregion(in);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(sectors_count, subsectors_count, subregions_count));
                    }
                    (*proxies[d])(j, i_mu, r_lambda) = in.read_and_next<T>();
                } break;
                case LEVEL_EXPORT_SUBREGIONAL_SUBSECTOR_TO_SUBREGION_14: {
                    const Sector<I>* i_mu = read_subsector(in);
                    const Region<I>* r_lambda1 = read_subregion(in);
                    const Region<I>* r_lambda2 = read_subregion(in);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(subsectors_count, subregions_count, subregions_count));
                    }
                    (*proxies[d])(i_mu, r_lambda1, r_lambda2) = in.read_and_next<T>();
                } break;
                case LEVEL_PETERS1_15:
                case LEVEL_PETERS2_16:
                case LEVEL_PETERS3_17:
                    throw std::runtime_error("Levels 15, 16, 17 cannot be given explicitly");
                    break;
                case LEVEL_EXACT_18: {
                    const Sector<I>* i_mu1 = read_subsector(in);
                    const Region<I>* r_lambda1 = read_subregion(in);
                    const Sector<I>* i_mu2 = read_subsector(in);
                    const Region<I>* r_lambda2 = read_subregion(in);
                    if (!proxies[d]) {
                        proxies[d].reset(new ProxyData(subsectors_count, subregions_count, subsectors_count, subregions_count));
                    }
                    (*proxies[d])(i_mu1, r_lambda1, i_mu2, r_lambda2) = in.read_and_next<T>();
                } break;
                default:
                    throw std::runtime_error("Unknown d-level");
                    break;
            }
        }
    } catch (const csv::parser_exception& ex) {
        std::stringstream s;
        s << ex.what();
        s << " (in " << filename << ", line " << ex.row << ", col " << ex.col << ")";
        throw std::runtime_error(s.str());
    }
}

template<typename T, typename I>
void Disaggregation<T, I>::refine() {
    last_table.reset(new Table<T, I>(*table));
    quality.reset(new Table<int, I>(table->index_set(), 0));

    // apply actual algorithm
    for (int d = 1; d < PROXY_COUNT; d++) {
        if (proxies[d]
            || (d == LEVEL_PETERS1_15 && proxies[LEVEL_IMPORT_SUBSECTOR_5] && proxies[LEVEL_IMPORT_SUBSECTOR_BY_REGIONAL_SECTOR_9]
                && proxies[LEVEL_EXPORT_SUBREGIONAL_SUBSECTOR_TO_REGION_12])
            || (d == LEVEL_PETERS2_16 && proxies[LEVEL_IMPORT_SUBREGION_6] && proxies[LEVEL_EXPORT_SUBREGION_10]
                && proxies[LEVEL_IMPORT_SUBREGIONAL_SUBSECTOR_13])
            || (d == LEVEL_PETERS3_17 && proxies[LEVEL_INTERREGIONAL_SUBSECTOR_INPUT_7] && proxies[LEVEL_SUBREGIONAL_SUBSECTOR_INPUT_11]
                && proxies[LEVEL_EXPORT_SUBREGIONAL_SUBSECTOR_TO_SUBREGION_14])) {
            last_table->replace_table_from(*table);
            approximate(d);
            adjust(d);
        }
    }

    // cleanup
    quality.reset();
    last_table.reset();
}

template<typename T, typename I>
static void for_all_sub(
    const Sector<I>* i,
    const Region<I>* r,
    const Sector<I>* j,
    const Region<I>* s,
    std::function<void(const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, bool i_sub, bool r_sub, bool j_sub, bool s_sub)> func,
    bool i_sub = false,
    bool r_sub = false,
    bool j_sub = false,
    bool s_sub = false) {
    if (i->has_sub()) {
        for (const auto& i_mu : i->as_super()->sub()) {
            for_all_sub<T, I>(i_mu, r, j, s, func, true, r_sub, j_sub, s_sub);
        }
    } else if (r->has_sub()) {
        for (const auto& r_lambda : r->as_super()->sub()) {
            for_all_sub<T, I>(i, r_lambda, j, s, func, i_sub, true, j_sub, s_sub);
        }
    } else if (j->has_sub()) {
        for (const auto& j_mu : j->as_super()->sub()) {
            for_all_sub<T, I>(i, r, j_mu, s, func, i_sub, r_sub, true, s_sub);
        }
    } else if (s->has_sub()) {
        for (const auto& s_lambda : s->as_super()->sub()) {
            for_all_sub<T, I>(i, r, j, s_lambda, func, i_sub, r_sub, j_sub, true);
        }
    } else {
        func(i, r, j, s, i_sub, r_sub, j_sub, s_sub);
    }
}

template<typename T, typename I>
void Disaggregation<T, I>::approximate(const int& d) {
    switch (d) {
        case LEVEL_POPULATION_1:

        case LEVEL_GDP_SUBREGION_2:

            for (const auto& r : table->index_set().superregions()) {
                if (std::isnan((*proxy_sums[d])(r.get()))) {
                    T sum = 0;
                    for (const auto& r_lambda : r->sub()) {
                        sum += (*proxies[d])(r_lambda);
                    }
                    (*proxy_sums[d])(r.get()) = sum;
                }
            }
            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(
                        ir.sector, ir.region, js.sector, js.region,
                        [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, bool /* i_sub */, bool r_sub, bool /* j_sub */,
                            bool s_sub) {
                            T sum1 = 1, value1 = 1, sum2 = 1, value2 = 1;
                            if (r_sub) {
                                sum1 = (*proxy_sums[d])(r->parent());
                                value1 = (*proxies[d])(r);
                            }
                            if (s_sub) {
                                sum2 = (*proxy_sums[d])(s->parent());
                                value2 = (*proxies[d])(s);
                            }
                            if (r_sub || s_sub) {
                                if (!std::isnan(value1) && !std::isnan(sum1) && sum1 > 0 && !std::isnan(value2) && !std::isnan(sum2) && sum2 > 0) {
                                    (*table)(i, r, j, s) = last_table->sum(i, r->super(), j, s->super()) * value1 * value2 / sum1 / sum2;
                                    (*quality)(i, r, j, s) = d;
                                }
                            }
                        });
                }
            }
            break;

        case LEVEL_GDP_SUBSECTOR_3:

            for (const auto& i : table->index_set().supersectors()) {
                if (i->has_sub()) {
                    for (const auto& r : i->regions()) {
                        if (std::isnan((*proxy_sums[d])(i.get(), r))) {
                            T sum = 0;
                            for (const auto& i_mu : i->sub()) {
                                sum += (*proxies[d])(i_mu, r);
                            }
                            (*proxy_sums[d])(i.get(), r) = sum;
                        }
                    }
                }
            }
            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(
                        ir.sector, ir.region, js.sector, js.region,
                        [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, bool i_sub, bool /* r_sub */, bool j_sub,
                            bool /* s_sub */) {
                            T sum1 = 1, value1 = 1, sum2 = 1, value2 = 1;
                            if (i_sub) {
                                sum1 = (*proxy_sums[d])(i->parent(), r->super());
                                value1 = (*proxies[d])(i, r->super());
                            }
                            if (j_sub) {
                                sum2 = (*proxy_sums[d])(j->parent(), s->super());
                                value2 = (*proxies[d])(j, s->super());
                            }
                            if (i_sub || j_sub) {
                                if (!std::isnan(value1) && !std::isnan(sum1) && sum1 > 0 && !std::isnan(value2) && !std::isnan(sum2) && sum2 > 0) {
                                    (*table)(i, r, j, s) = last_table->sum(i->super(), r, j->super(), s) * value1 * value2 / sum1 / sum2;
                                    (*quality)(i, r, j, s) = d;
                                }
                            }
                        });
                }
            }
            break;

        case LEVEL_GDP_SUBREGIONAL_SUBSECTOR_4:

            for (const auto& i : table->index_set().supersectors()) {
                if (i->has_sub()) {
                    for (const auto& r : i->regions()) {
                        if (r->has_sub()) {
                            if (std::isnan((*proxy_sums[d])(i.get(), r))) {
                                T sum = 0;
                                for (const auto& i_mu : i->sub()) {
                                    for (const auto& r_lambda : i->sub()) {
                                        sum += (*proxies[d])(i_mu, r_lambda);
                                    }
                                }
                                (*proxy_sums[d])(i.get(), r) = sum;
                            }
                        }
                    }
                }
            }
            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(
                        ir.sector, ir.region, js.sector, js.region,
                        [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, bool i_sub, bool r_sub, bool j_sub, bool s_sub) {
                            T sum1 = 1, value1 = 1, sum2 = 1, value2 = 1;
                            if (i_sub && r_sub) {
                                sum1 = (*proxy_sums[d])(i->parent(), r->parent());
                                value1 = (*proxies[d])(i, r);
                            }
                            if (j_sub && s_sub) {
                                sum2 = (*proxy_sums[d])(j->parent(), s->parent());
                                value2 = (*proxies[d])(j, s);
                            }
                            if ((i_sub && r_sub) || (j_sub && s_sub)) {
                                if (!std::isnan(value1) && !std::isnan(sum1) && sum1 > 0 && !std::isnan(value2) && !std::isnan(sum2) && sum2 > 0) {
                                    (*table)(i, r, j, s) = last_table->sum(i->super(), r->super(), j->super(), s->super()) * value1 * value2 / sum1 / sum2;
                                    (*quality)(i, r, j, s) = d;
                                }
                            }
                        });
                }
            }
            break;

        case LEVEL_IMPORT_SUBSECTOR_5:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, bool i_sub, bool /* r_sub */,
                                          bool /* j_sub */, bool /* s_sub */) {
                                          if (i_sub) {
                                              const T& value = (*proxies[d])(i, s->super());
                                              if (!std::isnan(value)) {
                                                  (*table)(i, r, j, s) = last_table->sum(i->parent(), r, j, s) * value
                                                                         / last_table->sum(i->parent(), nullptr, nullptr, s->super());
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        case LEVEL_IMPORT_SUBREGION_6:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, bool /* i_sub */, bool /* r_sub */,
                                          bool /* j_sub */, bool s_sub) {
                                          if (s_sub) {
                                              const T& value = (*proxies[d])(i->super(), s);
                                              if (!std::isnan(value)) {
                                                  (*table)(i, r, j, s) = last_table->sum(i, r, j, s->parent()) * value
                                                                         / last_table->sum(i->super(), nullptr, nullptr, s->parent());
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        case LEVEL_INTERREGIONAL_SUBSECTOR_INPUT_7:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, bool i_sub, bool /* r_sub */,
                                          bool /* j_sub */, bool s_sub) {
                                          if (i_sub && s_sub) {
                                              const T& value = (*proxies[d])(i, s);
                                              if (!std::isnan(value)) {
                                                  (*table)(i, r, j, s) = last_table->sum(i->parent(), r, j, s->parent()) * value
                                                                         / last_table->sum(i->parent(), nullptr, nullptr, s->parent());
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        case LEVEL_EXPORT_SUBREGIONAL_SUBSECTOR_8:

            for (const auto& ir : table->index_set().super_indices) {
                const T& sum = basetable->basesum(ir.sector, ir.region, nullptr, nullptr);
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(
                        ir.sector, ir.region, js.sector, js.region,
                        [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, bool i_sub, bool r_sub, bool j_sub, bool s_sub) {
                            if (i_sub && r_sub && j_sub == s_sub) {
                                const T& value = (*proxies[d])(i, r);
                                if (!std::isnan(value)) {
                                    (*table)(i, r, j, s) = last_table->sum(i->parent(), r->parent(), j, s) * value / sum;
                                    (*quality)(i, r, j, s) = d;
                                }
                            }
                        });
                }
            }
            break;

        case LEVEL_IMPORT_SUBSECTOR_BY_REGIONAL_SECTOR_9:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, bool i_sub, bool /* r_sub */,
                                          bool /* j_sub */, bool /* s_sub */) {
                                          if (i_sub) {
                                              const T& value = (*proxies[d])(i, j->super(), s->super());
                                              if (!std::isnan(value)) {
                                                  (*table)(i, r, j, s) = last_table->sum(i->parent(), r, j, s) * value
                                                                         / last_table->sum(i->parent(), nullptr, j->super(), s->super());
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        case LEVEL_EXPORT_SUBREGION_10:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, bool /* i_sub */, bool /* r_sub */,
                                          bool /* j_sub */, bool s_sub) {
                                          if (s_sub) {
                                              const T& value = (*proxies[d])(i->super(), r->super(), s);
                                              if (!std::isnan(value)) {
                                                  (*table)(i, r, j, s) = last_table->sum(i, r, j, s->parent()) * value
                                                                         / last_table->sum(i->super(), r->super(), nullptr, s->parent());
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        case LEVEL_SUBREGIONAL_SUBSECTOR_INPUT_11:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, bool i_sub, bool /* r_sub */,
                                          bool j_sub, bool s_sub) {
                                          if (i_sub && j_sub && s_sub) {
                                              const T& value = (*proxies[d])(i, j, s);
                                              if (!std::isnan(value)) {
                                                  (*table)(i, r, j, s) = last_table->sum(i->parent(), r, j->parent(), s->parent()) * value
                                                                         / last_table->sum(i->parent(), nullptr, j->parent(), s->parent());
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        case LEVEL_EXPORT_SUBREGIONAL_SUBSECTOR_TO_REGION_12:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, bool i_sub, bool r_sub,
                                          bool /* j_sub */, bool /* s_sub */) {
                                          if (i_sub && r_sub) {
                                              const T& value = (*proxies[d])(i, r, s->super());
                                              if (!std::isnan(value)) {
                                                  (*table)(i, r, j, s) = last_table->sum(i->parent(), r->parent(), j, s) * value
                                                                         / last_table->sum(i->parent(), r->parent(), nullptr, s->super());
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        case LEVEL_IMPORT_SUBREGIONAL_SUBSECTOR_13:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, bool /* i_sub */, bool /* r_sub */,
                                          bool j_sub, bool s_sub) {
                                          if (j_sub && s_sub) {
                                              const T& value = (*proxies[d])(i->super(), j, s);
                                              if (!std::isnan(value)) {
                                                  (*table)(i, r, j, s) = last_table->sum(i, r, j->parent(), s->parent()) * value
                                                                         / last_table->sum(i->super(), nullptr, j->parent(), s->parent());
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        case LEVEL_EXPORT_SUBREGIONAL_SUBSECTOR_TO_SUBREGION_14:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, bool i_sub, bool r_sub,
                                          bool /* j_sub */, bool s_sub) {
                                          if (i_sub && r_sub && s_sub) {
                                              const T& value = (*proxies[d])(i, r, s);
                                              if (!std::isnan(value)) {
                                                  (*table)(i, r, j, s) = last_table->sum(i->parent(), r->parent(), j, s->parent()) * value
                                                                         / last_table->sum(i->parent(), r->parent(), nullptr, s->parent());
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        case LEVEL_PETERS1_15:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, bool i_sub, bool r_sub,
                                          bool /* j_sub */, bool /* s_sub */) {
                                          if (i_sub && r_sub) {
                                              const T& value1 = (*proxies[LEVEL_IMPORT_SUBSECTOR_5])(i, s->super());
                                              const T& value2 = (*proxies[LEVEL_IMPORT_SUBSECTOR_BY_REGIONAL_SECTOR_9])(i, j->super(), s->super());
                                              const T& value3 = (*proxies[LEVEL_EXPORT_SUBREGIONAL_SUBSECTOR_TO_REGION_12])(i, r, s->super());
                                              if (!std::isnan(value1) && !std::isnan(value2) && !std::isnan(value3)) {
                                                  (*table)(i, r, j, s) = value2 * value3 / value1;
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        case LEVEL_PETERS2_16:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, bool /* i_sub */, bool /* r_sub */,
                                          bool j_sub, bool s_sub) {
                                          if (j_sub && s_sub) {
                                              const T& value1 = (*proxies[LEVEL_IMPORT_SUBREGION_6])(i->super(), s);
                                              const T& value2 = (*proxies[LEVEL_EXPORT_SUBREGION_10])(i->super(), r->super(), s);
                                              const T& value3 = (*proxies[LEVEL_IMPORT_SUBREGIONAL_SUBSECTOR_13])(i->super(), j, s);
                                              if (!std::isnan(value1) && !std::isnan(value2) && !std::isnan(value3)) {
                                                  (*table)(i, r, j, s) = value3 * value2 / value1;
                                                  (*quality)(i, r, j, s) = d;
                                              }
                                          }
                                      });
                }
            }
            break;

        case LEVEL_PETERS3_17:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(
                        ir.sector, ir.region, js.sector, js.region,
                        [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, bool i_sub, bool r_sub, bool j_sub, bool s_sub) {
                            if (i_sub && r_sub && j_sub && s_sub) {
                                const T& value1 = (*proxies[LEVEL_INTERREGIONAL_SUBSECTOR_INPUT_7])(i, s);
                                const T& value2 = (*proxies[LEVEL_SUBREGIONAL_SUBSECTOR_INPUT_11])(i, j, s);
                                const T& value3 = (*proxies[LEVEL_EXPORT_SUBREGIONAL_SUBSECTOR_TO_SUBREGION_14])(i, r, s);
                                if (!std::isnan(value1) && !std::isnan(value2) && !std::isnan(value3)) {
                                    (*table)(i, r, j, s) = value2 * value3 / value1;
                                    (*quality)(i, r, j, s) = d;
                                }
                            }
                        });
                }
            }
            break;

        case LEVEL_EXACT_18:

            for (const auto& ir : table->index_set().super_indices) {
                for (const auto& js : table->index_set().super_indices) {
                    for_all_sub<T, I>(
                        ir.sector, ir.region, js.sector, js.region,
                        [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, bool i_sub, bool r_sub, bool j_sub, bool s_sub) {
                            if (i_sub && r_sub && j_sub && s_sub) {
                                const T& value = (*proxies[d])(i, r, j, s);
                                if (!std::isnan(value)) {
                                    (*table)(i, r, j, s) = value;
                                    (*quality)(i, r, j, s) = d;
                                }
                            }
                        });
                }
            }
            break;

        default:

            throw std::runtime_error("Unknown d-level");
            break;
    }
}

template<typename T, typename I>
void Disaggregation<T, I>::adjust(const int& d) {
    for (const auto& ir : table->index_set().super_indices) {
        for (const auto& js : table->index_set().super_indices) {
            const T& base = basetable->base(ir.sector, ir.region, js.sector, js.region);
            if (base > 0) {
                T sum_of_exact = 0;
                T sum_of_non_exact = 0;
                for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                  [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, bool /* i_sub */, bool /* r_sub */,
                                      bool /* j_sub */, bool /* s_sub */) {
                                      if ((*quality)(i, r, j, s) == d) {
                                          sum_of_exact += (*table)(i, r, j, s);
                                      } else {
                                          sum_of_non_exact += (*table)(i, r, j, s);
                                      }
                                  });
                T correction_factor = base / (sum_of_exact + sum_of_non_exact);
                if (base > sum_of_exact && sum_of_non_exact > 0) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, bool /* i_sub */, bool /* r_sub */,
                                          bool /* j_sub */, bool /* s_sub */) {
                                          if ((*quality)(i, r, j, s) != d) {
                                              (*table)(i, r, j, s) = (base - sum_of_exact) * (*table)(i, r, j, s) / sum_of_non_exact;
                                          }
                                      });
                } else if (correction_factor < 1 || correction_factor > 1) {
                    for_all_sub<T, I>(ir.sector, ir.region, js.sector, js.region,
                                      [&](const Sector<I>* i, const Region<I>* r, const Sector<I>* j, const Region<I>* s, bool /* i_sub */, bool /* r_sub */,
                                          bool /* j_sub */, bool /* s_sub */) { (*table)(i, r, j, s) = correction_factor * (*table)(i, r, j, s); });
                }
            }
        }
    }
}

template class Disaggregation<double, std::size_t>;
template class Disaggregation<float, std::size_t>;
}  // namespace mrio

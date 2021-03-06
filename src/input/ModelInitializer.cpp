/*
  Copyright (C) 2014-2017 Sven Willner <sven.willner@pik-potsdam.de>
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

#include "input/ModelInitializer.h"
#include <algorithm>
#include <fstream>
#include <memory>
#include <vector>
#include "MRIOIndexSet.h"
#include "MRIOTable.h"
#include "model/BusinessConnection.h"
#include "model/Consumer.h"
#include "model/EconomicAgent.h"
#include "model/Firm.h"
#include "model/GeoConnection.h"
#include "model/GeoPoint.h"
#include "model/GeoRoute.h"
#include "model/Model.h"
#include "model/Region.h"
#include "model/Sector.h"
#include "model/Storage.h"
#include "netcdf_headers.h"
#include "optimization.h"
#include "run.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
ModelInitializer<ModelVariant>::ModelInitializer(Model<ModelVariant>* model_p, const settings::SettingsNode& settings_p)
    : model_m(model_p), settings(settings_p) {
    const settings::SettingsNode& parameters = settings["model"];
    const settings::SettingsNode& run = settings["run"];
    model()->start_time(run["start"].as<Time>());
    model()->stop_time(run["stop"].as<Time>());
    model()->delta_t(parameters["delta_t"].as<Time>());
    model()->no_self_supply(parameters["no_self_supply"].as<bool>());
}

template<class ModelVariant>
settings::SettingsNode ModelInitializer<ModelVariant>::get_named_property(const std::string& tag_name,
                                                                          const std::string& node_name,
                                                                          const std::string& property_name) const {
    const settings::SettingsNode& node_settings = settings[tag_name];
    if (node_settings.has(node_name) && node_settings[node_name].has(property_name)) {
        return node_settings[node_name][property_name];
    }
    return node_settings["ALL"][property_name];
}

template<class ModelVariant>
settings::SettingsNode ModelInitializer<ModelVariant>::get_firm_property(const std::string& sector_name,
                                                                         const std::string& region_name,
                                                                         const std::string& property_name) const {
    const settings::SettingsNode& firm_settings = settings["firms"];
    if (firm_settings.has(sector_name + ":" + region_name) && firm_settings[sector_name + ":" + region_name].has(property_name)) {
        return firm_settings[sector_name + ":" + region_name][property_name];
    }
    if (firm_settings.has(sector_name) && firm_settings[sector_name].has(property_name)) {
        return firm_settings[sector_name][property_name];
    }
    if (firm_settings.has(region_name) && firm_settings[region_name].has(property_name)) {
        return firm_settings[region_name][property_name];
    }
    return firm_settings["ALL"][property_name];
}

template<class ModelVariant>
Firm<ModelVariant>* ModelInitializer<ModelVariant>::add_firm(Sector<ModelVariant>* sector, Region<ModelVariant>* region) {
    auto firm = new Firm<ModelVariant>(sector, region,
                                       static_cast<Ratio>(get_firm_property(sector->id(), region->id(), "possible_overcapacity_ratio").template as<double>()));
    region->economic_agents.emplace_back(firm);
    sector->firms.push_back(firm);
    return firm;
}

template<class ModelVariant>
Consumer<ModelVariant>* ModelInitializer<ModelVariant>::add_consumer(Region<ModelVariant>* region) {
    auto consumer = new Consumer<ModelVariant>(region);
    region->economic_agents.emplace_back(consumer);
    return consumer;
}

template<class ModelVariant>
Region<ModelVariant>* ModelInitializer<ModelVariant>::add_region(const std::string& name) {
    Region<ModelVariant>* region = model()->find_region(name);
    if (region == nullptr) {
        region = model()->add_region(name);
    }
    return region;
}

#ifdef VARIANT_BASIC
template<>
Sector<VariantBasic>* ModelInitializer<VariantBasic>::add_sector(const std::string& name) {
    Sector<VariantBasic>* sector = model()->find_sector(name);
    if (sector == nullptr) {
        sector =
            model()->add_sector(name, get_named_property("sectors", name, "upper_storage_limit").template as<Ratio>(),
                                get_named_property("sectors", name, "initial_storage_fill_factor").template as<FloatType>() * model()->delta_t(),
                                Sector<VariantBasic>::map_transport_type(get_named_property("sectors", name, "transport").template as<settings::hstring>()));
    }
    return sector;
}
#endif

#ifdef VARIANT_DEMAND
template<>
Sector<VariantDemand>* ModelInitializer<VariantDemand>::add_sector(const std::string& name) {
    Sector<VariantDemand>* sector = model()->find_sector(name);
    if (sector == nullptr) {
        sector =
            model()->add_sector(name, get_named_property("sectors", name, "upper_storage_limit").template as<Ratio>(),
                                get_named_property("sectors", name, "initial_storage_fill_factor").template as<FloatType>() * model()->delta_t(),
                                Sector<VariantDemand>::map_transport_type(get_named_property("sectors", name, "transport").template as<settings::hstring>()));
        sector->parameters_writable().storage_refill_enforcement_gamma =
            get_named_property("sectors", name, "storage_refill_enforcement").template as<FloatType>() * model()->delta_t();
    }
    return sector;
}
#endif

#ifdef VARIANT_PRICES
template<>
Sector<VariantPrices>* ModelInitializer<VariantPrices>::add_sector(const std::string& name) {
    Sector<VariantPrices>* sector = model()->find_sector(name);
    if (sector == nullptr) {
        sector =
            model()->add_sector(name, get_named_property("sectors", name, "upper_storage_limit").template as<Ratio>(),
                                get_named_property("sectors", name, "initial_storage_fill_factor").template as<FloatType>() * model()->delta_t(),
                                Sector<VariantPrices>::map_transport_type(get_named_property("sectors", name, "transport").template as<settings::hstring>()));
        sector->parameters_writable().supply_elasticity = get_named_property("sectors", name, "supply_elasticity").template as<Ratio>();
        sector->parameters_writable().price_increase_production_extension =
            get_named_property("sectors", name, "price_increase_production_extension").template as<Price>();
        sector->parameters_writable().estimated_price_increase_production_extension =
            get_named_property("sectors", name, "estimated_price_increase_production_extension")
                .template as<Price>(to_float(sector->parameters_writable().price_increase_production_extension));
        sector->parameters_writable().initial_markup = get_named_property("sectors", name, "initial_markup").template as<Price>();
        sector->parameters_writable().consumption_price_elasticity = get_named_property("sectors", name, "consumption_price_elasticity").template as<Ratio>();
        sector->parameters_writable().target_storage_refill_time =
            get_named_property("sectors", name, "target_storage_refill_time").template as<FloatType>() * model()->delta_t();
        sector->parameters_writable().target_storage_withdraw_time =
            get_named_property("sectors", name, "target_storage_withdraw_time").template as<FloatType>() * model()->delta_t();
    }
    return sector;
}
#endif

template<class ModelVariant>
void ModelInitializer<ModelVariant>::initialize_connection(
    Sector<ModelVariant>* sector_from, Region<ModelVariant>* region_from, Sector<ModelVariant>* sector_to, Region<ModelVariant>* region_to, const Flow& flow) {
    Firm<ModelVariant>* firm_from = model()->find_firm(sector_from, region_from->id());
    if (!firm_from) {
        firm_from = add_firm(sector_from, region_from);
        if (!firm_from) {
            return;
        }
    }
    Firm<ModelVariant>* firm_to = model()->find_firm(sector_to, region_to->id());
    if (!firm_to) {
        firm_to = add_firm(sector_to, region_to);
        if (!firm_to) {
            return;
        }
    }
    initialize_connection(firm_from, firm_to, flow);
}

template<class ModelVariant>
void ModelInitializer<ModelVariant>::initialize_connection(Firm<ModelVariant>* firm_from, EconomicAgent<ModelVariant>* economic_agent_to, const Flow& flow) {
    if (model()->no_self_supply() && (static_cast<void*>(firm_from) == static_cast<void*>(economic_agent_to))) {
        return;
    }
#ifdef DEBUG
#define SKIP(a)                                                    \
    if (firm_from->id() + "->" + economic_agent_to->id() == (a)) { \
        warning("skipping " << (a));                               \
        return;                                                    \
    }
// use SKIP("...:...->...:..."); to skip connections
#endif
    Sector<ModelVariant>* sector_from = firm_from->sector;
    Storage<ModelVariant>* input_storage = economic_agent_to->find_input_storage(sector_from->id());
    if (!input_storage) {
        input_storage = new Storage<ModelVariant>(sector_from, economic_agent_to);
        economic_agent_to->input_storages.emplace_back(input_storage);
    }
    assert(flow.get_quantity() > 0.0);
    input_storage->add_initial_flow_Z_star(flow);
    firm_from->add_initial_production_X_star(flow);

    auto business_connection =
        std::make_shared<BusinessConnection<ModelVariant>>(input_storage->purchasing_manager.get(), firm_from->sales_manager.get(), flow);
    firm_from->sales_manager->business_connections.emplace_back(business_connection);
    input_storage->purchasing_manager->business_connections.emplace_back(business_connection);

    if (static_cast<void*>(firm_from) == static_cast<void*>(economic_agent_to)) {
        firm_from->self_supply_connection(business_connection);
    }
}

template<class ModelVariant>
void ModelInitializer<ModelVariant>::clean_network() {
    int firm_count;
    int consumer_count;
    bool needs_cleaning = true;
    while (needs_cleaning) {
        firm_count = 0;
        consumer_count = 0;
        needs_cleaning = false;
#ifdef CLEANUP_INFO
        info("Cleaning up...");
#endif
        for (auto region = model()->regions.begin(); region != model()->regions.end(); ++region) {
            for (auto economic_agent = (*region)->economic_agents.begin(); economic_agent != (*region)->economic_agents.end();) {
                if ((*economic_agent)->type == EconomicAgent<ModelVariant>::Type::FIRM) {
                    Firm<ModelVariant>* firm = (*economic_agent)->as_firm();

                    FlowQuantity input = FlowQuantity(0.0);
                    for (const auto& input_storage : firm->input_storages) {
                        input += input_storage->initial_input_flow_I_star().get_quantity();
                    }
                    FloatType value_added = to_float(firm->initial_production_X_star().get_quantity() - input);

                    if (value_added <= 0.0 || firm->sales_manager->business_connections.empty()
                        || (firm->sales_manager->business_connections.size() == 1 && firm->self_supply_connection()) || firm->input_storages.empty()
                        || (firm->input_storages.size() == 1 && firm->self_supply_connection())) {
                        needs_cleaning = true;

#ifdef CLEANUP_INFO
                        if (value_added <= 0.0) {
                            warning(firm->id() << ": removed (value added only " << value_added << ")");
                        } else if (firm->sales_manager->business_connections.size() == 0
                                   || (firm->sales_manager->business_connections.size() == 1 && firm->self_supply_connection())) {
                            warning(firm->id() << ": removed (no outgoing connection)");
                        } else {
                            warning(firm->id() << ": removed (no incoming connection)");
                        }
#endif
                        // Alter initial_input_flow of buying economic agents
                        for (auto& business_connection : firm->sales_manager->business_connections) {
                            if (!business_connection->buyer) {
                                error("Buyer invalid");
                            }
                            if (!business_connection->buyer->storage->subtract_initial_flow_Z_star(business_connection->initial_flow_Z_star())) {
                                business_connection->buyer->remove_business_connection(business_connection.get());
                            }
                        }

                        // Alter initial_production of supplying firms
                        for (auto& storage : firm->input_storages) {
                            for (auto& business_connection : storage->purchasing_manager->business_connections) {
                                if (!business_connection->seller) {
                                    error("Seller invalid");
                                }
                                business_connection->seller->firm->subtract_initial_production_X_star(business_connection->initial_flow_Z_star());
                                business_connection->seller->remove_business_connection(business_connection.get());
                            }
                        }

                        firm->sector->remove_firm(firm);
                        // Clean up memory of firm
                        economic_agent = (*region)->economic_agents.erase(economic_agent);
                    } else {
                        ++economic_agent;
                        ++firm_count;
                    }
                } else if ((*economic_agent)->type == EconomicAgent<ModelVariant>::Type::CONSUMER) {
                    Consumer<ModelVariant>* consumer = (*economic_agent)->as_consumer();

                    if (consumer->input_storages.empty()) {
#ifdef CLEANUP_INFO
                        warning(consumer->id() << ": removed (no incoming connection)");
#endif
                        // Clean up memory of consumer
                        economic_agent = (*region)->economic_agents.erase(economic_agent);
                    } else {
                        ++economic_agent;
                        ++consumer_count;
                    }
                } else {
                    error("Unknown economic agent type");
                }
            }
        }
    }
    info("Number of firms: " << firm_count);
    info("Number of consumers: " << consumer_count);
    if (firm_count == 0 && consumer_count == 0) {
        error("No economic agents present");
    }
}

#ifdef DEBUG
template<class ModelVariant>
void ModelInitializer<ModelVariant>::print_network_characteristics() const {
    FloatType average_transport_delay = 0;
    unsigned int region_wo_firm_count = 0;
    for (auto& region : model()->regions) {
        FloatType average_transport_delay_region = 0;
        unsigned int firm_count = 0;
        for (auto& economic_agent : region->economic_agents) {
            if (economic_agent->type == EconomicAgent<ModelVariant>::Type::FIRM) {
                FloatType average_tranport_delay_economic_agent = 0;
                Firm<ModelVariant>* firm = economic_agent->as_firm();
                for (auto& business_connection : firm->sales_manager->business_connections) {
                    average_tranport_delay_economic_agent += business_connection->get_transport_delay_tau();
                }
                assert(!economic_agent->as_firm()->sales_manager->business_connections.empty());
                average_tranport_delay_economic_agent /= FloatType(firm->sales_manager->business_connections.size());
                ++firm_count;
                average_transport_delay_region += average_tranport_delay_economic_agent;
            }
        }
        if (firm_count > 0) {
            average_transport_delay_region /= FloatType(firm_count);
#ifdef CLEANUP_INFO
            info(region->id() << ": number of firms: " << firm_count << " average transport delay: " << average_transport_delay_region);
#endif
            average_transport_delay += average_transport_delay_region;
        } else {
            ++region_wo_firm_count;
            warning(region->id() << ": no firm");
        }
    }
    average_transport_delay /= FloatType(model()->regions.size() - region_wo_firm_count);
#ifdef CLEANUP_INFO
    info("Average transport delay: " << average_transport_delay);
#endif
}
#endif

template<class ModelVariant>
void ModelInitializer<ModelVariant>::read_transport_network_netcdf(const std::string& filename) {
    const settings::SettingsNode& transport = settings["transport"];
    const auto aviation_speed = transport["aviation_speed"].as<FloatType>();
    const auto road_speed = transport["road_speed"].as<FloatType>();
    const auto sea_speed = transport["sea_speed"].as<FloatType>();
    const auto port_delay = transport["port_delay"].as<TransportDelay>();
    const auto road_km_costs = transport["road_km_costs"].as<FloatType>();
    const auto sea_km_costs = transport["sea_km_costs"].as<FloatType>();

    std::vector<std::unique_ptr<TemporaryGeoEntity>> points;
    std::vector<std::unique_ptr<TemporaryGeoEntity>> final_connections;
    std::vector<std::size_t> input_indices;
    std::unordered_map<std::string, std::size_t> point_indices;

    netCDF::NcFile file(filename, netCDF::NcFile::read, netCDF::NcFile::nc4);

    const auto types_size = file.getDim("typeindex").getSize();
    std::vector<char*> typenames(types_size);
    file.getVar("typeindex").getVar(&typenames[0]);
    char type_port = -1;
    char type_region = -1;
    char type_sea = -1;
    for (std::size_t i = 0; i < types_size; ++i) {
        const std::string s(typenames[i]);
        if (s == "port") {
            type_port = i;
        } else if (s == "sea") {
            type_sea = i;
        } else if (s == "region") {
            type_region = i;
        } else {
            error("Unknown transport node type '" << s << "'");
        }
    }
    if (type_port < 0) {
        error("No transport node type for ports found");
    }
    if (type_region < 0) {
        error("No transport node type for regions found");
    }
    if (type_sea < 0) {
        error("No transport node type for seas found");
    }

    const auto input_size = file.getDim("index").getSize();
    std::vector<char*> ids(input_size);
    std::vector<unsigned char> types(input_size);
    std::vector<double> latitudes(input_size);
    std::vector<double> longitudes(input_size);
    std::vector<unsigned char> connections(input_size * input_size);

    file.getVar("index").getVar(&ids[0]);
    file.getVar("type").getVar(&types[0]);
    file.getVar("latitude").getVar(&latitudes[0]);
    file.getVar("longitude").getVar(&longitudes[0]);
    file.getVar("connections").getVar(&connections.data()[0]);
    file.close();

    for (std::size_t i = 0; i < input_size; ++i) {
        GeoLocation<ModelVariant>* location = nullptr;
        if (types[i] == type_region) {
            location = model()->find_region(ids[i]);
        } else if (types[i] == type_port) {
            location = new GeoLocation<ModelVariant>(model(), port_delay, GeoLocation<ModelVariant>::Type::PORT, ids[i]);
        } else if (types[i] == type_sea) {
            location = new GeoLocation<ModelVariant>(model(), 0, GeoLocation<ModelVariant>::Type::SEA, ids[i]);
        }
        if (location) {
            std::unique_ptr<GeoPoint> centroid(new GeoPoint(longitudes[i], latitudes[i]));
            location->set_centroid(centroid);
            points.emplace_back(new TemporaryGeoEntity(location, types[i] == type_region));
            point_indices[ids[i]] = points.size() - 1;
            input_indices.emplace_back(i);
        }
    }

    // create direct connections
    const auto size = points.size();
    std::vector<Path> paths(size * size, Path());
    for (std::size_t i = 0; i < size; ++i) {
        auto& p1 = points[i];
        auto l1 = static_cast<GeoLocation<ModelVariant>*>(p1->entity());
        for (std::size_t j = 0; j < i; ++j) {  // assume connections is symmetric
            if (connections[input_indices[i] * input_size + input_indices[j]]) {
                auto& p2 = points[j];
                auto l2 = static_cast<GeoLocation<ModelVariant>*>(p2->entity());
                FloatType costs;
                TransportDelay delay;
                typename GeoConnection<ModelVariant>::Type type;
                const auto distance = l1->centroid()->distance_to(*l2->centroid());
                if (l1->type == GeoLocation<ModelVariant>::Type::SEA || l2->type == GeoLocation<ModelVariant>::Type::SEA) {
                    delay = iround(distance / sea_speed / 24. / to_float(model()->delta_t()));
                    type = GeoConnection<ModelVariant>::Type::SEAROUTE;
                    costs = sea_km_costs * distance;
                } else {
                    delay = iround(distance / road_speed / 24. / to_float(model()->delta_t()));
                    type = GeoConnection<ModelVariant>::Type::ROAD;
                    costs = road_km_costs * distance;
                }
                // connections is symmetric -> connection needs to be used twice to make sure it's the same object
                TemporaryGeoEntity* c = new TemporaryGeoEntity(new GeoConnection<ModelVariant>(model(), delay, type, l1, l2), false);
                paths[i * size + j] = Path(costs, p1.get(), p2.get(), c);
                paths[j * size + i] = Path(costs, p2.get(), p1.get(), c);
                final_connections.emplace_back(c);
            }
        }
    }

    // find cheapest connections
    bool done = false;
    while (!done) {
        info("Find cheapest paths iteration...");
        done = true;
        for (std::size_t i = 0; i < size; ++i) {
            for (std::size_t j = 0; j < size; ++j) {
                Path* p = &paths[i * size + j];
                for (std::size_t via = 0; via < size; ++via) {
                    const auto& p1 = paths[i * size + via];
                    if (!p1.empty()) {
                        const auto& p2 = paths[via * size + j];
                        if (!p2.empty()) {
                            if (p->empty() || p1.costs() + p2.costs() < p->costs()) {
                                paths[i * size + j] = p1 + p2;
                                p = &paths[i * size + j];
                                done = false;
                            }
                        }
                    }
                }
            }
        }
    }

    // mark everything used
    bool found = true;
    while (found) {
        found = false;
        for (std::size_t i = 0; i < size; ++i) {
            auto& p1 = points[i];
            if (p1->used) {  // regions are already marked used
                for (std::size_t j = 0; j < size; ++j) {
                    auto& p2 = points[j];
                    if (p2->used) {  // regions are already marked used
                        auto& path = paths[i * size + j].points();
                        if (path.empty()) {
                            error("No roadsea transport connection from " << ids[input_indices[i]] << " to " << ids[input_indices[j]]);
                        } else {
                            for (std::size_t k = 1; k < path.size() - 1; ++k) {
                                found = found || !path[k]->used;
                                path[k]->used = true;
                            }
                        }
                    }
                }
            }
        }
    }

    // add connections to locations actually used and create routes between regions
    for (std::size_t i = 0; i < size; ++i) {
        auto& p1 = points[i];
        if (p1->used) {
            auto l1 = static_cast<GeoLocation<ModelVariant>*>(p1->entity());
            if (l1->type != GeoLocation<ModelVariant>::Type::REGION) {
                model()->other_locations.emplace_back(l1);
            }
            for (std::size_t j = 0; j < size; ++j) {
                auto& p2 = points[j];
                if (p2->used) {
                    auto l2 = static_cast<GeoLocation<ModelVariant>*>(p2->entity());
                    auto& path = paths[i * size + j].points();
                    if (!path.empty()) {
                        if (path.size() == 3 && i < j) {  // direct connection, only once per i/j combination
                            auto c = std::shared_ptr<GeoConnection<ModelVariant>>(static_cast<GeoConnection<ModelVariant>*>(path[1]->entity()));
                            l1->connections.push_back(c);
                            l2->connections.push_back(c);
                        }
                        if (l1->type == GeoLocation<ModelVariant>::Type::REGION && l2->type == GeoLocation<ModelVariant>::Type::REGION) {
                            // create roadsea route
                            auto r1 = static_cast<Region<ModelVariant>*>(l1);
                            auto r2 = static_cast<Region<ModelVariant>*>(l2);
                            GeoRoute<ModelVariant> route;
                            route.path.resize(path.size() - 2);
                            for (std::size_t k = 1; k < path.size() - 1; ++k) {
                                route.path[k - 1] = path[k]->entity();
                            }
                            r1->routes.emplace(std::make_pair(r2->index(), Sector<ModelVariant>::TransportType::ROADSEA), route);
                        }
                    }
                    if (l1->type == GeoLocation<ModelVariant>::Type::REGION && l2->type == GeoLocation<ModelVariant>::Type::REGION) {
                        // create aviation route
                        const auto distance = l1->centroid()->distance_to(*l2->centroid());
                        const auto delay = iround(distance / aviation_speed / 24. / to_float(model()->delta_t()));
                        auto c = std::make_shared<GeoConnection<ModelVariant>>(model(), delay, GeoConnection<ModelVariant>::Type::AVIATION, l1, l2);
                        l1->connections.push_back(c);
                        l2->connections.push_back(c);
                        auto r1 = static_cast<Region<ModelVariant>*>(l1);
                        auto r2 = static_cast<Region<ModelVariant>*>(l2);
                        GeoRoute<ModelVariant> route;
                        route.path.emplace_back(c.get());
                        r1->routes.emplace(std::make_pair(r2->index(), Sector<ModelVariant>::TransportType::AVIATION), route);
                    }
                }
            }
        }
    }
}

template<class ModelVariant>
void ModelInitializer<ModelVariant>::read_centroids_netcdf(const std::string& filename) {
    {
        netCDF::NcFile file(filename, netCDF::NcFile::read);

        std::size_t regions_count = file.getDim("region").getSize();

        netCDF::NcVar regions_var = file.getVar("region");
        std::vector<const char*> regions_val(regions_count);
        regions_var.getVar(&regions_val[0]);

        netCDF::NcVar lons_var = file.getVar("lon");
        std::vector<float> lons_val(regions_count);
        lons_var.getVar(&lons_val[0]);

        netCDF::NcVar lats_var = file.getVar("lat");
        std::vector<float> lats_val(regions_count);
        lats_var.getVar(&lats_val[0]);

        for (std::size_t i = 0; i < regions_count; ++i) {
            Region<ModelVariant>* region = model()->find_region(regions_val[i]);
            if (region) {
                std::unique_ptr<GeoPoint> centroid(new GeoPoint(lons_val[i], lats_val[i]));
                region->set_centroid(centroid);
            }
        }
        file.close();
    }

    const settings::SettingsNode& transport = settings["transport"];
    const auto threshold_road_transport = transport["threshold_road_transport"].as<FloatType>();
    const auto aviation_speed = transport["aviation_speed"].as<FloatType>();
    const auto road_speed = transport["road_speed"].as<FloatType>();
    const auto sea_speed = transport["sea_speed"].as<FloatType>();

    for (auto& region_from : model()->regions) {
        if (!region_from->centroid()) {
            error("Centroid for " << region_from->id() << " not found");
        }
        for (auto& region_to : model()->regions) {
            if (region_from == region_to) {
                break;
            }
            const auto distance = region_from->centroid()->distance_to(*region_to->centroid());
            // create roadsea route
            {
                TransportDelay transport_delay;
                typename GeoConnection<ModelVariant>::Type type;
                if (distance >= threshold_road_transport) {
                    transport_delay = iround(distance / sea_speed / 24. / to_float(model()->delta_t()));
                    type = GeoConnection<ModelVariant>::Type::SEAROUTE;
                } else {
                    transport_delay = iround(distance / road_speed / 24. / to_float(model()->delta_t()));
                    type = GeoConnection<ModelVariant>::Type::ROAD;
                }
                if (transport_delay > 0) {
                    --transport_delay;
                }
                auto inf = std::make_shared<GeoConnection<ModelVariant>>(model(), transport_delay, type, region_from.get(), region_to.get());
                region_from->connections.push_back(inf);
                region_to->connections.push_back(inf);

                GeoRoute<ModelVariant> route;
                route.path.emplace_back(inf.get());
                region_from->routes.emplace(std::make_pair(region_to->index(), Sector<ModelVariant>::TransportType::ROADSEA), route);
                region_to->routes.emplace(std::make_pair(region_from->index(), Sector<ModelVariant>::TransportType::ROADSEA), route);
            }
            // create aviation route
            {
                TransportDelay transport_delay = iround(distance / aviation_speed / 24. / to_float(model()->delta_t()));
                if (transport_delay > 0) {
                    --transport_delay;
                }
                auto inf = std::make_shared<GeoConnection<ModelVariant>>(model(), transport_delay, GeoConnection<ModelVariant>::Type::AVIATION,
                                                                         region_from.get(), region_to.get());
                region_from->connections.push_back(inf);
                region_to->connections.push_back(inf);

                GeoRoute<ModelVariant> route;
                route.path.emplace_back(inf.get());
                region_from->routes.emplace(std::make_pair(region_to->index(), Sector<ModelVariant>::TransportType::AVIATION), route);
                region_to->routes.emplace(std::make_pair(region_from->index(), Sector<ModelVariant>::TransportType::AVIATION), route);
            }
        }
    }
}

template<class ModelVariant>
void ModelInitializer<ModelVariant>::create_simple_transport_connection(Region<ModelVariant>* region_from,
                                                                        Region<ModelVariant>* region_to,
                                                                        TransportDelay transport_delay) {
    auto inf = std::make_shared<GeoConnection<ModelVariant>>(model(), transport_delay, GeoConnection<ModelVariant>::Type::UNSPECIFIED, region_from, region_to);
    region_from->connections.push_back(inf);
    region_to->connections.push_back(inf);

    GeoRoute<ModelVariant> route;
    route.path.emplace_back(inf.get());
    region_from->routes.emplace(std::make_pair(region_to->index(), Sector<ModelVariant>::TransportType::AVIATION), route);
    region_from->routes.emplace(std::make_pair(region_to->index(), Sector<ModelVariant>::TransportType::ROADSEA), route);
    region_to->routes.emplace(std::make_pair(region_from->index(), Sector<ModelVariant>::TransportType::AVIATION), route);
    region_to->routes.emplace(std::make_pair(region_from->index(), Sector<ModelVariant>::TransportType::ROADSEA), route);
}

template<class ModelVariant>
void ModelInitializer<ModelVariant>::read_transport_times_csv(const std::string& index_filename, const std::string& filename) {
    std::vector<Region<ModelVariant>*> regions;
    std::ifstream index_file(index_filename.c_str());
    if (!index_file) {
        error("Could not open index file '" << index_filename << "'");
    }
    unsigned int index = 0;
    while (true) {
        std::string line;
        if (!getline(index_file, line)) {
            break;
        }
        if (line.empty() || (line.length() == 1 && line[0] == '\r')) {
            continue;
        }
        std::istringstream ss(line);

        std::string region_name;
        if (!getline(ss, region_name, ',')) {
            error("Unexpected end in index file");
        }
        if (!region_name.empty() && region_name[region_name.size() - 1] == '\r') {
            region_name.erase(region_name.size() - 1);
        }

        Region<ModelVariant>* region = model()->find_region(region_name);
        regions.push_back(region);  // might be == nullptr
        ++index;
    }

    std::ifstream transport_delays_file(filename.c_str());
    if (!transport_delays_file) {
        error("Could not open transport delays file '" << filename << "'");
    }

    TransportDelay transport_delay_tau = 1;
    std::string transport_line;

    for (std::size_t row = 0; row < regions.size(); ++row) {
        if (!getline(transport_delays_file, transport_line)) {
            error("Index and transport_delays are not consistent: Not enough rows");
        }
        if (transport_line.empty() || (transport_line.length() == 1 && transport_line[0] == '\r')) {
            --row;
            continue;
        }

        auto region_from = regions[row];
        if (region_from) {
            std::istringstream transport_string_stream(transport_line);
            std::string transport_str;

            for (std::size_t col = 0; col < row; ++col) {
                if (!getline(transport_string_stream, transport_str, ',')) {
                    error("Index and transport_delays are not consistent: Not enough columns in row " << row);
                }
                if (!transport_str.empty() && transport_str[transport_str.size() - 1] == '\r') {
                    transport_str.erase(transport_str.size() - 1);
                }

                auto region_to = regions[col];
                if (region_to && region_from != region_to) {
                    transport_delay_tau = std::stoi(transport_str);
                    if (transport_delay_tau <= 0) {
                        error("Transport delay not valid: " << transport_delay_tau << " in col " << col << " in row " << row);
                    }
                    create_simple_transport_connection(region_from, region_to, transport_delay_tau - 1);  // -1 for backwards compatibility
                }
            }
        }
    }
}

template<class ModelVariant>
void ModelInitializer<ModelVariant>::build_artificial_network() {
    const settings::SettingsNode& network = settings["network"];
    const auto closed = network["closed"].as<bool>(false);
    const auto skewness = network["skewness"].as<unsigned int>();
    if (skewness < 1) {
        error("Skewness must be >= 1");
    }
    const auto sectors_cnt = network["sectors"].as<unsigned int>();
    for (std::size_t i = 0; i < sectors_cnt; ++i) {
        add_sector("SEC" + std::to_string(i + 1));
    }
    const auto regions_cnt = network["regions"].as<unsigned int>();
    for (std::size_t i = 0; i < regions_cnt; ++i) {
        Region<ModelVariant>* region = add_region("RG" + std::to_string(i));
        add_consumer(region);
        for (std::size_t i = 0; i < sectors_cnt; ++i) {
            add_firm(model()->sectors[i + 1].get(), region);
        }
    }
    build_transport_network();
    const Flow flow = Flow(FlowQuantity(1.0), Price(1.0));
    const Flow double_flow = Flow(FlowQuantity(2.0), Price(1.0));
    for (std::size_t r = 0; r < regions_cnt; ++r) {
        for (std::size_t i = 0; i < sectors_cnt; ++i) {
            info(model()->sectors[i + 1]->firms[r]->id()
                 << "->" << model()->sectors[(i + 1) % sectors_cnt + 1]->firms[r]->id() << " = " << flow.get_quantity());
            initialize_connection(model()->sectors[i + 1]->firms[r], model()->sectors[(i + 1) % sectors_cnt + 1]->firms[r], flow);
            if (!closed && r == regions_cnt - 1) {
                info(model()->sectors[i + 1]->firms[r]->id()
                     << "->" << model()->find_consumer(model()->regions[r].get())->id() << " = " << double_flow.get_quantity());
                initialize_connection(model()->sectors[i + 1]->firms[r], model()->find_consumer(model()->regions[r].get()), double_flow);
            } else {
                info(model()->sectors[i + 1]->firms[r]->id()
                     << "->" << model()->find_consumer(model()->regions[r].get())->id() << " = " << flow.get_quantity());
                initialize_connection(model()->sectors[i + 1]->firms[r], model()->find_consumer(model()->regions[r].get()), flow);
            }
            if (closed || r < regions_cnt - 1) {
                info(model()->sectors[i + 1]->firms[r]->id()
                     << "->" << model()->sectors[(i + skewness) % sectors_cnt + 1]->firms[(r + 1) % regions_cnt]->id() << " = " << flow.get_quantity());
                initialize_connection(model()->sectors[i + 1]->firms[r], model()->sectors[(i + skewness) % sectors_cnt + 1]->firms[(r + 1) % regions_cnt],
                                      flow);
            }
        }
    }
}

template<class ModelVariant>
void ModelInitializer<ModelVariant>::build_agent_network_from_table(const mrio::Table<FloatType, std::size_t>& table, FloatType flow_threshold) {

    std::vector<EconomicAgent<ModelVariant>*> economic_agents;
    economic_agents.reserve(table.index_set().size());

    for (const auto& index : table.index_set().total_indices) {
        const std::string& region_name = index.region->name;
        const std::string& sector_name = index.sector->name;
        Region<ModelVariant>* region = add_region(region_name);
        if (sector_name == "FCON") {
            Consumer<ModelVariant>* consumer = model()->find_consumer(region);
            if (!consumer) {
                consumer = add_consumer(region);
                economic_agents.push_back(consumer);
            } else {
                error("Duplicate consumer for region " << region_name);
            }
        } else {
            Sector<ModelVariant>* sector = add_sector(sector_name);
            Firm<ModelVariant>* firm = model()->find_firm(sector, region->id());
            if (!firm) {
                firm = add_firm(sector, region);
                if (!firm) {
                    error("Could not add firm");
                }
            }
            economic_agents.push_back(firm);
        }
    }

    build_transport_network();

    const Ratio time_factor = model()->delta_t() / Time(365.0);
    const FlowQuantity daily_flow_threshold = round(FlowQuantity(flow_threshold * time_factor));
    auto d = table.raw_data().begin();
    for (auto& source : economic_agents) {
        if (source->type == EconomicAgent<ModelVariant>::Type::FIRM) {
            Firm<ModelVariant>* firm_from = source->as_firm();
            for (auto& target : economic_agents) {
                const FlowQuantity flow_quantity = round(FlowQuantity(*d) * time_factor);
                if (flow_quantity > daily_flow_threshold) {
                    initialize_connection(firm_from, target, flow_quantity);
                }
                ++d;
            }
        } else {
            d += table.index_set().size();
        }
    }
}

template<class ModelVariant>
void ModelInitializer<ModelVariant>::build_agent_network() {
    const settings::SettingsNode& network = settings["network"];
    const auto& type = network["type"].as<settings::hstring>();
    switch (type) {
        case settings::hstring::hash("csv"): {
            const auto& index_filename = network["index"].as<std::string>();
            std::ifstream index_file(index_filename.c_str(), std::ios::in | std::ios::binary);
            if (!index_file) {
                error("Could not open index file '" << index_filename << "'");
            }
            const auto& filename = network["file"].as<std::string>();
            std::ifstream flows_file(filename.c_str(), std::ios::in | std::ios::binary);
            if (!flows_file) {
                error("Could not open flows file '" << filename << "'");
            }
            mrio::Table<FloatType, std::size_t> table;
            const auto flow_threshold = network["threshold"].as<FloatType>();
            table.read_from_csv(index_file, flows_file, flow_threshold);
            flows_file.close();
            index_file.close();
            build_agent_network_from_table(table, flow_threshold);
        } break;
        case settings::hstring::hash("netcdf"): {
            const auto& filename = network["file"].as<std::string>();
            mrio::Table<FloatType, std::size_t> table;
            const auto flow_threshold = network["threshold"].as<FloatType>();
            table.read_from_netcdf(filename, flow_threshold);
            build_agent_network_from_table(table, flow_threshold);
        } break;
        case settings::hstring::hash("artificial"): {
            build_artificial_network();
            return;
        } break;
        default:
            error("Unknown network type '" << type << "'");
    }
}

template<class ModelVariant>
void ModelInitializer<ModelVariant>::build_transport_network() {
    const settings::SettingsNode& transport = settings["transport"];
    const auto& type = transport["type"].as<settings::hstring>();
    switch (type) {
        case settings::hstring::hash("const"): {
            auto transport_delay = transport["value"].as<TransportDelay>();
            for (auto& region_from : model()->regions) {
                for (auto& region_to : model()->regions) {
                    if (region_to == region_from) {
                        break;
                    }
                    create_simple_transport_connection(region_from.get(), region_to.get(), transport_delay);
                }
            }
        } break;
        case settings::hstring::hash("csv"):
            read_transport_times_csv(transport["index"].as<std::string>(), transport["file"].as<std::string>());
            break;
        case settings::hstring::hash("centroids"):
            read_centroids_netcdf(transport["file"].as<std::string>());
            break;
        case settings::hstring::hash("network"):
            read_transport_network_netcdf(transport["file"].as<std::string>());
            break;
        default:
            error("Unknown transport type '" << type << "'");
    }
}

template<class ModelVariant>
void ModelInitializer<ModelVariant>::initialize() {
    pre_initialize_variant();
    build_agent_network();
    clean_network();
    post_initialize_variant();
}

#ifdef VARIANT_BASIC
template<>
void ModelInitializer<VariantBasic>::pre_initialize_variant() {}
#endif

#ifdef VARIANT_DEMAND
template<>
void ModelInitializer<VariantDemand>::pre_initialize_variant() {
    const settings::SettingsNode& parameters = settings["model"];
    model()->parameters_writable().history_weight = parameters["history_weight"].as<Ratio>();
}
#endif

template<class ModelVariant>
void ModelInitializer<ModelVariant>::pre_initialize_variant() {
    const settings::SettingsNode& parameters = settings["model"];
    model()->parameters_writable().transport_penalty_small = parameters["transport_penalty_small"].as<Price>();
    model()->parameters_writable().transport_penalty_large = parameters["transport_penalty_large"].as<Price>();
    model()->parameters_writable().optimization_maxiter = parameters["optimization_maxiter"].as<unsigned int>();
    model()->parameters_writable().optimization_timeout = parameters["optimization_timeout"].as<unsigned int>();
    model()->parameters_writable().quadratic_transport_penalty = parameters["quadratic_transport_penalty"].as<bool>();
    model()->parameters_writable().maximal_decrease_reservation_price_limited_by_markup =
        parameters["maximal_decrease_reservation_price_limited_by_markup"].as<bool>();
    model()->parameters_writable().always_extend_expected_demand_curve = parameters["always_extend_expected_demand_curve"].as<bool>();
    model()->parameters_writable().naive_expectations = parameters["naive_expectations"].as<bool>();
    model()->parameters_writable().deviation_penalty = parameters["deviation_penalty"].as<bool>(false);
    model()->parameters_writable().cost_correction = parameters["cost_correction"].as<bool>();
    model()->parameters_writable().min_storage = parameters["min_storage"].as<Ratio>(0.0);
    model()->parameters_writable().cheapest_price_range_preserve_seller_price = parameters["cheapest_price_range_preserve_seller_price"].as<bool>(false);
    model()->parameters_writable().cheapest_price_range_generic_size = (parameters["cheapest_price_range_width"].as<std::string>() == "auto");
    if (!model()->parameters_writable().cheapest_price_range_generic_size) {
        model()->parameters_writable().cheapest_price_range_width = parameters["cheapest_price_range_width"].as<Price>();
    }
    model()->parameters_writable().relative_transport_penalty = parameters["relative_transport_penalty"].as<bool>();
    const auto& optimization_algorithm = parameters["optimization_algorithm"].as<settings::hstring>("slsqp");
    switch (optimization_algorithm) {
        case settings::hstring::hash("slsqp"):
            model()->parameters_writable().optimization_algorithm = nlopt::LD_SLSQP;
            break;
        case settings::hstring::hash("mma"):
            model()->parameters_writable().optimization_algorithm = nlopt::LD_MMA;
            break;
        case settings::hstring::hash("ccsaq"):
            model()->parameters_writable().optimization_algorithm = nlopt::LD_CCSAQ;
            break;
        case settings::hstring::hash("lbfgs"):
            model()->parameters_writable().optimization_algorithm = nlopt::LD_LBFGS;
            break;
        case settings::hstring::hash("tnewton_precond_restart"):
            model()->parameters_writable().optimization_algorithm = nlopt::LD_TNEWTON_PRECOND_RESTART;
            break;
        case settings::hstring::hash("tnewton_precond"):
            model()->parameters_writable().optimization_algorithm = nlopt::LD_TNEWTON_PRECOND;
            break;
        case settings::hstring::hash("tnewton_restart"):
            model()->parameters_writable().optimization_algorithm = nlopt::LD_TNEWTON_RESTART;
            break;
        case settings::hstring::hash("tnewton"):
            model()->parameters_writable().optimization_algorithm = nlopt::LD_TNEWTON;
            break;
        case settings::hstring::hash("var1"):
            model()->parameters_writable().optimization_algorithm = nlopt::LD_VAR1;
            break;
        case settings::hstring::hash("var2"):
            model()->parameters_writable().optimization_algorithm = nlopt::LD_VAR2;
            break;
        default:
            error("unknown optimization alorithm '" << optimization_algorithm << "'");
    }
}

#ifdef VARIANT_BASIC
template<>
void ModelInitializer<VariantBasic>::post_initialize_variant() {}
#endif

#ifdef VARIANT_DEMAND
template<>
void ModelInitializer<VariantDemand>::post_initialize_variant() {}
#endif

template<class ModelVariant>
void ModelInitializer<ModelVariant>::post_initialize_variant() {
    // initialize price dependent members of each capacity manager, which can only be calculated after the whole network has been initialized
    for (auto& sector : model()->sectors) {
        for (auto& firm : sector->firms) {
            firm->sales_manager->initialize();
        }
    }
}

INSTANTIATE_BASIC(ModelInitializer);
}  // namespace acclimate

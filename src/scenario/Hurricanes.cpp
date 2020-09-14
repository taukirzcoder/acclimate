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

#include "scenario/Hurricanes.h"

#include <memory>
#include <vector>

#include "model/EconomicAgent.h"
#include "model/Region.h"
#include "settingsnode.h"

namespace acclimate {

Hurricanes::Hurricanes(const settings::SettingsNode& settings_p, const settings::SettingsNode& scenario_node_p, Model* model_p)
    : RasteredScenario<FloatType>(settings_p, scenario_node_p, model_p) {
    threshold = scenario_node["forcing"]["threshold"].as<double>();
}

FloatType Hurricanes::new_region_forcing(Region* region) const {
    UNUSED(region);
    return 0.0;
}

void Hurricanes::reset_forcing(Region* region, FloatType& forcing) const {
    UNUSED(region);
    forcing = 0.0;
}

void Hurricanes::set_region_forcing(Region* region, const FloatType& forcing, FloatType proxy_sum) const {
    for (auto& it : region->economic_agents) {
        if (it->type == EconomicAgent::Type::FIRM) {
            it->set_forcing(1.0 - forcing / proxy_sum);
        }
    }
}

void Hurricanes::add_cell_forcing(
    FloatType x, FloatType y, FloatType proxy_value, FloatType cell_forcing, const Region* region, FloatType& region_forcing) const {
    UNUSED(region);
    UNUSED(x);
    UNUSED(y);
    region_forcing += cell_forcing > threshold ? proxy_value : 0.0;
}

}  // namespace acclimate

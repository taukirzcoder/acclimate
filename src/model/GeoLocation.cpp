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

#include "model/GeoLocation.h"
#include <algorithm>
#include "model/GeoConnection.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
GeoLocation<ModelVariant>::GeoLocation(TransportDelay delay_p, GeoLocation<ModelVariant>::Type type_p, std::string name_p)
    : GeoEntity<ModelVariant>(delay_p, GeoEntity<ModelVariant>::Type::LOCATION), type(type_p), name_m(name_p) {}

template<class ModelVariant>
void GeoLocation<ModelVariant>::remove_connection(const GeoConnection<ModelVariant>* connection) {
    auto it = std::find_if(connections.begin(), connections.end(),
                           [connection](const std::shared_ptr<GeoConnection<ModelVariant>>& it) { return it.get() == connection; });
    connections.erase(it);
}
template<class ModelVariant>
GeoLocation<ModelVariant>::~GeoLocation() {
    std::cout << "Reseting GL: " << this << std::endl;
    std::cout << "Reseting GL: " << std::string(*this) << std::endl;
    //~ std::cout << "Loc: " << std::string(*this) << std::endl;
}

INSTANTIATE_BASIC(GeoLocation);
}  // namespace acclimate

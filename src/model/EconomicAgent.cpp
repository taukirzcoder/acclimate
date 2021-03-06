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

#include "model/EconomicAgent.h"
#include <algorithm>
#include "model/Sector.h"
#include "model/Storage.h"
#include "variants/ModelVariants.h"

namespace acclimate {

template<class ModelVariant>
EconomicAgent<ModelVariant>::EconomicAgent(Sector<ModelVariant>* sector_p, Region<ModelVariant>* region_p, const EconomicAgent<ModelVariant>::Type& type_p)
    : sector(sector_p), region(region_p), type(type_p) {}

template<class ModelVariant>
inline Firm<ModelVariant>* EconomicAgent<ModelVariant>::as_firm() {
    assert(type == Type::FIRM);
    return nullptr;
}

template<class ModelVariant>
inline Consumer<ModelVariant>* EconomicAgent<ModelVariant>::as_consumer() {
    assert(type == Type::CONSUMER);
    return nullptr;
}

template<class ModelVariant>
inline const Firm<ModelVariant>* EconomicAgent<ModelVariant>::as_firm() const {
    assert(type == Type::FIRM);
    return nullptr;
}

template<class ModelVariant>
inline const Consumer<ModelVariant>* EconomicAgent<ModelVariant>::as_consumer() const {
    assert(type == Type::CONSUMER);
    return nullptr;
}

template<class ModelVariant>
Storage<ModelVariant>* EconomicAgent<ModelVariant>::find_input_storage(const std::string& sector_name) const {
    for (const auto& is : input_storages) {
        if (is->sector->id() == sector_name) {
            return is.get();
        }
    }
    return nullptr;
}

template<class ModelVariant>
void EconomicAgent<ModelVariant>::remove_storage(Storage<ModelVariant>* storage) {
    auto it =
        std::find_if(input_storages.begin(), input_storages.end(), [storage](const std::unique_ptr<Storage<ModelVariant>>& it) { return it.get() == storage; });
    input_storages.erase(it);
}

INSTANTIATE_BASIC(EconomicAgent);
}  // namespace acclimate

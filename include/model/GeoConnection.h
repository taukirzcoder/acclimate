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

#ifndef ACCLIMATE_GEOCONNECTION_H
#define ACCLIMATE_GEOCONNECTION_H

#include "acclimate.h"
#include "model/GeoEntity.h"

namespace acclimate {

template<class ModelVariant>
class GeoLocation;

template<class ModelVariant>
class GeoConnection : public GeoEntity<ModelVariant> {
  public:
    enum class Type { ROAD, AVIATION, SEAROUTE, UNSPECIFIED };

  public:
    const GeoLocation<ModelVariant>* location1;
    const GeoLocation<ModelVariant>* location2;
    const Type type;
	~GeoConnection();
    GeoConnection<ModelVariant>(TransportDelay delay, Type type_p, const GeoLocation<ModelVariant>* location1_p, const GeoLocation<ModelVariant>* location2_p);
    operator std::string() const { return std::string(*location1) + "-" + std::string(*location2); }
};
}  // namespace acclimate

#endif

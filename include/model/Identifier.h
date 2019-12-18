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

#ifndef ACCLIMATE_IDENTIFIER_H
#define ACCLIMATE_IDENTIFIER_H

#include <string>
#include <vector>
#include "run.h"
#include "types.h"

namespace acclimate {

    template<class ModelVariant>
    class Model;
    template<class ModelVariant>
    class Firm;
    template<class ModelVariant>
    class Identifier {
        friend class Model<ModelVariant>;
    protected:
        const IntType index_m;
        const std::string id_m;
        Model<ModelVariant> *const model_m;
    public:
        Identifier(Model<ModelVariant> *model_p,
                   std::string id_p,
                   IntType index_p);
        inline const std::string& id() const { return id_m; }

        std::vector<Firm<ModelVariant>*> firms;
    }; // namespace acclimate
}
#endif
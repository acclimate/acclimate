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

#include "checkpointing.h"

#include <dmtcp.h>
#include <unistd.h>

#include <chrono>
#include <csignal>
#include <ostream>
#include <thread>

#include "acclimate.h"

namespace acclimate::checkpoint {

static bool is_instantiated = false;
volatile bool is_scheduled = false;

static void handle_sigterm(int /* signal */) { is_scheduled = true; }

void initialize() {
    if (is_instantiated) {
        throw log::error("Only one model run instance supported when checkpointing");
    }
    std::signal(SIGTERM, handle_sigterm);
    close(10);
    close(11);

    is_instantiated = true;
}

void write() {
    std::size_t original_generation = dmtcp_get_generation();
    int retval = dmtcp_checkpoint();
    switch (retval) {
        case DMTCP_AFTER_CHECKPOINT:
            do {
                std::this_thread::sleep_for(std::chrono::seconds(1));
            } while (dmtcp_get_generation() == original_generation);
            throw return_after_checkpoint();
        case DMTCP_AFTER_RESTART:
            break;
        case DMTCP_NOT_PRESENT:
            throw log::error("dmtcp not present");
        default:
            throw log::error("unknown dmtcp result ", retval);
    }
    is_scheduled = false;
}

};  // namespace acclimate::checkpoint

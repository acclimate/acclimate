// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

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

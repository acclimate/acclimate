// SPDX-FileCopyrightText: Acclimate authors
//
// SPDX-License-Identifier: AGPL-3.0-or-later

#ifndef ACCLIMATE_CHECKPOINTING_H
#define ACCLIMATE_CHECKPOINTING_H

namespace acclimate::checkpoint {

extern volatile bool is_scheduled;

extern void initialize();
extern void write();

}  // namespace acclimate::checkpoint

#endif

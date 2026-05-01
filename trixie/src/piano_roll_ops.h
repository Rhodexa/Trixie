// piano_roll_ops.h
// Piano Roll operator declarations and keymap.
//
// To add a new operation:
//   1. Implement OperatorType (+ optional OperatorState subtype) in piano_roll_ops.cpp
//   2. Add a wmKeyMapItem to PIANO_ROLL_KEYMAP_ITEMS in piano_roll_ops.cpp
//   That's it — nothing else needs to change.

#pragma once

#include "wm.h"

// Returns the Piano Roll keymap. Call from the window region's handle_event.
const wmKeyMap& piano_roll_keymap();

// editor.h
// Blender-style editor hierarchy: SpaceType → ARegionType → ARegion.
// Convention: draw/event functions are named <space>_<region>_draw / handle_event.

#pragma once

#include "box.h"
#include "input_queue.h"

struct NVGcontext;
struct SpacePianoRoll;
struct Song;
class Journal;

// Mirrors Blender's RGN_TYPE_* enum.
enum class RegionType {
    Header,     // toolbar / menu strip      (top, fixed height)
    TimeRuler,  // beat/bar ruler            (below header, fixed height)
    Channels,   // piano keys sidebar        (left, fixed width)
    Window,     // main note grid            (fills remainder)
    UI,         // per-note params           (bottom, collapsible)
    Scrollbar,  // scroll track              (right edge, fixed width)
};

struct ARegion;

// Mirrors Blender's ARegionType — callbacks and metadata for one kind of region.
struct ARegionType {
    RegionType  type;
    const char* name;
    void (*draw)(NVGcontext*, ARegion&, const SpacePianoRoll&, const Song&);
    bool (*handle_event)(ARegion&, SpacePianoRoll&, Song&, Journal&, const InputEvent&);
};

// Mirrors Blender's ARegion — one live sub-area of an editor.
struct ARegion {
    RegionType   type;
    Box          winrct;        // screen-space rectangle, set each frame by layout
    ARegionType* runtime_type;  // pointer to type descriptor
};

// Mirrors Blender's SpaceType — descriptor for an editor type.
struct SpaceType {
    const char*   name;
    ARegionType*  region_types;
    int           region_type_count;
};

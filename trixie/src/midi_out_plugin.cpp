// midi_out_plugin.cpp
// Windows winmm MIDI output. Opens MIDI_MAPPER (the system's default MIDI device).

#include "midi_out_plugin.h"

#include <windows.h>
#include <mmsystem.h>
#include <cstdio>

static inline HMIDIOUT handle(void* p) { return static_cast<HMIDIOUT>(p); }

MidiOutPlugin::MidiOutPlugin() {
    HMIDIOUT h = nullptr;
    MMRESULT r = midiOutOpen(&h, MIDI_MAPPER, 0, 0, CALLBACK_NULL);
    if (r != MMSYSERR_NOERROR)
        fprintf(stderr, "trixie: midiOutOpen failed (error %u)\n", r);
    else
        handle_ = h;
}

MidiOutPlugin::~MidiOutPlugin() {
    if (!handle_) return;
    reset();
    midiOutClose(handle(handle_));
}

void MidiOutPlugin::note_on(int channel, int pitch, int velocity) {
    if (!handle_) return;
    DWORD msg = (0x90 | (channel & 0x0F))
              | ((pitch    & 0x7F) << 8)
              | ((velocity & 0x7F) << 16);
    midiOutShortMsg(handle(handle_), msg);
}

void MidiOutPlugin::note_off(int channel, int pitch) {
    if (!handle_) return;
    DWORD msg = (0x80 | (channel & 0x0F))
              | ((pitch & 0x7F) << 8);
    midiOutShortMsg(handle(handle_), msg);
}

void MidiOutPlugin::reset() {
    if (!handle_) return;
    for (int ch = 0; ch < 16; ch++) {
        // CC 123 = All Notes Off,  CC 120 = All Sound Off
        midiOutShortMsg(handle(handle_), (0xB0 | ch) | (123 << 8));
        midiOutShortMsg(handle(handle_), (0xB0 | ch) | (120 << 8));
    }
}

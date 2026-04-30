// midi_out_plugin.h
// Windows MIDI output plugin — routes note events to the default MIDI output device
// via the Windows Multimedia API (winmm).
// This is a plugin in the same sense as a future ChipSynthPlugin or VstPlugin:
// the playback engine doesn't know or care which one it's talking to.

#pragma once

#include "plugin.h"

class MidiOutPlugin : public InstrumentPlugin {
public:
    MidiOutPlugin();
    ~MidiOutPlugin();

    void note_on(int channel, int pitch, int velocity) override;
    void note_off(int channel, int pitch) override;
    void reset() override; // sends All Notes Off + All Sound Off on all 16 channels

private:
    void* handle_ = nullptr; // HMIDIOUT — opaque in the header to avoid pulling in windows.h
};

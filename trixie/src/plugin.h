// plugin.h
// Abstract instrument plugin interface.
// The playback engine fires note events here; what happens to them is the plugin's problem.
// MidiOutPlugin routes to the OS MIDI stack. Future: ChipSynthPlugin, VstPlugin, etc.

#pragma once

class InstrumentPlugin {
public:
    virtual ~InstrumentPlugin() = default;
    virtual void note_on(int channel, int pitch, int velocity) = 0;
    virtual void note_off(int channel, int pitch) = 0;
    virtual void reset() {} // silence everything; override for hardware MIDI
};

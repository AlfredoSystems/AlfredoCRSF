#pragma once

#include "AlfredoCRSF.h"

// One channel the transmitter drives to 1000, 1500 or 2000 us: a three-position
// switch, a two-position switch (never in the middle), or a pair of momentary
// buttons sharing a channel (one pulls it to 1000, the other to 2000).
//
//   CrsfSwitch arm(5);                        // channel 5
//   arm.update(crsf);                         // every loop, after crsf.update()
//   if (arm.is(CrsfSwitch::UP)) ...           // where it sits now
//   if (arm.movedTo(CrsfSwitch::UP)) ...      // this loop only: a button press
//
// Positions use thresholds (below 1250, above 1750), so a channel that does not
// land exactly on 1000/1500/2000 still reads correctly. The position at the
// first update() is not a move.
class CrsfSwitch
{
public:
    enum Position { DOWN, MIDDLE, UP };   // about 1000, 1500 and 2000 us

    explicit CrsfSwitch(uint8_t channel) : _channel(channel) {}

    uint8_t channel() const { return _channel; }

    void update(const AlfredoCRSF &crsf) { update(crsf.getChannel(_channel)); }

    // For channel values that come from somewhere other than an AlfredoCRSF
    void update(int us)
    {
        Position now = us < 1250 ? DOWN : us > 1750 ? UP : MIDDLE;
        _prev = _started ? _pos : now;
        _pos = now;
        _started = true;
    }

    Position position() const { return _pos; }
    bool is(Position p) const { return _pos == p; }
    bool moved() const { return _pos != _prev; }
    bool movedTo(Position p) const { return _pos == p && _prev != p; }     // pressed
    bool movedFrom(Position p) const { return _prev == p && _pos != p; }   // released

private:
    uint8_t _channel;
    Position _pos = MIDDLE;
    Position _prev = MIDDLE;
    bool _started = false;
};

// Copyright 2025 Luke Zulauf.
//
// Author: Luke Zulauf (lzulauf@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
// See http://creativecommons.org/licenses/MIT/ for more information.

// -----------------------------------------------------------------------------

// EnvelopeMode
// Manages modes with DAHDSR (Delay, Attack, Hold, Decay, Sustain, Release)
// envelopes.
//
// The DAHDSR envelope generator is a multi-channel, 6-stage (per channel) envelope
// generator. Each stage is independently configurable.

#ifndef STAGES_ENVELOPE_MODE_H
#define STAGES_ENVELOPE_MODE_H

#include "stmlib/stmlib.h"
#include "stmlib/system/storage.h"

#include "stages/envelope.h"
#include "stages/io_buffer.h"
#include "stages/modes.h"

namespace stages {

class Settings;
class Ui;

class EnvelopeMode {
 public:
  EnvelopeMode() { }
  ~EnvelopeMode() { }

  void Init(Settings* settings);
  void ReInit();
  void SetUI(Ui* ui);
  void ProcessSixIndependentEgs(IOBuffer::Block* block, size_t size);
  void ProcessSixIdenticalEgs(IOBuffer::Block* block, size_t size);

 private:
  Settings* settings_;
  Ui* ui_;
  Envelope eg_[kNumChannels];

  // Keep track of whether latest values have yet to be saved.
  // -1 means data is not dirty.
  // a positive time (>=0) means data is dirty and is the number of ticks since it first became dirty.
  int save_timer;
  
  // The index of the currently selected envelope
  size_t active_envelope;

  // Wait 1sec at boot before checking gates
  int egGateWarmTime;

  // Disallow channel switching for one second on startup (and also every time
  // the channel is switched - see below).
  int activeChannelSwitchTime;

  // Initial slider positions (set when switching channels). This allows us to
  // determine once the user has moved a slider sufficiently.
  // We don't use slider locking, since that feature does not support locking to
  // an arbitrary value.
  bool need_to_set_initial_slider_positions;
  float initial_slider_positions[kNumChannels];

  // For each envelope feature, whether we are currently using slider values
  // Slider positions are ignored for the active envelope until the user moves
  // them sufficiently to activate them.
  bool slider_enabled[kNumChannels];

  bool SetIndependentEGState_(uint8_t channel, uint8_t state_offset, float value);

  // Sets the given value on all envelopes. Does NOT store the values in state.
  // This is used in identical eg mode where the envelopes reflect the current
  // slider positions at all times (and so don't need state stored).
  //
  // It is recommended to use these functions over setting envelope values
  // directly.
  void SetAllDelayLength_(float value);
  void SetAllAttackLength_(float value);
  void SetAllAttackCurve_(float value);
  void SetAllHoldLength_(float value);
  void SetAllDecayLength_(float value);
  void SetAllDecayCurve_(float value);
  void SetAllSustainLevel_(float value);
  void SetAllReleaseLength_(float value);
  void SetAllReleaseCurve_(float value);

  // Sets the given value on the given envelope channel. Also stores the value
  // in state. This is used in individual eg mode where the envelopes do not
  // necessarily reflect the current slider positions. State is stored so that
  // envelope values survive power cycles.
  // Returns true if state was modified (new value is different from existing
  // value by more than a noise tolerance).
  //
  // It is recommended to use these functions over setting envelope values
  // directly.
  bool SetDelayLength_(uint8_t channel, float value);
  bool SetAttackLength_(uint8_t channel, float value);
  bool SetAttackCurve_(uint8_t channel, float value);
  bool SetHoldLength_(uint8_t channel, float value);
  bool SetDecayLength_(uint8_t channel, float value);
  bool SetDecayCurve_(uint8_t channel, float value);
  bool SetSustainLevel_(uint8_t channel, float value);
  bool SetReleaseLength_(uint8_t channel, float value);
  bool SetReleaseCurve_(uint8_t channel, float value);

  Envelope& GetEnvelope_(uint8_t channel) { return eg_[channel]; }


  DISALLOW_COPY_AND_ASSIGN(EnvelopeMode);
};

}  // namespace stages

#endif  // STAGES_ENVELOPE_MODE_H

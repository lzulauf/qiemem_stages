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

// EnvelopeManager
// Manages modes with DAHDSR (Delay, Attack, Hold, Decay, Sustain, Release)
// envelopes.
//
// The DAHDSR envelope generator is a multi-channel, 6-stage (per channel) envelope
// generator. Each stage is independently configurable.

#ifndef STAGES_ENVELOPE_MANAGER_H
#define STAGES_ENVELOPE_MANAGER_H

#include "stmlib/stmlib.h"
#include "stmlib/system/storage.h"

#include "stages/envelope.h"
#include "stages/io_buffer.h"
#include "stages/modes.h"

namespace stages {

class Settings;
class Ui;

class EnvelopeManager {
 public:
  EnvelopeManager() { }
  ~EnvelopeManager() { }

  void Init(Settings* settings);
  void ReInit();
  void SetUI(Ui* ui);

  void ProcessEnvelopes(IOBuffer::Block* block, size_t size);

 private:
  Settings* settings_;
  Ui* ui_;
  Envelope eg_[kNumChannels];

  // Keep track of whether latest values have yet to be saved.
  // -1 means data is not dirty.
  // a positive time (>=0) means data is dirty and is the number of ticks since it first became dirty.
  int save_timer_;
  
  // The index of the currently selected envelope
  size_t active_envelope_;

  // bootup delay before processing.
  int warm_time_;

  // Disallow channel switching for one second on startup (and also every time
  // the channel is switched - see below).
  int active_channel_switch_time_;

  // Initial slider positions (set when switching channels). This allows us to
  // determine once the user has moved a slider sufficiently.
  // We don't use slider locking, since that feature does not support locking to
  // an arbitrary value.
  bool need_to_set_initial_slider_positions_;
  float initial_slider_positions_[kNumChannels];

  // For each envelope feature, whether we are currently using slider values
  // Slider positions are ignored for the active envelope until the user moves
  // them sufficiently to activate them.
  bool slider_enabled_[kNumChannels];

  void ProcessSixIndependentEgs(IOBuffer::Block* block, size_t size);
  void ProcessSixIdenticalEgs(IOBuffer::Block* block, size_t size);

  bool SetIndependentEGState(uint8_t channel, uint8_t state_offset, float value);

  // Sets the given value on all envelopes. Does NOT store the values in state.
  // This is used in identical eg mode where the envelopes reflect the current
  // slider positions at all times (and so don't need state stored).
  //
  // It is recommended to use these functions over setting envelope values
  // directly.
  void SetAllDelayLength(float value);
  void SetAllAttackLength(float value);
  void SetAllAttackCurve(float value);
  void SetAllHoldLength(float value);
  void SetAllDecayLength(float value);
  void SetAllDecayCurve(float value);
  void SetAllSustainLevel(float value);
  void SetAllReleaseLength(float value);
  void SetAllReleaseCurve(float value);

  // Sets the given value on the given envelope channel. Also stores the value
  // in state. This is used in individual eg mode where the envelopes do not
  // necessarily reflect the current slider positions. State is stored so that
  // envelope values survive power cycles.
  // Returns true if state was modified (new value is different from existing
  // value by more than a noise tolerance).
  //
  // It is recommended to use these functions over setting envelope values
  // directly.
  bool SetDelayLength(uint8_t channel, float value);
  bool SetAttackLength(uint8_t channel, float value);
  bool SetAttackCurve(uint8_t channel, float value);
  bool SetHoldLength(uint8_t channel, float value);
  bool SetDecayLength(uint8_t channel, float value);
  bool SetDecayCurve(uint8_t channel, float value);
  bool SetSustainLevel(uint8_t channel, float value);
  bool SetReleaseLength(uint8_t channel, float value);
  bool SetReleaseCurve(uint8_t channel, float value);

  Envelope& get_envelope(uint8_t channel) { return eg_[channel]; }


  DISALLOW_COPY_AND_ASSIGN(EnvelopeManager);
};

}  // namespace stages

#endif  // STAGES_ENVELOPE_MANAGER_H

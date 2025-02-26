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

// DAHDSR Manager
// (Delay, Attack, Hold, Decay, Sustain, Release)
//
// The DAHDSR envelope generator is a multi-channel, 6-stage (per channel) envelope
// generator. Each stage is independently configurable.

#ifndef STAGES_DAHDSR_H_
#define STAGES_DAHDSR_H_

#include "stmlib/stmlib.h"
#include "stmlib/system/storage.h"

#include "stages/envelope.h"
#include "stages/io_buffer.h"
#include "stages/modes.h"

namespace stages {

class Settings;

class EnvelopeManager {
 public:
  EnvelopeManager() { }
  ~EnvelopeManager() { }

  void Init(Settings* settings);
  void ReInit();

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

  Envelope& GetEnvelope(uint8_t channel) { return eg_[channel]; }

 private:
  Settings* settings_;
  Envelope eg_[kNumChannels];

  bool SetIndependentEGState_(uint8_t channel, uint8_t state_offset, float value);

  DISALLOW_COPY_AND_ASSIGN(EnvelopeManager);
};

}  // namespace stages

#endif  // STAGES_DAHDSR_H_

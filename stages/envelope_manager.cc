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


#include "stages/envelope_manager.h"
#include "stages/ui.h"
#include "stages/chain_state.h"

#include <algorithm>

#include "stages/drivers/leds.h"
#include "stages/settings.h"
#include "stmlib/system/system_clock.h"

using namespace std;
using namespace stmlib;


namespace stages {

  const uint8_t noise_tolerance = 1;

  // Convert a pot or slider value on a float [-1., 2.) range to a [0, 255]
  // range.
  uint16_t pot_or_slider_to_uint16_t(float value) {
    float result = (value+1.0f) / 3.0f * 255.0f;
    CONSTRAIN(result, 0, 255);
    return result;
  }

  // Convert a uint16_t on an integer [0, 255] range to a [-1., 2.) range
  // suitable for a pot or slider.
  float uint16_t_to_slider_or_pot(uint16_t value) {
    return (value / 255.0f * 3.0f) - 1.0f;
  }

  void EnvelopeManager::Init(Settings* settings) {
    settings_ = settings;
    ReInit();
  }

  void EnvelopeManager::ReInit() {
    for (size_t i = 0; i < kNumChannels; ++i) {
      eg_[i].Init();
    }
    if (settings_->state().multimode == MULTI_MODE_SIX_INDEPENDENT_EGS) {
      for (size_t i = 0; i < kNumChannels; ++i) {
        const uint8_t* eg_state = settings_->state().independent_eg_state[i];
        Envelope& envelope = eg_[i];
        envelope.SetDelayLength(uint16_t_to_slider_or_pot(eg_state[IEG_DELAY_LENGTH]));
        envelope.SetAttackLength(uint16_t_to_slider_or_pot(eg_state[IEG_ATTACK_LENGTH]));
        envelope.SetAttackCurve(uint16_t_to_slider_or_pot(eg_state[IEG_ATTACK_CURVE]));
        envelope.SetHoldLength(uint16_t_to_slider_or_pot(eg_state[IEG_HOLD_LENGTH]));
        envelope.SetDecayLength(uint16_t_to_slider_or_pot(eg_state[IEG_DECAY_LENGTH]));
        envelope.SetDecayCurve(uint16_t_to_slider_or_pot(eg_state[IEG_DECAY_CURVE]));
        envelope.SetSustainLevel(uint16_t_to_slider_or_pot(eg_state[IEG_SUSTAIN_LEVEL]));
        envelope.SetReleaseLength(uint16_t_to_slider_or_pot(eg_state[IEG_RELEASE_LENGTH]));
        envelope.SetReleaseCurve(uint16_t_to_slider_or_pot(eg_state[IEG_RELEASE_CURVE]));
      }
    }
  }

  void EnvelopeManager::SetAllDelayLength(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      eg_[envelope].SetDelayLength(value);
    }
  }

  void EnvelopeManager::SetAllAttackLength(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      eg_[envelope].SetAttackLength(value);
    }
  }

  void EnvelopeManager::SetAllAttackCurve(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      eg_[envelope].SetAttackCurve(value);
    }
  }

  void EnvelopeManager::SetAllHoldLength(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      eg_[envelope].SetHoldLength(value);
    }
  }

  void EnvelopeManager::SetAllDecayLength(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      eg_[envelope].SetDecayLength(value);
    }
  }

  void EnvelopeManager::SetAllDecayCurve(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      eg_[envelope].SetDecayCurve(value);
    }
  }

  void EnvelopeManager::SetAllSustainLevel(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      eg_[envelope].SetSustainLevel(value);
    }
  }

  void EnvelopeManager::SetAllReleaseLength(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      eg_[envelope].SetReleaseLength(value);
    }
  }

  void EnvelopeManager::SetAllReleaseCurve(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      eg_[envelope].SetReleaseCurve(value);
    }
  }

  bool EnvelopeManager::SetDelayLength(uint8_t channel, float value) {
    eg_[channel].SetDelayLength(value);
    return SetIndependentEGState_(channel, IEG_DELAY_LENGTH, value);
  }

  bool EnvelopeManager::SetAttackLength(uint8_t channel, float value) {
    eg_[channel].SetAttackLength(value);
    return SetIndependentEGState_(channel, IEG_ATTACK_LENGTH, value);
  }

  bool EnvelopeManager::SetAttackCurve(uint8_t channel, float value) {
    eg_[channel].SetAttackCurve(value);
    return SetIndependentEGState_(channel, IEG_ATTACK_CURVE, value);
  }

  bool EnvelopeManager::SetHoldLength(uint8_t channel, float value) {
    eg_[channel].SetHoldLength(value);
    return SetIndependentEGState_(channel, IEG_HOLD_LENGTH, value);
  }

  bool EnvelopeManager::SetDecayLength(uint8_t channel, float value) {
    eg_[channel].SetDecayLength(value);
    return SetIndependentEGState_(channel, IEG_DECAY_LENGTH, value);
  }

  bool EnvelopeManager::SetDecayCurve(uint8_t channel, float value) {
    eg_[channel].SetDecayCurve(value);
    return SetIndependentEGState_(channel, IEG_DECAY_CURVE, value);
  }

  bool EnvelopeManager::SetSustainLevel(uint8_t channel, float value) {
    eg_[channel].SetSustainLevel(value);
    return SetIndependentEGState_(channel, IEG_SUSTAIN_LEVEL, value);
  }

  bool EnvelopeManager::SetReleaseLength(uint8_t channel, float value) {
    eg_[channel].SetReleaseLength(value);
    return SetIndependentEGState_(channel, IEG_RELEASE_LENGTH, value);
  }

  bool EnvelopeManager::SetReleaseCurve(uint8_t channel, float value) {
    eg_[channel].SetReleaseCurve(value);
    return SetIndependentEGState_(channel, IEG_RELEASE_CURVE, value);
  }

  bool EnvelopeManager::SetIndependentEGState_(uint8_t channel, uint8_t state_offset, float value) {
    uint8_t* eg_state = settings_->mutable_state()->independent_eg_state[channel];
    uint8_t existing_value = eg_state[state_offset];
    uint8_t converted = pot_or_slider_to_uint16_t(value);
    if (abs(converted - existing_value) > noise_tolerance) {
        eg_state[state_offset] = converted;
        return true;
    }

    return false;

    // TODO remove this dead code.
    // n.b. since we write to state regardless of whether or not we return True,
    // the existing value is always trailing the value. We will only detect a
    // change above noise_tolerance if moved quickly.
    // return abs(converted - existing_value) > noise_tolerance;
  }

}  // namespace stages

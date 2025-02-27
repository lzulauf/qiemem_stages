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

  const uint8_t kNoiseTolerance = 1;
  const int kSaveTimeWait = 20000; // 5 seconds
  const float kSliderMoveThreshold = 0.05f;

  // Convert a pot or slider value on a float [-1., 2.) range to a [0, 255]
  // range.
  uint16_t PotOrSliderToUint16(float value) {
    float result = (value+1.0f) / 3.0f * 255.0f;
    CONSTRAIN(result, 0, 255);
    return result;
  }

  // Convert a uint16_t on an integer [0, 255] range to a [-1., 2.) range
  // suitable for a pot or slider.
  float Uint16ToSliderOrPot(uint16_t value) {
    return (value / 255.0f * 3.0f) - 1.0f;
  }

  void EnvelopeManager::Init(Settings* settings) {
    settings_ = settings;
    ui_ = NULL;
    ReInit();
  }

  void EnvelopeManager::ReInit() {
    // Don't process gates for one second after boot or switching modes.
    warm_time_ = 2000;

    // Disallow channel switching for one second on startup (and also every time
    // the channel is switched - see below).
    active_channel_switch_time_ = 0;

    // Disable save timer
    save_timer_ = -1;

    // The index of the currently selected envelope. 
    active_envelope_ = 0;

    // We can't read the slider positions directly in this method, so we record
    // that we need to set the initial slider positions upon next process step.
    need_to_set_initial_slider_positions_ = true;
    fill(&initial_slider_positions_[0], &initial_slider_positions_[kNumChannels], 0.0f);

    // Disable all sliders
    fill(&slider_enabled_[0], &slider_enabled_[kNumChannels], false);

    for (size_t i = 0; i < kNumChannels; ++i) {
      get_envelope(i).Init();
    }
    if (settings_->state().multimode == MULTI_MODE_SIX_INDEPENDENT_EGS) {
      for (size_t i = 0; i < kNumChannels; ++i) {
        const uint8_t* eg_state = settings_->state().independent_eg_state[i];
        Envelope& envelope = get_envelope(i);
        envelope.SetDelayLength(Uint16ToSliderOrPot(eg_state[IEG_DELAY_LENGTH]));
        envelope.SetAttackLength(Uint16ToSliderOrPot(eg_state[IEG_ATTACK_LENGTH]));
        envelope.SetAttackCurve(Uint16ToSliderOrPot(eg_state[IEG_ATTACK_CURVE]));
        envelope.SetHoldLength(Uint16ToSliderOrPot(eg_state[IEG_HOLD_LENGTH]));
        envelope.SetDecayLength(Uint16ToSliderOrPot(eg_state[IEG_DECAY_LENGTH]));
        envelope.SetDecayCurve(Uint16ToSliderOrPot(eg_state[IEG_DECAY_CURVE]));
        envelope.SetSustainLevel(Uint16ToSliderOrPot(eg_state[IEG_SUSTAIN_LEVEL]));
        envelope.SetReleaseLength(Uint16ToSliderOrPot(eg_state[IEG_RELEASE_LENGTH]));
        envelope.SetReleaseCurve(Uint16ToSliderOrPot(eg_state[IEG_RELEASE_CURVE]));
      }
    }
  }

  void EnvelopeManager::SetUI(Ui* ui) {
    ui_ = ui;
  }

  void EnvelopeManager::ProcessEnvelopes(IOBuffer::Block* block, size_t size) {
    switch (settings_->state().multimode) {
      case MULTI_MODE_SIX_INDEPENDENT_EGS:
        ProcessSixIndependentEgs(block, size);
        break;
      case MULTI_MODE_SIX_IDENTICAL_EGS:
        ProcessSixIdenticalEgs(block, size);
        break;
    }
  }

  void EnvelopeManager::SetAllDelayLength(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      get_envelope(envelope).SetDelayLength(value);
    }
  }

  void EnvelopeManager::SetAllAttackLength(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      get_envelope(envelope).SetAttackLength(value);
    }
  }

  void EnvelopeManager::SetAllAttackCurve(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      get_envelope(envelope).SetAttackCurve(value);
    }
  }

  void EnvelopeManager::SetAllHoldLength(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      get_envelope(envelope).SetHoldLength(value);
    }
  }

  void EnvelopeManager::SetAllDecayLength(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      get_envelope(envelope).SetDecayLength(value);
    }
  }

  void EnvelopeManager::SetAllDecayCurve(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      get_envelope(envelope).SetDecayCurve(value);
    }
  }

  void EnvelopeManager::SetAllSustainLevel(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      get_envelope(envelope).SetSustainLevel(value);
    }
  }

  void EnvelopeManager::SetAllReleaseLength(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      get_envelope(envelope).SetReleaseLength(value);
    }
  }

  void EnvelopeManager::SetAllReleaseCurve(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      get_envelope(envelope).SetReleaseCurve(value);
    }
  }

  bool EnvelopeManager::SetDelayLength(uint8_t channel, float value) {
    get_envelope(channel).SetDelayLength(value);
    return SetIndependentEGState(channel, IEG_DELAY_LENGTH, value);
  }

  bool EnvelopeManager::SetAttackLength(uint8_t channel, float value) {
    get_envelope(channel).SetAttackLength(value);
    return SetIndependentEGState(channel, IEG_ATTACK_LENGTH, value);
  }

  bool EnvelopeManager::SetAttackCurve(uint8_t channel, float value) {
    get_envelope(channel).SetAttackCurve(value);
    return SetIndependentEGState(channel, IEG_ATTACK_CURVE, value);
  }

  bool EnvelopeManager::SetHoldLength(uint8_t channel, float value) {
    get_envelope(channel).SetHoldLength(value);
    return SetIndependentEGState(channel, IEG_HOLD_LENGTH, value);
  }

  bool EnvelopeManager::SetDecayLength(uint8_t channel, float value) {
    get_envelope(channel).SetDecayLength(value);
    return SetIndependentEGState(channel, IEG_DECAY_LENGTH, value);
  }

  bool EnvelopeManager::SetDecayCurve(uint8_t channel, float value) {
    get_envelope(channel).SetDecayCurve(value);
    return SetIndependentEGState(channel, IEG_DECAY_CURVE, value);
  }

  bool EnvelopeManager::SetSustainLevel(uint8_t channel, float value) {
    get_envelope(channel).SetSustainLevel(value);
    return SetIndependentEGState(channel, IEG_SUSTAIN_LEVEL, value);
  }

  bool EnvelopeManager::SetReleaseLength(uint8_t channel, float value) {
    get_envelope(channel).SetReleaseLength(value);
    return SetIndependentEGState(channel, IEG_RELEASE_LENGTH, value);
  }

  bool EnvelopeManager::SetReleaseCurve(uint8_t channel, float value) {
    get_envelope(channel).SetReleaseCurve(value);
    return SetIndependentEGState(channel, IEG_RELEASE_CURVE, value);
  }

  bool EnvelopeManager::SetIndependentEGState(uint8_t channel, uint8_t state_offset, float value) {
    uint8_t* eg_state = settings_->mutable_state()->independent_eg_state[channel];
    uint8_t existing_value = eg_state[state_offset];
    uint8_t converted = PotOrSliderToUint16(value);
    if (abs(converted - existing_value) > kNoiseTolerance) {
        eg_state[state_offset] = converted;
        return true;
    }

    return false;
  }

  void EnvelopeManager::ProcessSixIndependentEgs(IOBuffer::Block* block, size_t size) {
    // Support six independant envelope generators
    //
    // Pressing a the button corresponding to a non-active envelope generator
    // sets that envelope generator as the active envelope generator. Pressing a
    // button corresponding to the active envelope generator enables all sliders.
    // Therefore, setting all sliders and then double tapping each button will
    // set six identical envelope generators.
    //
    // In general, switching egs does not immediately reflect the physical
    // position of sliders and pots. The user must move a slider sufficiently to
    // enable it. Activating a slider also activates its pot and cv inputs.
    // Slider leds will be green when that slider is enabled.
    //
    // If no input cable is plugged into an envelope generator's gate input, then
    // the previous envelope generator's gate input is used. This allows a single
    // input to trigger multiple envelope generators.

    // Don't do any processing during warmup.
    if (warm_time_ > 0) {
      --warm_time_;
      for (size_t ch = 0; ch < kNumChannels; ++ch) {
        for (size_t i = 0; i < size; ++i) {
          block->output[ch][i] = settings_->dac_code(ch, 0.f);
        }
      }
      return;
    }

    if (active_channel_switch_time_ > 0) --active_channel_switch_time_;

    // If we need to set initial slider positions (from changing modes), record
    // the current slider positions.
    // Note that they will also be recorded when changing the active channel.
    if (need_to_set_initial_slider_positions_) {
      for (size_t ch = 0; ch < kNumChannels; ++ch) {
        initial_slider_positions_[ch] = block->slider[ch];
      }
      need_to_set_initial_slider_positions_ = false;
    }

    // Handle for channel switch presses
    for (size_t ch = 0; ch < kNumChannels; ++ch) {
      // Check if button is pressed to switch channels or activate all sliders.
      // Ignore switch presses if recently switched.
      if (!active_channel_switch_time_ && ui_->switches().pressed(ch)) {
        // Once a switch is pressed, delay processing switches for 1/4 second.
        // This is essentially a super basic debounce.
        active_channel_switch_time_ = 1000;

        if (ch == active_envelope_) {      
          // Pressing the active channel enables all sliders and pots
          fill(&slider_enabled_[0], &slider_enabled_[size], true);
        } else {
          // Pressing an inactive channel switches to that channel
          // Record initial slider positions after switch and set all sliders to inactive
          active_envelope_ = ch;
          fill(&slider_enabled_[0], &slider_enabled_[size], false);
          for (size_t slider_index = 0; slider_index < kNumChannels; ++slider_index) {
            initial_slider_positions_[slider_index] = block->slider[slider_index];
          }
        }
      }
    }

    // Update envelope parameters for active envelope.
    bool did_modify_state = false;
    if (slider_enabled_[0]) {
      did_modify_state |= SetDelayLength(active_envelope_, block->slider[0]);
    }
    if (slider_enabled_[1]) {
      did_modify_state |= SetAttackLength(active_envelope_, block->slider[1]);
      did_modify_state |= SetAttackCurve(active_envelope_, block->pot[1]);
    }
    if (slider_enabled_[2]) {
      did_modify_state |= SetHoldLength(active_envelope_, block->slider[2]);
    }
    if (slider_enabled_[3]) {
      did_modify_state |= SetDecayLength(active_envelope_, block->slider[3]);
      did_modify_state |= SetDecayCurve(active_envelope_, block->pot[3]);
    }
    if (slider_enabled_[4]) {
      did_modify_state |= SetSustainLevel(active_envelope_, block->slider[4]);
    }
    if (slider_enabled_[5]) {
      did_modify_state |= SetReleaseLength(active_envelope_, block->slider[5]);
      did_modify_state |= SetReleaseCurve(active_envelope_, block->pot[5]);
    }
    // Start/Reset the save timer if state was modified
    if (did_modify_state) {
      save_timer_ = 0;
    }

    // Process each channel
    bool gate = false;
    for (size_t ch = 0; ch < kNumChannels; ch++) {

      // Check if slider has moved sufficiently to enable the slider for the
      // active envelope.
      if (abs(block->slider[ch] - initial_slider_positions_[ch]) > kSliderMoveThreshold) {
        slider_enabled_[ch] = true;
      }

      // Set Slider LED to indicate whether slider is active for curent envelope.
      ui_->set_slider_led(ch, slider_enabled_[ch], 1);

      // Check for gates. If the input is not patched, the previous channel's
      // gate status will be used.
      if (block->input_patched[ch]) {
        gate = false;
        for (size_t i = 0; i < size; i++) {
          if (block->input[ch][i] & GATE_FLAG_HIGH) {
            gate = true;
            break;
          }
        }
      }

      Envelope& envelope = get_envelope(ch);
      envelope.Gate(gate);

      // Set LED to indicate stage of the channel's envelope. Active channel is lit rather than off when idle.
      switch (envelope.CurrentStage()) {
        case DELAY:
        case ATTACK:
        case HOLD:
        case DECAY:
          ui_->set_led(ch, LED_COLOR_GREEN);
          break;
        case SUSTAIN:
          ui_->set_led(ch, LED_COLOR_YELLOW);
          break;
        case RELEASE:
          ui_->set_led(ch, LED_COLOR_RED);
          break;
        default:
          ui_->set_led(ch, ch == active_envelope_ ? LED_COLOR_YELLOW : LED_COLOR_OFF);
          break;
      }

      // Compute output values for each envelope
      float value = envelope.Value();
      for (size_t i = 0; i < size; i++) {
        block->output[ch][i] = settings_->dac_code(ch, value);
      }
    }

    if (save_timer_ >= 0) {
      if (++save_timer_ >= kSaveTimeWait) {
        save_timer_ = -1;
        settings_->SaveState();
      }
    }
  }

  void EnvelopeManager::ProcessSixIdenticalEgs(IOBuffer::Block* block, size_t size) {

    // Don't do any processing during warmup.
    if (warm_time_ > 0) {
      --warm_time_;
      for (size_t ch = 0; ch < kNumChannels; ++ch) {
        for (size_t i = 0; i < size; ++i) {
          block->output[ch][i] = settings_->dac_code(ch, 0.f);
        }
      }
      return;
    }

    // Slider LEDs
    ui_->set_slider_led(0, get_envelope(0).HasDelay  (), 1);
    ui_->set_slider_led(1, get_envelope(0).HasAttack (), 1);
    ui_->set_slider_led(2, get_envelope(0).HasHold   (), 1);
    ui_->set_slider_led(3, get_envelope(0).HasDecay  (), 1);
    ui_->set_slider_led(4, get_envelope(0).HasSustain(), 1);
    ui_->set_slider_led(5, get_envelope(0).HasRelease(), 1);

    // Set pots params
    SetAllAttackCurve (block->pot[1]);
    SetAllDecayCurve  (block->pot[3]);
    SetAllReleaseCurve(block->pot[5]);

    // Set slider params
    SetAllDelayLength  (block->cv_slider[0]);
    SetAllAttackLength (block->cv_slider[1]);
    SetAllHoldLength   (block->cv_slider[2]);
    SetAllDecayLength  (block->cv_slider[3]);
    SetAllSustainLevel (block->cv_slider[4]);
    SetAllReleaseLength(block->cv_slider[5]);

    for (size_t ch = 0; ch < kNumChannels; ++ch) {
      // Gate or button?
      bool gate = ui_->switches().pressed(ch);
      if (!gate && block->input_patched[ch]) {
        for (size_t i = 0; i < size; i++) {
          if (block->input[ch][i] & GATE_FLAG_HIGH) {
            gate = true;
            break;
          }
        }
      }
      Envelope& envelope = get_envelope(ch);
      envelope.Gate(gate);
      ui_->set_led(ch, gate ? LED_COLOR_RED : LED_COLOR_OFF);

      // Compute value and set as output
      float value = envelope.Value();
      for (size_t i = 0; i < size; i++) {
        block->output[ch][i] = settings_->dac_code(ch, value);
      }

      // Display current stage
      switch (envelope.CurrentStage()) {
        case DELAY:
        case ATTACK:
        case HOLD:
        case DECAY:
          ui_->set_led(ch, LED_COLOR_GREEN);
          break;
        case SUSTAIN:
          ui_->set_led(ch, LED_COLOR_YELLOW);
          break;
        case RELEASE:
          ui_->set_led(ch, LED_COLOR_RED);
          break;
        default:
          ui_->set_led(ch, LED_COLOR_OFF);
          break;
      }

    }

  }

}  // namespace stages

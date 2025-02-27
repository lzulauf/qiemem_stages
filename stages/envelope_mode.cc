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


#include "stages/envelope_mode.h"
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
  const int save_time_wait = 20000; // 5 seconds
  const float slider_move_threshold = 0.05f;

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



  void EnvelopeMode::Init(Settings* settings) {
    settings_ = settings;
    ui_ = NULL;
    ReInit();
  }

  void EnvelopeMode::ReInit() {
    // Don't process gates for one second after boot or switching modes.
    egGateWarmTime = 4000;

    // Disallow channel switching for one second on startup (and also every time
    // the channel is switched - see below).
    activeChannelSwitchTime = 4000;

    // Reset save timer
    save_timer = -1;

    // The index of the currently selected envelope. 
    active_envelope = 0;

    // We can't read the slider positions directly in this method, so we record
    // that we need to set the initial slider positions upon next process step.
    need_to_set_initial_slider_positions = true;
    fill(&initial_slider_positions[0], &initial_slider_positions[kNumChannels], 0.0f);
    
    // Disable all sliders
    fill(&slider_enabled[0], &slider_enabled[kNumChannels], false);

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

  void EnvelopeMode::SetUI(Ui* ui) {
    ui_ = ui;
  }

  void EnvelopeMode::SetAllDelayLength_(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      eg_[envelope].SetDelayLength(value);
    }
  }

  void EnvelopeMode::SetAllAttackLength_(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      eg_[envelope].SetAttackLength(value);
    }
  }

  void EnvelopeMode::SetAllAttackCurve_(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      eg_[envelope].SetAttackCurve(value);
    }
  }

  void EnvelopeMode::SetAllHoldLength_(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      eg_[envelope].SetHoldLength(value);
    }
  }

  void EnvelopeMode::SetAllDecayLength_(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      eg_[envelope].SetDecayLength(value);
    }
  }

  void EnvelopeMode::SetAllDecayCurve_(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      eg_[envelope].SetDecayCurve(value);
    }
  }

  void EnvelopeMode::SetAllSustainLevel_(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      eg_[envelope].SetSustainLevel(value);
    }
  }

  void EnvelopeMode::SetAllReleaseLength_(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      eg_[envelope].SetReleaseLength(value);
    }
  }

  void EnvelopeMode::SetAllReleaseCurve_(float value) {
    for (uint8_t envelope = 0; envelope < kNumChannels; ++envelope) {
      eg_[envelope].SetReleaseCurve(value);
    }
  }

  bool EnvelopeMode::SetDelayLength_(uint8_t channel, float value) {
    eg_[channel].SetDelayLength(value);
    return SetIndependentEGState_(channel, IEG_DELAY_LENGTH, value);
  }

  bool EnvelopeMode::SetAttackLength_(uint8_t channel, float value) {
    eg_[channel].SetAttackLength(value);
    return SetIndependentEGState_(channel, IEG_ATTACK_LENGTH, value);
  }

  bool EnvelopeMode::SetAttackCurve_(uint8_t channel, float value) {
    eg_[channel].SetAttackCurve(value);
    return SetIndependentEGState_(channel, IEG_ATTACK_CURVE, value);
  }

  bool EnvelopeMode::SetHoldLength_(uint8_t channel, float value) {
    eg_[channel].SetHoldLength(value);
    return SetIndependentEGState_(channel, IEG_HOLD_LENGTH, value);
  }

  bool EnvelopeMode::SetDecayLength_(uint8_t channel, float value) {
    eg_[channel].SetDecayLength(value);
    return SetIndependentEGState_(channel, IEG_DECAY_LENGTH, value);
  }

  bool EnvelopeMode::SetDecayCurve_(uint8_t channel, float value) {
    eg_[channel].SetDecayCurve(value);
    return SetIndependentEGState_(channel, IEG_DECAY_CURVE, value);
  }

  bool EnvelopeMode::SetSustainLevel_(uint8_t channel, float value) {
    eg_[channel].SetSustainLevel(value);
    return SetIndependentEGState_(channel, IEG_SUSTAIN_LEVEL, value);
  }

  bool EnvelopeMode::SetReleaseLength_(uint8_t channel, float value) {
    eg_[channel].SetReleaseLength(value);
    return SetIndependentEGState_(channel, IEG_RELEASE_LENGTH, value);
  }

  bool EnvelopeMode::SetReleaseCurve_(uint8_t channel, float value) {
    eg_[channel].SetReleaseCurve(value);
    return SetIndependentEGState_(channel, IEG_RELEASE_CURVE, value);
  }

  bool EnvelopeMode::SetIndependentEGState_(uint8_t channel, uint8_t state_offset, float value) {
    uint8_t* eg_state = settings_->mutable_state()->independent_eg_state[channel];
    uint8_t existing_value = eg_state[state_offset];
    uint8_t converted = pot_or_slider_to_uint16_t(value);
    if (abs(converted - existing_value) > noise_tolerance) {
        eg_state[state_offset] = converted;
        return true;
    }

    return false;
  }

  void EnvelopeMode::ProcessSixIndependentEgs(IOBuffer::Block* block, size_t size) {
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

    // Wait 1sec at mode startup before checking gates
    if (egGateWarmTime > 0) egGateWarmTime--;

    // Disallow channel switching for one second on startup (and also every time
    // the channel is switched - see below).
    if (activeChannelSwitchTime > 0) --activeChannelSwitchTime;

    // If we need to set initial slider positions (from changing modes), record
    // the current slider positions.
    // Note that they will also be recorded when changing the active channel.
    if (need_to_set_initial_slider_positions) {
      for (size_t ch = 0; ch < kNumChannels; ++ch) {
        initial_slider_positions[ch] = block->slider[ch];
      }
      need_to_set_initial_slider_positions = false;
    }

    // Handle for channel switch presses
    for (size_t ch = 0; ch < kNumChannels; ++ch) {
      // Check if button is pressed to switch channels or activate all sliders.
      // Ignore switch presses if recently switched.
      if (!activeChannelSwitchTime && ui_->switches().pressed(ch)) {
        // Once a switch is pressed, delay processing switches for 1/4 second.
        // This is essentially a super basic debounce.
        activeChannelSwitchTime = 1000;

        if (ch == active_envelope) {      
          // Pressing the active channel enables all sliders and pots
          fill(&slider_enabled[0], &slider_enabled[size], true);
        } else {
          // Pressing an inactive channel switches to that channel
          // Record initial slider positions after switch and set all sliders to inactive
          active_envelope = ch;
          fill(&slider_enabled[0], &slider_enabled[size], false);
          for (size_t slider_index = 0; slider_index < kNumChannels; ++slider_index) {
            initial_slider_positions[slider_index] = block->slider[slider_index];
          }
        }
      }
    }

    // Update envelope parameters for active envelope.
    bool did_modify_state = false;
    if (slider_enabled[0]) {
      did_modify_state |= SetDelayLength_(active_envelope, block->slider[0]);
    }
    if (slider_enabled[1]) {
      did_modify_state |= SetAttackLength_(active_envelope, block->slider[1]);
      did_modify_state |= SetAttackCurve_(active_envelope, block->pot[1]);
    }
    if (slider_enabled[2]) {
      did_modify_state |= SetHoldLength_(active_envelope, block->slider[2]);
    }
    if (slider_enabled[3]) {
      did_modify_state |= SetDecayLength_(active_envelope, block->slider[3]);
      did_modify_state |= SetDecayCurve_(active_envelope, block->pot[3]);
    }
    if (slider_enabled[4]) {
      did_modify_state |= SetSustainLevel_(active_envelope, block->slider[4]);
    }
    if (slider_enabled[5]) {
      did_modify_state |= SetReleaseLength_(active_envelope, block->slider[5]);
      did_modify_state |= SetReleaseCurve_(active_envelope, block->pot[5]);
    }
    // Start/Reset the save timer if state was modified
    if (did_modify_state) {
      save_timer = 0;
    }

    // Process each channel
    bool gate = false;
    for (size_t ch = 0; ch < kNumChannels; ch++) {
      
      // Check if slider has moved sufficiently to enable the slider for the
      // active envelope.
      if (abs(block->slider[ch] - initial_slider_positions[ch]) > slider_move_threshold) {
        slider_enabled[ch] = true;
      }

      // Set Slider LED to indicate whether slider is active for curent envelope.
      ui_->set_slider_led(ch, slider_enabled[ch], 1);

      // Check for gates. If the input is not patched, the previous channel's
      // gate status will be used.
      if (egGateWarmTime == 0 && block->input_patched[ch]) {
        gate = false;
        for (size_t i = 0; i < size; i++) {
          if (block->input[ch][i] & GATE_FLAG_HIGH) {
            gate = true;
            break;
          }
        }
      }

      Envelope& envelope = GetEnvelope_(ch);
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
          ui_->set_led(ch, ch == active_envelope ? LED_COLOR_YELLOW : LED_COLOR_OFF);
          break;
      }

      // Compute output values for each envelope
      float value = envelope.Value();
      for (size_t i = 0; i < size; i++) {
        block->output[ch][i] = settings_->dac_code(ch, value);
      }
    }

    if (save_timer >= 0) {
      if (++save_timer >= save_time_wait) {
        save_timer = -1;
        settings_->SaveState();
      }
    }
  }

  void EnvelopeMode::ProcessSixIdenticalEgs(IOBuffer::Block* block, size_t size) {

    // Slider LEDs
    ui_->set_slider_led(0, GetEnvelope_(0).HasDelay  (), 1);
    ui_->set_slider_led(1, GetEnvelope_(0).HasAttack (), 1);
    ui_->set_slider_led(2, GetEnvelope_(0).HasHold   (), 1);
    ui_->set_slider_led(3, GetEnvelope_(0).HasDecay  (), 1);
    ui_->set_slider_led(4, GetEnvelope_(0).HasSustain(), 1);
    ui_->set_slider_led(5, GetEnvelope_(0).HasRelease(), 1);

    // Wait 1sec at mode startup before checking gates
    if (egGateWarmTime > 0) egGateWarmTime--;

    // Set pots params
    SetAllAttackCurve_ (block->pot[1]);
    SetAllDecayCurve_  (block->pot[3]);
    SetAllReleaseCurve_(block->pot[5]);

    // Set slider params
    SetAllDelayLength_  (block->cv_slider[0]);
    SetAllAttackLength_ (block->cv_slider[1]);
    SetAllHoldLength_   (block->cv_slider[2]);
    SetAllDecayLength_  (block->cv_slider[3]);
    SetAllSustainLevel_ (block->cv_slider[4]);
    SetAllReleaseLength_(block->cv_slider[5]);

    for (size_t ch = 0; ch < kNumChannels; ch++) {


      // Gate or button?
      bool gate = ui_->switches().pressed(ch);
      if (!gate && egGateWarmTime == 0 && block->input_patched[ch]) {
        for (size_t i = 0; i < size; i++) {
          if (block->input[ch][i] & GATE_FLAG_HIGH) {
            gate = true;
            break;
          }
        }
      }
      Envelope& envelope = GetEnvelope_(ch);
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

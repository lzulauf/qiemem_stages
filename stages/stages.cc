// Copyright 2017 Emilie Gillet.
//
// Author: Emilie Gillet (emilie.o.gillet@gmail.com)
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

#include <stm32f37x_conf.h>

#include "stmlib/dsp/dsp.h"

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/units.h"
#include "stages/chain_state.h"
#include "stages/drivers/dac.h"
#include "stages/drivers/gate_inputs.h"
#include "stages/drivers/leds.h"
#include "stages/drivers/serial_link.h"
#include "stages/drivers/system.h"
#include "stages/cv_reader.h"
#include "stages/factory_test.h"
#include "stages/io_buffer.h"
#include "stages/oscillator.h"
#include "stages/resources.h"
#include "stages/envelope_mode.h"
#include "stages/segment_generator.h"
#include "stages/settings.h"
#include "stages/ui.h"

using namespace stages;
using namespace std;
using namespace stmlib;

const bool skip_factory_test = false;
const bool test_adc_noise = false;

ChainState chain_state;
CvReader cv_reader;
Dac dac;
FactoryTest factory_test;
GateFlags no_gate[kBlockSize];
GateInputs gate_inputs;
HysteresisQuantizer2 note_quantizer[kNumChannels + kMaxNumSegments];
SegmentGenerator segment_generator[kNumChannels];
Oscillator oscillator[kNumChannels];
IOBuffer io_buffer;
EnvelopeMode eg_mode;
SerialLink left_link;
SerialLink right_link;
Settings settings;
Ui ui;

// Default interrupt handlers.
extern "C" {

  void NMI_Handler() { }
  void HardFault_Handler() { while (1); }
  void MemManage_Handler() { while (1); }
  void BusFault_Handler() { while (1); }
  void UsageFault_Handler() { while (1); }
  void SVC_Handler() { }
  void DebugMon_Handler() { }
  void PendSV_Handler() { }

}

// SysTick and 32kHz handles
extern "C" {

  void SysTick_Handler() {
    IWDG_ReloadCounter();
    ui.Poll();
    if (!skip_factory_test) {
      factory_test.Poll();
    }
  }

}

IOBuffer::Slice FillBuffer(size_t size) {
  IOBuffer::Slice s = io_buffer.NextSlice(size);
  gate_inputs.Read(s, size);
  if (io_buffer.new_block()) {
    cv_reader.Read(s.block);
    gate_inputs.ReadNormalization(s.block);
  }
  return s;
}

SegmentGenerator::Output out[kBlockSize];

static float note_lp[kNumChannels] = { 0, 0, 0, 0, 0, 0 };

void Process(IOBuffer::Block* block, size_t size) {
  chain_state.Update(
      *block,
      &settings,
      &segment_generator[0],
      out);
  for (size_t channel = 0; channel < kNumChannels; ++channel) {
    // Doing the shift here was found to have better performance that in the
    // conditional below. wtf...
    out->changed_segments >>= 1;
    bool led_state = segment_generator[channel].Process(
        block->input_patched[channel] ? block->input[channel] : no_gate,
        out,
        size);
    ui.set_slider_led(channel, led_state, 5);

    if (test_adc_noise) {
      float note = block->cv_slider[channel];
      ONE_POLE(note_lp[channel], note, 0.0001f);
      float cents = (note - note_lp[channel]) * 1200.0f * 0.5f;
      CONSTRAIN(cents, -1.0f, +1.0f)
      for (size_t i = 0; i < size; ++i) {
        out[i].value = cents;
      }
    }

    // changes are indicated on first output
    if (out->changed_segments & 1) {
      ui.set_discrete_change(channel);
    }

    for (size_t i = 0; i < size; ++i) {
      block->output[channel][i] = settings.dac_code(channel, out[i].value);
    }
  }
}

void ProcessEnvelopes(IOBuffer::Block* block, size_t size) {
  eg_mode.ProcessEnvelopes(block, size);
}

const int kNumOuroborosRatios = 11;
float ouroboros_ratios[] = {
  0.25f, 0.5f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 8.0f, 8.0f
};

const int kNumOuroborosRatiosHigh = 17;
const float ouroboros_ratios_high[] = {
  1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f, 9.0f,
  10.0f, 11.0f, 12.0f, 13.0f, 14.0f, 15.0f, 16.0f, 16.0f
};

const int kNumOuroborosRatiosLow = 17;
const float ouroboros_ratios_low[] = {
  (1.0f / 16.0f), (1.0f / 15.0f), (1.0f / 14.0f), (1.0f / 13.0f),
  (1.0f / 12.0f), (1.0f / 11.0f), (1.0f / 10.0f), (1.0f / 9.0f),
  (1.0f / 8.0f), (1.0f / 7.0f), (1.0f / 6.0f), (1.0f / 5.0f),
  (1.0f / 4.0f), (1.0f / 3.0f), (1.0f / 2.0f), 1.0f, 1.0f
};

const float* ouroboros_ratios_all[] = {ouroboros_ratios, ouroboros_ratios_high, ouroboros_ratios_low};
const float ouroboros_num_ratios[] = {kNumOuroborosRatios, kNumOuroborosRatiosHigh, kNumOuroborosRatiosLow};

float this_channel[kBlockSize];
float sum[kBlockSize];
float channel_amplitude[kNumChannels];
float channel_envelope[kNumChannels];
float previous_amplitude[kNumChannels];

void ProcessOuroboros(IOBuffer::Block* block, size_t size) {
  const float coarse = (block->cv_slider[0] - 0.5f) * 96.0f;
  const float fine = block->pot[0] * 2.0f - 1.0f;
  const uint16_t* config= settings.state().segment_configuration;
  const uint8_t range = (config[0] >> 10) & 0x3;
  const float range_mult =
    (range == 0x01) ? 1.0f / 128.0f :
    (range == 0x02) ? 1.0f / (128.0f * 16.0f) :
    1.0f;
  const bool lfo = range != 0x00;
  const float f0 = SemitonesToRatio(coarse + fine) * 261.6255f / kSampleRate * range_mult;

  std::fill(&sum[0], &sum[size], 0.0f);

  bool alternate = (MultiMode) settings.state().multimode == MULTI_MODE_OUROBOROS_ALTERNATE;
  float *blockHarmonic = alternate ? block->cv_slider : block->pot;
  float *blockAmplitude = alternate ? block->pot : block->cv_slider;

  bool reset_all = false;
  if (block->input_patched[0] && lfo) {
    for (size_t i = 0; i < size; ++i) {
      reset_all = reset_all || (block->input[0][i] & GATE_FLAG_RISING);
    }
  }

  for (int channel = kNumChannels - 1; channel >= 0; --channel) {

    const uint8_t r = (config[channel] >> 10) & 0x3;
    const float* ratios = ouroboros_ratios_all[r];
    const int num_ratios = ouroboros_num_ratios[r];
    const float harmonic = blockHarmonic[channel] * (num_ratios - 1.001f);

    MAKE_INTEGRAL_FRACTIONAL(harmonic);
    harmonic_fractional = 8.0f * (harmonic_fractional - 0.5f) + 0.5f;
    CONSTRAIN(harmonic_fractional, 0.0f, 1.0f);
    // harmonic_integral can go out of bounds with CV if harmonics set to cv_slider.
    CONSTRAIN(harmonic_integral, 0, num_ratios - 2);
    const float ratio = channel == 0 ? 1.0f : Crossfade(
        ratios[harmonic_integral],
        ratios[harmonic_integral + 1],
        harmonic_fractional);
    ONE_POLE(
        channel_amplitude[channel],
        channel == 0 ? 1.0f : std::max(blockAmplitude[channel] - 0.01f, 0.0f),
        0.2f);
    const float amplitude = channel_amplitude[channel];

    bool trigger = false;
    for (size_t i = 0; i < size; ++i) {
      trigger = trigger || (block->input[channel][i] & GATE_FLAG_RISING);
    }
    if (trigger || !block->input_patched[channel]) {
      channel_envelope[channel] = 1.0f;
    } else {
      channel_envelope[channel] *= 0.999f;
    }
    // For some reason, trigger can be true when no input is patched.
    if (lfo && (reset_all || (block->input_patched[channel] && trigger))) {
      oscillator[channel].Init();
    }
    ui.set_slider_led(
        channel, channel_envelope[channel] * amplitude > 0.02f, 1);
    const float f = f0 * ratio;

    uint8_t waveshape = (settings.state().segment_configuration[channel] & 0b01110000) >> 4;
    switch (waveshape) {
      case 0:
        oscillator[channel].Render<OSCILLATOR_SHAPE_SINE>(
            f, 0.5f, this_channel, size);
        break;
      case 1:
        oscillator[channel].Render<OSCILLATOR_SHAPE_TRIANGLE>(
            f, 0.5f, this_channel, size);
        break;
      case 2:
      case 3:
        oscillator[channel].Render<OSCILLATOR_SHAPE_SQUARE>(
            f, 0.5f, this_channel, size);
        break;
      case 4:
        oscillator[channel].Render<OSCILLATOR_SHAPE_SAW>(
            f, 0.5f, this_channel, size);
        break;
      case 5:
        oscillator[channel].Render<OSCILLATOR_SHAPE_SQUARE>(
            f, 0.75f, this_channel, size);
        break;
      case 6:
      case 7:
        oscillator[channel].Render<OSCILLATOR_SHAPE_SQUARE>(
            f, 0.9f, this_channel, size);
        break;
    }

    ParameterInterpolator am(
        &previous_amplitude[channel],
        amplitude * amplitude * channel_envelope[channel],
        size);
    for (size_t i = 0; i < size; ++i) {
      sum[i] += this_channel[i] * am.Next();
    }

    const float gain = channel == 0 ? 0.2f : 0.66f;
    // Don't bother interpolating over lfo amplitude as we don't apply pinging to the single LFO outs
    const float lfo_amp = lfo ? amplitude : 1.0f;
    const float* source = channel == 0 ? sum : this_channel;
    for (size_t i = 0; i < size; ++i) {
      block->output[channel][i] = settings.dac_code(channel, source[i] * gain * lfo_amp);
    }
  }
}

void ProcessTest(IOBuffer::Block* block, size_t size) {

  for (size_t channel = 0; channel < kNumChannels; channel++) {

    // Pot position affects LED color
    const float pot = block->pot[channel];
    ui.set_led(channel, pot > 0.5f ? LED_COLOR_GREEN : LED_COLOR_OFF);

    // Gete input and button turn the LED red
    bool gate = false;
    bool button = ui.switches().pressed(channel);
    if (block->input_patched[channel]) {
      for (size_t i = 0; i < size; i++) {
        gate = gate || (block->input[channel][i] & GATE_FLAG_HIGH);
      }
    }
    if (gate || button) {
      ui.set_led(channel, LED_COLOR_RED);
    }

    // Slider position (summed with input CV) affects output value
    const float output = (gate || button) ? 1.0f : block->cv_slider[channel];
    ui.set_slider_led(channel, output > 0.001f, 1);
    for (size_t i = 0; i < size; i++) {
      block->output[channel][i] = settings.dac_code(channel, output);
    }

  }

}

void Init() {
  System sys;
  sys.Init(true);
  dac.Init(int(kSampleRate), 2);
  gate_inputs.Init();
  io_buffer.Init();

  bool freshly_baked = !settings.Init();
  
  for (size_t i = 0; i < kNumChannels + kMaxNumSegments; ++i) {
    note_quantizer[i].Init(13, 0.03f, false);
  }
  for (size_t i = 0; i < kNumChannels; ++i) {
    segment_generator[i].Init((MultiMode) settings.state().multimode, &note_quantizer[i]);
    oscillator[i].Init();
  }
  std::fill(&no_gate[0], &no_gate[kBlockSize], GATE_FLAG_LOW);

  cv_reader.Init(&settings, &chain_state);
  eg_mode.Init(&settings);
  ui.Init(&settings, &chain_state, &cv_reader, &eg_mode);
  eg_mode.SetUI(&ui);

  if (freshly_baked && !skip_factory_test) {
    factory_test.Start(&settings, &cv_reader, &gate_inputs, &ui);
    ui.set_factory_test(true);
  } else {
    chain_state.Init(&left_link, &right_link, settings);
  }

  sys.StartTimers();
  dac.Start(&FillBuffer);
}

int main(void) {

  Init();

  while (1) {
    if (factory_test.running()) {
      io_buffer.Process(&FactoryTest::ProcessFn);
    } else if (
        chain_state.status() == stages::ChainState::CHAIN_DISCOVERING_NEIGHBORS
        || chain_state.status() == stages::ChainState::CHAIN_REINITIALIZING) {
      io_buffer.Process(&Process); // Still discovering neighbors, dont't process alternative multi-modes
    } else {
      switch ((MultiMode) settings.state().multimode) {
        case MULTI_MODE_SIX_IDENTICAL_EGS:
        case MULTI_MODE_SIX_INDEPENDENT_EGS:
          io_buffer.Process(&ProcessEnvelopes);
          break;
        case MULTI_MODE_OUROBOROS:
        case MULTI_MODE_OUROBOROS_ALTERNATE:
          io_buffer.Process(&ProcessOuroboros);
          break;
        default:
          io_buffer.Process(&Process);
          break;
      }
    }
  }

}

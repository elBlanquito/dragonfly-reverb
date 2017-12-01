/*
 * DISTRHO MVerb, a DPF'ied MVerb.
 * Copyright (c) 2010 Martin Eastwood
 * Copyright (C) 2015 Filipe Coelho <falktx@falktx.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of
 * the License, or any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * For a full copy of the GNU General Public License see the LICENSE file.
 */

#include "DragonflyReverbPlugin.hpp"

#define NUM_PARAMS 25
#define NUM_HIGH_LEVEL_PARAMS 4
#define PARAM_NAME_LENGTH 16
#define PARAM_SYMBOL_LENGTH 8

START_NAMESPACE_DISTRHO

typedef struct {
  const char *name;
  const char *symbol;
  float range_min;
  float range_def;
  float range_max;
  const char *unit;
} Param;

enum {
  // High level parameters, all integer values of 1, 2, 3, 4, or 5.
  SIZE, SHAPE, TONALITY, PRESENCE,
  // Low level parameters
  DRY_LEVEL, EARLY_LEVEL, LATE_LEVEL,
  EARLY_SIZE, EARLY_WIDTH, EARLY_LPF, EARLY_HPF, EARLY_SEND,
  LATE_PREDELAY, LATE_DECAY, LATE_SIZE, LATE_WIDTH, LATE_LPF, LATE_HPF,
  DIFFUSE, LOW_XOVER, LOW_MULT, HIGH_XOVER, HIGH_MULT, SPIN, WANDER
};

static Param params[NUM_PARAMS] = {
  {"Size",            "size",         0.0f,    2.0f,     4.0f,    ""},
  {"Shape",           "shape",        0.0f,    2.0f,     4.0f,    ""},
  {"Tonality",        "tonality",     0.0f,    2.0f,     4.0f,    ""},
  {"Presence",        "presense",     0.0f,    2.0f,     4.0f,    ""},
  {"Dry Level",       "dry",          0.0f,   50.0f,   100.0f,   "%"},
  {"Early Level",     "e_lev",        0.0f,   50.0f,   100.0f,   "%"},
  {"Late Level",      "l_level",      0.0f,   50.0f,   100.0f,   "%"},
  {"Early Size",      "e_size",       1.0f,    5.0f,    10.0f,   "m"},
  {"Early Width",     "e_width",     10.0f,   40.0f,   200.0f,   "%"},
  {"Early Low Pass",  "e_lpf",     2000.0f, 7500.0f, 20000.0f,  "Hz"},
  {"Early High Pass", "e_hpf",        0.0f,    4.0f,   100.0f,  "Hz"},
  {"Early Send",      "e_send",       0.0f,   30.0f,   100.0f,   "%"},
  {"Late Predelay",   "l_delay",      0.0f,   14.0f,   100.0f,  "ms"},
  {"Late Decay Time", "l_time",       0.1f,    2.0f,    10.0f, "sec"},
  {"Late Size",       "l_size",      10.0f,   40.0f,   100.0f,   "m"},
  {"Late Width",      "l_width",     10.0f,   90.0f,   200.0f,   "%"},
  {"Late Low Pass",   "l_lpf",     2000.0f, 7500.0f, 20000.0f,  "Hz"},
  {"Late High Pass",  "l_hpf",        0.0f,    4.0f,   100.0f,  "Hz"},
  {"Diffuse",         "diffuse",      0.0f,   80.0f,   100.0f,   "%"},
  {"Low Crossover",   "lo_xo",      100.0f,  600.0f,  1000.0f,  "Hz"},
  {"Low Decay Mult",  "lo_mult",      0.1f,    1.5f,     4.0f,   "X"},
  {"High Crossover",  "hi_xo",     1000.0f, 4500.0f, 20000.0f,  "Hz"},
  {"High Decay Mult", "hi_mult",      0.1f,    0.4f,     2.0f,   "X"},
  {"Spin",            "spin",         0.1f,    3.0f,     5.0f,  "Hz"},
  {"Wander",          "wander",       0.0f,   15.0f,    30.0f,  "ms"}
};

static const float DRY_LEVEL_BY_PRESENCE[] = {
  70.0, 60.0, 50.0, 40.0, 30.0
};

static const float EARLY_LEVEL_BY_PRESENCE[] = {
  75.0, 50.0, 25.0, 0.0, 0.0
};

static const float LATE_LEVEL_BY_PRESENCE[] = {
  40.0, 45.0, 50.0, 55.0, 60.0
};

static const float EARLY_SIZE_BY_SIZE[] = {
  3.0, 4.0, 5.0, 6.0, 7.0
};

static const float EARLY_SIZE_SHAPE_MULTIPLIER[] = {
  1.2, 1.1, 1.0, 0.9, 0.8
};

static const float EARLY_WIDTH_BY_SHAPE[] = {
  40.0, 60.0, 80.0, 100.0, 120.0
};

static const float EARLY_LPF_BY_SIZE[] = {
  9000.0, 8000.0, 7000.0, 6000.0, 5000.0
};

static const float EARLY_LPF_TONALITY_MULTIPLIER[] = {
  0.50, 0.75, 1.0, 1.25, 1.50
};

static const float LATE_PREDELAY_BY_SIZE[] = {
  12.0, 24.0, 36.0, 48.0, 60.0
};

static const float LATE_PREDELAY_PRESENCE_MULTIPLIER[] = {
  1.0, 0.75, 0.5, 0.25, 0.0 // no predelay in back
};

static const float LATE_DECAY_BY_SIZE[] = {
  0.75, 1.5, 2.25, 3.0, 4.0
};

static const float LATE_DECAY_TONALITY_MULTIPLIER[] = {
  0.8, 0.9, 1.0, 1.1, 1.2
};

static const float LATE_SIZE_BY_SIZE[] = {
  12.0, 24.0, 36.0, 48.0, 60.0
};

static const float LATE_SIZE_SHAPE_MULTIPLIER[] = {
  1.2, 1.1, 1.0, 0.9, 0.8
};

static const float LATE_WIDTH_BY_SHAPE[] = {
  80.0, 90.0, 100.0, 110.0, 120.0
};

static const float LATE_LPF_BY_SIZE[] = {
  7000.0, 6000.0, 5000.0, 4500.0, 4000.0
};

static const float LATE_LPF_TONALITY_MULTIPLIER[] = {
  0.50, 0.75, 1.0, 1.25, 1.50
};

static const float LOW_XOVER_BY_SIZE[] = {
  400.0, 500.0, 600.0, 700.0, 800.0
};

static const float LOW_MULT_BY_SIZE[] = {
  0.9, 1.5, 2.0, 2.5, 3.0
};

static const float LOW_MULT_TONALITY_MULTIPLIER[] = {
  1.2, 1.1, 1.0, 0.9, 0.8
};

static const float HIGH_XOVER_BY_SIZE[] = {
  6000.0, 5500.0, 4500.0, 4000.0, 3000.0
};

static const float HIGH_MULT_BY_SIZE[] = {
  0.50, 0.40, 0.35, 0.32, 0.30
};

static const float HIGH_MULT_TONALITY_MULTIPLIER[] = {
  0.9, 1.0, 1.0, 1.1, 1.2
};

static const float SPIN_BY_SIZE[] = {
  0.3, 2.5, 2.9, 2.1, 1.9
};

static const float WANDER_BY_SIZE[] = {
  27.0, 13.0, 15.0, 20.0, 17.0
};

// -----------------------------------------------------------------------

DragonflyReverbPlugin::DragonflyReverbPlugin() : Plugin(NUM_PARAMS, 0, 0) // 0 presets, 0 states
{
    early.loadPresetReflection(FV3_EARLYREF_PRESET_1);
    early.setMuteOnChange(true);
    early.setdryr(0); // mute dry signal
    early.setwet(0); // 0dB
    early.setLRDelay(0.3);
    early.setLRCrossApFreq(750, 4);
    early.setDiffusionApFreq(150, 4);
    early.setSampleRate(getSampleRate());

    late.setMuteOnChange(true);
    late.setwet(0); // 0dB
    late.setdryr(0); // mute dry signal
    late.setSampleRate(getSampleRate());
    late.setlfo1freq(0.9);
    late.setlfo2freq(1.3);
    late.setlfofactor(0.3);
    late.setspinfactor(0.3);
}

// -----------------------------------------------------------------------
// Init

void DragonflyReverbPlugin::initParameter(uint32_t index, Parameter& parameter)
{
    if (index < NUM_PARAMS)
    {
      parameter.hints      = kParameterIsAutomable;
      parameter.name       = params[index].name;
      parameter.symbol     = params[index].symbol;
      parameter.ranges.min = params[index].range_min;
      parameter.ranges.def = params[index].range_def;
      parameter.ranges.max = params[index].range_max;
      parameter.unit       = params[index].unit;
    }
}

// -----------------------------------------------------------------------
// Internal data

float DragonflyReverbPlugin::getParameterValue(uint32_t index) const
{
    switch(index) {
      case          SIZE: return size;
      case         SHAPE: return shape;
      case      TONALITY: return tonality;
      case      PRESENCE: return presence;
      case     DRY_LEVEL: return dry_level * 100.0;
      case   EARLY_LEVEL: return early_level * 100.0;
      case    LATE_LEVEL: return late_level * 100.0;
      case    EARLY_SIZE: return early.getRSFactor() * 7.0;
      case   EARLY_WIDTH: return early.getwidth() * 100.0;
      case     EARLY_LPF: return early.getoutputlpf();
      case     EARLY_HPF: return early.getoutputhpf();
      case    EARLY_SEND: return early_send * 100.0;
      case LATE_PREDELAY: return late.getPreDelay();
      case    LATE_DECAY: return late.getrt60();
      case     LATE_SIZE: return late.getRSFactor() * 80.0;
      case    LATE_WIDTH: return late.getwidth() * 100.0;
      case      LATE_LPF: return late.getoutputlpf();
      case      LATE_HPF: return late.getoutputhpf();
      case       DIFFUSE: return late.getidiffusion1() * 100.0;
      case     LOW_XOVER: return late.getxover_low();
      case      LOW_MULT: return late.getrt60_factor_low();
      case    HIGH_XOVER: return late.getxover_high();
      case     HIGH_MULT: return late.getrt60_factor_high();
      case          SPIN: return late.getspin();
      case        WANDER: return late.getwander();
    }

    return 0.0;
}

void DragonflyReverbPlugin::setParameterValue(uint32_t index, float value)
{
  if (index < NUM_HIGH_LEVEL_PARAMS)
  {
    setHighLevelParameterValue(index, value);
  }
  else switch(index)
  {
    case     DRY_LEVEL: dry_level        = (value / 100.0); break;
    case   EARLY_LEVEL: early_level      = (value / 100.0); break;
    case    LATE_LEVEL: late_level       = (value / 100.0); break;
    case    EARLY_SIZE: early.setRSFactor  (value / 7.0);   break;
    case   EARLY_WIDTH: early.setwidth     (value / 100.0); break;
    case     EARLY_LPF: early.setoutputlpf (value);         break;
    case     EARLY_HPF: early.setoutputhpf (value);         break;
    case    EARLY_SEND: early_send       = (value / 100.0); break;
    case LATE_PREDELAY: late.setPreDelay   (value);         break;
    case    LATE_DECAY: late.setrt60       (value);         break;
    case     LATE_SIZE: late.setRSFactor   (value / 80.0);  break;
    case    LATE_WIDTH: late.setwidth      (value / 100.0); break;
    case      LATE_LPF: late.setoutputlpf  (value);         break;
    case      LATE_HPF: late.setoutputhpf  (value);         break;
    case       DIFFUSE: late.setidiffusion1(value / 100.0 * 0.75);
                        late.setapfeedback (value / 100.0 * 0.75);
                                                            break;
    case     LOW_XOVER: late.setxover_low  (value);         break;
    case      LOW_MULT: late.setrt60_factor_low(value);     break;
    case    HIGH_XOVER: late.setxover_high (value);         break;
    case     HIGH_MULT: late.setrt60_factor_high(value);    break;
    case          SPIN: late.setspin       (value);         break;
    case        WANDER: late.setwander     (value);         break;
  }
}

void DragonflyReverbPlugin::setHighLevelParameterValue(uint32_t index, float value)
{
  uint32_t int_value = (uint32_t)(value + 0.5);  // Round to closest int

  switch(index) {                                //        0 1 2 3 4
    case     SIZE:     size = int_value; break;  //  small <-------> large
    case    SHAPE:    shape = int_value; break;  // narrow <-------> wide
    case TONALITY: tonality = int_value; break;  //   dark <-------> bright
    case PRESENCE: presence = int_value; break;  //  front <-------> back
  }

  setParameterValue(DRY_LEVEL, DRY_LEVEL_BY_PRESENCE[presence]);
  setParameterValue(EARLY_LEVEL, EARLY_LEVEL_BY_PRESENCE[presence]);
  setParameterValue(LATE_LEVEL, LATE_LEVEL_BY_PRESENCE[presence]);
  setParameterValue(EARLY_SIZE, EARLY_SIZE_BY_SIZE[size] * EARLY_SIZE_SHAPE_MULTIPLIER[shape]);
  setParameterValue(EARLY_WIDTH, EARLY_WIDTH_BY_SHAPE[shape]);
  setParameterValue(EARLY_LPF, EARLY_LPF_BY_SIZE[size] * EARLY_LPF_TONALITY_MULTIPLIER[tonality]);
  setParameterValue(EARLY_HPF, 4.0);
  setParameterValue(EARLY_SEND, 30.0);
  setParameterValue(LATE_PREDELAY, LATE_PREDELAY_BY_SIZE[size] * LATE_PREDELAY_PRESENCE_MULTIPLIER[presence]);
  setParameterValue(LATE_DECAY, LATE_DECAY_BY_SIZE[size] * LATE_DECAY_TONALITY_MULTIPLIER[tonality]);
  setParameterValue(LATE_SIZE, LATE_SIZE_BY_SIZE[size] * LATE_SIZE_SHAPE_MULTIPLIER[shape]);
  setParameterValue(LATE_WIDTH, 0.80 + (shape * 10.0));
  setParameterValue(LATE_LPF, LATE_LPF_BY_SIZE[size] * LATE_LPF_TONALITY_MULTIPLIER[tonality]);
  setParameterValue(LATE_HPF, 4.0);
  setParameterValue(DIFFUSE, 90.0); // TODO: Possibly introduce a new high level param for diffuse <-------> resonate
  setParameterValue(LOW_XOVER, LOW_XOVER_BY_SIZE[size]);
  setParameterValue(LOW_MULT, LOW_MULT_BY_SIZE[size] * LOW_MULT_TONALITY_MULTIPLIER[tonality]);
  setParameterValue(HIGH_XOVER, HIGH_XOVER_BY_SIZE[size]);
  setParameterValue(HIGH_MULT, HIGH_MULT_BY_SIZE[size] * HIGH_MULT_TONALITY_MULTIPLIER[tonality]);
  setParameterValue(SPIN, SPIN_BY_SIZE[size]);
  setParameterValue(WANDER, WANDER_BY_SIZE[size]);
}

// -----------------------------------------------------------------------
// Process

void DragonflyReverbPlugin::activate()
{
  early.setSampleRate(getSampleRate());
  late.setSampleRate(getSampleRate());
}

void DragonflyReverbPlugin::run(const float** inputs, float** outputs, uint32_t frames)
{
  for (uint32_t offset = 0; offset < frames; offset += BUFFER_SIZE) {
    long int buffer_frames = frames - offset < BUFFER_SIZE ? frames - offset : BUFFER_SIZE;

    early.processreplace(
        const_cast<float *>(inputs[0] + offset),
        const_cast<float *>(inputs[1] + offset),
        early_out_buffer[0],
        early_out_buffer[1],
        buffer_frames
    );

    for (uint32_t i = 0; i < buffer_frames; i++) {
      late_in_buffer[0][i] = early_send * early_out_buffer[0][i] + inputs[0][offset + i];
      late_in_buffer[1][i] = early_send * early_out_buffer[1][i] + inputs[1][offset + i];
    }

    late.processreplace(
      const_cast<float *>(late_in_buffer[0]),
      const_cast<float *>(late_in_buffer[1]),
      late_out_buffer[0],
      late_out_buffer[1],
      buffer_frames
    );

    for (uint32_t i = 0; i < buffer_frames; i++) {
      outputs[0][offset + i] =
        dry_level   * inputs[0][offset + i]  +
        early_level * early_out_buffer[0][i] +
        late_level  * late_out_buffer[0][i];

      outputs[1][offset + i] =
        dry_level   * inputs[1][offset + i]  +
        early_level * early_out_buffer[1][i] +
        late_level  * late_out_buffer[1][i];
    }
  }
}

// -----------------------------------------------------------------------
// Callbacks

void DragonflyReverbPlugin::sampleRateChanged(double newSampleRate)
{
    early.setSampleRate(newSampleRate);
    late.setSampleRate(newSampleRate);
}

// -----------------------------------------------------------------------

Plugin* createPlugin()
{
    return new DragonflyReverbPlugin();
}

// -----------------------------------------------------------------------

END_NAMESPACE_DISTRHO

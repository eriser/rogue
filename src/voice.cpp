/*
 * rogue - multimode synth
 *
 * Copyright (C) 2013 Timo Westkämper
 *
 * uses code from https://github.com/rekado/lv2-mdaPiano
 *                https://github.com/smbolton/whysynth
 */

#include <stdio.h>
#include "voice.h"

#define SEMITONE 1.05946f

namespace rogue {

// TODO use table for this
static float midi2f(unsigned char data) {
    return 0.007874f * (float)(data);
}

static float midi2hz(float key) {
    return 8.177445f * std::pow(SEMITONE, key);
}

rogueVoice::rogueVoice(double rate, SynthData* d) {
    data = d;
    sample_rate = rate;

    for (int i = 0; i < NOSC; i++) oscs[i] = Osc();
    for (int i = 0; i < NDCF; i++) filters[i] = Filter();
    for (int i = 0; i < NLFO; i++) lfos[i] = LFO();
    for (int i = 0; i < NENV; i++) envs[i] = Env();

    // set sample rate
    for (int i = 0; i < NOSC; i++) oscs[i].setSamplerate(rate);
    for (int i = 0; i < NDCF; i++) filters[i].setSamplerate(rate);

    mod[M_ON] = 1.0;
}

void rogueVoice::on(unsigned char key, unsigned char velocity) {
    std::cout << "on " << int(key) << " " << int(velocity) << std::endl;
    // store key that turned this voice on (used in 'get_key')
    m_key = key;

    mod[M_KEY] = midi2f(key);
    mod[M_VEL] = midi2f(velocity);

    if (velocity > 0) {
        m_velocity = velocity;
    } else {
        off(0);
    }

    // trigger on
    for (int i = 0; i < NLFO; i++) lfos[i].on();
    for (int i = 0; i < NENV; i++) envs[i].on();
    for (int i = 0; i < NOSC; i++) {
        if (!data->oscs[i].free) oscs[i].reset();
    }

    in_sustain = false;
}

void rogueVoice::off(unsigned char velocity) {
    std::cout << "off " << int(m_key) << " " << int(velocity) << std::endl;
    // trigger off
    for (int i = 0; i < NLFO; i++) lfos[i].off();
    for (int i = 0; i < NENV; i++) envs[i].off();

    //Mark the voice to be turned off later. It may not be set to
    //INVALID_KEY yet, because the release sound still needs to be
    //rendered.  m_key is finally set to INVALID_KEY by 'render' when
    //env < SILENCE
    in_sustain = true;
}

void rogueVoice::runLFO(int i, uint32_t from, uint32_t to) {
    LFOData& lfoData = data->lfos[i];
    LFO& lfo = lfos[i];
    float v = 0.0f;
    if (lfoData.on) {
        float f = lfoData.freq;
        // key to f
        if (lfoData.key_to_f != 0.0f) {
            f *= std::pow(SEMITONE, lfoData.key_to_f * float(m_key - 69));
        }

        // TODO freq modulation

        lfo.lfo.setType(lfoData.type);
        lfo.lfo.setEnv(lfoData.attack, lfoData.decay);
        lfo.lfo.setFreq(lfoData.freq);
        lfo.lfo.setSymmetry(lfoData.symmetry);
        // TODO humanize
        // TODO reset type
        v = lfo.lfo.tick(to - from);

        // TODO amp modulation
    }
    // update mod values
    mod[M_LFO1_BI + 2*i] = v;
    mod[M_LFO1_UN + 2*i] = 0.5 * v + 0.5;

    lfo.last = lfo.current;
    lfo.current = 0.0f;
}

void rogueVoice::runEnv(int i, uint32_t from, uint32_t to) {
    EnvData& envData = data->envs[i];
    Env& env = envs[i];
    float v = 0.0f;
    if (envData.on) {
        float f = 1.0f;
        // key to speed
        if (envData.key_to_speed != 0.0f) {
            f *= std::pow(SEMITONE, envData.key_to_speed * float(m_key - 69));
        }
        // vel to speed
        if (envData.vel_to_speed != 0.0f) {
            f *= std::pow(SEMITONE, envData.vel_to_speed * float(m_velocity - 64));
        }
        float a = envData.attack / f;
        float h = envData.hold / f;
        float d = envData.decay / f;
        float s = envData.sustain;
        float r = envData.release / f;
        // TODO pre-delay
        // TODO retrigger
        env.env.setAHDSR(a, h, d, s, r);
        v = env.env.tick(to - from);
        if (envData.vel_to_vol > 0.0f) {
            v *= 1.0 - envData.vel_to_vol + envData.vel_to_vol * midi2f(m_velocity);
        }

        // TODO amp modulation
    }
    // update mod values
    mod[M_EG1 + i] = v;

    env.last = env.current;
    env.current = v;
}

void rogueVoice::runOsc(int i, uint32_t from, uint32_t to) {
    OscData& oscData = data->oscs[i];
    Osc& osc = oscs[i];
    if (oscData.on) {
        float f = 440.0;
        if (oscData.tracking) {
            f = midi2hz(float(m_key) + oscData.coarse + oscData.fine);
        }
        f *= oscData.ratio;

        // TODO pitch modulation
        // TODO mod modulation
        // TODO amp modulation

        osc.osc.setType(oscData.type);
        osc.osc.setFreq(f);
        osc.osc.setParams(oscData.param1, oscData.param2);
        osc.osc.process(osc.buffer + from, to - from);
        float v = oscData.level;
        if (oscData.inv) {
            v *= -1.0f;
        }
        if (oscData.vel_to_vol > 0.0f) {
            v *= 1.0 - oscData.vel_to_vol + oscData.vel_to_vol * midi2f(m_velocity);
        }
        if (oscData.level_a > 0.0f) {
            float vv = v * oscData.level_a;
            for (int i = from; i < to; i++) {
                bus_a[i] += vv * osc.buffer[i];
            }
        }
        if (oscData.level_b > 0.0f) {
            float vv = v * oscData.level_b;
            for (int i = from; i < to; i++) {
                bus_b[i] += vv * osc.buffer[i];
            }
        }
    }
}

void rogueVoice::runFilter(int i, uint32_t from, uint32_t to) {
    FilterData& filterData = data->filters[i];
    Filter& filter = filters[i];
    if (filterData.on) {
        int type = filterData.type;
        float f = filterData.freq;
        // key to f
        if (filterData.key_to_f != 0.0f) {
            f *= std::pow(SEMITONE, filterData.key_to_f * float(m_key - 69));
        }
        // vel to f
        if (filterData.vel_to_f != 0.0f) {
            f *= std::pow(SEMITONE, filterData.vel_to_f * float(m_velocity - 64));
        }

        // TODO freq modulation
        // TODO q modulation
        // TODO pan modulation
        // TODO amp modulation

        float* source;
        switch (filterData.source) {
        case 0: source = bus_a; break;
        case 1: source = bus_b; break;
        case 2: source = filters[0].buffer;
        }
        if (type < 8) {
            filter.moog.setType(type);
            filter.moog.setCoefficients(f, filterData.q);
            filter.moog.process(source + from, filter.buffer + from, to - from);
        } else {
            filter.svf.setType(type - 8);
            filter.svf.setCoefficients(f, filterData.q);
            filter.svf.process(source + from, filter.buffer + from, to - from);
        }
    }
}

void rogueVoice::render(uint32_t from, uint32_t to) {
    uint32_t from_ = from % BUFFER_SIZE;
    uint32_t off = from - from_;
    while (off < to) {
        render(from_, std::min(to - off, uint32_t(BUFFER_SIZE)), off);
        off += BUFFER_SIZE;
        from_ = 0;
    }
}

void rogueVoice::render(uint32_t from, uint32_t to, uint32_t off) {
    if (m_key == lvtk::INVALID_KEY) {
        return;
    }

    // reset buses
    std::memset(bus_a, 0, sizeof(float) * BUFFER_SIZE);
    std::memset(bus_b, 0, sizeof(float) * BUFFER_SIZE);

    // run elements
    for (int i = 0; i < NLFO; i++) runLFO(i, from, to);
    for (int i = 0; i < NENV; i++) runEnv(i, from, to);
    for (int i = 0; i < NOSC; i++) runOsc(i, from, to);
    for (int i = 0; i < NDCF; i++) runFilter(i, from, to);

    // copy buses and filters to out
    float f1_l = data->filters[0].level * (1.0 * data->filters[0].pan),
          f1_r = data->filters[0].level * data->filters[0].pan,
          f2_l = data->filters[1].level * (1.0 * data->filters[1].pan),
          f2_r = data->filters[1].level * data->filters[1].pan,
          ba_l = data->bus_a_level * (1.0 - data->bus_a_pan),
          ba_r = data->bus_a_level * data->bus_a_pan,
          bb_l = data->bus_b_level * (1.0 - data->bus_b_pan),
          bb_r = data->bus_b_level * data->bus_b_pan;

    float e_from = envs[0].last;
    float e_step = (envs[0].current - e_from) / float(to - from);

    float* left = p(p_left);
    float* right = p(p_right);
    for (int i = from; i < to; i++) {
        left[off + i] += data->volume * e_from *
                  (f1_l * filters[0].buffer[i] +
                   f2_l * filters[1].buffer[i] +
                   ba_l * bus_a[i] +
                   bb_l * bus_b[i]);
        right[off + i] += data->volume * e_from *
                   (f1_r * filters[0].buffer[i] +
                    f2_r * filters[1].buffer[i] +
                    ba_r * bus_a[i] +
                    bb_r * bus_b[i]);
        e_from += e_step;
    }

    if (envs[0].current < SILENCE) {
        m_key = lvtk::INVALID_KEY;
        in_sustain = false;
    }
}

void rogueVoice::reset() {
    volume = 1.0f;
    m_key = lvtk::INVALID_KEY;
    in_sustain = false;
}


}

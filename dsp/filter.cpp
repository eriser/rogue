/*
 * rogue - multimode synth
 *
 * Copyright (C) 2013 Timo Westkämper
 */

#include "filter.h"

#include <algorithm>
#include <math.h>
#include "types.h"

namespace dsp {

// DCBlocker

void DCBlocker::clear() {
    x1 = y1 = 0.0f;
}

void DCBlocker::setSamplerate(float r) {
    R = 1.0f - (M_PI * 2.0f * 20.0f / r);
}

void DCBlocker::process(float* input, float* output, int samples) {
    for (uint i = 0; i < samples; i++) {
        // y(n) = x(n) - x(n - 1) + R * y(n - 1)
        float y = input[i] - x1 + R * y1;
        x1 = input[i];
        y1 = y;
        output[i] = y;
    }
}

// OnePole

void OnePole::clear(){
    last_ = 0.0;
}

void OnePole::setCoefficients(float b0, float a1) {
    b0_ = b0;
    a1_ = a1;
}

void OnePole::setPole(double p) {
    b0_ = (p > 0.0) ? (1.0 - p) : (1.0 + p);
    a1_ = -p;
}

float OnePole::process(float input) {
    last_ = b0_ * input - a1_ * last_;
    return last_;
}

void OnePole::process(float* input, float* output, int samples) {
    for (uint i = 0; i < samples; i++) {
        last_ = b0_ * input[i] - a1_ * last_;
        output[i] = last_;
    }
}

// OneZero

void OneZero::clear() {
    last_ = prevIn_ = 0.0;
}

void OneZero::setCoefficients(float b0, float b1) {
    b0_ = b0;
    b1_ = b1;
}

void OneZero::setZero(float z) {
    b0_ = (z > 0.0) ? (1.0 + z) : (1.0 - z);
    b1_ = -z * b0_;
}

float OneZero::process(float input) {
    last_ = b0_ * input + b1_ * prevIn_;
    prevIn_ = input;
    return last_;
}

void OneZero::process(float* input, float* output, int samples) {
    for (uint i = 0; i < samples; i++) {
        last_ = b0_ * input[i] + b1_ * prevIn_;
        prevIn_ = input[i];
        output[i] = last_;
    }
}

// PoleZero

void PoleZero::clear() {
    last_ = prevIn_ = 0.0;
}

void PoleZero::setCoefficients(float b0, float b1, float a1) {
    b0_ = b0;
    b1_ = b1;
    a1_ = a1;
}

void PoleZero::setAllpass(float coefficient) {
    b0_ = coefficient;
    b1_ = 1.0;
    a0_ = 1.0; // just in case
    a1_ = coefficient;
}

void PoleZero::setBlockZero(float thePole) {
    b0_ = 1.0;
    b1_ = -1.0;
    a0_ = 1.0; // just in case
    a1_ = -thePole;
}

float PoleZero::process(float input) {
    last_ = b0_ * input + b1_ * prevIn_ - a1_ * last_;
    prevIn_ = input;
    return last_;
}

void PoleZero::process(float* input, float* output, int samples) {
    for (uint i = 0; i < samples; i++) {
        last_ = b0_ * input[i] + b1_ * prevIn_ - a1_ * last_;
        prevIn_ = input[i];
        output[i] = last_;
    }
}

// TwoPole

void TwoPole::clear() {
    last_ = last__ = 0.0;
}

void TwoPole::setCoefficients(float b0, float a1, float a2) {
    b0_ = b0;
    a1_ = a1;
    a2_ = a2;
}

float TwoPole::process(float input) {
    float temp = last_;
    last_ = b0_ * input - a1_ * last_ - a2_ * last__;
    last__ = temp;
    return last_;
}

void TwoPole::process(float* input, float* output, int samples) {
    for (uint i = 0; i < samples; i++) {
        float temp = last_;
        last_ = b0_ * input[i] - a1_ * last_ - a2_ * last__;
        last__ = temp;
        output[i] = last_;
    }
}

// TwoZero

void TwoZero::clear() {
    last_ = prevIn_ = prevIn__ = 0.0;
}

void TwoZero::setCoefficients(float b0, float b1, float b2) {
    b0_ = b0;
    b1_ = b1;
    b2_ = b2;
}

float TwoZero::process(float input) {
    last_ = b0_ * input + b1_ * prevIn_ + b2_ * prevIn__;
    prevIn__ = prevIn_;
    prevIn_ = input;
    return last_;
}

void TwoZero::process(float* input, float* output, int samples) {
    for (uint i = 0; i < samples; i++) {
        last_ = b0_ * input[i] + b1_ * prevIn_ + b2_ * prevIn__;
        prevIn__ = prevIn_;
        prevIn_ = input[i];
        output[i] = last_;
    }
}

// BiQuad

void BiQuad::clear() {
    last_ = last__ = prevIn_ = prevIn__ = 0.0;
}

void BiQuad::setCoefficients(float b0, float b1, float b2, float a1, float a2) {
    b0_ = b0;
    b1_ = b1;
    b2_ = b2;
    a1_ = a1;
    a2_ = a2;
}

float BiQuad::process(float input) {
    float temp = last_;
    last_ = b0_ * input + b1_ * prevIn_ + b2_ * prevIn__;
    last_ -= a1_ * temp + a2_ * last__;
    prevIn__ = prevIn_;
    prevIn_ = input;
    last__ = temp;
    return last_;
}

void BiQuad::process(float* input, float* output, int samples) {
    for (uint i = 0; i < samples; i++) {
        float temp = last_;
        last_ = b0_ * input[i] + b1_ * prevIn_ + b2_ * prevIn__;
        last_ -= a1_ * temp + a2_ * last__;
        prevIn__ = prevIn_;
        prevIn_ = input[i];
        last__ = temp;
        output[i] = last_;
    }
}

// MoogFilter

void MoogFilter::clear() {
    drive_ = 1.0;
    type_ = 0;
    gcomp_ = 0.5;
    dlout_[0] = dlout_[1] = dlout_[2] = dlout_[3] = dlout_[4] = 0.0;
    dlin_[0] = dlin_[1] = dlin_[2] = dlin_[3] = dlin_[4] = 0.0;
}

void MoogFilter::setCoefficients(float freq, float res) {
    wc_ = 2.0f * M_PI * freq / sample_rate_;
    g_ = 0.9892 * wc_ - 0.4342 * pow(wc_, 2) + 0.1381 * pow(wc_, 3) - 0.0202 * pow(wc_, 4);
    gres_ = res * (1.0029 + 0.0526 * wc_ - 0.926 * pow(wc_, 2) + 0.0218 * pow(wc_, 3));
}

void MoogFilter::compute(float in) {
    // first input (with saturation and feedback)
    dlout_[0] = (float) tanh(drive_ * (in - 4 * gres_ * (dlout_[4] - gcomp_ * in)));

    // four filter blocks
    for (uint i = 0; i < 4; i++) {
        dlout_[i+1] = g_ * (0.3/1.3 * dlout_[i] + 1/1.3 * dlin_[i] - dlout_[i + 1]) + dlout_[i + 1];
        dlin_[i] = dlout_[i];
    }
}

#define MOOG_LOOP(x) \
    for (uint i = 0; i < samples; i++) { \
        compute(input[i]); \
        output[i] = x; \
    }

void MoogFilter::process(float* input, float* output, int samples) {
    switch(type_) {
    case LP24: // 24dB LP
        MOOG_LOOP(dlout_[4])
        break;
    case LP18: // 18dB LP
        MOOG_LOOP(dlout_[3])
        break;
    case LP12: // 12dB LP
        MOOG_LOOP(dlout_[2])
        break;
    case LP6:  // 6db LP
        MOOG_LOOP(dlout_[1])
        break;
    case HP24: // 24dB HP
        MOOG_LOOP(dlout_[0] - dlout_[4])
        break;
    case BP12: // 12db BP
        MOOG_LOOP(dlout_[4] - dlout_[2])
        break;
    case BP18: // 18/6dB BP
        MOOG_LOOP(dlout_[3] - dlout_[4])
        break;
    case NOTCH:// NOTCH
        MOOG_LOOP((dlout_[3] - dlout_[4]) + 2/3 * dlout_[0])
        break;
    }
}

// StateVariableFilter

void StateVariableFilter::clear() {
    drive = 0.0;
    notch = low = high = band = 0.0;
    out = &low;
}

void StateVariableFilter::setType(int type) {
    switch(type) {
    case LP:
        out = &low; break;
    case HP:
        out = &high; break;
    case BP:
        out = &band; break;
    case NOTCH:
        out = &notch; break;
    }
}

void StateVariableFilter::setCoefficients(float fc, float res) {
    freq = 2.0 * sinf(M_PI * std::min(0.25, fc / (sample_rate * 2.0)));
    damp = std::min(2.0*(1.0 - pow(res, 0.25)), std::min(2.0, 2.0 / freq - freq * 0.5));
}

void StateVariableFilter::process(float* input, float* output, int samples) {
    for (uint i = 0; i < samples; i++) {
        // TODO figure out better upsampling method
        const float in = input[i];
        notch = in - damp * band;
        low   = low + freq * band;
        high  = notch - low;
        band  = freq * high + band - drive * band * band * band;
        output[i] = 0.5 * *out;

        notch = in - damp * band;
        low   = low + freq * band;
        high  = notch - low;
        band  = freq * high + band - drive * band * band * band;
        output[i] += 0.5 * *out;
    }
}

// StateVariableFilter 2

void StateVariableFilter2::clear() {
    v0z = v1 = v2 = 0.0f;
}

void StateVariableFilter2::setCoefficients(float fc, float res) {
    float g = tan(M_PI * fc / sample_rate);
    //float damping = 1.0f / res;
    //k = damping;
    k = 1.0 - 0.99 * res;
    float ginv = g / (1.0f + g * (g + k));
    g1 = ginv;
    g2 = 2.0f * (g + k) * ginv;
    g3 = g * ginv;
    g4 = 2.0f * ginv;
}

#define SVF2_LOOP(x) \
    for (uint i = 0; i < samples; i++) { \
        float v0 = input[i]; \
        float v1z = v1; \
        float v2z = v2; \
        float v3 = v0 + v0z - 2.0 * v2z; \
        v1 += g1 * v3 - g2 * v1z; \
        v2 += g3 * v3 + g4 * v1z; \
        v0z = v0; \
        output[i] = x; \
    }

void StateVariableFilter2::process(float* input, float* output, int samples) {
    switch (type) {
    case LP:
        SVF2_LOOP(v2);
        break;
    case BP:
        SVF2_LOOP(v1);
        break;
    case HP:
        SVF2_LOOP(v0 - k * v1 - 2);
        break;
    case NOTCH:
        SVF2_LOOP(v0 - k * v1);
        break;
    }
}

}

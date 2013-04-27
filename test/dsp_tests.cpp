#include <sndfile.hh>
#include "oscillator.cpp"
#include "filter.cpp"
#include "lfo.cpp"
#include "envelope.cpp"
#include "tables.cpp"

#include <iostream>

#define SR 44100.0
#define SIZE 44100
#define CHANNELS 1

void write_wav(char* filename, float* buffer) {
    static const int FORMAT = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
    SndfileHandle outfile(filename, SFM_WRITE, FORMAT, CHANNELS, SR);
    if (outfile) {
        outfile.write(&buffer[0], SIZE);
    }
}

bool has_clicks(float* buffer) {
    float threshold = 0.3f;
    float diff = 0.0f;
    float diff2 = 0.0f;
    for (int i = 1; i < SIZE; i++) {
        diff = buffer[i] - buffer[i - 1];
        if (fabs(diff) > threshold && fabs(diff2) > threshold &&
            ((diff < 0.0f && diff2 > 0.0f) || (diff > 0.0f && diff2 < 0.0))) {
            return true;
        }
        diff2 = diff;
    }
    return false;
}

int main() {
    char filename[50];
    float buffer[SIZE];
    dsp::PhaseShaping osc;
    osc.setSamplerate(SR);
    osc.setFreq(440.0f);
    osc.setWidth(0.5f);
    osc.setParams(1.0f, 0.0f);

    dsp::MoogFilter moog;
    moog.setSamplerate(SR);
    moog.setCoefficients(1000.0, 0.5);

    dsp::DCBlocker dcBlocker;
    dcBlocker.setSamplerate(SR);

    dsp::StateVariableFilter svf;
    svf.setSamplerate(SR);
    svf.setCoefficients(1000.0, 0.5);

    dsp::LFO lfo;
    lfo.setSamplerate(SR);
    lfo.setFreq(10.0);

    dsp::AHDSR env;

    // oscs
    int errors = 0;
    int total = 0;
    float params[] = {0.0f, 0.1f, 0.2f, 0.25f, 0.3f, 0.4f, 0.5f, 0.6f, 0.75f, 0.8f, 0.9f,
                      1.0f, 1.1f, 1.2f, 1.25f, 1.3f, 1.4f, 1.5f, 1.6f, 1.7f, 1.75f, 1.9f};
    for (int i = 0; i < 10; i++) { // type
        for (int j = 0; j < 22; j++) { // param1
            for (int k = 0; k < 11; k++) { // param2 & width
                osc.reset();
                osc.setType(i);
                osc.setParams(params[j], params[k]);
                osc.setWidth(params[k]);
                osc.process(buffer, SIZE);

                //dcBlocker.clear();
                //dcBlocker.process(buffer, buffer, SIZE);

                sprintf(filename, "wavs/osc_%i_%i_%i.wav", i, j, k);
                write_wav(filename, buffer);

                // click detection
                // noise is allowed to have clicks
                if (i != 9 && has_clicks(buffer)) {
                    std::cout << "ERROR: has clicks " << i << " "<< j << " " << k << std::endl;
                    errors++;
                }
                total++;
            }
        }
    }

    if (errors > 0) {
        std::cout << "ERROR: " << errors << "/" << total << " errors (osc)" << std::endl;
    }

    // noise input
    float noise[SIZE];
    osc.reset();
    osc.setFreq(1000.0);
    osc.setType(9);
    osc.process(noise, SIZE);

    // moog
    for (int i = 0; i < 8; i++) {
        moog.clear();
        moog.setType(i);
        moog.process(noise, buffer, SIZE);

        sprintf(filename, "wavs/moog_%i.wav", i);
        write_wav(filename, buffer);
    }

    // svf
    for (int i = 0; i < 4; i++) {
        svf.clear();
        svf.setType(i);
        svf.process(noise, buffer, SIZE);

        sprintf(filename, "wavs/svf_%i.wav", i);
        write_wav(filename, buffer);
    }

    // lfos
    for (int i = 0; i < 5; i++) {
        lfo.reset();
        lfo.setType(i);
        //lfo.process(buffer, SIZE);
        for (int j = 0; j < SIZE; j++) {
            buffer[j] = lfo.tick();
        }

        sprintf(filename, "wavs/lfo_%i.wav", i);
        write_wav(filename, buffer);
    }

    // envs
    env.setAHDSR(0.1 * SR, 0.2 * SR, 0.3 * SR, 0.9, 0.5 * SR);
    env.on();
    for (int j = 0; j < 0.5 * SIZE; j++) {
        buffer[j] = env.tick();
    }
    env.off();
    for (int j = 0.5* SIZE; j < SIZE; j++) {
        buffer[j] = env.tick();
    }
    sprintf(filename, "wavs/env_%i.wav", 0);
    write_wav(filename, buffer);

    env.on();
    for (int j = 0; j < 0.25 * SIZE; j++) {
        buffer[j] = env.tick();
    }
    env.off();
    for (int j = 0.25 * SIZE; j < 0.5 * SIZE; j++) {
        buffer[j] = env.tick();
    }
    env.on();
    for (int j = 0.5 * SIZE; j < 0.75 * SIZE; j++) {
        buffer[j] = env.tick();
    }
    env.off();
    for (int j = 0.75 * SIZE; j < SIZE; j++) {
        buffer[j] = env.tick();
    }
    sprintf(filename, "wavs/env_%i.wav", 1);
    write_wav(filename, buffer);


    return 0;
}

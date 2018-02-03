#pragma once

#include <cmath>

class AudioBuffer {
public:
    AudioBuffer(size_t length) : buffer(new Uint8[length]), length(length)
    {
    }

    ~AudioBuffer() {
        delete [] buffer;
    }

    Uint8 * const buffer;
    const size_t length;

    float avg=0, max=0;
    sf_count_t usedSamples;

    void calculateAverage() {
        auto samples = reinterpret_cast<short*>(buffer);
        unsigned long long int sum=0;
        auto maxVal=*samples;
        for (size_t i=0; i < usedSamples; i++) {
            sum+=samples[i]*samples[i];
            if (samples[i] > maxVal)
                maxVal = samples[i];
        }
        avg = sqrt(float(sum))/usedSamples;
        max = maxVal;
    }
    void printAvg() {
        std::clog << "avg: " << avg << " max: " << (max/327.68f)<< std::endl;
    }
};

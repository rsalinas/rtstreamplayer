class AudioBuffer {
public:
    AudioBuffer(size_t length) : buffer(new Uint8[length]), length(length)
    {
    }

    Uint8 *buffer;
    size_t length;
    float avg=0, max=0;
    sf_count_t usedSamples;
    void calculateAverage() {
        short * ib = reinterpret_cast<short*>(buffer);
        unsigned long long int sum=0;
        auto maxVal=*ib;
        for (size_t i=0; i < usedSamples; i++) {
            sum+=ib[i]*ib[i];
            if (ib[i] > maxVal)
                maxVal = ib[i];
        }
        avg = sqrt(float(sum))/usedSamples;
        max = maxVal;
    }
    void printAvg() {
        std::clog << "avg: " << avg << " max: " << (max/327.68f)<< std::endl;
    }
};

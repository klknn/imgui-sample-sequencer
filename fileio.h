#pragma once

#include <vector>

struct WavData {
    std::vector<float> samples; // interleaved floats (frames * channels) or mono if convertToMono==true
    int sampleRate = 0;
    int channels = 0;
    int bitsPerSample = 0;
};


bool LoadWavRIFF(const std::string& filename, WavData& out, bool convertToMono = true, std::string* err = nullptr);


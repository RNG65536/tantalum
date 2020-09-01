#pragma once

#include <functional>
#include <map>
#include <mutex>
#include <vector>

struct GasDischargeLines
{
    std::string        name;
    std::vector<float> wavelengths;
    std::vector<int>   strengths;
};

struct TantalumData
{
    TantalumData();

    void init_data();

    std::vector<GasDischargeLines> lines;
    std::vector<float>             wavelength_to_rgb;
    std::once_flag                 flag;
};
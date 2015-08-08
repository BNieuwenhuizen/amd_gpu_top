//  Copyright (c) 2015 Bas Nieuwenhuizen
//  
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//  
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//  
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#include <cstdint>
#include <cstdlib>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>

#include <pciaccess.h>
#include <unistd.h>


namespace
{

char const* bars[] = {" ", "▏", "▎", "▍", "▌", "▋", "▊", "▉", "█"};


pci_device* getPCIDevice()
{
    if(int error = pci_system_init()) {
        std::cerr << "could not initialize PCI\n";
        std::exit(1);
    }

    pci_device* device = nullptr;


    pci_device_iterator* iter;
    pci_id_match match;

    match.vendor_id = 0x1002; // AMD?
    match.device_id = 0x6939; // TONGA?
    match.subvendor_id = PCI_MATCH_ANY;
    match.subdevice_id = PCI_MATCH_ANY;

    match.device_class = 0x3 << 16;
    match.device_class_mask = 0xff << 16;

    match.match_data = 0;

    iter = pci_id_match_iterator_create(&match);
    device = pci_device_next(iter);
    pci_iterator_destroy(iter);
    if(!device) {
        std::cerr << "could not find an AMD TONGA GPU\n";
        std::exit(1);
    }

    if(int error = pci_device_probe(device)) {
        std::cerr << "could not probe GPU\n";
        std::exit(1);
    }


    if(device->vendor_id != 0x1002) {
        std::cerr << "Graphics card is not identified\n";
        std::exit(1);
    }

    return device;
}

std::uint32_t readReg(void* mmioAddress, std::uint32_t index)
{
    return reinterpret_cast<std::uint32_t volatile*>(mmioAddress)[index];
}


struct Counter {
    std::string name;
    std::uint32_t index;
    std::uint32_t mask;
    bool isIdle;
};
std::vector<Counter> counters{
    Counter{"CL", 0x2284U, (1u << 31), false},
    Counter{"SU", 0x2294U, (1u << 31), false},
    Counter{"GDS", 0x25c1U, (1u << 0), false},
    Counter{"IA", 0x2237U, (1u << 0), false},
    Counter{"WD", 0x223fU, (1u << 0), false},
    Counter{"VGT", 0x223cU, (1u << 0), false},
    Counter{"TD", 0x2526U, (1u << 31), false},
    Counter{"CP", 0x21a0U, (1u << 31), false},
    Counter{"SDMA0", 0x340dU, (1u << 0), true},
    Counter{"SDMA1", 0x360dU, (1u << 0), true},
};

void printCounter(std::ostream& os, std::string const& name, int percentage)
{
    os << std::setw(10) << name << " " << std::setw(3) << percentage << "% " << std::setw(0);
    percentage *= 2;

    int len = 1 + (percentage / 8);
    for(int i = 0; i < (percentage / 8); ++i)
        os << bars[8];
    os << bars[percentage % 8];
    for(int i = len; i < 32; ++i)
        os << " ";
}
}

int main()
{
    auto dev = getPCIDevice(); // yes, we leak this
    int sampleCount = 100;

    void* mmioAddress;
    
    // TODO: better selection of region & size
    if(pci_device_map_range(dev, dev->regions[5].base_addr, 0x40000, 0, &mmioAddress)) {
        std::cerr << "mmio mem map failed (try to run as root)\n";
        std::exit(1);
    }

    std::vector<int> counterIndices(counters.size());
    for(std::size_t i = 0; i < counterIndices.size(); ++i)
        counterIndices[i] = i;

    for(;;) {

        std::vector<int> counts(counters.size());

        auto startTime = std::chrono::steady_clock::now();
        auto interval = 1000000 / sampleCount;

        for(int i = 0; i < sampleCount; ++i) {
            for(std::size_t i = 0; i < counts.size(); ++i) {
                if(counters[i].isIdle) {
                    if(!(readReg(mmioAddress, counters[i].index) & counters[i].mask))
                        ++counts[i];
                } else {
                    if(readReg(mmioAddress, counters[i].index) & counters[i].mask)
                        ++counts[i];
                }
            }

            auto currentTime = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - startTime);
            usleep((i + 1) * interval - elapsed.count());
        }

        std::stable_sort(
            counterIndices.begin(), counterIndices.end(), [&counts](int a, int b) { return counts[a] > counts[b]; });

        std::cout << "\033[2J\033[1;1H" << std::flush; // clears screen

        for(std::size_t i = 0; i < counters.size(); ++i) {
            printCounter(std::cout, counters[counterIndices[i]].name, counts[counterIndices[i]] * 100 / sampleCount);
            std::cout << "\n";
        }
    }
}
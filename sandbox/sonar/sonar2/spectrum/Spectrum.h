/**
 * @file Spectrum.h
 *
 * @author Leo Singer
 * @author Copyright 2007 Robotics@Maryland. All rights reserved.
 * 
 */

#ifndef _RAM_SPECTRUM_H
#define _RAM_SPECTRUM_H

#include <complex>

namespace ram {
namespace sonar {

class Spectrum {
public:
    virtual ~Spectrum() {}
    virtual void purge() =0;
    virtual void update(const adcdata_t *) =0;
    virtual const std::complex<int64_t> &getAmplitude(int k, int channel) const;
    virtual const std::complex<int64_t> &operator() (int k, int channel) const 
    { return getAmplitude(k, channel); }
};


} // namespace sonar
} // namespace ram

#endif

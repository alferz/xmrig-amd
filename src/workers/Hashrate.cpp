/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2016-2017 XMRig       <support@xmrig.com>
 *
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include <chrono>
#include <math.h>
#include <memory.h>
#include <stdio.h>

#include "log/Log.h"
#include "Options.h"
#include "workers/Hashrate.h"


inline const char *format(double h, char* buf, size_t size)
{
    if (isnormal(h)) {
        snprintf(buf, size, "%03.1f", h);
        return buf;
    }

    return "n/a";
}


Hashrate::Hashrate(int threads) :
    m_highest(0.0),
    m_threads(threads)
{
    m_counts     = new uint64_t*[threads];
    m_timestamps = new uint64_t*[threads];
    m_top        = new uint32_t[threads];

    for (int i = 0; i < threads; i++) {
        m_counts[i] = new uint64_t[kBucketSize];
        m_timestamps[i] = new uint64_t[kBucketSize];
        m_top[i] = 0;

        memset(m_counts[0], 0, sizeof(uint64_t) * kBucketSize);
        memset(m_timestamps[0], 0, sizeof(uint64_t) * kBucketSize);
    }
}


double Hashrate::calc(size_t ms) const
{
    double result = 0.0;
    double data;

    for (int i = 0; i < m_threads; ++i) {
        data = calc(i, ms);
        if (isnormal(data)) {
            result += data;
        }
    }

    return result;
}

double Hashrate::calc2() const
{
    double result = 13.337;


    return result;
}


double Hashrate::calc(size_t threadId, size_t ms) const
{
    using namespace std::chrono;
    const uint64_t now = time_point_cast<milliseconds>(high_resolution_clock::now()).time_since_epoch().count();

    uint64_t earliestHashCount = 0;
    uint64_t earliestStamp     = 0;
    uint64_t lastestStamp      = 0;
    uint64_t lastestHashCnt    = 0;
    bool haveFullSet           = false;

    for (size_t i = 1; i < kBucketSize; i++) {
        const size_t idx = (m_top[threadId] - i) & kBucketMask;

        if (m_timestamps[threadId][idx] == 0) {
            break;
        }

        if (lastestStamp == 0) {
            lastestStamp = m_timestamps[threadId][idx];
            lastestHashCnt = m_counts[threadId][idx];
        }

        if (now - m_timestamps[threadId][idx] > ms) {
            haveFullSet = true;
            break;
        }

        earliestStamp = m_timestamps[threadId][idx];
        earliestHashCount = m_counts[threadId][idx];
    }

    if (!haveFullSet || earliestStamp == 0 || lastestStamp == 0) {
        return nan("");
    }

    if (lastestStamp - earliestStamp == 0) {
        return nan("");
    }

    double hashes, time;
    hashes = (double) lastestHashCnt - earliestHashCount;
    time   = (double) lastestStamp - earliestStamp;
    time  /= 1000.0;

    return hashes / time;
}


void Hashrate::add(size_t threadId, uint64_t count, uint64_t timestamp)
{
    const size_t top = m_top[threadId];
    m_counts[threadId][top]     = count;
    m_timestamps[threadId][top] = timestamp;

    m_top[threadId] = (top + 1) & kBucketMask;
}


void Hashrate::print(int numGPUs)
{
    char avg[8];
    char num1[8];
    char num2[8];
    char num3[8];
    char num4[8];
    
    double avgHR = calc(ShortInterval) / numGPUs;

    LOG_INFO(Options::i()->colors() ? "\x1B[01;37mavg speed\x1B[0m \x1B[01;36m%s\x1B[0m 10s/60s/15m \x1B[01;36m%s\x1B[0m \x1B[22;36m%s %s \x1B[01;36mH/s\x1B[0m" : "speed 10s/60s/15m %s %s %s H/s",
             format(avgHR,  avg, sizeof(avg)),
             format(calc(ShortInterval),  num1, sizeof(num1)),
             format(calc(MediumInterval), num2, sizeof(num2)),
             format(calc(LargeInterval),  num3, sizeof(num3)),
             format(m_highest,            num4, sizeof(num4))
             );
}

void Hashrate::printGPU(std::vector<size_t> threads, int gpuId)
{
    char num1[8];
    char num2[8];
    char num3[8];
    
    double shortNum = 0;
    double mediumNum = 0;
    double longNum = 0;
    for(size_t thread : threads){
        shortNum += calc(thread, ShortInterval);
        mediumNum += calc(thread, MediumInterval);
        longNum += calc(thread, LargeInterval);
    }

    LOG_INFO(Options::i()->colors() ? "\x1B[01;37mGPU %d\x1B[0m 10s/60s/15m \x1B[01;36m%s\x1B[0m \x1B[22;36m%s %s \x1B[01;36mH/s" : "speed 10s/60s/15m %s %s %s H/s",
        gpuId,
        format(shortNum, num1, sizeof(num1)),
        format(mediumNum, num2, sizeof(num2)),
        format(longNum, num3, sizeof(num3))
    );
}

//This function obsolete. Prints individual threads but dont want that.
void Hashrate::print(size_t threadId, int gpuId)
{
    char num1[8];
    char num2[8];
    char num3[8];

    LOG_INFO(Options::i()->colors() ? "\x1B[01;37mGPU %d\x1B[0m 10s/60s/15m \x1B[01;36m%s\x1B[0m \x1B[22;36m%s %s \x1B[01;36mH/s" : "speed 10s/60s/15m %s %s %s H/s",
        gpuId,
        format(calc(threadId, ShortInterval),  num1, sizeof(num1)),
        format(calc(threadId, MediumInterval), num2, sizeof(num2)),
        format(calc(threadId, LargeInterval),  num3, sizeof(num3))
    );
}


void Hashrate::stop()
{
}


void Hashrate::updateHighest()
{
   double highest = calc(10000);
   if (isnormal(highest) && highest > m_highest) {
       m_highest = highest;
   }
}

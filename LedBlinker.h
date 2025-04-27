/*
    Copyright (C) 2023  Csaba K Molnár

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*/

#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <mutex>
#include <shared_mutex>
#include <cmath>
#ifndef EASYLOGGINGPP_H
#include "easylogging++.h"
#endif

#ifndef CLASS_LEDBLINKER_H
#define CLASS_LEDBLINKER_H

// Forward declaration of class X52
class X52;

class LedBlinker
{
// VARIABLES
    public:
        struct LedSequence {
            std::string led;
            std::string sequence;   // the sequence
            int loopCount = 0;      // (e.g., 3 means print 3 times, 0 means infinite loop
            int speed;              // characters per second
            int lastPrintedTick = -1;   // number of characters printed so far
            std::chrono::steady_clock::time_point start_time;  // Time when this LED started blinking
            std::chrono::duration<double> tickDuration;  // Tracks the last tick index that was printed
        };
    private:
        X52* myx52;
        std::atomic<bool> finishThread;
        std::thread workerThreadVariable;
        std::vector<LedSequence> sequencesVector;
        std::shared_mutex sequencesVectorMutex;

// FUNCTIONS
    public:
        LedBlinker();
        ~LedBlinker();
        void set_x52(X52& instance);
        void setLedToSequence(LedBlinker::LedSequence& newData);
    private:
        /// <summary>
        /// We have already restricted blinking speed to 10 in the main program, equivalent to 10Hz.
        /// This is still much faster than the 1-2Hz recommended for vehicle dashboards
        /// by NASA: https://colorusage.arc.nasa.gov/flashing.php
        /// We can sleep the thread for 25ms which is fourth of the shortest time a character can
        /// take in the pattern. According to https://blog.bearcats.nl/accurate-sleep-function/,
        /// at this speed even std::this_thread::sleep_for is accurate enough. This means that
        /// a highly accurate C++ timer implementation is not required.
        /// </summary>
        /// <returns></returns>
        int workerThread();
};

#endif CLASS_LEDBLINKER_H
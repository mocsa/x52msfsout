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

#include "LedBlinker.h"
#include "x52.h"

LedBlinker::LedBlinker() : finishThread(false) {
    workerThreadVariable = std::thread(&LedBlinker::workerThread, this);
}

LedBlinker::~LedBlinker() {
    finishThread.store(true);
    if (workerThreadVariable.joinable()) {
        workerThreadVariable.join();
    }
}

void LedBlinker::set_x52(X52& instance) {
	myx52 = &instance;
}

void LedBlinker::setLedToSequence(LedBlinker::LedSequence& newData) {
    std::unique_lock lock(sequencesVectorMutex); // Exclusive lock for writing
    // Find the first (and only) matching LED
    auto it = std::find_if(sequencesVector.begin(), sequencesVector.end(),
    [&newData](const LedSequence& p) { return p.led == newData.led; });
    // We already have this LED in the Vector
    if (it != sequencesVector.end()) {
        // Is this the same sequence?
        if (newData.sequence == it->sequence)
        {
            // Do nothing. Keep blinking the looped sequence
            // or, in case of non-looped sequence, do not restart it.
        }
        else if (newData.sequence.empty())
        {
            // The new sequence is empty, which means the user
            // wants this LED to stop blinking.
            sequencesVector.erase(it);
        }
        else
        {
            // We already have this LED but a new
            // sequence was requested. Replace old
            // sequence with new and reset start_time.
            it->sequence = newData.sequence;
            it->start_time = std::chrono::steady_clock::now();
        }
    }
    else
    {
        // We don't have this LED and we were not asked to stop blinking it.
        // Add this LED.
        if (!newData.sequence.empty())
        {
            newData.start_time = std::chrono::steady_clock::now();
            sequencesVector.push_back(newData);
            std::cout << "New sequence for " << newData.led << " is '" << newData.sequence << "'. Speed " << newData.speed << ". LastPrintedTick " << newData.lastPrintedTick << ".\n";
        }
    }
}

int LedBlinker::workerThread() {
    std::string b_light;


    // Main loop
    while (!finishThread.load()) {
        if (!sequencesVector.empty()) // Do we need to work?
        {
            auto currentTime = std::chrono::steady_clock::now();

            {
                std::shared_lock lock(sequencesVectorMutex); // Shared read lock
                /*
                // Check if every non-infinite loop leds have finished their sequence.
                for (const auto& s : sequencesVector) {
                    if (s.loopCount == 0 || s.lastPrintedTick < s.loopCount * static_cast<int>(s.sequence.size()) - 1) {
                        allFinished = false;
                        break;
                    }
                }
                if (allFinished)
                    break;  // Exit if all finite-loop leds are done */

                    // Process each led individually
                for (auto& s : sequencesVector) {
            
                    std::chrono::duration<double> elapsed = currentTime - s.start_time;

                    // Calculate the total number of "ticks" that should have occurred by now.
                    // This is equivalent to: floor(elapsed time / (1/speed)) which equals floor(elapsed time * speed)
                    auto currentTick = static_cast<int>(std::floor(elapsed / s.tickDuration));

                    int loopedTimes = currentTick / static_cast<int>(s.sequence.size());
                    // Determine the current character index:
                    int index = currentTick % s.sequence.size();

                    // If loopCount is finite and reached, stop blinking
                    if (s.loopCount != 0 && loopedTimes >= s.loopCount)
                        continue;

                    if (s.sequence[index] == ' ') b_light = "off";
                    else if (s.sequence[index] == 'a') b_light = "amber";
                    else if (s.sequence[index] == 'g') b_light = "green";
                    else if (s.sequence[index] == 'r') b_light = "red";
                    else if (s.sequence[index] == 'o') b_light = "on";
                    if (myx52 != nullptr) {
                        myx52->write_led(s.led, b_light);
                    }
                    s.lastPrintedTick = currentTick; // Update last printed tick
                }
            }
            // Sleep a short time to avoid busy waiting while still keeping timing accuracy.
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        else
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    return 0;
}
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

#include "x52HID.h"

x52HID::~x52HID()
{
    CloseHandle(hidCreateFileHandle);
}

int x52HID::initialize()
{
    // Get Number Of Devices
    UINT nDevices = 0;
    GetRawInputDeviceList(nullptr, &nDevices, sizeof(RAWINPUTDEVICELIST));

    // Got Any?
    if (nDevices < 1)
    {
        // Exit
        CLOG(ERROR,"toconsole", "tofile") << "0 RAWINPUT Devices found.";
        return 0;
    }

    // Allocate Memory For Device List
    std::vector<RAWINPUTDEVICELIST> pRawInputDeviceList(nDevices);

    // Fill Device List Buffer
    int nResult = GetRawInputDeviceList(pRawInputDeviceList.data(), &nDevices, sizeof(RAWINPUTDEVICELIST));

    // Got Device List?
    if (nResult < 0)
    {
        // Error
        CLOG(ERROR,"toconsole", "tofile") << "Could not get RAWINPUT device list.";
        return 0;
    }

    // Loop Through Device List
    for (UINT i = 0; i < nDevices; i++)
    {
        // Get Character Count For Device Name
        UINT nBufferSize = 0;
        nResult = GetRawInputDeviceInfo(pRawInputDeviceList[i].hDevice, // Device
            RIDI_DEVICENAME,                // Get Device Name
            nullptr,                        // NO Buff, Want Count!
            &nBufferSize);                  // Char Count Here!

        // Got Device Name?
        if (nResult < 0)
        {
            // Error
            CLOG(ERROR,"toconsole", "tofile") << "Unable to get HID Device Name character count.. Moving to next device.";

            // Next
            continue;
        }

        // Allocate Memory For Device Name
        std::string deviceName(nBufferSize, '\0');

        // Get Name
        nResult = GetRawInputDeviceInfo(pRawInputDeviceList[i].hDevice, // Device
            RIDI_DEVICENAME,                // Get Device Name
            &deviceName[0],                   // Get Name!
            &nBufferSize);                  // Char Count

        // Got Device Name?
        if (nResult < 0)
        {
            // Error
            CLOG(ERROR,"toconsole", "tofile") << "Unable to get HID Device Name.. Moving to next device.";

            // Next
            continue;
        }

        // Set Device Info & Buffer Size
        RID_DEVICE_INFO rdiDeviceInfo{};
        rdiDeviceInfo.cbSize = sizeof(RID_DEVICE_INFO);
        nBufferSize = rdiDeviceInfo.cbSize;

        // Get Device Info
        nResult = GetRawInputDeviceInfo(pRawInputDeviceList[i].hDevice,
            RIDI_DEVICEINFO,
            &rdiDeviceInfo,
            &nBufferSize);

        // Got All Buffer?
        if (nResult < 0)
        {
            // Error
            CLOG(ERROR,"toconsole", "tofile") << "Unable to read HID Device Info.. Moving to next device.";

            // Next
            continue;
        }

        if (deviceName.find(X52PRO_VID_PID) != std::string::npos) {
            hidPath = deviceName; // Store the HID path
            hidHandle = pRawInputDeviceList[i].hDevice; // Store the HID Handle
            CLOG(INFO,"toconsole", "tofile") << "hidHandle = " << hidHandle;

            hidCreateFileHandle = CreateFile(
                hidPath.c_str(),
                GENERIC_WRITE | GENERIC_READ,
                FILE_SHARE_WRITE | FILE_SHARE_READ,
                nullptr,
                OPEN_EXISTING,
                0,
                nullptr);

            return 1;
        }
    }
    return 0;
}

std::string x52HID::getHIDPath() const {
    return hidPath;
}

HANDLE x52HID::getHIDHandle() {
    return hidHandle;
}

void x52HID::setMFDCharDelay(long delay)
{
    mfddelayms = delay;
}

void x52HID::setBrightness(std::string_view target, unsigned char brightnessValue)
{
    std::array<unsigned char, 4> hidOutData{}; // {} initializes all elements to zero
    DWORD hidOutBytesReturned;

    if (brightnessValue > 128)
        brightnessValue = 128;
    if (target == "mfd")
    {
        hidOutData[0] = 0xB1;
    }
    else // led
    {
        hidOutData[0] = 0xB2;
    }
    hidOutData[1] = 0;
    hidOutData[2] = 0;
    hidOutData[3] = brightnessValue;

    std::lock_guard lock(deviceIoControlMutex);  // Lock to prevent simultaneous calls from different threads
    BOOL result = DeviceIoControl(
        hidCreateFileHandle,
        0x223008, // dwIoControlCode, Probably a proprietary Logitech constant
        hidOutData.data(),
        static_cast<DWORD>(hidOutData.size()),
        nullptr,
        0,
        &hidOutBytesReturned,
        nullptr);  // lpOverlapped, NULL means synchronous (blocking) I/O

    if (!result) {
        CLOG(ERROR,"toconsole", "tofile") << "Cannot send Brightness value to joystick.";
    }
}

bool x52HID::setLedColor(const std::string& targetLed, const std::string& color)
{
    std::array<unsigned char, 4> hidOutData{}; // {} initializes all elements to zero
    DWORD hidOutBytesReturned;
    bool success = true;
    std::map<std::string, std::vector<unsigned char>, std::less<>> LED_IDS {
        { "fire"     , { 1} }, // Fire button illumination on/off (color is controlled by the position of the safety cover)
        { "a"        , { 2, 3} }, // red component, green component
        { "b"        , { 4, 5} },
        { "d"        , { 6, 7} },
        { "e"        , { 8, 9} },
        { "t1"       , {10,11} },
        { "t2"       , {12,13} },
        { "t3"       , {14,15} },
        { "pov"      , {16,17} },
        { "clutch"   , {18,19} },
        { "throttle" , {20} } // Throttle axis illumination on/off (color is controlled by the throttle position)
    };

    std::map<std::string, std::vector<unsigned char>, std::less<>> LED_STATES{
        { "on"    , {1} },
        { "off"   , {0, 0} },
        { "red"   , {1, 0} },
        { "green" , {0, 1} },
        { "amber" , {1, 1} }
    };

    int i = 0;
    std::lock_guard lock(deviceIoControlMutex);  // Lock to prevent simultaneous calls from different threads
    for (unsigned char ledID : LED_IDS[targetLed])
    {
        try
        {
            hidOutData[0] = 0xB8;
            hidOutData[1] = 0;
            hidOutData[2] = ledID;
            hidOutData[3] = LED_STATES[color].at(i);

            if ( ! DeviceIoControl(
                hidCreateFileHandle,
                0x223008, // dwIoControlCode, Probably a proprietary Logitech constant
                hidOutData.data(),
                static_cast<DWORD>(hidOutData.size()),
                nullptr,
                0,
                &hidOutBytesReturned,
                nullptr)  // lpOverlapped, NULL means synchronous (blocking) I/O
            ) {
                CLOG(ERROR,"toconsole", "tofile") << "Cannot send Led value to joystick.";
                success = false;
            }

            i++;
        }
        catch (const std::out_of_range& e)
        {
            CLOG(WARNING, "toconsole", "tofile") << "Reading from LED_STATES[] is out of range. Probably tried to set the fire or throttle led to a color. Ignored it. Error message: " << e.what() << ".";
            return success;
        }
    }
    return success;
}

void x52HID::setShift(std::string_view shiftState)
{
    DWORD hidOutBytesReturned;
    std::array<unsigned char, 4> hidOutData{}; // {} initializes all elements to zero
    hidOutData[0] = 0xFD;
    hidOutData[1] = 0;
    hidOutData[2] = 0;
    hidOutData[3] = shiftState == "on" ? 0x51 : 0x50;

    std::lock_guard lock(deviceIoControlMutex);  // Lock to prevent simultaneous calls from different threads
    BOOL result = DeviceIoControl(
        hidCreateFileHandle,
        0x223008, // dwIoControlCode, Probably a proprietary Logitech constant
        hidOutData.data(),
        static_cast<DWORD>(hidOutData.size()),
        nullptr,
        0,
        &hidOutBytesReturned,
        nullptr);  // lpOverlapped, NULL means synchronous (blocking) I/O

    if (!result) {
        CLOG(ERROR,"toconsole", "tofile") << "Cannot send Shift state to joystick.";
    }

}

void x52HID::clearMFDTextLine(int line)
{
    std::array<unsigned char, 4> hidOutData{}; // {} initializes all elements to zero
    DWORD hidOutBytesReturned;
    constexpr std::array<unsigned char, 3> CLEAR_ADDRESS = { 0xd9, 0xda, 0xdc };

    // Ensure the line index is within bounds
    if (line < 0 || line >= CLEAR_ADDRESS.size()) {
        return;  // Handle invalid index gracefully
    }

    // Clear the line
    hidOutData[0] = CLEAR_ADDRESS[line];
    hidOutData[1] = 0;
    hidOutData[2] = 0;
    hidOutData[3] = 0;

    std::lock_guard lock(deviceIoControlMutex);  // Lock to prevent simultaneous calls from different threads
    DeviceIoControl(
        hidCreateFileHandle,
        0x223008, // dwIoControlCode, Probably a proprietary Logitech constant
        hidOutData.data(),
        static_cast<DWORD>(hidOutData.size()),
        nullptr,
        0,
        &hidOutBytesReturned,
        nullptr);  // lpOverlapped, NULL means synchronous (blocking) I/O
}

void x52HID::setMFDTextLine(int line, std::string text)
{
    std::array<unsigned char, 4> hidOutData{}; // {} initializes all elements to zero
    DWORD hidOutBytesReturned;
    constexpr std::array<unsigned char, 3> WRITE_ADDRESS = { 0xd1, 0xd2, 0xd4 };

    clearMFDTextLine(line);

    std::lock_guard lock(deviceIoControlMutex);  // Lock to prevent simultaneous calls from different threads
    for ( size_t i = 0; i < text.length(); i+=2 )
    {
        // Do we still have at least 2 characters in the text?
        if (i + 1 < text.length()) {
            hidOutData[2] = text[i+1]; // The later character should go in the earlier byte
            hidOutData[3] = text[i];
        }
        else
        {
            // If only 1 character remained in the text, fill the last character with space
            hidOutData[2] = 0x20;
            hidOutData[3] = text[i];
        }
        // Write text character-by-character
        hidOutData[0] = WRITE_ADDRESS[line];
        hidOutData[1] = 0;

        DeviceIoControl(
            hidCreateFileHandle,
            0x223008, // dwIoControlCode, Probably a proprietary Logitech constant
            &hidOutData,
            sizeof(hidOutData),
            nullptr,
            0,
            &hidOutBytesReturned,
            nullptr);  // lpOverlapped, NULL means synchronous (blocking) I/O

        Sleep(mfddelayms); // Delay given ms. Defaults to 0.
    }
}

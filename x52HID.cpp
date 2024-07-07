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

x52HID::x52HID()
{
}

x52HID::~x52HID()
{
    CloseHandle(hidCreateFileHandle);
}

int x52HID::initialize()
{
    // Get Number Of Devices
    UINT nDevices = 0;
    GetRawInputDeviceList(NULL, &nDevices, sizeof(RAWINPUTDEVICELIST));

    // Got Any?
    if (nDevices < 1)
    {
        // Exit
        std::cout << "ERROR: 0 RAWINPUT Devices found." << std::endl;
        return 0;
    }

    // Allocate Memory For Device List
    PRAWINPUTDEVICELIST pRawInputDeviceList;
    pRawInputDeviceList = new RAWINPUTDEVICELIST[sizeof(RAWINPUTDEVICELIST) * nDevices];

    // Got Memory?
    if (pRawInputDeviceList == NULL)
    {
        // Error
        std::cout << "ERROR: Could not allocate memory for RAWINPUT Device List." << std::endl;
        return 0;
    }

    // Fill Device List Buffer
    int nResult;
    nResult = GetRawInputDeviceList(pRawInputDeviceList, &nDevices, sizeof(RAWINPUTDEVICELIST));

    // Got Device List?
    if (nResult < 0)
    {
        // Clean Up
        delete[] pRawInputDeviceList;

        // Error
        std::cout << "ERROR: Could not get RAWINPUT device list." << std::endl;
        return 0;
    }

    // Loop Through Device List
    for (UINT i = 0; i < nDevices; i++)
    {
        // Get Character Count For Device Name
        UINT nBufferSize = 0;
        nResult = GetRawInputDeviceInfo(pRawInputDeviceList[i].hDevice, // Device
            RIDI_DEVICENAME,                // Get Device Name
            NULL,                           // NO Buff, Want Count!
            &nBufferSize);                 // Char Count Here!

        // Got Device Name?
        if (nResult < 0)
        {
            // Error
            std::cout << "ERROR: Unable to get HID Device Name character count.. Moving to next device." << std::endl;

            // Next
            continue;
        }

        // Allocate Memory For Device Name
        CHAR *wcDeviceName = new CHAR[nBufferSize + 1];

        // Got Memory
        if (wcDeviceName == NULL)
        {
            // Error
            std::cout << "ERROR: Unable to allocate memory for HID Device Name.. Moving to next device." << std::endl;

            // Next
            continue;
        }

        // Get Name
        nResult = GetRawInputDeviceInfo(pRawInputDeviceList[i].hDevice, // Device
            RIDI_DEVICENAME,                // Get Device Name
            wcDeviceName,                   // Get Name!
            &nBufferSize);                 // Char Count

                                           // Got Device Name?
        if (nResult < 0)
        {
            // Error
            std::cout << "ERROR: Unable to get HID Device Name.. Moving to next device." << std::endl;

            // Clean Up
            delete[] wcDeviceName;

            // Next
            continue;
        }

        // Set Device Info & Buffer Size
        RID_DEVICE_INFO rdiDeviceInfo;
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
            std::cout << "ERROR: Unable to read HID Device Info.. Moving to next device." << std::endl;

            // Next
            continue;
        }

        if (strstr(wcDeviceName, X52PRO_VID_PID)) {
            hidPath = wcDeviceName; // Store the HID path
            hidHandle = pRawInputDeviceList[i].hDevice; // Store the HID Handle

            hidCreateFileHandle = CreateFile(
                hidPath,
                GENERIC_WRITE | GENERIC_READ,
                FILE_SHARE_WRITE | FILE_SHARE_READ,
                NULL,
                OPEN_EXISTING,
                0,
                NULL);

            return 1;
        }
    }
    return 0;
}

CHAR* x52HID::getHIDPath() {
    return hidPath;
}

void x52HID::setMFDCharDelay(long delay)
{
    mfddelayms = delay;
}

void x52HID::setBrightness(std::string target, unsigned char brightnessValue)
{
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

    BOOL result = DeviceIoControl(
        hidCreateFileHandle,
        0x223008, // dwIoControlCode, Probably a proprietary Logitech constant
        &hidOutData,
        sizeof(hidOutData),
        NULL,
        0,
        &hidOutBytesReturned,
        NULL);

    if (!result) {
        std::cout << "Error Sending Brightness value to joystick." << std::endl;
    }
}

bool x52HID::setLedColor(std::string targetLed, std::string color)
{
    bool success = true;
    std::map<std::string, std::vector<int>> LED_IDS {
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

    std::map<std::string, std::vector<int>> LED_STATES{
        { "on"    , {1} },
        { "off"   , {0, 0} },
        { "red"   , {1, 0} },
        { "green" , {0, 1} },
        { "amber" , {1, 1} }
    };

    int i = 0;
    for (int ledID : LED_IDS[targetLed])
    {
        try
        {
            hidOutData[0] = 0xB8;
            hidOutData[1] = 0;
            hidOutData[2] = ledID;
            hidOutData[3] = LED_STATES[color].at(i);

            BOOL result = DeviceIoControl(
                hidCreateFileHandle,
                0x223008, // dwIoControlCode, Probably a proprietary Logitech constant
                &hidOutData,
                sizeof(hidOutData),
                NULL,
                0,
                &hidOutBytesReturned,
                NULL);

            if (!result) {
                std::cout << "Error Sending Led value to joystick." << std::endl;
                success = false;
            }

            i++;
        }
        catch (const std::out_of_range&)
        {
            // Somebody tried to set the fire or throttle led to a color. Ignore it.
            return success;
        }
    }
}

void x52HID::setShift(std::string shiftState)
{
    hidOutData[0] = 0xFD;
    hidOutData[1] = 0;
    hidOutData[2] = 0;
    hidOutData[3] = shiftState == "on" ? 0x51 : 0x50;

    BOOL result = DeviceIoControl(
        hidCreateFileHandle,
        0x223008, // dwIoControlCode, Probably a proprietary Logitech constant
        &hidOutData,
        sizeof(hidOutData),
        NULL,
        0,
        &hidOutBytesReturned,
        NULL);

    if (!result) {
        std::cout << "Error Sending Shift state to joystick." << std::endl;
    }

}

void x52HID::clearMFDTextLine(int line)
{
    unsigned char CLEAR_ADDRESS[3] = { 0xd9, 0xda, 0xdc };

    // Clear the line
    hidOutData[0] = CLEAR_ADDRESS[line];
    hidOutData[1] = 0;
    hidOutData[2] = 0;
    hidOutData[3] = 0;

    BOOL result = DeviceIoControl(
        hidCreateFileHandle,
        0x223008, // dwIoControlCode, Probably a proprietary Logitech constant
        &hidOutData,
        sizeof(hidOutData),
        NULL,
        0,
        &hidOutBytesReturned,
        NULL);
}

void x52HID::setMFDTextLine(int line, std::string text)
{
    unsigned char WRITE_ADDRESS[3] = { 0xd1, 0xd2, 0xd4 };

    clearMFDTextLine(line);

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

        BOOL result = DeviceIoControl(
            hidCreateFileHandle,
            0x223008, // dwIoControlCode, Probably a proprietary Logitech constant
            &hidOutData,
            sizeof(hidOutData),
            NULL,
            0,
            &hidOutBytesReturned,
            NULL);

        Sleep(mfddelayms); // Delay given ms. Defaults to 0.
    }
}

#pragma once

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <Windows.h>

/// <summary>The x52HID class allows two-way communication with a Saitek / Logitech X52Pro joystick using the factory drivers</summary>
class x52HID
{
public:
    enum Brightness
    {
        X52_BRIGHTNESS_MFD,
        X52_BRIGHTNESS_LED
    };
public:
	x52HID();
	~x52HID();
    /// <summary>
    /// Tries to find the HID path for the first connected X52 Pro. Further X52 Pros are ignored.
    /// </summary>
    /// <returns>int 1 means that a HID path was found</returns>
    int initialize();
    CHAR* getHIDPath();
    void setMFDCharDelay(long delay);
    /// <summary>
    /// Set the MFD or LED brightness.
    /// </summary>
    /// <param name="target">Contains the string mfd or something else for led.</param>
    /// <param name="brightnessValue">A value between 0 and 128</param>
    void setBrightness(std::string target, unsigned char value);
    /// <summary>
    /// DO NOT CALL DIRECTLY! ALWAYS CALL write_led() IN X52 CLASS!
    /// Sets a led to a given color.
    /// </summary>
    /// <param name="targetLed">The name of one of the 11 leds, for example, "t1". See the source for all names.</param>
    /// <param name="color">The string "off", "red", "green", "amber" for lights with 2 physical leds. "on" and "off" for lights with 1 physical led, that is fire and throttle, whose color is controlled by the joystick.</param>
    /// <returns></returns>
    bool setLedColor(std::string targetLed, std::string color);
    /// <summary>
    /// Turns on or off the SHIFT indicator on the MFD
    /// </summary>
    /// <param name="shiftState">The string "on" or "off"</param>
    void setShift(std::string shiftState);
    /// <summary>
    /// Clear the given line on the MFD.
    /// </summary>
    /// <param name="line">MFD line to clear. 0 = top, 1 = middle, 2 = bottom.</param>
    void clearMFDTextLine(int line);
    /// <summary>
    /// Overwrite the given line on the MFD with the supplied text.
    /// </summary>
    /// <param name="line">MFD line to write to. 0 = top, 1 = middle, 2 = bottom.</param>
    /// <param name="text">Max. 16 characters in ASCII encoding, but only up to 125 (0x7D), only English alphabet. X52 supports accented characters but with a non-standard encoding. The encoding should be reverse engineered in the future.</param>
    void setMFDTextLine(int line, std::string text);

private:
    /// <summary>
    /// HID path of X52 Pro. Looks like \\?\HID#VID_06A3&PID_0762#8&2c8f587f&1&0000#{4d1e55b2-f16f-11cf-88cb-001111000030}
    /// </summary>
    CHAR* hidPath;
    HANDLE hidHandle;
    HANDLE hidCreateFileHandle;
    unsigned char hidOutData[4];
    DWORD hidOutBytesReturned;
    const char X52PRO_VID_PID[18] = "VID_06A3&PID_0762";
    /// <summary>
    /// Delay in ms after sending each character to MFD. Defaults to 0ms.
    /// </summary>
    long mfddelayms;
};
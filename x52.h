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

#include <boost/property_tree/ptree.hpp>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <windows.h>
#define WSMCMND_API_STATIC
#include <client/WASimClient.h>

#include "SimConnect.h"

#include "x52HID.h"
#include "LedBlinker.h"

#ifndef CLASS_X52_H
#define CLASS_X52_H

// Forward declaration of class LedBlinker
class LedBlinker;

class X52
{
// VARIABLES
public:
	int MAX_MFD_LEN; // Max num of chars written to one MFD row
	std::string MFD_ON_JOY[3]; // The currently displayed 3 lines of text on the MFD
	/// <summary>
	/// Contains the name of the current active shift state as a string.
	/// </summary>
	std::string CUR_SHIFT_STATE;
	bool mfd_on, led_on;
	bool joybuttonstates[39];
	struct LastSentPacket {
		DWORD pdwSendID;
		std::string message;
	};
	LastSentPacket lastsentpacket;
	enum EVENT_ID {
		EVENT_JOYBUTTON_PRESS = 200,
		EVENT_JOYBUTTON_RELEASE = 300,
		EVENT_CLIENTID = 10000, // First Client Event ID to send a single command/InputEvent
	};
	int lastClientEventId = EVENT_CLIENTID - 1;
	struct DataForIndicators {
		std::string dataref;
		std::string unit;
		uint8_t simvarindex;
		DOUBLE value;
	};
protected:
	std::ofstream m_log_file;
	struct SingleDataref {
		double  dataref[1];
	};
	HANDLE  hSimConnect;
	WASimCommander::Client::WASimClient* wasimclient;
	x52HID* x52hid;
	LedBlinker* ledBlinker;
	boost::property_tree::ptree* xml_file;
	std::map<int, X52::DataForIndicators>* dataForIndicatorsMap;
	std::map<std::string, std::string> CURRENT_LED_COLOR;

// FUNCTIONS
public:
	X52();
	~X52();
	void logg(std::string status, std::string func, std::string msg);
	void set_simconnect_handle(HANDLE handle);
	void set_wasimconnect_instance(WASimCommander::Client::WASimClient& client);
	void set_x52HID(x52HID&);
	void set_LedBlinker(LedBlinker&);
	void set_xmlfile(boost::property_tree::ptree* xml_file);
	void setDataForIndicatorsMap( std::map<int, X52::DataForIndicators>& );
	/// <summary>
    /// Validate that all sequence tags contain valid data.
    /// </summary>
	bool validateSequences();
	void write_to_mfd(std::string line1, std::string line2, std::string line3);
	/// <summary>
    /// Maintain the led's current color in the CURRENT_LED_COLOR map. Sets a led to a given color, if it is not already that color, according to CURRENT_LED_COLOR.
    /// </summary>
    /// <param name="led">The name of one of the 11 leds, for example, "t1". See the source for all names.</param>
    /// <param name="light">The string "off", "red", "green", "amber" for lights with 2 physical leds. "on" and "off" for lights with 1 physical led, that is fire and throttle, whose color is controlled by the joystick.</param>
    /// <returns></returns>
	void write_led(std::string led, std::string light);
	/// <summary>
	/// Sets a led to a color or, if a sequence is used, sends it to the blinker thread. This function is not called recursively.
	/// </summary>
	/// <param name="led">The name of one of the 11 leds, for example, "t1".</param>
	/// <param name="light">The name of a color, or "on" or "off", or a sequence.</param>
	/// <param name="current_light">The name of the led's current color.</param>
	/// <param name="state">Ptree of a state tag</param>
	/// <param name="force">True to update the led's color even if it already has that color.</param>
	void update_led(std::string led, std::string light, std::string current_light, boost::property_tree::ptree &state, bool force);
	/// <summary>
	/// Evaluates if the given simvarvalue matches the given operand.
	/// </summary>
	/// <param name="simvarvalue">A SimVar's value as a double.</param>
	/// <param name="op">Four characters. The first two can be == for equality, -- for less than, and ++ for greater than. The last two are a two-digit number.</param>
	/// <returns></returns>
	bool evaluate_xml_op(double simvarvalue, std::string op);
	void execute_button_press(boost::property_tree::ptree &xmltree, int btn);
	void execute_button_release(boost::property_tree::ptree &xmltree, int btn);
	/// <summary>This method is called recursively to process elements under the assignments tag.</summary>
	/// <param name="xmltree">A Property Tree of EITHER all button tags under the assignments tag OR a button tag OR a shifted_button tag under a button tag.</param>
	/// <param name="status">Contains the string pressed or released.</param>
	/// https://learn.microsoft.com/en-us/cpp/build/reference/summary-visual-cpp?view=msvc-170
	bool assignment_button_action(boost::property_tree::ptree &xmltree, int btn, std::string status);
	/// <summary>
	/// A recursively called function. Checks if data has changed in MSFS and updates joystick leds.
	/// Each led's current color is stored in the led tag's current_light attribute created during runtime. It stores the value of the light
	/// attribute from the active state tag.
	/// </summary>
	/// <param name="tagname">Led or state. Initially an empty string.</param>
	/// <param name="xmltree">Initially, immediate children (led tags) of the indicators tag. On recursive calls, a state tag below a led tag or below another state tag.</param>
	/// <param name="led">std::string The led's name taken from the id attribute of the led tag</param>
	/// <param name="current_light">Used during recursive calls: current color of the currently processed led.</param>
	/// <param name="force">Passed on to the update_led function. Force the update of the led's color even if it already has that color.</param>
	/// <returns>The light attribute of the first state tag which is true when all state tags are evaluated from inside out.</returns>
	std::string dataref_ind_action(std::string_view tagname, boost::property_tree::ptree &xmltree, std::string const & led, std::string const & current_light, bool force = false);
	/// <summary>
	/// Called by the WASimServer when a requested SimVar has changed in MSFS.
	/// It stores the incoming value of the SimVar in dataForIndicatorsMap to be used later.
	/// </summary>
	void IndicatorDataCallback(const WASimCommander::Client::DataRequestRecord&);
	/// <summary>
	/// Handles switching a target on or off. For led on, it starts to operate leds according to XML configuration. For led off, it does nothing. For mfd on, it ???. For mfd off, it only clears the MFD text.
	/// </summary>
	/// <param name="id">The target's id. Can be "led" or "mfd".</param>
	/// <param name="on">True of false.</param>
	void all_on(std::string id, bool on);
	/// <summary>
	/// A recursively called function which, based on joybuttonstates[], finds out what is the currently active shift state.
	/// If a shift state was found, its name is stored in CUR_SHIFT_STATE and the joystick's SHIFT indicator is switched on.
	/// </summary>
	/// <param name="xmltree">Initially, the whole XML configuration file. On recursive calls, only the nested shift_state tag.</param>
	/// <returns>True if an active shift state was found. On recursive calls, true if the button in the nested shift_state tag is pressed.</returns>
	bool shift_state_active(const boost::property_tree::ptree xmltree);
	/// <summary>
	/// Initiates the calling of the recursive shift_state_active function and handles the case when no active shift state was found.
	/// </summary>
	/// <param name="xml_file">Pointer to the whole XML configuration file.</param>
	void shift_state_action(const boost::property_tree::ptree xml_file);
	bool construct_successful();
};

#endif
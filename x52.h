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
#include <windows.h>

class X52
{
public:                 // begin the public section
	X52();
	~X52();
	void logg(std::string status, std::string func, std::string msg);
	void set_simconnect_handle(HANDLE handle);
	void heartbeat();
	void heartbeatReset();
	void write_to_mfd(std::string line1, std::string line2, std::string line3);
	void write_shift(std::string on);
	void update_brightness(std::string id, std::string brightness);
	void execute_button_press(boost::property_tree::ptree &xmltree, int btn);
	void execute_button_release(boost::property_tree::ptree &xmltree, int btn);
	bool assignment_button_action(boost::property_tree::ptree &xmltree, int btn, std::string status);
	void all_on(std::string id, bool on);
	bool shift_state_active(const boost::property_tree::ptree xmltree);
	void shift_state_action(const boost::property_tree::ptree xml_file);
	void start_command_handler();
	bool construct_successful();
	std::ofstream m_cmd_file;
	int MAX_MFD_LEN; // Max num of chars written to one MFD row
	std::string MFD_ON_JOY[3]; // The currently displayed 3 lines of text on the MFD
	double HEARTBEAT_TIME = 0; // Last time of communication with command handler
	double X52_RUN_TIME; // Absolute time in the simulator. Variable name taken from x52luaout.
	std::string CUR_SHIFT_STATE;
	bool mfd_on, led_on;
	bool joybuttonstates[32];
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
protected:              // begin the protected section
	std::ofstream m_log_file;
	struct SingleDataref {
		double  dataref[1];
	};
	HANDLE  hSimConnect;
};

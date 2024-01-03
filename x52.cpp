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

#include "x52.h"
#include <fstream>
#include <iomanip>
#include <source_location>

#include "SimConnect.h"

void X52::logg(std::string status, std::string func, std::string msg) {
	std::time_t now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	m_log_file
		<< std::put_time(std::localtime(&now_c), "%Y-%m-%d %H.%M.%S")
		<< " [" << status << "]"
		<< " (" << func << ")"
		<< " " << msg
		<< std::endl;
}

void X52::set_simconnect_handle(HANDLE handle) {
	hSimConnect = handle;
}

void X52::heartbeat() {
	if (X52_RUN_TIME - HEARTBEAT_TIME > 60) {
		// Restart command handler as this script has been inactive for over a minute
		start_command_handler();
		logg("INFO", "x52.cpp:" + std::to_string(__LINE__), "Command handler will be restarted");
	}
	if (X52_RUN_TIME - HEARTBEAT_TIME > 30 ) {
		// Send a heartbeat to the command handler so it doesn't stop
		m_cmd_file << "heartbeat" << std::endl;
		HEARTBEAT_TIME = X52_RUN_TIME;
	}
}

void X52::heartbeatReset() {
    HEARTBEAT_TIME = X52_RUN_TIME;
}

void X52::write_to_mfd(std::string line1, std::string line2, std::string line3) {
	if (MFD_ON_JOY[0] != line1) {
		m_cmd_file << "mfd 1 \"" << line1 << "\"" << std::endl; // endl causes flush
		heartbeatReset();
	}
	if (MFD_ON_JOY[1] != line2) {
		m_cmd_file << "mfd 2 \"" << line2 << "\"" << std::endl; // endl causes flush
		heartbeatReset();
	}
	if (MFD_ON_JOY[2] != line3) {
		m_cmd_file << "mfd 3 \"" << line3 << "\"" << std::endl; // endl causes flush
		heartbeatReset();
	}
	MFD_ON_JOY[0] = line1;
	MFD_ON_JOY[1] = line2;
	MFD_ON_JOY[2] = line3;
}

void X52::write_shift(std::string on) {
    m_cmd_file << "shift " << on << std::endl;
    heartbeatReset();
}

void X52::update_brightness(std::string id, std::string brightness) {
    m_cmd_file << "light " << id << " " << brightness << std::endl; // endl causes flush
    heartbeatReset();
}

void X52::execute_button_press(boost::property_tree::ptree &xmltree, int btn) {
	std::string type = xmltree.get<std::string>("<xmlattr>.type","");
	if (type == "trigger_pos" ||
		type == "hold" ||
		type == "") {
		if ( xmltree.get<std::string>("<xmlattr>.pressed", "") != "true") {
			if (xmltree.get<std::string>("<xmlattr>.custom_command", "") != "") {

			} else if (xmltree.get<std::string>("<xmlattr>.command", "") != "") {
				int clienteventid =	xmltree.get<int>("<xmlattr>.clienteventid", 0);
				if (clienteventid == 0)
				{
					xmltree.put<int>("<xmlattr>.clienteventid",++lastClientEventId);
					clienteventid = lastClientEventId;
					SimConnect_MapClientEventToSimEvent(hSimConnect, clienteventid, xmltree.get<std::string>("<xmlattr>.command").c_str());
				}
				SimConnect_TransmitClientEvent(hSimConnect,
					SIMCONNECT_OBJECT_ID_USER, // Invoke InputEvent on the user's aircraft
					clienteventid, // Event_ID
					xmltree.get<DWORD>("<xmlattr>.on", 0), // dwData - Optional data for the InputEvent
					SIMCONNECT_GROUP_PRIORITY_HIGHEST, // GroupID - special case, we're not using a group, but a priority and we specify the "GroupID is a Priority" flag below
					SIMCONNECT_EVENT_FLAG_GROUPID_IS_PRIORITY
					);
				SimConnect_GetLastSentPacketID(hSimConnect, &lastsentpacket.pdwSendID);
				lastsentpacket.message = "TransmitClientEvent: EventName=" + xmltree.get<std::string>("<xmlattr>.command") + " Data=" + std::to_string(xmltree.get<double>("<xmlattr>.on", 0));
			} else if (xmltree.get<std::string>("<xmlattr>.dataref", "") != "") {
				std::string attr = xmltree.get<std::string>("<xmlattr>.dataref");
				size_t separatorpos = attr.find("%");
				std::string dataref = attr.substr(0, separatorpos);
				std::string unit = attr.substr(separatorpos + 1);
				SimConnect_ClearDataDefinition(hSimConnect, 10);
				SimConnect_AddToDataDefinition(hSimConnect, 10, dataref.c_str(), unit.c_str(), SIMCONNECT_DATATYPE_FLOAT64);
				SingleDataref datarefstruct;
				datarefstruct.dataref[0] = xmltree.get<double>("<xmlattr>.on");
				SimConnect_SetDataOnSimObject(hSimConnect,
					10, // Definition ID
					SIMCONNECT_OBJECT_ID_USER, // Set data on the user's aircraft
					0, // Flags
					0, // ArrayCount: Number of elements in the data array. A count of zero is interpreted as one element.
					sizeof(datarefstruct), // size of each element in the data array in bytes
					&datarefstruct );
			}
			xmltree.put("<xmlattr>.pressed","true");
		}
	}
}

void X52::execute_button_release(boost::property_tree::ptree& xmltree, int btn) {
	xmltree.put("<xmlattr>.press_time","");
	xmltree.put("<xmlattr>.pressed","");
	xmltree.put("<xmlattr>.in_repeat","");
}

/// <summary>This method is called recursively to process elements under the assignments tag.</summary>
/// <param name="xmltree">A Property Tree of 1. all button tags under the assignments tag OR 2. a button tag OR 3. a shifted_button tag under a button tag.</param>
/// <param name="status">Contains the string pressed or released.</param>
/// https://learn.microsoft.com/en-us/cpp/build/reference/summary-visual-cpp?view=msvc-170
bool X52::assignment_button_action(boost::property_tree::ptree &xmltree, int btn, std::string status) {
	try
	{
		for (boost::property_tree::ptree::value_type &v : xmltree)
		{
			if (v.first == "button") // button tag (first level child of assignments)
			{
				if (status == "released" && btn == v.second.get<int>("<xmlattr>.nr")) {
					if (!assignment_button_action(v.second, btn, "released")) { // If there is no shifted_button tag under this button tag then do the release process for this button tag.
						execute_button_release(v.second, btn);
						v.second.put("<xmlattr>.press_type","");
						return true;
					}
				}
				else if(status == "pressed" && btn == v.second.get<int>("<xmlattr>.nr")) {
					if (v.second.get<std::string>("<xmlattr>.press_type","") == "normal" ||
						!assignment_button_action(v.second, btn, "pressed")) { // If there is no shifted_button tag under this button tag then execute the command in this button tag.
						execute_button_press(v.second, btn);
						v.second.put("<xmlattr>.press_type","normal");
						return true;
					}
				}
			}
			else if (v.first == "shifted_button") // shifted_button tag (child of button tag)
			{
				if (status == "pressed" && (CUR_SHIFT_STATE == v.second.get<std::string>("<xmlattr>.shift_state","") || v.second.get<std::string>("<xmlattr>.press_type","") == "shift")) {
					execute_button_press(v.second, btn);
					v.second.put("<xmlattr>.press_type","shift");
					return true;
				}
				else if(status == "released" && v.second.get<std::string>("<xmlattr>.press_type") == "shift") {
					execute_button_release(v.second, btn);
					v.second.put("<xmlattr>.press_type","");
					return true;
				}
			}
		}
		return false;
	}
	catch (const boost::property_tree::ptree_error&)
	{
		return false;
	}
}

void X52::all_on(std::string id, bool on) {
	if (on) { // On!
		if (id == "led") {
			// X52.dataref_ind_action(X52.IND, nil, nil, true) --Force update for all defined leds
		}
		else
		{
			// X52.dataref_mfd_action(X52.ACTIVE_PAGE, true)   --Force update for active page
		}
	}
	else
	{ // Off!
		if (id == "led") {
			// do nothing
		}
		else
		{
			std::string empty(MAX_MFD_LEN, ' ');
			write_to_mfd(empty, empty, empty);
		}
	}
}

bool X52::shift_state_active(const boost::property_tree::ptree xmltree) {
	for (const boost::property_tree::ptree::value_type &v : xmltree) { // Read all children of shift_states tag
		if (v.first == "shift_state") // only process shift_state tags
		{
			if (joybuttonstates[std::stoi(v.second.get<std::string>("<xmlattr>.button"))-1]) {
				if (!shift_state_active(v.second)) {
					// nested shift button is not pressed, so this state is the current active
					if (CUR_SHIFT_STATE != v.second.get<std::string>("<xmlattr>.name")) { // we have a new current shiftstate
						CUR_SHIFT_STATE = v.second.get<std::string>("<xmlattr>.name");
						logg("INFO", "x52.cpp:" + std::to_string(__LINE__), "New shift state: " + CUR_SHIFT_STATE);
						if(mfd_on) {
							// X52.activate_page(X52.ACTIVE_PAGE)
						}
						write_shift("on");
					}
				}
				return true;
			}
		}
	}
	return false;
}

void X52::shift_state_action(const boost::property_tree::ptree xml_file) {
	try
	{
		if (!shift_state_active(xml_file.get_child("shift_states")) && !CUR_SHIFT_STATE.empty() )
		{
			write_shift("off");
			CUR_SHIFT_STATE.clear();
			logg("INFO", "x52.cpp:" + std::to_string(__LINE__), "Shift state was cleared.");
			if (mfd_on) {
				// X52.activate_page(X52.ACTIVE_PAGE)
			}
		}
	}
	catch (const boost::property_tree::ptree_error&)
	{

	}
}

void X52::start_command_handler() {
	// Command handler file
	m_cmd_file.open("cmds");
	// Command handling script
	logg("INFO", "x52.cpp:" + std::to_string(__LINE__), "Command handler started with: start \"X52LuaOut Command handler\" /min /low luajit.exe cmd_handler.lua cmds");
	std::system("start \"X52LuaOut Command handler\" /min /low luajit.exe cmd_handler.lua cmds");
}

bool X52::construct_successful() {
    if (m_cmd_file.is_open() &&
		m_log_file.is_open()) {
        return true;
    }
    return false;
}

X52::X52() {
	// Open log file
	m_log_file.open("x52msfsout_log.txt");
	start_command_handler();
// Detect OS: https://stackoverflow.com/questions/5919996/
#if defined(__linux__) || defined(__APPLE__)
    MAX_MFD_LEN = 16;
#elif _WIN32
    MAX_MFD_LEN = 15;
#endif

}

X52::~X52() {
    m_cmd_file.close();
	m_log_file.close();
}
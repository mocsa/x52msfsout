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

void X52::set_simconnect_handle(HANDLE handle) {
	hSimConnect = handle;
}

void X52::set_wasimconnect_instance(WASimCommander::Client::WASimClient & client) {
	wasimclient = &client;
}

void X52::set_x52HID(x52HID& instance) {
	x52hid = &instance;
}

void X52::set_LedBlinker(LedBlinker& instance) {
	ledBlinker = &instance;
}

void X52::set_xmlfile(boost::property_tree::ptree* file) {
	xml_file = file;
}

void X52::setDataForIndicatorsMap(std::map<int, X52::DataForIndicators>& map) {
	dataForIndicatorsMap = &map;
}

bool X52::validateSequences() {
	if (xml_file->count("sequences") == 0)
	{
		// Nothing to validate
		return true;
	}
	for (boost::property_tree::ptree::value_type& v : xml_file->get_child("sequences"))
	{
		if (v.first == "sequence")
		{
			if (v.second.get<double>("<xmlattr>.speed") > 10)
			{
				CLOG(ERROR,"toconsole", "tofile") << "The speed of sequence tag " << v.second.get<std::string>("<xmlattr>.name") << " is greater than 10. The maximum allowed speed is 10.";
				return false;
			}
		}
		else
		{
			CLOG(ERROR,"toconsole", "tofile") << "The sequences tag contains a <" << v.first << "> tag. It can only contain sequence tags.";
			return false;
		}
	}
	return true;
}

void X52::write_to_mfd(std::string& line1, std::string& line2, std::string& line3) {
	if (MFD_ON_JOY[0] != line1) {
		x52hid->setMFDTextLine(0, line1);
	}
	if (MFD_ON_JOY[1] != line2) {
		x52hid->setMFDTextLine(1, line2);
	}
	if (MFD_ON_JOY[2] != line3) {
		x52hid->setMFDTextLine(2, line3);
	}
	MFD_ON_JOY[0] = line1;
	MFD_ON_JOY[1] = line2;
	MFD_ON_JOY[2] = line3;
}

void X52::write_led(const std::string& led, const std::string& color) {
	if (CURRENT_LED_COLOR.empty())
	{
		// Initially all leds are off
		CURRENT_LED_COLOR = {
		{ "fire", "" },
		{ "a", "" },
		{ "b", "" },
		{ "d", "" },
		{ "e", "" },
		{ "t1", "" },
		{ "t2", "" },
		{ "t3", "" },
		{ "pov", "" },
		{ "clutch", "" },
		{ "throttle", "" }
		};
	}
	if (CURRENT_LED_COLOR[led] != color) {
		if (x52hid->setLedColor(led, color))
		{
			CURRENT_LED_COLOR[led] = color;
			CLOG(TRACE,"toconsole", "tofile") << "Color of LED \"" << led << "\" was successfully set to \"" << color << "\".";
		}
}
}

void X52::update_led(std::string led, std::string light, std::string current_light, boost::property_tree::ptree &state, bool force) {
	// Are there sequences defined in the XML file?
	if (xml_file->count("sequences") != 0) {
		for (auto& [first, second] : xml_file->get_child("sequences"))
		{
			if (first == "sequence" && light == second.get<std::string>("<xmlattr>.name"))
			{
				// Found sequence tag with the name in the light variable
				if (!state.empty())
				{
					LedBlinker::LedSequence ledSequence;
					ledSequence.led = led;
					ledSequence.sequence = second.get<std::string>("<xmlattr>.pattern");
					ledSequence.loopCount = second.get<int>("<xmlattr>.loop", 0);
					ledSequence.speed = second.get<int>("<xmlattr>.speed", 0);
					// Determine the led's tick duration (time per character)
					ledSequence.tickDuration = std::chrono::duration<double> (1.0 / ledSequence.speed);
					ledBlinker->setLedToSequence(ledSequence);
				}
				return; // If matching sequence was found, no need to loop further
			}
		}
	}
	// This led will not be blinking, so make sure it
	// stops blinking if it was blinking before.
	LedBlinker::LedSequence ledSequence;
	ledSequence.led = led;
	ledSequence.sequence = ""; // Empty sequence means stop blinking
	ledBlinker->setLedToSequence(ledSequence);
	if (light != current_light || force) write_led(led, light);
}

bool X52::evaluate_xml_op(double simvarvalue, std::string op) {
	std::string oper, value;

	oper = op.substr(0, 2);
	value = op.substr(2);
	
	if (oper == "==") {
		return(std::stod(value) == simvarvalue);
	} else if(oper == "--") {
		return(simvarvalue < std::stod(value));
	} else if (oper == "++") {
		return(simvarvalue > std::stod(value));
	} else {
		return false;
	}
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
			} else if (xmltree.get<std::string>("<xmlattr>.calculator_code", "") != "") {
				std::string calc_code = xmltree.get<std::string>("<xmlattr>.calculator_code");
				double fResult = 0.;
				std::string sResult {};
				if (wasimclient->executeCalculatorCode(calc_code, WASimCommander::Enums::CalcResultType::Double, &fResult, &sResult) == S_OK) {
					CLOG(DEBUG,"toconsole", "tofile") << "Calculator code \"" << calc_code << "\" numerical result = " << fResult << ", string result = \"" << sResult << "\".";
				}
				else {
					CLOG(DEBUG,"toconsole", "tofile") << "Calculator code \"" << calc_code << "\" could not be executed. Numerical result = " << fResult << ", string result = \"" << sResult << "\".";
				}
			}
			xmltree.put("<xmlattr>.pressed","true");
		}
	}
}

void X52::execute_button_release(boost::property_tree::ptree& xmltree, int btn) const {
	xmltree.put("<xmlattr>.press_time","");
	xmltree.put("<xmlattr>.pressed","");
	xmltree.put("<xmlattr>.in_repeat","");
}

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
	catch (const boost::property_tree::ptree_error& e)
	{
		CLOG(ERROR, "toconsole", "tofile") << "Boost ptree had problems with XML processing. Button = " << btn << ", status = " << status << ". Error message : " << e.what() << ".";
		return false;
	}
}


std::string X52::dataref_ind_action(std::string_view tagname, boost::property_tree::ptree &xmltree, std::string const & led = "", std::string const & current_light = "", bool force) {
	std::string r;
	boost::property_tree::ptree empty_ptree;
	try
	{
		for (boost::property_tree::ptree::value_type& v : xmltree)
		{
			if (v.first == "led") // a led tag which contains multiple state tags
			{
				if (v.second.count("state") != 0) // This led has a state declared
				{
					r = dataref_ind_action(v.first, v.second, v.second.get<std::string>("<xmlattr>.id"), v.second.get<std::string>("<xmlattr>.current_light",""), force);
					if (r == "") { // No state evaluates to true, set led to off
						update_led(v.second.get<std::string>("<xmlattr>.id"), "off", v.second.get<std::string>("<xmlattr>.current_light",""), v.second, force);
						v.second.put("<xmlattr>.current_light", "off");
					} else {
						v.second.put("<xmlattr>.current_light", r);
					}
				} else {
					// No declared states for led, set to off
					if (v.second.get("<xmlattr>.current_light", "") != "off") {
						update_led(v.second.get<std::string>("<xmlattr>.id"), "off", "", empty_ptree, force);
						v.second.put("<xmlattr>.current_light", "off");
					}
				}
			} else if (v.first == "state") { // a state tag below a led tag or below another state tag
				r = dataref_ind_action(v.first, v.second, led, current_light, force);
				if (r != "") { // Higher prio state was set, so reset this state's sequence timing and abort
					return r;
				}
			}
		}
		// No subitems to loop into, we're at bottom (innermost state)
		if (tagname == "state") {
			if (evaluate_xml_op((*dataForIndicatorsMap).at(xmltree.get<int>("<xmlattr>.requestid")).value, xmltree.get<std::string>("<xmlattr>.op"))) {
				update_led(led, xmltree.get<std::string>("<xmlattr>.light"), current_light, xmltree, force);
				return xmltree.get<std::string>("<xmlattr>.light");
			} else {
				// Dataref doesn't evaluate
				return "";
			}
		}
	}
	catch (const boost::property_tree::ptree_error&)
	{
		return "";
	}
	return "";
}

void X52::IndicatorDataCallback(const WASimCommander::Client::DataRequestRecord &dr)
{
	double dval;
	dr.tryConvert(dval);
	dataForIndicatorsMap->at(dr.requestId).value = dval;
	CLOG(DEBUG,"toconsole", "tofile") << "MSFS says " << dr.nameOrCode << " is now " << dval << " (in unit " << dr.unitName << ").";
	dataref_ind_action("", xml_file->get_child("indicators"), "", "", false);
	return;
}

void X52::all_on(std::string id, bool on) {
	if (on) { // On!
		if (id == "led") {
			dataref_ind_action("", xml_file->get_child("indicators"), "", "", true); // Force update for all defined leds
		}
		else
		{
			// X52.dataref_mfd_action(X52.ACTIVE_PAGE, true)   // Force update for active page
		}
	}
	else
	{ // Off!
		if (id == "led") {
			write_led("fire"    , "off");
			write_led("a"       , "off");
			write_led("b"       , "off");
			write_led("d"       , "off");
			write_led("e"       , "off");
			write_led("t1"      , "off");
			write_led("t2"      , "off");
			write_led("t3"      , "off");
			write_led("pov"     , "off");
			write_led("clutch"  , "off");
			write_led("throttle", "off");
		}
		else
		{
			x52hid->clearMFDTextLine(0);
			x52hid->clearMFDTextLine(1);
			x52hid->clearMFDTextLine(2);
		}
	}
}

bool X52::shift_state_active(const boost::property_tree::ptree xmltree) {
	for (const boost::property_tree::ptree::value_type &v : xmltree) { // Read all children of shift_states tag
		if (v.first == "shift_state") // only process shift_state tags
		{
			// If the button in this shift_state tag is pressed, check the nested shift_state tag.
			if (joybuttonstates[std::stoi(v.second.get<std::string>("<xmlattr>.button"))-1]) {
				if (!shift_state_active(v.second)) {
					// nested shift button is not pressed, so this state is the current active
					// Is this newly found state different from the current one?
					if (CUR_SHIFT_STATE != v.second.get<std::string>("<xmlattr>.name")) {
						CUR_SHIFT_STATE = v.second.get<std::string>("<xmlattr>.name");
						CLOG(DEBUG,"toconsole", "tofile") << "New shift state: " + CUR_SHIFT_STATE;
						if(mfd_on) {
							// X52.activate_page(X52.ACTIVE_PAGE)
						}
						x52hid->setShift("on");
					}
				}
				return true;
			}
		}
	}
	return false;
}

void X52::shift_state_action(const boost::property_tree::ptree& xmltree) {
	try
	{
		if (!shift_state_active(xmltree.get_child("shift_states")) && !CUR_SHIFT_STATE.empty() )
		{
			x52hid->setShift("off");
			CUR_SHIFT_STATE.clear();
			CLOG(DEBUG,"toconsole", "tofile") << "Shift state was cleared.";
			if (mfd_on) {
				// X52.activate_page(X52.ACTIVE_PAGE)
			}
		}
	}
	catch (const boost::property_tree::ptree_error& e)
	{
		CLOG(ERROR, "toconsole", "tofile") << "Boost ptree had problems with processing shift_states tags. Error message: " << e.what() << ".";
		return;
	}
}

X52::X52() {
// Detect OS: https://stackoverflow.com/questions/5919996/
#if defined(__linux__) || defined(__APPLE__)
    MAX_MFD_LEN = 16;
#elif _WIN32
    MAX_MFD_LEN = 15;
#endif

}

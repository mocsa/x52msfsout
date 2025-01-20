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

#include <mutex>
#include <deque>
#include <thread>
#include <condition_variable>
#include <string>

#include <iostream>
#include <fstream>
#include <x52.h>
#include <cstdlib>

// https://fmt.dev/latest/index.html
#define FMT_HEADER_ONLY
#include <fmt/core.h>

#include <windows.h>
#include "SimConnect.h"

#define WSMCMND_API_STATIC
#include <client/WASimClient.h>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/xml_parser.hpp>
#include <boost/program_options.hpp>

std::string quit ("n");
std::string constcharn ("n");
HANDLE  hSimConnect = NULL;
X52 myx52;
x52HID x52hid;
boost::property_tree::ptree xml_file; // Create empty property tree object
WASimCommander::Client::WASimClient* wasimclient;

const char* ExceptionList[] = {
    "SIMCONNECT_EXCEPTION_NONE",
    "SIMCONNECT_EXCEPTION_ERROR",
    "SIMCONNECT_EXCEPTION_SIZE_MISMATCH",
    "SIMCONNECT_EXCEPTION_UNRECOGNIZED_ID",
    "SIMCONNECT_EXCEPTION_UNOPENED",
    "SIMCONNECT_EXCEPTION_VERSION_MISMATCH",
    "SIMCONNECT_EXCEPTION_TOO_MANY_GROUPS",
    "SIMCONNECT_EXCEPTION_NAME_UNRECOGNIZED",
    "SIMCONNECT_EXCEPTION_TOO_MANY_EVENT_NAMES",
    "SIMCONNECT_EXCEPTION_EVENT_ID_DUPLICATE",
    "SIMCONNECT_EXCEPTION_TOO_MANY_MAPS",
    "SIMCONNECT_EXCEPTION_TOO_MANY_OBJECTS",
    "SIMCONNECT_EXCEPTION_TOO_MANY_REQUESTS",
    "SIMCONNECT_EXCEPTION_WEATHER_INVALID_PORT",
    "SIMCONNECT_EXCEPTION_WEATHER_INVALID_METAR",
    "SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_GET_OBSERVATION",
    "SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_CREATE_STATION",
    "SIMCONNECT_EXCEPTION_WEATHER_UNABLE_TO_REMOVE_STATION",
    "SIMCONNECT_EXCEPTION_INVALID_DATA_TYPE",
    "SIMCONNECT_EXCEPTION_INVALID_DATA_SIZE",
    "SIMCONNECT_EXCEPTION_DATA_ERROR",
    "SIMCONNECT_EXCEPTION_INVALID_ARRAY",
    "SIMCONNECT_EXCEPTION_CREATE_OBJECT_FAILED",
    "SIMCONNECT_EXCEPTION_LOAD_FLIGHTPLAN_FAILED",
    "SIMCONNECT_EXCEPTION_OPERATION_INVALID_FOR_OBJECT_TYPE",
    "SIMCONNECT_EXCEPTION_ILLEGAL_OPERATION",
    "SIMCONNECT_EXCEPTION_ALREADY_SUBSCRIBED",
    "SIMCONNECT_EXCEPTION_INVALID_ENUM",
    "SIMCONNECT_EXCEPTION_DEFINITION_ERROR",
    "SIMCONNECT_EXCEPTION_DUPLICATE_ID",
    "SIMCONNECT_EXCEPTION_DATUM_ID",
    "SIMCONNECT_EXCEPTION_OUT_OF_BOUNDS",
    "SIMCONNECT_EXCEPTION_ALREADY_CREATED",
    "SIMCONNECT_EXCEPTION_OBJECT_OUTSIDE_REALITY_BUBBLE",
    "SIMCONNECT_EXCEPTION_OBJECT_CONTAINER",
    "SIMCONNECT_EXCEPTION_OBJECT_AI",
    "SIMCONNECT_EXCEPTION_OBJECT_ATC",
    "SIMCONNECT_EXCEPTION_OBJECT_SCHEDULE",
    "SIMCONNECT_EXCEPTION_JETWAY_DATA",
    "SIMCONNECT_EXCEPTION_ACTION_NOT_FOUND",
    "SIMCONNECT_EXCEPTION_NOT_AN_ACTION",
    "SIMCONNECT_EXCEPTION_INCORRECT_ACTION_PARAMS",
    "SIMCONNECT_EXCEPTION_GET_INPUT_EVENT_FAILED",
    "SIMCONNECT_EXCEPTION_SET_INPUT_EVENT_FAILED",
};

struct StructData {
	double  dataarray[100];
};

enum INPUT_ID {
	INPUT_JOYBUTTONS,
};

enum GROUP_ID {
	GROUP_JOYBUTTONS,
};

enum DATA_DEFINE_ID {
	DEF_MASTER,
	DEF_ABSTIME,
	DEF_SINGLE_DATAREF = 10, // Definition used in X52 class to modify a single dataref/simvar
};

enum DATA_REQUEST_ID {
	REQ_MASTER,
	REQ_ABSTIME,
};

void CALLBACK MyDispatchProcRD(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext)
{
	switch (pData->dwID)
	{
	case SIMCONNECT_RECV_ID_EVENT:
	{
		SIMCONNECT_RECV_EVENT* evt = (SIMCONNECT_RECV_EVENT*)pData;
		// We made sure that our joy button press ClientEvents are numbered
		// from myx52.EVENT_JOYBUTTON_PRESS to maximum myx52.EVENT_JOYBUTTON_PRESS + 100.
		if (evt->uEventID >= myx52.EVENT_JOYBUTTON_PRESS &&
			evt->uEventID < myx52.EVENT_JOYBUTTON_PRESS + 100)
		{
			// Store in our array that a button is currently pressed.
			myx52.joybuttonstates[evt->uEventID - myx52.EVENT_JOYBUTTON_PRESS] = true;
			myx52.logg("INFO", "x52.cpp:" + std::to_string(__LINE__), "Joy Button " + std::to_string(evt->uEventID - myx52.EVENT_JOYBUTTON_PRESS + 1) + " was pressed.");
			
			// Carry out actions declared in the assignments tag for button press
			myx52.assignment_button_action(xml_file.get_child("assignments"), evt->uEventID - myx52.EVENT_JOYBUTTON_PRESS + 1, "pressed");
		}
		// We made sure that our joy button release ClientEvents are numbered
		// from myx52.EVENT_JOYBUTTON_RELEASE to maximum myx52.EVENT_JOYBUTTON_RELEASE + 100.
		if (evt->uEventID >= myx52.EVENT_JOYBUTTON_RELEASE &&
			evt->uEventID < myx52.EVENT_JOYBUTTON_RELEASE + 100)
		{
			// Update our array that a button has been released.
			myx52.joybuttonstates[evt->uEventID - myx52.EVENT_JOYBUTTON_RELEASE] = false;
			myx52.logg("INFO", "x52.cpp:" + std::to_string(__LINE__), "Joy Button " + std::to_string(evt->uEventID - myx52.EVENT_JOYBUTTON_RELEASE + 1) + " was released.");
			// Carry out actions declared in the assignments tag for button release
			myx52.assignment_button_action(xml_file.get_child("assignments"), evt->uEventID - myx52.EVENT_JOYBUTTON_RELEASE + 1, "released");
		}
		break;
	}
	case SIMCONNECT_RECV_ID_SIMOBJECT_DATA:
	{
		SIMCONNECT_RECV_SIMOBJECT_DATA* pObjData = (SIMCONNECT_RECV_SIMOBJECT_DATA*)pData;

		switch (pObjData->dwRequestID)
		{
		case REQ_MASTER:
		{
			StructData* pS = (StructData*)&pObjData->dwData;
			if(pObjData->dwDefineCount > 2)
			{
				// Below code is taken from Lua function X52.master_action(tbl)
				// Differences from the original Lua function: MSFS will only send us a message when the
				// SimVars change, therefore we don't use the needs_update variable and don't store the
				// last value of the SimVars in the XML.
				// Also, the second if-else block was merged into the first bigger if-else block.
				int targetnumber = 0;

				for (boost::property_tree::ptree::value_type &v : xml_file.get_child("master")) { // Read all children of master tag
					if (v.first == "target") // only process target tags
					{
						myx52.logg("INFO", "x52.cpp:" + std::to_string(__LINE__), fmt::format( "Master tag: Simvars changed. Target id={} switch simvar={:.2f} brightness simvar={:.2f}", v.second.get<std::string>("<xmlattr>.id").c_str(), pS->dataarray[targetnumber], pS->dataarray[targetnumber + 1]));
						// Check if SimVar value equals operator in XML
						if (myx52.evaluate_xml_op(pS->dataarray[targetnumber], v.second.get<std::string>("<xmlattr>.op"))) {
							if (v.second.get<std::string>("<xmlattr>.on", "") != "true") {
								// If this target is not already on, switch it on.
								myx52.all_on(v.second.get<std::string>("<xmlattr>.id"), true);
								// Store in the XML that this target is currently on
								v.second.put<std::string>("<xmlattr>.on", "true");
								// Store the target's status in the x52 class.
								if (v.second.get<std::string>("<xmlattr>.id") == "mfd") myx52.mfd_on = true;
								if (v.second.get<std::string>("<xmlattr>.id") == "led") myx52.led_on = true;
							}

							myx52.logg("INFO", "x52.cpp:" + std::to_string(__LINE__), fmt::format( "Master tag: Target id={}, setting brightness to {:.2f}", v.second.get<std::string>("<xmlattr>.id").c_str(), (pS->dataarray[targetnumber+1] - std::stod(v.second.get<std::string>("<xmlattr>.min"))) / (std::stod(v.second.get<std::string>("<xmlattr>.max")) - std::stod(v.second.get<std::string>("<xmlattr>.min"))) * 128 * std::stod(v.second.get<std::string>("<xmlattr>.default")) / 100));
							x52hid.setBrightness(v.second.get<std::string>("<xmlattr>.id"), (pS->dataarray[targetnumber+1] - std::stod(v.second.get<std::string>("<xmlattr>.min"))) / (std::stod(v.second.get<std::string>("<xmlattr>.max")) - std::stod(v.second.get<std::string>("<xmlattr>.min"))) * 128 * std::stod(v.second.get<std::string>("<xmlattr>.default")) / 100 );
						}
						else
						{
							if (v.second.get<std::string>("<xmlattr>.on", "") != "false") {
								// If this target is not already off, switch it off.
								myx52.all_on(v.second.get<std::string>("<xmlattr>.id"), false);
								// Set this target's brightness to 0.
								myx52.logg("INFO", "x52.cpp:" + std::to_string(__LINE__), fmt::format( "Master tag: Target id={}, setting brightness to zero.", v.second.get<std::string>("<xmlattr>.id").c_str()));
								x52hid.setBrightness( v.second.get<std::string>("<xmlattr>.id"), 0x0);
								// Store in the XML that this target is currently off
								v.second.put<std::string>("<xmlattr>.on", "false");
								// Store the target's status in the x52 class.
								if (v.second.get<std::string>("<xmlattr>.id") == "mfd") myx52.mfd_on = false;
							if (v.second.get<std::string>("<xmlattr>.id") == "led") myx52.led_on = false;
							}
						}
						targetnumber = targetnumber + 2;
					}
				}
			}
			break;
		}
		case REQ_ABSTIME:
		{
			StructData* pS = (StructData*)&pObjData->dwData;
			myx52.X52_RUN_TIME = pS->dataarray[0];
			break;
		}

		default:
			break;
		}
		break;
	}
	case SIMCONNECT_RECV_ID_EXCEPTION:
	{
		SIMCONNECT_RECV_EXCEPTION* pObjData = (SIMCONNECT_RECV_EXCEPTION*)pData;

		std::cerr << "Exception: " << ExceptionList[pObjData->dwException] << std::endl;
		if (pObjData->dwSendID == myx52.lastsentpacket.pdwSendID)
		{
			std::cerr << myx52.lastsentpacket.message << std::endl;
		}
		else {
			std::cerr << "Packet ID doesn't match. Details not found." << std::endl;
		}
		break;
	}
	default:
		printf("MyDispatchProcRD Received unhandled SIMCONNECT_RECV ID:%d\n", pData->dwID);
		break;
	}
}

void handleTerminate() {
    std::exception_ptr eptr = std::current_exception();
    try {
        if (eptr) {
            std::rethrow_exception(eptr);
        }
    } catch (const std::exception& e) {
        std::cerr << "Unhandled exception: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "Unhandled unknown exception." << std::endl;
    }
    std::abort();
}


int main(int argc, char *argv[])
{
	std::condition_variable cv;
	std::mutex mutex;
	std::deque<std::string> lines; // protected by m
	std::string xmlconfig;
	int joystick;
	long mfddelayms = 0;

	HRESULT hr;

	std::set_terminate(handleTerminate);

	// BEGIN Check command-line options
	try
	{
		boost::program_options::options_description desc("Allowed options");
		desc.add_options()
			("help", "Display help message")
			("xmlconfig", boost::program_options::value<std::string>(&xmlconfig)->required(), "XML configuration file")
			("joystick", boost::program_options::value<int>(&joystick)->required(), "Joystick number in MSFS")
			("mfddelayms", boost::program_options::value<long>(&mfddelayms)->default_value(0), "Delay in ms after sending each character-pair to MFD. Defaults to 0ms.");
		boost::program_options::variables_map vm;
		boost::program_options::store(boost::program_options::parse_command_line(argc, argv, desc), vm);
		if (vm.count("help")) {
			std::cout << desc << "\n";
			exit(EXIT_SUCCESS);
		}
		boost::program_options::notify(vm);
	}
	catch (const std::exception& e)
	{
		std::cerr << "Error with options: " << e.what() << "\n";
		exit(EXIT_FAILURE);
    }
    catch(...)
    {
        std::cerr << "Unknown error during option processing!" << "\n";
        exit(EXIT_FAILURE);
    }
	// END Check command-line options

	// Example code: https://learn.microsoft.com/en-us/windows/win32/services/svccontrol-cpp
	// https://stackoverflow.com/questions/7808085/
	SC_HANDLE hSCManager = NULL;
	hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if(hSCManager == NULL)
    {
        std::cerr << "Cannot open Windows Service manager." << "\n";
        exit(EXIT_FAILURE);
    }
	SC_HANDLE hService = OpenService(hSCManager, "SaiDOutput", SERVICE_STOP | SERVICE_START | SERVICE_QUERY_STATUS);
    if (!hService) {
        std::cerr << "Cannot open Logitech DirectOutput service. " << "\n";

		LPSTR messageBuffer = nullptr;

        // Use FormatMessage to convert the error code into a readable message
        size_t size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

        // Copy the message into a std::string
        std::string message(messageBuffer, size);

        // Free the buffer allocated by FormatMessage
        LocalFree(messageBuffer);
		std::cerr << message;

        exit(EXIT_FAILURE);
    }
	SERVICE_STATUS_PROCESS ssp;
    DWORD cbBytesNeeded;
    if(!QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE) &ssp, sizeof ssp, &cbBytesNeeded))
	{ 
		printf("QueryServiceStatusEx failed (%d)\n", GetLastError());

				LPSTR messageBuffer = nullptr;

        // Use FormatMessage to convert the error code into a readable message
        size_t size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

        // Copy the message into a std::string
        std::string message(messageBuffer, size);

        // Free the buffer allocated by FormatMessage
        LocalFree(messageBuffer);
		std::cerr << message;
        //CloseServiceHandle(schService); 
        //CloseServiceHandle(schSCManager);
        exit(EXIT_FAILURE);
	}
	BOOL bRet;
    if (ssp.dwCurrentState != SERVICE_STOPPED) {
		std::cout << "Logitech DirectOutput service is running. Trying to stop. (Current status " << ssp.dwCurrentState << ")\n";
        bRet = ControlService(hService, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS) &ssp);
        //assert(bRet);
		int stopTries = 0;
        while (ssp.dwCurrentState != SERVICE_STOPPED) {
            Sleep(ssp.dwWaitHint);
            QueryServiceStatusEx(hService, SC_STATUS_PROCESS_INFO, (LPBYTE) &ssp, sizeof ssp, &cbBytesNeeded);
			if (stopTries++ == 4)
			{
				std::cout << "Logitech DirectOutput service took too long to stop.\n";
				exit(EXIT_FAILURE);
			}
        }
    }

	// Thread to read from stdin
	// Code was taken from https://gist.github.com/vmrob/ff20420a20c59b5a98a1
	// Since waiting for the user to press the q key in the
	// main thread would block our
	// infinite MSFS processing loop, we need a separate
	// thread to capture the q key 
	// to stop the inifinite loop and quit the program.
	std::thread io{ [&] {
		std::string tmp;
		while (true) {
			std::getline(std::cin, tmp);
			std::lock_guard<std::mutex> lock{ mutex };
			lines.push_back(std::move(tmp));
			cv.notify_one();
		}
	} };

	std::cout << "Log is being written to x52msfsout_log.txt.\n";
	std::cout << "Press q+Enter to quit!\n";

	// Parse the XML into the property tree.
	try
	{
		boost::property_tree::read_xml(xmlconfig, xml_file, boost::property_tree::xml_parser::no_comments + boost::property_tree::xml_parser::trim_whitespace );
	}
	catch (const boost::property_tree::xml_parser_error&)
	{
		std::cout << "Cannot open or parse XML file. Make sure it is well-formed XML.\n";
		exit(EXIT_FAILURE);
	}
	myx52.set_xmlfile(&xml_file);

	if (myx52.construct_successful()) {
		if (x52hid.initialize() == 1)
		{
			std::cout << "HID path found: " << x52hid.getHIDPath() << std::endl;
			myx52.set_x52HID(x52hid);
			if (mfddelayms != 0) {
				x52hid.setMFDCharDelay(mfddelayms);
			}
		}
		else
		{
			std::cout << "Cannot initialize HID communication. Is X52 Pro plugged in? Is the Saitek / Logitech driver installed?" << std::endl;
			exit(EXIT_FAILURE);
		}

		// Connect to MSFS
		if (SUCCEEDED(SimConnect_Open(&hSimConnect, "x52 msfs out client", NULL, 0, 0, 0)))
		{
			printf("\nConnected to Flight Simulator!\n");
			myx52.set_simconnect_handle(hSimConnect);

			// Connect to WASimCommander module within Simulator using default timeout period and network configuration (local Simulator)
			WASimCommander::Client::WASimClient tempclient = WASimCommander::Client::WASimClient(0x52C11E47);  // "x52CLIENT"
			wasimclient = &tempclient;
			if ((hr = wasimclient->connectSimulator()) != S_OK) {
				std::cout << "WASimClient cannot connect to Simulator, quitting. Error Code: " + hr;
				exit(EXIT_FAILURE);
			}
			// Ping the WASimCommander server to make sure it's running and get the server version number (returns zero if no response).
			const uint32_t wasimversion = wasimclient->pingServer();
			if (wasimversion == 0) {
				std::cout << "WASimClient server did not respond to ping, quitting.";
				exit(EXIT_FAILURE);
			}
			// Decode version number to dotted format and print it
			std::cout << "Found WASimModule Server v" << (wasimversion >> 24) << '.' << ((wasimversion >> 16) & 0xFF) << '.' << ((wasimversion >> 8) & 0xFF) << '.' << (wasimversion & 0xFF);
			// Try to connect to the server, using default timeout value.
			if ((hr = wasimclient->connectServer()) != S_OK) {
				std::cout << "WASimClient Server connection failed, quitting. Error Code: " << hr;
				exit(EXIT_FAILURE);
			}
			myx52.set_wasimconnect_instance(tempclient);

			// BEGIN Request MSFS to send us all data mentioned in the master tag at Dispatch
			std::string attr, dataref, unit;
			size_t separatorpos;
			for (boost::property_tree::ptree::value_type &v : xml_file.get_child("master")) { // Read all children of master tag
				if (v.first == "target") // only process target tags
				{
					// For each target, request the value of switch_dataref and brightness_dataref.
					try
					{
					attr = v.second.get<std::string>("<xmlattr>.switch_dataref");
					separatorpos = attr.find("%");
					dataref = attr.substr(0, separatorpos);
					unit = attr.substr(separatorpos + 1);
					// We assume, switch_dataref is not empty, because it is a mandatory attribute.
					hr = SimConnect_AddToDataDefinition(hSimConnect, DEF_MASTER, dataref.c_str(), unit.c_str() );
					}
					catch (const boost::property_tree::ptree_bad_path&)
					{
						std::cout << "switch_dataref attribute for the " << v.second.get<std::string>("<xmlattr>.id") << " target is missing from the XML configuration! This might cause errors later!" << std::endl;
					}

					try
					{
					attr = v.second.get<std::string>("<xmlattr>.brightness_dataref");
					separatorpos = attr.find("%");
					dataref = attr.substr(0, separatorpos);
					unit = attr.substr(separatorpos + 1);
					// We assume, brightness_dataref is not empty, because it is a mandatory attribute.
					hr = SimConnect_AddToDataDefinition(hSimConnect, DEF_MASTER, dataref.c_str(), unit.c_str());
					}
					catch (const boost::property_tree::ptree_bad_path&)
					{
						std::cout << "brightness_dataref attribute for the " << v.second.get<std::string>("<xmlattr>.id") << " target is missing from the XML configuration! This might cause errors later!" << std::endl;
					}
				}
			}
			hr = SimConnect_RequestDataOnSimObject(hSimConnect,
				REQ_MASTER,
				DEF_MASTER,
				SIMCONNECT_SIMOBJECT_TYPE_USER,
				SIMCONNECT_PERIOD_VISUAL_FRAME,
				SIMCONNECT_DATA_REQUEST_FLAG_CHANGED, // Only send data when it has changed
				0, // origin: wait 0 frames before transmission starts
				2, // interval: send data every 2 frames
				0  // limit: 0 = send endlessly
			);
			// END Request MSFS to send us all data mentioned in the master tag at Dispatch

			// BEGIN Request the absolute time for various timing purposes
			hr = SimConnect_AddToDataDefinition(hSimConnect, DEF_ABSTIME, "ABSOLUTE TIME", "Seconds", SIMCONNECT_DATATYPE_FLOAT64);
			hr = SimConnect_RequestDataOnSimObject(hSimConnect,
				REQ_ABSTIME,
				DEF_ABSTIME,
				SIMCONNECT_SIMOBJECT_TYPE_USER,
				SIMCONNECT_PERIOD_VISUAL_FRAME,
				0, // Flags: none
				0, // origin: wait 0 frames before transmission starts
				2, // interval: send data every 2 frames
				0  // limit: 0 = send endlessly
			);
			// END Request the absolute time for various timing purposes

			// BEGIN Request data for 32 joy buttons from MSFS
			// X52 has 39 buttons but SimConnect only works with 32 buttons.
			// Bug has already been logged by Microsoft.
			// We map each joy button press and release SimEvent to a
			// ClientEvent which we number starting from myx52.EVENT_JOYBUTTON_PRESS
			// and myx52.EVENT_JOYBUTTON_RELEASE.
			// The events are placed in Notification Group GROUP_JOYBUTTONS.
			std::string joystickname;
			for (int i = 0; i < 32; i++)
			{
				hr = SimConnect_MapClientEventToSimEvent(hSimConnect, myx52.EVENT_JOYBUTTON_PRESS + i);
				hr = SimConnect_MapClientEventToSimEvent(hSimConnect, myx52.EVENT_JOYBUTTON_RELEASE + i);
				hr = SimConnect_AddClientEventToNotificationGroup(hSimConnect, GROUP_JOYBUTTONS, myx52.EVENT_JOYBUTTON_PRESS + i);
				hr = SimConnect_AddClientEventToNotificationGroup(hSimConnect, GROUP_JOYBUTTONS, myx52.EVENT_JOYBUTTON_RELEASE + i);
				joystickname = "joystick:" + std::to_string(joystick) + ":button:" + std::to_string(i);
					hr = SimConnect_MapInputEventToClientEvent(hSimConnect,
					INPUT_JOYBUTTONS,
					joystickname.c_str(),
					myx52.EVENT_JOYBUTTON_PRESS + i,	 // Down event
					1,							 // Down value
					myx52.EVENT_JOYBUTTON_RELEASE + i, // Up event
					0							 // Up value
					);
			}
			hr = SimConnect_SetNotificationGroupPriority(hSimConnect, GROUP_JOYBUTTONS, SIMCONNECT_GROUP_PRIORITY_HIGHEST);
			hr = SimConnect_SetInputGroupState(hSimConnect, INPUT_JOYBUTTONS, SIMCONNECT_STATE_ON);
			// END Request data for 32 joy buttons from MSFS

			// Initialize LEDs and MFD. Switch off LEDs which were set to a color
			// in Windows' "USB Game Controllers" window.
			myx52.all_on("led", false);
			myx52.all_on("mfd", false);

			std::deque<std::string> toProcess;
			// The infinite MSFS processing loop
			while (constcharn.compare(0, 1, quit) == 0)
			{
				// BEGIN of setting the quit variable when q key press detected in the other thread
				{
					// critical section
					std::unique_lock<std::mutex> lock{ mutex };
					if (cv.wait_for(lock, std::chrono::seconds(0), [&] { return !lines.empty(); })) {
						// get a new batch of lines to process
						std::swap(lines, toProcess);
					}
				}
				if (!toProcess.empty()) {
					std::cout << "processing new lines..." << std::endl;
					for (auto&& line : toProcess) {
						// process lines received by io thread
						quit = line;
					}
					toProcess.clear();
				}
				// END of setting the quit variable when q key press detected in the other thread

				SimConnect_CallDispatch(hSimConnect, MyDispatchProcRD, NULL);
				// After all dispatched events were processed,
				// some further processing is done.
				myx52.shift_state_action(xml_file);
				// Are leds on? They are on or off based on the master tag.
				if (myx52.led_on)
				{
					myx52.dataref_ind_action("", xml_file.get_child("indicators"));
				}
			}

			hr = SimConnect_Close(hSimConnect);
		}
		else {
			printf("\nCannot connect to Flight Simulator!");
			exit(EXIT_FAILURE);
		}
	}
	else
	{
		std::cout << "Cannot create X52 object.\n";
	}
	
	std::cout << "Starting Logitech DirectOutput service.\n";
	bRet = StartService(hService, 0, NULL);
	assert(bRet);
	CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
}

// Simulator time (to check if it is day or night) (E:LOCAL TIME, seconds)  https://docs.flightsimulator.com/html/Programming_Tools/Programming_APIs.htm#EnvironmentVariables

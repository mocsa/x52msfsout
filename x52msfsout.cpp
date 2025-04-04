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

#include <string>
#include <conio.h>   // For _kbhit() and _getch()
#include <csignal>   // For signal handling

#include <iostream>
#include <fstream>
#include "x52.h"
#include "LedBlinker.h"
#include <cstdlib>

#include <hidsdi.h>
#include <hidpi.h>

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

HANDLE  hSimConnect = NULL;
X52 myx52;
x52HID x52hid;
boost::property_tree::ptree xml_file; // Create empty property tree object
WASimCommander::Client::WASimClient* wasimclient;
uint32_t lastIndicatorRequestID = 1;
/// <summary>
/// Exit the infinite processing while loop in main()
/// </summary>
volatile bool exitMainWhileLoop = false;

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

struct LogitechDirectOutputService {
	BOOL stopped;		  // Stopping of service was successful
	SC_HANDLE hSCManager; // Used to restart the service when we quit
	SC_HANDLE hService;   // Used to restart the service when we quit
};
LogitechDirectOutputService LogitechServiceResults;

std::map<int, X52::DataForIndicators> dataForIndicatorsMap;

void CALLBACK MyDispatchProcRD(SIMCONNECT_RECV* pData, DWORD cbData, void* pContext)
{
	switch (pData->dwID)
	{
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

void handleRawInputData(LPARAM lParam)
{
	UINT size = 0;
	bool joybuttonstatesnew[39] = { false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false, false };
	GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &size, sizeof(RAWINPUTHEADER));
	RAWINPUT* input = (RAWINPUT*)malloc(size);
	bool gotInput = GetRawInputData((HRAWINPUT)lParam, RID_INPUT, input, &size, sizeof(RAWINPUTHEADER)) > 0;
	if (gotInput)
	{
		GetRawInputDeviceInfo(input->header.hDevice, RIDI_PREPARSEDDATA, 0, &size);
		_HIDP_PREPARSED_DATA* data = (_HIDP_PREPARSED_DATA*)malloc(size);
		bool gotPreparsedData = GetRawInputDeviceInfo(input->header.hDevice, RIDI_PREPARSEDDATA, data, &size) > 0;
		if (gotPreparsedData && input->header.hDevice == x52hid.getHIDHandle()) // Filter only for X52 joystick related messages
		{
			HIDP_CAPS caps;
			HidP_GetCaps(data, &caps);

			HIDP_BUTTON_CAPS* buttonCaps = (HIDP_BUTTON_CAPS*)malloc(caps.NumberInputButtonCaps * sizeof(HIDP_BUTTON_CAPS));
			HidP_GetButtonCaps(HidP_Input, buttonCaps, &caps.NumberInputButtonCaps, data);
			for (USHORT i = 0; i < caps.NumberInputButtonCaps; ++i)
			{
				ULONG usageCount = buttonCaps->Range.UsageMax - buttonCaps->Range.UsageMin + 1;
				USAGE* usages = (USAGE*)malloc(sizeof(USAGE) * usageCount);
				HidP_GetUsages(HidP_Input, buttonCaps[i].UsagePage, 0, usages, &usageCount, data, (PCHAR)input->data.hid.bRawData, input->data.hid.dwSizeHid);
				for (ULONG usageIndex = 0; usageIndex < usageCount; ++usageIndex) {
					//printf("%d ", usages[usageIndex]);
					joybuttonstatesnew[usages[usageIndex] - 1] = true;
				}

				for (USHORT buttonIndex = 0; buttonIndex < 39; ++buttonIndex) {
					// Was the button not pressed before? The it has just been pressed.
					if (myx52.joybuttonstates[buttonIndex] == false && joybuttonstatesnew[buttonIndex] == true) {
						myx52.logg("INFO", "x52.cpp:" + std::to_string(__LINE__), "Joy Button " + std::to_string(buttonIndex + 1) + " was pressed.");

						// Carry out actions declared in the assignments tag for button press
						myx52.assignment_button_action(xml_file.get_child("assignments"), buttonIndex + 1, "pressed");
						try
						{
							// Carry out actions declared in the mfd tag for buttons
							//myx52.mfd_button_action(xml_file.get_child("mfd"), i + 1);
						}
						catch (const boost::property_tree::ptree_bad_path&)
						{

						}
					}

					// Button not pressed now. Was it pressed before? Then it has just been released.
					if (myx52.joybuttonstates[buttonIndex] == true && joybuttonstatesnew[buttonIndex] == false) {
						myx52.logg("INFO", "x52.cpp:" + std::to_string(__LINE__), "Joy Button " + std::to_string(buttonIndex + 1) + " was released.");
						// Carry out actions declared in the assignments tag for button release
						myx52.assignment_button_action(xml_file.get_child("assignments"), buttonIndex + 1, "released");
					}
					
					// Update our array with the current state of the button.
					myx52.joybuttonstates[buttonIndex] = joybuttonstatesnew[buttonIndex];
				}
				free(usages);
			}
			free(buttonCaps);

			// After the state of all X52 buttons were collected,
			// check if the shift state was changed.
			myx52.shift_state_action(xml_file);
		}
		free(data);
	}
	free(input);
}

void LogitechServiceStop()
{
	// Example code: https://learn.microsoft.com/en-us/windows/win32/services/svccontrol-cpp
	// https://stackoverflow.com/questions/7808085/
	LogitechServiceResults.hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
	if (LogitechServiceResults.hSCManager == NULL)
	{
		std::cerr << "Cannot open Windows Service manager." << "\n";
		LogitechServiceResults.stopped = false;
		return;
	}
	LogitechServiceResults.hService = OpenService(LogitechServiceResults.hSCManager, "SaiDOutput", SERVICE_STOP | SERVICE_START | SERVICE_QUERY_STATUS);
	if (!LogitechServiceResults.hService) {
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

		LogitechServiceResults.stopped = false;
		return;
	}
	SERVICE_STATUS_PROCESS ssp;
	DWORD cbBytesNeeded;
	if (!QueryServiceStatusEx(LogitechServiceResults.hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof ssp, &cbBytesNeeded))
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
		CloseServiceHandle(LogitechServiceResults.hService); 
		CloseServiceHandle(LogitechServiceResults.hSCManager);
		LogitechServiceResults.stopped = false;
		return;
	}
	BOOL bRet;
	if (ssp.dwCurrentState != SERVICE_STOPPED) {
		std::cout << "Logitech DirectOutput service is running. Trying to stop. (Current status " << ssp.dwCurrentState << ")\n";
		bRet = ControlService(LogitechServiceResults.hService, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS)&ssp);
		//assert(bRet);
		int stopTries = 0;
		while (ssp.dwCurrentState != SERVICE_STOPPED) {
			Sleep(ssp.dwWaitHint);
			QueryServiceStatusEx(LogitechServiceResults.hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof ssp, &cbBytesNeeded);
			if (stopTries++ == 4)
			{
				std::cout << "Logitech DirectOutput service took too long to stop.\n";
				LogitechServiceResults.stopped = false;
				return;
			}
		}
		LogitechServiceResults.stopped = true;
	}
	else {
		// Service is not running
		std::cerr << "Logitech DirectOutput service is not running. Not trying to stop." << "\n";
		LogitechServiceResults.stopped = true;
	}
}

void LogitechServiceStart()
{
	BOOL bRet = StartService(LogitechServiceResults.hService, 0, NULL);
	assert(bRet);
	CloseServiceHandle(LogitechServiceResults.hService);
	CloseServiceHandle(LogitechServiceResults.hSCManager);
}

// Mandatory window function with only default content.
LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

HWND CreateWindowForRawInput()
{
	WNDCLASS wnd = { 0 };
	wnd.hInstance = GetModuleHandle(0);
	wnd.lpfnWndProc = WindowProcedure;
	wnd.lpszClassName = TEXT("Raw Input x52msfsout");
	RegisterClass(&wnd);
	HWND hwnd = CreateWindow(wnd.lpszClassName, 0, 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, wnd.hInstance, 0);
	// Register devices so that Windows knows that we want to receive WM_INPUT messages
	// About RawInput https://learn.microsoft.com/en-us/windows/win32/inputdev/about-raw-input
	RAWINPUTDEVICE deviceList[1];
	deviceList[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
	deviceList[0].usUsage = HID_USAGE_GENERIC_JOYSTICK;
	deviceList[0].dwFlags = RIDEV_INPUTSINK; // InputSink receives messages even when in the background. See https://learn.microsoft.com/en-us/windows/win32/api/winuser/ns-winuser-rawinputdevice
	deviceList[0].hwndTarget = hwnd;
	RegisterRawInputDevices(deviceList, 1 /*deviceCount*/, sizeof(RAWINPUTDEVICE));
	return hwnd;
}

// Signal handler for CTRL+C
void signalHandler(int signum) {
    std::cout << "\nCTRL+C detected! Cleaning up...\n";
    exitMainWhileLoop = true;
}

/// Function to release resources properly
void cleanup() {
	if(hSimConnect != nullptr) 
	{
		SimConnect_Close(hSimConnect);
	}
	std::cout << "Starting Logitech DirectOutput service.\n";
	LogitechServiceStart();
    std::cout << "Cleanup complete.\n";
	// myx52 destructor called automatically
	// x52HID destructor called automatically
	// LedBlinker destructor called automatically
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
	std::cerr << "\nPerforming final cleanup...\n";
	cleanup();
    std::abort();
}


void DataRequestsForIndicators(std::string tagname, boost::property_tree::ptree &xmltree)
{
	X52::DataForIndicators dataForIndicatorsStruct;
	try
	{
		// Discover all branches of the indicators tag
		for (boost::property_tree::ptree::value_type& v : xmltree)
		{
			if (v.first == "led") // a led tag which contains multiple state tags
			{
				if (v.second.count("state") != 0) // This led has a state declared
				{
					DataRequestsForIndicators(v.first, v.second);
				}
			} else if (v.first == "state") { // a state tag below a led tag or below another state tag
				DataRequestsForIndicators(v.first, v.second);
			}
		}
		// No subitems to loop into, we're at bottom (innermost state)
		if (tagname == "state") {
			std::string attr;
			std::string dataref;
			std::string unit;
			bool foundsamedataref = false;
			uint8_t simvarindex = 0;
			size_t separatorpos;
			float delta;

			attr = xmltree.get<std::string>("<xmlattr>.dataref");
			// Separate SimVar name from unit of measurement
			separatorpos = attr.find("%");
			dataref = attr.substr(0, separatorpos);
			unit = attr.substr(separatorpos + 1);
			// If an index is given, read it from the XML
			if (xmltree.get("<xmlattr>.index", "nil") != "nil")
			{
				simvarindex = xmltree.get<int>("<xmlattr>.index");
			}
			// If a delta is given, read it from the XML
			if (xmltree.get("<xmlattr>.delta", "nil") != "nil")
			{
				delta = xmltree.get<float>("<xmlattr>.delta");
			}
			else
			{
				delta = 0.0f; // Only notify us when value has changed, no matter with how small amount
			}

			// If we already asked for the same dataref + unit + simvarindex then
			// that is the same request. Don't request it again, just store its RequestID.
			for (auto it = dataForIndicatorsMap.begin(); it != dataForIndicatorsMap.end(); ++it)
			{
				if (it->second.dataref == dataref &&
					it->second.unit == unit &&
					it->second.simvarindex == simvarindex)
				{
					xmltree.put("<xmlattr>.requestid", it->first);
					foundsamedataref = true;
					break;
				}
			}

			if(!foundsamedataref)
			{
				// Store the data for later when we react to data changes
				dataForIndicatorsStruct.dataref = dataref;
				dataForIndicatorsStruct.unit = unit;
				dataForIndicatorsStruct.simvarindex = simvarindex;
				dataForIndicatorsStruct.value = 0;
				dataForIndicatorsMap.insert({ lastIndicatorRequestID, dataForIndicatorsStruct });

				wasimclient->saveDataRequest(
					WASimCommander::DataRequest(
						lastIndicatorRequestID,
						/* valueSize */			WASimCommander::DATA_TYPE_DOUBLE,
						/* requestType */		WASimCommander::Enums::RequestType::Named,
						/* calcResultType */	WASimCommander::Enums::CalcResultType::None,
						/* period */			WASimCommander::Enums::UpdatePeriod::Millisecond,
						/* nameOrCode */		dataref.c_str(),
						/* unitName */			unit.c_str(),
						/* varTypePrefix */		'A', //  'L' (local), 'A' (SimVar) and 'T' (Token, not an actual GaugeAPI prefix) are checked using respective GaugeAPI methods
						/* deltaEpsilon */		delta,
						/* interval */			50,   // Wait 50ms between checking value
						/* simVarIndex */		simvarindex
					)
				);
				// Store the request ID in the XML
				xmltree.put("<xmlattr>.requestid", lastIndicatorRequestID);
				std::cout << "Data requested via WASim for Dataref " << dataref << ", SimVarIndex " << std::to_string(simvarindex) << ", Unit: " << unit << " using RequestID " << lastIndicatorRequestID << "." << std::endl;
				lastIndicatorRequestID++;
			}
		}
	}
	catch (const boost::property_tree::ptree_error&)
	{
		std::cerr << "Error with registering WASim DataRequests for indicators.\n";
	}
}

int main(int argc, char *argv[])
{
	char lastPressedKey = 'a';
	std::string xmlconfig;
	long mfddelayms = 0;

	HRESULT hr;

	// Register signal handler for CTRL+C
    std::signal(SIGINT, signalHandler);
	// Set custom terminate handler
	std::set_terminate(handleTerminate);

	// BEGIN Check command-line options
	try
	{
		boost::program_options::options_description desc("Allowed options");
		desc.add_options()
			("help", "Display help message")
			("xmlconfig", boost::program_options::value<std::string>(&xmlconfig)->required(), "XML configuration file")
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

	LogitechServiceStop(); // Puts its results into struct LogitechServiceResults
	if (LogitechServiceResults.stopped == false) {
        // Possible errors were already printed by LogitechServiceStop()
		exit(EXIT_FAILURE);
	}

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

	LedBlinker ledBlinker;
	ledBlinker.set_x52(myx52);
	myx52.set_LedBlinker(ledBlinker);

	myx52.set_xmlfile(&xml_file);
	if (!myx52.validateSequences())
	{
		exit(EXIT_FAILURE);
	}

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
			cleanup();
			return EXIT_FAILURE;
		}

		// Create a window to receive raw input
		HWND hwnd = CreateWindowForRawInput();

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
				cleanup();
				return EXIT_FAILURE;
			}
			// Ping the WASimCommander server to make sure it's running and get the server version number (returns zero if no response).
			const uint32_t wasimversion = wasimclient->pingServer();
			if (wasimversion == 0) {
				std::cout << "WASimClient server did not respond to ping, quitting.";
				cleanup();
				return EXIT_FAILURE;
			}
			// Decode version number to dotted format and print it
			std::cout << "Found WASimModule Server v" << (wasimversion >> 24) << '.' << ((wasimversion >> 16) & 0xFF) << '.' << ((wasimversion >> 8) & 0xFF) << '.' << (wasimversion & 0xFF);
			// Try to connect to the server, using default timeout value.
			if ((hr = wasimclient->connectServer()) != S_OK) {
				std::cout << "WASimClient Server connection failed, quitting. Error Code: " << hr;
				cleanup();
				return EXIT_FAILURE;
			}
			myx52.set_wasimconnect_instance(tempclient);

			if (xml_file.count("indicators") != 0)
			{
				wasimclient->setDataCallback(&X52::IndicatorDataCallback, &myx52);
				myx52.setDataForIndicatorsMap(dataForIndicatorsMap); // This should happen before registering DataRequests, because after registration the callback is immediately called and it needs to access the Map.
				DataRequestsForIndicators("", xml_file.get_child("indicators"));
			}

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
				1, // interval: wait 1 frame (interval) before sending next data
				0  // limit: 0 = send endlessly
			);
			// END Request MSFS to send us all data mentioned in the master tag at Dispatch

			// Initialize LEDs and MFD. Switch off LEDs which were set to a color
			// in Windows' "USB Game Controllers" window.
			myx52.all_on("led", false);
			myx52.all_on("mfd", false);

			// The infinite MSFS processing loop
			MSG msg;

			try
			{
				while (!exitMainWhileLoop)
				{
					// BEGIN of setting the exit variable when q key press detected
					if (_kbhit())
					{
						char ch = _getche(); // Read char and also print it to the console
						if (ch == '\r') {  
							if (lastPressedKey == 'q') {
								std::cout << "Exit command received.\n";
								exitMainWhileLoop = true;
								break;
							}
							lastPressedKey = 0;
						} else {
							lastPressedKey = ch;
						}
					}
					// END of setting the exit variable when q key press detected

					// Get WM_INPUT messages via the invisible window we opened above
					if (PeekMessage(&msg, hwnd, WM_INPUT, WM_INPUT, PM_REMOVE | PM_QS_INPUT)) {
						handleRawInputData(msg.lParam);
					}

					SimConnect_CallDispatch(hSimConnect, MyDispatchProcRD, NULL);
				}
			} catch (const std::exception& e) {
				std::cerr << "Exception caught: " << e.what() << "\n";
				cleanup();
			} catch (...) {
				std::cerr << "Unknown exception caught!\n";
				cleanup();
			}
		}
		else {
			printf("\nCannot connect to Flight Simulator!");
			cleanup();
			return EXIT_FAILURE;

		}
	}
	else
	{
		std::cout << "Cannot create X52 object.\n";
	}
	
	cleanup();
	return EXIT_SUCCESS;
}

// Simulator time (to check if it is day or night) (E:LOCAL TIME, seconds)  https://docs.flightsimulator.com/html/Programming_Tools/Programming_APIs.htm#EnvironmentVariables

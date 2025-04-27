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
#include "easylogging++.h"
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

INITIALIZE_EASYLOGGINGPP

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

const char* const ExceptionList[] = {
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
						CLOG(DEBUG,"toconsole", "tofile") << fmt::format( "Simulator data for Master Target '{}' has changed. Switch data is now {:.2f} and brightness data is {:.2f}.", v.second.get<std::string>("<xmlattr>.id").c_str(), pS->dataarray[targetnumber], pS->dataarray[targetnumber + 1]);
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

							CLOG(DEBUG,"toconsole", "tofile") << fmt::format( "Setting brightness of Master Target '{}' to {:.2f}.", v.second.get<std::string>("<xmlattr>.id").c_str(), (pS->dataarray[targetnumber+1] - std::stod(v.second.get<std::string>("<xmlattr>.min"))) / (std::stod(v.second.get<std::string>("<xmlattr>.max")) - std::stod(v.second.get<std::string>("<xmlattr>.min"))) * 128 * std::stod(v.second.get<std::string>("<xmlattr>.default")) / 100);
							x52hid.setBrightness(v.second.get<std::string>("<xmlattr>.id"), (pS->dataarray[targetnumber+1] - std::stod(v.second.get<std::string>("<xmlattr>.min"))) / (std::stod(v.second.get<std::string>("<xmlattr>.max")) - std::stod(v.second.get<std::string>("<xmlattr>.min"))) * 128 * std::stod(v.second.get<std::string>("<xmlattr>.default")) / 100 );
						}
						else
						{
							if (v.second.get<std::string>("<xmlattr>.on", "") != "false") {
								// If this target is not already off, switch it off.
								myx52.all_on(v.second.get<std::string>("<xmlattr>.id"), false);
								// Set this target's brightness to 0.
								CLOG(DEBUG,"toconsole", "tofile") << fmt::format( "Setting brightness of Master Target '{}' to zero.", v.second.get<std::string>("<xmlattr>.id").c_str());
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
			CLOG(WARNING,"toconsole", "tofile") << "Dispatcher did nothing.";
			break;
		}
		break;
	}
	case SIMCONNECT_RECV_ID_EXCEPTION:
	{
		SIMCONNECT_RECV_EXCEPTION* pObjData = (SIMCONNECT_RECV_EXCEPTION*)pData;

		CLOG(ERROR,"toconsole", "tofile") << "Exception: " << ExceptionList[pObjData->dwException];
		if (pObjData->dwSendID == myx52.lastsentpacket.pdwSendID)
		{
			CLOG(ERROR,"toconsole", "tofile") << myx52.lastsentpacket.message;
		}
		else {
			CLOG(ERROR,"toconsole", "tofile") << "Packet ID doesn't match. Details not found.";
		}
		break;
	}
	default:
		CLOG(ERROR,"toconsole", "tofile") << "MyDispatchProcRD Received unhandled SIMCONNECT_RECV ID: " << pData->dwID;
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
					joybuttonstatesnew[usages[usageIndex] - 1] = true;
				}

				for (USHORT buttonIndex = 0; buttonIndex < 39; ++buttonIndex) {
					// Was the button not pressed before? The it has just been pressed.
					if (myx52.joybuttonstates[buttonIndex] == false && joybuttonstatesnew[buttonIndex] == true) {
						CLOG(TRACE,"toconsole", "tofile") << "Joy Button " << std::to_string(buttonIndex + 1) << " was pressed.";

						// Carry out actions declared in the assignments tag for button press
						myx52.assignment_button_action(xml_file.get_child("assignments"), buttonIndex + 1, "pressed");
						try
						{
							// Carry out actions declared in the mfd tag for buttons
							//myx52.mfd_button_action(xml_file.get_child("mfd"), i + 1);
						}
						catch (const boost::property_tree::ptree_bad_path& e)
						{
							CLOG(ERROR, "toconsole", "tofile") << "Boost ptree could not read XML path " << e.path<std::string>() << ". Error message: " << e.what() << ".";
						}
					}

					// Button not pressed now. Was it pressed before? Then it has just been released.
					if (myx52.joybuttonstates[buttonIndex] == true && joybuttonstatesnew[buttonIndex] == false) {
						CLOG(TRACE,"toconsole", "tofile") << "Joy Button " << std::to_string(buttonIndex + 1) << " was released.";
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
		CLOG(FATAL,"toconsole", "tofile") << "Cannot open Windows Service manager.";
		LogitechServiceResults.stopped = false;
		return;
	}
	LogitechServiceResults.hService = OpenService(LogitechServiceResults.hSCManager, "SaiDOutput", SERVICE_STOP | SERVICE_START | SERVICE_QUERY_STATUS);
	if (!LogitechServiceResults.hService) {
		CLOG(FATAL,"toconsole", "tofile") << "Cannot open Logitech DirectOutput service.";

		LPSTR messageBuffer = nullptr;

		// Use FormatMessage to convert the error code into a readable message
		size_t size = FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

		// Copy the message into a std::string
		std::string message(messageBuffer, size);

		// Free the buffer allocated by FormatMessage
		LocalFree(messageBuffer);
		CLOG(FATAL,"toconsole", "tofile") << message;

		LogitechServiceResults.stopped = false;
		return;
	}
	SERVICE_STATUS_PROCESS ssp;
	DWORD cbBytesNeeded;
	if (!QueryServiceStatusEx(LogitechServiceResults.hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof ssp, &cbBytesNeeded))
	{
		CLOG(FATAL,"toconsole", "tofile") << "QueryServiceStatusEx failed (" << GetLastError() << ")";

		LPSTR messageBuffer = nullptr;

		// Use FormatMessage to convert the error code into a readable message
		size_t size = FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&messageBuffer, 0, NULL);

		// Copy the message into a std::string
		std::string message(messageBuffer, size);

		// Free the buffer allocated by FormatMessage
		LocalFree(messageBuffer);
		CLOG(FATAL,"toconsole", "tofile") << message;
		CloseServiceHandle(LogitechServiceResults.hService); 
		CloseServiceHandle(LogitechServiceResults.hSCManager);
		LogitechServiceResults.stopped = false;
		return;
	}
	BOOL bRet;
	if (ssp.dwCurrentState != SERVICE_STOPPED) {
		CLOG(INFO,"toconsole", "tofile") << "Logitech DirectOutput service is running. Trying to stop. (Current status " << ssp.dwCurrentState << ")";
		bRet = ControlService(LogitechServiceResults.hService, SERVICE_CONTROL_STOP, (LPSERVICE_STATUS)&ssp);
		//assert(bRet);
		int stopTries = 0;
		while (ssp.dwCurrentState != SERVICE_STOPPED) {
			Sleep(ssp.dwWaitHint);
			QueryServiceStatusEx(LogitechServiceResults.hService, SC_STATUS_PROCESS_INFO, (LPBYTE)&ssp, sizeof ssp, &cbBytesNeeded);
			if (stopTries++ == 4)
			{
				CLOG(FATAL,"toconsole", "tofile") << "Logitech DirectOutput service took too long to stop.";
				LogitechServiceResults.stopped = false;
				return;
			}
		}
		LogitechServiceResults.stopped = true;
	}
	else {
		// Service is not running
		CLOG(INFO,"toconsole", "tofile") << "Logitech DirectOutput service is not running. Not trying to stop.";
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

std::string GetExecutablePath()
{
    wchar_t path[MAX_PATH]; // Buffer to store path
    
    if (GetModuleFileNameW(NULL, path, MAX_PATH) == 0)
    {
        CLOG(ERROR,"toconsole", "tofile") << "Failed to get executable path, error: " << GetLastError();
        return "";
    }
    
	// Keep only the path and cut-off the executable program name
	if (wchar_t* lastSlash = wcsrchr(path, L'\\')) {  // Find the last backslash
        *lastSlash = L'\0'; // Remove the filename by terminating the string early
	}
	// Convert the wide-character path into a standard UTF std::string
    std::vector<char> buffer(wcslen(path) * 4, 0); // Allocate buffer
    std::mbstate_t state{};
    const wchar_t* src = path;
    std::size_t len = std::wcsrtombs(buffer.data(), &src, buffer.size(), &state);
    return (len != static_cast<std::size_t>(-1)) ? std::string(buffer.data(), len) : "";
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

void SetupEasylogging()
{
	// Set custom defaults for easylogging++
	// Global settings
	el::Loggers::addFlag(el::LoggingFlag::ColoredTerminalOutput);
	el::Loggers::addFlag(el::LoggingFlag::MultiLoggerSupport);
	el::Loggers::addFlag(el::LoggingFlag::DisableApplicationAbortOnFatalLog);
	// Global settings added to easylogging++.h:
	// #define ELPP_THREAD_SAFE
	// #define ELPP_NO_DEFAULT_LOG_FILE
	// #define ELPP_UNICODE
	// Global settings added to easylogging++.cc:
	// #define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
	
	char *dummy_argv[1];
	// Only called because easylogging++ unicode support requires it.
	// We deliberately not pass any command-line arguments to the function,
	// because we'll handle that manually in AdjustEasyloggingConf().
	START_EASYLOGGINGPP(0, dummy_argv);

	// Create new logger for console logging
	el::Loggers::getLogger("toconsole"); // Create new "toconsole" logger
	el::Configurations consoleConf;
	consoleConf.setGlobally(el::ConfigurationType::Format, "[%level] %msg");
	consoleConf.setGlobally(el::ConfigurationType::ToFile, "false");
	consoleConf.set(el::Level::Debug, el::ConfigurationType::Enabled, "false"); // Hide DEBUG by default
	consoleConf.set(el::Level::Trace, el::ConfigurationType::Enabled, "false"); // Hide TRACE by default
    el::Loggers::reconfigureLogger("toconsole", consoleConf);
	
	// Create new logger for file logging
	el::Loggers::getLogger("tofile"); // Create new "tofile" logger
	el::Configurations fileConf;
	fileConf.setGlobally(el::ConfigurationType::Enabled, "false");  // Default to deactivated
	fileConf.setGlobally(el::ConfigurationType::ToStandardOutput, "false");
	fileConf.setGlobally(el::ConfigurationType::ToFile, "true");
	fileConf.setGlobally(el::ConfigurationType::Format, "%datetime{%H:%m:%s,%g} %level (%fbase, %line.) %msg");
	fileConf.setGlobally(el::ConfigurationType::Filename, GetExecutablePath() + "\\x52msfsout_log.txt");
	fileConf.set(el::Level::Debug, el::ConfigurationType::Enabled, "false"); // Hide DEBUG by default
	fileConf.set(el::Level::Trace, el::ConfigurationType::Enabled, "false"); // Hide TRACE by default
	el::Loggers::reconfigureLogger("tofile", fileConf);
}

void AdjustEasyloggingConf(const bool& logtofile, const bool& logdebug, const bool& logtrace)
{
	// Turn on logging to file
	if (logtofile) {
		el::Configurations fileConf;
		fileConf.setGlobally(el::ConfigurationType::Enabled, "true");
		el::Loggers::reconfigureLogger("tofile", fileConf);
		CLOG(INFO,"toconsole") << "Log is being written to " << GetExecutablePath() << "\\x52msfsout_log.txt.";
		std::time_t t = std::time(nullptr);
		std::tm localTime;
		localtime_s(&localTime, &t);
		std::ostringstream oss;
		oss << std::put_time(&localTime, "%d %B %Y");
		CLOG(INFO, "tofile") << "------------ x52msfsout started on " << oss.str() << " ------------";
	}
	if (logdebug) {
		el::Configurations tempConf;
		tempConf = *el::Loggers::getLogger("toconsole")->configurations();
		tempConf.set(el::Level::Debug, el::ConfigurationType::Enabled, "true");
		el::Loggers::reconfigureLogger("toconsole", tempConf);
		tempConf = *el::Loggers::getLogger("tofile")->configurations();
		tempConf.set(el::Level::Debug, el::ConfigurationType::Enabled, "true");
		el::Loggers::reconfigureLogger("tofile", tempConf);
	}
	if (logtrace) {
		el::Configurations tempConf;
		tempConf = *el::Loggers::getLogger("toconsole")->configurations();
		tempConf.set(el::Level::Trace, el::ConfigurationType::Enabled, "true");
		el::Loggers::reconfigureLogger("toconsole", tempConf);
		tempConf = *el::Loggers::getLogger("tofile")->configurations();
		tempConf.set(el::Level::Trace, el::ConfigurationType::Enabled, "true");
		el::Loggers::reconfigureLogger("tofile", tempConf);
	}
}

// Signal handler for CTRL+C
void signalHandler(int signum) {
    CLOG(INFO,"toconsole", "tofile") << "CTRL+C detected! Cleaning up...";
    exitMainWhileLoop = true;
}

/// Function to release resources properly
void cleanup() {
	if(hSimConnect != nullptr) 
	{
		SimConnect_Close(hSimConnect);
	}
	CLOG(INFO,"toconsole", "tofile") << "Starting Logitech DirectOutput service.";
	LogitechServiceStart();
    CLOG(INFO,"toconsole", "tofile") << "Cleanup complete.";
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
        CLOG(ERROR,"toconsole", "tofile") << "Unhandled exception: " << e.what();
    } catch (...) {
        CLOG(ERROR,"toconsole", "tofile") << "Unhandled unknown exception.";
    }
	CLOG(INFO,"toconsole", "tofile") << "Performing final cleanup...";
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
				try {
					delta = xmltree.get<float>("<xmlattr>.delta");
				}
				catch (const boost::property_tree::ptree_bad_data& e) {
					CLOG(ERROR, "toconsole", "tofile") << "For dataref " << dataref << ", unit " << unit << ", could not convert 'delta' " << e.data<std::string>() << " to a floating-point number. Check if the decimal separator is correct for your locale. Cannot request data from MSFS. Conversion error: " << e.what() << ".";
					return;
				}
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
				CLOG(DEBUG,"toconsole", "tofile") << "Data requested via WASim for Dataref " << dataref << ", SimVarIndex " << std::to_string(simvarindex) << ", Unit: " << unit << " using RequestID " << lastIndicatorRequestID << ".";
				lastIndicatorRequestID++;
			}
		}
	}
	catch (const boost::property_tree::ptree_error&)
	{
		CLOG(ERROR,"toconsole", "tofile") << "Error with registering WASim DataRequests for indicators.";
	}
}

int main(int argc, char *argv[])
{
	char lastPressedKey = 'a';
	// Command-line options
	std::string xmlconfig;
	long mfddelayms = 0;
	bool logtofile = false;
	bool logdebug = false;
	bool logtrace = false;

	HRESULT hr;

	// Register signal handler for CTRL+C
    std::signal(SIGINT, signalHandler);
	// Set custom terminate handler
	std::set_terminate(handleTerminate);

	SetupEasylogging();

	// BEGIN Check command-line options
	try
	{
		boost::program_options::options_description desc("Allowed options");
		desc.add_options()
			("help,h", "Display help message")
			("xmlconfig,x", boost::program_options::value<std::string>(&xmlconfig)->required(), "XML configuration file")
			("mfddelayms,m", boost::program_options::value<long>(&mfddelayms)->default_value(0), "Delay in ms after sending each character-pair to MFD. Defaults to 0ms.")
			("logtofile,l", boost::program_options::bool_switch(&logtofile), "In addition to console, log to file with more details. File is never deleted, only appended.")
			("logdebug,d", boost::program_options::bool_switch(&logdebug), "Debug infrequent events.")
			("logtrace,t", boost::program_options::bool_switch(&logtrace), "Trace frequent events.")
		;
		boost::program_options::variables_map vm;
		auto parsed_options = boost::program_options::parse_command_line(argc, argv, desc);
		boost::program_options::store(parsed_options, vm);
		if (vm.count("help") || parsed_options.options.empty()) {
			std::cout << desc;
			exit(EXIT_SUCCESS);
		}
		boost::program_options::notify(vm);
	}
	catch (const std::exception& e)
	{
		CLOG(FATAL,"toconsole", "tofile") << "Error with options: " << e.what();
		exit(EXIT_FAILURE);
    }
    catch(...)
    {
        CLOG(FATAL,"toconsole", "tofile") << "Unknown error during option processing!";
        exit(EXIT_FAILURE);
    }
	// END Check command-line options

	// Change easylogging++ configuration based on command-line parameters
	AdjustEasyloggingConf(logtofile, logdebug, logtrace);

	LogitechServiceStop(); // Puts its results into struct LogitechServiceResults
	if (LogitechServiceResults.stopped == false) {
        // Possible errors were already logged by LogitechServiceStop()
		exit(EXIT_FAILURE);
	}

	CLOG(INFO,"toconsole", "tofile") << "Press q+Enter to quit!";

	// Parse the XML into the property tree.
	try
	{
		boost::property_tree::read_xml(xmlconfig, xml_file, boost::property_tree::xml_parser::no_comments + boost::property_tree::xml_parser::trim_whitespace );
	}
	catch (const boost::property_tree::xml_parser_error&)
	{
		CLOG(FATAL,"toconsole", "tofile") << "Cannot open or parse XML file. Make sure it is well-formed XML.";
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

	if (x52hid.initialize() == 1)
	{
		CLOG(DEBUG,"toconsole", "tofile") << "HID path found: " << x52hid.getHIDPath();
		myx52.set_x52HID(x52hid);
		if (mfddelayms != 0) {
			x52hid.setMFDCharDelay(mfddelayms);
		}
	}
	else
	{
		CLOG(FATAL,"toconsole", "tofile") << "Cannot initialize HID communication. Is X52 Pro plugged in? Is the Saitek / Logitech driver installed?";
		cleanup();
		return EXIT_FAILURE;
	}

	// Create a window to receive raw input
	HWND hwnd = CreateWindowForRawInput();

	// Connect to MSFS
	if (SUCCEEDED(SimConnect_Open(&hSimConnect, "x52 msfs out client", NULL, 0, 0, 0)))
	{
		CLOG(INFO,"toconsole", "tofile") << "Connected to Flight Simulator via SimConnect!";
		myx52.set_simconnect_handle(hSimConnect);

		// Connect to WASimCommander module within Simulator using default timeout period and network configuration (local Simulator)
		std::locale::global(std::locale::classic());
		auto tempclient = WASimCommander::Client::WASimClient(0x52C11E47);  // "x52CLIENT"
		std::locale::global(std::locale(""));
		wasimclient = &tempclient;
		if ((hr = wasimclient->connectSimulator()) != S_OK) {
			CLOG(FATAL,"toconsole", "tofile") << "WASimClient cannot connect to Simulator, quitting. Error Code: " + hr;
			cleanup();
			return EXIT_FAILURE;
		}
		// Ping the WASimCommander server to make sure it's running and get the server version number (returns zero if no response).
		const uint32_t wasimversion = wasimclient->pingServer();
		if (wasimversion == 0) {
			CLOG(FATAL,"toconsole", "tofile") << "WASimClient server did not respond to ping, quitting.";
			cleanup();
			return EXIT_FAILURE;
		}
		// Decode version number to dotted format and print it
		CLOG(INFO,"toconsole", "tofile") << "Found WASimModule Server v" << (wasimversion >> 24) << '.' << ((wasimversion >> 16) & 0xFF) << '.' << ((wasimversion >> 8) & 0xFF) << '.' << (wasimversion & 0xFF);
		// Try to connect to the server, using default timeout value.
		if ((hr = wasimclient->connectServer()) != S_OK) {
			CLOG(FATAL,"toconsole", "tofile") << "WASimClient Server connection failed, quitting. Error Code: " << hr;
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
					CLOG(WARNING,"toconsole", "tofile") << "switch_dataref attribute for the " << v.second.get<std::string>("<xmlattr>.id") << " target is missing from the XML configuration! This might cause errors later!";
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
					CLOG(WARNING,"toconsole", "tofile") << "brightness_dataref attribute for the " << v.second.get<std::string>("<xmlattr>.id") << " target is missing from the XML configuration! This might cause errors later!";
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
							CLOG(INFO,"toconsole", "tofile") << "Exit command received.";
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
			CLOG(FATAL,"toconsole", "tofile") << "Exception caught: " << e.what();
			cleanup();
		} catch (...) {
			CLOG(FATAL,"toconsole", "tofile") << "Unknown exception caught!";
			cleanup();
		}
	}
	else {
		CLOG(FATAL,"toconsole", "tofile") << "Cannot connect to Flight Simulator!";
		cleanup();
		return EXIT_FAILURE;

	}

	
	cleanup();
	return EXIT_SUCCESS;
}

// Simulator time (to check if it is day or night) (E:LOCAL TIME, seconds)  https://docs.flightsimulator.com/html/Programming_Tools/Programming_APIs.htm#EnvironmentVariables

# Changelog

## 0.4.0 - 2025-03-24

### Changed

- Limited maximum speed of sequences to 10.
- Read SimVars for indicators only when they change. This puts less load on the CPU and also makes the blinking of LEDs much more accurate. Also, when different indicators depend on the same simulator data (SimVar, Unit, and Index are the same) then it is requested only once.
- No longer use a separate thread to read keyboard input.

### Added

- Move blinking of LEDs into a separate thread which speeds up performance of x52msfsout.
- Re-add the Architecture diagram.
- Use Saitek HID driver to detect joystick button clicks, instead of SimConnect. This way not only 32 but all 39 buttons can be detected. --joystick command line parameter is no longer necessary. "USB game controllers" no longer need to be open.
- Add testing of FLAPS LEDs to Testing.md
- New Flowchart diagram
- Add new delta attribute to state tags.
- Cleanup code moved into a dedicated function.

### Fixed

- No longer compile the Boost Program Options library into a separate DLL. Achieved by setting both "Use Static Libraries" and "Use Dynamic CRT" to Yes in VS vcpkg properties.
- Add missing index attributes to sequence tags in default.xml. Also clarified in README.md.
- Allocate memory for lpInBuffer at each call of DeviceIoControl because the main and blinker threads can both call it. Also, make these calls thread safe using a mutex.

## 0.3.0 - 2025-01-20

### Added

- Use vcpkg for dependency management
- Build the project with a workflow on GitHub
- Added default_invalid.xml to use during testing

### Removed

- No longer depend on an existing installation of X52LuaOut. The cmd_handler of X52LuaOut is no longer used.

## 0.2.0 - 2024-07-07

### Changed

- Make log messages on master tag more helpful for debugging
- Handle the speed of data processing more consistently. The maximum speed for \<sequence\> tags is your MSFS fps divided by 2
- Clarify instructions on testing the \<master\> tag
- Insert a lot of comments into several files
- Improve the instructions on testing the SHIFT indicator
- Make it possible to compile .cpp files individually by better arranging dependencies between them

### Added

- Print helpful error message when application is terminated with an error
- Enforce the presence of switch_dataref and brightness_dataref tags in the \<master\> tag
- Clear the LED colors and MFD text on start to reset settings made by other applications, particularly the Saitek driver
- Control joystick LEDs with \<indicators\> tag and define blinking patterns with \<sequences\> tag
- Add calculator code to XML configuration files. WASimCommander module must be installed in MSFS
- Use Saitek driver instead of cmd_handler.lua to change joystick LED colors and write text to MFD
- New --mfddelayms command line parameter to avoid garbled characters on the MFD

## 0.1.1 - 2024-06-23

### Changed

- README.md is more logically ordered for new users.

### Added

- README contains more information on how to find SimVars.

### Removed

- Win32-related settings removed from Visual Studio config files.

## 0.1.0-alpha - 2024-01-03

_First release._

### Added

- \<master\> tag fully supported, with target id=mfd and led.
- \<shift_states\> fully supported.
- \<assignments\> partially supported
- \<button type="trigger_pos"\> fully supported. Can handle dataref (SimVar in MSFS) and command (inputevent in MSFS).
- \<shifted_button\> tag inside \<button type="trigger_pos"\> tag is supported. They can trigger SimVar or inputevent on button press.
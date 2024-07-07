# Changelog

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
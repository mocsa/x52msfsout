# Changelog

## 0.1.1 - 2024-06-XX

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
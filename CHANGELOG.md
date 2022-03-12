# Changelog
All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Show when messages are from a thread
- Add command `/rocketchat reply <tmid> <message>` to reply inside a thread

## [0.4.0] - 2022-03-12

### Added

- Added command `/rocketchat subscribe <name> <event>` which is useful for
  debug
- Added support for text-only attachments

### Fixed

- "sub" calls that fail because of rate limit will be automatically retried
- Do not print an error message when one of the following signals is triggered
  for another server/protocol:
    - query created
    - query destroyed
    - channel created
    - channel name changed

## [0.3.1] - 2022-02-11

### Fixed

- Fixed build for older versions of CMake (#3)
- Fixed build documentation by adding missing requirements (#2, #4)
- Subscribe to room messages after creating a direct message

## [0.3.0] - 2021-12-06

### Added

- Added support for queries for direct message between two users (use `/query`)
- Added command `/rocketchat history`

### Changed

- History is no longer loaded automatically (use the new command `/rocketchat history`)
- The `/join` command can now takes either a room ID or a room name

## [0.2.0] - 2020-10-29

### Added

- Added support for attachments


## [0.1.0] - 2020-09-24

Initial release

[Unreleased]: https://github.com/jajm/irssi-rocketchat/compare/v0.4.0...HEAD
[0.4.0]: https://github.com/jajm/irssi-rocketchat/releases/tag/v0.4.0
[0.3.1]: https://github.com/jajm/irssi-rocketchat/releases/tag/v0.3.1
[0.3.0]: https://github.com/jajm/irssi-rocketchat/releases/tag/v0.3.0
[0.2.0]: https://github.com/jajm/irssi-rocketchat/releases/tag/v0.2.0
[0.1.0]: https://github.com/jajm/irssi-rocketchat/releases/tag/v0.1.0

# phosphor-post-code-manager

This phosphor-post-code-manager repository provides an infrastructure to persist
the POST codes in BMC filesystem & it also owns the systemd services that are
responsible for exposing the BIOS Post Codes to rest of the world via redfish.

## To Build

To build phosphor-post-code-manager package , do the following steps:

```bash
meson <build directory>
ninja -C <build directory>
```

## Hosted Services

This repository ships `xyz.openbmc_project.State.Boot.PostCode.service` systemd
service along with its
[template version](https://github.com/openbmc/docs/blob/master/designs/multi-host-postcode.md)
and a tiny binary that exposes the necessary
[dbus interfaces & methods](https://github.com/openbmc/phosphor-dbus-interfaces/blob/master/yaml/xyz/openbmc_project/State/Boot/PostCode.interface.yaml)
to extract the POST codes per boot cycle.

## Architecture

This repository is tightly coupled with
[phosphor-host-postd](https://github.com/openbmc/phosphor-host-postd) OpenBMC
repository which is responsible for emitting the dbus signals for every new POST
Code.

phosphor-post-code-manager is architected to look for the property changed
signals which are being emitted from the service that hosts
[Value](https://github.com/openbmc/phosphor-dbus-interfaces/blob/master/yaml/xyz/openbmc_project/State/Boot/Raw.interface.yaml)
property on `xyz.openbmc_project.State.Boot.Raw` interface & archive them per
boot on the filesystem, so that those can be exposed over
[redfish](https://github.com/openbmc/docs/blob/master/designs/redfish-postcodes.md)

## Special Post code handling/eventing

Platforms can provide custom configuration to allow for special handling of
specific postcodes. Special handling include starting user configured systemd
unit files and/or creating a structured event as defined by the user. The JSON
configuration file can be passed in using the `--config PATH` or `-c PATH`
options.

Configuration format:

```json
[
  {
    "primary": [123],
    "secondary": [234, 123],
    "targets": ["my_special.service"]
  },
  {
    "primary": [999],
    "targets": ["power_failure.service"],
    "event": {
      "name": "xyz.openbmc_project.State.Power.PowerRailFault",
      "arguments": {
        "POWER_RAIL": "MyDevice",
        "FAILURE_DATA": ""
      }
    }
  }
]
```

Each entry in the array describes a special handler for a specific post code.

- `primary` - [required] The primary post code to match.
- `secondary` - [optional] The secondary post code to match. If not present, the
  match will be for all post codes which match just the primary
- `targets` - [optional] List of systemd targets to start when the matching post
  code is received.
- `event` - [optional] The descriptor of the event to create with
  phosphor-logging.
  - `event::name` - Name of the event log. See `log-create --list` for a list of
    possible events which can be created
  - `event::arguments` - The named argument list which will be used to create
    the event.

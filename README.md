# SedonaPro Runtime

Portable Sedona-compatible runtime, reusable control kits, system configurators,
SDK tooling, simulation and a browser-based visual editor for open automation.

Sedona can be ported to many controller platforms with sufficient resources and
a platform adaptation layer. Linux and ESP32 are current development targets,
rather than the limits of the platform.

## Project

The shared runtime, compiler, kits and SDK tooling. This is the compatibility reference for the platform-specific projects.

**Audience:** Linux hosts, SDK developers and kit authors.

## Modernizing the runtime

A major part of this work is bringing the runtime to **64-bit Linux (amd64)**
and fixing issues exposed by the port. Changes include reference-size-aware
field layout, null-pointer initialization, VM stack handling across native
calls, UDP structure layout and pointer-safe native interfaces, including
Modbus handles. The build tooling has also been updated for Python 3.

SedonaPro is therefore more than a collection of board ports: runtime
correctness, compiler compatibility and reliable native integration are central
to the project. These changes are backed by existing runtime and integration
tests; publication builds will document their exact verification results.

## Protocol and communication improvements

We have also improved the communication stack and the way engineering tools
use it, with a focus on reliable transfers, live sessions and portable clients.

| Area | Improvements in our development lines |
| --- | --- |
| SOX file transfers | Fixed a race between the receiver and caller threads in the Java client's chunk queue, which could corrupt the queue, lose chunks and cause transfer timeouts |
| Read recovery in Linux Studio | Releases an abandoned file transfer before retrying a failed read, with at most three attempts; device writes are not automatically retried |
| Larger application schemas | Our Studio and web clients request a 1024-byte DASP packet limit for the Pro profile; older clients must be checked for compatibility |
| Device-provided resources | SOX access to kit manifests and widget resources lets supporting editors obtain the metadata and views associated with the installed kit builds |
| Web live sessions | Fixed a watch-renewal race during configurator rebuilds: components removed by a rebuild are excluded from the watch list instead of ending the live session |
| Operator-screen transfer | The web gateway writes the screen package as a separate device file and checks its bytes by reading it back; concurrent file transfer and device mutations are coordinated |
| MQTT | A non-blocking MQTT 3.1.1 client polled by the VM, with QoS 0/1, retained messages, last will, keep-alive, reconnect backoff and subscription restoration; shared implementation for Linux, simulation and ESP-IDF |

These include protocol-client fixes and editor/gateway behaviour, not a claim
that every feature changes the SOX/DASP wire format. Support differs by editor,
SDK and firmware version. Existing source tests cover relevant scenarios;
release-specific results will accompany verified builds.

## Web editor and an emerging SCADA environment

The **web editor** brings visual Sedona engineering to the browser: navigate
applications, edit function-block diagrams and properties, inspect live values,
and create graphical views through a Java server and React interface. Its source
currently lives in `sedonapro/editor/`.

Beyond diagram engineering, we have built much of an operator environment:
graphical screens, reusable equipment symbols and components, live bindings,
operator commands and trends. Linux Studio also has an optional SCADA module
with alarm handling, acknowledgement, an event journal and stored value history.
Features differ between the web, Linux and Windows editions.

**Almost a SCADA:** a substantial foundation is already implemented, while
standalone station operation without the editor and user/role management remain
unfinished. We describe the available pieces explicitly rather than presenting
this as a complete SCADA product.

## Control kit library and system configurators

The Pro kit library is a central part of SedonaPro. It combines individual
control components with configurable assemblies that build a system's internal
blocks, I/O points and links from selected equipment options.

| Configurator | Kit | Configuration and generated logic |
| --- | --- | --- |
| Air handling unit | `sproAhu` | Supply or supply/exhaust; water or electric heating; cooling, heat recovery, fan control, sensors and interlocks |
| Pump station | `sproPumpSt` | 1–4 pumps; on/off, common or individual VFDs; PID, staging, duty/standby rotation, dry-running protection and optional feedback |
| Heating substation | `sproHeatSt` | Up to two heating circuits, outdoor-reset curves, single or twin pumps, optional domestic hot water, circulation, make-up and common alarm |

**How it works:** add the configurator, select the equipment and options, then
invoke **Build**. The assembly creates the required components and connections.
Where supported, its kit-supplied widget offers setup and operation views.
Compatible SDK, kit manifests and editor support are required; support must be
verified separately for each editor edition.

A rebuild retains existing blocks of the same type and their settings, adds
required parts and removes obsolete generated parts. Custom logic belongs next
to the generated assembly. Automatic channel assignment reports the I/O list
and points left without a board channel; it does not replace electrical design
or commissioning.

The underlying library includes signal conditioning, PID and sequencing,
equipment control, alarms, schedules, HVAC, heating, energy functions and plant
simulation. Domain packs (`control`, `pumps`, `hvac`, `heat`, `energy`) select
related kits for a build. The library currently lives in the **sedonapro** source
project; it is distinct from graphical screen-symbol libraries.

## Library source and examples

The source tree contains `kits/`, the component catalog
`docs/kits.md`, engineering conventions in `docs/pro-kits.md`, and domain packs
in `profiles/packs/`. Each configurator has a kit README and source tests.
Examples include `examples/pump-station.xml` and `examples/heat-station.xml`.
Build and test commands are documented in the quick start below.

## Get started

This repository contains a source preview, not a production-certified release.
Start on a local simulator or development host.

Requirements: Linux, Python 3.11+, a JDK and GCC. Full Linux builds also need
`libmodbus-dev` and `libsqlite3-dev`. The web editor requires Node 20.19+ and JDK 17+.

```sh
git clone https://github.com/SedonaPro/sedonapro.git
cd sedonapro
./spro kits
./spro build linux
./spro test linux --quick
./spro run linux
```

See [Quick start](docs/quickstart_EN.md), [runtime and SDK guide](README_EN.md),
[kit catalog](docs/kits.md), [widgets](docs/widgets.md) and
[screen formats](docs/screens.md). Russian documentation is retained beside it.

The documented development login is for local experiments. Configure your own
credentials for other environments. Board-specific hardware checks are separate
from simulator and host-runtime results.

## Where we want to take SedonaPro

Our ambition is a broader automation ecosystem: more controller platforms,
more ways to connect equipment, a growing control and simulation kit library,
and **SedonaPro SCADA** as a dedicated operator environment.

These are development directions, not announcements of completed drivers or
committed delivery dates. Scope and priorities will evolve with contributors,
available hardware and real application needs. The protocols and platforms listed
below are examples, not a closed list: further integrations and kit domains are
welcome as the community grows.

| Direction | Planned work |
| --- | --- |
| More controllers | Port the runtime and HAL to additional embedded and industrial platforms; document board support, toolchains and tested I/O |
| Building-automation networks | Explore BACnet/IP, BACnet MS/TP, N2 Open, KNX and other protocols, including the required transports, hardware interfaces and interoperability tests |
| REST and other drivers | Develop HTTP/REST integrations, gateways and other protocol and equipment drivers around clear data models and documented interfaces |
| SedonaPro SCADA | Grow the existing screens, live bindings, commands, alarms, trends and history into a dedicated operator environment, including standalone station operation and user/role management |
| More control kits | Expand reusable libraries for HVAC, heating, pumps, energy, equipment, sequencing and system configurators |
| Object and plant models | Add models of physical processes, equipment, sensors and actuators, with configurable dynamics, disturbances and fault scenarios |
| Offline engineering | Develop and debug control logic without physical equipment: local simulation, accelerated model time, repeatable scenarios, regression checks and clear diagnostics |
| Shared engineering resources | Grow example applications, graphical libraries, documentation, translations and reusable test cases across tools |

A useful contribution can be a driver prototype, a board port, a validated
plant model or a well-documented offline test scenario. Bring ideas to
[community Discussions](https://github.com/SedonaPro/.github/discussions/categories/ideas)
so we can agree on interfaces and avoid duplicating work.

## Community

- [SedonaPro](https://github.com/SedonaPro)
- [Questions, ideas and show-and-tell](https://github.com/SedonaPro/.github/discussions)
- [Contributing](CONTRIBUTING.md) and [support](SUPPORT.md)

English is the reference language; translations are welcome.

## Licensing and attribution

Core code is under AFL-3.0; SedonaPro kits and tools use Apache-2.0 as documented
in [NOTICE](NOTICE). Third-party components retain their individual licences;
see [LICENSES](LICENSES).
SedonaPro is an independent project, not affiliated with or endorsed by Tridium.

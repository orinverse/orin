Orin Core
==========

This is the official reference wallet for Orin digital currency and comprises the backbone of the Orin peer-to-peer network. You can [download Orin Core](https://www.orin.org/downloads/) or [build it yourself](#building) using the guides below.

Running
---------------------
The following are some helpful notes on how to run Orin Core on your native platform.

### Unix

Unpack the files into a directory and run:

- `bin/orin-qt` (GUI) or
- `bin/orind` (headless)

### Windows

Unpack the files into a directory, and then run orin-qt.exe.

### macOS

Drag Orin Core to your applications folder, and then run Orin Core.

### Need Help?

* See the [Orin documentation](https://docs.orin.cash)
for help and more information.
* Ask for help on [Orin Discord](http://stayoriny.com)
* Ask for help on the [Orin Forum](https://orin.cash/forum)

Building
---------------------
The following are developer notes on how to build Orin Core on your native platform. They are not complete guides, but include notes on the necessary libraries, compile flags, etc.

- [Dependencies](dependencies.md)
- [macOS Build Notes](build-osx.md)
- [Unix Build Notes](build-unix.md)
- [Windows Build Notes](build-windows.md)
- [OpenBSD Build Notes](build-openbsd.md)
- [NetBSD Build Notes](build-netbsd.md)
- [Android Build Notes](build-android.md)

Development
---------------------
The Orin Core repo's [root README](/README.md) contains relevant information on the development process and automated testing.

- [Developer Notes](developer-notes.md)
- [Productivity Notes](productivity.md)
- [Release Notes](release-notes.md)
- [Release Process](release-process.md)
- Source Code Documentation ***TODO***
- [Translation Process](translation_process.md)
- [Translation Strings Policy](translation_strings_policy.md)
- [JSON-RPC Interface](JSON-RPC-interface.md)
- [Unauthenticated REST Interface](REST-interface.md)
- [Shared Libraries](shared-libraries.md)
- [BIPS](bips.md)
- [Dnsseed Policy](dnsseed-policy.md)
- [Benchmarking](benchmarking.md)
- [Internal Design Docs](design/)

### Resources
* See the [Orin Developer Documentation](https://orincore.readme.io/)
  for technical specifications and implementation details.
* Discuss on the [Orin Forum](https://orin.cash/forum), in the Development & Technical Discussion board.
* Discuss on [Orin Discord](http://stayoriny.com)
* Discuss on [Orin Developers Discord](http://chat.orindevs.org/)

### Miscellaneous
- [Assets Attribution](assets-attribution.md)
- [orin.conf Configuration File](orin-conf.md)
- [CJDNS Support](cjdns.md)
- [Files](files.md)
- [Fuzz-testing](fuzzing.md)
- [I2P Support](i2p.md)
- [Init Scripts (systemd/upstart/openrc)](init.md)
- [Managing Wallets](managing-wallets.md)
- [Multisig Tutorial](multisig-tutorial.md)
- [P2P bad ports definition and list](p2p-bad-ports.md)
- [PSBT support](psbt.md)
- [Reduce Memory](reduce-memory.md)
- [Reduce Traffic](reduce-traffic.md)
- [Tor Support](tor.md)
- [Transaction Relay Policy](policy/README.md)
- [ZMQ](zmq.md)

License
---------------------
Distributed under the [MIT software license](/COPYING).

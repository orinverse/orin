<p align="center">
  <img src="222.png" alt="Orin logo" width="220">
</p>

Orin Core staging tree
===========================

| `master` | `develop` |
| -------- | --------- |
| [![Build Status](https://github.com/orinverse/orin/actions/workflows/build.yml/badge.svg?branch=master)](https://github.com/orinverse/orin/tree/master) | [![Build Status](https://github.com/orinverse/orin/actions/workflows/build.yml/badge.svg?branch=develop)](https://github.com/orinverse/orin/tree/develop) |

Temporary website: https://welcome.orin.cash/  
Official website: https://orin.cash  
Block explorer: https://explorer.orin.cash  
Future ecosystem: https://orinverse.cash  
Discord: https://discord.gg/wewJuG6D  
Wallets & releases: https://github.com/orinverse/orin/releases  

For an immediately usable, binary version of the Orin Core software, see  
https://github.com/orinverse/orin/releases  

Orin Core connects to the Orin peer-to-peer network to download and fully  
validate blocks and transactions. It also includes a wallet and graphical user  
interface, which can be optionally built.

Further information about Orin Core is available in the [doc folder](/doc).

What is Orin?
-------------

Orin is a digital currency that enables instant, private payments to anyone,  
anywhere in the world. Orin uses peer-to-peer technology to operate with  
no central authority: managing transactions and issuing money are carried out  
collectively by the network. Orin Core is the name of the open  
source software which enables the use of this currency.

For more information read the original Orin whitepaper.

License
-------

Orin Core is released under the terms of the MIT license. See [COPYING](COPYING) for more  
information or see https://opensource.org/licenses/MIT.

Development Process
-------------------

The `master` branch is meant to be stable. Development is normally done in separate branches.  
[Tags](https://github.com/orinverse/orin/tags) are created to indicate new official,  
stable release versions of Orin Core.

The `develop` branch is regularly built (see doc/build-*.md for instructions) and tested, but is not guaranteed to be  
completely stable.

The contribution workflow is described in [CONTRIBUTING.md](CONTRIBUTING.md)  
and useful hints for developers can be found in [doc/developer-notes.md](doc/developer-notes.md).

Build / Compile from Source
---------------------------

The `./configure`, `make`, and `cmake` steps, as well as build dependencies, are in [./doc/](/doc) as well:

- **Linux**: [./doc/build-unix.md](/doc/build-unix.md) \
  Ubuntu, Debian, Fedora, Arch, and others
- **macOS**: [./doc/build-osx.md](/doc/build-osx.md)
- **Windows**: [./doc/build-windows.md](/doc/build-windows.md)
- **OpenBSD**: [./doc/build-openbsd.md](/doc/build-openbsd.md)
- **FreeBSD**: [./doc/build-freebsd.md](/doc/build-freebsd.md)
- **NetBSD**: [./doc/build-netbsd.md](/doc/build-netbsd.md)

Testing
-------

Testing and code review is the bottleneck for development; we get more pull  
requests than we can review and test on short notice. Please be patient and help out by testing  
other people's pull requests, and remember this is a security-critical project where any mistake might cost people  
lots of money.

### Automated Testing

Developers are strongly encouraged to write [unit tests](src/test/README.md) for new code, and to  
submit new unit tests for old code. Unit tests can be compiled and run  
(assuming they weren't disabled in configure) with: `make check`. Further details on running  
and extending unit tests can be found in [/src/test/README.md](/src/test/README.md).

There are also [regression and integration tests](/test), written  
in Python.  
These tests can be run (if the [test dependencies](/test) are installed) with: `test/functional/test_runner.py`

The CI (Continuous Integration) systems make sure that every pull request is built for Windows, Linux, and macOS,  
and that unit/sanity tests are run automatically.

### Manual Quality Assurance (QA) Testing

Changes should be tested by somebody other than the developer who wrote the  
code. This is especially important for large or high-risk changes. It is useful  
to add a test plan to the pull request description if testing the changes is  
not straightforward.

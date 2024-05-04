# Building with the framework {#compilation}

[TOC]

This page will guide the user in building benchmarks with RT-Bench.

## Dependencies

In the current implementation, the framework has some
dependencies the user has to be aware of:

### System dependencies {#sys-deps}

These features have to be present in the platform that will execute the
benchmarks.

- POSIX.4 real-time signals: used to execute the benchmark periodically and to
  gather stats.
- Linux scheduler syscalls: Used to change the scheduling policy.

### Core dependencies {#core-deps}

These dependencies are needed for compiling and executing the benchmarks on the
target machine.

- A shell that can run scripts in Bash >= 5.
- Glibc >= 2.34: Provides primitives used by the memory watcher and the argument
  parser. (Older versions might create problems with the makefile scaffolding).
  Its static version is optionally needed for static compilation.
- argp.h: Used for cli argument parsing (some distributions do not include it by
  default).
- Gcc >= 9.5.0: The compiler we have chosen.
- ldd >= 2.34: Should be bundled with glibc, optionally the standalone binary is
  needed also [StaticX](#staticx) compilation.
- git >= 2.37.
- perl >= 5.34.1. Needed for SD-VBS to find the suite root directory.

### Optional dependencies

Dependencies that are tied to specific benchmarks or some optional features that
can be disabled.

- git LFS >= 2.13: _Optional._ For the [Image Filters](https://rt-bench.gitlab.io/rt-bench/group__image-filters.html) module.
- Linux Perf: _Optional._ Used to read performance counters (currently only on intel and CORTEX A53)
- [JSON-C](https://github.com/json-c/json-c) >= 0.15: _Optional._ Used to read and parse input JSON configuration files.
- [imagemagick](https://imagemagick.org/) >= 7.1.0-45 _Optional._ For the [Image
  Filters](https://rt-bench.gitlab.io/rt-bench/group__image-filters.html)
  module.
- binutils >= 2.38 _Optional._ (readelf and objcopy) For [StaticX](#staticx)
  compilation.
- [patchelf](https://github.com/NixOS/patchelf) >= 0.14.5 _Optional._ For
  [StaticX](#staticx) compilation.
- Python 3.10+ with pip and venv _Optional._ For the utils scripts and
  [StaticX](#staticx) compilation.

Currently, RT-Bench targets the following platforms:

- x86/x86_64
- ARM64

### Dependencies installation

  The `json-c` dependence can be installed with the following command:
- Ubuntu/Debian:

```{.sh}
sudo apt install libjson-c5 libjson-c-dev
```

- Fedora / OpenSuse:

```{.sh}
sudo dnf install json-c json-c-devel
```

- Arch Linux:

```{.sh}
sudo pacman -S json-c
```

#### Nix users

For [Nix](https://nixos.org/) users, a flake and a [direnv](https://direnv.net/)
environment are available to make sure that all the dependencies are satisfied.
The flake provides two development shells:
- A default one, accessed by [direnv](https://direnv.net/) and by `nix develop
  .#` in the repo root with all dependencies satisfied for x86_64 systems.
- An aarch64 cross compilation shells, accessed with `nix develop .#aarch64` in
  the repo root, that provides dependencies and `aarch64-unknown-linux-gnu-gcc`
  cross-compiler.

## Compiling RT-Bench

Compiling a RT-Bench compliant benchmark (see [benchmark structure](3-Extending_rt-bench.markdown)) with the framework using the provided
`Makefile` structure is easy and the best way to benefit from all the features
offered by RT-Bench.

The `Makefile` provided in [Isolbench](@ref IsolBench) is a good example of how to use the provided makefile interface/variables.

Typically, once `generator/rtbench.mk` is included, five different variables are accessible:

- `rtbench`: recipe to initialize and build the RT-Bench core components for the desired target
- `CC`: user-specified compiler for the desired target
- `CFLAGS`: compilation flags. Automatically set by the `generator/rtbench.mk`, can be complemented with `override`
- `BASE_O`: set of object files for the RT-Bench core components
- `LDFLAGS`: linker flags. Automatically set by the `generator/rtbench.mk`, can be complemented with `override`
- `STATICX_REQ`: Requirements to have [StaticX](#staticx) on path, undefined	when the feature is disabled.
- `STATICX_CMD`: Command line for packing the executable with [StaticX](#staticx) replaced by a printf when the feature is disabled.

Using these variables and recipes, we recommend to write recipes for compiling your benchmark with the following template:

```{.mk}
<benchmark>: rtbench
	$(CC) $(CFLAGS) <benchmark>.c $(BASE_O) -o <benchmark> $(LDFLAGS)
	$(STATICX_REQ)
	$(STATICX_CMD) <benchmark> <benchmark>.sx
```

## Optional RT-Bench specific options

In addition, RT-Bench supports dedicated flags that enable access to further
features. These features are not part of the default set of features as they
depend on the benchmark nature itself or on the platform on which the benchmarks
will be deployed.

### Using the Makefile scaffolding to toggle optional features

For each of the below features, there is a matching variable with can
force-toggle the feature on or off, (consider as an example the JSON parser
feature, it can be manually controlled by setting `FEAT_JSON=y` or
`FEAT_JSON=n`). More details on these variables are in the corresponding feature section.

Additionally, these variables can be stored in a `options.mk` makefile in the root
of the repository to avoid having to input them manually each time. An example
of the `options.mk` is provided below:

```
#Path: rt-bench/options.mk
#This makefile can be used to explicitly toggle RT-bench optional features
# controlled by makefile variables. Refer to the documentation for more details.

#Example: disable SCHED_DEADLINE support
FEAT_SCHED_DEADLINE=n
#Example: enable json parser support
FEAT_JSON=y
```

### Extended Reporting (Benchmark Specific Measurement Reporting)

Some benchmark classes (e.g., synthetic workloads) measure specific impacts on
the platform. RT-Bench offers the possibility to extend the existing `.csv`
report interface to include the desired _benchmark-specific_ measurement.
Providing the benchmarks follow the rules mentioned in the
[benchmark](3-Extending_rt-bench.markdown)[
structure](3-Extending_rt-bench.markdown), extended reporting can be enabled by
adding the `-DEXTENDED_REPORT` flag in the compilation command line.

This feature be controlled in a limited fashion by the Makefile scaffolding since it's
benchmark-specific.

### JSON configuration files support {#json_support}

To disable parsing of JSON files, which requires the
[JSON-C](https://github.com/json-c/json-c) to be at least at version 0.15.
RT-Bench tries to detect automatically if the library with the correct version
is present and decides at compilation time it the support for this feature has
to be enabled or not.

This feature can be enabled or disabled on the fly while issuing a
make command by defining the `FEAT_JSON=y` or `FEAT_JSON=n` variable or adding
the `-DFEAT_JSON_SUPPORT=1` (enable) or `-DFEAT_JSON_SUPPORT=0` (disable) flag
in the compilation command line.

#### Deadline scheduler support {#sched_deadline_support}

On some systems, a deadline-aware scheduler might not be available. RT-Bench
tries to detect automatically at compilation time if the support for this
feature has to be enabled or not by checking if the macro `SCHED_DEADLINE` is
defined.

This feature can be enabled or disabled by defining the
`FEAT_SCHED_DEADLINE=y` or `FEAT_SCHED_DEADLINE=n` variable while issuing a make
command. The same behavior can be achieved by, adding the
`-DFEAT_SCHED_DEADLINE_SUPPORT=1` (enable) or `-DFEAT_SCHED_DEADLINE_SUPPORT=0`
(disable) flag in the compilation command line.

### Performance counters and monitoring thread {#perf_support}

RT-Bench supports monitoring the L1/L2 cache reference, refills, instruction
retired and CPU clock cycles, however, this feature is CPU specific, requiring
user intervention for it to work properly in most cases.

The feature can be enabled or disabled by using the `FEAT_PERF=y` or
`FEAT_PERF=n` variable while issuing a make command. The same behavior can be
achieved by, adding the `-DFEAT_PERF_SUPPORT=1` (enable) or
`-DFEAT_PERF_SUPPORT=0` (disable) flag in the compilation command
line.

With this set of features being specific to the core and platform on which the
benchmark will be deployed, two parameters must be added in other to enable
them: the ISA and the core model. The table below lists the flags to add and
provides examples of compliant/tested platforms and CPU models.

Additionally, there are equivalent Make variables that will enable the
corresponding parameters when issuing a `make` command.

|       ISA       |      CORE      |   Make Variable   |        Platform/CPU Model        |
| :-------------: | :------------: | :---------------: | :------------------------------: |
| `-D__aarch64__` | `-DCORTEX_A53` | `CORE=CORTEX_A53` | Xilinx ZCU102 / Raspberry Pi 3B+ |
| `-D__x86_64__`  |  `-DCORE_I7`   |  `CORE=CORE_I7`   |       Intel Core i7-8550U        |

#### Reporting of skipped deadlines

Reporting of skipped deadlines is enabled by default. Each time a job does not
meet its deadline, the unmet deadline will be reported as an execution where
most of the parameters are `0`, except the timestamps. This behavior can be
enabled or disabled by using the `FEAT_PRINT_SKIPPED_DEADLINE=y` or
`FEAT_PRINT_SKIPPED_DEADLINE=n` variable while issuing a make command. The same
behavior can be achieved by, adding the
`-DFEAT_PRINT_SKIPPED_DEADLINE_SUPPORT=1` (enable) or
`-DFEAT_PRINT_SKIPPED_DEADLINE_SUPPORT=0` (disable) flag in the compilation
command line.


#### Static Copilation

For systems that cannot met the [core dependencies](#core-deps) due to version constraints
but can meet the [system dependencies](#sys-deps) RT-Bench provides two optional ways to get
have the benchmarks running.

##### With GCC

The user can enable static compilation with GCC (which requires the static
version of the libraries to be installed) with the `FEAT_GCC_STATIC=y` variable
while issuing a make command. Conversely using `FEAT_GCC_STATIC=n` will make
sure that the feature stays disabled.

##### With StaticX {#staticx}

If only the dynamic version of the needed libraries is present, the user can
still enable a "semi-static" compilation by invoking
[StaticX](https://staticx.readthedocs.io/en/latest/introduction.html), which
will pack in the target executable the needed libraries and unpack them in a
temporary location when the executable is being run.

This feature can be enabled or force-disabled with the make variable
`FEAT_STATICX=y` or `FEAT_STATICX=n`.

Executables packed with staticx will have the `.sx` extension.

@author Mattia Nicolella, Denis Hoornaert
@copyright (C) 2021 - 2022, Denis Hoornaert <denis.hoornaert@tum.de>, Mattia Nicolella <mnico@bu.edu> and the rt-bench contributors.
SPDX-License-Identifier: MIT

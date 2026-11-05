#pragma once

#include "HeadlessArgs.h"

// Headless (no window) export of assets from a running game, driven by the command line.
// See HeadlessArgs.h for the options and exit codes, README.md for usage.
namespace HeadlessExport
{
    // Runs the whole job and returns a HeadlessExitCode. Initializes/shuts down the WraithX API itself.
    int Run(const HeadlessOptions& Options);
}

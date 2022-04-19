// Copyright (c) Microsoft Corporation
// SPDX-License-Identifier: MIT

#define CATCH_CONFIG_MAIN

#include <cstdarg>
#include <stdio.h>
#include <string>
#include "capture_helper.hpp"
#include "catch_wrapper.hpp"
#include "ebpf_api.h"

// Run a given command and return the output and exit code.
std::string
run_command(_In_ PCSTR command_line, _Out_ int* result)
{
    printf("Running command: %s\n\n", command_line);

    capture_helper_t capture;
    errno_t error = capture.begin_capture();
    if (error != NO_ERROR) {
        *result = error;
        return "Couldn't capture output\n";
    }

    *result = system(command_line);

    std::string stderr_contents = capture.get_stderr_contents();
    std::string stdout_contents = capture.get_stdout_contents();

    return stdout_contents + stderr_contents;
}

TEST_CASE("help", "[help]")
{
    int result;
    std::string output = run_command("bpftool help", &result);
    REQUIRE(
        output == "Usage: bpftool [OPTIONS] OBJECT { COMMAND | help }\n"
                  "       bpftool batch file FILE\n"
                  "       bpftool version\n"
                  "\n"
                  "       OBJECT := { prog | map | link | net }\n"
                  "       OPTIONS := { {-j|--json} [{-p|--pretty}] | {-d|--debug} | {-l|--legacy} |\n"
                  "                    {-V|--version} }\n");
    REQUIRE(result == 0);
}

TEST_CASE("prog help", "[prog][help]")
{
    int result;
    std::string output = run_command("bpftool prog help", &result);
    REQUIRE(
        output == "Usage: bpftool prog { show | list } [PROG]\n"
                  "       bpftool prog pin   PROG FILE\n"
                  "       bpftool prog { load | loadall } OBJ  PATH \\\n"
                  "                         [type TYPE] [dev NAME] \\\n"
                  "                         [map { idx IDX | name NAME } MAP]\\\n"
                  "                         [pinmaps MAP_DIR]\n"
                  "       bpftool prog help\n"
                  "\n"
                  "       MAP := { id MAP_ID | pinned FILE | name MAP_NAME }\n"
                  "       PROG := { id PROG_ID | pinned FILE | name PROG_NAME }\n"
                  "       TYPE := { bind | xdp }\n"
                  "       OPTIONS := { {-j|--json} [{-p|--pretty}] | {-d|--debug} | {-l|--legacy} |\n"
                  "                    {-m|--mapcompat} | \n");
    REQUIRE(result == 0);
}

TEST_CASE("prog load map_in_map.o", "[prog][load]")
{
    int result;
    std::string output;

    output = run_command("bpftool prog show", &result);
    REQUIRE(output == "");
    REQUIRE(result == 0);

    output = run_command("bpftool prog load map_in_map.o map_in_map", &result);
    REQUIRE(output == "");
    REQUIRE(result == 0);

    output = run_command("bpftool prog show", &result);
    REQUIRE(result == 0);
    std::string id = std::to_string(atoi(output.c_str()));
    REQUIRE(output == id + ": xdp  name lookup  \n\n");

    output = run_command("netsh ebpf sh prog", &result);
    REQUIRE(
        output == "\n\n"
                  "    ID  Pins  Links  Mode       Type           Name\n"
                  "======  ====  =====  =========  =============  ====================\n" +
                      id + "     1      0  JIT        xdp            lookup\n");
    REQUIRE(result == 0);

    // Netsh currently outputs a spurious "Program not found" after the delete.
    output = run_command(("netsh ebpf delete prog " + id).c_str(), &result);
    REQUIRE(
        output == "\nUnpinned " + id +
                      " from map_in_map\n"
                      "Program not found\n");
    REQUIRE(result == 1);

    output = run_command("bpftool prog show", &result);
    REQUIRE(output == "");
    REQUIRE(result == 0);
}

TEST_CASE("map create", "[map]")
{
    int status;
    std::string output =
        run_command("bpftool map create FileName type array key 4 value 4 entries 2 name Name", &status);
    REQUIRE(output == "");
    REQUIRE(status == 0);

    output = run_command("bpftool map show", &status);
    REQUIRE(status == 0);
    std::string id = std::to_string(atoi(output.c_str()));
    REQUIRE(output == id + ": array  name Name  flags 0x0\n\tkey 4B  value 4B  max_entries 2\n");

    output = run_command("netsh ebpf show maps", &status);
    REQUIRE(status == 0);
    REQUIRE(
        output == "\n\n"
                  "                             Key  Value      Max  Inner\n"
                  "    ID            Map Type  Size   Size  Entries     ID  Pins  Name\n"
                  "======  ==================  ====  =====  =======  =====  ====  ========\n"
                  " " +
                      id + "               Array     4      4        2     -1     1  Name\n");

    output = run_command(("bpftool map dump id " + id).c_str(), &status);
    REQUIRE(
        output == "key: 00 00 00 00  value: 00 00 00 00\n"
                  "key: 01 00 00 00  value: 00 00 00 00\n"
                  "Found 2 elements\n");
    REQUIRE(status == 0);

    ebpf_result_t result = ebpf_object_unpin("FileName");
    REQUIRE(result == EBPF_SUCCESS);
}

TEST_CASE("prog show id 1", "[prog][show]")
{
    int result;
    std::string output = run_command("bpftool prog show id 1", &result);
    REQUIRE(output == "Error: get by id (1): No such file or directory\n");
    REQUIRE(result == -1);
}
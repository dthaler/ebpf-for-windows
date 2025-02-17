// Copyright (c) eBPF for Windows contributors
// SPDX-License-Identifier: MIT

#include "bpf/libbpf.h"
#include "elf.h"
#include "tokens.h"
#include "utilities.h"

#include <cassert>
#include <iomanip>
#include <locale>

TOKEN_VALUE g_LevelEnum[3] = {
    {L"normal", VL_NORMAL},
    {L"informational", VL_INFORMATIONAL},
    {L"verbose", VL_VERBOSE},
};

// The following function uses windows specific type as an input to match
// definition of "FN_HANDLE_CMD" in public file of NetSh.h
unsigned long
handle_ebpf_show_disassembly(
    IN LPCWSTR machine,
    _Inout_updates_(argc) LPWSTR* argv,
    IN DWORD current_index,
    IN DWORD argc,
    IN DWORD flags,
    IN LPCVOID data,
    OUT BOOL* done)
{
    UNREFERENCED_PARAMETER(machine);
    UNREFERENCED_PARAMETER(flags);
    UNREFERENCED_PARAMETER(data);
    UNREFERENCED_PARAMETER(done);

    TAG_TYPE tags[] = {
        {TOKEN_FILENAME, NS_REQ_PRESENT, FALSE},
        {TOKEN_SECTION, NS_REQ_ZERO, FALSE},
        {TOKEN_PROGRAM, NS_REQ_ZERO, FALSE},
    };
    unsigned long tag_type[_countof(tags)] = {0};

    unsigned long status =
        PreprocessCommand(nullptr, argv, current_index, argc, tags, _countof(tags), 0, _countof(tags), tag_type);

    std::string filename;
    std::string section{}; // Use the first code section by default.
    std::string program{}; // Use the first program by default.
    for (int i = 0; (status == NO_ERROR) && ((i + current_index) < argc); i++) {
        switch (tag_type[i]) {
        case 0: // FILENAME
        {
            filename = down_cast_from_wstring(std::wstring(argv[current_index + i]));
            break;
        }
        case 1: // SECTION
        {
            section = down_cast_from_wstring(std::wstring(argv[current_index + i]));
            break;
        }
        case 2: // PROGRAM
        {
            program = down_cast_from_wstring(std::wstring(argv[current_index + i]));
            break;
        }
        default:
            status = ERROR_INVALID_SYNTAX;
            break;
        }
    }
    if (status != NO_ERROR) {
        return status;
    }

    const char* disassembly = nullptr;
    const char* error_message = nullptr;
    if (ebpf_api_elf_disassemble_program(
            filename.c_str(),
            !section.empty() ? section.c_str() : nullptr,
            !program.empty() ? program.c_str() : nullptr,
            &disassembly,
            &error_message) != 0) {
        if (error_message != nullptr) {
            std::cerr << error_message << std::endl;
        }
        ebpf_free_string(error_message);
        return ERROR_SUPPRESS_OUTPUT;
    } else {
        std::cout << disassembly << std::endl;
        ebpf_free_string(disassembly);
        return NO_ERROR;
    }
}

static PCSTR
_get_map_type_name(ebpf_map_type_t type)
{
    int index = (type >= _countof(_ebpf_map_display_names)) ? 0 : type;
    return _ebpf_map_display_names[index];
}

// The following function uses windows specific type as an input to match
// definition of "FN_HANDLE_CMD" in public file of NetSh.h
unsigned long
handle_ebpf_show_sections(
    IN LPCWSTR machine,
    _Inout_updates_(argc) LPWSTR* argv,
    IN DWORD current_index,
    IN DWORD argc,
    IN DWORD flags,
    IN LPCVOID data,
    OUT BOOL* done)
{
    UNREFERENCED_PARAMETER(machine);
    UNREFERENCED_PARAMETER(flags);
    UNREFERENCED_PARAMETER(data);
    UNREFERENCED_PARAMETER(done);

    TAG_TYPE tags[] = {
        {TOKEN_FILENAME, NS_REQ_PRESENT, FALSE},
        {TOKEN_SECTION, NS_REQ_ZERO, FALSE},
        {TOKEN_LEVEL, NS_REQ_ZERO, FALSE},
    };
    unsigned long tag_type[_countof(tags)] = {0};

    unsigned long status =
        PreprocessCommand(nullptr, argv, current_index, argc, tags, _countof(tags), 0, _countof(tags), tag_type);

    VERBOSITY_LEVEL level = VL_NORMAL;
    std::string filename;
    std::string section;
    for (int i = 0; (status == NO_ERROR) && ((i + current_index) < argc); i++) {
        switch (tag_type[i]) {
        case 0: // FILENAME
        {
            filename = down_cast_from_wstring(std::wstring(argv[current_index + i]));
            break;
        }
        case 1: // SECTION
        {
            section = down_cast_from_wstring(std::wstring(argv[current_index + i]));
            break;
        }
        case 2: // LEVEL
            status =
                MatchEnumTag(NULL, argv[current_index + i], _countof(g_LevelEnum), g_LevelEnum, (unsigned long*)&level);
            if (status != NO_ERROR) {
                status = ERROR_INVALID_PARAMETER;
            }
            break;

        default:
            status = ERROR_INVALID_SYNTAX;
            break;
        }
    }

    if (status != NO_ERROR) {
        return status;
    }

    // If the user specified a section and no level, default to verbose.
    if (tags[1].bPresent && !tags[2].bPresent) {
        level = VL_VERBOSE;
    }

    ebpf_api_program_info_t* program_data = nullptr;
    const char* error_message = nullptr;
    if (ebpf_enumerate_programs(filename.c_str(), level == VL_VERBOSE, &program_data, &error_message) != 0) {
        if (error_message != nullptr) {
            std::cerr << error_message << std::endl;
        }
        ebpf_free_string(error_message);
        ebpf_free_programs(program_data);
        return ERROR_SUPPRESS_OUTPUT;
    }

    if (level == VL_NORMAL) {
        std::cout << "\n";
        std::cout << "                                                            Size\n";
        std::cout << "             Section                 Program       Type  (bytes)\n";
        std::cout << "====================  ======================  =========  =======\n";
    }
    for (auto current_program = program_data; current_program != nullptr; current_program = current_program->next) {
        if (!section.empty() && strcmp(current_program->section_name, section.c_str()) != 0) {
            continue;
        }
        auto program_type_name = ebpf_get_program_type_name(&current_program->program_type);
        if (program_type_name == nullptr) {
            program_type_name = "unspec";
        }
        if (level == VL_NORMAL) {
            std::cout << std::setw(20) << std::right << current_program->section_name << "  " << std::setw(22)
                      << std::right << current_program->program_name << "  " << std::setw(9) << program_type_name
                      << "  " << std::setw(7) << current_program->raw_data_size << "\n";
        } else {
            std::cout << "\n";
            std::cout << "Section      : " << current_program->section_name << "\n";
            std::cout << "Program      : " << current_program->program_name << "\n";
            std::cout << "Program Type : " << program_type_name << "\n";
            std::cout << "Size         : " << current_program->raw_data_size << " bytes\n";
            for (auto stat = current_program->stats; stat != nullptr; stat = stat->next) {
                std::cout << std::setw(13) << std::left << stat->key << ": " << stat->value << "\n";
            }
        }
    }
    ebpf_free_programs(program_data);
    ebpf_free_string(error_message);

    // Show maps.
    std::cout << "\n";
    std::cout << "                     Key  Value      Max\n";
    std::cout << "          Map Type  Size   Size  Entries  Name\n";
    std::cout << "==================  ====  =====  =======  ========\n";
    bpf_object* object = bpf_object__open(filename.c_str());
    if (object == nullptr) {
        std::cout << "Couldn't get maps from " << filename << "\n";
        return ERROR_SUPPRESS_OUTPUT;
    }
    bpf_map* map;
    bpf_object__for_each_map(map, object)
    {
        printf(
            "%18s%6u%7u%9u  %s\n",
            _get_map_type_name(bpf_map__type(map)),
            bpf_map__key_size(map),
            bpf_map__value_size(map),
            bpf_map__max_entries(map),
            bpf_map__name(map));
    }
    bpf_object__close(object);
    return NO_ERROR;
}

static unsigned long
_verify_program(
    std::string filename,                             // Name of file in which the program appears.
    std::string section,                              // Name of section in which the program appears.
    std::string program_name,                         // Name of program.
    _In_opt_ const ebpf_program_type_t* program_type, // Program type.
    ebpf_verification_verbosity_t verbosity)          // Verbosity.
{
    const char* report;
    const char* error_message;
    ebpf_api_verifier_stats_t stats;

    unsigned long status = ebpf_api_elf_verify_program_from_file(
        filename.c_str(),
        !section.empty() ? section.c_str() : nullptr,
        !program_name.empty() ? program_name.c_str() : nullptr,
        program_type,
        verbosity,
        &report,
        &error_message,
        &stats);
    if (status == ERROR_SUCCESS) {
        std::cout << report;
        std::cout << "\nProgram terminates within " << stats.max_loop_count << " loop iterations\n";
        ebpf_free_string(report);
        return NO_ERROR;
    } else {
        if (error_message) {
            std::cerr << error_message << std::endl;
        }
        if (report) {
            std::cerr << "\nVerification report:\n" << report;
            std::cerr << stats.total_warnings << " errors\n\n";
        }
        ebpf_free_string(error_message);
        ebpf_free_string(report);
        return ERROR_SUPPRESS_OUTPUT;
    }
}

// The following function uses windows specific type as an input to match
// definition of "FN_HANDLE_CMD" in public file of NetSh.h
unsigned long
handle_ebpf_show_verification(
    IN LPCWSTR machine,
    _Inout_updates_(argc) LPWSTR* argv,
    IN DWORD current_index,
    IN DWORD argc,
    IN DWORD flags,
    IN LPCVOID data,
    OUT BOOL* done)
{
    UNREFERENCED_PARAMETER(machine);
    UNREFERENCED_PARAMETER(flags);
    UNREFERENCED_PARAMETER(data);
    UNREFERENCED_PARAMETER(done);

    TAG_TYPE tags[] = {
        {TOKEN_FILENAME, NS_REQ_PRESENT, FALSE},
        {TOKEN_SECTION, NS_REQ_ZERO, FALSE},
        {TOKEN_PROGRAM, NS_REQ_ZERO, FALSE},
        {TOKEN_TYPE, NS_REQ_ZERO, FALSE},
        {TOKEN_LEVEL, NS_REQ_ZERO, FALSE},
    };
    const int FILENAME_INDEX = 0;
    const int SECTION_INDEX = 1;
    const int PROGRAM_INDEX = 2;
    const int TYPE_INDEX = 3;
    const int LEVEL_INDEX = 4;

    unsigned long tag_type[_countof(tags)] = {0};

    unsigned long status =
        PreprocessCommand(nullptr, argv, current_index, argc, tags, _countof(tags), 0, _countof(tags), tag_type);

    VERBOSITY_LEVEL level = VL_NORMAL;
    std::string filename;
    std::string section = "";      // Use the first code section by default.
    std::string program_name = ""; // Use the first program name by default.
    std::string type_name = "";
    ebpf_program_type_t program_type_guid = {0};
    ebpf_program_type_t* program_type = nullptr;
    ebpf_attach_type_t attach_type;

    for (int i = 0; (status == NO_ERROR) && ((i + current_index) < argc); i++) {
        switch (tag_type[i]) {
        case FILENAME_INDEX: {
            filename = down_cast_from_wstring(std::wstring(argv[current_index + i]));
            break;
        }
        case SECTION_INDEX: {
            section = down_cast_from_wstring(std::wstring(argv[current_index + i]));
            break;
        }
        case PROGRAM_INDEX: {
            program_name = down_cast_from_wstring(std::wstring(argv[current_index + i]));
            break;
        }
        case TYPE_INDEX: {
            type_name = down_cast_from_wstring(std::wstring(argv[current_index + i]));
            if (ebpf_get_program_type_by_name(type_name.c_str(), &program_type_guid, &attach_type) != EBPF_SUCCESS) {
                status = ERROR_INVALID_PARAMETER;
            } else {
                program_type = &program_type_guid;
            }
            break;
        }
        case LEVEL_INDEX: {
            status =
                MatchEnumTag(NULL, argv[current_index + i], _countof(g_LevelEnum), g_LevelEnum, (unsigned long*)&level);
            if (status != NO_ERROR) {
                status = ERROR_INVALID_PARAMETER;
            }
            break;
        }
        default:
            status = ERROR_INVALID_SYNTAX;
            break;
        }
    }
    if (status != NO_ERROR) {
        return status;
    }

    ebpf_verification_verbosity_t verbosity = EBPF_VERIFICATION_VERBOSITY_NORMAL;
    switch (level) {
    case VL_NORMAL:
        verbosity = EBPF_VERIFICATION_VERBOSITY_NORMAL;
        break;
    case VL_INFORMATIONAL:
        verbosity = EBPF_VERIFICATION_VERBOSITY_INFORMATIONAL;
        break;
    case VL_VERBOSE:
        verbosity = EBPF_VERIFICATION_VERBOSITY_VERBOSE;
        break;
    default:
        // Assert this never happens.
        assert(!"Invalid verbosity level");
        break;
    }

    ebpf_api_program_info_t* program_data = nullptr;
    const char* error_message;
    ebpf_result_t result = ebpf_enumerate_programs(
        filename.c_str(), verbosity == EBPF_VERIFICATION_VERBOSITY_VERBOSE, &program_data, &error_message);
    if (result != ERROR_SUCCESS || program_data == nullptr) {
        if (error_message) {
            std::cerr << error_message << std::endl;
        } else {
            std::cerr << "\nNo section(s) found" << std::endl;
        }
        ebpf_free_string(error_message);
        ebpf_free_programs(program_data);
        return ERROR_SUPPRESS_OUTPUT;
    }

    for (ebpf_api_program_info_t* current = program_data; current != nullptr; current = current->next) {
        if (!section.empty() && section != current->section_name) {
            continue;
        }
        if (!program_name.empty() && program_name != current->program_name) {
            continue;
        }
        if (program_data->next != nullptr && strcmp(current->section_name, ".text") == 0) {
            // Skip subprograms.
            continue;
        }
        unsigned long program_status =
            _verify_program(filename, current->section_name, current->program_name, program_type, verbosity);
        if (program_status != NO_ERROR) {
            status = program_status;
        }
    }

    ebpf_free_string(error_message);
    ebpf_free_programs(program_data);
    return status;
}

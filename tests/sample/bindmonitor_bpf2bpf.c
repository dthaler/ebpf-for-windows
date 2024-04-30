// Copyright (c) Microsoft Corporation
// SPDX-License-Identifier: MIT

// clang -O2 -Werror -c bindmonitor_bpf2bpf.c -o bindmonitor_bpf2bpf.o
//
// For bpf code: clang -target bpf -O2 -Werror -c bindmonitor_bpf2bpf.c -o bindmonitor_bpf2bpf.o
// this passes the checker

// Whenever this sample program changes, bpf2c_tests will fail unless the
// expected files in tests\bpf2c_tests\expected are updated. The following
// script can be used to regenerate the expected files:
//     generate_expected_bpf2c_output.ps1
//
// Usage:
// .\scripts\generate_expected_bpf2c_output.ps1 <build_output_path>
// Example:
// .\scripts\generate_expected_bpf2c_output.ps1 .\x64\Debug\

#include "bpf_helpers.h"
#include "ebpf_nethooks.h"

bind_action_t
BindMonitor_Callee(uint8_t protocol);

SEC("bind")
bind_action_t
BindMonitor_Caller(bind_md_t* ctx)
{
    return BindMonitor_Callee(ctx->protocol);
}

SEC("bind")
__attribute__((noinline)) bind_action_t
BindMonitor_Callee(uint8_t protocol)
{
    return (protocol == 0) ? BIND_DENY : BIND_PERMIT;
}

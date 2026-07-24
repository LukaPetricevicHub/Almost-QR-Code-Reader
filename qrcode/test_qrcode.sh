#!/usr/bin/env bash

set -u

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "${script_dir}/.." && pwd)"
build_dir="${BUILD_DIR:-${repo_root}/cmake-build-debug}"
binary="${QRCODE_BINARY:-${build_dir}/qrcode/qrcode}"

if [[ -z "${QRCODE_BINARY:-}" && ! -f "${build_dir}/CMakeCache.txt" ]]; then
    printf 'Configuring CMake build directory: %s\n' "${build_dir}" >&2
    if ! cmake -S "${repo_root}" -B "${build_dir}" -DPROJECT_TOPIC=QRCODE >&2; then
        printf 'CMake configuration failed.\n' >&2
        exit 1
    fi
fi

if [[ -z "${QRCODE_BINARY:-}" ]]; then
    printf 'Building qrcode target.\n' >&2
    if ! cmake --build "${build_dir}" --target qrcode >&2; then
        printf 'Build failed.\n' >&2
        exit 1
    fi
fi

if [[ ! -x "${binary}" ]]; then
    printf 'QR code executable not found or not executable: %s\n' "${binary}" >&2
    exit 1
fi

total=0
failures=0

print_file_with_prefix() {
    local file="$1"
    while IFS= read -r line; do
        printf '    %s\n' "${line}" >&2
    done < "${file}"
}

run_case() {
    local image="$1"
    local expected="$2"
    local stdout_file
    local stderr_file
    local status
    local decoded

    total=$((total + 1))
    stdout_file="$(mktemp "${TMPDIR:-/tmp}/qrcode-test-stdout.XXXXXX")" || exit 2
    stderr_file="$(mktemp "${TMPDIR:-/tmp}/qrcode-test-stderr.XXXXXX")" || exit 2

    "${binary}" "${script_dir}/${image}" > "${stdout_file}" 2> "${stderr_file}"
    status=$?

    decoded="$(cat "${stdout_file}")"

    if [[ "${status}" -ne 0 ]]; then
        printf 'FAIL %-10s program exited with status %d\n' "${image}" "${status}" >&2
        failures=$((failures + 1))
    elif [[ "${decoded}" != "${expected}" ]]; then
        printf 'FAIL %-10s decoder mismatch\n' "${image}" >&2
        printf 'Expected: %s\n' "${expected}" >&2
        printf 'Actual:   %s\n' "${decoded}" >&2
        failures=$((failures + 1))
    else
        printf 'PASS %-10s %s\n' "${image}" "${expected}"
    fi

    if [[ -s "${stderr_file}" ]]; then
        printf 'stderr from %s:\n' "${image}" >&2
        print_file_with_prefix "${stderr_file}"
    fi

    rm -f "${stdout_file}" "${stderr_file}"
}

while IFS='|' read -r image expected; do
    [[ -z "${image}" ]] && continue
    run_case "${image}" "${expected}"
done <<'CASES'
qr01.png|12345
qr02.png|314159
qr03.png|Hello World
qr04.png|Intro. to C++
qr05.png|1 + 2 is 3
qr-v2.png|Hello from Luka
qr-v3.png|Almost QR Code Reader
qr-v4.png|I like Introduction to C++
qr-kanji.png|漢字は格好いい
CASES

if [[ "${failures}" -eq 0 ]]; then
    printf 'All %d QR regression tests passed.\n' "${total}"
    exit 0
fi

printf '%d of %d QR regression tests failed.\n' "${failures}" "${total}" >&2
exit 1

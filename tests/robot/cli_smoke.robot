*** Settings ***
Documentation       Small CLI smoke tests for cpu_monitor argument handling.
Library             OperatingSystem
Library             Process

Suite Setup         Cpu Monitor Binary Should Exist
Test Timeout        10 seconds


*** Variables ***
${REPO_ROOT}        ${CURDIR}/../..
${CPU_MONITOR}      ${REPO_ROOT}/bazel-bin/cpu_monitor


*** Test Cases ***
Help Prints Usage And Exits Successfully
    ${result}=    Run Cpu Monitor    --help
    Should Be Equal As Integers    ${result.rc}    0
    Should Be Empty    ${result.stderr}
    Should Contain    ${result.stdout}    Usage: cpu_monitor [--interval-sec N --output FILE]
    Should Contain    ${result.stdout}    Commands during Run state:

Missing Output Companion Flag Is Rejected
    ${result}=    Run Cpu Monitor    --interval-sec    1
    Should Be Equal As Integers    ${result.rc}    1
    Should Be Empty    ${result.stdout}
    Should Contain    ${result.stderr}    Both --interval-sec and --output must be provided together.
    Should Contain    ${result.stderr}    Usage: cpu_monitor [--interval-sec N --output FILE]

Unknown Argument Is Rejected
    ${result}=    Run Cpu Monitor    --wat
    Should Be Equal As Integers    ${result.rc}    1
    Should Be Empty    ${result.stdout}
    Should Contain    ${result.stderr}    Unknown argument: --wat
    Should Contain    ${result.stderr}    Usage: cpu_monitor [--interval-sec N --output FILE]


*** Keywords ***
Cpu Monitor Binary Should Exist
    File Should Exist    ${CPU_MONITOR}    Run `bazel build //:cpu_monitor` before this Robot suite.

Run Cpu Monitor
    [Arguments]    @{arguments}
    ${result}=    Run Process
    ...    ${CPU_MONITOR}
    ...    @{arguments}
    ...    cwd=${REPO_ROOT}
    ...    timeout=5 seconds
    ...    on_timeout=kill
    RETURN    ${result}

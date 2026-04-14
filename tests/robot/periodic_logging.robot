*** Settings ***
Documentation       Acceptance tests for cpu_monitor periodic file logging.
Library             OperatingSystem
Library             Process

Suite Setup         Periodic Logging Suite Setup
Test Timeout        10 seconds


*** Variables ***
${REPO_ROOT}        ${CURDIR}/../..
${CPU_MONITOR}      ${REPO_ROOT}/bazel-bin/cpu_monitor
${RESULTS_DIR}      ${REPO_ROOT}/robot-results


*** Test Cases ***
Periodic Logging Writes Timestamped Cpu Sample And Quit Exits
    ${log_file}=    Set Variable    ${RESULTS_DIR}/periodic-sample.log
    Remove File    ${log_file}
    ${result}=    Run Cpu Monitor With Delayed Quit    ${log_file}
    Should Be Equal As Integers    ${result.rc}    0
    Should Be Empty    ${result.stdout}
    Should Be Empty    ${result.stderr}
    File Should Exist    ${log_file}
    ${contents}=    Get File    ${log_file}
    Log File Should Contain Timestamped Cpu Sample    ${contents}

Periodic Logging Appends To Existing Output File
    ${log_file}=    Set Variable    ${RESULTS_DIR}/periodic-append.log
    Create File    ${log_file}    existing line${\n}
    ${result}=    Run Cpu Monitor With Delayed Quit    ${log_file}
    Should Be Equal As Integers    ${result.rc}    0
    Should Be Empty    ${result.stdout}
    Should Be Empty    ${result.stderr}
    ${contents}=    Get File    ${log_file}
    Should Start With    ${contents}    existing line${\n}
    Log File Should Contain Timestamped Cpu Sample    ${contents}


*** Keywords ***
Periodic Logging Suite Setup
    File Should Exist    ${CPU_MONITOR}    Run `bazel build //:cpu_monitor` before this Robot suite.
    Create Directory    ${RESULTS_DIR}

Run Cpu Monitor With Delayed Quit
    [Arguments]    ${log_file}
    ${result}=    Run Process
    ...    bash
    ...    -c
    ...    ( sleep 2; printf 'quit\n' ) | "$1" --interval-sec 1 --output "$2"
    ...    _
    ...    ${CPU_MONITOR}
    ...    ${log_file}
    ...    cwd=${REPO_ROOT}
    ...    timeout=5 seconds
    ...    on_timeout=kill
    RETURN    ${result}

Log File Should Contain Timestamped Cpu Sample
    [Arguments]    ${contents}
    Should Not Be Empty    ${contents}
    Should Match Regexp
    ...    ${contents}
    ...    (?m)^20[0-9][0-9]-[0-9][0-9]-[0-9][0-9] [0-9][0-9]:[0-9][0-9]:[0-9][0-9] core0=[0-9]+[.][0-9][0-9]%

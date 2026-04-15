*** Settings ***
Documentation       Acceptance tests for cpu_monitor periodic file logging.
Library             OperatingSystem
Library             Process
Library             CpuLogAssertions.py

Suite Setup         Periodic Logging Suite Setup
Test Timeout        15 seconds


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
    Log Text Should Contain Timestamped Cpu Sample    ${contents}

Periodic Logging Appends To Existing Output File
    ${log_file}=    Set Variable    ${RESULTS_DIR}/periodic-append.log
    Create File    ${log_file}    existing line${\n}
    ${result}=    Run Cpu Monitor With Delayed Quit    ${log_file}
    Should Be Equal As Integers    ${result.rc}    0
    Should Be Empty    ${result.stdout}
    Should Be Empty    ${result.stderr}
    ${contents}=    Get File    ${log_file}
    Should Start With    ${contents}    existing line${\n}
    Log Text Should Contain Timestamped Cpu Sample    ${contents}

Periodic Logging Continues Writing Multiple Samples
    ${log_file}=    Set Variable    ${RESULTS_DIR}/periodic-multiple.log
    Remove File    ${log_file}
    ${result}=    Run Cpu Monitor With Delayed Quit    ${log_file}    7    10 seconds
    Should Be Equal As Integers    ${result.rc}    0
    Should Be Empty    ${result.stdout}
    Should Be Empty    ${result.stderr}
    ${contents}=    Get File    ${log_file}
    Log Text Should Contain At Least Timestamped Cpu Samples    ${contents}    5
    Cpu Sample Timestamps Should Advance About Every Second    ${log_file}    5

Periodic Logging Reaches Each Expected Sample Count While Running
    ${log_file}=    Set Variable    ${RESULTS_DIR}/periodic-growth.log
    Remove File    ${log_file}
    Start Cpu Monitor With Delayed Quit    ${log_file}    7
    Sleep    300 milliseconds
    Log File Should Have Exactly Timestamped Cpu Samples    ${log_file}    0
    Log File Should Reach Timestamped Cpu Samples    ${log_file}    1
    Log File Should Reach Timestamped Cpu Samples    ${log_file}    2
    Log File Should Reach Timestamped Cpu Samples    ${log_file}    3
    Log File Should Reach Timestamped Cpu Samples    ${log_file}    4
    Log File Should Reach Timestamped Cpu Samples    ${log_file}    5
    ${result}=    Wait For Process    periodic-growth    timeout=10 seconds    on_timeout=kill
    Should Be Equal As Integers    ${result.rc}    0
    Cpu Sample Timestamps Should Advance About Every Second    ${log_file}    5

Interactive Prints Do Not Disturb Periodic Logging Schedule
    ${log_file}=    Set Variable    ${RESULTS_DIR}/periodic-with-prints.log
    Remove File    ${log_file}
    ${result}=    Run Cpu Monitor With Interactive Prints During Logging    ${log_file}
    Should Be Equal As Integers    ${result.rc}    0
    Should Be Empty    ${result.stderr}
    Stdout Text Should Contain At Least Cpu Samples    ${result.stdout}    3
    ${contents}=    Get File    ${log_file}
    Log Text Should Contain At Least Timestamped Cpu Samples    ${contents}    5
    Cpu Sample Timestamps Should Advance About Every Second    ${log_file}    5


*** Keywords ***
Periodic Logging Suite Setup
    File Should Exist    ${CPU_MONITOR}    Run `bazel build //:cpu_monitor` before this Robot suite.
    Create Directory    ${RESULTS_DIR}

Run Cpu Monitor With Delayed Quit
    [Arguments]    ${log_file}    ${quit_delay}=2    ${timeout}=5 seconds
    ${result}=    Run Process
    ...    bash
    ...    -c
    ...    ( sleep "$3"; printf 'quit\n' ) | "$1" --interval-sec 1 --output "$2"
    ...    _
    ...    ${CPU_MONITOR}
    ...    ${log_file}
    ...    ${quit_delay}
    ...    cwd=${REPO_ROOT}
    ...    timeout=${timeout}
    ...    on_timeout=kill
    RETURN    ${result}

Start Cpu Monitor With Delayed Quit
    [Arguments]    ${log_file}    ${quit_delay}=7
    Start Process
    ...    bash
    ...    -c
    ...    ( sleep "$3"; printf 'quit\n' ) | "$1" --interval-sec 1 --output "$2"
    ...    _
    ...    ${CPU_MONITOR}
    ...    ${log_file}
    ...    ${quit_delay}
    ...    cwd=${REPO_ROOT}
    ...    alias=periodic-growth

Run Cpu Monitor With Interactive Prints During Logging
    [Arguments]    ${log_file}
    ${result}=    Run Process
    ...    bash
    ...    -c
    ...    ( sleep 0.35; printf 'print\n'; sleep 0.65; printf 'print\n'; sleep 0.75; printf 'print\n'; sleep 4.8; printf 'quit\n' ) | "$1" --interval-sec 1 --output "$2"
    ...    _
    ...    ${CPU_MONITOR}
    ...    ${log_file}
    ...    cwd=${REPO_ROOT}
    ...    timeout=10 seconds
    ...    on_timeout=kill
    RETURN    ${result}

Log File Should Reach Timestamped Cpu Samples
    [Arguments]    ${log_file}    ${minimum_count}
    Wait Until Keyword Succeeds
    ...    2 seconds
    ...    100 milliseconds
    ...    Log File Should Contain At Least Timestamped Cpu Samples
    ...    ${log_file}
    ...    ${minimum_count}

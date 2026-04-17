*** Settings ***
Documentation       Acceptance tests for cpu_monitor interactive stdin commands.
Library             Process
Library             OperatingSystem
Library             CpuLogAssertions.py

Suite Setup         Cpu Monitor Binary Should Exist
Test Timeout        10 seconds


*** Variables ***
${REPO_ROOT}                    ${CURDIR}/../..
${CPU_MONITOR}                  ${REPO_ROOT}/bazel-bin/src/cpu_monitor
${UNKNOWN_COMMAND_MESSAGE}      Unknown command. Use Enter, print, or quit.


*** Test Cases ***
Print Command Emits Per-Core Load And Quit Exits
    ${result}=    Run Cpu Monitor With Input    print${\n}quit${\n}
    Should Be Equal As Integers    ${result.rc}    0
    Should Be Empty    ${result.stderr}
    Stdout Should Contain Cpu Sample    ${result.stdout}

Enter Emits Per-Core Load And Quit Exits
    ${result}=    Run Cpu Monitor With Input    ${\n}quit${\n}
    Should Be Equal As Integers    ${result.rc}    0
    Should Be Empty    ${result.stderr}
    Stdout Should Contain Cpu Sample    ${result.stdout}

Invalid Command Is Reported And Quit Still Exits
    ${result}=    Run Cpu Monitor With Input    wat${\n}quit${\n}
    Should Be Equal As Integers    ${result.rc}    0
    Should Not Print Cpu Sample    ${result.stdout}
    Stderr Should Report Unknown Command    ${result.stderr}


*** Keywords ***
Cpu Monitor Binary Should Exist
    File Should Exist    ${CPU_MONITOR}    Run `bazel build //:cpu_monitor` before this Robot suite.

Run Cpu Monitor With Input
    [Arguments]    ${stdin}
    ${result}=    Run Process
    ...    ${CPU_MONITOR}
    ...    cwd=${REPO_ROOT}
    ...    stdin=${stdin}
    ...    timeout=5 seconds
    ...    on_timeout=kill
    RETURN    ${result}

Stdout Should Contain Cpu Sample
    [Arguments]    ${stdout}
    Stdout Text Should Contain Cpu Sample    ${stdout}

Should Not Print Cpu Sample
    [Arguments]    ${stdout}
    Should Be Empty    ${stdout}

Stderr Should Report Unknown Command
    [Arguments]    ${stderr}
    Should Be Equal    ${stderr}    ${UNKNOWN_COMMAND_MESSAGE}

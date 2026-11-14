$ErrorActionPreference = 'Stop'
$corePath = Split-Path -Parent $PSScriptRoot
$testExe = Join-Path ([System.IO.Path]::GetTempPath()) ('fsm_reentry_' + [guid]::NewGuid().ToString('N') + '.exe')

function Test-ConfigCompilation {
    param (
        [string]$Name,
        [string[]]$Defines = @(),
        [string]$ExpectedDiagnostic = ''
    )

    # Capture expected compiler errors without PowerShell treating stderr as fatal.
    $ErrorActionPreference = 'Continue'
    $compilerOutput = & gcc -std=c99 -Wall -Wextra -Wconversion -Werror -pedantic `
        -I (Join-Path $PSScriptRoot 'support') -I $corePath -fsyntax-only `
        @Defines (Join-Path $PSScriptRoot 'test_fsm_config_compile.c') 2>&1 | Out-String
    $compilerExitCode = $LASTEXITCODE
    $ErrorActionPreference = 'Stop'

    if ($ExpectedDiagnostic -eq '') {
        if ($compilerExitCode -ne 0) { throw "$Name failed to compile:`n$compilerOutput" }
    }
    else {
        if (($compilerExitCode -eq 0) -or
            ($compilerOutput -notlike "*$ExpectedDiagnostic*")) {
            throw "$Name did not fail at the expected size check:`n$compilerOutput"
        }
    }
    Write-Output "PASS: $Name"
}

try {
    Test-ConfigCompilation -Name 'valid configuration'
    Test-ConfigCompilation -Name 'uint16 count upper boundary' `
        -Defines @('-DTEST_OPERATION_COUNT=65535U', '-DTEST_STATE_COUNT=65535U',
                   '-DTEST_TRANSITION_COUNT=65535U', '-DTEST_EVENT_COUNT=65535U')
    Test-ConfigCompilation -Name 'state table shorter than STATE_MAX rejected' `
        -Defines @('-DTEST_STATE_COUNT=3U') `
        -ExpectedDiagnostic 'Compile_Config_StateTableSizeCheck'
    Test-ConfigCompilation -Name 'state table longer than STATE_MAX rejected' `
        -Defines @('-DTEST_STATE_COUNT=1U') `
        -ExpectedDiagnostic 'Compile_Config_StateTableSizeCheck'
    Test-ConfigCompilation -Name '65536 transitions rejected before narrowing' `
        -Defines @('-DTEST_TRANSITION_COUNT=65536U') `
        -ExpectedDiagnostic 'Compile_Config_TransitionCountRangeCheck'
    Test-ConfigCompilation -Name 'state count overflow rejected' `
        -Defines @('-DTEST_OPERATION_COUNT=65536U', '-DTEST_STATE_COUNT=65536U') `
        -ExpectedDiagnostic 'Compile_Config_StateCountRangeCheck'
    Test-ConfigCompilation -Name 'event count overflow rejected' `
        -Defines @('-DTEST_EVENT_COUNT=65536U') `
        -ExpectedDiagnostic 'Compile_Config_EventCountRangeCheck'
    Test-ConfigCompilation -Name 'zero state count rejected' `
        -Defines @('-DTEST_STATE_COUNT=0U') `
        -ExpectedDiagnostic 'Compile_Config_StateCountRangeCheck'
    Test-ConfigCompilation -Name 'zero event count rejected' `
        -Defines @('-DTEST_EVENT_COUNT=0U') `
        -ExpectedDiagnostic 'Compile_Config_EventCountRangeCheck'

    & gcc -std=c99 -Wall -Wextra -Wconversion -Werror -pedantic `
        -I (Join-Path $PSScriptRoot 'support') -I $corePath `
        (Join-Path $corePath 'fsm.c') (Join-Path $corePath 'fsm_queue.c') `
        (Join-Path $PSScriptRoot 'test_fsm_reentry.c') -o $testExe
    if ($LASTEXITCODE -ne 0) { throw 'FSM host-test build failed.' }

    & $testExe
    if ($LASTEXITCODE -ne 0) { throw 'FSM host tests failed.' }
}
finally {
    if (Test-Path -LiteralPath $testExe) {
        Remove-Item -LiteralPath $testExe
    }
}

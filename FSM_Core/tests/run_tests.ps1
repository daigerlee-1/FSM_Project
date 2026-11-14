$ErrorActionPreference = 'Stop'
$corePath = Split-Path -Parent $PSScriptRoot
$testExe = Join-Path ([System.IO.Path]::GetTempPath()) ('fsm_reentry_' + [guid]::NewGuid().ToString('N') + '.exe')

try {
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

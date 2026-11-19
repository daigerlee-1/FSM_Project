$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$testExe = Join-Path ([IO.Path]::GetTempPath()) ('nvm_pc_' + [guid]::NewGuid().ToString('N') + '.exe')
try {
    $options = @('-std=c99', '-Wall', '-Wextra', '-Wconversion', '-Werror', '-pedantic')
    # main selects PC at build time; the derived PC branch has no selector.
    if (Select-String -LiteralPath (Join-Path $repoRoot 'Common/prj_defs.h') -Pattern '^#define PRJ_MOBILGENE_ENABLE' -Quiet) {
        $options += '-DPRJ_MOBILGENE_ENABLE=0U'
    }
    & gcc @options -I (Join-Path $repoRoot 'Common') -I (Join-Path $repoRoot 'FSM_Core') `
        -I (Join-Path $repoRoot 'Storage') (Join-Path $PSScriptRoot 'test_nvm_pc.c') `
        (Join-Path $repoRoot 'Storage/drv_nvm_pc.c') (Join-Path $repoRoot 'Storage/nvm_app.c') `
        (Join-Path $repoRoot 'FSM_Core/fsm.c') (Join-Path $repoRoot 'FSM_Core/fsm_queue.c') -o $testExe
    if ($LASTEXITCODE -ne 0) { throw 'NvM PC build failed' }
    & $testExe
    if ($LASTEXITCODE -ne 0) { throw 'NvM PC tests failed' }
    Write-Output 'PASS: NvM PC driver and application integration'
}
finally {
    if (Test-Path -LiteralPath $testExe) { Remove-Item -LiteralPath $testExe }
}

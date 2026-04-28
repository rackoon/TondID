param(
    [int]$Port = 3000,
    [string]$NodeExe = "C:\Users\User\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe",
    [string]$NpmCli = "C:\QA_Tools\npm-tmp\package\bin\npm-cli.js"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $NodeExe)) {
    throw "Node executable not found: $NodeExe"
}

if (-not (Test-Path $NpmCli)) {
    throw "npm CLI not found: $NpmCli"
}

$env:Path = "$(Split-Path $NodeExe);$env:Path"
$env:BROWSER = "none"
$env:PORT = "$Port"

& $NodeExe $NpmCli start

param(
    [string]$ArduinoCliDir = "C:\QA_Tools\arduino-cli",
    [string]$SketchPath = "C:\QA_Tools\MockBeaconRig\fw\squidrid\squidrid.ino",
    [string]$BuildPath = "C:\QA_Tools\MockBeaconRig\build-auto",
    [string]$Esp32CoreVersion = "2.0.17",
    [ValidateSet("auto", "esp32", "esp32c3", "esp32s2", "esp32s3")]
    [string]$Board = "auto",
    [string]$Port = "",
    [bool]$NoStub = $true,
    [switch]$CompileOnly
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

$Esp32PackageUrl = "https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json"

function Write-Step {
    param([string]$Message)
    Write-Host ""
    Write-Host "==> $Message"
}

function Add-ArduinoCliToPath {
    if (-not (Test-Path (Join-Path $ArduinoCliDir "arduino-cli.exe"))) {
        throw "arduino-cli.exe was not found in $ArduinoCliDir"
    }
    $env:Path = "$ArduinoCliDir;$env:Path"
}

function Find-EspPort {
    $activePorts = [System.IO.Ports.SerialPort]::GetPortNames()
    $ports = Get-CimInstance Win32_PnPEntity |
        Where-Object {
            $_.Name -match "\(COM\d+\)" -and
            ($_.Name -match "CP210|CH340|USB Serial|USB JTAG|ESP32|UART|Silicon Labs|WCH|FTDI|CDC" -or $_.PNPDeviceID -match "VID_303A")
        } |
        ForEach-Object {
            $com = [regex]::Match($_.Name, "COM\d+").Value
            if ($activePorts -contains $com) {
                [pscustomobject]@{
                    Port = $com
                    Name = $_.Name
                    PNPDeviceID = $_.PNPDeviceID
                }
            }
        }

    if (-not $ports) {
        throw "No active ESP32-like COM port found. Check USB cable, boot mode, driver installation, and close tools that may be holding the port."
    }

    return ($ports | Select-Object -First 1)
}

function Assert-PortPresent {
    param([string]$PortName)

    $activePorts = [System.IO.Ports.SerialPort]::GetPortNames()
    if ($activePorts -notcontains $PortName) {
        throw "$PortName is not currently active. Check the cable/boot mode and wait for Windows to enumerate the board."
    }
}

function Get-EspToolPath {
    $tool = Get-ChildItem -Path "$env:LOCALAPPDATA\Arduino15\packages\esp32\tools\esptool_py" -Recurse -Filter "esptool.exe" |
        Sort-Object FullName -Descending |
        Select-Object -First 1

    if (-not $tool) {
        throw "esptool.exe was not found. Install the ESP32 Arduino core first."
    }

    return $tool.FullName
}

function Get-EspChip {
    param([string]$DetectedPort)

    $esptool = Get-EspToolPath
    $output = & $esptool --chip auto --port $DetectedPort --before default_reset --after no_reset chip_id 2>&1
    $text = ($output | Out-String)

    if ($text -match "Chip is (ESP32-C3)") { return "esp32c3" }
    if ($text -match "Chip is (ESP32-S3)") { return "esp32s3" }
    if ($text -match "Chip is (ESP32-S2)") { return "esp32s2" }
    if ($text -match "Chip is (ESP32)") { return "esp32" }

    throw "Could not identify ESP32 chip from esptool output. Close Chrome WebSerial, serial monitors, and other tools using $DetectedPort, then retry. Raw output:`n$text"
}

function Get-Fqbn {
    param([string]$BoardId)

    switch ($BoardId) {
        "esp32c3" { return "esp32:esp32:esp32c3:UploadSpeed=115200,FlashMode=dio,FlashFreq=40" }
        "esp32s3" { return "esp32:esp32:esp32s3:UploadSpeed=115200,FlashMode=dio,FlashFreq=40" }
        "esp32s2" { return "esp32:esp32:esp32s2:UploadSpeed=115200,FlashMode=dio,FlashFreq=40" }
        default { return "esp32:esp32:esp32:UploadSpeed=115200,FlashMode=dio,FlashFreq=40" }
    }
}

Add-ArduinoCliToPath

Write-Step "Checking Arduino CLI"
arduino-cli version

Write-Step "Preparing ESP32 Arduino core $Esp32CoreVersion"
arduino-cli core update-index --additional-urls $Esp32PackageUrl
arduino-cli core install "esp32:esp32@$Esp32CoreVersion" --additional-urls $Esp32PackageUrl

Write-Step "Ensuring EspSoftwareSerial dependency"
arduino-cli lib install EspSoftwareSerial

if ($Board -eq "auto" -and [string]::IsNullOrWhiteSpace($Port)) {
    Write-Step "Detecting ESP32 COM port"
    $detected = Find-EspPort
    $Port = $detected.Port
    Write-Host "Using $Port ($($detected.Name))"
}

if ($Board -eq "auto") {
    Write-Step "Identifying ESP32 chip"
    $boardId = Get-EspChip -DetectedPort $Port
}
else {
    $boardId = $Board
    Write-Step "Using requested ESP32 board"
}

$fqbn = Get-Fqbn -BoardId $boardId
Write-Host "Detected $boardId"
Write-Host "Using FQBN $fqbn"

Write-Step "Compiling firmware"
New-Item -ItemType Directory -Path $BuildPath -Force | Out-Null
arduino-cli compile --warnings none --build-path $BuildPath --fqbn $fqbn $SketchPath

if ($CompileOnly) {
    Write-Host "Compile-only mode complete."
    exit 0
}

if ([string]::IsNullOrWhiteSpace($Port)) {
    Write-Step "Detecting ESP32 COM port"
    $detected = Find-EspPort
    $Port = $detected.Port
    Write-Host "Using $Port ($($detected.Name))"
}

Assert-PortPresent -PortName $Port

Write-Step "Uploading firmware"
$uploadArgs = @(
    "upload",
    "-p", $Port,
    "--build-path", $BuildPath,
    "--fqbn", $fqbn
)

if ($NoStub) {
    $uploadArgs += @("--upload-property", "upload.flags=--no-stub")
}

$uploadArgs += $SketchPath
& arduino-cli @uploadArgs
if ($LASTEXITCODE -ne 0) {
    throw "arduino-cli upload failed with exit code $LASTEXITCODE. Close Chrome/WebSerial or any serial monitor using $Port, then retry."
}

Write-Step "Waiting for boot"
Start-Sleep -Seconds 10
Write-Host "SUCCESS: SquidRID firmware flashed to $Port. Open Chrome at https://squidrid.flyandi.net and connect via WebSerial."

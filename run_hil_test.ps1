# run_hil_test.ps1 - Hardware-in-the-Loop Test Runner for STM32 and ADS1292R
# This script flashes the STM32 via ST-Link and reads test results directly from RAM.

$ErrorActionPreference = "Stop"

# 1. Config Paths
$ProjectRoot = Get-Location
$ProgrammerPath = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"

# Output and list directories
$ExeDir = Join-Path $ProjectRoot "EWARM\cECG\Exe"
$ListDir = Join-Path $ProjectRoot "EWARM\cECG\List"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "       STM32 - ADS1292R HIL Automation Test Runner        " -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 2. Check STM32CubeProgrammer existence
if (-not (Test-Path $ProgrammerPath)) {
    Write-Error "STM32CubeProgrammer not found at: $ProgrammerPath`nPlease make sure STM32CubeProgrammer is installed."
    exit 1
}

# 3. Locate compiled hex file and map file
$HexFile = $null
if (Test-Path $ExeDir) {
    $HexFile = Get-ChildItem -Path $ExeDir -Filter "*.hex" | Select-Object -First 1
}

$MapFile = $null
if (Test-Path $ListDir) {
    $MapFile = Get-ChildItem -Path $ListDir -Filter "*.map" | Select-Object -First 1
}

if ($null -eq $HexFile -or $null -eq $MapFile) {
    Write-Host "`n[!] Compiler outputs not found." -ForegroundColor Yellow
    Write-Host "    Expected Hex in: $ExeDir" -ForegroundColor DarkGray
    Write-Host "    Expected Map in: $ListDir" -ForegroundColor DarkGray
    Write-Host "`n[Action Required] Please open IAR Embedded Workbench, build the project (cECG configuration), and then run this script again." -ForegroundColor Magenta
    exit 1
}

Write-Host "[+] Found firmware: $($HexFile.Name)" -ForegroundColor Green
Write-Host "[+] Found linker map: $($MapFile.Name)" -ForegroundColor Green

# 4. Parse map file for symbol addresses
Write-Host "`n[~] Parsing map file to locate test variables..." -ForegroundColor Cyan
$testStepAddr = $null
$testResultAddr = $null

Get-Content $MapFile.FullName | ForEach-Object {
    # Match symbol address from IAR map file layout:
    # Example: test_step                 0x200000a4   0x1  Data  Gb  ads1292r_test.o [1]
    if ($_ -match 'test_step\s+(0x[0-9a-fA-F''`]+)') {
        $testStepAddr = $Matches[1].Replace("'", "").Replace("`", "")
    }
    if ($_ -match 'test_result\s+(0x[0-9a-fA-F''`]+)') {
        $testResultAddr = $Matches[1].Replace("'", "").Replace("`", "")
    }
}

if ($null -eq $testStepAddr -or $null -eq $testResultAddr) {
    Write-Error "Could not find symbols 'test_step' or 'test_result' in the map file. Please rebuild the project."
    exit 1
}

Write-Host "    -> test_step   address: $testStepAddr" -ForegroundColor Gray
Write-Host "    -> test_result address: $testResultAddr" -ForegroundColor Gray

# 5. User confirmation for flashing
$choice = Read-Host "`nDo you want to FLASH the firmware before running the test? (Y/N)"
$flash = $choice.Trim().ToUpper() -eq "Y"

if ($flash) {
    Write-Host "`n[~] Connecting to ST-Link and flashing firmware..." -ForegroundColor Cyan
    # Flash hex and verify
    & $ProgrammerPath -c port=SWD -w $HexFile.FullName -v
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to flash firmware to STM32. Check USB connection and pinouts."
        exit 1
    }
    Write-Host "[+] Flash successful!" -ForegroundColor Green
    
    Write-Host "`n[~] Resetting microcontroller to start tests..." -ForegroundColor Cyan
    & $ProgrammerPath -c port=SWD -rst
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Failed to reset microcontroller."
        exit 1
    }
    
    # Wait for the firmware to boot up and run tests (Wait 2 seconds)
    Write-Host "[~] Waiting 2 seconds for tests to run..." -ForegroundColor DarkCyan
    Start-Sleep -Seconds 2
} else {
    Write-Host "`n[~] Skipping flashing. Will connect and read current memory status..." -ForegroundColor Cyan
}

# 6. Read results from RAM
Write-Host "`n[~] Reading test variables from RAM via ST-Link (hotplug mode)..." -ForegroundColor Cyan

function Read-RAMByte([string]$address) {
    # Formats address to lowercase 8-char padded string
    $cleanAddr = $address.Replace("0x", "").ToLower().Trim().PadLeft(8, '0')
    
    # Run reader CLI with hotplug to avoid interrupting CPU
    $cliOutput = & $ProgrammerPath -c port=SWD mode=hotplug -r8 $address 1
    
    $val = $null
    foreach ($line in $cliOutput) {
        if ($line -match "$cleanAddr\s+([0-9a-fA-F]{2})") {
            $val = [Convert]::ToByte($Matches[1], 16)
            break
        }
    }
    return $val
}

$stepVal = Read-RAMByte $testStepAddr
$resultVal = Read-RAMByte $testResultAddr

if ($null -eq $stepVal -or $null -eq $resultVal) {
    Write-Error "Failed to read memory from target STM32."
    exit 1
}

Write-Host "----------------------------------------------------------" -ForegroundColor Gray
Write-Host "Read values from RAM:" -ForegroundColor Gray
Write-Host "  test_step   = $stepVal" -ForegroundColor Gray
Write-Host "  test_result = $resultVal" -ForegroundColor Gray
Write-Host "----------------------------------------------------------" -ForegroundColor Gray

# 7. Print analysis
Write-Host "`n====================== TEST RESULTS ======================" -ForegroundColor Cyan

# Step descriptions
$steps = @{
    0 = "Not Started / Resetting"
    1 = "SPI Communication Test (Read Chip ID)"
    2 = "Register Write & Readback Verification"
    3 = "DRDY (Data Ready) Hardware Signal Detection"
    4 = "Continuous Data Frame Reading"
}

# Print status of each step
for ($i = 1; $i -le 4; $i++) {
    $desc = $steps[$i]
    if ($stepVal -gt $i) {
        # Passed step
        Write-Host "[ PASS ] Step $i: $desc" -ForegroundColor Green
    } elseif ($stepVal -eq $i) {
        if ($resultVal -eq 0) {
            Write-Host "[ PASS ] Step $i: $desc" -ForegroundColor Green
        } elseif ($resultVal -eq $i) {
            Write-Host "[ FAIL ] Step $i: $desc" -ForegroundColor Red
        } else {
            # Running or unknown
            Write-Host "[ RUN  ] Step $i: $desc (Current Step)" -ForegroundColor Yellow
        }
    } else {
        # Not reached
        Write-Host "[ SKIP ] Step $i: $desc" -ForegroundColor Gray
    }
}

Write-Host "==========================================================" -ForegroundColor Cyan

# 8. Overall Conclusion and Recommendations
if ($resultVal -eq 0) {
    Write-Host "`n[*** SUCCESS ***] ADS1292R SPI Communication Test PASSED!" -ForegroundColor Green -BackgroundColor Black
    Write-Host "All hardware signals, clock phases, register read/writes, and data frames are functioning correctly!" -ForegroundColor Green
} elseif ($resultVal -eq 0xFF) {
    Write-Host "`n[!] Test is still running or has not started." -ForegroundColor Yellow
    Write-Host "Troubleshooting:" -ForegroundColor DarkYellow
    Write-Host "  - Verify that the STM32 has power." -ForegroundColor DarkYellow
    Write-Host "  - Check if the code is stuck in SystemClock_Config or HAL_Init." -ForegroundColor DarkYellow
} else {
    Write-Host "`n[*** FAILED ***] Test failed at Step $resultVal" -ForegroundColor Red -BackgroundColor Black
    
    Write-Host "`nTroubleshooting suggestions for Step $resultVal:" -ForegroundColor Yellow
    switch ($resultVal) {
        1 {
            Write-Host "  - Check SPI connections (SCK: PA5, MISO: PA6, MOSI: PA7)." -ForegroundColor Gray
            Write-Host "  - Check Chip Select connection (CS: PA4)." -ForegroundColor Gray
            Write-Host "  - Verify that the ADS1292R has power (AVDD/DVDD) and is out of Power-Down mode." -ForegroundColor Gray
            Write-Host "  - Check Reset Pin connection (RST: PA2) and verify it's driven high." -ForegroundColor Gray
        }
        2 {
            Write-Host "  - SPI read works, but write failed. Verify if SPI writes are clean on logic analyzer." -ForegroundColor Gray
            Write-Host "  - Check for SPI signal integrity issues (ribbon cables too long, speed too fast)." -ForegroundColor Gray
            Write-Host "  - Ensure that the SDATAC command was processed so registers are writeable." -ForegroundColor Gray
        }
        3 {
            Write-Host "  - DRDY pin (PA3) did not pulse low. Check if START pin (PA1) is high." -ForegroundColor Gray
            Write-Host "  - Check if ADS1292R internal oscillator is running (CLK pin status)." -ForegroundColor Gray
            Write-Host "  - Verify connection of DRDY pin to PA3." -ForegroundColor Gray
        }
        4 {
            Write-Host "  - DRDY goes low, but data frame read failed. Verify SPI baudrate (currently 1MHz)." -ForegroundColor Gray
            Write-Host "  - Check if SPI MISO line has proper logic levels." -ForegroundColor Gray
            Write-Host "  - Frame header did not match 0xC0 or 0x00. Check if SPI clock phase (CPHA=1) is correctly applied." -ForegroundColor Gray
        }
    }
}
Write-Host ""

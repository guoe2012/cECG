# C:\Users\guoe2\.gemini\antigravity-cli\brain\ecee6a38-f12d-45a2-9b3e-536dad1631d3\scratch\generate_plot.ps1
# This script reads the dumped memory files, reconstructs the ECG waveforms, and outputs both SVG and PNG plots.

Add-Type -AssemblyName System.Drawing

$scratchDir = "C:\Users\guoe2\.gemini\antigravity-cli\brain\ecee6a38-f12d-45a2-9b3e-536dad1631d3\scratch"
$ch1File = Join-Path $scratchDir "ch1_dump.txt"
$ch2File = Join-Path $scratchDir "ch2_dump.txt"

# 1. Read ecg_sample_count from RAM to find the write pointer
$progCli = "C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"
$cliOut = & $progCli -c port=SWD mode=hotplug -r32 0x20000f78 4
$line = $cliOut | Select-String "0x20000F78"
$sampleCount = 0
if ($line -match "0x20000F78\s*:\s*([0-9A-Fa-f]{8})") {
    $valHex = $Matches[1]
    $sampleCount = [System.Convert]::ToUInt32($valHex, 16)
}
Write-Host "Current ecg_sample_count: $sampleCount"

# 2. Parse hex files into integers
function Parse-HexDump($filePath) {
    $vals = New-Object System.Collections.Generic.List[int]
    $lines = Get-Content $filePath
    foreach ($l in $lines) {
        if ($l -match "0x[0-9A-Fa-f]{8}\s*:\s*(.*)") {
            $hexParts = $Matches[1] -split "\s+"
            foreach ($hp in $hexParts) {
                if ($hp.Length -eq 8) {
                    # Convert to 32-bit unsigned integer first
                    $uval = [System.Convert]::ToUInt32($hp, 16)
                    # Convert to 32-bit signed integer
                    $val = if ($uval -band 0x80000000) { [int]($uval - 0x100000000) } else { [int]$uval }
                    $vals.Add($val)
                }
            }
        }
    }
    return $vals.ToArray()
}

$ch1Raw = Parse-HexDump $ch1File
$ch2Raw = Parse-HexDump $ch2File

Write-Host "Read $($ch1Raw.Length) samples for CH1, $($ch2Raw.Length) samples for CH2"
if ($ch1Raw.Length -gt 0) {
    Write-Host "CH1 Raw Max: $(($ch1Raw | Measure-Object -Maximum).Maximum), Min: $(($ch1Raw | Measure-Object -Minimum).Minimum)"
}
if ($ch2Raw.Length -gt 0) {
    Write-Host "CH2 Raw Max: $(($ch2Raw | Measure-Object -Maximum).Maximum), Min: $(($ch2Raw | Measure-Object -Minimum).Minimum)"
}

if ($ch1Raw.Length -eq 0 -or $ch2Raw.Length -eq 0) {
    Write-Error "Failed to parse hex dump files."
    Exit
}

# 3. Align circular buffer data
# The circular buffer has size 480.
# The write pointer is index = sampleCount % 480.
# The oldest sample is at index (sampleCount % 480), and the newest is at index - 1 (modulo 480).
$bufSize = 480
$writePtr = $sampleCount % $bufSize

$ch1Aligned = New-Object int[] $bufSize
$ch2Aligned = New-Object int[] $bufSize

for ($i = 0; $i -lt $bufSize; $i++) {
    $srcIndex = ($writePtr + $i) % $bufSize
    if ($srcIndex -lt $ch1Raw.Length) {
        $ch1Aligned[$i] = $ch1Raw[$srcIndex]
    }
    if ($srcIndex -lt $ch2Raw.Length) {
        $ch2Aligned[$i] = $ch2Raw[$srcIndex]
    }
}

Write-Host "CH1 Aligned First 10: $(($ch1Aligned[0..9]) -join ', ')"
Write-Host "CH2 Aligned First 10: $(($ch2Aligned[0..9]) -join ', ')"

# 4. Apply HPF (baseline removal)
$ch1Baseline = $ch1Aligned[0]
$ch2Baseline = $ch2Aligned[0]
$ch1Filtered = New-Object int[] $bufSize
$ch2Filtered = New-Object int[] $bufSize

for ($i = 0; $i -lt $bufSize; $i++) {
    if ($i -eq 0) {
        $ch1Baseline = $ch1Aligned[$i]
        $ch2Baseline = $ch2Aligned[$i]
    } else {
        $ch1Baseline += [int](($ch1Aligned[$i] - $ch1Baseline) / 128)
        $ch2Baseline += [int](($ch2Aligned[$i] - $ch2Baseline) / 128)
    }
    # Use [int64] for subtraction to avoid PowerShell automatic double conversion, then cast back
    $diff1 = [int64]$ch1Aligned[$i] - [int64]$ch1Baseline
    $diff2 = [int64]$ch2Aligned[$i] - [int64]$ch2Baseline
    if ($diff1 -lt -2147483648 -or $diff1 -gt 2147483647) {
        Write-Host "CH1 Overflow at index $i - aligned=$($ch1Aligned[$i]), baseline=$ch1Baseline, diff=$diff1"
    }
    if ($diff2 -lt -2147483648 -or $diff2 -gt 2147483647) {
        Write-Host "CH2 Overflow at index $i - aligned=$($ch2Aligned[$i]), baseline=$ch2Baseline, diff=$diff2"
    }
    $ch1Filtered[$i] = [int]$diff1
    $ch2Filtered[$i] = [int]$diff2
}

# 5. Generate PNG using System.Drawing
$width = 1200
$height = 600
$bmp = New-Object System.Drawing.Bitmap($width, $height)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias

# Colors
$bgBrush = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::FromArgb(20, 20, 20))
$g.FillRectangle($bgBrush, 0, 0, $width, $height)

# Draw grid lines (1 large grid = 100 pixels, 1 small grid = 20 pixels)
$gridPenLarge = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(60, 40, 40), 1)
$gridPenSmall = New-Object System.Drawing.Pen([System.Drawing.Color]::FromArgb(35, 25, 25), 1)

# Vertical grid
for ($x = 0; $x -lt $width; $x += 20) {
    $pen = if ($x % 100 -eq 0) { $gridPenLarge } else { $gridPenSmall }
    $g.DrawLine($pen, $x, 0, $x, $height)
}
# Horizontal grid
for ($y = 0; $y -lt $height; $y += 20) {
    $pen = if ($y % 100 -eq 0) { $gridPenLarge } else { $gridPenSmall }
    $g.DrawLine($pen, 0, $y, $width, $y)
}

# Channels parameters
$ch1CenterY = 170
$ch2CenterY = 430
$scaleY = 1.0 / 512.0 # Multiply raw offset by this to get pixel height (equivalent to raw / 512)
$scaleX = $width / $bufSize # Map 480 points to 1200 pixels

# Draw Waveforms
$penCh1 = New-Object System.Drawing.Pen([System.Drawing.Color]::Cyan, 2)
$penCh2 = New-Object System.Drawing.Pen([System.Drawing.Color]::Yellow, 2)

for ($i = 1; $i -lt $bufSize; $i++) {
    $x1 = ($i - 1) * $scaleX
    $x2 = $i * $scaleX
    
    # CH1
    $y1_1 = $ch1CenterY - ($ch1Filtered[$i - 1] * $scaleY)
    $y1_2 = $ch1CenterY - ($ch1Filtered[$i] * $scaleY)
    # Clamp
    if ($y1_1 -lt 20) { $y1_1 = 20 } elseif ($y1_1 -gt 280) { $y1_1 = 280 }
    if ($y1_2 -lt 20) { $y1_2 = 20 } elseif ($y1_2 -gt 280) { $y1_2 = 280 }
    $g.DrawLine($penCh1, $x1, $y1_1, $x2, $y1_2)

    # CH2
    $y2_1 = $ch2CenterY - ($ch2Filtered[$i - 1] * $scaleY)
    $y2_2 = $ch2CenterY - ($ch2Filtered[$i] * $scaleY)
    # Clamp
    if ($y2_1 -lt 320) { $y2_1 = 320 } elseif ($y2_1 -gt 580) { $y2_1 = 580 }
    if ($y2_2 -lt 320) { $y2_2 = 320 } elseif ($y2_2 -gt 580) { $y2_2 = 580 }
    $g.DrawLine($penCh2, $x1, $y2_1, $x2, $y2_2)
}

# Add text labels
$fontTitle = New-Object System.Drawing.Font("Arial", 14, [System.Drawing.FontStyle]::Bold)
$fontText = New-Object System.Drawing.Font("Arial", 10, [System.Drawing.FontStyle]::Regular)
$brushCyan = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::Cyan)
$brushYellow = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::Yellow)
$brushWhite = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)

$g.DrawString("ADS1292R ECG CH1 (Test Signal)", $fontTitle, $brushCyan, 20, 20)
$g.DrawString("ADS1292R ECG CH2 (Test Signal)", $fontTitle, $brushYellow, 20, 320)

$timeText = "Duration: 0.96s (480 samples @ 500Hz) | Sample Count: $sampleCount"
$g.DrawString($timeText, $fontText, $brushWhite, 20, $height - 30)

# Save PNG
$pngPath = Join-Path $scratchDir "ecg_waveform.png"
$bmp.Save($pngPath, [System.Drawing.Imaging.ImageFormat]::Png)

# Clean up
$penCh1.Dispose()
$penCh2.Dispose()
$bgBrush.Dispose()
$brushCyan.Dispose()
$brushYellow.Dispose()
$brushWhite.Dispose()
$gridPenLarge.Dispose()
$gridPenSmall.Dispose()
$fontTitle.Dispose()
$fontText.Dispose()
$g.Dispose()
$bmp.Dispose()

Write-Host "PNG saved successfully to: $pngPath"

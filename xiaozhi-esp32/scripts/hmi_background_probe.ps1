param(
    [string]$SourceHmi = 'D:\QQ\serial_warm_home .HMI',
    [string]$OutputHmi = '',
    [string]$LightPng = '',
    [int]$TargetPngIndex = 7,
    [switch]$PatchCopy
)

$ErrorActionPreference = 'Stop'

function Get-ProjectRoot {
    $scriptPath = Split-Path -Parent $PSCommandPath
    return (Resolve-Path (Join-Path $scriptPath '..')).Path
}

function Get-Sha256([string]$Path) {
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
}

function Read-UInt32BE([byte[]]$Bytes, [int]$Offset) {
    return (([uint32]$Bytes[$Offset] -shl 24) -bor
            ([uint32]$Bytes[$Offset + 1] -shl 16) -bor
            ([uint32]$Bytes[$Offset + 2] -shl 8) -bor
            [uint32]$Bytes[$Offset + 3])
}

function Find-PngResources([byte[]]$Bytes) {
    $signature = [byte[]](0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A)
    $resources = New-Object System.Collections.Generic.List[object]
    $i = 0

    while ($i -le $Bytes.Length - $signature.Length) {
        $matched = $true
        for ($j = 0; $j -lt $signature.Length; $j++) {
            if ($Bytes[$i + $j] -ne $signature[$j]) {
                $matched = $false
                break
            }
        }

        if (-not $matched) {
            $i++
            continue
        }

        $width = Read-UInt32BE $Bytes ($i + 16)
        $height = Read-UInt32BE $Bytes ($i + 20)
        $pos = $i + 8
        $end = -1

        while ($pos -le $Bytes.Length - 12) {
            $chunkLen = Read-UInt32BE $Bytes $pos
            $chunkType = [System.Text.Encoding]::ASCII.GetString($Bytes, $pos + 4, 4)
            $next = $pos + 12 + $chunkLen

            if ($next -gt $Bytes.Length) {
                break
            }

            if ($chunkType -eq 'IEND') {
                $end = $next
                break
            }

            $pos = $next
        }

        if ($end -gt 0) {
            $resources.Add([pscustomobject]@{
                Index  = $resources.Count
                Offset = $i
                Length = $end - $i
                Width  = $width
                Height = $height
            }) | Out-Null
            $i = $end
        } else {
            $i++
        }
    }

    return $resources
}

function New-LightAirDetailPng([string]$Path) {
    Add-Type -AssemblyName System.Drawing

    $w = 480
    $h = 272
    $fmt = [System.Drawing.Imaging.PixelFormat]::Format24bppRgb
    $bmp = [System.Drawing.Bitmap]::new($w, $h, $fmt)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::None
    $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::None
    $g.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::SingleBitPerPixelGridFit

    $brushBg = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(245, 247, 241))
    $brushPanel = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(255, 255, 250))
    $brushPanel2 = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(235, 242, 232))
    $brushAccent = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(79, 152, 116))
    $brushAccent2 = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(239, 179, 92))
    $brushMuted = [System.Drawing.SolidBrush]::new([System.Drawing.Color]::FromArgb(214, 224, 209))
    $penLine = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(194, 206, 188), 1)
    $penAccent = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(79, 152, 116), 3)
    $penWarm = [System.Drawing.Pen]::new([System.Drawing.Color]::FromArgb(239, 179, 92), 3)

    try {
        $g.Clear($brushBg.Color)

        # Top status band and back affordance.
        $g.FillRectangle($brushPanel, 14, 12, 452, 34)
        $g.DrawRectangle($penLine, 14, 12, 452, 34)
        $g.FillEllipse($brushPanel2, 24, 20, 18, 18)
        $g.DrawLine($penAccent, 36, 24, 30, 29)
        $g.DrawLine($penAccent, 30, 29, 36, 34)
        $g.FillRectangle($brushAccent, 70, 24, 128, 10)
        $g.FillRectangle($brushMuted, 210, 24, 68, 10)
        $g.FillRectangle($brushMuted, 288, 24, 54, 10)

        # Main score card.
        $g.FillRectangle($brushPanel, 18, 58, 180, 150)
        $g.DrawRectangle($penLine, 18, 58, 180, 150)
        $g.FillEllipse($brushMuted, 50, 82, 112, 112)
        $g.FillEllipse($brushBg, 66, 98, 80, 80)
        $g.DrawArc($penAccent, 50, 82, 112, 112, -90, 245)
        $g.DrawArc($penWarm, 50, 82, 112, 112, 155, 55)
        $g.FillRectangle($brushAccent, 84, 130, 44, 10)
        $g.FillRectangle($brushMuted, 74, 150, 64, 8)

        # Three sensor cards.
        $xList = @(212, 300, 388)
        foreach ($x in $xList) {
            $g.FillRectangle($brushPanel, $x, 58, 74, 70)
            $g.DrawRectangle($penLine, $x, 58, 74, 70)
            $g.FillRectangle($brushMuted, $x + 12, 72, 50, 8)
            $g.FillRectangle($brushAccent, $x + 14, 94, 44, 10)
            $g.FillRectangle($brushPanel2, $x + 14, 110, 36, 6)
        }

        # Detail and comfort strips.
        $g.FillRectangle($brushPanel, 212, 140, 250, 32)
        $g.DrawRectangle($penLine, 212, 140, 250, 32)
        $g.FillRectangle($brushAccent, 226, 152, 66, 8)
        $g.FillRectangle($brushMuted, 306, 152, 124, 8)

        $g.FillRectangle($brushPanel2, 18, 220, 180, 34)
        $g.DrawRectangle($penLine, 18, 220, 180, 34)
        $g.FillRectangle($brushAccent2, 34, 232, 52, 8)
        $g.FillRectangle($brushMuted, 98, 232, 78, 8)

        # Trend chart visual frame.
        $g.FillRectangle($brushPanel, 212, 184, 250, 70)
        $g.DrawRectangle($penLine, 212, 184, 250, 70)
        for ($gx = 232; $gx -le 442; $gx += 42) {
            $g.DrawLine($penLine, $gx, 198, $gx, 240)
        }
        for ($gy = 198; $gy -le 240; $gy += 14) {
            $g.DrawLine($penLine, 224, $gy, 448, $gy)
        }
        $points = @(
            [System.Drawing.Point]::new(224, 232),
            [System.Drawing.Point]::new(258, 220),
            [System.Drawing.Point]::new(292, 226),
            [System.Drawing.Point]::new(326, 206),
            [System.Drawing.Point]::new(360, 212),
            [System.Drawing.Point]::new(394, 198),
            [System.Drawing.Point]::new(448, 204)
        )
        $g.DrawLines($penAccent, $points)
        foreach ($p in $points) {
            $g.FillRectangle($brushAccent, $p.X - 2, $p.Y - 2, 5, 5)
        }
    } finally {
        $penWarm.Dispose()
        $penAccent.Dispose()
        $penLine.Dispose()
        $brushMuted.Dispose()
        $brushAccent2.Dispose()
        $brushAccent.Dispose()
        $brushPanel2.Dispose()
        $brushPanel.Dispose()
        $brushBg.Dispose()
        $g.Dispose()
    }

    $directory = Split-Path -Parent $Path
    if (-not (Test-Path -LiteralPath $directory)) {
        New-Item -ItemType Directory -Path $directory | Out-Null
    }

    $bmp.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
}

$projectRoot = Get-ProjectRoot
if ($OutputHmi -eq '') {
    $OutputHmi = Join-Path $projectRoot 'tmp_hmi_probe_page1_bg_replace.HMI'
}
if ($LightPng -eq '') {
    $LightPng = Join-Path $projectRoot 'main\boards\bread-compact-wifi\ui_assets\page1_air_detail_hmi_blank_light.png'
}

if (-not (Test-Path -LiteralPath $SourceHmi)) {
    throw "Source HMI not found: $SourceHmi"
}

$sourceBytes = [System.IO.File]::ReadAllBytes($SourceHmi)
$resources = Find-PngResources $sourceBytes
if ($resources.Count -eq 0) {
    throw "No embedded PNG resources found in HMI: $SourceHmi"
}
if ($TargetPngIndex -lt 0 -or $TargetPngIndex -ge $resources.Count) {
    throw "TargetPngIndex $TargetPngIndex is outside detected range 0..$($resources.Count - 1)"
}

New-LightAirDetailPng $LightPng
$lightInfo = Get-Item -LiteralPath $LightPng
$target = $resources[$TargetPngIndex]
if ($target.Width -ne 480 -or $target.Height -ne 272) {
    throw "Target resource index $TargetPngIndex is $($target.Width)x$($target.Height), expected 480x272"
}
if ($lightInfo.Length -gt $target.Length) {
    throw "Generated PNG is $($lightInfo.Length) bytes, larger than target slot $($target.Length) bytes"
}

$report = [ordered]@{
    source_hmi = $SourceHmi
    source_size = $sourceBytes.Length
    source_sha256 = Get-Sha256 $SourceHmi
    detected_png_count = $resources.Count
    target_png_index = $TargetPngIndex
    target_offset_hex = ('0x{0:X8}' -f $target.Offset)
    target_slot_length = $target.Length
    target_size = "$($target.Width)x$($target.Height)"
    generated_png = $LightPng
    generated_png_length = $lightInfo.Length
    generated_png_sha256 = Get-Sha256 $LightPng
    patched_copy = $null
    patched_copy_sha256 = $null
}

if ($PatchCopy) {
    [System.IO.File]::Copy($SourceHmi, $OutputHmi, $true)
    $replacement = [System.IO.File]::ReadAllBytes($LightPng)
    $outputBytes = [System.IO.File]::ReadAllBytes($OutputHmi)
    [Array]::Copy($replacement, 0, $outputBytes, $target.Offset, $replacement.Length)

    for ($i = $target.Offset + $replacement.Length; $i -lt $target.Offset + $target.Length; $i++) {
        $outputBytes[$i] = 0
    }

    [System.IO.File]::WriteAllBytes($OutputHmi, $outputBytes)
    $report.patched_copy = $OutputHmi
    $report.patched_copy_sha256 = Get-Sha256 $OutputHmi
}

$resources |
    Select-Object Index,
                  @{Name='OffsetHex'; Expression={ '0x{0:X8}' -f $_.Offset }},
                  Length,
                  Width,
                  Height |
    Format-Table -AutoSize

$report | ConvertTo-Json -Depth 3

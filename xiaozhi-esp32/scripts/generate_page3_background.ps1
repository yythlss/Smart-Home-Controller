param(
    [string]$OutPath = (Join-Path $PSScriptRoot '..\main\boards\bread-compact-wifi\ui_assets\page3_ai_settings_hmi_manual_env.png')
)

Add-Type -AssemblyName System.Drawing

$width = 480
$height = 272
$bitmap = New-Object System.Drawing.Bitmap($width, $height)
$graphics = [System.Drawing.Graphics]::FromImage($bitmap)
$graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
$graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::ClearTypeGridFit

function New-SolidBrush([string]$hex) {
    return New-Object System.Drawing.SolidBrush([System.Drawing.ColorTranslator]::FromHtml($hex))
}

function New-PenBrush([string]$hex, [float]$size) {
    return New-Object System.Drawing.Pen([System.Drawing.ColorTranslator]::FromHtml($hex), $size)
}

function New-RoundRectPath([float]$x, [float]$y, [float]$w, [float]$h, [float]$r) {
    $path = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $r * 2
    $path.AddArc($x, $y, $d, $d, 180, 90)
    $path.AddArc($x + $w - $d, $y, $d, $d, 270, 90)
    $path.AddArc($x + $w - $d, $y + $h - $d, $d, $d, 0, 90)
    $path.AddArc($x, $y + $h - $d, $d, $d, 90, 90)
    $path.CloseFigure()
    return $path
}

function Fill-RoundRect([float]$x, [float]$y, [float]$w, [float]$h, [float]$r, [string]$color) {
    $path = New-RoundRectPath $x $y $w $h $r
    $brush = New-SolidBrush $color
    $graphics.FillPath($brush, $path)
    $brush.Dispose()
    $path.Dispose()
}

function Stroke-RoundRect([float]$x, [float]$y, [float]$w, [float]$h, [float]$r, [string]$color, [float]$size) {
    $path = New-RoundRectPath $x $y $w $h $r
    $pen = New-PenBrush $color $size
    $graphics.DrawPath($pen, $path)
    $pen.Dispose()
    $path.Dispose()
}

function Draw-Text([string]$text, [System.Drawing.Font]$font, [string]$color, [float]$x, [float]$y) {
    $brush = New-SolidBrush $color
    $graphics.DrawString($text, $font, $brush, $x, $y)
    $brush.Dispose()
}

function Draw-CenteredText([string]$text, [System.Drawing.Font]$font, [string]$color, [float]$x, [float]$y, [float]$w, [float]$h) {
    $brush = New-SolidBrush $color
    $format = New-Object System.Drawing.StringFormat
    $format.Alignment = [System.Drawing.StringAlignment]::Center
    $format.LineAlignment = [System.Drawing.StringAlignment]::Center
    $graphics.DrawString($text, $font, $brush, (New-Object System.Drawing.RectangleF($x, $y, $w, $h)), $format)
    $format.Dispose()
    $brush.Dispose()
}

function Join-Chars {
    param(
        [Parameter(ValueFromRemainingArguments = $true)]
        [int[]]$CodePoint
    )
    return -join ($CodePoint | ForEach-Object { [char]$_ })
}

$txtHome = ([string][char]0x2039) + ' ' + (Join-Chars 0x9996 0x9875)
$txtTitle = 'AI ' + (Join-Chars 0x4E0E 0x8BBE 0x7F6E)
$txtAiState = 'AI ' + (Join-Chars 0x72B6 0x6001)
$txtLinkState = Join-Chars 0x4E32 0x53E3 0x5C4F
$txtManual = Join-Chars 0x624B 0x52A8
$txtGood = Join-Chars 0x8212 0x9002
$txtHot = Join-Chars 0x9AD8 0x6E29
$txtDry = Join-Chars 0x5E72 0x71E5
$txtBad = Join-Chars 0x6C61 0x67D3
$txtTip = (Join-Chars 0x5DE6 0x53F3 0x6ED1 0x52A8 0x5207 0x6362 0x9875 0x9762) + ' - ' + (Join-Chars 0x70B9 0x51FB 0x6309 0x94AE 0x6A21 0x62DF 0x73AF 0x5883)

$fontTitle = New-Object System.Drawing.Font('Microsoft YaHei UI', 24, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
$fontCardTitle = New-Object System.Drawing.Font('Microsoft YaHei UI', 19, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
$fontSmallBold = New-Object System.Drawing.Font('Microsoft YaHei UI', 14, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
$fontButton = New-Object System.Drawing.Font('Microsoft YaHei UI', 18, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
$fontTip = New-Object System.Drawing.Font('Microsoft YaHei UI', 12, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)

$bg = New-SolidBrush '#F3EFE7'
$graphics.FillRectangle($bg, 0, 0, $width, $height)
$bg.Dispose()

Fill-RoundRect 14 13 112 30 15 '#E8DFD6'
Draw-Text $txtHome $fontSmallBold '#4C4036' 31 19
Draw-CenteredText $txtTitle $fontTitle '#111111' 134 16 212 34

Fill-RoundRect 28 63 424 56 8 '#FFFDF8'
Stroke-RoundRect 28 63 424 56 8 '#E9E1D7' 1
Fill-RoundRect 44 79 28 28 8 '#E9F2FF'
Draw-CenteredText 'AI' $fontSmallBold '#2E6AB3' 44 80 28 26
Draw-Text $txtAiState $fontCardTitle '#121212' 88 80
Fill-RoundRect 242 75 195 36 8 '#F4F0E9'
Stroke-RoundRect 242 75 195 36 8 '#E3DAD0' 1

Fill-RoundRect 28 127 424 54 8 '#FFFDF8'
Stroke-RoundRect 28 127 424 54 8 '#E9E1D7' 1
Fill-RoundRect 44 141 28 28 8 '#EAF8F0'
Draw-CenteredText 'IO' $fontSmallBold '#2D7A48' 44 142 28 26
Draw-Text $txtLinkState $fontCardTitle '#121212' 88 144
Fill-RoundRect 242 138 195 36 8 '#F4F0E9'
Stroke-RoundRect 242 138 195 36 8 '#E3DAD0' 1

$buttons = @(
    @{ X = 36;  Y = 184; W = 84; H = 42; Label = $txtManual; Color = '#356AE6'; Text = '#FFFFFF' }
    @{ X = 132; Y = 184; W = 70; H = 42; Label = $txtGood; Color = '#2F9E65'; Text = '#FFFFFF' }
    @{ X = 214; Y = 184; W = 70; H = 42; Label = $txtHot; Color = '#F2994A'; Text = '#3B2A1C' }
    @{ X = 296; Y = 184; W = 70; H = 42; Label = $txtDry; Color = '#C17C4A'; Text = '#FFFFFF' }
    @{ X = 378; Y = 184; W = 70; H = 42; Label = $txtBad; Color = '#D94C4C'; Text = '#FFFFFF' }
)

foreach ($button in $buttons) {
    Fill-RoundRect $button.X $button.Y $button.W $button.H 8 $button.Color
    Stroke-RoundRect $button.X $button.Y $button.W $button.H 8 '#FFFFFF' 1
    Draw-CenteredText $button.Label $fontButton $button.Text $button.X $button.Y $button.W $button.H
}

Fill-RoundRect 28 236 424 22 7 '#FFFDF8'
Draw-CenteredText $txtTip $fontTip '#7B6C60' 28 236 424 22

$dotBrush = New-SolidBrush '#D6CABE'
$activeDotBrush = New-SolidBrush '#6E6256'
foreach ($x in @(220, 232, 244)) {
    $graphics.FillEllipse($dotBrush, $x, 260, 5, 5)
}
$graphics.FillEllipse($activeDotBrush, 256, 260, 5, 5)
$dotBrush.Dispose()
$activeDotBrush.Dispose()

$outDir = Split-Path -Parent $OutPath
if (-not (Test-Path -LiteralPath $outDir)) {
    New-Item -ItemType Directory -Path $outDir | Out-Null
}

$bitmap.Save($OutPath, [System.Drawing.Imaging.ImageFormat]::Png)

$fontTitle.Dispose()
$fontCardTitle.Dispose()
$fontSmallBold.Dispose()
$fontButton.Dispose()
$fontTip.Dispose()
$graphics.Dispose()
$bitmap.Dispose()

Write-Output $OutPath

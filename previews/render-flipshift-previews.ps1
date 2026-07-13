$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

$outDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$referenceSpectrogram = Join-Path $outDir 'reference-spectrogram.png'
$variants = @(
    @{ Name = 'flipshift-compact.png'; Title = 'Compact'; Mode = 'Mirror'; Pivot = 0.39 },
    @{ Name = 'flipshift-analyzer.png'; Title = 'Analyzer-first'; Mode = 'Shepard W'; Pivot = 0.43 },
    @{ Name = 'flipshift-transform-map.png'; Title = 'Transform Map'; Mode = 'Comb'; Pivot = 0.35 }
)

function Clamp01([double]$value) {
    return [Math]::Max(0, [Math]::Min(1, $value))
}

function Transform-Frequency([double]$f, [hashtable]$variant) {
    $pivot = [double]$variant.Pivot
    switch ($variant.Mode) {
        'Mirror' { return $pivot - (($f - $pivot) * 1.12) }
        'Comb' { return $f }
        'Shepard W' { return $f }
        default { return $f }
    }
}

function Energy-At([double]$x, [double]$y, [hashtable]$variant) {
    $f = Transform-Frequency (1.0 - $y) $variant
    $vibrato = [Math]::Sin($x * 18.0) * 0.006
    $energy = 0.035 + [Math]::Max(0, [Math]::Sin($x * 44.0 + $y * 9.0)) * 0.018

    foreach ($base in @(0.12, 0.15, 0.18, 0.225, 0.27, 0.33, 0.40, 0.49, 0.60, 0.73)) {
        for ($harmonic = 1; $harmonic -le 13; $harmonic++) {
            $line = $base * $harmonic * 0.082 + $vibrato
            if ($line -gt 0.98) { continue }
            $distance = [Math]::Abs($f - $line)
            $band = [Math]::Exp(-($distance * $distance) / 0.000055)
            $energy += $band * (0.34 / [Math]::Sqrt($harmonic))
        }
    }

    foreach ($pulse in @(0.07, 0.18, 0.31, 0.46, 0.62, 0.78, 0.91)) {
        $energy += [Math]::Exp(-[Math]::Pow($x - $pulse, 2) / 0.00018) * (0.08 + [Math]::Max(0, 0.9 - $y) * 0.20)
    }

    $energy += [Math]::Exp(-[Math]::Pow($f - 0.18, 2) / 0.018) * 0.26
    $energy += [Math]::Exp(-[Math]::Pow($f - 0.32, 2) / 0.028) * 0.10

    if ($variant.Mode -eq 'Comb') {
        $energy *= 0.55 + 0.45 * [Math]::Max(0, [Math]::Cos($f * 95.0))
    }
    if ($variant.Mode -eq 'Shepard W') {
        $energy += [Math]::Max(0, [Math]::Sin([Math]::Log(0.08 + $f * 8.0, 2) * 12.0)) * 0.42
    }

    return Clamp01 ($energy * 0.58)
}

function Energy-Color([double]$energy) {
    $curve = [Math]::Pow($energy, 1.55)
    $r = [int](8 + $curve * 218)
    $g = [int](22 + $energy * 230)
    $b = [int](48 + [Math]::Pow($energy, 0.8) * 76)
    return [Drawing.Color]::FromArgb(255, $r, $g, $b)
}

function Draw-Spectrogram($g, [Drawing.Rectangle]$rect, [hashtable]$variant) {
    $bgBrush = New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(255, 14, 18, 20))
    $g.FillRectangle($bgBrush, $rect)
    $bgBrush.Dispose()

    if (Test-Path -LiteralPath $referenceSpectrogram) {
        $reference = [Drawing.Image]::FromFile($referenceSpectrogram)
        $oldMode = $g.InterpolationMode
        $g.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::NearestNeighbor

        $scale = [Math]::Max($rect.Width / [double]$reference.Width, $rect.Height / [double]$reference.Height)
        $drawW = [int]($reference.Width * $scale)
        $drawH = [int]($reference.Height * $scale)
        $drawX = $rect.X + [int](($rect.Width - $drawW) / 2)
        $drawY = $rect.Y + [int](($rect.Height - $drawH) / 2)
        $g.DrawImage($reference, $drawX, $drawY, $drawW, $drawH)

        if ($variant.Mode -eq 'Mirror') {
            $overlay = New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(42, 239, 173, 62))
            $g.FillRectangle($overlay, $rect.X, $rect.Y + [int]($rect.Height * 0.50), $rect.Width, [int]($rect.Height * 0.18))
            $overlay.Dispose()
        } elseif ($variant.Mode -eq 'Comb') {
            $combPen = New-Object Drawing.Pen ([Drawing.Color]::FromArgb(150, 16, 20, 22)), 3
            for ($y = $rect.Y + 12; $y -lt $rect.Bottom; $y += 28) { $g.DrawLine($combPen, $rect.X, $y, $rect.Right, $y) }
            $combPen.Dispose()
        } elseif ($variant.Mode -eq 'Shepard W') {
            $shepPen = New-Object Drawing.Pen ([Drawing.Color]::FromArgb(120, 84, 221, 183)), 2
            for ($x = $rect.X; $x -lt $rect.Right; $x += 46) { $g.DrawLine($shepPen, $x, $rect.Y, $x + 80, $rect.Bottom) }
            $shepPen.Dispose()
        }

        $g.InterpolationMode = $oldMode
        $reference.Dispose()
    } else {

        $smallW = [Math]::Max(1, [int]($rect.Width / 2))
        $smallH = [Math]::Max(1, [int]($rect.Height / 2))
        $heat = New-Object Drawing.Bitmap $smallW, $smallH
        for ($x = 0; $x -lt $smallW; $x++) {
            for ($y = 0; $y -lt $smallH; $y++) {
                $energy = Energy-At (($x / [double]$smallW)) (($y / [double]$smallH)) $variant
                $heat.SetPixel($x, $y, (Energy-Color $energy))
            }
        }
        $oldMode = $g.InterpolationMode
        $g.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $g.DrawImage($heat, $rect)
        $g.InterpolationMode = $oldMode
        $heat.Dispose()
    }

    $gridPen = New-Object Drawing.Pen ([Drawing.Color]::FromArgb(150, 52, 59, 63)), 1
    for ($i = 0; $i -le 12; $i++) {
        $x = [int]($rect.X + $i * $rect.Width / 12.0)
        $g.DrawLine($gridPen, $x, $rect.Y, $x, $rect.Bottom)
    }
    for ($i = 0; $i -le 8; $i++) {
        $y = [int]($rect.Y + $i * $rect.Height / 8.0)
        $g.DrawLine($gridPen, $rect.X, $y, $rect.Right, $y)
    }
    $gridPen.Dispose()

    $pivotPen = New-Object Drawing.Pen ([Drawing.Color]::FromArgb(255, 239, 173, 62)), 1
    $pivotY = [int]($rect.Bottom - $rect.Height * [double]$variant.Pivot)
    $g.DrawLine($pivotPen, $rect.X, $pivotY, $rect.Right, $pivotY)
    $pivotPen.Dispose()
}

function Draw-Slice($g, [Drawing.Rectangle]$rect, [hashtable]$variant) {
    $bgBrush = New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(255, 13, 17, 20))
    $g.FillRectangle($bgBrush, $rect)
    $bgBrush.Dispose()

    $path = New-Object Drawing.Drawing2D.GraphicsPath
    for ($x = 0; $x -lt $rect.Width; $x += 2) {
        $freq = $x / [double]$rect.Width
        $energy = Energy-At 0.64 (1.0 - $freq) $variant
        $y = $rect.Bottom - ($energy * 0.82 + 0.05) * $rect.Height
        if ($x -eq 0) { $path.StartFigure(); $path.AddLine($rect.X + $x, $y, $rect.X + $x + 1, $y) }
        else { $path.AddLine($rect.X + $x - 2, $prevY, $rect.X + $x, $y) }
        $prevY = $y
    }
    $pen = New-Object Drawing.Pen ([Drawing.Color]::FromArgb(255, 84, 221, 183)), 2
    $g.DrawPath($pen, $path)
    $pen.Dispose()
    $path.Dispose()
}

function Draw-Preview([hashtable]$variant) {
    $w = 1152
    $h = 620
    $bmp = New-Object Drawing.Bitmap $w, $h
    $g = [Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [Drawing.Drawing2D.SmoothingMode]::AntiAlias

    $bg = [Drawing.Color]::FromArgb(255,16,20,22)
    $panel = [Drawing.Color]::FromArgb(255,23,29,32)
    $line = [Drawing.Color]::FromArgb(255,52,59,63)
    $text = [Drawing.Color]::FromArgb(255,219,227,227)
    $muted = [Drawing.Color]::FromArgb(255,135,144,146)
    $mint = [Drawing.Color]::FromArgb(255,84,221,183)
    $amber = [Drawing.Color]::FromArgb(255,239,173,62)
    $g.Clear($bg)

    $fontBrand = New-Object Drawing.Font 'Arial', 17, ([Drawing.FontStyle]::Bold)
    $fontSmall = New-Object Drawing.Font 'Arial', 9
    $fontMid = New-Object Drawing.Font 'Arial', 11
    $brushMint = New-Object Drawing.SolidBrush $mint
    $brushText = New-Object Drawing.SolidBrush $text
    $brushMuted = New-Object Drawing.SolidBrush $muted
    $brushAmber = New-Object Drawing.SolidBrush $amber
    $penLine = New-Object Drawing.Pen $line, 1

    $g.DrawRectangle($penLine, 0, 0, $w - 1, $h - 1)
    $g.DrawLine($penLine, 0, 44, $w, 44)
    $g.DrawString('openFAD FLIPSHIFT', $fontBrand, $brushMint, 16, 12)
    $g.DrawString($variant.Title.ToUpper() + ' / SPECTRAL ' + $variant.Mode.ToUpper(), $fontSmall, $brushMuted, 850, 17)
    $g.FillEllipse($brushMint, 1126, 18, 8, 8)

    $specRect = New-Object Drawing.Rectangle 0, 45, $w, 350
    Draw-Spectrogram $g $specRect $variant
    $g.DrawString('PIVOT', $fontSmall, $brushAmber, 14, $specRect.Bottom - $specRect.Height * [double]$variant.Pivot - 18)

    $sliceRect = New-Object Drawing.Rectangle 0, 395, $w, 72
    Draw-Slice $g $sliceRect $variant

    $controlTop = 467
    $g.FillRectangle((New-Object Drawing.SolidBrush $panel), 0, $controlTop, $w, $h - $controlTop)
    $cols = @(170,150,120,120,120,120,120,120,132)
    $x0 = 0
    foreach ($col in $cols) { $g.DrawLine($penLine, $x0, $controlTop, $x0, $h); $x0 += $col }
    $labels = @('MODE',$variant.Mode,'SHIFT','+100 Hz','SCALE','1.25x','PIVOT','1.00k','AMOUNT','50%','WIDTH/Q','1.00','MIX','50%','FREEZE','OFF','OUTPUT','L/R')
    $x0 = 14
    for ($i=0; $i -lt $labels.Count; $i += 2) {
        $g.DrawString($labels[$i], $fontSmall, $brushMuted, $x0, $controlTop + 16)
        $g.DrawString($labels[$i+1], $fontMid, $(if ($labels[$i+1] -eq $variant.Mode) { $brushAmber } else { $brushText }), $x0, $controlTop + 48)
        $x0 += if ($i -eq 0) { 170 } else { 120 }
    }
    $g.FillRectangle($brushMint, 1078, $h - 82, 10, 58)
    $g.FillRectangle($brushMint, 1100, $h - 70, 10, 46)

    $file = Join-Path $outDir $variant.Name
    $bmp.Save($file, [Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose()
    $bmp.Dispose()
    Write-Host "Rendered $file"
}

foreach ($variant in $variants) { Draw-Preview $variant }

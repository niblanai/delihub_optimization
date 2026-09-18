$distPath = "E:\tifany\dist"
$invoiceDir = (Get-Item "F:\tifany\src\اذونات").FullName
$results = [System.Collections.Generic.List[string]]::new()

$pdfs = Get-ChildItem -LiteralPath $invoiceDir -Filter "*.pdf"
Write-Host "Found $($pdfs.Count) PDFs"

foreach ($pdf in $pdfs) {
    $safeName = [System.Text.RegularExpressions.Regex]::Replace($pdf.BaseName,'[^\w]','_')
    $imgBase = "$env:TEMP\ocr_$safeName"
    
    # Step 1: PDF to PNG
    & "$distPath\pdftoppm.exe" -r 300 -png -f 1 -l 1 $pdf.FullName $imgBase 2>$null
    $imgFile = "$imgBase-1.png"
    if (-not (Test-Path $imgFile)) {
        $results.Add("FAILED (no image): $($pdf.Name)")
        continue
    }
    
    # Step 2: Enhance
    $enhanced = "$imgBase`_enh.png"
    & "$distPath\magick.exe" $imgFile -colorspace Gray -contrast-stretch 0 -sharpen 0x1.5 -crop "2481x1400+0+450" +repage $enhanced 2>$null
    if (-not (Test-Path $enhanced)) { $enhanced = $imgFile }
    
    # Step 3: OCR
    $ocrOut = "$imgBase`_ocr"
    & "$distPath\tesseract.exe" $enhanced $ocrOut -l ara+eng --psm 4 --oem 1 "--tessdata-dir" "$distPath\tessdata" 2>$null
    
    $lines = Get-Content "$ocrOut.txt" -Encoding UTF8 -ErrorAction SilentlyContinue
    $productLines = $lines | Where-Object { $_ -match '[\u0600-\u06FF]' -and $_ -match '\d+[\.,]\d+' }
    
    $results.Add("=== $($pdf.Name) ===")
    foreach ($l in $productLines) { $results.Add("  $l") }
    if (-not $productLines) { $results.Add("  (no product lines found)") }
    
    Remove-Item "$imgFile","$enhanced","$ocrOut.txt" -ErrorAction SilentlyContinue
}

$results | Set-Content "E:\tifany\ocr_test_results.txt" -Encoding UTF8
Write-Host "Done. $($results.Count) lines written."

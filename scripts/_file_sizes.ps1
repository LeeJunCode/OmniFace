$releaseDir = "C:\Users\34341\Projects\OmniFace\build\Release"
Get-ChildItem $releaseDir -Recurse -File | Sort-Object Length -Descending | Select-Object -First 30 | ForEach-Object {
    $sizeMB = $_.Length / 1MB
    $relPath = $_.FullName.Replace($releaseDir + "\", "")
    Write-Host ("{0,8:N1} MB  {1}" -f $sizeMB, $relPath)
}

Write-Host ""
$totalMB = (Get-ChildItem $releaseDir -Recurse -File | Measure-Object -Property Length -Sum).Sum / 1MB
Write-Host ("Total: {0:N0} MB" -f $totalMB)

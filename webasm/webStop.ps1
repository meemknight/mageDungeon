$port = 8000
$found = $false

$processes = Get-CimInstance Win32_Process | Where-Object {
	$_.CommandLine -and (
		$_.CommandLine -like '*webServe.py*' -or
		$_.CommandLine -like '*http.server*'
	)
}

foreach ($process in $processes) {
	Write-Host "Stopping web server process $($process.ProcessId)..."
	Stop-Process -Id $process.ProcessId -Force -ErrorAction SilentlyContinue
	$found = $true
}

$listeners = Get-NetTCPConnection -State Listen -LocalPort $port -ErrorAction SilentlyContinue |
	Select-Object -ExpandProperty OwningProcess -Unique

foreach ($pid in $listeners) {
	Write-Host "Stopping listener PID $pid on port $port..."
	Stop-Process -Id $pid -Force -ErrorAction SilentlyContinue
	$found = $true
}

if (-not $found) {
	Write-Host "No local web server found."
} else {
	Write-Host "Done. Local web server stopped."
}

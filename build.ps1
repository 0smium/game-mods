[CmdletBinding()]
param([string]$SmtvfixSource = '', [string]$BuildDirectory = 'build')
$ErrorActionPreference = 'Stop'
$cmakePath = (Get-Command cmake -ErrorAction Stop).Source
function Invoke-BuildCommand([string[]] $Arguments) {
    $start = [System.Diagnostics.ProcessStartInfo]::new()
    $start.FileName = $cmakePath
    $start.WorkingDirectory = $PSScriptRoot
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    foreach ($argument in $Arguments) { $start.ArgumentList.Add($argument) }
    # MSBuild's .NET Framework host rejects duplicate PATH/Path entries inherited
    # through some shells. Normalize this child process only; never edit user or
    # machine environment variables.
    $normalized = [Collections.Generic.Dictionary[string,string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($entry in [Environment]::GetEnvironmentVariables('Process').GetEnumerator()) {
        $normalized[[string]$entry.Key] = [string]$entry.Value
    }
    $start.Environment.Clear()
    foreach ($entry in $normalized.GetEnumerator()) {
        $key = if ($entry.Key -ieq 'Path') { 'Path' } else { $entry.Key }
        $start.Environment[$key] = $entry.Value
    }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo = $start
    if (-not $process.Start()) { throw 'Failed to start CMake' }
    $stdout = $process.StandardOutput.ReadToEndAsync()
    $stderr = $process.StandardError.ReadToEndAsync()
    $process.WaitForExit()
    $output = $stdout.GetAwaiter().GetResult() + $stderr.GetAwaiter().GetResult()
    Write-Output $output
    if ($process.ExitCode -ne 0) { throw "CMake failed with exit code $($process.ExitCode)" }
}

$arguments = @('-S', $PSScriptRoot, '-B', (Join-Path $PSScriptRoot $BuildDirectory), '-G', 'Visual Studio 17 2022', '-A', 'x64', '-DCMAKE_VS_GLOBALS=ImportDirectoryBuildProps=false;ImportDirectoryBuildTargets=false;AutoDeploy=false')
if ($SmtvfixSource) { $arguments += '-DSMTVFIX_SOURCE=' + $SmtvfixSource }
Invoke-BuildCommand $arguments
Invoke-BuildCommand @('--build', (Join-Path $PSScriptRoot $BuildDirectory), '--config', 'Release', '--parallel', '4', '--', '/p:ImportDirectoryBuildProps=false', '/p:ImportDirectoryBuildTargets=false', '/p:AutoDeploy=false')

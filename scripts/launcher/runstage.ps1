# Helper for run.bat: runs the command in env RS_CMD, streams its combined stdout+stderr line by line to the
# console AND appends it to the log as it arrives (no buffering until exit), prints an honest heartbeat
# (elapsed time only, never a percentage) while the command is silent, and exits with the command's exit code.
param([Parameter(Mandatory)][string]$Log, [int]$Heartbeat = 15)

$cmd = $env:RS_CMD
if (-not $cmd) { [Console]::Error.WriteLine('runstage: RS_CMD not set'); exit 99 }

$psi = New-Object System.Diagnostics.ProcessStartInfo
$psi.FileName = 'cmd.exe'
$psi.Arguments = '/d /s /c "' + $cmd + ' 2>&1"'
$psi.UseShellExecute = $false
$psi.RedirectStandardOutput = $true
$psi.CreateNoWindow = $true
$psi.StandardOutputEncoding = [Console]::OutputEncoding

$fs = New-Object System.IO.FileStream($Log, [IO.FileMode]::Append, [IO.FileAccess]::Write, [IO.FileShare]::ReadWrite)
$logw = New-Object System.IO.StreamWriter($fs, (New-Object System.Text.UTF8Encoding $false))
$logw.AutoFlush = $true
[Console]::Out.WriteLine('    > ' + $cmd)
$logw.WriteLine('> ' + $cmd)
$sw = [Diagnostics.Stopwatch]::StartNew()
$p = [Diagnostics.Process]::Start($psi)
$code = 1
try {
    $task = $p.StandardOutput.ReadLineAsync()
    $silent = [Diagnostics.Stopwatch]::StartNew()
    while ($true) {
        if ($task.Wait(1000)) {
            $line = $task.Result
            if ($null -eq $line) { break }
            [Console]::Out.WriteLine('    ' + $line)
            $logw.WriteLine($line)
            $silent.Restart()
            $task = $p.StandardOutput.ReadLineAsync()
        } elseif ($silent.Elapsed.TotalSeconds -ge $Heartbeat) {
            $msg = '    ... still running, no new output (elapsed {0:mm\:ss})' -f $sw.Elapsed
            [Console]::Out.WriteLine($msg)
            $logw.WriteLine($msg)
            $silent.Restart()
        }
    }
    $p.WaitForExit()
    $code = $p.ExitCode
} finally {
    if (-not $p.HasExited) { & taskkill /F /T /PID $p.Id *> $null }
    $logw.WriteLine(('--- exit code {0} after {1:N1}s' -f $code, $sw.Elapsed.TotalSeconds))
    $logw.Dispose()
}
[Console]::Out.WriteLine(('    elapsed {0:N1}s' -f $sw.Elapsed.TotalSeconds))
exit $code

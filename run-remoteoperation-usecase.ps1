# run_all_interactive.ps1

# --- CONFIGURATION ---
# IMPORTANT: Update these paths to point to your actual files!
$cppExePath     = "C:\Users\iwsla\source\repos\Xinput-test\x64\Debug"
$pythonScriptPath = "C:\git\5GRobotControl\plc-target-sensor\pyadsclient"
$eventFileOutputPath = "C:\Users\iwsla\Downloads\git-Jing\temp-storage"


# --- SCRIPT LOGIC ---

# 1. Interactively ask the user for the output filename
$outputFileBase = Read-Host -Prompt "Please enter the base name for the output file (e.g., 'test_run_1')"

# 2. Interactively ask the user for the network setting
Write-Host "Please select the network setting:"
Write-Host "1. Wired"
Write-Host "2. 5G"
$choice = Read-Host -Prompt "Enter your choice (1 or 2)"

# 3. Set the server IP address based on the user's choice
$serverIpAddress = ""
$serverport = ""
$networkTypeName = ""
switch ($choice) {
    "1" { 
        $serverIpAddress = "12.1.1.136" 
		$serverport = "17998"
        $networkTypeName = "Wired"
    }
    "2" { 
        $serverIpAddress = "10.10.0.162" 
		$serverport = "17998"
        $networkTypeName = "5G"
    }
    default {
        Write-Host "Invalid choice. Exiting."
        exit
    }
}

# 3. Get the current date and time for a unique timestamp
$timestamp = Get-Date -Format "yyyy-MM-dd_HH-mm-ss"
$fullOutputName = "${outputFileBase}_${timestamp}"

Write-Host "--------------------------------------------------"
Write-Host "Starting execution with the following settings:"
Write-Host "Output File Name: $fullOutputName"
Write-Host "Network Setting:  $networkSetting"
Write-Host "--------------------------------------------------"


# 4. Execute the C++ program and pass the output name and setting as arguments in the background
$cppExe = "${cppExePath}\Xinput-test.exe"
Write-Host "Running C++ program: $cppExe"
# The $using: scope is needed to pass local variables to the job's script block
# The '&' is the call operator in PowerShell, used to execute commands/executables.
Start-Job -ScriptBlock { 
    # when C++ is executing in powershell background, it will set the default working directory to C:\Users\YourName or other place
	# Use Set-Location to locate the right output file path
	Set-Location $using:eventFileOutputPath
	& $using:cppExe "-o" $using:fullOutputName "-a" $using:serverIpAddress "-p" $using:serverport
}
# & $cppExe "-o" $fullOutputName "-a" $serverIpAddress "-p" $serverport

# Another option : Start the C++ program in a new console window using Start-Process
# This will allow you to see its output live.
# $cppArgumentList = "$fullOutputName $serverIpAddress"
# Start-Process -FilePath $cppExePath -ArgumentList $cppArgumentList



# 5. Execute the Python script and pass the output name and setting as arguments in the background
# This assumes 'python' is in your system's PATH environment variable.
Write-Host "Running Python script: $pythonScriptPath"
$pythonscript = "${pythonScriptPath}\pyadsclient2.py"
# Start the Python script as a background job
Start-Job -ScriptBlock { 
    # python is smart to handle the working directory
	python $using:pythonscript $using:fullOutputName
}
# python $pythonscript $fullOutputName

# 6. Wait for both jobs to complete and display their output
Write-Host "Waiting for processes to finish..."
Get-Job | Wait-Job | Receive-Job

# 7. Move the output event file
# mv .\"${fullOutputName}_cpp-event-timestamp.csv" $eventFileOutputPath
mv "${pythonScriptPath}\${fullOutputName}_py-event-timestamp.csv" $eventFileOutputPath

Write-Host "All processes finished."
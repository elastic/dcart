# DCART - Decoupled Components for Automated Ransomware Testing
This project provides a framework for capturing relevant event data from ransomware sample detonations. The captured data may then be analyzed at a later point without the need for further detonations for the given samples. The analysis component will produce a score to indicate the extent of anomalous, ransomware-like behavior observed in the event data.

## DISCLAIMER
Please keep in mind that this is proof of concept code that has been made available for research purposes only. This code and / or project SHOULD NOT be used in a production testing environment.

## Setup / Dependencies
Windows 7+ (Built under Win7x64 SP1)  
Visual Studio (Built with VS2015 Pro)  
Windows Driver Kit (Built with 10.0.15063.0)  
Python 3 (Tested with 3.4.4)

Easiest way to build the driver is to create a new project based off of a template: Templates->Visual C++->Windows Driver->Filter Driver: Filesystem Mini-filter

Once the project has been created, swap out the main source file (FsFilter.c) with DCART.c. You should be able to initiate a build at this point, but it will likely fail due to the contents of the auto-generated inf file. Follow the steps in the comments for properly setting the Class and ClassGuid fields then rebuild the solution. If everything was done correctly, the rebuild should complete successfully.

To install the driver, navigate to your project build directory; this should contain both the driver (FsFilter.sys) and inf (FsFilter.inf). Right-click on the inf and select Install, which will copy the driver over to C:\Windows\System32\drivers and create a corresponding service for the driver. To start the driver service, run the following from a Windows command prompt: sc start FsFilter

To confirm the service has been started, WinDbg or DebugView may be used to view output from the driver.

After confirming the driver is properly working, you may start the collector component (ensuring that you're invoking the correct version of the python 3 interpreter): python collector.py

At this point, both the driver and collector components will be producing log output. Once you have confirmed that your virtualized environment is suitable for detonating ransomware, proceed to launch any sample you wish to observe. By default, the collector component will end execution after observing 1000 unique file change events, but this can be adjusted in the script or by simply terminating the process early. Before reverting your virtualized environment to its previous clean state, copy the event trace file (default path: C:\python_log\python_log.dcart) to a safe location either on the host or in a shared directory accessible across reverts.

To analyze the event trace file, copy it over to your clean environment and execute the following: python analyzer.py python_log.dcart

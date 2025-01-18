:: First, start VcXsrv. This will depend on where you installed it.
:: Might look at the options available (with .\vcxsrv.exe -logfile "C:\Users\username\tmp.txt" -help).
:: Those most suspicous of these is '-ac', which disables access control to the X server (because I'm too lazy to figure it out properly).
:: There are guides online about using VcXsrv with WSL and how to set up '-auth' or whatever.
start /b "" "C:\Program Files\VcXsrv\vcxsrv.exe" -ac -multiwindow -nocursor

:: Runs the game. The first part you may have to change based on where you installed the game (for me it's ~/bittoss).
:: The second part (specifically DISPLAY=) you might have to change depending on what IP Windows shows itself to WSL over.
::   I think the value below is the default? You can find it from WSL via 'ip r' and lookig for the part after 'via'.
::   (and then append ':0')
:: The bit after ./game is just the host you're connecting to.
ubuntu.exe run "cd ~/bittoss; DISPLAY=192.168.128.1:0 ./game 127.0.0.1"

:: kill VcXsrv now that we're done with it.
taskkill /f /im vcxsrv.exe

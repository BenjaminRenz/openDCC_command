# openDCC_command
One Arduino and four potientiometers are the low cost solution to profit from your digitalized railroad, because you now can controll 4 lockos simultaneousely. With four dedicated potentiometers you can give your openDCC-Controller the commands.

# Material
4x Potentiometer
5x Pushbutton
1x Arduino Nano* 
3x LED
1x LCD Display 16 characters and 2 rows (LED backlight)
(* Arduino Uno would be possible too but needs minor modifications of EEPROM storage (because EEPROM has a different size) and Analog Pins (for potentiometer input, should be easy because you would only need to change some pin definitions))

# FAQ
What can you do with it?
-control four locomotives in parralel
-change the Loco-address which is controlled by the Potentiometer
-enabkle/disable the front light and functions F1 - F4
-design LOC-icons which are displayed as custom characters on the LCD

What is stored in the EEPROM?
-the last four addresses which have been in use (so that you can start driving right away after disconnecting the power)
-Loc icons of the first 100 trains (for each LOC two characters 5pixel * 8rows are stored)

What does which button do?
-"STOP":             Button initiates an emergency stop and gets you back to normal operation
-"LOC_Switch":       Button cycles through the four locs which you controll simultaneousely
-"Left":             Self explaining
-"Right":               "
-"Menu":             Cycles through the menus: Normal Operation, Address change, Function change, Icon change and back to Normal Op
-"Left+Right" = "Up":toggles bits  or  increases digits / decreases them

# If you have any questions, trouble getting it to work, improvements or found a bug please contact me :)

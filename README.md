# PN5180-Library

Arduino Uno / Arduino ESP-32 library for PN5180-NFC Module from NXP Semiconductors

![PN5180-NFC module](./doc/PN5180-NFC.png)
![PN5180 Schematics](./doc/FritzingLayout.jpg)

Release Notes:

Version 1.9 - 05.10.2021

	* avoid endless loop in reset()
	
Version 1.8 - 05.04.2021

	* Revert previous changes, SPI class was copied and caused problems
	* Speedup reading (shorter delays) in reset/transceive commands
	* better initialization for ISO-14443 cards, see https://www.nxp.com.cn/docs/en/application-note/AN12650.pdf
	
Version 1.7 - 27.03.2021

	* allow to setup with other SPIClass than default SPI, thanks to @tyllmoritz !
	
Version 1.6 - 31.01.2021

	* fix compiler warnings for platform.io
	* add LPCD (low power card detection) example for ESP-32 (with deep sleep tp save battery power)

	Version 1.5 - 07.12.2020

	* ISO-14443 protocol, basic support for Mifaire cards
	* Low power card detection
	* handle transceiveCommand timeout

Version 1.4 - 13.11.2019

	* ICODE SLIX2 specific commands, see https://www.nxp.com/docs/en/data-sheet/SL2S2602.pdf
	* Example usage, currently outcommented

Version 1.3 - 21.05.2019

	* Initialized Reset pin with HIGH
	* Made readBuffer static
	* Typo fixes
	* Data type corrections for length parameters

Version 1.2 - 28.01.2019

	* Cleared Option bit in PN5180ISO15693::readSingleBlock and ::writeSingleBlock

Version 1.1 - 26.10.2018

	* Cleanup, bug fixing, refactoring
	* Automatic check for Arduino vs. ESP-32 platform via compiler switches
	* Added open pull requests
	* Working on documentation

Version 1.0.x - 21.09.2018

	* Initial versions

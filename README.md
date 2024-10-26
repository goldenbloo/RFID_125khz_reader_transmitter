# RFID_125khz_reader_transmitter
Implementation of a 125 kHz RFID tag reader and transmitter. It utilizes the EM4100 protocol with Manchester encoding, using 64 cycles per bit.<br>
The code is written in C for the Atmega8A and compiled using the avr-gcc compiler.<br>
The schematic is based on Vassilis Serasidis's project: 125 kHz RFID Reader https://www.serasidis.gr/circuits/RFID_reader/125kHz_RFID_reader.htm
## Hardware
I used a coil from an RFID tag with an inductance of approximately 3.3 mH, paired with a 490 pF capacitor to achieve a resonant frequency of 125 kHz.<br> 
If you use a different coil or capacitor, be sure to create a resonant tank circuit.<br>
The transistors I used were any generic ones I could find on hand.
##
![Schematics diagram](./img/schema.png)

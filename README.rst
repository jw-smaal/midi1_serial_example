===========================================
Zephyr MIDI 1.0 serial UART implementation
SPDX-License-Identifier: Apache-2.0
===========================================

A hardware MIDI 1.0 serial UART implementation

---------------------------------------
Features
---------------------------------------
* API for channel and common mode messages 
* Callbacks for parsed MIDI1.0 messages 
* Configurable settings for the receive buffers 
* Timeout for running status is configurable

---------------------------------------
Hardware Requirements
---------------------------------------
Tested on:

* **NXP FRDM-MCXC242**
* **NXP FRDM-K64F** 
* **NXP FRDM-MCXA156** 

 tested with various hardware synthesizers.

---------------------------------------
Building
---------------------------------------
Standard Zephyr build:

.. code-block:: sh

   west build -b frdm_mcxc242 -p always

---------------------------------------
Running
---------------------------------------
* Be carefull with logging to the console depending on the board
  wireing this may go out the MIDI1.0 port.   
* The popular Arduino MIDI shields are not designed for 3.3V drive.  With 
  lower power uarts you are adviced to use an output buffer that can sink the
  5 mA current loop.    

---------------------------------------
License
---------------------------------------
Apache-2.0

---------------------------------------
Author
---------------------------------------
Jan-Willem Smaal <usenet@gispen.org>

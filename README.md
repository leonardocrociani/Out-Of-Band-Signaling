# Out-Of-Band Signaling
A system for out-of-band signaling.
This is a technique of communication in which two entities exchange information without transmitting it to each other directly, but using collateral signaling: for example, the number of "artificial" errors, or the length of a packet full of useless data, or even the exact timing of communications. 

## The implementation
It is a client-server system, in which clients possess a confidential code (which we will call secret) and want to communicate it to a central server, but without transmitting it. The goal is to make it difficult for those who are capturing data in transit to intercept the secret. 

In this project there are C client, S server, and 1 supervisor. 

The supervisor is launched first, with an S parameter indicating how many servers we want to activate; 
the supervisor will then create the S servers (which will have to be separate processes). 
The C clients, on the other hand, are launched independently, each at different times. 

The various parameters are modifiable in the Makefile script.

All the code is POSIX compliance.

## Requirements
To run the code, you have to install:
<ul>
  <li>gcc</li>
  <li>make</li>
</ul>

## Commands

In the file directory, run:  <br> <br>
``` make ```  - to compile all the files <br> <br>
``` make test ```  - to test the programm on your pc <br> <br>
``` make clean ```  - to remove compiled and temporary files from the directory <br> <br>

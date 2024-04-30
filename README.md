# gheith_final_project
Our final project for our operating systems class. 

https://github.com/VendelinSlezak/BleskOS/blob/master/source/drivers/sound/ac97.c

(1) What did you plan on doing?
We initially planned on using the AC97 device to play ADPCM audio files.

(2) What did you end up doing?
We ended up using the AC97 device by using the PCI to grab the device to play .wav audio files.
Configuring and initializing an audio driver for the Intel AC97 sound card to play .wav files. Searching
the PCI bus to find the sound card and configuring it for our certain spec of .wav files.
Implementing a user system call for users to play audio for their given .wav file.

(3) What did you learn?
I learned how .wav files are formatted. I learned the structure of registers from AC97 and how buffers in AC97 are utilized to play audio data.
The amount of buffers that can be read at once by AC97 is 32 and there are two registers you have to write to in order to tell AC97 which buffers to read.
The first register is the address of a list of buffers and the 2nd register is the last buffer the AC97 hardware should read (this number is 0 indexed 
so i would pass 31 to read 32 buffers). Also, in order to transfer/play multiple lists of buffers back to back I had to reset a transfer control bit.

(4) How much did each team member contribute?
The work was perfectly split 50/50

(5) How can we get/run your code?

You must be on a machine that has audio turned on (On the lab machines we found out you had to increase the qemu volume of the machine
as soon as qemu is booted up during the program). In the terminal run "./run_qemu t0" and that will play the 
testcase we have created which is set to the audio file we have in the t0 directory.

We made a screengrab of us running the program with some audio files we had. As you will hear in the recording, when a file bigger than
the max size of 32 buffers is played, we have to refill buffers and there is a half-second pause in the playback. We did not add the 2nd larger file
to the git repo because of its larger size.

Link: https://drive.google.com/file/d/1H6CH4hXLT_EIm1jBsUPprSuJavOluKaZ/view?usp=sharing
Demonstration of how to play audio.

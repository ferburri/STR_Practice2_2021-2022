# STR_Practice2_2021-2022

The main objective is to build a sound card that plays Pulse Code Modulation audio files (PCM) using an
Arduino that gets the data form a Raspberry Pi with RTEMS connected by the I2C port. The program
should guarantee the response times for the melodies. This project will use a priority scheduler using **priority ceiling**.

This repository would have two parts that have different features:

- **First Part:** The first one allows playing PCM files coded with 4.000 samples per second, where each sample’s size is 1 bit (0 o 1).

- **Second Part:** The second one allows playing PCM files coded with 4.000 samples per second, where each sample’s size is 8 bit (between 0 and 255).

If you have any doubts about PCM files, I recommend a webpage that explains the behaviour of PCM files. Link: https://planethifi.com/pcm-audio/

## Structure

The structure of this project is divided into two modules: 

- **Arduino Module:** This module will have different tasks to perform in a certain period.

    1. Receive data from the music files using the I2C port. This task should receive 128 bytes using the I2C port connected to the Raspberry Pi (RTEMS).

        This tasks should be repeated every 256 milliseconds (approx. four times per second).
        
    2. To play back the songs received:

        - To reproduce a sample, the system receives a byte from the file (read from the internal buffer) and then it should separate the 8 samples to play back each one independently. To do that, we are going to use this method:
        
           `digitalWrite(soundPin, (data & mask));`
           
           Where the mask is a byte with 0s except a 1 on the position of the sample (mask = 2^pos). Using the AND operation at the bit-level the result is zero if the sample is equal to 0 or bigger than zero if the sample was 1. 
           
           The algorithm to implement is the following (it must be repeated each 250 µsec)

    3. Read the button and change the playback mode. This task should sample N times per second if the button is pushed or not (the student should choose the value of N, but it is recommended at least 10 times per second to avoid noises). In case the button state goes from pushed to non-pushed, the playback mode should change from normal mode to “mute” and vice versa. (Remember that “mute” implies to change the actual sample for a zero value but that the bytes from the serial port must be read anyway)
    4. Show the playback mode using a LED: This task should turn on the LED if the system is in “mute” mode or turn it off otherwise. the student should choose the number of times per second to execute this task.


  I provide a schema with the arduino circuit to know the recommended pins to set the speaker and the LED: 

  ![image](https://user-images.githubusercontent.com/79408013/152519094-5d5c2913-53ae-4da7-85e4-4455214385be.png)


- **RTEMS Module (Raspberry):** This module will have different tasks that we have to perform periodically

    1. Read the audio file and send it to the serial port. This task should read 128 bytes of the audio file (opened before) and send them to the Arduino by writing them to the I2C port. 
      
     This task has to repeat itself each 256 milliseconds (approx. four times per second).

    2. Receive commands from the user to pause the play back. The task should receive the commands pause and resume:

        - Command Pause: It is sent using character ‘0’. When it is sent, the playback task should cease to reproduce the song. (Instead of the music data, zeros will be sent and the read of the audio file should stop).
        - Command Resume: It is sent using character ‘1’. When it is sent, the playback task should resume playing the song.
        
        The task has to consider that the minimum separation between two readings should be at least 2 seconds, executing the command if there is one.
    
    3.  Show the reproduction state. The task should show on the console, the reproduction state with the following messages:
    
        - **Reproduction paused:** when the play back is paused.
        - **Reproduction resumed:** when the play back is going on.

The second part is the same, but the arduino part need to read a file of 4000 samples with 8 bits each. 

## Source Files

I have provided some audio files to use in order to check the functionality of the system. 

/* Copy data from the microphone to the speaker.
  * 1. Check if a sample is ready (from the mic.)
  * 2. Read that sample, from both the left and right channels
  * 3. Write it to the left and write audio output channels (i.e. send to the speaker)
  */
#define AUDIO_BASE    0xFF203040
int main (void) {
   // Get a pointer to the start of the audio codec I/O memory map
   // This is an I/O interface address, so we declare it volatile to tell the compiler not to optimize
   //  (skip) or reorder any loads or stores done through it. 
   // We declare the pointer to be of int type, as it is 32-bits which matches the 32-bit width of 
   // the audio I/O interface 'registers'.
   volatile int * audio_ptr = (int *) AUDIO_BASE;

   // Intermediate values;
   int left, right, fifospace;

   // Infinite loop checking the 8-bit RARC ('read available right channel) value to see if 
   // there is at least one entry in the input FIFOs. If there is, copy it over to the output FIFO.
   // The speed at which new data shows up in the input FIFOs (which will be one new sample every 
   // 125 us given the CODEC's 8 kHz sampling rate) controls the speed at which we send data to 
   // the output FIFOs (i.e. we will send an output sample every 125 us, which is what we want as 
   // it matches the CODEC's 8 kHz D/A rate).

   while (1) {
      fifospace = *(audio_ptr + 1); // Read the audio port fifospace register
      
      if ((fifospace & 0x000000FF) > 0) {  // Check RARC to see if there is at least 1 input sample avail.
         // We only checked the right audio channel, but since the A/D hardware always samples the 
         // the left and right channels at the same time, there will be at least one sample available 
         // in each.
         left = *(audio_ptr + 2);   // Read the left data; removes one word from the left
                                                     // input FIFO.
         right = *(audio_ptr + 3);    // Same for right data

         // Store to the left and right data 'registers' to add this data to the output FIFOs
         *(audio_ptr + 2) = left;      // Insert the sample in the left output FIFO
         *(audio_ptr + 3) = right;    // Same for right channel
      }
   }    // End infinite copy loop
}
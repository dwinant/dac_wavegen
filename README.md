# dac_wavegen
D/A function generator for an ST32H743 (Nucleo)

This project uses the DAC (Digital to Analog Converter) module of an ST32H743 MCU to 
generate different types of waveforms.

Control is via the USB Debug Serial interface.

The commands include:
  help                                             display help information
  clear                                            clear waveform data
  add        [<wave>] [<freq> [<ampl> [<phase>]]]  add signal
  rate       <rate>                                set sample rate
  buffer                                           show buffer params

Waveform choices are:
  Sine
  Sawtooth
  Square
  Triangle

The default waveform is 'sine', so if you enter 'add 45k' it adds a 45k sine wave.

The DAC is used in single-ended mode and generates values from 0 to 3.3v, with the nominal 0 value being at 1.65 V.
Internally, the waveform points are generated on a -1.0 - +1.0 V scale, 
so an amplitude value of 1.0 would generate a waveform that reaches full scale. If you are going to produce waveforms at multiple frequencies and don't want them to get clipped, make sure the total amplitude is < 1.0.

The phase is represented as percentage of a full cycle, so a phase shift of pi radians (180 degrees) would be 
entered as 50.

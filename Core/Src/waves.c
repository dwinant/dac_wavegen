/*
 * waves.c
 *
 *  Created on: Dec 26, 2020
 *      Author: david.winant
 */
#include "output.h"
#include "waves.h"
#include "conio.h"
#include <math.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#define WAVE_BUFFER_SIZE			250000
#define DAC_FULL_SCALE				0xFFF0

typedef void (wavefunc) (uint32_t base_rate, float amplitude, int phase);
typedef void (cmdfunc) (int argc, char ** argv);

typedef struct {
	const char*		name;
	wavefunc *		function;
} waveform_type;


typedef struct {
	waveform_type	*type_of_waveform;
	uint32_t		frequency;
	uint32_t		sample_rate;
} waveform_definition;

typedef struct {
	const char*		name;
	const char*		help;
	const char*     help_args;
	cmdfunc *		function;
} command_definition;

// commands
void help (int argc, char ** argv);
void clear (int argc, char ** argv);
void sample_rate_cmd (int argc, char ** argv);
void add_freq (int argc, char ** argv);
void buffer (int argc, char ** argv);

// internal data
command_definition commands[] = {
		{ "help", "display help information", "", help },
		{ "clear", "clear waveform data", "", clear },
		{ "add",  "add signal",		"[<wave>] [<freq> [<ampl> [<phase>]]]", add_freq },
		{ "rate", "set sample rate","<rate>", sample_rate_cmd },
		{ "buffer","show buffer params", "", buffer },
};
static uint16_t		wave_buffer [WAVE_BUFFER_SIZE] = {0};
static uint32_t		wave_buffer_samples = WAVE_BUFFER_SIZE;

// pre-declared functions
static void MX_TIM6_SetFrequency (int f);
static void sawtooth (uint32_t base_rate, float amplitude, int phase);
static void sine (uint32_t base_rate, float amplitude, int phase);
static void square (uint32_t base_rate, float amplitude, int phase);
static void triangle (uint32_t base_rate, float amplitude, int phase);
#ifdef INCLUDE_VT
static void VT (uint32_t base_rate, uint32_t sample_rate, uint32_t samples);
#endif

static waveform_type waveform_types[] = {
		{ "Sine",     sine     },
		{ "Sawtooth", sawtooth },
		{ "Square",   square   },
		{ "Triangle", triangle },
#ifdef INCLUDE_VT
		{ "VT",       VT       },
#endif
		};


#if 0
static waveform_type wv_sawtooth = { "Sawtooth", sawtooth };
static waveform_type wv_sine     = { "Sine",     sine     };
static waveform_type wv_square   = { "Square",   square   };
static waveform_type wv_triangle = { "Triangle", triangle };
static waveform_type wv_vt       = { "VT",       VT       };

static waveform_definition	samples[] = {
		{ &wv_vt,		 2000, 480000 },
		{ &wv_sine,      3333, 99990*2},
		{ &wv_sawtooth, 10000, 200000 },
		{ &wv_triangle,  2000, 200000 },
		{ &wv_square,    5000,  50000 },
};
#endif

uint32_t sample_rate = 500000;

void new_sample_rate (void)
{
	// stop DMA
	HAL_DAC_Stop_DMA (&hdac1, DAC_CHANNEL_1);

	output ("Sample rate is %d KHz, %d Ksamples\r\n", sample_rate/1000, wave_buffer_samples/1000);

	// set timer period
	MX_TIM6_SetFrequency (sample_rate);

	// restart DMA
	HAL_DAC_Start_DMA (&hdac1, DAC_CHANNEL_1, (uint32_t *) wave_buffer, wave_buffer_samples, DAC_ALIGN_12B_L);
}

void add_waveform (waveform_type* ptype, uint32_t frequency, float amplitude, int phase)
{
	output ("%s at %d KHz ampl %4dmv (phase %d %%)\r\n", ptype->name, frequency / 1000, (int)(amplitude*1000), phase);
	(ptype->function) (frequency, amplitude, phase);
	SCB_CleanDCache_by_Addr ((uint32_t*) wave_buffer, 2 * wave_buffer_samples);
}

#if 0
void new_waveform (waveform_type *ptype, uint32_t frequency, float amplitude)
{
	// stop DMA
	HAL_DAC_Stop_DMA (&hdac1, DAC_CHANNEL_1);

	// setup sample size & fill buffer
	uint32_t samples_per_cycle = sample_rate / frequency;
	if (samples_per_cycle == 0) {
		output ("bad sample rate - no samples per cycle\r\n");
		return;
	}

	// check if integral number of samples per cycle
	if (samples_per_cycle * frequency == sample_rate) {
		// if so, we can setup an integral number of cycles
		wave_buffer_samples = ((int) (WAVE_BUFFER_SIZE / samples_per_cycle)) * samples_per_cycle;
	} else {
		wave_buffer_samples = WAVE_BUFFER_SIZE;
	}

	output ("%s at %d KHz\r\n", ptype->name, frequency / 1000);
	(ptype->function) (frequency, amplitude);
	SCB_CleanDCache_by_Addr ((uint32_t*) wave_buffer, 2 * wave_buffer_samples);

	// set timer period
	MX_TIM6_SetFrequency (sample_rate);

	// restart DMA
	HAL_DAC_Start_DMA (&hdac1, DAC_CHANNEL_1, (uint32_t *) wave_buffer, wave_buffer_samples, DAC_ALIGN_12B_L);
}
#endif


#define ARG_NUM			5
#define ARG_SIZE		20



uint32_t atofreq (const char* a)
{
	int mult = 1;

	for (int i = 0; i < strlen(a); i++) {
		if (toupper(a[i]) == 'K') {
			mult = 1000;
			break;
		}
	}

	return mult * atoi (a);
}


char** setup_argv (int max_args, int max_arg_size)
{
	char** argv = (char**) malloc (max_args * sizeof(char*));

	for (int i = 0; i < max_args; i++) {
		argv[i] = (char*) malloc (max_arg_size);
	}
	return argv;
}


int parse_argv (const char* line, char** argv, int max_args, int max_arg_size)
{
	int	argc = 0;
	int end;
	int pos = 0;

	while (1) {
		while (isspace(line[pos])) pos++;
		if (line[pos] == 0) break;

		// locate end of word
		end = pos + 1;
		while (line[end] != 0 && !isspace (line[end])) end++;

		if (end-pos > max_arg_size-1) {
			output ("bad function\r\n");
			continue;
		}

		// save this argument
		strncpy (argv[argc], &line[pos], end-pos);
		argv[argc][end-pos] = 0;

		// check for maxed out args
		argc++;
		if (argc >= max_args) break;

		pos = end;
		if (line[pos] == 0) break;
	}

	return argc;
}


void waveforms (void)
{
	char **		argv;
	int			argc;
	char		linebuffer[200];
	int 		valid = 0;
	int 		i;

	help (0, NULL);
	argv = setup_argv (ARG_NUM, ARG_SIZE);
	clear (0, NULL);
	new_sample_rate();

	while (1) {
		output ("\r\nCommand: ");
		getline (linebuffer, sizeof(linebuffer));
		output ("\r\n");
		argc = parse_argv (linebuffer, argv, ARG_NUM, ARG_SIZE);
		if (argc < 1) continue;

		for (i = 0; i < NELEMENTS(commands); i++) {
			if (strnicmp (commands[i].name, argv[0], strlen(argv[0])) == 0) {
				valid = 1;
				break;
			}
		}

		if (!valid) {
			help (0, NULL);
		} else {
			commands[i].function (argc-1, argv+1);
		}
	}
}

static void MX_TIM6_SetFrequency (int f)
{
  const uint32_t MASTER_FREQ = 200000000;
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  if (f == 0) return;

  HAL_TIM_Base_Stop (&htim6);

  uint32_t period = MASTER_FREQ / f - 1;

  if (0) output ("for freq %d KHz, period is %d (0x%x)\r\n", f/1000, period, period);

  htim6.Instance = TIM6;
  htim6.Init.Prescaler = 0;
  htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim6.Init.Period = period;
  if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim6, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }

  HAL_TIM_Base_Start (&htim6);
}

#if 0
static uint16_t swap16 (uint16_t v)
{
	return ((v & 0xFF) << 8) | (v >> 8);
}
#endif


static uint16_t dac_point_set (int n, float f)
{
	if (f > 1.0) f = 1.0;
	if (f < -1.0) f = -1.0;

	uint16_t val = (f + 1.0) / 2.0 * DAC_FULL_SCALE;
	wave_buffer[n] = val;
	return (val);
}


static uint16_t dac_point_sum (int n, float f)
{
	if (f > 1.0)  f = 1.0;
	if (f < -1.0) f = -1.0;

	float v0 = (2.0 * wave_buffer[n] / DAC_FULL_SCALE) - 1.0;
	float v1 = v0 + f;

	if (v1 > 1.0)  v1 = 1.0;
	if (v1 < -1.0) v1 = -1.0;

	uint16_t val = (v1 + 1.0) / 2.0 * DAC_FULL_SCALE;
	wave_buffer[n] = val;
	return (val);
}

static void sawtooth (uint32_t base_rate, float amplitude, int phase)
{
	uint32_t samples_per_cycle = sample_rate / base_rate;
	int phase_sample = samples_per_cycle * phase / 100;

	if (0) output ("initializing %ld samples at %d KHz sample rate\r\n", wave_buffer_samples, sample_rate / 1000);
	for (uint32_t sample = 0; sample < wave_buffer_samples; sample++) {
		// position in the cycle
		uint32_t n = (sample + phase_sample) % samples_per_cycle;

		// sawtooth waveform math
		dac_point_sum (sample, 1.0 * amplitude * n / samples_per_cycle);
		if (0 && sample <= samples_per_cycle) {
			output ("  sample %2d = %04x\r\n", sample, wave_buffer[sample]);
		}
	}
}


static void sine (uint32_t base_rate, float amplitude, int phase)
{
	if (0) output ("sine at %d KHz, ampl %4d phase %d%%\r\n", base_rate/1000, (int)(amplitude*1000), phase);
	uint32_t samples_per_cycle = sample_rate / base_rate;
	int phase_sample = samples_per_cycle * phase / 100;
	if (0) output ("  sample phase offset %d of %d per cycle\r\n", phase_sample, samples_per_cycle);

	if (0) output ("initializing %ld samples at %d KHz sample rate\r\n", wave_buffer_samples, sample_rate / 1000);
	for (uint32_t sample = 0; sample < wave_buffer_samples; sample++) {
		// position in the cycle
		uint32_t n = (sample + phase_sample) % samples_per_cycle;
		if (0) if (sample < 20) output ("  # %3d n %3d\r\n", sample, n);

		// sine waveform math
		float omega = n * 1.0 / samples_per_cycle * 2 * M_PI;
		dac_point_sum (sample, sin(omega) * amplitude);
	}
}


static void square (uint32_t base_rate, float amplitude, int phase)
{
	uint32_t samples_per_cycle = sample_rate / base_rate;
	int phase_sample = samples_per_cycle * phase / 100;

	if (0) output ("initializing %ld samples at %d KHz sample rate\r\n", wave_buffer_samples, sample_rate / 1000);
	for (uint32_t sample = 0; sample < wave_buffer_samples; sample++) {
		// position in the cycle
		uint32_t n = (sample + phase_sample) % samples_per_cycle;

		// square waveform math
		float value;

		if (n < samples_per_cycle / 2) {
			value = amplitude;
		} else {
			value = -amplitude;
		}
		dac_point_sum (sample, value);
	}
}


static void triangle (uint32_t base_rate, float amplitude, int phase)
{
	uint32_t samples_per_cycle = sample_rate / base_rate;
	int phase_sample = samples_per_cycle * phase / 100;

	if (0) output ("initializing %ld samples at %d KHz sample rate\r\n", wave_buffer_samples, sample_rate / 1000);
	for (uint32_t sample = 0; sample < wave_buffer_samples; sample++) {
		// position in the cycle
		uint32_t n = (sample + phase_sample) % samples_per_cycle;

		// triangle waveform math
		float value;

		if (n < samples_per_cycle / 2) {
			value = (1.0 * n / (samples_per_cycle/4) - 1) * amplitude;
		} else {
			value = (1.0 * n / (samples_per_cycle/4) - 3) * -amplitude;
		}
		dac_point_sum (sample, value);
	}
}

#ifdef INCLUDE_VT
static void VT (uint32_t base_rate, uint32_t sample_rate, uint32_t samples)
{
	uint32_t samples_per_cycle = sample_rate / base_rate;
	uint32_t samples_per_segment = samples_per_cycle / 8;

	if (0) output ("initializing %ld samples at %d KHz sample rate\r\n", samples, sample_rate / 1000);
	for (uint32_t sample = 0; sample < samples; sample++) {
		// position in the cycle
		uint32_t n = sample % samples_per_cycle;
		int segment = n / samples_per_segment;
		int offset  = n % samples_per_segment;
		float value;

		switch (segment) {
			case (0):	// downward stroke of V
			case (4):
				value = 1.0 * (samples_per_segment - offset) / samples_per_segment;
				break;
			case (1):	// upward stroke of V
			case (5):
				value = 1.0 * offset / samples_per_segment;
				break;
			case (2):	// flat stroke of T (2,3,7)
			case (3):
			case (7):
				value = 1.0;
				break;
			case (6):	// upward stroke of T
				value = 1.0 * offset / samples_per_segment;
				break;
		}
		if (0) if (sample < samples_per_cycle) output ("  VT %d seg %d off %d val %6d\r\n", sample, segment, offset, (int)(value*1000));
		wave_buffer[sample] = swap16 (value * DAC_FULL_SCALE);
	}
}
#endif

#if 0
static void v_triangle (uint32_t base_rate, uint32_t sample_rate, uint32_t samples)
{
	int bad_values = 0;

	uint32_t samples_per_cycle = sample_rate / base_rate;

	output ("validating %ld samples at %d KHz sample rate\r\n", samples, sample_rate / 1000);
	for (uint32_t sample = 0; sample < samples; sample++) {
		// position in the cycle
		uint32_t n = sample % samples_per_cycle;

		// triangle waveform math
		float value = 1.0 * n / samples_per_cycle;
		uint16_t exp = swap16 (value * DAC_FULL_SCALE);
		uint16_t act = wave_buffer[sample];
		if (act != exp) {
			output ("  sample %d of %d exp %04x act %04x\r\n", sample, samples, exp, act);
			if (bad_values++ > 9) return;
		}
	}
}
#endif


void cosine_test (void)
{
	output ("outputs to 3 decimal digits:\r\n");
	output ("PI:         %6d\r\n", (int) (M_PI * 1000));
	output ("cos(0):     %6d\r\n", (int) (cos(0) * 1000));
	output ("cos(pi/2):  %6d\r\n", (int) (cos(M_PI/2) * 1000));
	output ("cos(pi):    %6d\r\n", (int) (cos(M_PI) * 1000));
	output ("cos(3pi/2): %6d\r\n", (int) (cos(3*M_PI/2) * 1000));
	output ("cos(2pi):   %6d\r\n", (int) (cos(2*M_PI) * 1000));
	output ("\r\n");
}


void help (int argc, char ** argv)
{
	output ("Commands are:\r\n");
	for (int i = 0; i < NELEMENTS(commands); i++) {
		output ("  %-10s %-36s  %s\r\n", commands[i].name, commands[i].help_args, commands[i].help);
	}
	output ("Waveform choices are:\r\n");
	for (int i = 0; i < NELEMENTS(waveform_types); i++) {
		output ("  %s\r\n", waveform_types[i].name);
	}
}


void clear (int argc, char ** argv)
{
	for (int i = 0; i < wave_buffer_samples; i++) {
		dac_point_set (i, 0.0);
	}
	SCB_CleanDCache_by_Addr ((uint32_t*) wave_buffer, 2 * wave_buffer_samples);
}

void sample_rate_cmd (int argc, char ** argv)
{
	if (argc > 0) {
		sample_rate = atofreq (argv[0]);
		wave_buffer_samples = WAVE_BUFFER_SIZE;
		clear (0, NULL);
		new_sample_rate();
	}
	else {
		output ("sample rate is %d KHz\r\n", sample_rate / 1000);
	}
}


void add_freq (int argc, char ** argv)
{
	int valid = 0;
	int i;
	static float    amplitude = 0.2;
	static uint32_t	frequency = 5000;
	int   			phase = 0;

	for (i = 0; i < NELEMENTS(waveform_types); i++) {
		if (strnicmp (waveform_types[i].name, argv[0], strlen(argv[0])) == 0) {
			valid = 1;
			break;
		}
	}

	if (!valid && atofreq (argv[0]) > 0) {
		argc++;
		argv--;
		valid = 1;
		i = 0;		// Sine is first on the list
	}

	if (valid) {
		output ("%s waveform\r\n", waveform_types[i].name);
		if (argc > 1) {
			uint32_t new_frequency = atofreq (argv[1]);
			if (new_frequency != 0) frequency = new_frequency;
			output ("  frequency %d KHz\r\n", frequency / 1000);
		}
		if (argc > 2) {
			float new_amplitude = atof (argv[2]);
			if (new_amplitude > 0.0001) amplitude = new_amplitude;
			output ("  amplitude %d.%03d\r\n", (int)(amplitude/1000), (int) (amplitude*1000) % 1000);
		}
		if (argc > 3) {
			phase = atoi (argv[3]);
			output ("  phase %3d %\r\n", phase);
		}
		add_waveform (&waveform_types[i], frequency, amplitude, phase);
	}
}

void buffer (int argc, char ** argv)
{
	if (argc > 0) {
		uint32_t new_size = atoi (argv[0]);
		if (new_size > 0 && new_size <= WAVE_BUFFER_SIZE) {
			wave_buffer_samples = new_size;
			clear (0, NULL);
		}
	}
	output ("Buffer Parameters:\r\n");
	output ("  size         %d samples\r\n", wave_buffer_samples);
	output ("  max size     %d samples\r\n", WAVE_BUFFER_SIZE);
	output ("  sample rate  %d KHz\r\n", sample_rate / 1000);
}

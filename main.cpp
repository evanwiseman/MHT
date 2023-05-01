#include <stdio.h>
#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <limits>
#include <iomanip>
#include <csignal>
#include <cstdlib>
using std::cout;
using std::endl;
using  namespace std;

#define HIGH = 32767
#define LOW = 0
#ifdef _WIN32
#include "windows.h"
#include <conio.h>
#include "ps2000.h"
//#define PREF4 _stdcall
#else
#include <sys/types.h>
#include <string.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include "libps2000-2.1/ps2000.h"
#define PREF4

#define Sleep(a) usleep(1000*a)
#define scanf_s scanf
#define fscanf_s fscanf
#define memcpy_s(a,b,c,d) memcpy(a,c,d)
typedef uint8_t BYTE;
typedef enum enBOOL
{
	FALSE, TRUE
} BOOL;
/* A function to detect a keyboard press on Linux */
int32_t _getch()
{
	struct termios oldt, newt;
	int32_t ch;
	int32_t bytesWaiting;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	setbuf(stdin, NULL);
	do
	{
		ioctl(STDIN_FILENO, FIONREAD, &bytesWaiting);
		if (bytesWaiting)
			getchar();
	} while (bytesWaiting);

	ch = getchar();

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return ch;
}

int32_t _kbhit()
{
	struct termios oldt, newt;
	int32_t bytesWaiting;
	tcgetattr(STDIN_FILENO, &oldt);
	newt = oldt;
	newt.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &newt);
	setbuf(stdin, NULL);
	ioctl(STDIN_FILENO, FIONREAD, &bytesWaiting);

	tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
	return bytesWaiting;
}

int32_t fopen_s(FILE** a, const char* b, const char* c)
{
	FILE* fp = fopen(b, c);
	*a = fp;
	return (fp > 0) ? 0 : -1;
}

/* A function to get a single character on Linux */
#define max(a,b) ((a) > (b) ? a : b)
#define min(a,b) ((a) < (b) ? a : b)
#endif
wstring lpc_w;
HANDLE sh;
void open_handle(wstring lpc) {
	int baudrate = 57600;
	int byte_size = 8;
	int stop_bits = ONESTOPBIT;
	int parity = NOPARITY;

	sh = CreateFileW(
		lpc.c_str(),
		GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (sh == INVALID_HANDLE_VALUE) {
		printf("Error INVALID HANDLE VALUE\n");
		DWORD err = GetLastError();
		cout << err << std::endl;
		printf("Press any key to exit...");
		system("pause > nul");
		exit(2);
	}

	//Get device
	DCB serial_param = { 0 };
	serial_param.DCBlength = sizeof(serial_param);
	if (GetCommState(sh, &serial_param) == 0) {
		printf("Error getting device\n");
		CloseHandle(sh);
		printf("Press any key to exit...");
		system("pause > nul");
		exit(2);
	}

	// Set parameters for device
	serial_param.BaudRate = baudrate;
	serial_param.ByteSize = byte_size;
	serial_param.StopBits = stop_bits;
	serial_param.Parity = parity;
	if (SetCommState(sh, &serial_param) == 0) {
		printf("Error setting device parameters\n");
		CloseHandle(sh);
		printf("Press any key to exit...");
		system("pause > nul");
		exit(2);
	}

	// Set timeouts for device
	COMMTIMEOUTS timeout = { 0 };
	timeout.ReadIntervalTimeout = 50;
	timeout.ReadTotalTimeoutConstant = 50;
	timeout.ReadTotalTimeoutMultiplier = 50;
	timeout.WriteTotalTimeoutConstant = 50;
	timeout.WriteTotalTimeoutMultiplier = 10;
	if (SetCommTimeouts(sh, &timeout) == 0) {
		printf("Error setting timeouts\n");
		CloseHandle(sh);
		printf("Press any key to exit...");
		system("pause > nul");
		exit(2);
	}
}
void close_handle() {
	if (CloseHandle(sh) == 0) {
		printf("Error closing device\n");
		printf("Press any key to exit...");
		system("pause > nul");
		exit(2);
	}
}
void send_data(int32_t input) {
	float num = (float)input / 1000;

	stringstream stream;
	stream << fixed << setprecision(2) << num;
	std::string str = stream.str();
	str += "\r\n";
	const char* tx = str.c_str();
	int length = (int)str.size();

	DWORD bytes_written = 0;
	if (!WriteFile(sh, tx, length, &bytes_written, NULL)) {
		printf("Error Writing File\n");
		printf("Press any key to exit...");
		CloseHandle(sh);
		system("pause > nul");
		exit(2);
	}
}
int receive_data() {
	BYTE rx = 1;
	DWORD bytes_written = 0;
	if (!WriteFile(sh, &rx, 1, &bytes_written, 0)) {
		printf("Error Writing File\n");
		printf("Press any key to exit...");
		CloseHandle(sh);
		system("pause > nul");
		exit(2);
	}
	int value = (int)rx;
	return value;
}

#define BUFFER_SIZE 3072
#define BUFFER_SIZE_STREAMING 50000		// Overview buffer size
#define NUM_STREAMING_SAMPLES 10000000	// Number of streaming samples to collect
#define MAX_CHANNELS 4
#define SINGLE_CH_SCOPE 1 // Single channel scope
#define DUAL_SCOPE 2      // Dual channel scope
/*******************************************************
* Data types changed:
*
* int8_t   - int8_t
* int16_t  - 16-bit signed integer (int16_t)
* int32_t  - 32-bit signed integer (int)
* uint32_t - 32-bit unsigned integer (unsigned int)
*******************************************************/
int16_t values_a[BUFFER_SIZE]; // block mode buffer, Channel A
int16_t values_b[BUFFER_SIZE]; // block mode buffer, Channel B

int16_t	overflow;
int32_t	scale_to_mv = 1;

int16_t	channel_mv[PS2000_MAX_CHANNELS];
int16_t	timebase = 8;

typedef enum {
	MODEL_NONE = 0,
	MODEL_PS2104 = 2104,
	MODEL_PS2105 = 2105,
	MODEL_PS2202 = 2202,
	MODEL_PS2203 = 2203,
	MODEL_PS2204 = 2204,
	MODEL_PS2205 = 2205,
	MODEL_PS2204A = 0xA204,
	MODEL_PS2205A = 0xA205
} MODEL_TYPE;
typedef struct
{
	PS2000_THRESHOLD_DIRECTION	channelA;
	PS2000_THRESHOLD_DIRECTION	channelB;
	PS2000_THRESHOLD_DIRECTION	channelC;
	PS2000_THRESHOLD_DIRECTION	channelD;
	PS2000_THRESHOLD_DIRECTION	ext;
} DIRECTIONS;
typedef struct
{
	PS2000_PWQ_CONDITIONS* conditions;
	int16_t							nConditions;
	PS2000_THRESHOLD_DIRECTION		direction;
	uint32_t						lower;
	uint32_t						upper;
	PS2000_PULSE_WIDTH_TYPE			type;
} PULSE_WIDTH_QUALIFIER;
typedef struct
{
	PS2000_CHANNEL channel;
	float threshold;
	int16_t direction;
	float delay;
} SIMPLE;
typedef struct
{
	int16_t hysteresis;
	DIRECTIONS directions;
	int16_t nProperties;
	PS2000_TRIGGER_CONDITIONS* conditions;
	PS2000_TRIGGER_CHANNEL_PROPERTIES* channelProperties;
	PULSE_WIDTH_QUALIFIER pwq;
	uint32_t totalSamples;
	int16_t autoStop;
	int16_t triggered;
} ADVANCED;
typedef struct
{
	SIMPLE simple;
	ADVANCED advanced;
} TRIGGER_CHANNEL;
typedef struct {
	int16_t DCcoupled;
	int16_t range;
	int16_t enabled;
	int16_t values[BUFFER_SIZE];
} CHANNEL_SETTINGS;
typedef struct {
	int16_t			handle;
	MODEL_TYPE		model;
	PS2000_RANGE	firstRange;
	PS2000_RANGE	lastRange;
	TRIGGER_CHANNEL trigger;
	int16_t			maxTimebase;
	int16_t			timebases;
	int16_t			noOfChannels;
	CHANNEL_SETTINGS channelSettings[PS2000_MAX_CHANNELS];
	int16_t			hasAdvancedTriggering;
	int16_t			hasFastStreaming;
	int16_t			hasEts;
	int16_t			hasSignalGenerator;
	int16_t			awgBufferSize;
} UNIT_MODEL;

// Struct to help with retrieving data into 
// application buffers in streaming data capture
typedef struct
{
	UNIT_MODEL unit;
	int16_t* appBuffers[DUAL_SCOPE * 2];
	uint32_t bufferSizes[PS2000_MAX_CHANNELS];
} BUFFER_INFO;
UNIT_MODEL unitOpened;
BUFFER_INFO bufferInfo;
int32_t times[BUFFER_SIZE];
int32_t input_ranges[PS2000_MAX_RANGES] = { 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000 };

/****************************************************************************
 *
 * adc_units
 *
 ****************************************************************************/
const char* adc_units(int16_t time_units)
{
	time_units++;
	//printf ( "time unit:  %d\n", time_units ) ;
	switch (time_units)
	{
	case 0:
		return "ADC";
	case 1:
		return "fs";
	case 2:
		return "ps";
	case 3:
		return "ns";
	case 4:
		return "us";
	case 5:
		return "ms";
	}

	return "Not Known";
}

/****************************************************************************
 * adc_to_mv
 *
 * If the user selects scaling to millivolts,
 * Convert an 12-bit ADC count into millivolts
 ****************************************************************************/
int32_t adc_to_mv(int32_t raw, int32_t ch)
{
	return (scale_to_mv) ? (raw * input_ranges[ch]) / 32767 : raw;
}

/****************************************************************************
 * mv_to_adc
 *
 * Convert a millivolt value into a 12-bit ADC count
 *
 *  (useful for setting trigger thresholds)
 ****************************************************************************/
int16_t mv_to_adc(int16_t mv, int16_t ch)
{
	return ((mv * 32767) / input_ranges[ch]);
}

/****************************************************************************
 * set_defaults - restore default settings
 ****************************************************************************/
void set_defaults(void)
{
	int16_t ch = 0;
	ps2000_set_ets(unitOpened.handle, PS2000_ETS_OFF, 0, 0);

	for (ch = 0; ch < unitOpened.noOfChannels; ch++)
	{
		ps2000_set_channel(unitOpened.handle,
			ch,
			unitOpened.channelSettings[ch].enabled,
			unitOpened.channelSettings[ch].DCcoupled,
			unitOpened.channelSettings[ch].range);
	}
}
void get_info(void) {
	int8_t description[8][25] = { "Driver Version   ",
									"USB Version      ",
									"Hardware Version ",
									"Variant Info     ",
									"Serial           ",
									"Cal Date         ",
									"Error Code       ",
									"Kernel Driver    "
	};
	int16_t 	i;
	int8_t		line[80];
	int32_t		variant;

	if (unitOpened.handle)
	{
		for (i = 0; i < 8; i++)
		{
			ps2000_get_unit_info(unitOpened.handle, line, sizeof(line), i);

			if (i == 3)
			{
				variant = atoi((const char*)line);

				if (strlen((const char*)line) == 5) // Identify if 2204A or 2205A
				{
					line[4] = toupper(line[4]);

					if (line[1] == '2' && line[4] == 'A')		// i.e 2204A -> 0xA204
					{
						variant += 0x9968;
					}
				}
			}

			if (i != 6) // No need to print error code
			{
				printf("%s: %s\n", description[i], line);
			}
		}

		switch (variant)
		{
		case MODEL_PS2104:
			unitOpened.model = MODEL_PS2104;
			unitOpened.firstRange = PS2000_100MV;
			unitOpened.lastRange = PS2000_20V;
			unitOpened.maxTimebase = PS2104_MAX_TIMEBASE;
			unitOpened.timebases = unitOpened.maxTimebase;
			unitOpened.noOfChannels = 1;
			unitOpened.hasAdvancedTriggering = FALSE;
			unitOpened.hasSignalGenerator = FALSE;
			unitOpened.hasEts = TRUE;
			unitOpened.hasFastStreaming = FALSE;
			break;

		case MODEL_PS2105:
			unitOpened.model = MODEL_PS2105;
			unitOpened.firstRange = PS2000_100MV;
			unitOpened.lastRange = PS2000_20V;
			unitOpened.maxTimebase = PS2105_MAX_TIMEBASE;
			unitOpened.timebases = unitOpened.maxTimebase;
			unitOpened.noOfChannels = 1;
			unitOpened.hasAdvancedTriggering = FALSE;
			unitOpened.hasSignalGenerator = FALSE;
			unitOpened.hasEts = TRUE;
			unitOpened.hasFastStreaming = FALSE;
			break;

		case MODEL_PS2202:
			unitOpened.model = MODEL_PS2202;
			unitOpened.firstRange = PS2000_100MV;
			unitOpened.lastRange = PS2000_20V;
			unitOpened.maxTimebase = PS2200_MAX_TIMEBASE;
			unitOpened.timebases = unitOpened.maxTimebase;
			unitOpened.noOfChannels = 2;
			unitOpened.hasAdvancedTriggering = TRUE;
			unitOpened.hasSignalGenerator = FALSE;
			unitOpened.hasEts = FALSE;
			unitOpened.hasFastStreaming = TRUE;
			break;

		case MODEL_PS2203:
			unitOpened.model = MODEL_PS2203;
			unitOpened.firstRange = PS2000_50MV;
			unitOpened.lastRange = PS2000_20V;
			unitOpened.maxTimebase = PS2200_MAX_TIMEBASE;
			unitOpened.timebases = unitOpened.maxTimebase;
			unitOpened.noOfChannels = 2;
			unitOpened.hasAdvancedTriggering = FALSE;
			unitOpened.hasSignalGenerator = TRUE;
			unitOpened.hasEts = TRUE;
			unitOpened.hasFastStreaming = TRUE;
			break;

		case MODEL_PS2204:
			unitOpened.model = MODEL_PS2204;
			unitOpened.firstRange = PS2000_50MV;
			unitOpened.lastRange = PS2000_20V;
			unitOpened.maxTimebase = PS2200_MAX_TIMEBASE;
			unitOpened.timebases = unitOpened.maxTimebase;
			unitOpened.noOfChannels = 2;
			unitOpened.hasAdvancedTriggering = TRUE;
			unitOpened.hasSignalGenerator = TRUE;
			unitOpened.hasEts = TRUE;
			unitOpened.hasFastStreaming = TRUE;
			break;

		case MODEL_PS2204A:
			unitOpened.model = MODEL_PS2204A;
			unitOpened.firstRange = PS2000_50MV;
			unitOpened.lastRange = PS2000_20V;
			unitOpened.maxTimebase = PS2200_MAX_TIMEBASE;
			unitOpened.timebases = unitOpened.maxTimebase;
			unitOpened.noOfChannels = DUAL_SCOPE;
			unitOpened.hasAdvancedTriggering = TRUE;
			unitOpened.hasSignalGenerator = TRUE;
			unitOpened.hasEts = TRUE;
			unitOpened.hasFastStreaming = TRUE;
			unitOpened.awgBufferSize = 4096;
			break;

		case MODEL_PS2205:
			unitOpened.model = MODEL_PS2205;
			unitOpened.firstRange = PS2000_50MV;
			unitOpened.lastRange = PS2000_20V;
			unitOpened.maxTimebase = PS2200_MAX_TIMEBASE;
			unitOpened.timebases = unitOpened.maxTimebase;
			unitOpened.noOfChannels = 2;
			unitOpened.hasAdvancedTriggering = TRUE;
			unitOpened.hasSignalGenerator = TRUE;
			unitOpened.hasEts = TRUE;
			unitOpened.hasFastStreaming = TRUE;
			break;

		case MODEL_PS2205A:
			unitOpened.model = MODEL_PS2205A;
			unitOpened.firstRange = PS2000_50MV;
			unitOpened.lastRange = PS2000_20V;
			unitOpened.maxTimebase = PS2200_MAX_TIMEBASE;
			unitOpened.timebases = unitOpened.maxTimebase;
			unitOpened.noOfChannels = DUAL_SCOPE;
			unitOpened.hasAdvancedTriggering = TRUE;
			unitOpened.hasSignalGenerator = TRUE;
			unitOpened.hasEts = TRUE;
			unitOpened.hasFastStreaming = TRUE;
			unitOpened.awgBufferSize = 4096;
			break;

		default:
			printf("Unit not supported");
		}

		unitOpened.channelSettings[PS2000_CHANNEL_A].enabled = 1;
		unitOpened.channelSettings[PS2000_CHANNEL_A].DCcoupled = 1;
		unitOpened.channelSettings[PS2000_CHANNEL_A].range = PS2000_5V;

		if (unitOpened.noOfChannels == DUAL_SCOPE)
		{
			unitOpened.channelSettings[PS2000_CHANNEL_B].enabled = 1;
		}
		else
		{
			unitOpened.channelSettings[PS2000_CHANNEL_B].enabled = 0;
		}

		unitOpened.channelSettings[PS2000_CHANNEL_B].DCcoupled = 1;
		unitOpened.channelSettings[PS2000_CHANNEL_B].range = PS2000_5V;

		set_defaults();

	}
	else
	{
		printf("Unit Not Opened\n");

		ps2000_get_unit_info(unitOpened.handle, line, sizeof(line), 5);

		printf("%s: %s\n", description[5], line);
		unitOpened.model = MODEL_NONE;
		unitOpened.firstRange = PS2000_100MV;
		unitOpened.lastRange = PS2000_20V;
		unitOpened.timebases = PS2105_MAX_TIMEBASE;
		unitOpened.noOfChannels = SINGLE_CH_SCOPE;
	}
}
void open_unit() {
	printf("\nOpening the device...\n");

	unitOpened.handle = ps2000_open_unit();
	printf("Handler: %d\n", unitOpened.handle);
	if (!unitOpened.handle)
	{
		get_info();
		printf("Unable to open device\n");
		printf("\n");
		printf("Press any key to exit...");
		system("pause > nul");
		exit(1);
	}
	else
	{
		get_info();
		printf("Device opened successfully\n\n");
		printf("\n");
	}
}
void close_unit() {
	ps2000_close_unit(unitOpened.handle);
}

bool hhit = false;
void collect_block_back(void) {
	// Instantiate Variables
	int32_t		i;
	int32_t		trigger_sample;
	int32_t 	time_interval;
	int16_t 	time_units;
	int16_t 	oversample;
	int32_t 	no_of_samples = BUFFER_SIZE;
	int16_t 	auto_trigger_ms = 0;
	int32_t 	time_indisposed_ms;
	int16_t 	overflow;
	int32_t 	threshold_mv = 1500;
	int32_t 	max_samples;
	uint8_t		state_a = 0;
	uint8_t		state_b = 0;
	uint8_t		flip = 0;
	uint8_t		no_trigger_b = 0;
	int32_t		offset = 0;
	int32_t		t_a = 0;
	int32_t		t_i = 0;
	double		v_a = 0.00;
	double		v_b = 0.00;
	
	// Make sure all channels are set corredctly
	set_defaults();
	ps2000_set_channel(unitOpened.handle, PS2000_CHANNEL_A, true, true, PS2000_20V);
	ps2000_set_channel(unitOpened.handle, PS2000_CHANNEL_B, true, true, PS2000_5V);

	/* Trigger enabled
	 * ChannelB - to trigger unsing this channel it needs to be enabled using ps2000_set_channel()
	* Rising edge
	* Threshold = 1.5V
	* 50% pre-trigger  (negative is pre-, positive is post-)
	*/

	// Disabled Trigger method to enable different method
	unitOpened.trigger.simple.channel = PS2000_CHANNEL_B;
	unitOpened.trigger.simple.direction = (int16_t)PS2000_RISING;
	unitOpened.trigger.simple.threshold = 100.f;
	unitOpened.trigger.simple.delay = -50;

	ps2000_set_trigger(unitOpened.handle,
		(int16_t)unitOpened.trigger.simple.channel,
		mv_to_adc(threshold_mv, unitOpened.channelSettings[(int16_t)unitOpened.trigger.simple.channel].range),
		unitOpened.trigger.simple.direction,
		(int16_t)unitOpened.trigger.simple.delay,
		auto_trigger_ms);



	/*  Find the maximum number of samples, the time interval (in time_units),
	*		 the most suitable time units, and the maximum oversample at the current timebase
	*/
	oversample = 1;
	timebase = 19;
	while (!ps2000_get_timebase(unitOpened.handle,
		timebase,
		no_of_samples,
		&time_interval,
		&time_units,
		oversample,
		&max_samples))
		timebase++;
	time_units = 3;

	// Start data collection
	ps2000_run_block(unitOpened.handle, BUFFER_SIZE, timebase, oversample, &time_indisposed_ms);

	// Make user waits and show on screen before next data set can be read in
	printf("Waiting for sufficient data...");
	Sleep(16800);
	printf("Ready\n\n");

	// If trigger didn't activate, don't run the program and wait 1 ms
	while (!ps2000_ready(unitOpened.handle) && !_kbhit()) {
		Sleep(1);
	}
	
	// Ensure unit closes
	if (_kbhit()) {
		printf("Data collection aborted...");
		Sleep(8000);
		ps2000_stop(unitOpened.handle);
		hhit = true;

	}
	// Normal procedure on trigger
	else {
		// Stop gathering data so it can be read in to computer
		ps2000_stop(unitOpened.handle);

		// Read in all values, from the buffer
		ps2000_get_times_and_values(unitOpened.handle,
			times,
			unitOpened.channelSettings[PS2000_CHANNEL_A].values,
			unitOpened.channelSettings[PS2000_CHANNEL_B].values,
			NULL,
			NULL,
			&overflow,
			time_units,
			BUFFER_SIZE);

		// This calculation is correct for 50% pre-trigger
		trigger_sample = BUFFER_SIZE / 50;
		printf("------------------------------------------------------------\n");
		// Run through all values gathered, and check where rising edge on A is
		int state = 0;
		int state_back = 0;
		int flip = 1;
		int32_t t_c = 0;
		for (i = 0; i < BUFFER_SIZE; i++) {
			v_a = unitOpened.channelSettings[0].values[i] * (20.0 / 32767.0);

			t_i = times[i];
			
			
			if (v_a < 1.5) {
				t_a = t_i;
				state = 1;
				if (flip == 1)
					state_back = 1;
			}

			if (v_a >= 1.5) {
				flip = 1;
			}

			if (state_back == 1 && flip == 1) {
				t_c = t_i;
				state_back = 0;
			}
			if (v_a < 1.5)
				flip = 0;
			

			
			if (i == BUFFER_SIZE - 1) {
				if (state == 1) {
					printf("From IR sesnor to material detected\n");
					printf("\tFront: %.2f\n", (double)(offset - t_c) / 1000.0);
					printf("\tCenter: %.2f\n", (double)(offset - ((t_a + t_c) / 2.0)) / 1000.0);
					printf("\tBack: %.2f\n", (double)(offset - t_a) / 1000.0);
					//printf("Material detected %.2f from IR sensor\n", (double)(offset - ((t_a + t_c) / 2.0)) / 1000.0);

					//send data won't increase the offset since the trigger is back called and holds prev 2 seconds
					//in buffer, so data transmission is done when this is called.
					int32_t tx = offset - t_a;
					send_data(tx);
				}
				else {
					printf("ERROR: PLEASE CHECK IR SENSOR\n");
					int32_t tx = -999999999;
					send_data(tx);
				}
			}
			//// Latch each rising edge from CH_A to the time it triggers
			//t_i = times[i];
			//if (state_a == 0) { // SR Latch
			//	if (v_a < 1.5) {
			//		state_a = 1;
			//		t_a = t_i;
			//	}
			//	else {
			//		state_a = 0;
			//	}
			//}
			//if (state_a == 1) { // SR Latch
			//	if (v_a >= 1.5) {
			//		flip = 1;
			//	}
			//}
			//if (flip == 1) { //SR Latch
			//	if (v_a < 1.5) {
			//		flip = 0;
			//		t_a = t_i;
			//	}
			//}

			//// If the states match, output ms offset
			//if ((state_a == 1) && (i == BUFFER_SIZE - 1)) {
			//	state_a = 2;
			//	printf("Material detected %.2f from IR sensor\n", (double)(offset + times[BUFFER_SIZE - 1] - t_a) / 1000.0);
			//	
			//	//send data won't increase the offset since the trigger is back called and holds prev 2 seconds
			//	//in buffer, so data transmission is done when this is called.
			//	int32_t tx = offset + times[BUFFER_SIZE - 1] - t_a;
			//	send_data(tx);

			//	//sendData(offset + times[BUFFER_SIZE - 1] - t_a);
			//}
			//// Otherwise if only CH_B triggers, then output detection, but not within timing window available
			//else if ((state_a != 1) && (i == BUFFER_SIZE - 1)) {
			//	printf("Material detected, outside capture range of IR sensor\n");
			//}

			//// Part of latch, but needs to be at the end
			//if ((state_a == 2) && (v_a >= 1.5)) { // SR Latch
			//	state_a = 0;
			//}
		}
		
		printf("------------------------------------------------------------\n\n");
	}
}
void create_menu() {
	system("CLS");
	int choice;
	do {
		system("CLS");
		if (lpc_w.empty())
			printf("(WARNING) NO COM PORT SELECTED, WRITE\n");
		else
			printf("COM Port | Write = %ls\n", lpc_w.c_str());


		printf("\nTac-7 Mail Sorting Delay Timing\n");
		printf("Version 0.0.1\n");
		printf("1 - Run Delay Timing Program\n");
		printf("2 - Settings\n");
		printf("3 - About\n");
		printf("4 - Exit\n");
		if (!(cin >> choice)) {
			
		}
	} while (choice > 4 || choice < 1);

	switch (choice) {
	case 1: // Run program
	{
		
		printf("Press any key to return\n");
		
		// Runtime loop
		open_handle(lpc_w);
		while (hhit == false) {
			collect_block_back();
		}
		hhit = false;
		
		_getch();
		// Ensure unit closes
		close_handle();
		create_menu();
		
	}
		break;
	case 2: // Settings
	{
		int choice_settings;
		do {
			system("CLS");
			printf("Settings\n");
			printf("1 - Communications Setup\n");
			printf("2 - Back\n");
			if (!(cin >> choice_settings)) {
				cin.clear();
				cin.ignore(10000, '\n');
			}
		} while (choice_settings > 3 || choice_settings < 1);
		switch (choice_settings) {
		case 1: // comm setup
		{
			int choice_comm;
			do {
				system("CLS");
				printf("Communications Setup\n");
				printf("1 - COM Port | Write\n");
				printf("2 - Baudrate\n");
				printf("3 - Back\n");
				if (!(cin >> choice_comm)) {
					cin.clear();
					cin.ignore(10000, '\n');
				}
			} while (choice_comm > 3 || choice_comm < 1);
			switch (choice_comm) {
			case 1: // comm port
			{
				HANDLE serial_handle;

				do {
					system("CLS");
					wstring com_num;
					printf("COM Port\n");
					printf("Enter name of COM Port: ");
					wcin >> com_num;
					wstring com_prefix = L"\\\\.\\";
					wstring com_id = com_prefix + com_num;
					wofstream file("COMM.txt");
					file << com_id;
					file.close();

					lpc_w = com_id;
					serial_handle = CreateFileW(
						lpc_w.c_str(),
						GENERIC_WRITE,
						0,
						nullptr,
						OPEN_EXISTING,
						0,
						nullptr);
				} while (serial_handle == INVALID_HANDLE_VALUE);

				CloseHandle(serial_handle);
				create_menu();
			}
				break;
			case 2: // Baudrate setup
			{
				int choice_baud;
				do {
					system("CLS");
					printf("Baudrate\n");
					printf("1 - 57600\n");
					printf("2 - 115200\n");
					printf("3 - Back\n");
					if (!(cin >> choice_baud)) {
						cin.clear();
						cin.ignore(10000, '\n');
					}
				} while (choice_baud > 3 || choice_baud < 1);
				switch (choice_baud) {
				case 1:
					create_menu();
					break;
				case 2:
					create_menu();
					break;
				case 3:
					create_menu();
					break;
				}
			}
				break;
			case 3: // Back
				create_menu();
				break;
			}
		}
			break;
		case 2: // Back
			create_menu();
			break;
		}
	}
		break;
	case 3: // About
	{
		int choice_about;
		do {
			system("CLS");
			printf("About\n");
			printf("Tac-7 Mail Sorting Delay Timing\n");
			printf("Version 0.0.1\n");
			printf("Copyright 2022\n\n");
			printf("Calculates time between an envelope entering the system, and an alarm triggering.\n");
			printf("Needs a 6V input to trigger the alarm");
			printf("Make sure to set the COM Port in settings before use.\n");
			printf("The COM Port is setup via null-modem emulator(com0com).\n");

			printf("1 - Back\n");
			if (!(cin >> choice_about)) {
				cin.clear();
				cin.ignore(10000, '\n');
			}
		} while (choice_about != 1);
		switch (choice_about) {
		case 1:
			create_menu();
			break;
		}
	}
		break;
	case 4: // Exit
		system("CLS");
		break;
	}
}

int main() {
	wifstream file("COMM.txt");
	if (file.good()) {
		wstring line;
		getline(file, line);
		lpc_w = line;
		line.clear();
	}
	file.close();
	
	open_unit();
	create_menu();
	close_unit();

	return 0;
}
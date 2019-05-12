/* ********************************* FILE ************************************/
/** \file    main.cpp
 *
 * \brief    This file describes the definition of the main entry point.
 *
 * \author   Clemens Rabe
 * \date     Apr 06, 2019
 * \note     (C) Copyright Clemens Rabe <clemens.rabe@gmail.com>
 *
 *****************************************************************************/

/*****************************************************************************
 * SETTINGS
 ******************************************************************************/
#ifdef ON_RASPBERRY_PI
  #define GPIO_ARDUINO       7
  #define GPIO_INTTEST_OUT  24
  #define GPIO_INTTEST_IN   25
#else
  #define GPIO_ARDUINO      27
  #define GPIO_INTTEST_OUT   2
  #define GPIO_INTTEST_IN    3
#endif


/*****************************************************************************
 * INCLUDE FILES
 ******************************************************************************/
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <stdlib.h>
#include <sys/stat.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <vector>
#include <algorithm>

#if defined(ON_RASPBERRY_PI) or defined(ON_ODROID_XU4)
  #include <wiringPi.h>
#endif


/* ********************************* METHOD **********************************/
/**
 * \brief     Print the usage and exit.
 *
 * \author    Clemens Rabe
 * \date      Apr 06, 2019
 *
 * \param[in] f_progName_p - Name of the program.
 *
 *****************************************************************************/
void usage(const char* f_progName_p)
{
    printf("Usage: %s [<Options>] [device]\n"
           "\n"
           "Measure the times from writing a single byte on the serial port\n"
           "and the response from the Arduino.\n"
#ifdef ON_RASPBERRY_PI
           "In addition, the signal from the Arduino is received over the\n"
           "GPIO (Physical Pin 7/WiringPi Pin 7/BCM Pin 4) and the time\n"
           "is measured using an interrupt handler.\n"
           "To determine the latency of the interrupt handler, we use a\n"
           "connection between the Physical Pin 35/Wiring Pi Pin 24/BCM Pin 19\n"
           "and the Physical Pin 37/Wiring Pi Pin 25/BCM Pin 26 where we use\n"
           "the first pin as an output and the second one as an input.\n"
           "Then we measure the time between setting the output pin to HIGH\n"
           "and the interrupt handler call.\n"
#endif

#ifdef ON_ODROID_XU4
           "In addition, the signal from the Arduino is received over the\n"
           "GPIO (Physical Pin 27/WiringPi Pin 27/GPIO 33) and the time\n"
           "is measured using an interrupt handler.\n"
           "To determine the latency of the interrupt handler, we use a\n"
           "connection between the Physical Pin 13/Wiring Pi Pin 2/GPIO 21\n"
           "and the Physical Pin 17/Wiring Pi Pin 3/GPIO 22 where we use\n"
           "the first pin as an output and the second one as an input.\n"
           "Then we measure the time between setting the output pin to HIGH\n"
           "and the interrupt handler call.\n"
#endif
           "\n"
           "Arguments:\n"
           "  device: The serial device, e.g., 'ttyUSB0'.\n"
           "\n"
           "Options:\n"
           "  -h|--help:      Print this help.\n"
           "  -l|--latency N: Set the FTDI read latency timer to the given\n"
           "                  value in milliseconds [default: 1ms].\n"
           "  -s|--size N:    Send N bytes at once [default: 1].\n"
#if defined(ON_RASPBERRY_PI) or defined(ON_ODROID_XU4)
           "  -i|--interrupt: Perform only the interrupt latency test.\n"
	   "  --iloops N:     Number of loops for interrupt latency test [default: 10000].\n"
#endif
           "  -b|--bulk:      Perform only the bulk serial write/read test.\n"
           "  -t|--timed:     Perform only the serial write/read test at a\n"
           "                  fixed frequency (on Raspberry Pi with interrupt\n"
           "                  based signal set latency test).\n"
	   "  --tloops N:     Number of loops for serial write/read test [default: 1200]\n",
           f_progName_p);
    exit(1);
}


/* ********************************* METHOD **********************************/
/**
 * \brief     Check if a file exists.
 *
 * \author    Clemens Rabe
 * \date      Apr 06, 2019
 *
 * \param[in] fr_fileName - The file name.
 * \return    Returns \c true if the file exists, otherwise it returns
 *            \c false.
 *****************************************************************************/
bool fileExists(const std::string& fr_fileName)
{
    struct stat buffer;
    return (stat(fr_fileName.c_str(), &buffer) == 0);
}


/* ********************************* METHOD **********************************/
/**
 * \brief     Get the latency in milliseconds of an FTDI adapter with a given
 *            serial device name (e.g., ttyUSB0).
 *
 * \author    Clemens Rabe
 * \date      Apr 06, 2019
 *
 * \param[in] fr_serialDevice - The serial device name, e.g., "ttyUSB0".
 * \return    Returns the current latency in milliseconds or -1 if the device
 *            is not an FTDI.
 *
 *****************************************************************************/
int32_t getFtdiLatency(const std::string& fr_serialDevice)
{
    const std::string latencyTimerFileName = "/sys/bus/usb-serial/devices/" + fr_serialDevice + "/latency_timer";
    int32_t latencyMs_i = -1;

    FILE* file_p = fopen(latencyTimerFileName.c_str(), "r");

    if (file_p) {
        if (fscanf(file_p, "%d", &latencyMs_i) != 1) {
            latencyMs_i = -1;
        }

        fclose(file_p);
    }

    return latencyMs_i;
}


int32_t getSysfsCounter(const std::string& fr_fileName) {
  int32_t counter_i = -1;
  FILE* file_p = fopen(fr_fileName.c_str(), "r");
  if (file_p) {
    if (fscanf(file_p, "%d", &counter_i) != 1) {
      counter_i = -1;
    }
    fclose(file_p);
  }
  return counter_i;
}

uint64_t getSysfsTimestamp(const std::string& fr_fileName) {
  uint64_t timestamp_ui = 0;
  FILE* file_p = fopen(fr_fileName.c_str(), "r");
  if (file_p) {
    if (fscanf(file_p, "%llu", &timestamp_ui) != 1) {
      timestamp_ui = 0;
    }
    fclose(file_p);
  }
  return timestamp_ui;
}

/* ********************************* METHOD **********************************/
/**
 * \brief     Set the latency in milliseconds of a FTDI device.
 *
 * \author    Clemens Rabe
 * \date      Apr 06, 2019
 *
 * \param[in] fr_serialDevice - Serial device.
 * \param[in] f_latencyMs_i   - Latency to set.
 * \return    Returns \c true on success, otherwise \c false.
 *
 *****************************************************************************/
bool setFtdiLatency(const std::string& fr_serialDevice,
                    const int32_t      f_latencyMs_i)
{
    const std::string latencyTimerFileName = "/sys/bus/usb-serial/devices/" + fr_serialDevice + "/latency_timer";
    bool  retVal_b = false;
    FILE* file_p = fopen(latencyTimerFileName.c_str(), "w");

    if (file_p) {
        retVal_b = (fprintf(file_p, "%d", f_latencyMs_i) > 0);
        fclose(file_p);
    }

    return retVal_b;
}


/* ********************************* METHOD **********************************/
/**
 * \brief     Initialize the serial port.
 *
 * \author    Clemens Rabe
 * \date      Apr 06, 2019
 *
 * \param[in] f_serialPortHandle_i - The serial port handle.
 * \return    Returns \c true on success, otherwise \c false.
 *
 *****************************************************************************/
bool initializeSerialPort(int f_serialPortHandle_i, const uint32_t f_numBytes_ui)
{
    struct termios newtio;
    memset( &newtio, 0, sizeof( newtio ) );

    // No processing
    cfmakeraw( &newtio );

    cfsetspeed( &newtio, B115200 );

    // Ignore control lines
    newtio.c_cflag |= CLOCAL;
    newtio.c_cflag &= ~CRTSCTS;

    // set to 8N1
    newtio.c_cflag &= ~( PARENB | PARODD );
    newtio.c_cflag &= ~CSTOPB;
    newtio.c_cflag &= ~CSIZE;
    newtio.c_cflag |= CS8;

    // Enable read
    newtio.c_cflag |= CREAD;

    // Input flags (ignore parity errors)
    newtio.c_iflag = IGNPAR;

    // Raw output
    newtio.c_oflag = 0;

    // Disable canonical input
    newtio.c_lflag = 0;

    // Read parameters (block read until 1 character arrives)
    newtio.c_cc[VTIME]    = 0;     /* inter-character timer unused */
    newtio.c_cc[VMIN]     = f_numBytes_ui;     /* blocking read until 1 character arrives */

    if ( tcsetattr( f_serialPortHandle_i, TCSANOW, &newtio ) != 0 )
    {
        printf("Error: Can't setup serial port!\n");
        return false;
    }

    // Set serial port to blocking mode again. This is required, since
    // a blocking open() may block until a carrier is present, depending
    // on the serial port configuration. To be sure, we open with non-blocking
    // and reset to blocking here.
    int flags_i = fcntl( f_serialPortHandle_i, F_GETFL );
    flags_i    &= ~O_NONBLOCK;

    if ( fcntl( f_serialPortHandle_i, F_SETFL, flags_i ) < 0 )
    {
        printf("Error: Can't disable non-blocking mode of the serial port!\n");
        return false;
    }

    // Remove any content waiting on the serial port
    tcflush(f_serialPortHandle_i, TCIOFLUSH);

    return true;
}


bool writeChars(int f_serialPortHandle_i,
                const uint32_t f_numChars_ui,
                const uint8_t* f_chars_p)
{
    const ssize_t written_i = write(f_serialPortHandle_i, f_chars_p, f_numChars_ui);
    return (written_i == f_numChars_ui);
}

bool readChars(int f_serialPortHandle_i,
               const uint32_t f_numChars_ui,
               uint8_t* f_chars_p)
{
    const ssize_t read_i = read(f_serialPortHandle_i, f_chars_p, f_numChars_ui);
    return (read_i == f_numChars_ui);
}

bool writeChar(int f_serialPortHandle_i,
               const uint8_t f_char_ui)
{
    const ssize_t written_i = write(f_serialPortHandle_i, &f_char_ui, 1);
    return (written_i == 1);
}


bool readChar(int f_serialPortHandle_i,
              uint8_t& fr_char_ui)
{
    const ssize_t read_i = read(f_serialPortHandle_i, &fr_char_ui, 1);
    return (read_i == 1);
}


#define RECORD_TIME(timespecStruct) \
  clock_gettime(CLOCK_MONOTONIC_RAW, (struct timespec*) &timespecStruct);

#define GET_NANOSECONDS(timespecStruct) \
  (uint64_t(timespecStruct.tv_nsec) + (uint64_t(timespecStruct.tv_sec) * uint64_t(1000000000)))

uint64_t getTimeStampNs()
{
    struct timespec t;
    RECORD_TIME(t);
    return GET_NANOSECONDS(t);
}


float getMilliseconds(const uint64_t f_startNs_ui,
                      const uint64_t f_endNs_ui)
{
  if (f_endNs_ui > f_startNs_ui) {
    const uint64_t diffNs_ui = f_endNs_ui - f_startNs_ui;
    return float(diffNs_ui) / 1000000.0f;
  } else {
    const uint64_t diffNs_ui = f_startNs_ui - f_endNs_ui;
    return float(diffNs_ui) / -1000000.0f;
  }
}


float getMilliseconds(struct timespec& fr_startNs_ui,
                      struct timespec& fr_endNs_ui)
{
  const uint64_t startNs_ui = GET_NANOSECONDS(fr_startNs_ui);
  const uint64_t endNs_ui   = GET_NANOSECONDS(fr_endNs_ui);

  if (endNs_ui > startNs_ui) {
    const uint64_t diffNs_ui = endNs_ui - startNs_ui;
    return float(diffNs_ui) / 1000000.0f;
  } else {
    const uint64_t diffNs_ui = startNs_ui - endNs_ui;
    return float(diffNs_ui) / -1000000.0f;
  }
}


float getMilliseconds(struct timespec& fr_startNs_ui,
                      const uint64_t f_endNs_ui)
{
  const uint64_t startNs_ui = GET_NANOSECONDS(fr_startNs_ui);

  if (f_endNs_ui > startNs_ui) {
    const uint64_t diffNs_ui = f_endNs_ui - startNs_ui;
    return float(diffNs_ui) / 1000000.0f;
  } else {
    const uint64_t diffNs_ui = startNs_ui - f_endNs_ui;
    return float(diffNs_ui) / -1000000.0f;
  }
}


bool timeWriteRead(int           f_serialPortHandle_i,
                   const uint8_t f_dataByte_ui,
                   uint64_t&     fr_timeBeforeWrite_ui,
                   uint64_t&     fr_timeAfterWrite_ui,
                   uint64_t&     fr_timeAfterRead_ui)
{
  struct timespec timeBeforeWrite, timeAfterWrite, timeAfterRead;
  uint8_t readChar_ui;

  RECORD_TIME(timeBeforeWrite);
  if (!writeChar(f_serialPortHandle_i, f_dataByte_ui)) {
    return false;
  }
  RECORD_TIME(timeAfterWrite);

  if (!readChar(f_serialPortHandle_i, readChar_ui)) {
    return false;
  }
  RECORD_TIME(timeAfterRead);

  fr_timeBeforeWrite_ui = GET_NANOSECONDS(timeBeforeWrite);
  fr_timeAfterWrite_ui = GET_NANOSECONDS(timeAfterWrite);
  fr_timeAfterRead_ui = GET_NANOSECONDS(timeAfterRead);

  return (f_dataByte_ui == readChar_ui);
}


/// A time series (milliseconds)
typedef std::vector<float> TimeSeries_t;

/// Information we extract from a time series for printing
struct TimeAnalysis {
  float mean_f;
  float min_f;
  float max_f;
  float median_f;
};

void calculateStatistics(TimeSeries_t& fr_timeSeries,
                         TimeAnalysis& fr_analysis) {
  std::sort(fr_timeSeries.begin(), fr_timeSeries.end());
  fr_analysis.min_f = fr_timeSeries.front();
  fr_analysis.max_f = fr_timeSeries.back();
  fr_analysis.median_f = fr_timeSeries[fr_timeSeries.size()/2];

  float sum_f = 0.0f;
  for (TimeSeries_t::const_iterator i = fr_timeSeries.begin(); i != fr_timeSeries.end(); ++i)
    sum_f += *i;
  fr_analysis.mean_f = sum_f / float(fr_timeSeries.size());
}

void saveTimeSeries(const TimeSeries_t& fr_timeSeries,
                    const std::string& fr_fileName) {
  FILE* file_p = fopen(fr_fileName.c_str(), "w");

  if (file_p) {
    fprintf(file_p, "# Time series data. Unit is milliseconds.\n");
    for (TimeSeries_t::const_iterator i = fr_timeSeries.begin(); i != fr_timeSeries.end(); ++i) {
      fprintf(file_p, "%.6f\n", *i);
    }
    fclose(file_p);
  }
}

void printProgress(uint64_t&      fr_lastNs_ui,
                   const char*    f_prefix_p,
                   const uint32_t f_currentCounter_ui,
                   const uint32_t f_maxCounter_ui) {
  const uint64_t currentNs_ui = getTimeStampNs();

  if (getMilliseconds(fr_lastNs_ui, currentNs_ui) >= 1000.0f) {
    printf("%s: %d of %d iterations performed...\n", f_prefix_p, f_currentCounter_ui + 1, f_maxCounter_ui);
    fr_lastNs_ui = currentNs_ui;
  }
}

#if defined(ON_RASPBERRY_PI) or defined(ON_ODROID_XU4)

volatile uint64_t g_timeInterrupt_ui;

int32_t g_last_inttest_counter_i = -1;
int32_t g_last_arduino_counter_i = -1;

void waitForSysfsIntTestTimestamp() {
  while (getSysfsCounter("/sys/gpiotiming/inttest_counter") == g_last_inttest_counter_i) {
    usleep(1000);
  }
  g_last_inttest_counter_i = getSysfsCounter("/sys/gpiotiming/inttest_counter");
  g_timeInterrupt_ui = getSysfsTimestamp("/sys/gpiotiming/inttest_timestamp_ns");
}

void waitForSysfsArduinoTimestamp() {
  while (getSysfsCounter("/sys/gpiotiming/arduino_counter") == g_last_arduino_counter_i) {
    usleep(1000);
  }
  g_last_arduino_counter_i = getSysfsCounter("/sys/gpiotiming/arduino_counter");
  g_timeInterrupt_ui = getSysfsTimestamp("/sys/gpiotiming/arduino_timestamp_ns");
}


void interruptHandler() {
  struct timespec timeInterrupt;
  RECORD_TIME(timeInterrupt);
  g_timeInterrupt_ui = GET_NANOSECONDS(timeInterrupt);
}

bool initializeWiringPi() {
  // Initialize wiringPi library
  if (wiringPiSetup() < 0) {
    printf("Error: Can't setup wiringPi library!\n");
    return false;
  }

  // Set pin modes
#ifndef USE_KERNEL_DRIVER
  pinMode(GPIO_ARDUINO,     INPUT);
  pinMode(GPIO_INTTEST_IN,  INPUT);
#endif
  pinMode(GPIO_INTTEST_OUT, OUTPUT);

#ifndef USE_KERNEL_DRIVER
  // Add interrupt routines
  if (wiringPiISR(GPIO_ARDUINO, INT_EDGE_FALLING, &interruptHandler) < 0) {
    printf("Error: Can't add wiringPi interrupt on GPIO_ARDUINO!\n");
    return false;
  }
  if (wiringPiISR(GPIO_INTTEST_IN, INT_EDGE_FALLING, &interruptHandler) < 0) {
    printf("Error: Can't add wiringPi interrupt on GPIO_INTTEST_IN!\n");
    return false;
  }
  printf("Info: Registered interrupt handler.\n");
#endif
  return true;
}

void determineInterruptLatency(const uint32_t f_numLoops_ui) {
  printf("Info: Testing interrupt latency...\n");
  // We are triggering on falling edge, so we set the output to HIGH first
  digitalWrite(GPIO_INTTEST_OUT, HIGH);
  usleep(2000);

#ifdef USE_KERNEL_DRIVER
  g_last_inttest_counter_i = getSysfsCounter("/sys/gpiotiming/inttest_counter");
#endif
  
  TimeSeries_t timeToInterrupt1;
  TimeSeries_t timeToInterrupt2;
  timeToInterrupt1.reserve(f_numLoops_ui);
  timeToInterrupt2.reserve(f_numLoops_ui);
  uint64_t     lastNs_ui = getTimeStampNs();

  for (uint32_t i = 0; i < f_numLoops_ui; ++i) {
    struct timespec timeBeforeDigitalWrite, timeAfterDigitalWrite;
    g_timeInterrupt_ui = 0;

    RECORD_TIME(timeBeforeDigitalWrite);
    digitalWrite(GPIO_INTTEST_OUT, LOW);
    RECORD_TIME(timeAfterDigitalWrite);

#ifdef USE_KERNEL_DRIVER
    waitForSysfsIntTestTimestamp();
#else
    // Wait for interrupt to be called
    while (g_timeInterrupt_ui == 0)
      usleep(1000);
#endif

    timeToInterrupt1.push_back(getMilliseconds(timeBeforeDigitalWrite, g_timeInterrupt_ui));
    timeToInterrupt2.push_back(getMilliseconds(timeAfterDigitalWrite,  g_timeInterrupt_ui));

    // Set again to HIGH
    digitalWrite(GPIO_INTTEST_OUT, HIGH);
    usleep(2000);

    printProgress(lastNs_ui, "Interrupt latency measurement", i, f_numLoops_ui);
  }
  
  TimeAnalysis analysis1, analysis2;
  calculateStatistics(timeToInterrupt1, analysis1);
  calculateStatistics(timeToInterrupt2, analysis2);

  printf("Time between start of digital write and interrupt: %.3f ms (mean = %.3f, min = %.3f, max=%.3f)\n",
         analysis1.median_f, analysis1.mean_f, analysis1.min_f, analysis1.max_f);
  printf("Time between end of digital write and interrupt:   %.3f ms (mean = %.3f, min = %.3f, max=%.3f)\n",
         analysis2.median_f, analysis2.mean_f, analysis2.min_f, analysis2.max_f);

  saveTimeSeries(timeToInterrupt1, "digitalWriteStart_to_interrupt.gpd");
  saveTimeSeries(timeToInterrupt2, "digitalWriteEnd_to_interrupt.gpd");
}

#endif


/* ********************************* METHOD **********************************/
/**
 * \brief     Main entry point.
 *
 * \author    Clemens Rabe
 * \date      Apr 06, 2019
 *
 * \param[in] f_argc_i - Number of arguments.
 * \param[in] f_argv_p - Arguments.
 * \return    Return code of the application.
 *
 *****************************************************************************/
int main(int f_argc_i, char** f_argv_p) {
    const char* progName_p = f_argv_p[0];
    std::string serialDevice = "";
    int32_t ftdiTgtLatency_i = 1;
    uint32_t numBytes_ui = 1;
    bool performInterruptLatencyTest_b = true;
    uint32_t numInterruptLoops_ui = 10000;
    bool performBulkSerialTest_b = true;
    bool performedTimedSerialTest_b = true;
    uint32_t numTimedSerialLoops_ui = 20 * 60;

    // Parse command line arguments
    for (int i=1; i < f_argc_i; ++i) {
        if ((strcmp(f_argv_p[i], "-h") == 0) ||
            (strcmp(f_argv_p[i], "--help") == 0)) {
            usage(progName_p);
        }
        else if ((strcmp(f_argv_p[i], "-l") == 0) ||
                 (strcmp(f_argv_p[i], "--latency") == 0)) {
          if (++i < f_argc_i) {
            ftdiTgtLatency_i = atoi(f_argv_p[i]);
          } else {
            printf("Error: Expected argument after -l option!\n");
            usage(progName_p);
          }
        }
        else if ((strcmp(f_argv_p[i], "-s") == 0) ||
                 (strcmp(f_argv_p[i], "--size") == 0)) {
          if (++i < f_argc_i) {
            numBytes_ui = atoi(f_argv_p[i]);
          } else {
            printf("Error: Expected argument after -s option!\n");
            usage(progName_p);
          }
        }
        else if ((strcmp(f_argv_p[i], "-i") == 0) ||
                 (strcmp(f_argv_p[i], "--interrupt") == 0)) {
          performInterruptLatencyTest_b = true;
          performBulkSerialTest_b = false;
          performedTimedSerialTest_b = false;
        }
        else if ((strcmp(f_argv_p[i], "--iloops") == 0)) {
          if (++i < f_argc_i) {
            numInterruptLoops_ui = atoi(f_argv_p[i]);
          } else {
            printf("Error: Expected argument after --iloops option!\n");
            usage(progName_p);
          }
        }
        else if ((strcmp(f_argv_p[i], "-b") == 0) ||
                 (strcmp(f_argv_p[i], "--bulk") == 0)) {
          performInterruptLatencyTest_b = false;
          performBulkSerialTest_b = true;
          performedTimedSerialTest_b = false;
        }
        else if ((strcmp(f_argv_p[i], "-t") == 0) ||
                 (strcmp(f_argv_p[i], "--timed") == 0)) {
          performInterruptLatencyTest_b = false;
          performBulkSerialTest_b = false;
          performedTimedSerialTest_b = true;
        }
        else if ((strcmp(f_argv_p[i], "--tloops") == 0)) {
          if (++i < f_argc_i) {
            numTimedSerialLoops_ui = atoi(f_argv_p[i]);
          } else {
            printf("Error: Expected argument after --tloops option!\n");
            usage(progName_p);
          }
        }
        else if (f_argv_p[i][0] == '-') {
            printf("Error: Unknown option %s! Please see usage for available options!\n\n",
                   f_argv_p[i]);
            usage(progName_p);
        }
        else if (serialDevice.empty()) {
            serialDevice = f_argv_p[i];
        }
        else {
            printf("Error: You have already specified a serial device! Please see usage for syntax!\n\n");
            usage(progName_p);
        }
    }

    // Auto detect serial device
    if (serialDevice.empty()) {
        if (fileExists("/dev/ttyUSB0")) {
            serialDevice = "ttyUSB0";
        }
        else if (fileExists("/dev/ttyACM0")) {
            serialDevice = "ttyACM0";
        }
        else {
            printf("Error: Can't detect serial device! Please specify your serial device on the command line!\n");
            exit(1);
        }
        printf("Detected serial device %s.\n", serialDevice.c_str());
    }
    else {
        // Remove a prefix '/dev/' from the serialDevice identifier
        size_t pos_i = serialDevice.find("/dev/", 0);
        if (pos_i != std::string::npos) {
            serialDevice.replace(pos_i, 5, "");
        }

        // Check if the device exists
        if (!fileExists("/dev/" + serialDevice)) {
            printf("Error: the specified serial device %s does not exist!\n", serialDevice.c_str());
            exit(1);
        }
    }


    // Adjust Ftdi latency
    {
        int32_t latency_i = getFtdiLatency(serialDevice);

        if (latency_i != ftdiTgtLatency_i) {
            printf("Warning: FTDI adapter found with latency of %d ms.\n", latency_i);

            if (setFtdiLatency(serialDevice, ftdiTgtLatency_i)) {
              printf("Info: Set FTDI adapter latency to %d ms.\n", ftdiTgtLatency_i);
            } else {
                printf("Error: Can't set FTDI adapter latency. Please execute:\n"
                       "    echo %d | sudo tee /sys/bus/usb-serial/devices/%s/latency_timer\n"
                       "to reduce the latency or run this program as root.\n",
                       ftdiTgtLatency_i, serialDevice.c_str());
            }
        } else if (latency_i == ftdiTgtLatency_i) {
          printf("Info: FTDI adapter with latency of %d ms found.\n", ftdiTgtLatency_i);
        } else {
            printf("Info: No FTDI adapter found.\n");
        }
    }

#if defined(ON_RASPBERRY_PI) or defined(ON_ODROID_XU4)
    if (!initializeWiringPi())
      return 5;

    if (performInterruptLatencyTest_b) {
      determineInterruptLatency(numInterruptLoops_ui);
    }
#endif

    // Open the serial port
    const std::string serialPortName = "/dev/" + serialDevice;
    int serialPortHandle_i = open(serialPortName.c_str(), O_RDWR | O_NOCTTY | O_NONBLOCK);

    if (serialPortHandle_i == -1) {
        printf("Error: Can't open serial port %s!\n", serialPortName.c_str());
        return 10;
    }

    if (!initializeSerialPort(serialPortHandle_i, numBytes_ui)) {
        close(serialPortHandle_i);
        return 11;
    }

    printf("Waiting for arduino to start (5 seconds)...\n");
    sleep(5);

    if (performBulkSerialTest_b) {
      printf("Bulk write/read ...\n");
      const uint32_t numLoops_ui = 1200;
      const uint64_t startNs_ui = getTimeStampNs();

      uint8_t* writeBuffer_p = new uint8_t[numBytes_ui];
      uint8_t* readBuffer_p  = new uint8_t[numBytes_ui];

      for (uint32_t i = 0; i < numLoops_ui; ++i) {
        const uint8_t writtenChar_ui = i % 256;
        for (uint32_t j = 0; j < numBytes_ui; ++j)
          writeBuffer_p[j] = writtenChar_ui;

        uint8_t readChar_ui;

        if (!writeChars(serialPortHandle_i, numBytes_ui, writeBuffer_p)) {
          printf("Error: Can't write character(s) on serial line (loop %d)!\n", i);
          close(serialPortHandle_i);
          return 20;
        }

        if (!readChars(serialPortHandle_i, numBytes_ui, readBuffer_p)) {
          printf("Error: Can't read character(s) from serial line (loop %d)\n", i);
          close(serialPortHandle_i);
          return 21;
        }

        if (writtenChar_ui != readBuffer_p[0]) {
          printf("Error: Written character %d but received character %d!\n",
                 int(writtenChar_ui), int(readBuffer_p[0]));
          close(serialPortHandle_i);
          return 22;
        }
      }
      const uint64_t endNs_ui = getTimeStampNs();

      printf("%.3f ms per iteration\n", getMilliseconds(startNs_ui, endNs_ui) / float(numLoops_ui));
    }

    if (performedTimedSerialTest_b) {
      printf("Write/read at 20 Hz...\n");
      TimeSeries_t timeToInterrupt;
      TimeSeries_t timeOfWrite;
      TimeSeries_t timeToRead;
      TimeSeries_t timeTotal;
      uint64_t     lastNs_ui = getTimeStampNs();

      timeToInterrupt.reserve(numTimedSerialLoops_ui);
      timeOfWrite.reserve(numTimedSerialLoops_ui);
      timeToRead.reserve(numTimedSerialLoops_ui);
      timeTotal.reserve(numTimedSerialLoops_ui);

#if defined(ON_RASPBERRY_PI) or defined(ON_ODROID_XU4)
#ifdef USE_KERNEL_DRIVER
      g_last_arduino_counter_i = getSysfsCounter("/sys/gpiotiming/arduino_counter");
#endif /* USE_KERNEL_DRIVER */
#endif
      
      for (uint32_t i = 0; i < numTimedSerialLoops_ui; ++i) {
        const uint8_t writtenChar_ui = i % 256;
        uint64_t timeBeforeWrite_ui, timeAfterWrite_ui, timeAfterRead_ui;

        usleep(1000000 / 20);
        if (!timeWriteRead(serialPortHandle_i, writtenChar_ui,
                           timeBeforeWrite_ui, timeAfterWrite_ui, timeAfterRead_ui))
          {
            printf("Error: Write/Read of character failed (loop %d)\n", i);
            close(serialPortHandle_i);
            return 30;
          }

#if defined(ON_RASPBERRY_PI) or defined(ON_ODROID_XU4)
        // Interrupt is only triggered on falling edge, that means if we have written
        // a value with the lowest bit set to 0.
        if ((i > 0) && (writtenChar_ui % 2) == 0) {
#ifdef USE_KERNEL_DRIVER
	  waitForSysfsArduinoTimestamp();
#endif /* USE_KERNEL_DRIVER */
          const float timeToInterruptMs_f = getMilliseconds(timeBeforeWrite_ui, g_timeInterrupt_ui);
          timeToInterrupt.push_back(timeToInterruptMs_f);
        }
#endif

        const float timeOfWriteMs_f = getMilliseconds(timeBeforeWrite_ui, timeAfterWrite_ui);
        timeOfWrite.push_back(timeOfWriteMs_f);

        const float timeToReadMs_f = getMilliseconds(timeAfterWrite_ui, timeAfterRead_ui);
        timeToRead.push_back(timeToReadMs_f);

        const float timeTotalMs_f = getMilliseconds(timeBeforeWrite_ui, timeAfterRead_ui);
        timeTotal.push_back(timeTotalMs_f);

        printProgress(lastNs_ui, "Serial write/read latency measurement", i, numTimedSerialLoops_ui);
      }
#if defined(ON_RASPBERRY_PI) or defined(ON_ODROID_XU4)
      TimeAnalysis timeToInterruptAnalysis;
      calculateStatistics(timeToInterrupt, timeToInterruptAnalysis);

      printf("Time between start of write and interrupt:    %.3f ms (mean = %.3f, min = %.3f, max=%.3f)\n",
             timeToInterruptAnalysis.median_f, timeToInterruptAnalysis.mean_f,
             timeToInterruptAnalysis.min_f, timeToInterruptAnalysis.max_f);

      saveTimeSeries(timeToInterrupt, "startWrite_to_interrupt.gpd");
#endif
      TimeAnalysis timeOfWriteAnalysis, timeToReadAnalysis, timeTotalAnalysis;
      calculateStatistics(timeOfWrite, timeOfWriteAnalysis);
      calculateStatistics(timeToRead, timeToReadAnalysis);
      calculateStatistics(timeTotal, timeTotalAnalysis);

      printf("Time between start of write and end of write: %.3f ms (mean = %.3f, min = %.3f, max=%.3f)\n",
             timeOfWriteAnalysis.median_f, timeOfWriteAnalysis.mean_f,
             timeOfWriteAnalysis.min_f, timeOfWriteAnalysis.max_f);
      printf("Time between end of write and end of read:    %.3f ms (mean = %.3f, min = %.3f, max=%.3f)\n",
             timeToReadAnalysis.median_f, timeToReadAnalysis.mean_f,
             timeToReadAnalysis.min_f, timeToReadAnalysis.max_f);
      printf("Time between start of write and end of read:  %.3f ms (mean = %.3f, min = %.3f, max=%.3f)\n",
             timeTotalAnalysis.median_f, timeTotalAnalysis.mean_f,
             timeTotalAnalysis.min_f, timeTotalAnalysis.max_f);

      saveTimeSeries(timeOfWrite, "startWrite_to_endWrite.gpd");
      saveTimeSeries(timeToRead, "endWrite_to_endRead.gpd");
      saveTimeSeries(timeTotal, "startWrite_to_endRead.gpd");
    }

    close(serialPortHandle_i);

    return 0;
}


/*****************************************************************************
 * END OF FILE
 ******************************************************************************/

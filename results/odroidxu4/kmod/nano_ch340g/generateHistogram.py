#!/usr/bin/python
#
# Calculate histogram, density function and statistical properties.
#
# (c) 2019 by Clemens Rabe <clemens.rabe@gmail.com>

#------------------------------------------------------------------------------
# Module import
#------------------------------------------------------------------------------
import argparse
import sys
import math

#------------------------------------------------------------------------------
# Argument parsing
#------------------------------------------------------------------------------
DESCRIPTION = """
Calculate the histogram from the collected values and other statistical
properties.
"""
PARSER = argparse.ArgumentParser(description = DESCRIPTION,
                                 formatter_class = argparse.RawTextHelpFormatter)
PARSER.add_argument("-s", "--scale-factor",
                    type    = float,
                    action  = "store",
                    default = 1000.0,
                    help    = "The scale factor applied to all input data items. "
                    "For example, values stored in the input file are in "
                    "nanoseconds, but the histogram is specified and generated "
                    "in microseconds. [Default: %(default)s]")
PARSER.add_argument("-w", "--bin-width",
                    type    = float,
                    action  = "store",
                    default = 1.0,
                    help    = "The bin width. [Default: %(default)s]")
PARSER.add_argument("--bin-start",
                    type    = float,
                    action  = "store",
                    default = 0.5,
                    help    = "The bin start. [Default: %(default)s]")
PARSER.add_argument("--min-mode-frequency",
                    type    = float,
                    action  = "store",
                    default = 10.0,
                    help    = "The minimum frequency of a bin in percent to "
                    "be used in the mode detection. [Default: %(default)s]")
PARSER.add_argument("inputfile",
                    action = "store",
                    help   = "The gnuplot data input file.")
PARSER.add_argument("histogramfile",
                    action = "store",
                    help   = "The gnuplot data output file to write the histogram to. The "
                    "data is stored in a file with the suffix .gpd. A plot script to "
                    "show the histogram is written to a file with the suffix .gpi and "
                    "settings are written to a file with the suffix .gps.")
ARGS = PARSER.parse_args()

#------------------------------------------------------------------------------
# Load input data
#------------------------------------------------------------------------------
DATA = []
for line in open(ARGS.inputfile,'r').read().split('\n'):
    if line.startswith('#'):
        continue
    if line == '':
        continue
    DATA.append(float(line)*ARGS.scale_factor)

# Sort data in-place
DATA.sort()

#------------------------------------------------------------------------------
# Determine basic statistics
#------------------------------------------------------------------------------
NUMRECORDS=len(DATA)

def getQuantile(quantile):
    '''Get the n-th percent quantile, e.g., the 50% quantile (median) by calling
       getQuantile(50).'''
    idx = int((NUMRECORDS-1)*quantile/100)
    if (NUMRECORDS % 2) == 0:  # even number of elements
        return (DATA[idx] + DATA[idx+1])/2.0
    return DATA[idx]

# Minimum and maximum
MINIMUM=DATA[0]
MAXIMUM=DATA[-1]

print "data_records = %d" % NUMRECORDS
print "data_min = %f" % MINIMUM
print "data_max = %f" % MAXIMUM

# Quantiles (50% quantile is median)
QUANTILES=[ getQuantile(i) for i in range(0, 100) ]
for i in range(5, 100, 5):
    print "data_quantile_%d = %f" % (i, QUANTILES[i])

#------------------------------------------------------------------------------
# Calculate histogram
#------------------------------------------------------------------------------
class Histogram:
    def __init__(self, bin_width, bin_start):
        self.bin_width = bin_width
        self.bin_start = bin_start
        self.histogram = []

    def binIndex(self,x):
        return int(math.floor((x-self.bin_start)/self.bin_width))
    
    def binIdxToValue(self,idx):
        return self.bin_width*idx + self.bin_width*0.5 + self.bin_start
    
    def calculate(self,data):
        self.bin_idx_offset = self.binIndex(data[0])
        self.num_bins = self.binIndex(data[-1]) - self.bin_idx_offset + 1
        self.histogram = [ 0 for i in range(0, self.num_bins) ]
        self.sum = len(data)
        for d in data:
            idx = self.binIndex(d) - self.bin_idx_offset
            self.histogram[idx] = self.histogram[idx] + 1

    def getModes(self, min_frequency):
        # Non-maxima suppression
        filtered_histogram = []
        for i in range(0, len(self.histogram)):
            val = 0.0
            if (i == 0) or (self.histogram[i-1] < self.histogram[i]):
                if (i == len(self.histogram)-1) or (self.histogram[i] > self.histogram[i+1]):
                    val = self.histogram[i]
            filtered_histogram.append(val)

        # Find first mode
        max_value   = max(filtered_histogram)
        min_value_1 = int(self.sum * min_frequency / 100)
        min_value_2 = int(0.5*max_value)
        min_value   = max(min_value_1, min_value_2)
        
        candidates = []

        # Fill candidates list with tuples (x, frequency [%])
        for idx in range(0, self.num_bins):
            if filtered_histogram[idx] >= min_value:
                candidates.append((self.binIdxToValue(idx+self.bin_idx_offset),
                                   self.histogram[idx]*100.0/self.sum))

        # Sort the candidates according to the frequency in descending order
        candidates.sort(key = lambda tup: tup[1], reverse = True)

        return candidates

    def getMode(self):
        mode_idx = 0
        mode_val = self.histogram[0]

        for idx in range(1, self.num_bins):
            if self.histogram[idx] > mode_val:
                mode_idx = idx
                mode_val = self.histogram[idx]

        return (self.binIdxToValue(mode_idx+self.bin_idx_offset), mode_val*100.0/self.sum)

    def save(self, filename):
        outfile = open(filename, 'w')
        outfile.write('# Gnuplot histogram data.\n')
        for idx in range(0, self.num_bins):
            outfile.write("%f %f\n" % (self.binIdxToValue(idx+self.bin_idx_offset),
                                       self.histogram[idx]*100.0/self.sum))
        outfile.close()


HISTOGRAM = Histogram(ARGS.bin_width, ARGS.bin_start)
HISTOGRAM.calculate(DATA)
MODES = HISTOGRAM.getModes(ARGS.min_mode_frequency)

while len(MODES)==0:
    ARGS.bin_width=ARGS.bin_width+1.0
    HISTOGRAM = Histogram(ARGS.bin_width, ARGS.bin_width / 2.0)
    HISTOGRAM.calculate(DATA)
    MODES = HISTOGRAM.getModes(ARGS.min_mode_frequency)

print "histogram_num_modes = %d" % len(MODES)

for i in range(0, len(MODES)):
    print "histogram_mode_%d = %f" % (i, MODES[i][0])
    print "histogram_mode_%d_frequency = %f" % (i, MODES[i][1])


HISTOGRAM.save(ARGS.histogramfile + ".gpd")

SETTINGS = open(ARGS.histogramfile + ".gps", 'w')
SETTINGS.write('''# Settings of histogram
# Include this file using the gnuplot load command
bin_width = %f
data_records = %d
data_min = %f
data_max = %f
''' % (ARGS.bin_width, NUMRECORDS, MINIMUM, MAXIMUM))

for i in range(5, 100, 5):
    SETTINGS.write("data_quantile_%d = %f\n" % (i, QUANTILES[i]))

SETTINGS.write("histogram_num_modes = %d\n" % len(MODES))
for i in range(0, len(MODES)):
    SETTINGS.write("histogram_mode_%d = %f\n" % (i, MODES[i][0]))
    SETTINGS.write("histogram_mode_%d_frequency = %f\n" % (i, MODES[i][1]))

SETTINGS.close()

PLOTFILE = open(ARGS.histogramfile + ".gpi", 'w')

STATSSTRING = "range     = [%.2f;%.2f]\\n" % (MINIMUM, MAXIMUM)
STATSSTRING+= "80%% range = [%.2f;%.2f]\\n" % (QUANTILES[10], QUANTILES[90])
STATSSTRING+= "median    = %.2f\\n"        % QUANTILES[50]
STATSSTRING+= "modes     = "
STATSSTRING+= ", ".join([ "%.1f (%.1f%%)" % (m[0], m[1]) for m in MODES])

PLOTFILE.write('''
# Plot the histogram. Load this file with previously set up output and title, e.g.:

# set title "Interrupt latency (Write Start to Interrupt)"
# set output "interrupt_latency_write_start_to_interrupt.png"
# load "%s"

load "%s"

set boxwidth bin_width
set style fill solid 0.5 # fill style

set xlabel "Time [microseconds]"
set ylabel "Frequency [percent]"

set grid ytics lt 0 lw 1 lc rgb "#bbbbbb"
set grid xtics lt 0 lw 1 lc rgb "#bbbbbb"

# Remove any previous arrows and labels
unset arrow
unset label

# Show median
set arrow 1 from data_quantile_50, graph 0.90 to data_quantile_50, graph 0.0 fill front
set label 1 at data_quantile_50, graph 0.95 "median" center offset 0,1 front  rotate by 90

# Show statistic info
set label 2 at graph 0.10, graph 0.90 "%s" left offset 0,1 front

# Show modes
''' % (ARGS.histogramfile + ".gpi",
       ARGS.histogramfile + ".gps",
       STATSSTRING) )

for i in range(0, len(MODES)):
    PLOTFILE.write("set arrow %d from %f, graph 0.90 to %f, graph 0.0 fill front\n" % (i+3, MODES[i][0], MODES[i][0]))
    PLOTFILE.write('set label %d at %f, graph 0.95 "%d. mode" center offset 0,1 front rotate by 90\n' % (i+3, MODES[i][0], i+1))


PLOTFILE.write('''
# Calculate plot width
data_width=data_quantile_90-data_quantile_10   # 80%% of all points
min_data_width=(data_width<bin_width)?bin_width:data_width # ensure it is at least the bin_width
full_width=min_data_width*10.0/8.0             # 100%% of all points
min_full_width=(full_width<10.0*bin_width)?10.0*bin_width:full_width
extended_plot_width=(min_full_width-data_width)/2.0
plot_start=data_quantile_10-extended_plot_width
plot_end=data_quantile_90+extended_plot_width

# Plot data
plot [plot_start:plot_end] '%s' using 1:2 with boxes lc rgb"red" notitle
''' % (ARGS.histogramfile + ".gpd",) )
PLOTFILE.close()

################################################################################
# Copyright (c) 2016, Alan Barr
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# * Redistributions of source code must retain the above copyright notice, this
#   list of conditions and the following disclaimer.
#
# * Redistributions in binary form must reproduce the above copyright notice,
#   this list of conditions and the following disclaimer in the documentation
#   and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
# DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
# OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
################################################################################


################################################################################
# Usage
################################################################################
# Arg 1 - File with the expected signal.
#
# Arg 2 - File with unoptimised GDB output.
#
# Arg 3 - File with optimised GDB output.
#
# An example of this is:
#   python compare.py output/design/files/expected_signal.txt ../../../gdb_data_out_unopt.txt ../../../gdb_data_out_opt.txt 

import numpy as np
import matplotlib.pyplot as plot
import sys
import re
import os
import collections

################################################################################
# Configuration 
################################################################################

dir_plots = "./output/comparison/plots/"
dir_files = "./output/comparison/files/"

show_plots = False

################################################################################
# Helper Functions 
################################################################################
def show_plot(plot):
    if show_plots:
        plot.show()

def get_gdb_struct_variable(struct, var, data):
    struct = struct + "\s+=\s+{(.*?)}"
    var = var + "\s+=\s+(\w+),"
    struct = re.compile(struct, re.DOTALL).search(data).group(1)
    return re.compile(var, re.DOTALL).search(struct).group(1)

def get_gdb_array(arr, data):
    arr = arr + "\s+=\s+{(.*?)}"
    strs = re.compile(arr, re.DOTALL).search(data).group(1).replace(" ","").replace("\n","").split(",")
    return map(float, strs)

def get_error_info(expected, file_calculated):
    if len(expected) != len(file_calculated):
         raise Exception('!')
    diff = np.subtract(expected,file_calculated)
    max_abs = max(abs(diff))
    average = max_abs / len(expected)
    return (max_abs, average)

def get_worst_times(gdb_file):

    with open(gdb_file, "r") as f:
        data = f.read()
    
    worst_times = {}
    worst_times["Float"]     = int(get_gdb_struct_variable("fBlock",       "worst", data))
    worst_times["Q15"]       = int(get_gdb_struct_variable("q15Block",     "worst", data))
    worst_times["Q15 Fast"]  = int(get_gdb_struct_variable("q15FastBlock", "worst", data))
    worst_times["Q31"]       = int(get_gdb_struct_variable("q31Block",     "worst", data))
    worst_times["Q31 Fast"]  = int(get_gdb_struct_variable("q31FastBlock", "worst", data))
    
    return worst_times

def get_arm_calcs(gdb_file):
    with open(gdb_file, "r") as f:
        data = f.read()

    arm_calcs = {}
    arm_calcs["Float"]    = get_gdb_array("f",          data)
    arm_calcs["Q15"]      = get_gdb_array("q15",        data)
    arm_calcs["Q15 Fast"] = get_gdb_array("q15Fast",    data)
    arm_calcs["Q31"]      = get_gdb_array("q31",        data)
    arm_calcs["Q31 Fast"] = get_gdb_array("q31Fast",    data)

    return arm_calcs

def get_errors(gdb_file, calculated_file):
    with open(calculated_file, "r") as f:
        expected = []
        for line in f:
            expected.append(float(line.replace("\n","")))

    arm_calcs = get_arm_calcs(gdb_file)

    error = {}
    for k in arm_calcs:
        error[k] = np.subtract(expected, arm_calcs[k])

    return error

def get_max_error(error):
    m = {}
    for k in error:
        m[k] = (max(abs(error[k])))
    return m

def get_avg_error(error):
    a = {}
    for k in error:
        a[k] = (np.sum(abs(error[k]))/len(error[k]))
    return a

def toOrdered(dict):
    return collections.OrderedDict(sorted(dict.items()))

def get_rmsd(gdb_file, calculated_file):

    errors = get_errors(gdb_file, calculated_file)

    rmsds = {}
    for k in errors:
        rmsd = np.square(errors[k])
        rmsd = np.sum(rmsd)
        rmsd = rmsd/len(errors[k])
        rmsd = np.sqrt(rmsd)
        rmsds[k] = rmsd

    return rmsds

def table_print(title, label_keys, label_0, label_1, dict_0, dict_1):
    global file_h

    width = 25
    print >> file_h, "\n\n"
    print >> file_h, "|" + "-" * (width * 3 + 2) + "|"
    print >> file_h, "| " + title.ljust(width * 3 + 1) + "|"
    print >> file_h, "|" + "-" * (width * 3 + 2) + "|"
    print >> file_h, "|%s|%s|%s|" %(label_keys.ljust(width), label_0.ljust(width), label_1.ljust(width))

    for k in dict_0:
        print >> file_h, "|%s|%s|%s|" % (k.ljust(width), 
                            str(dict_0[k]).ljust(width),
                            str(dict_1[k]).ljust(width))
    print >> file_h, "|" + "-" * (width * 3 + 2) + "|"


################################################################################
# Run
################################################################################

file_calculated = sys.argv[1]
file_gdb_unopt  = sys.argv[2]
file_gdb_opt    = sys.argv[3]

if not os.path.exists(dir_plots):
    os.makedirs(dir_plots)

if not os.path.exists(dir_files):
    os.makedirs(dir_files)

file_h = open(dir_files + "/output.txt", "w")

################################################################################
# Times 
################################################################################
times_unopt =  toOrdered(get_worst_times(file_gdb_unopt))
times_opt   =  toOrdered(get_worst_times(file_gdb_opt))

fig, ax = plot.subplots()

index = np.arange(len(times_opt))

bar_width = 0.30

rects1 = plot.bar(index, 
                  times_unopt.values(),
                  bar_width,
                  color='g',
                  label='GCC -O0')

rects2 = plot.bar(index + bar_width, 
                 times_opt.values(),
                 bar_width,
                 color='b',
                 label='GCC -O2')

plot.title('CMSIS FIR Block Processing Time')
plot.xlabel('CMSIS FIR Function')
plot.ylabel('Time [cycles]')
plot.xticks(index + bar_width, times_opt.keys())
plot.legend()
plot.grid()

plot.savefig(dir_plots + "times.png")
show_plot(plot)
plot.close()

table_print("Times (Cycles)", "FIR Function", "Unoptimised (-O0)", "Optimised (-O2)",
            times_unopt, times_opt)

################################################################################
# Max Error
################################################################################
errors_unopt     = toOrdered(get_errors(file_gdb_unopt, file_calculated))
errors_unopt_max = toOrdered(get_max_error(errors_unopt))

errors_opt     =  toOrdered(get_errors(file_gdb_opt, file_calculated))
errors_opt_max =  toOrdered(get_max_error(errors_opt))

fig, ax = plot.subplots()

index = np.arange(len(errors_opt))

bar_width = 0.30

rects1 = plot.bar(index, 
                  np.log10(errors_unopt_max.values()),
                  bar_width,
                  color='g',
                  label='GCC -O0')

rects2 = plot.bar(index + bar_width, 
                  np.log10(errors_opt_max.values()),
                  bar_width,
                  color='b',
                  label='GCC -O2')

plot.title('CMSIS FIR Max Deviation')
plot.xlabel('CMSIS FIR Function')
plot.ylabel('Max Sample Deviation [log10 (max(|Deviation|))]')
plot.xticks(index + bar_width, errors_unopt.keys())
plot.legend()
plot.grid()

plot.savefig(dir_plots + "max_error.png")
show_plot(plot)
plot.close()

table_print("Max Error", "FIR Function", "Unoptimised (-O0)", "Optimised (-O2)",
            errors_unopt_max, errors_opt_max)

################################################################################
# RMSD Error
################################################################################
rmsds_unopt =  toOrdered(get_rmsd(file_gdb_unopt, file_calculated))
rmsds_opt   =  toOrdered(get_rmsd(file_gdb_opt, file_calculated))

index = np.arange(len(rmsds_opt))

bar_width = 0.30
rects1 = plot.bar(index, 
                  np.log10(rmsds_unopt.values()),
                  bar_width,
                  color='g',
                  label='GCC -O0')

rects2 = plot.bar(index + bar_width, 
                 np.log10(rmsds_opt.values()),
                 bar_width,
                 color='b',
                 label='GCC -O2')

plot.title('CMSIS FIR Root Mean Square Deviation (RMSD)')
plot.xlabel('CMSIS FIR Function')
plot.ylabel('Root Mean Square Deviation [log10 |RMSD|]')
plot.xticks(index + bar_width, rmsds_opt.keys())
plot.legend()
plot.grid()

plot.savefig(dir_plots + "rmsd_error.png")
show_plot(plot)
plot.close()


table_print("RMSD Error", "FIR Function", "Unoptimised (-O0)", "Optimised (-O2)", 
            rmsds_unopt, rmsds_opt)

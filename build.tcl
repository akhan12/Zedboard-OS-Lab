#*****************************************************************************************
# build.tcl
#
# Full build script for zed_os_fpga:
#   1. Creates the Vivado project (via create_project.tcl)
#   2. Runs synthesis and implementation
#   3. Generates bitstream
#   4. Exports XSA (with bitstream) to vitis/
#
# Usage (from Vivado TCL console or batch mode):
#   source /path/to/Zedboard-OS-Lab/build.tcl
#
# Or in batch mode from a terminal:
#   vivado -mode batch -source build.tcl
#
#*****************************************************************************************

set origin_dir [file dirname [info script]]

# Step 1: Create the project
puts "INFO: Creating project..."
source [file normalize "$origin_dir/create_project.tcl"]

# Step 2: Run synthesis
puts "INFO: Running synthesis..."
launch_runs synth_1 -jobs 4
wait_on_run synth_1
if {[get_property PROGRESS [get_runs synth_1]] != "100%"} {
    error "ERROR: Synthesis failed."
}

# Step 3: Run implementation
puts "INFO: Running implementation..."
launch_runs impl_1 -jobs 4
wait_on_run impl_1
if {[get_property PROGRESS [get_runs impl_1]] != "100%"} {
    error "ERROR: Implementation failed."
}

# Step 4: Generate bitstream
puts "INFO: Generating bitstream..."
launch_runs impl_1 -to_step write_bitstream -jobs 4
wait_on_run impl_1
if {[get_property PROGRESS [get_runs impl_1]] != "100%"} {
    error "ERROR: Bitstream generation failed."
}

# Step 5: Export XSA with bitstream included
set xsa_path [file normalize "$origin_dir/vitis/zed_os_fpga.xsa"]
puts "INFO: Exporting XSA to $xsa_path ..."
open_run impl_1
write_hw_platform -fixed -include_bit -force -file $xsa_path
puts "INFO: Build complete. XSA exported to $xsa_path"

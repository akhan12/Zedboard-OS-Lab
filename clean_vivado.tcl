#*****************************************************************************************
# clean_vivado.tcl
#
# Removes all Vivado-generated artifacts from the repo root:
#   - zed_os_fpga/          (generated project directory)
#   - vitis/zed_os_fpga.xsa (exported hardware platform)
#   - vivado.jou / vivado.log
#
# Usage (from Vivado TCL console):
#   source /path/to/Zedboard-OS-Lab/clean_vivado.tcl
#
# Or from a terminal (no Vivado needed — plain tclsh):
#   tclsh clean_vivado.tcl
#*****************************************************************************************

set origin_dir [file dirname [info script]]

proc remove_path {p} {
    if {[file exists $p]} {
        file delete -force $p
        puts "INFO: Removed $p"
    }
}

puts "INFO: Cleaning Vivado artifacts..."

remove_path [file normalize "$origin_dir/zed_os_fpga"]
remove_path [file normalize "$origin_dir/vitis/zed_os_fpga.xsa"]
remove_path [file normalize "$origin_dir/vivado.jou"]
remove_path [file normalize "$origin_dir/vivado.log"]

puts "INFO: Vivado clean complete."
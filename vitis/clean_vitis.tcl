#*****************************************************************************************
# clean_vitis.tcl
#
# Removes all Vitis-generated artifacts from vitis/workspace/, preserving tracked
# source files (src/ directories inside each app project).
#
# Removed:
#   workspace/.metadata/              (Eclipse workspace metadata)
#   workspace/.analytics              (Vitis analytics file)
#   workspace/zed_os_fpga_platform/   (generated BSP/platform)
#   workspace/zed_os_fpga_app_system/ (generated system project)
#   workspace/zed_vga_test_system/    (generated system project)
#   workspace/IDE.log
#   workspace/RemoteSystemsTempFiles/
#   workspace/zed_os_fpga_app/  — everything EXCEPT src/
#   workspace/zed_vga_test/     — everything EXCEPT src/
#   vitis/.xil/                       (Xilinx temp files)
#   vitis/Packages/                   (Vitis package cache)
#
# Usage (from XSCT console or plain tclsh):
#   tclsh /path/to/Zedboard-OS-Lab/vitis/clean_vitis.tcl
#*****************************************************************************************

set script_dir [file dirname [info script]]
set workspace  [file normalize "$script_dir/workspace"]

proc remove_path {p} {
    if {[file exists $p]} {
        file delete -force $p
        puts "INFO: Removed $p"
    }
}

# Remove workspace-level generated artifacts
foreach entry {
    .metadata
    .analytics
    zed_os_fpga_platform
    zed_os_fpga_app_system
    zed_vga_test_system
    RemoteSystemsTempFiles
    IDE.log
} {
    remove_path [file join $workspace $entry]
}

# Remove vitis/-level generated artifacts
foreach entry {
    .xil
    Packages
} {
    remove_path [file join $script_dir $entry]
}

# For app projects: remove everything except src/
foreach app {zed_os_fpga_app zed_vga_test} {
    set app_dir [file join $workspace $app]
    if {![file isdirectory $app_dir]} continue

    foreach entry [glob -nocomplain -directory $app_dir *] {
        if {[file tail $entry] eq "src"} continue
        file delete -force $entry
        puts "INFO: Removed $entry"
    }
}

puts "INFO: Vitis clean complete."
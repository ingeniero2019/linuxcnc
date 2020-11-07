#!/usr/bin/tclsh
package require Linuxcnc
emc_init -quick

#puts [info commands emc*]
puts "tool=[emc_tool]"
puts "tool_offset=[emc_tool_offset]"


#dng not sure this is working right
# messes up stuff
puts "try emc_set_tool_offset:"
emc_set_tool_offset 0 .222 .321
puts "tool_offset=[emc_tool_offset]"
exit

#dng: ??? not done:
#emc_set_tool_offset: need <tool> <length> <diameter>
#emc_load_tool_table: need filename

# emc commands with tool in name
#emc_set_tool_offset
#emc_load_tool_table
#emc_tool_offset
emc_set_tool_offset: need <tool> <length> <diameter>
#emc_tool

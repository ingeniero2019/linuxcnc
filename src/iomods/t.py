#!/usr/bin/python
import linuxcnc as l
s=l.stat()
s.poll()
print("tool in spindle=%d"%s.tool_in_spindle)
#print("tool_table[0]=",s.tool_table[0])

#print("\n")
#print("tool_table:",s.tool_table)

for idx in range(0,len(s.tool_table)):
    print
    print("tool_table[%d] = "%idx,s.tool_table[idx])

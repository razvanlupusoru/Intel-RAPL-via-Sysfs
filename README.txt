 * intel_rapl_power.c - Sysfs entries for Running Average Power Limit interfaces

 * Copyright (c) 2011 Razvan A. Lupusoru <razvan.lupusoru@gmail.com>

 * For documentation on Platform Specific Power Management Support using RAPL
 * please consult the Intel 64 and IA-32 Architectures Software Developer's
 * Manual Volume 3 Section 14.7 (as of April 2011)

INFORMATION ABOUT RAPL

• MSR_PKG_POWER_LIMIT allows a software agent to define power limitation for the 
   package domain. Power limitation is defined in terms of average power usage 
   (Watts) over a time window specified in MSR_PKG_POWER_LIMIT. Two power limits 
   can be specified, corresponding to time windows of different sizes. Each power limit 
   provides independent clamping control that would permit the processor cores to go 
   below OS-requested state to meet the power limits. A lock mechanism allow the soft-
   ware agent to enforce power limit settings. Once the lock bit is set, the power limit 
   settings are static and un-modifiable until next RESET.
 
• MSR_PKG_ENERGY_STATUS is a read-only MSR. It reports the actual energy use for 
   the package domain. This MSR is updated every ~1msec. It has a wraparound time 
   of around 60 secs when power consumption is high, and may be longer otherwise.
 
• MSR_PKG_POWER_INFO is a read-only MSR. It reports the package power range 
   information for RAPL usage. This MSR provides maximum/minimum values (derived 
   from electrical specification), thermal specification power of the package domain. It 
   also provides the largest possible time window for software to program the RAPL 
   interface.
  
• Power Units : Power related information (in Watts) is based on the multiplier, 1/ 2^PU;
   where PU is an unsigned integer with default value of 0011b, indicating power unit is
   in 1/8 Watts increment.

• Energy Status Units : Energy related information (in Joules) is based on the multiplier,
   1/ 2^ESU; where ESU is an unsigned integer with default value of 10000b, indicating
   energy status unit is in 15.3 micro-Joules increment.

• Time Units : Time related information (in Seconds) is based on the multiplier, 1/ 2^TU;
   where TU is an unsigned integer with default value of 1010b, indicating time unit is in
   976 micro-seconds increment.

SYSFS ENTRIES

   The following sysfs entries are created in /sys/kernel/cpu_power_info
   
 - total_energy_millijoules
	Associated with MSR_PKG_ENERGY_STATUS MSR. Value is read-only.
	Returns the actual energy use for the package domain in millijoules. The value is
	updated ~1msec. It has a wraparound time of around 60 secs when power consumption
	is high, and may be longer otherwise.
	
 - current_power_milliwatts
	Associated with MSR_PKG_ENERGY_STATUS MSR. Value is read-only.
	Returns the current power usage in milliwatts. The value is updated ~1sec.
	
 - thermal_spec_power_watts
	Associated with MSR_PKG_POWER_INFO MSR. Value is read-only.
	Returns the thermal specification power of the package domain in watts.
	
 - minimum_power_watts
	Associated with MSR_PKG_POWER_INFO MSR. Value is read-only.
	Returns the minimum power derived from the electrical spec of the package
	domain in watts.
	
 - maximum_power_watts
	Associated with MSR_PKG_POWER_INFO MSR. Value is read-only.
	Returns the maximum power derived from the electrical spec of the package
	domain in watts.
	
 - maximum_time_window_milliseconds
	Associated with MSR_PKG_POWER_INFO MSR. Value is read-only.
	Returns the largest acceptable value (in milliseconds) to program the time
	window of the MSR_PKG_POWER_LIMIT MSR.
	
 - power_limit_lock
	Associated with MSR_PKG_POWER_LIMIT MSR. Value is read/write.
	If enabled, all write attempts to the MSR are ignored until
	the next reset. 0 = disabled ; 1 = enabled
	
 - power_limit_1
	Associated with MSR_PKG_POWER_LIMIT MSR. Value is read/write.
	Sets the average power usage limit of the package domain
	corresponding to time window #1. The unit of this field is 
	specified by the “Power Units” from section above.
	
 - power_limit_2
	Associated with MSR_PKG_POWER_LIMIT MSR. Value is read/write.
	Sets the average power usage limit of the package domain
	corresponding to time window #2. The unit of this field is 
	specified by the “Power Units” from section above.
	
 - enable_limit_1
	Associated with MSR_PKG_POWER_LIMIT MSR. Value is read/write.
	Can enable/disable power limit #1. 0 = disabled ; 1 = enabled
	
 - enable_limit_2
	Associated with MSR_PKG_POWER_LIMIT MSR. Value is read/write.
	Can enable/disable power limit #2. 0 = disabled ; 1 = enabled
	
 - clamping_limit_1
	Associated with MSR_PKG_POWER_LIMIT MSR. Value is read/write.
	Sets the time window which allows going below OS-requested P/T
	state setting. The unit of this field is specified by the "Time
	Units” from section above.
	
 - clamping_limit_2
	Associated with MSR_PKG_POWER_LIMIT MSR. Value is read/write.
	Sets the time window which allows going below OS-requested P/T
	state setting. The unit of this field is specified by the "Time
	Units” from section above.
	
 - time_window_power_limit_1
	Associated with MSR_PKG_POWER_LIMIT MSR. Value is read/write.
	Indicates the length of time window over which the power limit
	#1. The numeric value is 7 bits long and is represented by the
	product of 2^Y *F; where F is a single-digit decimal floating-
	point value between 1.0 and 1.3 with the fraction digit
	represented by the two higher order bits, Y is an unsigned
	integer represented by the five lower order bits. The unit of
	this field is specified by the “Time Units” from section above.
	
 - time_window_power_limit_2
	Associated with MSR_PKG_POWER_LIMIT MSR. Value is read/write.
	Indicates the length of time window over which the power limit
	#2. The numeric value is 7 bits long and is represented by the
	product of 2^Y *F; where F is a single-digit decimal floating-
	point value between 1.0 and 1.3 with the fraction digit
	represented by the two higher order bits, Y is an unsigned
	integer represented by the five lower order bits. The unit of
	this field is specified by the “Time Units” from section above.
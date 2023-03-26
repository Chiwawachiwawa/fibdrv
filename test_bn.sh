CPUID=7
ORIG_ASLR=`cat /proc/sys/kernel/randomize_va_space`
ORIG_GOV=`cat /sys/devices/system/cpu/cpu$CPUID/cpufreq/scaling_governor`

sudo bash -c "echo 0 > /proc/sys/kernel/randomize_va_space"
sudo bash -c "echo performance > /sys/devices/system/cpu/cpu$CPUID/cpufreq/scaling_governor"

#measure the performance of fibdrv
make exp2
make unload
make load
sudo taskset -c 7 test_ground/exp2 > test_ground/exp2_plot_input
gnuplot test_ground/exp2.gp
make unload
#mv exp2.png test_ground/
# restore the original system settings
sudo bash -c "echo $ORIG_ASLR >  /proc/sys/kernel/randomize_va_space"
sudo bash -c "echo $ORIG_GOV > /sys/devices/system/cpu/cpu$CPUID/cpufreq/scaling_governor"
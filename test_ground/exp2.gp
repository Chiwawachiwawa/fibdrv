reset
set ylabel 'time(ns)'
set title 'runtime'
set key left top
set term png enhanced font 'Verdana,10'
set output 'fib_og_bn vs fib_fdbbn.png'
plot [1:817][:] \
'test_ground/exp2_plot_input' using 1:2 with linespoints linewidth 2 title "ogbn",\
'' using 1:3 with linespoints linewidth 2 title "fdbbn",\
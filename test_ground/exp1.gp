reset
set ylabel 'time(ns)'
set title 'clz_vs_nclz'
set key left top
set term png enhanced font 'Verdana,10'
set output 'nclz_vs_clz.png'
plot [2:93][:] \
'test_ground/exp1_plot_input' using 1:2 with linespoints linewidth 2 title "nclz",\
'' using 1:3 with linespoints linewidth 2 title "clz",\

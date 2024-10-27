#
# some example plots
#
# --global, -G: sets "global" parameters, that apply to all subsequent layers
#               unless specifically overwritten.
# --point, -P:  adds a point layer. You must provide text after this argument to
#               specify the data to use for this layer. This can be a file path,
#               an empty string ("") to use the global data, or a single dash ("-")
#               to provide "in-line" data, in which case -x and -y must specify
#               valid data points. For example, -x "1,2,3" -y "4,5,6" would plot
#               the points (1,4), (2,5), and (3,6).
# --line, -L:   adds a line layer
# --bar, -B:    adds a bar layer
# --labs:       specifies the start of a "labels" layer, but doesn't take any
#               input. LabelLayer specific settings should follow.
# --theme:      specifies the start of a "theme" layer, but doesn't take any
#               input. ThemeLayer specific settings should follow.
# -x, -y:       specify the x and y settings for the layer - could identify variables,
#               provide "in-line" data, or provide labels for a labels layer.
# --shape:      specify the shape of the points in a point layer
# --size:       specify the size of the points in a point layer
# --linetype:   specify the line type in a line layer
# --label:      specify the label to use for layer's legend item
./main \
    -G ./data/cubic.dat -x1 \
    -P '' -y3 --shape 7 --size 0.5 -c "black" --label "observed" \
    -L '' -y2 --linewidth 1 -c "red" --label "true" \
    -L -  -x"6,6" -y"0,1000" -s 3 -c "blue" --linetype 0 --label "x^3 = 216" \
    --labs -x "x" -y "x^3" --title "Simple Cubic Function" \
    --theme --legend_position "right"


# getting more creative in the column selection
./main -G "./data/cubic.dat" \
    --point "" -x x -y "(column('y') - column('z'))" --shape 7 --size 0.5 \
    --labs --title "Heteroskedastic Noise" -y "Residual"

# equivalent
# (note that gnuplot allows column references by "$n" or "column('name')",  but
#  bash interprets "$n" as a bash variable, so you need to escape it with a backslash)
./main -G "./data/cubic.dat" \
    --point "" -x x -y "(\$2 - \$3)" --shape 7 --size 0.5 \
    --labs --title "Heteroskedastic Noise" -y "Residual"


./main -G "./data/cubic.dat" \
    --line "" -x "(log(column('x')))" -y y \
    --labs --title "x on log-scale" -x "log(x)"

# equivalent:
./main -G "./data/cubic.dat" \
    --line "" -x "(log(\$1))" -y y \
    --labs --title "x on log-scale" -x "log(x)"

# a bar plot
./main \
    -G ./data/test.dat \
    -B '' -x1 -y2 --fill "#ff7f50" --label "Bars"  \
    -P ./data/test.dat -x1 -y2 --shape 1 --size 2 -c "#0080ff" --label "Points" \
    --labs -x "x" -y "y" --title "Bar Plot" \
    --theme --legend_position "bottom" --legend_direction "horizontal"

# TODO:
# need to add support for specifying variables for aesthetics...
#
#   for example, try the following gnuplot command which lets the pointsize vary
#   by the value of the second column and the color by the third column:
#> gnuplot
#> plot "data/cubic.dat" using 1:2:($2*0.01):($3*0.01) with points ps variable lt palette notitle

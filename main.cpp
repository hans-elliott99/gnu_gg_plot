/* gnu_gg_plot
*
* A silly, half-baked wrapper around gnuplot which tries to give it a
* ggplot2-like interface for the command line.
*
* Hans Elliott, 2024-10-24
*
*/


#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <unistd.h>
#include <cstdio>
#include <getopt.h>


/*
*  To generate a gnuplot script, need to determine what needs to be "set" in
* the lines before the plot command, and then what needs to appended to the plot command.
* A simple example script:
*
*> set key outside center bottom vertical
*> set xlabel 'x'
*> set ylabel 'y'
*> plot 'data/test.dat' using 1:2 with points linecolor rgb 'black', '' using 1:2 with lines linecolor rgb 'red'
*
* So we need a "global" state that is shared between all layers and can be modified
* by each, and a "local" state that is specific to each layer.
* Each layer needs to implement:
*   - update needed objects in global state based on the local state and layer defaults
*   - update needed objects in local state based on the global state (from earlier layers) and layer defaults
*   - generate its plot command (e.g., plot "'data.dat' using 1:2 with points...") using the updated states
*   - generate any relevant "set" commands based on the updated states
* The global state needs to:
*   - warn if a key is set more than once
*/
class Environment {
    private:
        std::unordered_map<std::string, std::string> params;
    public:
        void insert(const std::string key, const std::string& _str) {
            if (params.find(key) != params.end()) {
                std::cerr << "Warning: key '" << key << "' already set, ignoring\n";
                return;
            }
            params[key] = _str;
        }
        void fill(const std::string key, const std::string& _fill) {
            if (params.find(key) == params.end()) {
                params[key] = _fill;
            }
        }
        void replace(const std::string key, const std::string& _str) {
            params[key] = _str;
        }
        std::string get(const std::string key, const std::string _default = "") {
            if (params.find(key) == params.end()) {
                return _default;
            }
            return params[key];
        }

        std::string operator[](const std::string key) { return params[key]; };

        //TODO: temporary
        std::unordered_map<std::string, std::string> _params() { return params; }
};


std::string mkvar(const std::string x) {
    std::string y = x;
    // if y is wrapped in parentheses, it is a function call
    if (y.find("(") == 0) {
        return y;
    }
    // if x is not just an integer (index), wrap it in quotes
    if (y.find_first_not_of("0123456789") != std::string::npos) {
        y = "'" + y + "'";
    }
    return y;
}

std::string using_str_from_local(Environment& local) {
    std::string file, x_data, y_data;
    file = local.get("file");
    if (file == "-") {
        return "";
    }
    x_data = mkvar( local.get("x_data") );
    y_data = mkvar( local.get("y_data") );

    return "using " + x_data + ":" + y_data;
}


std::vector<std::string> parse_data(const std::string& data_str) {
    std::vector<std::string> data;
    size_t pos = 0;
    while (pos < data_str.size()) {
        size_t next_pos = data_str.find(',', pos);
        if (next_pos == std::string::npos) {
            next_pos = data_str.size(); // no comma found, next_pos is end of str
        }
        data.push_back( data_str.substr(pos, next_pos - pos) );
        pos = next_pos + 1;
    }
    return data;
}


// base class
class Layer {
    private:
        bool inline_data = false;
        std::vector<std::string> x;
        std::vector<std::string> y;

    protected:
        Environment& global;
        Environment& local;
        void _fill_local(const std::string key, const std::string& _layer_default) {
            // if key exists in local, do nothing. else, use global default if it
            //   is set, else fill with the layer default
            local.fill(key, global.get(key, _layer_default));
        }
        void _fill_global(const std::string key, const std::string& _layer_default) {
            // if key exists in global, do nothing and send warning that the key
            //   is already set.
            // otherwise, fill with the local value if provided, or the layer default
            global.insert(key, local.get(key, _layer_default));
        }
        std::string set_command = "";
        std::string plot_command = "";
        
        // pure virtual functions - must be implemented by derived classes
        virtual void _update_globals() = 0;
        virtual void _update_locals() = 0;
        virtual void _set_setters() = 0;
        virtual void _set_plotcmd() = 0;
        void _resolve_data_file() {
            /*
            If the local file is "", then the user is defaulting to the global file.
            If that is also blank, then we error.
            If the local file is something else, then the user is overriding the
            global file or they are explicitly specifying a file for each layer.
            Note - no reason to have a default file specified by layers because
            a file string has to be given by the user (either a path, "", or "-")
            */
            std::string local_file, global_file, resolved_file;
            local_file = local.get("file");
            global_file = global.get("file");
            if (local_file == "") {
                if (global_file != "") {
                    resolved_file = global_file;
                } else {
                    std::cerr << "Error: no global data set, nothing to use as default\n";
                    resolved_file = "";
                }
            } else {
                resolved_file = local_file;
            } 
            local.replace("file", resolved_file);
        }
        void _set_inline_data() {
            if (local.get("file") == "-") {
                inline_data = true;
                x = parse_data(local["x_data"]);
                y = parse_data(local["y_data"]);
            }
        }
    public:
        Layer(Environment& global, Environment& local)
            : global(global), local(local) {}

        // make all updates to shared objects
        void compose() {
            _update_globals();
            _update_locals();
            _set_setters();
            _set_plotcmd();
            _set_inline_data();
        }
        std::string get_set_line() {
            return set_command;
        }
        std::string get_plot_line() {
            return plot_command;
        }
        std::string str_from_inline_data() {
            std::string result = "";
            if (inline_data) {
                size_t y_size = y.size();
                for (size_t i = 0; i < x.size(); i++) {
                    if (i >= y_size) {
                        break;
                    }
                    result += x[i] + " " + y[i] + "\n";
                }
                result += "e\n";
            }
            return result;
        }
};


/* Base Layer
* The base layer is a special layer that can only be included once at the start
* of the plot, where the user can define data and aesthetic options that will then
* apply to any preceding layer, unless overwritten. This is equivalent to ggplot()
* in ggplot2 - e.g.,
* `ggplot(data = data, aes(x = x, y = y, color = color)) + geom_point(aes(shape = shape))`
* defines a data set and aesthetics to be used for all preceding layers.
* 
*/
class BaseLayer : public Layer {
    protected:
        std::string gd_file_delim = " "; // "global default -> gd"
        std::string gd_x_data = "1";
        std::string gd_y_data = "1";
        std::string gd_color = "black";
        std::string gd_shape = "1";
    public:
        using Layer::Layer;
        void _update_globals() override {
            if (local.get("file") == "-") {
                std::cerr << "Error: the base layer cannot use inline data, ignoring\n";
                global.insert("file", "");
            } else {
                global.insert("file", local.get("file", ""));
            }
            _fill_global("file_delim", gd_file_delim);
            _fill_global("x_data", gd_x_data);
            _fill_global("y_data", gd_y_data);
            _fill_global("color", gd_color);
            _fill_global("shape", gd_shape);
            return;
        };
        void _update_locals() override {
            // global layer doesn't need to update any local settings
            return;
        };
        void _set_setters() override {
            set_command += "set datafile separator '" + global["file_delim"] + "'\n";
        };
        void _set_plotcmd() override {
            // global layer doesn't set any plot commands
            return;
        };
};



// Labs layer (E.g., for axis labels)
class LabsLayer : public Layer {
    protected:
        std::string gd_title = "";
        std::string gd_xlab = "x";
        std::string gd_ylab = "y";
    public:
        using Layer::Layer;
        void _update_globals() override {
            _fill_global("title", gd_title);
            _fill_global("xlab", gd_xlab);
            _fill_global("ylab", gd_ylab);
            return;
        }
        void _update_locals() override {
            return;
        }
        void _set_setters() override {
            set_command += "set title '" + global["title"] + "'\n";
            set_command += "set xlabel '" + global["xlab"] + "'\n";
            set_command += "set ylabel '" + global["ylab"] + "'\n";
        }
        void _set_plotcmd() override {
            return;
        }
};


// Theme Layer (E.g., for legend position)
class ThemeLayer : public Layer {
    protected:
        std::string gd_legend_position = "right";
        std::string gd_legend_direction = "vertical";
    public:
        using Layer::Layer;
        void _update_globals() override {
            _fill_global("legend_position", gd_legend_position);
            _fill_global("legend_direction", gd_legend_direction);
            return;
        }
        void _update_locals() override {
            return;
        }
        void _set_setters() override {
            std::string leg;
            if (global["legend_position"] == "none") {
                leg = "off";
            } else if (global["legend_position"] == "right" ||
                       global["legend_position"] == "left") {
                leg = "outside " + global["legend_position"] + " center " +
                      global["legend_direction"];
            } else if (global["legend_position"] == "top" ||
                       global["legend_position"] == "bottom") {
                leg = "outside center " + global["legend_position"] + " " +
                      global["legend_direction"];
            } else {
                std::cerr << "Error: invalid legend position '" <<
                                   global["legend_position"] << "', ignoring\n";
                leg = "off";
            }
            set_command += "set key " + leg + "\n";
        }
        void _set_plotcmd() override {
            return; // n/a
        }
};


// Point layer
class PointLayer : public Layer {
    protected:
        std::string d_x_data = "1";
        std::string d_y_data = "1";
        std::string d_color = "black";
        std::string d_shape = "8";
        std::string d_size = "1";
        std::string d_label = "";
    public:
        using Layer::Layer;
        void _update_globals() override {
            // point layer doesn't need to update any global settings
            return;
        }
        void _update_locals() override {
            _resolve_data_file();
            _fill_local("x_data", d_x_data);
            _fill_local("y_data", d_y_data);
            _fill_local("color",  d_color);
            _fill_local("shape",  d_shape);
            _fill_local("size",  d_size);
            _fill_local("label", d_label);
        }
        void _set_setters() override {
            // point layer doesn't set any global settings
            return;
        }
        void _set_plotcmd() override {
            std::string using_str = using_str_from_local(local);            
            std::string title_str = (local["label"] == "") ? "notitle" :
                                    " title '" + local["label"] + "'";
            plot_command +=
                "'" + local["file"] + "' " + using_str + " with points"
                + " pointtype " + local["shape"]
                + " pointsize " + local["size"]
                + " linecolor rgb '" + local["color"] + "'"
                + " " + title_str
                ;
        };
};


// LineLayer
class LineLayer : public Layer {
    protected:
        std::string d_x_data = "1";
        std::string d_y_data = "1";
        std::string d_color = "black";
        std::string d_linetype = "1";
        std::string d_linewidth = "1";
        std::string d_label = "";
    public:
        using Layer::Layer;
        void _update_globals() override {
            // line layer doesn't need to update any global settings
            return;
        }
        void _update_locals() override {
            _resolve_data_file();
            _fill_local("x_data", d_x_data);
            _fill_local("y_data", d_y_data);
            _fill_local("color",  d_color);
            _fill_local("linetype",  d_linetype);
            _fill_local("linewidth",  d_linewidth);
            _fill_local("label", d_label);
        }
        void _set_setters() override {
            // line layer doesn't set any global settings
            return;
        }
        void _set_plotcmd() override {
            std::string using_str = using_str_from_local(local);            
            std::string title_str = (local["label"] == "") ? "notitle" :
                                    " title '" + local["label"] + "'";
            plot_command +=
                "'" + local["file"] + "' " + using_str + " with lines"
                + " linetype " + local["linetype"] // TODO: check if linetype is a number or string - if string, wrap in quotes
                + " linewidth " + local["linewidth"]
                + " linecolor rgb '" + local["color"] + "'"
                + " " + title_str
                ;
        };
};
    

// Bar layer
class BarLayer : public Layer {
    protected:
        std::string d_x_data = "1";
        std::string d_y_data = "1";
        std::string d_color = "black";
        std::string d_shape = "1";
        std::string gd_fillstyle = "solid";
        std::string gd_width = "0.8";
        std::string d_label = "";
    public:
        using Layer::Layer;
        void _update_globals() override {
            _fill_global("width", gd_width);
            _fill_global("fillstyle", gd_fillstyle);
        }
        void _update_locals() override {
            _resolve_data_file();
            _fill_local("x_data", d_x_data);
            _fill_local("y_data", d_y_data);
            _fill_local("color", d_color);
            _fill_local("shape", d_shape);
        }
        void _set_setters() override {
            set_command += "set style fill " + global["fillstyle"] + "\n";
            set_command += "set boxwidth " + global["width"] + " relative\n";
        }
        void _set_plotcmd() override {
            std::string using_str = using_str_from_local(local);
            std::string title_str = (local["label"] == "") ? "notitle" :
                                    " title '" + local["label"] + "'";
            plot_command +=
                "'" + local["file"] + "' " + using_str + " with boxes"
                + " fillstyle " + local["fill"]
                + " linecolor rgb '" + local["color"] + "'"
                + " " + title_str
                ;
        }
};





// dispatch function to map command line arguments to layer types
int add_layer(std::vector<std::shared_ptr<Layer>>& layers,
               int geom,
               Environment& global,
               Environment& local) {
    std::cout << "Adding layer of type '" << geom << "'\n";

    std::shared_ptr<Layer> layer_ptr;
    switch (geom) {
    case 'G':
        layer_ptr.reset(new BaseLayer(global, local));
        break;
    case 'P':
        layer_ptr.reset(new PointLayer(global, local));
        break;
    case 'L':
        layer_ptr.reset(new LineLayer(global, local));
        break;
    case 'B':
        layer_ptr.reset(new BarLayer(global, local));
        break;
    case 300:
        layer_ptr.reset(new LabsLayer(global, local));
        break;
    case 400:
        layer_ptr.reset(new ThemeLayer(global, local));
        break;
    default:
        std::cerr << "Error: unknown layer type '" << geom << "'\n";
        return EXIT_FAILURE;
    }

    layer_ptr->compose(); // compose the layer now while local env is current
    layers.push_back(layer_ptr);
    return EXIT_SUCCESS;
}


/*
* TODO:
*  - Some specifications - e.g., linetype - can take numeric values or strings
*   (e.g., 0 or "dash"). Need to check for this and wrap in quotes if needed -
*    maybe can have a helper function/friend fn of Environemnt that does this
*  - Some "sub-arguments" are only valid for certain geometry types. Need to
*    check for this and warn/error if not valid.
*
* Gnuplot resources:
*   "with": http://www.gnuplot.info/docs_4.2/node145.html
*/
int main(int argc, char* argv[]) {
    Environment global, local;
    std::vector<std::shared_ptr<Layer>> layers;

    // good reference: https://www.gnu.org/software/libc/manual/html_node/Getopt-Long-Option-Example.html
    static struct option long_options[] = {
        {"global",    required_argument, 0, 'G'},  // gg ggplot()
        {"point",     required_argument, 0, 'P'},  // gg geom_point()
        {"line",      required_argument, 0, 'L'},  // gg geom_line()
        {"bar",       required_argument, 0, 'B'},  // gg geom_bar()
        {"color",     required_argument, 0, 'c'},  // gn linecolor
        {"fill",      required_argument, 0, 'c'},  // gn color, ggplot fill is same as color in gnuplot
        {"shape",     required_argument, 0, 's'},  // gn pointtype
        {"size",      required_argument, 0, 'z'},  // gn pointsize
        {"linetype",  required_argument, 0, 't'},  // gn linetype
        {"linewidth", required_argument, 0, 'w'},  // gn linewidth
        {"fillstyle", required_argument, 0, 'f'},  // gn fillstyle, no ggplot equiv
        {"width",     required_argument, 0, 'w'},  // gg width
        {"label",     required_argument, 0, 'l'},  // gg label (for legend)
        {"labs",      no_argument,       0, 300},  // gg labs()
        {"title",     required_argument, 0, 301},  // gg title()
        {"theme",     no_argument,       0, 400},  // gg theme()
        {"legend_position", required_argument, 0, 401},  // gg theme(legend.position)
        {"legend_direction",required_argument, 0, 402},  // gg theme(legend.direction)
        {0, 0, 0, 0}
    };
    const char* short_options = "G:P:L:B:x:y:c:s:";

    int layer_count = 0;
    int opt_ix = 0;
    int opt;
    int ret;
    int current_geom;
    while ((opt = getopt_long(argc, argv, short_options, long_options, &opt_ix)) != -1) {
        switch (opt) {
            case 'G':
            case 'P':
            case 'L':
            case 'B':
            case 300: // labs
            case 400: // theme
                if (opt == 'G' && layer_count > 0) {
                    std::cerr << "Error: if global layer (--global,-G) is used, it should be set first\n";
                    return EXIT_FAILURE;
                }
                if (layer_count > 0) {
                    // finish up previous layer
                    ret = add_layer(layers, current_geom, global, local);
                    if (ret != EXIT_SUCCESS)
                        return ret;
                    // reset local env for each new layer
                    local = Environment();
                }
                if (opt < 300) {
                    // required arg for geom layers only
                    local.insert("file", optarg);
                }
                current_geom = opt;
                layer_count++;
                break;
            case 'x':
                if (current_geom == 300) {
                    local.insert("xlab", optarg);
                } else {
                    local.insert("x_data", optarg);
                }
                break;
            case 'y':
                if (current_geom == 300) {
                    local.insert("ylab", optarg);
                } else {
                    local.insert("y_data", optarg);
                }
                break;
            case 'c':
                local.insert("color", optarg);
                break;
            case 's':
                local.insert("shape", optarg);
                break;
            case 'z':
                local.insert("size", optarg);
                break;
            case 't':
                local.insert("linetype", optarg);
                break;
            case 'w':
                if (current_geom == 'L') {
                    local.insert("linewidth", optarg);
                } else if (current_geom == 'B') {
                    local.insert("width", optarg);
                }
                break;
            case 'f':
                local.insert("fillstyle", optarg);
                break;
            case 'l':
                local.insert("label", optarg);
                break;
            case 301:
                local.insert("title", optarg);
                break;
            case 401:
                local.insert("legend_position", optarg);
                break;
            case 402:
                local.insert("legend_direction", optarg);
                break;
            default:
                std::cerr << "Error: unknown option\n";
                return EXIT_FAILURE;
        }
    }
    // last layer
    ret = add_layer(layers, current_geom, global, local);
    if (ret != EXIT_SUCCESS)
        return ret;
    std::cout << "Layers: " << layer_count << std::endl;

    layer_count = 0;
    std::string set_lines = "", plot_lines = "plot ", data_lines = "", line = "";
    for (const auto& layer : layers) {
        line = layer->get_set_line();
        if (line != "") {
            set_lines += line;
        }
        line = layer->get_plot_line();
        if (line != "") {
            plot_lines += line + ",";
        } 
        data_lines += layer->str_from_inline_data();
        layer_count++;
    }
    // print items in global env
    std::cout << "__Global settings__\n";
    for (const auto& item : global._params()) {
        std::cout << item.first << ": " << item.second << std::endl;
    }

    // open pipe to gnuplot and write commands
    // note: adding --persist flag will allow user to keep the plot window open
    //       even after this program ends, but keeping the program running seems
    //       to allow for more functionality on the gnuplot side
    FILE* gnuplotPipe = popen("gnuplot", "w");
    if (!gnuplotPipe) {
        std::cerr << "Error: Could not open pipe to gnuplot.\n";
        return EXIT_FAILURE;
    }

    putchar('\n');
    // write set commands
    std::cout << set_lines << std::endl;
    fprintf(gnuplotPipe, "%s\n", set_lines.c_str());
    // write plot command
    std::cout << plot_lines << std::endl;
    fprintf(gnuplotPipe, "%s\n", plot_lines.c_str());
    // write data
    if (!data_lines.empty()) {
        fprintf(gnuplotPipe, "%s", data_lines.c_str());
    }

    fflush(gnuplotPipe);

    // send any user input to gnuplot
    std::cout << "Press enter to exit\n";
    std::cin.get();
    

    // close pipe
    pclose(gnuplotPipe);

    return 0;
}










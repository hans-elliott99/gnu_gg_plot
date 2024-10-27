/*
*  a wrapper around gnuplot
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <getopt.h>

const int buffsize = 100;
volatile sig_atomic_t keep_running = 1;
extern char *optarg;

void sigint_handler(int sig) {
    keep_running = 0;
}

void print_usage(const char *prog_name) {
    fprintf(stderr, "Usage: %s [--file <file_path>] -x <x_data> -y <y_data> [-t <plot_type>] [-c <color>] ...\n", prog_name);
    fprintf(stderr, "Example: %s --file data.csv -x 1 -y 2 -t lines -c red -x 3 -y 4 -t points -c blue\n", prog_name);
    fprintf(stderr, "Example: %s -x \"1,2,3\" -y \"1,4,9\" -t lines -c red -x \"4,5,6\" -y \"16,25,36\" -t points -c blue\n", prog_name);
}

struct plot_spec {
    char *file;
    char *x_data;
    char *y_data;
    char *geom_type;   // plot type: point, line, etc.
    char *geom_color;  // geometry color
    char *geom_shape;  // geometry shape
    // char *geom_size;   // geometry size
} plot_spec;

int count_data_points(const char *data) {
    int count = 1;
    for (int i = 0; data[i] != '\0'; i++) {
        if (data[i] == ',') {
            count++;
        }
    }
    return count;
}

void parse_data(const char *data_str, double *data, int n) {
    char *token;
    char *data_copy = strdup(data_str);
    int i = 0;

    token = strtok(data_copy, ",");
    while (token != NULL && i < n) {
        data[i++] = atof(token);
        token = strtok(NULL, ",");
    }

    free(data_copy);
}



int main(int argc, char *argv[]) {
    signal(SIGINT, sigint_handler);

    struct plot_spec plots[buffsize];
    int plot_count = 0;
    int opt;

    struct plot_spec current_plot = {NULL, NULL, NULL, "points", "black", "7"};

    struct option long_options[] = {
        {"file", required_argument, 0, 'f'},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "Px:y:t:c:s:", long_options, NULL)) != -1) {
        switch (opt) {
            case 'f':
                current_plot.file = optarg;
                break;
            case 'P':
                if (current_plot.x_data && current_plot.y_data) {
                    plots[plot_count++] = current_plot;
                    // reset current_plot
                    current_plot = (struct plot_spec){NULL, NULL, NULL, "points", "black", "7"};
                }
                break;
            case 'x':
                current_plot.x_data = optarg;
                break;
            case 'y':
                current_plot.y_data = optarg;
                break;
            case 't':
                current_plot.geom_type = optarg;
                break;
            case 'c':
                current_plot.geom_color = optarg;
                break;
            case 's':
                current_plot.geom_shape = optarg;
                break;
            default:
                fprintf(stderr, "Error: unknown option\n");
                print_usage(argv[0]);
                return EXIT_FAILURE;
        }

    }
    if (current_plot.x_data && current_plot.y_data) {
        plots[plot_count++] = current_plot; // add the last plot
    }

    if (plot_count == 0) {
        fprintf(stderr, "Error: no complete data provided\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    printf("Plot count: %d\n", plot_count);

    FILE *gp = popen("gnuplot", "w");
    if (gp == NULL) {
        perror("popen");
        return EXIT_FAILURE;
    }

    // Build the plot command
    // First, any file data layers
    fprintf(gp, "plot ");
    for (int p = 0; p < plot_count; p++) {
        if (p > 0) {
            fprintf(gp, ", ");
        }
        if (plots[p].file) {
            // file data
            int x_col = atoi(plots[p].x_data);
            int y_col = atoi(plots[p].y_data);

            fprintf(gp, "'%s' using %d:%d with %s %s %s lt rgb '%s'",
                    plots[p].file, x_col, y_col,
                    plots[p].geom_type,
                    (plots[p].geom_type[0] == 'p') ? "pt" : "",
                    (plots[p].geom_type[0] == 'p') ? plots[p].geom_shape : "",
                    plots[p].geom_color);
        } else {
            // inline data
            fprintf(gp, "'-' with %s %s %s lt rgb '%s'",
                    plots[p].geom_type,
                    (plots[p].geom_type[0] == 'p') ? "pt" : "",
                    (plots[p].geom_type[0] == 'p') ? plots[p].geom_shape : "",
                    plots[p].geom_color);
        }
    }
    fprintf(gp, "\n");

    // Send inline data for each plot
    for (int p = 0; p < plot_count; p++) {
        if (plots[p].file) {
            continue;
        }
        int nobs = count_data_points(plots[p].x_data);
        if (nobs != count_data_points(plots[p].y_data)) {
            fprintf(stderr, "Error: x and y data must have the same length\n");
            return EXIT_FAILURE;
        }

        double x[nobs], y[nobs];
        parse_data(plots[p].x_data, x, nobs);
        parse_data(plots[p].y_data, y, nobs);

        for (int i = 0; i < nobs; i++) {
            fprintf(gp, "%g %g\n", x[i], y[i]);
        }
        fprintf(gp, "e\n");
    }
    fprintf(gp, "\n");

    fflush(gp);

    while (keep_running) {
        sleep(1);
    }
    pclose(gp);

    return EXIT_SUCCESS;
}
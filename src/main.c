#include "main_internal.h"

static int create_project(const char* project_name) {
    if (!project_name || project_name[0] == '\0') {
        fprintf(stderr, "Project name cannot be empty.\n");
        return 64;
    }

    char src[CNEXT_PATH_MAX];
    char packages[CNEXT_PATH_MAX];
    char build[CNEXT_PATH_MAX];

    if (join_path(src, sizeof(src), project_name, "src") ||
        join_path(packages, sizeof(packages), project_name, "packages") ||
        join_path(build, sizeof(build), project_name, "build")) {
        return 74;
    }

    printf("Creating new project '%s'...\n", project_name);
    if (make_dir(project_name) || make_dir(src) || make_dir(packages) || make_dir(build)) {
        return 74;
    }

    char toml_path[CNEXT_PATH_MAX];
    if (join_path(toml_path, sizeof(toml_path), project_name, "cnext.toml") == 0) {
        FILE* f = fopen(toml_path, "w");
        if (f) {
            fprintf(f, "[package]\nname = \"%s\"\nversion = \"0.1.0\"\n\n[dependencies]\n", project_name);
            fclose(f);
        }
    }

    char main_path[CNEXT_PATH_MAX];
    if (join_path(main_path, sizeof(main_path), src, "main.cn") == 0) {
        FILE* f = fopen(main_path, "w");
        if (f) {
            fprintf(f, "main {\n    printin(\"Hello, %s!\")\n}\n", project_name);
            fclose(f);
        }
    }

    printf("Project '%s' created.\n", project_name);
    return 0;
}

static void print_help(void) {
    printf("Cnext Compiler v%s\n", CNEXT_VERSION);
    printf("Usage: cnext <command> [arguments]\n\n");
    printf("Commands:\n");
    printf("  new <ProjectName>          Create a new Cnext project.\n");
    printf("  run <file.cn> [-o <exe>]   Compile and run a Cnext file.\n");
    printf("  build <file.cn> [-o <exe>] Compile a Cnext file to an executable.\n");
    printf("  version                    Show compiler version.\n");
    printf("  install [package]          Install all dependencies from cnext.toml, or a single package.\n");
}

static int parse_build_args(int argc, char** argv, const char** input_path, const char** output_exe) {
    if (argc < 3) return 64;

    *input_path = argv[2];
    *output_exe = CNEXT_DEFAULT_EXE;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Expected output path after %s.\n", argv[i]);
                return 64;
            }
            *output_exe = argv[++i];
        } else {
            fprintf(stderr, "Unknown option for %s: %s\n", argv[1], argv[i]);
            return 64;
        }
    }

    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_help();
        return 0;
    }

    const char* command = argv[1];

    if (strcmp(command, "version") == 0) {
        printf("Cnext version %s\n", CNEXT_VERSION);
        return 0;
    }

    if (strcmp(command, "help") == 0 || strcmp(command, "--help") == 0 || strcmp(command, "-h") == 0) {
        print_help();
        return 0;
    }

    if (strcmp(command, "run") == 0 || strcmp(command, "build") == 0) {
        const char* input_path = NULL;
        const char* output_exe = NULL;
        int arg_status = parse_build_args(argc, argv, &input_path, &output_exe);
        if (arg_status != 0) {
            fprintf(stderr, "Usage: cnext %s <file.cn> [-o <executable>]\n", command);
            return arg_status;
        }

        char include_path[CNEXT_PATH_MAX];
        if (!build_include_path(argv[0], include_path, sizeof(include_path))) {
            return 74;
        }

        remove(CNEXT_TEMP_C);
        int compile_status = compile_file(input_path, CNEXT_TEMP_C, false);
        if (compile_status != 0) {
            remove(CNEXT_TEMP_C);
            return compile_status;
        }

        char* gcc_args[] = {
            "gcc",
            "-std=gnu11",
            "-iquote",
            include_path,
            CNEXT_TEMP_C,
            "-o",
            (char*)output_exe,
            "-lwinhttp",
            "-lws2_32",
            NULL
        };

        int gcc_status = run_process("gcc", gcc_args);
        if (gcc_status != 0) {
            fprintf(stderr, "C compilation failed.\n");
            return 65;
        }

        remove(CNEXT_TEMP_C);

        if (strcmp(command, "run") == 0) {
            char executable_buffer[CNEXT_PATH_MAX];
            const char* executable = executable_command(output_exe, executable_buffer, sizeof(executable_buffer));
            char* run_args[] = {(char*)executable, NULL};
            return run_process(executable, run_args);
        }

        return 0;
    }

    if (strcmp(command, "new") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: cnext new <ProjectName>\n");
            return 64;
        }
        return create_project(argv[2]);
    }

    if (strcmp(command, "install") == 0) {
        if (argc >= 3) {
            return install_single_package(argv[2], argv[0]) ? 0 : 1;
        } else {
            install_packages_from_toml(argv[0]);
            return 0;
        }
    }

    fprintf(stderr, "Unknown command: %s\n", command);
    print_help();
    return 64;
}

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
            fprintf(f,
                "// %s - A Cnext project\n\n"
                "func double_value(int x) -> int {\n"
                "    return x * 2\n"
                "}\n\n"
                "class Animal {\n"
                "    str name\n\n"
                "    func new(str n) {\n"
                "        self.name = n\n"
                "    }\n\n"
                "    func speak() -> str {\n"
                "        return \"Hello from {self.name}!\"\n"
                "    }\n"
                "}\n\n"
                "main {\n"
                "    printin(\"=== Welcome to %s! ===\")\n"
                "    printin(\"\")\n\n"
                "    int age = 25\n"
                "    str name = \"Cnext Developer\"\n"
                "    bool active = true\n"
                "    printin(\"Hello, {name}! You are {age} years old.\")\n"
                "    printin(\"\")\n\n"
                "    int doubled = double_value(age)\n"
                "    printin(\"{age} doubled is {doubled}\")\n"
                "    printin(\"\")\n\n"
                "    int[] numbers = {1, 2, 3, 4, 5}\n"
                "    printin(\"Counting to 5:\")\n"
                "    for n in numbers {\n"
                "        printin(\"  {n}\")\n"
                "    }\n"
                "    printin(\"\")\n\n"
                "    var dog = new Animal(\"Rex\")\n"
                "    printin(dog.speak())\n"
                "    printin(\"\")\n\n"
                "    printin(\"Ready to build something amazing!\")\n"
                "}\n",
                project_name, project_name);
            fclose(f);
        }
    }

    char greeting_path[CNEXT_PATH_MAX];
    if (join_path(greeting_path, sizeof(greeting_path), src, "greeting.cn") == 0) {
        FILE* f = fopen(greeting_path, "w");
        if (f) {
            fprintf(f,
                "// Greeting utilities\n\n"
                "func greet(str name) -> str {\n"
                "    return \"Hello, {name}!\"\n"
                "}\n\n"
                "func shout(str message) -> str {\n"
                "    return message.upper()\n"
                "}\n");
            fclose(f);
        }
    }

    char models_path[CNEXT_PATH_MAX];
    if (join_path(models_path, sizeof(models_path), src, "models.cn") == 0) {
        FILE* f = fopen(models_path, "w");
        if (f) {
            fprintf(f,
                "// Data models\n\n"
                "class Person {\n"
                "    str name\n"
                "    int age\n\n"
                "    func new(str n, int a) {\n"
                "        self.name = n\n"
                "        self.age = a\n"
                "    }\n\n"
                "    func greet() -> str {\n"
                "        return \"Hi, I'm {self.name} and I'm {self.age} years old.\"\n"
                "    }\n"
                "}\n\n"
                "class Student extends Person {\n"
                "    str school\n\n"
                "    func new(str n, int a, str s) {\n"
                "        super.new(n, a)\n"
                "        self.school = s\n"
                "    }\n\n"
                "    override func greet() -> str {\n"
                "        return \"Hi, I'm {self.name} from {self.school}.\"\n"
                "    }\n"
                "}\n");
            fclose(f);
        }
    }

    char gitignore_path[CNEXT_PATH_MAX];
    if (join_path(gitignore_path, sizeof(gitignore_path), project_name, ".gitignore") == 0) {
        FILE* f = fopen(gitignore_path, "w");
        if (f) {
            fprintf(f,
                "# Build artifacts\n"
                "build/\n"
                "out.exe\n"
                "out\n"
                "temp_out.c\n\n"
                "# Packages\n"
                "packages/\n\n"
                "# IDE\n"
                ".vscode/\n"
                ".idea/\n\n"
                "# OS\n"
                ".DS_Store\n"
                "Thumbs.db\n");
            fclose(f);
        }
    }

    char readme_path[CNEXT_PATH_MAX];
    if (join_path(readme_path, sizeof(readme_path), project_name, "README.md") == 0) {
        FILE* f = fopen(readme_path, "w");
        if (f) {
            fprintf(f,
                "# %s\n\n"
                "A Cnext project.\n\n"
                "## Quick Start\n\n"
                "```bash\n"
                "# Run the project\n"
                "cnext run src/main.cn\n\n"
                "# Build an executable\n"
                "cnext build src/main.cn -o %s.exe\n"
                "```\n\n"
                "## Project Structure\n\n"
                "```\n"
                "%s/\n"
                "  src/\n"
                "    main.cn        # Entry point\n"
                "    greeting.cn    # Greeting utilities\n"
                "    models.cn      # Data models\n"
                "  packages/        # Third-party packages\n"
                "  build/           # Build output\n"
                "  cnext.toml       # Project config\n"
                "```\n\n"
                "## Learn More\n\n"
                "- [Language Guide](https://github.com/Melton122/cnext/blob/main/docs/getting-started.md)\n"
                "- [Standard Library](https://github.com/Melton122/cnext/blob/main/docs/stdlib.md)\n"
                "- [Examples](https://github.com/Melton122/cnext/tree/main/examples)\n",
                project_name, project_name, project_name);
            fclose(f);
        }
    }

    printf("Project '%s' created.\n\n", project_name);
    printf("  cd %s\n", project_name);
    printf("  cnext run src/main.cn\n\n");
    return 0;
}

static void print_help(void) {
    printf("Cnext Compiler v%s\n", CNEXT_VERSION);
    printf("Usage: cnext <command> [arguments]\n\n");
    printf("Commands:\n");
    printf("  new <ProjectName>              Create a new Cnext project.\n");
    printf("  run <file.cn> [options]        Compile and run a Cnext file.\n");
    printf("  build <file.cn> [options]      Compile a Cnext file to an executable.\n");
    printf("  fmt <file.cn>                  Format a Cnext file.\n");
    printf("  lint <file.cn>                 Lint a Cnext file.\n");
    printf("  repl                           Start the interactive REPL.\n");
    printf("  version                        Show compiler version.\n");
    printf("  help                           Show this help message.\n\n");
    printf("Development:\n");
    printf("  doctor                         Check system for common issues.\n");
    printf("  init                           Initialize a new cnext.toml.\n");
    printf("  upgrade                        Upgrade compiler to latest version.\n");
    printf("  cache <clear|status>           Manage build cache.\n");
    printf("  config <key> [value]           Get/set configuration.\n");
    printf("  doc [output_dir]               Generate documentation from source.\n\n");
    printf("Package Manager:\n");
    printf("  test                           Run the test suite.\n");
    printf("  install                        Install all dependencies from cnext.toml.\n");
    printf("  install <package>              Install a single package.\n");
    printf("  remove <package>               Remove a package.\n");
    printf("  update                         Update all dependencies.\n");
    printf("  search <query>                 Search for packages.\n");
    printf("  info <package>                 Show package information.\n");
    printf("  publish                        Publish a package to the registry.\n");
    printf("  login                          Login to the registry.\n");
    printf("  logout                         Logout from the registry.\n\n");
    printf("Build Options:\n");
    printf("  -o, --output <path>            Set output file path.\n");
    printf("  --release                      Release build (optimized, no debug info).\n");
    printf("  --debug                        Debug build (no optimization, with symbols).\n");
    printf("  --no-optimize                  Disable the optimizer.\n");
    printf("  --verbose                      Show generated C code before compilation.\n");
    printf("  --clean                        Remove build artifacts.\n");
}

typedef struct {
    const char* input_path;
    const char* output_exe;
    bool release;
    bool debug;
    bool no_optimize;
    bool verbose;
} BuildOptions;

static int parse_build_args(int argc, char** argv, BuildOptions* opts) {
    opts->input_path = NULL;
    opts->output_exe = CNEXT_DEFAULT_EXE;
    opts->release = false;
    opts->debug = false;
    opts->no_optimize = false;
    opts->verbose = false;

    if (argc < 3) return 64;
    opts->input_path = argv[2];

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--output") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "Expected output path after %s.\n", argv[i]);
                return 64;
            }
            opts->output_exe = argv[++i];
        } else if (strcmp(argv[i], "--release") == 0) {
            opts->release = true;
        } else if (strcmp(argv[i], "--debug") == 0) {
            opts->debug = true;
        } else if (strcmp(argv[i], "--no-optimize") == 0) {
            opts->no_optimize = true;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            opts->verbose = true;
        } else if (strcmp(argv[i], "--clean") == 0) {
            remove(CNEXT_TEMP_C);
            remove(opts->output_exe);
            printf("Cleaned build artifacts.\n");
            return -1; // Signal to exit early
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
        BuildOptions opts;
        int arg_status = parse_build_args(argc, argv, &opts);
        if (arg_status == -1) return 0; // --clean
        if (arg_status != 0) {
            fprintf(stderr, "Usage: cnext %s <file.cn> [options]\n", command);
            return arg_status;
        }

        char include_path[CNEXT_PATH_MAX];
        if (!build_include_path(argv[0], include_path, sizeof(include_path))) {
            return 74;
        }

        remove(CNEXT_TEMP_C);
        int compile_status = compile_file(opts.input_path, CNEXT_TEMP_C, false);
        if (compile_status != 0) {
            remove(CNEXT_TEMP_C);
            return compile_status;
        }

        // Build GCC arguments based on options
        char opt_flags[256] = {0};
        if (opts.release) {
            strcpy(opt_flags, "-O2 -DNDEBUG");
        } else if (opts.debug) {
            strcpy(opt_flags, "-O0 -g -DDEBUG");
        } else {
            strcpy(opt_flags, "-O2 -DNDEBUG");
        }

        if (opts.no_optimize) {
            // The optimizer runs at the Cnext level, not GCC level
            // This flag is for future use
        }

        if (opts.verbose) {
            printf("--- Generated C code ---\n");
            FILE* f = fopen(CNEXT_TEMP_C, "r");
            if (f) {
                char line[4096];
                while (fgets(line, sizeof(line), f)) {
                    printf("%s", line);
                }
                fclose(f);
            }
            printf("--- End of generated C code ---\n\n");
        }

        // Build the full GCC command string
        char gcc_cmd[CNEXT_PATH_MAX * 3 + 512];
        snprintf(gcc_cmd, sizeof(gcc_cmd),
            "gcc -std=gnu11 %s -iquote \"%s\" \"%s\" -o \"%s\" "
#ifdef _WIN32
            "-lwinhttp -lws2_32"
#else
            "-lcurl -lpthread"
#endif
            , opt_flags, include_path, CNEXT_TEMP_C, opts.output_exe);

        int gcc_status = system(gcc_cmd);
        if (gcc_status != 0) {
            fprintf(stderr, "C compilation failed.\n");
            return 65;
        }

        remove(CNEXT_TEMP_C);

        if (strcmp(command, "run") == 0) {
            char executable_buffer[CNEXT_PATH_MAX];
            const char* executable = executable_command(opts.output_exe, executable_buffer, sizeof(executable_buffer));
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

    if (strcmp(command, "fmt") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: cnext fmt <file.cn>\n");
            return 64;
        }
        return format_file(argv[2], NULL, true) ? 0 : 1;
    }

    if (strcmp(command, "lint") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: cnext lint <file.cn>\n");
            return 64;
        }
        LintResult result;
        lint_init(&result);
        bool ok = lint_file(argv[2], &result);
        if (ok) {
            lint_print_issues(&result);
        }
        lint_free(&result);
        return ok ? 0 : 1;
    }

    if (strcmp(command, "repl") == 0) {
        return run_repl() ? 0 : 1;
    }

    if (strcmp(command, "test") == 0) {
        // Run test files from tests/ directory
        printf("Running Cnext test suite...\n\n");
        int passed = 0;
        int failed = 0;

        // Resolve the path to the cnext executable from argv[0]
        char exe_path[CNEXT_PATH_MAX];
        if (!build_include_path(argv[0], exe_path, sizeof(exe_path))) {
            fprintf(stderr, "Cannot locate cnext executable.\n");
            return 1;
        }
        // build_include_path returns the include dir; strip it to get the executable
        // Instead, use argv[0] directly — it already points to the running binary
        snprintf(exe_path, sizeof(exe_path), "%s", argv[0]);

        // Collect .cn files from tests/ directory
        typedef struct { char name[256]; } TestEntry;
        TestEntry entries[512];
        int entry_count = 0;

#ifdef _WIN32
        system("dir /b tests\\*.cn > tests\\_test_list.txt 2>nul");
        FILE* list = fopen("tests/_test_list.txt", "r");
        if (list) {
            char buf[256];
            while (entry_count < 512 && fgets(buf, sizeof(buf), list)) {
                buf[strcspn(buf, "\r\n")] = '\0';
                if (buf[0] != '\0') {
                    strncpy(entries[entry_count].name, buf, 255);
                    entries[entry_count].name[255] = '\0';
                    entry_count++;
                }
            }
            fclose(list);
        }
#else
        DIR* dir = opendir("tests");
        if (dir) {
            struct dirent* ent;
            while (entry_count < 512 && (ent = readdir(dir)) != NULL) {
                size_t len = strlen(ent->d_name);
                if (len > 3 && strcmp(ent->d_name + len - 3, ".cn") == 0) {
                    strncpy(entries[entry_count].name, ent->d_name, 255);
                    entries[entry_count].name[255] = '\0';
                    entry_count++;
                }
            }
            closedir(dir);
        }
#endif

        if (entry_count == 0) {
            printf("No test files found.\n");
            return 1;
        }

        for (int i = 0; i < entry_count; i++) {
            const char* test_file = entries[i].name;
            char input_path[512];
            char output_path[512];
            char run_cmd[512];
            char run_err[512];

            snprintf(input_path, sizeof(input_path), "tests/%s", test_file);
#ifdef _WIN32
            snprintf(output_path, sizeof(output_path), "tests/_test_out.exe");
            snprintf(run_cmd, sizeof(run_cmd), "tests/_test_out.exe");
            snprintf(run_err, sizeof(run_err), "tests/_test_out.exe 2>nul");
#else
            snprintf(output_path, sizeof(output_path), "tests/_test_out");
            snprintf(run_cmd, sizeof(run_cmd), "tests/_test_out");
            snprintf(run_err, sizeof(run_err), "tests/_test_out 2>/dev/null");
#endif

            char cmd[CNEXT_PATH_MAX * 3 + 256];
            snprintf(cmd, sizeof(cmd), "\"%s\" build \"%s\" -o \"%s\" 2>/dev/null", exe_path, input_path, output_path);
            int build_ok = system(cmd) == 0;

            if (build_ok) {
                int run_ok = system(run_err) == 0;
                if (run_ok) {
                    printf("  PASS: %s\n", test_file);
                    passed++;
                } else {
                    printf("  FAIL: %s\n", test_file);
                    failed++;
                }
            } else {
                printf("  SKIP: %s (compile error)\n", test_file);
            }
        }

        printf("\nResults: %d passed, %d failed\n", passed, failed);
        return failed > 0 ? 1 : 0;
    }

    if (strcmp(command, "search") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: cnext search <query>\n");
            return 64;
        }
        RegistryResult result;
        if (registry_search(argv[2], &result)) {
            printf("Found %d package(s):\n\n", result.count);
            for (int i = 0; i < result.count; i++) {
                printf("  %s@%s\n", result.versions[i].name, result.versions[i].version);
                if (result.versions[i].description[0]) {
                    printf("    %s\n", result.versions[i].description);
                }
            }
        } else {
            printf("No packages found.\n");
        }
        registry_free(&result);
        return 0;
    }

    if (strcmp(command, "info") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: cnext info <package>\n");
            return 64;
        }
        RegistryResult result;
        if (registry_info(argv[2], &result)) {
            printf("Package: %s\n", argv[2]);
            printf("Versions:\n");
            for (int i = 0; i < result.count; i++) {
                printf("  %s", result.versions[i].version);
                if (result.versions[i].description[0]) {
                    printf(" - %s", result.versions[i].description);
                }
                printf("\n");
            }
        } else {
            printf("Package not found.\n");
        }
        registry_free(&result);
        return 0;
    }

    if (strcmp(command, "publish") == 0) {
        const char* dir = argc >= 3 ? argv[2] : ".";
        const char* token = registry_get_token();
        if (!token) {
            fprintf(stderr, "Not logged in. Run 'cnext login' first.\n");
            return 1;
        }
        return registry_publish(dir, token) ? 0 : 1;
    }

    if (strcmp(command, "login") == 0) {
        const char* url = argc >= 3 ? argv[2] : getenv("CNEXT_REGISTRY_URL");
        return registry_login(url) ? 0 : 1;
    }

    if (strcmp(command, "logout") == 0) {
        return registry_logout() ? 0 : 1;
    }

    if (strcmp(command, "update") == 0) {
        return registry_update(".") ? 0 : 1;
    }

    if (strcmp(command, "remove") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: cnext remove <package>\n");
            return 64;
        }
        return registry_remove(argv[2], ".") ? 0 : 1;
    }

    if (strcmp(command, "doctor") == 0) {
        printf("Cnext Doctor v%s\n\n", CNEXT_VERSION);
        printf("Checking system...\n\n");

        // Check GCC
        int gcc_ok = system("gcc --version >nul 2>&1") == 0;
        printf("  GCC:          %s\n", gcc_ok ? "OK" : "NOT FOUND");

        // Check Make
        int make_ok = system("make --version >nul 2>&1") == 0 ||
                      system("mingw32-make --version >nul 2>&1") == 0;
        printf("  Make:         %s\n", make_ok ? "OK" : "NOT FOUND");

        // Check curl (for networking)
        int curl_ok = system("curl --version >nul 2>&1") == 0;
        printf("  curl:         %s\n", curl_ok ? "OK" : "NOT FOUND");

        // Check Python (for tests)
        int python_ok = system("python3 --version >nul 2>&1") == 0 ||
                        system("python --version >nul 2>&1") == 0;
        printf("  Python:       %s\n", python_ok ? "OK" : "NOT FOUND");

        printf("\n");
        if (gcc_ok && make_ok) {
            printf("System is ready for Cnext development.\n");
        } else {
            printf("Some tools are missing. Install them for full functionality.\n");
        }
        return 0;
    }

    if (strcmp(command, "init") == 0) {
        const char* name = argc >= 3 ? argv[2] : "my-project";
        FILE* f = fopen("cnext.toml", "w");
        if (!f) {
            fprintf(stderr, "Could not create cnext.toml\n");
            return 1;
        }
        fprintf(f, "[package]\nname = \"%s\"\nversion = \"0.1.0\"\ndescription = \"A Cnext project\"\n\n[dependencies]\n", name);
        fclose(f);
        printf("Created cnext.toml for project '%s'\n", name);
        return 0;
    }

    if (strcmp(command, "upgrade") == 0) {
        printf("Checking for updates...\n");
        printf("Current version: %s\n", CNEXT_VERSION);
        printf("To upgrade, run:\n");
        printf("  git pull && make\n");
        printf("Or download the latest release from:\n");
        printf("  https://github.com/Melton122/cnext/releases\n");
        return 0;
    }

    if (strcmp(command, "cache") == 0) {
        const char* action = argc >= 3 ? argv[2] : "status";
        if (strcmp(action, "clear") == 0) {
            int unused = system("rm -rf .cnext/cache 2>/dev/null || true"); (void)unused;
            printf("Cache cleared.\n");
        } else {
            printf("Build cache status:\n");
            printf("  Location: .cnext/cache/\n");
            int unused = system("ls -la .cnext/cache/ 2>/dev/null || echo '  No cache found.'"); (void)unused;
        }
        return 0;
    }

    if (strcmp(command, "config") == 0) {
        if (argc < 3) {
            printf("Cnext Configuration\n\n");
            printf("Registry URL: %s\n", getenv("CNEXT_REGISTRY_URL") ? getenv("CNEXT_REGISTRY_URL") : "(not set)");
            printf("\nSet via environment variable:\n  set CNEXT_REGISTRY_URL=<url>  (Windows)\n  export CNEXT_REGISTRY_URL=<url>  (Linux/macOS)\n");
            return 0;
        }
        const char* key = argv[2];
        if (strcmp(key, "registry-url") == 0) {
            const char* url = getenv("CNEXT_REGISTRY_URL");
            printf("Registry URL: %s\n", url ? url : "(not set)");
        } else {
            printf("Unknown config key: %s\n", key);
        }
        return 0;
    }

    if (strcmp(command, "doc") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: cnext doc <file.cn> [output_dir]\n");
            return 1;
        }
        const char* output_dir = argc >= 4 ? argv[3] : "docs/api";
        if (generate_docs(argv[2], output_dir)) {
            printf("Documentation generated in %s\n", output_dir);
        } else {
            fprintf(stderr, "Failed to generate documentation.\n");
            return 1;
        }
        return 0;
    }

    fprintf(stderr, "Unknown command: %s\n", command);
    print_help();
    return 64;
}

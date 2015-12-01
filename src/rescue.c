

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "deflate.h"

#ifndef RESCUE_BOOTSTRAP
#define rescue_header_only
#include "resources.c"
#endif

#define BUFFER_SIZE 128
#define DEFAULT_IDENTIFIER "rescue"
#define PLACEHOLDER "__RESCUE"
#define LINE_WIDTH 80
#define MAX_PATH 2048

#if defined(__OS2__) || defined(__WINDOWS__) || defined(WIN32) || defined(WIN64) || defined(_MSC_VER)
#define PATH_DELIMITER '\\'
#define PWD(B, L) GetCurrentDirectory(L, B)
#define ABSOLUTE_PATH(R, A, L) GetFullPathName(R, L, A, NULL)
#else
#define PATH_DELIMITER '/'
#define PWD(B, L) getcwd(B, L)
#define ABSOLUTE_PATH(R, A, L) realpath(R, A)
#endif

#ifdef RESCUE_BOOTSTRAP
#define BOOTSTRAP_WRITE(R, C, D) {char* path; path_join(RESCUE_BOOTSTRAP, R, &path); write_file(path, C, D); free(path);}
#endif 

#define PING {fprintf(stderr, "%s(%d): PING\n", __FILE__, __LINE__); }

typedef struct {
    FILE* out;
    int line;
    int total;
} compression_data;

typedef struct {
    FILE* out;
    char* placeholder;
    char* identifier;
    int state;
} source_data;

typedef struct {
    size_t inflated;
    size_t deflated;
    int metadata;
} resource_data;

typedef int (*data_callback)(const void* buffer, int len, void *user);

int path_join(const char* root, const char* path, char** out) {

    size_t rlen = strlen(root);
    size_t plen = strlen(root);
    
    *out = (char*) malloc(sizeof(char) * (rlen+plen+2));

	if (root[rlen-1] == PATH_DELIMITER)
    {
        strcpy(*out, root);
        strcpy(&((*out)[rlen]), path);
    } else if (path[0] == PATH_DELIMITER)
    {
        strcpy(*out, path);
    }
    else {
        strcpy(*out, root);
        (*out)[rlen] = PATH_DELIMITER;
        strcpy(&((*out)[rlen+1]), path);
    }

    return 1;

}

int path_split(const char* path, char** parent, char** name) {
    int i = 0;
    size_t len = strlen(path);

    if (len < 2)
        return 0;

    for (i = len-1; i >= 0; i--)
    {
        if (path[i] == PATH_DELIMITER)
        { 
            if (parent) { *parent = (char*) malloc(sizeof(char) * i); memcpy(*parent, path, i-1); (*parent)[i-1] = 0; }
            if (name) { *name = (char*) malloc(sizeof(char) * (len - i)); memcpy(*name, &(path[i+1]), len - i - 1); (*name)[len - i - 1] = 0; }
            return 1;
        }
    }

    if (name)
    { 
        *name = (char*) malloc(sizeof(char) * (len+1));
        memcpy(*name, path, len);
        (*name)[len] = 0; 
    }

    return 0;
}

mz_bool compression_callback(const void* data, int len, void *user)
{
    int i;
    const char* buffer = (const char*) data;
    compression_data* env = (compression_data*) user;

    for (i = 0; i < len; i++) 
    {
        if (env->line == 0)
        {
            fprintf(env->out, "\n\"");
        }            

        if (buffer[i] < 32 || buffer[i] == '"' || buffer[i] == '\\' || buffer[i] > 126) {
            fprintf(env->out, "\\%03o", (unsigned char)buffer[i]);
            env->line+=4;
        } else {
            fprintf(env->out, "%c", buffer[i]);
            env->line++;
        }

        if (env->line >= LINE_WIDTH) {
            fprintf(env->out, "\"");
            env->line = 0;
        } 

    }

    env->total += len;

}

resource_data generate_resource(const char* filename, FILE* out) 
{
    int f;
    tdefl_compressor compressor;
    char buffer[BUFFER_SIZE];

    resource_data result;
    compression_data cenv;
    FILE* fp = fopen(filename, "r");
    size_t length = 0;

    if (!fp)
    {
        result.inflated=-1;
        result.deflated=-1;
        return result;
    }

    cenv.out = out;
    cenv.line = 0;
    cenv.total = 0;

    tdefl_init(&compressor, &compression_callback, &cenv, TDEFL_MAX_PROBES_MASK);

    while (1) {
        int i;

        size_t n = fread (buffer, sizeof(char), BUFFER_SIZE, fp);

        if (n < 1) break;

        tdefl_compress_buffer(&compressor, buffer, n, TDEFL_SYNC_FLUSH);

        length += n;

        if (n < BUFFER_SIZE) {
            break;
        }

    }

    tdefl_compress_buffer(&compressor, NULL, 0, TDEFL_FINISH);

    if (cenv.line < LINE_WIDTH && cenv.line != 0) {
        fprintf(out, "\"");
    } 

    fprintf(out, ",\n");

    fclose(fp);

    result.metadata = 1;
    result.inflated = length;
    result.deflated = cenv.total;
}

int source_callback(const void* data, int len, void *user)
{
    source_data* env = (source_data*) user;
    const char* buffer = (const char*) data;
    int i = 0;

    // Copy the buffer to output and look for placeholder, replace it with data
    while (i < len)
    {
        // We are already recognizing placeholder
        if (env->state > 0) {
            if (buffer[i] == env->placeholder[env->state])
            {
                env->state++;
            } else {
                if (env->placeholder[env->state] == 0) // Placeholder recognized, insert data
                {

                    env->state = 0;

                    fprintf(env->out, "%s", env->identifier);
                    fputc(buffer[i], env->out);
                } else { // Not a placeholder, abort
                    int j;

                    for (j = 0; j < env->state; j++)
                        fputc(env->placeholder[j], env->out);

                    env->state = 0;

                    fputc(buffer[i], env->out);
                }

            }
        } else {
            if (buffer[i] == env->placeholder[0]) // Enter placeholder seek mode, pause output
            {
                env->state = 1;
            } else { // Output normally
                fputc(buffer[i], env->out);
            }
        }
        i++;
    }
}

int write_file(const char* source, data_callback callback, void* user)
{
    char buffer[BUFFER_SIZE];
    FILE* fp = fopen(source, "r");
    if (!fp)
        return 0;
    while (1) {
        size_t n = fread (buffer, sizeof(char), BUFFER_SIZE, fp);
        if (n < 1) break;
        callback(buffer, n, user);
        if (n < BUFFER_SIZE) break;
    }
    fclose(fp);
    return 1;
}

int help()
{

    fprintf(stderr, "rescue - A cross-platform resource compiler.\n\n");
    fprintf(stderr, "Usage: rescue [-h] [-v] [-o <path>] [-a] [-b] [-r <path>] [-p <prefix>] <file1> ...\n");
    fprintf(stderr, " -h\t\tPrint help.\n");
    fprintf(stderr, " -v\t\tBe verbose.\n");
    fprintf(stderr, " -o <path>\tOutput the resulting C source to the given file instead of printing it to standard output.\n\t\tThis flag can only be used before any source file is provided.\n");
    // NOT IMPLEMENTED YET!
    //fprintf(stderr, " -r <path>\tSet the root direcotry for the following files.\n\t\tThe embedded names of the files will be relative to this path.\n");
    fprintf(stderr, " -a\t\tSet the naming mode of the files to absolute name.\n\t\tThe embedded names of the files will include the full absolute name of the file.\n");
    fprintf(stderr, " -b\t\tSet the naming mode of the files to file basename.\n\t\tThe embedded names of the files will include only the basename of the file.\n");
    fprintf(stderr, " -p <prefix>\tUse the following alphanumerical string as a prefix for the functions and\n\t\tvariables in the generated file (instead of `rescue`).\n\t\tThis flag can only be used before any source file is provided.\n");
    fprintf(stderr, "\n");

}

#define NAMING_MODE_ABSOLUTE 0
#define NAMING_MODE_RELATIVE 1
#define NAMING_MODE_BASENAME 2

#define MAX_IDENTIFIER 64

#define VERBOSE(...) if (verbose) { fprintf(stderr, __VA_ARGS__); }

int main(int argc, char** argv)
{
    int i;
    FILE* out = stdout;
    char root[MAX_PATH];
    char identifier[MAX_IDENTIFIER];
    int processed_files = 0;
    int naming_mode = NAMING_MODE_BASENAME;
    source_data ctx;
    int verbose = 0;

    char** resource_names = (char**) malloc(sizeof(char*) * argc);
    int* resource_metadata = (int*) malloc(sizeof(int) * argc);
    size_t* resource_length_inflated = (size_t*) malloc(sizeof(size_t) * argc);
    size_t* resource_length_deflated = (size_t*) malloc(sizeof(size_t) * argc);

    PWD(root, MAX_PATH); // Get the current directory
    strcpy(identifier, DEFAULT_IDENTIFIER);

    for (i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-h") == 0)
        {

            if (argc == 2) help();
            continue;

        } else if (strcmp(argv[i], "-o") == 0) 
        {
            if (out != stdout)
            {
                fprintf(stderr, "Output already set.\n");
                continue;
            }

            if (processed_files > 0)
            {
                fprintf(stderr, "Output already set.\n");
                continue;
            }

            out = fopen(argv[++i], "w");

            VERBOSE("Writing to file %s.\n", argv[i]);

            continue;

        } else if (strcmp(argv[i], "-v") == 0) 
        {

            verbose = 1;

            continue;

        } else if (strcmp(argv[i], "-b") == 0) 
        {

            naming_mode = NAMING_MODE_BASENAME;

            continue;

        } else if (strcmp(argv[i], "-a") == 0) 
        {

            naming_mode = NAMING_MODE_ABSOLUTE;

            continue;

        } else if (strcmp(argv[i], "-r") == 0) 
        {

            if ((i + 1) == argc)
            {
                fprintf(stderr, "Missing directory.\n");
                continue;
            }

            strcpy(root, argv[++i]);
            naming_mode = NAMING_MODE_RELATIVE;

            continue;

        } else if (strcmp(argv[i], "-p") == 0) 
        {

            if ((i + 1) == argc)
            {
                fprintf(stderr, "Missing identifier.\n");
                continue;
            }

            if (processed_files > 0)
            {
                fprintf(stderr, "Output has already started.\n");
                continue;
            }

            strcpy(identifier, argv[++i]);

            continue;
        }

        ctx.state = 0;    
        ctx.out = out;
        ctx.placeholder = PLACEHOLDER;
        ctx.identifier = identifier;

        // copy resource

        if (processed_files == 0)
        {
#ifndef RESCUE_BOOTSTRAP
            rescue_get_resource("inflate.c", &source_callback, &ctx);
#else
            BOOTSTRAP_WRITE("inflate.c", &source_callback, &ctx);
#endif
            fprintf(out, "#ifndef %s_header_only\n", identifier);
            fprintf(out, "static const unsigned char* %s_resource_data[] = {", identifier);
        }

        {
            VERBOSE("Generating resource from %s.\n", argv[i]);
            resource_data r = generate_resource(argv[i], out);

            if (r.deflated == -1)
            {
                fprintf(stderr, "File %s does not exist or cannot be opened for reading, skipping.\n", argv[i]);
                continue;
            } else
            {
                resource_length_inflated[processed_files] = r.inflated;
                resource_length_deflated[processed_files] = r.deflated;
                resource_metadata[processed_files] = r.metadata;
                switch(naming_mode)
                {
                case NAMING_MODE_BASENAME:
                {
                    path_split(argv[i], NULL, &resource_names[processed_files]);
                    break;
                }
                case NAMING_MODE_RELATIVE:
                {
                    //TODO: not implemented yet!
                    break;
                }
                case NAMING_MODE_ABSOLUTE:
                {
                    char* abspath = (char*) malloc(sizeof(char) * MAX_PATH);
                    ABSOLUTE_PATH(argv[i], abspath, MAX_PATH);
                    resource_names[processed_files] = abspath;
                    break;
                }
                }

            }
 
        }

        processed_files++;
    }


    if (processed_files > 0)
    {
        int f;

        fprintf(out, " 0};\n");
        fprintf(out, "static const char* %s_resource_names[] = {\n", identifier);
        for (f = 0; f < processed_files; f++)
            fprintf(out, "\"%s\",", resource_names[f]);
        fprintf(out, " 0};\n");

        fprintf(out, "static const int %s_resource_metadata[] = {\n", identifier);
        for (f = 0; f < processed_files; f++) 
                fprintf(out, "%d,", resource_metadata[f]);
        fprintf(out, " 0};\n");

        fprintf(out, "static const size_t %s_resource_length_inflated[] = {\n", identifier);
        for (f = 0; f < processed_files; f++) 
            fprintf(out, "%ld,", resource_length_inflated[f]);
        fprintf(out, " 0};\n");

        fprintf(out, "static const size_t %s_resource_length_deflated[] = {\n", identifier);
        for (f = 0; f < processed_files; f++) 
            fprintf(out, "%ld,", resource_length_deflated[f]);
        fprintf(out, " 0};\n");

        free(resource_length_inflated);
        free(resource_length_deflated);
        free(resource_metadata);

        for (f = 0; f < processed_files; f++)
            free(resource_names[f]);
        free(resource_names);

        fprintf(out, "#endif\n");

#ifndef RESCUE_BOOTSTRAP
        rescue_get_resource("template.c", &source_callback, &ctx);
#else
        BOOTSTRAP_WRITE("template.c", &source_callback, &ctx);
#endif

    }

    if (argc < 2) {
        help();
        fprintf(stderr, "No input given.\n");
        return -1;
    }

    fflush(out);

    return 0;

}


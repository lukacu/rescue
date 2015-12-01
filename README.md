# rescue - A cross-platform resource compiler

Rescue is a cross-platform resource compiler for C and C++. It can be used to transform one or more static resource files into a C source file that can be compiled into your project to access the resources at runtime from memory. It also includes some basic compression based on the [miniz library](https://code.google.com/p/miniz/) to save space.

## Compiling

You can use CMake to compile the project into an executable, first a bootstrap version of the compiler will be generated that will then generate the final compiler. The project has no external dependencies and should work on multiple platforms (although that was not extensively tested and could require some minor adjustments).

## Using compiler

To use the compiler simply run it in the terminal and provide the list of files as an input.

You can also use the following flags to modify the output:

 * `-h` - Print help.
 * `-v` - Be verbose.
 * `-o <path>` - Output the resulting C source to the given file instead of printing it to standard output. This flag can only be used before any source file is provided.
 * `-a` - Set the naming mode of the files to absolute name. The embedded names of the files will include the full absolute name of the file.
 * `-b` - Set the naming mode of the files to file basename. The embedded names of the files will include only the basename of the file.
 * `-p <prefix>` - Use the following alphanumerical string as a prefix for the functions and variables in the generated file (instead of `rescue`). This flag can only be used before any source file is provided.

Here are some examples of using the compiler (using Unix shell syntax):

 * Compile three resources into a source file and output it to standard output: `rescue image1.png image2.jpg text.txt`
 * Compile three resources into a source file and output it into `resources.c`: `rescue -o resources.c image1.png image2.jpg text.txt`
 * Set the used prefix to a given string (`resources` instead of `rescue`): `rescue -o resources.c -p resources image1.png image2.jpg text.txt`

## Using resources

The generated C source file supports the following public functions (note that the prefix `rescue` may be different if you have manually set it):

 * `int rescue_has_resource(const char* name)` - Checks if a resource for a given name exists.
 * `int rescue_get_resource(const char* name, rescue_data_callback callback, void *user)` - Retrieves resource in chunks using callback function `callback(const void* buffer, int len, void *user)`.
 * `int rescue_copy_resource(const char* name, char** buffer, size_t* size)` - Retrieves the entire resource in a new buffer that has to be released when it is not used anymore.
 * `int rescue_get_length(const char* name, size_t* compressed, size_t* uncompressed)` - Get the compressed and uncompressed size of the resource.

You can include the entire file into your source (no need to compile it separately), however, if you wish to include it as a header file use rescue_header_only define as in this example (note that the prefix `rescue` may be different if you have manually set it):

```
#define rescue_header_only
#include "resources.c"
```



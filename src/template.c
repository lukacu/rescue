
#ifdef __RESCUE_header_only

#ifndef __RESCUE_RESOURCES_H
#define __RESCUE_RESOURCES_H

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*rescue_data_callback)(const void* buffer, int len, void *user);

int __RESCUE_has_resource(const char* name);

int __RESCUE_get_resource(const char* name, rescue_data_callback callback, void *user);

int __RESCUE_copy_resource(const char* name, char** buffer, size_t* size);

int __RESCUE_get_length(const char* name, size_t* compressed, size_t* uncompressed);

#ifdef __cplusplus
}
#endif

#endif

#else

#define __RESCUE_META_COMPRESSION 1
#define __RESCUE_CHUNK_SIZE 32*1024

typedef int (*rescue_data_callback)(const void* buffer, int len, void *user);

typedef struct rescue_copy_state {void* buffer; int size; int position; } rescue_copy_state;

int __RESCUE_copy_callback(const void* buffer, int len, void *user)
{
    rescue_copy_state* state = (rescue_copy_state *) user;

    memcpy(&(((char*)state->buffer)[state->position]), buffer, len);
    state->position += len;

    return 1;
}

int __RESCUE_inflate_resource(int i, rescue_data_callback callback, void *user)
{

    int result = 3;
    int segment = 0;
    const char** pIn_bufs = __RESCUE_resource_data[i];
    tinfl_decompressor decomp;
    mz_uint8 *pDict = (mz_uint8*)malloc(__RESCUE_CHUNK_SIZE); size_t dict_ofs = 0;
    size_t pIn_buf_size;
    if (!pDict)
        return 0;

    tinfl_init(&decomp);
    for (segment = 0;  ; segment++)
    {
        size_t in_buf_ofs = 0;
        mz_uint32 inf_flags = 0;
        const char* pIn_buf = pIn_bufs[segment];

        if (!pIn_buf) break;

        if (pIn_bufs[segment + 1]) {
            inf_flags = TINFL_FLAG_HAS_MORE_INPUT;
            pIn_buf_size = __RESCUE_SEGMENT_LENGTH;
        } else {
            inf_flags = 0;
            pIn_buf_size = __RESCUE_resource_length_deflated[i] - (segment) * __RESCUE_SEGMENT_LENGTH;
        }
        for ( ; ; )
        {
            size_t in_buf_size = pIn_buf_size - in_buf_ofs, dst_buf_size = __RESCUE_CHUNK_SIZE - dict_ofs;
            tinfl_status status = tinfl_decompress(&decomp, (const mz_uint8*)pIn_buf + in_buf_ofs,
                                                   &in_buf_size, pDict, pDict + dict_ofs, &dst_buf_size, inf_flags);
            in_buf_ofs += in_buf_size;
            if ((dst_buf_size) && (!callback(pDict + dict_ofs, (int)dst_buf_size, user))) {
                result = TINFL_STATUS_FAILED;
                break;
            }
            dict_ofs = (dict_ofs + dst_buf_size) & (__RESCUE_CHUNK_SIZE - 1);
            if (status == TINFL_STATUS_NEEDS_MORE_INPUT) {
                if (in_buf_ofs == pIn_buf_size)
                    break;
                else
                    continue;
            }

            if (status != TINFL_STATUS_HAS_MORE_OUTPUT)
            {
                result = (status == TINFL_STATUS_DONE);
                break;
            }

        }

        if (result != 3)
            break;
    }
    free(pDict);

    return result;
}

int __RESCUE_has_resource(const char* name)
{
    int i = 0;
    while (__RESCUE_resource_names[i])
    {
        if (strcmp(name, __RESCUE_resource_names[i]) == 0)
        {
            return 1;
        }
        i++;
    }
    return 0;
}

int __RESCUE_get_resource(const char* name, rescue_data_callback callback, void *user)
{
    int i = 0;
    while (__RESCUE_resource_names[i])
    {
        if (strcmp(name, __RESCUE_resource_names[i]) == 0)
        {
            if (__RESCUE_resource_metadata[i] & __RESCUE_META_COMPRESSION) {
                __RESCUE_inflate_resource(i, callback, user);
            } else {
                callback(__RESCUE_resource_data[i], __RESCUE_resource_length_deflated[i], user);
            }

            return 1;
        }
        i++;
    }
    return 0;
}

int __RESCUE_copy_resource(const char* name, char** buffer, size_t* size)
{

    int i = 0;
    while (__RESCUE_resource_names[i])
    {
        if (strcmp(name, __RESCUE_resource_names[i]) == 0)
        {
            rescue_copy_state state;
            *size = __RESCUE_resource_length_inflated[i];
            *buffer = (char*) malloc(sizeof(char) * (*size));
            state.buffer = *buffer;
            state.position = 0;
            state.size = *size;

            if (__RESCUE_resource_metadata[i] & __RESCUE_META_COMPRESSION) {

                __RESCUE_inflate_resource(i, &__RESCUE_copy_callback, &state);

            } else {
                int segment;
                size_t pIn_buf_size;
                const char** pIn_bufs = __RESCUE_resource_data[i];
                for (segment = 0;  ; segment++)
                {
                    size_t in_buf_ofs = 0;
                    const char* pIn_buf = pIn_bufs[segment];
                    if (!pIn_buf) break;

                    if (pIn_bufs[segment + 1]) {
                        pIn_buf_size = __RESCUE_SEGMENT_LENGTH;
                    } else {
                        pIn_buf_size = __RESCUE_resource_length_inflated[i] - (segment) * __RESCUE_SEGMENT_LENGTH;
                    }

                    __RESCUE_copy_callback(pIn_buf, pIn_buf_size, &state);

                }
            }

            return 1;
        }
        i++;
    }

    return 0;

}

int __RESCUE_get_length(const char* name, size_t* compressed, size_t* uncompressed)
{

    int i = 0;
    while (__RESCUE_resource_names[i])
    {
        if (strcmp(name, __RESCUE_resource_names[i]) == 0)
        {
            if (compressed)
                *compressed = __RESCUE_resource_length_deflated[i];

            if (uncompressed)
                *uncompressed = __RESCUE_resource_length_inflated[i];

            return 1;
        }
        i++;
    }

    return 0;

}

#ifdef __cplusplus
}
#endif

#endif

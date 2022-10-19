#include <gvox/gvox.h>

#include <stdio.h>
#include <memory.h>
#include <assert.h>

#if __linux__
#define EXPORT
#elif _WIN32
#define EXPORT __declspec(dllexport) __stdcall
#endif

extern "C" {
GVoxPayload EXPORT gvox_create_payload(GVoxScene scene) {
    // TODO: Implement this
    GVoxPayload result = {0};
    printf("creating magicavoxel payload from the %zu nodes at %p\n", scene.node_n, (void *)scene.nodes);
    return result;
}

void EXPORT gvox_destroy_payload(GVoxPayload payload) {
    // TODO: Implement this
    delete[] payload.data;
    printf("destroying magicavoxel payload at %p with size %zu\n", payload.data, payload.size);
}

typedef struct {
    uint8_t b0, b1, b2, b3;
} ChunkID;

static const ChunkID CHUNK_ID_MAIN = {'M', 'A', 'I', 'N'};
static const ChunkID CHUNK_ID_SIZE = {'S', 'I', 'Z', 'E'};
static const ChunkID CHUNK_ID_XYZI = {'X', 'Y', 'Z', 'I'};
static const ChunkID CHUNK_ID_RGBA = {'R', 'G', 'B', 'A'};
static const ChunkID CHUNK_ID_nTRN = {'n', 'T', 'R', 'N'};
static const ChunkID CHUNK_ID_nGRP = {'n', 'G', 'R', 'P'};
static const ChunkID CHUNK_ID_nSHP = {'n', 'S', 'H', 'P'};
static const ChunkID CHUNK_ID_IMAP = {'I', 'M', 'A', 'P'};
static const ChunkID CHUNK_ID_LAYR = {'L', 'A', 'Y', 'R'};
static const ChunkID CHUNK_ID_MATL = {'M', 'A', 'T', 'L'};
static const ChunkID CHUNK_ID_MATT = {'M', 'A', 'T', 'T'};
static const ChunkID CHUNK_ID_rOBJ = {'r', 'O', 'B', 'J'};
static const ChunkID CHUNK_ID_rCAM = {'r', 'C', 'A', 'M'};

static int parse_int32(uint8_t **buffer_ptr) {
    int result = *(int *)(*buffer_ptr);
    *buffer_ptr += sizeof(result);
    return result;
}
static void parse_string(uint8_t **buffer_ptr) {
    int string_size = *(int *)(*buffer_ptr);
    *buffer_ptr += sizeof(string_size);
    printf("     - got string at %p with size %d\n", (void *)*buffer_ptr, string_size);
    *buffer_ptr += string_size;
}
static void parse_dict(uint8_t **buffer_ptr) {
    int entry_n = *(int *)(*buffer_ptr);
    *buffer_ptr += sizeof(entry_n);
    printf("   - parsing dictionary at %p with %d entries\n", (void *)*buffer_ptr, entry_n);
    for (int entry_i = 0; entry_i < entry_n; ++entry_i) {
        parse_string(buffer_ptr);
    }
}

static void parse_chunk_SIZE(GVoxScene *result, uint8_t **buffer_ptr) {
    int size_x = parse_int32(buffer_ptr);
    int size_y = parse_int32(buffer_ptr);
    int size_z = parse_int32(buffer_ptr);
    result->nodes[0].size_x = (size_t)size_x;
    result->nodes[0].size_y = (size_t)size_y;
    result->nodes[0].size_z = (size_t)size_z;
    printf(" - dimensions %d, %d, %d\n", size_x, size_y, size_z);
}
static void parse_chunk_XYZI(GVoxScene *result, uint8_t **buffer_ptr) {
    int voxel_count = parse_int32(buffer_ptr);
    printf(" - voxel count %d\n", voxel_count);
    size_t size_x = result->nodes[0].size_x;
    size_t size_y = result->nodes[0].size_y;
    size_t size_z = result->nodes[0].size_z;
    size_t size = size_x * size_y * size_z;
    for (int i = 0; i < voxel_count; ++i) {
    }
    // seek past voxel data
    *buffer_ptr += sizeof(uint32_t) * (size_t)voxel_count;
}
static void parse_chunk_RGBA(GVoxScene *result, uint8_t **buffer_ptr) {
    *buffer_ptr += sizeof(uint32_t) * 256;
    printf(" - RGBA palette\n");
}
static void parse_chunk_nTRN(GVoxScene *result, uint8_t **buffer_ptr) {
    int node_id = parse_int32(buffer_ptr);
    printf(" - nTRN node_id %d\n", node_id);
    parse_dict(buffer_ptr);
    int child_node_id = parse_int32(buffer_ptr);
    int reserved_id = parse_int32(buffer_ptr);
    int layer_id = parse_int32(buffer_ptr);
    int frame_n = parse_int32(buffer_ptr);
    printf(" - child, reserved, layer, frame_n: %d, %d, %d, %d\n", child_node_id, reserved_id, layer_id, frame_n);
    for (int frame_i = 0; frame_i < frame_n; ++frame_i) {
        parse_dict(buffer_ptr);
    }
}
static void parse_chunk_nGRP(GVoxScene *result, uint8_t **buffer_ptr) {
    int node_id = parse_int32(buffer_ptr);
    printf(" - nGRP node_id %d\n", node_id);
    parse_dict(buffer_ptr);
    int child_node_n = parse_int32(buffer_ptr);
    for (int child_i = 0; child_i < child_node_n; ++child_i) {
        int child_id = parse_int32(buffer_ptr);
        printf("   - child_id %d\n", child_id);
    }
}
static void parse_chunk_nSHP(GVoxScene *result, uint8_t **buffer_ptr) {
    int node_id = parse_int32(buffer_ptr);
    printf(" - nSHP node_id %d\n", node_id);
    parse_dict(buffer_ptr);
    int child_node_n = parse_int32(buffer_ptr);
    for (int child_i = 0; child_i < child_node_n; ++child_i) {
        int model_id = parse_int32(buffer_ptr);
        printf("   - model_id %d\n", model_id);
        parse_dict(buffer_ptr);
    }
}
static void parse_chunk_IMAP(GVoxScene *result, uint8_t **buffer_ptr) {
    (void)buffer_ptr;
    assert(0 && "BRUH");
}
static void parse_chunk_LAYR(GVoxScene *result, uint8_t **buffer_ptr) {
    int node_id = parse_int32(buffer_ptr);
    printf(" - LAYR node_id %d\n", node_id);
    parse_dict(buffer_ptr);
    int reserved_id = parse_int32(buffer_ptr);
    printf("   - reserved_id %d\n", reserved_id);
}
static void parse_chunk_MATL(GVoxScene *result, uint8_t **buffer_ptr) {
    int material_id = parse_int32(buffer_ptr);
    printf(" - MATL %d\n", material_id);
    parse_dict(buffer_ptr);
}
static void parse_chunk_MATT(GVoxScene *result, uint8_t **buffer_ptr) {
    (void)buffer_ptr;
    assert(0 && "MATT Deprecated");
}
static void parse_chunk_rOBJ(GVoxScene *result, uint8_t **buffer_ptr) {
    printf(" - rOBJ\n");
    parse_dict(buffer_ptr);
}
static void parse_chunk_rCAM(GVoxScene *result, uint8_t **buffer_ptr) {
    int camera_id = parse_int32(buffer_ptr);
    printf(" - rCAM %d\n", camera_id);
    parse_dict(buffer_ptr);
}

static int parse_chunk(GVoxScene *result, uint8_t **buffer_ptr, uint8_t **buffer_sentinel) {
    if (!buffer_ptr || !*buffer_ptr)
        return 1;
    ChunkID chunk_id = *(ChunkID *)(*buffer_ptr);
    *buffer_ptr += sizeof(chunk_id);
    int self_size = parse_int32(buffer_ptr);
    int children_size = parse_int32(buffer_ptr);
    printf("Parsing chunk %c%c%c%c\n", chunk_id.b0, chunk_id.b1, chunk_id.b2, chunk_id.b3);
    printf(" - self size     %d\n", self_size);
    printf(" - children size %d\n", children_size);
    if (memcmp(&chunk_id, &CHUNK_ID_MAIN, sizeof(ChunkID)) == 0) {
        *buffer_ptr += self_size;
        printf("pointers were %p and %p\n", (void *)*buffer_ptr, (void *)*buffer_sentinel);
        *buffer_sentinel = (*buffer_ptr) + children_size;
        printf("pointers now  %p and %p\n", (void *)*buffer_ptr, (void *)*buffer_sentinel);
    } else if (memcmp(&chunk_id, &CHUNK_ID_SIZE, sizeof(ChunkID)) == 0) {
        parse_chunk_SIZE(result, buffer_ptr);
    } else if (memcmp(&chunk_id, &CHUNK_ID_XYZI, sizeof(ChunkID)) == 0) {
        parse_chunk_XYZI(result, buffer_ptr);
    } else if (memcmp(&chunk_id, &CHUNK_ID_RGBA, sizeof(ChunkID)) == 0) {
        parse_chunk_RGBA(result, buffer_ptr);
    } else if (memcmp(&chunk_id, &CHUNK_ID_nTRN, sizeof(ChunkID)) == 0) {
        parse_chunk_nTRN(result, buffer_ptr);
    } else if (memcmp(&chunk_id, &CHUNK_ID_nGRP, sizeof(ChunkID)) == 0) {
        parse_chunk_nGRP(result, buffer_ptr);
    } else if (memcmp(&chunk_id, &CHUNK_ID_nSHP, sizeof(ChunkID)) == 0) {
        parse_chunk_nSHP(result, buffer_ptr);
    } else if (memcmp(&chunk_id, &CHUNK_ID_IMAP, sizeof(ChunkID)) == 0) {
        parse_chunk_IMAP(result, buffer_ptr);
    } else if (memcmp(&chunk_id, &CHUNK_ID_LAYR, sizeof(ChunkID)) == 0) {
        parse_chunk_LAYR(result, buffer_ptr);
    } else if (memcmp(&chunk_id, &CHUNK_ID_MATL, sizeof(ChunkID)) == 0) {
        parse_chunk_MATL(result, buffer_ptr);
    } else if (memcmp(&chunk_id, &CHUNK_ID_MATT, sizeof(ChunkID)) == 0) {
        parse_chunk_MATT(result, buffer_ptr);
    } else if (memcmp(&chunk_id, &CHUNK_ID_rOBJ, sizeof(ChunkID)) == 0) {
        parse_chunk_rOBJ(result, buffer_ptr);
    } else if (memcmp(&chunk_id, &CHUNK_ID_rCAM, sizeof(ChunkID)) == 0) {
        parse_chunk_rCAM(result, buffer_ptr);
    } else {
        printf("pointers were %p and %p\n", (void *)*buffer_ptr, (void *)*buffer_sentinel);
        return 1;
    }
    return 0;
}

GVoxScene EXPORT gvox_parse_payload(GVoxPayload payload) {
    // TODO: Implement this
    GVoxScene result = {0};
    printf("parsing magicavoxel payload at %p with size %zu\n", payload.data, payload.size);
    uint8_t *buffer_ptr = (uint8_t *)payload.data;
    uint8_t *buffer_sentinel = (uint8_t *)payload.data + payload.size;
    uint8_t start_bytes[4] = {'V', 'O', 'X', ' '};
    result.node_n = 1;
    result.nodes = new GVoxSceneNode[result.node_n];
    if (memcmp(&start_bytes, buffer_ptr, sizeof(start_bytes)) != 0)
        return result;
    buffer_ptr += sizeof(start_bytes);
    int version = *(int *)(buffer_ptr);
    buffer_ptr += sizeof(version);
    printf("Found MagicaVoxel file has version %d\n", version);
    while (buffer_ptr < buffer_sentinel) {
        if (parse_chunk(&result, &buffer_ptr, &buffer_sentinel) != 0)
            break;
    }
    return result;
}
}

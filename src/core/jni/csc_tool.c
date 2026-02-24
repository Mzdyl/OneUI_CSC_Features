#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <zlib.h>
#include "lib/cJSON.h"

#define MAX_BUFFER_SIZE 4096
#define MAX_FEATURES 10000

// --- 解码/加密 核心数据 (来自 decode_csc.c) ---
int salts[] = {65, -59, 33, -34, 107, 28, -107, 55, 78, 17, -81, 6, -80, -121, -35, -23, 72, 122, -63, -43, 68, 119, -78, -111, -60, 31, 60, 57, 92, -88, -100, -69, -106, 91, 69, 93, 110, 23, 93, 53, -44, -51, 64, -80, 46, 2, -4, 12, -45, 80, -44, -35, -111, -28, -66, -116, 39, 2, -27, -45, -52, 125, 39, 66, -90, 63, -105, -67, 84, -57, -4, -4, 101, -90, 81, 10, -33, 1, 67, -57, -71, 18, -74, 102, 96, -89, 64, -17, 54, -94, -84, -66, 14, 119, 121, 2, -78, -79, 89, 63, 93, 109, -78, -51, 66, -36, 32, 86, 3, -58, -15, 92, 58, 2, -89, -80, -13, -1, 122, -4, 48, 63, -44, 59, 100, -42, -45, 59, -7, -17, -54, 34, -54, 71, -64, -26, -87, -80, -17, -44, -38, -112, 70, 10, -106, 95, -24, -4, -118, 45, -85, -13, 85, 25, -102, -119, 13, -37, 116, 46, -69, 59, 42, -90, -38, -105, 101, -119, -36, 97, -3, -62, -91, -97, -125, 17, 14, 106, -72, -119, 99, 111, 20, 18, -27, 113, 64, -24, 74, -60, -100, 26, 56, -44, -70, 12, -51, -100, -32, -11, 26, 48, -117, 98, -93, 51, -25, -79, -31, 97, 87, -105, -64, 7, -13, -101, 33, -122, 5, -104, 89, -44, -117, 63, -80, -6, -71, -110, -29, -105, 116, 107, -93, 91, -41, -13, 20, -115, -78, 43, 79, -122, 6, 102, -32, 52, -118, -51, 72, -104, 41, -38, 124, 72, -126, -35};
int shifts[] = {1, 1, 0, 2, 2, 4, 5, 0, 4, 7, 1, 6, 5, 3, 3, 1, 2, 5, 0, 6, 2, 2, 4, 2, 2, 3, 0, 2, 1, 2, 4, 3, 4, 0, 0, 0, 3, 5, 3, 1, 6, 5, 6, 1, 1, 1, 0, 0, 3, 2, 7, 7, 5, 6, 7, 3, 5, 1, 0, 7, 6, 3, 6, 5, 4, 5, 3, 5, 1, 3, 3, 1, 5, 4, 1, 0, 0, 2, 6, 6, 6, 6, 4, 0, 1, 1, 0, 5, 5, 4, 2, 4, 6, 1, 7, 1, 2, 1, 1, 6, 5, 4, 7, 6, 5, 1, 6, 7, 0, 2, 6, 3, 1, 7, 1, 1, 7, 4, 0, 4, 2, 5, 3, 1, 1, 5, 6, 0, 3, 5, 3, 6, 5, 7, 2, 5, 6, 6, 2, 2, 3, 6, 0, 4, 3, 2, 0, 2, 2, 3, 5, 3, 3, 2, 5, 5, 5, 1, 3, 1, 1, 1, 4, 5, 1, 6, 2, 4, 7, 1, 4, 6, 0, 6, 4, 3, 2, 6, 1, 6, 3, 2, 1, 6, 7, 3, 2, 1, 1, 5, 6, 7, 2, 2, 2, 7, 4, 6, 7, 5, 3, 1, 4, 2, 7, 1, 6, 2, 4, 1, 5, 6, 5, 4, 5, 0, 1, 1, 6, 3, 7, 2, 0, 2, 5, 0, 1, 3, 3, 2, 6, 7, 7, 2, 5, 6, 0, 4, 1, 2, 5, 3, 7, 6, 5, 2, 5, 2, 0, 1, 3, 1, 4, 3, 4, 2};

size_t num_salts = sizeof(salts) / sizeof(salts[0]);
size_t num_shifts = sizeof(shifts) / sizeof(shifts[0]);

// --- 通用工具函数 ---

char* read_file_to_memory(const char* path, size_t* out_size) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *out_size = ftell(f);
    rewind(f);
    char* data = malloc(*out_size + 1);
    fread(data, 1, *out_size, f);
    fclose(f);
    data[*out_size] = '\0';
    return data;
}

// --- GZIP 处理 ---

int decompress_gzip(const unsigned char* src, size_t src_len, const char* out_path) {
    z_stream strm = {0};
    unsigned char out[MAX_BUFFER_SIZE];
    if (inflateInit2(&strm, 31) != Z_OK) return -1;
    strm.avail_in = src_len;
    strm.next_in = (Bytef*)src;
    FILE* f = fopen(out_path, "wb");
    if (!f) { inflateEnd(&strm); return -1; }
    int ret;
    do {
        strm.avail_out = MAX_BUFFER_SIZE;
        strm.next_out = out;
        ret = inflate(&strm, Z_NO_FLUSH);
        fwrite(out, 1, MAX_BUFFER_SIZE - strm.avail_out, f);
    } while (strm.avail_out == 0);
    inflateEnd(&strm); fclose(f);
    return (ret == Z_STREAM_END) ? 0 : -1;
}

int compress_gzip(const unsigned char* src, size_t src_len, unsigned char** out, size_t* out_len) {
    z_stream strm = {0};
    if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK) return -1;
    size_t bound = deflateBound(&strm, src_len) + 100;
    *out = malloc(bound);
    strm.next_in = (Bytef*)src; strm.avail_in = src_len;
    strm.next_out = *out; strm.avail_out = bound;
    deflate(&strm, Z_FINISH);
    *out_len = bound - strm.avail_out;
    deflateEnd(&strm); return 0;
}

// --- XML 处理与排序 (CSC_SORT) ---

typedef struct { char *key; char *value; } Feature;

int compare_features(const void *a, const void *b) {
    return strcmp(((Feature*)a)->key, ((Feature*)b)->key);
}

void patch_xml(const char* xml_path, const char* config_json, const char* dest_path) {
    size_t xml_size, json_size;
    char* xml_content = read_file_to_memory(xml_path, &xml_size);
    char* json_content = read_file_to_memory(config_json, &json_size);
    if (!xml_content || !json_content) return;

    cJSON* configs = cJSON_Parse(json_content);
    if (!configs) { free(xml_content); free(json_content); return; }

    const char* tag_names[] = {"FeatureSet", "SecFloatingFeatureSet"};
    const char* active_tag = NULL;
    char *s_ptr = NULL, *e_ptr = NULL;

    for (int i = 0; i < 2; i++) {
        char s_tag[128], e_tag[128];
        sprintf(s_tag, "<%s>", tag_names[i]);
        sprintf(e_tag, "</%s>", tag_names[i]);
        s_ptr = strstr(xml_content, s_tag);
        if (s_ptr) {
            s_ptr += strlen(s_tag);
            e_ptr = strstr(s_ptr, e_tag);
            active_tag = tag_names[i];
            break;
        }
    }

    if (!active_tag) { printf("Error: No FeatureSet found\n"); return; }

    Feature* features = malloc(sizeof(Feature) * MAX_FEATURES);
    int count = 0;

    char* p = s_ptr;
    while (p < e_ptr) {
        p = strstr(p, "<");
        if (!p || p >= e_ptr || p[1] == '/') break;
        char* t_end = strchr(p, '>');
        if (!t_end) break;
        int k_len = t_end - (p + 1);
        char* key = malloc(k_len + 1);
        strncpy(key, p + 1, k_len); key[k_len] = '\0';

        char e_tag[256]; sprintf(e_tag, "</%s>", key);
        char* v_start = t_end + 1;
        char* v_end = strstr(v_start, e_tag);
        if (v_end && v_end < e_ptr) {
            int v_len = v_end - v_start;
            char* val = malloc(v_len + 1);
            strncpy(val, v_start, v_len); val[v_len] = '\0';
            features[count].key = key;
            features[count].value = val;
            count++;
            p = v_end + strlen(e_tag);
        } else { free(key); p = t_end + 1; }
    }

    cJSON* item;
    cJSON_ArrayForEach(item, configs) {
        const char* k = cJSON_GetObjectItem(item, "key")->valuestring;
        const char* v = cJSON_GetObjectItem(item, "value") ? cJSON_GetObjectItem(item, "value")->valuestring : "";
        const char* cmd = cJSON_GetObjectItem(item, "command")->valuestring;
        if (!cJSON_IsTrue(cJSON_GetObjectItem(item, "enabled"))) continue;

        int found = -1;
        for (int i = 0; i < count; i++) { if (strcmp(features[i].key, k) == 0) { found = i; break; } }

        if (strcmp(cmd, "MODIFY") == 0) {
            if (found >= 0) { free(features[found].value); features[found].value = strdup(v); }
            else { features[count].key = strdup(k); features[count].value = strdup(v); count++; }
        } else if (found >= 0) {
            free(features[found].key); features[found].key = strdup("ZZZ_DELETED");
        }
    }

    qsort(features, count, sizeof(Feature), compare_features);

    FILE* out = fopen(dest_path, "wb");
    fwrite(xml_content, 1, s_ptr - xml_content, out);
    fprintf(out, "\n");
    for (int i = 0; i < count; i++) {
        if (strcmp(features[i].key, "ZZZ_DELETED") != 0) {
            fprintf(out, "    <%s>%s</%s>\n", features[i].key, features[i].value, features[i].key);
        }
    }
    fprintf(out, "  "); // 对齐缩进
    fwrite(e_ptr, 1, (xml_content + xml_size) - e_ptr, out);
    fclose(out);
}

// --- JSON 处理与排序 ---

void sort_json(cJSON *obj) {
    int n = cJSON_GetArraySize(obj);
    if (n <= 1) return;
    cJSON **nodes = malloc(sizeof(cJSON*) * n);
    cJSON *c = obj->child;
    for (int i = 0; i < n; i++) { nodes[i] = c; c = c->next; }
    for (int i = 0; i < n - 1; i++) {
        for (int j = 0; j < n - i - 1; j++) {
            if (strcmp(nodes[j]->string, nodes[j+1]->string) > 0) {
                cJSON *t = nodes[j]; nodes[j] = nodes[j+1]; nodes[j+1] = t;
            }
        }
    }
    obj->child = nodes[0]; nodes[0]->prev = NULL;
    for (int i = 0; i < n - 1; i++) { nodes[i]->next = nodes[i+1]; nodes[i+1]->prev = nodes[i]; }
    nodes[n-1]->next = NULL; free(nodes);
}

void patch_json(const char* src, const char* cfg, const char* dest) {
    size_t s_sz, c_sz;
    char *s_d = read_file_to_memory(src, &s_sz), *c_d = read_file_to_memory(cfg, &c_sz);
    if(!s_d || !c_d) return;
    cJSON *s_r = cJSON_Parse(s_d), *c_r = cJSON_Parse(c_d);
    if(!s_r || !c_r) return;
    cJSON *customer = cJSON_GetObjectItem(s_r, "customer");
    cJSON *feat = cJSON_GetObjectItem(cJSON_GetArrayItem(customer, 0), "feature");
    cJSON *it;
    cJSON_ArrayForEach(it, c_r) {
        if (!cJSON_IsTrue(cJSON_GetObjectItem(it, "enabled"))) continue;
        const char *k = cJSON_GetObjectItem(it, "key")->valuestring;
        const char *v = cJSON_GetObjectItem(it, "value") ? cJSON_GetObjectItem(it, "value")->valuestring : "";
        if (strcmp(cJSON_GetObjectItem(it, "command")->valuestring, "MODIFY") == 0) {
            if (cJSON_HasObjectItem(feat, k)) cJSON_ReplaceItemInObject(feat, k, cJSON_CreateString(v));
            else cJSON_AddItemToObject(feat, k, cJSON_CreateString(v));
        } else cJSON_DeleteItemFromObject(feat, k);
    }
    sort_json(feat);
    char *out = cJSON_Print(s_r);
    FILE *f = fopen(dest, "w"); fputs(out, f); fclose(f);
}

// --- Main ---

int main(int argc, char **argv) {
    if (argc < 4) return 1;
    if (strcmp(argv[1], "--decode") == 0) {
        size_t sz; unsigned char *d = (unsigned char*)read_file_to_memory(argv[2], &sz);
        if(!d) return 1;
        unsigned char *dec = malloc(sz);
        for (size_t i = 0; i < sz; i++) {
            unsigned int b = d[i]; int s = shifts[i % num_shifts];
            dec[i] = (((b << s) | (b >> (8 - s))) & 0xFF) ^ salts[i % num_salts];
        }
        decompress_gzip(dec, sz, argv[3]);
        free(d); free(dec);
    } else if (strcmp(argv[1], "--encode") == 0) {
        size_t sz; unsigned char *d = (unsigned char*)read_file_to_memory(argv[2], &sz);
        if(!d) return 1;
        unsigned char *comp; size_t cs; compress_gzip(d, sz, &comp, &cs);
        unsigned char *enc = malloc(cs);
        for (size_t i = 0; i < cs; i++) {
            unsigned int x = (comp[i] ^ salts[i % num_salts]) & 0xFF;
            int s = shifts[i % num_shifts];
            enc[i] = ((x >> s) | (x << (8 - s))) & 0xFF;
        }
        FILE *f = fopen(argv[3], "wb"); fwrite(enc, 1, cs, f); fclose(f);
        free(d); free(comp); free(enc);
    } else if (strcmp(argv[1], "--patch") == 0 && argc == 5) {
        if (strstr(argv[2], ".json")) patch_json(argv[2], argv[3], argv[4]);
        else patch_xml(argv[2], argv[3], argv[4]);
    }
    return 0;
}

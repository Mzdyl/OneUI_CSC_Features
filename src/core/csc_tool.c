#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <zlib.h>
#include "lib/cJSON.h"

#define MAX_BUFFER_SIZE 4096
#define INITIAL_COMPRESSION_BUFFER_SIZE (1024 * 128)

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

// --- 加密/解密/压缩/解压 逻辑 ---

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
    inflateEnd(&strm);
    fclose(f);
    return (ret == Z_STREAM_END) ? 0 : -1;
}

void do_decode(const char* in, const char* out) {
    size_t size;
    unsigned char* data = (unsigned char*)read_file_to_memory(in, &size);
    if (!data) return;
    unsigned char* decrypted = malloc(size);
    for (size_t i = 0; i < size; i++) {
        unsigned int b = data[i];
        int s = shifts[i % num_shifts];
        unsigned int shifted = ((b << s) | (b >> (8 - s))) & 0xFF;
        decrypted[i] = (shifted ^ salts[i % num_salts]) & 0xFF;
    }
    if (decompress_gzip(decrypted, size, out) == 0) printf("Decode success: %s\n", out);
    free(data); free(decrypted);
}

// --- XML 补丁与排序逻辑 (CSC_SORT) ---

typedef struct {
    char *key;
    char *value;
} Feature;

int compare_features(const void *a, const void *b) {
    return strcmp(((Feature*)a)->key, ((Feature*)b)->key);
}

void patch_xml(const char* xml_path, const char* config_json, const char* dest_path) {
    size_t xml_size, json_size;
    char* xml_content = read_file_to_memory(xml_path, &xml_size);
    char* json_content = read_file_to_memory(config_json, &json_size);
    if (!xml_content || !json_content) return;

    cJSON* configs = cJSON_Parse(json_content);
    if (!configs) return;

    // 检测根标签
    const char* root_tag = strstr(xml_content, "<SecFloatingFeatureSet>") ? "SecFloatingFeatureSet" : "FeatureSet";
    
    // 解析现有 XML Features (简单字符串解析实现，避免引入复杂 XML 库)
    Feature* features = malloc(sizeof(Feature) * 2000);
    int count = 0;

    char* search_ptr = xml_content;
    while ((search_ptr = strstr(search_ptr, "<")) != NULL) {
        if (search_ptr[1] == '/' || search_ptr[1] == '?' || search_ptr[1] == '!') { search_ptr++; continue; }
        char* end_tag = strstr(search_ptr, ">");
        if (!end_tag) break;
        
        int key_len = end_tag - (search_ptr + 1);
        char* key = malloc(key_len + 1);
        strncpy(key, search_ptr + 1, key_len); key[key_len] = '\0';
        
        // 只处理 CscFeature_ 或 SEC_FLOATING_ 
        if (strstr(key, "CscFeature_") || strstr(key, "SEC_FLOATING_")) {
            char* close_tag_pattern = malloc(key_len + 4);
            sprintf(close_tag_pattern, "</%s>", key);
            char* val_start = end_tag + 1;
            char* val_end = strstr(val_start, close_tag_pattern);
            if (val_end) {
                int val_len = val_end - val_start;
                char* val = malloc(val_len + 1);
                strncpy(val, val_start, val_len); val[val_len] = '\0';
                features[count].key = key;
                features[count].value = val;
                count++;
            }
            free(close_tag_pattern);
        } else {
            free(key);
        }
        search_ptr = end_tag + 1;
    }

    // 应用 JSON 补丁
    cJSON* entry;
    cJSON_ArrayForEach(entry, configs) {
        const char* cmd = cJSON_GetObjectItem(entry, "command")->valuestring;
        const char* key = cJSON_GetObjectItem(entry, "key")->valuestring;
        const char* val = cJSON_GetObjectItem(entry, "value") ? cJSON_GetObjectItem(entry, "value")->valuestring : "";
        int enabled = cJSON_IsTrue(cJSON_GetObjectItem(entry, "enabled"));
        if (!enabled) continue;

        int found = -1;
        for (int i = 0; i < count; i++) {
            if (strcmp(features[i].key, key) == 0) { found = i; break; }
        }

        if (strcmp(cmd, "MODIFY") == 0) {
            if (found >= 0) {
                free(features[found].value);
                features[found].value = strdup(val);
            } else {
                features[count].key = strdup(key);
                features[count].value = strdup(val);
                count++;
            }
        } else if (strcmp(cmd, "DELETE") == 0 && found >= 0) {
            // 简单标记删除
            free(features[found].key); features[found].key = strdup("ZZZ_DELETED");
        }
    }

    // CSC_SORT
    qsort(features, count, sizeof(Feature), compare_features);

    // 写回文件
    FILE* out = fopen(dest_path, "w");
    fprintf(out, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
    // 保留原始非 Feature 部分 (Country, SalesCode 等)
    if (strstr(xml_content, "<SamsungMobileFeature>")) {
        fprintf(out, "<SamsungMobileFeature>\n");
        // 这里应有更精细的逻辑保留 Country 等，此处简化处理直接输出 FeatureSet
    }
    fprintf(out, "  <%s>\n", root_tag);
    for (int i = 0; i < count; i++) {
        if (strcmp(features[i].key, "ZZZ_DELETED") != 0) {
            fprintf(out, "    <%s>%s</%s>\n", features[i].key, features[i].value, features[i].key);
        }
    }
    fprintf(out, "  </%s>\n", root_tag);
    if (strstr(xml_content, "</SamsungMobileFeature>")) fprintf(out, "</SamsungMobileFeature>\n");
    fclose(out);

    printf("Patch applied and sorted to: %s\n", dest_path);
    // 内存释放省略...
}

// --- 主程序入口 ---

// --- JSON 排序辅助 ---
void sort_cjson_object(cJSON *object) {
    if (!object || !object->child) return;
    int count = cJSON_GetArraySize(object);
    if (count <= 1) return;
    
    cJSON **items = malloc(sizeof(cJSON*) * count);
    cJSON *curr = object->child;
    for (int i = 0; i < count; i++) {
        items[i] = curr;
        curr = curr->next;
    }
    
    // 简单的冒泡排序排序 cJSON 链表
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (strcmp(items[j]->string, items[j+1]->string) > 0) {
                cJSON *tmp = items[j];
                items[j] = items[j+1];
                items[j+1] = tmp;
            }
        }
    }
    
    // 重新构建链表
    object->child = items[0];
    items[0]->prev = NULL;
    for (int i = 0; i < count - 1; i++) {
        items[i]->next = items[i+1];
        items[i+1]->prev = items[i];
    }
    items[count-1]->next = NULL;
    free(items);
}

void patch_json(const char* src_path, const char* config_json, const char* dest_path) {
    size_t src_size, cfg_size;
    char* src_data = read_file_to_memory(src_path, &src_size);
    char* cfg_data = read_file_to_memory(config_json, &cfg_size);
    if (!src_data || !cfg_data) return;

    cJSON *src_root = cJSON_Parse(src_data);
    cJSON *cfg_root = cJSON_Parse(cfg_data);
    
    // 定位到 Carrier 的 feature 对象: customer[0].feature
    cJSON *customer = cJSON_GetObjectItem(src_root, "customer");
    cJSON *first_cust = cJSON_GetArrayItem(customer, 0);
    cJSON *feature_obj = cJSON_GetObjectItem(first_cust, "feature");

    cJSON *entry;
    cJSON_ArrayForEach(entry, cfg_root) {
        const char *key = cJSON_GetObjectItem(entry, "key")->valuestring;
        const char *val = cJSON_GetObjectItem(entry, "value") ? cJSON_GetObjectItem(entry, "value")->valuestring : "";
        const char *cmd = cJSON_GetObjectItem(entry, "command")->valuestring;
        if (!cJSON_IsTrue(cJSON_GetObjectItem(entry, "enabled"))) continue;

        if (strcmp(cmd, "MODIFY") == 0) {
            if (cJSON_HasObjectItem(feature_obj, key)) {
                cJSON_ReplaceItemInObject(feature_obj, key, cJSON_CreateString(val));
            } else {
                cJSON_AddItemToObject(feature_obj, key, cJSON_CreateString(val));
            }
        } else if (strcmp(cmd, "DELETE") == 0) {
            cJSON_DeleteItemFromObject(feature_obj, key);
        }
    }

    // 排序
    sort_cjson_object(feature_obj);

    char *out_str = cJSON_Print(src_root);
    FILE *f = fopen(dest_path, "w");
    fputs(out_str, f);
    fclose(f);
    printf("JSON Patch success: %s\n", dest_path);
}

int compress_gzip(const unsigned char* src, size_t src_len, unsigned char** out, size_t* out_len) {
    z_stream strm = {0};
    if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK) return -1;
    size_t bound = deflateBound(&strm, src_len) + 100;
    *out = malloc(bound);
    strm.next_in = (Bytef*)src;
    strm.avail_in = src_len;
    strm.next_out = *out;
    strm.avail_out = bound;
    deflate(&strm, Z_FINISH);
    *out_len = bound - strm.avail_out;
    deflateEnd(&strm);
    return 0;
}

void do_encode(const char* in, const char* out) {
    size_t size;
    unsigned char* data = (unsigned char*)read_file_to_memory(in, &size);
    if (!data) return;
    
    unsigned char* compressed;
    size_t comp_size;
    if (compress_gzip(data, size, &compressed, &comp_size) != 0) { free(data); return; }
    
    unsigned char* encrypted = malloc(comp_size);
    for (size_t i = 0; i < comp_size; i++) {
        unsigned int b = compressed[i];
        unsigned int xored = (b ^ salts[i % num_salts]) & 0xFF;
        int s = shifts[i % num_shifts];
        unsigned int shifted = ((xored >> s) | (xored << (8 - s))) & 0xFF;
        encrypted[i] = (unsigned char)shifted;
    }
    
    FILE* f = fopen(out, "wb");
    if (f) { fwrite(encrypted, 1, comp_size, f); fclose(f); }
    printf("Encode success: %s\n", out);
    free(data); free(compressed); free(encrypted);
}

int main(int argc, char** argv) {
    if (argc < 2) return 1;
    if (strcmp(argv[1], "--decode") == 0) do_decode(argv[2], argv[3]);
    else if (strcmp(argv[1], "--encode") == 0) do_encode(argv[2], argv[3]);
    else if (strcmp(argv[1], "--patch") == 0) {
        if (strstr(argv[2], ".json")) patch_json(argv[2], argv[3], argv[4]);
        else patch_xml(argv[2], argv[3], argv[4]);
    }
    return 0;
}

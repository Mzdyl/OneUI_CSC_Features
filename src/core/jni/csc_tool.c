#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <zlib.h>
#include "lib/cJSON.h"

#define MAX_BUFFER_SIZE 4096
#define MAX_FEATURES 10000

// --- 编解码核心数据 ---
const int salts[] = {65, -59, 33, -34, 107, 28, -107, 55, 78, 17, -81, 6, -80, -121, -35, -23, 72, 122, -63, -43, 68, 119, -78, -111, -60, 31, 60, 57, 92, -88, -100, -69, -106, 91, 69, 93, 110, 23, 93, 53, -44, -51, 64, -80, 46, 2, -4, 12, -45, 80, -44, -35, -111, -28, -66, -116, 39, 2, -27, -45, -52, 125, 39, 66, -90, 63, -105, -67, 84, -57, -4, -4, 101, -90, 81, 10, -33, 1, 67, -57, -71, 18, -74, 102, 96, -89, 64, -17, 54, -94, -84, -66, 14, 119, 121, 2, -78, -79, 89, 63, 93, 109, -78, -51, 66, -36, 32, 86, 3, -58, -15, 92, 58, 2, -89, -80, -13, -1, 122, -4, 48, 63, -44, 59, 100, -42, -45, 59, -7, -17, -54, 34, -54, 71, -64, -26, -87, -80, -17, -44, -38, -112, 70, 10, -106, 95, -24, -4, -118, 45, -85, -13, 85, 25, -102, -119, 13, -37, 116, 46, -69, 59, 42, -90, -38, -105, 101, -119, -36, 97, -3, -62, -91, -97, -125, 17, 14, 106, -72, -119, 99, 111, 20, 18, -27, 113, 64, -24, 74, -60, -100, 26, 56, -44, -70, 12, -51, -100, -32, -11, 26, 48, -117, 98, -93, 51, -25, -79, -31, 97, 87, -105, -64, 7, -13, -101, 33, -122, 5, -104, 89, -44, -117, 63, -80, -6, -71, -110, -29, -105, 116, 107, -93, 91, -41, -13, 20, -115, -78, 43, 79, -122, 6, 102, -32, 52, -118, -51, 72, -104, 41, -38, 124, 72, -126, -35};
const int shifts[] = {1, 1, 0, 2, 2, 4, 5, 0, 4, 7, 1, 6, 5, 3, 3, 1, 2, 5, 0, 6, 2, 2, 4, 2, 2, 3, 0, 2, 1, 2, 4, 3, 4, 0, 0, 0, 3, 5, 3, 1, 6, 5, 6, 1, 1, 1, 0, 0, 3, 2, 7, 7, 5, 6, 7, 3, 5, 1, 0, 7, 6, 3, 6, 5, 4, 5, 3, 5, 1, 3, 3, 1, 5, 4, 1, 0, 0, 2, 6, 6, 6, 6, 4, 0, 1, 1, 0, 5, 5, 4, 2, 4, 6, 1, 7, 1, 2, 1, 1, 6, 5, 4, 7, 6, 5, 1, 6, 7, 0, 2, 6, 3, 1, 7, 1, 1, 7, 4, 0, 4, 2, 5, 3, 1, 1, 5, 6, 0, 3, 5, 3, 6, 5, 7, 2, 5, 6, 6, 2, 2, 3, 6, 0, 4, 3, 2, 0, 2, 2, 3, 5, 3, 3, 2, 5, 5, 5, 1, 3, 1, 1, 1, 4, 5, 1, 6, 2, 4, 7, 1, 4, 6, 0, 6, 4, 3, 2, 6, 1, 6, 3, 2, 1, 6, 7, 3, 2, 1, 1, 5, 6, 7, 2, 2, 2, 7, 4, 6, 7, 5, 3, 1, 4, 2, 7, 1, 6, 2, 4, 1, 5, 6, 5, 4, 5, 0, 1, 1, 6, 3, 7, 2, 0, 2, 5, 0, 1, 3, 3, 2, 6, 7, 7, 2, 5, 6, 0, 4, 1, 2, 5, 3, 7, 6, 5, 2, 5, 2, 0, 1, 3, 1, 4, 3, 4, 2};
const size_t num_salts = 256;
const size_t num_shifts = 256;

typedef struct { char *key; char *value; } Feature;

char* read_file_to_mem(const char* path, size_t* out_sz) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    *out_sz = ftell(f);
    rewind(f);
    
    if (*out_sz == 0) {
        fclose(f);
        return NULL;
    }

    char* d = malloc(*out_sz + 1);
    if (d) {
        size_t read_sz = fread(d, 1, *out_sz, f);
        if (read_sz != *out_sz) { 
            free(d); d = NULL;
        } else {
            d[*out_sz] = '\0';
        }
    }
    fclose(f);
    return d;
}

int decompress_gz(const unsigned char* src, size_t src_len, const char* out_path) {
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
        if (ret == Z_STREAM_ERROR || ret == Z_DATA_ERROR || ret == Z_MEM_ERROR) {
            inflateEnd(&strm); fclose(f); return -1;
        }
        fwrite(out, 1, MAX_BUFFER_SIZE - strm.avail_out, f);
    } while (strm.avail_out == 0);
    inflateEnd(&strm); 
    fclose(f);
    return (ret == Z_STREAM_END) ? 0 : -1;
}

int compress_gz(const unsigned char* src, size_t src_len, unsigned char** out, size_t* out_len) {
    z_stream strm = {0};
    if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK) return -1;
    size_t bound = deflateBound(&strm, src_len) + 100;
    *out = malloc(bound);
    if (!*out) { deflateEnd(&strm); return -1; }
    
    strm.next_in = (Bytef*)src; strm.avail_in = src_len;
    strm.next_out = *out; strm.avail_out = bound;
    deflate(&strm, Z_FINISH);
    *out_len = bound - strm.avail_out;
    deflateEnd(&strm); 
    return 0;
}

void sort_json_obj(cJSON *obj) {
    if (!obj || !obj->child) return;
    int count = cJSON_GetArraySize(obj);
    if (count <= 1) return;
    
    cJSON **nodes = malloc(sizeof(cJSON*) * count);
    if (!nodes) return;

    cJSON *curr = obj->child;
    for (int i = 0; i < count; i++) { nodes[i] = curr; curr = curr->next; }
    
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (nodes[j]->string && nodes[j+1]->string && strcmp(nodes[j]->string, nodes[j+1]->string) > 0) {
                cJSON *tmp = nodes[j]; nodes[j] = nodes[j+1]; nodes[j+1] = tmp;
            }
        }
    }
    
    obj->child = nodes[0]; nodes[0]->prev = NULL;
    for (int i = 0; i < count - 1; i++) { 
        nodes[i]->next = nodes[i+1]; 
        nodes[i+1]->prev = nodes[i]; 
    }
    nodes[count-1]->next = NULL; 
    free(nodes);
}

int cmp_feat(const void *a, const void *b) { return strcmp(((Feature*)a)->key, ((Feature*)b)->key); }

void patch_xml(const char* xml_p, const char* cfg_p, const char* dst) {
    size_t xs, js;
    char *xc = read_file_to_mem(xml_p, &xs);
    char *jc = read_file_to_mem(cfg_p, &js);
    if (!xc || !jc) { free(xc); free(jc); return; }

    cJSON *cfg = cJSON_Parse(jc);
    if (!cfg) { free(xc); free(jc); return; }

    char *s = strstr(xc, "<FeatureSet>");
    char *e = NULL, *tag = "FeatureSet";
    if (!s) { 
        s = strstr(xc, "<SecFloatingFeatureSet>"); 
        tag = "SecFloatingFeatureSet"; 
    }
    
    if (s) { 
        s += strlen(tag) + 2; 
        // 【关键修复点】：精确寻找父节点的闭合标签，而不是随意寻找第一个 `</`
        char end_tag[64];
        snprintf(end_tag, sizeof(end_tag), "</%s>", tag);
        e = strstr(s, end_tag); 
    }
    
    if (!s || !e) { cJSON_Delete(cfg); free(xc); free(jc); return; }

    Feature *list = malloc(sizeof(Feature) * MAX_FEATURES); 
    if (!list) { cJSON_Delete(cfg); free(xc); free(jc); return; }
    
    int n = 0;
    char *p = s;
    while (p < e && n < MAX_FEATURES) {
        p = strstr(p, "<"); 
        
        // 【容错修复】：如果遇到的是注释、声明或者闭合标签，不要直接 break，而是跳过它
        if (!p || p >= e) break;
        if (p[1] == '/' || p[1] == '!' || p[1] == '?') { 
            p++; continue; 
        }

        char *te = strchr(p, '>'); if (!te || te >= e) break;
        
        int kl = te - p - 1; 
        if (kl <= 0 || kl > 200) { p = te + 1; continue; }

        char *k = malloc(kl + 1); strncpy(k, p + 1, kl); k[kl] = 0;
        
        char st[256], et[256]; 
        snprintf(st, sizeof(st), "<%s>", k); 
        snprintf(et, sizeof(et), "</%s>", k);
        
        char *vs = strstr(p, st); 
        if (vs) {
            vs += strlen(st); char *ve = strstr(vs, et);
            // 确保找到的闭合标签在我们限制的区块以内
            if (ve && ve < e) {
                int vl = ve - vs; char *v = malloc(vl + 1); strncpy(v, vs, vl); v[vl] = 0;
                list[n].key = k; list[n].value = v; n++; 
                p = ve + strlen(et); 
                continue;
            }
        }
        free(k); p = te + 1;
    }

    cJSON *item;
    cJSON_ArrayForEach(item, cfg) {
        cJSON *enabled_item = cJSON_GetObjectItem(item, "enabled");
        if (!cJSON_IsTrue(enabled_item)) continue;

        cJSON *key_item = cJSON_GetObjectItem(item, "key");
        cJSON *cmd_item = cJSON_GetObjectItem(item, "command");
        if (!cJSON_IsString(key_item) || !cJSON_IsString(cmd_item)) continue;
        
        const char *k = key_item->valuestring;
        cJSON *val_item = cJSON_GetObjectItem(item, "value");
        const char *v = (val_item && cJSON_IsString(val_item)) ? val_item->valuestring : "";
        
        int i, found = -1; 
        for (i = 0; i < n; i++) if (strcmp(list[i].key, k) == 0) { found = i; break; }

        if (strcmp(cmd_item->valuestring, "MODIFY") == 0) {
            if (found >= 0) { 
                free(list[found].value); 
                list[found].value = strdup(v); 
            } else if (n < MAX_FEATURES) { 
                list[n].key = strdup(k); list[n].value = strdup(v); n++; 
            }
        } else if (found >= 0) { 
            free(list[found].key); list[found].key = strdup("ZZZ_DEL"); 
        }
    }
    
    qsort(list, n, sizeof(Feature), cmp_feat);
    
    FILE *f = fopen(dst, "wb"); 
    if (f) {
        fwrite(xc, 1, s - xc, f); fprintf(f, "\n");
        for (int i = 0; i < n; i++) {
            if (strcmp(list[i].key, "ZZZ_DEL") != 0) {
                fprintf(f, "    <%s>%s</%s>\n", list[i].key, list[i].value, list[i].key);
            }
        }
        fprintf(f, "  "); fputs(e, f); 
        fclose(f);
    }

    for (int i = 0; i < n; i++) {
        free(list[i].key);
        free(list[i].value);
    }
    free(list);
    cJSON_Delete(cfg);
    free(xc);
    free(jc);
}

void patch_json(const char* src, const char* cfg_p, const char* dst) {
    size_t ss, cs;
    char *sd = read_file_to_mem(src, &ss);
    char *cd = read_file_to_mem(cfg_p, &cs);
    if (!sd || !cd) { free(sd); free(cd); return; }

    cJSON *sr = cJSON_Parse(sd);
    cJSON *cr = cJSON_Parse(cd);
    if (!sr || !cr) { cJSON_Delete(sr); cJSON_Delete(cr); free(sd); free(cd); return; }

    cJSON *customer_arr = cJSON_GetObjectItem(sr, "customer");
    cJSON *customer_item = cJSON_GetArrayItem(customer_arr, 0);
    cJSON *feat = cJSON_GetObjectItem(customer_item, "feature");

    if (feat) {
        cJSON *it;
        cJSON_ArrayForEach(it, cr) {
            cJSON *enabled_item = cJSON_GetObjectItem(it, "enabled");
            if (!cJSON_IsTrue(enabled_item)) continue;

            cJSON *key_item = cJSON_GetObjectItem(it, "key");
            cJSON *cmd_item = cJSON_GetObjectItem(it, "command");
            if (!cJSON_IsString(key_item) || !cJSON_IsString(cmd_item)) continue;

            const char *k = key_item->valuestring;
            cJSON *val_item = cJSON_GetObjectItem(it, "value");
            const char *v = (val_item && cJSON_IsString(val_item)) ? val_item->valuestring : "";

            if (strcmp(cmd_item->valuestring, "MODIFY") == 0) {
                if (cJSON_HasObjectItem(feat, k)) {
                    cJSON_ReplaceItemInObject(feat, k, cJSON_CreateString(v));
                } else {
                    cJSON_AddItemToObject(feat, k, cJSON_CreateString(v));
                }
            } else {
                cJSON_DeleteItemFromObject(feat, k);
            }
        }
        sort_json_obj(feat); 
    }

    char *os = cJSON_Print(sr); 
    if (os) {
        FILE *f = fopen(dst, "w"); 
        if (f) { fputs(os, f); fclose(f); }
        cJSON_free(os); 
    }

    cJSON_Delete(sr);
    cJSON_Delete(cr);
    free(sd);
    free(cd);
}

int main(int argc, char **argv) {
    if (argc < 4) {
        printf("Usage:\n");
        printf("  %s --decode <input> <output>\n", argv[0]);
        printf("  %s --encode <input> <output>\n", argv[0]);
        printf("  %s --patch <xml/json> <config_json> <output>\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "--decode") == 0) {
        size_t s; unsigned char *d = (unsigned char*)read_file_to_mem(argv[2], &s);
        if (!d) { printf("Failed to read %s\n", argv[2]); return 1; }
        
        unsigned char *res = malloc(s);
        if (!res) { free(d); return 1; }

        for (size_t i = 0; i < s; i++) {
            unsigned int b = d[i]; 
            int sh = shifts[i % num_shifts];
            res[i] = (((b << sh) | (b >> (8 - sh))) & 0xFF) ^ (unsigned char)salts[i % num_salts];
        }
        
        if (decompress_gz(res, s, argv[3]) != 0) {
            printf("Decompression failed!\n");
        }
        free(d); free(res);

    } else if (strcmp(argv[1], "--encode") == 0) {
        size_t s; unsigned char *d = (unsigned char*)read_file_to_mem(argv[2], &s);
        if (!d) { printf("Failed to read %s\n", argv[2]); return 1; }
        
        unsigned char *comp = NULL; size_t cs; 
        if (compress_gz(d, s, &comp, &cs) != 0) {
            printf("Compression failed!\n");
            free(d); return 1;
        }

        unsigned char *res = malloc(cs);
        if (!res) { free(d); free(comp); return 1; }

        for (size_t i = 0; i < cs; i++) {
            unsigned int x = (comp[i] ^ (unsigned char)salts[i % num_salts]) & 0xFF;
            int sh = shifts[i % num_shifts];
            res[i] = ((x >> sh) | (x << (8 - sh))) & 0xFF;
        }
        
        FILE *f = fopen(argv[3], "wb"); 
        if (f) { fwrite(res, 1, cs, f); fclose(f); }
        else { printf("Failed to write %s\n", argv[3]); }
        
        free(d); free(comp); free(res);

    } else if (strcmp(argv[1], "--patch") == 0 && argc == 5) {
        if (strstr(argv[2], ".json")) {
            patch_json(argv[2], argv[3], argv[4]);
        } else {
            patch_xml(argv[2], argv[3], argv[4]);
        }
    } else {
        printf("Invalid arguments.\n");
        return 1;
    }

    return 0;
}
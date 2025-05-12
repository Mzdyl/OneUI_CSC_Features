#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// 写入调试日志的函数
void log_debug(const char *message) {
    printf("[调试] %s\n", message);
}

// 去除字符串两端的空格和换行符
void trim(char *str) {
    if (str == NULL) return;
    
    char *end;
    // 去掉前面的空格
    while (isspace((unsigned char)*str)) str++;
    if (*str == '\0') return;  // 如果字符串只包含空格，则在清除后字符串为空
    
    // 去掉末尾的空格和换行符
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    *(end + 1) = '\0';
}

// 安全地复制字符串
char* safe_strdup(const char* str) {
    if (!str) return NULL;
    char* result = strdup(str);
    if (!result) {
        log_debug("内存分配失败");
        exit(EXIT_FAILURE);
    }
    return result;
}

// 读取整个文件内容到内存
char* read_file_contents(const char* filename, long* file_size) {
    FILE* file = fopen(filename, "r");
    if (!file) {
        perror("打开文件失败");
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    *file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    char* buffer = (char*)malloc(*file_size + 1);
    if (!buffer) {
        perror("内存分配失败");
        fclose(file);
        return NULL;
    }
    
    size_t read_size = fread(buffer, 1, *file_size, file);
    fclose(file);
    
    if (read_size != *file_size) {
        perror("读取文件失败");
        free(buffer);
        return NULL;
    }
    
    buffer[*file_size] = '\0';
    return buffer;
}

// 写入内容到文件
int write_file_contents(const char* filename, const char* content) {
    FILE* file = fopen(filename, "w");
    if (!file) {
        perror("创建文件失败");
        return 0;
    }
    
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, file);
    fclose(file);
    
    return (written == len);
}

// 辅助函数：查找JSON中的键
char* find_json_key(const char* json, const char* key, int* start_pos, int* end_pos) {
    char key_pattern[512];
    snprintf(key_pattern, sizeof(key_pattern), "\"%s\"", key);
    
    char* key_pos = strstr(json, key_pattern);
    if (!key_pos) return NULL;
    
    // 找到键后，查找值的开始位置（冒号后面）
    char* value_start = strchr(key_pos + strlen(key_pattern), ':');
    if (!value_start) return NULL;
    
    // 跳过冒号和空白字符
    value_start++;
    while (isspace((unsigned char)*value_start)) value_start++;
    
    // 判断值的类型
    if (*value_start != '"') {
        // 不是字符串值，查找到逗号或花括号结束
        char* value_end = value_start;
        while (*value_end && *value_end != ',' && *value_end != '}' && *value_end != ']') value_end++;
        
        int value_len = value_end - value_start;
        char* value = (char*)malloc(value_len + 1);
        if (!value) return NULL;
        
        strncpy(value, value_start, value_len);
        value[value_len] = '\0';
        trim(value);
        
        if (start_pos) *start_pos = value_start - json;
        if (end_pos) *end_pos = value_end - json;
        
        return value;
    } else {
        // 字符串值，需要处理转义引号
        value_start++; // 跳过开始的引号
        char* value_end = value_start;
        int escaped = 0;
        
        while (*value_end && (escaped || *value_end != '"')) {
            escaped = (!escaped && *value_end == '\\');
            value_end++;
        }
        
        if (*value_end != '"') return NULL; // 没有找到闭合的引号
        
        int value_len = value_end - value_start;
        char* value = (char*)malloc(value_len + 1);
        if (!value) return NULL;
        
        strncpy(value, value_start, value_len);
        value[value_len] = '\0';
        
        if (start_pos) *start_pos = value_start - 1 - json; // 包括开始的引号
        if (end_pos) *end_pos = value_end + 1 - json;      // 包括结束的引号
        
        return value;
    }
}

// 辅助函数：在JSON对象中查找字段位置
int find_feature_object(const char* json, const char** feature_start, const char** feature_end) {
    // 查找customer数组
    const char* customer_pos = strstr(json, "\"customer\"");
    if (!customer_pos) {
        log_debug("配置文件中未找到customer部分");
        return 0;
    }
    
    // 查找第一个左方括号
    const char* array_start = strchr(customer_pos, '[');
    if (!array_start) {
        log_debug("未找到customer数组开始位置");
        return 0;
    }
    
    // 查找第一个包含"feature"的对象
    const char* feature_pos = strstr(array_start, "\"feature\"");
    if (!feature_pos) {
        log_debug("配置文件中未找到feature对象");
        return 0;
    }
    
    // 查找feature字段后面的冒号
    const char* colon_pos = strchr(feature_pos, ':');
    if (!colon_pos) {
        log_debug("feature字段格式错误");
        return 0;
    }
    
    // 查找feature对象的开始花括号
    const char* open_brace = strchr(colon_pos, '{');
    if (!open_brace) {
        log_debug("未找到feature对象的开始位置");
        return 0;
    }
    
    // 查找匹配的结束花括号
    const char* close_brace = open_brace + 1;
    int brace_count = 1;
    
    while (brace_count > 0 && *close_brace) {
        if (*close_brace == '{') brace_count++;
        else if (*close_brace == '}') brace_count--;
        close_brace++;
    }
    
    if (brace_count != 0) {
        log_debug("未找到feature对象的结束位置");
        return 0;
    }
    
    close_brace--; // 回到最后一个右花括号位置
    
    *feature_start = open_brace;
    *feature_end = close_brace;
    
    return 1;
}

// 删除配置项
void delete_config(const char *key, const char *config_file) {
    log_debug("开始删除配置项...");
    
    long file_size;
    char* json = read_file_contents(config_file, &file_size);
    if (!json) return;
    
    const char* feature_start;
    const char* feature_end;
    
    if (!find_feature_object(json, &feature_start, &feature_end)) {
        free(json);
        return;
    }
    
    // 构建要查找的键模式
    char key_pattern[512];
    snprintf(key_pattern, sizeof(key_pattern), "\"%s\"", key);
    
    // 在feature对象中查找键
    const char* key_pos = strstr(feature_start, key_pattern);
    if (!key_pos || key_pos > feature_end) {
        log_debug("未找到要删除的配置项");
        free(json);
        return;
    }
    
    // 向前查找，找到键的开始位置（可能是逗号或花括号）
    const char* key_start = key_pos;
    while (key_start > feature_start) {
        key_start--;
        // 找到前导分隔符（逗号或花括号）
        if (*key_start == ',' || *key_start == '{') {
            break;
        }
    }
    
    // 找到键值对的结束位置
    const char* key_colon = strchr(key_pos, ':');
    if (!key_colon || key_colon > feature_end) {
        log_debug("键格式错误，未找到冒号");
        free(json);
        return;
    }
    
    const char* value_start = key_colon + 1;
    // 跳过空白字符
    while (value_start <= feature_end && isspace((unsigned char)*value_start)) {
        value_start++;
    }
    
    const char* value_end;
    if (*value_start == '"') {
        // 字符串值，查找结束引号
        value_end = value_start + 1;
        int escaped = 0;
        while (value_end <= feature_end && (escaped || *value_end != '"')) {
            escaped = (!escaped && *value_end == '\\');
            value_end++;
        }
        if (*value_end == '"') value_end++; // 包括结束引号
    } else {
        // 非字符串值，查找逗号或花括号
        value_end = value_start;
        while (value_end <= feature_end && *value_end != ',' && *value_end != '}') {
            value_end++;
        }
    }
    
    // 查找键值对之后的分隔符
    const char* next_char = value_end;
    while (next_char <= feature_end && isspace((unsigned char)*next_char)) {
        next_char++;
    }
    
    // 确定删除范围
    const char* delete_start;
    const char* delete_end;
    
    if (*key_start == '{') {
        // 第一个键值对
        delete_start = key_start + 1; // 保留左花括号
        if (*next_char == ',') {
            delete_end = next_char + 1; // 包括后面的逗号
        } else {
            delete_end = next_char; // 可能是右花括号或其他字符
        }
    } else if (*key_start == ',') {
        // 中间的键值对，删除前导逗号
        delete_start = key_start;
        delete_end = next_char;
    } else {
        // 异常情况
        log_debug("键位置异常");
        free(json);
        return;
    }
    
    // 计算新的JSON长度
    size_t delete_length = delete_end - delete_start;
    size_t new_json_length = file_size - delete_length;
    char* new_json = (char*)malloc(new_json_length + 1);
    
    if (!new_json) {
        log_debug("内存分配失败");
        free(json);
        return;
    }
    
    // 复制删除部分之前的内容
    size_t prefix_length = delete_start - json;
    strncpy(new_json, json, prefix_length);
    
    // 复制删除部分之后的内容
    strcpy(new_json + prefix_length, delete_end);
    
    // 写入修改后的内容
    if (!write_file_contents(config_file, new_json)) {
        log_debug("写入文件失败");
    } else {
        log_debug("配置项删除成功");
    }
    
    free(new_json);
    free(json);
}

// 修改或添加配置项
void modify_config(const char *key, const char *value, const char *config_file) {
    log_debug("开始修改配置项...");
    
    long file_size;
    char* json = read_file_contents(config_file, &file_size);
    if (!json) return;
    
    const char* feature_start;
    const char* feature_end;
    
    if (!find_feature_object(json, &feature_start, &feature_end)) {
        free(json);
        return;
    }
    
    // 复制feature对象部分进行操作
    size_t feature_length = feature_end - feature_start + 1;
    char* feature_obj = (char*)malloc(feature_length + 1);
    if (!feature_obj) {
        log_debug("内存分配失败");
        free(json);
        return;
    }
    
    strncpy(feature_obj, feature_start, feature_length);
    feature_obj[feature_length] = '\0';
    
    int start_pos = 0, end_pos = 0;
    char* old_value = find_json_key(feature_obj, key, &start_pos, &end_pos);
    
    char* new_feature = NULL;
    size_t new_length = 0;
    
    if (old_value) {
        // 已存在键，替换值
        free(old_value);
        
        // 构造新值的字符串
        char* new_value_str = (char*)malloc(strlen(value) + 3); // 添加引号和结束符
        if (!new_value_str) {
            log_debug("内存分配失败");
            free(feature_obj);
            free(json);
            return;
        }
        sprintf(new_value_str, "\"%s\"", value);
        
        // 计算新feature对象的长度
        size_t old_value_len = end_pos - start_pos;
        size_t new_value_len = strlen(new_value_str);
        new_length = feature_length - old_value_len + new_value_len;
        
        new_feature = (char*)malloc(new_length + 1);
        if (!new_feature) {
            log_debug("内存分配失败");
            free(new_value_str);
            free(feature_obj);
            free(json);
            return;
        }
        
        // 复制前半部分
        strncpy(new_feature, feature_obj, start_pos);
        new_feature[start_pos] = '\0';
        
        // 添加新值
        strcat(new_feature, new_value_str);
        
        // 添加后半部分
        strcat(new_feature, feature_obj + end_pos);
        
        free(new_value_str);
        log_debug("已替换现有配置项的值");
    } else {
        // 不存在键，添加新的键值对
        // 找到feature对象的结束花括号
        char* insert_pos = strrchr(feature_obj, '}');
        if (!insert_pos) {
            log_debug("无法找到feature对象的结束位置");
            free(feature_obj);
            free(json);
            return;
        }
        
        // 计算插入位置
        size_t pos = insert_pos - feature_obj;
        
        // 检查前面是否有其他键值对
        int is_empty = 1;
        for (char* p = feature_obj + 1; p < insert_pos; p++) {
            if (*p == '"') {
                is_empty = 0;
                break;
            }
        }
        
        // 构造新的键值对
        char* new_pair;
        if (is_empty) {
            new_pair = (char*)malloc(strlen(key) + strlen(value) + 10);
            if (!new_pair) {
                log_debug("内存分配失败");
                free(feature_obj);
                free(json);
                return;
            }
            sprintf(new_pair, "\"%s\": \"%s\"", key, value);
        } else {
            new_pair = (char*)malloc(strlen(key) + strlen(value) + 10);
            if (!new_pair) {
                log_debug("内存分配失败");
                free(feature_obj);
                free(json);
                return;
            }
            sprintf(new_pair, ",\n                \"%s\": \"%s\"", key, value);
        }
        
        // 计算新feature对象的长度
        new_length = feature_length + strlen(new_pair);
        new_feature = (char*)malloc(new_length + 1);
        if (!new_feature) {
            log_debug("内存分配失败");
            free(new_pair);
            free(feature_obj);
            free(json);
            return;
        }
        
        // 复制前半部分
        strncpy(new_feature, feature_obj, pos);
        new_feature[pos] = '\0';
        
        // 添加新的键值对
        strcat(new_feature, new_pair);
        
        // 添加结束花括号
        strcat(new_feature, "}");
        
        free(new_pair);
        log_debug("已添加新的配置项");
    }
    
    // 替换原JSON字符串中的feature对象
    size_t prefix_len = feature_start - json;
    size_t suffix_len = json + file_size - feature_end - 1;
    
    char* new_json = (char*)malloc(prefix_len + new_length + suffix_len + 1);
    if (!new_json) {
        log_debug("内存分配失败");
        free(new_feature);
        free(feature_obj);
        free(json);
        return;
    }
    
    // 复制前半部分JSON
    strncpy(new_json, json, prefix_len);
    new_json[prefix_len] = '\0';
    
    // 添加新的feature对象
    strcat(new_json, new_feature);
    
    // 添加后半部分JSON
    strcat(new_json, feature_end + 1);
    
    // 将新的JSON写入文件
    if (!write_file_contents(config_file, new_json)) {
        log_debug("写入文件失败");
    } else {
        log_debug("配置项修改成功");
    }
    
    free(new_json);
    free(new_feature);
    free(feature_obj);
    free(json);
}

// 读取并处理 ml_csc.txt 文件
void process_ml_csc(const char *ml_csc_file, const char *config_file) {
    log_debug("开始处理 ml_csc.txt 文件...");
    
    FILE *file = fopen(ml_csc_file, "r");
    if (!file) {
        perror("打开 ml_csc.txt 文件失败");
        return;
    }
    
    char line[1024];
    while (fgets(line, sizeof(line), file)) {
        char line_copy[1024];
        strcpy(line_copy, line);
        trim(line_copy);
        
        // 跳过空行或注释行
        if (line_copy[0] == '#' || line_copy[0] == '\0') continue;
        
        // 使用更健壮的方式分割行
        char *action = NULL, *key = NULL, *value = NULL;
        char *token_start = line_copy;
        char *separator = strchr(token_start, '|');
        
        if (separator) {
            *separator = '\0';
            action = strdup(token_start);
            token_start = separator + 1;
            
            separator = strchr(token_start, '|');
            if (separator) {
                *separator = '\0';
                key = strdup(token_start);
                value = strdup(separator + 1);
            } else {
                key = strdup(token_start);
            }
        }
        
        // 检查行格式是否有效
        if (action == NULL || key == NULL) {
            log_debug("无效的行格式，跳过该行。");
            free(action);
            free(key);
            free(value);
            continue;
        }
        
        // 确保没有额外的空格
        trim(action);
        trim(key);
        if (value) trim(value);
        
        // 输出解析后的内容
        printf("[调试] 操作: %s, 键: %s, 值: %s\n", action, key, value ? value : "NULL");
        
        if (strcmp(action, "MODIFY") == 0) {
            if (value == NULL) {
                log_debug("MODIFY 操作缺少值，跳过该行。");
                free(action);
                free(key);
                free(value);
                continue;
            }
            modify_config(key, value, config_file);
        } else if (strcmp(action, "DELETE") == 0) {
            delete_config(key, config_file);
        } else {
            log_debug("未知操作，跳过该行。");
        }
        
        free(action);
        free(key);
        free(value);
    }
    
    fclose(file);
}

// 主函数
int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "用法: %s <配置文件> <ml_csc.txt>\n", argv[0]);
        return 1;
    }
    
    // 调试输出传入的文件路径
    printf("[调试] 修改文件: %s\n", argv[1]);
    printf("[调试] 配置文件: %s\n", argv[2]);
    
    // 读取并处理 ml_csc.txt 文件
    process_ml_csc(argv[2], argv[1]);
    
    return 0;
}


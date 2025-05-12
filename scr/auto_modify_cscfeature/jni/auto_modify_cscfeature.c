#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// 写入调试日志的函数
void log_debug(const char *message) {
    printf("[调试] %s\n", message);
}

// 去除字符串两端的空白符 (空格, tab, newline, etc.)
void trim(char *str) {
  if (str == NULL || *str == '\0') {
    return;
  }
  
  char *start = str;
  // 跳过前导空白符
  while (isspace((unsigned char)*start)) {
    start++;
  }
  
  // 将非空白部分移到字符串开头
  // 如果 str 全是空白，start 会指向 '\0'，下面的循环不执行，dst 最终会指向 str
  char *dst = str;
  if (start != str) { // 只有当有前导空格时才移动
    while (*start != '\0') {
      *dst++ = *start++;
    }
  } else { // 没有前导空格，只需找到末尾
    while (*dst != '\0') {
      dst++;
    }
  }
  *dst = '\0'; // 添加新的结束符
  
  // 如果字符串在去除前导空白后变为空 (例如原字符串 "   ")
  if (*str == '\0') {
    return;
  }
  
  // 去除尾部空白符
  char *end = str + strlen(str) - 1;
  while (end >= str && isspace((unsigned char)*end)) {
    end--;
  }
  *(end + 1) = '\0';
}

// 删除配置项
void delete_config(const char *key, const char *config_file, const char *tag_type) {
  
    FILE *file = fopen(config_file, "r");
    if (!file) {
        perror("打开配置文件失败");
        return;
    }
  
    // 使用固定的临时文件名而不是tmpnam
    char temp_file_name[1024];
    snprintf(temp_file_name, sizeof(temp_file_name), "%s.tmp", config_file);
    FILE *temp_file = fopen(temp_file_name, "w");
    if (!temp_file) {
        perror("创建临时文件失败");
        fclose(file);
        return;
    }
  
    char line[1024];
    int skip_line = 0;
    char tag_pattern[256];
  
    // 构建要查找的标签模式
    snprintf(tag_pattern, sizeof(tag_pattern), "<%s>", key);
  
    while (fgets(line, sizeof(line), file)) {
        char trimmed_line[1024];
        strcpy(trimmed_line, line);
        trim(trimmed_line);
      
        // 如果找到包含要删除的键的行，则标记跳过
        if (strstr(trimmed_line, tag_pattern)) {
            skip_line = 1;
            log_debug("找到要删除的配置项");
        } else if (skip_line && (strstr(trimmed_line, "</") || strstr(trimmed_line, "/>"))) {
            // 如果处于跳过模式，且找到了结束标签，则不再跳过后续行
            skip_line = 0;
            log_debug("配置项删除完成");
            continue;  // 跳过结束标签行
        }
      
        // 如果不在跳过模式，则写入临时文件
        if (!skip_line) {
            fputs(line, temp_file);
        }
    }
  
    // 确保在关闭文件之前完成所有写入操作
    fflush(temp_file);
  
    // 确保先关闭所有文件句柄
    fclose(file);
    fclose(temp_file);
  
    // 用临时文件替换原始文件
    if (remove(config_file) != 0) {
        perror("删除原始配置文件失败");
        return;
    }
  
    if (rename(temp_file_name, config_file) != 0) {
        perror("重命名临时文件失败");
        return;
    }
  
    log_debug("配置项删除过程完成");
}

// 修改配置项
void modify_config(const char *key, const char *value, const char *config_file, const char *tag_type) {  
    FILE *file = fopen(config_file, "r");
    if (!file) {
        perror("打开配置文件失败");
        return;
    }
  
    // 使用固定的临时文件名而不是tmpnam
    char temp_file_name[1024];
    snprintf(temp_file_name, sizeof(temp_file_name), "%s.tmp", config_file);
    FILE *temp_file = fopen(temp_file_name, "w");
    if (!temp_file) {
        perror("创建临时文件失败");
        fclose(file);
        return;
    }
  
    char line[1024];
    int found = 0;
    int in_feature_set = 0;
    char tag_pattern[256];
    char feature_set_start[256];
    char feature_set_end[256];
  
    // 设置特性集标签
    snprintf(feature_set_start, sizeof(feature_set_start), "<%s>", tag_type);
    snprintf(feature_set_end, sizeof(feature_set_end), "</%s>", tag_type);
    
    // 构建要查找的标签模式
    snprintf(tag_pattern, sizeof(tag_pattern), "<%s>", key);
  
    while (fgets(line, sizeof(line), file)) {
        char trimmed_line[1024];
        strcpy(trimmed_line, line);
        trim(trimmed_line);
      
        // 检查是否进入或离开特性集标签
        if (strstr(trimmed_line, feature_set_start)) {
            in_feature_set = 1;
            fputs(line, temp_file);
        } else if (strstr(trimmed_line, feature_set_end)) {
            // 如果我们在离开特性集标签时还没有找到要修改的标签，则在这里添加
            if (!found && in_feature_set) {
                fprintf(temp_file, "    <%s>%s</%s>\n", key, value, key);
                log_debug("在特性集结束前添加了新配置项");
                found = 1;
            }
            in_feature_set = 0;
            fputs(line, temp_file);
        }
        // 如果找到要修改的配置项，则用新值替换
        else if (in_feature_set && strstr(trimmed_line, tag_pattern)) {
            fprintf(temp_file, "    <%s>%s</%s>\n", key, value, key);
            found = 1;
            log_debug("找到并修改了配置项");
          
            // 跳过原始行的剩余部分（如果有多行）
            char temp[1024];
            strcpy(temp, line);
            trim(temp);
          
            if (!(strstr(temp, "</") || strstr(temp, "/>"))) {
                while (fgets(temp, sizeof(temp), file)) {
                    trim(temp);
                    if (strstr(temp, "</") || strstr(temp, "/>")) {
                        break;
                    }
                }
            }
        } else {
            fputs(line, temp_file);
        }
    }
  
    // 如果不存在该配置项，且文件中没有特性集标签，则添加整个结构
    if (!found && !in_feature_set) {
        log_debug("未找到特性集标签，尝试添加新的特性集");
        // 将文件指针重置到开始位置
        fclose(temp_file);
        fclose(file);
        
        file = fopen(config_file, "r");
        temp_file = fopen(temp_file_name, "w");
        
        // 检查文件是否为空
        fseek(file, 0, SEEK_END);
        long file_size = ftell(file);
        rewind(file);
        
        if (file_size == 0) {
            // 如果文件为空，添加XML声明和特性集
            fprintf(temp_file, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
            fprintf(temp_file, "<%s>\n", tag_type);
            fprintf(temp_file, "    <%s>%s</%s>\n", key, value, key);
            fprintf(temp_file, "</%s>\n", tag_type);
            log_debug("在空文件中创建了新的特性集");
        } else {
            // 如果文件不为空，尝试在文件末尾添加特性集
            while (fgets(line, sizeof(line), file)) {
                fputs(line, temp_file);
            }
            fprintf(temp_file, "\n<%s>\n", tag_type);
            fprintf(temp_file, "    <%s>%s</%s>\n", key, value, key);
            fprintf(temp_file, "</%s>\n", tag_type);
            log_debug("在文件末尾添加了新的特性集");
        }
    }
  
    // 确保在关闭文件之前完成所有写入操作
    fflush(temp_file);
  
    // 确保先关闭所有文件句柄
    fclose(file);
    fclose(temp_file);
  
    // 用临时文件替换原始文件
    if (remove(config_file) != 0) {
        perror("删除原始配置文件失败");
        return;
    }
  
    if (rename(temp_file_name, config_file) != 0) {
        perror("重命名临时文件失败");
        return;
    }
  
}

// 读取并处理 ml_csc.txt 文件
void process_ml_csc(const char *ml_csc_file, const char *config_file, const char *tag_type) {
  char msg_buf[1100]; // For log messages
  snprintf(msg_buf, sizeof(msg_buf), "使用标签类型: %s", tag_type);
  log_debug(msg_buf);
  
  FILE *file = fopen(ml_csc_file, "r");
  if (!file) {
    char err_msg[1024];
    snprintf(err_msg, sizeof(err_msg), "打开 ml_csc.txt 文件 %s 失败", ml_csc_file);
    perror(err_msg); 
    return;
  }
  
  char line[1024];
  int line_num = 0;
  while (fgets(line, sizeof(line), file)) {
    line_num++;
    char line_copy[1024];
    strcpy(line_copy, line);
    trim(line_copy); 
    
    // 跳过空行或注释行
    if (line_copy[0] == '#' || line_copy[0] == '\0') {
      continue;
    }
    
    char action[64] = {0};
    char key[256] = {0};
    char value[1024] = {0}; // 初始化为空字符串
    
    // 找到第一个分隔符
    char *first_sep = strchr(line_copy, '|');
    if (first_sep == NULL) {
      snprintf(msg_buf, sizeof(msg_buf), "第 %d 行: 无效格式，缺少操作分隔符 '|'，跳过: \"%s\"", line_num, line_copy);
      log_debug(msg_buf);
      continue;
    }
    
    // 复制操作类型
    size_t action_len = first_sep - line_copy;
    if (action_len >= sizeof(action)) action_len = sizeof(action) - 1;
    strncpy(action, line_copy, action_len);
    action[action_len] = '\0';
    trim(action);
    
    char *rest_of_line = first_sep + 1; // 指向第一个'|'之后的部分
    
    if (strcmp(action, "DELETE") == 0) {
      // DELETE|KEY
      // KEY 是 rest_of_line 的全部内容
      // 检查 KEY 中是否还包含 '|'，如果包含，则格式错误
      if (strchr(rest_of_line, '|')) {
        snprintf(msg_buf, sizeof(msg_buf), "第 %d 行: DELETE 操作格式错误，键名后不应再有分隔符 '|'，跳过: \"%s\"", line_num, line_copy);
        log_debug(msg_buf);
        continue;
      }
      strncpy(key, rest_of_line, sizeof(key) - 1);
      key[sizeof(key) - 1] = '\0';
      trim(key);
      
      if (key[0] == '\0') {
        snprintf(msg_buf, sizeof(msg_buf), "第 %d 行: DELETE 操作缺少键名，跳过: \"%s\"", line_num, line_copy);
        log_debug(msg_buf);
        continue;
      }
      // value 保持为空
    } else if (strcmp(action, "MODIFY") == 0) {
      // MODIFY|KEY|VALUE
      char *second_sep = strchr(rest_of_line, '|');
      if (second_sep == NULL) {
        snprintf(msg_buf, sizeof(msg_buf), "第 %d 行: MODIFY 操作格式无效 (应为 ACTION|KEY|VALUE)，缺少第二个分隔符 '|'，跳过: \"%s\"", line_num, line_copy);
        log_debug(msg_buf);
        continue;
      }
      
      size_t key_len = second_sep - rest_of_line;
      if (key_len >= sizeof(key)) key_len = sizeof(key) - 1;
      strncpy(key, rest_of_line, key_len);
      key[key_len] = '\0';
      trim(key);
      
      if (key[0] == '\0') {
        snprintf(msg_buf, sizeof(msg_buf), "第 %d 行: MODIFY 操作缺少键名，跳过: \"%s\"", line_num, line_copy);
        log_debug(msg_buf);
        continue;
      }
      
      // 复制剩余部分作为值（可能包含多个管道符，但这里假设第二个'|'之后都是value）
      strncpy(value, second_sep + 1, sizeof(value) - 1);
      value[sizeof(value) - 1] = '\0';
      trim(value);
      
      if (value[0] == '\0') { // MODIFY 操作必须有值
        snprintf(msg_buf, sizeof(msg_buf), "第 %d 行: MODIFY 操作缺少值，跳过: \"%s\"", line_num, line_copy);
        log_debug(msg_buf);
        continue;
      }
    } else {
      snprintf(msg_buf, sizeof(msg_buf), "第 %d 行: 未知操作类型 \"%s\"，跳过: \"%s\"", line_num, action, line_copy);
      log_debug(msg_buf);
      continue; // 直接跳过未知操作
    }
    
    // 输出解析后的内容 (使用 log_debug)
    snprintf(msg_buf, sizeof(msg_buf), "第 %d 行: 解析 -> 操作: [%s], 键: [%s], 值: [%s]", line_num, action, key, value);
    log_debug(msg_buf);
    
    if (strcmp(action, "MODIFY") == 0) {
      // MODIFY 操作的值已在上面检查过不能为空
      modify_config(key, value, config_file, tag_type);
    } else if (strcmp(action, "DELETE") == 0) {
      // DELETE 操作的键已在上面检查过不能为空
      delete_config(key, config_file, tag_type);
    }
    // else 分支（未知操作）已在上面解析action时通过continue处理了
  }
  
  fclose(file);
}

// 主函数
int main(int argc, char *argv[]) {
    // 默认使用 FeatureSet 标签
    const char *tag_type = "FeatureSet";
    
    if (argc < 3 || argc > 4) {
        fprintf(stderr, "用法: %s <配置文件> <ml_csc.txt> [tag_type]\n", argv[0]);
        fprintf(stderr, "      tag_type 可选值: FeatureSet (默认) 或 SecFloatingFeatureSet\n");
        return 1;
    }
    
    // 第4个参数，则将其作为标签类型
    if (argc == 4) {
        if (strcmp(argv[3], "FeatureSet") == 0 || strcmp(argv[3], "SecFloatingFeatureSet") == 0) {
            tag_type = argv[3];
        } else {
            fprintf(stderr, "错误: 无效的标签类型 '%s'。必须是 'FeatureSet' 或 'SecFloatingFeatureSet'\n", argv[3]);
            return 1;
        }
    }
  
    // 调试输出传入的文件路径和标签类型
    printf("[调试] 修改文件: %s\n", argv[1]);
    printf("[调试] 配置文件: %s\n", argv[2]);
    printf("[调试] 标签类型: %s\n", tag_type);
  
    // 读取并处理 ml_csc.txt 文件
    process_ml_csc(argv[2], argv[1], tag_type);
  
    return 0;
}


# **OneUI CSC Features 模块重构与规范化计划**

## **1. 核心改进**

### **1.1 数据格式与存储**

1. **废弃** **.txt****，采用结构化格式**：将 ml_csc.txt 等改为 **JSON**。
2. **统一配置存储路径**：用户配置统一存放在 /data/adb/csc_config/。

### **1.2 脚本逻辑改进（**post-fs-data.sh）

1. **错误隔离与容错**：

   

   - 挂载失败不再 exit 1 终止整体流程。
   - 增加 umount 清理逻辑，防止残留挂载。

   

2. **日志系统优化**：

   - 限制日志大小或引入轮转机制，避免 log.txt 过大。
   - 增加 set -x 调试模式开关。

### **1.3 核心功能整合**

1. **二进制功能整合（All-in-One Tool）**：

   将 auto_modify_cscfeature、auto_modify_json_csc、cscdecode、CSC_SORT 整合为单一多功能工具

2. **实现 CSC_SORT**：

   修改完成后对 XML/JSON 条目进行字典序排序，便于对比与维护。

3. **智能备份机制**：

   修改前检查原始文件哈希值，仅在文件或配置发生变化时执行处理，减少 I/O。

### **1.4 鲁棒性与兼容性**

1. **WebUI 状态同步**：

   WebUI 可实时读取 log.txt 或执行状态，反馈挂载情况与错误信息。

## **2. 详细重构路线图**

### **第一阶段：数据格式标准化（Data Refactor）**

- 定义新的 JSON 配置规范，替代 MODIFY|KEY|VALUE 字符串格式，并增加功能描述字段。

**JSON 存储规范**

- 路径：/data/adb/csc_config/[category].json

  例如：csc.json、ff.json

- 结构示例：

```
[
  {
    "key": "CscFeature_Common_ConfigSmsMaxByte",
    "value": "140",
    "command": "MODIFY",
    "desc": "设置短信最大字节数",
    "enabled": true
  }
]
```

- 更新 WebUI 以支持读写新 JSON 格式。



### **第二阶段：底层处理引擎重写（Core Logic）**

1. **合并处理逻辑**

   编写新的核心处理程序，一次读取原始 XML/JSON 与用户配置 JSON，输出最终结果。

2. **统一工具入口设计**

```
csc_tool --decode [src] [dest]      # 自动识别 XML / JSON 解码
csc_tool --patch [src] [config] [dest]
csc_tool --sort [file]              # CSC_SORT（按 Key 字母升序）
csc_tool --encode [src] [dest]      # 编码回原始格式
```

1. **性能优化**

   

   - 使用内存 Pipeline 减少中间文件生成。

   

2. **CSC_SORT 实现**

   

   - 排序后保证 XML 结构合法性。

   

3. **Checksum 校验**

   

   - 原始文件与配置均未变化时跳过处理。

   

------





### **第三阶段：启动脚本规范化（Boot Script）**



- 重写 post-fs-data.sh，采用函数化结构。
- 优化 /prism、/optics 等分区扫描。
- 优化挂载流程，提升系统更新后的稳定性。





**脚本函数结构**



1. log()：统一带时间戳日志。
2. check_env()：检查 libs 权限与架构匹配。
3. apply_patch()：统一调用 All-in-One 工具。
4. safe_mount()：挂载前检查、挂载后验证、失败自动回滚。





------





### **第四阶段：WebUI 高级功能（UX Update）**





1. **实时日志查看**。
2. **一键恢复默认** 与 **导出备份** 功能。
3. 适配新数据格式，提供直观 Key-Value 编辑界面。





**具体实现**



- 引入 JSON 编辑器组件，支持 desc 字段显示与编辑。
- 适配 kernelsu 库，实现直接读写 /data/adb/csc_config/。
- 确立标准 JSON Schema，并提供旧数据转换逻辑。




import 'dart:io';
import 'dart:convert';
import 'package:flutter/material.dart';
import 'package:path_provider/path_provider.dart';

// CSC条目类，用于表示每一个CSC配置项
class CSCEntry {
  final String command; // DELETE 或 MODIFY
  final String key;     // 配置项键名
  final String value;   // 配置项值（只有MODIFY时才有）

  CSCEntry({required this.command, required this.key, this.value = ""});
}

class CSCFileManager {
  static const String basePath = "/data/adb/modules/auto_modify_cscfeature/";

  // 定义文件列表（优先使用无 .example 版本）
  static const Map<String, String> filePairs = {
    "ml_carrier.txt": "ml_carrier.txt.example",
    "ml_csc.txt": "ml_csc.txt.example",
    "ml_ff.txt": "ml_ff.txt.example",
  };

  // 检查是否有root权限
  static Future<bool> hasRootAccess() async {
    try {
      ProcessResult result = await Process.run('su', ['-c', 'echo root_test']);
      return result.stdout.toString().contains('root_test');
    } catch (e) {
      print("检查root权限失败: $e");
      return false;
    }
  }

  // 执行root命令
  static Future<String> execRootCmd(String cmd) async {
    try {
      ProcessResult result = await Process.run('su', ['-c', cmd]);
      return result.stdout.toString();
    } catch (e) {
      print("执行root命令失败: $e");
      return "";
    }
  }

  // 获取实际要读取的文件（如果 .txt 版本存在，优先使用它）
  static Future<String> getActualFileName(String baseName) async {
    String mainFile = basePath + baseName;
    String exampleFile = basePath + filePairs[baseName]!;
    
    try {
      bool hasRoot = await hasRootAccess();
      
      if (hasRoot) {
        // 使用root权限检查文件
        String mainResult = await execRootCmd("[ -f $mainFile ] && echo exists");
        if (mainResult.contains('exists')) {
          return mainFile;
        }
        
        // 检查example文件
        String exampleResult = await execRootCmd("[ -f $exampleFile ] && echo exists");
        if (exampleResult.contains('exists')) {
          return exampleFile;
        }
      } else {
        // 尝试常规方法（这种方法在大多数情况下无法访问/data目录）
        if (await File(mainFile).exists()) {
          return mainFile;
        } else if (await File(exampleFile).exists()) {
          return exampleFile;
        }
      }
    } catch (e) {
      print("检查文件失败: $e");
    }
    
    return ""; // 如果都不存在，返回空
  }

  // 使用root权限读取文件内容
  static Future<List<String>> readFile(String fileName) async {
    try {
      bool hasRoot = await hasRootAccess();
      
      if (hasRoot) {
        String content = await execRootCmd("cat $fileName");
        
        return content.split('\n')
            .map((line) => line.trim())
            .where((line) => line.isNotEmpty && !line.startsWith('#'))
            .toList();
      } else {
        // 尝试常规方法
        File file = File(fileName);
        if (await file.exists()) {
          String content = await file.readAsString();
          return content.split('\n')
              .map((line) => line.trim())
              .where((line) => line.isNotEmpty && !line.startsWith('#'))
              .toList();
        }
      }
    } catch (e) {
      print("读取失败: $e");
      throw Exception("无法读取文件: $e");
    }
    return [];
  }

  // 写入文件内容（如果是 .example 文件，询问是否去除后缀）
  static Future<bool> writeFile(BuildContext context, String fileName, List<String> lines) async {
    try {
      bool hasRoot = await hasRootAccess();
      
      // 添加原始注释说明
      List<String> finalLines = [
        "# 此文件由 CSC 编辑器生成",
        "# 格式：DELETE|CscFeature_Name 或 MODIFY|CscFeature_Name|Value",
        "",
      ];
      
      finalLines.addAll(lines);
      String content = finalLines.join("\n") + "\n";
      
      if (hasRoot) {
        // 使用应用缓存目录创建临时文件
        Directory tempDir = await getTemporaryDirectory();
        String tempFileName = "temp_csc_edit_${DateTime.now().millisecondsSinceEpoch}.txt";
        String tempPath = "${tempDir.path}/$tempFileName";
        
        // 写入临时文件
        await File(tempPath).writeAsString(content);
        
        // 使用root权限复制到目标位置
        await execRootCmd("cp $tempPath $fileName && chmod 644 $fileName");
        
        // 删除临时文件
        await File(tempPath).delete();
        
        // 如果是 .example 文件，询问是否去除 .example 后缀
        if (fileName.endsWith(".example")) {
          String newFileName = fileName.replaceAll(".example", "");
          bool rename = await _confirmRename(context, newFileName);
          if (rename) {
            await execRootCmd("cp $fileName $newFileName && chmod 644 $newFileName");
          }
        }
        return true;
      } else {
        // 尝试常规方法
        File file = File(fileName);
        await file.writeAsString(content);
        
        if (fileName.endsWith(".example")) {
          String newFileName = fileName.replaceAll(".example", "");
          bool rename = await _confirmRename(context, newFileName);
          if (rename) {
            await File(fileName).copy(newFileName);
          }
        }
        return true;
      }
    } catch (e) {
      print("写入失败: $e");
      return false;
    }
  }
  
  // 询问用户是否去除 .example 后缀
  static Future<bool> _confirmRename(BuildContext context, String newFileName) async {
    return await showDialog<bool>(
          context: context,
          builder: (context) {
            return AlertDialog(
              title: Text("去除 .example 后缀？"),
              content: Text("是否同时保存为 ${newFileName.split('/').last}？\n原 example 文件将保留作为备份。"),
              actions: [
                TextButton(onPressed: () => Navigator.of(context).pop(false), child: Text("否")),
                TextButton(onPressed: () => Navigator.of(context).pop(true), child: Text("是")),
              ],
            );
          },
        ) ??
        false;
  }

  // 列出目录中的所有文件
  static Future<List<String>> listDirectoryFiles() async {
    try {
      bool hasRoot = await hasRootAccess();
      
      if (hasRoot) {
        String output = await execRootCmd("ls -la $basePath");
        
        return output.split('\n')
            .where((line) => line.isNotEmpty)
            .map((line) => line.trim())
            .toList();
      }
    } catch (e) {
      print("列出目录失败: $e");
    }
    return [];
  }

  // 解析文件内容为CSCEntry列表
  static List<CSCEntry> parseContent(List<String> lines) {
    List<CSCEntry> entries = [];
    
    for (String line in lines) {
      line = line.trim();
      if (line.isEmpty || line.startsWith('#')) continue;
      
      if (line.startsWith("DELETE|")) {
        List<String> parts = line.split("|");
        if (parts.length >= 2) {
          entries.add(CSCEntry(
            command: "DELETE",
            key: parts[1],
          ));
        }
      } else if (line.startsWith("MODIFY|")) {
        List<String> parts = line.split("|");
        if (parts.length >= 3) {
          entries.add(CSCEntry(
            command: "MODIFY",
            key: parts[1],
            value: parts[2],
          ));
        }
      }
    }
    
    return entries;
  }

  // 将CSCEntry列表转换回文本格式
  static List<String> convertToLines(List<CSCEntry> entries) {
    List<String> lines = [];
    
    for (CSCEntry entry in entries) {
      if (entry.command == "DELETE") {
        lines.add("DELETE|${entry.key}");
      } else if (entry.command == "MODIFY") {
        lines.add("MODIFY|${entry.key}|${entry.value}");
      }
    }
    
    return lines;
  }
}

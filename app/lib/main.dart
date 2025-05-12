import 'package:flutter/material.dart';
import 'dart:io';
import 'csc_file_manager.dart';

void main() {
  runApp(MyApp());
}

class MyApp extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'CSC 编辑器',
      theme: ThemeData(
        primarySwatch: Colors.blue,
        visualDensity: VisualDensity.adaptivePlatformDensity,
      ),
      home: FileListScreen(),
    );
  }
}

class FileListScreen extends StatefulWidget {
  @override
  _FileListScreenState createState() => _FileListScreenState();
}

class _FileListScreenState extends State<FileListScreen> {
  Map<String, bool> _fileExists = {};
  bool _isLoading = true;
  String _diagnosisResult = "";

  @override
  void initState() {
    super.initState();
    _checkFiles();
  }

  Future<void> _checkFiles() async {
    setState(() {
      _isLoading = true;
    });

    Map<String, bool> exists = {};
    for (String fileName in CSCFileManager.filePairs.keys) {
      String actualFile = await CSCFileManager.getActualFileName(fileName);
      exists[fileName] = actualFile.isNotEmpty;
    }

    setState(() {
      _fileExists = exists;
      _isLoading = false;
    });
  }
  
  Future<void> _runDiagnosis() async {
    setState(() {
      _isLoading = true;
      _diagnosisResult = "正在诊断问题...";
    });

    StringBuffer result = StringBuffer();
    
    // 测试 root 权限
    bool hasRoot = await CSCFileManager.hasRootAccess();
    result.writeln("Root权限: ${hasRoot ? '✅ 已获取' : '❌ 未获取'}");
    
    // 测试目录访问
    if (hasRoot) {
      String lsOutput = await CSCFileManager.execRootCmd("ls -la ${CSCFileManager.basePath}");
      result.writeln("\n目录内容 (${CSCFileManager.basePath}):");
      if (lsOutput.isEmpty) {
        result.writeln("❌ 无法访问目录或目录为空");
      } else {
        result.writeln(lsOutput);
      }
      
      // 检查各文件
      for (String fileName in CSCFileManager.filePairs.keys) {
        String mainFile = CSCFileManager.basePath + fileName;
        String exampleFile = CSCFileManager.basePath + CSCFileManager.filePairs[fileName]!;
        
        String mainExists = await CSCFileManager.execRootCmd("[ -f $mainFile ] && echo 存在 || echo 不存在");
        String exampleExists = await CSCFileManager.execRootCmd("[ -f $exampleFile ] && echo 存在 || echo 不存在");
        
        result.writeln("\n文件状态:");
        result.writeln("- $mainFile: ${mainExists.trim()}");
        result.writeln("- $exampleFile: ${exampleExists.trim()}");
      }
    } else {
      result.writeln("\n❌ 无法获取Root权限，请确保:");
      result.writeln("1. 您的设备已经root");
      result.writeln("2. 已经允许此应用获取root权限");
      result.writeln("3. SELinux可能是阻碍因素，您可以尝试设置为permissive模式");
    }
    
    setState(() {
      _diagnosisResult = result.toString();
      _isLoading = false;
    });
  }

  Future<void> _openFile(BuildContext context, String fileName) async {
    try {
      String actualFile = await CSCFileManager.getActualFileName(fileName);
      
      if (actualFile.isEmpty) {
        // 文件不存在，创建新文件
        actualFile = CSCFileManager.basePath + fileName + ".example";
        
        bool confirm = await showDialog<bool>(
          context: context,
          builder: (context) {
            return AlertDialog(
              title: Text("创建新文件"),
              content: Text("文件 $fileName 不存在，是否创建?"),
              actions: [
                TextButton(onPressed: () => Navigator.of(context).pop(false), child: Text("取消")),
                TextButton(onPressed: () => Navigator.of(context).pop(true), child: Text("创建")),
              ],
            );
          },
        ) ?? false;
        
        if (!confirm) return;
        
        // 创建空的 CSC 文件
        await CSCFileManager.writeFile(context, actualFile, []);
      }
      
      List<String> fileContent = await CSCFileManager.readFile(actualFile);
      List<CSCEntry> entries = CSCFileManager.parseContent(fileContent);
      
      // 导航到编辑页面
      Navigator.push(
        context,
        MaterialPageRoute(
          builder: (context) => EditScreen(
            fileName: fileName,
            filePath: actualFile,
            entries: entries,
          ),
        ),
      ).then((_) => _checkFiles());
      
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text("无法打开文件: $e")),
      );
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text("CSC 文件编辑器"),
        actions: [
          IconButton(
            icon: Icon(Icons.refresh),
            onPressed: _checkFiles,
          ),
          IconButton(
            icon: Icon(Icons.bug_report),
            onPressed: _runDiagnosis,
            tooltip: "运行诊断",
          ),
        ],
      ),
      body: _isLoading 
        ? Center(child: CircularProgressIndicator())
        : Column(
            children: [
              if (_diagnosisResult.isNotEmpty)
                Expanded(
                  flex: 1,
                  child: Container(
                    margin: EdgeInsets.all(8),
                    padding: EdgeInsets.all(8),
                    decoration: BoxDecoration(
                      border: Border.all(color: Colors.grey),
                      borderRadius: BorderRadius.circular(8),
                    ),
                    child: SingleChildScrollView(
                      child: Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text("诊断结果:", style: TextStyle(fontWeight: FontWeight.bold)),
                          SizedBox(height: 8),
                          Text(_diagnosisResult),
                        ],
                      ),
                    ),
                  ),
                ),
              Expanded(
                flex: 2,
                child: ListView(
                  children: CSCFileManager.filePairs.keys.map((file) {
                    bool exists = _fileExists[file] ?? false;
                    return ListTile(
                      title: Text(file),
                      subtitle: Text(exists ? "文件可用" : "文件不存在"),
                      leading: Icon(
                        exists ? Icons.description : Icons.error_outline,
                        color: exists ? Colors.blue : Colors.grey,
                      ),
                      trailing: exists ? Icon(Icons.edit) : Icon(Icons.add),
                      onTap: () => _openFile(context, file),
                    );
                  }).toList(),
                ),
              ),
            ],
          ),
    );
  }
}

// 添加一个编辑页面
class EditScreen extends StatefulWidget {
  final String fileName;
  final String filePath;
  final List<CSCEntry> entries;
  
  EditScreen({
    required this.fileName,
    required this.filePath,
    required this.entries,
  });
  
  @override
  _EditScreenState createState() => _EditScreenState();
}

class _EditScreenState extends State<EditScreen> {
  late List<CSCEntry> _entries;
  bool _isModified = false;
  
  @override
  void initState() {
    super.initState();
    _entries = List.from(widget.entries); // 复制一份，避免修改原始数据
  }
  
  Future<void> _saveFile() async {
    try {
      List<String> lines = CSCFileManager.convertToLines(_entries);
      bool success = await CSCFileManager.writeFile(context, widget.filePath, lines);
      
      if (success) {
        setState(() {
          _isModified = false;
        });
        
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text("保存成功")),
        );
      } else {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text("保存失败")),
        );
      }
    } catch (e) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text("保存失败: $e")),
      );
    }
  }
  
  void _addEntry() {
    showDialog(
      context: context,
      builder: (context) {
        String command = "MODIFY";
        String key = "";
        String value = "";
        
        return StatefulBuilder(
          builder: (context, setState) {
            return AlertDialog(
              title: Text("添加配置项"),
              content: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  DropdownButton<String>(
                    value: command,
                    onChanged: (newValue) {
                      setState(() {
                        command = newValue!;
                      });
                    },
                    items: ["MODIFY", "DELETE"].map((value) {
                      return DropdownMenuItem<String>(
                        value: value,
                        child: Text(value),
                      );
                    }).toList(),
                  ),
                  TextField(
                    decoration: InputDecoration(labelText: "配置键名"),
                    onChanged: (text) => key = text,
                  ),
                  if (command == "MODIFY")
                    TextField(
                      decoration: InputDecoration(labelText: "配置值"),
                      onChanged: (text) => value = text,
                    ),
                ],
              ),
              actions: [
                TextButton(
                  onPressed: () => Navigator.of(context).pop(),
                  child: Text("取消"),
                ),
                TextButton(
                  onPressed: () {
                    if (key.isEmpty) {
                      ScaffoldMessenger.of(context).showSnackBar(
                        SnackBar(content: Text("键名不能为空")),
                      );
                      return;
                    }
                    
                    CSCEntry newEntry = CSCEntry(
                      command: command,
                      key: key,
                      value: command == "MODIFY" ? value : "",
                    );
                    
                    setState(() {
                      this.setState(() {
                        _entries.add(newEntry);
                        _isModified = true;
                      });
                    });
                    
                    Navigator.of(context).pop();
                  },
                  child: Text("添加"),
                ),
              ],
            );
          },
        );
      },
    );
  }
  
  void _editEntry(int index) {
    CSCEntry entry = _entries[index];
    
    showDialog(
      context: context,
      builder: (context) {
        String command = entry.command;
        String key = entry.key;
        String value = entry.value;
        
        return StatefulBuilder(
          builder: (context, setState) {
            return AlertDialog(
              title: Text("编辑配置项"),
              content: Column(
                mainAxisSize: MainAxisSize.min,
                children: [
                  DropdownButton<String>(
                    value: command,
                    onChanged: (newValue) {
                      setState(() {
                        command = newValue!;
                      });
                    },
                    items: ["MODIFY", "DELETE"].map((value) {
                      return DropdownMenuItem<String>(
                        value: value,
                        child: Text(value),
                      );
                    }).toList(),
                  ),
                  TextField(
                    decoration: InputDecoration(labelText: "配置键名"),
                    controller: TextEditingController(text: key),
                    onChanged: (text) => key = text,
                  ),
                  if (command == "MODIFY")
                    TextField(
                      decoration: InputDecoration(labelText: "配置值"),
                      controller: TextEditingController(text: value),
                      onChanged: (text) => value = text,
                    ),
                ],
              ),
              actions: [
                TextButton(
                  onPressed: () => Navigator.of(context).pop(),
                  child: Text("取消"),
                ),
                TextButton(
                  onPressed: () {
                    if (key.isEmpty) {
                      ScaffoldMessenger.of(context).showSnackBar(
                        SnackBar(content: Text("键名不能为空")),
                      );
                      return;
                    }
                    
                    CSCEntry updatedEntry = CSCEntry(
                      command: command,
                      key: key,
                      value: command == "MODIFY" ? value : "",
                    );
                    
                    this.setState(() {
                      _entries[index] = updatedEntry;
                      _isModified = true;
                    });
                    
                    Navigator.of(context).pop();
                  },
                  child: Text("保存"),
                ),
              ],
            );
          },
        );
      },
    );
  }
  
  void _deleteEntry(int index) {
    showDialog(
      context: context,
      builder: (context) {
        return AlertDialog(
          title: Text("确认删除"),
          content: Text("确定要删除此配置项吗？"),
          actions: [
            TextButton(
              onPressed: () => Navigator.of(context).pop(),
              child: Text("取消"),
            ),
            TextButton(
              onPressed: () {
                setState(() {
                  _entries.removeAt(index);
                  _isModified = true;
                });
                Navigator.of(context).pop();
              },
              child: Text("删除"),
            ),
          ],
        );
      },
    );
  }
  
  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(widget.fileName),
        actions: [
          IconButton(
            icon: Icon(Icons.save),
            onPressed: _isModified ? _saveFile : null,
          ),
        ],
      ),
      body: Column(
        children: [
          Padding(
            padding: EdgeInsets.all(8.0),
            child: Text(
              "文件路径: ${widget.filePath}",
              style: TextStyle(fontSize: 12, fontStyle: FontStyle.italic),
            ),
          ),
          Expanded(
            child: _entries.isEmpty
                ? Center(child: Text("文件为空，点击下方 + 按钮添加配置项"))
                : ListView.builder(
                    itemCount: _entries.length,
                    itemBuilder: (context, index) {
                      CSCEntry entry = _entries[index];
                      
                      return ListTile(
                        title: Text(entry.key),
                        subtitle: Text(
                            "${entry.command} ${entry.command == 'MODIFY' ? '| ' + entry.value : ''}"),
                        leading: Icon(
                          entry.command == "MODIFY" ? Icons.edit_attributes : Icons.delete_forever,
                          color: entry.command == "MODIFY" ? Colors.blue : Colors.red,
                        ),
                        trailing: Row(
                          mainAxisSize: MainAxisSize.min,
                          children: [
                            IconButton(
                              icon: Icon(Icons.edit),
                              onPressed: () => _editEntry(index),
                            ),
                            IconButton(
                              icon: Icon(Icons.delete),
                              onPressed: () => _deleteEntry(index),
                            ),
                          ],
                        ),
                      );
                    },
                  ),
          ),
        ],
      ),
      floatingActionButton: FloatingActionButton(
        onPressed: _addEntry,
        tooltip: "添加配置项",
        child: Icon(Icons.add),
      ),
    );
  }
}

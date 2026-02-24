import React, { useState, useEffect } from 'react';
import { exec, toast } from 'kernelsu';
import './App.css';

interface CSCEntry {
  command: 'MODIFY' | 'DELETE';
  key: string;
  value: string;
  desc: string;
  enabled: boolean;
}

const FILE_LIST = [
  { label: 'Carrier', name: 'carrier.json', oldTxt: 'ml_carrier.txt' },
  { label: 'CSC', name: 'csc.json', oldTxt: 'ml_csc.txt' },
  { label: 'FF', name: 'ff.json', oldTxt: 'ml_ff.txt' },
];

const CONFIG_PATH = "/data/adb/csc_config/";
const MODULE_PATH = "/data/adb/modules/auto_modify_cscfeature/";

export const App: React.FC = () => {
  const [currentFile, setCurrentFile] = useState(FILE_LIST[0]);
  const [entries, setEntries] = useState<CSCEntry[]>([]);
  const [loading, setLoading] = useState(false);
  const [status, setStatus] = useState('');
  const [isModalOpen, setIsModalOpen] = useState(false);
  const [editingIndex, setEditingIndex] = useState<number | null>(null);

  const [formData, setFormData] = useState<CSCEntry>({ 
    command: 'MODIFY', 
    key: '', 
    value: '', 
    desc: '', 
    enabled: true 
  });

  useEffect(() => {
    initAndLoad();
  }, [currentFile]);

  const initAndLoad = async () => {
    setLoading(true);
    // 确保配置目录存在
    await exec(`mkdir -p ${CONFIG_PATH}`);
    await loadFile(currentFile);
    setLoading(false);
  };

  const loadFile = async (fileInfo: typeof FILE_LIST[0]) => {
    setStatus(`正在读取 ${fileInfo.name}...`);
    
    const jsonPath = `${CONFIG_PATH}${fileInfo.name}`;
    let res = await exec(`[ -f ${jsonPath} ] && cat ${jsonPath}`);
    
    if (res.errno === 0 && res.stdout.trim()) {
      try {
        const parsed = JSON.parse(res.stdout);
        setEntries(parsed);
        setStatus(`已加载: ${jsonPath}`);
        return;
      } catch (e) {
        console.error("JSON 解析失败", e);
      }
    }

    // 如果 JSON 不存在，尝试从旧的 .txt 或 .example 迁移/读取
    setStatus(`尝试迁移旧配置或读取示例...`);
    const oldPath = `${MODULE_PATH}${fileInfo.oldTxt}`;
    const examplePath = `${MODULE_PATH}${fileInfo.oldTxt}.example`;
    
    let oldRes = await exec(`[ -f ${oldPath} ] && cat ${oldPath} || ([ -f ${examplePath} ] && cat ${examplePath})`);
    
    if (oldRes.errno === 0 && oldRes.stdout.trim()) {
      const migrated: CSCEntry[] = oldRes.stdout.split(/\r?\n/)
        .map(line => line.trim())
        .filter(line => line && !line.startsWith('#'))
        .map(line => {
          const parts = line.split('|');
          return {
            command: (parts[0] === 'DELETE' ? 'DELETE' : 'MODIFY') as any,
            key: parts[1] || '',
            value: parts[2] || '',
            desc: '',
            enabled: true
          };
        });
      setEntries(migrated);
      setStatus(`已从旧格式迁移 (未保存)`);
    } else {
      setEntries([]);
      setStatus('未找到配置，请点击添加。');
    }
  };

  const handleSave = async () => {
    setLoading(true);
    const content = JSON.stringify(entries, null, 2);
    const target = `${CONFIG_PATH}${currentFile.name}`;
    const temp = `/data/local/tmp/csc_config_temp.json`;

    try {
      // 使用 base64 传输以避免转义字符问题
      const base64Content = btoa(unescape(encodeURIComponent(content)));
      await exec(`echo "${base64Content}" | base64 -d > ${temp}`);
      const res = await exec(`cp ${temp} ${target} && chmod 644 ${target} && rm ${temp}`);
      
      if (res.errno === 0) {
        setStatus(`已成功保存至 ${target}`);
        toast('保存成功！');
      } else {
        throw new Error(res.stderr);
      }
    } catch (e: any) {
      toast(`保存失败: ${e.message}`);
    }
    setLoading(false);
  };

  const openModal = (index: number | null = null) => {
    if (index !== null) {
      setFormData({ ...entries[index] });
      setEditingIndex(index);
    } else {
      setFormData({ command: 'MODIFY', key: '', value: '', desc: '', enabled: true });
      setEditingIndex(null);
    }
    setIsModalOpen(true);
  };

  const saveEntry = () => {
    if (!formData.key) {
        toast('键名不能为空');
        return;
    }
    const newEntries = [...entries];
    if (editingIndex !== null) {
      newEntries[editingIndex] = formData;
    } else {
      newEntries.push(formData);
    }
    setEntries(newEntries);
    setIsModalOpen(false);
  };

  const toggleEnable = (index: number) => {
    const newEntries = [...entries];
    newEntries[index].enabled = !newEntries[index].enabled;
    setEntries(newEntries);
  };

  const removeEntry = (index: number) => {
    if (confirm('确定删除此项吗？')) {
      const newEntries = [...entries];
      newEntries.splice(index, 1);
      setEntries(newEntries);
    }
  };

  return (
    <div className="container">
      <header>
        <h1>CSC Editor v2</h1>
        <div className="tabs">
          {FILE_LIST.map(f => (
            <button 
              key={f.name}
              className={currentFile.name === f.name ? 'active' : ''}
              onClick={() => setCurrentFile(f)}
            >
              {f.label}
            </button>
          ))}
        </div>
      </header>

      <div className="status">{status}</div>

      <div className="actions">
        <button className="btn primary" onClick={() => openModal()}>添加配置</button>
        <button className="btn success" onClick={handleSave} disabled={loading}>保存全部</button>
      </div>

      <div className="list">
        {entries.map((entry, i) => (
          <div key={i} className={`item ${!entry.enabled ? 'disabled' : ''}`}>
            <div className="info" onClick={() => toggleEnable(i)}>
              <div className="key">{entry.key}</div>
              <div className="meta">
                <span className={`badge ${entry.command.toLowerCase()}`}>{entry.command}</span>
                {entry.command === 'MODIFY' && <span className="val"> | {entry.value}</span>}
              </div>
              {entry.desc && <div className="desc">{entry.desc}</div>}
            </div>
            <div className="btns">
              <button className="edit-btn" onClick={(e) => { e.stopPropagation(); openModal(i); }}>✎</button>
              <button className="del-btn" onClick={(e) => { e.stopPropagation(); removeEntry(i); }}>🗑</button>
            </div>
          </div>
        ))}
      </div>

      {isModalOpen && (
        <div className="modal">
          <div className="modal-content">
            <h3>{editingIndex !== null ? '编辑' : '添加'}配置</h3>
            
            <label className="form-label">操作</label>
            <select 
              value={formData.command} 
              onChange={e => setFormData({...formData, command: e.target.value as any})}
            >
              <option value="MODIFY">MODIFY</option>
              <option value="DELETE">DELETE</option>
            </select>

            <label className="form-label">键名 (Key)</label>
            <input 
              placeholder="CscFeature_..."
              value={formData.key}
              onChange={e => setFormData({...formData, key: e.target.value})}
            />

            {formData.command === 'MODIFY' && (
              <>
                <label className="form-label">键值 (Value)</label>
                <input 
                  placeholder="Value"
                  value={formData.value}
                  onChange={e => setFormData({...formData, value: e.target.value})}
                />
              </>
            )}

            <label className="form-label">描述 (Description)</label>
            <input 
              placeholder="此功能的作用..."
              value={formData.desc}
              onChange={e => setFormData({...formData, desc: e.target.value})}
            />

            <div className="modal-btns">
              <button onClick={() => setIsModalOpen(false)}>取消</button>
              <button className="primary" onClick={saveEntry}>确定</button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

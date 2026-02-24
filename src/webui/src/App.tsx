import React, { useState, useEffect, useMemo } from 'react';
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
  { label: 'Carrier', name: 'carrier.json', decoded: 'decoded_carrier' },
  { label: 'CSC', name: 'csc.json', decoded: 'decoded_csc' },
  { label: 'FF', name: 'ff.json', decoded: 'decoded_ff' },
];

const CONFIG_PATH = "/data/adb/csc_config/";
const MODULE_PATH = "/data/adb/modules/auto_modify_cscfeature/";

export const App: React.FC = () => {
  const [currentFile, setCurrentFile] = useState(FILE_LIST[1]); // 默认 CSC
  const [entries, setEntries] = useState<CSCEntry[]>([]);
  const [originEntries, setOriginEntries] = useState<Record<string, string>>({});
  const [loading, setLoading] = useState(false);
  const [status, setStatus] = useState('');
  const [searchTerm, setSearchTerm] = useState('');
  const [showOnlyModified, setShowOnlyModified] = useState(false);
  
  const [isModalOpen, setIsModalOpen] = useState(false);
  const [editingIndex, setEditingIndex] = useState<number | null>(null);
  const [formData, setFormData] = useState<CSCEntry>({ 
    command: 'MODIFY', key: '', value: '', desc: '', enabled: true 
  });

  useEffect(() => {
    loadData();
  }, [currentFile]);

  const loadData = async () => {
    setLoading(true);
    setStatus(`正在同步数据...`);
    
    // 1. 加载用户修改 JSON
    const jsonPath = `${CONFIG_PATH}${currentFile.name}`;
    let res = await exec(`[ -f ${jsonPath} ] && cat ${jsonPath}`);
    let userEntries: CSCEntry[] = [];
    if (res.errno === 0 && res.stdout.trim()) {
      try { userEntries = JSON.parse(res.stdout); } catch (e) {}
    }
    setEntries(userEntries);

    // 2. 加载解密后的原始文件 (由 post-fs-data.sh 生成)
    const decodedPath = `${MODULE_PATH}${currentFile.decoded}`;
    let decodedRes = await exec(`[ -f ${decodedPath} ] && cat ${decodedPath}`);
    let originMap: Record<string, string> = {};
    
    if (decodedRes.errno === 0 && decodedRes.stdout.trim()) {
      const content = decodedRes.stdout;
      if (currentFile.name.includes('json')) {
        // 解析 Carrier JSON
        try {
          const parsed = JSON.parse(content);
          const featObj = parsed.customer[0]?.feature || {};
          originMap = featObj;
        } catch(e) {}
      } else {
        // 解析 XML (CSC/FF)
        const lines = content.split(/\r?\n/);
        lines.forEach(line => {
          const match = line.match(/<([^>]+)>([^<]*)<\/\1>/);
          if (match) originMap[match[1]] = match[2];
        });
      }
    }
    setOriginEntries(originMap);
    setStatus(`已加载: ${currentFile.label}`);
    setLoading(false);
  };

  // 综合搜索与过滤后的列表
  const filteredList = useMemo(() => {
    const term = searchTerm.toLowerCase();
    
    // 合并用户已修改的和原始文件中存在但用户没修改的
    const allKeys = Array.from(new Set([...entries.map(e => e.key), ...Object.keys(originEntries)]));
    
    let result = allKeys.map(key => {
      const modified = entries.find(e => e.key === key);
      const originValue = originEntries[key];
      return { key, modified, originValue };
    });

    if (showOnlyModified) {
      result = result.filter(item => item.modified);
    }

    if (term) {
      result = result.filter(item => 
        item.key.toLowerCase().includes(term) || 
        (item.modified?.desc || '').toLowerCase().includes(term)
      );
    }

    return result;
  }, [entries, originEntries, searchTerm, showOnlyModified]);

  const handleSave = async () => {
    setLoading(true);
    const content = JSON.stringify(entries, null, 2);
    const target = `${CONFIG_PATH}${currentFile.name}`;
    try {
      const base64Content = btoa(unescape(encodeURIComponent(content)));
      await exec(`echo "${base64Content}" | base64 -d > ${target} && chmod 644 ${target}`);
      toast('保存成功！');
      // 清理 Hash 缓存触发重新 Patch
      await exec(`rm ${MODULE_PATH}last_hashes.txt`);
    } catch (e: any) {
      toast(`保存失败: ${e.message}`);
    }
    setLoading(false);
  };

  const openModal = (key: string, modified?: CSCEntry, originValue?: string) => {
    if (modified) {
      setFormData({ ...modified });
      const idx = entries.findIndex(e => e.key === key);
      setEditingIndex(idx);
    } else {
      setFormData({ command: 'MODIFY', key, value: originValue || '', desc: '', enabled: true });
      setEditingIndex(null);
    }
    setIsModalOpen(true);
  };

  const confirmModal = () => {
    if (!formData.key) return;
    const newEntries = [...entries];
    if (editingIndex !== null) {
      newEntries[editingIndex] = formData;
    } else {
      newEntries.push(formData);
    }
    setEntries(newEntries);
    setIsModalOpen(false);
  };

  const removeEntry = (key: string) => {
    if (confirm('还原此项到原始状态？')) {
      setEntries(entries.filter(e => e.key !== key));
    }
  };

  return (
    <div className="container">
      <header className="sticky-header">
        <h1>CSC Editor v3</h1>
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
        <div className="search-bar">
          <input 
            type="text" 
            placeholder="搜索键名或描述..." 
            value={searchTerm}
            onChange={e => setSearchTerm(e.target.value)}
          />
          <button 
            className={`filter-btn ${showOnlyModified ? 'active' : ''}`}
            onClick={() => setShowOnlyModified(!showOnlyModified)}
          >
            已修改
          </button>
        </div>
      </header>

      <div className="status">{status} ({filteredList.length} 项)</div>

      <div className="list">
        {filteredList.map(item => (
          <div key={item.key} className={`item ${item.modified ? 'is-modified' : ''} ${item.modified?.enabled === false ? 'disabled' : ''}`}>
            <div className="info" onClick={() => openModal(item.key, item.modified, item.originValue)}>
              <div className="key">{item.key}</div>
              <div className="compare-view">
                <div className="val-box">
                  <span className="label">原始:</span>
                  <span className="val">{item.originValue || '(空)'}</span>
                </div>
                {item.modified && (
                  <div className="val-box mod">
                    <span className="label">当前:</span>
                    <span className="val">{item.modified.command === 'DELETE' ? '[已删除]' : item.modified.value}</span>
                  </div>
                )}
              </div>
              {item.modified?.desc && <div className="desc">{item.modified.desc}</div>}
            </div>
            {item.modified && (
              <div className="btns">
                <button className="del-btn" onClick={() => removeEntry(item.key)}>↺</button>
              </div>
            )}
          </div>
        ))}
      </div>

      <div className="footer-actions">
        <button className="btn success" onClick={handleSave} disabled={loading}>保存修改并重启生效</button>
      </div>

      {isModalOpen && (
        <div className="modal">
          <div className="modal-content">
            <h3>编辑配置</h3>
            <div className="field">
              <label>Key</label>
              <input value={formData.key} disabled />
            </div>
            <div className="field">
              <label>操作类型</label>
              <select 
                value={formData.command} 
                onChange={e => setFormData({...formData, command: e.target.value as any})}
              >
                <option value="MODIFY">MODIFY (修改)</option>
                <option value="DELETE">DELETE (删除)</option>
              </select>
            </div>
            {formData.command === 'MODIFY' && (
              <div className="field">
                <label>键值 (Value)</label>
                <input 
                  value={formData.value}
                  onChange={e => setFormData({...formData, value: e.target.value})}
                />
              </div>
            )}
            <div className="field">
              <label>功能描述 (自定义)</label>
              <input 
                placeholder="例如: 开启通话录音"
                value={formData.desc}
                onChange={e => setFormData({...formData, desc: e.target.value})}
              />
            </div>
            <div className="modal-btns">
              <button onClick={() => setIsModalOpen(false)}>取消</button>
              <button className="primary" onClick={confirmModal}>确定</button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

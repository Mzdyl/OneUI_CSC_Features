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

type FilterMode = 'ALL' | 'ENABLED' | 'DISABLED' | 'UNSET';

const FILE_LIST = [
  { label: 'Carrier', name: 'carrier.json', decoded: 'decoded_carrier' },
  { label: 'CSC', name: 'csc.json', decoded: 'decoded_csc' },
  { label: 'FF', name: 'ff.json', decoded: 'decoded_ff' },
];

const CONFIG_PATH = "/data/adb/csc_config/";
const MODULE_PATH = "/data/adb/modules/auto_modify_cscfeature/";

export const App: React.FC = () => {
  const [currentFile, setCurrentFile] = useState(FILE_LIST[1]); 
  const [entries, setEntries] = useState<CSCEntry[]>([]);
  const [originEntries, setOriginEntries] = useState<Record<string, string>>({});
  const [loading, setLoading] = useState(false);
  const [status, setStatus] = useState('');
  const [searchTerm, setSearchTerm] = useState('');
  const [filterMode, setFilterMode] = useState<FilterMode>('ALL');
  
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
    setStatus(`同步中...`);
    
    const jsonPath = `${CONFIG_PATH}${currentFile.name}`;
    let res = await exec(`[ -f ${jsonPath} ] && cat ${jsonPath}`);
    let userEntries: CSCEntry[] = [];
    if (res.errno === 0 && res.stdout.trim()) {
      try { userEntries = JSON.parse(res.stdout); } catch (e) {}
    }
    setEntries(userEntries);

    const decodedPath = `${MODULE_PATH}${currentFile.decoded}`;
    let decodedRes = await exec(`[ -f ${decodedPath} ] && cat ${decodedPath}`);
    let originMap: Record<string, string> = {};
    
    if (decodedRes.errno === 0 && decodedRes.stdout.trim()) {
      const content = decodedRes.stdout;
      if (currentFile.name.includes('carrier')) {
        try {
          const parsed = JSON.parse(content);
          originMap = parsed.customer[0]?.feature || {};
        } catch(e) {}
      } else {
        const lines = content.split(/\r?\n/);
        lines.forEach(line => {
          const match = line.match(/<([^>]+)>([^<]*)<\/\1>/);
          if (match) originMap[match[1]] = match[2];
        });
      }
    }
    setOriginEntries(originMap);
    setStatus(`已就绪`);
    setLoading(false);
  };

  const syncToDisk = async (newEntries: CSCEntry[]) => {
    const content = JSON.stringify(newEntries, null, 2);
    const target = `${CONFIG_PATH}${currentFile.name}`;
    try {
      const base64Content = btoa(unescape(encodeURIComponent(content)));
      await exec(`echo "${base64Content}" | base64 -d > ${target} && chmod 644 ${target}`);
      setStatus(`已保存`);
    } catch (e: any) {
      toast(`同步失败: ${e.message}`);
    }
  };

  const filteredList = useMemo(() => {
    const term = searchTerm.toLowerCase();
    const allKeys = Array.from(new Set([...entries.map(e => e.key), ...Object.keys(originEntries)]));
    
    let result = allKeys.map(key => {
      const modified = entries.find(e => e.key === key);
      const originValue = originEntries[key];
      return { key, modified, originValue };
    });

    // 过滤逻辑
    if (filterMode === 'ENABLED') result = result.filter(item => item.modified?.enabled === true);
    if (filterMode === 'DISABLED') result = result.filter(item => item.modified?.enabled === false);
    if (filterMode === 'UNSET') result = result.filter(item => !item.modified);

    if (term) {
      result = result.filter(item => 
        item.key.toLowerCase().includes(term) || 
        (item.modified?.desc || '').toLowerCase().includes(term)
      );
    }

    // 排序逻辑：开启项 > 禁用项 > 未设定项
    return result.sort((a, b) => {
      const getScore = (item: typeof a) => {
        if (item.modified?.enabled === true) return 0;
        if (item.modified?.enabled === false) return 1;
        return 2;
      };
      return getScore(a) - getScore(b);
    });
  }, [entries, originEntries, searchTerm, filterMode]);

  const toggleEnable = (key: string) => {
    const nextEntries = [...entries];
    const idx = nextEntries.findIndex(e => e.key === key);
    if (idx > -1) {
      nextEntries[idx].enabled = !nextEntries[idx].enabled;
      setEntries(nextEntries);
      syncToDisk(nextEntries);
    }
  };

  const removeEntry = (key: string) => {
    if (confirm('还原此项到原始状态？')) {
      const nextEntries = entries.filter(e => e.key !== key);
      setEntries(nextEntries);
      syncToDisk(nextEntries);
    }
  };

  const confirmModal = () => {
    if (!formData.key) return;
    const nextEntries = [...entries];
    if (editingIndex !== null) {
      nextEntries[editingIndex] = formData;
    } else {
      nextEntries.push(formData);
    }
    setEntries(nextEntries);
    syncToDisk(nextEntries);
    setIsModalOpen(false);
  };

  const openModal = (key: string, modified?: CSCEntry, originValue?: string) => {
    if (modified) {
      setFormData({ ...modified });
      setEditingIndex(entries.findIndex(e => e.key === key));
    } else {
      setFormData({ command: 'MODIFY', key, value: originValue || '', desc: '', enabled: true });
      setEditingIndex(null);
    }
    setIsModalOpen(true);
  };

  return (
    <div className="container">
      <header className="sticky-header">
        <h1>CSC Editor Pro</h1>
        <div className="tabs">
          {FILE_LIST.map(f => (
            <button key={f.name} className={currentFile.name === f.name ? 'active' : ''} onClick={() => setCurrentFile(f)}>
              {f.label}
            </button>
          ))}
        </div>
        <div className="search-bar">
          <input type="text" placeholder="搜索键名或描述..." value={searchTerm} onChange={e => setSearchTerm(e.target.value)} />
        </div>
        <div className="filter-chips">
          {(['ALL', 'ENABLED', 'DISABLED', 'UNSET'] as FilterMode[]).map(mode => (
            <button 
              key={mode} 
              className={filterMode === mode ? 'active' : ''} 
              onClick={() => setFilterMode(mode)}
            >
              {mode === 'ALL' && '全部'}
              {mode === 'ENABLED' && '已开启'}
              {mode === 'DISABLED' && '未开启'}
              {mode === 'UNSET' && '未设定'}
            </button>
          ))}
        </div>
      </header>

      <div className="status-bar">
        <span>{status}</span>
        <span className="count">{filteredList.length} 项</span>
      </div>

      <div className="list">
        {filteredList.map(item => (
          <div key={item.key} className={`item ${item.modified ? 'is-modified' : ''} ${item.modified?.enabled === false ? 'disabled' : ''}`}>
            <div className="info" onClick={() => openModal(item.key, item.modified, item.originValue)}>
              <div className="key">{item.key}</div>
              <div className="compare-view">
                <div className="val-box origin">
                  <span className="dot"></span>
                  <span className="val">{item.originValue || '(未设置)'}</span>
                </div>
                {item.modified && (
                  <div className="val-box mod">
                    <span className="dot"></span>
                    <span className="val">{item.modified.command === 'DELETE' ? '[已删除]' : item.modified.value}</span>
                  </div>
                )}
              </div>
              {item.modified?.desc && <div className="desc">#{item.modified.desc}</div>}
            </div>
            
            <div className="item-actions">
              {item.modified ? (
                <>
                  <label className="switch">
                    <input type="checkbox" checked={item.modified.enabled} onChange={() => toggleEnable(item.key)} />
                    <span className="slider round"></span>
                  </label>
                  <button className="restore-btn" onClick={() => removeEntry(item.key)}>↺</button>
                </>
              ) : (
                <button className="add-quick-btn" onClick={() => openModal(item.key, undefined, item.originValue)}>+</button>
              )}
            </div>
          </div>
        ))}
      </div>

      {isModalOpen && (
        <div className="modal">
          <div className="modal-content">
            <h3>编辑条目</h3>
            <div className="field"><label>键名</label><input value={formData.key} disabled /></div>
            <div className="field">
              <label>操作类型</label>
              <select value={formData.command} onChange={e => setFormData({...formData, command: e.target.value as any})}>
                <option value="MODIFY">MODIFY (修改)</option><option value="DELETE">DELETE (删除)</option>
              </select>
            </div>
            {formData.command === 'MODIFY' && (
              <div className="field"><label>键值</label><input value={formData.value} onChange={e => setFormData({...formData, value: e.target.value})} /></div>
            )}
            <div className="field"><label>备注说明</label><input placeholder="输入功能描述..." value={formData.desc} onChange={e => setFormData({...formData, desc: e.target.value})} /></div>
            <div className="modal-btns">
              <button onClick={() => setIsModalOpen(false)}>取消</button>
              <button className="primary" onClick={confirmModal}>保存修改</button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

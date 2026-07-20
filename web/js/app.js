'use strict';

import { api, toast, $ } from './api.js';
import { loadFilesAndConfig, loadPresets } from './views/config.js';
import { connectStreams, resetMonitor, startPolling, setCurrentTaskId } from './views/monitor.js';
import { loadTasks, openTaskDetail } from './views/history.js';
import { initMonitorCharts } from './charts.js';

document.querySelectorAll('nav button').forEach(btn => {
    btn.onclick = () => switchView(btn.dataset.view);
});

function switchView(name) {
    document.querySelectorAll('nav button').forEach(b =>
        b.classList.toggle('active', b.dataset.view === name));
    document.querySelectorAll('.view').forEach(v =>
        v.classList.toggle('active', v.id === 'view-' + name));
    if (name === 'history') loadTasks();
}

window.switchView = switchView;

window._openTaskDetail = t => openTaskDetail(t);

(async function init() {
    if (typeof window.Chart === 'undefined')
        toast('图表库缺失 (web/vendor/chart.umd.js)，曲线不可用', 6000);
    await loadFilesAndConfig().catch(e => toast('初始化失败: ' + e.message));
    await loadPresets().catch(() => {});
    initMonitorCharts();

    
    const s = await api('/api/status').catch(() => null);
    if (s && (s.state === 'running' || s.state === 'loading')) {
        setCurrentTaskId(s.task_id);
        switchView('monitor');
        connectStreams();
        startPolling();
    }
})();

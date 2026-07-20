'use strict';

import { api, toast, $, apiUrlWithParams, stateNames, friendlyError } from '../api.js';
import { computeAttentionScore, resetAttentionState, AU_MEANINGS } from '../attention.js';
import { initMonitorCharts, updateMonitorCharts, destroyAllCharts, charts } from '../charts.js';
import { refFaces, collectParams } from './config.js';

export let currentTaskId = '';
let metricsBuf = [];
let statusTimer = null, metricsTimer = null;
let lastRunning = false;

export function setCurrentTaskId(id) { currentTaskId = id; }

export function connectStreams() {
    const t = Date.now();
    $('stream').src = apiUrlWithParams('/api/stream', { t });
    $('stream-raw').src = apiUrlWithParams('/api/stream_raw', { t });
}

function csvViewLink(csvPath, label) {
    const name = csvPath.split('/').pop();
    return `<a href="csv.html?file=${encodeURIComponent(csvPath)}&name=${encodeURIComponent(name)}"
              target="_blank" title="在浏览器中查看">📋 ${label}</a>`;
}

function buildResultLinks(t) {
    const links = [];
    const o = t.outputs || {};
    if (o.video) links.push(`<a href="/${o.video}" download>⬇ 标注视频</a>`);
    if (o.csv) {
        links.push(csvViewLink(o.csv, '跟踪日志'));
        links.push(`<a href="/${o.csv}" download>⬇ 下载 CSV</a>`);
    }
    const ofCsv = o.openface_csv || (o.openface ? o.openface + 'target_analysis.csv' : '');
    if (ofCsv) {
        links.push(csvViewLink(ofCsv, 'OpenFace 全量数据'));
        links.push(`<a href="/${ofCsv}" download>⬇ 下载 OpenFace CSV</a>`);
    }
    if (o.onnx_csv) {
        links.push(csvViewLink(o.onnx_csv, 'ONNX 分析数据'));
        links.push(`<a href="/${o.onnx_csv}" download>⬇ 下载 ONNX CSV</a>`);
    }
    if (o.metrics) links.push(`<a href="/${o.metrics}" download>⬇ 指标数据</a>`);
    links.push(`<a href="/api/tasks/${t.id}/zip">⬇ 打包下载全部</a>`);
    return links.join('');
}

export function resetMonitor() {
    $('run-result').style.display = 'none';
    $('stream').removeAttribute('src');
    $('stream-raw').removeAttribute('src');
    destroyAllCharts();
    resetAttentionState();
    metricsBuf = [];
    initMonitorCharts();
    drawTimeline();
}

export function startPolling() {
    clearInterval(statusTimer); clearInterval(metricsTimer);
    statusTimer = setInterval(pollStatus, 1000);
    metricsTimer = setInterval(pollMetrics, 500);
    pollStatus();
}

async function pollStatus() {
    const s = await api('/api/status').catch(() => null);
    if (!s) return;
    const el = $('st-state');
    el.textContent = stateNames[s.state] || s.state;
    el.className = 'tag ' + s.state;
    $('st-frame').textContent = `${s.frame} / ${s.total_frames || '?'}`;
    $('st-fps').textContent = s.fps > 0 ? s.fps.toFixed(1) : '-';
    $('st-target').textContent = s.target_found ? `已锁定 (track ${s.target_track_id})` : '未发现';

    const running = s.state === 'running' || s.state === 'loading';
    lastRunning = s.state === 'running';
    if (running && !$('stream').getAttribute('src')) connectStreams();
    $('btn-pause').disabled = s.state !== 'running';
    $('btn-stop').disabled = !running;
    $('btn-step').disabled = !s.paused;
    $('btn-start').disabled = running;
    if (!s.paused) $('btn-pause').textContent = '暂停';

    if (s.state === 'finished' && currentTaskId) {
        clearInterval(statusTimer); clearInterval(metricsTimer);
        await pollMetrics();
        drawTimeline();
        showRunResult(currentTaskId);
    } else if (s.state === 'error') {
        clearInterval(statusTimer); clearInterval(metricsTimer);
        toast(friendlyError(s.error), 8000);
    }
}

async function pollMetrics() {
    const inc = await api('/api/metrics?since=' + metricsBuf.length).catch(() => []);
    if (!inc.length) return;
    metricsBuf.push(...inc);

    const latest = inc[inc.length - 1];
    updatePanel(latest);
    updateAuPanel(latest);

    
    updateMonitorCharts(inc, computeAttentionScore, (attn, _latest) => {
        if (!attn) attn = { score: 0, level: 'none', gazeDesc: '--', headDesc: '--' };
        const se = $('m-attn-score');
        if (se) {
            se.textContent = attn.score + '%';
            se.style.color = attn.score >= 65 ? 'var(--green)' : attn.score >= 40 ? '#d97706' : 'var(--red)';
        }
        const gd = $('m-gaze-dir'); if (gd) gd.textContent = attn.gazeDesc;
        const hd = $('m-head-dir'); if (hd) hd.textContent = attn.headDesc;
        const si = $('m-sim'); if (si) si.textContent = _latest ? _latest.sim.toFixed(2) : '--';
        const sa = $('st-attention');
        if (sa) sa.textContent = attn.level === 'focused' ? '集中' : attn.level === 'partial' ? '部分' : attn.level === 'none' ? '--' : '分散';
    });

    drawTimeline();
}

function updatePanel(m) {
    $('m-tracks').innerHTML = (m.tracks || []).map(([id, sim, isT]) => `
        <tr class="${isT ? 'target' : ''}"><td>${id}</td><td>${sim.toFixed(3)}</td>
        <td>${isT ? '★ 目标' : ''}</td></tr>`).join('') ||
        '<tr><td colspan="3" style="color:#9ca3af">本帧无人脸</td></tr>';
}

function updateAuPanel(latest) {
    if (!latest || !latest.aus || !latest.aus.length) return;

    
    const active = latest.aus.filter(([, v]) => v >= 0.45).sort((a, b) => b[1] - a[1]);
    const wrap = $('m-au-active');
    if (wrap) {
        if (active.length) {
            wrap.innerHTML = active.map(([name, v]) => {
                const desc = AU_MEANINGS[name] || '';
                return `<div class="au-card"><span class="au-name">${name}</span><span class="au-desc">${desc}</span><span class="au-val">${v.toFixed(2)}</span></div>`;
            }).join('');
        } else {
            wrap.innerHTML = '<div class="hint">当前无明显 AU 激活</div>';
        }
    }

    
    if (charts.auHeatmap) {
        const hm = charts.auHeatmap;
        hm.data.labels = latest.aus.map(([n]) => n);
        hm.data.datasets[0].data = latest.aus.map(([, v]) => v);
        hm.data.datasets[0].backgroundColor = latest.aus.map(([, v]) =>
            v >= 0.5 ? `rgba(37,99,235,${0.3 + v * 0.7})` : 'rgba(156,163,175,0.2)');
        hm.update('none');
    }
}

export function drawTimeline() {
    const canvas = $('timeline');
    const total = Math.max(metricsBuf.length, 1);
    const dpr = window.devicePixelRatio || 1;
    const cssW = canvas.clientWidth || 800, cssH = 24;
    canvas.width = cssW * dpr;
    canvas.height = cssH * dpr;
    const ctx = canvas.getContext('2d');
    ctx.scale(dpr, dpr);
    ctx.fillStyle = '#e5e7eb';
    ctx.fillRect(0, 0, cssW, cssH);
    const w = Math.max(cssW / total, 0.5);
    metricsBuf.forEach((m, i) => {
        if (!m.found) return;
        const attn = m._attn || computeAttentionScore(m);
        if (attn.score >= 65) ctx.fillStyle = '#16a34a';
        else if (attn.score >= 40) ctx.fillStyle = '#f59e0b';
        else ctx.fillStyle = '#ef4444';
        ctx.fillRect(i * w, 0, w, cssH);
    });
    ctx.fillStyle = '#2563eb';
    ctx.fillRect(metricsBuf.length * w - 1, 0, 2, cssH);
}

async function showRunResult(taskId) {
    const tasks = await api('/api/tasks');
    const t = tasks.find(x => x.id === taskId);
    if (!t) return;
    $('run-result').style.display = 'block';
    $('run-result-links').innerHTML = buildResultLinks(t);
    $('btn-view-report').onclick = () => {
        
        document.querySelectorAll('nav button').forEach(b =>
            b.classList.toggle('active', b.dataset.view === 'history'));
        document.querySelectorAll('.view').forEach(v =>
            v.classList.toggle('active', v.id === 'view-history'));
        window._openTaskDetail(t);
    };
    toast(`任务完成: 处理 ${t.stats.frames} 帧, 目标出现 ${t.stats.target_frames} 帧`);
}

$('btn-start').onclick = async () => {
    if (refFaces.length === 0) { toast('参考图片中没有检测到人脸，无法开始'); return; }
    const btn = $('btn-start');
    btn.disabled = true;
    try {
        const params = collectParams();
        const resumeId = btn.dataset.resumeTask;
        if (resumeId) {
            params.resume_task = resumeId;
            delete btn.dataset.resumeTask;
            btn.textContent = '开始分析';
        }
        const r = await api('/api/start', { method: 'POST', body: new URLSearchParams(params) });
        currentTaskId = r.task_id;
        metricsBuf = [];
        resetMonitor();
        window.switchView('monitor');
        connectStreams();
        startPolling();
    } catch (e) { toast(e.message); }
    btn.disabled = false;
};

$('btn-pause').onclick = async () => {
    try {
        const r = await api('/api/pause', { method: 'POST' });
        $('btn-pause').textContent = r.paused ? '继续' : '暂停';
        $('btn-step').disabled = !r.paused;
    } catch (e) { toast(e.message); }
};
$('btn-step').onclick = () => api('/api/step', { method: 'POST' }).catch(e => toast(e.message));
$('btn-stop').onclick = () => api('/api/stop', { method: 'POST' }).catch(e => toast(e.message));

for (const id of ['stream', 'stream-raw']) {
    $(id).onerror = () => {
        if (lastRunning && $(id).getAttribute('src')) {
            setTimeout(() => { if (lastRunning) connectStreams(); }, 1000);
        }
    };
}

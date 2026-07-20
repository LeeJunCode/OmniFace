'use strict';

import { api, toast, $ } from '../api.js';
import { computeAttentionScore, AU_MEANINGS } from '../attention.js';
import { chartOpts } from '../charts.js';
import { applyParamsToForm } from './config.js';

let reportCharts = {};

export async function loadTasks() {
    const tasks = await api('/api/tasks').catch(() => []);
    $('task-detail').style.display = 'none';
    $('task-list-wrap').style.display = 'block';
    $('task-empty').style.display = tasks.length ? 'none' : 'block';
    $('task-list').innerHTML = tasks.map(t => {
        const ratio = t.stats.frames > 0 ?
            (100 * t.stats.target_frames / t.stats.frames).toFixed(1) : 0;
        const time = t.id.replace('_', ' ').replace(/(\d{4})(\d{2})(\d{2})/, '$1-$2-$3');
        return `<div class="task-card">
            <h4>${time} <span class="tag ${t.state === 'finished' ? 'finished' : t.state === 'error' ? 'error' : 'idle'}">${t.state}</span></h4>
            <div class="meta">
                后端: ${t.backend} | 参考: ${(t.reference || '').split('/').pop()}<br>
                视频: ${(t.video || '').split('/').pop()}<br>
                ${t.stats.frames} 帧 · 目标出现 ${t.stats.target_frames} 帧 (${ratio}%) · 最高相似度 ${t.stats.max_sim.toFixed(2)}
            </div>
            <div class="ops">
                <button class="small" onclick='window._openTaskDetailById("${t.id}")'>查看报告</button>
                ${t.state === 'stopped' ? `<button class="small" onclick='window._resumeTask("${t.id}")'>续传</button>` : ''}
                <button class="small" onclick='window._rerunTask("${t.id}")'>重跑</button>
                <button class="small danger" onclick='window._deleteTask("${t.id}")'>删除</button>
            </div>
        </div>`;
    }).join('');
    window._tasks = tasks;
}

window._openTaskDetailById = id => {
    const t = (window._tasks || []).find(x => x.id === id);
    if (t) openTaskDetail(t);
};
window._openTaskDetail = t => openTaskDetail(t);

window._resumeTask = async id => {
    const t = (window._tasks || []).find(x => x.id === id);
    if (!t || t.state !== 'stopped') return;
    try {
        if (t.params) {
            for (const [k, v] of Object.entries(t.params))
                if (k !== 'save_video' && k !== 'save_csv') applyParamValue(k, v);
        }
        window.switchView('config');
        $('btn-start').dataset.resumeTask = id;
        $('btn-start').textContent = '续传分析';
        toast(`已加载任务 ${id} 的配置, 点击「续传分析」从第 ${t.stats.frames} 帧继续`);
    } catch (e) { toast(e.message); }
};

function applyParamValue(k, v) {
    const el = document.getElementById(k);
    if (el) {
        if (el.type === 'checkbox') el.checked = (v === 'true');
        else if (el.type === 'radio') {
            const r = document.querySelector(`[name="${el.name}"][value="${v}"]`);
            if (r) r.checked = true;
        }
        else el.value = v;
    }
}

window._rerunTask = id => {
    const t = (window._tasks || []).find(x => x.id === id);
    if (!t) return;
    applyParamsToForm(t.params || {});
    window.switchView('config');
    toast('已回填该任务的参数，确认后点「开始分析」');
};

window._deleteTask = async id => {
    if (!confirm('删除该任务及其全部输出？')) return;
    try { await api('/api/tasks/' + id, { method: 'DELETE' }); loadTasks(); }
    catch (e) { toast(e.message); }
};

$('btn-back-list').onclick = loadTasks;

export async function openTaskDetail(t) {
    $('task-list-wrap').style.display = 'none';
    $('task-detail').style.display = 'block';
    $('detail-title').textContent = `任务 ${t.id} (${t.backend})`;

    const s = t.stats;
    const ratio = s.frames > 0 ? (100 * s.target_frames / s.frames).toFixed(1) + '%' : '-';
    $('detail-stats').innerHTML = `
        <div class="stat-tile"><div class="v">${s.frames}</div><div class="k">处理帧数</div></div>
        <div class="stat-tile"><div class="v">${ratio}</div><div class="k">目标出现率</div></div>
        <div class="stat-tile"><div class="v">${s.avg_sim.toFixed(3)}</div><div class="k">平均相似度</div></div>
        <div class="stat-tile"><div class="v">${s.max_sim.toFixed(3)}</div><div class="k">最高相似度</div></div>`;
    $('detail-links').innerHTML = buildResultLinks(t);
    $('detail-intervals').textContent = (s.intervals || []).length ?
        s.intervals.map(([a, b]) => `${a}-${b}`).join('，') : '目标从未出现';

    
    const vw = $('detail-video-wrap');
    const rawVid = t.video || '';
    const annoVid = t.outputs?.video || '';
    const makePlayer = (src, label) => src.endsWith('.mp4')
        ? `<div class="detail-vid-col"><span class="video-label">${label}</span>
           <div class="video-player playback-wrap"><video class="playback" controls src="/${src}"></video></div>
           <div class="frame-indicator"><span class="frame-label">帧: </span><span class="frame-num">--</span></div></div>`
        : (src ? `<div class="detail-vid-col"><span class="video-label">${label}</span><div class="hint">格式不支持播放</div></div>` : '');
    vw.innerHTML = makePlayer(rawVid, '原始视频') + makePlayer(annoVid, '分析画面');

    
    window._detailMetrics = null;
    window._detailFps = t.stats.fps || 30;
    try {
        const resp = await fetch('/' + t.outputs.metrics);
        const text = await resp.text();
        const metrics = text.trim().split('\n').filter(Boolean).map(l => JSON.parse(l));
        window._detailMetrics = metrics;
        renderReportCharts(metrics, t.backend);
    } catch { toast('指标数据缺失，无法生成图表'); }

    
    setTimeout(() => {
        const videos = vw.querySelectorAll('video.playback');
        const frameNums = vw.querySelectorAll('.frame-num');
        videos.forEach(v => {
            v.addEventListener('timeupdate', () => {
                const f = Math.round(v.currentTime * window._detailFps);
                frameNums.forEach(el => { el.textContent = f; });
            });
        });
    }, 300);
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

function renderReportCharts(metrics, backend) {
    if (typeof window.Chart === 'undefined') return;
    for (const c of Object.values(reportCharts)) c?.destroy?.();
    reportCharts = {};

    
    for (const m of metrics) { m._attn = computeAttentionScore(m); }

    
    const targetMetrics = metrics.filter(m => m.found);
    const attnScores = targetMetrics.map(m => m._attn.score);
    const avgAttn = attnScores.length ? Math.round(attnScores.reduce((a, b) => a + b, 0) / attnScores.length) : 0;
    const focusedPct = attnScores.length ? Math.round(100 * attnScores.filter(s => s >= 65).length / attnScores.length) : 0;
    const distractedPct = attnScores.length ? Math.round(100 * attnScores.filter(s => s < 40).length / attnScores.length) : 0;

    const auFreq = {}; const auThresh = backend === 'onnx' ? 0.5 : 1.0;
    for (const m of targetMetrics) if (m.aus) for (const [n, v] of m.aus) if (v >= auThresh) auFreq[n] = (auFreq[n] || 0) + 1;
    const topAu = Object.entries(auFreq).sort((a, b) => b[1] - a[1])[0] || ['--', 0];

    $('detail-summary').innerHTML = `
        <div class="summary-tile"><div class="v green">${avgAttn}%</div><div class="k">平均注意力评分</div></div>
        <div class="summary-tile"><div class="v accent">${focusedPct}%</div><div class="k">注意力集中占比</div></div>
        <div class="summary-tile"><div class="v accent">${topAu[1]} 帧</div><div class="k">最活跃 AU: ${topAu[0]} ${AU_MEANINGS[topAu[0]] || ''}</div></div>
        <div class="summary-tile"><div class="v amber">${distractedPct}%</div><div class="k">注意力分散占比</div></div>`;

    
    const step = Math.max(1, Math.floor(metrics.length / 800));
    const ds = metrics.filter((_, i) => i % step === 0);
    const labels = ds.map(m => m.f);

    
    const aCtx = $('rchart-attention')?.getContext('2d');
    if (aCtx) {
        reportCharts.attention = new window.Chart(aCtx, {
            type: 'line',
            data: {
                labels, datasets: [{
                    label: '注意力评分', data: ds.map(m => m.found ? m._attn.score : null),
                    borderColor: '#2563eb', borderWidth: 1.5, fill: true,
                    backgroundColor: 'rgba(37,99,235,0.08)', pointRadius: 0, tension: 0.3
                }]
            },
            options: Object.assign(chartOpts('注意力评分 (0-100) — 点击跳转视频'), {
                scales: { y: { min: 0, max: 100, ticks: { font: { size: 9 }, callback: function (v) { return v >= 65 ? 'focused' : v >= 40 ? 'partial' : 'distracted'; } } } },
            })
        });
        aCtx.canvas.addEventListener('click', function (e) {
            const rect = this.getBoundingClientRect();
            const x = e.clientX - rect.left;
            const chart = reportCharts.attention;
            if (!chart || !window._detailFps) return;
            const frameIdx = Math.round(chart.scales.x.getValueForPixel(x));
            if (frameIdx >= 0 && frameIdx < labels.length) {
                const frame = labels[frameIdx];
                const time = frame / window._detailFps;
                document.querySelectorAll('video.playback').forEach(function (v) { v.currentTime = time; });
            }
        });
    }

    
    const topAus = Object.entries(auFreq).sort((a, b) => b[1] - a[1]).slice(0, 6).map(x => x[0]);
    const auColors = ['#ef4444', '#f59e0b', '#16a34a', '#2563eb', '#8b5cf6', '#ec4899'];
    const auCtx = $('rchart-au-timeline')?.getContext('2d');
    if (auCtx && topAus.length) {
        reportCharts.auTimeline = new window.Chart(auCtx, {
            type: 'line',
            data: {
                labels, datasets: topAus.map((name, i) => ({
                    label: name, data: ds.map(m => m.aus ? (Object.fromEntries(m.aus)[name] ?? null) : null),
                    borderColor: auColors[i % 6], borderWidth: 1, pointRadius: 0, tension: 0.2
                }))
            },
            options: Object.assign(chartOpts('AU 激活强度时间线'), { plugins: { legend: { labels: { boxWidth: 8, font: { size: 10 } } } } })
        });
    }

    
    const rCtxs = { pose: $('rchart-pose'), gaze: $('rchart-gaze'), sim: $('rchart-sim') };
    if (rCtxs.pose?.getContext('2d')) reportCharts.pose = new window.Chart(rCtxs.pose.getContext('2d'), {
        type: 'line',
        data: {
            labels, datasets: [
                { label: 'Pitch', data: ds.map(m => m.pose_valid ? m.pose[0] : null), borderColor: '#ef4444', borderWidth: 1 },
                { label: 'Yaw', data: ds.map(m => m.pose_valid ? m.pose[1] : null), borderColor: '#16a34a', borderWidth: 1 },
                { label: 'Roll', data: ds.map(m => m.pose_valid ? m.pose[2] : null), borderColor: '#f59e0b', borderWidth: 1 }
            ]
        }, options: chartOpts('头部姿态 (度)')
    });
    if (rCtxs.gaze?.getContext('2d')) reportCharts.gaze = new window.Chart(rCtxs.gaze.getContext('2d'), {
        type: 'line',
        data: {
            labels, datasets: [
                { label: '水平', data: ds.map(m => m.gaze_valid ? m.gaze[0] : null), borderColor: '#7c3aed', borderWidth: 1 },
                { label: '垂直', data: ds.map(m => m.gaze_valid ? m.gaze[1] : null), borderColor: '#0891b2', borderWidth: 1 }
            ]
        }, options: chartOpts('视线角度 (弧度)')
    });
    if (rCtxs.sim?.getContext('2d')) reportCharts.sim = new window.Chart(rCtxs.sim.getContext('2d'), {
        type: 'line',
        data: { labels, datasets: [{ label: '相似度', data: ds.map(m => m.found ? m.sim : null), borderColor: '#2563eb', borderWidth: 1 }] },
        options: chartOpts('目标相似度 (全程)')
    });
}

'use strict';

const charts = {};

export function chartOpts(title) {
    return {
        responsive: true, maintainAspectRatio: false, animation: false,
        devicePixelRatio: window.devicePixelRatio || 2,
        plugins: {
            title: { display: true, text: title, font: { size: 12 } },
            legend: { labels: { boxWidth: 10, font: { size: 10 } } }
        },
        scales: {
            x: { ticks: { maxTicksLimit: 6, font: { size: 10 } } },
            y: { ticks: { font: { size: 10 } } }
        },
        elements: { point: { radius: 0 } },
    };
}

export function destroyAllCharts() {
    for (const c of Object.values(charts)) c?.destroy?.();
    for (const k of Object.keys(charts)) delete charts[k];
}

export function initMonitorCharts() {
    if (typeof window.Chart === 'undefined') return;
    try {
        
        const sCtx = document.getElementById('chart-gaze-scatter')?.getContext('2d');
        if (sCtx) {
            charts.gazeScatter = new window.Chart(sCtx, {
                type: 'scatter',
                data: {
                    datasets: [
                        { label: '集中', data: [], backgroundColor: 'rgba(22,163,74,0.5)', pointRadius: 2 },
                        { label: '分散', data: [], backgroundColor: 'rgba(239,68,68,0.3)', pointRadius: 2 }
                    ]
                },
                options: {
                    responsive: true, maintainAspectRatio: false, animation: false,
                    plugins: { legend: { labels: { boxWidth: 8, font: { size: 10 } } } },
                    scales: {
                        x: { title: { text: '水平(度)', display: true }, min: -35, max: 35, ticks: { font: { size: 9 } } },
                        y: { title: { text: '垂直(度)', display: true }, min: -35, max: 35, ticks: { font: { size: 9 } } }
                    }
                }
            });
        }

        
        const aCtx = document.getElementById('chart-au-heatmap')?.getContext('2d');
        if (aCtx) {
            charts.auHeatmap = new window.Chart(aCtx, {
                type: 'bar',
                data: { labels: [], datasets: [{ label: '强度', data: [], backgroundColor: [] }] },
                options: {
                    indexAxis: 'y', responsive: true, maintainAspectRatio: false, animation: false,
                    plugins: { legend: { display: false }, tooltip: { callbacks: { label: ctx => ctx.raw.toFixed(2) } } },
                    scales: { x: { min: 0, max: 1, ticks: { font: { size: 9 } } }, y: { ticks: { font: { size: 9 } } } }
                }
            });
        }

        
        const lineOpts = (title) => ({
            responsive: true, maintainAspectRatio: false, animation: false,
            plugins: { title: { display: true, text: title, font: { size: 11 } }, legend: { labels: { boxWidth: 8, font: { size: 9 } } } },
            scales: { x: { ticks: { maxTicksLimit: 5, font: { size: 9 } } }, y: { ticks: { font: { size: 9 } } } },
            elements: { point: { radius: 0 } }
        });

        const rpCtx = document.getElementById('chart-raw-pose')?.getContext('2d');
        if (rpCtx) charts.rawPose = new window.Chart(rpCtx, { type: 'line', data: { labels: [], datasets: [
            { label: 'Pitch', data: [], borderColor: '#ef4444', borderWidth: 1 },
            { label: 'Yaw', data: [], borderColor: '#16a34a', borderWidth: 1 },
            { label: 'Roll', data: [], borderColor: '#f59e0b', borderWidth: 1 }
        ]}, options: lineOpts('头部姿态 (度)') });

        const rgCtx = document.getElementById('chart-raw-gaze')?.getContext('2d');
        if (rgCtx) charts.rawGaze = new window.Chart(rgCtx, { type: 'line', data: { labels: [], datasets: [
            { label: '水平', data: [], borderColor: '#7c3aed', borderWidth: 1 },
            { label: '垂直', data: [], borderColor: '#0891b2', borderWidth: 1 }
        ]}, options: lineOpts('视线角度 (弧度)') });

        const raCtx = document.getElementById('chart-raw-au')?.getContext('2d');
        if (raCtx) charts.rawAu = new window.Chart(raCtx, { type: 'line', data: { labels: [], datasets: [] },
            options: Object.assign(lineOpts('AU 强度'), { plugins: { legend: { display: false } } })
        });
    } catch (e) { console.warn('图表初始化失败:', e); }
}

export function pushPoint(chart, label, values, maxLen = 300) {
    if (!chart) return;
    chart.data.labels.push(label);
    chart.data.datasets.forEach((d, i) => d.data.push(values[i]));
    while (chart.data.labels.length > maxLen) {
        chart.data.labels.shift();
        chart.data.datasets.forEach(d => d.data.shift());
    }
}

export function updateMonitorCharts(newMetrics, attentionFn, onAttnReady) {
    if (typeof window.Chart === 'undefined') return;
    
    for (const m of newMetrics) {
        m._attn = attentionFn(m);
    }
    const latest = newMetrics.length ? newMetrics[newMetrics.length - 1] : null;

    
    let lastAttn = null;
    for (let i = newMetrics.length - 1; i >= 0; i--) {
        if (newMetrics[i].found) { lastAttn = newMetrics[i]._attn; break; }
    }
    if (onAttnReady) onAttnReady(lastAttn, latest);

    
    if (charts.gazeScatter) {
        for (const m of newMetrics) {
            if (m.gaze_valid) {
                const ds = charts.gazeScatter.data.datasets;
                const focused = m._attn && m._attn.score >= 40;
                ds[focused ? 0 : 1].data.push({ x: m.gaze[0] * 180 / Math.PI, y: m.gaze[1] * 180 / Math.PI });
                for (const d of ds) if (d.data.length > 300) d.data.shift();
            }
        }
        charts.gazeScatter.update('none');
    }

    
    const W = 150;
    for (const m of newMetrics) {
        if (charts.rawPose) pushPoint(charts.rawPose, m.f, m.pose_valid ? m.pose : [null, null, null], W);
        if (charts.rawGaze) pushPoint(charts.rawGaze, m.f, m.gaze_valid ? m.gaze : [null, null], W);
        if (charts.rawAu && m.aus && m.aus.length) {
            if (!charts.rawAu.data.datasets.length) {
                charts.rawAu.data.datasets = m.aus.slice(0, 8).map(([name], i) => (
                    { label: name, data: [], borderColor: `hsl(${i * 45},60%,45%)`, borderWidth: 1, pointRadius: 0 }));
            }
            const auMap = Object.fromEntries(m.aus);
            pushPoint(charts.rawAu, m.f, charts.rawAu.data.datasets.map(d => auMap[d.label] ?? null), W);
        }
    }
    for (const c of [charts.rawPose, charts.rawGaze, charts.rawAu]) c?.update('none');
}

export { charts };

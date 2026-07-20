'use strict';

import { api, toast, $, apiUrlWithParams } from '../api.js';

export let refFaces = [];          
export let refFaceIndex = 0;       
let refImageObj = null;

function fillSelect(sel, items, current) {
    sel.innerHTML = '';
    for (const f of items || []) {
        const opt = document.createElement('option');
        opt.value = f;
        opt.textContent = f.split('/').pop();
        if (f === current) opt.selected = true;
        sel.appendChild(opt);
    }
}

function updateBackendUI() {
    const onnx = document.querySelector('input[name=backend]:checked').value === 'onnx';
    $('onnx-opts').style.display = onnx ? 'block' : 'none';
}

async function detectRefFaces() {
    const path = $('sel-ref').value;
    const canvas = $('ref-canvas'), hint = $('face-hint');
    refFaces = []; refFaceIndex = 0;
    if (!path) { hint.textContent = '请先选择参考图片'; hint.className = 'hint warn'; return; }

    refImageObj = new Image();
    refImageObj.onload = async () => {
        try {
            const det = await api('/api/detect_face', {
                method: 'POST',
                body: new URLSearchParams({ image: path }),
            });
            refFaces = det.faces || [];
            drawRefCanvas();
            if (refFaces.length === 0) {
                hint.textContent = '⚠ 未检测到人脸，请更换正面清晰照片';
                hint.className = 'hint warn';
            } else if (refFaces.length === 1) {
                hint.textContent = `✓ 检测到 1 张人脸 (置信度 ${refFaces[0].confidence.toFixed(2)})`;
                hint.className = 'hint ok';
            } else {
                hint.textContent = `检测到 ${refFaces.length} 张人脸 — 请点击选择目标人物`;
                hint.className = 'hint warn';
            }
        } catch (e) { hint.textContent = '人脸检测失败: ' + e.message; hint.className = 'hint warn'; }
    };
    refImageObj.src = '/' + path;
}

function drawRefCanvas() {
    const canvas = $('ref-canvas');
    if (!refImageObj) return;
    canvas.width = refImageObj.naturalWidth;
    canvas.height = refImageObj.naturalHeight;
    const ctx = canvas.getContext('2d');
    ctx.drawImage(refImageObj, 0, 0);
    refFaces.forEach((f, i) => {
        const sel = i === refFaceIndex;
        ctx.strokeStyle = sel ? '#16a34a' : '#f59e0b';
        ctx.lineWidth = Math.max(2, canvas.width / 250);
        ctx.strokeRect(f.x1, f.y1, f.x2 - f.x1, f.y2 - f.y1);
        ctx.fillStyle = ctx.strokeStyle;
        ctx.font = `${Math.max(14, canvas.width / 40)}px sans-serif`;
        ctx.fillText(sel ? '✓ 目标' : `#${i + 1}`, f.x1 + 4, Math.max(20, f.y1 - 6));
    });
}

function updateVideoPreview() {
    const path = $('sel-video').value;
    const wrap = $('video-preview-wrap'), video = $('video-preview'), hint = $('video-hint');
    hint.textContent = '';
    if (!path) { wrap.style.display = 'none'; return; }
    wrap.style.display = 'block';
    video.onerror = () => {
        wrap.style.display = 'none';
        hint.textContent = '该格式浏览器无法预览（不影响分析）';
        hint.className = 'hint';
    };
    video.src = '/' + path;
}

function uploadFile(type) {
    const input = $('file-input');
    input.accept = type === 'img' ? 'image/*' : 'video/*';
    input.onchange = async () => {
        if (!input.files.length) return;
        const fd = new FormData();
        fd.append('file', input.files[0]);
        fd.append('type', type);
        try {
            const otherVal = (type === 'img') ? $('sel-video').value : $('sel-ref').value;
            const r = await api('/api/upload', { method: 'POST', body: fd });
            const files = await api('/api/files');
            if (type === 'img') {
                fillSelect($('sel-ref'), files.images, r.path);
                $('sel-video').value = otherVal;
                detectRefFaces();
            } else {
                fillSelect($('sel-video'), files.videos, r.path);
                $('sel-ref').value = otherVal;
                updateVideoPreview();
            }
            toast('上传成功');
        } catch (e) { toast('上传失败: ' + e.message); }
        input.value = '';
    };
    input.click();
}

async function deleteFile(path) {
    if (!path || !confirm(`确认删除 ${path}？`)) return;
    await api('/api/files?path=' + encodeURIComponent(path), { method: 'DELETE' });
    toast('已删除');
    loadFilesAndConfig();
}

export async function loadPresets() {
    const presets = await api('/api/presets');
    const sel = $('sel-preset');
    sel.innerHTML = '<option value="">— 不使用预设 —</option>';
    for (const name of Object.keys(presets)) {
        const opt = document.createElement('option');
        opt.value = name; opt.textContent = name;
        sel.appendChild(opt);
    }
    sel._presets = presets;
}

const numParams = ['match_threshold', 'detection_confidence', 'detection_nms', 'recognition_iou',
    'track_buffer', 'tracking_high', 'tracking_match', 'tracking_match_second', 'tracking_new',
    'pose_margin', 'pose_score_threshold', 'gaze_margin', 'gaze_smooth_alpha', 'au_margin', 'au_threshold'];
const boolParams = ['enable_pose', 'enable_gaze', 'enable_au', 'save_video', 'save_csv'];

export function collectParams() {
    const p = {
        reference: $('sel-ref').value,
        video: $('sel-video').value,
        backend: document.querySelector('input[name=backend]:checked').value,
        reference_face_index: refFaceIndex,
    };
    for (const k of numParams) p[k] = $(k).value;
    for (const k of boolParams) p[k] = $(k).checked;
    return p;
}

export function applyParamsToForm(p) {
    if (p.reference) $('sel-ref').value = p.reference;
    if (p.video) $('sel-video').value = p.video;
    if (p.backend) document.querySelector(`input[name=backend][value=${p.backend}]`).checked = true;
    for (const k of numParams) if (k in p && $(k)) $(k).value = p[k];
    for (const k of boolParams) if (k in p && $(k)) $(k).checked = (p[k] === true || p[k] === 'true');
    $('thr-val').textContent = $('match_threshold').value;
    updateBackendUI();
    if (p.reference) detectRefFaces();
}

export async function loadFilesAndConfig() {
    const [files, cfg] = await Promise.all([api('/api/files'), api('/api/config')]);
    fillSelect($('sel-ref'), files.images, cfg.reference);
    fillSelect($('sel-video'), files.videos, cfg.video);
    document.querySelector(`input[name=backend][value=${cfg.backend}]`).checked = true;
    for (const key of ['enable_pose', 'enable_gaze', 'enable_au', 'save_video', 'save_csv']) {
        if ($(key) && key in cfg) $(key).checked = cfg[key];
    }
    for (const key of numParams) {
        if ($(key) && key in cfg) $(key).value = cfg[key];
    }
    $('thr-val').textContent = cfg.match_threshold;
    updateBackendUI();
    detectRefFaces();
    updateVideoPreview();
}

document.querySelectorAll('input[name=backend]').forEach(r => r.onchange = updateBackendUI);
$('match_threshold').oninput = () => $('thr-val').textContent = $('match_threshold').value;
$('sel-ref').onchange = detectRefFaces;
$('sel-video').onchange = updateVideoPreview;
$('ref-canvas').onclick = e => {
    if (refFaces.length < 2) return;
    const canvas = $('ref-canvas');
    const rect = canvas.getBoundingClientRect();
    const x = (e.clientX - rect.left) * canvas.width / rect.width;
    const y = (e.clientY - rect.top) * canvas.height / rect.height;
    refFaces.forEach((f, i) => {
        if (x >= f.x1 && x <= f.x2 && y >= f.y1 && y <= f.y2) {
            refFaceIndex = i;
            drawRefCanvas();
            $('face-hint').textContent = `✓ 已选择第 ${i + 1} 张人脸作为目标`;
            $('face-hint').className = 'hint ok';
        }
    });
};
$('btn-upload-img').onclick = () => uploadFile('img');
$('btn-upload-video').onclick = () => uploadFile('video');
$('btn-del-img').onclick = () => deleteFile($('sel-ref').value);
$('btn-del-video').onclick = () => deleteFile($('sel-video').value);
$('sel-preset').onchange = () => {
    const p = $('sel-preset')._presets?.[$('sel-preset').value];
    if (p) { applyParamsToForm(p); toast('已应用预设'); }
};
$('btn-save-preset').onclick = async () => {
    const name = prompt('预设名称:');
    if (!name) return;
    const params = collectParams();
    params.name = name;
    await api('/api/presets', { method: 'POST', body: new URLSearchParams(params) });
    toast('预设已保存');
    loadPresets();
};
$('btn-del-preset').onclick = async () => {
    const name = $('sel-preset').value;
    if (!name || !confirm(`删除预设「${name}」？`)) return;
    await api('/api/presets?name=' + encodeURIComponent(name), { method: 'DELETE' });
    loadPresets();
};

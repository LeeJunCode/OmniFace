'use strict';

const urlParams = new URLSearchParams(window.location.search);
const AUTH_TOKEN = urlParams.get('token') || '';

export function apiUrl(path) {
    if (!AUTH_TOKEN) return path;
    const sep = path.includes('?') ? '&' : '?';
    return path + sep + 'token=' + encodeURIComponent(AUTH_TOKEN);
}

export function apiUrlWithParams(path, params = {}) {
    if (AUTH_TOKEN) params.token = AUTH_TOKEN;
    const qs = Object.entries(params)
        .map(([k, v]) => encodeURIComponent(k) + '=' + encodeURIComponent(v))
        .join('&');
    return qs ? path + '?' + qs : path;
}

export const $ = id => document.getElementById(id);

export const stateNames = {
    idle: '空闲', loading: '加载模型...', running: '分析中',
    finished: '已完成', error: '出错'
};

const errorHints = {
    MODEL_LOAD_FAIL: '模型加载失败 — 请确认 models/ 目录下模型文件完整',
    REF_LOAD_FAIL:   '无法读取参考图片 — 文件可能已被删除或损坏',
    REF_NO_FACE:     '参考图片中未检测到人脸 — 请换一张正面、清晰、光线充足的照片',
    REF_ALIGN_FAIL:  '人脸对齐失败 — 照片中人脸角度过大，请换正面照',
    REF_EMBED_FAIL:  '特征提取失败 — 请重试或更换照片',
    VIDEO_OPEN_FAIL: '视频无法打开 — 请确认格式为 mp4/avi/mkv 且文件完整',
    RUNTIME:         '运行时错误',
    UNKNOWN:         '未知错误',
};

export function friendlyError(raw) {
    if (!raw) return '';
    const [code, detail] = raw.split('|');
    return (errorHints[code] || code) + (detail ? `（${detail}）` : '');
}

export function toast(msg, ms = 4000) {
    const t = $('toast');
    t.textContent = msg;
    t.style.display = 'block';
    clearTimeout(t._timer);
    t._timer = setTimeout(() => t.style.display = 'none', ms);
}

export async function api(path, opts = {}) {
    const ctrl = new AbortController();
    const timer = setTimeout(() => ctrl.abort(), 8000);
    try {
        const res = await fetch(apiUrl(path), { ...opts, signal: ctrl.signal });
        const data = await res.json().catch(() => ({}));
        if (!res.ok) {
            if (res.status === 413)
                throw new Error('文件过大, 请调高 config.yaml 中 server.max_upload_mb 的值');
            throw new Error(data.error || `请求失败 (${res.status})`);
        }
        return data;
    } catch (e) {
        if (e.name === 'AbortError') throw new Error('请求超时，服务可能无响应');
        throw e;
    } finally {
        clearTimeout(timer);
    }
}

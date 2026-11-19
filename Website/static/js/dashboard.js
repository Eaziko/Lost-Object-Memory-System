
// ==========================================
// LOST OBJECT MEMORY SYSTEM - DASHBOARD JS
// ==========================================

let lastNotifiedDetectionId = null;
let lastRenderedHistorySignature = null;
let toastTimeout;
const numberAnimationFrames = new Map();

async function fetchJSON(url) {
    const response = await fetch(url, {
        cache: "no-store"
    });

    if (!response.ok) {
        throw new Error(`Request failed: ${url} (${response.status})`);
    }

    return response.json();
}

// Display text inside HTML elements
function setText(id, value) {
    const element = document.getElementById(id);

    if (element) {
        element.textContent = value ?? "--";
    }
}

function cancelNumberAnimation(id) {
    const frame = numberAnimationFrames.get(id);

    if (frame !== undefined) {
        window.cancelAnimationFrame(frame);
        numberAnimationFrames.delete(id);
    }
}

function replayValueAnimation(element) {
    if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) return;

    element.classList.remove("value-updated");
    void element.offsetWidth;
    element.classList.add("value-updated");
}

function setAnimatedText(id, value) {
    const element = document.getElementById(id);

    if (!element) return;

    cancelNumberAnimation(id);
    const text = value ?? "--";
    if (element.textContent === String(text)) return;

    element.textContent = text;
    replayValueAnimation(element);
}

function setAnimatedNumber(id, targetValue, formatter) {
    const element = document.getElementById(id);

    if (!element) return;

    const target = Number(targetValue);

    if (!Number.isFinite(target)) {
        setAnimatedText(id, targetValue);
        return;
    }

    cancelNumberAnimation(id);
    const targetText = formatter(target);

    if (element.textContent === targetText) return;

    const current = Number.parseFloat(
        element.textContent.replace(/[^\d.-]/g, "")
    );
    const start = Number.isFinite(current) ? current : 0;

    if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) {
        element.classList.remove("value-updated");
        element.textContent = targetText;
        return;
    }

    replayValueAnimation(element);

    const duration = 700;
    const startedAt = performance.now();

    const update = now => {
        const progress = Math.min((now - startedAt) / duration, 1);
        const easedProgress = 1 - Math.pow(1 - progress, 3);
        const value = start + (target - start) * easedProgress;

        element.textContent = formatter(progress === 1 ? target : value);

        if (progress < 1) {
            numberAnimationFrames.set(id, window.requestAnimationFrame(update));
        } else {
            numberAnimationFrames.delete(id);
        }
    };

    numberAnimationFrames.set(id, window.requestAnimationFrame(update));
}

// Format database timestamp
function formatDate(timestamp) {
    if (!timestamp) return "--";

    const match = String(timestamp).match(
        /^(\d{4})(\d{2})(\d{2})_(\d{2})(\d{2})(\d{2})/
    );

    if (match) {
        return `${match[3]}/${match[2]}/${match[1]} ${match[4]}:${match[5]}:${match[6]}`;
    }

    const date = new Date(timestamp);

    return Number.isNaN(date.getTime())
        ? String(timestamp)
        : date.toLocaleString();
}

// Format AI confidence
function confidenceText(item) {
    if (!item || !item.detected || item.confidence == null) {
        return "--";
    }

    return `${(Number(item.confidence) * 100).toFixed(1)}%`;
}

// Update server connection status
function setServerStatus(online) {
    setText(
        "serverStatus",
        online ? "Online" : "Connection error"
    );

    const dot = document.getElementById("statusDot");

    if (dot) {
        dot.style.backgroundColor = online
            ? "#22c55e"
            : "#ef4444";
    }
}

// Display latest uploaded camera image
function showLatestImage(filename) {
    const image = document.getElementById("latestImage");
    const placeholder = document.getElementById("imagePlaceholder");

    if (!image || !placeholder) return;

    if (!filename) {
        image.removeAttribute("src");
        delete image.dataset.filename;
        image.style.display = "none";
        placeholder.style.display = "flex";
        placeholder.textContent = "No camera image available yet.";
        return;
    }

    if (image.dataset.filename === filename) return;

    image.dataset.filename = filename;
    image.onload = () => {
        image.style.display = "block";
        placeholder.style.display = "none";
    };

    image.onerror = () => {
        image.style.display = "none";
        placeholder.style.display = "flex";
        placeholder.textContent = "Image could not be loaded.";
    };

    image.src = `/images/${encodeURIComponent(filename)}`;
}

// Display recent detection history
function renderHistory(records) {
    const tableBody = document.getElementById("historyTable");

    if (!tableBody) return;

    const signature = JSON.stringify(records.map(item => [
        item.id,
        item.object_name,
        confidenceText(item),
        item.location,
        formatDate(item.timestamp),
        Boolean(item.detected)
    ]));

    if (signature === lastRenderedHistorySignature) return;
    lastRenderedHistorySignature = signature;
    tableBody.innerHTML = "";

    if (!records.length) {
        const row = tableBody.insertRow();
        animateHistoryRow(row, 0);
        const cell = row.insertCell();

        cell.colSpan = 6;
        cell.textContent = "No detection records found.";

        return;
    }

    records.forEach((item, index) => {
        const row = tableBody.insertRow();
        animateHistoryRow(row, index);

        const values = [
            item.id ?? "--",
            item.object_name ?? "--",
            confidenceText(item),
            item.location ?? "--",
            formatDate(item.timestamp)
        ];

        values.forEach(value => {
            const cell = row.insertCell();
            cell.textContent = value;
        });

        const statusCell = row.insertCell();
        const badge = document.createElement("span");
        badge.className = item.detected
            ? "status-badge"
            : "status-badge not-detected";
        badge.textContent = item.detected ? "Wallet found" : "Not detected";
        statusCell.appendChild(badge);
    });
}

function animateHistoryRow(row, index) {
    if (window.matchMedia("(prefers-reduced-motion: reduce)").matches) return;

    row.className = "history-row-enter";
    row.style.animationDelay = `${Math.min(index, 7) * 35}ms`;
    row.addEventListener("animationend", () => {
        row.classList.remove("history-row-enter");
        row.style.animationDelay = "";
    }, { once: true });
}

function showNotification(title, message) {
    const region = document.getElementById("toastRegion");

    if (!region) return;

    const toast = document.createElement("div");
    toast.className = "toast toast-warning";
    toast.setAttribute("role", "alert");

    const icon = document.createElement("span");
    icon.className = "toast-icon";
    icon.setAttribute("aria-hidden", "true");
    icon.textContent = "!";

    const content = document.createElement("div");
    content.className = "toast-content";

    const heading = document.createElement("strong");
    heading.textContent = title;

    const detail = document.createElement("p");
    detail.textContent = message;

    const closeButton = document.createElement("button");
    closeButton.className = "toast-close";
    closeButton.type = "button";
    closeButton.setAttribute("aria-label", "Dismiss notification");
    closeButton.textContent = "×";

    content.append(heading, detail);
    toast.append(icon, content, closeButton);
    region.replaceChildren(toast);

    const dismiss = () => {
        toast.classList.add("is-leaving");
        window.setTimeout(() => toast.remove(), 220);
    };

    closeButton.addEventListener("click", dismiss);
    window.clearTimeout(toastTimeout);
    toastTimeout = window.setTimeout(dismiss, 8000);
}

function notifyIfWalletNotDetected(detection) {
    if (!detection || detection.detected || detection.id == null) return;

    const detectionId = String(detection.id);

    if (detectionId === lastNotifiedDetectionId) return;

    lastNotifiedDetectionId = detectionId;
    showNotification(
        "Wallet not detected",
        `The latest camera capture at ${detection.location || "your saved location"} did not find your wallet. The capture is saved in detection history.`
    );
}

function initializeTheme() {
    const root = document.documentElement;
    const toggle = document.getElementById("themeToggle");
    const icon = toggle && toggle.querySelector(".theme-icon");
    const label = toggle && toggle.querySelector(".theme-label");

    if (!toggle || !icon || !label) return;

    const updateThemeControl = () => {
        const isLight = root.dataset.theme === "light";
        icon.textContent = isLight ? "☾" : "☀";
        label.textContent = isLight ? "Dark mode" : "Light mode";
        toggle.setAttribute(
            "aria-label",
            `Switch to ${isLight ? "dark" : "light"} theme`
        );
        document.querySelector('meta[name="theme-color"]').content =
            isLight ? "#f3f6fb" : "#080d17";
    };

    updateThemeControl();

    toggle.addEventListener("click", () => {
        const theme = root.dataset.theme === "light" ? "dark" : "light";
        root.dataset.theme = theme;
        updateThemeControl();
        try {
            localStorage.setItem("lost-object-theme", theme);
        } catch (error) {
            console.warn("Theme preference could not be saved:", error);
        }
    });
}

// Main dashboard update function
async function loadDashboard() {
    try {
        const data = await fetchJSON("/api/dashboard");

        // ======================================
        // STATISTICS
        // ======================================

        setAnimatedNumber(
            "totalDetections",
            data.total_records ?? 0,
            value => String(Math.round(value))
        );

        setText(
            "cameraLocation",
            data.location ?? "Bedroom"
        );

        setAnimatedNumber(
            "todayDetections",
            data.today_detections ?? 0,
            value => String(Math.round(value))
        );

        // ======================================
        // AVERAGE CONFIDENCE
        // ======================================

        const recent = Array.isArray(data.recent_detections)
            ? data.recent_detections
            : [];

        const scores = recent
            .filter(item =>
                item.detected && item.confidence != null
            )
            .map(item => Number(item.confidence))
            .filter(Number.isFinite);

        if (scores.length > 0) {
            const average =
                scores.reduce((a, b) => a + b, 0) / scores.length;

            setAnimatedNumber(
                "averageConfidence",
                average * 100,
                value => `${value.toFixed(1)}%`
            );
        } else {
            setAnimatedText("averageConfidence", "--");
        }

        // ======================================
        // LATEST DETECTION
        // ======================================

        const latest = data.latest_detection;
        const latestCapture =
            data.latest_capture || (latest && latest.image_filename);

        notifyIfWalletNotDetected(latest);

        if (latest) {
            setAnimatedText(
                "latestObject",
                latest.object_name ?? "--"
            );

            setAnimatedText(
                "latestLocation",
                latest.location ?? data.location ?? "--"
            );

            const latestConfidence = Number(latest.confidence);

            if (
                latest.detected &&
                latest.confidence != null &&
                Number.isFinite(latestConfidence)
            ) {
                setAnimatedNumber(
                    "latestConfidence",
                    latestConfidence * 100,
                    value => `${value.toFixed(1)}%`
                );
            } else {
                setAnimatedText("latestConfidence", "--");
            }

            setAnimatedText(
                "latestTime",
                formatDate(latest.timestamp)
            );

        } else {
            setAnimatedText("latestObject", "No detections yet");
            setAnimatedText("latestLocation", "--");
            setAnimatedText("latestConfidence", "--");
            setAnimatedText("latestTime", "--");
        }

        showLatestImage(latestCapture);

        // ======================================
        // DETECTION HISTORY
        // ======================================

        renderHistory(recent);

        // ======================================
        // SERVER STATUS
        // ======================================

        setServerStatus(true);

    } catch (error) {
        console.error(
            "Dashboard update failed:",
            error
        );

        setServerStatus(false);
    }
}

// Make function available to Refresh Data button
window.loadDashboard = loadDashboard;

// Automatic dashboard refresh
document.addEventListener("DOMContentLoaded", () => {
    initializeTheme();
    loadDashboard();

    setInterval(loadDashboard, 3000);
});

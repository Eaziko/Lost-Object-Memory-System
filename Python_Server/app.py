
import os
import json
import sqlite3
from datetime import datetime
from pathlib import Path

from flask import (
    Flask,
    request,
    jsonify,
    render_template
)
from werkzeug.utils import secure_filename

from inference_sdk import (
    InferenceHTTPClient,
    InferenceConfiguration
)


# =====================================================
# 1. PROJECT PATHS AND CONFIGURATION
# =====================================================

BASE_DIR = Path(__file__).resolve().parent.parent

IMAGE_FOLDER = BASE_DIR / "Captured_Images"
DATABASE_PATH = BASE_DIR / "Database" / "lost_objects.db"

WEBSITE_DIR = BASE_DIR / "Website"
TEMPLATE_DIR = WEBSITE_DIR / "templates"
STATIC_DIR = WEBSITE_DIR / "static"

IMAGE_FOLDER.mkdir(parents=True, exist_ok=True)
DATABASE_PATH.parent.mkdir(parents=True, exist_ok=True)
TEMPLATE_DIR.mkdir(parents=True, exist_ok=True)
STATIC_DIR.mkdir(parents=True, exist_ok=True)

LOCATION = "Bedroom"
CAMERA_ID = "CAM_01"
CONFIDENCE_THRESHOLD = 0.50

ROBOFLOW_API_KEY = os.getenv("ROBOFLOW_API_KEY")

WORKSPACE_NAME = "tammims-workspace"
WORKFLOW_ID = "custom-wallet-detection"

ROBOFLOW_CLIENT = None


# =====================================================
# 2. FLASK APPLICATION
# =====================================================

app = Flask(
    __name__,
    template_folder=str(TEMPLATE_DIR),
    static_folder=str(STATIC_DIR),
    static_url_path="/static"
)


# =====================================================
# 3. DATABASE CONNECTION
# =====================================================

def get_db_connection():

    connection = sqlite3.connect(DATABASE_PATH)
    connection.row_factory = sqlite3.Row

    return connection


def initialize_database():

    connection = get_db_connection()

    connection.execute("""
        CREATE TABLE IF NOT EXISTS detections (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            object_name TEXT,
            confidence REAL,
            location TEXT,
            timestamp TEXT,
            image_filename TEXT,
            detected INTEGER,
            raw_prediction TEXT
        )
    """)

    connection.commit()
    connection.close()


initialize_database()


# =====================================================
# 4. ROBOFLOW CLIENT
# =====================================================

def get_roboflow_client():

    global ROBOFLOW_CLIENT

    if not ROBOFLOW_API_KEY:
        raise RuntimeError(
            "ROBOFLOW_API_KEY environment variable is missing."
        )

    if ROBOFLOW_CLIENT is None:

        ROBOFLOW_CLIENT = InferenceHTTPClient(
            api_url="https://serverless.roboflow.com",
            api_key=ROBOFLOW_API_KEY
        ).configure(
            InferenceConfiguration(
                api_key_transport="header"
            )
        )

    return ROBOFLOW_CLIENT


# =====================================================
# 5. EXTRACT ROBOFLOW PREDICTIONS
# =====================================================

def extract_predictions(data):

    results = []

    def search(item):

        if isinstance(item, dict):

            predictions = item.get("predictions")

            if isinstance(predictions, list):

                for prediction in predictions:

                    if not isinstance(prediction, dict):
                        continue

                    class_name = (
                        prediction.get("class")
                        or prediction.get("class_name")
                        or prediction.get("label")
                    )

                    confidence = prediction.get(
                        "confidence",
                        prediction.get("score", 0)
                    )

                    if class_name:

                        try:
                            confidence = float(confidence)
                        except (TypeError, ValueError):
                            confidence = 0.0

                        results.append({
                            "class": str(class_name),
                            "confidence": confidence
                        })

            for value in item.values():

                if isinstance(value, (dict, list)):
                    search(value)

        elif isinstance(item, list):

            for value in item:
                search(value)

    search(data)

    return results


# =====================================================
# 6. WALLET DETECTION
# =====================================================

def detect_wallet(image_path):

    client = get_roboflow_client()

    result = client.run_workflow(
        workspace_name=WORKSPACE_NAME,
        workflow_id=WORKFLOW_ID,
        images={
            "image": str(image_path)
        },
        use_cache=True
    )

    print("\n========== ROBOFLOW RESPONSE ==========")
    print(json.dumps(result, indent=2))
    print("=======================================\n")

    predictions = extract_predictions(result)

    wallet_predictions = [
        prediction
        for prediction in predictions
        if prediction["class"].lower() == "wallet"
        and prediction["confidence"] >= CONFIDENCE_THRESHOLD
    ]

    if wallet_predictions:

        best_prediction = max(
            wallet_predictions,
            key=lambda item: item["confidence"]
        )

        return {
            "detected": True,
            "object_name": "Wallet",
            "confidence": best_prediction["confidence"],
            "all_predictions": predictions,
            "raw_response": result
        }

    return {
        "detected": False,
        "object_name": "Not Detected",
        "confidence": 0.0,
        "all_predictions": predictions,
        "raw_response": result
    }


# =====================================================
# 7. SAVE DETECTION TO SQLITE
# =====================================================

def save_detection(
    object_name,
    confidence,
    timestamp,
    filename,
    detected,
    raw_prediction
):

    connection = get_db_connection()

    connection.execute("""
        INSERT INTO detections (
            object_name,
            confidence,
            location,
            timestamp,
            image_filename,
            detected,
            raw_prediction
        )
        VALUES (?, ?, ?, ?, ?, ?, ?)
    """, (
        object_name,
        confidence,
        LOCATION,
        timestamp,
        filename,
        int(detected),
        json.dumps(raw_prediction, default=str)
    ))

    connection.commit()
    connection.close()


# =====================================================
# 8. WEBSITE DASHBOARD
# =====================================================

@app.route("/", methods=["GET"])
def dashboard():

    return render_template("index.html")


# =====================================================
# 9. SERVER STATUS
# =====================================================

@app.route("/status", methods=["GET"])
def status():

    return jsonify({
        "server": "online",
        "project": "Lost Object Memory System",
        "image_receiver": "ready",
        "roboflow": (
            "configured"
            if ROBOFLOW_API_KEY
            else "missing API key"
        ),
        "location": LOCATION,
        "camera": CAMERA_ID
    })


# =====================================================
# 10. IMAGE UPLOAD FROM ESP32
# =====================================================

@app.route("/upload", methods=["POST"])
def upload_image():

    if "image" not in request.files:

        return jsonify({
            "success": False,
            "message": "No image received"
        }), 400

    image = request.files["image"]

    if not image.filename:

        return jsonify({
            "success": False,
            "message": "Empty filename"
        }), 400

    timestamp = datetime.now().strftime(
        "%Y%m%d_%H%M%S_%f"
    )

    filename = f"capture_{timestamp}.jpg"
    filename = secure_filename(filename)

    save_path = IMAGE_FOLDER / filename

    image.save(save_path)

    print(f"\nIMAGE RECEIVED: {save_path}")

    try:

        print("Sending image to Roboflow...")

        detection = detect_wallet(save_path)

        save_detection(
            object_name=detection["object_name"],
            confidence=detection["confidence"],
            timestamp=timestamp,
            filename=filename,
            detected=detection["detected"],
            raw_prediction=detection["raw_response"]
        )

        print("\n========== DETECTION RESULT ==========")
        print("Object:", detection["object_name"])
        print("Confidence:", detection["confidence"])
        print("Location:", LOCATION)
        print("======================================")

        return jsonify({
            "success": True,
            "message": "Image received and processed",
            "filename": filename,
            "detected": detection["detected"],
            "object_name": detection["object_name"],
            "confidence": detection["confidence"],
            "location": LOCATION,
            "camera": CAMERA_ID,
            "timestamp": timestamp
        }), 200

    except Exception as error:

        print("ROBOFLOW ERROR:", str(error))

        return jsonify({
            "success": False,
            "message": "Image saved, but detection failed",
            "filename": filename,
            "detection_error": str(error)
        }), 500


# =====================================================
# 11. RECENT DETECTIONS API
# =====================================================

@app.route("/api/detections", methods=["GET"])
def api_detections():

    limit = request.args.get("limit", default=20, type=int)
    limit = max(1, min(limit, 100))

    connection = get_db_connection()

    rows = connection.execute("""
        SELECT
            id,
            object_name,
            confidence,
            location,
            timestamp,
            image_filename,
            detected
        FROM detections
        ORDER BY id DESC
        LIMIT ?
    """, (limit,)).fetchall()

    connection.close()

    return jsonify([
        dict(row) for row in rows
    ])


# =====================================================
# 12. LATEST DETECTION API
# =====================================================

@app.route("/api/latest", methods=["GET"])
def api_latest():

    connection = get_db_connection()

    row = connection.execute("""
        SELECT
            id,
            object_name,
            confidence,
            location,
            timestamp,
            image_filename,
            detected
        FROM detections
        ORDER BY id DESC
        LIMIT 1
    """).fetchone()

    connection.close()

    if row is None:

        return jsonify({
            "available": False,
            "message": "No detections recorded yet"
        })

    return jsonify({
        "available": True,
        "detection": dict(row)
    })


# =====================================================
# 13. DASHBOARD STATISTICS API
# =====================================================

@app.route("/api/stats", methods=["GET"])
def api_stats():

    connection = get_db_connection()

    total_records = connection.execute("""
        SELECT COUNT(*) AS total
        FROM detections
    """).fetchone()["total"]

    wallet_detections = connection.execute("""
        SELECT COUNT(*) AS total
        FROM detections
        WHERE detected = 1
    """).fetchone()["total"]

    latest = connection.execute("""
        SELECT
            object_name,
            confidence,
            location,
            timestamp,
            detected
        FROM detections
        ORDER BY id DESC
        LIMIT 1
    """).fetchone()

    connection.close()

    return jsonify({
        "total_records": total_records,
        "wallet_detections": wallet_detections,
        "camera": CAMERA_ID,
        "location": LOCATION,
        "latest_detection": (
            dict(latest) if latest else None
        )
    })


# =====================================================
# 14. COMBINED DASHBOARD API
# =====================================================

@app.route("/api/dashboard", methods=["GET"])
def api_dashboard():

    connection = get_db_connection()

    total_records = connection.execute("""
        SELECT COUNT(*) AS total
        FROM detections
    """).fetchone()["total"]

    today_detections = connection.execute("""
        SELECT COUNT(*) AS total
        FROM detections
        WHERE timestamp LIKE ?
    """, (datetime.now().strftime("%Y%m%d") + "%",)).fetchone()["total"]

    wallet_detections = connection.execute("""
        SELECT COUNT(*) AS total
        FROM detections
        WHERE detected = 1
    """).fetchone()["total"]

    latest = connection.execute("""
        SELECT
            id,
            object_name,
            confidence,
            location,
            timestamp,
            image_filename,
            detected
        FROM detections
        ORDER BY id DESC
        LIMIT 1
    """).fetchone()

    recent = connection.execute("""
        SELECT
            id,
            object_name,
            confidence,
            location,
            timestamp,
            image_filename,
            detected
        FROM detections
        ORDER BY id DESC
        LIMIT 10
    """).fetchall()

    latest_capture = max(
        IMAGE_FOLDER.glob("capture_*.jpg"),
        key=lambda path: path.name,
        default=None
    )

    connection.close()

    return jsonify({
        "server": "online",
        "camera": CAMERA_ID,
        "location": LOCATION,
        "total_records": total_records,
        "today_detections": today_detections,
        "wallet_detections": wallet_detections,
        "latest_detection": (
            dict(latest) if latest else None
        ),
        "latest_capture": (
            latest_capture.name if latest_capture else None
        ),
        "recent_detections": [
            dict(row) for row in recent
        ]
    })


# =====================================================
# 15. VIEW CAPTURED IMAGE
# =====================================================

@app.route("/images/<filename>", methods=["GET"])
def view_image(filename):

    safe_filename = secure_filename(filename)

    image_path = IMAGE_FOLDER / safe_filename

    if not image_path.is_file():

        return jsonify({
            "error": "Image not found"
        }), 404

    from flask import send_from_directory

    return send_from_directory(
        str(IMAGE_FOLDER),
        safe_filename
    )


# =====================================================
# 16. RUN FLASK SERVER
# =====================================================

if __name__ == "__main__":

    print("======================================")
    print("LOST OBJECT MEMORY SYSTEM")
    print("FLASK + ROBOFLOW + WEBSITE DASHBOARD")
    print("======================================")

    print("Image folder:", IMAGE_FOLDER)
    print("Database:", DATABASE_PATH)
    print("Website templates:", TEMPLATE_DIR)
    print(
        "Roboflow API key:",
        "Available" if ROBOFLOW_API_KEY else "MISSING"
    )

    app.run(
        host="0.0.0.0",
        port=5000,
        debug=False,
        threaded=True
    )

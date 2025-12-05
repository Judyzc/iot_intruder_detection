from flask import Flask, request, jsonify
import psycopg2
from datetime import datetime, timedelta, timezone
from flask_cors import CORS

app = Flask(__name__)
CORS(app)

def get_db_connection():
    return psycopg2.connect(
        database="intruder_detection",
        user="postgres",
        # hiding the password for now 
        password="************",
        host="127.0.0.1",
        port="5432"
    )

# posting all of the data on intruder status, confidence, and face_id info to intruder_data table
@app.route('/create', methods=['POST'])
def create():
    conn = get_db_connection()
    cur = conn.cursor()
    data = request.get_json()

    if not data:
        return jsonify({"error": "No JSON received"}), 400


    if isinstance(data, dict):
        data = [data]

    for row in data:
        # all data sent to the server must be in JSON format 
        # endpoint is http://54.167.124.79:5000/create - I am running this from an EC2 instance
        intruder_status = row.get("intruder_status")
        face_id = row.get("face_id")
        confidence = row.get("confidence")
        if intruder_status is None:
            continue  

        cur.execute(
            "INSERT INTO intruder_data (intruder_status, face_id, confidence) VALUES (%s, %s, %s)",
            (intruder_status, face_id, confidence)
        )

    conn.commit()
    cur.close()
    conn.close()

    return jsonify({"message": f"row inserted successfully"}), 201

# getting all of the data inserted in the last give minutes, so the data can be plotted in plotly 
@app.route('/recent', methods=['GET'])
def get_recent():
    """
    Get all rows inserted in the last 5 minutes - we only want the most recent data 
    """
    conn = get_db_connection()
    cur = conn.cursor()

    five_min_ago = datetime.now(timezone.utc) - timedelta(minutes=5)

    cur.execute(
        "SELECT intruder_status, face_id, confidence, timestamp FROM intruder_data WHERE timestamp >= %s ORDER BY timestamp ASC",
        (five_min_ago,)
    )

    rows = cur.fetchall()
    cur.close()
    conn.close()

    # formatting the JSON GET request

    data = [
        {
            "intruder_status": r[0],
            "face_id": r[1],
            "confidence": r[2],
            "timestamp": r[3].isoformat()
        } for r in rows
    ]

    return jsonify(data), 200
# getting the device heartbeat
@app.route('/status', methods=['GET'])
def get_status():
    """
    Get the device's current status every minute - if it's on and connected to wifi
    """
    conn = get_db_connection()
    cur = conn.cursor()

    one_min_ago = datetime.now(timezone.utc) - timedelta(minutes=1)
    cur.execute(
        "SELECT created_at, status FROM device_status WHERE created_at >= %s ORDER BY created_at ASC",
        (one_min_ago,)
    )

    rows = cur.fetchall()
    cur.close()
    conn.close()
    data = [
        {
            "created_at": r[0].isoformat(),
            "status": r[1]
        } for r in rows
    ]

    return jsonify(data), 200
# posting the device heartbeat
@app.route('/status_create', methods=['POST'])
def create_status():
    conn = get_db_connection()
    cur = conn.cursor()
    data = request.get_json()

    if not data:
        return jsonify({"error": "No JSON received"}), 400


    if isinstance(data, dict):
        data = [data]

    for row in data:
        # all data sent to the server must be in JSON format 
        # endpoint is http://54.167.124.79:5000/status_create 
        status_value = row.get("status")
        if status_value is None:
            continue  

        cur.execute(
            "INSERT INTO device_status (status) VALUES (%s)",
            (status_value,)
        )

    conn.commit()
    cur.close()
    conn.close()

    return jsonify({"message": f"row inserted successfully"}), 201

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)

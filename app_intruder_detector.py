from flask import Flask, request, jsonify
import psycopg2
from datetime import datetime

app = Flask(__name__)


def get_db_connection():
    return psycopg2.connect(
        database="intruder_detection",
        user="postgres",
        # hiding the password for now 
        password="**********",
        host="127.0.0.1",
        port="5432"
    )

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
            "INSERT INTO intruder_list (intruder_status, face_id, confidence) VALUES (%s, %s, %s)",
            (intruder_status, face_id, confidence)
        )

    conn.commit()
    cur.close()
    conn.close()

    return jsonify({"message": f"row inserted successfully"}), 201


if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)